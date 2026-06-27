// YES DAW - minimal built-in Instrument Node for the H4 MIDI timing gate.
//
// It consumes NoteOn Events and emits unit impulses after its advertised latency. This is intentionally
// a deterministic timing instrument, not a musical synthesizer.

#pragma once

#include "engine/Node.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace yesdaw::engine {

class ImpulseInstrumentNode final : public Node
{
public:
    static constexpr std::size_t kMaxPendingImpulses = 64;

    explicit ImpulseInstrumentNode (NodeId id, std::int64_t latencySamples = 0, int channels = 1) noexcept
        : id_ (id),
          latencySamples_ (latencySamples >= 0 ? latencySamples : 0),
          channels_ (channels > 0 ? channels : 1)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, channels_, latencySamples_, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&eventInput_, eventInput_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override { pendingCount_ = 0; }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels <= 0 || args.numFrames <= 0)
            return;

        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        for (int c = 0; c < channels; ++c)
        {
            float* const out = args.audio.channels[c];
            for (int i = 0; i < args.numFrames; ++i)
                out[i] = 0.0f;
        }

        emitPending (args);

        for (const Event& event : args.events)
        {
            if (event.type != EventType::NoteOn)
                continue;

            const double v = event.payload.note.normalizedVelocity;
            if (! std::isfinite (v) || v <= 0.0)
                continue;

            lastNoteOnKey_ = event.voice.key;

            const std::uint32_t due = event.timeInBlock + static_cast<std::uint32_t> (latencySamples_);
            if (due < static_cast<std::uint32_t> (args.numFrames))
            {
                emitAt (args, due, static_cast<float> (v));
            }
            else if (pendingCount_ < pending_.size())
            {
                pending_[pendingCount_++] = PendingImpulse {
                    due - static_cast<std::uint32_t> (args.numFrames),
                    static_cast<float> (v)
                };
            }
        }
    }

    void reset() noexcept override { pendingCount_ = 0; }
    void release() override { pendingCount_ = 0; }

    void setEventInput (Node* input) noexcept { eventInput_ = input; }
    [[nodiscard]] std::int16_t lastNoteOnKey() const noexcept { return lastNoteOnKey_; }

private:
    struct PendingImpulse
    {
        std::uint32_t framesUntilDue = 0;
        float         amplitude = 0.0f;
    };

    void emitPending (const ProcessArgs& args) noexcept YESDAW_RT_HOT
    {
        std::size_t i = 0;
        while (i < pendingCount_)
        {
            PendingImpulse& pending = pending_[i];
            if (pending.framesUntilDue < static_cast<std::uint32_t> (args.numFrames))
            {
                emitAt (args, pending.framesUntilDue, pending.amplitude);
                pending_[i] = pending_[pendingCount_ - 1u];
                --pendingCount_;
            }
            else
            {
                pending.framesUntilDue -= static_cast<std::uint32_t> (args.numFrames);
                ++i;
            }
        }
    }

    void emitAt (const ProcessArgs& args, std::uint32_t frame, float amplitude) noexcept YESDAW_RT_HOT
    {
        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        for (int c = 0; c < channels; ++c)
            args.audio.channels[c][frame] += amplitude;
    }

    NodeId       id_;
    std::int64_t latencySamples_;
    int          channels_;
    std::array<PendingImpulse, kMaxPendingImpulses> pending_ {};
    std::size_t  pendingCount_ = 0;
    Node*        eventInput_ = nullptr;
    std::int16_t lastNoteOnKey_ = -1;
};

} // namespace yesdaw::engine
