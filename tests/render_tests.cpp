// YES DAW - RT-vs-offline Render equivalence gate.
//
// H1 has no asset decoder -> SourceNode projection yet, so this gate stays inside the current Project
// value surface: a valid Project's Clips compile to deterministic built-in source/fader nodes, then the
// same Project projection is rendered through Runtime and through a free-wheeling offline driver.

#include "engine/GraphBuilder.h"
#include "engine/Project.h"
#include "engine/Runtime.h"
#include "engine/nodes/DecodedClipNode.h"
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
using yesdaw::engine::DecodedClipNode;
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

// The engine must place each Clip at its timeline frame and SUM overlaps — checked against an independent
// hand-computed reference, NOT against another engine render. Ignoring timelineStart, mis-positioning, or
// dropping a clip on overlap all make the engine output differ from this reference (a real negative control
// for "multi-track timeline playback", which the OscillatorNode RT-vs-offline gate above does not exercise).
TEST_CASE ("Engine renders Clips at their timeline positions and sums overlaps", "[h1][render][timeline]")
{
    constexpr int    kBlock = 16;
    constexpr NodeId kClipA = 64001, kClipB = 64002, kTimelineMaster = 64000;

    const std::vector<float> a { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f };       // at frame 0
    const std::vector<float> b { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f }; // at frame 4 (overlaps A on [4,6))
    constexpr std::int64_t aStart = 0, bStart = 4;

    auto clipA  = std::make_unique<DecodedClipNode> (kClipA, a, 1, aStart);
    auto clipB  = std::make_unique<DecodedClipNode> (kClipB, b, 1, bStart);
    auto master = std::make_unique<MasterNode> (kTimelineMaster, 1);
    master->setInputNodes ({ clipA.get(), clipB.get() });

    GraphBuilder::Inputs inputs;
    inputs.id           = 70;
    inputs.masterNodeId = kTimelineMaster;
    inputs.sampleRate   = 48000.0;
    inputs.maxBlockSize = kBlock;
    inputs.nodes.push_back (std::move (clipA));
    inputs.nodes.push_back (std::move (clipB));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    std::vector<float> out (kBlock, -123.0f);
    graph->process (out.data(), kBlock);

    for (int f = 0; f < kBlock; ++f)
    {
        float expected = 0.0f;
        const std::int64_t la = static_cast<std::int64_t> (f) - aStart;
        if (la >= 0 && la < static_cast<std::int64_t> (a.size()))
            expected += a[static_cast<std::size_t> (la)];
        const std::int64_t lb = static_cast<std::int64_t> (f) - bStart;
        if (lb >= 0 && lb < static_cast<std::int64_t> (b.size()))
            expected += b[static_cast<std::size_t> (lb)];

        INFO ("frame " << f << " expected " << expected << " got " << out[static_cast<std::size_t> (f)]);
        REQUIRE (std::fabs (static_cast<double> (out[static_cast<std::size_t> (f)] - expected)) <= kTolerance);
    }

    // Discriminating frames, spelled out: A-only, A+B overlap, B-only after A ends, then silence.
    REQUIRE (out[0] == 1.0f);            // A[0]
    REQUIRE (out[4] == 15.0f);           // A[4] + B[0] = 5 + 10
    REQUIRE (out[6] == 30.0f);           // A exhausted, B[2]
    REQUIRE (out[10] == 0.0f);           // both exhausted
}

// A real crossfade: clip A fades OUT while overlapping clip B fades IN, both applied BY THE ENGINE from
// clip metadata (not pre-baked into the samples). Checked against an independent linear-fade reference;
// if the engine ignored the fades the overlap would be the raw 4+8=12 sum, not the faded ramp.
TEST_CASE ("Engine renders an overlapping crossfade from clip fade metadata", "[h2][render][crossfade]")
{
    constexpr int          kBlock = 16;
    constexpr NodeId       kClipA = 64011, kClipB = 64012, kXfadeMaster = 64010;
    constexpr std::int64_t aStart = 0, bStart = 4, fade = 4;

    const std::vector<float> a (8, 4.0f);   // frames [0,8), fade OUT over the last 4
    const std::vector<float> b (8, 8.0f);   // frames [4,12), fade IN over the first 4 -> overlap on [4,8)

    auto clipA  = std::make_unique<DecodedClipNode> (kClipA, a, 1, aStart, /*fadeIn*/ 0,    /*fadeOut*/ fade);
    auto clipB  = std::make_unique<DecodedClipNode> (kClipB, b, 1, bStart, /*fadeIn*/ fade, /*fadeOut*/ 0);
    auto master = std::make_unique<MasterNode> (kXfadeMaster, 1);
    master->setInputNodes ({ clipA.get(), clipB.get() });

    GraphBuilder::Inputs inputs;
    inputs.id           = 71;
    inputs.masterNodeId = kXfadeMaster;
    inputs.sampleRate   = 48000.0;
    inputs.maxBlockSize = kBlock;
    inputs.nodes.push_back (std::move (clipA));
    inputs.nodes.push_back (std::move (clipB));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    std::vector<float> out (kBlock, -123.0f);
    graph->process (out.data(), kBlock);

    // Independent linear-fade reference (same spec as DecodedClipNode::fadeGainAt, separate code).
    const auto faded = [] (float value, std::int64_t local, std::int64_t total,
                           std::int64_t fadeIn, std::int64_t fadeOut) -> float
    {
        float g = 1.0f;
        if (fadeIn > 0 && local < fadeIn)
            g *= static_cast<float> (local) / static_cast<float> (fadeIn);
        if (fadeOut > 0)
        {
            const std::int64_t s = total - fadeOut;
            if (local >= s)
                g *= static_cast<float> (total - local) / static_cast<float> (fadeOut);
        }
        return value * g;
    };

    for (int f = 0; f < kBlock; ++f)
    {
        float expected = 0.0f;
        const std::int64_t la = static_cast<std::int64_t> (f) - aStart;
        if (la >= 0 && la < 8) expected += faded (4.0f, la, 8, 0, fade);
        const std::int64_t lb = static_cast<std::int64_t> (f) - bStart;
        if (lb >= 0 && lb < 8) expected += faded (8.0f, lb, 8, fade, 0);

        INFO ("crossfade frame " << f << " expected " << expected << " got " << out[static_cast<std::size_t> (f)]);
        REQUIRE (std::fabs (static_cast<double> (out[static_cast<std::size_t> (f)] - expected)) <= kTolerance);
    }

    // The fades are really applied: the overlap is NOT the un-faded 4 + 8 = 12.
    REQUIRE (static_cast<double> (out[4]) < 12.0 - kTolerance);
    REQUIRE (static_cast<double> (out[7]) < 12.0 - kTolerance);
}
