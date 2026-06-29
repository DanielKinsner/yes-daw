#include "analysis/LoudnessMeter.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

namespace {

using yesdaw::analysis::LoudnessMetrics;
using yesdaw::analysis::LoudnessStatus;

constexpr std::uint32_t kSampleRate = 48000;

std::vector<float> makeStereoFixture (std::size_t frames)
{
    std::vector<float> samples (frames * 2u, 0.0f);
    for (std::size_t i = 0; i < frames; ++i)
    {
        const double t = static_cast<double> (i) / static_cast<double> (kSampleRate);
        samples[i * 2u] = static_cast<float> (0.25 * std::sin (2.0 * std::numbers::pi * 997.0 * t));
        samples[i * 2u + 1u] = static_cast<float> (0.05 * std::sin (2.0 * std::numbers::pi * 333.0 * t)
                                                   + 0.01 * std::sin (2.0 * std::numbers::pi * 117.0 * t));
    }
    return samples;
}

std::vector<float> makeMonoFixture (std::size_t frames)
{
    std::vector<float> samples (frames, 0.0f);
    for (std::size_t i = 0; i < frames; ++i)
    {
        const double t = static_cast<double> (i) / static_cast<double> (kSampleRate);
        samples[i] = static_cast<float> (0.2 * std::sin (2.0 * std::numbers::pi * 440.0 * t));
    }
    return samples;
}

struct EburState
{
    ebur128_state* state { nullptr };

    ~EburState()
    {
        if (state != nullptr)
            ebur128_destroy (&state);
    }

    EburState() = default;
    explicit EburState (ebur128_state* s) : state (s) {}
    EburState (const EburState&) = delete;
    EburState& operator= (const EburState&) = delete;
};

double linearToDb (double value)
{
    return value > 0.0 ? 20.0 * std::log10 (value) : -std::numeric_limits<double>::infinity();
}

LoudnessMetrics referenceMetrics (std::span<const float> interleaved,
                                  std::uint32_t channels,
                                  std::size_t chunkFrames = 0)
{
    EburState state {
        ebur128_init (channels,
                      static_cast<unsigned long> (kSampleRate),
                      yesdaw::analysis::kLoudnessMode)
    };
    REQUIRE (state.state != nullptr);

    if (channels == 1)
        REQUIRE (ebur128_set_channel (state.state, 0, EBUR128_CENTER) == EBUR128_SUCCESS);
    else
    {
        REQUIRE (channels == 2);
        REQUIRE (ebur128_set_channel (state.state, 0, EBUR128_LEFT) == EBUR128_SUCCESS);
        REQUIRE (ebur128_set_channel (state.state, 1, EBUR128_RIGHT) == EBUR128_SUCCESS);
    }

    const std::size_t frames = interleaved.size() / static_cast<std::size_t> (channels);
    std::size_t done = 0;
    const std::size_t step = chunkFrames == 0 ? frames : chunkFrames;
    while (done < frames)
    {
        const std::size_t n = std::min (step, frames - done);
        REQUIRE (ebur128_add_frames_float (state.state,
                                           interleaved.data() + done * static_cast<std::size_t> (channels),
                                           n)
                 == EBUR128_SUCCESS);
        done += n;
    }

    LoudnessMetrics metrics;
    metrics.channels = channels;
    metrics.frames = static_cast<std::uint64_t> (frames);
    REQUIRE (ebur128_loudness_global (state.state, &metrics.integratedLufs) == EBUR128_SUCCESS);
    REQUIRE (ebur128_loudness_momentary (state.state, &metrics.momentaryLufs) == EBUR128_SUCCESS);
    REQUIRE (ebur128_loudness_shortterm (state.state, &metrics.shortTermLufs) == EBUR128_SUCCESS);
    REQUIRE (ebur128_loudness_range (state.state, &metrics.loudnessRangeLu) == EBUR128_SUCCESS);
    REQUIRE (ebur128_relative_threshold (state.state, &metrics.relativeThresholdLufs) == EBUR128_SUCCESS);

    for (std::uint32_t ch = 0; ch < channels; ++ch)
    {
        double samplePeak = 0.0;
        double truePeak = 0.0;
        REQUIRE (ebur128_sample_peak (state.state, ch, &samplePeak) == EBUR128_SUCCESS);
        REQUIRE (ebur128_true_peak (state.state, ch, &truePeak) == EBUR128_SUCCESS);
        metrics.samplePeakDbfs[ch] = linearToDb (samplePeak);
        metrics.truePeakDbtp[ch] = linearToDb (truePeak);
    }

    return metrics;
}

void requireClose (double actual, double expected, double margin)
{
    if (std::isinf (expected))
        REQUIRE (actual == expected);
    else
        REQUIRE (actual == Catch::Approx (expected).margin (margin));
}

void requireSameMetrics (const LoudnessMetrics& actual, const LoudnessMetrics& expected)
{
    requireClose (actual.integratedLufs, expected.integratedLufs, 1.0e-9);
    requireClose (actual.momentaryLufs, expected.momentaryLufs, 1.0e-9);
    requireClose (actual.shortTermLufs, expected.shortTermLufs, 1.0e-9);
    requireClose (actual.loudnessRangeLu, expected.loudnessRangeLu, 1.0e-9);
    requireClose (actual.relativeThresholdLufs, expected.relativeThresholdLufs, 1.0e-9);
    REQUIRE (actual.channels == expected.channels);
    REQUIRE (actual.frames == expected.frames);

    for (std::uint32_t ch = 0; ch < actual.channels; ++ch)
    {
        requireClose (actual.samplePeakDbfs[ch], expected.samplePeakDbfs[ch], 1.0e-9);
        requireClose (actual.truePeakDbtp[ch], expected.truePeakDbtp[ch], 1.0e-9);
    }
}

} // namespace

