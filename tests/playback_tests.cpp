// YES DAW - H8 playback gate: play a Project through the realtime Runtime and prove it equals an
// INDEPENDENT reference (Clips summed at their timeline positions) -- not the engine compared to itself.

#include "engine/OfflineRenderer.h"
#include "engine/PlaybackEngine.h"
#include "persistence/PlaybackAutosave.h"
#include "persistence/ProjectBundle.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::AutomationBreakpoint;
using yesdaw::engine::AutomationCurveType;
using yesdaw::engine::AutomationLaneData;
using yesdaw::engine::AutomationTargetRole;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::FaderNode;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::PanNode;
using yesdaw::engine::PlaybackEngine;
using yesdaw::engine::Project;
using yesdaw::engine::RecordingChunkFifo;
using yesdaw::engine::RecordingConfig;
using yesdaw::engine::RecordingTakeFileStatus;
using yesdaw::engine::RecordingTakeFileWriter;
using yesdaw::engine::findRecordedSample;
using yesdaw::engine::readRecordingTakeFile;
using yesdaw::engine::renderOfflineProject;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::persistence::ProjectBundleDb;
using yesdaw::persistence::readAutosaveSnapshot;
using yesdaw::persistence::writeAutosaveFromControlTick;

namespace {

constexpr float  kCenterGain = 0.70710677f;
constexpr double kTolerance  = 1.0e-6;

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
        hash.bytes[i] = static_cast<std::uint8_t> (seed + static_cast<std::uint8_t> (i * 13u));

    return hash;
}

struct PlaybackFixture
{
    Project                         project;
    std::vector<std::vector<float>> samples;
    std::vector<DecodedAssetAudio>  decodedAssets;
};

PlaybackFixture makePlaybackFixture()
{
    PlaybackFixture fixture;
    fixture.samples = {
        { -0.30f, 0.10f, 0.20f, -0.40f, 0.50f, -0.60f, 0.70f, -0.80f },
        { 0.90f, -0.75f, 0.60f, -0.45f, 0.30f, -0.15f },
    };

    Asset first;
    first.id = idFromLowByte (10);
    first.contentHash = hashWithSeed (10);
    first.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    first.sampleRate = SampleRate { 48000.0 };
    first.channels = 1;

    Asset second = first;
    second.id = idFromLowByte (11);
    second.contentHash = hashWithSeed (11);
    second.frames = static_cast<std::uint64_t> (fixture.samples[1].size());

    Clip left;
    left.id = idFromLowByte (20);
    left.assetId = first.id;
    left.trackId = idFromLowByte (30);
    left.timelineStart = 2;
    left.timelineLength = 6;
    left.srcOffset = 1;
    left.srcLen = 6;
    left.gain = 0.50f;
    left.fadeIn = 2;
    left.fadeOut = 2;
    left.timeBase = TimeBase::SampleLocked;

    Clip overlap;
    overlap.id = idFromLowByte (21);
    overlap.assetId = second.id;
    overlap.trackId = left.trackId;
    overlap.timelineStart = 5;
    overlap.timelineLength = 4;
    overlap.srcOffset = 0;
    overlap.srcLen = 4;
    overlap.gain = 0.25f;
    overlap.fadeIn = 0;
    overlap.fadeOut = 0;
    overlap.timeBase = TimeBase::SampleLocked;

    fixture.project.id = idFromLowByte (1);
    fixture.project.sampleRate = SampleRate { 48000.0 };
    fixture.project.assets = { first, second };
    Track track;
    track.id = left.trackId;
    track.strip.name = "Audio 1";
    fixture.project.tracks = { track };
    fixture.project.clips = { left, overlap };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());

    fixture.decodedAssets = {
        DecodedAssetAudio { first.id, first.sampleRate, first.frames, first.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
        DecodedAssetAudio { second.id, second.sampleRate, second.frames, second.channels,
                            std::span<const float> (fixture.samples[1].data(), fixture.samples[1].size()) },
    };
    return fixture;
}

