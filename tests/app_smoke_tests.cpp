// YES DAW - H11 app smoke gate: bundle load -> action IDs -> playback transport.

#include "ui/UiAppModel.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using yesdaw::engine::Asset;
using yesdaw::engine::AssetContentHash;
using yesdaw::engine::Clip;
using yesdaw::engine::EntityId;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::persistence::ProjectBundleDb;
using yesdaw::ui::UiActionId;
using yesdaw::ui::UiAppLoadStatus;
using yesdaw::ui::UiAppLoadResult;
using yesdaw::ui::UiAppModel;
using yesdaw::ui::UiDecodedAsset;

namespace {

constexpr EntityId idFromLowByte (std::uint8_t low) noexcept
{
    EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return EntityId::fromBytes (bytes);
}

std::filesystem::path makeTempBundlePath (std::string_view label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("yesdaw-app-smoke-" + std::string (label) + "-" + std::to_string (ticks) + ".yesdaw");

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

std::vector<std::uint8_t> assetBytesForId (EntityId id)
{
    return {
        0x59u,
        0x45u,
        0x53u,
        0x44u,
        0x41u,
        0x57u,
        id.bytes.back(),
        static_cast<std::uint8_t> (id.bytes.back() + 17u),
        static_cast<std::uint8_t> (id.bytes.back() + 43u),
    };
}

AssetContentHash hashBytes (std::span<const std::uint8_t> bytes) noexcept
{
    return yesdaw::persistence::detail::sha256Bytes (bytes);
}

void writeBytes (const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    std::error_code ec;
    std::filesystem::create_directories (path.parent_path(), ec);
    REQUIRE (! ec);

    std::ofstream output (path, std::ios::binary | std::ios::trunc);
    REQUIRE (output.good());
    output.write (reinterpret_cast<const char*> (bytes.data()), static_cast<std::streamsize> (bytes.size()));
    output.close();
    REQUIRE (output.good());
}

Project makeSmokeProject()
{
    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };

    Asset asset;
    asset.id = idFromLowByte (2);
    const std::vector<std::uint8_t> bytes = assetBytesForId (asset.id);
    asset.contentHash = hashBytes (std::span<const std::uint8_t> (bytes.data(), bytes.size()));
    asset.frames = 16;
    asset.sampleRate = project.sampleRate;
    asset.channels = 1;

    Clip clip;
    clip.id = idFromLowByte (3);
    clip.assetId = asset.id;
    clip.trackId = idFromLowByte (4);
    clip.timelineStart = 0;
    clip.timelineLength = 8;
    clip.srcOffset = 0;
    clip.srcLen = 8;
    clip.gain = 1.0f;
    clip.fadeIn = 0;
    clip.fadeOut = 0;
    clip.timeBase = TimeBase::SampleLocked;

    project.assets = { asset };
    Track track;
    track.id = clip.trackId;
    track.strip.name = "Audio 1";
    project.tracks = { track };
    project.clips = { clip };
    REQUIRE (project.hasValidAssetClipIndirection());
    return project;
}

void writeProjectAssetFiles (const std::filesystem::path& bundlePath, const Project& project)
{
    for (const Asset& asset : project.assets)
    {
        const std::vector<std::uint8_t> bytes = assetBytesForId (asset.id);
        REQUIRE (hashBytes (std::span<const std::uint8_t> (bytes.data(), bytes.size())) == asset.contentHash);
        writeBytes (bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash),
                    std::span<const std::uint8_t> (bytes.data(), bytes.size()));
    }
}

UiDecodedAsset makeDecodedAsset (const Asset& asset)
{
    UiDecodedAsset decoded;
    decoded.assetId = asset.id;
    decoded.sampleRate = asset.sampleRate;
    decoded.frames = asset.frames;
    decoded.channels = asset.channels;
    decoded.interleavedSamples = {
        0.10f, -0.20f, 0.30f, -0.40f,
        0.50f, -0.60f, 0.70f, -0.80f,
        0.90f, -1.00f, 0.80f, -0.70f,
        0.60f, -0.50f, 0.40f, -0.30f,
    };
    return decoded;
}

} // namespace

TEST_CASE ("H11 app model loads a Project bundle and drives transport through action ids",
           "[ui][app][smoke]")
{
    const std::filesystem::path bundlePath = makeTempBundlePath ("load-transport");
    const Project project = makeSmokeProject();

    {
        ProjectBundleDb db;
        REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());
        REQUIRE (db.writeProjectSnapshot (project).ok());
        writeProjectAssetFiles (bundlePath, project);
    }

    UiAppModel app;
    REQUIRE_FALSE (app.dispatch (UiActionId::TransportPlay).dispatched);
    REQUIRE_FALSE (app.context().projectLoaded);

    UiDecodedAsset decoded = makeDecodedAsset (project.assets.front());
    const UiAppLoadResult loaded = app.loadProjectBundle (
        bundlePath,
        std::span<const UiDecodedAsset> (&decoded, 1));

    REQUIRE (loaded.ok());
    REQUIRE (loaded.status == UiAppLoadStatus::Ok);
    REQUIRE (app.bundlePath() == bundlePath);
    REQUIRE (app.project().id == project.id);
    REQUIRE (app.playbackReady());
    REQUIRE (app.context().projectLoaded);
    REQUIRE_FALSE (app.context().isPlaying);
    REQUIRE_FALSE (app.context().loopEnabled);
    REQUIRE (app.context().playheadFrame == 0);
    REQUIRE (app.registry().stateFor (UiActionId::TransportPlay, app.context()).enabled);

    REQUIRE (app.dispatch (UiActionId::TransportPlay).dispatched);
    REQUIRE (app.context().isPlaying);

    REQUIRE (app.dispatch (UiActionId::TransportLocateStart).dispatched);
    REQUIRE (app.context().playheadFrame == 0);

    REQUIRE (app.dispatch (UiActionId::TransportToggleLoop).dispatched);
    REQUIRE (app.context().loopEnabled);

    REQUIRE (app.dispatch (UiActionId::TransportStop).dispatched);
    REQUIRE_FALSE (app.context().isPlaying);
    REQUIRE (app.context().commandDispatchCount == 4);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}
