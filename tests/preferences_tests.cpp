// YES DAW - ADR-0061 gates: the user's preferences in prefs.json. cp1: the file and the keymap; the per-user records
// written whole.

#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using yesdaw::ui::UiActionId;
using yesdaw::ui::UiPreferencesState;

namespace {

std::filesystem::path prefsScratch (const std::string& label)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-prefs-" + label + "-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

std::string textOf (const std::filesystem::path& path)
{
    std::ifstream in (path, std::ios::binary);
    return std::string ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
}

void writeText (const std::filesystem::path& path, const std::string& text)
{
    std::ofstream out (path, std::ios::binary | std::ios::trunc);
    out << text;
}

bool anyTemporaryIn (const std::filesystem::path& directory)
{
    for (const auto& entry : std::filesystem::directory_iterator (directory))
        if (entry.path().extension() == ".tmp")
            return true;
    return false;
}

std::string chordOf (const yesdaw::ui::UiAppModel& model, UiActionId id)
{
    return model.registry().keymap().chordFor (id);
}

constexpr const char* kCopyChord = "Ctrl+Alt+Shift+F9";   // free: Save a Copy carries no default chord

} // namespace

TEST_CASE ("ADR-0061 a rebind lives in prefs.json across launches; an old keymap-overrides.txt is imported once and "
           "retired",
           "[prefs]")
{
    const auto directory = prefsScratch ("keymap");
    writeText (directory / "keymap-overrides.txt", std::string ("project.save_a_copy\t") + kCopyChord + "\nproject.save_as\t-\n");
    {
        yesdaw::ui::UiAppModel model;
        model.setSessionStateDirectory (directory);
        REQUIRE (chordOf (model, UiActionId::ProjectSaveACopy) == kCopyChord);
        REQUIRE (chordOf (model, UiActionId::ProjectSaveAs).empty());   // "-" = unbound
        REQUIRE (model.migratedKeymapPreferences());
        REQUIRE (model.preferencesState() == UiPreferencesState::Missing);   // the first launch of prefs.json
        REQUIRE (model.statusLineText().empty());                            // the import is silent
        REQUIRE_FALSE (std::filesystem::exists (directory / "keymap-overrides.txt"));
        REQUIRE (std::filesystem::exists (directory / "keymap-overrides.txt.migrated"));
        REQUIRE (textOf (directory / "prefs.json").find (kCopyChord) != std::string::npos);
        REQUIRE_FALSE (anyTemporaryIn (directory));
    }

    // A stale old record is never imported again once prefs.json holds a keymap.
    writeText (directory / "keymap-overrides.txt", "project.save_a_copy\tCtrl+Alt+Shift+F11\n");
    {
        yesdaw::ui::UiAppModel model;
        model.setSessionStateDirectory (directory);
        REQUIRE (model.preferencesState() == UiPreferencesState::Loaded);
        REQUIRE_FALSE (model.migratedKeymapPreferences());
        REQUIRE (chordOf (model, UiActionId::ProjectSaveACopy) == kCopyChord);
        REQUIRE (chordOf (model, UiActionId::ProjectSaveAs).empty());
        model.setSessionStateDirectory (directory);   // the same folder again reads nothing
        REQUIRE (chordOf (model, UiActionId::ProjectSaveACopy) == kCopyChord);

        model.restoreDefaultKeymap();
    }
    {
        yesdaw::ui::UiAppModel model;
        model.setSessionStateDirectory (directory);
        REQUIRE (chordOf (model, UiActionId::ProjectSaveACopy).empty());
        REQUIRE (chordOf (model, UiActionId::ProjectSaveAs) == "Ctrl+Shift+S");
    }

    // No folder: preferences live in memory only; nothing is read or written.
    const bool strayBefore = std::filesystem::exists (std::filesystem::current_path() / "prefs.json");
    yesdaw::ui::UiAppModel inMemory;
    REQUIRE (inMemory.preferencesState() == UiPreferencesState::NotRead);
    REQUIRE (inMemory.rebindChord (UiActionId::ProjectSaveACopy, kCopyChord) == yesdaw::ui::KeymapRebindStatus::Ok);
    REQUIRE (std::filesystem::exists (std::filesystem::current_path() / "prefs.json") == strayBefore);
}

