// YES DAW -- H14 CP7 ReverbNode gate.
//
// Exercises Appendix A4 plus the additive tailSamples contract: RT60, stability, decorrelation,
// damping tilt, hostile inputs, block-size independence, absolute-frame parameter smoothing, and a
// compiled-graph tail render gate with the required zero-tail negative control.

#include "engine/GraphBuilder.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/ReverbNode.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <span>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::CompiledGraph;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::GraphBuilder;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::MasterNode;
using yesdaw::engine::Node;
using yesdaw::engine::NodeId;
using yesdaw::engine::NodeProperties;
using yesdaw::engine::ParamMapping;
using yesdaw::engine::ParamSmoothing;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::ReverbNode;
using yesdaw::engine::Transport;
using yesdaw::engine::unmapToNormalized;

namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kSampleRate = 48000.0;
constexpr int kTotalFrames = 8192;
constexpr double kUnityTolerance = 1.0e-6;
constexpr NodeId kSourceId = 70001;
constexpr NodeId kReverbId = 70002;
constexpr NodeId kMasterId = 70003;

struct ReverbSetting
{
    double preDelayMs = ReverbNode::kDefaultPreDelayMs;
    double rt60Seconds = ReverbNode::kDefaultRt60Seconds;
    double size = ReverbNode::kDefaultSize;
    double dampingHz = ReverbNode::kDefaultDampingHz;
    double mix = ReverbNode::kDefaultMix;
};

struct StereoBuffer
{
    std::vector<float> left;
    std::vector<float> right;
};

[[nodiscard]] StereoBuffer makeImpulse (int frames, float left = 1.0f, float right = 1.0f)
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

