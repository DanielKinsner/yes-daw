// YES DAW -- H14 CP8 LimiterNode gate.
//
// Exercises Appendix A5 plus the CP8 PDC clause: ceiling property, transparent delay,
// gain-slope bound, latency/tail reporting, block-size independence, event anchoring,
// and limiter-vs-dry parallel-path PDC alignment.

#include "engine/GraphBuilder.h"
#include "engine/nodes/LimiterNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/SumNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::CompiledNodeKind;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::GraphBuildError;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::LimiterNode;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::MasterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::ParamMapping;
using yesdaw::engine::ParamSmoothing;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::SumNode;
using yesdaw::engine::Transport;
using yesdaw::engine::unmapToNormalized;
using Catch::Approx;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kSampleRate = 48000.0;
constexpr NodeId kNodeId = 100;
constexpr NodeId kSourceId = 200;
constexpr NodeId kLimiterId = 201;
constexpr NodeId kSumId = 202;
constexpr NodeId kMasterId = 203;

struct LimiterSetting
{
    double ceilingDb = LimiterNode::kDefaultCeilingDb;
    double releaseMs = LimiterNode::kDefaultReleaseMs;
    double lookaheadMs = LimiterNode::kDefaultLookaheadMs;
};

struct StereoBuffer
{
    std::vector<float> left;
    std::vector<float> right;
};

[[nodiscard]] std::int64_t lookaheadSamples (const LimiterSetting& setting)
{
    return LimiterNode::lookaheadSamplesForMs (kSampleRate, setting.lookaheadMs);
}

[[nodiscard]] double ceilingLinear (const LimiterSetting& setting)
{
    return LimiterNode::ceilingLinearForDb (setting.ceilingDb);
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

[[nodiscard]] StereoBuffer makeImpulse (int frames, float left = 0.5f, float right = 0.0f)
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

[[nodiscard]] StereoBuffer deterministicInput (int frames)
{
    StereoBuffer buffer;
    buffer.left.resize (static_cast<std::size_t> (frames));
    buffer.right.resize (static_cast<std::size_t> (frames));
    for (int i = 0; i < frames; ++i)
    {
        double sample = 0.24 + 0.06 * std::sin (0.019 * static_cast<double> (i));
        if (i % 257 == 0 || i % 509 == 73)
            sample = 7.5;
        if (i % 997 == 211)
            sample = -9.0;
        if ((i / 173) % 9 == 3)
            sample += 0.35;

        buffer.left[static_cast<std::size_t> (i)] = static_cast<float> (sample);
        buffer.right[static_cast<std::size_t> (i)] = static_cast<float> (-0.7 * sample);
    }
    return buffer;
}

[[nodiscard]] StereoBuffer adversarialProgram (const std::vector<int>& schedule)
{
    int totalFrames = 0;
    for (int block : schedule)
        totalFrames += block;

    StereoBuffer buffer;
    buffer.left.resize (static_cast<std::size_t> (totalFrames));
    buffer.right.resize (static_cast<std::size_t> (totalFrames));

    std::uint32_t state = 0xC8A5EEDu;
    double dc = 0.0;
    for (int i = 0; i < totalFrames; ++i)
    {
        state = state * 1664525u + 1013904223u;
        if ((state & 0x3Fu) == 0)
            dc = (state & 0x80u) != 0 ? 0.75 : -0.75;
        if ((state & 0x1FFu) == 0)
            dc = 0.0;

        double sample = 0.22 + dc + 0.05 * std::sin (0.013 * static_cast<double> (i));
        if ((state & 0x7Fu) == 3u)
            sample = 10.0;
        if ((state & 0xFFu) == 17u)
            sample = -10.0;
        if ((state & 0x3FFu) == 123u)
            sample = 0.0;

        buffer.left[static_cast<std::size_t> (i)] = static_cast<float> (sample);
        buffer.right[static_cast<std::size_t> (i)] = static_cast<float> (-0.6 * sample);
    }

    return buffer;
}

[[nodiscard]] std::vector<int> makeTenThousandBlockSchedule()
{
    std::vector<int> schedule;
    schedule.reserve (10000);
    std::uint32_t state = 0x51A7E11u;
    for (int i = 0; i < 10000; ++i)
    {
        state = state * 1103515245u + 12345u;
        schedule.push_back (1 + static_cast<int> ((state >> 16u) % 31u));
    }
    return schedule;
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

[[nodiscard]] bool allFinite (const StereoBuffer& buffer)
{
    for (std::size_t i = 0; i < buffer.left.size(); ++i)
        if (! std::isfinite (buffer.left[i]) || ! std::isfinite (buffer.right[i]))
            return false;
    return true;
}

void processBlock (LimiterNode& node,
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
                                       const LimiterSetting& setting,
                                       int eventFrame = -1,
                                       yesdaw::engine::ParameterId eventParam = LimiterNode::kCeilingParamId,
                                       double eventRealValue = LimiterNode::kDefaultCeilingDb,
                                       bool wrongEventAtBlockStart = false,
                                       bool resetAtBlockBoundary = false)
{
    int maxBlock = 1;
    for (const int block : schedule)
        maxBlock = std::max (maxBlock, block);

    LimiterNode node (kNodeId);
    node.setParameters (setting.ceilingDb, setting.releaseMs, setting.lookaheadMs);
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
            const double normalized = unmapToNormalized (LimiterNode::parameterSpec (eventParam), eventRealValue);
            const std::uint32_t time = wrongEventAtBlockStart
                ? 0u
                : static_cast<std::uint32_t> (eventFrame - offset);
            eventStorage[0] = makeParameterChangeEvent (time, kNodeId, eventParam, normalized);
            eventSpan = std::span<const Event> (eventStorage.data(), 1u);
        }

        EventStream events { eventSpan };
        processBlock (node, out, offset, frames, events);
        offset += frames;
        ++scheduleIndex;
    }

    return out;
}

