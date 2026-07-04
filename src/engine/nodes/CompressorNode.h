// YES DAW -- CompressorNode: feed-forward stereo-linked compressor for H14 CP5.
//
// Stereo, in-place, log-domain detector per Appendix A2 of the H14 FX-suite plan. Parameter events
// carry normalized values; the node maps them to real values, clamps finitely, and ramps real-valued
// parameter changes from the event's absolute frame.

#pragma once

#include "engine/Node.h"
#include "engine/ParamSpec.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>

namespace yesdaw::engine {

class CompressorNode final : public Node
{
public:
    static constexpr int kChannels = 2;

    static constexpr ParameterId kThresholdParamId = 0;
    static constexpr ParameterId kRatioParamId = 1;
    static constexpr ParameterId kAttackParamId = 2;
    static constexpr ParameterId kReleaseParamId = 3;
    static constexpr ParameterId kKneeParamId = 4;
    static constexpr ParameterId kMakeupParamId = 5;

    static constexpr double kMinThresholdDb = -60.0;
    static constexpr double kMaxThresholdDb = 0.0;
    static constexpr double kDefaultThresholdDb = 0.0;
    static constexpr double kMinRatio = 1.0;
    static constexpr double kMaxRatio = 20.0;
    static constexpr double kDefaultRatio = 1.0;
    static constexpr double kMinAttackMs = 0.1;
    static constexpr double kMaxAttackMs = 300.0;
    static constexpr double kDefaultAttackMs = 10.0;
    static constexpr double kMinReleaseMs = 10.0;
    static constexpr double kMaxReleaseMs = 3000.0;
    static constexpr double kDefaultReleaseMs = 100.0;
    static constexpr double kMinKneeDb = 0.0;
    static constexpr double kMaxKneeDb = 24.0;
    static constexpr double kDefaultKneeDb = 0.0;
    static constexpr double kMinMakeupDb = 0.0;
    static constexpr double kMaxMakeupDb = 24.0;
    static constexpr double kDefaultMakeupDb = 0.0;

