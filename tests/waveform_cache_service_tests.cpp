#include "persistence/WaveformPeakCache.h"
#include "ui/WaveformPeakService.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
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