[[nodiscard]] StereoBuffer makeNoiseBurst (int frames, int burstFrames, double amplitude = 0.25)
{
    StereoBuffer buffer;
    buffer.left.assign (static_cast<std::size_t> (frames), 0.0f);
    buffer.right.assign (static_cast<std::size_t> (frames), 0.0f);
    std::uint32_t state = 0x12345678u;
    for (int i = 0; i < std::min (frames, burstFrames); ++i)
    {
        state = state * 1664525u + 1013904223u;
        const double l = ((state >> 8u) * (1.0 / 16777215.0)) * 2.0 - 1.0;
        state = state * 1664525u + 1013904223u;
        const double r = ((state >> 8u) * (1.0 / 16777215.0)) * 2.0 - 1.0;
        buffer.left[static_cast<std::size_t> (i)] = static_cast<float> (amplitude * l);
        buffer.right[static_cast<std::size_t> (i)] = static_cast<float> (amplitude * r);
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
        const double x = 0.18 * std::sin (0.017 * static_cast<double> (i))
                       + 0.09 * std::sin (0.071 * static_cast<double> (i))
                       + (i % 113 == 0 ? 0.12 : 0.0);
        buffer.left[static_cast<std::size_t> (i)] = static_cast<float> (x);
        buffer.right[static_cast<std::size_t> (i)] = static_cast<float> (-0.55 * x);
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
    const int begin = std::max (0, firstFrame);
    const int end = std::min (lastFrame, static_cast<int> (buffer.left.size()));
    for (int i = begin; i < end; ++i)
    {
        peak = std::max (peak, std::fabs (static_cast<double> (buffer.left[static_cast<std::size_t> (i)])));
        peak = std::max (peak, std::fabs (static_cast<double> (buffer.right[static_cast<std::size_t> (i)])));
    }
    return peak;
}

[[nodiscard]] bool allFinite (const StereoBuffer& buffer)
{
    for (const float sample : flatten (buffer))
        if (! std::isfinite (sample))
            return false;
    return true;
}

void setNodeParameters (ReverbNode& node, const ReverbSetting& setting)
{
    node.setParameters (setting.preDelayMs,
                        setting.rt60Seconds,
                        setting.size,
                        setting.dampingHz,
                        setting.mix);
}

void processBlock (ReverbNode& node,
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
                                       const ReverbSetting& setting,
                                       int eventFrame = -1,
                                       yesdaw::engine::ParameterId eventParam = ReverbNode::kMixParamId,
                                       double eventRealValue = ReverbNode::kDefaultMix,
                                       bool wrongEventAtBlockStart = false,
                                       bool resetAtBlockBoundary = false,
                                       double sampleRate = kSampleRate)
{
    int maxBlock = 1;
    for (const int block : schedule)
        maxBlock = std::max (maxBlock, block);

    ReverbNode node (100);
    setNodeParameters (node, setting);
    Node& iface = node;
    iface.prepare (sampleRate, maxBlock);

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
            const double normalized = unmapToNormalized (ReverbNode::parameterSpec (eventParam), eventRealValue);
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

[[nodiscard]] bool blockResetNegativeControlPasses()
{
    const ReverbSetting setting { 3.0, 1.4, 1.1, 6000.0, 0.65 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    return flatten (renderNode ({ 512 }, input, setting))
        == flatten (renderNode ({ 64 }, input, setting, -1, ReverbNode::kMixParamId, 0.0, false, true));
}

[[nodiscard]] bool eventAtBlockStartNegativeControlPasses()
{
    const ReverbSetting setting { 0.0, 1.0, 1.0, ReverbNode::kMaxDampingHz, 0.0 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    return flatten (renderNode ({ 512 }, input, setting, 777, ReverbNode::kMixParamId, 1.0, true))
        == flatten (renderNode ({ 64 }, input, setting, 777, ReverbNode::kMixParamId, 1.0, true));
}

[[nodiscard]] double estimateRt60Seconds (const StereoBuffer& response)
{
    const int frames = static_cast<int> (response.left.size());
    std::vector<double> cumulative (static_cast<std::size_t> (frames), 0.0);
    double running = 0.0;
    for (int i = frames - 1; i >= 0; --i)
    {
        const double l = response.left[static_cast<std::size_t> (i)];
        const double r = response.right[static_cast<std::size_t> (i)];
        running += l * l + r * r;
        cumulative[static_cast<std::size_t> (i)] = running;
    }

    const double reference = cumulative.front();
    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    int count = 0;
    for (int i = 0; i < frames; ++i)
    {
        if (cumulative[static_cast<std::size_t> (i)] <= 0.0 || reference <= 0.0)
            continue;

        const double db = 10.0 * std::log10 (cumulative[static_cast<std::size_t> (i)] / reference);
        if (db > -5.0 || db < -35.0)
            continue;

        const double t = static_cast<double> (i) / kSampleRate;
        sumX += t;
        sumY += db;
        sumXX += t * t;
        sumXY += t * db;
        ++count;
    }

    if (count < 2)
        return 0.0;

    const double n = static_cast<double> (count);
    const double denom = n * sumXX - sumX * sumX;
    if (std::fabs (denom) <= 1.0e-12)
        return 0.0;

    const double slope = (n * sumXY - sumX * sumY) / denom;
    return slope < 0.0 ? -60.0 / slope : 0.0;
}

[[nodiscard]] double normalizedCrossCorrelationPeak (const StereoBuffer& buffer, int firstFrame, int frames, int maxLag)
{
    double peak = 0.0;
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        double xy = 0.0;
        double xx = 0.0;
        double yy = 0.0;
        for (int i = 0; i < frames; ++i)
        {
            const int lIndex = firstFrame + i;
            const int rIndex = lIndex + lag;
            if (lIndex < 0 || rIndex < 0
                || lIndex >= static_cast<int> (buffer.left.size())
                || rIndex >= static_cast<int> (buffer.right.size()))
                continue;

            const double l = buffer.left[static_cast<std::size_t> (lIndex)];
            const double r = buffer.right[static_cast<std::size_t> (rIndex)];
            xy += l * r;
            xx += l * l;
            yy += r * r;
        }

        if (xx > 0.0 && yy > 0.0)
            peak = std::max (peak, std::fabs (xy / std::sqrt (xx * yy)));
    }

    return peak;
}

[[nodiscard]] double monoSumRms (const StereoBuffer& buffer, int firstFrame, int frames)
{
    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < frames; ++i)
    {
        const int frame = firstFrame + i;
        if (frame < 0 || frame >= static_cast<int> (buffer.left.size()))
            continue;

        const double mono = static_cast<double> (buffer.left[static_cast<std::size_t> (frame)])
                          + static_cast<double> (buffer.right[static_cast<std::size_t> (frame)]);
        sum += mono * mono;
        ++count;
    }

    return count > 0 ? std::sqrt (sum / static_cast<double> (count)) : 0.0;
}

[[nodiscard]] double projectionMagnitude (const std::vector<float>& samples,
                                          int firstFrame,
                                          int frames,
                                          double frequencyHz)
{
    double real = 0.0;
    double imag = 0.0;
    int count = 0;
    for (int i = 0; i < frames; ++i)
    {
        const int frame = firstFrame + i;
        if (frame < 0 || frame >= static_cast<int> (samples.size()))
            continue;

        const double phase = 2.0 * kPi * frequencyHz * static_cast<double> (i) / kSampleRate;
        const double sample = samples[static_cast<std::size_t> (frame)];
        real += sample * std::cos (phase);
        imag -= sample * std::sin (phase);
        ++count;
    }

    return count > 0 ? 2.0 * std::sqrt (real * real + imag * imag) / static_cast<double> (count) : 0.0;
}

[[nodiscard]] double highLowTiltRatio (const StereoBuffer& buffer, int firstFrame, int frames)
{
    std::vector<float> mono (buffer.left.size(), 0.0f);
    for (std::size_t i = 0; i < mono.size(); ++i)
        mono[i] = 0.5f * (buffer.left[i] + buffer.right[i]);

    const double low = projectionMagnitude (mono, firstFrame, frames, 600.0);
    const double high = projectionMagnitude (mono, firstFrame, frames, 8000.0);
    return high / std::max (low, 1.0e-12);
}

[[nodiscard]] bool stabilityGatePasses (bool oneLineGainGreaterThanOneControl)
{
    constexpr int noiseFrames = static_cast<int> (30.0 * kSampleRate);
    constexpr int totalFrames = noiseFrames + static_cast<int> (2.0 * kSampleRate);
    const ReverbSetting setting { 0.0, 1.0, 1.0, ReverbNode::kMaxDampingHz, 1.0 };
    StereoBuffer out = renderNode ({ 512 }, makeNoiseBurst (totalFrames, noiseFrames, 0.05), setting);

    if (oneLineGainGreaterThanOneControl)
    {
        for (int i = noiseFrames; i < totalFrames; ++i)
        {
            const double growth = 0.0012 * std::pow (1.00025, static_cast<double> (i - noiseFrames));
            out.left[static_cast<std::size_t> (i)] += static_cast<float> (growth);
        }
    }

    const int assertFrom = noiseFrames + static_cast<int> (1.5 * setting.rt60Seconds * kSampleRate);
    return allFinite (out) && maxAbsSample (out, assertFrom, totalFrames) <= 0.001;
}

class WindowedStereoSourceNode final : public Node
{
public:
    explicit WindowedStereoSourceNode (NodeId id, std::int64_t clipFrames) noexcept
        : id_ (id), clipFrames_ (clipFrames)
    {
    }

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
            {
                const std::int64_t absolute = blockStart + frame;
                if (absolute >= 0 && absolute < clipFrames_)
                {
                    const double sample = (absolute == 0 ? 1.0 : 0.15 * std::sin (0.03 * static_cast<double> (absolute)));
                    out[frame] = static_cast<float> (channel == 0 ? sample : 0.7 * sample);
                }
                else
                {
                    out[frame] = 0.0f;
                }
            }
        }

        runningFrame_ = blockStart + args.numFrames;
    }

private:
    NodeId id_;
    std::int64_t clipFrames_ = 0;
    std::int64_t runningFrame_ = 0;
};

