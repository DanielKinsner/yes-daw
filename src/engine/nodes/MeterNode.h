// YES DAW — MeterNode: a metering tap behind the Node contract (ADR-0008).
//
// A true passthrough — it reads the signal and leaves it untouched — that publishes every processed
// block's peak (max |sample|) through a PeakSinceRead source and this block's RMS through atomics
// for the UI to read. The publish is a single std::atomic release-store per metric from the audio
// thread; the UI does an acquire-load. One writer (the audio thread), one reader (the UI tick, in
// one place per source — ADR-0067). NO compare-exchange loop in the hot path (ADR-0006).
//
// ADR-0067 §5: the peak is a PeakSinceRead source — the max of every block since the last read,
// silence when no block ran (a stopped transport, a stalled device). RMS stays on this block's
// store so a stopped meter has nothing to leak forward — nothing meters RMS as a peak.
//
// Pure C++ — no JUCE — so RTSan/TSan cover process(). In-place eligible: since it never writes the audio,
// the compiler may give it the same buffer for input and output.

#pragma once

#include "engine/Node.h"
#include "engine/PeakSinceRead.h"

#include <array>
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
                                channels_, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ true };
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
            // ADR-0067 §5: the per-channel peak source accumulates every block's peak since the last read.
            perChannelPeak_[static_cast<std::size_t> (c)].publishBlock (chPeak);
            rmsCh_ [static_cast<std::size_t> (c)].store (chRms,  std::memory_order_release);

            if (chPeak > aggPeak)
                aggPeak = chPeak;
            aggSumSq += chSumSq;
            aggCount += static_cast<std::size_t> (args.numFrames);
        }

        // A now-silent channel reads 0: this block drives nothing, so the source counts a zero block
        // (the window's running max is unaffected by a zero, and the count still advances so the
        // reader cannot linger on a stale non-zero reading).
        for (int c = channels; c < channels_; ++c)
        {
            perChannelPeak_[static_cast<std::size_t> (c)].publishBlock (0.0f);
            rmsCh_ [static_cast<std::size_t> (c)].store (0.0f, std::memory_order_release);
        }

        const float aggRms = aggCount > 0 ? static_cast<float> (std::sqrt (aggSumSq / static_cast<double> (aggCount))) : 0.0f;
        aggregatePeak_.publishBlock (aggPeak);   // single writer; UI reads via readPeak()
        rms_.store  (aggRms,  std::memory_order_release);
    }

    void reset() noexcept override
    {
        // ADR-0067 §5: a reset is NOT a block — the peak sources stay as they are, so a reset
        // between blocks never drops the running window. RMS is this block's, so it falls to 0.
        rms_.store  (0.0f, std::memory_order_release);
        for (int c = 0; c < channels_; ++c)
            rmsCh_ [static_cast<std::size_t> (c)].store (0.0f, std::memory_order_release);
    }

    void release() override {}

    void setInput (Node* in) noexcept { input_ = in; }   // builder-only wiring

    // UI / control thread: read the latest published peak reading since the last read (ADR-0067 §5).
    // Non-const — the read advances the source's `consumed` cursor. Call once per UI tick, in the
    // one place per source the shell designates (the tick's meter step).
    [[nodiscard]] PeakSinceRead::Reading readPeak() noexcept   // max across channels
    {
        return aggregatePeak_.read();
    }

    [[nodiscard]] PeakSinceRead::Reading readPeak (int channel) noexcept
    {
        if (channel < 0 || channel >= kMaxMeterChannels)
            return {};   // out of range: empty reading
        return perChannelPeak_[static_cast<std::size_t> (channel)].read();
    }

    float rms()  const noexcept { return rms_.load  (std::memory_order_acquire); }   // pooled across channels

    // Per-channel RMS readout (a stereo meter shows L and R independently). Out-of-range channels read 0.
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
    PeakSinceRead      aggregatePeak_;                                                      // ADR-0067 §5
    std::array<PeakSinceRead, kMaxMeterChannels> perChannelPeak_ {};                         // ADR-0067 §5
    std::atomic<float> rms_  { 0.0f };
    std::atomic<float> rmsCh_ [kMaxMeterChannels] {};
};

} // namespace yesdaw::engine
