// YES DAW - H14 CP9 full built-in FX suite integration gate.
//
// Proves persisted insert chains wire through the shared Project graph path used by offline render and
// realtime playback. The committed fixture is the first FX-schema bundle and is treated as frozen.

#include "engine/OfflineRenderer.h"
#include "engine/PlaybackEngine.h"
#include "engine/nodes/CompressorNode.h"
#include "engine/nodes/EqNode.h"
#include "engine/nodes/FxDelayNode.h"
#include "engine/nodes/LimiterNode.h"
#include "engine/nodes/ReverbNode.h"
#include "persistence/ProjectBundle.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::FxInsert;
using yesdaw::engine::FxKind;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::OfflineRenderResult;
using yesdaw::engine::PlaybackEngine;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::unmapToNormalized;
using yesdaw::persistence::AssetImportRequest;
using yesdaw::persistence::ProjectBundleDb;

namespace {

constexpr int kSampleRate = 48000;
constexpr int kAssetFrames = 2048;
constexpr int kMaxBlockSize = 512;

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

std::filesystem::path tempPath (std::string_view label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
                               / ("yesdaw-h14-cp9-" + std::string (label) + "-" + std::to_string (ticks));

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

void writeBytes (const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    if (path.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories (path.parent_path(), ec);
        REQUIRE (! ec);
    }

    std::ofstream output (path, std::ios::binary | std::ios::trunc);
    REQUIRE (output.good());
    output.write (reinterpret_cast<const char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    output.close();
    REQUIRE (output.good());
}

std::uint32_t floatBits (float value) noexcept
{
    std::uint32_t bits = 0;
    std::memcpy (&bits, &value, sizeof (bits));
    return bits;
}

bool bitIdentical (std::span<const float> a, std::span<const float> b) noexcept
{
    if (a.size() != b.size())
        return false;

    for (std::size_t i = 0; i < a.size(); ++i)
        if (floatBits (a[i]) != floatBits (b[i]))
            return false;

    return true;
}

bool changed (std::span<const float> a, std::span<const float> b) noexcept
{
    if (a.size() != b.size())
        return true;

    for (std::size_t i = 0; i < a.size(); ++i)
        if (floatBits (a[i]) != floatBits (b[i]))
            return true;

    return false;
}

template <typename Node>
std::pair<std::uint32_t, double> normalizedParam (std::uint32_t paramId, double realValue)
{
    return { paramId, unmapToNormalized (Node::parameterSpec (paramId), realValue) };
}

FxInsert makeInsert (std::uint8_t id, FxKind kind, std::vector<std::pair<std::uint32_t, double>> params)
{
    FxInsert insert;
    insert.id = idFromLowByte (id);
    insert.kind = kind;
    insert.enabled = true;
    insert.normalizedParams = std::move (params);
    return insert;
}

std::vector<float> makeSamples()
{
    std::vector<float> samples (kAssetFrames, 0.0f);
    for (int i = 0; i < kAssetFrames; ++i)
    {
        const double t = static_cast<double> (i) / static_cast<double> (kSampleRate);
        const double a = 0.18 * std::sin (2.0 * 3.14159265358979323846 * 173.0 * t);
        const double b = 0.11 * std::sin (2.0 * 3.14159265358979323846 * 911.0 * t);
        const double pulse = (i % 197 == 0) ? 0.20 : 0.0;
        samples[static_cast<std::size_t> (i)] = static_cast<float> (a + b + pulse);
    }

    return samples;
}

Asset makeAsset (EntityId id)
{
    Asset asset;
    asset.id = id;
    asset.frames = kAssetFrames;
    asset.sampleRate = SampleRate { static_cast<double> (kSampleRate) };
    asset.channels = 1;
    return asset;
}

Project makeFxSuiteProject (Asset asset)
{
    Clip clip;
    clip.id = idFromLowByte (20);
    clip.assetId = asset.id;
    clip.trackId = idFromLowByte (30);
    clip.timelineStart = 0;
    clip.timelineLength = kAssetFrames;
    clip.srcOffset = 0;
    clip.srcLen = kAssetFrames;
    clip.gain = 1.0f;
    clip.fadeIn = 0;
    clip.fadeOut = 0;
    clip.timeBase = TimeBase::SampleLocked;

    Track track;
    track.id = clip.trackId;
    track.strip.name = "CP9 FX Track";
    track.strip.fxChain = {
        makeInsert (40,
                    FxKind::Eq,
                    {
                        normalizedParam<yesdaw::engine::EqNode> (
                            yesdaw::engine::EqNode::parameterIdFor (0, yesdaw::engine::EqNode::kTypeParamOffset),
                            0.0),
                        normalizedParam<yesdaw::engine::EqNode> (
                            yesdaw::engine::EqNode::parameterIdFor (0, yesdaw::engine::EqNode::kFrequencyParamOffset),
                            720.0),
                        normalizedParam<yesdaw::engine::EqNode> (
                            yesdaw::engine::EqNode::parameterIdFor (0, yesdaw::engine::EqNode::kGainParamOffset),
                            3.0),
                        normalizedParam<yesdaw::engine::EqNode> (
                            yesdaw::engine::EqNode::parameterIdFor (0, yesdaw::engine::EqNode::kQParamOffset),
                            0.8),
                    }),
        makeInsert (41,
                    FxKind::Compressor,
                    {
                        normalizedParam<yesdaw::engine::CompressorNode> (0, -24.0),
                        normalizedParam<yesdaw::engine::CompressorNode> (1, 2.5),
                        normalizedParam<yesdaw::engine::CompressorNode> (2, 2.0),
                        normalizedParam<yesdaw::engine::CompressorNode> (3, 80.0),
                        normalizedParam<yesdaw::engine::CompressorNode> (4, 6.0),
                        normalizedParam<yesdaw::engine::CompressorNode> (5, 1.5),
                    }),
        makeInsert (42,
                    FxKind::Delay,
                    {
                        normalizedParam<yesdaw::engine::FxDelayNode> (0, 3.0),
                        normalizedParam<yesdaw::engine::FxDelayNode> (1, 5.0),
                        normalizedParam<yesdaw::engine::FxDelayNode> (2, 0.18),
                        normalizedParam<yesdaw::engine::FxDelayNode> (3, 8000.0),
                        normalizedParam<yesdaw::engine::FxDelayNode> (4, 1.0),
                        normalizedParam<yesdaw::engine::FxDelayNode> (5, 0.22),
                    }),
        makeInsert (43,
                    FxKind::Reverb,
                    {
                        normalizedParam<yesdaw::engine::ReverbNode> (0, 1.0),
                        normalizedParam<yesdaw::engine::ReverbNode> (1, 0.1),
                        normalizedParam<yesdaw::engine::ReverbNode> (2, 0.65),
                        normalizedParam<yesdaw::engine::ReverbNode> (3, 10000.0),
                        normalizedParam<yesdaw::engine::ReverbNode> (4, 0.12),
                    }),
        makeInsert (44,
                    FxKind::Limiter,
                    {
                        normalizedParam<yesdaw::engine::LimiterNode> (0, -0.5),
                        normalizedParam<yesdaw::engine::LimiterNode> (1, 60.0),
                        normalizedParam<yesdaw::engine::LimiterNode> (2, 1.0),
                    }),
    };

    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { static_cast<double> (kSampleRate) };
    project.assets.push_back (asset);
    project.tracks.push_back (std::move (track));
    project.clips.push_back (clip);
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

std::vector<DecodedAssetAudio> decodedFor (const Project& project, std::span<const float> samples)
{
    REQUIRE (project.assets.size() == 1u);
    const Asset& asset = project.assets.front();
    return {
        DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels, samples },
    };
}

OfflineRenderResult renderOfflineAt (const Project& project,
                                      std::span<const DecodedAssetAudio> decoded,
                                      int maxBlockSize)
{
    OfflineRenderOptions options;
    options.maxBlockSize = maxBlockSize;
    return yesdaw::engine::renderOfflineProject (project, decoded, options);
}

std::vector<float> drainPlayback (PlaybackEngine& engine, const std::vector<int>& schedule)
{
    const int channels = static_cast<int> (engine.channels());
    REQUIRE (channels == 2);

    std::vector<float> out (static_cast<std::size_t> (engine.frames()) * static_cast<std::size_t> (channels), 0.0f);
    std::vector<float> storage (static_cast<std::size_t> (channels) * static_cast<std::size_t> (kMaxBlockSize), 0.0f);
    std::vector<float*> ptrs (static_cast<std::size_t> (channels), nullptr);
    for (int c = 0; c < channels; ++c)
        ptrs[static_cast<std::size_t> (c)] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (kMaxBlockSize);

    std::uint64_t offset = 0;
    std::size_t scheduleIndex = 0;
    while (offset < engine.frames())
    {
        const int requested = schedule[scheduleIndex % schedule.size()];
        REQUIRE (requested > 0);
        REQUIRE (requested <= kMaxBlockSize);
        const int frames = static_cast<int> (std::min<std::uint64_t> (engine.frames() - offset,
                                                                      static_cast<std::uint64_t> (requested)));

        engine.processBlock (ptrs.data(), channels, frames);

        for (int frame = 0; frame < frames; ++frame)
            for (int channel = 0; channel < channels; ++channel)
                out[(static_cast<std::size_t> (offset) + static_cast<std::size_t> (frame)) * 2u
                    + static_cast<std::size_t> (channel)] =
                    ptrs[static_cast<std::size_t> (channel)][frame];

        offset += static_cast<std::uint64_t> (frames);
        ++scheduleIndex;
    }

    return out;
}

std::vector<float> renderRealtimeAt (const Project& project,
                                     std::span<const DecodedAssetAudio> decoded,
                                     const std::vector<int>& schedule)
{
    OfflineRenderOptions options;
    options.maxBlockSize = kMaxBlockSize;
    PlaybackEngine::Result created = PlaybackEngine::create (project, decoded, options);
    REQUIRE (created.ok());
    std::vector<float> out = drainPlayback (*created.engine, schedule);
    created.engine->reclaim();
    return out;
}

Project writeAndReopen (const Project& project, const std::filesystem::path& bundlePath)
{
    const std::filesystem::path sourcePath = tempPath ("asset-source.bin");
    const std::vector<std::uint8_t> bytes { 0x59u, 0x45u, 0x53u, 0x44u, 0x41u, 0x57u, 0x48u, 0x31u, 0x34u };
    writeBytes (sourcePath, std::span<const std::uint8_t> (bytes.data(), bytes.size()));

    Project persisted = project;
    ProjectBundleDb db;
    REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
    Asset imported;
    REQUIRE (db.importAssetBytes ({ sourcePath, project.assets.front().id, kAssetFrames, project.sampleRate, 1 },
                                  imported)
                 .ok());
    persisted.assets.front() = imported;
    REQUIRE (persisted.hasValidAssetClipIndirection());
    REQUIRE (db.writeProjectSnapshot (persisted).ok());

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, reopened).ok());
    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    return readback;
}

} // namespace

