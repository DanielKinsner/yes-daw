// G3.5 — MIDI clips at arrange level: the Clip's own settings at the flatten (mute, transpose,
// velocity offset, loop length), split and join, the verbs and their undo.
//
// Gates:
//  1. Flatten: a muted Clip emits nothing; transpose shifts every key (a note pushed off the
//     keyboard drops); the velocity offset adds and clamps; a loop repeats the content window to fill
//     the Clip, the last repeat cut at the Clip's end; plain settings flatten exactly as before.
//  2. Split: notes and control points land on the side they start on, re-based; a note crossing
//     the split is cut into a head (the old id) and a tail (a derived id); both halves lose the loop;
//     the right Clip sits right after the left. Refusals: on / outside the edges, a taken id.
//  3. Join: the right's notes and points append re-based with the right's transpose and velocity
//     offset baked in (the render stays), the left's settings win; refusals: gap, other track, order.
//  4. Verbs: every setting and both shape verbs are undoable under their labels; a split's undo
//     restores the one row, a join's undo restores the two.
//  5. The model's change test (a settings edit re-flattens) is pinned by the render: the same Clip
//     muted renders silence, transposed renders another pitch.

#include "engine/GraphBuilder.h"
#include "engine/Midi.h"
#include "engine/OfflineRenderer.h"
#include "engine/Project.h"
#include "engine/ProjectMixerProjection.h"
#include "engine/ProjectUndo.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::EventStream;
using yesdaw::engine::EventType;
using yesdaw::engine::MidiClip;
using yesdaw::engine::MidiControlEvent;
using yesdaw::engine::MidiControlKind;
using yesdaw::engine::MidiFlattenStatus;
using yesdaw::engine::Note;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditCommand;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::ProjectUndoStack;
using yesdaw::engine::SampleRate;
using yesdaw::engine::ScheduledMidiEvent;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::TempoMapView;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::Transport;

namespace {

constexpr double kSampleRate = 30720.0;   // 120 BPM => one tick == one frame

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

Note makeNote (std::uint8_t id, Tick start, Tick length, std::int16_t key, double velocity = 1.0) noexcept
{
    Note note;
    note.id = idFromLowByte (id);
    note.startTick = start;
    note.lengthTicks = length;
    note.key = key;
    note.pitchNote = static_cast<double> (key);
    note.normalizedVelocity = velocity;
    note.portIndex = 0;
    note.channel = 1;
    return note;
}

MidiControlEvent makeControl (std::uint8_t id, Tick tick, std::int16_t number, double value) noexcept
{
    MidiControlEvent control;
    control.id = idFromLowByte (id);
    control.tick = tick;
    control.kind = MidiControlKind::ControlChange;
    control.number = number;
    control.value = value;
    control.portIndex = 0;
    control.channel = 1;
    return control;
}

MidiClip makeClip (std::uint8_t id, Tick start, Tick length, std::vector<Note> notes, std::vector<MidiControlEvent> controls = {})
{
    MidiClip clip;
    clip.id = idFromLowByte (id);
    clip.trackId = idFromLowByte (31);
    clip.timelineStart = start;
    clip.timelineLength = length;
    clip.timeBase = TimeBase::TempoLocked;
    clip.notes = std::move (notes);
    clip.controlEvents = std::move (controls);
    REQUIRE (clip.isValid());
    return clip;
}

Project makeProject (std::vector<MidiClip> clips)
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { kSampleRate };
    Track track;
    track.id = idFromLowByte (31);
    track.strip.name = "MIDI";
    project.tracks = { track };
    project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    project.midiClips = std::move (clips);
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

std::vector<ScheduledMidiEvent> flatten (const MidiClip& clip)
{
    const std::vector<TempoChange> tempo { TempoChange { 0, 120.0, TempoCurve::Jump } };
    std::vector<ScheduledMidiEvent> timeline;
    REQUIRE (yesdaw::engine::flattenMidiClipForProjection (clip, TempoMapView { tempo.data(), tempo.size() },
                                                           SampleRate { kSampleRate }, timeline)
             == MidiFlattenStatus::Ok);
    return timeline;
}

struct OnOff { std::int64_t on; std::int64_t off; std::int16_t key; double velocity; };

// The note pairs of a timeline in on-order (key and velocity from the On).
std::vector<OnOff> notePairs (const std::vector<ScheduledMidiEvent>& timeline)
{
    std::vector<OnOff> pairs;
    for (const ScheduledMidiEvent& e : timeline)
        if (e.event.type == EventType::NoteOn)
        {
            OnOff pair { e.frame, -1, e.event.voice.key, e.event.payload.note.normalizedVelocity };
            for (const ScheduledMidiEvent& f : timeline)
                if (f.event.type == EventType::NoteOff && f.event.voice.noteId == e.event.voice.noteId && f.frame >= e.frame
                    && (pair.off < 0 || f.frame < pair.off))
                    pair.off = f.frame;
            pairs.push_back (pair);
        }
    return pairs;
}

std::vector<float> renderProject (const Project& project)
{
    OfflineRenderOptions options;
    options.maxBlockSize = 64;
    auto built = yesdaw::engine::buildProjectGraph (project, std::span<const DecodedAssetAudio> {}, options);
    REQUIRE (built.ok());
    const std::uint16_t channels = built.channels;
    const std::uint64_t frames = built.frames;
    std::vector<float> out (static_cast<std::size_t> (frames) * channels, 0.0f);
    std::vector<float> storage (static_cast<std::size_t> (channels) * static_cast<std::size_t> (options.maxBlockSize), 0.0f);
    std::vector<float*> outputs (channels, nullptr);
    for (std::uint16_t c = 0; c < channels; ++c)
        outputs[c] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (options.maxBlockSize);
    std::uint64_t offset = 0;
    while (offset < frames)
    {
        const int blockFrames = static_cast<int> (std::min<std::uint64_t> (frames - offset, static_cast<std::uint64_t> (options.maxBlockSize)));
        Transport transport;
        transport.projectSampleRate = built.sampleRate;
        transport.isPlaying = true;
        transport.hasTimelineFrame = true;
        transport.timelineFrame = static_cast<std::int64_t> (offset);
        EventStream events;
        built.graph->process (outputs.data(), channels, blockFrames, events, transport);
        for (std::uint16_t c = 0; c < channels; ++c)
            for (int i = 0; i < blockFrames; ++i)
                out[(offset + static_cast<std::uint64_t> (i)) * channels + c] = outputs[c][i];
        offset += static_cast<std::uint64_t> (blockFrames);
    }
    return out;
}

bool allZero (const std::vector<float>& samples)
{
    for (const float s : samples)
        if (s != 0.0f)
            return false;
    return true;
}

} // namespace

