// YES DAW - automation value surface and first block evaluator slice (ADR-0009).
//
// This is deliberately narrow: stored automation points become fixed-size parameter Events for one
// target parameter over one half-open Block. The item-5 H3 gate adds the first owned lane/cursor surface
// and curve interpolation needed to feed hosted PluginNode parameters through the Event stream.

#pragma once

#include "engine/Node.h"
#include "engine/Time.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace yesdaw::engine {

enum class AutomationCurveType : std::uint8_t
{
    Linear = 0,
    Hold   = 1,
    Bezier = 2,
    Log    = 3
};

struct AutomationPoint
{
    Tick                tick = 0;
    double              value = 0.0;
    AutomationCurveType curveType = AutomationCurveType::Linear;

    [[nodiscard]] bool isValid() const noexcept
    {
        return std::isfinite (value) && value >= 0.0 && value <= 1.0;
    }
};

struct AutomationTarget
{
    NodeId      targetNode = 0;
    ParameterId parameterId = 0;
};

struct AutomationBlock
{
    double        startFrame = 0.0;
    std::uint32_t numFrames = 0;
    double        pdcShiftFrames = 0.0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return std::isfinite (startFrame) && std::isfinite (pdcShiftFrames);
    }
};

enum class AutomationEvalStatus : std::uint8_t
{
    Ok = 0,
    InvalidInput,
    UnsortedPoints,
    OutputTooSmall
};

struct AutomationEvalResult
{
    AutomationEvalStatus status = AutomationEvalStatus::Ok;
    std::size_t          eventsWritten = 0;
    std::size_t          nextPointIndex = 0;
};

struct AutomationLane
{
    AutomationTarget                target;
    std::vector<AutomationPoint>    points;
};

struct AutomationLaneCursor
{
    std::size_t nextPointIndex = 0;
    double      lastSourceBlockStart = 0.0;
    bool        initialized = false;
};

static_assert (std::is_trivially_copyable_v<AutomationPoint>, "Automation points must stay storage-flat");
static_assert (std::is_trivially_copyable_v<AutomationTarget>, "Automation target must stay storage-flat");
static_assert (std::is_trivially_copyable_v<AutomationBlock>, "Automation block must stay flat");
static_assert (std::is_trivially_copyable_v<AutomationLaneCursor>, "Automation cursors must stay storage-flat");

[[nodiscard]] inline double automationCurveProgress (AutomationCurveType curve, double t) noexcept
{
    if (! std::isfinite (t))
        return 0.0;

    if (t <= 0.0)
        return 0.0;
    if (t >= 1.0)
        return 1.0;

    switch (curve)
    {
        case AutomationCurveType::Hold:
            return 0.0;

        case AutomationCurveType::Linear:
            return t;

        case AutomationCurveType::Bezier:
            return t * t * (3.0 - 2.0 * t);

        case AutomationCurveType::Log:
            return std::log1p (9.0 * t) / std::log1p (9.0);
    }

    return t;
}

[[nodiscard]] inline double interpolateAutomationValue (const AutomationPoint& a,
                                                       const AutomationPoint& b,
                                                       double frameA,
                                                       double frameB,
                                                       double frame) noexcept
{
    if (frameB <= frameA)
        return b.value;

    const double t = (frame - frameA) / (frameB - frameA);
    const double u = automationCurveProgress (a.curveType, t);
    return a.value + (b.value - a.value) * u;
}

template <typename TickToFrame>
[[nodiscard]] inline AutomationEvalStatus validateAutomationPoints (
    std::span<const AutomationPoint> points,
    TickToFrame tickToFrame) noexcept
{
    Tick previousTick = 0;
    double previousFrame = 0.0;
    bool havePrevious = false;

    for (const AutomationPoint& point : points)
    {
        if (havePrevious && point.tick < previousTick)
            return AutomationEvalStatus::UnsortedPoints;

        if (! point.isValid())
            return AutomationEvalStatus::InvalidInput;

        const double frame = tickToFrame (point.tick);
        if (! std::isfinite (frame) || (havePrevious && frame < previousFrame))
            return AutomationEvalStatus::InvalidInput;

        previousTick = point.tick;
        previousFrame = frame;
        havePrevious = true;
    }

    return AutomationEvalStatus::Ok;
}

template <typename TickToFrame>
[[nodiscard]] inline std::size_t seekAutomationPointIndexForFrame (
    std::span<const AutomationPoint> points,
    double sourceFrame,
    TickToFrame tickToFrame) noexcept
{
    std::size_t i = 0;
    while (i < points.size() && tickToFrame (points[i].tick) < sourceFrame)
        ++i;
    return i;
}

