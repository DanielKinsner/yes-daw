// YES DAW -- EqNode: six-band built-in EQ for H14 CP4.
//
// Stereo, in-place, TPT SVF per Appendix A1 of the H14 FX-suite plan. Parameter events carry
// normalized values; the node maps them to real values, clamps finitely, and ramps real-valued
// frequency/gain/Q changes from the event's absolute frame.

#pragma once

#include "engine/Node.h"
#include "engine/ParamSpec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace yesdaw::engine {

class EqNode final : public Node
{
public:
    enum class BandType : std::uint8_t
    {
        Bell = 0,
        LowShelf,
        HighShelf,
        Hpf,
        Lpf,
        Notch
    };

    static constexpr int kBands = 6;
    static constexpr int kChannels = 2;
    static constexpr ParameterId kParamsPerBand = 16;
    static constexpr ParameterId kTypeParamOffset = 0;
    static constexpr ParameterId kFrequencyParamOffset = 1;
    static constexpr ParameterId kGainParamOffset = 2;
    static constexpr ParameterId kQParamOffset = 3;
    static constexpr double kDefaultFrequencyHz = 1000.0;
    static constexpr double kDefaultGainDb = 0.0;
    static constexpr double kDefaultQ = 1.0;

    explicit EqNode (NodeId id = 0) noexcept : id_ (id) {}

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
        prepared_ = true;

        for (int band = 0; band < kBands; ++band)
            recomputeBand (band, 0);
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (! prepared_)
            return;

        const int frames = args.numFrames > 0 ? args.numFrames : 0;
        const int channels = std::min (args.audio.numChannels, kChannels);
        if (channels <= 0 || args.audio.channels == nullptr)
        {
            advanceRunningFrame (args, frames);
            return;
        }

        const std::int64_t blockStart = absoluteBlockStart (args);
        std::size_t eventIndex = 0;
        const std::span<const Event> events = args.events.events();

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
            refreshRampingBands (absoluteFrame);

            for (int channel = 0; channel < channels; ++channel)
            {
                float* const channelData = args.audio.channels[channel];
                if (channelData == nullptr)
                    continue;

                double sample = static_cast<double> (channelData[frame]);
                for (int band = 0; band < kBands; ++band)
                    sample = processBandSample (bands_[band], states_[band][channel], sample);

                channelData[frame] = static_cast<float> (sample);
            }
        }

        advanceRunningFrame (args, frames);
    }

    void reset() noexcept override
    {
        for (auto& channelStates : states_)
            for (BandState& state : channelStates)
                state = BandState {};

        for (Band& band : bands_)
        {
            snapParam (band.frequency, band.frequency.target);
            snapParam (band.gainDb, band.gainDb.target);
            snapParam (band.q, band.q.target);
            band.nextRecomputeFrame = 0;
        }

        for (int band = 0; band < kBands; ++band)
            recomputeBand (band, runningFrame_);
    }

    void release() override {}

    void setInput (Node* in) noexcept { input_ = in; }

    void setBand (int band, BandType type, double frequencyHz, double gainDb, double q) noexcept
    {
        if (! isValidBand (band))
            return;

        Band& b = bands_[band];
        b.type = type;
        snapParam (b.frequency, clampFrequency (frequencyHz));
        snapParam (b.gainDb, clampGainDb (gainDb));
        snapParam (b.q, clampQ (q));
        recomputeBand (band, runningFrame_);
    }

    void setNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        int band = 0;
        ParameterId offset = 0;
        if (! decodeParameterId (parameterId, band, offset))
            return;

        Band& b = bands_[band];
        if (offset == kTypeParamOffset)
        {
            b.type = typeFromNormalized (normalizedValue);
            recomputeBand (band, runningFrame_);
            return;
        }

        const double real = mapNormalizedParameter (parameterId, normalizedValue);
        if (offset == kFrequencyParamOffset)
            snapParam (b.frequency, clampFrequency (real));
        else if (offset == kGainParamOffset)
            snapParam (b.gainDb, clampGainDb (real));
        else if (offset == kQParamOffset)
            snapParam (b.q, clampQ (real));

        recomputeBand (band, runningFrame_);
    }

    [[nodiscard]] static constexpr ParameterId parameterIdFor (int band, ParameterId offset) noexcept
    {
        return static_cast<ParameterId> (band) * kParamsPerBand + offset;
    }

    [[nodiscard]] static ParamSpec parameterSpec (ParameterId parameterId) noexcept
    {
        int band = 0;
        ParameterId offset = 0;
        if (! decodeParameterId (parameterId, band, offset))
            return {};
        (void) band;

        switch (offset)
        {
            case kTypeParamOffset:
                return ParamSpec { parameterId, "eq.band.type", "", 0.0, 5.0, 0.0,
                                   ParamMapping::Linear, ParamSmoothing::None };
            case kFrequencyParamOffset:
                return ParamSpec { parameterId, "eq.band.freq", "Hz", 20.0, 20000.0, kDefaultFrequencyHz,
                                   ParamMapping::Log, ParamSmoothing::Linear5Ms };
            case kGainParamOffset:
                return ParamSpec { parameterId, "eq.band.gain", "dB", -24.0, 24.0, kDefaultGainDb,
                                   ParamMapping::Db, ParamSmoothing::Linear5Ms };
            case kQParamOffset:
                return ParamSpec { parameterId, "eq.band.q", "", 0.1, 18.0, kDefaultQ,
                                   ParamMapping::Log, ParamSmoothing::Linear5Ms };
            default:
                return {};
        }
    }

    [[nodiscard]] static double mapNormalizedParameter (ParameterId parameterId, double normalizedValue) noexcept
    {
        int band = 0;
        ParameterId offset = 0;
        if (! decodeParameterId (parameterId, band, offset))
            return 0.0;
        (void) band;

        if (offset == kTypeParamOffset)
            return static_cast<double> (typeIndexFromNormalized (normalizedValue));

        return mapNormalized (parameterSpec (parameterId), normalizedValue);
    }

