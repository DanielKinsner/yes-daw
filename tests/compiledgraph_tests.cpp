// YES DAW — headless checks for the CompiledGraph stub (ADR-0006/0007 foundation).
//
// Pure C++ + Catch2, no JUCE. Proves the immutable graph renders its observable identity, and that the
// liveness counter tracks construction/destruction (the leak gate the reclamation tests rely on). Runs
// on the normal matrix AND the RTSan/TSan legs.

#include "engine/CompiledGraph.h"
#include "engine/nodes/DelayNode.h"
#include "engine/nodes/SumNode.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <span>
#include <vector>

using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNode;
using yesdaw::engine::CompiledNodeKind;
using yesdaw::engine::DelayCacheEntry;
using yesdaw::engine::DelayCacheKey;
using yesdaw::engine::DelayNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::SumNode;

namespace {

class ReleaseTrackingNode final : public Node
{
public:
    explicit ReleaseTrackingNode (bool& released) noexcept : released_ (released) {}

    NodeProperties properties() const noexcept override { return {}; }
    std::span<Node* const> directInputs() const noexcept override { return {}; }
    void prepare (double, int) override {}
    void process (const ProcessArgs&) noexcept YESDAW_RT_HOT override {}
    void reset() noexcept override {}
    void release() override { released_ = true; }

private:
    bool& released_;
};

} // namespace

TEST_CASE ("CompiledGraph::process fills the buffer with its identity DC", "[graph][process]")
{
    CompiledGraph g (1, 0.25f);

    std::vector<float> out (64, -1.0f);
    g.process (out.data(), static_cast<int> (out.size()));

    for (float v : out)
        REQUIRE (v == 0.25f);   // exact: the whole point is a mechanically distinguishable identity
}

TEST_CASE ("CompiledGraph exposes its id and identity DC", "[graph][identity]")
{
    CompiledGraph g (42, -0.5f);
    REQUIRE (g.id() == 42u);
    REQUIRE (g.identityDc() == -0.5f);
}

TEST_CASE ("CompiledGraph scalar applies are no-ops on the degenerate seam", "[graph][scalar][degenerate]")
{
    CompiledGraph g (42, 0.5f);

    REQUIRE_FALSE (g.applySetGain (7, 0.25f));
    REQUIRE_FALSE (g.applySetPan (7, 1.0f));

    std::vector<float> out (16, -1.0f);
    g.process (out.data(), static_cast<int> (out.size()));
    for (float v : out)
        REQUIRE (v == 0.5f);
}

TEST_CASE ("CompiledGraph liveness counter tracks construction and destruction", "[graph][liveness]")
{
    // Baseline-relative so the test is independent of Catch2's run order.
    const std::uint64_t base = CompiledGraph::aliveCount();
    {
        CompiledGraph a (1, 0.0f);
        REQUIRE (CompiledGraph::aliveCount() == base + 1);
        {
            CompiledGraph b (2, 0.0f);
            REQUIRE (CompiledGraph::aliveCount() == base + 2);
        }
        REQUIRE (CompiledGraph::aliveCount() == base + 1);   // b destructed
    }
    REQUIRE (CompiledGraph::aliveCount() == base);            // a destructed -> no leak
}

TEST_CASE ("CompiledGraph releases owned Nodes on the janitor side before destruction", "[graph][lifecycle]")
{
    bool released = false;

    {
        CompiledGraph::Payload payload;
        payload.nodeStorage.push_back (std::make_unique<ReleaseTrackingNode> (released));

        CompiledGraph g (std::move (payload));
        REQUIRE (! released);
    }

    REQUIRE (released);
}

TEST_CASE ("CompiledGraph exposes an assertable unbound multi-input check", "[graph][bind]")
{
    auto sum = std::make_unique<SumNode> (7, 1);
    SumNode* const sumPtr = sum.get();

    CompiledGraph::Payload payload;
    payload.nodeStorage.push_back (std::move (sum));

    CompiledNode cn;
    cn.node = sumPtr;
    cn.id = 7;
    cn.kind = CompiledNodeKind::Sum;
    cn.numInputs = 2;
    cn.numChannels = 1;
    payload.compiledNodes.push_back (cn);

    CompiledGraph graph (std::move (payload));
    REQUIRE_FALSE (graph.debugMultiInputNodesBound());
}

TEST_CASE ("CompiledGraph delay cache keeps full keys when synthetic node ids collide", "[graph][carry-over]")
{
    constexpr yesdaw::engine::NodeId kLowCollisionId = 0x8000'0001u;
    constexpr DelayCacheKey kFirstKey = 0x8000'0000'0000'0001ull;
    constexpr DelayCacheKey kSecondKey = 0x8000'0001'0000'0001ull;

    auto first = std::make_unique<DelayNode> (kLowCollisionId, 1, 1);
    auto second = std::make_unique<DelayNode> (kLowCollisionId, 2, 1);
    first->prepare (48000.0, 8);
    second->prepare (48000.0, 8);

    DelayNode* const firstPtr = first.get();
    DelayNode* const secondPtr = second.get();

    CompiledGraph::Payload payload;
    payload.nodeStorage.push_back (std::move (first));
    payload.nodeStorage.push_back (std::move (second));

    CompiledNode firstCompiled;
    firstCompiled.node = firstPtr;
    firstCompiled.id = kLowCollisionId;
    firstCompiled.kind = CompiledNodeKind::Latency;
    firstCompiled.delayCacheKey = kFirstKey;
    payload.compiledNodes.push_back (firstCompiled);

    CompiledNode secondCompiled;
    secondCompiled.node = secondPtr;
    secondCompiled.id = kLowCollisionId;
    secondCompiled.kind = CompiledNodeKind::Latency;
    secondCompiled.delayCacheKey = kSecondKey;
    payload.compiledNodes.push_back (secondCompiled);

    CompiledGraph graph (std::move (payload));
    graph.snapshotDelayCache();

    const std::span<const DelayCacheEntry> cache = graph.debugDelayCache();
    REQUIRE (cache.size() == 2u);
    REQUIRE (cache[0].key == kFirstKey);
    REQUIRE (cache[1].key == kSecondKey);
}
