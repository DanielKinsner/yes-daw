// YES DAW - H4 MIDI timing gate.
//
// Proves MIDI Clip Notes flatten into sample-accurate Events across Block boundaries and a tempo change,
// then drive a non-zero-latency Instrument Node through GraphBuilder PDC.

#include "engine/GraphBuilder.h"
#include "engine/Midi.h"
#include "engine/nodes/ImpulseInstrumentNode.h"
#include "engine/nodes/MasterNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

using Catch::Approx;
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
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::Note;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::TempoMapView;
using yesdaw::engine::Tick;
using yesdaw::engine::flattenMidiClipNotesForBlock;
using yesdaw::engine::kTicksPerQuarter;
using yesdaw::engine::tickToFrame;

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

std::vector<std::uint32_t> noteOnOffsets (std::span<const Event> events)
{
    std::vector<std::uint32_t> offsets;
    for (const Event& event : events)
        if (event.type == EventType::NoteOn)
            offsets.push_back (event.timeInBlock);
    return offsets;
}

std::vector<float> render (CompiledGraph& graph, std::span<const Event> events, int frames)
{
    std::vector<float> out (static_cast<std::size_t> (frames), -999.0f);
    graph.process (out.data(), frames, events);
    return out;
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
