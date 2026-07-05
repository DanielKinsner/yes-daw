// YES DAW — headless checks for the Runtime: RT-safe graph publish/swap + generation-counter
// reclamation (ADR-0006). Pure C++ + Catch2, no JUCE, so this runs on the normal matrix AND the
// RTSan leg (processBlock under -fsanitize=realtime) AND the TSan leg (the stress test under
// -fsanitize=thread). Every test is also a leak gate: it asserts CompiledGraph::aliveCount() returns
// to its baseline once the Runtime is destroyed.

#include "engine/CompiledGraph.h"
#include "engine/GraphBuilder.h"
#include "engine/Runtime.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/IdentityDcNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/PanNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

using Catch::Approx;
using yesdaw::engine::AutomationCurveType;
using yesdaw::engine::CompiledAutomationLane;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::EventStream;
using yesdaw::engine::FaderNode;
using yesdaw::engine::GraphId;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::MasterNode;
using yesdaw::engine::NodeId;
using yesdaw::engine::PanNode;
using yesdaw::engine::Runtime;
using yesdaw::engine::Transport;

namespace {
std::unique_ptr<CompiledGraph> graph (GraphId id, float dc) { return std::make_unique<CompiledGraph> (id, dc); }

constexpr NodeId kRuntimeMasterId = 61000;

// Raw builders — NO Catch2 assertions, so they are safe to call from a worker thread (Catch2's REQUIRE
// is not thread-safe). The stress test below builds real graphs off the main thread; the wrappers add
// the main-thread-only REQUIREs the single-threaded tests rely on.
std::unique_ptr<CompiledGraph> buildFaderGraph (GraphId id, NodeId faderId, float dc)
{
    auto source = std::make_unique<IdentityDcNode> (100, dc, 1);
    auto fader = std::make_unique<FaderNode> (faderId, 1);
    auto master = std::make_unique<MasterNode> (kRuntimeMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    FaderNode* const faderPtr = fader.get();
    faderPtr->setInput (sourcePtr);
    master->setInputNodes ({ faderPtr });

    GraphBuilder::Inputs inputs;
    inputs.id           = id;
    inputs.masterNodeId = kRuntimeMasterId;
    inputs.sampleRate   = 48000.0;
    inputs.maxBlockSize = 512;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (fader));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    return GraphBuilder::build (std::move (inputs), &error);   // may be null; the caller decides how to assert
}

std::unique_ptr<CompiledGraph> buildAutomatedFaderGraph (GraphId id, NodeId faderId, float dc)
{
    auto source = std::make_unique<IdentityDcNode> (100, dc, 1);
    auto fader = std::make_unique<FaderNode> (faderId, 1);
    auto master = std::make_unique<MasterNode> (kRuntimeMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    FaderNode* const faderPtr = fader.get();
    faderPtr->setInput (sourcePtr);
    master->setInputNodes ({ faderPtr });

    GraphBuilder::Inputs inputs;
    inputs.id           = id;
    inputs.masterNodeId = kRuntimeMasterId;
    inputs.sampleRate   = 48000.0;
    inputs.maxBlockSize = 512;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (fader));
    inputs.nodes.push_back (std::move (master));

    CompiledAutomationLane lane;
    lane.targetNode = faderId;
    lane.parameterId = FaderNode::kGainParameterId;
    lane.frames = { 0 };
    lane.values = { 0.0 };
    lane.curveTypes = { AutomationCurveType::Hold };
    inputs.automationLanes.push_back (std::move (lane));

    GraphBuildError error;
    return GraphBuilder::build (std::move (inputs), &error);
}

std::unique_ptr<CompiledGraph> buildPanGraph (GraphId id, NodeId panId, float dc)
{
    auto source = std::make_unique<IdentityDcNode> (100, dc, 1);
    auto pan = std::make_unique<PanNode> (panId);
    auto master = std::make_unique<MasterNode> (kRuntimeMasterId, 2);

    IdentityDcNode* const sourcePtr = source.get();
    PanNode* const panPtr = pan.get();
    panPtr->setInput (sourcePtr);
    master->setInputNodes ({ panPtr });

    GraphBuilder::Inputs inputs;
    inputs.id           = id;
    inputs.masterNodeId = kRuntimeMasterId;
    inputs.sampleRate   = 48000.0;
    inputs.maxBlockSize = 512;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (pan));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    return GraphBuilder::build (std::move (inputs), &error);
}

