// YES DAW - time model value types (ADR-0010).
//
// These are the storage-facing, JUCE-free types shared by Project round-trip, Transport, and the
// render boundary. They deliberately do not own memory: the audio thread may read them through a
// published Snapshot, but allocation and lifetime stay on the control side.

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

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

enum class TimeLookupStatus : std::uint8_t
{
    Ok,
    InvalidInput,
    InvalidMap,
    Overflow,
    NumericDomain
};

enum class TimeRounding : std::uint8_t { Floor, Nearest, Ceil };

// Exact render/control-side boundary: whole + numerator/denominator. Fractions are proper,
// not necessarily reduced. This introduces no stored fractional coordinates.
struct RationalTick
{
    Tick          whole = 0;
    std::uint32_t numerator = 0;
    std::uint32_t denominator = 1;

    friend constexpr bool operator== (const RationalTick&, const RationalTick&) noexcept = default;
};

// Timeline positions are nonnegative. Nearest ties go to the later Tick. Inputs are values so
// the output may alias their original whole member; failure always resets the output.
[[nodiscard]] constexpr TimeLookupStatus roundMusicalTime (MusicalTime time,
                                                           TimeRounding rounding,
                                                           Tick& out) noexcept
{
    out = 0;
    if (time.tick < 0 || ! time.hasValidFraction())
        return TimeLookupStatus::InvalidInput;
    bool increment = false;
    switch (rounding)
    {
        case TimeRounding::Floor: break;
        case TimeRounding::Nearest: increment = time.frac >= 0.5; break;
        case TimeRounding::Ceil: increment = time.frac > 0.0; break;
        default: return TimeLookupStatus::InvalidInput;
    }
    if (increment && time.tick == std::numeric_limits<Tick>::max())
        return TimeLookupStatus::Overflow;
    out = time.tick + (increment ? 1 : 0);
    return TimeLookupStatus::Ok;
}

[[nodiscard]] constexpr TimeLookupStatus roundRationalTick (RationalTick time,
                                                           TimeRounding rounding,
                                                           Tick& out) noexcept
{
    out = 0;
    if (time.whole < 0 || time.denominator == 0 || time.numerator >= time.denominator)
        return TimeLookupStatus::InvalidInput;
    bool increment = false;
    switch (rounding)
    {
        case TimeRounding::Floor: break;
        case TimeRounding::Nearest: increment = time.numerator >= time.denominator - time.numerator; break;
        case TimeRounding::Ceil: increment = time.numerator != 0; break;
        default: return TimeLookupStatus::InvalidInput;
    }
    if (increment && time.whole == std::numeric_limits<Tick>::max())
        return TimeLookupStatus::Overflow;
    out = time.whole + (increment ? 1 : 0);
    return TimeLookupStatus::Ok;
}

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

// G3.4 — Quantize v2 (Logic's region inspector: Quantize, Q-Strength, Q-Swing, Q-Length; humanize).
// The settings are plain percentages so a gate can state each law in closed form:
//   strength   how far a note travels toward its target (100 = onto it, 50 = halfway);
//   swing      every ODD grid slot lands this much of the interval late (0 = straight; 50 = the
//              classic maximum; capped at kQuantizeSwingMaxPercent — Logic's 50..75 % scale maps
//              to this 0..50 % one);
//   noteEnds   the note's END quantizes to the straight grid too (Logic's Q-Length off / on);
//   humanize   a deterministic offset within ± this much of the interval, drawn from the seed and
//              the note's id (the same seed reproduces the same feel; 0 = none).
inline constexpr std::uint8_t kQuantizeSwingMaxPercent = 66;

struct QuantizeSettings
{
    SnapGrid      grid;
    std::uint8_t  strengthPercent = 100;
    std::uint8_t  swingPercent = 0;
    bool          noteEnds = false;
    std::uint8_t  humanizePercent = 0;
    std::uint32_t humanizeSeed = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept
    {
        return grid.isValid()
            && strengthPercent <= 100
            && swingPercent <= kQuantizeSwingMaxPercent
            && humanizePercent <= 100;
    }

    // The plain grid quantize every earlier verb meant: onto the grid, straight, no humanize.
    [[nodiscard]] constexpr bool isPlainGrid() const noexcept
    {
        return strengthPercent == 100 && swingPercent == 0 && ! noteEnds && humanizePercent == 0;
    }

    friend constexpr bool operator== (const QuantizeSettings&, const QuantizeSettings&) noexcept = default;
};

// The swung grid: slot k sits at k * interval, plus swing % of the interval when k is odd.
[[nodiscard]] constexpr bool quantizeSlotTick (Tick slot, SnapGrid grid, std::uint8_t swingPercent, Tick& tick) noexcept
{
    Tick base = 0;
    if (! detail::scaleTick (slot, grid.intervalTicks, base))
        return false;
    const Tick late = (slot % 2 != 0) ? (grid.intervalTicks * static_cast<Tick> (swingPercent)) / 100 : 0;
    if (late > 0 && base > std::numeric_limits<Tick>::max() - late)
        return false;
    tick = base + late;
    return true;
}