TEST_CASE ("the flatten honours mute, transpose, velocity offset and the loop; plain settings are unchanged",
           "[engine][midi][midi-clip-ops][flatten][g3]")
{
    const MidiClip plain = makeClip (40, 1000, 4000, { makeNote (50, 100, 200, 60, 0.5), makeNote (51, 500, 100, 127, 0.9) },
                                     { makeControl (60, 50, 1, 0.5) });
    const std::vector<ScheduledMidiEvent> reference = flatten (plain);
    REQUIRE (reference.size() == 5u);   // two notes on / off + one control

    // Mute: nothing, still Ok.
    MidiClip muted = plain;
    muted.muted = true;
    REQUIRE (flatten (muted).empty());

    // Transpose +5: every key up five; the note at 127 drops off the keyboard.
    MidiClip up = plain;
    up.transposeSemitones = 5;
    {
        const std::vector<OnOff> pairs = notePairs (flatten (up));
        REQUIRE (pairs.size() == 1u);
        REQUIRE (pairs[0].key == 65);
        REQUIRE (pairs[0].on == 1100);
        REQUIRE (pairs[0].off == 1300);
    }
    MidiClip down = plain;
    down.transposeSemitones = -12;
    {
        const std::vector<OnOff> pairs = notePairs (flatten (down));
        REQUIRE (pairs.size() == 2u);
        REQUIRE (pairs[0].key == 48);
        REQUIRE (pairs[1].key == 115);
    }

    // Velocity offset +0.3: 0.5 -> 0.8, 0.9 -> 1.0 (clamped); -0.6: 0.5 -> 0, 0.9 -> 0.3.
    MidiClip louder = plain;
    louder.velocityOffset = 0.3;
    {
        const std::vector<OnOff> pairs = notePairs (flatten (louder));
        REQUIRE (pairs[0].velocity == Catch::Approx (0.8));
        REQUIRE (pairs[1].velocity == Catch::Approx (1.0));
    }
    MidiClip softer = plain;
    softer.velocityOffset = -0.6;
    {
        const std::vector<OnOff> pairs = notePairs (flatten (softer));
        REQUIRE (pairs[0].velocity == Catch::Approx (0.0));
        REQUIRE (pairs[1].velocity == Catch::Approx (0.3));
    }

    // Loop 1000 in a 4000 Clip: four repeats of the content window; the note at 500..600 repeats at
    // 1500, 2500, 3500; the control at 50 repeats at 1050, 2050, 3050. A note beyond the window (at
    // 1200) never sounds. A repeat that would cross the Clip's end is cut there.
    MidiClip looped = makeClip (41, 0, 4000, { makeNote (50, 500, 100, 60), makeNote (51, 1200, 100, 62), makeNote (52, 900, 300, 64) },
                                { makeControl (60, 50, 1, 0.5) });
    looped.loopLengthTicks = 1000;
    REQUIRE (looped.isValid());
    {
        const std::vector<ScheduledMidiEvent> timeline = flatten (looped);
        const std::vector<OnOff> pairs = notePairs (timeline);
        std::vector<std::int64_t> ons60, ons64;
        for (const OnOff& pair : pairs)
        {
            if (pair.key == 60) ons60.push_back (pair.on);
            if (pair.key == 64) ons64.push_back (pair.on);
            REQUIRE (pair.key != 62);
        }
        REQUIRE (ons60 == std::vector<std::int64_t> { 500, 1500, 2500, 3500 });
        REQUIRE (ons64 == std::vector<std::int64_t> { 900, 1900, 2900, 3900 });
        for (const OnOff& pair : pairs)
            if (pair.key == 64)
                REQUIRE (pair.off == std::min<std::int64_t> (pair.on + 300, 4000));   // the last repeat is cut at 4000
        std::vector<std::int64_t> controls;
        for (const ScheduledMidiEvent& e : timeline)
            if (e.event.type == EventType::Midi1)
                controls.push_back (e.frame);
        REQUIRE (controls == std::vector<std::int64_t> { 50, 1050, 2050, 3050 });
    }
    // A loop as long as the Clip (or 0) is the plain law.
    MidiClip wholeLoop = plain;
    wholeLoop.loopLengthTicks = plain.timelineLength;
    REQUIRE (flatten (wholeLoop).size() == reference.size());
    MidiClip invalidLoop = plain;
    invalidLoop.loopLengthTicks = plain.timelineLength + 1;
    REQUIRE_FALSE (invalidLoop.isValid());
    MidiClip invalidTranspose = plain;
    invalidTranspose.transposeSemitones = 49;
    REQUIRE_FALSE (invalidTranspose.isValid());
}

