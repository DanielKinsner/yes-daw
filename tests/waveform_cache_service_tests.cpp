#include "persistence/WaveformPeakCache.h"
#include "io/WavFile.h"
#include "ui/UiAppModel.h"
#include "ui/WaveformColumns.h"
#include "ui/WaveformPeakService.h"

#if YESDAW_WAVEFORM_CACHE_PAINT_TESTS
#include "ui/TimelineCanvas.h"
#endif

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::EntityId;
using yesdaw::engine::SampleRate;
using yesdaw::persistence::buildWaveformPeakCache;
using yesdaw::persistence::waveformPeakCachePathForHash;
using yesdaw::persistence::writeWaveformPeakCache;
#if YESDAW_WAVEFORM_CACHE_PAINT_TESTS
using yesdaw::ui::Clip;
using yesdaw::ui::TimelineCanvasClipStyle;
using yesdaw::ui::TimelineCanvasPaintStats;
using yesdaw::ui::TimelineCanvasState;
using yesdaw::ui::TimelineCanvasTrack;
#endif
using yesdaw::ui::UiAppModel;
using yesdaw::ui::UiDecodedAsset;
using yesdaw::ui::WaveformColumnViewport;
using yesdaw::ui::WaveformPeakService;
using yesdaw::ui::computeWaveformColumns;
using yesdaw::ui::interleavedToChannelMajor;
#if YESDAW_WAVEFORM_CACHE_PAINT_TESTS
using yesdaw::ui::paintTimelineCanvas;
#endif

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

constexpr AssetContentHash hashFromSeed (std::uint8_t seed) noexcept
{
    AssetContentHash hash;
    for (std::size_t i = 0; i < hash.bytes.size(); ++i)
        hash.bytes[i] = static_cast<std::uint8_t> (seed + i);
    return hash;
}

Asset makeAsset()
{
    Asset asset;
    asset.id = idFromLowByte (0x31);
    asset.contentHash = hashFromSeed (0x40);
    asset.frames = 8;
    asset.sampleRate = SampleRate { 48000.0 };
    asset.channels = 2;
    return asset;
}

Asset makeColumnAsset()
{
    Asset asset;
    asset.id = idFromLowByte (0x33);
    asset.contentHash = hashFromSeed (0x90);
    asset.frames = 64;
    asset.sampleRate = SampleRate { 16.0 };
    asset.channels = 1;
    return asset;
}

std::vector<float> makeChannelMajorSamples()
{
    return {
        -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 0.75f, -0.25f, 0.25f,
        0.25f, -0.25f, 0.5f, -0.5f, 0.75f, -0.75f, 1.0f, -1.0f,
    };
}

std::vector<float> makeColumnSamples()
{
    std::vector<float> samples;
    samples.reserve (64u);
    for (std::uint64_t frame = 0; frame < 64u; ++frame)
    {
        const float value = static_cast<float> (frame + 1u) / 64.0f;
        samples.push_back ((frame % 2u) == 0u ? -value : value);
    }

    return samples;
}

std::vector<float> makeDifferentChannelMajorSamples()
{
    return {
        0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f,
        -0.125f, -0.25f, -0.375f, -0.5f, -0.625f, -0.75f, -0.875f, -1.0f,
    };
}

std::vector<float> channelMajorToInterleaved (std::span<const float> channelMajor,
                                              std::uint64_t frames,
                                              std::uint16_t channels)
{
    std::vector<float> interleaved (
        static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels),
        0.0f);

    for (std::uint64_t frame = 0; frame < frames; ++frame)
        for (std::uint16_t channel = 0; channel < channels; ++channel)
            interleaved[static_cast<std::size_t> (frame) * channels + channel] =
                channelMajor[static_cast<std::size_t> (channel) * static_cast<std::size_t> (frames)
                             + static_cast<std::size_t> (frame)];

    return interleaved;
}

