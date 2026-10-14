// YES DAW - the musical grid: bar, beat and sixteenth lines under the full tempo and meter maps.
//
// The layout pass, step 13: the timeline's grid lines and its Grid snap follow the music - the bars the ruler numbers,
// the beats inside them, sixteenths when zoomed in - through every tempo and meter change. One law for the paint, the
// snap and their gates. JUCE-free (engine time types only).

#pragma once

#include "engine/Time.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace yesdaw::ui {

enum class MusicalGridLevel : std::uint8_t
{
    Bar,
    Beat,
    Sixteenth
};

struct MusicalGridLine
{
    double frame = 0.0;
    MusicalGridLevel level = MusicalGridLevel::Bar;
};

namespace musical_grid_detail {

// The meter in force at a musical tick (the engine's meter walk).
[[nodiscard]] inline engine::MeterChange meterAt (engine::MeterMapView meterMap, engine::Tick tick) noexcept
{
    return engine::detail::walkMeterMap (meterMap, [tick] (engine::Tick changeTick, std::int64_t) noexcept {
               return changeTick > tick;
           }).meter;
}

[[nodiscard]] inline double beatTicks (engine::MeterMapView meterMap, engine::Tick tick) noexcept
{
    return engine::detail::meterBeatTicks (meterAt (meterMap, tick));
}

inline constexpr double kSixteenthTicks = static_cast<double> (engine::kTicksPerQuarter) / 4.0;

} // namespace musical_grid_detail

// Every grid line from `startFrame` to `endFrame`, at least `minSpacingFrames` apart: bar starts (every 2^k-th bar when
// a bar is narrower than the spacing), the beats inside a bar when a beat is wide enough, and the sixteenths between
// them when a sixteenth is. A bar a meter change cuts short keeps only the lines inside it. At most `maxLines`.
[[nodiscard]] inline std::vector<MusicalGridLine> musicalGridLines (const engine::CompiledTempoMap& tempoMap,
                                                                    engine::MeterMapView meterMap,
                                                                    double startFrame, double endFrame,
                                                                    double minSpacingFrames, std::size_t maxLines = 4096)
{
    using namespace musical_grid_detail;
    std::vector<MusicalGridLine> lines;
    if (! (endFrame > startFrame) || ! (minSpacingFrames > 0.0))
        return lines;
    engine::Tick leftTick = 0;
    if (! tempoMap.tickForFrame (std::max (0.0, startFrame), leftTick))
        return lines;
    std::int64_t bar = std::max<std::int64_t> (1, engine::barBeatAtTick (meterMap, leftTick).bar);
    const auto frameAt = [&tempoMap] (engine::Tick tick, double& frame) { return tempoMap.frameForTick (tick, frame); };

    // The bar step: the smallest power of two whose nominal bars at the left edge clear the spacing.
    std::int64_t step = 1;
    {
        engine::Tick barTick = 0;
        double barFrame = 0.0, nominalEnd = 0.0;
        const auto nominal = static_cast<engine::Tick> (std::llround (engine::nominalBarTicks (meterMap, leftTick)));
        if (engine::barStartTick (meterMap, bar, barTick) && frameAt (barTick, barFrame) && frameAt (barTick + nominal, nominalEnd))
            while (static_cast<double> (step) * (nominalEnd - barFrame) < minSpacingFrames && step < (std::int64_t { 1 } << 40))
                step *= 2;
    }
    bar = ((bar - 1) / step) * step + 1;

    for (std::size_t guard = 0; guard < maxLines && lines.size() < maxLines; ++guard)
    {
        engine::Tick barTick = 0, nextBarTick = 0;
        double barFrame = 0.0;
        if (! engine::barStartTick (meterMap, bar, barTick) || ! frameAt (barTick, barFrame))
            break;
        if (barFrame > endFrame)
            break;
        if (barFrame >= startFrame)
            lines.push_back ({ barFrame, MusicalGridLevel::Bar });
        if (! engine::barStartTick (meterMap, bar + 1, nextBarTick))
            break;
        if (step == 1)
        {
            const double beat = beatTicks (meterMap, barTick);
            double beatEnd = 0.0;
            if (frameAt (barTick + static_cast<engine::Tick> (std::llround (beat)), beatEnd) && beatEnd - barFrame >= minSpacingFrames)
            {
                double sixteenthEnd = 0.0;
                const bool sixteenths = kSixteenthTicks < beat
                                     && frameAt (barTick + static_cast<engine::Tick> (std::llround (kSixteenthTicks)), sixteenthEnd)
                                     && sixteenthEnd - barFrame >= minSpacingFrames;
                const double unit = sixteenths ? kSixteenthTicks : beat;
                for (int n = 1; lines.size() < maxLines; ++n)
                {
                    const double offset = unit * static_cast<double> (n);
                    const auto tick = barTick + static_cast<engine::Tick> (std::llround (offset));
                    if (tick >= nextBarTick)
                        break;
                    double frame = 0.0;
                    if (! frameAt (tick, frame) || frame > endFrame)
                        break;
                    const double inBeats = offset / beat;
                    const bool onBeat = std::abs (inBeats - std::round (inBeats)) < 1.0e-9;
                    if (frame >= startFrame)
                        lines.push_back ({ frame, onBeat ? MusicalGridLevel::Beat : MusicalGridLevel::Sixteenth });
                }
            }
        }
        bar += step;
    }
    return lines;
}

// The Grid snap under the maps: the frame of the nearest line of `level` (halved `subdivisions` times - the zoom's finer
// cells) to `frame`, counted from the start of the bar `frame` is in and never past the next bar's start. nullopt when
// the maps cannot place it.
[[nodiscard]] inline std::optional<std::int64_t> snapFrameToMusicalGrid (const engine::CompiledTempoMap& tempoMap,
                                                                         engine::MeterMapView meterMap, double frame,
                                                                         MusicalGridLevel level, int subdivisions)
{
    using namespace musical_grid_detail;
    engine::Tick tick = 0;
    if (! tempoMap.tickForFrame (std::max (0.0, frame), tick))
        return std::nullopt;
    const std::int64_t bar = engine::barBeatAtTick (meterMap, tick).bar;
    engine::Tick barTick = 0, nextBarTick = 0;
    if (! engine::barStartTick (meterMap, bar, barTick) || ! engine::barStartTick (meterMap, bar + 1, nextBarTick))
        return std::nullopt;
    double unit = level == MusicalGridLevel::Bar  ? engine::detail::meterBarTicks (meterAt (meterMap, barTick))
                : level == MusicalGridLevel::Beat ? beatTicks (meterMap, barTick)
                                                  : kSixteenthTicks;
    unit /= static_cast<double> (std::int64_t { 1 } << std::clamp (subdivisions, 0, 16));
    if (! (unit > 0.0))
        return std::nullopt;
    const double cells = std::round (static_cast<double> (tick - barTick) / unit);
    const auto snapped = std::min (nextBarTick, barTick + static_cast<engine::Tick> (std::llround (cells * unit)));
    double snappedFrame = 0.0;
    if (! tempoMap.frameForTick (snapped, snappedFrame) || ! std::isfinite (snappedFrame))
        return std::nullopt;
    return static_cast<std::int64_t> (std::llround (snappedFrame));
}

} // namespace yesdaw::ui
