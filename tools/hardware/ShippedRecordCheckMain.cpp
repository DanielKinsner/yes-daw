// YES DAW — shipped-path hardware record proof (E35).
//
//   YesDawShippedRecordCheck [--seconds <n>] [--work-dir <dir>]
//   YesDawShippedRecordCheck --version
//
// Drives the SAME model verbs the shipped Record button calls — adoptRealRecordingDevice (the
// audioDeviceAboutToStart law), RecordingArmTrack, startRealRecordingCapture with the REAL
// device's parameters, processDeviceAudioBlock as the live callback, drainRealRecordingCapture,
// stopRealRecordingCaptureAndCommit — against the machine's real audio device. While capturing,
// the callback overlays a deterministic coded burst on the outputs; with a loopback route the
// burst returns on the inputs and the committed take must CONTAIN it (normalized
// cross-correlation, never a human ear). Self-asserting: prints PASS/FAIL and exits 0/1;
// setup problems (no device, no inputs, bundle failure) exit 2.

#include "ui/UiAppModel.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifndef YESDAW_VERSION_STRING
  #define YESDAW_VERSION_STRING "0.0.0-dev"
#endif

namespace {

constexpr double kBurstAmplitude = 0.25;
constexpr double kBurstSeconds = 0.1;
constexpr double kBurstStartSeconds = 0.5;
constexpr double kCorrelationSnrThreshold = 8.0;

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

// The same deterministic coded burst the packaged recording checker plays (sign sequence from
// a fixed LCG): distinctive under cross-correlation, unlike a pure tone.
std::vector<float> makeCodedBurst (double sampleRateHz)
{
    const auto frames = static_cast<std::size_t> (std::max (1.0, sampleRateHz * kBurstSeconds));
    std::vector<float> burst (frames, 0.0f);
    std::uint32_t state = 0x59534457u;   // "YSDW"
    for (float& sample : burst)
    {
        state = state * 1664525u + 1013904223u;
        sample = (state & 0x80000000u) != 0 ? static_cast<float> (kBurstAmplitude)
                                            : static_cast<float> (-kBurstAmplitude);
    }
    return burst;
}

// Live callback: EXACTLY the shipped shell's law — the model processes the block first, then
// the checker overlays its burst on the outputs (frame-cursor scheduled).
struct ShippedRecordCallback final : juce::AudioIODeviceCallback
{
    yesdaw::ui::UiAppModel& model;
    std::vector<float> burst;
    std::int64_t burstStartFrame = 0;
    std::atomic<std::int64_t> framesElapsed { 0 };
    std::atomic<int> callbackCount { 0 };
    std::atomic<int> inputChannelsSeen { -1 };

    explicit ShippedRecordCallback (yesdaw::ui::UiAppModel& appModel) : model (appModel) {}

    void audioDeviceAboutToStart (juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext (const float* const* inputChannels,
                                           int numInputChannels,
                                           float* const* outputChannels,
                                           int numOutputChannels,
                                           int numFrames,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        callbackCount.fetch_add (1);
        inputChannelsSeen.store (numInputChannels);
        (void) model.processDeviceAudioBlock (
            inputChannels, numInputChannels, outputChannels, numOutputChannels, numFrames);

        const std::int64_t blockStart = framesElapsed.fetch_add (numFrames);
        for (int frame = 0; frame < numFrames; ++frame)
        {
            const std::int64_t position = blockStart + frame - burstStartFrame;
            if (position < 0 || position >= static_cast<std::int64_t> (burst.size()))
                continue;
            for (int channel = 0; channel < numOutputChannels; ++channel)
                if (outputChannels[channel] != nullptr)
                    outputChannels[channel][frame] += burst[static_cast<std::size_t> (position)];
        }
    }
};

// Normalized cross-correlation peak vs the mean absolute correlation elsewhere (an SNR-style
// figure) — the mechanical "the burst is in the capture" test.
double correlateBurst (const std::vector<float>& capture, const std::vector<float>& burst)
{
    if (capture.size() < burst.size() + 1)
        return 0.0;

    double best = 0.0;
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t offset = 0; offset + burst.size() <= capture.size(); offset += 4)
    {
        double correlation = 0.0;
        for (std::size_t i = 0; i < burst.size(); i += 2)
            correlation += static_cast<double> (capture[offset + i]) * static_cast<double> (burst[i]);
        const double magnitude = std::abs (correlation);
        best = std::max (best, magnitude);
        sum += magnitude;
        ++count;
    }
    const double mean = count > 0 ? sum / static_cast<double> (count) : 0.0;
    return mean > 0.0 ? best / mean : 0.0;
}

} // namespace

