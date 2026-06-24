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
    static constexpr ParameterId kGainParameterId = 1;

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
        const float requested = requestedGain_.load (std::memory_order_relaxed);
        gain_.setRampLength (static_cast<int> (kRampSeconds * sr));   // ~5 ms glide
        gain_.snap (requested); // start settled at the requested gain
        lastRequestedGain_ = requested;
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        syncControlTarget();

        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        const std::uint32_t frames = args.numFrames > 0 ? static_cast<std::uint32_t> (args.numFrames) : 0u;
        std::uint32_t cursor = 0;

        for (const Event& event : args.events.events())
        {
            if (! isGainParameterEvent (event) || event.timeInBlock >= frames || event.timeInBlock < cursor)
                continue;

            processRange (args, channels, static_cast<int> (cursor), static_cast<int> (event.timeInBlock));
            gain_.setTarget (static_cast<float> (event.payload.parameter.normalizedValue));
            cursor = event.timeInBlock;
        }

        processRange (args, channels, static_cast<int> (cursor), args.numFrames);
    }

    void reset() noexcept override
    {
        const float requested = requestedGain_.load (std::memory_order_relaxed);
        gain_.snap (requested);
        lastRequestedGain_ = requested;
    }
    void release() override {}

    // Control-thread setters (the SetGain seam, ADR-0006). Builder wires the input.
    void setTargetGain (float linearGain) noexcept { requestedGain_.store (linearGain, std::memory_order_relaxed); }
    void setInput (Node* in) noexcept { input_ = in; }

private:
    static constexpr double kRampSeconds = 0.005;

    void syncControlTarget() noexcept YESDAW_RT_HOT
    {
        const float requested = requestedGain_.load (std::memory_order_relaxed);
        if (requested == lastRequestedGain_)
            return;

        lastRequestedGain_ = requested;
        gain_.setTarget (requested);
    }

    bool isGainParameterEvent (const Event& event) const noexcept YESDAW_RT_HOT
    {
        return event.type == EventType::ParameterChange
               && event.payload.parameter.targetNode == id_
               && event.payload.parameter.parameterId == kGainParameterId;
    }

    void processRange (const ProcessArgs& args, int channels, int beginFrame, int endFrame) noexcept YESDAW_RT_HOT
    {
        if (beginFrame >= endFrame)
            return;

        if (! gain_.isRamping())
        {
            for (int c = 0; c < channels; ++c)
            {
                float* const x = args.audio.channels[c];
                const float  g = gain_.current();
                for (int i = beginFrame; i < endFrame; ++i)
                    x[i] *= g;
            }
            return;
        }

        // Ramping: one gain per frame, applied to every channel — block-size-independent by construction.
        for (int i = beginFrame; i < endFrame; ++i)
        {
            const float g = gain_.next();
            for (int c = 0; c < channels; ++c)
                args.audio.channels[c][i] *= g;
        }
    }

    NodeId            id_;
    int               channels_;
    Node*             input_ = nullptr;
    std::atomic<float> requestedGain_ { 1.0f };   // unity by default
    float              lastRequestedGain_ = 1.0f;
    yesdaw::dsp::LinearRamp gain_;
};

} // namespace yesdaw::engine
