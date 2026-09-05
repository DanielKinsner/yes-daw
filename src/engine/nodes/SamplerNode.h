// YES DAW — SamplerNode (G3.9 / ADR-0048): the Sampler instrument behind the frozen Node contract.
//
// Pads: a trigger key, one shared AssetSamples owner (never copied on the audio thread — the owner
// is handed in on the control side by the projection), a root key, one-shot or pitched, a gain.
// Playback law (ADR-0048): a NoteOn on a pad's own key plays it at its root; a NoteOn on a key no
// pad claims plays the nearest LOWER pitched pad transposed by the interval (a one-shot never
// answers to other keys); a one-shot ignores NoteOff and plays to the sample's end; a pitched pad
// releases on NoteOff through the ADSR. Sixteen voices, the oldest stolen. Pitch is a linear-
// interpolated read at 2^((key - root) / 12). Deterministic: no state but the voices.
//
// Parameters (ParamSpec, normalized in the Track's instrument state like the synth's):
//   1 attack s   2 decay s   3 sustain   4 release s   5 gain        defaults 1 ms / 1 ms / 1.0 / 20 ms / 1.0

#pragma once

#include "engine/Node.h"
#include "engine/ParamSpec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace yesdaw::engine {

struct SamplerNodePad
{
    std::int16_t key = -1;          // the trigger key; -1 = an empty slot
    std::int16_t rootKey = 60;      // the key at which the sample plays at unity rate
    bool         oneShot = true;
    float        gain = 1.0f;
    // The sample: an interleaved view (channels 1..2, `frames` frames) over storage `owner` keeps
    // alive — the projection's asset-samples seam hands both in (ADR-0048); the node never copies.
    std::shared_ptr<const void> owner;
    std::span<const float> interleaved;
    int           channels = 1;
    std::uint64_t frames = 0;

    [[nodiscard]] bool isLoaded() const noexcept
    {
        return key >= 0 && frames > 0 && channels >= 1 && channels <= 2
            && interleaved.size() >= static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels);
    }
};

class SamplerNode final : public Node
{
public:
    static constexpr std::size_t kMaxPads = 128;
    static constexpr std::size_t kMaxVoices = 16;
    static constexpr ParameterId kAttackParamId = 1;
    static constexpr ParameterId kDecayParamId = 2;
    static constexpr ParameterId kSustainParamId = 3;
    static constexpr ParameterId kReleaseParamId = 4;
    static constexpr ParameterId kGainParamId = 5;
    static constexpr double kAttackSeconds = 0.001;
    static constexpr double kDecaySeconds = 0.001;
    static constexpr double kSustain = 1.0;
    static constexpr double kReleaseSeconds = 0.02;
    static constexpr double kGain = 1.0;