[[nodiscard]] bool transparencyGatePassesLocal (bool perturbGain)
{
    const LimiterSetting setting { -1.0, 100.0, 2.0 };
    const int d = static_cast<int> (lookaheadSamples (setting));
    const StereoBuffer input = makeSine (4096, 997.0, std::pow (10.0, -6.0 / 20.0));
    StereoBuffer out = renderNode ({ 257 }, input, setting);
    if (perturbGain)
    {
        for (float& sample : out.left)
            sample *= 0.99f;
        for (float& sample : out.right)
            sample *= 0.99f;
    }

    for (int i = 0; i + d < static_cast<int> (input.left.size()); ++i)
    {
        if (std::fabs (static_cast<double> (out.left[static_cast<std::size_t> (i + d)])
                       - static_cast<double> (input.left[static_cast<std::size_t> (i)])) > 1.0e-6)
            return false;
        if (std::fabs (static_cast<double> (out.right[static_cast<std::size_t> (i + d)])
                       - static_cast<double> (input.right[static_cast<std::size_t> (i)])) > 1.0e-6)
            return false;
    }

    return true;
}

struct ModelResult
{
    std::vector<double> output;
    std::vector<double> gain;
};

[[nodiscard]] ModelResult renderLimiterModel (const std::vector<double>& input,
                                              const LimiterSetting& setting,
                                              int windowDelta,
                                              bool bypassBoxcar)
{
    const int d = static_cast<int> (lookaheadSamples (setting));
    const int window = std::max (1, d + windowDelta);
    const int n = static_cast<int> (input.size());
    const double ceiling = ceilingLinear (setting);
    const double alpha = 1.0 - std::exp (-1.0 / ((setting.releaseMs * 0.001) * kSampleRate));

    std::vector<double> target (static_cast<std::size_t> (n + d + window + 2), 1.0);
    double released = 1.0;
    for (int i = 0; i < n; ++i)
    {
        const double raw = std::min (1.0, ceiling / std::max (std::fabs (input[static_cast<std::size_t> (i)]), 1.0e-10));
        released = std::min (raw, released + alpha * (1.0 - released));
        target[static_cast<std::size_t> (i)] = released;
    }

    std::vector<double> gMin (static_cast<std::size_t> (n), 1.0);
    for (int i = 0; i < n; ++i)
    {
        double m = 1.0;
        for (int w = 0; w < window; ++w)
            m = std::min (m, target[static_cast<std::size_t> (i + w)]);
        gMin[static_cast<std::size_t> (i)] = m;
    }

    ModelResult result;
    result.output.assign (static_cast<std::size_t> (n + d), 0.0);
    result.gain.assign (static_cast<std::size_t> (n), 1.0);

    for (int i = 0; i < n; ++i)
    {
        double gain = gMin[static_cast<std::size_t> (i)];
        if (! bypassBoxcar)
        {
            double sum = 0.0;
            for (int k = 0; k < d; ++k)
            {
                const int idx = i - k;
                sum += idx >= 0 ? gMin[static_cast<std::size_t> (idx)] : gMin[0];
            }
            gain = sum / static_cast<double> (d);
        }

        result.gain[static_cast<std::size_t> (i)] = gain;
        result.output[static_cast<std::size_t> (i + d)] = input[static_cast<std::size_t> (i)] * gain;
    }

    return result;
}

