// YES DAW — FaderNode: a track/bus output gain behind the Node contract (ADR-0008).
//
// A linear-gain multiply with a per-frame ramp so a gain change never zippers and never depends on the
// Block size. The target gain is a linear value (dB->linear conversion lives in the command layer, never
// on the audio thread); it is set from the control thread via an atomic and read relaxed at the top of
// each Block, which is the SetGain seam (ADR-0006) the Runtime will route in a later chunk.
//
// Pure C++ — no JUCE — so RTSan/TSan cover process(). The only multiply loop runs frame-outer / channel-
// inner so every channel sees the SAME ramped gain at each frame (a channel-outer loop would advance the
// ramp once per channel and desynchronise stereo). In-place eligible: the compiler may hand a FaderNode
// the same buffer for input and output (it reads x[i], scales, writes x[i]).

#pragma once

#include "engine/Node.h"
#include "dsp/LinearRamp.h"

#include <atomic>
#include <span>

namespace yesdaw::engine {

class FaderNode final : public Node
{
public:
    explicit FaderNode (NodeId id = 0, int channels = 1) noexcept
        : id_ (id), channels_ (channels > 0 ? channels : 1) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double sampleRate, int /*maxBlockSize*/) override
    {
        const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        gain_.setRampLength (static_cast<int> (kRampSeconds * sr));   // ~5 ms glide
        gain_.snap (requestedGain_.load (std::memory_order_relaxed)); // start settled at the requested gain
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        gain_.setTarget (requestedGain_.load (std::memory_order_relaxed));   // no-op if unchanged

        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;

        if (! gain_.isRamping())
        {
            const float g = gain_.current();
            for (int c = 0; c < channels; ++c)
            {
                float* x = args.audio.channels[c];
                for (int i = 0; i < args.numFrames; ++i)
                    x[i] *= g;
            }
            return;
        }

        // Ramping: one gain per frame, applied to every channel — block-size-independent by construction.
        for (int i = 0; i < args.numFrames; ++i)
        {
            const float g = gain_.next();
            for (int c = 0; c < channels; ++c)
                args.audio.channels[c][i] *= g;
        }
    }

    void reset() noexcept override { gain_.snap (requestedGain_.load (std::memory_order_relaxed)); }
    void release() override {}

    // Control-thread setters (the SetGain seam, ADR-0006). Builder wires the input.
    void setTargetGain (float linearGain) noexcept { requestedGain_.store (linearGain, std::memory_order_relaxed); }
    void setInput (Node* in) noexcept { input_ = in; }

private:
    static constexpr double kRampSeconds = 0.005;

    NodeId            id_;
    int               channels_;
    Node*             input_ = nullptr;
    std::atomic<float> requestedGain_ { 1.0f };   // unity by default
    yesdaw::dsp::LinearRamp gain_;
};

} // namespace yesdaw::engine