std::unique_ptr<CompiledGraph> buildAutomatedPanGraph (GraphId id, NodeId panId, float dc)
{
    auto source = std::make_unique<IdentityDcNode> (100, dc, 1);
    auto pan = std::make_unique<PanNode> (panId);
    auto master = std::make_unique<MasterNode> (kRuntimeMasterId, 2);

    IdentityDcNode* const sourcePtr = source.get();
    PanNode* const panPtr = pan.get();
    panPtr->setInput (sourcePtr);
    master->setInputNodes ({ panPtr });

    GraphBuilder::Inputs inputs;
    inputs.id           = id;
    inputs.masterNodeId = kRuntimeMasterId;
    inputs.sampleRate   = 48000.0;
    inputs.maxBlockSize = 512;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (pan));
    inputs.nodes.push_back (std::move (master));

    CompiledAutomationLane lane;
    lane.targetNode = panId;
    lane.parameterId = PanNode::kPanParameterId;
    lane.frames = { 0 };
    lane.values = { 0.0 };
    lane.curveTypes = { AutomationCurveType::Hold };
    inputs.automationLanes.push_back (std::move (lane));

    GraphBuildError error;
    return GraphBuilder::build (std::move (inputs), &error);
}

std::unique_ptr<CompiledGraph> faderGraph (GraphId id, NodeId faderId, float dc)
{
    std::unique_ptr<CompiledGraph> built = buildFaderGraph (id, faderId, dc);
    REQUIRE (built != nullptr);
    return built;
}

std::unique_ptr<CompiledGraph> panGraph (GraphId id, NodeId panId, float dc)
{
    std::unique_ptr<CompiledGraph> built = buildPanGraph (id, panId, dc);
    REQUIRE (built != nullptr);
    return built;
}

std::unique_ptr<CompiledGraph> automatedFaderGraph (GraphId id, NodeId faderId, float dc)
{
    std::unique_ptr<CompiledGraph> built = buildAutomatedFaderGraph (id, faderId, dc);
    REQUIRE (built != nullptr);
    return built;
}

std::unique_ptr<CompiledGraph> automatedPanGraph (GraphId id, NodeId panId, float dc)
{
    std::unique_ptr<CompiledGraph> built = buildAutomatedPanGraph (id, panId, dc);
    REQUIRE (built != nullptr);
    return built;
}
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

