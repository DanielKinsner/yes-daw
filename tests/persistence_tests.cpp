// YES DAW - headless checks for ADR-0012 SQLite bundle schema/migrations/intent log.

#include "persistence/ProjectBundle.h"

#include <catch2/catch_test_macros.hpp>

#include <sqlite3.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::EntityId;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::persistence::BundleStatus;
using yesdaw::persistence::PendingFsOp;
using yesdaw::persistence::PendingFsOpKind;
using yesdaw::persistence::ProjectBundleDb;
using yesdaw::persistence::detail::SchemaMigration;
using yesdaw::persistence::kApplicationId;
using yesdaw::persistence::kBusyTimeoutMs;
using yesdaw::persistence::kCacheSizeKiB;
using yesdaw::persistence::kCodeSchemaVersion;
using yesdaw::persistence::kWalAutoCheckpointPages;

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

Asset makeAsset (EntityId id, std::uint64_t frames = 48000)
{
    Asset asset;
    asset.id = id;
    asset.contentHash = hashFromLowByte (static_cast<std::uint8_t> (id.bytes.back() + 10u));
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

ProjectBundleDb openFreshBundle (const std::filesystem::path& path)
{
    ProjectBundleDb db;
    const auto result = ProjectBundleDb::openOrCreateBundle (path, db);
    REQUIRE (result.ok());
    return db;
}

void requireSameProjectSurface (const Project& actual, const Project& expected)
{
    REQUIRE (actual.id == expected.id);
    REQUIRE (actual.sampleRate == expected.sampleRate);
    REQUIRE (actual.assets == expected.assets);
    REQUIRE (actual.clips == expected.clips);
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
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'pending_fs_ops';", value).ok());
    REQUIRE (value == 1);
    REQUIRE (db.queryInt64 ("SELECT COUNT(*) FROM sqlite_schema WHERE type = 'table' AND name = 'plugin_state_chunks';", value).ok());
    REQUIRE (value == 1);
}

TEST_CASE ("Migration harness refuses forward schema and rolls back failed migrations", "[persistence][migration]")
{
    const auto path = makeTempBundlePath ("forward");
    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.executeSql ("PRAGMA user_version = 2;").ok());
    }

    ProjectBundleDb reopened;
    const auto forward = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (forward.status == BundleStatus::ForwardSchema);
    REQUIRE (forward.userVersion == 2);

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
        "INSERT INTO clips(id, asset_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base) "
        "VALUES (X'000000000000000000000000000000EE', X'000000000000000000000000000000EF', 0, 1, 0, 1, 1.0, 0, 0, 1);");
    REQUIRE ((insertOrphan.sqliteCode == SQLITE_CONSTRAINT || insertOrphan.sqliteCode == SQLITE_CONSTRAINT_FOREIGNKEY));
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
    REQUIRE (db.validateStoredProjectSemantics().ok());

    Project orphan = project;
    orphan.clips[0].assetId = idFromLowByte (99);
    REQUIRE (db.writeProjectSnapshot (orphan).status == BundleStatus::SemanticInvalid);

    Project invalidGain = project;
    invalidGain.clips[0].gain = -0.25f;
    REQUIRE (db.writeProjectSnapshot (invalidGain).status == BundleStatus::SemanticInvalid);
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

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (project).ok());
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
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
        REQUIRE (db.executeSql ("UPDATE clips SET src_len = 901 WHERE id = X'00000000000000000000000000000004';").ok());
    }

    ProjectBundleDb reopened;
    const auto validation = ProjectBundleDb::openExistingBundle (path, reopened);
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
}

TEST_CASE ("Opening an existing bundle rejects non-canonical Project value storage types", "[persistence][semantic][open]")
{
    const auto path = makeTempBundlePath ("semantic-type-open");

    {
        ProjectBundleDb db = openFreshBundle (path);
        REQUIRE (db.writeProjectSnapshot (makeProject()).ok());
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
