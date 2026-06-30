// YES DAW - H9 engine scaling and robustness gate.

#include "engine/GraphScheduler.h"
#include "engine/GraphBuilder.h"
#include "engine/PlaybackEngine.h"
#include "engine/Reliability.h"
#include "engine/nodes/DelayNode.h"
#include "engine/nodes/IdentityDcNode.h"
#include "engine/nodes/MasterNode.h"
#include "persistence/PluginFailureBlacklist.h"
#include "persistence/ProjectBundle.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::DeadlineSoakStats;
using yesdaw::engine::EntityId;
using yesdaw::engine::Marker;
using yesdaw::engine::MidiClip;
using yesdaw::engine::Note;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::PlaybackEngine;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::ScheduledRenderResult;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::TimeBase;
using yesdaw::engine::renderOfflineProject;
using yesdaw::engine::renderProjectWithScheduler;
using yesdaw::engine::summarizeDeadlineSoak;
using yesdaw::persistence::PluginFailureIdentity;
using yesdaw::persistence::PluginFailureKind;
using yesdaw::persistence::PluginStateChunkKind;
using yesdaw::persistence::PluginStateFormat;
using yesdaw::persistence::PluginStateRestoreChunk;
using yesdaw::persistence::PluginStateRestoreStatus;
using yesdaw::persistence::ProjectBundleDb;
using yesdaw::persistence::writePluginBlacklistEntryForFailure;

namespace {

constexpr double kSampleRate = 30720.0; // 120 BPM => one tick == one frame.
constexpr int    kBlockSize = 16;
constexpr float  kCenterGain = 0.70710677f;

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

AssetContentHash hashWithSeed (std::uint8_t seed) noexcept
{
    AssetContentHash hash;
    for (std::size_t i = 0; i < hash.bytes.size(); ++i)
        hash.bytes[i] = static_cast<std::uint8_t> (seed + static_cast<std::uint8_t> (i * 11u));

    return hash;
}

std::uint32_t floatBits (float value) noexcept
{
    std::uint32_t bits = 0;
    std::memcpy (&bits, &value, sizeof (bits));
    return bits;
}

bool bitIdentical (std::span<const float> a, std::span<const float> b) noexcept
{
    if (a.size() != b.size())
        return false;

    for (std::size_t i = 0; i < a.size(); ++i)
        if (floatBits (a[i]) != floatBits (b[i]))
            return false;

    return true;
}

std::filesystem::path tempPath (std::string_view label, std::string_view extension = ".yesdaw")
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
                               / ("yesdaw-h9-" + std::string (label) + "-" + std::to_string (ticks)
                                  + std::string (extension));

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

struct SchedulerFixture
{
    Project                         project;
    std::vector<std::vector<float>> samples;
    std::vector<DecodedAssetAudio>  decodedAssets;
};

Note makeNote (std::uint8_t id, std::int64_t start, std::int64_t length, std::int16_t key = 60) noexcept
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

SchedulerFixture makeSchedulerFixture()
{
    SchedulerFixture fixture;
    fixture.samples = {
        { -0.2f, 0.1f, 0.3f, -0.4f, 0.5f, -0.6f, 0.7f, -0.8f,
          0.9f, -0.1f, 0.2f, -0.3f, 0.4f, -0.5f, 0.6f, -0.7f },
        { 0.4f, 0.35f, -0.30f, 0.25f, -0.20f, 0.15f, -0.10f, 0.05f },
    };

    Asset first;
    first.id = idFromLowByte (10);
    first.contentHash = hashWithSeed (10);
    first.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    first.sampleRate = SampleRate { kSampleRate };
    first.channels = 1;

    Asset second;
    second.id = idFromLowByte (11);
    second.contentHash = hashWithSeed (11);
    second.frames = static_cast<std::uint64_t> (fixture.samples[1].size());
    second.sampleRate = SampleRate { kSampleRate };
    second.channels = 1;

    Clip left;
    left.id = idFromLowByte (20);
    left.assetId = first.id;
    left.timelineStart = 3;
    left.timelineLength = 16;
    left.srcOffset = 0;
    left.srcLen = 16;
    left.gain = 0.5f;
    left.fadeIn = 2;
    left.fadeOut = 3;
    left.timeBase = TimeBase::SampleLocked;

    Clip overlap;
    overlap.id = idFromLowByte (21);
    overlap.assetId = second.id;
    overlap.timelineStart = 12;
    overlap.timelineLength = 8;
    overlap.srcOffset = 0;
    overlap.srcLen = 8;
    overlap.gain = 0.25f;
    overlap.timeBase = TimeBase::SampleLocked;

    MidiClip midi;
    midi.id = idFromLowByte (30);
    midi.trackId = idFromLowByte (31);
    midi.timelineStart = 0;
    midi.timelineLength = 40;
    midi.timeBase = TimeBase::TempoLocked;
    midi.notes = { makeNote (32, 4, 4, 64), makeNote (33, 24, 4, 67) };

    fixture.project.id = idFromLowByte (1);
    fixture.project.sampleRate = SampleRate { kSampleRate };
    fixture.project.assets = { first, second };
    fixture.project.clips = { left, overlap };
    fixture.project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    fixture.project.midiClips = { midi };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());