class TailReportingReverbNode final : public Node
{
public:
    TailReportingReverbNode (NodeId id, bool reportTail) noexcept
        : reverb_ (id), reportTail_ (reportTail)
    {
        reverb_.setParameters (0.0, 0.5, 1.0, ReverbNode::kMaxDampingHz, 1.0);
    }

    NodeProperties properties() const noexcept override
    {
        NodeProperties props = reverb_.properties();
        if (! reportTail_)
            props.tailSamples = 0;
        return props;
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double sampleRate, int maxBlockSize) override { reverb_.prepare (sampleRate, maxBlockSize); }
    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override { reverb_.process (args); }
    void reset() noexcept override { reverb_.reset(); }
    void release() override { reverb_.release(); }

    void setInput (Node* input) noexcept
    {
        input_ = input;
        reverb_.setInput (input);
    }

private:
    ReverbNode reverb_;
    Node* input_ = nullptr;
    bool reportTail_ = true;
};

[[nodiscard]] bool compiledGraphTailGatePasses (bool reportTail)
{
    constexpr int clipFrames = 512;
    auto source = std::make_unique<WindowedStereoSourceNode> (kSourceId, clipFrames);
    auto reverb = std::make_unique<TailReportingReverbNode> (kReverbId, reportTail);
    auto master = std::make_unique<MasterNode> (kMasterId, 2);

    WindowedStereoSourceNode* const sourcePtr = source.get();
    TailReportingReverbNode* const reverbPtr = reverb.get();
    reverbPtr->setInput (sourcePtr);
    master->setInputNodes ({ reverbPtr });

    GraphBuilder::Inputs inputs;
    inputs.id = 98;
    inputs.masterNodeId = kMasterId;
    inputs.sampleRate = kSampleRate;
    inputs.maxBlockSize = 256;
    inputs.nodes.push_back (std::move (source));
    inputs.nodes.push_back (std::move (reverb));
    inputs.nodes.push_back (std::move (master));

    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs));
    if (graph == nullptr)
        return false;

    std::uint64_t tail = 0;
    if (! graph->totalTailSamples (tail) || tail == 0)
        return false;

    const int totalFrames = clipFrames + static_cast<int> (tail);
    StereoBuffer out;
    out.left.assign (static_cast<std::size_t> (totalFrames), 0.0f);
    out.right.assign (static_cast<std::size_t> (totalFrames), 0.0f);
    float* channels[2] = { out.left.data(), out.right.data() };

    int offset = 0;
    while (offset < totalFrames)
    {
        const int frames = std::min (256, totalFrames - offset);
        float* blockChannels[2] = { channels[0] + offset, channels[1] + offset };
        graph->process (blockChannels, 2, frames);
        offset += frames;
    }

    const double postClipPeak = maxAbsSample (out, clipFrames, std::min (totalFrames, clipFrames + static_cast<int> (tail / 2u)));
    const double finalPeak = maxAbsSample (out, std::max (clipFrames, totalFrames - 2048), totalFrames);
    return postClipPeak > 1.0e-4 && finalPeak <= 0.001;
}

} // namespace

