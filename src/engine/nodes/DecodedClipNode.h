// YES DAW - decoded Asset Clip source node.
//
// H2 starts with bundled Asset bytes decoded on the control side, then projected into the same graph
// path as every other source. The audio-thread process path only reads pre-owned samples.

#pragma once

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
    DecodedClipNode (NodeId id, std::vector<float> samples, int channels = 1,
                     std::int64_t timelineStartFrames = 0,
                     std::int64_t fadeInFrames = 0, std::int64_t fadeOutFrames = 0,
                     float clipGain = 1.0f) noexcept
        : id_ (id),
          samples_ (std::move (samples)),
          channels_ (channels > 0 ? channels : 1),
          timelineStart_ (timelineStartFrames > 0 ? timelineStartFrames : 0),
          fadeIn_ (fadeInFrames > 0 ? fadeInFrames : 0),
          fadeOut_ (fadeOutFrames > 0 ? fadeOutFrames : 0),
          clipGain_ (std::isfinite (clipGain) && clipGain >= 0.0f ? clipGain : 0.0f)
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
        const std::int64_t total    = static_cast<std::int64_t> (samples_.size());
        const std::int64_t blockStart = args.transport.hasTimelineFrame ? args.transport.timelineFrame : playFrame_;
        for (int i = 0; i < args.numFrames; ++i)
        {
            const std::int64_t local  = (blockStart + static_cast<std::int64_t> (i)) - timelineStart_;
            float              sample = 0.0f;
            if (local >= 0 && local < total)
                sample = samples_[static_cast<std::size_t> (local)] * fadeGainAt (local, total) * clipGain_;

            for (int c = 0; c < channels; ++c)
                args.audio.channels[c][i] = sample;
        }

        if (! args.transport.hasTimelineFrame)
            playFrame_ += static_cast<std::int64_t> (args.numFrames);
    }

    void reset() noexcept override { playFrame_ = 0; }
    void release() override { samples_.clear(); samples_.shrink_to_fit(); }

    [[nodiscard]] std::int64_t timelineStartFrames() const noexcept { return timelineStart_; }

    // Linear fade envelope (non-destructive — applied at render, the underlying samples are untouched).
    // fade-in ramps 0 -> 1 over the first fadeIn_ frames; fade-out ramps 1 -> 0 over the last fadeOut_.
    // Overlapping two clips with a matching fade-out / fade-in renders a real crossfade. (Equal-power is a
    // later refinement; linear is the simple, correct baseline.)
    [[nodiscard]] float fadeGainAt (std::int64_t local, std::int64_t total) const noexcept
    {
        float gain = 1.0f;
        if (fadeIn_ > 0 && local < fadeIn_)
            gain *= static_cast<float> (local) / static_cast<float> (fadeIn_);
        if (fadeOut_ > 0)
        {
            const std::int64_t fadeOutStart = total - fadeOut_;
            if (local >= fadeOutStart)
                gain *= static_cast<float> (total - local) / static_cast<float> (fadeOut_);
        }
        return gain;
    }

private:
    NodeId             id_ = 0;
    std::vector<float> samples_;
    int                channels_ = 1;
    std::int64_t       timelineStart_ = 0;
    std::int64_t       fadeIn_        = 0;
    std::int64_t       fadeOut_       = 0;
    float              clipGain_      = 1.0f;
    std::int64_t       playFrame_     = 0;
};

} // namespace yesdaw::engine
