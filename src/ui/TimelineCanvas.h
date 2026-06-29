// YES DAW - H11 native Timeline canvas.
//
// The app shell and the frame-time gate share this renderer so dense timeline drawing is measured
// through the same code path the user sees.

#pragma once

#include "ui/TimelineLayout.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace yesdaw::ui {

struct TimelineCanvasTrack
{
    const char* name;
    juce::Colour colour;
    float meter;
};

struct TimelineCanvasClipStyle
{
    juce::Colour colour;
    float amplitude;
};

struct TimelineMarker
{
    double seconds;
    const char* label;
};

struct TimelineCanvasState
{
    const TimelineCanvasTrack* tracks = nullptr;
    int trackCount = 0;

    const Clip* clips = nullptr;
    const TimelineCanvasClipStyle* clipStyles = nullptr;
    int clipCount = 0;

    const TimelineMarker* markers = nullptr;
    int markerCount = 0;

    Viewport viewport {};
    double totalSeconds = 96.0;
    double playheadSeconds = 32.0;
};

struct TimelineCanvasPaintStats
{
    int visibleClips = 0;
    int visibleClipCapacity = 0;
    bool hitVisibleClipCapacity = false;
};

namespace timeline_canvas_detail {

constexpr int kVisibleClipCapacity = 4096;

const juce::Colour kPanel (0xff11161c);
const juce::Colour kPanelStroke (0xff2a323b);
const juce::Colour kText (0xfff0f3f6);
const juce::Colour kMutedText (0xff9aa3ad);
const juce::Colour kGrid (0xff24303a);
const juce::Colour kCanvasBack (0xff0c1217);
const juce::Colour kToolbarBack (0xff101720);
const juce::Colour kRulerBack (0xff0b1117);
const juce::Colour kPurple (0xffa762f0);

inline void drawSmallLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                            juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour (kMutedText);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (text, area, justification, false);
}

inline void fillPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (kPanel);
    g.fillRoundedRectangle (area.toFloat(), 6.0f);
    g.setColour (kPanelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 6.0f, 1.0f);
}

inline TimelineCanvasClipStyle styleForClip (const TimelineCanvasState& state, int clipId)
{
    const TimelineCanvasClipStyle fallback { juce::Colour (0xff3b8cff), 0.7f };
    if (state.clipStyles == nullptr || state.clips == nullptr || state.clipCount <= 0)
        return fallback;

    if (clipId >= 0 && clipId < state.clipCount && state.clips[clipId].id == clipId)
        return state.clipStyles[clipId];

    for (int i = 0; i < state.clipCount; ++i)
        if (state.clips[i].id == clipId)
            return state.clipStyles[i];

    return fallback;
}

inline void drawClipWaveform (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
                              float amplitude, int clipId)
{
    area.reduce (7, 5);
    if (area.isEmpty())
        return;

    const float midY = static_cast<float> (area.getCentreY());
    const float half = static_cast<float> (area.getHeight()) * std::clamp (amplitude, 0.1f, 1.0f) * 0.42f;
    const int step = std::max (8, area.getWidth() / 9);

    g.setColour (colour.brighter (0.42f));
    for (int x = 0; x < area.getWidth(); x += step)
    {
        const int phase = (clipId * 37 + x * 13) & 31;
        const float scaled = 0.32f + static_cast<float> (phase) / 31.0f * 0.68f;
        const float top = midY - half * scaled;
        const float bottom = midY + half * scaled;
        g.drawVerticalLine (area.getX() + x, top, bottom);
    }
}

inline void drawClip (juce::Graphics& g, juce::Rectangle<int> area, const TimelineCanvasClipStyle& style,
                      int clipId)
{
    if (area.getWidth() <= 2 || area.getHeight() <= 2)
        return;

    g.setColour (style.colour.withAlpha (0.42f));
    g.fillRoundedRectangle (area.toFloat(), 4.0f);
    g.setColour (style.colour.brighter (0.35f));
    g.drawRoundedRectangle (area.toFloat().reduced (0.5f), 4.0f, 1.0f);
    drawClipWaveform (g, area, style.colour, style.amplitude, clipId);
}