    explicit SamplerNode (NodeId id, int channels = 2) noexcept
        : id_ (id), channels_ (std::clamp (channels, 1, 2))
    {
        recomputeDerived();
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                static_cast<std::uint16_t> (channels_), /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&eventInput_, eventInput_ != nullptr ? 1u : 0u);
    }

    void setEventInput (Node* input) noexcept { eventInput_ = input; }

    // Control side: the pads (sorted by key; an empty slot has key -1). The owners come from the
    // projection's asset-samples seam (ADR-0048), never from a copy this node makes.
    void setPads (std::vector<SamplerNodePad> pads)
    {
        pads_ = std::move (pads);
        std::sort (pads_.begin(), pads_.end(), [] (const SamplerNodePad& a, const SamplerNodePad& b) { return a.key < b.key; });
    }
    [[nodiscard]] const std::vector<SamplerNodePad>& pads() const noexcept { return pads_; }

    void prepare (double sampleRate, int) override
    {
        sampleRate_ = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        for (Voice& voice : voices_)
            voice = Voice {};
        voiceAge_ = 0;
        recomputeDerived();
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        const int channels = std::min (channels_, args.audio.numChannels);
        for (int c = 0; c < args.audio.numChannels; ++c)
        {
            float* const out = args.audio.channels[c];
            if (out == nullptr)
                continue;
            for (int i = 0; i < args.numFrames; ++i)
                out[i] = 0.0f;
        }
        if (channels <= 0)
            return;

        // The live lane (audition, later G3.10 input): block-top notes addressed to THIS instrument.
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

        const std::span<const Event> regular = args.events.events();
        const std::span<const Event> automation =
            args.automationEvents != nullptr ? args.automationEvents->events() : std::span<const Event> {};
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
                continue;
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
    }

    void release() override
    {
        pads_.clear();
        pads_.shrink_to_fit();
    }

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kAttackParamId:  return ParamSpec { kAttackParamId, "sampler.attack", "s", 0.001, 2.0, kAttackSeconds, ParamMapping::Log };
            case kDecayParamId:   return ParamSpec { kDecayParamId, "sampler.decay", "s", 0.001, 2.0, kDecaySeconds, ParamMapping::Log };
            case kSustainParamId: return ParamSpec { kSustainParamId, "sampler.sustain", "", 0.0, 1.0, kSustain };
            case kReleaseParamId: return ParamSpec { kReleaseParamId, "sampler.release", "s", 0.005, 4.0, kReleaseSeconds, ParamMapping::Log };
            case kGainParamId:    return ParamSpec { kGainParamId, "sampler.gain", "x", 0.0, 2.0, kGain };
            default: return {};
        }
    }

    [[nodiscard]] static bool acceptsParameterId (ParameterId parameterId) noexcept
    {
        return parameterId >= kAttackParamId && parameterId <= kGainParamId;
    }

    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        if (acceptsParameterId (parameterId))
            applyParameterReal (parameterId, mapNormalized (parameterSpec (parameterId), normalizedValue));
    }

    [[nodiscard]] std::size_t activeVoiceCount() const noexcept
    {
        std::size_t count = 0;
        for (const Voice& voice : voices_)
            if (voice.stage != Stage::Idle)
                ++count;
        return count;
    }

    // The pad a key plays and the rate it plays at (ADR-0048's mapping law); -1 = silence.
    [[nodiscard]] int padIndexForKey (std::int16_t key, double& rateOut) const noexcept
    {
        rateOut = 1.0;
        int best = -1;
        for (std::size_t i = 0; i < pads_.size(); ++i)
        {
            const SamplerNodePad& pad = pads_[i];
            if (! pad.isLoaded())
                continue;
            if (pad.key == key)
                return static_cast<int> (i);
            if (! pad.oneShot && pad.key < key && (best < 0 || pad.key > pads_[static_cast<std::size_t> (best)].key))
                best = static_cast<int> (i);
        }
        if (best >= 0)
            rateOut = std::pow (2.0, static_cast<double> (key - pads_[static_cast<std::size_t> (best)].rootKey) / 12.0);
        return best;
    }

