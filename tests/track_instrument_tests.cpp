// G3.1 / ADR-0047 — one persisted instrument per Track, shared by every MIDI Clip on it.
//
// Gates:
//  1. The compiled graph's N-event-input law: a node fed by several event producers reads ONE
//     stream, merged by timeInBlock, stable on ties, bounded by the per-block budget.
//  2. Projection identity: a Track holding MIDI projects ONE Instrument keyed by the Track and a
//     MidiMerge keyed by the Track; MidiSource stays keyed by the Clip; no Clip-keyed Instrument.
//  3. Shared voices: two overlapping MIDI Clips on one Track render bit-identically to one Clip
//     holding both notes at the same absolute positions — the merged stream is the same stream.
//  4. Pre-slot projects: a Track with TrackInstrumentKind::None and one Clip renders
//     bit-identically to the same Track marked SimpleSynth (None resolves to SimpleSynth).

#include "engine/GraphBuilder.h"
#include "engine/OfflineRenderer.h"
#include "engine/Project.h"
#include "engine/ProjectMixerProjection.h"
#include "engine/nodes/DecodedMidiClipNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/MidiMergeNode.h"
#include "engine/nodes/SimpleSynthNode.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNode;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::DecodedMidiClipNode;
using yesdaw::engine::EntityId;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::EventType;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::MasterNode;
using yesdaw::engine::MidiClip;
using yesdaw::engine::MidiMergeNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::Note;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectMixerNodeRole;
using yesdaw::engine::SampleRate;
using yesdaw::engine::ScheduledMidiEvent;
using yesdaw::engine::SimpleSynthNode;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::TrackInstrumentKind;
using yesdaw::engine::Transport;

namespace {

constexpr double kSampleRate = 30720.0;   // 120 BPM => one tick == one frame (the scheduler tests' law)

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

Note makeNote (std::uint8_t id, std::int64_t start, std::int64_t length, std::int16_t key) noexcept
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

ScheduledMidiEvent noteOnAt (std::int64_t frame, std::int16_t key) noexcept
{
    ScheduledMidiEvent out;
    out.frame = frame;
    out.event.type = EventType::NoteOn;
    out.event.voice.key = key;
    out.event.payload.note.normalizedVelocity = 1.0;
    return out;
}

// A consumer that records the ONE stream it is handed — fixed storage, no allocation in process.
class EventTapNode final : public Node
{
public:
    static constexpr std::size_t kCapacity = 64;

    EventTapNode (NodeId id, std::vector<Node*> inputs) : id_ (id), inputs_ (std::move (inputs)) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, 1, 0, id_, true };
    }
    std::span<Node* const> directInputs() const noexcept override { return inputs_; }
    void prepare (double, int) override {}
    void process (const ProcessArgs& args) noexcept override
    {
        for (int c = 0; c < args.audio.numChannels; ++c)
            for (int i = 0; i < args.numFrames; ++i)
                args.audio.channels[c][i] = 0.0f;
        count_ = 0;
        for (const Event& event : args.events)
            if (count_ < kCapacity)
                seen_[count_++] = event;
    }
    void reset() noexcept override { count_ = 0; }
    void release() override {}

    [[nodiscard]] std::span<const Event> seen() const noexcept { return { seen_.data(), count_ }; }

private:
    NodeId id_;
    std::vector<Node*> inputs_;
    std::array<Event, kCapacity> seen_ {};
    std::size_t count_ = 0;
};

const CompiledNode* compiledNodeById (const CompiledGraph& graph, NodeId id)
{
    for (const CompiledNode& node : graph.debugCompiledNodes())
        if (node.id == id)
            return &node;
    return nullptr;
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
    return out;
}

bool anyNonZero (const std::vector<float>& samples)
{
    for (const float s : samples)
        if (s != 0.0f)
            return true;
    return false;
}

} // namespace

