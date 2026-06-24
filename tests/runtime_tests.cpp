// YES DAW — headless checks for the Runtime: RT-safe graph publish/swap + generation-counter
// reclamation (ADR-0006). Pure C++ + Catch2, no JUCE, so this runs on the normal matrix AND the
// RTSan leg (processBlock under -fsanitize=realtime) AND the TSan leg (the stress test under
// -fsanitize=thread). Every test is also a leak gate: it asserts CompiledGraph::aliveCount() returns
// to its baseline once the Runtime is destroyed.

#include "engine/CompiledGraph.h"
#include "engine/Runtime.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

using yesdaw::engine::CompiledGraph;
using yesdaw::engine::GraphId;
using yesdaw::engine::Runtime;

namespace {
std::unique_ptr<CompiledGraph> graph (GraphId id, float dc) { return std::make_unique<CompiledGraph> (id, dc); }
}

TEST_CASE ("a runtime with no published graph outputs silence", "[runtime][silence]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        Runtime rt;
        std::vector<float> buf (32, 1.0f);   // pre-fill with non-zero to prove it gets cleared
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));
        for (float v : buf)
            REQUIRE (v == 0.0f);
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("a published graph is observable on the audio thread, and swaps replace it", "[runtime][swap]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        Runtime rt;
        std::vector<float> buf (32, 9.0f);

        REQUIRE (rt.publish (graph (1, 0.25f)));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));
        for (float v : buf)
            REQUIRE (v == 0.25f);            // graph A installed and rendered

        REQUIRE (rt.publish (graph (2, -0.5f)));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));
        for (float v : buf)
            REQUIRE (v == -0.5f);            // mechanically proves the audio thread switched A -> B

        rt.reclaim();
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("the scalar seam is drained in order with swaps", "[runtime][ordering][seam]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        Runtime rt;
        std::vector<float> buf (8, 0.0f);

        REQUIRE (rt.postSetGain (7, 0.5f));
        REQUIRE (rt.publish (graph (1, -0.25f)));   // a swap sandwiched between two scalar ops
        REQUIRE (rt.postSetPan (7, 0.0f));
        REQUIRE (rt.scalarsApplied() == 0);         // nothing drained until the audio thread runs

        rt.processBlock (buf.data(), static_cast<int> (buf.size()));

        REQUIRE (rt.scalarsApplied() == 2);         // both scalar ops applied, in order with the swap
        for (float v : buf)
            REQUIRE (v == -0.25f);                  // and the sandwiched swap took effect

        rt.reclaim();
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("reclamation frees a graph only after its swap block completes", "[runtime][reclaim][fencepost]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        Runtime rt;
        std::vector<float> buf (8, 0.0f);

        REQUIRE (rt.publish (graph (1, 0.25f)));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));   // install A (gen 0->1); nothing retired
        REQUIRE (CompiledGraph::aliveCount() == base + 1);
        REQUIRE (rt.reclaim() == 0);

        REQUIRE (rt.publish (graph (2, -0.5f)));                       // B queued, not yet installed
        REQUIRE (CompiledGraph::aliveCount() == base + 2);
        REQUIRE (rt.reclaim() == 0);                                   // A is still current -> free nothing
        REQUIRE (CompiledGraph::aliveCount() == base + 2);

        rt.processBlock (buf.data(), static_cast<int> (buf.size()));   // install B, retire A@gen1 (gen 1->2)
        REQUIRE (rt.reclaim() == 1);                                   // gen 2 > 1 -> A freed
        REQUIRE (CompiledGraph::aliveCount() == base + 1);

        rt.processBlock (buf.data(), static_cast<int> (buf.size()));
        for (float v : buf)
            REQUIRE (v == -0.5f);
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("backpressure: a full retirement queue defers swaps without leaking", "[runtime][backpressure]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        Runtime::Config cfg;
        cfg.commandCapacity = 8;
        cfg.retireCapacity  = 4;
        Runtime rt (cfg);
        std::vector<float> buf (16, 0.0f);

        REQUIRE (rt.publish (graph (0, 0.0f)));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));   // install the first graph

        // Spam publishes WITHOUT reclaiming. Retirements pile up; once the retirement queue is full the
        // audio thread stops applying swaps, they back up into the command queue, and publish() then
        // refuses — the control side backpressures, the audio thread never drops (leaks) a graph.
        bool sawBackpressure = false;
        std::uint64_t peakAlive = 0;
        for (GraphId k = 1; k <= 100; ++k)
        {
            if (! rt.publish (graph (k, 0.0f)))
                sawBackpressure = true;
            rt.processBlock (buf.data(), static_cast<int> (buf.size()));
            peakAlive = std::max (peakAlive, CompiledGraph::aliveCount() - base);
        }

        REQUIRE (sawBackpressure);                                     // the queues really did fill up
        // Bounded by what the queues + the installed graph can hold — never the 100 we published.
        REQUIRE (peakAlive <= static_cast<std::uint64_t> (cfg.commandCapacity + cfg.retireCapacity + 2));

        for (int i = 0; i < 200; ++i)                                  // drain: apply remaining + reclaim
        {
            rt.processBlock (buf.data(), static_cast<int> (buf.size()));
            rt.reclaim();
        }
        REQUIRE (CompiledGraph::aliveCount() == base + 1);             // only the installed graph survives
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("stress: control publishes while the audio thread runs and a janitor reclaims", "[runtime][stress]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        Runtime rt;                                  // default capacities
        std::vector<float> buf (128, 0.0f);

        constexpr GraphId kPublishes = 2000;
        std::atomic<bool> controlDone { false };
        std::atomic<bool> stopJanitor { false };

        std::thread control ([&]
        {
            for (GraphId k = 0; k < kPublishes; ++k)
                (void) rt.publish (graph (k, (k & 1u) ? 0.25f : -0.25f));   // drop on backpressure -> freed
            controlDone.store (true, std::memory_order_release);
        });

        std::thread janitor ([&]
        {
            while (! stopJanitor.load (std::memory_order_acquire))
                rt.reclaim();
            rt.reclaim();
        });

        // This (main) thread is the audio thread.
        while (! controlDone.load (std::memory_order_acquire))
            rt.processBlock (buf.data(), static_cast<int> (buf.size()));

        control.join();

        // Keep pumping so every queued swap gets applied (janitor still reclaiming concurrently).
        for (int i = 0; i < 512; ++i)
            rt.processBlock (buf.data(), static_cast<int> (buf.size()));

        stopJanitor.store (true, std::memory_order_release);
        janitor.join();

        // Final deterministic drain on this thread alone (apply remaining + reclaim).
        for (int i = 0; i < 512; ++i)
        {
            rt.processBlock (buf.data(), static_cast<int> (buf.size()));
            rt.reclaim();
        }

        REQUIRE (CompiledGraph::aliveCount() == base + 1);   // exactly one installed graph survives
    }
    REQUIRE (CompiledGraph::aliveCount() == base);           // Runtime destroyed -> no leak, no UAF
}
