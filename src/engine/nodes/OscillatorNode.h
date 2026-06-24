// YES DAW — OscillatorNode: a built-in sine generator behind the Node contract (ADR-0008).
//
// Wraps the (already golden-tested) SineSource DSP. It is a STATEFUL generator (a phase accumulator), so
// it exercises the contract's hardest property: block-size independence — the same audio regardless of
// how the host slices process() into Blocks. A Node that re-prepared per Block or lost phase would fail.

#pragma once

#include "engine/Node.h"
#include "dsp/SineSource.h"

#include <span>

namespace yesdaw::engine {

class OscillatorNode final : public Node
{
public:
    explicit OscillatorNode (NodeId id = 0) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                /*channels*/ 1, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }   // a source has no inputs

    void prepare (double sampleRate, int /*maxBlockSize*/) override
    {
        sampleRate_ = sampleRate;
        src_.prepare (sampleRate);
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels < 1)
            return;
        src_.processMono (args.audio.channels[0], args.numFrames);
    }

    void reset() noexcept override { src_.prepare (sampleRate_); }   // re-arm phase + fade; no allocation

    void release() override {}                                      // nothing heap-allocated

private:
    yesdaw::dsp::SineSource src_;
    double sampleRate_ = 48000.0;
    NodeId id_         = 0;
};

} // namespace yesdaw::engine
