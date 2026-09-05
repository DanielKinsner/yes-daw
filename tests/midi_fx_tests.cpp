// G3.8 — MIDI FX on the Track's MIDI path: the transpose / scale inserts (the H4 nodes reached through
// the insert chain), the Chord Trigger and the Arpeggiator, their ParamSpecs, the projection's chain
// law, the Track-only rule, and the render goldens.
//
// Gates:
//  1. Params: every MIDI kind answers fxParamSpecForKind with named, usable specs; choice params carry
//     their names; a normalized value maps to the node's real setting.
//  2. Chord node (event level): a NoteOn / NoteOff pair grows into the triad at the same tick, copies
//     with derived ids and scaled velocity; an interval of 0 is off; a copy past 127 drops; the block
//     stays sorted and half-open.
//  3. Arpeggiator (event level): two held keys step on the 1/16 grid in Up order with the gate's
//     NoteOff; Down reverses; Up-Down bounces; two octaves double the pass; the input NoteOn / NoteOff
//     are consumed; releasing a key stops it; a locate releases the sounding step first; the grid is
//     absolute (a block boundary changes nothing).
//  4. Projection: a MIDI insert renders through the instrument — a transposed insert renders
//     bit-identically to the same note written a fifth up (golden by equivalence); a disabled insert
//     renders as no insert; the chord insert renders the same as the three notes written out; the
//     arpeggiator's render has energy only in the gate windows of its steps; audio inserts still sit
//     on the audio chain (a MIDI kind is not an insert node).
//  5. Track-only: addFxInsert of a MIDI kind on a Bus refuses MidiFxNeedsTrack; the undo verb refuses
//     the same way; a MIDI kind persists through the bundle (kind 5..8 in the same column).

#include "engine/GraphBuilder.h"
#include "engine/Midi.h"
#include "engine/OfflineRenderer.h"
#include "engine/Project.h"
#include "engine/ProjectMixerProjection.h"
#include "engine/ProjectUndo.h"
#include "engine/nodes/MidiEffectNode.h"
#include "persistence/ProjectBundle.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::EventType;
using yesdaw::engine::FxInsert;
using yesdaw::engine::FxKind;
using yesdaw::engine::MidiArpeggiatorNode;
using yesdaw::engine::MidiChordNode;
using yesdaw::engine::MidiClip;
using yesdaw::engine::MidiScaleMapNode;
using yesdaw::engine::MidiTransposeNode;
using yesdaw::engine::Note;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::ParamSpec;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectEditCommand;
using yesdaw::engine::ProjectEditStatus;
using yesdaw::engine::ProjectUndoStack;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::Transport;

