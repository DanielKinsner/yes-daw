// G3.10 — RT-safe MIDI input and thru: the device→engine lane (MidiInputQueue), the thru target, and
// the one-block latency law. Pure C++ + Catch2, so it runs on the RTSan leg too (the audio-thread
// role — the engine's processBlock draining the queue — under -fsanitize=realtime proves it never
// allocates, locks or makes a syscall).
//
// Gates:
//  1. The queue: takes notes only; counts every push (seen); a full queue refuses the newest (never
//     blocks); pop stamps the thru target and DROPS (counting) an event with no target; an event that
//     already names a target keeps it.
//  2. Latency: a NoteOn pushed on the "device thread" reaches the selected Instrument in the NEXT block
//     the engine processes — the block right after the push has energy (transport stopped: the G3.2
//     live-only law), and the drained count reads 1.
//  3. Thru follows the target: with the target 0 the note is dropped and the render stays silent; a
//     second Track's Instrument as the target plays THAT synth (its render differs from the first's
//     because its cutoff differs).
//  4. The queue outlives an engine: a note pushed after engine A is destroyed plays through engine B
//     created on the same queue; a NoteOff on B releases it (silence after the release).
//  5. Producer / consumer stress: a device thread pushing while the audio thread pops never loses
//     ordering (keys arrive in push order) — the SPSC law.

#include "engine/GraphBuilder.h"
#include "engine/MidiInputQueue.h"
#include "engine/OfflineRenderer.h"
#include "engine/PlaybackEngine.h"
#include "engine/Project.h"
#include "engine/ProjectMixerProjection.h"
#include "engine/nodes/SimpleSynthNode.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <span>
#include <thread>
#include <vector>

using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::Event;
using yesdaw::engine::EventType;
using yesdaw::engine::MidiClip;
using yesdaw::engine::MidiInputQueue;
using yesdaw::engine::NodeId;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::PlaybackEngine;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectMixerNodeRole;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::TrackInstrumentKind;

