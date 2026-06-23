// YES DAW — headless, self-asserting checks for the H0 spike DSP (SineSource).
//
// This is the MECHANICAL gate that replaces "Dan listens to the tone." It runs with no audio device
// and no display, so it works in CI. It returns exit code 0 (all green) or 1 (a check failed) — never
// asks a human to judge. Three layers:
//   1. golden compare  — regenerate the first 1024 samples and match a committed golden within a
//                        tolerance (locks the exact output against regressions; cross-platform-safe).
//   2. property checks — the output really IS a ~440 Hz, ~ -20 dBFS sine with a fade-in (independent
//                        of the golden: bounds, finiteness, pitch via zero-crossings, RMS, peak, fade).
//   3. throughput floor — the DSP runs many times faster than real time (catches an accidental
//                        per-sample allocation / O(n^2) / denormal-stall regression). Averaged over
//                        thousands of blocks so a CI scheduler hiccup can't false-fail it.
//
// Regenerate the golden after an intended DSP change:
//   YesDawTests --emit-golden > tests/golden/sine_440_48k.csv

#include "dsp/SineSource.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_checks   = 0;
int g_failures = 0;

void check (bool cond, const char* what)
{
    ++g_checks;
    std::printf (cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (! cond) ++g_failures;
}

constexpr double kSampleRate   = 48000.0;
constexpr int    kGoldenFrames = 1024;
constexpr float  kAmplitude    = yesdaw::dsp::SineSource::kDefaultAmplitude;
constexpr double kFreqHz       = yesdaw::dsp::SineSource::kDefaultFrequencyHz;

// Render `numFrames` mono samples from a freshly-prepared SineSource.
std::vector<float> render (double sampleRate, int numFrames)
{
    yesdaw::dsp::SineSource src;
    src.prepare (sampleRate);
    std::vector<float> out (static_cast<size_t> (numFrames));
    src.processMono (out.data(), numFrames);
    return out;
}

// --- 1. Golden compare: regenerate the first 1024 samples, match the committed reference. ----------
void testGolden()
{
    std::printf ("[golden] regenerate first %d samples and diff against the committed reference\n",
                 kGoldenFrames);

    std::ifstream f (YESDAW_GOLDEN_PATH);
    if (! f)
    {
        check (false, "golden file present at YESDAW_GOLDEN_PATH");
        return;
    }

    std::vector<float> golden;
    std::string line;
    while (std::getline (f, line))
        if (! line.empty())
            golden.push_back (std::stof (line));

    if (golden.size() != static_cast<size_t> (kGoldenFrames))
    {
        check (false, "golden has exactly 1024 samples");
        return;
    }

    const std::vector<float> fresh = render (kSampleRate, kGoldenFrames);

    double maxAbsDiff = 0.0;
    for (size_t i = 0; i < golden.size(); ++i)
        maxAbsDiff = std::max (maxAbsDiff, std::fabs (static_cast<double> (fresh[i] - golden[i])));

    std::printf ("         max abs diff vs golden = %.3e (tol 1e-5)\n", maxAbsDiff);
    check (maxAbsDiff <= 1e-5, "regenerated output matches golden within 1e-5");
}

// --- 2. Property checks: it really is a tamed ~440 Hz sine. -----------------------------------------
void testProperties()
{
    std::printf ("[properties] verify pitch, level, bounds and fade-in on 1 s of audio\n");

    const int oneSecond = static_cast<int> (kSampleRate);
    const std::vector<float> x = render (kSampleRate, oneSecond);
    const size_t n = x.size();

    // Bounds + finiteness across the whole render.
    bool inBounds = true, allFinite = true;
    const float bound = kAmplitude + 1.0e-6f;
    for (float v : x)
    {
        if (! std::isfinite (v))      allFinite = false;
        if (v > bound || v < -bound)  inBounds  = false;
    }
    check (allFinite, "all samples finite (no NaN/Inf)");
    check (inBounds,  "all samples within +/- amplitude");

    // First sample is exactly silence (envelope starts at 0, sin(0)=0).
    check (x[0] == 0.0f, "first sample is silence (clean start)");

    // Steady-state region: well past the 50 ms (2400-sample) fade.
    const size_t steadyStart = 4800;

    // Pitch via rising zero-crossings: f = crossings * sr / duration.
    int crossings = 0;
    for (size_t i = steadyStart + 1; i < n; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            ++crossings;
    const double steadyFrames = static_cast<double> (n - steadyStart);
    const double freqEst = static_cast<double> (crossings) * kSampleRate / steadyFrames;
    std::printf ("         recovered frequency = %.2f Hz (expected %.2f)\n", freqEst, kFreqHz);
    check (std::fabs (freqEst - kFreqHz) <= kFreqHz * 0.01, "recovered frequency within 1% of 440 Hz");

    // RMS of a full-scale sine is amplitude / sqrt(2); peak is amplitude.
    double sumSq = 0.0;
    float steadyPeak = 0.0f;
    for (size_t i = steadyStart; i < n; ++i)
    {
        sumSq += static_cast<double> (x[i]) * static_cast<double> (x[i]);
        steadyPeak = std::max (steadyPeak, std::fabs (x[i]));
    }
    const double rms = std::sqrt (sumSq / steadyFrames);
    const double rmsExpected = static_cast<double> (kAmplitude) / std::sqrt (2.0);
    std::printf ("         steady RMS = %.5f (expected %.5f), peak = %.5f (expected %.5f)\n",
                 rms, rmsExpected, static_cast<double> (steadyPeak), static_cast<double> (kAmplitude));
    check (std::fabs (rms - rmsExpected) <= rmsExpected * 0.01, "steady RMS within 1% of A/sqrt(2)");
    check (std::fabs (static_cast<double> (steadyPeak - kAmplitude)) <= kAmplitude * 0.01,
           "steady peak within 1% of amplitude");

    // Fade-in present: the first 5 ms is much quieter than steady state.
    float firstWindowPeak = 0.0f;
    for (size_t i = 0; i < 240 && i < n; ++i)
        firstWindowPeak = std::max (firstWindowPeak, std::fabs (x[i]));
    std::printf ("         first-5ms peak = %.5f vs steady peak = %.5f\n",
                 static_cast<double> (firstWindowPeak), static_cast<double> (steadyPeak));
    check (firstWindowPeak < 0.5f * steadyPeak, "fade-in present (first 5 ms below half steady level)");
}

// --- 3. Throughput floor: the DSP runs many times faster than real time. ---------------------------
void testThroughput()
{
    std::printf ("[throughput] render ~10 s of audio in 128-frame blocks and measure real-time factor\n");

    const int       blockSize  = 128;
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
    const double renderedSec  = static_cast<double> (totalBlocks * blockSize) / kSampleRate;
    const double realtimeFactor = renderedSec / wallSec;

    std::printf ("         rendered %.1f s of audio in %.4f s wall = %.0fx real time (floor 20x)\n",
                 renderedSec, wallSec, realtimeFactor);
    check (realtimeFactor >= 20.0, "DSP sustains >= 20x real time");
}

} // namespace

int main (int argc, char** argv)
{
    // Regeneration mode: print the golden samples so they can be redirected into the committed file.
    if (argc >= 2 && std::string (argv[1]) == "--emit-golden")
    {
        const std::vector<float> g = render (kSampleRate, kGoldenFrames);
        for (float v : g)
            std::printf ("%.9g\n", static_cast<double> (v));
        return 0;
    }

    std::printf ("YES DAW — DSP self-check (SineSource)\n\n");
    testGolden();
    testProperties();
    testThroughput();
    std::printf ("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
