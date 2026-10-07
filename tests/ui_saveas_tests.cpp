#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

// ADR-0060: a copy that cannot be validated leaves no target and no temporary folder; the original stays current
// (its database handle never closed) and keeps working.
TEST_CASE ("Save As that cannot validate its copy leaves no target and keeps the original current",
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
    std::filesystem::path partial;
    const auto failed = model.saveProjectBundleAs (destination, [&] (const auto& copied) {
        reachedCopyBoundary = true;
        partial = copied;
        REQUIRE (copied.extension() == ".partial");
        REQUIRE (std::filesystem::exists (copied / "retained-asset.txt"));
        REQUIRE (std::filesystem::file_size (copied / "project.db") > 0);
        // Keep the copied bytes but make the real SQLite open fail deterministically.
        std::filesystem::rename (copied / "project.db", copied / "project.db.retained");
        std::filesystem::create_directory (copied / "project.db");
    });

    INFO (failed.state.disabledReason);
    REQUIRE (reachedCopyBoundary);
    REQUIRE_FALSE (failed.dispatched);
    REQUIRE (std::string (failed.state.disabledReason) == "the copy did not validate");
    REQUIRE_FALSE (std::filesystem::exists (destination));
    REQUIRE_FALSE (std::filesystem::exists (partial));
    REQUIRE (model.bundlePath() == original);
    REQUIRE (model.readLastProjectRecord() == original);
    REQUIRE (model.hasUnsavedChanges());
    REQUIRE (model.context().saveCount == savesBefore);
    REQUIRE (model.context().commandDispatchCount == dispatchesBefore);

    // An active source path alone is insufficient: prove the connection accepts a further edit and an explicit
    // Save, and another model reads that saved state from disk.
    REQUIRE (model.addAudioTrack().dispatched);
    REQUIRE (model.saveProjectBundle().ok());
    REQUIRE_FALSE (model.hasUnsavedChanges());
    yesdaw::ui::UiAppModel reopened;
    REQUIRE (reopened.openProjectBundle (original).ok());
    REQUIRE (reopened.project().tracks.size() == 3u);
}
