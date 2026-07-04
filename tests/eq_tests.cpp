// YES DAW -- H14 CP4 EqNode gate.
//
// The response reference below is deliberately separate from EqNode's sample loop. It uses the closed-form
// TPT SVF transfer from the H14 plan, then compares it to an impulse response measured through the node.

#include "engine/nodes/EqNode.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::EqNode;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::Node;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Transport;
using yesdaw::engine::unmapToNormalized;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kSampleRate = 48000.0;
constexpr int kImpulseSize = 8192;
constexpr int kTotalFrames = 4096;
constexpr double kResponseToleranceDb = 0.25;
constexpr double kNullTolerance = 1.0e-6;

using BandType = EqNode::BandType;

struct RefCoefficients
{
    double g = 0.0;
    double k = 1.0;
    double m0 = 1.0;
    double m1 = 0.0;
    double m2 = 0.0;
};

struct TptCoefficients
{
    double a1 = 1.0;
    double a2 = 0.0;
    double a3 = 0.0;
    double m0 = 1.0;
    double m1 = 0.0;
    double m2 = 0.0;
};

struct TptState
{
    double ic1eq = 0.0;
    double ic2eq = 0.0;
};

struct BandSetting
{
    BandType type = BandType::Bell;
    double frequencyHz = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
};

enum class LocalMutation
{
    None,
    PerturbM0,
    DetuneG
};

[[nodiscard]] double clampFrequency (double frequencyHz, double sampleRate) noexcept
{
    const double maxHz = std::min (20000.0, 0.49 * sampleRate);
    const double fallback = EqNode::kDefaultFrequencyHz <= maxHz ? EqNode::kDefaultFrequencyHz : 20.0;
    return std::clamp (std::isfinite (frequencyHz) ? frequencyHz : fallback, 20.0, maxHz);
}

[[nodiscard]] double clampGainDb (double gainDb) noexcept
{
    return std::clamp (std::isfinite (gainDb) ? gainDb : EqNode::kDefaultGainDb, -24.0, 24.0);
}

[[nodiscard]] double clampQ (double q) noexcept
{
    return std::clamp (std::isfinite (q) ? q : EqNode::kDefaultQ, 0.1, 18.0);
}

[[nodiscard]] RefCoefficients makeRefCoefficients (const BandSetting& setting,
                                                   double sampleRate,
                                                   bool flipM1Sign = false,
                                                   bool detuneG = false)
{
    const double f = clampFrequency (setting.frequencyHz, sampleRate);
    const double q = clampQ (setting.q);
    const double db = clampGainDb (setting.gainDb);
    const double a = std::pow (10.0, db / 40.0);
    const double a2 = a * a;

    RefCoefficients c;
    c.g = std::tan (kPi * f / sampleRate);

    switch (setting.type)
    {
        case BandType::Bell:
            c.k = 1.0 / (q * a);
            c.m0 = 1.0;
            c.m1 = c.k * (a2 - 1.0);
            c.m2 = 0.0;
            break;

        case BandType::LowShelf:
            c.g /= std::sqrt (a);
            c.k = 1.0 / q;
            c.m0 = 1.0;
            c.m1 = c.k * (a - 1.0);
            c.m2 = a2 - 1.0;
            break;

        case BandType::HighShelf:
            c.g *= std::sqrt (a);
            c.k = 1.0 / q;
            c.m0 = a2;
            c.m1 = c.k * (1.0 - a) * a;
            c.m2 = 1.0 - a2;
            break;

        case BandType::Hpf:
            c.k = 1.0 / q;
            c.m0 = 1.0;
            c.m1 = -c.k;
            c.m2 = -1.0;
            break;

        case BandType::Lpf:
            c.k = 1.0 / q;
            c.m0 = 0.0;
            c.m1 = 0.0;
            c.m2 = 1.0;
            break;

        case BandType::Notch:
            c.k = 1.0 / q;
            c.m0 = 1.0;
            c.m1 = -c.k;
            c.m2 = 0.0;
            break;
    }

    if (flipM1Sign)
        c.m1 = -c.m1;
    if (detuneG)
        c.g *= 1.01;

    return c;
}

