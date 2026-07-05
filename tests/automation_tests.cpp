// YES DAW - H15 CP0 automation evaluator characterization gate.
//
// This locks the existing Automation.h evaluator before H15 builds storage, undo, graph compilation,
// or runtime delivery on top of it.

#include "engine/Automation.h"
#include "engine/Node.h"
#include "engine/nodes/FaderNode.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

using yesdaw::engine::AutomationBlock;
using yesdaw::engine::AutomationCurveType;
using yesdaw::engine::AutomationEvalStatus;
using yesdaw::engine::AutomationLane;
using yesdaw::engine::AutomationLaneCursor;
using yesdaw::engine::AutomationPoint;
using yesdaw::engine::AutomationTarget;
using yesdaw::engine::Event;
using yesdaw::engine::FaderNode;
using yesdaw::engine::Tick;
using yesdaw::engine::evaluateAutomationLaneForBlock;
using yesdaw::engine::evaluateAutomationPointsForBlock;

namespace {

constexpr yesdaw::engine::NodeId      kNode = 21;
constexpr yesdaw::engine::ParameterId kParam = FaderNode::kGainParameterId;
constexpr double                      kEpsilon = 1.0e-12;

struct EmittedValue
{
    std::uint32_t timeInBlock = 0;
    double        value = 0.0;
};

struct AbsoluteValue
{
    std::uint32_t frame = 0;
    double        value = 0.0;
};

[[nodiscard]] auto identityTickToFrame() noexcept
{
    return [] (Tick tick) noexcept { return static_cast<double> (tick); };
}

[[nodiscard]] AutomationLane makeLane (std::vector<AutomationPoint> points)
{
    AutomationLane lane;
    lane.target = AutomationTarget { kNode, kParam };
    lane.points = std::move (points);
    return lane;
}

[[nodiscard]] std::vector<EmittedValue> evaluateBlock (const AutomationLane& lane,
                                                       AutomationLaneCursor& cursor,
                                                       AutomationBlock block)
{
    std::array<Event, 64> events {};
    const auto result = evaluateAutomationLaneForBlock (
        lane, cursor, block, identityTickToFrame(), std::span<Event> (events));

    REQUIRE (result.status == AutomationEvalStatus::Ok);

    std::vector<EmittedValue> values;
    values.reserve (result.eventsWritten);
    for (std::size_t i = 0; i < result.eventsWritten; ++i)
    {
        REQUIRE (events[i].payload.parameter.targetNode == kNode);
        REQUIRE (events[i].payload.parameter.parameterId == kParam);
        values.push_back ({ events[i].timeInBlock, events[i].payload.parameter.normalizedValue });
    }
    return values;
}

[[nodiscard]] std::vector<EmittedValue> evaluateFreshBlock (const AutomationLane& lane,
                                                            AutomationBlock block)
{
    AutomationLaneCursor cursor;
    return evaluateBlock (lane, cursor, block);
}

[[nodiscard]] std::vector<AbsoluteValue> toAbsolute (double blockStart,
                                                     const std::vector<EmittedValue>& values)
{
    std::vector<AbsoluteValue> absolute;
    absolute.reserve (values.size());
    for (const EmittedValue& value : values)
        absolute.push_back ({ static_cast<std::uint32_t> (blockStart) + value.timeInBlock, value.value });
    return absolute;
}

void appendAbsolute (std::vector<AbsoluteValue>& out,
                     double blockStart,
                     const std::vector<EmittedValue>& values)
{
    std::vector<AbsoluteValue> absolute = toAbsolute (blockStart, values);
    out.insert (out.end(), absolute.begin(), absolute.end());
}

void requireClose (double actual, double expected)
{
    REQUIRE (std::abs (actual - expected) < kEpsilon);
}

void requireSameAbsoluteValues (const std::vector<AbsoluteValue>& actual,
                                const std::vector<AbsoluteValue>& expected)
{
    REQUIRE (actual.size() == expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        REQUIRE (actual[i].frame == expected[i].frame);
        requireClose (actual[i].value, expected[i].value);
    }
}

[[nodiscard]] bool sameAbsoluteValues (const std::vector<AbsoluteValue>& actual,
                                       const std::vector<AbsoluteValue>& expected)
{
    if (actual.size() != expected.size())
        return false;

    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        if (actual[i].frame != expected[i].frame)
            return false;
        if (std::abs (actual[i].value - expected[i].value) >= kEpsilon)
            return false;
    }
    return true;
}

} // namespace

