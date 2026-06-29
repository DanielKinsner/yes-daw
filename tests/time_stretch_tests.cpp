#include "engine/TimeStretch.h"
#include "engine/nodes/TimeStretchNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

namespace {

constexpr double kSampleRate = 48000.0;
constexpr std::size_t kSourceFrames = 8192u;

using yesdaw::engine::PreparedTimeStretch;
using yesdaw::engine::TimeStretchStatus;

std::vector<float> makeStereoFixture()
{
    std::vector<float> samples (kSourceFrames * 2u, 0.0f);
    for (std::size_t frame = 0; frame < kSourceFrames; ++frame)
    {
        const double t = static_cast<double> (frame) / kSampleRate;
        samples[frame * 2u] = static_cast<float> (
            0.18 * std::sin (2.0 * std::numbers::pi * 220.0 * t)
            + 0.05 * std::sin (2.0 * std::numbers::pi * 937.0 * t));
        samples[frame * 2u + 1u] = static_cast<float> (
            0.13 * std::sin (2.0 * std::numbers::pi * 330.0 * t)
            - 0.04 * std::sin (2.0 * std::numbers::pi * 701.0 * t));
    }

    samples[17u * 2u] += 0.4f;
    samples[2048u * 2u + 1u] -= 0.3f;
    samples[4096u * 2u] += 0.2f;
    return samples;
}

std::vector<float> makeMonoFixture()
{
    std::vector<float> samples (kSourceFrames, 0.0f);
    for (std::size_t frame = 0; frame < kSourceFrames; ++frame)
    {
        const double t = static_cast<double> (frame) / kSampleRate;
        samples[frame] = static_cast<float> (
            0.16 * std::sin (2.0 * std::numbers::pi * 123.0 * t)
            + 0.07 * std::sin (2.0 * std::numbers::pi * 811.0 * t));
    }
    samples[1024u] += 0.25f;
    return samples;
}

double weightedSum (std::span<const float> samples)
{
    double sum = 0.0;
    for (std::size_t i = 0; i < samples.size(); ++i)
        sum += static_cast<double> (samples[i]) * static_cast<double> ((i % 29u) + 1u);
    return sum / static_cast<double> (samples.size());
}

double absMean (std::span<const float> samples)
{
    double sum = 0.0;
    for (const float sample : samples)
        sum += std::abs (static_cast<double> (sample));
    return sum / static_cast<double> (samples.size());
}

struct GoldenProbe
{
    std::uint64_t frame = 0;
    std::uint32_t channel = 0;
    float expected = 0.0f;
};

struct GoldenSummary
{
    double factor = 1.0;
    std::uint64_t frames = 0;
    double weighted = 0.0;
    double absolute = 0.0;
    std::array<GoldenProbe, 4> probes {};
};

void requireGoldenSummary (const PreparedTimeStretch& prepared, const GoldenSummary& golden)
{
    REQUIRE (prepared.outputFrames == golden.frames);
    INFO ("factor=" << golden.factor << " weighted=" << std::setprecision (17)
                    << weightedSum (prepared.interleavedSamples)
                    << " abs=" << absMean (prepared.interleavedSamples));
    CHECK (weightedSum (prepared.interleavedSamples) == Catch::Approx (golden.weighted).margin (0.0005));
    CHECK (absMean (prepared.interleavedSamples) == Catch::Approx (golden.absolute).margin (0.0005));

    const std::size_t channels = static_cast<std::size_t> (prepared.channels);
    for (const GoldenProbe& probe : golden.probes)
    {
        const std::size_t index = static_cast<std::size_t> (probe.frame) * channels
                                + static_cast<std::size_t> (probe.channel);
        INFO ("probe frame=" << probe.frame << " channel=" << probe.channel << std::setprecision (17)
                             << " actual=" << prepared.interleavedSamples[index]);
        CHECK (prepared.interleavedSamples[index] == Catch::Approx (probe.expected).margin (0.0005f));
    }
}

std::vector<float> renderNodeTimeline (const PreparedTimeStretch& prepared,
                                       std::int64_t timelineStart,
                                       std::int64_t renderFrames,
                                       std::span<const int> blockSizes)
{
    yesdaw::engine::TimeStretchNode node (
        700u,
        std::vector<float> (prepared.interleavedSamples.begin(), prepared.interleavedSamples.end()),
        static_cast<int> (prepared.channels),
        timelineStart);
    node.prepare (kSampleRate, 512);

    const int channels = static_cast<int> (prepared.channels);
    std::vector<float> rendered (
        static_cast<std::size_t> (renderFrames) * static_cast<std::size_t> (channels),
        0.0f);
    std::vector<std::vector<float>> planes (static_cast<std::size_t> (channels), std::vector<float> (512u, 0.0f));
    std::vector<float*> ptrs (static_cast<std::size_t> (channels), nullptr);
    for (int channel = 0; channel < channels; ++channel)
        ptrs[static_cast<std::size_t> (channel)] = planes[static_cast<std::size_t> (channel)].data();

    std::int64_t cursor = 0;
    std::size_t blockIndex = 0;
    while (cursor < renderFrames)
    {
        const int requested = blockSizes[blockIndex % blockSizes.size()];
        const int frames = static_cast<int> (
            std::min<std::int64_t> (renderFrames - cursor, static_cast<std::int64_t> (requested)));
        for (int channel = 0; channel < channels; ++channel)
            std::fill (planes[static_cast<std::size_t> (channel)].begin(),
                       planes[static_cast<std::size_t> (channel)].begin() + frames,
                       -99.0f);

        yesdaw::engine::EventStream events;
        yesdaw::engine::Transport transport;
        transport.projectSampleRate.hz = kSampleRate;
        transport.isPlaying = true;
        transport.hasTimelineFrame = true;
        transport.timelineFrame = cursor;
        const yesdaw::engine::AudioBlock audio { ptrs.data(), channels };
        node.process ({ audio, events, transport, frames });

        for (int frame = 0; frame < frames; ++frame)
            for (int channel = 0; channel < channels; ++channel)
                rendered[(static_cast<std::size_t> (cursor) + static_cast<std::size_t> (frame))
                         * static_cast<std::size_t> (channels) + static_cast<std::size_t> (channel)] =
                    planes[static_cast<std::size_t> (channel)][static_cast<std::size_t> (frame)];

        cursor += frames;
        ++blockIndex;
    }

    return rendered;
}

std::vector<float> expectedTimeline (const PreparedTimeStretch& prepared,
                                     std::int64_t timelineStart,
                                     std::int64_t renderFrames)
{
    const std::size_t channels = static_cast<std::size_t> (prepared.channels);
    std::vector<float> expected (static_cast<std::size_t> (renderFrames) * channels, 0.0f);
    for (std::int64_t frame = 0; frame < renderFrames; ++frame)
    {
        const std::int64_t local = frame - timelineStart;
        if (local < 0 || local >= static_cast<std::int64_t> (prepared.outputFrames))
            continue;

        for (std::size_t channel = 0; channel < channels; ++channel)
            expected[static_cast<std::size_t> (frame) * channels + channel] =
                prepared.interleavedSamples[static_cast<std::size_t> (local) * channels + channel];
    }
    return expected;
}

void requireSamplesClose (std::span<const float> actual, std::span<const float> expected)
{
    REQUIRE (actual.size() == expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i)
        REQUIRE (actual[i] == Catch::Approx (expected[i]).margin (0.000001f));
}

} // namespace

