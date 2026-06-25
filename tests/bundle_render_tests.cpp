// YES DAW - H2 bundled Asset read/decode projection gate.
//
// Imports real audio bytes into a `.yesdaw` bundle, reopens the Project snapshot, decodes the bundled
// Asset file, and feeds Clip source windows through the graph/Render path without mutating Asset bytes.

#include "engine/ClipEnvelope.h"
#include "engine/GraphBuilder.h"
#include "engine/Project.h"
#include "engine/Runtime.h"
#include "engine/nodes/DecodedClipNode.h"
#include "engine/nodes/MasterNode.h"
#include "persistence/ProjectBundle.h"
#include "persistence/WaveformPeakCache.h"

#include <catch2/catch_test_macros.hpp>

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::DecodedClipNode;
using yesdaw::engine::EntityId;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::GraphId;
using yesdaw::engine::MasterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::Runtime;
using yesdaw::engine::SampleRate;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::evaluateClipGainEnvelope;
using yesdaw::engine::setClipFades;
using yesdaw::engine::setClipGain;
using yesdaw::engine::splitClip;
using yesdaw::persistence::AssetImportRequest;
using yesdaw::persistence::ProjectBundleDb;
using yesdaw::persistence::WaveformPeakCacheStatus;
using yesdaw::persistence::buildWaveformPeakCache;
using yesdaw::persistence::readWaveformPeakCache;
using yesdaw::persistence::waveformPeakCachePathForHash;
using yesdaw::persistence::waveformPeakCacheRelativePathForHash;
using yesdaw::persistence::writeWaveformPeakCache;

