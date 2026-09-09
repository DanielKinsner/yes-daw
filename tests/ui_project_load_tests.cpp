#include "ui/MainComponentInternal.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <type_traits>

namespace {

std::filesystem::path loadTestDirectory()
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-prepared-load-" + std::to_string (
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

void seedAudioBundle (const std::filesystem::path& directory)
{
    const auto wav = directory / "source.wav";
    const std::vector<float> samples (4096, 0.25f);
    REQUIRE (yesdaw::io::writeFloat32WavFile (
        wav, yesdaw::engine::SampleRate { 48000.0 }, 1, samples.size(), samples).ok());
    auto decoded = yesdaw::ui::shell::decodeProjectWav (wav);
    REQUIRE (decoded.has_value());
    yesdaw::ui::UiAppModel source;
    REQUIRE (source.createProjectBundle (directory / "audio.yesdaw").ok());
    REQUIRE (source.importAudioFile (wav, std::move (*decoded)).ok());
    REQUIRE (source.saveProjectBundle().ok());
}

} // namespace

TEST_CASE ("prepared project load owns decoded audio after the shell result is destroyed",
           "[ui][input][project-load][prepared-project-load]")
{
    static_assert (! std::is_copy_constructible_v<yesdaw::ui::UiPreparedProjectBundle>);
    const auto directory = loadTestDirectory();
    seedAudioBundle (directory);
    yesdaw::ui::UiAppModel model;
    {
        auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (directory / "audio.yesdaw");
        REQUIRE (stored.assets.has_value());
        REQUIRE (stored.assets->size() == 1u);
        REQUIRE (stored.prepared.project().clips.size() == 1u);
        REQUIRE (model.loadPreparedProjectBundle (std::move (stored.prepared),
                                                  std::move (*stored.assets)).ok());
    }
    REQUIRE (model.bundlePath() == directory / "audio.yesdaw");
    REQUIRE (model.playbackReady());
    REQUIRE_FALSE (model.hasUnsavedChanges());
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TransportPlay).dispatched);
    const auto rendered = model.renderPlaybackFrames (512, 128);
    REQUIRE (rendered.size() == 1024u);
    REQUIRE (std::all_of (rendered.begin(), rendered.end(), [] (float sample) { return std::isfinite (sample); }));
    REQUIRE (std::any_of (rendered.begin(), rendered.end(), [] (float sample) { return std::abs (sample) > 0.1f; }));
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TransportStop).dispatched);
    REQUIRE (model.addAudioTrack().dispatched);
    REQUIRE (model.saveProjectBundle().ok());
    yesdaw::ui::UiAppModel reopened;
    REQUIRE (reopened.openProjectBundle (directory / "audio.yesdaw").ok());
    REQUIRE (reopened.project().tracks.size() == 2u);
}

TEST_CASE ("prepared load failure retains the editable original session",
           "[ui][input][project-load][prepared-project-load]")
{
    const auto directory = loadTestDirectory();
    seedAudioBundle (directory);
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "original.yesdaw").ok());
    REQUIRE (model.addAudioTrack().dispatched);
    const auto originalId = model.project().id;
    const auto dispatches = model.context().commandDispatchCount;
    SECTION ("an unopened wrapper cannot be adopted")
    {
        const auto result = model.loadPreparedProjectBundle ({}, {});
        REQUIRE (result.status == yesdaw::ui::UiAppLoadStatus::BundleOpenFailed);
    }
    SECTION ("playback refuses a bad decoded asset before attachment")
    {
        auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (directory / "audio.yesdaw");
        REQUIRE (stored.assets.has_value());
        stored.assets->front().interleavedSamples.front() = std::numeric_limits<float>::quiet_NaN();
        const auto result = model.loadPreparedProjectBundle (std::move (stored.prepared),
                                                              std::move (*stored.assets));
        REQUIRE (result.status == yesdaw::ui::UiAppLoadStatus::PlaybackBuildFailed);
    }
    REQUIRE (model.bundlePath() == directory / "original.yesdaw");
    REQUIRE (model.project().id == originalId);
    REQUIRE (model.context().commandDispatchCount == dispatches);
    REQUIRE (model.hasUnsavedChanges());
    REQUIRE (model.addAudioTrack().dispatched);
    REQUIRE (model.saveProjectBundle().ok());
    yesdaw::ui::UiAppModel reopened;
    REQUIRE (reopened.openProjectBundle (directory / "original.yesdaw").ok());
    REQUIRE (reopened.project().tracks.size() == 3u);
}

TEST_CASE ("prepared shell open retains missing and corrupt asset integrity refusal",
           "[ui][input][project-load][prepared-project-load]")
{
    const auto directory = loadTestDirectory();
    seedAudioBundle (directory);
    const auto bundle = directory / "audio.yesdaw";
    std::filesystem::path assetPath;
    {
        auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (bundle);
        REQUIRE (stored.assets.has_value());
        assetPath = yesdaw::persistence::storedAssetPathForHash (
            bundle, stored.prepared.project().assets.front().contentHash);
    }
    SECTION ("missing bytes")
    {
        REQUIRE (std::filesystem::remove (assetPath));
    }
    SECTION ("changed bytes with the same length")
    {
        std::fstream damaged (assetPath, std::ios::binary | std::ios::in | std::ios::out);
        REQUIRE (damaged.good());
        damaged.seekp (-1, std::ios::end);
        damaged.put ('X');
        damaged.close();
        REQUIRE (damaged.good());
    }
    auto failed = yesdaw::ui::shell::decodeStoredProjectAssets (bundle);
    REQUIRE_FALSE (failed.assets.has_value());
    REQUIRE (failed.failureReason.find (assetPath.filename().string()) != std::string::npos);
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.createProjectBundle (directory / "original.yesdaw").ok());
    const auto originalId = model.project().id;
    REQUIRE_FALSE (model.loadProjectBundle (bundle, {}).ok());
    REQUIRE (model.project().id == originalId);
    REQUIRE (model.bundlePath() == directory / "original.yesdaw");
}

TEST_CASE ("prepared empty project keeps the existing transport-only open behavior",
           "[ui][input][project-load][prepared-project-load]")
{
    const auto directory = loadTestDirectory();
    const auto bundle = directory / "empty.yesdaw";
    {
        yesdaw::ui::UiAppModel source;
        REQUIRE (source.createProjectBundle (bundle).ok());
    }
    auto stored = yesdaw::ui::shell::decodeStoredProjectAssets (bundle);
    REQUIRE (stored.assets.has_value());
    REQUIRE (stored.assets->empty());
    yesdaw::ui::UiAppModel model;
    REQUIRE (model.openPreparedProjectBundle (std::move (stored.prepared)).ok());
    REQUIRE (model.project().tracks.size() == 1u);
    REQUIRE (model.project().clips.empty());
    REQUIRE (model.playbackReady());
    REQUIRE (model.context().commandDispatchCount == 1u);
    REQUIRE (model.dispatch (yesdaw::ui::UiActionId::TransportPlay).dispatched);
    const auto rendered = model.renderPlaybackFrames (512, 128);
    REQUIRE (rendered.size() == 1024u);
    REQUIRE (std::all_of (rendered.begin(), rendered.end(), [] (float sample) { return sample == 0.0f; }));
}