namespace {

constexpr double kSampleRate = 30720.0;   // 120 BPM => one tick == one frame

EntityId idFromLowByte (std::uint8_t low)
{
    EntityId id;
    id.bytes.fill (0);
    id.bytes.back() = low;
    return id;
}

Note makeNote (std::uint8_t idLow, Tick start, Tick length, std::int16_t key, double velocity = 1.0)
{
    Note note;
    note.id = idFromLowByte (idLow);
    note.startTick = start;
    note.lengthTicks = length;
    note.key = key;
    note.pitchNote = static_cast<double> (key);
    note.normalizedVelocity = velocity;
    note.channel = 0;
    return note;
}

Event noteEvent (EventType type, std::uint32_t timeInBlock, std::int16_t key, double velocity = 1.0, std::int32_t noteId = 7)
{
    Event event;
    event.timeInBlock = timeInBlock;
    event.type = type;
    event.voice.noteId = noteId;
    event.voice.channel = 0;
    event.voice.portIndex = 0;
    event.voice.key = key;
    event.payload.note.normalizedVelocity = velocity;
    event.payload.note.pitchNote = static_cast<double> (key);
    return event;
}

// Run one block through a MIDI FX node with the given input events, at a timeline frame.
struct BlockRun
{
    std::vector<Event> storage;
    std::vector<Event> out;
};

template <typename NodeT>
std::vector<Event> runBlock (NodeT& node, std::vector<Event> input, int numFrames, std::int64_t timelineFrame, bool silenced = false)
{
    std::vector<Event> storage (1024);
    EventStream stream { std::span<Event> (storage), 0 };
    REQUIRE (stream.replaceEvents (std::span<const Event> (input)));
    std::vector<float> audio (static_cast<std::size_t> (numFrames), 1.0f);
    float* channels[1] = { audio.data() };
    Transport transport;
    transport.projectSampleRate = SampleRate { kSampleRate };
    transport.isPlaying = true;
    transport.hasTimelineFrame = true;
    transport.timelineFrame = timelineFrame;
    transport.clipsSilenced = silenced;
    const ProcessArgs args { yesdaw::engine::AudioBlock { channels, 1 }, stream, transport, numFrames, nullptr, nullptr };
    node.process (args);
    REQUIRE (stream.isValidForBlock (static_cast<std::uint32_t> (numFrames)));
    return std::vector<Event> (stream.events().begin(), stream.events().end());
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

Project makeNotesProject (std::vector<Note> notes, Tick lengthTicks = 30720)
{
    MidiClip clip;
    clip.id = idFromLowByte (40);
    clip.trackId = idFromLowByte (31);
    clip.timelineStart = 0;
    clip.timelineLength = lengthTicks;
    clip.timeBase = TimeBase::TempoLocked;
    clip.notes = std::move (notes);
    return makeMidiProject ({ clip });
}

Project withInsert (Project project, FxKind kind, std::vector<std::pair<std::uint32_t, double>> params, bool enabled = true, std::uint8_t idLow = 80)
{
    FxInsert insert;
    insert.id = idFromLowByte (idLow);
    insert.kind = kind;
    insert.enabled = enabled;
    insert.normalizedParams = std::move (params);
    REQUIRE (yesdaw::engine::addFxInsert (project, project.tracks[0].id, insert, project.tracks[0].strip.fxChain.size()) == ProjectEditStatus::Applied);
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
    std::vector<float> left (static_cast<std::size_t> (frames), 0.0f);
    for (std::size_t i = 0; i < left.size(); ++i)
        left[i] = out[i * channels];
    return left;
}

double rmsOf (const std::vector<float>& samples, std::size_t begin, std::size_t end)
{
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t i = begin; i < end && i < samples.size(); ++i, ++count)
        sum += static_cast<double> (samples[i]) * static_cast<double> (samples[i]);
    return count > 0 ? std::sqrt (sum / static_cast<double> (count)) : 0.0;
}

double normalizedFor (FxKind kind, std::uint32_t paramId, double real)
{
    const ParamSpec spec = yesdaw::engine::fxParamSpecForKind (kind, paramId);
    REQUIRE (spec.id == paramId);
    return (real - spec.min) / (spec.max - spec.min);
}

std::filesystem::path makeTempBundlePath (const std::string& label)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("yesdaw-midi-fx-" + label + "-" + std::to_string (stamp) + ".yesdaw");
}

} // namespace

