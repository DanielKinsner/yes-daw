// YES DAW - H8 playback gate: play a Project through the realtime Runtime and prove it equals an
// INDEPENDENT reference (Clips summed at their timeline positions) -- not the engine compared to itself.

#include "engine/LoudnessTap.h"
#include "engine/OfflineRenderer.h"
#include "engine/PlaybackEngine.h"
#include "engine/Reliability.h"
#include "engine/nodes/CompressorNode.h"
#include "engine/nodes/EqNode.h"
#include "engine/nodes/FxDelayNode.h"
#include "engine/nodes/LimiterNode.h"
#include "engine/nodes/ReverbNode.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::AutomationBreakpoint;
using yesdaw::engine::AutomationCurveType;
using yesdaw::engine::AutomationLaneData;
using yesdaw::engine::AutomationTargetRole;
using yesdaw::engine::Bus;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompressorNode;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::EntityId;
using yesdaw::engine::EqNode;
using yesdaw::engine::FaderNode;
using yesdaw::engine::FxDelayNode;
using yesdaw::engine::FxInsert;
using yesdaw::engine::FxKind;
using yesdaw::engine::LimiterNode;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::PanNode;
using yesdaw::engine::PlaybackEngine;
using yesdaw::engine::Project;
using yesdaw::engine::ProjectMixerSendRoute;
using yesdaw::engine::RecordingChunkFifo;
using yesdaw::engine::RecordingConfig;
using yesdaw::engine::RecordingTakeFileStatus;
using yesdaw::engine::RecordingTakeFileWriter;
using yesdaw::engine::ReverbNode;
using yesdaw::engine::findRecordedSample;
using yesdaw::engine::readRecordingTakeFile;
using yesdaw::engine::renderOfflineProject;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::engine::MixerSendTap;
using yesdaw::engine::unmapToNormalized;

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

PlaybackFixture makeAutomatedSendRideFixture()
{
    PlaybackFixture fixture;
    fixture.samples = { {} };
    fixture.samples[0].reserve (128u);
    for (int i = 0; i < 128; ++i)
        fixture.samples[0].push_back (i % 3 == 0 ? 0.40f : -0.20f);

    Asset asset;
    asset.id = idFromLowByte (50);
    asset.contentHash = hashWithSeed (50);
    asset.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    asset.sampleRate = SampleRate { 30720.0 }; // 120 BPM makes one tick equal one frame.
    asset.channels = 1;

    Track track;
    track.id = idFromLowByte (51);
    track.strip.name = "Send ride source";
    track.strip.linearGain = 0.0f; // Direct path silent; the return proves the SendLevel lane.
    track.strip.pan = 0.0f;

    Bus bus;
    bus.id = idFromLowByte (52);
    bus.strip.name = "Automation return";
    bus.strip.linearGain = 1.0f;
    bus.strip.pan = 0.0f;

    Clip clip;
    clip.id = idFromLowByte (53);
    clip.assetId = asset.id;
    clip.trackId = track.id;
    clip.timelineStart = 0;
    clip.timelineLength = 128;
    clip.srcOffset = 0;
    clip.srcLen = 128;
    clip.gain = 1.0f;
    clip.timeBase = TimeBase::SampleLocked;

    AutomationLaneData send;
    send.id = idFromLowByte (54);
    send.ownerEntity = track.id;
    send.role = AutomationTargetRole::SendLevel;
    send.paramId = 0; // Send ordinal; projection rewrites this to FaderNode::kGainParameterId.
    send.points = {
        AutomationBreakpoint { 0, 0.0, AutomationCurveType::Linear },
        AutomationBreakpoint { 64, 1.0, AutomationCurveType::Linear },
        AutomationBreakpoint { 128, 0.5, AutomationCurveType::Hold },
    };

    fixture.project.id = idFromLowByte (55);
    fixture.project.sampleRate = asset.sampleRate;
    fixture.project.assets = { asset };
    fixture.project.tracks = { track };
    fixture.project.buses = { bus };
    fixture.project.clips = { clip };
    fixture.project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    fixture.project.automationLanes = { send };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());
    REQUIRE (fixture.project.automationTargetsReferenceProjectRows());

    fixture.decodedAssets = {
        DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
    };
    return fixture;
}

PlaybackFixture makeAutomatedEqFxFixture()
{
    PlaybackFixture fixture;
    constexpr int kFrames = 2048;
    fixture.samples = { {} };
    fixture.samples[0].reserve (static_cast<std::size_t> (kFrames));
    for (int i = 0; i < kFrames; ++i)
    {
        const double phase = 2.0 * 3.14159265358979323846 * 1000.0
                           * (static_cast<double> (i) / 30720.0);
        fixture.samples[0].push_back (static_cast<float> (0.20 * std::sin (phase)));
    }

    Asset asset;
    asset.id = idFromLowByte (60);
    asset.contentHash = hashWithSeed (60);
    asset.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    asset.sampleRate = SampleRate { 30720.0 }; // 120 BPM makes one tick equal one frame.
    asset.channels = 1;

    FxInsert eq;
    eq.id = idFromLowByte (61);
    eq.kind = FxKind::Eq;
    eq.enabled = true;

    Track track;
    track.id = idFromLowByte (62);
    track.strip.name = "Automated EQ FX";
    track.strip.linearGain = 1.0f;
    track.strip.pan = 0.0f;
    track.strip.fxChain = { eq };

    Clip clip;
    clip.id = idFromLowByte (63);
    clip.assetId = asset.id;
    clip.trackId = track.id;
    clip.timelineStart = 0;
    clip.timelineLength = kFrames;
    clip.srcOffset = 0;
    clip.srcLen = kFrames;
    clip.gain = 1.0f;
    clip.timeBase = TimeBase::SampleLocked;

    const auto gainParam = EqNode::parameterIdFor (0, EqNode::kGainParamOffset);
    const double boostedGain = unmapToNormalized (EqNode::parameterSpec (gainParam), 12.0);
    AutomationLaneData eqGain;
    eqGain.id = idFromLowByte (64);
    eqGain.ownerEntity = eq.id;
    eqGain.role = AutomationTargetRole::FxInsertParam;
    eqGain.paramId = gainParam;
    eqGain.points = {
        AutomationBreakpoint { 0, boostedGain, AutomationCurveType::Hold },
        AutomationBreakpoint { kFrames, boostedGain, AutomationCurveType::Hold },
    };

    fixture.project.id = idFromLowByte (65);
    fixture.project.sampleRate = asset.sampleRate;
    fixture.project.assets = { asset };
    fixture.project.tracks = { track };
    fixture.project.clips = { clip };
    fixture.project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    fixture.project.automationLanes = { eqGain };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());
    REQUIRE (fixture.project.automationTargetsReferenceProjectRows());

    fixture.decodedAssets = {
        DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
    };
    return fixture;
}

PlaybackFixture makeAutomatedCompressorFxFixture()
{
    PlaybackFixture fixture;
    constexpr int kFrames = 2048;
    fixture.samples = { std::vector<float> (static_cast<std::size_t> (kFrames), 0.50f) };

    Asset asset;
    asset.id = idFromLowByte (66);
    asset.contentHash = hashWithSeed (66);
    asset.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    asset.sampleRate = SampleRate { 30720.0 }; // 120 BPM makes one tick equal one frame.
    asset.channels = 1;

    FxInsert compressor;
    compressor.id = idFromLowByte (67);
    compressor.kind = FxKind::Compressor;
    compressor.enabled = true;
    compressor.normalizedParams = {
        { CompressorNode::kRatioParamId,
          unmapToNormalized (CompressorNode::parameterSpec (CompressorNode::kRatioParamId), 4.0) },
        { CompressorNode::kAttackParamId,
          unmapToNormalized (CompressorNode::parameterSpec (CompressorNode::kAttackParamId), 0.1) },
        { CompressorNode::kReleaseParamId,
          unmapToNormalized (CompressorNode::parameterSpec (CompressorNode::kReleaseParamId), 10.0) },
    };

    Track track;
    track.id = idFromLowByte (68);
    track.strip.name = "Automated compressor FX";
    track.strip.linearGain = 1.0f;
    track.strip.pan = 0.0f;
    track.strip.fxChain = { compressor };

    Clip clip;
    clip.id = idFromLowByte (69);
    clip.assetId = asset.id;
    clip.trackId = track.id;
    clip.timelineStart = 0;
    clip.timelineLength = kFrames;
    clip.srcOffset = 0;
    clip.srcLen = kFrames;
    clip.gain = 1.0f;
    clip.timeBase = TimeBase::SampleLocked;

    const double compressedThreshold =
        unmapToNormalized (CompressorNode::parameterSpec (CompressorNode::kThresholdParamId), -60.0);
    AutomationLaneData threshold;
    threshold.id = idFromLowByte (70);
    threshold.ownerEntity = compressor.id;
    threshold.role = AutomationTargetRole::FxInsertParam;
    threshold.paramId = CompressorNode::kThresholdParamId;
    threshold.points = {
        AutomationBreakpoint { 0, compressedThreshold, AutomationCurveType::Hold },
        AutomationBreakpoint { kFrames, compressedThreshold, AutomationCurveType::Hold },
    };

    fixture.project.id = idFromLowByte (71);
    fixture.project.sampleRate = asset.sampleRate;
    fixture.project.assets = { asset };
    fixture.project.tracks = { track };
    fixture.project.clips = { clip };
    fixture.project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    fixture.project.automationLanes = { threshold };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());
    REQUIRE (fixture.project.automationTargetsReferenceProjectRows());

    fixture.decodedAssets = {
        DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
    };
    return fixture;
}

