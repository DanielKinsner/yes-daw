// YES DAW - ADR-0062 gates: missing audio relink. cp1: the bundle inspection (every missing or damaged Asset file,
// nothing written) and the adoption of an Asset's original bytes only. cp2: the open's questions, Cancel, the render.

#include "io/WavFile.h"
#include "persistence/ProjectBundle.h"
#include "ui/MainComponent.h"
#include "ui/MainComponentInternal.h"
#include "ui/MissingAudio.h"
#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

using MissingAssetFile = yesdaw::persistence::ProjectBundleDb::MissingAssetFile;

std::filesystem::path relinkScratch (const std::string& label)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-relink-" + label + "-" + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

void writeTone (const std::filesystem::path& path, std::size_t frames, float level, std::size_t period)
{
    std::vector<float> samples (frames);
    for (std::size_t i = 0; i < frames; ++i)
        samples[i] = level * static_cast<float> ((i % period) < period / 2u ? 1.0 : -1.0);
    REQUIRE (yesdaw::io::writeFloat32WavFile (path, yesdaw::engine::SampleRate { 48'000.0 }, 1, samples.size(), samples).ok());
}

std::string bytesOf (const std::filesystem::path& path)
{
    std::ifstream in (path, std::ios::binary);
    return std::string ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
}

void writeBytes (const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream out (path, std::ios::binary | std::ios::trunc);
    out << bytes;
}

// Every file in a bundle and its bytes (SQLite's transient -wal / -shm left out).
std::map<std::string, std::string> snapshotOf (const std::filesystem::path& bundle)
{
    std::map<std::string, std::string> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator (bundle))
    {
        const std::string name = entry.path().filename().string();
        if (! entry.is_regular_file() || name == "project.db-wal" || name == "project.db-shm")
            continue;
        files[std::filesystem::relative (entry.path(), bundle).generic_string()] = bytesOf (entry.path());
    }
    return files;
}

std::filesystem::path assetFileOf (const std::filesystem::path& bundle, const yesdaw::engine::AssetContentHash& hash)
{
    return bundle / std::filesystem::path (yesdaw::persistence::detail::assetRelativePathForHash (hash));
}

// A closed bundle with three imported tones (each an Asset whose original is the .wav beside the bundle).
struct ThreeTones
{
    std::filesystem::path directory;
    std::filesystem::path bundle;
    std::vector<std::filesystem::path> originals;
    std::vector<yesdaw::engine::AssetContentHash> hashes;

    explicit ThreeTones (const std::string& label) : directory (relinkScratch (label)), bundle (directory / "song.yesdaw")
    {
        yesdaw::ui::UiAppModel model;
        REQUIRE (model.createProjectBundle (bundle).ok());
        for (std::size_t i = 0; i < 3u; ++i)
        {
            const auto wav = directory / ("tone-" + std::to_string (i) + ".wav");
            writeTone (wav, 24'000 * (i + 1u), 0.2f, 60u + 20u * i);   // 0.5 s, 1.0 s, 1.5 s
            yesdaw::ui::shell::UiAudioDecodeResult decoded = yesdaw::ui::shell::decodeProjectAudio (wav);
            REQUIRE (decoded.decoded.has_value());
            REQUIRE (model.importAudioFile (wav, std::move (*decoded.decoded)).ok());
            originals.push_back (wav);
        }
        REQUIRE (model.project().assets.size() == 3u);
        for (const yesdaw::engine::Asset& asset : model.project().assets)
            hashes.push_back (asset.contentHash);
    }
};

bool listed (const std::vector<MissingAssetFile>& missing, const yesdaw::engine::AssetContentHash& hash, bool damaged)
{
    for (const MissingAssetFile& file : missing)
        if (file.hash == hash && file.damaged == damaged)
            return true;
    return false;
}

} // namespace

TEST_CASE ("ADR-0062 the inspection lists every missing and damaged Asset file and writes nothing", "[relink]")
{
    ThreeTones f ("inspect");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));
    writeBytes (assetFileOf (f.bundle, f.hashes[1]), "damaged bytes");
    yesdaw::persistence::ProjectBundleDb refused;
    REQUIRE_FALSE (yesdaw::persistence::ProjectBundleDb::openExistingBundle (f.bundle, refused).ok());   // the open stops at one

    const auto before = snapshotOf (f.bundle);
    yesdaw::engine::Project project;
    std::vector<MissingAssetFile> missing;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::inspectAssetFiles (f.bundle, project, missing).ok());
    REQUIRE (missing.size() == 2u);   // both, past the first
    REQUIRE (listed (missing, f.hashes[0], false));
    REQUIRE (listed (missing, f.hashes[1], true));
    REQUIRE (project.assets.size() == 3u);
    REQUIRE (project.clips.size() == 3u);
    REQUIRE (snapshotOf (f.bundle) == before);   // nothing written, nothing swept
}

