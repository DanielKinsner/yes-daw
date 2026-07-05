// YES DAW — headless checks for ADR-0009's generic, block-sliced EventStream.
//
// This deliberately proves only the first narrow flow: a shared EventStream carries fixed-size
// parameter changes, and FaderNode consumes its gain parameter at the exact in-Block offset.

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
#include <type_traits>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::AutomationBlock;
using yesdaw::engine::AutomationCurveType;
using yesdaw::engine::AutomationEvalStatus;
using yesdaw::engine::AutomationLane;
using yesdaw::engine::AutomationLaneCursor;
using yesdaw::engine::AutomationPoint;
using yesdaw::engine::AutomationTarget;
using yesdaw::engine::Event;
using yesdaw::engine::EventPayload;
using yesdaw::engine::EventStream;
using yesdaw::engine::FaderNode;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::Node;
using yesdaw::engine::ParameterChangePayload;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Tick;
using yesdaw::engine::Transport;
using yesdaw::engine::VoiceAddress;
using yesdaw::engine::evaluateAutomationLaneForBlock;
using yesdaw::engine::evaluateAutomationPointsForBlock;
using yesdaw::engine::kTicksPerQuarter;

namespace {

constexpr double kSr = 48000.0;

void processFaderBlock (FaderNode& node, std::vector<float>& block, EventStream& events)
{
    Node& iface = node;
    Transport transport;
    float* const channels[1] = { block.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 1 }, events, transport, static_cast<int> (block.size()) });
}

[[nodiscard]] auto linearTickToFrame (Tick origin, double framesPerTick) noexcept
{
    return [origin, framesPerTick] (Tick tick) noexcept {
        return (static_cast<double> (tick) - static_cast<double> (origin)) * framesPerTick;
    };
}

} // namespace

TEST_CASE ("Event is a fixed-size trivially-copyable value", "[event][shape]")
{
    STATIC_REQUIRE (std::is_trivially_copyable_v<VoiceAddress>);
    STATIC_REQUIRE (std::is_trivially_copyable_v<ParameterChangePayload>);
    STATIC_REQUIRE (std::is_trivially_copyable_v<EventPayload>);
    STATIC_REQUIRE (std::is_trivially_copyable_v<Event>);
    STATIC_REQUIRE (sizeof (Event) <= 64);
}

TEST_CASE ("Automation point storage locks ADR-0009 curve enum values", "[automation][shape]")
{
    STATIC_REQUIRE (std::is_trivially_copyable_v<AutomationPoint>);
    STATIC_REQUIRE (std::is_trivially_copyable_v<AutomationTarget>);
    STATIC_REQUIRE (std::is_trivially_copyable_v<AutomationBlock>);

    REQUIRE (static_cast<std::uint8_t> (AutomationCurveType::Linear) == 0u);
    REQUIRE (static_cast<std::uint8_t> (AutomationCurveType::Hold) == 1u);
    REQUIRE (static_cast<std::uint8_t> (AutomationCurveType::Bezier) == 2u);
    REQUIRE (static_cast<std::uint8_t> (AutomationCurveType::Log) == 3u);

    REQUIRE (AutomationPoint { 0, 0.0, AutomationCurveType::Linear }.isValid());
    REQUIRE (AutomationPoint { 0, 1.0, AutomationCurveType::Log }.isValid());
    REQUIRE_FALSE (AutomationPoint { 0, -0.01, AutomationCurveType::Hold }.isValid());
    REQUIRE_FALSE (AutomationPoint { 0, 1.01, AutomationCurveType::Bezier }.isValid());
    REQUIRE_FALSE (AutomationPoint { 0, std::numeric_limits<double>::quiet_NaN(), AutomationCurveType::Linear }.isValid());
}

TEST_CASE ("EventStream validates sorted half-open block offsets", "[event][block]")
{
    const std::array<Event, 2> sorted {
        makeParameterChangeEvent (0, 10, FaderNode::kGainParameterId, 0.25),
        makeParameterChangeEvent (7, 10, FaderNode::kGainParameterId, 0.75),
    };
    REQUIRE (EventStream (std::span<const Event> (sorted)).isValidForBlock (8));

    const std::array<Event, 1> boundary {
        makeParameterChangeEvent (8, 10, FaderNode::kGainParameterId, 0.25),
    };
    REQUIRE_FALSE (EventStream (std::span<const Event> (boundary)).isValidForBlock (8));

    const std::array<Event, 2> unsorted {
        makeParameterChangeEvent (6, 10, FaderNode::kGainParameterId, 0.25),
        makeParameterChangeEvent (5, 10, FaderNode::kGainParameterId, 0.75),
    };
    REQUIRE_FALSE (EventStream (std::span<const Event> (unsorted)).isValidForBlock (8));

    const std::array<Event, 1> nonFinite {
        makeParameterChangeEvent (3, 10, FaderNode::kGainParameterId, std::numeric_limits<double>::infinity()),
    };
    REQUIRE_FALSE (EventStream (std::span<const Event> (nonFinite)).isValidForBlock (8));
}

