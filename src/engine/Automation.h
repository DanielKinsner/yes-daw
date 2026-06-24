// YES DAW - automation value surface and first block evaluator slice (ADR-0009).
//
// This is deliberately narrow: stored automation points become fixed-size parameter Events for one
// target parameter over one half-open Block. Storage/persistence, lane ownership, and curve-segment
// interpolation stay out of this slice.

#pragma once

#include "engine/Node.h"
#include "engine/Time.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

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

    [[nodiscard]] bool isValid() const noexcept
    {
        return std::isfinite (startFrame);
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

static_assert (std::is_trivially_copyable_v<AutomationPoint>, "Automation points must stay storage-flat");
static_assert (std::is_trivially_copyable_v<AutomationTarget>, "Automation target must stay storage-flat");
static_assert (std::is_trivially_copyable_v<AutomationBlock>, "Automation block must stay flat");

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
