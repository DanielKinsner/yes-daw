// G3.3 — MIDI CC, pitch bend, aftertouch, program change: storage, flatten, render, undo.
//
// Gates:
//  1. The edit model: a MidiControlEvent validates its kind, number, value range and voice
//     address; a MidiClip refuses a point past its end.
//  2. The flatten: control points ride the Clip's note timeline; at one frame a control message
//     precedes every note event; the wire bytes are the 7-bit (14-bit for a bend) quantization of
//     the normalized value, on the event's channel.
//  3. SimpleSynth: CC64 holds a released note until the pedal lifts; pitch bend moves a pure sine
//     by the bend range (closed form on zero crossings); CC1 opens the filter from the cutoff
//     parameter and does nothing at the bypass cutoff; aftertouch and program change leave the
//     render bit-identical (they stay on the stream for an instrument that reads them).
//  4. Undo: add / set / remove are Clip verbs; an add at an occupied tick replaces; a point drag
//     inside a group coalesces to one step.

#include "engine/GraphBuilder.h"
#include "engine/InstrumentState.h"
#include "engine/Midi.h"
#include "engine/OfflineRenderer.h"
#include "engine/ParamSpec.h"
#include "engine/Project.h"
#include "engine/ProjectMixerProjection.h"
#include "engine/ProjectUndo.h"
#include "engine/nodes/SimpleSynthNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::Event;
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
using yesdaw::engine::SimpleSynthNode;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::TempoMapView;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::Transport;

namespace {

constexpr double kSampleRate = 30720.0;   // 120 BPM => one tick == one frame (the scheduler tests' law)

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

Note makeNote (std::uint8_t id, Tick start, Tick length, std::int16_t key) noexcept
{
    Note note;
    note.id = idFromLowByte (id);
    note.startTick = start;
    note.lengthTicks = length;
    note.key = key;
    note.pitchNote = static_cast<double> (key);
    note.normalizedVelocity = 1.0;
    note.portIndex = 0;
    note.channel = 1;
    return note;
}

MidiControlEvent makeControl (std::uint8_t id, Tick tick, MidiControlKind kind, std::int16_t number, double value,
                              std::int16_t channel = 1) noexcept
{
    MidiControlEvent control;
    control.id = idFromLowByte (id);
    control.tick = tick;
    control.kind = kind;
    control.number = number;
    control.value = value;
    control.portIndex = 0;
    control.channel = channel;
    return control;
}

Project makeMidiProject (std::vector<MidiClip> clips)
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { kSampleRate };
    Track track;
    track.id = idFromLowByte (31);
    track.strip.name = "MIDI Track";
    project.tracks = { track };
    project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    project.midiClips = std::move (clips);
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

Project makeOneNoteProject (Tick lengthTicks, Tick noteLength, std::int16_t key)
{
    MidiClip clip;
    clip.id = idFromLowByte (40);
    clip.trackId = idFromLowByte (31);
    clip.timelineStart = 0;
    clip.timelineLength = lengthTicks;
    clip.timeBase = TimeBase::TempoLocked;
    clip.notes = { makeNote (50, 0, noteLength, key) };
    return makeMidiProject ({ clip });
}

Project withControls (Project project, std::vector<MidiControlEvent> controls)
{
    project.midiClips[0].controlEvents = std::move (controls);
    REQUIRE (project.midiClips[0].isValid());
    return project;
}

Project withParam (Project project, std::uint32_t paramId, double normalized)
{
    REQUIRE (yesdaw::engine::setTrackInstrumentParam (project, project.tracks[0].id, paramId, normalized)
             == ProjectEditStatus::Applied);
    return project;
}

std::vector<float> renderProject (const Project& project)
{
    OfflineRenderOptions options;
    options.maxBlockSize = 64;
    auto built = yesdaw::engine::buildProjectGraph (project, std::span<const DecodedAssetAudio> {}, options);
    REQUIRE (built.ok());
    const std::uint16_t channels = built.channels;
    const std::uint64_t frames = built.frames;
    REQUIRE (frames > 0);
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
    // The strip is stereo and the synth widens symmetrically: hand back channel 0 so an index is a frame.
    std::vector<float> left (static_cast<std::size_t> (frames), 0.0f);
    for (std::size_t i = 0; i < left.size(); ++i)
        left[i] = out[i * channels];
    return left;
}

double rmsOf (const std::vector<float>& samples, std::size_t begin, std::size_t end)
{
    double sum = 0.0;
    std::size_t n = 0;
    for (std::size_t i = begin; i < end && i < samples.size(); ++i, ++n)
        sum += static_cast<double> (samples[i]) * static_cast<double> (samples[i]);
    return n > 0 ? std::sqrt (sum / static_cast<double> (n)) : 0.0;
}

bool silentOver (const std::vector<float>& samples, std::size_t begin, std::size_t end)
{
    for (std::size_t i = begin; i < end && i < samples.size(); ++i)
        if (samples[i] != 0.0f)
            return false;
    return true;
}

// Sign changes over a window: a sine at f Hz crosses zero 2 f times per second.
std::size_t zeroCrossings (const std::vector<float>& samples, std::size_t begin, std::size_t end)
{
    std::size_t count = 0;
    for (std::size_t i = begin + 1; i < end && i < samples.size(); ++i)
        if ((samples[i - 1] < 0.0f) != (samples[i] < 0.0f))
            ++count;
    return count;
}

bool sameOver (const std::vector<float>& a, const std::vector<float>& b, std::size_t begin, std::size_t end)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = begin; i < end && i < a.size(); ++i)
        if (a[i] != b[i])
            return false;
    return true;
}

} // namespace

