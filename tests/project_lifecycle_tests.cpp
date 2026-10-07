// YES DAW - ADR-0060 cp1 gates: the New Project dialog, New refusing an occupied target, unsaved changes before New
// and Open, and the audio device asked to run at the project's rate.

#include "persistence/ProjectBundle.h"
#include "ui/MainComponent.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using yesdaw::ui::MainComponentFileChoices;
using yesdaw::ui::UiActionId;
using yesdaw::ui::UiNewProjectChoices;

namespace {

std::filesystem::path lifecycleScratch (const std::string& label)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-lifecycle-" + label + "-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

yesdaw::engine::Project readProject (const std::filesystem::path& bundle)
{
    yesdaw::persistence::ProjectBundleDb db;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::openExistingBundle (bundle, db).ok());
    yesdaw::engine::Project project;
    REQUIRE (db.readProjectSnapshot (project).ok());
    return project;
}

std::string bytesOf (const std::filesystem::path& path)
{
    std::ifstream in (path, std::ios::binary);
    return std::string ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
}

juce::var probeOf (juce::Component& shell)
{
    return juce::JSON::parse (juce::String (yesdaw::ui::mainComponentStateProbeJson (shell)));
}

juce::String statusOf (juce::Component& shell)
{
    return probeOf (shell)["status"]["text"].toString();
}

bool press (juce::Component& shell, int keyCode, juce::ModifierKeys mods = {})
{
    return shell.keyPressed (juce::KeyPress (keyCode, mods, 0));
}

bool dialogOpen (juce::Component& shell)
{
    return static_cast<bool> (probeOf (shell)["view"]["newProjectDialog"]);
}

struct LifecycleShell
{
    std::filesystem::path directory;
    std::filesystem::path nextBundle;
    std::optional<UiNewProjectChoices> dialogAnswer;
    int closeChoice = yesdaw::ui::kCloseChoiceClose;
    int prompts = 0;
    std::vector<double> rateRequests;
    bool deviceAccepts = true;
    double deviceRate = 48'000.0;   // the fake output device
    std::filesystem::path saveAsBundle;   // the Save As chooser's answer (empty = cancelled)
    int saveAsAsks = 0;
    std::unique_ptr<juce::Component> shell;

    // `launchSession`: the native launch (the last project or `openAtLaunch`, else the untitled session).
    explicit LifecycleShell (const std::string& label, bool overlay = false, bool launchSession = false,
                             const std::filesystem::path& openAtLaunch = {})
        : directory (lifecycleScratch (label))
    {
        juce::MessageManager::getInstance();
        MainComponentFileChoices choices;
        choices.sessionStateDirectory = directory / "session";
        std::filesystem::create_directories (choices.sessionStateDirectory);
        choices.chooseNewProjectBundle = [this] { return nextBundle; };
        choices.chooseOpenProjectBundle = [this] { return nextBundle; };
        if (! overlay)
            choices.newProjectDialogChoices = [this] { return dialogAnswer; };
        choices.newProjectDialog = overlay;
        choices.confirmCloseUnsavedChanges = [this] {
            ++prompts;
            return closeChoice;
        };
        choices.requestAudioDeviceSampleRate = [this] (double hz) {
            rateRequests.push_back (hz);
            if (deviceAccepts)
                deviceRate = hz;
            return deviceAccepts;
        };
        choices.currentAudioDeviceSampleRate = [this] { return deviceRate; };
        choices.chooseSaveAsProjectBundle = [this] {
            ++saveAsAsks;
            return saveAsBundle;
        };
        choices.initialiseSessionAtLaunch = launchSession;
        choices.openBundleAtLaunch = openAtLaunch;
        shell = yesdaw::ui::createMainComponent (std::move (choices));
        REQUIRE (shell != nullptr);
    }

    juce::Component& operator*() { return *shell; }

    void newProject (const std::string& name, std::optional<UiNewProjectChoices> answer)
    {
        nextBundle = directory / (name + ".yesdaw");
        dialogAnswer = answer;
        yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ProjectNew);
    }
};

UiNewProjectChoices choicesOf (double rate, double bpm, std::uint16_t numerator, std::uint16_t denominator)
{
    UiNewProjectChoices choices;
    choices.sampleRateHz = rate;
    choices.bpm = bpm;
    choices.meterNumerator = numerator;
    choices.meterDenominator = denominator;
    return choices;
}

} // namespace

