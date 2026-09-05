// G3.4 — Quantize v2: grid, strength %, swing %, note ends, humanize. Closed forms per parameter.
//
// Gates:
//  1. Plain settings (100 / 0 / off / 0) reproduce the original grid quantize tick-for-tick.
//  2. Strength: a note travels strength % of the way to its target, rounded half away from zero.
//  3. Swing: odd grid slots land swing % of the interval late; the nearest swung slot wins, ties to
//     the later one; 0 is exactly snapTick.
//  4. Note ends: the end quantizes to the straight grid; a collapsing note keeps one grid step; a
//     zero-length note stays zero-length.
//  5. Humanize: a deterministic offset within ± humanize % of the interval — the same seed and id
//     reproduce it, another seed changes the set, 0 % is exactly the strength law.
//  6. Refusals: out-of-range percentages leave the note untouched; the command routes plain settings
//     to the original verb and v2 settings to the new one, both undoable under the "Quantize" label.

#include "engine/Project.h"
#include "engine/ProjectUndo.h"
#include "engine/Time.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

using yesdaw::engine::EntityId;
using yesdaw::engine::MidiClip;
using yesdaw::engine::Note;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditCommand;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::ProjectUndoStack;
using yesdaw::engine::QuantizeSettings;
using yesdaw::engine::SampleRate;
using yesdaw::engine::SnapGrid;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;

namespace {

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

Note makeNote (std::uint8_t id, Tick start, Tick length) noexcept
{
    Note note;
    note.id = idFromLowByte (id);
    note.startTick = start;
    note.lengthTicks = length;
    note.key = 60;
    note.pitchNote = 60.0;
    return note;
}

Project makeProject (std::vector<Note> notes, Tick clipLength = 16384)
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };
    Track track;
    track.id = idFromLowByte (31);
    track.strip.name = "MIDI";
    project.tracks = { track };
    project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    MidiClip clip;
    clip.id = idFromLowByte (40);
    clip.trackId = track.id;
    clip.timelineLength = clipLength;
    clip.timeBase = TimeBase::TempoLocked;
    clip.notes = std::move (notes);
    project.midiClips = { clip };
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

QuantizeSettings settingsOf (Tick grid, std::uint8_t strength = 100, std::uint8_t swing = 0, bool ends = false,
                             std::uint8_t humanize = 0, std::uint32_t seed = 0) noexcept
{
    QuantizeSettings settings;
    settings.grid = SnapGrid { grid };
    settings.strengthPercent = strength;
    settings.swingPercent = swing;
    settings.noteEnds = ends;
    settings.humanizePercent = humanize;
    settings.humanizeSeed = seed;
    return settings;
}

const Note& firstNote (const Project& project) { return project.midiClips[0].notes[0]; }

} // namespace

TEST_CASE ("plain settings reproduce the original grid quantize tick-for-tick", "[engine][midi][quantize-v2][g3]")
{
    for (Tick start = 0; start < 4096; start += 7)
    {
        Project plain = makeProject ({ makeNote (50, start, 100) });
        Project v2 = plain;
        const EntityId clipId = plain.midiClips[0].id;
        REQUIRE (yesdaw::engine::quantizeNote (plain, clipId, idFromLowByte (50), SnapGrid { 512 }) == ProjectEditStatus::Applied);
        REQUIRE (yesdaw::engine::quantizeNoteWith (v2, clipId, idFromLowByte (50), settingsOf (512)) == ProjectEditStatus::Applied);
        INFO ("start " << start);
        REQUIRE (firstNote (v2) == firstNote (plain));
    }
    REQUIRE (settingsOf (512).isPlainGrid());
    REQUIRE_FALSE (settingsOf (512, 99).isPlainGrid());
}

TEST_CASE ("strength moves a note that share of the way to its target, rounded half away from zero", "[engine][midi][quantize-v2][strength][g3]")
{
    REQUIRE (yesdaw::engine::quantizeTowards (100, 0, 50) == 50);
    REQUIRE (yesdaw::engine::quantizeTowards (300, 512, 50) == 406);   // 212 * 0.5 = 106
    REQUIRE (yesdaw::engine::quantizeTowards (300, 512, 100) == 512);
    REQUIRE (yesdaw::engine::quantizeTowards (300, 512, 0) == 300);
    REQUIRE (yesdaw::engine::quantizeTowards (101, 0, 50) == 50);   // -50.5 rounds away: -51 ... wait: 101 - 51 = 50
    REQUIRE (yesdaw::engine::quantizeTowards (0, 101, 50) == 51);    // +50.5 rounds away from zero

    Project project = makeProject ({ makeNote (50, 100, 200), makeNote (51, 300, 200) });
    const EntityId clipId = project.midiClips[0].id;
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (50), settingsOf (512, 50)) == ProjectEditStatus::Applied);
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (51), settingsOf (512, 80)) == ProjectEditStatus::Applied);
    REQUIRE (project.midiClips[0].notes[0].startTick == 50);
    REQUIRE (project.midiClips[0].notes[1].startTick == 300 + (212 * 80 + 50) / 100);   // 470
    REQUIRE (project.midiClips[0].notes[0].lengthTicks == 200);   // strength never touches the length
}

