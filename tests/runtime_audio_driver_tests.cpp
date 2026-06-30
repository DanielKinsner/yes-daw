// YES DAW - H3 item 4b runtime audio driver checks.
//
// Proves a device-callback-shaped caller can drive Runtime::processBlock from a published mixer
// projection, with multichannel master output surfaced mechanically.

#include "engine/ProjectMixerProjection.h"
#include "engine/RuntimeAudioDriver.h"
#include "engine/nodes/IdentityDcNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

using Catch::Approx;
using yesdaw::engine::Asset;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::EntityId;
using yesdaw::engine::GraphId;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::MixerProjectionError;
using yesdaw::engine::MixerProjectionInputs;
using yesdaw::engine::MixerTrackProjection;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectMixerProjectionConfig;
using yesdaw::engine::ProjectMixerProjectionError;
using yesdaw::engine::RuntimeAudioDriver;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::buildMixerGraphProjection;
using yesdaw::engine::projectToMixerProjectionInputs;

namespace {

constexpr int    kFrames = 64;
constexpr NodeId kMasterSumId = 65000;
constexpr NodeId kMasterId = 65001;
constexpr float  kCenterGain = 0.70710677f;

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

bool allSamplesNear (const std::vector<float>& values, float expected) noexcept
{
    for (const float value : values)
        if (std::fabs (value - expected) > 0.00001f)
            return false;

    return true;
}

Project makeSingleClipProject()
{
    Asset asset;
    asset.id = idFromLowByte (2);
    asset.contentHash.bytes.back() = 2;
    asset.frames = 2048;
    asset.sampleRate = SampleRate { 48000.0 };
    asset.channels = 1;

    Clip clip;
    clip.id = idFromLowByte (3);
    clip.assetId = asset.id;
    clip.trackId = idFromLowByte (4);
    clip.timelineStart = 0;
    clip.timelineLength = 512;
    clip.srcOffset = 0;
    clip.srcLen = 512;
    clip.gain = 0.5f;
    clip.fadeIn = 0;
    clip.fadeOut = 0;
    clip.timeBase = TimeBase::SampleLocked;

    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };
    project.assets.push_back (asset);
    Track track;
    track.id = clip.trackId;
    track.strip.name = "Audio 1";
    project.tracks.push_back (track);
    project.clips.push_back (clip);
    return project;
}

std::unique_ptr<CompiledGraph> buildProjectMixerGraph (GraphId graphId)
{
    const Project project = makeSingleClipProject();
    REQUIRE (project.hasValidAssetClipIndirection());

    ProjectMixerProjectionConfig config;
    config.id = graphId;
    config.masterSumNodeId = kMasterSumId;
    config.masterNodeId = kMasterId;
    config.maxBlockSize = kFrames;

    MixerProjectionInputs projection;
    ProjectMixerProjectionError projectError;
    REQUIRE (projectToMixerProjectionInputs (
        project,
        config,
        [] (const Project&, const Clip&, const Asset&, NodeId expectedSourceId)
            -> std::unique_ptr<Node>
        {
            return std::make_unique<IdentityDcNode> (expectedSourceId, 1.0f, 1);
        },
        projection,
        &projectError));
    REQUIRE (projectError.code == ProjectMixerProjectionError::Code::None);

    MixerProjectionError graphError;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (projection), &graphError);
    REQUIRE (graph != nullptr);
    REQUIRE (graphError.code == MixerProjectionError::Code::None);
    return graph;
}

std::unique_ptr<CompiledGraph> buildPannedMixerGraph (GraphId graphId, float pan)
{
    MixerProjectionInputs projection;
    projection.id = graphId;
    projection.masterSumNodeId = kMasterSumId;
    projection.masterNodeId = kMasterId;
    projection.sampleRate = 48000.0;
    projection.maxBlockSize = kFrames;

    MixerTrackProjection track;
    track.source = std::make_unique<IdentityDcNode> (101, 1.0f, 1);
    track.faderNodeId = 102;
    track.panNodeId = 103;
    track.meterNodeId = 104;
    track.linearGain = 1.0f;
    track.pan = pan;
    projection.tracks.push_back (std::move (track));

    MixerProjectionError graphError;
    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (projection), &graphError);
    REQUIRE (graph != nullptr);
    REQUIRE (graphError.code == MixerProjectionError::Code::None);
    return graph;
}

} // namespace

TEST_CASE ("RuntimeAudioDriver clears device outputs before a graph is published", "[runtime][driver]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();

    {
        RuntimeAudioDriver driver;
        std::vector<float> left (kFrames, -1.0f);
        std::vector<float> right (kFrames, 2.0f);
        float* outputs[2] = { left.data(), right.data() };

        driver.processDeviceBlock (outputs, 2, kFrames);

        REQUIRE (allSamplesNear (left, 0.0f));
        REQUIRE (allSamplesNear (right, 0.0f));
        REQUIRE (driver.processedGen() == 1u);
        REQUIRE (driver.reclaim() == 0u);
    }

    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("RuntimeAudioDriver publishes Project mixer projection to the device callback", "[runtime][driver][mixer][project]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();

    {
        RuntimeAudioDriver driver;
        REQUIRE (driver.publish (buildProjectMixerGraph (10)));

        std::vector<float> left (kFrames, -1.0f);
        std::vector<float> right (kFrames, -1.0f);
        float* outputs[2] = { left.data(), right.data() };

        driver.processDeviceBlock (outputs, 2, kFrames);

        const float expected = 0.5f * kCenterGain;
        REQUIRE (left.back() == Approx (expected).margin (0.00001f));
        REQUIRE (right.back() == Approx (expected).margin (0.00001f));
        REQUIRE (driver.reclaim() == 0u);
    }

    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("RuntimeAudioDriver preserves distinct stereo master channels", "[runtime][driver][mixer][stereo]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();

    {
        RuntimeAudioDriver driver;
        REQUIRE (driver.publish (buildPannedMixerGraph (20, -1.0f)));

        std::vector<float> left (kFrames, 0.0f);
        std::vector<float> right (kFrames, 0.0f);
        float* outputs[2] = { left.data(), right.data() };

        driver.processDeviceBlock (outputs, 2, kFrames);
        REQUIRE (left.back() == Approx (1.0f).margin (0.00001f));
        REQUIRE (right.back() == Approx (0.0f).margin (0.00001f));

        REQUIRE (driver.publish (buildPannedMixerGraph (21, 1.0f)));
        driver.processDeviceBlock (outputs, 2, kFrames);

        REQUIRE (left.back() == Approx (0.0f).margin (0.00001f));
        REQUIRE (right.back() == Approx (1.0f).margin (0.00001f));
        REQUIRE (driver.reclaim() == 1u);
    }

    REQUIRE (CompiledGraph::aliveCount() == base);
}
