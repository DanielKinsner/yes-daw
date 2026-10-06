// YES DAW - ADR-0055 / ADR-0010 gates for the band-limited resampler behind the rate-matched view.
//
// Against analytic references: a 1 kHz sine at 44.1 kHz resampled to 48 kHz matches the analytic 48 kHz sine
// within -80 dB (offline tier) and -60 dB (live tier); downsampling 96 kHz to 48 kHz keeps an in-band tone within
// 0.1 dB and attenuates a 30 kHz tone (which would alias to 18 kHz) by at least 70 dB; an impulse lands at its
// exactly scaled position (zero phase); the output is deterministic and same-rate input passes through.

#include "engine/Resample.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <numbers>
#include <vector>

namespace {

using yesdaw::engine::ResampleQuality;
using yesdaw::engine::resampleInterleaved;

std::vector<float> sine (double frequency, double rate, std::size_t frames, double amplitude = 0.5)
{
    std::vector<float> out (frames);
    for (std::size_t i = 0; i < frames; ++i)
        out[i] = static_cast<float> (amplitude * std::sin (2.0 * std::numbers::pi * frequency * static_cast<double> (i) / rate));
    return out;
}

// RMS error relative to the reference's RMS, in dB, over [from, to).
double relativeErrorDb (const std::vector<float>& actual, const std::vector<float>& reference, std::size_t from, std::size_t to)
{
    double error = 0.0;
    double signal = 0.0;
    for (std::size_t i = from; i < to; ++i)
    {
        const double d = static_cast<double> (actual[i]) - static_cast<double> (reference[i]);
        error += d * d;
        signal += static_cast<double> (reference[i]) * static_cast<double> (reference[i]);
    }
    return 10.0 * std::log10 (error / signal);
}

// The amplitude of one frequency (a single-bin DFT over [from, to)).
double toneAmplitude (const std::vector<float>& samples, double frequency, double rate, std::size_t from, std::size_t to)
{
    double re = 0.0;
    double im = 0.0;
    // A Hann window keeps a strong neighbour's leakage out of a weak bin.
    double windowSum = 0.0;
    for (std::size_t i = from; i < to; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * std::numbers::pi * static_cast<double> (i - from) / static_cast<double> (to - from - 1));
        const double phase = 2.0 * std::numbers::pi * frequency * static_cast<double> (i) / rate;
        re += w * static_cast<double> (samples[i]) * std::cos (phase);
        im += w * static_cast<double> (samples[i]) * std::sin (phase);
        windowSum += w;
    }
    return 2.0 * std::sqrt (re * re + im * im) / windowSum;
}

} // namespace

