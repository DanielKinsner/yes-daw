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

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <share.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

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

struct AssetImportRequest
{
    std::filesystem::path sourcePath;
    engine::EntityId      assetId;
    std::uint64_t         frames = 0;
    engine::SampleRate    sampleRate;
    std::uint16_t         channels = 0;
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

inline BundleResult semanticInvalid (std::string message)
{
    return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, std::move (message) };
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

inline std::string assetRelativePathForHash (const engine::AssetContentHash& hash)
{
    return "audio/" + hexBytes (hash.bytes) + ".asset";
}

inline std::string assetTempRelativePathForHash (const engine::AssetContentHash& hash)
{
    return "audio/." + hexBytes (hash.bytes) + ".tmp";
}

inline BundleResult filesystemMessage (std::string_view action, const std::filesystem::path& path, int error)
{
    return BundleResult {
        BundleStatus::FilesystemError,
        SQLITE_OK,
        0,
        std::string (action) + ": " + utf8Path (path) + ": " + std::generic_category().message (error),
    };
}

constexpr std::uint32_t rotr32 (std::uint32_t value, std::uint32_t bits) noexcept
{
    return (value >> bits) | (value << (32u - bits));
}

class Sha256 final
{
public:
    void update (const std::uint8_t* data, std::size_t size) noexcept
    {
        if (size == 0)
            return;

        totalBytes_ += size;

        std::size_t offset = 0;
        if (bufferSize_ > 0)
        {
            const std::size_t toCopy = std::min (size, kBlockBytes - bufferSize_);
            for (std::size_t i = 0; i < toCopy; ++i)
                buffer_[bufferSize_ + i] = data[i];

            bufferSize_ += toCopy;
            offset += toCopy;

            if (bufferSize_ == kBlockBytes)
            {
                transform (buffer_.data());
                bufferSize_ = 0;
            }
        }

        while (offset + kBlockBytes <= size)
        {
            transform (data + offset);
            offset += kBlockBytes;
        }

        const std::size_t remaining = size - offset;
        for (std::size_t i = 0; i < remaining; ++i)
            buffer_[i] = data[offset + i];
        bufferSize_ = remaining;
    }

    [[nodiscard]] engine::AssetContentHash finalize() noexcept
    {
        const std::uint64_t bitLength = static_cast<std::uint64_t> (totalBytes_) * 8ull;

        buffer_[bufferSize_++] = 0x80u;
        if (bufferSize_ > 56u)
        {
            while (bufferSize_ < kBlockBytes)
                buffer_[bufferSize_++] = 0u;
            transform (buffer_.data());
            bufferSize_ = 0;
        }

        while (bufferSize_ < 56u)
            buffer_[bufferSize_++] = 0u;

        for (std::size_t i = 0; i < 8u; ++i)
            buffer_[56u + i] = static_cast<std::uint8_t> ((bitLength >> ((7u - i) * 8u)) & 0xFFu);
        transform (buffer_.data());

        engine::AssetContentHash out;
        for (std::size_t i = 0; i < state_.size(); ++i)
        {
            out.bytes[i * 4u] = static_cast<std::uint8_t> ((state_[i] >> 24u) & 0xFFu);
            out.bytes[i * 4u + 1u] = static_cast<std::uint8_t> ((state_[i] >> 16u) & 0xFFu);
            out.bytes[i * 4u + 2u] = static_cast<std::uint8_t> ((state_[i] >> 8u) & 0xFFu);
            out.bytes[i * 4u + 3u] = static_cast<std::uint8_t> (state_[i] & 0xFFu);
        }

        return out;
    }

private:
    static constexpr std::size_t kBlockBytes = 64;
    static constexpr std::array<std::uint32_t, 64> kRoundConstants {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };

