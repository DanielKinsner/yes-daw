// YES DAW - H3 headless mixer graph projection checks.
//
// Proves the mixer-as-graph foundation using existing Nodes only:
// source -> FaderNode -> PanNode -> MeterNode -> SumNode(master bus) -> MasterNode,
// with Send edges to Bus SumNodes whose Returns feed the master bus.

#include "engine/MixerGraphProjection.h"
#include "engine/nodes/IdentityDcNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

using Catch::Approx;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNode;
using yesdaw::engine::CompiledNodeKind;
using yesdaw::engine::FaderNode;
using yesdaw::engine::GraphId;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::MixerBusProjection;
using yesdaw::engine::MixerProjectionError;
using yesdaw::engine::MixerProjectionInputs;
using yesdaw::engine::MixerSendProjection;
using yesdaw::engine::MixerSendTap;
using yesdaw::engine::MixerTrackProjection;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::buildMixerGraphProjection;

namespace {

constexpr NodeId kMasterSumId = 61000;
constexpr NodeId kMasterId    = 61001;
constexpr int    kMaxBlock    = 512;
constexpr float  kCenterGain  = 0.70710677f;

class LatentImpulseSource final : public Node
{
public:
    LatentImpulseSource (NodeId id, std::int64_t latencySamples) noexcept
        : id_ (id), latencySamples_ (latencySamples)
    {
    }

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
    NodeId       id_ = 0;
    std::int64_t latencySamples_ = 0;
};

MixerProjectionInputs baseProjection (GraphId graphId)
{
    MixerProjectionInputs inputs;
    inputs.id = graphId;
    inputs.masterSumNodeId = kMasterSumId;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = 48000.0;
    inputs.maxBlockSize = kMaxBlock;
    return inputs;
}

MixerTrackProjection makeTrack (NodeId sourceId,
                                float dc,
                                NodeId faderId,
                                NodeId panId,
                                NodeId meterId,
                                float gain,
                                float pan)
{
    MixerTrackProjection track;
    track.source = std::make_unique<IdentityDcNode> (sourceId, dc, 1);
    track.faderNodeId = faderId;
    track.panNodeId = panId;
    track.meterNodeId = meterId;
    track.linearGain = gain;
    track.pan = pan;
    return track;
}

MixerTrackProjection makeImpulseTrack (NodeId sourceId,
                                       std::int64_t latencySamples,
                                       NodeId faderId,
                                       NodeId panId,
                                       NodeId meterId,
                                       float gain,
                                       float pan)
{
    MixerTrackProjection track;
    track.source = std::make_unique<LatentImpulseSource> (sourceId, latencySamples);
    track.faderNodeId = faderId;
    track.panNodeId = panId;
    track.meterNodeId = meterId;
    track.linearGain = gain;
    track.pan = pan;
    return track;
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

} // namespace

TEST_CASE ("Mixer projection renders an empty master bus as silence", "[mixer][projection][silence]")
{
    MixerProjectionInputs inputs = baseProjection (1);
    MixerProjectionError error;

    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::None);
    REQUIRE (graph->debugMultiInputNodesBound());
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Fader) == 0u);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Pan) == 0u);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Meter) == 0u);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Sum) == 1u);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Master) == 1u);

    const std::vector<float> out = render (*graph, 64);
    for (float v : out)
        REQUIRE (v == 0.0f);
}

TEST_CASE ("Mixer projection builds track fader pan meter chains into the master bus", "[mixer][projection]")
{
    MixerProjectionInputs inputs = baseProjection (2);
    inputs.tracks.push_back (makeTrack (100, 0.25f, 200, 300, 400, 0.5f, 0.0f));
    inputs.tracks.push_back (makeTrack (101, 0.50f, 201, 301, 401, 0.25f, 0.0f));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::None);
    REQUIRE (graph->debugMultiInputNodesBound());
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Fader) == 2u);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Pan) == 2u);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Meter) == 2u);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Sum) == 1u);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Master) == 1u);

    const CompiledNode* const masterSum = compiledNodeById (*graph, kMasterSumId);
    const CompiledNode* const master = compiledNodeById (*graph, kMasterId);
    REQUIRE (masterSum != nullptr);
    REQUIRE (master != nullptr);
    REQUIRE (masterSum->numInputs == 2u);
    REQUIRE (master->numInputs == 1u);

    const float expectedLeft = (0.25f * 0.5f + 0.50f * 0.25f) * kCenterGain;
    const std::vector<float> out = render (*graph, kMaxBlock);
    for (float v : out)
        REQUIRE (v == Approx (expectedLeft).margin (1.0e-4f));
}

