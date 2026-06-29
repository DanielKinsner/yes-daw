// YES DAW - H10 device hot-swap gate.
//
// A deterministic fake-device harness proves the control-side stop/snapshot/rebuild/resume path without
// relying on real hardware or subjective glitch checks.

#include "engine/DeviceHotSwap.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::DecodedAssetAudio;
using yesdaw::engine::DeviceHotSwapCoordinator;
using yesdaw::engine::DeviceHotSwapFormat;
using yesdaw::engine::DeviceHotSwapStatus;
using yesdaw::engine::EntityId;
using yesdaw::engine::OfflineRenderOptions;
using yesdaw::engine::PlaybackEngine;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::engine::renderOfflineProject;

namespace {

constexpr SampleRate kSampleRate { 48000.0 };

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
        hash.bytes[i] = static_cast<std::uint8_t> (seed + static_cast<std::uint8_t> (i * 7u));

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

bool allZero (std::span<const float> samples) noexcept
{
    return std::all_of (samples.begin(), samples.end(), [] (float sample) { return floatBits (sample) == 0u; });
}

struct HotSwapFixture
{
    Project                         project;
    std::vector<std::vector<float>> samples;
    std::vector<DecodedAssetAudio>  decodedAssets;
};

HotSwapFixture makeFixture()
{
    HotSwapFixture fixture;
    fixture.samples = {
        {
            0.13f, -0.21f, 0.34f, -0.43f, 0.55f, -0.62f, 0.71f, -0.83f,
            0.92f, -0.37f, 0.28f, -0.19f, 0.48f, -0.57f, 0.66f, -0.75f,
            0.84f, -0.93f, 0.31f, -0.42f, 0.53f, -0.64f, 0.76f, -0.87f,
            0.98f, -0.16f, 0.27f, -0.38f, 0.49f, -0.50f, 0.61f, -0.72f,
        },
    };

    Asset asset;
    asset.id = idFromLowByte (10);
    asset.contentHash = hashWithSeed (10);
    asset.frames = static_cast<std::uint64_t> (fixture.samples[0].size());
    asset.sampleRate = kSampleRate;
    asset.channels = 1;

    Clip clip;
    clip.id = idFromLowByte (20);
    clip.assetId = asset.id;
    clip.timelineStart = 0;
    clip.timelineLength = static_cast<yesdaw::engine::Tick> (asset.frames);
    clip.srcOffset = 0;
    clip.srcLen = asset.frames;
    clip.gain = 1.0f;
    clip.fadeIn = 0;
    clip.fadeOut = 0;
    clip.timeBase = TimeBase::SampleLocked;

    fixture.project.id = idFromLowByte (1);
    fixture.project.sampleRate = kSampleRate;
    fixture.project.assets = { asset };
    fixture.project.clips = { clip };
    REQUIRE (fixture.project.hasValidAssetClipIndirection());

    fixture.decodedAssets = {
        DecodedAssetAudio { asset.id, asset.sampleRate, asset.frames, asset.channels,
                            std::span<const float> (fixture.samples[0].data(), fixture.samples[0].size()) },
    };
    return fixture;
}

std::vector<float> drainPlayback (PlaybackEngine& engine, std::uint64_t frames, int blockSize)
{
    const int channels = static_cast<int> (engine.channels());
    REQUIRE (channels == 2);

    std::vector<float> out (static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels), 0.0f);
    std::vector<float> storage (static_cast<std::size_t> (channels) * static_cast<std::size_t> (blockSize), -99.0f);
    std::vector<float*> ptrs (static_cast<std::size_t> (channels), nullptr);

    std::uint64_t offset = 0;
    while (offset < frames)
    {
        const int n = static_cast<int> (std::min<std::uint64_t> (frames - offset, static_cast<std::uint64_t> (blockSize)));
        std::fill (storage.begin(), storage.end(), -99.0f);
        for (int c = 0; c < channels; ++c)
            ptrs[static_cast<std::size_t> (c)] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (blockSize);

        engine.processBlock (ptrs.data(), channels, n);

        for (int frame = 0; frame < n; ++frame)
            for (int channel = 0; channel < channels; ++channel)
                out[(static_cast<std::size_t> (offset) + static_cast<std::size_t> (frame)) * static_cast<std::size_t> (channels)
                    + static_cast<std::size_t> (channel)] = ptrs[static_cast<std::size_t> (channel)][frame];

        offset += static_cast<std::uint64_t> (n);
    }

    return out;
}

