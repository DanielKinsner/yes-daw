// YES DAW - H8 playback gate: play a Project through the realtime Runtime and prove it equals an
// INDEPENDENT reference (Clips summed at their timeline positions) -- not the engine compared to itself.

#include "engine/OfflineRenderer.h"
#include "engine/PlaybackEngine.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::PlaybackEngine;
using yesdaw::engine::Project;
using yesdaw::engine::renderOfflineProject;
using yesdaw::engine::SampleRate;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;

namespace {

constexpr float  kCenterGain = 0.70710677f;
constexpr double kTolerance  = 1.0e-6;

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

AssetContentHash hashWithSeed (std::uint8_t seed) noexcept
{
    AssetContentHash hash;
    for (std::size_t i = 0; i < hash.bytes.size(); ++i)
        hash.bytes[i] = static_cast<std::uint8_t> (seed + static_cast<std::uint8_t> (i * 13u));

    return hash;
}

struct PlaybackFixture
{
    Project                         project;
    std::vector<std::vector<float>> samples;
    std::vector<DecodedAssetAudio>  decodedAssets;
};

PlaybackFixture makePlaybackFixture()
{
    PlaybackFixture fixture;
    fixture.samples = {
        { -0.30f, 0.10f, 0.20f, -0.40f, 0.50f, -0.60f, 0.70f, -0.80f },
        { 0.90f, -0.75f, 0.60f, -0.45f, 0.30f, -0.15f },
    };

    Asset first;
    first.id = idFromLowByte (10);
    first.contentHash = hashWithSeed (10);
    first.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    first.sampleRate = SampleRate { 48000.0 };
    first.channels = 1;

    Asset second = first;
    second.id = idFromLowByte (11);
    second.contentHash = hashWithSeed (11);
    second.frames = static_cast<std::uint64_t> (fixture.samples[1].size());

    Clip left;
    left.id = idFromLowByte (20);
    left.assetId = first.id;
    left.timelineStart = 2;
    left.timelineLength = 6;
    left.srcOffset = 1;
    left.srcLen = 6;
    left.gain = 0.50f;
    left.fadeIn = 2;
    left.fadeOut = 2;
    left.timeBase = TimeBase::SampleLocked;

    Clip overlap;
    overlap.id = idFromLowByte (21);
    overlap.assetId = second.id;
    overlap.timelineStart = 5;
    overlap.timelineLength = 4;
    overlap.srcOffset = 0;
    overlap.srcLen = 4;
    overlap.gain = 0.25f;
    overlap.fadeIn = 0;
    overlap.fadeOut = 0;
    overlap.timeBase = TimeBase::SampleLocked;

    fixture.project.id = idFromLowByte (1);
    fixture.project.sampleRate = SampleRate { 48000.0 };
    fixture.project.assets = { first, second };
    fixture.project.clips = { left, overlap };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());

    fixture.decodedAssets = {
        DecodedAssetAudio { first.id, first.sampleRate, first.frames, first.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
        DecodedAssetAudio { second.id, second.sampleRate, second.frames, second.channels,
                            std::span<const float> (fixture.samples[1].data(), fixture.samples[1].size()) },
    };
    return fixture;
}

const std::vector<float>& samplesForAsset (const PlaybackFixture& fixture, EntityId assetId)
{
    for (std::size_t i = 0; i < fixture.project.assets.size(); ++i)
        if (fixture.project.assets[i].id == assetId)
            return fixture.samples[i];

    FAIL ("missing fixture samples for asset");
    static const std::vector<float> empty;
    return empty;
}

// The canonical LINEAR fade DecodedClipNode applies (fade-in * fade-out, anchored to the source-frame
// count) -- the textbook ramp, derived independently of the engine.
float linearFade (const Clip& clip, Tick local, std::uint64_t total) noexcept
{
    float gain = 1.0f;
    if (clip.fadeIn > 0 && local < clip.fadeIn)
        gain *= static_cast<float> (local) / static_cast<float> (clip.fadeIn);
    if (clip.fadeOut > 0)
    {
        const Tick fadeStart = static_cast<Tick> (total) - clip.fadeOut;
        if (local >= fadeStart)
            gain *= static_cast<float> (static_cast<Tick> (total) - local) / static_cast<float> (clip.fadeOut);
    }
    return gain;
}

std::vector<float> independentReference (const PlaybackFixture& fixture)
{
    std::uint64_t frames = 0;
    for (const Clip& clip : fixture.project.clips)
        frames = std::max<std::uint64_t> (frames, static_cast<std::uint64_t> (clip.timelineStart + clip.timelineLength));

    std::vector<float> expected (static_cast<std::size_t> (frames) * 2u, 0.0f);
    for (const Clip& clip : fixture.project.clips)
    {
        const std::vector<float>& samples = samplesForAsset (fixture, clip.assetId);
        const std::uint64_t sourceFrames = std::min<std::uint64_t> (clip.srcLen, static_cast<std::uint64_t> (clip.timelineLength));
        for (std::uint64_t local = 0; local < sourceFrames; ++local)
        {
            const float source = samples[static_cast<std::size_t> (clip.srcOffset + local)];
            const float value = source * linearFade (clip, static_cast<Tick> (local), sourceFrames) * clip.gain * kCenterGain;
            const std::size_t frame = static_cast<std::size_t> (clip.timelineStart + static_cast<Tick> (local));
            expected[frame * 2u] += value;
            expected[frame * 2u + 1u] += value;
        }
    }
    return expected;
}

bool buffersNear (std::span<const float> a, std::span<const float> b, double tol = kTolerance) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::fabs (static_cast<double> (a[i] - b[i])) > tol)
            return false;
    return true;
}

