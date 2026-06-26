// YES DAW - H3 host-isolation close-out gate (ADR-0015 / ADR-0013 / ADR-0006 / ADR-0007).
//
// This target is the mechanical H3 finish line. It intentionally starts RED and is tagged
// [!shouldfail] so `main` stays green while the real hosted-process implementation is still absent.
// Replace each boolean with a real assertion as the corresponding close-out-plan item lands, then
// remove [!shouldfail] when the whole gate passes without inversion.

#include <catch2/catch_test_macros.hpp>

#include "engine/plugin/RtLaneRing.h"
#include "plugin_host/PluginHostCoordinator.h"

#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>
#include <utility>
#include <vector>

using yesdaw::engine::Event;
using yesdaw::engine::RtLaneConfig;
using yesdaw::engine::RtLaneAttachFailure;
using yesdaw::engine::RtLaneExchangeResult;
using yesdaw::engine::RtLaneOutput;
using yesdaw::engine::RtLaneRing;
using yesdaw::plugin_host::PluginHostCoordinator;
using yesdaw::plugin_host::RtLaneLoadConfig;
using yesdaw::plugin_host::RtLaneLoadReplyStatus;

namespace {

struct HostIsolationGateState
{
    bool syntheticProcessorRunsInWorkerChild = false;
    bool rtLaneUsesOsSharedMemory             = false;
    bool rtLaneIdentityPassesControlLane      = false;
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

HostIsolationGateState currentHostIsolationGateState()
{
    HostIsolationGateState state;
    state.syntheticProcessorRunsInWorkerChild = runSyntheticWorkerSelfCheck();
    state.rtLaneUsesOsSharedMemory = proveRtLaneUsesOsSharedMemory().passed;
    state.rtLaneIdentityPassesControlLane = proveRtLaneIdentityPassesControlLane().passed;
    return state;
}

bool hostIsolationGateSatisfied (HostIsolationGateState s) noexcept
{
    return s.syntheticProcessorRunsInWorkerChild
        && s.rtLaneUsesOsSharedMemory
        && s.rtLaneIdentityPassesControlLane
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

TEST_CASE ("H3 host isolation exit gate is satisfied", "[h3][host-isolation][!shouldfail]")
{
    const HostIsolationGateState gate = currentHostIsolationGateState();

    CHECK (gate.syntheticProcessorRunsInWorkerChild);
    CHECK (gate.rtLaneUsesOsSharedMemory);
    CHECK (gate.rtLaneIdentityPassesControlLane);
    CHECK (gate.mixerProjectionPublishesToRuntime);
    CHECK (gate.triStreamPdcThroughHostedPlugin);
    CHECK (gate.watchdogKillsHungOrCrashedChild);
    CHECK (gate.failOpenHasNoDeadlineMisses);
    CHECK (gate.placeholderSwapUsesOrderedPublish);
    CHECK (gate.blacklistPersistsAcrossRestart);
    CHECK (gate.opaqueStateRoundTripsAcrossProcess);

    REQUIRE (hostIsolationGateSatisfied (gate));
}
