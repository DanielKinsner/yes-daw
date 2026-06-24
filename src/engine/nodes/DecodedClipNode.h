// YES DAW - decoded Asset Clip source node.
//
// H2 starts with bundled Asset bytes decoded on the control side, then projected into the same graph
// path as every other source. The audio-thread process path only reads pre-owned samples.

#pragma once

#include "engine/Node.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace yesdaw::engine {

class DecodedClipNode final : public Node
{
public:
    DecodedClipNode (NodeId id, std::vector<float> samples, int channels = 1) noexcept
        : id_ (id), samples_ (std::move (samples)), channels_ (channels > 0 ? channels : 1)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }

    void prepare (double, int) override { cursor_ = 0; }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels < 1)
            return;

        const int channels = std::min (args.audio.numChannels, channels_);
        for (int i = 0; i < args.numFrames; ++i)
        {
            const float sample = cursor_ < samples_.size() ? samples_[cursor_] : 0.0f;
            if (cursor_ < samples_.size())
                ++cursor_;

            for (int c = 0; c < channels; ++c)
                args.audio.channels[c][i] = sample;
        }
    }

    void reset() noexcept override { cursor_ = 0; }
    void release() override { samples_.clear(); samples_.shrink_to_fit(); }

private:
    NodeId             id_ = 0;
    std::vector<float> samples_;
    int                channels_ = 1;
    std::size_t        cursor_ = 0;
};

} // namespace yesdaw::engine
