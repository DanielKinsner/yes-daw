// YES DAW - SQLite Project bundle persistence slice (ADR-0012).
//
// This is a narrow, headless control-thread surface: bring up a `.yesdaw` bundle database with the
// v1 schema, run append-only migrations, enforce foreign keys, validate the existing Project value
// types against schema semantics, and carry the pending filesystem intent-log shape.

#pragma once

#include "engine/Automation.h"
#include "engine/Project.h"
#include "engine/Time.h"

#include <sqlite3.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace yesdaw::persistence {

inline constexpr std::int32_t kApplicationId = 0x59455331; // "YES1"
inline constexpr int          kCodeSchemaVersion = 1;
inline constexpr int          kBusyTimeoutMs = 5000;
inline constexpr int          kWalAutoCheckpointPages = 1000;
inline constexpr int          kCacheSizeKiB = -16384;

static_assert (static_cast<std::uint8_t> (engine::TimeBase::TempoLocked) == 0u);
static_assert (static_cast<std::uint8_t> (engine::TimeBase::SampleLocked) == 1u);
static_assert (static_cast<std::uint8_t> (engine::TempoCurve::Jump) == 0u);
static_assert (static_cast<std::uint8_t> (engine::TempoCurve::LinearRamp) == 1u);
static_assert (static_cast<std::uint8_t> (engine::AutomationCurveType::Linear) == 0u);
static_assert (static_cast<std::uint8_t> (engine::AutomationCurveType::Hold) == 1u);
static_assert (static_cast<std::uint8_t> (engine::AutomationCurveType::Bezier) == 2u);
static_assert (static_cast<std::uint8_t> (engine::AutomationCurveType::Log) == 3u);

enum class BundleStatus : std::uint8_t
{
    Ok = 0,
    FilesystemError,
    SqliteError,
    InvalidApplicationId,
    ForwardSchema,
    MigrationFailed,
    IntegrityFailed,
    SemanticInvalid,
    ConstraintFailed
};

struct BundleResult
{
    BundleStatus status = BundleStatus::Ok;
    int          sqliteCode = SQLITE_OK;
    int          userVersion = 0;
    std::string  message;

    [[nodiscard]] bool ok() const noexcept { return status == BundleStatus::Ok; }
};

enum class PendingFsOpKind : std::int32_t
{
    StageAsset = 0,
    TrashAsset = 1,
    MovePluginBlob = 2
};

struct PendingFsOp
{
    PendingFsOpKind           kind = PendingFsOpKind::StageAsset;
    std::string               tempRelativePath;
    std::string               finalRelativePath;
    engine::AssetContentHash  contentHash;
};

namespace detail {

inline BundleResult ok (int userVersion = kCodeSchemaVersion)
{
    return BundleResult { BundleStatus::Ok, SQLITE_OK, userVersion, {} };
}

inline BundleResult sqliteError (sqlite3* db, BundleStatus status = BundleStatus::SqliteError)
{
    const int code = db == nullptr ? SQLITE_ERROR : sqlite3_errcode (db);
    return BundleResult { status, code, 0, db == nullptr ? "sqlite handle is null" : sqlite3_errmsg (db) };
}

inline BundleResult sqliteMessage (sqlite3* db, BundleStatus status, std::string message)
{
    const int code = db == nullptr ? SQLITE_ERROR : sqlite3_errcode (db);
    return BundleResult { status, code, 0, std::move (message) };
}

inline std::string utf8Path (const std::filesystem::path& path)
{
    const auto utf8 = path.generic_u8string();
    return std::string (utf8.begin(), utf8.end());
}

inline bool fitsSqliteInteger (std::uint64_t value) noexcept
{
    return value <= static_cast<std::uint64_t> (std::numeric_limits<sqlite3_int64>::max());
}

inline bool isFinitePositive (double value) noexcept
{
    return value > 0.0 && value <= std::numeric_limits<double>::max();
}

inline bool isFiniteNonNegative (double value) noexcept
{
    return value >= 0.0 && value <= std::numeric_limits<double>::max();
}

inline std::string hexBytes (std::span<const std::uint8_t> bytes)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.resize (bytes.size() * 2u);

