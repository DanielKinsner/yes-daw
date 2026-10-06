// YES DAW - ADR-0053: the live loudness tap.
//
// The mix (the graph's master output, before the metronome and input monitoring) leaves the audio thread through
// ONE single-producer / single-consumer float ring, preallocated once for the worst case it accepts (192 kHz x 2
// channels x 2 seconds) and owned for the model's whole lifetime, so every engine the model builds can feed the
// same ring and nothing is ever reallocated under a running callback. Each block is written as a header (frames,
// channels, sample rate — exact in float up to 2^24) followed by its interleaved samples, so the reader always
// knows the format of what it reads, even across an engine swap. A block that does not fit is skipped whole and
// counted; the reader marks its measurement approximate.

#pragma once

#include "rt/RtHot.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace yesdaw::engine {

class LoudnessTap
{
public:
    static constexpr std::size_t kMaxChannels = 2;
    static constexpr std::size_t kCapacityFloats = 192'000u * kMaxChannels * 2u;   // 2 s at the worst case
    static constexpr std::size_t kHeaderFloats = 3;                               // frames, channels, rate

    // MESSAGE thread, once (the model's construction).
    LoudnessTap() : ring_ (kCapacityFloats, 0.0f) {}
    LoudnessTap (const LoudnessTap&) = delete;
    LoudnessTap& operator= (const LoudnessTap&) = delete;

    // AUDIO thread: one block's first `channels` (1 or 2) channels, from `offset`, interleaved into the ring.
    // A copy and index stores only — no allocation, lock, log or I/O.
    void write (const float* const* source, int channels, int offset, int frames, double sampleRate) noexcept YESDAW_RT_HOT
    {
        if (source == nullptr || frames <= 0 || channels <= 0 || ! (sampleRate > 0.0))
            return;
        const std::size_t ch = std::min<std::size_t> (static_cast<std::size_t> (channels), kMaxChannels);
        const std::size_t count = static_cast<std::size_t> (frames);
        const std::size_t need = kHeaderFloats + ch * count;
        const std::uint64_t writeAt = write_.load (std::memory_order_relaxed);
        const std::uint64_t readAt = read_.load (std::memory_order_acquire);
        if (need > kCapacityFloats - static_cast<std::size_t> (writeAt - readAt))
        {
            overflows_.fetch_add (1u, std::memory_order_relaxed);
            return;
        }

        std::uint64_t at = writeAt;
        put (at++, static_cast<float> (count));
        put (at++, static_cast<float> (ch));
        put (at++, static_cast<float> (sampleRate));
        for (std::size_t frame = 0; frame < count; ++frame)
            for (std::size_t c = 0; c < ch; ++c)
                put (at++, source[c][static_cast<std::size_t> (offset) + frame]);
        write_.store (at, std::memory_order_release);
    }

    // MESSAGE thread: every complete block written so far, in order, handed to `sink (interleaved, frames,
    // channels, sampleRate)`; `scratch` holds one block (it grows here, never on the audio thread).
    template <typename Sink>
    void drain (std::vector<float>& scratch, Sink&& sink)
    {
        const std::uint64_t writeAt = write_.load (std::memory_order_acquire);
        std::uint64_t readAt = read_.load (std::memory_order_relaxed);
        while (readAt < writeAt)
        {
            const auto frames = static_cast<std::size_t> (get (readAt));
            const auto ch = static_cast<std::size_t> (get (readAt + 1u));
            const double rate = static_cast<double> (get (readAt + 2u));
            const std::size_t samples = frames * ch;
            scratch.resize (samples);
            for (std::size_t i = 0; i < samples; ++i)
                scratch[i] = get (readAt + kHeaderFloats + i);
            readAt += kHeaderFloats + samples;
            sink (static_cast<const float*> (scratch.data()), frames, ch, rate);
        }
        read_.store (readAt, std::memory_order_release);
    }

    // How many blocks were skipped because the ring was full (since construction).
    [[nodiscard]] std::uint64_t overflowCount() const noexcept { return overflows_.load (std::memory_order_relaxed); }

private:
    void put (std::uint64_t index, float value) noexcept YESDAW_RT_HOT
    {
        ring_[static_cast<std::size_t> (index % kCapacityFloats)] = value;
    }

    [[nodiscard]] float get (std::uint64_t index) const noexcept
    {
        return ring_[static_cast<std::size_t> (index % kCapacityFloats)];
    }

    std::vector<float> ring_;
    std::atomic<std::uint64_t> write_ { 0 };
    std::atomic<std::uint64_t> read_ { 0 };
    std::atomic<std::uint64_t> overflows_ { 0 };
};

} // namespace yesdaw::engine