std::filesystem::path makeTempPath (std::string_view label, std::string_view extension)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
         / ("yesdaw-waveform-cache-" + std::string (label) + "-" + std::to_string (ticks)
            + std::string (extension));
}

UiDecodedAsset makeDecodedImport()
{
    UiDecodedAsset decoded;
    decoded.sampleRate = SampleRate { 48000.0 };
    decoded.frames = 8;
    decoded.channels = 1;
    decoded.interleavedSamples = {
        0.10f,
        0.25f,
        0.40f,
        0.55f,
        0.70f,
        0.85f,
        1.00f,
        0.50f,
    };
    return decoded;
}

std::vector<std::uint8_t> readFileBytes (const std::filesystem::path& path)
{
    std::ifstream input (path, std::ios::binary);
    REQUIRE (input);

    return std::vector<std::uint8_t> (std::istreambuf_iterator<char> (input),
                                      std::istreambuf_iterator<char>());
}

std::shared_ptr<const yesdaw::persistence::WaveformPeakCache> waitForReady (
    const WaveformPeakService& service,
    const AssetContentHash& hash)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds (3);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (auto ready = service.tryGetReady (hash))
            return ready;

        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    return {};
}

#if YESDAW_WAVEFORM_CACHE_PAINT_TESTS
TimelineCanvasState makePaintState (
    const Clip* clips,
    const TimelineCanvasClipStyle* styles,
    int clipCount)
{
    static const TimelineCanvasTrack kTrack {
        "Audio 1",
        juce::Colour { 0xff7c5cff },
        0.0f
    };

    TimelineCanvasState state;
    state.tracks = &kTrack;
    state.trackCount = 1;
    state.clips = clips;
    state.clipStyles = styles;
    state.clipCount = clipCount;
    state.totalSeconds = 2.0;
    state.playheadSeconds = 0.0;
    state.viewport.scrollSeconds = 0.0;
    state.viewport.pixelsPerSecond = 160.0;
    return state;
}
#endif

} // namespace

TEST_CASE ("H16 CP2c interleaved audio converts exactly to channel-major storage", "[ui][waveform-cache]")
{
    constexpr std::uint64_t frames = 4;
    constexpr std::uint16_t channels = 3;
    const std::vector<float> interleaved {
        10.0f, 20.0f, 30.0f,
        11.0f, 21.0f, 31.0f,
        12.0f, 22.0f, 32.0f,
        13.0f, 23.0f, 33.0f,
    };

    const std::vector<float> expectedChannelMajor {
        10.0f, 11.0f, 12.0f, 13.0f,
        20.0f, 21.0f, 22.0f, 23.0f,
        30.0f, 31.0f, 32.0f, 33.0f,
    };

    const std::vector<float> channelMajor = interleavedToChannelMajor (interleaved, frames, channels);
    REQUIRE (channelMajor == expectedChannelMajor);
    REQUIRE (channelMajorToInterleaved (channelMajor, frames, channels) == interleaved);

    std::vector<float> swapped = channelMajor;
    std::swap_ranges (swapped.begin(),
                      swapped.begin() + static_cast<std::ptrdiff_t> (frames),
                      swapped.begin() + static_cast<std::ptrdiff_t> (frames));
    REQUIRE_FALSE (channelMajorToInterleaved (swapped, frames, channels) == interleaved);
}

