// YES DAW — H0 spike #1 (finish): load + scrub a WAV, verified mechanically.
//
// Proves the asset pipeline's first link — decode a real WAV and read arbitrary sub-ranges — without
// a human listening. Unlike YesDawCheck this one DOES link JUCE (juce::WavAudioFormat), so it lives in
// its own target (YesDawAssetCheck), built only when YESDAW_BUILD_APPS is on. It still runs headless
// (file decode, no device/display), so it's part of the normal CI matrix + ctest.
//
// The fixture (tests/fixtures/sine_440_48k_mono.wav) is a committed, deterministic 440 Hz sine at 0.5,
// mono, 48 kHz, 16-bit — so the decode can be golden-diffed against the analytic signal AND its pitch
// recovered independently.

#include <catch2/catch_test_macros.hpp>

#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <memory>

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kSr    = 48000.0;
constexpr double kFreq  = 440.0;
constexpr float  kAmp   = 0.5f;
constexpr int    kLen   = 4096;

std::unique_ptr<juce::AudioFormatReader> openFixture()
{
    juce::WavAudioFormat wav;
    const juce::File f { juce::String { YESDAW_WAV_FIXTURE_PATH } };
    return std::unique_ptr<juce::AudioFormatReader> (
        wav.createReaderFor (new juce::FileInputStream (f), true));
}

} // namespace

TEST_CASE ("fixture WAV metadata is as authored", "[wav][metadata]")
{
    const auto reader = openFixture();
    REQUIRE (reader != nullptr);
    REQUIRE (reader->sampleRate == kSr);
    REQUIRE (reader->numChannels == 1u);
    REQUIRE (reader->lengthInSamples == kLen);
    REQUIRE (reader->bitsPerSample == 16u);
}

TEST_CASE ("decoded WAV matches the 440 Hz sine (golden-diff)", "[wav][golden]")
{
    const auto reader = openFixture();
    REQUIRE (reader != nullptr);

    juce::AudioBuffer<float> buf (1, kLen);
    REQUIRE (reader->read (&buf, 0, kLen, 0, true, false));
    const float* x = buf.getReadPointer (0);

    double maxDiff = 0.0;
    bool inBounds = true;
    for (int i = 0; i < kLen; ++i)
    {
        const double expected = (double) kAmp * std::sin (kTwoPi * kFreq * (double) i / kSr);
        maxDiff = std::max (maxDiff, std::fabs ((double) x[i] - expected));
        if (x[i] > kAmp + 1.0e-4f || x[i] < -kAmp - 1.0e-4f) inBounds = false;
    }
    INFO ("max abs diff vs analytic sine = " << maxDiff);
    REQUIRE (maxDiff <= 1.0e-4);   // 16-bit quantization (~1.5e-5) sits well inside this
    REQUIRE (inBounds);
}

TEST_CASE ("pitch recovered from the decoded WAV is 440 Hz", "[wav][pitch]")
{
    const auto reader = openFixture();
    REQUIRE (reader != nullptr);

    juce::AudioBuffer<float> buf (1, kLen);
    REQUIRE (reader->read (&buf, 0, kLen, 0, true, false));
    const float* x = buf.getReadPointer (0);

    int crossings = 0;
    for (int i = 1; i < kLen; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            ++crossings;

    const double freqEst = (double) crossings * kSr / (double) kLen;
    INFO ("zero-crossing frequency = " << freqEst << " Hz");
    REQUIRE (std::fabs (freqEst - kFreq) <= kFreq * 0.02);   // short buffer -> a looser 2% bound
}

TEST_CASE ("scrub: a sub-range read equals the matching slice of the whole", "[wav][scrub]")
{
    const auto reader = openFixture();
    REQUIRE (reader != nullptr);

    juce::AudioBuffer<float> full (1, kLen);
    REQUIRE (reader->read (&full, 0, kLen, 0, true, false));

    const int offset = 1000, len = 500;
    juce::AudioBuffer<float> sub (1, len);
    REQUIRE (reader->read (&sub, 0, len, offset, true, false));   // start reading at sample `offset`

    double maxDiff = 0.0;
    for (int i = 0; i < len; ++i)
        maxDiff = std::max (maxDiff,
                            std::fabs ((double) (sub.getReadPointer (0)[i] - full.getReadPointer (0)[offset + i])));
    INFO ("scrub vs full-slice max diff = " << maxDiff);
    REQUIRE (maxDiff == 0.0);   // same decode path -> must be bit-identical
}