TEST_CASE ("Automation evaluator emits parameter events for one half-open block", "[automation][event]")
{
    const Tick blockStart = kTicksPerQuarter;
    const std::array<AutomationPoint, 4> points {
        AutomationPoint { blockStart - 2, 0.10, AutomationCurveType::Hold },
        AutomationPoint { blockStart, 0.25, AutomationCurveType::Linear },
        AutomationPoint { blockStart + 8, 0.50, AutomationCurveType::Bezier },
        AutomationPoint { blockStart + 16, 0.75, AutomationCurveType::Log },
    };

    std::array<Event, 4> out {};
    const auto first = evaluateAutomationPointsForBlock (
        std::span<const AutomationPoint> (points),
        0,
        AutomationTarget { 10, FaderNode::kGainParameterId },
        AutomationBlock { 0.0, 8 },
        linearTickToFrame (blockStart, 0.5),
        std::span<Event> (out));

    REQUIRE (first.status == AutomationEvalStatus::Ok);
    REQUIRE (first.eventsWritten == 2u);
    REQUIRE (first.nextPointIndex == 3u); // the exact end-boundary point belongs to the next Block

    REQUIRE (out[0].timeInBlock == 0u);
    REQUIRE (out[0].payload.parameter.targetNode == 10u);
    REQUIRE (out[0].payload.parameter.parameterId == FaderNode::kGainParameterId);
    REQUIRE (out[0].payload.parameter.normalizedValue == 0.25);

    REQUIRE (out[1].timeInBlock == 4u);
    REQUIRE (out[1].payload.parameter.normalizedValue == 0.50);

    EventStream firstStream { std::span<const Event> (out.data(), first.eventsWritten) };
    REQUIRE (firstStream.isValidForBlock (8));

    std::array<Event, 2> secondOut {};
    const auto second = evaluateAutomationPointsForBlock (
        std::span<const AutomationPoint> (points),
        first.nextPointIndex,
        AutomationTarget { 10, FaderNode::kGainParameterId },
        AutomationBlock { 8.0, 8 },
        linearTickToFrame (blockStart, 0.5),
        std::span<Event> (secondOut));

    REQUIRE (second.status == AutomationEvalStatus::Ok);
    REQUIRE (second.eventsWritten == 1u);
    REQUIRE (second.nextPointIndex == points.size());
    REQUIRE (secondOut[0].timeInBlock == 0u);
    REQUIRE (secondOut[0].payload.parameter.normalizedValue == 0.75);
}

TEST_CASE ("Automation lane interpolates curves and applies PDC shift", "[automation][lane][curve][pdc]")
{
    auto valueAtShiftedFrame = [] (AutomationCurveType curve, std::uint32_t shiftedFrame)
    {
        AutomationLane lane;
        lane.target = AutomationTarget { 10, FaderNode::kGainParameterId };
        lane.points = {
            AutomationPoint { 0, 0.0, curve },
            AutomationPoint { 4, 1.0, AutomationCurveType::Hold },
        };

        AutomationLaneCursor cursor;
        std::array<Event, 8> out {};
        const auto result = evaluateAutomationLaneForBlock (
            lane,
            cursor,
            AutomationBlock { 0.0, 8, 2.0 },
            linearTickToFrame (0, 1.0),
            std::span<Event> (out));

        REQUIRE (result.status == AutomationEvalStatus::Ok);
        REQUIRE (result.eventsWritten == 8u);
        REQUIRE (result.nextPointIndex == lane.points.size());
        REQUIRE (cursor.initialized);

        for (std::size_t i = 0; i < result.eventsWritten; ++i)
            if (out[i].timeInBlock == shiftedFrame)
                return out[i].payload.parameter.normalizedValue;

        FAIL ("expected shifted automation event was not emitted");
        return -1.0;
    };

    // The first generated event is shifted by +2 frames from tick/frame 0.
    REQUIRE (valueAtShiftedFrame (AutomationCurveType::Linear, 2) == 0.0);

    // At shifted frame 3 the source-frame progress is 25%. These distinct values prove the evaluator
    // reads AutomationCurveType instead of treating every segment as point-only or linear.
    const double hold = valueAtShiftedFrame (AutomationCurveType::Hold, 3);
    const double linear = valueAtShiftedFrame (AutomationCurveType::Linear, 3);
    const double bezier = valueAtShiftedFrame (AutomationCurveType::Bezier, 3);
    const double log = valueAtShiftedFrame (AutomationCurveType::Log, 3);

    REQUIRE (hold == 0.0);
    REQUIRE (std::abs (linear - 0.25) < 1.0e-12);
    REQUIRE (std::abs (bezier - 0.15625) < 1.0e-12);
    REQUIRE (log > linear);
    REQUIRE (log < 1.0);
}