TEST_CASE ("swing lands odd grid slots late; the nearest swung slot wins, ties go later; zero is snapTick", "[engine][midi][quantize-v2][swing][g3]")
{
    const QuantizeSettings swung = settingsOf (480, 100, 50);
    Tick target = 0;
    // Slots: 0, 720 (480 + 240), 960, 1680 (1440 + 240), 1920.
    REQUIRE (yesdaw::engine::quantizeTargetTick (700, swung, target)); REQUIRE (target == 720);
    REQUIRE (yesdaw::engine::quantizeTargetTick (500, swung, target)); REQUIRE (target == 720);
    REQUIRE (yesdaw::engine::quantizeTargetTick (300, swung, target)); REQUIRE (target == 0);
    REQUIRE (yesdaw::engine::quantizeTargetTick (1300, swung, target)); REQUIRE (target == 960);
    REQUIRE (yesdaw::engine::quantizeTargetTick (1320, swung, target)); REQUIRE (target == 1680);   // a tie (360 / 360) goes later
    REQUIRE (yesdaw::engine::quantizeTargetTick (1900, swung, target)); REQUIRE (target == 1920);

    // Swing 0 is exactly snapTick over a sweep.
    const QuantizeSettings straight = settingsOf (480);
    for (Tick tick = 0; tick < 5000; tick += 13)
    {
        Tick snapped = 0, v2 = 0;
        REQUIRE (yesdaw::engine::snapTick (tick, SnapGrid { 480 }, snapped));
        REQUIRE (yesdaw::engine::quantizeTargetTick (tick, straight, v2));
        REQUIRE (v2 == snapped);
    }

    // Through the Clip: a note near the swung slot lands ON it at strength 100, and the plain law
    // would have put it elsewhere.
    Project project = makeProject ({ makeNote (50, 700, 100) });
    const EntityId clipId = project.midiClips[0].id;
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (50), swung) == ProjectEditStatus::Applied);
    REQUIRE (firstNote (project).startTick == 720);
    REQUIRE_FALSE (settingsOf (480, 100, yesdaw::engine::kQuantizeSwingMaxPercent + 1).isValid());
}

TEST_CASE ("note ends quantize the end to the straight grid; a collapsing note keeps one step; zero length stays", "[engine][midi][quantize-v2][ends][g3]")
{
    Project project = makeProject ({ makeNote (50, 100, 700), makeNote (51, 1000, 30), makeNote (52, 2000, 0) });
    const EntityId clipId = project.midiClips[0].id;
    const QuantizeSettings ends = settingsOf (512, 100, 0, true);
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (50), ends) == ProjectEditStatus::Applied);
    REQUIRE (project.midiClips[0].notes[0].startTick == 0);
    REQUIRE (project.midiClips[0].notes[0].lengthTicks == 1024);   // end 800 -> 1024
    // 1000..1030: start -> 1024, end 1030 -> 1024: the end would meet the start, so one grid step.
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (51), ends) == ProjectEditStatus::Applied);
    REQUIRE (project.midiClips[0].notes[1].startTick == 1024);
    REQUIRE (project.midiClips[0].notes[1].lengthTicks == 512);
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (52), ends) == ProjectEditStatus::Applied);
    REQUIRE (project.midiClips[0].notes[2].startTick == 2048);
    REQUIRE (project.midiClips[0].notes[2].lengthTicks == 0);

    // Strength applies to the end too: 50 % of the way from 800 to 1024 is 912; start 100 -> 50.
    Project half = makeProject ({ makeNote (50, 100, 700) });
    REQUIRE (yesdaw::engine::quantizeNoteWith (half, clipId, idFromLowByte (50), settingsOf (512, 50, 0, true)) == ProjectEditStatus::Applied);
    REQUIRE (firstNote (half).startTick == 50);
    REQUIRE (firstNote (half).lengthTicks == 912 - 50);
}

