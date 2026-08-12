// YES DAW - JUCE-free UI layout tokens shared by pure layout helpers.

#pragma once

namespace yesdaw::ui {

struct UiThemeLayout
{
    static constexpr double timelineLayoutDefaultPixelsPerSecond = 100.0;
    static constexpr double timelineLayoutDefaultWidthPixels = 1280.0;
    static constexpr double timelineLayoutDefaultLaneHeightPixels = 64.0;
    static constexpr double timelineLayoutZeroFloor = 0.0;
    static constexpr double timelineClipDefaultFadeSeconds = 0.01;
    // Piano-roll viewport defaults (E10): the historical C3-anchored 25-key window; the full
    // 0-127 range scrolls behind it.
    static constexpr int pianoRollDefaultLowKey = 48;
    static constexpr int pianoRollKeyMin = 0;
    static constexpr int pianoRollKeyMax = 127;
    static constexpr double pianoRollZoomMin = 1.0;
    static constexpr double pianoRollZoomMax = 64.0;
};

} // namespace yesdaw::ui