    explicit CompressorNode (NodeId id = 0) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                kChannels, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ false };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double sampleRate, int /*maxBlockSize*/) override
    {
        sampleRate_ = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        rampLengthSamples_ = std::max<std::int64_t> (1, static_cast<std::int64_t> (std::round (sampleRate_ * kRampSeconds)));
        runningFrame_ = 0;
        envelopeDb_ = kDetectorFloorDb;
        publishedGainReductionDb_.store (0.0f, std::memory_order_release);
        prepared_ = true;
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (! prepared_)
            return;

        const int frames = args.numFrames > 0 ? args.numFrames : 0;
        const int channels = std::min (args.audio.numChannels, kChannels);
        if (channels <= 0 || args.audio.channels == nullptr)
        {
            publishedGainReductionDb_.store (0.0f, std::memory_order_release);
            advanceRunningFrame (args, frames);
            return;
        }

        const std::int64_t blockStart = absoluteBlockStart (args);
        std::size_t eventIndex = 0;
        const std::span<const Event> events = args.events.events();
        double blockMaxGainReductionDb = 0.0;

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
            const double thresholdDb = paramValueAtFrame (thresholdDb_, absoluteFrame, clampThresholdDb);
            const double ratio = paramValueAtFrame (ratio_, absoluteFrame, clampRatio);
            const double attackMs = paramValueAtFrame (attackMs_, absoluteFrame, clampAttackMs);
            const double releaseMs = paramValueAtFrame (releaseMs_, absoluteFrame, clampReleaseMs);
            const double kneeDb = paramValueAtFrame (kneeDb_, absoluteFrame, clampKneeDb);
            const double makeupDb = paramValueAtFrame (makeupDb_, absoluteFrame, clampMakeupDb);

            const double left = channelSample (args, 0, frame);
            const double right = channels > 1 ? channelSample (args, 1, frame) : 0.0;
            const double detector = std::max (std::fabs (left), std::fabs (right));
            const double inputDb = 20.0 * std::log10 (std::max (detector, kDetectorFloorLinear));

            const double alpha = inputDb > envelopeDb_ ? alphaForMs (attackMs) : alphaForMs (releaseMs);
            envelopeDb_ += alpha * (inputDb - envelopeDb_);

            const double gainReductionDb = gainReductionFor (envelopeDb_, thresholdDb, ratio, kneeDb);
            blockMaxGainReductionDb = std::max (blockMaxGainReductionDb, gainReductionDb);
            const double gain = std::pow (10.0, (makeupDb - gainReductionDb) / 20.0);

            for (int channel = 0; channel < channels; ++channel)
            {
                float* const data = args.audio.channels[channel];
                if (data == nullptr)
                    continue;

                const double input = sanitizeInput (static_cast<double> (data[frame]));
                data[frame] = finiteFloat (input * gain);
            }

            finishCompletedRamps (absoluteFrame);
        }

        publishedGainReductionDb_.store (finiteFloat (blockMaxGainReductionDb), std::memory_order_release);
        advanceRunningFrame (args, frames);
    }

    void reset() noexcept override
    {
        envelopeDb_ = kDetectorFloorDb;
        snapParam (thresholdDb_, thresholdDb_.target);
        snapParam (ratio_, ratio_.target);
        snapParam (attackMs_, attackMs_.target);
        snapParam (releaseMs_, releaseMs_.target);
        snapParam (kneeDb_, kneeDb_.target);
        snapParam (makeupDb_, makeupDb_.target);
        publishedGainReductionDb_.store (0.0f, std::memory_order_release);
    }

    void release() override {}

    void setInput (Node* in) noexcept { input_ = in; }

    void setParameters (double thresholdDb,
                        double ratio,
                        double attackMs,
                        double releaseMs,
                        double kneeDb,
                        double makeupDb) noexcept
    {
        snapParam (thresholdDb_, clampThresholdDb (thresholdDb));
        snapParam (ratio_, clampRatio (ratio));
        snapParam (attackMs_, clampAttackMs (attackMs));
        snapParam (releaseMs_, clampReleaseMs (releaseMs));
        snapParam (kneeDb_, clampKneeDb (kneeDb));
        snapParam (makeupDb_, clampMakeupDb (makeupDb));
    }

    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        RampedParam* const param = paramForId (parameterId);
        if (param == nullptr)
            return;

        snapParam (*param, clampRealParameter (parameterId, mapNormalizedParameter (parameterId, normalizedValue)));
    }

    [[nodiscard]] float gainReductionDb() const noexcept
    {
        return publishedGainReductionDb_.load (std::memory_order_acquire);
    }

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kThresholdParamId:
                return ParamSpec { parameterId, "compressor.threshold", "dB", kMinThresholdDb, kMaxThresholdDb,
                                   kDefaultThresholdDb, ParamMapping::Db, ParamSmoothing::Linear5Ms };
            case kRatioParamId:
                return ParamSpec { parameterId, "compressor.ratio", ":1", kMinRatio, kMaxRatio,
                                   kDefaultRatio, ParamMapping::Linear, ParamSmoothing::Linear5Ms };
            case kAttackParamId:
                return ParamSpec { parameterId, "compressor.attack", "ms", kMinAttackMs, kMaxAttackMs,
                                   kDefaultAttackMs, ParamMapping::Log, ParamSmoothing::Linear5Ms };
            case kReleaseParamId:
                return ParamSpec { parameterId, "compressor.release", "ms", kMinReleaseMs, kMaxReleaseMs,
                                   kDefaultReleaseMs, ParamMapping::Log, ParamSmoothing::Linear5Ms };
            case kKneeParamId:
                return ParamSpec { parameterId, "compressor.knee", "dB", kMinKneeDb, kMaxKneeDb,
                                   kDefaultKneeDb, ParamMapping::Linear, ParamSmoothing::Linear5Ms };
            case kMakeupParamId:
                return ParamSpec { parameterId, "compressor.makeup", "dB", kMinMakeupDb, kMaxMakeupDb,
                                   kDefaultMakeupDb, ParamMapping::Db, ParamSmoothing::Linear5Ms };
            default:
                return {};
        }
    }

    [[nodiscard]] static double mapNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        return mapNormalized (parameterSpec (parameterId), normalizedValue);
    }

    [[nodiscard]] static double closedFormGainReductionDb (double envelopeDb,
                                                           double thresholdDb,
                                                           double ratio,
                                                           double kneeDb) noexcept
    {
        const double t = clampThresholdDb (thresholdDb);
        const double r = clampRatio (ratio);
        const double w = clampKneeDb (kneeDb);
        const double e = std::isfinite (envelopeDb) ? envelopeDb : kDetectorFloorDb;
        const double factor = 1.0 - 1.0 / r;

        if (factor <= 0.0)
            return 0.0;

        if (w <= 0.0)
            return e <= t ? 0.0 : std::max (0.0, (e - t) * factor);

        const double lower = t - 0.5 * w;
        const double upper = t + 0.5 * w;
        if (e < lower)
            return 0.0;
        if (e > upper)
            return std::max (0.0, (e - t) * factor);

        const double x = e - t + 0.5 * w;
        return std::max (0.0, factor * x * x / (2.0 * w));
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
    static constexpr double kDetectorFloorLinear = 1.0e-10;
    static constexpr double kDetectorFloorDb = -200.0;

    [[nodiscard]] static double finiteOrDefault (double value, double fallback) noexcept
    {
        return std::isfinite (value) ? value : fallback;
    }

    [[nodiscard]] static double clampThresholdDb (double thresholdDb) noexcept
    {
        return std::clamp (finiteOrDefault (thresholdDb, kDefaultThresholdDb), kMinThresholdDb, kMaxThresholdDb);
    }

    [[nodiscard]] static double clampRatio (double ratio) noexcept
    {
        return std::clamp (finiteOrDefault (ratio, kDefaultRatio), kMinRatio, kMaxRatio);
    }

    [[nodiscard]] static double clampAttackMs (double attackMs) noexcept
    {
        return std::clamp (finiteOrDefault (attackMs, kDefaultAttackMs), kMinAttackMs, kMaxAttackMs);
    }

    [[nodiscard]] static double clampReleaseMs (double releaseMs) noexcept
    {
        return std::clamp (finiteOrDefault (releaseMs, kDefaultReleaseMs), kMinReleaseMs, kMaxReleaseMs);
    }

    [[nodiscard]] static double clampKneeDb (double kneeDb) noexcept
    {
        return std::clamp (finiteOrDefault (kneeDb, kDefaultKneeDb), kMinKneeDb, kMaxKneeDb);
    }

    [[nodiscard]] static double clampMakeupDb (double makeupDb) noexcept
    {
        return std::clamp (finiteOrDefault (makeupDb, kDefaultMakeupDb), kMinMakeupDb, kMaxMakeupDb);
    }

    [[nodiscard]] static double clampRealParameter (ParameterId parameterId, double value) noexcept
    {
        switch (parameterId)
        {
            case kThresholdParamId:
                return clampThresholdDb (value);
            case kRatioParamId:
                return clampRatio (value);
            case kAttackParamId:
                return clampAttackMs (value);
            case kReleaseParamId:
                return clampReleaseMs (value);
            case kKneeParamId:
                return clampKneeDb (value);
            case kMakeupParamId:
                return clampMakeupDb (value);
            default:
                return 0.0;
        }
    }

    [[nodiscard]] RampedParam* paramForId (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kThresholdParamId:
                return &thresholdDb_;
            case kRatioParamId:
                return &ratio_;
            case kAttackParamId:
                return &attackMs_;
            case kReleaseParamId:
                return &releaseMs_;
            case kKneeParamId:
                return &kneeDb_;
            case kMakeupParamId:
                return &makeupDb_;
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
        if (param.active && frame >= param.eventFrame + param.duration)
            snapParam (param, param.target);
    }

    void finishCompletedRamps (std::int64_t frame) noexcept
    {
        finishCompletedRamp (thresholdDb_, frame);
        finishCompletedRamp (ratio_, frame);
        finishCompletedRamp (attackMs_, frame);
        finishCompletedRamp (releaseMs_, frame);
        finishCompletedRamp (kneeDb_, frame);
        finishCompletedRamp (makeupDb_, frame);
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

    [[nodiscard]] double alphaForMs (double tauMs) const noexcept
    {
        const double tauSeconds = std::max (tauMs, kMinAttackMs) * 0.001;
        return 1.0 - std::exp (-1.0 / (tauSeconds * sampleRate_));
    }

    [[nodiscard]] static double gainReductionFor (double envelopeDb,
                                                  double thresholdDb,
                                                  double ratio,
                                                  double kneeDb) noexcept
    {
        return closedFormGainReductionDb (envelopeDb, thresholdDb, ratio, kneeDb);
    }

    [[nodiscard]] static double sanitizeInput (double sample) noexcept
    {
        return std::isfinite (sample) ? sample : 0.0;
    }

    [[nodiscard]] double channelSample (const ProcessArgs& args, int channel, int frame) const noexcept
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
    double envelopeDb_ = kDetectorFloorDb;
    bool prepared_ = false;
    RampedParam thresholdDb_ { kDefaultThresholdDb, kDefaultThresholdDb, kDefaultThresholdDb, 0, 1, false };
    RampedParam ratio_ { kDefaultRatio, kDefaultRatio, kDefaultRatio, 0, 1, false };
    RampedParam attackMs_ { kDefaultAttackMs, kDefaultAttackMs, kDefaultAttackMs, 0, 1, false };
    RampedParam releaseMs_ { kDefaultReleaseMs, kDefaultReleaseMs, kDefaultReleaseMs, 0, 1, false };
    RampedParam kneeDb_ { kDefaultKneeDb, kDefaultKneeDb, kDefaultKneeDb, 0, 1, false };
    RampedParam makeupDb_ { kDefaultMakeupDb, kDefaultMakeupDb, kDefaultMakeupDb, 0, 1, false };
    std::atomic<float> publishedGainReductionDb_ { 0.0f };
};

} // namespace yesdaw::engine
