// YES DAW - time model value types (ADR-0010).
//
// These are the storage-facing, JUCE-free types shared by Project round-trip, Transport, and the
// render boundary. They deliberately do not own memory: the audio thread may read them through a
// published Snapshot, but allocation and lifetime stay on the control side.

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace yesdaw::engine {

namespace detail {

constexpr bool isFinitePositive (double value) noexcept
{
    return value > 0.0 && value <= std::numeric_limits<double>::max();
}

constexpr bool scaleTick (std::int64_t value, std::int64_t factor, std::int64_t& out) noexcept
{
    if (factor <= 0)
        return false;

    if (value > 0 && value > std::numeric_limits<std::int64_t>::max() / factor)
        return false;

    if (value < 0 && value < std::numeric_limits<std::int64_t>::min() / factor)
        return false;

    out = value * factor;
    return true;
}

} // namespace detail

using Tick = std::int64_t;

constexpr Tick kTicksPerQuarter = 15360;

struct MusicalTime
{
    Tick   tick = 0;
    double frac = 0.0;   // render-only fractional tick, always [0, 1) when valid

    constexpr bool hasValidFraction() const noexcept
    {
        return frac >= 0.0 && frac < 1.0;
    }

    friend constexpr bool operator== (const MusicalTime&, const MusicalTime&) noexcept = default;
};

struct SnapGrid
{
    Tick intervalTicks = kTicksPerQuarter;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return intervalTicks > 0;
    }

    friend constexpr bool operator== (const SnapGrid&, const SnapGrid&) noexcept = default;
};

[[nodiscard]] constexpr bool snapTick (Tick tick, SnapGrid grid, Tick& snapped) noexcept
{
    if (! grid.isValid())
        return false;

    const Tick interval = grid.intervalTicks;
    Tick quotient = tick / interval;
    Tick remainder = tick % interval;

    if (remainder < 0)
    {
        --quotient;
        remainder += interval;
    }

    if (remainder >= interval - remainder)
    {
        if (quotient == std::numeric_limits<Tick>::max())
            return false;

        ++quotient;
    }

    return detail::scaleTick (quotient, interval, snapped);
}

[[nodiscard]] constexpr bool gridIndexForTick (Tick tick, SnapGrid grid, Tick& index) noexcept
{
    if (! grid.isValid() || tick % grid.intervalTicks != 0)
        return false;

    index = tick / grid.intervalTicks;
    return true;
}

[[nodiscard]] constexpr bool tickForGridIndex (Tick index, SnapGrid grid, Tick& tick) noexcept
{
    if (! grid.isValid())
        return false;

    return detail::scaleTick (index, grid.intervalTicks, tick);
}

enum class TimeBase : std::uint8_t
{
    TempoLocked = 0,
    SampleLocked = 1
};

enum class TempoCurve : std::uint8_t
{
    Jump = 0,
    LinearRamp = 1
};

struct TempoChange
{
    Tick       tick = 0;
    double     bpm = 120.0;
    TempoCurve curveToNext = TempoCurve::Jump;

    constexpr bool hasValidBpm() const noexcept
    {
        return detail::isFinitePositive (bpm);
    }

    friend constexpr bool operator== (const TempoChange&, const TempoChange&) noexcept = default;
};

struct MeterChange
{
    Tick          tick = 0;
    std::uint16_t numerator = 4;
    std::uint16_t denominator = 4;

    constexpr bool isValid() const noexcept
    {
        return numerator > 0 && denominator > 0;
    }

    friend constexpr bool operator== (const MeterChange&, const MeterChange&) noexcept = default;
};

struct SampleRate
{
    double hz = 48000.0;

    constexpr bool isValid() const noexcept
    {
        return detail::isFinitePositive (hz);
    }

    friend constexpr bool operator== (const SampleRate&, const SampleRate&) noexcept = default;
};

enum class ResampleQuality : std::uint8_t
{
    LivePlayback = 0,
    OfflineRender = 1
};

