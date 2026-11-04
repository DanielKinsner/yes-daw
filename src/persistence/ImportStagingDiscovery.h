#pragma once

#include "persistence/ImportStagingLease.h"

#if defined(_WIN32)
#include <winternl.h>

#include <bit>
#include <limits>
#endif

namespace yesdaw::persistence::prepared_asset_detail {

#if defined(_WIN32)
namespace import_discovery_detail {

struct Handle
{
    HANDLE value = INVALID_HANDLE_VALUE;
    ~Handle() { if (value != INVALID_HANDLE_VALUE && value != nullptr) (void) CloseHandle (value); }
};

inline DWORD openParent (const std::filesystem::path& path, Handle& parent)
{
    parent.value = CreateFileW (path.c_str(), FILE_READ_ATTRIBUTES | FILE_TRAVERSE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (parent.value == INVALID_HANDLE_VALUE)
        return GetLastError();
    FILE_BASIC_INFO basic {};
    FILE_STANDARD_INFO standard {};
    if (! GetFileInformationByHandleEx (parent.value, FileBasicInfo, &basic, sizeof (basic))
        || ! GetFileInformationByHandleEx (parent.value, FileStandardInfo, &standard, sizeof (standard)))
        return GetLastError();
    if (GetFileType (parent.value) != FILE_TYPE_DISK || standard.Directory == FALSE
        || (basic.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0
        || (basic.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        return ERROR_INVALID_PARAMETER;
    return ERROR_SUCCESS;
}

inline bool nameGone (decltype (&::NtCreateFile) open, HANDLE parent, const std::filesystem::path& filename)
{
    const auto& name = filename.native();
    if (name.empty() || name == L"." || name == L".." || name.find_first_of (L"/\\:") != std::wstring::npos
        || name.size() > std::numeric_limits<USHORT>::max() / sizeof (wchar_t))
        return false;
    UNICODE_STRING child {};
    child.Buffer = const_cast<wchar_t*> (name.data());
    child.Length = static_cast<USHORT> (name.size() * sizeof (wchar_t));
    child.MaximumLength = child.Length;
    OBJECT_ATTRIBUTES attributes {};
    attributes.Length = sizeof (attributes);
    attributes.RootDirectory = parent;
    attributes.ObjectName = &child;
    attributes.Attributes = OBJ_CASE_INSENSITIVE;
    IO_STATUS_BLOCK io {};
    Handle item;
    // FILE_OPEN cannot create, truncate or delete. Metadata-only create operations complete synchronously.
    // https://learn.microsoft.com/windows/win32/api/winternl/nf-winternl-ntcreatefile
    const auto status = static_cast<ULONG> (open (&item.value, FILE_READ_ATTRIBUTES, &attributes, &io, nullptr, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN, FILE_OPEN_REPARSE_POINT, nullptr, 0));
    // MS-ERREF: DELETE_PENDING, OBJECT_NAME_NOT_FOUND, OBJECT_PATH_NOT_FOUND, NO_SUCH_FILE.
    return status == 0xC0000056u || status == 0xC0000034u || status == 0xC000003Au || status == 0xC000000Fu;
}

}
#endif

// Only pre-authority discovery calls this. Never use it for producer validation, durable intents or locked cleanup.
inline bool retiredImportDiscoveryEntry (const std::error_code& error, const std::filesystem::path& path,
                                         const std::filesystem::path& audio)
{
#if defined(_WIN32)
    if (error.category() != std::system_category() || error.value() != ERROR_ACCESS_DENIED)
        return false;
    const bool marker = path.filename() == kImportLeaseFilename;
    const auto job = marker ? path.parent_path() : path;
    const auto utf8Name = job.filename().generic_u8string();
    const std::string name (utf8Name.begin(), utf8Name.end());
    if (job.parent_path() != audio || ! name.starts_with (".import-") || ! lowerHexId (std::string_view (name).substr (8)))
        return false;
    const HMODULE module = GetModuleHandleW (L"ntdll.dll");
    const auto symbol = module != nullptr ? GetProcAddress (module, "NtCreateFile") : nullptr;
    const auto open = std::bit_cast<decltype (&::NtCreateFile)> (symbol);
    if (open == nullptr)
        return false;
    import_discovery_detail::Handle parent;
    const auto parentError = import_discovery_detail::openParent (path.parent_path(), parent);
    if (parentError == ERROR_SUCCESS)
        return import_discovery_detail::nameGone (open, parent.value, path.filename());
    if (! marker || (parentError != ERROR_ACCESS_DENIED && parentError != ERROR_FILE_NOT_FOUND
                     && parentError != ERROR_PATH_NOT_FOUND))
        return false;
    import_discovery_detail::Handle audioParent;
    if (import_discovery_detail::openParent (audio, audioParent) != ERROR_SUCCESS)
        return false;
    return import_discovery_detail::nameGone (open, audioParent.value, job.filename());
#else
    (void) error;
    (void) path;
    (void) audio;
    return false;
#endif
}

}
