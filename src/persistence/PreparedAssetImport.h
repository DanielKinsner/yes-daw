// Filesystem identity and no-overwrite transfer for ADR-0080 import staging. No SQLite or live model access.
#pragma once

#include <array>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#pragma push_macro("near")
#pragma push_macro("far")
#pragma push_macro("small")
#include <windows.h>
#include <io.h>
#pragma pop_macro("small")
#pragma pop_macro("far")
#pragma pop_macro("near")
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <linux/fs.h>
#include <sys/syscall.h>
#elif defined(__APPLE__)
#include <stdio.h>
#include <sys/stdio.h>
#endif
#endif

namespace yesdaw::persistence::prepared_asset_detail {

struct FileFingerprint
{
    std::uint64_t volume = 0;
    std::array<std::uint8_t, 16> identity {};
    std::uint64_t size = 0;
    std::int64_t writeSeconds = 0, changeSeconds = 0;
    std::int64_t writeFraction = 0, changeFraction = 0;
    bool directory = false;

    [[nodiscard]] bool sameIdentity (const FileFingerprint& other) const noexcept
    {
        return volume == other.volume && identity == other.identity && directory == other.directory;
    }
    bool operator== (const FileFingerprint&) const = default;
};

// Do not follow a symlink/reparse point at the captured boundary. Directory stamps are deliberately ignored
// by sameIdentity: creating or removing our own children changes them.
inline std::error_code fingerprint (const std::filesystem::path& path, FileFingerprint& out, int descriptor = -1)
{
#if defined(_WIN32)
    const HANDLE handle = descriptor >= 0 ? reinterpret_cast<HANDLE> (_get_osfhandle (descriptor))
                          : CreateFileW (path.c_str(), FILE_READ_ATTRIBUTES,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                       FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return { static_cast<int> (GetLastError()), std::system_category() };
    FILE_ID_INFO id {};
    FILE_BASIC_INFO basic {};
    FILE_STANDARD_INFO standard {};
    const bool read = GetFileInformationByHandleEx (handle, FileIdInfo, &id, sizeof (id))
                   && GetFileInformationByHandleEx (handle, FileBasicInfo, &basic, sizeof (basic))
                   && GetFileInformationByHandleEx (handle, FileStandardInfo, &standard, sizeof (standard));
    const DWORD error = read ? ERROR_SUCCESS : GetLastError();
    if (descriptor < 0)
        CloseHandle (handle);
    if (! read)
        return { static_cast<int> (error), std::system_category() };
    if ((basic.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        return std::make_error_code (std::errc::too_many_symbolic_link_levels);
    out = {};
    out.volume = id.VolumeSerialNumber;
    for (std::size_t i = 0; i < out.identity.size(); ++i)
        out.identity[i] = id.FileId.Identifier[i];
    out.size = static_cast<std::uint64_t> (standard.EndOfFile.QuadPart);
    out.writeSeconds = basic.LastWriteTime.QuadPart;
    out.changeSeconds = basic.ChangeTime.QuadPart;
    out.directory = standard.Directory != FALSE;
#else
    struct stat info {};
    if ((descriptor >= 0 ? ::fstat (descriptor, &info) : ::lstat (path.c_str(), &info)) != 0)
        return { errno, std::generic_category() };
    if (! S_ISREG (info.st_mode) && ! S_ISDIR (info.st_mode))
        return std::make_error_code (std::errc::invalid_argument);
    out = {};
    out.volume = static_cast<std::uint64_t> (info.st_dev);
    const auto inode = static_cast<std::uint64_t> (info.st_ino);
    for (std::size_t i = 0; i < sizeof (inode); ++i)
        out.identity[i] = static_cast<std::uint8_t> (inode >> (i * 8));
    out.size = static_cast<std::uint64_t> (info.st_size);
    out.directory = S_ISDIR (info.st_mode);
#if defined(__APPLE__)
    out.writeSeconds = info.st_mtimespec.tv_sec;
    out.writeFraction = info.st_mtimespec.tv_nsec;
    out.changeSeconds = info.st_ctimespec.tv_sec;
    out.changeFraction = info.st_ctimespec.tv_nsec;
#else
    out.writeSeconds = info.st_mtim.tv_sec;
    out.writeFraction = info.st_mtim.tv_nsec;
    out.changeSeconds = info.st_ctim.tv_sec;
    out.changeFraction = info.st_ctim.tv_nsec;
#endif
#endif
    return {};
}

// Same-filesystem transfer only. A competing destination always wins without its bytes being changed.
// Unsupported kernels/filesystems refuse rather than copying or falling back to overwrite-capable rename.
inline std::error_code transferNoReplace (const std::filesystem::path& source, const std::filesystem::path& destination)
{
#if defined(_WIN32)
    if (MoveFileExW (source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
        return {};
    return { static_cast<int> (GetLastError()), std::system_category() };
#elif defined(__linux__) && defined(SYS_renameat2)
    if (::syscall (SYS_renameat2, AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(), RENAME_NOREPLACE) == 0)
        return {};
    return { errno, std::generic_category() };
#elif defined(__APPLE__)
    if (::renamex_np (source.c_str(), destination.c_str(), RENAME_EXCL) == 0)
        return {};
    return { errno, std::generic_category() };
#else
    (void) source;
    (void) destination;
    return std::make_error_code (std::errc::operation_not_supported);
#endif
}

} // namespace yesdaw::persistence::prepared_asset_detail
