// YES DAW - decoded Asset Clip source node.
//
// H2 starts with bundled Asset bytes decoded on the control side, then projected into the same graph
// path as every other source. The audio-thread process path only reads pre-owned samples.

#pragma once

#include "engine/ClipEnvelope.h"
#include "engine/Node.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace yesdaw::engine {

class DecodedClipNode final : public Node
{
public:
    // timelineStartFrames positions the clip on the timeline: the node emits silence until the global
    // playhead reaches it, then plays its samples, then silence again once they are exhausted. The tick
    // -> frame conversion (through the tempo map) happens on the control side; the node is handed the
    // resolved frame offset, so its audio-thread path stays a branch-only positioned read.
    //
    // Width (ADR-0042): `samples` is interleaved with `sourceChannels` (1 or 2); the node EMITS
    // `channels` (its strip's width). A mono source on a stereo strip is widened with equal-power
    // centre compensation (both outputs x cos(pi/4)) so its centred loudness matches the mono strip's
    // centre pan. A stereo source on a stereo strip passes its channels through unchanged.
    DecodedClipNode (NodeId id, std::vector<float> samples, int channels = 1,
                     std::int64_t timelineStartFrames = 0,
                     std::int64_t fadeInFrames = 0, std::int64_t fadeOutFrames = 0,
                     float clipGain = 1.0f, int sourceChannels = 1) noexcept
        : id_ (id),
          samples_ (std::move (samples)),
          channels_ (channels > 0 ? channels : 1),
          sourceChannels_ (sourceChannels > 0 ? sourceChannels : 1),
          timelineStart_ (timelineStartFrames > 0 ? timelineStartFrames : 0),
          fadeIn_ (fadeInFrames > 0 ? fadeInFrames : 0),
          fadeOut_ (fadeOutFrames > 0 ? fadeOutFrames : 0),
          clipGain_ (std::isfinite (clipGain) && clipGain >= 0.0f ? clipGain : 0.0f),
          monoWidenGain_ (sourceChannels_ == 1 && channels_ > 1 ? kEqualPowerCentreGain : 1.0f)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ true };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }

    void prepare (double, int) override { playFrame_ = 0; }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels < 1)
            return;

        const int          channels = std::min (args.audio.numChannels, channels_);
        const std::int64_t total    = static_cast<std::int64_t> (samples_.size() / static_cast<std::size_t> (sourceChannels_));
        const std::int64_t blockStart = args.transport.hasTimelineFrame ? args.transport.timelineFrame : playFrame_;

        if (sourceChannels_ == 1)
        {
            for (int i = 0; i < args.numFrames; ++i)
            {
                const std::int64_t local  = (blockStart + static_cast<std::int64_t> (i)) - timelineStart_;
                float              sample = 0.0f;
                if (local >= 0 && local < total)
                    sample = samples_[static_cast<std::size_t> (local)]
                           * evaluateClipFadeEnvelopeGain (local, total, fadeIn_, fadeOut_)
                           * clipGain_
                           * monoWidenGain_;

                for (int c = 0; c < channels; ++c)
                    args.audio.channels[c][i] = sample;
            }
        }
        else
        {
            const int sourceChannels = sourceChannels_;
            for (int i = 0; i < args.numFrames; ++i)
            {
                const std::int64_t local = (blockStart + static_cast<std::int64_t> (i)) - timelineStart_;
                if (local >= 0 && local < total)
                {
                    const float frameGain = evaluateClipFadeEnvelopeGain (local, total, fadeIn_, fadeOut_) * clipGain_;
                    const std::size_t base = static_cast<std::size_t> (local) * static_cast<std::size_t> (sourceChannels);
                    for (int c = 0; c < channels; ++c)
                        args.audio.channels[c][i] =
                            samples_[base + static_cast<std::size_t> (c < sourceChannels ? c : sourceChannels - 1)] * frameGain;
                }
                else
                {
                    for (int c = 0; c < channels; ++c)
                        args.audio.channels[c][i] = 0.0f;
                }
            }
        }

        if (! args.transport.hasTimelineFrame)
            playFrame_ += static_cast<std::int64_t> (args.numFrames);
    }

    void reset() noexcept override { playFrame_ = 0; }
    void release() override { samples_.clear(); samples_.shrink_to_fit(); }

    [[nodiscard]] std::int64_t timelineStartFrames() const noexcept { return timelineStart_; }

private:
    // Equal-power centre gain (cos(pi/4)): a mono source widened onto a stereo strip carries this on
    // both channels so its centred loudness matches the mono strip's centre PanNode (ADR-0042).
    static constexpr float kEqualPowerCentreGain = 0.70710678118654752440f;

    NodeId             id_ = 0;
    std::vector<float> samples_;          // interleaved by sourceChannels_
    int                channels_ = 1;      // emitted width (the strip's width)
    int                sourceChannels_ = 1;
    std::int64_t       timelineStart_ = 0;
    std::int64_t       fadeIn_        = 0;
    std::int64_t       fadeOut_       = 0;
    float              clipGain_      = 1.0f;
    float              monoWidenGain_ = 1.0f;
    std::int64_t       playFrame_     = 0;
};

} // namespace yesdaw::engine
