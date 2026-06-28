// YES DAW — IdentityDcNode: a tiny source Node for compiler smoke tests and empty graph plumbing.
//
// This is not a musical source. It gives the first real GraphBuilder slice a deterministic signal that
// can prove end-to-end routing without depending on oscillator phase, device I/O, or human listening.

#pragma once

#include "engine/Node.h"

#include <span>

namespace yesdaw::engine {

class IdentityDcNode final : public Node
{
public:
    explicit IdentityDcNode (NodeId id = 0, float dc = 0.0f, int channels = 1) noexcept
        : id_ (id), dc_ (dc), channels_ (channels > 0 ? channels : 1)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ true };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }

    void prepare (double, int) override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        for (int c = 0; c < channels; ++c)
        {
            float* out = args.audio.channels[c];
            for (int i = 0; i < args.numFrames; ++i)
                out[i] = dc_;
        }
    }

    void reset() noexcept override {}
    void release() override {}

    float dc() const noexcept { return dc_; }

private:
    NodeId id_;
    float  dc_;
    int    channels_;
};

} // namespace yesdaw::engine
