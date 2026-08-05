// YES DAW — real-hardware soak (H0 exit gate). THROWAWAY spike tooling.
//
// This is the ONE check that needs a real machine: "did sound actually leave the device, and were
// there zero dropouts over a long real-time run?" CI can't do it (runners have no audio device), so
// it's a self-asserting console app + tools/soak.sh — never "Dan listens".
//
// It opens the DEFAULT audio device, plays either the tamed 440 Hz SineSource or (with
// --playback-project) a tiny Project through PlaybackEngine for N seconds, and writes raw counters to
// stats.json. tools/soak.sh then decides PASS/FAIL from that JSON (measurement here, policy there). With
// --loopback (output physically/virtually wired back to an input) it also captures its own output and
// records RMS + a 440 Hz single-bin magnitude, which is how "sound actually came out, and it was the
// right tone" becomes a number instead of an ear.
//
// Links only juce_audio_devices (no GUI) so it stays light and needs only ALSA on Linux.
//
// Since U3 of the H17 packaged verifier the measurement callback + tone-Project fixture live in
// tools/soak/PlaybackSoakCallback.h + src/app/PlaybackCheckFixture.h, shared with the packaged
// YesDawHardwarePlaybackCheck so the playback engine path is never duplicated (KTD6). This main
// keeps its historical stats.json shape — tools/soak.ps1 / soak.sh parse it.

#include "PlaybackSoakCallback.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

namespace {

std::string argValue (int argc, char** argv, const std::string& flag, const std::string& fallback)
{
    for (int i = 1; i < argc - 1; ++i)
        if (flag == argv[i])
            return argv[i + 1];
    return fallback;
}
bool hasFlag (int argc, char** argv, const std::string& flag)
{
    for (int i = 1; i < argc; ++i)
        if (flag == argv[i]) return true;
    return false;
}

} // namespace