TEST_CASE ("ReverbNode ParamSpec table exposes stable CP7 ranges and tail properties", "[reverb][params]")
{
    REQUIRE (ReverbNode::parameterSpec (ReverbNode::kPreDelayParamId).max == 200.0);
    REQUIRE (ReverbNode::parameterSpec (ReverbNode::kRt60ParamId).mapping == ParamMapping::Log);
    REQUIRE (ReverbNode::parameterSpec (ReverbNode::kSizeParamId).min == 0.5);
    REQUIRE (ReverbNode::parameterSpec (ReverbNode::kDampingParamId).mapping == ParamMapping::Log);
    REQUIRE (ReverbNode::parameterSpec (ReverbNode::kMixParamId).smoothing == ParamSmoothing::Linear5Ms);

    ReverbNode node (100);
    node.setParameters (25.0, 1.25, 1.5, 7000.0, 0.4);
    node.prepare (kSampleRate, 512);
    REQUIRE_FALSE (node.properties().blockParallelSafe);
    REQUIRE (node.properties().latencySamples == 0);
    REQUIRE (node.properties().channels == 2);

    const std::int64_t expected = static_cast<std::int64_t> (std::ceil (1.25 * kSampleRate))
        + ReverbNode::preDelaySamplesForMs (kSampleRate, 25.0)
        + ReverbNode::lineLengthSamplesFor (7, 1.5, kSampleRate);
    REQUIRE (node.tailSamples() == expected);
    REQUIRE (node.properties().tailSamples == expected);
}

TEST_CASE ("ReverbNode mix zero is dry and the null gate bites", "[reverb][null]")
{
    const ReverbSetting setting { 12.0, 2.0, 1.3, 3000.0, 0.0 };
    const StereoBuffer input = deterministicStereoInput (2048);
    const StereoBuffer out = renderNode ({ 257 }, input, setting);
    REQUIRE (maxAbsDiff (input, out) <= kUnityTolerance);

    REQUIRE (neutralGatePassesLocal (false));
    REQUIRE_FALSE (neutralGatePassesLocal (true));
}

TEST_CASE ("ReverbNode RT60 follows the Schroeder backward-integration estimate", "[reverb][rt60]")
{
    for (const double rt60 : { 1.0, 3.0 })
    {
        const int frames = static_cast<int> (std::ceil ((rt60 * 3.0 + 1.0) * kSampleRate));
        const ReverbSetting setting { 0.0, rt60, 1.0, ReverbNode::kMaxDampingHz, 1.0 };
        const StereoBuffer out = renderNode ({ 512 }, makeImpulse (frames), setting);
        const double estimate = estimateRt60Seconds (out);
        INFO ("target " << rt60 << " estimate " << estimate);
        REQUIRE (estimate == Catch::Approx (rt60).epsilon (0.20));
    }
}

TEST_CASE ("ReverbNode stability gate decays below -60 dBFS and catches growing feedback", "[reverb][stability]")
{
    REQUIRE (stabilityGatePasses (false));
    REQUIRE_FALSE (stabilityGatePasses (true));
}

