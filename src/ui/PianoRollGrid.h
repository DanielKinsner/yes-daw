#pragma once

#include "ui/TimelineCoordinates.h"
#include <set>

namespace yesdaw::ui {

[[nodiscard]] inline engine::Tick pianoQuantizeTowards (engine::Tick from, engine::Tick target, std::uint8_t strength) noexcept
{
    const bool right = target >= from;
    const auto distance = right ? target - from : from - target;
    const auto tail = (distance % 100) * strength;
    const auto whole = (distance / 100) * strength + tail / 100;
    const auto fraction = tail % 100;
    return right ? from + whole + (fraction >= 50 ? 1 : 0)
                 : from - whole - (fraction > 50 ? 1 : 0);
}

[[nodiscard]] inline engine::Tick pianoHumanizeOffset (const engine::QuantizeSettings& settings,
                                                       const engine::EntityId& id) noexcept
{
    const auto interval = settings.grid.intervalTicks;
    const auto percent = settings.humanizePercent;
    if (percent == 0) return 0;
    const auto range = (interval / 100) * percent + ((interval % 100) * percent) / 100;
    if (interval <= INT64_MAX / 100 && range <= (UINT32_MAX - 1u) / 2u)
        return engine::quantizeHumanizeOffset (settings, id.bytes.data(), id.bytes.size());
    std::uint32_t hash = 2166136261u;
    for (int shift = 0; shift < 32; shift += 8)
    {
        hash ^= (settings.humanizeSeed >> shift) & 0xFFu;
        hash *= 16777619u;
    }
    for (const auto byte : id.bytes) { hash ^= byte; hash *= 16777619u; }
    const auto offset = static_cast<std::uint64_t> (hash) % (2u * static_cast<std::uint64_t> (range) + 1u);
    return offset >= static_cast<std::uint64_t> (range) ? static_cast<engine::Tick> (offset - range)
        : -static_cast<engine::Tick> (static_cast<std::uint64_t> (range) - offset);
}

struct PianoGridDivision
{
    MusicalGridLevel level = MusicalGridLevel::Beat;
    unsigned subdivisions = 0;
};

struct PianoNativeGridLine
{
    engine::Tick tick = 0;
    MusicalGridLevel level = MusicalGridLevel::Beat;
};

struct PianoRollGrid
{
    std::shared_ptr<const engine::MusicalClickMap> clock;
    engine::TimeBase timeBase = engine::TimeBase::SampleLocked;
    engine::Tick origin = 0;
    engine::Tick length = 0;
    std::vector<engine::MeterChange> meterChanges;

    [[nodiscard]] bool musicalTime (engine::Tick local, engine::MusicalTime& out) const noexcept
    {
        if (clock == nullptr || origin < 0 || local < -origin || local > INT64_MAX - origin)
            return false;
        const auto absolute = origin + local;
        if (timeBase == engine::TimeBase::TempoLocked)
        {
            out = { absolute, 0.0 };
            return true;
        }
        const auto frame = engine::click_detail::integer (static_cast<std::uint64_t> (absolute));
        return frame.low == frame.high && ! frame.numericFailure
            && clock->tempoMap().musicalTimeForFrame (frame.low, out) == engine::TimeLookupStatus::Ok;
    }

    [[nodiscard]] bool rationalFrame (engine::RationalTick tick, engine::click_detail::Enclosure& out) const noexcept
    {
        if (clock == nullptr || tick.whole < 0 || tick.denominator == 0 || tick.numerator >= tick.denominator)
            return false;
        out = clock->framePosition (tick.whole);
        if (tick.numerator != 0)
        {
            engine::click_detail::Enclosure fraction;
            bool before = false;
            if (clock->frameDisplacement (tick.whole, true, tick.numerator, tick.denominator,
                                          fraction, before) != engine::TimeLookupStatus::Ok || before)
                return false;
            out = engine::click_detail::add (out, fraction);
        }
        return ! out.numericFailure && out.low >= 0.0 && std::isfinite (out.high);
    }