TEST_CASE ("split cuts a MIDI Clip in two: notes and points re-base, a crossing note is cut, the loop drops, refusals hold",
           "[engine][midi][midi-clip-ops][split][g3]")
{
    Project project = makeProject ({ makeClip (40, 1000, 4000,
        { makeNote (50, 100, 200, 60), makeNote (51, 1900, 400, 62), makeNote (52, 3000, 100, 64) },
        { makeControl (60, 50, 1, 0.5), makeControl (61, 2500, 1, 0.9) }) });
    project.midiClips[0].loopLengthTicks = 2000;
    project.midiClips[0].transposeSemitones = 3;
    const EntityId leftId = project.midiClips[0].id;
    const EntityId rightId = idFromLowByte (41);

    REQUIRE (yesdaw::engine::splitMidiClip (project, leftId, rightId, 2000) == ProjectEditStatus::Applied);
    REQUIRE (project.midiClips.size() == 2u);
    const MidiClip& left = project.midiClips[0];
    const MidiClip& right = project.midiClips[1];
    REQUIRE (left.id == leftId);
    REQUIRE (right.id == rightId);
    REQUIRE (left.timelineStart == 1000);
    REQUIRE (left.timelineLength == 2000);
    REQUIRE (right.timelineStart == 3000);
    REQUIRE (right.timelineLength == 2000);
    REQUIRE (left.loopLengthTicks == 0);
    REQUIRE (right.loopLengthTicks == 0);
    REQUIRE (right.transposeSemitones == 3);   // the settings carry to both halves
    REQUIRE (left.notes.size() == 2u);   // 100..300 whole; the head of 1900..2300
    REQUIRE (left.notes[1].id == idFromLowByte (51));
    REQUIRE (left.notes[1].startTick == 1900);
    REQUIRE (left.notes[1].lengthTicks == 100);
    REQUIRE (right.notes.size() == 2u);   // the tail of 51 (a derived id) at 0..300, then 52 at 1000
    REQUIRE (right.notes[0].startTick == 0);
    REQUIRE (right.notes[0].lengthTicks == 300);
    REQUIRE (right.notes[0].key == 62);
    REQUIRE (right.notes[0].id != idFromLowByte (51));
    REQUIRE (right.notes[0].id.isValid());
    REQUIRE (right.notes[1].id == idFromLowByte (52));
    REQUIRE (right.notes[1].startTick == 1000);
    REQUIRE (left.controlEvents.size() == 1u);
    REQUIRE (right.controlEvents.size() == 1u);
    REQUIRE (right.controlEvents[0].tick == 500);
    REQUIRE (project.hasValidAssetClipIndirection());

    // The split is deterministic: the same split of the same Clip derives the same tail id.
    Project again = makeProject ({ makeClip (40, 1000, 4000, { makeNote (51, 1900, 400, 62) }) });
    REQUIRE (yesdaw::engine::splitMidiClip (again, leftId, rightId, 2000) == ProjectEditStatus::Applied);
    REQUIRE (again.midiClips[1].notes[0].id == right.notes[0].id);

    // Refusals: on / past the edges, a taken right id, an unknown Clip.
    Project fresh = makeProject ({ makeClip (40, 0, 1000, { makeNote (50, 0, 100, 60) }) });
    REQUIRE (yesdaw::engine::splitMidiClip (fresh, leftId, rightId, 0) == ProjectEditStatus::InvalidTimelineWindow);
    REQUIRE (yesdaw::engine::splitMidiClip (fresh, leftId, rightId, 1000) == ProjectEditStatus::InvalidTimelineWindow);
    REQUIRE (yesdaw::engine::splitMidiClip (fresh, leftId, leftId, 500) == ProjectEditStatus::DuplicateEntityId);
    REQUIRE (yesdaw::engine::splitMidiClip (fresh, idFromLowByte (99), rightId, 500) == ProjectEditStatus::MidiClipNotFound);
    REQUIRE (fresh.midiClips.size() == 1u);
}

