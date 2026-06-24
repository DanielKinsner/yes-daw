// YES DAW — headless, self-asserting checks for the H0 spike DSP (SineSource).
//
// This is the MECHANICAL gate that replaces "Dan listens to the tone". It runs with no audio device
// and no display, so it works in CI; ctest reports pass/fail. Pure C++ + Catch2 — it never touches
// JUCE — so it builds and runs fast on every runner. Layers:
//   - golden compare : regenerate the first 3072 samples (PAST the 50 ms fade, so it locks the
//                      sustained waveform, not just the ramp) and match a committed golden within a
//                      tolerance (regression lock; absorbs cross-platform libm ULP differences).
//   - pitch + level  : a single-bin DFT (Goertzel) and a zero-crossing count both confirm 440 Hz at
//                      ~ -20 dBFS — "the sine IS 440 Hz", asserted, not heard.
//   - purity         : symmetry (positive peak == |negative peak|), ~zero DC, and 2nd/3rd harmonic
//                      bins >= 40 dB down — catches an asymmetric/nonlinear distortion that the level
//                      and pitch checks alone would pass.
//   - envelope       : the 50 ms fade-in is present (start isn't a jumpscare); steady level == target.
//   - throughput     : a COARSE smoke test for catastrophic algorithmic blow-ups (O(n^2), pathological
//                      slowdowns). It does NOT catch a per-sample allocation — that is the RTSan CI
//                      leg's job ([[clang::nonblocking]] on the hot path), not this.
//
// Bless the golden after an INTENTIONAL DSP change:  cmake --build build --target bless-goldens
// NOTE: main() must pass unrecognised CLI flags through to Catch (catch_discover_tests relies on it).

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
constexpr int    kGoldenFrames = 3072;   // past the 2400-sample (50 ms) fade -> locks steady state
constexpr float  kAmplitude    = yesdaw::dsp::SineSource::kDefaultAmplitude;
constexpr double kFreqHz       = yesdaw::dsp::SineSource::kDefaultFrequencyHz;
constexpr double kTwoPi        = 6.283185307179586476925286766559;
constexpr size_t kSteadyStart  = 4800;   // 100 ms in: envelope fully open

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
// amplitude A whose frequency lands on an exact bin, this returns ~A.
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

// Write the first kGoldenFrames samples to `path` as CSV (one %.9g float per line) — the blessed golden.
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

