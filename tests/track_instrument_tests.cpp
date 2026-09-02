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
#include "engine/InstrumentState.h"
#include "engine/ProjectUndo.h"
#include "engine/OfflineRenderer.h"
#include "engine/Project.h"
#include "engine/ProjectMixerProjection.h"
#include "engine/nodes/DecodedMidiClipNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/MidiMergeNode.h"
#include "engine/nodes/SimpleSynthNode.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
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

// ---------------- cp2: ParamSpec, the verbs, the automation target ----------------

namespace {

double rmsOf (const std::vector<float>& samples, std::size_t begin, std::size_t end)
{
    double sum = 0.0;
    std::size_t n = 0;
    for (std::size_t i = begin; i < end && i < samples.size(); ++i, ++n)
        sum += static_cast<double> (samples[i]) * static_cast<double> (samples[i]);
    return n > 0 ? std::sqrt (sum / static_cast<double> (n)) : 0.0;
}

double peakOf (const std::vector<float>& samples)
{
    double peak = 0.0;
    for (const float s : samples)
        peak = std::max (peak, std::fabs (static_cast<double> (s)));
    return peak;
}

Project makeOneNoteProject (std::int64_t lengthTicks = 4096, std::int64_t noteLength = 2048, std::int16_t key = 60)
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

Project withParam (Project project, std::uint32_t paramId, double normalized)
{
    REQUIRE (yesdaw::engine::setTrackInstrumentParam (project, project.tracks[0].id, paramId, normalized)
             == yesdaw::engine::ProjectEditStatus::Applied);
    return project;
}

} // namespace

TEST_CASE ("SimpleSynth ParamSpec: every id has a usable spec, unknown ids are refused, defaults are the ADR-0043 sound",
           "[engine][instrument][params][g3]")
{
    for (std::uint32_t id = 1; id <= SimpleSynthNode::kParameterCount; ++id)
    {
        INFO ("param " << id);
        REQUIRE (SimpleSynthNode::acceptsParameterId (id));
        const yesdaw::engine::ParamSpec spec = SimpleSynthNode::parameterSpec (id);
        REQUIRE (spec.id == id);
        REQUIRE (yesdaw::engine::paramSpecHasUsableRange (spec));
        REQUIRE (yesdaw::engine::instrumentKindAcceptsParameterId (TrackInstrumentKind::None, id));
        REQUIRE (yesdaw::engine::instrumentKindAcceptsParameterId (TrackInstrumentKind::SimpleSynth, id));
    }
    REQUIRE_FALSE (SimpleSynthNode::acceptsParameterId (0));
    REQUIRE_FALSE (SimpleSynthNode::acceptsParameterId (SimpleSynthNode::kParameterCount + 1));
    REQUIRE_FALSE (yesdaw::engine::instrumentKindAcceptsParameterId (TrackInstrumentKind::SimpleSynth, 99));

    // Setting every parameter to its spec DEFAULT (in real units) leaves the render bit-identical
    // to an untouched instrument — the defaults ARE the historical sound.
    const Project plain = makeOneNoteProject();
    const std::vector<float> reference = renderProject (plain);
    REQUIRE (anyNonZero (reference));
    {
        OfflineRenderOptions options;
        options.maxBlockSize = 64;
        SimpleSynthNode probe (1);
        for (std::uint32_t id = 1; id <= SimpleSynthNode::kParameterCount; ++id)
        {
            const yesdaw::engine::ParamSpec spec = SimpleSynthNode::parameterSpec (id);
            REQUIRE (probe.parameterReal (id) == spec.def);
        }
    }
}

