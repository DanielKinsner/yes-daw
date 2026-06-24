// YES DAW — H0 spike #3: a Node behind a stub of the format-neutral trait.
//
// THROWAWAY spike stub (H0), NOT the real Node contract — that's a frozen H1 decision (CLAP-shaped,
// see ADR tracking). This minimal interface exists only to de-risk the scary unknown: does a stateful
// node give the SAME output no matter how the host slices it into blocks? Hosts hand us odd, tiny,
// and huge block sizes (1, 31, 512, 4096, ...), and any place that resets or mishandles state at a
// block boundary would corrupt the audio. The test drives one node at all those sizes and asserts the
// full output is bit-identical every way.

#pragma once

#include "dsp/SineSource.h"

namespace yesdaw::engine {

// The stub trait every processing unit will implement (built-in or hosted plugin), block-sliced.
struct Node
{
    virtual ~Node() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void process (float* block, int numFrames) = 0;   // fills/transforms `block` in place
};

// A generator node behind the trait — a stateful phase accumulator, so block-size independence is a
// real property to prove (a node that re-prepared per block, or lost phase, would fail it).
class ToneNode final : public Node
{
public:
    void prepare (double sampleRate) override         { src_.prepare (sampleRate); }
    void process (float* block, int numFrames) override { src_.processMono (block, numFrames); }

private:
    yesdaw::dsp::SineSource src_;
};

} // namespace yesdaw::engine