[[nodiscard]] TptCoefficients makeTptCoefficients (const BandSetting& setting,
                                                   double sampleRate,
                                                   LocalMutation mutation = LocalMutation::None)
{
    const RefCoefficients ref = makeRefCoefficients (setting, sampleRate, false, mutation == LocalMutation::DetuneG);
    const double a1 = 1.0 / (1.0 + ref.g * (ref.g + ref.k));
    TptCoefficients c { a1, ref.g * a1, ref.g * ref.g * a1, ref.m0, ref.m1, ref.m2 };
    if (mutation == LocalMutation::PerturbM0)
        c.m0 *= 1.01;
    return c;
}

[[nodiscard]] std::complex<double> hrefAtS (const RefCoefficients& c, std::complex<double> s)
{
    const std::complex<double> d = s * s + c.k * c.g * s + c.g * c.g;
    return c.m0 + c.m1 * (c.g * s) / d + c.m2 * (c.g * c.g) / d;
}

[[nodiscard]] std::complex<double> hrefAtFrequency (const BandSetting& setting, double sampleRate, double probeHz)
{
    const RefCoefficients c = makeRefCoefficients (setting, sampleRate);
    const std::complex<double> s { 0.0, std::tan (kPi * probeHz / sampleRate) };
    return hrefAtS (c, s);
}

[[nodiscard]] bool identityAnchorsPass (bool flipM1Sign)
{
    constexpr double db = 9.0;
    constexpr double q = 0.8;
    constexpr double f = 1000.0;
    const double a = std::pow (10.0, db / 40.0);
    const double a2 = a * a;
    const auto close = [] (double got, double expected) {
        return std::fabs (got - expected) <= 1.0e-9;
    };

    const RefCoefficients bell = makeRefCoefficients (BandSetting { BandType::Bell, f, db, q },
                                                      kSampleRate,
                                                      flipM1Sign);
    if (! close (std::abs (hrefAtS (bell, { 0.0, bell.g })), a2))
        return false;

    const RefCoefficients low = makeRefCoefficients (BandSetting { BandType::LowShelf, f, db, q },
                                                     kSampleRate,
                                                     flipM1Sign);
    if (! close (std::abs (hrefAtS (low, { 0.0, 0.0 })), a2))
        return false;
    if (! close (std::abs (low.m0), 1.0))
        return false;

    const RefCoefficients high = makeRefCoefficients (BandSetting { BandType::HighShelf, f, db, q },
                                                      kSampleRate,
                                                      flipM1Sign);
    if (! close (std::abs (hrefAtS (high, { 0.0, 0.0 })), 1.0))
        return false;
    if (! close (std::abs (high.m0), a2))
        return false;

    const RefCoefficients lpf = makeRefCoefficients (BandSetting { BandType::Lpf, f, 0.0, q },
                                                     kSampleRate,
                                                     flipM1Sign);
    if (! close (std::abs (hrefAtS (lpf, { 0.0, 0.0 })), 1.0))
        return false;

    const RefCoefficients hpf = makeRefCoefficients (BandSetting { BandType::Hpf, f, 0.0, q },
                                                     kSampleRate,
                                                     flipM1Sign);
    if (! close (std::abs (hpf.m0), 1.0))
        return false;

    const RefCoefficients notch = makeRefCoefficients (BandSetting { BandType::Notch, f, 0.0, q },
                                                       kSampleRate,
                                                       flipM1Sign);
    return close (std::abs (hrefAtS (notch, { 0.0, notch.g })), 0.0);
}

void fft (std::vector<std::complex<double>>& a)
{
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1u;
        for (; (j & bit) != 0u; bit >>= 1u)
            j ^= bit;
        j ^= bit;

        if (i < j)
            std::swap (a[i], a[j]);
    }

    for (std::size_t len = 2; len <= n; len <<= 1u)
    {
        const double angle = -2.0 * kPi / static_cast<double> (len);
        const std::complex<double> wLen { std::cos (angle), std::sin (angle) };
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> w { 1.0, 0.0 };
            for (std::size_t j = 0; j < len / 2u; ++j)
            {
                const std::complex<double> u = a[i + j];
                const std::complex<double> v = a[i + j + len / 2u] * w;
                a[i + j] = u + v;
                a[i + j + len / 2u] = u - v;
                w *= wLen;
            }
        }
    }
}

