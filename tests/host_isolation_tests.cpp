// YES DAW - H3 host-isolation close-out gate (ADR-0015 / ADR-0013 / ADR-0006 / ADR-0007).
//
// This target is the mechanical H3 finish line. It intentionally starts RED and is tagged
// [!shouldfail] so `main` stays green while the real hosted-process implementation is still absent.
// Replace each boolean with a real assertion as the corresponding close-out-plan item lands, then
// remove [!shouldfail] when the whole gate passes without inversion.

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <string>

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

HostIsolationGateState currentHostIsolationGateState()
{
    HostIsolationGateState state;
    state.syntheticProcessorRunsInWorkerChild = runSyntheticWorkerSelfCheck();
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
