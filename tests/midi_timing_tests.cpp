// YES DAW - H4 MIDI timing gate.
//
// Proves MIDI Clip Notes flatten into sample-accurate Events across Block boundaries and a tempo change,
// then drive a non-zero-latency Instrument Node through GraphBuilder PDC.

#include "engine/GraphBuilder.h"
#include "engine/Midi.h"
#include "engine/nodes/ImpulseInstrumentNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/MidiEffectNode.h"
#include "engine/plugin/PluginNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

using Catch::Approx;
using yesdaw::engine::AudioBlock;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNodeKind;
using yesdaw::engine::EntityId;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::EventType;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::ImpulseInstrumentNode;
using yesdaw::engine::MasterNode;
using yesdaw::engine::MidiClip;
using yesdaw::engine::MidiFlattenBlock;
using yesdaw::engine::MidiFlattenStatus;
using yesdaw::engine::MidiScaleMapNode;
using yesdaw::engine::MidiTransposeNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::Note;
using yesdaw::engine::PluginNode;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::TempoMapView;
using yesdaw::engine::Tick;
using yesdaw::engine::Transport;
using yesdaw::engine::MpeVoiceAllocationConfig;
using yesdaw::engine::MpeVoiceAllocationStatus;
using yesdaw::engine::allocateMpeVoiceAddresses;
using yesdaw::engine::flattenMidiClipNotesForBlock;
using yesdaw::engine::kTicksPerQuarter;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::tickToFrame;
using yesdaw::engine::voiceNoteIdFromEntityId;

namespace {

constexpr double kSampleRate = 30720.0; // 120 BPM => exactly 1 frame per tick at PPQ 15360.
constexpr NodeId kMasterId = 91000;

EntityId id (std::uint8_t value) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = value;
    return EntityId::fromBytes (bytes);
}

TempoMapView tempoView (const std::array<TempoChange, 1>& tempo) noexcept
{
    return TempoMapView { tempo.data(), tempo.size() };
}

TempoMapView tempoView (const std::array<TempoChange, 2>& tempo) noexcept
{
    return TempoMapView { tempo.data(), tempo.size() };
}

MidiClip clipWithNotes (std::vector<Note> notes, Tick start = 100, Tick length = 64)
{
    MidiClip clip;
    clip.id = id (1);
    clip.trackId = id (90);
    clip.timelineStart = start;
    clip.timelineLength = length;
    clip.notes = std::move (notes);
    return clip;
}

Note note (std::uint8_t noteId, Tick start, Tick length = 4, std::int16_t key = 60)
{
    Note n;
    n.id = id (noteId);
    n.startTick = start;
    n.lengthTicks = length;
    n.key = key;
    n.pitchNote = static_cast<double> (key);
    n.normalizedVelocity = 1.0;
    return n;
}

Event noteEvent (std::uint32_t timeInBlock, EventType type, std::int16_t key, double pitch)
{
    Event event {};
    event.timeInBlock = timeInBlock;
    event.type = type;
    event.voice.noteId = static_cast<std::int32_t> (key);
    event.voice.key = key;
    event.payload.note.normalizedVelocity = type == EventType::NoteOff ? 0.0 : 1.0;
    event.payload.note.pitchNote = pitch;
    return event;
}

std::vector<std::uint32_t> noteOnOffsets (std::span<const Event> events)
{
    std::vector<std::uint32_t> offsets;
    for (const Event& event : events)
        if (event.type == EventType::NoteOn)
            offsets.push_back (event.timeInBlock);
    return offsets;
}

const Event* findNoteOnFor (std::span<const Event> events, EntityId noteId)
{
    const std::int32_t voiceNoteId = voiceNoteIdFromEntityId (noteId);
    for (const Event& event : events)
        if (event.type == EventType::NoteOn && event.voice.noteId == voiceNoteId)
            return &event;

    return nullptr;
}

std::vector<float> render (CompiledGraph& graph, std::span<const Event> events, int frames)
{
    std::vector<float> out (static_cast<std::size_t> (frames), -999.0f);
    graph.process (out.data(), frames, events);
    return out;
}

std::vector<float> render (CompiledGraph& graph, std::span<Event> events, int frames)
{
    std::vector<float> out (static_cast<std::size_t> (frames), -999.0f);
    graph.process (out.data(), frames, events);
    return out;
}

