// YES DAW — the media browser's shell half (G5.2 / ADR-0056): what the rows are for each source, the header-fact
// cache, keeping a row (the import verbs), a drag onto the lanes (the drop verb), and the browser's session state.

#include "ui/MainComponentShell.h"

#include <cstring>
#include <optional>

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

namespace {

juce::String utf8String (const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return juce::String::fromUTF8 (reinterpret_cast<const char*> (utf8.data()), static_cast<int> (utf8.size()));
}

// A path's name as UTF-8 bytes for the status line (path::string() converts through the ANSI code page on Windows
// and throws on a name it cannot represent).
std::string utf8Name (const std::filesystem::path& path)
{
    const std::u8string utf8 = path.filename().u8string();
    return std::string (reinterpret_cast<const char*> (utf8.data()), utf8.size());
}

bool sameRow (const BrowserRow& a, const BrowserRow& b)
{
    return a.kind == b.kind && (a.kind == BrowserRow::Kind::Asset ? a.assetId == b.assetId : a.path == b.path);
}

void hashBytes (std::uint64_t& hash, const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*> (data);
    for (std::size_t i = 0; i < size; ++i)
        hash = (hash ^ bytes[i]) * 1'099'511'628'211ull;   // FNV-1a
}

constexpr std::size_t kBrowserFactsCacheLimit = 4'096;

juce::String rateText (double hz)
{
    return std::fmod (hz, 1'000.0) == 0.0 ? juce::String (static_cast<int> (hz / 1'000.0)) + " kHz"
                                          : juce::String (hz / 1'000.0, 1) + " kHz";
}

juce::String lengthText (std::uint64_t frames, double hz)
{
    const double seconds = hz > 0.0 ? static_cast<double> (frames) / hz : 0.0;
    const int minutes = static_cast<int> (seconds / 60.0);
    return juce::String (minutes) + ":" + juce::String (seconds - 60.0 * minutes, 2).paddedLeft ('0', 5);
}

juce::String factsText (const juce::String& format, double hz, std::uint16_t channels, std::uint64_t frames)
{
    return (format.isEmpty() ? juce::String() : format + juce::String::fromUTF8 (" \xc2\xb7 "))
         + rateText (hz) + juce::String::fromUTF8 (" \xc2\xb7 ") + (channels == 1u ? "mono" : "stereo")
         + juce::String::fromUTF8 (" \xc2\xb7 ") + lengthText (frames, hz);
}

bool lessCaseInsensitive (const BrowserRow& a, const BrowserRow& b)
{
    return a.name.compareIgnoreCase (b.name) < 0;
}

} // namespace

void MainComponent::initialiseBrowser()
{
    browserPanel.source.onChange = [this] {
        browserSourceIndex = std::max (0, browserPanel.source.getSelectedItemIndex());
        saveBrowserState();
        refreshBrowserRows();
    };
    browserPanel.upButton.onClick = [this] {
        if (browserSourceIndex == 0 && browserFolder.has_parent_path() && browserFolder.parent_path() != browserFolder)
            browserOpenFolder (browserFolder.parent_path());
    };
    browserPanel.importButton.onClick = [this] {
        if (const BrowserRow* row = browserPanel.list.selectedRow())
            browserKeep (*row);
    };
    browserPanel.list.loadFacts = [this] (BrowserRow& row) { loadBrowserFacts (row); };
    browserPanel.list.onKeep = [this] (const BrowserRow& row) { browserKeep (row); };
    browserPanel.list.onDragReleased = [this] (std::vector<BrowserRow> rows, juce::Point<int> listPosition) {
        browserDropAt (std::move (rows), listPosition);
    };
    addChildComponent (browserPanel);
}

// The browser's session state, read the first time it is shown: a native launch points the model at its records
// folder late in start-up, and nothing lists a folder until someone looks.
void MainComponent::restoreBrowserState()
{
    browserStateRestored = true;
    const yesdaw::ui::UiAppModel::UiBrowserState state = appModel.browserState();
    browserSourceIndex = state.source == "project" ? 1 : state.source == "recent" ? 2 : 0;
    browserFolder = state.folder;
    std::error_code error;
    if (browserFolder.empty() || ! std::filesystem::is_directory (browserFolder, error))
    {
        juce::File music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        if (! music.isDirectory())
            music = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        browserFolder = pathFromJuceString (music.getFullPathName());
    }
    browserPanel.source.setSelectedItemIndex (browserSourceIndex, juce::dontSendNotification);
}

