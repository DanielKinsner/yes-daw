// YES DAW - H3 headless mixer graph projection checks.
//
// Proves the mixer-as-graph foundation using existing Nodes only:
// source -> FaderNode -> PanNode -> MeterNode -> SumNode(master bus) -> MasterNode,
// with Send edges to Bus SumNodes whose Returns feed the master bus.

#include "engine/MixerGraphProjection.h"
#include "engine/ProjectMixerProjection.h"
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
using yesdaw::engine::Asset;
using yesdaw::engine::Clip;
using yesdaw::engine::EntityId;
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
using yesdaw::engine::Project;
using yesdaw::engine::ProjectMixerNodeRole;
using yesdaw::engine::ProjectMixerProjectionConfig;
using yesdaw::engine::ProjectMixerProjectionError;
using yesdaw::engine::SampleRate;
using yesdaw::engine::buildMixerGraphProjection;
using yesdaw::engine::projectMixerNodeIdForClip;
using yesdaw::engine::projectToMixerProjectionInputs;

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

struct StereoCapture
{
    std::vector<float> left;
    std::vector<float> right;
};

// Renders one block and captures BOTH master channels. process() fills the buffer it is handed with the
// master's channel 0 (left) and computes channel 1 (right) into the pool; debugMasterChannel(1) exposes
// the right so the gate can no longer be blind to a left-only bug. A silent/absent right reads as zeros.
StereoCapture render (CompiledGraph& graph, int frames)
{
    StereoCapture cap;
    cap.left.assign (static_cast<std::size_t> (frames), -999.0f);
    graph.process (cap.left.data(), frames);

    cap.right.assign (static_cast<std::size_t> (frames), 0.0f);
    if (const float* const r = graph.debugMasterChannel (1))
        for (int i = 0; i < frames; ++i)
            cap.right[static_cast<std::size_t> (i)] = r[i];

    return cap;
}

const CompiledNode* compiledNodeById (const CompiledGraph& graph, NodeId id)
{
    for (const CompiledNode& node : graph.debugCompiledNodes())
        if (node.id == id)
            return &node;

    return nullptr;
}

constexpr EntityId entityIdFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

Asset makeProjectAsset (std::uint8_t id, std::uint64_t frames)
{
    Asset asset;
    asset.id = entityIdFromLowByte (id);
    asset.contentHash.bytes.back() = id;
    asset.frames = frames;
    asset.sampleRate = SampleRate { 48000.0 };
    asset.channels = 1;
    return asset;
}

Clip makeProjectClip (std::uint8_t id, EntityId assetId, float gain)
{
    Clip clip;
    clip.id = entityIdFromLowByte (id);
    clip.assetId = assetId;
    clip.timelineStart = 0;
    clip.timelineLength = 15360;
    clip.srcOffset = 0;
    clip.srcLen = 64;
    clip.gain = gain;
    return clip;
}

Project makeMixerProjectionProject()
{
    Project project;
    project.id = entityIdFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };
    project.assets = {
        makeProjectAsset (2, 100),
        makeProjectAsset (3, 200),
    };
    project.clips = {
        makeProjectClip (4, project.assets[0].id, 0.5f),
        makeProjectClip (5, project.assets[1].id, 0.25f),
    };
    return project;
}

float sourceDcForAsset (const Asset& asset) noexcept
{
    return asset.frames == 100u ? 0.5f : 0.25f;
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

    const StereoCapture out = render (*graph, 64);
    for (float v : out.left)
        REQUIRE (v == 0.0f);
    for (float v : out.right)
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

    // Both tracks are centre-panned, so the master must carry the same summed value in L and R.
    const float expectedCenter = (0.25f * 0.5f + 0.50f * 0.25f) * kCenterGain;
    const StereoCapture out = render (*graph, kMaxBlock);
    for (float v : out.left)
        REQUIRE (v == Approx (expectedCenter).margin (1.0e-4f));
    for (float v : out.right)
        REQUIRE (v == Approx (expectedCenter).margin (1.0e-4f));
}

