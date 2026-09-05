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

// G3.3: the render-side wire form of a control event - the edit model's normalized value becomes
// the MIDI 1.0 message's 7-bit (14-bit for a bend) data. This is the ONE law that quantizes.
[[nodiscard]] inline std::uint8_t midiSevenBit (double normalized) noexcept
{
    const double clamped = normalized < 0.0 ? 0.0 : (normalized > 1.0 ? 1.0 : normalized);
    return static_cast<std::uint8_t> (std::lround (clamped * 127.0));
}

[[nodiscard]] inline std::uint16_t midiFourteenBitBend (double bend) noexcept
{
    const double clamped = bend < -1.0 ? -1.0 : (bend > 1.0 ? 1.0 : bend);
    const long value = std::lround ((clamped + 1.0) * 0.5 * 16383.0);
    return static_cast<std::uint16_t> (value < 0 ? 0 : (value > 16383 ? 16383 : value));
}

[[nodiscard]] inline Event makeMidiControlEvent (std::uint32_t timeInBlock,
                                                 const MidiControlEvent& control) noexcept
{
    const std::uint8_t channelNibble = static_cast<std::uint8_t> (control.channel >= 0 ? (control.channel & 0x0F) : 0);
    std::uint8_t status = 0;
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;
    switch (control.kind)
    {
        case MidiControlKind::ControlChange:
            status = kMidiStatusControlChange;
            data1 = static_cast<std::uint8_t> (control.number & 0x7F);
            data2 = midiSevenBit (control.value);
            break;
        case MidiControlKind::PitchBend:
        {
            status = kMidiStatusPitchBend;
            const std::uint16_t bend = midiFourteenBitBend (control.value);
            data1 = static_cast<std::uint8_t> (bend & 0x7F);
            data2 = static_cast<std::uint8_t> ((bend >> 7) & 0x7F);
            break;
        }
        case MidiControlKind::ChannelPressure:
            status = kMidiStatusChannelPressure;
            data1 = midiSevenBit (control.value);
            break;
        case MidiControlKind::PolyPressure:
            status = kMidiStatusPolyPressure;
            data1 = static_cast<std::uint8_t> (control.number & 0x7F);
            data2 = midiSevenBit (control.value);
            break;
        case MidiControlKind::ProgramChange:
            status = kMidiStatusProgramChange;
            data1 = static_cast<std::uint8_t> (control.number & 0x7F);
            break;
    }

    Event event = makeMidi1Event (timeInBlock,
                                  static_cast<std::uint8_t> (status | channelNibble),
                                  data1, data2, control.portIndex, control.channel);
    if (control.kind == MidiControlKind::PolyPressure)
        event.voice.key = control.number;
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

            // Half-open [startFrame, blockEnd): a frame landing exactly on blockEnd belongs to the NEXT
            // Block (ADR-0017). This is the single load-bearing boundary check — once it passes, offset is
            // necessarily in [0, numFrames), so the previously-present second guard was redundant and
            // (per the H4 review) masked regressions in this very check. Keep exactly one.
            if (eventFrame < block.startFrame || eventFrame >= blockEnd)
                return true;

            const double offset = eventFrame - block.startFrame;

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

// A render Event positioned at an absolute project frame. The control side pre-flattens a whole MidiClip
// into a sorted timeline of these; the audio thread (DecodedMidiClipNode) advances a per-source cursor and
// emits each Block's slice with Block-relative timeInBlock — the same cursor model DecodedClipNode uses for
// audio (ADR-0009 per-source monotonic read cursors).
struct ScheduledMidiEvent
{
    std::int64_t frame = 0;   // absolute project frame (floor of the tempo-mapped position)
    Event        event {};    // event.timeInBlock is a placeholder; set per Block at emit time
};

[[nodiscard]] inline bool scheduledMidiEventLess (const ScheduledMidiEvent& a,
                                                  const ScheduledMidiEvent& b) noexcept
{
    if (a.frame != b.frame)
        return a.frame < b.frame;
    // G3.3: at one frame a control message (Midi1) precedes every note event - a program change or
    // a sustain pedal at a note's tick is in force when the note sounds. Control ties order by their
    // wire bytes, so the flatten is deterministic whatever the edit order.
    const bool aControl = a.event.type == EventType::Midi1;
    const bool bControl = b.event.type == EventType::Midi1;
    if (aControl != bControl)
        return aControl;
    if (aControl)
    {
        const Midi1Payload& x = a.event.payload.midi1;
        const Midi1Payload& y = b.event.payload.midi1;
        if (x.status != y.status)
            return x.status < y.status;
        if (x.data1 != y.data1)
            return x.data1 < y.data1;
        return x.data2 < y.data2;
    }
    if (a.event.voice.noteId != b.event.voice.noteId)
        return a.event.voice.noteId < b.event.voice.noteId;
    // Same frame + same note: On (1) before Off (2) so a zero-length Note never leaves a hung voice.
    return static_cast<std::uint16_t> (a.event.type) < static_cast<std::uint16_t> (b.event.type);
}

// Control-side: flatten every Note of a MidiClip into a sorted absolute-frame timeline (allocation is fine
// here — this runs off the audio thread, exactly like DecodedClipNode's decode step). The audio thread
// never calls this; it consumes the produced timeline by cursor.
template <typename TickToFrame>
[[nodiscard]] inline MidiFlattenStatus flattenMidiClipToTimeline (const MidiClip& clip,
                                                                  TickToFrame tickToFrameFn,
                                                                  std::vector<ScheduledMidiEvent>& outTimeline)
{
    outTimeline.clear();

    if (! clip.isValid())
        return MidiFlattenStatus::InvalidInput;

    for (const Note& note : clip.notes)
    {
        if (! note.isValid())
            return MidiFlattenStatus::InvalidInput;

        Tick noteEnd = 0;
        Tick onTick = 0;
        Tick offTick = 0;
        if (! detail::addMidiTickChecked (note.startTick, note.lengthTicks, noteEnd)
            || noteEnd > clip.timelineLength
            || ! detail::addMidiTickChecked (clip.timelineStart, note.startTick, onTick)
            || ! detail::addMidiTickChecked (clip.timelineStart, noteEnd, offTick))
            return MidiFlattenStatus::InvalidInput;

        const auto append = [&] (Tick tick, EventType type) -> bool
        {
            double frame = 0.0;
            if (! tickToFrameFn (tick, frame) || ! std::isfinite (frame) || frame < 0.0)
                return false;

            outTimeline.push_back (ScheduledMidiEvent {
                static_cast<std::int64_t> (std::floor (frame)),
                makeNoteEvent (0u, type, note) });
            return true;
        };

        if (! append (onTick, EventType::NoteOn) || ! append (offTick, EventType::NoteOff))
            return MidiFlattenStatus::InvalidInput;
    }

    // G3.3: the Clip's control events ride the same timeline as its notes.
    for (const MidiControlEvent& control : clip.controlEvents)
    {
        Tick absoluteTick = 0;
        if (! control.isValid() || control.tick > clip.timelineLength
            || ! detail::addMidiTickChecked (clip.timelineStart, control.tick, absoluteTick))
            return MidiFlattenStatus::InvalidInput;

        double frame = 0.0;
        if (! tickToFrameFn (absoluteTick, frame) || ! std::isfinite (frame) || frame < 0.0)
            return MidiFlattenStatus::InvalidInput;

        outTimeline.push_back (ScheduledMidiEvent {
            static_cast<std::int64_t> (std::floor (frame)),
            makeMidiControlEvent (0u, control) });
    }

    std::sort (outTimeline.begin(), outTimeline.end(), scheduledMidiEventLess);
    return MidiFlattenStatus::Ok;
}

[[nodiscard]] inline MidiFlattenStatus flattenMidiClipToTimeline (const MidiClip& clip,
                                                                  const CompiledTempoMap& tempoMap,
                                                                  std::vector<ScheduledMidiEvent>& outTimeline)
{
    return flattenMidiClipToTimeline (
        clip,
        [&tempoMap] (Tick tick, double& frame) noexcept
        {
            return tempoMap.frameForTick (tick, frame);
        },
        outTimeline);
}

[[nodiscard]] inline MidiFlattenStatus flattenMidiClipToTimeline (const MidiClip& clip,
                                                                  TempoMapView tempoMap,
                                                                  SampleRate sampleRate,
                                                                  std::vector<ScheduledMidiEvent>& outTimeline)
{
    // Build the ADR-0010 prefix-sum lookup ONCE, then resolve each Note's start/end in O(log n) — instead
    // of the per-Note O(n) scan + revalidation the raw tickToFrame would do across the whole clip.
    CompiledTempoMap compiled;
    if (! CompiledTempoMap::build (tempoMap, sampleRate, compiled))
    {
        outTimeline.clear();
        return MidiFlattenStatus::InvalidInput;
    }

    return flattenMidiClipToTimeline (clip, compiled, outTimeline);
}

// The Project projection's entry point: pick the flattening law from the Clip's TimeBase.
// SampleLocked Clips address frames directly; TempoLocked Clips resolve through the tempo map.
// (Lived in OfflineRenderer's detail namespace until M1 gave the mixer projection its own MIDI
// path — it belongs with its siblings so both callers share one law.)
[[nodiscard]] inline MidiFlattenStatus flattenMidiClipForProjection (const MidiClip& clip,
                                                                     TempoMapView tempoMap,
                                                                     SampleRate sampleRate,
                                                                     std::vector<ScheduledMidiEvent>& outTimeline)
{
    if (clip.timeBase == TimeBase::SampleLocked)
    {
        return flattenMidiClipToTimeline (
            clip,
            [] (Tick tick, double& frame) noexcept
            {
                if (tick < 0)
                    return false;
                frame = static_cast<double> (tick);
                return std::isfinite (frame);
            },
            outTimeline);
    }

    if (clip.timeBase == TimeBase::TempoLocked)
        return flattenMidiClipToTimeline (clip, tempoMap, sampleRate, outTimeline);

    outTimeline.clear();
    return MidiFlattenStatus::InvalidInput;
}

} // namespace yesdaw::engine
