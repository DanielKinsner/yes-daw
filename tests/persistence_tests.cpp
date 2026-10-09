// YES DAW - headless checks for ADR-0012 SQLite bundle schema/migrations/intent log.

#define YESDAW_PERSISTENCE_TEST_HOOKS 1
#include "persistence/AutosaveRecovery.h"
#include "persistence/ProjectBundle.h"
#include "persistence/WaveformPeakCache.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <grp.h>
#include <membership.h>
#include <sys/acl.h>
#endif
#endif

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
    CAPTURE (path);
    const auto size = std::filesystem::file_size (path);
    std::vector<std::uint8_t> bytes (static_cast<std::size_t> (size));

    std::ifstream input (path, std::ios::binary);
    REQUIRE (input.good());
    input.read (reinterpret_cast<char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    REQUIRE (input.good());
    return bytes;
}

using SourceInventory = std::map<std::filesystem::path,
                                 std::pair<std::filesystem::file_type, std::vector<std::uint8_t>>>;

SourceInventory sourceInventory (const std::filesystem::path& root)
{
    SourceInventory inventory;
    for (const auto& entry : std::filesystem::recursive_directory_iterator (root))
    {
        const auto type = entry.symlink_status().type();
        inventory.emplace (entry.path().lexically_relative (root),
                           std::make_pair (type, type == std::filesystem::file_type::regular
                                                    ? readBytes (entry.path()) : std::vector<std::uint8_t> {}));
    }
    return inventory;
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
    REQUIRE (actual.locatePoints == expected.locatePoints);
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
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 9;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 10;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 11;", value).ok());
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
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'midi_control_events';", value).ok());   // G3.3: v28
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'locate_points';", value).ok());
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

    // R16 re-pin: Bezier writes cleanly now — only an out-of-enum curve refuses.
    Project invalidAutomation = project;
    invalidAutomation.automationLanes = {
        makeAutomationLane (idFromLowByte (70), project.tracks[0].id, AutomationTargetRole::TrackFader, 1),
    };
    invalidAutomation.automationLanes.front().points.front().curveType = static_cast<AutomationCurveType> (9);
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
    project.clips[1].stretchFactor = 1.5f;   // G2.9: schema v22
    project.clips[1].fadeInShape = yesdaw::engine::FadeShape::SCurve;   // G2.10: schema v23
    project.clips[1].fadeOutShape = yesdaw::engine::FadeShape::Log;
    project.clips[1].fadeInCurve = 0.25f;
    project.clips[1].fadeOutCurve = -0.5f;
    project.clips[1].colour = 0xff3b8cffu;   // G2.12: schema v24
    project.clips[1].muted = true;
    project.clips[1].reversed = true;   // G2.13: schema v25
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

TEST_CASE ("Project send routes round-trip through schema v9", "[persistence][project][round-trip][sends]")
{
    const auto path = makeTempBundlePath ("sends-round-trip");

    Project project = makeProject();
    project.buses = { makeBus (idFromLowByte (11), "Return"), makeBus (idFromLowByte (12), "Verb") };
    project.tracks[0].sends = {
        yesdaw::engine::SendRow { idFromLowByte (80), idFromLowByte (11),
                                  yesdaw::engine::SendTap::PostFader, 0.5f },
        yesdaw::engine::SendRow { idFromLowByte (81), idFromLowByte (12),
                                  yesdaw::engine::SendTap::PreFader, 0.25f },
    };
    REQUIRE (project.hasValidAssetClipIndirection());

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);

        sqlite3_int64 count = 0;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sends;", count).ok());
        REQUIRE (count == 2);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
    REQUIRE (readback.tracks[0].sends == project.tracks[0].sends);
    REQUIRE (readback.tracks[0].sends[0].tap == yesdaw::engine::SendTap::PostFader);
    REQUIRE (readback.tracks[0].sends[1].tap == yesdaw::engine::SendTap::PreFader);
    REQUIRE (readback.tracks[0].sends[1].linearGain == 0.25f);

    Project mutatedSend = project;
    mutatedSend.tracks[0].sends[0].linearGain = 0.75f;
    REQUIRE_FALSE (readback.tracks == mutatedSend.tracks);
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

TEST_CASE ("frozen H15 automation schema-v8 fixture bundle opens on HEAD forever",
           "[persistence][project][fixture][automation][h15]")
{
    const std::filesystem::path fixturePath { YESDAW_AUTOMATION_SCHEMA_FIXTURE_PATH };
    REQUIRE (std::filesystem::exists (fixturePath / "project.db"));

    const std::vector<std::uint8_t> fixtureDbBefore = readBytes (fixturePath / "project.db");
    const std::filesystem::path workingPath = makeTempBundlePath ("automation-fixture-copy");

    std::error_code ec;
    std::filesystem::copy (fixturePath, workingPath, std::filesystem::copy_options::recursive, ec);
    REQUIRE (! ec);

    ProjectBundleDb db;
    REQUIRE (ProjectBundleDb::openExistingBundle (workingPath, db).ok());

    sqlite3_int64 value = 0;
    REQUIRE (db.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM automation_lanes;", value).ok());
    REQUIRE (value == 2);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM automation_breakpoints;", value).ok());
    REQUIRE (value == 4);

    Project project;
    REQUIRE (db.readProjectSnapshot (project).ok());
    REQUIRE (project.automationLanes.size() == 2u);
    REQUIRE (project.automationLanes[0].ownerEntity == project.tracks[0].id);
    // ADR-0044 additive migration: a pre-v9 bundle opens with the sends table present and empty.
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sends;", value).ok());
    REQUIRE (value == 0);
    for (const auto& track : project.tracks)
        REQUIRE (track.sends.empty());
    REQUIRE (project.automationLanes[0].role == AutomationTargetRole::TrackFader);
    REQUIRE (project.automationLanes[0].paramId == 1u);
    REQUIRE (project.automationLanes[0].points.size() == 2u);
    REQUIRE (project.automationLanes[1].ownerEntity == project.tracks[0].strip.fxChain[0].id);
    REQUIRE (project.automationLanes[1].role == AutomationTargetRole::FxInsertParam);
    REQUIRE (project.automationLanes[1].paramId == 2u);
    REQUIRE (project.automationLanes[1].points.size() == 2u);
    REQUIRE (project.hasValidAssetClipIndirection());

    REQUIRE (readBytes (fixturePath / "project.db") == fixtureDbBefore);
}

// G3.1 / ADR-0047: the Track instrument slot round-trips; a v26 bundle (no columns) opens with
// None / empty; an out-of-range kind is refused as semantically invalid.
TEST_CASE ("Track instrument slot round-trips through a reopened bundle (schema v27)",
           "[persistence][project][round-trip][instrument]")
{
    const auto path = makeTempBundlePath ("instrument-round-trip");

    Project project = makeProject();
    REQUIRE (project.tracks.size() >= 1u);
    project.tracks[0].instrumentKind = yesdaw::engine::TrackInstrumentKind::SimpleSynth;
    // cp2: the state is a real encoded parameter blob (an arbitrary byte string is malformed).
    project.tracks[0].instrumentState = yesdaw::engine::encodeInstrumentParams ({ { 2u, 0.25 }, { 6u, 0.5 } });
    REQUIRE (! project.tracks[0].instrumentState.empty());
    REQUIRE (project.tracks[0].isValid());

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
    REQUIRE (readback.tracks[0].instrumentKind == yesdaw::engine::TrackInstrumentKind::SimpleSynth);
    REQUIRE (readback.tracks[0].instrumentState == project.tracks[0].instrumentState);

    // Negative control: the stored kind is read, not re-derived.
    Project mutated = project;
    mutated.tracks[0].instrumentKind = yesdaw::engine::TrackInstrumentKind::None;
    REQUIRE_FALSE (readback.tracks == mutated.tracks);

    // An out-of-range kind on disk is refused.
    REQUIRE (reopened.executeSql ("UPDATE tracks SET instrument_kind = 9;").ok());
    Project refused;
    REQUIRE_FALSE (reopened.readProjectSnapshot (refused).ok());
}

// G3.1 checkpoint (SS-3 found it): a saved project.db is self-contained — the WAL is checkpointed
// and truncated by the save, so nothing waits in project.db-wal, and reopening then closing the
// bundle leaves the bytes exactly as the save wrote them.
TEST_CASE ("a save leaves no pending WAL and project.db survives a reopen byte-identically",
           "[persistence][project][wal][g3]")
{
    const auto path = makeTempBundlePath ("wal-checkpoint");
    const Project project = makeProject();
    const std::filesystem::path db = path / "project.db";
    const std::filesystem::path wal = path / "project.db-wal";

    std::vector<std::uint8_t> saved;
    {
        ProjectBundleDb bundle = openFreshBundle (path);
        REQUIRE (bundle.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        std::error_code ec;
        const auto walSize = std::filesystem::exists (wal, ec) ? std::filesystem::file_size (wal, ec) : 0u;
        REQUIRE (walSize == 0u);   // nothing pending after the save
        saved = readBytes (db);
        REQUIRE (! saved.empty());
    }
    REQUIRE (readBytes (db) == saved);   // closing the connection rewrote nothing

    {
        ProjectBundleDb reopened;
        REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());
        Project readback;
        REQUIRE (reopened.readProjectSnapshot (readback).ok());
        requireSameProjectSurface (readback, project);
    }
    REQUIRE (readBytes (db) == saved);   // a read-only open + close changes no byte
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
    // G3.3: one control point of every kind rides the Clip (schema v28).
    {
        using yesdaw::engine::MidiControlEvent;
        using yesdaw::engine::MidiControlKind;
        const auto control = [] (std::uint8_t id, yesdaw::engine::Tick tick, MidiControlKind kind, std::int16_t number, double value)
        {
            MidiControlEvent event;
            event.id = idFromLowByte (id);
            event.tick = tick;
            event.kind = kind;
            event.number = number;
            event.value = value;
            event.portIndex = 0;
            event.channel = 1;
            return event;
        };
        project.midiClips[0].controlEvents = {
            control (80, 0, MidiControlKind::ControlChange, 64, 1.0),
            control (81, 256, MidiControlKind::PitchBend, 0, -0.5),
            control (82, 300, MidiControlKind::ChannelPressure, 0, 0.25),
            control (83, 400, MidiControlKind::PolyPressure, 60, 0.75),
            control (84, 512, MidiControlKind::ProgramChange, 7, 0.0),
        };
        // G3.5: the Clip's own settings ride the row (schema v29).
        project.midiClips[0].muted = true;
        project.midiClips[0].transposeSemitones = -7;
        project.midiClips[0].velocityOffset = 0.25;
        project.midiClips[0].loopLengthTicks = 15360;
        REQUIRE (project.midiClips[0].isValid());
    }

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);

        sqlite3_int64 count = 0;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM midi_clips;", count).ok());
        REQUIRE (count == 1);
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM midi_notes;", count).ok());
        REQUIRE (count == 3);
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM midi_control_events;", count).ok());
        REQUIRE (count == 5);
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
    REQUIRE (readback.midiClips[0].controlEvents == project.midiClips[0].controlEvents);
    REQUIRE (readback.midiClips[0].controlEvents[1].kind == yesdaw::engine::MidiControlKind::PitchBend);
    REQUIRE (readback.midiClips[0].controlEvents[1].value == -0.5);
    REQUIRE (readback.midiClips[0].muted);   // G3.5
    REQUIRE (readback.midiClips[0].transposeSemitones == -7);
    REQUIRE (readback.midiClips[0].velocityOffset == 0.25);
    REQUIRE (readback.midiClips[0].loopLengthTicks == 15360);
    // G3.5 negative controls: a transpose past the range and a loop longer than the Clip are refused on open.
    REQUIRE (reopened.executeSql ("UPDATE midi_clips SET transpose = 60;").ok());
    Project refusedTranspose;
    REQUIRE (reopened.readProjectSnapshot (refusedTranspose).status == BundleStatus::SemanticInvalid);
    REQUIRE (reopened.executeSql ("UPDATE midi_clips SET transpose = -7, loop_length = timeline_length + 1;").ok());
    Project refusedLoop;
    REQUIRE (reopened.readProjectSnapshot (refusedLoop).status == BundleStatus::SemanticInvalid);
    REQUIRE (reopened.executeSql ("UPDATE midi_clips SET loop_length = 15360;").ok());

    Project mutatedNote = project;
    mutatedNote.midiClips[0].notes[1].normalizedVelocity = 0.5;
    REQUIRE_FALSE (readback.midiClips == mutatedNote.midiClips);

    // G3.3 negative controls: a control point past the Clip's end is refused on write; a row whose
    // kind or value is outside the engine's range is refused on open (a hand-edited bundle).
    {
        Project controlPastClip = project;
        controlPastClip.midiClips[0].controlEvents[0].tick = controlPastClip.midiClips[0].timelineLength + 1;
        ProjectBundleDb invalidControlDb = openFreshBundle (makeTempBundlePath ("midi-control-invalid"));
        REQUIRE (invalidControlDb.writeProjectSnapshot (controlPastClip).status == BundleStatus::SemanticInvalid);

        REQUIRE (reopened.executeSql ("PRAGMA ignore_check_constraints = ON; UPDATE midi_control_events SET kind = 9 WHERE number = 7; PRAGMA ignore_check_constraints = OFF;").ok());
        Project refusedKind;
        REQUIRE_FALSE (reopened.readProjectSnapshot (refusedKind).ok());   // the CHECK trips before the semantic read does
        REQUIRE (reopened.executeSql ("UPDATE midi_control_events SET kind = 4 WHERE number = 7;").ok());
        REQUIRE (reopened.executeSql ("UPDATE midi_control_events SET value = -0.5 WHERE number = 64;").ok());   // a CC below 0
        Project refusedValue;
        REQUIRE (reopened.readProjectSnapshot (refusedValue).status == BundleStatus::SemanticInvalid);
        REQUIRE (reopened.executeSql ("UPDATE midi_control_events SET value = 1.0 WHERE number = 64;").ok());
        Project restored;
        REQUIRE (reopened.readProjectSnapshot (restored).ok());
        REQUIRE (restored.midiClips[0].controlEvents == project.midiClips[0].controlEvents);
    }

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

TEST_CASE ("Clip display names round-trip through schema v10", "[persistence][project][round-trip][clip-name]")
{
    const auto path = makeTempBundlePath ("clip-name-round-trip");
    Project project = makeProject();
    project.clips[0].name = "Lead Vocal Comp";

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
    REQUIRE (readback.clips[0].name == "Lead Vocal Comp");
}

TEST_CASE ("Locate points round-trip through schema v11", "[persistence][project][round-trip][locate-points]")
{
    const auto path = makeTempBundlePath ("locate-points-round-trip");
    Project project = makeProject();
    project.locatePoints[0] = 12'345;
    project.locatePoints[4] = 67'890;

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());
    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM locate_points;", value).ok());
    REQUIRE (value == 2);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
    REQUIRE (readback.locatePoints[0] == 12'345);
    REQUIRE_FALSE (readback.locatePoints[1].has_value());
    REQUIRE (readback.locatePoints[4] == 67'890);
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

TEST_CASE ("Schema v10 migration gives legacy Clips the default display name", "[persistence][migration][clip-name]")
{
    const auto path = makeTempBundlePath ("clip-name-v9-migration");

    std::error_code ec;
    std::filesystem::create_directories (path / "audio", ec);
    REQUIRE (! ec);

    const EntityId projectId = idFromLowByte (1);
    const Asset asset = makeAsset (idFromLowByte (2), 1000);
    const EntityId trackId = idFromLowByte (10);
    const EntityId clipId = idFromLowByte (4);

    const std::vector<std::uint8_t> assetBytes = assetBytesForId (asset.id);
    writeBytes (path / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash),
                std::span<const std::uint8_t> (assetBytes.data(), assetBytes.size()));

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV9 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 9);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV9).ok());
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
        "INSERT INTO tracks(id, name, linear_gain, pan, muted, soloed, solo_safe) VALUES ("
        + blobLiteral (trackId) + ", 'Audio 1', 1.0, 0.0, 0, 0, 0);");
    requireRawExec (
        rawDb,
        "INSERT INTO clips(id, asset_id, track_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base) VALUES ("
        + blobLiteral (clipId) + ", " + blobLiteral (asset.id) + ", " + blobLiteral (trackId)
        + ", 0, 15360, 100, 900, 0.75, 16, 32, 1);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    // N5 re-pin: migrations now run through v14 (automation_mode).
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 10;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 11;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 12;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 13;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM locate_points;", value).ok());
    REQUIRE (value == 0);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM master_strip;", value).ok());
    REQUIRE (value == 0);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM track_outputs;", value).ok());
    REQUIRE (value == 0);

    std::string storedName;
    REQUIRE (reopened.queryText ("SELECT name FROM clips WHERE id = X'00000000000000000000000000000004';", storedName).ok());
    REQUIRE (storedName == "Audio Clip");

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.clips.size() == 1u);
    REQUIRE (readback.clips[0].name == "Audio Clip");
    REQUIRE (readback.hasValidAssetClipIndirection());
}

