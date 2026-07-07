// YES DAW - H11 native Timeline canvas.
//
// The app shell and the frame-time gate share this renderer so dense timeline drawing is measured
// through the same code path the user sees.

#pragma once

#include "ui/TimelineLayout.h"
#include "ui/UiTheme.h"

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

struct TimelineCanvasGeometry
{
    juce::Rectangle<int> toolbarArea;
    juce::Rectangle<int> rulerArea;
    juce::Rectangle<int> clipArea;
    Viewport viewport;
    int laneHeight = 0;
};

namespace timeline_canvas_detail {

constexpr int kVisibleClipCapacity = UiTheme::Layout::timelineCanvasVisibleClipCapacity;

const juce::Colour kPanel = UiTheme::Color::panel();
const juce::Colour kPanelStroke = UiTheme::Color::panelStroke();
const juce::Colour kText = UiTheme::Color::text();
const juce::Colour kMutedText = UiTheme::Color::mutedText();
const juce::Colour kGrid = UiTheme::Color::timelineGrid();
const juce::Colour kCanvasBack = UiTheme::Color::timelineCanvas();
const juce::Colour kToolbarBack = UiTheme::Color::timelineToolbar();
const juce::Colour kRulerBack = UiTheme::Color::timelineRuler();
const juce::Colour kPurple = UiTheme::Color::accentPurple();

inline void drawSmallLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                            juce::Justification justification = juce::Justification::centredLeft)
{
    g.setColour (kMutedText);
    g.setFont (juce::Font (juce::FontOptions (UiTheme::Type::small)));
    g.drawText (text, area, justification, false);
}

inline void fillPanel (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (kPanel);
    g.fillRoundedRectangle (area.toFloat(), UiTheme::Radius::lg);
    g.setColour (kPanelStroke);
    g.drawRoundedRectangle (area.toFloat().reduced (UiTheme::Layout::timelineCanvasOutlineInset),
                            UiTheme::Radius::lg,
                            UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
}

inline TimelineCanvasClipStyle styleForClip (const TimelineCanvasState& state, int clipId)
{
    const TimelineCanvasClipStyle fallback { UiTheme::Color::accentBlue(), 0.7f };
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
    area.reduce (UiTheme::Layout::timelineCanvasWaveformInsetX,
                 UiTheme::Layout::timelineCanvasWaveformInsetY);
    if (area.isEmpty())
        return;

    const float midY = static_cast<float> (area.getCentreY());
    const float half = static_cast<float> (area.getHeight())
                     * std::clamp (amplitude,
                                   UiTheme::Layout::timelineCanvasWaveformMinAmplitude,
                                   UiTheme::Layout::timelineCanvasWaveformMaxAmplitude)
                     * UiTheme::Layout::timelineCanvasWaveformHeightScale;
    const int step = std::max (UiTheme::Layout::timelineCanvasWaveformMinStep,
                               area.getWidth() / UiTheme::Layout::timelineCanvasWaveformStepDivisor);

    g.setColour (colour.brighter (0.42f));
    for (int x = 0; x < area.getWidth(); x += step)
    {
        const int phase = (clipId * UiTheme::Layout::timelineCanvasWaveformClipPhaseMultiplier
                           + x * UiTheme::Layout::timelineCanvasWaveformXPhaseMultiplier)
                        & UiTheme::Layout::timelineCanvasWaveformPhaseMask;
        const float scaled = UiTheme::Layout::timelineCanvasWaveformMinScale
                           + static_cast<float> (phase)
                                 / static_cast<float> (UiTheme::Layout::timelineCanvasWaveformPhaseMask)
                                 * UiTheme::Layout::timelineCanvasWaveformScaleRange;
        const float top = midY - half * scaled;
        const float bottom = midY + half * scaled;
        g.drawVerticalLine (area.getX() + x, top, bottom);
    }
}

inline void drawClip (juce::Graphics& g, juce::Rectangle<int> area, const TimelineCanvasClipStyle& style,
                      int clipId)
{
    if (area.getWidth() <= UiTheme::Layout::timelineCanvasClipMinPaintWidth
        || area.getHeight() <= UiTheme::Layout::timelineCanvasClipMinPaintHeight)
        return;

    if (area.getHeight() <= UiTheme::Layout::timelineCanvasClipCompactHeight)
    {
        g.setColour (style.colour.withAlpha (0.44f));
        g.fillRect (area);
        g.setColour (style.colour.brighter (0.3f));
        g.fillRect (area.withHeight (UiTheme::Layout::timelineCanvasClipCompactHighlightHeight));
        return;
    }

    g.setColour (style.colour.withAlpha (0.42f));
    g.fillRoundedRectangle (area.toFloat(), UiTheme::Radius::md);
    g.setColour (style.colour.brighter (0.35f));
    g.drawRoundedRectangle (area.toFloat().reduced (UiTheme::Layout::timelineCanvasOutlineInset),
                            UiTheme::Radius::md,
                            UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
    drawClipWaveform (g, area, style.colour, style.amplitude, clipId);
}

inline void drawToolbar (juce::Graphics& g, juce::Rectangle<int> toolbar)
{
    g.setColour (kToolbarBack);
    g.fillRect (toolbar);

    auto tools = toolbar.withTrimmedLeft (UiTheme::Space::xl)
                         .withWidth (UiTheme::Layout::timelineCanvasToolbarWidth)
                         .reduced (0, UiTheme::Space::sm);
    for (const auto* label : { "A", "P", "E", "S", "L" })
    {
        auto cell = tools.removeFromLeft (UiTheme::Layout::timelineCanvasToolCellWidth)
                         .reduced (UiTheme::Space::xxs + UiTheme::Space::hairline, 0);
        g.setColour (UiTheme::Color::toolButton());
        g.fillRoundedRectangle (cell.toFloat(), UiTheme::Radius::sm);
        g.setColour (kMutedText);
        g.setFont (juce::Font (juce::FontOptions (UiTheme::Type::small)));
        g.drawText (label, cell, juce::Justification::centred, false);
    }

    drawSmallLabel (g,
                    "SNAP",
                    toolbar.withTrimmedLeft (UiTheme::Layout::timelineCanvasSnapLabelX)
                           .withWidth (UiTheme::Layout::timelineCanvasSnapLabelWidth),
                    juce::Justification::centred);
    g.setColour (UiTheme::Color::snapField());
    g.fillRoundedRectangle (toolbar.withTrimmedLeft (UiTheme::Layout::timelineCanvasSnapFieldX)
                                   .withWidth (UiTheme::Layout::timelineCanvasSnapFieldWidth)
                                   .reduced (0, UiTheme::Space::sm + 1)
                                   .toFloat(),
                            UiTheme::Radius::sm);
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (UiTheme::Type::body)));
    g.drawText ("Bar",
                toolbar.withTrimmedLeft (UiTheme::Layout::timelineCanvasSnapValueX)
                       .withWidth (UiTheme::Layout::timelineCanvasSnapValueWidth),
                juce::Justification::centredLeft,
                false);
}

