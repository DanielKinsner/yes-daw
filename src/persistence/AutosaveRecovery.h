// YES DAW - H6 autosave recovery helper.
//
// Autosaves are bundle-shaped snapshots under autosave/, so recovery goes through the same ProjectBundle
// integrity and semantic validators as normal project open.
//
// Durability contract (ADR-0019 "recover to the last autosave with no corruption"): a publish keeps the
// previous good snapshot on disk under last.previous until the new one is fully written and fsync'd, and
// recovery falls back to last.previous when last.yesdaw is missing. So a crash anywhere in the two-rename
// publish window still leaves at least one valid snapshot reachable — never zero.

#pragma once

#include "persistence/ProjectBundle.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Do not introduce Windows' legacy near/far macros into later engine headers (PanNode uses far).
#pragma push_macro("near")
#pragma push_macro("far")
#include <windows.h>
#pragma pop_macro("far")
#pragma pop_macro("near")
#else
#include <sys/stat.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/acl.h>
#include <sys/mount.h>
#endif
#endif

namespace yesdaw::persistence {

enum class AutosaveStatus : std::uint8_t
{
    Ok = 0,
    NoAutosave,
    FilesystemError,
    BundleError
};

struct AutosaveResult
{
    AutosaveStatus status = AutosaveStatus::Ok;
    BundleResult bundle;
    std::string message;
    std::size_t linked = 0;
    std::size_t copied = 0;