private:
    enum class Stage : std::uint8_t { Idle, Attack, Decay, Sustain, Release };

    struct Voice
    {
        Stage stage = Stage::Idle;
        std::int16_t key = -1;
        int pad = -1;
        bool oneShot = true;
        double position = 0.0;
        double rate = 1.0;
        float envelope = 0.0f;
        float targetLevel = 0.0f;
        float sustainLevel = 0.0f;
        float gain = 1.0f;
        std::uint64_t age = 0;
    };

    void noteOn (const Event& event) noexcept YESDAW_RT_HOT
    {
        const double velocity = event.payload.note.normalizedVelocity;
        if (! std::isfinite (velocity) || velocity <= 0.0 || event.voice.key < 0)
            return;
        double rate = 1.0;
        const int padIndex = padIndexForKey (event.voice.key, rate);
        if (padIndex < 0)
            return;
        const SamplerNodePad& pad = pads_[static_cast<std::size_t> (padIndex)];
        // The pad's own key plays at its root: rate 1 when key == root, else transposed.
        if (pad.key == event.voice.key)
            rate = pad.key == pad.rootKey ? 1.0 : std::pow (2.0, static_cast<double> (pad.key - pad.rootKey) / 12.0);

        Voice* target = nullptr;
        for (Voice& voice : voices_)
            if (voice.stage == Stage::Idle)
            {
                target = &voice;
                break;
            }
        if (target == nullptr)
        {
            target = &voices_[0];
            for (Voice& voice : voices_)
                if (voice.age < target->age)
                    target = &voice;
        }
        target->stage = Stage::Attack;
        target->key = event.voice.key;
        target->pad = padIndex;
        target->oneShot = pad.oneShot;
        target->position = 0.0;
        target->rate = rate;
        target->envelope = 0.0f;
        target->targetLevel = static_cast<float> (velocity > 1.0 ? 1.0 : velocity);
        target->sustainLevel = target->targetLevel * sustain_;
        target->gain = pad.gain;
        target->age = ++voiceAge_;
    }

    void noteOff (const Event& event) noexcept YESDAW_RT_HOT
    {
        for (Voice& voice : voices_)
            if (voice.stage != Stage::Idle && voice.stage != Stage::Release && ! voice.oneShot
                && voice.key == event.voice.key)
                voice.stage = Stage::Release;
    }

    void renderRange (const ProcessArgs& args, int beginFrame, int endFrame, int channels) noexcept YESDAW_RT_HOT
    {
        if (beginFrame >= endFrame)
            return;
        for (Voice& voice : voices_)
        {
            if (voice.stage == Stage::Idle || voice.pad < 0 || static_cast<std::size_t> (voice.pad) >= pads_.size())
                continue;
            const SamplerNodePad& pad = pads_[static_cast<std::size_t> (voice.pad)];
            if (! pad.isLoaded())
            {
                voice.stage = Stage::Idle;
                continue;
            }
            const float* const data = pad.interleaved.data();
            const int sampleChannels = pad.channels;
            const double lastFrame = static_cast<double> (pad.frames - 1);
            const float outGain = gain_ * voice.gain;

            for (int i = beginFrame; i < endFrame; ++i)
            {
                if (voice.stage == Stage::Attack)
                {
                    voice.envelope += attackPerFrame_ * voice.targetLevel;
                    if (voice.envelope >= voice.targetLevel)
                    {
                        voice.envelope = voice.targetLevel;
                        voice.stage = voice.sustainLevel < voice.targetLevel ? Stage::Decay : Stage::Sustain;
                    }
                }
                else if (voice.stage == Stage::Decay)
                {
                    voice.envelope -= decayPerFrame_ * voice.targetLevel;
                    if (voice.envelope <= voice.sustainLevel)
                    {
                        voice.envelope = voice.sustainLevel;
                        voice.stage = Stage::Sustain;
                    }
                }
                else if (voice.stage == Stage::Release)
                {
                    voice.envelope -= releasePerFrame_ * voice.targetLevel;
                    if (voice.envelope <= 0.0f)
                    {
                        voice.envelope = 0.0f;
                        voice.stage = Stage::Idle;
                        break;
                    }
                }

                if (voice.position >= lastFrame)
                {
                    voice.stage = Stage::Idle;
                    break;
                }
                const double floorPosition = std::floor (voice.position);
                const std::size_t index = static_cast<std::size_t> (floorPosition);
                const float fraction = static_cast<float> (voice.position - floorPosition);
                const float level = voice.envelope * outGain;
                if (sampleChannels == 1)
                {
                    const float a = data[index];
                    const float b = data[index + 1];
                    const float sample = (a + (b - a) * fraction) * level;
                    for (int c = 0; c < channels; ++c)
                        args.audio.channels[c][i] += sample;
                }
                else
                {
                    const float l0 = data[index * 2], l1 = data[index * 2 + 2];
                    const float r0 = data[index * 2 + 1], r1 = data[index * 2 + 3];
                    const float left = (l0 + (l1 - l0) * fraction) * level;
                    const float right = (r0 + (r1 - r0) * fraction) * level;
                    if (channels >= 2)
                    {
                        args.audio.channels[0][i] += left;
                        args.audio.channels[1][i] += right;
                    }
                    else
                        args.audio.channels[0][i] += 0.5f * (left + right);
                }
                voice.position += voice.rate;
            }
        }
    }

    void applyParameterReal (ParameterId parameterId, double real) noexcept
    {
        switch (parameterId)
        {
            case kAttackParamId:  attackSeconds_ = real; break;
            case kDecayParamId:   decaySeconds_ = real; break;
            case kSustainParamId: sustain_ = static_cast<float> (std::clamp (real, 0.0, 1.0)); break;
            case kReleaseParamId: releaseSeconds_ = real; break;
            case kGainParamId:    gain_ = static_cast<float> (std::clamp (real, 0.0, 2.0)); break;
            default: break;
        }
        recomputeDerived();
    }

    void recomputeDerived() noexcept
    {
        const auto perFrame = [this] (double seconds) {
            return static_cast<float> (1.0 / std::max (1.0, seconds * sampleRate_));
        };
        attackPerFrame_ = perFrame (attackSeconds_);
        decayPerFrame_ = perFrame (decaySeconds_);
        releasePerFrame_ = perFrame (releaseSeconds_);
    }

    NodeId id_;
    int channels_ = 2;
    Node* eventInput_ = nullptr;
    double sampleRate_ = 48000.0;
    std::vector<SamplerNodePad> pads_;
    std::array<Voice, kMaxVoices> voices_ {};
    std::uint64_t voiceAge_ = 0;
    double attackSeconds_ = kAttackSeconds;
    double decaySeconds_ = kDecaySeconds;
    float sustain_ = static_cast<float> (kSustain);
    double releaseSeconds_ = kReleaseSeconds;
    float gain_ = static_cast<float> (kGain);
    float attackPerFrame_ = 1.0f;
    float decayPerFrame_ = 1.0f;
    float releasePerFrame_ = 1.0f;
};

} // namespace yesdaw::engine