namespace {

constexpr NodeId kBundleRenderMasterId = 64000;
constexpr int    kMaxBlockSize = 128;
constexpr int    kRenderFrames = 384;
constexpr double kTolerance = 1.0e-7;

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

std::filesystem::path makeTempBundlePath (std::string_view label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
                               / ("yesdaw-" + std::string (label) + "-" + std::to_string (ticks) + ".yesdaw");

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

std::vector<std::uint8_t> readBytes (const std::filesystem::path& path)
{
    const auto size = std::filesystem::file_size (path);
    std::vector<std::uint8_t> bytes (static_cast<std::size_t> (size));

    std::ifstream input (path, std::ios::binary);
    REQUIRE (input.good());
    input.read (reinterpret_cast<char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    REQUIRE (input.good());
    return bytes;
}

void writeBytes (const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    std::error_code ec;
    std::filesystem::create_directories (path.parent_path(), ec);
    REQUIRE (! ec);

    std::ofstream output (path, std::ios::binary | std::ios::trunc);
    REQUIRE (output.good());
    output.write (reinterpret_cast<const char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    output.close();
    REQUIRE (output.good());
}

std::unique_ptr<juce::AudioFormatReader> openWavReader (const std::filesystem::path& path)
{
    juce::WavAudioFormat wav;
    const juce::File file { juce::String { path.string() } };
    return std::unique_ptr<juce::AudioFormatReader> (
        wav.createReaderFor (new juce::FileInputStream (file), true));
}

std::vector<float> decodeClipWindowFromBundle (const std::filesystem::path& bundlePath,
                                               const Asset& asset,
                                               const Clip& clip)
{
    REQUIRE (clip.sourceWindowFits (asset));

    const std::filesystem::path assetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash);

    const auto reader = openWavReader (assetPath);
    REQUIRE (reader != nullptr);
    REQUIRE (reader->sampleRate == asset.sampleRate.hz);
    REQUIRE (reader->numChannels == asset.channels);
    REQUIRE (reader->lengthInSamples == static_cast<juce::int64> (asset.frames));

    juce::AudioBuffer<float> decoded (1, static_cast<int> (clip.srcLen));
    REQUIRE (reader->read (&decoded,
                           0,
                           static_cast<int> (clip.srcLen),
                           static_cast<juce::int64> (clip.srcOffset),
                           true,
                           false));

    const float* const channel = decoded.getReadPointer (0);
    return std::vector<float> (channel, channel + static_cast<std::ptrdiff_t> (clip.srcLen));
}

std::vector<float> decodeFullAssetChannelMajorFromBundle (const std::filesystem::path& bundlePath,
                                                          const Asset& asset)
{
    REQUIRE (asset.channels == 1u);
    REQUIRE (asset.frames <= static_cast<std::uint64_t> (std::numeric_limits<int>::max()));

    const std::filesystem::path assetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash);

    const auto reader = openWavReader (assetPath);
    REQUIRE (reader != nullptr);
    REQUIRE (reader->sampleRate == asset.sampleRate.hz);
    REQUIRE (reader->numChannels == asset.channels);
    REQUIRE (reader->lengthInSamples == static_cast<juce::int64> (asset.frames));

    juce::AudioBuffer<float> decoded (1, static_cast<int> (asset.frames));
    REQUIRE (reader->read (&decoded, 0, static_cast<int> (asset.frames), 0, true, false));

    const float* const channel = decoded.getReadPointer (0);
    return std::vector<float> (channel, channel + static_cast<std::ptrdiff_t> (asset.frames));
}

bool applyClipGainEnvelopeToDecodedWindow (const Clip& clip, std::vector<float>& decoded)
{
    for (std::size_t frame = 0; frame < decoded.size(); ++frame)
    {
        if (frame > static_cast<std::size_t> (std::numeric_limits<Tick>::max()))
            return false;

        const auto envelope = evaluateClipGainEnvelope (clip, static_cast<Tick> (frame));
        if (! envelope.valid)
            return false;

        decoded[frame] *= envelope.gain;
    }

    return true;
}

std::unique_ptr<CompiledGraph> buildBundleClipProjection (const ProjectBundleDb& db,
                                                          const Project& project,
                                                          GraphId graphId)
{
    if (! project.hasValidAssetClipIndirection())
        return nullptr;

    GraphBuilder::Inputs inputs;
    inputs.id = graphId;
    inputs.masterNodeId = kBundleRenderMasterId;
    inputs.sampleRate = project.sampleRate.hz;
    inputs.maxBlockSize = kMaxBlockSize;

    std::vector<Node*> masterInputs;
    masterInputs.reserve (project.clips.size());
    inputs.nodes.reserve (project.clips.size() + 1u);

    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        const Clip& clip = project.clips[i];
        const Asset* const asset = project.findAsset (clip.assetId);
        if (asset == nullptr)
            return nullptr;

        std::vector<float> decoded = decodeClipWindowFromBundle (db.bundlePath(), *asset, clip);
        if (! applyClipGainEnvelopeToDecodedWindow (clip, decoded))
            return nullptr;

        auto source = std::make_unique<DecodedClipNode> (static_cast<NodeId> (3000u + i), std::move (decoded), 1);

        DecodedClipNode* const sourcePtr = source.get();

        inputs.nodes.push_back (std::move (source));
        masterInputs.push_back (sourcePtr);
    }

    auto master = std::make_unique<MasterNode> (kBundleRenderMasterId, 1);
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

std::vector<float> renderRealtimePath (const ProjectBundleDb& db, const Project& project)
{
    Runtime runtime;

    std::unique_ptr<CompiledGraph> graph = buildBundleClipProjection (db, project, 301);
    REQUIRE (graph != nullptr);
    REQUIRE (runtime.publish (std::move (graph)));

    std::vector<float> rendered = renderByBlocks (
        [&runtime] (float* out, int frames)
        {
            runtime.processBlock (out, frames);
        },
        { 128, 31, 79, 7 });

    runtime.reclaim();
    return rendered;
}

std::vector<float> renderOfflinePath (const ProjectBundleDb& db, const Project& project)
{
    std::unique_ptr<CompiledGraph> graph = buildBundleClipProjection (db, project, 302);
    REQUIRE (graph != nullptr);

    return renderByBlocks (
        [&graph] (float* out, int frames)
        {
            graph->process (out, frames);
        },
        { 23, 113, 41, 19 });
}

std::vector<float> expectedClipSum (const ProjectBundleDb& db, const Project& project)
{
    std::vector<float> expected (static_cast<std::size_t> (kRenderFrames), 0.0f);

    for (const Clip& clip : project.clips)
    {
        const Asset* const asset = project.findAsset (clip.assetId);
        REQUIRE (asset != nullptr);

        std::vector<float> decoded = decodeClipWindowFromBundle (db.bundlePath(), *asset, clip);
        REQUIRE (applyClipGainEnvelopeToDecodedWindow (clip, decoded));
        for (std::size_t i = 0; i < expected.size() && i < decoded.size(); ++i)
            expected[i] += decoded[i];
    }

    return expected;
}

std::vector<float> constantGainClipSum (const ProjectBundleDb& db, const Project& project)
{
    std::vector<float> expected (static_cast<std::size_t> (kRenderFrames), 0.0f);

    for (const Clip& clip : project.clips)
    {
        const Asset* const asset = project.findAsset (clip.assetId);
        REQUIRE (asset != nullptr);

        const std::vector<float> decoded = decodeClipWindowFromBundle (db.bundlePath(), *asset, clip);
        for (std::size_t i = 0; i < expected.size() && i < decoded.size(); ++i)
            expected[i] += decoded[i] * clip.gain;
    }

    return expected;
}

} // namespace

TEST_CASE ("bundled Asset bytes decode through Clip envelope indirection into graph Render", "[render][bundle][asset][clip][envelope]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();
    const auto bundlePath = makeTempBundlePath ("bundle-render");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    Asset imported;
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());