TEST_CASE ("Project projector emits MixerProjectionInputs from Project clips", "[mixer][projection][project]")
{
    const Project project = makeMixerProjectionProject();

    ProjectMixerProjectionConfig config;
    config.id = 70;
    config.masterSumNodeId = kMasterSumId;
    config.masterNodeId = kMasterId;
    config.maxBlockSize = kMaxBlock;

    MixerProjectionInputs projection;
    ProjectMixerProjectionError projectError;
    const bool projected = projectToMixerProjectionInputs (
        project,
        config,
        [] (const Project&, const Clip&, const Asset& asset, NodeId expectedSourceId)
            -> std::unique_ptr<Node>
        {
            return std::make_unique<IdentityDcNode> (expectedSourceId, sourceDcForAsset (asset), 1);
        },
        projection,
        &projectError);

    REQUIRE (projected);
    REQUIRE (projectError.code == ProjectMixerProjectionError::Code::None);
    REQUIRE (projection.id == config.id);
    REQUIRE (projection.sampleRate == project.sampleRate.hz);
    REQUIRE (projection.maxBlockSize == kMaxBlock);
    REQUIRE (projection.tracks.size() == project.clips.size());

    const NodeId firstSource = projectMixerNodeIdForClip (project.clips[0].id, ProjectMixerNodeRole::Source);
    const NodeId firstFader = projectMixerNodeIdForClip (project.clips[0].id, ProjectMixerNodeRole::Fader);
    const NodeId firstPan = projectMixerNodeIdForClip (project.clips[0].id, ProjectMixerNodeRole::Pan);
    const NodeId firstMeter = projectMixerNodeIdForClip (project.clips[0].id, ProjectMixerNodeRole::Meter);
    REQUIRE (projection.tracks[0].source->properties().id == firstSource);
    REQUIRE (projection.tracks[0].faderNodeId == firstFader);
    REQUIRE (projection.tracks[0].panNodeId == firstPan);
    REQUIRE (projection.tracks[0].meterNodeId == firstMeter);
    REQUIRE (projection.tracks[0].linearGain == project.clips[0].gain);
    REQUIRE (projection.tracks[0].pan == 0.0f);

    MixerProjectionError graphError;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (projection), &graphError);
    REQUIRE (graph != nullptr);
    REQUIRE (graphError.code == MixerProjectionError::Code::None);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Fader) == project.clips.size());
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Pan) == project.clips.size());
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Meter) == project.clips.size());

    const float expected =
        (sourceDcForAsset (project.assets[0]) * project.clips[0].gain
         + sourceDcForAsset (project.assets[1]) * project.clips[1].gain)
        * kCenterGain;
    const StereoCapture out = render (*graph, kMaxBlock);
    REQUIRE (out.left.back() == Approx (expected).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (expected).margin (1.0e-4f));
}

TEST_CASE ("Project projector rejects invalid Project and invalid clip gain before source creation",
           "[mixer][projection][project][invalid]")
{
    ProjectMixerProjectionConfig config;
    config.masterSumNodeId = kMasterSumId;
    config.masterNodeId = kMasterId;

    {
        Project project = makeMixerProjectionProject();
        project.clips[0].assetId = entityIdFromLowByte (99);

        bool factoryCalled = false;
        MixerProjectionInputs projection;
        ProjectMixerProjectionError error;
        REQUIRE_FALSE (projectToMixerProjectionInputs (
            project,
            config,
            [&factoryCalled] (const Project&, const Clip&, const Asset&, NodeId)
                -> std::unique_ptr<Node>
            {
                factoryCalled = true;
                return nullptr;
            },
            projection,
            &error));

        REQUIRE (error.code == ProjectMixerProjectionError::Code::InvalidProject);
        REQUIRE_FALSE (factoryCalled);
    }

    {
        Project project = makeMixerProjectionProject();
        project.clips[0].gain = std::nextafter (FaderNode::kMaxLinearGain, std::numeric_limits<float>::max());

        bool factoryCalled = false;
        MixerProjectionInputs projection;
        ProjectMixerProjectionError error;
        REQUIRE_FALSE (projectToMixerProjectionInputs (
            project,
            config,
            [&factoryCalled] (const Project&, const Clip&, const Asset&, NodeId)
                -> std::unique_ptr<Node>
            {
                factoryCalled = true;
                return nullptr;
            },
            projection,
            &error));

        REQUIRE (error.code == ProjectMixerProjectionError::Code::InvalidClipGain);
        REQUIRE (error.clipIndex == 0u);
        REQUIRE_FALSE (factoryCalled);
    }
}

