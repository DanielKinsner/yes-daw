// YES DAW — SimpleSynthNode: the built-in musical Instrument (ADR-0043; parameters per ADR-0047 / G3.1).
//
// A deterministic 8-voice wavetable synth behind the frozen Node contract: NoteOn allocates a voice
// (stealing the oldest when full), NoteOff releases it, and every event applies sample-accurately at
// its timeInBlock (block-sliced rendering, same pattern as PanNode's event handling). The single-cycle
// wavetables (a pure sine and the sine + gentle 2nd/3rd harmonics "synthy" table) and the 128-entry
// note-frequency table are built in prepare() on the control thread — the audio thread only does
// phase accumulation, table reads, envelope math and a 2-pole filter per voice (the ADR-0008
// per-Block-evaluation rule: no std::sin/std::pow per frame; the few transcendental calls happen at
// parameter-change time, exactly like FaderNode's dB mapping).
//
// Parameters (ParamSpec, ADR-0038 shape; ids stable for persistence and automation):
//   1 osc mix (0 = pure sine, 1 = the harmonic table)      default 1
//   2 attack s   3 decay s   4 sustain   5 release s        defaults 5 ms / 1 ms / 1.0 / 120 ms
//   6 filter cutoff Hz (20..20000; at the top the filter is BYPASSED — bit-exact)   default 20000
//   7 filter resonance (Q 0.5..10)                          default 0.7
//   8 glide s (portamento from the previous note)           default 0
//   9 volume dB (-60..+12)                                  default 0
// Every default reproduces the ADR-0043 sound bit-for-bit: an unset instrument renders exactly as
// before this node had parameters. Deterministic by construction: no randomness, no time queries;
// the same event stream renders bit-identically. NOT block-parallel-safe: voice state spans Blocks.
//
// Pure C++ — no JUCE — so RTSan/TSan cover process().

#pragma once