    [[nodiscard]] bool nativeTick (engine::RationalTick tick, engine::Tick& out) const noexcept
    {
        engine::Tick absolute = 0;
        if (timeBase == engine::TimeBase::TempoLocked)
        {
            if (engine::roundRationalTick (tick, engine::TimeRounding::Nearest, absolute) != engine::TimeLookupStatus::Ok)
                return false;
        }
        else
        {
            TimelineFramePoint point;
            if (! rationalFrame (tick, point.offset)
                || timelineNativeFrameTarget (clock, engine::TimeBase::SampleLocked, point, absolute) != TimelineCoordinateStatus::Ok)
                return false;
        }
        out = absolute - origin;
        return true;
    }

    [[nodiscard]] bool frameForNative (engine::Tick local, std::int64_t& frame) const noexcept
    {
        if (local < -origin || local > INT64_MAX - origin)
            return false;
        if (timeBase == engine::TimeBase::SampleLocked)
        {
            frame = origin + local;
            return true;
        }
        TimelineFramePoint point;
        if (clock == nullptr) return false;
        point.offset = clock->framePosition (origin + local);
        return timelineNativeFrameTarget (clock, engine::TimeBase::SampleLocked, point, frame) == TimelineCoordinateStatus::Ok;
    }

    [[nodiscard]] static bool spanForMeter (const engine::MeterChange& meter, PianoGridDivision division,
                                            std::uint64_t& numerator, std::uint32_t& denominator) noexcept
    {
        if (division.subdivisions > 16) return false;
        numerator = division.level == MusicalGridLevel::Sixteenth ? 3840u
                  : division.level == MusicalGridLevel::Bar ? 61440ULL * meter.numerator : 61440u;
        denominator = division.level == MusicalGridLevel::Sixteenth ? 1u : meter.denominator;
        denominator *= std::uint32_t { 1 } << division.subdivisions;
        return true;
    }

    [[nodiscard]] bool span (engine::MusicalTime at, PianoGridDivision division,
                             std::uint64_t& numerator, std::uint32_t& denominator) const noexcept
    {
        engine::MeterChange meter;
        if (clock == nullptr || division.subdivisions > 16
            || clock->meterAtPosition (at.tick, true, meter) != engine::TimeLookupStatus::Ok)
            return false;
        return spanForMeter (meter, division, numerator, denominator);
    }

    // A nominal starting-Meter step, independent of snapping to a boundary.
    [[nodiscard]] bool advance (engine::Tick local, PianoGridDivision division, bool right,
                                engine::Tick& target, engine::RationalTick* cursor = nullptr) const noexcept
    {
        engine::MusicalTime at;
        if (! musicalTime (local, at)) return false;
        if (cursor != nullptr && timeBase == engine::TimeBase::TempoLocked)
            at = musical_grid_detail::musicalTime (*cursor);
        std::uint64_t numerator = 0;
        std::uint32_t denominator = 0;
        if (! span (at, division, numerator, denominator)) return false;
        if (timeBase == engine::TimeBase::TempoLocked)
        {
            auto start = cursor != nullptr ? *cursor : engine::RationalTick { origin + local, 0, 1 };
            engine::RationalTick end;
            if (right)
            {
                if (musical_grid_detail::offset (start, 1, numerator, denominator, end) != MusicalGridStatus::Ok)
                    return false;
            }
            else
            {
                const auto common = std::uint64_t { denominator / std::gcd (denominator, start.denominator) } * start.denominator;
                if (common > UINT32_MAX) return false;
                const auto whole = numerator / denominator;
                const auto fraction = numerator % denominator * (common / denominator);
                const auto startFraction = std::uint64_t { start.numerator } * (common / start.denominator);
                const auto borrow = fraction > startFraction ? 1u : 0u;
                if (whole + borrow > static_cast<std::uint64_t> (start.whole)) end = {};
                else end = { start.whole - static_cast<engine::Tick> (whole + borrow),
                             static_cast<std::uint32_t> (borrow != 0 ? common + startFraction - fraction : startFraction - fraction),
                             static_cast<std::uint32_t> (common) };
            }
            if (! nativeTick (end, target)) return false;
            if (cursor != nullptr) *cursor = end;
            return true;
        }
        TimelineFramePoint point;
        point.integerOrigin = origin + local;
        bool before = false;
        if (numerator > static_cast<std::uint64_t> (INT64_MAX)
            || clock->frameDisplacement (point.integerOrigin, false,
                right ? static_cast<std::int64_t> (numerator) : -static_cast<std::int64_t> (numerator), denominator,
                point.offset, before) != engine::TimeLookupStatus::Ok)
            return false;
        if (before) { target = -origin; return true; }
        engine::Tick absolute = 0;
        if (timelineNativeFrameTarget (clock, timeBase, point, absolute) != TimelineCoordinateStatus::Ok) return false;
        target = absolute - origin;
        return true;
    }