    [[nodiscard]] bool ok() const noexcept { return status == AutosaveStatus::Ok; }
};

[[nodiscard]] inline std::filesystem::path autosaveDirectory (const std::filesystem::path& bundlePath)
{
    return bundlePath / "autosave";
}

// The live snapshot slot. Recovery prefers this; if it is missing (e.g. a crash landed between the two
// publish renames) recovery falls back to the previous slot.
[[nodiscard]] inline std::filesystem::path autosaveSnapshotPath (const std::filesystem::path& bundlePath)
{
    return autosaveDirectory (bundlePath) / "last.yesdaw";
}

namespace autosave_detail {

struct AssetCarry
{
    engine::AssetContentHash hash;
    bool linked = false;
};

#if defined(YESDAW_PERSISTENCE_TEST_HOOKS)
inline thread_local std::optional<engine::AssetContentHash> failLinkForTest;
inline thread_local std::uint64_t assetBytesCopiedForTest = 0;
inline thread_local void (*afterCarryForTest) (const std::filesystem::path&) = nullptr;
inline thread_local void (*beforeRecoveryCleanupForTest) (const std::filesystem::path&,
                                                          const std::filesystem::path&) = nullptr;
#endif

// Private access keeps the write-time shortcut out of normal opens and recovery reads.
struct SnapshotValidator
{
    [[nodiscard]] static BundleResult validate (const std::filesystem::path& snapshotPath,
                                                const std::filesystem::path& sourcePath,
                                                const std::vector<AssetCarry>& carries)
    {
        ProjectBundleDb snapshot;
        if (auto result = ProjectBundleDb::openDatabaseConnection (snapshotPath, false, snapshot); ! result.ok())
            return result;
        std::vector<ProjectBundleDb::StoredAssetFile> assets;
        if (auto result = snapshot.loadStoredAssetFiles (assets); ! result.ok())
            return result;
        if (assets.size() != carries.size())
            return detail::semanticInvalid ("autosave asset carries do not match its rows");
        std::map<engine::AssetContentHash::StorageBytes, bool> linkedByHash;
        for (const auto& carry : carries)
            linkedByHash.emplace (carry.hash.bytes, carry.linked);
        for (const auto& asset : assets)
        {
            const auto carry = linkedByHash.find (asset.hash.bytes);
            if (carry == linkedByHash.end())
                return detail::semanticInvalid ("autosave asset has no carry record");
            const auto target = snapshotPath / asset.relativePath;
            if (carry->second)
            {
                std::error_code error;
                if (! std::filesystem::equivalent (sourcePath / asset.relativePath, target, error) || error)
                    return BundleResult { BundleStatus::IntegrityFailed, SQLITE_CORRUPT, kCodeSchemaVersion,
                                          "autosave asset link identity differs: " + detail::utf8Path (target) };
            }
            else if (auto result = snapshot.verifyAssetFile (asset.hash, target); ! result.ok())
                return result;
        }
        return detail::ok();
    }
};

[[nodiscard]] inline std::filesystem::path tempSnapshotPath (const std::filesystem::path& bundlePath)
{
    return autosaveDirectory (bundlePath) / "last.tmp";
}

[[nodiscard]] inline std::filesystem::path previousSnapshotPath (const std::filesystem::path& bundlePath)
{
    return autosaveDirectory (bundlePath) / "last.previous";
}

[[nodiscard]] inline AutosaveResult ok()
{
    return AutosaveResult { AutosaveStatus::Ok, detail::ok(), {} };
}

[[nodiscard]] inline AutosaveResult filesystemError (std::string action,
                                                     const std::filesystem::path& path,
                                                     const std::error_code& ec)
{
    return AutosaveResult {
        AutosaveStatus::FilesystemError,
        BundleResult { BundleStatus::FilesystemError, SQLITE_OK, 0, ec.message() },
        std::move (action) + ": " + detail::utf8Path (path) + ": " + ec.message(),
    };
}

[[nodiscard]] inline AutosaveResult bundleError (BundleResult result)
{
    return AutosaveResult { AutosaveStatus::BundleError, std::move (result), {} };
}

[[nodiscard]] inline AutosaveResult flushError (BundleResult result)
{
    return AutosaveResult { AutosaveStatus::FilesystemError, std::move (result), {} };
}

[[nodiscard]] inline AutosaveResult removeTreeIfExists (const std::filesystem::path& path)
{
    std::error_code ec;
    if (! std::filesystem::exists (path, ec))
        return ec ? filesystemError ("stat failed", path, ec) : ok();

    std::filesystem::remove_all (path, ec);
    return ec ? filesystemError ("remove failed", path, ec) : ok();
}

// True if a published snapshot file exists in either slot (used to distinguish "no autosave yet" from
// "an autosave exists but failed validation").
[[nodiscard]] inline bool anySnapshotSlotExists (const std::filesystem::path& bundlePath)
{
    std::error_code ec;
    return std::filesystem::exists (autosaveSnapshotPath (bundlePath), ec)
        || std::filesystem::exists (previousSnapshotPath (bundlePath), ec);
}

struct RecoverySelection
{
    std::filesystem::path path;
    engine::Project project;
    std::int64_t writeStamp = 0;
};

// Own only a directory this invocation created. Normal returns report cleanup errors; the destructor
// also removes the copy if an allocation/filesystem exception unwinds the read.
struct RecoveryScratch
{
    std::filesystem::path path;
    bool cleanupFailed = false;

    ~RecoveryScratch()
    {
        std::error_code ec;
        if (! path.empty())
            std::filesystem::remove_all (path, ec);
    }

