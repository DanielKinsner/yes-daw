// YES DAW - MIDI Clip edit model and render bridge (ADR-0017).
//
// MIDI is edited as Note objects in ticks, then flattened one-way into ADR-0009 Events at the render
// boundary. This header stays JUCE-free and pure C++ so the timing gate runs everywhere.

#pragma once

#include "engine/Node.h"
#include "engine/Project.h"
#include "engine/Time.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace yesdaw::engine {

struct Note
{
    EntityId     id;
    Tick         startTick = 0;          // clip-relative
    Tick         lengthTicks = 0;        // zero-length is legal: On and Off at the same sample
    std::int16_t key = 60;
    double       pitchNote = 60.0;
    double       normalizedVelocity = 1.0;
    std::int16_t portIndex = -1;
    std::int16_t channel = -1;

    [[nodiscard]] bool isValid() const noexcept
    {
        return id.isValid()
            && startTick >= 0
            && lengthTicks >= 0
            && key >= 0
            && key <= 127
            && std::isfinite (pitchNote)
            && std::isfinite (normalizedVelocity)
            && normalizedVelocity >= 0.0
            && normalizedVelocity <= 1.0;
    }
};

struct MidiClip
{
    EntityId id;
    Tick     timelineStart = 0;
    Tick     timelineLength = 0;
    TimeBase timeBase = TimeBase::TempoLocked;
    std::vector<Note> notes;

    [[nodiscard]] bool isValid() const noexcept
    {
        return id.isValid()
            && timelineLength >= 0
            && (timeBase == TimeBase::TempoLocked || timeBase == TimeBase::SampleLocked);
    }
};

struct MidiFlattenBlock
{
    double        startFrame = 0.0;
    std::uint32_t numFrames = 0;
    double        pdcShiftFrames = 0.0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return std::isfinite (startFrame) && std::isfinite (pdcShiftFrames);
    }
};

enum class MidiFlattenStatus : std::uint8_t
{
    Ok = 0,
    InvalidInput,
    OutputTooSmall
};

struct MidiFlattenResult
{
    MidiFlattenStatus status = MidiFlattenStatus::Ok;
    std::size_t       eventsWritten = 0;
};

[[nodiscard]] inline std::int32_t voiceNoteIdFromEntityId (EntityId id) noexcept
{
    std::uint32_t h = 2166136261u;
    for (const std::uint8_t byte : id.bytes)
    {
        h ^= static_cast<std::uint32_t> (byte);
        h *= 16777619u;
    }

    h &= 0x7FFF'FFFFu;
    return h == 0u ? 1 : static_cast<std::int32_t> (h);
}

[[nodiscard]] inline Event makeNoteEvent (std::uint32_t timeInBlock,
                                          EventType type,
                                          const Note& note) noexcept
{
    Event event {};
    event.timeInBlock = timeInBlock;
    event.type = type;
    event.voice.noteId = voiceNoteIdFromEntityId (note.id);
    event.voice.portIndex = note.portIndex;
    event.voice.channel = note.channel;
    event.voice.key = note.key;
    event.payload.note.normalizedVelocity =
        type == EventType::NoteOn ? note.normalizedVelocity : 0.0;
    event.payload.note.pitchNote = note.pitchNote;
    return event;
}

namespace detail {

[[nodiscard]] inline bool addMidiTickChecked (Tick a, Tick b, Tick& out) noexcept
{
    if (b > 0 && a > std::numeric_limits<Tick>::max() - b)
        return false;
    if (b < 0 && a < std::numeric_limits<Tick>::min() - b)
        return false;

    out = a + b;
    return true;
}

struct MidiEventCandidate
{
    double    frame = 0.0;
    EntityId  clipId;
    EntityId  noteId;
    EventType type = EventType::NoteOn;
    Event     event;
};

[[nodiscard]] inline bool midiCandidateLess (const MidiEventCandidate& a,
                                             const MidiEventCandidate& b) noexcept
{
    if (a.event.timeInBlock != b.event.timeInBlock)
        return a.event.timeInBlock < b.event.timeInBlock;
    if (a.clipId != b.clipId)
        return a.clipId < b.clipId;
    if (a.noteId != b.noteId)
        return a.noteId < b.noteId;

    // Same-sample zero-length Notes stay On then Off.
    return static_cast<std::uint16_t> (a.type) < static_cast<std::uint16_t> (b.type);
}

} // namespace detail