    fixture.decodedAssets = {
        DecodedAssetAudio { first.id, first.sampleRate, first.frames, first.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
        DecodedAssetAudio { second.id, second.sampleRate, second.frames, second.channels,
                            std::span<const float> (fixture.samples[1].data(), fixture.samples[1].size()) },
    };

    return fixture;
}

std::vector<float> drainPlayback (PlaybackEngine& engine, std::uint64_t frames, int blockSize)
{
    const int channels = static_cast<int> (engine.channels());
    std::vector<float> out (static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels), 0.0f);
    std::vector<float> storage (static_cast<std::size_t> (channels) * static_cast<std::size_t> (blockSize), 0.0f);
    std::vector<float*> ptrs (static_cast<std::size_t> (channels), nullptr);

    std::uint64_t offset = 0;
    while (offset < frames)
    {
        const int n = static_cast<int> (std::min<std::uint64_t> (frames - offset, static_cast<std::uint64_t> (blockSize)));
        for (int c = 0; c < channels; ++c)
            ptrs[static_cast<std::size_t> (c)] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (blockSize);

        engine.processBlock (ptrs.data(), channels, n);

        for (int frame = 0; frame < n; ++frame)
            for (int c = 0; c < channels; ++c)
                out[(static_cast<std::size_t> (offset) + static_cast<std::size_t> (frame)) * static_cast<std::size_t> (channels)
                    + static_cast<std::size_t> (c)] = ptrs[static_cast<std::size_t> (c)][frame];

        offset += static_cast<std::uint64_t> (n);
    }

    return out;
}

std::vector<float> loopSlice (std::span<const float> full,
                              std::uint16_t channels,
                              std::int64_t loopStart,
                              std::int64_t loopEnd,
                              std::uint64_t frames)
{
    const std::int64_t loopLen = loopEnd - loopStart;
    std::vector<float> out (static_cast<std::size_t> (frames) * channels, 0.0f);
    for (std::uint64_t frame = 0; frame < frames; ++frame)
    {
        const std::int64_t srcFrame = loopStart + (static_cast<std::int64_t> (frame) % loopLen);
        for (std::uint16_t channel = 0; channel < channels; ++channel)
            out[static_cast<std::size_t> (frame) * channels + channel] =
                full[static_cast<std::size_t> (srcFrame) * channels + channel];
    }
    return out;
}

std::string blobLiteral (EntityId id)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out = "X'";
    for (std::uint8_t byte : id.bytes)
    {
        out.push_back (kHex[byte >> 4u]);
        out.push_back (kHex[byte & 0x0Fu]);
    }
    out.push_back ('\'');
    return out;
}

ProjectBundleDb openFreshBundle (const std::filesystem::path& path)
{
    ProjectBundleDb db;
    REQUIRE (ProjectBundleDb::openOrCreateBundle (path, db).ok());
    return db;
}

} // namespace