// The nearest swung slot to a tick (a tie goes to the later slot, the snapTick law). With swing 0
// this is exactly snapTick.
[[nodiscard]] constexpr bool quantizeTargetTick (Tick tick, const QuantizeSettings& settings, Tick& target) noexcept
{
    if (! settings.isValid() || tick < 0)
        return false;
    if (settings.swingPercent == 0)
        return snapTick (tick, settings.grid, target);

    const Tick slot0 = tick / settings.grid.intervalTicks;
    bool found = false;
    Tick bestDistance = 0;
    for (Tick slot = slot0 > 0 ? slot0 - 1 : 0; slot <= slot0 + 2; ++slot)
    {
        Tick candidate = 0;
        if (! quantizeSlotTick (slot, settings.grid, settings.swingPercent, candidate))
            break;
        const Tick distance = candidate >= tick ? candidate - tick : tick - candidate;
        if (! found || distance < bestDistance || (distance == bestDistance && candidate > target))
        {
            found = true;
            bestDistance = distance;
            target = candidate;
        }
    }
    return found;
}

// A note moves strength % of the way from where it is to its target (rounded half away from zero).
[[nodiscard]] constexpr Tick quantizeTowards (Tick from, Tick target, std::uint8_t strengthPercent) noexcept
{
    const Tick delta = target - from;
    const Tick scaled = delta * static_cast<Tick> (strengthPercent);
    const Tick moved = scaled >= 0 ? (scaled + 50) / 100 : -((-scaled + 50) / 100);
    return from + moved;
}

