// YES DAW - G4.6 / ADR-0052 automation span edits (control side; never on the audio thread).
//
// A ride pass and a Pencil sweep REPLACE a span of a lane's points. The lane's curve outside the span
// keeps its values: where the lane had points before (after) the span, an edge anchor carrying the OLD
// curve's value is inserted just outside it. "Just outside" is ADR-0052's t0 - 1 / t1 + 1 tick, stepped
// further out only when that tick compiles to the span edge's own audio frame (a tick is shorter than a
// frame above 187.5 BPM at 48 kHz, 172.3 BPM at 44.1 kHz): the lane compiler refuses two points on one
// frame, so a literal t0 - 1 would make the whole edit unplayable.

#pragma once

#include "engine/Automation.h"
#include "engine/Project.h"
#include "engine/Time.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace yesdaw::engine {

// The frame a breakpoint compiles to — the one law the projection's lane compiler and every span edit
// share (a tick that cannot compile fails both).
[[nodiscard]] inline bool compiledAutomationFrameForTick (const CompiledTempoMap& tempoMap, Tick tick, std::int64_t& out) noexcept
{
    out = 0;
    double frame = 0.0;
    if (! tempoMap.frameForTick (tick, frame) || ! std::isfinite (frame) || frame < 0.0)
        return false;

    constexpr double kMaxFrame = static_cast<double> (std::numeric_limits<std::int64_t>::max()) - 1.0;
    if (frame > kMaxFrame)
        return false;

    out = static_cast<std::int64_t> (std::llround (frame));
    return true;
}

enum class AutomationSpanEditStatus : std::uint8_t
{
    Ok = 0,
    InvalidInput,    // the written points are empty, unsorted, out of range, or share a compiled frame
    Uncompilable     // the lane's kept points do not compile against this tempo map
};

namespace detail {

[[nodiscard]] inline bool compileAutomationPointFrames (const CompiledTempoMap& tempoMap,
                                                        std::span<const AutomationBreakpoint> points,
                                                        std::vector<std::int64_t>& frames)
{
    frames.clear();
    frames.reserve (points.size());
    for (const AutomationBreakpoint& point : points)
    {
        std::int64_t frame = 0;
        if (! compiledAutomationFrameForTick (tempoMap, point.tick, frame))
            return false;
        frames.push_back (frame);
    }
    return true;
}

// The lane's value at `frame` by the compiled law (CompiledGraph's automationValueAtFrame: hold the first
// value before the first point and the last after the last; each segment shaped by its LEFT point's curve).
// `segmentCurve` is that left point's curve — what an anchor placed here carries.
[[nodiscard]] inline double automationValueAtCompiledFrame (std::span<const AutomationBreakpoint> points,
                                                           std::span<const std::int64_t> frames,
                                                           std::int64_t frame,
                                                           AutomationCurveType& segmentCurve) noexcept
{
    segmentCurve = AutomationCurveType::Linear;
    if (points.empty())
        return 0.0;

    std::size_t next = 0;
    while (next < frames.size() && frames[next] < frame)
        ++next;

    if (next == 0u)
        return points.front().value;
    if (next < frames.size() && frames[next] == frame)
    {
        segmentCurve = points[next].curveType;
        return points[next].value;
    }
    segmentCurve = points[next - 1u].curveType;
    if (next >= frames.size())
        return points.back().value;

    const std::int64_t startFrame = frames[next - 1u];
    const std::int64_t endFrame = frames[next];
    if (endFrame <= startFrame)
        return points[next].value;

    const double t = static_cast<double> (frame - startFrame) / static_cast<double> (endFrame - startFrame);
    const double u = automationCurveProgress (points[next - 1u].curveType, t);
    return points[next - 1u].value + (points[next].value - points[next - 1u].value) * u;
}

} // namespace detail

