// YES DAW - H3 headless mixer graph projection checks.
//
// Proves the first mixer-as-graph foundation using existing Nodes only:
// source -> FaderNode -> PanNode -> MeterNode -> SumNode(master bus) -> MasterNode.

#include "engine/MixerGraphProjection.h"
#include "engine/nodes/IdentityDcNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

using Catch::Approx;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNode;
using yesdaw::engine::CompiledNodeKind;
using yesdaw::engine::FaderNode;
using yesdaw::engine::GraphId;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::MixerProjectionError;
using yesdaw::engine::MixerProjectionInputs;
using yesdaw::engine::MixerTrackProjection;
using yesdaw::engine::NodeId;
using yesdaw::engine::buildMixerGraphProjection;

namespace {

constexpr NodeId kMasterSumId = 61000;
constexpr NodeId kMasterId    = 61001;
constexpr int    kMaxBlock    = 512;
constexpr float  kCenterGain  = 0.70710677f;

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