#include "engine/Node.h"
#include "engine/ParamSpec.h"

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

    // G3.1: the parameter ids (stable; persisted in the Track's instrument state and named by
    // automation lanes).
    static constexpr ParameterId kOscMixParamId = 1;
    static constexpr ParameterId kAttackParamId = 2;
    static constexpr ParameterId kDecayParamId = 3;
    static constexpr ParameterId kSustainParamId = 4;
    static constexpr ParameterId kReleaseParamId = 5;
    static constexpr ParameterId kCutoffParamId = 6;
    static constexpr ParameterId kResonanceParamId = 7;
    static constexpr ParameterId kGlideParamId = 8;
    static constexpr ParameterId kVolumeParamId = 9;
    static constexpr ParameterId kParameterCount = 9;

    static constexpr double kCutoffMaxHz = 20000.0;   // the bypass point

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kOscMixParamId:    return ParamSpec { kOscMixParamId, "synth.osc_mix", "", 0.0, 1.0, 1.0, ParamMapping::Linear };
            case kAttackParamId:    return ParamSpec { kAttackParamId, "synth.attack", "s", 0.001, 2.0, kAttackSeconds, ParamMapping::Log };
            case kDecayParamId:     return ParamSpec { kDecayParamId, "synth.decay", "s", 0.001, 2.0, 0.001, ParamMapping::Log };
            case kSustainParamId:   return ParamSpec { kSustainParamId, "synth.sustain", "", 0.0, 1.0, 1.0, ParamMapping::Linear };
            case kReleaseParamId:   return ParamSpec { kReleaseParamId, "synth.release", "s", 0.005, 4.0, kReleaseSeconds, ParamMapping::Log };
            case kCutoffParamId:    return ParamSpec { kCutoffParamId, "synth.cutoff", "Hz", 20.0, kCutoffMaxHz, kCutoffMaxHz, ParamMapping::Log };
            case kResonanceParamId: return ParamSpec { kResonanceParamId, "synth.resonance", "Q", 0.5, 10.0, 0.7, ParamMapping::Log };
            case kGlideParamId:     return ParamSpec { kGlideParamId, "synth.glide", "s", 0.0, 1.0, 0.0, ParamMapping::Linear };
            case kVolumeParamId:    return ParamSpec { kVolumeParamId, "synth.volume", "dB", -60.0, 12.0, 0.0, ParamMapping::Db };
            default: break;
        }
        return {};
    }

    [[nodiscard]] static bool acceptsParameterId (ParameterId parameterId) noexcept
    {
        const ParamSpec spec = parameterSpec (parameterId);
        return spec.id == parameterId && spec.name != nullptr && spec.name[0] != '\0' && paramSpecHasUsableRange (spec);
    }

    // Width (ADR-0042, M1): the synth is inherently mono. On a stereo strip it widens with the same
    // equal-power centre compensation DecodedClipNode uses for a mono Asset, so a MIDI Clip on a
    // stereo Track sits at the same centred loudness as one on a mono Track.
    explicit SimpleSynthNode (NodeId id, int channels = 1) noexcept
        : id_ (id),
          channels_ (channels > 0 ? channels : 1),
          widenGain_ (channels > 1 ? kEqualPowerCentreGain : 1.0f)
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
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;

        // Single-cycle wavetables: the pure sine, and the "synthy" fundamental plus gentle upper
        // harmonics for definition (the ADR-0043 sound; the default mix selects it exactly).
        sineTable_.resize (kWavetableSize);
        wavetable_.resize (kWavetableSize);
        constexpr double kTwoPi = 6.283185307179586;
        for (std::size_t i = 0; i < kWavetableSize; ++i)
        {
            const double phase = kTwoPi * static_cast<double> (i) / static_cast<double> (kWavetableSize);
            sineTable_[i] = static_cast<float> (std::sin (phase));
            wavetable_[i] = static_cast<float> (std::sin (phase)
                                                + 0.35 * std::sin (2.0 * phase)
                                                + 0.15 * std::sin (3.0 * phase));
        }

        for (std::size_t key = 0; key < phaseIncrementForKey_.size(); ++key)
        {
            const double hz = 440.0 * std::pow (2.0, (static_cast<double> (key) - 69.0) / 12.0);
            phaseIncrementForKey_[key] =
                static_cast<float> (hz * static_cast<double> (kWavetableSize) / sampleRate_);
        }

        recomputeDerived();
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

        // Two sorted streams — the regular events (notes, control-side parameter posts) and the
        // compiled automation side-band (ADR-0039) — walked together in time order, the same
        // merge the FX nodes do; each event renders up to its offset, then applies.
        const std::span<const Event> regular = args.events.events();
        const std::span<const Event> automation =
            args.automationEvents != nullptr ? args.automationEvents->events() : std::span<const Event> {};

        // G3.2 / ADR-0047: the live lane (audition, later G3.10 input) - block-top notes addressed to THIS
        // Instrument by NotePayload::targetNode; every other node's live notes are not ours.
        if (args.liveEvents != nullptr)
            for (const Event& event : args.liveEvents->events())
            {
                if (event.payload.note.targetNode != id_)
                    continue;
                if (event.type == EventType::NoteOn)
                    noteOn (event);
                else if (event.type == EventType::NoteOff)
                    noteOff (event);
            }

        std::size_t regularIndex = 0;
        std::size_t automationIndex = 0;
        std::uint32_t cursor = 0;
        const std::uint32_t frames = static_cast<std::uint32_t> (args.numFrames);

        while (regularIndex < regular.size() || automationIndex < automation.size())
        {
            const bool useRegular = automationIndex >= automation.size()
                                 || (regularIndex < regular.size()
                                     && regular[regularIndex].timeInBlock <= automation[automationIndex].timeInBlock);
            const Event& event = useRegular ? regular[regularIndex++] : automation[automationIndex++];

            if (event.type == EventType::ParameterChange)
            {
                if (event.payload.parameter.targetNode != id_ || ! acceptsParameterId (event.payload.parameter.parameterId))
                    continue;
            }
            else if (! useRegular || (event.type != EventType::NoteOn && event.type != EventType::NoteOff))
            {
                continue;
            }

            if (event.timeInBlock >= frames || event.timeInBlock < cursor)
                continue;

            renderRange (args, static_cast<int> (cursor), static_cast<int> (event.timeInBlock), channels);
            cursor = event.timeInBlock;

            if (event.type == EventType::ParameterChange)
                applyParameterReal (event.payload.parameter.parameterId,
                                    mapNormalized (parameterSpec (event.payload.parameter.parameterId),
                                                   event.payload.parameter.normalizedValue));
            else if (event.type == EventType::NoteOn)
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
        lastKey_ = -1;
    }

    void release() override
    {
        wavetable_.clear();
        wavetable_.shrink_to_fit();
        sineTable_.clear();
        sineTable_.shrink_to_fit();
    }

    // Control thread: builder wires the flattened MIDI source (ADR-0047: the Track's MidiMerge).
    void setEventInput (Node* in) noexcept { eventInput_ = in; }

    // Control thread, before the node goes live (the projection applies the Track's persisted
    // state): a normalized value maps through the spec exactly as an automation event does.
    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        if (! acceptsParameterId (parameterId))
            return;
        applyParameterReal (parameterId, mapNormalized (parameterSpec (parameterId), normalizedValue));
    }

    [[nodiscard]] double parameterReal (ParameterId parameterId) const noexcept
    {
        switch (parameterId)
        {
            case kOscMixParamId:    return oscMix_;
            case kAttackParamId:    return attackSeconds_;
            case kDecayParamId:     return decaySeconds_;
            case kSustainParamId:   return sustain_;
            case kReleaseParamId:   return releaseSeconds_;
            case kCutoffParamId:    return cutoffHz_;
            case kResonanceParamId: return resonanceQ_;
            case kGlideParamId:     return glideSeconds_;
            case kVolumeParamId:    return volumeDb_;
            default: break;
        }
        return 0.0;
    }