template <typename TickToFrame>
[[nodiscard]] inline AutomationEvalResult evaluateAutomationLaneForBlock (
    const AutomationLane& lane,
    AutomationLaneCursor& cursor,
    AutomationBlock block,
    TickToFrame tickToFrame,
    std::span<Event> outEvents) noexcept
{
    AutomationEvalResult result;

    if (! block.isValid())
    {
        result.status = AutomationEvalStatus::InvalidInput;
        result.nextPointIndex = cursor.nextPointIndex;
        return result;
    }

    const std::span<const AutomationPoint> points (lane.points.data(), lane.points.size());
    result.status = validateAutomationPoints (points, tickToFrame);
    if (result.status != AutomationEvalStatus::Ok || points.empty())
    {
        result.nextPointIndex = cursor.nextPointIndex;
        return result;
    }

    const double sourceBlockStart = block.startFrame - block.pdcShiftFrames;
    if (! cursor.initialized || sourceBlockStart < cursor.lastSourceBlockStart
        || cursor.nextPointIndex > points.size())
    {
        cursor.nextPointIndex = seekAutomationPointIndexForFrame (points, sourceBlockStart, tickToFrame);
    }

    const double firstFrame = tickToFrame (points.front().tick);
    const double lastFrame  = tickToFrame (points.back().tick);

    std::size_t segment = cursor.nextPointIndex > 0 ? cursor.nextPointIndex - 1u : 0u;
    if (segment >= points.size())
        segment = points.size() - 1u;

    for (std::uint32_t frameOffset = 0; frameOffset < block.numFrames; ++frameOffset)
    {
        const double sourceFrame = block.startFrame + static_cast<double> (frameOffset) - block.pdcShiftFrames;
        if (sourceFrame < firstFrame || sourceFrame > lastFrame)
            continue;

        while (segment + 1u < points.size()
               && tickToFrame (points[segment + 1u].tick) <= sourceFrame)
        {
            ++segment;
        }

        double value = points[segment].value;
        if (segment + 1u < points.size())
        {
            value = interpolateAutomationValue (points[segment],
                                                points[segment + 1u],
                                                tickToFrame (points[segment].tick),
                                                tickToFrame (points[segment + 1u].tick),
                                                sourceFrame);
        }

        if (! std::isfinite (value) || value < 0.0 || value > 1.0)
        {
            result.status = AutomationEvalStatus::InvalidInput;
            result.nextPointIndex = cursor.nextPointIndex;
            return result;
        }

        if (result.eventsWritten >= outEvents.size())
        {
            result.status = AutomationEvalStatus::OutputTooSmall;
            result.nextPointIndex = cursor.nextPointIndex;
            return result;
        }

        outEvents[result.eventsWritten] = makeParameterChangeEvent (frameOffset,
                                                                    lane.target.targetNode,
                                                                    lane.target.parameterId,
                                                                    value);
        ++result.eventsWritten;
    }

    const double shiftedBlockEnd = block.startFrame + static_cast<double> (block.numFrames);
    std::size_t next = cursor.nextPointIndex;
    while (next < points.size() && tickToFrame (points[next].tick) + block.pdcShiftFrames < shiftedBlockEnd)
        ++next;

    cursor.nextPointIndex = next;
    cursor.lastSourceBlockStart = sourceBlockStart;
    cursor.initialized = true;
    result.nextPointIndex = cursor.nextPointIndex;
    return result;
}

template <typename TickToFrame>
[[nodiscard]] inline AutomationEvalResult evaluateAutomationPointsForBlock (
    std::span<const AutomationPoint> points,
    std::size_t startPointIndex,
    AutomationTarget target,
    AutomationBlock block,
    TickToFrame tickToFrame,
    std::span<Event> outEvents) noexcept
{
    AutomationEvalResult result;
    result.nextPointIndex = startPointIndex;

    if (! block.isValid() || startPointIndex > points.size())
    {
        result.status = AutomationEvalStatus::InvalidInput;
        return result;
    }

    Tick previousTick = startPointIndex > 0 ? points[startPointIndex - 1].tick : 0;
    bool havePrevious = startPointIndex > 0;

    for (std::size_t i = startPointIndex; i < points.size(); ++i)
    {
        const AutomationPoint& point = points[i];

        if (havePrevious && point.tick < previousTick)
        {
            result.status = AutomationEvalStatus::UnsortedPoints;
            result.nextPointIndex = i;
            return result;
        }

        if (! point.isValid())
        {
            result.status = AutomationEvalStatus::InvalidInput;
            result.nextPointIndex = i;
            return result;
        }

        previousTick = point.tick;
        havePrevious = true;

        const double frame = tickToFrame (point.tick) - block.startFrame;

        if (! std::isfinite (frame))
        {
            result.status = AutomationEvalStatus::InvalidInput;
            result.nextPointIndex = i;
            return result;
        }

        if (frame < 0.0)
        {
            result.nextPointIndex = i + 1;
            continue;
        }

        if (frame >= static_cast<double> (block.numFrames))
        {
            result.nextPointIndex = i;
            return result;
        }

        if (result.eventsWritten >= outEvents.size())
        {
            result.status = AutomationEvalStatus::OutputTooSmall;
            result.nextPointIndex = i;
            return result;
        }

        const auto timeInBlock = static_cast<std::uint32_t> (std::floor (frame));
        outEvents[result.eventsWritten] = makeParameterChangeEvent (timeInBlock,
                                                                    target.targetNode,
                                                                    target.parameterId,
                                                                    point.value);
        ++result.eventsWritten;
        result.nextPointIndex = i + 1;
    }

    return result;
}

} // namespace yesdaw::engine
