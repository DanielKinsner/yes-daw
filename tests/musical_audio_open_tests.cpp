#include "ui/UiAppModel.h"
#include "io/WavFile.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <numbers>

TEST_CASE ("Open and Play keep a pitched audio pulse on the beat across 120 to 90 BPM",
           "[musical-listening-open]")
{
    using namespace yesdaw;
    using namespace engine;
    bool crossRate = false;
    SECTION ("48 kHz listening song") {}
    SECTION ("24 kHz source retains its exact 48 kHz view after another graph build") { crossRate = true; }
    const auto directory = std::filesystem::temp_directory_path() / (std::string (crossRate ? "yesdaw-cross-listen-" : "yesdaw-first-listen-") +
        std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto bundle = directory / "Pulse - 120 to 90.yesdaw";
    REQUIRE (std::filesystem::create_directories (directory));
    const auto wavPath = directory / "Pitched pulse.wav";
    const std::size_t frames = crossRate ? 96000u : 192000u;
    const SampleRate rate { crossRate ? 24000.0 : 48000.0 };
    const int beatLength = crossRate ? 12000 : 24000;
    std::vector<float> source (frames);
    for (std::size_t i = 0; i < frames; ++i)
    {
        const auto local = static_cast<int> (i % static_cast<unsigned> (beatLength));
        const double distance = std::min (local, beatLength - local) / (rate.hz * 0.005);
        const double amplitude = 0.025 + 0.20 * std::exp (-0.5 * distance * distance);
        source[i] = static_cast<float> (amplitude * std::sin (2 * std::numbers::pi * 997 * static_cast<double> (i) / rate.hz));
    }
    REQUIRE (io::writeFloat32WavFile (wavPath, rate, 1, frames, source).ok());
    Asset asset;
    {
        persistence::ProjectBundleDb db;
        REQUIRE (persistence::ProjectBundleDb::openOrCreateBundle (bundle, db).ok());
        REQUIRE (db.importAssetBytes ({ wavPath, EntityId::fromBigEndianParts (1, 2), frames, rate, 1 }, asset).ok());
        Project song;
        song.id = EntityId::fromBigEndianParts (1, 1);
        song.sampleRate = { 48000.0 };
        song.tempoMap = { { 0, 120.0 }, { 4 * kTicksPerQuarter, 90.0 } };
        song.meterMap = { { 0, 4, 4 } };
        Track track;
        track.id = EntityId::fromBigEndianParts (1, 3);
        track.strip.name = "Pitched pulse";
        song.tracks = { track };
        Clip clip;
        clip.id = EntityId::fromBigEndianParts (1, 4);
        clip.trackId = track.id;
        clip.assetId = asset.id;
        clip.timeBase = TimeBase::TempoLocked;
        clip.timelineLength = 8 * kTicksPerQuarter;
        clip.srcLen = frames;
        clip.name = "120 to 90 BPM";
        song.assets = { asset };
        song.clips = { clip };
        REQUIRE (db.writeProjectSnapshot (song).ok());
    }
    ui::UiPreparedProjectBundle prepared;
    REQUIRE (ui::UiPreparedProjectBundle::open (bundle, prepared).ok());
    io::Float32Wav stored;
    REQUIRE (io::readFloat32WavFile (bundle / persistence::detail::assetRelativePathForHash (asset.contentHash), stored).ok());
    std::vector<ui::UiDecodedAsset> decoded;
    decoded.push_back (ui::UiDecodedAsset::fromInterleaved (asset.id, stored.sampleRate, stored.channels,
                                                            std::move (stored.interleavedSamples)));
    ui::UiAppModel model;
    const auto loaded = model.loadPreparedProjectBundle (std::move (prepared), std::move (decoded));
    INFO (ui::describeUiAppLoadFailure (loaded));
    REQUIRE (loaded.ok());
    REQUIRE (model.playbackReady());
    if (crossRate)
    {
        REQUIRE (model.selectTimelineClip (model.project().clips.front().id));
        REQUIRE (model.setSelectedTimelineClipGain (0.9f).dispatched);
    }
    REQUIRE_FALSE (model.context().metronomeEnabled);
    REQUIRE (model.dispatch (ui::UiActionId::TransportPlay).dispatched);
    const auto audio = model.renderPlaybackFrames (224000, 128);
    REQUIRE (audio.size() == 448000u);
    double worstMilliseconds = 0, worstCents = 0;
    for (int beat = 1; beat < 8; ++beat)
    {
        const int expected = beat <= 4 ? beat * 24000 : 96000 + (beat - 4) * 32000;
        double energy = 0, moment = 0;
        for (int delta = -11520; delta <= 11520; ++delta)
        {
            const double sample = audio[static_cast<std::size_t> (expected + delta) * 2];
            const double accent = std::max (0.0, sample * sample - 0.025 * 0.025);
            energy += accent;
            moment += accent * delta;
        }
        REQUIRE (energy > 0.1);
        worstMilliseconds = std::max (worstMilliseconds, std::abs (moment / energy) / 48.0);
    }
    for (int start = 5760; start + 12000 <= 224000 - 5760; start += 2400)
    {
        double first = 0, last = 0;
        int count = 0;
        for (int i = start; i < start + 12000; ++i)
        {
            const double prev = audio[static_cast<std::size_t> (i - 1) * 2];
            const double now = audio[static_cast<std::size_t> (i) * 2];
            if (prev < 0 && now >= 0)
            {
                last = i - 1 - prev / (now - prev);
                if (count++ == 0) first = last;
            }
        }
        REQUIRE (count >= 2);
        worstCents = std::max (worstCents, std::abs (1200 * std::log2 ((count - 1) * 48000.0 / ((last - first) * 997))));
    }
    INFO ("Listening song " << bundle.string());
    CAPTURE (worstMilliseconds, worstCents);
    REQUIRE (worstMilliseconds <= 5.0);
    REQUIRE (worstCents <= 5.0);
    REQUIRE (model.dispatch (ui::UiActionId::TransportStop).dispatched);
    ui::UiPreparedProjectBundle missing;
    REQUIRE (ui::UiPreparedProjectBundle::open (bundle, missing).ok());
    const auto refused = model.loadPreparedProjectBundle (std::move (missing), {});
    REQUIRE_FALSE (refused.ok());
    CHECK (model.playbackReady());
    CHECK (model.bundlePath() == bundle);
}
