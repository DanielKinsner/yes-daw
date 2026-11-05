#pragma once

#include "ui/TimelineCoordinates.h"

namespace yesdaw::ui {

enum class MidiCreationStatus { Ok, Unsupported, InvalidInput, InvalidClock, Overflow, Precision, CollapsedClipWindow };

struct MidiCreationSpan
{
    engine::Tick start = 0, end = 0;
    [[nodiscard]] engine::Tick length() const noexcept { return end - start; }
    friend bool operator== (const MidiCreationSpan&, const MidiCreationSpan&) = default;
};

// Control-side geometry only: no identities, Project edits or file I/O. Capture this context
// when placement is eligible, so deferred imports retain their physical Frame on the current map.
class MidiCreationContext
{
public:
    [[nodiscard]] static MidiCreationStatus prepare (engine::TempoMapView tempo, engine::MeterMapView meter,
        engine::SampleRate rate, std::int64_t insertionFrame, engine::TimeBase timeBase, MidiCreationContext& out)
    {
        out = {};
        if (timeBase != engine::TimeBase::TempoLocked && timeBase != engine::TimeBase::SampleLocked)
            return MidiCreationStatus::Unsupported;
        if (insertionFrame < 0) return MidiCreationStatus::InvalidInput;
        auto clock = std::make_shared<engine::MusicalClickMap>();
        auto result = status (engine::MusicalClickMap::build (tempo, meter, rate, *clock));
        if (result != MidiCreationStatus::Ok) return result;
        engine::RationalTick first;
        result = status (clock->meterMap().barStart (1, first));
        if (result != MidiCreationStatus::Ok) return result;
        engine::Tick start = 0;
        result = status (clock->tickAtFramePosition (
            engine::click_detail::integer (static_cast<std::uint64_t> (insertionFrame)), start));
        if (result != MidiCreationStatus::Ok) return result;
        out.clock_ = std::move (clock);
        out.frame_ = insertionFrame;
        out.tick_ = start;
        out.timeBase_ = timeBase;
        if (timeBase == engine::TimeBase::SampleLocked)
            out.fixedBaseStatus_ = out.nearestFrame ({ start, 0, 1 }, out.fixedBaseFrame_);
        return MidiCreationStatus::Ok;
    }

    [[nodiscard]] bool ready() const noexcept { return clock_ != nullptr; }
    [[nodiscard]] std::int64_t insertionFrame() const noexcept { return frame_; }
    [[nodiscard]] engine::Tick insertionTick() const noexcept { return tick_; }
    [[nodiscard]] engine::TimeBase timeBase() const noexcept { return timeBase_; }

    // Source PPQ coordinates are absolute endpoints, never separately rounded durations.
    [[nodiscard]] MidiCreationStatus canonicalSourceEndpoint (std::uint32_t sourceTick,
        std::uint16_t sourcePpq, engine::Tick& out) const noexcept
    {
        out = 0;
        if (! ready()) return MidiCreationStatus::InvalidClock;
        if (sourcePpq == 0) return MidiCreationStatus::InvalidInput;
        if (sourcePpq >= 0x8000u) return MidiCreationStatus::Unsupported; // SMPTE division is not PPQ.
        const std::uint64_t product = std::uint64_t { sourceTick } * engine::kTicksPerQuarter;
        const auto quotient = product / sourcePpq, remainder = product % sourcePpq;
        const auto offset = quotient + (remainder >= sourcePpq - remainder ? 1u : 0u);
        if (offset > static_cast<std::uint64_t> (INT64_MAX - tick_)) return MidiCreationStatus::Overflow;
        out = tick_ + static_cast<engine::Tick> (offset);
        return MidiCreationStatus::Ok;
    }

    [[nodiscard]] MidiCreationStatus nativeEndpoint (engine::Tick canonicalAbsolute, engine::Tick& out) const noexcept
    {
        return endpoint ({ canonicalAbsolute, 0, 1 }, out);
    }

    [[nodiscard]] MidiCreationStatus sourcePosition (std::uint32_t sourceTick, std::uint16_t sourcePpq,
        engine::Tick& relative) const noexcept
    {
        relative = 0;
        engine::Tick canonical = 0, native = 0;
        auto result = canonicalSourceEndpoint (sourceTick, sourcePpq, canonical);
        if (result == MidiCreationStatus::Ok) result = nativeEndpoint (canonical, native);
        if (result == MidiCreationStatus::Ok) relative = native - nativeStart();
        return result;
    }

    [[nodiscard]] MidiCreationStatus sourceNote (std::uint32_t sourceStart, std::uint32_t sourceEnd,
        std::uint16_t sourcePpq, MidiCreationSpan& relative) const noexcept
    {
        relative = {};
        if (sourceEnd < sourceStart) return MidiCreationStatus::InvalidInput;
        engine::Tick start = 0, end = 0;
        auto result = sourcePosition (sourceStart, sourcePpq, start);
        if (result == MidiCreationStatus::Ok) result = sourcePosition (sourceEnd, sourcePpq, end);
        if (result != MidiCreationStatus::Ok) return result;
        if (end < start) return MidiCreationStatus::Precision;
        relative = { start, end };
        return MidiCreationStatus::Ok;
    }

