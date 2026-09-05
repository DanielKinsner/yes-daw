// YES DAW - deterministic built-in MIDI-effect Nodes for H4.
//
// These Nodes transform the block EventStream in-place when it is backed by writable storage. They do
// not allocate, reorder, add, or remove Events in process(), so the ADR-0009 sorted half-open block
// contract stays intact.

#pragma once

#include "engine/Node.h"
#include "engine/ParamSpec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace yesdaw::engine {

class MidiTransposeNode final : public Node
{
public:
    explicit MidiTransposeNode (NodeId id, std::int32_t semitones = 0) noexcept
        : id_ (id), semitones_ (semitones)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ false, /*producesEvents*/ true,
                                /*channels*/ 1, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        silenceAudio (args);

        for (Event& event : args.events.writableEvents())
            transposeEvent (event, semitones_);
    }

    void reset() noexcept override {}
    void release() override {}

    void setInput (Node* input) noexcept { input_ = input; }
    void setSemitones (std::int32_t semitones) noexcept { semitones_ = semitones; }
    [[nodiscard]] std::int32_t semitones() const noexcept { return semitones_; }

    // G3.8: the insert chain's parameter law (one ParamSpec per id, normalized 0..1 in the Project).
    static constexpr ParameterId kSemitonesParamId = 1;
    static constexpr double kMaxSemitones = 24.0;
    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        if (parameterId == kSemitonesParamId)
            semitones_ = static_cast<std::int32_t> (std::lround (mapNormalized (parameterSpec (parameterId), normalizedValue)));
    }
    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        if (parameterId == kSemitonesParamId)
            return ParamSpec { parameterId, "transpose.semitones", "st", -kMaxSemitones, kMaxSemitones, 0.0 };
        return {};
    }

    static void transposeEvent (Event& event, std::int32_t semitones) noexcept
    {
        if (! isNoteLikeEvent (event) || event.voice.key < 0)
            return;

        const int transposed = static_cast<int> (event.voice.key) + static_cast<int> (semitones);
        if (transposed < 0 || transposed > 127)
            return;

        event.voice.key = static_cast<std::int16_t> (transposed);
        if (std::isfinite (event.payload.note.pitchNote))
            event.payload.note.pitchNote += static_cast<double> (semitones);
    }

private:
    static bool isNoteLikeEvent (const Event& event) noexcept
    {
        return event.type == EventType::NoteOn
            || event.type == EventType::NoteOff
            || event.type == EventType::NoteExpression;
    }

    static void silenceAudio (const ProcessArgs& args) noexcept YESDAW_RT_HOT
    {
        for (int c = 0; c < args.audio.numChannels; ++c)
        {
            float* const out = args.audio.channels[c];
            if (out == nullptr)
                continue;

            for (int i = 0; i < args.numFrames; ++i)
                out[i] = 0.0f;
        }
    }

    NodeId       id_;
    std::int32_t semitones_;
    Node*        input_ = nullptr;
};

class MidiScaleMapNode final : public Node
{
public:
    static constexpr std::uint16_t kChromaticMask = 0x0FFFu;
    static constexpr std::uint16_t kMajorMask =
        static_cast<std::uint16_t> ((1u << 0u) | (1u << 2u) | (1u << 4u) | (1u << 5u)
                                    | (1u << 7u) | (1u << 9u) | (1u << 11u));
    static constexpr std::uint16_t kNaturalMinorMask =
        static_cast<std::uint16_t> ((1u << 0u) | (1u << 2u) | (1u << 3u) | (1u << 5u)
                                    | (1u << 7u) | (1u << 8u) | (1u << 10u));

    explicit MidiScaleMapNode (NodeId id,
                               std::int16_t rootKey = 0,
                               std::uint16_t scaleMask = kMajorMask) noexcept
        : id_ (id), rootKey_ (normalizeRoot (rootKey)), scaleMask_ (sanitizeMask (scaleMask))
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ false, /*producesEvents*/ true,
                                /*channels*/ 1, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        silenceAudio (args);