inline void drawRuler (juce::Graphics& g, juce::Rectangle<int> ruler, juce::Rectangle<int> clipArea,
                       const TimelineCanvasState& state, const Viewport& vp)
{
    g.setColour (kRulerBack);
    g.fillRect (ruler);
    g.setColour (kGrid);
    g.fillRect (ruler.withHeight (UiTheme::Layout::timelineCanvasRulerSeparatorHeight)
                      .withY (ruler.getBottom() - UiTheme::Layout::timelineCanvasRulerSeparatorHeight));

    const double rightSeconds = vp.scrollSeconds + static_cast<double> (clipArea.getWidth()) / vp.pixelsPerSecond;
    const double labelStep = vp.pixelsPerSecond < UiTheme::Layout::timelineCanvasRulerDensePixelsPerSecond
                                 ? UiTheme::Layout::timelineCanvasRulerWideLabelStepSeconds
                                 : UiTheme::Layout::timelineCanvasRulerNarrowLabelStepSeconds;
    const double firstLabel = std::floor (vp.scrollSeconds / labelStep) * labelStep;

    for (double seconds = firstLabel; seconds <= rightSeconds + labelStep; seconds += labelStep)
    {
        const int x = clipArea.getX() + juce::roundToInt ((seconds - vp.scrollSeconds) * vp.pixelsPerSecond);
        if (x < clipArea.getX() - UiTheme::Layout::timelineCanvasRulerLabelCullPadding
            || x > clipArea.getRight() + UiTheme::Layout::timelineCanvasRulerLabelCullPadding)
            continue;

        const int barNumber = std::max (1, juce::roundToInt (seconds) + 1);
        g.setColour (kMutedText);
        g.setFont (juce::Font (juce::FontOptions (UiTheme::Type::small)));
        g.drawText (juce::String (barNumber),
                    x - UiTheme::Layout::timelineCanvasRulerLabelLeftInset,
                    ruler.getY() + UiTheme::Layout::timelineCanvasRulerLabelTopInset,
                    UiTheme::Layout::timelineCanvasRulerLabelWidth,
                    UiTheme::Layout::timelineCanvasRulerLabelHeight,
                    juce::Justification::centred, false);
        g.setColour (kMutedText.withAlpha (0.65f));
        g.fillRect (x,
                    ruler.getBottom() - UiTheme::Layout::timelineCanvasRulerTickHeight,
                    UiTheme::Layout::timelineCanvasRulerTickWidth,
                    UiTheme::Layout::timelineCanvasRulerTickHeight);
    }

    if (state.markers == nullptr)
        return;

    for (int i = 0; i < state.markerCount; ++i)
    {
        const auto& marker = state.markers[i];
        const int x = clipArea.getX() + juce::roundToInt ((marker.seconds - vp.scrollSeconds) * vp.pixelsPerSecond);
        if (x < clipArea.getX() - UiTheme::Layout::timelineCanvasRulerMarkerCullPadding
            || x > clipArea.getRight())
            continue;

        g.setColour (kText);
        g.setFont (juce::Font (juce::FontOptions (UiTheme::Type::small)));
        g.drawText (marker.label,
                    x + UiTheme::Layout::timelineCanvasRulerMarkerLabelLeftInset,
                    ruler.getY() + UiTheme::Layout::timelineCanvasRulerMarkerLabelTopInset,
                    UiTheme::Layout::timelineCanvasRulerMarkerLabelWidth,
                    UiTheme::Layout::timelineCanvasRulerMarkerLabelHeight,
                    juce::Justification::centredLeft, false);
    }
}