PlaybackFixture makeAutomatedDelayFxFixture()
{
    PlaybackFixture fixture;
    constexpr int kFrames = 2048;
    fixture.samples = { std::vector<float> (static_cast<std::size_t> (kFrames), 0.0f) };
    fixture.samples[0][0] = 0.75f;

    Asset asset;
    asset.id = idFromLowByte (72);
    asset.contentHash = hashWithSeed (72);
    asset.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    asset.sampleRate = SampleRate { 30720.0 }; // 120 BPM makes one tick equal one frame.
    asset.channels = 1;

    FxInsert delay;
    delay.id = idFromLowByte (73);
    delay.kind = FxKind::Delay;
    delay.enabled = true;
    delay.normalizedParams = {
        { FxDelayNode::kTimeLeftParamId,
          unmapToNormalized (FxDelayNode::parameterSpec (FxDelayNode::kTimeLeftParamId), 1.0) },
        { FxDelayNode::kTimeRightParamId,
          unmapToNormalized (FxDelayNode::parameterSpec (FxDelayNode::kTimeRightParamId), 1.0) },
        { FxDelayNode::kFeedbackParamId,
          unmapToNormalized (FxDelayNode::parameterSpec (FxDelayNode::kFeedbackParamId), 0.0) },
        { FxDelayNode::kDampingParamId,
          unmapToNormalized (FxDelayNode::parameterSpec (FxDelayNode::kDampingParamId), FxDelayNode::kMaxDampingHz) },
    };

    Track track;
    track.id = idFromLowByte (74);
    track.strip.name = "Automated delay FX";
    track.strip.linearGain = 1.0f;
    track.strip.pan = 0.0f;
    track.strip.fxChain = { delay };

    Clip clip;
    clip.id = idFromLowByte (75);
    clip.assetId = asset.id;
    clip.trackId = track.id;
    clip.timelineStart = 0;
    clip.timelineLength = kFrames;
    clip.srcOffset = 0;
    clip.srcLen = kFrames;
    clip.gain = 1.0f;
    clip.timeBase = TimeBase::SampleLocked;

    const double wetMix = unmapToNormalized (FxDelayNode::parameterSpec (FxDelayNode::kMixParamId), 1.0);
    AutomationLaneData mix;
    mix.id = idFromLowByte (76);
    mix.ownerEntity = delay.id;
    mix.role = AutomationTargetRole::FxInsertParam;
    mix.paramId = FxDelayNode::kMixParamId;
    mix.points = {
        AutomationBreakpoint { 0, wetMix, AutomationCurveType::Hold },
        AutomationBreakpoint { kFrames, wetMix, AutomationCurveType::Hold },
    };

    fixture.project.id = idFromLowByte (77);
    fixture.project.sampleRate = asset.sampleRate;
    fixture.project.assets = { asset };
    fixture.project.tracks = { track };
    fixture.project.clips = { clip };
    fixture.project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    fixture.project.automationLanes = { mix };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());
    REQUIRE (fixture.project.automationTargetsReferenceProjectRows());

    fixture.decodedAssets = {
        DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
    };
    return fixture;
}

PlaybackFixture makeAutomatedReverbFxFixture()
{
    PlaybackFixture fixture;
    constexpr int kFrames = 2048;
    fixture.samples = { std::vector<float> (static_cast<std::size_t> (kFrames), 0.0f) };
    fixture.samples[0][0] = 0.75f;

    Asset asset;
    asset.id = idFromLowByte (78);
    asset.contentHash = hashWithSeed (78);
    asset.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    asset.sampleRate = SampleRate { 30720.0 }; // 120 BPM makes one tick equal one frame.
    asset.channels = 1;

    FxInsert reverb;
    reverb.id = idFromLowByte (79);
    reverb.kind = FxKind::Reverb;
    reverb.enabled = true;
    reverb.normalizedParams = {
        { ReverbNode::kPreDelayParamId,
          unmapToNormalized (ReverbNode::parameterSpec (ReverbNode::kPreDelayParamId), 0.0) },
        { ReverbNode::kRt60ParamId,
          unmapToNormalized (ReverbNode::parameterSpec (ReverbNode::kRt60ParamId), 0.25) },
        { ReverbNode::kSizeParamId,
          unmapToNormalized (ReverbNode::parameterSpec (ReverbNode::kSizeParamId), 0.5) },
        { ReverbNode::kDampingParamId,
          unmapToNormalized (ReverbNode::parameterSpec (ReverbNode::kDampingParamId), ReverbNode::kMaxDampingHz) },
    };

    Track track;
    track.id = idFromLowByte (80);
    track.strip.name = "Automated reverb FX";
    track.strip.linearGain = 1.0f;
    track.strip.pan = 0.0f;
    track.strip.fxChain = { reverb };

    Clip clip;
    clip.id = idFromLowByte (81);
    clip.assetId = asset.id;
    clip.trackId = track.id;
    clip.timelineStart = 0;
    clip.timelineLength = kFrames;
    clip.srcOffset = 0;
    clip.srcLen = kFrames;
    clip.gain = 1.0f;
    clip.timeBase = TimeBase::SampleLocked;

    const double wetMix = unmapToNormalized (ReverbNode::parameterSpec (ReverbNode::kMixParamId), 1.0);
    AutomationLaneData mix;
    mix.id = idFromLowByte (82);
    mix.ownerEntity = reverb.id;
    mix.role = AutomationTargetRole::FxInsertParam;
    mix.paramId = ReverbNode::kMixParamId;
    mix.points = {
        AutomationBreakpoint { 0, wetMix, AutomationCurveType::Hold },
        AutomationBreakpoint { kFrames, wetMix, AutomationCurveType::Hold },
    };

    fixture.project.id = idFromLowByte (83);
    fixture.project.sampleRate = asset.sampleRate;
    fixture.project.assets = { asset };
    fixture.project.tracks = { track };
    fixture.project.clips = { clip };
    fixture.project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    fixture.project.automationLanes = { mix };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());
    REQUIRE (fixture.project.automationTargetsReferenceProjectRows());

    fixture.decodedAssets = {
        DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
    };
    return fixture;
}

PlaybackFixture makeAutomatedLimiterFxFixture()
{
    PlaybackFixture fixture;
    constexpr int kFrames = 2048;
    fixture.samples = { std::vector<float> (static_cast<std::size_t> (kFrames), 1.0f) };

    Asset asset;
    asset.id = idFromLowByte (84);
    asset.contentHash = hashWithSeed (84);
    asset.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    asset.sampleRate = SampleRate { 30720.0 }; // 120 BPM makes one tick equal one frame.
    asset.channels = 1;

    FxInsert limiter;
    limiter.id = idFromLowByte (85);
    limiter.kind = FxKind::Limiter;
    limiter.enabled = true;
    limiter.normalizedParams = {
        { LimiterNode::kReleaseParamId,
          unmapToNormalized (LimiterNode::parameterSpec (LimiterNode::kReleaseParamId), 50.0) },
        { LimiterNode::kLookaheadParamId,
          unmapToNormalized (LimiterNode::parameterSpec (LimiterNode::kLookaheadParamId), 1.0) },
    };

    Track track;
    track.id = idFromLowByte (86);
    track.strip.name = "Automated limiter FX";
    track.strip.linearGain = 1.0f;
    track.strip.pan = 0.0f;
    track.strip.fxChain = { limiter };

    Clip clip;
    clip.id = idFromLowByte (87);
    clip.assetId = asset.id;
    clip.trackId = track.id;
    clip.timelineStart = 0;
    clip.timelineLength = kFrames;
    clip.srcOffset = 0;
    clip.srcLen = kFrames;
    clip.gain = 1.0f;
    clip.timeBase = TimeBase::SampleLocked;

    const double ceiling = unmapToNormalized (LimiterNode::parameterSpec (LimiterNode::kCeilingParamId), -12.0);
    AutomationLaneData limiterCeiling;
    limiterCeiling.id = idFromLowByte (88);
    limiterCeiling.ownerEntity = limiter.id;
    limiterCeiling.role = AutomationTargetRole::FxInsertParam;
    limiterCeiling.paramId = LimiterNode::kCeilingParamId;
    limiterCeiling.points = {
        AutomationBreakpoint { 0, ceiling, AutomationCurveType::Hold },
        AutomationBreakpoint { kFrames, ceiling, AutomationCurveType::Hold },
    };

    fixture.project.id = idFromLowByte (89);
    fixture.project.sampleRate = asset.sampleRate;
    fixture.project.assets = { asset };
    fixture.project.tracks = { track };
    fixture.project.clips = { clip };
    fixture.project.tempoMap = { TempoChange { 0, 120.0, TempoCurve::Jump } };
    fixture.project.automationLanes = { limiterCeiling };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());
    REQUIRE (fixture.project.automationTargetsReferenceProjectRows());

    fixture.decodedAssets = {
        DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
    };
    return fixture;
}