        for (Event& event : args.events.writableEvents())
            mapEvent (event, rootKey_, scaleMask_);
    }

    void reset() noexcept override {}
    void release() override {}

    void setInput (Node* input) noexcept { input_ = input; }
    void setScale (std::int16_t rootKey, std::uint16_t scaleMask) noexcept
    {
        rootKey_ = normalizeRoot (rootKey);
        scaleMask_ = sanitizeMask (scaleMask);
    }

    // G3.8: the insert chain's parameter law — the root as a 12-way choice, the scale as a choice
    // (Chromatic passes everything; Major; Natural Minor).
    static constexpr ParameterId kRootParamId = 1;
    static constexpr ParameterId kScaleParamId = 2;
    static constexpr std::uint8_t kRootCount = 12;
    static constexpr std::uint8_t kScaleChoiceCount = 3;
    [[nodiscard]] static std::uint16_t maskForScaleChoice (std::uint8_t choice) noexcept
    {
        switch (choice)
        {
            case 0: return kChromaticMask;
            case 1: return kMajorMask;
            default: return kNaturalMinorMask;
        }
    }
    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        const long real = std::lround (mapNormalized (parameterSpec (parameterId), normalizedValue));
        if (parameterId == kRootParamId)
            rootKey_ = normalizeRoot (static_cast<std::int16_t> (std::clamp (real, 0L, static_cast<long> (kRootCount - 1))));
        else if (parameterId == kScaleParamId)
            scaleMask_ = maskForScaleChoice (static_cast<std::uint8_t> (std::clamp (real, 0L, static_cast<long> (kScaleChoiceCount - 1))));
    }
    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        static constexpr const char* kRootNames[kRootCount] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        static constexpr const char* kScaleNames[kScaleChoiceCount] = { "Chromatic", "Major", "Minor" };
        if (parameterId == kRootParamId)
            return ParamSpec { parameterId, "scale.root", "", 0.0, static_cast<double> (kRootCount - 1), 0.0,
                               ParamMapping::Linear, ParamSmoothing::None, kRootCount, kRootNames };
        if (parameterId == kScaleParamId)
            return ParamSpec { parameterId, "scale.scale", "", 0.0, static_cast<double> (kScaleChoiceCount - 1), 1.0,
                               ParamMapping::Linear, ParamSmoothing::None, kScaleChoiceCount, kScaleNames };
        return {};
    }

    [[nodiscard]] std::int16_t rootKey() const noexcept { return rootKey_; }
    [[nodiscard]] std::uint16_t scaleMask() const noexcept { return scaleMask_; }

    static void mapEvent (Event& event, std::int16_t rootKey, std::uint16_t scaleMask) noexcept
    {
        if (! isNoteLikeEvent (event) || event.voice.key < 0)
            return;

        const std::int16_t mapped = mapKeyToScale (event.voice.key, rootKey, sanitizeMask (scaleMask));
        const int delta = static_cast<int> (mapped) - static_cast<int> (event.voice.key);
        event.voice.key = mapped;
        if (std::isfinite (event.payload.note.pitchNote))
            event.payload.note.pitchNote += static_cast<double> (delta);
    }

    static std::int16_t mapKeyToScale (std::int16_t key,
                                       std::int16_t rootKey,
                                       std::uint16_t scaleMask) noexcept
    {
        if (key < 0)
            return key;
        if (key > 127)
            return 127;

        const std::int16_t root = normalizeRoot (rootKey);
        const std::uint16_t mask = sanitizeMask (scaleMask);
        const int degree = positiveMod (static_cast<int> (key) - static_cast<int> (root), 12);
        if ((mask & (1u << static_cast<unsigned> (degree))) != 0u)
            return key;

        for (int delta = 1; delta < 12; ++delta)
        {
            const int upKey = static_cast<int> (key) + delta;
            const int upDegree = positiveMod (degree + delta, 12);
            if (upKey <= 127 && (mask & (1u << static_cast<unsigned> (upDegree))) != 0u)
                return static_cast<std::int16_t> (upKey);
        }

        for (int delta = 1; delta < 12; ++delta)
        {
            const int downKey = static_cast<int> (key) - delta;
            const int downDegree = positiveMod (degree - delta, 12);
            if (downKey >= 0 && (mask & (1u << static_cast<unsigned> (downDegree))) != 0u)
                return static_cast<std::int16_t> (downKey);
        }

        return key;
    }

