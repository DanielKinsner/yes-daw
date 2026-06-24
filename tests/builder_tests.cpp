// YES DAW — GraphBuilder checks (ADR-0007 slice G).
//
// Pure C++ + Catch2, no JUCE. These tests prove the first real compiled graph path: validate Nodes,
// iterative topo from Master, one-slot-per-node execution, and loud failures for bad graph inputs.

#include "engine/GraphBuilder.h"
#include "engine/nodes/DelayNode.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/IdentityDcNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/OscillatorNode.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <span>
#include <utility>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::DelayNode;
using yesdaw::engine::FaderNode;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::MasterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::OscillatorNode;
using yesdaw::engine::ProcessArgs;

namespace {

constexpr NodeId kMasterId = 50000;

GraphBuilder::Inputs inputsWithMaster (std::unique_ptr<Node> source)
{
    Node* const sourcePtr = source.get();

    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ sourcePtr });

    GraphBuilder::Inputs inputs;
    inputs.id           = 77;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate   = 48000.0;
    inputs.maxBlockSize = 512;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (master));
    return inputs;
}

std::vector<float> render (CompiledGraph& graph, int frames)
{
    std::vector<float> out (static_cast<std::size_t> (frames), -999.0f);
    graph.process (out.data(), frames);
    return out;
}

class LinkNode final : public Node
{
public:
    explicit LinkNode (NodeId id) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, 1, latencySamples_, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override {}
    void process (const ProcessArgs&) noexcept YESDAW_RT_HOT override {}
    void reset() noexcept override {}
    void release() override {}

    void setInput (Node* input) noexcept { input_ = input; }
    void setLatency (std::int64_t latency) noexcept { latencySamples_ = latency; }

private:
    NodeId id_;
    Node* input_ = nullptr;
    std::int64_t latencySamples_ = 0;
};

} // namespace

TEST_CASE ("GraphBuilder renders IdentityDc through Master", "[builder][render]")
{
    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph =
        GraphBuilder::build (inputsWithMaster (std::make_unique<IdentityDcNode> (1, 0.375f, 1)), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const std::vector<float> out = render (*graph, 64);
    for (float v : out)
        REQUIRE (v == 0.375f);
}

TEST_CASE ("GraphBuilder renders Oscillator through Master as non-DC", "[builder][render][osc]")
{
    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph =
        GraphBuilder::build (inputsWithMaster (std::make_unique<OscillatorNode> (1)), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const std::vector<float> out = render (*graph, 512);

    float minSample = out.front();
    float maxSample = out.front();
    for (float v : out)
    {
        REQUIRE (std::isfinite (v));
        if (v < minSample)
            minSample = v;
        if (v > maxSample)
            maxSample = v;
    }

    REQUIRE (maxSample - minSample > 0.001f);
}

TEST_CASE ("GraphBuilder renders an empty project as silence", "[builder][render][silence]")
{
    GraphBuilder::Inputs inputs;
    inputs.id = 99;
    inputs.maxBlockSize = 128;

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const std::vector<float> out = render (*graph, 128);
    for (float v : out)
        REQUIRE (v == 0.0f);
}

TEST_CASE ("GraphBuilder rejects missing master node", "[builder][validate]")
{
    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.nodes.push_back (std::make_unique<IdentityDcNode> (1, 0.1f, 1));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::MissingNode);
    REQUIRE (error.nodeId() == kMasterId);
}

TEST_CASE ("GraphBuilder topo walk handles a 1000-node chain iteratively", "[builder][topo][deep]")
{
    constexpr int kLinks = 1000;

    auto source = std::make_unique<IdentityDcNode> (1, -0.125f, 1);
    Node* previous = source.get();

    GraphBuilder::Inputs inputs;
    inputs.id           = 88;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate   = 48000.0;
    inputs.maxBlockSize = 64;
    inputs.nodes.reserve (static_cast<std::size_t> (kLinks) + 2u);
    inputs.nodes.push_back (std::move (source));

    for (int i = 0; i < kLinks; ++i)
    {
        auto link = std::make_unique<FaderNode> (static_cast<NodeId> (2 + i), 1);
        link->setInput (previous);
        previous = link.get();
        inputs.nodes.push_back (std::move (link));
    }

    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ previous });
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const std::vector<float> out = render (*graph, 64);
    for (float v : out)
        REQUIRE (v == -0.125f);
}

