// YES DAW -- H14 CP5 CompressorNode gate.
//
// The checks below exercise the closed-form static curve, one-pole ballistics, unity/null behavior,
// hostile inputs, block-size independence, and absolute-frame parameter smoothing required by Appendix A2.

#include "engine/nodes/CompressorNode.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::CompressorNode;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::Node;
using yesdaw::engine::ParamMapping;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Transport;
using yesdaw::engine::unmapToNormalized;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kSampleRate = 48000.0;
constexpr int kTotalFrames = 4096;
constexpr double kUnityTolerance = 1.0e-6;
constexpr double kStaticToleranceDb = 0.5;

struct CompressorSetting
{
    double thresholdDb = CompressorNode::kDefaultThresholdDb;
    double ratio = CompressorNode::kDefaultRatio;
    double attackMs = CompressorNode::kDefaultAttackMs;
    double releaseMs = CompressorNode::kDefaultReleaseMs;
    double kneeDb = CompressorNode::kDefaultKneeDb;
    double makeupDb = CompressorNode::kDefaultMakeupDb;
};

struct StereoBuffer
{
    std::vector<float> left;
    std::vector<float> right;
};

enum class LocalMutation
{
    None,
    PerturbGainTerm,
    SwapAttackRelease
};

[[nodiscard]] double dbToLinear (double db) noexcept
{
    return std::pow (10.0, db / 20.0);
}

[[nodiscard]] double linearToDb (double linear) noexcept
{
    return 20.0 * std::log10 (std::max (linear, 1.0e-10));
}

[[nodiscard]] double alphaForMs (double tauMs) noexcept
{
    const double tauSeconds = std::max (tauMs, 0.1) * 0.001;
    return 1.0 - std::exp (-1.0 / (tauSeconds * kSampleRate));
}

[[nodiscard]] StereoBuffer makeSine (int frames, double peakDb, double frequencyHz)
{
    StereoBuffer buffer;
    buffer.left.resize (static_cast<std::size_t> (frames));
    buffer.right.resize (static_cast<std::size_t> (frames));
    const double amp = dbToLinear (peakDb);
    for (int i = 0; i < frames; ++i)
    {
        const float sample = static_cast<float> (amp * std::sin (2.0 * kPi * frequencyHz * static_cast<double> (i) / kSampleRate));
        buffer.left[static_cast<std::size_t> (i)] = sample;
        buffer.right[static_cast<std::size_t> (i)] = sample;
    }
    return buffer;
}

[[nodiscard]] StereoBuffer makeConstant (int frames, double levelDb)
{
    StereoBuffer buffer;
    buffer.left.assign (static_cast<std::size_t> (frames), static_cast<float> (dbToLinear (levelDb)));
    buffer.right.assign (static_cast<std::size_t> (frames), static_cast<float> (dbToLinear (levelDb)));
    return buffer;
}