TEST_CASE ("YesDawSchedulerCheck: scheduled workers are bit-identical to serial offline render",
           "[h9][scheduler][determinism]")
{
    const SchedulerFixture fixture = makeSchedulerFixture();
    OfflineRenderOptions options;
    options.maxBlockSize = kBlockSize;

    const auto serial = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        options);
    REQUIRE (serial.ok());

    for (const std::uint32_t workers : { 1u, 2u, 4u, 8u })
    {
        const ScheduledRenderResult scheduled = renderProjectWithScheduler (
            fixture.project,
            std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
            workers,
            options);

        REQUIRE (scheduled.ok());
        REQUIRE (scheduled.frames == serial.frames);
        REQUIRE (scheduled.channels == serial.channels);
        REQUIRE (bitIdentical (scheduled.interleavedSamples, serial.interleavedSamples));
    }
}

TEST_CASE ("YesDawSchedulerCheck negative control: without absolute transport frames, block order changes output",
           "[h9][scheduler][determinism][negative-control]")
{
    // The determinism gate above passes because every source node is keyed by an absolute transport frame,
    // so block dispatch order cannot move a sample. This negative control proves that property is
    // LOAD-BEARING and that the bit-identity comparison can actually tell correct from incorrect: render the
    // SAME graph WITHOUT absolute transport frames (the legacy monotonic per-source cursor) in two different
    // block orders. The samples MUST differ -- if they did not, the determinism gate would be passing by
    // construction/luck, not because the scheduler is deterministic. (The real scheduler always sets
    // hasTimelineFrame=true; this drives the graph directly to exercise the unsafe path on purpose.)
    const SchedulerFixture fixture = makeSchedulerFixture();
    OfflineRenderOptions options;
    options.maxBlockSize = kBlockSize;

    auto renderInBlockOrder = [&] (std::span<const std::size_t> blockOrder)
    {
        auto built = yesdaw::engine::buildProjectGraph (
            fixture.project,
            std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
            options);
        REQUIRE (built.ok());

        const std::uint16_t channels = built.channels;
        const std::uint64_t frames   = built.frames;
        std::vector<float> out (static_cast<std::size_t> (frames) * channels, 0.0f);
        std::vector<float> storage (
            static_cast<std::size_t> (channels) * static_cast<std::size_t> (options.maxBlockSize), 0.0f);
        std::vector<float*> outputs (channels, nullptr);
        for (std::uint16_t c = 0; c < channels; ++c)
            outputs[c] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (options.maxBlockSize);

        for (const std::size_t block : blockOrder)
        {
            const std::uint64_t offset = block * static_cast<std::uint64_t> (options.maxBlockSize);
            const int blockFrames = static_cast<int> (
                std::min<std::uint64_t> (frames - offset, static_cast<std::uint64_t> (options.maxBlockSize)));

            yesdaw::engine::Transport transport;  // hasTimelineFrame defaults false => legacy monotonic cursor
            transport.projectSampleRate = built.sampleRate;
            transport.isPlaying = true;
            yesdaw::engine::EventStream events;
            built.graph->process (outputs.data(), channels, blockFrames, events, transport);

            for (int frame = 0; frame < blockFrames; ++frame)
                for (std::uint16_t c = 0; c < channels; ++c)
                    out[(static_cast<std::size_t> (offset) + static_cast<std::size_t> (frame)) * channels + c]
                        = outputs[c][frame];
        }
        return out;
    };

    const std::array<std::size_t, 3> canonicalOrder { 0u, 1u, 2u };
    const std::array<std::size_t, 3> shuffledOrder  { 2u, 0u, 1u };
    const std::vector<float> ordered  = renderInBlockOrder (canonicalOrder);
    const std::vector<float> shuffled = renderInBlockOrder (shuffledOrder);

    REQUIRE (ordered.size() == shuffled.size());
    REQUIRE_FALSE (bitIdentical (ordered, shuffled));
}