TEST_CASE ("ADR-0055 a 44.1 kHz sine resampled to 48 kHz matches the analytic sine (both tiers)", "[cross-rate][resample]")
{
    const std::vector<float> source = sine (1'000.0, 44'100.0, 44'100);
    const std::vector<float> reference = sine (1'000.0, 48'000.0, 48'000);
    for (const auto [quality, floorDb] : { std::pair { ResampleQuality::OfflineRender, -80.0 },
                                           std::pair { ResampleQuality::LivePlayback, -60.0 } })
    {
        const std::vector<float> resampled = resampleInterleaved (source, 1, 44'100.0, 48'000.0, quality);
        REQUIRE (resampled.size() == 48'000u);
        const double errorDb = relativeErrorDb (resampled, reference, 2'000, 46'000);   // the steady state
        INFO ("tier " << static_cast<int> (quality) << " error " << errorDb << " dB");
        REQUIRE (errorDb < floorDb);
    }
    // And back down: 48 kHz to 44.1 kHz.
    const std::vector<float> down = resampleInterleaved (reference, 1, 48'000.0, 44'100.0, ResampleQuality::OfflineRender);
    REQUIRE (down.size() == 44'100u);
    REQUIRE (relativeErrorDb (down, source, 2'000, 42'000) < -80.0);
}

TEST_CASE ("ADR-0055 downsampling 96 kHz to 48 kHz keeps the band and does not alias", "[cross-rate][resample]")
{
    const std::size_t frames = 96'000;
    std::vector<float> source (frames);
    const std::vector<float> inBand = sine (1'000.0, 96'000.0, frames, 0.4);
    const std::vector<float> ultrasonic = sine (30'000.0, 96'000.0, frames, 0.4);
    for (std::size_t i = 0; i < frames; ++i)
        source[i] = inBand[i] + ultrasonic[i];

    for (const ResampleQuality quality : { ResampleQuality::OfflineRender, ResampleQuality::LivePlayback })
    {
        const std::vector<float> out = resampleInterleaved (source, 1, 96'000.0, 48'000.0, quality);
        REQUIRE (out.size() == 48'000u);
        const double kept = toneAmplitude (out, 1'000.0, 48'000.0, 4'000, 44'000);
        const double alias = toneAmplitude (out, 18'000.0, 48'000.0, 4'000, 44'000);   // 30 kHz folds to 18 kHz
        INFO ("tier " << static_cast<int> (quality) << " kept " << kept << " alias " << alias);
        REQUIRE (std::abs (20.0 * std::log10 (kept / 0.4)) < 0.1);
        REQUIRE (20.0 * std::log10 (alias / 0.4) < -70.0);
    }
}

TEST_CASE ("ADR-0055 an impulse lands at its scaled position (zero phase); stereo channels stay apart",
           "[cross-rate][resample]")
{
    const std::size_t frames = 44'100;
    std::vector<float> stereo (frames * 2u, 0.0f);
    const std::size_t clickLeft = 10'000;
    const std::size_t clickRight = 30'000;
    stereo[clickLeft * 2u] = 1.0f;
    stereo[clickRight * 2u + 1u] = 1.0f;
    for (const ResampleQuality quality : { ResampleQuality::OfflineRender, ResampleQuality::LivePlayback })
    {
        const std::vector<float> out = resampleInterleaved (stereo, 2, 44'100.0, 48'000.0, quality);
        REQUIRE (out.size() == 48'000u * 2u);
        const auto peakFrame = [&out] (std::size_t channel) {
            std::size_t best = 0;
            for (std::size_t f = 0; f < out.size() / 2u; ++f)
                if (std::abs (out[f * 2u + channel]) > std::abs (out[best * 2u + channel]))
                    best = f;
            return best;
        };
        const auto expected = [] (std::size_t n) { return static_cast<long long> (std::llround (static_cast<double> (n) * 48'000.0 / 44'100.0)); };
        REQUIRE (std::abs (static_cast<long long> (peakFrame (0)) - expected (clickLeft)) <= 1);
        REQUIRE (std::abs (static_cast<long long> (peakFrame (1)) - expected (clickRight)) <= 1);
    }
}

TEST_CASE ("ADR-0055 the resampler is deterministic, and same-rate input passes through", "[cross-rate][resample]")
{
    const std::vector<float> source = sine (440.0, 44'100.0, 10'000);
    REQUIRE (resampleInterleaved (source, 1, 44'100.0, 48'000.0, ResampleQuality::LivePlayback)
             == resampleInterleaved (source, 1, 44'100.0, 48'000.0, ResampleQuality::LivePlayback));
    REQUIRE (resampleInterleaved (source, 1, 44'100.0, 44'100.0, ResampleQuality::OfflineRender) == source);
    REQUIRE (resampleInterleaved (source, 3, 44'100.0, 48'000.0, ResampleQuality::LivePlayback).empty());   // not frame-aligned
    REQUIRE (yesdaw::engine::resampledFrameCount (44'100, 44'100.0, 48'000.0) == 48'000u);
    REQUIRE (yesdaw::engine::resampledFrameCount (1, 44'100.0, 48'000.0) == 1u);
}

// Measured, not gated in CI (machine-dependent): a three-minute stereo 44.1 kHz file's live view — the cost an
// import or an open pays (ADR-0055 reports it in the evidence). Run with the tag to see the time.
TEST_CASE ("ADR-0055 live view build time for three minutes of stereo 44.1 kHz", "[.perf][cross-rate]")
{
    const std::size_t frames = 44'100u * 180u;
    std::vector<float> stereo (frames * 2u);
    for (std::size_t i = 0; i < frames; ++i)
    {
        stereo[i * 2u] = static_cast<float> (0.3 * std::sin (0.0627 * static_cast<double> (i)));
        stereo[i * 2u + 1u] = static_cast<float> (0.3 * std::sin (0.0911 * static_cast<double> (i)));
    }
    const auto start = std::chrono::steady_clock::now();
    const std::vector<float> view = resampleInterleaved (stereo, 2, 44'100.0, 48'000.0, ResampleQuality::LivePlayback);
    const double liveSeconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    const auto offlineStart = std::chrono::steady_clock::now();
    const std::vector<float> offline = resampleInterleaved (stereo, 2, 44'100.0, 48'000.0, ResampleQuality::OfflineRender);
    const double offlineSeconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - offlineStart).count();
    WARN ("live view: " << liveSeconds << " s, offline view: " << offlineSeconds << " s for 180 s stereo");
    REQUIRE (view.size() == 48'000u * 180u * 2u);
    REQUIRE (offline.size() == view.size());
}