    for (std::size_t i = 0; i < bytes.size(); ++i)
    {
        out[i * 2u] = digits[(bytes[i] >> 4u) & 0x0Fu];
        out[i * 2u + 1u] = digits[bytes[i] & 0x0Fu];
    }

    return out;
}

inline BundleResult exec (sqlite3* db, std::string_view sql, BundleStatus status = BundleStatus::SqliteError)
{
    char* rawError = nullptr;
    const std::string sqlString (sql);
    const int rc = sqlite3_exec (db, sqlString.c_str(), nullptr, nullptr, &rawError);
    if (rc == SQLITE_OK)
        return ok();

    std::string message = rawError == nullptr ? sqlite3_errmsg (db) : rawError;
    sqlite3_free (rawError);
    return BundleResult { status, rc, 0, std::move (message) };
}

class Statement final
{
public:
    Statement() = default;
    Statement (sqlite3* db, std::string_view sql) { (void) prepare (db, sql); }
    Statement (const Statement&) = delete;
    Statement& operator= (const Statement&) = delete;

    Statement (Statement&& other) noexcept : stmt_ (std::exchange (other.stmt_, nullptr)) {}

    Statement& operator= (Statement&& other) noexcept
    {
        if (this != &other)
        {
            finalize();
            stmt_ = std::exchange (other.stmt_, nullptr);
        }

        return *this;
    }

    ~Statement() { finalize(); }

    [[nodiscard]] BundleResult prepare (sqlite3* db, std::string_view sql)
    {
        finalize();
        const std::string sqlString (sql);
        const int rc = sqlite3_prepare_v2 (db, sqlString.c_str(), -1, &stmt_, nullptr);
        if (rc != SQLITE_OK)
            return sqliteError (db);

        return ok();
    }

    [[nodiscard]] sqlite3_stmt* get() const noexcept { return stmt_; }

    [[nodiscard]] BundleResult bindBlob (int index, std::span<const std::uint8_t> bytes)
    {
        const int rc = sqlite3_bind_blob (stmt_,
                                          index,
                                          bytes.data(),
                                          static_cast<int> (bytes.size()),
                                          SQLITE_TRANSIENT);
        return rc == SQLITE_OK ? ok() : BundleResult { BundleStatus::SqliteError, rc, 0, sqlite3_errstr (rc) };
    }

    [[nodiscard]] BundleResult bindText (int index, std::string_view text)
    {
        const int rc = sqlite3_bind_text (stmt_,
                                          index,
                                          text.data(),
                                          static_cast<int> (text.size()),
                                          SQLITE_TRANSIENT);
        return rc == SQLITE_OK ? ok() : BundleResult { BundleStatus::SqliteError, rc, 0, sqlite3_errstr (rc) };
    }

    [[nodiscard]] BundleResult bindDouble (int index, double value)
    {
        const int rc = sqlite3_bind_double (stmt_, index, value);
        return rc == SQLITE_OK ? ok() : BundleResult { BundleStatus::SqliteError, rc, 0, sqlite3_errstr (rc) };
    }

    [[nodiscard]] BundleResult bindInt64 (int index, sqlite3_int64 value)
    {
        const int rc = sqlite3_bind_int64 (stmt_, index, value);
        return rc == SQLITE_OK ? ok() : BundleResult { BundleStatus::SqliteError, rc, 0, sqlite3_errstr (rc) };
    }

    [[nodiscard]] int step() noexcept { return sqlite3_step (stmt_); }
    void reset() noexcept
    {
        sqlite3_reset (stmt_);
        sqlite3_clear_bindings (stmt_);
    }

private:
    void finalize() noexcept
    {
        if (stmt_ != nullptr)
        {
            sqlite3_finalize (stmt_);
            stmt_ = nullptr;
        }
    }

    sqlite3_stmt* stmt_ = nullptr;
};

