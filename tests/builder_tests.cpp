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
#include "engine/nodes/PanNode.h"
#include "engine/nodes/PlaceholderNode.h"
#include "engine/nodes/SumNode.h"

#include <catch2/catch_approx.hpp>
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
using yesdaw::engine::AutomationCurveType;
using yesdaw::engine::CompiledAutomationLane;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNode;
using yesdaw::engine::CompiledNodeKind;
using yesdaw::engine::DelayCacheEntry;
using yesdaw::engine::DelayCacheKey;
using yesdaw::engine::DelayNode;
using yesdaw::engine::Event;
using yesdaw::engine::FaderNode;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::InputSlot;
using yesdaw::engine::kNoSlot;
using yesdaw::engine::kSilenceSlot;
using yesdaw::engine::MasterNode;
using yesdaw::engine::MeterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::OscillatorNode;
using yesdaw::engine::PanNode;
using yesdaw::engine::PlaceholderNode;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::SumNode;
using yesdaw::engine::Transport;
using Catch::Approx;

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

GraphBuilder::Inputs placeholderInputs (float sourceValue, NodeId placeholderId)
{
    auto source = std::make_unique<IdentityDcNode> (1, sourceValue, 1);
    auto placeholder = std::make_unique<PlaceholderNode> (placeholderId, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    PlaceholderNode* const placeholderPtr = placeholder.get();
    placeholderPtr->setInput (sourcePtr);
    master->setInputNodes ({ placeholderPtr });

    GraphBuilder::Inputs inputs;
    inputs.id = 78;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = 48000.0;
    inputs.maxBlockSize = 512;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (placeholder));
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

const DelayCacheEntry* delayCacheByKey (const CompiledGraph& graph, DelayCacheKey key)
{
    graph.snapshotDelayCache();
    for (const DelayCacheEntry& entry : graph.debugDelayCache())
        if (entry.key == key)
            return &entry;

    return nullptr;
}

GraphBuilder::Inputs delayedIdentityInputs (float dc,
                                            NodeId delayId,
                                            std::int64_t delaySamples,
                                            int maxBlockSize,
                                            const CompiledGraph* previous = nullptr)
{
    auto source = std::make_unique<IdentityDcNode> (1, dc, 1);
    auto delay = std::make_unique<DelayNode> (delayId, delaySamples, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    DelayNode* const delayPtr = delay.get();
    delayPtr->setInput (sourcePtr);
    master->setInputNodes ({ delayPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = maxBlockSize;
    inputs.previousForCarryOver = previous;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (delay));
    inputs.nodes.push_back (std::move (master));
    return inputs;
}

GraphBuilder::Inputs faderInputs (float dc, NodeId faderId)
{
    auto source = std::make_unique<IdentityDcNode> (1, dc, 1);
    auto fader = std::make_unique<FaderNode> (faderId, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    FaderNode* const faderPtr = fader.get();
    faderPtr->setInput (sourcePtr);
    master->setInputNodes ({ faderPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 512;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (fader));
    inputs.nodes.push_back (std::move (master));
    return inputs;
}

GraphBuilder::Inputs panInputs (float dc, NodeId panId)
{
    auto source = std::make_unique<IdentityDcNode> (1, dc, 1);
    auto pan = std::make_unique<PanNode> (panId);
    auto master = std::make_unique<MasterNode> (kMasterId, 2);

    IdentityDcNode* const sourcePtr = source.get();
    PanNode* const panPtr = pan.get();
    panPtr->setInput (sourcePtr);
    master->setInputNodes ({ panPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 512;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (pan));
    inputs.nodes.push_back (std::move (master));
    return inputs;
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

struct AutomationProbeState
{
    std::size_t count = 0;
    Event events[8] {};
};

class AutomationProbeNode final : public Node
{
public:
    AutomationProbeNode (NodeId id, AutomationProbeState& state) noexcept : id_ (id), state_ (&state) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, 1, 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double, int) override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (state_ == nullptr)
            return;

        const std::span<const Event> events =
            args.automationEvents != nullptr ? args.automationEvents->events() : std::span<const Event> {};
        state_->count = events.size();
        const std::size_t n = events.size() < 8u ? events.size() : 8u;
        for (std::size_t i = 0; i < n; ++i)
            state_->events[i] = events[i];
    }

    void reset() noexcept override {}
    void release() override {}

    void setInput (Node* input) noexcept { input_ = input; }

private:
    NodeId id_;
    AutomationProbeState* state_ = nullptr;
    Node* input_ = nullptr;
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

TEST_CASE ("GraphBuilder compiles PlaceholderNode as a silent graph node", "[builder][placeholder]")
{
    constexpr NodeId kPlaceholderId = 41;

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph =
        GraphBuilder::build (placeholderInputs (0.75f, kPlaceholderId), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Placeholder) == 1u);

    const CompiledNode* const placeholder = compiledNodeById (*graph, kPlaceholderId);
    REQUIRE (placeholder != nullptr);
    REQUIRE (placeholder->kind == CompiledNodeKind::Placeholder);
    REQUIRE (placeholder->numInputs == 1u);
    REQUIRE (graph->isMuteCapable (kPlaceholderId));

    const std::vector<float> out = render (*graph, 64);
    for (float v : out)
        REQUIRE (v == 0.0f);
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

TEST_CASE ("CompiledGraph mute flips take effect without rebuilding", "[builder][mute]")
{
    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph =
        GraphBuilder::build (inputsWithMaster (std::make_unique<IdentityDcNode> (1, 0.5f, 1)), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->debugMuteMask() == 0u);

    std::vector<float> out = render (*graph, 16);
    for (float v : out)
        REQUIRE (v == 0.5f);

    REQUIRE (graph->setMuted (1, true));
    REQUIRE (graph->isMuted (1));
    REQUIRE (graph->debugMuteMask() != 0u);

    out = render (*graph, 16);
    for (float v : out)
        REQUIRE (v == 0.0f);

    REQUIRE (graph->setMuted (1, false));
    REQUIRE_FALSE (graph->isMuted (1));

    out = render (*graph, 16);
    for (float v : out)
        REQUIRE (v == 0.5f);

    REQUIRE_FALSE (graph->setMuted (123456u, true));
}

TEST_CASE ("CompiledGraph applySetGain only mutates Fader nodes", "[builder][scalar][gain]")
{
    constexpr NodeId kFaderId = 2;

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (faderInputs (1.0f, kFaderId), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    REQUIRE_FALSE (graph->applySetGain (1, 0.0f));          // wrong kind: IdentityDcNode
    REQUIRE_FALSE (graph->applySetPan (kFaderId, 1.0f));    // wrong command for FaderNode

    std::vector<float> out = render (*graph, 512);
    REQUIRE (out.back() == 1.0f);

    REQUIRE (graph->applySetGain (kFaderId, 0.25f));
    out = render (*graph, 512);
    REQUIRE (out.back() == Approx (0.25f).margin (1.0e-6f));

    REQUIRE_FALSE (graph->applySetGain (123456u, 0.0f));
    out = render (*graph, 512);
    REQUIRE (out.back() == Approx (0.25f).margin (1.0e-6f));
}

TEST_CASE ("CompiledGraph applySetPan only mutates Pan nodes", "[builder][scalar][pan]")
{
    constexpr NodeId kPanId = 2;

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (panInputs (1.0f, kPanId), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    REQUIRE_FALSE (graph->applySetPan (1, 1.0f));          // wrong kind: IdentityDcNode
    REQUIRE_FALSE (graph->applySetGain (kPanId, 0.0f));   // wrong command for PanNode

    std::vector<float> out = render (*graph, 512);
    REQUIRE (out.back() == Approx (std::sqrt (0.5f)).margin (1.0e-4f));

    REQUIRE (graph->applySetPan (kPanId, 1.0f));
    out = render (*graph, 512);
    REQUIRE (out.back() == Approx (0.0f).margin (1.0e-4f));

    REQUIRE_FALSE (graph->applySetPan (123456u, -1.0f));
    out = render (*graph, 512);
    REQUIRE (out.back() == Approx (0.0f).margin (1.0e-4f));
}

TEST_CASE ("GraphBuilder carries compiled automation lane metadata", "[builder][automation][h15][cp3]")
{
    constexpr NodeId kFaderId = 2;

    GraphBuilder::Inputs inputs = faderInputs (1.0f, kFaderId);
    CompiledAutomationLane lane;
    lane.targetNode = kFaderId;
    lane.parameterId = FaderNode::kGainParameterId;
    lane.frames = { 0, 64, 128 };
    lane.values = { 0.0, 0.5, 1.0 };
    lane.curveTypes = { AutomationCurveType::Linear, AutomationCurveType::Linear, AutomationCurveType::Hold };
    inputs.automationLanes.push_back (std::move (lane));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const std::span<const CompiledAutomationLane> lanes = graph->debugAutomationLanes();
    REQUIRE (lanes.size() == 1u);
    REQUIRE (lanes[0].targetNode == kFaderId);
    REQUIRE (lanes[0].parameterId == FaderNode::kGainParameterId);
    REQUIRE (lanes[0].frames == std::vector<std::int64_t> { 0, 64, 128 });
    REQUIRE (lanes[0].values == std::vector<double> { 0.0, 0.5, 1.0 });
    REQUIRE (lanes[0].curveTypes == std::vector<AutomationCurveType> {
        AutomationCurveType::Linear,
        AutomationCurveType::Linear,
        AutomationCurveType::Hold });

    // CP3 scheduler prerequisite: the presence of compiled lanes alone makes an otherwise fader-only
    // zero-latency graph unsafe for parallel block dispatch.
    REQUIRE_FALSE (graph->isBlockParallelSafe());

    const std::vector<float> out = render (*graph, 64);
    for (float v : out)
        REQUIRE (v == 1.0f);
}

TEST_CASE ("CompiledGraph emits compiled automation lanes into the ProcessArgs side-band",
           "[builder][automation][runtime][h15][cp3]")
{
    constexpr NodeId kProbeId = 44;
    constexpr yesdaw::engine::ParameterId kParameterId = 7;
    AutomationProbeState state;

    auto source = std::make_unique<IdentityDcNode> (1, 0.25f, 1);
    auto probe = std::make_unique<AutomationProbeNode> (kProbeId, state);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    AutomationProbeNode* const probePtr = probe.get();
    probePtr->setInput (sourcePtr);
    master->setInputNodes ({ probePtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 128;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (probe));
    inputs.nodes.push_back (std::move (master));

    CompiledAutomationLane lane;
    lane.targetNode = kProbeId;
    lane.parameterId = kParameterId;
    lane.frames = { 32, 160 };
    lane.values = { 0.25, 0.75 };
    lane.curveTypes = { AutomationCurveType::Linear, AutomationCurveType::Hold };
    inputs.automationLanes.push_back (std::move (lane));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    std::vector<float> out (128, -999.0f);
    float* outChannels[1] = { out.data() };
    yesdaw::engine::EventStream events;
    Transport transport;
    transport.timelineFrame = 0;
    transport.hasTimelineFrame = true;
    graph->process (outChannels, 1, static_cast<int> (out.size()), events, transport);

    REQUIRE (state.count == 2u);
    REQUIRE (state.events[0].timeInBlock == 32u);
    REQUIRE (state.events[0].payload.parameter.targetNode == kProbeId);
    REQUIRE (state.events[0].payload.parameter.parameterId == kParameterId);
    REQUIRE (state.events[0].payload.parameter.normalizedValue == Approx (0.25));
    REQUIRE (state.events[1].timeInBlock == 64u);
    REQUIRE (state.events[1].payload.parameter.targetNode == kProbeId);
    REQUIRE (state.events[1].payload.parameter.parameterId == kParameterId);
    REQUIRE (state.events[1].payload.parameter.normalizedValue == Approx (0.375));

    for (float v : out)
        REQUIRE (v == 0.25f);
}

TEST_CASE ("CompiledGraph automation lane cursors continue across sequential Blocks",
           "[builder][automation][runtime][cursor][h15][cp3]")
{
    constexpr NodeId kProbeId = 45;
    constexpr yesdaw::engine::ParameterId kParameterId = 8;
    AutomationProbeState state;

    auto source = std::make_unique<IdentityDcNode> (1, 0.5f, 1);
    auto probe = std::make_unique<AutomationProbeNode> (kProbeId, state);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    AutomationProbeNode* const probePtr = probe.get();
    probePtr->setInput (sourcePtr);
    master->setInputNodes ({ probePtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 128;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (probe));
    inputs.nodes.push_back (std::move (master));

    CompiledAutomationLane lane;
    lane.targetNode = kProbeId;
    lane.parameterId = kParameterId;
    lane.frames = { 32, 160 };
    lane.values = { 0.25, 0.75 };
    lane.curveTypes = { AutomationCurveType::Linear, AutomationCurveType::Hold };
    inputs.automationLanes.push_back (std::move (lane));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    std::vector<float> out (128, -999.0f);
    float* outChannels[1] = { out.data() };
    yesdaw::engine::EventStream events;
    Transport transport;
    transport.hasTimelineFrame = true;

    transport.timelineFrame = 0;
    graph->process (outChannels, 1, 96, events, transport);
    REQUIRE (state.count == 2u);
    REQUIRE (state.events[0].timeInBlock == 32u);
    REQUIRE (state.events[0].payload.parameter.normalizedValue == Approx (0.25));
    REQUIRE (state.events[1].timeInBlock == 64u);
    REQUIRE (state.events[1].payload.parameter.normalizedValue == Approx (0.375));

    const std::span<const yesdaw::engine::CompiledAutomationLaneCursor> cursors =
        graph->debugAutomationLaneCursors();
    REQUIRE (cursors.size() == 1u);
    REQUIRE (cursors[0].initialized);
    REQUIRE (cursors[0].nextBreakpointIndex == 1u);
    REQUIRE (cursors[0].controlSegmentIndex == 0u);
    REQUIRE (cursors[0].nextControlFrame == 128);
    REQUIRE (cursors[0].lastBlockEnd == 96);

    transport.timelineFrame = 96;
    graph->process (outChannels, 1, 96, events, transport);
    REQUIRE (state.count == 2u);
    REQUIRE (state.events[0].timeInBlock == 32u);
    REQUIRE (state.events[0].payload.parameter.targetNode == kProbeId);
    REQUIRE (state.events[0].payload.parameter.parameterId == kParameterId);
    REQUIRE (state.events[0].payload.parameter.normalizedValue == Approx (0.625));
    REQUIRE (state.events[1].timeInBlock == 64u);
    REQUIRE (state.events[1].payload.parameter.targetNode == kProbeId);
    REQUIRE (state.events[1].payload.parameter.parameterId == kParameterId);
    REQUIRE (state.events[1].payload.parameter.normalizedValue == Approx (0.75));

    for (int i = 0; i < 96; ++i)
        REQUIRE (out[static_cast<std::size_t> (i)] == 0.5f);
}

TEST_CASE ("CompiledGraph automation lane cursors reset on discontinuous locate and loop Blocks",
           "[builder][automation][runtime][cursor][locate][h15][cp3]")
{
    constexpr NodeId kProbeId = 46;
    constexpr yesdaw::engine::ParameterId kParameterId = 9;
    AutomationProbeState state;

    auto source = std::make_unique<IdentityDcNode> (1, 0.75f, 1);
    auto probe = std::make_unique<AutomationProbeNode> (kProbeId, state);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const sourcePtr = source.get();
    AutomationProbeNode* const probePtr = probe.get();
    probePtr->setInput (sourcePtr);
    master->setInputNodes ({ probePtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 192;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (probe));
    inputs.nodes.push_back (std::move (master));

    CompiledAutomationLane lane;
    lane.targetNode = kProbeId;
    lane.parameterId = kParameterId;
    lane.frames = { 32, 160 };
    lane.values = { 0.25, 0.75 };
    lane.curveTypes = { AutomationCurveType::Linear, AutomationCurveType::Hold };
    inputs.automationLanes.push_back (std::move (lane));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    std::vector<float> out (192, -999.0f);
    float* outChannels[1] = { out.data() };
    yesdaw::engine::EventStream events;
    Transport transport;
    transport.hasTimelineFrame = true;

    transport.timelineFrame = 0;
    graph->process (outChannels, 1, 192, events, transport);
    REQUIRE (state.count == 4u);

    const std::span<const yesdaw::engine::CompiledAutomationLaneCursor> cursors =
        graph->debugAutomationLaneCursors();
    REQUIRE (cursors.size() == 1u);
    REQUIRE (cursors[0].initialized);
    REQUIRE (cursors[0].lastBlockEnd == 192);

    transport.timelineFrame = 96;
    graph->process (outChannels, 1, 96, events, transport);
    REQUIRE (state.count == 3u);
    REQUIRE (state.events[0].timeInBlock == 0u);
    REQUIRE (state.events[0].payload.parameter.targetNode == kProbeId);
    REQUIRE (state.events[0].payload.parameter.parameterId == kParameterId);
    REQUIRE (state.events[0].payload.parameter.normalizedValue == Approx (0.5));
    REQUIRE (state.events[1].timeInBlock == 32u);
    REQUIRE (state.events[1].payload.parameter.normalizedValue == Approx (0.625));
    REQUIRE (state.events[2].timeInBlock == 64u);
    REQUIRE (state.events[2].payload.parameter.normalizedValue == Approx (0.75));
    REQUIRE (cursors[0].lastBlockEnd == 192);

    transport.timelineFrame = 32;
    graph->process (outChannels, 1, 64, events, transport);
    REQUIRE (state.count == 2u);
    REQUIRE (state.events[0].timeInBlock == 0u);
    REQUIRE (state.events[0].payload.parameter.normalizedValue == Approx (0.25));
    REQUIRE (state.events[1].timeInBlock == 32u);
    REQUIRE (state.events[1].payload.parameter.normalizedValue == Approx (0.375));
    REQUIRE (cursors[0].lastBlockEnd == 96);

    for (int i = 0; i < 64; ++i)
        REQUIRE (out[static_cast<std::size_t> (i)] == 0.75f);
}

TEST_CASE ("GraphBuilder rejects unresolved compiled automation lane targets", "[builder][automation][h15][cp3]")
{
    constexpr NodeId kMissingAutomationTarget = 123456u;

    GraphBuilder::Inputs inputs = faderInputs (1.0f, 2);
    CompiledAutomationLane lane;
    lane.targetNode = kMissingAutomationTarget;
    lane.parameterId = FaderNode::kGainParameterId;
    lane.frames = { 0 };
    lane.values = { 0.5 };
    lane.curveTypes = { AutomationCurveType::Linear };
    inputs.automationLanes.push_back (std::move (lane));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::InvalidAutomationLane);
    REQUIRE (error.nodeId() == kMissingAutomationTarget);
}

TEST_CASE ("GraphBuilder rejects compiled automation lanes that exceed the per-block event budget",
           "[builder][automation][budget][h15][cp3]")
{
    constexpr NodeId kFaderId = 2;
    constexpr int kMaxBlockSize = 512;
    constexpr std::size_t kEventsPerLane = static_cast<std::size_t> (kMaxBlockSize) / 64u + 2u;
    constexpr std::size_t kMaxBudgetedLanes = CompiledGraph::kMaxEventsPerBlock / kEventsPerLane;

    auto buildWithLaneCount = [] (std::size_t laneCount)
    {
        GraphBuilder::Inputs inputs = faderInputs (1.0f, kFaderId);
        inputs.maxBlockSize = kMaxBlockSize;

        for (std::size_t i = 0; i < laneCount; ++i)
        {
            CompiledAutomationLane lane;
            lane.targetNode = kFaderId;
            lane.parameterId = FaderNode::kGainParameterId;
            lane.frames = { 0 };
            lane.values = { 0.5 };
            lane.curveTypes = { AutomationCurveType::Hold };
            inputs.automationLanes.push_back (std::move (lane));
        }

        return inputs;
    };

    GraphBuildError boundaryError;
    std::unique_ptr<CompiledGraph> boundary =
        GraphBuilder::build (buildWithLaneCount (kMaxBudgetedLanes), &boundaryError);
    REQUIRE (boundary != nullptr);
    REQUIRE (boundaryError.code() == GraphBuildError::Code::None);
    REQUIRE (boundary->debugAutomationLanes().size() == kMaxBudgetedLanes);

    GraphBuildError overBudgetError;
    std::unique_ptr<CompiledGraph> overBudget =
        GraphBuilder::build (buildWithLaneCount (kMaxBudgetedLanes + 1u), &overBudgetError);
    REQUIRE (overBudget == nullptr);
    REQUIRE (overBudgetError.code() == GraphBuildError::Code::AutomationEventBudgetExceeded);
    REQUIRE (overBudgetError.nodeId() == kFaderId);
}

TEST_CASE ("GraphBuilder carries matching DelayNode state across rebuilds", "[builder][carry-over]")
{
    constexpr NodeId kDelayId = 22;
    constexpr int kDelay = 4;
    constexpr int kFrames = 4;

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> previous =
        GraphBuilder::build (delayedIdentityInputs (1.0f, kDelayId, kDelay, kFrames), &error);
    REQUIRE (previous != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const std::vector<float> priming = render (*previous, kFrames);
    for (float v : priming)
        REQUIRE (v == 0.0f);

    std::unique_ptr<CompiledGraph> next =
        GraphBuilder::build (delayedIdentityInputs (0.0f, kDelayId, kDelay, kFrames, previous.get()), &error);
    REQUIRE (next != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const std::vector<float> carried = render (*next, kFrames);
    for (float v : carried)
        REQUIRE (v == 1.0f);
}

TEST_CASE ("GraphBuilder carry-over zero-fills mismatched DelayNode tails", "[builder][carry-over]")
{
    constexpr NodeId kDelayId = 33;
    constexpr int kFrames = 4;

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> previous =
        GraphBuilder::build (delayedIdentityInputs (1.0f, kDelayId, 3, kFrames), &error);
    REQUIRE (previous != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    (void) render (*previous, kFrames);

    const DelayCacheEntry* const oldEntry = delayCacheByKey (*previous, kDelayId);
    REQUIRE (oldEntry != nullptr);
    const std::size_t oldRingSize = oldEntry->ring.size();

    std::unique_ptr<CompiledGraph> next =
        GraphBuilder::build (delayedIdentityInputs (0.0f, kDelayId, 12, kFrames, previous.get()), &error);
    REQUIRE (next != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const DelayCacheEntry* const newEntry = delayCacheByKey (*next, kDelayId);
    REQUIRE (newEntry != nullptr);
    REQUIRE (newEntry->ring.size() > oldRingSize);

    for (std::size_t i = oldRingSize; i < newEntry->ring.size(); ++i)
        REQUIRE (newEntry->ring[i] == 0.0f);

    const std::vector<float> out = render (*next, kFrames);
    for (float v : out)
        REQUIRE (std::isfinite (v));
}

TEST_CASE ("GraphBuilder sorts multi-input metadata by producer id and binds buses", "[builder][bind][order]")
{
    auto a = std::make_unique<IdentityDcNode> (30, 0.25f, 1);
    auto b = std::make_unique<IdentityDcNode> (10, 0.5f, 1);
    auto c = std::make_unique<IdentityDcNode> (20, 0.125f, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    IdentityDcNode* const aPtr = a.get();
    IdentityDcNode* const bPtr = b.get();
    IdentityDcNode* const cPtr = c.get();
    master->setInputNodes ({ aPtr, bPtr, cPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = 16;
    inputs.nodes.push_back (std::move (a));
    inputs.nodes.push_back (std::move (master));
    inputs.nodes.push_back (std::move (b));
    inputs.nodes.push_back (std::move (c));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->debugMultiInputNodesBound());

    const CompiledNode* const masterNode = compiledNodeById (*graph, kMasterId);
    REQUIRE (masterNode != nullptr);
    REQUIRE (masterNode->numInputs == 3u);

    const std::span<const InputSlot> slots = graph->debugInputSlots();
    const std::span<const CompiledNode> nodes = graph->debugCompiledNodes();
    REQUIRE (slots.size() >= static_cast<std::size_t> (masterNode->inputsBegin) + masterNode->numInputs);

    std::vector<NodeId> producerIds;
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t> (masterNode->numInputs); ++i)
    {
        const InputSlot& slot = slots[static_cast<std::size_t> (masterNode->inputsBegin) + i];
        producerIds.push_back (nodes[slot.producerNodeIdx].id);
    }

    REQUIRE (producerIds == std::vector<NodeId> { 10u, 20u, 30u });

    const std::vector<float> out = render (*graph, 16);
    for (float v : out)
        REQUIRE (v == 0.875f);
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

TEST_CASE ("GraphBuilder PDC aligns convergence impulses through a SumNode", "[builder][pdc][sum]")
{
    // The Master convergence test above proves PDC alignment for a 2-input Master. The H1 coverage
    // review found the SAME alignment was never proven for a SumNode convergence (it runs the same
    // splice logic). One branch is delayed kLatency, the other is direct; both feed a SumNode -> PDC
    // must splice the direct branch so a single impulse lands at exactly frame kLatency and the two
    // aligned paths sum to 2.0.
    constexpr int kLatency = 9;
    constexpr int kFrames = 32;

    auto source = std::make_unique<ImpulseNode> (1);
    auto latent = std::make_unique<StubLatencyNode> (2, kLatency);
    auto sum = std::make_unique<SumNode> (3, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    ImpulseNode* const sourcePtr = source.get();
    StubLatencyNode* const latentPtr = latent.get();
    SumNode* const sumPtr = sum.get();
    latentPtr->setInput (sourcePtr);
    sumPtr->setInputNodes ({ sourcePtr, latentPtr });
    master->setInputNodes ({ sumPtr });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = kFrames;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (latent));
    inputs.nodes.push_back (std::move (sum));
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
    inputs.maxBlockSize = 32;
    inputs.nodes.push_back (std::move (a));
    inputs.nodes.push_back (std::move (delay));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    // The H1 coverage review found this test only proved the feedback graph BUILDS. Now actually run it:
    // a DelayNode feedback boundary is excluded from the forward longest-path so latency stays finite,
    // and with nothing exciting the loop the output must be exact silence. A stale/garbage feedback read
    // would surface as a non-zero sample or a NaN from the debug pool-paint (NaN != 0 fails the check).
    REQUIRE (graph->totalLatency() >= 0);
    REQUIRE (graph->totalLatency() < 32);

    const std::vector<float> out = render (*graph, 32);
    for (float v : out)
        REQUIRE (v == 0.0f);
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

TEST_CASE ("GraphBuilder binds the maximum representable bus fan-in without uint16 iterator wrap", "[builder][validate]")
{
    auto source = std::make_unique<IdentityDcNode> (1, 1.0f, 1);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    std::vector<Node*> inputs (GraphBuilder::kMaxInputsPerNode, source.get());
    master->setInputNodes (std::move (inputs));

    GraphBuilder::Inputs buildInputs;
    buildInputs.masterNodeId = kMasterId;
    buildInputs.nodes.push_back (std::move (source));
    buildInputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (buildInputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const CompiledNode* masterNode = compiledNodeById (*graph, kMasterId);
    REQUIRE (masterNode != nullptr);
    REQUIRE (masterNode->numInputs == static_cast<std::uint16_t> (GraphBuilder::kMaxInputsPerNode));
}