[[nodiscard]] bool ceilingModelPasses (int windowDelta)
{
    const LimiterSetting setting { -6.0, 50.0, 1.0 };
    const int d = static_cast<int> (lookaheadSamples (setting));
    std::vector<double> input (static_cast<std::size_t> (d * 8), 0.25);
    input[static_cast<std::size_t> (4 * d)] = 10.0;

    const ModelResult result = renderLimiterModel (input, setting, windowDelta, false);
    const double ceiling = ceilingLinear (setting);
    for (double sample : result.output)
        if (std::fabs (sample) > ceiling + 1.0e-12)
            return false;
    return true;
}

[[nodiscard]] bool slopeModelPasses (bool bypassBoxcar)
{
    const LimiterSetting setting { -6.0, 50.0, 1.0 };
    const int d = static_cast<int> (lookaheadSamples (setting));
    std::vector<double> input (static_cast<std::size_t> (d * 8), 0.25);
    for (int i = 4 * d; i < 5 * d; ++i)
        input[static_cast<std::size_t> (i)] = 10.0;

    const ModelResult result = renderLimiterModel (input, setting, 0, bypassBoxcar);
    const double bound = 1.0 / static_cast<double> (d);
    for (std::size_t i = 1; i < result.gain.size(); ++i)
        if (std::fabs (result.gain[i] - result.gain[i - 1]) > bound + 1.0e-12)
            return false;
    return true;
}

[[nodiscard]] bool productionGainSlopePasses()
{
    const LimiterSetting setting { -6.0, 50.0, 1.0 };
    const int d = static_cast<int> (lookaheadSamples (setting));
    const StereoBuffer input = deterministicInput (d * 64);
    const StereoBuffer out = renderNode ({ 193 }, input, setting);
    const double bound = 1.0 / static_cast<double> (d);

    bool havePrevious = false;
    double previousGain = 1.0;
    for (int i = d; i < static_cast<int> (out.left.size()); ++i)
    {
        const double delayed = input.left[static_cast<std::size_t> (i - d)];
        if (std::fabs (delayed) < 1.0e-4)
            continue;

        const double gain = static_cast<double> (out.left[static_cast<std::size_t> (i)]) / delayed;
        if (! std::isfinite (gain))
            return false;

        if (havePrevious && std::fabs (gain - previousGain) > bound + 1.0e-6)
            return false;

        previousGain = gain;
        havePrevious = true;
    }

    return havePrevious;
}