void processMidiEffectNode (Node& node, EventStream& stream)
{
    std::array<float, 8> audio {};
    audio.fill (1.0f);
    float* channels[1] = { audio.data() };
    Transport transport;

    node.process (ProcessArgs { AudioBlock { channels, 1 }, stream, transport, static_cast<int> (audio.size()) });

    for (const float sample : audio)
        REQUIRE (sample == Approx (0.0f));
}

} // namespace

TEST_CASE ("Tempo map converts ticks to frames across a Jump tempo change", "[midi][tempo]")
{
    const std::array<TempoChange, 2> tempo {
        TempoChange { 0, 120.0, TempoCurve::Jump },
        TempoChange { 100, 60.0, TempoCurve::Jump },
    };

    double before = -1.0;
    double atChange = -1.0;
    double after = -1.0;
    REQUIRE (tickToFrame (tempoView (tempo), SampleRate { kSampleRate }, 96, before));
    REQUIRE (tickToFrame (tempoView (tempo), SampleRate { kSampleRate }, 100, atChange));
    REQUIRE (tickToFrame (tempoView (tempo), SampleRate { kSampleRate }, 104, after));

    REQUIRE (before == Approx (96.0));
    REQUIRE (atChange == Approx (100.0));
    REQUIRE (after == Approx (108.0)); // 4 ticks at 60 BPM are 8 frames, not 4.
}

TEST_CASE ("MIDI Clip flattening uses half-open Block boundaries", "[midi][flatten][boundary]")
{
    const std::array<TempoChange, 1> tempo { TempoChange { 0, 120.0, TempoCurve::Jump } };
    const MidiClip clip = clipWithNotes ({
        note (2, 0, 2),
        note (3, 7, 4),
        note (4, 8, 4),
    });

    std::array<Event, 8> out {};
    const auto first = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 100.0, 8, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (out));

    REQUIRE (first.status == MidiFlattenStatus::Ok);
    EventStream firstStream { std::span<const Event> (out.data(), first.eventsWritten) };
    REQUIRE (firstStream.isValidForBlock (8));
    REQUIRE (noteOnOffsets (firstStream.events()) == std::vector<std::uint32_t> { 0u, 7u });

    std::array<Event, 4> nextOut {};
    const auto second = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 108.0, 8, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (nextOut));

    REQUIRE (second.status == MidiFlattenStatus::Ok);
    EventStream secondStream { std::span<const Event> (nextOut.data(), second.eventsWritten) };
    REQUIRE (secondStream.isValidForBlock (8));
    REQUIRE (noteOnOffsets (secondStream.events()) == std::vector<std::uint32_t> { 0u });
}

TEST_CASE ("MIDI Clip flattening uses the full tempo map after a tempo change", "[midi][flatten][tempo]")
{
    const std::array<TempoChange, 2> tempo {
        TempoChange { 0, 120.0, TempoCurve::Jump },
        TempoChange { 100, 60.0, TempoCurve::Jump },
    };
    const MidiClip clip = clipWithNotes ({ note (2, 4, 4) }, /*start*/ 100, /*length*/ 16);

    std::array<Event, 4> out {};
    const auto result = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 100.0, 16, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (out));

    REQUIRE (result.status == MidiFlattenStatus::Ok);
    EventStream stream { std::span<const Event> (out.data(), result.eventsWritten) };
    REQUIRE (stream.isValidForBlock (16));
    REQUIRE (noteOnOffsets (stream.events()) == std::vector<std::uint32_t> { 8u });
}

TEST_CASE ("MIDI transpose Node transforms writable Note Events in place", "[midi][effect][transpose]")
{
    std::array<Event, 3> events {
        noteEvent (2, EventType::NoteOn, 60, 60.0),
        makeParameterChangeEvent (3, 7, 1, 0.5),
        noteEvent (6, EventType::NoteOff, 61, 61.25),
    };

    EventStream stream { std::span<Event> (events) };
    REQUIRE (stream.isWritable());
    REQUIRE (stream.isValidForBlock (8));

    MidiTransposeNode node (500, 7);
    processMidiEffectNode (node, stream);

    REQUIRE (events[0].timeInBlock == 2u);
    REQUIRE (events[0].voice.key == 67);
    REQUIRE (events[0].payload.note.pitchNote == Approx (67.0));
    REQUIRE (events[1].type == EventType::ParameterChange);
    REQUIRE (events[1].payload.parameter.normalizedValue == Approx (0.5));
    REQUIRE (events[2].timeInBlock == 6u);
    REQUIRE (events[2].voice.key == 68);
    REQUIRE (events[2].payload.note.pitchNote == Approx (68.25));
    REQUIRE (stream.isValidForBlock (8));
}