    static constexpr std::uint32_t choose (std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
    {
        return (x & y) ^ (~x & z);
    }

    static constexpr std::uint32_t majority (std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
    {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    static constexpr std::uint32_t bigSigma0 (std::uint32_t x) noexcept
    {
        return rotr32 (x, 2u) ^ rotr32 (x, 13u) ^ rotr32 (x, 22u);
    }

    static constexpr std::uint32_t bigSigma1 (std::uint32_t x) noexcept
    {
        return rotr32 (x, 6u) ^ rotr32 (x, 11u) ^ rotr32 (x, 25u);
    }

    static constexpr std::uint32_t smallSigma0 (std::uint32_t x) noexcept
    {
        return rotr32 (x, 7u) ^ rotr32 (x, 18u) ^ (x >> 3u);
    }

    static constexpr std::uint32_t smallSigma1 (std::uint32_t x) noexcept
    {
        return rotr32 (x, 17u) ^ rotr32 (x, 19u) ^ (x >> 10u);
    }

    void transform (const std::uint8_t* block) noexcept
    {
        std::array<std::uint32_t, 64> words {};
        for (std::size_t i = 0; i < 16u; ++i)
        {
            words[i] = (static_cast<std::uint32_t> (block[i * 4u]) << 24u)
                       | (static_cast<std::uint32_t> (block[i * 4u + 1u]) << 16u)
                       | (static_cast<std::uint32_t> (block[i * 4u + 2u]) << 8u)
                       | static_cast<std::uint32_t> (block[i * 4u + 3u]);
        }

        for (std::size_t i = 16u; i < words.size(); ++i)
            words[i] = words[i - 16u] + smallSigma0 (words[i - 15u]) + words[i - 7u] + smallSigma1 (words[i - 2u]);

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];

        for (std::size_t i = 0; i < words.size(); ++i)
        {
            const std::uint32_t t1 = h + bigSigma1 (e) + choose (e, f, g) + kRoundConstants[i] + words[i];
            const std::uint32_t t2 = bigSigma0 (a) + majority (a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_ {
        0x6a09e667u,
        0xbb67ae85u,
        0x3c6ef372u,
        0xa54ff53au,
        0x510e527fu,
        0x9b05688cu,
        0x1f83d9abu,
        0x5be0cd19u,
    };
    std::array<std::uint8_t, kBlockBytes> buffer_ {};
    std::size_t bufferSize_ = 0;
    std::size_t totalBytes_ = 0;
};

inline engine::AssetContentHash sha256Bytes (std::span<const std::uint8_t> bytes) noexcept
{
    Sha256 sha;
    sha.update (bytes.data(), bytes.size());
    return sha.finalize();
}

inline BundleResult hashFile (const std::filesystem::path& path, engine::AssetContentHash& out)
{
    std::error_code ec;
    if (! std::filesystem::is_regular_file (path, ec))
        return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "file is not readable: " + utf8Path (path) };
    if (ec)
        return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

    std::ifstream input (path, std::ios::binary);
    if (! input)
        return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "failed to open file: " + utf8Path (path) };

    Sha256 sha;
    std::array<char, 65536> buffer {};
    while (input)
    {
        input.read (buffer.data(), static_cast<std::streamsize> (buffer.size()));
        const std::streamsize read = input.gcount();
        if (read > 0)
        {
            sha.update (reinterpret_cast<const std::uint8_t*> (buffer.data()), static_cast<std::size_t> (read));
        }
    }

    if (! input.eof())
        return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "failed while reading file: " + utf8Path (path) };

    out = sha.finalize();
    return ok();
}

inline BundleResult flushFileToDisk (const std::filesystem::path& path)
{
#if defined(_WIN32)
    int fd = -1;
    const errno_t openError = _wsopen_s (&fd, path.c_str(), _O_RDWR | _O_BINARY, _SH_DENYNO, 0);
    if (openError != 0)
        return filesystemMessage ("open for flush failed", path, static_cast<int> (openError));

    const int rc = _commit (fd);
    const int savedErrno = errno;
    _close (fd);
    if (rc != 0)
        return filesystemMessage ("file flush failed", path, savedErrno);
#else
    const int fd = ::open (path.c_str(), O_RDONLY);
    if (fd == -1)
        return filesystemMessage ("open for flush failed", path, errno);

    const int rc = ::fsync (fd);
    const int savedErrno = errno;
    ::close (fd);
    if (rc != 0)
        return filesystemMessage ("file flush failed", path, savedErrno);
#endif

    return ok();
}

inline BundleResult flushDirectoryToDisk (const std::filesystem::path& path)
{
#if defined(_WIN32)
    (void) path;
    return ok();
#else
    int flags = O_RDONLY;
#if defined(O_DIRECTORY)
    flags |= O_DIRECTORY;
#endif
    const int fd = ::open (path.c_str(), flags);
    if (fd == -1)
        return filesystemMessage ("open directory for flush failed", path, errno);

    const int rc = ::fsync (fd);
    const int savedErrno = errno;
    ::close (fd);
    if (rc != 0)
        return filesystemMessage ("directory flush failed", path, savedErrno);

    return ok();
#endif
}

inline BundleResult copyFileBytes (const std::filesystem::path& source, const std::filesystem::path& destination)
{
    std::ifstream input (source, std::ios::binary);
    if (! input)
        return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "failed to open source file: " + utf8Path (source) };

    std::ofstream output (destination, std::ios::binary | std::ios::trunc);
    if (! output)
        return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "failed to open temp asset file: " + utf8Path (destination) };

