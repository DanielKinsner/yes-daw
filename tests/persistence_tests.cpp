// YES DAW - headless checks for ADR-0012 SQLite bundle schema/migrations/intent log.

#include "persistence/ProjectBundle.h"
#include "persistence/WaveformPeakCache.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::AutomationBreakpoint;
using yesdaw::engine::AutomationCurveType;
using yesdaw::engine::AutomationLaneData;
using yesdaw::engine::AutomationTargetRole;
using yesdaw::engine::Clip;
using yesdaw::engine::EntityId;
using yesdaw::engine::FxInsert;
using yesdaw::engine::FxKind;
using yesdaw::engine::Marker;
using yesdaw::engine::MeterChange;
using yesdaw::engine::MidiClip;
using yesdaw::engine::moveClip;
using yesdaw::engine::Note;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::ProjectRecordingCompSegment;
using yesdaw::engine::RecordingMonitoringPolicy;
using yesdaw::engine::RecordingTake;
using yesdaw::engine::SampleRate;
using yesdaw::engine::setClipFades;
using yesdaw::engine::setClipGain;
using yesdaw::engine::splitClip;
using yesdaw::engine::Bus;
using yesdaw::engine::kDefaultAudioTrackId;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::trimClip;
using yesdaw::persistence::AssetImportRequest;
using yesdaw::persistence::BundleStatus;
using yesdaw::persistence::PendingFsOp;
using yesdaw::persistence::PendingFsOpKind;
using yesdaw::persistence::PluginBlacklistEntry;
using yesdaw::persistence::PluginStateChunkKind;
using yesdaw::persistence::PluginStateChunkRecord;
using yesdaw::persistence::PluginStateFormat;
using yesdaw::persistence::PluginStateRestoreChunk;
using yesdaw::persistence::PluginStateRestoreStatus;
using yesdaw::persistence::ProjectBundleDb;
using yesdaw::persistence::buildWaveformPeakCache;
using yesdaw::persistence::detail::SchemaMigration;
using yesdaw::persistence::kApplicationId;
using yesdaw::persistence::kBusyTimeoutMs;
using yesdaw::persistence::kCacheSizeKiB;
using yesdaw::persistence::kCodeSchemaVersion;
using yesdaw::persistence::kWalAutoCheckpointPages;
using yesdaw::persistence::detail::kSchemaV1Sql;
using yesdaw::persistence::detail::kSchemaV2Sql;
using yesdaw::persistence::detail::kSchemaV3Sql;
using yesdaw::persistence::detail::kSchemaV4Sql;
using yesdaw::persistence::detail::kSchemaV5Sql;
using yesdaw::persistence::detail::kSchemaV6Sql;
using yesdaw::persistence::detail::kSchemaV7Sql;
using yesdaw::persistence::detail::kSchemaV8Sql;
using Catch::Approx;

namespace {

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

AssetContentHash hashFromLowByte (std::uint8_t low) noexcept
{
    AssetContentHash hash;
    hash.bytes.back() = low;
    return hash;
}

std::filesystem::path makeTempBundlePath (std::string_view label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("yesdaw-" + std::string (label) + "-" + std::to_string (ticks) + ".yesdaw");

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

std::size_t countAudioAssetFiles (const std::filesystem::path& bundlePath)
{
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator (bundlePath / "audio"))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".asset")
            ++count;
    }

    return count;
}

std::string utf8Path (const std::filesystem::path& path)
{
    const auto utf8 = path.generic_u8string();
    return std::string (utf8.begin(), utf8.end());
}

std::string blobLiteral (std::span<const std::uint8_t> bytes)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string out = "X'";
    out.reserve (3u + bytes.size() * 2u);
    for (const std::uint8_t byte : bytes)
    {
        out.push_back (digits[(byte >> 4u) & 0x0Fu]);
        out.push_back (digits[byte & 0x0Fu]);
    }
    out.push_back ('\'');
    return out;
}

std::string blobLiteral (EntityId id)
{
    return blobLiteral (std::span<const std::uint8_t> (id.bytes.data(), id.bytes.size()));
}

std::string blobLiteral (AssetContentHash hash)
{
    return blobLiteral (std::span<const std::uint8_t> (hash.bytes.data(), hash.bytes.size()));
}

void requireRawExec (sqlite3* db, std::string_view sql)
{
    char* rawError = nullptr;
    const std::string command (sql);
    const int rc = sqlite3_exec (db, command.c_str(), nullptr, nullptr, &rawError);
    const std::string message = rawError == nullptr ? sqlite3_errmsg (db) : rawError;
    sqlite3_free (rawError);

    INFO (message);
    REQUIRE (rc == SQLITE_OK);
}

std::vector<std::uint8_t> readRawPluginStateBytes (const std::filesystem::path& bundlePath,
                                                   EntityId nodeId,
                                                   PluginStateChunkKind kind)
{
    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (bundlePath / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);

    sqlite3_stmt* stmt = nullptr;
    REQUIRE (sqlite3_prepare_v2 (
                 rawDb,
                 "SELECT data FROM plugin_state_chunks WHERE node_id = ? AND chunk_kind = ?;",
                 -1,
                 &stmt,
                 nullptr)
             == SQLITE_OK);
    REQUIRE (sqlite3_bind_blob (stmt,
                                1,
                                nodeId.bytes.data(),
                                static_cast<int> (nodeId.bytes.size()),
                                SQLITE_TRANSIENT)
             == SQLITE_OK);
    REQUIRE (sqlite3_bind_int64 (stmt, 2, static_cast<sqlite3_int64> (kind)) == SQLITE_OK);
    REQUIRE (sqlite3_step (stmt) == SQLITE_ROW);

    const int bytes = sqlite3_column_bytes (stmt, 0);
    const void* raw = sqlite3_column_blob (stmt, 0);
    std::vector<std::uint8_t> out (static_cast<std::size_t> (bytes));
    if (bytes > 0)
    {
        REQUIRE (raw != nullptr);
        const auto* data = static_cast<const std::uint8_t*> (raw);
        std::copy (data, data + bytes, out.begin());
    }

    REQUIRE (sqlite3_finalize (stmt) == SQLITE_OK);
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);
    return out;
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

Track makeTrack (EntityId id, std::string name = "Audio 1")
{
    Track track;
    track.id = id;
    track.strip.name = std::move (name);
    return track;
}

Bus makeBus (EntityId id, std::string name = "Verb")
{
    Bus bus;
    bus.id = id;
    bus.strip.name = std::move (name);
    return bus;
}

Clip makeClip (EntityId id, EntityId assetId, EntityId trackId, std::uint64_t srcOffset, std::uint64_t srcLen)
{
    Clip clip;
    clip.id = id;
    clip.assetId = assetId;
    clip.trackId = trackId;
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

Note makeNote (EntityId id, Tick start, Tick length, std::int16_t key = 60)
{
    Note note;
    note.id = id;
    note.startTick = start;
    note.lengthTicks = length;
    note.key = key;
    note.pitchNote = static_cast<double> (key) + 0.25;
    note.normalizedVelocity = 0.75;
    note.portIndex = 1;
    note.channel = 2;
    return note;
}

MidiClip makeMidiClip (EntityId id, EntityId trackId)
{
    MidiClip midiClip;
    midiClip.id = id;
    midiClip.trackId = trackId;
    midiClip.timelineStart = 7680;
    midiClip.timelineLength = 15360 * 4;
    midiClip.timeBase = TimeBase::TempoLocked;
    midiClip.notes = {
        makeNote (idFromLowByte (72), 0, 15360, 60),
        makeNote (idFromLowByte (73), 15360, 7680, 67),
        makeNote (idFromLowByte (74), midiClip.timelineLength, 0, 72),
    };
    return midiClip;
}

FxInsert makeFxInsert (EntityId id, FxKind kind = FxKind::Eq, bool enabled = true)
{
    FxInsert insert;
    insert.id = id;
    insert.kind = kind;
    insert.enabled = enabled;
    insert.normalizedParams = {
        { 10u, 0.125 },
        { 11u, 0.875 },
    };
    return insert;
}

AutomationLaneData makeAutomationLane (EntityId id,
                                       EntityId ownerEntity,
                                       AutomationTargetRole role,
                                       std::uint32_t paramId)
{
    AutomationLaneData lane;
    lane.id = id;
    lane.ownerEntity = ownerEntity;
    lane.role = role;
    lane.paramId = paramId;
    lane.points = {
        AutomationBreakpoint { 0,     0.25, AutomationCurveType::Linear },
        AutomationBreakpoint { 15360, 0.75, AutomationCurveType::Hold },
    };
    return lane;
}

RecordingTake makeRecordingTake (EntityId id,
                                 EntityId assetId,
                                 EntityId trackId,
                                 EntityId clipId,
                                 Tick timelineStart,
                                 std::uint64_t frameCount)
{
    RecordingTake take;
    take.id = id;
    take.assetId = assetId;
    take.trackId = trackId;
    take.clipId = clipId;
    take.timelineStart = timelineStart;
    take.frameCount = frameCount;
    take.takeOrdinal = 3;
    take.inputChannel = 1;
    take.deviceStableId = 42;
    take.monitoringPolicy = RecordingMonitoringPolicy::DirectInput;
    return take;
}

ProjectRecordingCompSegment makeProjectRecordingCompSegment (EntityId id,
                                                             EntityId takeId,
                                                             Tick timelineStart,
                                                             Tick timelineLength,
                                                             std::uint64_t sourceOffset)
{
    ProjectRecordingCompSegment segment;
    segment.id = id;
    segment.takeId = takeId;
    segment.timelineStart = timelineStart;
    segment.timelineLength = timelineLength;
    segment.sourceOffset = sourceOffset;
    return segment;
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
    project.tracks = {
        makeTrack (idFromLowByte (10), "Audio 1"),
    };
    project.clips = {
        makeClip (idFromLowByte (4), project.assets[0].id, project.tracks[0].id, 100, 900),
        makeClip (idFromLowByte (5), project.assets[1].id, project.tracks[0].id, 0, 128),
    };
    return project;
}

ProjectBundleDb openFreshBundle (const std::filesystem::path& path)
{
    ProjectBundleDb db;
    const auto result = ProjectBundleDb::openOrCreateBundle (path, db);
    REQUIRE (result.ok());
    return db;
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
    REQUIRE (actual.tracks == expected.tracks);
    REQUIRE (actual.buses == expected.buses);
    REQUIRE (actual.clips == expected.clips);
    REQUIRE (actual.midiClips == expected.midiClips);
    REQUIRE (actual.recordingTakes == expected.recordingTakes);
    REQUIRE (actual.recordingCompSegments == expected.recordingCompSegments);
    REQUIRE (actual.automationLanes == expected.automationLanes);
    REQUIRE (actual.hasValidAssetClipIndirection());
}

} // namespace