private:
    static bool isNoteLikeEvent (const Event& event) noexcept
    {
        return event.type == EventType::NoteOn
            || event.type == EventType::NoteOff
            || event.type == EventType::NoteExpression;
    }

    static int positiveMod (int value, int modulus) noexcept
    {
        const int result = value % modulus;
        return result < 0 ? result + modulus : result;
    }

    static std::int16_t normalizeRoot (std::int16_t rootKey) noexcept
    {
        return static_cast<std::int16_t> (positiveMod (static_cast<int> (rootKey), 12));
    }

    static std::uint16_t sanitizeMask (std::uint16_t scaleMask) noexcept
    {
        const std::uint16_t masked = static_cast<std::uint16_t> (scaleMask & kChromaticMask);
        return masked == 0u ? kChromaticMask : masked;
    }

    static void silenceAudio (const ProcessArgs& args) noexcept YESDAW_RT_HOT
    {
        for (int c = 0; c < args.audio.numChannels; ++c)
        {
            float* const out = args.audio.channels[c];
            if (out == nullptr)
                continue;

            for (int i = 0; i < args.numFrames; ++i)
                out[i] = 0.0f;
        }
    }

    NodeId        id_;
    std::int16_t  rootKey_;
    std::uint16_t scaleMask_;
    Node*         input_ = nullptr;
};

// --- G3.8: the MIDI FX that ADD events. Both own a fixed scratch buffer (sized in prepare(), the only
// --- place a Node may allocate) and hand it back through EventStream::replaceEvents, so the block
// --- contract (sorted, half-open) holds and nothing allocates in process().

// Chord Trigger (Logic's Chord Trigger, the simple form): every NoteOn / NoteOff is followed by copies at
// up to three intervals above it (0 = that interval is off; a copy pushed past 127 drops). The copies
// carry the original's channel / port, a derived noteId, and the original velocity scaled.
class MidiChordNode final : public Node
{
public:
    static constexpr ParameterId kInterval1ParamId = 1;
    static constexpr ParameterId kInterval2ParamId = 2;
    static constexpr ParameterId kInterval3ParamId = 3;
    static constexpr ParameterId kVelocityParamId = 4;   // the copies' velocity as % of the original
    static constexpr int kMaxInterval = 24;
    static constexpr std::size_t kMaxEventsPerBlock = 1024;

    explicit MidiChordNode (NodeId id) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ false, /*producesEvents*/ true,
                                /*channels*/ 1, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override { scratch_.assign (kMaxEventsPerBlock, Event {}); }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        silenceAudio (args);
        if (scratch_.empty())
            return;