TEST_CASE ("MIDI control events: the edit model validates kind, number, value and clip bounds",
           "[engine][midi][midi-control][g3]")
{
    REQUIRE (makeControl (1, 0, MidiControlKind::ControlChange, 64, 1.0).isValid());
    REQUIRE (makeControl (1, 0, MidiControlKind::PitchBend, 0, -1.0).isValid());
    REQUIRE (makeControl (1, 0, MidiControlKind::ChannelPressure, 0, 0.5).isValid());
    REQUIRE (makeControl (1, 0, MidiControlKind::PolyPressure, 127, 0.0).isValid());
    REQUIRE (makeControl (1, 0, MidiControlKind::ProgramChange, 127, 0.0).isValid());
    REQUIRE (makeControl (1, 0, MidiControlKind::ControlChange, 1, 0.5, -1).isValid());   // wildcard channel

    REQUIRE_FALSE (makeControl (0, 0, MidiControlKind::ControlChange, 1, 0.5).isValid());   // no id
    REQUIRE_FALSE (makeControl (1, -1, MidiControlKind::ControlChange, 1, 0.5).isValid());
    REQUIRE_FALSE (makeControl (1, 0, static_cast<MidiControlKind> (9), 1, 0.5).isValid());
    REQUIRE_FALSE (makeControl (1, 0, MidiControlKind::ControlChange, 128, 0.5).isValid());
    REQUIRE_FALSE (makeControl (1, 0, MidiControlKind::ControlChange, 1, 1.5).isValid());
    REQUIRE_FALSE (makeControl (1, 0, MidiControlKind::ControlChange, 1, -0.5).isValid());   // only a bend goes below 0
    REQUIRE_FALSE (makeControl (1, 0, MidiControlKind::PitchBend, 0, -1.5).isValid());
    REQUIRE_FALSE (makeControl (1, 0, MidiControlKind::PitchBend, 3, 0.0).isValid());   // a bend has no number
    REQUIRE_FALSE (makeControl (1, 0, MidiControlKind::ProgramChange, 3, 0.5).isValid());   // a program has no value
    REQUIRE_FALSE (makeControl (1, 0, MidiControlKind::ControlChange, 1, 0.5, 16).isValid());

    MidiClip clip;
    clip.id = idFromLowByte (40);
    clip.trackId = idFromLowByte (31);
    clip.timelineLength = 1000;
    clip.controlEvents = { makeControl (2, 1000, MidiControlKind::ControlChange, 1, 0.5) };
    REQUIRE (clip.isValid());   // the end tick is inside (a point may sit on the Clip's end)
    clip.controlEvents[0].tick = 1001;
    REQUIRE_FALSE (clip.isValid());
    clip.controlEvents[0].tick = 10;
    clip.controlEvents[0].value = 2.0;
    REQUIRE_FALSE (clip.isValid());
}