TEST_CASE ("Mixer projection wires pre and post fader Sends into bus Returns", "[mixer][projection][send]")
{
    MixerProjectionInputs inputs = baseProjection (7);
    inputs.buses.push_back (MixerBusProjection { 62000 });

    MixerTrackProjection pre = makeTrack (150, 1.0f, 250, 350, 450, 0.0f, 0.0f);
    pre.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });

    MixerTrackProjection post = makeTrack (151, 1.0f, 251, 351, 451, 0.0f, 0.0f);
    post.sends.push_back (MixerSendProjection { 0, MixerSendTap::PostFader });

    inputs.tracks.push_back (std::move (pre));
    inputs.tracks.push_back (std::move (post));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::None);
    REQUIRE (graph->debugMultiInputNodesBound());

    const CompiledNode* const bus = compiledNodeById (*graph, 62000);
    const CompiledNode* const masterSum = compiledNodeById (*graph, kMasterSumId);
    REQUIRE (bus != nullptr);
    REQUIRE (masterSum != nullptr);
    REQUIRE (bus->kind == CompiledNodeKind::Sum);
    REQUIRE (bus->numInputs == 2u);
    REQUIRE (masterSum->numInputs == 3u);

    const std::vector<float> out = render (*graph, kMaxBlock);
    for (float v : out)
        REQUIRE (v == Approx (1.0f).margin (1.0e-4f));
}

TEST_CASE ("Mixer projection bus Return summing is deterministic across declaration order", "[mixer][projection][send]")
{
    auto build = [] (bool reversed)
    {
        MixerProjectionInputs inputs = baseProjection (8);
        inputs.buses.push_back (MixerBusProjection { 62010 });

        constexpr float kHuge = 1.0e20f;

        MixerTrackProjection big = makeTrack (160, kHuge, 260, 360, 460, 0.0f, 0.0f);
        big.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });

        MixerTrackProjection negBig = makeTrack (161, -kHuge, 261, 361, 461, 0.0f, 0.0f);
        negBig.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });

        MixerTrackProjection small = makeTrack (162, 1.0f, 262, 362, 462, 0.0f, 0.0f);
        small.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });

        if (reversed)
        {
            inputs.tracks.push_back (std::move (small));
            inputs.tracks.push_back (std::move (negBig));
            inputs.tracks.push_back (std::move (big));
        }
        else
        {
            inputs.tracks.push_back (std::move (big));
            inputs.tracks.push_back (std::move (negBig));
            inputs.tracks.push_back (std::move (small));
        }

        MixerProjectionError error;
        std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
        REQUIRE (graph != nullptr);
        REQUIRE (error.code == MixerProjectionError::Code::None);
        return graph;
    };

    std::unique_ptr<CompiledGraph> normal = build (false);
    std::unique_ptr<CompiledGraph> reversed = build (true);

    const std::vector<float> normalOut = render (*normal, kMaxBlock);
    const std::vector<float> reversedOut = render (*reversed, kMaxBlock);

    REQUIRE (normalOut.size() == reversedOut.size());
    for (std::size_t i = 0; i < normalOut.size(); ++i)
    {
        REQUIRE (normalOut[i] == Approx (1.0f).margin (0.0f));
        REQUIRE (reversedOut[i] == Approx (normalOut[i]).margin (0.0f));
    }
}

