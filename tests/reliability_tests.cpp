// YES DAW - H6 reliability gate: deadline soak + autosave recovery.

#include "engine/GraphBuilder.h"
#include "engine/Reliability.h"
#include "engine/nodes/IdentityDcNode.h"
#include "engine/nodes/MasterNode.h"
#include "persistence/AutosaveRecovery.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::DeadlineSoakStats;
using yesdaw::engine::EntityId;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::IdentityDcNode;
using yesdaw::engine::MasterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::engine::summarizeDeadlineSoak;
using yesdaw::persistence::ProjectBundleDb;
using yesdaw::persistence::restoreAutosaveSnapshot;
using yesdaw::persistence::writeAutosaveSnapshot;

namespace {

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
                               / ("yesdaw-h6-" + std::string (label) + "-" + std::to_string (ticks) + ".yesdaw");

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

std::vector<std::uint8_t> assetBytesForId (EntityId id)
{
    return {
        0x59u,
        0x45u,
        0x53u,
        0x44u,
        0x41u,
        0x57u,
        id.bytes.back(),
        static_cast<std::uint8_t> (id.bytes.back() + 17u),
        static_cast<std::uint8_t> (id.bytes.back() + 43u),
    };
}

AssetContentHash hashBytes (std::span<const std::uint8_t> bytes) noexcept
{
    return yesdaw::persistence::detail::sha256Bytes (bytes);
}

Asset makeAsset (EntityId id, std::uint64_t frames = 48000)
{
    Asset asset;
    asset.id = id;
    const std::vector<std::uint8_t> bytes = assetBytesForId (id);
    asset.contentHash = hashBytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
    asset.frames = frames;
    asset.sampleRate = SampleRate { 48000.0 };
    asset.channels = 2;
    return asset;
}

Clip makeClip (EntityId id, EntityId assetId, std::uint64_t srcOffset, std::uint64_t srcLen)
{
    Clip clip;
    clip.id = id;
    clip.assetId = assetId;
    clip.timelineStart = 0;
    clip.timelineLength = 15360;
    clip.srcOffset = srcOffset;
    clip.srcLen = srcLen;
    clip.gain = 0.75f;
    clip.fadeIn = 16;
    clip.fadeOut = 32;
    clip.timeBase = TimeBase::SampleLocked;
    return clip;
}

Project makeProject()
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };
    project.assets = {
        makeAsset (idFromLowByte (2), 1000),
        makeAsset (idFromLowByte (3), 256),
    };
    project.clips = {
        makeClip (idFromLowByte (4), project.assets[0].id, 100, 900),
        makeClip (idFromLowByte (5), project.assets[1].id, 0, 128),
    };
    return project;
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

void writeProjectAssetFiles (const std::filesystem::path& bundlePath, const Project& project)
{
    for (const Asset& asset : project.assets)
    {
        const std::vector<std::uint8_t> bytes = assetBytesForId (asset.id);
        REQUIRE (hashBytes (std::span<const std::uint8_t> (bytes.data(), bytes.size())) == asset.contentHash);
        writeBytes (bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash),
                    std::span<const std::uint8_t> (bytes.data(), bytes.size()));
    }
}

void requireSameProjectSurface (const Project& actual, const Project& expected)
{
    REQUIRE (actual.id == expected.id);
    REQUIRE (actual.sampleRate == expected.sampleRate);
    REQUIRE (actual.assets == expected.assets);
    REQUIRE (actual.clips == expected.clips);
    REQUIRE (actual.tempoMap == expected.tempoMap);
    REQUIRE (actual.meterMap == expected.meterMap);
    REQUIRE (actual.markers == expected.markers);
    REQUIRE (actual.midiClips == expected.midiClips);
}

std::unique_ptr<CompiledGraph> makeHeavySessionGraph (int trackCount, int blockSize)
{
    constexpr NodeId kMasterId = 900000u;

    std::vector<Node*> masterInputs;
    masterInputs.reserve (static_cast<std::size_t> (trackCount));

    GraphBuilder::Inputs inputs;
    inputs.id = 1900;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = 48000.0;
    inputs.maxBlockSize = blockSize;
    inputs.nodes.reserve (static_cast<std::size_t> (trackCount) + 1u);

    for (int track = 0; track < trackCount; ++track)
    {
        auto source = std::make_unique<IdentityDcNode> (static_cast<NodeId> (track + 1),
                                                        0.001f * static_cast<float> ((track % 11) + 1),
                                                        1);
        masterInputs.push_back (source.get());
        inputs.nodes.push_back (std::move (source));
    }

    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes (std::move (masterInputs));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->debugCompiledNodes().size() >= static_cast<std::size_t> (trackCount + 1));
    return graph;
}

