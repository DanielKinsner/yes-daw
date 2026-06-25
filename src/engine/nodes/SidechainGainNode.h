// YES DAW - SidechainGainNode: a minimal sidechain-capable Node behind the frozen contract (ADR-0014).
//
// The simplest built-in that exercises the Sidechain input pin contract end to end: its MAIN input is
// gain-modulated sample-by-sample by its SIDECHAIN input (out[c][i] = main[c][i] * sidechain[0][i]), a
// VCA keyed by the sidechain. Multiplication is deliberately alignment-sensitive: if main and sidechain
// are not PDC-aligned at this node, the product is wrong - which is exactly the property the sidechain
// PDC test leans on.
//
// Pin roles are POSITIONAL ordered inputs (ADR-0008's Node base contract stays frozen; sidechain-ness is
// binding metadata, not a contract change): directInputs() is [main, sidechain], and bindInputs() takes
// the resolved buffers in that same order. GraphBuilder treats BOTH as real graph edges, so topo sort,
// cycle detection, PDC convergence, and buffer liveness all already see the sidechain edge; it also skips
// its producer-id input sort for this kind so main stays input 0 even after a LatencyNode is spliced onto
// the shorter path.
//
// Pure C++ - no JUCE - so RTSan/TSan cover process(). Multi-input (NOT in-place eligible): it reads two
// resolved input buffers and writes its own output slot, like SumNode. The ONLY non-RT work is in
// bindInputs() on the control thread; process() reads bound pointers and writes, with no allocation.

#pragma once

#include "engine/Node.h"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace yesdaw::engine {

class SidechainGainNode final : public Node
{
public:
    static constexpr int kMaxChannels = 8;

    // One bound input: a producer's identity and its per-channel buffers (same shape as SumNode::Input).
    struct Input
    {
        NodeId                                 producerId = 0;
        std::array<const float*, kMaxChannels> channels   { };
    };

    explicit SidechainGainNode (NodeId id = 0, int channels = 1) noexcept
        : id_ (id), channels_ (channels > 0 ? (channels < kMaxChannels ? channels : kMaxChannels) : 1) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (inputNodes_.data(), numInputNodes_);
    }

    void prepare (double, int) override {}   // no internal buffers: out = main * sidechain, read in place

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        const int frames   = args.numFrames;
        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        const float* const sc = sidechainPtr_;   // sidechain channel 0 drives every main channel

        for (int c = 0; c < channels; ++c)
        {
            float* const out = args.audio.channels[c];
            const float* const m = mainPtrs_[static_cast<std::size_t> (c)];

            if (m == nullptr || sc == nullptr)
            {
                for (int i = 0; i < frames; ++i)
                    out[i] = 0.0f;
                continue;
            }

            for (int i = 0; i < frames; ++i)
                out[i] = m[i] * sc[i];
        }
    }

    void reset() noexcept override {}
    void release() override
    {
        mainPtrs_.fill (nullptr);
        sidechainPtr_ = nullptr;
        bound_ = false;
    }

    // Control thread: bind the resolved input buffers in POSITIONAL order - inputs[0] is the main signal,
    // inputs[1] is the sidechain. Unlike SumNode this does NOT sort by producer id (sum order is
    // commutative; main-vs-sidechain is not), and GraphBuilder skips its sort for this kind so the order
    // here is the declared directInputs() order even after PDC splices a LatencyNode onto a shorter path.
    void bindInputs (std::vector<Input> inputs) noexcept
    {
        mainPtrs_.fill (nullptr);
        sidechainPtr_ = nullptr;

        if (! inputs.empty())
            for (int c = 0; c < channels_; ++c)
                mainPtrs_[static_cast<std::size_t> (c)] = inputs[0].channels[static_cast<std::size_t> (c)];

        if (inputs.size() >= 2u)
            sidechainPtr_ = inputs[1].channels[0];

        bound_ = true;
    }

    // Control thread: wire the graph edges. Main is input 0, sidechain is input 1 (the directInputs order).
    void setMainInput (Node* in) noexcept
    {
        inputNodes_[0] = in;
        if (numInputNodes_ < 1u)
            numInputNodes_ = 1u;
    }

    void setSidechainInput (Node* in) noexcept
    {
        inputNodes_[1] = in;
        numInputNodes_ = 2u;
    }

    int  channels() const noexcept { return channels_; }
    bool isBound()  const noexcept { return bound_; }

private:
    NodeId                                 id_;
    int                                    channels_;
    std::size_t                            numInputNodes_ = 0;
    std::array<Node*, 2>                   inputNodes_ { nullptr, nullptr };   // [0]=main, [1]=sidechain
    std::array<const float*, kMaxChannels> mainPtrs_   { };                    // resolved main per channel
    const float*                           sidechainPtr_ = nullptr;           // resolved sidechain channel 0
    bool                                   bound_ = false;
};

} // namespace yesdaw::engine