OfflineRenderOptions sendRideOptions (const PlaybackFixture& fixture)
{
    OfflineRenderOptions options;
    REQUIRE (fixture.project.tracks.size() == 1u);
    REQUIRE (fixture.project.buses.size() == 1u);
    options.sendRoutes = {
        ProjectMixerSendRoute { fixture.project.tracks[0].id, fixture.project.buses[0].id, MixerSendTap::PreFader, 0.0f },
    };
    return options;
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
std::vector<float> playToBuffer (const PlaybackFixture& fixture, int blockSize, OfflineRenderOptions options = {})
{
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        std::move (options));
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

TEST_CASE ("PlaybackEngine automated send ride output matches offline render",
           "[h15][automation][cp4][offline-parity][send]")
{
    const PlaybackFixture fixture = makeAutomatedSendRideFixture();
    const OfflineRenderOptions options = sendRideOptions (fixture);

    PlaybackFixture withoutAutomation = fixture;
    withoutAutomation.project.automationLanes.clear();
    const auto staticOffline = renderOfflineProject (
        withoutAutomation.project,
        std::span<const DecodedAssetAudio> (withoutAutomation.decodedAssets.data(), withoutAutomation.decodedAssets.size()),
        options);
    REQUIRE (staticOffline.ok());
    for (const float sample : staticOffline.interleavedSamples)
        REQUIRE (sample == 0.0f);

    const auto automatedOffline = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        options);
    REQUIRE (automatedOffline.ok());
    REQUIRE_FALSE (bitIdentical (automatedOffline.interleavedSamples, staticOffline.interleavedSamples));

    for (const int blockSize : { 1, 7, 64 })
    {
        const std::vector<float> played = playToBuffer (fixture, blockSize, options);
        REQUIRE (bitIdentical (played, automatedOffline.interleavedSamples));
    }
}

TEST_CASE ("PlaybackEngine automated EQ FX parameter output matches offline render",
           "[h15][automation][cp4][offline-parity][fx][eq]")
{
    const PlaybackFixture fixture = makeAutomatedEqFxFixture();

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

    for (const int blockSize : { 1, 7, 64 })
    {
        const std::vector<float> played = playToBuffer (fixture, blockSize);
        REQUIRE (bitIdentical (played, automatedOffline.interleavedSamples));
    }
}

TEST_CASE ("PlaybackEngine automated Compressor FX parameter output matches offline render",
           "[h15][automation][cp4][offline-parity][fx][compressor]")
{
    const PlaybackFixture fixture = makeAutomatedCompressorFxFixture();

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

    for (const int blockSize : { 1, 7, 64 })
    {
        const std::vector<float> played = playToBuffer (fixture, blockSize);
        REQUIRE (bitIdentical (played, automatedOffline.interleavedSamples));
    }
}

TEST_CASE ("PlaybackEngine automated Delay FX parameter output matches offline render",
           "[h15][automation][cp4][offline-parity][fx][delay]")
{
    const PlaybackFixture fixture = makeAutomatedDelayFxFixture();

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

    for (const int blockSize : { 1, 7, 64 })
    {
        const std::vector<float> played = playToBuffer (fixture, blockSize);
        REQUIRE (bitIdentical (played, automatedOffline.interleavedSamples));
    }
}

TEST_CASE ("PlaybackEngine automated Reverb FX parameter output matches offline render",
           "[h15][automation][cp4][offline-parity][fx][reverb]")
{
    const PlaybackFixture fixture = makeAutomatedReverbFxFixture();

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

    for (const int blockSize : { 1, 7, 64 })
    {
        const std::vector<float> played = playToBuffer (fixture, blockSize);
        REQUIRE (bitIdentical (played, automatedOffline.interleavedSamples));
    }
}