TEST_CASE ("YesDawSchedulerCheck refuses graphs that are not block-parallel-safe (ADR-0027)",
           "[h9][scheduler][determinism][guard]")
{
    OfflineRenderOptions options;
    options.maxBlockSize = kBlockSize;

    // The current Project surface compiles to an all-order-independent graph, so the scheduler accepts it.
    const SchedulerFixture fixture = makeSchedulerFixture();
    auto safe = yesdaw::engine::buildProjectGraph (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        options);
    REQUIRE (safe.ok());
    REQUIRE (safe.graph->isBlockParallelSafe());

    // A graph with a DelayNode carries cross-Block ring state, so it must NOT be marked safe — the scheduler
    // would otherwise scramble the ring when workers steal Blocks out of order. The default-false node
    // property means any future stateful node (reverb/automation/plugin) flips this the same way.
    const yesdaw::engine::NodeId masterId = 9000u;
    auto source = std::make_unique<yesdaw::engine::IdentityDcNode> (1u, 0.25f, 1);
    auto delay  = std::make_unique<yesdaw::engine::DelayNode> (2u, 4, 1);
    auto master = std::make_unique<yesdaw::engine::MasterNode> (masterId, 1);
    delay->setInput (source.get());
    master->setInputNodes ({ delay.get() });

    yesdaw::engine::GraphBuilder::Inputs inputs;
    inputs.masterNodeId = masterId;
    inputs.maxBlockSize = kBlockSize;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (delay));
    inputs.nodes.push_back (std::move (master));

    yesdaw::engine::GraphBuildError error;
    std::unique_ptr<yesdaw::engine::CompiledGraph> stateful =
        yesdaw::engine::GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (stateful != nullptr);
    REQUIRE_FALSE (stateful->isBlockParallelSafe());
}

TEST_CASE ("PlaybackEngine transport command queue is race-free while audio pumps Blocks",
           "[h9][transport][tsan]")
{
    const SchedulerFixture fixture = makeSchedulerFixture();
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        OfflineRenderOptions { 7100, 7101, 7102, kBlockSize });
    REQUIRE (created.ok());
    PlaybackEngine& engine = *created.engine;

    std::atomic<bool> start { false };
    std::atomic<bool> controlDone { false };
    std::atomic<bool> enqueueFailed { false };

    auto retryPost = [&] (auto&& fn)
    {
        for (int spin = 0; spin < 100000; ++spin)
        {
            if (fn())
                return true;
            std::this_thread::yield();
        }
        return false;
    };

    std::thread audio ([&]
    {
        std::vector<float> left (2u, 0.0f);
        std::vector<float> right (2u, 0.0f);
        float* outputs[2] = { left.data(), right.data() };
        while (! start.load (std::memory_order_acquire)) {}

        std::uint32_t tail = 0;
        while (! controlDone.load (std::memory_order_acquire) || tail < 1024u)
        {
            engine.processBlock (outputs, 2, 2);
            if (controlDone.load (std::memory_order_acquire))
                ++tail;
        }
    });

    std::thread control ([&]
    {
        while (! start.load (std::memory_order_acquire)) {}
        for (int i = 0; i < 2000; ++i)
        {
            if (! retryPost ([&] { return engine.locate (i % 28); })
                || ! retryPost ([&] { return engine.setLoop (4, 32); }))
            {
                enqueueFailed.store (true, std::memory_order_release);
                break;
            }
        }

        if (! retryPost ([&] { return engine.stop(); }))
            enqueueFailed.store (true, std::memory_order_release);

        controlDone.store (true, std::memory_order_release);
    });

    start.store (true, std::memory_order_release);
    control.join();
    audio.join();

    REQUIRE_FALSE (enqueueFailed.load (std::memory_order_acquire));
    REQUIRE (engine.locate (7));
    REQUIRE (engine.setLoop (4, 32));

    std::vector<float> left (2u, 0.0f);
    std::vector<float> right (2u, 0.0f);
    float* outputs[2] = { left.data(), right.data() };
    engine.processBlock (outputs, 2, 2);

    REQUIRE_FALSE (engine.isPlaying());
    REQUIRE (engine.loopEnabled());
    REQUIRE (engine.loopStartFrame() == 4);
    REQUIRE (engine.loopEndFrame() == 32);
    REQUIRE (engine.playheadFrame() == 7);
}

