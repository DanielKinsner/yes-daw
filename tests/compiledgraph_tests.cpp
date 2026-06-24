// YES DAW — headless checks for the CompiledGraph stub (ADR-0006/0007 foundation).
//
// Pure C++ + Catch2, no JUCE. Proves the immutable graph renders its observable identity, and that the
// liveness counter tracks construction/destruction (the leak gate the reclamation tests rely on). Runs
// on the normal matrix AND the RTSan/TSan legs.

#include "engine/CompiledGraph.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using yesdaw::engine::CompiledGraph;

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