TEST_CASE ("MIDI FX params: every kind answers with named specs, choices carry names, normalized maps to real", "[midi-fx]")
{
    using yesdaw::engine::fxKindAcceptsParameterId;
    using yesdaw::engine::fxKindIsMidi;
    using yesdaw::engine::fxParamSpecForKind;
    REQUIRE (fxKindIsMidi (FxKind::MidiTranspose));
    REQUIRE (fxKindIsMidi (FxKind::MidiChord));
    REQUIRE_FALSE (fxKindIsMidi (FxKind::Eq));
    REQUIRE (yesdaw::engine::fxKindIsKnown (FxKind::MidiArpeggiator));

    REQUIRE (fxKindAcceptsParameterId (FxKind::MidiTranspose, MidiTransposeNode::kSemitonesParamId));
    REQUIRE (std::string (fxParamSpecForKind (FxKind::MidiTranspose, 1).name) == "transpose.semitones");
    REQUIRE_FALSE (fxKindAcceptsParameterId (FxKind::MidiTranspose, 2));
    REQUIRE (fxParamSpecForKind (FxKind::MidiScaleMap, MidiScaleMapNode::kRootParamId).choiceCount == 12);
    REQUIRE (std::string (fxParamSpecForKind (FxKind::MidiScaleMap, MidiScaleMapNode::kScaleParamId).choiceNames[1]) == "Major");
    REQUIRE (fxParamSpecForKind (FxKind::MidiArpeggiator, MidiArpeggiatorNode::kRateParamId).choiceCount == 4);
    REQUIRE (std::string (fxParamSpecForKind (FxKind::MidiArpeggiator, MidiArpeggiatorNode::kOrderParamId).choiceNames[2]) == "Up-Down");
    REQUIRE (fxKindAcceptsParameterId (FxKind::MidiArpeggiator, MidiArpeggiatorNode::kGateParamId));
    REQUIRE (fxKindAcceptsParameterId (FxKind::MidiChord, MidiChordNode::kInterval3ParamId));
    REQUIRE (fxParamSpecForKind (FxKind::MidiChord, MidiChordNode::kInterval1ParamId).def == 4.0);

    MidiTransposeNode transpose (1);
    transpose.setNormalizedParameter (MidiTransposeNode::kSemitonesParamId, normalizedFor (FxKind::MidiTranspose, 1, 7.0));
    REQUIRE (transpose.semitones() == 7);
    MidiScaleMapNode scale (2);
    scale.setNormalizedParameter (MidiScaleMapNode::kRootParamId, normalizedFor (FxKind::MidiScaleMap, 1, 9.0));   // A
    scale.setNormalizedParameter (MidiScaleMapNode::kScaleParamId, normalizedFor (FxKind::MidiScaleMap, 2, 2.0));  // Minor
    REQUIRE (scale.rootKey() == 9);
    REQUIRE (scale.scaleMask() == MidiScaleMapNode::kNaturalMinorMask);
    MidiArpeggiatorNode arp (3);
    arp.setFramesPerQuarter (15360.0);
    arp.setNormalizedParameter (MidiArpeggiatorNode::kRateParamId, normalizedFor (FxKind::MidiArpeggiator, 1, 1.0));   // 1/8
    REQUIRE (arp.stepFrames() == 7680);
    arp.setNormalizedParameter (MidiArpeggiatorNode::kOctavesParamId, normalizedFor (FxKind::MidiArpeggiator, 3, 3.0));
    REQUIRE (arp.octaves() == 3);
    arp.setNormalizedParameter (MidiArpeggiatorNode::kOrderParamId, normalizedFor (FxKind::MidiArpeggiator, 2, 1.0));
    REQUIRE (arp.order() == MidiArpeggiatorNode::Order::Down);
    MidiChordNode chord (4);
    chord.setNormalizedParameter (MidiChordNode::kInterval3ParamId, normalizedFor (FxKind::MidiChord, 3, 12.0));
    REQUIRE (chord.interval3() == 12);
}

TEST_CASE ("Chord node: a note grows into its triad at the same tick, copies derive ids, off intervals and overflow drop", "[midi-fx]")
{
    MidiChordNode chord (10);
    chord.prepare (kSampleRate, 64);
    chord.setIntervals (4, 7, 0);
    chord.setVelocityPercent (50.0);

    std::vector<Event> out = runBlock (chord, { noteEvent (EventType::NoteOn, 3, 60, 0.8), noteEvent (EventType::NoteOff, 40, 60, 0.0) }, 64, 0);
    REQUIRE (out.size() == 6u);
    REQUIRE (out[0].voice.key == 60);
    REQUIRE (out[1].voice.key == 64);
    REQUIRE (out[2].voice.key == 67);
    REQUIRE (out[1].timeInBlock == 3);
    REQUIRE (out[1].payload.note.normalizedVelocity == Catch::Approx (0.4));
    REQUIRE (out[1].payload.note.pitchNote == Catch::Approx (64.0));
    REQUIRE (out[1].voice.noteId != out[0].voice.noteId);
    REQUIRE (out[2].voice.noteId != out[1].voice.noteId);
    REQUIRE (out[3].type == EventType::NoteOff);
    REQUIRE (out[4].voice.key == 64);
    REQUIRE (out[4].voice.noteId == out[1].voice.noteId);   // the off finds its on
    REQUIRE (out[5].voice.key == 67);

    // The third interval on; a copy past 127 drops; a non-note event passes through untouched.
    chord.setIntervals (4, 7, 12);
    Event cc = noteEvent (EventType::Midi1, 5, -1);
    cc.type = EventType::Midi1;
    out = runBlock (chord, { noteEvent (EventType::NoteOn, 1, 120), cc }, 64, 64);
    REQUIRE (out.size() == 4u);   // 120, 124, 127, (132 drops), the Midi1
    REQUIRE (out[2].voice.key == 127);
    REQUIRE (out[3].type == EventType::Midi1);
}

