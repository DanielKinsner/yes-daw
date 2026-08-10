// YES DAW - H11 app smoke gate: bundle load -> action IDs -> playback transport.

#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using Catch::Approx;
using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::EntityId;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::persistence::ProjectBundleDb;
using yesdaw::ui::UiActionId;
using yesdaw::ui::UiAppLoadStatus;
using yesdaw::ui::UiAppLoadResult;
using yesdaw::ui::UiAppModel;
using yesdaw::ui::UiDecodedAsset;

namespace {

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

std::filesystem::path makeTempBundlePath (std::string_view label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("yesdaw-app-smoke-" + std::string (label) + "-" + std::to_string (ticks) + ".yesdaw");

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

std::vector<std::uint8_t> assetBytesForId (EntityId id)
{
    return {
        0x59u,
        0x45u,
        0x53u,
        0x44u,
        0x41u,
        0x57u,
        id.bytes.back(),
        static_cast<std::uint8_t> (id.bytes.back() + 17u),
        static_cast<std::uint8_t> (id.bytes.back() + 43u),
    };
}

AssetContentHash hashBytes (std::span<const std::uint8_t> bytes) noexcept
{
    return yesdaw::persistence::detail::sha256Bytes (bytes);
}

void writeBytes (const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    std::error_code ec;
    std::filesystem::create_directories (path.parent_path(), ec);
    REQUIRE (! ec);

    std::ofstream output (path, std::ios::binary | std::ios::trunc);
    REQUIRE (output.good());
    output.write (reinterpret_cast<const char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    output.close();
    REQUIRE (output.good());
}

Project makeSmokeProject()
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };

    Asset asset;
    asset.id = idFromLowByte (2);
    const std::vector<std::uint8_t> bytes = assetBytesForId (asset.id);
    asset.contentHash = hashBytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
    asset.frames = 16;
    asset.sampleRate = project.sampleRate;
    asset.channels = 1;

    Clip clip;
    clip.id = idFromLowByte (3);
    clip.assetId = asset.id;
    clip.trackId = idFromLowByte (4);
    clip.timelineStart = 0;
    clip.timelineLength = 8;
    clip.srcOffset = 0;
    clip.srcLen = 8;
    clip.gain = 1.0f;
    clip.fadeIn = 0;
    clip.fadeOut = 0;
    clip.timeBase = TimeBase::SampleLocked;

    project.assets = { asset };
    Track track;
    track.id = clip.trackId;
    track.strip.name = "Audio 1";
    project.tracks = { track };
    project.clips = { clip };
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

void writeProjectAssetFiles (const std::filesystem::path& bundlePath, const Project& project)
{
    for (const Asset& asset : project.assets)
    {
        const std::vector<std::uint8_t> bytes = assetBytesForId (asset.id);
        REQUIRE (hashBytes (std::span<const std::uint8_t> (bytes.data(), bytes.size())) == asset.contentHash);
        writeBytes (bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash),
                    std::span<const std::uint8_t> (bytes.data(), bytes.size()));
    }
}

UiDecodedAsset makeDecodedAsset (const Asset& asset)
{
    UiDecodedAsset decoded;
    decoded.assetId = asset.id;
    decoded.sampleRate = asset.sampleRate;
    decoded.frames = asset.frames;
    decoded.channels = asset.channels;
    decoded.interleavedSamples = {
        0.10f, -0.20f, 0.30f, -0.40f,
        0.50f, -0.60f, 0.70f, -0.80f,
        0.90f, -1.00f, 0.80f, -0.70f,
        0.60f, -0.50f, 0.40f, -0.30f,
    };
    return decoded;
}

} // namespace

