// YES DAW - RT-vs-offline Render equivalence gate.
//
// H1 has no asset decoder -> SourceNode projection yet, so this gate stays inside the current Project
// value surface: a valid Project's Clips compile to deterministic built-in source/fader nodes, then the
// same Project projection is rendered through Runtime and through a free-wheeling offline driver.

#include "engine/GraphBuilder.h"
#include "engine/Project.h"
#include "engine/Runtime.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/OscillatorNode.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::EntityId;
using yesdaw::engine::FaderNode;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::GraphId;
using yesdaw::engine::MasterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::OscillatorNode;
using yesdaw::engine::Project;
using yesdaw::engine::Runtime;
using yesdaw::engine::SampleRate;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;

namespace {

constexpr NodeId kRenderMasterId = 63000;
constexpr int    kMaxBlockSize   = 128;
constexpr int    kRenderFrames   = 997;
constexpr double kTolerance      = 1.0e-6;

EntityId id (std::uint64_t high, std::uint64_t low) noexcept
{
    return EntityId::fromBigEndianParts (high, low);
}

AssetContentHash hashWithSeed (std::uint8_t seed) noexcept
{
    AssetContentHash hash;
    for (std::size_t i = 0; i < hash.bytes.size(); ++i)
        hash.bytes[i] = static_cast<std::uint8_t> (seed + static_cast<std::uint8_t> (i * 17u));

    return hash;
}

Project makeMinimalRenderProject()
{
    Asset first;
    first.id          = id (0x0100, 0x0001);
    first.contentHash = hashWithSeed (17);
    first.frames      = 4096;
    first.sampleRate  = SampleRate { 48000.0 };
    first.channels    = 1;

    Asset second;
    second.id          = id (0x0100, 0x0002);
    second.contentHash = hashWithSeed (43);
    second.frames      = 4096;
    second.sampleRate  = SampleRate { 48000.0 };
    second.channels    = 1;

    Clip left;
    left.id             = id (0x0200, 0x0001);
    left.assetId        = first.id;
    left.timelineStart  = 0;
    left.timelineLength = static_cast<Tick> (yesdaw::engine::kTicksPerQuarter);
    left.srcOffset      = 0;
    left.srcLen         = 1024;
    left.gain           = 0.5f;
    left.fadeIn         = 0;
    left.fadeOut        = 0;
    left.timeBase       = TimeBase::SampleLocked;

    Clip right;
    right.id             = id (0x0200, 0x0002);
    right.assetId        = second.id;
    right.timelineStart  = 0;
    right.timelineLength = static_cast<Tick> (yesdaw::engine::kTicksPerQuarter);
    right.srcOffset      = 128;
    right.srcLen         = 2048;
    right.gain           = 0.25f;
    right.fadeIn         = 0;
    right.fadeOut        = 0;
    right.timeBase       = TimeBase::SampleLocked;

    Project project;
    project.id         = id (0x0F00, 0x0001);
    project.sampleRate = SampleRate { 48000.0 };
    project.assets.push_back (first);
    project.assets.push_back (second);
    project.clips.push_back (left);
    project.clips.push_back (right);
    return project;
}

std::unique_ptr<CompiledGraph> buildProjectProjection (const Project& project, GraphId graphId)
{
    if (! project.hasValidAssetClipIndirection())
        return nullptr;

    GraphBuilder::Inputs inputs;
    inputs.id           = graphId;
    inputs.masterNodeId = kRenderMasterId;
    inputs.sampleRate   = project.sampleRate.hz;
    inputs.maxBlockSize = kMaxBlockSize;

    std::vector<Node*> masterInputs;
    masterInputs.reserve (project.clips.size());
    inputs.nodes.reserve (project.clips.size() * 2u + 1u);

    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        const Clip& clip = project.clips[i];
        const Asset* const asset = project.findAsset (clip.assetId);
        if (asset == nullptr || ! std::isfinite (clip.gain))
            return nullptr;

        auto source = std::make_unique<OscillatorNode> (static_cast<NodeId> (1000u + i));
        auto fader = std::make_unique<FaderNode> (static_cast<NodeId> (2000u + i), 1);

        OscillatorNode* const sourcePtr = source.get();
        FaderNode* const      faderPtr  = fader.get();
        faderPtr->setInput (sourcePtr);
        faderPtr->setTargetGain (clip.gain);

        inputs.nodes.push_back (std::move (source));
        inputs.nodes.push_back (std::move (fader));
        masterInputs.push_back (faderPtr);
    }

    auto master = std::make_unique<MasterNode> (kRenderMasterId, 1);
    master->setInputNodes (std::move (masterInputs));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    if (error.code() != GraphBuildError::Code::None)
        return nullptr;

    return graph;
}

template <typename Process>
std::vector<float> renderByBlocks (Process&& process, const std::vector<int>& blockSizes)
{
    std::vector<float> rendered (static_cast<std::size_t> (kRenderFrames), 0.0f);
    std::vector<float> block;
    block.reserve (kMaxBlockSize);

    int offset = 0;
    std::size_t blockIndex = 0;
    while (offset < kRenderFrames)
    {
        const int requested = blockSizes[blockIndex % blockSizes.size()];
        REQUIRE (requested > 0);
        REQUIRE (requested <= kMaxBlockSize);

        const int frames = std::min (requested, kRenderFrames - offset);
        block.assign (static_cast<std::size_t> (frames), -999.0f);
        process (block.data(), frames);

        std::copy (block.begin(), block.end(), rendered.begin() + offset);
        offset += frames;
        ++blockIndex;
    }

    return rendered;
}

std::vector<float> renderRealtimePath (const Project& project)
{
    Runtime runtime;

    std::unique_ptr<CompiledGraph> graph = buildProjectProjection (project, 101);
    REQUIRE (graph != nullptr);
    REQUIRE (runtime.publish (std::move (graph)));

    std::vector<float> rendered = renderByBlocks (
        [&runtime] (float* out, int frames)
        {
            runtime.processBlock (out, frames);
        },
        { 128, 37, 64, 11 });

    runtime.reclaim();
    return rendered;
}

std::vector<float> renderOfflinePath (const Project& project)
{
    std::unique_ptr<CompiledGraph> graph = buildProjectProjection (project, 202);
    REQUIRE (graph != nullptr);

    return renderByBlocks (
        [&graph] (float* out, int frames)
        {
            graph->process (out, frames);
        },
        { 17, 113, 29, 71 });
}

} // namespace

TEST_CASE ("RT path and offline Render path match for the same Project", "[render][runtime][project]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();

    const Project project = makeMinimalRenderProject();
    REQUIRE (project.hasValidAssetClipIndirection());

    const std::vector<float> rt      = renderRealtimePath (project);
    const std::vector<float> offline = renderOfflinePath (project);
    REQUIRE (rt.size() == offline.size());

    double maxAbsDiff = 0.0;
    double peak       = 0.0;
    for (std::size_t i = 0; i < rt.size(); ++i)
    {
        maxAbsDiff = std::max (maxAbsDiff, std::fabs (static_cast<double> (rt[i] - offline[i])));
        peak       = std::max (peak, std::fabs (static_cast<double> (offline[i])));
    }

    INFO ("max abs diff between RT and offline Render = " << maxAbsDiff);
    REQUIRE (peak > 0.01);
    REQUIRE (maxAbsDiff <= kTolerance);

    REQUIRE (CompiledGraph::aliveCount() == base);
}
