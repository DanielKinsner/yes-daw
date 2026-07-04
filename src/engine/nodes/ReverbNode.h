// YES DAW -- ReverbNode: stereo built-in FDN reverb for H14 CP7.
//
// Stereo, in-place, 8-line Householder FDN with pre-delay and input diffusion allpasses. Parameter
// events carry normalized values; the node maps them to real values, clamps finitely, and ramps
// real-valued parameter changes from the event's absolute frame.

#pragma once

#include "engine/Node.h"
#include "engine/ParamSpec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace yesdaw::engine {

class ReverbNode final : public Node
{
public:
    static constexpr int kChannels = 2;
    static constexpr int kLines = 8;
    static constexpr int kAllpassStages = 2;

    static constexpr ParameterId kPreDelayParamId = 0;
    static constexpr ParameterId kRt60ParamId = 1;
    static constexpr ParameterId kSizeParamId = 2;
    static constexpr ParameterId kDampingParamId = 3;
    static constexpr ParameterId kMixParamId = 4;

    static constexpr double kMinPreDelayMs = 0.0;
    static constexpr double kMaxPreDelayMs = 200.0;
    static constexpr double kDefaultPreDelayMs = 0.0;
    static constexpr double kMinRt60Seconds = 0.1;
    static constexpr double kMaxRt60Seconds = 10.0;
    static constexpr double kDefaultRt60Seconds = 1.0;
    static constexpr double kMinSize = 0.5;
    static constexpr double kMaxSize = 2.0;
    static constexpr double kDefaultSize = 1.0;
    static constexpr double kMinDampingHz = 500.0;
    static constexpr double kMaxDampingHz = 20000.0;
    static constexpr double kDefaultDampingHz = kMaxDampingHz;
    static constexpr double kMinMix = 0.0;
    static constexpr double kMaxMix = 1.0;
    static constexpr double kDefaultMix = 0.0;

