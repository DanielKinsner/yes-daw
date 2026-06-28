// YES DAW - H7 offline render/export gate.
//
// Proves Project -> OfflineRenderer -> canonical float32 WAV -> bundle Asset import/decode without
// comparing the engine against itself.

#include "engine/OfflineRenderer.h"
#include "io/WavFile.h"
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
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::OfflineRenderResult;
using yesdaw::engine::OfflineRenderStatus;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::renderOfflineProject;
using yesdaw::io::Float32Wav;
using yesdaw::io::WavStatus;
using yesdaw::io::readFloat32WavFile;
using yesdaw::io::writeFloat32WavFile;
using yesdaw::persistence::AssetImportRequest;
using yesdaw::persistence::ProjectBundleDb;

namespace {

constexpr float kCenterGain = 0.70710677f;
constexpr double kTolerance = 1.0e-6;

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

std::filesystem::path makeTempPath (std::string_view label, std::string_view extension)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
                               / ("yesdaw-h7-" + std::string (label) + "-" + std::to_string (ticks) + std::string (extension));

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

bool floatBuffersBitIdentical (std::span<const float> a, std::span<const float> b) noexcept
{
    if (a.size() != b.size())
        return false;

    for (std::size_t i = 0; i < a.size(); ++i)
        if (floatBits (a[i]) != floatBits (b[i]))
            return false;

    return true;
}

bool buffersNear (std::span<const float> a, std::span<const float> b, double tolerance = kTolerance) noexcept
{
    if (a.size() != b.size())
        return false;

    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::fabs (static_cast<double> (a[i] - b[i])) > tolerance)
            return false;

    return true;
}

// The reference fade is the canonical LINEAR ramp that DecodedClipNode applies on the realtime path
// (fade-in * fade-out, anchored to the clip's source-frame count) — derived from the textbook definition,
// not copied from the engine's equal-power polynomial. The offline renderer plays Clips through that same
// node, so this is what both the export and playback produce.
float independentLinearFade (const Clip& clip, Tick localFrame, std::uint64_t totalFrames) noexcept
{
    float gain = 1.0f;
    if (clip.fadeIn > 0 && localFrame < clip.fadeIn)
        gain *= static_cast<float> (localFrame) / static_cast<float> (clip.fadeIn);

    if (clip.fadeOut > 0)
    {
        const Tick fadeStart = static_cast<Tick> (totalFrames) - clip.fadeOut;
        if (localFrame >= fadeStart)
            gain *= static_cast<float> (static_cast<Tick> (totalFrames) - localFrame)
                  / static_cast<float> (clip.fadeOut);
    }

    return gain;
}

struct OfflineFixture
{
    Project project;
    std::vector<std::vector<float>> samples;
    std::vector<DecodedAssetAudio> decodedAssets;
};

OfflineFixture makeOfflineFixture()
{
    OfflineFixture fixture;
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

    Asset second;
    second.id = idFromLowByte (11);
    second.contentHash = hashWithSeed (11);
    second.frames = static_cast<std::uint64_t> (fixture.samples[1].size());
    second.sampleRate = SampleRate { 48000.0 };
    second.channels = 1;

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

const std::vector<float>& samplesForAsset (const OfflineFixture& fixture, EntityId assetId)
{
    for (std::size_t i = 0; i < fixture.project.assets.size(); ++i)
        if (fixture.project.assets[i].id == assetId)
            return fixture.samples[i];

    FAIL ("missing fixture samples for asset");
    static const std::vector<float> empty;
    return empty;
}

std::vector<float> independentProjectReference (const OfflineFixture& fixture, int secondClipStartDelta = 0)
{
    std::uint64_t frames = 0;
    for (std::size_t clipIndex = 0; clipIndex < fixture.project.clips.size(); ++clipIndex)
    {
        const Clip& clip = fixture.project.clips[clipIndex];
        const Tick start = clip.timelineStart + (clipIndex == 1u ? static_cast<Tick> (secondClipStartDelta) : 0);
        REQUIRE (start >= 0);
        frames = std::max<std::uint64_t> (frames, static_cast<std::uint64_t> (start + clip.timelineLength));
    }

    std::vector<float> expected (static_cast<std::size_t> (frames) * 2u, 0.0f);
    for (std::size_t clipIndex = 0; clipIndex < fixture.project.clips.size(); ++clipIndex)
    {
        const Clip& clip = fixture.project.clips[clipIndex];
        const auto& samples = samplesForAsset (fixture, clip.assetId);
        const Tick start = clip.timelineStart + (clipIndex == 1u ? static_cast<Tick> (secondClipStartDelta) : 0);
        const std::uint64_t sourceFrames = std::min<std::uint64_t> (clip.srcLen, static_cast<std::uint64_t> (clip.timelineLength));

        for (std::uint64_t local = 0; local < sourceFrames; ++local)
        {
            const float source = samples[static_cast<std::size_t> (clip.srcOffset + local)];
            const float value = source
                              * independentLinearFade (clip, static_cast<Tick> (local), sourceFrames)
                              * clip.gain
                              * kCenterGain;
            const std::size_t frame = static_cast<std::size_t> (start + static_cast<Tick> (local));
            expected[frame * 2u] += value;
            expected[frame * 2u + 1u] += value;
        }
    }

    return expected;
}

OfflineRenderResult renderFixtureAt (const OfflineFixture& fixture, int maxBlockSize)
{
    OfflineRenderOptions options;
    options.maxBlockSize = maxBlockSize;
    return renderOfflineProject (fixture.project,
                                 std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(),
                                                                     fixture.decodedAssets.size()),
                                 options);
}

} // namespace

