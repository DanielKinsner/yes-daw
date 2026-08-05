// YES DAW — packaged real-Project playback stage checker (H17 packaged verifier, U3).
//
//   YesDawHardwarePlaybackCheck [--run-id <id>] [--json-out <path>] [--seconds <n>]   exit 0/1/2
//   YesDawHardwarePlaybackCheck --version                                             exit 0
//
// Opens real audio hardware, requests the locked 48 kHz / 128-frame target through a DETERMINISTIC
// backend order (ASIO first when compiled in — U6 — then Windows low-latency/exclusive WASAPI,
// then the platform default), records EVERY route attempt with a stable reason code (R7), plays
// the shared tone Project through PlaybackEngine via the same callback the H0 soak uses (KTD6:
// one playback engine path), and emits the schema-v1 playback stage document.
//
// Honesty rules baked in: granted values are recorded independently of the request; a relaxed
// grant runs anyway but can only ever be a measured FAIL (AE2 — zero Underruns cannot rescue a
// 480-frame Block); the inter-arrival deadline heuristic is recorded as diagnostic and never
// gates; no flag or environment variable changes a threshold. --seconds only sets how long the
// measurement listens — the verdict policy (src/app/HardwareVerification.h) is fixed.
//
// Exit codes mirror the per-stage meaning of R10: 0 = locked gate met, 1 = a completed
// measurement violated it, 2 = setup/incomplete (no device, open failure, JSON write failure).

#include "app/HardwareVerification.h"
#include "../soak/PlaybackSoakCallback.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#ifndef YESDAW_VERSION_STRING
  #define YESDAW_VERSION_STRING "0.0.0-dev"
#endif

namespace hw = yesdaw::app::hardware;

namespace {

int printUsage()
{
    std::puts ("usage: YesDawHardwarePlaybackCheck [--run-id <id>] [--json-out <path>] [--seconds <n>]");
    std::puts ("       YesDawHardwarePlaybackCheck --version");
    return 2;
}

const char* argValue (int argc, char** argv, const char* name)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string (argv[i]) == name)
            return argv[i + 1];
    return nullptr;
}

bool hasFlag (int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc; ++i)
        if (std::string (argv[i]) == name)
            return true;
    return false;
}

// The deterministic route order (R6): ASIO first when that backend exists in this build, then the
// supported WASAPI modes — and on Windows NOTHING else. DirectSound is deliberately not a
// candidate: it "grants" a 128-frame callback chunk backed by a much larger ring buffer, a false
// latency claim this verifier must never credit (observed live 2026-08-04: DirectSound reported
// met_target and then xrunned within a 5 s run). On non-Windows platforms the platform default
// types keep JUCE's enumeration order, which is deterministic there.
std::vector<juce::String> candidateBackends (juce::AudioDeviceManager& adm)
{
    std::vector<juce::String> available;
    for (auto* type : adm.getAvailableDeviceTypes())
        available.push_back (type->getTypeName());

#if JUCE_WINDOWS
    const std::vector<juce::String> preferred = { "ASIO",
                                                  "Windows Audio (Low Latency Mode)",
                                                  "Windows Audio (Exclusive Mode)",
                                                  "Windows Audio" };
    std::vector<juce::String> order;
    for (const auto& name : preferred)
        for (const auto& have : available)
            if (have == name)
                order.push_back (name);
    return order;
#else
    return available;
#endif
}

struct RouteSelection
{
    std::vector<hw::PlaybackBackendAttempt> attempts;
    bool opened = false;      // some device is open (target met or diagnostic fallback)
    bool metTarget = false;
};