// ADR-0052's span replacement. `written` (a ride pass or Pencil sweep: non-empty, strictly increasing
// ticks and compiled frames, valid values) replaces the span [t0, t1] = its first and last ticks. An old
// point goes when its tick lies in the span or its compiled frame lands on [frame(t0), frame(t1)] (it
// would share a frame with the written edge). Kept points before (after) the span get an edge anchor at
// the nearest tick outside the span with its own frame: the OLD lane's value there, carrying the curve
// of the old segment it cuts — so a Linear or Hold curve outside the span is unchanged, and a Bezier or
// Log segment cut by the edge is re-shaped between the same values (ADR-0052's accepted approximation).
// An anchor that would share a frame with the kept neighbour is skipped: the neighbour already holds
// that value within one frame. With no kept point on a side, the lane holds the written edge value there.
[[nodiscard]] inline AutomationSpanEditStatus replaceAutomationSpan (std::span<const AutomationBreakpoint> lane,
                                                                    std::span<const AutomationBreakpoint> written,
                                                                    const CompiledTempoMap& tempoMap,
                                                                    std::vector<AutomationBreakpoint>& out)
{
    out.clear();
    if (written.empty())
        return AutomationSpanEditStatus::InvalidInput;

    std::vector<std::int64_t> writtenFrames;
    if (! detail::compileAutomationPointFrames (tempoMap, written, writtenFrames))
        return AutomationSpanEditStatus::InvalidInput;
    for (std::size_t i = 0; i < written.size(); ++i)
    {
        if (! written[i].isValid())
            return AutomationSpanEditStatus::InvalidInput;
        if (i > 0u && (written[i].tick <= written[i - 1u].tick || writtenFrames[i] <= writtenFrames[i - 1u]))
            return AutomationSpanEditStatus::InvalidInput;
    }

    std::vector<std::int64_t> laneFrames;
    if (! detail::compileAutomationPointFrames (tempoMap, lane, laneFrames))
        return AutomationSpanEditStatus::Uncompilable;

    const Tick t0 = written.front().tick;
    const Tick t1 = written.back().tick;
    const std::int64_t f0 = writtenFrames.front();
    const std::int64_t f1 = writtenFrames.back();

    // The kept points on each side (the lane is tick-sorted, so its frames never decrease).
    std::size_t leftEnd = 0;   // [0, leftEnd) kept before the span
    while (leftEnd < lane.size() && lane[leftEnd].tick < t0 && laneFrames[leftEnd] < f0)
        ++leftEnd;
    std::size_t rightBegin = lane.size();   // [rightBegin, size) kept after the span
    while (rightBegin > leftEnd && lane[rightBegin - 1u].tick > t1 && laneFrames[rightBegin - 1u] > f1)
        --rightBegin;

    out.reserve (lane.size() + written.size() + 2u);
    out.assign (lane.begin(), lane.begin() + static_cast<std::ptrdiff_t> (leftEnd));

    if (leftEnd > 0u)
    {
        const Tick keptTick = lane[leftEnd - 1u].tick;
        const std::int64_t keptFrame = laneFrames[leftEnd - 1u];
        Tick anchorTick = t0 - 1;
        std::int64_t anchorFrame = 0;
        bool compiled = compiledAutomationFrameForTick (tempoMap, anchorTick, anchorFrame);
        while (compiled && anchorFrame >= f0 && anchorTick > keptTick)
        {
            --anchorTick;
            compiled = compiledAutomationFrameForTick (tempoMap, anchorTick, anchorFrame);
        }
        if (compiled && anchorTick > keptTick && anchorFrame > keptFrame && anchorFrame < f0)
        {
            AutomationCurveType curve = AutomationCurveType::Linear;
            const double value = detail::automationValueAtCompiledFrame (lane, laneFrames, anchorFrame, curve);
            out.push_back ({ anchorTick, std::clamp (value, 0.0, 1.0), curve });
        }
    }

    out.insert (out.end(), written.begin(), written.end());

    if (rightBegin < lane.size())
    {
        const Tick keptTick = lane[rightBegin].tick;
        const std::int64_t keptFrame = laneFrames[rightBegin];
        Tick anchorTick = t1 + 1;
        std::int64_t anchorFrame = 0;
        bool compiled = compiledAutomationFrameForTick (tempoMap, anchorTick, anchorFrame);
        while (compiled && anchorFrame <= f1 && anchorTick < keptTick)
        {
            ++anchorTick;
            compiled = compiledAutomationFrameForTick (tempoMap, anchorTick, anchorFrame);
        }
        if (compiled && anchorTick < keptTick && anchorFrame < keptFrame && anchorFrame > f1)
        {
            AutomationCurveType curve = AutomationCurveType::Linear;
            const double value = detail::automationValueAtCompiledFrame (lane, laneFrames, anchorFrame, curve);
            out.push_back ({ anchorTick, std::clamp (value, 0.0, 1.0), curve });
        }
    }

    out.insert (out.end(), lane.begin() + static_cast<std::ptrdiff_t> (rightBegin), lane.end());

    // The result must compile: strictly increasing ticks and frames (a kept pair that already shared a
    // frame — a lane stored under Off and then retimed — is not this edit's to repair).
    std::int64_t previousFrame = 0;
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        std::int64_t frame = 0;
        if (! out[i].isValid() || ! compiledAutomationFrameForTick (tempoMap, out[i].tick, frame)
            || (i > 0u && (out[i].tick <= out[i - 1u].tick || frame <= previousFrame)))
        {
            out.clear();
            return AutomationSpanEditStatus::Uncompilable;
        }
        previousFrame = frame;
    }

    return AutomationSpanEditStatus::Ok;
}

} // namespace yesdaw::engine