TEST_CASE ("compiled graph merges N event-producing inputs into one time-ordered stream (stable, bounded)",
           "[engine][graph][event][midi-merge][g3]")
{
    constexpr NodeId kA = 11, kB = 12, kC = 13, kMerge = 20, kTap = 30, kMaster = 90;
    // Three producers with interleaved times; B and C share frame 8 (a tie: B first, then C).
    auto a = std::make_unique<DecodedMidiClipNode> (kA, std::vector<ScheduledMidiEvent> { noteOnAt (2, 60), noteOnAt (10, 61) });
    auto b = std::make_unique<DecodedMidiClipNode> (kB, std::vector<ScheduledMidiEvent> { noteOnAt (8, 62) });
    auto c = std::make_unique<DecodedMidiClipNode> (kC, std::vector<ScheduledMidiEvent> { noteOnAt (0, 63), noteOnAt (8, 64) });
    auto merge = std::make_unique<MidiMergeNode> (kMerge, std::vector<Node*> { a.get(), b.get(), c.get() });
    auto tap = std::make_unique<EventTapNode> (kTap, std::vector<Node*> { merge.get() });
    auto master = std::make_unique<MasterNode> (kMaster, 1);
    master->setInputNodes ({ tap.get() });
    EventTapNode* const tapPtr = tap.get();

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMaster;
    inputs.sampleRate = 48000.0;
    inputs.maxBlockSize = 16;
    inputs.nodes.push_back (std::move (a));
    inputs.nodes.push_back (std::move (b));
    inputs.nodes.push_back (std::move (c));
    inputs.nodes.push_back (std::move (merge));
    inputs.nodes.push_back (std::move (tap));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph != nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::None);

    const CompiledNode* const mergeNode = compiledNodeById (*graph, kMerge);
    REQUIRE (mergeNode != nullptr);
    REQUIRE (mergeNode->numEventInputs == 3);
    REQUIRE (mergeNode->eventOutputSlot != yesdaw::engine::kNoEventSlot);
    REQUIRE (mergeNode->eventInputSlot != yesdaw::engine::kRootEventSlot);
    const CompiledNode* const tapNode = compiledNodeById (*graph, kTap);
    REQUIRE (tapNode != nullptr);
    REQUIRE (tapNode->numEventInputs == 0);   // one producer: the plain single-slot path
    REQUIRE (tapNode->eventInputSlot == mergeNode->eventOutputSlot);

    std::vector<float> out (16, 0.0f);
    float* outChannels[1] = { out.data() };
    Transport transport;
    transport.projectSampleRate = SampleRate { 48000.0 };
    transport.isPlaying = true;
    EventStream events;
    graph->process (outChannels, 1, 16, events, transport);

    const std::span<const Event> seen = tapPtr->seen();
    REQUIRE (seen.size() == 5u);
    const std::array<std::pair<std::uint32_t, std::int16_t>, 5> expected {
        std::pair<std::uint32_t, std::int16_t> { 0u, 63 }, { 2u, 60 }, { 8u, 62 }, { 8u, 64 }, { 10u, 61 } };
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        INFO ("event " << i);
        REQUIRE (seen[i].timeInBlock == expected[i].first);
        REQUIRE (seen[i].voice.key == expected[i].second);
    }
}

TEST_CASE ("compiled graph refuses an event fan-in wider than the per-node ceiling",
           "[engine][graph][event][midi-merge][g3]")
{
    constexpr NodeId kMerge = 20, kTap = 30, kMaster = 90;
    std::vector<std::unique_ptr<Node>> owned;
    std::vector<Node*> producers;
    for (std::uint16_t i = 0; i < yesdaw::engine::kMaxEventInputsPerNode + 1; ++i)
    {
        auto source = std::make_unique<DecodedMidiClipNode> (static_cast<NodeId> (100 + i), std::vector<ScheduledMidiEvent> { noteOnAt (i, 60) });
        producers.push_back (source.get());
        owned.push_back (std::move (source));
    }
    auto merge = std::make_unique<MidiMergeNode> (kMerge, producers);
    auto tap = std::make_unique<EventTapNode> (kTap, std::vector<Node*> { merge.get() });
    auto master = std::make_unique<MasterNode> (kMaster, 1);
    master->setInputNodes ({ tap.get() });

    GraphBuilder::Inputs inputs;
    inputs.masterNodeId = kMaster;
    inputs.maxBlockSize = 16;
    for (auto& node : owned)
        inputs.nodes.push_back (std::move (node));
    inputs.nodes.push_back (std::move (merge));
    inputs.nodes.push_back (std::move (tap));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (graph == nullptr);
    REQUIRE (error.code() == GraphBuildError::Code::GraphTooLarge);
}