inline BundleResult queryInt64 (sqlite3* db, std::string_view sql, sqlite3_int64& out)
{
    Statement stmt;
    if (auto result = stmt.prepare (db, sql); ! result.ok())
        return result;

    const int step = stmt.step();
    if (step != SQLITE_ROW)
        return sqliteMessage (db, BundleStatus::SqliteError, "query returned no row");

    out = sqlite3_column_int64 (stmt.get(), 0);
    return ok();
}

inline BundleResult queryText (sqlite3* db, std::string_view sql, std::string& out)
{
    Statement stmt;
    if (auto result = stmt.prepare (db, sql); ! result.ok())
        return result;

    const int step = stmt.step();
    if (step != SQLITE_ROW)
        return sqliteMessage (db, BundleStatus::SqliteError, "query returned no row");

    const unsigned char* text = sqlite3_column_text (stmt.get(), 0);
    out = text == nullptr ? std::string {} : reinterpret_cast<const char*> (text);
    return ok();
}

inline BundleResult hasAnyRow (sqlite3* db, std::string_view sql, bool& out)
{
    Statement stmt;
    if (auto result = stmt.prepare (db, sql); ! result.ok())
        return result;

    const int step = stmt.step();
    if (step == SQLITE_ROW)
    {
        out = true;
        return ok();
    }

    if (step == SQLITE_DONE)
    {
        out = false;
        return ok();
    }

    out = false;
    return sqliteMessage (db, BundleStatus::SqliteError, sqlite3_errmsg (db));
}

inline BundleResult expectDone (sqlite3* db, Statement& stmt, BundleStatus status = BundleStatus::SqliteError)
{
    const int step = stmt.step();
    if (step == SQLITE_DONE)
        return ok();

    return sqliteMessage (db, status, sqlite3_errmsg (db));
}

inline bool projectFitsSchemaV1 (const engine::Project& project) noexcept
{
    if (! project.hasValidAssetClipIndirection() || ! isFinitePositive (project.sampleRate.hz))
        return false;

    for (const engine::Asset& asset : project.assets)
    {
        if (! fitsSqliteInteger (asset.frames) || ! isFinitePositive (asset.sampleRate.hz))
            return false;
    }

    for (const engine::Clip& clip : project.clips)
    {
        if (! fitsSqliteInteger (clip.srcOffset) || ! fitsSqliteInteger (clip.srcLen))
            return false;

        if (clip.timelineLength < 0 || clip.fadeIn < 0 || clip.fadeOut < 0)
            return false;

        if (! isFiniteNonNegative (static_cast<double> (clip.gain)))
            return false;

        if (clip.timeBase != engine::TimeBase::TempoLocked && clip.timeBase != engine::TimeBase::SampleLocked)
            return false;
    }

    return true;
}

inline constexpr std::string_view kSchemaV1Sql = R"SQL(
CREATE TABLE schema_migrations (
  version INTEGER PRIMARY KEY,
  applied_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  app_build TEXT NOT NULL
);

CREATE TABLE project (
  singleton_id INTEGER PRIMARY KEY CHECK (singleton_id = 1),
  id BLOB NOT NULL UNIQUE CHECK (length(id) = 16),
  sample_rate_hz REAL NOT NULL CHECK (sample_rate_hz > 0)
);

CREATE TABLE tempo_changes (
  tick INTEGER PRIMARY KEY,
  bpm REAL NOT NULL CHECK (bpm > 0),
  curve_to_next INTEGER NOT NULL CHECK (curve_to_next IN (0, 1))
);

CREATE TABLE meter_changes (
  tick INTEGER PRIMARY KEY,
  numerator INTEGER NOT NULL CHECK (numerator > 0),
  denominator INTEGER NOT NULL CHECK (denominator > 0)
);

CREATE TABLE markers (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  tick INTEGER NOT NULL,
  name TEXT NOT NULL
);