        const std::size_t capacity = std::min (scratch_.size(), args.events.writableCapacity());
        std::size_t written = 0;
        for (const Event& event : args.events.events())
        {
            if (written >= capacity)
                break;
            scratch_[written++] = event;
            if (! isNoteEvent (event) || event.voice.key < 0)
                continue;
            const int intervals[3] = { interval1_, interval2_, interval3_ };
            for (int i = 0; i < 3; ++i)
            {
                const int interval = intervals[i];
                if (interval <= 0 || written >= capacity)
                    continue;
                const int key = static_cast<int> (event.voice.key) + interval;
                if (key > 127)
                    continue;
                Event copy = event;
                copy.voice.key = static_cast<std::int16_t> (key);
                copy.voice.noteId = derivedNoteId (event.voice.noteId, i + 1);
                if (std::isfinite (copy.payload.note.pitchNote))
                    copy.payload.note.pitchNote += static_cast<double> (interval);
                if (event.type == EventType::NoteOn)
                    copy.payload.note.normalizedVelocity = std::clamp (event.payload.note.normalizedVelocity * velocityScale_, 0.0, 1.0);
                scratch_[written++] = copy;
            }
        }
        (void) args.events.replaceEvents (std::span<const Event> (scratch_.data(), written));
    }

    void reset() noexcept override {}
    void release() override { scratch_.clear(); scratch_.shrink_to_fit(); }

    void setInput (Node* input) noexcept { input_ = input; }
    void setIntervals (int first, int second, int third) noexcept
    {
        interval1_ = std::clamp (first, 0, kMaxInterval);
        interval2_ = std::clamp (second, 0, kMaxInterval);
        interval3_ = std::clamp (third, 0, kMaxInterval);
    }
    void setVelocityPercent (double percent) noexcept { velocityScale_ = std::clamp (percent, 10.0, 100.0) / 100.0; }
    [[nodiscard]] int interval1() const noexcept { return interval1_; }
    [[nodiscard]] int interval2() const noexcept { return interval2_; }
    [[nodiscard]] int interval3() const noexcept { return interval3_; }

    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        const double real = mapNormalized (parameterSpec (parameterId), normalizedValue);
        switch (parameterId)
        {
            case kInterval1ParamId: interval1_ = static_cast<int> (std::lround (real)); break;
            case kInterval2ParamId: interval2_ = static_cast<int> (std::lround (real)); break;
            case kInterval3ParamId: interval3_ = static_cast<int> (std::lround (real)); break;
            case kVelocityParamId:  setVelocityPercent (real); break;
            default: break;
        }
    }

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kInterval1ParamId: return ParamSpec { parameterId, "chord.interval1", "st", 0.0, static_cast<double> (kMaxInterval), 4.0 };
            case kInterval2ParamId: return ParamSpec { parameterId, "chord.interval2", "st", 0.0, static_cast<double> (kMaxInterval), 7.0 };
            case kInterval3ParamId: return ParamSpec { parameterId, "chord.interval3", "st", 0.0, static_cast<double> (kMaxInterval), 0.0 };
            case kVelocityParamId:  return ParamSpec { parameterId, "chord.velocity", "%", 10.0, 100.0, 100.0 };
            default: return {};
        }
    }

    [[nodiscard]] static std::int32_t derivedNoteId (std::int32_t noteId, int slot) noexcept
    {
        // A distinct, stable id per copy so a NoteOff finds its NoteOn (the synth pairs by key; MPE-
        // aware instruments pair by id).
        const std::uint32_t mixed = (static_cast<std::uint32_t> (noteId) * 2654435761u) ^ (static_cast<std::uint32_t> (slot) * 40503u);
        return static_cast<std::int32_t> (mixed & 0x7FFFFFFFu);
    }

private:
    static bool isNoteEvent (const Event& event) noexcept
    {
        return event.type == EventType::NoteOn || event.type == EventType::NoteOff;
    }

    static void silenceAudio (const ProcessArgs& args) noexcept YESDAW_RT_HOT
    {
        for (int c = 0; c < args.audio.numChannels; ++c)
        {
            float* const out = args.audio.channels[c];
            if (out == nullptr)
                continue;
            for (int i = 0; i < args.numFrames; ++i)
                out[i] = 0.0f;
        }
    }

    NodeId id_;
    Node* input_ = nullptr;
    int interval1_ = 4;
    int interval2_ = 7;
    int interval3_ = 0;
    double velocityScale_ = 1.0;
    std::vector<Event> scratch_;
};

// Arpeggiator (Logic's Arpeggiator, the core): the held notes (the NoteOns not yet released) are
// played one per step on a grid of the timeline — rate 1/4 · 1/8 · 1/16 · 1/32 of a quarter at the
// frames-per-quarter the projection sets from the head tempo — in an order (Up, Down, Up-Down, As
// Played) across 1–4 octaves, each step's note held for the gate fraction of the step. The incoming
// NoteOn / NoteOff are consumed (they only change the held set); every other event passes through.
// The grid is absolute (step k starts at frame k × stepFrames), so playback is deterministic and a
// loop or locate lands on the same notes; a transport jump releases the sounding step first.
class MidiArpeggiatorNode final : public Node
{
public:
    static constexpr ParameterId kRateParamId = 1;      // choice: 1/4, 1/8, 1/16, 1/32
    static constexpr ParameterId kOrderParamId = 2;     // choice: Up, Down, Up-Down, As Played
    static constexpr ParameterId kOctavesParamId = 3;   // 1..4
    static constexpr ParameterId kGateParamId = 4;      // 10..100 % of the step
    static constexpr std::size_t kMaxHeld = 16;
    static constexpr std::size_t kMaxEventsPerBlock = 1024;
    static constexpr std::uint8_t kRateCount = 4;
    static constexpr std::uint8_t kOrderCount = 4;

    enum class Order : std::uint8_t { Up = 0, Down = 1, UpDown = 2, AsPlayed = 3 };