TEST_CASE ("ADR-0061 bindings apply as one set: two actions that swapped chords both bind; an editor's byte-order "
           "mark and a whole-number float version are read",
           "[prefs]")
{
    const auto directory = prefsScratch ("swap");
    // New and Open traded chords: whichever is applied first, the other still holds its default until cleared.
    writeText (directory / "prefs.json",
               "\xEF\xBB\xBF{ \"version\": 7.0, \"keymap\": { \"project.new\": \"Ctrl+O\", \"project.open\": \"Ctrl+N\" } }");
    yesdaw::ui::UiAppModel model;
    model.setSessionStateDirectory (directory);
    REQUIRE (model.preferencesState() == UiPreferencesState::Loaded);
    REQUIRE (model.rejectedPreferenceKeys() == 0);
    REQUIRE (chordOf (model, UiActionId::ProjectNew) == "Ctrl+O");
    REQUIRE (chordOf (model, UiActionId::ProjectOpen) == "Ctrl+N");
    REQUIRE (model.rebindChord (UiActionId::ProjectSaveACopy, kCopyChord) == yesdaw::ui::KeymapRebindStatus::Ok);
    REQUIRE (textOf (directory / "prefs.json").find ("\"version\": 7") != std::string::npos);

    // The same swap in the retired record imports the same way.
    const auto old = prefsScratch ("swap-old");
    writeText (old / "keymap-overrides.txt", "project.new\tCtrl+O\nproject.open\tCtrl+N\n");
    yesdaw::ui::UiAppModel imported;
    imported.setSessionStateDirectory (old);
    REQUIRE (chordOf (imported, UiActionId::ProjectNew) == "Ctrl+O");
    REQUIRE (chordOf (imported, UiActionId::ProjectOpen) == "Ctrl+N");

    // An unreadable prefs.json never imports the old record behind the "defaults are in use" message.
    const auto both = prefsScratch ("unreadable-old");
    writeText (both / "prefs.json", "{ broken");
    writeText (both / "keymap-overrides.txt", std::string ("project.save_a_copy\t") + kCopyChord + "\n");
    yesdaw::ui::UiAppModel unreadable;
    unreadable.setSessionStateDirectory (both);
    REQUIRE (unreadable.preferencesState() == UiPreferencesState::Unreadable);
    REQUIRE (chordOf (unreadable, UiActionId::ProjectSaveACopy).empty());
    REQUIRE (std::filesystem::exists (both / "keymap-overrides.txt"));
}

TEST_CASE ("ADR-0061 a prefs.json that cannot be read gives the defaults, is kept aside and is reported", "[prefs]")
{
    for (const char* bad : { "", "{\"version\": 1, \"keymap\": {", "not json", "[1, 2]", "\"a string\"" })
    {
        INFO (bad);
        const auto directory = prefsScratch ("unreadable");
        writeText (directory / "prefs.json", bad);
        yesdaw::ui::UiAppModel model;
        model.setSessionStateDirectory (directory);
        REQUIRE (model.preferencesState() == UiPreferencesState::Unreadable);
        REQUIRE (model.statusLineIsError());
        REQUIRE (model.statusLineText().find ("Preferences could not be read") != std::string::npos);
        REQUIRE (textOf (directory / "prefs.json.unreadable") == bad);
        REQUIRE_FALSE (std::filesystem::exists (directory / "prefs.json"));
        REQUIRE (chordOf (model, UiActionId::ProjectSaveAs) == "Ctrl+Shift+S");

        // The next change writes a fresh, readable file.
        REQUIRE (model.rebindChord (UiActionId::ProjectSaveACopy, kCopyChord) == yesdaw::ui::KeymapRebindStatus::Ok);
        yesdaw::ui::UiAppModel relaunched;
        relaunched.setSessionStateDirectory (directory);
        REQUIRE (relaunched.preferencesState() == UiPreferencesState::Loaded);
        REQUIRE (chordOf (relaunched, UiActionId::ProjectSaveACopy) == kCopyChord);
    }
}

