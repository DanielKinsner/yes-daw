// YES DAW - MidiClip event source node.
//
// The MIDI analogue of DecodedClipNode: a MidiClip is flattened into a sorted absolute-frame Event
// timeline ON THE CONTROL SIDE (allocation is fine there), then this node streams it into the graph one
// Block at a time by advancing a per-source cursor (ADR-0009 per-source monotonic read cursors). The
// audio-thread process() path allocates nothing and never flattens — it copies this Block's slice into the
// graph's writable Event slot via EventStream::replaceEvents.

#pragma once

#include "engine/MidiSchedule.h"
#include "engine/Node.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace yesdaw::engine {

class DecodedMidiClipNode final : public Node
{
public:
    // Hard ceiling on Events emitted per Block; matches the graph's per-slot Event capacity so a copy into
    // the slot can never overflow. Excess Events in a single Block are dropped (bounded, never UB).
    static constexpr std::size_t kMaxEventsPerBlock = kMidiScheduleEventCapacity;

    DecodedMidiClipNode (NodeId id, std::vector<ScheduledMidiEvent> timeline)
        : id_ (id)
    {
        auto schedule = std::make_unique<MidiSchedule>();
        schedule->timeline = std::move (timeline);
        schedule_ = schedule.release();
    }

    DecodedMidiClipNode (NodeId id, std::unique_ptr<const MidiSchedule> schedule,
                         bool allowLiveTracking = true) noexcept
        : id_ (id), schedule_ (schedule.release()),
          liveTrackingAllowed_ (allowLiveTracking && schedule_ != nullptr && schedule_->liveEligible) {}

    ~DecodedMidiClipNode() override { delete schedule_; }
    DecodedMidiClipNode (const DecodedMidiClipNode&) = delete;
    DecodedMidiClipNode& operator= (const DecodedMidiClipNode&) = delete;

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ false, /*producesEvents*/ true,
                                /*channels*/ 1, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ true };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }

    void prepare (double, int) override
    {
        cursorFrame_ = 0;
        activeCount_ = 0;
        hasRendered_ = false;
        reconcilePending_ = false;
        discontinuityPending_ = false;
        emittedReconciliation_ = false;
        scratch_.assign (kMaxEventsPerBlock, Event {});
        active_.assign (liveTrackingAllowed_ ? kMaxEventsPerBlock : 0,
                        MidiNoteSpan {});
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        silenceAudio (args);
        emittedReconciliation_ = false;
        if (args.numFrames <= 0 || schedule_ == nullptr)
        {
            (void) args.events.replaceEvents (std::span<const Event> {});
            return;
        }

        const std::int64_t blockStart = args.transport.hasTimelineFrame ? args.transport.timelineFrame : cursorFrame_;
        const std::int64_t blockEnd = blockStart > std::numeric_limits<std::int64_t>::max() - args.numFrames
            ? std::numeric_limits<std::int64_t>::max() : blockStart + args.numFrames;
        const bool discontinuous = discontinuityPending_ || args.transport.clipsSilenced
            || (hasRendered_ && blockStart != cursorFrame_);
        std::size_t written = 0;

        // At offset zero, unchanged controllers precede addressed releases. Computing both
        // ends up front also makes dense unsupported overflow O(log E + capacity), not O(E).
        std::size_t next = schedule_->firstEventAtOrAfter (blockStart);
        const std::size_t end = schedule_->firstEventAtOrAfter (blockEnd);
        if (! args.transport.clipsSilenced)
            while (next < end && schedule_->timeline[next].frame == blockStart
                   && schedule_->timeline[next].event.type == EventType::Midi1 && written < scratch_.size())
                emitOrdinary (next++, blockStart, written);

        if (discontinuous || reconcilePending_)
        {
            std::size_t keep = 0;
            for (std::size_t i = 0; i < activeCount_; ++i)
            {
                const MidiNoteSpan& active = active_[i];
                const MidiNoteSpan* const replacement = discontinuous ? nullptr
                    : schedule_->findSpan (active.noteId, active.occurrence);
                if (replacement != nullptr && replacement->startFrame <= blockStart && blockStart < replacement->endFrame
                    && midiVoiceAddressesEqual (replacement->onset.voice, active.onset.voice)
                    && replacement->onset.payload.note.pitchNote == active.onset.payload.note.pitchNote
                    && replacement->onset.payload.note.normalizedVelocity == active.onset.payload.note.normalizedVelocity)
                    active_[keep++] = *replacement;
                else if (written < scratch_.size())
                {
                    Event off = active.onset;
                    off.type = EventType::NoteOff;
                    off.timeInBlock = 0;
                    off.payload.note.normalizedVelocity = 0.0;
                    scratch_[written++] = off;
                    emittedReconciliation_ = true;
                }
            }
            activeCount_ = keep;
            reconcilePending_ = false;
            discontinuityPending_ = false;
        }

        if (! args.transport.clipsSilenced)
        {
            while (next < end && written < scratch_.size())
                emitOrdinary (next++, blockStart, written);
            cursorFrame_ = blockEnd;
            hasRendered_ = true;
        }
        (void) args.events.replaceEvents (std::span<const Event> (scratch_.data(), written));
    }

    void reset() noexcept override
    {
        cursorFrame_ = 0;
        discontinuityPending_ = true;
    }

    void release() override
    {
        delete schedule_;
        schedule_ = nullptr;
        scratch_.clear();
        scratch_.shrink_to_fit();
        active_.clear();
        active_.shrink_to_fit();
        activeCount_ = 0;
    }

    [[nodiscard]] std::int64_t cursorFrame() const noexcept { return cursorFrame_; }
    [[nodiscard]] std::size_t eventCount() const noexcept { return schedule_ == nullptr ? 0 : schedule_->timeline.size(); }

    // AUDIO only. Ownership of previous passes to Runtime; no active state borrows from it.
    [[nodiscard]] bool exchangeSchedule (const MidiSchedule* next,
                                          const MidiSchedule*& previous) noexcept YESDAW_RT_HOT
    {
        if (next == nullptr || schedule_ == nullptr || ! liveTrackingAllowed_ || ! next->liveEligible
            || next->clipId != schedule_->clipId || next->trackId != schedule_->trackId)
            return false;
        previous = schedule_;
        schedule_ = next;
        reconcilePending_ = true;
        return true;
    }

    void markDiscontinuity() noexcept YESDAW_RT_HOT { discontinuityPending_ = true; }
    [[nodiscard]] bool emittedReconciliation() const noexcept YESDAW_RT_HOT { return emittedReconciliation_; }

