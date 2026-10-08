// YES DAW - ADR-0056 gates (cp1): the media browser.
//
// The browser is a dock tab (Y) with three sources (Files, Project, Recent). Keeping a file imports it exactly as
// Ctrl+Shift+I does; a drag of rows onto the lanes lands exactly as an OS drop of the same files; a project Asset
// is placed as a new clip on that Asset (no copy) in one undo step; refusals carry their reasons; Files lists "..",
// folders, then supported files only (non-ASCII names included); Recent and the browser's state live in the
// session folder; the Control target browses with the keyboard alone; only painted rows read a file header.

#include "io/AudioFileDecode.h"
#include "io/WavFile.h"
#include "persistence/ProjectBundle.h"
#include "ui/MainComponent.h"
#include "ui/BrowserPanelComponent.h"
#include "ui/MainComponentInternal.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using yesdaw::ui::MainComponentFileChoices;
using yesdaw::ui::UiActionId;

namespace {

std::filesystem::path browserScratch (const std::string& label)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-browser-" + label + "-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

void writeTone (const std::filesystem::path& path, std::size_t frames, float level, double rate = 48'000.0)
{
    std::vector<float> samples (frames);
    for (std::size_t i = 0; i < frames; ++i)
        samples[i] = level * static_cast<float> ((i % 64u) < 32u ? 1.0 : -1.0);
    REQUIRE (yesdaw::io::writeFloat32WavFile (path, yesdaw::engine::SampleRate { rate }, 1, samples.size(), samples).ok());
}

yesdaw::engine::Project readProject (const std::filesystem::path& bundlePath)
{
    yesdaw::persistence::ProjectBundleDb db;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::openExistingBundle (bundlePath, db).ok());
    yesdaw::engine::Project project;
    REQUIRE (db.readProjectSnapshot (project).ok());
    return project;
}

juce::String fromU8 (const char8_t* text)
{
    return juce::String::fromUTF8 (reinterpret_cast<const char*> (text));
}

juce::Component* findById (juce::Component& component, const juce::String& id)
{
    if (component.getComponentID() == id)
        return &component;
    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (juce::Component* found = findById (*component.getChildComponent (i), id))
            return found;
    return nullptr;
}

juce::var probeOf (juce::Component& shell)
{
    return juce::JSON::parse (juce::String (yesdaw::ui::mainComponentStateProbeJson (shell)));
}

juce::String statusOf (juce::Component& shell)
{
    return probeOf (shell)["status"]["text"].toString();
}

juce::Rectangle<int> layoutRect (juce::Component& shell, const juce::String& key)
{
    const juce::var rect = probeOf (shell)["layout"][juce::Identifier (key)];
    REQUIRE (rect.isArray());
    return { static_cast<int> (rect[0]), static_cast<int> (rect[1]), static_cast<int> (rect[2]), static_cast<int> (rect[3]) };
}

bool press (juce::Component& shell, int keyCode, juce::ModifierKeys mods = {})
{
    return shell.keyPressed (juce::KeyPress (keyCode, mods, 0));
}

// A shell with a fresh project and its own session folder; `importPick` feeds Ctrl+Shift+I.
struct BrowserShell
{
    std::filesystem::path bundle;
    std::filesystem::path session;
    std::filesystem::path importPick;
    std::unique_ptr<juce::Component> shell;

    BrowserShell (const std::filesystem::path& directory, const std::string& name, std::filesystem::path sessionFolder = {})
        : bundle (directory / (name + ".yesdaw")),
          session (sessionFolder.empty() ? directory / (name + "-session") : std::move (sessionFolder))
    {
        std::filesystem::create_directories (session);
        juce::MessageManager::getInstance();
        MainComponentFileChoices choices;
        choices.sessionStateDirectory = session;
        choices.chooseNewProjectBundle = [this] { return bundle; };
        choices.chooseImportAudioFile = [this] { return importPick; };
        shell = yesdaw::ui::createMainComponent (std::move (choices));
        REQUIRE (shell != nullptr);
        yesdaw::ui::mainComponentDispatchAction (*shell, UiActionId::ProjectNew);
        REQUIRE (std::filesystem::exists (bundle));
    }

    juce::Component& operator*() { return *shell; }
    yesdaw::engine::Project project() const { return readProject (bundle); }
    yesdaw::ui::MainComponentBrowser browser() { return yesdaw::ui::mainComponentBrowser (*shell); }

    void showBrowser()
    {
        if (! browser().visible)
            REQUIRE (press (*shell, 'y'));
        REQUIRE (browser().visible);
    }

