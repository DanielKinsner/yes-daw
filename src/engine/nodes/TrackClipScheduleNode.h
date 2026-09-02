// YES DAW - one source Node per Track playing an atomically swappable ClipSchedule (G0.5).
//
// Replaces the per-Clip DecodedClipNode fan-in: the Track's audio Clips are one immutable
// ClipSchedule the audio thread reads through a single acquire-load. A placement edit publishes a
// NEW schedule through the Runtime command queue (ordered with graph swaps, applied by the audio
// thread), and the previous schedule is retired to the janitor — never freed under a running block.
#pragma once

#include "engine/ClipSchedule.h"
#include "engine/Node.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace yesdaw::engine {

class TrackClipScheduleNode final : public Node
{
public:
    // `channels` is the strip's width (ADR-0042). The node OWNS its current schedule; a schedule it
    // hands back from exchangeSchedule() is owned by whoever retires it (the Runtime).
    TrackClipScheduleNode (NodeId id, int channels, std::unique_ptr<const ClipSchedule> initial) noexcept
        : id_ (id),
          channels_ (channels > 0 ? channels : 1),
          schedule_ (initial.release())
    {
    }

    ~TrackClipScheduleNode() override
    {
        delete schedule_.load (std::memory_order_acquire);
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                channels_, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ true };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }

    void prepare (double, int maxBlockSize) override
    {
        maxBlock_ = maxBlockSize > 0 ? maxBlockSize : 1;
        accum_.assign (static_cast<std::size_t> (channels_) * static_cast<std::size_t> (maxBlock_), 0.0);
        playFrame_ = 0;
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels < 1)
            return;

        const int channels = std::min (args.audio.numChannels, channels_);
        const int frames = std::min (args.numFrames, maxBlock_);
        const std::int64_t blockStart = args.transport.hasTimelineFrame ? args.transport.timelineFrame : playFrame_;

        // G3.2: a stopped transport carrying live notes - silence out, the play cursor holds.
        if (args.transport.clipsSilenced)
        {
            for (int c = 0; c < channels; ++c)
            {
                float* const out = args.audio.channels[c];
                for (int i = 0; i < frames; ++i)
                    out[i] = 0.0f;
            }
            return;
        }

        double* const accum = accum_.data();
        for (int c = 0; c < channels; ++c)
        {
            double* const a = accum + static_cast<std::size_t> (c) * static_cast<std::size_t> (maxBlock_);
            for (int i = 0; i < frames; ++i)
                a[i] = 0.0;
        }

        if (const ClipSchedule* const schedule = schedule_.load (std::memory_order_acquire))
            accumulateClipSchedule (*schedule, blockStart, frames, channels, maxBlock_, accum);

        for (int c = 0; c < channels; ++c)
        {
            const double* const a = accum + static_cast<std::size_t> (c) * static_cast<std::size_t> (maxBlock_);
            float* const out = args.audio.channels[c];
            for (int i = 0; i < frames; ++i)
                out[i] = static_cast<float> (a[i]);
        }

        if (! args.transport.hasTimelineFrame)
            playFrame_ += static_cast<std::int64_t> (args.numFrames);
    }

    void reset() noexcept override { playFrame_ = 0; }
    void release() override { accum_.clear(); accum_.shrink_to_fit(); }

    // AUDIO THREAD (via the Runtime command lane): install `next`, hand back the previous schedule
    // for retirement. Never frees anything.
    [[nodiscard]] const ClipSchedule* exchangeSchedule (const ClipSchedule* next) noexcept YESDAW_RT_HOT
    {
        return schedule_.exchange (next, std::memory_order_acq_rel);
    }

    // CONTROL THREAD (tests / diagnostics): the schedule currently installed.
    [[nodiscard]] const ClipSchedule* currentSchedule() const noexcept
    {
        return schedule_.load (std::memory_order_acquire);
    }

    [[nodiscard]] int channels() const noexcept { return channels_; }

private:
    NodeId                           id_ = 0;
    int                              channels_ = 1;
    int                              maxBlock_ = 1;
    std::atomic<const ClipSchedule*> schedule_ { nullptr };
    std::vector<double>              accum_;
    std::int64_t                     playFrame_ = 0;
};

static_assert (std::atomic<const ClipSchedule*>::is_always_lock_free,
               "the schedule pointer must stay lock-free on the audio thread");

} // namespace yesdaw::engine
