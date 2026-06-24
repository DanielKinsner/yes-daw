// YES DAW — headless checks for ADR-0009's generic, block-sliced EventStream.
//
// This deliberately proves only the first narrow flow: a shared EventStream carries fixed-size
// parameter changes, and FaderNode consumes its gain parameter at the exact in-Block offset.

#include "engine/Node.h"
#include "engine/nodes/FaderNode.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <type_traits>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::Event;
using yesdaw::engine::EventPayload;
using yesdaw::engine::EventStream;
using yesdaw::engine::FaderNode;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::Node;
using yesdaw::engine::ParameterChangePayload;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Transport;
using yesdaw::engine::VoiceAddress;

namespace {

constexpr double kSr = 48000.0;

void processFaderBlock (FaderNode& node, std::vector<float>& block, EventStream& events)
{
    Node& iface = node;
    Transport transport;
    float* const channels[1] = { block.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 1 }, events, transport, static_cast<int> (block.size()) });
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