TEST_CASE ("YesDawAutomationCheck characterizes Linear and Hold half-open block slicing",
           "[automation][h15][characterization]")
{
    const AutomationLane lane = makeLane ({
        AutomationPoint { 10, 0.20, AutomationCurveType::Linear },
        AutomationPoint { 14, 0.60, AutomationCurveType::Hold },
        AutomationPoint { 18, 0.90, AutomationCurveType::Linear },
    });

    const std::vector<AbsoluteValue> whole =
        toAbsolute (8.0, evaluateFreshBlock (lane, AutomationBlock { 8.0, 12, 0.0 }));

    const std::array<double, 12> expected {
        0.20, 0.20, 0.20, 0.30, 0.40, 0.50, 0.60, 0.60, 0.60, 0.60, 0.90, 0.90,
    };
    REQUIRE (whole.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        REQUIRE (whole[i].frame == 8u + static_cast<std::uint32_t> (i));
        requireClose (whole[i].value, expected[i]);
    }

    std::vector<AbsoluteValue> split;
    appendAbsolute (split, 8.0, evaluateFreshBlock (lane, AutomationBlock { 8.0, 6, 0.0 }));
    appendAbsolute (split, 14.0, evaluateFreshBlock (lane, AutomationBlock { 14.0, 6, 0.0 }));
    requireSameAbsoluteValues (split, whole);

    std::vector<AbsoluteValue> shiftedAtBoundary = split;
    REQUIRE (shiftedAtBoundary.size() > 6u);
    shiftedAtBoundary[6].frame += 1u;
    REQUIRE_FALSE (sameAbsoluteValues (shiftedAtBoundary, whole));
}

TEST_CASE ("YesDawAutomationCheck clamps before the first and after the last breakpoint",
           "[automation][h15][characterization]")
{
    const AutomationLane lane = makeLane ({
        AutomationPoint { 4, 0.25, AutomationCurveType::Linear },
        AutomationPoint { 6, 0.75, AutomationCurveType::Linear },
    });

    const std::vector<EmittedValue> values =
        evaluateFreshBlock (lane, AutomationBlock { 0.0, 10, 0.0 });

    REQUIRE (values.size() == 10u);
    for (std::uint32_t frame = 0; frame < 4; ++frame)
    {
        REQUIRE (values[frame].timeInBlock == frame);
        requireClose (values[frame].value, 0.25);
    }

    requireClose (values[4].value, 0.25);
    requireClose (values[5].value, 0.50);
    for (std::uint32_t frame = 6; frame < 10; ++frame)
    {
        REQUIRE (values[frame].timeInBlock == frame);
        requireClose (values[frame].value, 0.75);
    }
}

TEST_CASE ("YesDawAutomationCheck cursor reuse and locate reseek match fresh evaluation",
           "[automation][h15][cursor]")
{
    const AutomationLane lane = makeLane ({
        AutomationPoint { 3, 0.10, AutomationCurveType::Linear },
        AutomationPoint { 7, 0.90, AutomationCurveType::Linear },
    });

    AutomationLaneCursor cursor;
    for (const double start : { 0.0, 4.0, 8.0 })
    {
        const std::vector<EmittedValue> reused =
            evaluateBlock (lane, cursor, AutomationBlock { start, 4, 0.0 });
        const std::vector<EmittedValue> fresh =
            evaluateFreshBlock (lane, AutomationBlock { start, 4, 0.0 });
        REQUIRE (reused.size() == fresh.size());
        for (std::size_t i = 0; i < reused.size(); ++i)
        {
            REQUIRE (reused[i].timeInBlock == fresh[i].timeInBlock);
            requireClose (reused[i].value, fresh[i].value);
        }
    }

    const std::vector<EmittedValue> located =
        evaluateBlock (lane, cursor, AutomationBlock { 2.0, 4, 0.0 });
    const std::vector<EmittedValue> freshLocated =
        evaluateFreshBlock (lane, AutomationBlock { 2.0, 4, 0.0 });
    REQUIRE (located.size() == freshLocated.size());
    for (std::size_t i = 0; i < located.size(); ++i)
    {
        REQUIRE (located[i].timeInBlock == freshLocated[i].timeInBlock);
        requireClose (located[i].value, freshLocated[i].value);
    }
}