TEST_CASE ("PlaybackEngine automated Limiter FX parameter output matches offline render",
           "[h15][automation][cp4][offline-parity][fx][limiter]")
{
    const PlaybackFixture fixture = makeAutomatedLimiterFxFixture();

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

    for (const int blockSize : { 1, 7, 64 })
    {
        const std::vector<float> played = playToBuffer (fixture, blockSize);
        REQUIRE (bitIdentical (played, automatedOffline.interleavedSamples));
    }
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

// G0.5 — the live placement lane at the engine boundary (ADR-0046 §6; plan §5.3 lane 2). A
// Track's new ClipSchedule, built by the SAME resolver a rebuild uses and published through the
// ordered command queue onto a RUNNING engine, renders bit-identically to an engine freshly built
// from the edited Project; the edit is audible (the render differs from the unedited one); and
// the previous schedule is retired to the janitor, never freed under the audio thread.
TEST_CASE ("PlaybackEngine live ClipSchedule publish renders bit-identically to a rebuild",
           "[g0][playback][live-placement][clip-schedule]")
{
    using yesdaw::engine::buildTrackClipSchedule;
    using yesdaw::engine::ClipSchedule;
    using yesdaw::engine::OfflineRenderStatus;
    using yesdaw::engine::ProjectMixerNodeRole;
    using yesdaw::engine::projectMixerNodeIdForTrack;

    const PlaybackFixture fixture = makePlaybackFixture();
    REQUIRE_FALSE (fixture.project.clips.empty());
    const std::span<const DecodedAssetAudio> views (fixture.decodedAssets.data(), fixture.decodedAssets.size());

    // The edited Project: the first Clip moved later by three frames, quieter, with a short fade in.
    Project edited = fixture.project;
    edited.clips.front().timelineStart += 3;
    edited.clips.front().gain *= 0.5f;
    edited.clips.front().fadeIn = 2;
    REQUIRE (edited.hasValidAssetClipIndirection());
    const EntityId trackId = edited.clips.front().trackId;

    const auto offline = renderOfflineProject (edited, views);
    REQUIRE (offline.ok());
    const std::uint64_t totalFrames = offline.interleavedSamples.size() / 2u;

    // Live path: an engine running the ORIGINAL project takes the edited Track's schedule.
    PlaybackEngine::Result live = PlaybackEngine::create (fixture.project, views, OfflineRenderOptions {});
    REQUIRE (live.ok());
    REQUIRE (live.engine->liveSchedulesApplied() == 0);
    const std::vector<float> beforeEdit = drainPlayback (*live.engine, totalFrames, 8);

    OfflineRenderStatus status = OfflineRenderStatus::Ok;
    std::unique_ptr<ClipSchedule> schedule = buildTrackClipSchedule (edited, trackId, views, status);
    REQUIRE (schedule != nullptr);
    REQUIRE (status == OfflineRenderStatus::Ok);
    REQUIRE (live.engine->postLiveClipSchedule (
        projectMixerNodeIdForTrack (trackId, ProjectMixerNodeRole::ClipSchedule), std::move (schedule)));
    REQUIRE (live.engine->locate (0));
    const std::vector<float> afterEdit = drainPlayback (*live.engine, totalFrames, 8);
    REQUIRE (live.engine->liveSchedulesApplied() == 1);
    REQUIRE_FALSE (bitIdentical (beforeEdit, afterEdit));

    // Rebuild path: a fresh engine from the edited Project — and the offline render of it.
    PlaybackEngine::Result rebuilt = PlaybackEngine::create (edited, views, OfflineRenderOptions {});
    REQUIRE (rebuilt.ok());
    REQUIRE (rebuilt.engine->locate (0));
    const std::vector<float> rebuiltPlayed = drainPlayback (*rebuilt.engine, totalFrames, 8);
    REQUIRE (bitIdentical (afterEdit, rebuiltPlayed));
    REQUIRE (bitIdentical (afterEdit, offline.interleavedSamples));

    // The retired schedule is the janitor's: reclaim() frees it once the audio thread is past it.
    (void) live.engine->reclaim();

    // A schedule posted for a node that does not exist is retired (never leaked, never installed).
    std::unique_ptr<ClipSchedule> orphan = buildTrackClipSchedule (edited, trackId, views, status);
    REQUIRE (orphan != nullptr);
    REQUIRE (live.engine->postLiveClipSchedule (12345678u, std::move (orphan)));
    (void) drainPlayback (*live.engine, 16, 8);
    REQUIRE (live.engine->liveSchedulesApplied() == 1);
    (void) live.engine->reclaim();
}

TEST_CASE ("PlaybackEngine MIDI publication keeps held audio continuous and reports pending and rejected installs",
           "[playback][midi-schedule]")
{
    using namespace yesdaw::engine;
    const PlaybackFixture fixture = makePlaybackFixture();
    Project project = fixture.project;
    MidiClip clip;
    clip.id = EntityId::fromBigEndianParts (0, 8000);
    clip.trackId = project.tracks[0].id;
    clip.timelineLength = 128;
    clip.timeBase = TimeBase::SampleLocked;
    Note held;
    held.id = EntityId::fromBigEndianParts (0, 8001);
    held.lengthTicks = 112;
    Note future = held;
    future.id = EntityId::fromBigEndianParts (0, 8002);
    future.startTick = 64;
    future.lengthTicks = 16;
    future.key = 72;
    future.pitchNote = 72.0;
    clip.notes = { held, future };
    project.midiClips.push_back (clip);
    REQUIRE (project.hasValidAssetClipIndirection());
    const auto views = std::span<const DecodedAssetAudio> (fixture.decodedAssets);
    auto live = PlaybackEngine::create (project, views);
    auto reference = PlaybackEngine::create (project, views);
    REQUIRE (live.ok());
    REQUIRE (reference.ok());
    REQUIRE (bitIdentical (drainPlayback (*live.engine, 32, 8), drainPlayback (*reference.engine, 32, 8)));

    clip.notes[1].startTick = 80;
    auto built = buildMidiSchedule (clip, {}, project.sampleRate);
    REQUIRE (built.ok());
    const NodeId source = projectMixerNodeIdForClip (clip.id, ProjectMixerNodeRole::MidiSource);
    REQUIRE (live.engine->postLiveMidiSchedule (source, std::move (built.schedule)));
    REQUIRE (live.engine->liveMidiSchedulesApplied() == 0u);
    REQUIRE (bitIdentical (drainPlayback (*live.engine, 32, 8), drainPlayback (*reference.engine, 32, 8)));
    REQUIRE (live.engine->liveMidiSchedulesApplied() == 1u);
    REQUIRE (live.engine->liveMidiSchedulesRejected() == 0u);
    REQUIRE_FALSE (bitIdentical (drainPlayback (*live.engine, 32, 8), drainPlayback (*reference.engine, 32, 8)));
    REQUIRE (live.engine->reclaim() == 1u);

    REQUIRE (live.engine->stop());
    (void) drainPlayback (*live.engine, 8, 8);
    const auto stoppedGeneration = live.engine->processedGen();
    clip.notes[0].lengthTicks = 120;
    built = buildMidiSchedule (clip, {}, project.sampleRate);
    REQUIRE (built.ok());
    REQUIRE (live.engine->postLiveMidiSchedule (source, std::move (built.schedule)));
    const auto stopped = drainPlayback (*live.engine, 8, 8);
    REQUIRE (std::all_of (stopped.begin(), stopped.end(), [] (float sample) { return sample == 0.0f; }));
    REQUIRE (live.engine->processedGen() == stoppedGeneration);
    REQUIRE (live.engine->liveMidiSchedulesApplied() == 1u); // Control does not drain a stopped Runtime
    REQUIRE (live.engine->play());
    (void) drainPlayback (*live.engine, 8, 8);
    REQUIRE (live.engine->liveMidiSchedulesApplied() == 2u);
    built = buildMidiSchedule (clip, {}, project.sampleRate);
    REQUIRE (built.ok());
    REQUIRE (live.engine->postLiveMidiSchedule (12345678u, std::move (built.schedule)));
    (void) drainPlayback (*live.engine, 8, 8);
    REQUIRE (live.engine->liveMidiSchedulesApplied() == 2u);
    REQUIRE (live.engine->liveMidiSchedulesRejected() == 1u);
    REQUIRE (live.engine->reclaim() == 2u);
}

TEST_CASE ("Initial MIDI Track certificate gates every source before any live publication",
           "[playback][midi-schedule]")
{
    using namespace yesdaw::engine;
    bool denseTrack = false;
    SECTION ("eligible Track services seek releases") {}
    SECTION ("dense neighboring Clip preserves unsupported ordinary rendering") { denseTrack = true; }

    Project project;
    project.id = EntityId::fromBigEndianParts (0, 8100);
    project.sampleRate = SampleRate { 48000.0 };
    Track track;
    track.id = EntityId::fromBigEndianParts (0, 8101);
    project.tracks.push_back (track);
    MidiClip clip;
    clip.id = EntityId::fromBigEndianParts (0, 8102);
    clip.trackId = track.id;
    clip.timeBase = TimeBase::SampleLocked;
    clip.timelineLength = 128;
    Note held;
    held.id = EntityId::fromBigEndianParts (0, 8103);
    held.lengthTicks = 100;
    clip.notes.push_back (held);
    project.midiClips.push_back (clip);
    if (denseTrack)
    {
        MidiClip neighbor = clip;
        neighbor.id = EntityId::fromBigEndianParts (0, 8104);
        neighbor.notes.clear();
        for (std::uint64_t i = 0; i < 400u; ++i)
        {
            held.id = EntityId::fromBigEndianParts (0, 9000u + i);
            neighbor.notes.push_back (held);
        }
        project.midiClips.push_back (std::move (neighbor));
    }
    REQUIRE (project.hasValidAssetClipIndirection());
    MixerProjectionInputs projection;
    REQUIRE (projectToMixerProjectionInputs (project, {},
        [] (const Project&, const Clip&, const Asset&, int, ScheduledClipSource&) { return false; }, projection));
    DecodedMidiClipNode* source = nullptr;
    const NodeId sourceId = projectMixerNodeIdForClip (clip.id, ProjectMixerNodeRole::MidiSource);
    for (auto& node : projection.tracks[0].supportNodes)
        if (node->properties().id == sourceId)
            source = dynamic_cast<DecodedMidiClipNode*> (node.get());
    REQUIRE (source != nullptr);
    source->prepare (48000.0, 8);
    std::array<Event, 1024> storage {};
    EventStream stream { std::span<Event> (storage), 0 };
    Transport transport;
    transport.hasTimelineFrame = true;
    source->process (ProcessArgs { AudioBlock {}, stream, transport, 8 });
    REQUIRE (stream.size() == 1u);
    REQUIRE (stream.events()[0].type == EventType::NoteOn); // ordinary timeline is unchanged
    transport.timelineFrame = 32;
    source->process (ProcessArgs { AudioBlock {}, stream, transport, 8 });
    if (denseTrack)
        REQUIRE (stream.events().empty()); // no extra release demand on a never-admitted dense Track
    else
    {
        REQUIRE (stream.size() == 1u);
        REQUIRE (stream.events()[0].type == EventType::NoteOff);
        REQUIRE (stream.events()[0].timeInBlock == 0u);
    }
    auto next = buildMidiSchedule (clip, {}, project.sampleRate);
    REQUIRE (next.ok());
    const MidiSchedule* previous = nullptr;
    REQUIRE (source->exchangeSchedule (next.schedule.get(), previous) == ! denseTrack);
    if (! denseTrack)
    {
        (void) next.schedule.release();
        const std::unique_ptr<const MidiSchedule> retired (previous);
    }
}

// ADR-0053: the live loudness tap. The ring round-trips blocks with their format (frames, channels, rate); a block
// that does not fit is skipped whole and counted; and the engine writes the MIX into it — the master output
// before the metronome click is summed in.
TEST_CASE ("ADR-0053 the loudness tap carries the mix before the click; a full ring skips whole blocks", "[playback][loudness-tap]")
{
    using yesdaw::engine::LoudnessTap;
    struct Block
    {
        std::vector<float> samples;
        std::size_t frames = 0;
        std::size_t channels = 0;
        double rate = 0.0;
    };
    const auto drainAll = [] (LoudnessTap& tap) {
        std::vector<Block> blocks;
        std::vector<float> scratch;
        tap.drain (scratch, [&blocks] (const float* data, std::size_t frames, std::size_t channels, double rate) {
            blocks.push_back ({ std::vector<float> (data, data + frames * channels), frames, channels, rate });
        });
        return blocks;
    };

    {
        LoudnessTap tap;
        std::vector<float> left (100), right (100);
        for (std::size_t i = 0; i < left.size(); ++i)
        {
            left[i] = static_cast<float> (i) * 0.01f;
            right[i] = -static_cast<float> (i) * 0.01f;
        }
        const float* channels[2] = { left.data(), right.data() };
        tap.write (channels, 2, 10, 50, 48'000.0);
        tap.write (channels, 1, 0, 20, 44'100.0);   // mono
        tap.write (channels, 4, 0, 5, 48'000.0);    // more than two channels: the first two
        const std::vector<Block> blocks = drainAll (tap);
        REQUIRE (blocks.size() == 3u);
        REQUIRE (blocks[0].frames == 50u);
        REQUIRE (blocks[0].channels == 2u);
        REQUIRE (blocks[0].rate == 48'000.0);
        for (std::size_t f = 0; f < 50u; ++f)
        {
            REQUIRE (blocks[0].samples[f * 2u] == left[10u + f]);
            REQUIRE (blocks[0].samples[f * 2u + 1u] == right[10u + f]);
        }
        REQUIRE (blocks[1].channels == 1u);
        REQUIRE (blocks[1].rate == 44'100.0);
        REQUIRE (blocks[1].samples == std::vector<float> (left.begin(), left.begin() + 20));
        REQUIRE (blocks[2].channels == 2u);
        REQUIRE (drainAll (tap).empty());   // drained
        REQUIRE (tap.overflowCount() == 0u);

        // A block bigger than the free space is skipped whole and counted; the ring keeps working.
        std::vector<float> huge (LoudnessTap::kCapacityFloats / 2u, 0.5f);
        const float* hugeChannels[2] = { huge.data(), huge.data() };
        tap.write (hugeChannels, 2, 0, static_cast<int> (huge.size()), 48'000.0);   // header + 2x frames > capacity
        REQUIRE (tap.overflowCount() == 1u);
        REQUIRE (drainAll (tap).empty());
        // Fill it with blocks that fit until one does not.
        std::uint64_t written = 0;
        while (tap.overflowCount() == 1u)
        {
            tap.write (hugeChannels, 2, 0, 8'192, 48'000.0);
            ++written;
        }
        const std::vector<Block> filled = drainAll (tap);
        REQUIRE (filled.size() == written - 1u);   // every block that fitted, whole; the overflowing one skipped
        tap.write (channels, 2, 0, 50, 48'000.0);   // room again after the drain
        REQUIRE (drainAll (tap).size() == 1u);
    }

    // The engine feeds the mix: what the tap carries equals a click-free render, bit for bit, while the device
    // output carries the click on top.
    const PlaybackFixture fixture = makePlaybackFixture();
    const std::vector<float> plain = playToBuffer (fixture, 64);
    LoudnessTap tap;
    OfflineRenderOptions options;
    options.loudnessTap = &tap;
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        std::move (options));
    REQUIRE (created.ok());
    PlaybackEngine& engine = *created.engine;
    REQUIRE (engine.setMetronomeEnabled (true) == yesdaw::engine::TimeLookupStatus::Ok);
    const std::vector<float> withClick = drainPlayback (engine, engine.frames(), 64);
    engine.reclaim();
    REQUIRE (withClick != plain);   // the click is in what the device plays

    std::vector<float> tapped;
    for (const Block& block : drainAll (tap))
    {
        REQUIRE (block.channels == 2u);
        REQUIRE (block.rate == 48'000.0);
        tapped.insert (tapped.end(), block.samples.begin(), block.samples.end());
    }
    REQUIRE (tapped == plain);   // and not in the mix the meter measures
}

TEST_CASE ("metronome follows the full ramp instead of a head-tempo beat period", "[playback][musical-click]")
{
    PlaybackFixture fixture = makePlaybackFixture();
    fixture.project.tempoMap = { { 0, 120.0, TempoCurve::LinearRamp },
                                 { 4 * yesdaw::engine::kTicksPerQuarter, 240.0, TempoCurve::Jump } };
    OfflineRenderOptions options;
    options.maxBlockSize = 257;
    auto created = PlaybackEngine::create (fixture.project, fixture.decodedAssets, options);
    REQUIRE (created.ok());
    auto& playback = *created.engine;
    REQUIRE (playback.setMetronomeEnabled (true) == yesdaw::engine::TimeLookupStatus::Ok);
    // Independent integral: F(q)=96000*log(1+q/4), then 12000 frames/quarter.
    constexpr std::array<std::int64_t, 5> onsets { 21421, 38924, 53723, 66542, 78542 };
    for (const auto onset : onsets)
    {
        CAPTURE (onset);
        REQUIRE (playback.locate (onset));
        std::array<float, 64> left {}, right {};
        float* outputs[] { left.data(), right.data() };
        playback.processBlock (outputs, 2, 64);
        float peak = 0.0f;
        for (const float value : left)
            peak = std::max (peak, std::abs (value));
        CHECK (peak > 0.1f);
    }
    REQUIRE (playback.locate (24000));
    std::array<float, 64> left {}, right {};
    float* outputs[] { left.data(), right.data() };
    playback.processBlock (outputs, 2, 64);
    CHECK (std::all_of (left.begin(), left.end(), [] (float value) { return value == 0.0f; }));
}

namespace {
using yesdaw::engine::MeterChange;
using yesdaw::engine::MusicalClickMap;
using yesdaw::engine::TimeLookupStatus;

MusicalClickMap compileClick (const std::vector<TempoChange>& tempo,
                              const std::vector<MeterChange>& meter, double rate = 48000.0)
{
    MusicalClickMap map;
    REQUIRE (MusicalClickMap::build ({ tempo.data(), tempo.size() }, { meter.data(), meter.size() },
                                     { rate }, map) == TimeLookupStatus::Ok);
    return map;
}

std::vector<float> renderClick (PlaybackEngine& engine, std::int64_t start, int count, int partition)
{
    REQUIRE (engine.locate (start));
    std::vector<float> rendered;
    rendered.reserve (static_cast<std::size_t> (count));
    std::array<float, 257> left {}, right {};
    float* outputs[] { left.data(), right.data() };
    while (count > 0)
    {
        const int n = std::min (count, partition);
        engine.processBlock (outputs, 2, n);
        for (int i = 0; i < n; ++i)
        {
            CHECK (left[static_cast<std::size_t> (i)] == right[static_cast<std::size_t> (i)]);
            rendered.push_back (left[static_cast<std::size_t> (i)]);
        }
        count -= n;
    }
    return rendered;
}

float referenceClick (std::int64_t frame, std::span<const std::pair<std::int64_t, bool>> onsets,
                      double rate = 48000.0)
{
    float result = 0.0f;
    for (const auto& [onset, downbeat] : onsets)
    {
        const auto phase = frame - onset;
        if (phase < 0 || phase >= static_cast<std::int64_t> (rate * 0.005))
            continue;
        const double t = static_cast<double> (phase) / rate;
        result += static_cast<float> (std::sin (6.283185307179586 * (downbeat ? 1500.0 : 1000.0) * t)
                                      * std::exp (-600.0 * t) * 0.5);
    }
    return result;
}
} // namespace

TEST_CASE ("click index uses rational origins, truncated bars and full stored Meter values", "[playback][musical-click]")
{
    const auto seven = compileClick ({}, { { 0, 7, 39 } });
    std::int64_t cursor = 0;
    for (std::uint64_t j = 0; j < 80; ++j)
    {
        const auto onset = seven.nextOnset (cursor);
        REQUIRE (onset.status == TimeLookupStatus::Ok);
        REQUIRE (onset.found);
        // At 120 BPM the exact Frame position is j*96000/39.
        CHECK (onset.frame == static_cast<std::int64_t> (j * 96000 / 39));
        CHECK (onset.downbeat == (j % 7 == 0));
        CHECK (onset.ordinalProbes <= 66);
        CHECK (onset.segmentComparisons <= 2);
        cursor = onset.frame + 1;
    }
    const auto truncated = compileClick ({}, { { 0, 3, 7 }, { 26332, 4, 4 } });
    const auto oldBar = truncated.nextOnset (41142);
    REQUIRE (oldBar.found);
    CHECK (oldBar.frame == 41142); // exact 3/7 bar two is still before the change
    CHECK (oldBar.downbeat);
    const auto changed = truncated.nextOnset (41143);
    REQUIRE (changed.found);
    CHECK (changed.frame == 41143);
    CHECK (changed.downbeat);
    for (const MeterChange meter : { MeterChange { 0, 65535, 1 }, MeterChange { 0, 65535, 65535 },
                                    MeterChange { 0, 1, 65535 } })
    {
        const auto map = compileClick ({}, { meter });
        const auto onset = map.nextOnset (1);
        REQUIRE (onset.found);
        CHECK (onset.frame == static_cast<std::int64_t> (96000 / meter.denominator));
        CHECK (onset.downbeat == (meter.numerator == 1));
    }
    // The global one-based bar prefix would overflow, but click ordinals have no such restriction.
    const auto noBarCounter = compileClick ({}, { { 0, 1, 65535 },
        { std::numeric_limits<Tick>::max(), 4, 4 } });
    CHECK (noBarCounter.nextOnset (1).found);
    const auto jump = compileClick ({ { 0, 120.0, TempoCurve::Jump },
                                      { 2 * yesdaw::engine::kTicksPerQuarter, 60.0, TempoCurve::Jump } }, {});
    for (const auto [query, expected] : { std::pair<std::int64_t, std::int64_t> { 1, 24000 },
                                        { 24001, 48000 }, { 48001, 96000 } })
        CHECK (jump.nextOnset (query).frame == expected);
}

TEST_CASE ("click buckets skip dense ordinals and coincident Meter segments with bounded work", "[playback][musical-click]")
{
    const auto dense = compileClick ({ { 0, 11520000.0, TempoCurve::Jump } }, { { 0, 7, 4 } });
    for (std::int64_t frame = 0; frame < 257; ++frame)
    {
        const auto onset = dense.nextOnset (frame);
        REQUIRE (onset.status == TimeLookupStatus::Ok);
        REQUIRE (onset.found);
        CHECK (onset.frame == frame);
        // Four exact quarter-note onsets per Frame; any downbeat in the bucket wins.
        CHECK (onset.downbeat == (((frame * 4 + 6) / 7) * 7 <= frame * 4 + 3));
        CHECK (onset.ordinalProbes <= 66);
        CHECK (onset.segmentComparisons <= 2);
    }
    std::vector<MeterChange> meters;
    std::vector<TempoChange> tempos;
    for (Tick i = 0; i < 8192; ++i)
    {
        meters.push_back ({ i, static_cast<std::uint16_t> (i % 65535 + 1), 65535 });
        tempos.push_back ({ i, 0x1p60, TempoCurve::Jump });
    }
    const auto coincident = compileClick (tempos, meters);
    const auto onset = coincident.nextOnset (0);
    REQUIRE (onset.status == TimeLookupStatus::Ok);
    REQUIRE (onset.found);
    CHECK (onset.frame == 0);
    CHECK (onset.downbeat);
    CHECK (onset.segmentComparisons <= 28);
    CHECK (onset.ordinalProbes <= 66);
    // At this BPM the next Frame cannot be inverted to whole Tick zero; forward search still advances.
    const auto after = coincident.nextOnset (1);
    CHECK (after.status == TimeLookupStatus::Ok);
    CHECK (after.frame >= 1);
    CHECK (after.segmentComparisons <= 28);
    const auto collapsed = compileClick ({ { 0, 1.0e300, TempoCurve::Jump } }, meters);
    CHECK (collapsed.nextOnset (0).frame == 0);
    CHECK (collapsed.nextOnset (0).downbeat);
    CHECK (collapsed.nextOnset (1).status == TimeLookupStatus::Overflow);
}

TEST_CASE ("click rendering retains seek tails and repeats the absolute grid across Block partitions", "[playback][musical-click]")
{
    for (const double rate : { 48000.0, 44100.0 })
    {
        Project project;
        project.sampleRate = { rate };
        project.tempoMap = { { 0, 120.0, TempoCurve::LinearRamp },
                             { 4 * yesdaw::engine::kTicksPerQuarter, 240.0, TempoCurve::Jump } };
        project.meterMap = { { 0, 7, 39 } };
        std::vector<std::pair<std::int64_t, bool>> onsets;
        for (int j = 0; j < 90; ++j)
        {
            const double q = 4.0 * j / 39.0;
            const double frame = q <= 4.0 ? 2.0 * rate * std::log1p (q / 4.0)
                : 2.0 * rate * std::log (2.0) + (q - 4.0) * rate / 4.0;
            onsets.emplace_back (static_cast<std::int64_t> (std::floor (frame)), j % 7 == 0);
        }
        const auto start = onsets[11].first + 37; // inside a preceding click's tail
        for (const int partition : { 1, 7, 128, 257 })
        {
            auto engine = PlaybackEngine::createTransportOnly (project, 257);
            REQUIRE (engine != nullptr);
            REQUIRE (engine->setMetronomeEnabled (true) == TimeLookupStatus::Ok);
            const auto retained = engine->metronomeClock();
            const auto actual = renderClick (*engine, start, 5000, partition);
            for (std::size_t i = 0; i < actual.size(); ++i)
                CHECK (std::abs (actual[i] - referenceClick (start + static_cast<std::int64_t> (i), onsets, rate)) < 1.0e-6f);
            const std::int64_t loopStart = onsets[4].first + 23, loopEnd = onsets[8].first + 71;
            REQUIRE (engine->setLoop (loopStart, loopEnd));
            const int count = static_cast<int> ((loopEnd - loopStart) * 2 + 59);
            const auto looped = renderClick (*engine, loopStart, count, partition);
            for (std::size_t i = 0; i < looped.size(); ++i)
            {
                const auto frame = loopStart + static_cast<std::int64_t> (i) % (loopEnd - loopStart);
                CHECK (std::abs (looped[i] - referenceClick (frame, onsets, rate)) < 1.0e-6f);
            }
            REQUIRE (engine->clearLoop());
            REQUIRE (engine->setPlaybackRate (4));
            const auto shuttle = renderClick (*engine, start, 257, partition);
            for (std::size_t i = 0; i < shuttle.size(); ++i)
                CHECK (std::abs (shuttle[i] - referenceClick (start + 4 * static_cast<std::int64_t> (i), onsets, rate)) < 1.0e-6f);
            CHECK (engine->metronomeLookupStatus() == TimeLookupStatus::Ok);
            engine.reset();
            CHECK (retained->nextOnset (0).found); // accepted clock ownership survives replacement
        }
    }
}

TEST_CASE ("click failures are named and do not prevent SampleLocked audio playback", "[playback][musical-click]")
{
    MusicalClickMap map;
    CHECK (map.nextOnset (0).status == TimeLookupStatus::InvalidMap);
    CHECK (map.nextOnset (-1).status == TimeLookupStatus::InvalidInput);
    CHECK (MusicalClickMap::build ({ nullptr, 1 }, {}, { 48000.0 }, map) == TimeLookupStatus::InvalidMap);
    CHECK (MusicalClickMap::build ({}, { nullptr, 1 }, { 48000.0 }, map) == TimeLookupStatus::InvalidMap);
    const MeterChange bad { 1, 4, 4 };
    CHECK (MusicalClickMap::build ({}, { &bad, 1 }, { 48000.0 }, map) == TimeLookupStatus::InvalidMap);
    const auto native = compileClick ({}, {});
    CHECK (native.nextOnset (std::numeric_limits<std::int64_t>::max()).status == TimeLookupStatus::Ok);
    CHECK_FALSE (native.nextOnset (std::numeric_limits<std::int64_t>::max()).found);
    const auto slow = compileClick ({ { 0, 1.0e-300, TempoCurve::Jump } }, {});
    CHECK_FALSE (slow.nextOnset (1).found); // next beat is provably beyond native Frames
    const auto tiny = compileClick ({ { 0, std::numeric_limits<double>::denorm_min(), TempoCurve::Jump } }, {});
    CHECK (tiny.nextOnset (0).found);
    CHECK (tiny.nextOnset (1).status == TimeLookupStatus::NumericDomain);

    PlaybackFixture fixture = makePlaybackFixture();
    const auto plain = playToBuffer (fixture, 64);
    fixture.project.tempoMap = { { 0, 1.0e-300, TempoCurve::LinearRamp },
        { Tick { 1 } << 48, std::nextafter (1.0e-300, 1.0), TempoCurve::Jump } };
    auto created = PlaybackEngine::create (fixture.project, fixture.decodedAssets);
    REQUIRE (created.ok());
    CHECK (created.engine->metronomePreparationStatus() == TimeLookupStatus::NumericDomain);
    CHECK (created.engine->setMetronomeEnabled (true) == TimeLookupStatus::NumericDomain);
    CHECK_FALSE (created.engine->metronomeEnabled());
    const auto actual = drainPlayback (*created.engine, created.engine->frames(), 64);
    CHECK (actual == plain);
}

TEST_CASE ("dense click rendering meets the existing 128-frame deadline oracle", "[playback][musical-click][perf]")
{
    for (const bool coincidentChanges : { false, true })
    {
        Project project;
        project.sampleRate = { 48000.0 };
        project.meterMap = { { 0, 65535, 65535 } };
        project.tempoMap = { { 0, 6000.0, TempoCurve::LinearRamp },
                             { 4 * yesdaw::engine::kTicksPerQuarter, 12000.0, TempoCurve::Jump } };
        if (coincidentChanges)
        {
            project.meterMap.clear();
            project.tempoMap.clear();
            for (Tick tick = 0; tick < 8192; ++tick)
            {
                project.meterMap.push_back ({ tick, static_cast<std::uint16_t> (tick % 65535 + 1), 65535 });
                project.tempoMap.push_back ({ tick, 6000.0 + 6000.0 * static_cast<double> (tick) / 61440.0,
                                             TempoCurve::LinearRamp });
            }
            project.tempoMap.push_back ({ 61440, 12000.0, TempoCurve::Jump });
        }
        auto engine = PlaybackEngine::createTransportOnly (project, 257);
        REQUIRE (engine != nullptr);
        REQUIRE (engine->setMetronomeEnabled (true) == TimeLookupStatus::Ok);
        REQUIRE (engine->setLoop (256, 1024));
        std::array<float, 257> left {}, right {};
        float* outputs[] { left.data(), right.data() };
        double checksum = 0.0;
        for (const int blockSize : { 128, 257 })
        {
            for (int warm = 0; warm < 50; ++warm)
                engine->processBlock (outputs, 2, blockSize);
            std::vector<std::uint64_t> times;
            times.reserve (1000);
            for (int block = 0; block < 1000; ++block)
            {
                const auto before = std::chrono::steady_clock::now();
                engine->processBlock (outputs, 2, blockSize);
                const auto after = std::chrono::steady_clock::now();
                const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds> (after - before).count();
                times.push_back (ns > 0 ? static_cast<std::uint64_t> (ns) : 1u);
                checksum += left[static_cast<std::size_t> (block % blockSize)];
            }
            const auto stats = yesdaw::engine::summarizeDeadlineSoak (times, 48000.0, blockSize, 1u, 0u);
            INFO ("coincident=" << coincidentChanges << " block=" << blockSize
                  << " p999=" << stats.p999BlockNanos << " max=" << stats.maxBlockNanos
                  << " deadline=" << stats.blockPeriodNanos << " ns");
            CHECK (stats.blocksProcessed == times.size());
    #if ! defined (YESDAW_SANITIZER_BUILD)
            CHECK (stats.passesDeadline());
    #endif
            CHECK (engine->metronomeLookupStatus() == TimeLookupStatus::Ok);
        }
        CHECK (std::isfinite (checksum));
        CHECK (checksum != 0.0);
    }
}

TEST_CASE ("click native Frame flooring is exact or names lost floating precision", "[playback][musical-click][click-native-floor]")
{
    const auto map = compileClick ({ { 0, 125.0, TempoCurve::Jump } }, { { 0, 7, 65535 } });
    // Exact constant law: beat j is at j*92160/65535 Frames. Integer quotients keep this
    // independent oracle exact above double's consecutive-integer range on Windows too.
    for (const std::int64_t frame : { (std::int64_t { 1 } << 53) + 1, (std::int64_t { 1 } << 53) + 3,
                                     (std::int64_t { 1 } << 53) + 257 })
    {
        const auto whole = static_cast<std::uint64_t> (frame);
        const auto ordinal = (whole / 92160) * 65535 + ((whole % 92160) * 65535 + 92159) / 92160;
        const auto expected = (ordinal / 65535) * 92160 + ((ordinal % 65535) * 92160) / 65535;
        const auto onset = map.nextOnset (frame);
        CAPTURE (frame, ordinal, expected, onset.frame);
        CHECK ((onset.status == TimeLookupStatus::NumericDomain
                || (onset.status == TimeLookupStatus::Ok && onset.found
                    && onset.frame == static_cast<std::int64_t> (expected))));
    }
}

namespace {
bool clickQueriesInRealtimeScope (const MusicalClickMap& clock) noexcept YESDAW_RT_HOT
{
    auto window = clock.onsetsInRange (0, 257);
    if (window.preparation().ordinalProbes > 132 || window.preparation().segmentComparisons > 4)
        return false;
    for (std::int64_t frame = 0; frame < 257; ++frame)
    {
        const auto onset = window.next();
        if (onset.status != TimeLookupStatus::Ok || ! onset.found || onset.frame != frame
            || onset.ordinalProbes > 66 || onset.segmentComparisons > 2)
            return false;
    }
    const auto end = window.next();
    return end.status == TimeLookupStatus::Ok && ! end.found;
}
} // namespace

TEST_CASE ("dense click PCM coalesces accents once and stays real-time safe", "[playback][musical-click][rtsan]")
{
    Project project;
    project.sampleRate = { 48000.0 };
    project.tempoMap = { { 0, 11520000.0, TempoCurve::Jump } };
    project.meterMap = { { 0, 7, 4 } };
    std::vector<std::pair<std::int64_t, bool>> onsets;
    for (std::int64_t frame = 0; frame < 600; ++frame)
        onsets.emplace_back (frame, (((frame * 4 + 6) / 7) * 7 <= frame * 4 + 3));
    for (const int partition : { 1, 7, 128, 257 })
    {
        auto engine = PlaybackEngine::createTransportOnly (project, 257);
        REQUIRE (engine != nullptr);
        REQUIRE (engine->setMetronomeEnabled (true) == TimeLookupStatus::Ok);
        REQUIRE (clickQueriesInRealtimeScope (*engine->metronomeClock()));
        const auto rendered = renderClick (*engine, 300, 257, partition);
        for (std::size_t i = 0; i < rendered.size(); ++i)
            CHECK (std::abs (rendered[i] - referenceClick (300 + static_cast<std::int64_t> (i), onsets)) < 1.0e-6f);
    }
}

TEST_CASE ("click index names malformed clocks and moved-from state without losing valid prefixes", "[playback][musical-click]")
{
    MusicalClickMap map;
    for (const double bpm : { -1.0, 0.0, std::numeric_limits<double>::infinity(),
                              std::numeric_limits<double>::quiet_NaN() })
    {
        const TempoChange bad { 0, bpm, TempoCurve::Jump };
        CHECK (MusicalClickMap::build ({ &bad, 1 }, {}, { 48000.0 }, map) == TimeLookupStatus::InvalidMap);
        CHECK (map.nextOnset (0).status == TimeLookupStatus::InvalidMap);
    }
    for (const double rate : { -1.0, 0.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN() })
        CHECK (MusicalClickMap::build ({}, {}, { rate }, map) == TimeLookupStatus::InvalidMap);
    for (const std::vector<MeterChange>& meter : { std::vector<MeterChange> { { 0, 0, 4 } },
        { { 0, 4, 0 } }, { { 0, 4, 4 }, { 0, 3, 4 } }, { { 0, 4, 4 }, { -1, 3, 4 } } })
        CHECK (MusicalClickMap::build ({}, { meter.data(), meter.size() }, { 48000.0 }, map)
               == TimeLookupStatus::InvalidMap);
    map = compileClick ({}, {});
    MusicalClickMap moved = std::move (map);
    CHECK (moved.nextOnset (24000).frame == 24000);
    CHECK (map.nextOnset (0).status == TimeLookupStatus::InvalidMap);
    CHECK (moved.nextOnset (0).frame == 0);
    const auto largeExact = moved.nextOnset (std::int64_t { 1 } << 53);
    CHECK (largeExact.status == TimeLookupStatus::Ok);
    CHECK (largeExact.found);
    CHECK (largeExact.frame == (((std::int64_t { 1 } << 53) + 23999) / 24000) * 24000);
}

TEST_CASE ("click transport windows match independent sparse and dense buckets within finite work", "[playback][musical-click]")
{
    for (const bool dense : { false, true })
    {
        const auto clock = compileClick ({ { 0, dense ? 11520000.0 : 120.0, TempoCurve::Jump } },
                                         { { 0, 7, 4 } });
        for (const auto [first, end] : { std::pair<std::int64_t, std::int64_t> { 0, 0 }, { 0, 1 },
            { 0, 257 }, { 1, 257 }, { 23990, 24010 }, { 24000, 24001 }, { 24001, 48001 } })
        {
            auto window = clock.onsetsInRange (first, end);
            const auto width = static_cast<std::uint64_t> (end - first);
            CHECK (window.preparation().ordinalProbes <= 132);
            CHECK (window.preparation().segmentComparisons <= 4);
            std::uint64_t events = 0, probes = window.preparation().ordinalProbes;
            std::int64_t previous = first - 1;
            for (;;)
            {
                const auto onset = window.next();
                REQUIRE (onset.status == TimeLookupStatus::Ok);
                probes += onset.ordinalProbes;
                if (! onset.found)
                    break;
                REQUIRE (++events <= width);
                CHECK (onset.frame > previous);
                CHECK (onset.frame < end);
                if (dense)
                {
                    CHECK (onset.frame == first + static_cast<std::int64_t> (events) - 1);
                    CHECK (onset.downbeat == (((onset.frame * 4 + 6) / 7) * 7 <= onset.frame * 4 + 3));
                }
                else
                {
                    const auto ordinal = onset.frame / 24000;
                    CHECK (onset.frame == ordinal * 24000);
                    CHECK (onset.downbeat == (ordinal % 7 == 0));
                }
                previous = onset.frame;
            }
            CHECK (probes <= 132u + (width + 1u) * 66u);
            CHECK (events == (dense ? width
                : static_cast<std::uint64_t> ((end + 23999) / 24000 - (first + 23999) / 24000)));
        }
    }
    const auto validClock = compileClick ({}, {});
    auto invalid = validClock.onsetsInRange (-1, 10);
    CHECK (invalid.next().status == TimeLookupStatus::InvalidInput);
    MusicalClickMap empty;
    auto unbuilt = empty.onsetsInRange (0, 1);
    CHECK (unbuilt.next().status == TimeLookupStatus::InvalidMap);
}

TEST_CASE ("rational click floors below the double integer limit are exact or refused", "[playback][musical-click][click-rational-floor]")
{
    constexpr std::uint64_t ordinal = 2902u + 4369u * (std::uint64_t { 1 } << 38);
    constexpr std::uint64_t upperFrame = 3u * (std::uint64_t { 1 } << 49) + 4081u;
    // Exact 125-BPM law: Frame = ordinal*92160/65535 = upperFrame - 1/4369.
    // Quotient/remainder arithmetic is independent of the binary64 clock under test.
    constexpr auto exactWhole = (ordinal / 65535u) * 92160u + ((ordinal % 65535u) * 92160u) / 65535u;
    constexpr auto exactRemainder = ((ordinal % 65535u) * 92160u) % 65535u;
    STATIC_REQUIRE (exactWhole == upperFrame - 1u);
    STATIC_REQUIRE (exactRemainder == 65520u);
    const auto clock = compileClick ({ { 0, 125.0, TempoCurve::Jump } }, { { 0, 7, 65535 } });
    const auto onset = clock.nextOnset (static_cast<std::int64_t> (upperFrame - 1u));
    CAPTURE (ordinal, upperFrame, onset.frame);
    CHECK ((onset.status == TimeLookupStatus::NumericDomain
            || (onset.status == TimeLookupStatus::Ok && onset.found
                && onset.frame == static_cast<std::int64_t> (exactWhole))));
}

TEST_CASE ("rational click certification preserves distant exact ordinals and earlier window tails", "[playback][musical-click][click-rational-floor]")
{
    constexpr std::uint64_t k = std::uint64_t { 1 } << 29;
    constexpr std::uint64_t ordinal = 2902u + 4369u * k;
    constexpr std::uint64_t upperFrame = 3u * (std::uint64_t { 1 } << 40) + 4081u;
    constexpr auto exactWhole = (ordinal / 65535u) * 92160u + ((ordinal % 65535u) * 92160u) / 65535u;
    STATIC_REQUIRE (exactWhole == upperFrame - 1u);
    const auto clock = compileClick ({ { 0, 125.0, TempoCurve::Jump } }, { { 0, 7, 65535 } });
    const auto ambiguous = clock.nextOnset (static_cast<std::int64_t> (upperFrame - 1u));
    CAPTURE (ordinal, upperFrame, ambiguous.frame);
    CHECK ((ambiguous.status == TimeLookupStatus::NumericDomain
            || (ambiguous.status == TimeLookupStatus::Ok && ambiguous.found
                && ambiguous.frame == static_cast<std::int64_t> (exactWhole))));
    const auto early = clock.nextOnset (1);
    CHECK (early.status == TimeLookupStatus::Ok);
    CHECK (early.found);
    CHECK (early.frame == 1);
    // The ambiguous ordinal is followed by a provable exact integer onset; certification
    // failure must not be treated as a monotonic end of the usable clock.
    const auto laterExactFrame = static_cast<std::int64_t> ((k + 1u) * 6144u);
    const auto later = clock.nextOnset (laterExactFrame);
    CHECK (later.status == TimeLookupStatus::Ok);
    CHECK (later.found);
    CHECK (later.frame == laterExactFrame);
    auto window = clock.onsetsInRange (static_cast<std::int64_t> (exactWhole) - 16,
                                      static_cast<std::int64_t> (upperFrame));
    const auto tail = window.next();
    CHECK (tail.status == TimeLookupStatus::Ok);
    CHECK (tail.found);
    CHECK (tail.frame < static_cast<std::int64_t> (exactWhole));
    CHECK (clock.nextOnset (static_cast<std::int64_t> (upperFrame)).status == TimeLookupStatus::NumericDomain);
}

TEST_CASE ("click joins refuse possible backward buckets without poisoning earlier or later queries", "[playback][musical-click][click-join-order]")
{
    constexpr Tick join = (Tick { 1 } << 53) + 1;
    constexpr std::int64_t anchor = std::int64_t { 3 } << 52;
    const auto clock = compileClick ({ { 0, 125.0, TempoCurve::Jump }, { join, 125.0, TempoCurve::Jump } },
                                     { { 0, 7, 65535 }, { join, 4, 4 } });
    // The inherited whole-Tick prefix rounds this redundant join down to anchor while
    // the previous constant piece can approach anchor+1.5. Global bucket order is uncertain.
    const auto refused = clock.nextOnset (anchor);
    CHECK (refused.status == TimeLookupStatus::NumericDomain);
    CHECK (refused.precisionComparisons > 0);
    CHECK (refused.precisionComparisons <= 4);
    CHECK (clock.nextOnset (1).frame == 1);
    const auto later = clock.nextOnset (anchor + 46080);
    CHECK (later.status == TimeLookupStatus::Ok);
    CHECK (later.found);
    CHECK (later.frame == anchor + 46080);
    CHECK (later.precisionComparisons <= 4);
}

TEST_CASE ("uncertain dense ordinal brackets keep exact buckets and bounded work above large local Ticks", "[playback][musical-click][click-uncertain-work]")
{
    std::vector<TempoChange> tempos;
    std::vector<MeterChange> meters;
    for (Tick tick = 0; tick < 8192; ++tick)
    {
        tempos.push_back ({ tick, 0x1p60, TempoCurve::Jump });
        meters.push_back ({ tick, static_cast<std::uint16_t> (tick + 1), 65535 });
    }
    const auto clock = compileClick (tempos, meters);
    for (const std::int64_t frame : { 1, 2, 7, 257 })
    {
        const auto onset = clock.nextOnset (frame);
        REQUIRE (onset.status == TimeLookupStatus::Ok);
        REQUIRE (onset.found);
        CHECK (onset.frame == frame);
        CHECK (onset.downbeat);
        // Two 64-step searches with rough+tight fallback, two tight endpoints and
        // two accent comparisons with fallback: 256+2+4 evaluations, all counted.
        CHECK (onset.ordinalProbes <= 2u * 64u * 2u + 2u + 4u);
        CHECK (onset.ordinalProbes == onset.roughProbes + onset.tightProbes);
        CHECK (onset.segmentComparisons <= 2u * 14u + 2u);
        CHECK (onset.precisionComparisons == 0);
    }
    auto window = clock.onsetsInRange (1, 258);
    CHECK (window.preparation().ordinalProbes <= 2u * 262u);
    CHECK (window.preparation().ordinalProbes == window.preparation().roughProbes + window.preparation().tightProbes);
    CHECK (window.preparation().segmentComparisons <= 2u * 30u);
    CHECK (window.preparation().precisionComparisons == 0);
    std::uint64_t probes = window.preparation().ordinalProbes;
    std::uint64_t events = 0;
    for (;;)
    {
        const auto onset = window.next();
        REQUIRE (onset.status == TimeLookupStatus::Ok);
        probes += onset.ordinalProbes;
        if (! onset.found)
            break;
        REQUIRE (++events <= 257);
        CHECK (onset.frame == static_cast<std::int64_t> (events));
        CHECK (onset.downbeat);
        CHECK (onset.ordinalProbes <= 262);
        CHECK (onset.ordinalProbes == onset.roughProbes + onset.tightProbes);
    }
    CHECK (events == 257);
    CHECK (probes <= 524u + 258u * 262u);
}