RouteSelection selectRoute (juce::AudioDeviceManager& adm)
{
    RouteSelection selection;

    for (const auto& backend : candidateBackends (adm))
    {
        hw::PlaybackBackendAttempt attempt;
        attempt.backend = backend;

        adm.setCurrentAudioDeviceType (backend, true);
        if (adm.getCurrentDeviceTypeObject() == nullptr
            || adm.getCurrentDeviceTypeObject()->getTypeName() != backend)
        {
            attempt.reasonCode = hw::kRouteTypeUnavailable;
            selection.attempts.push_back (attempt);
            continue;
        }

        juce::AudioDeviceManager::AudioDeviceSetup setup = adm.getAudioDeviceSetup();
        setup.sampleRate = hw::kRequiredSampleRateHz;
        setup.bufferSize = hw::kMaxGrantedBlockFrames;
        const juce::String error = adm.setAudioDeviceSetup (setup, true);

        auto* device = adm.getCurrentAudioDevice();
        if (error.isNotEmpty() || device == nullptr)
        {
            attempt.openError = error.isNotEmpty() ? error : juce::String { "no device opened" };
            attempt.reasonCode = device == nullptr && error.isEmpty() ? hw::kRouteNoDevice
                                                                      : hw::kRouteOpenError;
            selection.attempts.push_back (attempt);
            continue;
        }

        attempt.device = device->getName();
        attempt.grantedSampleRateHz = device->getCurrentSampleRate();
        attempt.grantedBlockFrames = device->getCurrentBufferSizeSamples();

        if (attempt.grantedSampleRateHz != hw::kRequiredSampleRateHz)
            attempt.reasonCode = hw::kRouteWrongSampleRate;
        else if (attempt.grantedBlockFrames > hw::kMaxGrantedBlockFrames)
            attempt.reasonCode = hw::kRouteBlockAboveTarget;
        else
            attempt.reasonCode = hw::kRouteMetTarget;

        selection.attempts.push_back (attempt);
        selection.opened = true;
        if (attempt.reasonCode == juce::String { hw::kRouteMetTarget })
        {
            selection.metTarget = true;
            return selection;   // first backend meeting the target wins, deterministically
        }
    }

    // No backend met the target. Keep whatever the LAST successful open was as the diagnostic
    // route (the plan retains relaxed runs as diagnostic evidence; the verdict still FAILs).
    return selection;
}

} // namespace