    [[nodiscard]] bool candidates (engine::Tick local, PianoGridDivision division,
                                   std::uint8_t swing, std::vector<engine::RationalTick>& out) const
    {
        using namespace musical_grid_detail;
        engine::MusicalTime time;
        engine::MeterPosition position;
        if (! musicalTime (local, time)
            || clock->meterMap().positionForMusicalTime (time, position) != engine::TimeLookupStatus::Ok)
            return false;
        engine::MeterChange meter;
        if (clock->meterAtPosition (time.tick, true, meter) != engine::TimeLookupStatus::Ok) return false;
        const engine::RationalTick start { meter.tick, 0, 1 };
        const bool haveEnd = position.segment + 1u < meterChanges.size();
        const engine::RationalTick end { haveEnd ? meterChanges[position.segment + 1u].tick : INT64_MAX, 0, 1 };
        out.push_back (start);
        if (haveEnd) out.push_back (end);
        const auto append = [&] (PianoGridDivision unit, std::uint8_t swingPercent) {
            std::uint64_t numerator = 0;
            std::uint32_t denominator = 0;
            if (! spanForMeter (meter, unit, numerator, denominator)) return false;
            const auto localWhole = static_cast<std::uint64_t> (time.tick - start.whole);
            auto cycles = localWhole / numerator;
            if (cycles != 0) --cycles;
            const engine::RationalTick nearby { start.whole + static_cast<engine::Tick> (cycles * numerator), 0, 1 };
            std::uint64_t middle = 0, remainder = 0;
            if (! engine::detail::meterMulDiv (static_cast<std::uint64_t> (time.tick - nearby.whole),
                                               denominator, numerator, middle, remainder)) return false;
            const auto fractional = static_cast<std::uint64_t> (std::floor (
                (static_cast<double> (remainder) + time.frac * denominator) / static_cast<double> (numerator)));
            if (fractional > UINT64_MAX - middle) return false;
            middle += fractional;
            const auto first = middle > 1 ? middle - 1 : 0;
            const bool oddOrigin = cycles % 2 != 0 && denominator % 2 != 0;
            for (std::uint64_t i = first; i - first < 4; ++i)
            {
                engine::RationalTick tick;
                const auto code = offset (nearby, i, numerator, denominator, tick);
                if (code == MusicalGridStatus::Overflow) break;
                if (code != MusicalGridStatus::Ok) return false;
                if (swingPercent != 0 && ((i % 2 != 0) != oddOrigin))
                {
                    if (denominator > UINT32_MAX / 100u
                        || offset (tick, 1, numerator * swingPercent, denominator * 100u, tick) != MusicalGridStatus::Ok)
                        return false;
                }
                if (haveEnd && ! before (tick, end)) break;
                out.push_back (tick);
            }
            return true;
        };
        if (! append (division, swing)) return false;
        return division.level != MusicalGridLevel::Sixteenth
            || append ({ MusicalGridLevel::Beat, division.subdivisions }, swing);
    }