TEST_CASE ("Automation lane cursor re-seeks on loop or seek and rejects non-finite points",
           "[automation][lane][cursor][robust]")
{
    AutomationLane lane;
    lane.target = AutomationTarget { 10, FaderNode::kGainParameterId };
    lane.points = {
        AutomationPoint { 0, 0.0, AutomationCurveType::Linear },
        AutomationPoint { 4, 1.0, AutomationCurveType::Linear },
    };

    AutomationLaneCursor cursor;
    std::array<Event, 8> out {};
    const auto later = evaluateAutomationLaneForBlock (
        lane,
        cursor,
        AutomationBlock { 4.0, 4, 0.0 },
        linearTickToFrame (0, 1.0),
        std::span<Event> (out));
    REQUIRE (later.status == AutomationEvalStatus::Ok);
    REQUIRE (later.eventsWritten == 4u);

    const auto looped = evaluateAutomationLaneForBlock (
        lane,
        cursor,
        AutomationBlock { 0.0, 4, 0.0 },
        linearTickToFrame (0, 1.0),
        std::span<Event> (out));
    REQUIRE (looped.status == AutomationEvalStatus::Ok);
    REQUIRE (looped.eventsWritten == 4u);
    REQUIRE (out[0].timeInBlock == 0u);
    REQUIRE (out[0].payload.parameter.normalizedValue == 0.0);

    AutomationLane invalid;
    invalid.target = lane.target;
    invalid.points = {
        AutomationPoint { 0, std::numeric_limits<double>::infinity(), AutomationCurveType::Linear },
        AutomationPoint { 4, 1.0, AutomationCurveType::Linear },
    };

    AutomationLaneCursor invalidCursor;
    std::array<Event, 2> invalidOut {};
    const auto invalidResult = evaluateAutomationLaneForBlock (
        invalid,
        invalidCursor,
        AutomationBlock { 0.0, 4, 0.0 },
        linearTickToFrame (0, 1.0),
        std::span<Event> (invalidOut));
    REQUIRE (invalidResult.status == AutomationEvalStatus::InvalidInput);
    REQUIRE (invalidResult.eventsWritten == 0u);
}

TEST_CASE ("Automation evaluator reports invalid inputs before writing past fixed storage", "[automation][event]")
{
    const Tick blockStart = kTicksPerQuarter * 2;
    const std::array<AutomationPoint, 2> unsorted {
        AutomationPoint { blockStart - 1, 0.25, AutomationCurveType::Linear },
        AutomationPoint { blockStart - 2, 0.50, AutomationCurveType::Linear },
    };
    std::array<Event, 2> out {};

    const auto unsortedResult = evaluateAutomationPointsForBlock (
        std::span<const AutomationPoint> (unsorted),
        0,
        AutomationTarget { 10, FaderNode::kGainParameterId },
        AutomationBlock { 0.0, 8 },
        linearTickToFrame (blockStart, 1.0),
        std::span<Event> (out));
    REQUIRE (unsortedResult.status == AutomationEvalStatus::UnsortedPoints);
    REQUIRE (unsortedResult.eventsWritten == 0u);

    const std::array<AutomationPoint, 1> invalid {
        AutomationPoint { blockStart, std::numeric_limits<double>::infinity(), AutomationCurveType::Hold },
    };
    const auto invalidResult = evaluateAutomationPointsForBlock (
        std::span<const AutomationPoint> (invalid),
        0,
        AutomationTarget { 10, FaderNode::kGainParameterId },
        AutomationBlock { 0.0, 8 },
        linearTickToFrame (blockStart, 1.0),
        std::span<Event> (out));
    REQUIRE (invalidResult.status == AutomationEvalStatus::InvalidInput);
    REQUIRE (invalidResult.eventsWritten == 0u);

    const std::array<AutomationPoint, 2> tooMany {
        AutomationPoint { blockStart, 0.25, AutomationCurveType::Linear },
        AutomationPoint { blockStart + 1, 0.50, AutomationCurveType::Linear },
    };
    std::array<Event, 1> tinyOut {};
    const auto tinyResult = evaluateAutomationPointsForBlock (
        std::span<const AutomationPoint> (tooMany),
        0,
        AutomationTarget { 10, FaderNode::kGainParameterId },
        AutomationBlock { 0.0, 8 },
        linearTickToFrame (blockStart, 1.0),
        std::span<Event> (tinyOut));
    REQUIRE (tinyResult.status == AutomationEvalStatus::OutputTooSmall);
    REQUIRE (tinyResult.eventsWritten == 1u);
    REQUIRE (tinyResult.nextPointIndex == 1u);
}