TEST_CASE ("join appends the right Clip onto the left with its settings baked in; refusals: gap, track, order",
           "[engine][midi][midi-clip-ops][join][g3]")
{
    MidiClip left = makeClip (40, 1000, 2000, { makeNote (50, 100, 200, 60, 0.5) });
    left.transposeSemitones = 2;
    left.velocityOffset = 0.1;
    MidiClip right = makeClip (41, 3000, 1000, { makeNote (51, 100, 200, 62, 0.5), makeNote (52, 500, 100, 126, 0.5) },
                               { makeControl (60, 400, 1, 0.7) });
    right.transposeSemitones = 5;    // three more than the left: its notes bake +3
    right.velocityOffset = -0.2;     // 0.3 less than the left: its notes bake -0.3
    right.loopLengthTicks = 500;
    Project project = makeProject ({ left, right });

    REQUIRE (yesdaw::engine::joinMidiClips (project, left.id, right.id) == ProjectEditStatus::Applied);
    REQUIRE (project.midiClips.size() == 1u);
    const MidiClip& joined = project.midiClips[0];
    REQUIRE (joined.id == left.id);
    REQUIRE (joined.timelineLength == 3000);
    REQUIRE (joined.transposeSemitones == 2);
    REQUIRE (joined.velocityOffset == 0.1);
    REQUIRE (joined.loopLengthTicks == 0);
    // 50 stays; 51 bakes the right's extra +3 and -0.3; 52 at 126 + 3 = 129 is off the keyboard —
    // it never sounded under the right's +5 either — so the join drops it rather than invent a pitch.
    REQUIRE (joined.notes.size() == 2u);
    REQUIRE (joined.notes[1].id == idFromLowByte (51));
    REQUIRE (joined.notes[1].startTick == 2100);
    REQUIRE (joined.notes[1].key == 65);
    REQUIRE (joined.notes[1].normalizedVelocity == Catch::Approx (0.2));
    REQUIRE (joined.controlEvents.size() == 1u);
    REQUIRE (joined.controlEvents[0].tick == 2400);
}

TEST_CASE ("join's render law: the joined Clip renders what the two Clips rendered (a loop excepted)",
           "[engine][midi][midi-clip-ops][join][render][g3]")
{
    MidiClip left = makeClip (40, 0, 2048, { makeNote (50, 0, 1024, 60, 0.5) });
    left.transposeSemitones = 2;
    MidiClip right = makeClip (41, 2048, 2048, { makeNote (51, 0, 1024, 60, 0.5) });
    right.transposeSemitones = 5;
    Project two = makeProject ({ left, right });
    const std::vector<float> before = renderProject (two);
    Project one = two;
    REQUIRE (yesdaw::engine::joinMidiClips (one, left.id, right.id) == ProjectEditStatus::Applied);
    REQUIRE (renderProject (one) == before);

    // Refusals: a gap, another track, the wrong order, unknown ids.
    Project gap = makeProject ({ left, makeClip (41, 2100, 1000, {}) });
    REQUIRE (yesdaw::engine::joinMidiClips (gap, left.id, idFromLowByte (41)) == ProjectEditStatus::InvalidMidiClipValue);
    Project wrongOrder = makeProject ({ right, left });
    REQUIRE (yesdaw::engine::joinMidiClips (wrongOrder, left.id, right.id) == ProjectEditStatus::InvalidMidiClipValue);
    Project otherTrack = makeProject ({ left, right });
    Track second;
    second.id = idFromLowByte (32);
    second.strip.name = "Other";
    otherTrack.tracks.push_back (second);
    otherTrack.midiClips[1].trackId = second.id;
    REQUIRE (yesdaw::engine::joinMidiClips (otherTrack, left.id, right.id) == ProjectEditStatus::InvalidMidiClipValue);
    REQUIRE (yesdaw::engine::joinMidiClips (otherTrack, left.id, idFromLowByte (99)) == ProjectEditStatus::MidiClipNotFound);
}