CREATE TABLE assets (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  content_hash BLOB NOT NULL CHECK (length(content_hash) = 32),
  frames INTEGER NOT NULL CHECK (frames > 0),
  sample_rate_hz REAL NOT NULL CHECK (sample_rate_hz > 0),
  channels INTEGER NOT NULL CHECK (channels > 0),
  relative_path TEXT NOT NULL
);

CREATE TABLE clips (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  asset_id BLOB NOT NULL,
  timeline_start INTEGER NOT NULL,
  timeline_length INTEGER NOT NULL CHECK (timeline_length >= 0),
  src_offset INTEGER NOT NULL CHECK (src_offset >= 0),
  src_len INTEGER NOT NULL CHECK (src_len >= 0),
  gain REAL NOT NULL CHECK (gain >= 0),
  fade_in INTEGER NOT NULL CHECK (fade_in >= 0),
  fade_out INTEGER NOT NULL CHECK (fade_out >= 0),
  time_base INTEGER NOT NULL CHECK (time_base IN (0, 1)),
  FOREIGN KEY (asset_id) REFERENCES assets(id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX clips_asset_id_idx ON clips(asset_id);

CREATE TABLE automation_points (
  target_node_id INTEGER NOT NULL,
  parameter_id INTEGER NOT NULL,
  tick INTEGER NOT NULL,
  value REAL NOT NULL CHECK (value >= 0 AND value <= 1),
  curve_type INTEGER NOT NULL CHECK (curve_type IN (0, 1, 2, 3)),
  PRIMARY KEY (target_node_id, parameter_id, tick)
);

CREATE TABLE pending_fs_ops (
  id INTEGER PRIMARY KEY,
  op_kind INTEGER NOT NULL CHECK (op_kind IN (0, 1, 2)),
  temp_relative_path TEXT NOT NULL,
  final_relative_path TEXT NOT NULL,
  content_hash BLOB NOT NULL CHECK (length(content_hash) = 32),
  committed INTEGER NOT NULL DEFAULT 0 CHECK (committed IN (0, 1)),
  created_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);
CREATE INDEX pending_fs_ops_committed_idx ON pending_fs_ops(committed, op_kind);

CREATE TABLE plugin_state_chunks (
  node_id BLOB NOT NULL CHECK (length(node_id) = 16),
  format TEXT NOT NULL,
  plugin_uid TEXT NOT NULL,
  plugin_version TEXT NOT NULL,
  chunk_kind INTEGER NOT NULL,
  chunk_len INTEGER NOT NULL CHECK (chunk_len >= 0),
  crc32 INTEGER NOT NULL,
  data BLOB NOT NULL CHECK (length(data) = chunk_len),
  PRIMARY KEY (node_id, chunk_kind)
);
)SQL";

struct SchemaMigration
{
    int              toVersion = 0;
    std::string_view sql;
};

inline constexpr std::array<SchemaMigration, 1> kMigrations {
    SchemaMigration { 1, kSchemaV1Sql },
};

inline BundleResult applyMigration (sqlite3* db, SchemaMigration migration, std::string_view appBuild)
{
    if (auto result = exec (db, "BEGIN IMMEDIATE;", BundleStatus::MigrationFailed); ! result.ok())
        return result;

    const auto rollback = [db] { (void) exec (db, "ROLLBACK;", BundleStatus::MigrationFailed); };

    if (auto result = exec (db, migration.sql, BundleStatus::MigrationFailed); ! result.ok())
    {
        rollback();
        return result;
    }

    {
        Statement stmt (db, "INSERT INTO schema_migrations(version, app_build) VALUES (?, ?);");
        if (auto result = stmt.bindInt64 (1, migration.toVersion); ! result.ok())
        {
            rollback();
            return result;
        }
        if (auto result = stmt.bindText (2, appBuild); ! result.ok())
        {
            rollback();
            return result;
        }
        if (auto result = expectDone (db, stmt, BundleStatus::MigrationFailed); ! result.ok())
        {
            rollback();
            return result;
        }
    }

    const std::string setApplicationId = "PRAGMA application_id = " + std::to_string (kApplicationId) + ";";
    if (auto result = exec (db, setApplicationId, BundleStatus::MigrationFailed); ! result.ok())
    {
        rollback();
        return result;
    }

    const std::string setVersion = "PRAGMA user_version = " + std::to_string (migration.toVersion) + ";";
    if (auto result = exec (db, setVersion, BundleStatus::MigrationFailed); ! result.ok())
    {
        rollback();
        return result;
    }

    if (auto result = exec (db, "COMMIT;", BundleStatus::MigrationFailed); ! result.ok())
    {
        rollback();
        return result;
    }

    return ok (migration.toVersion);
}

inline BundleResult runMigrations (sqlite3* db, int fromVersion, std::span<const SchemaMigration> migrations, std::string_view appBuild)
{
    int currentVersion = fromVersion;
    for (const SchemaMigration migration : migrations)
    {
        if (migration.toVersion <= currentVersion)
            continue;

        if (migration.toVersion != currentVersion + 1)
            return BundleResult { BundleStatus::MigrationFailed, SQLITE_ERROR, currentVersion, "schema migration versions must be contiguous" };

        if (auto result = applyMigration (db, migration, appBuild); ! result.ok())
            return result;

        currentVersion = migration.toVersion;
    }

    return ok (currentVersion);
}

inline BundleResult configureConnection (sqlite3* db)
{
    std::string journalMode;
    if (auto result = queryText (db, "PRAGMA journal_mode=WAL;", journalMode); ! result.ok())
        return result;

    if (journalMode != "wal")
        return BundleResult { BundleStatus::SqliteError, SQLITE_ERROR, 0, "SQLite did not enter WAL mode" };

    if (auto result = exec (db, "PRAGMA synchronous=NORMAL;"); ! result.ok())
        return result;

    if (auto result = exec (db, "PRAGMA foreign_keys=ON;"); ! result.ok())
        return result;

    if (sqlite3_busy_timeout (db, kBusyTimeoutMs) != SQLITE_OK)
        return sqliteError (db);

    if (auto result = exec (db, "PRAGMA wal_autocheckpoint=1000;"); ! result.ok())
        return result;

    if (auto result = exec (db, "PRAGMA cache_size=-16384;"); ! result.ok())
        return result;

    return exec (db, "PRAGMA temp_store=MEMORY;");
}

} // namespace detail