private:
    struct Coefficients
    {
        double a1 = 1.0;
        double a2 = 0.0;
        double a3 = 0.0;
        double m0 = 1.0;
        double m1 = 0.0;
        double m2 = 0.0;
    };

    struct BandState
    {
        double ic1eq = 0.0;
        double ic2eq = 0.0;
    };

    struct RampedParam
    {
        double current = 0.0;
        double start = 0.0;
        double target = 0.0;
        std::int64_t eventFrame = 0;
        std::int64_t duration = 1;
        bool active = false;
    };

    struct Band
    {
        BandType type = BandType::Bell;
        RampedParam frequency { kDefaultFrequencyHz, kDefaultFrequencyHz, kDefaultFrequencyHz, 0, 1, false };
        RampedParam gainDb { kDefaultGainDb, kDefaultGainDb, kDefaultGainDb, 0, 1, false };
        RampedParam q { kDefaultQ, kDefaultQ, kDefaultQ, 0, 1, false };
        Coefficients coeffs;
        std::int64_t nextRecomputeFrame = 0;
    };

    static constexpr double kRampSeconds = 0.005;
    static constexpr double kPi = 3.141592653589793238462643383279502884;
    static constexpr int kCoefficientCadenceSamples = 16;

    [[nodiscard]] static constexpr bool isValidBand (int band) noexcept
    {
        return band >= 0 && band < kBands;
    }

    [[nodiscard]] static bool decodeParameterId (ParameterId parameterId, int& band, ParameterId& offset) noexcept
    {
        band = static_cast<int> (parameterId / kParamsPerBand);
        offset = parameterId % kParamsPerBand;
        return isValidBand (band)
               && (offset == kTypeParamOffset || offset == kFrequencyParamOffset
                   || offset == kGainParamOffset || offset == kQParamOffset);
    }

    [[nodiscard]] static int typeIndexFromNormalized (double normalizedValue) noexcept
    {
        if (! std::isfinite (normalizedValue))
            return 0;

        const double clamped = std::clamp (normalizedValue, 0.0, 1.0);
        return static_cast<int> (std::clamp (std::floor (clamped * 5.0 + 0.5), 0.0, 5.0));
    }

    [[nodiscard]] static BandType typeFromNormalized (double normalizedValue) noexcept
    {
        return static_cast<BandType> (typeIndexFromNormalized (normalizedValue));
    }

    [[nodiscard]] static double finiteOrDefault (double value, double fallback) noexcept
    {
        return std::isfinite (value) ? value : fallback;
    }

    [[nodiscard]] double clampFrequency (double frequencyHz) const noexcept
    {
        const double nyquistSafe = std::min (20000.0, 0.49 * sampleRate_);
        const double maxHz = nyquistSafe > 20.0 ? nyquistSafe : 20.0;
        return std::clamp (finiteOrDefault (frequencyHz, kDefaultFrequencyHz), 20.0, maxHz);
    }

    [[nodiscard]] static double clampGainDb (double gainDb) noexcept
    {
        return std::clamp (finiteOrDefault (gainDb, kDefaultGainDb), -24.0, 24.0);
    }

    [[nodiscard]] static double clampQ (double q) noexcept
    {
        return std::clamp (finiteOrDefault (q, kDefaultQ), 0.1, 18.0);
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

    [[nodiscard]] static bool paramIsActiveAt (const RampedParam& param, std::int64_t frame) noexcept
    {
        return param.active && frame < param.eventFrame + param.duration;
    }

    [[nodiscard]] static bool bandHasActiveRampAt (const Band& band, std::int64_t frame) noexcept
    {
        return paramIsActiveAt (band.frequency, frame)
               || paramIsActiveAt (band.gainDb, frame)
               || paramIsActiveAt (band.q, frame);
    }

    void consumeEvent (const Event& event, std::int64_t absoluteFrame) noexcept YESDAW_RT_HOT
    {
        if (event.type != EventType::ParameterChange
            || event.payload.parameter.targetNode != id_)
            return;

        int band = 0;
        ParameterId offset = 0;
        if (! decodeParameterId (event.payload.parameter.parameterId, band, offset))
            return;

        Band& b = bands_[band];
        if (offset == kTypeParamOffset)
        {
            b.type = typeFromNormalized (event.payload.parameter.normalizedValue);
            b.nextRecomputeFrame = absoluteFrame;
            recomputeBand (band, absoluteFrame);
            return;
        }

        const double real = mapNormalizedParameter (event.payload.parameter.parameterId,
                                                    event.payload.parameter.normalizedValue);
        if (offset == kFrequencyParamOffset)
            startRamp (b.frequency, clampFrequency (real), absoluteFrame);
        else if (offset == kGainParamOffset)
            startRamp (b.gainDb, clampGainDb (real), absoluteFrame);
        else if (offset == kQParamOffset)
            startRamp (b.q, clampQ (real), absoluteFrame);

        b.nextRecomputeFrame = absoluteFrame;
    }

    void refreshRampingBands (std::int64_t absoluteFrame) noexcept YESDAW_RT_HOT
    {
        for (int band = 0; band < kBands; ++band)
        {
            Band& b = bands_[band];
            const bool anyActive = bandHasActiveRampAt (b, absoluteFrame);
            const bool anyCompleting = (b.frequency.active && absoluteFrame >= b.frequency.eventFrame + b.frequency.duration)
                                    || (b.gainDb.active && absoluteFrame >= b.gainDb.eventFrame + b.gainDb.duration)
                                    || (b.q.active && absoluteFrame >= b.q.eventFrame + b.q.duration);
            if (! anyActive && ! anyCompleting)
                continue;

            if (absoluteFrame >= b.nextRecomputeFrame || anyCompleting)
            {
                recomputeBand (band, absoluteFrame);
                b.nextRecomputeFrame = absoluteFrame + kCoefficientCadenceSamples;
            }
        }
    }

    void finishCompletedRamp (RampedParam& param, std::int64_t frame) noexcept
    {
        param.current = valueAtFrame (param, frame);
        if (param.active && frame >= param.eventFrame + param.duration)
            snapParam (param, param.target);
    }

    void recomputeBand (int bandIndex, std::int64_t frame) noexcept YESDAW_RT_HOT
    {
        Band& b = bands_[bandIndex];
        finishCompletedRamp (b.frequency, frame);
        finishCompletedRamp (b.gainDb, frame);
        finishCompletedRamp (b.q, frame);

        const double f = clampFrequency (b.frequency.current);
        const double q = clampQ (b.q.current);
        const double db = clampGainDb (b.gainDb.current);
        const double a = std::pow (10.0, db / 40.0);
        const double a2 = a * a;

        double g = std::tan (kPi * f / sampleRate_);
        double k = 1.0 / q;
        double m0 = 1.0;
        double m1 = 0.0;
        double m2 = 0.0;

        switch (b.type)
        {
            case BandType::Bell:
                k = 1.0 / (q * a);
                m0 = 1.0;
                m1 = k * (a2 - 1.0);
                m2 = 0.0;
                break;

            case BandType::LowShelf:
                g /= std::sqrt (a);
                k = 1.0 / q;
                m0 = 1.0;
                m1 = k * (a - 1.0);
                m2 = a2 - 1.0;
                break;

            case BandType::HighShelf:
                g *= std::sqrt (a);
                k = 1.0 / q;
                m0 = a2;
                m1 = k * (1.0 - a) * a;
                m2 = 1.0 - a2;
                break;

            case BandType::Hpf:
                k = 1.0 / q;
                m0 = 1.0;
                m1 = -k;
                m2 = -1.0;
                break;

            case BandType::Lpf:
                k = 1.0 / q;
                m0 = 0.0;
                m1 = 0.0;
                m2 = 1.0;
                break;

            case BandType::Notch:
                k = 1.0 / q;
                m0 = 1.0;
                m1 = -k;
                m2 = 0.0;
                break;
        }

        const double aa1 = 1.0 / (1.0 + g * (g + k));
        b.coeffs = Coefficients { aa1, g * aa1, g * g * aa1, m0, m1, m2 };
    }

    [[nodiscard]] static double processBandSample (const Band& band, BandState& state, double v0) noexcept YESDAW_RT_HOT
    {
        const Coefficients& c = band.coeffs;
        const double v3 = v0 - state.ic2eq;
        const double v1 = c.a1 * state.ic1eq + c.a2 * v3;
        const double v2 = state.ic2eq + c.a2 * state.ic1eq + c.a3 * v3;
        state.ic1eq = 2.0 * v1 - state.ic1eq;
        state.ic2eq = 2.0 * v2 - state.ic2eq;
        return c.m0 * v0 + c.m1 * v1 + c.m2 * v2;
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
    std::array<Band, kBands> bands_;
    std::array<std::array<BandState, kChannels>, kBands> states_;
};

} // namespace yesdaw::engine