TEST_CASE ("flatten: control points ride the note timeline, precede notes at a frame, and quantize to the wire",
           "[engine][midi][midi-control][flatten][g3]")
{
    MidiClip clip;
    clip.id = idFromLowByte (40);
    clip.trackId = idFromLowByte (31);
    clip.timelineStart = 0;
    clip.timelineLength = 4096;
    clip.timeBase = TimeBase::TempoLocked;
    clip.notes = { makeNote (50, 100, 200, 60) };
    // Deliberately out of time order: the flatten sorts.
    clip.controlEvents = {
        makeControl (60, 300, MidiControlKind::PolyPressure, 61, 0.25),
        makeControl (61, 100, MidiControlKind::ControlChange, 64, 1.0),
        makeControl (62, 70, MidiControlKind::PitchBend, 0, 1.0),
        makeControl (63, 60, MidiControlKind::PitchBend, 0, 0.0),
        makeControl (64, 50, MidiControlKind::PitchBend, 0, -1.0),
        makeControl (65, 0, MidiControlKind::ProgramChange, 5, 0.0),
        makeControl (66, 200, MidiControlKind::ChannelPressure, 0, 0.5),
    };
    REQUIRE (clip.isValid());

    const std::vector<TempoChange> tempo { TempoChange { 0, 120.0, TempoCurve::Jump } };
    std::vector<ScheduledMidiEvent> timeline;
    REQUIRE (yesdaw::engine::flattenMidiClipForProjection (clip, TempoMapView { tempo.data(), tempo.size() },
                                                           SampleRate { kSampleRate }, timeline)
             == MidiFlattenStatus::Ok);
    REQUIRE (timeline.size() == 9u);

    const auto midi1 = [&] (std::size_t i, std::int64_t frame, std::uint8_t status, std::uint8_t d1, std::uint8_t d2)
    {
        INFO ("event " << i);
        REQUIRE (timeline[i].frame == frame);
        REQUIRE (timeline[i].event.type == EventType::Midi1);
        REQUIRE (timeline[i].event.payload.midi1.status == status);
        REQUIRE (timeline[i].event.payload.midi1.data1 == d1);
        REQUIRE (timeline[i].event.payload.midi1.data2 == d2);
        REQUIRE (timeline[i].event.voice.channel == 1);
        REQUIRE (timeline[i].event.voice.portIndex == 0);
    };
    midi1 (0, 0, 0xC1, 5, 0);       // program 5 on channel 1
    midi1 (1, 50, 0xE1, 0x00, 0x00);   // bend -1 -> 0
    midi1 (2, 60, 0xE1, 0x00, 0x40);   // bend 0 -> 8192
    midi1 (3, 70, 0xE1, 0x7F, 0x7F);   // bend +1 -> 16383
    midi1 (4, 100, 0xB1, 64, 127);     // CC64 1.0 -> 127, BEFORE the note-on at the same frame
    REQUIRE (timeline[5].frame == 100);
    REQUIRE (timeline[5].event.type == EventType::NoteOn);
    REQUIRE (timeline[5].event.voice.key == 60);
    midi1 (6, 200, 0xD1, 64, 0);       // channel pressure 0.5 -> 64
    midi1 (7, 300, 0xA1, 61, 32);      // poly pressure 0.25 on key 61 -> 32, BEFORE the note-off at 300
    REQUIRE (timeline[7].event.voice.key == 61);
    REQUIRE (timeline[8].frame == 300);
    REQUIRE (timeline[8].event.type == EventType::NoteOff);

    // The wildcard channel writes a 0 nibble on the wire and keeps -1 in the voice address.
    clip.controlEvents = { makeControl (70, 0, MidiControlKind::ControlChange, 1, 0.5, -1) };
    REQUIRE (yesdaw::engine::flattenMidiClipForProjection (clip, TempoMapView { tempo.data(), tempo.size() },
                                                           SampleRate { kSampleRate }, timeline)
             == MidiFlattenStatus::Ok);
    REQUIRE (timeline[0].event.payload.midi1.status == 0xB0);
    REQUIRE (timeline[0].event.payload.midi1.data2 == 64);   // round (63.5)
    REQUIRE (timeline[0].event.voice.channel == -1);

    // A SampleLocked Clip addresses frames directly; a point past the Clip's end is refused.
    clip.timeBase = TimeBase::SampleLocked;
    clip.timelineStart = 1000;
    REQUIRE (yesdaw::engine::flattenMidiClipForProjection (clip, TempoMapView { tempo.data(), tempo.size() },
                                                           SampleRate { kSampleRate }, timeline)
             == MidiFlattenStatus::Ok);
    REQUIRE (timeline[0].frame == 1000);
    clip.controlEvents[0].tick = clip.timelineLength + 1;
    REQUIRE (yesdaw::engine::flattenMidiClipForProjection (clip, TempoMapView { tempo.data(), tempo.size() },
                                                           SampleRate { kSampleRate }, timeline)
             == MidiFlattenStatus::InvalidInput);
}