TEST_CASE ("Project MIDI auto-wire follows transport locate and loop through PlaybackEngine",
           "[h9][midi][transport]")
{
    const SchedulerFixture fixture = makeSchedulerFixture();
    OfflineRenderOptions options;
    options.maxBlockSize = kBlockSize;

    const auto offline = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        options);
    REQUIRE (offline.ok());
    REQUIRE (offline.channels == 2);

    Project audioOnly = fixture.project;
    audioOnly.midiClips.clear();
    const auto audioReference = renderOfflineProject (
        audioOnly,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        options);
    REQUIRE (audioReference.ok());

    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        options);
    REQUIRE (created.ok());

    REQUIRE (created.engine->locate (4));
    const std::vector<float> located = drainPlayback (*created.engine, 12, 3);
    REQUIRE (bitIdentical (
        located,
        std::span<const float> (offline.interleavedSamples.data() + 4u * 2u, located.size())));
    REQUIRE (std::fabs ((located[0] - audioReference.interleavedSamples[4u * 2u]) - kCenterGain) < 0.000001f);
    REQUIRE (std::fabs ((located[1] - audioReference.interleavedSamples[4u * 2u + 1u]) - kCenterGain) < 0.000001f);

    PlaybackEngine::Result loopedEngine = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        options);
    REQUIRE (loopedEngine.ok());
    REQUIRE (loopedEngine.engine->setLoop (4, 28));
    REQUIRE (loopedEngine.engine->locate (4));

    const std::vector<float> looped = drainPlayback (*loopedEngine.engine, 56, 5);
    const std::vector<float> expected = loopSlice (offline.interleavedSamples, offline.channels, 4, 28, 56);
    REQUIRE (bitIdentical (looped, expected));
}