    explicit MidiArpeggiatorNode (NodeId id) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ false, /*producesEvents*/ true,
                                /*channels*/ 1, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override
    {
        scratch_.assign (kMaxEventsPerBlock, Event {});
        reset();
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        silenceAudio (args);
        if (scratch_.empty() || args.numFrames <= 0)
            return;

        const std::size_t capacity = std::min (scratch_.size(), args.events.writableCapacity());
        std::size_t written = 0;
        const auto emit = [&] (const Event& event) noexcept
        {
            if (written < capacity)
                scratch_[written++] = event;
        };

        const bool hasTransportFrame = args.transport.hasTimelineFrame;
        const std::int64_t blockStart = hasTransportFrame ? args.transport.timelineFrame : cursorFrame_;
        const std::int64_t blockEnd = blockStart + static_cast<std::int64_t> (args.numFrames);

        // A stopped transport (live notes only, G3.2) or a jump: the sounding step ends now.
        if (args.transport.clipsSilenced)
        {
            if (soundingOffFrame_ >= 0)
            {
                emit (offEvent (0));
                soundingOffFrame_ = -1;
            }
            heldCount_ = 0;
            (void) args.events.replaceEvents (std::span<const Event> (scratch_.data(), written));
            cursorFrame_ = blockEnd;
            return;
        }
        if (soundingOffFrame_ >= 0 && (blockStart != cursorFrame_ || soundingOffFrame_ < blockStart))
        {
            emit (offEvent (0));
            soundingOffFrame_ = -1;
        }

        const std::span<const Event> input = args.events.events();
        std::size_t inputIndex = 0;
        const std::int64_t stepFrames = std::max<std::int64_t> (1, stepFrames_);
        const std::int64_t gateFrames = std::clamp<std::int64_t> (static_cast<std::int64_t> (static_cast<double> (stepFrames) * gate_ + 0.5), 1, stepFrames - 1 > 0 ? stepFrames - 1 : 1);

        for (std::int64_t frame = blockStart; frame < blockEnd; ++frame)
        {
            const std::uint32_t timeInBlock = static_cast<std::uint32_t> (frame - blockStart);
            // Input at this frame: notes change the held set, everything else passes through.
            while (inputIndex < input.size() && input[inputIndex].timeInBlock <= timeInBlock)
            {
                const Event& event = input[inputIndex++];
                if (event.type == EventType::NoteOn && event.voice.key >= 0)
                    hold (event);
                else if (event.type == EventType::NoteOff && event.voice.key >= 0)
                    unhold (event);
                else
                    emit (event);
            }
            if (soundingOffFrame_ == frame)
            {
                emit (offEvent (timeInBlock));
                soundingOffFrame_ = -1;
            }
            if (frame % stepFrames == 0 && heldCount_ > 0)
            {
                const std::int64_t step = frame / stepFrames;
                Held note;
                if (pickStep (step, note))
                {
                    if (soundingOffFrame_ >= 0)   // a gate of 100 % meets the next step: release first
                    {
                        emit (offEvent (timeInBlock));
                        soundingOffFrame_ = -1;
                    }
                    sounding_ = note;
                    sounding_.noteId = static_cast<std::int32_t> ((static_cast<std::uint32_t> (note.noteId) * 31u + static_cast<std::uint32_t> (step & 0xFFFF)) & 0x7FFFFFFFu);
                    Event on;
                    on.timeInBlock = timeInBlock;
                    on.type = EventType::NoteOn;
                    on.voice.noteId = sounding_.noteId;
                    on.voice.portIndex = note.portIndex;
                    on.voice.channel = note.channel;
                    on.voice.key = sounding_.key;
                    on.payload.note.normalizedVelocity = note.velocity;
                    on.payload.note.pitchNote = static_cast<double> (sounding_.key);
                    emit (on);
                    soundingOffFrame_ = frame + gateFrames;
                }
            }
        }
        // Input events past the block (never expected: the contract is half-open) still change the held set.
        for (; inputIndex < input.size(); ++inputIndex)
        {
            const Event& event = input[inputIndex];
            if (event.type == EventType::NoteOn && event.voice.key >= 0)
                hold (event);
            else if (event.type == EventType::NoteOff && event.voice.key >= 0)
                unhold (event);
        }
        (void) args.events.replaceEvents (std::span<const Event> (scratch_.data(), written));
        cursorFrame_ = blockEnd;
    }