DeadlineSoakStats runDeadlineSoak (CompiledGraph& graph,
                                   int blockSize,
                                   std::uint64_t seconds)
{
    constexpr double kSampleRate = 48000.0;
    const std::uint64_t totalBlocks = yesdaw::engine::audioBlocksForDuration (kSampleRate, blockSize, seconds);
    REQUIRE (totalBlocks > 0);

    std::vector<float> out (static_cast<std::size_t> (blockSize), 0.0f);
    std::vector<std::uint64_t> blockNanos;
    blockNanos.reserve (static_cast<std::size_t> (totalBlocks));

    double checksum = 0.0;
    for (std::uint64_t block = 0; block < totalBlocks; ++block)
    {
        const auto before = std::chrono::steady_clock::now();
        graph.process (out.data(), blockSize);
        const auto after = std::chrono::steady_clock::now();

        const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds> (after - before).count();
        blockNanos.push_back (nanos > 0 ? static_cast<std::uint64_t> (nanos) : 1u);

        if ((block & 0xFFFu) == 0u)
            checksum += out[static_cast<std::size_t> (block % static_cast<std::uint64_t> (blockSize))];
    }

    REQUIRE (blockNanos.size() == totalBlocks);
    REQUIRE (std::isfinite (checksum));
    REQUIRE (checksum > 0.0);

    // blockNanos.size() was REQUIRE'd above to equal totalBlocks, so the summarizer's own measured
    // count is authoritative — do not overwrite blocksProcessed with the planned total.
    return summarizeDeadlineSoak (blockNanos, kSampleRate, blockSize, seconds, 0);
}

} // namespace

// Negative control for the deadline oracle. The live soak above runs the real engine with a comfortable
// margin, so by itself it can never drive summarizeDeadlineSoak/passesDeadline to FALSE — i.e. the oracle
// has no biting test. This case feeds hand-built block-time distributions with known answers so a future
// regression in the percentile index, the strict-< comparison, or the underrun/empty guards goes red.
TEST_CASE ("H6 deadline oracle rejects over-budget, underrun, and empty soaks (negative control)",
           "[h6][reliability][deadline][negative-control]")
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 128;
    const std::uint64_t period = yesdaw::engine::blockPeriodNanos (kSampleRate, kBlockSize);
    REQUIRE (period > 0u);

    const std::uint64_t small = period / 4u;

    // (a) Positive: 1000 healthy blocks pass, and p99.9 reports the healthy time.
    {
        const std::vector<std::uint64_t> healthy (1000u, small);
        const auto stats = summarizeDeadlineSoak (healthy, kSampleRate, kBlockSize, 1u, 0u);
        REQUIRE (stats.blocksProcessed == 1000u);
        REQUIRE (stats.p999BlockNanos == small);
        REQUIRE (stats.passesDeadline());
    }

    // (b) The 99.9th percentile tolerates exactly the top 0.1%: for n=1000 the p999 index is 998, so a
    //     single over-deadline outlier is intentionally ignored and the soak still passes.
    {
        std::vector<std::uint64_t> oneSpike (1000u, small);
        oneSpike.back() = period * 10u;
        const auto stats = summarizeDeadlineSoak (oneSpike, kSampleRate, kBlockSize, 1u, 0u);
        REQUIRE (stats.maxBlockNanos == period * 10u);
        REQUIRE (stats.p999BlockNanos == small);
        REQUIRE (stats.passesDeadline());   // a lone outlier does not trip the gate, by design
    }

    // (c) Two over-deadline blocks (> top 0.1%) push the p99.9 element over the period => fail.
    {
        std::vector<std::uint64_t> tooMany (1000u, small);
        tooMany[998] = period + 1u;
        tooMany[999] = period + 1u;
        const auto stats = summarizeDeadlineSoak (tooMany, kSampleRate, kBlockSize, 1u, 0u);
        REQUIRE (stats.p999BlockNanos == period + 1u);
        REQUIRE_FALSE (stats.passesDeadline());
    }

    // (d) Boundary: a p99.9 block exactly AT the period must fail (the predicate is strict <, not <=).
    {
        std::vector<std::uint64_t> atDeadline (1000u, small);
        atDeadline[998] = period;
        atDeadline[999] = period;
        const auto stats = summarizeDeadlineSoak (atDeadline, kSampleRate, kBlockSize, 1u, 0u);
        REQUIRE (stats.p999BlockNanos == period);
        REQUIRE_FALSE (stats.passesDeadline());
    }

    // (e) A reported underrun fails even when every block is under the period.
    {
        const std::vector<std::uint64_t> healthy (1000u, small);
        const auto stats = summarizeDeadlineSoak (healthy, kSampleRate, kBlockSize, 1u, 1u);
        REQUIRE (stats.p999BlockNanos < period);
        REQUIRE_FALSE (stats.passesDeadline());
    }

    // (f) Empty soak (no blocks processed) never passes.
    {
        const auto stats = summarizeDeadlineSoak (std::span<const std::uint64_t> {},
                                                  kSampleRate, kBlockSize, 1u, 0u);
        REQUIRE (stats.blocksProcessed == 0u);
        REQUIRE_FALSE (stats.passesDeadline());
    }
}