TEST_CASE ("Arpeggiator: held keys step on the grid in order with the gate, the input notes are consumed, the grid is absolute", "[midi-fx]")
{
    MidiArpeggiatorNode arp (11);
    arp.prepare (kSampleRate, 256);
    arp.setFramesPerQuarter (15360.0);
    arp.setRateChoice (2);   // 1/16 = 3840 frames
    arp.setGatePercent (50.0);
    REQUIRE (arp.stepFrames() == 3840);

    // Two keys pressed at frame 0: C4 then E4 (E4 played second).
    const std::vector<Event> press = { noteEvent (EventType::NoteOn, 0, 60, 0.9, 1), noteEvent (EventType::NoteOn, 0, 64, 0.7, 2) };
    std::vector<Event> out = runBlock (arp, press, 256, 0);
    // The input NoteOns are consumed; step 0 fires at frame 0 with the lowest key (Up).
    REQUIRE (out.size() == 1u);
    REQUIRE (out[0].type == EventType::NoteOn);
    REQUIRE (out[0].voice.key == 60);
    REQUIRE (out[0].timeInBlock == 0);
    REQUIRE (out[0].payload.note.normalizedVelocity == Catch::Approx (0.9));
    REQUIRE (arp.heldCount() == 2u);

    // Walk to the gate's end (frame 1920) and the next step (3840): the off, then E4.
    std::vector<Event> collected;
    for (std::int64_t frame = 256; frame < 3840 + 256; frame += 256)
        for (const Event& e : runBlock (arp, {}, 256, frame))
            collected.push_back (e);
    REQUIRE (collected.size() == 2u);
    REQUIRE (collected[0].type == EventType::NoteOff);
    REQUIRE (collected[0].voice.key == 60);
    REQUIRE (collected[0].voice.noteId == out[0].voice.noteId);
    REQUIRE (collected[1].type == EventType::NoteOn);
    REQUIRE (collected[1].voice.key == 64);
    REQUIRE (collected[1].timeInBlock == 3840 % 256);

    SECTION ("Down and Up-Down orders, two octaves")
    {
        arp.setOrder (MidiArpeggiatorNode::Order::Down);
        // A fresh grid from frame 7680 (step 2): Down over {60, 64} → step 2 % 2 = 0 → the top key.
        std::vector<Event> down = runBlock (arp, {}, 256, 7680);
        std::vector<Event> ons;
        for (const Event& e : down) if (e.type == EventType::NoteOn) ons.push_back (e);
        REQUIRE (ons.size() == 1u);
        REQUIRE (ons[0].voice.key == 64);

        arp.setOrder (MidiArpeggiatorNode::Order::UpDown);
        arp.setOctaves (2);
        // The pass is {60, 64, 72, 76}; Up-Down's pattern is 60 64 72 76 72 64 (length 6).
        const std::int16_t expected[6] = { 60, 64, 72, 76, 72, 64 };
        for (int step = 0; step < 6; ++step)
        {
            const std::int64_t frame = 3840 * (12 + step);   // step 12 % 6 == 0
            std::vector<Event> block = runBlock (arp, {}, 64, frame);
            std::vector<Event> stepOns;
            for (const Event& e : block) if (e.type == EventType::NoteOn) stepOns.push_back (e);
            REQUIRE (stepOns.size() == 1u);
            REQUIRE (stepOns[0].voice.key == expected[step]);
        }
    }

    SECTION ("releasing a key stops it; a locate releases the sounding step; a silenced transport clears")
    {
        // Release E4 just before step 3 (frame 11520): step 3 plays C4 (the only held key).
        std::vector<Event> block = runBlock (arp, { noteEvent (EventType::NoteOff, 10, 64, 0.0, 2) }, 256, 11520 - 256);
        for (const Event& e : block)
            REQUIRE (e.type == EventType::NoteOff);   // only the gate's off, the input off is consumed
        REQUIRE (arp.heldCount() == 1u);
        block = runBlock (arp, {}, 256, 11520);
        REQUIRE (block.size() == 1u);
        REQUIRE (block[0].type == EventType::NoteOn);
        REQUIRE (block[0].voice.key == 60);
        // A locate away while the step sounds: the off comes at the new block's top, before anything.
        block = runBlock (arp, {}, 256, 100000);   // 100000 % 3840 != 0: no step in this block
        REQUIRE (block.size() == 1u);
        REQUIRE (block[0].type == EventType::NoteOff);
        REQUIRE (block[0].timeInBlock == 0);
        // Silenced: the held set clears, nothing more sounds.
        block = runBlock (arp, {}, 256, 100256, true);
        REQUIRE (block.empty());
        REQUIRE (arp.heldCount() == 0u);
        block = runBlock (arp, {}, 256, 3840 * 40);
        REQUIRE (block.empty());
    }
}

