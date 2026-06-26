// YES DAW - H3 host-isolation close-out gate (ADR-0015 / ADR-0013 / ADR-0006 / ADR-0007).
//
// This target is the mechanical H3 finish line. It intentionally starts RED and is tagged
// [!shouldfail] so `main` stays green while the real hosted-process implementation is still absent.
// Replace each boolean with a real assertion as the corresponding close-out-plan item lands, then
// remove [!shouldfail] when the whole gate passes without inversion.

#include <catch2/catch_test_macros.hpp>

#include "engine/plugin/RtLaneRing.h"

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

namespace {

struct HostIsolationGateState
{
    bool syntheticProcessorRunsInWorkerChild = false;
    bool rtLaneUsesOsSharedMemory             = false;
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

HostIsolationGateState currentHostIsolationGateState()
{
    HostIsolationGateState state;
    state.syntheticProcessorRunsInWorkerChild = runSyntheticWorkerSelfCheck();
    state.rtLaneUsesOsSharedMemory = proveRtLaneUsesOsSharedMemory().passed;
    return state;
}

bool hostIsolationGateSatisfied (HostIsolationGateState s) noexcept
{
    return s.syntheticProcessorRunsInWorkerChild
        && s.rtLaneUsesOsSharedMemory
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

TEST_CASE ("H3 host isolation exit gate is satisfied", "[h3][host-isolation][!shouldfail]")
{
    const HostIsolationGateState gate = currentHostIsolationGateState();

    CHECK (gate.syntheticProcessorRunsInWorkerChild);
    CHECK (gate.rtLaneUsesOsSharedMemory);
    CHECK (gate.mixerProjectionPublishesToRuntime);
    CHECK (gate.triStreamPdcThroughHostedPlugin);
    CHECK (gate.watchdogKillsHungOrCrashedChild);
    CHECK (gate.failOpenHasNoDeadlineMisses);
    CHECK (gate.placeholderSwapUsesOrderedPublish);
    CHECK (gate.blacklistPersistsAcrossRestart);
    CHECK (gate.opaqueStateRoundTripsAcrossProcess);

    REQUIRE (hostIsolationGateSatisfied (gate));
}