TEST_CASE ("MIDI scale-map Node deterministically maps accidentals to the next scale tone", "[midi][effect][scale]")
{
    std::array<Event, 4> events {
        noteEvent (0, EventType::NoteOn, 61, 61.25),  // C# -> D in C major.
        noteEvent (1, EventType::NoteOn, 64, 64.0),   // E already belongs.
        noteEvent (2, EventType::NoteOff, 70, 70.0),  // Bb -> B in C major.
        noteEvent (3, EventType::NoteOn, 127, 127.0), // B stays valid at the top edge.
    };

    EventStream stream { std::span<Event> (events) };
    MidiScaleMapNode node (501, 0, MidiScaleMapNode::kMajorMask);
    processMidiEffectNode (node, stream);

    REQUIRE (events[0].voice.key == 62);
    REQUIRE (events[0].payload.note.pitchNote == Approx (62.25));
    REQUIRE (events[1].voice.key == 64);
    REQUIRE (events[1].payload.note.pitchNote == Approx (64.0));
    REQUIRE (events[2].voice.key == 71);
    REQUIRE (events[2].payload.note.pitchNote == Approx (71.0));
    REQUIRE (events[3].voice.key == 127);
    REQUIRE (events[3].payload.note.pitchNote == Approx (127.0));
    REQUIRE (stream.isValidForBlock (8));
}