    [[nodiscard]] AutosaveResult create (const std::filesystem::path& root,
                                         const std::filesystem::path& protectedBundle,
                                         const std::filesystem::path& protectedSource)
    {
        std::error_code ec;
        const auto resolvedRoot = std::filesystem::canonical (root, ec);
        if (ec)
            return filesystemError ("resolve recovery scratch directory failed", root, ec);
        // OS temp can be redirected into a project. Refuse before creating anything, resolving
        // aliases and comparing directory identity so case-insensitive filesystems are covered.
        for (const auto& protectedPath : { protectedBundle, protectedSource })
        {
            const auto resolvedProtected = std::filesystem::canonical (protectedPath, ec);
            if (ec)
                return filesystemError ("resolve protected recovery directory failed", protectedPath, ec);
            for (auto ancestor = resolvedRoot; ! ancestor.empty();)
            {
                const bool contained = std::filesystem::equivalent (ancestor, resolvedProtected, ec);
                if (ec)
                    return filesystemError ("inspect recovery scratch ancestry failed", ancestor, ec);
                if (contained)
                    return filesystemError ("recovery scratch directory is inside the project or snapshot", root,
                                            std::make_error_code (std::errc::invalid_argument));
                const auto parent = ancestor.parent_path();
                if (parent == ancestor)
                    break;
                ancestor = parent;
            }
        }
#if defined(__APPLE__) && ! defined(_WIN32)
        // On a noowners mount even an owner-only mode can appear owned by every caller.
        struct statfs mountInfo {};
        if (::statfs (resolvedRoot.c_str(), &mountInfo) != 0)
            return filesystemError ("inspect recovery scratch mount ownership failed", resolvedRoot,
                                    { errno, std::generic_category() });
        if ((mountInfo.f_flags & MNT_IGNORE_OWNERSHIP) != 0)
            return filesystemError ("recovery scratch mount ignores ownership", resolvedRoot,
                                    std::make_error_code (std::errc::operation_not_supported));
#endif
#if defined(_WIN32)
        const auto windowsError = [&] (const char* action, DWORD error) {
            return filesystemError (action, resolvedRoot, { static_cast<int> (error), std::system_category() });
        };
        // CreateDirectory silently ignores a security descriptor on a filesystem without ACLs.
        // Refuse before creating/copying there; a private name alone does not protect its contents.
        const HANDLE volume = CreateFileW (resolvedRoot.c_str(), FILE_READ_ATTRIBUTES,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (volume == INVALID_HANDLE_VALUE)
            return windowsError ("open recovery scratch volume failed", GetLastError());
        DWORD filesystemFlags = 0;
        const BOOL inspected = GetVolumeInformationByHandleW (volume, nullptr, 0, nullptr, nullptr, &filesystemFlags, nullptr, 0);
        const DWORD inspectionError = inspected ? ERROR_SUCCESS : GetLastError();
        const BOOL volumeClosed = CloseHandle (volume);
        const DWORD volumeCloseError = volumeClosed ? ERROR_SUCCESS : GetLastError();
        if (! inspected)
            return windowsError ("inspect recovery scratch volume security failed", inspectionError);
        if (! volumeClosed)
            return windowsError ("close recovery scratch volume failed", volumeCloseError);
        if ((filesystemFlags & FILE_PERSISTENT_ACLS) == 0)
            return filesystemError ("recovery scratch filesystem cannot protect metadata", resolvedRoot,
                                    std::make_error_code (std::errc::operation_not_supported));

        HANDLE token = nullptr;
        if (! OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &token))
            return windowsError ("open recovery process token failed", GetLastError());
        alignas(TOKEN_USER) std::array<std::uint8_t, sizeof (TOKEN_USER) + SECURITY_MAX_SID_SIZE> userStorage {};
        DWORD userSize = 0;
        const BOOL gotUser = GetTokenInformation (token, TokenUser, userStorage.data(),
                                                 static_cast<DWORD> (userStorage.size()), &userSize);
        const DWORD userError = gotUser ? ERROR_SUCCESS : GetLastError();
        const BOOL tokenClosed = CloseHandle (token);
        const DWORD tokenCloseError = tokenClosed ? ERROR_SUCCESS : GetLastError();
        if (! gotUser)
            return windowsError ("read recovery process user failed", userError);
        if (! tokenClosed)
            return windowsError ("close recovery process token failed", tokenCloseError);

        alignas(DWORD) std::array<std::uint8_t, sizeof (ACL) + sizeof (ACCESS_ALLOWED_ACE) + SECURITY_MAX_SID_SIZE> aclStorage {};
        const auto acl = reinterpret_cast<ACL*> (aclStorage.data());
        SECURITY_DESCRIPTOR descriptor {};
        if (! InitializeAcl (acl, static_cast<DWORD> (aclStorage.size()), ACL_REVISION)
            || ! AddAccessAllowedAceEx (acl, ACL_REVISION, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE, FILE_ALL_ACCESS,
                                        reinterpret_cast<const TOKEN_USER*> (userStorage.data())->User.Sid)
            || ! InitializeSecurityDescriptor (&descriptor, SECURITY_DESCRIPTOR_REVISION)
            || ! SetSecurityDescriptorDacl (&descriptor, TRUE, acl, FALSE)
            || ! SetSecurityDescriptorControl (&descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
            return windowsError ("prepare private recovery scratch permissions failed", GetLastError());
        SECURITY_ATTRIBUTES attributes { static_cast<DWORD> (sizeof (SECURITY_ATTRIBUTES)), &descriptor, FALSE };
#endif
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int n = 0; n < 1000; ++n)
        {
            const auto candidate = resolvedRoot / ("yesdaw-recovery-" + std::to_string (ticks) + "-" + std::to_string (n));
#if defined(_WIN32)
            const bool created = CreateDirectoryW (candidate.c_str(), &attributes) != FALSE;
            if (! created)
                ec = { static_cast<int> (GetLastError()), std::system_category() };
#else
            // Set owner-only mode at creation; macOS inherited ACLs are removed below before copying.
            const bool created = ::mkdir (candidate.c_str(), S_IRWXU) == 0;
            if (! created)
                ec = { errno, std::generic_category() };
#endif
            if (created)
            {
                path = candidate;
#if ! defined(_WIN32)
                // Some filesystems report mkdir success while substituting their mount's mode.
                // Own and remove the empty directory on refusal, before any metadata is copied.
                struct stat createdInfo {};
                if (::lstat (path.c_str(), &createdInfo) != 0)
                    return finish (filesystemError ("inspect private recovery scratch directory failed", path,
                                                    { errno, std::generic_category() }));
                if (! S_ISDIR (createdInfo.st_mode) || createdInfo.st_uid != ::geteuid()
                    || (createdInfo.st_mode & (S_IRWXG | S_IRWXO)) != 0)
                    return finish (filesystemError ("recovery scratch filesystem did not enforce private ownership and permissions",
                                                    path, std::make_error_code (std::errc::permission_denied)));
#if defined(__APPLE__)
                // Darwin ACL grants can bypass mode 0700. Change only our new empty directory,
                // then verify the boundary before any project metadata or SQLite sidecar exists.
                const acl_t emptyAcl = ::acl_init (0);
                if (emptyAcl == nullptr)
                    return finish (filesystemError ("prepare private recovery scratch ACL failed", path,
                                                    { errno, std::generic_category() }));
                const int setResult = ::acl_set_file (path.c_str(), ACL_TYPE_EXTENDED, emptyAcl);
                const int setError = errno;
                (void) ::acl_free (emptyAcl);
                if (setResult != 0)
                    return finish (filesystemError ("clear inherited recovery scratch ACL failed", path,
                                                    { setError, std::generic_category() }));
                const acl_t actualAcl = ::acl_get_file (path.c_str(), ACL_TYPE_EXTENDED);
                if (actualAcl == nullptr)
                {
                    // Darwin also returns ENOENT when an existing object has no ACL property.
                    if (errno != ENOENT)
                        return finish (filesystemError ("inspect private recovery scratch ACL failed", path,
                                                        { errno, std::generic_category() }));
                }
                else
                {
                    acl_entry_t entry = nullptr;
                    const int entryResult = ::acl_get_entry (actualAcl, ACL_FIRST_ENTRY, &entry);
                    const int entryError = errno;
                    (void) ::acl_free (actualAcl);
                    // Darwin returns -1/EINVAL for the first entry of an empty ACL.
                    if (entryResult != -1 || entryError != EINVAL)
                        return finish (filesystemError ("recovery scratch ACL is not private", path,
                                                        std::make_error_code (std::errc::permission_denied)));
                }
#endif
#endif
                return ok();
            }
            if (ec == std::errc::file_exists)
            {
                std::error_code statusError;
                if (std::filesystem::is_directory (candidate, statusError) && ! statusError)
                    continue; // Exclusively own a newly created name; never reuse an existing directory.
                if (statusError)
                    ec = statusError;
            }
            return filesystemError ("create recovery scratch directory failed", candidate, ec);
        }
        return filesystemError ("create recovery scratch directory failed", root,
                                std::make_error_code (std::errc::file_exists));
    }

    [[nodiscard]] AutosaveResult finish (AutosaveResult result)
    {
        std::error_code ec;
        std::filesystem::remove_all (path, ec);
        if (ec)
        {
            cleanupFailed = true;
            auto cleanup = filesystemError ("remove recovery scratch directory failed", path, ec);
            if (result.ok())
                result = std::move (cleanup);
            else
            {
                if (result.message.empty())
                    result.message = result.bundle.message;
                result.message += "; " + cleanup.message;
                result.bundle.message += "; " + cleanup.message;
            }
        }
        else
            path.clear(); // Retain ownership on failure so the destructor can retry.
        return result;
    }
};

struct RecoveryReader
{
    [[nodiscard]] static AutosaveResult read (const std::filesystem::path& bundlePath,
                                             const std::filesystem::path& source, RecoverySelection& out,
                                             bool& cleanupFailed)
    {
        cleanupFailed = false;
        // Published slots are stable under the existing control-thread publication lifecycle.
        // A raw DB/WAL copy is not an atomic snapshot of an arbitrary concurrent external writer.
        // Never open the source in SQLite: even READONLY may change its SHM or checkpoint on close.
        std::error_code ec;
        const auto journal = source / "project.db-journal";
        const auto journalStatus = std::filesystem::symlink_status (journal, ec);
        if (ec && ec != std::errc::no_such_file_or_directory)
            return filesystemError ("stat recovery rollback journal failed", journal, ec);
        if (std::filesystem::exists (journalStatus))
            return bundleError ({ BundleStatus::IntegrityFailed, SQLITE_CORRUPT, 0,
                                  "recovery refuses a rollback journal: " + detail::utf8Path (journal) });

        RecoveryScratch scratch;
        const auto scratchRoot = std::filesystem::temp_directory_path (ec);
        if (ec)
            return filesystemError ("locate recovery scratch directory failed", scratchRoot, ec);
        if (auto result = scratch.create (scratchRoot, bundlePath, source); ! result.ok())
        {
            cleanupFailed = scratch.cleanupFailed;
            return result;
        }
        RecoverySelection selected { source, {}, 0 };
        auto result = [&]() -> AutosaveResult {
            for (const char* name : { "project.db", "project.db-wal" })
            {
                const auto original = source / name;
                const auto status = std::filesystem::symlink_status (original, ec);
                if (ec && ec != std::errc::no_such_file_or_directory)
                    return filesystemError ("stat recovery metadata failed", original, ec);
                if (std::string_view (name) == "project.db-wal" && ! std::filesystem::exists (status))
                    continue;
                const auto copy = scratch.path / name;
                std::filesystem::copy_file (original, copy, ec);
                if (ec)
                    return filesystemError ("copy recovery metadata failed", original, ec);
                // copy_file preserves read-only attributes. Only our copy may become writable.
                std::filesystem::permissions (copy, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
                                               std::filesystem::perm_options::add, ec);
                if (ec)
                    return filesystemError ("make recovery metadata copy writable failed", copy, ec);
            }
            // SHM has no persistent data; SQLite reconstructs it beside the owned DB/WAL copy.
            ProjectBundleDb metadata;
            if (auto opened = ProjectBundleDb::openDatabaseConnection (scratch.path, false, metadata); ! opened.ok())
                return bundleError (std::move (opened));
            if (auto read = metadata.readProjectSnapshot (selected.project); ! read.ok())
                return bundleError (std::move (read));
            if (auto stamp = metadata.projectWriteStamp (selected.writeStamp); ! stamp.ok())
                return bundleError (std::move (stamp));
            std::vector<ProjectBundleDb::StoredAssetFile> assets;
            if (auto loaded = metadata.loadStoredAssetFiles (assets); ! loaded.ok())
                return bundleError (std::move (loaded));
            for (const auto& asset : assets)
                if (auto verified = metadata.verifyAssetFile (asset.hash, source / asset.relativePath); ! verified.ok())
                    return bundleError (std::move (verified));
            return ok();
        }(); // metadata closes before scratch removal, including on every refusal.
#if defined(YESDAW_PERSISTENCE_TEST_HOOKS)
        if (beforeRecoveryCleanupForTest != nullptr)
            beforeRecoveryCleanupForTest (source, scratch.path);
#endif
        result = scratch.finish (std::move (result));
        cleanupFailed = scratch.cleanupFailed;
        if (result.ok())
            out = std::move (selected);
        return result;
    }
};

// Prefer live, then previous, preserving concrete errors if neither validates. last.tmp is never a
// candidate. Reuse this validated selection only within one invocation, never across a recovery prompt.
[[nodiscard]] inline AutosaveResult pickLiveSnapshot (const std::filesystem::path& bundlePath, RecoverySelection& out)
{
    std::optional<AutosaveResult> failure;
    for (const auto& candidate : { autosaveSnapshotPath (bundlePath), previousSnapshotPath (bundlePath) })
    {
        std::error_code ec;
        const auto status = std::filesystem::symlink_status (candidate, ec);
        if (ec == std::errc::no_such_file_or_directory || (! ec && ! std::filesystem::exists (status)))
            continue;
        bool cleanupFailed = false;
        auto result = ec ? filesystemError ("stat autosave slot failed", candidate, ec)
                         : RecoveryReader::read (bundlePath, candidate, out, cleanupFailed);
        // A cleanup error says nothing about source validity. Do not hide a newer validated
        // snapshot behind an older success: the caller might then retire both as up to date.
        if (result.ok())
            return result;
        const auto reason = detail::utf8Path (candidate) + ": "
                          + (result.message.empty() ? result.bundle.message : result.message);
        if (! failure)
        {
            result.message = reason;
            result.bundle.message = reason;
            failure = std::move (result);
        }
        else
        {
            failure->message += "; " + reason;
            failure->bundle.message += "; " + reason;
        }
        if (cleanupFailed)
            return std::move (*failure);
    }
    if (failure)
        return std::move (*failure);
    return AutosaveResult { AutosaveStatus::NoAutosave,
                            { BundleStatus::FilesystemError, SQLITE_NOTFOUND, 0, "no autosave snapshot" },
                            "no autosave snapshot" };
}

[[nodiscard]] inline AutosaveResult carryProjectAssets (const std::filesystem::path& sourceBundlePath,
                                                       const engine::Project& project,
                                                       const std::filesystem::path& targetBundlePath,
                                                       std::vector<AssetCarry>& carries)
{
    std::map<engine::AssetContentHash::StorageBytes, bool> linkedByHash;
    for (const auto& carry : carries)
        linkedByHash.emplace (carry.hash.bytes, carry.linked);
    for (const engine::Asset& asset : project.assets)
    {
        const auto existing = linkedByHash.find (asset.contentHash.bytes);
        if (existing != linkedByHash.end())
        {
            carries.push_back ({ asset.contentHash, existing->second });
            continue;
        }
        const std::string relative = detail::assetRelativePathForHash (asset.contentHash);
        const std::filesystem::path sourcePath = sourceBundlePath / relative;
        const std::filesystem::path targetPath = targetBundlePath / relative;

        std::error_code ec;
        std::filesystem::create_directories (targetPath.parent_path(), ec);
        if (ec)
            return filesystemError ("create autosave asset directory failed", targetPath.parent_path(), ec);

#if defined(YESDAW_PERSISTENCE_TEST_HOOKS)
        if (failLinkForTest && *failLinkForTest == asset.contentHash)
            ec = std::make_error_code (std::errc::operation_not_supported);
        else
#endif
            std::filesystem::create_hard_link (sourcePath, targetPath, ec);
        const bool linked = ! ec;
        if (! linked)
        {
            // The scratch slot is fresh: never overwrite an inode that might have another link.
            std::filesystem::copy_file (sourcePath, targetPath, ec);
            if (ec)
                return filesystemError ("copy autosave asset failed", sourcePath, ec);
#if defined(YESDAW_PERSISTENCE_TEST_HOOKS)
            assetBytesCopiedForTest += std::filesystem::file_size (sourcePath);
#endif
        }
        carries.push_back ({ asset.contentHash, linked });
        linkedByHash.emplace (asset.contentHash.bytes, linked);

        // Flush each carried name, including hard links (directory flush is a no-op on Windows).
        if (auto result = detail::flushFileToDisk (targetPath); ! result.ok())
            return flushError (std::move (result));
    }

    return ok();
}

// Crash-safe publish: keep the previous good snapshot under last.previous until the new one is durable,
// and never delete the only reachable copy. Combined with pickLiveSnapshot's last.yesdaw -> last.previous
// fallback, a kill anywhere in this window leaves at least one valid snapshot on disk.
[[nodiscard]] inline AutosaveResult replaceSnapshotDirectory (const std::filesystem::path& tempPath,
                                                              const std::filesystem::path& finalPath)
{
    const std::filesystem::path backupPath = finalPath.parent_path() / "last.previous";

    std::error_code ec;
    const bool finalExists = std::filesystem::exists (finalPath, ec);
    if (ec)
        return filesystemError ("stat autosave failed", finalPath, ec);

    if (finalExists)
    {
        // finalPath is the current good copy, so it is safe to retire any stale backup and move the
        // current snapshot aside. (Directory rename does not overwrite on Windows, so clear the slot.)
        if (auto result = removeTreeIfExists (backupPath); ! result.ok())
            return result;

        std::filesystem::rename (finalPath, backupPath, ec);
        if (ec)
            return filesystemError ("move old autosave aside failed", finalPath, ec);

        if (auto result = detail::flushDirectoryToDisk (finalPath.parent_path()); ! result.ok())
            return flushError (std::move (result));
    }
    // else: finalPath is absent (first publish, or a prior publish was interrupted). If last.previous is
    // present it is the only good copy and must be left untouched until the new one is published.

    std::filesystem::rename (tempPath, finalPath, ec);
    if (ec)
    {
        // Publish failed. Restore the live slot from the backup if we have one; either way a valid copy
        // remains reachable (last.yesdaw if the restore succeeds, otherwise last.previous via fallback).
        std::error_code restoreEc;
        if (std::filesystem::exists (backupPath, restoreEc))
        {
            std::filesystem::rename (backupPath, finalPath, restoreEc);
            if (restoreEc)
                return filesystemError ("publish autosave failed and rollback to last.previous failed",
                                        backupPath, restoreEc);
        }
        return filesystemError ("publish autosave failed (recoverable from last.previous)", tempPath, ec);
    }

    if (auto result = detail::flushDirectoryToDisk (finalPath.parent_path()); ! result.ok())
        return flushError (std::move (result));

    // The new snapshot is now durable at finalPath; retire the backup.
    if (auto result = removeTreeIfExists (backupPath); ! result.ok())
        return result;

    if (auto result = detail::flushDirectoryToDisk (finalPath.parent_path()); ! result.ok())
        return flushError (std::move (result));

    return ok();
}

} // namespace autosave_detail

[[nodiscard]] inline AutosaveResult writeAutosaveSnapshot (const ProjectBundleDb& sourceDb,
                                                           const engine::Project& project)
{
    const std::filesystem::path finalPath = autosaveSnapshotPath (sourceDb.bundlePath());
    const std::filesystem::path tempPath = autosave_detail::tempSnapshotPath (sourceDb.bundlePath());

    std::error_code ec;
    std::filesystem::create_directories (finalPath.parent_path(), ec);
    if (ec)
        return autosave_detail::filesystemError ("create autosave directory failed", finalPath.parent_path(), ec);

    if (auto result = autosave_detail::removeTreeIfExists (tempPath); ! result.ok())
        return result;

    // ADR-0068 §5: the snapshot carries the source's write stamp, written in the snapshot's own transaction (and so in
    // the project.db flushed below), so an open can tell whether the bundle already holds everything it holds.
    std::int64_t sourceStamp = 0;
    if (auto result = sourceDb.projectWriteStamp (sourceStamp); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    {
        ProjectBundleDb snapshot;
        if (auto result = ProjectBundleDb::openOrCreateBundle (tempPath, snapshot); ! result.ok())
            return autosave_detail::bundleError (std::move (result));
        if (auto result = snapshot.writeProjectSnapshot (project, sourceStamp); ! result.ok())
            return autosave_detail::bundleError (std::move (result));
    }

    // The snapshot connection is closed (WAL checkpointed into project.db); force the DB file out before
    // we publish so the autosave survives a power loss, not just a clean shutdown.
    if (auto result = detail::flushFileToDisk (tempPath / "project.db"); ! result.ok())
        return autosave_detail::flushError (std::move (result));

    std::vector<autosave_detail::AssetCarry> carries;
    if (auto result = autosave_detail::carryProjectAssets (sourceDb.bundlePath(), project, tempPath, carries); ! result.ok())
        return result;
#if defined(YESDAW_PERSISTENCE_TEST_HOOKS)
    if (autosave_detail::afterCarryForTest != nullptr)
        autosave_detail::afterCarryForTest (tempPath);
#endif
    if (auto result = autosave_detail::SnapshotValidator::validate (tempPath, sourceDb.bundlePath(), carries); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    auto result = autosave_detail::replaceSnapshotDirectory (tempPath, finalPath);
    if (result.ok())
        for (const auto& carry : carries)
            carry.linked ? ++result.linked : ++result.copied;
    return result;
}

// `writeStamp` (optional): the snapshot's write stamp - its source bundle's when it was written (ADR-0068 §5; a snapshot
// from before schema v35 is migrated only in metadata scratch and reads 0).
[[nodiscard]] inline AutosaveResult readAutosaveSnapshot (const std::filesystem::path& bundlePath,
                                                          engine::Project& out,
                                                          std::int64_t* writeStamp = nullptr)
{
    autosave_detail::RecoverySelection selected;
    if (auto result = autosave_detail::pickLiveSnapshot (bundlePath, selected); ! result.ok())
        return result;
    out = std::move (selected.project);
    if (writeStamp != nullptr)
        *writeStamp = selected.writeStamp;
    return autosave_detail::ok();
}

[[nodiscard]] inline AutosaveResult restoreAutosaveSnapshot (ProjectBundleDb& targetDb,
                                                             engine::Project& out)
{
    autosave_detail::RecoverySelection selected;
    if (auto result = autosave_detail::pickLiveSnapshot (targetDb.bundlePath(), selected); ! result.ok())
        return result;
    auto& recovered = selected.project;

    for (const auto& asset : recovered.assets)
    {
        const auto source = storedAssetPathForHash (selected.path, asset.contentHash);
        const auto target = storedAssetPathForHash (targetDb.bundlePath(), asset.contentHash);
        std::error_code error;
        if (std::filesystem::equivalent (source, target, error) && ! error)
            continue;
        // A target may itself be linked by an older snapshot. Replace its NAME, never overwrite shared bytes.
        if (auto result = ProjectBundleDb::adoptAssetFile (targetDb.bundlePath(), asset.contentHash, source); ! result.ok())
            return autosave_detail::bundleError (std::move (result));
    }

    // ADR-0068 §5: Restore answers the question - the marker clears in the transaction that writes the recovered rows.
    if (auto result = targetDb.writeProjectSnapshot (recovered, std::nullopt, UnresolvedSnapshotStamp::Clear); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    out = std::move (recovered);
    return autosave_detail::ok();
}

[[nodiscard]] inline AutosaveResult discardAutosaveSnapshot (const std::filesystem::path& bundlePath)
{
    return autosave_detail::removeTreeIfExists (autosaveDirectory (bundlePath));
}

} // namespace yesdaw::persistence