TEST_CASE ("Parallel scheduler soak feeds the H6 deadline oracle with measured scheduled Blocks",
           "[h9][scheduler][soak]")
{
    #if defined (YESDAW_SANITIZER_BUILD)
    constexpr int          kTracks = 24;
    constexpr std::int64_t kSoakFrames = 16384;
    #elif defined (__APPLE__)
    // GitHub's macOS shared runners have shown enough scheduling jitter to push otherwise-healthy
    // p999 timings over one 128-frame period. Keep the deterministic soak there, but leave the hard
    // shared-runner timing gate to Linux.
    constexpr int          kTracks = 64;
    constexpr std::int64_t kSoakFrames = 128000;
    #else
    constexpr int          kTracks = 100;
    constexpr std::int64_t kSoakFrames = 128000;
    #endif

    SchedulerFixture fixture;
    fixture.project.id = idFromLowByte (1);
    fixture.project.sampleRate = SampleRate { kSampleRate };
    fixture.project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    fixture.project.markers = { Marker { idFromLowByte (220), kSoakFrames, "scheduler-soak-extent" } };

    fixture.samples.reserve (kTracks);
    fixture.project.assets.reserve (kTracks);
    fixture.project.clips.reserve (kTracks);
    fixture.decodedAssets.reserve (kTracks);
    for (int track = 0; track < kTracks; ++track)
    {
        std::vector<float> samples (128u, 0.001f * static_cast<float> ((track % 17) + 1));
        const std::uint8_t assetByte = static_cast<std::uint8_t> (10 + track);
        const std::uint8_t clipByte = static_cast<std::uint8_t> (120 + track);

        Asset asset;
        asset.id = idFromLowByte (assetByte);
        asset.contentHash = hashWithSeed (assetByte);
        asset.frames = samples.size();
        asset.sampleRate = SampleRate { kSampleRate };
        asset.channels = 1;

        Clip clip;
        clip.id = idFromLowByte (clipByte);
        clip.assetId = asset.id;
        clip.timelineStart = 0;
        clip.timelineLength = kSoakFrames;
        clip.srcOffset = 0;
        clip.srcLen = 128;
        clip.gain = 0.25f + 0.001f * static_cast<float> (track % 11);
        clip.timeBase = TimeBase::SampleLocked;

        fixture.project.assets.push_back (asset);
        fixture.project.clips.push_back (clip);
        fixture.samples.push_back (std::move (samples));
    }

    for (std::size_t i = 0; i < fixture.project.assets.size(); ++i)
    {
        const Asset& asset = fixture.project.assets[i];
        fixture.decodedAssets.push_back (
            DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                                std::span<const float> (fixture.samples[i].data(), fixture.samples[i].size()) });
    }
    REQUIRE (fixture.project.hasValidAssetClipIndirection());

    OfflineRenderOptions options;
    options.maxBlockSize = 128;
    std::vector<std::uint64_t> blockNanos;

    // Warm once so the timed run measures steady graph work rather than first-touch effects.
    REQUIRE (renderProjectWithScheduler (
                 fixture.project,
                 std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
                 4,
                 options)
                 .ok());

    const ScheduledRenderResult scheduled = renderProjectWithScheduler (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        4,
        options,
        &blockNanos);
    REQUIRE (scheduled.ok());

    // Bite the determinism failure mode AT SCALE. The small determinism case only fills 3 blocks, so it
    // never exercises worker contention or false-sharing across a heavy multi-track soak. Compare the
    // parallel render against the serial reference bit-for-bit on the real heavy fixture -- a contention or
    // worker-ordering bug that only shows up under load bites here (deterministically, no timing involved).
    const auto serial = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        options);
    REQUIRE (serial.ok());
    REQUIRE (bitIdentical (scheduled.interleavedSamples, serial.interleavedSamples));

    const std::size_t expectedBlocks = static_cast<std::size_t> (kSoakFrames / options.maxBlockSize);
    REQUIRE (blockNanos.size() == expectedBlocks);
    // Every block must have been timed by SOME worker: the scheduler stores >=1 ns for a processed block and
    // leaves 0u only for an untouched slot, so a regression that skipped blocks bites here instead of sliding
    // past the size-equality tautology.
    for (const std::uint64_t nanos : blockNanos)
        REQUIRE (nanos > 0u);

    const DeadlineSoakStats stats = summarizeDeadlineSoak (blockNanos, kSampleRate, options.maxBlockSize, 1u, 0u);
    REQUIRE (stats.blocksProcessed == expectedBlocks);
    REQUIRE (stats.underruns == 0u);
    INFO ("p999=" << stats.p999BlockNanos << " period=" << stats.blockPeriodNanos
                  << " max=" << stats.maxBlockNanos);
    #if ! defined (YESDAW_SANITIZER_BUILD) && ! defined (_WIN32) && ! defined (__APPLE__)
    REQUIRE (stats.passesDeadline());
    #endif
}

