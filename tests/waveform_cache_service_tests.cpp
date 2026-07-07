#include "persistence/WaveformPeakCache.h"
#include "ui/WaveformPeakService.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <memory>
#include <span>
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
using yesdaw::ui::WaveformPeakService;

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

std::vector<float> makeChannelMajorSamples()
{
    return {
        -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 0.75f, -0.25f, 0.25f,
        0.25f, -0.25f, 0.5f, -0.5f, 0.75f, -0.75f, 1.0f, -1.0f,
    };
}

std::vector<float> makeDifferentChannelMajorSamples()
{
    return {
        0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f,
        -0.125f, -0.25f, -0.375f, -0.5f, -0.625f, -0.75f, -0.875f, -1.0f,
    };
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

} // namespace

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