TEST_CASE ("a Track holding MIDI projects ONE Instrument and ONE MidiMerge keyed by the Track",
           "[engine][projection][instrument][g3]")
{
    MidiClip first;
    first.id = idFromLowByte (40);
    first.trackId = idFromLowByte (31);
    first.timelineStart = 0;
    first.timelineLength = 64;
    first.timeBase = TimeBase::TempoLocked;
    first.notes = { makeNote (50, 0, 32, 60) };
    MidiClip second = first;
    second.id = idFromLowByte (41);
    second.timelineStart = 16;
    second.notes = { makeNote (51, 0, 32, 64) };
    const Project project = makeMidiProject ({ first, second });

    OfflineRenderOptions options;
    options.maxBlockSize = 64;
    auto built = yesdaw::engine::buildProjectGraph (project, std::span<const DecodedAssetAudio> {}, options);
    REQUIRE (built.ok());

    const NodeId trackInstrument = yesdaw::engine::projectMixerNodeIdForTrack (project.tracks[0].id, ProjectMixerNodeRole::Instrument);
    const NodeId trackMerge = yesdaw::engine::projectMixerNodeIdForTrack (project.tracks[0].id, ProjectMixerNodeRole::MidiMerge);
    const NodeId firstSource = yesdaw::engine::projectMixerNodeIdForClip (first.id, ProjectMixerNodeRole::MidiSource);
    const NodeId secondSource = yesdaw::engine::projectMixerNodeIdForClip (second.id, ProjectMixerNodeRole::MidiSource);
    const NodeId clipKeyedInstrument = yesdaw::engine::projectMixerNodeIdForClip (first.id, ProjectMixerNodeRole::Instrument);

    REQUIRE (compiledNodeById (*built.graph, trackInstrument) != nullptr);
    const CompiledNode* const mergeNode = compiledNodeById (*built.graph, trackMerge);
    REQUIRE (mergeNode != nullptr);
    REQUIRE (mergeNode->numEventInputs == 2);
    REQUIRE (compiledNodeById (*built.graph, firstSource) != nullptr);
    REQUIRE (compiledNodeById (*built.graph, secondSource) != nullptr);
    REQUIRE (compiledNodeById (*built.graph, clipKeyedInstrument) == nullptr);   // ADR-0045's key is gone

    int instruments = 0;
    for (const CompiledNode& node : built.graph->debugCompiledNodes())
        if (dynamic_cast<SimpleSynthNode*> (node.node) != nullptr)
            ++instruments;
    REQUIRE (instruments == 1);
}

TEST_CASE ("two overlapping MIDI Clips on one Track render bit-identically to one Clip holding both notes",
           "[engine][projection][instrument][render][g3]")
{
    // Clip A: C4 at 0..96; Clip B (starts at 32): E4 at its 0..96 => absolute 32..128. Overlap 32..96.
    MidiClip a;
    a.id = idFromLowByte (40);
    a.trackId = idFromLowByte (31);
    a.timelineStart = 0;
    a.timelineLength = 256;
    a.timeBase = TimeBase::TempoLocked;
    a.notes = { makeNote (50, 0, 96, 60) };
    MidiClip b = a;
    b.id = idFromLowByte (41);
    b.timelineStart = 32;
    b.notes = { makeNote (51, 0, 96, 64) };

    MidiClip both = a;
    both.timelineLength = b.timelineStart + b.timelineLength;   // the same project end as the pair
    both.notes = { makeNote (50, 0, 96, 60), makeNote (51, 32, 96, 64) };

    const std::vector<float> twoClips = renderProject (makeMidiProject ({ a, b }));
    const std::vector<float> oneClip = renderProject (makeMidiProject ({ both }));
    REQUIRE (anyNonZero (twoClips));
    REQUIRE (twoClips.size() == oneClip.size());
    REQUIRE (twoClips == oneClip);

    // Negative control: dropping the second note changes the render (the comparison bites).
    MidiClip onlyFirst = both;
    onlyFirst.notes = { makeNote (50, 0, 96, 60) };
    const std::vector<float> single = renderProject (makeMidiProject ({ onlyFirst }));
    REQUIRE (single != twoClips);
}

TEST_CASE ("TrackInstrumentKind::None renders exactly as SimpleSynth (pre-slot bundles are unchanged)",
           "[engine][projection][instrument][render][g3]")
{
    MidiClip clip;
    clip.id = idFromLowByte (40);
    clip.trackId = idFromLowByte (31);
    clip.timelineStart = 0;
    clip.timelineLength = 128;
    clip.timeBase = TimeBase::TempoLocked;
    clip.notes = { makeNote (50, 0, 64, 67) };
    Project none = makeMidiProject ({ clip });
    REQUIRE (none.tracks[0].instrumentKind == TrackInstrumentKind::None);
    Project synth = none;
    synth.tracks[0].instrumentKind = TrackInstrumentKind::SimpleSynth;
    REQUIRE (synth.tracks[0].isValid());

    const std::vector<float> fromNone = renderProject (none);
    const std::vector<float> fromSynth = renderProject (synth);
    REQUIRE (anyNonZero (fromNone));
    REQUIRE (fromNone == fromSynth);

    // The slot's validity law: an unknown kind is invalid.
    Track bad = none.tracks[0];
    bad.instrumentKind = static_cast<TrackInstrumentKind> (9);
    REQUIRE_FALSE (bad.isValid());
}
