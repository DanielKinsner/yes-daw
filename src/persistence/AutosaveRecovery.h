// YES DAW - H6 autosave recovery helper.
//
// Autosaves are bundle-shaped snapshots under autosave/, so recovery goes through the same ProjectBundle
// integrity and semantic validators as normal project open.

#pragma once

#include "persistence/ProjectBundle.h"

#include <cstdint>
#include <filesystem>
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

[[nodiscard]] inline std::filesystem::path autosaveSnapshotPath (const std::filesystem::path& bundlePath)
{
    return bundlePath / "autosave" / "last.yesdaw";
}

namespace autosave_detail {

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

[[nodiscard]] inline AutosaveResult removeTreeIfExists (const std::filesystem::path& path)
{
    std::error_code ec;
    if (! std::filesystem::exists (path, ec))
        return ec ? filesystemError ("stat failed", path, ec) : ok();

    std::filesystem::remove_all (path, ec);
    return ec ? filesystemError ("remove failed", path, ec) : ok();
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
    }

    return ok();
}

[[nodiscard]] inline AutosaveResult replaceSnapshotDirectory (const std::filesystem::path& tempPath,
                                                              const std::filesystem::path& finalPath)
{
    const std::filesystem::path backupPath = finalPath.parent_path() / "last.previous";

    if (auto result = removeTreeIfExists (backupPath); ! result.ok())
        return result;

    std::error_code ec;
    if (std::filesystem::exists (finalPath, ec))
    {
        std::filesystem::rename (finalPath, backupPath, ec);
        if (ec)
            return filesystemError ("move old autosave aside failed", finalPath, ec);
    }
    else if (ec)
    {
        return filesystemError ("stat autosave failed", finalPath, ec);
    }

    std::filesystem::rename (tempPath, finalPath, ec);
    if (ec)
    {
        std::error_code restoreEc;
        if (std::filesystem::exists (backupPath, restoreEc))
            std::filesystem::rename (backupPath, finalPath, restoreEc);

        return filesystemError ("publish autosave failed", tempPath, ec);
    }

    if (auto result = removeTreeIfExists (backupPath); ! result.ok())
        return result;

    return ok();
}

} // namespace autosave_detail

[[nodiscard]] inline AutosaveResult writeAutosaveSnapshot (const ProjectBundleDb& sourceDb,
                                                           const engine::Project& project)
{
    const std::filesystem::path finalPath = autosaveSnapshotPath (sourceDb.bundlePath());
    const std::filesystem::path tempPath = finalPath.parent_path() / "last.tmp";

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
    const std::filesystem::path snapshotPath = autosaveSnapshotPath (bundlePath);

    std::error_code ec;
    if (! std::filesystem::exists (snapshotPath, ec))
    {
        if (ec)
            return autosave_detail::filesystemError ("stat autosave failed", snapshotPath, ec);

        return AutosaveResult {
            AutosaveStatus::NoAutosave,
            BundleResult { BundleStatus::FilesystemError, SQLITE_NOTFOUND, 0, "no autosave snapshot" },
            "no autosave snapshot",
        };
    }

    ProjectBundleDb snapshot;
    if (auto result = ProjectBundleDb::openExistingBundle (snapshotPath, snapshot); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    if (auto result = snapshot.readProjectSnapshot (out); ! result.ok())
        return autosave_detail::bundleError (std::move (result));

    return autosave_detail::ok();
}

[[nodiscard]] inline AutosaveResult restoreAutosaveSnapshot (ProjectBundleDb& targetDb,
                                                             engine::Project& out)
{
    const std::filesystem::path snapshotPath = autosaveSnapshotPath (targetDb.bundlePath());

    ProjectBundleDb snapshot;
    if (auto result = ProjectBundleDb::openExistingBundle (snapshotPath, snapshot); ! result.ok())
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

} // namespace yesdaw::persistence