PlaybackFixture makeAutomatedFaderPanFixture()
{
    PlaybackFixture fixture;
    fixture.samples = { {} };
    fixture.samples[0].reserve (128u);
    for (int i = 0; i < 128; ++i)
        fixture.samples[0].push_back (i % 2 == 0 ? 0.25f : -0.125f);

    Asset asset;
    asset.id = idFromLowByte (40);
    asset.contentHash = hashWithSeed (40);
    asset.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    asset.sampleRate = SampleRate { 30720.0 }; // 120 BPM makes one tick equal one frame.
    asset.channels = 1;

    Track track;
    track.id = idFromLowByte (41);
    track.strip.name = "Automated audio";
    track.strip.linearGain = 1.0f;
    track.strip.pan = 0.0f;

    Clip clip;
    clip.id = idFromLowByte (42);
    clip.assetId = asset.id;
    clip.trackId = track.id;
    clip.timelineStart = 0;
    clip.timelineLength = 128;
    clip.srcOffset = 0;
    clip.srcLen = 128;
    clip.gain = 1.0f;
    clip.timeBase = TimeBase::SampleLocked;

    AutomationLaneData fader;
    fader.id = idFromLowByte (43);
    fader.ownerEntity = track.id;
    fader.role = AutomationTargetRole::TrackFader;
    fader.paramId = FaderNode::kGainParameterId;
    fader.points = {
        AutomationBreakpoint { 0, 1.0, AutomationCurveType::Linear },
        AutomationBreakpoint { 64, 0.25, AutomationCurveType::Linear },
        AutomationBreakpoint { 128, 0.75, AutomationCurveType::Hold },
    };

    AutomationLaneData pan;
    pan.id = idFromLowByte (44);
    pan.ownerEntity = track.id;
    pan.role = AutomationTargetRole::TrackPan;
    pan.paramId = PanNode::kPanParameterId;
    pan.points = {
        AutomationBreakpoint { 0, 0.0, AutomationCurveType::Linear },
        AutomationBreakpoint { 64, 1.0, AutomationCurveType::Linear },
        AutomationBreakpoint { 128, 0.5, AutomationCurveType::Hold },
    };

    fixture.project.id = idFromLowByte (45);
    fixture.project.sampleRate = asset.sampleRate;
    fixture.project.assets = { asset };
    fixture.project.tracks = { track };
    fixture.project.clips = { clip };
    fixture.project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    fixture.project.automationLanes = { fader, pan };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());
    REQUIRE (fixture.project.automationTargetsReferenceProjectRows());

    fixture.decodedAssets = {
        DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
    };
    return fixture;
}

const std::vector<float>& samplesForAsset (const PlaybackFixture& fixture, EntityId assetId)
{
    for (std::size_t i = 0; i < fixture.project.assets.size(); ++i)
        if (fixture.project.assets[i].id == assetId)
            return fixture.samples[i];

    FAIL ("missing fixture samples for asset");
    static const std::vector<float> empty;
    return empty;
}

float independentEqualPowerGain (double x) noexcept
{
    constexpr double halfPi = 1.57079632679489661923;
    return static_cast<float> (std::sin (halfPi * std::clamp (x, 0.0, 1.0)));
}

// The canonical equal-power fade DecodedClipNode applies, anchored to the source-frame count.
// This stays independent of the engine helper so playback is still checked against a hand reference.
float equalPowerFade (const Clip& clip, Tick local, std::uint64_t total) noexcept
{
    float fadeIn = 1.0f;
    if (clip.fadeIn > 0 && local < clip.fadeIn)
        fadeIn = independentEqualPowerGain (static_cast<double> (local) / static_cast<double> (clip.fadeIn));

    float fadeOut = 1.0f;
    if (clip.fadeOut > 0)
    {
        const Tick fadeStart = static_cast<Tick> (total) - clip.fadeOut;
        if (local >= fadeStart)
        {
            const double progress = static_cast<double> (local - fadeStart)
                                  / static_cast<double> (clip.fadeOut);
            fadeOut = independentEqualPowerGain (1.0 - progress);
        }
    }
    return std::min (fadeIn, fadeOut);
}