    explicit ReverbNode (NodeId id = 0) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                kChannels, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ false,
                                tailSamples() };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        sampleRate_ = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        const int maxBlock = maxBlockSize > 0 ? maxBlockSize : 1;

        const std::int64_t maxPreDelay = preDelaySamplesForMs (sampleRate_, kMaxPreDelayMs);
        for (int channel = 0; channel < kChannels; ++channel)
        {
            preDelay_[channel].assign (static_cast<std::size_t> (maxPreDelay + maxBlock + 2), 0.0f);
            preDelayWritePos_[channel] = 0;

            for (int stage = 0; stage < kAllpassStages; ++stage)
            {
                const std::int64_t length = allpassLengthFor (stage, sampleRate_);
                allpass_[channel][stage].assign (static_cast<std::size_t> (length), 0.0f);
                allpassWritePos_[channel][stage] = 0;
            }
        }

        for (int line = 0; line < kLines; ++line)
        {
            const std::int64_t length = lineLengthFor (line, kMaxSize, sampleRate_);
            lines_[line].assign (static_cast<std::size_t> (length), 0.0f);
            lineWritePos_[line] = 0;
            dampingState_[line] = 0.0;
        }

        runningFrame_ = 0;
        rampLengthSamples_ = std::max<std::int64_t> (1, static_cast<std::int64_t> (std::round (sampleRate_ * kRampSeconds)));
        prepared_ = true;
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (! prepared_)
            return;

        const int frames = args.numFrames > 0 ? args.numFrames : 0;
        if (frames <= 0)
            return;

        if (args.audio.channels == nullptr || args.audio.numChannels <= 0)
        {
            advanceRunningFrame (args, frames);
            return;
        }

        const int channels = std::min (args.audio.numChannels, kChannels);
        const std::int64_t blockStart = absoluteBlockStart (args);
        const std::span<const Event> events = args.events.events();
        std::size_t eventIndex = 0;

        for (int frame = 0; frame < frames; ++frame)
        {
            while (eventIndex < events.size() && events[eventIndex].timeInBlock < static_cast<std::uint32_t> (frame))
                ++eventIndex;

            while (eventIndex < events.size() && events[eventIndex].timeInBlock == static_cast<std::uint32_t> (frame))
            {
                consumeEvent (events[eventIndex], blockStart + frame);
                ++eventIndex;
            }

            const std::int64_t absoluteFrame = blockStart + frame;
            const double preDelayMs = paramValueAtFrame (preDelayMs_, absoluteFrame, clampPreDelayMs);
            const double rt60 = paramValueAtFrame (rt60Seconds_, absoluteFrame, clampRt60Seconds);
            const double size = paramValueAtFrame (size_, absoluteFrame, clampSize);
            const double dampingHz = paramValueAtFrame (dampingHz_, absoluteFrame, clampDampingHz);
            const double mix = paramValueAtFrame (mix_, absoluteFrame, clampMix);

            const double inL = inputAt (args, 0, frame);
            const double inR = inputAt (args, 1, frame);
            double diffuseL = processPreDelay (0, inL, preDelaySamplesForMs (sampleRate_, preDelayMs));
            double diffuseR = processPreDelay (1, inR, preDelaySamplesForMs (sampleRate_, preDelayMs));
            for (int stage = 0; stage < kAllpassStages; ++stage)
            {
                diffuseL = processAllpass (0, stage, diffuseL);
                diffuseR = processAllpass (1, stage, diffuseR);
            }

            const double monoIn = 0.5 * (diffuseL + diffuseR);
            std::array<double, kLines> lineOut {};
            std::array<std::int64_t, kLines> lineLengths {};
            for (int line = 0; line < kLines; ++line)
            {
                lineLengths[line] = lineLengthFor (line, size, sampleRate_);
                lineOut[line] = readLine (line, lineLengths[line]);
            }

            const double wetL = decorrelatedLeft (lineOut);
            const double wetR = decorrelatedRight (lineOut);

            const double alphaD = dampingAlpha (dampingHz);
            std::array<double, kLines> feedback {};
            double feedbackSum = 0.0;
            for (int line = 0; line < kLines; ++line)
            {
                dampingState_[line] += alphaD * (lineOut[line] - dampingState_[line]);
                feedback[line] = lineGain (lineLengths[line], rt60) * dampingState_[line];
                feedbackSum += feedback[line];
            }

            const double householderScale = 2.0 / static_cast<double> (kLines);
            for (int line = 0; line < kLines; ++line)
            {
                const double householder = feedback[line] - householderScale * feedbackSum;
                writeLine (line, lineLengths[line], monoIn + householder);
            }

            if (channels > 0 && args.audio.channels[0] != nullptr)
                args.audio.channels[0][frame] = finiteFloat ((1.0 - mix) * inL + mix * wetL);
            if (channels > 1 && args.audio.channels[1] != nullptr)
                args.audio.channels[1][frame] = finiteFloat ((1.0 - mix) * inR + mix * wetR);

            finishCompletedRamps (absoluteFrame);
        }

        advanceRunningFrame (args, frames);
    }

    void reset() noexcept override
    {
        for (int channel = 0; channel < kChannels; ++channel)
        {
            std::fill (preDelay_[channel].begin(), preDelay_[channel].end(), 0.0f);
            preDelayWritePos_[channel] = 0;
            for (int stage = 0; stage < kAllpassStages; ++stage)
            {
                std::fill (allpass_[channel][stage].begin(), allpass_[channel][stage].end(), 0.0f);
                allpassWritePos_[channel][stage] = 0;
            }
        }

        for (int line = 0; line < kLines; ++line)
        {
            std::fill (lines_[line].begin(), lines_[line].end(), 0.0f);
            lineWritePos_[line] = 0;
            dampingState_[line] = 0.0;
        }

        snapParam (preDelayMs_, preDelayMs_.target);
        snapParam (rt60Seconds_, rt60Seconds_.target);
        snapParam (size_, size_.target);
        snapParam (dampingHz_, dampingHz_.target);
        snapParam (mix_, mix_.target);
    }

    void release() override
    {
        for (int channel = 0; channel < kChannels; ++channel)
        {
            preDelay_[channel].clear();
            preDelay_[channel].shrink_to_fit();
            for (int stage = 0; stage < kAllpassStages; ++stage)
            {
                allpass_[channel][stage].clear();
                allpass_[channel][stage].shrink_to_fit();
            }
        }

        for (int line = 0; line < kLines; ++line)
        {
            lines_[line].clear();
            lines_[line].shrink_to_fit();
        }

        prepared_ = false;
    }

    void setInput (Node* in) noexcept { input_ = in; }

    void setParameters (double preDelayMs,
                        double rt60Seconds,
                        double size,
                        double dampingHz,
                        double mix) noexcept
    {
        snapParam (preDelayMs_, clampPreDelayMs (preDelayMs));
        snapParam (rt60Seconds_, clampRt60Seconds (rt60Seconds));
        snapParam (size_, clampSize (size));
        snapParam (dampingHz_, clampDampingHz (dampingHz));
        snapParam (mix_, clampMix (mix));
    }

    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        RampedParam* const param = paramForId (parameterId);
        if (param == nullptr)
            return;

        snapParam (*param, clampRealParameter (parameterId, mapNormalizedParameter (parameterId, normalizedValue)));
    }

    [[nodiscard]] std::int64_t tailSamples() const noexcept
    {
        const std::int64_t preDelay = preDelaySamplesForMs (sampleRate_, preDelayMs_.target);
        const std::int64_t maxLine = maxLineLengthForSize (size_.target, sampleRate_);
        const std::int64_t rt60Frames = static_cast<std::int64_t> (std::ceil (clampRt60Seconds (rt60Seconds_.target) * sampleRate_));
        const std::int64_t cap = static_cast<std::int64_t> (std::ceil (30.0 * sampleRate_));

        std::int64_t tail = 0;
        if (! checkedAddI64 (rt60Frames, preDelay, tail))
            return cap;
        if (! checkedAddI64 (tail, maxLine, tail))
            return cap;

        return std::clamp<std::int64_t> (tail, 0, cap);
    }

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kPreDelayParamId:
                return ParamSpec { parameterId, "reverb.pre_delay", "ms", kMinPreDelayMs, kMaxPreDelayMs,
                                   kDefaultPreDelayMs, ParamMapping::Linear, ParamSmoothing::Linear5Ms };
            case kRt60ParamId:
                return ParamSpec { parameterId, "reverb.rt60", "s", kMinRt60Seconds, kMaxRt60Seconds,
                                   kDefaultRt60Seconds, ParamMapping::Log, ParamSmoothing::Linear5Ms };
            case kSizeParamId:
                return ParamSpec { parameterId, "reverb.size", "", kMinSize, kMaxSize,
                                   kDefaultSize, ParamMapping::Linear, ParamSmoothing::Linear5Ms };
            case kDampingParamId:
                return ParamSpec { parameterId, "reverb.damping", "Hz", kMinDampingHz, kMaxDampingHz,
                                   kDefaultDampingHz, ParamMapping::Log, ParamSmoothing::Linear5Ms };
            case kMixParamId:
                return ParamSpec { parameterId, "reverb.mix", "", kMinMix, kMaxMix,
                                   kDefaultMix, ParamMapping::Linear, ParamSmoothing::Linear5Ms };
            default:
                return {};
        }
    }

    [[nodiscard]] static double mapNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        return mapNormalized (parameterSpec (parameterId), normalizedValue);
    }

    [[nodiscard]] static std::int64_t lineLengthSamplesFor (int line, double size, double sampleRate) noexcept
    {
        return lineLengthFor (line, size, sampleRate);
    }

    [[nodiscard]] static std::int64_t preDelaySamplesForMs (double sampleRate, double preDelayMs) noexcept
    {
        const double sr = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        return static_cast<std::int64_t> (std::llround (clampPreDelayMs (preDelayMs) * 0.001 * sr));
    }

