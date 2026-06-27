// YES DAW - H3 host-isolation close-out gate (ADR-0015 / ADR-0013 / ADR-0006 / ADR-0007).
//
// This target is the mechanical H3 finish line. It intentionally starts RED and is tagged
// [!shouldfail] so `main` stays green while the real hosted-process implementation is still absent.
// Replace each boolean with a real assertion as the corresponding close-out-plan item lands, then
// remove [!shouldfail] when the whole gate passes without inversion.

#include <catch2/catch_test_macros.hpp>

#include "engine/MixerGraphProjection.h"
#include "engine/MixerMutePolicy.h"
#include "engine/Runtime.h"
#include "engine/plugin/RtLaneRing.h"
#include "persistence/ProjectBundle.h"
#include "plugin_host/PluginHostCoordinator.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using yesdaw::engine::Event;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNode;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::GraphId;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::MasterNode;
using yesdaw::engine::MixerBusProjection;
using yesdaw::engine::MixerMuteTarget;
using yesdaw::engine::MixerProjectionError;
using yesdaw::engine::MixerProjectionInputs;
using yesdaw::engine::MixerSendProjection;
using yesdaw::engine::MixerSendTap;
using yesdaw::engine::MixerTrackProjection;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::PlaceholderNode;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::RtLaneConfig;
using yesdaw::engine::RtLaneAttachFailure;
using yesdaw::engine::RtLaneExchangeResult;
using yesdaw::engine::RtLaneOutput;
using yesdaw::engine::RtLaneRing;
using yesdaw::engine::Runtime;
using yesdaw::engine::applyMixerMutePolicy;
using yesdaw::engine::buildMixerGraphProjection;
using yesdaw::plugin_host::PluginHostCoordinator;
using yesdaw::plugin_host::RtLaneLoadConfig;
using yesdaw::plugin_host::RtLaneLoadReplyStatus;
using yesdaw::persistence::PluginStateFormat;
using yesdaw::persistence::ProjectBundleDb;

namespace {

class SimulatedHostedPluginNode final : public Node
{
public:
    explicit SimulatedHostedPluginNode (NodeId id, int channels = 1) noexcept
        : id_ (id), channels_ (channels > 0 ? channels : 1)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return { true, false, channels_, 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override {}
    void process (const ProcessArgs&) noexcept YESDAW_RT_HOT override {}
    void reset() noexcept override {}
    void release() override {}

    void setInput (Node* input) noexcept
    {
        input_ = input;
    }

private:
    NodeId id_;
    int channels_;
    Node* input_ = nullptr;
};

class SimulatedHostedPluginSourceNode final : public Node
{
public:
    SimulatedHostedPluginSourceNode (NodeId id, float dc) noexcept
        : id_ (id), dc_ (dc)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return { true, false, 1, 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }
    void prepare (double, int) override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels < 1)
            return;

        float* const out = args.audio.channels[0];
        for (int i = 0; i < args.numFrames; ++i)
            out[i] = dc_;
    }

    void reset() noexcept override {}
    void release() override {}

private:
    NodeId id_ = 0;
    float dc_ = 0.0f;
};

constexpr NodeId kRecoverySourceId = 72001;
constexpr NodeId kRecoveryOffenderId = 72002;
constexpr NodeId kRecoveryMasterId = 72003;

GraphBuilder::Inputs recoveryGraphInputs (bool placeholder, GraphId graphId)
{
    GraphBuilder::Inputs inputs;
    inputs.id = graphId;
    inputs.masterNodeId = kRecoveryMasterId;
    inputs.maxBlockSize = 8;

    auto source = std::make_unique<IdentityDcNode> (kRecoverySourceId, 0.75f, 1);
    Node* const sourcePtr = source.get();

    std::unique_ptr<Node> offender;
    if (placeholder)
    {
        auto replacement = std::make_unique<PlaceholderNode> (kRecoveryOffenderId, 1);
        replacement->setInput (sourcePtr);
        offender = std::move (replacement);
    }
    else
    {
        auto hosted = std::make_unique<SimulatedHostedPluginNode> (kRecoveryOffenderId, 1);
        hosted->setInput (sourcePtr);
        offender = std::move (hosted);
    }

    Node* const offenderPtr = offender.get();
    auto master = std::make_unique<MasterNode> (kRecoveryMasterId, 1);
    master->setInputNodes ({ offenderPtr });

    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (offender));
    inputs.nodes.push_back (std::move (master));
    return inputs;
}

bool allSamplesNear (const std::vector<float>& values, float expected) noexcept
{
    for (const float value : values)
        if (std::abs (value - expected) > 0.000001f)
            return false;

    return true;
}

constexpr int kMixerRuntimeFrames = 16;
constexpr float kMixerRuntimeCenterGain = 0.70710677f;
constexpr NodeId kMixerRuntimeSourceA = 73001;
constexpr NodeId kMixerRuntimeSourceB = 73011;
constexpr NodeId kMixerRuntimeBusSum = 73100;
constexpr NodeId kMixerRuntimeMasterSum = 73998;
constexpr NodeId kMixerRuntimeMaster = 73999;

MixerTrackProjection makeHostedRuntimeTrack (NodeId sourceId, float dc, NodeId faderId, NodeId panId, NodeId meterId)
{
    MixerTrackProjection track;
    track.source = std::make_unique<SimulatedHostedPluginSourceNode> (sourceId, dc);
    track.faderNodeId = faderId;
    track.panNodeId = panId;
    track.meterNodeId = meterId;
    track.linearGain = 0.0f; // Direct path silent; this proof isolates bus Return/SIP leakage.
    track.pan = 0.0f;
    return track;
}

MixerProjectionInputs hostedRuntimeProjectionInputs (GraphId graphId, std::vector<NodeId>* fillerSourceIds = nullptr)
{
    MixerProjectionInputs inputs;
    inputs.id = graphId;
    inputs.masterSumNodeId = kMixerRuntimeMasterSum;
    inputs.masterNodeId = kMixerRuntimeMaster;
    inputs.maxBlockSize = kMixerRuntimeFrames;
    inputs.buses.push_back (MixerBusProjection { kMixerRuntimeBusSum, 73101, 73102 });

    MixerTrackProjection soloed = makeHostedRuntimeTrack (kMixerRuntimeSourceA, 1.0f, 73002, 73003, 73004);
    soloed.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });
    inputs.tracks.push_back (std::move (soloed));

    MixerTrackProjection nonSoloed = makeHostedRuntimeTrack (kMixerRuntimeSourceB, 2.0f, 73012, 73013, 73014);
    nonSoloed.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });
    inputs.tracks.push_back (std::move (nonSoloed));

    constexpr int kFillersPastOldMuteMask = 70;
    for (int i = 0; i < kFillersPastOldMuteMask; ++i)
    {
        const NodeId source = static_cast<NodeId> (73200 + i * 10);
        if (fillerSourceIds != nullptr)
            fillerSourceIds->push_back (source);
        inputs.tracks.push_back (makeHostedRuntimeTrack (source, 0.0f, source + 1u, source + 2u, source + 3u));
    }

    return inputs;
}

