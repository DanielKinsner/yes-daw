#pragma once

#include "engine/MusicalClick.h"
#include "engine/Project.h"
#include "ui/MusicalGrid.h"

#include <optional>
#include <span>

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
    using namespace musical_grid_detail;
    if (! std::isfinite (frame) || frame < 0.0 || subdivisions < 0 || subdivisions > 16) return std::nullopt;
    engine::MusicalTime time;
    engine::MeterPosition position;
    if (tempo.musicalTimeForFrame (frame, time) != engine::TimeLookupStatus::Ok
        || meters.positionForMusicalTime (time, position) != engine::TimeLookupStatus::Ok) return std::nullopt;
    Bar bar;
    if (barAt (tempo, meters, view, position.barBeat.bar, bar) != MusicalGridStatus::Ok) return std::nullopt;
    double best = bar.frame;
    const auto consider = [&] (double candidate) {
        if (std::abs (candidate - frame) < std::abs (best - frame)
            || (std::abs (candidate - frame) == std::abs (best - frame) && candidate > best)) best = candidate;
    };
    consider (bar.endFrame);
    const auto candidates = [&] (std::uint64_t span, std::uint32_t denominator) {
        denominator *= std::uint32_t { 1 } << subdivisions;
        const auto middle = ordinalAt (time, bar.start, span, denominator);
        const auto first = middle > 1 ? middle - 1 : 0;
        for (std::uint64_t n = first; n - first < 4; ++n)
        {
            engine::RationalTick tick;
            if (offset (bar.start, n, span, denominator, tick) != MusicalGridStatus::Ok) return false;
            if (! before (tick, bar.end)) break;
            double mapped = 0.0;
            if (frameAt (tempo, tick, mapped) != MusicalGridStatus::Ok) return false;
            consider (mapped);
        }
        return true;
    };
    bool ok = false;
    if (level == MusicalGridLevel::Bar) ok = candidates (61440ULL * bar.meter.numerator, bar.meter.denominator);
    else if (level == MusicalGridLevel::Beat) ok = candidates (61440, bar.meter.denominator);
    else if (level == MusicalGridLevel::Sixteenth)
        ok = candidates (3840, 1) && candidates (61440, bar.meter.denominator);
    return ok && std::isfinite (best) && best >= 0.0 && best < 0x1p63 ? std::optional<double> (best) : std::nullopt;
}

}