    int rowNamed (const juce::String& name)
    {
        const auto snapshot = browser();
        for (std::size_t i = 0; i < snapshot.names.size(); ++i)
            if (snapshot.names[i] == name)
                return static_cast<int> (i);
        FAIL ("no browser row named " << name);
        return -1;
    }
};

struct LandedClip
{
    std::size_t lane = 0;
    yesdaw::engine::Tick start = 0;
    yesdaw::engine::Tick length = 0;
    yesdaw::engine::AssetContentHash hash;
    std::uint64_t frames = 0;
    double rate = 0.0;
    std::uint16_t channels = 0;
    friend bool operator== (const LandedClip&, const LandedClip&) = default;
};

std::vector<LandedClip> landedClips (const yesdaw::engine::Project& project)
{
    std::vector<LandedClip> out;
    for (const yesdaw::engine::Clip& clip : project.clips)
    {
        LandedClip landed;
        for (std::size_t lane = 0; lane < project.tracks.size(); ++lane)
            if (project.tracks[lane].id == clip.trackId)
                landed.lane = lane;
        landed.start = clip.timelineStart;
        landed.length = clip.timelineLength;
        const yesdaw::engine::Asset* asset = project.findAsset (clip.assetId);
        REQUIRE (asset != nullptr);
        landed.hash = asset->contentHash;
        landed.frames = asset->frames;
        landed.rate = asset->sampleRate.hz;
        landed.channels = asset->channels;
        out.push_back (landed);
    }
    std::sort (out.begin(), out.end(), [] (const LandedClip& a, const LandedClip& b) { return a.lane < b.lane; });
    return out;
}

} // namespace