const CompiledNode* compiledNodeById (const CompiledGraph& graph, NodeId id)
{
    for (const CompiledNode& node : graph.debugCompiledNodes())
        if (node.id == id)
            return &node;

    return nullptr;
}

std::filesystem::path makeTempBundlePath (std::string_view label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("yesdaw-h3-" + std::string (label) + "-"
                                                  + std::to_string (ticks) + ".yesdaw");

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

struct HostIsolationGateState
{
    bool syntheticProcessorRunsInWorkerChild = false;
    bool rtLaneUsesOsSharedMemory             = false;
    bool rtLaneIdentityPassesControlLane      = false;
    bool workerPollsHostedProcessorOverRtLane = false;
    bool mixerProjectionPublishesToRuntime    = false;
    bool triStreamPdcThroughHostedPlugin      = false;
    bool watchdogKillsHungOrCrashedChild      = false;
    bool failOpenHasNoDeadlineMisses          = false;
    bool placeholderSwapUsesOrderedPublish    = false;
    bool blacklistPersistsAcrossRestart       = false;
    bool opaqueStateRoundTripsAcrossProcess   = false;
};

std::string quotedWorkerSelfCheckCommand (const std::filesystem::path& worker)
{
    std::string command = "\"";

    for (const char ch : worker.string())
    {
        if (ch == '"')
            command += "\\\"";
        else
            command += ch;
    }

    command += "\" --synthetic-plugin-self-check";
    return command;
}

std::string workerPathFromEnvironment()
{
#if defined (_WIN32)
    char* value = nullptr;
    size_t valueSize = 0;
    if (_dupenv_s (&value, &valueSize, "YESDAW_PLUGIN_HOST_PATH") != 0 || value == nullptr)
        return {};

    std::string result (value);
    std::free (value);
    return result;
#else
    const char* const value = std::getenv ("YESDAW_PLUGIN_HOST_PATH");
    return value == nullptr ? std::string() : std::string (value);
#endif
}

bool runSyntheticWorkerSelfCheck()
{
    static const bool result = [] {
        const std::string workerPath = workerPathFromEnvironment();
        if (workerPath.empty())
            return false;

        const std::filesystem::path worker (workerPath);
        if (! std::filesystem::exists (worker))
            return false;

        return std::system (quotedWorkerSelfCheckCommand (worker).c_str()) == 0;
    }();

    return result;
}

struct RtLaneSharedMemoryProof
{
    bool passed = false;
    std::string failureStep = "not-run";
    std::uint64_t inputSeq = 0;
    std::uint64_t outputSeq = 0;
    RtLaneOutput r0Source = RtLaneOutput::Silence;
    RtLaneOutput r1Source = RtLaneOutput::Silence;
    RtLaneAttachFailure childAttachFailure = RtLaneAttachFailure::None;
    int childAttachSystemError = 0;
    RtLaneAttachFailure negativeAttachFailure = RtLaneAttachFailure::None;
    int negativeAttachSystemError = 0;
    std::uint64_t r1OutputBlockIndex = 0;
    int failedFrame = -1;
    float observed = 0.0f;
    float expected = 0.0f;
};

RtLaneSharedMemoryProof proveRtLaneUsesOsSharedMemory()
{
    RtLaneSharedMemoryProof proof;
    auto fail = [&] (std::string step)
    {
        proof.failureStep = std::move (step);
        return proof;
    };

    const std::string regionName = RtLaneRing::makeUniqueSharedMemoryName();

    RtLaneConfig cfg;
    cfg.channels = 1;
    cfg.maxBlockSize = 8;
    cfg.maxEventsPerBlock = 2;

    RtLaneRing parentRtSide;
    parentRtSide.prepareSharedMemory (cfg, regionName);
    if (! parentRtSide.usesOsSharedMemory())
        return fail ("parent endpoint did not map OS shared memory");

    RtLaneRing childWorkerSide;
    if (! childWorkerSide.attachSharedMemory (regionName))
    {
        proof.childAttachFailure = childWorkerSide.lastAttachFailure();
        proof.childAttachSystemError = childWorkerSide.lastAttachSystemError();
        return fail ("child endpoint could not attach named shared memory");
    }
    if (! childWorkerSide.usesOsSharedMemory())
        return fail ("child endpoint attached without OS shared memory");

    const std::string missingRegionName = RtLaneRing::makeUniqueSharedMemoryName();
    RtLaneRing invalidAttach;
    if (invalidAttach.attachSharedMemory (missingRegionName))
        return fail ("negative control unexpectedly attached missing shared memory");
    proof.negativeAttachFailure = invalidAttach.lastAttachFailure();
    proof.negativeAttachSystemError = invalidAttach.lastAttachSystemError();

    constexpr int numFrames = 8;
    std::vector<float> in (numFrames, 0.0f);
    std::vector<float> out (numFrames, -1.0f);
    float* inCh[1] = { in.data() };
    float* outCh[1] = { out.data() };

    auto fillInput = [&] (std::uint64_t block)
    {
        for (int frame = 0; frame < numFrames; ++frame)
            in[static_cast<std::size_t> (frame)] = static_cast<float> (block * 100u + static_cast<std::uint64_t> (frame));
    };

    auto doubleProcess = [] (std::span<const Event>,
                             const float* const* input, float* const* output, int channels, int frames) noexcept
    {
        for (int channel = 0; channel < channels; ++channel)
            for (int frame = 0; frame < frames; ++frame)
                output[channel][frame] = input[channel][frame] * 2.0f;
    };

    fillInput (0);
    const RtLaneExchangeResult r0 = parentRtSide.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);
    proof.r0Source = r0.source;
    proof.inputSeq = parentRtSide.inputSeq();
    if (r0.source != RtLaneOutput::Silence || parentRtSide.inputSeq() != 1)
        return fail ("parent failed to publish block zero into shared memory");

    if (! childWorkerSide.pollOnce (doubleProcess))
        return fail ("child could not poll block zero from shared memory");
    proof.outputSeq = parentRtSide.outputSeq();
    if (proof.outputSeq != 1)
        return fail ("child output sequence was not visible to parent");

    fillInput (1);
    const RtLaneExchangeResult r1 = parentRtSide.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);
    proof.r1Source = r1.source;
    proof.r1OutputBlockIndex = r1.outputBlockIndex;
    if (r1.source != RtLaneOutput::Fresh || r1.outputBlockIndex != 0)
        return fail ("parent did not receive fresh processed block zero");

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const float expected = static_cast<float> (frame) * 2.0f;
        if (out[static_cast<std::size_t> (frame)] != expected)
        {
            proof.failedFrame = frame;
            proof.observed = out[static_cast<std::size_t> (frame)];
            proof.expected = expected;
            return fail ("processed shared-memory sample mismatch");
        }
    }

    proof.passed = true;
    proof.failureStep = "passed";
    return proof;
}