TEST_CASE ("the settings and shape verbs are undoable; a split's undo restores one row, a join's undo restores two",
           "[engine][midi][midi-clip-ops][undo][g3]")
{
    Project project = makeProject ({ makeClip (40, 0, 4000, { makeNote (50, 100, 200, 60), makeNote (51, 2100, 200, 62) }) });
    const EntityId clipId = project.midiClips[0].id;
    const EntityId rightId = idFromLowByte (41);
    ProjectUndoStack undo;

    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiClipMuted (clipId, true)).applied());
    REQUIRE (project.midiClips[0].muted);
    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiClipTranspose (clipId, -5)).applied());
    REQUIRE (project.midiClips[0].transposeSemitones == -5);
    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiClipVelocityOffset (clipId, 0.25)).applied());
    REQUIRE (project.midiClips[0].velocityOffset == 0.25);
    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiClipLoopLength (clipId, 1000)).applied());
    REQUIRE (project.midiClips[0].loopLengthTicks == 1000);
    REQUIRE_FALSE (undo.apply (project, ProjectEditCommand::setMidiClipTranspose (clipId, 49)).applied());
    REQUIRE_FALSE (undo.apply (project, ProjectEditCommand::setMidiClipVelocityOffset (clipId, 1.5)).applied());
    REQUIRE_FALSE (undo.apply (project, ProjectEditCommand::setMidiClipLoopLength (clipId, 4001)).applied());
    for (int i = 0; i < 4; ++i)
        REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE_FALSE (project.midiClips[0].muted);
    REQUIRE (project.midiClips[0].transposeSemitones == 0);
    REQUIRE (project.midiClips[0].velocityOffset == 0.0);
    REQUIRE (project.midiClips[0].loopLengthTicks == 0);
    for (int i = 0; i < 4; ++i)
        REQUIRE (undo.redo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0].loopLengthTicks == 1000);
    REQUIRE (std::string_view (yesdaw::engine::projectEditVerbLabel (yesdaw::engine::ProjectEditVerb::SplitMidiClip)) == "Split MIDI Clip");

    const Project beforeSplit = project;
    REQUIRE (undo.apply (project, ProjectEditCommand::splitMidiClip (clipId, rightId, 2000)).applied());
    REQUIRE (project.midiClips.size() == 2u);
    REQUIRE (project.midiClips[1].id == rightId);
    REQUIRE (project.midiClips[1].notes.size() == 1u);
    REQUIRE (project.midiClips[1].notes[0].startTick == 100);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips == beforeSplit.midiClips);
    REQUIRE (undo.redo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips.size() == 2u);

    const Project beforeJoin = project;
    REQUIRE (undo.apply (project, ProjectEditCommand::joinMidiClips (clipId, rightId)).applied());
    REQUIRE (project.midiClips.size() == 1u);
    REQUIRE (project.midiClips[0].notes.size() == 2u);
    REQUIRE (project.midiClips[0].notes[1].startTick == 2100);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips == beforeJoin.midiClips);
    REQUIRE (undo.redo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips.size() == 1u);
}

TEST_CASE ("the render honours a MIDI Clip's mute and transpose", "[engine][midi][midi-clip-ops][render][g3]")
{
    Project plain = makeProject ({ makeClip (40, 0, 4096, { makeNote (50, 0, 2048, 60) }) });
    const std::vector<float> reference = renderProject (plain);
    REQUIRE_FALSE (allZero (reference));

    Project muted = plain;
    muted.midiClips[0].muted = true;
    REQUIRE (allZero (renderProject (muted)));

    Project up = plain;
    up.midiClips[0].transposeSemitones = 12;
    Project asNote = plain;
    asNote.midiClips[0].notes[0].key = 72;
    asNote.midiClips[0].notes[0].pitchNote = 72.0;
    REQUIRE (renderProject (up) == renderProject (asNote));   // a Clip transpose is the note's transpose at render
    REQUIRE (renderProject (up) != reference);
}

TEST_CASE ("native MIDI occurrence counts retain two cycles near the Tick limit",
           "[engine][midi][midi-clip-ops][midi-native-occurrence][regression]")
{
    constexpr auto limit = std::numeric_limits<yesdaw::engine::Tick>::max();
    MidiClip clip = makeClip (40, 0, limit, { makeNote (50, 0, 1, 60) },
                             { makeControl (60, 0, 1, 0.5) });
    clip.loopLengthTicks = limit - 1;
    REQUIRE (clip.isValid());
    std::vector<ScheduledMidiEvent> timeline;
    REQUIRE (yesdaw::engine::flattenMidiClipToTimeline (
        clip, [] (yesdaw::engine::Tick tick, double& frame)
        {
            frame = static_cast<double> (tick) / 4.0;
            return tick >= 0;
        }, timeline) == MidiFlattenStatus::Ok);
    CHECK (timeline.size() == 6u);
    CHECK (std::count_if (timeline.begin(), timeline.end(), [] (const auto& event)
    {
        return event.event.type == EventType::NoteOn;
    }) == 2);
    CHECK (std::count_if (timeline.begin(), timeline.end(), [] (const auto& event)
    {
        return event.event.type == EventType::NoteOff;
    }) == 2);
    CHECK (std::count_if (timeline.begin(), timeline.end(), [] (const auto& event)
    {
        return event.event.type == EventType::Midi1;
    }) == 2);
}

TEST_CASE ("native MIDI extraction preserves the existing projection refusal output",
           "[engine][midi][midi-clip-ops][midi-native-occurrence][contract]")
{
    MidiClip clip = makeClip (40, 10, 100, { makeNote (50, 0, 5, 60) });
    std::vector<ScheduledMidiEvent> timeline { ScheduledMidiEvent {} };
    std::vector<yesdaw::engine::Tick> visited;
    const auto refuseOff = [&] (yesdaw::engine::Tick tick, double& frame)
    {
        visited.push_back (tick);
        frame = static_cast<double> (tick);
        return tick != 15;
    };
    REQUIRE (yesdaw::engine::flattenMidiClipToTimeline (clip, refuseOff, timeline)
             == MidiFlattenStatus::InvalidInput);
    REQUIRE (visited == std::vector<yesdaw::engine::Tick> { 10, 15 });
    REQUIRE (timeline.size() == 1u);
    REQUIRE (timeline[0].frame == 10);
    REQUIRE (timeline[0].event.type == EventType::NoteOn);

    clip.muted = true;
    visited.clear();
    REQUIRE (yesdaw::engine::flattenMidiClipToTimeline (clip, refuseOff, timeline)
             == MidiFlattenStatus::Ok);
    REQUIRE (timeline.empty());
    REQUIRE (visited.empty());

    clip.notes[0].key = 128;
    REQUIRE (yesdaw::engine::flattenMidiClipToTimeline (clip, refuseOff, timeline)
             == MidiFlattenStatus::InvalidInput);
    REQUIRE (timeline.empty());
    REQUIRE (visited.empty());
}