int main (int argc, char** argv)
{
    if (hasFlag (argc, argv, "--version"))
    {
        std::printf ("YesDawShippedRecordCheck %s\n", YESDAW_VERSION_STRING);
        return 0;
    }

    const double seconds = argValue (argc, argv, "--seconds") != nullptr
        ? std::max (1.0, std::atof (argValue (argc, argv, "--seconds")))
        : 3.0;
    const std::filesystem::path workDir = argValue (argc, argv, "--work-dir") != nullptr
        ? std::filesystem::path (argValue (argc, argv, "--work-dir"))
        : std::filesystem::temp_directory_path() / "yesdaw-shipped-record-check";

    juce::MessageManager::getInstance();
    juce::AudioDeviceManager deviceManager;
    const juce::String initError = deviceManager.initialise (2, 2, nullptr, true);
    juce::AudioIODevice* const device = deviceManager.getCurrentAudioDevice();
    if (! initError.isEmpty() || device == nullptr)
    {
        std::printf ("SETUP: audio device initialise failed (%s)\n", initError.toRawUTF8());
        return 2;
    }
    const int activeInputs = device->getActiveInputChannels().countNumberOfSetBits();
    if (activeInputs <= 0)
    {
        std::puts ("SETUP: device has no active inputs (loopback route required)");
        return 2;
    }

    std::error_code ec;
    std::filesystem::remove_all (workDir, ec);
    std::filesystem::create_directories (workDir, ec);
    const std::filesystem::path bundlePath = workDir / "shipped-record.yesdaw";

    yesdaw::ui::UiAppModel model;
    // The device's block size must be known BEFORE the playback engine is built (the shell
    // sets it at device open, before any project loads) — otherwise real device blocks
    // exceed the engine's max and every callback refuses.
    model.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
    if (! model.createProjectBundle (bundlePath).ok())
    {
        std::puts ("SETUP: project bundle create failed");
        return 2;
    }

    // The SAME adoption law audioDeviceAboutToStart applies (E28).
    yesdaw::ui::UiRealRecordingDeviceProfile profile;
    const auto nameHash = static_cast<std::uint32_t> (device->getName().hashCode());
    profile.stableDeviceId = nameHash != 0u ? nameHash : 0xFFFFFFFFu;
    profile.sampleRateHz = device->getCurrentSampleRate();
    profile.inputChannels = activeInputs;
    profile.maxBlockSize = device->getCurrentBufferSizeSamples();
    profile.inputLatencyFrames = std::max (0, device->getInputLatencyInSamples());
    profile.outputLatencyFrames = std::max (0, device->getOutputLatencyInSamples());
    if (! model.adoptRealRecordingDevice (profile))
    {
        std::puts ("SETUP: real device profile rejected");
        return 2;
    }
    model.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
    // Mono channel 0: the widest-compatibility pick (drivers may deliver fewer callback
    // channels than the active mask advertises).
    if (! model.setRecordingInputChannel (0, false)
        || ! model.dispatch (yesdaw::ui::UiActionId::RecordingArmTrack).dispatched)
    {
        std::puts ("SETUP: arm failed");
        return 2;
    }

    // The SAME start law the Record button uses (E28/E29/E32).
    if (! model.startRealRecordingCapture (activeInputs,
                                           device->getCurrentSampleRate(),
                                           std::max (0, device->getInputLatencyInSamples()),
                                           std::max (0, device->getOutputLatencyInSamples())))
    {
        std::puts ("SETUP: startRealRecordingCapture refused");
        return 2;
    }

    ShippedRecordCallback callback { model };
    callback.burst = makeCodedBurst (device->getCurrentSampleRate());
    callback.burstStartFrame = static_cast<std::int64_t> (
        device->getCurrentSampleRate() * kBurstStartSeconds);
    deviceManager.addAudioCallback (&callback);

    const auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds (static_cast<int> (seconds * 1000.0));
    while (std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (50));
        model.drainRealRecordingCapture();
    }
    deviceManager.removeAudioCallback (&callback);

    const yesdaw::ui::UiAppRecordResult committed = model.stopRealRecordingCaptureAndCommit();
    if (! committed.ok())
    {
        std::printf ("FAIL: the shipped Record path committed no take (silent/unrouted input is"
                     " never masked) [callbacks=%d inputChannels=%d]\n",
                     callback.callbackCount.load(),
                     callback.inputChannelsSeen.load());
        return 1;
    }

    // Read the committed float-WAV back from the bundle and require the coded burst inside it.
    const yesdaw::ui::UiRecordedAudioTake take = model.lastRecordedAudioTake();
    const yesdaw::engine::Asset* recorded = nullptr;
    for (const yesdaw::engine::Asset& asset : model.project().assets)
        if (asset.id == take.assetId)
            recorded = &asset;
    if (recorded == nullptr)
    {
        std::puts ("FAIL: committed take asset missing from the project");
        return 1;
    }
    const std::filesystem::path wavPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (recorded->contentHash);
    std::ifstream in (wavPath, std::ios::binary);
    if (! in.good())
    {
        std::puts ("FAIL: committed take WAV unreadable");
        return 1;
    }
    in.seekg (44);
    std::vector<float> samples (static_cast<std::size_t> (take.frames) * take.channels, 0.0f);
    in.read (reinterpret_cast<char*> (samples.data()),
             static_cast<std::streamsize> (samples.size() * sizeof (float)));

    // Deinterleave channel 0 for the correlation.
    std::vector<float> channelZero (static_cast<std::size_t> (take.frames), 0.0f);
    for (std::size_t frame = 0; frame < channelZero.size(); ++frame)
        channelZero[frame] = samples[frame * take.channels];

    float peak = 0.0f;
    for (float sample : channelZero)
        peak = std::max (peak, std::abs (sample));

    const double snr = correlateBurst (channelZero, callback.burst);
    std::printf ("captured frames=%llu channels=%u peak=%.4f burst-snr=%.2f device=\"%s\"\n",
                 static_cast<unsigned long long> (take.frames),
                 static_cast<unsigned> (take.channels),
                 static_cast<double> (peak),
                 snr,
                 device->getName().toRawUTF8());

    if (snr >= kCorrelationSnrThreshold)
    {
        std::puts ("PASS: the shipped Record path captured the coded burst over the real device");
        return 0;
    }

    std::puts ("FAIL: coded burst not found in the capture (loopback route required — route an"
               " output back into the recorded input)");
    return 1;
}