TEST_CASE ("Projection: MIDI inserts render through the instrument — transpose by equivalence, bypass, chord, arpeggiator gate windows, audio chain untouched", "[midi-fx][golden]")
{
    const Project plain = makeNotesProject ({ makeNote (50, 0, 15360, 60) });
    const std::vector<float> reference = renderProject (plain);
    REQUIRE (rmsOf (reference, 0, 15360) > 0.01);

    SECTION ("transpose: the insert renders bit-identically to the note written a fifth up")
    {
        const Project transposed = withInsert (plain, FxKind::MidiTranspose, { { MidiTransposeNode::kSemitonesParamId, normalizedFor (FxKind::MidiTranspose, 1, 7.0) } });
        const Project written = makeNotesProject ({ makeNote (50, 0, 15360, 67) });
        REQUIRE (renderProject (transposed) == renderProject (written));
        REQUIRE (renderProject (transposed) != reference);
    }

    SECTION ("a disabled MIDI insert is not in the path; an insert with default params is the identity")
    {
        const Project bypassed = withInsert (plain, FxKind::MidiTranspose, { { MidiTransposeNode::kSemitonesParamId, 1.0 } }, false);
        REQUIRE (renderProject (bypassed) == reference);
        const Project zero = withInsert (plain, FxKind::MidiTranspose, {});
        REQUIRE (renderProject (zero) == reference);
    }

    SECTION ("scale: a C# under C Major renders as the D written out")
    {
        const Project offScale = makeNotesProject ({ makeNote (50, 0, 15360, 61) });
        const Project mapped = withInsert (offScale, FxKind::MidiScaleMap, { { MidiScaleMapNode::kScaleParamId, normalizedFor (FxKind::MidiScaleMap, 2, 1.0) } });
        const Project written = makeNotesProject ({ makeNote (50, 0, 15360, 62) });
        REQUIRE (renderProject (mapped) == renderProject (written));
    }

    SECTION ("chord: the insert renders as the three notes written out (same ids' order-free sum)")
    {
        const Project chorded = withInsert (plain, FxKind::MidiChord, { { MidiChordNode::kInterval3ParamId, normalizedFor (FxKind::MidiChord, 3, 12.0) } });
        const std::vector<float> chordRender = renderProject (chorded);
        REQUIRE (rmsOf (chordRender, 0, 15360) > rmsOf (reference, 0, 15360) * 1.2);
        const Project written = makeNotesProject ({ makeNote (50, 0, 15360, 60), makeNote (51, 0, 15360, 64), makeNote (52, 0, 15360, 67), makeNote (53, 0, 15360, 72) });
        const std::vector<float> writtenRender = renderProject (written);
        REQUIRE (chordRender.size() == writtenRender.size());
        for (std::size_t i = 0; i < chordRender.size(); i += 97)
            REQUIRE (chordRender[i] == Catch::Approx (writtenRender[i]).margin (1.0e-4));
    }

    SECTION ("arpeggiator: energy in the gate windows of its steps, silence between (release aside)")
    {
        // A whole-note C4 + E4 held; 1/16 steps of 3840 frames at 120 BPM / 30720 Hz; gate 50 %.
        const Project held = makeNotesProject ({ makeNote (50, 0, 30720, 60), makeNote (51, 0, 30720, 64) });
        const Project arped = withInsert (held, FxKind::MidiArpeggiator, {
            { MidiArpeggiatorNode::kRateParamId, normalizedFor (FxKind::MidiArpeggiator, 1, 2.0) },
            { MidiArpeggiatorNode::kGateParamId, normalizedFor (FxKind::MidiArpeggiator, 4, 50.0) } });
        const std::vector<float> render = renderProject (arped);
        REQUIRE (render != renderProject (held));
        // The golden by equivalence: the same steps written out as notes — C4 / E4 alternating on the
        // 1/16 grid (3840 frames), each 1920 frames long (the 50 % gate) — render the same.
        std::vector<Note> steps;
        for (int step = 0; step < 8; ++step)
            steps.push_back (makeNote (static_cast<std::uint8_t> (60 + step), static_cast<Tick> (3840 * step), 1920, step % 2 == 0 ? 60 : 64));
        const std::vector<float> written = renderProject (makeNotesProject (steps));
        REQUIRE (render.size() == written.size());
        REQUIRE (rmsOf (render, 4000, 5500) > 0.01);
        for (std::size_t i = 0; i < render.size(); i += 61)
            REQUIRE (render[i] == Catch::Approx (written[i]).margin (1.0e-4));
    }

    SECTION ("the audio chain: a MIDI kind is no insert node; an EQ beside it still is")
    {
        Project both = withInsert (plain, FxKind::MidiChord, {}, true, 80);
        both = withInsert (both, FxKind::Eq, {}, true, 81);
        yesdaw::engine::ProjectMixerProjectionConfig config;
        config.id = 70;
        yesdaw::engine::MixerProjectionInputs projection;
        yesdaw::engine::ProjectMixerProjectionError error;
        REQUIRE (yesdaw::engine::projectToMixerProjectionInputs (
            both, config,
            [] (const Project&, const yesdaw::engine::Clip&, const yesdaw::engine::Asset&, int, yesdaw::engine::ScheduledClipSource&) { return false; },
            projection, &error));
        REQUIRE (projection.tracks.size() == 1u);
        REQUIRE (projection.tracks[0].insertNodes.size() == 1u);   // the EQ only
        bool chordOnPath = false;
        for (const auto& node : projection.tracks[0].supportNodes)
            if (dynamic_cast<MidiChordNode*> (node.get()) != nullptr)
                chordOnPath = true;
        REQUIRE (chordOnPath);
    }
}

