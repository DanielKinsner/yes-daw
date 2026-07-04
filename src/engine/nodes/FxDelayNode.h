// YES DAW -- FxDelayNode: stereo built-in delay for H14 CP6.
//
// Stereo, in-place delay with feedback, feedback-path damping, ping-pong routing, and delay-time
// changes that switch by dual-tap equal-power crossfade after the real-valued time parameter ramp.

#pragma once

#include "engine/Node.h"
#include "engine/ParamSpec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace yesdaw::engine {

class FxDelayNode final : public Node
{
public:
    static constexpr int kChannels = 2;

    static constexpr ParameterId kTimeLeftParamId = 0;
    static constexpr ParameterId kTimeRightParamId = 1;
    static constexpr ParameterId kFeedbackParamId = 2;
    static constexpr ParameterId kDampingParamId = 3;
    static constexpr ParameterId kPingPongParamId = 4;
    static constexpr ParameterId kMixParamId = 5;

    static constexpr double kMinTimeMs = 1.0;
    static constexpr double kMaxTimeMs = 2000.0;
    static constexpr double kDefaultTimeMs = 250.0;
    static constexpr double kMinFeedback = 0.0;
    static constexpr double kMaxFeedback = 0.95;
    static constexpr double kHardMaxFeedback = 0.98;
    static constexpr double kDefaultFeedback = 0.0;
    static constexpr double kMinDampingHz = 500.0;
    static constexpr double kMaxDampingHz = 20000.0;
    static constexpr double kDefaultDampingHz = kMaxDampingHz;
    static constexpr double kMinMix = 0.0;
    static constexpr double kMaxMix = 1.0;
    static constexpr double kDefaultMix = 0.0;