TEST_CASE ("GraphBuilder rejects a non-Delay back-edge", "[builder][cycle]")
{
    auto a = std::make_unique<LinkNode> (1);
    auto b = std::make_unique<LinkNode> (2);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    LinkNode* const aPtr = a.get();
    LinkNode* const bPtr = b.get();
    aPtr->setInput (bPtr);
    bPtr->setInput (aPtr);
    master->setInputNodes ({ aPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.nodes.push_back (std::move (a));
    inputs.nodes.push_back (std::move (b));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::CyclicGraph);
}

TEST_CASE ("GraphBuilder allows a DelayNode back-edge", "[builder][cycle][delay]")
{
    auto a = std::make_unique<LinkNode> (1);
    auto delay = std::make_unique<DelayNode> (2, 1, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    LinkNode* const aPtr = a.get();
    DelayNode* const delayPtr = delay.get();
    aPtr->setInput (delayPtr);
    delayPtr->setInput (aPtr);
    master->setInputNodes ({ aPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.nodes.push_back (std::move (a));
    inputs.nodes.push_back (std::move (delay));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
}

TEST_CASE ("GraphBuilder rejects duplicate node ids", "[builder][validate]")
{
    auto a = std::make_unique<IdentityDcNode> (1, 0.1f, 1);
    auto b = std::make_unique<IdentityDcNode> (1, 0.2f, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ a.get(), b.get() });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.nodes.push_back (std::move (a));
    inputs.nodes.push_back (std::move (b));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::DuplicateNodeId);
    REQUIRE (error.nodeId() == 1u);
}

TEST_CASE ("GraphBuilder rejects missing input nodes", "[builder][validate]")
{
    auto missing = std::make_unique<IdentityDcNode> (99, 0.1f, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ missing.get() });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::MissingNode);
    REQUIRE (error.nodeId() == 99u);
}

TEST_CASE ("GraphBuilder rejects latency outside the cap", "[builder][validate]")
{
    auto source = std::make_unique<LinkNode> (1);
    source->setLatency (GraphBuilder::kMaxLatencyCap + 1);

    GraphBuilder::Inputs inputs = inputsWithMaster (std::move (source));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::LatencyOutOfRange);
    REQUIRE (error.nodeId() == 1u);
}

TEST_CASE ("GraphBuilder rejects negative latency", "[builder][validate]")
{
    auto source = std::make_unique<LinkNode> (1);
    source->setLatency (-1);

    GraphBuilder::Inputs inputs = inputsWithMaster (std::move (source));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::LatencyOutOfRange);
    REQUIRE (error.nodeId() == 1u);
}

TEST_CASE ("GraphBuilder clamps advertised channels into the supported node range", "[builder][validate]")
{
    std::unique_ptr<CompiledGraph> graph =
        GraphBuilder::build (inputsWithMaster (std::make_unique<IdentityDcNode> (1, 0.25f, 99)));

    REQUIRE (graph != nullptr);

    const std::vector<float> out = render (*graph, 32);
    for (float v : out)
        REQUIRE (v == 0.25f);
}

TEST_CASE ("GraphBuilder rejects fan-in that cannot fit flat input metadata", "[builder][validate]")
{
    auto source = std::make_unique<IdentityDcNode> (1, 1.0f, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    std::vector<Node*> inputs (65536u, source.get());
    master->setInputNodes (std::move (inputs));

    GraphBuilder::Inputs buildInputs;
    buildInputs.masterNodeId = kMasterId;
    buildInputs.nodes.push_back (std::move (source));
    buildInputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (buildInputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::GraphTooLarge);
    REQUIRE (error.nodeId() == kMasterId);
}