struct RtLaneControlLoadProof
{
    bool passed = false;
    std::string failureStep = "not-run";
    PluginHostCoordinator::RtLaneLoadStatus loadStatus {
        PluginHostCoordinator::RtLaneLoadStatus::notStarted
    };
    RtLaneLoadReplyStatus loadReplyStatus = RtLaneLoadReplyStatus::none;
    RtLaneAttachFailure loadAttachFailure = RtLaneAttachFailure::None;
    PluginHostCoordinator::StopStatus loadStopStatus {
        PluginHostCoordinator::StopStatus::notStarted
    };
    PluginHostCoordinator::RtLaneLoadStatus missingStatus {
        PluginHostCoordinator::RtLaneLoadStatus::notStarted
    };
    RtLaneLoadReplyStatus missingReplyStatus = RtLaneLoadReplyStatus::none;
    RtLaneAttachFailure missingAttachFailure = RtLaneAttachFailure::None;
    PluginHostCoordinator::RtLaneLoadStatus absentStatus {
        PluginHostCoordinator::RtLaneLoadStatus::notStarted
    };
    RtLaneLoadReplyStatus absentReplyStatus = RtLaneLoadReplyStatus::none;
    RtLaneAttachFailure absentAttachFailure = RtLaneAttachFailure::None;
    std::string loadName;
    std::string activeName;
    bool loadMessageSent = false;
    bool loadReplySeen = false;
    bool coordinatorAllocated = false;
    bool coordinatorUsesOsSharedMemory = false;
    bool workerAccepted = false;
    bool activeUsesOsSharedMemory = false;
};

RtLaneControlLoadProof computeRtLaneIdentityPassesControlLane()
{
    RtLaneControlLoadProof proof;
    auto fail = [&] (std::string step)
    {
        proof.failureStep = std::move (step);
        return proof;
    };

    const std::string workerPath = workerPathFromEnvironment();
    if (workerPath.empty())
        return fail ("YESDAW_PLUGIN_HOST_PATH is missing");

    const std::filesystem::path worker (workerPath);
    if (! std::filesystem::exists (worker))
        return fail ("worker executable path does not exist");

    RtLaneConfig cfg;
    cfg.channels = 1;
    cfg.maxBlockSize = 8;
    cfg.maxEventsPerBlock = 2;

    PluginHostCoordinator loadCoordinator;
    const auto load = loadCoordinator.launchAndLoadRtLane (juce::File (juce::String (workerPath)), cfg);
    const auto activeIdentity = loadCoordinator.activeRtLaneLoadIdentity();
    const bool activeUsesOsSharedMemory = loadCoordinator.activeRtLaneUsesOsSharedMemory();
    const auto loadStop = loadCoordinator.requestStopAndWait();

    proof.loadStatus = load.status;
    proof.loadReplyStatus = load.workerReplyStatus;
    proof.loadAttachFailure = load.workerAttachFailure;
    proof.loadStopStatus = loadStop.status;
    proof.loadName = load.identity.sharedMemoryName;
    proof.activeName = activeIdentity.sharedMemoryName;
    proof.loadMessageSent = load.loadMessageSent;
    proof.loadReplySeen = load.loadReplySeen;
    proof.coordinatorAllocated = load.coordinatorAllocated;
    proof.coordinatorUsesOsSharedMemory = load.coordinatorUsesOsSharedMemory;
    proof.workerAccepted = load.workerAccepted;
    proof.activeUsesOsSharedMemory = activeUsesOsSharedMemory;

    if (load.status != PluginHostCoordinator::RtLaneLoadStatus::success
        || load.workerReplyStatus != RtLaneLoadReplyStatus::accepted
        || load.workerAttachFailure != RtLaneAttachFailure::None
        || ! load.readySeen
        || ! load.loadMessageSent
        || ! load.loadReplySeen
        || ! load.coordinatorAllocated
        || ! load.coordinatorUsesOsSharedMemory
        || ! load.workerAccepted
        || load.identity.sharedMemoryName.empty()
        || activeIdentity.sharedMemoryName != load.identity.sharedMemoryName
        || ! activeUsesOsSharedMemory
        || loadStop.status != PluginHostCoordinator::StopStatus::stopped)
        return fail ("coordinator did not allocate/pass an accepted RT-lane identity");

    const RtLaneLoadConfig loadConfig {
        1u,
        8u,
        2u,
        cfg.lastGoodHoldBlocks,
        cfg.bypassAfterMisses
    };

    PluginHostCoordinator missingCoordinator;
    const auto missing =
        missingCoordinator.launchAndSendRtLaneLoadIdentity (juce::File (juce::String (workerPath)),
                                                            { {}, loadConfig });
    const auto missingStop = missingCoordinator.requestStopAndWait();
    proof.missingStatus = missing.status;
    proof.missingReplyStatus = missing.workerReplyStatus;
    proof.missingAttachFailure = missing.workerAttachFailure;
    if (missing.status != PluginHostCoordinator::RtLaneLoadStatus::workerRejected
        || missing.workerReplyStatus != RtLaneLoadReplyStatus::rejectedInvalidIdentity
        || missing.workerAttachFailure != RtLaneAttachFailure::None
        || ! missing.loadMessageSent
        || ! missing.loadReplySeen
        || missing.coordinatorAllocated
        || missing.coordinatorUsesOsSharedMemory
        || missing.workerAccepted
        || missingStop.status != PluginHostCoordinator::StopStatus::stopped)
        return fail ("worker did not reject a missing RT-lane identity");

    const std::string absentName = RtLaneRing::makeUniqueSharedMemoryName();
    PluginHostCoordinator absentCoordinator;
    const auto absent =
        absentCoordinator.launchAndSendRtLaneLoadIdentity (juce::File (juce::String (workerPath)),
                                                           { absentName, loadConfig });
    const auto absentStop = absentCoordinator.requestStopAndWait();
    proof.absentStatus = absent.status;
    proof.absentReplyStatus = absent.workerReplyStatus;
    proof.absentAttachFailure = absent.workerAttachFailure;
    if (absent.status != PluginHostCoordinator::RtLaneLoadStatus::workerRejected
        || absent.workerReplyStatus != RtLaneLoadReplyStatus::rejectedAttachFailed
        || absent.workerAttachFailure != RtLaneAttachFailure::OpenFailed
        || ! absent.loadMessageSent
        || ! absent.loadReplySeen
        || absent.coordinatorAllocated
        || absent.coordinatorUsesOsSharedMemory
        || absent.workerAccepted
        || absent.identity.sharedMemoryName != absentName
        || absentStop.status != PluginHostCoordinator::StopStatus::stopped)
        return fail ("worker did not reject an absent RT-lane region");

    proof.passed = true;
    proof.failureStep = "passed";
    return proof;
}

