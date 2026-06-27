// YES DAW - MidiClip event source node.
//
// The MIDI analogue of DecodedClipNode: a MidiClip is flattened into a sorted absolute-frame Event
// timeline ON THE CONTROL SIDE (allocation is fine there), then this node streams it into the graph one
// Block at a time by advancing a per-source cursor (ADR-0009 per-source monotonic read cursors). The
// audio-thread process() path allocates nothing and never flattens — it copies this Block's slice into the
// graph's writable Event slot via EventStream::replaceEvents.

#pragma once

#include "engine/Midi.h"
#include "engine/Node.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace yesdaw::engine {

class DecodedMidiClipNode final : public Node
{
public:
    // Hard ceiling on Events emitted per Block; matches the graph's per-slot Event capacity so a copy into
    // the slot can never overflow. Excess Events in a single Block are dropped (bounded, never UB).
    static constexpr std::size_t kMaxEventsPerBlock = 1024;

    DecodedMidiClipNode (NodeId id, std::vector<ScheduledMidiEvent> timeline) noexcept
        : id_ (id), timeline_ (std::move (timeline))
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ false, /*producesEvents*/ true,
                                /*channels*/ 1, /*latencySamples*/ 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }

    void prepare (double, int) override
    {
        cursorFrame_ = 0;
        nextIndex_   = 0;
        scratch_.assign (kMaxEventsPerBlock, Event {});
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        silenceAudio (args);

        if (args.numFrames <= 0)
        {
            (void) args.events.replaceEvents (std::span<const Event> {});
            return;
        }

        const std::int64_t blockStart = cursorFrame_;
        const std::int64_t blockEnd   = blockStart + static_cast<std::int64_t> (args.numFrames);

        std::size_t written = 0;
        while (nextIndex_ < timeline_.size() && timeline_[nextIndex_].frame < blockEnd)
        {
            const ScheduledMidiEvent& scheduled = timeline_[nextIndex_];
            if (scheduled.frame >= blockStart && written < scratch_.size())
            {
                Event event = scheduled.event;
                event.timeInBlock = static_cast<std::uint32_t> (scheduled.frame - blockStart);
                scratch_[written++] = event;
            }
            ++nextIndex_;
        }

        (void) args.events.replaceEvents (std::span<const Event> (scratch_.data(), written));
        cursorFrame_ = blockEnd;
    }

    void reset() noexcept override
    {
        cursorFrame_ = 0;
        nextIndex_   = 0;
    }

    void release() override
    {
        timeline_.clear();
        timeline_.shrink_to_fit();
        scratch_.clear();
        scratch_.shrink_to_fit();
    }

    [[nodiscard]] std::int64_t cursorFrame() const noexcept { return cursorFrame_; }
    [[nodiscard]] std::size_t eventCount() const noexcept { return timeline_.size(); }

private:
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
    std::vector<ScheduledMidiEvent> timeline_;       // sorted absolute-frame timeline (control-side built)
    std::vector<Event>              scratch_;        // pre-sized per-Block emit buffer (prepare() owns it)
    std::int64_t                    cursorFrame_ = 0;
    std::size_t                     nextIndex_   = 0;
};

} // namespace yesdaw::engine
