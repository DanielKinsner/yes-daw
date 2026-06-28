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

#include "dsp/SineSource.h"
#include "engine/PlaybackEngine.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr double kTwoPi  = 6.283185307179586476925286766559;
constexpr double kToneHz = 440.0;

constexpr yesdaw::engine::EntityId idFromLowByte (std::uint8_t low) noexcept
{
    yesdaw::engine::EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return yesdaw::engine::EntityId::fromBytes (bytes);
}

yesdaw::engine::AssetContentHash hashWithSeed (std::uint8_t seed) noexcept
{
    yesdaw::engine::AssetContentHash hash;
    for (std::size_t i = 0; i < hash.bytes.size(); ++i)
        hash.bytes[i] = static_cast<std::uint8_t> (seed + static_cast<std::uint8_t> (i * 11u));

    return hash;
}

// Audio callback: fills the output with the tamed tone, times each callback against the block
// deadline, and (when an input is present) accumulates loopback RMS + a 440 Hz Goertzel magnitude.
// Everything here is allocation- and lock-free — a soak harness that allocated on the audio thread
// would create the very dropout it's trying to detect.
class SoakCallback final : public juce::AudioIODeviceCallback
{
public:
    explicit SoakCallback (bool playbackProject, double seconds)
        : seconds_ (seconds), playbackProject_ (playbackProject)
    {
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        sampleRate_        = device->getCurrentSampleRate();
        blockSize_         = device->getCurrentBufferSizeSamples();
        expectedPeriodSec_ = (sampleRate_ > 0.0) ? (blockSize_ / sampleRate_) : 0.0;
        if (playbackProject_)
            preparePlaybackProject();
        else
            tone_.prepare (sampleRate_);

        gWcoeff_ = 2.0 * std::cos (kTwoPi * kToneHz / (sampleRate_ > 0.0 ? sampleRate_ : 48000.0));
        lastTick_ = 0;
        deadlineMisses_.store (0, std::memory_order_relaxed);
        maxCallbackMs_.store (0.0, std::memory_order_relaxed);
        loopSumSq_ = 0.0; loopCount_ = 0; g1_ = 0.0; g2_ = 0.0;
    }

    void audioDeviceIOCallbackWithContext (const float* const* input, int numInput,
                                           float* const* output, int numOutput,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        const int64_t enter = juce::Time::getHighResolutionTicks();
        const double  ticksPerSec = (double) juce::Time::getHighResolutionTicksPerSecond();

        // Inter-arrival deadline check (catches an audio-thread stall the device may not report).
        if (lastTick_ != 0 && expectedPeriodSec_ > 0.0)
        {
            const double gap = (double) (enter - lastTick_) / ticksPerSec;
            if (gap > 1.5 * expectedPeriodSec_)
                deadlineMisses_.fetch_add (1, std::memory_order_relaxed);
        }
        lastTick_ = enter;

        for (int i = 0; i < numSamples; ++i)
        {
            if (! playbackProject_)
            {
                const float s = tone_.nextSample();
                for (int ch = 0; ch < numOutput; ++ch)
                    if (output[ch] != nullptr)
                        output[ch][i] = s;
            }

            if (numInput > 0 && input != nullptr && input[0] != nullptr)
            {
                const double v = (double) input[0][i];
                loopSumSq_ += v * v;
                ++loopCount_;
                const double g0 = v + gWcoeff_ * g1_ - g2_;   // Goertzel @ 440 Hz
                g2_ = g1_; g1_ = g0;
            }
        }

        if (playbackProject_)
            renderPlaybackProject (output, numOutput, numSamples);

        // How long this callback took, vs the block period it must beat.
        const double tookMs = (double) (juce::Time::getHighResolutionTicks() - enter) / ticksPerSec * 1000.0;
        double prev = maxCallbackMs_.load (std::memory_order_relaxed);
        while (tookMs > prev && ! maxCallbackMs_.compare_exchange_weak (prev, tookMs, std::memory_order_relaxed)) {}
    }

    void audioDeviceStopped() override {}
    void audioDeviceError (const juce::String&) override { errored_.store (true, std::memory_order_relaxed); }

    // Read these only after removeAudioCallback() (no concurrent callback).
    double sampleRate() const         { return sampleRate_; }
    int    blockSize()  const         { return blockSize_; }
    int    deadlineMisses() const     { return deadlineMisses_.load (std::memory_order_relaxed); }
    double maxCallbackMs() const      { return maxCallbackMs_.load (std::memory_order_relaxed); }
    bool   errored() const            { return errored_.load (std::memory_order_relaxed); }
    long long loopCount() const       { return loopCount_; }
    double loopRms() const            { return loopCount_ > 0 ? std::sqrt (loopSumSq_ / (double) loopCount_) : -1.0; }
    double loop440Mag() const
    {
        if (loopCount_ <= 0) return -1.0;
        const double w = kTwoPi * kToneHz / sampleRate_;
        const double real = g1_ - g2_ * std::cos (w);
        const double imag = g2_ * std::sin (w);
        return std::sqrt (real * real + imag * imag) * 2.0 / (double) loopCount_;
    }
    const char* modeName() const { return playbackProject_ ? "playback_project" : "sine"; }

private:
    void preparePlaybackProject()
    {
        if (sampleRate_ <= 0.0 || blockSize_ <= 0)
        {
            errored_.store (true, std::memory_order_relaxed);
            return;
        }

        const std::uint64_t frames = static_cast<std::uint64_t> (
            std::max (1.0, std::ceil (seconds_ * sampleRate_))) + static_cast<std::uint64_t> (blockSize_);
        if (frames > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max()))
        {
            errored_.store (true, std::memory_order_relaxed);
            return;
        }

