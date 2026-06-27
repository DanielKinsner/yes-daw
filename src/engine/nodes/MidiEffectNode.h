// YES DAW - deterministic built-in MIDI-effect Nodes for H4.
//
// These Nodes transform the block EventStream in-place when it is backed by writable storage. They do
// not allocate, reorder, add, or remove Events in process(), so the ADR-0009 sorted half-open block
// contract stays intact.

#pragma once

#include "engine/Node.h"

#include <cmath>
#include <cstdint>
#include <span>

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

} // namespace yesdaw::engine