TEST_CASE ("ADR-0060 New creates the project at the chosen rate, tempo and meter, remembers them; Cancel changes nothing",
           "[project-lifecycle]")
{
    LifecycleShell f ("create");
    f.newProject ("song", choicesOf (44'100.0, 96.0, 7, 8));
    const yesdaw::engine::Project project = readProject (f.directory / "song.yesdaw");
    REQUIRE (project.sampleRate.hz == 44'100.0);
    REQUIRE (project.tempoMap.size() == 1u);
    REQUIRE (project.tempoMap[0].bpm == 96.0);
    REQUIRE (project.meterMap.size() == 1u);
    REQUIRE (project.meterMap[0].numerator == 7u);
    REQUIRE (project.meterMap[0].denominator == 8u);
    REQUIRE (project.tracks.size() == 1u);   // the Default template

    // The choices are remembered (a new shell on the same session folder opens the dialog with them).
    {
        MainComponentFileChoices choices;
        choices.sessionStateDirectory = f.directory / "session";
        choices.newProjectDialog = true;
        auto shell = yesdaw::ui::createMainComponent (std::move (choices));
        yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ProjectNew);
        const UiNewProjectChoices shown = yesdaw::ui::mainComponentNewProjectDialogChoices (*shell);
        REQUIRE (shown.sampleRateHz == 44'100.0);
        REQUIRE (shown.bpm == 96.0);
        REQUIRE (shown.meterNumerator == 7u);
        REQUIRE (shown.meterDenominator == 8u);
    }

    // Cancel: no bundle, the current project unchanged.
    f.newProject ("never", std::nullopt);
    REQUIRE_FALSE (std::filesystem::exists (f.directory / "never.yesdaw"));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*f).windowTitle.find ("song") != std::string::npos);
}

TEST_CASE ("ADR-0060 the New Project overlay: the keyboard alone creates a project; Esc closes it creating nothing",
           "[project-lifecycle]")
{
    LifecycleShell f ("overlay", true);
    f.nextBundle = f.directory / "keyed.yesdaw";
    REQUIRE (press (*f, 'n', juce::ModifierKeys::ctrlModifier));
    REQUIRE (static_cast<bool> (probeOf (*f)["view"]["newProjectDialog"]));

    // Tab stays inside the overlay.
    const auto order = yesdaw::ui::mainComponentControlTraversal (*f);
    REQUIRE_FALSE (order.empty());
    for (const juce::String& id : order)
        REQUIRE (id.startsWith ("newproject."));

    // Tab to the rate chooser, Enter, Up to 44.1 kHz (from the remembered 48), Enter; Tab to Create, Enter.
    REQUIRE (press (*f, juce::KeyPress::tabKey));
    for (std::size_t i = 0; i < order.size() && yesdaw::ui::mainComponentControlTarget (*f).id != "newproject.rate"; ++i)
        REQUIRE (press (*f, juce::KeyPress::tabKey));
    REQUIRE (yesdaw::ui::mainComponentControlTarget (*f).id == "newproject.rate");
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    REQUIRE (press (*f, juce::KeyPress::upKey));
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    for (std::size_t i = 0; i < order.size() && yesdaw::ui::mainComponentControlTarget (*f).id != "newproject.create"; ++i)
        REQUIRE (press (*f, juce::KeyPress::tabKey));
    REQUIRE (yesdaw::ui::mainComponentControlTarget (*f).id == "newproject.create");
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);   // the button's click is posted
    REQUIRE_FALSE (static_cast<bool> (probeOf (*f)["view"]["newProjectDialog"]));
    REQUIRE (readProject (f.directory / "keyed.yesdaw").sampleRate.hz == 44'100.0);

    // Esc closes it, creating nothing: one press when not navigating; while navigating, ADR-0049's order holds —
    // the first Esc ends navigation, the second closes.
    LifecycleShell g ("overlay-esc", true);
    g.nextBundle = g.directory / "escaped.yesdaw";
    REQUIRE (press (*g, 'n', juce::ModifierKeys::ctrlModifier));
    REQUIRE (dialogOpen (*g));
    REQUIRE (press (*g, juce::KeyPress::escapeKey));
    REQUIRE_FALSE (dialogOpen (*g));
    REQUIRE (press (*g, 'n', juce::ModifierKeys::ctrlModifier));
    REQUIRE (press (*g, juce::KeyPress::tabKey));
    REQUIRE (press (*g, juce::KeyPress::escapeKey));
    REQUIRE (dialogOpen (*g));
    REQUIRE (press (*g, juce::KeyPress::escapeKey));
    REQUIRE_FALSE (dialogOpen (*g));
    REQUIRE_FALSE (std::filesystem::exists (g.directory / "escaped.yesdaw"));
}

TEST_CASE ("ADR-0060 New refuses an occupied target and leaves it byte-identical; a failed New keeps the current project",
           "[project-lifecycle]")
{
    LifecycleShell f ("occupied");
    f.newProject ("song", choicesOf (48'000.0, 120.0, 4, 4));
    REQUIRE (std::filesystem::exists (f.directory / "song.yesdaw"));
    const std::string before = bytesOf (f.directory / "song.yesdaw" / "project.db");

    LifecycleShell g ("occupied-other");
    g.newProject ("current", choicesOf (48'000.0, 120.0, 4, 4));
    g.nextBundle = f.directory / "song.yesdaw";
    g.dialogAnswer = choicesOf (96'000.0, 100.0, 3, 4);
    yesdaw::ui::mainComponentDispatchAction (*g, UiActionId::ProjectNew);
    REQUIRE (statusOf (*g).contains ("a project already exists"));
    REQUIRE (bytesOf (f.directory / "song.yesdaw" / "project.db") == before);
    REQUIRE (yesdaw::ui::snapshotMainComponent (*g).windowTitle.find ("current") != std::string::npos);

    // A creation that fails (its folder's parent is a file) leaves the current project current, its undo intact.
    yesdaw::ui::mainComponentDispatchAction (*g, UiActionId::TrackAdd);
    REQUIRE (yesdaw::ui::snapshotMainComponent (*g).context.canUndo);
    {
        std::ofstream blocker (g.directory / "blocker");
        blocker << "a file";
    }
    g.closeChoice = yesdaw::ui::kCloseChoiceClose;   // don't save
    g.nextBundle = g.directory / "blocker" / "new.yesdaw";
    g.dialogAnswer = choicesOf (48'000.0, 120.0, 4, 4);
    yesdaw::ui::mainComponentDispatchAction (*g, UiActionId::ProjectNew);
    REQUIRE (statusOf (*g).contains ("New project failed"));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*g).windowTitle.find ("current") != std::string::npos);
    REQUIRE (yesdaw::ui::snapshotMainComponent (*g).context.canUndo);
}

TEST_CASE ("ADR-0060 New and Open with unsaved changes ask Save / Don't Save / Cancel and do what was chosen",
           "[project-lifecycle]")
{
    LifecycleShell f ("unsaved");
    f.newProject ("first", choicesOf (48'000.0, 120.0, 4, 4));
    REQUIRE (f.prompts == 0);   // a clean project is replaced without asking

    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::TrackAdd);   // now it has unsaved changes
    f.closeChoice = yesdaw::ui::kCloseChoiceCancel;
    f.newProject ("second", choicesOf (48'000.0, 120.0, 4, 4));
    REQUIRE (f.prompts == 1);
    REQUIRE_FALSE (std::filesystem::exists (f.directory / "second.yesdaw"));   // Cancel: nothing happened

    f.closeChoice = yesdaw::ui::kCloseChoiceSave;
    f.newProject ("second", choicesOf (48'000.0, 120.0, 4, 4));
    REQUIRE (f.prompts == 2);
    REQUIRE (std::filesystem::exists (f.directory / "second.yesdaw"));   // saved, then created
    REQUIRE (readProject (f.directory / "first.yesdaw").tracks.size() == 2u);

    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::TrackAdd);
    f.closeChoice = yesdaw::ui::kCloseChoiceCancel;
    f.nextBundle = f.directory / "first.yesdaw";
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectOpen);
    REQUIRE (f.prompts == 3);
    REQUIRE (yesdaw::ui::snapshotMainComponent (*f).windowTitle.find ("second") != std::string::npos);   // Open cancelled

    f.closeChoice = yesdaw::ui::kCloseChoiceClose;   // Don't Save: open the other one
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectOpen);
    REQUIRE (f.prompts == 4);
    REQUIRE (yesdaw::ui::snapshotMainComponent (*f).windowTitle.find ("first") != std::string::npos);
}

TEST_CASE ("ADR-0060 the device is asked to run at the project's rate; when it cannot, the warning names both rates",
           "[project-lifecycle]")
{
    LifecycleShell f ("device-rate");
    f.newProject ("start", choicesOf (48'000.0, 120.0, 4, 4));   // the fake device runs at 48 kHz
    REQUIRE (f.rateRequests.empty());
    REQUIRE (static_cast<double> (probeOf (*f)["audio"]["deviceRateHz"]) == 48'000.0);

    f.newProject ("cd", choicesOf (44'100.0, 120.0, 4, 4));
    REQUIRE (f.rateRequests == std::vector<double> { 44'100.0 });
    REQUIRE (static_cast<double> (probeOf (*f)["audio"]["deviceRateHz"]) == 44'100.0);
    REQUIRE (static_cast<int> (probeOf (*f)["audio"]["rateRequests"]) == 1);

    f.newProject ("same", choicesOf (44'100.0, 120.0, 4, 4));   // already at the rate: not asked again
    REQUIRE (f.rateRequests.size() == 1u);

    f.deviceAccepts = false;
    f.newProject ("hi", choicesOf (96'000.0, 120.0, 4, 4));
    REQUIRE (f.rateRequests.size() == 2u);
    REQUIRE (statusOf (*f).contains ("44100 Hz"));
    REQUIRE (statusOf (*f).contains ("96000 Hz"));
}

TEST_CASE ("ADR-0060 the untitled launch session: clean is replaced without asking; Save goes through Save As, and "
           "cancelling it cancels New",
           "[project-lifecycle]")
{
    {
        LifecycleShell clean ("untitled-clean", false, true);
        REQUIRE (yesdaw::ui::snapshotMainComponent (*clean).context.projectLoaded);
        clean.newProject ("fresh", choicesOf (48'000.0, 120.0, 4, 4));
        REQUIRE (clean.prompts == 0);
        REQUIRE (std::filesystem::exists (clean.directory / "fresh.yesdaw"));
    }

    LifecycleShell f ("untitled", false, true);
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::TrackAdd);   // unsaved changes in the untitled session
    f.closeChoice = yesdaw::ui::kCloseChoiceSave;
    f.newProject ("next", choicesOf (48'000.0, 120.0, 4, 4));   // Save -> Save As, whose chooser is cancelled
    REQUIRE (f.prompts == 1);
    REQUIRE (f.saveAsAsks == 1);
    REQUIRE_FALSE (std::filesystem::exists (f.directory / "next.yesdaw"));

    f.saveAsBundle = f.directory / "kept.yesdaw";
    f.newProject ("next", choicesOf (48'000.0, 120.0, 4, 4));   // Save -> Save As to kept, then New
    REQUIRE (f.prompts == 2);
    REQUIRE (f.saveAsAsks == 2);
    REQUIRE (readProject (f.directory / "kept.yesdaw").tracks.size() == 2u);
    REQUIRE (std::filesystem::exists (f.directory / "next.yesdaw"));
    REQUIRE (yesdaw::ui::snapshotMainComponent (*f).windowTitle.find ("next") != std::string::npos);

    LifecycleShell g ("untitled-discard", false, true);
    yesdaw::ui::mainComponentDispatchAction (*g, UiActionId::TrackAdd);
    g.closeChoice = yesdaw::ui::kCloseChoiceClose;   // Don't Save
    g.newProject ("other", choicesOf (48'000.0, 120.0, 4, 4));
    REQUIRE (g.prompts == 1);
    REQUIRE (g.saveAsAsks == 0);
    REQUIRE (std::filesystem::exists (g.directory / "other.yesdaw"));
}

TEST_CASE ("ADR-0060 a project opened at launch asks the device for its rate once the device is open", "[project-lifecycle]")
{
    std::filesystem::path bundle;
    {
        LifecycleShell maker ("launch-maker");
        maker.newProject ("cd", choicesOf (44'100.0, 120.0, 4, 4));
        bundle = maker.directory / "cd.yesdaw";
    }
    LifecycleShell f ("launch", false, true, bundle);
    REQUIRE (yesdaw::ui::snapshotMainComponent (*f).windowTitle.find ("cd") != std::string::npos);
    REQUIRE (f.rateRequests == std::vector<double> { 44'100.0 });
}

TEST_CASE ("ADR-0060 the dialog's rate, tempo and meter apply to an injected project too; out-of-range choices are refused",
           "[project-lifecycle]")
{
    const auto directory = lifecycleScratch ("inject");
    std::optional<UiNewProjectChoices> answer = choicesOf (88'200.0, 140.0, 5, 4);
    std::filesystem::path next = directory / "injected.yesdaw";
    MainComponentFileChoices choices;
    choices.sessionStateDirectory = directory / "session";
    std::filesystem::create_directories (choices.sessionStateDirectory);
    choices.chooseNewProjectBundle = [&next] { return next; };
    choices.newProjectDialogChoices = [&answer] { return answer; };
    choices.makeNewProject = [] {
        yesdaw::engine::Project project = yesdaw::ui::UiAppModel::makeDefaultSessionProject();
        project.tracks.front().colour = 0xFF336699u;
        return project;
    };
    auto shell = yesdaw::ui::createMainComponent (std::move (choices));
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ProjectNew);
    const yesdaw::engine::Project project = readProject (directory / "injected.yesdaw");
    REQUIRE (project.sampleRate.hz == 88'200.0);
    REQUIRE (project.tempoMap.front().bpm == 140.0);
    REQUIRE (project.meterMap.front().numerator == 5u);
    REQUIRE (project.tracks.front().colour == 0xFF336699u);   // the injected content is kept

    answer = choicesOf (12'345.0, 140.0, 5, 4);
    next = directory / "refused.yesdaw";
    yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ProjectNew);
    REQUIRE (statusOf (*shell).contains ("New project refused"));
    REQUIRE_FALSE (std::filesystem::exists (next));
}

TEST_CASE ("ADR-0060 the remembered choices: radix '.' both ways; a bad value or an out-of-range set falls back to the "
           "defaults",
           "[project-lifecycle]")
{
    const auto directory = lifecycleScratch ("record");
    yesdaw::ui::UiAppModel model;
    model.setSessionStateDirectory (directory);
    const auto record = directory / yesdaw::ui::UiAppModel::kNewProjectRecordFileName;
    const auto readWith = [&model, &record] (const std::string& text) {
        {
            std::ofstream out (record, std::ios::binary | std::ios::trunc);
            out << text;
        }
        return model.newProjectChoices();
    };

    model.setNewProjectChoices (choicesOf (88'200.0, 96.5, 7, 8));   // a fractional tempo round-trips exactly
    const std::string written = bytesOf (record);
    REQUIRE (written.find ("bpm=96.5") != std::string::npos);
    REQUIRE (written.find (',') == std::string::npos);
    UiNewProjectChoices back = model.newProjectChoices();
    REQUIRE (back.sampleRateHz == 88'200.0);
    REQUIRE (back.bpm == 96.5);
    REQUIRE (back.meterNumerator == 7u);
    REQUIRE (back.meterDenominator == 8u);

    back = readWith ("bpm=90\n");   // a partial record: the rest are the defaults
    REQUIRE (back.bpm == 90.0);
    REQUIRE (back.sampleRateHz == 48'000.0);
    REQUIRE (back.meterNumerator == 4u);
    back = readWith ("rate=96000\nfuture=1\n");   // an unknown key is skipped
    REQUIRE (back.sampleRateHz == 96'000.0);

    for (const char* bad : { "rate=abc\nbpm=90\n", "bpm=96,5\nrate=44100\n", "rate=44100\nmeter=7/0\n", "meter=40/4\n",
                             "rate=12345\n", "bpm=1e400\n", "meter=3.5/4\n", "meter=7\n" })
    {
        INFO (bad);
        back = readWith (bad);
        REQUIRE (back.sampleRateHz == 48'000.0);
        REQUIRE (back.bpm == 120.0);
        REQUIRE (back.meterNumerator == 4u);
        REQUIRE (back.meterDenominator == 4u);
    }
}