// N6: mirrors the v10 clip-name test's ADD COLUMN pattern — height_px is a new column on the
// EXISTING tracks table (not a new locate-points-style table), so "before v15" is built by
// running migrations only up to v14 and inserting a Track row that predates the column entirely.
TEST_CASE ("Schema v15 migration gives legacy Tracks the auto-shared height default",
           "[persistence][migration][track-height]")
{
    const auto path = makeTempBundlePath ("track-height-v14-migration");

    std::error_code ec;
    std::filesystem::create_directories (path / "audio", ec);
    REQUIRE (! ec);

    const EntityId projectId = idFromLowByte (1);
    const EntityId trackId = idFromLowByte (10);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV14 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 14);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV14).ok());
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) VALUES (1, " + blobLiteral (projectId) + ", 48000.0);");
    requireRawExec (
        rawDb,
        "INSERT INTO tracks(id, name, linear_gain, pan, muted, soloed, solo_safe) VALUES ("
        + blobLiteral (trackId) + ", 'Audio 1', 1.0, 0.0, 0, 0, 0);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 15;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT height_px FROM tracks WHERE id = X'0000000000000000000000000000000A';", value).ok());
    REQUIRE (value == 0);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.tracks.size() == 1u);
    REQUIRE (readback.tracks[0].heightPx == 0);
    REQUIRE (readback.hasValidAssetClipIndirection());
}

// N7: mirrors N6's height_px migration test exactly — colour is the SAME v10-ALTER-TABLE shape,
// a new column on the existing tracks table. "Before v16" is built by running migrations only up
// to v15 and inserting a Track row that predates the colour column entirely.
TEST_CASE ("Schema v16 migration gives legacy Tracks the no-override colour default",
           "[persistence][migration][track-colour]")
{
    const auto path = makeTempBundlePath ("track-colour-v15-migration");

    std::error_code ec;
    std::filesystem::create_directories (path / "audio", ec);
    REQUIRE (! ec);

    const EntityId projectId = idFromLowByte (1);
    const EntityId trackId = idFromLowByte (10);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV15 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 15);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV15).ok());
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) VALUES (1, " + blobLiteral (projectId) + ", 48000.0);");
    requireRawExec (
        rawDb,
        "INSERT INTO tracks(id, name, linear_gain, pan, muted, soloed, solo_safe, height_px) VALUES ("
        + blobLiteral (trackId) + ", 'Audio 1', 1.0, 0.0, 0, 0, 0, 0);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 16;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT colour FROM tracks WHERE id = X'0000000000000000000000000000000A';", value).ok());
    REQUIRE (value == 0);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.tracks.size() == 1u);
    REQUIRE (readback.tracks[0].colour == yesdaw::engine::kTrackColourUnset);
    REQUIRE (readback.hasValidAssetClipIndirection());
}

// N8: punch_region is a NEW TABLE (locate-points/automation_mode pattern), not an ALTER TABLE —
// "before v17" is built by running migrations only up to v16, so the table doesn't exist yet.
TEST_CASE ("Schema v17 migration gives legacy Projects the disabled no-punch default",
           "[persistence][migration][punch-record]")
{
    const auto path = makeTempBundlePath ("punch-region-v16-migration");

    std::error_code ec;
    std::filesystem::create_directories (path / "audio", ec);
    REQUIRE (! ec);

    const EntityId projectId = idFromLowByte (1);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV16 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 16);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV16).ok());
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) VALUES (1, " + blobLiteral (projectId) + ", 48000.0);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 17;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM punch_region;", value).ok());
    REQUIRE (value == 0);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE_FALSE (readback.punchRegion.enabled);
    REQUIRE (readback.hasValidAssetClipIndirection());
}

// R3: loop_region is a NEW TABLE (the punch_region twin) — "before v18" is built by running
// migrations only up to v17, so the table doesn't exist yet.
TEST_CASE ("Schema v18 migration gives legacy Projects the disabled no-loop default",
           "[persistence][migration][loop-region]")
{
    const auto path = makeTempBundlePath ("loop-region-v17-migration");

    std::error_code ec;
    std::filesystem::create_directories (path / "audio", ec);
    REQUIRE (! ec);

    const EntityId projectId = idFromLowByte (1);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV17 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 17);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV17).ok());
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) VALUES (1, " + blobLiteral (projectId) + ", 48000.0);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 18;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM loop_region;", value).ok());
    REQUIRE (value == 0);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE_FALSE (readback.loopRegion.enabled);
    REQUIRE (readback.hasValidAssetClipIndirection());
}

// R13: a v18 bundle migrates by gaining the EMPTY bus_sends/bus_outputs tables — a legacy Bus
// keeps the historical "no sends, straight to master" default, byte-identically.
TEST_CASE ("Schema v19 migration gives legacy buses the no-sends straight-to-master default",
           "[persistence][migration][bus-routing]")
{
    const auto path = makeTempBundlePath ("bus-routing-v18-migration");

    std::error_code ec;
    std::filesystem::create_directories (path / "audio", ec);
    REQUIRE (! ec);

    const EntityId projectId = idFromLowByte (1);
    const EntityId busId = idFromLowByte (2);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV18 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 18);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV18).ok());
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) VALUES (1, " + blobLiteral (projectId) + ", 48000.0);");
    requireRawExec (
        rawDb,
        "INSERT INTO buses(id, name, linear_gain, pan, muted, soloed, solo_safe) VALUES ("
            + blobLiteral (busId) + ", 'Bus 1', 1.0, 0.0, 0, 0, 1);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 19;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM bus_sends;", value).ok());
    REQUIRE (value == 0);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM bus_outputs;", value).ok());
    REQUIRE (value == 0);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.buses.size() == 1u);
    REQUIRE (readback.buses.front().sends.empty());
    REQUIRE_FALSE (readback.buses.front().outputBusId.isValid());
    REQUIRE (readback.hasValidAssetClipIndirection());
}

// R15: v20 recreates automation_mode with the Off-capable CHECK, carrying a stored mode across
// unchanged — and the widened table accepts Off (3) where the v14 CHECK refused it.
TEST_CASE ("Schema v20 migration widens automation_mode for Off and keeps the stored mode",
           "[persistence][migration][automation-mode]")
{
    const auto path = makeTempBundlePath ("automation-mode-v19-migration");

    std::error_code ec;
    std::filesystem::create_directories (path / "audio", ec);
    REQUIRE (! ec);

    const EntityId projectId = idFromLowByte (1);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV19 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 19);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV19).ok());
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) VALUES (1, " + blobLiteral (projectId) + ", 48000.0);");
    requireRawExec (rawDb, "INSERT INTO automation_mode(slot, mode) VALUES (1, 1);");   // Touch, pre-migration
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT mode FROM automation_mode WHERE slot = 1;", value).ok());
    REQUIRE (value == 1);   // the stored Touch survived the table recreation

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.automationMode == yesdaw::engine::AutomationMode::Touch);

    // The widened CHECK accepts Off where v14's refused it.
    REQUIRE (reopened.executeSql ("UPDATE automation_mode SET mode = 3 WHERE slot = 1;").ok());
    Project offMode;
    REQUIRE (reopened.readProjectSnapshot (offMode).ok());
    REQUIRE (offMode.automationMode == yesdaw::engine::AutomationMode::Off);
}

