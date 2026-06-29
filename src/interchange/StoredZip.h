#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace yesdaw::interchange::storedzip {

enum class ZipStatus : std::uint8_t
{
    Ok = 0,
    InvalidArgument,
    FilesystemError,
    UnsupportedFormat,
    MalformedArchive,
    CrcMismatch
};

struct ZipResult
{
    ZipStatus   status = ZipStatus::Ok;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return status == ZipStatus::Ok; }
};

struct Entry
{
    std::string path;
    std::vector<std::uint8_t> bytes;
};

struct ReadResult
{
    ZipStatus          status = ZipStatus::Ok;
    std::string        message;
    std::vector<Entry> entries;

    [[nodiscard]] bool ok() const noexcept { return status == ZipStatus::Ok; }
};

namespace detail {

inline ZipResult invalid (std::string message) { return { ZipStatus::InvalidArgument, std::move (message) }; }
inline ZipResult filesystem (std::string message) { return { ZipStatus::FilesystemError, std::move (message) }; }
inline ZipResult unsupported (std::string message) { return { ZipStatus::UnsupportedFormat, std::move (message) }; }
inline ZipResult malformed (std::string message) { return { ZipStatus::MalformedArchive, std::move (message) }; }
inline ZipResult crcMismatch (std::string message) { return { ZipStatus::CrcMismatch, std::move (message) }; }

inline void appendLe16 (std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back (static_cast<std::uint8_t> (value & 0xffu));
    out.push_back (static_cast<std::uint8_t> ((value >> 8u) & 0xffu));
}

inline void appendLe32 (std::vector<std::uint8_t>& out, std::uint32_t value)
{
    out.push_back (static_cast<std::uint8_t> (value & 0xffu));
    out.push_back (static_cast<std::uint8_t> ((value >> 8u) & 0xffu));
    out.push_back (static_cast<std::uint8_t> ((value >> 16u) & 0xffu));
    out.push_back (static_cast<std::uint8_t> ((value >> 24u) & 0xffu));
}

inline bool readLe16 (std::span<const std::uint8_t> bytes, std::size_t offset, std::uint16_t& out) noexcept
{
    if (offset > bytes.size() || bytes.size() - offset < 2u)
        return false;

    out = static_cast<std::uint16_t> (bytes[offset])
        | static_cast<std::uint16_t> (static_cast<std::uint16_t> (bytes[offset + 1u]) << 8u);
    return true;
}

inline bool readLe32 (std::span<const std::uint8_t> bytes, std::size_t offset, std::uint32_t& out) noexcept
{
    if (offset > bytes.size() || bytes.size() - offset < 4u)
        return false;

    out = static_cast<std::uint32_t> (bytes[offset])
        | (static_cast<std::uint32_t> (bytes[offset + 1u]) << 8u)
        | (static_cast<std::uint32_t> (bytes[offset + 2u]) << 16u)
        | (static_cast<std::uint32_t> (bytes[offset + 3u]) << 24u);
    return true;
}

inline std::uint32_t crc32 (std::span<const std::uint8_t> bytes) noexcept
{
    std::uint32_t crc = 0xffffffffu;
    for (const std::uint8_t byte : bytes)
    {
        crc ^= static_cast<std::uint32_t> (byte);
        for (int bit = 0; bit < 8; ++bit)
        {
            if ((crc & 1u) != 0u)
                crc = (crc >> 1u) ^ 0xedb88320u;
            else
                crc >>= 1u;
        }
    }

    return crc ^ 0xffffffffu;
}

inline bool pathIsValid (std::string_view path) noexcept
{
    if (path.empty() || path.front() == '/' || path.find ('\\') != std::string_view::npos)
        return false;

    return path.find ("..") == std::string_view::npos;
}

inline bool addSize (std::size_t a, std::size_t b, std::size_t& out) noexcept
{
    if (b > std::numeric_limits<std::size_t>::max() - a)
        return false;

    out = a + b;
    return true;
}

struct CentralEntry
{
    std::string   path;
    std::uint32_t crc = 0;
    std::uint32_t compressedSize = 0;
    std::uint32_t uncompressedSize = 0;
    std::uint32_t localHeaderOffset = 0;
};

} // namespace detail