TEST_CASE ("FX insert chain renders the full built-in suite identically offline and realtime",
           "[h14][cp9][fx-suite][offline-rt]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();
    const std::vector<float> samples = makeSamples();
    const Project project = makeFxSuiteProject (makeAsset (idFromLowByte (10)));
    const std::vector<DecodedAssetAudio> decoded = decodedFor (project, samples);

    const OfflineRenderResult offline = renderOfflineAt (project, decoded, kMaxBlockSize);
    REQUIRE (offline.ok());
    REQUIRE (offline.channels == 2u);
    REQUIRE (offline.frames > static_cast<std::uint64_t> (kAssetFrames));

    const std::vector<float> realtime = renderRealtimeAt (project, decoded, { 1, 2, 3, 5, 8, 13, 64, 333, 512 });
    REQUIRE (bitIdentical (realtime, offline.interleavedSamples));

    Project bypass = project;
    bypass.tracks[0].strip.fxChain.clear();
    const OfflineRenderResult dry = renderOfflineAt (bypass, decoded, kMaxBlockSize);
    REQUIRE (dry.ok());
    REQUIRE (changed (offline.interleavedSamples, dry.interleavedSamples));

    Project mutated = project;
    mutated.tracks[0].strip.fxChain[2].normalizedParams.back().second = 0.0; // delay mix off
    const OfflineRenderResult lessFx = renderOfflineAt (mutated, decoded, kMaxBlockSize);
    REQUIRE (lessFx.ok());
    REQUIRE (changed (offline.interleavedSamples, lessFx.interleavedSamples));

    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("FX insert chain output is bit-identical across CP9 block schedules",
           "[h14][cp9][fx-suite][block-size]")
{
    const std::vector<float> samples = makeSamples();
    const Project project = makeFxSuiteProject (makeAsset (idFromLowByte (10)));
    const std::vector<DecodedAssetAudio> decoded = decodedFor (project, samples);

    const OfflineRenderResult reference = renderOfflineAt (project, decoded, kMaxBlockSize);
    REQUIRE (reference.ok());

    for (const int blockSize : { 1, 2, 3, 4, 5, 6, 7, 8, 9, 64, 128, 333, 512 })
    {
        const OfflineRenderResult rendered = renderOfflineAt (project, decoded, blockSize);
        REQUIRE (rendered.ok());
        REQUIRE (rendered.frames == reference.frames);
        REQUIRE (rendered.channels == reference.channels);
        REQUIRE (bitIdentical (rendered.interleavedSamples, reference.interleavedSamples));
    }
}

TEST_CASE ("FX schema v7 insert chains save and reopen the full mix",
           "[h14][cp9][fx-suite][persistence]")
{
    const std::vector<float> samples = makeSamples();
    const Project project = makeFxSuiteProject (makeAsset (idFromLowByte (10)));
    const Project reopened = writeAndReopen (project, tempPath ("save-reopen.yesdaw"));
    const std::vector<DecodedAssetAudio> decoded = decodedFor (reopened, samples);

    REQUIRE (reopened.tracks.size() == 1u);
    REQUIRE (reopened.tracks[0].strip.fxChain.size() == 5u);
    REQUIRE (reopened.tracks[0].strip.fxChain[0].kind == FxKind::Eq);
    REQUIRE (reopened.tracks[0].strip.fxChain[1].kind == FxKind::Compressor);
    REQUIRE (reopened.tracks[0].strip.fxChain[2].kind == FxKind::Delay);
    REQUIRE (reopened.tracks[0].strip.fxChain[3].kind == FxKind::Reverb);
    REQUIRE (reopened.tracks[0].strip.fxChain[4].kind == FxKind::Limiter);

    const OfflineRenderResult rendered = renderOfflineAt (reopened, decoded, kMaxBlockSize);
    REQUIRE (rendered.ok());
    REQUIRE (rendered.channels == 2u);
}

TEST_CASE ("frozen CP9 FX-schema fixture bundle opens and re-renders on HEAD forever",
           "[h14][cp9][fx-suite][fixture]")
{
    const std::filesystem::path fixturePath { YESDAW_FX_SCHEMA_FIXTURE_PATH };
    REQUIRE (std::filesystem::exists (fixturePath / "project.db"));

    const std::filesystem::path workingPath = tempPath ("fixture-copy.yesdaw");
    std::error_code ec;
    std::filesystem::copy (fixturePath, workingPath, std::filesystem::copy_options::recursive, ec);
    REQUIRE (! ec);

    ProjectBundleDb db;
    REQUIRE (ProjectBundleDb::openExistingBundle (workingPath, db).ok());

    Project project;
    REQUIRE (db.readProjectSnapshot (project).ok());
    REQUIRE (project.tracks.size() == 1u);
    REQUIRE (project.tracks[0].strip.fxChain.size() == 5u);

    const std::vector<float> samples = makeSamples();
    const std::vector<DecodedAssetAudio> decoded = decodedFor (project, samples);
    const OfflineRenderResult first = renderOfflineAt (project, decoded, kMaxBlockSize);
    const OfflineRenderResult second = renderOfflineAt (project, decoded, 7);
    REQUIRE (first.ok());
    REQUIRE (second.ok());
    REQUIRE (bitIdentical (first.interleavedSamples, second.interleavedSamples));
}