TEST_CASE ("Pinned Signalsmith Stretch version is the ADR-0030 dependency", "[h10][time-stretch]")
{
    const auto version = yesdaw::engine::timeStretchLibraryVersion();
    REQUIRE (version.major == 1u);
    REQUIRE (version.minor == 1u);
    REQUIRE (version.patch == 0u);
}

TEST_CASE ("Time-stretch preparation rejects malformed inputs before Signalsmith", "[h10][time-stretch]")
{
    const std::vector<float> stereo = makeStereoFixture();

    REQUIRE (yesdaw::engine::prepareTimeStretch ({}, 2u, kSampleRate, 1.0).status
             == TimeStretchStatus::EmptySource);
    REQUIRE (yesdaw::engine::prepareTimeStretch (stereo, 3u, kSampleRate, 1.0).status
             == TimeStretchStatus::UnsupportedChannelLayout);
    REQUIRE (yesdaw::engine::prepareTimeStretch ({ stereo.data(), 3u }, 2u, kSampleRate, 1.0).status
             == TimeStretchStatus::SampleCountNotFrameAligned);
    REQUIRE (yesdaw::engine::prepareTimeStretch (stereo, 2u, 0.0, 1.0).status
             == TimeStretchStatus::InvalidSampleRate);
    REQUIRE (yesdaw::engine::prepareTimeStretch (stereo, 2u, kSampleRate, 0.49).status
             == TimeStretchStatus::InvalidStretchFactor);
    REQUIRE (yesdaw::engine::prepareTimeStretch (stereo, 2u, kSampleRate, 2.01).status
             == TimeStretchStatus::InvalidStretchFactor);
    REQUIRE (yesdaw::engine::prepareTimeStretch (
                 stereo, 2u, kSampleRate, std::numeric_limits<double>::quiet_NaN())
             .status
             == TimeStretchStatus::InvalidStretchFactor);

    std::vector<float> nonFinite = stereo;
    nonFinite[9u] = std::numeric_limits<float>::infinity();
    REQUIRE (yesdaw::engine::prepareTimeStretch (nonFinite, 2u, kSampleRate, 1.0).status
             == TimeStretchStatus::NonFiniteInput);
}