RtLaneControlLoadProof proveRtLaneIdentityPassesControlLane()
{
    static const RtLaneControlLoadProof proof = computeRtLaneIdentityPassesControlLane();
    return proof;
}

struct RtLaneWorkerPollProof
{
    bool passed = false;
    std::string failureStep = "not-run";
    PluginHostCoordinator::RtLaneLoadStatus loadStatus {
        PluginHostCoordinator::RtLaneLoadStatus::notStarted
    };
    RtLaneLoadReplyStatus loadReplyStatus = RtLaneLoadReplyStatus::none;
    RtLaneAttachFailure loadAttachFailure = RtLaneAttachFailure::None;
    PluginHostCoordinator::StopStatus stopStatus {
        PluginHostCoordinator::StopStatus::notStarted
    };
    RtLaneAttachFailure audioAttachFailure = RtLaneAttachFailure::None;
    int audioAttachSystemError = 0;
    std::uint64_t initialOutputSeq = 0;
    std::uint64_t outputSeqAfterPoll = 0;
    RtLaneOutput r0Source = RtLaneOutput::Silence;
    RtLaneOutput r1Source = RtLaneOutput::Silence;
    std::uint64_t r1OutputBlockIndex = 0;
    int failedChannel = -1;
    int failedFrame = -1;
    float observed = 0.0f;
    float expected = 0.0f;
};

bool waitForOutputSeqAtLeast (RtLaneRing& ring, std::uint64_t expected)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (2);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (ring.outputSeq() >= expected)
            return true;

        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    return ring.outputSeq() >= expected;
}

RtLaneWorkerPollProof computeWorkerPollsHostedProcessorOverRtLane()
{
    RtLaneWorkerPollProof proof;
    PluginHostCoordinator loadCoordinator;
    bool workerMayBeRunning = false;
    auto fail = [&] (std::string step)
    {
        proof.failureStep = std::move (step);
        if (workerMayBeRunning)
            proof.stopStatus = loadCoordinator.requestStopAndWait().status;
        return proof;
    };

    const std::string workerPath = workerPathFromEnvironment();
    if (workerPath.empty())
        return fail ("YESDAW_PLUGIN_HOST_PATH is missing");

    const std::filesystem::path worker (workerPath);
    if (! std::filesystem::exists (worker))
        return fail ("worker executable path does not exist");

    RtLaneConfig cfg;
    cfg.channels = 2;
    cfg.maxBlockSize = 8;
    cfg.maxEventsPerBlock = 2;

    const auto load = loadCoordinator.launchAndLoadRtLane (juce::File (juce::String (workerPath)), cfg);
    workerMayBeRunning = load.readySeen;
    const auto activeIdentity = loadCoordinator.activeRtLaneLoadIdentity();
    proof.loadStatus = load.status;
    proof.loadReplyStatus = load.workerReplyStatus;
    proof.loadAttachFailure = load.workerAttachFailure;

    if (load.status != PluginHostCoordinator::RtLaneLoadStatus::success
        || load.workerReplyStatus != RtLaneLoadReplyStatus::accepted
        || load.workerAttachFailure != RtLaneAttachFailure::None
        || ! load.readySeen
        || ! load.loadMessageSent
        || ! load.loadReplySeen
        || ! load.coordinatorAllocated
        || ! load.coordinatorUsesOsSharedMemory
        || ! load.workerAccepted
        || activeIdentity.sharedMemoryName != load.identity.sharedMemoryName
        || ! loadCoordinator.activeRtLaneUsesOsSharedMemory())
        return fail ("coordinator did not load an accepted RT-lane identity for worker polling");

    RtLaneRing audioRtSide;
    if (! audioRtSide.attachSharedMemory (activeIdentity.sharedMemoryName))
    {
        proof.audioAttachFailure = audioRtSide.lastAttachFailure();
        proof.audioAttachSystemError = audioRtSide.lastAttachSystemError();
        return fail ("test audio endpoint could not attach the coordinator-owned RT lane");
    }

    proof.initialOutputSeq = audioRtSide.outputSeq();
    if (proof.initialOutputSeq != 0)
        return fail ("worker produced output before any RT-lane input was published");

    constexpr int channels = 2;
    constexpr int numFrames = 8;
    std::vector<float> in (static_cast<std::size_t> (channels * numFrames), 0.0f);
    std::vector<float> out (static_cast<std::size_t> (channels * numFrames), -1.0f);
    std::vector<float*> inCh (channels);
    std::vector<float*> outCh (channels);
    for (int channel = 0; channel < channels; ++channel)
    {
        inCh[static_cast<std::size_t> (channel)] =
            in.data() + static_cast<std::size_t> (channel * numFrames);
        outCh[static_cast<std::size_t> (channel)] =
            out.data() + static_cast<std::size_t> (channel * numFrames);
    }

    auto sampleFor = [] (int channel, std::uint64_t block, int frame) noexcept
    {
        return static_cast<float> ((channel + 1) * 1000)
            + static_cast<float> (block * 100u)
            + static_cast<float> (frame);
    };

    auto fillInput = [&] (std::uint64_t block)
    {
        for (int channel = 0; channel < channels; ++channel)
            for (int frame = 0; frame < numFrames; ++frame)
                in[static_cast<std::size_t> (channel * numFrames + frame)] =
                    sampleFor (channel, block, frame);
    };

    fillInput (0);
    const RtLaneExchangeResult r0 =
        audioRtSide.exchangeBlock (inCh.data(), channels, numFrames, {}, outCh.data(), channels);
    proof.r0Source = r0.source;
    if (r0.source != RtLaneOutput::Silence || r0.inputBlockIndex != 0)
        return fail ("first RT-lane exchange did not prime the one-Block pipeline");

    if (! waitForOutputSeqAtLeast (audioRtSide, 1))
    {
        proof.outputSeqAfterPoll = audioRtSide.outputSeq();
        return fail ("worker did not pollOnce and publish hosted processor output for block zero");
    }
    proof.outputSeqAfterPoll = audioRtSide.outputSeq();

    fillInput (1);
    const RtLaneExchangeResult r1 =
        audioRtSide.exchangeBlock (inCh.data(), channels, numFrames, {}, outCh.data(), channels);
    proof.r1Source = r1.source;
    proof.r1OutputBlockIndex = r1.outputBlockIndex;
    if (r1.source != RtLaneOutput::Fresh || r1.outputBlockIndex != 0)
        return fail ("second RT-lane exchange did not receive fresh worker-processed block zero");

    for (int channel = 0; channel < channels; ++channel)
    {
        for (int frame = 0; frame < numFrames; ++frame)
        {
            const float expected = sampleFor (channel, 0, frame);
            const float observed = out[static_cast<std::size_t> (channel * numFrames + frame)];
            if (observed != expected)
            {
                proof.failedChannel = channel;
                proof.failedFrame = frame;
                proof.observed = observed;
                proof.expected = expected;
                return fail ("worker-hosted processor output did not match block zero input");
            }
        }
    }

    proof.stopStatus = loadCoordinator.requestStopAndWait().status;
    workerMayBeRunning = false;
    if (proof.stopStatus != PluginHostCoordinator::StopStatus::stopped)
        return fail ("worker did not stop after RT-lane poll proof");

    proof.passed = true;
    proof.failureStep = "passed";
    return proof;
}