class ProjectBundleDb final
{
public:
    ProjectBundleDb() = default;
    ProjectBundleDb (const ProjectBundleDb&) = delete;
    ProjectBundleDb& operator= (const ProjectBundleDb&) = delete;

    ProjectBundleDb (ProjectBundleDb&& other) noexcept
        : db_ (std::exchange (other.db_, nullptr)), bundlePath_ (std::move (other.bundlePath_))
    {
    }

    ProjectBundleDb& operator= (ProjectBundleDb&& other) noexcept
    {
        if (this != &other)
        {
            close();
            db_ = std::exchange (other.db_, nullptr);
            bundlePath_ = std::move (other.bundlePath_);
        }

        return *this;
    }

    ~ProjectBundleDb() { close(); }

    [[nodiscard]] static BundleResult openOrCreateBundle (const std::filesystem::path& bundlePath, ProjectBundleDb& out)
    {
        std::error_code ec;
        std::filesystem::create_directories (bundlePath / "audio", ec);
        if (ec)
            return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

        for (const char* dir : { "peaks", "plugins", "autosave", ".trash" })
        {
            std::filesystem::create_directories (bundlePath / dir, ec);
            if (ec)
                return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };
        }

        return openProjectDb (bundlePath, true, out);
    }

    [[nodiscard]] static BundleResult openExistingBundle (const std::filesystem::path& bundlePath, ProjectBundleDb& out)
    {
        return openProjectDb (bundlePath, false, out);
    }

    [[nodiscard]] bool isOpen() const noexcept { return db_ != nullptr; }
    [[nodiscard]] const std::filesystem::path& bundlePath() const noexcept { return bundlePath_; }
    [[nodiscard]] std::filesystem::path databasePath() const { return bundlePath_ / "project.db"; }

    [[nodiscard]] BundleResult executeSql (std::string_view sql)
    {
        return detail::exec (db_, sql);
    }

    [[nodiscard]] BundleResult queryInt64 (std::string_view sql, sqlite3_int64& out) const
    {
        return detail::queryInt64 (db_, sql, out);
    }

    [[nodiscard]] BundleResult queryText (std::string_view sql, std::string& out) const
    {
        return detail::queryText (db_, sql, out);
    }

    [[nodiscard]] BundleResult writeProjectSnapshot (const engine::Project& project)
    {
        if (! detail::projectFitsSchemaV1 (project))
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "Project violates schema v1 semantics" };

        if (auto result = detail::exec (db_, "BEGIN IMMEDIATE;"); ! result.ok())
            return result;

        const auto rollback = [this] { (void) detail::exec (db_, "ROLLBACK;"); };

        if (auto result = detail::exec (db_, "DELETE FROM clips; DELETE FROM assets; DELETE FROM project;"); ! result.ok())
        {
            rollback();
            return result;
        }

        {
            detail::Statement stmt (db_, "INSERT INTO project(singleton_id, id, sample_rate_hz) VALUES (1, ?, ?);");
            if (auto result = stmt.bindBlob (1, project.id.bytes); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = stmt.bindDouble (2, project.sampleRate.hz); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = detail::expectDone (db_, stmt); ! result.ok())
            {
                rollback();
                return result;
            }
        }

        detail::Statement assetStmt (
            db_,
            "INSERT INTO assets(id, content_hash, frames, sample_rate_hz, channels, relative_path) VALUES (?, ?, ?, ?, ?, ?);");

        for (const engine::Asset& asset : project.assets)
        {
            assetStmt.reset();
            if (auto result = assetStmt.bindBlob (1, asset.id.bytes); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = assetStmt.bindBlob (2, asset.contentHash.bytes); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = assetStmt.bindInt64 (3, static_cast<sqlite3_int64> (asset.frames)); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = assetStmt.bindDouble (4, asset.sampleRate.hz); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = assetStmt.bindInt64 (5, asset.channels); ! result.ok())
            {
                rollback();
                return result;
            }

            const std::string relativePath = "audio/" + detail::hexBytes (asset.contentHash.bytes) + ".asset";
            if (auto result = assetStmt.bindText (6, relativePath); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = detail::expectDone (db_, assetStmt); ! result.ok())
            {
                rollback();
                return result;
            }
        }

        detail::Statement clipStmt (
            db_,
            "INSERT INTO clips(id, asset_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");

        for (const engine::Clip& clip : project.clips)
        {
            clipStmt.reset();
            if (auto result = clipStmt.bindBlob (1, clip.id.bytes); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindBlob (2, clip.assetId.bytes); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (3, clip.timelineStart); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (4, clip.timelineLength); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (5, static_cast<sqlite3_int64> (clip.srcOffset)); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (6, static_cast<sqlite3_int64> (clip.srcLen)); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindDouble (7, clip.gain); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (8, clip.fadeIn); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (9, clip.fadeOut); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (10, static_cast<sqlite3_int64> (clip.timeBase)); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = detail::expectDone (db_, clipStmt); ! result.ok())
            {
                rollback();
                return result;
            }
        }

        if (auto result = detail::exec (db_, "COMMIT;"); ! result.ok())
        {
            rollback();
            return result;
        }

        return detail::ok();
    }

    [[nodiscard]] BundleResult recordPendingFsOp (const PendingFsOp& op, sqlite3_int64& rowId)
    {
        detail::Statement stmt (
            db_,
            "INSERT INTO pending_fs_ops(op_kind, temp_relative_path, final_relative_path, content_hash, committed) "
            "VALUES (?, ?, ?, ?, 0);");

        if (auto result = stmt.bindInt64 (1, static_cast<sqlite3_int64> (op.kind)); ! result.ok())
            return result;
        if (auto result = stmt.bindText (2, op.tempRelativePath); ! result.ok())
            return result;
        if (auto result = stmt.bindText (3, op.finalRelativePath); ! result.ok())
            return result;
        if (auto result = stmt.bindBlob (4, op.contentHash.bytes); ! result.ok())
            return result;
        if (auto result = detail::expectDone (db_, stmt); ! result.ok())
            return result;

        rowId = sqlite3_last_insert_rowid (db_);
        return detail::ok();
    }

    [[nodiscard]] BundleResult markPendingFsOpCommitted (sqlite3_int64 rowId)
    {
        detail::Statement stmt (db_, "UPDATE pending_fs_ops SET committed = 1 WHERE id = ?;");
        if (auto result = stmt.bindInt64 (1, rowId); ! result.ok())
            return result;

        return detail::expectDone (db_, stmt);
    }

    [[nodiscard]] BundleResult pendingFsOpCount (bool committed, sqlite3_int64& out) const
    {
        detail::Statement stmt (db_, "SELECT COUNT(*) FROM pending_fs_ops WHERE committed = ?;");
        if (auto result = stmt.bindInt64 (1, committed ? 1 : 0); ! result.ok())
            return result;

        if (stmt.step() != SQLITE_ROW)
            return detail::sqliteMessage (db_, BundleStatus::SqliteError, "pending_fs_ops count returned no row");

        out = sqlite3_column_int64 (stmt.get(), 0);
        return detail::ok();
    }

    [[nodiscard]] BundleResult validateStoredProjectSemantics() const
    {
        std::string quickCheck;
        if (auto result = detail::queryText (db_, "PRAGMA quick_check;", quickCheck); ! result.ok())
            return result;
        if (quickCheck != "ok")
            return BundleResult { BundleStatus::IntegrityFailed, SQLITE_CORRUPT, kCodeSchemaVersion, quickCheck };

        bool foreignKeyProblem = false;
        if (auto result = detail::hasAnyRow (db_, "PRAGMA foreign_key_check;", foreignKeyProblem); ! result.ok())
            return result;
        if (foreignKeyProblem)
            return BundleResult { BundleStatus::IntegrityFailed, SQLITE_CONSTRAINT_FOREIGNKEY, kCodeSchemaVersion, "foreign_key_check returned rows" };

        bool sourceWindowProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM clips c JOIN assets a ON a.id = c.asset_id "
                "WHERE c.src_offset > a.frames OR c.src_len > a.frames - c.src_offset LIMIT 1;",
                sourceWindowProblem);
            ! result.ok())
            return result;
        if (sourceWindowProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "clip source window exceeds asset frames" };

        if (auto result = validateFiniteReals(); ! result.ok())
            return result;

        return validateStoredRanges();
    }

    [[nodiscard]] static BundleResult runMigrationsForTest (sqlite3* db, int fromVersion, std::span<const detail::SchemaMigration> migrations)
    {
        return detail::runMigrations (db, fromVersion, migrations, "test");
    }