std::vector<float> independentReference (const PlaybackFixture& fixture)
{
    std::uint64_t frames = 0;
    for (const Clip& clip : fixture.project.clips)
        frames = std::max<std::uint64_t> (frames, static_cast<std::uint64_t> (clip.timelineStart + clip.timelineLength));

    std::vector<float> expected (static_cast<std::size_t> (frames) * 2u, 0.0f);
    for (const Clip& clip : fixture.project.clips)
    {
        const std::vector<float>& samples = samplesForAsset (fixture, clip.assetId);
        const std::uint64_t sourceFrames = std::min<std::uint64_t> (clip.srcLen, static_cast<std::uint64_t> (clip.timelineLength));
        for (std::uint64_t local = 0; local < sourceFrames; ++local)
        {
            const float source = samples[static_cast<std::size_t> (clip.srcOffset + local)];
            const float value = source
                              * equalPowerFade (clip, static_cast<Tick> (local), sourceFrames)
                              * clip.gain
                              * kCenterGain;
            const std::size_t frame = static_cast<std::size_t> (clip.timelineStart + static_cast<Tick> (local));
            expected[frame * 2u] += value;
            expected[frame * 2u + 1u] += value;
        }
    }
    return expected;
}

std::vector<float> referenceSlice (const PlaybackFixture& fixture, std::int64_t startFrame, std::uint64_t frames)
{
    const std::vector<float> full = independentReference (fixture);
    const std::size_t fullFrames = full.size() / 2u;
    std::vector<float> out (static_cast<std::size_t> (frames) * 2u, 0.0f);
    for (std::uint64_t frame = 0; frame < frames; ++frame)
    {
        const std::int64_t sourceFrame = startFrame + static_cast<std::int64_t> (frame);
        if (sourceFrame < 0 || static_cast<std::size_t> (sourceFrame) >= fullFrames)
            continue;

        out[static_cast<std::size_t> (frame) * 2u] = full[static_cast<std::size_t> (sourceFrame) * 2u];
        out[static_cast<std::size_t> (frame) * 2u + 1u] = full[static_cast<std::size_t> (sourceFrame) * 2u + 1u];
    }
    return out;
}

std::vector<float> loopReference (const PlaybackFixture& fixture,
                                  std::int64_t loopStart,
                                  std::int64_t loopEnd,
                                  std::uint64_t frames)
{
    REQUIRE (loopStart >= 0);
    REQUIRE (loopEnd > loopStart);

    const std::vector<float> full = independentReference (fixture);
    const std::size_t fullFrames = full.size() / 2u;
    const std::int64_t loopLength = loopEnd - loopStart;
    std::vector<float> out (static_cast<std::size_t> (frames) * 2u, 0.0f);
    for (std::uint64_t frame = 0; frame < frames; ++frame)
    {
        const std::int64_t sourceFrame = loopStart + (static_cast<std::int64_t> (frame) % loopLength);
        if (static_cast<std::size_t> (sourceFrame) >= fullFrames)
            continue;

        out[static_cast<std::size_t> (frame) * 2u] = full[static_cast<std::size_t> (sourceFrame) * 2u];
        out[static_cast<std::size_t> (frame) * 2u + 1u] = full[static_cast<std::size_t> (sourceFrame) * 2u + 1u];
    }
    return out;
}

bool buffersNear (std::span<const float> a, std::span<const float> b, double tol = kTolerance) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::fabs (static_cast<double> (a[i] - b[i])) > tol)
            return false;
    return true;
}

bool bitIdentical (std::span<const float> a, std::span<const float> b) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        std::uint32_t ba = 0, bb = 0;
        std::memcpy (&ba, &a[i], sizeof (ba));
        std::memcpy (&bb, &b[i], sizeof (bb));
        if (ba != bb)
            return false;
    }
    return true;
}