TEST_CASE ("Mixer projection lets GraphBuilder align Return convergence with PDC", "[mixer][projection][send][pdc]")
{
    constexpr int kLatency = 5;
    constexpr int kFrames = 24;

    MixerProjectionInputs inputs = baseProjection (9);
    inputs.maxBlockSize = kFrames;
    inputs.buses.push_back (MixerBusProjection { 62020 });

    MixerTrackProjection direct = makeImpulseTrack (170, 0, 270, 370, 470, 0.0f, -1.0f);
    direct.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });

    MixerTrackProjection latent = makeImpulseTrack (171, kLatency, 271, 371, 471, 0.0f, -1.0f);
    latent.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });

    inputs.tracks.push_back (std::move (direct));
    inputs.tracks.push_back (std::move (latent));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);

    REQUIRE (graph != nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::None);
    REQUIRE (graph->totalLatency() == kLatency);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Latency) >= 1u);

    const CompiledNode* const bus = compiledNodeById (*graph, 62020);
    REQUIRE (bus != nullptr);
    REQUIRE (bus->kind == CompiledNodeKind::Sum);
    REQUIRE (bus->numInputs == 2u);
    REQUIRE (bus->pathLatency == kLatency);

    const std::vector<float> out = render (*graph, kFrames);
    int nonZeroFrames = 0;
    for (int i = 0; i < kFrames; ++i)
    {
        const float expected = i == kLatency ? 2.0f : 0.0f;
        REQUIRE (out[static_cast<std::size_t> (i)] == Approx (expected).margin (1.0e-4f));
        if (std::fabs (out[static_cast<std::size_t> (i)]) > 1.0e-5f)
            ++nonZeroFrames;
    }
    REQUIRE (nonZeroFrames == 1);
}

TEST_CASE ("Mixer projection exposes fader and pan nodes to existing scalar routing", "[mixer][projection][scalar]")
{
    constexpr NodeId kFaderId = 220;
    constexpr NodeId kPanId   = 320;

    MixerProjectionInputs inputs = baseProjection (3);
    inputs.tracks.push_back (makeTrack (120, 1.0f, kFaderId, kPanId, 420, 1.0f, 0.0f));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::None);

    REQUIRE_FALSE (graph->applySetGain (kPanId, 0.25f));
    REQUIRE_FALSE (graph->applySetPan (kFaderId, -1.0f));

    std::vector<float> out = render (*graph, kMaxBlock);
    REQUIRE (out.back() == Approx (kCenterGain).margin (1.0e-4f));

    REQUIRE (graph->applySetGain (kFaderId, 0.25f));
    out = render (*graph, kMaxBlock);
    REQUIRE (out.back() == Approx (0.25f * kCenterGain).margin (1.0e-4f));

    REQUIRE (graph->applySetPan (kPanId, -1.0f));
    out = render (*graph, kMaxBlock);
    REQUIRE (out.back() == Approx (0.25f).margin (1.0e-4f));
}

TEST_CASE ("Mixer projection rejects non-mono track sources before graph build", "[mixer][projection][invalid]")
{
    MixerProjectionInputs inputs = baseProjection (4);

    MixerTrackProjection track;
    track.source = std::make_unique<IdentityDcNode> (130, 0.5f, 2);
    track.faderNodeId = 230;
    track.panNodeId = 330;
    track.meterNodeId = 430;
    inputs.tracks.push_back (std::move (track));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::UnsupportedTrackSource);
    REQUIRE (error.trackIndex == 0u);
}

TEST_CASE ("Mixer projection rejects invalid scalar values before graph build", "[mixer][projection][invalid]")
{
    {
        MixerProjectionInputs inputs = baseProjection (5);
        inputs.tracks.push_back (makeTrack (140, 0.5f, 240, 340, 440, -0.01f, 0.0f));

        MixerProjectionError error;
        std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
        REQUIRE (graph == nullptr);
        REQUIRE (error.code == MixerProjectionError::Code::InvalidTrackGain);
        REQUIRE (error.trackIndex == 0u);
    }

    {
        MixerProjectionInputs inputs = baseProjection (6);
        inputs.tracks.push_back (makeTrack (141, 0.5f, 241, 341, 441, 1.0f, 1.01f));

        MixerProjectionError error;
        std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
        REQUIRE (graph == nullptr);
        REQUIRE (error.code == MixerProjectionError::Code::InvalidTrackPan);
        REQUIRE (error.trackIndex == 0u);
    }
}

