// YES DAW - SQLite Project bundle persistence slice (ADR-0012).
//
// This is a narrow, headless control-thread surface: bring up a `.yesdaw` bundle database, run
// append-only migrations, enforce foreign keys, validate the existing Project value
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
inline constexpr int          kCodeSchemaVersion = 8;
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
static_assert (static_cast<std::uint8_t> (engine::AutomationTargetRole::TrackFader) == 0u);
static_assert (static_cast<std::uint8_t> (engine::AutomationTargetRole::TrackPan) == 1u);
static_assert (static_cast<std::uint8_t> (engine::AutomationTargetRole::SendLevel) == 2u);
static_assert (static_cast<std::uint8_t> (engine::AutomationTargetRole::BusFader) == 3u);
static_assert (static_cast<std::uint8_t> (engine::AutomationTargetRole::BusPan) == 4u);
static_assert (static_cast<std::uint8_t> (engine::AutomationTargetRole::FxInsertParam) == 5u);
static_assert (static_cast<std::uint8_t> (engine::RecordingMonitoringPolicy::Off) == 0u);
static_assert (static_cast<std::uint8_t> (engine::RecordingMonitoringPolicy::DirectInput) == 1u);
static_assert (static_cast<std::uint8_t> (engine::RecordingMonitoringPolicy::LatencyCompensated) == 2u);

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

enum class PluginStateFormat : std::uint8_t
{
    Vst3 = 0,
    AudioUnit,
    Clap
};

enum class PluginStateChunkKind : std::int32_t
{
    Vst3Component = 0,
    Vst3Controller = 1,
    AudioUnitState = 2,
    ClapState = 3
};

enum class PluginStateRestoreStatus : std::uint8_t
{
    Ready = 0,
    Missing,
    Unreadable
};

struct PluginStateChunkRecord
{
    engine::EntityId           nodeId;
    PluginStateFormat          format = PluginStateFormat::Vst3;
    std::string                pluginUid;
    std::string                pluginVersion;
    PluginStateChunkKind       chunkKind = PluginStateChunkKind::Vst3Component;
    std::vector<std::uint8_t>  bytes;
    std::uint32_t              crc32 = 0;
};

struct PluginBlacklistEntry
{
    PluginStateFormat format = PluginStateFormat::Vst3;
    std::string       pluginUid;
    std::string       pluginVersion;
    std::string       reason;
};

struct PluginStateRestoreChunk
{
    PluginStateRestoreStatus status = PluginStateRestoreStatus::Missing;
    PluginStateChunkRecord   chunk;
    std::string              message;

    [[nodiscard]] bool ready() const noexcept { return status == PluginStateRestoreStatus::Ready; }
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

inline std::uint32_t crc32Bytes (std::span<const std::uint8_t> bytes) noexcept
{
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const std::uint8_t byte : bytes)
    {
        crc ^= static_cast<std::uint32_t> (byte);
        for (int bit = 0; bit < 8; ++bit)
        {
            if ((crc & 1u) != 0u)
                crc = (crc >> 1u) ^ 0xEDB88320u;
            else
                crc >>= 1u;
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

inline bool pluginStateFormatIsKnown (PluginStateFormat format) noexcept
{
    switch (format)
    {
        case PluginStateFormat::Vst3:
        case PluginStateFormat::AudioUnit:
        case PluginStateFormat::Clap:
            return true;
    }

    return false;
}

inline std::string_view pluginStateFormatStorageName (PluginStateFormat format) noexcept
{
    switch (format)
    {
        case PluginStateFormat::Vst3:
            return "vst3";
        case PluginStateFormat::AudioUnit:
            return "au";
        case PluginStateFormat::Clap:
            return "clap";
    }

    return {};
}

inline bool pluginStateFormatFromStorage (std::string_view text, PluginStateFormat& out) noexcept
{
    if (text == "vst3")
    {
        out = PluginStateFormat::Vst3;
        return true;
    }
    if (text == "au")
    {
        out = PluginStateFormat::AudioUnit;
        return true;
    }
    if (text == "clap")
    {
        out = PluginStateFormat::Clap;
        return true;
    }

    return false;
}

inline bool pluginStateChunkKindIsKnown (sqlite3_int64 kind) noexcept
{
    switch (kind)
    {
        case static_cast<sqlite3_int64> (PluginStateChunkKind::Vst3Component):
        case static_cast<sqlite3_int64> (PluginStateChunkKind::Vst3Controller):
        case static_cast<sqlite3_int64> (PluginStateChunkKind::AudioUnitState):
        case static_cast<sqlite3_int64> (PluginStateChunkKind::ClapState):
            return true;
    }

    return false;
}

inline bool pluginStateChunkKindMatchesFormat (PluginStateFormat format, PluginStateChunkKind kind) noexcept
{
    switch (format)
    {
        case PluginStateFormat::Vst3:
            return kind == PluginStateChunkKind::Vst3Component || kind == PluginStateChunkKind::Vst3Controller;
        case PluginStateFormat::AudioUnit:
            return kind == PluginStateChunkKind::AudioUnitState;
        case PluginStateFormat::Clap:
            return kind == PluginStateChunkKind::ClapState;
    }

    return false;
}

inline bool pluginBlacklistKeyIsValid (PluginStateFormat format,
                                       std::string_view pluginUid,
                                       std::string_view pluginVersion) noexcept
{
    return pluginStateFormatIsKnown (format) && ! pluginUid.empty() && ! pluginVersion.empty();
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

    for (const engine::Track& track : project.tracks)
    {
        if (! track.isValid())
            return false;
    }

    for (const engine::Bus& bus : project.buses)
    {
        if (! bus.isValid())
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

    for (const engine::TempoChange& tempo : project.tempoMap)
    {
        if (! tempo.hasValidBpm()
            || (tempo.curveToNext != engine::TempoCurve::Jump && tempo.curveToNext != engine::TempoCurve::LinearRamp))
            return false;
    }

    for (const engine::MeterChange& meter : project.meterMap)
        if (! meter.isValid())
            return false;

    for (const engine::Marker& marker : project.markers)
        if (! marker.id.isValid())
            return false;

    for (const engine::MidiClip& midiClip : project.midiClips)
        if (! midiClip.isValid())
            return false;

    for (const engine::RecordingTake& take : project.recordingTakes)
        if (! take.isValid() || ! fitsSqliteInteger (take.frameCount))
            return false;

    for (const engine::ProjectRecordingCompSegment& segment : project.recordingCompSegments)
    {
        if (! segment.isValid()
            || ! fitsSqliteInteger (segment.sourceOffset)
            || segment.timelineLength > std::numeric_limits<sqlite3_int64>::max())
            return false;
    }

    const auto fxChainFits = [] (const std::vector<engine::FxInsert>& chain) noexcept
    {
        for (std::size_t position = 0; position < chain.size(); ++position)
        {
            if (position > static_cast<std::size_t> (std::numeric_limits<sqlite3_int64>::max()))
                return false;

            const engine::FxInsert& insert = chain[position];
            if (! insert.isValid())
                return false;

            for (const auto& [paramId, value] : insert.normalizedParams)
            {
                (void) paramId;
                if (! engine::normalizedFxParamValueIsValid (value))
                    return false;
            }
        }

        return true;
    };

    for (const engine::Track& track : project.tracks)
        if (! fxChainFits (track.strip.fxChain))
            return false;

    for (const engine::Bus& bus : project.buses)
        if (! fxChainFits (bus.strip.fxChain))
            return false;

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

inline constexpr std::string_view kSchemaV2Sql = R"SQL(
CREATE TABLE plugin_blacklist (
  format TEXT NOT NULL CHECK (format IN ('vst3', 'au', 'clap')),
  plugin_uid TEXT NOT NULL CHECK (length(plugin_uid) > 0),
  plugin_version TEXT NOT NULL CHECK (length(plugin_version) > 0),
  reason TEXT NOT NULL DEFAULT '',
  created_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  updated_at_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
  PRIMARY KEY (format, plugin_uid, plugin_version)
);
)SQL";

inline constexpr std::string_view kSchemaV3Sql = R"SQL(
CREATE TABLE midi_clips (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  track_id BLOB NOT NULL CHECK (length(track_id) = 16),
  timeline_start INTEGER NOT NULL,
  timeline_length INTEGER NOT NULL CHECK (timeline_length >= 0),
  time_base INTEGER NOT NULL CHECK (time_base IN (0, 1))
);

CREATE TABLE midi_notes (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  clip_id BLOB NOT NULL,
  start_tick INTEGER NOT NULL CHECK (start_tick >= 0),
  length_ticks INTEGER NOT NULL CHECK (length_ticks >= 0),
  key INTEGER NOT NULL CHECK (key >= 0 AND key <= 127),
  pitch_note REAL NOT NULL,
  normalized_velocity REAL NOT NULL CHECK (normalized_velocity >= 0 AND normalized_velocity <= 1),
  port_index INTEGER NOT NULL CHECK (port_index >= -1),
  channel INTEGER NOT NULL CHECK (channel >= -1 AND channel <= 15),
  FOREIGN KEY (clip_id) REFERENCES midi_clips(id) ON UPDATE RESTRICT ON DELETE CASCADE
);
CREATE INDEX midi_notes_clip_id_idx ON midi_notes(clip_id);
)SQL";

inline constexpr std::string_view kSchemaV4Sql = R"SQL(
CREATE TABLE tracks (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  name TEXT NOT NULL,
  linear_gain REAL NOT NULL CHECK (linear_gain >= 0 AND linear_gain <= 1000.0),
  pan REAL NOT NULL CHECK (pan >= -1.0 AND pan <= 1.0),
  muted INTEGER NOT NULL CHECK (muted IN (0, 1)),
  soloed INTEGER NOT NULL CHECK (soloed IN (0, 1)),
  solo_safe INTEGER NOT NULL CHECK (solo_safe IN (0, 1))
);

CREATE TABLE buses (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  name TEXT NOT NULL,
  linear_gain REAL NOT NULL CHECK (linear_gain >= 0 AND linear_gain <= 1000.0),
  pan REAL NOT NULL CHECK (pan >= -1.0 AND pan <= 1.0),
  muted INTEGER NOT NULL CHECK (muted IN (0, 1)),
  soloed INTEGER NOT NULL CHECK (soloed IN (0, 1)),
  solo_safe INTEGER NOT NULL CHECK (solo_safe IN (0, 1))
);

INSERT OR IGNORE INTO tracks(id, name, linear_gain, pan, muted, soloed, solo_safe)
SELECT X'5945534441575F415544494F5F303031', 'Audio 1', 1.0, 0.0, 0, 0, 0
WHERE EXISTS (SELECT 1 FROM clips);

INSERT OR IGNORE INTO tracks(id, name, linear_gain, pan, muted, soloed, solo_safe)
SELECT DISTINCT track_id, 'MIDI Track', 1.0, 0.0, 0, 0, 0
FROM midi_clips;

CREATE TABLE clips_v4 (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  asset_id BLOB NOT NULL,
  track_id BLOB NOT NULL CHECK (length(track_id) = 16),
  timeline_start INTEGER NOT NULL,
  timeline_length INTEGER NOT NULL CHECK (timeline_length >= 0),
  src_offset INTEGER NOT NULL CHECK (src_offset >= 0),
  src_len INTEGER NOT NULL CHECK (src_len >= 0),
  gain REAL NOT NULL CHECK (gain >= 0),
  fade_in INTEGER NOT NULL CHECK (fade_in >= 0),
  fade_out INTEGER NOT NULL CHECK (fade_out >= 0),
  time_base INTEGER NOT NULL CHECK (time_base IN (0, 1)),
  FOREIGN KEY (asset_id) REFERENCES assets(id) ON UPDATE RESTRICT ON DELETE RESTRICT,
  FOREIGN KEY (track_id) REFERENCES tracks(id) ON UPDATE RESTRICT ON DELETE RESTRICT
);

INSERT INTO clips_v4(id, asset_id, track_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base)
SELECT id, asset_id, X'5945534441575F415544494F5F303031', timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base
FROM clips;

DROP TABLE clips;
ALTER TABLE clips_v4 RENAME TO clips;
CREATE INDEX clips_asset_id_idx ON clips(asset_id);
CREATE INDEX clips_track_id_idx ON clips(track_id);
CREATE INDEX midi_clips_track_id_idx ON midi_clips(track_id);
)SQL";

inline constexpr std::string_view kSchemaV5Sql = R"SQL(
CREATE TABLE recording_takes (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  asset_id BLOB NOT NULL,
  track_id BLOB NOT NULL,
  clip_id BLOB NOT NULL UNIQUE,
  timeline_start INTEGER NOT NULL CHECK (timeline_start >= 0),
  frame_count INTEGER NOT NULL CHECK (frame_count > 0),
  take_ordinal INTEGER NOT NULL CHECK (take_ordinal >= 0),
  input_channel INTEGER NOT NULL CHECK (input_channel >= 0),
  device_stable_id INTEGER NOT NULL CHECK (device_stable_id >= 0),
  monitoring_policy INTEGER NOT NULL CHECK (monitoring_policy IN (0, 1, 2)),
  FOREIGN KEY (asset_id) REFERENCES assets(id) ON UPDATE RESTRICT ON DELETE RESTRICT,
  FOREIGN KEY (track_id) REFERENCES tracks(id) ON UPDATE RESTRICT ON DELETE RESTRICT,
  FOREIGN KEY (clip_id) REFERENCES clips(id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX recording_takes_asset_id_idx ON recording_takes(asset_id);
CREATE INDEX recording_takes_track_id_idx ON recording_takes(track_id);
)SQL";

inline constexpr std::string_view kSchemaV6Sql = R"SQL(
CREATE TABLE recording_comp_segments (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  take_id BLOB NOT NULL,
  sort_index INTEGER NOT NULL CHECK (sort_index >= 0),
  timeline_start INTEGER NOT NULL CHECK (timeline_start >= 0),
  timeline_length INTEGER NOT NULL CHECK (timeline_length > 0),
  source_offset INTEGER NOT NULL CHECK (source_offset >= 0),
  FOREIGN KEY (take_id) REFERENCES recording_takes(id) ON UPDATE RESTRICT ON DELETE RESTRICT
);
CREATE INDEX recording_comp_segments_take_id_idx ON recording_comp_segments(take_id);
)SQL";

inline constexpr std::string_view kSchemaV7Sql = R"SQL(
CREATE TABLE fx_inserts (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  owner_entity BLOB NOT NULL CHECK (length(owner_entity) = 16),
  position INTEGER NOT NULL CHECK (position >= 0),
  kind INTEGER NOT NULL,
  enabled INTEGER NOT NULL CHECK (enabled IN (0, 1)),
  UNIQUE(owner_entity, position)
);
CREATE INDEX fx_inserts_owner_entity_idx ON fx_inserts(owner_entity);

CREATE TABLE fx_insert_params (
  insert_id BLOB NOT NULL CHECK (length(insert_id) = 16),
  param_id INTEGER NOT NULL CHECK (param_id >= 0),
  value REAL NOT NULL CHECK(value>=0 AND value<=1),
  PRIMARY KEY(insert_id, param_id)
);
)SQL";

inline constexpr std::string_view kSchemaV8Sql = R"SQL(
CREATE TABLE automation_lanes (
  id BLOB PRIMARY KEY CHECK (length(id) = 16),
  owner_entity BLOB NOT NULL CHECK (length(owner_entity) = 16),
  target_role INTEGER NOT NULL CHECK (target_role IN (0, 1, 2, 3, 4, 5)),
  param_id INTEGER NOT NULL CHECK (param_id >= 0),
  UNIQUE(owner_entity, target_role, param_id)
);
CREATE INDEX automation_lanes_owner_entity_idx ON automation_lanes(owner_entity);

CREATE TABLE automation_breakpoints (
  lane_id BLOB NOT NULL CHECK (length(lane_id) = 16),
  tick INTEGER NOT NULL CHECK (tick >= 0),
  value REAL NOT NULL CHECK(value>=0 AND value<=1),
  curve_type INTEGER NOT NULL CHECK(curve_type IN (0,1)),
  PRIMARY KEY(lane_id, tick),
  FOREIGN KEY(lane_id) REFERENCES automation_lanes(id) ON UPDATE RESTRICT ON DELETE CASCADE
);
)SQL";