TEST_CASE ("MPE boundary allocation assigns stable voice addresses before flattening", "[midi][mpe]")
{
    const std::array<TempoChange, 1> tempo { TempoChange { 0, 120.0, TempoCurve::Jump } };
    MidiClip clip = clipWithNotes ({
        note (3, 0, 8, 60),
        note (2, 0, 8, 64),
        note (4, 8, 4, 67),
    }, /*start*/ 100, /*length*/ 32);

    for (Note& n : clip.notes)
    {
        n.portIndex = -1;
        n.channel = -1;
    }

    std::array<Note, 3> allocated {};
    const auto allocation = allocateMpeVoiceAddresses (
        clip,
        MpeVoiceAllocationConfig { /*portIndex*/ 2, /*firstMemberChannel*/ 1, /*memberChannelCount*/ 2 },
        std::span<Note> (allocated));

    REQUIRE (allocation.status == MpeVoiceAllocationStatus::Ok);
    REQUIRE (allocation.notesWritten == clip.notes.size());

    // Same-start notes allocate in stable note-id order; the later non-overlapping note reuses channel 1.
    REQUIRE (allocated[0].portIndex == 2);
    REQUIRE (allocated[0].channel == 2);
    REQUIRE (allocated[1].portIndex == 2);
    REQUIRE (allocated[1].channel == 1);
    REQUIRE (allocated[2].portIndex == 2);
    REQUIRE (allocated[2].channel == 1);

    MidiClip renderClip = clip;
    renderClip.notes.assign (allocated.begin(), allocated.begin() + static_cast<std::ptrdiff_t> (allocation.notesWritten));

    std::array<Event, 8> events {};
    const auto flat = flattenMidiClipNotesForBlock (
        renderClip,
        MidiFlattenBlock { 100.0, 32, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (events));

    REQUIRE (flat.status == MidiFlattenStatus::Ok);
    REQUIRE (flat.eventsWritten == 6u);

    const std::span<const Event> flattened { events.data(), flat.eventsWritten };
    const Event* const c = findNoteOnFor (flattened, id (3));
    const Event* const e = findNoteOnFor (flattened, id (2));
    const Event* const g = findNoteOnFor (flattened, id (4));
    REQUIRE (c != nullptr);
    REQUIRE (e != nullptr);
    REQUIRE (g != nullptr);
    REQUIRE (c->voice.portIndex == 2);
    REQUIRE (c->voice.channel == 2);
    REQUIRE (e->voice.portIndex == 2);
    REQUIRE (e->voice.channel == 1);
    REQUIRE (g->voice.portIndex == 2);
    REQUIRE (g->voice.channel == 1);
}

TEST_CASE ("MPE boundary allocation preserves explicit voice hints and fails when voices are exhausted",
           "[midi][mpe]")
{
    MidiClip clip = clipWithNotes ({
        note (5, 0, 8, 60),
        note (6, 0, 8, 64),
    }, /*start*/ 100, /*length*/ 32);
    clip.notes[0].portIndex = -1;
    clip.notes[0].channel = -1;
    clip.notes[1].portIndex = 4;
    clip.notes[1].channel = 1;

    std::array<Note, 2> allocated {};
    const auto allocation = allocateMpeVoiceAddresses (
        clip,
        MpeVoiceAllocationConfig { /*portIndex*/ 4, /*firstMemberChannel*/ 1, /*memberChannelCount*/ 2 },
        std::span<Note> (allocated));

    REQUIRE (allocation.status == MpeVoiceAllocationStatus::Ok);
    REQUIRE (allocation.notesWritten == 2u);
    REQUIRE (allocated[0].portIndex == 4);
    REQUIRE (allocated[0].channel == 2);
    REQUIRE (allocated[1].portIndex == 4);
    REQUIRE (allocated[1].channel == 1);

    clip.notes[0].channel = -1;
    clip.notes[1].channel = -1;
    const auto exhausted = allocateMpeVoiceAddresses (
        clip,
        MpeVoiceAllocationConfig { /*portIndex*/ 0, /*firstMemberChannel*/ 1, /*memberChannelCount*/ 1 },
        std::span<Note> (allocated));

    REQUIRE (exhausted.status == MpeVoiceAllocationStatus::OutOfVoices);
    REQUIRE (exhausted.notesWritten == 0u);
}

TEST_CASE ("MPE boundary allocation avoids overlapping future explicit voice reservations", "[midi][mpe]")
{
    MidiClip clip = clipWithNotes ({
        note (7, 0, 12, 60),
        note (8, 4, 8, 64),
    }, /*start*/ 100, /*length*/ 32);
    clip.notes[0].portIndex = -1;
    clip.notes[0].channel = -1;
    clip.notes[1].portIndex = 0;
    clip.notes[1].channel = 1;

    std::array<Note, 2> allocated {};
    const auto allocation = allocateMpeVoiceAddresses (
        clip,
        MpeVoiceAllocationConfig { /*portIndex*/ 0, /*firstMemberChannel*/ 1, /*memberChannelCount*/ 2 },
        std::span<Note> (allocated));

    REQUIRE (allocation.status == MpeVoiceAllocationStatus::Ok);
    REQUIRE (allocation.notesWritten == 2u);
    REQUIRE (allocated[0].portIndex == 0);
    REQUIRE (allocated[0].channel == 2);
    REQUIRE (allocated[1].portIndex == 0);
    REQUIRE (allocated[1].channel == 1);
}

TEST_CASE ("MIDI note-ons through a latent Instrument Node are aligned by PDC", "[midi][instrument][pdc]")
{
    constexpr NodeId kFastInstrument = 1000;
    constexpr NodeId kSlowInstrument = 1001;
    constexpr int kBlock = 32;
    constexpr int kInstrumentLatency = 5;

    const std::array<TempoChange, 1> tempo { TempoChange { 0, 120.0, TempoCurve::Jump } };
    const MidiClip clip = clipWithNotes ({ note (2, 4, 16) }, /*start*/ 100, /*length*/ 32);

    std::array<Event, 4> events {};
    const auto flat = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 100.0, kBlock, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (events));
    REQUIRE (flat.status == MidiFlattenStatus::Ok);
    REQUIRE (noteOnOffsets (std::span<const Event> (events.data(), flat.eventsWritten)) == std::vector<std::uint32_t> { 4u });

    auto fast = std::make_unique<ImpulseInstrumentNode> (kFastInstrument, 0);
    auto slow = std::make_unique<ImpulseInstrumentNode> (kSlowInstrument, kInstrumentLatency);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ fast.get(), slow.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 40;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = kSampleRate;
    inputs.maxBlockSize = kBlock;
    inputs.nodes.push_back (std::move (fast));
    inputs.nodes.push_back (std::move (slow));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->totalLatency() == kInstrumentLatency);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Latency) == 1u);

    const std::vector<float> out = render (*graph, std::span<const Event> (events.data(), flat.eventsWritten), kBlock);

    for (int i = 0; i < kBlock; ++i)
    {
        if (i == 4 + kInstrumentLatency)
            REQUIRE (out[static_cast<std::size_t> (i)] == Approx (2.0f));
        else
            REQUIRE (out[static_cast<std::size_t> (i)] == Approx (0.0f).margin (1.0e-6f));
    }
}

