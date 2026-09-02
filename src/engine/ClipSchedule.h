// YES DAW - the live placement lane's data (G0.5, ADR-0046 §6, plan §5.3 lane 2).
//
// A Track's audio Clips are described to the engine as ONE immutable ClipSchedule per Track: which
// asset samples play where, with which fades and gain. The control thread builds a schedule, the
// audio thread swaps it in atomically (TrackClipScheduleNode), and the old one is retired to the
// Runtime janitor — so a move, trim, split, delete, fade or gain edit never rebuilds the graph.
//
// The per-frame law here is DecodedClipNode's, verbatim (ADR-0042 mono widening, the ClipEnvelope
// fade, the clip gain, float evaluation order), and the clips are summed in schedule order into a
// double accumulator exactly as the strip Sum accumulates its inputs — so a Track of N scheduled
// Clips renders bit-identically to the N per-clip source Nodes it replaces.
#pragma once

#include "engine/ClipEnvelope.h"
#include "engine/Node.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace yesdaw::engine {

// Decoded asset audio the schedule keeps alive. Interleaved by `channels`; `frames` frames.
struct AssetSamples
{
    std::vector<float> interleaved;
    int                channels = 1;
    std::uint64_t      frames   = 0;
};

// One Clip's placement, resolved to frames. `samples` points at the window start inside an
// AssetSamples the owning ClipSchedule keeps alive (`srcOffset` already applied).
struct ScheduledClip
{
    const float* samples        = nullptr;
    std::int64_t sourceFrames   = 0;    // playable frames in the window (min (srcLen, timeline length))
    int          sourceChannels = 1;    // 1 or 2 (ADR-0042)
    std::int64_t startFrame     = 0;    // timeline frame the window starts at
    std::int64_t fadeInFrames   = 0;
    std::int64_t fadeOutFrames  = 0;
    float        gain           = 1.0f;
};

struct ClipSchedule
{
    std::vector<ScheduledClip> clips;                            // Project clip order
    std::vector<std::shared_ptr<const AssetSamples>> keepAlive;  // every asset a clip points into
};

// Equal-power centre gain (cos(pi/4)) — DecodedClipNode's constant, ADR-0042.
inline constexpr float kScheduledClipEqualPowerCentreGain = 0.70710678118654752440f;

// AUDIO THREAD: accumulate every scheduled clip's contribution for one block into `accum`
// (numChannels * numFrames doubles, channel-major, caller-zeroed), then the caller writes floats.
// Branch-only positioned reads; no allocation, locks, or I/O.
inline void accumulateClipSchedule (const ClipSchedule& schedule,
                                    std::int64_t blockStart,
                                    int numFrames,
                                    int numChannels,
                                    int maxBlock,
                                    double* accum) noexcept YESDAW_RT_HOT
{
    for (const ScheduledClip& clip : schedule.clips)
    {
        if (clip.samples == nullptr || clip.sourceFrames <= 0 || clip.sourceChannels <= 0)
            continue;

        // Skip blocks that cannot touch this clip at all (the common case with many clips).
        const std::int64_t blockEnd = blockStart + static_cast<std::int64_t> (numFrames);
        if (blockEnd <= clip.startFrame || blockStart >= clip.startFrame + clip.sourceFrames)
            continue;

        const float monoWiden = clip.sourceChannels == 1 && numChannels > 1
                                    ? kScheduledClipEqualPowerCentreGain
                                    : 1.0f;
        if (clip.sourceChannels == 1)
        {
            for (int i = 0; i < numFrames; ++i)
            {
                const std::int64_t local = (blockStart + static_cast<std::int64_t> (i)) - clip.startFrame;
                if (local < 0 || local >= clip.sourceFrames)
                    continue;
                const float sample = clip.samples[static_cast<std::size_t> (local)]
                                   * evaluateClipFadeEnvelopeGain (local, clip.sourceFrames,
                                                                   clip.fadeInFrames, clip.fadeOutFrames)
                                   * clip.gain
                                   * monoWiden;
                for (int c = 0; c < numChannels; ++c)
                    accum[static_cast<std::size_t> (c) * static_cast<std::size_t> (maxBlock)
                          + static_cast<std::size_t> (i)] += static_cast<double> (sample);
            }
        }
        else
        {
            const int sourceChannels = clip.sourceChannels;
            for (int i = 0; i < numFrames; ++i)
            {
                const std::int64_t local = (blockStart + static_cast<std::int64_t> (i)) - clip.startFrame;
                if (local < 0 || local >= clip.sourceFrames)
                    continue;
                const float frameGain = evaluateClipFadeEnvelopeGain (local, clip.sourceFrames,
                                                                      clip.fadeInFrames, clip.fadeOutFrames)
                                      * clip.gain;
                const std::size_t base = static_cast<std::size_t> (local) * static_cast<std::size_t> (sourceChannels);
                for (int c = 0; c < numChannels; ++c)
                {
                    const float sample =
                        clip.samples[base + static_cast<std::size_t> (c < sourceChannels ? c : sourceChannels - 1)]
                        * frameGain;
                    accum[static_cast<std::size_t> (c) * static_cast<std::size_t> (maxBlock)
                          + static_cast<std::size_t> (i)] += static_cast<double> (sample);
                }
            }
        }
    }
}

} // namespace yesdaw::engine
