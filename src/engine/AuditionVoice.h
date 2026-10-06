// YES DAW — the audition voice (ADR-0056 cp2): a decoded file heard on the monitor path, never in the mix.
//
// The control thread builds a voice (the file's samples at the project rate, its channel count and length) and
// publishes it through one atomic pointer; the device thread sums the voice's next frames into the outputs AFTER the
// engine's block (so the engine's loudness tap, an export, a bounce and a recording never carry it) and advances
// the voice's own read index, marking it finished at the file's end. The device thread only reads samples and
// advances an index: no allocation, lock, log or I/O. A voice that stops (a new audition, a second press, Play,
// Record, the browser closing, the file's end) is retired by the control thread and freed under the device-block
// watermark law (ADR-0046 §6), never while the device thread may still hold it.
//
// Pure C++ (no JUCE), so the RTSan leg runs sumAuditionVoice under -fsanitize=realtime.

#pragma once

#include "rt/RtHot.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace yesdaw::engine {

struct AuditionVoice
{
    std::vector<float> interleaved;   // at the project rate (control thread fills it before publishing)
    std::uint16_t channels = 0;       // 1 or 2
    std::uint64_t frames = 0;
    std::uint64_t position = 0;       // the device thread's read index
    std::atomic<bool> finished { false };
};

// DEVICE thread: add the voice's next `numFrames` frames to the outputs at unity — a mono voice to every output, a
// stereo voice channel to channel (outputs past the second get nothing) — and advance its index; at the end, mark
// it finished. A finished voice adds nothing.
inline void sumAuditionVoice (AuditionVoice& voice, float* const* outputChannels, int numOutputChannels,
                              int numFrames) noexcept YESDAW_RT_HOT
{
    if (voice.finished.load (std::memory_order_relaxed) || numFrames <= 0 || voice.channels == 0u)
        return;
    const std::uint64_t remaining = voice.frames - std::min (voice.position, voice.frames);
    const int frames = static_cast<int> (std::min<std::uint64_t> (remaining, static_cast<std::uint64_t> (numFrames)));
    const std::size_t channels = voice.channels;
    if (outputChannels != nullptr && frames > 0)
    {
        for (int out = 0; out < numOutputChannels; ++out)
        {
            float* const dest = outputChannels[out];
            if (dest == nullptr || (channels == 2u && out > 1))
                continue;
            const std::size_t source = channels == 1u ? 0u : static_cast<std::size_t> (out);
            const float* const samples = voice.interleaved.data() + static_cast<std::size_t> (voice.position) * channels + source;
            for (int frame = 0; frame < frames; ++frame)
                dest[frame] += samples[static_cast<std::size_t> (frame) * channels];
        }
    }
    voice.position += static_cast<std::uint64_t> (frames);
    if (voice.position >= voice.frames)
        voice.finished.store (true, std::memory_order_release);
}

} // namespace yesdaw::engine