TEST_CASE ("H16 CP3 waveform columns choose the peak tier from pixels per second", "[ui][waveform-cache]")
{
    const Asset asset = makeColumnAsset();
    const std::vector<float> samples = makeColumnSamples();
    const auto built = buildWaveformPeakCache (asset, std::span<const float> (samples.data(), samples.size()), 4);
    INFO (built.message);
    REQUIRE (built.ok());
    REQUIRE (built.cache.tiers.size() == 2u);

    WaveformColumnViewport detailedViewport;
    detailedViewport.sourceFrameCount = 16;
    detailedViewport.sampleRate = asset.sampleRate.hz;
    detailedViewport.pixelsPerSecond = 4.0;
    detailedViewport.widthPixels = 4;
    const auto detailed = computeWaveformColumns (built.cache, detailedViewport);
    REQUIRE (detailed.tierIndex == 0u);
    REQUIRE (detailed.framesPerPeak == 4u);
    REQUIRE (detailed.columns.size() == 4u);
    for (std::size_t i = 0; i < detailed.columns.size(); ++i)
    {
        const auto& peak = built.cache.tiers[0].peaks[i];
        REQUIRE (detailed.columns[i].min == peak.min);
        REQUIRE (detailed.columns[i].max == peak.max);
        REQUIRE (detailed.columns[i].rms == peak.rms);
    }

    WaveformColumnViewport foldedViewport;
    foldedViewport.sourceFrameCount = asset.frames;
    foldedViewport.sampleRate = asset.sampleRate.hz;
    foldedViewport.pixelsPerSecond = 0.25;
    foldedViewport.widthPixels = 1;
    const auto folded = computeWaveformColumns (built.cache, foldedViewport);
    REQUIRE (folded.tierIndex == 1u);
    REQUIRE (folded.framesPerPeak == 64u);
    REQUIRE (folded.columns.size() == 1u);
    REQUIRE (folded.columns.front().min == built.cache.tiers[1].peaks.front().min);
    REQUIRE (folded.columns.front().max == built.cache.tiers[1].peaks.front().max);
    REQUIRE (folded.columns.front().rms == built.cache.tiers[1].peaks.front().rms);
}

TEST_CASE ("H16 CP3 waveform columns are deterministic for source-frame viewports", "[ui][waveform-cache]")
{
    const Asset asset = makeColumnAsset();
    const std::vector<float> samples = makeColumnSamples();
    const auto built = buildWaveformPeakCache (asset, std::span<const float> (samples.data(), samples.size()), 4);
    INFO (built.message);
    REQUIRE (built.ok());

    WaveformColumnViewport viewport;
    viewport.sourceFrameOffset = 8;
    viewport.sourceFrameCount = 8;
    viewport.sampleRate = asset.sampleRate.hz;
    viewport.pixelsPerSecond = 4.0;
    viewport.widthPixels = 2;

    const auto first = computeWaveformColumns (built.cache, viewport);
    const auto second = computeWaveformColumns (built.cache, viewport);
    REQUIRE (first == second);
    REQUIRE (first.tierIndex == 0u);
    REQUIRE (first.columns.size() == 2u);

    for (std::size_t i = 0; i < first.columns.size(); ++i)
    {
        const auto& peak = built.cache.tiers[0].peaks[i + 2u];
        REQUIRE (first.columns[i].min == peak.min);
        REQUIRE (first.columns[i].max == peak.max);
        REQUIRE (first.columns[i].rms == peak.rms);
    }
}

TEST_CASE ("H16 CP2a waveform service builds and publishes on its worker thread", "[ui][waveform-cache]")
{
    const auto bundlePath = std::filesystem::temp_directory_path()
                          / "yesdaw-waveform-cache-service-off-thread";
    std::filesystem::remove_all (bundlePath);

    const Asset asset = makeAsset();
    const std::vector<float> samples = makeChannelMajorSamples();
    const auto expected = buildWaveformPeakCache (asset, std::span<const float> (samples.data(), samples.size()));
    INFO (expected.message);
    REQUIRE (expected.ok());

    WaveformPeakService service;
    service.start (bundlePath);

    const std::thread::id callerThread = std::this_thread::get_id();
    REQUIRE (service.tryGetReady (asset.contentHash) == nullptr);

    service.requestBuild (asset, samples);

    const auto ready = waitForReady (service, asset.contentHash);
    REQUIRE (ready != nullptr);
    REQUIRE (*ready == expected.cache);
    REQUIRE (service.buildCount() == 1u);
    REQUIRE (service.workerThreadId() != std::thread::id {});
    REQUIRE (service.lastBuildThreadId() == service.workerThreadId());
    REQUIRE (service.lastBuildThreadId() != callerThread);
    REQUIRE_FALSE (service.builtOnForbiddenThread());

    std::filesystem::remove_all (bundlePath);
}