        const AssetImportRequest import {
            fixturePath,
            idFromLowByte (10),
            4096,
            SampleRate { 48000.0 },
            1,
        };
        REQUIRE (db.importAssetBytes (import, imported).ok());

        Clip first;
        first.id = idFromLowByte (20);
        first.assetId = imported.id;
        first.timelineStart = 0;
        first.timelineLength = 256;
        first.srcOffset = 1000;
        first.srcLen = 256;
        first.gain = 0.50f;
        first.fadeIn = 64;
        first.fadeOut = 96;
        first.timeBase = TimeBase::SampleLocked;

        Clip second;
        second.id = idFromLowByte (21);
        second.assetId = imported.id;
        second.timelineStart = 0;
        second.timelineLength = 128;
        second.srcOffset = 1200;
        second.srcLen = 128;
        second.gain = 0.25f;
        second.fadeIn = 32;
        second.fadeOut = 64;
        second.timeBase = TimeBase::SampleLocked;

        Project project;
        project.id = idFromLowByte (1);
        project.sampleRate = SampleRate { 48000.0 };
        project.assets.push_back (imported);
        project.clips.push_back (first);
        project.clips.push_back (second);
        REQUIRE (project.hasValidAssetClipIndirection());
        REQUIRE (db.writeProjectSnapshot (project).ok());
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, reopened).ok());

    Project project;
    REQUIRE (reopened.readProjectSnapshot (project).ok());
    REQUIRE (project.assets.size() == 1u);
    REQUIRE (project.clips.size() == 2u);

    const std::filesystem::path bundledAssetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (project.assets.front().contentHash);
    const std::vector<std::uint8_t> bytesBefore = readBytes (bundledAssetPath);
    const Project projectBeforeRender = project;

    const std::vector<float> rt = renderRealtimePath (reopened, project);
    const std::vector<float> offline = renderOfflinePath (reopened, project);
    const std::vector<float> expected = expectedClipSum (reopened, project);
    const std::vector<float> constantGain = constantGainClipSum (reopened, project);
    REQUIRE (rt.size() == offline.size());
    REQUIRE (offline.size() == expected.size());
    REQUIRE (constantGain.size() == expected.size());

    double maxRtOfflineDiff = 0.0;
    double maxExpectedDiff = 0.0;
    double maxConstantGainDiff = 0.0;
    double peak = 0.0;
    for (std::size_t i = 0; i < offline.size(); ++i)
    {
        maxRtOfflineDiff = std::max (maxRtOfflineDiff, std::fabs (static_cast<double> (rt[i] - offline[i])));
        maxExpectedDiff = std::max (maxExpectedDiff, std::fabs (static_cast<double> (offline[i] - expected[i])));
        maxConstantGainDiff = std::max (maxConstantGainDiff,
                                        std::fabs (static_cast<double> (constantGain[i] - expected[i])));
        peak = std::max (peak, std::fabs (static_cast<double> (offline[i])));
    }

    INFO ("RT/offline diff = " << maxRtOfflineDiff);
    INFO ("expected decoded Clip diff = " << maxExpectedDiff);
    INFO ("old constant-gain projection diff = " << maxConstantGainDiff);
    REQUIRE (peak > 0.01);
    REQUIRE (maxRtOfflineDiff <= kTolerance);
    REQUIRE (maxExpectedDiff <= kTolerance);
    REQUIRE (maxConstantGainDiff > 1.0e-3);
    REQUIRE (project.id == projectBeforeRender.id);
    REQUIRE (project.sampleRate == projectBeforeRender.sampleRate);
    REQUIRE (project.assets == projectBeforeRender.assets);
    REQUIRE (project.clips == projectBeforeRender.clips);
    REQUIRE (readBytes (bundledAssetPath) == bytesBefore);

    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("split Clip with crossfade metadata renders identically through RT and offline paths", "[render][bundle][asset][clip][split][crossfade]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();
    const auto bundlePath = makeTempBundlePath ("split-crossfade-render");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    Asset imported;
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());

        const AssetImportRequest import {
            fixturePath,
            idFromLowByte (40),
            4096,
            SampleRate { 48000.0 },
            1,
        };
        REQUIRE (db.importAssetBytes (import, imported).ok());

        Clip original;
        original.id = idFromLowByte (41);
        original.assetId = imported.id;
        original.timelineStart = 512;
        original.timelineLength = 256;
        original.srcOffset = 960;
        original.srcLen = 256;
        original.gain = 1.0f;
        original.fadeIn = 0;
        original.fadeOut = 0;
        original.timeBase = TimeBase::SampleLocked;

        Project project;
        project.id = idFromLowByte (42);
        project.sampleRate = SampleRate { 48000.0 };
        project.assets.push_back (imported);
        project.clips.push_back (original);
        REQUIRE (project.hasValidAssetClipIndirection());

        const Asset immutableAsset = project.assets.front();
        const EntityId rightId = idFromLowByte (43);
        constexpr Tick crossfadeTicks = 64;
        REQUIRE (setClipGain (project, original.id, 0.625f) == ProjectEditStatus::Applied);
        REQUIRE (splitClip (project, original.id, rightId, 128, 128) == ProjectEditStatus::Applied);
        REQUIRE (setClipFades (project, original.id, 0, crossfadeTicks) == ProjectEditStatus::Applied);
        REQUIRE (setClipFades (project, rightId, crossfadeTicks, 0) == ProjectEditStatus::Applied);

        REQUIRE (project.assets.size() == 1u);
        REQUIRE (project.assets.front() == immutableAsset);
        REQUIRE (project.clips.size() == 2u);
        REQUIRE (project.hasValidAssetClipIndirection());

        const Clip& left = project.clips[0];
        const Clip& right = project.clips[1];
        REQUIRE (left.id == original.id);
        REQUIRE (right.id == rightId);
        REQUIRE (left.assetId == imported.id);
        REQUIRE (right.assetId == imported.id);
        REQUIRE (right.timelineStart == left.timelineStart + left.timelineLength);
        REQUIRE (right.srcOffset == left.srcOffset + left.srcLen);
        REQUIRE (left.timelineLength == 128);
        REQUIRE (right.timelineLength == 128);
        REQUIRE (left.srcLen == 128u);
        REQUIRE (right.srcLen == 128u);
        REQUIRE (left.gain == right.gain);
        REQUIRE (left.fadeOut == crossfadeTicks);
        REQUIRE (right.fadeIn == crossfadeTicks);

        const auto leftMidpoint = evaluateClipGainEnvelope (left, left.timelineLength - (crossfadeTicks / 2));
        const auto rightMidpoint = evaluateClipGainEnvelope (right, crossfadeTicks / 2);
        REQUIRE (leftMidpoint.valid);
        REQUIRE (rightMidpoint.valid);
        REQUIRE (std::fabs (static_cast<double> (leftMidpoint.gain - rightMidpoint.gain)) <= 1.0e-6);

        const Project projectBeforeWrite = project;
        REQUIRE (db.writeProjectSnapshot (project).ok());
        REQUIRE (project.id == projectBeforeWrite.id);
        REQUIRE (project.sampleRate == projectBeforeWrite.sampleRate);
        REQUIRE (project.assets == projectBeforeWrite.assets);
        REQUIRE (project.clips == projectBeforeWrite.clips);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, reopened).ok());

    Project project;
    REQUIRE (reopened.readProjectSnapshot (project).ok());
    REQUIRE (project.assets.size() == 1u);
    REQUIRE (project.clips.size() == 2u);
    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (project.clips[1].timelineStart == project.clips[0].timelineStart + project.clips[0].timelineLength);
    REQUIRE (project.clips[1].srcOffset == project.clips[0].srcOffset + project.clips[0].srcLen);

    const std::filesystem::path bundledAssetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (project.assets.front().contentHash);
    const std::vector<std::uint8_t> bytesBefore = readBytes (bundledAssetPath);
    const Project projectBeforeRender = project;

    const std::vector<float> rt = renderRealtimePath (reopened, project);
    const std::vector<float> offline = renderOfflinePath (reopened, project);
    const std::vector<float> expected = expectedClipSum (reopened, project);
    const std::vector<float> constantGain = constantGainClipSum (reopened, project);
    REQUIRE (rt.size() == offline.size());
    REQUIRE (offline.size() == expected.size());
    REQUIRE (constantGain.size() == expected.size());

    double maxRtOfflineDiff = 0.0;
    double maxExpectedDiff = 0.0;
    double maxConstantGainDiff = 0.0;
    double peak = 0.0;
    for (std::size_t i = 0; i < offline.size(); ++i)
    {
        maxRtOfflineDiff = std::max (maxRtOfflineDiff, std::fabs (static_cast<double> (rt[i] - offline[i])));
        maxExpectedDiff = std::max (maxExpectedDiff, std::fabs (static_cast<double> (offline[i] - expected[i])));
        maxConstantGainDiff = std::max (maxConstantGainDiff,
                                        std::fabs (static_cast<double> (constantGain[i] - expected[i])));
        peak = std::max (peak, std::fabs (static_cast<double> (offline[i])));
    }

    INFO ("split crossfade RT/offline diff = " << maxRtOfflineDiff);
    INFO ("split crossfade expected decoded Clip diff = " << maxExpectedDiff);
    INFO ("split crossfade old constant-gain projection diff = " << maxConstantGainDiff);
    REQUIRE (peak > 0.01);
    REQUIRE (maxRtOfflineDiff <= kTolerance);
    REQUIRE (maxExpectedDiff <= kTolerance);
    REQUIRE (maxConstantGainDiff > 1.0e-3);
    REQUIRE (project.id == projectBeforeRender.id);
    REQUIRE (project.sampleRate == projectBeforeRender.sampleRate);
    REQUIRE (project.assets == projectBeforeRender.assets);
    REQUIRE (project.clips == projectBeforeRender.clips);
    REQUIRE (readBytes (bundledAssetPath) == bytesBefore);

    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("bundled Asset waveform peak cache is content hash keyed and regenerable", "[bundle][asset][peaks]")
{
    const auto bundlePath = makeTempBundlePath ("bundle-peaks");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    Asset imported;
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());

        const AssetImportRequest import {
            fixturePath,
            idFromLowByte (30),
            4096,
            SampleRate { 48000.0 },
            1,
        };
        REQUIRE (db.importAssetBytes (import, imported).ok());

        Project project;
        project.id = idFromLowByte (31);
        project.sampleRate = SampleRate { 48000.0 };
        project.assets.push_back (imported);
        REQUIRE (project.hasValidAssetClipIndirection());
        REQUIRE (db.writeProjectSnapshot (project).ok());
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, reopened).ok());

    Project project;
    REQUIRE (reopened.readProjectSnapshot (project).ok());
    REQUIRE (project.assets.size() == 1u);
    const Asset asset = project.assets.front();

    const std::vector<float> decoded = decodeFullAssetChannelMajorFromBundle (bundlePath, asset);
    const auto built = buildWaveformPeakCache (asset, std::span<const float> (decoded.data(), decoded.size()));
    INFO (built.message);
    REQUIRE (built.ok());
    REQUIRE (built.cache.tiers.size() == 2u);
    REQUIRE (built.cache.tiers[0].framesPerPeak == 256u);
    REQUIRE (built.cache.tiers[0].peaks.size() == 16u);
    REQUIRE (built.cache.tiers[1].framesPerPeak == 4096u);
    REQUIRE (built.cache.tiers[1].peaks.size() == 1u);
    REQUIRE (built.cache.tiers[1].peaks.front().max > 0.49f);
    REQUIRE (built.cache.tiers[1].peaks.front().min < -0.49f);
    REQUIRE (built.cache.tiers[1].peaks.front().rms > 0.35f);

    const std::string relativePeakPath = waveformPeakCacheRelativePathForHash (asset.contentHash);
    REQUIRE (relativePeakPath.rfind ("peaks/", 0) == 0);

    const std::filesystem::path peakPath = waveformPeakCachePathForHash (bundlePath, asset.contentHash);
    REQUIRE (writeWaveformPeakCache (bundlePath, built.cache).ok());
    REQUIRE (std::filesystem::exists (peakPath));

    const auto loaded = readWaveformPeakCache (bundlePath, asset.contentHash);
    INFO (loaded.message);
    REQUIRE (loaded.ok());
    REQUIRE (loaded.cache == built.cache);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath / "peaks", ec);
    REQUIRE (! ec);
    REQUIRE_FALSE (std::filesystem::exists (peakPath));

    ProjectBundleDb afterDelete;
    REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, afterDelete).ok());

    Project readback;
    REQUIRE (afterDelete.readProjectSnapshot (readback).ok());
    REQUIRE (readback.id == project.id);
    REQUIRE (readback.sampleRate == project.sampleRate);
    REQUIRE (readback.assets == project.assets);
    REQUIRE (readback.clips == project.clips);

    const auto regenerated = buildWaveformPeakCache (asset, std::span<const float> (decoded.data(), decoded.size()));
    INFO (regenerated.message);
    REQUIRE (regenerated.ok());
    REQUIRE (regenerated.cache == loaded.cache);
    REQUIRE (writeWaveformPeakCache (bundlePath, regenerated.cache).ok());
    REQUIRE (readWaveformPeakCache (bundlePath, asset.contentHash).cache == loaded.cache);

    const std::vector<std::uint8_t> corrupt { 'n', 'o', 'p', 'e' };
    writeBytes (peakPath, std::span<const std::uint8_t> (corrupt.data(), corrupt.size()));
    const auto rejected = readWaveformPeakCache (bundlePath, asset.contentHash);
    REQUIRE (rejected.status == WaveformPeakCacheStatus::FormatInvalid);

    REQUIRE (writeWaveformPeakCache (bundlePath, regenerated.cache).ok());
    std::vector<std::uint8_t> wrongHash = readBytes (peakPath);
    REQUIRE (wrongHash.size() > 16u);
    wrongHash[16] ^= 0x01u;
    writeBytes (peakPath, std::span<const std::uint8_t> (wrongHash.data(), wrongHash.size()));
    REQUIRE (readWaveformPeakCache (bundlePath, asset.contentHash).status == WaveformPeakCacheStatus::FormatInvalid);

    REQUIRE (writeWaveformPeakCache (bundlePath, regenerated.cache).ok());
    std::vector<std::uint8_t> truncated = readBytes (peakPath);
    REQUIRE_FALSE (truncated.empty());
    truncated.pop_back();
    writeBytes (peakPath, std::span<const std::uint8_t> (truncated.data(), truncated.size()));
    REQUIRE (readWaveformPeakCache (bundlePath, asset.contentHash).status == WaveformPeakCacheStatus::FormatInvalid);

    REQUIRE (writeWaveformPeakCache (bundlePath, regenerated.cache).ok());
    std::vector<std::uint8_t> nanPayload = readBytes (peakPath);
    constexpr std::size_t firstPayloadRmsOffset = 4u + 2u + 2u + 8u + 32u + 4u + 4u + 8u + 8u;
    REQUIRE (nanPayload.size() >= firstPayloadRmsOffset + 4u);
    constexpr std::uint32_t quietNanBits = 0x7FC00000u;
    for (std::size_t i = 0; i < 4u; ++i)
        nanPayload[firstPayloadRmsOffset + i] = static_cast<std::uint8_t> ((quietNanBits >> (i * 8u)) & 0xFFu);
    writeBytes (peakPath, std::span<const std::uint8_t> (nanPayload.data(), nanPayload.size()));
    REQUIRE (readWaveformPeakCache (bundlePath, asset.contentHash).status == WaveformPeakCacheStatus::FormatInvalid);

    REQUIRE (writeWaveformPeakCache (bundlePath, regenerated.cache).ok());
    const auto recovered = readWaveformPeakCache (bundlePath, asset.contentHash);
    REQUIRE (recovered.ok());
    REQUIRE (recovered.cache == loaded.cache);
}