// R16: v21 recreates automation_breakpoints with the four-curve CHECK, carrying stored rows
// across — and the widened table accepts Bezier/Log where v8's Linear/Hold CHECK refused them.
TEST_CASE ("Schema v21 migration widens breakpoint curves and keeps stored rows",
           "[persistence][migration][automation-curves]")
{
    const auto path = makeTempBundlePath ("automation-curves-v20-migration");

    std::error_code ec;
    std::filesystem::create_directories (path / "audio", ec);
    REQUIRE (! ec);

    const EntityId projectId = idFromLowByte (1);
    const EntityId trackId = idFromLowByte (2);
    const EntityId laneId = idFromLowByte (3);

    sqlite3* rawDb = nullptr;
    const std::string dbPath = utf8Path (path / "project.db");
    REQUIRE (sqlite3_open_v2 (dbPath.c_str(), &rawDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (rawDb, "PRAGMA journal_mode=WAL;");
    const auto migrationsToV20 = std::span<const SchemaMigration> (yesdaw::persistence::detail::kMigrations.data(), 20);
    REQUIRE (ProjectBundleDb::runMigrationsForTest (rawDb, 0, migrationsToV20).ok());
    requireRawExec (
        rawDb,
        "INSERT INTO project(singleton_id, id, sample_rate_hz) VALUES (1, " + blobLiteral (projectId) + ", 48000.0);");
    requireRawExec (
        rawDb,
        "INSERT INTO tracks(id, name, linear_gain, pan, muted, soloed, solo_safe) VALUES ("
            + blobLiteral (trackId) + ", 'Audio 1', 1.0, 0.0, 0, 0, 0);");
    requireRawExec (
        rawDb,
        "INSERT INTO automation_lanes(id, owner_entity, target_role, param_id) VALUES ("
            + blobLiteral (laneId) + ", " + blobLiteral (trackId) + ", 0, 1);");
    requireRawExec (
        rawDb,
        "INSERT INTO automation_breakpoints(lane_id, tick, value, curve_type) VALUES ("
            + blobLiteral (laneId) + ", 0, 1.0, 0), (" + blobLiteral (laneId) + ", 480, 0.25, 1);");
    REQUIRE (sqlite3_close (rawDb) == SQLITE_OK);

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM automation_breakpoints;", value).ok());
    REQUIRE (value == 2);   // both stored breakpoints survived the table recreation

    // The widened CHECK accepts Bezier where v8's refused it, and it reads back as Bezier.
    REQUIRE (reopened.executeSql ("UPDATE automation_breakpoints SET curve_type = 2 WHERE tick = 0;").ok());
    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.automationLanes.size() == 1u);
    REQUIRE (readback.automationLanes.front().points.size() == 2u);
    REQUIRE (readback.automationLanes.front().points.front().curveType
             == yesdaw::engine::AutomationCurveType::Bezier);
    REQUIRE (readback.automationLanes.front().points.back().curveType
             == yesdaw::engine::AutomationCurveType::Hold);
}

TEST_CASE ("Schema v11 migration adds empty locate points to a v10 bundle",
           "[persistence][migration][locate-points]")
{
    const auto path = makeTempBundlePath ("locate-points-v10-migration");
    const Project project = makeProject();

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        // N6/N7/N8/R3/R13: a fresh bundle is v19 now — the v10 simulation also strips the
        // v11-v19 artifacts, including dropping the height_px and colour COLUMNs (v15/v16 are
        // ALTER TABLEs, not new tables like v11-v14/v17-v19, so re-running their migrations on
        // reopen would otherwise fail with a duplicate column error).
        REQUIRE (db.executeSql (
            "DROP TABLE locate_points; DROP TABLE master_strip; DROP TABLE track_outputs; "
            "DROP TABLE automation_mode; ALTER TABLE tracks DROP COLUMN height_px; "
            "ALTER TABLE tracks DROP COLUMN colour; DROP TABLE punch_region; DROP TABLE loop_region; "
            "DROP TABLE bus_sends; DROP TABLE bus_outputs; "
            "DELETE FROM schema_migrations WHERE version = 11; "
            "DELETE FROM schema_migrations WHERE version = 12; "
            "DELETE FROM schema_migrations WHERE version = 13; "
            "DELETE FROM schema_migrations WHERE version = 14; "
            "DELETE FROM schema_migrations WHERE version = 15; "
            "DELETE FROM schema_migrations WHERE version = 16; "
            "DELETE FROM schema_migrations WHERE version = 17; "
            "DELETE FROM schema_migrations WHERE version = 18; "
            "DELETE FROM schema_migrations WHERE version = 19; "
            "DELETE FROM schema_migrations WHERE version = 20; "
            "DELETE FROM schema_migrations WHERE version = 21; "
            "ALTER TABLE clips DROP COLUMN stretch_factor; "   // G2.9: v22 is an ALTER TABLE too
            "DELETE FROM schema_migrations WHERE version = 22; "
            "ALTER TABLE clips DROP COLUMN fade_in_shape; ALTER TABLE clips DROP COLUMN fade_out_shape; "   // G2.10: v23
            "ALTER TABLE clips DROP COLUMN fade_in_curve; ALTER TABLE clips DROP COLUMN fade_out_curve; "
            "DELETE FROM schema_migrations WHERE version = 23; "
            "ALTER TABLE clips DROP COLUMN colour; ALTER TABLE clips DROP COLUMN muted; "   // G2.12: v24
            "DELETE FROM schema_migrations WHERE version = 24; "
            "ALTER TABLE clips DROP COLUMN reversed; "   // G2.13: v25
            "DELETE FROM schema_migrations WHERE version = 25; "
            "ALTER TABLE markers DROP COLUMN colour; "   // G2.14: v26
            "DELETE FROM schema_migrations WHERE version = 26; "
            "ALTER TABLE tracks DROP COLUMN instrument_kind; ALTER TABLE tracks DROP COLUMN instrument_state; "   // G3.1: v27
            "DELETE FROM schema_migrations WHERE version = 27; "
            "DROP TABLE midi_control_events; "   // G3.3: v28
            "DELETE FROM schema_migrations WHERE version = 28; "
            "ALTER TABLE midi_clips DROP COLUMN muted; ALTER TABLE midi_clips DROP COLUMN transpose; "   // G3.5: v29
            "ALTER TABLE midi_clips DROP COLUMN velocity_offset; ALTER TABLE midi_clips DROP COLUMN loop_length; "
            "DELETE FROM schema_migrations WHERE version = 29; "
            "DROP TABLE automation_follow_clips; DELETE FROM schema_migrations WHERE version = 34; "   // G4.6 re-pin: v34
            "DELETE FROM schema_migrations WHERE version = 33; "   // v33 rebuilds the automation tables in place
            "DROP TABLE fx_insert_sidechain; "   // G4.4 re-pin: v32 (a Compressor's sidechain key)
            "DELETE FROM schema_migrations WHERE version = 32; "
            "DROP TABLE sampler_pads; "   // G3.9 re-pin: v31 (the Sampler's pads)
            "DELETE FROM schema_migrations WHERE version = 31; "
            "DROP TABLE project_scale; "   // G3.8 re-pin: v30 (the project's key / scale row)
            "DELETE FROM schema_migrations WHERE version = 30; "
            "DROP TABLE project_write_stamp; DELETE FROM schema_migrations WHERE version = 35; "   // ADR-0068: v35
            "PRAGMA user_version = 10;").ok());
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());
    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    // N5 re-pin: reopening migrates through v14 (automation_mode).
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 11;", value).ok());
    REQUIRE (value == 1);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM locate_points;", value).ok());
    REQUIRE (value == 0);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM master_strip;", value).ok());
    REQUIRE (value == 0);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM track_outputs;", value).ok());
    REQUIRE (value == 0);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM automation_mode;", value).ok());
    REQUIRE (value == 0);

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
    for (const std::optional<Tick>& locatePoint : readback.locatePoints)
        REQUIRE_FALSE (locatePoint.has_value());
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
        // R16 re-pin: curve 2 (Bezier) is legal storage now — the quarantine boundary is 4+.
        "automation-quarantined-curve-open",
        "PRAGMA ignore_check_constraints = ON; "
        "UPDATE automation_breakpoints SET curve_type = 9 WHERE lane_id = " + blobLiteral (laneId) + " AND tick = 0; "
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

// ADR-0051 — schema v32: a Compressor's sidechain key round-trips (a Track key, a Bus key, the master's
// Compressor keyed too); unkeyed inserts write no row; deleting an insert's row cascades its key; a
// stored key on a non-Compressor, or naming no Track or Bus, refuses the bundle.
TEST_CASE ("Compressor sidechain keys round-trip through schema v32 and bad keys refuse to open",
           "[persistence][project][round-trip][sidechain]")
{
    const auto path = makeTempBundlePath ("sidechain-round-trip");

    Project project = makeProject();
    project.tracks.push_back (makeTrack (idFromLowByte (12), "Kick"));
    project.buses = { makeBus (idFromLowByte (11), "Music") };
    project.buses[0].strip.fxChain = {
        makeFxInsert (idFromLowByte (90), FxKind::Compressor),
        makeFxInsert (idFromLowByte (91), FxKind::Eq),
    };
    project.buses[0].strip.fxChain[0].sidechainSourceId = idFromLowByte (12);   // the Kick keys the Music bus
    project.tracks[0].strip.fxChain = { makeFxInsert (idFromLowByte (92), FxKind::Compressor) };
    project.tracks[0].strip.fxChain[0].sidechainSourceId = idFromLowByte (11);  // a Bus key (Audio 1 feeds master)
    project.masterStrip.fxChain = { makeFxInsert (idFromLowByte (93), FxKind::Compressor) };
    project.masterStrip.fxChain[0].sidechainSourceId = idFromLowByte (12);
    REQUIRE (project.hasValidAssetClipIndirection());

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        sqlite3_int64 rows = 0;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM fx_insert_sidechain;", rows).ok());
        REQUIRE (rows == 3);   // the EQ writes none
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());
    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.buses[0].strip.fxChain == project.buses[0].strip.fxChain);
    REQUIRE (readback.tracks[0].strip.fxChain == project.tracks[0].strip.fxChain);
    REQUIRE (readback.masterStrip.fxChain == project.masterStrip.fxChain);
    REQUIRE (readback.buses[0].strip.fxChain[0].sidechainSourceId == idFromLowByte (12));
    REQUIRE_FALSE (readback.buses[0].strip.fxChain[1].sidechainSourceId.isValid());

    // An insert row's deletion takes its key with it (ON DELETE CASCADE).
    REQUIRE (reopened.executeSql ("DELETE FROM fx_insert_params WHERE insert_id = X'0000000000000000000000000000005c'; "
                                  "DELETE FROM fx_inserts WHERE id = X'0000000000000000000000000000005c';").ok());
    sqlite3_int64 rows = 0;
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM fx_insert_sidechain;", rows).ok());
    REQUIRE (rows == 2);

    // A key on the EQ, or naming no Track or Bus, is a bundle that does not open as a Project.
    REQUIRE (reopened.executeSql ("INSERT INTO fx_insert_sidechain(insert_id, source_entity) "
                                  "VALUES (X'0000000000000000000000000000005b', X'0000000000000000000000000000000c');").ok());
    Project refused;
    REQUIRE_FALSE (reopened.readProjectSnapshot (refused).ok());
    REQUIRE (reopened.executeSql ("DELETE FROM fx_insert_sidechain WHERE insert_id = X'0000000000000000000000000000005b'; "
                                  "UPDATE fx_insert_sidechain SET source_entity = X'00000000000000000000000000000063' "
                                  "WHERE insert_id = X'0000000000000000000000000000005d';").ok());
    REQUIRE_FALSE (reopened.readProjectSnapshot (refused).ok());

    // A tampered row typed as TEXT (sixteen characters pass the length CHECK) is refused by the stored-type law.
    REQUIRE (reopened.executeSql ("UPDATE fx_insert_sidechain SET source_entity = '0123456789abcdef' "
                                  "WHERE insert_id = X'0000000000000000000000000000005d';").ok());
    REQUIRE (reopened.validateStoredProjectSemantics().status == BundleStatus::SemanticInvalid);
}

// v33 (repair of ADR-0047 and R14): an instrument-parameter lane (role 6) and a BUS-owned send-level lane
// both save and reopen. The v8 CHECK refused role 6 at write, and the open-time owner law demanded a Track
// for every send-level lane — a project with either could be made and played but not saved or reopened.
TEST_CASE ("Instrument-parameter and bus send-level automation lanes save and reopen (schema v33)",
           "[persistence][project][round-trip][automation][v33]")
{
    const auto path = makeTempBundlePath ("automation-v33-round-trip");

    Project project = makeProject();
    project.tracks[0].instrumentKind = yesdaw::engine::TrackInstrumentKind::SimpleSynth;
    std::uint32_t synthParam = 0;
    while (! yesdaw::engine::instrumentKindAcceptsParameterId (project.tracks[0].instrumentKind, synthParam))
    {
        ++synthParam;
        REQUIRE (synthParam < 64u);
    }
    project.buses = { makeBus (idFromLowByte (11), "Verb"), makeBus (idFromLowByte (12), "Room") };
    project.buses[0].sends = { yesdaw::engine::SendRow { idFromLowByte (80), idFromLowByte (12),
                                                         yesdaw::engine::SendTap::PostFader, 0.5f } };
    project.automationLanes = {
        makeAutomationLane (idFromLowByte (70), project.tracks[0].id, AutomationTargetRole::InstrumentParam, synthParam),
        makeAutomationLane (idFromLowByte (71), project.buses[0].id, AutomationTargetRole::SendLevel, 0),
    };
    REQUIRE (project.hasValidAssetClipIndirection());

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());
    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.automationLanes == project.automationLanes);
    REQUIRE (readback.tracks[0].instrumentKind == yesdaw::engine::TrackInstrumentKind::SimpleSynth);

    // The open-time owner law still bites: an instrument lane on a Bus, or a send-level lane on no strip.
    REQUIRE (reopened.executeSql ("UPDATE automation_lanes SET owner_entity = X'0000000000000000000000000000000b' "
                                  "WHERE id = X'00000000000000000000000000000046';").ok());
    REQUIRE (reopened.validateStoredProjectSemantics().status == BundleStatus::SemanticInvalid);
    REQUIRE (reopened.executeSql ("UPDATE automation_lanes SET owner_entity = X'0000000000000000000000000000000a' "
                                  "WHERE id = X'00000000000000000000000000000046'; "
                                  "UPDATE automation_lanes SET owner_entity = X'00000000000000000000000000000063' "
                                  "WHERE id = X'00000000000000000000000000000047';").ok());
    REQUIRE (reopened.validateStoredProjectSemantics().status == BundleStatus::SemanticInvalid);
}

// v33 rebuilds both automation tables with foreign keys ON: every lane and every point of a v32 bundle
// survives (a naive drop of the parent table would cascade-delete the points).
TEST_CASE ("Schema v33 migration keeps every automation lane and point", "[persistence][migration][automation][v33]")
{
    const auto path = makeTempBundlePath ("automation-v33-migration");
    Project project = makeProject();
    project.automationLanes = {
        makeAutomationLane (idFromLowByte (70), project.tracks[0].id, AutomationTargetRole::TrackFader, 1),
        makeAutomationLane (idFromLowByte (71), project.tracks[0].id, AutomationTargetRole::TrackPan, 1),
    };
    project.automationLanes[0].points.push_back (AutomationBreakpoint { 30720, 0.5, AutomationCurveType::Bezier });
    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        // Back to v32's shape: the v8 CHECK on the lanes table, and the migration row gone.
        REQUIRE (db.executeSql (
            "CREATE TABLE lanes_old (id BLOB PRIMARY KEY CHECK (length(id) = 16), "
            "owner_entity BLOB NOT NULL CHECK (length(owner_entity) = 16), "
            "target_role INTEGER NOT NULL CHECK (target_role IN (0, 1, 2, 3, 4, 5)), "
            "param_id INTEGER NOT NULL CHECK (param_id >= 0), UNIQUE(owner_entity, target_role, param_id)); "
            "INSERT INTO lanes_old SELECT id, owner_entity, target_role, param_id FROM automation_lanes; "
            "CREATE TABLE points_old (lane_id BLOB NOT NULL CHECK (length(lane_id) = 16), tick INTEGER NOT NULL CHECK (tick >= 0), "
            "value REAL NOT NULL CHECK(value>=0 AND value<=1), curve_type INTEGER NOT NULL CHECK(curve_type IN (0,1,2,3)), "
            "PRIMARY KEY(lane_id, tick), FOREIGN KEY(lane_id) REFERENCES lanes_old(id) ON UPDATE RESTRICT ON DELETE CASCADE); "
            "INSERT INTO points_old SELECT lane_id, tick, value, curve_type FROM automation_breakpoints; "
            "DROP TABLE automation_breakpoints; DROP TABLE automation_lanes; "
            "ALTER TABLE lanes_old RENAME TO automation_lanes; ALTER TABLE points_old RENAME TO automation_breakpoints; "
            "CREATE INDEX automation_lanes_owner_entity_idx ON automation_lanes(owner_entity); "
            "DELETE FROM schema_migrations WHERE version = 33; "
            "DROP TABLE automation_follow_clips; DELETE FROM schema_migrations WHERE version = 34; "   // v34 replays too
            "DROP TABLE project_write_stamp; DELETE FROM schema_migrations WHERE version = 35; "   // and v35
            "PRAGMA user_version = 32;").ok());
        sqlite3_int64 points = 0;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM automation_breakpoints;", points).ok());
        REQUIRE (points == 5);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());
    sqlite3_int64 value = 0;
    REQUIRE (reopened.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM automation_breakpoints;", value).ok());
    REQUIRE (value == 5);
    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    REQUIRE (readback.automationLanes == project.automationLanes);
}