TEST_CASE ("Project projector rejects duplicate generated NodeIds and bad source factories",
           "[mixer][projection][project][invalid]")
{
    Project project = makeMixerProjectionProject();

    {
        ProjectMixerProjectionConfig config;
        config.masterSumNodeId = kMasterSumId;
        config.masterNodeId = projectMixerNodeIdForClip (project.clips[0].id, ProjectMixerNodeRole::Source);

        bool factoryCalled = false;
        MixerProjectionInputs projection;
        ProjectMixerProjectionError error;
        REQUIRE_FALSE (projectToMixerProjectionInputs (
            project,
            config,
            [&factoryCalled] (const Project&, const Clip&, const Asset&, NodeId)
                -> std::unique_ptr<Node>
            {
                factoryCalled = true;
                return nullptr;
            },
            projection,
            &error));

        REQUIRE (error.code == ProjectMixerProjectionError::Code::DuplicateNodeId);
        REQUIRE (error.nodeId == config.masterNodeId);
        REQUIRE_FALSE (factoryCalled);
    }

    {
        ProjectMixerProjectionConfig config;
        config.masterSumNodeId = kMasterSumId;
        config.masterNodeId = kMasterId;

        MixerProjectionInputs projection;
        ProjectMixerProjectionError error;
        REQUIRE_FALSE (projectToMixerProjectionInputs (
            project,
            config,
            [] (const Project&, const Clip&, const Asset&, NodeId)
                -> std::unique_ptr<Node>
            {
                return nullptr;
            },
            projection,
            &error));

        REQUIRE (error.code == ProjectMixerProjectionError::Code::SourceFactoryFailed);
        REQUIRE (error.clipIndex == 0u);
    }

    {
        ProjectMixerProjectionConfig config;
        config.masterSumNodeId = kMasterSumId;
        config.masterNodeId = kMasterId;

        MixerProjectionInputs projection;
        ProjectMixerProjectionError error;
        REQUIRE_FALSE (projectToMixerProjectionInputs (
            project,
            config,
            [] (const Project&, const Clip&, const Asset&, NodeId expectedSourceId)
                -> std::unique_ptr<Node>
            {
                return std::make_unique<IdentityDcNode> (expectedSourceId + 1u, 1.0f, 1);
            },
            projection,
            &error));

        REQUIRE (error.code == ProjectMixerProjectionError::Code::SourceNodeIdMismatch);
        REQUIRE (error.clipIndex == 0u);
    }
}

TEST_CASE ("Mixer projection wires pre and post fader Sends into bus Returns", "[mixer][projection][send]")
{
    MixerProjectionInputs inputs = baseProjection (7);
    inputs.buses.push_back (MixerBusProjection { 62000, 62001, 62002 });

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

    // The bus Return is centre-widened, so the summed Send (1.0) reaches the master equally in L and R at
    // the equal-power centre gain — NOT hard-left. Before the fix the mono Return put it in L only.
    const float expectedCenter = 1.0f * kCenterGain;
    const StereoCapture out = render (*graph, kMaxBlock);
    for (float v : out.left)
        REQUIRE (v == Approx (expectedCenter).margin (1.0e-4f));
    for (float v : out.right)
        REQUIRE (v == Approx (expectedCenter).margin (1.0e-4f));
}

TEST_CASE ("Mixer projection deduplicates identical Sends to the same bus tap", "[mixer][projection][send][dedup]")
{
    MixerProjectionInputs inputs = baseProjection (43);
    inputs.buses.push_back (MixerBusProjection { 62040, 62041, 62042 });

    MixerTrackProjection track = makeTrack (1430, 1.0f, 2430, 3430, 4430, 0.0f, 0.0f);
    track.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });
    track.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });
    inputs.tracks.push_back (std::move (track));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::None);

    const CompiledNode* const bus = compiledNodeById (*graph, 62040);
    REQUIRE (bus != nullptr);
    REQUIRE (bus->kind == CompiledNodeKind::Sum);
    REQUIRE (bus->numInputs == 1u);

    const StereoCapture out = render (*graph, kMaxBlock);
    REQUIRE (out.left.back() == Approx (kCenterGain).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (kCenterGain).margin (1.0e-4f));
}

TEST_CASE ("Mixer projection bus Return is centred and audible in both master channels not left only", "[mixer][projection][send][stereo]")
{
    // Direct regression guard for the latent left-only defect. A single mono Track (direct path silenced
    // by gain 0) sends pre-fader into one Bus, whose Return feeds the master, so the master hears ONLY the
    // bus Return. A mono Return summed straight into the stereo master would be audible in L only (R
    // silent); the centre-widened Return must place the signal equally in L and R.
    constexpr float kSourceDc = 0.5f;

    MixerProjectionInputs inputs = baseProjection (42);
    inputs.buses.push_back (MixerBusProjection { 62030, 62031, 62032 });

    MixerTrackProjection track = makeTrack (1420, kSourceDc, 2420, 3420, 4420, 0.0f, 0.0f);
    track.sends.push_back (MixerSendProjection { 0, MixerSendTap::PreFader });
    inputs.tracks.push_back (std::move (track));

    MixerProjectionError error;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code == MixerProjectionError::Code::None);

    const StereoCapture out = render (*graph, kMaxBlock);

    const float expected = kSourceDc * kCenterGain;   // pre-fader Send (0.5) widened to centre
    REQUIRE (out.left.back() == Approx (expected).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (expected).margin (1.0e-4f));
    // The anti-"left-only" assertions: the right channel is NOT silent, and it matches the left exactly.
    REQUIRE (out.right.back() > 1.0e-3f);
    REQUIRE (out.right.back() == Approx (out.left.back()).margin (1.0e-6f));
}

