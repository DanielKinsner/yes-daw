#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

TEST_CASE ("Save As restores the original database when the completed copy cannot reopen",
           "[ui][input][saveas][saveas-safety]")
{
    const auto directory = std::filesystem::temp_directory_path()
        / ("yesdaw-saveas-reopen-" + std::to_string (
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto original = directory / "original.yesdaw";
    const auto destination = directory / "copy.yesdaw";
    yesdaw::ui::UiAppModel model;
    model.setSessionStateDirectory (directory / "session-state");
    REQUIRE (model.createProjectBundle (original).ok());
    REQUIRE (model.addAudioTrack().dispatched);
    REQUIRE (model.hasUnsavedChanges());
    const auto savesBefore = model.context().saveCount;
    const auto dispatchesBefore = model.context().commandDispatchCount;
    std::ofstream (original / "retained-asset.txt") << "retain the copied content";

    bool reachedCopyBoundary = false;
    const auto failed = model.saveProjectBundleAs (destination, [&] (const auto& copied) {
        reachedCopyBoundary = true;
        REQUIRE (copied == std::filesystem::weakly_canonical (destination));
        REQUIRE (std::filesystem::exists (copied / "retained-asset.txt"));
        REQUIRE (std::filesystem::file_size (copied / "project.db") > 0);
        // Preserve the copied bytes but make the real SQLite open fail deterministically.
        std::filesystem::rename (copied / "project.db", copied / "project.db.retained");
        std::filesystem::create_directory (copied / "project.db");
    });

    REQUIRE (reachedCopyBoundary);
    REQUIRE_FALSE (failed.dispatched);
    REQUIRE (std::string (failed.state.disabledReason) == "bundle reopen failed after save-as");
    REQUIRE (model.bundlePath() == original);
    REQUIRE (model.readLastProjectRecord() == original);
    REQUIRE (model.hasUnsavedChanges());
    REQUIRE (model.context().saveCount == savesBefore);
    REQUIRE (model.context().commandDispatchCount == dispatchesBefore);
    REQUIRE (std::filesystem::exists (destination / "project.db.retained"));
    REQUIRE (std::filesystem::exists (destination / "retained-asset.txt"));
    REQUIRE (std::filesystem::is_directory (destination / "project.db"));

    // An active source path alone is insufficient: prove the restored connection accepts a
    // further edit and an explicit Save, and another model reads that saved state from disk.
    REQUIRE (model.addAudioTrack().dispatched);
    REQUIRE (model.saveProjectBundle().ok());
    REQUIRE_FALSE (model.hasUnsavedChanges());
    yesdaw::ui::UiAppModel reopened;
    REQUIRE (reopened.openProjectBundle (original).ok());
    REQUIRE (reopened.project().tracks.size() == 3u);
    REQUIRE (std::filesystem::exists (destination / "project.db.retained"));
}