    void reset() noexcept override
    {
        heldCount_ = 0;
        soundingOffFrame_ = -1;
        cursorFrame_ = 0;
        holdSerial_ = 0;
    }
    void release() override { scratch_.clear(); scratch_.shrink_to_fit(); }

    void setInput (Node* input) noexcept { input_ = input; }
    // The projection's tempo law: frames per quarter note at the head tempo (the grid is in frames).
    void setFramesPerQuarter (double framesPerQuarter) noexcept
    {
        framesPerQuarter_ = std::isfinite (framesPerQuarter) && framesPerQuarter > 0.0 ? framesPerQuarter : 24000.0;
        updateStepFrames();
    }
    void setRateChoice (std::uint8_t rate) noexcept { rate_ = std::min<std::uint8_t> (rate, kRateCount - 1); updateStepFrames(); }
    void setOrder (Order order) noexcept { order_ = order; }
    void setOctaves (int octaves) noexcept { octaves_ = std::clamp (octaves, 1, 4); }
    void setGatePercent (double percent) noexcept { gate_ = std::clamp (percent, 10.0, 100.0) / 100.0; }
    [[nodiscard]] std::int64_t stepFrames() const noexcept { return stepFrames_; }
    [[nodiscard]] Order order() const noexcept { return order_; }
    [[nodiscard]] int octaves() const noexcept { return octaves_; }
    [[nodiscard]] std::size_t heldCount() const noexcept { return heldCount_; }

    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        const double real = mapNormalized (parameterSpec (parameterId), normalizedValue);
        switch (parameterId)
        {
            case kRateParamId:    setRateChoice (static_cast<std::uint8_t> (std::clamp (std::lround (real), 0L, static_cast<long> (kRateCount - 1)))); break;
            case kOrderParamId:   order_ = static_cast<Order> (std::clamp (std::lround (real), 0L, static_cast<long> (kOrderCount - 1))); break;
            case kOctavesParamId: setOctaves (static_cast<int> (std::lround (real))); break;
            case kGateParamId:    setGatePercent (real); break;
            default: break;
        }
    }

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        static constexpr const char* kRateNames[kRateCount] = { "1/4", "1/8", "1/16", "1/32" };
        static constexpr const char* kOrderNames[kOrderCount] = { "Up", "Down", "Up-Down", "As Played" };
        switch (parameterId)
        {
            case kRateParamId:
                return ParamSpec { parameterId, "arp.rate", "", 0.0, static_cast<double> (kRateCount - 1), 2.0,
                                   ParamMapping::Linear, ParamSmoothing::None, kRateCount, kRateNames };
            case kOrderParamId:
                return ParamSpec { parameterId, "arp.order", "", 0.0, static_cast<double> (kOrderCount - 1), 0.0,
                                   ParamMapping::Linear, ParamSmoothing::None, kOrderCount, kOrderNames };
            case kOctavesParamId: return ParamSpec { parameterId, "arp.octaves", "", 1.0, 4.0, 1.0 };
            case kGateParamId:    return ParamSpec { parameterId, "arp.gate", "%", 10.0, 100.0, 80.0 };
            default: return {};
        }
    }

    [[nodiscard]] static double quartersForRateChoice (std::uint8_t rate) noexcept
    {
        switch (std::min<std::uint8_t> (rate, kRateCount - 1))
        {
            case 0: return 1.0;
            case 1: return 0.5;
            case 2: return 0.25;
            default: return 0.125;
        }
    }