[[nodiscard]] std::vector<int> responseProbeBins()
{
    std::vector<int> bins;
    bins.reserve (24);
    const double minBin = std::ceil (40.0 * static_cast<double> (kImpulseSize) / kSampleRate);
    const double maxBin = std::floor (18000.0 * static_cast<double> (kImpulseSize) / kSampleRate);
    int previous = 0;
    for (int i = 0; i < 24; ++i)
    {
        const double t = static_cast<double> (i) / 23.0;
        int bin = static_cast<int> (std::round (minBin * std::pow (maxBin / minBin, t)));
        if (bin <= previous)
            bin = previous + 1;
        bin = std::min (bin, static_cast<int> (maxBin));
        bins.push_back (bin);
        previous = bin;
    }
    return bins;
}

[[nodiscard]] std::vector<float> deterministicStereoInput (int frames)
{
    std::vector<float> input (static_cast<std::size_t> (frames) * 2u, 0.0f);
    for (int i = 0; i < frames; ++i)
    {
        const double x = 0.18 * std::sin (0.017 * static_cast<double> (i))
                       + 0.11 * std::sin (0.071 * static_cast<double> (i))
                       + (i % 97 == 0 ? 0.07 : 0.0);
        input[static_cast<std::size_t> (i) * 2u] = static_cast<float> (x);
        input[static_cast<std::size_t> (i) * 2u + 1u] = static_cast<float> (-0.5 * x);
    }
    return input;
}

void processBlock (EqNode& node,
                   std::vector<float>& interleaved,
                   int offset,
                   int frames,
                   EventStream& events)
{
    Transport transport;
    transport.hasTimelineFrame = true;
    transport.timelineFrame = offset;

    float* channels[2] = {
        interleaved.data() + static_cast<std::size_t> (offset) * 2u,
        interleaved.data() + static_cast<std::size_t> (offset) * 2u + 1u,
    };

    std::vector<float> left (static_cast<std::size_t> (frames), 0.0f);
    std::vector<float> right (static_cast<std::size_t> (frames), 0.0f);
    for (int i = 0; i < frames; ++i)
    {
        left[static_cast<std::size_t> (i)] = channels[0][static_cast<std::size_t> (i) * 2u];
        right[static_cast<std::size_t> (i)] = channels[1][static_cast<std::size_t> (i) * 2u];
    }

    float* planar[2] = { left.data(), right.data() };
    Node& iface = node;
    iface.process (ProcessArgs { AudioBlock { planar, 2 }, events, transport, frames });

    for (int i = 0; i < frames; ++i)
    {
        channels[0][static_cast<std::size_t> (i) * 2u] = left[static_cast<std::size_t> (i)];
        channels[1][static_cast<std::size_t> (i) * 2u] = right[static_cast<std::size_t> (i)];
    }
}

[[nodiscard]] std::vector<float> renderNode (const std::vector<int>& schedule,
                                             const std::vector<float>& input,
                                             const BandSetting& setting,
                                             int eventFrame = -1,
                                             bool wrongEventAtBlockStart = false)
{
    int maxBlock = 1;
    for (const int block : schedule)
        maxBlock = std::max (maxBlock, block);

    EqNode node (100);
    node.setBand (0, setting.type, setting.frequencyHz, setting.gainDb, setting.q);
    Node& iface = node;
    iface.prepare (kSampleRate, maxBlock);

    std::vector<float> out = input;
    const int totalFrames = static_cast<int> (out.size() / 2u);
    int offset = 0;
    std::size_t scheduleIndex = 0;
    while (offset < totalFrames)
    {
        const int blockSize = schedule[scheduleIndex % schedule.size()];
        const int frames = std::min (blockSize, totalFrames - offset);
        std::array<Event, 1> eventStorage {};
        std::span<const Event> eventSpan;
        if (eventFrame >= offset && eventFrame < offset + frames)
        {
            const double normalizedGain = unmapToNormalized (
                EqNode::parameterSpec (EqNode::parameterIdFor (0, EqNode::kGainParamOffset)),
                12.0);
            const std::uint32_t time = wrongEventAtBlockStart
                ? 0u
                : static_cast<std::uint32_t> (eventFrame - offset);
            eventStorage[0] = makeParameterChangeEvent (time,
                                                        100,
                                                        EqNode::parameterIdFor (0, EqNode::kGainParamOffset),
                                                        normalizedGain);
            eventSpan = std::span<const Event> (eventStorage.data(), 1u);
        }

        EventStream events { eventSpan };
        processBlock (node, out, offset, frames, events);
        offset += frames;
        ++scheduleIndex;
    }

    return out;
}