private:
    [[nodiscard]] static BundleResult openProjectDb (const std::filesystem::path& bundlePath, bool create, ProjectBundleDb& out)
    {
        sqlite3* rawDb = nullptr;
        const std::filesystem::path dbPath = bundlePath / "project.db";
        const std::string path = detail::utf8Path (dbPath);
        const int flags = SQLITE_OPEN_READWRITE | (create ? SQLITE_OPEN_CREATE : 0);
        const int rc = sqlite3_open_v2 (path.c_str(), &rawDb, flags, nullptr);

        if (rc != SQLITE_OK)
        {
            BundleResult result = detail::sqliteError (rawDb);
            if (rawDb != nullptr)
                sqlite3_close (rawDb);
            return result;
        }

        ProjectBundleDb opened;
        opened.db_ = rawDb;
        opened.bundlePath_ = bundlePath;

        if (auto result = detail::configureConnection (opened.db_); ! result.ok())
            return result;

        sqlite3_int64 appId = 0;
        if (auto result = detail::queryInt64 (opened.db_, "PRAGMA application_id;", appId); ! result.ok())
            return result;

        sqlite3_int64 userVersion = 0;
        if (auto result = detail::queryInt64 (opened.db_, "PRAGMA user_version;", userVersion); ! result.ok())
            return result;

        if (appId != 0 && appId != kApplicationId)
            return BundleResult { BundleStatus::InvalidApplicationId, SQLITE_NOTADB, static_cast<int> (userVersion), "project.db application_id is not YES1" };

        if (userVersion > kCodeSchemaVersion)
            return BundleResult { BundleStatus::ForwardSchema, SQLITE_OK, static_cast<int> (userVersion), "project.db was created by a newer YES DAW schema" };

        if (userVersion < kCodeSchemaVersion)
        {
            if (auto result = detail::runMigrations (opened.db_, static_cast<int> (userVersion), detail::kMigrations, "dev"); ! result.ok())
                return result;
        }

        if (auto result = detail::queryInt64 (opened.db_, "PRAGMA application_id;", appId); ! result.ok())
            return result;
        if (auto result = detail::queryInt64 (opened.db_, "PRAGMA user_version;", userVersion); ! result.ok())
            return result;

        if (appId != kApplicationId || userVersion != kCodeSchemaVersion)
            return BundleResult { BundleStatus::MigrationFailed, SQLITE_ERROR, static_cast<int> (userVersion), "schema migration did not publish v1 identity" };

        if (auto result = opened.validateStoredProjectSemantics(); ! result.ok())
            return result;

        out = std::move (opened);
        return detail::ok (static_cast<int> (userVersion));
    }

    [[nodiscard]] BundleResult validateFiniteReals() const
    {
        detail::Statement stmt (
            db_,
            "SELECT sample_rate_hz FROM project "
            "UNION ALL SELECT sample_rate_hz FROM assets "
            "UNION ALL SELECT bpm FROM tempo_changes "
            "UNION ALL SELECT value FROM automation_points "
            "UNION ALL SELECT gain FROM clips;");
        while (true)
        {
            const int step = stmt.step();
            if (step == SQLITE_DONE)
                return detail::ok();
            if (step != SQLITE_ROW)
                return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

            const double value = sqlite3_column_double (stmt.get(), 0);
            if (! (value <= std::numeric_limits<double>::max() && value >= -std::numeric_limits<double>::max()))
                return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "non-finite persisted real value" };
        }
    }

    [[nodiscard]] BundleResult validateStoredRanges() const
    {
        bool rangeProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM project WHERE sample_rate_hz <= 0 "
                "UNION ALL SELECT 1 FROM assets WHERE frames <= 0 OR sample_rate_hz <= 0 OR channels <= 0 "
                "UNION ALL SELECT 1 FROM clips WHERE timeline_length < 0 OR src_offset < 0 OR src_len < 0 OR gain < 0 OR fade_in < 0 OR fade_out < 0 OR time_base NOT IN (0, 1) "
                "UNION ALL SELECT 1 FROM tempo_changes WHERE bpm <= 0 OR curve_to_next NOT IN (0, 1) "
                "UNION ALL SELECT 1 FROM meter_changes WHERE numerator <= 0 OR denominator <= 0 "
                "UNION ALL SELECT 1 FROM automation_points WHERE target_node_id < 0 OR parameter_id < 0 OR value < 0 OR value > 1 OR curve_type NOT IN (0, 1, 2, 3) "
                "LIMIT 1;",
                rangeProblem);
            ! result.ok())
            return result;

        if (rangeProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "persisted row violates schema v1 semantic ranges" };

        return detail::ok();
    }

    void close() noexcept
    {
        if (db_ != nullptr)
        {
            sqlite3_close (db_);
            db_ = nullptr;
        }
    }

    sqlite3*              db_ = nullptr;
    std::filesystem::path bundlePath_;
};

} // namespace yesdaw::persistence