bool MainComponent::dockShowsBrowser() const noexcept
{
    return appModel.context().mixerDockVisible
        && appModel.context().editorDockTab == yesdaw::ui::UiEditorDockTab::Browser;
}

void MainComponent::saveBrowserState()
{
    yesdaw::ui::UiAppModel::UiBrowserState state;
    state.source = browserSourceIndex == 1 ? "project" : browserSourceIndex == 2 ? "recent" : "files";
    state.folder = browserFolder;
    appModel.setBrowserState (state);
}

void MainComponent::browserOpenFolder (const std::filesystem::path& folder)
{
    std::error_code error;
    if (! std::filesystem::is_directory (folder, error))
    {
        appModel.reportStatus ("Browser: cannot open " + utf8Name (folder), true);
        return;
    }
    browserFolder = folder;
    browserSourceIndex = 0;
    browserPanel.source.setSelectedItemIndex (0, juce::dontSendNotification);
    saveBrowserState();
    refreshBrowserRows();
}

// What the Project and Recent rows are built from: the Assets and the clip names that name them; the Recent record's
// revision. The stamp moves only when those do, so the UI tick rebuilds the rows only then.
std::uint64_t MainComponent::browserSourceStamp() const noexcept
{
    std::uint64_t hash = 14'695'981'039'346'656'037ull;
    hashBytes (hash, &browserSourceIndex, sizeof (browserSourceIndex));
    if (browserSourceIndex == 2)
    {
        const std::uint64_t revision = appModel.recentAudioRevision();
        hashBytes (hash, &revision, sizeof (revision));
        return hash;
    }
    const yesdaw::engine::Project& project = appModel.project();
    for (const yesdaw::engine::Asset& asset : project.assets)
        hashBytes (hash, asset.id.bytes.data(), asset.id.bytes.size());
    for (const yesdaw::engine::Clip& clip : project.clips)
    {
        hashBytes (hash, clip.assetId.bytes.data(), clip.assetId.bytes.size());
        const char* name = clip.name.c_str();
        hashBytes (hash, name, std::strlen (name));
    }
    return hash;
}