std::vector<float> drainCoordinator (DeviceHotSwapCoordinator& coordinator, std::uint64_t frames, int blockSize)
{
    const int channels = static_cast<int> (coordinator.channels());
    REQUIRE (channels == 2);

    std::vector<float> out (static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels), 0.0f);
    std::vector<float> storage (static_cast<std::size_t> (channels) * static_cast<std::size_t> (blockSize), -99.0f);
    std::vector<float*> ptrs (static_cast<std::size_t> (channels), nullptr);

    std::uint64_t offset = 0;
    while (offset < frames)
    {
        const int n = static_cast<int> (std::min<std::uint64_t> (frames - offset, static_cast<std::uint64_t> (blockSize)));
        std::fill (storage.begin(), storage.end(), -99.0f);
        for (int c = 0; c < channels; ++c)
            ptrs[static_cast<std::size_t> (c)] = storage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (blockSize);

        REQUIRE (coordinator.processBlock (ptrs.data(), channels, n) == DeviceHotSwapStatus::Ok);

        for (int frame = 0; frame < n; ++frame)
            for (int channel = 0; channel < channels; ++channel)
                out[(static_cast<std::size_t> (offset) + static_cast<std::size_t> (frame)) * static_cast<std::size_t> (channels)
                    + static_cast<std::size_t> (channel)] = ptrs[static_cast<std::size_t> (channel)][frame];

        offset += static_cast<std::uint64_t> (n);
    }

    return out;
}

std::vector<float> concatenate (std::span<const float> a, std::span<const float> b)
{
    std::vector<float> out;
    out.reserve (a.size() + b.size());
    out.insert (out.end(), a.begin(), a.end());
    out.insert (out.end(), b.begin(), b.end());
    return out;
}

std::vector<float> referenceSlice (std::span<const float> full,
                                   std::uint16_t channels,
                                   std::int64_t startFrame,
                                   std::uint64_t frames)
{
    std::vector<float> out (static_cast<std::size_t> (frames) * channels, 0.0f);
    const std::size_t fullFrames = full.size() / channels;
    for (std::uint64_t frame = 0; frame < frames; ++frame)
    {
        const std::int64_t sourceFrame = startFrame + static_cast<std::int64_t> (frame);
        if (sourceFrame < 0 || static_cast<std::size_t> (sourceFrame) >= fullFrames)
            continue;

        for (std::uint16_t channel = 0; channel < channels; ++channel)
            out[static_cast<std::size_t> (frame) * channels + channel] =
                full[static_cast<std::size_t> (sourceFrame) * channels + channel];
    }
    return out;
}

std::vector<float> loopSlice (std::span<const float> full,
                              std::uint16_t channels,
                              std::int64_t loopStart,
                              std::int64_t loopEnd,
                              std::uint64_t frames)
{
    REQUIRE (loopStart >= 0);
    REQUIRE (loopEnd > loopStart);

    const std::int64_t loopLength = loopEnd - loopStart;
    std::vector<float> out (static_cast<std::size_t> (frames) * channels, 0.0f);
    for (std::uint64_t frame = 0; frame < frames; ++frame)
    {
        const std::int64_t sourceFrame = loopStart + (static_cast<std::int64_t> (frame) % loopLength);
        for (std::uint16_t channel = 0; channel < channels; ++channel)
            out[static_cast<std::size_t> (frame) * channels + channel] =
                full[static_cast<std::size_t> (sourceFrame) * channels + channel];
    }
    return out;
}

DeviceHotSwapCoordinator::CreateResult createCoordinator (const HotSwapFixture& fixture,
                                                          DeviceHotSwapFormat format)
{
    OfflineRenderOptions options;
    options.maxBlockSize = format.maxBlockSize;
    return DeviceHotSwapCoordinator::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        format,
        options);
}

std::vector<float> uninterruptedPlayback (const HotSwapFixture& fixture, int blockSize)
{
    OfflineRenderOptions options;
    options.maxBlockSize = blockSize;
    PlaybackEngine::Result created = PlaybackEngine::create (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()),
        options);
    REQUIRE (created.ok());
    return drainPlayback (*created.engine, created.engine->frames(), blockSize);
}

} // namespace