inline void drawGrid (juce::Graphics& g, juce::Rectangle<int> clipArea, const TimelineCanvasState& state,
                      const Viewport& vp, int laneHeight)
{
    g.setColour (kCanvasBack);
    g.fillRect (clipArea);

    const int laneCount = std::max (UiTheme::Layout::timelineCanvasGridMinLaneCount, state.trackCount);
    for (int lane = 0; lane <= laneCount; ++lane)
    {
        const int y = clipArea.getY() + lane * laneHeight;
        g.setColour (kGrid.withAlpha (0.7f));
        g.fillRect (clipArea.getX(), y, clipArea.getWidth(), UiTheme::Layout::timelineCanvasGridLaneSeparatorHeight);

        if (lane < state.trackCount && state.tracks != nullptr)
        {
            g.setColour (state.tracks[lane].colour.withAlpha (0.20f));
            g.fillRect (clipArea.getX(),
                        y + UiTheme::Layout::timelineCanvasGridTrackTintTopInset,
                        UiTheme::Layout::timelineCanvasGridTrackTintWidth,
                        std::max (0, laneHeight - UiTheme::Layout::timelineCanvasGridTrackTintHeightTrim));
        }
    }

    const double rightSeconds = vp.scrollSeconds + static_cast<double> (clipArea.getWidth()) / vp.pixelsPerSecond;
    const double gridStepSeconds = UiTheme::Layout::timelineCanvasGridStepSeconds;
    const double firstGrid = std::floor (vp.scrollSeconds / gridStepSeconds) * gridStepSeconds;
    for (double seconds = firstGrid; seconds <= rightSeconds + gridStepSeconds; seconds += gridStepSeconds)
    {
        const int x = clipArea.getX() + juce::roundToInt ((seconds - vp.scrollSeconds) * vp.pixelsPerSecond);
        if (x < clipArea.getX() || x > clipArea.getRight())
            continue;

        const bool major = (juce::roundToInt (seconds) % UiTheme::Layout::timelineCanvasGridMajorStepSeconds) == 0;
        g.setColour (major ? kGrid.brighter (0.25f) : kGrid.withAlpha (0.38f));
        g.fillRect (x, clipArea.getY(), UiTheme::Layout::timelineCanvasGridLineWidth, clipArea.getHeight());
    }
}