TEST_CASE ("Time-stretch preparation has exact duration and checked-in golden fingerprints",
           "[h10][time-stretch][golden]")
{
    const std::vector<float> stereo = makeStereoFixture();
    const auto shorter = yesdaw::engine::prepareTimeStretch (stereo, 2u, kSampleRate, 0.75);
    const auto longer = yesdaw::engine::prepareTimeStretch (stereo, 2u, kSampleRate, 1.5);
    REQUIRE (shorter.ok());
    REQUIRE (longer.ok());

    for (const PreparedTimeStretch& prepared : { shorter, longer })
    {
        REQUIRE (prepared.channels == 2u);
        REQUIRE (prepared.sourceFrames == kSourceFrames);
        REQUIRE (prepared.interleavedSamples.size()
                 == static_cast<std::size_t> (prepared.outputFrames) * static_cast<std::size_t> (prepared.channels));
        for (const float sample : prepared.interleavedSamples)
            REQUIRE (std::isfinite (sample));
    }

    constexpr GoldenSummary kShorterGolden {
        0.75,
        6144u,
        0.0088911437055270905,
        0.096869113511072413,
        { GoldenProbe { 768u, 0u, -0.091402962803840637f },
          GoldenProbe { 1536u, 1u, -0.24674156308174133f },
          GoldenProbe { 3072u, 0u, 0.1934913843870163f },
          GoldenProbe { 6132u, 1u, 0.089817747473716736f } }
    };
    constexpr GoldenSummary kLongerGolden {
        1.5,
        12288u,
        0.0014688923205331921,
        0.09522708971400122,
        { GoldenProbe { 1536u, 0u, 0.0034115137532353401f },
          GoldenProbe { 3072u, 1u, -0.10578276216983795f },
          GoldenProbe { 6144u, 0u, 0.34352758526802063f },
          GoldenProbe { 12276u, 1u, -0.061190385371446609f } }
    };

    requireGoldenSummary (shorter, kShorterGolden);
    requireGoldenSummary (longer, kLongerGolden);
}