TEST_CASE ("YesDawAutomationCheck rejects and documents hostile point inputs",
           "[automation][h15][robust]")
{
    const std::array<AutomationPoint, 2> unsorted {
        AutomationPoint { 4, 0.25, AutomationCurveType::Linear },
        AutomationPoint { 3, 0.50, AutomationCurveType::Linear },
    };
    std::array<Event, 8> out {};
    const auto unsortedResult = evaluateAutomationPointsForBlock (
        std::span<const AutomationPoint> (unsorted),
        0,
        AutomationTarget { kNode, kParam },
        AutomationBlock { 0.0, 8, 0.0 },
        identityTickToFrame(),
        std::span<Event> (out));
    REQUIRE (unsortedResult.status == AutomationEvalStatus::UnsortedPoints);
    REQUIRE (unsortedResult.eventsWritten == 1u);
    REQUIRE (unsortedResult.nextPointIndex == 1u);

    AutomationLane unsortedLane = makeLane ({
        AutomationPoint { 4, 0.25, AutomationCurveType::Linear },
        AutomationPoint { 3, 0.50, AutomationCurveType::Linear },
    });
    AutomationLaneCursor unsortedCursor;
    const auto unsortedLaneResult = evaluateAutomationLaneForBlock (
        unsortedLane,
        unsortedCursor,
        AutomationBlock { 0.0, 8, 0.0 },
        identityTickToFrame(),
        std::span<Event> (out));
    REQUIRE (unsortedLaneResult.status == AutomationEvalStatus::UnsortedPoints);
    REQUIRE (unsortedLaneResult.eventsWritten == 0u);

    AutomationLane invalid = makeLane ({
        AutomationPoint { 0, std::numeric_limits<double>::quiet_NaN(), AutomationCurveType::Linear },
        AutomationPoint { 4, 1.0, AutomationCurveType::Linear },
    });
    AutomationLaneCursor cursor;
    const auto invalidResult = evaluateAutomationLaneForBlock (
        invalid,
        cursor,
        AutomationBlock { 0.0, 8, 0.0 },
        identityTickToFrame(),
        std::span<Event> (out));
    REQUIRE (invalidResult.status == AutomationEvalStatus::InvalidInput);
    REQUIRE (invalidResult.eventsWritten == 0u);
}

TEST_CASE ("YesDawAutomationCheck records Bezier and Log evaluator behavior as storage-quarantined",
           "[automation][h15][curve]")
{
    auto valueAtFrame = [] (AutomationCurveType curve, std::uint32_t frame)
    {
        const AutomationLane lane = makeLane ({
            AutomationPoint { 0, 0.0, curve },
            AutomationPoint { 4, 1.0, AutomationCurveType::Hold },
        });
        const std::vector<EmittedValue> values =
            evaluateFreshBlock (lane, AutomationBlock { 0.0, 5, 0.0 });
        REQUIRE (values.size() == 5u);
        return values[frame].value;
    };

    requireClose (valueAtFrame (AutomationCurveType::Bezier, 1), 0.15625);
    requireClose (valueAtFrame (AutomationCurveType::Bezier, 2), 0.50);

    const double logQuarter = valueAtFrame (AutomationCurveType::Log, 1);
    const double logHalf = valueAtFrame (AutomationCurveType::Log, 2);
    REQUIRE (logQuarter > 0.25);
    REQUIRE (logHalf > logQuarter);
    REQUIRE (logHalf < 1.0);
}
