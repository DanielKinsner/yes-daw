// YES DAW — reality-lane Smoke 2: ONE real VST3 across the worker boundary (M14; ADR-0037).
//
//   YesDawPluginSmoke --version
//   YesDawPluginSmoke --worker <YesDawPluginHost> --synthetic
//   YesDawPluginSmoke --worker <YesDawPluginHost> --plugin <absolute path to a .vst3>
//
// Launches the existing plugin-host worker child, loads ONE named plugin into it, processes N
// blocks of a known signal through the real RT lane (OS shared memory), and self-asserts:
//   * the worker accepted the load and stayed alive (no crash, no watchdog kill, no lost link)
//   * every output sample is finite (no NaN/Inf reached the lane)
//   * the plugin actually did something (real plugin) / passed through exactly (synthetic)
//   * the opaque state chunk round-trips (pull -> push -> the worker compares the bytes back)
//
// Prints PASS/FAIL and exits 0/1. Setup problems — no worker binary, no plugin installed, a path
// that is not a plugin — exit 2, never a false FAIL and never a false PASS.
//
// GUARDRAIL (ADR-0037): this is a SMOKE, not hosting. No scanner, no plugin identity or blacklist
// surface, no parameter surface, no editor. It names one file and processes audio through it.

#include "plugin_host/PluginHostCoordinator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifndef YESDAW_VERSION_STRING
  #define YESDAW_VERSION_STRING "unknown"
#endif

namespace {

constexpr int kChannels = 2;
constexpr int kBlockSize = 64;
constexpr int kBlockCount = 32;

bool hasFlag (int argc, char** argv, const char* flag) noexcept
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp (argv[i], flag) == 0)
            return true;

    return false;
}

std::string valueForFlag (int argc, char** argv, const char* flag)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp (argv[i], flag) == 0)
            return std::string (argv[i + 1]);

    return {};
}

int setupFailure (const std::string& reason)
{
    std::printf ("SETUP: %s\n", reason.c_str());
    return 2;
}

int smokeFailure (const std::string& reason)
{
    std::printf ("FAIL: %s\n", reason.c_str());
    return 1;
}

// The worker sits next to this tool in a packaged build, and next to its own artefacts directory
// in a build tree. Both are checked before giving up — the tool never guesses a plugin location.
juce::File resolveWorkerExecutable (const std::string& explicitPath)
{
    if (! explicitPath.empty())
        return juce::File (juce::String (explicitPath));

    const juce::File self = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    const juce::String workerName = "YesDawPluginHost"
                                  + self.getFileExtension();
    const juce::File beside = self.getSiblingFile (workerName);
    if (beside.existsAsFile())
        return beside;

    return self.getParentDirectory()
               .getParentDirectory()
               .getParentDirectory()
               .getChildFile ("YesDawPluginHost_artefacts")
               .getChildFile ("Release")
               .getChildFile (workerName);
}

// The ONE place this tool looks for a plugin when the operator did not name one. This is a
// convenience for the owner-machine run, NOT a scanner: first VST3 in the OS folder, no cache,
// no recursion into bundles, no identity, no blacklist.
juce::File findOneInstalledVst3()
{
   #if JUCE_WINDOWS
    const juce::File folder { "C:\\Program Files\\Common Files\\VST3" };
   #elif JUCE_MAC
    const juce::File folder { "/Library/Audio/Plug-Ins/VST3" };
   #else
    const juce::File folder { "/usr/lib/vst3" };
   #endif

    if (! folder.isDirectory())
        return {};

    for (const juce::File& entry : folder.findChildFiles (juce::File::findFilesAndDirectories, false, "*.vst3"))
        return entry;

    return {};
}