TEST_CASE ("raw MIDI occurrences retain native identities overlapping tails and effective payloads",
           "[engine][midi][midi-clip-ops][midi-native-occurrence]")
{
    MidiClip clip = makeClip (40, 100, 2500,
        { makeNote (50, 900, 1200, 60, 0.5), makeNote (51, 100, 0, 62, 0.9),
          makeNote (52, 100, 1, 127), makeNote (53, 1200, 1, 64) },
        { makeControl (60, 50, 64, 1.0) });
    clip.loopLengthTicks = 1000;
    clip.transposeSemitones = 5;
    clip.velocityOffset = 0.3;
    const auto preflight = yesdaw::engine::preflightMidiClipOccurrences (clip, 13);
    REQUIRE (preflight.status == MidiFlattenStatus::Ok);
    REQUIRE (preflight.events == 13u);
    std::vector<yesdaw::engine::MidiNativeOccurrence> raw;
    REQUIRE (yesdaw::engine::visitMidiClipOccurrences (clip, 13, [&] (const auto& occurrence)
    {
        raw.push_back (occurrence);
        return true;
    }) == MidiFlattenStatus::Ok);
    REQUIRE (raw.size() == 13u);

    const std::array<Tick, 13> positions { 1000, 2200, 2000, 2600, 200, 200,
                                         1200, 1200, 2200, 2200, 150, 1150, 2150 };
    const std::array<std::uint8_t, 13> sourceIds { 50, 50, 50, 50, 51, 51, 51, 51, 51, 51, 60, 60, 60 };
    const std::array<std::uint64_t, 13> ordinals { 0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 0, 1, 2 };
    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        CAPTURE (i);
        REQUIRE (raw[i].nativePosition == positions[i]);
        REQUIRE (raw[i].timeBase == TimeBase::TempoLocked);
        REQUIRE (raw[i].clipId == clip.id);
        REQUIRE (raw[i].sourceId == idFromLowByte (sourceIds[i]));
        REQUIRE (raw[i].repeatOrdinal == ordinals[i]);
        REQUIRE (raw[i].event.timeInBlock == 0u);
        if (i < 10)
        {
            REQUIRE (raw[i].event.type == (i % 2u == 0u ? EventType::NoteOn : EventType::NoteOff));
            const bool firstNote = i < 4;
            REQUIRE (raw[i].event.voice.key == (firstNote ? 65 : 67));
            REQUIRE (raw[i].event.payload.note.pitchNote == (firstNote ? 65.0 : 67.0));
            const double expectedVelocity = i % 2u == 1u ? 0.0 : (firstNote ? 0.8 : 1.0);
            REQUIRE (raw[i].event.payload.note.normalizedVelocity == Catch::Approx (expectedVelocity));
        }
        else
        {
            REQUIRE (raw[i].event.type == EventType::Midi1);
            REQUIRE (raw[i].event.payload.midi1.status == 0xB1);
            REQUIRE (raw[i].event.payload.midi1.data1 == 64);
            REQUIRE (raw[i].event.payload.midi1.data2 == 127);
        }
    }
}

TEST_CASE ("raw MIDI preserves exact native coordinates before double conversion and Frame collapse",
           "[engine][midi][midi-clip-ops][midi-native-occurrence]")
{
    constexpr Tick origin = 9'007'199'254'740'993;
    MidiClip fixed = makeClip (40, origin, 10, { makeNote (50, 1, 1, 60) },
                              { makeControl (60, 2, 1, 0.5) });
    fixed.timeBase = TimeBase::SampleLocked;
    std::vector<yesdaw::engine::MidiNativeOccurrence> raw;
    const auto collect = [&] (const auto& occurrence) { raw.push_back (occurrence); return true; };
    REQUIRE (yesdaw::engine::visitMidiClipOccurrences (fixed, 3, collect) == MidiFlattenStatus::Ok);
    REQUIRE (raw.size() == 3u);
    REQUIRE (raw[0].nativePosition == origin + 1);
    REQUIRE (raw[1].nativePosition == origin + 2);
    REQUIRE (raw[2].nativePosition == origin + 2);
    for (const auto& occurrence : raw)
        REQUIRE (occurrence.timeBase == TimeBase::SampleLocked);

    MidiClip fast = makeClip (41, 0, 5, { makeNote (51, 1, 1, 60) },
                             { makeControl (61, 3, 1, 0.5) });
    raw.clear();
    REQUIRE (yesdaw::engine::visitMidiClipOccurrences (fast, 3, collect) == MidiFlattenStatus::Ok);
    REQUIRE (raw.size() == 3u);
    REQUIRE (raw[0].nativePosition == 1);
    REQUIRE (raw[1].nativePosition == 2);
    REQUIRE (raw[2].nativePosition == 3);
    const std::array<TempoChange, 1> tempo { TempoChange { 0, 100'000'000.0, TempoCurve::Jump } };
    std::vector<ScheduledMidiEvent> timeline;
    REQUIRE (yesdaw::engine::flattenMidiClipForProjection (fast, { tempo.data(), tempo.size() },
             SampleRate { 48000.0 }, timeline) == MidiFlattenStatus::Ok);
    REQUIRE (timeline.size() == 3u);
    for (const auto& event : timeline)
        REQUIRE (event.frame == 0);
    REQUIRE (timeline[0].event.type == EventType::Midi1);
    REQUIRE (timeline[1].event.type == EventType::NoteOn);
    REQUIRE (timeline[2].event.type == EventType::NoteOff);
}