int main (int argc, char** argv)
{
    const double      seconds        = std::stod (argValue (argc, argv, "--seconds", "30"));
    const std::string statsPath      = argValue (argc, argv, "--stats-out", "stats.json");
    const bool        loopback       = hasFlag  (argc, argv, "--loopback");
    const bool        playbackMode   = hasFlag  (argc, argv, "--playback-project");
    const int         requestedBlock = (int) std::stol (argValue (argc, argv, "--block-size", "128"));

    const juce::ScopedJuceInitialiser_GUI juceInit;   // brings up the device backends; no message loop needed

    juce::AudioDeviceManager adm;
    // Request the H0 target block size (128 frames by default) — the tight-deadline stress case the
    // roadmap names. The device may not honour it (e.g. WASAPI shared mode); the actual size is
    // reported as block_size and the soak scripts check it against requested_block_size.
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.bufferSize = requestedBlock;
    const juce::String err = adm.initialise (loopback ? 2 : 0, 2, nullptr, true, {}, &setup);

    // Default shared-mode WASAPI rounds the block up to its 10 ms period (480 @ 48 kHz), which can
    // never meet the 128-frame target. Escalate through JUCE's lower-latency Windows backends until
    // one honours the request; whatever is actually granted is still reported honestly in stats.
    if (err.isEmpty())
    {
        const auto meetsBlock = [&adm, requestedBlock]
        {
            auto* d = adm.getCurrentAudioDevice();
            return d != nullptr && d->getCurrentBufferSizeSamples() <= requestedBlock;
        };

        if (! meetsBlock())
        {
            const juce::String defaultType = adm.getCurrentAudioDeviceType();
            for (auto* type : adm.getAvailableDeviceTypes())
                std::printf ("block-escalate: available type \"%s\"\n", type->getTypeName().toRawUTF8());
            bool met = false;
            for (const char* typeName : { "Windows Audio (Low Latency Mode)", "Windows Audio (Exclusive Mode)" })
            {
                adm.setCurrentAudioDeviceType (typeName, true);
                juce::AudioDeviceManager::AudioDeviceSetup retry = adm.getAudioDeviceSetup();
                retry.bufferSize = requestedBlock;
                const juce::String retryError = adm.setAudioDeviceSetup (retry, true);
                auto* granted = adm.getCurrentAudioDevice();
                int minAvailable = -1;
                if (granted != nullptr)
                    for (const int candidate : granted->getAvailableBufferSizes())
                        if (minAvailable < 0 || candidate < minAvailable)
                            minAvailable = candidate;
                std::printf ("block-escalate: %s -> %s (granted block %d, device min %d)\n",
                             typeName,
                             retryError.isEmpty() ? "opened" : retryError.toRawUTF8(),
                             granted != nullptr ? granted->getCurrentBufferSizeSamples() : -1,
                             minAvailable);
                if (retryError.isEmpty() && meetsBlock())
                {
                    met = true;
                    break;
                }
            }
            if (! met)
            {
                // No backend met the target: fall back to the default type so the soak still runs
                // and the script reports "block > target" with the real numbers. (Switching type
                // reopens the type's default device; no extra setup call, that would just churn
                // the stream again.)
                adm.setCurrentAudioDeviceType (defaultType, true);
            }
        }
    }

    auto writeStats = [&] (const std::string& deviceName, double sr, int block,
                           int xruns, int deadlineMisses, double maxCbMs, double budgetMs,
                           bool errored, long long loopCount, double loopRms, double loop440,
                           const std::string& mode,
                           const std::string& setupError)
    {
        std::ofstream o (statsPath, std::ios::binary);
        o << "{\n"
          << "  \"seconds\": "            << seconds        << ",\n"
          << "  \"device\": \""           << deviceName     << "\",\n"
          << "  \"sample_rate\": "         << sr             << ",\n"
          << "  \"block_size\": "          << block          << ",\n"
          << "  \"requested_block_size\": "<< requestedBlock << ",\n"
          << "  \"xruns\": "               << xruns          << ",\n"
          << "  \"deadline_misses\": "     << deadlineMisses << ",\n"
          << "  \"max_block_ms\": "        << maxCbMs        << ",\n"
          << "  \"block_budget_ms\": "     << budgetMs       << ",\n"
          << "  \"device_error\": "        << (errored ? "true" : "false") << ",\n"
          << "  \"mode\": \""              << mode           << "\",\n"
          << "  \"loopback\": "            << (loopback ? "true" : "false") << ",\n"
          << "  \"loopback_samples\": "    << loopCount      << ",\n"
          << "  \"loopback_peak_rms\": "   << loopRms        << ",\n"
          << "  \"loopback_440_mag\": "    << loop440        << ",\n"
          << "  \"setup_error\": \""       << setupError     << "\"\n"
          << "}\n";
    };

    if (err.isNotEmpty())
    {
        std::printf ("SOAK SETUP ERROR: %s\n", err.toRawUTF8());
        writeStats ("", 0, 0, -1, -1, 0, 0, false, 0, -1, -1,
                    playbackMode ? "playback_project" : "sine", err.toStdString());
        return 2;
    }

    auto* device = adm.getCurrentAudioDevice();
    if (device == nullptr)
    {
        std::printf ("SOAK SETUP ERROR: no audio device\n");
        writeStats ("", 0, 0, -1, -1, 0, 0, false, 0, -1, -1,
                    playbackMode ? "playback_project" : "sine", "no audio device");
        return 2;
    }

    yesdaw::soak::PlaybackSoakCallback cb { playbackMode, seconds };
    const int xrunStart = device->getXRunCount();   // -1 if unsupported
    adm.addAudioCallback (&cb);

    std::printf ("Soaking %.0f s on \"%s\" (%.0f Hz, %d-frame block)%s ...\n",
                 seconds, device->getName().toRawUTF8(), device->getCurrentSampleRate(),
                 device->getCurrentBufferSizeSamples(), loopback ? " [loopback]" : "");
    std::this_thread::sleep_for (std::chrono::duration<double> (seconds));

    adm.removeAudioCallback (&cb);
    const int xrunEnd = device->getXRunCount();
    const int xruns   = (xrunStart >= 0 && xrunEnd >= 0) ? (xrunEnd - xrunStart) : -1;

    const double budgetMs = cb.sampleRate() > 0.0 ? (cb.blockSize() / cb.sampleRate() * 1000.0) : 0.0;

    writeStats (device->getName().toStdString(), cb.sampleRate(), cb.blockSize(),
                xruns, cb.deadlineMisses(), cb.maxCallbackMs(), budgetMs,
                cb.errored(), cb.loopCount(), cb.loopRms(), cb.loop440Mag(), cb.modeName(), "");

    std::printf ("xruns=%d deadline_misses=%d max_block_ms=%.3f budget_ms=%.3f loop_rms=%.4f -> %s\n",
                 xruns, cb.deadlineMisses(), cb.maxCallbackMs(), budgetMs, cb.loopRms(), statsPath.c_str());

    adm.closeAudioDevice();
    return 0;   // wrote stats; tools/soak.sh decides PASS/FAIL
}