TEST_CASE ("SQLite bundle bring-up applies ADR-0012 pragmas and schema identity", "[persistence][sqlite][schema]")
{
    const auto path = makeTempBundlePath ("bringup");
    ProjectBundleDb db = openFreshBundle (path);

    REQUIRE (std::filesystem::exists (path / "project.db"));
    REQUIRE (std::filesystem::is_directory (path / "audio"));
    REQUIRE (std::filesystem::is_directory (path / "peaks"));
    REQUIRE (std::filesystem::is_directory (path / "plugins"));
    REQUIRE (std::filesystem::is_directory (path / "autosave"));
    REQUIRE (std::filesystem::is_directory (path / ".trash"));

    sqlite3_int64 value = 0;
    REQUIRE (db.queryInt64 ("PRAGMA application_id;", value).ok());
    REQUIRE (value == kApplicationId);
    REQUIRE (db.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (db.queryInt64 ("PRAGMA foreign_keys;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("PRAGMA busy_timeout;", value).ok());
    REQUIRE (value == kBusyTimeoutMs);
    REQUIRE (db.queryInt64 ("PRAGMA wal_autocheckpoint;", value).ok());
    REQUIRE (value == kWalAutoCheckpointPages);
    REQUIRE (db.queryInt64 ("PRAGMA cache_size;", value).ok());
    REQUIRE (value == kCacheSizeKiB);
    REQUIRE (db.queryInt64 ("PRAGMA temp_store;", value).ok());
    REQUIRE (value == 2);

    std::string journalMode;
    REQUIRE (db.queryText ("PRAGMA journal_mode;", journalMode).ok());
    REQUIRE (journalMode == "wal");
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 1;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 2;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 3;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 4;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 5;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 6;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 7;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 8;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'pending_fs_ops';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'plugin_state_chunks';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'plugin_blacklist';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'midi_clips';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'midi_notes';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'tracks';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'buses';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'recording_takes';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'recording_comp_segments';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'fx_inserts';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'fx_insert_params';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'automation_lanes';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'automation_breakpoints';", value).ok());
    REQUIRE (value == 1);
}

TEST_CASE ("Migration harness refuses forward schema and rolls back failed migrations", "[persistence][migration]")
{
    const auto path = makeTempBundlePath ("forward");
    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.executeSql ("PRAGMA user_version = " + std::to_string (kCodeSchemaVersion + 1) + ";").ok());
    }

    ProjectBundleDb reopened;
    const auto forward = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (forward.status == BundleStatus::ForwardSchema);
    REQUIRE (forward.userVersion == kCodeSchemaVersion + 1);

    sqlite3* memoryDb = nullptr;
    REQUIRE (sqlite3_open (":memory:", &memoryDb) == SQLITE_OK);
    const SchemaMigration badMigration { 1, "CREATE TABLE ok_table(id INTEGER PRIMARY KEY); INSERT INTO missing_table VALUES (1);" };
    const auto failed = ProjectBundleDb::runMigrationsForTest (memoryDb, 0, std::span<const SchemaMigration> (&badMigration, 1));
    REQUIRE (failed.status == BundleStatus::MigrationFailed);

    sqlite3_stmt* stmt = nullptr;
    REQUIRE (sqlite3_prepare_v2 (memoryDb, "SELECT COUNT(*) FROM sqlite_schema WHERE name = 'ok_table';", -1, &stmt, nullptr) == SQLITE_OK);
    REQUIRE (sqlite3_step (stmt) == SQLITE_ROW);
    REQUIRE (sqlite3_column_int64 (stmt, 0) == 0);
    sqlite3_finalize (stmt);

    REQUIRE (sqlite3_prepare_v2 (memoryDb, "PRAGMA user_version;", -1, &stmt, nullptr) == SQLITE_OK);
    REQUIRE (sqlite3_step (stmt) == SQLITE_ROW);
    REQUIRE (sqlite3_column_int64 (stmt, 0) == 0);
    sqlite3_finalize (stmt);
    sqlite3_close (memoryDb);
}

TEST_CASE ("Schema v1 enforces Clip to Asset foreign keys", "[persistence][foreign-key]")
{
    const auto path = makeTempBundlePath ("fk");
    ProjectBundleDb db = openFreshBundle (path);

    const Project project = makeProject();
    REQUIRE (db.writeProjectSnapshot (project).ok());

    const auto deleteReferencedAsset = db.executeSql ("DELETE FROM assets WHERE id = X'00000000000000000000000000000002';");
    REQUIRE ((deleteReferencedAsset.sqliteCode == SQLITE_CONSTRAINT || deleteReferencedAsset.sqliteCode == SQLITE_CONSTRAINT_FOREIGNKEY));

    const auto insertOrphan = db.executeSql (
        "INSERT INTO clips(id, asset_id, track_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base) "
        "VALUES (X'000000000000000000000000000000EE', X'000000000000000000000000000000EF', X'0000000000000000000000000000000A', 0, 1, 0, 1, 1.0, 0, 0, 1);");
    REQUIRE ((insertOrphan.sqliteCode == SQLITE_CONSTRAINT || insertOrphan.sqliteCode == SQLITE_CONSTRAINT_FOREIGNKEY));

    const auto insertOrphanTrack = db.executeSql (
        "INSERT INTO clips(id, asset_id, track_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base) "
        "VALUES (X'000000000000000000000000000000ED', X'00000000000000000000000000000002', X'000000000000000000000000000000EF', 0, 1, 0, 1, 1.0, 0, 0, 1);");
    REQUIRE ((insertOrphanTrack.sqliteCode == SQLITE_CONSTRAINT || insertOrphanTrack.sqliteCode == SQLITE_CONSTRAINT_FOREIGNKEY));
}

TEST_CASE ("Project value types persist only when schema v1 semantics hold", "[persistence][project]")
{
    const auto path = makeTempBundlePath ("project");
    ProjectBundleDb db = openFreshBundle (path);

    Project project = makeProject();
    REQUIRE (db.writeProjectSnapshot (project).ok());

    sqlite3_int64 value = 0;
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM assets;", value).ok());
    REQUIRE (value == 2);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM clips;", value).ok());
    REQUIRE (value == 2);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM tracks;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.validateStoredProjectSemantics().ok());

    Project orphan = project;
    orphan.clips[0].assetId = idFromLowByte (99);
    REQUIRE (db.writeProjectSnapshot (orphan).status == BundleStatus::SemanticInvalid);

    Project invalidGain = project;
    invalidGain.clips[0].gain = -0.25f;
    REQUIRE (db.writeProjectSnapshot (invalidGain).status == BundleStatus::SemanticInvalid);

    Project orphanTrack = project;
    orphanTrack.clips[0].trackId = idFromLowByte (99);
    REQUIRE (db.writeProjectSnapshot (orphanTrack).status == BundleStatus::SemanticInvalid);

    Project invalidStrip = project;
    invalidStrip.tracks[0].strip.pan = 1.25f;
    REQUIRE (db.writeProjectSnapshot (invalidStrip).status == BundleStatus::SemanticInvalid);

    Project invalidFxKind = project;
    invalidFxKind.tracks[0].strip.fxChain = { makeFxInsert (idFromLowByte (90), static_cast<FxKind> (99)) };
    REQUIRE (db.writeProjectSnapshot (invalidFxKind).status == BundleStatus::SemanticInvalid);

    Project invalidFxParam = project;
    invalidFxParam.tracks[0].strip.fxChain = { makeFxInsert (idFromLowByte (90), FxKind::Eq) };
    invalidFxParam.tracks[0].strip.fxChain.front().normalizedParams.front().second = 1.25;
    REQUIRE (db.writeProjectSnapshot (invalidFxParam).status == BundleStatus::SemanticInvalid);

    Project invalidAutomation = project;
    invalidAutomation.automationLanes = {
        makeAutomationLane (idFromLowByte (70), project.tracks[0].id, AutomationTargetRole::TrackFader, 1),
    };
    invalidAutomation.automationLanes.front().points.front().curveType = AutomationCurveType::Bezier;
    REQUIRE (db.writeProjectSnapshot (invalidAutomation).status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Project value surface round-trips through a reopened bundle", "[persistence][project][round-trip]")
{
    const auto path = makeTempBundlePath ("round-trip");

    Project project = makeProject();
    project.assets[1].sampleRate = SampleRate { 44100.0 };
    project.assets[1].channels = 1;
    project.clips[0].timelineStart = 3840;
    project.clips[0].timelineLength = 30720;
    project.clips[1].timelineStart = 15360 * 9;
    project.clips[1].timelineLength = 15360 * 2;
    project.clips[1].gain = 1.25f;
    project.clips[1].fadeIn = 0;
    project.clips[1].fadeOut = 96;
    project.clips[1].timeBase = TimeBase::TempoLocked;
    project.tracks[0].strip.name = "Vocal";
    project.tracks[0].strip.linearGain = 0.5f;
    project.tracks[0].strip.pan = -0.25f;
    project.tracks[0].strip.muted = true;
    project.tracks[0].strip.soloed = true;
    project.tracks[0].strip.soloSafe = false;
    project.tracks[0].strip.fxChain = {
        makeFxInsert (idFromLowByte (90), FxKind::Eq),
        makeFxInsert (idFromLowByte (91), FxKind::Compressor, false),
    };
    project.tracks[0].strip.fxChain[0].normalizedParams.push_back ({ 99u, 0.5 });
    project.buses = { makeBus (idFromLowByte (11), "Delay") };
    project.buses[0].strip.linearGain = 0.75f;
    project.buses[0].strip.pan = 0.4f;
    project.buses[0].strip.soloSafe = true;
    project.buses[0].strip.fxChain = {
        makeFxInsert (idFromLowByte (92), FxKind::Delay),
    };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
    REQUIRE (readback.tracks[0].strip.name == "Vocal");
    REQUIRE (readback.tracks[0].strip.linearGain == 0.5f);
    REQUIRE (readback.tracks[0].strip.pan == -0.25f);
    REQUIRE (readback.tracks[0].strip.muted);
    REQUIRE (readback.tracks[0].strip.soloed);
    REQUIRE_FALSE (readback.tracks[0].strip.soloSafe);
    REQUIRE (readback.tracks[0].strip.fxChain == project.tracks[0].strip.fxChain);
    REQUIRE (readback.buses[0].strip.name == "Delay");
    REQUIRE (readback.buses[0].strip.linearGain == 0.75f);
    REQUIRE (readback.buses[0].strip.pan == 0.4f);
    REQUIRE (readback.buses[0].strip.soloSafe);
    REQUIRE (readback.buses[0].strip.fxChain == project.buses[0].strip.fxChain);

    Project mutatedStrip = project;
    mutatedStrip.tracks[0].strip.pan = 0.25f;
    REQUIRE_FALSE (readback.tracks == mutatedStrip.tracks);
}

TEST_CASE ("Project automation lanes round-trip through schema v8", "[persistence][project][round-trip][automation][h15]")
{
    const auto path = makeTempBundlePath ("automation-round-trip");

    Project project = makeProject();
    project.buses = { makeBus (idFromLowByte (11), "Return") };
    project.tracks[0].strip.fxChain = { makeFxInsert (idFromLowByte (90), FxKind::Eq) };
    project.automationLanes = {
        makeAutomationLane (idFromLowByte (70), project.tracks[0].id, AutomationTargetRole::TrackFader, 1),
        makeAutomationLane (idFromLowByte (71), project.tracks[0].id, AutomationTargetRole::TrackPan, 1),
        makeAutomationLane (idFromLowByte (72), project.buses[0].id, AutomationTargetRole::BusFader, 1),
        makeAutomationLane (idFromLowByte (73), project.tracks[0].strip.fxChain[0].id, AutomationTargetRole::FxInsertParam, 2),
    };
    project.automationLanes[0].points.push_back (AutomationBreakpoint { 30720, 0.5, AutomationCurveType::Linear });
    REQUIRE (project.hasValidAssetClipIndirection());

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);

        sqlite3_int64 count = 0;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM automation_lanes;", count).ok());
        REQUIRE (count == 4);
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM automation_breakpoints;", count).ok());
        REQUIRE (count == 9);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
    REQUIRE (readback.automationLanes == project.automationLanes);

    Project mutatedLane = project;
    mutatedLane.automationLanes[0].points[1].value = 0.625;
    REQUIRE_FALSE (readback.automationLanes == mutatedLane.automationLanes);
}

TEST_CASE ("Project tempo map, meter map, and markers round-trip through a reopened bundle",
           "[persistence][project][round-trip][time]")
{
    const auto path = makeTempBundlePath ("time-round-trip");

    Project project = makeProject();
    project.tempoMap = {
        TempoChange { 0,     120.0, TempoCurve::Jump },
        TempoChange { 15360, 140.0, TempoCurve::LinearRamp },
        TempoChange { 61440, 90.5,  TempoCurve::Jump },
    };
    project.meterMap = {
        MeterChange { 0,     4, 4 },
        MeterChange { 61440, 7, 8 },
    };
    project.markers = {
        Marker { idFromLowByte (40), 0,     "Intro" },
        Marker { idFromLowByte (41), 15360, "Verse 1" },
        Marker { idFromLowByte (42), 61440, "" },         // an empty name is valid (NOT NULL, may be empty)
    };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());

    // The clips/assets surface still round-trips, AND the tempo/meter/marker surface comes back intact.
    requireSameProjectSurface (readback, project);
    REQUIRE (readback.tempoMap == project.tempoMap);
    REQUIRE (readback.meterMap == project.meterMap);
    REQUIRE (readback.markers == project.markers);

    // Negative controls: a single tweaked value in each map must break equality, proving the round-trip
    // actually carries the stored data rather than re-deriving defaults.
    REQUIRE_FALSE (readback.tempoMap.empty());
    Project mutatedTempo = project;
    mutatedTempo.tempoMap[1].bpm = 141.0;
    REQUIRE_FALSE (readback.tempoMap == mutatedTempo.tempoMap);
    Project mutatedMeter = project;
    mutatedMeter.meterMap[1].denominator = 4;
    REQUIRE_FALSE (readback.meterMap == mutatedMeter.meterMap);
    Project mutatedMarker = project;
    mutatedMarker.markers[1].name = "Verse 2";
    REQUIRE_FALSE (readback.markers == mutatedMarker.markers);

    // An invalid tempo (bpm <= 0) is rejected before write — the write-side schema-v1 guard covers the
    // new time surface too.
    Project invalidTempo = project;
    invalidTempo.tempoMap.push_back (TempoChange { 80000, 0.0, TempoCurve::Jump });
    ProjectBundleDb invalidDb = openFreshBundle (makeTempBundlePath ("time-invalid"));
    REQUIRE (invalidDb.writeProjectSnapshot (invalidTempo).status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Project MIDI Clips and Notes round-trip through a reopened bundle",
           "[persistence][project][round-trip][midi]")
{
    const auto path = makeTempBundlePath ("midi-round-trip");

    Project project = makeProject();
    project.tracks.push_back (makeTrack (idFromLowByte (71), "MIDI Track"));
    project.midiClips = { makeMidiClip (idFromLowByte (70), idFromLowByte (71)) };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);

        sqlite3_int64 count = 0;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM midi_clips;", count).ok());
        REQUIRE (count == 1);
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM midi_notes;", count).ok());
        REQUIRE (count == 3);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
    REQUIRE (readback.midiClips.size() == 1u);
    REQUIRE (readback.midiClips[0].trackId == idFromLowByte (71));
    REQUIRE (readback.midiClips[0].notes[1].pitchNote == Approx (67.25));
    REQUIRE (readback.midiClips[0].notes[2].lengthTicks == 0);

    Project mutatedNote = project;
    mutatedNote.midiClips[0].notes[1].normalizedVelocity = 0.5;
    REQUIRE_FALSE (readback.midiClips == mutatedNote.midiClips);

    Project noteExtendsPastClip = project;
    noteExtendsPastClip.midiClips[0].notes[0].startTick = noteExtendsPastClip.midiClips[0].timelineLength;
    noteExtendsPastClip.midiClips[0].notes[0].lengthTicks = 1;
    ProjectBundleDb invalidDb = openFreshBundle (makeTempBundlePath ("midi-invalid"));
    REQUIRE (invalidDb.writeProjectSnapshot (noteExtendsPastClip).status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Project recording Takes round-trip through a reopened bundle",
           "[persistence][project][round-trip][recording]")
{
    const auto path = makeTempBundlePath ("recording-take-round-trip");

    Project project = makeProject();
    project.clips[0].timelineLength = static_cast<Tick> (project.clips[0].srcLen);
    project.clips[1].timelineStart = 15360;
    project.clips[1].timelineLength = static_cast<Tick> (project.clips[1].srcLen);
    project.recordingTakes = {
        makeRecordingTake (idFromLowByte (80),
                           project.assets[1].id,
                           project.tracks[0].id,
                           project.clips[1].id,
                           project.clips[1].timelineStart,
                           project.clips[1].srcLen),
        makeRecordingTake (idFromLowByte (81),
                           project.assets[0].id,
                           project.tracks[0].id,
                           project.clips[0].id,
                           project.clips[0].timelineStart,
                           project.clips[0].srcLen),
    };
    project.recordingCompSegments = {
        makeProjectRecordingCompSegment (idFromLowByte (82), project.recordingTakes[0].id, 0, 64, 0),
        makeProjectRecordingCompSegment (idFromLowByte (83), project.recordingTakes[1].id, 128, 96, 32),
    };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);

        sqlite3_int64 count = 0;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM recording_takes;", count).ok());
        REQUIRE (count == 2);
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM recording_comp_segments;", count).ok());
        REQUIRE (count == 2);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
    REQUIRE (readback.recordingTakes[0].takeOrdinal == 3u);
    REQUIRE (readback.recordingTakes[0].inputChannel == 1u);
    REQUIRE (readback.recordingTakes[0].deviceStableId == 42u);
    REQUIRE (readback.recordingTakes[0].monitoringPolicy == RecordingMonitoringPolicy::DirectInput);
    REQUIRE (readback.recordingCompSegments[0].takeId == project.recordingTakes[0].id);
    REQUIRE (readback.recordingCompSegments[0].timelineLength == 64);
    REQUIRE (readback.recordingCompSegments[1].takeId == project.recordingTakes[1].id);
    REQUIRE (readback.recordingCompSegments[1].sourceOffset == 32u);

    Project mismatchedClip = project;
    mismatchedClip.recordingTakes[0].frameCount = project.assets[1].frames + 1u;
    ProjectBundleDb invalidDb = openFreshBundle (makeTempBundlePath ("recording-take-invalid"));
    REQUIRE (invalidDb.writeProjectSnapshot (mismatchedClip).status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Project clip edit metadata round-trips through a reopened bundle", "[persistence][project][clip-edit]")
{
    const auto path = makeTempBundlePath ("clip-edit-round-trip");

    Project project = makeProject();
    const EntityId splitTarget = project.clips[0].id;
    const EntityId trimmedTarget = project.clips[1].id;
    const EntityId rightId = idFromLowByte (6);

    REQUIRE (splitClip (project, splitTarget, rightId, 4096, 333) == ProjectEditStatus::Applied);
    REQUIRE (moveClip (project, trimmedTarget, 64'000) == ProjectEditStatus::Applied);
    REQUIRE (trimClip (project, trimmedTarget, 64'000, 2048, 12, 64) == ProjectEditStatus::Applied);
    REQUIRE (setClipGain (project, splitTarget, 1.125f) == ProjectEditStatus::Applied);
    REQUIRE (setClipFades (project, splitTarget, 240, 480) == ProjectEditStatus::Applied);
    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (project.clips.size() == 3u);
    REQUIRE (project.clips[0].gain == 1.125f);
    REQUIRE (project.clips[0].fadeIn == 240);
    REQUIRE (project.clips[0].fadeOut == 480);
    REQUIRE (project.clips[1].id == rightId);
    REQUIRE (project.clips[1].srcOffset == project.clips[0].srcOffset + project.clips[0].srcLen);

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
    REQUIRE (readback.clips[0].gain == 1.125f);
    REQUIRE (readback.clips[0].fadeIn == 240);
    REQUIRE (readback.clips[0].fadeOut == 480);
    REQUIRE (readback.clips[1].srcOffset == readback.clips[0].srcOffset + readback.clips[0].srcLen);
}

TEST_CASE ("Interrupted save transaction reopens the last committed Project", "[persistence][recovery][save]")
{
    const auto path = makeTempBundlePath ("save-recovery");
    const Project committed = makeProject();

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (committed).ok());
        writeProjectAssetFiles (path, committed);
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

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, committed);
}

TEST_CASE ("Interrupted schema migration reruns cleanly on reopen", "[persistence][recovery][migration]")
{
    const auto path = makeTempBundlePath ("migration-recovery");

    std::error_code ec;
    std::filesystem::create_directories (path, ec);
    REQUIRE (! ec);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    requireRawExec (rawDb, "BEGIN IMMEDIATE;");
    requireRawExec (rawDb, kSchemaV1Sql);
    requireRawExec (rawDb, "INSERT INTO schema_migrations(version, app_build) VALUES (1, 'interrupted');");
    requireRawExec (rawDb, "PRAGMA application_id = 1497715505;");
    requireRawExec (rawDb, "PRAGMA user_version = 1;");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openOrCreateBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA application_id;", value).ok());
    REQUIRE (value == kApplicationId);
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 1;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 2;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 3;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 4;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 1 AND app_build = 'interrupted';", value).ok());
    REQUIRE (value == 0);

    std::string integrity;
    REQUIRE (reopened.queryText ("PRAGMA integrity_check;", integrity).ok());
    REQUIRE (integrity == "ok");
    REQUIRE (reopened.validateStoredProjectSemantics().ok());
}

TEST_CASE ("Schema v4 migration promotes old Clip and MIDI track ownership", "[persistence][migration][track-bus]")
{
    const auto path = makeTempBundlePath ("track-bus-migration");

    std::error_code ec;
    std::filesystem::create_directories (path / "audio", ec);
    REQUIRE (! ec);

    const EntityId projectId = idFromLowByte (1);
    const Asset asset = makeAsset (idFromLowByte (2), 1000);
    const EntityId clipId = idFromLowByte (4);
    const EntityId midiClipId = idFromLowByte (70);
    const EntityId midiTrackId = idFromLowByte (71);

    const std::vector<std::uint8_t> assetBytes = assetBytesForId (asset.id);
    writeBytes (path / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash),
                std::span<const std::uint8_t> (assetBytes.data(), assetBytes.size()));

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    requireRawExec (rawDb, kSchemaV1Sql);
    requireRawExec (rawDb, kSchemaV2Sql);
    requireRawExec (rawDb, kSchemaV3Sql);
    requireRawExec (rawDb, "INSERT INTO schema_migrations(version, app_build) VALUES (1, 'legacy'), (2, 'legacy'), (3, 'legacy');");
    requireRawExec (rawDb, "PRAGMA application_id = 1497715505;");
    requireRawExec (rawDb, "PRAGMA user_version = 3;");
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) VALUES (1, " + blobLiteral (projectId) + ", 48000.0);");
    requireRawExec (
        rawDb,
        "INSERT INTO assets(id, content_hash, frames, sample_rate_hz, channels, relative_path) VALUES ("
        + blobLiteral (asset.id) + ", " + blobLiteral (asset.contentHash) + ", 1000, 48000.0, 2, '"
        + yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash) + "');");
    requireRawExec (
        rawDb,
        "INSERT INTO clips(id, asset_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base) VALUES ("
        + blobLiteral (clipId) + ", " + blobLiteral (asset.id) + ", 0, 15360, 100, 900, 0.75, 16, 32, 1);");
    requireRawExec (
        rawDb,
        "INSERT INTO midi_clips(id, track_id, timeline_start, timeline_length, time_base) VALUES ("
        + blobLiteral (midiClipId) + ", " + blobLiteral (midiTrackId) + ", 7680, 61440, 0);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM tracks;", value).ok());
    REQUIRE (value == 2);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM clips WHERE track_id = X'5945534441575F415544494F5F303031';", value).ok());
    REQUIRE (value == 1);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.tracks.size() == 2u);
    REQUIRE (readback.findTrack (kDefaultAudioTrackId) != nullptr);
    REQUIRE (readback.findTrack (midiTrackId) != nullptr);
    REQUIRE (readback.clips.size() == 1u);
    REQUIRE (readback.clips[0].trackId == kDefaultAudioTrackId);
    REQUIRE (readback.midiClips.size() == 1u);
    REQUIRE (readback.midiClips[0].trackId == midiTrackId);
    REQUIRE (readback.hasValidAssetClipIndirection());
}