[[nodiscard]] double processLocalSample (const TptCoefficients& c, TptState& state, double v0) noexcept
{
    const double v3 = v0 - state.ic2eq;
    const double v1 = c.a1 * state.ic1eq + c.a2 * v3;
    const double v2 = state.ic2eq + c.a2 * state.ic1eq + c.a3 * v3;
    state.ic1eq = 2.0 * v1 - state.ic1eq;
    state.ic2eq = 2.0 * v2 - state.ic2eq;
    return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
}

[[nodiscard]] std::vector<float> renderLocalMono (const BandSetting& setting,
                                                  int frames,
                                                  int blockSize,
                                                  LocalMutation mutation,
                                                  bool resetAtBlockBoundary)
{
    const TptCoefficients c = makeTptCoefficients (setting, kSampleRate, mutation);
    TptState state;
    std::vector<float> out (static_cast<std::size_t> (frames), 0.0f);
    out[0] = 1.0f;

    for (int offset = 0; offset < frames; offset += blockSize)
    {
        if (resetAtBlockBoundary)
            state = TptState {};

        const int n = std::min (blockSize, frames - offset);
        for (int i = 0; i < n; ++i)
        {
            const std::size_t index = static_cast<std::size_t> (offset + i);
            out[index] = static_cast<float> (processLocalSample (c, state, out[index]));
        }
    }

    return out;
}

[[nodiscard]] std::vector<float> renderNodeImpulse (const BandSetting& setting)
{
    std::vector<float> input (static_cast<std::size_t> (kImpulseSize) * 2u, 0.0f);
    input[0] = 1.0f;
    input[1] = 1.0f;
    const std::vector<float> interleaved = renderNode ({ 512 }, input, setting);
    std::vector<float> mono (static_cast<std::size_t> (kImpulseSize), 0.0f);
    for (int i = 0; i < kImpulseSize; ++i)
        mono[static_cast<std::size_t> (i)] = interleaved[static_cast<std::size_t> (i) * 2u];
    return mono;
}

[[nodiscard]] bool responseGatePasses (const std::vector<float>& impulseResponse, const BandSetting& setting)
{
    std::vector<std::complex<double>> spectrum (static_cast<std::size_t> (kImpulseSize));
    for (std::size_t i = 0; i < spectrum.size(); ++i)
        spectrum[i] = { static_cast<double> (impulseResponse[i]), 0.0 };

    fft (spectrum);

    for (const int bin : responseProbeBins())
    {
        const double probeHz = static_cast<double> (bin) * kSampleRate / static_cast<double> (kImpulseSize);
        const double measured = std::max (std::abs (spectrum[static_cast<std::size_t> (bin)]), 1.0e-12);
        const double expected = std::max (std::abs (hrefAtFrequency (setting, kSampleRate, probeHz)), 1.0e-12);
        const double deltaDb = 20.0 * std::log10 (measured / expected);
        if (std::fabs (deltaDb) > kResponseToleranceDb)
            return false;
    }

    return true;
}

[[nodiscard]] bool neutralLocalNullGatePasses (LocalMutation mutation)
{
    const TptCoefficients c = makeTptCoefficients (BandSetting { BandType::Bell, 1000.0, 0.0, 1.0 },
                                                   kSampleRate,
                                                   mutation);
    TptState state;
    double maxAbs = 0.0;
    for (int i = 0; i < 1024; ++i)
    {
        const double input = 0.31 * std::sin (0.03 * static_cast<double> (i)) + (i == 17 ? 0.5 : 0.0);
        const double output = processLocalSample (c, state, input);
        maxAbs = std::max (maxAbs, std::fabs (output - input));
    }
    return maxAbs <= kNullTolerance;
}

[[nodiscard]] bool blockResetNegativeControlPasses()
{
    const BandSetting setting { BandType::Bell, 900.0, 12.0, 5.0 };
    const std::vector<float> reference = renderLocalMono (setting,
                                                          kTotalFrames,
                                                          512,
                                                          LocalMutation::None,
                                                          false);
    const std::vector<float> reset = renderLocalMono (setting,
                                                      kTotalFrames,
                                                      64,
                                                      LocalMutation::None,
                                                      true);
    return reference == reset;
}