TEST_CASE ("YesDawDeviceHotSwapCheck preserves samples across a max-Block device swap",
           "[h10][device-hot-swap][continuity]")
{
    const std::uint64_t base = CompiledGraph::aliveCount();
    {
        const HotSwapFixture fixture = makeFixture();
        const auto offline = renderOfflineProject (
            fixture.project,
            std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()));
        REQUIRE (offline.ok());
        REQUIRE (offline.channels == 2);

        const std::vector<float> uninterrupted = uninterruptedPlayback (fixture, 5);
        REQUIRE (bitIdentical (uninterrupted, offline.interleavedSamples));

        auto created = createCoordinator (fixture, DeviceHotSwapFormat { 1001u, kSampleRate, 2u, 3 });
        REQUIRE (created.ok());
        DeviceHotSwapCoordinator& coordinator = *created.coordinator;
        REQUIRE (CompiledGraph::aliveCount() == base + 1u);

        REQUIRE (coordinator.startCallback() == DeviceHotSwapStatus::Ok);
        const std::vector<float> beforeSwap = drainCoordinator (coordinator, 7u, 3);
        REQUIRE (coordinator.playheadFrame() == 7);
        REQUIRE (coordinator.stopCallback() == DeviceHotSwapStatus::Ok);

        const auto swapped = coordinator.swapDevice (DeviceHotSwapFormat { 1002u, kSampleRate, 2u, 5 });
        REQUIRE (swapped.ok());
        REQUIRE (coordinator.format().deviceId == 1002u);
        REQUIRE (coordinator.maxBlockSize() == 5);
        REQUIRE (CompiledGraph::aliveCount() == base + 1u);

        REQUIRE (coordinator.startCallback() == DeviceHotSwapStatus::Ok);
        const std::vector<float> afterSwap = drainCoordinator (coordinator, offline.frames - 7u, 5);
        const std::vector<float> hotSwapped = concatenate (beforeSwap, afterSwap);

        REQUIRE (bitIdentical (hotSwapped, uninterrupted));
        REQUIRE (floatBits (hotSwapped[6u * 2u]) != 0u);
        REQUIRE (floatBits (hotSwapped[7u * 2u]) != 0u);
    }
    REQUIRE (CompiledGraph::aliveCount() == base);
}

TEST_CASE ("YesDawDeviceHotSwapCheck preserves loop transport through a swap",
           "[h10][device-hot-swap][loop]")
{
    const HotSwapFixture fixture = makeFixture();
    const auto offline = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()));
    REQUIRE (offline.ok());
    REQUIRE (offline.channels == 2);

    auto created = createCoordinator (fixture, DeviceHotSwapFormat { 2001u, kSampleRate, 2u, 4 });
    REQUIRE (created.ok());
    DeviceHotSwapCoordinator& coordinator = *created.coordinator;

    REQUIRE (coordinator.setLoop (5, 13));
    REQUIRE (coordinator.locate (5));
    REQUIRE (coordinator.startCallback() == DeviceHotSwapStatus::Ok);

    constexpr std::uint64_t firstFrames = 11u;
    constexpr std::uint64_t totalFrames = 24u;
    const std::vector<float> beforeSwap = drainCoordinator (coordinator, firstFrames, 4);
    REQUIRE (coordinator.loopEnabled());
    REQUIRE (coordinator.loopStartFrame() == 5);
    REQUIRE (coordinator.loopEndFrame() == 13);
    REQUIRE (coordinator.playheadFrame() == 8);

    REQUIRE (coordinator.stopCallback() == DeviceHotSwapStatus::Ok);
    REQUIRE (coordinator.swapDevice (DeviceHotSwapFormat { 2002u, kSampleRate, 2u, 6 }).ok());
    REQUIRE (coordinator.startCallback() == DeviceHotSwapStatus::Ok);

    const std::vector<float> afterSwap = drainCoordinator (coordinator, totalFrames - firstFrames, 6);
    const std::vector<float> hotSwapped = concatenate (beforeSwap, afterSwap);
    const std::vector<float> expected = loopSlice (offline.interleavedSamples, offline.channels, 5, 13, totalFrames);
    REQUIRE (bitIdentical (hotSwapped, expected));
    REQUIRE (coordinator.loopEnabled());
    REQUIRE (coordinator.loopStartFrame() == 5);
    REQUIRE (coordinator.loopEndFrame() == 13);
}