TEST_CASE ("ADR-0061 a malformed key falls back alone; unknown keys and a higher version survive a rewrite", "[prefs]")
{
    const auto directory = prefsScratch ("keys");
    writeText (directory / "prefs.json",
               std::string ("{ \"version\": 7, \"future\": { \"x\": [1, 2] },\n")
                   + "  \"keymap\": { \"project.save_a_copy\": \"" + kCopyChord + "\", \"project.save_as\": 5,\n"
                   + "              \"action.from.the.future\": \"Ctrl+F12\", \"project.new\": \"Ctrl+O\" } }\n");
    yesdaw::ui::UiAppModel model;
    model.setSessionStateDirectory (directory);
    REQUIRE (model.preferencesState() == UiPreferencesState::Loaded);
    REQUIRE (model.rejectedPreferenceKeys() == 2);   // a number for a chord; a chord Open already owns
    REQUIRE (chordOf (model, UiActionId::ProjectSaveACopy) == kCopyChord);
    REQUIRE (chordOf (model, UiActionId::ProjectSaveAs) == "Ctrl+Shift+S");
    REQUIRE (chordOf (model, UiActionId::ProjectNew) == "Ctrl+N");
    REQUIRE (model.statusLineText().empty());

    REQUIRE (model.rebindChord (UiActionId::ProjectSaveAsTemplate, "Ctrl+Alt+Shift+F10") == yesdaw::ui::KeymapRebindStatus::Ok);
    const std::string rewritten = textOf (directory / "prefs.json");
    REQUIRE (rewritten.find ("\"future\"") != std::string::npos);
    REQUIRE (rewritten.find ("action.from.the.future") != std::string::npos);
    REQUIRE (rewritten.find ("Ctrl+Alt+Shift+F10") != std::string::npos);
    REQUIRE (rewritten.find ("\"version\": 7") != std::string::npos);
    REQUIRE_FALSE (anyTemporaryIn (directory));

    // A version of the wrong type counts as one rejected key; the rest still loads.
    const auto other = prefsScratch ("version");
    writeText (other / "prefs.json", std::string ("{ \"version\": \"one\", \"keymap\": { \"project.save_a_copy\": \"") + kCopyChord + "\" } }");
    yesdaw::ui::UiAppModel second;
    second.setSessionStateDirectory (other);
    REQUIRE (second.rejectedPreferenceKeys() == 1);
    REQUIRE (chordOf (second, UiActionId::ProjectSaveACopy) == kCopyChord);
}

TEST_CASE ("ADR-0061 the last-project, recent-projects and view-state records are written whole", "[prefs]")
{
    const auto directory = prefsScratch ("records");
    yesdaw::ui::UiAppModel model;
    model.setSessionStateDirectory (directory / "session");
    const auto song = directory / "song.yesdaw";
    REQUIRE (model.createProjectBundle (song).ok());
    REQUIRE (model.readLastProjectRecord() == song);
    REQUIRE (model.recentProjectBundles().at (0) == song);
    REQUIRE_FALSE (anyTemporaryIn (directory / "session"));

    model.writeViewStateRecord ("rail\t300\ndock\t240\n");
    REQUIRE (textOf (song / "view-state.txt") == "rail\t300\ndock\t240\n");
    REQUIRE (model.readViewStateRecord() == "rail\t300\ndock\t240\n");
    REQUIRE_FALSE (anyTemporaryIn (song));
}