RtLaneWorkerPollProof proveWorkerPollsHostedProcessorOverRtLane()
{
    static const RtLaneWorkerPollProof proof = computeWorkerPollsHostedProcessorOverRtLane();
    return proof;
}

struct WatchdogRecoveryProof
{
    bool passed = false;
    std::string failureStep = "not-run";
    PluginHostCoordinator::WatchdogStatus watchdogStatus {
        PluginHostCoordinator::WatchdogStatus::notStarted
    };
    PluginHostCoordinator::CrashStatus crashStatus {
        PluginHostCoordinator::CrashStatus::notStarted
    };
    PluginHostCoordinator::HostFailureKind watchdogFailure {
        PluginHostCoordinator::HostFailureKind::none
    };
    PluginHostCoordinator::HostFailureKind crashFailure {
        PluginHostCoordinator::HostFailureKind::none
    };
    PluginHostCoordinator::GraphRecompileStatus recoveryStatus {
        PluginHostCoordinator::GraphRecompileStatus::noAction
    };
    PluginHostCoordinator::GraphRecompileStatus missingPlaceholderStatus {
        PluginHostCoordinator::GraphRecompileStatus::noAction
    };
    bool watchdogAutoQueued = false;
    bool crashAutoQueued = false;
    bool liveGraphPublished = false;
    bool liveGraphAudible = false;
    bool placeholderCompiled = false;
    bool orderedPublishAccepted = false;
    bool graphRecompileExecuted = false;
    bool placeholderOutputSilent = false;
    bool missingPlaceholderRejected = false;
    std::size_t reclaimedGraphs = 0;
};

struct BlacklistPersistenceProof
{
    bool passed = false;
    std::string failureStep = "not-run";
    bool exactBeforeRestart = false;
    bool exactAfterRestart = false;
    bool wrongVersionAfterRestart = true;
    bool wrongFormatAfterRestart = true;
};

struct MixerRuntimeProjectionProof
{
    bool passed = false;
    std::string failureStep = "not-run";
    MixerProjectionError::Code buildError = MixerProjectionError::Code::None;
    bool negativeNoPolicyLeaks = false;
    bool maskCapacityPast64 = false;
    bool policyApplied = false;
    bool soloedTrackUnmuted = false;
    bool nonSoloedTrackMuted = false;
    bool soloSafeReturnUnmuted = false;
    bool highIndexTrackMuted = false;
    bool publishAccepted = false;
    bool runtimeRenderedStereo = false;
    std::uint32_t highIndexMuteBit = 0;
    float observedLeft = 0.0f;
    float observedRight = 0.0f;
    float expected = 0.0f;
};