private:
    struct RampedParam
    {
        double current = 0.0;
        double start = 0.0;
        double target = 0.0;
        std::int64_t eventFrame = 0;
        std::int64_t duration = 1;
        bool active = false;
    };

    static constexpr double kRampSeconds = 0.005;
    static constexpr double kPi = 3.141592653589793238462643383279502884;
    static constexpr double kInvSqrt8 = 0.35355339059327376220042218105242451964;
    static constexpr double kAllpassGain = 0.5;
    static constexpr std::array<int, kAllpassStages> kAllpassBaseLengths48k { 142, 379 };
    static constexpr std::array<int, kLines> kLineBaseLengths48k { 1123, 1327, 1523, 1723, 1931, 2129, 2311, 2539 };
    static constexpr std::array<double, kLines> kLeftSigns { 1.0, -1.0, 1.0, -1.0, 1.0, -1.0, 1.0, -1.0 };
    static constexpr std::array<double, kLines> kRightSigns { 1.0, 1.0, -1.0, -1.0, 1.0, 1.0, -1.0, -1.0 };

    [[nodiscard]] static double finiteOrDefault (double value, double fallback) noexcept
    {
        return std::isfinite (value) ? value : fallback;
    }

    [[nodiscard]] static double clampPreDelayMs (double preDelayMs) noexcept
    {
        return std::clamp (finiteOrDefault (preDelayMs, kDefaultPreDelayMs), kMinPreDelayMs, kMaxPreDelayMs);
    }

    [[nodiscard]] static double clampRt60Seconds (double rt60Seconds) noexcept
    {
        return std::clamp (finiteOrDefault (rt60Seconds, kDefaultRt60Seconds), kMinRt60Seconds, kMaxRt60Seconds);
    }

    [[nodiscard]] static double clampSize (double size) noexcept
    {
        return std::clamp (finiteOrDefault (size, kDefaultSize), kMinSize, kMaxSize);
    }

    [[nodiscard]] static double clampDampingHz (double dampingHz) noexcept
    {
        return std::clamp (finiteOrDefault (dampingHz, kDefaultDampingHz), kMinDampingHz, kMaxDampingHz);
    }

    [[nodiscard]] static double clampMix (double mix) noexcept
    {
        return std::clamp (finiteOrDefault (mix, kDefaultMix), kMinMix, kMaxMix);
    }

    [[nodiscard]] static double clampRealParameter (ParameterId parameterId, double value) noexcept
    {
        switch (parameterId)
        {
            case kPreDelayParamId:
                return clampPreDelayMs (value);
            case kRt60ParamId:
                return clampRt60Seconds (value);
            case kSizeParamId:
                return clampSize (value);
            case kDampingParamId:
                return clampDampingHz (value);
            case kMixParamId:
                return clampMix (value);
            default:
                return 0.0;
        }
    }

    [[nodiscard]] RampedParam* paramForId (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kPreDelayParamId:
                return &preDelayMs_;
            case kRt60ParamId:
                return &rt60Seconds_;
            case kSizeParamId:
                return &size_;
            case kDampingParamId:
                return &dampingHz_;
            case kMixParamId:
                return &mix_;
            default:
                return nullptr;
        }
    }

    static void snapParam (RampedParam& param, double value) noexcept
    {
        param.current = value;
        param.start = value;
        param.target = value;
        param.duration = 1;
        param.active = false;
    }

    [[nodiscard]] static double valueAtFrame (const RampedParam& param, std::int64_t frame) noexcept
    {
        if (! param.active)
            return param.current;

        if (frame <= param.eventFrame)
            return param.start;

        const std::int64_t elapsed = frame - param.eventFrame;
        if (elapsed >= param.duration)
            return param.target;

        const double t = static_cast<double> (elapsed) / static_cast<double> (param.duration);
        return param.start + (param.target - param.start) * t;
    }

    static void finishCompletedRamp (RampedParam& param, std::int64_t frame) noexcept
    {
        param.current = valueAtFrame (param, frame);
        if (param.active && frame >= param.eventFrame + param.duration)
            snapParam (param, param.target);
    }

    void finishCompletedRamps (std::int64_t frame) noexcept
    {
        finishCompletedRamp (preDelayMs_, frame);
        finishCompletedRamp (rt60Seconds_, frame);
        finishCompletedRamp (size_, frame);
        finishCompletedRamp (dampingHz_, frame);
        finishCompletedRamp (mix_, frame);
    }

    [[nodiscard]] static double paramValueAtFrame (const RampedParam& param,
                                                   std::int64_t frame,
                                                   double (*clampFn) (double)) noexcept
    {
        return clampFn (valueAtFrame (param, frame));
    }

    void startRamp (RampedParam& param, double target, std::int64_t eventFrame) noexcept
    {
        const std::int64_t previousFrame = eventFrame > 0 ? eventFrame - 1 : eventFrame;
        param.current = valueAtFrame (param, previousFrame);
        param.start = param.current;
        param.target = target;
        param.eventFrame = eventFrame;
        param.duration = rampLengthSamples_;
        param.active = param.target != param.start;
    }

    void consumeEvent (const Event& event, std::int64_t absoluteFrame) noexcept YESDAW_RT_HOT
    {
        if (event.type != EventType::ParameterChange
            || event.payload.parameter.targetNode != id_)
            return;

        RampedParam* const param = paramForId (event.payload.parameter.parameterId);
        if (param == nullptr)
            return;

        const double real = mapNormalizedParameter (event.payload.parameter.parameterId,
                                                    event.payload.parameter.normalizedValue);
        startRamp (*param,
                   clampRealParameter (event.payload.parameter.parameterId, real),
                   absoluteFrame);
    }

    [[nodiscard]] static bool checkedAddI64 (std::int64_t a, std::int64_t b, std::int64_t& out) noexcept
    {
        if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b)
            || (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b))
            return false;

        out = a + b;
        return true;
    }

    [[nodiscard]] static std::int64_t scaledLength (double baseLength, double scale, std::int64_t minimum) noexcept
    {
        const double value = baseLength * scale;
        if (! std::isfinite (value) || value < static_cast<double> (minimum))
            return minimum;

        if (value > static_cast<double> (std::numeric_limits<std::int64_t>::max()))
            return std::numeric_limits<std::int64_t>::max();

        return std::max<std::int64_t> (minimum, static_cast<std::int64_t> (std::llround (value)));
    }

    [[nodiscard]] static std::int64_t allpassLengthFor (int stage, double sampleRate) noexcept
    {
        const int index = std::clamp (stage, 0, kAllpassStages - 1);
        const double sr = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        return scaledLength (static_cast<double> (kAllpassBaseLengths48k[index]), sr / 48000.0, 1);
    }

    [[nodiscard]] static std::int64_t lineLengthFor (int line, double size, double sampleRate) noexcept
    {
        const int index = std::clamp (line, 0, kLines - 1);
        const double sr = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        return scaledLength (static_cast<double> (kLineBaseLengths48k[index]),
                             clampSize (size) * sr / 48000.0,
                             32);
    }

    [[nodiscard]] static std::int64_t maxLineLengthForSize (double size, double sampleRate) noexcept
    {
        std::int64_t maxLength = 32;
        for (int line = 0; line < kLines; ++line)
            maxLength = std::max (maxLength, lineLengthFor (line, size, sampleRate));
        return maxLength;
    }

    [[nodiscard]] static std::int64_t wrap (std::int64_t index, std::int64_t length) noexcept YESDAW_RT_HOT
    {
        if (length <= 0)
            return 0;

        std::int64_t wrapped = index % length;
        if (wrapped < 0)
            wrapped += length;
        return wrapped;
    }

    [[nodiscard]] double processPreDelay (int channel, double input, std::int64_t delaySamples) noexcept YESDAW_RT_HOT
    {
        std::vector<float>& ring = preDelay_[channel];
        if (ring.empty())
            return input;

        const std::int64_t length = static_cast<std::int64_t> (ring.size());
        std::int64_t& writePos = preDelayWritePos_[channel];
        const std::int64_t delay = std::clamp<std::int64_t> (delaySamples, 0, length - 1);
        const double out = delay == 0 ? input : static_cast<double> (ring[static_cast<std::size_t> (wrap (writePos - delay, length))]);
        ring[static_cast<std::size_t> (writePos)] = finiteFloat (input);
        writePos = wrap (writePos + 1, length);
        return out;
    }

    [[nodiscard]] double processAllpass (int channel, int stage, double input) noexcept YESDAW_RT_HOT
    {
        std::vector<float>& ring = allpass_[channel][stage];
        if (ring.empty())
            return input;

        const std::int64_t length = static_cast<std::int64_t> (ring.size());
        std::int64_t& writePos = allpassWritePos_[channel][stage];
        const double delayed = static_cast<double> (ring[static_cast<std::size_t> (writePos)]);
        const double out = delayed - kAllpassGain * input;
        ring[static_cast<std::size_t> (writePos)] = finiteFloat (input + kAllpassGain * out);
        writePos = wrap (writePos + 1, length);
        return out;
    }

    [[nodiscard]] double readLine (int line, std::int64_t length) const noexcept YESDAW_RT_HOT
    {
        const std::vector<float>& ring = lines_[line];
        if (ring.empty())
            return 0.0;

        const std::int64_t ringLength = static_cast<std::int64_t> (ring.size());
        const std::int64_t readPos = wrap (lineWritePos_[line], std::min (length, ringLength));
        return static_cast<double> (ring[static_cast<std::size_t> (readPos)]);
    }

    void writeLine (int line, std::int64_t length, double value) noexcept YESDAW_RT_HOT
    {
        std::vector<float>& ring = lines_[line];
        if (ring.empty())
            return;

        const std::int64_t ringLength = static_cast<std::int64_t> (ring.size());
        const std::int64_t activeLength = std::min (length, ringLength);
        std::int64_t& writePos = lineWritePos_[line];
        writePos = wrap (writePos, activeLength);
        ring[static_cast<std::size_t> (writePos)] = finiteFloat (value);
        writePos = wrap (writePos + 1, activeLength);
    }

    [[nodiscard]] double lineGain (std::int64_t lineLength, double rt60Seconds) const noexcept YESDAW_RT_HOT
    {
        const double rt60 = clampRt60Seconds (rt60Seconds);
        return std::pow (10.0, -3.0 * static_cast<double> (lineLength) / (rt60 * sampleRate_));
    }

    [[nodiscard]] double dampingAlpha (double dampingHz) const noexcept YESDAW_RT_HOT
    {
        const double fc = clampDampingHz (dampingHz);
        if (fc >= kMaxDampingHz)
            return 1.0;

        return 1.0 - std::exp (-2.0 * kPi * fc / sampleRate_);
    }

    [[nodiscard]] static double decorrelatedLeft (const std::array<double, kLines>& lineOut) noexcept YESDAW_RT_HOT
    {
        double sum = 0.0;
        for (int line = 0; line < kLines; ++line)
            sum += kLeftSigns[line] * lineOut[line];
        return kInvSqrt8 * sum;
    }

    [[nodiscard]] static double decorrelatedRight (const std::array<double, kLines>& lineOut) noexcept YESDAW_RT_HOT
    {
        double sum = 0.0;
        for (int line = 0; line < kLines; ++line)
            sum += kRightSigns[line] * lineOut[line];
        return kInvSqrt8 * sum;
    }

    [[nodiscard]] static double sanitizeInput (double sample) noexcept
    {
        return std::isfinite (sample) ? sample : 0.0;
    }

    [[nodiscard]] double inputAt (const ProcessArgs& args, int channel, int frame) const noexcept
    {
        if (channel >= args.audio.numChannels || args.audio.channels[channel] == nullptr)
            return 0.0;

        return sanitizeInput (static_cast<double> (args.audio.channels[channel][frame]));
    }

    [[nodiscard]] static float finiteFloat (double value) noexcept
    {
        if (! std::isfinite (value))
            return 0.0f;

        const double limit = static_cast<double> (std::numeric_limits<float>::max());
        return static_cast<float> (std::clamp (value, -limit, limit));
    }

    [[nodiscard]] std::int64_t absoluteBlockStart (const ProcessArgs& args) const noexcept
    {
        return args.transport.hasTimelineFrame ? args.transport.timelineFrame : runningFrame_;
    }

    void advanceRunningFrame (const ProcessArgs& args, int frames) noexcept
    {
        runningFrame_ = absoluteBlockStart (args) + frames;
    }

    NodeId id_;
    Node* input_ = nullptr;
    double sampleRate_ = 48000.0;
    std::int64_t rampLengthSamples_ = 240;
    std::int64_t runningFrame_ = 0;
    bool prepared_ = false;
    std::array<std::vector<float>, kChannels> preDelay_;
    std::array<std::int64_t, kChannels> preDelayWritePos_ {};
    std::array<std::array<std::vector<float>, kAllpassStages>, kChannels> allpass_;
    std::array<std::array<std::int64_t, kAllpassStages>, kChannels> allpassWritePos_ {};
    std::array<std::vector<float>, kLines> lines_;
    std::array<std::int64_t, kLines> lineWritePos_ {};
    std::array<double, kLines> dampingState_ {};
    RampedParam preDelayMs_ { kDefaultPreDelayMs, kDefaultPreDelayMs, kDefaultPreDelayMs, 0, 1, false };
    RampedParam rt60Seconds_ { kDefaultRt60Seconds, kDefaultRt60Seconds, kDefaultRt60Seconds, 0, 1, false };
    RampedParam size_ { kDefaultSize, kDefaultSize, kDefaultSize, 0, 1, false };
    RampedParam dampingHz_ { kDefaultDampingHz, kDefaultDampingHz, kDefaultDampingHz, 0, 1, false };
    RampedParam mix_ { kDefaultMix, kDefaultMix, kDefaultMix, 0, 1, false };
};

} // namespace yesdaw::engine
