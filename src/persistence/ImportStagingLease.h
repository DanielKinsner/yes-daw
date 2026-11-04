// ADR-0081: operation-specific, disposable cleanup receipts. Never an import-resume or Project record.
#pragma once

#include "persistence/PreparedAssetImport.h"

#include <algorithm>
#include <charconv>
#include <fcntl.h>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#if ! defined(_WIN32)
#include <sys/file.h>
#endif

namespace yesdaw::persistence::prepared_asset_detail {

inline constexpr std::string_view kImportLeaseFilename = ".lease";

inline bool lowerHexId (std::string_view value)
{
    return value.size() == 32 && std::all_of (value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

inline std::string identityHex (const std::array<std::uint8_t, 16>& bytes)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (const auto byte : bytes)
    {
        text += digits[byte >> 4];
        text += digits[byte & 15];
    }
    return text;
}

inline bool parseIdentity (std::string_view volume, std::string_view identity, bool directory, FileFingerprint& out)
{
    if (! lowerHexId (identity))
        return false;
    out = {};
    const auto parsed = std::from_chars (volume.data(), volume.data() + volume.size(), out.volume);
    if (parsed.ec != std::errc {} || parsed.ptr != volume.data() + volume.size())
        return false;
    const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
    for (std::size_t i = 0; i < out.identity.size(); ++i)
        out.identity[i] = static_cast<std::uint8_t> (nibble (identity[i * 2]) * 16 + nibble (identity[i * 2 + 1]));
    out.directory = directory;
    return true;
}

struct ImportLeaseHeader
{
    std::string jobId;
    FileFingerprint root, audio, job, marker;
};

inline std::string serializeHeader (const ImportLeaseHeader& header)
{
    std::string text = "YESDAW_IMPORT_STAGING 1 " + header.jobId;
    for (const auto* identity : { &header.root, &header.audio, &header.job, &header.marker })
        text += " " + std::to_string (identity->volume) + " " + identityHex (identity->identity);
    return text + "\n";
}

inline bool parseHeader (std::string_view line, ImportLeaseHeader& out)
{
    std::istringstream input { std::string (line) };
    std::string magic, version, volume, identity, extra;
    if (! (input >> magic >> version >> out.jobId) || magic != "YESDAW_IMPORT_STAGING" || version != "1"
        || ! lowerHexId (out.jobId))
        return false;
    for (auto* target : { &out.root, &out.audio, &out.job, &out.marker })
        if (! (input >> volume >> identity) || ! parseIdentity (volume, identity, target != &out.marker, *target))
            return false;
    return ! (input >> extra);
}

inline bool privateReceiptName (std::string_view name)
{
    if (name.empty() || name.find_first_of ("/\\:\0", 0, 4) != std::string_view::npos)
        return false;
    // This protocol writes only item-<decimal>[.<original extension>]. Enforcing that exact stem also
    // excludes every case, trailing-dot/space or short-name alias of the lease marker on Windows.
    const auto stem = name.substr (0, name.find ('.'));
    return stem.starts_with ("item-") && stem.size() > 5
        && std::all_of (stem.begin() + 5, stem.end(), [](char c) { return c >= '0' && c <= '9'; });
}

struct ImportItemReceipt
{
    std::filesystem::path name;
    FileFingerprint identity;
};

inline std::string serializeReceipt (const std::filesystem::path& name, const FileFingerprint& identity)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string encoded;
    for (const auto byte : name.generic_u8string())
    {
        encoded += digits[static_cast<std::uint8_t> (byte) >> 4];
        encoded += digits[static_cast<std::uint8_t> (byte) & 15];
    }
    return "ITEM " + encoded + " " + std::to_string (identity.volume) + " " + identityHex (identity.identity) + "\n";
}

inline bool parseReceipt (std::string_view line, ImportItemReceipt& out)
{
    std::istringstream input { std::string (line) };
    std::string magic, encoded, volume, identity, extra;
    if (! (input >> magic >> encoded >> volume >> identity) || (input >> extra) || magic != "ITEM"
        || encoded.empty() || encoded.size() % 2 != 0 || ! parseIdentity (volume, identity, false, out.identity))
        return false;
    const auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return -1;
    };
    std::u8string name;
    for (std::size_t i = 0; i < encoded.size(); i += 2)
    {
        const int high = nibble (encoded[i]), low = nibble (encoded[i + 1]);
        if (high < 0 || low < 0)
            return false;
        name += static_cast<char8_t> (high * 16 + low);
    }
    if (! privateReceiptName ({ reinterpret_cast<const char*> (name.data()), name.size() }))
        return false;
    try { out.name = std::filesystem::path (name); }
    catch (const std::system_error&) { return false; }
    return out.name == out.name.filename() && ! out.name.has_root_name();
}

// A separate open description is required even in-process: flock and LockFileEx both then exclude live owners.
class ImportStagingLease final
{
public:
    ImportStagingLease() = default;
    ImportStagingLease (const ImportStagingLease&) = delete;
    ImportStagingLease& operator= (const ImportStagingLease&) = delete;
    ~ImportStagingLease() { close(); }

