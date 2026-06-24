// YES DAW — GraphBuilder checks (ADR-0007 slice G/H).
//
// Pure C++ + Catch2, no JUCE. These tests prove the first real compiled graph path: validate Nodes,
// iterative topo from Master, one-slot-per-node execution, and loud failures for bad graph inputs.

#include "engine/GraphBuilder.h"
#include "engine/nodes/DelayNode.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/IdentityDcNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/MeterNode.h"
#include "engine/nodes/OscillatorNode.h"
#include "engine/nodes/SumNode.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNode;
using yesdaw::engine::CompiledNodeKind;
using yesdaw::engine::DelayNode;
using yesdaw::engine::FaderNode;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::kNoSlot;
using yesdaw::engine::kSilenceSlot;
using yesdaw::engine::MasterNode;
using yesdaw::engine::MeterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::OscillatorNode;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::SumNode;

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

const CompiledNode* compiledNodeById (const CompiledGraph& graph, NodeId id)
{
    for (const CompiledNode& node : graph.debugCompiledNodes())
        if (node.id == id)
            return &node;

    return nullptr;
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

class ImpulseNode final : public Node
{
public:
    explicit ImpulseNode (NodeId id) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, 1, 0, id_ };
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

        if (! emitted_ && args.numFrames > 0)
            out[0] = 1.0f;

        emitted_ = true;
    }

    void reset() noexcept override { emitted_ = false; }
    void release() override {}

private:
    NodeId id_;
    bool emitted_ = false;
};

class StubLatencyNode final : public Node
{
public:
    StubLatencyNode (NodeId id, std::int64_t latencySamples) noexcept
        : id_ (id), latencySamples_ (latencySamples)
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, 1, latencySamples_, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int maxBlockSize) override
    {
        const std::int64_t delay = latencySamples_ > 0 ? latencySamples_ : 0;
        const std::int64_t needed = delay + static_cast<std::int64_t> (maxBlockSize > 0 ? maxBlockSize : 1) + 1;

        std::uint32_t pow2 = 1;
        while (static_cast<std::int64_t> (pow2) < needed)
            pow2 <<= 1;

        framesPerChannel_ = pow2;
        mask_ = pow2 - 1u;
        writePos_ = 0;
        ring_.assign (framesPerChannel_, 0.0f);
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels < 1)
            return;

        float* const x = args.audio.channels[0];
        float* const ring = ring_.data();
        const std::uint32_t w = writePos_;
        const std::uint32_t d = static_cast<std::uint32_t> (latencySamples_ > 0 ? latencySamples_ : 0);

        for (int i = 0; i < args.numFrames; ++i)
        {
            const std::uint32_t ui = static_cast<std::uint32_t> (i);
            ring[(w + ui) & mask_] = x[i];
            x[i] = ring[(w + ui - d) & mask_];
        }

        writePos_ = (writePos_ + static_cast<std::uint32_t> (args.numFrames)) & mask_;
    }

    void reset() noexcept override
    {
        if (! ring_.empty())
            std::memset (ring_.data(), 0, ring_.size() * sizeof (float));
        writePos_ = 0;
    }

    void release() override { ring_.clear(); ring_.shrink_to_fit(); }

    void setInput (Node* input) noexcept { input_ = input; }

private:
    NodeId id_;
    std::int64_t latencySamples_;
    Node* input_ = nullptr;
    std::vector<float> ring_;
    std::uint32_t framesPerChannel_ = 0;
    std::uint32_t mask_ = 0;
    std::uint32_t writePos_ = 0;
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