TEST_CASE ("every SimpleSynth parameter is audible: an off-default value changes the render and the closed forms hold",
           "[engine][instrument][params][render][g3]")
{
    const Project plain = makeOneNoteProject();
    const std::vector<float> reference = renderProject (plain);
    REQUIRE (anyNonZero (reference));

    // Each parameter, pushed away from its default, changes the render.
    const std::array<std::pair<std::uint32_t, double>, 9> offDefaults {
        std::pair<std::uint32_t, double> { SimpleSynthNode::kOscMixParamId, 0.0 },
        { SimpleSynthNode::kAttackParamId, 0.8 },
        { SimpleSynthNode::kDecayParamId, 0.7 },
        { SimpleSynthNode::kSustainParamId, 0.5 },
        { SimpleSynthNode::kReleaseParamId, 0.9 },
        { SimpleSynthNode::kCutoffParamId, 0.3 },
        { SimpleSynthNode::kResonanceParamId, 0.9 },
        { SimpleSynthNode::kGlideParamId, 0.5 },
        { SimpleSynthNode::kVolumeParamId, 0.5 } };
    for (const auto& [id, normalized] : offDefaults)
    {
        INFO ("param " << id);
        if (id == SimpleSynthNode::kGlideParamId)
            continue;   // glide needs a PREVIOUS note; proven below
        Project edited = withParam (plain, id, normalized);
        if (id == SimpleSynthNode::kDecayParamId)
            edited = withParam (edited, SimpleSynthNode::kSustainParamId, 0.5);   // decay only shows below full sustain
        if (id == SimpleSynthNode::kResonanceParamId)
            edited = withParam (edited, SimpleSynthNode::kCutoffParamId, 0.5);   // resonance only shows with the filter in
        const std::vector<float> changed = renderProject (edited);
        REQUIRE (changed != reference);
    }

    // Volume: -6.0206 dB halves the peak (the envelope and phase are untouched, so the ratio is exact
    // up to float rounding).
    {
        const yesdaw::engine::ParamSpec volume = SimpleSynthNode::parameterSpec (SimpleSynthNode::kVolumeParamId);
        const double halfDb = 20.0 * std::log10 (0.5);
        const std::vector<float> half = renderProject (withParam (plain, SimpleSynthNode::kVolumeParamId,
                                                                  yesdaw::engine::unmapToNormalized (volume, halfDb)));
        REQUIRE (peakOf (half) == Catch::Approx (peakOf (reference) * 0.5).epsilon (1.0e-3));
    }

    // Sustain 0.5 with a fast decay: the held level after the decay is half the reference's.
    {
        const yesdaw::engine::ParamSpec decay = SimpleSynthNode::parameterSpec (SimpleSynthNode::kDecayParamId);
        Project sustained = withParam (plain, SimpleSynthNode::kSustainParamId, 0.5);
        sustained = withParam (sustained, SimpleSynthNode::kDecayParamId, yesdaw::engine::unmapToNormalized (decay, 0.002));
        const std::vector<float> halfHeld = renderProject (sustained);
        // 30720 frames/s: frames 512..1536 sit after attack + decay and before the note-off at 2048.
        REQUIRE (rmsOf (halfHeld, 512, 1536) == Catch::Approx (rmsOf (reference, 512, 1536) * 0.5).epsilon (2.0e-2));
    }

    // Cutoff at 200 Hz on a C4 (261.6 Hz) note removes energy: the RMS drops well below the reference.
    {
        const yesdaw::engine::ParamSpec cutoff = SimpleSynthNode::parameterSpec (SimpleSynthNode::kCutoffParamId);
        const std::vector<float> dark = renderProject (withParam (plain, SimpleSynthNode::kCutoffParamId,
                                                                  yesdaw::engine::unmapToNormalized (cutoff, 200.0)));
        REQUIRE (rmsOf (dark, 512, 1536) < rmsOf (reference, 512, 1536) * 0.8);
        REQUIRE (anyNonZero (dark));
    }

    // Glide: a second note after a first slides into pitch — the render differs from no-glide
    // only AFTER the second note starts.
    {
        MidiClip clip;
        clip.id = idFromLowByte (40);
        clip.trackId = idFromLowByte (31);
        clip.timelineStart = 0;
        clip.timelineLength = 4096;
        clip.timeBase = TimeBase::TempoLocked;
        clip.notes = { makeNote (50, 0, 1024, 48), makeNote (51, 1024, 1024, 72) };
        const Project twoNotes = makeMidiProject ({ clip });
        const std::vector<float> straight = renderProject (twoNotes);
        const std::vector<float> glided = renderProject (withParam (twoNotes, SimpleSynthNode::kGlideParamId, 0.5));
        REQUIRE (straight != glided);
        REQUIRE (std::equal (straight.begin(), straight.begin() + 1024, glided.begin()));   // identical before note 2
    }
}