TEST_CASE ("YesDawDeviceHotSwapCheck preserves stopped state and flags callbacks between devices",
           "[h10][device-hot-swap][stopped]")
{
    const HotSwapFixture fixture = makeFixture();
    auto created = createCoordinator (fixture, DeviceHotSwapFormat { 3001u, kSampleRate, 2u, 4 });
    REQUIRE (created.ok());
    DeviceHotSwapCoordinator& coordinator = *created.coordinator;

    REQUIRE (coordinator.startCallback() == DeviceHotSwapStatus::Ok);
    (void) drainCoordinator (coordinator, 6u, 4);
    REQUIRE (coordinator.playheadFrame() == 6);

    REQUIRE (coordinator.stopTransport());
    const std::vector<float> stoppedBeforeSwap = drainCoordinator (coordinator, 3u, 3);
    REQUIRE (allZero (stoppedBeforeSwap));
    REQUIRE_FALSE (coordinator.isPlaying());
    REQUIRE (coordinator.playheadFrame() == 6);
    REQUIRE (coordinator.stopCallback() == DeviceHotSwapStatus::Ok);

    std::vector<float> left (2u, -1.0f);
    std::vector<float> right (2u, -1.0f);
    float* outputs[2] = { left.data(), right.data() };
    REQUIRE (coordinator.processBlock (outputs, 2, 2) == DeviceHotSwapStatus::CallbackStopped);
    REQUIRE (coordinator.callbackWhileStoppedCount() == 1u);
    REQUIRE (allZero (left));
    REQUIRE (allZero (right));
    REQUIRE (coordinator.playheadFrame() == 6);

    REQUIRE (coordinator.swapDevice (DeviceHotSwapFormat { 3002u, kSampleRate, 2u, 6 }).ok());
    REQUIRE (coordinator.startCallback() == DeviceHotSwapStatus::Ok);
    const std::vector<float> stoppedAfterSwap = drainCoordinator (coordinator, 4u, 4);
    REQUIRE (allZero (stoppedAfterSwap));
    REQUIRE_FALSE (coordinator.isPlaying());
    REQUIRE (coordinator.playheadFrame() == 6);
}

TEST_CASE ("YesDawDeviceHotSwapCheck rejects unsupported swaps without replacing playback",
           "[h10][device-hot-swap][negative-controls]")
{
    const HotSwapFixture fixture = makeFixture();
    const auto offline = renderOfflineProject (
        fixture.project,
        std::span<const DecodedAssetAudio> (fixture.decodedAssets.data(), fixture.decodedAssets.size()));
    REQUIRE (offline.ok());
    REQUIRE (offline.channels == 2);

    auto created = createCoordinator (fixture, DeviceHotSwapFormat { 4001u, kSampleRate, 2u, 4 });
    REQUIRE (created.ok());
    DeviceHotSwapCoordinator& coordinator = *created.coordinator;
    REQUIRE (coordinator.startCallback() == DeviceHotSwapStatus::Ok);

    const std::vector<float> first = drainCoordinator (coordinator, 4u, 4);
    REQUIRE (bitIdentical (first, referenceSlice (offline.interleavedSamples, offline.channels, 0, 4)));
    REQUIRE (coordinator.playheadFrame() == 4);

    auto rejected = coordinator.swapDevice (DeviceHotSwapFormat { 4002u, kSampleRate, 2u, 6 });
    REQUIRE (rejected.status == DeviceHotSwapStatus::CallbackAlreadyActive);
    REQUIRE (coordinator.format().deviceId == 4001u);
    REQUIRE (coordinator.maxBlockSize() == 4);
    REQUIRE (coordinator.playheadFrame() == 4);

    const std::vector<float> afterActiveReject = drainCoordinator (coordinator, 3u, 3);
    REQUIRE (bitIdentical (afterActiveReject, referenceSlice (offline.interleavedSamples, offline.channels, 4, 3)));
    REQUIRE (coordinator.playheadFrame() == 7);
    REQUIRE (coordinator.stopCallback() == DeviceHotSwapStatus::Ok);

    rejected = coordinator.swapDevice (DeviceHotSwapFormat { 4003u, SampleRate { 44100.0 }, 2u, 6 });
    REQUIRE (rejected.status == DeviceHotSwapStatus::UnsupportedSampleRate);
    REQUIRE (coordinator.format().deviceId == 4001u);
    REQUIRE (coordinator.maxBlockSize() == 4);
    REQUIRE (coordinator.playheadFrame() == 7);

    rejected = coordinator.swapDevice (DeviceHotSwapFormat { 4004u, kSampleRate, 1u, 6 });
    REQUIRE (rejected.status == DeviceHotSwapStatus::UnsupportedChannelCount);
    REQUIRE (coordinator.format().deviceId == 4001u);
    REQUIRE (coordinator.maxBlockSize() == 4);
    REQUIRE (coordinator.playheadFrame() == 7);

    rejected = coordinator.swapDevice (DeviceHotSwapFormat { 4005u, kSampleRate, 2u, 0 });
    REQUIRE (rejected.status == DeviceHotSwapStatus::InvalidMaxBlockSize);
    REQUIRE (coordinator.format().deviceId == 4001u);
    REQUIRE (coordinator.maxBlockSize() == 4);
    REQUIRE (coordinator.playheadFrame() == 7);

    REQUIRE (coordinator.startCallback() == DeviceHotSwapStatus::Ok);
    const std::vector<float> afterRejectedSwaps = drainCoordinator (coordinator, 5u, 4);
    REQUIRE (bitIdentical (afterRejectedSwaps, referenceSlice (offline.interleavedSamples, offline.channels, 7, 5)));
}