private:
    [[nodiscard]] std::size_t activeIndex (const MidiNoteSpan& span) const noexcept YESDAW_RT_HOT
    {
        for (std::size_t i = 0; i < activeCount_; ++i)
            if (active_[i].noteId == span.noteId && active_[i].occurrence == span.occurrence)
                return i;
        return kNoMidiSpan;
    }

    void emitOrdinary (std::size_t index, std::int64_t blockStart,
                       std::size_t& written) noexcept YESDAW_RT_HOT
    {
        const ScheduledMidiEvent& scheduled = schedule_->timeline[index];
        const std::size_t spanIndex = liveTrackingAllowed_ ? schedule_->eventToSpan[index] : kNoMidiSpan;
        if (spanIndex != kNoMidiSpan)
        {
            const MidiNoteSpan& span = schedule_->spans[spanIndex];
            const std::size_t active = activeIndex (span);
            if (scheduled.event.type == EventType::NoteOn)
            {
                if (active != kNoMidiSpan)
                    return;
                if (activeCount_ < active_.size())
                    active_[activeCount_++] = span;
            }
            else if (scheduled.event.type == EventType::NoteOff && active != kNoMidiSpan)
                active_[active] = active_[--activeCount_];
        }
        Event event = scheduled.event;
        event.timeInBlock = static_cast<std::uint32_t> (scheduled.frame - blockStart);
        scratch_[written++] = event;
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

    NodeId                          id_ = 0;
    const MidiSchedule*            schedule_ = nullptr; // sole owner until exchanged into retirement
    std::vector<Event>              scratch_;        // pre-sized per-Block emit buffer (prepare() owns it)
    std::vector<MidiNoteSpan>        active_;         // fixed-size storage; copied identities, no borrowed spans
    std::size_t                     activeCount_ = 0;
    std::int64_t                    cursorFrame_ = 0;
    bool                            hasRendered_ = false;
    bool                            reconcilePending_ = false;
    bool                            discontinuityPending_ = false;
    bool                            emittedReconciliation_ = false;
    bool                            liveTrackingAllowed_ = false; // initial whole-Track certificate, never widened on Audio
};

} // namespace yesdaw::engine