TEST_CASE ("Time-stretch output length rounds (not truncates) on a non-integer factor product",
           "[h10][time-stretch]")
{
    // The golden factors (0.75, 1.5) both give exact integer products with 8192, so a floor()/trunc()
    // regression in the output-length math would pass them. 8192 * 1.3 = 10649.6 must round to 10650.
    const std::vector<float> stereo = makeStereoFixture();
    constexpr double factor = 1.3;
    const auto prepared = yesdaw::engine::prepareTimeStretch (stereo, 2u, kSampleRate, factor);
    REQUIRE (prepared.ok());

    const auto expectedFrames =
        static_cast<std::uint64_t> (std::llround (static_cast<double> (kSourceFrames) * factor));
    REQUIRE (expectedFrames == 10650u); // pin the intent so a truncating impl (10649) bites
    REQUIRE (prepared.outputFrames == expectedFrames);
    REQUIRE (prepared.sourceFrames == kSourceFrames);
    REQUIRE (prepared.interleavedSamples.size()
             == static_cast<std::size_t> (prepared.outputFrames) * static_cast<std::size_t> (prepared.channels));
    for (const float sample : prepared.interleavedSamples)
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("TimeStretchNode renders prepared samples by absolute timeline frame across block splits",
           "[h10][time-stretch][node]")
{
    const std::vector<float> stereo = makeStereoFixture();
    const auto prepared = yesdaw::engine::prepareTimeStretch (stereo, 2u, kSampleRate, 1.5);
    REQUIRE (prepared.ok());

    const std::int64_t timelineStart = 17;
    const std::int64_t renderFrames = static_cast<std::int64_t> (prepared.outputFrames) + 64;
    const std::array<int, 4> irregularBlocks { 7, 31, 128, 19 };
    const std::array<int, 1> wholeBlocks { 512 };

    const std::vector<float> expected = expectedTimeline (prepared, timelineStart, renderFrames);
    const std::vector<float> irregular = renderNodeTimeline (prepared, timelineStart, renderFrames, irregularBlocks);
    const std::vector<float> whole = renderNodeTimeline (prepared, timelineStart, renderFrames, wholeBlocks);
    requireSamplesClose (irregular, expected);
    requireSamplesClose (whole, expected);
    requireSamplesClose (irregular, whole);

    for (int frame = 0; frame < timelineStart; ++frame)
    {
        REQUIRE (irregular[static_cast<std::size_t> (frame) * 2u] == 0.0f);
        REQUIRE (irregular[static_cast<std::size_t> (frame) * 2u + 1u] == 0.0f);
    }
    for (std::int64_t frame = timelineStart + static_cast<std::int64_t> (prepared.outputFrames);
         frame < renderFrames;
         ++frame)
    {
        REQUIRE (irregular[static_cast<std::size_t> (frame) * 2u] == 0.0f);
        REQUIRE (irregular[static_cast<std::size_t> (frame) * 2u + 1u] == 0.0f);
    }

    yesdaw::engine::TimeStretchNode node (
        701u,
        std::vector<float> (prepared.interleavedSamples.begin(), prepared.interleavedSamples.end()),
        2,
        timelineStart);
    const auto props = node.properties();
    REQUIRE (props.producesAudio);
    REQUIRE_FALSE (props.producesEvents);
    REQUIRE (props.channels == 2);
    REQUIRE (props.latencySamples == 0);
    REQUIRE (props.blockParallelSafe);
    REQUIRE (node.timelineStartFrames() == timelineStart);
    REQUIRE (node.preparedFrames() == static_cast<std::int64_t> (prepared.outputFrames));
    REQUIRE (node.directInputs().empty());
}

TEST_CASE ("TimeStretchNode fallback cursor resets for simple sequential tests", "[h10][time-stretch][node]")
{
    const std::vector<float> mono = makeMonoFixture();
    const auto prepared = yesdaw::engine::prepareTimeStretch (mono, 1u, kSampleRate, 0.75);
    REQUIRE (prepared.ok());

    yesdaw::engine::TimeStretchNode node (
        702u,
        std::vector<float> (prepared.interleavedSamples.begin(), prepared.interleavedSamples.end()),
        1);
    node.prepare (kSampleRate, 64);

    std::vector<float> storage (64u, 0.0f);
    float* outputs[1] = { storage.data() };
    yesdaw::engine::EventStream events;
    yesdaw::engine::Transport transport;
    transport.projectSampleRate.hz = kSampleRate;
    transport.isPlaying = true;
    transport.hasTimelineFrame = false;

    const yesdaw::engine::AudioBlock audio { outputs, 1 };
    node.process ({ audio, events, transport, 64 });
    requireSamplesClose (storage, std::span<const float> (prepared.interleavedSamples.data(), 64u));

    std::fill (storage.begin(), storage.end(), 0.0f);
    node.process ({ audio, events, transport, 64 });
    requireSamplesClose (storage, std::span<const float> (prepared.interleavedSamples.data() + 64u, 64u));

    std::fill (storage.begin(), storage.end(), 0.0f);
    node.reset();
    node.process ({ audio, events, transport, 64 });
    requireSamplesClose (storage, std::span<const float> (prepared.interleavedSamples.data(), 64u));
}