TEST_CASE ("Seeded parser fuzz replay keeps bundle and plugin-state parsers clean",
           "[h9][fuzz][persistence]")
{
    const std::filesystem::path path = tempPath ("fuzz-replay");
    ProjectBundleDb db = openFreshBundle (path);
    const SchedulerFixture fixture = makeSchedulerFixture();
    REQUIRE (db.writeProjectSnapshot (fixture.project).ok());

    const std::array<std::string_view, 4> projectMutations {
        "PRAGMA ignore_check_constraints = ON; UPDATE clips SET src_len = 999999 WHERE rowid = 1; PRAGMA ignore_check_constraints = OFF;",
        "PRAGMA ignore_check_constraints = ON; UPDATE midi_notes SET length_ticks = 999999 WHERE rowid = 1; PRAGMA ignore_check_constraints = OFF;",
        "UPDATE clips SET src_offset = 0.5 WHERE rowid = 1;",
        "PRAGMA ignore_check_constraints = ON; UPDATE midi_notes SET key = 255 WHERE rowid = 1; PRAGMA ignore_check_constraints = OFF;"
    };

    for (std::string_view sql : projectMutations)
    {
        INFO (sql);
        ProjectBundleDb fuzzDb = openFreshBundle (tempPath ("fuzz-project"));
        REQUIRE (fuzzDb.writeProjectSnapshot (fixture.project).ok());
        REQUIRE (fuzzDb.executeSql (std::string (sql)).ok());
        const auto validation = fuzzDb.validateStoredProjectSemantics();
        REQUIRE_FALSE (validation.ok());
    }

    const EntityId nodeId = idFromLowByte (240);
    const std::vector<std::uint8_t> stateBytes { 0x41u, 0x42u, 0x43u, 0x44u };
    REQUIRE (db.writePluginStateChunk ({ nodeId,
                                         PluginStateFormat::Vst3,
                                         "com.yesdaw.h9.fuzz",
                                         "1.0.0",
                                         PluginStateChunkKind::Vst3Component,
                                         stateBytes })
                 .ok());

    const std::array<std::string, 5> chunkMutations {
        "UPDATE plugin_state_chunks SET chunk_len = 99 WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;",
        "UPDATE plugin_state_chunks SET crc32 = 0 WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;",
        "UPDATE plugin_state_chunks SET format = X'76737433' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;",
        "UPDATE plugin_state_chunks SET data = 'ABCD' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;",
        "UPDATE plugin_state_chunks SET chunk_kind = X'00' WHERE node_id = " + blobLiteral (nodeId) + " AND chunk_kind = 0;"
    };

    for (const std::string& mutation : chunkMutations)
    {
        ProjectBundleDb fuzzDb = openFreshBundle (tempPath ("fuzz-plugin"));
        REQUIRE (fuzzDb.writePluginStateChunk ({ nodeId,
                                                PluginStateFormat::Vst3,
                                                "com.yesdaw.h9.fuzz",
                                                "1.0.0",
                                                PluginStateChunkKind::Vst3Component,
                                                stateBytes })
                     .ok());
        REQUIRE (fuzzDb.executeSql ("PRAGMA ignore_check_constraints = ON; " + mutation
                                    + " PRAGMA ignore_check_constraints = OFF;")
                     .ok());

        std::vector<PluginStateRestoreChunk> chunks;
        REQUIRE (fuzzDb.readPluginStateChunksForNode (nodeId, chunks).ok());
        REQUIRE (chunks.size() == 1u);
        REQUIRE (chunks[0].status == PluginStateRestoreStatus::Unreadable);
        REQUIRE_FALSE (chunks[0].ready());
    }
}

TEST_CASE ("Plugin failure action persists a blacklist row keyed by plugin identity",
           "[h9][plugin][blacklist]")
{
    const std::filesystem::path path = tempPath ("blacklist-action");
    ProjectBundleDb db = openFreshBundle (path);
    const PluginFailureIdentity identity {
        PluginStateFormat::Vst3,
        "com.yesdaw.h9.synthetic.bad",
        "9.0.0"
    };

    REQUIRE (writePluginBlacklistEntryForFailure (db, identity, PluginFailureKind::WatchdogTimeout).ok());
    bool present = false;
    REQUIRE (db.pluginBlacklistContains (identity.format, identity.pluginUid, identity.pluginVersion, present).ok());
    REQUIRE (present);

    REQUIRE (writePluginBlacklistEntryForFailure (db, identity, PluginFailureKind::Crash).ok());
    std::string reason;
    REQUIRE (db.queryText (
                 "SELECT reason FROM plugin_blacklist WHERE format = 'vst3' "
                 "AND plugin_uid = 'com.yesdaw.h9.synthetic.bad' AND plugin_version = '9.0.0';",
                 reason)
                 .ok());
    REQUIRE (reason == "crash");

    ProjectBundleDb reopened;
    REQUIRE (ProjectBundleDb::openExistingBundle (path, reopened).ok());
    bool afterRestart = false;
    REQUIRE (reopened.pluginBlacklistContains (identity.format, identity.pluginUid, identity.pluginVersion, afterRestart).ok());
    REQUIRE (afterRestart);
}
