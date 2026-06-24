// YES DAW — DelayNode: the ONE delay primitive (ADR-0007 #5).
//
// A single sample-delay ring serves BOTH roles the engine needs: PDC inserts it with a *computed*
// delay (the LatencyNode the compiler splices onto a shorter convergence input), and explicit feedback
// edges use it with a *fixed* one-Block delay. One implementation, one test, one RT-critical hot spot to
// harden — instead of two near-identical delay lines drifting apart. `LatencyNode` is a type alias of
// this class; the CompiledNode kind tag (ADR-0007 Pass 3) is what tells PDC-spliced delays apart from
// user feedback delays for diagnostics — same code, same RTSan coverage, same tests.
//
// Pure C++ — no JUCE — so it is covered by the RTSan/TSan legs. process() is the audio hot path:
// allocation/lock/syscall free (RTSan-enforced); the ONLY allocation is the ring, sized in prepare().
//
// Ring sizing + indexing: the ring is rounded UP to a power of two so process() masks (single AND)
// instead of doing a modulo per sample. It is sized to hold `delaySamples + maxBlockSize` in flight, so
// across any one Block the write window [w, w+N) and the read window [w-d, w+N-d) never wrap onto each
// other. We WRITE the incoming sample THEN READ the delayed one: at delay 0 the read sees the value just
// written (a true bit-exact pass-through); at delay d>0 the write index never equals the read index, so
// the order is immaterial there. (Reading first would make delay 0 emit the stale ring contents — a 0 —
// instead of the input: a real bug.)

#pragma once

#include "engine/Node.h"   // the frozen Node contract (ADR-0008)

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace yesdaw::engine {

class DelayNode final : public Node
{
public:
    // delaySamples is immutable after construction: PDC computes the value once per compile; feedback
    // users pick a fixed value. channels defaults to mono; clamped to >= 1 / >= 0 so a bad caller can
    // never size a negative ring.
    DelayNode (NodeId id, std::int64_t delaySamples, int channels = 1) noexcept
        : id_ (id),
          delaySamples_ (delaySamples >= 0 ? delaySamples : 0),
          channels_ (channels > 0 ? channels : 1)
    {
    }

    NodeProperties properties() const noexcept override
    {
        // A DelayNode is the one built-in that reports non-zero latency — and PDC uses exactly this
        // value to splice OTHER delays onto the shorter sibling inputs of a convergence node.
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, delaySamples_, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double /*sampleRate*/, int maxBlockSize) override
    {
        const int maxBlock = maxBlockSize > 0 ? maxBlockSize : 1;

        // +1 keeps write and read windows disjoint even when delay == maxBlock exactly.
        std::int64_t needed = delaySamples_ + static_cast<std::int64_t> (maxBlock) + 1;

        std::uint32_t pow2 = 1;
        while (static_cast<std::int64_t> (pow2) < needed)
            pow2 <<= 1;

        framesPerChannel_ = pow2;
        mask_             = pow2 - 1u;
        writePos_         = 0;
        ring_.assign (static_cast<std::size_t> (framesPerChannel_) * static_cast<std::size_t> (channels_), 0.0f);
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        const std::uint32_t m   = mask_;
        const std::uint32_t fpc = framesPerChannel_;
        const std::uint32_t d   = static_cast<std::uint32_t> (delaySamples_);   // delaySamples_ < fpc by construction

        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        for (int c = 0; c < channels; ++c)
        {
            float*              x    = args.audio.channels[c];
            float* const        ring = ring_.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (fpc);
            const std::uint32_t w    = writePos_;

            for (int i = 0; i < args.numFrames; ++i)
            {
                const std::uint32_t ui = static_cast<std::uint32_t> (i);
                ring[(w + ui) & m]     = x[i];               // write the incoming sample first...
                x[i]                   = ring[(w + ui - d) & m];  // ...then read the one d frames behind it
            }
        }

        writePos_ = (writePos_ + static_cast<std::uint32_t> (args.numFrames)) & mask_;
    }

    void reset() noexcept override
    {
        if (! ring_.empty())
            std::memset (ring_.data(), 0, ring_.size() * sizeof (float));
        writePos_ = 0;
    }

    void release() override
    {
        ring_.clear();
        ring_.shrink_to_fit();
        framesPerChannel_ = 0;
        mask_             = 0;
        writePos_         = 0;
    }

    // Builder-only wiring (control thread). Not part of the Node trait.
    void         setInput (Node* in) noexcept { input_ = in; }
    std::int64_t delaySamples() const noexcept { return delaySamples_; }

private:
    NodeId             id_;
    std::int64_t       delaySamples_;
    int                channels_;
    Node*              input_            = nullptr;   // set by the graph builder; a bare delay has none
    std::vector<float> ring_;                         // per-channel contiguous: [channel*framesPerChannel_ + i]
    std::uint32_t      framesPerChannel_ = 0;
    std::uint32_t      mask_             = 0;
    std::uint32_t      writePos_         = 0;
};

// PDC-spliced LatencyNode IS this primitive (ADR-0007 #5). The CompiledNode kind tag distinguishes a
// compiler-synthesized latency delay from a user feedback delay; the code path is identical.
using LatencyNode = DelayNode;

} // namespace yesdaw::engine
