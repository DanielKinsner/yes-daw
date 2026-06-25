// YES DAW - PluginNode IPC-proxy checks (ADR-0015 / ADR-0013 / ADR-0008 / ADR-0007).
//
// Proves the graph-visible PluginNode adapter end to end through the REAL GraphBuilder + CompiledGraph:
//   - a PluginNode inside a compiled graph delivers its stub child's output EXACTLY one pipeline Block
//     late (the deterministic single-Block latency PDC depends on - ADR-0007);
//   - it FAILS OPEN (last-good -> silence -> bypass) when the stub child stalls, never blocking the audio
//     thread and never emitting garbage, and recovers to Fresh when the child catches up (ADR-0002 /0015);
//   - its reported latency (one pipeline Block + the validated plugin latency) DRIVES PDC convergence:
//     an alignment-sensitive multiply lands a single impulse only because PDC delayed the parallel path;
//   - plugin-reported latency/channels are VALIDATED before they reach the compiler, so a hostile plugin
//     cannot overflow the PDC walk (ADR-0015).
//
// Headless + pure C++ (no JUCE): the "plugin" is the ring's in-process stub child, driven synchronously by
// the test between audio Blocks (modelling the real child process publishing off the audio thread). Built
// unconditionally so the RTSan leg covers PluginNode::process() (exchangeBlock) and the TSan leg covers it.

#include "engine/GraphBuilder.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/SidechainGainNode.h"
#include "engine/plugin/PluginNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

using Catch::Approx;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNodeKind;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::MasterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::PluginNode;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::RtLaneOutput;
using yesdaw::engine::SidechainGainNode;

namespace {

constexpr NodeId kMasterId = 90000;

// A source whose every sample encodes (blockIndex, channel, frame), so a delivered output can be matched
// to the EXACT input Block it came from - which is what makes "exactly one Block late" a real assertion
// rather than a steady-state coincidence. Stateful: the block counter advances once per process().
class BlockSignalSource final : public Node
{
public:
    BlockSignalSource (NodeId id, int channels) noexcept
        : id_ (id), channels_ (channels > 0 ? channels : 1) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, channels_, 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }
    void prepare (double, int) override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        const int channels = args.audio.numChannels < channels_ ? args.audio.numChannels : channels_;
        for (int c = 0; c < channels; ++c)
        {
            float* const out = args.audio.channels[c];
            for (int f = 0; f < args.numFrames; ++f)
                out[f] = valueFor (block_, c, f);
        }
        ++block_;
    }

    void reset() noexcept override { block_ = 0; }
    void release() override {}

    static float valueFor (std::uint64_t block, int channel, int frame) noexcept
    {
        return static_cast<float> (block) * 1000.0f
             + static_cast<float> (channel) * 1.0e6f
             + static_cast<float> (frame);
    }

private:
    NodeId        id_;
    int           channels_;
    std::uint64_t block_ = 0;
};

// A mono source that fires a single unit impulse at frame 0 of its FIRST processed Block, then silence.
// Tracking ONE impulse through the pipeline is what makes a one-Block-misalignment visible.
class OneShotImpulseSource final : public Node
{
public:
    explicit OneShotImpulseSource (NodeId id) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, 1, 0, id_ };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }
    void prepare (double, int) override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        float* const out = args.audio.channels[0];
        for (int f = 0; f < args.numFrames; ++f)
            out[f] = 0.0f;

        if (! fired_ && args.numFrames > 0)
        {
            out[0] = 1.0f;
            fired_ = true;
        }
    }

    void reset() noexcept override { fired_ = false; }
    void release() override {}

private:
    NodeId id_;
    bool   fired_ = false;
};

std::vector<float> render (CompiledGraph& graph, int frames)
{
    std::vector<float> out (static_cast<std::size_t> (frames), -999.0f);
    graph.process (out.data(), frames);
    return out;
}

} // namespace