TEST_CASE ("humanize is a deterministic offset within its range: same seed same feel, another seed another, zero is none", "[engine][midi][quantize-v2][humanize][g3]")
{
    const QuantizeSettings humanized = settingsOf (400, 100, 0, false, 25, 7);   // range = 100 ticks
    std::vector<Note> notes;
    for (std::uint8_t i = 0; i < 8; ++i)
        notes.push_back (makeNote (static_cast<std::uint8_t> (50 + i), 400 * (i + 1) + 37, 100));
    Project a = makeProject (notes);
    Project b = makeProject (notes);
    const EntityId clipId = a.midiClips[0].id;
    for (const Note& note : notes)
    {
        REQUIRE (yesdaw::engine::quantizeNoteWith (a, clipId, note.id, humanized) == ProjectEditStatus::Applied);
        REQUIRE (yesdaw::engine::quantizeNoteWith (b, clipId, note.id, humanized) == ProjectEditStatus::Applied);
    }
    REQUIRE (a.midiClips[0].notes == b.midiClips[0].notes);   // the same seed reproduces the feel
    bool anyOffGrid = false;
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        const Tick grid = 400 * static_cast<Tick> (i + 1);
        const Tick offset = a.midiClips[0].notes[i].startTick - grid;
        INFO ("note " << i << " offset " << offset);
        REQUIRE (std::abs (offset) <= 100);
        REQUIRE (offset == yesdaw::engine::quantizeHumanizeOffset (humanized, notes[i].id.bytes.data(), notes[i].id.bytes.size()));
        anyOffGrid = anyOffGrid || offset != 0;
    }
    REQUIRE (anyOffGrid);

    // Another seed changes the set; humanize 0 is exactly the strength law (on the grid here).
    Project c = makeProject (notes);
    QuantizeSettings other = humanized;
    other.humanizeSeed = 8;
    for (const Note& note : notes)
        REQUIRE (yesdaw::engine::quantizeNoteWith (c, clipId, note.id, other) == ProjectEditStatus::Applied);
    REQUIRE_FALSE (c.midiClips[0].notes == a.midiClips[0].notes);
    Project d = makeProject (notes);
    for (const Note& note : notes)
        REQUIRE (yesdaw::engine::quantizeNoteWith (d, clipId, note.id, settingsOf (400, 100, 0, false, 0, 7)) == ProjectEditStatus::Applied);
    for (std::size_t i = 0; i < notes.size(); ++i)
        REQUIRE (d.midiClips[0].notes[i].startTick == 400 * static_cast<Tick> (i + 1));
    REQUIRE (yesdaw::engine::quantizeHumanizeOffset (settingsOf (400), notes[0].id.bytes.data(), notes[0].id.bytes.size()) == 0);
}

TEST_CASE ("refusals leave the note untouched; the command routes plain and v2 settings and undoes under Quantize", "[engine][midi][quantize-v2][undo][g3]")
{
    Project project = makeProject ({ makeNote (50, 300, 200) });
    const EntityId clipId = project.midiClips[0].id;
    const Note before = firstNote (project);
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (50), settingsOf (512, 101)) == ProjectEditStatus::InvalidQuantizeSettings);
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (50), settingsOf (512, 100, 67)) == ProjectEditStatus::InvalidQuantizeSettings);
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (50), settingsOf (512, 100, 0, false, 101)) == ProjectEditStatus::InvalidQuantizeSettings);
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (50), settingsOf (0)) == ProjectEditStatus::InvalidSnapGrid);
    REQUIRE (yesdaw::engine::quantizeNoteWith (project, clipId, idFromLowByte (99), settingsOf (512)) == ProjectEditStatus::NoteNotFound);
    REQUIRE (firstNote (project) == before);

    ProjectUndoStack undo;
    const ProjectEditCommand plain = ProjectEditCommand::quantizeNote (clipId, idFromLowByte (50), SnapGrid { 512 });
    REQUIRE (plain.quantizeSettings().isPlainGrid());
    REQUIRE (undo.apply (project, plain).applied());
    REQUIRE (firstNote (project).startTick == 512);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (firstNote (project) == before);

    const ProjectEditCommand v2 = ProjectEditCommand::quantizeNoteWith (clipId, idFromLowByte (50), settingsOf (512, 50, 0, true));
    REQUIRE_FALSE (v2.quantizeSettings().isPlainGrid());
    REQUIRE (v2.quantizeSettings() == settingsOf (512, 50, 0, true));
    REQUIRE (undo.apply (project, v2).applied());
    REQUIRE (firstNote (project).startTick == 406);
    REQUIRE (firstNote (project).lengthTicks == 100);   // the end 500 travels half of the way to 512: 506, so 506 - 406
    REQUIRE (std::string_view (yesdaw::engine::projectEditVerbLabel (v2.verb)) == "Quantize");
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (firstNote (project) == before);
}
