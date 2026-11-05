#pragma once

#include "engine/MusicalClick.h"
#include "engine/Project.h"
#include "ui/MusicalGrid.h"

#include <optional>
#include <span>
#include <memory>

namespace yesdaw::ui {

enum class TimelineCoordinateStatus { Ok, InvalidInput, Overflow, Precision, BeforeOrigin };

struct TimelineNativePosition
{
    engine::TimeBase timeBase = engine::TimeBase::SampleLocked;
    engine::Tick start = 0;
    friend bool operator== (const TimelineNativePosition&, const TimelineNativePosition&) = default;
};

struct TimelineFrameTarget
{
    enum class Kind { ExactFrame, FractionalFrame, NoTimeChange };
    Kind kind = Kind::ExactFrame;
    std::int64_t frame = 0;
    double fractionalFrame = 0.0;
    TimelineFrameTarget (std::int64_t value) noexcept : frame (value) {}
    [[nodiscard]] static TimelineFrameTarget fractional (double value) noexcept
    {
        if (std::isfinite (value) && value >= 0.0 && value < 0x1p63 && std::floor (value) == value)
            return TimelineFrameTarget (static_cast<std::int64_t> (value));
        TimelineFrameTarget target (0);
        target.kind = Kind::FractionalFrame;
        target.fractionalFrame = value;
        return target;
    }
    [[nodiscard]] static TimelineFrameTarget noTimeChange() noexcept
    {
        TimelineFrameTarget target (0);
        target.kind = Kind::NoTimeChange;
        return target;
    }
};

struct TimelineFrameDisplacement
{
    struct TargetCertificate
    {
        TimelineNativePosition origin;
        engine::click_detail::Enclosure destination;
    };
    bool noTimeChange = false;
    bool exactInteger = false;
    std::int64_t integerFrames = 0;
    engine::click_detail::Enclosure frames;
    std::optional<TargetCertificate> targetCertificate;
};

[[nodiscard]] inline engine::click_detail::Enclosure timelineSignedInteger (std::int64_t value) noexcept
{
    const auto magnitude = value < 0 ? std::uint64_t { 0 } - static_cast<std::uint64_t> (value)
                                     : static_cast<std::uint64_t> (value);
    const auto position = engine::click_detail::integer (magnitude);
    return value < 0 ? engine::click_detail::Enclosure { -position.high, -position.low, position.numericFailure }
                     : position;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineStatus (engine::TimeLookupStatus status) noexcept
{
    if (status == engine::TimeLookupStatus::Ok) return TimelineCoordinateStatus::Ok;
    if (status == engine::TimeLookupStatus::Overflow) return TimelineCoordinateStatus::Overflow;
    if (status == engine::TimeLookupStatus::InvalidInput) return TimelineCoordinateStatus::InvalidInput;
    return TimelineCoordinateStatus::Precision;
}

[[nodiscard]] inline engine::click_detail::Enclosure timelineFramePosition (
    const engine::MusicalClickMap& clock, TimelineNativePosition position) noexcept
{
    return position.timeBase == engine::TimeBase::SampleLocked
        ? engine::click_detail::integer (static_cast<std::uint64_t> (position.start))
        : clock.framePosition (position.start);
}

struct TimelineFramePoint
{
    std::int64_t integerOrigin = 0;
    engine::click_detail::Enclosure offset;
    std::shared_ptr<const engine::MusicalClickMap> clock;
    std::optional<engine::Tick> nativeTick;

    [[nodiscard]] static TimelineFramePoint exact (std::int64_t frame) noexcept
    {
        TimelineFramePoint point;
        point.integerOrigin = frame;
        return point;
    }
    [[nodiscard]] bool exactFrame() const noexcept
    {
        return ! offset.numericFailure && offset.low == 0.0 && offset.high == 0.0;
    }
};

struct TimelineFrameSpan
{
    TimelineFramePoint start, end;
};

[[nodiscard]] inline engine::click_detail::Enclosure timelineFrameValue (const TimelineFramePoint& point) noexcept
{
    return engine::click_detail::add (timelineSignedInteger (point.integerOrigin), point.offset);
}

[[nodiscard]] inline bool timelineSameFramePoint (const TimelineFramePoint& a, const TimelineFramePoint& b) noexcept
{
    if (a.exactFrame() && b.exactFrame()) return a.integerOrigin == b.integerOrigin;
    if (a.nativeTick && b.nativeTick && a.clock == b.clock && a.clock != nullptr)
        return a.nativeTick == b.nativeTick;
    const auto av = timelineFrameValue (a), bv = timelineFrameValue (b);
    return ! av.numericFailure && ! bv.numericFailure && av.low == av.high && bv.low == bv.high && av.low == bv.low;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineCheckFramePoint (const TimelineFramePoint& point) noexcept
{
    if (point.integerOrigin < 0) return TimelineCoordinateStatus::BeforeOrigin;
    if (point.exactFrame()) return TimelineCoordinateStatus::Ok;
    const auto value = point.offset;
    if (value.numericFailure || ! std::isfinite (value.low) || ! std::isfinite (value.high)) return TimelineCoordinateStatus::Precision;
    if (value.low < 0.0 && engine::click_detail::compareScalar (-value.low, point.integerOrigin) > 0)
        return value.high < 0.0 && engine::click_detail::compareScalar (-value.high, point.integerOrigin) > 0
            ? TimelineCoordinateStatus::BeforeOrigin : TimelineCoordinateStatus::Precision;
    if (point.integerOrigin == 0)
    {
        if (value.low >= 0x1p63) return TimelineCoordinateStatus::Overflow;
        if (value.high >= 0x1p63) return TimelineCoordinateStatus::Precision;
    }
    else
    {
        const auto limit = (INT64_MAX - point.integerOrigin) + 1;
        if (engine::click_detail::compareScalar (value.low, limit) >= 0) return TimelineCoordinateStatus::Overflow;
        if (engine::click_detail::compareScalar (value.high, limit) >= 0) return TimelineCoordinateStatus::Precision;
    }
    return TimelineCoordinateStatus::Ok;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineCompareFramePoints (
    const TimelineFramePoint& a, const TimelineFramePoint& b, int& order) noexcept
{
    order = 0;
    auto status = timelineCheckFramePoint (a);
    if (status != TimelineCoordinateStatus::Ok) return status;
    status = timelineCheckFramePoint (b);
    if (status != TimelineCoordinateStatus::Ok) return status;
    if (timelineSameFramePoint (a, b)) return TimelineCoordinateStatus::Ok;
    if (a.exactFrame() && b.exactFrame()) order = a.integerOrigin < b.integerOrigin ? -1 : 1;
    else if (a.nativeTick && b.nativeTick && a.clock == b.clock && a.clock != nullptr)
        order = *a.nativeTick < *b.nativeTick ? -1 : 1;
    else
    {
        const auto av = timelineFrameValue (a), bv = timelineFrameValue (b);
        if (av.numericFailure || bv.numericFailure) return TimelineCoordinateStatus::Precision;
        if (av.high <= bv.low) order = -1;
        else if (bv.high <= av.low) order = 1;
        else return TimelineCoordinateStatus::Precision;
    }
    return TimelineCoordinateStatus::Ok;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineProjectFrameSpan (
    const std::shared_ptr<const engine::MusicalClickMap>& clock, TimelineNativePosition start,
    engine::Tick length, TimelineFrameSpan& out) noexcept
{
    if (start.start < 0 || length <= 0) return TimelineCoordinateStatus::InvalidInput;
    if (length > INT64_MAX - start.start) return TimelineCoordinateStatus::Overflow;
    const auto project = [&] (engine::Tick tick, TimelineFramePoint& point) {
        point = TimelineFramePoint::exact (tick);
        if (start.timeBase == engine::TimeBase::TempoLocked)
        {
            if (clock == nullptr) return TimelineCoordinateStatus::Precision;
            point.integerOrigin = 0;
            point.offset = clock->framePosition (tick);
            point.clock = clock;
            point.nativeTick = tick;
            const auto status = timelineCheckFramePoint (point);
            if (status != TimelineCoordinateStatus::Ok) return status;
            const double integral = std::floor (point.offset.low);
            point.integerOrigin = static_cast<std::int64_t> (integral);
            point.offset = engine::click_detail::add (point.offset, { -integral, -integral, false });
        }
        return timelineCheckFramePoint (point);
    };
    auto status = project (start.start, out.start);
    return status == TimelineCoordinateStatus::Ok ? project (start.start + length, out.end) : status;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineExpandFrameSpan (
    TimelineFrameSpan& bounds, const TimelineFrameSpan& candidate) noexcept
{
    int order = 0;
    auto status = timelineCompareFramePoints (candidate.start, bounds.start, order);
    if (status != TimelineCoordinateStatus::Ok) return status;
    if (order < 0) bounds.start = candidate.start;
    status = timelineCompareFramePoints (candidate.end, bounds.end, order);
    if (status == TimelineCoordinateStatus::Ok && order > 0) bounds.end = candidate.end;
    return status;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineTranslateFramePoint (
    const TimelineFramePoint& base, const TimelineFrameSpan& offset, TimelineFramePoint& out) noexcept
{
    auto status = timelineCheckFramePoint (base);
    if (status != TimelineCoordinateStatus::Ok) return status;
    status = timelineCheckFramePoint (offset.start);
    if (status != TimelineCoordinateStatus::Ok) return status;
    status = timelineCheckFramePoint (offset.end);
    if (status != TimelineCoordinateStatus::Ok) return status;
    if (timelineSameFramePoint (offset.start, offset.end)) out = base;
    else if (timelineSameFramePoint (base, offset.start)) out = offset.end;
    else
    {
        const auto integerShift = offset.end.integerOrigin - offset.start.integerOrigin;
        auto fractionalShift = engine::click_detail::add (offset.end.offset,
            { -offset.start.offset.high, -offset.start.offset.low, offset.start.offset.numericFailure });
        out = base;
        out.clock.reset();
        out.nativeTick.reset();
        if (integerShift < -out.integerOrigin)
        {
            fractionalShift = engine::click_detail::add (fractionalShift,
                timelineSignedInteger (out.integerOrigin + integerShift));
            out.integerOrigin = 0;
        }
        else if (integerShift > INT64_MAX - out.integerOrigin)
        {
            fractionalShift = engine::click_detail::add (fractionalShift,
                timelineSignedInteger (integerShift - (INT64_MAX - out.integerOrigin)));
            out.integerOrigin = INT64_MAX;
        }
        else out.integerOrigin += integerShift;
        out.offset = engine::click_detail::add (out.offset, fractionalShift);
    }
    return timelineCheckFramePoint (out);
}

[[nodiscard]] inline TimelineCoordinateStatus timelineNativeFrameTarget (
    const std::shared_ptr<const engine::MusicalClickMap>& clock, engine::TimeBase timeBase,
    const TimelineFramePoint& point, engine::Tick& out) noexcept
{
    const auto status = timelineCheckFramePoint (point);
    if (status != TimelineCoordinateStatus::Ok) return status;
    if (timeBase == engine::TimeBase::TempoLocked)
    {
        if (clock == nullptr) return TimelineCoordinateStatus::Precision;
        if (point.clock == clock && point.nativeTick) { out = *point.nativeTick; return TimelineCoordinateStatus::Ok; }
        return timelineStatus (clock->tickAtFramePosition (timelineFrameValue (point), out));
    }
    const auto shifted = engine::click_detail::add (point.offset, { 0.5, 0.5, false });
    const double low = std::floor (shifted.low), high = std::floor (shifted.high);
    if (shifted.numericFailure) return TimelineCoordinateStatus::Precision;
    if (low < -0x1p63 || high >= 0x1p63) return TimelineCoordinateStatus::Overflow;
    if (low != high) return TimelineCoordinateStatus::Precision;
    const auto delta = static_cast<std::int64_t> (low);
    if (delta < -point.integerOrigin) return TimelineCoordinateStatus::BeforeOrigin;
    if (delta > INT64_MAX - point.integerOrigin) return TimelineCoordinateStatus::Overflow;
    out = point.integerOrigin + delta;
    return TimelineCoordinateStatus::Ok;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineDisplacementTo (
    const engine::MusicalClickMap& clock, TimelineNativePosition anchor,
    TimelineFrameTarget target, TimelineFrameDisplacement& out) noexcept
{
    using namespace engine::click_detail;
    out = {};
    if (target.kind == TimelineFrameTarget::Kind::NoTimeChange)
    {
        out.noTimeChange = true;
        return TimelineCoordinateStatus::Ok;
    }
    if (anchor.start < 0 || (target.kind == TimelineFrameTarget::Kind::ExactFrame ? target.frame < 0
        : ! std::isfinite (target.fractionalFrame) || target.fractionalFrame < 0.0))
        return TimelineCoordinateStatus::InvalidInput;
    if (anchor.timeBase == engine::TimeBase::SampleLocked && target.kind == TimelineFrameTarget::Kind::ExactFrame)
    {
        out.exactInteger = true;
        out.integerFrames = target.frame - anchor.start;
        out.frames = timelineSignedInteger (out.integerFrames);
        return TimelineCoordinateStatus::Ok;
    }
    const auto origin = timelineFramePosition (clock, anchor);
    const auto destination = target.kind == TimelineFrameTarget::Kind::ExactFrame
        ? integer (static_cast<std::uint64_t> (target.frame))
        : Enclosure { target.fractionalFrame, target.fractionalFrame, false };
    out.frames = add (destination, { -origin.high, -origin.low, origin.numericFailure });
    out.targetCertificate = TimelineFrameDisplacement::TargetCertificate { anchor, destination };
    return out.frames.numericFailure ? TimelineCoordinateStatus::Precision : TimelineCoordinateStatus::Ok;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineNativeDestination (
    const engine::MusicalClickMap& clock, TimelineNativePosition origin,
    const TimelineFrameDisplacement& shift, engine::Tick& out) noexcept
{
    using namespace engine::click_detail;
    out = origin.start;
    if (shift.noTimeChange) return TimelineCoordinateStatus::Ok;
    if (origin.timeBase == engine::TimeBase::SampleLocked && shift.exactInteger)
    {
        if (shift.integerFrames < -origin.start) return TimelineCoordinateStatus::BeforeOrigin;
        if (shift.integerFrames > INT64_MAX - origin.start) return TimelineCoordinateStatus::Overflow;
        out = origin.start + shift.integerFrames;
        return TimelineCoordinateStatus::Ok;
    }
    const auto position = shift.targetCertificate && shift.targetCertificate->origin == origin
        ? shift.targetCertificate->destination : add (timelineFramePosition (clock, origin), shift.frames);
    if (position.numericFailure) return TimelineCoordinateStatus::Precision;
    if (position.high < 0.0) return TimelineCoordinateStatus::BeforeOrigin;
    if (position.low < 0.0) return TimelineCoordinateStatus::Precision;
    if (origin.timeBase == engine::TimeBase::TempoLocked)
        return timelineStatus (clock.tickAtFramePosition (position, out));
    // For integer origins, ties toward the later absolute destination are translation
    // invariant. This also keeps every original Frame above binary64 integer precision.
    const auto shifted = add (shift.frames, { 0.5, 0.5, false });
    const double low = std::floor (shifted.low), high = std::floor (shifted.high);
    if (low < -0x1p63 || high >= 0x1p63) return TimelineCoordinateStatus::Overflow;
    if (low != high) return TimelineCoordinateStatus::Precision;
    const auto delta = static_cast<std::int64_t> (low);
    if (delta < -origin.start) return TimelineCoordinateStatus::BeforeOrigin;
    if (delta > INT64_MAX - origin.start) return TimelineCoordinateStatus::Overflow;
    out = origin.start + delta;
    return TimelineCoordinateStatus::Ok;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineClampToOrigin (
    const engine::MusicalClickMap& clock, std::span<const TimelineNativePosition> selection,
    TimelineFrameDisplacement& shift, bool provedBeforeOrigin = false) noexcept
{
    using namespace engine::click_detail;
    if (selection.empty()) return TimelineCoordinateStatus::InvalidInput;
    if (shift.noTimeChange) return TimelineCoordinateStatus::Ok;
    bool clamp = provedBeforeOrigin;
    for (const auto origin : selection)
    {
        if (origin.start < 0) return TimelineCoordinateStatus::InvalidInput;
        if (shift.exactInteger && origin.timeBase == engine::TimeBase::SampleLocked)
            clamp = clamp || shift.integerFrames < -origin.start;
        else if (! provedBeforeOrigin)
        {
            const auto position = shift.targetCertificate && shift.targetCertificate->origin == origin
                ? shift.targetCertificate->destination : add (timelineFramePosition (clock, origin), shift.frames);
            if (position.numericFailure) return TimelineCoordinateStatus::Precision;
            if (position.high < 0.0) clamp = true;
            else if (position.low < 0.0) return TimelineCoordinateStatus::Precision;
        }
    }
    if (! clamp) return TimelineCoordinateStatus::Ok;
    auto earliest = selection.front();
    auto first = timelineFramePosition (clock, earliest);
    if (first.numericFailure) return TimelineCoordinateStatus::Precision;
    for (const auto candidate : selection.subspan (1))
    {
        if (candidate == earliest) continue;
        if (candidate.timeBase == engine::TimeBase::SampleLocked && earliest.timeBase == candidate.timeBase)
        {
            if (candidate.start < earliest.start) { earliest = candidate; first = timelineFramePosition (clock, candidate); }
            continue;
        }
        const auto position = timelineFramePosition (clock, candidate);
        if (position.numericFailure) return TimelineCoordinateStatus::Precision;
        if (position.high <= first.low) { earliest = candidate; first = position; }
        else if (position.low < first.high) return TimelineCoordinateStatus::Precision;
    }
    shift = {};
    shift.targetCertificate = TimelineFrameDisplacement::TargetCertificate { earliest, { 0.0, 0.0, false } };
    shift.exactInteger = earliest.timeBase == engine::TimeBase::SampleLocked;
    shift.integerFrames = -earliest.start;
    shift.frames = shift.exactInteger ? timelineSignedInteger (shift.integerFrames)
                                     : Enclosure { -first.high, -first.low, first.numericFailure };
    return TimelineCoordinateStatus::Ok;
}

[[nodiscard]] inline std::optional<double> timelineSnapFrame (
    const engine::CompiledTempoMap& tempo, const engine::CompiledMeterMap& meters, engine::MeterMapView view,
    double frame, MusicalGridLevel level, int subdivisions)
{
    double snapped = 0.0;
    return checkedSnapFrameToMusicalGridPosition (tempo, meters, view, frame, level, subdivisions, snapped)
        == MusicalGridStatus::Ok ? std::optional<double> (snapped) : std::nullopt;
}

}
