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
// The span law with an explicit span [t0, t1] (inclusive ticks) and the points written inside it — none
// erases the span, edges anchored (what a clip leaving its span does to the automation it carries away).
[[nodiscard]] inline AutomationSpanEditStatus replaceAutomationSpanBetween (std::span<const AutomationBreakpoint> lane,
                                                                           Tick t0,
                                                                           Tick t1,
                                                                           std::span<const AutomationBreakpoint> written,
                                                                           const CompiledTempoMap& tempoMap,
                                                                           std::vector<AutomationBreakpoint>& out)
{
    out.clear();
    if (t0 < 0 || t1 < t0 || (! written.empty() && (written.front().tick < t0 || written.back().tick > t1)))
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

    std::int64_t f0 = 0;
    std::int64_t f1 = 0;
    if (! compiledAutomationFrameForTick (tempoMap, t0, f0) || ! compiledAutomationFrameForTick (tempoMap, t1, f1))
        return AutomationSpanEditStatus::InvalidInput;

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

// ADR-0052's span replacement: `written` (a ride pass or Pencil sweep: non-empty, strictly increasing ticks
// and compiled frames, valid values) replaces the span its first and last ticks bound.
[[nodiscard]] inline AutomationSpanEditStatus replaceAutomationSpan (std::span<const AutomationBreakpoint> lane,
                                                                    std::span<const AutomationBreakpoint> written,
                                                                    const CompiledTempoMap& tempoMap,
                                                                    std::vector<AutomationBreakpoint>& out)
{
    out.clear();
    if (written.empty())
        return AutomationSpanEditStatus::InvalidInput;
    return replaceAutomationSpanBetween (lane, written.front().tick, written.back().tick, written, tempoMap, out);
}

// ADR-0052 "automation follows clips": one clip moving in time on its own track. Its span is half-open in its
// own time base — frames for a SampleLocked clip, ticks for a TempoLocked one — and `delta` is the move in
// that same base.
struct AutomationClipMove
{
    TimeBase timeBase = TimeBase::SampleLocked;
    Tick start = 0;
    Tick length = 0;
    Tick delta = 0;
};

namespace detail {

// A tick carried by the clip's own time law: a SampleLocked clip shifts the tick's FRAME (the automation stays
// locked to the audio across tempo changes); a TempoLocked clip shifts the tick (locked to the music).
[[nodiscard]] inline bool automationTickCarriedByClip (const CompiledTempoMap& tempoMap, const AutomationClipMove& move,
                                                       Tick tick, Tick& out) noexcept
{
    if (move.timeBase == TimeBase::TempoLocked)
    {
        out = tick + move.delta;
        return out >= 0;
    }
    double frame = 0.0;
    return tempoMap.frameForTick (tick, frame)
           && tempoMap.tickForFrame (frame + static_cast<double> (move.delta), out);
}

// The clip's span [first, last] in ticks (inclusive), before (`moved` false) or after its move.
[[nodiscard]] inline bool automationClipSpanTicks (const CompiledTempoMap& tempoMap, const AutomationClipMove& move,
                                                   bool moved, Tick& first, Tick& last) noexcept
{
    const Tick start = move.start + (moved ? move.delta : 0);
    const Tick end = start + move.length;
    if (move.length <= 0 || start < 0)
        return false;
    if (move.timeBase == TimeBase::TempoLocked)
    {
        first = start;
        last = end - 1;
        return true;
    }
    Tick endTick = 0;
    if (! tempoMap.tickForFrame (static_cast<double> (start), first) || ! tempoMap.tickForFrame (static_cast<double> (end), endTick))
        return false;
    last = endTick - 1;
    return last >= first;
}

} // namespace detail

// Moving clips (all on one track, in time only) carry the lane's automation in their spans: each clip's
// block — the lane's value at its span's first and last tick (so the curve inside the span travels whole)
// and every point between — is captured from the ORIGINAL lane, so clips moving together never pick up
// each other's points. Every vacated span is erased with edge anchors; every block then replaces its
// destination span, edges anchored (the span law): points already there are replaced, the curve outside
// keeps its values. An empty lane stays empty.
[[nodiscard]] inline AutomationSpanEditStatus moveAutomationWithClips (std::span<const AutomationBreakpoint> lane,
                                                                      std::span<const AutomationClipMove> moves,
                                                                      const CompiledTempoMap& tempoMap,
                                                                      std::vector<AutomationBreakpoint>& out)
{
    out.assign (lane.begin(), lane.end());
    if (lane.empty() || moves.empty())
        return AutomationSpanEditStatus::Ok;

    std::vector<std::int64_t> laneFrames;
    if (! detail::compileAutomationPointFrames (tempoMap, lane, laneFrames))
        return AutomationSpanEditStatus::Uncompilable;

    struct Block
    {
        Tick vacatedFirst = 0;
        Tick vacatedLast = 0;
        Tick destinationFirst = 0;
        Tick destinationLast = 0;
        std::vector<AutomationBreakpoint> points {};
    };
    std::vector<Block> blocks;
    for (const AutomationClipMove& move : moves)
    {
        Block block;
        if (! detail::automationClipSpanTicks (tempoMap, move, false, block.vacatedFirst, block.vacatedLast)
            || ! detail::automationClipSpanTicks (tempoMap, move, true, block.destinationFirst, block.destinationLast))
            return AutomationSpanEditStatus::InvalidInput;

        // The block in the lane's ORIGINAL values: its edges, then the points strictly inside.
        std::vector<AutomationBreakpoint> captured;
        const auto edge = [&] (Tick tick) {
            std::int64_t frame = 0;
            AutomationCurveType curve = AutomationCurveType::Linear;
            if (! compiledAutomationFrameForTick (tempoMap, tick, frame))
                return false;
            const double value = detail::automationValueAtCompiledFrame (lane, laneFrames, frame, curve);
            captured.push_back ({ tick, std::clamp (value, 0.0, 1.0), curve });
            return true;
        };
        if (! edge (block.vacatedFirst))
            return AutomationSpanEditStatus::InvalidInput;
        for (const AutomationBreakpoint& point : lane)
            if (point.tick > block.vacatedFirst && point.tick < block.vacatedLast)
                captured.push_back (point);
        if (block.vacatedLast > block.vacatedFirst && ! edge (block.vacatedLast))
            return AutomationSpanEditStatus::InvalidInput;

        // Carried by the clip's time law; a point carried onto an earlier point's tick or frame updates it.
        std::int64_t previousFrame = -1;
        for (const AutomationBreakpoint& point : captured)
        {
            AutomationBreakpoint carried = point;
            std::int64_t frame = 0;
            if (! detail::automationTickCarriedByClip (tempoMap, move, point.tick, carried.tick)
                || ! compiledAutomationFrameForTick (tempoMap, carried.tick, frame))
                return AutomationSpanEditStatus::InvalidInput;
            carried.tick = std::clamp (carried.tick, block.destinationFirst, block.destinationLast);
            if (! compiledAutomationFrameForTick (tempoMap, carried.tick, frame))
                return AutomationSpanEditStatus::InvalidInput;
            if (! block.points.empty() && (carried.tick <= block.points.back().tick || frame <= previousFrame))
                block.points.back().value = carried.value;
            else
                block.points.push_back (carried);
            previousFrame = frame;
        }
        blocks.push_back (std::move (block));
    }

    std::vector<AutomationBreakpoint> edited;
    for (const Block& block : blocks)
    {
        const AutomationSpanEditStatus status =
            replaceAutomationSpanBetween (out, block.vacatedFirst, block.vacatedLast, {}, tempoMap, edited);
        if (status != AutomationSpanEditStatus::Ok)
            return status;
        out.swap (edited);
    }
    for (const Block& block : blocks)
    {
        const AutomationSpanEditStatus status =
            replaceAutomationSpanBetween (out, block.destinationFirst, block.destinationLast, block.points, tempoMap, edited);
        if (status != AutomationSpanEditStatus::Ok)
            return status;
        out.swap (edited);
    }
    return AutomationSpanEditStatus::Ok;
}

} // namespace yesdaw::engine