TEST_CASE ("MIDI-effect Nodes run before an Instrument Node in the compiled graph", "[midi][effect][graph]")
{
    constexpr NodeId kScale = 2000;
    constexpr NodeId kTranspose = 2001;
    constexpr NodeId kInstrument = 2002;
    constexpr int kBlock = 32;

    const std::array<TempoChange, 1> tempo { TempoChange { 0, 120.0, TempoCurve::Jump } };
    const MidiClip clip = clipWithNotes ({ note (2, 4, 16, 61) }, /*start*/ 100, /*length*/ 32);

    std::array<Event, 4> events {};
    const auto flat = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 100.0, kBlock, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (events));
    REQUIRE (flat.status == MidiFlattenStatus::Ok);
    REQUIRE (flat.eventsWritten == 2u);
    REQUIRE (events[0].voice.key == 61);

    auto scale = std::make_unique<MidiScaleMapNode> (kScale, 0, MidiScaleMapNode::kMajorMask);
    auto transpose = std::make_unique<MidiTransposeNode> (kTranspose, 12);
    auto instrument = std::make_unique<ImpulseInstrumentNode> (kInstrument, 0);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    ImpulseInstrumentNode* const instrumentPtr = instrument.get();

    transpose->setInput (scale.get());
    instrument->setEventInput (transpose.get());
    master->setInputNodes ({ instrument.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 41;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = kSampleRate;
    inputs.maxBlockSize = kBlock;
    inputs.nodes.push_back (std::move (scale));
    inputs.nodes.push_back (std::move (transpose));
    inputs.nodes.push_back (std::move (instrument));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::MidiEffect) == 2u);

    const std::vector<float> out = render (*graph, std::span<Event> (events.data(), flat.eventsWritten), kBlock);
    REQUIRE (instrumentPtr->lastNoteOnKey() == 74); // C# -> D in C major, then +12 semitones.
    REQUIRE (events[0].voice.key == 61);
    REQUIRE (events[0].payload.note.pitchNote == Approx (61.0));
    REQUIRE (events[1].voice.key == 61);
    REQUIRE (events[1].payload.note.pitchNote == Approx (61.0));

    for (int i = 0; i < kBlock; ++i)
    {
        if (i == 4)
            REQUIRE (out[static_cast<std::size_t> (i)] == Approx (1.0f));
        else
            REQUIRE (out[static_cast<std::size_t> (i)] == Approx (0.0f).margin (1.0e-6f));
    }
}