TEST_CASE ("ADR-0056 Y shows the Browser tab; a kept file lands as Ctrl+Shift+I lands it", "[browser]")
{
    const auto directory = browserScratch ("keep");
    const auto media = directory / "media";
    std::filesystem::create_directories (media);
    writeTone (media / "kick.wav", 4'800, 0.5f);

    // Through the browser: Y, open the folder, select the file, Import.
    BrowserShell viaBrowser (directory, "browser");
    viaBrowser.showBrowser();
    REQUIRE (probeOf (*viaBrowser)["view"]["dock"].toString() == "Browser");
    yesdaw::ui::mainComponentBrowserOpenFolder (*viaBrowser, media);
    REQUIRE (viaBrowser.browser().source == "Files");
    yesdaw::ui::mainComponentBrowserSelect (*viaBrowser, viaBrowser.rowNamed ("kick.wav"));
    yesdaw::ui::mainComponentBrowserImport (*viaBrowser);

    // Through Ctrl+Shift+I with the same file.
    BrowserShell viaChord (directory, "chord");
    viaChord.importPick = media / "kick.wav";
    REQUIRE (press (*viaChord, 'i', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier));

    const auto browserClips = landedClips (viaBrowser.project());
    REQUIRE (browserClips.size() == 1u);
    REQUIRE (browserClips == landedClips (viaChord.project()));

    // A double-click on the row is the same keep.
    yesdaw::ui::mainComponentBrowserDoubleClick (*viaBrowser, viaBrowser.rowNamed ("kick.wav"));
    REQUIRE (viaBrowser.project().clips.size() == 2u);

    // Y again hides it; the dock tab is view state only.
    REQUIRE (press (*viaBrowser, 'y'));
    REQUIRE_FALSE (viaBrowser.browser().visible);
}

TEST_CASE ("ADR-0056 a browser drag of two rows lands exactly as an OS drop of the same files, in one undo step", "[browser]")
{
    const auto directory = browserScratch ("drag");
    const auto media = directory / "media";
    std::filesystem::create_directories (media);
    writeTone (media / "a.wav", 9'600, 0.4f);
    writeTone (media / "b.wav", 4'800, 0.3f);

    BrowserShell viaBrowser (directory, "browser");
    viaBrowser.showBrowser();
    yesdaw::ui::mainComponentBrowserOpenFolder (*viaBrowser, media);
    yesdaw::ui::mainComponentBrowserSelect (*viaBrowser, viaBrowser.rowNamed ("a.wav"));
    yesdaw::ui::mainComponentBrowserSelect (*viaBrowser, viaBrowser.rowNamed ("b.wav"), true, false);   // Ctrl+click adds
    const juce::Rectangle<int> lane = layoutRect (*viaBrowser, "lane.0");
    const juce::Point<int> releaseAt { lane.getX() + lane.getWidth() / 3, lane.getCentreY() };
    yesdaw::ui::mainComponentBrowserDragSelectedTo (*viaBrowser, releaseAt);

    BrowserShell viaDrop (directory, "drop");
    juce::Component* timeline = findById (*viaDrop, yesdaw::ui::shell::kTimelineComponentId);
    REQUIRE (timeline != nullptr);
    auto* dropTarget = dynamic_cast<juce::FileDragAndDropTarget*> (timeline);
    REQUIRE (dropTarget != nullptr);
    const juce::Point<int> local = timeline->getLocalPoint (viaDrop.shell.get(), releaseAt);
    dropTarget->filesDropped (juce::StringArray { juce::String ((media / "a.wav").string()), juce::String ((media / "b.wav").string()) },
                              local.x, local.y);

    const yesdaw::engine::Project dragged = viaBrowser.project();
    REQUIRE (dragged.tracks.size() == 2u);   // the second file made the track it needed
    REQUIRE (landedClips (dragged).size() == 2u);
    REQUIRE (landedClips (dragged) == landedClips (viaDrop.project()));
    REQUIRE (landedClips (dragged)[0].start > 0);

    // One undo takes both clips and the track the drag made.
    REQUIRE (press (*viaBrowser, 'z', juce::ModifierKeys::ctrlModifier));
    const yesdaw::engine::Project undone = viaBrowser.project();
    REQUIRE (undone.clips.empty());
    REQUIRE (undone.tracks.size() == 1u);

    // A drag released below the last track starts a new track there.
    yesdaw::ui::mainComponentDispatchAction (*viaBrowser, UiActionId::EditRedo);
    REQUIRE (viaBrowser.project().tracks.size() == 2u);
    const juce::Rectangle<int> lastLane = layoutRect (*viaBrowser, "lane.1");
    yesdaw::ui::mainComponentBrowserSelect (*viaBrowser, viaBrowser.rowNamed ("b.wav"));
    yesdaw::ui::mainComponentBrowserDragSelectedTo (*viaBrowser, { releaseAt.x, lastLane.getBottom() + lastLane.getHeight() / 2 });
    const yesdaw::engine::Project below = viaBrowser.project();
    REQUIRE (below.tracks.size() == 3u);
    REQUIRE (landedClips (below).back().lane == 2u);
}

TEST_CASE ("ADR-0056 a project Asset placed from the browser is a new clip on that Asset, no new Asset, one undo step",
           "[browser]")
{
    const auto directory = browserScratch ("asset");
    writeTone (directory / "loop.wav", 12'000, 0.25f);
    BrowserShell f (directory, "asset");
    f.importPick = directory / "loop.wav";
    REQUIRE (press (*f, 'i', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier));
    const yesdaw::engine::Project imported = f.project();
    REQUIRE (imported.assets.size() == 1u);
    REQUIRE (imported.clips.size() == 1u);

    f.showBrowser();
    yesdaw::ui::mainComponentBrowserSetSource (*f, 1);   // Project
    auto snapshot = f.browser();
    REQUIRE (snapshot.source == "Project");
    REQUIRE (snapshot.names.size() == 1u);
    REQUIRE (snapshot.kinds[0] == "asset");
    REQUIRE (snapshot.names[0] == juce::String::fromUTF8 (imported.clips[0].name.c_str()));   // named by its clip
    REQUIRE (snapshot.facts[0].contains ("48 kHz"));
    REQUIRE (snapshot.facts[0].contains ("mono"));

    yesdaw::ui::mainComponentBrowserSelect (*f, 0);
    yesdaw::ui::mainComponentBrowserImport (*f);
    const yesdaw::engine::Project placed = f.project();
    REQUIRE (placed.assets.size() == 1u);   // no copy
    REQUIRE (placed.clips.size() == 2u);
    REQUIRE (placed.clips[1].assetId == imported.assets[0].id);
    REQUIRE (placed.clips[1].srcLen == imported.clips[0].srcLen);

    // A drag of the Asset onto the lanes places it too (at the snapped point, through placeAssetsAt).
    const juce::Rectangle<int> lane = layoutRect (*f, "lane.0");
    yesdaw::ui::mainComponentBrowserDragSelectedTo (*f, { lane.getRight() - 40, lane.getCentreY() });
    REQUIRE (f.project().clips.size() == 3u);
    REQUIRE (f.project().assets.size() == 1u);

    // One undo each.
    REQUIRE (press (*f, 'z', juce::ModifierKeys::ctrlModifier));
    REQUIRE (f.project().clips.size() == 2u);
    REQUIRE (press (*f, 'z', juce::ModifierKeys::ctrlModifier));
    REQUIRE (f.project().clips.size() == 1u);
    REQUIRE (f.project().assets.size() == 1u);

    // The UI tick rebuilds the Project rows only when the Assets or their names change; a rebuild keeps the
    // selected rows (by Asset) and the scroll.
    writeTone (directory / "second.wav", 6'000, 0.2f);
    writeTone (directory / "third.wav", 3'000, 0.2f);
    for (const char* name : { "second.wav", "third.wav" })
    {
        f.importPick = directory / name;
        REQUIRE (press (*f, 'i', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier));
    }
    REQUIRE (f.browser().names.size() == 3u);   // the other surface's imports showed up
    yesdaw::ui::mainComponentBrowserSelect (*f, 0);
    yesdaw::ui::mainComponentBrowserSelect (*f, 2, true, false);
    REQUIRE (f.browser().marked == std::vector<int> { 0, 2 });
    yesdaw::ui::mainComponentDispatchAction (*f, UiActionId::TransportToggleMetronome);   // not an Asset change
    REQUIRE (f.browser().marked == std::vector<int> { 0, 2 });
    writeTone (directory / "fourth.wav", 1'500, 0.2f);
    f.importPick = directory / "fourth.wav";
    REQUIRE (press (*f, 'i', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier));
    auto rebuilt = f.browser();
    REQUIRE (rebuilt.names.size() == 4u);
    REQUIRE (rebuilt.marked == std::vector<int> { 0, 2 });   // the same two Assets, still selected
    REQUIRE (rebuilt.selected == 2);
}

TEST_CASE ("ADR-0056 an unreadable and a missing file show their reasons in the row and on the status line, and change nothing",
           "[browser]")
{
    const auto directory = browserScratch ("refusals");
    const auto media = directory / "media";
    std::filesystem::create_directories (media);
    {
        std::ofstream junk (media / "junk.mp3", std::ios::binary);
        junk << std::string (2'048, '\0');
    }
    writeTone (media / "gone.wav", 4'800, 0.5f);

    BrowserShell f (directory, "refusals");
    f.importPick = media / "gone.wav";
    REQUIRE (press (*f, 'i', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier));   // into Recent
    REQUIRE (f.project().clips.size() == 1u);
    std::filesystem::remove (media / "gone.wav");

    f.showBrowser();
    yesdaw::ui::mainComponentBrowserOpenFolder (*f, media);
    yesdaw::ui::mainComponentBrowserPaint (*f);
    auto snapshot = f.browser();
    const int junkRow = f.rowNamed ("junk.mp3");
    INFO (snapshot.reasons[static_cast<std::size_t> (junkRow)]);
    REQUIRE (snapshot.reasons[static_cast<std::size_t> (junkRow)].contains ("not a readable MP3 file"));
    const yesdaw::engine::Project before = f.project();
    yesdaw::ui::mainComponentBrowserSelect (*f, junkRow);
    REQUIRE (f.browser().description.contains ("not a readable MP3 file"));
    yesdaw::ui::mainComponentBrowserImport (*f);
    REQUIRE (statusOf (*f).contains ("junk.mp3"));
    REQUIRE (statusOf (*f).contains ("not a readable MP3 file"));
    REQUIRE (f.project().clips.size() == before.clips.size());
    REQUIRE (f.project().assets.size() == before.assets.size());

    yesdaw::ui::mainComponentBrowserSetSource (*f, 2);   // Recent: the file that is gone
    yesdaw::ui::mainComponentBrowserPaint (*f);
    snapshot = f.browser();
    REQUIRE (snapshot.names.size() == 1u);
    REQUIRE (snapshot.names[0] == "gone.wav");
    REQUIRE (snapshot.reasons[0] == "missing");
    yesdaw::ui::mainComponentBrowserSelect (*f, 0);
    yesdaw::ui::mainComponentBrowserImport (*f);
    REQUIRE (statusOf (*f).contains ("gone.wav"));
    REQUIRE (f.project().clips.size() == before.clips.size());
}

TEST_CASE ("ADR-0056 Files lists .., folders, then supported files only; non-ASCII folders and files work", "[browser]")
{
    const auto directory = browserScratch ("listing");
    const auto media = directory / "media";
    const std::filesystem::path accented = media / std::filesystem::path (u8"\u00dcn\u00efc\u00f8d\u00e9 \u97f3");
    std::filesystem::create_directories (media / "Zeta");
    std::filesystem::create_directories (media / "alpha");
    std::filesystem::create_directories (accented);
    writeTone (media / "b.wav", 480, 0.1f);
    writeTone (media / "A.wav", 480, 0.1f);
    writeTone (media / ".hidden.wav", 480, 0.1f);
    {
        std::ofstream notes (media / "notes.txt");
        notes << "not audio";
    }
    const std::filesystem::path accentedFile = accented / std::filesystem::path (u8"se\u00f1al.wav");
    writeTone (accentedFile, 2'400, 0.2f);

    BrowserShell f (directory, "listing");
    f.showBrowser();
    yesdaw::ui::mainComponentBrowserOpenFolder (*f, media);
    auto snapshot = f.browser();
    const std::vector<juce::String> expected { "..", "alpha", "Zeta", fromU8 (u8"\u00dcn\u00efc\u00f8d\u00e9 \u97f3"),
                                               "A.wav", "b.wav" };
    REQUIRE (snapshot.names == expected);
    REQUIRE (snapshot.kinds[0] == "parent");
    REQUIRE (snapshot.kinds[1] == "folder");
    REQUIRE (snapshot.kinds[4] == "file");

    // Open the non-ASCII folder by keeping its row; import the non-ASCII file in it.
    yesdaw::ui::mainComponentBrowserDoubleClick (*f, 3);
    snapshot = f.browser();
    REQUIRE (snapshot.location.contains (fromU8 (u8"\u97f3")));
    REQUIRE (snapshot.names == std::vector<juce::String> { "..", fromU8 (u8"se\u00f1al.wav") });
    yesdaw::ui::mainComponentBrowserPaint (*f);
    REQUIRE (f.browser().facts[1].contains ("48 kHz"));
    yesdaw::ui::mainComponentBrowserDoubleClick (*f, 1);
    REQUIRE (f.project().clips.size() == 1u);

    // Up (and the ".." row) go back to the parent.
    yesdaw::ui::mainComponentBrowserDoubleClick (*f, 0);
    REQUIRE (f.browser().names == expected);
}

TEST_CASE ("ADR-0056 Recent holds imports newest first and survives a new shell; so do the source and folder", "[browser]")
{
    const auto directory = browserScratch ("recent");
    const auto media = directory / "media";
    std::filesystem::create_directories (media);
    writeTone (media / "first.wav", 480, 0.1f);
    writeTone (media / "second.wav", 480, 0.2f);
    const auto session = directory / "shared-session";
    {
        BrowserShell f (directory, "one", session);
        f.importPick = media / "first.wav";
        REQUIRE (press (*f, 'i', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier));
        f.showBrowser();
        yesdaw::ui::mainComponentBrowserOpenFolder (*f, media);
        yesdaw::ui::mainComponentBrowserSelect (*f, f.rowNamed ("second.wav"));
        yesdaw::ui::mainComponentBrowserImport (*f);   // a browser import is a Recent entry too
        yesdaw::ui::mainComponentBrowserSetSource (*f, 2);
        REQUIRE (f.browser().names == std::vector<juce::String> { "second.wav", "first.wav" });
        // Re-importing the older one moves it to the top, once.
        f.importPick = media / "first.wav";
        REQUIRE (press (*f, 'i', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier));
        yesdaw::ui::mainComponentBrowserSetSource (*f, 2);
        REQUIRE (f.browser().names == std::vector<juce::String> { "first.wav", "second.wav" });
        for (const auto& entry : std::filesystem::directory_iterator (session))
            REQUIRE (entry.path().extension() != ".tmp");   // the record's temporary never stays behind
    }
    {
        BrowserShell g (directory, "two", session);   // a new shell reading the same session folder
        g.showBrowser();
        auto snapshot = g.browser();
        REQUIRE (snapshot.source == "Recent");
        REQUIRE (snapshot.names == std::vector<juce::String> { "first.wav", "second.wav" });
        yesdaw::ui::mainComponentBrowserSetSource (*g, 0);
        REQUIRE (g.browser().location == juce::String (media.string()));
    }
}

TEST_CASE ("ADR-0056 the keyboard alone browses: Tab to the list, Enter, arrows, Enter opens a folder and imports, Esc restores",
           "[browser]")
{
    const auto directory = browserScratch ("keyboard");
    const auto media = directory / "media";
    std::filesystem::create_directories (media / "drums");
    writeTone (media / "drums" / "snare.wav", 2'400, 0.3f);
    writeTone (media / "pad.wav", 4'800, 0.3f);

    BrowserShell f (directory, "keyboard");
    f.showBrowser();
    yesdaw::ui::mainComponentBrowserOpenFolder (*f, media);   // rows: .., drums, pad.wav

    // Tab reaches the list (and the browser's other controls).
    const auto order = yesdaw::ui::mainComponentControlTraversal (*f);
    for (const char* id : { "browser.source", "browser.up", "browser.import", "browser.list" })
        REQUIRE (std::find (order.begin(), order.end(), juce::String (id)) != order.end());
    REQUIRE (press (*f, juce::KeyPress::tabKey));
    for (std::size_t i = 0; i < order.size() && yesdaw::ui::mainComponentControlTarget (*f).id != "browser.list"; ++i)
        REQUIRE (press (*f, juce::KeyPress::tabKey));
    auto target = yesdaw::ui::mainComponentControlTarget (*f);
    REQUIRE (target.id == "browser.list");
    REQUIRE (target.role == "chooser");
    REQUIRE (target.value.contains (".."));   // the selected row is read out

    // Enter, Down, Esc: the selection is restored.
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    REQUIRE (yesdaw::ui::mainComponentControlTarget (*f).interacting);
    REQUIRE (press (*f, juce::KeyPress::downKey));
    REQUIRE (f.browser().selected == 1);
    REQUIRE (yesdaw::ui::mainComponentControlTarget (*f).value.contains ("drums"));
    REQUIRE (press (*f, juce::KeyPress::escapeKey));
    REQUIRE (f.browser().selected == 0);

    // Enter, Down to "drums", Enter: the folder opens.
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    REQUIRE (press (*f, juce::KeyPress::downKey));
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    REQUIRE (f.browser().location.endsWith ("drums"));
    REQUIRE (f.browser().names == std::vector<juce::String> { "..", "snare.wav" });

    // Enter, Down to the file, Enter: it imports. Space stayed transport throughout (no key was taken).
    REQUIRE (yesdaw::ui::mainComponentControlTarget (*f).id == "browser.list");
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    REQUIRE (press (*f, juce::KeyPress::downKey));
    REQUIRE (press (*f, juce::KeyPress::returnKey));
    const yesdaw::engine::Project project = f.project();
    REQUIRE (project.clips.size() == 1u);
    REQUIRE (project.assets.size() == 1u);
    REQUIRE (project.assets[0].frames == 2'400u);   // the snare, not the pad
}

TEST_CASE ("ADR-0056 a 1000-file folder lists without reading a header for rows not shown", "[browser]")
{
    const auto directory = browserScratch ("thousand");
    const auto media = directory / "media";
    std::filesystem::create_directories (media);
    for (int i = 0; i < 1'000; ++i)
    {
        char name[32];
        std::snprintf (name, sizeof (name), "take-%04d.wav", i);
        std::ofstream (media / name, std::ios::binary) << "RIFF";   // a header the decoder refuses: cheap and distinct
    }

    BrowserShell f (directory, "thousand");
    f.showBrowser();
    const std::uint64_t readsBefore = f.browser().headerReads;
    yesdaw::ui::mainComponentBrowserOpenFolder (*f, media);
    auto snapshot = f.browser();
    REQUIRE (snapshot.names.size() == 1'001u);
    REQUIRE (snapshot.headerReads == readsBefore);   // listing reads no header
    REQUIRE (snapshot.visibleRows > 0);

    yesdaw::ui::mainComponentBrowserPaint (*f);
    snapshot = f.browser();
    const std::uint64_t firstPaint = snapshot.headerReads - readsBefore;
    INFO ("visible " << snapshot.visibleRows << " read " << firstPaint);
    REQUIRE (firstPaint > 0u);
    REQUIRE (firstPaint <= static_cast<std::uint64_t> (snapshot.visibleRows + 1));
    REQUIRE (snapshot.reasons[1].isNotEmpty());   // a painted row carries its reason
    REQUIRE (snapshot.reasons[900].isEmpty());    // a row never shown was never read

    // Painting again reads nothing new; a scroll reads only the rows it brings on screen.
    yesdaw::ui::mainComponentBrowserPaint (*f);
    REQUIRE (f.browser().headerReads - readsBefore == firstPaint);
    yesdaw::ui::mainComponentBrowserScroll (*f, 3);   // nine rows down
    yesdaw::ui::mainComponentBrowserPaint (*f);
    const std::uint64_t afterScroll = f.browser().headerReads - readsBefore;
    REQUIRE (afterScroll > firstPaint);
    REQUIRE (afterScroll <= firstPaint + 10u);
}

// ---- G5.2 cp2 / ADR-0056: the audition voice ----

namespace {

// A model with a project holding one clip (so the engine's mix is not silent), its device "live".
struct AuditionModel
{
    std::filesystem::path directory;
    yesdaw::ui::UiAppModel model;

    AuditionModel (const std::filesystem::path& root, const std::string& name, const std::filesystem::path& song)
        : directory (root / name)
    {
        std::filesystem::create_directories (directory);
        REQUIRE (model.createProjectBundle (directory / "song.yesdaw").ok());
        yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (song);
        REQUIRE (decoded.decoded.has_value());
        REQUIRE (model.importAudioFile (song, std::move (*decoded.decoded)).ok());
        model.setDeviceCallbackLive (true);
    }

    std::vector<float> block (int frames, bool monitorStage = false)   // one device block, interleaved L R
    {
        std::vector<float> left (static_cast<std::size_t> (frames), 0.0f);
        std::vector<float> right (static_cast<std::size_t> (frames), 0.0f);
        float* outputs[] { left.data(), right.data() };
        REQUIRE (model.processDeviceAudioBlock (outputs, 2, frames));
        if (monitorStage)
            model.applyMonitorStage (outputs, 2, frames);
        std::vector<float> out;
        for (int i = 0; i < frames; ++i)
        {
            out.push_back (left[static_cast<std::size_t> (i)]);
            out.push_back (right[static_cast<std::size_t> (i)]);
        }
        return out;
    }
};

yesdaw::ui::UiDecodedAsset decodedFile (const std::filesystem::path& path)
{
    yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (path);
    REQUIRE (decoded.decoded.has_value());
    return std::move (*decoded.decoded);
}

} // namespace

TEST_CASE ("ADR-0056 an audition is on the device outputs from frame 0; the engine's mix, the loudness tap and an export carry none of it",
           "[audition]")
{
    const auto directory = browserScratch ("audition-mix");
    writeTone (directory / "song.wav", 48'000, 0.2f);
    std::vector<float> clicks (1'000, 0.0f);
    clicks[0] = 0.5f;
    clicks[300] = -0.25f;
    REQUIRE (yesdaw::io::writeFloat32WavFile (directory / "clicks.wav", yesdaw::engine::SampleRate { 48'000.0 }, 1, clicks.size(), clicks).ok());

    AuditionModel heard (directory, "heard", directory / "song.wav");
    AuditionModel twin (directory, "twin", directory / "song.wav");
    for (AuditionModel* m : { &heard, &twin })
    {
        REQUIRE (m->model.locatePlaybackFrame (0));
        REQUIRE (m->model.dispatch (UiActionId::TransportPlay).dispatched);
    }
    REQUIRE (heard.model.startAudition (decodedFile (directory / "clicks.wav"), directory / "clicks.wav"));   // while playing
    REQUIRE (heard.model.auditioning());

    // Device blocks in lockstep: the difference is exactly the clicks (mono to both outputs), from frame 0.
    std::vector<float> difference;
    for (int b = 0; b < 16; ++b)
    {
        const std::vector<float> a = heard.block (128);
        const std::vector<float> t = twin.block (128);
        for (std::size_t i = 0; i < a.size(); ++i)
            difference.push_back (a[i] - t[i]);
    }
    for (std::size_t frame = 0; frame < 2'048; ++frame)
    {
        const float expected = frame < clicks.size() ? clicks[frame] : 0.0f;
        REQUIRE (std::abs (difference[frame * 2u] - expected) < 1.0e-6f);
        REQUIRE (std::abs (difference[frame * 2u + 1u] - expected) < 1.0e-6f);
    }

    // The voice played to its end: the UI tick retires it, and the watermark frees it two device blocks later.
    REQUIRE_FALSE (heard.model.auditioning());
    heard.model.serviceAudition();
    REQUIRE (heard.model.auditionSource().empty());
    for (int b = 0; b < 2; ++b)
    {
        (void) heard.block (128);
        (void) twin.block (128);   // in lockstep still
    }
    heard.model.reclaimRetiredAudioObjects();
    REQUIRE (heard.model.retiredAudioObjectCount() == 0u);

    // The live loudness measured the mix only: both models read the same.
    heard.model.serviceLiveLoudness();
    twin.model.serviceLiveLoudness();
    const auto& lh = heard.model.liveLoudnessReadout();
    const auto& lt = twin.model.liveLoudnessReadout();
    REQUIRE (heard.model.liveLoudnessFrames() > 0u);
    REQUIRE (heard.model.liveLoudnessFrames() == twin.model.liveLoudnessFrames());
    REQUIRE (lh.valid == lt.valid);
    REQUIRE (lh.momentaryLufs == lt.momentaryLufs);
    REQUIRE (lh.truePeakDbtp == lt.truePeakDbtp);

    // An export of the same moment is the twin's, sample for sample.
    for (AuditionModel* m : { &heard, &twin })
        REQUIRE (m->model.dispatch (UiActionId::TransportStop).dispatched);
    REQUIRE (heard.model.startAudition (decodedFile (directory / "clicks.wav"), directory / "clicks.wav"));
    REQUIRE (heard.model.exportAudioFile (directory / "heard.wav").dispatched);
    REQUIRE (twin.model.exportAudioFile (directory / "twin.wav").dispatched);
    const auto heardExport = yesdaw::io::decodeAudioFile (directory / "heard.wav");
    const auto twinExport = yesdaw::io::decodeAudioFile (directory / "twin.wav");
    REQUIRE (heardExport.audio.has_value());
    REQUIRE (twinExport.audio.has_value());
    REQUIRE (heardExport.audio->interleaved == twinExport.audio->interleaved);
}

TEST_CASE ("ADR-0056 Play, a new audition and Stop end an audition; Mute silences it; no device refuses it; a 44.1 kHz file plays at the project rate",
           "[audition]")
{
    const auto directory = browserScratch ("audition-stops");
    writeTone (directory / "song.wav", 4'800, 0.2f);
    writeTone (directory / "a.wav", 48'000, 0.5f);
    writeTone (directory / "b.wav", 48'000, 0.4f);
    writeTone (directory / "c44.wav", 44'100, 0.3f, 44'100.0);
    AuditionModel f (directory, "stops", directory / "song.wav");

    // No device: refused with its reason, nothing published.
    f.model.setDeviceCallbackLive (false);
    REQUIRE_FALSE (f.model.startAudition (decodedFile (directory / "a.wav"), directory / "a.wav"));
    REQUIRE_FALSE (f.model.auditioning());
    REQUIRE (f.model.statusLineText().find ("a.wav: no audio device") != std::string::npos);
    f.model.setDeviceCallbackLive (true);

    // Another audition replaces it (the old voice retires); Stop ends it.
    REQUIRE (f.model.startAudition (decodedFile (directory / "a.wav"), directory / "a.wav"));
    REQUIRE (f.model.startAudition (decodedFile (directory / "b.wav"), directory / "b.wav"));
    REQUIRE (f.model.auditionSource().filename() == "b.wav");
    REQUIRE (f.model.retiredAudioObjectCount() >= 1u);
    f.model.stopAudition();
    REQUIRE_FALSE (f.model.auditioning());
    const std::vector<float> silent = f.block (128);
    REQUIRE (std::all_of (silent.begin(), silent.end(), [] (float s) { return s == 0.0f; }));   // stopped transport, no voice

    // Play ends it; so does Record (whichever record path runs); so does opening another project.
    REQUIRE (f.model.startAudition (decodedFile (directory / "a.wav"), directory / "a.wav"));
    REQUIRE (f.model.dispatch (UiActionId::TransportPlay).dispatched);
    REQUIRE_FALSE (f.model.auditioning());
    REQUIRE (f.model.dispatch (UiActionId::TransportStop).dispatched);
    {
        AuditionModel recorder (directory, "recorder", directory / "song.wav");
        REQUIRE (recorder.model.dispatch (UiActionId::DeviceSelectTestAudio).dispatched);   // the harness device
        REQUIRE (recorder.model.dispatch (UiActionId::RecordingArmTrack).dispatched);
        REQUIRE (recorder.model.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);
        REQUIRE (recorder.model.startAudition (decodedFile (directory / "a.wav"), directory / "a.wav"));
        REQUIRE (recorder.model.dispatch (UiActionId::TransportRecord).dispatched);
        REQUIRE_FALSE (recorder.model.auditioning());
    }
    {
        AuditionModel other (directory, "other", directory / "song.wav");
        REQUIRE (other.model.startAudition (decodedFile (directory / "a.wav"), directory / "a.wav"));
        REQUIRE (other.model.createProjectBundle (directory / "replacement.yesdaw").ok());
        REQUIRE_FALSE (other.model.auditioning());
    }

    // Mute (the monitor's last stage) silences it once its 5 ms ramp is done.
    REQUIRE (f.model.startAudition (decodedFile (directory / "a.wav"), directory / "a.wav"));
    const std::vector<float> loud = f.block (128, true);
    REQUIRE (std::any_of (loud.begin(), loud.end(), [] (float s) { return s != 0.0f; }));
    REQUIRE (f.model.dispatch (UiActionId::MasterMonitorMuteToggle).dispatched);
    (void) f.block (128, true);   // the ramp: 240 frames at 48 kHz, two blocks
    (void) f.block (128, true);
    const std::vector<float> muted = f.block (128, true);
    REQUIRE (std::all_of (muted.begin(), muted.end(), [] (float s) { return s == 0.0f; }));
    REQUIRE (f.model.auditioning());   // muted, not stopped
    REQUIRE (f.model.dispatch (UiActionId::MasterMonitorMuteToggle).dispatched);
    f.model.stopAudition();

    // One second at 44.1 kHz plays for 48 000 frames at the project rate (ADR-0055's live view): 375 blocks.
    REQUIRE (f.model.startAudition (decodedFile (directory / "c44.wav"), directory / "c44.wav"));
    int blocks = 0;
    while (f.model.auditioning() && blocks < 1'000)
    {
        (void) f.block (128);
        ++blocks;
    }
    REQUIRE (blocks == 375);
}

// The real mouse path on a play mark: press, release, press, release, double-click - as Windows delivers a quick
// double press. Both presses toggle the audition and nothing is imported ("auditioning never imports"); a double-click
// on the row's name still keeps it (imports a file, opens a folder).
TEST_CASE ("ADR-0056 a double press on a play mark toggles the audition twice and imports nothing", "[audition][browser]")
{
    yesdaw::ui::BrowserListComponent list;
    list.setSize (400, 200);
    yesdaw::ui::BrowserRow file;
    file.kind = yesdaw::ui::BrowserRow::Kind::File;
    file.name = "a.wav";
    file.path = "a.wav";
    list.setRows ({ file });
    int auditions = 0;
    int keeps = 0;
    list.onAudition = [&auditions] (const yesdaw::ui::BrowserRow&) { ++auditions; };
    list.onKeep = [&keeps] (const yesdaw::ui::BrowserRow&) { ++keeps; };

    const auto event = [&list] (juce::Point<int> at, int clicks) {
        const juce::Time now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), at.toFloat(),
                                 juce::ModifierKeys::leftButtonModifier, juce::MouseInputSource::defaultPressure,
                                 juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation,
                                 juce::MouseInputSource::defaultTiltX, juce::MouseInputSource::defaultTiltY, &list, &list,
                                 now, at.toFloat(), now, clicks, false);
    };
    const auto doublePress = [&list, &event] (juce::Point<int> at) {
        list.mouseDown (event (at, 1));
        list.mouseUp (event (at, 1));
        list.mouseDown (event (at, 2));
        list.mouseUp (event (at, 2));
        list.mouseDoubleClick (event (at, 2));
    };

    const juce::Point<int> mark { yesdaw::ui::UiTheme::Layout::browserRowTextInset + 9, 10 };   // row 0's play mark
    doublePress (mark);
    REQUIRE (auditions == 2);   // start, then stop
    REQUIRE (keeps == 0);       // never an import

    doublePress ({ 200, 10 });  // the row's name
    REQUIRE (auditions == 2);
    REQUIRE (keeps == 1);
}

TEST_CASE ("ADR-0056 the browser's Audition button and play marks start, replace and stop an audition; closing the browser stops it",
           "[audition][browser]")
{
    const auto directory = browserScratch ("audition-shell");
    const auto media = directory / "media";
    std::filesystem::create_directories (media);
    writeTone (media / "a.wav", 48'000, 0.3f);
    writeTone (media / "b.wav", 48'000, 0.3f);

    BrowserShell f (directory, "audition");
    f.showBrowser();
    yesdaw::ui::mainComponentBrowserOpenFolder (*f, media);
    yesdaw::ui::mainComponentBrowserSelect (*f, f.rowNamed ("a.wav"));

    // A headless shell has no device: refused by name and reason.
    yesdaw::ui::mainComponentBrowserAudition (*f);
    REQUIRE_FALSE (f.browser().auditioning);
    REQUIRE (statusOf (*f).contains ("a.wav: no audio device"));

    yesdaw::ui::mainComponentSetDeviceCallbackLiveForTest (*f, true);
    yesdaw::ui::mainComponentBrowserAudition (*f);
    auto snapshot = f.browser();
    REQUIRE (snapshot.auditioning);
    REQUIRE (snapshot.auditionName == "a.wav");
    REQUIRE (snapshot.auditionButton == "Stop");
    REQUIRE (static_cast<bool> (probeOf (*f)["view"]["browser"]["auditioning"]));
    yesdaw::ui::mainComponentBrowserAudition (*f);   // a second press stops it
    REQUIRE_FALSE (f.browser().auditioning);
    REQUIRE (f.browser().auditionButton == "Audition");

    // The play marks: b auditions; a replaces it; a again stops it.
    yesdaw::ui::mainComponentBrowserPressPlayMark (*f, f.rowNamed ("b.wav"));
    REQUIRE (f.browser().auditionName == "b.wav");
    REQUIRE (f.browser().selected == f.rowNamed ("b.wav"));
    yesdaw::ui::mainComponentBrowserPressPlayMark (*f, f.rowNamed ("a.wav"));
    REQUIRE (f.browser().auditionName == "a.wav");
    yesdaw::ui::mainComponentBrowserPressPlayMark (*f, f.rowNamed ("a.wav"));
    REQUIRE_FALSE (f.browser().auditioning);
    REQUIRE (f.project().clips.empty());   // auditioning never imports

    // Closing the browser stops it.
    yesdaw::ui::mainComponentBrowserPressPlayMark (*f, f.rowNamed ("b.wav"));
    REQUIRE (f.browser().auditioning);
    REQUIRE (press (*f, 'y'));
    REQUIRE_FALSE (f.browser().auditioning);

    // The keyboard reaches the Audition button (Tab), and Play (Space stays transport) stops an audition.
    REQUIRE (press (*f, 'y'));
    const auto order = yesdaw::ui::mainComponentControlTraversal (*f);
    REQUIRE (std::find (order.begin(), order.end(), juce::String ("browser.audition")) != order.end());
    yesdaw::ui::mainComponentBrowserSelect (*f, f.rowNamed ("a.wav"));
    yesdaw::ui::mainComponentBrowserAudition (*f);
    REQUIRE (f.browser().auditioning);
    REQUIRE (press (*f, juce::KeyPress::spaceKey));
    REQUIRE_FALSE (f.browser().auditioning);
    yesdaw::ui::mainComponentServiceUiTick (*f);
    REQUIRE (f.browser().auditionButton == "Audition");
}