TEST_CASE ("ADR-0062 a bundle on an older schema is not inspected", "[relink]")
{
    ThreeTones f ("old-schema");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));
    {
        sqlite3* db = nullptr;
        REQUIRE (sqlite3_open_v2 (yesdaw::persistence::detail::utf8Path (f.bundle / "project.db").c_str(), &db,
                                  SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);
        const std::string sql = "PRAGMA user_version = " + std::to_string (yesdaw::persistence::kCodeSchemaVersion - 1) + ";";
        REQUIRE (sqlite3_exec (db, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close (db);
    }
    const auto before = snapshotOf (f.bundle);
    yesdaw::engine::Project project;
    std::vector<MissingAssetFile> missing;
    REQUIRE_FALSE (yesdaw::persistence::ProjectBundleDb::inspectAssetFiles (f.bundle, project, missing).ok());
    REQUIRE (missing.empty());
    REQUIRE (snapshotOf (f.bundle) == before);   // no migration ran
}

TEST_CASE ("ADR-0062 adoption takes only an Asset's original bytes; a damaged file goes to the trash, never over earlier "
           "evidence",
           "[relink]")
{
    ThreeTones f ("adopt");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));
    writeBytes (assetFileOf (f.bundle, f.hashes[1]), "damaged bytes");

    // Another tone is refused with its reason and writes nothing.
    const auto before = snapshotOf (f.bundle);
    const yesdaw::persistence::BundleResult wrong =
        yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[0], f.originals[2]);
    REQUIRE_FALSE (wrong.ok());
    REQUIRE (wrong.message == "its content differs");
    REQUIRE (snapshotOf (f.bundle) == before);

    // The originals are taken: the missing one appears, the damaged one moves to the trash.
    REQUIRE (yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[0], f.originals[0]).ok());
    REQUIRE (bytesOf (assetFileOf (f.bundle, f.hashes[0])) == bytesOf (f.originals[0]));
    REQUIRE (yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[1], f.originals[1]).ok());
    REQUIRE (bytesOf (assetFileOf (f.bundle, f.hashes[1])) == bytesOf (f.originals[1]));
    const std::string hex = yesdaw::persistence::detail::hexBytes (f.hashes[1].bytes);
    REQUIRE (bytesOf (f.bundle / ".trash" / (hex + ".asset.1")) == "damaged bytes");

    // Damaged again and relinked again: the first evidence stays.
    writeBytes (assetFileOf (f.bundle, f.hashes[1]), "damaged again");
    REQUIRE (yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[1], f.originals[1]).ok());
    REQUIRE (bytesOf (f.bundle / ".trash" / (hex + ".asset.1")) == "damaged bytes");
    REQUIRE (bytesOf (f.bundle / ".trash" / (hex + ".asset.2")) == "damaged again");

    // A file already holding the bytes is left alone (no new trash entry).
    REQUIRE (yesdaw::persistence::ProjectBundleDb::adoptAssetFile (f.bundle, f.hashes[1], f.originals[1]).ok());
    REQUIRE_FALSE (std::filesystem::exists (f.bundle / ".trash" / (hex + ".asset.3")));

    for (const auto& entry : std::filesystem::directory_iterator (f.bundle / "audio"))
        REQUIRE (entry.path().extension() != ".tmp");   // no temporary left
    yesdaw::persistence::ProjectBundleDb reopened;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::openExistingBundle (f.bundle, reopened).ok());   // the validator agrees
}

// ---- cp2: the open asks ----