template <typename TickToFrame>
[[nodiscard]] inline MidiFlattenResult flattenMidiClipNotesForBlock (
    const MidiClip& clip,
    MidiFlattenBlock block,
    TickToFrame tickToFrameFn,
    std::span<Event> outEvents)
{
    MidiFlattenResult result;

    if (! clip.isValid() || ! block.isValid())
    {
        result.status = MidiFlattenStatus::InvalidInput;
        return result;
    }

    std::vector<detail::MidiEventCandidate> candidates;
    candidates.reserve (clip.notes.size() * 2u);

    const double blockEnd = block.startFrame + static_cast<double> (block.numFrames);
    for (const Note& note : clip.notes)
    {
        if (! note.isValid())
        {
            result.status = MidiFlattenStatus::InvalidInput;
            return result;
        }

        Tick noteEnd = 0;
        Tick onTick = 0;
        Tick offTick = 0;
        if (! detail::addMidiTickChecked (note.startTick, note.lengthTicks, noteEnd)
            || noteEnd > clip.timelineLength
            || ! detail::addMidiTickChecked (clip.timelineStart, note.startTick, onTick)
            || ! detail::addMidiTickChecked (clip.timelineStart, noteEnd, offTick))
        {
            result.status = MidiFlattenStatus::InvalidInput;
            return result;
        }

        const auto addCandidate = [&] (Tick tick, EventType type) -> bool
        {
            double sourceFrame = 0.0;
            if (! tickToFrameFn (tick, sourceFrame) || ! std::isfinite (sourceFrame))
                return false;

            const double eventFrame = sourceFrame + block.pdcShiftFrames;
            if (eventFrame < block.startFrame || eventFrame >= blockEnd)
                return true;

            const double offset = eventFrame - block.startFrame;
            if (offset < 0.0 || offset >= static_cast<double> (block.numFrames))
                return true;

            detail::MidiEventCandidate candidate;
            candidate.frame = eventFrame;
            candidate.clipId = clip.id;
            candidate.noteId = note.id;
            candidate.type = type;
            candidate.event = makeNoteEvent (static_cast<std::uint32_t> (std::floor (offset)), type, note);
            candidates.push_back (candidate);
            return true;
        };

        if (! addCandidate (onTick, EventType::NoteOn) || ! addCandidate (offTick, EventType::NoteOff))
        {
            result.status = MidiFlattenStatus::InvalidInput;
            return result;
        }
    }

    std::sort (candidates.begin(), candidates.end(), detail::midiCandidateLess);

    if (candidates.size() > outEvents.size())
    {
        result.status = MidiFlattenStatus::OutputTooSmall;
        return result;
    }

    for (const detail::MidiEventCandidate& candidate : candidates)
        outEvents[result.eventsWritten++] = candidate.event;

    return result;
}

[[nodiscard]] inline MidiFlattenResult flattenMidiClipNotesForBlock (
    const MidiClip& clip,
    MidiFlattenBlock block,
    TempoMapView tempoMap,
    SampleRate sampleRate,
    std::span<Event> outEvents)
{
    return flattenMidiClipNotesForBlock (
        clip,
        block,
        [tempoMap, sampleRate] (Tick tick, double& frame) noexcept
        {
            return tickToFrame (tempoMap, sampleRate, tick, frame);
        },
        outEvents);
}

} // namespace yesdaw::engine