MixerRuntimeProjectionProof computeMixerProjectionPublishesToRuntime()
{
    MixerRuntimeProjectionProof proof;
    auto fail = [&] (std::string step)
    {
        proof.failureStep = std::move (step);
        return proof;
    };

    {
        MixerProjectionError noPolicyError;
        std::unique_ptr<CompiledGraph> noPolicyGraph =
            buildMixerGraphProjection (hostedRuntimeProjectionInputs (20), &noPolicyError);
        if (noPolicyGraph == nullptr || noPolicyError.code != MixerProjectionError::Code::None)
        {
            proof.buildError = noPolicyError.code;
            return fail ("negative-control mixer projection did not build");
        }

        Runtime noPolicyRuntime;
        if (! noPolicyRuntime.publish (std::move (noPolicyGraph)))
            return fail ("negative-control mixer graph did not publish through Runtime");

        std::vector<float> left (kMixerRuntimeFrames, 0.0f);
        std::vector<float> right (kMixerRuntimeFrames, 0.0f);
        float* outputs[2] = { left.data(), right.data() };
        noPolicyRuntime.processBlock (outputs, 2, kMixerRuntimeFrames);

        const float leaked = 3.0f * kMixerRuntimeCenterGain;
        proof.negativeNoPolicyLeaks =
            std::abs (left.back() - leaked) < 0.0001f
            && std::abs (right.back() - leaked) < 0.0001f;
        if (! proof.negativeNoPolicyLeaks)
            return fail ("negative-control graph did not expose the non-soloed Send leak");
    }

    std::vector<NodeId> fillerSourceIds;
    MixerProjectionError graphError;
    std::unique_ptr<CompiledGraph> graph =
        buildMixerGraphProjection (hostedRuntimeProjectionInputs (21, &fillerSourceIds), &graphError);
    proof.buildError = graphError.code;
    if (graph == nullptr || graphError.code != MixerProjectionError::Code::None)
        return fail ("mixer projection did not build");

    NodeId highIndexSource = fillerSourceIds.empty() ? kMixerRuntimeSourceB : fillerSourceIds.back();
    const CompiledNode* highNode = compiledNodeById (*graph, highIndexSource);
    if (highNode == nullptr)
        return fail ("high-index mixer target was not compiled");
    proof.highIndexMuteBit = highNode->muteBit;
    proof.maskCapacityPast64 = highNode->muteBit >= 64u && graph->isMuteCapable (highIndexSource);
    if (! proof.maskCapacityPast64)
        return fail ("mixer policy target did not cross the old 64-node mute-mask ceiling");

    std::vector<MixerMuteTarget> targets;
    targets.push_back (MixerMuteTarget { kMixerRuntimeSourceA, false, true, false });
    targets.push_back (MixerMuteTarget { kMixerRuntimeSourceB, false, false, false });
    targets.push_back (MixerMuteTarget { kMixerRuntimeBusSum, false, false, true });
    for (const NodeId source : fillerSourceIds)
        targets.push_back (MixerMuteTarget { source, false, false, false });

    proof.policyApplied = applyMixerMutePolicy (*graph, targets);
    if (! proof.policyApplied)
        return fail ("mixer mute policy did not apply to every projected target");

    proof.soloedTrackUnmuted = ! graph->isMuted (kMixerRuntimeSourceA);
    proof.nonSoloedTrackMuted = graph->isMuted (kMixerRuntimeSourceB);
    proof.soloSafeReturnUnmuted = ! graph->isMuted (kMixerRuntimeBusSum);
    proof.highIndexTrackMuted = graph->isMuted (highIndexSource);
    if (! proof.soloedTrackUnmuted || ! proof.nonSoloedTrackMuted
        || ! proof.soloSafeReturnUnmuted || ! proof.highIndexTrackMuted)
        return fail ("published mixer policy mask did not match SIP/solo-safe expectations");

    Runtime runtime;
    proof.publishAccepted = runtime.publish (std::move (graph));
    if (! proof.publishAccepted)
        return fail ("policy-applied mixer graph did not publish through Runtime");

    std::vector<float> left (kMixerRuntimeFrames, 0.0f);
    std::vector<float> right (kMixerRuntimeFrames, 0.0f);
    float* outputs[2] = { left.data(), right.data() };
    runtime.processBlock (outputs, 2, kMixerRuntimeFrames);

    proof.expected = kMixerRuntimeCenterGain;
    proof.observedLeft = left.back();
    proof.observedRight = right.back();
    proof.runtimeRenderedStereo =
        std::abs (proof.observedLeft - proof.expected) < 0.0001f
        && std::abs (proof.observedRight - proof.expected) < 0.0001f;
    if (! proof.runtimeRenderedStereo)
        return fail ("Runtime output did not match SIP-safe projected mixer graph");

    proof.passed = true;
    proof.failureStep = "passed";
    return proof;
}

WatchdogRecoveryProof computeWatchdogRecoverySwapsPlaceholder()
{
    WatchdogRecoveryProof proof;
    auto fail = [&] (std::string step)
    {
        proof.failureStep = std::move (step);
        return proof;
    };

    const std::string workerPath = workerPathFromEnvironment();
    if (workerPath.empty())
        return fail ("YESDAW_PLUGIN_HOST_PATH is missing");

    const std::filesystem::path worker (workerPath);
    if (! std::filesystem::exists (worker))
        return fail ("worker executable path does not exist");

    RtLaneConfig cfg;
    cfg.channels = 1;
    cfg.maxBlockSize = 8;
    cfg.maxEventsPerBlock = 2;

    PluginHostCoordinator watchdogCoordinator { std::chrono::milliseconds (500),
                                                std::chrono::milliseconds (25) };
    const auto watchdog =
        watchdogCoordinator.launchAndExpectRunningWatchdogTimeout (juce::File (juce::String (workerPath)), cfg);
    proof.watchdogStatus = watchdog.status;
    proof.watchdogFailure = watchdogCoordinator.hostFailureReport().kind;
    if (watchdog.status != PluginHostCoordinator::WatchdogStatus::timeoutKilled
        || ! watchdog.runningRtLaneBacklogSeen
        || ! watchdog.runningRtLaneOutputProgressSeen
        || watchdog.runningRtLaneInputSeq <= watchdog.runningRtLaneOutputSeq
        || proof.watchdogFailure != PluginHostCoordinator::HostFailureKind::watchdogTimeout)
        return fail ("running watchdog did not kill a live child after RT-lane progress stalled");

    const auto watchdogAction = watchdogCoordinator.pendingFailureActionRequest();
    proof.watchdogAutoQueued =
        watchdogAction.action == PluginHostCoordinator::FailureActionKind::bypassAndRecompile
        && watchdogAction.failureKind == PluginHostCoordinator::HostFailureKind::watchdogTimeout
        && watchdogAction.bypassRequested
        && watchdogAction.recompileRequested;
    if (! proof.watchdogAutoQueued)
        return fail ("watchdog failure did not auto-enqueue bypass/recompile");

    Runtime runtime;
    std::vector<float> out (8u, 0.0f);
    GraphBuildError liveBuildError;
    auto liveGraph = GraphBuilder::build (recoveryGraphInputs (false, 11), &liveBuildError);
    proof.liveGraphPublished = liveGraph != nullptr
        && liveBuildError.code() == GraphBuildError::Code::None
        && runtime.publish (std::move (liveGraph));
    if (! proof.liveGraphPublished)
        return fail ("live simulated plugin graph did not publish through Runtime");

    runtime.processBlock (out.data(), static_cast<int> (out.size()));
    proof.liveGraphAudible = allSamplesNear (out, 0.75f);
    if (! proof.liveGraphAudible)
        return fail ("live simulated plugin graph was not audible before recovery");

    const auto recovery =
        watchdogCoordinator.executePendingFailureActionRequestToPlaceholderGraph (
            runtime, recoveryGraphInputs (true, 12), kRecoveryOffenderId);
    proof.recoveryStatus = recovery.status;
    proof.placeholderCompiled = recovery.placeholderCompiled;
    proof.orderedPublishAccepted = recovery.orderedPublishAccepted;
    proof.graphRecompileExecuted = recovery.graphRecompileExecuted
        && recovery.commandResult.graphRecompileExecuted;
    if (recovery.status != PluginHostCoordinator::GraphRecompileStatus::graphPublished
        || ! proof.placeholderCompiled
        || ! proof.orderedPublishAccepted
        || ! proof.graphRecompileExecuted)
        return fail ("placeholder graph recompile was not published");

    runtime.processBlock (out.data(), static_cast<int> (out.size()));
    proof.placeholderOutputSilent = allSamplesNear (out, 0.0f);
    proof.reclaimedGraphs = runtime.reclaim();
    if (! proof.placeholderOutputSilent || proof.reclaimedGraphs == 0u)
        return fail ("placeholder recovery did not replace the live graph through ordered publish");

    PluginHostCoordinator missingPlaceholderCoordinator;
    (void) missingPlaceholderCoordinator.queueFailureActionRequest (watchdogAction);
    Runtime missingPlaceholderRuntime;
    const auto missingPlaceholder =
        missingPlaceholderCoordinator.executePendingFailureActionRequestToPlaceholderGraph (
            missingPlaceholderRuntime, recoveryGraphInputs (false, 13), kRecoveryOffenderId);
    proof.missingPlaceholderStatus = missingPlaceholder.status;
    proof.missingPlaceholderRejected =
        missingPlaceholder.status == PluginHostCoordinator::GraphRecompileStatus::missingPlaceholder
        && ! missingPlaceholder.placeholderCompiled
        && ! missingPlaceholder.orderedPublishAccepted
        && ! missingPlaceholder.graphRecompileExecuted;
    if (! proof.missingPlaceholderRejected)
        return fail ("negative control accepted a non-placeholder recovery graph");

    PluginHostCoordinator crashCoordinator;
    const auto crash = crashCoordinator.launchAndExpectCrash (juce::File (juce::String (workerPath)));
    proof.crashStatus = crash.status;
    proof.crashFailure = crashCoordinator.hostFailureReport().kind;
    const auto crashAction = crashCoordinator.pendingFailureActionRequest();
    proof.crashAutoQueued =
        crash.status == PluginHostCoordinator::CrashStatus::connectionLost
        && proof.crashFailure == PluginHostCoordinator::HostFailureKind::crash
        && crashAction.action == PluginHostCoordinator::FailureActionKind::bypassAndRecompile
        && crashAction.failureKind == PluginHostCoordinator::HostFailureKind::crash
        && crashAction.bypassRequested
        && crashAction.recompileRequested;
    if (! proof.crashAutoQueued)
        return fail ("crash failure did not auto-enqueue bypass/recompile");

    proof.passed = true;
    proof.failureStep = "passed";
    return proof;
}