[[nodiscard]] double maxAdjacentDelta (const std::vector<float>& interleaved, int firstFrame)
{
    double maxDelta = 0.0;
    const int frames = static_cast<int> (interleaved.size() / 2u);
    for (int i = std::max (1, firstFrame); i < frames; ++i)
    {
        const double a = interleaved[static_cast<std::size_t> (i) * 2u];
        const double b = interleaved[static_cast<std::size_t> (i - 1) * 2u];
        maxDelta = std::max (maxDelta, std::fabs (a - b));
    }
    return maxDelta;
}

[[nodiscard]] bool eventAtBlockStartNegativeControlPasses()
{
    const BandSetting setting { BandType::LowShelf, 250.0, 0.0, 0.707 };
    std::vector<float> input (static_cast<std::size_t> (kTotalFrames) * 2u, 0.20f);
    const std::vector<float> a = renderNode ({ 512 }, input, setting, 777, true);
    const std::vector<float> b = renderNode ({ 64 }, input, setting, 777, true);
    return a == b;
}

} // namespace

TEST_CASE ("EqNode reference identity anchors match the H14 closed form", "[eq][reference][anchors]")
{
    REQUIRE (identityAnchorsPass (false));
    REQUIRE_FALSE (identityAnchorsPass (true)); // negative control: mistyped m1 sign must bite
}

TEST_CASE ("EqNode ParamIDs are sequential from band times sixteen", "[eq][params]")
{
    for (int band = 0; band < EqNode::kBands; ++band)
    {
        REQUIRE (EqNode::parameterIdFor (band, EqNode::kTypeParamOffset)
                 == static_cast<yesdaw::engine::ParameterId> (band * EqNode::kParamsPerBand));
        REQUIRE (EqNode::parameterSpec (EqNode::parameterIdFor (band, EqNode::kFrequencyParamOffset)).mapping
                 == yesdaw::engine::ParamMapping::Log);
        REQUIRE (EqNode::parameterSpec (EqNode::parameterIdFor (band, EqNode::kGainParamOffset)).mapping
                 == yesdaw::engine::ParamMapping::Db);
        REQUIRE (EqNode::parameterSpec (EqNode::parameterIdFor (band, EqNode::kQParamOffset)).mapping
                 == yesdaw::engine::ParamMapping::Log);
    }
}

TEST_CASE ("EqNode neutral default is bit-exact even after state is polluted", "[eq][null]")
{
    EqNode node (100);
    node.setBand (0, BandType::Bell, 700.0, 18.0, 4.0);
    node.setBand (1, BandType::LowShelf, 180.0, -9.0, 0.8);
    node.setBand (2, BandType::HighShelf, 7000.0, 6.0, 0.9);
    node.prepare (kSampleRate, 256);

    std::vector<float> prime = deterministicStereoInput (2048);
    EventStream emptyEvents;
    processBlock (node, prime, 0, 2048, emptyEvents);

    for (int band = 0; band < EqNode::kBands; ++band)
        node.setBand (band, BandType::Bell, EqNode::kDefaultFrequencyHz, 0.0, EqNode::kDefaultQ);

    std::vector<float> input = deterministicStereoInput (1024);
    const std::vector<float> expected = input;
    EventStream secondEmpty;
    processBlock (node, input, 0, 1024, secondEmpty);

    REQUIRE (input == expected);
    REQUIRE (neutralLocalNullGatePasses (LocalMutation::None));
    REQUIRE_FALSE (neutralLocalNullGatePasses (LocalMutation::PerturbM0));
}

TEST_CASE ("EqNode measured frequency response matches the independent reference", "[eq][response]")
{
    const std::vector<int> bins = responseProbeBins();
    const double center = static_cast<double> (bins[15]) * kSampleRate / static_cast<double> (kImpulseSize);

    const std::array<BandSetting, 6> settings {
        BandSetting { BandType::Bell, center, 9.0, 1.2 },
        BandSetting { BandType::LowShelf, 350.0, 9.0, 0.707 },
        BandSetting { BandType::HighShelf, 5000.0, -9.0, 0.707 },
        BandSetting { BandType::Hpf, 180.0, 0.0, 0.707 },
        BandSetting { BandType::Lpf, 6000.0, 0.0, 0.707 },
        BandSetting { BandType::Notch, 1350.0, 0.0, 0.6 },
    };

    for (const BandSetting& setting : settings)
    {
        INFO ("band type " << static_cast<int> (setting.type));
        REQUIRE (responseGatePasses (renderNodeImpulse (setting), setting));
    }
}