TEST_CASE ("OfflineRenderer renders the full Project timeline against an independent reference",
           "[h7][offline-render][project]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();

    const OfflineFixture fixture = makeOfflineFixture();
    const auto rendered = renderOfflineProject (fixture.project,
                                                std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(),
                                                                                   fixture.decodedAssets.size()),
                                                OfflineRenderOptions {});

    REQUIRE (rendered.ok());
    REQUIRE (rendered.sampleRate == fixture.project.sampleRate);
    REQUIRE (rendered.channels == 2u);
    REQUIRE (rendered.frames == 9u);
    REQUIRE (rendered.interleavedSamples.size() == static_cast<std::size_t> (rendered.frames * rendered.channels));

    const std::vector<float> expected = independentProjectReference (fixture);
    REQUIRE (buffersNear (rendered.interleavedSamples, expected));

    // Negative controls: a one-frame clip-position mutation and a dropped-tail mutation both fail the
    // same compare the real gate uses.
    const std::vector<float> wrongPosition = independentProjectReference (fixture, -1);
    REQUIRE_FALSE (buffersNear (rendered.interleavedSamples, wrongPosition));

    std::vector<float> droppedTail = expected;
    droppedTail.resize (droppedTail.size() - rendered.channels);
    REQUIRE_FALSE (buffersNear (rendered.interleavedSamples, droppedTail));

    Project tempoLocked = fixture.project;
    tempoLocked.clips[0].timeBase = TimeBase::TempoLocked;
    const auto rejected = renderOfflineProject (tempoLocked,
                                                std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(),
                                                                                   fixture.decodedAssets.size()));
    REQUIRE (rejected.status == OfflineRenderStatus::UnsupportedTimeBase);

    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("OfflineRenderer output is identical at every block size (ADR-0008)",
           "[h7][offline-render][block-size]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();
    const OfflineFixture fixture = makeOfflineFixture();

    // The fixture timeline is 9 frames, so the default 128-frame block renders it in ONE block — the
    // multi-block path (offset advance, every node's cross-block playFrame/ramp state) is otherwise dead.
    // Block-size independence is THE defining property of an offline renderer (ADR-0008): require
    // bit-identical output at block sizes that force 9, 5, 3, 2, 1 ... blocks.
    const OfflineRenderResult reference = renderFixtureAt (fixture, 128);
    REQUIRE (reference.ok());
    REQUIRE (reference.frames == 9u);
    REQUIRE (reference.channels == 2u);

    for (const int blockSize : { 1, 2, 3, 4, 5, 7, 8, 9, 13, 64, 256 })
    {
        const OfflineRenderResult rendered = renderFixtureAt (fixture, blockSize);
        REQUIRE (rendered.ok());
        REQUIRE (rendered.frames == reference.frames);
        REQUIRE (rendered.channels == reference.channels);
        REQUIRE (floatBuffersBitIdentical (rendered.interleavedSamples, reference.interleavedSamples));
    }

    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("OfflineRenderer output tracks a Project mutation (the renderer is exercised, not just the compare)",
           "[h7][offline-render][mutation]")
{
    const OfflineFixture baselineFixture = makeOfflineFixture();
    const OfflineRenderResult baseline = renderFixtureAt (baselineFixture, 4);
    REQUIRE (baseline.ok());

    // Mutate the renderer INPUT (halve a clip's gain) rather than perturbing the reference. The render
    // itself must change, and still match the independent reference recomputed for the mutated Project.
    OfflineFixture mutatedFixture = makeOfflineFixture();
    mutatedFixture.project.clips[0].gain *= 0.5f;
    const OfflineRenderResult mutated = renderFixtureAt (mutatedFixture, 4);
    REQUIRE (mutated.ok());

    REQUIRE_FALSE (floatBuffersBitIdentical (mutated.interleavedSamples, baseline.interleavedSamples));
    REQUIRE (buffersNear (mutated.interleavedSamples, independentProjectReference (mutatedFixture)));
}

TEST_CASE ("Canonical float32 WAV writes and reads bit-exact samples", "[h7][wav][float32]")
{
    const std::filesystem::path path = makeTempPath ("roundtrip", ".wav");
    const std::vector<float> samples {
        0.0f, -0.0f,
        0.125f, -0.25f,
        0.5f, -0.75f,
        1.0f, -1.0f,
    };

    REQUIRE (writeFloat32WavFile (path, SampleRate { 48000.0 }, 2, 4, samples).ok());

    Float32Wav decoded;
    REQUIRE (readFloat32WavFile (path, decoded).ok());
    REQUIRE (decoded.sampleRate == SampleRate { 48000.0 });
    REQUIRE (decoded.channels == 2u);
    REQUIRE (decoded.frames == 4u);
    REQUIRE (floatBuffersBitIdentical (decoded.interleavedSamples, samples));

    std::vector<float> scaled = samples;
    scaled[3] *= 0.5f;
    const std::filesystem::path scaledPath = makeTempPath ("scaled", ".wav");
    REQUIRE (writeFloat32WavFile (scaledPath, SampleRate { 48000.0 }, 2, 4, scaled).ok());

    Float32Wav scaledDecoded;
    REQUIRE (readFloat32WavFile (scaledPath, scaledDecoded).ok());
    REQUIRE_FALSE (floatBuffersBitIdentical (scaledDecoded.interleavedSamples, samples));

    const std::filesystem::path notWave = makeTempPath ("not-wave", ".wav");
    const std::array<std::uint8_t, 4> invalid { 'n', 'o', 'p', 'e' };
    writeBytes (notWave, invalid);
    REQUIRE (readFloat32WavFile (notWave, decoded).status == WavStatus::FormatInvalid);

    std::vector<std::uint8_t> truncated = readBytes (path);
    REQUIRE_FALSE (truncated.empty());
    truncated.pop_back();
    const std::filesystem::path truncatedPath = makeTempPath ("truncated", ".wav");
    writeBytes (truncatedPath, std::span<const std::uint8_t> (truncated.data(), truncated.size()));
    REQUIRE (readFloat32WavFile (truncatedPath, decoded).status == WavStatus::FormatInvalid);

    std::vector<float> nonFinite = samples;
    nonFinite[0] = std::numeric_limits<float>::infinity();
    REQUIRE (writeFloat32WavFile (makeTempPath ("nonfinite", ".wav"),
                                  SampleRate { 48000.0 },
                                  2,
                                  4,
                                  nonFinite).status == WavStatus::InvalidArgument);
}

TEST_CASE ("Offline render exports to WAV and re-imports through the Project bundle Asset path",
           "[h7][offline-render][export][bundle]")
{
    const OfflineFixture fixture = makeOfflineFixture();
    const auto rendered = renderOfflineProject (fixture.project,
                                                std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(),
                                                                                   fixture.decodedAssets.size()));
    REQUIRE (rendered.ok());

    const std::filesystem::path exportPath = makeTempPath ("export", ".wav");
    REQUIRE (writeFloat32WavFile (exportPath,
                                  rendered.sampleRate,
                                  rendered.channels,
                                  rendered.frames,
                                  rendered.interleavedSamples).ok());

    const std::filesystem::path bundlePath = makeTempPath ("export-import", ".yesdaw");
    ProjectBundleDb db;
    REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());

    Asset imported;
    const AssetImportRequest import {
        exportPath,
        idFromLowByte (90),
        rendered.frames,
        rendered.sampleRate,
        rendered.channels,
    };
    REQUIRE (db.importAssetBytes (import, imported).ok());

    Project project;
    project.id = idFromLowByte (91);
    project.sampleRate = rendered.sampleRate;
    project.assets.push_back (imported);
    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (db.writeProjectSnapshot (project).ok());

    const std::filesystem::path importedAssetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (imported.contentHash);
    Float32Wav decodedImport;
    REQUIRE (readFloat32WavFile (importedAssetPath, decodedImport).ok());
    REQUIRE (decodedImport.sampleRate == rendered.sampleRate);
    REQUIRE (decodedImport.channels == rendered.channels);
    REQUIRE (decodedImport.frames == rendered.frames);
    REQUIRE (floatBuffersBitIdentical (decodedImport.interleavedSamples, rendered.interleavedSamples));

    std::vector<std::uint8_t> corrupt = readBytes (exportPath);
    REQUIRE_FALSE (corrupt.empty());
    corrupt.pop_back();
    const std::filesystem::path corruptPath = makeTempPath ("corrupt-export", ".wav");
    writeBytes (corruptPath, std::span<const std::uint8_t> (corrupt.data(), corrupt.size()));

    Asset corruptImported;
    const AssetImportRequest corruptImport {
        corruptPath,
        idFromLowByte (92),
        rendered.frames,
        rendered.sampleRate,
        rendered.channels,
    };
    REQUIRE (db.importAssetBytes (corruptImport, corruptImported).ok());

    const std::filesystem::path corruptAssetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (corruptImported.contentHash);
    REQUIRE (readFloat32WavFile (corruptAssetPath, decodedImport).status == WavStatus::FormatInvalid);
}
