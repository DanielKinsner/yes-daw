// YES DAW -- H14 CP6 FxDelayNode gate.
//
// The checks below exercise Appendix A3: tap alignment, feedback decay, damping, ping-pong,
// delay-time crossfade, tail formula, hostile inputs, block-size independence, and event anchoring.

#include "engine/nodes/FxDelayNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::FxDelayNode;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::Node;
using yesdaw::engine::ParamMapping;
using yesdaw::engine::ParamSmoothing;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Transport;
using yesdaw::engine::unmapToNormalized;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kSampleRate = 48000.0;
constexpr int kTotalFrames = 8192;
constexpr double kUnityTolerance = 1.0e-6;

struct DelaySetting
{
    double timeLeftMs = FxDelayNode::kDefaultTimeMs;
    double timeRightMs = FxDelayNode::kDefaultTimeMs;
    double feedback = FxDelayNode::kDefaultFeedback;
    double dampingHz = FxDelayNode::kDefaultDampingHz;
    bool pingPong = false;
    double mix = FxDelayNode::kDefaultMix;
};

struct StereoBuffer
{
    std::vector<float> left;
    std::vector<float> right;
};

[[nodiscard]] StereoBuffer makeImpulse (int frames, float left = 1.0f, float right = 0.0f)
{
    StereoBuffer buffer;
    buffer.left.assign (static_cast<std::size_t> (frames), 0.0f);
    buffer.right.assign (static_cast<std::size_t> (frames), 0.0f);
    if (frames > 0)
    {
        buffer.left[0] = left;
        buffer.right[0] = right;
    }
    return buffer;
}

[[nodiscard]] StereoBuffer makeSine (int frames, double frequencyHz, double amplitude)
{
    StereoBuffer buffer;
    buffer.left.resize (static_cast<std::size_t> (frames));
    buffer.right.resize (static_cast<std::size_t> (frames));
    for (int i = 0; i < frames; ++i)
    {
        const float sample = static_cast<float> (amplitude * std::sin (2.0 * kPi * frequencyHz * static_cast<double> (i) / kSampleRate));
        buffer.left[static_cast<std::size_t> (i)] = sample;
        buffer.right[static_cast<std::size_t> (i)] = -0.5f * sample;
    }
    return buffer;
}

[[nodiscard]] StereoBuffer deterministicStereoInput (int frames)
{
    StereoBuffer buffer;
    buffer.left.resize (static_cast<std::size_t> (frames));
    buffer.right.resize (static_cast<std::size_t> (frames));
    for (int i = 0; i < frames; ++i)
    {
        const double x = 0.19 * std::sin (0.021 * static_cast<double> (i))
                       + 0.07 * std::sin (0.073 * static_cast<double> (i))
                       + (i % 97 == 0 ? 0.15 : 0.0);
        buffer.left[static_cast<std::size_t> (i)] = static_cast<float> (x);
        buffer.right[static_cast<std::size_t> (i)] = static_cast<float> (-0.6 * x);
    }
    return buffer;
}

[[nodiscard]] std::vector<float> flatten (const StereoBuffer& buffer)
{
    std::vector<float> out;
    out.reserve (buffer.left.size() * 2u);
    for (std::size_t i = 0; i < buffer.left.size(); ++i)
    {
        out.push_back (buffer.left[i]);
        out.push_back (buffer.right[i]);
    }
    return out;
}

[[nodiscard]] double maxAbsDiff (const StereoBuffer& a, const StereoBuffer& b)
{
    double maxDiff = 0.0;
    for (std::size_t i = 0; i < a.left.size(); ++i)
    {
        maxDiff = std::max (maxDiff, std::fabs (static_cast<double> (a.left[i]) - static_cast<double> (b.left[i])));
        maxDiff = std::max (maxDiff, std::fabs (static_cast<double> (a.right[i]) - static_cast<double> (b.right[i])));
    }
    return maxDiff;
}

[[nodiscard]] double maxAbsSample (const StereoBuffer& buffer, int firstFrame, int lastFrame)
{
    double peak = 0.0;
    const int end = std::min (lastFrame, static_cast<int> (buffer.left.size()));
    for (int i = std::max (0, firstFrame); i < end; ++i)
    {
        peak = std::max (peak, std::fabs (static_cast<double> (buffer.left[static_cast<std::size_t> (i)])));
        peak = std::max (peak, std::fabs (static_cast<double> (buffer.right[static_cast<std::size_t> (i)])));
    }
    return peak;
}