TEST_CASE ("EqNode response negative control catches a one percent g detune", "[eq][response][negative]")
{
    const std::vector<int> bins = responseProbeBins();
    const double center = static_cast<double> (bins[15]) * kSampleRate / static_cast<double> (kImpulseSize);
    const BandSetting setting { BandType::Bell, center, 24.0, 18.0 };
    const std::vector<float> detuned = renderLocalMono (setting,
                                                        kImpulseSize,
                                                        512,
                                                        LocalMutation::DetuneG,
                                                        false);
    REQUIRE_FALSE (responseGatePasses (detuned, setting));
}

TEST_CASE ("EqNode stays finite under hostile parameters and the 20 kHz clamp case", "[eq][robust]")
{
    EqNode node (100);
    node.prepare (44100.0, 128);
    node.setBand (0, BandType::Bell, 20000.0, 24.0, 18.0);

    const std::array<double, 5> hostile {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -10.0,
        10.0,
    };
    std::array<Event, 15> events {};
    std::size_t count = 0;
    for (const double value : hostile)
    {
        events[count++] = makeParameterChangeEvent (0, 100, EqNode::parameterIdFor (0, EqNode::kFrequencyParamOffset), value);
        events[count++] = makeParameterChangeEvent (0, 100, EqNode::parameterIdFor (0, EqNode::kGainParamOffset), value);
        events[count++] = makeParameterChangeEvent (0, 100, EqNode::parameterIdFor (0, EqNode::kQParamOffset), value);
    }

    std::vector<float> input = deterministicStereoInput (4096);
    EventStream stream { std::span<const Event> (events.data(), count) };
    processBlock (node, input, 0, 4096, stream);

    for (const float sample : input)
    {
        REQUIRE (std::isfinite (sample));
        REQUIRE (std::fabs (sample) < 1000.0f);
    }
}

TEST_CASE ("EqNode output is bit-identical across H14 block schedules", "[eq][blocksize]")
{
    const BandSetting setting { BandType::Bell, 1200.0, 12.0, 3.5 };
    const std::vector<float> input = deterministicStereoInput (kTotalFrames);
    const std::vector<float> reference = renderNode ({ 512 }, input, setting);

    for (const int block : { 1, 2, 3, 4, 5, 6, 7, 8, 9, 64, 128, 333, 512 })
    {
        INFO ("block size " << block);
        REQUIRE (renderNode ({ block }, input, setting) == reference);
    }

    REQUIRE_FALSE (blockResetNegativeControlPasses());
}

TEST_CASE ("EqNode parameter smoothing is event-offset anchored across schedules", "[eq][automation]")
{
    const BandSetting setting { BandType::LowShelf, 250.0, 0.0, 0.707 };
    std::vector<float> input (static_cast<std::size_t> (kTotalFrames) * 2u, 0.20f);

    const int eventFrame = 777;
    const std::vector<float> reference = renderNode ({ 512 }, input, setting, eventFrame);
    REQUIRE (renderNode ({ 1 }, input, setting, eventFrame) == reference);
    REQUIRE (renderNode ({ 7 }, input, setting, eventFrame) == reference);
    REQUIRE (renderNode ({ 64 }, input, setting, eventFrame) == reference);
    REQUIRE (renderNode ({ 333 }, input, setting, eventFrame) == reference);

    for (int frame = 0; frame < eventFrame; ++frame)
        REQUIRE (reference[static_cast<std::size_t> (frame) * 2u] == 0.20f);

    bool changedAfterEvent = false;
    for (int frame = eventFrame + 16; frame < eventFrame + 256; ++frame)
        changedAfterEvent = changedAfterEvent
                          || reference[static_cast<std::size_t> (frame) * 2u] != 0.20f;
    REQUIRE (changedAfterEvent);

    INFO ("max adjacent delta after event = " << maxAdjacentDelta (reference, eventFrame));
    REQUIRE (maxAdjacentDelta (reference, eventFrame) <= 0.08);

    REQUIRE_FALSE (eventAtBlockStartNegativeControlPasses());
}