    std::array<char, 65536> buffer {};
    while (input)
    {
        input.read (buffer.data(), static_cast<std::streamsize> (buffer.size()));
        const std::streamsize read = input.gcount();
        if (read > 0)
        {
            output.write (buffer.data(), read);
            if (! output)
                return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "failed while writing temp asset file: " + utf8Path (destination) };
        }
    }

    if (! input.eof())
        return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "failed while reading source file: " + utf8Path (source) };

    output.flush();
    if (! output)
        return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "failed to flush temp asset stream: " + utf8Path (destination) };
    output.close();
    if (! output)
        return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "failed to close temp asset stream: " + utf8Path (destination) };

    return flushFileToDisk (destination);
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

template <std::size_t N>
inline BundleResult columnBlob (sqlite3_stmt* stmt, int column, std::array<std::uint8_t, N>& out, std::string_view label)
{
    const int   bytes = sqlite3_column_bytes (stmt, column);
    const void* raw = sqlite3_column_blob (stmt, column);

    if (bytes != static_cast<int> (N) || raw == nullptr)
        return semanticInvalid (std::string (label) + " has invalid blob length");

    const auto* data = static_cast<const std::uint8_t*> (raw);
    for (std::size_t i = 0; i < N; ++i)
        out[i] = data[i];

    return ok();
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

    [[nodiscard]] BundleResult importAssetBytes (const AssetImportRequest& request, engine::Asset& out)
    {
        if (! request.assetId.isValid() || request.frames == 0 || ! request.sampleRate.isValid() || request.channels == 0)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "Asset import metadata violates ADR-0011" };

        std::error_code ec;
        if (! std::filesystem::is_regular_file (request.sourcePath, ec))
            return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, "source asset file is not readable: " + detail::utf8Path (request.sourcePath) };
        if (ec)
            return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

        engine::AssetContentHash hash;
        if (auto result = detail::hashFile (request.sourcePath, hash); ! result.ok())
            return result;

        engine::Asset existing;
        std::string existingRelativePath;
        bool foundExisting = false;
        if (auto result = findAssetByContentHash (hash, existing, existingRelativePath, foundExisting); ! result.ok())
            return result;

        if (foundExisting)
        {
            if (existing.frames != request.frames || existing.sampleRate != request.sampleRate || existing.channels != request.channels)
                return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "duplicate content hash has conflicting Asset metadata" };
            if (existingRelativePath != detail::assetRelativePathForHash (existing.contentHash))
                return detail::semanticInvalid ("duplicate content hash points at a non-canonical Asset path");

            std::filesystem::path existingPath;
            if (auto result = bundlePathForRelativePath (existingRelativePath, existingPath); ! result.ok())
                return result;
            if (auto result = verifyAssetFile (existing.contentHash, existingPath); ! result.ok())
                return result;

            out = existing;
            return detail::ok();
        }

        const std::string finalRelativePath = detail::assetRelativePathForHash (hash);
        const std::string tempRelativePath = detail::assetTempRelativePathForHash (hash);

        std::filesystem::path finalPath;
        if (auto result = bundlePathForRelativePath (finalRelativePath, finalPath); ! result.ok())
            return result;
        std::filesystem::path tempPath;
        if (auto result = bundlePathForRelativePath (tempRelativePath, tempPath); ! result.ok())
            return result;

        std::filesystem::create_directories (finalPath.parent_path(), ec);
        if (ec)
            return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

        PendingFsOp op {
            PendingFsOpKind::StageAsset,
            tempRelativePath,
            finalRelativePath,
            hash,
        };

        sqlite3_int64 pendingRowId = 0;
        if (auto result = detail::exec (db_, "BEGIN IMMEDIATE;"); ! result.ok())
            return result;
        if (auto result = recordPendingFsOp (op, pendingRowId); ! result.ok())
        {
            (void) detail::exec (db_, "ROLLBACK;");
            return result;
        }
        if (auto result = detail::exec (db_, "COMMIT;"); ! result.ok())
        {
            (void) detail::exec (db_, "ROLLBACK;");
            return result;
        }

        if (auto result = removeFileIfExists (tempPath); ! result.ok())
            return result;
        if (auto result = detail::copyFileBytes (request.sourcePath, tempPath); ! result.ok())
            return result;

        engine::AssetContentHash copiedHash;
        if (auto result = detail::hashFile (tempPath, copiedHash); ! result.ok())
            return result;
        if (! (copiedHash == hash))
            return BundleResult { BundleStatus::IntegrityFailed, SQLITE_CORRUPT, kCodeSchemaVersion, "copied asset bytes do not match source hash" };

        if (std::filesystem::exists (finalPath, ec))
        {
            if (ec)
                return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

            if (auto result = verifyAssetFile (hash, finalPath); ! result.ok())
                return result;
            if (auto result = removeFileIfExists (tempPath); ! result.ok())
                return result;
        }
        else
        {
            std::filesystem::rename (tempPath, finalPath, ec);
            if (ec)
                return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };
            if (auto result = detail::flushDirectoryToDisk (finalPath.parent_path()); ! result.ok())
                return result;
        }

        engine::Asset imported;
        imported.id = request.assetId;
        imported.contentHash = hash;
        imported.frames = request.frames;
        imported.sampleRate = request.sampleRate;
        imported.channels = request.channels;

        if (auto result = detail::exec (db_, "BEGIN IMMEDIATE;"); ! result.ok())
            return result;
        const auto rollback = [this] { (void) detail::exec (db_, "ROLLBACK;"); };

        if (auto result = insertAssetRow (imported, finalRelativePath); ! result.ok())
        {
            rollback();
            return result;
        }
        if (auto result = markPendingFsOpCommitted (pendingRowId); ! result.ok())
        {
            rollback();
            return result;
        }
        if (auto result = detail::exec (db_, "COMMIT;"); ! result.ok())
        {
            rollback();
            return result;
        }

        out = imported;
        return detail::ok();
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

    [[nodiscard]] BundleResult readProjectSnapshot (engine::Project& out) const
    {
        if (auto result = validateStoredProjectSemantics(); ! result.ok())
            return result;

        engine::Project project;

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (db_, "SELECT id, sample_rate_hz FROM project WHERE singleton_id = 1;"); ! result.ok())
                return result;

            const int first = stmt.step();
            if (first == SQLITE_DONE)
                return detail::semanticInvalid ("project snapshot is missing the project row");
            if (first != SQLITE_ROW)
                return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

            if (auto result = detail::columnBlob (stmt.get(), 0, project.id.bytes, "project.id"); ! result.ok())
                return result;
            project.sampleRate = engine::SampleRate { sqlite3_column_double (stmt.get(), 1) };

            const int second = stmt.step();
            if (second != SQLITE_DONE)
                return second == SQLITE_ROW ? detail::semanticInvalid ("project snapshot has multiple project rows")
                                            : detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));
        }

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (
                    db_,
                    "SELECT id, content_hash, frames, sample_rate_hz, channels FROM assets ORDER BY rowid;");
                ! result.ok())
                return result;

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::Asset asset;
                if (auto result = detail::columnBlob (stmt.get(), 0, asset.id.bytes, "assets.id"); ! result.ok())
                    return result;
                if (auto result = detail::columnBlob (stmt.get(), 1, asset.contentHash.bytes, "assets.content_hash"); ! result.ok())
                    return result;

                const sqlite3_int64 frames = sqlite3_column_int64 (stmt.get(), 2);
                if (frames <= 0)
                    return detail::semanticInvalid ("assets.frames is outside the Project value range");
                asset.frames = static_cast<std::uint64_t> (frames);
                asset.sampleRate = engine::SampleRate { sqlite3_column_double (stmt.get(), 3) };

                const sqlite3_int64 channels = sqlite3_column_int64 (stmt.get(), 4);
                if (channels <= 0 || channels > std::numeric_limits<std::uint16_t>::max())
                    return detail::semanticInvalid ("assets.channels is outside the Project value range");
                asset.channels = static_cast<std::uint16_t> (channels);

                project.assets.push_back (asset);
            }
        }

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (
                    db_,
                    "SELECT id, asset_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base "
                    "FROM clips ORDER BY rowid;");
                ! result.ok())
                return result;

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::Clip clip;
                if (auto result = detail::columnBlob (stmt.get(), 0, clip.id.bytes, "clips.id"); ! result.ok())
                    return result;
                if (auto result = detail::columnBlob (stmt.get(), 1, clip.assetId.bytes, "clips.asset_id"); ! result.ok())
                    return result;

                clip.timelineStart = sqlite3_column_int64 (stmt.get(), 2);
                clip.timelineLength = sqlite3_column_int64 (stmt.get(), 3);

                const sqlite3_int64 srcOffset = sqlite3_column_int64 (stmt.get(), 4);
                const sqlite3_int64 srcLen = sqlite3_column_int64 (stmt.get(), 5);
                if (srcOffset < 0 || srcLen < 0)
                    return detail::semanticInvalid ("clips source window is outside the Project value range");
                clip.srcOffset = static_cast<std::uint64_t> (srcOffset);
                clip.srcLen = static_cast<std::uint64_t> (srcLen);

                clip.gain = static_cast<float> (sqlite3_column_double (stmt.get(), 6));
                clip.fadeIn = sqlite3_column_int64 (stmt.get(), 7);
                clip.fadeOut = sqlite3_column_int64 (stmt.get(), 8);

                const sqlite3_int64 timeBase = sqlite3_column_int64 (stmt.get(), 9);
                if (timeBase != static_cast<sqlite3_int64> (engine::TimeBase::TempoLocked)
                    && timeBase != static_cast<sqlite3_int64> (engine::TimeBase::SampleLocked))
                    return detail::semanticInvalid ("clips.time_base is outside the Project value range");
                clip.timeBase = static_cast<engine::TimeBase> (timeBase);

                project.clips.push_back (clip);
            }
        }

        if (! detail::projectFitsSchemaV1 (project))
            return detail::semanticInvalid ("read Project violates schema v1 semantics");

        out = std::move (project);
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

        if (auto result = validateStoredRanges(); ! result.ok())
            return result;

        return validateStoredTypes();
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

        if (auto result = opened.reconcileBundleFilesystem(); ! result.ok())
            return result;

        out = std::move (opened);
        return detail::ok (static_cast<int> (userVersion));
    }

    struct StoredAssetFile
    {
        engine::AssetContentHash hash;
        std::string              relativePath;
    };

    [[nodiscard]] BundleResult findAssetByContentHash (const engine::AssetContentHash& hash,
                                                       engine::Asset& out,
                                                       std::string& relativePath,
                                                       bool& found) const
    {
        detail::Statement stmt (
            db_,
            "SELECT id, content_hash, frames, sample_rate_hz, channels, relative_path "
            "FROM assets WHERE content_hash = ? ORDER BY rowid LIMIT 1;");
        if (auto result = stmt.bindBlob (1, hash.bytes); ! result.ok())
            return result;

        const int step = stmt.step();
        if (step == SQLITE_DONE)
        {
            found = false;
            return detail::ok();
        }
        if (step != SQLITE_ROW)
            return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

        if (auto result = detail::columnBlob (stmt.get(), 0, out.id.bytes, "assets.id"); ! result.ok())
            return result;
        if (auto result = detail::columnBlob (stmt.get(), 1, out.contentHash.bytes, "assets.content_hash"); ! result.ok())
            return result;

        const sqlite3_int64 frames = sqlite3_column_int64 (stmt.get(), 2);
        const sqlite3_int64 channels = sqlite3_column_int64 (stmt.get(), 4);
        if (frames <= 0 || channels <= 0 || channels > std::numeric_limits<std::uint16_t>::max())
            return detail::semanticInvalid ("stored Asset metadata is outside the Project value range");

        out.frames = static_cast<std::uint64_t> (frames);
        out.sampleRate = engine::SampleRate { sqlite3_column_double (stmt.get(), 3) };
        out.channels = static_cast<std::uint16_t> (channels);

        const unsigned char* text = sqlite3_column_text (stmt.get(), 5);
        relativePath = text == nullptr ? std::string {} : reinterpret_cast<const char*> (text);
        found = true;
        return detail::ok();
    }

    [[nodiscard]] BundleResult insertAssetRow (const engine::Asset& asset, std::string_view relativePath)
    {
        detail::Statement stmt (
            db_,
            "INSERT INTO assets(id, content_hash, frames, sample_rate_hz, channels, relative_path) VALUES (?, ?, ?, ?, ?, ?);");
        if (auto result = stmt.bindBlob (1, asset.id.bytes); ! result.ok())
            return result;
        if (auto result = stmt.bindBlob (2, asset.contentHash.bytes); ! result.ok())
            return result;
        if (auto result = stmt.bindInt64 (3, static_cast<sqlite3_int64> (asset.frames)); ! result.ok())
            return result;
        if (auto result = stmt.bindDouble (4, asset.sampleRate.hz); ! result.ok())
            return result;
        if (auto result = stmt.bindInt64 (5, asset.channels); ! result.ok())
            return result;
        if (auto result = stmt.bindText (6, relativePath); ! result.ok())
            return result;

        return detail::expectDone (db_, stmt);
    }

    [[nodiscard]] BundleResult bundlePathForRelativePath (std::string_view relative, std::filesystem::path& out) const
    {
        if (relative.empty())
            return detail::semanticInvalid ("bundle relative path is empty");

        const std::filesystem::path relativePath { std::string (relative) };
        if (relativePath.is_absolute() || relativePath.has_root_name())
            return detail::semanticInvalid ("bundle relative path must stay inside the bundle");

        for (const auto& part : relativePath)
        {
            if (part == "..")
                return detail::semanticInvalid ("bundle relative path must not contain parent traversal");
        }

        out = bundlePath_ / relativePath;
        return detail::ok();
    }

    [[nodiscard]] BundleResult verifyAssetFile (const engine::AssetContentHash& expectedHash, const std::filesystem::path& path) const
    {
        std::error_code ec;
        if (! std::filesystem::exists (path, ec) || ec)
            return BundleResult { BundleStatus::IntegrityFailed, SQLITE_CORRUPT, kCodeSchemaVersion, "committed asset bytes are missing: " + detail::utf8Path (path) };

        engine::AssetContentHash actualHash;
        if (auto result = detail::hashFile (path, actualHash); ! result.ok())
            return result;
        if (! (actualHash == expectedHash))
            return BundleResult { BundleStatus::IntegrityFailed, SQLITE_CORRUPT, kCodeSchemaVersion, "committed asset bytes do not match content hash: " + detail::utf8Path (path) };

        return detail::ok();
    }

    [[nodiscard]] BundleResult removeFileIfExists (const std::filesystem::path& path) const
    {
        std::error_code ec;
        if (! std::filesystem::exists (path, ec))
            return ec ? BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() } : detail::ok();

        std::filesystem::remove (path, ec);
        if (ec)
            return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

        return detail::ok();
    }

    [[nodiscard]] BundleResult moveFileToTrash (const std::filesystem::path& path) const
    {
        std::error_code ec;
        if (! std::filesystem::exists (path, ec))
            return ec ? BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() } : detail::ok();

        const std::filesystem::path trashDir = bundlePath_ / ".trash";
        std::filesystem::create_directories (trashDir, ec);
        if (ec)
            return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

        std::filesystem::path target = trashDir / path.filename();
        if (std::filesystem::exists (target, ec))
        {
            std::filesystem::remove (target, ec);
            if (ec)
                return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };
        }

        std::filesystem::rename (path, target, ec);
        if (ec)
            return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

        return detail::flushDirectoryToDisk (trashDir);
    }

    [[nodiscard]] BundleResult loadStoredAssetFiles (std::vector<StoredAssetFile>& out) const
    {
        detail::Statement stmt (db_, "SELECT content_hash, relative_path FROM assets ORDER BY rowid;");

        while (true)
        {
            const int step = stmt.step();
            if (step == SQLITE_DONE)
                return detail::ok();
            if (step != SQLITE_ROW)
                return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

            StoredAssetFile asset;
            if (auto result = detail::columnBlob (stmt.get(), 0, asset.hash.bytes, "assets.content_hash"); ! result.ok())
                return result;

            const unsigned char* text = sqlite3_column_text (stmt.get(), 1);
            asset.relativePath = text == nullptr ? std::string {} : reinterpret_cast<const char*> (text);

            const std::string expectedRelativePath = detail::assetRelativePathForHash (asset.hash);
            if (asset.relativePath != expectedRelativePath)
                return detail::semanticInvalid ("stored Asset relative path does not match its content hash");

            std::filesystem::path ignored;
            if (auto result = bundlePathForRelativePath (asset.relativePath, ignored); ! result.ok())
                return result;

            out.push_back (std::move (asset));
        }
    }

    [[nodiscard]] BundleResult verifyStoredAssetFiles (const std::vector<StoredAssetFile>& assets) const
    {
        for (const StoredAssetFile& asset : assets)
        {
            std::filesystem::path path;
            if (auto result = bundlePathForRelativePath (asset.relativePath, path); ! result.ok())
                return result;
            if (auto result = verifyAssetFile (asset.hash, path); ! result.ok())
                return result;
        }

        return detail::ok();
    }

    [[nodiscard]] bool isStoredAssetRelativePath (const std::vector<StoredAssetFile>& assets, std::string_view relativePath) const
    {
        return std::find_if (assets.begin(), assets.end(), [relativePath] (const StoredAssetFile& asset)
        {
            return asset.relativePath == relativePath;
        }) != assets.end();
    }

    [[nodiscard]] BundleResult deletePendingFsOp (sqlite3_int64 rowId)
    {
        detail::Statement stmt (db_, "DELETE FROM pending_fs_ops WHERE id = ?;");
        if (auto result = stmt.bindInt64 (1, rowId); ! result.ok())
            return result;

        return detail::expectDone (db_, stmt);
    }

    [[nodiscard]] BundleResult reconcilePendingFsOps (const std::vector<StoredAssetFile>& assets)
    {
        struct PendingRow
        {
            sqlite3_int64 id = 0;
            std::string   tempRelativePath;
            std::string   finalRelativePath;
        };

        std::vector<PendingRow> pending;
        {
            detail::Statement stmt (
                db_,
                "SELECT id, temp_relative_path, final_relative_path FROM pending_fs_ops "
                "WHERE committed = 0 AND op_kind = 0 ORDER BY id;");

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                PendingRow row;
                row.id = sqlite3_column_int64 (stmt.get(), 0);

                const unsigned char* temp = sqlite3_column_text (stmt.get(), 1);
                const unsigned char* final = sqlite3_column_text (stmt.get(), 2);
                row.tempRelativePath = temp == nullptr ? std::string {} : reinterpret_cast<const char*> (temp);
                row.finalRelativePath = final == nullptr ? std::string {} : reinterpret_cast<const char*> (final);
                pending.push_back (std::move (row));
            }
        }

        for (const PendingRow& row : pending)
        {
            std::filesystem::path tempPath;
            if (auto result = bundlePathForRelativePath (row.tempRelativePath, tempPath); ! result.ok())
                return result;
            if (auto result = removeFileIfExists (tempPath); ! result.ok())
                return result;

            std::filesystem::path finalPath;
            if (auto result = bundlePathForRelativePath (row.finalRelativePath, finalPath); ! result.ok())
                return result;
            if (! isStoredAssetRelativePath (assets, row.finalRelativePath))
            {
                if (auto result = moveFileToTrash (finalPath); ! result.ok())
                    return result;
            }

            if (auto result = deletePendingFsOp (row.id); ! result.ok())
                return result;
        }

        return detail::ok();
    }

    [[nodiscard]] BundleResult sweepOrphanAssetFiles (const std::vector<StoredAssetFile>& assets)
    {
        const std::filesystem::path audioDir = bundlePath_ / "audio";
        std::error_code ec;
        std::filesystem::create_directories (audioDir, ec);
        if (ec)
            return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

        std::filesystem::directory_iterator it (audioDir, ec);
        if (ec)
            return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };

        const std::filesystem::directory_iterator end;
        for (; it != end; it.increment (ec))
        {
            if (ec)
                return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };
            if (! it->is_regular_file (ec))
            {
                if (ec)
                    return BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() };
                continue;
            }

            const std::filesystem::path path = it->path();
            const std::string relativePath = "audio/" + path.filename().generic_string();
            if (path.extension() == ".tmp")
            {
                if (auto result = removeFileIfExists (path); ! result.ok())
                    return result;
                continue;
            }

            if (path.extension() == ".asset" && ! isStoredAssetRelativePath (assets, relativePath))
            {
                if (auto result = moveFileToTrash (path); ! result.ok())
                    return result;
            }
        }

        return detail::ok();
    }

    [[nodiscard]] BundleResult reconcileBundleFilesystem()
    {
        std::vector<StoredAssetFile> assets;
        if (auto result = loadStoredAssetFiles (assets); ! result.ok())
            return result;
        if (auto result = verifyStoredAssetFiles (assets); ! result.ok())
            return result;
        if (auto result = reconcilePendingFsOps (assets); ! result.ok())
            return result;
        return sweepOrphanAssetFiles (assets);
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

    [[nodiscard]] BundleResult validateStoredTypes() const
    {
        bool typeProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM project WHERE typeof(id) != 'blob' OR typeof(sample_rate_hz) NOT IN ('integer', 'real') "
                "UNION ALL SELECT 1 FROM assets WHERE typeof(id) != 'blob' OR typeof(content_hash) != 'blob' OR typeof(frames) != 'integer' OR typeof(sample_rate_hz) NOT IN ('integer', 'real') OR typeof(channels) != 'integer' "
                "UNION ALL SELECT 1 FROM clips WHERE typeof(id) != 'blob' OR typeof(asset_id) != 'blob' OR typeof(timeline_start) != 'integer' OR typeof(timeline_length) != 'integer' OR typeof(src_offset) != 'integer' OR typeof(src_len) != 'integer' OR typeof(gain) NOT IN ('integer', 'real') OR typeof(fade_in) != 'integer' OR typeof(fade_out) != 'integer' OR typeof(time_base) != 'integer' "
                "LIMIT 1;",
                typeProblem);
            ! result.ok())
            return result;

        if (typeProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "persisted Project value row uses a non-canonical storage type" };

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