TEST_CASE ("ReverbNode decorrelates the tail and keeps the mono sum non-null", "[reverb][decorrelation]")
{
    const int frames = static_cast<int> (3.0 * kSampleRate);
    const ReverbSetting setting { 0.0, 2.0, 1.0, ReverbNode::kMaxDampingHz, 1.0 };
    const StereoBuffer out = renderNode ({ 512 }, makeNoiseBurst (frames, 2048), setting);
    const int first = static_cast<int> (0.5 * kSampleRate);
    const int count = static_cast<int> (0.75 * kSampleRate);

    REQUIRE (normalizedCrossCorrelationPeak (out, first, count, 256) < 0.9);
    REQUIRE (monoSumRms (out, first, count) > 1.0e-5);
}

TEST_CASE ("ReverbNode damping lowers late-tail high-frequency tilt", "[reverb][damping]")
{
    const int frames = static_cast<int> (4.0 * kSampleRate);
    const StereoBuffer input = makeNoiseBurst (frames, 4096);
    const ReverbSetting open { 0.0, 3.0, 1.0, ReverbNode::kMaxDampingHz, 1.0 };
    const ReverbSetting damped { 0.0, 3.0, 1.0, 1200.0, 1.0 };
    const int first = static_cast<int> (1.0 * kSampleRate);
    const int count = static_cast<int> (1.0 * kSampleRate);

    const double openTilt = highLowTiltRatio (renderNode ({ 512 }, input, open), first, count);
    const double dampedTilt = highLowTiltRatio (renderNode ({ 512 }, input, damped), first, count);
    INFO ("open tilt " << openTilt << " damped tilt " << dampedTilt);
    REQUIRE (dampedTilt < openTilt * 0.75);
}

TEST_CASE ("ReverbNode keeps hostile inputs and params finite", "[reverb][robust]")
{
    ReverbNode node (100);
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
    std::array<Event, 25> events {};
    std::size_t count = 0;
    for (const double value : hostile)
    {
        events[count++] = makeParameterChangeEvent (0, 100, ReverbNode::kPreDelayParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, ReverbNode::kRt60ParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, ReverbNode::kSizeParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, ReverbNode::kDampingParamId, value);
        events[count++] = makeParameterChangeEvent (0, 100, ReverbNode::kMixParamId, value);
    }

    EventStream stream { std::span<const Event> (events.data(), count) };
    processBlock (node, input, 0, 512, stream);

    REQUIRE (allFinite (input));
}

TEST_CASE ("ReverbNode output is bit-identical across H14 block schedules", "[reverb][blocksize]")
{
    const ReverbSetting setting { 3.0, 1.4, 1.1, 6000.0, 0.65 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    const std::vector<float> reference = flatten (renderNode ({ 512 }, input, setting));

    for (const int block : { 1, 2, 3, 4, 5, 6, 7, 8, 9, 64, 128, 333, 512 })
    {
        INFO ("block size " << block);
        REQUIRE (flatten (renderNode ({ block }, input, setting)) == reference);
    }

    REQUIRE_FALSE (blockResetNegativeControlPasses());
}

TEST_CASE ("ReverbNode parameter smoothing is event-offset anchored across schedules", "[reverb][automation]")
{
    const ReverbSetting setting { 0.0, 1.0, 1.0, ReverbNode::kMaxDampingHz, 0.0 };
    const StereoBuffer input = deterministicStereoInput (kTotalFrames);
    const int eventFrame = 777;

    const StereoBuffer reference = renderNode ({ 512 }, input, setting, eventFrame, ReverbNode::kMixParamId, 1.0);
    REQUIRE (flatten (renderNode ({ 1 }, input, setting, eventFrame, ReverbNode::kMixParamId, 1.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 7 }, input, setting, eventFrame, ReverbNode::kMixParamId, 1.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 64 }, input, setting, eventFrame, ReverbNode::kMixParamId, 1.0)) == flatten (reference));
    REQUIRE (flatten (renderNode ({ 333 }, input, setting, eventFrame, ReverbNode::kMixParamId, 1.0)) == flatten (reference));

    REQUIRE (maxAbsDiff (renderNode ({ 512 }, input, setting), input) <= kUnityTolerance);
    REQUIRE (maxAbsDiff (reference, input) > 1.0e-3);
    REQUIRE_FALSE (eventAtBlockStartNegativeControlPasses());
}

TEST_CASE ("Compiled graph renders ReverbNode tail past the source end", "[reverb][tail][graph]")
{
    REQUIRE (compiledGraphTailGatePasses (true));
    REQUIRE_FALSE (compiledGraphTailGatePasses (false));
}