TEST_CASE ("output is a clean 440 Hz sine at ~ -20 dBFS", "[pitch][level][purity]")
{
    const int oneSecond = static_cast<int> (kSampleRate);
    const std::vector<float> x = render (kSampleRate, oneSecond);
    const size_t n = x.size();

    // Bounds + finiteness across the whole render (slack kept fast-math-safe).
    const float bound = kAmplitude * (1.0f + 1.0e-4f);
    for (float v : x)
    {
        REQUIRE (std::isfinite (v));
        REQUIRE (v <= bound);
        REQUIRE (v >= -bound);
    }

    // First sample is exactly silence (envelope starts at 0, sin(0)=0).
    REQUIRE (x[0] == 0.0f);

    // --- Spectral: the 440 Hz bin dominates non-harmonic neighbours, and harmonics are far down. ---
    const double mag440  = goertzelAmplitude (x, kFreqHz, kSampleRate);
    const double mag330  = goertzelAmplitude (x, 330.0,   kSampleRate);
    const double mag550  = goertzelAmplitude (x, 550.0,   kSampleRate);
    const double mag880  = goertzelAmplitude (x, 880.0,   kSampleRate);   // 2nd harmonic
    const double mag1320 = goertzelAmplitude (x, 1320.0,  kSampleRate);   // 3rd harmonic
    INFO ("Goertzel @440=" << mag440 << " @330=" << mag330 << " @550=" << mag550
                           << " @880=" << mag880 << " @1320=" << mag1320);
    REQUIRE (mag440 > 10.0 * mag330);
    REQUIRE (mag440 > 10.0 * mag550);
    REQUIRE (std::fabs (mag440 - static_cast<double> (kAmplitude)) <= 0.05 * kAmplitude);
    // Harmonics >= 40 dB (100x) below the fundamental => no nonlinear distortion.
    REQUIRE (mag880  < mag440 / 100.0);
    REQUIRE (mag1320 < mag440 / 100.0);

    // --- Steady-state stats: pitch, level, symmetry, DC. ---
    const double steadyFrames = static_cast<double> (n - kSteadyStart);

    int crossings = 0;
    double sumSq = 0.0, sum = 0.0;
    float posPeak = 0.0f, negPeak = 0.0f;
    for (size_t i = kSteadyStart; i < n; ++i)
    {
        const float v = x[i];
        if (i > kSteadyStart && x[i - 1] < 0.0f && v >= 0.0f) ++crossings;  // rising zero-crossings
        sumSq += static_cast<double> (v) * static_cast<double> (v);
        sum   += static_cast<double> (v);
        posPeak = std::max (posPeak, v);
        negPeak = std::min (negPeak, v);
    }

    // Pitch via rising zero-crossings: f = crossings * sr / duration.
    const double freqEst = static_cast<double> (crossings) * kSampleRate / steadyFrames;
    INFO ("zero-crossing frequency = " << freqEst << " Hz");
    REQUIRE (std::fabs (freqEst - kFreqHz) <= kFreqHz * 0.01);

    // RMS of a full-scale sine is A/sqrt(2); peak is A.
    const double rms = std::sqrt (sumSq / steadyFrames);
    const double rmsExpected = static_cast<double> (kAmplitude) / std::sqrt (2.0);
    REQUIRE (std::fabs (rms - rmsExpected) <= rmsExpected * 0.01);
    REQUIRE (std::fabs (static_cast<double> (posPeak - kAmplitude)) <= kAmplitude * 0.01);

    // Symmetry: positive peak ~ |negative peak| (catches one-sided clipping/distortion).
    INFO ("posPeak=" << posPeak << " negPeak=" << negPeak);
    REQUIRE (std::fabs (static_cast<double> (posPeak + negPeak)) <= kAmplitude * 0.01);

    // DC offset ~ 0 (a sine has no DC; distortion would introduce it).
    const double dc = sum / steadyFrames;
    INFO ("DC offset = " << dc);
    REQUIRE (std::fabs (dc) <= static_cast<double> (kAmplitude) * 1e-3);
}

TEST_CASE ("fade-in tames the start", "[envelope]")
{
    const std::vector<float> x = render (kSampleRate, static_cast<int> (kSampleRate));

    float firstWindowPeak = 0.0f;   // first 5 ms
    for (size_t i = 0; i < 240 && i < x.size(); ++i)
        firstWindowPeak = std::max (firstWindowPeak, std::fabs (x[i]));

    float steadyPeak = 0.0f;        // after the fade
    for (size_t i = kSteadyStart; i < x.size(); ++i)
        steadyPeak = std::max (steadyPeak, std::fabs (x[i]));

    INFO ("first-5ms peak = " << firstWindowPeak << " vs steady peak = " << steadyPeak);
    REQUIRE (firstWindowPeak < 0.5f * steadyPeak);
}

TEST_CASE ("DSP sustains real time with headroom (coarse smoke test)", "[perf]")
{
    // Coarse blow-up detector only (O(n^2), pathological slowdown). The real allocation/lock
    // RT-safety guard is the RTSan CI leg, not this throughput number.
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
                      << realtimeFactor << "x real time");
    REQUIRE (realtimeFactor >= 20.0);
}

TEST_CASE ("hot path is real-time-safe", "[rtsan]")
{
    // processMono/nextSample are marked YESDAW_RT_HOT ([[clang::nonblocking]]). Under the RTSan CI
    // leg (-fsanitize=realtime) this run ABORTS if the hot path allocates, locks, or does I/O. In a
    // normal build it's just a smoke render that must produce finite output.
    yesdaw::dsp::SineSource src;
    src.prepare (kSampleRate);
    std::vector<float> buf (256);   // allocation is here, OUTSIDE the nonblocking call — fine
    for (int b = 0; b < 100; ++b)
        src.processMono (buf.data(), 256);
    REQUIRE (std::isfinite (buf[0]));
}

int main (int argc, char** argv)
{
    // Bless mode: write the golden, then exit (used by the `bless-goldens` CMake target).
    if (argc >= 2 && std::string (argv[1]) == "--emit-golden")
        return emitGolden (argc >= 3 ? argv[2] : YESDAW_GOLDEN_PATH);

    // Everything else (incl. catch_discover_tests' --list flags) goes straight to Catch.
    return Catch::Session().run (argc, argv);
}