TEST_CASE ("H6 heavy session stays under the 128-frame block deadline for the configured audio-frame duration",
           "[h6][reliability][deadline]")
{
    constexpr int kBlockSize = 128;
    constexpr int kTrackCount = 100;
#if defined (YESDAW_SANITIZER_BUILD)
    constexpr std::uint64_t kSeconds = 10u;
#else
    constexpr std::uint64_t kSeconds = 60u * 60u;
#endif

    std::unique_ptr<CompiledGraph> graph = makeHeavySessionGraph (kTrackCount, kBlockSize);
    const DeadlineSoakStats stats = runDeadlineSoak (*graph, kBlockSize, kSeconds);

    REQUIRE (stats.virtualFrames == 48000u * kSeconds);
#if defined (YESDAW_SANITIZER_BUILD)
    REQUIRE (stats.blocksProcessed == yesdaw::engine::audioBlocksForDuration (48000.0, kBlockSize, kSeconds));
    REQUIRE (stats.underruns == 0u);
#else
    REQUIRE (stats.blocksProcessed == 1'350'000u);
    REQUIRE (stats.underruns == 0u);
    REQUIRE (stats.p999BlockNanos < stats.blockPeriodNanos);
    REQUIRE (stats.passesDeadline());
#endif
}

TEST_CASE ("H6 hard kill mid-edit restores the last autosave without corruption",
           "[h6][reliability][autosave]")
{
    const std::filesystem::path path = makeTempBundlePath ("autosave-recovery");
    const Project saved = makeProject();

    Project autosaved = saved;
    autosaved.clips[0].gain = 0.5f;
    autosaved.clips[1].timelineStart = 15360;

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (path, db).ok());
        REQUIRE (db.writeProjectSnapshot (saved).ok());
        writeProjectAssetFiles (path, saved);
        REQUIRE (writeAutosaveSnapshot (db, autosaved).ok());

        REQUIRE (db.executeSql ("BEGIN IMMEDIATE;").ok());
        REQUIRE (db.executeSql (
                     "DELETE FROM clips; DELETE FROM assets; DELETE FROM project; "
                     "INSERT INTO project(singleton_id, id, sample_rate_hz) "
                     "VALUES (1, X'000000000000000000000000000000FE', 96000.0);")
                     .ok());
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    std::string integrity;
    REQUIRE (reopened.queryText ("PRAGMA integrity_check;", integrity).ok());
    REQUIRE (integrity == "ok");

    Project rolledBack;
    REQUIRE (reopened.readProjectSnapshot (rolledBack).ok());
    requireSameProjectSurface (rolledBack, saved);

    const std::filesystem::path restoredAssetPath =
        path / yesdaw::persistence::detail::assetRelativePathForHash (autosaved.assets[0].contentHash);
    std::error_code removeError;
    REQUIRE (std::filesystem::remove (restoredAssetPath, removeError));
    REQUIRE (! removeError);

    Project recovered;
    REQUIRE (restoreAutosaveSnapshot (reopened, recovered).ok());
    requireSameProjectSurface (recovered, autosaved);

    reopened = {};

    ProjectBundleDb validated;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, validated).ok());
    REQUIRE (validated.queryText ("PRAGMA integrity_check;", integrity).ok());
    REQUIRE (integrity == "ok");
    REQUIRE (validated.validateStoredProjectSemantics().ok());

    Project readback;
    REQUIRE (validated.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, autosaved);
}
