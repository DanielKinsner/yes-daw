#pragma once

#include "ui/TimelineCoordinates.h"

#include <numeric>

namespace yesdaw::ui {

// A Control-side display operation captures one clock for every native endpoint it
// compares. Geometry does not require playback/renderer admission.
[[nodiscard]] inline std::shared_ptr<const engine::MusicalClickMap> timelineReadoutClock (
    const engine::Project& project)
{
    auto clock = std::make_shared<engine::MusicalClickMap>();
    if (engine::MusicalClickMap::build ({ project.tempoMap.data(), project.tempoMap.size() },
            { project.meterMap.data(), project.meterMap.size() }, project.sampleRate, *clock)
        != engine::TimeLookupStatus::Ok)
        return nullptr;
    return clock;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineReadoutSeconds (
    const TimelineFramePoint& point, engine::SampleRate rate, double& out) noexcept
{
    if (! rate.isValid()) return TimelineCoordinateStatus::InvalidInput;
    const auto status = timelineCheckFramePoint (point);
    if (status != TimelineCoordinateStatus::Ok) return status;
    out = static_cast<double> (point.integerOrigin) / rate.hz
        + std::midpoint (point.offset.low, point.offset.high) / rate.hz;
    return std::isfinite (out) && out >= 0.0 ? TimelineCoordinateStatus::Ok : TimelineCoordinateStatus::Precision;
}

struct TimelineReadout
{
    TimelineFrameSpan span;
    TimelineFramePoint duration;
    double startSeconds = 0.0, endSeconds = 0.0, lengthSeconds = 0.0;
};

[[nodiscard]] inline TimelineCoordinateStatus timelineReadout (
    const std::shared_ptr<const engine::MusicalClickMap>& clock, TimelineNativePosition start,
    engine::Tick length, engine::SampleRate rate, TimelineReadout& out) noexcept
{
    TimelineReadout value;
    auto status = timelineProjectFrameSpan (clock, start, length, value.span);
    if (status == TimelineCoordinateStatus::Ok)
        status = timelineTranslateFramePoint (TimelineFramePoint::exact (0), value.span, value.duration);
    if (status == TimelineCoordinateStatus::Ok) status = timelineReadoutSeconds (value.span.start, rate, value.startSeconds);
    if (status == TimelineCoordinateStatus::Ok) status = timelineReadoutSeconds (value.span.end, rate, value.endSeconds);
    if (status == TimelineCoordinateStatus::Ok) status = timelineReadoutSeconds (value.duration, rate, value.lengthSeconds);
    if (status != TimelineCoordinateStatus::Ok) return status;
    if (value.lengthSeconds <= 0.0) return TimelineCoordinateStatus::Precision;
    out = std::move (value);
    return TimelineCoordinateStatus::Ok;
}

[[nodiscard]] inline TimelineCoordinateStatus timelineReadoutFrameAtSeconds (
    double seconds, engine::SampleRate rate, TimelineFramePoint& out) noexcept
{
    if (! rate.isValid() || ! std::isfinite (seconds) || seconds < 0.0)
        return TimelineCoordinateStatus::InvalidInput;
    const double frame = seconds * rate.hz;
    if (! std::isfinite (frame) || frame >= 0x1p63) return TimelineCoordinateStatus::Overflow;
    const double integral = std::floor (frame);
    out = TimelineFramePoint::exact (static_cast<std::int64_t> (integral));
    out.offset = { frame - integral, frame - integral, false };
    return timelineCheckFramePoint (out);
}

// End is an absolute position. Length is a Frame duration added to the actual
// projected start before the native inverse, including when a Tempo ramp crosses it.
[[nodiscard]] inline TimelineCoordinateStatus timelineReadoutNativeEnd (
    const std::shared_ptr<const engine::MusicalClickMap>& clock, TimelineNativePosition start,
    engine::Tick length, engine::SampleRate rate, double seconds, bool duration,
    engine::Tick& out) noexcept
{
    TimelineFrameSpan span;
    auto status = timelineProjectFrameSpan (clock, start, length, span);
    TimelineFramePoint target;
    if (status == TimelineCoordinateStatus::Ok) status = timelineReadoutFrameAtSeconds (seconds, rate, target);
    if (status == TimelineCoordinateStatus::Ok && duration)
        status = timelineTranslateFramePoint (span.start, { TimelineFramePoint::exact (0), target }, target);
    if (status == TimelineCoordinateStatus::Ok)
        status = timelineNativeFrameTarget (clock, start.timeBase, target, out);
    return status;
}

// Every seconds field must round-trip through its actual edit path, including
// Length independently of its endpoints. Binary64 controls cannot replace exact
// large SampleLocked Frames with a nearby representable value.
[[nodiscard]] inline bool timelineReadoutCanEdit (
    const std::shared_ptr<const engine::MusicalClickMap>& clock, TimelineNativePosition start,
    engine::Tick length, engine::SampleRate rate, const TimelineReadout& readout) noexcept
{
    if (start.start < 0 || length <= 0 || length > INT64_MAX - start.start) return false;
    const auto matches = [&] (double seconds, engine::Tick expected) {
        TimelineFramePoint point;
        engine::Tick native = 0;
        return timelineReadoutFrameAtSeconds (seconds, rate, point) == TimelineCoordinateStatus::Ok
            && timelineNativeFrameTarget (clock, start.timeBase, point, native) == TimelineCoordinateStatus::Ok
            && native == expected;
    };
    engine::Tick durationEnd = 0;
    return matches (readout.startSeconds, start.start) && matches (readout.endSeconds, start.start + length)
        && timelineReadoutNativeEnd (clock, start, length, rate, readout.lengthSeconds, true, durationEnd)
            == TimelineCoordinateStatus::Ok
        && durationEnd == start.start + length;
}

// Sort by a deterministic enclosure key, then certify the entire chronological
// chain before publishing it. Ambiguous overlaps never act as equality in a sort.
[[nodiscard]] inline TimelineCoordinateStatus timelineReadoutOrder (
    std::span<const TimelineFramePoint> starts, std::vector<std::size_t>& out)
{
    std::vector<std::size_t> order (starts.size());
    std::iota (order.begin(), order.end(), std::size_t { 0 });
    for (const auto& start : starts)
    {
        const auto status = timelineCheckFramePoint (start);
        if (status != TimelineCoordinateStatus::Ok) return status;
    }
    std::stable_sort (order.begin(), order.end(), [&] (std::size_t a, std::size_t b) {
        const auto& av = starts[a];
        const auto& bv = starts[b];
        if (av.integerOrigin != bv.integerOrigin) return av.integerOrigin < bv.integerOrigin;
        if (av.offset.low != bv.offset.low) return av.offset.low < bv.offset.low;
        if (av.offset.high != bv.offset.high) return av.offset.high < bv.offset.high;
        if (av.exactFrame() && bv.exactFrame()) return false;
        return av.nativeTick.value_or (0) < bv.nativeTick.value_or (0);
    });
    for (std::size_t i = 1; i < order.size(); ++i)
    {
        int comparison = 0;
        const auto status = timelineCompareFramePoints (starts[order[i - 1]], starts[order[i]], comparison);
        if (status != TimelineCoordinateStatus::Ok) return status;
        if (comparison > 0) return TimelineCoordinateStatus::Precision;
    }
    out = std::move (order);
    return TimelineCoordinateStatus::Ok;
}

}