TEST_CASE ("H16 CP2c waveform service restart drops old derived state", "[ui][waveform-cache]")
{
    const auto firstBundle = makeTempPath ("restart-first", ".yesdaw");
    const auto secondBundle = makeTempPath ("restart-second", ".yesdaw");
    std::filesystem::remove_all (firstBundle);
    std::filesystem::remove_all (secondBundle);

    const Asset firstAsset = makeAsset();
    Asset secondAsset = firstAsset;
    secondAsset.id = idFromLowByte (0x32);
    secondAsset.contentHash = hashFromSeed (0x80);

    WaveformPeakService service;
    service.start (firstBundle);
    service.requestBuild (firstAsset, makeChannelMajorSamples());
    REQUIRE (waitForReady (service, firstAsset.contentHash) != nullptr);
    REQUIRE (service.buildCount() == 1u);

    service.start (secondBundle);
    REQUIRE (service.tryGetReady (firstAsset.contentHash) == nullptr);
    REQUIRE (service.buildCount() == 0u);

    service.requestBuild (secondAsset, makeDifferentChannelMajorSamples());
    REQUIRE (waitForReady (service, secondAsset.contentHash) != nullptr);
    REQUIRE (service.buildCount() == 1u);

    std::filesystem::remove_all (firstBundle);
    std::filesystem::remove_all (secondBundle);
}

TEST_CASE ("H16 CP2c app model import enqueues waveform cache build", "[ui][waveform-cache]")
{
    const auto bundlePath = makeTempPath ("ui-import-ready", ".yesdaw");
    const auto sourcePath = makeTempPath ("ui-import-source", ".wav");
    std::filesystem::remove_all (bundlePath);
    std::filesystem::remove (sourcePath);

    UiDecodedAsset decoded = makeDecodedImport();
    const auto writeSource = yesdaw::io::writeFloat32WavFile (
        sourcePath,
        decoded.sampleRate,
        decoded.channels,
        decoded.frames,
        std::span<const float> (decoded.interleavedSamples.data(), decoded.interleavedSamples.size()));
    INFO (writeSource.message);
    REQUIRE (writeSource.ok());

    {
        UiAppModel app;
        REQUIRE (app.createProjectBundle (bundlePath).ok());

        UiDecodedAsset importDecoded = decoded;
        const auto imported = app.importAudioFile (sourcePath, std::move (importDecoded));
        INFO ("import status=" << static_cast<int> (imported.status));
        INFO ("bundle status=" << static_cast<int> (imported.bundleResult.status));
        INFO ("bundle message=" << imported.bundleResult.message);
        INFO ("playback status=" << static_cast<int> (imported.playbackStatus));
        REQUIRE (imported.ok());
        REQUIRE (app.project().assets.size() == 1u);

        const Asset& asset = app.project().assets.front();
        const std::vector<float> channelMajor =
            interleavedToChannelMajor (decoded.interleavedSamples, decoded.frames, decoded.channels);
        const auto expected = buildWaveformPeakCache (asset,
                                                      std::span<const float> (channelMajor.data(),
                                                                              channelMajor.size()));
        INFO (expected.message);
        REQUIRE (expected.ok());

        const auto ready = waitForReady (app.waveformService(), asset.contentHash);
        REQUIRE (ready != nullptr);
        REQUIRE (*ready == expected.cache);
    }

    std::filesystem::remove (sourcePath);
    std::filesystem::remove_all (bundlePath);
}

