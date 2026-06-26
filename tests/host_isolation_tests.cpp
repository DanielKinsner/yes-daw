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
#include <vector>

using yesdaw::engine::Event;
using yesdaw::engine::RtLaneConfig;
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

bool proveRtLaneUsesOsSharedMemory()
{
    const std::string regionName = RtLaneRing::makeUniqueSharedMemoryName();

    RtLaneConfig cfg;
    cfg.channels = 1;
    cfg.maxBlockSize = 8;
    cfg.maxEventsPerBlock = 2;

    RtLaneRing parentRtSide;
    parentRtSide.prepareSharedMemory (cfg, regionName);
    if (! parentRtSide.usesOsSharedMemory())
        return false;

    RtLaneRing childWorkerSide;
    if (! childWorkerSide.attachSharedMemory (regionName))
        return false;
    if (! childWorkerSide.usesOsSharedMemory())
        return false;

    const std::string missingRegionName = RtLaneRing::makeUniqueSharedMemoryName();
    RtLaneRing invalidAttach;
    if (invalidAttach.attachSharedMemory (missingRegionName))
        return false;

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
    if (r0.source != RtLaneOutput::Silence || parentRtSide.inputSeq() != 1)
        return false;

    if (! childWorkerSide.pollOnce (doubleProcess))
        return false;
    if (parentRtSide.outputSeq() != 1)
        return false;

    fillInput (1);
    const RtLaneExchangeResult r1 = parentRtSide.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);
    if (r1.source != RtLaneOutput::Fresh || r1.outputBlockIndex != 0)
        return false;

    for (int frame = 0; frame < numFrames; ++frame)
    {
        const float expected = static_cast<float> (frame) * 2.0f;
        if (out[static_cast<std::size_t> (frame)] != expected)
            return false;
    }

    return true;
}

HostIsolationGateState currentHostIsolationGateState()
{
    HostIsolationGateState state;
    state.syntheticProcessorRunsInWorkerChild = runSyntheticWorkerSelfCheck();
    state.rtLaneUsesOsSharedMemory = proveRtLaneUsesOsSharedMemory();
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
    REQUIRE (proveRtLaneUsesOsSharedMemory());
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