class StereoImpulseSourceNode final : public Node
{
public:
    explicit StereoImpulseSourceNode (NodeId id) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { true, false, 2, 0, id_, false };
    }

    std::span<Node* const> directInputs() const noexcept override { return {}; }
    void prepare (double, int) override { runningFrame_ = 0; }
    void reset() noexcept override { runningFrame_ = 0; }
    void release() override {}

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        const std::int64_t blockStart = args.transport.hasTimelineFrame ? args.transport.timelineFrame : runningFrame_;
        for (int channel = 0; channel < args.audio.numChannels; ++channel)
        {
            float* const out = args.audio.channels[channel];
            if (out == nullptr)
                continue;

            for (int frame = 0; frame < args.numFrames; ++frame)
                out[frame] = blockStart + frame == 0 ? 0.5f : 0.0f;
        }

        runningFrame_ = blockStart + args.numFrames;
    }

private:
    NodeId id_;
    std::int64_t runningFrame_ = 0;
};

class LatencyReportingLimiterNode final : public Node
{
public:
    LatencyReportingLimiterNode (NodeId id, bool reportLatency) noexcept
        : limiter_ (id), reportLatency_ (reportLatency)
    {
        limiter_.setParameters (0.0, 100.0, 1.0);
    }

    NodeProperties properties() const noexcept override
    {
        NodeProperties props = limiter_.properties();
        if (! reportLatency_)
            props.latencySamples = 0;
        return props;
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double sampleRate, int maxBlockSize) override { limiter_.prepare (sampleRate, maxBlockSize); }
    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override { limiter_.process (args); }
    void reset() noexcept override { limiter_.reset(); }
    void release() override { limiter_.release(); }

    void setInput (Node* input) noexcept
    {
        input_ = input;
        limiter_.setInput (input);
    }

private:
    LimiterNode limiter_;
    Node* input_ = nullptr;
    bool reportLatency_ = true;
};

[[nodiscard]] std::unique_ptr<CompiledGraph> buildParallelLimiterGraph (bool reportLatency)
{
    auto source = std::make_unique<StereoImpulseSourceNode> (kSourceId);
    auto limiter = std::make_unique<LatencyReportingLimiterNode> (kLimiterId, reportLatency);
    auto sum = std::make_unique<SumNode> (kSumId, 2);
    auto master = std::make_unique<MasterNode> (kMasterId, 2);

    StereoImpulseSourceNode* const sourcePtr = source.get();
    LatencyReportingLimiterNode* const limiterPtr = limiter.get();
    SumNode* const sumPtr = sum.get();
    MasterNode* const masterPtr = master.get();

    limiterPtr->setInput (sourcePtr);
    sumPtr->setInputNodes ({ sourcePtr, limiterPtr });
    masterPtr->setInputNodes ({ sumPtr });

    GraphBuilder::Inputs inputs;
    inputs.id = 500;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = kSampleRate;
    inputs.maxBlockSize = 256;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (limiter));
    inputs.nodes.push_back (std::move (sum));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError error;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &error);
    REQUIRE (error.code() == GraphBuildError::Code::None);
    return graph;
}

[[nodiscard]] StereoBuffer renderGraph (CompiledGraph& graph, int frames)
{
    StereoBuffer out;
    out.left.assign (static_cast<std::size_t> (frames), -999.0f);
    out.right.assign (static_cast<std::size_t> (frames), -999.0f);
    float* channels[2] = { out.left.data(), out.right.data() };
    graph.process (channels, 2, frames);
    return out;
}

} // namespace

