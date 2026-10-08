#pragma once

#include "ui/UiTheme.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>

namespace yesdaw::ui {

// ADR-0072 §1 / §8: a control's hover or pressed state, one form for the shell's painted controls and the native
// widgets alike - pressed replaces hover; neither paints nothing.
struct PointerStrokeForm
{
    int width = 0;   // 0: no stroke
    float alpha = 0.0f;
};

[[nodiscard]] inline PointerStrokeForm pointerStrokeForm (bool hovered, bool pressed,
                                                         int pressedWidth = UiTheme::Layout::pointerPressedStrokeWidth) noexcept
{
    if (pressed)
        return { pressedWidth, UiTheme::Tone::pressedStrokeAlpha };
    if (hovered)
        return { UiTheme::Layout::pointerHoverStrokeWidth, UiTheme::Tone::hoverStrokeAlpha };
    return {};
}

// ADR-0072 §1's text clearance for a native widget: one too short for even the smallest text (Type::tiny) to keep clear
// of the full pressed width's reach above and below (the master's 14 px DIM) presses at the close width. A hint from
// the height only: the authority is [widget-states][pointer-text-contrast], which renders every widget the shell shows
// with and without its text - a widget whose label comes closer (a larger font) fails there, whatever this returns.
[[nodiscard]] inline int pointerPressedStrokeWidthForHeight (int height) noexcept
{
    const int fullReach = UiTheme::Layout::pointerStrokeInset + UiTheme::Layout::pointerPressedStrokeWidth;
    return static_cast<float> (height) < UiTheme::Type::tiny + 2.0f * static_cast<float> (fullReach)
               ? UiTheme::Layout::pointerPressedStrokeWidthClose
               : UiTheme::Layout::pointerPressedStrokeWidth;
}

// The stroke: a white band `form.width` px wide just inside `bounds` (inset L::pointerStrokeInset) - never a fill, so no
// text and no pixel under it changes. Its corner follows `radius`, shrunk so a thin control (a meter) keeps a straight,
// full-strength middle on every edge. On a control thinner than two strokes (the rail's 3 px colour swatch) every pixel
// inside the inset is within a stroke's reach of an edge, so the band is all of them - still the stroke, never more.
inline void paintPointerStroke (juce::Graphics& g, juce::Rectangle<float> bounds, PointerStrokeForm form, float radius)
{
    if (form.width <= 0)
        return;
    const auto outer = bounds.reduced (static_cast<float> (UiTheme::Layout::pointerStrokeInset));
    if (outer.isEmpty())
        return;
    const float width = static_cast<float> (form.width);
    const auto inner = outer.reduced (width);
    const float corner = juce::jlimit (0.0f, radius, (std::min (outer.getWidth(), outer.getHeight()) - 2.0f) / 2.0f);
    juce::Path band;
    band.addRoundedRectangle (outer, corner);
    if (! inner.isEmpty())
        band.addRoundedRectangle (inner, std::max (0.0f, corner - width));   // concentric
    band.setUsingNonZeroWinding (false);
    g.setColour (UiTheme::Color::white().withAlpha (form.alpha));
    g.fillPath (band);
}

} // namespace yesdaw::ui