TEST_CASE ("Schema v7 migration adds empty FX chains to a v6 bundle", "[persistence][migration][fx]")
{
    const auto path = makeTempBundlePath ("fx-v6-migration");

    std::error_code ec;
    std::filesystem::create_directories (path, ec);
    REQUIRE (! ec);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV6 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 6);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV6).ok());
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) "
        "VALUES (1, X'00000000000000000000000000000001', 48000.0);");
    requireRawExec (
        rawDb,
        "INSERT INTO tracks(id, name, linear_gain, pan, muted, soloed, solo_safe) "
        "VALUES (X'0000000000000000000000000000000A', 'Audio 1', 1.0, 0.0, 0, 0, 0);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 7;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'fx_inserts';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'fx_insert_params';", value).ok());
    REQUIRE (value == 1);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.tracks.size() == 1u);
    REQUIRE (readback.tracks.front().strip.fxChain.empty());
    REQUIRE (readback.hasValidAssetClipIndirection());
}

TEST_CASE ("Schema v8 migration adds empty automation lane tables to a v7 bundle", "[persistence][migration][automation][h15]")
{
    const auto path = makeTempBundlePath ("automation-v7-migration");

    std::error_code ec;
    std::filesystem::create_directories (path, ec);
    REQUIRE (! ec);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV7 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 7);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV7).ok());
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) "
        "VALUES (1, X'00000000000000000000000000000001', 48000.0);");
    requireRawExec (
        rawDb,
        "INSERT INTO tracks(id, name, linear_gain, pan, muted, soloed, solo_safe) "
        "VALUES (X'0000000000000000000000000000000A', 'Audio 1', 1.0, 0.0, 0, 0, 0);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 8;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'automation_lanes';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'automation_breakpoints';", value).ok());
    REQUIRE (value == 1);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.automationLanes.empty());
    REQUIRE (readback.hasValidAssetClipIndirection());
}

