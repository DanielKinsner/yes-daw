// YES DAW - headless checks for ADR-0012 SQLite bundle schema/migrations/intent log.

#include "persistence/ProjectBundle.h"
#include "persistence/WaveformPeakCache.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <sqlite3.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::EntityId;
using yesdaw::engine::moveClip;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::SampleRate;
using yesdaw::engine::splitClip;
using yesdaw::engine::TimeBase;
using yesdaw::engine::trimClip;
using yesdaw::persistence::AssetImportRequest;
using yesdaw::persistence::BundleStatus;
using yesdaw::persistence::PendingFsOp;
using yesdaw::persistence::PendingFsOpKind;
using yesdaw::persistence::ProjectBundleDb;
using yesdaw::persistence::buildWaveformPeakCache;
using yesdaw::persistence::detail::SchemaMigration;
using yesdaw::persistence::kApplicationId;
using yesdaw::persistence::kBusyTimeoutMs;
using yesdaw::persistence::kCacheSizeKiB;
using yesdaw::persistence::kCodeSchemaVersion;
using yesdaw::persistence::kWalAutoCheckpointPages;
using yesdaw::persistence::detail::kSchemaV1Sql;
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
        writeProjectAssetFiles (path, project);
    }

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());

    Project readback;
    REQUIRE (reopened.readProjectSnapshot (readback).ok());
    requireSameProjectSurface (readback, project);
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
    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (project.clips.size() == 3u);
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
    REQUIRE (reopened.queryInt64 ("SELECT COUNT(*) FROM schema_migrations WHERE version = 1 AND app_build = 'interrupted';", value).ok());
    REQUIRE (value == 0);

    std::string integrity;
    REQUIRE (reopened.queryText ("PRAGMA integrity_check;", integrity).ok());
    REQUIRE (integrity == "ok");
    REQUIRE (reopened.validateStoredProjectSemantics().ok());
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
    REQUIRE (validation.status == BundleStatus::SemanticInvalid);
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