inline void drawPlayhead (juce::Graphics& g, juce::Rectangle<int> ruler, juce::Rectangle<int> clipArea,
                          const TimelineCanvasState& state, const Viewport& vp)
{
    const int playheadX = clipArea.getX() + juce::roundToInt ((state.playheadSeconds - vp.scrollSeconds)
                                                             * vp.pixelsPerSecond);
    if (playheadX < clipArea.getX() || playheadX > clipArea.getRight())
        return;

    g.setColour (UiTheme::Color::white());
    g.fillRect (playheadX,
                ruler.getY(),
                UiTheme::Layout::timelineCanvasPlayheadLineWidth,
                clipArea.getBottom() - ruler.getY());
    g.setColour (kPurple);
    g.fillRoundedRectangle (
        static_cast<float> (playheadX - UiTheme::Layout::timelineCanvasPlayheadBadgeHalfWidth),
        static_cast<float> (ruler.getY() + UiTheme::Layout::timelineCanvasPlayheadBadgeTopInset),
        static_cast<float> (UiTheme::Layout::timelineCanvasPlayheadBadgeWidth),
        static_cast<float> (UiTheme::Layout::timelineCanvasPlayheadBadgeHeight),
        UiTheme::Radius::pill);
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions (UiTheme::Type::small)));
    g.drawText (juce::String (std::max (1, juce::roundToInt (state.playheadSeconds) + 1)),
                playheadX - UiTheme::Layout::timelineCanvasPlayheadTextHalfWidth,
                ruler.getY() + UiTheme::Layout::timelineCanvasPlayheadBadgeTopInset,
                UiTheme::Layout::timelineCanvasPlayheadTextWidth,
                UiTheme::Layout::timelineCanvasPlayheadTextHeight,
                juce::Justification::centred,
                false);
}

} // namespace timeline_canvas_detail

inline TimelineCanvasGeometry timelineCanvasGeometry (juce::Rectangle<int> area,
                                                       const TimelineCanvasState& state)
{
    TimelineCanvasGeometry geometry;
    if (area.getWidth() <= 0 || area.getHeight() <= 0)
        return geometry;

    auto content = area.reduced (UiTheme::Layout::timelineCanvasOuterInset);
    geometry.toolbarArea = content.removeFromTop (UiTheme::Layout::timelineCanvasToolbarHeight);
    geometry.rulerArea = content.removeFromTop (UiTheme::Layout::timelineCanvasRulerHeight);
    geometry.clipArea = content.reduced (UiTheme::Layout::timelineCanvasClipAreaInsetX,
                                         UiTheme::Layout::timelineCanvasClipAreaInsetY);

    const int laneCount = std::max (1, state.trackCount);
    geometry.laneHeight = std::max (UiTheme::Layout::timelineCanvasLaneMinHeight,
                                    geometry.clipArea.getHeight() / laneCount);

    geometry.viewport = state.viewport;
    geometry.viewport.pixelsPerSecond = std::max (1.0, geometry.viewport.pixelsPerSecond);
    geometry.viewport.widthPixels = static_cast<double> (geometry.clipArea.getWidth());
    geometry.viewport.laneHeightPixels = static_cast<double> (geometry.laneHeight);
    return geometry;
}

inline TimelineHitTestResult hitTestTimelineCanvas (juce::Rectangle<int> area,
                                                    const TimelineCanvasState& state,
                                                    juce::Point<int> position)
{
    const TimelineCanvasGeometry geometry = timelineCanvasGeometry (area, state);
    if (state.clips == nullptr || state.clipCount <= 0 || ! geometry.clipArea.contains (position))
        return {};

    return hitTestVisibleClip (
        state.clips,
        state.clipCount,
        geometry.viewport,
        static_cast<double> (position.x - geometry.clipArea.getX()),
        static_cast<double> (position.y - geometry.clipArea.getY()));
}

inline TimelineCanvasPaintStats paintTimelineCanvas (juce::Graphics& g, juce::Rectangle<int> area,
                                                     const TimelineCanvasState& state)
{
    using namespace timeline_canvas_detail;

    TimelineCanvasPaintStats stats;
    stats.visibleClipCapacity = kVisibleClipCapacity;

    if (area.getWidth() <= 0 || area.getHeight() <= 0)
        return stats;

    fillPanel (g, area);

    const TimelineCanvasGeometry geometry = timelineCanvasGeometry (area, state);
    const auto clipArea = geometry.clipArea;
    const auto ruler = geometry.rulerArea;
    const auto vp = geometry.viewport;
    const int laneHeight = geometry.laneHeight;

    drawToolbar (g, geometry.toolbarArea);
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
                            .reduced (UiTheme::Space::xs, UiTheme::Space::xs + UiTheme::Space::hairline);
        clipRect = clipRect.getIntersection (clipArea);
        drawClip (g, clipRect, style, rect.id);
    }

    drawPlayhead (g, ruler, clipArea, state, vp);

    return stats;
}

} // namespace yesdaw::ui
