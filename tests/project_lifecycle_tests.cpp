// YES DAW - ADR-0060 gates. cp1: the New Project dialog, New refusing an occupied target, unsaved changes before New
// and Open, and the audio device asked to run at the project's rate. cp2: Save As and Save a Copy through one atomic
// bundle copy. cp3: templates - a layout without content, instantiated with fresh identities.

#include "io/WavFile.h"
#include "persistence/AutosaveRecovery.h"
#include "persistence/ProjectBundle.h"
#include "ui/MainComponent.h"
#include "ui/MainComponentInternal.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
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
    std::filesystem::path saveACopyBundle;   // the Save a Copy chooser's answer
    std::string templateName;                // the Save as Template name box's answer (empty = cancelled)
    bool replaceTemplate = false;            // the replace question's answer
    int replaceAsks = 0;
    std::string replaceAskedFor;
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
        choices.chooseSaveACopyProjectBundle = [this] { return saveACopyBundle; };
        choices.chooseSaveAsTemplateName = [this] { return templateName; };
        choices.confirmReplaceTemplate = [this] (const std::string& name) {
            ++replaceAsks;
            replaceAskedFor = name;
            return replaceTemplate;
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

// ---- cp2: a source with an Asset, unsaved changes and an autosave of its own ----

void writeTone (const std::filesystem::path& path, std::size_t frames, float level)
{
    std::vector<float> samples (frames);
    for (std::size_t i = 0; i < frames; ++i)
        samples[i] = level * static_cast<float> ((i % 120u) < 60u ? 1.0 : -1.0);
    REQUIRE (yesdaw::io::writeFloat32WavFile (path, yesdaw::engine::SampleRate { 48'000.0 }, 1, samples.size(), samples).ok());
}

struct CopyFixture
{
    std::filesystem::path directory;
    std::filesystem::path source;
    yesdaw::ui::UiAppModel model;

    explicit CopyFixture (const std::string& label) : directory (lifecycleScratch (label)), source (directory / "source.yesdaw")
    {
        model.setSessionStateDirectory (directory / "session");
        writeTone (directory / "tone.wav", 48'000, 0.25f);
        REQUIRE (model.createProjectBundle (source).ok());
        yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (directory / "tone.wav");
        REQUIRE (decoded.decoded.has_value());
        REQUIRE (model.importAudioFile (directory / "tone.wav", std::move (*decoded.decoded)).ok());
        REQUIRE (model.addAudioTrack().dispatched);
        REQUIRE (model.hasUnsavedChanges());
        // The source's own autosave: a copy never carries it (it would open with a recovery prompt).
        std::filesystem::create_directories (yesdaw::persistence::autosaveSnapshotPath (source));
        REQUIRE (yesdaw::persistence::autosave_detail::anySnapshotSlotExists (source));
    }
};

std::string renderOf (yesdaw::ui::UiAppModel& model, const std::filesystem::path& wav)
{
    REQUIRE (model.exportAudioFile (wav).dispatched);
    REQUIRE (std::filesystem::exists (wav));
    return bytesOf (wav);
}

// The bundle reopened from disk (its Assets decoded as Open decodes them) renders this.
std::string renderReopened (const std::filesystem::path& bundle, const std::filesystem::path& wav)
{
    yesdaw::ui::UiAppModel reopened;
    yesdaw::ui::shell::StoredProjectAssetsResult stored = yesdaw::ui::shell::decodeStoredProjectAssets (bundle);
    REQUIRE (stored.assets.has_value());
    REQUIRE (reopened.loadPreparedProjectBundle (std::move (stored.prepared), std::move (*stored.assets)).ok());
    REQUIRE (reopened.project().tracks.size() == 2u);
    REQUIRE (reopened.project().clips.size() == 1u);
    REQUIRE (reopened.project().assets.size() == 1u);
    return renderOf (reopened, wav);
}

void requireSameAssetFiles (const std::filesystem::path& a, const std::filesystem::path& b)
{
    std::size_t files = 0;
    for (const auto& entry : std::filesystem::directory_iterator (a / "audio"))
    {
        INFO (entry.path().filename().string());
        REQUIRE (bytesOf (entry.path()) == bytesOf (b / "audio" / entry.path().filename()));
        ++files;
    }
    REQUIRE (files >= 1u);
}

bool anyPartialIn (const std::filesystem::path& directory)
{
    for (const auto& entry : std::filesystem::directory_iterator (directory))
        if (entry.path().extension() == ".partial")
            return true;
    return false;
}

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
    // The dialog's controls are in the probe's layout whatever the dock shows: a drive clicks Create by name
    // (2026-10-07: they were published only with the dock open). Open the piano roll's tab, then close the dock.
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ViewPianoRoll);
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ViewPianoRoll);
    REQUIRE (probeOf (*f)["view"]["dock"].toString() == "None");
    REQUIRE (press (*f, 'n', juce::ModifierKeys::ctrlModifier));
    REQUIRE (static_cast<bool> (probeOf (*f)["view"]["newProjectDialog"]));
    for (const char* id : { "newproject.rate", "newproject.tempo", "newproject.meter.numerator", "newproject.meter.denominator",
                            "newproject.template", "newproject.create", "newproject.cancel" })
    {
        INFO ("layout " << id);
        REQUIRE (probeOf (*f)["layout"].hasProperty (id));
    }

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

// ---- cp2: Save As and Save a Copy through one atomic bundle copy ----

TEST_CASE ("ADR-0060 Save As writes the copy through a .partial folder and continues in it; the source is never written",
           "[project-lifecycle]")
{
    CopyFixture f ("save-as");
    const std::string mix = renderOf (f.model, f.directory / "before.wav");
    REQUIRE (readProject (f.source).tracks.size() == 2u);

    const auto copy = f.directory / "copy.yesdaw";
    std::filesystem::path seen;
    REQUIRE (f.model.saveProjectBundleAs (copy, [&] (const std::filesystem::path& partial) {
        seen = partial;
        REQUIRE_FALSE (std::filesystem::exists (copy));   // nothing at the target until the rename
        REQUIRE (std::filesystem::exists (partial / "project.db"));
    }).dispatched);
    REQUIRE (seen.extension() == ".partial");
    REQUIRE_FALSE (anyPartialIn (f.directory));
    REQUIRE (f.model.bundlePath() == copy);
    REQUIRE_FALSE (f.model.hasUnsavedChanges());   // the copy is current and clean
    requireSameAssetFiles (f.source, copy);
    REQUIRE_FALSE (yesdaw::persistence::autosave_detail::anySnapshotSlotExists (copy));   // no recovery prompt
    REQUIRE (std::filesystem::is_directory (copy / "autosave"));
    REQUIRE (renderReopened (copy, f.directory / "after.wav") == mix);

    // Undo continues, in the copy; the source is never written.
    REQUIRE (f.model.context().canUndo);
    REQUIRE (f.model.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (readProject (copy).tracks.size() == 1u);
    REQUIRE (readProject (f.source).tracks.size() == 2u);
    REQUIRE (yesdaw::persistence::autosave_detail::anySnapshotSlotExists (f.source));
}

TEST_CASE ("ADR-0060 Save a Copy writes and closes the copy; the source stays current, unsaved, undoable and saveable",
           "[project-lifecycle]")
{
    CopyFixture f ("save-a-copy");
    const std::string mix = renderOf (f.model, f.directory / "before.wav");
    // A peak cache mid-write (the builder's temporary file) is not copied, and does not fail the copy.
    std::filesystem::create_directories (f.source / "peaks");
    {
        std::ofstream partialCache (f.source / "peaks" / ".mid-write.ypeaks.tmp", std::ios::binary);
        partialCache << "half a cache";
    }
    const auto copy = f.directory / "copy.yesdaw";
    REQUIRE (f.model.saveProjectBundleCopy (copy).dispatched);
    REQUIRE_FALSE (std::filesystem::exists (copy / "peaks" / ".mid-write.ypeaks.tmp"));
    REQUIRE_FALSE (anyPartialIn (f.directory));
    REQUIRE (f.model.bundlePath() == f.source);
    REQUIRE (f.model.hasUnsavedChanges());
    REQUIRE (readProject (f.source).tracks.size() == 2u);
    requireSameAssetFiles (f.source, copy);
    REQUIRE_FALSE (yesdaw::persistence::autosave_detail::anySnapshotSlotExists (copy));

    // The copy is closed: its folder can move while the source carries on (Windows refuses with a file open).
    const auto moved = f.directory / "moved.yesdaw";
    std::filesystem::rename (copy, moved);
    REQUIRE (renderReopened (moved, f.directory / "after.wav") == mix);

    REQUIRE (f.model.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (readProject (f.source).tracks.size() == 1u);
    REQUIRE (readProject (moved).tracks.size() == 2u);
    REQUIRE (f.model.saveProjectBundle().ok());
    REQUIRE_FALSE (f.model.hasUnsavedChanges());
}

TEST_CASE ("ADR-0060 a copy that fails mid-way leaves no target and no .partial folder; an occupied target is refused",
           "[project-lifecycle]")
{
    CopyFixture f ("copy-failure");
    const auto copy = f.directory / "copy.yesdaw";
    const auto corruptAudio = [] (const std::filesystem::path& partial) {   // the copy's Asset bytes, not the source's
        for (const auto& entry : std::filesystem::directory_iterator (partial / "audio"))
        {
            std::ofstream out (entry.path(), std::ios::binary | std::ios::trunc);
            out << "not the asset's bytes";
        }
    };
    const auto failedAs = f.model.saveProjectBundleAs (copy, corruptAudio);
    REQUIRE_FALSE (failedAs.dispatched);
    REQUIRE (std::string (failedAs.state.disabledReason) == "the copy did not validate");
    const auto failedCopy = f.model.saveProjectBundleCopy (copy, corruptAudio);
    REQUIRE_FALSE (failedCopy.dispatched);
    REQUIRE (std::string (failedCopy.state.disabledReason) == "the copy did not validate");
    REQUIRE_FALSE (std::filesystem::exists (copy));
    REQUIRE_FALSE (anyPartialIn (f.directory));
    REQUIRE (f.model.bundlePath() == f.source);
    REQUIRE (f.model.hasUnsavedChanges());
    REQUIRE (readProject (f.source).tracks.size() == 2u);   // opening it checks the source's Asset bytes too

    REQUIRE (f.model.addAudioTrack().dispatched);   // the source carries on
    REQUIRE (f.model.saveProjectBundle().ok());
    REQUIRE (readProject (f.source).tracks.size() == 3u);

    std::filesystem::create_directories (copy);
    {
        std::ofstream keep (copy / "keep.txt", std::ios::binary);
        keep << "keep";
    }
    REQUIRE (std::string (f.model.saveProjectBundleCopy (copy).state.disabledReason) == "the target already exists");
    REQUIRE (std::string (f.model.saveProjectBundleAs (copy).state.disabledReason) == "the target already exists");
    REQUIRE (bytesOf (copy / "keep.txt") == "keep");
    REQUIRE_FALSE (anyPartialIn (f.directory));
}

TEST_CASE ("ADR-0060 an export running through Save a Copy or Save As still completes", "[project-lifecycle]")
{
    CopyFixture f ("copy-export");
    yesdaw::engine::OfflineRenderLatch latch;
    latch.holdAfterFrames = 4'096;
    f.model.setExportLatchForTest (&latch);
    for (const bool saveAs : { false, true })
    {
        INFO (saveAs);
        latch.held.store (false);
        latch.released.store (false);
        const auto wav = f.directory / (saveAs ? "as.wav" : "copy.wav");
        const int exportsBefore = f.model.context().audioExportCount;
        REQUIRE (f.model.startAudioExport (wav).dispatched);
        for (int i = 0; i < 20'000 && ! latch.held.load(); ++i)
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        REQUIRE (latch.held.load());
        const auto target = f.directory / (saveAs ? "as.yesdaw" : "copy.yesdaw");
        REQUIRE ((saveAs ? f.model.saveProjectBundleAs (target) : f.model.saveProjectBundleCopy (target)).dispatched);
        REQUIRE (f.model.exportRunning());
        latch.released.store (true);
        f.model.waitForExport();
        REQUIRE (f.model.context().audioExportCount == exportsBefore + 1);
        REQUIRE (std::filesystem::exists (wav));
    }
    f.model.setExportLatchForTest (nullptr);
}

TEST_CASE ("ADR-0060 File > Save a Copy: the chooser's place gets the copy, this project stays current; a refusal says why",
           "[project-lifecycle]")
{
    LifecycleShell f ("menu-copy");
    f.newProject ("song", choicesOf (48'000.0, 120.0, 4, 4));
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::TrackAdd);
    f.saveACopyBundle = f.directory / "song copy.yesdaw";
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectSaveACopy);
    REQUIRE (readProject (f.directory / "song copy.yesdaw").tracks.size() == 2u);
    REQUIRE (yesdaw::ui::snapshotMainComponent (*f).bundlePath == f.directory / "song.yesdaw");
    REQUIRE (yesdaw::ui::snapshotMainComponent (*f).windowTitle.find ("copy") == std::string::npos);

    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectSaveACopy);   // now occupied
    REQUIRE (statusOf (*f).contains ("Save a Copy failed: the target already exists"));
}

// ---- cp3: templates ----

namespace {

yesdaw::ui::UiDecodedAsset decodeTone (const std::filesystem::path& path)
{
    yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (path);
    REQUIRE (decoded.decoded.has_value());
    return std::move (*decoded.decoded);
}

// The layout every cp3 gate builds by hand: two tracks, two buses; track 0 sends to bus 0, carries an EQ and a
// Compressor keyed by track 1 and plays into bus 1; bus 0 has a Delay, the master a Limiter; one marker.
void buildLayout (yesdaw::ui::UiAppModel& model)
{
    using yesdaw::engine::FxKind;
    REQUIRE (model.addAudioTrack().dispatched);
    REQUIRE (model.dispatch (UiActionId::MixerBusAdd).dispatched);
    REQUIRE (model.dispatch (UiActionId::MixerBusAdd).dispatched);
    REQUIRE (model.selectMixerTrack (0));
    REQUIRE (model.addSendOnSelectedTrack (0).dispatched);
    REQUIRE (model.addFxInsertToSelectedStrip (FxKind::Eq).dispatched);
    REQUIRE (model.addFxInsertToSelectedStrip (FxKind::Compressor).dispatched);
    REQUIRE (model.setFxInsertSidechainOnSelectedStrip (1, model.project().tracks.at (1).id).dispatched);
    REQUIRE (model.setOutputOnSelectedTrack (model.project().buses.at (1).id).dispatched);
    REQUIRE (model.selectMixerBus (0));
    REQUIRE (model.addFxInsertToSelectedStrip (FxKind::Delay).dispatched);
    REQUIRE (model.selectMixerMaster());
    REQUIRE (model.addFxInsertToSelectedStrip (FxKind::Limiter).dispatched);
    REQUIRE (model.addTimelineMarkerAtTick (1'920).dispatched);
}

// Content a template never carries: a clip and its Asset, a MIDI clip, an automation lane, punch, loop and scale.
void addContent (yesdaw::ui::UiAppModel& model, const std::filesystem::path& tone)
{
    REQUIRE (model.importAudioFileToTrack (tone, decodeTone (tone), model.project().tracks.at (0).id).ok());
    REQUIRE (model.addMidiClipOnTrackAt (model.project().tracks.at (1).id, 0).dispatched);
    REQUIRE (model.addAutomationBreakpointToTrackLane (model.project().tracks.at (0).id, 960, 0.5).dispatched);   // makes the lane
    REQUIRE (model.setPunchRegion (true, 0, 48'000).dispatched);
    REQUIRE (model.setProjectScale (2, 1, true).dispatched);
    REQUIRE (model.setPlaybackLoopRegion (0, 96'000).dispatched);
}

// A tone on track 0, exported as float: what the project's routing makes of it.
std::vector<float> renderTone (yesdaw::ui::UiAppModel& model, const std::filesystem::path& tone, const std::filesystem::path& wav)
{
    REQUIRE (model.importAudioFileToTrack (tone, decodeTone (tone), model.project().tracks.at (0).id).ok());
    model.setExportBitDepth (yesdaw::ui::UiAppModel::UiExportBitDepth::Float32);
    REQUIRE (model.exportAudioFile (wav).dispatched);
    yesdaw::io::Float32Wav read;
    REQUIRE (yesdaw::io::readFloat32WavFile (wav, read).ok());
    return read.interleavedSamples;
}

} // namespace

TEST_CASE ("ADR-0060 a template holds exactly the layout and none of the content", "[project-lifecycle]")
{
    const auto directory = lifecycleScratch ("template-layout");
    writeTone (directory / "tone.wav", 48'000, 0.25f);
    yesdaw::ui::UiAppModel model;
    model.setSessionStateDirectory (directory / "session");
    REQUIRE (model.createProjectBundle (directory / "song.yesdaw").ok());
    buildLayout (model);
    addContent (model, directory / "tone.wav");
    const yesdaw::engine::Project source = model.project();
    REQUIRE_FALSE (source.clips.empty());
    REQUIRE_FALSE (source.midiClips.empty());
    REQUIRE_FALSE (source.automationLanes.empty());
    REQUIRE_FALSE (source.assets.empty());

    REQUIRE (model.saveProjectAsTemplate ("Band: Setup", false).dispatched);
    const auto templates = directory / "session" / "templates";
    const auto bundle = templates / "Band_ Setup.yesdaw";   // ADR-0058's file-safe rule
    REQUIRE (std::filesystem::is_directory (bundle));
    REQUIRE_FALSE (anyPartialIn (templates));
    const yesdaw::engine::Project layout = readProject (bundle);

    // Kept: the tracks, buses, master, rate, tempo, meter and markers, exactly.
    REQUIRE (layout.tracks.size() == source.tracks.size());
    for (std::size_t i = 0; i < source.tracks.size(); ++i)
    {
        REQUIRE (layout.tracks[i].id == source.tracks[i].id);
        REQUIRE (layout.tracks[i].strip == source.tracks[i].strip);
        REQUIRE (layout.tracks[i].sends == source.tracks[i].sends);
        REQUIRE (layout.tracks[i].outputBusId == source.tracks[i].outputBusId);
        REQUIRE (layout.tracks[i].instrumentKind == source.tracks[i].instrumentKind);
    }
    REQUIRE (layout.buses == source.buses);
    REQUIRE (layout.masterStrip == source.masterStrip);
    REQUIRE (layout.masterLinearGain == source.masterLinearGain);
    REQUIRE (layout.markers == source.markers);
    REQUIRE (layout.sampleRate.hz == source.sampleRate.hz);
    REQUIRE (layout.tempoMap.size() == source.tempoMap.size());
    REQUIRE (layout.meterMap.size() == source.meterMap.size());

    // Dropped: every piece of content.
    const yesdaw::engine::Project empty;
    REQUIRE (layout.clips.empty());
    REQUIRE (layout.midiClips.empty());
    REQUIRE (layout.assets.empty());
    REQUIRE (layout.recordingTakes.empty());
    REQUIRE (layout.recordingCompSegments.empty());
    REQUIRE (layout.automationLanes.empty());
    REQUIRE (layout.punchRegion == empty.punchRegion);
    REQUIRE (layout.loopRegion == empty.loopRegion);
    REQUIRE (layout.scale == empty.scale);
    REQUIRE (layout.automationMode == empty.automationMode);
    REQUIRE (std::filesystem::is_empty (bundle / "audio"));

    // A Sampler keeps its kind and loses its pads; nothing stays soloed; locate points go.
    yesdaw::engine::Project withPads = source;
    withPads.tracks.at (1).instrumentKind = yesdaw::engine::TrackInstrumentKind::Sampler;
    yesdaw::engine::SamplerPad pad;
    pad.key = 60;
    pad.assetId = source.assets.at (0).id;
    withPads.tracks.at (1).samplerPads.push_back (pad);
    withPads.tracks.at (0).strip.soloed = true;
    withPads.locatePoints[0] = 480;
    const yesdaw::engine::Project padless = yesdaw::ui::UiAppModel::makeTemplateLayout (withPads);
    REQUIRE (padless.tracks.at (1).instrumentKind == yesdaw::engine::TrackInstrumentKind::Sampler);
    REQUIRE (padless.tracks.at (1).samplerPads.empty());
    REQUIRE_FALSE (padless.tracks.at (0).strip.soloed);
    REQUIRE_FALSE (padless.locatePoints[0].has_value());
    REQUIRE (yesdaw::ui::UiAppModel::isTemplateLayout (padless));
    REQUIRE_FALSE (yesdaw::ui::UiAppModel::isTemplateLayout (withPads));
    const yesdaw::engine::Project fromPadless = yesdaw::ui::UiAppModel::instantiateTemplate (padless);
    REQUIRE (fromPadless.tracks.at (1).instrumentKind == yesdaw::engine::TrackInstrumentKind::Sampler);
    REQUIRE (fromPadless.tracks.at (1).samplerPads.empty());

    // A bundle put in the templates folder by hand brings only its layout into a new project.
    withPads.loopRegion = yesdaw::engine::LoopRegion { true, 0, 96'000 };
    withPads.automationMode = yesdaw::engine::AutomationMode::Latch;
    const yesdaw::engine::Project fromFull = yesdaw::ui::UiAppModel::instantiateTemplate (withPads);
    REQUIRE (fromFull.clips.empty());
    REQUIRE (fromFull.assets.empty());
    REQUIRE (fromFull.automationLanes.empty());
    REQUIRE (fromFull.tracks.at (1).samplerPads.empty());
    REQUIRE (fromFull.loopRegion == empty.loopRegion);
    REQUIRE (fromFull.punchRegion == empty.punchRegion);
    REQUIRE (fromFull.scale == empty.scale);
    REQUIRE (fromFull.automationMode == empty.automationMode);
    REQUIRE_FALSE (fromFull.locatePoints[0].has_value());
    REQUIRE_FALSE (fromFull.tracks.at (0).strip.soloed);
}

TEST_CASE ("ADR-0060 New from a template: its layout with fresh identities; a tone renders as through a hand-built twin",
           "[project-lifecycle]")
{
    LifecycleShell f ("from-template");
    writeTone (f.directory / "tone.wav", 48'000, 0.25f);
    yesdaw::engine::Project templateLayout;
    {
        yesdaw::ui::UiAppModel source;   // shares the shell's session folder, so the shell sees its templates
        source.setSessionStateDirectory (f.directory / "session");
        REQUIRE (source.createProjectBundle (f.directory / "source.yesdaw").ok());
        buildLayout (source);
        addContent (source, f.directory / "tone.wav");
        REQUIRE (source.saveProjectAsTemplate ("Band", false).dispatched);
        templateLayout = readProject (f.directory / "session" / "templates" / "Band.yesdaw");
    }
    UiNewProjectChoices choices = choicesOf (48'000.0, 120.0, 4, 4);
    choices.templateName = "Band";
    f.newProject ("song", choices);
    f.shell.reset();
    const yesdaw::engine::Project project = readProject (f.directory / "song.yesdaw");
    REQUIRE (project.tracks.size() == templateLayout.tracks.size());
    REQUIRE (project.buses.size() == templateLayout.buses.size());
    REQUIRE (project.markers.size() == templateLayout.markers.size());
    REQUIRE (project.clips.empty());

    // Fresh identities: nothing in the project carries an ID of the template.
    const auto requireFresh = [&templateLayout] (yesdaw::engine::EntityId id) {
        REQUIRE (id.isValid());
        REQUIRE_FALSE (yesdaw::engine::detail::projectContainsEntityId (templateLayout, id));
    };
    const auto requireFreshStrip = [&requireFresh] (const yesdaw::engine::MixerStripState& strip) {
        for (const yesdaw::engine::FxInsert& insert : strip.fxChain)
            requireFresh (insert.id);
    };
    requireFresh (project.id);
    for (const yesdaw::engine::Track& track : project.tracks)
    {
        requireFresh (track.id);
        requireFreshStrip (track.strip);
        for (const yesdaw::engine::SendRow& send : track.sends)
            requireFresh (send.id);
    }
    for (const yesdaw::engine::Bus& bus : project.buses)
    {
        requireFresh (bus.id);
        requireFreshStrip (bus.strip);
        for (const yesdaw::engine::SendRow& send : bus.sends)
            requireFresh (send.id);
    }
    requireFreshStrip (project.masterStrip);
    for (const yesdaw::engine::Marker& marker : project.markers)
        requireFresh (marker.id);

    // The routing, rewired through the one map.
    REQUIRE (project.tracks.at (0).outputBusId == project.buses.at (1).id);
    REQUIRE (project.tracks.at (0).sends.at (0).busId == project.buses.at (0).id);
    REQUIRE (project.tracks.at (0).strip.fxChain.at (1).sidechainSourceId == project.tracks.at (1).id);
    REQUIRE (project.masterStrip.fxChain.size() == templateLayout.masterStrip.fxChain.size());

    // A test tone through the new project renders as through a hand-built twin of the layout.
    yesdaw::ui::UiAppModel fromTemplate;
    REQUIRE (fromTemplate.openProjectBundle (f.directory / "song.yesdaw").ok());
    yesdaw::ui::UiAppModel twin;
    REQUIRE (twin.createProjectBundle (f.directory / "twin.yesdaw").ok());
    buildLayout (twin);
    const std::vector<float> rendered = renderTone (fromTemplate, f.directory / "tone.wav", f.directory / "from-template.wav");
    const std::vector<float> expected = renderTone (twin, f.directory / "tone.wav", f.directory / "twin.wav");
    REQUIRE (rendered.size() == expected.size());
    float peak = 0.0f;
    float worst = 0.0f;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        peak = std::max (peak, std::abs (expected[i]));
        worst = std::max (worst, std::abs (rendered[i] - expected[i]));
    }
    REQUIRE (peak > 0.01f);
    REQUIRE (worst <= 1.0e-6f);
}

TEST_CASE ("ADR-0060 the dialog lists the templates folder: Default first, a refused one with its reason and not "
           "choosable; choosing one sets the rate, tempo and meter",
           "[project-lifecycle]")
{
    LifecycleShell f ("template-list", true);
    const auto templates = f.directory / "session" / "templates";
    {
        yesdaw::ui::UiAppModel maker;
        maker.setSessionStateDirectory (f.directory / "session");
        REQUIRE (maker.createProjectBundle (f.directory / "cd.yesdaw",
                                            yesdaw::ui::UiAppModel::makeNewSessionProject (choicesOf (44'100.0, 96.0, 7, 8))).ok());
        REQUIRE (maker.saveProjectAsTemplate ("Band", false).dispatched);
        // A bundle with content dropped into the folder by hand, and one that does not open.
        writeTone (f.directory / "tone.wav", 48'000, 0.25f);
        REQUIRE (maker.createProjectBundle (templates / "Full.yesdaw").ok());
        REQUIRE (maker.importAudioFile (f.directory / "tone.wav", decodeTone (f.directory / "tone.wav")).ok());
    }
    std::filesystem::create_directories (templates / "Broken.yesdaw");
    {
        std::ofstream garbage (templates / "Broken.yesdaw" / "project.db", std::ios::binary);
        garbage << "not a database";
    }
    std::filesystem::create_directories (templates / "Band.yesdaw.1.partial");   // never listed

    f.nextBundle = f.directory / "keyed.yesdaw";
    REQUIRE (press (*f, 'n', juce::ModifierKeys::ctrlModifier));
    const auto items = yesdaw::ui::mainComponentNewProjectDialogTemplateItems (*f);
    REQUIRE (items.size() == 4u);
    REQUIRE (items[0] == std::pair<std::string, bool> { "Default (one audio track)", true });
    REQUIRE (items[1] == std::pair<std::string, bool> { "Band", true });
    REQUIRE (items[2].first.rfind ("Broken - cannot be used: ", 0) == 0);
    REQUIRE_FALSE (items[2].second);
    REQUIRE (items[3].first == "Full - cannot be used: it holds content (clips, audio or automation)");
    REQUIRE_FALSE (items[3].second);
    REQUIRE (yesdaw::ui::mainComponentNewProjectDialogChoices (*f).sampleRateHz == 48'000.0);

    // The keyboard picks Band: its rate, tempo and meter fill the dialog. Down from Band skips the refused ones.
    const auto order = yesdaw::ui::mainComponentControlTraversal (*f);
    REQUIRE (press (*f, juce::KeyPress::tabKey));
    for (std::size_t i = 0; i < order.size() && yesdaw::ui::mainComponentControlTarget (*f).id != "newproject.template"; ++i)
        REQUIRE (press (*f, juce::KeyPress::tabKey));
    REQUIRE (yesdaw::ui::mainComponentControlTarget (*f).id == "newproject.template");
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    REQUIRE (press (*f, juce::KeyPress::downKey));
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    UiNewProjectChoices shown = yesdaw::ui::mainComponentNewProjectDialogChoices (*f);
    REQUIRE (shown.templateName == "Band");
    REQUIRE (shown.sampleRateHz == 44'100.0);
    REQUIRE (shown.bpm == 96.0);
    REQUIRE (shown.meterNumerator == 7u);
    REQUIRE (shown.meterDenominator == 8u);
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    REQUIRE (press (*f, juce::KeyPress::downKey));
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    REQUIRE (yesdaw::ui::mainComponentNewProjectDialogChoices (*f).templateName == "Band");

    for (std::size_t i = 0; i < order.size() && yesdaw::ui::mainComponentControlTarget (*f).id != "newproject.create"; ++i)
        REQUIRE (press (*f, juce::KeyPress::tabKey));
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    (void) juce::MessageManager::getInstance()->runDispatchLoopUntil (50);   // the button's click is posted
    const yesdaw::engine::Project created = readProject (f.directory / "keyed.yesdaw");
    REQUIRE (created.sampleRate.hz == 44'100.0);
    REQUIRE (created.meterMap.front().numerator == 7u);
}

TEST_CASE ("ADR-0060 Save as Template asks before replacing a template; a template that cannot be used refuses New",
           "[project-lifecycle]")
{
    LifecycleShell f ("template-replace");
    f.newProject ("song", choicesOf (48'000.0, 120.0, 4, 4));
    const auto bundle = f.directory / "session" / "templates" / "Mine.yesdaw";
    f.templateName = "Mine";
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectSaveAsTemplate);
    REQUIRE (readProject (bundle).tracks.size() == 1u);
    REQUIRE (f.replaceAsks == 0);

    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::TrackAdd);
    f.replaceTemplate = false;   // keep it
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectSaveAsTemplate);
    REQUIRE (f.replaceAsks == 1);
    REQUIRE (readProject (bundle).tracks.size() == 1u);
    f.replaceTemplate = true;    // replace it
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectSaveAsTemplate);
    REQUIRE (f.replaceAsks == 2);
    REQUIRE (readProject (bundle).tracks.size() == 2u);
    REQUIRE_FALSE (anyPartialIn (f.directory / "session" / "templates"));
    REQUIRE_FALSE (std::filesystem::exists (f.directory / "session" / "templates" / "Mine.yesdaw.replaced"));

    f.templateName.clear();   // a cancelled name writes nothing
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectSaveAsTemplate);
    REQUIRE (f.replaceAsks == 2);

    // Two typed names that share one file name: the question names the template it would replace.
    f.templateName = "A?B";
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectSaveAsTemplate);
    f.templateName = "A/B";
    f.replaceTemplate = false;
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::ProjectSaveAsTemplate);
    REQUIRE (f.replaceAsks == 3);
    REQUIRE (f.replaceAskedFor == "A_B");

    std::filesystem::create_directories (f.directory / "session" / "templates" / "Broken.yesdaw");
    UiNewProjectChoices broken = choicesOf (48'000.0, 120.0, 4, 4);
    broken.templateName = "Broken";
    f.newProject ("never", broken);
    REQUIRE (statusOf (*f).contains ("New project refused: the template Broken cannot be used"));
    REQUIRE_FALSE (std::filesystem::exists (f.directory / "never.yesdaw"));
}
