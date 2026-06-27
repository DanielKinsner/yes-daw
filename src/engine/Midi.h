// YES DAW - MIDI Clip edit model and render bridge (ADR-0017).
//
// MIDI is edited as Note objects in ticks, then flattened one-way into ADR-0009 Events at the render
// boundary. This header stays JUCE-free and pure C++ so the timing gate runs everywhere.

#pragma once

#include "engine/Node.h"
#include "engine/Project.h"
#include "engine/Time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace yesdaw::engine {

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

struct MpeVoiceAllocationConfig
{
    std::int16_t portIndex = 0;
    std::int16_t firstMemberChannel = 1;
    std::int16_t memberChannelCount = 15;

    [[nodiscard]] bool isValid() const noexcept
    {
        return portIndex >= 0
            && firstMemberChannel >= 0
            && firstMemberChannel <= 15
            && memberChannelCount > 0
            && static_cast<int> (firstMemberChannel) + static_cast<int> (memberChannelCount) <= 16;
    }
};

enum class MpeVoiceAllocationStatus : std::uint8_t
{
    Ok = 0,
    InvalidInput,
    OutputTooSmall,
    OutOfVoices
};

struct MpeVoiceAllocationResult
{
    MpeVoiceAllocationStatus status = MpeVoiceAllocationStatus::Ok;
    std::size_t              notesWritten = 0;
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

struct MpeAllocationItem
{
    std::size_t index = 0;
    Tick        startTick = 0;
    Tick        endTick = 0;
    EntityId    noteId;
    bool        hasExplicitChannel = false;
};

struct MpeExplicitReservation
{
    Tick        startTick = 0;
    Tick        endTick = 0;
    std::int16_t channel = -1;
};

[[nodiscard]] inline bool mpeAllocationItemLess (const MpeAllocationItem& a,
                                                 const MpeAllocationItem& b) noexcept
{
    if (a.startTick != b.startTick)
        return a.startTick < b.startTick;
    if (a.hasExplicitChannel != b.hasExplicitChannel)
        return a.hasExplicitChannel;
    if (a.endTick != b.endTick)
        return a.endTick < b.endTick;
    return a.noteId < b.noteId;
}

[[nodiscard]] inline bool mpeChannelInRange (std::int16_t channel,
                                             const MpeVoiceAllocationConfig& config) noexcept
{
    return channel >= config.firstMemberChannel
        && channel < static_cast<std::int16_t> (config.firstMemberChannel + config.memberChannelCount);
}

[[nodiscard]] inline bool mpeTicksOverlap (Tick aStart, Tick aEnd, Tick bStart, Tick bEnd) noexcept
{
    return aStart < bEnd && bStart < aEnd;
}

[[nodiscard]] inline bool mpeChannelReserved (std::int16_t channel,
                                              Tick startTick,
                                              Tick endTick,
                                              std::span<const MpeExplicitReservation> reservations) noexcept
{
    for (const MpeExplicitReservation& reservation : reservations)
        if (reservation.channel == channel
            && mpeTicksOverlap (startTick, endTick, reservation.startTick, reservation.endTick))
            return true;

    return false;
}

} // namespace detail

[[nodiscard]] inline MpeVoiceAllocationResult allocateMpeVoiceAddresses (
    const MidiClip& clip,
    MpeVoiceAllocationConfig config,
    std::span<Note> outNotes)
{
    MpeVoiceAllocationResult result;

    if (! clip.isValid() || ! config.isValid())
    {
        result.status = MpeVoiceAllocationStatus::InvalidInput;
        return result;
    }

    if (clip.notes.size() > outNotes.size())
    {
        result.status = MpeVoiceAllocationStatus::OutputTooSmall;
        return result;
    }

    std::vector<detail::MpeAllocationItem> items;
    items.reserve (clip.notes.size());
    std::vector<detail::MpeExplicitReservation> explicitReservations;
    explicitReservations.reserve (clip.notes.size());

    for (std::size_t i = 0; i < clip.notes.size(); ++i)
    {
        const Note& note = clip.notes[i];
        Tick noteEnd = 0;
        if (! note.isValid() || ! detail::addMidiTickChecked (note.startTick, note.lengthTicks, noteEnd)
            || noteEnd > clip.timelineLength)
        {
            result.status = MpeVoiceAllocationStatus::InvalidInput;
            return result;
        }

        outNotes[i] = note;
        if (outNotes[i].portIndex < 0)
            outNotes[i].portIndex = config.portIndex;

        const bool hasExplicitChannel = outNotes[i].channel >= 0;
        items.push_back (detail::MpeAllocationItem { i, note.startTick, noteEnd, note.id, hasExplicitChannel });

        if (hasExplicitChannel
            && outNotes[i].portIndex == config.portIndex
            && detail::mpeChannelInRange (outNotes[i].channel, config)
            && noteEnd > note.startTick)
        {
            explicitReservations.push_back (detail::MpeExplicitReservation {
                note.startTick,
                noteEnd,
                outNotes[i].channel
            });
        }
    }

    std::sort (items.begin(), items.end(), detail::mpeAllocationItemLess);

    std::array<Tick, 16> activeUntil {};
    activeUntil.fill (0);

    const auto reserveIfMember = [&] (const Note& note, Tick endTick) noexcept
    {
        if (note.portIndex != config.portIndex || ! detail::mpeChannelInRange (note.channel, config))
            return;

        const std::size_t channel = static_cast<std::size_t> (note.channel);
        if (endTick > activeUntil[channel])
            activeUntil[channel] = endTick;
    };

    for (const detail::MpeAllocationItem& item : items)
    {
        Note& note = outNotes[item.index];

        if (note.channel >= 0)
        {
            reserveIfMember (note, item.endTick);
            continue;
        }

        bool allocated = false;
        for (std::int16_t channel = config.firstMemberChannel;
             channel < static_cast<std::int16_t> (config.firstMemberChannel + config.memberChannelCount);
             ++channel)
        {
            const std::size_t index = static_cast<std::size_t> (channel);
            if (activeUntil[index] > item.startTick)
                continue;
            if (detail::mpeChannelReserved (channel, item.startTick, item.endTick, explicitReservations))
                continue;

            note.channel = channel;
            reserveIfMember (note, item.endTick);
            allocated = true;
            break;
        }

        if (! allocated)
        {
            result.status = MpeVoiceAllocationStatus::OutOfVoices;
            result.notesWritten = 0;
            return result;
        }
    }

    result.notesWritten = clip.notes.size();
    return result;
}

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
