// YES DAW - Sidechain input pin checks (ADR-0014).
//
// Proves the sidechain pin contract end to end through the real GraphBuilder + CompiledGraph:
//   - a sidechain-capable Node has ordered inputs [main, sidechain], both real graph edges, and its main
//     signal is driven by its sidechain (SidechainGainNode: out = main * sidechain);
//   - the sidechain edge participates in PDC convergence, so main and sidechain reach the consumer
//     sample-aligned (an alignment-sensitive multiply of two impulses is the probe);
//   - multiple sources feeding one pin converge through an explicit SumNode (ADR-0014), and that Sum is
//     the single resolved stream the pin sees.

#include "engine/GraphBuilder.h"
#include "engine/nodes/IdentityDcNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/SidechainGainNode.h"
#include "engine/nodes/SumNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

using Catch::Approx;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNode;
using yesdaw::engine::CompiledNodeKind;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::MasterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::SidechainGainNode;
using yesdaw::engine::SumNode;

namespace {

constexpr NodeId kMasterId = 80000;

class LatentImpulseSource final : public Node
{
public:
    LatentImpulseSource (NodeId id, std::int64_t latencySamples) noexcept
        : id_ (id), latencySamples_ (latencySamples) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, 1, latencySamples_, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }
    void prepare (double, int) override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels < 1)
            return;

        float* const out = args.audio.channels[0];
        for (int i = 0; i < args.numFrames; ++i)
            out[i] = 0.0f;

        if (latencySamples_ >= 0 && latencySamples_ < static_cast<std::int64_t> (args.numFrames))
            out[static_cast<std::size_t> (latencySamples_)] = 1.0f;
    }

    void reset() noexcept override {}
    void release() override {}

private:
    NodeId       id_;
    std::int64_t latencySamples_;
};

std::vector<float> render (CompiledGraph& graph, int frames)
{
    std::vector<float> out (static_cast<std::size_t> (frames), -999.0f);
    graph.process (out.data(), frames);
    return out;
}

const CompiledNode* compiledNodeById (const CompiledGraph& graph, NodeId id)
{
    for (const CompiledNode& node : graph.debugCompiledNodes())
        if (node.id == id)
            return &node;
    return nullptr;
}

} // namespace

TEST_CASE ("Sidechain node drives its main signal by its sidechain input", "[sidechain][graph]")
{
    constexpr NodeId kMain = 100, kSide = 101, kSc = 102;

    auto mainSrc = std::make_unique<IdentityDcNode> (kMain, 0.5f, 1);
    auto sideSrc = std::make_unique<IdentityDcNode> (kSide, 2.0f, 1);
    auto sc = std::make_unique<SidechainGainNode> (kSc, 1);
    sc->setMainInput (mainSrc.get());
    sc->setSidechainInput (sideSrc.get());

    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ sc.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 1;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 64;
    inputs.nodes.push_back (std::move (mainSrc));
    inputs.nodes.push_back (std::move (sideSrc));
    inputs.nodes.push_back (std::move (sc));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->debugMultiInputNodesBound());

    // The sidechain consumer is a real 2-input node (main + sidechain are both graph edges).
    const CompiledNode* const scNode = compiledNodeById (*graph, kSc);
    REQUIRE (scNode != nullptr);
    REQUIRE (scNode->kind == CompiledNodeKind::Sidechain);
    REQUIRE (scNode->numInputs == 2u);

    // out = main (0.5) * sidechain (2.0) = 1.0.
    const std::vector<float> out = render (*graph, 64);
    for (float v : out)
        REQUIRE (v == Approx (1.0f).margin (1.0e-5f));
}

TEST_CASE ("Sidechain edge participates in PDC so main and sidechain align at the consumer", "[sidechain][graph][pdc]")
{
    constexpr NodeId kMain = 110, kSide = 111, kSc = 112;
    constexpr int    kLatency = 5;
    constexpr int    kFrames  = 24;

    // Main impulse at frame 0 (no latency); sidechain impulse at frame 5 (latency 5). The multiply is
    // zero everywhere unless the two impulses land on the SAME frame - which only happens if PDC delays
    // the shorter (main) path by 5 so both reach the consumer aligned.
    auto mainSrc = std::make_unique<LatentImpulseSource> (kMain, 0);
    auto sideSrc = std::make_unique<LatentImpulseSource> (kSide, kLatency);
    auto sc = std::make_unique<SidechainGainNode> (kSc, 1);
    sc->setMainInput (mainSrc.get());
    sc->setSidechainInput (sideSrc.get());

    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ sc.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 2;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = kFrames;
    inputs.nodes.push_back (std::move (mainSrc));
    inputs.nodes.push_back (std::move (sideSrc));
    inputs.nodes.push_back (std::move (sc));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->totalLatency() == kLatency);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Latency) >= 1u);   // a LatencyNode was spliced

    const std::vector<float> out = render (*graph, kFrames);
    int nonZero = 0;
    for (int i = 0; i < kFrames; ++i)
    {
        const float expected = i == kLatency ? 1.0f : 0.0f;
        REQUIRE (out[static_cast<std::size_t> (i)] == Approx (expected).margin (1.0e-5f));
        if (std::fabs (out[static_cast<std::size_t> (i)]) > 1.0e-5f)
            ++nonZero;
    }
    REQUIRE (nonZero == 1);   // exactly one aligned impulse; misalignment would be silent
}

TEST_CASE ("Sidechain pin sees multiple sources converged through an explicit SumNode", "[sidechain][graph][sum]")
{
    constexpr NodeId kMain = 120, kSideA = 121, kSideB = 122, kSum = 123, kSc = 124;

    // Two sidechain sources sum through a Bus before feeding the single pin (ADR-0014: multiple sources
    // into one pin must converge through an explicit Sum so the convergence is visible to PDC).
    auto mainSrc = std::make_unique<IdentityDcNode> (kMain, 1.0f, 1);
    auto sideA = std::make_unique<IdentityDcNode> (kSideA, 0.25f, 1);
    auto sideB = std::make_unique<IdentityDcNode> (kSideB, 0.75f, 1);

    auto sideSum = std::make_unique<SumNode> (kSum, 1);
    sideSum->setInputNodes ({ sideA.get(), sideB.get() });

    auto sc = std::make_unique<SidechainGainNode> (kSc, 1);
    sc->setMainInput (mainSrc.get());
    sc->setSidechainInput (sideSum.get());

    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ sc.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 3;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 32;
    inputs.nodes.push_back (std::move (mainSrc));
    inputs.nodes.push_back (std::move (sideA));
    inputs.nodes.push_back (std::move (sideB));
    inputs.nodes.push_back (std::move (sideSum));
    inputs.nodes.push_back (std::move (sc));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->debugMultiInputNodesBound());

    // out = main (1.0) * (0.25 + 0.75) = 1.0.
    const std::vector<float> out = render (*graph, 32);
    for (float v : out)
        REQUIRE (v == Approx (1.0f).margin (1.0e-5f));
}