void MainComponent::refreshBrowserRows()
{
    // A refresh of the same listing is not a navigation: the selected rows, the current one and the scroll stay.
    const bool sameListing = browserRowsSourceShown == browserSourceIndex
                          && (browserSourceIndex != 0 || browserRowsFolderShown == browserFolder);
    std::vector<BrowserRow> keptRows;
    std::optional<BrowserRow> keptCurrent;
    const int keptFirstVisible = browserPanel.list.firstVisibleRow();
    if (sameListing)
    {
        keptRows = browserPanel.list.selectedRows();
        if (const BrowserRow* current = browserPanel.list.selectedRow())
            keptCurrent = *current;
    }
    browserRowsSourceShown = browserSourceIndex;
    browserRowsFolderShown = browserFolder;
    browserRowsStamp = browserSourceStamp();
    std::vector<BrowserRow> rows;
    if (browserSourceIndex == 0)   // Files: "..", the sub-folders, then the supported files
    {
        browserPanel.location.setText (utf8String (browserFolder), juce::dontSendNotification);
        if (browserFolder.has_parent_path() && browserFolder.parent_path() != browserFolder)
        {
            BrowserRow parent;
            parent.kind = BrowserRow::Kind::Parent;
            parent.name = "..";
            parent.path = browserFolder.parent_path();
            parent.factsLoaded = true;
            rows.push_back (std::move (parent));
        }
        std::vector<BrowserRow> folders;
        std::vector<BrowserRow> files;
        std::error_code error;
        for (std::filesystem::directory_iterator it (browserFolder, std::filesystem::directory_options::skip_permission_denied, error), end;
             ! error && it != end; it.increment (error))
        {
            const std::filesystem::path& path = it->path();
            const juce::String name = utf8String (path.filename());
            if (name.startsWithChar ('.'))
                continue;
            std::error_code kindError;
            BrowserRow row;
            row.name = name;
            row.path = path;
            if (it->is_directory (kindError))
            {
                row.kind = BrowserRow::Kind::Folder;
                row.factsLoaded = true;
                folders.push_back (std::move (row));
            }
            else if (yesdaw::io::isImportableAudioPath (path))
            {
                row.kind = BrowserRow::Kind::File;
                files.push_back (std::move (row));
            }
        }
        std::sort (folders.begin(), folders.end(), lessCaseInsensitive);
        std::sort (files.begin(), files.end(), lessCaseInsensitive);
        rows.insert (rows.end(), std::make_move_iterator (folders.begin()), std::make_move_iterator (folders.end()));
        rows.insert (rows.end(), std::make_move_iterator (files.begin()), std::make_move_iterator (files.end()));
    }
    else if (browserSourceIndex == 1)   // Project: the Assets, named by their first clip
    {
        browserPanel.location.setText ("This project's audio", juce::dontSendNotification);
        const yesdaw::engine::Project& project = appModel.project();
        for (const yesdaw::engine::Asset& asset : project.assets)
        {
            BrowserRow row;
            row.kind = BrowserRow::Kind::Asset;
            row.assetId = asset.id;
            for (const yesdaw::engine::Clip& clip : project.clips)
                if (clip.assetId == asset.id)
                {
                    row.name = juce::String::fromUTF8 (clip.name.c_str());
                    break;
                }
            if (row.name.isEmpty())
            {
                juce::String hex;
                for (std::size_t i = 0; i < 4u; ++i)
                    hex += juce::String::toHexString (static_cast<int> (asset.contentHash.bytes[i])).paddedLeft ('0', 2);
                row.name = "Asset " + hex;
            }
            row.facts = factsText ({}, asset.sampleRate.hz, asset.channels, asset.frames);
            row.factsLoaded = true;
            rows.push_back (std::move (row));
        }
    }
    else   // Recent: newest first
    {
        browserPanel.location.setText ("Recently imported", juce::dontSendNotification);
        for (const std::filesystem::path& path : appModel.recentAudioFiles())
        {
            BrowserRow row;
            row.kind = BrowserRow::Kind::File;
            row.name = utf8String (path.filename());
            row.path = path;
            rows.push_back (std::move (row));
        }
    }
    browserPanel.upButton.setEnabled (browserSourceIndex == 0);
    browserPanel.list.setRows (std::move (rows));
    if (! sameListing)
        return;
    const std::vector<BrowserRow>& listed = browserPanel.list.rows();
    std::vector<int> marked;
    int current = -1;
    for (std::size_t i = 0; i < listed.size(); ++i)
    {
        for (const BrowserRow& kept : keptRows)
            if (sameRow (listed[i], kept))
                marked.push_back (static_cast<int> (i));
        if (keptCurrent.has_value() && sameRow (listed[i], *keptCurrent))
            current = static_cast<int> (i);
    }
    browserPanel.list.restoreState (marked, current, keptFirstVisible);
}

// A row's facts from its file header, cached by path, size and modification time (only painted rows ask).
void MainComponent::loadBrowserFacts (BrowserRow& row)
{
    if (row.kind != BrowserRow::Kind::File)
        return;
    std::error_code error;
    const auto size = std::filesystem::file_size (row.path, error);
    const auto modified = error ? std::filesystem::file_time_type {} : std::filesystem::last_write_time (row.path, error);
    const std::u8string keyUtf8 = row.path.u8string();
    const std::string key (reinterpret_cast<const char*> (keyUtf8.data()), keyUtf8.size());
    if (! error)
        if (const auto cached = browserFactsCache.find (key);
            cached != browserFactsCache.end() && cached->second.size == size && cached->second.modified == modified)
        {
            row.facts = cached->second.facts;
            row.reason = cached->second.reason;
            return;
        }
    ++browserHeaderReads;
    if (browserFactsCache.size() >= kBrowserFactsCacheLimit)
        browserFactsCache.clear();   // bounded: a long session browsing many folders never grows it without end
    const yesdaw::io::AudioFileFacts facts = yesdaw::io::readAudioFileFacts (row.path);
    row.reason = juce::String::fromUTF8 (facts.reason.c_str());
    if (facts.readable)
        row.facts = factsText (juce::String::fromUTF8 (facts.formatName.c_str()), facts.sampleRateHz, facts.channels, facts.frames);
    if (! error)
        browserFactsCache[key] = { size, modified, row.facts, row.reason };
}