namespace {

// The bundle reopened through the ordinary open (its Assets decoded), rendered to float: what the project sounds like.
std::string renderOf (const std::filesystem::path& bundle, const std::filesystem::path& wav)
{
    yesdaw::ui::UiAppModel model;
    yesdaw::ui::shell::StoredProjectAssetsResult stored = yesdaw::ui::shell::decodeStoredProjectAssets (bundle);
    REQUIRE (stored.assets.has_value());
    REQUIRE (model.loadPreparedProjectBundle (std::move (stored.prepared), std::move (*stored.assets)).ok());
    model.setExportBitDepth (yesdaw::ui::UiAppModel::UiExportBitDepth::Float32);
    REQUIRE (model.exportAudioFile (wav).dispatched);
    return bytesOf (wav);
}

// A shell whose missing-audio answers come from a script (empty = Cancel); every question is kept.
struct RelinkShell
{
    std::filesystem::path open;
    std::vector<std::filesystem::path> answers;
    std::vector<yesdaw::ui::UiMissingAsset> asked;
    std::function<void()> whileAsking;   // runs inside the question (the probe's `relink` while it is up)
    std::unique_ptr<juce::Component> shell;

    explicit RelinkShell (const std::filesystem::path& sessionFolder, const std::filesystem::path& launchBundle = {})
    {
        juce::MessageManager::getInstance();
        yesdaw::ui::MainComponentFileChoices choices;
        choices.sessionStateDirectory = sessionFolder;
        choices.chooseOpenProjectBundle = [this] { return open; };
        choices.chooseNewProjectBundle = [this] { return open; };
        choices.chooseMissingAudioReplacement = [this] (const yesdaw::ui::UiMissingAsset& missing) {
            asked.push_back (missing);
            if (whileAsking)
                whileAsking();
            if (answers.empty())
                return std::filesystem::path {};
            const std::filesystem::path answer = answers.front();
            answers.erase (answers.begin());
            return answer;
        };
        if (! launchBundle.empty())
        {
            choices.initialiseSessionAtLaunch = true;
            choices.openBundleAtLaunch = launchBundle;
        }
        shell = yesdaw::ui::createMainComponent (std::move (choices));
        REQUIRE (shell != nullptr);
    }

    void dispatch (yesdaw::ui::UiActionId action) { yesdaw::ui::mainComponentDispatchAction (*shell, action); }
    [[nodiscard]] yesdaw::ui::MainComponentSnapshot snapshot() { return yesdaw::ui::snapshotMainComponent (*shell); }
};

} // namespace

TEST_CASE ("ADR-0062 the open asks about each missing or damaged audio file, refuses another file, takes the originals; "
           "the project renders as before and reopens without a question",
           "[relink]")
{
    ThreeTones f ("open");
    const std::string before = renderOf (f.bundle, f.directory / "before.wav");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));
    writeBytes (assetFileOf (f.bundle, f.hashes[1]), "damaged bytes");

    RelinkShell r (f.directory / "session");
    r.open = f.bundle;
    r.answers = { f.originals[2], f.originals[0], f.originals[1] };   // the wrong tone first
    r.dispatch (yesdaw::ui::UiActionId::ProjectOpen);
    REQUIRE (r.asked.size() == 3u);
    REQUIRE (r.asked[0].description == "Audio Clip - 0:00.5 48 kHz mono, used by 1 clip");   // ADR-0056: its first clip's name
    REQUIRE (r.asked[0].refusal.empty());
    REQUIRE_FALSE (r.asked[0].damaged);
    REQUIRE (r.asked[1].description == r.asked[0].description);   // asked again, with the reason
    REQUIRE (r.asked[1].refusal == "tone-2.wav is not the missing audio: its content differs");
    REQUIRE (r.asked[2].description == "Audio Clip - 0:01.0 48 kHz mono, used by 1 clip");
    REQUIRE (r.asked[2].damaged);
    REQUIRE (r.snapshot().context.projectLoaded);
    REQUIRE (r.snapshot().windowTitle.find ("song") != std::string::npos);
    const std::string hex = yesdaw::persistence::detail::hexBytes (f.hashes[1].bytes);
    REQUIRE (bytesOf (f.bundle / ".trash" / (hex + ".asset.1")) == "damaged bytes");

    r.dispatch (yesdaw::ui::UiActionId::ProjectSave);
    r.shell.reset();
    REQUIRE (renderOf (f.bundle, f.directory / "after.wav") == before);   // the intended content, exactly

    RelinkShell again (f.directory / "session-2");
    again.open = f.bundle;
    again.dispatch (yesdaw::ui::UiActionId::ProjectOpen);
    REQUIRE (again.asked.empty());   // the validator is satisfied: no question
    REQUIRE (again.snapshot().context.projectLoaded);
}