TEST_CASE ("SimpleSynth honours CC64: a released note holds while the pedal is down and releases on the lift",
           "[engine][midi][midi-control][render][sustain][g3]")
{
    // The note ends at 2048; the 120 ms release (3686 frames) is long gone by 8000.
    const Project plain = makeOneNoteProject (16384, 2048, 60);
    const std::vector<float> reference = renderProject (plain);
    REQUIRE_FALSE (silentOver (reference, 0, 2048));
    REQUIRE (silentOver (reference, 8000, 10000));

    const Project pedalled = withControls (plain, {
        makeControl (60, 0, MidiControlKind::ControlChange, 64, 1.0),      // pedal down before the note
        makeControl (61, 10000, MidiControlKind::ControlChange, 64, 0.0),  // pedal up
    });
    const std::vector<float> held = renderProject (pedalled);
    REQUIRE (sameOver (held, reference, 0, 2048));       // the pedal changes nothing while the key is down
    REQUIRE_FALSE (silentOver (held, 8000, 10000));      // the released note is still sounding
    REQUIRE (rmsOf (held, 8000, 10000) == Catch::Approx (rmsOf (held, 512, 1536)).epsilon (0.05));   // at the held level
    REQUIRE (silentOver (held, 10000 + 3686 + 512, 16384));   // the lift releases it

    // A pedal that lifts BEFORE the note-off changes nothing at all.
    const Project earlyLift = withControls (plain, {
        makeControl (60, 0, MidiControlKind::ControlChange, 64, 1.0),
        makeControl (61, 1000, MidiControlKind::ControlChange, 64, 0.0),
    });
    REQUIRE (renderProject (earlyLift) == reference);

    // The pedal value's threshold is the MIDI law: 63 is up, 64 is down.
    const Project halfPedal = withControls (plain, { makeControl (60, 0, MidiControlKind::ControlChange, 64, 63.0 / 127.0) });
    REQUIRE (renderProject (halfPedal) == reference);
}

TEST_CASE ("pitch bend moves a pure sine by the bend range; the centre leaves the render bit-identical",
           "[engine][midi][midi-control][render][bend][g3]")
{
    // A4 (440 Hz) held for the whole Clip on the pure sine table; count crossings over one second
    // after the attack: 2 f per second.
    const Project sine = withParam (makeOneNoteProject (40960, 40960, 69), SimpleSynthNode::kOscMixParamId, 0.0);
    const std::vector<float> reference = renderProject (sine);
    const std::size_t window = static_cast<std::size_t> (kSampleRate);
    REQUIRE (zeroCrossings (reference, 8192, 8192 + window) == Catch::Approx (880.0).margin (4.0));

    const std::vector<float> up = renderProject (withControls (sine, { makeControl (60, 0, MidiControlKind::PitchBend, 0, 1.0) }));
    REQUIRE (zeroCrossings (up, 8192, 8192 + window)
             == Catch::Approx (2.0 * 440.0 * std::pow (2.0, SimpleSynthNode::kPitchBendSemitones / 12.0)).margin (4.0));

    const std::vector<float> down = renderProject (withControls (sine, { makeControl (60, 0, MidiControlKind::PitchBend, 0, -1.0) }));
    REQUIRE (zeroCrossings (down, 8192, 8192 + window)
             == Catch::Approx (2.0 * 440.0 * std::pow (2.0, -SimpleSynthNode::kPitchBendSemitones / 12.0)).margin (4.0));

    const std::vector<float> centred = renderProject (withControls (sine, { makeControl (60, 0, MidiControlKind::PitchBend, 0, 0.0) }));
    REQUIRE (centred == reference);

    // A bend mid-note: identical before it, different after it.
    const std::vector<float> late = renderProject (withControls (sine, { makeControl (60, 20480, MidiControlKind::PitchBend, 0, 1.0) }));
    REQUIRE (sameOver (late, reference, 0, 20480));
    REQUIRE_FALSE (sameOver (late, reference, 20480, 40960));
}