// G4.6 / ADR-0052 — schema v34: Write (4) is a stored mode, and "automation follows clips" persists in its own
// one-row table (a missing row is off); a v33 bundle keeps its stored mode across the rebuild.
TEST_CASE ("Write mode and automation-follows-clips round-trip through schema v34", "[persistence][project][round-trip][automation][v34]")
{
    const auto path = makeTempBundlePath ("automation-v34");
    Project project = makeProject();
    project.automationMode = yesdaw::engine::AutomationMode::Write;
    project.automationFollowsClips = true;
    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
    }
    {
        ProjectBundleDb reopened;
        REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());
        Project readback;
        REQUIRE (reopened.readProjectSnapshot (readback).ok());
        REQUIRE (readback.automationMode == yesdaw::engine::AutomationMode::Write);
        REQUIRE (readback.automationFollowsClips);

        // Back to v33's shape with Off stored: the migration keeps the mode, and follow is off (no row).
        REQUIRE (reopened.executeSql (
            "DROP TABLE automation_follow_clips; "
            "CREATE TABLE mode_old (slot INTEGER PRIMARY KEY CHECK (slot = 1), mode INTEGER NOT NULL CHECK (mode >= 0 AND mode <= 3)); "
            "INSERT INTO mode_old VALUES (1, 3); DROP TABLE automation_mode; ALTER TABLE mode_old RENAME TO automation_mode; "
            "DELETE FROM schema_migrations WHERE version = 34; "
            "DROP TABLE project_write_stamp; DELETE FROM schema_migrations WHERE version = 35; PRAGMA user_version = 33;").ok());
    }
    ProjectBundleDb migrated;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, migrated).ok());
    sqlite3_int64 value = 0;
    REQUIRE (migrated.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    Project readback;
    REQUIRE (migrated.readProjectSnapshot (readback).ok());
    REQUIRE (readback.automationMode == yesdaw::engine::AutomationMode::Off);
    REQUIRE_FALSE (readback.automationFollowsClips);
    // A mode past Write is refused at open.
    REQUIRE_FALSE (migrated.executeSql ("UPDATE automation_mode SET mode = 5 WHERE slot = 1;").ok());   // the CHECK
}

// ADR-0068 §5 — schema v35: the bundle's write stamp. A fresh bundle starts at 0 with no unresolved recovery question;
// every snapshot write advances it by one; a v34 bundle migrates to stamp 0 and is otherwise unchanged.
TEST_CASE ("ADR-0068 schema v35 adds the write stamp; every snapshot write advances it; a v34 bundle lands at 0",
           "[persistence][migration][autosave][v35]")
{
    const auto path = makeTempBundlePath ("write-stamp-v35");
    const Project project = makeProject();
    {
        ProjectBundleDb db = openFreshBundle (path);
        sqlite3_int64 value = -1;
        REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM project_write_stamp;", value).ok());
        REQUIRE (value == 1);
        std::int64_t stamp = -1;
        REQUIRE (db.projectWriteStamp (stamp).ok());
        REQUIRE (stamp == 0);
        REQUIRE (db.queryInt64 ("SELECT unresolved_snapshot_stamp IS NULL FROM project_write_stamp;", value).ok());
        REQUIRE (value == 1);
        for (int i = 0; i < 3; ++i)
            REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (db.projectWriteStamp (stamp).ok());
        REQUIRE (stamp == 3);
        // Back to v34's shape: no stamp table, no v35 migration row.
        REQUIRE (db.executeSql ("DROP TABLE project_write_stamp; DELETE FROM schema_migrations WHERE version = 35; "
                                "PRAGMA user_version = 34;").ok());
    }
    ProjectBundleDb migrated;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, migrated).ok());
    sqlite3_int64 value = 0;
    REQUIRE (migrated.queryInt64 ("PRAGMA user_version;", value).ok());
    REQUIRE (value == kCodeSchemaVersion);
    std::int64_t stamp = -1;
    REQUIRE (migrated.projectWriteStamp (stamp).ok());
    REQUIRE (stamp == 0);   // the migration lands the stamp at 0 and writes no snapshot
    REQUIRE (migrated.queryInt64 ("SELECT unresolved_snapshot_stamp IS NULL FROM project_write_stamp;", value).ok());
    REQUIRE (value == 1);
    Project readback;
    REQUIRE (migrated.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
}

// ADR-0068 §5 — the stamp shares the snapshot's transaction: a stamp that cannot be written (a refusing trigger on this
// connection, or a missing row) fails the whole snapshot - the rows roll back with it, and no transaction is left open.
TEST_CASE ("ADR-0068 the write stamp shares the snapshot's transaction; a refused stamp rolls the snapshot back",
           "[persistence][autosave][stamp-shared-transaction]")
{
    const auto path = makeTempBundlePath ("write-stamp-transaction");
    const Project first = makeProject();
    Project second = makeProject();
    second.tracks[0].strip.name = "Renamed";
    ProjectBundleDb db = openFreshBundle (path);
    REQUIRE (db.writeProjectSnapshot (first).ok());
    writeProjectAssetFiles (path, first);
    std::int64_t stamp = -1;

    SECTION ("the stamp UPDATE fails")
    {
        REQUIRE (db.executeSql ("CREATE TEMP TRIGGER stamp_refuses BEFORE UPDATE ON project_write_stamp "
                                "BEGIN SELECT RAISE(ABORT, 'injected'); END;").ok());
        REQUIRE_FALSE (db.writeProjectSnapshot (second).ok());
        REQUIRE (db.projectWriteStamp (stamp).ok());
        REQUIRE (stamp == 1);
        Project readback;
        REQUIRE (db.readProjectSnapshot (readback).ok());
        REQUIRE (readback.tracks[0].strip.name == "Audio 1");   // the rows rolled back with the stamp
        REQUIRE (db.executeSql ("BEGIN IMMEDIATE; ROLLBACK;").ok());   // no transaction was left open
        REQUIRE (db.executeSql ("DROP TRIGGER temp.stamp_refuses;").ok());
        REQUIRE (db.writeProjectSnapshot (second).ok());
        REQUIRE (db.projectWriteStamp (stamp).ok());
        REQUIRE (stamp == 2);
        REQUIRE (db.readProjectSnapshot (readback).ok());
        REQUIRE (readback.tracks[0].strip.name == "Renamed");
    }
    SECTION ("the stamp row is missing")
    {
        REQUIRE (db.executeSql ("DELETE FROM project_write_stamp;").ok());
        REQUIRE_FALSE (db.writeProjectSnapshot (second).ok());
        Project readback;
        REQUIRE (db.readProjectSnapshot (readback).ok());
        REQUIRE (readback.tracks[0].strip.name == "Audio 1");
    }
}