TEST_CASE ("raw MIDI applies each controller wire law on every repeat",
           "[engine][midi][midi-clip-ops][midi-native-occurrence]")
{
    std::vector<MidiControlEvent> controls {
        makeControl (60, 0, 64, 1.0), makeControl (61, 0, 0, 1.0),
        makeControl (62, 0, 0, 0.5), makeControl (63, 0, 60, 0.25), makeControl (64, 0, 12, 0.0)
    };
    controls[1].kind = MidiControlKind::PitchBend;
    controls[2].kind = MidiControlKind::ChannelPressure;
    controls[3].kind = MidiControlKind::PolyPressure;
    controls[4].kind = MidiControlKind::ProgramChange;
    MidiClip clip = makeClip (40, 17, 300, {}, controls);
    clip.loopLengthTicks = 100;
    const std::array<std::uint8_t, 5> statuses { 0xB1, 0xE1, 0xD1, 0xA1, 0xC1 };
    const std::array<std::uint8_t, 5> data1 { 64, 127, 64, 60, 12 };
    const std::array<std::uint8_t, 5> data2 { 127, 127, 0, 32, 0 };
    std::size_t visited = 0;
    REQUIRE (yesdaw::engine::visitMidiClipOccurrences (clip, 15, [&] (const auto& occurrence)
    {
        const auto lane = visited / 3u;
        const auto cycle = visited % 3u;
        REQUIRE (lane < controls.size());
        REQUIRE (occurrence.nativePosition == 17 + static_cast<Tick> (cycle) * 100);
        REQUIRE (occurrence.sourceId == controls[lane].id);
        REQUIRE (occurrence.repeatOrdinal == cycle);
        REQUIRE (occurrence.event.type == EventType::Midi1);
        REQUIRE (occurrence.event.payload.midi1.status == statuses[lane]);
        REQUIRE (occurrence.event.payload.midi1.data1 == data1[lane]);
        REQUIRE (occurrence.event.payload.midi1.data2 == data2[lane]);
        REQUIRE (occurrence.event.voice.portIndex == 0);
        REQUIRE (occurrence.event.voice.channel == 1);
        ++visited;
        return true;
    }) == MidiFlattenStatus::Ok);
    REQUIRE (visited == 15u);
}

TEST_CASE ("raw MIDI preflights budgets and numeric refusals before invoking visitors",
           "[engine][midi][midi-clip-ops][midi-native-occurrence]")
{
    constexpr Tick limit = std::numeric_limits<Tick>::max();
    MidiClip clip = makeClip (40, 10, 100, { makeNote (50, 0, 5, 60) },
                             { makeControl (60, 20, 1, 0.5) });
    const auto refused = [&] (std::size_t budget, MidiFlattenStatus expected)
    {
        const auto preflight = yesdaw::engine::preflightMidiClipOccurrences (clip, budget);
        REQUIRE (preflight.status == expected);
        REQUIRE (preflight.events == 0u);
        std::size_t callbacks = 0;
        REQUIRE (yesdaw::engine::visitMidiClipOccurrences (clip, budget, [&] (const auto&)
        {
            ++callbacks;
            return true;
        }) == expected);
        REQUIRE (callbacks == 0u);
    };
    SECTION ("a later controller exceeds an otherwise fitting Note budget")
    {
        refused (2, MidiFlattenStatus::OutputTooSmall);
        const auto exact = yesdaw::engine::preflightMidiClipOccurrences (clip, 3);
        REQUIRE (exact.status == MidiFlattenStatus::Ok);
        REQUIRE (exact.events == 3u);
    }
    SECTION ("the total event count cannot wrap size_t")
    {
        clip.timelineStart = 0;
        clip.timelineLength = limit;
        clip.loopLengthTicks = 1;
        clip.notes = { makeNote (50, 0, 1, 60), makeNote (51, 0, 1, 62) };
        clip.controlEvents.clear();
        refused (std::numeric_limits<std::size_t>::max(), MidiFlattenStatus::OutputTooSmall);
    }
    SECTION ("absolute Note endpoints overflow before any earlier valid onset is emitted")
    {
        clip.timelineStart = limit - 5;
        clip.notes[0].lengthTicks = 10;
        clip.controlEvents.clear();
        refused (3, MidiFlattenStatus::InvalidInput);
    }
    SECTION ("absolute controller endpoints are also checked")
    {
        clip.timelineStart = limit - 5;
        clip.notes.clear();
        refused (1, MidiFlattenStatus::InvalidInput);
    }
    SECTION ("an unrepresentable repeated Note tail retains the existing numeric refusal")
    {
        clip.timelineStart = 0;
        clip.timelineLength = limit;
        clip.loopLengthTicks = limit - 1;
        clip.notes = { makeNote (50, 0, limit, 60) };
        clip.controlEvents.clear();
        refused (4, MidiFlattenStatus::InvalidInput);
    }
    SECTION ("a valid muted Clip needs no output budget")
    {
        clip.muted = true;
        const auto preflight = yesdaw::engine::preflightMidiClipOccurrences (clip, 0);
        REQUIRE (preflight.status == MidiFlattenStatus::Ok);
        REQUIRE (preflight.events == 0u);
        std::size_t callbacks = 0;
        REQUIRE (yesdaw::engine::visitMidiClipOccurrences (clip, 0, [&] (const auto&)
        {
            ++callbacks;
            return true;
        }) == MidiFlattenStatus::Ok);
        REQUIRE (callbacks == 0u);
    }
    SECTION ("a visitor refusal stops without rolling back prior callbacks")
    {
        std::size_t callbacks = 0;
        REQUIRE (yesdaw::engine::visitMidiClipOccurrences (clip, 3, [&] (const auto&)
        {
            return ++callbacks < 2u;
        }) == MidiFlattenStatus::InvalidInput);
        REQUIRE (callbacks == 2u);
    }
}