TEST_CASE ("CC1 opens the filter from the cutoff parameter and is silent at the bypass cutoff",
           "[engine][midi][midi-control][render][mod][g3]")
{
    const Project plain = makeOneNoteProject (4096, 2048, 60);
    const std::vector<float> reference = renderProject (plain);

    // At the default (bypass) cutoff there is nothing to open: bit-identical.
    REQUIRE (renderProject (withControls (plain, { makeControl (60, 0, MidiControlKind::ControlChange, 1, 1.0) })) == reference);

    // With the cutoff at 300 Hz the mod wheel lifts it (four octaves at 127): more energy.
    const yesdaw::engine::ParamSpec cutoff = SimpleSynthNode::parameterSpec (SimpleSynthNode::kCutoffParamId);
    const Project dark = withParam (plain, SimpleSynthNode::kCutoffParamId, yesdaw::engine::unmapToNormalized (cutoff, 300.0));
    const std::vector<float> darkRender = renderProject (dark);
    const std::vector<float> opened = renderProject (withControls (dark, { makeControl (60, 0, MidiControlKind::ControlChange, 1, 1.0) }));
    const std::vector<float> halfOpened = renderProject (withControls (dark, { makeControl (60, 0, MidiControlKind::ControlChange, 1, 0.5) }));
    const double darkRms = rmsOf (darkRender, 512, 1536);
    const double halfRms = rmsOf (halfOpened, 512, 1536);
    const double openRms = rmsOf (opened, 512, 1536);
    REQUIRE (darkRms > 0.0);
    REQUIRE (halfRms > darkRms * 1.1);
    REQUIRE (openRms > halfRms);
    REQUIRE (openRms <= rmsOf (reference, 512, 1536) * 1.05);   // opened, not louder than the unfiltered sound

    // A wheel at 0 is the parameter's cutoff exactly.
    REQUIRE (renderProject (withControls (dark, { makeControl (60, 0, MidiControlKind::ControlChange, 1, 0.0) })) == darkRender);
}

TEST_CASE ("aftertouch and program change reach the instrument's stream and leave SimpleSynth bit-identical",
           "[engine][midi][midi-control][render][g3]")
{
    const Project plain = makeOneNoteProject (4096, 2048, 60);
    const std::vector<float> reference = renderProject (plain);
    const Project decorated = withControls (plain, {
        makeControl (60, 0, MidiControlKind::ProgramChange, 12, 0.0),
        makeControl (61, 100, MidiControlKind::ChannelPressure, 0, 0.9),
        makeControl (62, 200, MidiControlKind::PolyPressure, 60, 0.9),
        makeControl (63, 300, MidiControlKind::ControlChange, 7, 0.1),   // CC7 is not one the synth reads either
    });
    REQUIRE (renderProject (decorated) == reference);
}