    [[nodiscard]] std::error_code open (const std::filesystem::path& marker, bool create, bool& busy)
    {
        busy = false;
#if defined(_WIN32)
        const HANDLE handle = CreateFileW (marker.c_str(), GENERIC_READ | GENERIC_WRITE,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                           create ? CREATE_NEW : OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            return { static_cast<int> (GetLastError()), std::system_category() };
        descriptor_ = _open_osfhandle (reinterpret_cast<intptr_t> (handle), _O_RDWR | _O_BINARY | _O_NOINHERIT);
        if (descriptor_ < 0)
        {
            const int error = errno;
            CloseHandle (handle);
            return { error, std::generic_category() };
        }
#else
        descriptor_ = ::open (marker.c_str(), O_RDWR | O_NOFOLLOW | O_CLOEXEC | (create ? O_CREAT | O_EXCL : 0), 0600);
        if (descriptor_ < 0)
            return { errno, std::generic_category() };
#endif
        if (const auto error = fingerprint (marker, identity_, descriptor_); error || identity_.directory)
        {
            close();
            return error ? error : std::make_error_code (std::errc::invalid_argument);
        }
#if defined(_WIN32)
        OVERLAPPED range {};
        if (! LockFileEx (handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &range))
        {
            const DWORD error = GetLastError();
            busy = error == ERROR_LOCK_VIOLATION || error == ERROR_IO_PENDING;
            close();
            return { static_cast<int> (error), std::system_category() };
        }
#else
        if (::flock (descriptor_, LOCK_EX | LOCK_NB) != 0)
        {
            const int error = errno;
            busy = error == EWOULDBLOCK || error == EAGAIN;
            close();
            return { error, std::generic_category() };
        }
#endif
        return {};
    }

    void close() noexcept
    {
        if (descriptor_ < 0)
            return;
#if defined(_WIN32)
        _close (std::exchange (descriptor_, -1));
#else
        ::close (std::exchange (descriptor_, -1));
#endif
    }

    [[nodiscard]] const FileFingerprint& identity() const noexcept { return identity_; }

    [[nodiscard]] std::error_code write (std::string_view bytes)
    {
#if defined(_WIN32)
        if (_lseeki64 (descriptor_, 0, SEEK_END) < 0)
#else
        if (::lseek (descriptor_, 0, SEEK_END) < 0)
#endif
            return { errno, std::generic_category() };
        while (! bytes.empty())
        {
#if defined(_WIN32)
            const auto count = _write (descriptor_, bytes.data(), static_cast<unsigned int> (bytes.size()));
#else
            const auto count = ::write (descriptor_, bytes.data(), bytes.size());
#endif
            if (count <= 0)
                return { errno, std::generic_category() };
            bytes.remove_prefix (static_cast<std::size_t> (count));
        }
#if defined(_WIN32)
        if (_commit (descriptor_) != 0)
#else
        if (::fsync (descriptor_) != 0)
#endif
            return { errno, std::generic_category() };
        return {};
    }

    [[nodiscard]] std::error_code rewind()
    {
        readPosition_ = readSize_ = 0;
#if defined(_WIN32)
        if (_lseeki64 (descriptor_, 0, SEEK_SET) < 0)
#else
        if (::lseek (descriptor_, 0, SEEK_SET) < 0)
#endif
            return { errno, std::generic_category() };
        return {};
    }

    [[nodiscard]] std::error_code readLine (std::string& out, bool& complete)
    {
        out.clear();
        complete = false;
        while (true)
        {
            if (readPosition_ == readSize_)
            {
#if defined(_WIN32)
                const auto count = _read (descriptor_, readBuffer_.data(), static_cast<unsigned int> (readBuffer_.size()));
#else
                const auto count = ::read (descriptor_, readBuffer_.data(), readBuffer_.size());
#endif
                if (count < 0)
                    return { errno, std::generic_category() };
                if (count == 0)
                    return {};
                readPosition_ = 0;
                readSize_ = static_cast<std::size_t> (count);
            }
            const char c = readBuffer_[readPosition_++];
            if (c == '\n')
            {
                complete = true;
                return {};
            }
            if (out.size() == 4096)
                return std::make_error_code (std::errc::invalid_argument);
            out += c;
        }
    }

    [[nodiscard]] std::error_code readHeader (ImportLeaseHeader& header, bool& recognized)
    {
        recognized = false;
        if (const auto error = rewind())
            return error;
        std::string line;
        bool complete = false;
        if (const auto error = readLine (line, complete))
            return error;
        recognized = complete && parseHeader (line, header);
        return {};
    }

    [[nodiscard]] std::error_code validateMarker (const std::filesystem::path& path) const
    {
        FileFingerprint current, handle;
        if (const auto error = fingerprint (path, handle, descriptor_))
            return error;
        if (const auto error = fingerprint (path, current))
            return error;
        if (! current.sameIdentity (identity_) || ! handle.sameIdentity (identity_))
            return std::make_error_code (std::errc::state_not_recoverable);
        return {};
    }

private:
    int descriptor_ = -1;
    FileFingerprint identity_;
    std::array<char, 4096> readBuffer_ {};
    std::size_t readPosition_ = 0, readSize_ = 0;
};

} // namespace yesdaw::persistence::prepared_asset_detail