    [[nodiscard]] MidiCreationStatus emptyClip (MidiCreationSpan& native) const noexcept
    {
        return clip (tick_, true, native);
    }

    // contentEnd is the maximum canonical absolute Note/control/EOT endpoint.
    [[nodiscard]] MidiCreationStatus importedClip (engine::Tick contentEnd, MidiCreationSpan& native) const noexcept
    {
        return clip (contentEnd, false, native);
    }

private:
    [[nodiscard]] static MidiCreationStatus status (engine::TimeLookupStatus value) noexcept
    {
        switch (value)
        {
            case engine::TimeLookupStatus::Ok: return MidiCreationStatus::Ok;
            case engine::TimeLookupStatus::InvalidInput: return MidiCreationStatus::InvalidInput;
            case engine::TimeLookupStatus::InvalidMap: return MidiCreationStatus::InvalidClock;
            case engine::TimeLookupStatus::Overflow: return MidiCreationStatus::Overflow;
            case engine::TimeLookupStatus::NumericDomain: return MidiCreationStatus::Precision;
        }
        return MidiCreationStatus::Precision;
    }

    [[nodiscard]] engine::Tick nativeStart() const noexcept
    {
        return timeBase_ == engine::TimeBase::TempoLocked ? tick_ : frame_;
    }

    [[nodiscard]] MidiCreationStatus nearestFrame (engine::RationalTick tick, engine::Tick& out) const noexcept
    {
        out = 0;
        TimelineFramePoint point;
        point.offset = clock_->framePosition (tick.whole);
        if (tick.numerator != 0)
        {
            engine::click_detail::Enclosure fraction;
            bool before = false;
            const auto result = status (clock_->frameDisplacement (tick.whole, true,
                tick.numerator, tick.denominator, fraction, before));
            if (result != MidiCreationStatus::Ok) return result;
            if (before) return MidiCreationStatus::Precision;
            point.offset = engine::click_detail::add (point.offset, fraction);
        }
        const auto result = timelineNativeFrameTarget ({}, engine::TimeBase::SampleLocked, point, out);
        if (result == TimelineCoordinateStatus::Ok) return MidiCreationStatus::Ok;
        out = 0;
        return result == TimelineCoordinateStatus::Overflow ? MidiCreationStatus::Overflow : MidiCreationStatus::Precision;
    }

    [[nodiscard]] MidiCreationStatus endpoint (engine::RationalTick canonical, engine::Tick& out) const noexcept
    {
        out = 0;
        if (! ready()) return MidiCreationStatus::InvalidClock;
        if (canonical.whole < tick_ || canonical.denominator == 0 || canonical.numerator >= canonical.denominator)
            return MidiCreationStatus::InvalidInput;
        if (timeBase_ == engine::TimeBase::TempoLocked)
            return status (engine::roundRationalTick (canonical, engine::TimeRounding::Nearest, out));
        if (canonical.whole == tick_ && canonical.numerator == 0)
        {
            out = frame_;
            return MidiCreationStatus::Ok;
        }
        engine::Tick destination = 0;
        auto result = fixedBaseStatus_;
        if (result == MidiCreationStatus::Ok) result = nearestFrame (canonical, destination);
        if (result != MidiCreationStatus::Ok) return result;
        if (destination < fixedBaseFrame_) return MidiCreationStatus::Precision;
        const auto delta = destination - fixedBaseFrame_;
        if (delta > INT64_MAX - frame_) return MidiCreationStatus::Overflow;
        out = frame_ + delta;
        return MidiCreationStatus::Ok;
    }

    [[nodiscard]] MidiCreationStatus clip (engine::Tick contentEnd, bool strict, MidiCreationSpan& out) const noexcept
    {
        out = {};
        if (! ready()) return MidiCreationStatus::InvalidClock;
        if (contentEnd < tick_) return MidiCreationStatus::InvalidInput;
        engine::MeterPosition position;
        auto result = status (clock_->meterMap().positionForTick (contentEnd, position));
        if (result != MidiCreationStatus::Ok) return result;
        engine::RationalTick boundary;
        result = status (clock_->meterMap().barStart (position.barBeat.bar, boundary));
        if (result != MidiCreationStatus::Ok) return result;
        if (strict || boundary.whole != contentEnd || boundary.numerator != 0)
        {
            if (position.barBeat.bar == INT64_MAX) return MidiCreationStatus::Overflow;
            result = status (clock_->meterMap().barStart (position.barBeat.bar + 1, boundary));
            if (result != MidiCreationStatus::Ok) return result;
        }
        engine::Tick end = 0;
        result = endpoint (boundary, end);
        if (result != MidiCreationStatus::Ok) return result;
        if (end <= nativeStart()) return MidiCreationStatus::CollapsedClipWindow;
        out = { nativeStart(), end };
        return MidiCreationStatus::Ok;
    }

    std::shared_ptr<const engine::MusicalClickMap> clock_;
    std::int64_t frame_ = 0;
    engine::Tick tick_ = 0;
    engine::TimeBase timeBase_ = engine::TimeBase::TempoLocked;
    engine::Tick fixedBaseFrame_ = 0;
    MidiCreationStatus fixedBaseStatus_ = MidiCreationStatus::InvalidClock;
};

}