TEST_CASE ("ADR-0062 Cancel keeps the current project and names what remains missing; a partial relink is kept",
           "[relink]")
{
    ThreeTones f ("cancel");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));
    writeBytes (assetFileOf (f.bundle, f.hashes[1]), "damaged bytes");

    RelinkShell r (f.directory / "session");
    r.open = f.directory / "current.yesdaw";
    r.dispatch (yesdaw::ui::UiActionId::ProjectNew);
    r.dispatch (yesdaw::ui::UiActionId::TrackAdd);
    REQUIRE (r.snapshot().context.canUndo);

    const auto untouched = snapshotOf (f.bundle);
    r.open = f.bundle;
    r.dispatch (yesdaw::ui::UiActionId::ProjectOpen);   // Cancel at the first question
    REQUIRE (r.asked.size() == 1u);
    REQUIRE (r.snapshot().statusLineText == "Open cancelled: 2 audio files still missing (Audio Clip, Audio Clip)");
    REQUIRE (r.snapshot().windowTitle.find ("current") != std::string::npos);
    REQUIRE (r.snapshot().context.canUndo);
    REQUIRE (snapshotOf (f.bundle) == untouched);

    r.asked.clear();
    r.answers = { f.originals[0] };   // one put back, then Cancel
    r.dispatch (yesdaw::ui::UiActionId::ProjectOpen);
    REQUIRE (r.asked.size() == 2u);
    REQUIRE (r.snapshot().statusLineText == "Open cancelled: 1 audio file still missing (Audio Clip)");
    REQUIRE (r.snapshot().windowTitle.find ("current") != std::string::npos);

    r.asked.clear();
    r.answers = { f.originals[1] };   // the next open asks only about the other
    r.dispatch (yesdaw::ui::UiActionId::ProjectOpen);
    REQUIRE (r.asked.size() == 1u);
    REQUIRE (r.asked[0].description == "Audio Clip - 0:01.0 48 kHz mono, used by 1 clip");
    REQUIRE (r.snapshot().windowTitle.find ("song") != std::string::npos);
}

// SS-6: a drive answers the missing-audio question through the real dialog, so the probe shows the question while it is
// up (its name, the refusal of the last file, damaged or missing), how many were asked, and how the open ended.
TEST_CASE ("SS-6 the probe shows the missing-audio question while it is up and how the relink ended", "[relink][probe]")
{
    ThreeTones f ("probe");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));

    RelinkShell r (f.directory / "session");
    const auto relinkProbe = [&r] {
        juce::var probe;
        REQUIRE (juce::JSON::parse (juce::String (yesdaw::ui::mainComponentStateProbeJson (*r.shell)), probe).wasOk());
        return probe["relink"];
    };
    REQUIRE_FALSE (static_cast<bool> (relinkProbe()["asking"]));
    REQUIRE (relinkProbe()["lastOutcome"].toString().isEmpty());

    std::vector<juce::var> seen;
    r.whileAsking = [&] { seen.push_back (relinkProbe()); };
    r.open = f.bundle;
    r.answers = { f.originals[2], f.originals[0] };   // the wrong tone, then the original
    r.dispatch (yesdaw::ui::UiActionId::ProjectOpen);
    REQUIRE (seen.size() == 2u);
    REQUIRE (static_cast<bool> (seen[0]["asking"]));
    REQUIRE (seen[0]["name"].toString() == "Audio Clip");
    REQUIRE (seen[0]["refusal"].toString().isEmpty());
    REQUIRE_FALSE (static_cast<bool> (seen[0]["damaged"]));
    REQUIRE (seen[1]["refusal"].toString() == "tone-2.wav is not the missing audio: its content differs");
    REQUIRE (static_cast<int> (seen[1]["questions"]) == 2);
    REQUIRE_FALSE (static_cast<bool> (relinkProbe()["asking"]));   // answered: down
    REQUIRE (relinkProbe()["lastOutcome"].toString() == "relinked");

    // Cancel names its outcome too.
    ThreeTones g ("probe-cancel");
    std::filesystem::remove (assetFileOf (g.bundle, g.hashes[0]));
    RelinkShell c (g.directory / "session");
    c.open = g.bundle;
    c.dispatch (yesdaw::ui::UiActionId::ProjectOpen);
    juce::var probe;
    REQUIRE (juce::JSON::parse (juce::String (yesdaw::ui::mainComponentStateProbeJson (*c.shell)), probe).wasOk());
    REQUIRE (probe["relink"]["lastOutcome"].toString() == "cancelled");
    REQUIRE_FALSE (static_cast<bool> (probe["relink"]["asking"]));
}