struct TempoMapView
{
    const TempoChange* changes = nullptr;
    std::size_t        count = 0;

    constexpr bool empty() const noexcept { return count == 0; }
};

struct MeterMapView
{
    const MeterChange* changes = nullptr;
    std::size_t        count = 0;

    constexpr bool empty() const noexcept { return count == 0; }
};

struct Transport
{
    MusicalTime playhead;
    TempoMapView tempoMap;
    MeterMapView meterMap;
    SampleRate   projectSampleRate;
    bool         isPlaying = false;
};

namespace detail {

[[nodiscard]] inline bool appendTempoSegmentFrames (double sampleRate,
                                                    Tick segmentTicks,
                                                    Tick deltaTicks,
                                                    double startBpm,
                                                    double endBpm,
                                                    TempoCurve curve,
                                                    double& frame) noexcept
{
    if (segmentTicks <= 0 || deltaTicks < 0 || deltaTicks > segmentTicks
        || ! isFinitePositive (startBpm) || ! isFinitePositive (endBpm))
        return false;

    if (deltaTicks == 0)
        return true;

    const double delta = static_cast<double> (deltaTicks);
    const double k = 60.0 * sampleRate / static_cast<double> (kTicksPerQuarter);

    if (curve != TempoCurve::LinearRamp || startBpm == endBpm)
    {
        frame += delta * k / startBpm;
        return std::isfinite (frame);
    }

    const double slope = (endBpm - startBpm) / static_cast<double> (segmentTicks);
    if (std::abs (slope) < 1.0e-12)
    {
        frame += delta * k / startBpm;
        return std::isfinite (frame);
    }

    const double bpmAtDelta = startBpm + slope * delta;
    if (! isFinitePositive (bpmAtDelta))
        return false;

    frame += (k / slope) * std::log (bpmAtDelta / startBpm);
    return std::isfinite (frame);
}

} // namespace detail

[[nodiscard]] inline bool tickToFrame (TempoMapView tempoMap,
                                       SampleRate sampleRate,
                                       Tick tick,
                                       double& frameOut) noexcept
{
    frameOut = 0.0;

    if (! sampleRate.isValid() || tick < 0)
        return false;

    if (tempoMap.empty())
    {
        frameOut = static_cast<double> (tick)
                 * (60.0 * sampleRate.hz / (120.0 * static_cast<double> (kTicksPerQuarter)));
        return std::isfinite (frameOut);
    }

    if (tempoMap.changes == nullptr || tempoMap.changes[0].tick != 0
        || ! tempoMap.changes[0].hasValidBpm())
        return false;

    for (std::size_t i = 1; i < tempoMap.count; ++i)
    {
        if (tempoMap.changes[i].tick <= tempoMap.changes[i - 1u].tick
            || ! tempoMap.changes[i].hasValidBpm())
            return false;
    }

    double frame = 0.0;
    for (std::size_t i = 0; i < tempoMap.count; ++i)
    {
        const TempoChange& current = tempoMap.changes[i];
        const bool haveNext = i + 1u < tempoMap.count;
        const Tick nextTick = haveNext ? tempoMap.changes[i + 1u].tick : tick;
        const Tick segmentEnd = tick < nextTick ? tick : nextTick;

        if (segmentEnd > current.tick)
        {
            const double endBpm = haveNext ? tempoMap.changes[i + 1u].bpm : current.bpm;
            if (! detail::appendTempoSegmentFrames (sampleRate.hz,
                                                    nextTick - current.tick,
                                                    segmentEnd - current.tick,
                                                    current.bpm,
                                                    endBpm,
                                                    current.curveToNext,
                                                    frame))
                return false;
        }

        if (tick <= nextTick || ! haveNext)
        {
            frameOut = frame;
            return std::isfinite (frameOut);
        }
    }

    return false;
}

} // namespace yesdaw::engine
