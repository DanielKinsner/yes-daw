// Control owns the accepted clock and finite handoff PCM. Audio borrows this stable session
// through engine replacement; only Audio writes its one-shot completion and tail cursor.
#pragma once

#include "engine/MusicalClick.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace yesdaw::engine {

class PlaybackEngine;

class CapturedCountIn
{
public:
    CapturedCountIn (std::shared_ptr<const MusicalClickMap> clock, SampleRate rate,
                     std::int64_t opening, std::vector<float> tail)
        : clock_ (std::move (clock)), rate_ (rate), opening_ (opening), tail_ (std::move (tail)) {}

    [[nodiscard]] std::int64_t openingFrame() const noexcept { return opening_; }
    [[nodiscard]] bool completed() const noexcept { return publishedCompleted_.load (std::memory_order_acquire); }

private:
    friend class PlaybackEngine;
    const std::shared_ptr<const MusicalClickMap> clock_;
    const SampleRate rate_;
    const std::int64_t opening_;
    const std::vector<float> tail_;
    bool completedAudio_ = false;
    std::size_t consumedTail_ = 0;
    std::atomic<bool> publishedCompleted_ { false };
};

} // namespace yesdaw::engine
