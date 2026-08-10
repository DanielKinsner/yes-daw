// YES DAW — SimpleSynthNode: the built-in musical Instrument (ADR-0043).
//
// A deterministic 8-voice wavetable synth behind the frozen Node contract: NoteOn allocates a voice
// (stealing the oldest when full), NoteOff releases it, and every event applies sample-accurately at
// its timeInBlock (block-sliced rendering, same pattern as PanNode's event handling). The single-cycle
// wavetable (sine + gentle 2nd/3rd harmonics) and the 128-entry note-frequency table are built in
// prepare() on the control thread — the audio thread only does phase accumulation, table reads, and
// linear envelope math (the ADR-0008 per-Block-evaluation rule: no std::sin/std::pow per frame).
//
// Envelope: linear attack (5 ms) to the note's velocity level, sustain, linear release (120 ms).
// Deterministic by construction: no randomness, no time queries; the same event stream renders
// bit-identically. NOT block-parallel-safe: voice state spans Blocks (ADR-0027 guard applies).
//
// Pure C++ — no JUCE — so RTSan/TSan cover process().

#pragma once

#include "engine/Node.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace yesdaw::engine {

class SimpleSynthNode final : public Node
{
public:
    static constexpr std::size_t kMaxVoices = 8;
    static constexpr std::size_t kWavetableSize = 2048;
    static constexpr double kAttackSeconds = 0.005;
    static constexpr double kReleaseSeconds = 0.120;
    static constexpr double kOutputGain = 0.30;   // headroom for 8 voices

    explicit SimpleSynthNode (NodeId id, int channels = 1) noexcept
        : id_ (id), channels_ (channels > 0 ? channels : 1)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ false };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&eventInput_, eventInput_ != nullptr ? 1u : 0u);
    }

    void prepare (double sampleRate, int /*maxBlockSize*/) override
    {
        const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        attackPerFrame_ = 1.0f / static_cast<float> (sr * kAttackSeconds);
        releasePerFrame_ = 1.0f / static_cast<float> (sr * kReleaseSeconds);

        // Single-cycle "synthy" wavetable: fundamental plus gentle upper harmonics for definition.
        wavetable_.resize (kWavetableSize);
        constexpr double kTwoPi = 6.283185307179586;
        for (std::size_t i = 0; i < kWavetableSize; ++i)
        {
            const double phase = kTwoPi * static_cast<double> (i) / static_cast<double> (kWavetableSize);
            wavetable_[i] = static_cast<float> (std::sin (phase)
                                                + 0.35 * std::sin (2.0 * phase)
                                                + 0.15 * std::sin (3.0 * phase));
        }

        for (std::size_t key = 0; key < phaseIncrementForKey_.size(); ++key)
        {
            const double hz = 440.0 * std::pow (2.0, (static_cast<double> (key) - 69.0) / 12.0);
            phaseIncrementForKey_[key] =
                static_cast<float> (hz * static_cast<double> (kWavetableSize) / sr);
        }

        reset();
    }

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

        std::uint32_t cursor = 0;
        for (const Event& event : args.events)
        {
            if (event.type != EventType::NoteOn && event.type != EventType::NoteOff)
                continue;

            if (event.timeInBlock >= static_cast<std::uint32_t> (args.numFrames) || event.timeInBlock < cursor)
                continue;

            renderRange (args, static_cast<int> (cursor), static_cast<int> (event.timeInBlock), channels);
            cursor = event.timeInBlock;

            if (event.type == EventType::NoteOn)
                noteOn (event);
            else
                noteOff (event);
        }

        renderRange (args, static_cast<int> (cursor), args.numFrames, channels);
    }

    void reset() noexcept override
    {
        for (Voice& voice : voices_)
            voice = Voice {};
        voiceAge_ = 0;
    }

    void release() override
    {
        wavetable_.clear();
        wavetable_.shrink_to_fit();
    }

    // Control thread: builder wires the flattened MIDI source.
    void setEventInput (Node* in) noexcept { eventInput_ = in; }