namespace {
// The stamp as project.db itself holds it: a fresh connection to a file with no -wal beside it.
std::int64_t rawWriteStamp (const std::filesystem::path& databaseFile)
{
    sqlite3* raw = nullptr;
    REQUIRE (sqlite3_open_v2 (utf8Path (databaseFile).c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
    sqlite3_stmt* stmt = nullptr;
    REQUIRE (sqlite3_prepare_v2 (raw, "SELECT write_count FROM project_write_stamp WHERE singleton_id = 1;", -1, &stmt, nullptr)
             == SQLITE_OK);
    REQUIRE (sqlite3_step (stmt) == SQLITE_ROW);
    const std::int64_t value = sqlite3_column_int64 (stmt, 0);
    sqlite3_finalize (stmt);
    sqlite3_close (raw);
    return value;
}
} // namespace

// ADR-0068 §5 — an autosave snapshot carries its source's stamp, in its own project.db (read with no WAL beside it), and
// the source is not advanced by its autosave. The override lands inside the snapshot's transaction and checkpoint: the
// bytes of project.db, copied while the connection is still open, already hold it; a failed override leaves nothing.
TEST_CASE ("ADR-0068 an autosave snapshot carries its source's write stamp, durable in its project.db",
           "[persistence][autosave][stamp-override-durable]")
{
    const auto source = makeTempBundlePath ("write-stamp-source");
    const Project project = makeProject();
    {
        ProjectBundleDb db = openFreshBundle (source);
        for (int i = 0; i < 7; ++i)
            REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (source, project);
        REQUIRE (yesdaw::persistence::writeAutosaveSnapshot (db, project).ok());
        std::int64_t stamp = -1;
        REQUIRE (db.projectWriteStamp (stamp).ok());
        REQUIRE (stamp == 7);
    }
    const auto snapshotDb = yesdaw::persistence::autosaveSnapshotPath (source) / "project.db";
    std::error_code ec;
    for (const char* suffix : { "-wal", "-shm" })
    {
        std::filesystem::path sidecar = snapshotDb;
        sidecar += suffix;
        std::filesystem::remove (sidecar, ec);
    }
    REQUIRE (rawWriteStamp (snapshotDb) == 7);

    const auto live = makeTempBundlePath ("write-stamp-override");
    ProjectBundleDb overridden = openFreshBundle (live);
    REQUIRE (overridden.writeProjectSnapshot (project, 42).ok());
    const auto copied = makeTempBundlePath ("write-stamp-override-copy");
    std::filesystem::create_directories (copied);
    const std::vector<std::uint8_t> bytes = readBytes (live / "project.db");
    writeBytes (copied / "project.db", std::span<const std::uint8_t> (bytes.data(), bytes.size()));
    REQUIRE (rawWriteStamp (copied / "project.db") == 42);

    Project renamed = project;
    renamed.tracks[0].strip.name = "Renamed";
    REQUIRE (overridden.executeSql ("CREATE TEMP TRIGGER stamp_refuses BEFORE UPDATE ON project_write_stamp "
                                    "BEGIN SELECT RAISE(ABORT, 'injected'); END;").ok());
    REQUIRE_FALSE (overridden.writeProjectSnapshot (renamed, 7).ok());
    std::int64_t stamp = -1;
    REQUIRE (overridden.projectWriteStamp (stamp).ok());
    REQUIRE (stamp == 42);
    writeProjectAssetFiles (live, project);
    Project readback;
    REQUIRE (overridden.readProjectSnapshot (readback).ok());
    REQUIRE (readback.tracks[0].strip.name == "Audio 1");
}

TEST_CASE ("ADR-0069 recovery reads preserve published source bytes and inventory",
           "[persistence][autosave][recovery-source-inventory]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("recovery-source-inventory");
    const auto project = makeProject();
    {
        auto db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (writeAutosaveSnapshot (db, project).ok());
    }
    const auto snapshot = autosaveSnapshotPath (path);
    const std::vector<std::uint8_t> extra { 0x11u, 0x22u, 0x33u };
    const auto orphanHash = hashBytes (extra);
    const auto orphan = detail::assetRelativePathForHash (orphanHash);
    const auto pending = detail::assetTempRelativePathForHash (orphanHash);
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openExistingBundle (snapshot, db).ok());
        sqlite3_int64 rowId = 0;
        REQUIRE (db.recordPendingFsOp ({ PendingFsOpKind::StageAsset, pending, orphan, orphanHash }, rowId).ok());
    }
    writeBytes (snapshot / orphan, extra);
    writeBytes (snapshot / pending, extra);
    writeBytes (snapshot / "audio" / "abandoned.tmp", extra);
    writeBytes (snapshot / ".trash" / "old.asset", extra);
    writeBytes (snapshot / "unknown" / "leave-me.bin", extra);
    bool succeeds = true;
    bool readOnly = false;
    bool legacy = false;
    std::string reason;
    SECTION ("current schema") {}
    SECTION ("read-only metadata")
    {
        readOnly = true;
        std::filesystem::permissions (snapshot / "project.db", std::filesystem::perms::owner_read,
                                       std::filesystem::perm_options::replace);
    }
    SECTION ("v34 migrates in scratch to stamp zero")
    {
        legacy = true;
        sqlite3* raw = nullptr;
        REQUIRE (sqlite3_open_v2 (utf8Path (snapshot / "project.db").c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
        requireRawExec (raw, "DROP TABLE project_write_stamp; DELETE FROM schema_migrations WHERE version=35; PRAGMA user_version=34;");
        sqlite3_int64 version = 0;
        REQUIRE (detail::queryInt64 (raw, "PRAGMA user_version;", version).ok());
        REQUIRE (version == 34);
        REQUIRE (detail::queryInt64 (raw, "SELECT count(*) FROM sqlite_master WHERE name='project_write_stamp';", version).ok());
        REQUIRE (version == 0);
        REQUIRE (sqlite3_close (raw) == SQLITE_OK);
    }
    SECTION ("missing source audio refuses without reconciling anything")
    {
        succeeds = false;
        reason = detail::utf8Path (storedAssetPathForHash (snapshot, project.assets.back().contentHash));
        REQUIRE (std::filesystem::remove (storedAssetPathForHash (snapshot, project.assets.back().contentHash)));
    }
    SECTION ("damaged source audio refuses without reconciling anything")
    {
        succeeds = false;
        reason = detail::utf8Path (storedAssetPathForHash (snapshot, project.assets.back().contentHash));
        writeBytes (storedAssetPathForHash (snapshot, project.assets.back().contentHash), extra);
    }
    const auto before = sourceInventory (snapshot);
    const auto permissions = std::filesystem::status (snapshot / "project.db").permissions();
    Project sentinel = project;
    sentinel.tracks.front().strip.name = "output must survive a refusal";
    Project recovered = sentinel;
    std::int64_t stamp = -1;
    detail::assetHashCallsForTest = 0;
    const auto result = readAutosaveSnapshot (path, recovered, &stamp);
    INFO (result.message);
    REQUIRE (result.ok() == succeeds);
    if (succeeds)
    {
        requireSameProjectSurface (recovered, project);
        REQUIRE (stamp == (legacy ? 0 : 1));
        REQUIRE (detail::assetHashCallsForTest == project.assets.size());
    }
    else
    {
        requireSameProjectSurface (recovered, sentinel);
        REQUIRE (stamp == -1);
        REQUIRE (result.bundle.message.find (reason) != std::string::npos);
    }
    REQUIRE (sourceInventory (snapshot) == before);
    REQUIRE (std::filesystem::status (snapshot / "project.db").permissions() == permissions);
    if (readOnly)
        std::filesystem::permissions (snapshot / "project.db", std::filesystem::perms::owner_write,
                                       std::filesystem::perm_options::add);
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 recovery uses committed WAL rows and stamp without touching source sidecars",
           "[persistence][autosave][recovery-source-wal]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("recovery-source-wal");
    auto project = makeProject();
    {
        auto db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (writeAutosaveSnapshot (db, project).ok());
    }
    const auto snapshot = autosaveSnapshotPath (path);
    const auto originalDb = readBytes (snapshot / "project.db");
    sqlite3* raw = nullptr;
    REQUIRE (sqlite3_open_v2 (utf8Path (snapshot / "project.db").c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
    int persistWal = 1;
    REQUIRE (sqlite3_file_control (raw, "main", SQLITE_FCNTL_PERSIST_WAL, &persistWal) == SQLITE_OK);
    requireRawExec (raw, "PRAGMA wal_autocheckpoint=0; BEGIN IMMEDIATE; UPDATE tracks SET name='committed WAL'; "
                        "UPDATE project_write_stamp SET write_count=41; COMMIT;");
    REQUIRE (readBytes (snapshot / "project.db") == originalDb);
    REQUIRE (std::filesystem::file_size (snapshot / "project.db-wal") > 0);
    REQUIRE (std::filesystem::exists (snapshot / "project.db-shm"));
    project.tracks.front().strip.name = "committed WAL";
    bool uncommitted = false;
    bool readOnly = false;
    SECTION ("committed WAL") {}
    SECTION ("read-only DB and WAL copies become writable only in scratch")
    {
        readOnly = true;
    }
    SECTION ("an actual spilled uncommitted tail is ignored")
    {
        uncommitted = true;
        const auto committedSize = std::filesystem::file_size (snapshot / "project.db-wal");
        requireRawExec (raw, "PRAGMA cache_size=1; BEGIN IMMEDIATE; UPDATE project_write_stamp SET write_count=99; "
                            "UPDATE tracks SET name='" + std::string (32000, 'x') + "';");
        REQUIRE (sqlite3_db_cacheflush (raw) == SQLITE_OK);
        REQUIRE (std::filesystem::file_size (snapshot / "project.db-wal") > committedSize);
    }
    // Freeze the actual DB/WAL bytes before closing the fixture writer. A live Windows SQLite
    // connection byte-locks SHM, so it cannot serve as the inventory-readable published source.
    // The restored pair still contains WAL-only committed rows and the real uncommitted tail;
    // persisted SHM is deliberately stale derived state that recovery must neither use nor edit.
    const auto walBytes = readBytes (snapshot / "project.db-wal");
    if (uncommitted)
        requireRawExec (raw, "ROLLBACK;");
    REQUIRE (sqlite3_close (raw) == SQLITE_OK);
    REQUIRE (std::filesystem::exists (snapshot / "project.db-shm"));
    writeBytes (snapshot / "project.db", originalDb);
    writeBytes (snapshot / "project.db-wal", walBytes);
    if (readOnly)
        for (const char* name : { "project.db", "project.db-wal" })
            std::filesystem::permissions (snapshot / name, std::filesystem::perms::owner_read,
                                           std::filesystem::perm_options::replace);
    const auto before = sourceInventory (snapshot);
    Project recovered;
    std::int64_t stamp = -1;
    detail::assetHashCallsForTest = 0;
    const auto result = readAutosaveSnapshot (path, recovered, &stamp);
    INFO (result.message);
    REQUIRE (result.ok());
    requireSameProjectSurface (recovered, project);
    REQUIRE (stamp == 41);
    REQUIRE (detail::assetHashCallsForTest == project.assets.size());
    REQUIRE (sourceInventory (snapshot) == before);
    if (readOnly)
        for (const char* name : { "project.db", "project.db-wal" })
        {
            REQUIRE ((std::filesystem::status (snapshot / name).permissions() & std::filesystem::perms::owner_write)
                     == std::filesystem::perms::none);
            std::filesystem::permissions (snapshot / name, std::filesystem::perms::owner_write,
                                           std::filesystem::perm_options::add);
        }
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 recovery rebuilds legacy rows only in metadata scratch",
           "[persistence][autosave][recovery-source-legacy]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("recovery-source-v3");
    const auto snapshot = autosaveSnapshotPath (path);
    const auto asset = makeAsset (idFromLowByte (2), 1000);
    writeBytes (storedAssetPathForHash (snapshot, asset.contentHash), assetBytesForId (asset.id));
    writeBytes (snapshot / "audio" / "orphan.asset", { });
    writeBytes (snapshot / "peaks" / "untouched.cache", assetBytesForId (asset.id));
    sqlite3* raw = nullptr;
    REQUIRE (sqlite3_open_v2 (utf8Path (snapshot / "project.db").c_str(), &raw,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) == SQLITE_OK);
    requireRawExec (raw, kSchemaV1Sql);
    requireRawExec (raw, kSchemaV2Sql);
    requireRawExec (raw, kSchemaV3Sql);
    requireRawExec (raw, "INSERT INTO schema_migrations(version, app_build) VALUES (1,'legacy'),(2,'legacy'),(3,'legacy'); "
                        "PRAGMA application_id=1497715505; PRAGMA user_version=3;");
    requireRawExec (raw, "INSERT INTO project(singleton_id,id,sample_rate_hz) VALUES (1," + blobLiteral (idFromLowByte (1))
                        + ",48000.0); INSERT INTO assets(id,content_hash,frames,sample_rate_hz,channels,relative_path) VALUES ("
                        + blobLiteral (asset.id) + "," + blobLiteral (asset.contentHash) + ",1000,48000.0,2,'"
                        + detail::assetRelativePathForHash (asset.contentHash) + "');");
    requireRawExec (raw, "INSERT INTO clips(id,asset_id,timeline_start,timeline_length,src_offset,src_len,gain,fade_in,fade_out,time_base) VALUES ("
                        + blobLiteral (idFromLowByte (4)) + "," + blobLiteral (asset.id) + ",0,15360,100,900,0.75,16,32,1);");
    REQUIRE (sqlite3_close (raw) == SQLITE_OK);
    const auto before = sourceInventory (snapshot);
    Project recovered;
    std::int64_t stamp = -1;
    detail::assetHashCallsForTest = 0;
    const auto result = readAutosaveSnapshot (path, recovered, &stamp);
    INFO (result.message);
    REQUIRE (result.ok());
    REQUIRE (stamp == 0);
    REQUIRE (recovered.assets == std::vector<Asset> { asset });
    REQUIRE (recovered.tracks.size() == 1);
    REQUIRE (recovered.tracks.front().id == kDefaultAudioTrackId);
    REQUIRE (recovered.clips.size() == 1);
    REQUIRE (recovered.clips.front().trackId == kDefaultAudioTrackId);
    REQUIRE (recovered.clips.front().srcOffset == 100);
    REQUIRE (recovered.clips.front().srcLen == 900);
    REQUIRE (recovered.clips.front().gain == Approx (0.75f));
    REQUIRE (detail::assetHashCallsForTest == 1);
    REQUIRE (sourceInventory (snapshot) == before);
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 recovery retains database and filesystem refusals without publishing outputs",
           "[persistence][autosave][recovery-source-refusal]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("recovery-source-refusal");
    const auto project = makeProject();
    {
        auto db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (writeAutosaveSnapshot (db, project).ok());
    }
    const auto snapshot = autosaveSnapshotPath (path);
    std::string sql;
    std::string reason;
    SECTION ("semantic ranges")
    {
        sql = "UPDATE clips SET src_len=901 WHERE id=" + blobLiteral (project.clips.front().id) + ";";
    }
    SECTION ("foreign keys")
    {
        sql = "PRAGMA foreign_keys=OFF; UPDATE clips SET asset_id=zeroblob(16);";
    }
    SECTION ("storage types")
    {
        sql = "UPDATE clips SET src_offset=0.5;";
        reason = "storage type";
    }
    SECTION ("canonical path spelling")
    {
        sql = "UPDATE assets SET relative_path='audio/./' || substr(relative_path,7);";
        reason = "relative path";
    }
    SECTION ("missing stamp row does not publish decoded project")
    {
        sql = "DELETE FROM project_write_stamp;";
    }
    SECTION ("rollback journal is explicitly refused")
    {
        writeBytes (snapshot / "project.db-journal", assetBytesForId (project.id));
        reason = "rollback journal";
    }
    SECTION ("unreadable WAL is not treated as absent")
    {
        REQUIRE (std::filesystem::create_directory (snapshot / "project.db-wal"));
        reason = "project.db-wal";
    }
    if (! sql.empty())
    {
        sqlite3* raw = nullptr;
        REQUIRE (sqlite3_open_v2 (utf8Path (snapshot / "project.db").c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
        requireRawExec (raw, sql);
        REQUIRE (sqlite3_close (raw) == SQLITE_OK);
    }
    writeBytes (snapshot / "audio" / "orphan.asset", { });
    writeBytes (snapshot / "audio" / "abandoned.tmp", { });
    const auto before = sourceInventory (snapshot);
    Project sentinel = project;
    sentinel.tracks.front().strip.name = "do not publish partial output";
    Project recovered = sentinel;
    std::int64_t stamp = -73;
    const auto result = readAutosaveSnapshot (path, recovered, &stamp);
    REQUIRE_FALSE (result.ok());
    REQUIRE (result.status != AutosaveStatus::NoAutosave);
    INFO (result.message);
    REQUIRE (result.bundle.message.find ("last.yesdaw") != std::string::npos);
    REQUIRE_FALSE (result.bundle.message.empty());
    if (! reason.empty())
        REQUIRE (result.bundle.message.find (reason) != std::string::npos);
    REQUIRE (stamp == -73);
    requireSameProjectSurface (recovered, sentinel);
    REQUIRE (sourceInventory (snapshot) == before);
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 owned recovery scratch is removed on success refusal and unwinding",
           "[persistence][autosave][recovery-source-scratch]")
{
    using namespace yesdaw::persistence;
    const auto protectedBundle = makeTempBundlePath ("scratch-protected");
    const auto protectedSource = autosaveSnapshotPath (protectedBundle);
    std::filesystem::create_directories (protectedSource);
    std::filesystem::path owned;
    SECTION ("successful and refused reads remove their owned metadata")
    {
        for (const bool succeeds : { true, false })
        {
            autosave_detail::RecoveryScratch scratch;
            REQUIRE (scratch.create (std::filesystem::temp_directory_path(), protectedBundle, protectedSource).ok());
            owned = scratch.path;
            REQUIRE (std::filesystem::is_directory (owned));
            writeBytes (owned / "project.db", assetBytesForId (idFromLowByte (1)));
            writeBytes (owned / "project.db-wal", assetBytesForId (idFromLowByte (2)));
            const auto result = scratch.finish (succeeds ? autosave_detail::ok()
                : autosave_detail::bundleError (detail::semanticInvalid ("original refusal")));
            REQUIRE (result.ok() == succeeds);
            if (! succeeds)
                REQUIRE (result.bundle.message == "original refusal");
            REQUIRE_FALSE (std::filesystem::exists (owned));
        }
    }
    SECTION ("exception unwind removes a partially copied database")
    {
        try
        {
            autosave_detail::RecoveryScratch scratch;
            REQUIRE (scratch.create (std::filesystem::temp_directory_path(), protectedBundle, protectedSource).ok());
            owned = scratch.path;
            writeBytes (owned / "project.db", assetBytesForId (idFromLowByte (1)));
            throw 1;
        }
        catch (int) {}
        REQUIRE_FALSE (owned.empty());
        REQUIRE_FALSE (std::filesystem::exists (owned));
    }
#if defined(_WIN32)
    SECTION ("Windows deletion denial preserves the validation and cleanup errors")
    {
        for (const bool succeeds : { true, false })
        {
            autosave_detail::RecoveryScratch scratch;
            REQUIRE (scratch.create (std::filesystem::temp_directory_path(), protectedBundle, protectedSource).ok());
            owned = scratch.path;
            const auto metadata = owned / "project.db";
            writeBytes (metadata, assetBytesForId (idFromLowByte (1)));
            // Even an assertion failure must release the real OS lock and remove the owned fixture.
            struct HeldScratch
            {
                std::filesystem::path path;
                HANDLE handle = INVALID_HANDLE_VALUE;
                ~HeldScratch()
                {
                    if (handle != INVALID_HANDLE_VALUE)
                        (void) CloseHandle (handle);
                    std::error_code ec;
                    if (! path.empty())
                        std::filesystem::remove_all (path, ec);
                }
            } held { owned, CreateFileW (metadata.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };
            // Omit FILE_SHARE_DELETE deliberately: remove_all must encounter a genuine sharing violation.
            CAPTURE (GetLastError());
            REQUIRE (held.handle != INVALID_HANDLE_VALUE);
            const auto result = scratch.finish (succeeds ? autosave_detail::ok()
                : autosave_detail::bundleError (detail::semanticInvalid ("original validation refusal")));
            REQUIRE_FALSE (result.ok());
            REQUIRE (result.status == (succeeds ? AutosaveStatus::FilesystemError : AutosaveStatus::BundleError));
            REQUIRE (result.message.find ("remove recovery scratch directory failed") != std::string::npos);
            REQUIRE (result.message.find (detail::utf8Path (owned)) != std::string::npos);
            if (! succeeds)
            {
                REQUIRE (result.bundle.status == BundleStatus::SemanticInvalid);
                REQUIRE (result.message.find ("original validation refusal") != std::string::npos);
                REQUIRE (result.bundle.message.find ("original validation refusal") != std::string::npos);
                REQUIRE (result.bundle.message.find ("remove recovery scratch directory failed") != std::string::npos);
                REQUIRE (result.bundle.message.find (detail::utf8Path (owned)) != std::string::npos);
            }
            CHECK (scratch.path == owned); // Keep ownership for a destructor retry after failure.
            REQUIRE (std::filesystem::exists (metadata));
            const BOOL closed = CloseHandle (held.handle);
            if (closed)
                held.handle = INVALID_HANDLE_VALUE;
            REQUIRE (closed != FALSE);
            std::error_code cleanup;
            std::filesystem::remove_all (owned, cleanup);
            REQUIRE (! cleanup);
            REQUIRE_FALSE (std::filesystem::exists (owned));
            held.path.clear();
        }
    }
#endif
    std::filesystem::remove_all (protectedBundle);
}

#if defined(_WIN32)
TEST_CASE ("ADR-0069 recovery cleanup failure never publishes an older slot",
           "[persistence][autosave][recovery-cleanup-selection]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("recovery-cleanup-selection");
    static thread_local HANDLE held = INVALID_HANDLE_VALUE;
    static thread_local std::filesystem::path heldPath;
    struct Cleanup
    {
        std::filesystem::path bundle;
        ~Cleanup()
        {
            autosave_detail::beforeRecoveryCleanupForTest = nullptr;
            if (held != INVALID_HANDLE_VALUE)
                (void) CloseHandle (held);
            held = INVALID_HANDLE_VALUE;
            std::error_code ec;
            if (! heldPath.empty())
                std::filesystem::remove_all (heldPath, ec);
            heldPath.clear();
            std::filesystem::remove_all (bundle, ec);
        }
    } cleanup { path };
    auto db = openFreshBundle (path);
    const auto project = makeProject();
    REQUIRE (db.writeProjectSnapshot (project).ok());
    writeProjectAssetFiles (path, project);
    REQUIRE (writeAutosaveSnapshot (db, project).ok());
    const auto live = autosaveSnapshotPath (path);
    const auto previous = autosave_detail::previousSnapshotPath (path);
    std::filesystem::copy (live, previous, std::filesystem::copy_options::recursive);
    {
        sqlite3* raw = nullptr;
        REQUIRE (sqlite3_open_v2 (utf8Path (live / "project.db").c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
        const std::unique_ptr<sqlite3, decltype (&sqlite3_close)> ownedDb (raw, sqlite3_close);
        requireRawExec (raw, "UPDATE tracks SET name='newer live'; UPDATE project_write_stamp SET write_count=42;");
    }
    bool restore = false;
    bool invalid = false;
    SECTION ("public read refuses without publishing the older stamp") {}
    SECTION ("public Restore refuses before changing target rows or output") { restore = true; }
    SECTION ("validation and cleanup errors survive instead of selecting valid previous") { invalid = true; }
    if (invalid)
        REQUIRE (std::filesystem::remove (storedAssetPathForHash (live, project.assets.back().contentHash)));
    const auto before = sourceInventory (autosaveDirectory (path));
    autosave_detail::beforeRecoveryCleanupForTest = [] (const std::filesystem::path& source,
                                                       const std::filesystem::path& scratch) {
        if (source.filename() != "last.yesdaw")
            return;
        heldPath = scratch;
        // Induce the real OS sharing violation after SQLite closes; no synthetic error result.
        held = CreateFileW ((scratch / "project.db").c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        REQUIRE (held != INVALID_HANDLE_VALUE);
    };
    auto sentinel = project;
    sentinel.tracks.front().strip.name = "output sentinel";
    auto recovered = sentinel;
    std::int64_t stamp = -17;
    const auto result = restore ? restoreAutosaveSnapshot (db, recovered)
                                : readAutosaveSnapshot (path, recovered, &stamp);
    INFO (result.message);
    CHECK_FALSE (result.ok());
    CHECK (result.status == (invalid ? AutosaveStatus::BundleError : AutosaveStatus::FilesystemError));
    CHECK (result.message.find ("remove recovery scratch directory failed") != std::string::npos);
    CHECK (result.message.find (detail::utf8Path (heldPath)) != std::string::npos);
    if (invalid)
        CHECK (result.message.find (detail::utf8Path (storedAssetPathForHash (live, project.assets.back().contentHash)))
               != std::string::npos);
    CHECK (stamp == -17);
    requireSameProjectSurface (recovered, sentinel);
    REQUIRE (sourceInventory (autosaveDirectory (path)) == before);
    Project target;
    REQUIRE (db.readProjectSnapshot (target).ok());
    requireSameProjectSurface (target, project);
    std::int64_t targetStamp = 0;
    REQUIRE (db.projectWriteStamp (targetStamp).ok());
    REQUIRE (targetStamp == 1);
    std::optional<std::int64_t> unresolved;
    REQUIRE (db.unresolvedSnapshotStamp (unresolved).ok());
    REQUIRE_FALSE (unresolved.has_value());
    // Once the actual denial is gone, the same public reader recovers the newer live snapshot.
    REQUIRE (CloseHandle (held) != FALSE);
    held = INVALID_HANDLE_VALUE;
    autosave_detail::beforeRecoveryCleanupForTest = nullptr;
    std::filesystem::remove_all (heldPath);
    heldPath.clear();
    if (! invalid)
    {
        REQUIRE (readAutosaveSnapshot (path, recovered, &stamp).ok());
        REQUIRE (stamp == 42);
        REQUIRE (recovered.tracks.front().strip.name == "newer live");
    }
}
#endif

TEST_CASE ("ADR-0069 recovery scratch keeps copied metadata and SQLite sidecars private",
           "[persistence][autosave][recovery-scratch-privacy]")
{
    using namespace yesdaw::persistence;
    const auto fixture = makeTempBundlePath ("scratch-privacy");
    struct FixtureCleanup
    {
        std::filesystem::path root;
#if defined(_WIN32)
        std::filesystem::path deniedParent;
        PSECURITY_DESCRIPTOR permissive = nullptr;
#endif
        ~FixtureCleanup()
        {
#if defined(_WIN32)
            if (! deniedParent.empty())
                (void) SetFileSecurityW (deniedParent.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                        permissive);
            if (permissive != nullptr)
                (void) LocalFree (permissive);
#endif
            std::error_code ec;
            if (! root.empty())
                std::filesystem::remove_all (root, ec);
        }
    } cleanup;
    cleanup.root = fixture;
    const auto bundle = fixture / "project.yesdaw";
    const auto source = autosaveSnapshotPath (bundle);
    const auto tempParent = fixture / "permissive-temp";
    const auto project = makeProject();
    {
        auto db = openFreshBundle (bundle);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundle, project);
        REQUIRE (writeAutosaveSnapshot (db, project).ok());
    }
    bool populate = false;
    bool denyCreation = false;
    SECTION ("scratch is private before the first byte is copied") {}
    SECTION ("copied database and actual SQLite WAL and SHM remain private") { populate = true; }
#if defined(_WIN32)
    SECTION ("a real create-subdirectory denial refuses before copying") { denyCreation = true; }
    // This descriptor is confined to the new empty fixture parent; no ambient temp ACL is changed.
    REQUIRE (ConvertStringSecurityDescriptorToSecurityDescriptorW (L"D:P(A;OICI;FA;;;WD)", SDDL_REVISION_1,
                                                                    &cleanup.permissive, nullptr) != FALSE);
    SECURITY_ATTRIBUTES attributes { static_cast<DWORD> (sizeof (SECURITY_ATTRIBUTES)), cleanup.permissive, FALSE };
    REQUIRE (CreateDirectoryW (tempParent.c_str(), &attributes) != FALSE);
    struct LocalDescriptor
    {
        PSECURITY_DESCRIPTOR value = nullptr;
        ~LocalDescriptor() { if (value != nullptr) (void) LocalFree (value); }
    };
    {
        LocalDescriptor parent;
        PACL dacl = nullptr;
        REQUIRE (GetNamedSecurityInfoW (tempParent.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                        nullptr, nullptr, &dacl, nullptr, &parent.value) == ERROR_SUCCESS);
        REQUIRE (dacl != nullptr);
        REQUIRE (dacl->AceCount == 1);
        void* rawAce = nullptr;
        REQUIRE (GetAce (dacl, 0, &rawAce) != FALSE);
        const auto* ace = static_cast<const ACCESS_ALLOWED_ACE*> (rawAce);
        REQUIRE (ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE);
        REQUIRE (IsWellKnownSid (const_cast<DWORD*> (&ace->SidStart), WinWorldSid) != FALSE);
        REQUIRE ((ace->Mask & FILE_ALL_ACCESS) == FILE_ALL_ACCESS);
    }
    if (denyCreation)
    {
        LocalDescriptor denied;
        REQUIRE (ConvertStringSecurityDescriptorToSecurityDescriptorW (L"D:P(D;;0x00000004;;;WD)(A;OICI;FA;;;WD)",
                    SDDL_REVISION_1, &denied.value, nullptr) != FALSE);
        cleanup.deniedParent = tempParent; // Restore even if an assertion interrupts the denial case.
        REQUIRE (SetFileSecurityW (tempParent.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                   denied.value) != FALSE);
    }
    HANDLE token = nullptr;
    REQUIRE (OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &token) != FALSE);
    alignas(TOKEN_USER) std::array<std::uint8_t, sizeof (TOKEN_USER) + SECURITY_MAX_SID_SIZE> userStorage {};
    DWORD userSize = 0;
    const BOOL gotUser = GetTokenInformation (token, TokenUser, userStorage.data(),
                                             static_cast<DWORD> (userStorage.size()), &userSize);
    const DWORD userError = GetLastError();
    const BOOL closedToken = CloseHandle (token);
    CAPTURE (userError);
    REQUIRE (gotUser != FALSE);
    REQUIRE (closedToken != FALSE);
    const PSID userSid = reinterpret_cast<const TOKEN_USER*> (userStorage.data())->User.Sid;
#else
    REQUIRE (std::filesystem::create_directory (tempParent));
    std::filesystem::permissions (tempParent, std::filesystem::perms::all, std::filesystem::perm_options::replace);
    REQUIRE ((std::filesystem::status (tempParent).permissions() & std::filesystem::perms::all)
             == std::filesystem::perms::all);
#endif
#if defined(__APPLE__)
    // An owned parent with a real inheritable Everyone grant reproduces the mode-only gap.
    // Do not change the ambient temp ACL or claim another-account execution.
    struct OwnedAcl
    {
        acl_t value = nullptr;
        ~OwnedAcl() { if (value != nullptr) (void) ::acl_free (value); }
    } parentAcl { ::acl_init (1) };
    REQUIRE (parentAcl.value != nullptr);
    acl_entry_t grant = nullptr;
    REQUIRE (::acl_create_entry (&parentAcl.value, &grant) == 0);
    REQUIRE (::acl_set_tag_type (grant, ACL_EXTENDED_ALLOW) == 0);
    const auto* everyone = ::getgrnam ("everyone");
    REQUIRE (everyone != nullptr);
    uuid_t everyoneId {};
    REQUIRE (::mbr_gid_to_uuid (everyone->gr_gid, everyoneId) == 0);
    REQUIRE (::acl_set_qualifier (grant, everyoneId) == 0);
    acl_permset_t permissions = nullptr;
    REQUIRE (::acl_get_permset (grant, &permissions) == 0);
    REQUIRE (::acl_add_perm (permissions, ACL_READ_DATA) == 0);
    REQUIRE (::acl_add_perm (permissions, ACL_EXECUTE) == 0);
    acl_flagset_t flags = nullptr;
    REQUIRE (::acl_get_flagset_np (grant, &flags) == 0);
    REQUIRE (::acl_add_flag_np (flags, ACL_ENTRY_FILE_INHERIT) == 0);
    REQUIRE (::acl_add_flag_np (flags, ACL_ENTRY_DIRECTORY_INHERIT) == 0);
    REQUIRE (::acl_set_file (tempParent.c_str(), ACL_TYPE_EXTENDED, parentAcl.value) == 0);
    const auto aclText = [] (const std::filesystem::path& item) {
        REQUIRE (std::filesystem::exists (item));
        OwnedAcl acl { ::acl_get_file (item.c_str(), ACL_TYPE_EXTENDED) };
        if (acl.value == nullptr)
        {
            REQUIRE (errno == ENOENT); // Existing object with no ACL property on Darwin.
            return std::string ("<no ACL property>");
        }
        char* raw = ::acl_to_text (acl.value, nullptr);
        const std::unique_ptr<char, decltype (&::acl_free)> text (raw, ::acl_free);
        REQUIRE (raw != nullptr);
        return std::string (raw);
    };
    const auto parentAclBefore = aclText (tempParent);
    const auto sourceAclBefore = aclText (source);
    const auto modeOnly = tempParent / "mode-only-characterization";
    REQUIRE (::mkdir (modeOnly.c_str(), S_IRWXU) == 0);
    {
        struct stat mode {};
        REQUIRE (::lstat (modeOnly.c_str(), &mode) == 0);
        REQUIRE ((mode.st_mode & 0077) == 0);
        OwnedAcl inherited { ::acl_get_file (modeOnly.c_str(), ACL_TYPE_EXTENDED) };
        REQUIRE (inherited.value != nullptr);
        acl_entry_t entry = nullptr;
        REQUIRE (::acl_get_entry (inherited.value, ACL_FIRST_ENTRY, &entry) == 0);
        acl_tag_t tag {};
        REQUIRE (::acl_get_tag_type (entry, &tag) == 0);
        REQUIRE (tag == ACL_EXTENDED_ALLOW);
        REQUIRE (::acl_get_flagset_np (entry, &flags) == 0);
        REQUIRE (::acl_get_flag_np (flags, ACL_ENTRY_INHERITED) == 1);
        REQUIRE (::acl_get_permset (entry, &permissions) == 0);
        REQUIRE (::acl_get_perm_np (permissions, ACL_READ_DATA) == 1);
        REQUIRE (::acl_get_perm_np (permissions, ACL_EXECUTE) == 1);
    }
    REQUIRE (std::filesystem::remove (modeOnly));
#endif
    const auto sourceBefore = sourceInventory (source);
    autosave_detail::RecoveryScratch scratch;
    const auto result = scratch.create (tempParent, bundle, source);
    INFO (result.message);
    if (denyCreation)
    {
        REQUIRE_FALSE (result.ok());
        REQUIRE (result.status == AutosaveStatus::FilesystemError);
        REQUIRE (result.message.find (detail::utf8Path (std::filesystem::canonical (tempParent))) != std::string::npos);
        REQUIRE (scratch.path.empty());
        REQUIRE (std::filesystem::is_empty (tempParent));
    }
    else
    {
        REQUIRE (result.ok());
        const auto requirePrivate = [&] (const std::filesystem::path& item) {
            CAPTURE (item);
            REQUIRE (std::filesystem::exists (item));
#if defined(_WIN32)
            // Inspect the actual DACL, not the read-only attribute. This is not a second-account access test.
            LocalDescriptor descriptor;
            PACL dacl = nullptr;
            REQUIRE (GetNamedSecurityInfoW (item.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                            nullptr, nullptr, &dacl, nullptr, &descriptor.value) == ERROR_SUCCESS);
            REQUIRE (dacl != nullptr);
            REQUIRE (dacl->AceCount > 0);
            if (item == scratch.path)
            {
                SECURITY_DESCRIPTOR_CONTROL control = 0;
                DWORD revision = 0;
                REQUIRE (GetSecurityDescriptorControl (descriptor.value, &control, &revision) != FALSE);
                CHECK ((control & SE_DACL_PROTECTED) != 0);
            }
            for (DWORD i = 0; i < dacl->AceCount; ++i)
            {
                void* rawAce = nullptr;
                REQUIRE (GetAce (dacl, i, &rawAce) != FALSE);
                const auto* ace = static_cast<const ACCESS_ALLOWED_ACE*> (rawAce);
                REQUIRE (ace->Header.AceType == ACCESS_ALLOWED_ACE_TYPE);
                CHECK (EqualSid (const_cast<DWORD*> (&ace->SidStart), userSid) != FALSE);
                if (item == scratch.path)
                    CHECK ((ace->Header.AceFlags & (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE))
                           == (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE));
            }
#else
            // POSIX confidentiality comes from the owner-only traversal boundary. A copied 0644
            // file inside this directory is still inaccessible to another ordinary user.
            struct stat directory {};
            REQUIRE (::stat (scratch.path.c_str(), &directory) == 0);
            CHECK (directory.st_uid == ::geteuid());
            CHECK ((directory.st_mode & 0077) == 0);
#if defined(__APPLE__)
            // The directory must block traversal even if a copied file has permissive mode bits.
            // Inspect it for every actual DB/WAL/SHM check; mode 0700 alone is insufficient here.
            OwnedAcl directoryAcl { ::acl_get_file (scratch.path.c_str(), ACL_TYPE_EXTENDED) };
            if (directoryAcl.value == nullptr)
                CHECK (errno == ENOENT);
            else
            {
                acl_entry_t entry = nullptr;
                errno = 0;
                const int entryResult = ::acl_get_entry (directoryAcl.value, ACL_FIRST_ENTRY, &entry);
                const int entryError = errno;
                CHECK (entryResult == -1);
                CHECK (entryError == EINVAL); // Darwin's empty-ACL iterator result, not POSIX/Linux's 0.
            }
#endif
            if (item != scratch.path)
            {
                REQUIRE (item.parent_path() == scratch.path);
                REQUIRE (std::filesystem::symlink_status (item).type() == std::filesystem::file_type::regular);
            }
#endif
        };
        requirePrivate (scratch.path);
        if (populate)
        {
            // Same metadata-copy operation as RecoveryReader; never copy a source SHM.
            REQUIRE (std::filesystem::copy_file (source / "project.db", scratch.path / "project.db"));
            requirePrivate (scratch.path / "project.db");
            {
                sqlite3* raw = nullptr;
                const int opened = sqlite3_open_v2 (utf8Path (scratch.path / "project.db").c_str(), &raw,
                                                    SQLITE_OPEN_READWRITE, nullptr);
                const std::unique_ptr<sqlite3, decltype (&sqlite3_close)> ownedDb (raw, sqlite3_close);
                REQUIRE (opened == SQLITE_OK);
                requireRawExec (raw, "PRAGMA journal_mode=WAL; PRAGMA wal_autocheckpoint=0; "
                                    "UPDATE tracks SET name='private scratch sidecars';");
                REQUIRE (std::filesystem::file_size (scratch.path / "project.db-wal") > 0);
                requirePrivate (scratch.path / "project.db-wal");
                requirePrivate (scratch.path / "project.db-shm");
            }
        }
        REQUIRE (scratch.finish (autosave_detail::ok()).ok());
        REQUIRE (std::filesystem::is_empty (tempParent));
    }
    REQUIRE (sourceInventory (source) == sourceBefore);
#if defined(__APPLE__)
    REQUIRE (aclText (tempParent) == parentAclBefore);
    REQUIRE (aclText (source) == sourceAclBefore);
#endif
#if defined(_WIN32)
    if (! cleanup.deniedParent.empty())
    {
        REQUIRE (SetFileSecurityW (tempParent.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                   cleanup.permissive) != FALSE);
        cleanup.deniedParent.clear();
    }
#endif
    std::filesystem::remove_all (fixture);
    REQUIRE_FALSE (std::filesystem::exists (fixture));
    cleanup.root.clear();
}

TEST_CASE ("ADR-0069 recovery scratch refuses source and target containment before creating anything",
           "[persistence][autosave][recovery-scratch-containment]")
{
    using namespace yesdaw::persistence;
    const auto fixture = makeTempBundlePath ("scratch-containment");
    const auto bundle = fixture / "project.yesdaw";
    auto source = autosaveSnapshotPath (bundle);
    const auto outside = fixture / "outside";
    std::filesystem::create_directories (source / "child");
    std::filesystem::create_directories (bundle / "audio");
    std::filesystem::create_directories (outside);
    auto scratchRoot = outside;
    bool refuses = true;
    std::string reason = "inside the project or snapshot";
    SECTION ("source root") { scratchRoot = source; }
    SECTION ("source child") { scratchRoot = source / "child"; }
    SECTION ("target root") { scratchRoot = bundle; }
    SECTION ("target child") { scratchRoot = bundle / "audio"; }
    SECTION ("canonical parent traversal alias") { scratchRoot = source / "child" / ".."; }
#if ! defined(_WIN32)
    SECTION ("directory symlink alias")
    {
        scratchRoot = outside / "source-alias";
        std::error_code ec;
        std::filesystem::create_directory_symlink (source, scratchRoot, ec);
        INFO (ec.message());
        REQUIRE (! ec);
    }
#endif
    SECTION ("source outside the target tree is protected independently")
    {
        source = fixture / "detached-snapshot";
        std::filesystem::create_directory (source);
        scratchRoot = source;
    }
    SECTION ("an outside sibling is permitted") { refuses = false; }
    SECTION ("protected source inspection errors are retained")
    {
        source = fixture / "missing-source";
        reason = "missing-source";
    }
    const auto before = sourceInventory (fixture);
    const auto rootTime = std::filesystem::last_write_time (scratchRoot);
    autosave_detail::RecoveryScratch scratch;
    const auto result = scratch.create (scratchRoot, bundle, source);
    INFO (result.message);
    REQUIRE (result.ok() != refuses);
    if (refuses)
    {
        REQUIRE (result.status == AutosaveStatus::FilesystemError);
        REQUIRE (result.message.find (reason) != std::string::npos);
        REQUIRE (scratch.path.empty());
        REQUIRE (sourceInventory (fixture) == before);
        // Compare the native times without Catch trying to print libc++'s __int128 clock representation.
        REQUIRE ((std::filesystem::last_write_time (scratchRoot) == rootTime));
    }
    else
    {
        REQUIRE (std::filesystem::is_directory (scratch.path));
        REQUIRE (std::filesystem::equivalent (scratch.path.parent_path(), outside));
        REQUIRE (scratch.finish (autosave_detail::ok()).ok());
        REQUIRE (sourceInventory (fixture) == before);
    }
    std::filesystem::remove_all (fixture);
}

TEST_CASE ("ADR-0069 recovery validates live then previous and preserves both sources",
           "[persistence][autosave][recovery-source-selection]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("recovery-source-selection");
    const auto project = makeProject();
    {
        auto db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (path, project);
        REQUIRE (writeAutosaveSnapshot (db, project).ok());
    }
    const auto live = autosaveSnapshotPath (path);
    const auto previous = autosave_detail::previousSnapshotPath (path);
    std::filesystem::copy (live, previous, std::filesystem::copy_options::recursive);
    // Distinguish the selected slot by actual rows and stamp, not only the reported path.
    {
        sqlite3* raw = nullptr;
        REQUIRE (sqlite3_open_v2 (utf8Path (previous / "project.db").c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
        requireRawExec (raw, "UPDATE tracks SET name='previous'; UPDATE project_write_stamp SET write_count=29;");
        REQUIRE (sqlite3_close (raw) == SQLITE_OK);
    }
    writeBytes (live / "audio" / "live-orphan.asset", { });
    writeBytes (previous / "audio" / "previous-orphan.asset", { });
    bool usePrevious = false;
    bool bothInvalid = false;
    SECTION ("valid live is preferred") {}
    SECTION ("missing live uses previous")
    {
        usePrevious = true;
        std::filesystem::remove_all (live);
    }
    SECTION ("audio-invalid live uses valid previous")
    {
        usePrevious = true;
        REQUIRE (std::filesystem::remove (storedAssetPathForHash (live, project.assets.back().contentHash)));
    }
    SECTION ("both invalid retain the live and previous causes")
    {
        bothInvalid = true;
        REQUIRE (std::filesystem::remove (storedAssetPathForHash (live, project.assets.back().contentHash)));
        REQUIRE (std::filesystem::remove (previous / "project.db"));
    }
    const auto before = sourceInventory (autosaveDirectory (path));
    Project sentinel = project;
    sentinel.tracks.front().strip.name = "sentinel";
    Project recovered = sentinel;
    std::int64_t stamp = -17;
    detail::assetHashCallsForTest = 0;
    const auto result = readAutosaveSnapshot (path, recovered, &stamp);
    INFO (result.message);
    if (bothInvalid)
    {
        REQUIRE_FALSE (result.ok());
        REQUIRE (result.status != AutosaveStatus::NoAutosave);
        REQUIRE (result.bundle.status == BundleStatus::IntegrityFailed);
        REQUIRE (result.bundle.message.find (detail::utf8Path (storedAssetPathForHash (live, project.assets.back().contentHash)))
                 != std::string::npos);
        REQUIRE (result.bundle.message.find (detail::utf8Path (previous / "project.db")) != std::string::npos);
        requireSameProjectSurface (recovered, sentinel);
        REQUIRE (stamp == -17);
    }
    else
    {
        REQUIRE (result.ok());
        Project expected = project;
        if (usePrevious)
            expected.tracks.front().strip.name = "previous";
        requireSameProjectSurface (recovered, expected);
        REQUIRE (stamp == (usePrevious ? 29 : 1));
        if (! usePrevious)
            REQUIRE (detail::assetHashCallsForTest == project.assets.size());
    }
    REQUIRE (sourceInventory (autosaveDirectory (path)) == before);
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 only genuinely absent published slots mean no autosave",
           "[persistence][autosave][recovery-source-absence]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("recovery-source-absence");
    const auto live = autosaveSnapshotPath (path);
    std::filesystem::create_directories (autosaveDirectory (path));
    bool absent = true;
    SECTION ("neither published slot exists") {}
    SECTION ("even a valid last.tmp is never selected")
    {
        auto temporary = openFreshBundle (autosave_detail::tempSnapshotPath (path));
        REQUIRE (temporary.writeProjectSnapshot (makeProject()).ok());
        writeProjectAssetFiles (temporary.bundlePath(), makeProject());
    }
    SECTION ("present live directory without project.db is invalid")
    {
        absent = false;
        std::filesystem::create_directory (live);
    }
    SECTION ("stat failure from a non-directory slot is invalid")
    {
        absent = false;
        writeBytes (live, assetBytesForId (idFromLowByte (1)));
    }
    const auto before = sourceInventory (path);
    const auto sentinel = makeProject();
    Project recovered = sentinel;
    std::int64_t stamp = -5;
    const auto result = readAutosaveSnapshot (path, recovered, &stamp);
    REQUIRE_FALSE (result.ok());
    REQUIRE ((result.status == AutosaveStatus::NoAutosave) == absent);
    if (! absent)
    {
        REQUIRE (result.status == AutosaveStatus::FilesystemError);
        REQUIRE (result.message.find ("last.yesdaw") != std::string::npos);
    }
    REQUIRE (stamp == -5);
    requireSameProjectSurface (recovered, sentinel);
    REQUIRE (sourceInventory (path) == before);
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 restore validates freshly once and leaves its published source untouched",
           "[persistence][autosave][recovery-source-restore]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("recovery-source-restore");
    const auto project = makeProject();
    auto db = openFreshBundle (path);
    REQUIRE (db.writeProjectSnapshot (project).ok());
    writeProjectAssetFiles (path, project);
    REQUIRE (writeAutosaveSnapshot (db, project).ok());
    REQUIRE (db.setUnresolvedSnapshotStamp (1).ok());
    const auto live = autosaveSnapshotPath (path);
    writeBytes (live / "audio" / "orphan.asset", { });
    Project promptProject;
    REQUIRE (readAutosaveSnapshot (path, promptProject).ok());
    bool succeeds = true;
    SECTION ("unchanged snapshot validates once and restores") {}
    SECTION ("source damage after the prompt is validated afresh before any target mutation")
    {
        succeeds = false;
        // Remove only the source name; target audio remains valid. A cached selection would accept it.
        REQUIRE (std::filesystem::remove (storedAssetPathForHash (live, project.assets.back().contentHash)));
    }
    const auto before = sourceInventory (path);
    const auto sourceBefore = sourceInventory (live);
    Project sentinel = project;
    sentinel.tracks.front().strip.name = "restore sentinel";
    Project restored = sentinel;
    detail::assetHashCallsForTest = 0;
    const auto result = restoreAutosaveSnapshot (db, restored);
    INFO (result.message);
    REQUIRE (result.ok() == succeeds);
    if (succeeds)
    {
        requireSameProjectSurface (restored, project);
        REQUIRE (detail::assetHashCallsForTest == project.assets.size());
    }
    else
    {
        requireSameProjectSurface (restored, sentinel);
        REQUIRE (sourceInventory (path) == before);
        REQUIRE (result.bundle.message.find (detail::utf8Path (storedAssetPathForHash (live, project.assets.back().contentHash)))
                 != std::string::npos);
    }
    REQUIRE (sourceInventory (live) == sourceBefore);
    std::optional<std::int64_t> marker;
    REQUIRE (db.unresolvedSnapshotStamp (marker).ok());
    REQUIRE (marker == (succeeds ? std::optional<std::int64_t> {} : std::optional<std::int64_t> { 1 }));
    db = {};
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 autosaves share immutable asset bytes and release their links on retirement",
           "[persistence][autosave][cheap][inode-identity][link-count][no-asset-bytes]")
{
    const auto path = makeTempBundlePath ("linked-autosave");
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };
    for (std::uint8_t n = 10; n < 20; ++n)
        project.assets.push_back (makeAsset (idFromLowByte (n)));
    // Opening a new database sweeps unreferenced files, so populate its rows before the files.
    auto db = openFreshBundle (path);
    REQUIRE (db.writeProjectSnapshot (project).ok());
    writeProjectAssetFiles (path, project);

    for (int i = 0; i < 200; ++i)
    {
        yesdaw::persistence::detail::assetHashCallsForTest = 0;
        yesdaw::persistence::autosave_detail::assetBytesCopiedForTest = 0;
        const auto result = yesdaw::persistence::writeAutosaveSnapshot (db, project);
        REQUIRE (result.ok());
        REQUIRE (result.linked == 10);
        REQUIRE (result.copied == 0);
        REQUIRE (yesdaw::persistence::detail::assetHashCallsForTest == 0);
        REQUIRE (yesdaw::persistence::autosave_detail::assetBytesCopiedForTest == 0);
        for (const auto& asset : project.assets)
        {
            const auto source = yesdaw::persistence::storedAssetPathForHash (path, asset.contentHash);
            const auto snapshot = yesdaw::persistence::storedAssetPathForHash (
                yesdaw::persistence::autosaveSnapshotPath (path), asset.contentHash);
            REQUIRE (std::filesystem::equivalent (source, snapshot));
            REQUIRE (std::filesystem::hard_link_count (source) == 2);
        }
    }
    REQUIRE (yesdaw::persistence::discardAutosaveSnapshot (path).ok());
    for (const auto& asset : project.assets)
        REQUIRE (std::filesystem::hard_link_count (
            yesdaw::persistence::storedAssetPathForHash (path, asset.contentHash)) == 1);
    db = {};
    std::filesystem::remove_all (path);
}

namespace {
struct AutosaveHooksScope
{
    AutosaveHooksScope()
    {
        yesdaw::persistence::detail::assetHashCallsForTest = 0;
        yesdaw::persistence::autosave_detail::assetBytesCopiedForTest = 0;
        yesdaw::persistence::autosave_detail::failLinkForTest.reset();
        yesdaw::persistence::autosave_detail::afterCarryForTest = nullptr;
    }
    ~AutosaveHooksScope()
    {
        yesdaw::persistence::autosave_detail::failLinkForTest.reset();
        yesdaw::persistence::autosave_detail::afterCarryForTest = nullptr;
    }
};
} // namespace

TEST_CASE ("ADR-0069 one failed link copies and hashes only that asset, while recovery still hashes every asset",
           "[persistence][autosave][cheap][copy-fallback]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("autosave-copy-fallback");
    auto db = openFreshBundle (path);
    const auto project = makeProject();
    REQUIRE (db.writeProjectSnapshot (project).ok());
    writeProjectAssetFiles (path, project);
    AutosaveHooksScope hooks;
    autosave_detail::failLinkForTest = project.assets.front().contentHash;
    const auto result = writeAutosaveSnapshot (db, project);
    REQUIRE (result.ok());
    REQUIRE (result.linked == 1);
    REQUIRE (result.copied == 1);
    REQUIRE (detail::assetHashCallsForTest == 1);
    REQUIRE (autosave_detail::assetBytesCopiedForTest == assetBytesForId (project.assets.front().id).size());
    const auto snapshot = autosaveSnapshotPath (path);
    REQUIRE_FALSE (std::filesystem::equivalent (storedAssetPathForHash (path, project.assets.front().contentHash),
                                               storedAssetPathForHash (snapshot, project.assets.front().contentHash)));
    Project recovered;
    detail::assetHashCallsForTest = 0;
    REQUIRE (readAutosaveSnapshot (path, recovered).ok());
    REQUIRE (detail::assetHashCallsForTest >= project.assets.size());
    requireSameProjectSurface (recovered, project);
    // A damaged linked inode is still rejected on recovery and ordinary project open.
    writeBytes (storedAssetPathForHash (snapshot, project.assets.back().contentHash), { });
    REQUIRE_FALSE (readAutosaveSnapshot (path, recovered).ok());
    db = {};
    REQUIRE_FALSE (ProjectBundleDb::openExistingBundle (path, db).ok());
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 write validation rejects a detached link, a bad copied hash, or altered stored rows",
           "[persistence][autosave][cheap][carry-validation]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("autosave-carry-validation");
    auto db = openFreshBundle (path);
    const auto project = makeProject();
    REQUIRE (db.writeProjectSnapshot (project).ok());
    writeProjectAssetFiles (path, project);
    REQUIRE (writeAutosaveSnapshot (db, project).ok());
    const auto temporary = autosave_detail::tempSnapshotPath (path);
    {
        auto snapshot = openFreshBundle (temporary);
        REQUIRE (snapshot.writeProjectSnapshot (project).ok());
    }
    AutosaveHooksScope hooks;
    SECTION ("an equal-content copy cannot stand in for a recorded link")
    {
        std::vector<autosave_detail::AssetCarry> carries;
        REQUIRE (autosave_detail::carryProjectAssets (path, project, temporary, carries).ok());
        const auto file = storedAssetPathForHash (temporary, project.assets.front().contentHash);
        REQUIRE (std::filesystem::remove (file));
        writeBytes (file, assetBytesForId (project.assets.front().id));
        REQUIRE_FALSE (autosave_detail::SnapshotValidator::validate (temporary, path, carries).ok());
    }
    SECTION ("a fallback copy must match its hash")
    {
        autosave_detail::failLinkForTest = project.assets.front().contentHash;
        std::vector<autosave_detail::AssetCarry> carries;
        REQUIRE (autosave_detail::carryProjectAssets (path, project, temporary, carries).ok());
        writeBytes (storedAssetPathForHash (temporary, project.assets.front().contentHash), { });
        REQUIRE_FALSE (autosave_detail::SnapshotValidator::validate (temporary, path, carries).ok());
        REQUIRE (detail::assetHashCallsForTest == 1);
    }
    SECTION ("every stored row needs a carry")
    {
        std::vector<autosave_detail::AssetCarry> carries;
        REQUIRE (autosave_detail::carryProjectAssets (path, project, temporary, carries).ok());
        carries.pop_back();
        REQUIRE_FALSE (autosave_detail::SnapshotValidator::validate (temporary, path, carries).ok());
    }
    SECTION ("canonical stored paths are still required")
    {
        std::vector<autosave_detail::AssetCarry> carries;
        REQUIRE (autosave_detail::carryProjectAssets (path, project, temporary, carries).ok());
        {
            ProjectBundleDb snapshot;
            REQUIRE (ProjectBundleDb::openExistingBundle (temporary, snapshot).ok());
            REQUIRE (snapshot.executeSql ("UPDATE assets SET relative_path = '../elsewhere.asset';").ok());
        }
        REQUIRE_FALSE (autosave_detail::SnapshotValidator::validate (temporary, path, carries).ok());
    }
    SECTION ("semantic validation remains enabled")
    {
        std::vector<autosave_detail::AssetCarry> carries;
        REQUIRE (autosave_detail::carryProjectAssets (path, project, temporary, carries).ok());
        {
            ProjectBundleDb snapshot;
            REQUIRE (ProjectBundleDb::openExistingBundle (temporary, snapshot).ok());
            REQUIRE (snapshot.executeSql ("PRAGMA ignore_check_constraints=ON; UPDATE assets SET channels=0;").ok());
        }
        REQUIRE_FALSE (autosave_detail::SnapshotValidator::validate (temporary, path, carries).ok());
    }
    Project recovered;
    REQUIRE (readAutosaveSnapshot (path, recovered).ok());
    requireSameProjectSurface (recovered, project);
    db = {};
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 ten 50 MB assets autosave in under 100 ms on the local disk",
           "[.][persistence][autosave][hardware-cost]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("autosave-hardware-cost");
    auto db = openFreshBundle (path);
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };
    std::vector<std::uint8_t> bytes (50'000'000, 0x51);
    for (std::uint8_t n = 10; n < 20; ++n)
    {
        auto asset = makeAsset (idFromLowByte (n));
        bytes.front() = n;
        asset.contentHash = hashBytes (bytes);
        const auto file = storedAssetPathForHash (path, asset.contentHash);
        writeBytes (file, bytes);
        REQUIRE (detail::flushFileToDisk (file).ok());
        project.assets.push_back (asset);
    }
    REQUIRE (db.writeProjectSnapshot (project).ok());
    const auto start = std::chrono::steady_clock::now();
    const auto result = writeAutosaveSnapshot (db, project);
    const auto elapsed = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - start).count();
    CAPTURE (elapsed);
    REQUIRE (result.ok());
    REQUIRE (result.linked == 10);
    REQUIRE (result.copied == 0);
    REQUIRE (elapsed < 100.0);
    db = {};
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 a damaged fallback copy cannot replace the previous good autosave",
           "[persistence][autosave][cheap][no-bad-publish]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("autosave-bad-publish");
    auto db = openFreshBundle (path);
    auto project = makeProject();
    REQUIRE (db.writeProjectSnapshot (project).ok());
    writeProjectAssetFiles (path, project);
    REQUIRE (writeAutosaveSnapshot (db, project).ok());
    const auto snapshotDbBytes = readBytes (autosaveSnapshotPath (path) / "project.db");
    AutosaveHooksScope hooks;
    autosave_detail::failLinkForTest = project.assets.front().contentHash;
    autosave_detail::afterCarryForTest = [] (const std::filesystem::path& temporary) {
        writeBytes (storedAssetPathForHash (temporary, makeProject().assets.front().contentHash), { });
    };
    project.tracks.front().strip.name = "Must not publish";
    REQUIRE_FALSE (writeAutosaveSnapshot (db, project).ok());
    REQUIRE (readBytes (autosaveSnapshotPath (path) / "project.db") == snapshotDbBytes);
    Project recovered;
    REQUIRE (readAutosaveSnapshot (path, recovered).ok());
    requireSameProjectSurface (recovered, makeProject());
    db = {};
    std::filesystem::remove_all (path);
}

TEST_CASE ("ADR-0069 restoring over a different inode does not overwrite its other links",
           "[persistence][autosave][cheap][restore-preserves-shared-bytes]")
{
    using namespace yesdaw::persistence;
    const auto path = makeTempBundlePath ("autosave-restore-shared-bytes");
    auto db = openFreshBundle (path);
    const auto project = makeProject();
    REQUIRE (db.writeProjectSnapshot (project).ok());
    writeProjectAssetFiles (path, project);
    REQUIRE (writeAutosaveSnapshot (db, project).ok());
    const auto target = storedAssetPathForHash (path, project.assets.front().contentHash);
    REQUIRE (std::filesystem::remove (target));
    const std::vector<std::uint8_t> damaged { 1, 2, 3 };
    writeBytes (target, damaged);
    const auto sibling = path / "older-snapshot-audio.asset";
    std::filesystem::create_hard_link (target, sibling);
    Project recovered;
    REQUIRE (restoreAutosaveSnapshot (db, recovered).ok());
    requireSameProjectSurface (recovered, project);
    REQUIRE (readBytes (sibling) == damaged);
    REQUIRE (readBytes (target) == assetBytesForId (project.assets.front().id));
    REQUIRE_FALSE (std::filesystem::equivalent (target, sibling));
    db = {};
    std::filesystem::remove_all (path);
}
