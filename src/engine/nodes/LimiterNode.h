// YES DAW -- LimiterNode: stereo lookahead limiter for H14 CP8.
//
// Implements Appendix A5's single normative algorithm: raw target gain, release smoothing,
// sliding-window minimum over the lookahead horizon, boxcar attack smoothing, and delayed output.

#pragma once

#include "engine/Node.h"
#include "engine/ParamSpec.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace yesdaw::engine {

class LimiterNode final : public Node
{
public:
    static constexpr int kChannels = 2;

    static constexpr ParameterId kCeilingParamId = 0;
    static constexpr ParameterId kReleaseParamId = 1;
    static constexpr ParameterId kLookaheadParamId = 2;

    static constexpr double kMinCeilingDb = -12.0;
    static constexpr double kMaxCeilingDb = 0.0;
    static constexpr double kDefaultCeilingDb = -1.0;
    static constexpr double kMinReleaseMs = 50.0;
    static constexpr double kMaxReleaseMs = 1000.0;
    static constexpr double kDefaultReleaseMs = 100.0;
    static constexpr double kMinLookaheadMs = 1.0;
    static constexpr double kMaxLookaheadMs = 10.0;
    static constexpr double kDefaultLookaheadMs = 5.0;

    explicit LimiterNode (NodeId id = 0) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        const std::int64_t latency = lookaheadSamples();
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                kChannels, latency, id_, /*blockParallelSafe*/ false, latency };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double sampleRate, int /*maxBlockSize*/) override
    {
        sampleRate_ = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        maxLookaheadSamples_ = lookaheadSamplesForMs (sampleRate_, kMaxLookaheadMs);
        const std::int64_t ringFrames = maxLookaheadSamples_ + 2;

        audioDelay_.assign (static_cast<std::size_t> (ringFrames) * kChannels, 0.0f);
        gainDelay_.assign (static_cast<std::size_t> (ringFrames), 1.0);
        boxcar_.assign (static_cast<std::size_t> (maxLookaheadSamples_ + 1), 1.0);
        dequeFrames_.assign (static_cast<std::size_t> (maxLookaheadSamples_ + 2), 0);
        dequeValues_.assign (static_cast<std::size_t> (maxLookaheadSamples_ + 2), 1.0);

        ringFrames_ = ringFrames;
        rampLengthSamples_ = std::max<std::int64_t> (1, static_cast<std::int64_t> (std::llround (sampleRate_ * kRampSeconds)));
        prepared_ = true;
        runningFrame_ = 0;
        resetAlgorithmState (lookaheadSamples());
        publishedGainReductionDb_.store (0.0f, std::memory_order_release);
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (! prepared_ || ringFrames_ <= 0 || audioDelay_.empty())
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
            const std::int64_t d = activeLookaheadSamples_;
            const double ceilingLinear = ceilingLinearForDb (paramValueAtFrame (ceilingDb_, absoluteFrame, clampCeilingDb));
            const double releaseMs = paramValueAtFrame (releaseMs_, absoluteFrame, clampReleaseMs);

            const double inL = inputAt (args, 0, frame);
            const double inR = inputAt (args, 1, frame);
            writeInput (0, inL);
            writeInput (1, inR);

            const double peak = std::max (std::fabs (inL), std::fabs (inR));
            const double rawTarget = std::min (1.0, ceilingLinear / std::max (peak, kPeakFloor));
            const double alpha = releaseAlpha (releaseMs);
            targetRelease_ = std::clamp (std::min (rawTarget, targetRelease_ + alpha * (1.0 - targetRelease_)), 0.0, 1.0);

            pushTarget (absoluteFrame, targetRelease_);

            if (samplesSeen_ + 1 >= d)
            {
                const std::int64_t gainFrame = absoluteFrame - d + 1;
                pruneTargetWindow (gainFrame);
                const double minTarget = dequeCount_ > 0 ? dequeValues_[dequeHead_] : 1.0;
                const double gain = boxcarGain (gainFrame, minTarget);
                storeGain (gainFrame, gain);
            }

            const bool haveDelayedInput = samplesSeen_ >= d;
            const std::int64_t sourceFrame = absoluteFrame - d;
            const double gain = haveDelayedInput ? readGain (sourceFrame) : 0.0;
            if (haveDelayedInput)
                blockMaxGainReductionDb = std::max (blockMaxGainReductionDb, gainReductionDbForGain (gain));

            if (channels > 0 && args.audio.channels[0] != nullptr)
                args.audio.channels[0][frame] = finiteFloat (readDelayedInput (0, d) * gain);
            if (channels > 1 && args.audio.channels[1] != nullptr)
                args.audio.channels[1][frame] = finiteFloat (readDelayedInput (1, d) * gain);

            finishCompletedRamps (absoluteFrame);
            writePos_ = wrap (writePos_ + 1);
            ++samplesSeen_;
        }

        publishedGainReductionDb_.store (finiteFloat (blockMaxGainReductionDb), std::memory_order_release);
        advanceRunningFrame (args, frames);
    }

    void reset() noexcept override
    {
        resetAlgorithmState (lookaheadSamples());
        snapParam (ceilingDb_, ceilingDb_.target);
        snapParam (releaseMs_, releaseMs_.target);
        snapParam (lookaheadMs_, lookaheadMs_.target);
        runningFrame_ = 0;
        publishedGainReductionDb_.store (0.0f, std::memory_order_release);
    }

    void release() override
    {
        audioDelay_.clear();
        audioDelay_.shrink_to_fit();
        gainDelay_.clear();
        gainDelay_.shrink_to_fit();
        boxcar_.clear();
        boxcar_.shrink_to_fit();
        dequeFrames_.clear();
        dequeFrames_.shrink_to_fit();
        dequeValues_.clear();
        dequeValues_.shrink_to_fit();
        ringFrames_ = 0;
        prepared_ = false;
    }

    void setInput (Node* in) noexcept { input_ = in; }

    void setParameters (double ceilingDb, double releaseMs, double lookaheadMs) noexcept
    {
        snapParam (ceilingDb_, clampCeilingDb (ceilingDb));
        snapParam (releaseMs_, clampReleaseMs (releaseMs));
        snapParam (lookaheadMs_, clampLookaheadMs (lookaheadMs));
        if (prepared_)
            resetAlgorithmState (lookaheadSamples());
    }

    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        RampedParam* const param = paramForId (parameterId);
        if (param == nullptr)
            return;

        snapParam (*param, clampRealParameter (parameterId, mapNormalizedParameter (parameterId, normalizedValue)));
        if (parameterId == kLookaheadParamId && prepared_)
            resetAlgorithmState (lookaheadSamples());
    }

    [[nodiscard]] float gainReductionDb() const noexcept
    {
        return publishedGainReductionDb_.load (std::memory_order_acquire);
    }

    [[nodiscard]] std::int64_t lookaheadSamples() const noexcept
    {
        return lookaheadSamplesForMs (sampleRate_, lookaheadMs_.target);
    }

    [[nodiscard]] std::int64_t tailSamples() const noexcept
    {
        return lookaheadSamples();
    }

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kCeilingParamId:
                return ParamSpec { parameterId, "limiter.ceiling", "dBFS", kMinCeilingDb, kMaxCeilingDb,
                                   kDefaultCeilingDb, ParamMapping::Db, ParamSmoothing::Linear5Ms };
            case kReleaseParamId:
                return ParamSpec { parameterId, "limiter.release", "ms", kMinReleaseMs, kMaxReleaseMs,
                                   kDefaultReleaseMs, ParamMapping::Log, ParamSmoothing::Linear5Ms };
            case kLookaheadParamId:
                return ParamSpec { parameterId, "limiter.lookahead", "ms", kMinLookaheadMs, kMaxLookaheadMs,
                                   kDefaultLookaheadMs, ParamMapping::Log, ParamSmoothing::Linear5Ms };
            default:
                return {};
        }
    }

    [[nodiscard]] static double mapNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        return mapNormalized (parameterSpec (parameterId), normalizedValue);
    }

    [[nodiscard]] static std::int64_t lookaheadSamplesForMs (double sampleRate, double lookaheadMs) noexcept
    {
        const double sr = std::isfinite (sampleRate) && sampleRate > 0.0 ? sampleRate : 48000.0;
        const double ms = clampLookaheadMs (lookaheadMs);
        return std::max<std::int64_t> (1, static_cast<std::int64_t> (std::llround (ms * 0.001 * sr)));
    }

    [[nodiscard]] static double ceilingLinearForDb (double ceilingDb) noexcept
    {
        return std::pow (10.0, clampCeilingDb (ceilingDb) / 20.0);
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
    static constexpr double kPeakFloor = 1.0e-10;

    [[nodiscard]] static double finiteOrDefault (double value, double fallback) noexcept
    {
        return std::isfinite (value) ? value : fallback;
    }

    [[nodiscard]] static double clampCeilingDb (double ceilingDb) noexcept
    {
        return std::clamp (finiteOrDefault (ceilingDb, kDefaultCeilingDb), kMinCeilingDb, kMaxCeilingDb);
    }

    [[nodiscard]] static double clampReleaseMs (double releaseMs) noexcept
    {
        return std::clamp (finiteOrDefault (releaseMs, kDefaultReleaseMs), kMinReleaseMs, kMaxReleaseMs);
    }

    [[nodiscard]] static double clampLookaheadMs (double lookaheadMs) noexcept
    {
        return std::clamp (finiteOrDefault (lookaheadMs, kDefaultLookaheadMs), kMinLookaheadMs, kMaxLookaheadMs);
    }

    [[nodiscard]] static double clampRealParameter (ParameterId parameterId, double value) noexcept
    {
        switch (parameterId)
        {
            case kCeilingParamId:
                return clampCeilingDb (value);
            case kReleaseParamId:
                return clampReleaseMs (value);
            case kLookaheadParamId:
                return clampLookaheadMs (value);
            default:
                return 0.0;
        }
    }

    [[nodiscard]] RampedParam* paramForId (ParameterId parameterId) noexcept
    {
        switch (parameterId)
        {
            case kCeilingParamId:
                return &ceilingDb_;
            case kReleaseParamId:
                return &releaseMs_;
            case kLookaheadParamId:
                return &lookaheadMs_;
            default:
                return nullptr;
        }
    }

    static void snapParam (RampedParam& param, double value) noexcept
    {
        param.current = value;
        param.start = value;
        param.target = value;
        param.eventFrame = 0;
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

    static bool finishCompletedRamp (RampedParam& param, std::int64_t frame) noexcept
    {
        param.current = valueAtFrame (param, frame);
        if (param.active && frame >= param.eventFrame + param.duration)
        {
            snapParam (param, param.target);
            return true;
        }

        return false;
    }

    void finishCompletedRamps (std::int64_t frame) noexcept
    {
        finishCompletedRamp (ceilingDb_, frame);
        finishCompletedRamp (releaseMs_, frame);
        if (finishCompletedRamp (lookaheadMs_, frame))
            resetAlgorithmState (lookaheadSamples());
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

    void resetAlgorithmState (std::int64_t lookaheadSamples) noexcept
    {
        activeLookaheadSamples_ = std::clamp<std::int64_t> (lookaheadSamples, 1, std::max<std::int64_t> (1, maxLookaheadSamples_));
        if (! audioDelay_.empty())
            std::memset (audioDelay_.data(), 0, audioDelay_.size() * sizeof (float));
        std::fill (gainDelay_.begin(), gainDelay_.end(), 1.0);
        std::fill (boxcar_.begin(), boxcar_.end(), 1.0);
        std::fill (dequeValues_.begin(), dequeValues_.end(), 1.0);
        std::fill (dequeFrames_.begin(), dequeFrames_.end(), 0);
        writePos_ = 0;
        samplesSeen_ = 0;
        targetRelease_ = 1.0;
        dequeHead_ = 0;
        dequeCount_ = 0;
        boxcarWritePos_ = 0;
        boxcarSum_ = static_cast<double> (activeLookaheadSamples_);
        boxcarInitialized_ = false;
    }

    [[nodiscard]] double releaseAlpha (double releaseMs) const noexcept
    {
        const double tauSeconds = clampReleaseMs (releaseMs) * 0.001;
        return 1.0 - std::exp (-1.0 / (tauSeconds * sampleRate_));
    }

    void pushTarget (std::int64_t frame, double value) noexcept YESDAW_RT_HOT
    {
        const std::size_t capacity = dequeValues_.size();
        if (capacity == 0)
            return;

        while (dequeCount_ > 0)
        {
            const std::size_t back = (dequeHead_ + dequeCount_ - 1u) % capacity;
            if (dequeValues_[back] < value)
                break;
            --dequeCount_;
        }

        const std::size_t write = (dequeHead_ + dequeCount_) % capacity;
        dequeFrames_[write] = frame;
        dequeValues_[write] = value;
        if (dequeCount_ < capacity)
            ++dequeCount_;
        else
            dequeHead_ = (dequeHead_ + 1u) % capacity;
    }

    void pruneTargetWindow (std::int64_t firstFrameInWindow) noexcept YESDAW_RT_HOT
    {
        const std::size_t capacity = dequeValues_.size();
        while (dequeCount_ > 0 && dequeFrames_[dequeHead_] < firstFrameInWindow)
        {
            dequeHead_ = (dequeHead_ + 1u) % capacity;
            --dequeCount_;
        }
    }

    [[nodiscard]] double boxcarGain (std::int64_t gainFrame, double minTarget) noexcept YESDAW_RT_HOT
    {
        const std::int64_t d64 = activeLookaheadSamples_;
        const std::size_t d = static_cast<std::size_t> (d64);
        if (d == 0 || boxcar_.empty())
            return 1.0;

        if (! boxcarInitialized_)
        {
            for (std::size_t i = 0; i < d; ++i)
                boxcar_[i] = minTarget;
            boxcarSum_ = minTarget * static_cast<double> (d64);
            boxcarWritePos_ = 0;
            boxcarInitialized_ = true;
            return minTarget;
        }

        boxcarSum_ += minTarget - boxcar_[boxcarWritePos_];
        boxcar_[boxcarWritePos_] = minTarget;
        boxcarWritePos_ = (boxcarWritePos_ + 1u) % d;

        if (positiveModulo (gainFrame, kBoxcarRebuildPeriod) == 0)
            rebuildBoxcarSum (d);

        return std::clamp (boxcarSum_ / static_cast<double> (d64), 0.0, 1.0);
    }

    void rebuildBoxcarSum (std::size_t d) noexcept YESDAW_RT_HOT
    {
        double sum = 0.0;
        for (std::size_t i = 0; i < d; ++i)
            sum += boxcar_[i];
        boxcarSum_ = sum;
    }

    void writeInput (int channel, double sample) noexcept YESDAW_RT_HOT
    {
        audioDelay_[ringIndex (channel, writePos_)] = finiteFloat (sanitizeInput (sample));
    }

    [[nodiscard]] double readDelayedInput (int channel, std::int64_t delaySamples) const noexcept YESDAW_RT_HOT
    {
        return static_cast<double> (audioDelay_[ringIndex (channel, writePos_ - delaySamples)]);
    }

    void storeGain (std::int64_t sourceFrame, double gain) noexcept YESDAW_RT_HOT
    {
        gainDelay_[frameRingIndex (sourceFrame)] = std::clamp (finiteOrDefault (gain, 1.0), 0.0, 1.0);
    }

    [[nodiscard]] double readGain (std::int64_t sourceFrame) const noexcept YESDAW_RT_HOT
    {
        return gainDelay_[frameRingIndex (sourceFrame)];
    }

    [[nodiscard]] std::size_t ringIndex (int channel, std::int64_t frameIndex) const noexcept YESDAW_RT_HOT
    {
        return static_cast<std::size_t> (channel) * static_cast<std::size_t> (ringFrames_)
             + frameRingIndex (frameIndex);
    }

    [[nodiscard]] std::size_t frameRingIndex (std::int64_t frameIndex) const noexcept YESDAW_RT_HOT
    {
        return static_cast<std::size_t> (positiveModulo (frameIndex, ringFrames_));
    }

    [[nodiscard]] std::int64_t wrap (std::int64_t frameIndex) const noexcept YESDAW_RT_HOT
    {
        return positiveModulo (frameIndex, ringFrames_);
    }

    [[nodiscard]] static std::int64_t positiveModulo (std::int64_t value, std::int64_t modulus) noexcept
    {
        if (modulus <= 0)
            return 0;
        std::int64_t m = value % modulus;
        if (m < 0)
            m += modulus;
        return m;
    }

    [[nodiscard]] double inputAt (const ProcessArgs& args, int channel, int frame) const noexcept
    {
        if (channel >= args.audio.numChannels || args.audio.channels[channel] == nullptr)
            return 0.0;

        return sanitizeInput (static_cast<double> (args.audio.channels[channel][frame]));
    }

    [[nodiscard]] static double sanitizeInput (double sample) noexcept
    {
        return std::isfinite (sample) ? sample : 0.0;
    }

    [[nodiscard]] static double gainReductionDbForGain (double gain) noexcept
    {
        const double g = std::clamp (finiteOrDefault (gain, 1.0), kPeakFloor, 1.0);
        return -20.0 * std::log10 (g);
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

    static constexpr std::int64_t kBoxcarRebuildPeriod = 4096;

    NodeId id_;
    Node* input_ = nullptr;
    double sampleRate_ = 48000.0;
    std::int64_t maxLookaheadSamples_ = lookaheadSamplesForMs (48000.0, kMaxLookaheadMs);
    std::int64_t activeLookaheadSamples_ = lookaheadSamplesForMs (48000.0, kDefaultLookaheadMs);
    std::int64_t ringFrames_ = 0;
    std::int64_t writePos_ = 0;
    std::int64_t samplesSeen_ = 0;
    std::int64_t runningFrame_ = 0;
    std::int64_t rampLengthSamples_ = 240;
    double targetRelease_ = 1.0;
    double boxcarSum_ = static_cast<double> (lookaheadSamplesForMs (48000.0, kDefaultLookaheadMs));
    std::size_t dequeHead_ = 0;
    std::size_t dequeCount_ = 0;
    std::size_t boxcarWritePos_ = 0;
    bool boxcarInitialized_ = false;
    bool prepared_ = false;
    RampedParam ceilingDb_ { kDefaultCeilingDb, kDefaultCeilingDb, kDefaultCeilingDb, 0, 1, false };
    RampedParam releaseMs_ { kDefaultReleaseMs, kDefaultReleaseMs, kDefaultReleaseMs, 0, 1, false };
    RampedParam lookaheadMs_ { kDefaultLookaheadMs, kDefaultLookaheadMs, kDefaultLookaheadMs, 0, 1, false };
    std::vector<float> audioDelay_;
    std::vector<double> gainDelay_;
    std::vector<double> boxcar_;
    std::vector<std::int64_t> dequeFrames_;
    std::vector<double> dequeValues_;
    std::atomic<float> publishedGainReductionDb_ { 0.0f };
};

} // namespace yesdaw::engine
