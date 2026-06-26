// YES DAW - PlaceholderNode: silent replacement for an isolated/failed hosted plugin.
//
// H3's host-isolation recovery swaps a failed PluginNode out of the compiled graph for a Placeholder
// through the normal Node/GraphBuilder path. This node keeps the frozen Node contract intact: it can
// occupy the same single-input graph position, but it emits silence and never calls plugin code.

#pragma once

#include "engine/Node.h"

#include <span>

namespace yesdaw::engine {

class PlaceholderNode final : public Node
{
public:
    explicit PlaceholderNode (NodeId id = 0, int channels = 1) noexcept
        : id_ (id), channels_ (channels > 0 ? channels : 1)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        for (int c = 0; c < channels; ++c)
        {
            float* const out = args.audio.channels[c];
            for (int i = 0; i < args.numFrames; ++i)
                out[i] = 0.0f;
        }
    }

    void reset() noexcept override {}
    void release() override {}

    void setInput (Node* in) noexcept { input_ = in; }
    int  channels() const noexcept { return channels_; }

private:
    NodeId id_;
    int    channels_;
    Node*  input_ = nullptr;
};

} // namespace yesdaw::engine