TEST_CASE ("H11 app model loads a Project bundle and drives transport through action ids",
           "[ui][app][smoke]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("load-transport");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    REQUIRE_FALSE (app.dispatch (UiActionId::TransportPlay).dispatched);
    REQUIRE_FALSE (app.context().projectLoaded);

    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    const UiAppLoadResult loaded = app.loadProjectBundle (
        bundlePath,
        std::span<const UiDecodedAsset> (&decoded, 1));

    REQUIRE (loaded.ok());
    REQUIRE (loaded.status == UiAppLoadStatus::Ok);
    REQUIRE (app.bundlePath() == bundlePath);
    REQUIRE (app.project().id == project.id);
    REQUIRE (app.playbackReady());
    REQUIRE (app.context().projectLoaded);
    REQUIRE_FALSE (app.context().isPlaying);
    REQUIRE_FALSE (app.context().loopEnabled);
    REQUIRE (app.context().playheadFrame == 0);
    REQUIRE (app.registry().stateFor (UiActionId::TransportPlay, app.context()).enabled);

    REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
    REQUIRE (app.context().isPlaying);

    REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
    REQUIRE (app.context().playheadFrame == 0);

    REQUIRE (app.dispatch (UiActionId::TransportToggleLoop).dispatched);
    REQUIRE (app.context().loopEnabled);

    REQUIRE (app.dispatch (UiActionId::TransportStop).dispatched);
    REQUIRE_FALSE (app.context().isPlaying);
    REQUIRE (app.context().commandDispatchCount == 4);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("H17 CP4 autosave scheduling default is on and writeAutosaveTick is a safe no-op",
           "[ui][app][autosave]")
{
    UiAppModel app;

    // The scheduling policy default flows through the model headlessly (the shell reads this to
    // decide whether to start its Timer).
    REQUIRE (app.autosaveSchedule().enabled);
    REQUIRE (app.autosaveSchedule().intervalMs > 0);

    // With no bundle/engine live yet, a scheduled tick must no-op cleanly — not crash, not error.
    REQUIRE (app.writeAutosaveTick().ok());

    // With a real bundle open, a tick still returns ok: a freshly-loaded project is clean, so the
    // underlying write no-ops via PlaybackEngine::needsAutosave rather than doing spurious disk I/O.
    const std::filesystem::path bundlePath = makeTempBundlePath ("autosave-tick");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
    REQUIRE (app.writeAutosaveTick().ok());

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Arrangement verbs drive track lifecycle, cross-track move, deletes, and notes through the app model",
           "[ui][app][arrangement]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("arrangement-verbs");
    Project project = makeSmokeProject();

    yesdaw::engine::MidiClip midiClip;
    midiClip.id = idFromLowByte (5);
    midiClip.trackId = idFromLowByte (4);
    midiClip.timelineStart = 0;
    midiClip.timelineLength = 1024;
    midiClip.timeBase = TimeBase::TempoLocked;
    project.midiClips = { midiClip };
    REQUIRE (project.hasValidAssetClipIndirection());

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());

    // Track lifecycle: add + rename, both undoable and persisted.
    REQUIRE (app.addAudioTrack().dispatched);
    REQUIRE (app.project().tracks.size() == 2u);
    const EntityId newTrackId = app.project().tracks.back().id;
    REQUIRE (app.project().tracks.back().strip.name == "Audio 2");

    REQUIRE (app.renameProjectTrack (newTrackId, "Vox").dispatched);
    REQUIRE (app.project().tracks.back().strip.name == "Vox");

    // Cross-track move through selection.
    REQUIRE (app.selectTimelineClip (idFromLowByte (3)));
    REQUIRE (app.moveSelectedTimelineClipToTrack (newTrackId, 4).dispatched);
    REQUIRE (app.project().clips.front().trackId == newTrackId);
    REQUIRE (app.project().clips.front().timelineStart == 4);

    // Reorder the new track to the front.
    REQUIRE (app.reorderProjectTrack (newTrackId, 0).dispatched);
    REQUIRE (app.project().tracks.front().id == newTrackId);

    // Remove-with-contents: the moved clip dies with its track; selection is cleared.
    REQUIRE (app.removeProjectTrack (newTrackId).dispatched);
    REQUIRE (app.project().tracks.size() == 1u);
    REQUIRE (app.project().clips.empty());
    REQUIRE_FALSE (app.context().timelineClipSelected);

    // A track with MIDI clips refuses removal (no MIDI-clip delete verb yet).
    REQUIRE_FALSE (app.removeProjectTrack (idFromLowByte (4)).dispatched);
    REQUIRE (app.project().tracks.size() == 1u);

    // Note add + delete on the selected MIDI clip.
    REQUIRE (app.selectFirstMidiClip());
    REQUIRE (app.addPianoRollNoteAt (16, 256, 64).dispatched);
    REQUIRE (app.project().midiClips.front().notes.size() == 1u);
    REQUIRE (app.project().midiClips.front().notes.front().key == 64);
    REQUIRE (app.context().midiNoteSelected);

    REQUIRE (app.deleteSelectedPianoRollNote().dispatched);
    REQUIRE (app.project().midiClips.front().notes.empty());
    REQUIRE_FALSE (app.context().midiNoteSelected);

    // Clip delete via the dedicated verb, then the full undo chain restores everything.
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);   // undo note delete
    REQUIRE (app.project().midiClips.front().notes.size() == 1u);

    while (app.context().canUndo)
        REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);

    REQUIRE (app.project().tracks.size() == 1u);
    REQUIRE (app.project().tracks.front().strip.name == "Audio 1");
    REQUIRE (app.project().clips.size() == 1u);
    REQUIRE (app.project().clips.front().trackId == idFromLowByte (4));
    REQUIRE (app.project().clips.front().timelineStart == 0);
    REQUIRE (app.project().midiClips.front().notes.empty());

    // Persistence: reopen the bundle cold and confirm the undone state was saved.
    UiAppModel reopened;
    UiDecodedAsset decodedAgain = makeDecodedAsset (project.assets.front());
    REQUIRE (reopened.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decodedAgain, 1)).ok());
    REQUIRE (reopened.project().tracks.size() == 1u);
    REQUIRE (reopened.project().clips.size() == 1u);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("Deleting the selected timeline clip is undoable and clears selection",
           "[ui][app][arrangement][delete]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("clip-delete");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());

    // Disabled without a selection.
    REQUIRE_FALSE (app.deleteSelectedTimelineClip().dispatched);

    REQUIRE (app.selectTimelineClip (idFromLowByte (3)));
    REQUIRE (app.deleteSelectedTimelineClip().dispatched);
    REQUIRE (app.project().clips.empty());
    REQUIRE_FALSE (app.context().timelineClipSelected);

    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (app.project().clips.size() == 1u);
    REQUIRE (app.project().clips.front().id == idFromLowByte (3));

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("real capture session records device input into a persisted take at the compensated frame",
           "[ui][app][recording][capture]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("real-capture");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());

    // Select a device, arm, then start a capture session with zero-latency device parameters.
    REQUIRE (app.dispatch (UiActionId::DeviceSelectTestAudio).dispatched);
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE_FALSE (app.realRecordingCaptureActive());
    REQUIRE (app.startRealRecordingCapture (1, 48000.0, 0, 0));
    REQUIRE (app.realRecordingCaptureActive());
    REQUIRE (app.context().isRecording);

    // Drive the device callback with a recognizable mono input ramp for 4 blocks of 128 frames.
    std::array<float, 128> input {};
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    const float* inputs[1] = { input.data() };
    for (int block = 0; block < 4; ++block)
    {
        for (int i = 0; i < 128; ++i)
            input[static_cast<std::size_t> (i)] = static_cast<float> (block) + static_cast<float> (i) * 0.001f;
        REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, 128));
        app.drainRealRecordingCapture();
    }

    const yesdaw::ui::UiAppRecordResult committed = app.stopRealRecordingCaptureAndCommit();
    INFO ("record status " << static_cast<int> (committed.status));
    REQUIRE (committed.ok());
    REQUIRE_FALSE (app.realRecordingCaptureActive());
    REQUIRE_FALSE (app.context().isRecording);

    // The committed take is a REAL captured asset: 512 frames at the compensated start (0 with zero
    // latency), placed on the armed track, persisted in the bundle, and selected for editing.
    const yesdaw::ui::UiRecordedAudioTake take = app.lastRecordedAudioTake();
    REQUIRE (take.frames == 512u);
    REQUIRE (take.channels == 1u);
    REQUIRE (take.timelineStart == 0);

    ProjectBundleDb verify;
    REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
    Project persisted;
    REQUIRE (verify.readProjectSnapshot (persisted).ok());
    REQUIRE (persisted.recordingTakes.size() == 1u);
    REQUIRE (persisted.clips.size() == 2u);   // original + recorded
    REQUIRE (persisted.assets.size() == 2u);
    REQUIRE (app.context().timelineClipSelected);

    // Stopping with no captured audio reports an honest failure, never a synthetic take.
    REQUIRE_FALSE (app.stopRealRecordingCaptureAndCommit().ok());

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("rapid same-millisecond FX adds never drop an insert", "[ui][app][fx][rapid]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("rapid-fx");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
    REQUIRE (app.selectMixerTrack (0));

    for (int i = 0; i < 10; ++i)
    {
        const auto result = app.addFxInsertToSelectedStrip (yesdaw::engine::FxKind::Eq);
        INFO ("add " << i << " dispatched " << result.dispatched
              << " reason '" << result.state.disabledReason << "'");
        REQUIRE (result.dispatched);
        REQUIRE (app.project().tracks.front().strip.fxChain.size() == static_cast<std::size_t> (i + 1));
    }

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("metronome clicks land on the beat grid, survive edits, and never reach the export",
           "[ui][app][metronome]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("metronome");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());

    REQUIRE_FALSE (app.context().metronomeEnabled);
    REQUIRE (app.dispatch (UiActionId::TransportToggleMetronome).dispatched);
    REQUIRE (app.context().metronomeEnabled);

    // 120 BPM at 48 kHz -> a beat every 24000 frames. The project's clip is 8 frames long, so any
    // energy at frame 24000+ can only be the click.
    REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
    const std::vector<float> rendered = app.renderPlaybackFrames (30'000, 128);
    REQUIRE_FALSE (rendered.empty());
    const std::size_t channels = 2;

    const auto peakAround = [&] (std::size_t frame, std::size_t span) {
        float peak = 0.0f;
        for (std::size_t f = frame; f < frame + span && f * channels < rendered.size(); ++f)
            peak = std::max (peak, std::abs (rendered[f * channels]));
        return peak;
    };

    REQUIRE (peakAround (24'000, 240) > 0.1f);    // beat 2 click present
    REQUIRE (peakAround (12'000, 240) < 1.0e-6f); // mid-beat silence (clip long gone)

    // The click survives an edit that rebuilds the playback engine.
    REQUIRE (app.dispatch (UiActionId::TransportStop).dispatched);
    REQUIRE (app.addAudioTrack().dispatched);
    REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
    const std::vector<float> renderedAfterEdit = app.renderPlaybackFrames (30'000, 128);
    float editPeak = 0.0f;
    for (std::size_t f = 24'000; f < 24'240; ++f)
        editPeak = std::max (editPeak, std::abs (renderedAfterEdit[f * channels]));
    REQUIRE (editPeak > 0.1f);

    // Offline export NEVER contains the click: the monitoring overlay is playback-only.
    REQUIRE (app.dispatch (UiActionId::TransportStop).dispatched);
    std::filesystem::path exportPath = bundlePath;
    exportPath += "-metro-export.wav";
    REQUIRE (app.exportAudioFile (exportPath).dispatched);
    yesdaw::io::Float32Wav exported;
    REQUIRE (yesdaw::io::readFloat32WavFile (exportPath, exported).ok());
    // The export is timeline-bounded (the 8-frame clip plus tail) — it can never even REACH the
    // 24000-frame beat grid, and everything after the clip is exact digital silence.
    REQUIRE (exported.frames < 20'000u);
    float postClipPeak = 0.0f;
    for (std::size_t f = 16; f < exported.frames; ++f)
        postClipPeak = std::max (postClipPeak, std::abs (exported.interleavedSamples[f * exported.channels]));
    REQUIRE (postClipPeak == 0.0f);

    // Toggle off silences the click again.
    REQUIRE (app.dispatch (UiActionId::TransportToggleMetronome).dispatched);
    REQUIRE_FALSE (app.context().metronomeEnabled);
    REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
    const std::vector<float> renderedOff = app.renderPlaybackFrames (30'000, 128);
    float offPeak = 0.0f;
    for (std::size_t f = 23'900; f < 24'400; ++f)
        offPeak = std::max (offPeak, std::abs (renderedOff[f * channels]));
    REQUIRE (offPeak < 1.0e-6f);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (exportPath, ec);
}

TEST_CASE ("import places the new clip at the playhead", "[ui][app][import][playhead]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("import-at-playhead");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());

    // Locate mid-timeline, then import: the clip starts exactly at the playhead frame.
    REQUIRE (app.locatePlaybackFrame (4'321));
    UiDecodedAsset imported = makeDecodedAsset (project.assets.front());
    imported.assetId = {};
    const std::filesystem::path storedAssetPath =
        bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (project.assets.front().contentHash);
    const auto result = app.importAudioFile (storedAssetPath, std::move (imported));
    REQUIRE (result.ok());

    const Project after = [&] {
        ProjectBundleDb verify;
        REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
        Project loaded;
        REQUIRE (verify.readProjectSnapshot (loaded).ok());
        return loaded;
    }();
    REQUIRE (after.clips.size() == 2u);
    REQUIRE (after.clips.back().timelineStart == 4'321);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("last-project record round-trips through the session-state directory", "[ui][app][lastproject]")
{
    const std::filesystem::path sessionDir = makeTempBundlePath ("session-state");
    const std::filesystem::path bundlePath = makeTempBundlePath ("last-project");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    {
        UiAppModel app;
        app.setSessionStateDirectory (sessionDir);
        REQUIRE (app.readLastProjectRecord().empty());   // nothing recorded yet

        UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
        REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
        REQUIRE (app.readLastProjectRecord() == bundlePath);
    }

    // A fresh model (a relaunch) reads the same record back.
    {
        UiAppModel relaunched;
        relaunched.setSessionStateDirectory (sessionDir);
        REQUIRE (relaunched.readLastProjectRecord() == bundlePath);
    }

    // A record pointing at a deleted bundle reads as empty — the shell never chases ghosts.
    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    {
        UiAppModel afterDelete;
        afterDelete.setSessionStateDirectory (sessionDir);
        REQUIRE (afterDelete.readLastProjectRecord().empty());
    }

    // No session directory set -> record functions are inert (the harness default).
    {
        UiAppModel inert;
        REQUIRE (inert.readLastProjectRecord().empty());
    }

    std::filesystem::remove_all (sessionDir, ec);
}

TEST_CASE ("mute and solo are audible: the strip state drives the playback mute mask",
           "[ui][app][mixer][mute-solo]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("mute-solo-audible");
    Project project = makeSmokeProject();

    // Second track with its own clip referencing the same asset, offset later on the timeline.
    Track second;
    second.id = idFromLowByte (7);
    second.strip.name = "Audio 2";
    project.tracks.push_back (second);
    Clip clip2 = project.clips.front();
    clip2.id = idFromLowByte (8);
    clip2.trackId = second.id;
    project.clips.push_back (clip2);
    REQUIRE (project.hasValidAssetClipIndirection());

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());

    const auto renderPeak = [&] {
        REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
        REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
        const std::vector<float> rendered = app.renderPlaybackFrames (64, 32);
        REQUIRE (app.dispatch (UiActionId::TransportStop).dispatched);
        float peak = 0.0f;
        for (const float sample : rendered)
            peak = std::max (peak, std::abs (sample));
        return peak;
    };

    const float bothAudible = renderPeak();
    REQUIRE (bothAudible > 0.1f);

    // Mute track 1: the output drops (half the summed signal is gone), never rises.
    REQUIRE (app.selectMixerTrack (0));
    REQUIRE (app.toggleSelectedMixerMute().dispatched);
    const float mutedPeak = renderPeak();
    REQUIRE (mutedPeak < bothAudible);
    REQUIRE (mutedPeak > 0.0f);   // track 2 still plays

    // Solo the MUTED track 1: per ADR-0014 a muted target's solo does NOT engage solo for the
    // others, so track 2 keeps playing exactly as before.
    REQUIRE (app.toggleSelectedMixerSolo().dispatched);
    const float mutedSoloPeak = renderPeak();
    REQUIRE (mutedSoloPeak == Approx (mutedPeak).margin (1.0e-6));

    // Unmute: solo isolates track 1 alone -> audible again, and quieter than both together.
    REQUIRE (app.toggleSelectedMixerMute().dispatched);
    const float soloPeak = renderPeak();
    REQUIRE (soloPeak > 0.1f);
    REQUIRE (soloPeak < bothAudible + 1.0e-6f);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

namespace {

struct WavHeaderFields
{
    std::uint16_t format = 0;
    std::uint16_t channels = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint32_t dataBytes = 0;
};

WavHeaderFields readWavHeaderFields (const std::filesystem::path& path)
{
    WavHeaderFields fields;
    std::ifstream in (path, std::ios::binary);
    REQUIRE (in.good());
    std::array<unsigned char, 44> header {};
    in.read (reinterpret_cast<char*> (header.data()), static_cast<std::streamsize> (header.size()));
    REQUIRE (in.good());
    const auto le16 = [&header] (std::size_t offset) {
        return static_cast<std::uint16_t> (header[offset]
                                           | (static_cast<std::uint16_t> (header[offset + 1]) << 8u));
    };
    const auto le32 = [&header] (std::size_t offset) {
        return static_cast<std::uint32_t> (header[offset])
             | (static_cast<std::uint32_t> (header[offset + 1]) << 8u)
             | (static_cast<std::uint32_t> (header[offset + 2]) << 16u)
             | (static_cast<std::uint32_t> (header[offset + 3]) << 24u);
    };
    fields.format = le16 (20);
    fields.channels = le16 (22);
    fields.bitsPerSample = le16 (34);
    fields.dataBytes = le32 (40);
    return fields;
}

} // namespace

TEST_CASE ("export options write PCM bit depths and slice the loop range", "[ui][app][export-options]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("export-options");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());

    // 16-bit PCM export carries the PCM format tag and the full 8-frame project.
    std::filesystem::path wav16 = bundlePath;
    wav16 += "-16.wav";
    app.setExportBitDepth (UiAppModel::UiExportBitDepth::Int16);
    REQUIRE (app.exportAudioFile (wav16).dispatched);
    const WavHeaderFields fields16 = readWavHeaderFields (wav16);
    REQUIRE (fields16.format == 1u);
    REQUIRE (fields16.bitsPerSample == 16u);
    REQUIRE (fields16.channels > 0u);
    REQUIRE (fields16.dataBytes == 8u * fields16.channels * 2u);

    // Loop-range export without a loop region is an honest failure, not a silent full export.
    std::filesystem::path wavLoop = bundlePath;
    wavLoop += "-loop.wav";
    app.setExportLoopRangeOnly (true);
    REQUIRE_FALSE (app.exportAudioFile (wavLoop).dispatched);
    REQUIRE_FALSE (std::filesystem::exists (wavLoop));

    // With a loop region set, the export is exactly the loop's frames at 24-bit PCM.
    REQUIRE (app.setPlaybackLoopRegion (2, 6).dispatched);
    app.setExportBitDepth (UiAppModel::UiExportBitDepth::Int24);
    REQUIRE (app.exportAudioFile (wavLoop).dispatched);
    const WavHeaderFields fieldsLoop = readWavHeaderFields (wavLoop);
    REQUIRE (fieldsLoop.format == 1u);
    REQUIRE (fieldsLoop.bitsPerSample == 24u);
    REQUIRE (fieldsLoop.channels == fields16.channels);
    REQUIRE (fieldsLoop.dataBytes == 4u * fieldsLoop.channels * 3u);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
    std::filesystem::remove (wav16, ec);
    std::filesystem::remove (wavLoop, ec);
}

TEST_CASE ("midi-only project pencils a note and renders it audibly", "[ui][app][midi-only]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("midi-only");

    UiAppModel app;
    REQUIRE (app.createProjectBundle (bundlePath).ok());

    REQUIRE (app.addMidiClipAtPlayhead().dispatched);
    REQUIRE (app.project().midiClips.size() == 1u);

    REQUIRE (app.addPianoRollNoteAt (0, 24'000, 69).dispatched);
    REQUIRE (app.project().midiClips.front().notes.size() == 1u);

    REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
    const std::vector<float> rendered = app.renderPlaybackFrames (24'000, 512);
    double energy = 0.0;
    for (const float sample : rendered)
        energy += std::abs (static_cast<double> (sample));
    REQUIRE (energy > 1.0);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}