[[nodiscard]] StereoBuffer deterministicStereoInput (int frames)
{
    StereoBuffer buffer;
    buffer.left.resize (static_cast<std::size_t> (frames));
    buffer.right.resize (static_cast<std::size_t> (frames));
    for (int i = 0; i < frames; ++i)
    {
        const double x = 0.22 * std::sin (0.019 * static_cast<double> (i))
                       + 0.09 * std::sin (0.071 * static_cast<double> (i))
                       + (i % 113 == 0 ? 0.18 : 0.0);
        buffer.left[static_cast<std::size_t> (i)] = static_cast<float> (x);
        buffer.right[static_cast<std::size_t> (i)] = static_cast<float> (-0.65 * x);
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

[[nodiscard]] double peakLeftFrom (const StereoBuffer& buffer, int firstFrame)
{
    double peak = 0.0;
    for (std::size_t i = static_cast<std::size_t> (firstFrame); i < buffer.left.size(); ++i)
        peak = std::max (peak, std::fabs (static_cast<double> (buffer.left[i])));
    return peak;
}

[[nodiscard]] double maxAdjacentDeltaLeft (const StereoBuffer& buffer, int firstFrame)
{
    double maxDelta = 0.0;
    for (int i = std::max (1, firstFrame); i < static_cast<int> (buffer.left.size()); ++i)
    {
        const double a = buffer.left[static_cast<std::size_t> (i)];
        const double b = buffer.left[static_cast<std::size_t> (i - 1)];
        maxDelta = std::max (maxDelta, std::fabs (a - b));
    }
    return maxDelta;
}

void setNodeParameters (CompressorNode& node, const CompressorSetting& setting)
{
    node.setParameters (setting.thresholdDb,
                        setting.ratio,
                        setting.attackMs,
                        setting.releaseMs,
                        setting.kneeDb,
                        setting.makeupDb);
}

void processBlock (CompressorNode& node,
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
                                       const CompressorSetting& setting,
                                       int eventFrame = -1,
                                       yesdaw::engine::ParameterId eventParam = CompressorNode::kThresholdParamId,
                                       double eventRealValue = CompressorNode::kDefaultThresholdDb,
                                       bool wrongEventAtBlockStart = false,
                                       bool resetAtBlockBoundary = false)
{
    int maxBlock = 1;
    for (const int block : schedule)
        maxBlock = std::max (maxBlock, block);

    CompressorNode node (100);
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
            const double normalized = unmapToNormalized (CompressorNode::parameterSpec (eventParam), eventRealValue);
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

[[nodiscard]] double closedFormOutputDb (double inputDb, const CompressorSetting& setting)
{
    return inputDb
         - CompressorNode::closedFormGainReductionDb (inputDb, setting.thresholdDb, setting.ratio, setting.kneeDb)
         + setting.makeupDb;
}

[[nodiscard]] bool staticCurveGatePassesFor (const CompressorSetting& setting)
{
    constexpr int frames = 96000;
    constexpr int settleFrame = 72000;
    for (int inputDb = -40; inputDb <= 0; inputDb += 5)
    {
        StereoBuffer input = makeSine (frames, static_cast<double> (inputDb), 1000.0);
        const StereoBuffer out = renderNode ({ 256 }, input, setting);
        const double measuredDb = linearToDb (peakLeftFrom (out, settleFrame));
        const double expectedDb = closedFormOutputDb (static_cast<double> (inputDb), setting);
        if (std::fabs (measuredDb - expectedDb) > kStaticToleranceDb)
            return false;
    }

    return true;
}

[[nodiscard]] StereoBuffer renderLocal (const StereoBuffer& input,
                                        const CompressorSetting& setting,
                                        LocalMutation mutation)
{
    StereoBuffer out = input;
    double envelopeDb = -200.0;

    for (std::size_t i = 0; i < out.left.size(); ++i)
    {
        const double detector = std::max (std::fabs (static_cast<double> (out.left[i])),
                                          std::fabs (static_cast<double> (out.right[i])));
        const double inputDb = linearToDb (detector);
        const bool rising = inputDb > envelopeDb;
        double alpha = rising ? alphaForMs (setting.attackMs) : alphaForMs (setting.releaseMs);
        if (mutation == LocalMutation::SwapAttackRelease)
            alpha = rising ? alphaForMs (setting.releaseMs) : alphaForMs (setting.attackMs);

        envelopeDb += alpha * (inputDb - envelopeDb);
        const double gr = CompressorNode::closedFormGainReductionDb (envelopeDb,
                                                                     setting.thresholdDb,
                                                                     setting.ratio,
                                                                     setting.kneeDb);
        double gain = dbToLinear (setting.makeupDb - gr);
        if (mutation == LocalMutation::PerturbGainTerm)
            gain *= 1.01;

        out.left[i] = static_cast<float> (static_cast<double> (out.left[i]) * gain);
        out.right[i] = static_cast<float> (static_cast<double> (out.right[i]) * gain);
    }

    return out;
}

[[nodiscard]] bool unityGatePassesLocal (LocalMutation mutation)
{
    const CompressorSetting setting { -30.0, 1.0, 0.1, 10.0, 24.0, 0.0 };
    const StereoBuffer input = deterministicStereoInput (2048);
    const StereoBuffer out = renderLocal (input, setting, mutation);
    return maxAbsDiff (input, out) <= kUnityTolerance;
}

[[nodiscard]] double estimatedEnvelopeDb (double input, double output, const CompressorSetting& setting)
{
    const double gain = std::max (std::fabs (output / input), 1.0e-10);
    const double gainReductionDb = -linearToDb (gain) + setting.makeupDb;
    const double factor = 1.0 - 1.0 / setting.ratio;
    return setting.thresholdDb + gainReductionDb / factor;
}

[[nodiscard]] bool ballisticsGatePassesFor (const StereoBuffer& out, const StereoBuffer& input, const CompressorSetting& setting)
{
    const int preFrames = static_cast<int> (0.25 * kSampleRate);
    const int attackTau = static_cast<int> (std::round (setting.attackMs * 0.001 * kSampleRate));
    const int releaseTau = static_cast<int> (std::round (setting.releaseMs * 0.001 * kSampleRate));
    const int releaseStart = static_cast<int> (0.55 * kSampleRate);

    const double attackTarget = -20.0 + (1.0 - std::exp (-1.0)) * 15.0;
    const double releaseTarget = -20.0 + std::exp (-1.0) * 15.0;

    int attackCross = -1;
    for (int i = preFrames; i < releaseStart; ++i)
    {
        const double env = estimatedEnvelopeDb (input.left[static_cast<std::size_t> (i)],
                                                out.left[static_cast<std::size_t> (i)],
                                                setting);
        if (env >= attackTarget)
        {
            attackCross = i - preFrames;
            break;
        }
    }

    int releaseCross = -1;
    for (int i = releaseStart; i < static_cast<int> (out.left.size()); ++i)
    {
        const double env = estimatedEnvelopeDb (input.left[static_cast<std::size_t> (i)],
                                                out.left[static_cast<std::size_t> (i)],
                                                setting);
        if (env <= releaseTarget)
        {
            releaseCross = i - releaseStart;
            break;
        }
    }

    const auto inWindow = [] (int got, int expected) {
        return got >= static_cast<int> (std::floor (0.8 * static_cast<double> (expected)))
            && got <= static_cast<int> (std::ceil (1.2 * static_cast<double> (expected)));
    };

    return inWindow (attackCross, attackTau) && inWindow (releaseCross, releaseTau);
}

[[nodiscard]] StereoBuffer makeBallisticsInput()
{
    const int totalFrames = static_cast<int> (1.20 * kSampleRate);
    const int attackStart = static_cast<int> (0.25 * kSampleRate);
    const int releaseStart = static_cast<int> (0.55 * kSampleRate);
    StereoBuffer input;
    input.left.resize (static_cast<std::size_t> (totalFrames));
    input.right.resize (static_cast<std::size_t> (totalFrames));
    for (int i = 0; i < totalFrames; ++i)
    {
        const double levelDb = (i >= attackStart && i < releaseStart) ? -5.0 : -20.0;
        const float sample = static_cast<float> (dbToLinear (levelDb));
        input.left[static_cast<std::size_t> (i)] = sample;
        input.right[static_cast<std::size_t> (i)] = sample;
    }
    return input;
}

[[nodiscard]] bool blockResetNegativeControlPasses()
{
    const CompressorSetting setting { -30.0, 6.0, 2.0, 70.0, 6.0, 0.0 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    return flatten (renderNode ({ 512 }, input, setting))
        == flatten (renderNode ({ 64 }, input, setting, -1, CompressorNode::kThresholdParamId, 0.0, false, true));
}

[[nodiscard]] bool eventAtBlockStartNegativeControlPasses()
{
    const CompressorSetting setting { 0.0, 4.0, 0.1, 3000.0, 0.0, 0.0 };
    const StereoBuffer input = makeConstant (kTotalFrames, -6.0);
    return flatten (renderNode ({ 512 }, input, setting, 777, CompressorNode::kThresholdParamId, -30.0, true))
        == flatten (renderNode ({ 64 }, input, setting, 777, CompressorNode::kThresholdParamId, -30.0, true));
}

} // namespace

TEST_CASE ("CompressorNode ParamSpec table exposes stable CP5 ranges", "[compressor][params]")
{
    REQUIRE (CompressorNode::parameterSpec (CompressorNode::kThresholdParamId).mapping == ParamMapping::Db);
    REQUIRE (CompressorNode::parameterSpec (CompressorNode::kRatioParamId).min == 1.0);
    REQUIRE (CompressorNode::parameterSpec (CompressorNode::kRatioParamId).max == 20.0);
    REQUIRE (CompressorNode::parameterSpec (CompressorNode::kAttackParamId).mapping == ParamMapping::Log);
    REQUIRE (CompressorNode::parameterSpec (CompressorNode::kReleaseParamId).mapping == ParamMapping::Log);
    REQUIRE (CompressorNode::parameterSpec (CompressorNode::kKneeParamId).max == 24.0);
    REQUIRE (CompressorNode::parameterSpec (CompressorNode::kMakeupParamId).mapping == ParamMapping::Db);

    CompressorNode node (100);
    REQUIRE_FALSE (node.properties().blockParallelSafe);
    REQUIRE (node.properties().channels == 2);
}

TEST_CASE ("CompressorNode static curve matches the closed form", "[compressor][curve]")
{
    const std::array<CompressorSetting, 3> settings {
        CompressorSetting { -18.0, 2.0, 0.1, 3000.0, 0.0, 0.0 },
        CompressorSetting { -24.0, 4.0, 0.1, 3000.0, 6.0, 0.0 },
        CompressorSetting { -12.0, 10.0, 0.1, 3000.0, 12.0, 0.0 },
    };

    for (const CompressorSetting& setting : settings)
    {
        INFO ("threshold " << setting.thresholdDb << " ratio " << setting.ratio << " knee " << setting.kneeDb);
        REQUIRE (staticCurveGatePassesFor (setting));
    }
}

TEST_CASE ("CompressorNode ballistics follow attack and release tau", "[compressor][ballistics]")
{
    const CompressorSetting setting { -60.0, 4.0, 10.0, 100.0, 0.0, 0.0 };
    const StereoBuffer input = makeBallisticsInput();
    const StereoBuffer out = renderNode ({ 128 }, input, setting);

    REQUIRE (ballisticsGatePassesFor (out, input, setting));
    REQUIRE_FALSE (ballisticsGatePassesFor (renderLocal (input, setting, LocalMutation::SwapAttackRelease),
                                            input,
                                            setting));
}

TEST_CASE ("CompressorNode ratio one is unity and the null gate bites", "[compressor][unity]")
{
    const CompressorSetting setting { -30.0, 1.0, 0.1, 10.0, 24.0, 0.0 };
    const StereoBuffer input = deterministicStereoInput (2048);
    const StereoBuffer out = renderNode ({ 257 }, input, setting);
    REQUIRE (maxAbsDiff (input, out) <= kUnityTolerance);

    REQUIRE (unityGatePassesLocal (LocalMutation::None));
    REQUIRE_FALSE (unityGatePassesLocal (LocalMutation::PerturbGainTerm));
}

TEST_CASE ("CompressorNode keeps hostile inputs and params finite", "[compressor][robust]")
{
    CompressorNode node (100);
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
        events[count++] = makeParameterChangeEvent (0, 100, CompressorNode::kThresholdParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, CompressorNode::kRatioParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, CompressorNode::kAttackParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, CompressorNode::kReleaseParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, CompressorNode::kKneeParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, CompressorNode::kMakeupParamId, value);
    }

    EventStream stream { std::span<const Event> (events.data(), count) };
    processBlock (node, input, 0, 512, stream);

    for (const float sample : flatten (input))
        REQUIRE (std::isfinite (sample));
    REQUIRE (std::isfinite (node.gainReductionDb()));
}

TEST_CASE ("CompressorNode output is bit-identical across H14 block schedules", "[compressor][blocksize]")
{
    const CompressorSetting setting { -30.0, 6.0, 2.0, 70.0, 6.0, 0.0 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    const std::vector<float> reference = flatten (renderNode ({ 512 }, input, setting));

    for (const int block : { 1, 2, 3, 4, 5, 6, 7, 8, 9, 64, 128, 333, 512 })
    {
        INFO ("block size " << block);
        REQUIRE (flatten (renderNode ({ block }, input, setting)) == reference);
    }

    REQUIRE_FALSE (blockResetNegativeControlPasses());
}

TEST_CASE ("CompressorNode parameter smoothing is event-offset anchored across schedules", "[compressor][automation]")
{
    const CompressorSetting setting { 0.0, 4.0, 0.1, 3000.0, 0.0, 0.0 };
    const StereoBuffer input = makeConstant (kTotalFrames, -6.0);
    const int eventFrame = 777;

    const StereoBuffer reference = renderNode ({ 512 }, input, setting, eventFrame, CompressorNode::kThresholdParamId, -30.0);
    REQUIRE (flatten (renderNode ({ 1 }, input, setting, eventFrame, CompressorNode::kThresholdParamId, -30.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 7 }, input, setting, eventFrame, CompressorNode::kThresholdParamId, -30.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 64 }, input, setting, eventFrame, CompressorNode::kThresholdParamId, -30.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 333 }, input, setting, eventFrame, CompressorNode::kThresholdParamId, -30.0)) == flatten (reference));

    for (int frame = 0; frame < eventFrame; ++frame)
        REQUIRE (reference.left[static_cast<std::size_t> (frame)] == input.left[static_cast<std::size_t> (frame)]);

    bool changedAfterEvent = false;
    for (int frame = eventFrame + 64; frame < eventFrame + 512; ++frame)
        changedAfterEvent = changedAfterEvent || reference.left[static_cast<std::size_t> (frame)] != input.left[static_cast<std::size_t> (frame)];
    REQUIRE (changedAfterEvent);

    INFO ("max adjacent delta after event = " << maxAdjacentDeltaLeft (reference, eventFrame));
    REQUIRE (maxAdjacentDeltaLeft (reference, eventFrame) <= 0.01);

    REQUIRE_FALSE (eventAtBlockStartNegativeControlPasses());
}