// Humanize: a deterministic offset in [-range, +range] where range = humanize % of the interval,
// from an FNV-1a hash of the seed and the note's id bytes. 0 % is exactly 0.
[[nodiscard]] constexpr Tick quantizeHumanizeOffset (const QuantizeSettings& settings,
                                                     const std::uint8_t* idBytes,
                                                     std::size_t idByteCount) noexcept
{
    const Tick range = (settings.grid.intervalTicks * static_cast<Tick> (settings.humanizePercent)) / 100;
    if (range <= 0)
        return 0;
    std::uint32_t h = 2166136261u;
    for (int shift = 0; shift < 32; shift += 8)
    {
        h ^= static_cast<std::uint32_t> ((settings.humanizeSeed >> shift) & 0xFFu);
        h *= 16777619u;
    }
    for (std::size_t i = 0; i < idByteCount; ++i)
    {
        h ^= static_cast<std::uint32_t> (idBytes[i]);
        h *= 16777619u;
    }
    const Tick span = 2 * range + 1;
    return static_cast<Tick> (h % static_cast<std::uint32_t> (span)) - range;
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
    std::int64_t timelineFrame = 0;
    bool         isPlaying = false;
    bool         hasTimelineFrame = false;
    bool         clipsSilenced = false;   // G3.2: the transport is stopped and the graph runs only to carry live
                                          // notes - clip sources emit nothing and hold their cursors.
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
    if (slope == 0.0 || ! std::isfinite (slope))
        return false;

    const double relativeChange = slope * delta / startBpm;
    if (! std::isfinite (relativeChange) || relativeChange <= -1.0)
        return false;

    frame += (k / slope) * std::log1p (relativeChange);
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

// V2/V4: a 1-based bar and beat-within-bar position.
struct BarBeat
{
    std::int64_t bar = 1;
    std::int64_t beat = 1;
};

namespace detail {

// Normalize a*b/c before multiplication, retaining finite tiny and huge intermediate ratios.
// A nonzero result collapsing to zero is a named domain failure rather than a constant clock.
[[nodiscard]] inline bool scaledTimeRatio (double a, double b, double c, double& out) noexcept
{
    out = 0.0;
    if (! std::isfinite (a) || ! std::isfinite (b) || ! std::isfinite (c) || c == 0.0)
        return false;
    if (a == 0.0 || b == 0.0)
        return true;
    int aPower = 0, bPower = 0, cPower = 0;
    const double aMantissa = std::frexp (a, &aPower);
    const double bMantissa = std::frexp (b, &bPower);
    const double cMantissa = std::frexp (c, &cPower);
    out = std::ldexp ((aMantissa * bMantissa) / cMantissa, aPower + bPower - cPower);
    return std::isfinite (out) && out != 0.0;
}

[[nodiscard]] inline TimeLookupStatus checkedNativeFrame (double frame, double& out) noexcept
{
    if (! std::isfinite (frame) || frame < 0.0)
        return TimeLookupStatus::NumericDomain;
    if (frame >= 0x1p63)
        return TimeLookupStatus::Overflow;
    out = frame;
    return TimeLookupStatus::Ok;
}

} // namespace detail

// V4: the bar/beat lengths at a SINGLE tempo/meter, in frames at the given sample rate. This is
// the shared sub-law under computeBarBeat (the V2 transport readout's fallback) and the ruler's
// fallback bar labels (V4) — both derive their grid from this exact float-operation order, so the
// two fallbacks never disagree; with a compiled tempo map both read the full maps instead (G2.15,
// UI correctness pass unit 6). Passing sampleRateHz = 1.0 yields the lengths in SECONDS (frames at
// 1 Hz are seconds), which is how the seconds-based ruler consumes it.
struct BarGrid
{
    double barFrames = 0.0;
    double beatFrames = 0.0;
};

[[nodiscard]] inline BarGrid computeBarGrid (double bpm,
                                             std::uint16_t numerator,
                                             std::uint16_t denominator,
                                             double sampleRateHz) noexcept
{
    const double clampedBpm = std::clamp (bpm, 20.0, 400.0);
    const double clampedNumerator = std::clamp (static_cast<double> (numerator), 1.0, 32.0);
    const double clampedDenominator = std::clamp (static_cast<double> (denominator), 1.0, 64.0);
    const double quarterNoteFrames = sampleRateHz * 60.0 / clampedBpm;
    const double beatFrames = quarterNoteFrames * 4.0 / clampedDenominator;
    return { beatFrames * clampedNumerator, beatFrames };
}

// V2/V4: bar|beat at a frame position, for a SINGLE tempo/meter (the project's head values) — the
// same scope the headBarFrames() family (UiAppModel.h) commits to. The full tempo and meter maps
// have their own law below (G2.15 computeBarBeatPiecewise over barBeatAtTick, and its inverse
// barStartTick); this one is the fallback when no compiled map is at hand. Clamp ranges mirror
// headQuarterNoteFrames/headMeterBeatFrames/headBarFramesExact exactly, so a bar|beat computed
// here always agrees with those existing bar-length helpers.
[[nodiscard]] inline BarBeat computeBarBeat (double bpm,
                                             std::uint16_t numerator,
                                             std::uint16_t denominator,
                                             double sampleRateHz,
                                             std::int64_t playheadFrame) noexcept
{
    const double clampedNumerator = std::clamp (static_cast<double> (numerator), 1.0, 32.0);
    const BarGrid grid = computeBarGrid (bpm, numerator, denominator, sampleRateHz);
    const double beatFrames = grid.beatFrames;
    const double barFrames = grid.barFrames;
    const double frame = static_cast<double> (std::max<std::int64_t> (0, playheadFrame));

    const std::int64_t barIndex = barFrames > 0.0
                                      ? static_cast<std::int64_t> (std::floor (frame / barFrames))
                                      : 0;
    const double framesIntoBar = frame - static_cast<double> (barIndex) * barFrames;
    const std::int64_t beatIndex = beatFrames > 0.0
                                       ? static_cast<std::int64_t> (std::floor (framesIntoBar / beatFrames))
                                       : 0;

    BarBeat result;
    result.bar = barIndex + 1;
    result.beat = std::clamp<std::int64_t> (beatIndex + 1, 1, static_cast<std::int64_t> (clampedNumerator));
    return result;
}

// ADR-0010's mandated tempo lookup: validate the map and accumulate each segment's cumulative start frame
// ONCE on the control side, then resolve any tick to a frame in O(log n) via binary search — never the
// per-call O(n) scan + revalidation the free `tickToFrame` does. `frameForTick` is bit-identical to
// `tickToFrame` by construction (same closed-form per-segment math via appendTempoSegmentFrames, same
// accumulation order), proven by the bit-identity gate in tests/time_tests.cpp. This is a derived,
// control-side cache (ADR-0010 "if a derived-sample cache is kept for performance"); the audio thread
// reads frames already produced, it does not build this.
class CompiledTempoMap
{
public:
    [[nodiscard]] static bool build (TempoMapView tempoMap, SampleRate sampleRate, CompiledTempoMap& out)
    {
        out = CompiledTempoMap {};

        if (! sampleRate.isValid())
            return false;

        out.sampleRate_ = sampleRate;

        if (tempoMap.empty())
        {
            out.empty_ = true;
            out.checkedBuilt_ = true;
            out.checkedFailure_ = TimeLookupStatus::Ok;
            return true;
        }

        if (tempoMap.changes == nullptr || tempoMap.changes[0].tick != 0 || ! tempoMap.changes[0].hasValidBpm())
            return false;

        for (std::size_t i = 1; i < tempoMap.count; ++i)
            if (tempoMap.changes[i].tick <= tempoMap.changes[i - 1u].tick || ! tempoMap.changes[i].hasValidBpm())
                return false;

        out.empty_ = false;
        out.changes_.assign (tempoMap.changes, tempoMap.changes + tempoMap.count);
        out.startFrame_.assign (tempoMap.count, 0.0);

        for (std::size_t i = 1; i < out.changes_.size(); ++i)
        {
            double frame = out.startFrame_[i - 1u];
            const Tick segmentTicks = out.changes_[i].tick - out.changes_[i - 1u].tick;
            if (! detail::appendTempoSegmentFrames (sampleRate.hz,
                                                    segmentTicks,
                                                    segmentTicks,
                                                    out.changes_[i - 1u].bpm,
                                                    out.changes_[i].bpm,
                                                    out.changes_[i - 1u].curveToNext,
                                                    frame))
            {
                out.checkedFailure_ = TimeLookupStatus::NumericDomain;
                return false;
            }

            out.startFrame_[i] = frame;
        }

        out.checkedBuilt_ = true;
        out.checkedFailure_ = TimeLookupStatus::Ok;
        return true;
    }

    [[nodiscard]] bool frameForTick (Tick tick, double& frameOut) const noexcept
    {
        frameOut = 0.0;

        if (! sampleRate_.isValid() || tick < 0)
            return false;

        if (empty_)
        {
            frameOut = static_cast<double> (tick)
                     * (60.0 * sampleRate_.hz / (120.0 * static_cast<double> (kTicksPerQuarter)));
            return std::isfinite (frameOut);
        }

        // Last segment whose start tick <= tick (changes_[0].tick == 0 <= tick, so the index is >= 1).
        const std::size_t upper = static_cast<std::size_t> (
            std::upper_bound (changes_.begin(), changes_.end(), tick,
                              [] (Tick value, const TempoChange& change) noexcept { return value < change.tick; })
            - changes_.begin());
        const std::size_t i = upper - 1u;

        double frame = startFrame_[i];
        if (tick > changes_[i].tick)
        {
            const bool   haveNext = i + 1u < changes_.size();
            const Tick   nextTick = haveNext ? changes_[i + 1u].tick : tick;
            const double endBpm   = haveNext ? changes_[i + 1u].bpm : changes_[i].bpm;
            if (! detail::appendTempoSegmentFrames (sampleRate_.hz,
                                                    nextTick - changes_[i].tick,
                                                    tick - changes_[i].tick,
                                                    changes_[i].bpm,
                                                    endBpm,
                                                    changes_[i].curveToNext,
                                                    frame))
                return false;
        }

        frameOut = frame;
        return std::isfinite (frameOut);
    }

    // G2.15: the inverse — the tick at a frame, per segment: a jump inverts the linear law, a ramp
    // inverts the logarithm (delta = (startBpm/slope)·expm1(frame·slope/k)), retaining small relative
    // changes without cancellation. Rounded to the nearest tick; frameForTick (tickForFrame (f)) ≈ f.
    [[nodiscard]] bool tickForFrame (double frame, Tick& tickOut) const noexcept
    {
        tickOut = 0;
        if (! sampleRate_.isValid() || ! std::isfinite (frame) || frame < 0.0)
            return false;
        const double k = 60.0 * sampleRate_.hz / static_cast<double> (kTicksPerQuarter);
        if (empty_)
        {
            tickOut = static_cast<Tick> (std::llround (frame * 120.0 / k));
            return true;
        }
        // Last segment whose start frame <= frame.
        const std::size_t upper = static_cast<std::size_t> (
            std::upper_bound (startFrame_.begin(), startFrame_.end(), frame) - startFrame_.begin());
        const std::size_t i = upper == 0 ? 0 : upper - 1u;
        const double local = frame - startFrame_[i];
        const bool haveNext = i + 1u < changes_.size();
        const double startBpm = changes_[i].bpm;
        double deltaTicks = 0.0;
        if (haveNext && changes_[i].curveToNext == TempoCurve::LinearRamp && changes_[i + 1u].bpm != startBpm)
        {
            const double segmentTicks = static_cast<double> (changes_[i + 1u].tick - changes_[i].tick);
            const double slope = (changes_[i + 1u].bpm - startBpm) / segmentTicks;
            if (slope == 0.0 || ! std::isfinite (slope) || ! detail::isFinitePositive (k))
                return false;
            // Normalize this ratio so neither local*slope nor local/k must be representable.
            int localPower = 0, slopePower = 0, kPower = 0;
            const double localMantissa = std::frexp (local, &localPower);
            const double slopeMantissa = std::frexp (slope, &slopePower);
            const double kMantissa = std::frexp (k, &kPower);
            const double exponent = std::ldexp ((localMantissa * slopeMantissa) / kMantissa,
                                                localPower + slopePower - kPower);
            if (! std::isfinite (exponent) || (local != 0.0 && exponent == 0.0))
                return false;
            deltaTicks = (startBpm / slope) * std::expm1 (exponent);
        }
        else
            deltaTicks = local * startBpm / k;
        if (! std::isfinite (deltaTicks))
            return false;
        tickOut = changes_[i].tick + static_cast<Tick> (std::llround (deltaTicks));
        return true;
    }

    // Checked fractional lookups use native Frame doubles in [0, 2^63). Zero fractions retain
    // the legacy whole-Tick operation order exactly; fractional work is local to its segment.
    [[nodiscard]] TimeLookupStatus frameForMusicalTime (MusicalTime time, double& frameOut) const noexcept
    {
        frameOut = 0.0;
        if (time.tick < 0 || ! time.hasValidFraction())
            return TimeLookupStatus::InvalidInput;
        if (! checkedBuilt_)
            return checkedFailure_;
        if (! empty_ && changes_.empty())
            return TimeLookupStatus::InvalidMap;
        double frame = 0.0;
        if (! frameForTick (time.tick, frame))
            return TimeLookupStatus::NumericDomain;
        if (time.frac == 0.0)
            return detail::checkedNativeFrame (frame, frameOut);
        double acceptedWhole = 0.0;
        const auto wholeStatus = detail::checkedNativeFrame (frame, acceptedWhole);
        if (wholeStatus != TimeLookupStatus::Ok)
            return wholeStatus;

        const double k = 60.0 * sampleRate_.hz / static_cast<double> (kTicksPerQuarter);
        if (! detail::isFinitePositive (k))
            return TimeLookupStatus::NumericDomain;
        double bpm = 120.0;
        double slope = 0.0;
        if (! empty_)
        {
            const auto upper = std::upper_bound (changes_.begin(), changes_.end(), time.tick,
                [] (Tick tick, const TempoChange& change) noexcept { return tick < change.tick; });
            const auto i = static_cast<std::size_t> (upper - changes_.begin()) - 1u;
            bpm = changes_[i].bpm;
            if (i + 1u < changes_.size() && changes_[i].curveToNext == TempoCurve::LinearRamp
                && changes_[i + 1u].bpm != bpm)
            {
                const Tick length = changes_[i + 1u].tick - changes_[i].tick;
                const Tick local = time.tick - changes_[i].tick;
                slope = (changes_[i + 1u].bpm - bpm) / static_cast<double> (length);
                if (slope == 0.0 || ! std::isfinite (slope))
                    return TimeLookupStatus::NumericDomain;
                // Anchor to the nearer endpoint, avoiding cancellation near a descending end.
                bpm = local <= length / 2 ? bpm + slope * static_cast<double> (local)
                    : changes_[i + 1u].bpm - slope * static_cast<double> (length - local);
            }
        }
        if (! detail::isFinitePositive (bpm))
            return TimeLookupStatus::NumericDomain;
        double extra = 0.0;
        if (slope == 0.0)
        {
            if (! detail::scaledTimeRatio (time.frac, k, bpm, extra))
                return TimeLookupStatus::NumericDomain;
        }
        else
        {
            double relative = 0.0;
            if (! detail::scaledTimeRatio (slope, time.frac, bpm, relative) || relative <= -1.0
                || ! detail::scaledTimeRatio (k, std::log1p (relative), slope, extra))
                return TimeLookupStatus::NumericDomain;
        }
        if (extra >= 0x1p63 - frame)
            return TimeLookupStatus::Overflow;
        return detail::checkedNativeFrame (frame + extra, frameOut);
    }

    [[nodiscard]] TimeLookupStatus musicalTimeForFrame (double frame, MusicalTime& timeOut) const noexcept
    {
        timeOut = MusicalTime {};
        if (! std::isfinite (frame) || frame < 0.0)
            return TimeLookupStatus::InvalidInput;
        if (frame >= 0x1p63)
            return TimeLookupStatus::Overflow;
        if (! checkedBuilt_)
            return checkedFailure_;
        if (! empty_ && changes_.empty())
            return TimeLookupStatus::InvalidMap;
        const double k = 60.0 * sampleRate_.hz / static_cast<double> (kTicksPerQuarter);
        if (! detail::isFinitePositive (k))
            return TimeLookupStatus::NumericDomain;
        Tick origin = 0;
        std::size_t i = 0;
        double local = frame;
        double bpm = 120.0;
        if (! empty_)
        {
            const auto upper = std::upper_bound (startFrame_.begin(), startFrame_.end(), frame);
            i = upper == startFrame_.begin() ? 0u : static_cast<std::size_t> (upper - startFrame_.begin()) - 1u;
            origin = changes_[i].tick;
            local -= startFrame_[i];
            bpm = changes_[i].bpm;
        }
        double delta = 0.0;
        const bool ramp = ! empty_ && i + 1u < changes_.size()
            && changes_[i].curveToNext == TempoCurve::LinearRamp && changes_[i + 1u].bpm != bpm;
        if (ramp)
        {
            const double slope = (changes_[i + 1u].bpm - bpm)
                / static_cast<double> (changes_[i + 1u].tick - origin);
            double exponent = 0.0;
            if (slope == 0.0 || ! detail::scaledTimeRatio (local, slope, k, exponent)
                || ! detail::scaledTimeRatio (bpm, std::expm1 (exponent), slope, delta))
                return TimeLookupStatus::NumericDomain;
        }
        else if (! detail::scaledTimeRatio (local, bpm, k, delta))
            return TimeLookupStatus::NumericDomain;

        // double(Tick::max) rounds UP to 2^63. Check that exact boundary BEFORE casting,
        // then check integer origin addition; never add an absolute Tick in floating point.
        if (! std::isfinite (delta) || delta < 0.0)
            return TimeLookupStatus::NumericDomain;
        if (delta >= 0x1p63)
            return TimeLookupStatus::Overflow;
        const double integral = std::floor (delta);
        const Tick whole = static_cast<Tick> (integral);
        if (whole > std::numeric_limits<Tick>::max() - origin)
            return TimeLookupStatus::Overflow;
        const MusicalTime result { origin + whole, delta - integral };
        if (! empty_ && i + 1u < changes_.size() && result.tick >= changes_[i + 1u].tick)
            return TimeLookupStatus::NumericDomain;
        timeOut = result;
        return TimeLookupStatus::Ok;
    }

    [[nodiscard]] bool empty() const noexcept { return empty_; }
    [[nodiscard]] std::size_t segmentCount() const noexcept { return changes_.size(); }

private:
    SampleRate               sampleRate_ {};
    std::vector<TempoChange> changes_;
    std::vector<double>      startFrame_;
    bool                     empty_ = true;
    bool                     checkedBuilt_ = false;
    TimeLookupStatus         checkedFailure_ = TimeLookupStatus::InvalidMap;
};

struct MeterPosition
{
    std::size_t   segment = 0;
    std::uint64_t beatOrdinal = 0;   // zero-based, relative to this Meter segment
    BarBeat       barBeat;
    bool          downbeat = true;  // the current beat's ordinal is divisible by the numerator
};

namespace detail {

// Exact a*b/divisor without a wide-integer extension. Meter callers bound the remainder
// product below 2^48; checks keep the helper safe even if a future caller exceeds that bound.
[[nodiscard]] inline bool meterMulDiv (std::uint64_t a, std::uint64_t b, std::uint64_t divisor,
                                      std::uint64_t& quotient, std::uint64_t& remainder) noexcept
{
    quotient = remainder = 0;
    if (divisor == 0)
        return false;
    const auto q = a / divisor;
    const auto r = a % divisor;
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    if (b != 0 && (q > maximum / b || r > maximum / b))
        return false;
    const auto tail = r * b;
    const auto head = q * b;
    if (head > maximum - tail / divisor)
        return false;
    quotient = head + tail / divisor;
    remainder = tail % divisor;
    return true;
}

} // namespace detail

// Built on Control, read through immutable indexed segments. All queries are allocation-free,
// lock-free and O(log segment count), and retain every positive stored uint16 Meter value.
class CompiledMeterMap
{
public:
    [[nodiscard]] static TimeLookupStatus build (MeterMapView map, CompiledMeterMap& out)
    {
        out = CompiledMeterMap {};
        if (! map.empty())
        {
            if (map.changes == nullptr || map.changes[0].tick != 0 || ! map.changes[0].isValid())
                return out.status_;
            for (std::size_t i = 1; i < map.count; ++i)
                if (! map.changes[i].isValid() || map.changes[i].tick <= map.changes[i - 1u].tick)
                    return out.status_;
        }
        CompiledMeterMap prepared;
        prepared.segments_.reserve (map.empty() ? 1u : map.count);
        prepared.segments_.push_back ({ map.empty() ? MeterChange {} : map.changes[0], 1 });
        for (std::size_t i = 1; i < map.count; ++i)
        {
            const Segment& previous = prepared.segments_.back();
            std::uint64_t bars = 0, remainder = 0;
            if (! detail::meterMulDiv (static_cast<std::uint64_t> (map.changes[i].tick - previous.meter.tick),
                                       previous.meter.denominator, 61440ULL * previous.meter.numerator,
                                       bars, remainder)
                || bars > static_cast<std::uint64_t> (std::numeric_limits<Tick>::max() - previous.firstBar)
                || (remainder != 0 && bars == static_cast<std::uint64_t> (std::numeric_limits<Tick>::max() - previous.firstBar)))
            {
                out.status_ = TimeLookupStatus::Overflow;
                return out.status_;
            }
            const Tick firstBar = previous.firstBar + static_cast<Tick> (bars) + (remainder != 0 ? 1 : 0);
            prepared.segments_.push_back ({ map.changes[i], firstBar });
        }
        prepared.status_ = TimeLookupStatus::Ok;
        out = std::move (prepared);
        return TimeLookupStatus::Ok;
    }

    [[nodiscard]] TimeLookupStatus positionForTick (Tick tick, MeterPosition& out) const noexcept
    {
        out = MeterPosition {};
        if (tick < 0)
            return TimeLookupStatus::InvalidInput;
        if (status_ != TimeLookupStatus::Ok)
            return status_;
        if (segments_.empty())
            return TimeLookupStatus::InvalidMap;
        const auto upper = std::upper_bound (segments_.begin(), segments_.end(), tick,
            [] (Tick value, const Segment& segment) noexcept { return value < segment.meter.tick; });
        const auto i = static_cast<std::size_t> (upper - segments_.begin()) - 1u;
        const Segment& segment = segments_[i];
        std::uint64_t ordinal = 0, remainder = 0;
        if (! detail::meterMulDiv (static_cast<std::uint64_t> (tick - segment.meter.tick),
                                   segment.meter.denominator, 61440, ordinal, remainder))
            return TimeLookupStatus::Overflow;
        const auto bars = ordinal / segment.meter.numerator;
        if (bars > static_cast<std::uint64_t> (std::numeric_limits<Tick>::max() - segment.firstBar))
            return TimeLookupStatus::Overflow;
        out = { i, ordinal,
                { segment.firstBar + static_cast<Tick> (bars), static_cast<Tick> (ordinal % segment.meter.numerator) + 1 },
                ordinal % segment.meter.numerator == 0 };
        return TimeLookupStatus::Ok;
    }

    [[nodiscard]] TimeLookupStatus positionForMusicalTime (MusicalTime time, MeterPosition& out) const noexcept
    {
        out = MeterPosition {};
        if (time.tick < 0 || ! time.hasValidFraction())
            return TimeLookupStatus::InvalidInput;
        if (time.frac == 0.0)
            return positionForTick (time.tick, out);
        MeterPosition position;
        const auto status = positionForTick (time.tick, position);
        if (status != TimeLookupStatus::Ok)
            return status;
        const Segment& segment = segments_[position.segment];
        // A stored Meter beat is at least 61440/65535 > 0.937 Tick, so a fraction below
        // one Tick can cross at most two beat starts after the whole-Tick position.
        for (unsigned step = 0; step < 2; ++step)
        {
            RationalTick next;
            const auto nextStatus = beatStart (position.segment, position.beatOrdinal + 1u, next);
            // Either refusal proves that this next onset is beyond the active segment or
            // the representable whole-Tick range, hence strictly after this valid input.
            if (nextStatus == TimeLookupStatus::InvalidInput || nextStatus == TimeLookupStatus::Overflow)
                break;
            if (nextStatus != TimeLookupStatus::Ok)
                return nextStatus;
            if (next.whole > time.tick
                || (next.whole == time.tick
                    && std::fma (time.frac, static_cast<double> (next.denominator),
                                 -static_cast<double> (next.numerator)) < 0.0))
                break;
            ++position.beatOrdinal;
            if (position.barBeat.beat == segment.meter.numerator)
            {
                if (position.barBeat.bar == std::numeric_limits<Tick>::max())
                    return TimeLookupStatus::Overflow;
                ++position.barBeat.bar;
                position.barBeat.beat = 1;
            }
            else
                ++position.barBeat.beat;
            position.downbeat = position.beatOrdinal % segment.meter.numerator == 0;
        }
        out = position;
        return TimeLookupStatus::Ok;
    }

    [[nodiscard]] TimeLookupStatus barStart (Tick oneBasedBar, RationalTick& out) const noexcept
    {
        out = RationalTick {};
        if (oneBasedBar < 1)
            return TimeLookupStatus::InvalidInput;
        if (status_ != TimeLookupStatus::Ok)
            return status_;
        if (segments_.empty())
            return TimeLookupStatus::InvalidMap;
        const auto upper = std::upper_bound (segments_.begin(), segments_.end(), oneBasedBar,
            [] (Tick value, const Segment& segment) noexcept { return value < segment.firstBar; });
        const auto i = static_cast<std::size_t> (upper - segments_.begin()) - 1u;
        return boundary (i, static_cast<std::uint64_t> (oneBasedBar - segments_[i].firstBar),
                         61440ULL * segments_[i].meter.numerator, out);
    }

    // Ordinals are zero-based and belong only to [this origin, next Meter-change tick).
    // A beat exactly at the next change belongs to that next segment, whose ordinal is zero.
    [[nodiscard]] TimeLookupStatus beatStart (std::size_t segment, std::uint64_t ordinal, RationalTick& out) const noexcept
    {
        out = RationalTick {};
        if (status_ != TimeLookupStatus::Ok)
            return status_;
        if (segments_.empty())
            return TimeLookupStatus::InvalidMap;
        if (segment >= segments_.size())
            return TimeLookupStatus::InvalidInput;
        return boundary (segment, ordinal, 61440, out);
    }

    [[nodiscard]] std::size_t segmentCount() const noexcept { return segments_.size(); }

private:
    struct Segment
    {
        MeterChange meter;
        Tick        firstBar = 1;
    };

    [[nodiscard]] TimeLookupStatus boundary (std::size_t i, std::uint64_t ordinal,
                                            std::uint64_t span, RationalTick& out) const noexcept
    {
        const Segment& segment = segments_[i];
        std::uint64_t whole = 0, remainder = 0;
        if (! detail::meterMulDiv (ordinal, span, segment.meter.denominator, whole, remainder)
            || whole > static_cast<std::uint64_t> (std::numeric_limits<Tick>::max() - segment.meter.tick))
            return TimeLookupStatus::Overflow;
        const Tick tick = segment.meter.tick + static_cast<Tick> (whole);
        if (i + 1u < segments_.size() && tick >= segments_[i + 1u].meter.tick)
            return TimeLookupStatus::InvalidInput;
        out = { tick, static_cast<std::uint32_t> (remainder), remainder == 0 ? 1u : segment.meter.denominator };
        return TimeLookupStatus::Ok;
    }

    std::vector<Segment> segments_;
    TimeLookupStatus    status_ = TimeLookupStatus::InvalidMap;
};

namespace detail {

// G2.15: one meter's beat and full-bar lengths in ticks. Beat lengths follow the denominator; the
// clamps are the head law's (computeBarGrid).
[[nodiscard]] inline double meterBeatTicks (const MeterChange& meter) noexcept
{
    const double den = std::clamp (static_cast<double> (meter.denominator), 1.0, 64.0);
    return static_cast<double> (kTicksPerQuarter) * 4.0 / den;
}

[[nodiscard]] inline double meterBarTicks (const MeterChange& meter) noexcept
{
    return meterBeatTicks (meter) * std::clamp (static_cast<double> (meter.numerator), 1.0, 32.0);
}

// One stretch of the meter map under a single meter: the tick it starts at and the bar that starts there.
struct MeterSegment
{
    Tick         start = 0;
    std::int64_t firstBar = 1;
    MeterChange  meter { 0, 4, 4 };
};

// The G2.15 meter walk, the one law under barBeatAtTick, barStartTick and nominalBarTicks. Each valid
// change past the last one starts a bar, so a change inside a bar cuts that bar short. The head
// meter covers everything before the first change (4/4 when the head is invalid). The walk stops at
// the first invalid or out-of-order change after the head, and before the first change
// `stopsBefore (changeTick, barItStarts)` accepts.
template <typename StopsBefore>
[[nodiscard]] inline MeterSegment walkMeterMap (MeterMapView meterMap, StopsBefore stopsBefore) noexcept
{
    MeterSegment segment;
    if (meterMap.count > 0 && meterMap.changes != nullptr && meterMap.changes[0].isValid())
        segment.meter = meterMap.changes[0];
    for (std::size_t i = 1; meterMap.changes != nullptr && i < meterMap.count; ++i)
    {
        const MeterChange& next = meterMap.changes[i];
        if (! next.isValid() || next.tick <= segment.start)
            break;
        const std::int64_t nextBar = segment.firstBar
                                   + static_cast<std::int64_t> (std::ceil (static_cast<double> (next.tick - segment.start)
                                                                           / meterBarTicks (segment.meter)));
        if (stopsBefore (next.tick, nextBar))
            break;
        segment = MeterSegment { next.tick, nextBar, next };
    }
    return segment;
}

} // namespace detail

// G2.15: bar|beat at a tick under the meter map (the walk above).
[[nodiscard]] inline BarBeat barBeatAtTick (MeterMapView meterMap, Tick tick) noexcept
{
    const detail::MeterSegment segment =
        detail::walkMeterMap (meterMap, [tick] (Tick changeTick, std::int64_t) noexcept { return changeTick > tick; });
    const double beatTicks = detail::meterBeatTicks (segment.meter);
    const double barTicks = detail::meterBarTicks (segment.meter);
    const double local = static_cast<double> (tick - segment.start);
    const auto barsIn = static_cast<std::int64_t> (std::floor (local / barTicks));
    const double inBar = local - static_cast<double> (barsIn) * barTicks;
    BarBeat out;
    out.bar = segment.firstBar + barsIn;
    out.beat = 1 + static_cast<std::int64_t> (std::floor (inBar / beatTicks));
    return out;
}

// UI correctness pass unit 6: the first tick barBeatAtTick reads as `bar` or later - its exact
// inverse on whole ticks. A bar that starts between two ticks starts at the next one (the walk
// floors), and the estimate is checked against the walk's own arithmetic, because bar x length can
// round onto the wrong side of a whole tick (3/39 at bar 66). A bar that a meter change cuts to less
// than one tick starts at that change. False for bar < 1 and for a start past 2^52 ticks.
[[nodiscard]] inline bool barStartTick (MeterMapView meterMap, std::int64_t bar, Tick& out) noexcept
{
    out = 0;
    if (bar < 1)
        return false;
    const detail::MeterSegment segment =
        detail::walkMeterMap (meterMap, [bar] (Tick, std::int64_t barItStarts) noexcept { return barItStarts > bar; });
    const double barTicks = detail::meterBarTicks (segment.meter);
    const std::int64_t barsIn = bar - segment.firstBar;
    const double estimate = std::ceil (static_cast<double> (barsIn) * barTicks);
    if (! (estimate <= 4503599627370496.0 - static_cast<double> (segment.start)))
        return false;
    const auto readsEarlier = [&] (Tick tick) noexcept
    {
        return static_cast<std::int64_t> (std::floor (static_cast<double> (tick - segment.start) / barTicks)) < barsIn;
    };
    Tick start = segment.start + static_cast<Tick> (estimate);
    while (start > segment.start && ! readsEarlier (start - 1))
        --start;
    while (readsEarlier (start))
        ++start;
    out = start;
    return true;
}

// UI correctness pass unit 6: the ticks in one full bar of the meter in force at `tick` - the step a
// ruler thins its labels by. Not the length of the bar at `tick`, which a meter change can cut short.
[[nodiscard]] inline double nominalBarTicks (MeterMapView meterMap, Tick tick) noexcept
{
    return detail::meterBarTicks (
        detail::walkMeterMap (meterMap, [tick] (Tick changeTick, std::int64_t) noexcept { return changeTick > tick; }).meter);
}

// G2.15: bar|beat at a frame under the FULL tempo and meter maps — frame → tick through the
// compiled tempo map's inverse, then the meter walk. Falls back to the single-tempo law when the
// maps are empty.
[[nodiscard]] inline bool computeBarBeatPiecewise (const CompiledTempoMap& tempoMap,
                                                   MeterMapView meterMap,
                                                   std::int64_t frame,
                                                   BarBeat& out) noexcept
{
    out = BarBeat {};
    Tick tick = 0;
    if (! tempoMap.tickForFrame (static_cast<double> (std::max<std::int64_t> (0, frame)), tick))
        return false;
    out = barBeatAtTick (meterMap, tick);
    return true;
}

} // namespace yesdaw::engine