TEST_CASE ("FaderNode consumes generated automation events at exact in-block offsets", "[automation][fader]")
{
    const Tick blockStart = kTicksPerQuarter * 3;
    const std::array<AutomationPoint, 1> points {
        AutomationPoint { blockStart + 4, 0.0, AutomationCurveType::Hold },
    };

    std::array<Event, 1> events {};
    const auto result = evaluateAutomationPointsForBlock (
        std::span<const AutomationPoint> (points),
        0,
        AutomationTarget { 10, FaderNode::kGainParameterId },
        AutomationBlock { 0.0, 8 },
        linearTickToFrame (blockStart, 1.0),
        std::span<Event> (events));
    REQUIRE (result.status == AutomationEvalStatus::Ok);
    REQUIRE (result.eventsWritten == 1u);

    EventStream stream { std::span<const Event> (events.data(), result.eventsWritten) };
    REQUIRE (stream.isValidForBlock (8));

    FaderNode node (10, 1);
    Node& iface = node;
    iface.prepare (kSr, 16);

    std::vector<float> block (8, 1.0f);
    processFaderBlock (node, block, stream);

    REQUIRE (block[0] == 1.0f);
    REQUIRE (block[1] == 1.0f);
    REQUIRE (block[2] == 1.0f);
    REQUIRE (block[3] == 1.0f);
    REQUIRE (block[4] < 1.0f);
    REQUIRE (block[5] < block[4]);
    REQUIRE (block[6] < block[5]);
    REQUIRE (block[7] < block[6]);
}

TEST_CASE ("FaderNode consumes gain parameter events at exact in-block offsets", "[event][parameter][fader]")
{
    FaderNode node (10, 1);
    Node& iface = node;
    iface.prepare (kSr, 16);

    const std::array<Event, 2> events {
        makeParameterChangeEvent (2, 99, FaderNode::kGainParameterId, 0.0),
        makeParameterChangeEvent (4, 10, FaderNode::kGainParameterId, 0.0),
    };
    EventStream stream { std::span<const Event> (events) };
    REQUIRE (stream.isValidForBlock (8));

    std::vector<float> block (8, 1.0f);
    processFaderBlock (node, block, stream);

    REQUIRE (block[0] == 1.0f);
    REQUIRE (block[1] == 1.0f);
    REQUIRE (block[2] == 1.0f); // event for another Node is ignored
    REQUIRE (block[3] == 1.0f);
    REQUIRE (block[4] < 1.0f);
    REQUIRE (block[5] < block[4]);
    REQUIRE (block[6] < block[5]);
    REQUIRE (block[7] < block[6]);
}

TEST_CASE ("FaderNode keeps an event target across the next block", "[event][parameter][fader]")
{
    FaderNode node (10, 1);
    Node& iface = node;
    iface.prepare (kSr, 32);

    const std::array<Event, 1> events {
        makeParameterChangeEvent (0, 10, FaderNode::kGainParameterId, 0.0),
    };
    EventStream firstStream { std::span<const Event> (events) };
    REQUIRE (firstStream.isValidForBlock (16));

    std::vector<float> first (16, 1.0f);
    processFaderBlock (node, first, firstStream);

    EventStream empty;
    std::vector<float> second (16, 1.0f);
    processFaderBlock (node, second, empty);

    REQUIRE (first.front() < 1.0f);
    REQUIRE (first.back() > 0.0f);
    REQUIRE (second.front() < first.back());
    REQUIRE (second.back() < second.front());
    for (const float v : second)
        REQUIRE (std::isfinite (v));
}

TEST_CASE ("FaderNode SetGain command can override a previous event target", "[event][parameter][fader]")
{
    FaderNode node (10, 1);
    Node& iface = node;
    iface.prepare (kSr, 32);

    const std::array<Event, 1> events {
        makeParameterChangeEvent (0, 10, FaderNode::kGainParameterId, 0.0),
    };
    EventStream firstStream { std::span<const Event> (events) };

    std::vector<float> first (16, 1.0f);
    processFaderBlock (node, first, firstStream);
    REQUIRE (first.back() < first.front());

    node.setTargetGain (1.0f); // same as the original command target, but different from the event target

    EventStream empty;
    std::vector<float> second (16, 1.0f);
    processFaderBlock (node, second, empty);

    REQUIRE (second.front() > first.back());
    REQUIRE (second.back() > second.front());
    REQUIRE (second.back() <= 1.0f);
}