bool bitIdentical (std::span<const float> a, std::span<const float> b) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        std::uint32_t ba = 0, bb = 0;
        std::memcpy (&ba, &a[i], sizeof (ba));
        std::memcpy (&bb, &b[i], sizeof (bb));
        if (ba != bb)
            return false;
    }
    return true;
}

// Pump a fresh PlaybackEngine block by block through the realtime device-callback seam, collecting the
// interleaved Master output for the whole timeline.
std::vector<float> playToBuffer (const PlaybackFixture& fixture, int blockSize)
{
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        OfflineRenderOptions {});
    REQUIRE (created.ok());
    PlaybackEngine& engine = *created.engine;

    const std::uint64_t total = engine.frames();
    const int           ch    = static_cast<int> (engine.channels());
    REQUIRE (ch == 2);

    std::vector<float>  out (static_cast<std::size_t> (total) * static_cast<std::size_t> (ch), 0.0f);
    std::vector<float>  storage (static_cast<std::size_t> (ch) * static_cast<std::size_t> (blockSize), 0.0f);
    std::vector<float*> ptrs (static_cast<std::size_t> (ch), nullptr);

    std::uint64_t offset = 0;
    while (offset < total)
    {
        const int n = static_cast<int> (std::min<std::uint64_t> (total - offset, static_cast<std::uint64_t> (blockSize)));
        for (int c = 0; c < ch; ++c)
            ptrs[static_cast<std::size_t> (c)] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (blockSize);

        engine.processBlock (ptrs.data(), ch, n);

        for (int frame = 0; frame < n; ++frame)
            for (int c = 0; c < ch; ++c)
                out[(static_cast<std::size_t> (offset) + static_cast<std::size_t> (frame)) * static_cast<std::size_t> (ch)
                    + static_cast<std::size_t> (c)] = ptrs[static_cast<std::size_t> (c)][frame];

        offset += static_cast<std::uint64_t> (n);
    }

    engine.reclaim();
    return out;
}

} // namespace

TEST_CASE ("PlaybackEngine plays a Project through the realtime Runtime, matching an independent reference",
           "[h8][playback][runtime]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();
    {
        const PlaybackFixture fixture = makePlaybackFixture();
        const std::vector<float> played = playToBuffer (fixture, 4);   // multi-block

        REQUIRE (played.size() == static_cast<std::size_t> (9u * 2u));
        REQUIRE (buffersNear (played, independentReference (fixture)));
    }
    REQUIRE (CompiledGraph::aliveCount() == base);   // the Runtime path frees the graph on teardown
}

TEST_CASE ("PlaybackEngine output is identical at every device block size (ADR-0008)",
           "[h8][playback][block-size]")
{
    const PlaybackFixture fixture = makePlaybackFixture();
    const std::vector<float> reference = playToBuffer (fixture, 128);

    for (const int blockSize : { 1, 2, 3, 4, 5, 7, 8, 9, 64 })
    {
        const std::vector<float> played = playToBuffer (fixture, blockSize);
        REQUIRE (bitIdentical (played, reference));
    }
}

TEST_CASE ("PlaybackEngine output matches the offline render of the same Project",
           "[h8][playback][offline-parity]")
{
    const PlaybackFixture fixture = makePlaybackFixture();
    const std::vector<float> played = playToBuffer (fixture, 5);

    const auto offline = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()));
    REQUIRE (offline.ok());

    // The realtime publish/drain/install path must reproduce the offline render bit-for-bit.
    REQUIRE (bitIdentical (played, offline.interleavedSamples));
}
