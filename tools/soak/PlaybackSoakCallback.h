// YES DAW — the real-hardware playback measurement callback, shared by tools/soak/SoakMain.cpp
// (the H0 soak smoke) and tools/hardware/PlaybackCheckMain.cpp (the H17 packaged playback
// checker). Factored out in U3 so the two never duplicate the playback engine path (KTD6).
//
// Fills the output with either the tamed 440 Hz SineSource or a real tone Project through
// PlaybackEngine, times each callback against the block deadline, accumulates output RMS (the
// mechanical "the Project rendered non-silence into the device buffer" proof), and — when an input
// is present — loopback RMS + a 440 Hz Goertzel magnitude. Everything on the audio thread is
// allocation- and lock-free: a harness that allocated there would create the very dropout it is
// trying to detect.
//
// Measurement notes carried over from the soak:
//   - deadlineMisses() is the 1.5x inter-arrival heuristic. It is DIAGNOSTIC ONLY — bursty (but
//     healthy) exclusive-WASAPI delivery trips it constantly, so it never gates a verdict until an
//     independent negative control proves it maps to a real callback-budget breach (KTD6).
//   - maxCallbackMs() vs the block budget IS authoritative: it measures our own work.
//   - errored() latches juce's audioDeviceError callback and any engine-create/render failure.

#pragma once

#include "app/PlaybackCheckFixture.h"
#include "dsp/SineSource.h"
#include "engine/PlaybackEngine.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace yesdaw::soak {

class PlaybackSoakCallback final : public juce::AudioIODeviceCallback
{
public:
    explicit PlaybackSoakCallback (bool playbackProject, double seconds)
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

        gWcoeff_ = 2.0 * std::cos (kTwoPi * app::hardware::kPlaybackToneHz
                                   / (sampleRate_ > 0.0 ? sampleRate_ : 48000.0));
        lastTick_ = 0;
        deadlineMisses_.store (0, std::memory_order_relaxed);
        maxCallbackMs_.store (0.0, std::memory_order_relaxed);
        loopSumSq_ = 0.0; loopCount_ = 0; g1_ = 0.0; g2_ = 0.0;
        outputSumSq_ = 0.0; outputCount_ = 0;
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

        // Output-side non-silence proof: RMS of what actually landed in channel 0 of the device
        // buffer, tone or Project alike. Plain arithmetic — RT-safe.
        if (numOutput > 0 && output != nullptr && output[0] != nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const double v = (double) output[0][i];
                outputSumSq_ += v * v;
            }
            outputCount_ += numSamples;
        }

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
    double outputRms() const          { return outputCount_ > 0 ? std::sqrt (outputSumSq_ / (double) outputCount_) : -1.0; }
    double loop440Mag() const
    {
        if (loopCount_ <= 0) return -1.0;
        const double w = kTwoPi * app::hardware::kPlaybackToneHz / sampleRate_;
        const double real = g1_ - g2_ * std::cos (w);
        const double imag = g2_ * std::sin (w);
        return std::sqrt (real * real + imag * imag) * 2.0 / (double) loopCount_;
    }
    const char* modeName() const { return playbackProject_ ? "playback_project" : "sine"; }

private:
    static constexpr double kTwoPi = 6.283185307179586476925286766559;

    void preparePlaybackProject()
    {
        fixture_ = app::hardware::buildTonePlaybackFixture (sampleRate_, blockSize_, seconds_);
        if (! fixture_.ok)
        {
            errored_.store (true, std::memory_order_relaxed);
            return;
        }

        engine::OfflineRenderOptions options;
        options.maxBlockSize = blockSize_;
        auto created = engine::PlaybackEngine::create (
            fixture_.project,
            std::span<const engine::DecodedAssetAudio> (fixture_.decodedAssets.data(),
                                                        fixture_.decodedAssets.size()),
            options);
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

    dsp::SineSource tone_;
    double sampleRate_ = 48000.0, expectedPeriodSec_ = 0.0, gWcoeff_ = 0.0;
    double seconds_ = 30.0;
    int    blockSize_  = 0;
    int64_t lastTick_  = 0;
    std::atomic<int>    deadlineMisses_ { 0 };
    std::atomic<double> maxCallbackMs_  { 0.0 };
    std::atomic<bool>   errored_        { false };
    bool playbackProject_ = false;
    app::hardware::TonePlaybackFixture fixture_;
    std::vector<float> nullOutputScratch_;
    std::unique_ptr<engine::PlaybackEngine> playback_;
    // loopback + output accumulators — written only by the audio thread, read after it stops.
    double    loopSumSq_ = 0.0, g1_ = 0.0, g2_ = 0.0;
    double    outputSumSq_ = 0.0;
    long long loopCount_ = 0;
    long long outputCount_ = 0;
};

} // namespace yesdaw::soak
