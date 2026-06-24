// YES DAW — MasterNode: the graph's top-level Bus behind the frozen Node contract.
//
// Master is intentionally boring at H1: it is the top-level f64-summed Bus the compiler starts from,
// and CompiledGraph copies channel 0 of its output to the legacy mono process(out, frames) seam.

#pragma once

#include "engine/Node.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace yesdaw::engine {

class MasterNode final : public Node
{
public:
    static constexpr int kMaxChannels = 8;

    struct Input
    {
        NodeId                                 producerId = 0;
        std::array<const float*, kMaxChannels> channels   { };
    };

    explicit MasterNode (NodeId id = 0, int channels = 1) noexcept
        : id_ (id), channels_ (channels > 0 ? (channels < kMaxChannels ? channels : kMaxChannels) : 1)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (inputNodes_.data(), inputNodes_.size());
    }

    void prepare (double, int maxBlockSize) override
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

    void setInputNodes (std::vector<Node*> nodes) { inputNodes_ = std::move (nodes); }

    int  channels() const noexcept { return channels_; }
    bool isBound()  const noexcept { return bound_; }

private:
    NodeId                    id_;
    int                       channels_;
    int                       maxBlock_  = 0;
    int                       numInputs_ = 0;
    bool                      bound_     = false;
    std::unique_ptr<double[]> acc_;
    std::vector<const float*> inputPtrs_;
    std::vector<Node*>        inputNodes_;
};

} // namespace yesdaw::engine