    [[nodiscard]] bool snappedPosition (engine::Tick local, PianoGridDivision division, bool floor,
                                        engine::RationalTick& out, std::uint8_t swing = 0) const
    {
        if (local < 0 || local > INT64_MAX - origin) return false;
        std::vector<engine::RationalTick> options;
        if (! candidates (local, division, swing, options)) return false;
        const auto absolute = origin + local;
        bool found = false;
        engine::RationalTick best, bestDistance;
        engine::click_detail::Enclosure bestFrameDistance;
        const auto distance = [absolute] (engine::RationalTick tick) {
            if (tick.whole >= absolute)
                return engine::RationalTick { tick.whole - absolute, tick.numerator, tick.denominator };
            return tick.numerator == 0 ? engine::RationalTick { absolute - tick.whole, 0, 1 }
                : engine::RationalTick { absolute - tick.whole - 1, tick.denominator - tick.numerator, tick.denominator };
        };
        for (const auto tick : options)
        {
            bool better = false;
            if (timeBase == engine::TimeBase::TempoLocked)
            {
                if (floor && (tick.whole > absolute || (tick.whole == absolute && tick.numerator != 0))) continue;
                const auto d = distance (tick);
                better = ! found || (floor ? musical_grid_detail::before (best, tick)
                    : musical_grid_detail::before (d, bestDistance)
                      || (! musical_grid_detail::before (bestDistance, d) && musical_grid_detail::before (best, tick)));
                if (better) bestDistance = d;
            }
            else
            {
                engine::click_detail::Enclosure mapped;
                if (! rationalFrame (tick, mapped)) return false;
                const auto relative = engine::click_detail::add (mapped, timelineSignedInteger (-absolute));
                if (relative.numericFailure) return false;
                if (floor && relative.low > 0.0) continue;
                if (floor && relative.high > 0.0) return false;
                engine::click_detail::Enclosure d = relative;
                if (relative.high <= 0.0) d = { -relative.high, -relative.low, false };
                else if (relative.low < 0.0) return false;
                if (! found) better = true;
                else if (floor) better = musical_grid_detail::before (best, tick);
                else if (d.high < bestFrameDistance.low) better = true;
                else if (d.low > bestFrameDistance.high) better = false;
                else if (d.low == d.high && bestFrameDistance.low == bestFrameDistance.high && d.low == bestFrameDistance.low)
                    better = musical_grid_detail::before (best, tick);
                else if (musical_grid_detail::before (best, tick) || musical_grid_detail::before (tick, best)) return false;
                if (better) bestFrameDistance = d;
            }
            if (better) { best = tick; found = true; }
        }
        if (! found) return false;
        out = best;
        return true;
    }

    [[nodiscard]] bool snapped (engine::Tick local, PianoGridDivision division, bool floor,
                                engine::Tick& out, std::uint8_t swing = 0) const
    {
        engine::RationalTick position;
        if (! snappedPosition (local, division, floor, position, swing) || ! nativeTick (position, out)) return false;
        out = std::max<engine::Tick> (0, out);
        return true;
    }

    [[nodiscard]] bool quantized (engine::Tick local, PianoGridDivision division, std::uint8_t swing,
                                  std::uint8_t strength, engine::Tick& out) const
    {
        if (strength > 100 || local < 0 || local > INT64_MAX - origin) return false;
        if (strength == 0) { out = local; return true; }
        engine::RationalTick position;
        if (! snappedPosition (local, division, false, position, swing)) return false;
        const auto from = origin + local;
        engine::Tick absolute = 0;
        if (timeBase == engine::TimeBase::TempoLocked)
        {
            if (position.whole < origin) position = { origin, 0, 1 };
            if (position.numerator == 0) absolute = pianoQuantizeTowards (from, position.whole, strength);
            else
            {
                const bool right = position.whole >= from;
                const auto wholeDistance = right ? position.whole - from : from - position.whole - 1;
                const auto fraction = right ? position.numerator : position.denominator - position.numerator;
                const auto denominator = std::uint64_t { position.denominator } * 100u;
                const auto tail = std::uint64_t { static_cast<std::uint64_t> (wholeDistance % 100) } * strength * position.denominator
                    + std::uint64_t { fraction } * strength;
                const auto whole = (wholeDistance / 100) * strength + static_cast<engine::Tick> (tail / denominator);
                const auto remainder = tail % denominator;
                const bool increment = right ? remainder >= denominator - remainder : remainder > denominator - remainder;
                if (right && (whole > INT64_MAX - from || (increment && whole == INT64_MAX - from))) return false;
                absolute = right ? from + whole + (increment ? 1 : 0) : from - whole - (increment ? 1 : 0);
            }
        }
        else
        {
            engine::click_detail::Enclosure frame;
            if (! rationalFrame (position, frame)) return false;
            const auto clipStart = engine::click_detail::integer (static_cast<std::uint64_t> (origin));
            if (clipStart.numericFailure || clipStart.low != clipStart.high) return false;
            if (frame.high <= clipStart.low) frame = clipStart;
            else if (frame.low < clipStart.high) return false;
            if (frame.low == frame.high && frame.low >= 0.0 && frame.low < 0x1p63 && std::floor (frame.low) == frame.low)
                absolute = pianoQuantizeTowards (from, static_cast<engine::Tick> (frame.low), strength);
            else
            {
                TimelineFramePoint point;
                point.integerOrigin = from;
                const auto delta = engine::click_detail::add (frame, timelineSignedInteger (-from));
                const auto common = std::gcd (static_cast<unsigned> (strength), 100u);
                const double numerator = strength / common, denominator = 100u / common;
                point.offset = engine::click_detail::divide (engine::click_detail::multiply (delta,
                    { numerator, numerator, false }), { denominator, denominator, false });
                if (timelineNativeFrameTarget (clock, engine::TimeBase::SampleLocked, point, absolute) != TimelineCoordinateStatus::Ok)
                    return false;
            }
        }
        out = std::max<engine::Tick> (0, absolute - origin);
        return true;
    }