    explicit FxDelayNode (NodeId id = 0) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                kChannels, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ false };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        sampleRate_ = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        const int maxBlock = maxBlockSize > 0 ? maxBlockSize : 1;
        ringFrames_ = static_cast<std::int64_t> (std::ceil (2.0 * sampleRate_))
                    + static_cast<std::int64_t> (maxBlock) + 2;
        ring_.assign (static_cast<std::size_t> (ringFrames_) * kChannels, 0.0f);
        writePos_ = 0;
        runningFrame_ = 0;
        rampLengthSamples_ = std::max<std::int64_t> (1, static_cast<std::int64_t> (std::round (sampleRate_ * kRampSeconds)));
        crossfadeLengthSamples_ = std::max<std::int64_t> (1, static_cast<std::int64_t> (std::round (sampleRate_ * kCrossfadeSeconds)));
        activeDelaySamples_[0] = delaySamplesForTimeMs (sampleRate_, timeMs_[0].target);
        activeDelaySamples_[1] = delaySamplesForTimeMs (sampleRate_, timeMs_[1].target);
        crossfades_[0] = DelayCrossfade {};
        crossfades_[1] = DelayCrossfade {};
        dampingState_[0] = 0.0;
        dampingState_[1] = 0.0;
        prepared_ = true;
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (! prepared_ || ring_.empty() || ringFrames_ <= 0)
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
            refreshDelayTime (0, absoluteFrame);
            refreshDelayTime (1, absoluteFrame);

            const double feedback = paramValueAtFrame (feedback_, absoluteFrame, clampFeedback);
            const double dampingHz = paramValueAtFrame (dampingHz_, absoluteFrame, clampDampingHz);
            const double mix = paramValueAtFrame (mix_, absoluteFrame, clampMix);
            const double alphaD = dampingAlpha (dampingHz);
            const bool pingPong = pingPong_;

            const double inL = inputAt (args, 0, frame);
            const double inR = inputAt (args, 1, frame);
            const double tapL = readWetTap (0, absoluteFrame);
            const double tapR = readWetTap (1, absoluteFrame);
            const double wetL = pingPong ? tapR : tapL;
            const double wetR = pingPong ? tapL : tapR;

            dampingState_[0] += alphaD * (tapL - dampingState_[0]);
            dampingState_[1] += alphaD * (tapR - dampingState_[1]);

            const double fbL = pingPong ? dampingState_[1] : dampingState_[0];
            const double fbR = pingPong ? dampingState_[0] : dampingState_[1];
            writeSample (0, inL + feedback * fbL);
            writeSample (1, inR + feedback * fbR);

            if (channels > 0 && args.audio.channels[0] != nullptr)
                args.audio.channels[0][frame] = finiteFloat ((1.0 - mix) * inL + mix * wetL);
            if (channels > 1 && args.audio.channels[1] != nullptr)
                args.audio.channels[1][frame] = finiteFloat ((1.0 - mix) * inR + mix * wetR);

            finishCompletedRamps (absoluteFrame);
            finishCompletedCrossfade (0, absoluteFrame);
            finishCompletedCrossfade (1, absoluteFrame);
            writePos_ = wrap (writePos_ + 1);
        }

        advanceRunningFrame (args, frames);
    }

    void reset() noexcept override
    {
        if (! ring_.empty())
            std::memset (ring_.data(), 0, ring_.size() * sizeof (float));

        writePos_ = 0;
        dampingState_[0] = 0.0;
        dampingState_[1] = 0.0;
        snapParam (timeMs_[0], timeMs_[0].target);
        snapParam (timeMs_[1], timeMs_[1].target);
        snapParam (feedback_, feedback_.target);
        snapParam (dampingHz_, dampingHz_.target);
        snapParam (mix_, mix_.target);
        activeDelaySamples_[0] = delaySamplesForTimeMs (sampleRate_, timeMs_[0].target);
        activeDelaySamples_[1] = delaySamplesForTimeMs (sampleRate_, timeMs_[1].target);
        crossfades_[0] = DelayCrossfade {};
        crossfades_[1] = DelayCrossfade {};
    }

    void release() override
    {
        ring_.clear();
        ring_.shrink_to_fit();
        ringFrames_ = 0;
        writePos_ = 0;
        prepared_ = false;
    }

    void setInput (Node* in) noexcept { input_ = in; }

    void setParameters (double timeLeftMs,
                        double timeRightMs,
                        double feedback,
                        double dampingHz,
                        bool pingPong,
                        double mix) noexcept
    {
        snapParam (timeMs_[0], clampTimeMs (timeLeftMs));
        snapParam (timeMs_[1], clampTimeMs (timeRightMs));
        snapParam (feedback_, clampFeedback (feedback));
        snapParam (dampingHz_, clampDampingHz (dampingHz));
        snapParam (mix_, clampMix (mix));
        pingPong_ = pingPong;
        activeDelaySamples_[0] = delaySamplesForTimeMs (sampleRate_, timeMs_[0].target);
        activeDelaySamples_[1] = delaySamplesForTimeMs (sampleRate_, timeMs_[1].target);
        crossfades_[0] = DelayCrossfade {};
        crossfades_[1] = DelayCrossfade {};
    }

    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        if (parameterId == kPingPongParamId)
        {
            pingPong_ = steppedBoolFromNormalized (normalizedValue);
            return;
        }

        RampedParam* const param = paramForId (parameterId);
        if (param == nullptr)
            return;

        snapParam (*param, clampRealParameter (parameterId, mapNormalizedParameter (parameterId, normalizedValue)));
        if (parameterId == kTimeLeftParamId)
            activeDelaySamples_[0] = delaySamplesForTimeMs (sampleRate_, timeMs_[0].target);
        else if (parameterId == kTimeRightParamId)
            activeDelaySamples_[1] = delaySamplesForTimeMs (sampleRate_, timeMs_[1].target);
    }

    [[nodiscard]] std::int64_t tailSamples() const noexcept
    {
        const std::int64_t delay = std::max (delaySamplesForTimeMs (sampleRate_, timeMs_[0].target),
                                             delaySamplesForTimeMs (sampleRate_, timeMs_[1].target));
        const double feedback = clampFeedback (feedback_.target);
        const std::int64_t cap = static_cast<std::int64_t> (std::ceil (30.0 * sampleRate_));
        if (feedback <= 0.0)
            return std::min (delay, cap);

        const double taps = std::ceil (std::log (0.001) / std::log (feedback));
        if (! std::isfinite (taps) || taps <= 0.0)
            return cap;

        const long double tail = static_cast<long double> (delay) * static_cast<long double> (taps);
        if (tail >= static_cast<long double> (cap))
            return cap;

        return static_cast<std::int64_t> (tail);
    }

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kTimeLeftParamId:
                return ParamSpec { parameterId, "delay.time_l", "ms", kMinTimeMs, kMaxTimeMs,
                                   kDefaultTimeMs, ParamMapping::Log, ParamSmoothing::Linear5Ms };
            case kTimeRightParamId:
                return ParamSpec { parameterId, "delay.time_r", "ms", kMinTimeMs, kMaxTimeMs,
                                   kDefaultTimeMs, ParamMapping::Log, ParamSmoothing::Linear5Ms };
            case kFeedbackParamId:
                return ParamSpec { parameterId, "delay.feedback", "", kMinFeedback, kMaxFeedback,
                                   kDefaultFeedback, ParamMapping::Linear, ParamSmoothing::Linear5Ms };
            case kDampingParamId:
                return ParamSpec { parameterId, "delay.damping", "Hz", kMinDampingHz, kMaxDampingHz,
                                   kDefaultDampingHz, ParamMapping::Log, ParamSmoothing::Linear5Ms };
            case kPingPongParamId:
                return ParamSpec { parameterId, "delay.ping_pong", "", 0.0, 1.0,
                                   0.0, ParamMapping::Linear, ParamSmoothing::None };
            case kMixParamId:
                return ParamSpec { parameterId, "delay.mix", "", kMinMix, kMaxMix,
                                   kDefaultMix, ParamMapping::Linear, ParamSmoothing::Linear5Ms };
            default:
                return {};
        }
    }

    [[nodiscard]] static double mapNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        if (parameterId == kPingPongParamId)
            return steppedBoolFromNormalized (normalizedValue) ? 1.0 : 0.0;

        return mapNormalized (parameterSpec (parameterId), normalizedValue);
    }

    [[nodiscard]] static std::int64_t delaySamplesForTimeMs (double sampleRate, double timeMs) noexcept
    {
        const double sr = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        const double clamped = clampTimeMs (timeMs);
        return std::max<std::int64_t> (1, static_cast<std::int64_t> (std::llround (clamped * 0.001 * sr)));
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

    struct DelayCrossfade
    {
        std::int64_t oldDelay = 0;
        std::int64_t newDelay = 0;
        std::int64_t startFrame = 0;
        std::int64_t duration = 1;
        bool active = false;
    };

    static constexpr double kRampSeconds = 0.005;
    static constexpr double kCrossfadeSeconds = 0.020;
    static constexpr double kPi = 3.141592653589793238462643383279502884;

    [[nodiscard]] static double finiteOrDefault (double value, double fallback) noexcept
    {
        return std::isfinite (value) ? value : fallback;
    }

    [[nodiscard]] static double clampTimeMs (double timeMs) noexcept
    {
        return std::clamp (finiteOrDefault (timeMs, kDefaultTimeMs), kMinTimeMs, kMaxTimeMs);
    }

    [[nodiscard]] static double clampFeedback (double feedback) noexcept
    {
        return std::clamp (finiteOrDefault (feedback, kDefaultFeedback), kMinFeedback, kHardMaxFeedback);
    }

    [[nodiscard]] static double clampDampingHz (double dampingHz) noexcept
    {
        return std::clamp (finiteOrDefault (dampingHz, kDefaultDampingHz), kMinDampingHz, kMaxDampingHz);
    }

    [[nodiscard]] static double clampMix (double mix) noexcept
    {
        return std::clamp (finiteOrDefault (mix, kDefaultMix), kMinMix, kMaxMix);
    }

    [[nodiscard]] static bool steppedBoolFromNormalized (double normalizedValue) noexcept
    {
        if (! std::isfinite (normalizedValue))
            return false;

        return normalizedValue >= 0.5;
    }

    [[nodiscard]] static double clampRealParameter (ParameterId parameterId, double value) noexcept
    {
        switch (parameterId)
        {
            case kTimeLeftParamId:
            case kTimeRightParamId:
                return clampTimeMs (value);
            case kFeedbackParamId:
                return clampFeedback (value);
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
            case kTimeLeftParamId:
                return &timeMs_[0];
            case kTimeRightParamId:
                return &timeMs_[1];
            case kFeedbackParamId:
                return &feedback_;
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
        finishCompletedRamp (feedback_, frame);
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

        if (event.payload.parameter.parameterId == kPingPongParamId)
        {
            pingPong_ = steppedBoolFromNormalized (event.payload.parameter.normalizedValue);
            return;
        }

        RampedParam* const param = paramForId (event.payload.parameter.parameterId);
        if (param == nullptr)
            return;

        const double real = mapNormalizedParameter (event.payload.parameter.parameterId,
                                                    event.payload.parameter.normalizedValue);
        startRamp (*param,
                   clampRealParameter (event.payload.parameter.parameterId, real),
                   absoluteFrame);
    }

    void refreshDelayTime (int channel, std::int64_t absoluteFrame) noexcept YESDAW_RT_HOT
    {
        RampedParam& param = timeMs_[channel];
        if (! param.active || absoluteFrame < param.eventFrame + param.duration)
            return;

        const std::int64_t newDelay = delaySamplesForTimeMs (sampleRate_, param.target);
        const std::int64_t oldDelay = currentDelayForChannel (channel, absoluteFrame);
        snapParam (param, param.target);

        if (newDelay == oldDelay)
        {
            activeDelaySamples_[channel] = newDelay;
            crossfades_[channel] = DelayCrossfade {};
            return;
        }

        crossfades_[channel] = DelayCrossfade { oldDelay, newDelay, absoluteFrame,
                                                crossfadeLengthSamples_, true };
    }

    [[nodiscard]] std::int64_t currentDelayForChannel (int channel, std::int64_t absoluteFrame) const noexcept
    {
        const DelayCrossfade& xfade = crossfades_[channel];
        if (! xfade.active)
            return activeDelaySamples_[channel];

        return absoluteFrame >= xfade.startFrame + xfade.duration ? xfade.newDelay : xfade.oldDelay;
    }

    void finishCompletedCrossfade (int channel, std::int64_t absoluteFrame) noexcept
    {
        DelayCrossfade& xfade = crossfades_[channel];
        if (xfade.active && absoluteFrame >= xfade.startFrame + xfade.duration)
        {
            activeDelaySamples_[channel] = xfade.newDelay;
            xfade = DelayCrossfade {};
        }
    }

    [[nodiscard]] double readWetTap (int channel, std::int64_t absoluteFrame) const noexcept YESDAW_RT_HOT
    {
        const DelayCrossfade& xfade = crossfades_[channel];
        if (! xfade.active)
            return readDelay (channel, activeDelaySamples_[channel]);

        const std::int64_t elapsed = absoluteFrame - xfade.startFrame;
        if (elapsed <= 0)
            return readDelay (channel, xfade.oldDelay);
        if (elapsed >= xfade.duration)
            return readDelay (channel, xfade.newDelay);

        const double t = static_cast<double> (elapsed) / static_cast<double> (xfade.duration);
        const double theta = 0.5 * kPi * t;
        return std::cos (theta) * readDelay (channel, xfade.oldDelay)
             + std::sin (theta) * readDelay (channel, xfade.newDelay);
    }

    [[nodiscard]] double readDelay (int channel, std::int64_t delaySamples) const noexcept YESDAW_RT_HOT
    {
        const std::int64_t clampedDelay = std::clamp<std::int64_t> (delaySamples, 1, ringFrames_ - 1);
        return static_cast<double> (ring_[ringIndex (channel, writePos_ - clampedDelay)]);
    }

    void writeSample (int channel, double value) noexcept YESDAW_RT_HOT
    {
        ring_[ringIndex (channel, writePos_)] = finiteFloat (value);
    }

    [[nodiscard]] std::size_t ringIndex (int channel, std::int64_t frameIndex) const noexcept YESDAW_RT_HOT
    {
        return static_cast<std::size_t> (channel) * static_cast<std::size_t> (ringFrames_)
             + static_cast<std::size_t> (wrap (frameIndex));
    }

    [[nodiscard]] std::int64_t wrap (std::int64_t index) const noexcept YESDAW_RT_HOT
    {
        const std::int64_t m = ringFrames_;
        std::int64_t wrapped = index % m;
        if (wrapped < 0)
            wrapped += m;
        return wrapped;
    }

    [[nodiscard]] double inputAt (const ProcessArgs& args, int channel, int frame) const noexcept
    {
        if (channel >= args.audio.numChannels || args.audio.channels[channel] == nullptr)
            return 0.0;

        return sanitizeInput (static_cast<double> (args.audio.channels[channel][frame]));
    }

    [[nodiscard]] double dampingAlpha (double dampingHz) const noexcept
    {
        const double fc = clampDampingHz (dampingHz);
        if (fc >= kMaxDampingHz)
            return 1.0;

        return 1.0 - std::exp (-2.0 * kPi * fc / sampleRate_);
    }

    [[nodiscard]] static double sanitizeInput (double sample) noexcept
    {
        return std::isfinite (sample) ? sample : 0.0;
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
    std::int64_t ringFrames_ = 0;
    std::int64_t writePos_ = 0;
    std::int64_t runningFrame_ = 0;
    std::int64_t rampLengthSamples_ = 240;
    std::int64_t crossfadeLengthSamples_ = 960;
    bool prepared_ = false;
    bool pingPong_ = false;
    std::vector<float> ring_;
    std::array<std::int64_t, kChannels> activeDelaySamples_ {
        delaySamplesForTimeMs (48000.0, kDefaultTimeMs),
        delaySamplesForTimeMs (48000.0, kDefaultTimeMs),
    };
    std::array<double, kChannels> dampingState_ { 0.0, 0.0 };
    std::array<RampedParam, kChannels> timeMs_ {
        RampedParam { kDefaultTimeMs, kDefaultTimeMs, kDefaultTimeMs, 0, 1, false },
        RampedParam { kDefaultTimeMs, kDefaultTimeMs, kDefaultTimeMs, 0, 1, false },
    };
    RampedParam feedback_ { kDefaultFeedback, kDefaultFeedback, kDefaultFeedback, 0, 1, false };
    RampedParam dampingHz_ { kDefaultDampingHz, kDefaultDampingHz, kDefaultDampingHz, 0, 1, false };
    RampedParam mix_ { kDefaultMix, kDefaultMix, kDefaultMix, 0, 1, false };
    std::array<DelayCrossfade, kChannels> crossfades_ {};
};

} // namespace yesdaw::engine
