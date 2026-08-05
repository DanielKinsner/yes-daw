// YES DAW — the real-Project playback fixture shared by the soak and the packaged playback
// checker (H17 packaged verifier, U3).
//
// Extracted from tools/soak/SoakMain.cpp's preparePlaybackProject so the fixture is buildable and
// assertable WITHOUT an audio device: a tiny tone Project whose Clip lives on a Track (the mixer
// projection only projects clips owned by a project.tracks entry — a track-less project renders
// silence, which soaked zeros on real hardware on 2026-07-27; tests/hardware_verification_tests.cpp
// now pins that failure class device-free).
//
// Pure C++ over engine types only — no JUCE — so the deterministic test target can render it
// through PlaybackEngine and assert non-silence.

#pragma once

#include "engine/PlaybackEngine.h"
#include "engine/Project.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace yesdaw::app::hardware {

inline constexpr double kPlaybackToneHz = 440.0;
inline constexpr double kPlaybackToneAmplitude = 0.18;

// Holds the Project plus the decoded sample storage its DecodedAssetAudio span points into.
// Movable (the heap buffer survives a move), but never copy it — the span would dangle.
struct TonePlaybackFixture
{
    engine::Project project;
    std::vector<float> samples;
    std::vector<engine::DecodedAssetAudio> decodedAssets;
    bool ok = false;
    std::string error;

    TonePlaybackFixture() = default;
    TonePlaybackFixture (const TonePlaybackFixture&) = delete;
    TonePlaybackFixture& operator= (const TonePlaybackFixture&) = delete;
    TonePlaybackFixture (TonePlaybackFixture&&) = default;
    TonePlaybackFixture& operator= (TonePlaybackFixture&&) = default;
};

namespace fixture_detail {

constexpr engine::EntityId idFromLowByte (std::uint8_t low) noexcept
{
    engine::EntityId::StorageBytes bytes {};
    bytes.back() = low;
    return engine::EntityId::fromBytes (bytes);
}

inline engine::AssetContentHash hashWithSeed (std::uint8_t seed) noexcept
{
    engine::AssetContentHash hash;
    for (std::size_t i = 0; i < hash.bytes.size(); ++i)
        hash.bytes[i] = static_cast<std::uint8_t> (seed + static_cast<std::uint8_t> (i * 11u));
    return hash;
}

} // namespace fixture_detail

[[nodiscard]] inline TonePlaybackFixture buildTonePlaybackFixture (double sampleRateHz,
                                                                   int blockSize,
                                                                   double seconds)
{
    TonePlaybackFixture fixture;

    if (sampleRateHz <= 0.0 || blockSize <= 0 || seconds <= 0.0)
    {
        fixture.error = "invalid sample rate / block size / duration";
        return fixture;
    }

    const std::uint64_t frames = static_cast<std::uint64_t> (
        std::max (1.0, std::ceil (seconds * sampleRateHz))) + static_cast<std::uint64_t> (blockSize);
    if (frames > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max()))
    {
        fixture.error = "fixture length out of range";
        return fixture;
    }

    constexpr double twoPi = 6.283185307179586476925286766559;
    fixture.samples.assign (static_cast<std::size_t> (frames), 0.0f);
    double phase = 0.0;
    const double phaseStep = twoPi * kPlaybackToneHz / sampleRateHz;
    for (float& sample : fixture.samples)
    {
        sample = static_cast<float> (kPlaybackToneAmplitude * std::sin (phase));
        phase += phaseStep;
        if (phase >= twoPi)
            phase -= twoPi;
    }

    engine::Asset asset;
    asset.id = fixture_detail::idFromLowByte (1);
    asset.contentHash = fixture_detail::hashWithSeed (1);
    asset.frames = frames;
    asset.sampleRate = engine::SampleRate { sampleRateHz };
    asset.channels = 1;

    engine::Clip clip;
    clip.id = fixture_detail::idFromLowByte (2);
    clip.assetId = asset.id;
    clip.trackId = fixture_detail::idFromLowByte (4);
    clip.timelineStart = 0;
    clip.timelineLength = static_cast<engine::Tick> (frames);
    clip.srcOffset = 0;
    clip.srcLen = frames;
    clip.gain = 1.0f;
    clip.timeBase = engine::TimeBase::SampleLocked;

    // The Clip must live on a Track: the mixer projection only projects clips owned by a
    // project.tracks entry, so a track-less project renders silence.
    engine::Track track;
    track.id = clip.trackId;
    track.strip.name = "Verifier tone";

    fixture.project = {};
    fixture.project.id = fixture_detail::idFromLowByte (3);
    fixture.project.sampleRate = engine::SampleRate { sampleRateHz };
    fixture.project.assets = { asset };
    fixture.project.clips = { clip };
    fixture.project.tracks = { track };

    fixture.decodedAssets = {
        engine::DecodedAssetAudio {
            asset.id,
            asset.sampleRate,
            asset.frames,
            asset.channels,
            std::span<const float> (fixture.samples.data(), fixture.samples.size()),
        },
    };

    fixture.ok = true;
    return fixture;
}

} // namespace yesdaw::app::hardware