TEST_CASE ("ADR-0062 the launch reopen asks the same way; a pad's Asset and an unused one are described", "[relink]")
{
    ThreeTones f ("launch");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[2]));
    RelinkShell r (f.directory / "session", f.bundle);   // the launch opens it
    REQUIRE (r.asked.size() == 1u);   // no answer: Cancel
    REQUIRE (r.snapshot().statusLineText == "Open cancelled: 1 audio file still missing (Audio Clip)");
    r.shell.reset();

    RelinkShell relaunch (f.directory / "session-2", f.bundle);
    REQUIRE (relaunch.asked.size() == 1u);
    REQUIRE_FALSE (relaunch.snapshot().context.projectLoaded);   // asked, cancelled: no project, as a failed launch open
    relaunch.shell.reset();
    RelinkShell found (f.directory / "session-3", {});
    found.answers = { f.originals[2] };
    found.open = f.bundle;
    found.dispatch (yesdaw::ui::UiActionId::ProjectOpen);
    REQUIRE (found.snapshot().context.projectLoaded);

    // The wording for audio only a Sampler pad uses, and for audio nothing uses.
    yesdaw::persistence::ProjectBundleDb db;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::openExistingBundle (f.bundle, db).ok());
    yesdaw::engine::Project project;
    REQUIRE (db.readProjectSnapshot (project).ok());
    const yesdaw::engine::EntityId padAsset = project.assets.at (0).id;
    project.clips.erase (std::remove_if (project.clips.begin(), project.clips.end(),
                                         [&] (const yesdaw::engine::Clip& clip) { return clip.assetId == padAsset; }),
                         project.clips.end());
    yesdaw::engine::SamplerPad pad;
    pad.key = 60;
    pad.assetId = padAsset;
    project.tracks.at (0).samplerPads.push_back (pad);
    const std::string hex = yesdaw::persistence::detail::hexBytes (f.hashes[0].bytes).substr (0, 8);
    REQUIRE (yesdaw::ui::describeMissingAsset (project, f.hashes[0], false).description
             == "Asset " + hex + " - 0:00.5 48 kHz mono, used by 1 clip");
    project.tracks.at (0).samplerPads.clear();
    REQUIRE (yesdaw::ui::describeMissingAsset (project, f.hashes[0], false).description
             == "Asset " + hex + " - 0:00.5 48 kHz mono, not used by any clip");
}

TEST_CASE ("ADR-0062 a right file that cannot be written is not called the wrong audio", "[relink]")
{
    ThreeTones f ("io-refusal");
    std::filesystem::remove (assetFileOf (f.bundle, f.hashes[0]));
    // The temporary's place is taken by a folder: the copy fails for a reason that has nothing to do with the content.
    const auto temporary = f.bundle / std::filesystem::path (yesdaw::persistence::detail::assetTempRelativePathForHash (f.hashes[0]));
    std::filesystem::create_directories (temporary / "blocker");
    RelinkShell r (f.directory / "session");
    r.open = f.bundle;
    r.answers = { f.originals[0] };   // the right file, then Cancel
    r.dispatch (yesdaw::ui::UiActionId::ProjectOpen);
    REQUIRE (r.asked.size() == 2u);
    REQUIRE (r.asked[1].refusal.rfind ("tone-0.wav could not be put back: ", 0) == 0);
    REQUIRE (r.asked[1].refusal.find ("is not the missing audio") == std::string::npos);
}
