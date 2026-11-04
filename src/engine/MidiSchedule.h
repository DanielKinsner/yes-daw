// Construction and indexing run on Control; Audio only reads bounded immutable lookups.
#pragma once

#include "engine/Midi.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace yesdaw::engine {

inline constexpr std::size_t kMidiScheduleEventCapacity = 1024;
inline constexpr std::size_t kNoMidiSpan = std::numeric_limits<std::size_t>::max();

[[nodiscard]] constexpr bool midiVoiceAddressesEqual (const VoiceAddress& a,
                                                      const VoiceAddress& b) noexcept
{
    return a.noteId == b.noteId && a.portIndex == b.portIndex
        && a.channel == b.channel && a.key == b.key;
}

struct MidiNoteSpan
{
    EntityId noteId;
    std::uint32_t occurrence = 0;
    std::int64_t startFrame = 0;
    std::int64_t endFrame = 0;
    Event onset {};
};

struct MidiSchedule
{
    EntityId clipId;
    EntityId trackId;
    std::vector<ScheduledMidiEvent> timeline;
    // Sorted by full EntityId + occurrence, never by the lossy wire noteId.
    std::vector<MidiNoteSpan> spans;
    std::vector<std::size_t> eventToSpan;
    std::size_t noteOns = 0;
    bool liveEligible = false;

    [[nodiscard]] const MidiNoteSpan* findSpan (EntityId noteId,
                                               std::uint32_t occurrence) const noexcept YESDAW_RT_HOT
    {
        std::size_t first = 0;
        std::size_t count = spans.size();
        while (count > 0)
        {
            const std::size_t step = count / 2u;
            const std::size_t mid = first + step;
            const MidiNoteSpan& span = spans[mid];
            if (span.noteId < noteId || (span.noteId == noteId && span.occurrence < occurrence))
            {
                first = mid + 1u;
                count -= step + 1u;
            }
            else
                count = step;
        }
        return first < spans.size() && spans[first].noteId == noteId
            && spans[first].occurrence == occurrence ? &spans[first] : nullptr;
    }

    [[nodiscard]] std::size_t firstEventAtOrAfter (std::int64_t frame) const noexcept YESDAW_RT_HOT
    {
        const auto found = std::lower_bound (timeline.begin(), timeline.end(), frame,
            [] (const ScheduledMidiEvent& event, std::int64_t value) { return event.frame < value; });
        return static_cast<std::size_t> (found - timeline.begin());
    }
};

struct MidiScheduleBuildResult
{
    MidiFlattenStatus status = MidiFlattenStatus::InvalidInput;
    std::unique_ptr<const MidiSchedule> schedule;

    [[nodiscard]] bool ok() const noexcept
    {
        return status == MidiFlattenStatus::Ok && schedule != nullptr;
    }
};