TEST_CASE ("MIDI-effect Nodes only transform their downstream Event branch", "[midi][effect][graph]")
{
    constexpr NodeId kTranspose = 2010;
    constexpr NodeId kShiftedInstrument = 2011;
    constexpr NodeId kRawInstrument = 2012;
    constexpr int kBlock = 32;

    std::array<Event, 2> events {
        noteEvent (4, EventType::NoteOn, 60, 60.0),
        noteEvent (12, EventType::NoteOff, 60, 60.0),
    };

    auto transpose = std::make_unique<MidiTransposeNode> (kTranspose, 12);
    auto shiftedInstrument = std::make_unique<ImpulseInstrumentNode> (kShiftedInstrument, 0);
    auto rawInstrument = std::make_unique<ImpulseInstrumentNode> (kRawInstrument, 0);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    ImpulseInstrumentNode* const shiftedPtr = shiftedInstrument.get();
    ImpulseInstrumentNode* const rawPtr = rawInstrument.get();

    shiftedInstrument->setEventInput (transpose.get());
    master->setInputNodes ({ shiftedInstrument.get(), rawInstrument.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 42;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = kSampleRate;
    inputs.maxBlockSize = kBlock;
    inputs.nodes.push_back (std::move (transpose));
    inputs.nodes.push_back (std::move (shiftedInstrument));
    inputs.nodes.push_back (std::move (rawInstrument));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const std::vector<float> out = render (*graph, std::span<Event> (events), kBlock);

    REQUIRE (shiftedPtr->lastNoteOnKey() == 72);
    REQUIRE (rawPtr->lastNoteOnKey() == 60);
    REQUIRE (out[4] == Approx (2.0f));
}

TEST_CASE ("Hosted instrument PluginNode receives transformed Note Events through the RT lane",
           "[midi][plugin][instrument][events]")
{
    constexpr NodeId kTranspose = 2020;
    constexpr NodeId kPlugin = 2021;
    constexpr int kBlock = 32;

    std::array<Event, 2> events {
        noteEvent (5, EventType::NoteOn, 60, 60.0),
        noteEvent (12, EventType::NoteOff, 60, 60.0),
    };
    events[0].voice.noteId = 1234;
    events[0].voice.portIndex = 3;
    events[0].voice.channel = 4;
    events[1].voice.noteId = 1234;
    events[1].voice.portIndex = 3;
    events[1].voice.channel = 4;

    auto transpose = std::make_unique<MidiTransposeNode> (kTranspose, 12);
    auto plugin = std::make_unique<PluginNode> (kPlugin, 1, kBlock);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);

    PluginNode* const pluginPtr = plugin.get();
    std::int16_t capturedKey = -1;
    std::int16_t capturedPort = -1;
    std::int16_t capturedChannel = -1;
    std::int32_t capturedNoteId = -1;
    std::uint32_t capturedOffset = 0;
    bool capturedNoteOn = false;

    plugin->setInput (transpose.get());
    plugin->setStubProcessor (
        [&capturedKey, &capturedPort, &capturedChannel, &capturedNoteId, &capturedOffset, &capturedNoteOn]
        (std::span<const Event> pluginEvents,
         const float* const*,
         float* const* output,
         int channels,
         int frames) noexcept
        {
            for (int c = 0; c < channels; ++c)
                for (int f = 0; f < frames; ++f)
                    output[c][f] = 0.0f;

            for (const Event& event : pluginEvents)
            {
                if (event.type != EventType::NoteOn || event.payload.note.normalizedVelocity <= 0.0)
                    continue;

                capturedKey = event.voice.key;
                capturedPort = event.voice.portIndex;
                capturedChannel = event.voice.channel;
                capturedNoteId = event.voice.noteId;
                capturedOffset = event.timeInBlock;
                capturedNoteOn = true;

                if (channels > 0 && event.timeInBlock < static_cast<std::uint32_t> (frames))
                    output[0][event.timeInBlock] = static_cast<float> (event.payload.note.normalizedVelocity);
            }
        });
    master->setInputNodes ({ plugin.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 43;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = kSampleRate;
    inputs.maxBlockSize = kBlock;
    inputs.nodes.push_back (std::move (transpose));
    inputs.nodes.push_back (std::move (plugin));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::MidiEffect) == 1u);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Plugin) == 1u);
    REQUIRE (graph->totalLatency() == kBlock);

    const std::vector<float> first = render (*graph, std::span<Event> (events), kBlock);
    for (float sample : first)
        REQUIRE (sample == Approx (0.0f).margin (1.0e-6f));

    REQUIRE (pluginPtr->serviceStubChild());
    REQUIRE (capturedNoteOn);
    REQUIRE (capturedNoteId == 1234);
    REQUIRE (capturedPort == 3);
    REQUIRE (capturedChannel == 4);
    REQUIRE (capturedKey == 72);
    REQUIRE (capturedOffset == 5u);
    REQUIRE (events[0].voice.key == 60);

    const std::vector<float> second = render (*graph, std::span<const Event> {}, kBlock);
    for (int i = 0; i < kBlock; ++i)
    {
        if (i == 5)
            REQUIRE (second[static_cast<std::size_t> (i)] == Approx (1.0f));
        else
            REQUIRE (second[static_cast<std::size_t> (i)] == Approx (0.0f).margin (1.0e-6f));
    }
}

// --- Negative controls ---------------------------------------------------------------------------
// Each of the next cases is built to FAIL if the specific machinery it guards regresses (the project
// rule: a green test that does not fail on a real break is theater). They cover the three failure
// modes the H4 gate has always claimed: block-boundary off-by-one, constant-tempo flattening across a
// tempo change, and uncompensated instrument latency.