TEST_CASE ("LimiterNode ParamSpec table exposes stable CP8 ranges and latency/tail properties", "[limiter][params]")
{
    REQUIRE (LimiterNode::parameterSpec (LimiterNode::kCeilingParamId).mapping == ParamMapping::Db);
    REQUIRE (LimiterNode::parameterSpec (LimiterNode::kCeilingParamId).min == -12.0);
    REQUIRE (LimiterNode::parameterSpec (LimiterNode::kReleaseParamId).mapping == ParamMapping::Log);
    REQUIRE (LimiterNode::parameterSpec (LimiterNode::kReleaseParamId).min == 50.0);
    REQUIRE (LimiterNode::parameterSpec (LimiterNode::kLookaheadParamId).mapping == ParamMapping::Log);
    REQUIRE (LimiterNode::parameterSpec (LimiterNode::kLookaheadParamId).def == 5.0);
    REQUIRE (LimiterNode::parameterSpec (LimiterNode::kLookaheadParamId).smoothing == ParamSmoothing::Linear5Ms);

    LimiterNode node (kNodeId);
    node.setParameters (-3.0, 250.0, 2.0);
    Node& iface = node;
    iface.prepare (kSampleRate, 128);
    const std::int64_t d = LimiterNode::lookaheadSamplesForMs (kSampleRate, 2.0);
    REQUIRE (node.lookaheadSamples() == d);
    REQUIRE (node.tailSamples() == d);
    REQUIRE (node.properties().latencySamples == d);
    REQUIRE (node.properties().tailSamples == d);
    REQUIRE_FALSE (node.properties().blockParallelSafe);
}

TEST_CASE ("LimiterNode transparency is delayed input and the null gate bites", "[limiter][transparent]")
{
    REQUIRE (transparencyGatePassesLocal (false));
    REQUIRE_FALSE (transparencyGatePassesLocal (true));
}

TEST_CASE ("LimiterNode ceiling property holds over a 10000-block adversarial program", "[limiter][ceiling]")
{
    const std::vector<int> schedule = makeTenThousandBlockSchedule();
    const LimiterSetting setting { -6.0, 50.0, 1.0 };
    const StereoBuffer out = renderNode (schedule, adversarialProgram (schedule), setting);
    const double ceiling = ceilingLinear (setting);

    REQUIRE (allFinite (out));
    for (std::size_t i = 0; i < out.left.size(); ++i)
    {
        REQUIRE (std::fabs (static_cast<double> (out.left[i])) <= ceiling);
        REQUIRE (std::fabs (static_cast<double> (out.right[i])) <= ceiling);
    }

    REQUIRE (ceilingModelPasses (0));
    REQUIRE_FALSE (ceilingModelPasses (-1));
}

TEST_CASE ("LimiterNode attack boxcar keeps gain slope bounded and the bypass control fails", "[limiter][slope]")
{
    REQUIRE (productionGainSlopePasses());
    REQUIRE (slopeModelPasses (false));
    REQUIRE_FALSE (slopeModelPasses (true));
}

TEST_CASE ("LimiterNode reported latency equals measured impulse delay", "[limiter][latency]")
{
    const LimiterSetting setting { 0.0, 100.0, 2.0 };
    const int d = static_cast<int> (lookaheadSamples (setting));
    const StereoBuffer out = renderNode ({ 512 }, makeImpulse (d + 64), setting);

    int measured = -1;
    for (int i = 0; i < static_cast<int> (out.left.size()); ++i)
    {
        if (std::fabs (static_cast<double> (out.left[static_cast<std::size_t> (i)])) > 1.0e-6)
        {
            measured = i;
            break;
        }
    }

    LimiterNode node (kNodeId);
    node.setParameters (setting.ceilingDb, setting.releaseMs, setting.lookaheadMs);
    node.prepare (kSampleRate, 512);
    REQUIRE (node.properties().latencySamples == d);
    REQUIRE (node.properties().tailSamples == d);
    REQUIRE (measured == d);
    REQUIRE (out.left[static_cast<std::size_t> (d)] == Approx (0.5f));
}

TEST_CASE ("LimiterNode keeps hostile inputs and params finite", "[limiter][robust]")
{
    LimiterNode node (kNodeId);
    node.setParameters (std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity());
    Node& iface = node;
    iface.prepare (kSampleRate, 16);

    StereoBuffer input;
    input.left = { 0.0f, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::quiet_NaN(), 10.0f, -10.0f, 0.25f, -0.25f,
                   0.1f, 0.2f, -0.3f, 0.4f, 0.0f, 0.0f, 0.0f, 0.0f };
    input.right = input.left;
    StereoBuffer out = renderNode ({ 16 }, input, { LimiterNode::kDefaultCeilingDb,
                                                    LimiterNode::kDefaultReleaseMs,
                                                    LimiterNode::kMinLookaheadMs });
    REQUIRE (allFinite (out));
}