inline void drawToolbar (juce::Graphics& g, juce::Rectangle<int> toolbar)
{
    g.setColour (kToolbarBack);
    g.fillRect (toolbar);

    auto tools = toolbar.withTrimmedLeft (16).withWidth (190).reduced (0, 6);
    for (const auto* label : { "A", "P", "E", "S", "L" })
    {
        auto cell = tools.removeFromLeft (34).reduced (3, 0);
        g.setColour (juce::Colour (0xff080d12));
        g.fillRoundedRectangle (cell.toFloat(), 3.0f);
        g.setColour (kMutedText);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (label, cell, juce::Justification::centred, false);
    }

    drawSmallLabel (g, "SNAP", toolbar.withTrimmedLeft (234).withWidth (42), juce::Justification::centred);
    g.setColour (juce::Colour (0xff070b0f));
    g.fillRoundedRectangle (toolbar.withTrimmedLeft (276).withWidth (80).reduced (0, 7).toFloat(), 3.0f);
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("Bar", toolbar.withTrimmedLeft (284).withWidth (54), juce::Justification::centredLeft, false);
}

inline void drawRuler (juce::Graphics& g, juce::Rectangle<int> ruler, juce::Rectangle<int> clipArea,
                       const TimelineCanvasState& state, const Viewport& vp)
{
    g.setColour (kRulerBack);
    g.fillRect (ruler);
    g.setColour (kGrid);
    g.fillRect (ruler.withHeight (1).withY (ruler.getBottom() - 1));

    const double rightSeconds = vp.scrollSeconds + static_cast<double> (clipArea.getWidth()) / vp.pixelsPerSecond;
    const double labelStep = vp.pixelsPerSecond < 24.0 ? 8.0 : 4.0;
    const double firstLabel = std::floor (vp.scrollSeconds / labelStep) * labelStep;

    for (double seconds = firstLabel; seconds <= rightSeconds + labelStep; seconds += labelStep)
    {
        const int x = clipArea.getX() + juce::roundToInt ((seconds - vp.scrollSeconds) * vp.pixelsPerSecond);
        if (x < clipArea.getX() - 40 || x > clipArea.getRight() + 40)
            continue;

        const int barNumber = std::max (1, juce::roundToInt (seconds) + 1);
        g.setColour (kMutedText);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (juce::String (barNumber), x - 18, ruler.getY() + 7, 36, 16,
                    juce::Justification::centred, false);
        g.setColour (kMutedText.withAlpha (0.65f));
        g.fillRect (x, ruler.getBottom() - 14, 1, 14);
    }

    if (state.markers == nullptr)
        return;

    for (int i = 0; i < state.markerCount; ++i)
    {
        const auto& marker = state.markers[i];
        const int x = clipArea.getX() + juce::roundToInt ((marker.seconds - vp.scrollSeconds) * vp.pixelsPerSecond);
        if (x < clipArea.getX() - 60 || x > clipArea.getRight())
            continue;

        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (marker.label, x + 4, ruler.getY() + 24, 76, 16,
                    juce::Justification::centredLeft, false);
    }
}

