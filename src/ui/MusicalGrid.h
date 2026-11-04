// YES DAW - checked musical grid positions in absolute Project frames. JUCE-free.
#pragma once
#include "engine/Time.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <vector>
namespace yesdaw::ui {
enum class MusicalGridLevel : std::uint8_t { Bar, Beat, Sixteenth };
struct MusicalGridLine { double frame = 0.0; MusicalGridLevel level = MusicalGridLevel::Bar; engine::Tick bar = 1; };
enum class MusicalGridStatus : std::uint8_t { Ok, InvalidInput, InvalidMap, Overflow, NumericDomain, ResourceLimit };
struct MusicalGridResult { MusicalGridStatus status = MusicalGridStatus::Ok; std::vector<MusicalGridLine> lines; };
namespace musical_grid_detail {
[[nodiscard]] inline MusicalGridStatus status (engine::TimeLookupStatus value) noexcept
{
    switch (value)
    {
        case engine::TimeLookupStatus::Ok: return MusicalGridStatus::Ok;
        case engine::TimeLookupStatus::InvalidInput: return MusicalGridStatus::InvalidInput;
        case engine::TimeLookupStatus::InvalidMap: return MusicalGridStatus::InvalidMap;
        case engine::TimeLookupStatus::Overflow: return MusicalGridStatus::Overflow;
        case engine::TimeLookupStatus::NumericDomain: return MusicalGridStatus::NumericDomain;
    }
    return MusicalGridStatus::InvalidInput;
}
[[nodiscard]] inline engine::MusicalTime musicalTime (engine::RationalTick tick) noexcept
{
    return { tick.whole, static_cast<double> (tick.numerator) / tick.denominator };
}
[[nodiscard]] inline bool before (engine::RationalTick a, engine::RationalTick b) noexcept
{
    return a.whole < b.whole || (a.whole == b.whole
        && std::uint64_t { a.numerator } * b.denominator < std::uint64_t { b.numerator } * a.denominator);
}
// Add an indexed rational offset to its original origin, never a rounded period.
[[nodiscard]] inline MusicalGridStatus offset (engine::RationalTick origin, std::uint64_t ordinal,
    std::uint64_t span, std::uint32_t denominator, engine::RationalTick& out) noexcept
{
    out = {};
    std::uint64_t whole = 0, remainder = 0;
    if (! engine::detail::meterMulDiv (ordinal, span, denominator, whole, remainder))
        return MusicalGridStatus::Overflow;
    const auto common = std::uint64_t { denominator / std::gcd (denominator, origin.denominator) } * origin.denominator;
    if (common > std::numeric_limits<std::uint32_t>::max())
        return MusicalGridStatus::Overflow;
    const auto fraction = remainder * (common / denominator) + origin.numerator * (common / origin.denominator);
    const auto available = static_cast<std::uint64_t> (std::numeric_limits<engine::Tick>::max() - origin.whole);
    if (whole > available || fraction / common > available - whole)
        return MusicalGridStatus::Overflow;
    out = { origin.whole + static_cast<engine::Tick> (whole + fraction / common),
            static_cast<std::uint32_t> (fraction % common), static_cast<std::uint32_t> (common) };
    return MusicalGridStatus::Ok;
}
[[nodiscard]] inline std::uint64_t ordinalAt (engine::MusicalTime time, engine::RationalTick origin,
    std::uint64_t span, std::uint32_t denominator) noexcept
{
    const double local = static_cast<double> (time.tick - origin.whole) + time.frac
        - static_cast<double> (origin.numerator) / origin.denominator;
    const double ordinal = std::max (0.0, local) / static_cast<double> (span) * denominator;
    return ordinal < 0x1p64 ? static_cast<std::uint64_t> (std::floor (ordinal)) : std::numeric_limits<std::uint64_t>::max();
}
[[nodiscard]] inline MusicalGridStatus frameAt (const engine::CompiledTempoMap& tempo, engine::RationalTick tick, double& frame) noexcept
{
    return status (tempo.frameForMusicalTime (musicalTime (tick), frame));
}
struct Bar
{
    engine::RationalTick start, end;
    engine::MeterPosition position;
    engine::MeterChange meter;
    double frame = 0.0, endFrame = 0.0;
};
[[nodiscard]] inline MusicalGridStatus barAt (const engine::CompiledTempoMap& tempo, const engine::CompiledMeterMap& meters,
    engine::MeterMapView view, engine::Tick number, Bar& out) noexcept
{
    auto result = status (meters.barStart (number, out.start));
    if (result != MusicalGridStatus::Ok) return result;
    result = status (meters.positionForMusicalTime (musicalTime (out.start), out.position));
    if (result != MusicalGridStatus::Ok) return result;
    out.meter = view.empty() ? engine::MeterChange {} : view.changes[out.position.segment];
    result = frameAt (tempo, out.start, out.frame);
    if (result != MusicalGridStatus::Ok) return result;
    if (number == std::numeric_limits<engine::Tick>::max()) return MusicalGridStatus::Overflow;
    result = status (meters.barStart (number + 1, out.end));
    return result == MusicalGridStatus::Ok ? frameAt (tempo, out.end, out.endFrame) : result;
}
[[nodiscard]] inline MusicalGridStatus positionAtFrame (const engine::CompiledTempoMap& tempo,
    const engine::CompiledMeterMap& meters, double frame, engine::MeterPosition& position) noexcept
{
    engine::MusicalTime time;
    const auto result = tempo.musicalTimeForFrame (frame, time);
    return result == engine::TimeLookupStatus::Ok ? status (meters.positionForMusicalTime (time, position)) : status (result);
}
[[nodiscard]] inline MusicalGridStatus stepAt (const engine::CompiledTempoMap& tempo, const Bar& bar,
    double spacing, engine::Tick& step) noexcept
{
    engine::RationalTick end;
    auto result = offset (bar.start, 1, 61440ULL * bar.meter.numerator, bar.meter.denominator, end);
    double frame = 0.0;
    if (result == MusicalGridStatus::Ok) result = frameAt (tempo, end, frame);
    if (result != MusicalGridStatus::Ok) return result;
    const double width = frame - bar.frame;
    if (! (width > 0.0)) return MusicalGridStatus::NumericDomain;
    step = 1;
    while (static_cast<double> (step) * width < spacing && step < (engine::Tick { 1 } << 60)) step *= 2;
    return static_cast<double> (step) * width >= spacing ? MusicalGridStatus::Ok : MusicalGridStatus::ResourceLimit;
}
inline void orderLines (std::vector<MusicalGridLine>& lines, double spacing)
{
    std::sort (lines.begin(), lines.end(), [] (const auto& a, const auto& b) {
        return a.frame < b.frame || (a.frame == b.frame && a.level < b.level);
    });
    std::vector<MusicalGridLine> kept;
    for (const auto level : { MusicalGridLevel::Bar, MusicalGridLevel::Beat, MusicalGridLevel::Sixteenth })
    {
        std::vector<MusicalGridLine> selected;
        double previous = -std::numeric_limits<double>::infinity();
        for (const auto& line : lines)
        {
            if (line.level != level || line.frame - previous < spacing)
                continue;
            const auto next = std::lower_bound (kept.begin(), kept.end(), line.frame,
                [] (const auto& value, double frame) { return value.frame < frame; });
            if ((next != kept.end() && next->frame - line.frame < spacing)
                || (next != kept.begin() && line.frame - (next - 1)->frame < spacing))
                continue;
            selected.push_back (line);
            previous = line.frame;
        }
        kept.insert (kept.end(), selected.begin(), selected.end());
        std::sort (kept.begin(), kept.end(), [] (const auto& a, const auto& b) { return a.frame < b.frame; });
    }
    lines = std::move (kept);
}
} // namespace musical_grid_detail
// Positions and labels retain fractional Frames. Resource failure publishes no partial grid.
[[nodiscard]] inline MusicalGridResult checkedMusicalGridLines (
    const engine::CompiledTempoMap& tempo, const engine::CompiledMeterMap& meters, engine::MeterMapView view,
    double startFrame, double endFrame, double minSpacingFrames, std::size_t maxLines = 4096, bool barsOnly = false)
{
    using namespace musical_grid_detail;
    MusicalGridResult result;
    const auto fail = [&result] (MusicalGridStatus failure) { result.status = failure; result.lines.clear(); return result; };
    const auto finish = [&result, minSpacingFrames] { orderLines (result.lines, minSpacingFrames); return result; };
    if (! std::isfinite (startFrame) || ! std::isfinite (endFrame) || ! std::isfinite (minSpacingFrames)
        || ! (endFrame > startFrame) || ! (minSpacingFrames > 0.0)) return fail (MusicalGridStatus::InvalidInput);
    if (maxLines == 0 || maxLines > 4096) return fail (MusicalGridStatus::ResourceLimit);
    if (startFrame >= 0x1p63 || endFrame >= 0x1p63) return fail (MusicalGridStatus::Overflow);
    startFrame = std::max (0.0, startFrame);
    if (endFrame < startFrame) return result;
    engine::MeterPosition position;
    auto lookup = positionAtFrame (tempo, meters, startFrame, position);
    if (lookup != MusicalGridStatus::Ok) return fail (lookup);
    engine::Tick number = position.barBeat.bar;
    Bar bar;
    lookup = barAt (tempo, meters, view, number, bar);
    if (lookup != MusicalGridStatus::Ok) return fail (lookup);
    engine::Tick step = 1;
    if ((lookup = stepAt (tempo, bar, minSpacingFrames, step)) != MusicalGridStatus::Ok) return fail (lookup);
    number = ((number - 1) / step) * step + 1;
    std::size_t work = 0;
    const std::size_t workLimit = maxLines * 16u + 128u;
    const auto append = [&] (double frame, MusicalGridLevel level, engine::Tick barNumber) {
        if (frame < startFrame || frame > endFrame) return true;
        if (result.lines.size() >= maxLines) return false;
        result.lines.push_back ({ frame, level, barNumber });
        return true;
    };
    double previousBarFrame = -std::numeric_limits<double>::infinity();
    while (++work <= workLimit)
    {
        if ((lookup = barAt (tempo, meters, view, number, bar)) != MusicalGridStatus::Ok) return fail (lookup);
        if (bar.frame > endFrame) return finish();
        if ((lookup = stepAt (tempo, bar, minSpacingFrames, step)) != MusicalGridStatus::Ok) return fail (lookup);
        // A change's accented bar wins a label collision with the preceding truncated bar.
        engine::MeterPosition after;
        lookup = status (meters.positionForMusicalTime (musicalTime (bar.end), after));
        if (lookup != MusicalGridStatus::Ok) return fail (lookup);
        const bool startsAtChange = bar.start.whole == bar.meter.tick && bar.start.numerator == 0;
        const bool changeWins = ! startsAtChange && after.segment != bar.position.segment && bar.endFrame - bar.frame < minSpacingFrames;
        if (! changeWins && bar.frame - previousBarFrame >= minSpacingFrames)
        {
            if (! append (bar.frame, MusicalGridLevel::Bar, number)) return fail (MusicalGridStatus::ResourceLimit);
            previousBarFrame = bar.frame;
        }
        if (! barsOnly && step == 1)
        {
            const auto enumerate = [&] (std::uint64_t span, std::uint32_t denominator, MusicalGridLevel level) {
                engine::RationalTick first;
                auto code = offset (bar.start, 1, span, denominator, first);
                double firstFrame = 0.0;
                if (code == MusicalGridStatus::Ok) code = frameAt (tempo, first, firstFrame);
                if (code != MusicalGridStatus::Ok || firstFrame - bar.frame < minSpacingFrames) return code;
                engine::MusicalTime left;
                code = status (tempo.musicalTimeForFrame (std::max (bar.frame, startFrame), left));
                if (code != MusicalGridStatus::Ok) return code;
                std::uint64_t ordinal = std::max<std::uint64_t> (1, ordinalAt (left, bar.start, span, denominator));
                while (++work <= workLimit)
                {
                    engine::RationalTick tick;
                    code = offset (bar.start, ordinal, span, denominator, tick);
                    if (code != MusicalGridStatus::Ok || ! before (tick, bar.end)) return code;
                    double frame = 0.0;
                    code = frameAt (tempo, tick, frame);
                    if (code != MusicalGridStatus::Ok || frame > endFrame) return code;
                    if (! append (frame, level, number)) return MusicalGridStatus::ResourceLimit;
                    if (ordinal == std::numeric_limits<std::uint64_t>::max()) return MusicalGridStatus::Overflow;
                    ++ordinal;
                    // Jump over dense ordinals by inverse lookup, rather than walking every beat.
                    const double seek = frame + minSpacingFrames;
                    if (seek < bar.endFrame && seek < endFrame)
                    {
                        engine::MusicalTime next;
                        code = status (tempo.musicalTimeForFrame (seek, next));
                        if (code != MusicalGridStatus::Ok) return code;
                        ordinal = std::max (ordinal, ordinalAt (next, bar.start, span, denominator));
                    }
                }
                return MusicalGridStatus::ResourceLimit;
            };
            lookup = enumerate (61440, bar.meter.denominator, MusicalGridLevel::Beat);
            if (lookup != MusicalGridStatus::Ok) return fail (lookup);
            lookup = enumerate (3840, 1, MusicalGridLevel::Sixteenth);
            if (lookup != MusicalGridStatus::Ok) return fail (lookup);
        }
        // Inverse-index to the spacing horizon, then align to this power-of-two bar step.
        const double seek = std::max (bar.frame + minSpacingFrames, startFrame);
        engine::Tick next = number;
        if (seek <= endFrame)
        {
            lookup = positionAtFrame (tempo, meters, seek, position);
            if (lookup != MusicalGridStatus::Ok) return fail (lookup);
            next = std::max (next, position.barBeat.bar - 1);
        }
        if ((next - 1) / step >= (std::numeric_limits<engine::Tick>::max() - 1) / step) return fail (MusicalGridStatus::Overflow);
        next = ((next - 1) / step + 1) * step + 1;
        if (changeWins) next = number + 1;
        number = next;
    }
    return fail (MusicalGridStatus::ResourceLimit);
}
[[nodiscard]] inline std::vector<MusicalGridLine> musicalGridLines (
    const engine::CompiledTempoMap& tempo, engine::MeterMapView view, double startFrame, double endFrame,
    double minSpacingFrames, std::size_t maxLines = 4096)
{
    engine::CompiledMeterMap meters;
    if (engine::CompiledMeterMap::build (view, meters) != engine::TimeLookupStatus::Ok) return {};
    return checkedMusicalGridLines (tempo, meters, view, startFrame, endFrame, minSpacingFrames, maxLines).lines;
}
[[nodiscard]] inline MusicalGridStatus checkedSnapFrameToMusicalGrid (
    const engine::CompiledTempoMap& tempo, const engine::CompiledMeterMap& meters, engine::MeterMapView view,
    double frame, MusicalGridLevel level, int subdivisions, std::int64_t& out) noexcept
{
    using namespace musical_grid_detail;
    out = 0;
    if (! std::isfinite (frame) || frame < 0.0 || subdivisions < 0 || subdivisions > 16) return MusicalGridStatus::InvalidInput;
    engine::MusicalTime time;
    auto code = status (tempo.musicalTimeForFrame (frame, time));
    engine::MeterPosition position;
    if (code == MusicalGridStatus::Ok) code = status (meters.positionForMusicalTime (time, position));
    Bar bar;
    if (code == MusicalGridStatus::Ok) code = barAt (tempo, meters, view, position.barBeat.bar, bar);
    if (code != MusicalGridStatus::Ok) return code;
    double best = bar.frame;
    const auto consider = [&] (double candidate) {
        if (std::abs (candidate - frame) < std::abs (best - frame)
            || (std::abs (candidate - frame) == std::abs (best - frame) && candidate > best)) best = candidate;
    };
    consider (bar.endFrame);
    const auto candidates = [&] (std::uint64_t span, std::uint32_t denominator) {
        denominator *= std::uint32_t { 1 } << subdivisions;
        const auto middle = ordinalAt (time, bar.start, span, denominator);
        const auto first = middle > 1 ? middle - 1 : 0;
        for (std::uint64_t n = first; n - first < 4; ++n)
        {
            engine::RationalTick tick;
            auto result = offset (bar.start, n, span, denominator, tick);
            if (result != MusicalGridStatus::Ok) return result;
            if (! before (tick, bar.end)) break;
            double mapped = 0.0;
            result = frameAt (tempo, tick, mapped);
            if (result != MusicalGridStatus::Ok) return result;
            consider (mapped);
        }
        return MusicalGridStatus::Ok;
    };
    switch (level)
    {
        case MusicalGridLevel::Bar: code = candidates (61440ULL * bar.meter.numerator, bar.meter.denominator); break;
        case MusicalGridLevel::Beat: code = candidates (61440, bar.meter.denominator); break;
        case MusicalGridLevel::Sixteenth:
            code = candidates (3840, 1);
            if (code == MusicalGridStatus::Ok) code = candidates (61440, bar.meter.denominator);
            break;
        default: return MusicalGridStatus::InvalidInput;
    }
    if (code != MusicalGridStatus::Ok) return code;
    if (! std::isfinite (best) || best < 0.0 || best >= 0x1p63) return MusicalGridStatus::Overflow;
    const double whole = std::floor (best);
    return status (engine::roundMusicalTime ({ static_cast<engine::Tick> (whole), best - whole }, engine::TimeRounding::Nearest, out));
}
[[nodiscard]] inline std::optional<std::int64_t> snapFrameToMusicalGrid (
    const engine::CompiledTempoMap& tempo, engine::MeterMapView view, double frame, MusicalGridLevel level, int subdivisions)
{
    engine::CompiledMeterMap meters;
    std::int64_t out = 0;
    if (engine::CompiledMeterMap::build (view, meters) != engine::TimeLookupStatus::Ok
        || checkedSnapFrameToMusicalGrid (tempo, meters, view, frame, level, subdivisions, out) != MusicalGridStatus::Ok) return std::nullopt;
    return out;
}
[[nodiscard]] inline MusicalGridStatus checkedMusicalBarBeatForFrame (const engine::CompiledTempoMap& tempo,
    const engine::CompiledMeterMap& meters, double frame, engine::BarBeat& out) noexcept
{
    out = { 0, 0 };
    engine::MeterPosition position;
    const auto result = musical_grid_detail::positionAtFrame (tempo, meters, frame, position);
    if (result == MusicalGridStatus::Ok)
        out = position.barBeat;
    return result;
}
} // namespace yesdaw::ui