TEST_CASE ("unapplied scalar seam commands do not disturb degenerate swaps", "[runtime][ordering][seam]")
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

        REQUIRE (rt.scalarsApplied() == 0);         // degenerate graphs expose no Fader/Pan command target
        for (float v : buf)
            REQUIRE (v == -0.25f);                  // and the sandwiched swap took effect

        rt.reclaim();
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("Runtime routes SetGain to the graph current at each command point", "[runtime][scalar][gain]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        constexpr NodeId kFaderId = 10;

        Runtime rt;
        std::vector<float> buf (512, 0.0f);

        REQUIRE (rt.publish (faderGraph (1, kFaderId, 1.0f)));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));
        REQUIRE (buf.back() == 1.0f);

        REQUIRE (rt.postSetGain (kFaderId, 0.25f));
        REQUIRE (rt.publish (faderGraph (2, kFaderId, 2.0f)));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));
        REQUIRE (rt.scalarsApplied() == 1);
        REQUIRE (buf.back() == Approx (2.0f).margin (1.0e-6f));  // pre-swap gain did not hit graph B

        REQUIRE (rt.postSetGain (kFaderId, 0.75f));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));
        REQUIRE (rt.scalarsApplied() == 2);
        REQUIRE (buf.back() == Approx (1.5f).margin (1.0e-6f));

        rt.reclaim();
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("Runtime routes SetPan through the current compiled graph", "[runtime][scalar][pan]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        constexpr NodeId kPanId = 20;

        Runtime rt;
        std::vector<float> buf (512, 0.0f);

        REQUIRE (rt.publish (panGraph (1, kPanId, 1.0f)));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));
        REQUIRE (buf.back() == Approx (std::sqrt (0.5f)).margin (1.0e-4f));

        REQUIRE (rt.postSetPan (kPanId, 1.0f));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));
        REQUIRE (rt.scalarsApplied() == 1);
        REQUIRE (buf.back() == Approx (0.0f).margin (1.0e-4f));

        rt.reclaim();
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("Runtime keeps fader automation lanes ahead of SetGain scalar posts",
           "[runtime][automation][precedence][h15][cp3]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        constexpr NodeId kFaderId = 40;

        Runtime rt;
        std::vector<float> buf (512, 1.0f);
        float* outChannels[1] = { buf.data() };
        EventStream events;
        Transport transport;
        transport.hasTimelineFrame = true;

        REQUIRE (rt.publish (automatedFaderGraph (1, kFaderId, 1.0f)));
        transport.timelineFrame = 0;
        rt.processBlock (outChannels, 1, static_cast<int> (buf.size()), events, transport);
        REQUIRE (buf.back() == Approx (0.0f).margin (1.0e-6f));

        REQUIRE (rt.postSetGain (kFaderId, 1.0f));
        transport.timelineFrame = 512;
        rt.processBlock (outChannels, 1, static_cast<int> (buf.size()), events, transport);
        REQUIRE (rt.scalarsApplied() == 0);
        REQUIRE (buf.back() == Approx (0.0f).margin (1.0e-6f));

        rt.reclaim();
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("Runtime keeps pan automation lanes ahead of SetPan scalar posts",
           "[runtime][automation][precedence][h15][cp3]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        constexpr NodeId kPanId = 41;

        Runtime rt;
        std::vector<float> left (512, 1.0f);
        std::vector<float> right (512, 1.0f);
        float* outChannels[2] = { left.data(), right.data() };
        EventStream events;
        Transport transport;
        transport.hasTimelineFrame = true;

        REQUIRE (rt.publish (automatedPanGraph (1, kPanId, 1.0f)));
        transport.timelineFrame = 0;
        rt.processBlock (outChannels, 2, static_cast<int> (left.size()), events, transport);
        REQUIRE (left.back() == Approx (1.0f).margin (1.0e-4f));
        REQUIRE (right.back() == Approx (0.0f).margin (1.0e-4f));

        REQUIRE (rt.postSetPan (kPanId, 1.0f));
        transport.timelineFrame = 512;
        rt.processBlock (outChannels, 2, static_cast<int> (left.size()), events, transport);
        REQUIRE (rt.scalarsApplied() == 0);
        REQUIRE (left.back() == Approx (1.0f).margin (1.0e-4f));
        REQUIRE (right.back() == Approx (0.0f).margin (1.0e-4f));

        rt.reclaim();
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("Runtime ignores scalar commands for missing ids and wrong node kinds", "[runtime][scalar][invalid]")
{
    const auto base = CompiledGraph::aliveCount();
    {
        constexpr NodeId kFaderId = 30;

        Runtime rt;
        std::vector<float> buf (512, 0.0f);

        REQUIRE (rt.publish (faderGraph (1, kFaderId, 1.0f)));
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));

        REQUIRE (rt.postSetGain (9999, 0.0f));       // missing id
        REQUIRE (rt.postSetPan (kFaderId, 1.0f));    // wrong node kind
        rt.processBlock (buf.data(), static_cast<int> (buf.size()));

        REQUIRE (rt.scalarsApplied() == 0);
        REQUIRE (buf.back() == 1.0f);

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

TEST_CASE ("stress: real-node graphs swap under concurrency so RTSan/TSan cover node dispatch",
           "[runtime][stress][nodes]")
{
    // The degenerate-graph stress test above proves the swap/reclaim MACHINERY is race-free. This one
    // closes the gap the H1 coverage review found: it swaps REAL compiled graphs (Fader / Pan nodes)
    // while the audio thread renders them, so the RTSan leg actually executes Node::process() under
    // concurrent swaps and the TSan leg sees real node state cross the publish/process/reclaim boundary.
    const auto base = CompiledGraph::aliveCount();
    {
        Runtime rt;                                  // default capacities
        std::vector<float> buf (128, 0.0f);

        REQUIRE (buildFaderGraph (0, 10, 0.5f) != nullptr);   // main-thread sanity before the workers run
        REQUIRE (buildPanGraph   (0, 20, 0.5f) != nullptr);

        constexpr GraphId kPublishes = 600;
        std::atomic<bool> controlDone { false };
        std::atomic<bool> stopJanitor { false };

        std::thread control ([&]
        {
            for (GraphId k = 0; k < kPublishes; ++k)
            {
                std::unique_ptr<CompiledGraph> g = (k & 1u) ? buildPanGraph (k, 20, 0.25f)
                                                            : buildFaderGraph (k, 10, 0.25f);
                (void) rt.publish (std::move (g));   // dropped on backpressure -> freed; never the audio thread
            }
            controlDone.store (true, std::memory_order_release);
        });

        std::thread janitor ([&]
        {
            while (! stopJanitor.load (std::memory_order_acquire))
                rt.reclaim();
            rt.reclaim();
        });

        // This (main) thread is the audio thread: render real nodes while graphs swap under it.
        while (! controlDone.load (std::memory_order_acquire))
            rt.processBlock (buf.data(), static_cast<int> (buf.size()));

        control.join();

        for (int i = 0; i < 512; ++i)
            rt.processBlock (buf.data(), static_cast<int> (buf.size()));

        stopJanitor.store (true, std::memory_order_release);
        janitor.join();

        for (int i = 0; i < 512; ++i)
        {
            rt.processBlock (buf.data(), static_cast<int> (buf.size()));
            rt.reclaim();
        }

        REQUIRE (CompiledGraph::aliveCount() == base + 1);   // exactly one installed graph survives
    }
    REQUIRE (CompiledGraph::aliveCount() == base);           // no leak, no UAF
}