TEST_CASE ("A Note landing exactly on a Block boundary belongs to the next Block", "[midi][flatten][boundary][negctl]")
{
    const std::array<TempoChange, 1> tempo { TempoChange { 0, 120.0, TempoCurve::Jump } };
    // Note at clip tick 8 -> project tick 108 -> frame 108, exactly the end of the first 8-frame block.
    const MidiClip clip = clipWithNotes ({ note (9, 8, 4) });

    std::array<Event, 4> firstOut {};
    const auto first = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 100.0, 8, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (firstOut));
    REQUIRE (first.status == MidiFlattenStatus::Ok);
    EventStream firstStream { std::span<const Event> (firstOut.data(), first.eventsWritten) };
    REQUIRE (firstStream.isValidForBlock (8));
    // Half-open: the boundary Note must NOT appear in [100, 108). A closed/closed regression would
    // emit it here at offset 8 and trip isValidForBlock.
    REQUIRE (noteOnOffsets (firstStream.events()).empty());

    std::array<Event, 4> secondOut {};
    const auto second = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 108.0, 8, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (secondOut));
    REQUIRE (second.status == MidiFlattenStatus::Ok);
    EventStream secondStream { std::span<const Event> (secondOut.data(), second.eventsWritten) };
    REQUIRE (secondStream.isValidForBlock (8));
    // ...and MUST appear at offset 0 of the next Block [108, 116).
    REQUIRE (noteOnOffsets (secondStream.events()) == std::vector<std::uint32_t> { 0u });
}

TEST_CASE ("Flattening through a tempo change differs from constant-tempo math", "[midi][flatten][tempo][negctl]")
{
    const std::array<TempoChange, 2> changing {
        TempoChange { 0, 120.0, TempoCurve::Jump },
        TempoChange { 100, 60.0, TempoCurve::Jump },
    };
    const std::array<TempoChange, 1> constant { TempoChange { 0, 120.0, TempoCurve::Jump } };
    // Note 4 ticks past the 60 BPM change: the full map places its NoteOn at offset 8, constant 120 at 4.
    const MidiClip clip = clipWithNotes ({ note (2, 4, 4) }, /*start*/ 100, /*length*/ 16);

    std::array<Event, 4> mapped {};
    const auto withChange = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 100.0, 16, 0.0 },
        tempoView (changing),
        SampleRate { kSampleRate },
        std::span<Event> (mapped));
    REQUIRE (withChange.status == MidiFlattenStatus::Ok);

    std::array<Event, 4> flat {};
    const auto withConstant = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 100.0, 16, 0.0 },
        tempoView (constant),
        SampleRate { kSampleRate },
        std::span<Event> (flat));
    REQUIRE (withConstant.status == MidiFlattenStatus::Ok);

    const std::vector<std::uint32_t> mappedOffsets = noteOnOffsets (std::span<const Event> (mapped.data(), withChange.eventsWritten));
    const std::vector<std::uint32_t> constantOffsets = noteOnOffsets (std::span<const Event> (flat.data(), withConstant.eventsWritten));

    REQUIRE (mappedOffsets == std::vector<std::uint32_t> { 8u });
    REQUIRE (constantOffsets == std::vector<std::uint32_t> { 4u });
    // A regression that flattened at constant tempo across the change would collapse these — they must differ.
    REQUIRE (mappedOffsets != constantOffsets);
}

TEST_CASE ("PDC moves a latent instrument's impulse vs an uncompensated single path", "[midi][instrument][pdc][negctl]")
{
    constexpr int kBlock = 32;
    constexpr int kLatency = 5;
    const std::array<TempoChange, 1> tempo { TempoChange { 0, 120.0, TempoCurve::Jump } };
    const MidiClip clip = clipWithNotes ({ note (2, 4, 16) }, /*start*/ 100, /*length*/ 32);

    std::array<Event, 4> events {};
    const auto flat = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 100.0, kBlock, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (events));
    REQUIRE (flat.status == MidiFlattenStatus::Ok);
    const std::span<const Event> evspan { events.data(), flat.eventsWritten };

    // Single zero-latency path: nothing to compensate, so the impulse lands at the raw NoteOn frame 4.
    {
        auto only = std::make_unique<ImpulseInstrumentNode> (3000, 0);
        auto master = std::make_unique<MasterNode> (kMasterId, 1);
        master->setInputNodes ({ only.get() });

        GraphBuilder::Inputs inputs;
        inputs.id = 50;
        inputs.masterNodeId = kMasterId;
        inputs.sampleRate = kSampleRate;
        inputs.maxBlockSize = kBlock;
        inputs.nodes.push_back (std::move (only));
        inputs.nodes.push_back (std::move (master));

        GraphBuildError error;
        std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
        REQUIRE (graph != nullptr);
        REQUIRE (graph->totalLatency() == 0);

        const std::vector<float> out = render (*graph, evspan, kBlock);
        REQUIRE (out[4] == Approx (1.0f));
    }

    // Add a latent sibling: PDC must splice a Latency Node onto the fast path so the fast impulse moves
    // OFF frame 4 to the compensated frame 4 + latency, where it sums with the slow path.
    {
        auto fast = std::make_unique<ImpulseInstrumentNode> (3001, 0);
        auto slow = std::make_unique<ImpulseInstrumentNode> (3002, kLatency);
        auto master = std::make_unique<MasterNode> (kMasterId, 1);
        master->setInputNodes ({ fast.get(), slow.get() });

        GraphBuilder::Inputs inputs;
        inputs.id = 51;
        inputs.masterNodeId = kMasterId;
        inputs.sampleRate = kSampleRate;
        inputs.maxBlockSize = kBlock;
        inputs.nodes.push_back (std::move (fast));
        inputs.nodes.push_back (std::move (slow));
        inputs.nodes.push_back (std::move (master));

        GraphBuildError error;
        std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
        REQUIRE (graph != nullptr);
        REQUIRE (graph->totalLatency() == kLatency);

        const std::vector<float> out = render (*graph, evspan, kBlock);
        REQUIRE (out[4] == Approx (0.0f).margin (1.0e-6f));        // fast path was shifted away from 4...
        REQUIRE (out[4 + kLatency] == Approx (2.0f));              // ...to 4 + latency, summing with the slow path.
    }
}

