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

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

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

// Pick the first slot that opens cleanly through the NORMAL bundle validators (integrity, foreign key,
// semantic, asset reconciliation). Prefer the live slot, fall back to the previous one. last.tmp is a
// scratch slot that may hold a partially written snapshot, so it is never trusted here.
[[nodiscard]] inline std::optional<std::filesystem::path> pickLiveSnapshot (const std::filesystem::path& bundlePath)
{
    for (const std::filesystem::path& candidate : { autosaveSnapshotPath (bundlePath),
                                                    previousSnapshotPath (bundlePath) })
    {
        std::error_code ec;
        if (! std::filesystem::exists (candidate, ec) || ec)
            continue;

        ProjectBundleDb probe;
        if (ProjectBundleDb::openExistingBundle (candidate, probe).ok())
            return candidate;
    }

    return std::nullopt;
}

[[nodiscard]] inline AutosaveResult copyProjectAssets (const std::filesystem::path& sourceBundlePath,
                                                       const engine::Project& project,
                                                       const std::filesystem::path& targetBundlePath)
{
    for (const engine::Asset& asset : project.assets)
    {
        const std::string relative = detail::assetRelativePathForHash (asset.contentHash);
        const std::filesystem::path sourcePath = sourceBundlePath / relative;
        const std::filesystem::path targetPath = targetBundlePath / relative;

        std::error_code ec;
        std::filesystem::create_directories (targetPath.parent_path(), ec);
        if (ec)
            return filesystemError ("create autosave asset directory failed", targetPath.parent_path(), ec);

        std::filesystem::copy_file (sourcePath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec)
            return filesystemError ("copy autosave asset failed", sourcePath, ec);

        // Force the copied bytes out so a crash after publish cannot leave a referenced asset truncated.
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

    {
        ProjectBundleDb snapshot;
        if (auto result = ProjectBundleDb::openOrCreateBundle (tempPath, snapshot); ! result.ok())
            return autosave_detail::bundleError (std::move (result));
        if (auto result = snapshot.writeProjectSnapshot (project); ! result.ok())
            return autosave_detail::bundleError (std::move (result));
    }

    // The snapshot connection is closed (WAL checkpointed into project.db); force the DB file out before
    // we publish so the autosave survives a power loss, not just a clean shutdown.
    if (auto result = detail::flushFileToDisk (tempPath / "project.db"); ! result.ok())
        return autosave_detail::flushError (std::move (result));

    if (auto result = autosave_detail::copyProjectAssets (sourceDb.bundlePath(), project, tempPath); ! result.ok())
        return result;

    {
        ProjectBundleDb validated;
        if (auto result = ProjectBundleDb::openExistingBundle (tempPath, validated); ! result.ok())
            return autosave_detail::bundleError (std::move (result));
    }

    return autosave_detail::replaceSnapshotDirectory (tempPath, finalPath);
}

[[nodiscard]] inline AutosaveResult readAutosaveSnapshot (const std::filesystem::path& bundlePath,
                                                          engine::Project& out)
{
    const std::optional<std::filesystem::path> snapshotPath = autosave_detail::pickLiveSnapshot (bundlePath);
    if (! snapshotPath)
    {
        if (autosave_detail::anySnapshotSlotExists (bundlePath))
            return autosave_detail::bundleError (
                BundleResult { BundleStatus::IntegrityFailed, SQLITE_CORRUPT, 0, "autosave snapshot failed validation" });

        return AutosaveResult {
            AutosaveStatus::NoAutosave,
            BundleResult { BundleStatus::FilesystemError, SQLITE_NOTFOUND, 0, "no autosave snapshot" },
            "no autosave snapshot",
        };
    }

    ProjectBundleDb snapshot;
    if (auto result = ProjectBundleDb::openExistingBundle (*snapshotPath, snapshot); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    if (auto result = snapshot.readProjectSnapshot (out); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    return autosave_detail::ok();
}

[[nodiscard]] inline AutosaveResult restoreAutosaveSnapshot (ProjectBundleDb& targetDb,
                                                             engine::Project& out)
{
    const std::optional<std::filesystem::path> snapshotPath =
        autosave_detail::pickLiveSnapshot (targetDb.bundlePath());
    if (! snapshotPath)
        return autosave_detail::bundleError (
            BundleResult { BundleStatus::FilesystemError, SQLITE_NOTFOUND, 0, "no autosave snapshot to restore" });

    ProjectBundleDb snapshot;
    if (auto result = ProjectBundleDb::openExistingBundle (*snapshotPath, snapshot); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    engine::Project recovered;
    if (auto result = snapshot.readProjectSnapshot (recovered); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    if (auto result = autosave_detail::copyProjectAssets (snapshot.bundlePath(), recovered, targetDb.bundlePath()); ! result.ok())
        return result;

    if (auto result = targetDb.writeProjectSnapshot (recovered); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    out = std::move (recovered);
    return autosave_detail::ok();
}

[[nodiscard]] inline AutosaveResult discardAutosaveSnapshot (const std::filesystem::path& bundlePath)
{
    return autosave_detail::removeTreeIfExists (autosaveDirectory (bundlePath));
}

} // namespace yesdaw::persistence