private:
    struct Held
    {
        std::int16_t key = -1;
        std::int16_t channel = -1;
        std::int16_t portIndex = -1;
        std::int32_t noteId = -1;
        double velocity = 1.0;
        std::uint32_t serial = 0;   // the order the note was played
    };

    void updateStepFrames() noexcept
    {
        stepFrames_ = std::max<std::int64_t> (1, static_cast<std::int64_t> (framesPerQuarter_ * quartersForRateChoice (rate_) + 0.5));
    }

    void hold (const Event& event) noexcept
    {
        for (std::size_t i = 0; i < heldCount_; ++i)
            if (held_[i].key == event.voice.key && held_[i].channel == event.voice.channel)
            {
                held_[i].velocity = event.payload.note.normalizedVelocity;
                held_[i].noteId = event.voice.noteId;
                return;
            }
        if (heldCount_ >= kMaxHeld)
        {
            // Full: the oldest goes (its serial is the smallest).
            std::size_t oldest = 0;
            for (std::size_t i = 1; i < heldCount_; ++i)
                if (held_[i].serial < held_[oldest].serial)
                    oldest = i;
            held_[oldest] = held_[heldCount_ - 1];
            --heldCount_;
        }
        Held& slot = held_[heldCount_++];
        slot.key = event.voice.key;
        slot.channel = event.voice.channel;
        slot.portIndex = event.voice.portIndex;
        slot.noteId = event.voice.noteId;
        slot.velocity = event.payload.note.normalizedVelocity;
        slot.serial = holdSerial_++;
    }

    void unhold (const Event& event) noexcept
    {
        for (std::size_t i = 0; i < heldCount_; ++i)
            if (held_[i].key == event.voice.key && held_[i].channel == event.voice.channel)
            {
                for (std::size_t j = i + 1; j < heldCount_; ++j)
                    held_[j - 1] = held_[j];
                --heldCount_;
                return;
            }
    }

    // The note for step k: the held set ordered, spread across the octaves, cycled.
    [[nodiscard]] bool pickStep (std::int64_t step, Held& out) const noexcept
    {
        if (heldCount_ == 0)
            return false;
        Held ordered[kMaxHeld];
        std::size_t count = 0;
        for (std::size_t i = 0; i < heldCount_; ++i)
            ordered[count++] = held_[i];
        if (order_ != Order::AsPlayed)
            std::sort (ordered, ordered + count, [] (const Held& a, const Held& b) { return a.key < b.key; });
        else
            std::sort (ordered, ordered + count, [] (const Held& a, const Held& b) { return a.serial < b.serial; });

        // One pass across the octaves, then the order's shape over that pass.
        const std::size_t passLength = count * static_cast<std::size_t> (octaves_);
        std::size_t patternLength = passLength;
        if (order_ == Order::UpDown && passLength > 2)
            patternLength = passLength * 2 - 2;
        const std::size_t index = static_cast<std::size_t> (step % static_cast<std::int64_t> (patternLength));
        std::size_t passIndex = index;
        if (order_ == Order::UpDown && index >= passLength)
            passIndex = (passLength - 1) - (index - passLength + 1);
        if (order_ == Order::Down)
            passIndex = passLength - 1 - passIndex;
        const std::size_t noteIndex = passIndex % count;
        const int octave = static_cast<int> (passIndex / count);
        out = ordered[noteIndex];
        const int key = static_cast<int> (out.key) + 12 * octave;
        if (key > 127)
            return false;
        out.key = static_cast<std::int16_t> (key);
        return true;
    }

    [[nodiscard]] Event offEvent (std::uint32_t timeInBlock) const noexcept
    {
        Event off;
        off.timeInBlock = timeInBlock;
        off.type = EventType::NoteOff;
        off.voice.noteId = sounding_.noteId;
        off.voice.portIndex = sounding_.portIndex;
        off.voice.channel = sounding_.channel;
        off.voice.key = sounding_.key;
        off.payload.note.normalizedVelocity = 0.0;
        off.payload.note.pitchNote = static_cast<double> (sounding_.key);
        return off;
    }

    static void silenceAudio (const ProcessArgs& args) noexcept YESDAW_RT_HOT
    {
        for (int c = 0; c < args.audio.numChannels; ++c)
        {
            float* const out = args.audio.channels[c];
            if (out == nullptr)
                continue;
            for (int i = 0; i < args.numFrames; ++i)
                out[i] = 0.0f;
        }
    }

    NodeId id_;
    Node* input_ = nullptr;
    double framesPerQuarter_ = 24000.0;
    std::uint8_t rate_ = 2;
    Order order_ = Order::Up;
    int octaves_ = 1;
    double gate_ = 0.8;
    std::int64_t stepFrames_ = 6000;
    Held held_[kMaxHeld];
    std::size_t heldCount_ = 0;
    std::uint32_t holdSerial_ = 0;
    Held sounding_;
    std::int64_t soundingOffFrame_ = -1;
    std::int64_t cursorFrame_ = 0;
    std::vector<Event> scratch_;
};

} // namespace yesdaw::engine