[[nodiscard]] double maxAdjacentDeltaLeft (const StereoBuffer& buffer, int firstFrame, int lastFrame)
{
    double maxDelta = 0.0;
    const int begin = std::max (1, firstFrame);
    const int end = std::min (lastFrame, static_cast<int> (buffer.left.size()));
    for (int i = begin; i < end; ++i)
    {
        const double a = buffer.left[static_cast<std::size_t> (i)];
        const double b = buffer.left[static_cast<std::size_t> (i - 1)];
        maxDelta = std::max (maxDelta, std::fabs (a - b));
    }
    return maxDelta;
}

[[nodiscard]] double rmsSegment (const std::vector<float>& samples, int firstFrame, int frames)
{
    double sum = 0.0;
    for (int i = 0; i < frames; ++i)
    {
        const double sample = samples[static_cast<std::size_t> (firstFrame + i)];
        sum += sample * sample;
    }
    return std::sqrt (sum / static_cast<double> (frames));
}

void setNodeParameters (FxDelayNode& node, const DelaySetting& setting)
{
    node.setParameters (setting.timeLeftMs,
                        setting.timeRightMs,
                        setting.feedback,
                        setting.dampingHz,
                        setting.pingPong,
                        setting.mix);
}

void processBlock (FxDelayNode& node,
                   StereoBuffer& buffer,
                   int offset,
                   int frames,
                   EventStream& events)
{
    Transport transport;
    transport.hasTimelineFrame = true;
    transport.timelineFrame = offset;

    float* channels[2] = {
        buffer.left.data() + offset,
        buffer.right.data() + offset,
    };

    Node& iface = node;
    iface.process (ProcessArgs { AudioBlock { channels, 2 }, events, transport, frames });
}

[[nodiscard]] StereoBuffer renderNode (const std::vector<int>& schedule,
                                       const StereoBuffer& input,
                                       const DelaySetting& setting,
                                       int eventFrame = -1,
                                       yesdaw::engine::ParameterId eventParam = FxDelayNode::kMixParamId,
                                       double eventRealValue = FxDelayNode::kDefaultMix,
                                       bool wrongEventAtBlockStart = false,
                                       bool resetAtBlockBoundary = false)
{
    int maxBlock = 1;
    for (const int block : schedule)
        maxBlock = std::max (maxBlock, block);

    FxDelayNode node (100);
    setNodeParameters (node, setting);
    Node& iface = node;
    iface.prepare (kSampleRate, maxBlock);

    StereoBuffer out = input;
    const int totalFrames = static_cast<int> (out.left.size());
    int offset = 0;
    std::size_t scheduleIndex = 0;
    while (offset < totalFrames)
    {
        if (resetAtBlockBoundary && offset > 0)
            node.reset();

        const int blockSize = schedule[scheduleIndex % schedule.size()];
        const int frames = std::min (blockSize, totalFrames - offset);
        std::array<Event, 1> eventStorage {};
        std::span<const Event> eventSpan;
        if (eventFrame >= offset && eventFrame < offset + frames)
        {
            const double normalized = unmapToNormalized (FxDelayNode::parameterSpec (eventParam), eventRealValue);
            const std::uint32_t time = wrongEventAtBlockStart
                ? 0u
                : static_cast<std::uint32_t> (eventFrame - offset);
            eventStorage[0] = makeParameterChangeEvent (time, 100, eventParam, normalized);
            eventSpan = std::span<const Event> (eventStorage.data(), 1u);
        }

        EventStream events { eventSpan };
        processBlock (node, out, offset, frames, events);
        offset += frames;
        ++scheduleIndex;
    }

    return out;
}

[[nodiscard]] bool neutralGatePassesLocal (bool perturbDryTerm)
{
    const StereoBuffer input = deterministicStereoInput (2048);
    StereoBuffer out = input;
    const double dry = perturbDryTerm ? 0.99 : 1.0;
    for (std::size_t i = 0; i < out.left.size(); ++i)
    {
        out.left[i] = static_cast<float> (static_cast<double> (out.left[i]) * dry);
        out.right[i] = static_cast<float> (static_cast<double> (out.right[i]) * dry);
    }

    return maxAbsDiff (input, out) <= kUnityTolerance;
}