TEST_CASE ("Note-on across a Block boundary AND a tempo change reaches a latent instrument PDC-compensated",
           "[midi][instrument][pdc][tempo][boundary][integrated]")
{
    constexpr int kBlock = 8;
    constexpr int kLatency = 2;
    const std::array<TempoChange, 2> tempo {
        TempoChange { 0, 120.0, TempoCurve::Jump },
        TempoChange { 100, 60.0, TempoCurve::Jump },
    };
    // Clip starts at the tempo change (project tick 100 == frame 100). Note at clip tick 6 ->
    // project tick 106 -> 100 frames (120 BPM) + 6 ticks * 2 (60 BPM) = frame 112, in the SECOND block.
    const MidiClip clip = clipWithNotes ({ note (2, 6, 16) }, /*start*/ 100, /*length*/ 32);

    auto fast = std::make_unique<ImpulseInstrumentNode> (4000, 0);
    auto slow = std::make_unique<ImpulseInstrumentNode> (4001, kLatency);
    auto master = std::make_unique<MasterNode> (kMasterId, 1);
    master->setInputNodes ({ fast.get(), slow.get() });

    GraphBuilder::Inputs inputs;
    inputs.id = 52;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = kSampleRate;
    inputs.maxBlockSize = kBlock;
    inputs.nodes.push_back (std::move (fast));
    inputs.nodes.push_back (std::move (slow));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    REQUIRE (graph->totalLatency() == kLatency);

    // Block 1 = frames [100, 108): tempo-mapped, the note (frame 112) has not arrived.
    std::array<Event, 4> firstOut {};
    const auto first = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 100.0, kBlock, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (firstOut));
    REQUIRE (first.status == MidiFlattenStatus::Ok);
    REQUIRE (first.eventsWritten == 0u);
    const std::vector<float> firstAudio = render (*graph, std::span<const Event> (firstOut.data(), first.eventsWritten), kBlock);
    for (const float sample : firstAudio)
        REQUIRE (sample == Approx (0.0f).margin (1.0e-6f));

    // Block 2 = frames [108, 116): NoteOn at frame 112 -> offset 4; PDC aligns both paths at 4 + latency.
    std::array<Event, 4> secondOut {};
    const auto second = flattenMidiClipNotesForBlock (
        clip,
        MidiFlattenBlock { 108.0, kBlock, 0.0 },
        tempoView (tempo),
        SampleRate { kSampleRate },
        std::span<Event> (secondOut));
    REQUIRE (second.status == MidiFlattenStatus::Ok);
    REQUIRE (noteOnOffsets (std::span<const Event> (secondOut.data(), second.eventsWritten)) == std::vector<std::uint32_t> { 4u });

    const std::vector<float> secondAudio = render (*graph, std::span<const Event> (secondOut.data(), second.eventsWritten), kBlock);
    for (int i = 0; i < kBlock; ++i)
    {
        if (i == 4 + kLatency)
            REQUIRE (secondAudio[static_cast<std::size_t> (i)] == Approx (2.0f));
        else
            REQUIRE (secondAudio[static_cast<std::size_t> (i)] == Approx (0.0f).margin (1.0e-6f));
    }
}