[[nodiscard]] inline MidiScheduleBuildResult buildMidiSchedule (const MidiClip& clip,
                                                                TempoMapView tempoMap,
                                                                SampleRate sampleRate)
{
    MidiScheduleBuildResult result;
    auto schedule = std::make_unique<MidiSchedule>();
    schedule->clipId = clip.id;
    schedule->trackId = clip.trackId;

    CompiledTempoMap compiled;
    if (clip.timeBase != TimeBase::SampleLocked
        && ! CompiledTempoMap::build (tempoMap, sampleRate, compiled))
        return result;

    const auto frameForTick = [&] (Tick tick, double& frame) noexcept
    {
        if (clip.timeBase == TimeBase::SampleLocked)
        {
            frame = static_cast<double> (tick);
            return true;
        }
        return compiled.frameForTick (tick, frame);
    };
    result.status = flattenMidiClipToTimeline (clip, frameForTick, schedule->timeline);
    if (result.status != MidiFlattenStatus::Ok)
        return result;

    for (const ScheduledMidiEvent& event : schedule->timeline)
        if (event.event.type == EventType::NoteOn)
            ++schedule->noteOns;
    schedule->liveEligible = clip.loopLengthTicks == 0
        && schedule->timeline.size() <= kMidiScheduleEventCapacity
        && schedule->noteOns <= kMidiScheduleEventCapacity - schedule->timeline.size();

    if (schedule->liveEligible)
    {
        schedule->eventToSpan.assign (schedule->timeline.size(), kNoMidiSpan);
        MidiClip single = clip;
        single.notes.clear();
        single.controlEvents.clear();
        single.notes.reserve (1);
        std::vector<ScheduledMidiEvent> noteTimeline;
        for (const Note& note : clip.notes)
        {
            single.notes.assign (1, note);
            if (flattenMidiClipToTimeline (single, frameForTick, noteTimeline) != MidiFlattenStatus::Ok)
            {
                result.status = MidiFlattenStatus::InvalidInput;
                return result;
            }
            if (noteTimeline.empty())
                continue;
            if (noteTimeline.size() != 2u)
            {
                schedule->liveEligible = false;
                continue;
            }
            schedule->spans.push_back ({ note.id, 0, noteTimeline[0].frame,
                                        noteTimeline[1].frame, noteTimeline[0].event });
        }
        std::sort (schedule->spans.begin(), schedule->spans.end(),
                   [] (const MidiNoteSpan& a, const MidiNoteSpan& b)
                   {
                       return a.noteId < b.noteId
                           || (a.noteId == b.noteId && a.occurrence < b.occurrence);
                   });

        std::vector<std::pair<std::int32_t, std::size_t>> wireIndex;
        wireIndex.reserve (schedule->spans.size());
        for (std::size_t i = 0; i < schedule->spans.size(); ++i)
            wireIndex.emplace_back (schedule->spans[i].onset.voice.noteId, i);
        std::sort (wireIndex.begin(), wireIndex.end());
        for (std::size_t i = 1; i < wireIndex.size(); ++i)
            if (wireIndex[i - 1u].first == wireIndex[i].first)
                schedule->liveEligible = false;

        for (std::size_t i = 0; i < schedule->timeline.size(); ++i)
        {
            const ScheduledMidiEvent& event = schedule->timeline[i];
            if (event.event.type != EventType::NoteOn && event.event.type != EventType::NoteOff)
                continue;
            const auto found = std::lower_bound (wireIndex.begin(), wireIndex.end(), event.event.voice.noteId,
                [] (const auto& item, std::int32_t wireId) { return item.first < wireId; });
            if (found == wireIndex.end() || found->first != event.event.voice.noteId)
            {
                schedule->liveEligible = false;
                continue;
            }
            const MidiNoteSpan& span = schedule->spans[found->second];
            if (! midiVoiceAddressesEqual (span.onset.voice, event.event.voice)
                || event.frame != (event.event.type == EventType::NoteOn ? span.startFrame : span.endFrame))
            {
                schedule->liveEligible = false;
                continue;
            }
            schedule->eventToSpan[i] = found->second;
        }
    }
    result.schedule = std::move (schedule);
    return result;
}

struct MidiTrackBounds
{
    std::size_t ordinaryEvents = 0;
    std::size_t noteOns = 0;
    bool liveEligible = false;

    [[nodiscard]] bool fits (std::size_t retainedNoteOnHighWater) const noexcept
    {
        return liveEligible && ordinaryEvents <= kMidiScheduleEventCapacity
            && retainedNoteOnHighWater <= kMidiScheduleEventCapacity - ordinaryEvents;
    }
};

// CONTROL only. R is owned by the model and never shrinks until its engine is rebuilt.
[[nodiscard]] inline MidiTrackBounds midiTrackBounds (const Project& project, EntityId trackId)
{
    MidiTrackBounds bounds;
    const Track* const track = project.findTrack (trackId);
    if (track == nullptr)
        return bounds;
    bounds.liveEligible = true;
    for (const FxInsert& insert : track->strip.fxChain)
        if (insert.enabled && fxKindIsMidi (insert.kind))
            bounds.liveEligible = false;

    std::vector<std::int32_t> wireIds;
    for (const MidiClip& clip : project.midiClips)
    {
        if (clip.trackId != trackId)
            continue;
        MidiScheduleBuildResult built = buildMidiSchedule (clip,
            TempoMapView { project.tempoMap.data(), project.tempoMap.size() }, project.sampleRate);
        if (! built.ok())
        {
            bounds.liveEligible = false;
            continue;
        }
        const MidiSchedule& schedule = *built.schedule;
        bounds.liveEligible = bounds.liveEligible && schedule.liveEligible;
        const auto addBound = [&bounds] (std::size_t& total, std::size_t value)
        {
            if (value > std::numeric_limits<std::size_t>::max() - total)
            {
                bounds.liveEligible = false;
                total = std::numeric_limits<std::size_t>::max();
            }
            else
                total += value;
        };
        addBound (bounds.ordinaryEvents, schedule.timeline.size());
        addBound (bounds.noteOns, schedule.noteOns);
        for (const MidiNoteSpan& span : schedule.spans)
            wireIds.push_back (span.onset.voice.noteId);
    }
    std::sort (wireIds.begin(), wireIds.end());
    for (std::size_t i = 1; i < wireIds.size(); ++i)
        if (wireIds[i - 1u] == wireIds[i])
            bounds.liveEligible = false;
    return bounds;
}

}