[[nodiscard]] bool tapAlignmentGatePassesWithExpectedOffset (int expectedOffset)
{
    const DelaySetting setting { 10.0, 10.0, 0.0, FxDelayNode::kMaxDampingHz, false, 1.0 };
    const int delay = static_cast<int> (FxDelayNode::delaySamplesForTimeMs (kSampleRate, setting.timeLeftMs));
    const StereoBuffer out = renderNode ({ 257 }, makeImpulse (delay + 128), setting);

    for (int frame = 0; frame < static_cast<int> (out.left.size()); ++frame)
    {
        const double expected = frame == delay + expectedOffset ? 1.0 : 0.0;
        if (std::fabs (static_cast<double> (out.left[static_cast<std::size_t> (frame)]) - expected) > 1.0e-6)
            return false;
        if (std::fabs (static_cast<double> (out.right[static_cast<std::size_t> (frame)])) > 1.0e-6)
            return false;
    }

    return true;
}

[[nodiscard]] bool blockResetNegativeControlPasses()
{
    const DelaySetting setting { 9.0, 13.0, 0.45, FxDelayNode::kMaxDampingHz, true, 0.7 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    return flatten (renderNode ({ 512 }, input, setting))
        == flatten (renderNode ({ 64 }, input, setting, -1, FxDelayNode::kMixParamId, 0.0, false, true));
}

[[nodiscard]] bool eventAtBlockStartNegativeControlPasses()
{
    const DelaySetting setting { 5.0, 5.0, 0.0, FxDelayNode::kMaxDampingHz, false, 0.0 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    return flatten (renderNode ({ 512 }, input, setting, 777, FxDelayNode::kMixParamId, 1.0, true))
        == flatten (renderNode ({ 64 }, input, setting, 777, FxDelayNode::kMixParamId, 1.0, true));
}

[[nodiscard]] double onePoleMagnitude (double cutoffHz, double frequencyHz)
{
    const double alpha = 1.0 - std::exp (-2.0 * kPi * cutoffHz / kSampleRate);
    const double pole = 1.0 - alpha;
    const double omega = 2.0 * kPi * frequencyHz / kSampleRate;
    return alpha / std::sqrt (1.0 + pole * pole - 2.0 * pole * std::cos (omega));
}

[[nodiscard]] double dampingRatioForProbe (double frequencyHz)
{
    constexpr double cutoffHz = 2000.0;
    constexpr double feedback = 0.8;
    constexpr int burstFrames = 8192;
    const DelaySetting setting { 200.0, 200.0, feedback, cutoffHz, false, 1.0 };
    const int delay = static_cast<int> (FxDelayNode::delaySamplesForTimeMs (kSampleRate, setting.timeLeftMs));
    StereoBuffer input = makeSine (2 * delay + burstFrames + 64, frequencyHz, 0.5);
    for (int i = burstFrames; i < static_cast<int> (input.left.size()); ++i)
    {
        input.left[static_cast<std::size_t> (i)] = 0.0f;
        input.right[static_cast<std::size_t> (i)] = 0.0f;
    }

    const StereoBuffer out = renderNode ({ 512 }, input, setting);
    const int analysisOffset = 3072;
    const int analysisFrames = 4096;
    const double first = rmsSegment (out.left, delay + analysisOffset, analysisFrames);
    const double second = rmsSegment (out.left, 2 * delay + analysisOffset, analysisFrames);
    return second / (std::max (first, 1.0e-12) * feedback);
}

} // namespace

TEST_CASE ("FxDelayNode ParamSpec table exposes stable CP6 ranges", "[fx-delay][params]")
{
    REQUIRE (FxDelayNode::parameterSpec (FxDelayNode::kTimeLeftParamId).mapping == ParamMapping::Log);
    REQUIRE (FxDelayNode::parameterSpec (FxDelayNode::kTimeRightParamId).mapping == ParamMapping::Log);
    REQUIRE (FxDelayNode::parameterSpec (FxDelayNode::kFeedbackParamId).max == 0.95);
    REQUIRE (FxDelayNode::parameterSpec (FxDelayNode::kDampingParamId).mapping == ParamMapping::Log);
    REQUIRE (FxDelayNode::parameterSpec (FxDelayNode::kPingPongParamId).smoothing == ParamSmoothing::None);
    REQUIRE (FxDelayNode::parameterSpec (FxDelayNode::kMixParamId).max == 1.0);

    FxDelayNode node (100);
    REQUIRE_FALSE (node.properties().blockParallelSafe);
    REQUIRE (node.properties().latencySamples == 0);
    REQUIRE (node.properties().channels == 2);
}

TEST_CASE ("FxDelayNode mix zero is dry and the null gate bites", "[fx-delay][null]")
{
    const DelaySetting setting { 10.0, 13.0, 0.65, 1500.0, true, 0.0 };
    const StereoBuffer input = deterministicStereoInput (2048);
    const StereoBuffer out = renderNode ({ 257 }, input, setting);
    REQUIRE (maxAbsDiff (input, out) <= kUnityTolerance);

    REQUIRE (neutralGatePassesLocal (false));
    REQUIRE_FALSE (neutralGatePassesLocal (true));
}

TEST_CASE ("FxDelayNode impulse tap aligns to round time times sample rate", "[fx-delay][tap]")
{
    REQUIRE (tapAlignmentGatePassesWithExpectedOffset (0));
    REQUIRE_FALSE (tapAlignmentGatePassesWithExpectedOffset (1));
}

TEST_CASE ("FxDelayNode feedback decay taps follow powers of feedback", "[fx-delay][feedback]")
{
    const DelaySetting setting { 7.0, 7.0, 0.5, FxDelayNode::kMaxDampingHz, false, 1.0 };
    const int delay = static_cast<int> (FxDelayNode::delaySamplesForTimeMs (kSampleRate, setting.timeLeftMs));
    const StereoBuffer out = renderNode ({ 128 }, makeImpulse (delay * 7 + 16), setting);

    for (int tap = 0; tap < 6; ++tap)
    {
        const int frame = (tap + 1) * delay;
        const double expected = std::pow (setting.feedback, static_cast<double> (tap));
        INFO ("tap " << tap << " frame " << frame);
        REQUIRE (out.left[static_cast<std::size_t> (frame)] == Catch::Approx (expected).margin (1.0e-4));
        REQUIRE (std::fabs (static_cast<double> (out.right[static_cast<std::size_t> (frame)])) <= 1.0e-6);
    }
}

TEST_CASE ("FxDelayNode damping tap ratio matches one-pole magnitude", "[fx-delay][damping]")
{
    for (const double frequencyHz : { 500.0, 8000.0 })
    {
        const double ratio = dampingRatioForProbe (frequencyHz);
        const double expected = onePoleMagnitude (2000.0, frequencyHz);
        const double errorDb = 20.0 * std::log10 (std::max (ratio, 1.0e-12) / expected);
        INFO ("frequency " << frequencyHz << " ratio " << ratio << " expected " << expected << " errorDb " << errorDb);
        REQUIRE (std::fabs (errorDb) <= 0.5);
    }
}

TEST_CASE ("FxDelayNode ping-pong sends a left impulse to the first right tap", "[fx-delay][pingpong]")
{
    const DelaySetting setting { 5.0, 5.0, 0.0, FxDelayNode::kMaxDampingHz, true, 1.0 };
    const int delay = static_cast<int> (FxDelayNode::delaySamplesForTimeMs (kSampleRate, setting.timeLeftMs));
    const StereoBuffer out = renderNode ({ 129 }, makeImpulse (delay + 32), setting);

    REQUIRE (std::fabs (static_cast<double> (out.left[static_cast<std::size_t> (delay)])) <= 1.0e-6);
    REQUIRE (out.right[static_cast<std::size_t> (delay)] == Catch::Approx (1.0).margin (1.0e-6));
}

TEST_CASE ("FxDelayNode delay time changes crossfade without a click-sized step", "[fx-delay][time-change]")
{
    const DelaySetting setting { 20.0, 20.0, 0.0, FxDelayNode::kMaxDampingHz, false, 1.0 };
    const int eventFrame = 3000;
    const int rampEnd = eventFrame + static_cast<int> (std::round (0.005 * kSampleRate));
    const int crossfadeEnd = rampEnd + static_cast<int> (std::round (0.020 * kSampleRate));
    const StereoBuffer input = makeSine (crossfadeEnd + 512, 1234.0, 0.75);
    const StereoBuffer out = renderNode ({ 333 }, input, setting, eventFrame, FxDelayNode::kTimeLeftParamId, 37.0);

    REQUIRE (maxAbsSample (out, rampEnd, crossfadeEnd) <= std::sqrt (2.0) * 0.75 + 1.0e-5);
    REQUIRE (maxAdjacentDeltaLeft (out, rampEnd - 32, crossfadeEnd + 32) <= 0.20);
}

TEST_CASE ("FxDelayNode tailSamples formula matches the rendered feedback decay", "[fx-delay][tail]")
{
    const DelaySetting setting { 10.0, 10.0, 0.5, FxDelayNode::kMaxDampingHz, false, 1.0 };
    FxDelayNode node (100);
    setNodeParameters (node, setting);
    node.prepare (kSampleRate, 512);

    const std::int64_t delay = FxDelayNode::delaySamplesForTimeMs (kSampleRate, setting.timeLeftMs);
    const std::int64_t expected = delay * static_cast<std::int64_t> (std::ceil (std::log (0.001) / std::log (setting.feedback)));
    REQUIRE (node.tailSamples() == expected);

    const StereoBuffer out = renderNode ({ 256 }, makeImpulse (static_cast<int> (expected + delay + 16)), setting);
    const int lastAssertedTap = static_cast<int> (expected);
    const int nextTap = static_cast<int> (expected + delay);
    REQUIRE (std::fabs (static_cast<double> (out.left[static_cast<std::size_t> (lastAssertedTap)])) > 0.001);
    REQUIRE (std::fabs (static_cast<double> (out.left[static_cast<std::size_t> (nextTap)])) <= 0.001);
}

TEST_CASE ("FxDelayNode keeps hostile inputs and params finite", "[fx-delay][robust]")
{
    FxDelayNode node (100);
    node.prepare (kSampleRate, 512);

    StereoBuffer input = deterministicStereoInput (512);
    input.left[0] = std::numeric_limits<float>::quiet_NaN();
    input.right[1] = std::numeric_limits<float>::infinity();
    input.left[2] = -std::numeric_limits<float>::infinity();
    input.right[3] = std::numeric_limits<float>::max();
    input.left[4] = -std::numeric_limits<float>::max();

    const std::array<double, 5> hostile {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -1.0,
        2.0,
    };
    std::array<Event, 30> events {};
    std::size_t count = 0;
    for (const double value : hostile)
    {
        events[count++] = makeParameterChangeEvent (0, 100, FxDelayNode::kTimeLeftParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, FxDelayNode::kTimeRightParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, FxDelayNode::kFeedbackParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, FxDelayNode::kDampingParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, FxDelayNode::kPingPongParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, FxDelayNode::kMixParamId, value);
    }

    EventStream stream { std::span<const Event> (events.data(), count) };
    processBlock (node, input, 0, 512, stream);

    for (const float sample : flatten (input))
        REQUIRE (std::isfinite (sample));
}

TEST_CASE ("FxDelayNode output is bit-identical across H14 block schedules", "[fx-delay][blocksize]")
{
    const DelaySetting setting { 9.0, 13.0, 0.45, FxDelayNode::kMaxDampingHz, true, 0.7 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    const std::vector<float> reference = flatten (renderNode ({ 512 }, input, setting));

    for (const int block : { 1, 2, 3, 4, 5, 6, 7, 8, 9, 64, 128, 333, 512 })
    {
        INFO ("block size " << block);
        REQUIRE (flatten (renderNode ({ block }, input, setting)) == reference);
    }

    REQUIRE_FALSE (blockResetNegativeControlPasses());
}

TEST_CASE ("FxDelayNode parameter smoothing is event-offset anchored across schedules", "[fx-delay][automation]")
{
    const DelaySetting setting { 5.0, 5.0, 0.0, FxDelayNode::kMaxDampingHz, false, 0.0 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    const int eventFrame = 777;

    const StereoBuffer reference = renderNode ({ 512 }, input, setting, eventFrame, FxDelayNode::kMixParamId, 1.0);
    REQUIRE (flatten (renderNode ({ 1 }, input, setting, eventFrame, FxDelayNode::kMixParamId, 1.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 7 }, input, setting, eventFrame, FxDelayNode::kMixParamId, 1.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 64 }, input, setting, eventFrame, FxDelayNode::kMixParamId, 1.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 333 }, input, setting, eventFrame, FxDelayNode::kMixParamId, 1.0)) == flatten (reference));

    REQUIRE (maxAbsDiff (renderNode ({ 512 }, input, setting), input) <= kUnityTolerance);
    REQUIRE (maxAbsDiff (reference, input) > 1.0e-3);
    REQUIRE_FALSE (eventAtBlockStartNegativeControlPasses());
}