namespace {

constexpr double kSampleRate = 30720.0;

EntityId idFromLowByte (std::uint8_t low)
{
    EntityId id;
    id.bytes.fill (0);
    id.bytes.back() = low;
    return id;
}

Event inputNote (std::int16_t key, bool on, double velocity = 0.8, NodeId target = 0)
{
    Event event;
    event.type = on ? EventType::NoteOn : EventType::NoteOff;
    event.voice.key = key;
    event.voice.channel = 0;
    event.voice.noteId = key;
    event.payload.note.normalizedVelocity = on ? velocity : 0.0;
    event.payload.note.pitchNote = static_cast<double> (key);
    event.payload.note.targetNode = target;
    return event;
}

// Two MIDI tracks, each with an (empty) clip so each projects an Instrument; the second's cutoff differs.
Project makeTwoSynthProject()
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { kSampleRate };
    project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    for (std::uint8_t i = 0; i < 2; ++i)
    {
        Track track;
        track.id = idFromLowByte (static_cast<std::uint8_t> (31 + i));
        track.strip.name = i == 0 ? "Keys" : "Lead";
        track.instrumentKind = TrackInstrumentKind::SimpleSynth;
        project.tracks.push_back (track);
        MidiClip clip;
        clip.id = idFromLowByte (static_cast<std::uint8_t> (41 + i));
        clip.trackId = track.id;
        clip.timelineStart = 0;
        clip.timelineLength = 30720;
        clip.timeBase = TimeBase::TempoLocked;
        project.midiClips.push_back (clip);
    }
    REQUIRE (yesdaw::engine::setTrackInstrumentParam (project, project.tracks[1].id, yesdaw::engine::SimpleSynthNode::kCutoffParamId, 0.2)
             == yesdaw::engine::ProjectEditStatus::Applied);
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

std::vector<float> renderEngineBlocks (PlaybackEngine& engine, int blocks, int blockSize)
{
    const int channels = static_cast<int> (engine.channels());
    REQUIRE (channels > 0);
    std::vector<float> out (static_cast<std::size_t> (blocks) * static_cast<std::size_t> (blockSize) * static_cast<std::size_t> (channels), 0.0f);
    std::vector<float> storage (static_cast<std::size_t> (channels) * static_cast<std::size_t> (blockSize), 0.0f);
    std::vector<float*> outputs (static_cast<std::size_t> (channels), nullptr);
    for (int c = 0; c < channels; ++c)
        outputs[static_cast<std::size_t> (c)] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (blockSize);
    for (int b = 0; b < blocks; ++b)
    {
        engine.processBlock (outputs.data(), channels, blockSize);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < blockSize; ++i)
                out[(static_cast<std::size_t> (b) * static_cast<std::size_t> (blockSize) + static_cast<std::size_t> (i)) * static_cast<std::size_t> (channels)
                    + static_cast<std::size_t> (c)] = outputs[static_cast<std::size_t> (c)][i];
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

std::unique_ptr<PlaybackEngine> makeEngine (const Project& project, MidiInputQueue& queue)
{
    OfflineRenderOptions options;
    options.maxBlockSize = 64;
    options.midiInput = &queue;
    PlaybackEngine::Result created = PlaybackEngine::create (project, std::span<const DecodedAssetAudio> {}, options);
    REQUIRE (created.ok());
    created.engine->stop();
    REQUIRE (created.engine->locate (0));
    return std::move (created.engine);
}

} // namespace

TEST_CASE ("MidiInputQueue: notes only, counted, bounded, the thru target stamped, no target drops", "[midi-input]")
{
    MidiInputQueue queue;
    Event parameter;
    parameter.type = EventType::ParameterChange;
    REQUIRE_FALSE (queue.push (parameter));
    REQUIRE (queue.seen() == 0u);

    REQUIRE (queue.push (inputNote (60, true)));
    REQUIRE (queue.seen() == 1u);
    Event out;
    REQUIRE_FALSE (queue.pop (out));   // no target: dropped, counted
    REQUIRE (queue.dropped() == 1u);

    queue.setThruTarget (77u);
    REQUIRE (queue.push (inputNote (61, true)));
    REQUIRE (queue.push (inputNote (62, true, 0.5, 99u)));   // names its own target
    REQUIRE (queue.pop (out));
    REQUIRE (out.voice.key == 61);
    REQUIRE (out.payload.note.targetNode == 77u);
    REQUIRE (queue.pop (out));
    REQUIRE (out.voice.key == 62);
    REQUIRE (out.payload.note.targetNode == 99u);
    REQUIRE_FALSE (queue.pop (out));

    // Bounded: the capacity holds, the next push is refused, nothing blocks.
    std::uint32_t accepted = 0;
    for (std::uint32_t i = 0; i < MidiInputQueue::kCapacity + 8; ++i)
        if (queue.push (inputNote (static_cast<std::int16_t> (i % 128), true)))
            ++accepted;
    REQUIRE (accepted >= MidiInputQueue::kCapacity - 1);
    REQUIRE (accepted <= MidiInputQueue::kCapacity);
    REQUIRE (queue.seen() == 3u + accepted);   // refused pushes are not seen
}

TEST_CASE ("MIDI thru latency: a pushed note reaches the Instrument in the next block; the target law; the queue outlives an engine", "[midi-input]")
{
    const Project project = makeTwoSynthProject();
    const NodeId keys = yesdaw::engine::projectMixerNodeIdForTrack (project.tracks[0].id, ProjectMixerNodeRole::Instrument);
    const NodeId lead = yesdaw::engine::projectMixerNodeIdForTrack (project.tracks[1].id, ProjectMixerNodeRole::Instrument);
    MidiInputQueue queue;

    SECTION ("one block of latency, stopped transport")
    {
        auto engine = makeEngine (project, queue);
        REQUIRE (engine->midiInput() == &queue);
        REQUIRE_FALSE (anyNonZero (renderEngineBlocks (*engine, 2, 64)));
        queue.setThruTarget (keys);
        REQUIRE (queue.push (inputNote (64, true)));
        // The very next block carries the note (block-top) and its attack is audible in it.
        const std::vector<float> first = renderEngineBlocks (*engine, 1, 64);
        REQUIRE (anyNonZero (first));
        REQUIRE (engine->midiInputDrained() == 1u);
        REQUIRE (engine->playheadFrame() == 0);   // stopped: the playhead holds
        REQUIRE (queue.push (inputNote (64, false)));
        (void) renderEngineBlocks (*engine, 400, 64);   // the release rings out (< 0.85 s)
        REQUIRE_FALSE (anyNonZero (renderEngineBlocks (*engine, 2, 64)));
        REQUIRE (engine->midiInputDrained() == 2u);
    }

    SECTION ("no target: dropped and silent; another target: that synth")
    {
        auto engine = makeEngine (project, queue);
        queue.setThruTarget (0u);
        REQUIRE (queue.push (inputNote (64, true)));
        REQUIRE_FALSE (anyNonZero (renderEngineBlocks (*engine, 4, 64)));
        REQUIRE (queue.dropped() >= 1u);
        REQUIRE (engine->midiInputDrained() == 0u);

        queue.setThruTarget (keys);
        REQUIRE (queue.push (inputNote (64, true)));
        const std::vector<float> viaKeys = renderEngineBlocks (*engine, 8, 64);
        REQUIRE (queue.push (inputNote (64, false)));
        (void) renderEngineBlocks (*engine, 400, 64);

        queue.setThruTarget (lead);
        REQUIRE (queue.push (inputNote (64, true)));
        const std::vector<float> viaLead = renderEngineBlocks (*engine, 8, 64);
        REQUIRE (anyNonZero (viaLead));
        REQUIRE (viaLead != viaKeys);   // the Lead's cutoff is 0.2: a different sound
        REQUIRE (queue.push (inputNote (64, false)));
    }

    SECTION ("the queue outlives an engine")
    {
        {
            auto engineA = makeEngine (project, queue);
            queue.setThruTarget (keys);
            REQUIRE (queue.push (inputNote (60, true)));
            REQUIRE (anyNonZero (renderEngineBlocks (*engineA, 2, 64)));
            REQUIRE (queue.push (inputNote (60, false)));
            (void) renderEngineBlocks (*engineA, 400, 64);
        }
        auto engineB = makeEngine (project, queue);
        REQUIRE (queue.push (inputNote (67, true)));
        REQUIRE (anyNonZero (renderEngineBlocks (*engineB, 2, 64)));
        REQUIRE (engineB->midiInputDrained() == 1u);
        REQUIRE (queue.push (inputNote (67, false)));
        (void) renderEngineBlocks (*engineB, 400, 64);
        REQUIRE_FALSE (anyNonZero (renderEngineBlocks (*engineB, 2, 64)));
    }
}

TEST_CASE ("MidiInputQueue SPSC stress: a device thread pushing, the consumer popping — order kept, nothing lost below capacity", "[midi-input]")
{
    MidiInputQueue queue;
    queue.setThruTarget (5u);
    constexpr int kNotes = 20000;
    std::atomic<bool> done { false };
    std::thread producer ([&] {
        for (int i = 0; i < kNotes; ++i)
        {
            while (! queue.push (inputNote (static_cast<std::int16_t> (i % 128), (i & 1) == 0)))
                std::this_thread::yield();
        }
        done.store (true, std::memory_order_release);
    });
    int received = 0;
    bool ordered = true;
    Event out;
    for (long spins = 0; received < kNotes && spins < 200'000'000L; ++spins)
    {
        if (queue.pop (out))
        {
            ordered = ordered && out.voice.key == static_cast<std::int16_t> (received % 128) && out.payload.note.targetNode == 5u;
            ++received;
        }
        else
            std::this_thread::yield();
    }
    producer.join();
    REQUIRE (done.load (std::memory_order_acquire));
    REQUIRE (ordered);
    REQUIRE (received == kNotes);
    REQUIRE (queue.seen() == static_cast<std::uint32_t> (kNotes));
}