std::vector<float> drainPlayback (PlaybackEngine& engine, std::uint64_t frames, int blockSize)
{
    const int ch = static_cast<int> (engine.channels());
    REQUIRE (ch == 2);

    std::vector<float> out (static_cast<std::size_t> (frames) * static_cast<std::size_t> (ch), 0.0f);
    std::vector<float> storage (static_cast<std::size_t> (ch) * static_cast<std::size_t> (blockSize), 0.0f);
    std::vector<float*> ptrs (static_cast<std::size_t> (ch), nullptr);

    std::uint64_t offset = 0;
    while (offset < frames)
    {
        const int n = static_cast<int> (std::min<std::uint64_t> (frames - offset, static_cast<std::uint64_t> (blockSize)));
        for (int c = 0; c < ch; ++c)
            ptrs[static_cast<std::size_t> (c)] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (blockSize);

        engine.processBlock (ptrs.data(), ch, n);

        for (int frame = 0; frame < n; ++frame)
            for (int c = 0; c < ch; ++c)
                out[(static_cast<std::size_t> (offset) + static_cast<std::size_t> (frame)) * static_cast<std::size_t> (ch)
                    + static_cast<std::size_t> (c)] = ptrs[static_cast<std::size_t> (c)][frame];

        offset += static_cast<std::uint64_t> (n);
    }

    return out;
}

// Pump a fresh PlaybackEngine block by block through the realtime device-callback seam, collecting the
// interleaved Master output for the whole timeline.
std::vector<float> playToBuffer (const PlaybackFixture& fixture, int blockSize)
{
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        OfflineRenderOptions {});
    REQUIRE (created.ok());
    PlaybackEngine& engine = *created.engine;

    const std::uint64_t total = engine.frames();
    std::vector<float> out = drainPlayback (engine, total, blockSize);

    engine.reclaim();
    return out;
}

std::filesystem::path tempPath (std::string_view label, std::string_view extension)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
                               / ("yesdaw-h8-" + std::string (label) + "-" + std::to_string (ticks)
                                  + std::string (extension));

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

Project makeAutosaveProject()
{
    Project project;
    project.id = idFromLowByte (90);
    project.sampleRate = SampleRate { 48000.0 };
    return project;
}

} // namespace

TEST_CASE ("PlaybackEngine plays a Project through the realtime Runtime, matching an independent reference",
           "[h8][playback][runtime]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();
    {
        const PlaybackFixture fixture = makePlaybackFixture();
        const std::vector<float> played = playToBuffer (fixture, 4);   // multi-block

        REQUIRE (played.size() == static_cast<std::size_t> (9u * 2u));
        REQUIRE (buffersNear (played, independentReference (fixture)));
    }
    REQUIRE (CompiledGraph::aliveCount() == base);   // the Runtime path frees the graph on teardown
}

TEST_CASE ("PlaybackEngine output is identical at every device block size (ADR-0008)",
           "[h8][playback][block-size]")
{
    const PlaybackFixture fixture = makePlaybackFixture();
    const std::vector<float> reference = playToBuffer (fixture, 128);

    for (const int blockSize : { 1, 2, 3, 4, 5, 7, 8, 9, 64 })
    {
        const std::vector<float> played = playToBuffer (fixture, blockSize);
        REQUIRE (bitIdentical (played, reference));
    }
}

TEST_CASE ("PlaybackEngine output matches the offline render of the same Project",
           "[h8][playback][offline-parity]")
{
    const PlaybackFixture fixture = makePlaybackFixture();
    const std::vector<float> played = playToBuffer (fixture, 5);

    const auto offline = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()));
    REQUIRE (offline.ok());

    // The realtime publish/drain/install path must reproduce the offline render bit-for-bit.
    REQUIRE (bitIdentical (played, offline.interleavedSamples));
}