TEST_CASE ("Mixer projection bus Return summing is deterministic across declaration order", "[mixer][projection][send]")
{
    auto build = [] (bool reversed)
    {
        MixerProjectionInputs inputs = baseProjection (8);
        inputs.buses.push_back (MixerBusProjection { 62010, 62011, 62012 });

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

    const StereoCapture normalOut = render (*normal, kMaxBlock);
    const StereoCapture reversedOut = render (*reversed, kMaxBlock);

    REQUIRE (normalOut.left.size() == reversedOut.left.size());
    const float expectedCenter = 1.0f * kCenterGain;
    for (std::size_t i = 0; i < normalOut.left.size(); ++i)
    {
        // Centre-widened Return: present equally in L and R at the equal-power centre gain.
        REQUIRE (normalOut.left[i] == Approx (expectedCenter).margin (1.0e-4f));
        REQUIRE (normalOut.right[i] == Approx (normalOut.left[i]).margin (0.0f));
        // Determinism: canonical sum order makes the result independent of declaration order, bit-exact.
        REQUIRE (reversedOut.left[i] == Approx (normalOut.left[i]).margin (0.0f));
        REQUIRE (reversedOut.right[i] == Approx (normalOut.right[i]).margin (0.0f));
    }
}

TEST_CASE ("Mixer projection lets GraphBuilder align Return convergence with PDC", "[mixer][projection][send][pdc]")
{
    constexpr int kLatency = 5;
    constexpr int kFrames = 24;

    MixerProjectionInputs inputs = baseProjection (9);
    inputs.maxBlockSize = kFrames;
    inputs.buses.push_back (MixerBusProjection { 62020, 62021, 62022 });

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

    const StereoCapture out = render (*graph, kFrames);
    int nonZeroLeft = 0;
    int nonZeroRight = 0;
    for (int i = 0; i < kFrames; ++i)
    {
        // Two PDC-aligned impulses sum to 2.0 in the mono bus, then the centre-widened Return places that
        // equally in L and R at the equal-power centre gain.
        const float expected = i == kLatency ? 2.0f * kCenterGain : 0.0f;
        REQUIRE (out.left[static_cast<std::size_t> (i)] == Approx (expected).margin (1.0e-4f));
        REQUIRE (out.right[static_cast<std::size_t> (i)] == Approx (expected).margin (1.0e-4f));
        if (std::fabs (out.left[static_cast<std::size_t> (i)]) > 1.0e-5f)
            ++nonZeroLeft;
        if (std::fabs (out.right[static_cast<std::size_t> (i)]) > 1.0e-5f)
            ++nonZeroRight;
    }
    REQUIRE (nonZeroLeft == 1);
    REQUIRE (nonZeroRight == 1);
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

    StereoCapture out = render (*graph, kMaxBlock);
    REQUIRE (out.left.back() == Approx (kCenterGain).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (kCenterGain).margin (1.0e-4f));

    REQUIRE (graph->applySetGain (kFaderId, 0.25f));
    out = render (*graph, kMaxBlock);
    REQUIRE (out.left.back() == Approx (0.25f * kCenterGain).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (0.25f * kCenterGain).margin (1.0e-4f));

    // Pan hard left: the equal-power law puts the full signal in L and silences R.
    REQUIRE (graph->applySetPan (kPanId, -1.0f));
    out = render (*graph, kMaxBlock);
    REQUIRE (out.left.back() == Approx (0.25f).margin (1.0e-4f));
    REQUIRE (out.right.back() == Approx (0.0f).margin (1.0e-4f));
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

    const StereoCapture out = render (*graph, kMaxBlock);
    for (float v : out.left)
        REQUIRE (std::isfinite (v));
    for (float v : out.right)
        REQUIRE (std::isfinite (v));

    // The gain is clamped to the ceiling, so the settled value is source * ceiling * centre pan, present
    // equally in L and R (the source track is centre-panned).
    const float expected = 1.0e20f * FaderNode::kMaxLinearGain * kCenterGain;
    REQUIRE (out.left.back() == Approx (expected).margin (std::fabs (expected) * 1.0e-4f));
    REQUIRE (out.right.back() == Approx (expected).margin (std::fabs (expected) * 1.0e-4f));
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
