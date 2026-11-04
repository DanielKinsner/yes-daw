// Pure child process: leave a real staged snapshot behind without running any destructors.
#include "persistence/ProjectBundle.h"

#include <cstdlib>
#include <filesystem>

#if defined(_WIN32)
int wmain (int argc, wchar_t** argv)
#else
int main (int argc, char** argv)
#endif
{
    if (argc != 3 && argc != 4)
        return 1;
    using namespace yesdaw::persistence;
    ProjectBundleDb db;
    if (! ProjectBundleDb::openExistingBundle (std::filesystem::path (argv[1]), db).ok())
        return 2;
    AssetImportStaging staging;
    if (! AssetImportStaging::create (std::filesystem::path (argv[1]),
                                      yesdaw::engine::EntityId::fromBigEndianParts (0, 200), staging).ok())
        return 3;
    StagedAssetImport snapshot;
    if (! staging.stageSource (std::filesystem::path (argv[2]), 0, snapshot).ok())
        return 4;
    if (argc == 4)
    {
        if (std::filesystem::path (argv[3]) != "retire-marker")
            return 5;
        const auto privatePath = snapshot.snapshotPath();
        PreparedAssetImport prepared;
        if (! std::move (snapshot).complete (yesdaw::engine::EntityId::fromBigEndianParts (0, 200), 1,
                                             yesdaw::engine::SampleRate { 48000.0 }, 1, prepared).ok())
            return 6;
        const auto root = std::filesystem::canonical (std::filesystem::path (argv[1]));
        const auto canonical = storedAssetPathForHash (root, prepared.asset().contentHash);
        const PendingFsOp intent { PendingFsOpKind::StageAsset, privatePath.lexically_relative (root).generic_string(),
                                   detail::assetRelativePathForHash (prepared.asset().contentHash), prepared.asset().contentHash };
        sqlite3_int64 row = 0;
        if (! db.executeSql ("PRAGMA synchronous=FULL; BEGIN IMMEDIATE;").ok()
            || ! db.recordPendingFsOp (intent, row).ok() || ! db.executeSql ("COMMIT;").ok())
            return 7;
        if (prepared_asset_detail::transferNoReplace (privatePath, canonical)
            || ! detail::flushFileToDisk (canonical).ok()
            || ! detail::flushDirectoryToDisk (canonical.parent_path()).ok()
            || ! detail::flushDirectoryToDisk (privatePath.parent_path()).ok())
            return 8;
        std::error_code error;
        if (! std::filesystem::remove (privatePath.parent_path() / ".lease", error) || error
            || ! detail::flushDirectoryToDisk (privatePath.parent_path()).ok())
            return 9;
        // Exact retirement interval: transferred bytes and durable intent remain; the empty job has no marker.
    }
    std::_Exit (0);
}