TEST_CASE ("Mixer projection validator rejects non-finite and out-of-range track gain", "[mixer][projection][invalid][gain]")
{
    using yesdaw::engine::mixerGainIsValid;

    // Musical gains pass: silence, unity, and a generous-but-bounded ceiling.
    REQUIRE (mixerGainIsValid (0.0f));
    REQUIRE (mixerGainIsValid (1.0f));
    REQUIRE (mixerGainIsValid (FaderNode::kMaxLinearGain));

    // Non-finite and negative gains are rejected.
    REQUIRE_FALSE (mixerGainIsValid (std::numeric_limits<float>::infinity()));
    REQUIRE_FALSE (mixerGainIsValid (-std::numeric_limits<float>::infinity()));
    REQUIRE_FALSE (mixerGainIsValid (std::numeric_limits<float>::quiet_NaN()));
    REQUIRE_FALSE (mixerGainIsValid (-0.001f));

    // A finite-but-absurd linear gain must be rejected too. Before the fix the validator's
    // `<= float max` upper bound was a tautology, so any finite value slipped through.
    REQUIRE_FALSE (mixerGainIsValid (std::nextafter (FaderNode::kMaxLinearGain, 1.0e30f)));
    REQUIRE_FALSE (mixerGainIsValid (1.0e30f));
}

TEST_CASE ("Mixer projection rejects an absurd track gain before graph build", "[mixer][projection][invalid][gain]")
{
    MixerProjectionInputs inputs = baseProjection (40);
    inputs.tracks.push_back (makeTrack (1400, 0.5f, 2400, 3400, 4400, 1.0e30f, 0.0f));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph == nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::InvalidTrackGain);
    REQUIRE (error.trackIndex == 0u);
}

TEST_CASE ("Mixer projection clamps a runtime SetGain so no inf or NaN reaches the output", "[mixer][projection][gain][rt]")
{
    constexpr NodeId kFaderId = 2410;

    MixerProjectionInputs inputs = baseProjection (41);
    // A large but finite source amplitude. Combined with an unclamped pathological runtime gain this
    // would overflow float to +inf at the FaderNode multiply (1e20 * 1e30 == +inf), poisoning the mix.
    inputs.tracks.push_back (makeTrack (1410, 1.0e20f, kFaderId, 3410, 4410, 1.0f, 0.0f));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::None);

    // Drive a pathological gain through the SetGain seam (the same path the audio thread takes).
    REQUIRE (graph->applySetGain (kFaderId, 1.0e30f));

    const std::vector<float> out = render (*graph, kMaxBlock);
    for (float v : out)
        REQUIRE (std::isfinite (v));

    // The gain is clamped to the ceiling, so the settled value is source * ceiling * centre pan.
    const float expected = 1.0e20f * FaderNode::kMaxLinearGain * kCenterGain;
    REQUIRE (out.back() == Approx (expected).margin (std::fabs (expected) * 1.0e-4f));
}

TEST_CASE ("Mixer projection rejects Sends to missing buses before graph build", "[mixer][projection][invalid]")
{
    MixerProjectionInputs inputs = baseProjection (10);

    MixerTrackProjection track = makeTrack (180, 0.5f, 280, 380, 480, 1.0f, 0.0f);
    track.sends.push_back (MixerSendProjection { 0, MixerSendTap::PostFader });
    inputs.tracks.push_back (std::move (track));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);

    REQUIRE (graph == nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::InvalidSendDestination);
    REQUIRE (error.trackIndex == 0u);
    REQUIRE (error.sendIndex == 0u);
}