TEST_CASE ("MIDI FX are Track-only and persist: a Bus refuses MidiFxNeedsTrack, the verb agrees, the bundle round-trips the kind", "[midi-fx]")
{
    Project project = makeNotesProject ({ makeNote (50, 0, 15360, 60) });
    yesdaw::engine::Bus bus;
    bus.id = idFromLowByte (90);
    bus.strip.name = "Bus";
    project.buses.push_back (bus);
    FxInsert insert;
    insert.id = idFromLowByte (81);
    insert.kind = FxKind::MidiArpeggiator;
    REQUIRE (yesdaw::engine::addFxInsert (project, bus.id, insert, 0) == ProjectEditStatus::MidiFxNeedsTrack);
    REQUIRE (project.buses[0].strip.fxChain.empty());
    insert.kind = FxKind::Eq;
    REQUIRE (yesdaw::engine::addFxInsert (project, bus.id, insert, 0) == ProjectEditStatus::Applied);

    ProjectUndoStack undo;
    REQUIRE_FALSE (undo.apply (project, ProjectEditCommand::addFxInsert (bus.id, idFromLowByte (82), FxKind::MidiChord, true, 1)).applied());
    REQUIRE (undo.apply (project, ProjectEditCommand::addFxInsert (project.tracks[0].id, idFromLowByte (82), FxKind::MidiChord, true, 0)).applied());
    REQUIRE (project.tracks[0].strip.fxChain.size() == 1u);
    REQUIRE (project.tracks[0].strip.fxChain[0].kind == FxKind::MidiChord);
    REQUIRE (undo.apply (project, ProjectEditCommand::setFxInsertParam (project.tracks[0].id, idFromLowByte (82), MidiChordNode::kInterval3ParamId, 0.5)).applied());

    const std::filesystem::path path = makeTempBundlePath ("kind");
    {
        yesdaw::persistence::ProjectBundleDb db;
        REQUIRE (yesdaw::persistence::ProjectBundleDb::openOrCreateBundle (path, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        Project back;
        const yesdaw::persistence::BundleResult read = db.readProjectSnapshot (back);
        INFO (read.message);
        REQUIRE (read.ok());
        REQUIRE (back.tracks[0].strip.fxChain.size() == 1u);
        REQUIRE (back.tracks[0].strip.fxChain[0].kind == FxKind::MidiChord);
        REQUIRE (back.tracks[0].strip.fxChain[0].normalizedParams.size() == 1u);
        REQUIRE (back.tracks[0].strip.fxChain[0].normalizedParams[0].second == Catch::Approx (0.5));
    }
    std::error_code ec;
    std::filesystem::remove_all (path, ec);
}
