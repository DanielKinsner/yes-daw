// YES DAW -- H15 CP2 FX automation consumer gates.
//
// These tests prove the H14 built-in FX nodes consume the H15 ProcessArgs automation side-band.
// Compiled lane evaluation/emission is CP3; this CP2 gate injects the side-band mechanically.

#include "engine/nodes/CompressorNode.h"
#include "engine/nodes/EqNode.h"
#include "engine/nodes/FxDelayNode.h"
#include "engine/nodes/LimiterNode.h"
#include "engine/nodes/ReverbNode.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::CompressorNode;
using yesdaw::engine::EqNode;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::FxDelayNode;
using yesdaw::engine::LimiterNode;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::ParameterId;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::ReverbNode;
using yesdaw::engine::Transport;
using yesdaw::engine::unmapToNormalized;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr NodeId kFxNodeId = 100;
constexpr NodeId kWrongNodeId = 999;
constexpr double kPi = 3.141592653589793238462643383279502884;

struct StereoBuffer
{
    std::vector<float> left;
    std::vector<float> right;
};

StereoBuffer constantStereo (int frames, float value)
{
    return { std::vector<float> (static_cast<std::size_t> (frames), value),
             std::vector<float> (static_cast<std::size_t> (frames), value) };
}

StereoBuffer impulseStereo (int frames)
{
    StereoBuffer buffer = constantStereo (frames, 0.0f);
    buffer.left[0] = 1.0f;
    buffer.right[0] = 1.0f;
    return buffer;
}

StereoBuffer sineStereo (int frames, double frequencyHz, double amplitude)
{
    StereoBuffer buffer = constantStereo (frames, 0.0f);
    for (int frame = 0; frame < frames; ++frame)
    {
        const double phase = 2.0 * kPi * frequencyHz * static_cast<double> (frame) / kSampleRate;
        const float sample = static_cast<float> (amplitude * std::sin (phase));
        buffer.left[static_cast<std::size_t> (frame)] = sample;
        buffer.right[static_cast<std::size_t> (frame)] = sample;
    }
    return buffer;
}

void processOnce (Node& node,
                  StereoBuffer& buffer,
                  EventStream& regularEvents,
                  EventStream* automationEvents)
{
    float* channels[2] = { buffer.left.data(), buffer.right.data() };
    Transport transport;
    node.process (ProcessArgs { AudioBlock { channels, 2 },
                                regularEvents,
                                transport,
                                static_cast<int> (buffer.left.size()),
                                automationEvents });
}

double maxAbsDiff (const StereoBuffer& a, const StereoBuffer& b)
{
    double diff = 0.0;
    for (std::size_t i = 0; i < a.left.size(); ++i)
    {
        diff = std::max (diff, std::fabs (static_cast<double> (a.left[i] - b.left[i])));
        diff = std::max (diff, std::fabs (static_cast<double> (a.right[i] - b.right[i])));
    }
    return diff;
}

double maxAbsFrom (const StereoBuffer& buffer, int firstFrame)
{
    double peak = 0.0;
    for (std::size_t i = static_cast<std::size_t> (firstFrame); i < buffer.left.size(); ++i)
    {
        peak = std::max (peak, std::fabs (static_cast<double> (buffer.left[i])));
        peak = std::max (peak, std::fabs (static_cast<double> (buffer.right[i])));
    }
    return peak;
}

double maxAbsInRange (const StereoBuffer& buffer, int firstFrame, int lastFrame)
{
    double peak = 0.0;
    for (int frame = firstFrame; frame < lastFrame; ++frame)
    {
        const std::size_t i = static_cast<std::size_t> (frame);
        peak = std::max (peak, std::fabs (static_cast<double> (buffer.left[i])));
        peak = std::max (peak, std::fabs (static_cast<double> (buffer.right[i])));
    }
    return peak;
}

template <typename NodeType>
StereoBuffer renderWithOptionalSideBand (NodeType& node,
                                         StereoBuffer input,
                                         ParameterId parameterId,
                                         double normalizedValue,
                                         bool includeSideBand)
{
    Node& iface = node;
    iface.prepare (kSampleRate, static_cast<int> (input.left.size()));

    const Event regular[1] = {
        makeParameterChangeEvent (0, kWrongNodeId, parameterId, normalizedValue),
    };
    const Event automation[1] = {
        makeParameterChangeEvent (0, kFxNodeId, parameterId, normalizedValue),
    };
    EventStream regularStream (std::span<const Event> (regular, 1));
    EventStream automationStream (std::span<const Event> (automation, 1));

    processOnce (iface, input, regularStream, includeSideBand ? &automationStream : nullptr);
    return input;
}

} // namespace

TEST_CASE ("EqNode consumes H15 automation side-band events", "[h15][cp2][fx-automation][eq]")
{
    const ParameterId gainParam = EqNode::parameterIdFor (0, EqNode::kGainParamOffset);
    const double normalizedGain = unmapToNormalized (EqNode::parameterSpec (gainParam), 12.0);
    const StereoBuffer input = sineStereo (2048, 1000.0, 0.20);

    EqNode baseline (kFxNodeId);
    EqNode automated (kFxNodeId);
    const StereoBuffer withoutSideBand = renderWithOptionalSideBand (baseline, input, gainParam, normalizedGain, false);
    const StereoBuffer withSideBand = renderWithOptionalSideBand (automated, input, gainParam, normalizedGain, true);

    REQUIRE (maxAbsDiff (withoutSideBand, input) <= 1.0e-7);
    REQUIRE (maxAbsDiff (withSideBand, withoutSideBand) > 1.0e-3);
}

