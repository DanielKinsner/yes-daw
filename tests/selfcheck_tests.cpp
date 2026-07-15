// YES DAW — H17 CP1 self-check render gate.
//
// The committed .yesdaw fixtures ship stub (9-byte) asset files, not audio, so the render happy-path
// can't be proven against them. This test GENERATES a real bundle from the sine WAV fixture (exactly
// as tests/bundle_render_tests.cpp builds bundles) and asserts yesdaw::app::runSelfCheck renders it.
// Plus a negative: a path that is not a bundle is rejected.

#include "app/SelfCheck.h"
#include "engine/Project.h"
#include "persistence/ProjectBundle.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

using yesdaw::engine::Asset;
using yesdaw::engine::Clip;
using yesdaw::engine::EntityId;
using yesdaw::engine::Project;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Track;
using yesdaw::persistence::AssetImportRequest;
using yesdaw::persistence::ProjectBundleDb;

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
                               / ("yesdaw-" + std::string (label) + "-" + std::to_string (ticks) + ".yesdaw");

    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

// Build a real .yesdaw bundle: import the sine WAV fixture as a mono Asset, place one clip on a
// track, and persist. Returns the bundle path.
std::filesystem::path buildSineBundle()
{
    const auto bundlePath = makeTempBundlePath ("selfcheck-render");
    const std::filesystem::path fixturePath { YESDAW_WAV_FIXTURE_PATH };

    ProjectBundleDb db;
    REQUIRE (ProjectBundleDb::openOrCreateBundle (bundlePath, db).ok());

    Asset imported;
    const AssetImportRequest import { fixturePath, idFromLowByte (10), 4096, SampleRate { 48000.0 }, 1 };
    REQUIRE (db.importAssetBytes (import, imported).ok());

    Clip clip;
    clip.id = idFromLowByte (20);
    clip.assetId = imported.id;
    clip.trackId = idFromLowByte (30);
    clip.timelineStart = 0;
    clip.timelineLength = 256;
    clip.srcOffset = 0;
    clip.srcLen = 256;
    clip.gain = 1.0f;
    clip.fadeIn = 0;
    clip.fadeOut = 0;
    clip.timeBase = TimeBase::SampleLocked;

    Project project;
    project.id = idFromLowByte (1);
    project.sampleRate = SampleRate { 48000.0 };
    project.assets.push_back (imported);

    Track track;
    track.id = clip.trackId;
    track.strip.name = "Audio 1";
    project.tracks.push_back (track);
    project.clips.push_back (clip);

    REQUIRE (project.hasValidAssetClipIndirection());
    REQUIRE (db.writeProjectSnapshot (project).ok());

    return bundlePath;
}

} // namespace

TEST_CASE ("selfcheck renders a real generated bundle", "[selfcheck][render][h17]")
{
    const std::filesystem::path bundlePath = buildSineBundle();

    const yesdaw::app::SelfCheckResult result = yesdaw::app::runSelfCheck (bundlePath);

    INFO ("selfcheck message: " << result.message);
    REQUIRE (result.ok);
    CHECK (result.assetCount == 1u);
    CHECK (result.clipCount == 1u);
    CHECK (result.renderedFrames > 0u);
    CHECK (result.renderedChannels >= 1u);
    // Slice 3: the render was exported to WAV and re-imported bit-exact (part of ok, asserted here).
    CHECK (result.exportedFrames == result.renderedFrames);
    CHECK (result.exportedFrames > 0u);

    std::error_code ec;
    std::filesystem::remove_all (bundlePath, ec);
}

TEST_CASE ("selfcheck rejects a path that is not a bundle", "[selfcheck][negative][h17]")
{
    const yesdaw::app::SelfCheckResult result =
        yesdaw::app::runSelfCheck (std::filesystem::temp_directory_path());

    CHECK_FALSE (result.ok);
}

TEST_CASE ("verify-wav round-trips a canonical float32 wav bit-exact", "[selfcheck][verifywav][h17]")
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path wavPath =
        std::filesystem::temp_directory_path() / ("yesdaw-verifywav-test-" + std::to_string (ticks) + ".wav");

    const std::vector<float> samples { 0.0f, 0.25f, -0.5f, 0.75f, -1.0f, 1.0f, 0.125f, -0.125f };
    const std::uint16_t channels = 2;
    const std::uint64_t frames = static_cast<std::uint64_t> (samples.size()) / channels; // 4

    REQUIRE (yesdaw::io::writeFloat32WavFile (
                 wavPath, SampleRate { 48000.0 }, channels, frames,
                 std::span<const float> (samples.data(), samples.size())).ok());

    const yesdaw::app::WavVerifyResult r = yesdaw::app::verifyWavRoundTrip (wavPath);
    INFO ("verify message: " << r.message);
    REQUIRE (r.ok);
    CHECK (r.channels == channels);
    CHECK (r.frames == frames);
    CHECK (r.sampleRateHz == 48000.0);

    std::error_code ec;
    std::filesystem::remove (wavPath, ec);
}

TEST_CASE ("verify-wav rejects a missing file", "[selfcheck][verifywav][negative][h17]")
{
    const yesdaw::app::WavVerifyResult r =
        yesdaw::app::verifyWavRoundTrip (std::filesystem::temp_directory_path() / "yesdaw-verifywav-absent-xyz.wav");
    CHECK_FALSE (r.ok);
}