struct SmokeOutcome
{
    bool loaded = false;
    bool linkAlive = false;
    bool allFinite = true;
    bool anyFreshOutput = false;
    bool outputDiffersFromInput = false;
    bool outputMatchesInput = true;
    std::uint64_t freshBlocks = 0;
    float peakOut = 0.0f;
};

} // namespace

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    if (hasFlag (argc, argv, "--version"))
    {
        std::printf ("YesDawPluginSmoke %s\n", YESDAW_VERSION_STRING);
        return 0;
    }

    const juce::File worker = resolveWorkerExecutable (valueForFlag (argc, argv, "--worker"));
    if (! worker.existsAsFile())
        return setupFailure ("plugin-host worker executable not found: "
                             + worker.getFullPathName().toStdString()
                             + " (pass --worker <path>)");

    const bool syntheticMode = hasFlag (argc, argv, "--synthetic");
    juce::File pluginFile;
    if (! syntheticMode)
    {
        const std::string named = valueForFlag (argc, argv, "--plugin");
        pluginFile = named.empty() ? findOneInstalledVst3() : juce::File (juce::String (named));
        if (pluginFile == juce::File())
            return setupFailure ("no VST3 installed and none named — install one plugin, or pass "
                                 "--plugin <path>, or run --synthetic for the harness path");
        if (! pluginFile.exists())
            return setupFailure ("named plugin does not exist: "
                                 + pluginFile.getFullPathName().toStdString());
    }

    yesdaw::engine::RtLaneConfig config;
    config.channels = kChannels;
    config.maxBlockSize = kBlockSize;
    config.maxEventsPerBlock = 4;

    yesdaw::plugin_host::PluginHostCoordinator coordinator;
    const auto load = coordinator.launchAndLoadRtLane (
        worker, config, syntheticMode ? std::string {} : pluginFile.getFullPathName().toStdString());

    SmokeOutcome outcome;
    outcome.loaded = load.status == yesdaw::plugin_host::PluginHostCoordinator::RtLaneLoadStatus::success
                  && load.workerReplyStatus == yesdaw::plugin_host::RtLaneLoadReplyStatus::accepted
                  && load.workerAccepted;
    if (! outcome.loaded)
    {
        const bool pluginRejected =
            load.workerReplyStatus == yesdaw::plugin_host::RtLaneLoadReplyStatus::rejectedPluginLoadFailed;
        (void) coordinator.requestStopAndWait();
        if (pluginRejected)
            return setupFailure ("the worker could not load that file as a plugin: "
                                 + pluginFile.getFullPathName().toStdString());
        return smokeFailure ("worker did not accept the RT-lane load (status="
                             + std::to_string (static_cast<int> (load.status)) + " reply="
                             + std::to_string (static_cast<int> (load.workerReplyStatus)) + ")");
    }

    // A known, non-trivial signal: a quarter-amplitude ramp per block, so a passthrough is exactly
    // reproducible and any real processing is visible.
    std::vector<float> inputLeft (kBlockSize, 0.0f);
    std::vector<float> inputRight (kBlockSize, 0.0f);
    std::vector<float> outputLeft (kBlockSize, 0.0f);
    std::vector<float> outputRight (kBlockSize, 0.0f);
    for (int frame = 0; frame < kBlockSize; ++frame)
    {
        inputLeft[static_cast<std::size_t> (frame)] =
            0.25f * std::sin (6.2831853f * static_cast<float> (frame) / static_cast<float> (kBlockSize));
        inputRight[static_cast<std::size_t> (frame)] =
            -inputLeft[static_cast<std::size_t> (frame)];
    }

    float* inputChannels[kChannels] = { inputLeft.data(), inputRight.data() };
    float* outputChannels[kChannels] = { outputLeft.data(), outputRight.data() };

    for (int block = 0; block < kBlockCount; ++block)
    {
        std::fill (outputLeft.begin(), outputLeft.end(), 0.0f);
        std::fill (outputRight.begin(), outputRight.end(), 0.0f);

        const auto exchanged = coordinator.exchangeActiveRtLaneBlock (
            inputChannels, kChannels, kBlockSize, outputChannels, kChannels);

        if (exchanged.deliveredFresh)
        {
            outcome.anyFreshOutput = true;
            ++outcome.freshBlocks;

            for (int channel = 0; channel < kChannels; ++channel)
            {
                const float* out = outputChannels[channel];
                const float* in = inputChannels[channel];
                for (int frame = 0; frame < kBlockSize; ++frame)
                {
                    const float sample = out[frame];
                    if (! std::isfinite (sample))
                        outcome.allFinite = false;
                    outcome.peakOut = std::max (outcome.peakOut, std::abs (sample));
                    if (std::abs (sample - in[frame]) > 1.0e-6f)
                    {
                        outcome.outputDiffersFromInput = true;
                        outcome.outputMatchesInput = false;
                    }
                }
            }
        }

        // The worker polls off the audio thread; give it a slice to turn the block around.
        std::this_thread::sleep_for (std::chrono::milliseconds (2));
    }

    outcome.linkAlive = ! coordinator.workerConnectionLost();
    const auto stateRoundTrip = coordinator.roundTripPluginStateOnLiveWorker();
    const auto stop = coordinator.requestStopAndWait();

    if (! outcome.linkAlive)
        return smokeFailure ("the worker died while processing (crash or watchdog kill)");
    if (! outcome.anyFreshOutput)
        return smokeFailure ("the worker never returned a processed block through the RT lane");
    if (! outcome.allFinite)
        return smokeFailure ("a non-finite (NaN/Inf) sample came back through the RT lane");
    if (syntheticMode && ! outcome.outputMatchesInput)
        return smokeFailure ("the synthetic passthrough changed the signal");
    if (! syntheticMode && ! outcome.outputDiffersFromInput)
        return smokeFailure ("the loaded plugin left the signal bit-identical — pick a plugin that "
                             "processes audio at its default settings");
    if (! stateRoundTrip.pulled)
        return smokeFailure ("the hosted plugin's opaque state chunk could not be pulled (status="
                             + std::to_string (static_cast<int> (stateRoundTrip.pullStatus)) + ")");
    if (! stateRoundTrip.restored)
        return smokeFailure ("the hosted plugin's opaque state chunk did not round-trip (status="
                             + std::to_string (static_cast<int> (stateRoundTrip.pushStatus)) + ")");
    if (stop.status != yesdaw::plugin_host::PluginHostCoordinator::StopStatus::stopped)
        return smokeFailure ("the worker did not stop cleanly");

    std::printf ("PASS: %s crossed the worker boundary; blocks=%llu peak=%.4f state-bytes=%u "
                 "signal=%s plugin=\"%s\"\n",
                 syntheticMode ? "the synthetic worker plugin" : "one real plugin",
                 static_cast<unsigned long long> (outcome.freshBlocks),
                 static_cast<double> (outcome.peakOut),
                 stateRoundTrip.chunkLength,
                 syntheticMode ? "passthrough-exact" : "processed",
                 syntheticMode ? "synthetic" : pluginFile.getFullPathName().toRawUTF8());
    return 0;
}
