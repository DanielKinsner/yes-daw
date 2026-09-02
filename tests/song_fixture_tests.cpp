// YES DAW - G0.6 song fixture gate: the generator is deterministic and hash-stable, the bundle
// it writes reopens with the promised shape, and the audio it holds is real (renders non-silent
// through the same offline path the app uses). The pinned hashes ARE the fixture's identity: a
// generator change that moves them must re-pin here with a rationale.

#include "app/SongFixture.h"
#include "engine/OfflineRenderer.h"
#include "engine/Project.h"
#include "io/WavFile.h"
#include "persistence/ProjectBundle.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

using yesdaw::app::fixture::buildSongFixture;
using yesdaw::app::fixture::SongFixtureResult;
using yesdaw::app::fixture::SongFixtureSpec;

namespace {

// The small gate variant: 16 tracks, six seconds, 48 kHz stereo, 4 MIDI clips. Pinned on the
// first green run and never re-pinned silently.
constexpr std::uint64_t kPinnedStemHash    = 0x16cb1e048c484aa2ull;   // pinned 2026-09-01 (G0.6)
constexpr std::uint64_t kPinnedProjectHash = 0x1464a389f66aca53ull;

std::filesystem::path makeTempDir (const std::string& label)
{
    const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("yesdaw-song-fixture-" + label + "-" + std::to_string (ticks));
    std::error_code ec;
    std::filesystem::remove_all (path, ec);
    return path;
}

SongFixtureSpec gateSpec()
{
    SongFixtureSpec spec;
    spec.tracks = 16;
    spec.seconds = 6.0;
    spec.sampleRateHz = 48000;
    spec.channels = 2;
    spec.midiTracks = 4;
    return spec;
}

} // namespace

TEST_CASE ("song fixture: deterministic, hash-stable, reopens with the promised shape",
           "[g0][fixture][song-fixture]")
{
    const SongFixtureResult first = buildSongFixture (makeTempDir ("a"), gateSpec());
    INFO (first.error);
    REQUIRE (first.ok);
    const SongFixtureResult second = buildSongFixture (makeTempDir ("b"), gateSpec());
    REQUIRE (second.ok);

    // Determinism: two builds, two directories, identical hashes.
    REQUIRE (first.stemHash == second.stemHash);
    REQUIRE (first.projectHash == second.projectHash);
    INFO ("stemHash=0x" << std::hex << first.stemHash << " projectHash=0x" << first.projectHash);
    REQUIRE (first.stemHash == kPinnedStemHash);
    REQUIRE (first.projectHash == kPinnedProjectHash);

    // Shape: the bundle reopens with 16 tracks, 16 stereo assets, 3..6 clips per track, 4 MIDI clips.
    yesdaw::persistence::ProjectBundleDb db;
    REQUIRE (yesdaw::persistence::ProjectBundleDb::openExistingBundle (first.bundlePath, db).ok());
    yesdaw::engine::Project project;
    REQUIRE (db.readProjectSnapshot (project).ok());
    REQUIRE (project.tracks.size() == 16u);
    REQUIRE (project.assets.size() == 16u);
    REQUIRE (project.midiClips.size() == 4u);
    REQUIRE (project.clips.size() == first.clipCount);
    REQUIRE (first.noteCount == 64u);
    for (const yesdaw::engine::Asset& asset : project.assets)
    {
        REQUIRE (asset.channels == 2u);
        REQUIRE (asset.frames == first.stemFrames);
    }
    for (const yesdaw::engine::Track& track : project.tracks)
    {
        std::size_t onTrack = 0;
        for (const yesdaw::engine::Clip& clip : project.clips)
            if (clip.trackId == track.id)
                ++onTrack;
        REQUIRE (onTrack >= 3u);
        REQUIRE (onTrack <= 6u);
    }
    REQUIRE (first.stemPaths.size() == 16u);

    // Audio: the stems are real WAVs and the first second of the song renders non-silent through
    // the same offline path the app's export uses.
    std::vector<yesdaw::io::Float32Wav> wavs (project.assets.size());
    std::vector<yesdaw::engine::DecodedAssetAudio> views;
    for (std::size_t i = 0; i < project.assets.size(); ++i)
    {
        REQUIRE (yesdaw::io::readFloat32WavFile (first.stemPaths[i], wavs[i]).ok());
        views.push_back (yesdaw::engine::DecodedAssetAudio {
            project.assets[i].id, project.assets[i].sampleRate, project.assets[i].frames, project.assets[i].channels,
            std::span<const float> (wavs[i].interleavedSamples.data(), wavs[i].interleavedSamples.size()) });
    }
    const auto rendered = yesdaw::engine::renderOfflineProject (
        project, std::span<const yesdaw::engine::DecodedAssetAudio> (views.data(), views.size()));
    INFO (static_cast<int> (rendered.status));
    REQUIRE (rendered.ok());
    float peak = 0.0f;
    const std::size_t firstSecond = std::min<std::size_t> (rendered.interleavedSamples.size(), 48000u * 2u);
    for (std::size_t i = 0; i < firstSecond; ++i)
        peak = std::max (peak, std::abs (rendered.interleavedSamples[i]));
    REQUIRE (peak > 0.01f);
    REQUIRE (peak < 1.0f);
}