TEST_CASE ("H16 CP2b waveform service reloads existing peak files without rebuilding", "[ui][waveform-cache]")
{
    const auto bundlePath = std::filesystem::temp_directory_path()
                          / "yesdaw-waveform-cache-service-reload";
    std::filesystem::remove_all (bundlePath);

    const Asset asset = makeAsset();
    const std::vector<float> prewrittenSamples = makeChannelMajorSamples();
    const auto prewritten =
        buildWaveformPeakCache (asset, std::span<const float> (prewrittenSamples.data(), prewrittenSamples.size()));
    INFO (prewritten.message);
    REQUIRE (prewritten.ok());

    const auto writeResult = writeWaveformPeakCache (bundlePath, prewritten.cache);
    INFO (writeResult.message);
    REQUIRE (writeResult.ok());

    const auto peakPath = waveformPeakCachePathForHash (bundlePath, asset.contentHash);
    const auto prewrittenBytes = readFileBytes (peakPath);

    const std::vector<float> requestSamples = makeDifferentChannelMajorSamples();
    const auto rebuilt =
        buildWaveformPeakCache (asset, std::span<const float> (requestSamples.data(), requestSamples.size()));
    INFO (rebuilt.message);
    REQUIRE (rebuilt.ok());
    REQUIRE_FALSE (rebuilt.cache == prewritten.cache);

    WaveformPeakService service;
    service.start (bundlePath);
    service.requestBuild (asset, requestSamples);

    const auto ready = waitForReady (service, asset.contentHash);
    REQUIRE (ready != nullptr);
    REQUIRE (*ready == prewritten.cache);
    REQUIRE (readFileBytes (peakPath) == prewrittenBytes);
    REQUIRE (service.buildCount() == 0u);
    REQUIRE (service.lastBuildThreadId() == std::thread::id {});

    std::filesystem::remove_all (bundlePath);
}

TEST_CASE ("H16 CP2b waveform service delete-file control rebuilds once", "[ui][waveform-cache]")
{
    const auto bundlePath = std::filesystem::temp_directory_path()
                          / "yesdaw-waveform-cache-service-delete-control";
    std::filesystem::remove_all (bundlePath);

    const Asset asset = makeAsset();
    const std::vector<float> prewrittenSamples = makeChannelMajorSamples();
    const auto prewritten =
        buildWaveformPeakCache (asset, std::span<const float> (prewrittenSamples.data(), prewrittenSamples.size()));
    INFO (prewritten.message);
    REQUIRE (prewritten.ok());

    const auto writeResult = writeWaveformPeakCache (bundlePath, prewritten.cache);
    INFO (writeResult.message);
    REQUIRE (writeResult.ok());

    const auto peakPath = waveformPeakCachePathForHash (bundlePath, asset.contentHash);
    std::error_code ec;
    REQUIRE (std::filesystem::remove (peakPath, ec));
    REQUIRE_FALSE (ec);

    const std::vector<float> requestSamples = makeDifferentChannelMajorSamples();
    const auto rebuilt =
        buildWaveformPeakCache (asset, std::span<const float> (requestSamples.data(), requestSamples.size()));
    INFO (rebuilt.message);
    REQUIRE (rebuilt.ok());
    REQUIRE_FALSE (rebuilt.cache == prewritten.cache);

    WaveformPeakService service;
    service.start (bundlePath);
    service.requestBuild (asset, requestSamples);

    const auto ready = waitForReady (service, asset.contentHash);
    REQUIRE (ready != nullptr);
    REQUIRE (*ready == rebuilt.cache);
    REQUIRE (service.buildCount() == 1u);
    REQUIRE (service.lastBuildThreadId() == service.workerThreadId());

    std::filesystem::remove_all (bundlePath);
}

TEST_CASE ("H16 CP2a waveform service negative control flags caller-thread builds", "[ui][waveform-cache]")
{
    const auto bundlePath = std::filesystem::temp_directory_path()
                          / "yesdaw-waveform-cache-service-negative-control";
    std::filesystem::remove_all (bundlePath);

    const Asset asset = makeAsset();
    const std::vector<float> samples = makeChannelMajorSamples();

    WaveformPeakService service;
    service.start (bundlePath);
    service.registerPaintThread (std::this_thread::get_id());

    const auto ready = service.forceSynchronousBuildOnCallerThread (asset, samples);
    REQUIRE (ready != nullptr);
    REQUIRE (service.builtOnForbiddenThread());
    REQUIRE (service.lastBuildThreadId() == std::this_thread::get_id());

    std::filesystem::remove_all (bundlePath);
}