TEST_CASE ("PluginNode delivers its stub child's output exactly one Block late", "[plugin][graph][pipeline]")
{
    constexpr NodeId kSrc = 200, kPlugin = 201;
    constexpr int    B  = 16;   // maxBlockSize == one pipeline Block
    constexpr int    ch = 1;

    auto src    = std::make_unique<BlockSignalSource> (kSrc, ch);
    auto plugin = std::make_unique<PluginNode> (kPlugin, ch, B);   // identity stub child by default
    plugin->setInput (src.get());
    PluginNode* const p = plugin.get();

    auto master = std::make_unique<MasterNode> (kMasterId, ch);
    master->setInputNodes ({ plugin.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 1;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = B;
    inputs.nodes.push_back (std::move (src));
    inputs.nodes.push_back (std::move (plugin));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    // Identity stub reports 0 internal latency, so the whole node's latency is exactly one pipeline Block.
    REQUIRE (graph->totalLatency() == B);

    constexpr std::uint64_t kBlocks = 6;
    for (std::uint64_t n = 0; n < kBlocks; ++n)
    {
        const std::vector<float> out = render (*graph, B);

        if (n == 0)
        {
            for (float v : out)
                REQUIRE (v == Approx (0.0f).margin (1.0e-6f));   // one-Block pipeline primes with silence
        }
        else
        {
            for (int f = 0; f < B; ++f)
                REQUIRE (out[static_cast<std::size_t> (f)] == Approx (BlockSignalSource::valueFor (n - 1, 0, f)));
        }

        // The stub child processes the freshly-published input Block, off the audio thread.
        REQUIRE (p->serviceStubChild());
    }
}

TEST_CASE ("PluginNode fails open (last-good -> silence -> bypass) when the stub child stalls, then recovers",
           "[plugin][graph][failopen]")
{
    constexpr NodeId kSrc = 210, kPlugin = 211;
    constexpr int    B  = 8, ch = 1;

    auto src    = std::make_unique<BlockSignalSource> (kSrc, ch);
    auto plugin = std::make_unique<PluginNode> (kPlugin, ch, B);
    plugin->setFailOpenThresholds (/*lastGoodHold*/ 2, /*bypassAfter*/ 5);
    plugin->setInput (src.get());
    PluginNode* const p = plugin.get();

    auto master = std::make_unique<MasterNode> (kMasterId, ch);
    master->setInputNodes ({ plugin.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 2;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = B;
    inputs.nodes.push_back (std::move (src));
    inputs.nodes.push_back (std::move (plugin));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);

    // Prime: the child keeps up for a few Blocks. Block 0 = silence; Blocks 1-2 are Fresh.
    for (std::uint64_t n = 0; n <= 2; ++n)
    {
        render (*graph, B);
        REQUIRE (p->serviceStubChild());
    }

    // Block 3 is still Fresh (the child produced input Block 2 during Block 2's service), delivering s(2).
    // From here on the child has stalled - no more serviceStubChild() calls. s(2) becomes last-good.
    {
        const std::vector<float> out = render (*graph, B);
        REQUIRE (p->lastOutputSource() == RtLaneOutput::Fresh);
        for (int f = 0; f < B; ++f)
            REQUIRE (out[static_cast<std::size_t> (f)] == Approx (BlockSignalSource::valueFor (2, 0, f)));
    }

    // Misses 1-2: re-serve last-good (== s(2)). The audio thread NEVER blocks; output is always defined.
    for (int miss = 1; miss <= 2; ++miss)
    {
        const std::vector<float> out = render (*graph, B);
        REQUIRE (p->lastOutputSource() == RtLaneOutput::LastGood);
        for (int f = 0; f < B; ++f)
            REQUIRE (out[static_cast<std::size_t> (f)] == Approx (BlockSignalSource::valueFor (2, 0, f)));
    }

    // Misses 3-4: last-good held long enough -> silence.
    for (int miss = 3; miss <= 4; ++miss)
    {
        const std::vector<float> out = render (*graph, B);
        REQUIRE (p->lastOutputSource() == RtLaneOutput::Silence);
        for (float v : out)
            REQUIRE (v == Approx (0.0f).margin (1.0e-6f));
    }

    // Miss 5: bypass latches (the coordinator's cue to swap a placeholder - a later chunk).
    {
        const std::vector<float> out = render (*graph, B);
        REQUIRE (p->lastOutputSource() == RtLaneOutput::Bypass);
        REQUIRE (p->bypassActive());
        for (float v : out)
            REQUIRE (v == Approx (0.0f).margin (1.0e-6f));
    }

    // Recovery: the child drains the backlog off the audio thread; the audio thread sees Fresh again.
    while (p->serviceStubChild()) { }
    {
        const std::vector<float> out = render (*graph, B);
        REQUIRE (p->lastOutputSource() == RtLaneOutput::Fresh);
        REQUIRE_FALSE (p->bypassActive());
    }
}

TEST_CASE ("PluginNode's reported latency drives PDC convergence (alignment-sensitive)", "[plugin][graph][pdc]")
{
    constexpr NodeId kMainSrc = 220, kSideSrc = 221, kPlugin = 222, kSc = 223;
    constexpr int    B = 8;

    // Main path: impulse -> PluginNode (identity stub, plugin latency 0 -> path latency == one Block B).
    // Sidechain path: impulse -> [PDC must splice LatencyNode(B)] -> sidechain pin.
    // The two impulses coincide at exactly one (Block, frame) ONLY if PDC delayed the sidechain path by one
    // Block to match the plugin's one-Block pipeline latency. No compensation -> the product is all zeros.
    auto mainSrc = std::make_unique<OneShotImpulseSource> (kMainSrc);
    auto plugin  = std::make_unique<PluginNode> (kPlugin, 1, B);
    plugin->setInput (mainSrc.get());
    PluginNode* const p = plugin.get();

    auto sideSrc = std::make_unique<OneShotImpulseSource> (kSideSrc);
    auto sc      = std::make_unique<SidechainGainNode> (kSc, 1);
    sc->setMainInput (plugin.get());
    sc->setSidechainInput (sideSrc.get());

    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ sc.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 3;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = B;
    inputs.nodes.push_back (std::move (mainSrc));
    inputs.nodes.push_back (std::move (plugin));
    inputs.nodes.push_back (std::move (sideSrc));
    inputs.nodes.push_back (std::move (sc));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    // Structural PDC: the longest path is one pipeline Block, and a LatencyNode was spliced onto the shorter
    // (sidechain) path to align it.
    REQUIRE (graph->totalLatency() == B);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Latency) >= 1u);

    // Behavioural PDC: render two Blocks, servicing the child between them. The aligned impulse lands at
    // exactly Block 1 / frame 0; everything else is zero.
    const std::vector<float> b0 = render (*graph, B);
    REQUIRE (p->serviceStubChild());
    const std::vector<float> b1 = render (*graph, B);

    for (float v : b0)
        REQUIRE (v == Approx (0.0f).margin (1.0e-6f));

    int nonZero = 0;
    for (int f = 0; f < B; ++f)
    {
        const float expected = (f == 0) ? 1.0f : 0.0f;
        REQUIRE (b1[static_cast<std::size_t> (f)] == Approx (expected).margin (1.0e-5f));
        if (std::fabs (b1[static_cast<std::size_t> (f)]) > 1.0e-5f)
            ++nonZero;
    }
    REQUIRE (nonZero == 1);   // exactly one aligned impulse; a one-Block misalignment would be silent
}

TEST_CASE ("PluginNode reports one Block + validated plugin latency and quarantines impossible values",
           "[plugin][latency][validation]")
{
    constexpr int B = 32;
    PluginNode plugin (300, 1, B);

    // Default: no extra plugin latency -> exactly one pipeline Block of IPC latency.
    REQUIRE (plugin.properties().latencySamples == B);

    // A sane plugin latency adds to the one-Block pipeline latency.
    plugin.setReportedLatencySamples (100);
    REQUIRE (plugin.properties().latencySamples == B + 100);

    // A negative (impossible) latency is quarantined to zero - it must never reach PDC.
    plugin.setReportedLatencySamples (-5);
    REQUIRE (plugin.properties().latencySamples == B);

    // An absurd latency is clamped to a sane maximum so the PDC walk cannot overflow.
    plugin.setReportedLatencySamples (std::numeric_limits<std::int64_t>::max());
    const std::int64_t reported = plugin.properties().latencySamples;
    REQUIRE (reported == B + PluginNode::kMaxValidatedLatencySamples);
    REQUIRE (reported <= GraphBuilder::kMaxLatencyCap);   // within the compiler's cap -> accepted, not rejected

    // Channels are clamped into [1, kMaxChannels].
    REQUIRE (PluginNode (301, 999, B).properties().channels == PluginNode::kMaxChannels);
    REQUIRE (PluginNode (302, 0,   B).properties().channels == 1);
}

TEST_CASE ("a plugin reporting an absurd latency cannot overflow the PDC walk", "[plugin][graph][validation]")
{
    constexpr NodeId kSrc = 310, kPlugin = 311;
    constexpr int    B = 16;

    auto src    = std::make_unique<BlockSignalSource> (kSrc, 1);
    auto plugin = std::make_unique<PluginNode> (kPlugin, 1, B);
    plugin->setReportedLatencySamples (std::numeric_limits<std::int64_t>::max());   // hostile claim
    plugin->setInput (src.get());

    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ plugin.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 4;
    inputs.masterNodeId = kMasterId;
    inputs.maxBlockSize = B;
    inputs.nodes.push_back (std::move (src));
    inputs.nodes.push_back (std::move (plugin));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);

    // The node clamped its own claim, so the build SUCCEEDS (no overflow, no rejection) and reports the
    // clamped latency rather than crashing or quarantining the whole graph.
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->totalLatency() == B + PluginNode::kMaxValidatedLatencySamples);
}