// Keep: a file imports onto the selected track at the playhead (Ctrl+Shift+I's verb), a project Asset is placed the
// same way with no copy, a folder or ".." opens.
void MainComponent::browserKeep (const BrowserRow& row)
{
    switch (row.kind)
    {
        case BrowserRow::Kind::Parent:
        case BrowserRow::Kind::Folder:
            browserOpenFolder (row.path);
            return;
        case BrowserRow::Kind::File:
            importAudioFromPath (row.path);
            break;
        case BrowserRow::Kind::Asset:
        {
            const auto& tracks = appModel.project().tracks;
            const std::size_t lane = selectedTrackLane >= 0 && selectedTrackLane < static_cast<int> (tracks.size())
                                         ? static_cast<std::size_t> (selectedTrackLane) : 0u;
            const yesdaw::engine::EntityId ids[] { row.assetId };
            const yesdaw::ui::UiAudioDropResult placed = appModel.placeAssetsAt (
                ids, lane, static_cast<yesdaw::engine::Tick> (std::max<std::int64_t> (0, appModel.context().playheadFrame)));
            if (! placed.refusals.empty())
                appModel.reportStatus ("Import refused: " + placed.refusals.front(), true);
            break;
        }
    }
    if (browserSourceIndex == 1 || browserSourceIndex == 2)
        refreshBrowserRows();   // a new Asset or a new Recent entry shows at once
    refreshActionState();
    repaintAll();
}

// A drag released over the lanes lands through the drop verb (files) or placeAssetsAt (project Assets).
void MainComponent::browserDropAt (std::vector<BrowserRow> rows, juce::Point<int> listPosition)
{
    const juce::Point<int> inTimeline = timelineInput.getLocalPoint (&browserPanel.list, listPosition);
    if (! timelineInput.getLocalBounds().contains (inTimeline))
        return;
    const std::optional<std::pair<int, double>> target = timelineInput.dropLaneAndSecondsAt (inTimeline);
    if (! target.has_value())
        return;
    juce::StringArray files;
    std::vector<yesdaw::engine::EntityId> assets;
    for (const BrowserRow& row : rows)
    {
        if (row.kind == BrowserRow::Kind::File)
            files.add (utf8String (row.path));
        else if (row.kind == BrowserRow::Kind::Asset)
            assets.push_back (row.assetId);
    }
    if (! files.isEmpty() && timelineInput.onFilesDropped)
        timelineInput.onFilesDropped (files, target->first, target->second);
    if (! assets.empty())
    {
        const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (target->second);
        if (! tick.has_value())
            return;
        const yesdaw::ui::UiAudioDropResult placed = appModel.placeAssetsAt (
            assets, static_cast<std::size_t> (std::max (0, target->first)), snappedTimelineTick (*tick, false));
        if (! placed.refusals.empty())
            appModel.reportStatus ("Import refused: " + placed.refusals.front(), true);
        refreshActionState();
        repaintAll();
    }
    if (browserSourceIndex != 0)
        refreshBrowserRows();
}

// Ctrl+Shift+I's import, for a path from any surface: the selected track at the playhead; a refusal is named.
void MainComponent::importAudioFromPath (const std::filesystem::path& path)
{
    UiAudioDecodeResult decodedFile = decodeProjectAudio (path);   // ADR-0054: any supported format
    if (auto& decoded = decodedFile.decoded)
    {
        // Import lands on the SELECTED Track when the rail has a selection.
        const auto& tracks = appModel.project().tracks;
        // R7: verb failures report their precise reason inside the model.
        if (selectedTrackLane >= 0 && selectedTrackLane < static_cast<int> (tracks.size()))
            (void) appModel.importAudioFileToTrack (path, std::move (*decoded),
                                                    tracks[static_cast<std::size_t> (selectedTrackLane)].id);
        else
            (void) appModel.importAudioFile (path, std::move (*decoded));
    }
    else
    {
        // R6: a refused file is named with its reason, never swallowed.
        appModel.reportStatus ("Import refused: " + utf8Name (path) + ": " + decodedFile.reason, true);
    }
}

} // namespace yesdaw::ui