TEST_CASE ("GraphBuilder sizes the float pool to live width instead of node count", "[builder][pool]")
{
    constexpr int kLinks = 32;

    auto source = std::make_unique<IdentityDcNode> (1, 0.5f, 1);
    Node* previous = source.get();

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 64;
    inputs.nodes.reserve (static_cast<std::size_t> (kLinks) + 2u);
    inputs.nodes.push_back (std::move (source));

    for (int i = 0; i < kLinks; ++i)
    {
        auto link = std::make_unique<LinkNode> (static_cast<NodeId> (2 + i));
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
    REQUIRE (graph->debugPoolLayout().numFloatSlots == 3u);
    REQUIRE (graph->debugPoolLayout().numFloatSlots < static_cast<std::uint16_t> (kLinks));

    const std::vector<float> out = render (*graph, 64);
    for (float v : out)
        REQUIRE (v == 0.5f);
}

TEST_CASE ("GraphBuilder never assigns slot zero as a producer output", "[builder][pool][silence-slot]")
{
    auto source = std::make_unique<IdentityDcNode> (1, 0.25f, 1);
    auto fader = std::make_unique<FaderNode> (2, 1);
    auto meter = std::make_unique<MeterNode> (3, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    FaderNode* const faderPtr = fader.get();
    MeterNode* const meterPtr = meter.get();
    faderPtr->setInput (sourcePtr);
    meterPtr->setInput (faderPtr);
    master->setInputNodes ({ meterPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (fader));
    inputs.nodes.push_back (std::move (meter));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    for (const CompiledNode& node : graph->debugCompiledNodes())
    {
        REQUIRE (node.outputSlot != kSilenceSlot);
        REQUIRE (node.outputSlot != kNoSlot);
    }
}

TEST_CASE ("GraphBuilder marks R3 in-place reuse only for whitelisted last-reader nodes", "[builder][pool][alias]")
{
    auto source = std::make_unique<IdentityDcNode> (1, 0.25f, 1);
    auto fader = std::make_unique<FaderNode> (2, 1);
    auto meter = std::make_unique<MeterNode> (3, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    FaderNode* const faderPtr = fader.get();
    MeterNode* const meterPtr = meter.get();
    faderPtr->setInput (sourcePtr);
    meterPtr->setInput (faderPtr);
    master->setInputNodes ({ meterPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 32;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (fader));
    inputs.nodes.push_back (std::move (meter));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const CompiledNode* sourceNode = compiledNodeById (*graph, 1);
    const CompiledNode* faderNode = compiledNodeById (*graph, 2);
    const CompiledNode* meterNode = compiledNodeById (*graph, 3);
    REQUIRE (sourceNode != nullptr);
    REQUIRE (faderNode != nullptr);
    REQUIRE (meterNode != nullptr);

    REQUIRE (faderNode->aliasOk);
    REQUIRE (faderNode->outputSlot == sourceNode->outputSlot);
    REQUIRE (meterNode->aliasOk);
    REQUIRE (meterNode->outputSlot == faderNode->outputSlot);

    const std::vector<float> out = render (*graph, 32);
    for (float v : out)
        REQUIRE (v == 0.25f);
}

TEST_CASE ("GraphBuilder keeps pending multi-input readers from being reused early", "[builder][pool][alias]")
{
    auto source = std::make_unique<IdentityDcNode> (1, 0.25f, 1);
    auto fader = std::make_unique<FaderNode> (2, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    FaderNode* const faderPtr = fader.get();
    faderPtr->setInput (sourcePtr);
    master->setInputNodes ({ faderPtr, sourcePtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 32;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (fader));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const CompiledNode* sourceNode = compiledNodeById (*graph, 1);
    const CompiledNode* faderNode = compiledNodeById (*graph, 2);
    REQUIRE (sourceNode != nullptr);
    REQUIRE (faderNode != nullptr);

    REQUIRE_FALSE (faderNode->aliasOk);
    REQUIRE (faderNode->outputSlot != sourceNode->outputSlot);

    const std::vector<float> out = render (*graph, 32);
    for (float v : out)
        REQUIRE (v == 0.5f);
}

TEST_CASE ("GraphBuilder does not alias Fader output over SumNode output", "[builder][pool][alias][sum]")
{
    auto source = std::make_unique<IdentityDcNode> (1, 0.25f, 1);
    auto sum = std::make_unique<SumNode> (2, 1);
    auto fader = std::make_unique<FaderNode> (3, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    SumNode* const sumPtr = sum.get();
    FaderNode* const faderPtr = fader.get();
    sumPtr->setInputNodes ({ sourcePtr });
    faderPtr->setInput (sumPtr);
    master->setInputNodes ({ faderPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 32;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (sum));
    inputs.nodes.push_back (std::move (fader));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const CompiledNode* sumNode = compiledNodeById (*graph, 2);
    const CompiledNode* faderNode = compiledNodeById (*graph, 3);
    const CompiledNode* masterNode = compiledNodeById (*graph, kMasterId);
    REQUIRE (sumNode != nullptr);
    REQUIRE (faderNode != nullptr);
    REQUIRE (masterNode != nullptr);

    REQUIRE_FALSE (faderNode->aliasOk);
    REQUIRE (faderNode->outputSlot != sumNode->outputSlot);
    REQUIRE (sumNode->busAccumSlot != kNoSlot);
    REQUIRE (masterNode->busAccumSlot != kNoSlot);
    REQUIRE (sumNode->busAccumSlot != masterNode->busAccumSlot);
    REQUIRE (graph->debugPoolLayout().numDoubleSlots == 2u);

    const std::vector<float> out = render (*graph, 32);
    for (float v : out)
        REQUIRE (v == 0.25f);
}

TEST_CASE ("GraphBuilder renders equivalent diamond graphs identically across input order", "[builder][pool][order]")
{
    auto buildDiamond = [] (bool reverse)
    {
        auto source = std::make_unique<IdentityDcNode> (1, 1.0f, 1);
        auto left = std::make_unique<FaderNode> (10, 1);
        auto right = std::make_unique<FaderNode> (20, 1);
        auto master = std::make_unique<MasterNode> (kMasterId, 1);

        IdentityDcNode* const sourcePtr = source.get();
        FaderNode* const leftPtr = left.get();
        FaderNode* const rightPtr = right.get();
        leftPtr->setTargetGain (0.25f);
        rightPtr->setTargetGain (0.75f);
        leftPtr->setInput (sourcePtr);
        rightPtr->setInput (sourcePtr);

        GraphBuilder::Inputs inputs;
        inputs.masterNodeId = kMasterId;
        inputs.maxBlockSize = 64;

        if (reverse)
        {
            master->setInputNodes ({ rightPtr, leftPtr });
            inputs.nodes.push_back (std::move (right));
            inputs.nodes.push_back (std::move (master));
            inputs.nodes.push_back (std::move (source));
            inputs.nodes.push_back (std::move (left));
        }
        else
        {
            master->setInputNodes ({ leftPtr, rightPtr });
            inputs.nodes.push_back (std::move (source));
            inputs.nodes.push_back (std::move (left));
            inputs.nodes.push_back (std::move (right));
            inputs.nodes.push_back (std::move (master));
        }

        GraphBuildError error;
        std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
        REQUIRE (graph != nullptr);
        REQUIRE (error.code() == GraphBuildError::Code::None);
        return graph;
    };

    std::unique_ptr<CompiledGraph> a = buildDiamond (false);
    std::unique_ptr<CompiledGraph> b = buildDiamond (true);

    const std::vector<float> outA = render (*a, 64);
    const std::vector<float> outB = render (*b, 64);

    REQUIRE (outA == outB);
    for (float v : outA)
        REQUIRE (v == 1.0f);
}

TEST_CASE ("GraphBuilder PDC aligns convergence impulses at total latency", "[builder][pdc]")
{
    constexpr int kLatency = 11;
    constexpr int kFrames = 32;

    auto source = std::make_unique<ImpulseNode> (1);
    auto latent = std::make_unique<StubLatencyNode> (2, kLatency);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    ImpulseNode* const sourcePtr = source.get();
    StubLatencyNode* const latentPtr = latent.get();
    latentPtr->setInput (sourcePtr);
    master->setInputNodes ({ sourcePtr, latentPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = kFrames;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (latent));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->totalLatency() == kLatency);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Latency) == 1u);

    const std::vector<float> out = render (*graph, kFrames);
    for (int i = 0; i < kFrames; ++i)
        REQUIRE (out[static_cast<std::size_t> (i)] == (i == kLatency ? 2.0f : 0.0f));
}

TEST_CASE ("GraphBuilder PDC guard would catch the unspliced two-peak case", "[builder][pdc][guard]")
{
    constexpr int kLatency = 7;
    constexpr int kFrames = 24;

    auto source = std::make_unique<ImpulseNode> (1);
    auto latent = std::make_unique<StubLatencyNode> (2, kLatency);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    ImpulseNode* const sourcePtr = source.get();
    StubLatencyNode* const latentPtr = latent.get();
    latentPtr->setInput (sourcePtr);
    master->setInputNodes ({ sourcePtr, latentPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = kFrames;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (latent));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const std::vector<float> out = render (*graph, kFrames);

    int nonZeroFrames = 0;
    for (float v : out)
        if (v != 0.0f)
            ++nonZeroFrames;

    REQUIRE (out[0] == 0.0f);
    REQUIRE (out[static_cast<std::size_t> (kLatency)] == 2.0f);
    REQUIRE (nonZeroFrames == 1);
}

TEST_CASE ("GraphBuilder PDC leaves single-input chains unspliced", "[builder][pdc]")
{
    constexpr int kLatency = 13;
    constexpr int kFrames = 32;

    auto source = std::make_unique<ImpulseNode> (1);
    auto latent = std::make_unique<StubLatencyNode> (2, kLatency);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    ImpulseNode* const sourcePtr = source.get();
    StubLatencyNode* const latentPtr = latent.get();
    latentPtr->setInput (sourcePtr);
    master->setInputNodes ({ latentPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = kFrames;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (latent));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->totalLatency() == kLatency);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Latency) == 0u);

    const std::vector<float> out = render (*graph, kFrames);
    for (int i = 0; i < kFrames; ++i)
        REQUIRE (out[static_cast<std::size_t> (i)] == (i == kLatency ? 1.0f : 0.0f));
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

TEST_CASE ("GraphBuilder rejects INT64_MAX latency before PDC arithmetic", "[builder][validate][pdc]")
{
    auto source = std::make_unique<LinkNode> (1);
    source->setLatency (std::numeric_limits<std::int64_t>::max());

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
