// YES DAW — FaderNode: a track/bus output gain behind the Node contract (ADR-0008).
//
// A linear-gain multiply with a per-frame ramp so a gain change never zippers and never depends on the
// Block size. Control-thread SetGain uses linear gain; parameter events use the stable H15 ParamSpec
// normalized -> dB -> linear mapping. The control-thread target is set via an atomic and read relaxed at
// the top of each Block, which is the SetGain seam (ADR-0006) the Runtime will route in a later chunk.
//
// Pure C++ — no JUCE — so RTSan/TSan cover process(). The only multiply loop runs frame-outer / channel-
// inner so every channel sees the SAME ramped gain at each frame (a channel-outer loop would advance the
// ramp once per channel and desynchronise stereo). In-place eligible: the compiler may hand a FaderNode
// the same buffer for input and output (it reads x[i], scales, writes x[i]).

#pragma once

#include "engine/Node.h"
#include "engine/ParamSpec.h"
#include "dsp/LinearRamp.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <span>

namespace yesdaw::engine {

class FaderNode final : public Node
{
public:
    static constexpr ParameterId kGainParameterId = 1;
    static constexpr double kMinGainDb = -60.0;
    static constexpr double kMaxGainDb = 6.0;
    static constexpr double kDefaultGainDb = 0.0;

    // Generous-but-bounded linear-gain ceiling (1000x == +60 dB). No musical fader, trim, or clip gain
    // ever approaches this, yet it is small enough that the per-frame multiply can never overflow a
    // float from any finite signal — so a pathological SetGain cannot inject inf/NaN into the mix. The
    // mixer projection's build-time validator (mixerGainIsValid) shares this exact ceiling.
    static constexpr float kMaxLinearGain = 1.0e3f;

    static_assert (std::atomic<std::uint64_t>::is_always_lock_free,
                   "FaderNode command revision must stay lock-free on the audio thread");

    explicit FaderNode (NodeId id = 0, int channels = 1) noexcept
        : id_ (id), channels_ (channels > 0 ? channels : 1) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ true };
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
        seenRequestedGainVersion_ = requestedGainVersion_.load (std::memory_order_acquire);
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        syncControlTarget();

        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        const std::uint32_t frames = args.numFrames > 0 ? static_cast<std::uint32_t> (args.numFrames) : 0u;
        std::uint32_t cursor = 0;
        const std::span<const Event> events = args.events.events();
        const std::span<const Event> automationEvents =
            args.automationEvents != nullptr ? args.automationEvents->events() : std::span<const Event> {};
        std::size_t eventIndex = 0;
        std::size_t automationIndex = 0;

        while (eventIndex < events.size() || automationIndex < automationEvents.size())
        {
            const Event* event = nullptr;
            if (automationIndex >= automationEvents.size())
            {
                event = &events[eventIndex++];
            }
            else if (eventIndex >= events.size())
            {
                event = &automationEvents[automationIndex++];
            }
            else if (events[eventIndex].timeInBlock <= automationEvents[automationIndex].timeInBlock)
            {
                event = &events[eventIndex++];
            }
            else
            {
                event = &automationEvents[automationIndex++];
            }

            if (! isGainParameterEvent (*event) || event->timeInBlock >= frames || event->timeInBlock < cursor)
                continue;

            processRange (args, channels, static_cast<int> (cursor), static_cast<int> (event->timeInBlock));
            gain_.setTarget (linearGainForNormalizedEvent (event->payload.parameter.normalizedValue));
            cursor = event->timeInBlock;
        }

        processRange (args, channels, static_cast<int> (cursor), args.numFrames);
    }

    void reset() noexcept override
    {
        const float requested = requestedGain_.load (std::memory_order_relaxed);
        gain_.snap (requested);
        seenRequestedGainVersion_ = requestedGainVersion_.load (std::memory_order_acquire);
    }
    void release() override {}

    // Control-thread setters (the SetGain seam, ADR-0006). Builder wires the input.
    void setTargetGain (float linearGain) noexcept
    {
        requestedGain_.store (clampGain (linearGain), std::memory_order_relaxed);
        requestedGainVersion_.fetch_add (1, std::memory_order_release);
    }
    void setInput (Node* in) noexcept { input_ = in; }

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        if (parameterId != kGainParameterId)
            return {};

        return ParamSpec { kGainParameterId, "fader.gain", "dB", kMinGainDb, kMaxGainDb, kDefaultGainDb,
                           ParamMapping::Db, ParamSmoothing::Linear5Ms };
    }

    [[nodiscard]] static float linearGainForNormalizedEvent (double normalizedValue) noexcept
    {
        const double gainDb = mapNormalized (parameterSpec (kGainParameterId), normalizedValue);
        if (gainDb <= kMinGainDb)
            return 0.0f;

        return clampGain (static_cast<float> (std::pow (10.0, gainDb / 20.0)));
    }

private:
    static constexpr double kRampSeconds = 0.005;

    // Defensive clamp, mirroring PanNode::setPan: a control- or audio-thread SetGain can never push a
    // non-finite or out-of-range gain onto the ramp, where the per-frame multiply would inject inf/NaN
    // into the mix. Non-finite folds to silence; finite values are bounded to [0, kMaxLinearGain]. Pure
    // arithmetic, so it stays RT-safe on the SetGain seam (applySetGain is RT-hot).
    [[nodiscard]] static float clampGain (float linearGain) noexcept
    {
        return std::isfinite (linearGain) ? std::clamp (linearGain, 0.0f, kMaxLinearGain) : 0.0f;
    }

    void syncControlTarget() noexcept YESDAW_RT_HOT
    {
        const std::uint64_t version = requestedGainVersion_.load (std::memory_order_acquire);
        if (version == seenRequestedGainVersion_)
            return;

        seenRequestedGainVersion_ = version;
        const float requested = requestedGain_.load (std::memory_order_relaxed);
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
    std::atomic<std::uint64_t> requestedGainVersion_ { 0 };
    std::uint64_t              seenRequestedGainVersion_ = 0;
    yesdaw::dsp::LinearRamp gain_;
};

} // namespace yesdaw::engine