TEST_CASE ("raw MIDI retains the existing Note and controller end-boundary distinction",
           "[engine][midi][midi-clip-ops][midi-native-occurrence]")
{
    MidiClip clip = makeClip (40, 10, 100, { makeNote (50, 100, 0, 60) },
                             { makeControl (60, 100, 1, 0.5) });
    for (const Tick length : { Tick { 100 }, Tick { 0 } })
    {
        clip.timelineLength = length;
        clip.notes[0].startTick = length;
        clip.controlEvents[0].tick = length;
        const auto preflight = yesdaw::engine::preflightMidiClipOccurrences (clip, 1);
        REQUIRE (preflight.status == MidiFlattenStatus::Ok);
        REQUIRE (preflight.events == 1u);
        std::size_t callbacks = 0;
        REQUIRE (yesdaw::engine::visitMidiClipOccurrences (clip, 1, [&] (const auto& occurrence)
        {
            REQUIRE (occurrence.nativePosition == 10 + length);
            REQUIRE (occurrence.sourceId == clip.controlEvents[0].id);
            REQUIRE (occurrence.event.type == EventType::Midi1);
            ++callbacks;
            return true;
        }) == MidiFlattenStatus::Ok);
        REQUIRE (callbacks == 1u);
    }
}

TEST_CASE ("MIDI Frame projection refuses the int64 upper boundary before casting",
           "[engine][midi][midi-clip-ops][midi-native-occurrence][midi-frame-domain][regression]")
{
    const MidiClip clip = makeClip (40, 0, 10, { makeNote (50, 0, 1, 60) });
    std::vector<ScheduledMidiEvent> timeline;
    SECTION ("the exact exclusive limit and all larger finite doubles are refused")
    {
        for (const double projected : { 0x1p63, std::nextafter (0x1p63, std::numeric_limits<double>::infinity()),
                                         std::numeric_limits<double>::max() })
        {
            CAPTURE (projected);
            std::size_t callbacks = 0;
            CHECK (yesdaw::engine::flattenMidiClipToTimeline (clip, [&] (Tick, double& frame)
            {
                ++callbacks;
                frame = projected;
                return true;
            }, timeline) == MidiFlattenStatus::InvalidInput);
            CHECK (timeline.empty());
            CHECK (callbacks == 1u);
        }
    }
    SECTION ("the greatest representable double below the limit remains valid")
    {
        const double projected = std::nextafter (0x1p63, 0.0);
        REQUIRE (yesdaw::engine::flattenMidiClipToTimeline (clip, [&] (Tick, double& frame)
        {
            frame = projected;
            return true;
        }, timeline) == MidiFlattenStatus::Ok);
        REQUIRE (timeline.size() == 2u);
        for (const auto& event : timeline)
            REQUIRE (event.frame == std::numeric_limits<std::int64_t>::max() - 1023);
    }
    SECTION ("negative and nonfinite positions keep their existing refusal")
    {
        for (const double projected : { -1.0, std::numeric_limits<double>::infinity(),
                                         std::numeric_limits<double>::quiet_NaN() })
        {
            REQUIRE (yesdaw::engine::flattenMidiClipToTimeline (clip, [&] (Tick, double& frame)
            {
                frame = projected;
                return true;
            }, timeline) == MidiFlattenStatus::InvalidInput);
            REQUIRE (timeline.empty());
        }
    }
    SECTION ("an overflowing Off preserves the preceding valid On")
    {
        CHECK (yesdaw::engine::flattenMidiClipToTimeline (clip, [] (Tick tick, double& frame)
        {
            frame = tick == 0 ? 123.0 : 0x1p63;
            return true;
        }, timeline) == MidiFlattenStatus::InvalidInput);
        CHECK (timeline.size() == 1u);
        REQUIRE_FALSE (timeline.empty());
        CHECK (timeline[0].frame == 123);
        CHECK (timeline[0].event.type == EventType::NoteOn);
    }
}