struct SchemaMigration
{
    int              toVersion = 0;
    std::string_view sql;
};

inline constexpr std::array<SchemaMigration, 8> kMigrations {
    SchemaMigration { 1, kSchemaV1Sql },
    SchemaMigration { 2, kSchemaV2Sql },
    SchemaMigration { 3, kSchemaV3Sql },
    SchemaMigration { 4, kSchemaV4Sql },
    SchemaMigration { 5, kSchemaV5Sql },
    SchemaMigration { 6, kSchemaV6Sql },
    SchemaMigration { 7, kSchemaV7Sql },
    SchemaMigration { 8, kSchemaV8Sql },
};

inline PluginStateRestoreChunk decodePluginStateChunkRow (sqlite3_stmt* stmt)
{
    PluginStateRestoreChunk out;
    out.status = PluginStateRestoreStatus::Unreadable;

    const auto unreadable = [&out] (std::string message)
    {
        out.chunk.bytes.clear();
        out.message = std::move (message);
        return out;
    };

    if (sqlite3_column_type (stmt, 0) != SQLITE_BLOB)
        return unreadable ("plugin state node_id is not stored as a blob");
    const int nodeBytes = sqlite3_column_bytes (stmt, 0);
    const void* nodeRaw = sqlite3_column_blob (stmt, 0);
    if (nodeBytes != static_cast<int> (engine::EntityId::kNumBytes) || nodeRaw == nullptr)
        return unreadable ("plugin state node_id is not a persistent 16-byte Entity ID");

    const auto* nodeData = static_cast<const std::uint8_t*> (nodeRaw);
    for (std::size_t i = 0; i < out.chunk.nodeId.bytes.size(); ++i)
        out.chunk.nodeId.bytes[i] = nodeData[i];

    if (sqlite3_column_type (stmt, 1) != SQLITE_TEXT)
        return unreadable ("plugin state format is not stored as text");
    const unsigned char* formatText = sqlite3_column_text (stmt, 1);
    if (formatText == nullptr)
        return unreadable ("plugin state format is missing");
    const std::string_view formatView {
        reinterpret_cast<const char*> (formatText),
        static_cast<std::size_t> (sqlite3_column_bytes (stmt, 1)),
    };
    if (! pluginStateFormatFromStorage (formatView, out.chunk.format))
        return unreadable ("plugin state format is unknown");

    if (sqlite3_column_type (stmt, 2) != SQLITE_TEXT)
        return unreadable ("plugin state plugin_uid is not stored as text");
    const unsigned char* uidText = sqlite3_column_text (stmt, 2);
    if (uidText == nullptr)
        return unreadable ("plugin state plugin_uid is missing");
    out.chunk.pluginUid.assign (reinterpret_cast<const char*> (uidText),
                                static_cast<std::size_t> (sqlite3_column_bytes (stmt, 2)));

    if (sqlite3_column_type (stmt, 3) != SQLITE_TEXT)
        return unreadable ("plugin state plugin_version is not stored as text");
    const unsigned char* versionText = sqlite3_column_text (stmt, 3);
    if (versionText == nullptr)
        return unreadable ("plugin state plugin_version is missing");
    out.chunk.pluginVersion.assign (reinterpret_cast<const char*> (versionText),
                                    static_cast<std::size_t> (sqlite3_column_bytes (stmt, 3)));

    if (sqlite3_column_type (stmt, 4) != SQLITE_INTEGER)
        return unreadable ("plugin state chunk_kind is not stored as an integer");
    const sqlite3_int64 rawKind = sqlite3_column_int64 (stmt, 4);
    if (! pluginStateChunkKindIsKnown (rawKind))
        return unreadable ("plugin state chunk_kind is unknown");
    out.chunk.chunkKind = static_cast<PluginStateChunkKind> (rawKind);
    if (! pluginStateChunkKindMatchesFormat (out.chunk.format, out.chunk.chunkKind))
        return unreadable ("plugin state chunk_kind does not match format");

    if (sqlite3_column_type (stmt, 5) != SQLITE_INTEGER)
        return unreadable ("plugin state chunk_len is not stored as an integer");
    const sqlite3_int64 declaredLen = sqlite3_column_int64 (stmt, 5);
    if (declaredLen < 0)
        return unreadable ("plugin state chunk_len is negative");

    if (sqlite3_column_type (stmt, 6) != SQLITE_INTEGER)
        return unreadable ("plugin state crc32 is not stored as an integer");
    const sqlite3_int64 storedCrc = sqlite3_column_int64 (stmt, 6);
    if (storedCrc < 0 || storedCrc > static_cast<sqlite3_int64> (std::numeric_limits<std::uint32_t>::max()))
        return unreadable ("plugin state crc32 is outside uint32 range");

    if (sqlite3_column_type (stmt, 7) != SQLITE_BLOB)
        return unreadable ("plugin state data is not stored as a blob");
    const int dataBytes = sqlite3_column_bytes (stmt, 7);
    if (declaredLen != static_cast<sqlite3_int64> (dataBytes))
        return unreadable ("plugin state chunk_len does not match data length");

    const void* dataRaw = sqlite3_column_blob (stmt, 7);
    if (dataBytes > 0 && dataRaw == nullptr)
        return unreadable ("plugin state data is missing");

    std::vector<std::uint8_t> bytes (static_cast<std::size_t> (dataBytes));
    if (dataBytes > 0)
    {
        const auto* data = static_cast<const std::uint8_t*> (dataRaw);
        std::copy (data, data + dataBytes, bytes.begin());
    }

    out.chunk.crc32 = static_cast<std::uint32_t> (storedCrc);
    if (crc32Bytes (std::span<const std::uint8_t> (bytes.data(), bytes.size())) != out.chunk.crc32)
        return unreadable ("plugin state crc32 does not match data");

    out.status = PluginStateRestoreStatus::Ready;
    out.chunk.bytes = std::move (bytes);
    out.message.clear();
    return out;
}

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

    [[nodiscard]] BundleResult writePluginStateChunk (const PluginStateChunkRecord& chunk)
    {
        if (! chunk.nodeId.isValid()
            || ! detail::pluginStateFormatIsKnown (chunk.format)
            || ! detail::pluginStateChunkKindIsKnown (static_cast<sqlite3_int64> (chunk.chunkKind))
            || ! detail::pluginStateChunkKindMatchesFormat (chunk.format, chunk.chunkKind)
            || chunk.pluginUid.empty()
            || ! detail::fitsSqliteInteger (static_cast<std::uint64_t> (chunk.bytes.size())))
        {
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "Plugin state chunk metadata violates ADR-0013" };
        }

        const std::uint32_t crc = detail::crc32Bytes (std::span<const std::uint8_t> (chunk.bytes.data(), chunk.bytes.size()));

        detail::Statement stmt (
            db_,
            "INSERT INTO plugin_state_chunks(node_id, format, plugin_uid, plugin_version, chunk_kind, chunk_len, crc32, data) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(node_id, chunk_kind) DO UPDATE SET "
            "format = excluded.format, "
            "plugin_uid = excluded.plugin_uid, "
            "plugin_version = excluded.plugin_version, "
            "chunk_len = excluded.chunk_len, "
            "crc32 = excluded.crc32, "
            "data = excluded.data;");

        if (auto result = stmt.bindBlob (1, chunk.nodeId.bytes); ! result.ok())
            return result;
        if (auto result = stmt.bindText (2, detail::pluginStateFormatStorageName (chunk.format)); ! result.ok())
            return result;
        if (auto result = stmt.bindText (3, chunk.pluginUid); ! result.ok())
            return result;
        if (auto result = stmt.bindText (4, chunk.pluginVersion); ! result.ok())
            return result;
        if (auto result = stmt.bindInt64 (5, static_cast<sqlite3_int64> (chunk.chunkKind)); ! result.ok())
            return result;
        if (auto result = stmt.bindInt64 (6, static_cast<sqlite3_int64> (chunk.bytes.size())); ! result.ok())
            return result;
        if (auto result = stmt.bindInt64 (7, static_cast<sqlite3_int64> (crc)); ! result.ok())
            return result;
        if (auto result = stmt.bindBlob (8, std::span<const std::uint8_t> (chunk.bytes.data(), chunk.bytes.size())); ! result.ok())
            return result;

        return detail::expectDone (db_, stmt);
    }

    [[nodiscard]] BundleResult readPluginStateChunk (engine::EntityId nodeId,
                                                     PluginStateChunkKind chunkKind,
                                                     PluginStateRestoreChunk& out) const
    {
        out = {};
        out.status = PluginStateRestoreStatus::Missing;
        out.chunk.nodeId = nodeId;
        out.chunk.chunkKind = chunkKind;

        if (! nodeId.isValid() || ! detail::pluginStateChunkKindIsKnown (static_cast<sqlite3_int64> (chunkKind)))
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "Plugin state lookup metadata violates ADR-0013" };

        detail::Statement stmt (
            db_,
            "SELECT node_id, format, plugin_uid, plugin_version, chunk_kind, chunk_len, crc32, data "
            "FROM plugin_state_chunks WHERE node_id = ? AND chunk_kind = ?;");
        if (auto result = stmt.bindBlob (1, nodeId.bytes); ! result.ok())
            return result;
        if (auto result = stmt.bindInt64 (2, static_cast<sqlite3_int64> (chunkKind)); ! result.ok())
            return result;

        const int step = stmt.step();
        if (step == SQLITE_DONE)
        {
            out.message = "plugin state chunk is missing; plugin should load defaults";
            return detail::ok();
        }
        if (step != SQLITE_ROW)
            return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

        out = detail::decodePluginStateChunkRow (stmt.get());
        return detail::ok();
    }

    [[nodiscard]] BundleResult readPluginStateChunksForNode (engine::EntityId nodeId, std::vector<PluginStateRestoreChunk>& out) const
    {
        out.clear();
        if (! nodeId.isValid())
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "Plugin state node_id violates ADR-0013" };

        detail::Statement stmt (
            db_,
            "SELECT node_id, format, plugin_uid, plugin_version, chunk_kind, chunk_len, crc32, data "
            "FROM plugin_state_chunks WHERE node_id = ? "
            "ORDER BY CASE chunk_kind WHEN 0 THEN 0 WHEN 1 THEN 1 ELSE 2 END, chunk_kind;");
        if (auto result = stmt.bindBlob (1, nodeId.bytes); ! result.ok())
            return result;

        while (true)
        {
            const int step = stmt.step();
            if (step == SQLITE_DONE)
                return detail::ok();
            if (step != SQLITE_ROW)
                return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

            out.push_back (detail::decodePluginStateChunkRow (stmt.get()));
        }
    }

    [[nodiscard]] BundleResult writePluginBlacklistEntry (const PluginBlacklistEntry& entry)
    {
        if (! detail::pluginBlacklistKeyIsValid (entry.format, entry.pluginUid, entry.pluginVersion))
        {
            return BundleResult { BundleStatus::SemanticInvalid,
                                  SQLITE_CONSTRAINT,
                                  kCodeSchemaVersion,
                                  "Plugin blacklist identity must be keyed by format, plugin_uid, and plugin_version" };
        }

        detail::Statement stmt (
            db_,
            "INSERT INTO plugin_blacklist(format, plugin_uid, plugin_version, reason) "
            "VALUES (?, ?, ?, ?) "
            "ON CONFLICT(format, plugin_uid, plugin_version) DO UPDATE SET "
            "reason = excluded.reason, "
            "updated_at_utc = strftime('%Y-%m-%dT%H:%M:%fZ', 'now');");

        if (auto result = stmt.bindText (1, detail::pluginStateFormatStorageName (entry.format)); ! result.ok())
            return result;
        if (auto result = stmt.bindText (2, entry.pluginUid); ! result.ok())
            return result;
        if (auto result = stmt.bindText (3, entry.pluginVersion); ! result.ok())
            return result;
        if (auto result = stmt.bindText (4, entry.reason); ! result.ok())
            return result;

        return detail::expectDone (db_, stmt);
    }

    [[nodiscard]] BundleResult pluginBlacklistContains (PluginStateFormat format,
                                                        std::string_view pluginUid,
                                                        std::string_view pluginVersion,
                                                        bool& out) const
    {
        out = false;
        if (! detail::pluginBlacklistKeyIsValid (format, pluginUid, pluginVersion))
        {
            return BundleResult { BundleStatus::SemanticInvalid,
                                  SQLITE_CONSTRAINT,
                                  kCodeSchemaVersion,
                                  "Plugin blacklist lookup must use format, plugin_uid, and plugin_version" };
        }

        detail::Statement stmt (
            db_,
            "SELECT 1 FROM plugin_blacklist "
            "WHERE format = ? AND plugin_uid = ? AND plugin_version = ? "
            "LIMIT 1;");

        if (auto result = stmt.bindText (1, detail::pluginStateFormatStorageName (format)); ! result.ok())
            return result;
        if (auto result = stmt.bindText (2, pluginUid); ! result.ok())
            return result;
        if (auto result = stmt.bindText (3, pluginVersion); ! result.ok())
            return result;

        const int step = stmt.step();
        if (step == SQLITE_ROW)
        {
            out = true;
            return detail::ok();
        }
        if (step == SQLITE_DONE)
            return detail::ok();

        return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));
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

        if (auto result = detail::exec (
                db_,
                "DELETE FROM automation_breakpoints; DELETE FROM automation_lanes; "
                "DELETE FROM fx_insert_params; DELETE FROM fx_inserts; "
                "DELETE FROM midi_notes; DELETE FROM midi_clips; DELETE FROM recording_comp_segments; DELETE FROM recording_takes; DELETE FROM clips; "
                "DELETE FROM buses; DELETE FROM tracks; "
                "DELETE FROM tempo_changes; DELETE FROM meter_changes; DELETE FROM markers; "
                "DELETE FROM assets; DELETE FROM project;");
            ! result.ok())
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

        detail::Statement trackStmt (
            db_,
            "INSERT INTO tracks(id, name, linear_gain, pan, muted, soloed, solo_safe) "
            "VALUES (?, ?, ?, ?, ?, ?, ?);");
        for (const engine::Track& track : project.tracks)
        {
            trackStmt.reset();
            if (auto result = trackStmt.bindBlob (1, track.id.bytes); ! result.ok()) { rollback(); return result; }
            if (auto result = trackStmt.bindText (2, track.strip.name); ! result.ok()) { rollback(); return result; }
            if (auto result = trackStmt.bindDouble (3, track.strip.linearGain); ! result.ok()) { rollback(); return result; }
            if (auto result = trackStmt.bindDouble (4, track.strip.pan); ! result.ok()) { rollback(); return result; }
            if (auto result = trackStmt.bindInt64 (5, track.strip.muted ? 1 : 0); ! result.ok()) { rollback(); return result; }
            if (auto result = trackStmt.bindInt64 (6, track.strip.soloed ? 1 : 0); ! result.ok()) { rollback(); return result; }
            if (auto result = trackStmt.bindInt64 (7, track.strip.soloSafe ? 1 : 0); ! result.ok()) { rollback(); return result; }
            if (auto result = detail::expectDone (db_, trackStmt); ! result.ok()) { rollback(); return result; }
        }

        detail::Statement busStmt (
            db_,
            "INSERT INTO buses(id, name, linear_gain, pan, muted, soloed, solo_safe) "
            "VALUES (?, ?, ?, ?, ?, ?, ?);");
        for (const engine::Bus& bus : project.buses)
        {
            busStmt.reset();
            if (auto result = busStmt.bindBlob (1, bus.id.bytes); ! result.ok()) { rollback(); return result; }
            if (auto result = busStmt.bindText (2, bus.strip.name); ! result.ok()) { rollback(); return result; }
            if (auto result = busStmt.bindDouble (3, bus.strip.linearGain); ! result.ok()) { rollback(); return result; }
            if (auto result = busStmt.bindDouble (4, bus.strip.pan); ! result.ok()) { rollback(); return result; }
            if (auto result = busStmt.bindInt64 (5, bus.strip.muted ? 1 : 0); ! result.ok()) { rollback(); return result; }
            if (auto result = busStmt.bindInt64 (6, bus.strip.soloed ? 1 : 0); ! result.ok()) { rollback(); return result; }
            if (auto result = busStmt.bindInt64 (7, bus.strip.soloSafe ? 1 : 0); ! result.ok()) { rollback(); return result; }
            if (auto result = detail::expectDone (db_, busStmt); ! result.ok()) { rollback(); return result; }
        }

        {
            detail::Statement insertStmt (
                db_,
                "INSERT INTO fx_inserts(id, owner_entity, position, kind, enabled) "
                "VALUES (?, ?, ?, ?, ?);");
            detail::Statement paramStmt (
                db_,
                "INSERT INTO fx_insert_params(insert_id, param_id, value) "
                "VALUES (?, ?, ?);");

            const auto writeFxChain = [&] (engine::EntityId ownerId, const std::vector<engine::FxInsert>& chain) -> BundleResult
            {
                for (std::size_t position = 0; position < chain.size(); ++position)
                {
                    const engine::FxInsert& insert = chain[position];
                    insertStmt.reset();
                    if (auto result = insertStmt.bindBlob (1, insert.id.bytes); ! result.ok()) { return result; }
                    if (auto result = insertStmt.bindBlob (2, ownerId.bytes); ! result.ok()) { return result; }
                    if (auto result = insertStmt.bindInt64 (3, static_cast<sqlite3_int64> (position)); ! result.ok()) { return result; }
                    if (auto result = insertStmt.bindInt64 (4, static_cast<sqlite3_int64> (insert.kind)); ! result.ok()) { return result; }
                    if (auto result = insertStmt.bindInt64 (5, insert.enabled ? 1 : 0); ! result.ok()) { return result; }
                    if (auto result = detail::expectDone (db_, insertStmt); ! result.ok()) { return result; }

                    for (const auto& [paramId, value] : insert.normalizedParams)
                    {
                        paramStmt.reset();
                        if (auto result = paramStmt.bindBlob (1, insert.id.bytes); ! result.ok()) { return result; }
                        if (auto result = paramStmt.bindInt64 (2, static_cast<sqlite3_int64> (paramId)); ! result.ok()) { return result; }
                        if (auto result = paramStmt.bindDouble (3, value); ! result.ok()) { return result; }
                        if (auto result = detail::expectDone (db_, paramStmt); ! result.ok()) { return result; }
                    }
                }

                return detail::ok();
            };

            for (const engine::Track& track : project.tracks)
            {
                if (auto result = writeFxChain (track.id, track.strip.fxChain); ! result.ok())
                {
                    rollback();
                    return result;
                }
            }

            for (const engine::Bus& bus : project.buses)
            {
                if (auto result = writeFxChain (bus.id, bus.strip.fxChain); ! result.ok())
                {
                    rollback();
                    return result;
                }
            }
        }

        {
            detail::Statement laneStmt (
                db_,
                "INSERT INTO automation_lanes(id, owner_entity, target_role, param_id) "
                "VALUES (?, ?, ?, ?);");
            detail::Statement pointStmt (
                db_,
                "INSERT INTO automation_breakpoints(lane_id, tick, value, curve_type) "
                "VALUES (?, ?, ?, ?);");

            for (const engine::AutomationLaneData& lane : project.automationLanes)
            {
                laneStmt.reset();
                if (auto result = laneStmt.bindBlob (1, lane.id.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = laneStmt.bindBlob (2, lane.ownerEntity.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = laneStmt.bindInt64 (3, static_cast<sqlite3_int64> (lane.role)); ! result.ok()) { rollback(); return result; }
                if (auto result = laneStmt.bindInt64 (4, static_cast<sqlite3_int64> (lane.paramId)); ! result.ok()) { rollback(); return result; }
                if (auto result = detail::expectDone (db_, laneStmt); ! result.ok()) { rollback(); return result; }

                for (const engine::AutomationBreakpoint& point : lane.points)
                {
                    pointStmt.reset();
                    if (auto result = pointStmt.bindBlob (1, lane.id.bytes); ! result.ok()) { rollback(); return result; }
                    if (auto result = pointStmt.bindInt64 (2, point.tick); ! result.ok()) { rollback(); return result; }
                    if (auto result = pointStmt.bindDouble (3, point.value); ! result.ok()) { rollback(); return result; }
                    if (auto result = pointStmt.bindInt64 (4, static_cast<sqlite3_int64> (point.curveType)); ! result.ok()) { rollback(); return result; }
                    if (auto result = detail::expectDone (db_, pointStmt); ! result.ok()) { rollback(); return result; }
                }
            }
        }

        detail::Statement clipStmt (
            db_,
            "INSERT INTO clips(id, asset_id, track_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");

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
            if (auto result = clipStmt.bindBlob (3, clip.trackId.bytes); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (4, clip.timelineStart); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (5, clip.timelineLength); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (6, static_cast<sqlite3_int64> (clip.srcOffset)); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (7, static_cast<sqlite3_int64> (clip.srcLen)); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindDouble (8, clip.gain); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (9, clip.fadeIn); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (10, clip.fadeOut); ! result.ok())
            {
                rollback();
                return result;
            }
            if (auto result = clipStmt.bindInt64 (11, static_cast<sqlite3_int64> (clip.timeBase)); ! result.ok())
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

        {
            detail::Statement takeStmt (
                db_,
                "INSERT INTO recording_takes(id, asset_id, track_id, clip_id, timeline_start, frame_count, take_ordinal, input_channel, device_stable_id, monitoring_policy) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");

            for (const engine::RecordingTake& take : project.recordingTakes)
            {
                takeStmt.reset();
                if (auto result = takeStmt.bindBlob (1, take.id.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = takeStmt.bindBlob (2, take.assetId.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = takeStmt.bindBlob (3, take.trackId.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = takeStmt.bindBlob (4, take.clipId.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = takeStmt.bindInt64 (5, take.timelineStart); ! result.ok()) { rollback(); return result; }
                if (auto result = takeStmt.bindInt64 (6, static_cast<sqlite3_int64> (take.frameCount)); ! result.ok()) { rollback(); return result; }
                if (auto result = takeStmt.bindInt64 (7, static_cast<sqlite3_int64> (take.takeOrdinal)); ! result.ok()) { rollback(); return result; }
                if (auto result = takeStmt.bindInt64 (8, static_cast<sqlite3_int64> (take.inputChannel)); ! result.ok()) { rollback(); return result; }
                if (auto result = takeStmt.bindInt64 (9, static_cast<sqlite3_int64> (take.deviceStableId)); ! result.ok()) { rollback(); return result; }
                if (auto result = takeStmt.bindInt64 (10, static_cast<sqlite3_int64> (take.monitoringPolicy)); ! result.ok()) { rollback(); return result; }
                if (auto result = detail::expectDone (db_, takeStmt); ! result.ok()) { rollback(); return result; }
            }
        }

        {
            detail::Statement compStmt (
                db_,
                "INSERT INTO recording_comp_segments(id, take_id, sort_index, timeline_start, timeline_length, source_offset) "
                "VALUES (?, ?, ?, ?, ?, ?);");

            for (std::size_t i = 0; i < project.recordingCompSegments.size(); ++i)
            {
                const engine::ProjectRecordingCompSegment& segment = project.recordingCompSegments[i];
                compStmt.reset();
                if (auto result = compStmt.bindBlob (1, segment.id.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = compStmt.bindBlob (2, segment.takeId.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = compStmt.bindInt64 (3, static_cast<sqlite3_int64> (i)); ! result.ok()) { rollback(); return result; }
                if (auto result = compStmt.bindInt64 (4, segment.timelineStart); ! result.ok()) { rollback(); return result; }
                if (auto result = compStmt.bindInt64 (5, segment.timelineLength); ! result.ok()) { rollback(); return result; }
                if (auto result = compStmt.bindInt64 (6, static_cast<sqlite3_int64> (segment.sourceOffset)); ! result.ok()) { rollback(); return result; }
                if (auto result = detail::expectDone (db_, compStmt); ! result.ok()) { rollback(); return result; }
            }
        }

        {
            detail::Statement midiClipStmt (
                db_,
                "INSERT INTO midi_clips(id, track_id, timeline_start, timeline_length, time_base) "
                "VALUES (?, ?, ?, ?, ?);");
            detail::Statement noteStmt (
                db_,
                "INSERT INTO midi_notes(id, clip_id, start_tick, length_ticks, key, pitch_note, normalized_velocity, port_index, channel) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);");

            for (const engine::MidiClip& midiClip : project.midiClips)
            {
                midiClipStmt.reset();
                if (auto result = midiClipStmt.bindBlob (1, midiClip.id.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = midiClipStmt.bindBlob (2, midiClip.trackId.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = midiClipStmt.bindInt64 (3, midiClip.timelineStart); ! result.ok()) { rollback(); return result; }
                if (auto result = midiClipStmt.bindInt64 (4, midiClip.timelineLength); ! result.ok()) { rollback(); return result; }
                if (auto result = midiClipStmt.bindInt64 (5, static_cast<sqlite3_int64> (midiClip.timeBase)); ! result.ok()) { rollback(); return result; }
                if (auto result = detail::expectDone (db_, midiClipStmt); ! result.ok()) { rollback(); return result; }

                for (const engine::Note& note : midiClip.notes)
                {
                    noteStmt.reset();
                    if (auto result = noteStmt.bindBlob (1, note.id.bytes); ! result.ok()) { rollback(); return result; }
                    if (auto result = noteStmt.bindBlob (2, midiClip.id.bytes); ! result.ok()) { rollback(); return result; }
                    if (auto result = noteStmt.bindInt64 (3, note.startTick); ! result.ok()) { rollback(); return result; }
                    if (auto result = noteStmt.bindInt64 (4, note.lengthTicks); ! result.ok()) { rollback(); return result; }
                    if (auto result = noteStmt.bindInt64 (5, note.key); ! result.ok()) { rollback(); return result; }
                    if (auto result = noteStmt.bindDouble (6, note.pitchNote); ! result.ok()) { rollback(); return result; }
                    if (auto result = noteStmt.bindDouble (7, note.normalizedVelocity); ! result.ok()) { rollback(); return result; }
                    if (auto result = noteStmt.bindInt64 (8, note.portIndex); ! result.ok()) { rollback(); return result; }
                    if (auto result = noteStmt.bindInt64 (9, note.channel); ! result.ok()) { rollback(); return result; }
                    if (auto result = detail::expectDone (db_, noteStmt); ! result.ok()) { rollback(); return result; }
                }
            }
        }

        {
            detail::Statement tempoStmt (db_, "INSERT INTO tempo_changes(tick, bpm, curve_to_next) VALUES (?, ?, ?);");
            for (const engine::TempoChange& tempo : project.tempoMap)
            {
                tempoStmt.reset();
                if (auto result = tempoStmt.bindInt64 (1, tempo.tick); ! result.ok()) { rollback(); return result; }
                if (auto result = tempoStmt.bindDouble (2, tempo.bpm); ! result.ok()) { rollback(); return result; }
                if (auto result = tempoStmt.bindInt64 (3, static_cast<sqlite3_int64> (tempo.curveToNext)); ! result.ok()) { rollback(); return result; }
                if (auto result = detail::expectDone (db_, tempoStmt); ! result.ok()) { rollback(); return result; }
            }
        }

        {
            detail::Statement meterStmt (db_, "INSERT INTO meter_changes(tick, numerator, denominator) VALUES (?, ?, ?);");
            for (const engine::MeterChange& meter : project.meterMap)
            {
                meterStmt.reset();
                if (auto result = meterStmt.bindInt64 (1, meter.tick); ! result.ok()) { rollback(); return result; }
                if (auto result = meterStmt.bindInt64 (2, meter.numerator); ! result.ok()) { rollback(); return result; }
                if (auto result = meterStmt.bindInt64 (3, meter.denominator); ! result.ok()) { rollback(); return result; }
                if (auto result = detail::expectDone (db_, meterStmt); ! result.ok()) { rollback(); return result; }
            }
        }

        {
            detail::Statement markerStmt (db_, "INSERT INTO markers(id, tick, name) VALUES (?, ?, ?);");
            for (const engine::Marker& marker : project.markers)
            {
                markerStmt.reset();
                if (auto result = markerStmt.bindBlob (1, marker.id.bytes); ! result.ok()) { rollback(); return result; }
                if (auto result = markerStmt.bindInt64 (2, marker.tick); ! result.ok()) { rollback(); return result; }
                if (auto result = markerStmt.bindText (3, marker.name); ! result.ok()) { rollback(); return result; }
                if (auto result = detail::expectDone (db_, markerStmt); ! result.ok()) { rollback(); return result; }
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
                    "SELECT id, name, linear_gain, pan, muted, soloed, solo_safe FROM tracks ORDER BY rowid;");
                ! result.ok())
                return result;

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::Track track;
                if (auto result = detail::columnBlob (stmt.get(), 0, track.id.bytes, "tracks.id"); ! result.ok())
                    return result;

                const unsigned char* const name = sqlite3_column_text (stmt.get(), 1);
                const int nameBytes = sqlite3_column_bytes (stmt.get(), 1);
                if (name != nullptr && nameBytes > 0)
                    track.strip.name.assign (reinterpret_cast<const char*> (name), static_cast<std::size_t> (nameBytes));

                track.strip.linearGain = static_cast<float> (sqlite3_column_double (stmt.get(), 2));
                track.strip.pan = static_cast<float> (sqlite3_column_double (stmt.get(), 3));
                track.strip.muted = sqlite3_column_int64 (stmt.get(), 4) != 0;
                track.strip.soloed = sqlite3_column_int64 (stmt.get(), 5) != 0;
                track.strip.soloSafe = sqlite3_column_int64 (stmt.get(), 6) != 0;
                project.tracks.push_back (std::move (track));
            }
        }

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (
                    db_,
                    "SELECT id, name, linear_gain, pan, muted, soloed, solo_safe FROM buses ORDER BY rowid;");
                ! result.ok())
                return result;

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::Bus bus;
                if (auto result = detail::columnBlob (stmt.get(), 0, bus.id.bytes, "buses.id"); ! result.ok())
                    return result;

                const unsigned char* const name = sqlite3_column_text (stmt.get(), 1);
                const int nameBytes = sqlite3_column_bytes (stmt.get(), 1);
                if (name != nullptr && nameBytes > 0)
                    bus.strip.name.assign (reinterpret_cast<const char*> (name), static_cast<std::size_t> (nameBytes));

                bus.strip.linearGain = static_cast<float> (sqlite3_column_double (stmt.get(), 2));
                bus.strip.pan = static_cast<float> (sqlite3_column_double (stmt.get(), 3));
                bus.strip.muted = sqlite3_column_int64 (stmt.get(), 4) != 0;
                bus.strip.soloed = sqlite3_column_int64 (stmt.get(), 5) != 0;
                bus.strip.soloSafe = sqlite3_column_int64 (stmt.get(), 6) != 0;
                project.buses.push_back (std::move (bus));
            }
        }

        {
            const auto loadFxChain = [this] (engine::EntityId ownerId, std::vector<engine::FxInsert>& out) -> BundleResult
            {
                detail::Statement insertStmt;
                if (auto result = insertStmt.prepare (
                        db_,
                        "SELECT id, kind, enabled FROM fx_inserts WHERE owner_entity = ? ORDER BY position, rowid;");
                    ! result.ok())
                    return result;
                if (auto result = insertStmt.bindBlob (1, ownerId.bytes); ! result.ok())
                    return result;

                while (true)
                {
                    const int step = insertStmt.step();
                    if (step == SQLITE_DONE)
                        break;
                    if (step != SQLITE_ROW)
                        return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                    engine::FxInsert insert;
                    if (auto result = detail::columnBlob (insertStmt.get(), 0, insert.id.bytes, "fx_inserts.id"); ! result.ok())
                        return result;

                    const sqlite3_int64 kind = sqlite3_column_int64 (insertStmt.get(), 1);
                    if (kind < static_cast<sqlite3_int64> (engine::FxKind::Eq)
                        || kind > static_cast<sqlite3_int64> (engine::FxKind::Limiter))
                        return detail::semanticInvalid ("fx_inserts.kind is outside the Project value range");
                    insert.kind = static_cast<engine::FxKind> (kind);
                    insert.enabled = sqlite3_column_int64 (insertStmt.get(), 2) != 0;

                    detail::Statement paramStmt;
                    if (auto result = paramStmt.prepare (
                            db_,
                            "SELECT param_id, value FROM fx_insert_params WHERE insert_id = ? ORDER BY rowid;");
                        ! result.ok())
                        return result;
                    if (auto result = paramStmt.bindBlob (1, insert.id.bytes); ! result.ok())
                        return result;

                    while (true)
                    {
                        const int paramStep = paramStmt.step();
                        if (paramStep == SQLITE_DONE)
                            break;
                        if (paramStep != SQLITE_ROW)
                            return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                        const sqlite3_int64 paramId = sqlite3_column_int64 (paramStmt.get(), 0);
                        if (paramId < 0 || paramId > std::numeric_limits<std::uint32_t>::max())
                            return detail::semanticInvalid ("fx_insert_params.param_id is outside the Project value range");

                        const double value = sqlite3_column_double (paramStmt.get(), 1);
                        if (! engine::normalizedFxParamValueIsValid (value))
                            return detail::semanticInvalid ("fx_insert_params.value is outside the Project value range");

                        insert.normalizedParams.push_back ({ static_cast<std::uint32_t> (paramId), value });
                    }

                    out.push_back (std::move (insert));
                }

                return detail::ok();
            };

            for (engine::Track& track : project.tracks)
            {
                if (auto result = loadFxChain (track.id, track.strip.fxChain); ! result.ok())
                    return result;
            }

            for (engine::Bus& bus : project.buses)
            {
                if (auto result = loadFxChain (bus.id, bus.strip.fxChain); ! result.ok())
                    return result;
            }
        }

        {
            detail::Statement laneStmt;
            if (auto result = laneStmt.prepare (
                    db_,
                    "SELECT id, owner_entity, target_role, param_id FROM automation_lanes ORDER BY rowid;");
                ! result.ok())
                return result;

            while (true)
            {
                const int step = laneStmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::AutomationLaneData lane;
                if (auto result = detail::columnBlob (laneStmt.get(), 0, lane.id.bytes, "automation_lanes.id"); ! result.ok())
                    return result;
                if (auto result = detail::columnBlob (laneStmt.get(), 1, lane.ownerEntity.bytes, "automation_lanes.owner_entity"); ! result.ok())
                    return result;

                const sqlite3_int64 role = sqlite3_column_int64 (laneStmt.get(), 2);
                if (role < static_cast<sqlite3_int64> (engine::AutomationTargetRole::TrackFader)
                    || role > static_cast<sqlite3_int64> (engine::AutomationTargetRole::FxInsertParam))
                    return detail::semanticInvalid ("automation_lanes.target_role is outside the Project value range");
                lane.role = static_cast<engine::AutomationTargetRole> (role);

                const sqlite3_int64 paramId = sqlite3_column_int64 (laneStmt.get(), 3);
                if (paramId < 0 || paramId > std::numeric_limits<std::uint32_t>::max())
                    return detail::semanticInvalid ("automation_lanes.param_id is outside the Project value range");
                lane.paramId = static_cast<std::uint32_t> (paramId);

                detail::Statement pointStmt;
                if (auto result = pointStmt.prepare (
                        db_,
                        "SELECT tick, value, curve_type FROM automation_breakpoints WHERE lane_id = ? ORDER BY tick, rowid;");
                    ! result.ok())
                    return result;
                if (auto result = pointStmt.bindBlob (1, lane.id.bytes); ! result.ok())
                    return result;

                while (true)
                {
                    const int pointStep = pointStmt.step();
                    if (pointStep == SQLITE_DONE)
                        break;
                    if (pointStep != SQLITE_ROW)
                        return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                    engine::AutomationBreakpoint point;
                    point.tick = sqlite3_column_int64 (pointStmt.get(), 0);
                    point.value = sqlite3_column_double (pointStmt.get(), 1);

                    const sqlite3_int64 curve = sqlite3_column_int64 (pointStmt.get(), 2);
                    if (curve != static_cast<sqlite3_int64> (engine::AutomationCurveType::Linear)
                        && curve != static_cast<sqlite3_int64> (engine::AutomationCurveType::Hold))
                        return detail::semanticInvalid ("automation_breakpoints.curve_type is outside the Project value range");
                    point.curveType = static_cast<engine::AutomationCurveType> (curve);
                    lane.points.push_back (point);
                }

                project.automationLanes.push_back (std::move (lane));
            }
        }

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (
                    db_,
                    "SELECT id, asset_id, track_id, timeline_start, timeline_length, src_offset, src_len, gain, fade_in, fade_out, time_base "
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
                if (auto result = detail::columnBlob (stmt.get(), 2, clip.trackId.bytes, "clips.track_id"); ! result.ok())
                    return result;

                clip.timelineStart = sqlite3_column_int64 (stmt.get(), 3);
                clip.timelineLength = sqlite3_column_int64 (stmt.get(), 4);

                const sqlite3_int64 srcOffset = sqlite3_column_int64 (stmt.get(), 5);
                const sqlite3_int64 srcLen = sqlite3_column_int64 (stmt.get(), 6);
                if (srcOffset < 0 || srcLen < 0)
                    return detail::semanticInvalid ("clips source window is outside the Project value range");
                clip.srcOffset = static_cast<std::uint64_t> (srcOffset);
                clip.srcLen = static_cast<std::uint64_t> (srcLen);

                clip.gain = static_cast<float> (sqlite3_column_double (stmt.get(), 7));
                clip.fadeIn = sqlite3_column_int64 (stmt.get(), 8);
                clip.fadeOut = sqlite3_column_int64 (stmt.get(), 9);

                const sqlite3_int64 timeBase = sqlite3_column_int64 (stmt.get(), 10);
                if (timeBase != static_cast<sqlite3_int64> (engine::TimeBase::TempoLocked)
                    && timeBase != static_cast<sqlite3_int64> (engine::TimeBase::SampleLocked))
                    return detail::semanticInvalid ("clips.time_base is outside the Project value range");
                clip.timeBase = static_cast<engine::TimeBase> (timeBase);

                project.clips.push_back (clip);
            }
        }

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (
                    db_,
                    "SELECT id, asset_id, track_id, clip_id, timeline_start, frame_count, take_ordinal, input_channel, device_stable_id, monitoring_policy "
                    "FROM recording_takes ORDER BY rowid;");
                ! result.ok())
                return result;

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::RecordingTake take;
                if (auto result = detail::columnBlob (stmt.get(), 0, take.id.bytes, "recording_takes.id"); ! result.ok())
                    return result;
                if (auto result = detail::columnBlob (stmt.get(), 1, take.assetId.bytes, "recording_takes.asset_id"); ! result.ok())
                    return result;
                if (auto result = detail::columnBlob (stmt.get(), 2, take.trackId.bytes, "recording_takes.track_id"); ! result.ok())
                    return result;
                if (auto result = detail::columnBlob (stmt.get(), 3, take.clipId.bytes, "recording_takes.clip_id"); ! result.ok())
                    return result;

                take.timelineStart = sqlite3_column_int64 (stmt.get(), 4);

                const sqlite3_int64 frameCount = sqlite3_column_int64 (stmt.get(), 5);
                if (frameCount <= 0)
                    return detail::semanticInvalid ("recording_takes.frame_count is outside the Project value range");
                take.frameCount = static_cast<std::uint64_t> (frameCount);

                const sqlite3_int64 takeOrdinal = sqlite3_column_int64 (stmt.get(), 6);
                const sqlite3_int64 inputChannel = sqlite3_column_int64 (stmt.get(), 7);
                const sqlite3_int64 deviceStableId = sqlite3_column_int64 (stmt.get(), 8);
                if (takeOrdinal < 0
                    || takeOrdinal > std::numeric_limits<std::uint32_t>::max()
                    || inputChannel < 0
                    || inputChannel > std::numeric_limits<std::uint16_t>::max()
                    || deviceStableId < 0
                    || deviceStableId > std::numeric_limits<std::uint32_t>::max())
                    return detail::semanticInvalid ("recording_takes metadata is outside the Project value range");

                take.takeOrdinal = static_cast<std::uint32_t> (takeOrdinal);
                take.inputChannel = static_cast<std::uint16_t> (inputChannel);
                take.deviceStableId = static_cast<std::uint32_t> (deviceStableId);

                const sqlite3_int64 monitoringPolicy = sqlite3_column_int64 (stmt.get(), 9);
                if (monitoringPolicy < static_cast<sqlite3_int64> (engine::RecordingMonitoringPolicy::Off)
                    || monitoringPolicy > static_cast<sqlite3_int64> (engine::RecordingMonitoringPolicy::LatencyCompensated))
                    return detail::semanticInvalid ("recording_takes.monitoring_policy is outside the Project value range");
                take.monitoringPolicy = static_cast<engine::RecordingMonitoringPolicy> (monitoringPolicy);

                project.recordingTakes.push_back (take);
            }
        }

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (
                    db_,
                    "SELECT id, take_id, timeline_start, timeline_length, source_offset "
                    "FROM recording_comp_segments ORDER BY sort_index, rowid;");
                ! result.ok())
                return result;

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::ProjectRecordingCompSegment segment;
                if (auto result = detail::columnBlob (stmt.get(), 0, segment.id.bytes, "recording_comp_segments.id"); ! result.ok())
                    return result;
                if (auto result = detail::columnBlob (stmt.get(), 1, segment.takeId.bytes, "recording_comp_segments.take_id"); ! result.ok())
                    return result;

                segment.timelineStart = sqlite3_column_int64 (stmt.get(), 2);
                segment.timelineLength = sqlite3_column_int64 (stmt.get(), 3);

                const sqlite3_int64 sourceOffset = sqlite3_column_int64 (stmt.get(), 4);
                if (sourceOffset < 0)
                    return detail::semanticInvalid ("recording_comp_segments.source_offset is outside the Project value range");
                segment.sourceOffset = static_cast<std::uint64_t> (sourceOffset);

                project.recordingCompSegments.push_back (segment);
            }
        }

        {
            detail::Statement clipStmt;
            if (auto result = clipStmt.prepare (
                    db_,
                    "SELECT id, track_id, timeline_start, timeline_length, time_base FROM midi_clips ORDER BY rowid;");
                ! result.ok())
                return result;

            while (true)
            {
                const int step = clipStmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::MidiClip midiClip;
                if (auto result = detail::columnBlob (clipStmt.get(), 0, midiClip.id.bytes, "midi_clips.id"); ! result.ok())
                    return result;
                if (auto result = detail::columnBlob (clipStmt.get(), 1, midiClip.trackId.bytes, "midi_clips.track_id"); ! result.ok())
                    return result;

                midiClip.timelineStart = sqlite3_column_int64 (clipStmt.get(), 2);
                midiClip.timelineLength = sqlite3_column_int64 (clipStmt.get(), 3);

                const sqlite3_int64 timeBase = sqlite3_column_int64 (clipStmt.get(), 4);
                if (timeBase != static_cast<sqlite3_int64> (engine::TimeBase::TempoLocked)
                    && timeBase != static_cast<sqlite3_int64> (engine::TimeBase::SampleLocked))
                    return detail::semanticInvalid ("midi_clips.time_base is outside the Project value range");
                midiClip.timeBase = static_cast<engine::TimeBase> (timeBase);

                detail::Statement noteStmt;
                if (auto result = noteStmt.prepare (
                        db_,
                        "SELECT id, start_tick, length_ticks, key, pitch_note, normalized_velocity, port_index, channel "
                        "FROM midi_notes WHERE clip_id = ? ORDER BY rowid;");
                    ! result.ok())
                    return result;
                if (auto result = noteStmt.bindBlob (1, midiClip.id.bytes); ! result.ok())
                    return result;

                while (true)
                {
                    const int noteStep = noteStmt.step();
                    if (noteStep == SQLITE_DONE)
                        break;
                    if (noteStep != SQLITE_ROW)
                        return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                    engine::Note note;
                    if (auto result = detail::columnBlob (noteStmt.get(), 0, note.id.bytes, "midi_notes.id"); ! result.ok())
                        return result;

                    note.startTick = sqlite3_column_int64 (noteStmt.get(), 1);
                    note.lengthTicks = sqlite3_column_int64 (noteStmt.get(), 2);

                    const sqlite3_int64 key = sqlite3_column_int64 (noteStmt.get(), 3);
                    const sqlite3_int64 portIndex = sqlite3_column_int64 (noteStmt.get(), 6);
                    const sqlite3_int64 channel = sqlite3_column_int64 (noteStmt.get(), 7);
                    if (key < 0 || key > 127
                        || portIndex < -1 || portIndex > std::numeric_limits<std::int16_t>::max()
                        || channel < -1 || channel > 15)
                        return detail::semanticInvalid ("midi_notes voice address is outside the Project value range");

                    note.key = static_cast<std::int16_t> (key);
                    note.pitchNote = sqlite3_column_double (noteStmt.get(), 4);
                    note.normalizedVelocity = sqlite3_column_double (noteStmt.get(), 5);
                    note.portIndex = static_cast<std::int16_t> (portIndex);
                    note.channel = static_cast<std::int16_t> (channel);
                    midiClip.notes.push_back (note);
                }

                project.midiClips.push_back (std::move (midiClip));
            }
        }

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (db_, "SELECT tick, bpm, curve_to_next FROM tempo_changes ORDER BY tick;"); ! result.ok())
                return result;

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::TempoChange tempo;
                tempo.tick = sqlite3_column_int64 (stmt.get(), 0);
                tempo.bpm = sqlite3_column_double (stmt.get(), 1);

                const sqlite3_int64 curve = sqlite3_column_int64 (stmt.get(), 2);
                if (curve != static_cast<sqlite3_int64> (engine::TempoCurve::Jump)
                    && curve != static_cast<sqlite3_int64> (engine::TempoCurve::LinearRamp))
                    return detail::semanticInvalid ("tempo_changes.curve_to_next is outside the Project value range");
                tempo.curveToNext = static_cast<engine::TempoCurve> (curve);

                project.tempoMap.push_back (tempo);
            }
        }

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (db_, "SELECT tick, numerator, denominator FROM meter_changes ORDER BY tick;"); ! result.ok())
                return result;

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                const sqlite3_int64 numerator = sqlite3_column_int64 (stmt.get(), 1);
                const sqlite3_int64 denominator = sqlite3_column_int64 (stmt.get(), 2);
                if (numerator <= 0 || numerator > std::numeric_limits<std::uint16_t>::max()
                    || denominator <= 0 || denominator > std::numeric_limits<std::uint16_t>::max())
                    return detail::semanticInvalid ("meter_changes value is outside the Project value range");

                engine::MeterChange meter;
                meter.tick = sqlite3_column_int64 (stmt.get(), 0);
                meter.numerator = static_cast<std::uint16_t> (numerator);
                meter.denominator = static_cast<std::uint16_t> (denominator);
                project.meterMap.push_back (meter);
            }
        }

        {
            detail::Statement stmt;
            if (auto result = stmt.prepare (db_, "SELECT id, tick, name FROM markers ORDER BY tick, id;"); ! result.ok())
                return result;

            while (true)
            {
                const int step = stmt.step();
                if (step == SQLITE_DONE)
                    break;
                if (step != SQLITE_ROW)
                    return detail::sqliteMessage (db_, BundleStatus::SqliteError, sqlite3_errmsg (db_));

                engine::Marker marker;
                if (auto result = detail::columnBlob (stmt.get(), 0, marker.id.bytes, "markers.id"); ! result.ok())
                    return result;
                marker.tick = sqlite3_column_int64 (stmt.get(), 1);

                const unsigned char* const text = sqlite3_column_text (stmt.get(), 2);
                const int textBytes = sqlite3_column_bytes (stmt.get(), 2);
                if (text != nullptr && textBytes > 0)
                    marker.name.assign (reinterpret_cast<const char*> (text), static_cast<std::size_t> (textBytes));

                project.markers.push_back (marker);
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

        bool midiNoteWindowProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM midi_notes n JOIN midi_clips c ON c.id = n.clip_id "
                "WHERE n.start_tick > c.timeline_length OR n.length_ticks > c.timeline_length - n.start_tick LIMIT 1;",
                midiNoteWindowProblem);
            ! result.ok())
            return result;
        if (midiNoteWindowProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "MIDI note window exceeds MIDI Clip length" };

        bool trackReferenceProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM clips c LEFT JOIN tracks t ON t.id = c.track_id WHERE t.id IS NULL "
                "UNION ALL SELECT 1 FROM midi_clips c LEFT JOIN tracks t ON t.id = c.track_id WHERE t.id IS NULL "
                "LIMIT 1;",
                trackReferenceProblem);
            ! result.ok())
            return result;
        if (trackReferenceProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "Clip track reference does not resolve to a Track" };

        bool takeWindowProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM recording_takes rt JOIN assets a ON a.id = rt.asset_id "
                "WHERE rt.frame_count > a.frames LIMIT 1;",
                takeWindowProblem);
            ! result.ok())
            return result;
        if (takeWindowProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "recording Take window exceeds asset frames" };

        bool takeClipProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM recording_takes rt JOIN clips c ON c.id = rt.clip_id "
                "WHERE rt.asset_id != c.asset_id OR rt.track_id != c.track_id "
                "OR rt.timeline_start != c.timeline_start OR rt.frame_count != c.src_len LIMIT 1;",
                takeClipProblem);
            ! result.ok())
            return result;
        if (takeClipProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "recording Take does not match its Clip placement" };

        bool compWindowProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM recording_comp_segments cs JOIN recording_takes rt ON rt.id = cs.take_id "
                "WHERE cs.source_offset > rt.frame_count OR cs.timeline_length > rt.frame_count - cs.source_offset LIMIT 1;",
                compWindowProblem);
            ! result.ok())
            return result;
        if (compWindowProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "recording Comp segment window exceeds Take frames" };

        bool fxUnknownKindProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM fx_inserts WHERE kind NOT IN (0, 1, 2, 3, 4) LIMIT 1;",
                fxUnknownKindProblem);
            ! result.ok())
            return result;
        if (fxUnknownKindProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "FX insert kind is unknown" };

        bool fxOwnerProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM fx_inserts f "
                "LEFT JOIN tracks t ON t.id = f.owner_entity "
                "LEFT JOIN buses b ON b.id = f.owner_entity "
                "WHERE t.id IS NULL AND b.id IS NULL LIMIT 1;",
                fxOwnerProblem);
            ! result.ok())
            return result;
        if (fxOwnerProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "FX insert owner does not resolve to a Track or Bus" };

        bool fxDuplicatePositionProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM ("
                "SELECT owner_entity, position, COUNT(*) AS c FROM fx_inserts "
                "GROUP BY owner_entity, position HAVING c > 1"
                ") LIMIT 1;",
                fxDuplicatePositionProblem);
            ! result.ok())
            return result;
        if (fxDuplicatePositionProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "FX insert positions must be unique per owner" };

        bool fxOrphanParamProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM fx_insert_params p LEFT JOIN fx_inserts f ON f.id = p.insert_id "
                "WHERE f.id IS NULL LIMIT 1;",
                fxOrphanParamProblem);
            ! result.ok())
            return result;
        if (fxOrphanParamProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "FX insert param row is orphaned" };

        bool fxParamRangeProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM fx_insert_params WHERE value < 0 OR value > 1 LIMIT 1;",
                fxParamRangeProblem);
            ! result.ok())
            return result;
        if (fxParamRangeProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "FX insert param value is outside normalized range" };

        bool automationOwnerProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM automation_lanes l LEFT JOIN tracks t ON t.id = l.owner_entity "
                "WHERE l.target_role IN (0, 1, 2) AND t.id IS NULL "
                "UNION ALL SELECT 1 FROM automation_lanes l LEFT JOIN buses b ON b.id = l.owner_entity "
                "WHERE l.target_role IN (3, 4) AND b.id IS NULL "
                "UNION ALL SELECT 1 FROM automation_lanes l LEFT JOIN fx_inserts f ON f.id = l.owner_entity "
                "WHERE l.target_role = 5 AND f.id IS NULL "
                "LIMIT 1;",
                automationOwnerProblem);
            ! result.ok())
            return result;
        if (automationOwnerProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "automation lane owner does not resolve for its target role" };

        bool automationStripParamProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM automation_lanes "
                "WHERE (target_role IN (0, 3) AND param_id != 1) "
                "OR (target_role IN (1, 4) AND param_id != 1) "
                "LIMIT 1;",
                automationStripParamProblem);
            ! result.ok())
            return result;
        if (automationStripParamProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "automation lane param_id is invalid for its strip target" };

        bool automationFxParamProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM automation_lanes l JOIN fx_inserts f ON f.id = l.owner_entity "
                "WHERE l.target_role = 5 AND NOT ("
                "(f.kind = 0 AND l.param_id >= 0 AND l.param_id < 96 AND (l.param_id % 16) IN (0, 1, 2, 3)) "
                "OR (f.kind = 1 AND l.param_id BETWEEN 0 AND 5) "
                "OR (f.kind = 2 AND l.param_id BETWEEN 0 AND 5) "
                "OR (f.kind = 3 AND l.param_id BETWEEN 0 AND 4) "
                "OR (f.kind = 4 AND l.param_id BETWEEN 0 AND 2)) "
                "LIMIT 1;",
                automationFxParamProblem);
            ! result.ok())
            return result;
        if (automationFxParamProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "automation lane param_id is not in the target FX ParamSpec" };

        bool automationDuplicateTargetProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM ("
                "SELECT owner_entity, target_role, param_id, COUNT(*) AS c FROM automation_lanes "
                "GROUP BY owner_entity, target_role, param_id HAVING c > 1"
                ") LIMIT 1;",
                automationDuplicateTargetProblem);
            ! result.ok())
            return result;
        if (automationDuplicateTargetProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "automation lanes must be unique per target" };

        bool automationOrphanPointProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM automation_breakpoints p LEFT JOIN automation_lanes l ON l.id = p.lane_id "
                "WHERE l.id IS NULL LIMIT 1;",
                automationOrphanPointProblem);
            ! result.ok())
            return result;
        if (automationOrphanPointProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "automation breakpoint row is orphaned" };

        bool automationDuplicateTickProblem = false;
        if (auto result = detail::hasAnyRow (
                db_,
                "SELECT 1 FROM ("
                "SELECT lane_id, tick, COUNT(*) AS c FROM automation_breakpoints "
                "GROUP BY lane_id, tick HAVING c > 1"
                ") LIMIT 1;",
                automationDuplicateTickProblem);
            ! result.ok())
            return result;
        if (automationDuplicateTickProblem)
            return BundleResult { BundleStatus::SemanticInvalid, SQLITE_CONSTRAINT, kCodeSchemaVersion, "automation breakpoint ticks must be unique per lane" };

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
            "UNION ALL SELECT value FROM automation_breakpoints "
            "UNION ALL SELECT gain FROM clips "
            "UNION ALL SELECT linear_gain FROM tracks "
            "UNION ALL SELECT pan FROM tracks "
            "UNION ALL SELECT linear_gain FROM buses "
            "UNION ALL SELECT pan FROM buses "
            "UNION ALL SELECT pitch_note FROM midi_notes "
            "UNION ALL SELECT normalized_velocity FROM midi_notes "
            "UNION ALL SELECT value FROM fx_insert_params;");
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
                "UNION ALL SELECT 1 FROM tracks WHERE id = zeroblob(16) OR linear_gain < 0 OR linear_gain > 1000.0 OR pan < -1.0 OR pan > 1.0 OR muted NOT IN (0, 1) OR soloed NOT IN (0, 1) OR solo_safe NOT IN (0, 1) "
                "UNION ALL SELECT 1 FROM buses WHERE id = zeroblob(16) OR linear_gain < 0 OR linear_gain > 1000.0 OR pan < -1.0 OR pan > 1.0 OR muted NOT IN (0, 1) OR soloed NOT IN (0, 1) OR solo_safe NOT IN (0, 1) "
                "UNION ALL SELECT 1 FROM clips WHERE track_id = zeroblob(16) OR timeline_length < 0 OR src_offset < 0 OR src_len < 0 OR gain < 0 OR fade_in < 0 OR fade_out < 0 OR time_base NOT IN (0, 1) "
                "UNION ALL SELECT 1 FROM recording_takes WHERE id = zeroblob(16) OR asset_id = zeroblob(16) OR track_id = zeroblob(16) OR clip_id = zeroblob(16) OR timeline_start < 0 OR frame_count <= 0 OR take_ordinal < 0 OR input_channel < 0 OR device_stable_id < 0 OR monitoring_policy NOT IN (0, 1, 2) "
                "UNION ALL SELECT 1 FROM recording_comp_segments WHERE id = zeroblob(16) OR take_id = zeroblob(16) OR sort_index < 0 OR timeline_start < 0 OR timeline_length <= 0 OR source_offset < 0 "
                "UNION ALL SELECT 1 FROM fx_inserts WHERE id = zeroblob(16) OR owner_entity = zeroblob(16) OR position < 0 OR kind NOT IN (0, 1, 2, 3, 4) OR enabled NOT IN (0, 1) "
                "UNION ALL SELECT 1 FROM fx_insert_params WHERE insert_id = zeroblob(16) OR param_id < 0 OR value < 0 OR value > 1 "
                "UNION ALL SELECT 1 FROM automation_lanes WHERE id = zeroblob(16) OR owner_entity = zeroblob(16) OR target_role NOT IN (0, 1, 2, 3, 4, 5) OR param_id < 0 "
                "UNION ALL SELECT 1 FROM automation_breakpoints WHERE lane_id = zeroblob(16) OR tick < 0 OR value < 0 OR value > 1 OR curve_type NOT IN (0, 1) "
                "UNION ALL SELECT 1 FROM midi_clips WHERE track_id = zeroblob(16) OR timeline_length < 0 OR time_base NOT IN (0, 1) "
                "UNION ALL SELECT 1 FROM midi_notes WHERE start_tick < 0 OR length_ticks < 0 OR key < 0 OR key > 127 OR normalized_velocity < 0 OR normalized_velocity > 1 OR port_index < -1 OR channel < -1 OR channel > 15 "
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
                "UNION ALL SELECT 1 FROM tracks WHERE typeof(id) != 'blob' OR typeof(name) != 'text' OR typeof(linear_gain) NOT IN ('integer', 'real') OR typeof(pan) NOT IN ('integer', 'real') OR typeof(muted) != 'integer' OR typeof(soloed) != 'integer' OR typeof(solo_safe) != 'integer' "
                "UNION ALL SELECT 1 FROM buses WHERE typeof(id) != 'blob' OR typeof(name) != 'text' OR typeof(linear_gain) NOT IN ('integer', 'real') OR typeof(pan) NOT IN ('integer', 'real') OR typeof(muted) != 'integer' OR typeof(soloed) != 'integer' OR typeof(solo_safe) != 'integer' "
                "UNION ALL SELECT 1 FROM clips WHERE typeof(id) != 'blob' OR typeof(asset_id) != 'blob' OR typeof(track_id) != 'blob' OR typeof(timeline_start) != 'integer' OR typeof(timeline_length) != 'integer' OR typeof(src_offset) != 'integer' OR typeof(src_len) != 'integer' OR typeof(gain) NOT IN ('integer', 'real') OR typeof(fade_in) != 'integer' OR typeof(fade_out) != 'integer' OR typeof(time_base) != 'integer' "
                "UNION ALL SELECT 1 FROM recording_takes WHERE typeof(id) != 'blob' OR typeof(asset_id) != 'blob' OR typeof(track_id) != 'blob' OR typeof(clip_id) != 'blob' OR typeof(timeline_start) != 'integer' OR typeof(frame_count) != 'integer' OR typeof(take_ordinal) != 'integer' OR typeof(input_channel) != 'integer' OR typeof(device_stable_id) != 'integer' OR typeof(monitoring_policy) != 'integer' "
                "UNION ALL SELECT 1 FROM recording_comp_segments WHERE typeof(id) != 'blob' OR typeof(take_id) != 'blob' OR typeof(sort_index) != 'integer' OR typeof(timeline_start) != 'integer' OR typeof(timeline_length) != 'integer' OR typeof(source_offset) != 'integer' "
                "UNION ALL SELECT 1 FROM fx_inserts WHERE typeof(id) != 'blob' OR typeof(owner_entity) != 'blob' OR typeof(position) != 'integer' OR typeof(kind) != 'integer' OR typeof(enabled) != 'integer' "
                "UNION ALL SELECT 1 FROM fx_insert_params WHERE typeof(insert_id) != 'blob' OR typeof(param_id) != 'integer' OR typeof(value) NOT IN ('integer', 'real') "
                "UNION ALL SELECT 1 FROM automation_lanes WHERE typeof(id) != 'blob' OR typeof(owner_entity) != 'blob' OR typeof(target_role) != 'integer' OR typeof(param_id) != 'integer' "
                "UNION ALL SELECT 1 FROM automation_breakpoints WHERE typeof(lane_id) != 'blob' OR typeof(tick) != 'integer' OR typeof(value) NOT IN ('integer', 'real') OR typeof(curve_type) != 'integer' "
                "UNION ALL SELECT 1 FROM midi_clips WHERE typeof(id) != 'blob' OR typeof(track_id) != 'blob' OR typeof(timeline_start) != 'integer' OR typeof(timeline_length) != 'integer' OR typeof(time_base) != 'integer' "
                "UNION ALL SELECT 1 FROM midi_notes WHERE typeof(id) != 'blob' OR typeof(clip_id) != 'blob' OR typeof(start_tick) != 'integer' OR typeof(length_ticks) != 'integer' OR typeof(key) != 'integer' OR typeof(pitch_note) NOT IN ('integer', 'real') OR typeof(normalized_velocity) NOT IN ('integer', 'real') OR typeof(port_index) != 'integer' OR typeof(channel) != 'integer' "
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