TEST_CASE ("CompressorNode consumes H15 automation side-band events", "[h15][cp2][fx-automation][compressor]")
{
    const double normalizedThreshold =
        unmapToNormalized (CompressorNode::parameterSpec (CompressorNode::kThresholdParamId), -60.0);
    const StereoBuffer input = constantStereo (2048, 0.50f);

    CompressorNode baseline (kFxNodeId);
    baseline.setParameters (0.0, 4.0, 0.1, 10.0, 0.0, 0.0);
    CompressorNode automated (kFxNodeId);
    automated.setParameters (0.0, 4.0, 0.1, 10.0, 0.0, 0.0);

    const StereoBuffer withoutSideBand =
        renderWithOptionalSideBand (baseline, input, CompressorNode::kThresholdParamId, normalizedThreshold, false);
    const StereoBuffer withSideBand =
        renderWithOptionalSideBand (automated, input, CompressorNode::kThresholdParamId, normalizedThreshold, true);

    REQUIRE (maxAbsDiff (withoutSideBand, input) <= 1.0e-7);
    REQUIRE (maxAbsInRange (withSideBand, 512, 2048) < 0.20);
}

TEST_CASE ("FxDelayNode consumes H15 automation side-band events", "[h15][cp2][fx-automation][delay]")
{
    const double normalizedMix = unmapToNormalized (FxDelayNode::parameterSpec (FxDelayNode::kMixParamId), 1.0);
    const StereoBuffer input = impulseStereo (2048);

    FxDelayNode baseline (kFxNodeId);
    baseline.setParameters (1.0, 1.0, 0.0, FxDelayNode::kMaxDampingHz, false, 0.0);
    FxDelayNode automated (kFxNodeId);
    automated.setParameters (1.0, 1.0, 0.0, FxDelayNode::kMaxDampingHz, false, 0.0);

    const StereoBuffer withoutSideBand =
        renderWithOptionalSideBand (baseline, input, FxDelayNode::kMixParamId, normalizedMix, false);
    const StereoBuffer withSideBand =
        renderWithOptionalSideBand (automated, input, FxDelayNode::kMixParamId, normalizedMix, true);

    const int delayFrame = static_cast<int> (FxDelayNode::delaySamplesForTimeMs (kSampleRate, 1.0));
    REQUIRE (maxAbsFrom (withoutSideBand, 1) <= 1.0e-7);
    REQUIRE (maxAbsInRange (withSideBand, delayFrame, delayFrame + 4) > 0.10);
}

TEST_CASE ("ReverbNode consumes H15 automation side-band events", "[h15][cp2][fx-automation][reverb]")
{
    const double normalizedMix = unmapToNormalized (ReverbNode::parameterSpec (ReverbNode::kMixParamId), 1.0);
    const StereoBuffer input = impulseStereo (4096);

    ReverbNode baseline (kFxNodeId);
    baseline.setParameters (0.0, 1.0, 1.0, ReverbNode::kMaxDampingHz, 0.0);
    ReverbNode automated (kFxNodeId);
    automated.setParameters (0.0, 1.0, 1.0, ReverbNode::kMaxDampingHz, 0.0);

    const StereoBuffer withoutSideBand =
        renderWithOptionalSideBand (baseline, input, ReverbNode::kMixParamId, normalizedMix, false);
    const StereoBuffer withSideBand =
        renderWithOptionalSideBand (automated, input, ReverbNode::kMixParamId, normalizedMix, true);

    REQUIRE (maxAbsFrom (withoutSideBand, 1) <= 1.0e-7);
    REQUIRE (maxAbsFrom (withSideBand, 512) > 1.0e-5);
}

TEST_CASE ("LimiterNode consumes H15 automation side-band events", "[h15][cp2][fx-automation][limiter]")
{
    const double normalizedCeiling =
        unmapToNormalized (LimiterNode::parameterSpec (LimiterNode::kCeilingParamId), -12.0);
    const StereoBuffer input = constantStereo (4096, 0.90f);

    LimiterNode baseline (kFxNodeId);
    baseline.setParameters (0.0, 50.0, 1.0);
    LimiterNode automated (kFxNodeId);
    automated.setParameters (0.0, 50.0, 1.0);

    const StereoBuffer withoutSideBand =
        renderWithOptionalSideBand (baseline, input, LimiterNode::kCeilingParamId, normalizedCeiling, false);
    const StereoBuffer withSideBand =
        renderWithOptionalSideBand (automated, input, LimiterNode::kCeilingParamId, normalizedCeiling, true);

    REQUIRE (maxAbsInRange (withoutSideBand, 512, 4096) > 0.89);
    REQUIRE (maxAbsInRange (withSideBand, 1024, 4096) < 0.55);
}
