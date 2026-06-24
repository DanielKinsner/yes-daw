// YES DAW — headless, self-asserting checks for the H0 spike DSP (SineSource).
//
// This is the MECHANICAL gate that replaces "Dan listens to the tone". It runs with no audio device
// and no display, so it works in CI; ctest reports pass/fail. Pure C++ + Catch2 — it never touches
// JUCE — so it builds and runs fast on every runner. Four layers:
//   - golden compare  : regenerate the first 1024 samples and match a committed golden within a
//                       tolerance (locks exact output against regressions; absorbs cross-platform
//                       libm ULP differences).
//   - spectral + pitch: a single-bin DFT (Goertzel) and a zero-crossing count both confirm the tone
//                       is 440 Hz at ~ -20 dBFS — "the sine IS 440 Hz", asserted, not heard.
//   - envelope        : the 50 ms fade-in is present (start isn't a jumpscare); steady level == target.
//   - throughput floor: the DSP runs many times faster than real time, averaged over thousands of
//                       blocks so a CI scheduler hiccup can't false-fail it.
//
// Bless the golden after an INTENTIONAL DSP change:  cmake --build build --target bless-goldens

#include "dsp/SineSource.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr double kSampleRate   = 48000.0;
constexpr int    kGoldenFrames = 1024;
constexpr float  kAmplitude    = yesdaw::dsp::SineSource::kDefaultAmplitude;
constexpr double kFreqHz       = yesdaw::dsp::SineSource::kDefaultFrequencyHz;
constexpr double kTwoPi        = 6.283185307179586476925286766559;

// Render `numFrames` mono samples from a freshly-prepared SineSource.
std::vector<float> render (double sampleRate, int numFrames)
{
    yesdaw::dsp::SineSource src;
    src.prepare (sampleRate);
    std::vector<float> out (static_cast<size_t> (numFrames));
    src.processMono (out.data(), numFrames);
    return out;
}

// Magnitude of a single DFT bin via the Goertzel algorithm (a 1-frequency FFT). For a pure sine of
// amplitude A whose frequency lands on an exact bin, this returns ~A. Used to assert "the spectral
// peak is at 440 Hz" without pulling in a full FFT.
double goertzelAmplitude (const std::vector<float>& x, double freq, double sampleRate)
{
    const double w     = kTwoPi * freq / sampleRate;
    const double coeff = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (float v : x)
    {
        const double s0 = static_cast<double> (v) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double real = s1 - s2 * std::cos (w);
    const double imag = s2 * std::sin (w);
    return std::sqrt (real * real + imag * imag) * 2.0 / static_cast<double> (x.size());
}

// Write the first 1024 samples to `path` as CSV (one %.9g float per line) — the blessed golden.
int emitGolden (const std::string& path)
{
    const std::vector<float> g = render (kSampleRate, kGoldenFrames);
    std::ofstream out (path, std::ios::binary);   // binary => LF newlines, no CRLF on Windows
    if (! out)
    {
        std::fprintf (stderr, "could not open golden path for writing: %s\n", path.c_str());
        return 2;
    }
    for (float v : g)
    {
        char buf[32];
        std::snprintf (buf, sizeof buf, "%.9g\n", static_cast<double> (v));
        out << buf;
    }
    return 0;
}

} // namespace

TEST_CASE ("golden output matches the committed reference", "[golden]")
{
    std::ifstream f (YESDAW_GOLDEN_PATH);
    REQUIRE (f.good());   // a missing golden is a red, never a silent pass

    std::vector<float> golden;
    std::string line;
    while (std::getline (f, line))
        if (! line.empty())
            golden.push_back (std::stof (line));

    REQUIRE (golden.size() == static_cast<size_t> (kGoldenFrames));

    const std::vector<float> fresh = render (kSampleRate, kGoldenFrames);

    double maxAbsDiff = 0.0;
    for (size_t i = 0; i < golden.size(); ++i)
        maxAbsDiff = std::max (maxAbsDiff, std::fabs (static_cast<double> (fresh[i] - golden[i])));

    INFO ("max abs diff vs golden = " << maxAbsDiff);
    REQUIRE (maxAbsDiff <= 1e-5);
}