BlacklistPersistenceProof proveBlacklistPersistsAcrossRestart()
{
    BlacklistPersistenceProof proof;
    auto fail = [&] (std::string step)
    {
        proof.failureStep = std::move (step);
        return proof;
    };

    const auto path = makeTempBundlePath ("plugin-blacklist");
    constexpr auto format = PluginStateFormat::Vst3;
    const std::string uid = "com.yesdaw.h3.synthetic.crashy";
    const std::string version = "3.0.0";

    {
        ProjectBundleDb db;
        if (! ProjectBundleDb::openOrCreateBundle (path, db).ok())
            return fail ("bundle did not open for blacklist write");

        if (! db.writePluginBlacklistEntry ({ format, uid, version, "watchdog-timeout" }).ok())
            return fail ("blacklist row did not write");

        if (! db.pluginBlacklistContains (format, uid, version, proof.exactBeforeRestart).ok()
            || ! proof.exactBeforeRestart)
            return fail ("blacklist row was not visible before restart");
    }

    ProjectBundleDb reopened;
    if (! ProjectBundleDb::openExistingBundle (path, reopened).ok())
        return fail ("bundle did not reopen for blacklist read");

    if (! reopened.pluginBlacklistContains (format, uid, version, proof.exactAfterRestart).ok())
        return fail ("blacklist lookup failed after restart");
    if (! reopened.pluginBlacklistContains (format, uid, "3.0.1", proof.wrongVersionAfterRestart).ok())
        return fail ("blacklist wrong-version lookup failed after restart");
    if (! reopened.pluginBlacklistContains (PluginStateFormat::AudioUnit, uid, version, proof.wrongFormatAfterRestart).ok())
        return fail ("blacklist wrong-format lookup failed after restart");

    std::error_code ec;
    std::filesystem::remove_all (path, ec);

    if (! proof.exactAfterRestart || proof.wrongVersionAfterRestart || proof.wrongFormatAfterRestart)
        return fail ("blacklist key did not survive restart as an exact plugin identity");

    proof.passed = true;
    proof.failureStep = "passed";
    return proof;
}

WatchdogRecoveryProof proveWatchdogRecoverySwapsPlaceholder()
{
    static const WatchdogRecoveryProof proof = computeWatchdogRecoverySwapsPlaceholder();
    return proof;
}

BlacklistPersistenceProof proveBlacklistPersistsAcrossRestartCached()
{
    static const BlacklistPersistenceProof proof = proveBlacklistPersistsAcrossRestart();
    return proof;
}

MixerRuntimeProjectionProof proveMixerProjectionPublishesToRuntime()
{
    static const MixerRuntimeProjectionProof proof = computeMixerProjectionPublishesToRuntime();
    return proof;
}

HostIsolationGateState currentHostIsolationGateState()
{
    HostIsolationGateState state;
    state.syntheticProcessorRunsInWorkerChild = runSyntheticWorkerSelfCheck();
    state.rtLaneUsesOsSharedMemory = proveRtLaneUsesOsSharedMemory().passed;
    state.rtLaneIdentityPassesControlLane = proveRtLaneIdentityPassesControlLane().passed;
    state.workerPollsHostedProcessorOverRtLane = proveWorkerPollsHostedProcessorOverRtLane().passed;
    state.mixerProjectionPublishesToRuntime = proveMixerProjectionPublishesToRuntime().passed;
    const WatchdogRecoveryProof recovery = proveWatchdogRecoverySwapsPlaceholder();
    state.watchdogKillsHungOrCrashedChild =
        recovery.watchdogAutoQueued && recovery.crashAutoQueued;
    state.placeholderSwapUsesOrderedPublish =
        recovery.placeholderCompiled && recovery.orderedPublishAccepted && recovery.graphRecompileExecuted;
    state.blacklistPersistsAcrossRestart = proveBlacklistPersistsAcrossRestartCached().passed;
    return state;
}

bool hostIsolationGateSatisfied (HostIsolationGateState s) noexcept
{
    return s.syntheticProcessorRunsInWorkerChild
        && s.rtLaneUsesOsSharedMemory
        && s.rtLaneIdentityPassesControlLane
        && s.workerPollsHostedProcessorOverRtLane
        && s.mixerProjectionPublishesToRuntime
        && s.triStreamPdcThroughHostedPlugin
        && s.watchdogKillsHungOrCrashedChild
        && s.failOpenHasNoDeadlineMisses
        && s.placeholderSwapUsesOrderedPublish
        && s.blacklistPersistsAcrossRestart
        && s.opaqueStateRoundTripsAcrossProcess;
}

} // namespace