TEST_CASE ("PlaybackEngine automated fader and pan output matches offline render",
           "[h15][automation][cp4][offline-parity]")
{
    const PlaybackFixture fixture = makeAutomatedFaderPanFixture();

    PlaybackFixture withoutAutomation = fixture;
    withoutAutomation.project.automationLanes.clear();
    const auto staticOffline = renderOfflineProject (
        withoutAutomation.project,
        std::span<const DecodedAssetAudio> (withoutAutomation.decodedAssets.data(), withoutAutomation.decodedAssets.size()));
    REQUIRE (staticOffline.ok());

    const auto automatedOffline = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()));
    REQUIRE (automatedOffline.ok());
    REQUIRE_FALSE (bitIdentical (automatedOffline.interleavedSamples, staticOffline.interleavedSamples));

    const std::vector<float> played = playToBuffer (fixture, 7);
    REQUIRE (bitIdentical (played, automatedOffline.interleavedSamples));
}

TEST_CASE ("PlaybackEngine locate(N) reproduces the offline render slice bit-for-bit",
           "[h8][playback][offline-parity][transport]")
{
    const PlaybackFixture fixture = makePlaybackFixture();
    const auto offline = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()));
    REQUIRE (offline.ok());
    const std::int64_t totalFrames = static_cast<std::int64_t> (offline.interleavedSamples.size() / 2u);

    // The transport branch (hasTimelineFrame=true) and the offline no-transport branch must produce the
    // SAME absolute frames. Locate to several non-zero starts (clip boundary, mid-overlap, fade tail) and
    // demand the played output is bit-identical to the offline render's matching slice — the real renderer,
    // not the hand-rolled reference. This pins the ADR-0022 "same graph, same absolute frames" claim.
    for (const std::int64_t startFrame : { std::int64_t { 0 }, std::int64_t { 2 }, std::int64_t { 5 }, std::int64_t { 6 } })
    {
        for (const int blockSize : { 1, 3, 4, 8 })
        {
            PlaybackEngine::Result created = PlaybackEngine::create (
                fixture.project,
                std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
                OfflineRenderOptions {});
            REQUIRE (created.ok());
            REQUIRE (created.engine->locate (startFrame));

            const std::uint64_t frames = static_cast<std::uint64_t> (totalFrames - startFrame);
            const std::vector<float> played = drainPlayback (*created.engine, frames, blockSize);

            std::vector<float> offlineSlice (played.size(), 0.0f);
            for (std::size_t i = 0; i < played.size(); ++i)
                offlineSlice[i] = offline.interleavedSamples[static_cast<std::size_t> (startFrame) * 2u + i];

            REQUIRE (bitIdentical (played, offlineSlice));
        }
    }
}

TEST_CASE ("PlaybackEngine looped output is identical at every device block size (ADR-0008)",
           "[h8][playback][block-size][transport]")
{
    const PlaybackFixture fixture = makePlaybackFixture();

    // Drive a loop region [3,7) from frame 3 for 32 frames at a range of block sizes, including ones equal
    // to and double the loop length (forcing in-block and multi-wrap). Every block size must produce the
    // SAME samples as the independent loop reference — so a wrap bug that only shows at blockSize 1 (or only
    // when a block spans multiple wraps) bites instead of sliding through on the single size the gate used.
    auto loopAt = [&] (int blockSize)
    {
        PlaybackEngine::Result created = PlaybackEngine::create (
            fixture.project,
            std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
            OfflineRenderOptions {});
        REQUIRE (created.ok());
        REQUIRE (created.engine->setLoop (3, 7));
        REQUIRE (created.engine->locate (3));
        return drainPlayback (*created.engine, 32, blockSize);
    };

    const std::vector<float> reference = loopAt (128);
    REQUIRE (buffersNear (reference, loopReference (fixture, 3, 7, 32)));
    for (const int blockSize : { 1, 2, 3, 4, 5, 7, 8, 16, 64 })
        REQUIRE (bitIdentical (loopAt (blockSize), reference));
}