#if YESDAW_WAVEFORM_CACHE_PAINT_TESTS
TEST_CASE ("H16 CP2d timeline paint observes ready and not-ready waveform cache state", "[ui][waveform-cache]")
{
    const Asset asset = makeAsset();
    const std::vector<float> samples = makeChannelMajorSamples();
    const auto expected = buildWaveformPeakCache (asset, std::span<const float> (samples.data(), samples.size()));
    INFO (expected.message);
    REQUIRE (expected.ok());
    const auto ready = std::make_shared<const yesdaw::persistence::WaveformPeakCache> (expected.cache);

    const std::array<Clip, 2> clips {{
        { 0, 0, 0.0, 0.75 },
        { 1, 0, 0.8, 0.75 },
    }};
    const std::array<TimelineCanvasClipStyle, 2> styles {{
        { juce::Colour { 0xff7c5cff }, 0.65f },
        { juce::Colour { 0xff26d7c9 }, 0.65f },
    }};

    TimelineCanvasState state = makePaintState (clips.data(), styles.data(), static_cast<int> (clips.size()));
    int lookupCount = 0;
    state.waveformCacheLookup = [&lookupCount, ready] (int clipId)
        -> std::shared_ptr<const yesdaw::persistence::WaveformPeakCache>
    {
        ++lookupCount;
        if (clipId == 0)
            return ready;

        return {};
    };

    juce::Image image (juce::Image::ARGB, 640, 160, true);
    juce::Graphics graphics (image);
    const TimelineCanvasPaintStats stats = paintTimelineCanvas (graphics, image.getBounds(), state);

    REQUIRE (lookupCount == 2);
    REQUIRE (stats.visibleClips == 2);
    REQUIRE (stats.readyWaveformClips == 1);
    REQUIRE (stats.pendingWaveformClips == 1);
}

TEST_CASE ("H16 CP2d timeline paint lookup does not build on the paint thread", "[ui][waveform-cache]")
{
    const auto bundlePath = std::filesystem::temp_directory_path()
                          / "yesdaw-waveform-cache-paint-read-only";
    std::filesystem::remove_all (bundlePath);

    const Asset asset = makeAsset();
    const std::vector<float> samples = makeChannelMajorSamples();

    WaveformPeakService service;
    service.start (bundlePath);
    service.requestBuild (asset, samples);
    REQUIRE (waitForReady (service, asset.contentHash) != nullptr);
    REQUIRE (service.buildCount() == 1u);

    service.registerPaintThread (std::this_thread::get_id());

    const std::array<Clip, 1> clips {{ { 0, 0, 0.0, 0.75 } }};
    const std::array<TimelineCanvasClipStyle, 1> styles {{ { juce::Colour { 0xff7c5cff }, 0.65f } }};
    TimelineCanvasState state = makePaintState (clips.data(), styles.data(), static_cast<int> (clips.size()));
    state.waveformCacheLookup = [&service, &asset] (int)
        -> std::shared_ptr<const yesdaw::persistence::WaveformPeakCache>
    {
        return service.tryGetReady (asset.contentHash);
    };

    juce::Image image (juce::Image::ARGB, 640, 160, true);
    juce::Graphics graphics (image);
    const TimelineCanvasPaintStats stats = paintTimelineCanvas (graphics, image.getBounds(), state);

    REQUIRE (stats.visibleClips == 1);
    REQUIRE (stats.readyWaveformClips == 1);
    REQUIRE (stats.pendingWaveformClips == 0);
    REQUIRE (service.buildCount() == 1u);
    REQUIRE_FALSE (service.builtOnForbiddenThread());

    std::filesystem::remove_all (bundlePath);
}
#endif