TEST_CASE ("SetTrackInstrument / SetTrackInstrumentParam are undoable Track verbs; a knob drag coalesces",
           "[engine][instrument][undo][g3]")
{
    using yesdaw::engine::ProjectEditCommand;
    using yesdaw::engine::ProjectUndoStack;
    Project project = makeOneNoteProject();
    const EntityId trackId = project.tracks[0].id;
    ProjectUndoStack undo;

    REQUIRE (undo.apply (project, ProjectEditCommand::setTrackInstrument (trackId, TrackInstrumentKind::SimpleSynth)).applied());
    REQUIRE (project.tracks[0].instrumentKind == TrackInstrumentKind::SimpleSynth);

    REQUIRE (undo.beginTransactionGroup());
    REQUIRE (undo.apply (project, ProjectEditCommand::setTrackInstrumentParam (trackId, SimpleSynthNode::kCutoffParamId, 0.4)).applied());
    REQUIRE (undo.apply (project, ProjectEditCommand::setTrackInstrumentParam (trackId, SimpleSynthNode::kCutoffParamId, 0.3)).applied());
    REQUIRE (undo.apply (project, ProjectEditCommand::setTrackInstrumentParam (trackId, SimpleSynthNode::kCutoffParamId, 0.2)).applied());
    REQUIRE (undo.endTransactionGroup());
    REQUIRE (project.tracks[0].instrumentParamNormalized (SimpleSynthNode::kCutoffParamId) == Catch::Approx (0.2));
    REQUIRE (undo.history().steps.size() == 2u);   // the kind, then ONE knob step

    REQUIRE (undo.undo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.tracks[0].instrumentState.empty());   // the whole drag is one step
    REQUIRE (undo.redo (project) == yesdaw::engine::ProjectUndoStatus::Applied);
    REQUIRE (project.tracks[0].instrumentParamNormalized (SimpleSynthNode::kCutoffParamId) == Catch::Approx (0.2));

    // An unknown parameter id and a non-normalized value are refused; the state is untouched.
    const Track before = project.tracks[0];
    REQUIRE_FALSE (undo.apply (project, ProjectEditCommand::setTrackInstrumentParam (trackId, 77, 0.5)).applied());
    REQUIRE_FALSE (undo.apply (project, ProjectEditCommand::setTrackInstrumentParam (trackId, SimpleSynthNode::kCutoffParamId, 1.5)).applied());
    REQUIRE (project.tracks[0] == before);

    // The blob is canonical: equal states encode to equal bytes regardless of edit order.
    Project a = makeOneNoteProject();
    Project b = makeOneNoteProject();
    a = withParam (withParam (a, 2, 0.25), 6, 0.5);
    b = withParam (withParam (b, 6, 0.5), 2, 0.25);
    REQUIRE (a.tracks[0].instrumentState == b.tracks[0].instrumentState);
    yesdaw::engine::InstrumentParamValues decoded;
    REQUIRE (yesdaw::engine::decodeInstrumentParams (a.tracks[0].instrumentState, decoded));
    REQUIRE (decoded == yesdaw::engine::InstrumentParamValues { { 2, 0.25 }, { 6, 0.5 } });
    std::vector<std::uint8_t> malformed = a.tracks[0].instrumentState;
    malformed.pop_back();
    REQUIRE_FALSE (yesdaw::engine::decodeInstrumentParams (malformed, decoded));
    Project corrupt = a;
    corrupt.tracks[0].instrumentState = malformed;
    REQUIRE_FALSE (corrupt.tracks[0].isValid());
}

TEST_CASE ("an automation lane on an instrument parameter targets the Track's Instrument and changes the render; it sleeps without MIDI",
           "[engine][instrument][automation][render][g3]")
{
    using yesdaw::engine::AutomationBreakpoint;
    using yesdaw::engine::AutomationCurveType;
    using yesdaw::engine::AutomationLaneData;
    using yesdaw::engine::AutomationTargetRole;

    Project plain = makeOneNoteProject();
    const std::vector<float> reference = renderProject (plain);

    // A cutoff lane held at a low value from tick 0.
    Project automated = plain;
    AutomationLaneData lane;
    lane.id = idFromLowByte (90);
    lane.ownerEntity = automated.tracks[0].id;
    lane.role = AutomationTargetRole::InstrumentParam;
    lane.paramId = SimpleSynthNode::kCutoffParamId;
    lane.points = { AutomationBreakpoint { 0, 0.3, AutomationCurveType::Hold } };
    automated.automationLanes = { lane };
    REQUIRE (automated.hasValidAssetClipIndirection());
    const std::vector<float> withLane = renderProject (automated);
    REQUIRE (anyNonZero (withLane));
    REQUIRE (withLane != reference);
    REQUIRE (rmsOf (withLane, 512, 1536) < rmsOf (reference, 512, 1536));

    // The lane's validity law: an unknown param id or a non-Track owner is invalid.
    Project badParam = automated;
    badParam.automationLanes[0].paramId = 99;
    REQUIRE_FALSE (badParam.hasValidAssetClipIndirection());
    Project badOwner = automated;
    badOwner.automationLanes[0].ownerEntity = idFromLowByte (77);
    REQUIRE_FALSE (badOwner.hasValidAssetClipIndirection());

    // Without MIDI on the lane's Track the lane has no node: it sleeps, the project still projects
    // (a second Track keeps the timeline non-empty, so the only question is the lane).
    Project silent = automated;
    Track other;
    other.id = idFromLowByte (32);
    other.strip.name = "Other";
    silent.tracks.push_back (other);
    for (MidiClip& clip : silent.midiClips)
        clip.trackId = other.id;
    REQUIRE (silent.hasValidAssetClipIndirection());
    OfflineRenderOptions options;
    options.maxBlockSize = 64;
    auto built = yesdaw::engine::buildProjectGraph (silent, std::span<const DecodedAssetAudio> {}, options);
    INFO ("status " << static_cast<int> (built.status) << " projection error " << static_cast<int> (built.projectError.code)
          << " at lane/clip " << built.projectError.clipIndex);
    REQUIRE (built.ok());
}