TEST_CASE ("PlaybackEngine transport stop, locate, and loop are sample-accurate",
           "[h8][playback][transport]")
{
    const PlaybackFixture fixture = makePlaybackFixture();
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        OfflineRenderOptions {});
    REQUIRE (created.ok());
    PlaybackEngine& engine = *created.engine;

    REQUIRE (engine.locate (3));
    REQUIRE (buffersNear (drainPlayback (engine, 4, 3), referenceSlice (fixture, 3, 4)));
    REQUIRE (engine.playheadFrame() == 7);

    REQUIRE (engine.locate (2));
    engine.stop();
    std::vector<float> left (4u, -1.0f);
    std::vector<float> right (4u, -1.0f);
    float* stopped[2] = { left.data(), right.data() };
    engine.processBlock (stopped, 2, 4);
    REQUIRE (engine.playheadFrame() == 2);
    for (float sample : left)
        REQUIRE (sample == 0.0f);
    for (float sample : right)
        REQUIRE (sample == 0.0f);

    engine.play();
    REQUIRE (buffersNear (drainPlayback (engine, 2, 2), referenceSlice (fixture, 2, 2)));
    REQUIRE (engine.playheadFrame() == 4);

    REQUIRE (engine.setLoop (3, 7));
    REQUIRE (engine.locate (3));
    REQUIRE (buffersNear (drainPlayback (engine, 10, 5), loopReference (fixture, 3, 7, 10)));
    REQUIRE (engine.playheadFrame() == 5);
}

TEST_CASE ("PlaybackEngine transport rejects out-of-range positions and survives a loop wider than INT_MAX",
           "[h8][playback][transport]")
{
    const PlaybackFixture fixture = makePlaybackFixture();
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        OfflineRenderOptions {});
    REQUIRE (created.ok());
    PlaybackEngine& engine = *created.engine;

    // Out-of-range transport positions are rejected at the control-side API, before any audio-thread math.
    REQUIRE_FALSE (engine.locate (-1));
    REQUIRE_FALSE (engine.locate (std::numeric_limits<std::int64_t>::max()));
    REQUIRE_FALSE (engine.setLoop (0, std::numeric_limits<std::int64_t>::max()));
    REQUIRE_FALSE (engine.setLoop (5, 5));
    REQUIRE_FALSE (engine.setLoop (-1, 4));

    // A valid loop region far wider than INT_MAX must NOT truncate to a zero/negative segment and hang or
    // trap the audio thread. The region is longer than the block, so the first block plays straight from the
    // located frame with no wrap, matching the reference slice. (Pre-fix this aborted in CompiledGraph.)
    REQUIRE (engine.setLoop (0, std::int64_t { 1 } << 40));
    REQUIRE (engine.locate (2));
    REQUIRE (buffersNear (drainPlayback (engine, 4, 4), referenceSlice (fixture, 2, 4)));
    REQUIRE (engine.playheadFrame() == 6);
}