private:
    enum class VoiceStage : std::uint8_t { Idle, Attack, Decay, Sustain, Release };

    struct Voice
    {
        VoiceStage stage = VoiceStage::Idle;
        std::int16_t key = -1;
        float phase = 0.0f;
        float phaseIncrement = 0.0f;
        float phaseIncrementTarget = 0.0f;   // glide: the increment slides toward this
        float phaseIncrementStep = 0.0f;     // per frame (0 = no glide)
        float envelope = 0.0f;
        float targetLevel = 0.0f;
        float sustainLevel = 0.0f;
        float filterIc1 = 0.0f;   // the TPT state-variable filter's two integrator states
        float filterIc2 = 0.0f;
        std::uint64_t age = 0;
    };

    // Parameter application — at event time (audio thread, a handful of transcendental calls per
    // change, never per frame) or on the control thread before the node goes live.
    void applyParameterReal (ParameterId parameterId, double real) noexcept YESDAW_RT_HOT
    {
        switch (parameterId)
        {
            case kOscMixParamId:    oscMix_ = static_cast<float> (real); break;
            case kAttackParamId:    attackSeconds_ = real; break;
            case kDecayParamId:     decaySeconds_ = real; break;
            case kSustainParamId:   sustain_ = static_cast<float> (real); break;
            case kReleaseParamId:   releaseSeconds_ = real; break;
            case kCutoffParamId:    cutoffHz_ = real; break;
            case kResonanceParamId: resonanceQ_ = real; break;
            case kGlideParamId:     glideSeconds_ = real; break;
            case kVolumeParamId:    volumeDb_ = real; break;
            default: return;
        }
        recomputeDerived();
    }

    void recomputeDerived() noexcept YESDAW_RT_HOT
    {
        attackPerFrame_ = 1.0f / static_cast<float> (sampleRate_ * attackSeconds_);
        decayPerFrame_ = 1.0f / static_cast<float> (sampleRate_ * decaySeconds_);
        releasePerFrame_ = 1.0f / static_cast<float> (sampleRate_ * releaseSeconds_);
        glideFrames_ = glideSeconds_ > 0.0 ? static_cast<float> (sampleRate_ * glideSeconds_) : 0.0f;
        volumeGain_ = volumeDb_ == 0.0 ? 1.0f : static_cast<float> (std::pow (10.0, volumeDb_ / 20.0));

        // The filter is bypassed at the top of its range — bit-exact transparency for the default.
        filterEnabled_ = cutoffHz_ < kCutoffMaxHz;
        if (filterEnabled_)
        {
            constexpr double kPi = 3.14159265358979323846;
            const double fc = std::min (cutoffHz_, sampleRate_ * 0.49);
            const double g = std::tan (kPi * fc / sampleRate_);
            const double k = 1.0 / std::max (resonanceQ_, 0.5);
            const double a1 = 1.0 / (1.0 + g * (g + k));
            filterA1_ = static_cast<float> (a1);
            filterA2_ = static_cast<float> (g * a1);
            filterA3_ = static_cast<float> (g * g * a1);
        }
    }

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

        const float increment = phaseIncrementForKey_[static_cast<std::size_t> (key)];
        target->stage = VoiceStage::Attack;
        target->key = static_cast<std::int16_t> (key);
        target->phase = 0.0f;
        target->phaseIncrementTarget = increment;
        if (glideFrames_ > 0.0f && lastKey_ >= 0)
        {
            // Portamento: the pitch slides from the previous note's over the glide time (a linear
            // ramp of the phase increment — deterministic and division-free per frame).
            target->phaseIncrement = phaseIncrementForKey_[static_cast<std::size_t> (lastKey_)];
            target->phaseIncrementStep = (increment - target->phaseIncrement) / glideFrames_;
        }
        else
        {
            target->phaseIncrement = increment;
            target->phaseIncrementStep = 0.0f;
        }
        target->envelope = 0.0f;
        target->targetLevel = static_cast<float> (velocity > 1.0 ? 1.0 : velocity);
        target->sustainLevel = target->targetLevel * sustain_;
        target->filterIc1 = 0.0f;
        target->filterIc2 = 0.0f;
        target->age = ++voiceAge_;
        lastKey_ = static_cast<std::int16_t> (key);
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
        const float* const sine = sineTable_.data();
        const float tableSize = static_cast<float> (kWavetableSize);
        const bool blend = oscMix_ < 1.0f;
        const float mix = oscMix_;
        const bool filtered = filterEnabled_;
        const float a1 = filterA1_, a2 = filterA2_, a3 = filterA3_;
        const float outGain = static_cast<float> (kOutputGain) * widenGain_ * volumeGain_;

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
                        voice.stage = voice.sustainLevel < voice.targetLevel ? VoiceStage::Decay : VoiceStage::Sustain;
                    }
                }
                else if (voice.stage == VoiceStage::Decay)
                {
                    voice.envelope -= decayPerFrame_ * voice.targetLevel;
                    if (voice.envelope <= voice.sustainLevel)
                    {
                        voice.envelope = voice.sustainLevel;
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
                float sample = blend ? (1.0f - mix) * sine[index] + mix * table[index] : table[index];

                if (filtered)
                {
                    // Zavalishin's TPT state-variable low-pass: stable at every cutoff.
                    const float v3 = sample - voice.filterIc2;
                    const float v1 = a1 * voice.filterIc1 + a2 * v3;
                    const float v2 = voice.filterIc2 + a2 * voice.filterIc1 + a3 * v3;
                    voice.filterIc1 = 2.0f * v1 - voice.filterIc1;
                    voice.filterIc2 = 2.0f * v2 - voice.filterIc2;
                    sample = v2;
                }

                sample *= voice.envelope * outGain;

                for (int c = 0; c < channels; ++c)
                    args.audio.channels[c][i] += sample;

                if (voice.phaseIncrementStep != 0.0f)
                {
                    voice.phaseIncrement += voice.phaseIncrementStep;
                    if ((voice.phaseIncrementStep > 0.0f && voice.phaseIncrement >= voice.phaseIncrementTarget)
                        || (voice.phaseIncrementStep < 0.0f && voice.phaseIncrement <= voice.phaseIncrementTarget))
                    {
                        voice.phaseIncrement = voice.phaseIncrementTarget;
                        voice.phaseIncrementStep = 0.0f;
                    }
                }
                voice.phase += voice.phaseIncrement;
                while (voice.phase >= tableSize)
                    voice.phase -= tableSize;
            }
        }
    }

    // Equal-power centre gain (cos(pi/4)) — the ADR-0042 mono-widening law, shared with
    // DecodedClipNode so mono sources match whatever strip width they land on.
    static constexpr float kEqualPowerCentreGain = 0.70710678118654752440f;

    NodeId id_;
    int channels_ = 1;
    float widenGain_ = 1.0f;
    Node* eventInput_ = nullptr;
    std::array<Voice, kMaxVoices> voices_ {};
    std::array<float, 128> phaseIncrementForKey_ {};
    std::vector<float> wavetable_;
    std::vector<float> sineTable_;
    double sampleRate_ = 48000.0;
    std::uint64_t voiceAge_ = 0;
    std::int16_t lastKey_ = -1;

    // The parameters (real units) and what the audio thread derives from them.
    float oscMix_ = 1.0f;
    double attackSeconds_ = kAttackSeconds;
    double decaySeconds_ = 0.001;
    float sustain_ = 1.0f;
    double releaseSeconds_ = kReleaseSeconds;
    double cutoffHz_ = kCutoffMaxHz;
    double resonanceQ_ = 0.7;
    double glideSeconds_ = 0.0;
    double volumeDb_ = 0.0;
    float attackPerFrame_ = 0.0f;
    float decayPerFrame_ = 0.0f;
    float releasePerFrame_ = 0.0f;
    float glideFrames_ = 0.0f;
    float volumeGain_ = 1.0f;
    bool filterEnabled_ = false;
    float filterA1_ = 0.0f, filterA2_ = 0.0f, filterA3_ = 0.0f;
};

} // namespace yesdaw::engine