private:
    enum class VoiceStage : std::uint8_t { Idle, Attack, Sustain, Release };

    struct Voice
    {
        VoiceStage stage = VoiceStage::Idle;
        std::int16_t key = -1;
        float phase = 0.0f;
        float phaseIncrement = 0.0f;
        float envelope = 0.0f;
        float targetLevel = 0.0f;
        std::uint64_t age = 0;
    };

    void noteOn (const Event& event) noexcept YESDAW_RT_HOT
    {
        const double velocity = event.payload.note.normalizedVelocity;
        if (! std::isfinite (velocity) || velocity <= 0.0)
            return;

        const int key = event.voice.key;
        if (key < 0 || key >= static_cast<int> (phaseIncrementForKey_.size()))
            return;

        Voice* target = nullptr;
        for (Voice& voice : voices_)
            if (voice.stage == VoiceStage::Idle)
            {
                target = &voice;
                break;
            }

        if (target == nullptr)
        {
            target = &voices_[0];
            for (Voice& voice : voices_)
                if (voice.age < target->age)
                    target = &voice;   // steal the oldest voice
        }

        target->stage = VoiceStage::Attack;
        target->key = static_cast<std::int16_t> (key);
        target->phase = 0.0f;
        target->phaseIncrement = phaseIncrementForKey_[static_cast<std::size_t> (key)];
        target->envelope = 0.0f;
        target->targetLevel = static_cast<float> (velocity > 1.0 ? 1.0 : velocity);
        target->age = ++voiceAge_;
    }

    void noteOff (const Event& event) noexcept YESDAW_RT_HOT
    {
        for (Voice& voice : voices_)
            if (voice.stage != VoiceStage::Idle
                && voice.stage != VoiceStage::Release
                && voice.key == static_cast<std::int16_t> (event.voice.key))
                voice.stage = VoiceStage::Release;
    }

    void renderRange (const ProcessArgs& args, int beginFrame, int endFrame, int channels) noexcept YESDAW_RT_HOT
    {
        if (beginFrame >= endFrame || wavetable_.empty())
            return;

        const float* const table = wavetable_.data();
        const float tableSize = static_cast<float> (kWavetableSize);
        for (Voice& voice : voices_)
        {
            if (voice.stage == VoiceStage::Idle)
                continue;

            for (int i = beginFrame; i < endFrame; ++i)
            {
                if (voice.stage == VoiceStage::Attack)
                {
                    voice.envelope += attackPerFrame_ * voice.targetLevel;
                    if (voice.envelope >= voice.targetLevel)
                    {
                        voice.envelope = voice.targetLevel;
                        voice.stage = VoiceStage::Sustain;
                    }
                }
                else if (voice.stage == VoiceStage::Release)
                {
                    voice.envelope -= releasePerFrame_ * voice.targetLevel;
                    if (voice.envelope <= 0.0f)
                    {
                        voice.envelope = 0.0f;
                        voice.stage = VoiceStage::Idle;
                        break;
                    }
                }

                std::size_t index = static_cast<std::size_t> (voice.phase);
                if (index >= kWavetableSize)
                    index = kWavetableSize - 1;
                const float sample = table[index]
                                   * voice.envelope
                                   * static_cast<float> (kOutputGain);

                for (int c = 0; c < channels; ++c)
                    args.audio.channels[c][i] += sample;

                voice.phase += voice.phaseIncrement;
                while (voice.phase >= tableSize)
                    voice.phase -= tableSize;
            }
        }
    }

    NodeId id_;
    int channels_ = 1;
    Node* eventInput_ = nullptr;
    std::array<Voice, kMaxVoices> voices_ {};
    std::array<float, 128> phaseIncrementForKey_ {};
    std::vector<float> wavetable_;
    float attackPerFrame_ = 0.0f;
    float releasePerFrame_ = 0.0f;
    std::uint64_t voiceAge_ = 0;
};

} // namespace yesdaw::engine
