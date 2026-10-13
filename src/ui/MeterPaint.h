#pragma once

// YES DAW - the meters' painters (moved from MainComponentInternal.h, UI pass unit 24, so gates can draw them directly).
// A meter shows a level `value` as a fraction of its own scale; its yellow zone starts at UiTheme::Meter::hotFromDb on that
// same scale (`hotFrom`) - a level, never a share of the live bar.

#include "ui/UiTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

namespace yesdaw::ui::shell {

// UI pass unit 24: where a linear-peak meter's (the strips', the rail's, the header's) hot zone begins - Meter::hotFromDb as
// a fraction of its scale. The master meter's scale is its own (masterMeterFractionForDb).
[[nodiscard]] inline float linearMeterHotFraction() noexcept
{
    return std::pow (10.0f, yesdaw::ui::UiTheme::Meter::hotFromDb / 20.0f);
}

// A meter's level `value` and its hot zone's start `hotFrom`, both fractions of its scale: a segment reaching into the hot
// zone paints yellow, the rest green - the zone is a level, so a quiet bar is all green.
inline void drawMeter (juce::Graphics& g, juce::Rectangle<int> area, float value, float hotFrom = linearMeterHotFraction())
{
    g.setColour (yesdaw::ui::UiTheme::Color::meterTrack());
    g.fillRoundedRectangle (area.toFloat(), yesdaw::ui::UiTheme::Radius::xs);

    const auto fill = area.reduced (yesdaw::ui::UiTheme::Layout::meterFillInset);
    const int height = juce::roundToInt (
        static_cast<float> (fill.getHeight()) * juce::jlimit (0.0f, 1.0f, value));
    const int liveTop = fill.getBottom() - height;
    const int hotBottom = fill.getBottom() - juce::roundToInt (
        static_cast<float> (fill.getHeight()) * juce::jlimit (0.0f, 1.0f, hotFrom));
    const int segmentStep = yesdaw::ui::UiTheme::Layout::meterSegmentSize
                          + yesdaw::ui::UiTheme::Layout::meterSegmentGap;
    for (int bottom = fill.getBottom(); bottom > liveTop; bottom -= segmentStep)
    {
        const int top = juce::jmax (liveTop,
                                    bottom - yesdaw::ui::UiTheme::Layout::meterSegmentSize);
        g.setColour (top < hotBottom ? yesdaw::ui::UiTheme::Meter::hotFill()
                                    : yesdaw::ui::UiTheme::Meter::nominalFill());
        g.fillRect (fill.getX(), top, fill.getWidth(), bottom - top);
    }
}

// B32: vertical meter with a peak-hold marker and a latched clip light. The live bar paints
// exactly like drawMeter; the held peak paints as a marker line while its hold lasts; the clip
// light fills the meter's top cell while latched.
inline void drawMeterWithHold (juce::Graphics& g,
                        juce::Rectangle<int> area,
                        float liveValue,
                        float heldValue,
                        bool clipLatched,
                        float hotFrom = linearMeterHotFraction())
{
    drawMeter (g, area, liveValue, hotFrom);

    const auto fill = area.reduced (yesdaw::ui::UiTheme::Layout::meterFillInset);
    if (heldValue > 0.0f)
    {
        const int heldHeight = juce::roundToInt (
            static_cast<float> (fill.getHeight()) * juce::jlimit (0.0f, 1.0f, heldValue));
        const int hotBottom = fill.getBottom() - juce::roundToInt (
            static_cast<float> (fill.getHeight()) * juce::jlimit (0.0f, 1.0f, hotFrom));
        g.setColour (fill.getBottom() - heldHeight < hotBottom ? yesdaw::ui::UiTheme::Meter::hotFill()   // unit 24: as the bar
                                                               : yesdaw::ui::UiTheme::Meter::nominalFill());
        g.fillRect (fill.getX(),
                    fill.getBottom() - heldHeight,
                    fill.getWidth(),
                    yesdaw::ui::UiTheme::Meter::peakTickThickness);
    }

    if (clipLatched)
    {
        g.setColour (yesdaw::ui::UiTheme::Meter::clipFill());
        g.fillRect (fill.getX(), area.getY(), fill.getWidth(),
                    yesdaw::ui::UiTheme::Meter::clipLightSize);
    }
}

inline void drawHorizontalMeter (juce::Graphics& g, juce::Rectangle<int> area, float value,
                                 float hotFrom = linearMeterHotFraction())
{
    g.setColour (yesdaw::ui::UiTheme::Color::meterTrack());
    g.fillRoundedRectangle (area.toFloat(), yesdaw::ui::UiTheme::Radius::xs);
    const auto fill = area.reduced (yesdaw::ui::UiTheme::Layout::meterFillInset);
    const int width = juce::roundToInt (
        static_cast<float> (fill.getWidth()) * juce::jlimit (0.0f, 1.0f, value));
    const int liveRight = fill.getX() + width;
    const int hotLeft = fill.getX() + juce::roundToInt (   // unit 24: the hot zone is a level, as drawMeter's
        static_cast<float> (fill.getWidth()) * juce::jlimit (0.0f, 1.0f, hotFrom));
    const int segmentStep = yesdaw::ui::UiTheme::Layout::meterSegmentSize
                          + yesdaw::ui::UiTheme::Layout::meterSegmentGap;
    for (int left = fill.getX(); left < liveRight; left += segmentStep)
    {
        const int right = juce::jmin (liveRight,
                                      left + yesdaw::ui::UiTheme::Layout::meterSegmentSize);
        g.setColour (right > hotLeft ? yesdaw::ui::UiTheme::Meter::hotFill()
                                     : yesdaw::ui::UiTheme::Meter::nominalFill());
        g.fillRect (left, fill.getY(), right - left, fill.getHeight());
    }
}

} // namespace yesdaw::ui::shell