        playbackSamples_.assign (static_cast<std::size_t> (frames), 0.0f);
        double phase = 0.0;
        const double phaseStep = kTwoPi * kToneHz / sampleRate_;
        for (float& sample : playbackSamples_)
        {
            sample = static_cast<float> (0.18 * std::sin (phase));
            phase += phaseStep;
            if (phase >= kTwoPi)
                phase -= kTwoPi;
        }

        yesdaw::engine::Asset asset;
        asset.id = idFromLowByte (1);
        asset.contentHash = hashWithSeed (1);
        asset.frames = frames;
        asset.sampleRate = yesdaw::engine::SampleRate { sampleRate_ };
        asset.channels = 1;

        yesdaw::engine::Clip clip;
        clip.id = idFromLowByte (2);
        clip.assetId = asset.id;
        clip.timelineStart = 0;
        clip.timelineLength = static_cast<yesdaw::engine::Tick> (frames);
        clip.srcOffset = 0;
        clip.srcLen = frames;
        clip.gain = 1.0f;
        clip.timeBase = yesdaw::engine::TimeBase::SampleLocked;

        project_ = {};
        project_.id = idFromLowByte (3);
        project_.sampleRate = yesdaw::engine::SampleRate { sampleRate_ };
        project_.assets = { asset };
        project_.clips = { clip };

        decodedAssets_ = {
            yesdaw::engine::DecodedAssetAudio {
                asset.id,
                asset.sampleRate,
                asset.frames,
                asset.channels,
                std::span<const float> (playbackSamples_.data(), playbackSamples_.size()),
            },
        };

        yesdaw::engine::OfflineRenderOptions options;
        options.maxBlockSize = blockSize_;
        auto created = yesdaw::engine::PlaybackEngine::create (
            project_, std::span<const yesdaw::engine::DecodedAssetAudio> (decodedAssets_.data(), decodedAssets_.size()), options);
        if (! created.ok())
        {
            errored_.store (true, std::memory_order_relaxed);
            return;
        }

        playback_ = std::move (created.engine);
        nullOutputScratch_.assign (static_cast<std::size_t> (kMaxOutputs) * static_cast<std::size_t> (blockSize_), 0.0f);
    }

    void renderPlaybackProject (float* const* output, int numOutput, int numSamples) noexcept
    {
        if (playback_ == nullptr || numOutput <= 0 || numOutput > kMaxOutputs || numSamples > blockSize_)
        {
            zeroOutputs (output, numOutput, numSamples);
            errored_.store (true, std::memory_order_relaxed);
            return;
        }

        std::array<float*, kMaxOutputs> outputs {};
        for (int channel = 0; channel < numOutput; ++channel)
        {
            if (output != nullptr && output[channel] != nullptr)
            {
                outputs[static_cast<std::size_t> (channel)] = output[channel];
            }
            else
            {
                outputs[static_cast<std::size_t> (channel)] =
                    nullOutputScratch_.data() + static_cast<std::size_t> (channel) * static_cast<std::size_t> (blockSize_);
            }
        }

        playback_->processBlock (outputs.data(), numOutput, numSamples);
    }

    static void zeroOutputs (float* const* output, int numOutput, int numSamples) noexcept
    {
        if (output == nullptr)
            return;

        for (int channel = 0; channel < numOutput; ++channel)
            if (output[channel] != nullptr)
                for (int frame = 0; frame < numSamples; ++frame)
                    output[channel][frame] = 0.0f;
    }

    static constexpr int kMaxOutputs = 64;

    yesdaw::dsp::SineSource tone_;
    double sampleRate_ = 48000.0, expectedPeriodSec_ = 0.0, gWcoeff_ = 0.0;
    double seconds_ = 30.0;
    int    blockSize_  = 0;
    int64_t lastTick_  = 0;
    std::atomic<int>    deadlineMisses_ { 0 };
    std::atomic<double> maxCallbackMs_  { 0.0 };
    std::atomic<bool>   errored_        { false };
    bool playbackProject_ = false;
    yesdaw::engine::Project project_;
    std::vector<float> playbackSamples_;
    std::vector<float> nullOutputScratch_;
    std::vector<yesdaw::engine::DecodedAssetAudio> decodedAssets_;
    std::unique_ptr<yesdaw::engine::PlaybackEngine> playback_;
    // loopback accumulators — written only by the audio thread, read after it stops.
    double    loopSumSq_ = 0.0, g1_ = 0.0, g2_ = 0.0;
    long long loopCount_ = 0;
};

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

    SoakCallback cb { playbackMode, seconds };
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