TEST_CASE ("synthetic hosted processor runs in the plugin host child", "[h3][host-isolation][synthetic]")
{
    REQUIRE (runSyntheticWorkerSelfCheck());
}

TEST_CASE ("RT lane uses OS-backed shared memory between parent and worker endpoints",
           "[h3][host-isolation][rtlane][shared-memory]")
{
    const RtLaneSharedMemoryProof proof = proveRtLaneUsesOsSharedMemory();
    CAPTURE (proof.failureStep,
             proof.inputSeq,
             proof.outputSeq,
             static_cast<int> (proof.r0Source),
             static_cast<int> (proof.r1Source),
             static_cast<int> (proof.childAttachFailure),
             proof.childAttachSystemError,
             static_cast<int> (proof.negativeAttachFailure),
             proof.negativeAttachSystemError,
             proof.r1OutputBlockIndex,
             proof.failedFrame,
             proof.observed,
             proof.expected);
    REQUIRE (proof.passed);
}

TEST_CASE ("coordinator passes RT-lane shared-memory identity over the control lane",
           "[h3][host-isolation][rtlane][control-lane]")
{
    const RtLaneControlLoadProof proof = proveRtLaneIdentityPassesControlLane();
    CAPTURE (proof.failureStep,
             static_cast<int> (proof.loadStatus),
             static_cast<int> (proof.loadReplyStatus),
             static_cast<int> (proof.loadAttachFailure),
             static_cast<int> (proof.loadStopStatus),
             static_cast<int> (proof.missingStatus),
             static_cast<int> (proof.missingReplyStatus),
             static_cast<int> (proof.missingAttachFailure),
             static_cast<int> (proof.absentStatus),
             static_cast<int> (proof.absentReplyStatus),
             static_cast<int> (proof.absentAttachFailure),
             proof.loadName,
             proof.activeName,
             proof.loadMessageSent,
             proof.loadReplySeen,
             proof.coordinatorAllocated,
             proof.coordinatorUsesOsSharedMemory,
             proof.workerAccepted,
             proof.activeUsesOsSharedMemory);
    REQUIRE (proof.passed);
}

TEST_CASE ("worker polls mapped RT lane through the hosted processor path",
           "[h3][host-isolation][rtlane][worker-poll]")
{
    const RtLaneWorkerPollProof proof = proveWorkerPollsHostedProcessorOverRtLane();
    CAPTURE (proof.failureStep,
             static_cast<int> (proof.loadStatus),
             static_cast<int> (proof.loadReplyStatus),
             static_cast<int> (proof.loadAttachFailure),
             static_cast<int> (proof.stopStatus),
             static_cast<int> (proof.audioAttachFailure),
             proof.audioAttachSystemError,
             proof.initialOutputSeq,
             proof.outputSeqAfterPoll,
             static_cast<int> (proof.r0Source),
             static_cast<int> (proof.r1Source),
             proof.r1OutputBlockIndex,
             proof.failedChannel,
             proof.failedFrame,
             proof.observed,
             proof.expected);
    REQUIRE (proof.passed);
}

TEST_CASE ("host-isolation plugin path runs inside projected mixer graph through Runtime",
           "[h3][host-isolation][mixer][runtime]")
{
    const MixerRuntimeProjectionProof proof = proveMixerProjectionPublishesToRuntime();
    CAPTURE (proof.failureStep,
             static_cast<int> (proof.buildError),
             proof.negativeNoPolicyLeaks,
             proof.maskCapacityPast64,
             proof.policyApplied,
             proof.soloedTrackUnmuted,
             proof.nonSoloedTrackMuted,
             proof.soloSafeReturnUnmuted,
             proof.highIndexTrackMuted,
             proof.highIndexMuteBit,
             proof.publishAccepted,
             proof.runtimeRenderedStereo,
             proof.observedLeft,
             proof.observedRight,
             proof.expected);
    REQUIRE (proof.passed);
}

TEST_CASE ("coordinator watchdog/crash recovery swaps a Placeholder through Runtime",
           "[h3][host-isolation][watchdog][placeholder]")
{
    const WatchdogRecoveryProof proof = proveWatchdogRecoverySwapsPlaceholder();
    CAPTURE (proof.failureStep,
             static_cast<int> (proof.watchdogStatus),
             static_cast<int> (proof.crashStatus),
             static_cast<int> (proof.watchdogFailure),
             static_cast<int> (proof.crashFailure),
             static_cast<int> (proof.recoveryStatus),
             static_cast<int> (proof.missingPlaceholderStatus),
             proof.watchdogAutoQueued,
             proof.crashAutoQueued,
             proof.liveGraphPublished,
             proof.liveGraphAudible,
             proof.placeholderCompiled,
             proof.orderedPublishAccepted,
             proof.graphRecompileExecuted,
             proof.placeholderOutputSilent,
             proof.missingPlaceholderRejected,
             proof.reclaimedGraphs);
    REQUIRE (proof.passed);
}

TEST_CASE ("plugin blacklist identity row persists across coordinator restart",
           "[h3][host-isolation][blacklist]")
{
    const BlacklistPersistenceProof proof = proveBlacklistPersistsAcrossRestartCached();
    CAPTURE (proof.failureStep,
             proof.exactBeforeRestart,
             proof.exactAfterRestart,
             proof.wrongVersionAfterRestart,
             proof.wrongFormatAfterRestart);
    REQUIRE (proof.passed);
}

TEST_CASE ("H3 host isolation exit gate is satisfied", "[h3][host-isolation][!shouldfail]")
{
    const HostIsolationGateState gate = currentHostIsolationGateState();

    CHECK (gate.syntheticProcessorRunsInWorkerChild);
    CHECK (gate.rtLaneUsesOsSharedMemory);
    CHECK (gate.rtLaneIdentityPassesControlLane);
    CHECK (gate.workerPollsHostedProcessorOverRtLane);
    CHECK (gate.mixerProjectionPublishesToRuntime);
    CHECK (gate.triStreamPdcThroughHostedPlugin);
    CHECK (gate.watchdogKillsHungOrCrashedChild);
    CHECK (gate.failOpenHasNoDeadlineMisses);
    CHECK (gate.placeholderSwapUsesOrderedPublish);
    CHECK (gate.blacklistPersistsAcrossRestart);
    CHECK (gate.opaqueStateRoundTripsAcrossProcess);

    REQUIRE (hostIsolationGateSatisfied (gate));
}
