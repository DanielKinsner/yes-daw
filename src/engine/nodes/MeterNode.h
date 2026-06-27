// YES DAW — MeterNode: a metering tap behind the Node contract (ADR-0008).
//
// A true passthrough — it reads the signal and leaves it untouched — that publishes this Block's peak
// (max |sample|) and RMS for the UI to read. The publish is a single std::atomic release-store per metric
// from the audio thread; the UI does an acquire-load. That is the whole synchronisation: one writer (the
// audio thread), one reader (the UI). NO compare-exchange loop in the hot path — a CAS loop can spin under
// contention and buys nothing for a single-writer value (ADR-0006; the design panel's must-fix).
//
// Pure C++ — no JUCE — so RTSan/TSan cover process(). In-place eligible: since it never writes the audio,
// the compiler may give it the same buffer for input and output.

#pragma once

#include "engine/Node.h"

#include <atomic>
#include <cmath>
#include <span>

namespace yesdaw::engine {

class MeterNode final : public Node
{
public:
    static constexpr int kMaxMeterChannels = 8;

    explicit MeterNode (NodeId id = 0, int channels = 1) noexcept
        : id_ (id),
          channels_ (channels > 0 ? (channels < kMaxMeterChannels ? channels : kMaxMeterChannels) : 1) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double /*sampleRate*/, int /*maxBlockSize*/) override {}   // no state to size

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;

        float       aggPeak  = 0.0f;   // aggregate peak/RMS across all channels (back-compat scalar)
        double      aggSumSq = 0.0;
        std::size_t aggCount = 0;

        for (int c = 0; c < channels; ++c)
        {
            const float* x = args.audio.channels[c];   // read-only: a meter never alters the signal
            float  chPeak  = 0.0f;
            double chSumSq = 0.0;
            for (int i = 0; i < args.numFrames; ++i)
            {
                const float a = std::fabs (x[i]);
                if (a > chPeak)
                    chPeak = a;
                chSumSq += static_cast<double> (x[i]) * static_cast<double> (x[i]);
            }

            const float chRms = args.numFrames > 0
                ? static_cast<float> (std::sqrt (chSumSq / static_cast<double> (args.numFrames)))
                : 0.0f;
            peakCh_[static_cast<std::size_t> (c)].store (chPeak, std::memory_order_release);   // per-channel L/R
            rmsCh_ [static_cast<std::size_t> (c)].store (chRms,  std::memory_order_release);

            if (chPeak > aggPeak)
                aggPeak = chPeak;
            aggSumSq += chSumSq;
            aggCount += static_cast<std::size_t> (args.numFrames);
        }

        // Clear channel slots this Block did not drive so a stale reading cannot linger on a now-silent
        // (or removed) channel.
        for (int c = channels; c < channels_; ++c)
        {
            peakCh_[static_cast<std::size_t> (c)].store (0.0f, std::memory_order_release);
            rmsCh_ [static_cast<std::size_t> (c)].store (0.0f, std::memory_order_release);
        }

        const float aggRms = aggCount > 0 ? static_cast<float> (std::sqrt (aggSumSq / static_cast<double> (aggCount))) : 0.0f;
        peak_.store (aggPeak, std::memory_order_release);   // single writer; UI acquire-loads
        rms_.store  (aggRms,  std::memory_order_release);
    }

    void reset() noexcept override
    {
        peak_.store (0.0f, std::memory_order_release);
        rms_.store  (0.0f, std::memory_order_release);
        for (int c = 0; c < channels_; ++c)
        {
            peakCh_[static_cast<std::size_t> (c)].store (0.0f, std::memory_order_release);
            rmsCh_ [static_cast<std::size_t> (c)].store (0.0f, std::memory_order_release);
        }
    }

    void release() override {}

    void setInput (Node* in) noexcept { input_ = in; }   // builder-only wiring

    // UI / control thread: read the latest published Block metrics.
    float peak() const noexcept { return peak_.load (std::memory_order_acquire); }   // max across channels
    float rms()  const noexcept { return rms_.load  (std::memory_order_acquire); }   // pooled across channels

    // Per-channel readout (a stereo meter shows L and R independently). Out-of-range channels read 0.
    float peak (int channel) const noexcept
    {
        return (channel >= 0 && channel < kMaxMeterChannels)
            ? peakCh_[static_cast<std::size_t> (channel)].load (std::memory_order_acquire) : 0.0f;
    }
    float rms (int channel) const noexcept
    {
        return (channel >= 0 && channel < kMaxMeterChannels)
            ? rmsCh_[static_cast<std::size_t> (channel)].load (std::memory_order_acquire) : 0.0f;
    }
    int channels() const noexcept { return channels_; }

private:
    NodeId             id_;
    int                channels_;
    Node*              input_ = nullptr;
    std::atomic<float> peak_ { 0.0f };
    std::atomic<float> rms_  { 0.0f };
    std::atomic<float> peakCh_[kMaxMeterChannels] {};
    std::atomic<float> rmsCh_ [kMaxMeterChannels] {};
};

} // namespace yesdaw::engine
