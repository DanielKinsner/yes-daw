// YES DAW - H11 app smoke gate: bundle load -> action IDs -> playback transport.

#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

TEST_CASE ("playing MIDI note edits preserve held instrument and FX state, publish the changed onset and undo durably",
           "[ui][app][midi-live]")
{
    using namespace yesdaw::engine;
    const auto entity = [] (std::uint8_t value) {
        EntityId id;
        id.bytes.back() = value;
        return id;
    };
    Project song;
    song.id = entity (1);
    song.sampleRate = SampleRate { 48000.0 };
    song.tempoMap = { TempoChange { 0, 120.0 } };
    song.meterMap = { MeterChange { 0, 4, 4 } };
    Track track;
    track.id = entity (4);
    track.strip.name = "MIDI continuity";
    track.instrumentKind = TrackInstrumentKind::SimpleSynth;
    FxInsert delay;
    delay.id = entity (6);
    delay.kind = FxKind::Delay;
    track.strip.fxChain.push_back (delay);
    song.tracks.push_back (track);
    MidiClip clip;
    clip.id = entity (7);
    clip.trackId = track.id;
    clip.timeBase = TimeBase::SampleLocked;
    clip.timelineLength = 48000;
    Note held;
    held.id = entity (8);
    held.startTick = 0;
    held.lengthTicks = 36000;
    held.key = 60;
    held.pitchNote = 60.0;
    held.normalizedVelocity = 0.6;
    Note later = held;
    later.id = entity (9);
    later.startTick = 24000;
    later.lengthTicks = 2048;
    later.key = 67;
    later.pitchNote = 67.0;
    clip.notes = { held, later };
    song.midiClips.push_back (clip);
    REQUIRE (song.hasValidAssetClipIndirection());

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto bundle = std::filesystem::temp_directory_path()
        / ("yesdaw-midi-live-" + std::to_string (stamp) + ".yesdaw");
    {
        yesdaw::persistence::ProjectBundleDb db;
        REQUIRE (yesdaw::persistence::ProjectBundleDb::openOrCreateBundle (bundle, db).ok());
        REQUIRE (db.writeProjectSnapshot (song).ok());
    }
    {
        yesdaw::ui::UiAppModel edited;
        yesdaw::ui::UiAppModel control;
        REQUIRE (edited.loadProjectBundle (bundle, {}).ok());
        REQUIRE (control.loadProjectBundle (bundle, {}).ok());
        REQUIRE (edited.dispatch (yesdaw::ui::UiActionId::TransportPlay).dispatched);
        REQUIRE (control.dispatch (yesdaw::ui::UiActionId::TransportPlay).dispatched);
        const auto render = [] (yesdaw::ui::UiAppModel& model) {
            std::array<float, 256> block {};
            float* outputs[2] = { block.data(), block.data() + 128 };
            REQUIRE (model.processDeviceAudioBlock (outputs, 2, 128));
            model.serviceLiveMidiEdits();
            model.refreshTransportSnapshot();
            return block;
        };
        bool sounded = false;
        for (int block = 0; block < 128; ++block)
        {
            const auto expected = render (control);
            REQUIRE (render (edited) == expected);
            for (float value : expected)
                sounded = sounded || value != 0.0f;
        }
        REQUIRE (sounded);
        REQUIRE (edited.selectPianoRollNote (clip.id, later.id).dispatched);
        const auto replacements = edited.playbackReplaceCount();
        const auto refuseSave = [&] (bool refuse) {
            sqlite3* connection = nullptr;
            const std::string dbPath = yesdaw::io::utf8Text (bundle / "project.db");
            REQUIRE (sqlite3_open (dbPath.c_str(), &connection) == SQLITE_OK);
            const char* sql = refuse
                ? "CREATE TRIGGER refuse_live_note_save BEFORE DELETE ON midi_notes BEGIN SELECT RAISE(ABORT, 'save refused'); END;"
                : "DROP TRIGGER refuse_live_note_save;";
            REQUIRE (sqlite3_exec (connection, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
            REQUIRE (sqlite3_close (connection) == SQLITE_OK);
        };
        refuseSave (true);
        REQUIRE_FALSE (edited.moveSelectedPianoRollNotesBy (1024, 0).dispatched);
        REQUIRE (edited.project().midiClips[0].notes == clip.notes);
        REQUIRE_FALSE (edited.context().canUndo);
        REQUIRE (edited.playbackReplaceCount() == replacements);
        REQUIRE (render (edited) == render (control));
        refuseSave (false);
        REQUIRE (edited.moveSelectedPianoRollNotesBy (2048, 0).dispatched);
        REQUIRE (edited.playbackReplaceCount() == replacements);
        bool changedOnsetHeard = false;
        for (int block = 0; block < 100; ++block)
        {
            const auto begin = control.context().playheadFrame;
            const auto expected = render (control);
            const auto actual = render (edited);
            REQUIRE (edited.context().isPlaying);
            REQUIRE (edited.context().playheadFrame == control.context().playheadFrame);
            if (begin + 128 <= later.startTick)
                REQUIRE (actual == expected);   // running oscillator, envelope and feedback history survive
            else if (begin >= later.startTick && begin + 128 <= later.startTick + 2048)
                changedOnsetHeard = changedOnsetHeard || actual != expected;
        }
        REQUIRE (changedOnsetHeard);   // keeping the old schedule is not a passing implementation
        REQUIRE (edited.dispatch (yesdaw::ui::UiActionId::EditUndo).dispatched);
        REQUIRE (edited.project().midiClips[0].notes[1].startTick == later.startTick);
        REQUIRE (edited.playbackReplaceCount() == replacements);
        (void) render (edited);
        REQUIRE (edited.dispatch (yesdaw::ui::UiActionId::EditRedo).dispatched);
        REQUIRE (edited.playbackReplaceCount() == replacements);
        (void) render (edited);
        // The existing no-topology budget also applies to a hundred consecutive note edits.
        for (int index = 0; index < 100; ++index)
        {
            REQUIRE (edited.moveSelectedPianoRollNotesBy (index % 2 == 0 ? 32 : -32, 0).dispatched);
            (void) render (edited);
            REQUIRE (edited.playbackReplaceCount() == replacements);
        }
        REQUIRE (edited.playbackLiveMidiSchedulesApplied() == 103);
    }
    {
        yesdaw::ui::UiAppModel reopened;
        REQUIRE (reopened.loadProjectBundle (bundle, {}).ok());
        REQUIRE (reopened.project().midiClips[0].notes[1].startTick == later.startTick + 2048);
    }
    std::error_code error;
    REQUIRE (std::filesystem::equivalent (bundle.parent_path(), std::filesystem::temp_directory_path()));
    REQUIRE (bundle.filename().string().starts_with ("yesdaw-midi-live-"));
    std::filesystem::remove_all (bundle, error);
    REQUIRE_FALSE (error);
}

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

// M11: three real Tracks so multi-track arming is gated on a 3+ track fixture. Track 0 keeps the
// smoke project's asset+clip; tracks 1 and 2 start empty, exactly like freshly added tracks.
Project makeThreeTrackRecordingProject()
{
    Project project = makeSmokeProject();

    Track second;
    second.id = idFromLowByte (0x21);
    second.strip.name = "Audio 2";
    Track third;
    third.id = idFromLowByte (0x22);
    third.strip.name = "Audio 3";
    project.tracks.push_back (second);
    project.tracks.push_back (third);

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
    UiDecodedAsset decoded = UiDecodedAsset::fromInterleaved (asset.id, asset.sampleRate, asset.channels, {
        0.10f, -0.20f, 0.30f, -0.40f,
        0.50f, -0.60f, 0.70f, -0.80f,
        0.90f, -1.00f, 0.80f, -0.70f,
        0.60f, -0.50f, 0.40f, -0.30f,
    });
    decoded.frames = asset.frames;   // the helper's callers pair it with their Asset row as before
    return decoded;
}

Project makeWholeClipClipboardProject (TimeBase midiTimeBase = TimeBase::SampleLocked)
{
    Project project = makeThreeTrackRecordingProject();
    project.clips.front().timelineStart = 300;
    project.clips.front().name = "Audio clipboard source";
    project.clips.front().gain = 0.7f;
    project.tracks[1].instrumentKind = yesdaw::engine::TrackInstrumentKind::SimpleSynth;

    yesdaw::engine::MidiClip midi;
    midi.id = idFromLowByte (0x31);
    midi.trackId = project.tracks[1].id;
    midi.timelineStart = 125;
    midi.timelineLength = 96;
    midi.timeBase = midiTimeBase;
    midi.muted = true;
    midi.transposeSemitones = -9;
    midi.velocityOffset = -0.2;
    midi.loopLengthTicks = 24;
    midi.notes = {
        { idFromLowByte (0x32), 9, 17, 67, 67.25, 0.61, 3, 9 },
        { idFromLowByte (0x33), 32, 0, 52, 51.75, 0.34, 1, 2 },
    };
    using yesdaw::engine::MidiControlKind;
    midi.controlEvents = {
        { idFromLowByte (0x40), 18, MidiControlKind::ControlChange, 64, 0.8, 2, 7 },
        { idFromLowByte (0x41), 19, MidiControlKind::PitchBend, 0, -0.3, 1, 6 },
        { idFromLowByte (0x42), 20, MidiControlKind::ChannelPressure, 0, 0.4, 4, 5 },
        { idFromLowByte (0x43), 21, MidiControlKind::PolyPressure, 52, 0.6, 3, 4 },
        { idFromLowByte (0x44), 22, MidiControlKind::ProgramChange, 17, 0.0, 2, 3 },
    };
    project.midiClips.push_back (midi);
    midi.id = idFromLowByte (0x60);
    midi.timelineStart = 500;
    midi.transposeSemitones = 12;
    midi.notes.front().key = 43;
    for (std::size_t index = 0; index < midi.notes.size(); ++index)
        midi.notes[index].id = idFromLowByte (static_cast<std::uint8_t> (0x61u + index));
    for (std::size_t index = 0; index < midi.controlEvents.size(); ++index)
        midi.controlEvents[index].id = idFromLowByte (static_cast<std::uint8_t> (0x70u + index));
    project.midiClips.push_back (midi);   // an unselected Clip also seeds the refusal test's old clipboard
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

void requireFreshWholeMidiCopy (const yesdaw::engine::MidiClip& copy,
                               const yesdaw::engine::MidiClip& source,
                               EntityId trackId,
                               yesdaw::engine::Tick start,
                               const Project& original)
{
    std::vector<EntityId> ids;
    for (const auto& midi : original.midiClips)
    {
        ids.push_back (midi.id);
        for (const auto& note : midi.notes)
            ids.push_back (note.id);
        for (const auto& control : midi.controlEvents)
            ids.push_back (control.id);
    }
    const auto requireFreshId = [&] (EntityId id) {
        REQUIRE (id.isValid());
        REQUIRE (std::find (ids.begin(), ids.end(), id) == ids.end());
        ids.push_back (id);
    };
    requireFreshId (copy.id);
    REQUIRE (copy.trackId == trackId);
    REQUIRE (copy.timelineStart == start);
    REQUIRE (copy.notes.size() == source.notes.size());
    REQUIRE (copy.controlEvents.size() == source.controlEvents.size());
    auto expected = source;
    expected.id = copy.id;
    expected.trackId = trackId;
    expected.timelineStart = start;
    for (std::size_t index = 0; index < copy.notes.size(); ++index)
    {
        requireFreshId (copy.notes[index].id);
        expected.notes[index].id = copy.notes[index].id;
    }
    for (std::size_t index = 0; index < copy.controlEvents.size(); ++index)
    {
        requireFreshId (copy.controlEvents[index].id);
        expected.controlEvents[index].id = copy.controlEvents[index].id;
    }
    REQUIRE (copy == expected);
}

} // namespace

TEST_CASE ("stretch requests leave committed geometry and playback unchanged until serviced",
           "[ui][app][stretch-background]")
{
    const auto bundlePath = makeTempBundlePath ("stretch-background");
    Project song = makeSmokeProject();
    song.assets.front().frames = 4800;
    song.assets.front().sampleRate = SampleRate { 44100.0 };
    song.clips.front().srcLen = 4001;
    song.clips.front().timelineLength = yesdaw::engine::timelineLengthForSource (4001, 48000.0 / 44100.0, 1.0);
    song.clips.front().srcOffset = 143;
    Clip other = song.clips.front();
    other.id = idFromLowByte (5);
    other.timelineStart = 10000;
    song.clips.push_back (other);
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (song).ok());
        writeProjectAssetFiles (bundlePath, song);
    }
    const auto decoded = UiDecodedAsset::fromInterleaved (song.assets.front().id,
        song.assets.front().sampleRate, 1, std::vector<float> (4800, 0.125f));
    {
        UiAppModel app;
        REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
        REQUIRE (app.selectTimelineClip (song.clips.front().id));
        REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
        const auto replacements = app.playbackReplaceCount();
        REQUIRE (app.setSelectedTimelineClipStretchFactor (1.5f).dispatched);
        REQUIRE (app.project().clips.front() == song.clips.front());
        REQUIRE_FALSE (app.context().canUndo);
        REQUIRE (app.playbackReplaceCount() == replacements);
        REQUIRE (app.context().isPlaying);
        REQUIRE (app.stretchPreparationPending());
        REQUIRE (app.context().stretchPreparationPending);
        REQUIRE (app.statusLineText().find ("Preparing") != std::string::npos);
        UiAppModel unchanged;
        REQUIRE (unchanged.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
        REQUIRE (unchanged.dispatch (UiActionId::TransportPlay).dispatched);
        const auto render = [] (UiAppModel& model) {
            std::array<float, 256> block {};
            float* outputs[2] = { block.data(), block.data() + 128 };
            REQUIRE (model.processDeviceAudioBlock (outputs, 2, 128));
            return block;
        };
        REQUIRE (render (app) == render (unchanged)); // in-memory processing; no devices or native shell
        const auto drain = [&] {
            std::size_t adoptions = 0;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (10);
            while ((app.stretchPreparationPending() || app.stretchPreparationRetiring())
                && std::chrono::steady_clock::now() < deadline)
            {
                adoptions += app.serviceStretchPreparation() ? 1u : 0u;
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
            }
            REQUIRE_FALSE (app.stretchPreparationPending());
            REQUIRE_FALSE (app.stretchPreparationRetiring());
            return adoptions;
        };
        app.serviceStretchPreparation();
        REQUIRE (app.project().clips == song.clips); // first service only starts the owned worker

        SECTION ("current-project adoption captures target, preserves gain/name, and is one durable undo step")
        {
            REQUIRE (app.setSelectedTimelineClipGain (0.4f).dispatched);
            REQUIRE (app.renameSelectedTimelineClip ("Kept while preparing").dispatched);
            REQUIRE (app.stretchPreparationPending());
            REQUIRE (app.selectTimelineClip (other.id));
            const auto edits = app.context().timelineEditCount;
            REQUIRE (drain() == 1u);
            const auto landed = app.project().clips.front();
            REQUIRE (landed.stretchFactor == 1.5f);
            REQUIRE (landed.gain == 0.4f);
            REQUIRE (landed.name.asView() == "Kept while preparing");
            REQUIRE (landed.timelineLength == yesdaw::engine::timelineLengthForSource (4001, 48000.0 / 44100.0, 1.5));
            REQUIRE (landed.srcOffset == 143);
            REQUIRE (landed.srcLen == 4001);
            REQUIRE (app.project().clips.back() == other);
            REQUIRE (app.selectedTimelineClipId() == other.id);
            REQUIRE (app.context().timelineEditCount == edits + 1);
            REQUIRE (app.context().isPlaying);
            app.serviceStretchPreparation();
            REQUIRE (app.context().timelineEditCount == edits + 1);
            REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
            REQUIRE (app.project().clips.front().stretchFactor == 1.0f);
            REQUIRE (app.project().clips.front().gain == 0.4f);
            REQUIRE (app.project().clips.front().name == landed.name);
            REQUIRE (app.dispatch (UiActionId::EditRedo).dispatched);
            REQUIRE (app.project().clips.front() == landed);
            UiAppModel reopened;
            REQUIRE (reopened.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
            REQUIRE (reopened.project().clips.front() == landed);
            // The DSP window maps both source endpoints; saved geometry retains the ratio law.
            const auto window = yesdaw::engine::mapWindowToView (143, 4001, 48000.0 / 44100.0,
                yesdaw::engine::resampledFrameCount (4800, 44100.0, 48000.0));
            REQUIRE (window.srcLen == 4354);
            REQUIRE (yesdaw::engine::viewFrameFor (4001, 48000.0 / 44100.0) == 4355);
        }
        SECTION ("save refusal preserves committed geometry, undo, playback and durable rows")
        {
            sqlite3* connection = nullptr;
            const auto dbPath = yesdaw::io::utf8Text (bundlePath / "project.db");
            REQUIRE (sqlite3_open (dbPath.c_str(), &connection) == SQLITE_OK);
            REQUIRE (sqlite3_exec (connection,
                "CREATE TRIGGER refuse_stretch_save BEFORE DELETE ON clips BEGIN SELECT RAISE(ABORT, 'save refused'); END;",
                nullptr, nullptr, nullptr) == SQLITE_OK);
            REQUIRE (drain() == 0u);
            REQUIRE (app.project().clips == song.clips);
            REQUIRE_FALSE (app.context().canUndo);
            REQUIRE (app.playbackReplaceCount() == replacements);
            REQUIRE (app.context().isPlaying);
            REQUIRE (app.statusLineIsError());
            REQUIRE (render (app) == render (unchanged));
            REQUIRE (app.statusLineText().find ("did not persist") != std::string::npos);
            REQUIRE (sqlite3_exec (connection, "DROP TRIGGER refuse_stretch_save;", nullptr, nullptr, nullptr) == SQLITE_OK);
            REQUIRE (sqlite3_close (connection) == SQLITE_OK);
            UiAppModel reopened;
            REQUIRE (reopened.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
            REQUIRE (reopened.project().clips == song.clips);
        }
        SECTION ("cancel retires the worker without changing status or committed state")
        {
            REQUIRE (app.dispatch (UiActionId::TimelineClipCancelStretch).dispatched);
            REQUIRE_FALSE (app.stretchPreparationPending());
            REQUIRE (app.stretchPreparationRetiring());
            const auto status = app.statusLineText();
            drain();
            REQUIRE (app.statusLineText() == status);
            REQUIRE (app.project().clips == song.clips);
            REQUIRE_FALSE (app.context().canUndo);
        }
        SECTION ("supersession coalesces replacement intents behind the retiring worker")
        {
            REQUIRE (app.setSelectedTimelineClipStretchFactor (0.6f).dispatched);
            REQUIRE (app.stretchPreparationRetiring());
            REQUIRE (app.setSelectedTimelineClipStretchFactor (0.75f).dispatched);
            REQUIRE (app.project().clips == song.clips);
            drain();
            REQUIRE (app.project().clips.front().stretchFactor == 0.75f);
            REQUIRE (app.context().timelineEditCount == 1);
            REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
            REQUIRE (app.project().clips == song.clips);
            REQUIRE_FALSE (app.context().canUndo);
            REQUIRE (app.dispatch (UiActionId::EditRedo).dispatched);
            REQUIRE (app.project().clips.front().stretchFactor == 0.75f);
        }
        SECTION ("move and undo cannot revive stale absolute-end intent")
        {
            // Supersede the percentage request with an absolute-end gesture on the same target.
            REQUIRE (app.stretchSelectedTimelineClipTo (6000).dispatched);
            REQUIRE (app.moveSelectedTimelineClipTo (100).dispatched);
            REQUIRE_FALSE (app.stretchPreparationPending());
            REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
            REQUIRE (app.project().clips == song.clips);
            drain();
            REQUIRE (app.project().clips == song.clips);
        }
        SECTION ("source-window edits invalidate before undo restores the old window")
        {
            REQUIRE (app.slipSelectedTimelineClipBy (20).dispatched);
            REQUIRE_FALSE (app.stretchPreparationPending());
            REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
            drain();
            REQUIRE (app.project().clips == song.clips);
        }
        SECTION ("deleted targets stay invalid after undo recreates them")
        {
            REQUIRE (app.deleteSelectedTimelineClip().dispatched);
            REQUIRE_FALSE (app.stretchPreparationPending());
            REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
            drain();
            REQUIRE (app.project().clips == song.clips);
        }
        SECTION ("project replacement invalidates even with identical Project and source identities")
        {
            REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
            REQUIRE_FALSE (app.stretchPreparationPending());
            drain();
            REQUIRE (app.project().clips == song.clips);
            REQUIRE_FALSE (app.context().canUndo);
        }
        SECTION ("nonfinite requests are refused without superseding a valid intent")
        {
            const auto refused = app.setSelectedTimelineClipStretchFactor (std::numeric_limits<float>::quiet_NaN());
            REQUIRE_FALSE (refused.dispatched);
            REQUIRE (std::string_view (refused.state.disabledReason).find ("finite") != std::string_view::npos);
            REQUIRE (app.stretchPreparationPending());
            drain();
            REQUIRE (app.project().clips.front().stretchFactor == 1.5f);
        }
    }
    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("a loaded stretched neighbour keeps its prepared audio when another Clip is stretched",
           "[ui][app][stretch-background][stretch-loaded]")
{
    const auto bundlePath = makeTempBundlePath ("stretch-loaded");
    Project song = makeSmokeProject();
    song.assets.front().frames = 4800;
    song.assets.front().sampleRate = SampleRate { 44100.0 };
    auto& first = song.clips.front();
    first.srcOffset = 143;
    first.srcLen = 4001;
    first.timelineLength = yesdaw::engine::timelineLengthForSource (4001, 48000.0 / 44100.0, 1.0);
    Clip other = first;
    other.id = idFromLowByte (5);
    other.timelineStart = 10000;
    other.stretchFactor = 0.75f;
    other.timelineLength = yesdaw::engine::timelineLengthForSource (4001, 48000.0 / 44100.0, 0.75);
    song.clips.push_back (other);
    SECTION ("the loaded neighbour is audible") {}
    SECTION ("a muted neighbour needs no compiler preparation")
    {
        other.muted = true;
        song.clips.back() = other;
    }
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (song).ok());
        writeProjectAssetFiles (bundlePath, song);
    }
    const auto decoded = UiDecodedAsset::fromInterleaved (song.assets.front().id,
        song.assets.front().sampleRate, 1, std::vector<float> (4800, 0.125f));
    {
        UiAppModel app;
        REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
        REQUIRE (app.selectTimelineClip (song.clips.front().id));
        REQUIRE (app.setSelectedTimelineClipStretchFactor (1.5f).dispatched);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (10);
        while (app.stretchPreparationPending() && std::chrono::steady_clock::now() < deadline)
        {
            app.serviceStretchPreparation();
            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
        REQUIRE_FALSE (app.stretchPreparationPending());
        INFO (app.statusLineText());
        REQUIRE (app.project().clips.front().stretchFactor == 1.5f);
        REQUIRE (app.project().clips.back() == other);
        REQUIRE_FALSE (app.statusLineIsError());
        REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
        REQUIRE (app.project().clips == song.clips);
        REQUIRE (app.dispatch (UiActionId::EditRedo).dispatched);
        REQUIRE (app.project().clips.front().stretchFactor == 1.5f);
        REQUIRE (app.project().clips.back() == other);
    }
    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("playback build returns the immutable prepared stretch owner it actually reused",
           "[ui][app][stretch-background][stretch-owner]")
{
    Project song = makeSmokeProject();
    auto& clip = song.clips.front();
    clip.stretchFactor = 1.5f;
    clip.timelineLength = 12;
    const auto decoded = makeDecodedAsset (song.assets.front());
    const yesdaw::engine::DecodedAssetAudio view { decoded.assetId, decoded.sampleRate,
        decoded.frames, decoded.channels, decoded.interleaved() };
    auto samples = std::make_shared<yesdaw::engine::AssetSamples>();
    samples->channels = 1;
    samples->frames = 12;
    samples->interleaved.assign (12, 0.375f);
    yesdaw::engine::OfflineRenderOptions options;
    options.assetOwners.push_back ({ decoded.assetId, decoded.samples });
    options.stretchOwners.push_back ({ clip.id, clip.assetId, clip.srcOffset, clip.srcLen,
                                      clip.stretchFactor, samples });
    auto built = yesdaw::engine::PlaybackEngine::create (song,
        std::span<const yesdaw::engine::DecodedAssetAudio> (&view, 1), options);
    REQUIRE (built.ok());
    REQUIRE (built.preparedStretches.size() == 1u);
    REQUIRE (built.preparedStretches.front().samples == samples);
    REQUIRE (built.preparedStretches.front().clipId == clip.id);
}

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

TEST_CASE ("H17 CP4 autosave scheduling default is on; writeAutosaveTick writes nothing until an edit, then writes it",
           "[ui][app][autosave]")
{
    UiAppModel app;

    // The scheduling policy default flows through the model headlessly (the shell reads this to
    // decide whether to start its Timer).
    REQUIRE (app.autosaveSchedule().enabled);
    REQUIRE (app.autosaveSchedule().intervalMs > 0);

    // With no bundle/engine live yet, a scheduled tick must no-op cleanly — not crash, not error.
    REQUIRE (app.writeAutosaveTick().ok());

    // With a real bundle open, a freshly loaded project is clean: a tick writes nothing (ADR-0068 §2: nothing since the
    // last Save). After an edit the next tick writes the autosave.
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
    REQUIRE (app.autosaveWrites() == 0);
    REQUIRE_FALSE (yesdaw::persistence::autosave_detail::anySnapshotSlotExists (bundlePath));
    REQUIRE (app.addAudioTrack().dispatched);
    REQUIRE (app.writeAutosaveTick().ok());
    REQUIRE (app.autosaveWrites() == 1);
    REQUIRE (yesdaw::persistence::autosave_detail::anySnapshotSlotExists (bundlePath));

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

TEST_CASE ("offered whole-Clip Cut removes MIDI and mixed selections durably as one complete clipboard edit",
           "[ui][app][clipboard][midi-clip-cut]")
{
    bool includeAudio = false;
    TimeBase midiTimeBase = TimeBase::SampleLocked;
    SECTION ("MIDI-only SampleLocked selection") {}
    SECTION ("MIDI-only TempoLocked selection") { midiTimeBase = TimeBase::TempoLocked; }
    SECTION ("mixed audio and MIDI selection") { includeAudio = true; }

    const auto bundlePath = makeTempBundlePath ("midi-clip-cut");
    const Project original = makeWholeClipClipboardProject (midiTimeBase);
    UiAppModel app;
    REQUIRE (app.createProjectBundle (bundlePath, original).ok());
    writeProjectAssetFiles (bundlePath, original);
    const UiDecodedAsset decoded = makeDecodedAsset (original.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
    const auto requireDurableRows = [&] (const Project& expected) {
        UiAppModel reopened;
        REQUIRE (reopened.openProjectBundle (bundlePath).ok());
        REQUIRE (reopened.project().clips == expected.clips);
        REQUIRE (reopened.project().midiClips == expected.midiClips);
        REQUIRE (reopened.project().tracks == expected.tracks);
        REQUIRE (reopened.project().assets == expected.assets);
    };
    REQUIRE (app.selectMixerTrack (includeAudio ? 0u : 1u, false));
    // Put audio first so the old mixed path applies a scratch audio deletion before refusing MIDI.
    if (includeAudio)
    {
        REQUIRE (app.selectTimelineClip (original.clips.front().id));
        REQUIRE (app.selectTimelineClipForGesture (original.midiClips.front().id, true));
    }
    else
        REQUIRE (app.selectTimelineClip (original.midiClips.front().id));
    REQUIRE (app.context().activePanel == yesdaw::ui::UiPanel::Timeline);
    REQUIRE (app.registry().stateFor (UiActionId::TimelineClipCut, app.context()).enabled);
    REQUIRE_FALSE (app.context().canUndo);
    const auto commands = app.context().commandDispatchCount;
    const auto edits = app.context().timelineEditCount;
    REQUIRE (app.dispatch (UiActionId::TimelineClipCut).dispatched);
    REQUIRE (app.context().commandDispatchCount == commands + 1);
    REQUIRE (app.context().timelineEditCount == edits + 1);
    REQUIRE (app.context().clipboardHasClip);
    REQUIRE_FALSE (app.context().timelineClipSelected);
    REQUIRE (app.selectedTimelineClipCount() == 0u);
    Project cut = original;
    cut.midiClips.erase (cut.midiClips.begin());
    if (includeAudio)
        cut.clips.clear();
    REQUIRE (app.project().clips == cut.clips);
    REQUIRE (app.project().midiClips == cut.midiClips);
    requireDurableRows (cut);

    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (app.project().clips == original.clips);
    REQUIRE (app.project().midiClips == original.midiClips);
    REQUIRE_FALSE (app.context().canUndo);   // the entire Cut was one transaction
    REQUIRE (app.context().canRedo);
    requireDurableRows (original);
    REQUIRE (app.dispatch (UiActionId::EditRedo).dispatched);
    REQUIRE (app.project().clips == cut.clips);
    REQUIRE (app.project().midiClips == cut.midiClips);
    REQUIRE_FALSE (app.context().canRedo);

    REQUIRE (app.locatePlaybackFrame (1000));
    REQUIRE (app.dispatch (UiActionId::TimelineClipPaste).dispatched);
    REQUIRE (app.project().midiClips.size() == cut.midiClips.size() + 1u);
    requireFreshWholeMidiCopy (app.project().midiClips.back(), original.midiClips.front(),
                              original.midiClips.front().trackId, 1000, original);
    REQUIRE (app.project().midiClips.front() == original.midiClips.back());
    if (includeAudio)
    {
        REQUIRE (app.project().clips.size() == 1u);
        Clip expected = original.clips.front();
        REQUIRE (app.project().clips.front().id != expected.id);
        expected.id = app.project().clips.front().id;
        expected.timelineStart = 1000 + original.clips.front().timelineStart - original.midiClips.front().timelineStart;
        REQUIRE (app.project().clips.front() == expected);
    }
    else
        REQUIRE (app.project().clips == original.clips);
    const Project pasted = app.project();
    requireDurableRows (pasted);
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (app.project().clips == cut.clips);
    REQUIRE (app.project().midiClips == cut.midiClips);
    REQUIRE (app.dispatch (UiActionId::EditRedo).dispatched);
    REQUIRE (app.project().clips == pasted.clips);
    REQUIRE (app.project().midiClips == pasted.midiClips);
    requireDurableRows (pasted);
}

TEST_CASE ("whole-Clip Cut save refusal preserves MIDI and mixed selection, undo and the prior clipboard",
           "[ui][app][clipboard][midi-clip-cut]")
{
    bool includeAudio = false;
    SECTION ("MIDI-only refusal") {}
    SECTION ("mixed audio and MIDI refusal") { includeAudio = true; }
    const auto bundlePath = makeTempBundlePath ("midi-clip-cut-refusal");
    const Project original = makeWholeClipClipboardProject();
    UiAppModel app;
    REQUIRE (app.createProjectBundle (bundlePath, original).ok());
    writeProjectAssetFiles (bundlePath, original);
    const UiDecodedAsset decoded = makeDecodedAsset (original.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
    REQUIRE (app.renameProjectTrack (original.tracks.front().id, "Existing undo step").dispatched);
    REQUIRE (app.selectTimelineClip (original.midiClips.back().id));
    REQUIRE (app.dispatch (UiActionId::TimelineClipCopy).dispatched);
    if (includeAudio)
    {
        REQUIRE (app.selectTimelineClip (original.clips.front().id));
        REQUIRE (app.selectTimelineClipForGesture (original.midiClips.front().id, true));
    }
    else
        REQUIRE (app.selectTimelineClip (original.midiClips.front().id));
    REQUIRE (app.registry().stateFor (UiActionId::TimelineClipCut, app.context()).enabled);
    const Project before = app.project();
    const auto contextBefore = app.context();
    const EntityId selectionBefore = app.selectedTimelineClipId();
    const auto serialBefore = app.editSerial();
    const auto replacements = app.playbackReplaceCount();
    const auto refuseSave = [&] (bool refuse) {
        sqlite3* connection = nullptr;
        const std::string dbPath = yesdaw::io::utf8Text (bundlePath / "project.db");
        REQUIRE (sqlite3_open (dbPath.c_str(), &connection) == SQLITE_OK);
        const char* sql = refuse
            ? "CREATE TRIGGER refuse_clip_cut BEFORE DELETE ON midi_clips BEGIN SELECT RAISE(ABORT, 'save refused'); END;"
            : "DROP TRIGGER refuse_clip_cut;";
        REQUIRE (sqlite3_exec (connection, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
        REQUIRE (sqlite3_close (connection) == SQLITE_OK);
    };
    refuseSave (true);
    const auto refused = app.dispatch (UiActionId::TimelineClipCut);
    REQUIRE_FALSE (refused.dispatched);
    REQUIRE (std::string_view (refused.state.disabledReason) == "timeline edit did not persist");
    REQUIRE (app.project().clips == before.clips);
    REQUIRE (app.project().midiClips == before.midiClips);
    REQUIRE (app.project().tracks == before.tracks);
    REQUIRE (app.context().timelineClipSelected);
    REQUIRE (app.selectedTimelineClipId() == selectionBefore);
    REQUIRE (app.selectedTimelineClipCount() == (includeAudio ? 2u : 1u));
    REQUIRE (app.isTimelineClipSelected (original.midiClips.front().id));
    if (includeAudio)
        REQUIRE (app.isTimelineClipSelected (original.clips.front().id));
    REQUIRE (app.context().clipboardHasClip == contextBefore.clipboardHasClip);
    REQUIRE (app.context().canUndo == contextBefore.canUndo);
    REQUIRE (app.context().canRedo == contextBefore.canRedo);
    REQUIRE (app.context().commandDispatchCount == contextBefore.commandDispatchCount);
    REQUIRE (app.context().timelineEditCount == contextBefore.timelineEditCount);
    REQUIRE (app.editSerial() == serialBefore);
    REQUIRE (app.playbackReplaceCount() == replacements);
    {
        UiAppModel reopened;
        REQUIRE (reopened.openProjectBundle (bundlePath).ok());
        REQUIRE (reopened.project().clips == before.clips);
        REQUIRE (reopened.project().midiClips == before.midiClips);
        REQUIRE (reopened.project().tracks == before.tracks);
    }
    refuseSave (false);
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (app.project().tracks == original.tracks);
    REQUIRE (app.project().clips == original.clips);
    REQUIRE (app.project().midiClips == original.midiClips);
    REQUIRE_FALSE (app.context().canUndo);
    REQUIRE (app.dispatch (UiActionId::EditRedo).dispatched);
    REQUIRE (app.project().tracks == before.tracks);
    REQUIRE (app.selectMixerTrack (2, false));
    REQUIRE (app.locatePlaybackFrame (1000));
    REQUIRE (app.dispatch (UiActionId::TimelineClipPaste).dispatched);
    REQUIRE (app.project().midiClips.size() == before.midiClips.size() + 1u);
    requireFreshWholeMidiCopy (app.project().midiClips.back(), original.midiClips.back(),
                              original.tracks[2].id, 1000, original);
}

TEST_CASE ("mixed audio and MIDI clipboard retains both source Tracks when another Track is targeted",
           "[ui][app][clipboard][mixed-clip-paste]")
{
    const auto bundlePath = makeTempBundlePath ("mixed-clip-paste");
    const Project original = makeWholeClipClipboardProject();
    UiAppModel app;
    REQUIRE (app.createProjectBundle (bundlePath, original).ok());
    writeProjectAssetFiles (bundlePath, original);
    const UiDecodedAsset decoded = makeDecodedAsset (original.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
    REQUIRE (app.selectTimelineClip (original.clips.front().id));
    REQUIRE (app.selectTimelineClipForGesture (original.midiClips.front().id, true));
    REQUIRE (app.dispatch (UiActionId::TimelineClipCopy).dispatched);
    REQUIRE (app.selectMixerTrack (2, false));
    REQUIRE (app.locatePlaybackFrame (1000));
    REQUIRE (app.dispatch (UiActionId::TimelineClipPaste).dispatched);
    REQUIRE (app.project().clips.size() == original.clips.size() + 1u);
    REQUIRE (app.project().clips.back().trackId == original.clips.front().trackId);
    REQUIRE (app.project().clips.back().timelineStart == 1175);
    requireFreshWholeMidiCopy (app.project().midiClips.back(), original.midiClips.front(),
                              original.midiClips.front().trackId, 1000, original);
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE_FALSE (app.context().canUndo);
    REQUIRE (app.project().clips == original.clips);
    REQUIRE (app.project().midiClips == original.midiClips);
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

TEST_CASE ("real capture count-in rejects pre-roll input and starts the Take at bar two",
           "[ui][app][recording][capture][count-in]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("real-capture-count-in");
    Project project = makeSmokeProject();
    project.tempoMap = { { 0, 150.0, yesdaw::engine::TempoCurve::Jump } };
    project.meterMap = { { 0, 7, 8 } };

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
    REQUIRE (app.dispatch (UiActionId::DeviceSelectTestAudio).dispatched);
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE (app.dispatch (UiActionId::TransportToggleRecordCountIn).dispatched);
    REQUIRE (app.context().recordCountInEnabled);

    REQUIRE (app.startRealRecordingCapture (1, 48'000.0, 0, 0));
    REQUIRE (app.realRecordingCaptureActive());
    REQUIRE (app.context().recordCountInActive);
    REQUIRE_FALSE (app.context().isRecording);

    constexpr std::int64_t kExpectedBarFrames = 67'200; // 150 BPM, 7/8, 48 kHz.
    constexpr int kBlockFrames = 128;
    static_assert (kExpectedBarFrames % kBlockFrames == 0);
    std::array<float, kBlockFrames> input {};
    std::array<float, kBlockFrames> outLeft {};
    std::array<float, kBlockFrames> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    const float* inputs[1] = { input.data() };

    for (int block = 0; block < static_cast<int> (kExpectedBarFrames / kBlockFrames) - 1; ++block)
    {
        input.fill (-0.75f); // pre-roll must never reach the persisted Take.
        REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, kBlockFrames));
        app.drainRealRecordingCapture();
    }
    app.refreshTransportSnapshot();
    app.serviceRecordingCountIn();
    REQUIRE (app.context().recordCountInActive);
    REQUIRE_FALSE (app.context().isRecording);
    REQUIRE (app.project().recordingTakes.empty());

    input.fill (-0.75f);
    REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, kBlockFrames));
    app.drainRealRecordingCapture();
    app.refreshTransportSnapshot();
    app.serviceRecordingCountIn();
    REQUIRE_FALSE (app.context().recordCountInActive);
    REQUIRE (app.context().isRecording);
    REQUIRE (app.project().recordingTakes.empty());

    for (int block = 0; block < 4; ++block)
    {
        for (int frame = 0; frame < kBlockFrames; ++frame)
            input[static_cast<std::size_t> (frame)] = 0.25f
                + static_cast<float> (block * kBlockFrames + frame) * 0.0001f;
        REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, kBlockFrames));
        app.drainRealRecordingCapture();
    }

    const yesdaw::ui::UiAppRecordResult committed = app.stopRealRecordingCaptureAndCommit();
    REQUIRE (committed.ok());
    REQUIRE (committed.take.timelineStart == kExpectedBarFrames);
    REQUIRE (committed.take.frames == 512u);

    ProjectBundleDb verify;
    REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
    Project persisted;
    REQUIRE (verify.readProjectSnapshot (persisted).ok());
    REQUIRE (persisted.recordingTakes.size() == 1u);
    REQUIRE (persisted.recordingTakes.front().timelineStart == kExpectedBarFrames);
    REQUIRE (persisted.recordingTakes.front().frameCount == 512u);
    REQUIRE (persisted.clips.back().timelineStart == kExpectedBarFrames);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("a committed recording clears the undo history for real",
           "[ui][app][recording][record-undo-clear]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("record-undo-clear");
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

    // A prior undoable edit that a stale post-record undo would silently eat.
    REQUIRE (app.addAudioTrack().dispatched);
    const std::size_t tracksAfterEdit = app.project().tracks.size();
    REQUIRE (app.context().canUndo);

    // Record and commit a real take through the test-device capture session.
    REQUIRE (app.dispatch (UiActionId::DeviceSelectTestAudio).dispatched);
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE (app.startRealRecordingCapture (1, 48000.0, 0, 0));
    std::array<float, 128> input {};
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    const float* inputs[1] = { input.data() };
    for (int block = 0; block < 2; ++block)
    {
        for (int i = 0; i < 128; ++i)
            input[static_cast<std::size_t> (i)] = 0.25f;
        REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, 128));
        app.drainRealRecordingCapture();
    }
    REQUIRE (app.stopRealRecordingCaptureAndCommit().ok());

    // R8: the commit's undo clear is REAL — no stale history survives, so undo can never
    // silently revert the pre-record edit. It refuses honestly instead.
    REQUIRE_FALSE (app.context().canUndo);
    REQUIRE_FALSE (app.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (app.project().tracks.size() == tracksAfterEdit);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("adopting a device at another sample rate warns that playback speed will be wrong",
           "[ui][app][recording][sample-rate]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("device-rate-warning");
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
    REQUIRE (app.statusLineText().empty());

    // A matching-rate device adopts quietly.
    REQUIRE (app.adoptRealRecordingDevice ({ 0xFEED0001u, 48'000.0, 2, 256, 12, 34 }));
    REQUIRE (app.statusLineText().empty());

    // A mismatched device adopts (recording provenance stays honest) but WARNS: the engine
    // pulls frames 1:1, so everything would play at the wrong speed.
    REQUIRE (app.adoptRealRecordingDevice ({ 0xFEED0002u, 44'100.0, 2, 256, 12, 34 }));
    REQUIRE (app.statusLineIsError());
    REQUIRE (app.statusLineText().find ("44100") != std::string::npos);
    REQUIRE (app.statusLineText().find ("48000") != std::string::npos);
    REQUIRE (app.statusLineText().find ("playback speed will be wrong") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("adopting the real device unlocks arm and records honest provenance",
           "[ui][app][recording][real-device]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("real-device");
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

    // No device yet: arm is refused and capture never rolls.
    REQUIRE_FALSE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE_FALSE (app.startRealRecordingCapture (2, 44'100.0, 12, 34));

    // Hostile profiles are rejected outright.
    REQUIRE_FALSE (app.adoptRealRecordingDevice ({}));
    REQUIRE_FALSE (app.adoptRealRecordingDevice ({ 0u, 44'100.0, 2, 256, 12, 34 }));
    REQUIRE_FALSE (app.adoptRealRecordingDevice ({ 0xABCD1234u, 0.0, 2, 256, 12, 34 }));
    REQUIRE_FALSE (app.adoptRealRecordingDevice ({ 0xABCD1234u, 44'100.0, -1, 256, 12, 34 }));
    REQUIRE_FALSE (app.context().recordingDeviceSelected);

    // The REAL device profile (injected seam for the audioDeviceAboutToStart wiring): its
    // actual input count, stable id, and driver latencies become the recording selection.
    const yesdaw::ui::UiRealRecordingDeviceProfile profile { 0xABCD1234u, 44'100.0, 2, 256, 12, 34 };
    REQUIRE (app.adoptRealRecordingDevice (profile));
    REQUIRE (app.context().recordingDeviceSelected);
    REQUIRE (app.context().selectedRecordingDeviceId == 0xABCD1234u);
    REQUIRE (app.recordingDeviceSelection().inputChannels == 2u);
    REQUIRE (app.recordingDeviceSelection().inputLatencyFrames == 12);
    REQUIRE (app.recordingDeviceSelection().outputLatencyFrames == 34);
    REQUIRE_FALSE (app.recordingDeviceSelection().latencyCalibrated);

    // E28: capture NEVER rolls unarmed — no silent fallback take onto the first track.
    REQUIRE_FALSE (app.startRealRecordingCapture (2, 44'100.0, 12, 34));
    REQUIRE_FALSE (app.realRecordingCaptureActive());

    // Armed (no Test Device click anywhere), the real capture path commits a take whose
    // provenance records the REAL device's stable id — never the fake stamp.
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE (app.startRealRecordingCapture (2, 44'100.0, 12, 34));
    REQUIRE (app.realRecordingCaptureActive());

    std::array<float, 128> inLeft {};
    std::array<float, 128> inRight {};
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<const float*, 2> inputs { inLeft.data(), inRight.data() };
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    for (int block = 0; block < 4; ++block)
    {
        inLeft.fill (0.5f);
        inRight.fill (-0.25f);
        REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
        app.drainRealRecordingCapture();
    }

    const yesdaw::ui::UiAppRecordResult committed = app.stopRealRecordingCaptureAndCommit();
    REQUIRE (committed.ok());

    ProjectBundleDb verify;
    REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
    Project persisted;
    REQUIRE (verify.readProjectSnapshot (persisted).ok());
    REQUIRE (persisted.recordingTakes.size() == 1u);
    REQUIRE (persisted.recordingTakes.front().deviceStableId == 0xABCD1234u);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("the picked input channel drives the recorded take", "[ui][app][recording][input-pick]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("input-pick");
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

    // No device: the pick is refused.
    REQUIRE_FALSE (app.setRecordingInputChannel (0, false));

    // A 4-input device: out-of-range picks are refused, in-range picks stick.
    REQUIRE (app.adoptRealRecordingDevice ({ 0x5117A0DEu, 48'000.0, 4, 128, 0, 0 }));
    REQUIRE_FALSE (app.setRecordingInputChannel (4, false));
    REQUIRE_FALSE (app.setRecordingInputChannel (3, true));   // pair (3,4) exceeds 4 inputs
    REQUIRE (app.setRecordingInputChannel (2, false));

    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE (app.context().selectedRecordingInputChannel == 2);
    REQUIRE_FALSE (app.context().selectedRecordingInputStereoPair);

    // Four device channels carry distinct DC values; ONLY channel 2's samples may reach the take.
    std::array<float, 128> ch0 {};
    std::array<float, 128> ch1 {};
    std::array<float, 128> ch2 {};
    std::array<float, 128> ch3 {};
    ch0.fill (0.1f);
    ch1.fill (0.2f);
    ch2.fill (0.5f);
    ch3.fill (0.8f);
    std::array<const float*, 4> inputs { ch0.data(), ch1.data(), ch2.data(), ch3.data() };
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };

    REQUIRE (app.startRealRecordingCapture (4, 48'000.0, 0, 0));
    for (int block = 0; block < 4; ++block)
    {
        REQUIRE (app.processDeviceAudioBlock (inputs.data(), 4, outputs.data(), 2, 128));
        app.drainRealRecordingCapture();
    }
    const yesdaw::ui::UiAppRecordResult monoTake = app.stopRealRecordingCaptureAndCommit();
    REQUIRE (monoTake.ok());
    REQUIRE (monoTake.take.channels == 1u);
    REQUIRE (monoTake.take.frames == 512u);

    Project persisted;
    {
        ProjectBundleDb verify;
        REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
        REQUIRE (verify.readProjectSnapshot (persisted).ok());
    }
    REQUIRE (persisted.recordingTakes.size() == 1u);
    REQUIRE (persisted.recordingTakes.front().inputChannel == 2u);

    // The persisted float-WAV take holds channel 2's DC (0.5), not channel 0's.
    const auto readFirstFloats = [&bundlePath, &persisted] (const yesdaw::engine::EntityId assetId,
                                                            std::size_t count)
    {
        const yesdaw::engine::Asset* recorded = nullptr;
        for (const Asset& asset : persisted.assets)
            if (asset.id == assetId)
                recorded = &asset;
        REQUIRE (recorded != nullptr);
        const std::filesystem::path wavPath =
            bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (recorded->contentHash);
        std::ifstream in (wavPath, std::ios::binary);
        REQUIRE (in.good());
        in.seekg (44);
        std::vector<float> samples (count, 0.0f);
        in.read (reinterpret_cast<char*> (samples.data()),
                 static_cast<std::streamsize> (count * sizeof (float)));
        REQUIRE (in.good());
        return samples;
    };
    const std::vector<float> monoSamples = readFirstFloats (monoTake.take.assetId, 4);
    for (float sample : monoSamples)
        REQUIRE (sample == Approx (0.5f));

    // Re-pick the stereo pair (0,1) while still armed: the next take is 2-channel and carries
    // channel 0 and 1 interleaved.
    REQUIRE (app.setRecordingInputChannel (0, true));
    REQUIRE (app.context().selectedRecordingInputStereoPair);
    REQUIRE (app.startRealRecordingCapture (4, 48'000.0, 0, 0));
    for (int block = 0; block < 4; ++block)
    {
        REQUIRE (app.processDeviceAudioBlock (inputs.data(), 4, outputs.data(), 2, 128));
        app.drainRealRecordingCapture();
    }
    const yesdaw::ui::UiAppRecordResult stereoTake = app.stopRealRecordingCaptureAndCommit();
    REQUIRE (stereoTake.ok());
    REQUIRE (stereoTake.take.channels == 2u);

    {
        ProjectBundleDb verify;
        REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
        REQUIRE (verify.readProjectSnapshot (persisted).ok());
    }
    const std::vector<float> stereoSamples = readFirstFloats (stereoTake.take.assetId, 4);
    REQUIRE (stereoSamples[0] == Approx (0.1f));
    REQUIRE (stereoSamples[1] == Approx (0.2f));
    REQUIRE (stereoSamples[2] == Approx (0.1f));
    REQUIRE (stereoSamples[3] == Approx (0.2f));

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("the armed input's live meter reads the picked channel", "[ui][app][recording][input-meter]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("input-meter");
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
    REQUIRE (app.adoptRealRecordingDevice ({ 0x11223344u, 48'000.0, 2, 128, 0, 0 }));

    std::array<float, 128> ch0 {};
    std::array<float, 128> ch1 {};
    ch0.fill (0.9f);
    ch1.fill (0.3f);
    std::array<const float*, 2> inputs { ch0.data(), ch1.data() };
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };

    // ADR-0067 §5: the UI tick's meter step is the one reader per source. In the shell it runs
    // every tick (updateTrackMeterHoldStates calls readArmedInputPeaks); the headless app model
    // test drives it explicitly between the audio block and the control-thread accessor so the
    // meter sees every processed block, not just the last.
    const auto tickMeter = [&] { app.readArmedInputPeaks(); };

    // Unarmed: no live input meter, whatever the device carries. No block publishes to any slot,
    // so a tick reads silence.
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    tickMeter();
    REQUIRE (app.inputMeterPeak() == 0.0f);

    // Armed on the PICKED mono channel 1: the meter reads 0.3 — the pick, not the loudest input.
    REQUIRE (app.setRecordingInputChannel (1, false));
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    tickMeter();
    REQUIRE (app.inputMeterPeak() == Approx (0.3f));

    // The stereo pair meters the pair's max.
    REQUIRE (app.setRecordingInputChannel (0, true));
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    tickMeter();
    REQUIRE (app.inputMeterPeak() == Approx (0.9f));

    // Disarm: no block publishes to the slot any more, so the next tick's cache reads silence
    // (the un-armed slot's cache is cleared to 0 by readArmedInputPeaks).
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    tickMeter();
    REQUIRE (app.inputMeterPeak() == 0.0f);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("DirectInput monitoring routes the armed pick into the live outputs; Off is off",
           "[ui][app][recording][monitoring]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("monitoring");
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
    REQUIRE (app.adoptRealRecordingDevice ({ 0x0DDBA11u, 48'000.0, 2, 128, 0, 0 }));
    REQUIRE (app.setRecordingInputChannel (1, false));
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);

    std::array<float, 128> ch0 {};
    std::array<float, 128> ch1 {};
    ch0.fill (0.8f);
    ch1.fill (0.25f);
    std::array<const float*, 2> inputs { ch0.data(), ch1.data() };
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };

    // Unselected policy: NOTHING reaches the outputs (transport stopped renders silence).
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == 0.0f);
    REQUIRE (outRight[0] == 0.0f);

    // One Monitor cycle = DirectInput: the PICKED mono channel (0.25) sums into BOTH outputs.
    REQUIRE (app.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);
    REQUIRE (app.context().selectedRecordingMonitoringPolicy
             == yesdaw::ui::UiRecordingMonitoringPolicy::DirectInput);
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == Approx (0.25f));
    REQUIRE (outRight[0] == Approx (0.25f));
    REQUIRE (outLeft[127] == Approx (0.25f));

    // The stereo pair routes channel-to-channel.
    REQUIRE (app.setRecordingInputChannel (0, true));
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == Approx (0.8f));
    REQUIRE (outRight[0] == Approx (0.25f));

    // M13 re-pin: LatencyCompensated used to be an honest no-op. It now routes the pick through
    // the armed track's strip — which on THIS default strip (unity gain, centre pan, no inserts,
    // stereo pair pick) is unity, so the pair still arrives channel-to-channel. Off is truly off.
    // (The discriminating gain/pan/FX/latency assertions live in [monitor-compensated].)
    REQUIRE (app.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);   // LatencyCompensated
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == Approx (0.8f));
    REQUIRE (outRight[0] == Approx (0.25f));
    REQUIRE (app.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);   // Off
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == 0.0f);
    REQUIRE (outRight[0] == 0.0f);

    // Disarm kills monitoring even if the policy stays DirectInput.
    REQUIRE (app.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);   // DirectInput
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == Approx (0.8f));
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);   // disarm
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == 0.0f);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("loop recording commits one take per cycle, placed identically",
           "[ui][app][recording][loop-record]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("loop-record");
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
    REQUIRE (app.adoptRealRecordingDevice ({ 0x100731F5u, 48'000.0, 1, 128, 0, 0 }));
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);

    // An 8-frame transport loop (setPlaybackLoopRegion enables the loop): one 128-frame device
    // block covers 16 cycles; the session cap keeps the first 8 as takes and drops the rest.
    REQUIRE (app.setPlaybackLoopRegion (0, 8).dispatched);
    REQUIRE (app.context().loopEnabled);

    REQUIRE (app.startRealRecordingCapture (1, 48'000.0, 0, 0));
    std::array<float, 128> input {};
    for (int i = 0; i < 128; ++i)
        input[static_cast<std::size_t> (i)] = static_cast<float> (i) * 0.001f;
    const float* inputs[1] = { input.data() };
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, 128));
    app.drainRealRecordingCapture();

    const yesdaw::ui::UiAppRecordResult committed = app.stopRealRecordingCaptureAndCommit();
    REQUIRE (committed.ok());

    Project persisted;
    {
        ProjectBundleDb verify;
        REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
        REQUIRE (verify.readProjectSnapshot (persisted).ok());
    }

    // Eight takes — every cycle placed IDENTICALLY at the loop start, 8 frames each, with
    // distinct consecutive ordinals.
    REQUIRE (persisted.recordingTakes.size() == 8u);
    std::set<std::uint32_t> ordinals;
    for (const yesdaw::engine::RecordingTake& take : persisted.recordingTakes)
    {
        REQUIRE (take.timelineStart == 0);
        REQUIRE (take.frameCount == 8u);
        ordinals.insert (take.takeOrdinal);
    }
    REQUIRE (ordinals.size() == 8u);
    REQUIRE (persisted.clips.size() == 9u);   // the original + one clip per cycle take

    // Cycle 2's asset carries frames 16..23 of the device ramp — proof each cycle keeps its
    // OWN audio, not a copy of cycle 0.
    {
        const yesdaw::engine::RecordingTake& cycleTake = persisted.recordingTakes[2];
        const yesdaw::engine::Asset* recorded = nullptr;
        for (const Asset& asset : persisted.assets)
            if (asset.id == cycleTake.assetId)
                recorded = &asset;
        REQUIRE (recorded != nullptr);
        const std::filesystem::path wavPath =
            bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (recorded->contentHash);
        std::ifstream in (wavPath, std::ios::binary);
        REQUIRE (in.good());
        in.seekg (44);
        float firstSample = 0.0f;
        in.read (reinterpret_cast<char*> (&firstSample), sizeof (float));
        REQUIRE (in.good());
        REQUIRE (firstSample == Approx (0.016f));
    }

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("the take stack lists, switches the audible take, and deletes takes undoably",
           "[ui][app][recording][take-switch]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("take-switch");
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
    REQUIRE (app.adoptRealRecordingDevice ({ 0x7A4E0001u, 48'000.0, 1, 128, 0, 0 }));
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE (app.setPlaybackLoopRegion (0, 8).dispatched);

    // Three loop cycles -> a three-take stack, all clips at gain 1.0 (all summing).
    REQUIRE (app.startRealRecordingCapture (1, 48'000.0, 0, 0));
    std::array<float, 24> input {};
    input.fill (0.5f);
    const float* inputs[1] = { input.data() };
    std::array<float, 24> outLeft {};
    std::array<float, 24> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, 24));
    app.drainRealRecordingCapture();
    REQUIRE (app.stopRealRecordingCaptureAndCommit().ok());
    REQUIRE (app.project().recordingTakes.size() == 3u);
    REQUIRE (app.context().timelineClipSelected);

    const std::vector<yesdaw::ui::UiClipTakeView> takes = app.takesForSelectedClipWindow();
    REQUIRE (takes.size() == 3u);
    for (const yesdaw::ui::UiClipTakeView& view : takes)
        REQUIRE (view.audible);

    // Switching makes EXACTLY the chosen take audible (its clip gain 1.0, the others 0.0).
    REQUIRE_FALSE (app.switchAudibleTakeForSelectedClip (idFromLowByte (199)));
    REQUIRE (app.switchAudibleTakeForSelectedClip (takes[0].takeId));
    const auto gainOfClip = [&app] (const EntityId clipId)
    {
        for (const Clip& clip : app.project().clips)
            if (clip.id == clipId)
                return clip.gain;
        return -1.0f;
    };
    REQUIRE (gainOfClip (takes[0].clipId) == 1.0f);
    REQUIRE (gainOfClip (takes[1].clipId) == 0.0f);
    REQUIRE (gainOfClip (takes[2].clipId) == 0.0f);
    const std::vector<yesdaw::ui::UiClipTakeView> switched = app.takesForSelectedClipWindow();
    REQUIRE (switched[0].audible);
    REQUIRE_FALSE (switched[1].audible);
    REQUIRE_FALSE (switched[2].audible);

    // ONE undo restores every gain (the switch was a single group).
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (gainOfClip (takes[1].clipId) == 1.0f);
    REQUIRE (gainOfClip (takes[2].clipId) == 1.0f);

    // Deleting a take scrubs the take AND its clip, undoably.
    const std::size_t clipsBefore = app.project().clips.size();
    REQUIRE_FALSE (app.deleteRecordingTake (idFromLowByte (199)));
    REQUIRE (app.deleteRecordingTake (takes[1].takeId));
    REQUIRE (app.project().recordingTakes.size() == 2u);
    REQUIRE (app.project().clips.size() == clipsBefore - 1u);
    REQUIRE (gainOfClip (takes[1].clipId) == -1.0f);   // clip gone
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (app.project().recordingTakes.size() == 3u);
    REQUIRE (gainOfClip (takes[1].clipId) == 1.0f);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("captured MIDI commits to a real MidiClip mapped through the recording window",
           "[ui][app][recording][midi-record]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("midi-record");
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
    REQUIRE (app.adoptRealRecordingDevice ({ 0x3D1D0001u, 48'000.0, 1, 128, 64, 0 }));
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    const std::size_t midiClipsBefore = app.project().midiClips.size();

    // MIDI outside a live session is refused outright.
    REQUIRE_FALSE (app.captureMidiEventDuringRecording ({ 100, 60, 0.8f, 200 }));

    // A 64-frame input-latency session: device frames map back by 64.
    REQUIRE (app.startRealRecordingCapture (1, 48'000.0, 64, 0));
    std::array<float, 128> input {};
    input.fill (0.25f);
    const float* inputs[1] = { input.data() };
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    for (int block = 0; block < 4; ++block)
    {
        REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, 128));
        app.drainRealRecordingCapture();
    }

    // One note inside the window (device 164 -> timeline 100) and one in the pre-window
    // region the latency mapping rejects (device 10 -> timeline -54).
    REQUIRE (app.captureMidiEventDuringRecording ({ 164, 72, 0.9f, 96 }));
    REQUIRE (app.captureMidiEventDuringRecording ({ 10, 40, 0.5f, 50 }));
    // Hostile events are refused (zero length, out-of-range velocity).
    REQUIRE_FALSE (app.captureMidiEventDuringRecording ({ 164, 72, 0.9f, 0 }));
    REQUIRE_FALSE (app.captureMidiEventDuringRecording ({ 164, 72, 1.5f, 96 }));

    REQUIRE (app.stopRealRecordingCaptureAndCommit().ok());

    // Exactly ONE MidiClip landed, holding exactly the in-window note at the compensated spot.
    REQUIRE (app.project().midiClips.size() == midiClipsBefore + 1u);
    const yesdaw::engine::MidiClip& placed = app.project().midiClips.back();
    REQUIRE (placed.notes.size() == 1u);
    REQUIRE (placed.notes.front().key == 72);
    REQUIRE (placed.notes.front().startTick == 100 - placed.timelineStart);
    REQUIRE (placed.notes.front().lengthTicks == 96);
    REQUIRE (app.lastRecordedMidiTake().midiClipId.isValid());

    Project persisted;
    {
        ProjectBundleDb verify;
        REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
        REQUIRE (verify.readProjectSnapshot (persisted).ok());
    }
    REQUIRE (persisted.midiClips.size() == midiClipsBefore + 1u);
    REQUIRE (persisted.midiClips.back().notes.size() == 1u);

    // ------------------------------------------------------- M12: loop cycles beyond cycle 0
    // A LOOP capture commits one MidiClip per cycle that carries notes, beside that cycle's own
    // audio take — E34 dropped everything after the first cycle.
    const std::size_t midiClipsBeforeLoop = app.project().midiClips.size();
    const std::size_t takesBeforeLoop = app.project().recordingTakes.size();
    REQUIRE (app.setPlaybackLoopRegion (0, 8).dispatched);
    REQUIRE (app.context().loopEnabled);

    // 8-frame loop, 8 frames of input latency: device frame F lands on cycle (F-8)/8 at tick
    // (F-8)%8, and anything before device frame 8 is pre-roll the mapping must reject.
    REQUIRE (app.startRealRecordingCapture (1, 48'000.0, 8, 0));
    std::array<float, 64> loopInput {};
    loopInput.fill (0.25f);
    const float* loopInputs[1] = { loopInput.data() };
    std::array<float, 64> loopOutLeft {};
    std::array<float, 64> loopOutRight {};
    std::array<float*, 2> loopOutputs { loopOutLeft.data(), loopOutRight.data() };
    REQUIRE (app.processDeviceAudioBlock (loopInputs, 1, loopOutputs.data(), 2, 64));
    app.drainRealRecordingCapture();

    REQUIRE (app.captureMidiEventDuringRecording ({ 11, 60, 0.9f, 2 }));   // cycle 0, tick 3
    REQUIRE (app.captureMidiEventDuringRecording ({ 26, 62, 0.9f, 2 }));   // cycle 2, tick 2
    REQUIRE (app.captureMidiEventDuringRecording ({ 51, 65, 0.9f, 2 }));   // cycle 5, tick 3
    REQUIRE (app.captureMidiEventDuringRecording ({ 2, 40, 0.9f, 2 }));    // pre-roll: rejected
    REQUIRE (app.stopRealRecordingCaptureAndCommit().ok());

    // Seven cycles of audio (device frames 8..63 map to cycles 0..6) and THREE MidiClips — one
    // per cycle that carried a note, each on the armed track at that cycle's own position.
    REQUIRE (app.project().recordingTakes.size() == takesBeforeLoop + 7u);
    std::vector<yesdaw::engine::MidiClip> loopClips;
    for (std::size_t i = midiClipsBeforeLoop; i < app.project().midiClips.size(); ++i)
        loopClips.push_back (app.project().midiClips[i]);
    REQUIRE (loopClips.size() == 3u);

    // Cycle order, clip-relative ticks, and the loop-normalised placement (every cycle's take
    // sits at the same timeline window — the E32 take-stack law, now shared by MIDI).
    REQUIRE (loopClips[0].notes.size() == 1u);
    REQUIRE (loopClips[0].notes.front().key == 60);
    REQUIRE (loopClips[0].notes.front().startTick == 3);
    REQUIRE (loopClips[1].notes.size() == 1u);
    REQUIRE (loopClips[1].notes.front().key == 62);
    REQUIRE (loopClips[1].notes.front().startTick == 2);
    REQUIRE (loopClips[2].notes.size() == 1u);
    REQUIRE (loopClips[2].notes.front().key == 65);
    REQUIRE (loopClips[2].notes.front().startTick == 3);
    for (const yesdaw::engine::MidiClip& loopClip : loopClips)
    {
        REQUIRE (loopClip.timelineStart == 0);
        REQUIRE (loopClip.timelineLength == 8);
        REQUIRE (loopClip.trackId == app.project().tracks.front().id);
        for (const yesdaw::engine::Note& note : loopClip.notes)
            REQUIRE (note.key != 40);   // the pre-roll note never lands, in any cycle
    }

    // Each MidiClip shares its cycle's audio take window exactly.
    {
        const Project loopPersisted = [&bundlePath] ()
        {
            Project snapshot;
            ProjectBundleDb verify;
            REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
            REQUIRE (verify.readProjectSnapshot (snapshot).ok());
            return snapshot;
        }();
        REQUIRE (loopPersisted.midiClips.size() == midiClipsBeforeLoop + 3u);
        for (const yesdaw::engine::MidiClip& loopClip : loopClips)
        {
            bool matchedAudioWindow = false;
            for (const yesdaw::engine::Clip& audioClip : loopPersisted.clips)
                matchedAudioWindow = matchedAudioWindow
                                  || (audioClip.trackId == loopClip.trackId
                                      && audioClip.timelineStart == loopClip.timelineStart
                                      && audioClip.timelineLength == loopClip.timelineLength);
            REQUIRE (matchedAudioWindow);
        }
    }

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("N8 a persisted punch region gates both audio and MIDI capture at exact frame edges",
           "[ui][app][recording][punch-record]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("punch-record");
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
    REQUIRE (app.adoptRealRecordingDevice ({ 0x3D1D0002u, 48'000.0, 1, 128, 64, 0 }));
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);

    // A 200-frame punch window: [200, 400).
    REQUIRE (app.setPunchRegion (true, 200, 400).dispatched);
    REQUIRE (app.project().punchRegion.enabled);
    REQUIRE (app.project().punchRegion.startFrame == 200);
    REQUIRE (app.project().punchRegion.endFrame == 400);

    // Zero input latency keeps device frames == compensated frames == timeline frames, so the
    // committed take's bounds can be asserted frame-exact against the punch window directly.
    REQUIRE (app.startRealRecordingCapture (1, 48'000.0, 0, 0));

    std::array<float, 128> input {};
    input.fill (0.25f);
    const float* inputs[1] = { input.data() };
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    // 5 blocks of 128 = 640 device frames (0..639), spanning well before and after [200, 400).
    for (int block = 0; block < 5; ++block)
    {
        REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, 128));
        app.drainRealRecordingCapture();
    }

    // Capture always succeeds for a validly-shaped event (session active, positive length,
    // in-range velocity) — the window rejection happens later, at COMMIT, via the same
    // latency-compensated mapping that already drops pre-roll notes (mirrors the existing
    // "captured MIDI commits to a real MidiClip" test's own before/inside/after shape). One note
    // before the window (device frame 100 &lt; 200), one inside (device frame 250), one after
    // (device frame 450 &gt;= 400) — only the inside one should land after commit.
    REQUIRE (app.captureMidiEventDuringRecording ({ 100, 60, 0.8f, 20 }));
    REQUIRE (app.captureMidiEventDuringRecording ({ 250, 72, 0.9f, 20 }));
    REQUIRE (app.captureMidiEventDuringRecording ({ 450, 65, 0.7f, 20 }));

    const std::size_t midiClipsBefore = app.project().midiClips.size();
    REQUIRE (app.stopRealRecordingCaptureAndCommit().ok());

    // Audio: exactly the punch span, frame-exact at both edges.
    REQUIRE (app.lastRecordedAudioTake().timelineStart == 200);
    REQUIRE (app.lastRecordedAudioTake().frames == 200u);   // [200, 400)

    // MIDI: exactly the one in-window note landed.
    REQUIRE (app.project().midiClips.size() == midiClipsBefore + 1u);
    const yesdaw::engine::MidiClip& placed = app.project().midiClips.back();
    REQUIRE (placed.notes.size() == 1u);
    REQUIRE (placed.notes.front().key == 72);

    // Survives save/reopen.
    Project persisted;
    {
        ProjectBundleDb verify;
        REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
        REQUIRE (verify.readProjectSnapshot (persisted).ok());
    }
    REQUIRE (persisted.punchRegion.enabled);
    REQUIRE (persisted.punchRegion.startFrame == 200);
    REQUIRE (persisted.punchRegion.endFrame == 400);

    // Disabling punch restores today's behaviour bit-identically: a fresh capture with punch off
    // starts recording immediately at frame 0, with no rejection at all.
    REQUIRE (app.setPunchRegion (false, 0, 0).dispatched);
    REQUIRE_FALSE (app.project().punchRegion.enabled);

    REQUIRE (app.startRealRecordingCapture (1, 48'000.0, 0, 0));
    for (int block = 0; block < 2; ++block)
    {
        REQUIRE (app.processDeviceAudioBlock (inputs, 1, outputs.data(), 2, 128));
        app.drainRealRecordingCapture();
    }
    REQUIRE (app.captureMidiEventDuringRecording ({ 10, 50, 0.6f, 20 }));   // no longer pre-window
    REQUIRE (app.stopRealRecordingCaptureAndCommit().ok());
    REQUIRE (app.lastRecordedAudioTake().timelineStart == 0);   // starts immediately, like today

    std::error_code ec2;
    std::filesystem::remove_all (bundlePath, ec2);
}

TEST_CASE ("M13 latency-compensated monitoring runs the armed pick through the armed track's strip",
           "[ui][app][recording][monitor-compensated]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("monitor-compensated");
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
    REQUIRE (app.adoptRealRecordingDevice ({ 0xC0FFEE01u, 48'000.0, 2, 128, 0, 0 }));
    REQUIRE (app.setRecordingInputChannel (0, false));   // mono pick: channel 0
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);

    std::array<float, 128> ch0 {};
    std::array<float, 128> ch1 {};
    ch0.fill (0.4f);
    ch1.fill (0.9f);
    std::array<const float*, 2> inputs { ch0.data(), ch1.data() };
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };

    // DirectInput is the RAW pick: no strip, no compensation.
    REQUIRE (app.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);
    REQUIRE (app.context().selectedRecordingMonitoringPolicy
             == yesdaw::ui::UiRecordingMonitoringPolicy::DirectInput);
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == Approx (0.4f));
    REQUIRE (outRight[0] == Approx (0.4f));
    REQUIRE (app.monitorCompensatedLatencySamples() == 0);   // no compensated path is live

    // The armed track's strip: half gain, centre pan.
    REQUIRE (app.selectMixerTrack (0));
    REQUIRE (app.setSelectedMixerFader (0.5f).dispatched);

    // LatencyCompensated: the SAME pick, but through the strip — the fader applies and the mono
    // pick widens equal-power at centre (ADR-0042), so it is emphatically not the direct signal.
    REQUIRE (app.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);
    REQUIRE (app.context().selectedRecordingMonitoringPolicy
             == yesdaw::ui::UiRecordingMonitoringPolicy::LatencyCompensated);
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    const float centreGain = 0.4f * 0.5f * 0.70710678f;
    REQUIRE (outLeft[0] == Approx (centreGain));
    REQUIRE (outRight[0] == Approx (centreGain));
    REQUIRE (outLeft[127] == Approx (centreGain));
    REQUIRE (outLeft[0] != Approx (0.4f));

    // The strip's pan applies too: hard left silences the right side of the monitor path.
    REQUIRE (app.setSelectedMixerPan (-1.0f).dispatched);
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == Approx (0.4f * 0.5f));
    REQUIRE (std::abs (outRight[0]) < 1.0e-9f);   // equal-power sin at the extreme

    // The strip's FX apply, and their reported latency IS the compensation: a Limiter's 5 ms
    // lookahead at 48 kHz delays the monitored signal by exactly 240 frames.
    REQUIRE (app.addFxInsertToSelectedStrip (yesdaw::engine::FxKind::Limiter).dispatched);
    REQUIRE (app.project().tracks.front().strip.fxChain.size() == 1u);
    REQUIRE (app.monitorCompensatedLatencySamples() == 240);

    // Block 1 (frames 0..127) is still inside the lookahead: silence.
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    for (int frame = 0; frame < 128; ++frame)
        REQUIRE (std::abs (outLeft[frame]) < 1.0e-9f);

    // Block 2 crosses frame 240: silent up to it, then the strip value.
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (std::abs (outLeft[111]) < 1.0e-9f);
    REQUIRE (outLeft[112] == Approx (0.4f * 0.5f));
    REQUIRE (outLeft[127] == Approx (0.4f * 0.5f));

    // Block 3 is fully past the compensation.
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == Approx (0.4f * 0.5f));
    REQUIRE (outLeft[127] == Approx (0.4f * 0.5f));
    REQUIRE (std::abs (outRight[127]) < 1.0e-9f);

    // Off is off, and disarming kills the compensated path even with the policy still set.
    REQUIRE (app.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);   // Off
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == 0.0f);
    REQUIRE (outRight[0] == 0.0f);
    REQUIRE (app.monitorCompensatedLatencySamples() == 0);

    REQUIRE (app.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);   // DirectInput
    REQUIRE (app.dispatch (UiActionId::RecordingSetMonitoringPolicy).dispatched);   // LatencyCompensated
    REQUIRE (app.monitorCompensatedLatencySamples() == 240);
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);              // disarm
    REQUIRE (app.monitorCompensatedLatencySamples() == 0);
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
    REQUIRE (outLeft[0] == 0.0f);
    REQUIRE (outRight[0] == 0.0f);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("M11 an arm SET records one take per armed track", "[ui][app][recording][multi-arm]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("multi-arm");
    const Project project = makeThreeTrackRecordingProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
    REQUIRE (app.project().tracks.size() == 3u);
    REQUIRE (app.adoptRealRecordingDevice ({ 0x4A11ED00u, 48'000.0, 4, 128, 0, 0 }));
    const EntityId trackId0 = app.project().tracks[0].id;
    const EntityId trackId1 = app.project().tracks[1].id;
    const EntityId trackId2 = app.project().tracks[2].id;

    // Four device channels carry distinct DC values; each armed track may only ever receive its
    // OWN picked channel's samples.
    std::array<float, 128> ch0 {};
    std::array<float, 128> ch1 {};
    std::array<float, 128> ch2 {};
    std::array<float, 128> ch3 {};
    ch0.fill (0.1f);
    ch1.fill (0.2f);
    ch2.fill (0.5f);
    ch3.fill (0.8f);
    std::array<const float*, 4> inputs { ch0.data(), ch1.data(), ch2.data(), ch3.data() };
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };

    const auto runCaptureSession = [&app, &inputs, &outputs] ()
    {
        REQUIRE (app.startRealRecordingCapture (4, 48'000.0, 0, 0));
        for (int block = 0; block < 4; ++block)
        {
            REQUIRE (app.processDeviceAudioBlock (inputs.data(), 4, outputs.data(), 2, 128));
            app.drainRealRecordingCapture();
        }
        REQUIRE (app.stopRealRecordingCaptureAndCommit().ok());
    };

    const auto readPersisted = [&bundlePath] ()
    {
        Project persisted;
        ProjectBundleDb verify;
        REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
        REQUIRE (verify.readProjectSnapshot (persisted).ok());
        return persisted;
    };

    const auto takesOnTrack = [] (const Project& snapshot, const EntityId trackId)
    {
        std::vector<yesdaw::engine::RecordingTake> found;
        for (const yesdaw::engine::RecordingTake& take : snapshot.recordingTakes)
            if (take.trackId == trackId)
                found.push_back (take);
        return found;
    };

    const auto readTakeSamples = [&bundlePath] (const Project& snapshot,
                                                const EntityId assetId,
                                                std::size_t count)
    {
        const Asset* recorded = nullptr;
        for (const Asset& asset : snapshot.assets)
            if (asset.id == assetId)
                recorded = &asset;
        REQUIRE (recorded != nullptr);
        const std::filesystem::path wavPath =
            bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (recorded->contentHash);
        std::ifstream in (wavPath, std::ios::binary);
        REQUIRE (in.good());
        in.seekg (44);
        std::vector<float> samples (count, 0.0f);
        in.read (reinterpret_cast<char*> (samples.data()),
                 static_cast<std::streamsize> (count * sizeof (float)));
        REQUIRE (in.good());
        return samples;
    };

    // ------------------------------------------------------------------ single-arm laws hold
    // One armed track behaves exactly as it always has: one take, on that track, ordinal 0,
    // carrying the picked channel — and the single-arm context surface names it.
    REQUIRE (app.toggleRecordingArmForTrack (1).dispatched);
    REQUIRE (app.armedRecordingTrackInputs().size() == 1u);
    REQUIRE (app.context().recordingTrackArmed);
    REQUIRE (app.context().selectedRecordingTrackIndex == 1);
    REQUIRE (app.recordingTrackInputSelection().trackId == trackId1);
    REQUIRE (app.setRecordingInputChannel (1, false));
    runCaptureSession();
    {
        const Project persisted = readPersisted();
        REQUIRE (persisted.recordingTakes.size() == 1u);
        const std::vector<yesdaw::engine::RecordingTake> seeded = takesOnTrack (persisted, trackId1);
        REQUIRE (seeded.size() == 1u);
        REQUIRE (seeded.front().takeOrdinal == 0u);
        REQUIRE (seeded.front().inputChannel == 1u);
        REQUIRE (app.lastRecordedAudioTakes().size() == 1u);
        REQUIRE (app.lastRecordedAudioTakes().front().trackId == trackId1);
        REQUIRE (app.lastRecordedAudioTake().trackId == trackId1);
        REQUIRE (app.selectedTimelineClipCount() == 1u);
    }
    REQUIRE (app.toggleRecordingArmForTrack (1).dispatched);
    REQUIRE (app.armedRecordingTrackInputs().empty());
    REQUIRE_FALSE (app.context().recordingTrackArmed);

    // ------------------------------------------------------------------------- the arm SET
    // Arming a second and third track ADDS them: arming no longer retargets the arm off the
    // first. The FIRST armed track stays the primary the single-arm surface reports.
    REQUIRE (app.toggleRecordingArmForTrack (0).dispatched);
    REQUIRE (app.toggleRecordingArmForTrack (1).dispatched);
    REQUIRE (app.toggleRecordingArmForTrack (2).dispatched);
    REQUIRE (app.armedRecordingTrackInputs().size() == 3u);
    REQUIRE (app.isRecordingTrackIndexArmed (0));
    REQUIRE (app.isRecordingTrackIndexArmed (1));
    REQUIRE (app.isRecordingTrackIndexArmed (2));
    REQUIRE (app.context().recordingTrackArmed);
    REQUIRE (app.context().selectedRecordingTrackIndex == 0);

    // Per-track input picks: mono channel 0, mono channel 3, and the stereo pair (1,2).
    REQUIRE (app.setRecordingInputChannelForTrack (0, 0, false));
    REQUIRE (app.setRecordingInputChannelForTrack (1, 3, false));
    REQUIRE (app.setRecordingInputChannelForTrack (2, 1, true));
    // Hostile picks are refused per track, and an unarmed track has no pick to set.
    REQUIRE_FALSE (app.setRecordingInputChannelForTrack (1, 4, false));
    REQUIRE_FALSE (app.setRecordingInputChannelForTrack (1, 3, true));   // pair (3,4) > 4 inputs
    REQUIRE_FALSE (app.setRecordingInputChannelForTrack (7, 0, false));  // no such track row
    REQUIRE (app.armedRecordingTrackInputs()[0].inputChannel == 0u);
    REQUIRE_FALSE (app.armedRecordingTrackInputs()[0].stereoPair);
    REQUIRE (app.armedRecordingTrackInputs()[1].inputChannel == 3u);
    REQUIRE (app.armedRecordingTrackInputs()[2].inputChannel == 1u);
    REQUIRE (app.armedRecordingTrackInputs()[2].stereoPair);

    // Each armed track meters its OWN input, live, before the transport rolls. ADR-0067 §5: the
    // UI tick's meter step is the one reader per source — call it explicitly in this headless
    // test so the control-thread accessors see every processed block. A drain BEFORE the publish
    // flushes any blocks the first capture session published while the UI tick never ran (a real
    // UI reads every 33 ms, so this is a headless-only step).
    app.readArmedInputPeaks();   // drain carry-over from the single-arm capture above
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 4, outputs.data(), 2, 128));
    app.readArmedInputPeaks();
    REQUIRE (app.inputMeterPeakForTrackIndex (0) == Approx (0.1f));
    REQUIRE (app.inputMeterPeakForTrackIndex (1) == Approx (0.8f));
    REQUIRE (app.inputMeterPeakForTrackIndex (2) == Approx (0.5f));   // the pair's max
    REQUIRE (app.inputMeterPeak() == Approx (0.1f));                  // the primary's meter

    runCaptureSession();

    // One take per armed track, each on its OWN track, each carrying EXACTLY its picked
    // channel's samples at its own per-track ordinal.
    {
        const Project persisted = readPersisted();
        REQUIRE (persisted.recordingTakes.size() == 4u);   // the seed take plus one per armed track

        const std::vector<yesdaw::engine::RecordingTake> onTrack0 = takesOnTrack (persisted, trackId0);
        const std::vector<yesdaw::engine::RecordingTake> onTrack1 = takesOnTrack (persisted, trackId1);
        const std::vector<yesdaw::engine::RecordingTake> onTrack2 = takesOnTrack (persisted, trackId2);
        REQUIRE (onTrack0.size() == 1u);
        REQUIRE (onTrack1.size() == 2u);
        REQUIRE (onTrack2.size() == 1u);

        // Per-track ordinals: track 1 continues its own numbering; the others start at 0.
        REQUIRE (onTrack0.front().takeOrdinal == 0u);
        REQUIRE (onTrack1.back().takeOrdinal == 1u);
        REQUIRE (onTrack2.front().takeOrdinal == 0u);

        // Per-track provenance: the picked channel and the REAL device id ride each take.
        REQUIRE (onTrack0.front().inputChannel == 0u);
        REQUIRE (onTrack1.back().inputChannel == 3u);
        REQUIRE (onTrack2.front().inputChannel == 1u);
        for (const yesdaw::engine::RecordingTake& take : persisted.recordingTakes)
            REQUIRE (take.deviceStableId == 0x4A11ED00u);

        // The bytes themselves: channel 0's DC, channel 3's DC, and the (1,2) pair interleaved.
        const std::vector<float> track0Samples = readTakeSamples (persisted, onTrack0.front().assetId, 4);
        for (float sample : track0Samples)
            REQUIRE (sample == Approx (0.1f));
        const std::vector<float> track1Samples = readTakeSamples (persisted, onTrack1.back().assetId, 4);
        for (float sample : track1Samples)
            REQUIRE (sample == Approx (0.8f));
        const std::vector<float> track2Samples = readTakeSamples (persisted, onTrack2.front().assetId, 4);
        REQUIRE (track2Samples[0] == Approx (0.2f));
        REQUIRE (track2Samples[1] == Approx (0.5f));
        REQUIRE (track2Samples[2] == Approx (0.2f));
        REQUIRE (track2Samples[3] == Approx (0.5f));

        // Every armed track's take is reported, in arm order, and the session selects all three
        // new clips with the primary's clip as the focused one.
        REQUIRE (app.lastRecordedAudioTakes().size() == 3u);
        REQUIRE (app.lastRecordedAudioTakes()[0].trackId == trackId0);
        REQUIRE (app.lastRecordedAudioTakes()[1].trackId == trackId1);
        REQUIRE (app.lastRecordedAudioTakes()[2].trackId == trackId2);
        REQUIRE (app.lastRecordedAudioTake().trackId == trackId0);
        REQUIRE (app.selectedTimelineClipCount() == 3u);
        for (const yesdaw::ui::UiRecordedAudioTake& take : app.lastRecordedAudioTakes())
            REQUIRE (app.isTimelineClipSelected (take.clipId));
    }

    // A multi-track commit follows the SAME undo law as a single-track one: the commit is
    // not an undo step (it is bundle-owned persistence, not a project edit) and R8 clears
    // the stack for real. (Imports ARE undo steps since R8.) Takes are removed with the
    // shipped delete verb, which IS undoable — and removing one leaves the other armed
    // tracks' takes untouched.
    REQUIRE_FALSE (app.context().canUndo);
    REQUIRE_FALSE (app.context().canRedo);
    {
        const yesdaw::ui::UiRecordedAudioTake removed = app.lastRecordedAudioTakes()[1];
        REQUIRE (app.deleteRecordingTake (removed.takeId));
        REQUIRE (app.project().recordingTakes.size() == 3u);
        REQUIRE (takesOnTrack (app.project(), trackId0).size() == 1u);
        REQUIRE (takesOnTrack (app.project(), trackId2).size() == 1u);
        REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
        REQUIRE (app.project().recordingTakes.size() == 4u);
    }

    // ------------------------------------------------------- disarming ONE track mid-session
    // Toggling an armed track drops just that track; the rest of the set stays armed and its
    // dropped meter reads silent.
    REQUIRE (app.toggleRecordingArmForTrack (1).dispatched);
    REQUIRE (app.armedRecordingTrackInputs().size() == 2u);
    REQUIRE (app.isRecordingTrackIndexArmed (0));
    REQUIRE_FALSE (app.isRecordingTrackIndexArmed (1));
    REQUIRE (app.isRecordingTrackIndexArmed (2));
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 4, outputs.data(), 2, 128));
    app.readArmedInputPeaks();   // ADR-0067 §5: the tick's one reader fills the cache
    REQUIRE (app.inputMeterPeakForTrackIndex (1) == 0.0f);

    runCaptureSession();
    {
        const Project persisted = readPersisted();
        REQUIRE (persisted.recordingTakes.size() == 6u);   // two more, none on the disarmed track
        REQUIRE (takesOnTrack (persisted, trackId0).size() == 2u);
        REQUIRE (takesOnTrack (persisted, trackId1).size() == 2u);
        REQUIRE (takesOnTrack (persisted, trackId2).size() == 2u);
        REQUIRE (takesOnTrack (persisted, trackId0).back().takeOrdinal == 1u);
        REQUIRE (takesOnTrack (persisted, trackId2).back().takeOrdinal == 1u);
    }

    // Arming never changes the take path mid-capture.
    REQUIRE (app.startRealRecordingCapture (4, 48'000.0, 0, 0));
    REQUIRE_FALSE (app.toggleRecordingArmForTrack (1).dispatched);
    REQUIRE_FALSE (app.setRecordingInputChannelForTrack (0, 2, false));
    REQUIRE (app.processDeviceAudioBlock (inputs.data(), 4, outputs.data(), 2, 128));
    app.drainRealRecordingCapture();
    REQUIRE (app.stopRealRecordingCaptureAndCommit().ok());
    REQUIRE (app.armedRecordingTrackInputs().size() == 2u);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("the shipped Record button's exact verb sequence commits a latency-compensated take",
           "[ui][app][recording][record-button-path]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("record-button-path");
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

    // The EXACT sequence MainComponent's Record button runs on the real-device branch:
    // adoption (audioDeviceAboutToStart), arm-if-unarmed, then startRealRecordingCapture with
    // the device's live parameters — here with NONZERO driver latencies.
    REQUIRE (app.adoptRealRecordingDevice ({ 0xB0770001u, 48'000.0, 2, 128, 128, 64 }));
    REQUIRE_FALSE (app.context().recordingTrackArmed);
    REQUIRE (app.dispatch (UiActionId::RecordingArmTrack).dispatched);
    REQUIRE (app.startRealRecordingCapture (2, 48'000.0, 128, 64));

    std::array<float, 128> ch0 {};
    std::array<float, 128> ch1 {};
    std::array<const float*, 2> inputs { ch0.data(), ch1.data() };
    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    for (int block = 0; block < 4; ++block)
    {
        ch0.fill (0.4f);
        ch1.fill (-0.4f);
        REQUIRE (app.processDeviceAudioBlock (inputs.data(), 2, outputs.data(), 2, 128));
        app.drainRealRecordingCapture();
    }

    const yesdaw::ui::UiAppRecordResult committed = app.stopRealRecordingCaptureAndCommit();
    REQUIRE (committed.ok());

    // ADR-0018 compensation: with 128+64 driver latency and a mono pick default of channel 0,
    // the first 192 device frames map before the timeline start and are rejected — the take
    // starts at frame 0 and holds the remaining 320 frames.
    REQUIRE (committed.take.timelineStart == 0);
    REQUIRE (committed.take.frames == 512u - 192u);

    Project persisted;
    {
        ProjectBundleDb verify;
        REQUIRE (ProjectBundleDb::openExistingBundle (bundlePath, verify).ok());
        REQUIRE (verify.readProjectSnapshot (persisted).ok());
    }
    REQUIRE (persisted.recordingTakes.size() == 1u);
    REQUIRE (persisted.recordingTakes.front().deviceStableId == 0xB0770001u);

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

    // The click survives an edit that rebuilds the playback engine. R2: the edit no longer
    // returns the playhead to zero, so locate there explicitly for the windowed peak scan.
    REQUIRE (app.dispatch (UiActionId::TransportStop).dispatched);
    REQUIRE (app.addAudioTrack().dispatched);
    REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
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

// G0.3 — the retire law behind "the audio callback is never removed by a UI action" (ADR-0046
// §6). An engine swap publishes the new engine atomically and RETIRES the old one; the janitor
// frees it only once the device thread has started two blocks past the retirement (so the last
// block that could have loaded the old pointer has finished), or immediately while no device
// callback is live. Headless: this test IS the device thread, one block at a time.
TEST_CASE ("G0.3 engine swaps retire the old engine until the device thread is provably past it",
           "[ui][app][no-callback-teardown][retire]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("retire-law");
    UiAppModel app;
    REQUIRE (app.createProjectBundle (bundlePath).ok());
    app.reclaimRetiredAudioObjects();
    REQUIRE (app.retiredAudioObjectCount() == 0);
    REQUIRE_FALSE (app.deviceCallbackLive());

    std::array<float, 128> outLeft {};
    std::array<float, 128> outRight {};
    std::array<float*, 2> outputs { outLeft.data(), outRight.data() };
    const auto deviceBlock = [&] { REQUIRE (app.processDeviceAudioBlock (outputs.data(), 2, 128)); };

    // Live callback: a topology edit (Add Track) rebuilds the engine — the old one is retired,
    // not freed, until two more blocks have STARTED.
    app.setDeviceCallbackLive (true);
    const std::uint64_t rebuildsBefore = app.playbackReplaceCount();
    REQUIRE (app.dispatch (UiActionId::TrackAdd).dispatched);
    REQUIRE (app.playbackReplaceCount() == rebuildsBefore + 1);
    REQUIRE (app.retiredAudioObjectCount() == 1);
    app.reclaimRetiredAudioObjects();
    REQUIRE (app.retiredAudioObjectCount() == 1);
    deviceBlock();
    app.reclaimRetiredAudioObjects();
    REQUIRE (app.retiredAudioObjectCount() == 1);   // one block: the pre-swap block may still be inside it
    deviceBlock();
    app.reclaimRetiredAudioObjects();
    REQUIRE (app.retiredAudioObjectCount() == 0);   // two blocks: provably past it

    // Two swaps back to back retire two engines; both wait for the same proof.
    REQUIRE (app.dispatch (UiActionId::TrackAdd).dispatched);
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    REQUIRE (app.retiredAudioObjectCount() == 2);
    deviceBlock();
    deviceBlock();
    app.reclaimRetiredAudioObjects();
    REQUIRE (app.retiredAudioObjectCount() == 0);

    // No live callback (headless harness, no device, chooser suspended): freed at once.
    app.setDeviceCallbackLive (false);
    REQUIRE (app.dispatch (UiActionId::TrackAdd).dispatched);
    REQUIRE (app.retiredAudioObjectCount() == 0);

    // The device thread never sees a null engine during a swap: every block after the first
    // rebuild rendered through a live engine (returns true), and the block counter only grows.
    const std::uint64_t blocks = app.deviceBlocksStarted();
    REQUIRE (blocks == 4);
    deviceBlock();
    REQUIRE (app.deviceBlocksStarted() == blocks + 1);
}

// G0.4 (a G0.3 consequence): the REAL device survives a project change. attachProjectBundle
// drops per-project recording state; the adopted hardware profile is re-adopted with a NEW,
// monotonic generation so the shell's choosers re-read it. Before G0.3 this happened only by
// accident (every action re-registered the audio callback, and JUCE re-ran device adoption).
TEST_CASE ("G0.4 the adopted real device survives New / Open with a new generation",
           "[ui][app][recording][device-survives-project]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("device-survives");
    UiAppModel app;
    REQUIRE_FALSE (app.recordingDeviceSelection().selected);

    // Without an adopted device a project change leaves the selection honestly empty.
    REQUIRE (app.createProjectBundle (bundlePath).ok());
    REQUIRE_FALSE (app.recordingDeviceSelection().selected);
    REQUIRE (app.recordingDeviceSelection().generation == 0u);

    REQUIRE (app.adoptRealRecordingDevice ({ 0xFEED0042u, 48'000.0, 2, 256, 12, 34 }));
    const std::uint32_t adopted = app.recordingDeviceSelection().generation;
    REQUIRE (adopted >= 1u);
    REQUIRE (app.recordingDeviceSelection().inputChannels == 2u);

    const std::filesystem::path second = makeTempBundlePath ("device-survives-2");
    REQUIRE (app.createProjectBundle (second).ok());
    REQUIRE (app.recordingDeviceSelection().selected);
    REQUIRE (app.recordingDeviceSelection().stableDeviceId == 0xFEED0042u);
    REQUIRE (app.recordingDeviceSelection().inputChannels == 2u);
    REQUIRE (app.recordingDeviceSelection().generation > adopted);
    REQUIRE (app.context().recordingDeviceGeneration == app.recordingDeviceSelection().generation);
    REQUIRE (app.context().recordingDeviceSelected);

    // Reopening the first bundle re-adopts again — still the same hardware, one generation on.
    const std::uint32_t afterCreate = app.recordingDeviceSelection().generation;
    REQUIRE (app.openProjectBundle (bundlePath).ok());
    REQUIRE (app.recordingDeviceSelection().selected);
    REQUIRE (app.recordingDeviceSelection().generation > afterCreate);
}

// G0.5 — the live placement lane (ADR-0046 §6; plan §5.3 lane 2; feel budget B4). A placement-only
// edit (nudge, gain, fades, delete, and its undo) publishes the changed Track's ClipSchedule to
// the RUNNING engine instead of rebuilding it; what the live engine then renders is bit-identical
// to what a fresh engine built from the persisted bundle renders; a topology edit still rebuilds.
TEST_CASE ("G0.5 placement edits ride the live lane: no engine rebuild, audio equals a rebuild",
           "[ui][app][live-placement][b4]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("live-placement");
    Project project = makeSmokeProject();
    Clip neighbour = project.clips.front();
    neighbour.id = idFromLowByte (9);
    neighbour.timelineStart = 12;
    project.clips.push_back (neighbour);
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
    const std::uint64_t rebuilds = app.playbackReplaceCount();
    REQUIRE (app.livePlacementEdits() == 0);
    const yesdaw::engine::DecodedAssetAudio diagView {
        decoded.assetId, decoded.sampleRate, decoded.frames, decoded.channels,
        decoded.interleaved() };
    // Phase check: the live engine renders what the offline render of the in-memory project renders.
    const auto checkLive = [&] (const char* phase) {
        REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
        REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
        (void) app.renderPlaybackFrames (256, 128);   // let a queued burst land (<= 64 commands per block)
        REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
        const std::vector<float> live = app.renderPlaybackFrames (256, 128);
        REQUIRE (app.dispatch (UiActionId::TransportStop).dispatched);
        const auto offline = yesdaw::engine::renderOfflineProject (app.project(), std::span<const yesdaw::engine::DecodedAssetAudio> (&diagView, 1));
        REQUIRE (offline.ok());
        const std::size_t n = std::min (live.size(), offline.interleavedSamples.size());
        INFO ("phase: " << phase << " gain0=" << app.project().clips[0].gain << " live[2]=" << live[2]
              << " offline[2]=" << offline.interleavedSamples[2] << " applied=" << app.playbackLiveSchedulesApplied()
              << " liveEdits=" << app.livePlacementEdits() << " rebuilds=" << app.playbackReplaceCount());
        REQUIRE (app.playbackLiveSchedulesApplied() == app.livePlacementEdits());
        for (std::size_t i = 0; i < n; ++i)
            REQUIRE (live[i] == offline.interleavedSamples[i]);
    };

    // 100 placement edits of every live verb: nudges both ways, gain, fades, delete, and undo.
    REQUIRE (app.selectTimelineClip (idFromLowByte (3)));
    for (int i = 0; i < 20; ++i)
        REQUIRE (app.dispatch (UiActionId::EditNudgeRight).dispatched);
    for (int i = 0; i < 20; ++i)
        REQUIRE (app.dispatch (UiActionId::EditNudgeLeft).dispatched);
    checkLive ("after 40 nudges");
    for (int i = 0; i < 10; ++i)
        REQUIRE (app.dispatch (UiActionId::TimelineClipGainIncrease).dispatched);
    for (int i = 0; i < 10; ++i)
        REQUIRE (app.dispatch (UiActionId::TimelineClipGainDecrease).dispatched);
    REQUIRE (app.dispatch (UiActionId::TimelineClipApplyDefaultFades).dispatched);
    checkLive ("after gains + fades");
    for (int i = 0; i < 20; ++i)
        REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    checkLive ("after 20 undos");
    for (int i = 0; i < 19; ++i)
        REQUIRE (app.dispatch (UiActionId::EditRedo).dispatched);
    checkLive ("after 19 redos");
    REQUIRE (app.playbackReplaceCount() == rebuilds);
    REQUIRE (app.livePlacementEdits() == 100);

    // Delete and its undo: still no rebuild.
    REQUIRE (app.dispatch (UiActionId::TimelineClipDelete).dispatched);
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    checkLive ("after delete + undo");
    REQUIRE (app.playbackReplaceCount() == rebuilds);
    REQUIRE (app.livePlacementEdits() == 102);
    REQUIRE (app.project().clips.size() == 2u);

    // What the LIVE engine plays now is exactly what a fresh engine built from the persisted
    // bundle plays (the same law, bit for bit), and it is not silence. Both play from zero: a
    // stopped transport renders silence without draining the command queue.
    REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
    REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
    (void) app.renderPlaybackFrames (256, 128);   // settle any queued burst (<= 64 commands per block)
    REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
    const std::vector<float> live = app.renderPlaybackFrames (2048, 128);
    REQUIRE (app.playbackLiveSchedulesApplied() == app.livePlacementEdits());
    UiAppModel fresh;
    UiDecodedAsset decodedAgain = makeDecodedAsset (project.assets.front());
    REQUIRE (fresh.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decodedAgain, 1)).ok());
    REQUIRE (fresh.project().clips.size() == 2u);
    // The persisted bundle and the in-memory project agree on every clip (placement + gain + fades).
    for (std::size_t i = 0; i < 2u; ++i)
    {
        INFO ("clip " << i << " gain live=" << app.project().clips[i].gain << " persisted=" << fresh.project().clips[i].gain
              << " start live=" << app.project().clips[i].timelineStart << " persisted=" << fresh.project().clips[i].timelineStart
              << " fadeIn live=" << app.project().clips[i].fadeIn << " persisted=" << fresh.project().clips[i].fadeIn);
        REQUIRE (app.project().clips[i] == fresh.project().clips[i]);
    }
    REQUIRE (fresh.dispatch (UiActionId::TransportLocateStart).dispatched);
    REQUIRE (fresh.dispatch (UiActionId::TransportPlay).dispatched);
    const std::vector<float> rebuilt = fresh.renderPlaybackFrames (2048, 128);
    REQUIRE (live.size() == rebuilt.size());
    // Diagnostics: the project's gains are back at unity, and the offline render of the
    // in-memory project tells which engine drifted if the two disagree.
    REQUIRE (app.project().clips[0].gain == Catch::Approx (1.0f));
    REQUIRE (app.project().clips[1].gain == Catch::Approx (1.0f));
    {
        const yesdaw::engine::DecodedAssetAudio view {
            decoded.assetId, decoded.sampleRate, decoded.frames, decoded.channels,
            decoded.interleaved() };
        const auto offline = yesdaw::engine::renderOfflineProject (app.project(), std::span<const yesdaw::engine::DecodedAssetAudio> (&view, 1));
        REQUIRE (offline.ok());
        const std::size_t n = std::min (live.size(), offline.interleavedSamples.size());
        std::vector<float> offlineHead (offline.interleavedSamples.begin(), offline.interleavedSamples.begin() + static_cast<std::ptrdiff_t> (n));
        std::vector<float> liveHead (live.begin(), live.begin() + static_cast<std::ptrdiff_t> (n));
        std::vector<float> rebuiltHead (rebuilt.begin(), rebuilt.begin() + static_cast<std::ptrdiff_t> (n));
        INFO ("offline[2]=" << offline.interleavedSamples[2] << " live[2]=" << live[2] << " rebuilt[2]=" << rebuilt[2]);
        REQUIRE (rebuiltHead == offlineHead);
        REQUIRE (liveHead == offlineHead);
    }
    REQUIRE (live == rebuilt);
    float peak = 0.0f;
    for (const float v : live)
        peak = std::max (peak, std::abs (v));
    REQUIRE (peak > 0.0f);

    // Negative control: topology (a new Track) still rebuilds — exactly once.
    REQUIRE (app.addAudioTrack().dispatched);
    REQUIRE (app.playbackReplaceCount() == rebuilds + 1);
    REQUIRE (app.livePlacementEdits() == 102);
}

// G0.5 bisect: after EACH single placement verb the live engine must render what the offline
// render of the in-memory project renders. Localizes which verb leaves the live schedule stale.
TEST_CASE ("G0.5 live lane stays in step with the project after every single verb",
           "[ui][app][live-placement][bisect]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("live-placement-bisect");
    Project project = makeSmokeProject();
    Clip neighbour = project.clips.front();
    neighbour.id = idFromLowByte (9);
    neighbour.timelineStart = 12;
    project.clips.push_back (neighbour);
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }
    UiAppModel app;
    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
    REQUIRE (app.selectTimelineClip (idFromLowByte (3)));
    const yesdaw::engine::DecodedAssetAudio view {
        decoded.assetId, decoded.sampleRate, decoded.frames, decoded.channels,
        decoded.interleaved() };

    const auto check = [&] (const char* step) {
        REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
        REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
        (void) app.renderPlaybackFrames (256, 128);   // let the post land (<= 64 commands per block)
        REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
        const std::vector<float> live = app.renderPlaybackFrames (256, 128);
        REQUIRE (app.dispatch (UiActionId::TransportStop).dispatched);
        const auto offline = yesdaw::engine::renderOfflineProject (app.project(), std::span<const yesdaw::engine::DecodedAssetAudio> (&view, 1));
        REQUIRE (offline.ok());
        const std::size_t n = std::min (live.size(), offline.interleavedSamples.size());
        INFO ("step: " << step << " gain0=" << app.project().clips[0].gain << " live[2]=" << live[2]
              << " offline[2]=" << offline.interleavedSamples[2] << " applied=" << app.playbackLiveSchedulesApplied()
              << " liveEdits=" << app.livePlacementEdits() << " rebuilds=" << app.playbackReplaceCount());
        for (std::size_t i = 0; i < n; ++i)
            REQUIRE (live[i] == offline.interleavedSamples[i]);
    };

    check ("initial");
    REQUIRE (app.dispatch (UiActionId::TimelineClipGainIncrease).dispatched);
    check ("gain+");
    REQUIRE (app.dispatch (UiActionId::TimelineClipGainDecrease).dispatched);
    check ("gain-");
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    check ("undo (back to gain+)");
    REQUIRE (app.dispatch (UiActionId::EditRedo).dispatched);
    check ("redo (back to unity)");
    REQUIRE (app.dispatch (UiActionId::EditNudgeRight).dispatched);
    check ("nudge right");
    REQUIRE (app.dispatch (UiActionId::TimelineClipDelete).dispatched);
    check ("delete");
    REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
    check ("undo delete");
}

// G0.5 burst probe: N posts queued with no block processed in between must all be applied by
// the next render, in order, so the live engine renders the project's final state — swept across
// the runtime's queue depths (command queue 256, retirement queue 64, 64 commands per block).
TEST_CASE ("G0.5 a burst of queued schedule posts is applied in order by the next render",
           "[ui][app][live-placement][burst]")
{
    for (const int posts : { 63, 64, 65, 71, 90, 102, 127, 128, 129, 200 })
    {
        const std::filesystem::path bundlePath = makeTempBundlePath ("live-placement-burst-" + std::to_string (posts));
        Project project = makeSmokeProject();
        {
            ProjectBundleDb db;
            REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
            REQUIRE (db.writeProjectSnapshot (project).ok());
            writeProjectAssetFiles (bundlePath, project);
        }
        UiAppModel app;
        UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
        REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
        REQUIRE (app.selectTimelineClip (idFromLowByte (3)));
        const yesdaw::engine::DecodedAssetAudio view {
            decoded.assetId, decoded.sampleRate, decoded.frames, decoded.channels,
            decoded.interleaved() };

        for (int i = 0; i < posts; ++i)
            REQUIRE (app.dispatch (i % 2 == 0 ? UiActionId::TimelineClipGainIncrease
                                              : UiActionId::TimelineClipGainDecrease).dispatched);
        REQUIRE (app.livePlacementEdits() == static_cast<std::uint64_t> (posts));
        const std::uint64_t appliedBeforeTransport = app.playbackLiveSchedulesApplied();
        REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
        REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
        const std::uint64_t appliedBeforeRender = app.playbackLiveSchedulesApplied();
        // The runtime applies at most 64 commands per block (Runtime::Config::maxCommandsPerBlock,
        // the O(1) block bound), so a burst of N posts lands within ceil (N / 64) blocks — the
        // running app's next few device blocks. Settle for four blocks, then compare from zero.
        (void) app.renderPlaybackFrames (512, 128);
        const std::uint64_t appliedAfterRender = app.playbackLiveSchedulesApplied();
        REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
        const std::vector<float> live = app.renderPlaybackFrames (512, 128);
        const auto offline = yesdaw::engine::renderOfflineProject (app.project(), std::span<const yesdaw::engine::DecodedAssetAudio> (&view, 1));
        REQUIRE (offline.ok());
        INFO ("posts=" << posts << " applied before transport=" << appliedBeforeTransport << " before render=" << appliedBeforeRender
              << " after render=" << appliedAfterRender << " gain0=" << app.project().clips[0].gain
              << " live[2]=" << live[2] << " offline[2]=" << offline.interleavedSamples[2]
              << " rebuilds=" << app.playbackReplaceCount());
        REQUIRE (appliedAfterRender == static_cast<std::uint64_t> (posts));
        const std::size_t n = std::min (live.size(), offline.interleavedSamples.size());
        for (std::size_t i = 0; i < n; ++i)
            REQUIRE (live[i] == offline.interleavedSamples[i]);
    }
}

TEST_CASE ("current Project clocks reach the metronome through load and playback replacement", "[ui][app][musical-click]")
{
    const auto bundlePath = makeTempBundlePath ("musical-click-map");
    Project project = makeSmokeProject();
    project.tempoMap = { { 0, 120.0, yesdaw::engine::TempoCurve::LinearRamp },
                         { 61440, 240.0, yesdaw::engine::TempoCurve::Jump } };
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }
    {
        UiAppModel app;
        const UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
        REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
        REQUIRE (app.dispatch (UiActionId::TransportToggleMetronome).dispatched);
        const auto peakAt = [&] (std::int64_t frame) {
            REQUIRE (app.locatePlaybackFrame (frame));
            REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
            const auto audio = app.renderPlaybackFrames (64, 7);
            float peak = 0.0f;
            for (const float sample : audio)
                peak = std::max (peak, std::abs (sample));
            return peak;
        };
        CHECK (peakAt (21421) > 0.1f);
        CHECK (peakAt (24000) == 0.0f);
        REQUIRE (app.addAudioTrack().dispatched);
        CHECK (peakAt (21421) > 0.1f);
        REQUIRE (app.locatePlaybackFrame (0));
        REQUIRE (app.setProjectTempoBpm (60.0).dispatched);
        // New ramp slope: F(1)=64000*log(1.75), independently floored.
        CHECK (peakAt (35815) > 0.1f);
        CHECK (peakAt (21421) == 0.0f);
        REQUIRE (app.dispatch (UiActionId::EditUndo).dispatched);
        CHECK (peakAt (21421) > 0.1f);
    }
    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("unrepresentable click preparation refuses enabling while SampleLocked audio remains playable", "[ui][app][musical-click]")
{
    const auto bundlePath = makeTempBundlePath ("musical-click-refusal");
    Project project = makeSmokeProject();
    project.tempoMap = { { 0, 1.0e-300, yesdaw::engine::TempoCurve::LinearRamp },
        { yesdaw::engine::Tick { 1 } << 48, std::nextafter (1.0e-300, 1.0), yesdaw::engine::TempoCurve::Jump } };
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }
    {
        UiAppModel app;
        const UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
        REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
        REQUIRE_FALSE (app.dispatch (UiActionId::TransportToggleMetronome).dispatched);
        CHECK_FALSE (app.context().metronomeEnabled);
        CHECK (app.statusLineIsError());
        CHECK (app.statusLineText().starts_with ("Metronome unavailable:"));
        REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
        const auto audio = app.renderPlaybackFrames (64, 7);
        CHECK (std::any_of (audio.begin(), audio.end(), [] (float sample) { return sample != 0.0f; }));
    }
    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("runtime metronome domain refusals reach the existing status line", "[ui][app][musical-click]")
{
    const auto bundlePath = makeTempBundlePath ("musical-click-lookup-refusal");
    Project project = makeSmokeProject();
    project.tempoMap = { { 0, std::numeric_limits<double>::denorm_min(), yesdaw::engine::TempoCurve::Jump } };
    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }
    {
        UiAppModel app;
        const UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
        REQUIRE (app.loadProjectBundle (bundlePath, std::span<const UiDecodedAsset> (&decoded, 1)).ok());
        REQUIRE (app.dispatch (UiActionId::TransportToggleMetronome).dispatched);
        REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
        REQUIRE_FALSE (app.renderPlaybackFrames (64, 7).empty());
        app.refreshTransportSnapshot();
        CHECK (app.statusLineIsError());
        CHECK (app.statusLineText() == "Metronome unavailable: musical timing lookup failed");
    }
    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}