    [[nodiscard]] std::vector<PianoNativeGridLine> lines (engine::Tick first, engine::Tick last,
                                                        double spacing, PianoGridDivision snap, bool snapOn,
                                                        bool includeMeter = true) const
    {
        std::vector<PianoNativeGridLine> result;
        if (clock == nullptr || first < 0 || last < first || ! std::isfinite (spacing) || spacing <= 0.0)
            return result;
        const auto samples = std::min<std::uint64_t> (2048, static_cast<std::uint64_t> (
            std::min (2048.0, std::ceil (static_cast<double> (last - first) / spacing))) + 1u);
        struct Candidate { engine::RationalTick tick; MusicalGridLevel level; };
        std::vector<Candidate> candidatesToPaint;
        for (const auto level : { MusicalGridLevel::Bar, MusicalGridLevel::Beat, MusicalGridLevel::Sixteenth })
        {
            if ((! includeMeter || level == MusicalGridLevel::Sixteenth) && (! snapOn || level != snap.level)) continue;
            if (! includeMeter && level != snap.level) continue;
            const PianoGridDivision division = level == snap.level && snapOn ? snap : PianoGridDivision { level, 0 };
            std::vector<engine::RationalTick> options;
            for (std::uint64_t i = 0; i <= samples; ++i)
            {
                const auto distance = last - first;
                const auto offset = static_cast<engine::Tick> ((static_cast<std::uint64_t> (distance) / samples) * i
                    + (static_cast<std::uint64_t> (distance) % samples) * i / samples);
                options.clear();
                if (! candidates (first + offset, division, 0, options)) return {};
                for (const auto tick : options) candidatesToPaint.push_back ({ tick, level });
            }
        }
        std::sort (candidatesToPaint.begin(), candidatesToPaint.end(), [] (const Candidate& a, const Candidate& b) {
            return musical_grid_detail::before (a.tick, b.tick)
                || (! musical_grid_detail::before (b.tick, a.tick) && a.level < b.level);
        });
        candidatesToPaint.erase (std::unique (candidatesToPaint.begin(), candidatesToPaint.end(), [] (const Candidate& a, const Candidate& b) {
            return a.level == b.level && ! musical_grid_detail::before (a.tick, b.tick) && ! musical_grid_detail::before (b.tick, a.tick);
        }), candidatesToPaint.end());
        std::set<engine::Tick> keptTicks;
        for (const auto level : { MusicalGridLevel::Bar, MusicalGridLevel::Beat, MusicalGridLevel::Sixteenth })
            for (const auto& candidate : candidatesToPaint)
            {
                if (candidate.level != level) continue;
                engine::Tick local = 0;
                if (! nativeTick (candidate.tick, local)) return {};
                if (local < first || local > last) continue;
                const auto next = keptTicks.lower_bound (local);
                if (next != keptTicks.end() && static_cast<double> (*next - local) < spacing) continue;
                if (next != keptTicks.begin() && static_cast<double> (local - *std::prev (next)) < spacing) continue;
                keptTicks.insert (next, local);
                result.push_back ({ local, level });
            }
        std::sort (result.begin(), result.end(), [] (const auto& a, const auto& b) { return a.tick < b.tick; });
        return result;
    }
};

}