TEST_CASE ("Loudness wrapper matches direct libebur128 for stereo fixtures", "[loudness]")
{
    const auto samples = makeStereoFixture (static_cast<std::size_t> (kSampleRate) * 5u);

    const auto whole = yesdaw::analysis::analyzeInterleavedLoudness (samples, 2, kSampleRate);
    REQUIRE (whole.status == LoudnessStatus::Ok);
    requireSameMetrics (whole.metrics, referenceMetrics (samples, 2));

    const auto chunked = yesdaw::analysis::analyzeInterleavedLoudness (samples, 2, kSampleRate, 257);
    REQUIRE (chunked.status == LoudnessStatus::Ok);
    requireSameMetrics (chunked.metrics, referenceMetrics (samples, 2, 257));
    requireSameMetrics (chunked.metrics, whole.metrics);

    REQUIRE (whole.metrics.integratedLufs < -10.0);
    REQUIRE (whole.metrics.integratedLufs > -40.0);
}

TEST_CASE ("Mono loudness is not treated as dual mono", "[loudness]")
{
    const auto mono = makeMonoFixture (static_cast<std::size_t> (kSampleRate) * 5u);
    std::vector<float> duplicatedStereo (mono.size() * 2u, 0.0f);
    for (std::size_t i = 0; i < mono.size(); ++i)
    {
        duplicatedStereo[i * 2u] = mono[i];
        duplicatedStereo[i * 2u + 1u] = mono[i];
    }

    const auto monoResult = yesdaw::analysis::analyzeInterleavedLoudness (mono, 1, kSampleRate);
    const auto stereoResult = yesdaw::analysis::analyzeInterleavedLoudness (duplicatedStereo, 2, kSampleRate);
    REQUIRE (monoResult.status == LoudnessStatus::Ok);
    REQUIRE (stereoResult.status == LoudnessStatus::Ok);
    requireSameMetrics (monoResult.metrics, referenceMetrics (mono, 1));

    REQUIRE (stereoResult.metrics.integratedLufs - monoResult.metrics.integratedLufs > 2.9);
    REQUIRE (stereoResult.metrics.integratedLufs - monoResult.metrics.integratedLufs < 3.2);
}

TEST_CASE ("Loudness wrapper rejects unsupported or malformed input before libebur128", "[loudness]")
{
    const std::vector<float> stereo { 0.0f, 0.0f, 0.25f, -0.25f };

    REQUIRE (yesdaw::analysis::analyzeInterleavedLoudness (stereo, 2, 0).status
             == LoudnessStatus::InvalidSampleRate);
    REQUIRE (yesdaw::analysis::analyzeInterleavedLoudness (stereo, 3, kSampleRate).status
             == LoudnessStatus::UnsupportedChannelLayout);
    REQUIRE (yesdaw::analysis::analyzeInterleavedLoudness ({ stereo.data(), 3 }, 2, kSampleRate).status
             == LoudnessStatus::SampleCountNotFrameAligned);

    std::vector<float> nonFinite = stereo;
    nonFinite[1] = std::numeric_limits<float>::quiet_NaN();
    REQUIRE (yesdaw::analysis::analyzeInterleavedLoudness (nonFinite, 2, kSampleRate).status
             == LoudnessStatus::NonFiniteInput);

    nonFinite[1] = std::numeric_limits<float>::infinity();
    REQUIRE (yesdaw::analysis::analyzeInterleavedLoudness (nonFinite, 2, kSampleRate).status
             == LoudnessStatus::NonFiniteInput);
}

TEST_CASE ("Silence and peak-heavy inputs produce deterministic edge metrics", "[loudness]")
{
    std::vector<float> silence (static_cast<std::size_t> (kSampleRate) * 2u, 0.0f);
    const auto silent = yesdaw::analysis::analyzeInterleavedLoudness (silence, 1, kSampleRate, 128);
    REQUIRE (silent.status == LoudnessStatus::Ok);
    REQUIRE (silent.metrics.integratedLufs == -std::numeric_limits<double>::infinity());
    REQUIRE (silent.metrics.samplePeakDbfs[0] == -std::numeric_limits<double>::infinity());
    REQUIRE (silent.metrics.truePeakDbtp[0] == -std::numeric_limits<double>::infinity());

    std::vector<float> clipped (static_cast<std::size_t> (kSampleRate) * 2u, 0.0f);
    clipped[0] = 1.0f;
    clipped[1] = -1.0f;
    clipped[1234] = 0.5f;
    const auto peak = yesdaw::analysis::analyzeInterleavedLoudness (clipped, 1, kSampleRate, 31);
    REQUIRE (peak.status == LoudnessStatus::Ok);
    REQUIRE (peak.metrics.samplePeakDbfs[0] == Catch::Approx (0.0).margin (1.0e-9));
    REQUIRE (peak.metrics.truePeakDbtp[0] >= peak.metrics.samplePeakDbfs[0]);
}

TEST_CASE ("Pinned libebur128 version is the ADR-0028 dependency", "[loudness]")
{
    const auto version = yesdaw::analysis::loudnessLibraryVersion();
    REQUIRE (version.major == 1);
    REQUIRE (version.minor == 2);
    REQUIRE (version.patch == 6);
}