inline void drawGrid (juce::Graphics& g, juce::Rectangle<int> clipArea, const TimelineCanvasState& state,
                      const Viewport& vp, int laneHeight)
{
    g.setColour (kCanvasBack);
    g.fillRect (clipArea);

    const int laneCount = std::max (1, state.trackCount);
    for (int lane = 0; lane <= laneCount; ++lane)
    {
        const int y = clipArea.getY() + lane * laneHeight;
        g.setColour (kGrid.withAlpha (0.7f));
        g.fillRect (clipArea.getX(), y, clipArea.getWidth(), 1);

        if (lane < state.trackCount && state.tracks != nullptr)
        {
            g.setColour (state.tracks[lane].colour.withAlpha (0.20f));
            g.fillRect (clipArea.getX(), y + 1, 3, std::max (0, laneHeight - 1));
        }
    }

    const double rightSeconds = vp.scrollSeconds + static_cast<double> (clipArea.getWidth()) / vp.pixelsPerSecond;
    const double firstGrid = std::floor (vp.scrollSeconds / 4.0) * 4.0;
    for (double seconds = firstGrid; seconds <= rightSeconds + 4.0; seconds += 4.0)
    {
        const int x = clipArea.getX() + juce::roundToInt ((seconds - vp.scrollSeconds) * vp.pixelsPerSecond);
        if (x < clipArea.getX() || x > clipArea.getRight())
            continue;

        const bool major = (juce::roundToInt (seconds) % 16) == 0;
        g.setColour (major ? kGrid.brighter (0.25f) : kGrid.withAlpha (0.38f));
        g.fillRect (x, clipArea.getY(), 1, clipArea.getHeight());
    }
}

} // namespace timeline_canvas_detail

inline TimelineCanvasPaintStats paintTimelineCanvas (juce::Graphics& g, juce::Rectangle<int> area,
                                                     const TimelineCanvasState& state)
{
    using namespace timeline_canvas_detail;

    TimelineCanvasPaintStats stats;
    stats.visibleClipCapacity = kVisibleClipCapacity;

    if (area.getWidth() <= 0 || area.getHeight() <= 0)
        return stats;

    fillPanel (g, area);

    auto content = area.reduced (1);
    drawToolbar (g, content.removeFromTop (36));
    auto ruler = content.removeFromTop (48);
    auto clipArea = content.reduced (12, 0);

    const int laneCount = std::max (1, state.trackCount);
    const int laneHeight = std::max (8, clipArea.getHeight() / laneCount);

    Viewport vp = state.viewport;
    vp.pixelsPerSecond = std::max (1.0, vp.pixelsPerSecond);
    vp.widthPixels = static_cast<double> (clipArea.getWidth());
    vp.laneHeightPixels = static_cast<double> (laneHeight);

    drawRuler (g, ruler, clipArea, state, vp);
    drawGrid (g, clipArea, state, vp, laneHeight);

    std::array<ElementRect, kVisibleClipCapacity> visible {};
    if (state.clips != nullptr && state.clipCount > 0)
        stats.visibleClips = layoutVisible (state.clips, state.clipCount, vp, visible.data(),
                                            static_cast<int> (visible.size()));
    stats.hitVisibleClipCapacity = stats.visibleClips == static_cast<int> (visible.size());

    for (int i = 0; i < stats.visibleClips; ++i)
    {
        const auto& rect = visible[static_cast<std::size_t> (i)];
        const auto style = styleForClip (state, rect.id);
        auto clipRect = juce::Rectangle<int> (clipArea.getX() + juce::roundToInt (rect.x),
                                             clipArea.getY() + juce::roundToInt (rect.y),
                                             juce::roundToInt (rect.w),
                                             juce::roundToInt (rect.h))
                            .reduced (4, 5);
        clipRect = clipRect.getIntersection (clipArea);
        drawClip (g, clipRect, style, rect.id);
    }

    const int playheadX = clipArea.getX() + juce::roundToInt ((state.playheadSeconds - vp.scrollSeconds)
                                                             * vp.pixelsPerSecond);
    if (playheadX >= clipArea.getX() && playheadX <= clipArea.getRight())
    {
        g.setColour (juce::Colours::white);
        g.fillRect (playheadX, ruler.getY(), 2, clipArea.getBottom() - ruler.getY());
        g.setColour (kPurple);
        g.fillRoundedRectangle (static_cast<float> (playheadX - 15), static_cast<float> (ruler.getY() + 4),
                                30.0f, 16.0f, 7.0f);
        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (juce::String (std::max (1, juce::roundToInt (state.playheadSeconds) + 1)),
                    playheadX - 12, ruler.getY() + 4, 24, 16, juce::Justification::centred, false);
    }

    return stats;
}

} // namespace yesdaw::ui