TEST_CASE ("Layered semantic validation catches DB rows that SQLite integrity checks cannot", "[persistence][semantic]")
{
    const auto path = makeTempBundlePath ("semantic");
    ProjectBundleDb db = openFreshBundle (path);

    REQUIRE (db.writeProjectSnapshot (makeProject()).ok());
    REQUIRE (db.executeSql ("UPDATE clips SET src_len = 901 WHERE id = X'00000000000000000000000000000004';").ok());

    const auto validation = db.validateStoredProjectSemantics();
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Opening an existing bundle runs layered semantic validation", "[persistence][semantic][open]")
{
    const auto path = makeTempBundlePath ("semantic-open");

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (makeProject()).ok());
        writeProjectAssetFiles (path, makeProject());
        REQUIRE (db.executeSql ("UPDATE clips SET src_len = 901 WHERE id = X'00000000000000000000000000000004';").ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE ((validation.status == BundleStatus::SemanticInvalid || validation.status == BundleStatus::IntegrityFailed));
}

TEST_CASE ("Opening an existing bundle rejects MIDI Notes outside their Clip", "[persistence][semantic][open][midi]")
{
    const auto path = makeTempBundlePath ("semantic-midi-open");
    Project project = makeProject();
    project.tracks.push_back (makeTrack (idFromLowByte (71), "MIDI Track"));
    project.midiClips = { makeMidiClip (idFromLowByte (70), idFromLowByte (71)) };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.executeSql (
                    "UPDATE midi_notes SET start_tick = 999999 WHERE id = X'00000000000000000000000000000048';")
                     .ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE ((validation.status == BundleStatus::SemanticInvalid || validation.status == BundleStatus::IntegrityFailed));
}

TEST_CASE ("Opening an existing bundle rejects recording Takes that no longer match their Clip",
           "[persistence][semantic][open][recording]")
{
    const auto path = makeTempBundlePath ("semantic-recording-open");
    Project project = makeProject();
    project.recordingTakes = {
        makeRecordingTake (idFromLowByte (80),
                           project.assets[1].id,
                           project.tracks[0].id,
                           project.clips[1].id,
                           project.clips[1].timelineStart,
                           project.clips[1].srcLen),
    };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.executeSql (
                    "UPDATE recording_takes SET frame_count = 999999 "
                    "WHERE id = X'00000000000000000000000000000050';")
                     .ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Opening an existing bundle rejects recording Comp segments outside their Take",
           "[persistence][semantic][open][recording][comp]")
{
    const auto path = makeTempBundlePath ("semantic-recording-comp-open");
    Project project = makeProject();
    project.recordingTakes = {
        makeRecordingTake (idFromLowByte (80),
                           project.assets[1].id,
                           project.tracks[0].id,
                           project.clips[1].id,
                           project.clips[1].timelineStart,
                           project.clips[1].srcLen),
    };
    project.recordingCompSegments = {
        makeProjectRecordingCompSegment (idFromLowByte (81), project.recordingTakes[0].id, 0, 64, 0),
    };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.executeSql (
                    "UPDATE recording_comp_segments SET source_offset = 128 "
                    "WHERE id = X'00000000000000000000000000000051';")
                     .ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Opening an existing bundle rejects orphan Clip track references", "[persistence][semantic][open][track]")
{
    const auto path = makeTempBundlePath ("semantic-track-open");
    Project project = makeProject();
    project.tracks.push_back (makeTrack (idFromLowByte (71), "MIDI Track"));
    project.midiClips = { makeMidiClip (idFromLowByte (70), idFromLowByte (71)) };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.executeSql ("UPDATE midi_clips SET track_id = X'000000000000000000000000000000AA';").ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Opening an existing bundle rejects invalid Track and Bus strip ranges", "[persistence][semantic][open][track-bus]")
{
    const auto path = makeTempBundlePath ("semantic-strip-open");
    Project project = makeProject();
    project.buses = { makeBus (idFromLowByte (11), "Bus 1") };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.executeSql (
                    "PRAGMA ignore_check_constraints = ON; "
                    "UPDATE tracks SET pan = 1.25 WHERE id = X'0000000000000000000000000000000A'; "
                    "UPDATE buses SET linear_gain = 1000.5 WHERE id = X'0000000000000000000000000000000B'; "
                    "PRAGMA ignore_check_constraints = OFF;")
                     .ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE ((validation.status == BundleStatus::SemanticInvalid || validation.status == BundleStatus::IntegrityFailed));
}

TEST_CASE ("Opening an existing bundle rejects unknown FX insert kind", "[persistence][semantic][open][fx]")
{
    const auto path = makeTempBundlePath ("semantic-fx-kind-open");
    Project project = makeProject();
    project.tracks.front().strip.fxChain = { makeFxInsert (idFromLowByte (90), FxKind::Eq) };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.executeSql ("UPDATE fx_inserts SET kind = 99 WHERE id = X'0000000000000000000000000000005A';").ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Opening an existing bundle rejects duplicate FX owner positions", "[persistence][semantic][open][fx]")
{
    const auto path = makeTempBundlePath ("semantic-fx-duplicate-position-open");
    Project project = makeProject();
    project.tracks.front().strip.fxChain = { makeFxInsert (idFromLowByte (90), FxKind::Eq) };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.executeSql (
                    "PRAGMA foreign_keys = OFF; "
                    "DROP TABLE fx_insert_params; "
                    "DROP TABLE fx_inserts; "
                    "CREATE TABLE fx_inserts ("
                    "id BLOB PRIMARY KEY CHECK (length(id) = 16), "
                    "owner_entity BLOB NOT NULL CHECK (length(owner_entity) = 16), "
                    "position INTEGER NOT NULL CHECK (position >= 0), "
                    "kind INTEGER NOT NULL, "
                    "enabled INTEGER NOT NULL CHECK (enabled IN (0, 1))); "
                    "CREATE TABLE fx_insert_params ("
                    "insert_id BLOB NOT NULL CHECK (length(insert_id) = 16), "
                    "param_id INTEGER NOT NULL CHECK (param_id >= 0), "
                    "value REAL NOT NULL CHECK(value>=0 AND value<=1), "
                    "PRIMARY KEY(insert_id, param_id)); "
                    "INSERT INTO fx_inserts(id, owner_entity, position, kind, enabled) VALUES "
                    "(X'0000000000000000000000000000005A', X'0000000000000000000000000000000A', 0, 0, 1), "
                    "(X'0000000000000000000000000000005B', X'0000000000000000000000000000000A', 0, 1, 1); "
                    "PRAGMA foreign_keys = ON;")
                     .ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Opening an existing bundle rejects orphaned FX param rows", "[persistence][semantic][open][fx]")
{
    const auto path = makeTempBundlePath ("semantic-fx-orphan-param-open");
    Project project = makeProject();
    project.tracks.front().strip.fxChain = { makeFxInsert (idFromLowByte (90), FxKind::Eq) };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.executeSql (
                    "INSERT INTO fx_insert_params(insert_id, param_id, value) "
                    "VALUES (X'000000000000000000000000000000EE', 1, 0.5);")
                     .ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Opening an existing bundle rejects out-of-range FX normalized params", "[persistence][semantic][open][fx]")
{
    const auto path = makeTempBundlePath ("semantic-fx-param-range-open");
    Project project = makeProject();
    project.tracks.front().strip.fxChain = { makeFxInsert (idFromLowByte (90), FxKind::Eq) };

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.executeSql (
                    "DROP TABLE fx_insert_params; "
                    "CREATE TABLE fx_insert_params ("
                    "insert_id BLOB NOT NULL CHECK (length(insert_id) = 16), "
                    "param_id INTEGER NOT NULL CHECK (param_id >= 0), "
                    "value REAL NOT NULL, "
                    "PRIMARY KEY(insert_id, param_id)); "
                    "INSERT INTO fx_insert_params(insert_id, param_id, value) "
                    "VALUES (X'0000000000000000000000000000005A', 10, 1.25);")
                     .ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Opening an existing bundle rejects invalid stored automation lanes", "[persistence][semantic][open][automation][h15]")
{
    const EntityId trackId = idFromLowByte (10);
    const EntityId busId = idFromLowByte (11);
    const EntityId fxId = idFromLowByte (90);
    const EntityId laneId = idFromLowByte (70);
    const EntityId fxLaneId = idFromLowByte (72);

    const auto makeAutomationProject = [&]
    {
        Project project = makeProject();
        project.buses = { makeBus (busId, "Return") };
        project.tracks[0].strip.fxChain = { makeFxInsert (fxId, FxKind::Eq) };
        project.automationLanes = {
            makeAutomationLane (laneId, trackId, AutomationTargetRole::TrackFader, 1),
            makeAutomationLane (idFromLowByte (71), busId, AutomationTargetRole::BusPan, 1),
            makeAutomationLane (fxLaneId, fxId, AutomationTargetRole::FxInsertParam, 2),
        };
        REQUIRE (project.hasValidAssetClipIndirection());
        return project;
    };

    const auto requireRejectedAfter = [&] (std::string_view label, const std::string& mutationSql)
    {
        INFO (label);
        const auto path = makeTempBundlePath (label);
        const Project project = makeAutomationProject();

        {
            ProjectBundleDb db = openFreshBundle (path);
            REQUIRE (db.writeProjectSnapshot (project).ok());
            writeProjectAssetFiles (path, project);
            REQUIRE (db.executeSql (mutationSql).ok());
        }

        ProjectBundleDb reopened;
        const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
        REQUIRE ((validation.status == BundleStatus::SemanticInvalid || validation.status == BundleStatus::IntegrityFailed));
    };

    requireRejectedAfter (
        "automation-orphan-owner-open",
        "UPDATE automation_lanes SET owner_entity = X'000000000000000000000000000000EE' WHERE id = " + blobLiteral (laneId) + ";");

    requireRejectedAfter (
        "automation-unknown-role-open",
        "PRAGMA ignore_check_constraints = ON; "
        "UPDATE automation_lanes SET target_role = 99 WHERE id = " + blobLiteral (laneId) + "; "
        "PRAGMA ignore_check_constraints = OFF;");

    requireRejectedAfter (
        "automation-value-range-open",
        "PRAGMA ignore_check_constraints = ON; "
        "UPDATE automation_breakpoints SET value = 1.25 WHERE lane_id = " + blobLiteral (laneId) + " AND tick = 0; "
        "PRAGMA ignore_check_constraints = OFF;");

    requireRejectedAfter (
        "automation-quarantined-curve-open",
        "PRAGMA ignore_check_constraints = ON; "
        "UPDATE automation_breakpoints SET curve_type = 2 WHERE lane_id = " + blobLiteral (laneId) + " AND tick = 0; "
        "PRAGMA ignore_check_constraints = OFF;");

    requireRejectedAfter (
        "automation-invalid-strip-param-open",
        "UPDATE automation_lanes SET param_id = 0 WHERE id = " + blobLiteral (laneId) + ";");

    requireRejectedAfter (
        "automation-invalid-fx-param-open",
        "UPDATE automation_lanes SET param_id = 100 WHERE id = " + blobLiteral (fxLaneId) + ";");

    requireRejectedAfter (
        "automation-duplicate-target-open",
        "PRAGMA foreign_keys = OFF; "
        "DROP TABLE automation_breakpoints; "
        "DROP TABLE automation_lanes; "
        "CREATE TABLE automation_lanes ("
        "id BLOB PRIMARY KEY CHECK (length(id) = 16), "
        "owner_entity BLOB NOT NULL CHECK (length(owner_entity) = 16), "
        "target_role INTEGER NOT NULL CHECK (target_role IN (0, 1, 2, 3, 4, 5)), "
        "param_id INTEGER NOT NULL CHECK (param_id >= 0)); "
        "CREATE TABLE automation_breakpoints ("
        "lane_id BLOB NOT NULL CHECK (length(lane_id) = 16), "
        "tick INTEGER NOT NULL CHECK (tick >= 0), "
        "value REAL NOT NULL CHECK(value>=0 AND value<=1), "
        "curve_type INTEGER NOT NULL CHECK(curve_type IN (0,1))); "
        "INSERT INTO automation_lanes(id, owner_entity, target_role, param_id) VALUES "
        "(X'00000000000000000000000000000070', " + blobLiteral (trackId) + ", 0, 1), "
        "(X'00000000000000000000000000000071', " + blobLiteral (trackId) + ", 0, 1); "
        "PRAGMA foreign_keys = ON;");

    requireRejectedAfter (
        "automation-orphan-breakpoint-open",
        "PRAGMA foreign_keys = OFF; "
        "DROP TABLE automation_breakpoints; "
        "CREATE TABLE automation_breakpoints ("
        "lane_id BLOB NOT NULL CHECK (length(lane_id) = 16), "
        "tick INTEGER NOT NULL CHECK (tick >= 0), "
        "value REAL NOT NULL CHECK(value>=0 AND value<=1), "
        "curve_type INTEGER NOT NULL CHECK(curve_type IN (0,1))); "
        "INSERT INTO automation_breakpoints(lane_id, tick, value, curve_type) VALUES "
        "(X'000000000000000000000000000000EE', 0, 0.25, 0); "
        "PRAGMA foreign_keys = ON;");

    requireRejectedAfter (
        "automation-duplicate-tick-open",
        "PRAGMA foreign_keys = OFF; "
        "DROP TABLE automation_breakpoints; "
        "CREATE TABLE automation_breakpoints ("
        "lane_id BLOB NOT NULL CHECK (length(lane_id) = 16), "
        "tick INTEGER NOT NULL CHECK (tick >= 0), "
        "value REAL NOT NULL CHECK(value>=0 AND value<=1), "
        "curve_type INTEGER NOT NULL CHECK(curve_type IN (0,1))); "
        "INSERT INTO automation_breakpoints(lane_id, tick, value, curve_type) VALUES "
        "(" + blobLiteral (laneId) + ", 0, 0.25, 0), "
        "(" + blobLiteral (laneId) + ", 0, 0.75, 1); "
        "PRAGMA foreign_keys = ON;");
}

TEST_CASE ("Opening an existing bundle rejects non-canonical Project value storage types", "[persistence][semantic][open]")
{
    const auto path = makeTempBundlePath ("semantic-type-open");

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (makeProject()).ok());
        writeProjectAssetFiles (path, makeProject());
        REQUIRE (db.executeSql ("UPDATE clips SET src_offset = 0.5 WHERE id = X'00000000000000000000000000000004';").ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Intent log rows commit or roll back with the surrounding asset transaction", "[persistence][intent-log]")
{
    const auto path = makeTempBundlePath ("intent");
    ProjectBundleDb db = openFreshBundle (path);

    const PendingFsOp op {
        PendingFsOpKind::StageAsset,
        "audio/tmp/import.tmp",
        "audio/asset.wav",
        hashFromLowByte (77),
    };

    sqlite3_int64 rowId = 0;
    REQUIRE (db.executeSql ("BEGIN IMMEDIATE;").ok());
    REQUIRE (db.recordPendingFsOp (op, rowId).ok());
    REQUIRE (rowId > 0);
    REQUIRE (db.executeSql ("ROLLBACK;").ok());

    sqlite3_int64 count = 0;
    REQUIRE (db.pendingFsOpCount (false, count).ok());
    REQUIRE (count == 0);

    REQUIRE (db.executeSql ("BEGIN IMMEDIATE;").ok());
    REQUIRE (db.recordPendingFsOp (op, rowId).ok());
    REQUIRE (db.executeSql ("COMMIT;").ok());

    REQUIRE (db.pendingFsOpCount (false, count).ok());
    REQUIRE (count == 1);
    REQUIRE (db.markPendingFsOpCommitted (rowId).ok());
    REQUIRE (db.pendingFsOpCount (false, count).ok());
    REQUIRE (count == 0);
    REQUIRE (db.pendingFsOpCount (true, count).ok());
    REQUIRE (count == 1);
}

TEST_CASE ("Asset import copies bytes by content hash and dedupes repeated imports", "[persistence][asset][import]")
{
    const auto path = makeTempBundlePath ("asset-import-dedupe");
    const auto source = std::filesystem::temp_directory_path() / "yesdaw-import-source-audio.bin";
    const std::vector<std::uint8_t> bytes { 0x00u, 0x11u, 0x22u, 0x33u, 0xFEu, 0xDCu, 0xBAu, 0x98u };
    writeBytes (source, std::span<const std::uint8_t> (bytes.data(), bytes.size()));

    ProjectBundleDb db = openFreshBundle (path);

    const AssetImportRequest firstRequest {
        source,
        idFromLowByte (80),
        48000,
        SampleRate { 48000.0 },
        2,
    };

    Asset first;
    const auto firstImport = db.importAssetBytes (firstRequest, first);
    INFO (firstImport.message);
    REQUIRE (firstImport.ok());
    REQUIRE (first.id == firstRequest.assetId);
    REQUIRE (first.contentHash == hashBytes (std::span<const std::uint8_t> (bytes.data(), bytes.size())));

    const std::filesystem::path finalPath = path / yesdaw::persistence::detail::assetRelativePathForHash (first.contentHash);
    REQUIRE (std::filesystem::exists (finalPath));
    REQUIRE (readBytes (finalPath) == bytes);

    const AssetImportRequest secondRequest {
        source,
        idFromLowByte (81),
        48000,
        SampleRate { 48000.0 },
        2,
    };

    Asset second;
    REQUIRE (db.importAssetBytes (secondRequest, second).ok());
    REQUIRE (second == first);

    sqlite3_int64 count = 0;
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM assets;", count).ok());
    REQUIRE (count == 1);
    REQUIRE (countAudioAssetFiles (path) == 1);
    REQUIRE (db.pendingFsOpCount (false, count).ok());
    REQUIRE (count == 0);
}

TEST_CASE ("Interrupted import reconcile removes orphan final files and stale intents", "[persistence][asset][recovery]")
{
    const auto path = makeTempBundlePath ("asset-import-recovery");
    const std::vector<std::uint8_t> bytes { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u };
    const AssetContentHash hash = hashBytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
    const std::string finalRelativePath = yesdaw::persistence::detail::assetRelativePathForHash (hash);
    const std::string tempRelativePath = yesdaw::persistence::detail::assetTempRelativePathForHash (hash);

    {
        ProjectBundleDb db = openFreshBundle (path);
        writeBytes (path / finalRelativePath, std::span<const std::uint8_t> (bytes.data(), bytes.size()));
        writeBytes (path / tempRelativePath, std::span<const std::uint8_t> (bytes.data(), bytes.size()));

        const PendingFsOp op {
            PendingFsOpKind::StageAsset,
            tempRelativePath,
            finalRelativePath,
            hash,
        };

        sqlite3_int64 rowId = 0;
        REQUIRE (db.executeSql ("BEGIN IMMEDIATE;").ok());
        REQUIRE (db.recordPendingFsOp (op, rowId).ok());
        REQUIRE (db.executeSql ("COMMIT;").ok());
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    REQUIRE_FALSE (std::filesystem::exists (path / finalRelativePath));
    REQUIRE_FALSE (std::filesystem::exists (path / tempRelativePath));
    REQUIRE (countAudioAssetFiles (path) == 0);

    sqlite3_int64 count = 0;
    REQUIRE (reopened.pendingFsOpCount (false, count).ok());
    REQUIRE (count == 0);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM assets;", count).ok());
    REQUIRE (count == 0);
}

TEST_CASE ("Opening a bundle rejects committed Asset rows with missing or corrupt bytes", "[persistence][asset][open]")
{
    const std::vector<std::uint8_t> bytes { 0xA0u, 0xA1u, 0xA2u, 0xA3u, 0xA4u, 0xA5u };

    const auto missingPath = makeTempBundlePath ("asset-missing");
    const auto missingSource = std::filesystem::temp_directory_path() / "yesdaw-import-missing-source.bin";
    writeBytes (missingSource, std::span<const std::uint8_t> (bytes.data(), bytes.size()));

    Asset missingAsset;
    {
        ProjectBundleDb db = openFreshBundle (missingPath);
        const auto import = db.importAssetBytes ({ missingSource, idFromLowByte (90), 256, SampleRate { 48000.0 }, 2 }, missingAsset);
        INFO (import.message);
        REQUIRE (import.ok());
    }
    std::error_code removeError;
    REQUIRE (std::filesystem::remove (missingPath / yesdaw::persistence::detail::assetRelativePathForHash (missingAsset.contentHash), removeError));
    REQUIRE (! removeError);

    ProjectBundleDb missingReopen;
    REQUIRE (ProjectBundleDb::openExistingBundle (missingPath, missingReopen).status == BundleStatus::IntegrityFailed);

    const auto corruptPath = makeTempBundlePath ("asset-corrupt");
    const auto corruptSource = std::filesystem::temp_directory_path() / "yesdaw-import-corrupt-source.bin";
    writeBytes (corruptSource, std::span<const std::uint8_t> (bytes.data(), bytes.size()));

    Asset corruptAsset;
    {
        ProjectBundleDb db = openFreshBundle (corruptPath);
        const auto import = db.importAssetBytes ({ corruptSource, idFromLowByte (91), 256, SampleRate { 48000.0 }, 2 }, corruptAsset);
        INFO (import.message);
        REQUIRE (import.ok());
    }

    const std::vector<std::uint8_t> badBytes { 0x00u, 0x00u, 0x00u };
    writeBytes (corruptPath / yesdaw::persistence::detail::assetRelativePathForHash (corruptAsset.contentHash),
                std::span<const std::uint8_t> (badBytes.data(), badBytes.size()));

    ProjectBundleDb corruptReopen;
    REQUIRE (ProjectBundleDb::openExistingBundle (corruptPath, corruptReopen).status == BundleStatus::IntegrityFailed);
}

TEST_CASE ("Plugin state chunks persist opaque bytes with host metadata and VST3 restore ordering", "[persistence][plugin-state]")
{
    const auto path = makeTempBundlePath ("plugin-state-vst3");
    ProjectBundleDb db = openFreshBundle (path);

    const EntityId nodeId = EntityId::fromBigEndianParts (0x1122334455667788ull, 0x99AABBCCDDEEFF00ull);
    const std::vector<std::uint8_t> componentBytes { 0x10u, 0x20u, 0x30u, 0x40u, 0x50u };
    const std::vector<std::uint8_t> controllerBytes { 0xCCu, 0xBBu, 0xAAu, 0x99u };

    PluginStateChunkRecord controller {
        nodeId,
        PluginStateFormat::Vst3,
        "com.yesdaw.test.delay",
        "1.2.3",
        PluginStateChunkKind::Vst3Controller,
        controllerBytes,
    };
    PluginStateChunkRecord component = controller;
    component.chunkKind = PluginStateChunkKind::Vst3Component;
    component.bytes = componentBytes;

    REQUIRE (db.writePluginStateChunk (controller).ok());
    REQUIRE (db.writePluginStateChunk (component).ok());

    sqlite3_int64 count = 0;
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM plugin_state_chunks WHERE node_id = " + blobLiteral (nodeId) + ";", count).ok());
    REQUIRE (count == 2);

    sqlite3_int64 storedLength = 0;
    REQUIRE (db.queryInt64 (
                "SELECT chunk_len FROM plugin_state_chunks WHERE node_id = " + blobLiteral (nodeId)
                    + " AND chunk_kind = 0;",
                storedLength)
                 .ok());
    REQUIRE (storedLength == static_cast<sqlite3_int64> (componentBytes.size()));

    sqlite3_int64 storedCrc = 0;
    REQUIRE (db.queryInt64 (
                "SELECT crc32 FROM plugin_state_chunks WHERE node_id = " + blobLiteral (nodeId)
                    + " AND chunk_kind = 0;",
                storedCrc)
                 .ok());
    REQUIRE (storedCrc == static_cast<sqlite3_int64> (yesdaw::persistence::detail::crc32Bytes (
                              std::span<const std::uint8_t> (componentBytes.data(), componentBytes.size()))));

    std::vector<PluginStateRestoreChunk> chunks;
    REQUIRE (db.readPluginStateChunksForNode (nodeId, chunks).ok());
    REQUIRE (chunks.size() == 2u);
    REQUIRE (chunks[0].ready());
    REQUIRE (chunks[1].ready());
    REQUIRE (chunks[0].chunk.chunkKind == PluginStateChunkKind::Vst3Component);
    REQUIRE (chunks[1].chunk.chunkKind == PluginStateChunkKind::Vst3Controller);
    REQUIRE (chunks[0].chunk.bytes == componentBytes);
    REQUIRE (chunks[1].chunk.bytes == controllerBytes);
    REQUIRE (chunks[0].chunk.format == PluginStateFormat::Vst3);
    REQUIRE (chunks[0].chunk.pluginUid == "com.yesdaw.test.delay");
    REQUIRE (chunks[0].chunk.pluginVersion == "1.2.3");
}

TEST_CASE ("Plugin state chunks are keyed by the persistent 16-byte node Entity ID", "[persistence][plugin-state]")
{
    const auto path = makeTempBundlePath ("plugin-state-node-id");
    ProjectBundleDb db = openFreshBundle (path);

    const EntityId firstNode = EntityId::fromBigEndianParts (0x0102030405060708ull, 0xDEADBEEF12345678ull);
    const EntityId secondNode = EntityId::fromBigEndianParts (0xF1E2D3C4B5A69788ull, 0xDEADBEEF12345678ull);
    const std::vector<std::uint8_t> firstBytes { 0x01u, 0x02u, 0x03u };
    const std::vector<std::uint8_t> secondBytes { 0xA0u, 0xB0u, 0xC0u, 0xD0u };

    REQUIRE (db.writePluginStateChunk ({ firstNode, PluginStateFormat::Clap, "org.yesdaw.same-low-node", "7", PluginStateChunkKind::ClapState, firstBytes }).ok());
    REQUIRE (db.writePluginStateChunk ({ secondNode, PluginStateFormat::Clap, "org.yesdaw.same-low-node", "7", PluginStateChunkKind::ClapState, secondBytes }).ok());

    sqlite3_int64 count = 0;
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM plugin_state_chunks;", count).ok());
    REQUIRE (count == 2);

    PluginStateRestoreChunk first;
    REQUIRE (db.readPluginStateChunk (firstNode, PluginStateChunkKind::ClapState, first).ok());
    REQUIRE (first.ready());
    REQUIRE (first.chunk.bytes == firstBytes);

    PluginStateRestoreChunk second;
    REQUIRE (db.readPluginStateChunk (secondNode, PluginStateChunkKind::ClapState, second).ok());
    REQUIRE (second.ready());
    REQUIRE (second.chunk.bytes == secondBytes);
}

TEST_CASE ("Plugin state restore validates headers and degrades corrupt or missing chunks to defaults", "[persistence][plugin-state]")
{
    const auto path = makeTempBundlePath ("plugin-state-corrupt");
    ProjectBundleDb db = openFreshBundle (path);

    const EntityId nodeId = EntityId::fromBigEndianParts (0xABCDEF0001020304ull, 0x05060708090A0B0Cull);
    const std::vector<std::uint8_t> bytes { 0x42u, 0x24u, 0x66u, 0x18u };
    REQUIRE (yesdaw::persistence::detail::crc32Bytes (std::span<const std::uint8_t> (bytes.data(), bytes.size())) != 0u);
    REQUIRE (db.writePluginStateChunk ({ nodeId, PluginStateFormat::Vst3, "com.yesdaw.test.synth", "2026.6", PluginStateChunkKind::Vst3Component, bytes }).ok());

    PluginStateRestoreChunk ready;
    REQUIRE (db.readPluginStateChunk (nodeId, PluginStateChunkKind::Vst3Component, ready).ok());
    REQUIRE (ready.ready());
    REQUIRE (ready.chunk.bytes == bytes);

    PluginStateRestoreChunk missing;
    REQUIRE (db.readPluginStateChunk (nodeId, PluginStateChunkKind::Vst3Controller, missing).ok());
    REQUIRE (missing.status == PluginStateRestoreStatus::Missing);
    REQUIRE_FALSE (missing.ready());
    REQUIRE (missing.chunk.bytes.empty());

    REQUIRE (db.executeSql (
                "PRAGMA ignore_check_constraints = ON; "
                "UPDATE plugin_state_chunks SET chunk_len = 99 WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0; "
                "PRAGMA ignore_check_constraints = OFF;")
                 .ok());

    PluginStateRestoreChunk badLength;
    REQUIRE (db.readPluginStateChunk (nodeId, PluginStateChunkKind::Vst3Component, badLength).ok());
    REQUIRE (badLength.status == PluginStateRestoreStatus::Unreadable);
    REQUIRE_FALSE (badLength.ready());
    REQUIRE (badLength.chunk.bytes.empty());
    REQUIRE (readRawPluginStateBytes (path, nodeId, PluginStateChunkKind::Vst3Component) == bytes);

    REQUIRE (db.executeSql (
                "PRAGMA ignore_check_constraints = ON; "
                "UPDATE plugin_state_chunks SET chunk_len = 4, crc32 = 0 WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0; "
                "PRAGMA ignore_check_constraints = OFF;")
                 .ok());

    PluginStateRestoreChunk badCrc;
    REQUIRE (db.readPluginStateChunk (nodeId, PluginStateChunkKind::Vst3Component, badCrc).ok());
    REQUIRE (badCrc.status == PluginStateRestoreStatus::Unreadable);
    REQUIRE_FALSE (badCrc.ready());
    REQUIRE (badCrc.chunk.bytes.empty());
    REQUIRE (readRawPluginStateBytes (path, nodeId, PluginStateChunkKind::Vst3Component) == bytes);
}

TEST_CASE ("Plugin state restore rejects non-canonical SQLite header storage classes", "[persistence][plugin-state]")
{
    const auto path = makeTempBundlePath ("plugin-state-storage-classes");
    ProjectBundleDb db = openFreshBundle (path);

    const EntityId nodeId = EntityId::fromBigEndianParts (0x1234000012340000ull, 0x5678000056780000ull);
    const std::vector<std::uint8_t> bytes { 0x41u, 0x42u, 0x43u, 0x44u };
    const PluginStateChunkRecord record {
        nodeId,
        PluginStateFormat::Vst3,
        "com.yesdaw.test.storage-classes",
        "1.0",
        PluginStateChunkKind::Vst3Component,
        bytes,
    };

    const auto resetRecord = [&]
    {
        REQUIRE (db.executeSql ("DELETE FROM plugin_state_chunks WHERE node_id = " + blobLiteral (nodeId) + ";").ok());
        REQUIRE (db.writePluginStateChunk (record).ok());
    };

    const auto requireUnreadableAfter = [&] (std::string_view updateSql)
    {
        resetRecord();
        REQUIRE (db.executeSql (
                    "PRAGMA ignore_check_constraints = ON; "
                    + std::string (updateSql)
                    + " PRAGMA ignore_check_constraints = OFF;")
                     .ok());

        PluginStateRestoreChunk chunk;
        REQUIRE (db.readPluginStateChunk (nodeId, PluginStateChunkKind::Vst3Component, chunk).ok());
        REQUIRE (chunk.status == PluginStateRestoreStatus::Unreadable);
        REQUIRE_FALSE (chunk.ready());
        REQUIRE (chunk.chunk.bytes.empty());
        REQUIRE (readRawPluginStateBytes (path, nodeId, PluginStateChunkKind::Vst3Component) == bytes);
    };

    requireUnreadableAfter ("UPDATE plugin_state_chunks SET format = X'76737433' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;");
    requireUnreadableAfter ("UPDATE plugin_state_chunks SET format = CAST(X'76737433006a756e6b' AS TEXT) WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;");
    requireUnreadableAfter ("UPDATE plugin_state_chunks SET plugin_uid = X'75736572' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;");
    requireUnreadableAfter ("UPDATE plugin_state_chunks SET plugin_version = X'312e30' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;");
    requireUnreadableAfter ("UPDATE plugin_state_chunks SET chunk_len = X'00000004' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;");
    requireUnreadableAfter ("UPDATE plugin_state_chunks SET crc32 = X'00000000' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;");
    requireUnreadableAfter ("UPDATE plugin_state_chunks SET data = 'ABCD' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;");

    resetRecord();
    REQUIRE (db.executeSql (
                "UPDATE plugin_state_chunks SET chunk_kind = X'00' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;")
                 .ok());

    std::vector<PluginStateRestoreChunk> chunks;
    REQUIRE (db.readPluginStateChunksForNode (nodeId, chunks).ok());
    REQUIRE (chunks.size() == 1u);
    REQUIRE (chunks[0].status == PluginStateRestoreStatus::Unreadable);
    REQUIRE_FALSE (chunks[0].ready());
    REQUIRE (chunks[0].chunk.bytes.empty());
}

TEST_CASE ("Plugin blacklist rows are keyed by plugin identity and survive reopen", "[persistence][plugin-blacklist]")
{
    const auto path = makeTempBundlePath ("plugin-blacklist");
    constexpr auto format = PluginStateFormat::Vst3;
    const std::string uid = "com.yesdaw.test.crashy";
    const std::string version = "1.0.0";
    const std::string otherVersion = "1.0.1";

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writePluginBlacklistEntry ({ format, uid, version, "watchdog-timeout" }).ok());

        bool exact = false;
        REQUIRE (db.pluginBlacklistContains (format, uid, version, exact).ok());
        REQUIRE (exact);

        bool wrongVersion = true;
        REQUIRE (db.pluginBlacklistContains (format, uid, otherVersion, wrongVersion).ok());
        REQUIRE_FALSE (wrongVersion);

        REQUIRE (db.writePluginBlacklistEntry ({ format, uid, otherVersion, "crash" }).ok());
        REQUIRE (db.writePluginBlacklistEntry ({ format, uid, version, "watchdog-timeout-repeat" }).ok());

        sqlite3_int64 count = 0;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM plugin_blacklist WHERE format = 'vst3' AND plugin_uid = 'com.yesdaw.test.crashy';", count).ok());
        REQUIRE (count == 2);

        std::string reason;
        REQUIRE (db.queryText ("SELECT reason FROM plugin_blacklist WHERE format = 'vst3' AND plugin_uid = 'com.yesdaw.test.crashy' AND plugin_version = '1.0.0';", reason).ok());
        REQUIRE (reason == "watchdog-timeout-repeat");
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    bool exactAfterRestart = false;
    REQUIRE (reopened.pluginBlacklistContains (format, uid, version, exactAfterRestart).ok());
    REQUIRE (exactAfterRestart);

    bool wrongFormat = true;
    REQUIRE (reopened.pluginBlacklistContains (PluginStateFormat::AudioUnit, uid, version, wrongFormat).ok());
    REQUIRE_FALSE (wrongFormat);
}

TEST_CASE ("Plugin blacklist rejects incomplete plugin identity", "[persistence][plugin-blacklist]")
{
    const auto path = makeTempBundlePath ("plugin-blacklist-invalid");
    ProjectBundleDb db = openFreshBundle (path);

    REQUIRE (db.writePluginBlacklistEntry ({ PluginStateFormat::Vst3, "", "1.0.0", "crash" }).status
             == BundleStatus::SemanticInvalid);
    REQUIRE (db.writePluginBlacklistEntry ({ PluginStateFormat::Vst3, "com.yesdaw.test", "", "crash" }).status
             == BundleStatus::SemanticInvalid);
    REQUIRE (db.writePluginBlacklistEntry ({ static_cast<PluginStateFormat> (255), "com.yesdaw.test", "1.0.0", "crash" }).status
             == BundleStatus::SemanticInvalid);

    bool present = true;
    REQUIRE (db.pluginBlacklistContains (PluginStateFormat::Vst3, "", "1.0.0", present).status
             == BundleStatus::SemanticInvalid);
    REQUIRE_FALSE (present);
}

TEST_CASE ("Waveform peak cache builds deterministic min max and RMS tiers", "[persistence][asset][peaks]")
{
    Asset asset;
    asset.id = idFromLowByte (100);
    asset.contentHash = hashFromLowByte (101);
    asset.frames = 16;
    asset.sampleRate = SampleRate { 48000.0 };
    asset.channels = 1;

    const std::vector<float> samples {
        -1.0f, 0.5f, 0.25f, -0.25f,
        2.0f, -2.0f, 0.0f, 1.0f,
        0.25f, 0.25f, 0.25f, 0.25f,
        -0.5f, -0.5f, 0.5f, 0.5f,
    };

    const auto result = buildWaveformPeakCache (asset, std::span<const float> (samples.data(), samples.size()), 4);
    INFO (result.message);
    REQUIRE (result.ok());

    const auto& cache = result.cache;
    REQUIRE (cache.contentHash == asset.contentHash);
    REQUIRE (cache.sourceFrames == asset.frames);
    REQUIRE (cache.channels == asset.channels);
    REQUIRE (cache.tiers.size() == 2u);

    const auto& tier0 = cache.tiers[0];
    REQUIRE (tier0.framesPerPeak == 4u);
    REQUIRE (tier0.peaks.size() == 4u);
    REQUIRE (tier0.peaks[0].min == Approx (-1.0f));
    REQUIRE (tier0.peaks[0].max == Approx (0.5f));
    REQUIRE (tier0.peaks[0].rms == Approx (std::sqrt (1.375 / 4.0)));
    REQUIRE (tier0.peaks[1].min == Approx (-2.0f));
    REQUIRE (tier0.peaks[1].max == Approx (2.0f));
    REQUIRE (tier0.peaks[1].rms == Approx (1.5));

    const auto& folded = cache.tiers[1];
    REQUIRE (folded.framesPerPeak == 64u);
    REQUIRE (folded.peaks.size() == 1u);
    REQUIRE (folded.peaks[0].min == Approx (-2.0f));
    REQUIRE (folded.peaks[0].max == Approx (2.0f));
    REQUIRE (folded.peaks[0].rms == Approx (std::sqrt (11.625 / 16.0)));
}