int main (int argc, char** argv)
{
    if (hasFlag (argc, argv, "--version"))
    {
        std::printf ("YesDawHardwarePlaybackCheck %s\n", YESDAW_VERSION_STRING);
        return 0;
    }
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--run-id" || arg == "--json-out" || arg == "--seconds") { ++i; continue; }
        return printUsage();
    }

    const char* runIdArg = argValue (argc, argv, "--run-id");
    const char* jsonOutArg = argValue (argc, argv, "--json-out");
    const char* secondsArg = argValue (argc, argv, "--seconds");
    const juce::String runId = runIdArg != nullptr ? juce::String { runIdArg }
                                                   : juce::String { "standalone" };
    const double seconds = secondsArg != nullptr ? std::stod (secondsArg) : 30.0;

    const juce::String startedAt = juce::Time::getCurrentTime().toISO8601 (true);
    const auto t0 = juce::Time::getMillisecondCounterHiRes();

    const juce::ScopedJuceInitialiser_GUI juceInit;

    const auto emitDocument = [&] (const hw::StageRecord& record) -> bool
    {
        if (jsonOutArg == nullptr)
            return true;
        std::string error;
        if (! hw::writeJsonAtomically (std::filesystem::path { jsonOutArg },
                                       hw::stageDocumentToVar (record, runId), error))
        {
            std::printf ("PLAYBACK ERROR: could not write result JSON: %s\n", error.c_str());
            return false;
        }
        return true;
    };

    const auto emitSetup = [&] (const std::vector<hw::PlaybackBackendAttempt>& attempts,
                                const char* why) -> int
    {
        hw::StageRecord record;
        record.stage = hw::kStagePlayback;
        record.state = hw::StageState::setup;
        record.startedAt = startedAt;
        record.completedAt = juce::Time::getCurrentTime().toISO8601 (true);
        record.durationMs = juce::Time::getMillisecondCounterHiRes() - t0;
        record.failureCodes.add (hw::kFailureDeviceUnavailable);
        record.detail = why;
        record.checkerVersion = YESDAW_VERSION_STRING;
        auto* meas = new juce::DynamicObject();
        meas->setProperty ("backend_attempts", hw::backendAttemptsToVar (attempts));
        record.measurements = juce::var { meas };
        emitDocument (record);
        std::printf ("PLAYBACK SETUP: %s\n", why);
        return 2;
    };

    juce::AudioDeviceManager adm;
    juce::AudioDeviceManager::AudioDeviceSetup initial;
    initial.sampleRate = hw::kRequiredSampleRateHz;
    initial.bufferSize = hw::kMaxGrantedBlockFrames;
    const juce::String initError = adm.initialise (0, 2, nullptr, true, {}, &initial);
    if (initError.isNotEmpty())
        return emitSetup ({}, ("audio device initialise failed: " + initError).toRawUTF8());

    const RouteSelection selection = selectRoute (adm);
    auto* device = adm.getCurrentAudioDevice();
    if (! selection.opened || device == nullptr)
        return emitSetup (selection.attempts, "no backend opened a usable output device");

    // Run the shared real-Project playback measurement on the selected (or diagnostic) route.
    yesdaw::soak::PlaybackSoakCallback callback { true, seconds };
    const int xrunStart = device->getXRunCount();   // -1 if the backend cannot report them
    adm.addAudioCallback (&callback);

    std::printf ("PLAYBACK measuring %.0f s on \"%s\" via %s (%.0f Hz, %d-frame block, target %d)...\n",
                 seconds, device->getName().toRawUTF8(),
                 adm.getCurrentAudioDeviceType().toRawUTF8(),
                 device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples(),
                 hw::kMaxGrantedBlockFrames);
    std::this_thread::sleep_for (std::chrono::duration<double> (seconds));

    adm.removeAudioCallback (&callback);
    const int xrunEnd = device->getXRunCount();

    hw::PlaybackMeasurement m;
    m.grantedSampleRateHz = callback.sampleRate();
    m.grantedBlockFrames = callback.blockSize();
    m.seconds = seconds;
    m.xruns = (xrunStart >= 0 && xrunEnd >= 0) ? (xrunEnd - xrunStart) : -1;
    m.deadlineMisses = callback.deadlineMisses();
    m.maxCallbackMs = callback.maxCallbackMs();
    m.blockBudgetMs = callback.sampleRate() > 0.0
                        ? (callback.blockSize() / callback.sampleRate() * 1000.0) : 0.0;
    m.deviceError = callback.errored();
    m.outputRms = callback.outputRms();
    m.deviceName = device->getName();
    m.backendName = adm.getCurrentAudioDeviceType();

    adm.closeAudioDevice();

    const hw::PlaybackVerdict verdict = hw::evaluatePlaybackMeasurement (m);
    const hw::StageRecord record = hw::makePlaybackStageRecord (
        m, verdict, selection.attempts, YESDAW_VERSION_STRING,
        startedAt, juce::Time::getCurrentTime().toISO8601 (true),
        juce::Time::getMillisecondCounterHiRes() - t0);

    if (! emitDocument (record))
        return 2;

    juce::String codes;
    for (const auto& code : verdict.failureCodes)
        codes += (codes.isEmpty() ? "" : ",") + code;

    std::printf ("PLAYBACK %s: granted %.0f Hz / %d frames (target 48000/%d) xruns=%d misses_diag=%d "
                 "max_cb_ms=%.3f budget_ms=%.3f output_rms=%.4f%s%s\n",
                 verdict.state == hw::StageState::pass ? "PASS" : "FAIL",
                 m.grantedSampleRateHz, m.grantedBlockFrames, hw::kMaxGrantedBlockFrames,
                 m.xruns, m.deadlineMisses, m.maxCallbackMs, m.blockBudgetMs, m.outputRms,
                 codes.isEmpty() ? "" : " codes=",
                 codes.toRawUTF8());

    return verdict.state == hw::StageState::pass ? 0 : 1;
}