TEST_CASE ("output is a 440 Hz sine at ~ -20 dBFS", "[pitch][level]")
{
    const int oneSecond = static_cast<int> (kSampleRate);
    const std::vector<float> x = render (kSampleRate, oneSecond);
    const size_t n = x.size();

    // Bounds + finiteness across the whole render.
    const float bound = kAmplitude + 1.0e-6f;
    for (float v : x)
    {
        REQUIRE (std::isfinite (v));
        REQUIRE (v <= bound);
        REQUIRE (v >= -bound);
    }

    // First sample is exactly silence (envelope starts at 0, sin(0)=0).
    REQUIRE (x[0] == 0.0f);

    // Spectral check: the single-bin DFT at 440 Hz dominates non-harmonic neighbours.
    const double mag440 = goertzelAmplitude (x, kFreqHz, kSampleRate);
    const double mag330 = goertzelAmplitude (x, 330.0,   kSampleRate);
    const double mag550 = goertzelAmplitude (x, 550.0,   kSampleRate);
    INFO ("Goertzel amp @440=" << mag440 << " @330=" << mag330 << " @550=" << mag550);
    REQUIRE (mag440 > 10.0 * mag330);
    REQUIRE (mag440 > 10.0 * mag550);
    // Amplitude in the frequency domain ~ A (a few % low because the 50 ms fade trims early energy).
    REQUIRE (std::fabs (mag440 - static_cast<double> (kAmplitude)) <= 0.05 * kAmplitude);

    // Steady-state region: well past the 50 ms (2400-sample) fade.
    const size_t steadyStart = 4800;
    const double steadyFrames = static_cast<double> (n - steadyStart);

    // Pitch via rising zero-crossings: f = crossings * sr / duration.
    int crossings = 0;
    for (size_t i = steadyStart + 1; i < n; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            ++crossings;
    const double freqEst = static_cast<double> (crossings) * kSampleRate / steadyFrames;
    INFO ("zero-crossing frequency = " << freqEst << " Hz");
    REQUIRE (std::fabs (freqEst - kFreqHz) <= kFreqHz * 0.01);

    // RMS of a full-scale sine is A/sqrt(2); peak is A.
    double sumSq = 0.0;
    float steadyPeak = 0.0f;
    for (size_t i = steadyStart; i < n; ++i)
    {
        sumSq += static_cast<double> (x[i]) * static_cast<double> (x[i]);
        steadyPeak = std::max (steadyPeak, std::fabs (x[i]));
    }
    const double rms = std::sqrt (sumSq / steadyFrames);
    const double rmsExpected = static_cast<double> (kAmplitude) / std::sqrt (2.0);
    REQUIRE (std::fabs (rms - rmsExpected) <= rmsExpected * 0.01);
    REQUIRE (std::fabs (static_cast<double> (steadyPeak - kAmplitude)) <= kAmplitude * 0.01);
}

TEST_CASE ("fade-in tames the start", "[envelope]")
{
    const std::vector<float> x = render (kSampleRate, static_cast<int> (kSampleRate));

    float firstWindowPeak = 0.0f;   // first 5 ms
    for (size_t i = 0; i < 240 && i < x.size(); ++i)
        firstWindowPeak = std::max (firstWindowPeak, std::fabs (x[i]));

    float steadyPeak = 0.0f;        // after the fade
    for (size_t i = 4800; i < x.size(); ++i)
        steadyPeak = std::max (steadyPeak, std::fabs (x[i]));

    INFO ("first-5ms peak = " << firstWindowPeak << " vs steady peak = " << steadyPeak);
    REQUIRE (firstWindowPeak < 0.5f * steadyPeak);
}

TEST_CASE ("DSP sustains many times real time", "[perf]")
{
    const int       blockSize   = 128;
    const long long totalBlocks = static_cast<long long> (10.0 * kSampleRate / blockSize); // ~3750
    const long long warmup      = totalBlocks / 10;

    yesdaw::dsp::SineSource src;
    src.prepare (kSampleRate);
    std::vector<float> buf (static_cast<size_t> (blockSize));

    for (long long b = 0; b < warmup; ++b)
        src.processMono (buf.data(), blockSize);

    const auto t0 = std::chrono::steady_clock::now();
    for (long long b = 0; b < totalBlocks; ++b)
        src.processMono (buf.data(), blockSize);
    const auto t1 = std::chrono::steady_clock::now();

    double wallSec = std::chrono::duration<double> (t1 - t0).count();
    if (wallSec <= 1.0e-9) wallSec = 1.0e-9;
    const double renderedSec    = static_cast<double> (totalBlocks * blockSize) / kSampleRate;
    const double realtimeFactor = renderedSec / wallSec;

    INFO ("rendered " << renderedSec << " s of audio in " << wallSec << " s = "
                      << realtimeFactor << "x real time (floor 20x)");
    REQUIRE (realtimeFactor >= 20.0);
}

int main (int argc, char** argv)
{
    // Bless mode: write the golden, then exit (used by the `bless-goldens` CMake target).
    if (argc >= 2 && std::string (argv[1]) == "--emit-golden")
        return emitGolden (argc >= 3 ? argv[2] : YESDAW_GOLDEN_PATH);

    return Catch::Session().run (argc, argv);
}