TEST_CASE ("control points are undoable Clip edits: an add replaces at an occupied tick, set moves, remove, a drag coalesces",
           "[engine][midi][midi-control][undo][g3]")
{
    Project project = makeOneNoteProject (4096, 2048, 60);
    const EntityId clipId = project.midiClips[0].id;
    ProjectUndoStack undo;

    REQUIRE (undo.apply (project, ProjectEditCommand::addMidiControlEvent (clipId, makeControl (60, 100, MidiControlKind::ControlChange, 1, 0.2))).applied());
    REQUIRE (project.midiClips[0].controlEvents.size() == 1u);

    // One value per tick per lane: the second add at tick 100 replaces the first (its id wins).
    REQUIRE (undo.apply (project, ProjectEditCommand::addMidiControlEvent (clipId, makeControl (61, 100, MidiControlKind::ControlChange, 1, 0.7))).applied());
    REQUIRE (project.midiClips[0].controlEvents.size() == 1u);
    REQUIRE (project.midiClips[0].controlEvents[0].id == idFromLowByte (61));
    REQUIRE (project.midiClips[0].controlEvents[0].value == 0.7);
    // A different lane at the same tick is its own point.
    REQUIRE (undo.apply (project, ProjectEditCommand::addMidiControlEvent (clipId, makeControl (62, 100, MidiControlKind::ControlChange, 64, 1.0))).applied());
    REQUIRE (project.midiClips[0].controlEvents.size() == 2u);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0].controlEvents.size() == 1u);
    REQUIRE (project.midiClips[0].controlEvents[0].id == idFromLowByte (60));
    REQUIRE (project.midiClips[0].controlEvents[0].value == 0.2);
    REQUIRE (undo.redo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0].controlEvents[0].id == idFromLowByte (61));

    // Refusals leave the Clip untouched: a duplicate id, a bad value, a tick past the end, an unknown id.
    const MidiClip before = project.midiClips[0];
    REQUIRE (undo.apply (project, ProjectEditCommand::addMidiControlEvent (clipId, makeControl (61, 500, MidiControlKind::ControlChange, 1, 0.5))).editStatus
             == ProjectEditStatus::DuplicateEntityId);
    REQUIRE (undo.apply (project, ProjectEditCommand::addMidiControlEvent (clipId, makeControl (70, 500, MidiControlKind::ControlChange, 1, 1.5))).editStatus
             == ProjectEditStatus::InvalidMidiControlEvent);
    REQUIRE (undo.apply (project, ProjectEditCommand::addMidiControlEvent (clipId, makeControl (70, 5000, MidiControlKind::ControlChange, 1, 0.5))).editStatus
             == ProjectEditStatus::InvalidMidiControlEvent);
    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiControlEvent (clipId, idFromLowByte (99), 10, 0.5)).editStatus
             == ProjectEditStatus::MidiControlEventNotFound);
    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiControlEvent (clipId, idFromLowByte (61), 10, -0.5)).editStatus
             == ProjectEditStatus::InvalidMidiControlEvent);
    REQUIRE (undo.apply (project, ProjectEditCommand::removeMidiControlEvent (clipId, idFromLowByte (99))).editStatus
             == ProjectEditStatus::MidiControlEventNotFound);
    REQUIRE (project.midiClips[0] == before);

    // A drag (a group of sets) is ONE undo step; the point lands at the last tick / value. One undo
    // returns to the pre-drag Clip; a second undo steps past the drag to the state before add 61.
    REQUIRE (undo.beginTransactionGroup());
    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiControlEvent (clipId, idFromLowByte (61), 120, 0.75)).applied());
    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiControlEvent (clipId, idFromLowByte (61), 140, 0.8)).applied());
    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiControlEvent (clipId, idFromLowByte (61), 160, 0.9)).applied());
    REQUIRE (undo.endTransactionGroup());
    REQUIRE (project.midiClips[0].controlEvents[0].tick == 160);
    REQUIRE (project.midiClips[0].controlEvents[0].value == 0.9);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0] == before);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0].controlEvents[0].id == idFromLowByte (60));   // the whole drag was one step
    REQUIRE (undo.redo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0] == before);
    REQUIRE (undo.redo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0].controlEvents[0].tick == 160);

    // A set that lands on another point of the same lane replaces it.
    REQUIRE (undo.apply (project, ProjectEditCommand::addMidiControlEvent (clipId, makeControl (63, 300, MidiControlKind::ControlChange, 1, 0.1))).applied());
    REQUIRE (project.midiClips[0].controlEvents.size() == 2u);
    REQUIRE (undo.apply (project, ProjectEditCommand::setMidiControlEvent (clipId, idFromLowByte (61), 300, 0.95)).applied());
    REQUIRE (project.midiClips[0].controlEvents.size() == 1u);
    REQUIRE (project.midiClips[0].controlEvents[0].id == idFromLowByte (61));
    REQUIRE (project.midiClips[0].controlEvents[0].tick == 300);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0].controlEvents.size() == 2u);

    // Remove, undo, redo.
    REQUIRE (undo.apply (project, ProjectEditCommand::removeMidiControlEvent (clipId, idFromLowByte (63))).applied());
    REQUIRE (project.midiClips[0].controlEvents.size() == 1u);
    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0].controlEvents.size() == 2u);
    REQUIRE (undo.redo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.midiClips[0].controlEvents.size() == 1u);

    // The label the history window shows.
    REQUIRE (std::string_view (yesdaw::engine::projectEditVerbLabel (yesdaw::engine::ProjectEditVerb::AddMidiControlEvent)) == "Add Controller Point");
}
