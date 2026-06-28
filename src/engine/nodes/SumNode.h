// YES DAW — SumNode: a Bus that sums N inputs in f64 behind the Node contract (ADR-0008 / ADR-0007 #6).
//
// Every Bus — and the Master is a Bus — accumulates its inputs into a double temp buffer and narrows to
// float on output (decision #13). f64 summing keeps a quiet signal from vanishing under a loud one
// (catastrophic cancellation) the way a float accumulator would. The internal sample type is fixed here.
//
// DETERMINISM: float/double addition is not associative, so the sum ORDER must be canonical or the same
// mix would render bit-differently after an unrelated recompile reordered the inputs. SumNode therefore
// sorts its inputs by producer NodeId at bind time (control thread); the audio thread then always adds in
// that fixed order. This is the property the order-shuffle invariance gate leans on.
//
// Pure C++ — no JUCE — so RTSan/TSan cover process(). The inputs are resolved float* (each producer's
// output buffer, a fixed address for the graph's lifetime); the compiler binds them once after it lays
// out the buffer pool. NOT in-place eligible (it reads many buffers, writes one). The ONLY allocation is
// the f64 accumulator + the resolved-pointer table, both in prepare()/bindInputs() on the control thread.

#pragma once

#include "engine/Node.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace yesdaw::engine {

class SumNode final : public Node
{
public:
    static constexpr int kMaxChannels = 8;

    // One bound input: a producer's identity (for the canonical sum order) and its per-channel buffers.
    struct Input
    {
        NodeId                                  producerId = 0;
        std::array<const float*, kMaxChannels>  channels   { };   // [c] valid for c < the bus channel count
    };

    explicit SumNode (NodeId id = 0, int channels = 2) noexcept
        : id_ (id), channels_ (channels > 0 ? (channels < kMaxChannels ? channels : kMaxChannels) : 1) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ true };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (inputNodes_.data(), inputNodes_.size());
    }

    void prepare (double /*sampleRate*/, int maxBlockSize) override
    {
        maxBlock_ = maxBlockSize > 0 ? maxBlockSize : 1;
        acc_      = std::make_unique<double[]> (static_cast<std::size_t> (channels_) * static_cast<std::size_t> (maxBlock_));
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        const int frames   = args.numFrames;
        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        const int nInputs  = numInputs_;

        for (int c = 0; c < channels; ++c)
        {
            double* a = acc_.get() + static_cast<std::size_t> (c) * static_cast<std::size_t> (maxBlock_);
            for (int i = 0; i < frames; ++i)
                a[i] = 0.0;

            for (int in = 0; in < nInputs; ++in)
            {
                const float* src = inputPtrs_[static_cast<std::size_t> (in) * static_cast<std::size_t> (channels_)
                                              + static_cast<std::size_t> (c)];
                if (src == nullptr)
                    continue;
                for (int i = 0; i < frames; ++i)
                    a[i] += static_cast<double> (src[i]);
            }

            float* out = args.audio.channels[c];
            for (int i = 0; i < frames; ++i)
                out[i] = static_cast<float> (a[i]);
        }
    }

    void reset() noexcept override {}
    void release() override { acc_.reset(); inputPtrs_.clear(); inputPtrs_.shrink_to_fit(); }

    // Control thread: bind the resolved input buffers. Sorts by producer NodeId so the audio thread always
    // sums in one canonical order (bit-identity across recompiles). Called once per compile after the pool
    // is laid out; in tests, called directly with scratch buffers.
    void bindInputs (std::vector<Input> inputs) noexcept
    {
        std::sort (inputs.begin(), inputs.end(),
                   [] (const Input& a, const Input& b) { return a.producerId < b.producerId; });

        numInputs_ = static_cast<int> (inputs.size());
        inputPtrs_.assign (static_cast<std::size_t> (numInputs_) * static_cast<std::size_t> (channels_), nullptr);
        for (int in = 0; in < numInputs_; ++in)
            for (int c = 0; c < channels_; ++c)
                inputPtrs_[static_cast<std::size_t> (in) * static_cast<std::size_t> (channels_) + static_cast<std::size_t> (c)]
                    = inputs[static_cast<std::size_t> (in)].channels[static_cast<std::size_t> (c)];
        bound_ = true;
    }

    // Control thread: the input Nodes, for the compiler's topo walk (directInputs()).
    void setInputNodes (std::vector<Node*> nodes) { inputNodes_ = std::move (nodes); }

    int  channels() const noexcept { return channels_; }
    bool isBound()  const noexcept { return bound_; }

private:
    NodeId                     id_;
    int                        channels_;
    int                        maxBlock_  = 0;
    int                        numInputs_ = 0;
    bool                       bound_     = false;
    std::unique_ptr<double[]>  acc_;                 // channels_ * maxBlock_ f64 accumulator
    std::vector<const float*>  inputPtrs_;           // [input*channels_ + c], sorted by producer NodeId
    std::vector<Node*>         inputNodes_;          // for directInputs() (compiler topo); unused on audio thread
};

} // namespace yesdaw::engine