[[nodiscard]] inline ZipResult writeStoredZip (const std::filesystem::path& path, std::span<const Entry> entries)
{
    if (entries.empty())
        return detail::invalid ("ZIP must contain at least one entry");

    std::vector<detail::CentralEntry> centralEntries;
    centralEntries.reserve (entries.size());

    std::vector<std::uint8_t> out;
    for (const Entry& entry : entries)
    {
        if (! detail::pathIsValid (entry.path))
            return detail::invalid ("ZIP entry path is invalid");
        if (entry.path.size() > std::numeric_limits<std::uint16_t>::max())
            return detail::invalid ("ZIP entry path is too long");
        if (entry.bytes.size() > std::numeric_limits<std::uint32_t>::max())
            return detail::invalid ("ZIP entry is too large for ZIP32");
        if (out.size() > std::numeric_limits<std::uint32_t>::max())
            return detail::invalid ("ZIP archive is too large for ZIP32");

        for (const detail::CentralEntry& earlier : centralEntries)
            if (earlier.path == entry.path)
                return detail::invalid ("ZIP entry path is duplicated");

        const std::uint32_t crc = detail::crc32 (std::span<const std::uint8_t> (entry.bytes.data(), entry.bytes.size()));
        const std::uint32_t size = static_cast<std::uint32_t> (entry.bytes.size());
        const std::uint32_t localHeaderOffset = static_cast<std::uint32_t> (out.size());

        detail::appendLe32 (out, 0x04034b50u);
        detail::appendLe16 (out, 20u);
        detail::appendLe16 (out, 0u);
        detail::appendLe16 (out, 0u);
        detail::appendLe16 (out, 0u);
        detail::appendLe16 (out, 0u);
        detail::appendLe32 (out, crc);
        detail::appendLe32 (out, size);
        detail::appendLe32 (out, size);
        detail::appendLe16 (out, static_cast<std::uint16_t> (entry.path.size()));
        detail::appendLe16 (out, 0u);
        out.insert (out.end(), entry.path.begin(), entry.path.end());
        out.insert (out.end(), entry.bytes.begin(), entry.bytes.end());

        centralEntries.push_back ({ entry.path, crc, size, size, localHeaderOffset });
    }

    if (out.size() > std::numeric_limits<std::uint32_t>::max())
        return detail::invalid ("ZIP archive is too large for ZIP32");

    const std::uint32_t centralDirectoryOffset = static_cast<std::uint32_t> (out.size());
    for (const detail::CentralEntry& entry : centralEntries)
    {
        detail::appendLe32 (out, 0x02014b50u);
        detail::appendLe16 (out, 20u);
        detail::appendLe16 (out, 20u);
        detail::appendLe16 (out, 0u);
        detail::appendLe16 (out, 0u);
        detail::appendLe16 (out, 0u);
        detail::appendLe16 (out, 0u);
        detail::appendLe32 (out, entry.crc);
        detail::appendLe32 (out, entry.compressedSize);
        detail::appendLe32 (out, entry.uncompressedSize);
        detail::appendLe16 (out, static_cast<std::uint16_t> (entry.path.size()));
        detail::appendLe16 (out, 0u);
        detail::appendLe16 (out, 0u);
        detail::appendLe16 (out, 0u);
        detail::appendLe16 (out, 0u);
        detail::appendLe32 (out, 0u);
        detail::appendLe32 (out, entry.localHeaderOffset);
        out.insert (out.end(), entry.path.begin(), entry.path.end());
    }

    if (out.size() > std::numeric_limits<std::uint32_t>::max())
        return detail::invalid ("ZIP archive is too large for ZIP32");

    const std::uint32_t centralDirectorySize = static_cast<std::uint32_t> (out.size() - centralDirectoryOffset);
    if (centralEntries.size() > std::numeric_limits<std::uint16_t>::max())
        return detail::invalid ("ZIP entry count is too large for ZIP32");

    detail::appendLe32 (out, 0x06054b50u);
    detail::appendLe16 (out, 0u);
    detail::appendLe16 (out, 0u);
    detail::appendLe16 (out, static_cast<std::uint16_t> (centralEntries.size()));
    detail::appendLe16 (out, static_cast<std::uint16_t> (centralEntries.size()));
    detail::appendLe32 (out, centralDirectorySize);
    detail::appendLe32 (out, centralDirectoryOffset);
    detail::appendLe16 (out, 0u);

    std::error_code ec;
    if (path.has_parent_path())
    {
        std::filesystem::create_directories (path.parent_path(), ec);
        if (ec)
            return detail::filesystem (ec.message());
    }

    std::ofstream file (path, std::ios::binary | std::ios::trunc);
    if (! file)
        return detail::filesystem ("failed to open ZIP for writing");
    if (! out.empty())
        file.write (reinterpret_cast<const char*> (out.data()), static_cast<std::streamsize> (out.size()));
    file.close();
    if (! file)
        return detail::filesystem ("failed to flush ZIP");

    return {};
}

