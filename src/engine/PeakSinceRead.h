// YES DAW — a metered peak that misses no block (ADR-0067 §5).
//
// A meter is read once per UI tick, and many blocks run between two ticks; a source that published only "this block's
// peak" showed one block in several, so a peak or a clip in the others never reached the meter. PeakSinceRead publishes
// the max |sample| of every block processed since the reader last read it, and reads silence when no block ran (a
// stopped transport, a stalled device, an un-armed input) — real state, not a flag.
//
// Count-stamped windows. Every atomic has one writer: the block writer (the thread processing the block) owns `value`
// (the window's running max) and `count` (blocks so far); the reader (the UI tick, in exactly one place per source)
// stores its progress in `consumed`. The writer's next window starts after the block the reader last took; blocks that
// ran while the reader was between its loads and its store are carried in from the last four blocks' peaks, and a reader
// that fell further behind is given the whole old window again — an over-read for one tick, never a dropped block. No
// compare-exchange, no read-modify-write (ADR-0006): one acquire load, at most four compares and two release stores per
// block on the audio thread. Model-checked exhaustively before it landed (2.6 million interleavings, no block lost).
//
// Pure C++ — no JUCE — so RTSan / TSan cover it with the engine suites.

#pragma once

#include "rt/RtHot.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>

namespace yesdaw::engine {

class PeakSinceRead
{
public:
    static_assert (std::atomic<float>::is_always_lock_free);
    static_assert (std::atomic<std::uint64_t>::is_always_lock_free);

    // What one read found: the peak since the last read (0 when no block ran), and the block it reads through.
    struct Reading
    {
        float peak = 0.0f;
        std::uint64_t throughBlock = 0;
        bool fresh = false;   // at least one block ran since the last read
    };

    // WRITER — once per processed block, on the thread that processed it.
    void publishBlock (float blockPeak) noexcept YESDAW_RT_HOT
    {
        const std::uint64_t taken = consumed_.load (std::memory_order_acquire);
        if (taken != windowStart_)
        {
            // The reader took the window through block `taken`: start after it, carrying the blocks it could not see.
            windowStart_ = taken;
            if (blocks_ - taken <= kRecent)
            {
                float carried = 0.0f;
                for (std::uint64_t block = taken + 1; block <= blocks_; ++block)
                    carried = std::max (carried, recent_[block % kRecent]);
                runningMax_ = carried;
            }
            // else: the whole old window stays — counted again, never dropped.
        }
        ++blocks_;
        recent_[blocks_ % kRecent] = blockPeak;
        runningMax_ = std::max (runningMax_, blockPeak);
        value_.store (runningMax_, std::memory_order_release);
        count_.store (blocks_, std::memory_order_release);
    }

    // READER — once per UI tick, in one place per source. `between` runs between the loads and the store (the gates'
    // seam for forcing a writer into that gap; a no-op otherwise).
    template <typename Between>
    [[nodiscard]] Reading read (Between&& between) noexcept
    {
        const std::uint64_t through = count_.load (std::memory_order_acquire);
        const std::uint64_t last = consumed_.load (std::memory_order_relaxed);
        if (through == last)
            return { 0.0f, through, false };
        const float peak = value_.load (std::memory_order_acquire);
        between();
        consumed_.store (through, std::memory_order_release);
        return { peak, through, true };
    }
    [[nodiscard]] Reading read() noexcept { return read ([] {}); }

private:
    static constexpr std::uint64_t kRecent = 4;   // covers the reader's few-instruction gap

    std::atomic<float> value_ { 0.0f };
    std::atomic<std::uint64_t> count_ { 0 };
    std::atomic<std::uint64_t> consumed_ { 0 };   // the reader's (stored once per UI tick)

    // The writer's own.
    std::uint64_t blocks_ = 0;
    std::uint64_t windowStart_ = 0;
    float runningMax_ = 0.0f;
    std::array<float, kRecent> recent_ {};
};

} // namespace yesdaw::engine