TEST_CASE ("PlaybackEngine drives H5 recording capture from the transport playhead",
           "[h8][playback][recording]")
{
    const PlaybackFixture fixture = makePlaybackFixture();
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        OfflineRenderOptions {});
    REQUIRE (created.ok());
    PlaybackEngine& engine = *created.engine;
    REQUIRE (engine.locate (0));

    RecordingConfig config;
    config.channels = 1;
    config.sampleRateHz = 48000.0;
    config.latency.inputLatencyFrames = 3;
    config.latency.outputLatencyFrames = 5;
    config.latency.includeOutputLatency = true;
    config.window.punchStartFrame = 0;
    config.window.punchEndFrame = 64;

    std::int64_t roundTrip = 0;
    REQUIRE (config.latency.compensatedLatencyFrames (roundTrip));
    constexpr std::int64_t kClickFrame = 28;
    const std::int64_t impulseFrame = kClickFrame + roundTrip;

    const std::filesystem::path path = tempPath ("recording", ".ysdtake");
    RecordingChunkFifo fifo { 8 };
    RecordingTakeFileWriter writer;
    REQUIRE (writer.open (path, config));

    constexpr int kBlock = 7;
    const std::int64_t totalFrames = config.window.punchEndFrame + roundTrip + kBlock;
    for (std::int64_t processed = 0; processed < totalFrames; processed += kBlock)
    {
        // Independently witness the transport mapping: the TEST owns `processed` (the absolute device frame)
        // and places the impulse in those coordinates, then asserts the engine's playhead tracks it. A
        // stuck/off-by-block playhead fails HERE rather than being masked by reading playheadFrame() on both
        // sides of the same equation.
        REQUIRE (engine.playheadFrame() == processed);
        const int frames = static_cast<int> (std::min<std::int64_t> (kBlock, totalFrames - processed));
        std::vector<float> input (static_cast<std::size_t> (frames), 0.0f);
        if (impulseFrame >= processed && impulseFrame < processed + frames)
            input[static_cast<std::size_t> (impulseFrame - processed)] = 1.0f;

        const float* inputChannels[1] = { input.data() };
        const auto capture = engine.captureRecordingInputBlock (fifo, config, inputChannels, 1, frames);
        REQUIRE_FALSE (capture.inputInvalid);
        REQUIRE_FALSE (capture.fifoFull);
        REQUIRE (writer.drain (fifo));

        std::vector<float> left (static_cast<std::size_t> (frames), 0.0f);
        std::vector<float> right (static_cast<std::size_t> (frames), 0.0f);
        float* outputs[2] = { left.data(), right.data() };
        engine.processBlock (outputs, 2, frames);
    }

    REQUIRE (writer.drain (fifo));
    REQUIRE (writer.close());

    const auto read = readRecordingTakeFile (path);
    REQUIRE (read.status == RecordingTakeFileStatus::Ok);
    float sample = 0.0f;
    REQUIRE (findRecordedSample (read.file, 0, kClickFrame, 0, sample));
    REQUIRE (sample == 1.0f);
    std::filesystem::remove (path);
}

TEST_CASE ("Playback autosave tick writes and recovers the last dirty Project",
           "[h8][playback][autosave]")
{
    const PlaybackFixture fixture = makePlaybackFixture();
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        OfflineRenderOptions {});
    REQUIRE (created.ok());
    PlaybackEngine& engine = *created.engine;

    const std::filesystem::path path = tempPath ("autosave", ".yesdaw");
    ProjectBundleDb db;
    Project saved = makeAutosaveProject();
    Project dirty = saved;
    dirty.sampleRate = SampleRate { 96000.0 };

    REQUIRE (ProjectBundleDb::openOrCreateBundle (path, db).ok());
    REQUIRE (db.writeProjectSnapshot (saved).ok());

    // Negative control: a CLEAN engine must SKIP the write — the needsAutosave() guard is the whole point
    // of the tick. Without this, deleting that guard would still pass the suite. Prove a no-op tick leaves
    // no autosave snapshot behind.
    REQUIRE_FALSE (engine.needsAutosave());
    REQUIRE (writeAutosaveFromControlTick (engine, db, dirty).ok());
    {
        Project none;
        REQUIRE_FALSE (readAutosaveSnapshot (path, none).ok());
    }

    engine.markProjectEdited();
    REQUIRE (engine.needsAutosave());
    REQUIRE (writeAutosaveFromControlTick (engine, db, dirty).ok());
    REQUIRE_FALSE (engine.needsAutosave());

    Project recovered;
    REQUIRE (readAutosaveSnapshot (path, recovered).ok());
    REQUIRE (recovered.id == dirty.id);
    REQUIRE (recovered.sampleRate == dirty.sampleRate);
    REQUIRE (recovered.assets.empty());
    REQUIRE (recovered.clips.empty());
}