[[nodiscard]] inline const Entry* findEntry (std::span<const Entry> entries, std::string_view path) noexcept
{
    for (const Entry& entry : entries)
        if (entry.path == path)
            return &entry;

    return nullptr;
}

[[nodiscard]] inline ReadResult readStoredZip (const std::filesystem::path& path)
{
    std::ifstream file (path, std::ios::binary);
    if (! file)
        return { ZipStatus::FilesystemError, "failed to open ZIP for reading", {} };

    file.seekg (0, std::ios::end);
    const std::streamoff fileSize = file.tellg();
    file.seekg (0, std::ios::beg);
    if (fileSize < 0)
        return { ZipStatus::FilesystemError, "failed to size ZIP", {} };

    std::vector<std::uint8_t> bytes (static_cast<std::size_t> (fileSize));
    if (! bytes.empty())
        file.read (reinterpret_cast<char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    if (! file && ! bytes.empty())
        return { ZipStatus::FilesystemError, "failed to read ZIP", {} };

    if (bytes.size() < 22u)
        return { ZipStatus::MalformedArchive, "ZIP is too small", {} };

    const std::size_t maxCommentSearch = std::min<std::size_t> (bytes.size() - 22u, 0xffffu);
    std::size_t eocd = std::numeric_limits<std::size_t>::max();
    for (std::size_t back = 0; back <= maxCommentSearch; ++back)
    {
        const std::size_t pos = bytes.size() - 22u - back;
        std::uint32_t signature = 0;
        if (detail::readLe32 (bytes, pos, signature) && signature == 0x06054b50u)
        {
            eocd = pos;
            break;
        }
    }

    if (eocd == std::numeric_limits<std::size_t>::max())
        return { ZipStatus::MalformedArchive, "ZIP end-of-central-directory not found", {} };

    std::uint16_t disk = 0;
    std::uint16_t centralDisk = 0;
    std::uint16_t diskEntries = 0;
    std::uint16_t totalEntries = 0;
    std::uint16_t commentLength = 0;
    std::uint32_t centralSize = 0;
    std::uint32_t centralOffset = 0;
    if (! detail::readLe16 (bytes, eocd + 4u, disk)
        || ! detail::readLe16 (bytes, eocd + 6u, centralDisk)
        || ! detail::readLe16 (bytes, eocd + 8u, diskEntries)
        || ! detail::readLe16 (bytes, eocd + 10u, totalEntries)
        || ! detail::readLe32 (bytes, eocd + 12u, centralSize)
        || ! detail::readLe32 (bytes, eocd + 16u, centralOffset)
        || ! detail::readLe16 (bytes, eocd + 20u, commentLength))
    {
        return { ZipStatus::MalformedArchive, "ZIP end-of-central-directory is truncated", {} };
    }

    if (disk != 0u || centralDisk != 0u || diskEntries != totalEntries)
        return { ZipStatus::UnsupportedFormat, "multi-disk ZIP is unsupported", {} };
    if (eocd + 22u + commentLength != bytes.size())
        return { ZipStatus::MalformedArchive, "ZIP comment length is inconsistent", {} };
    if (centralOffset > bytes.size() || centralSize > bytes.size() - centralOffset)
        return { ZipStatus::MalformedArchive, "ZIP central directory is outside the file", {} };

    std::vector<detail::CentralEntry> centralEntries;
    centralEntries.reserve (totalEntries);
    std::size_t pos = centralOffset;
    for (std::uint16_t i = 0; i < totalEntries; ++i)
    {
        std::uint32_t signature = 0;
        if (! detail::readLe32 (bytes, pos, signature) || signature != 0x02014b50u)
            return { ZipStatus::MalformedArchive, "ZIP central directory entry is malformed", {} };

        std::uint16_t method = 0;
        std::uint16_t nameLength = 0;
        std::uint16_t extraLength = 0;
        std::uint16_t commentLen = 0;
        std::uint32_t crc = 0;
        std::uint32_t compressedSize = 0;
        std::uint32_t uncompressedSize = 0;
        std::uint32_t localHeaderOffset = 0;
        if (! detail::readLe16 (bytes, pos + 10u, method)
            || ! detail::readLe32 (bytes, pos + 16u, crc)
            || ! detail::readLe32 (bytes, pos + 20u, compressedSize)
            || ! detail::readLe32 (bytes, pos + 24u, uncompressedSize)
            || ! detail::readLe16 (bytes, pos + 28u, nameLength)
            || ! detail::readLe16 (bytes, pos + 30u, extraLength)
            || ! detail::readLe16 (bytes, pos + 32u, commentLen)
            || ! detail::readLe32 (bytes, pos + 42u, localHeaderOffset))
        {
            return { ZipStatus::MalformedArchive, "ZIP central directory entry is truncated", {} };
        }

        if (method != 0u)
            return { ZipStatus::UnsupportedFormat, "compressed ZIP entries are unsupported", {} };
        if (compressedSize != uncompressedSize)
            return { ZipStatus::MalformedArchive, "stored ZIP entry has mismatched sizes", {} };

        std::size_t nameStart = 0;
        std::size_t entryEnd = 0;
        if (! detail::addSize (pos, 46u, nameStart)
            || ! detail::addSize (nameStart, nameLength, entryEnd)
            || ! detail::addSize (entryEnd, extraLength, entryEnd)
            || ! detail::addSize (entryEnd, commentLen, entryEnd)
            || entryEnd > bytes.size())
        {
            return { ZipStatus::MalformedArchive, "ZIP central directory entry exceeds file", {} };
        }

        std::string entryPath (reinterpret_cast<const char*> (bytes.data() + nameStart), nameLength);
        if (! detail::pathIsValid (entryPath))
            return { ZipStatus::MalformedArchive, "ZIP central directory path is invalid", {} };
        for (const detail::CentralEntry& earlier : centralEntries)
            if (earlier.path == entryPath)
                return { ZipStatus::MalformedArchive, "ZIP contains duplicate paths", {} };

        centralEntries.push_back ({ std::move (entryPath), crc, compressedSize, uncompressedSize, localHeaderOffset });
        pos = entryEnd;
    }

    if (pos != static_cast<std::size_t> (centralOffset) + static_cast<std::size_t> (centralSize))
        return { ZipStatus::MalformedArchive, "ZIP central directory size is inconsistent", {} };

    std::vector<Entry> entries;
    entries.reserve (centralEntries.size());
    for (const detail::CentralEntry& central : centralEntries)
    {
        const std::size_t local = central.localHeaderOffset;
        std::uint32_t signature = 0;
        if (! detail::readLe32 (bytes, local, signature) || signature != 0x04034b50u)
            return { ZipStatus::MalformedArchive, "ZIP local file header is malformed", {} };

        std::uint16_t method = 0;
        std::uint16_t nameLength = 0;
        std::uint16_t extraLength = 0;
        if (! detail::readLe16 (bytes, local + 8u, method)
            || ! detail::readLe16 (bytes, local + 26u, nameLength)
            || ! detail::readLe16 (bytes, local + 28u, extraLength))
        {
            return { ZipStatus::MalformedArchive, "ZIP local file header is truncated", {} };
        }
        if (method != 0u)
            return { ZipStatus::UnsupportedFormat, "compressed ZIP local entry is unsupported", {} };

        std::size_t dataStart = 0;
        std::size_t dataEnd = 0;
        if (! detail::addSize (local, 30u, dataStart)
            || ! detail::addSize (dataStart, nameLength, dataStart)
            || ! detail::addSize (dataStart, extraLength, dataStart)
            || ! detail::addSize (dataStart, central.compressedSize, dataEnd)
            || dataEnd > bytes.size())
        {
            return { ZipStatus::MalformedArchive, "ZIP local entry exceeds file", {} };
        }

        if (dataStart < central.path.size() || local + 30u + nameLength > bytes.size())
            return { ZipStatus::MalformedArchive, "ZIP local entry path is malformed", {} };

        const std::string localPath (reinterpret_cast<const char*> (bytes.data() + local + 30u), nameLength);
        if (localPath != central.path)
            return { ZipStatus::MalformedArchive, "ZIP local and central paths disagree", {} };

        Entry entry;
        entry.path = central.path;
        entry.bytes.assign (bytes.begin() + static_cast<std::ptrdiff_t> (dataStart),
                            bytes.begin() + static_cast<std::ptrdiff_t> (dataEnd));
        const std::uint32_t actualCrc = detail::crc32 (std::span<const std::uint8_t> (entry.bytes.data(), entry.bytes.size()));
        if (actualCrc != central.crc)
            return { ZipStatus::CrcMismatch, "ZIP entry CRC does not match", {} };

        entries.push_back (std::move (entry));
    }

    return { ZipStatus::Ok, {}, std::move (entries) };
}

} // namespace yesdaw::interchange::storedzip
