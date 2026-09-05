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

// V2/V4: a 1-based bar and beat-within-bar position.
struct BarBeat
{
    std::int64_t bar = 1;
    std::int64_t beat = 1;
};

// V4: the bar/beat lengths at a SINGLE tempo/meter, in frames at the given sample rate. This is
// the ONE shared sub-law under computeBarBeat (the V2 transport readout) and the ruler's bar-label
// positions (V4) — both derive their grid from this exact float-operation order, so the header
// readout and the painted ruler can never disagree. Passing sampleRateHz = 1.0 yields the lengths
// in SECONDS (frames at 1 Hz are seconds), which is how the seconds-based ruler consumes it.
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
// same scope the existing headBarFrames() family (UiAppModel.h) already commits to; piecewise
// tempo/meter changes are not supported by ANY bar-length law in this codebase yet, and extending
// to piecewise is a materially larger task (accurate tempo-ramp inversion) than this presentation
// fix calls for. Clamp ranges mirror headQuarterNoteFrames/headMeterBeatFrames/headBarFramesExact
// exactly, so a bar|beat computed here always agrees with those existing bar-length helpers.
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
                return false;

            out.startFrame_[i] = frame;
        }

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
    // inverts the logarithm (frame = (k/slope)·ln(bpmAt/startBpm)  ⇒  bpmAt = startBpm·exp(frame·slope/k),
    // delta = (bpmAt − startBpm)/slope). Rounded to the nearest tick; frameForTick (tickForFrame (f)) ≈ f.
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
            if (std::abs (slope) < 1.0e-12)
                deltaTicks = local * startBpm / k;
            else
                deltaTicks = (startBpm * std::exp (local * slope / k) - startBpm) / slope;
        }
        else
            deltaTicks = local * startBpm / k;
        if (! std::isfinite (deltaTicks))
            return false;
        tickOut = changes_[i].tick + static_cast<Tick> (std::llround (deltaTicks));
        return true;
    }

    [[nodiscard]] bool empty() const noexcept { return empty_; }
    [[nodiscard]] std::size_t segmentCount() const noexcept { return changes_.size(); }

private:
    SampleRate               sampleRate_ {};
    std::vector<TempoChange> changes_;
    std::vector<double>      startFrame_;
    bool                     empty_ = true;
};

// G2.15: bar|beat at a frame under the FULL tempo and meter maps — frame → tick through the
// compiled tempo map's inverse, then a walk over the meter changes (each change starts a bar).
// Beat lengths follow each meter's denominator; the head meter covers everything before the first
// change. Falls back to the single-tempo law when the maps are empty.
[[nodiscard]] inline bool computeBarBeatPiecewise (const CompiledTempoMap& tempoMap,
                                                   MeterMapView meterMap,
                                                   std::int64_t frame,
                                                   BarBeat& out) noexcept
{
    out = BarBeat {};
    Tick tick = 0;
    if (! tempoMap.tickForFrame (static_cast<double> (std::max<std::int64_t> (0, frame)), tick))
        return false;
    const auto ticksPerBeat = [] (const MeterChange& meter) noexcept -> double
    {
        const double den = std::clamp (static_cast<double> (meter.denominator), 1.0, 64.0);
        return static_cast<double> (kTicksPerQuarter) * 4.0 / den;
    };
    MeterChange head { 0, 4, 4 };
    if (meterMap.count > 0 && meterMap.changes != nullptr && meterMap.changes[0].isValid())
        head = meterMap.changes[0];
    std::int64_t bar = 1;
    Tick segmentStart = 0;
    MeterChange current = head;
    for (std::size_t i = 1; meterMap.changes != nullptr && i < meterMap.count; ++i)
    {
        const MeterChange& next = meterMap.changes[i];
        if (! next.isValid() || next.tick <= segmentStart || next.tick > tick)
            break;
        const double barTicks = ticksPerBeat (current) * std::clamp (static_cast<double> (current.numerator), 1.0, 32.0);
        bar += static_cast<std::int64_t> (std::ceil (static_cast<double> (next.tick - segmentStart) / barTicks));
        segmentStart = next.tick;
        current = next;
    }
    const double beatTicks = ticksPerBeat (current);
    const double numerator = std::clamp (static_cast<double> (current.numerator), 1.0, 32.0);
    const double barTicks = beatTicks * numerator;
    const double local = static_cast<double> (tick - segmentStart);
    const auto barsIn = static_cast<std::int64_t> (std::floor (local / barTicks));
    const double inBar = local - static_cast<double> (barsIn) * barTicks;
    out.bar = bar + barsIn;
    out.beat = 1 + static_cast<std::int64_t> (std::floor (inBar / beatTicks));
    return true;
}

} // namespace yesdaw::engine