TEST_CASE ("LimiterNode output is bit-identical across H14 block schedules", "[limiter][blocksize]")
{
    const LimiterSetting setting { -6.0, 75.0, 1.0 };
    const StereoBuffer input = deterministicInput (8192);
    const StereoBuffer reference = renderNode ({ 512 }, input, setting);

    REQUIRE (flatten (renderNode ({ 1 }, input, setting)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 7 }, input, setting)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 64 }, input, setting)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 333 }, input, setting)) == flatten (reference));

    REQUIRE (flatten (renderNode ({ 64 }, input, setting, -1, LimiterNode::kCeilingParamId, 0.0, false, true))
             != flatten (reference));
}

TEST_CASE ("LimiterNode parameter smoothing is event-offset anchored across schedules", "[limiter][automation]")
{
    const LimiterSetting setting { 0.0, 100.0, 1.0 };
    const StereoBuffer input = deterministicInput (8192);
    constexpr int eventFrame = 777;

    const StereoBuffer reference = renderNode ({ 512 }, input, setting, eventFrame, LimiterNode::kCeilingParamId, -12.0);
    REQUIRE (flatten (renderNode ({ 1 }, input, setting, eventFrame, LimiterNode::kCeilingParamId, -12.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 7 }, input, setting, eventFrame, LimiterNode::kCeilingParamId, -12.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 64 }, input, setting, eventFrame, LimiterNode::kCeilingParamId, -12.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 333 }, input, setting, eventFrame, LimiterNode::kCeilingParamId, -12.0)) == flatten (reference));

    REQUIRE (flatten (renderNode ({ 64 }, input, setting, eventFrame, LimiterNode::kCeilingParamId, -12.0, true))
             != flatten (reference));
}

TEST_CASE ("LimiterNode latency report PDC-aligns a dry parallel sibling", "[limiter][pdc][graph]")
{
    const int d = static_cast<int> (LimiterNode::lookaheadSamplesForMs (kSampleRate, 1.0));
    std::unique_ptr<CompiledGraph> graph = buildParallelLimiterGraph (true);
    REQUIRE (graph != nullptr);
    REQUIRE (graph->totalLatency() == d);
    REQUIRE (graph->debugCountNodesOfKind (CompiledNodeKind::Latency) == 1u);

    const StereoBuffer out = renderGraph (*graph, d + 32);
    int nonZeroFrames = 0;
    for (int i = 0; i < static_cast<int> (out.left.size()); ++i)
    {
        if (std::fabs (static_cast<double> (out.left[static_cast<std::size_t> (i)])) > 1.0e-6)
            ++nonZeroFrames;
        const double expected = i == d ? 1.0 : 0.0;
        REQUIRE (static_cast<double> (out.left[static_cast<std::size_t> (i)]) == Approx (expected));
    }
    REQUIRE (nonZeroFrames == 1);

    std::unique_ptr<CompiledGraph> broken = buildParallelLimiterGraph (false);
    REQUIRE (broken != nullptr);
    REQUIRE (broken->totalLatency() == 0);
    const StereoBuffer brokenOut = renderGraph (*broken, d + 32);

    int brokenNonZeroFrames = 0;
    REQUIRE (brokenOut.left[0] == Approx (0.5f));
    REQUIRE (brokenOut.left[static_cast<std::size_t> (d)] == Approx (0.5f));
    for (float sample : brokenOut.left)
        if (std::fabs (static_cast<double> (sample)) > 1.0e-6)
            ++brokenNonZeroFrames;
    REQUIRE (brokenNonZeroFrames == 2);
}
