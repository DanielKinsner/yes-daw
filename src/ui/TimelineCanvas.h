// YES DAW - H11 native Timeline canvas.
//
// The app shell and the frame-time gate share this renderer so dense timeline drawing is measured
// through the same code path the user sees.

#pragma once

#include "ui/TimelineLayout.h"
#include "ui/UiTheme.h"
#include "ui/WaveformColumns.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>

namespace yesdaw::persistence {
struct WaveformPeakCache;
}

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

    std::function<std::shared_ptr<const persistence::WaveformPeakCache> (int clipId)> waveformCacheLookup;

    Viewport viewport {};
    double totalSeconds = UiTheme::Layout::timelineCanvasDefaultTotalSeconds;
    double playheadSeconds = UiTheme::Layout::timelineCanvasDefaultPlayheadSeconds;
};

struct TimelineCanvasPaintStats
{
    int visibleClips = 0;
    int visibleClipCapacity = 0;
    bool hitVisibleClipCapacity = false;
    int readyWaveformClips = 0;
    int pendingWaveformClips = 0;
    int readyWaveformColumns = 0;
    int placeholderWaveformClips = 0;
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
    const TimelineCanvasClipStyle fallback {
        UiTheme::Color::accentBlue(),
        UiTheme::Tone::timelineCanvasFallbackClipAmplitude
    };
    if (state.clipStyles == nullptr || state.clips == nullptr || state.clipCount <= 0)
        return fallback;

    if (clipId >= 0 && clipId < state.clipCount && state.clips[clipId].id == clipId)
        return state.clipStyles[clipId];

    for (int i = 0; i < state.clipCount; ++i)
        if (state.clips[i].id == clipId)
            return state.clipStyles[i];

    return fallback;
}

inline const Clip* clipForId (const TimelineCanvasState& state, int clipId)
{
    if (state.clips == nullptr || state.clipCount <= 0)
        return nullptr;

    if (clipId >= 0 && clipId < state.clipCount && state.clips[clipId].id == clipId)
        return &state.clips[clipId];

    for (int i = 0; i < state.clipCount; ++i)
        if (state.clips[i].id == clipId)
            return &state.clips[i];

    return nullptr;
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

    g.setColour (colour.brighter (UiTheme::Tone::timelineCanvasWaveformBrightness));
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

inline int drawClipCachedWaveform (juce::Graphics& g, juce::Rectangle<int> area, juce::Colour colour,
                                   float amplitude, const Clip& clip,
                                   const persistence::WaveformPeakCache& cache,
                                   const Viewport& vp)
{
    area.reduce (UiTheme::Layout::timelineCanvasWaveformInsetX,
                 UiTheme::Layout::timelineCanvasWaveformInsetY);
    if (area.isEmpty()
        || clip.lengthSeconds <= UiThemeLayout::timelineLayoutZeroFloor
        || vp.pixelsPerSecond <= UiThemeLayout::timelineLayoutZeroFloor)
    {
        return 0;
    }

    const double sampleRate = static_cast<double> (cache.sourceFrames) / clip.lengthSeconds;
    const double clipLocalStartSeconds = std::max (UiThemeLayout::timelineLayoutZeroFloor,
                                                   vp.scrollSeconds - clip.startSeconds);
    const double visibleSeconds = static_cast<double> (area.getWidth()) / vp.pixelsPerSecond;
    const auto sourceFrameOffset = static_cast<std::uint64_t> (
        std::llround (clipLocalStartSeconds * sampleRate));
    const auto sourceFrameCount = static_cast<std::uint64_t> (
        std::llround (visibleSeconds * sampleRate));

    const WaveformColumnViewport columnViewport {
        sourceFrameOffset,
        sourceFrameCount,
        sampleRate,
        vp.pixelsPerSecond,
        area.getWidth()
    };
    const WaveformColumns columns = computeWaveformColumns (cache, columnViewport);
    if (columns.columns.empty())
        return 0;

    const float midY = static_cast<float> (area.getCentreY());
    const float half = static_cast<float> (area.getHeight())
                     * std::clamp (amplitude,
                                   UiTheme::Layout::timelineCanvasWaveformMinAmplitude,
                                   UiTheme::Layout::timelineCanvasWaveformMaxAmplitude)
                     * UiTheme::Layout::timelineCanvasWaveformHeightScale;
    const float minValue = -UiTheme::Layout::timelineCanvasWaveformMaxAmplitude;
    const float maxValue = UiTheme::Layout::timelineCanvasWaveformMaxAmplitude;
    const juce::Colour peakColour = colour.brighter (UiTheme::Tone::timelineCanvasWaveformBrightness);
    const juce::Colour rmsColour = peakColour.withAlpha (UiTheme::Tone::timelineCanvasGridMinorLineAlpha);

    int x = area.getX();
    for (const auto& column : columns.columns)
    {
        const float top = midY - half * std::clamp (column.max, minValue, maxValue);
        const float bottom = midY - half * std::clamp (column.min, minValue, maxValue);
        const float rms = half * std::clamp (column.rms, UiTheme::Layout::timelineCanvasWaveformMinAmplitude,
                                            UiTheme::Layout::timelineCanvasWaveformMaxAmplitude);

        g.setColour (rmsColour);
        g.drawVerticalLine (x, midY - rms, midY + rms);
        g.setColour (peakColour);
        g.drawVerticalLine (x, top, bottom);

        ++x;
        if (x >= area.getRight())
            break;
    }

    return static_cast<int> (columns.columns.size());
}

inline bool drawClipFrame (juce::Graphics& g, juce::Rectangle<int> area,
                           const TimelineCanvasClipStyle& style)
{
    if (area.getWidth() <= UiTheme::Layout::timelineCanvasClipMinPaintWidth
        || area.getHeight() <= UiTheme::Layout::timelineCanvasClipMinPaintHeight)
        return false;

    if (area.getHeight() <= UiTheme::Layout::timelineCanvasClipCompactHeight)
    {
        g.setColour (style.colour.withAlpha (UiTheme::Tone::timelineCanvasCompactClipAlpha));
        g.fillRect (area);
        g.setColour (style.colour.brighter (UiTheme::Tone::timelineCanvasCompactHighlightBrightness));
        g.fillRect (area.withHeight (UiTheme::Layout::timelineCanvasClipCompactHighlightHeight));
        return false;
    }

    g.setColour (style.colour.withAlpha (UiTheme::Tone::timelineCanvasClipFillAlpha));
    g.fillRoundedRectangle (area.toFloat(), UiTheme::Radius::md);
    g.setColour (style.colour.brighter (UiTheme::Tone::timelineCanvasClipOutlineBrightness));
    g.drawRoundedRectangle (area.toFloat().reduced (UiTheme::Layout::timelineCanvasOutlineInset),
                            UiTheme::Radius::md,
                            UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
    return true;
}

inline void drawClip (juce::Graphics& g, juce::Rectangle<int> area, const TimelineCanvasClipStyle& style,
                      int clipId)
{
    if (! drawClipFrame (g, area, style))
        return;

    drawClipWaveform (g, area, style.colour, style.amplitude, clipId);
}

inline void drawToolbar (juce::Graphics& g, juce::Rectangle<int> toolbar)
{
    g.setColour (kToolbarBack);
    g.fillRect (toolbar);

    auto tools = toolbar.withTrimmedLeft (UiTheme::Space::xl)
                         .withWidth (UiTheme::Layout::timelineCanvasToolbarWidth)
                         .reduced (UiTheme::Layout::timelineCanvasToolbarInsetX,
                                   UiTheme::Layout::timelineCanvasToolbarInsetY);
    for (const auto* label : { "A", "P", "E", "S", "L" })
    {
        auto cell = tools.removeFromLeft (UiTheme::Layout::timelineCanvasToolCellWidth)
                         .reduced (UiTheme::Layout::timelineCanvasToolCellInsetX,
                                   UiTheme::Layout::timelineCanvasToolCellInsetY);
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
                                   .reduced (UiTheme::Layout::timelineCanvasSnapFieldInsetX,
                                             UiTheme::Layout::timelineCanvasSnapFieldInsetY)
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
        g.setColour (kMutedText.withAlpha (UiTheme::Tone::timelineCanvasRulerTickAlpha));
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
        g.setColour (kGrid.withAlpha (UiTheme::Tone::timelineCanvasGridLaneSeparatorAlpha));
        g.fillRect (clipArea.getX(), y, clipArea.getWidth(), UiTheme::Layout::timelineCanvasGridLaneSeparatorHeight);

        if (lane < state.trackCount && state.tracks != nullptr)
        {
            g.setColour (state.tracks[lane].colour.withAlpha (UiTheme::Tone::timelineCanvasGridTrackTintAlpha));
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
        g.setColour (major ? kGrid.brighter (UiTheme::Tone::timelineCanvasGridMajorLineBrightness)
                            : kGrid.withAlpha (UiTheme::Tone::timelineCanvasGridMinorLineAlpha));
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

    const int laneCount = std::max (UiTheme::Layout::timelineCanvasGeometryMinLaneCount, state.trackCount);
    geometry.laneHeight = std::max (UiTheme::Layout::timelineCanvasLaneMinHeight,
                                    geometry.clipArea.getHeight() / laneCount);

    geometry.viewport = state.viewport;
    geometry.viewport.pixelsPerSecond =
        std::max (UiTheme::Layout::timelineCanvasViewportMinPixelsPerSecond,
                  geometry.viewport.pixelsPerSecond);
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
        const auto readyCache = state.waveformCacheLookup ? state.waveformCacheLookup (rect.id) : nullptr;
        if (readyCache != nullptr)
        {
            ++stats.readyWaveformClips;
            if (drawClipFrame (g, clipRect, style))
            {
                const Clip* clip = clipForId (state, rect.id);
                if (clip != nullptr)
                {
                    stats.readyWaveformColumns += drawClipCachedWaveform (g, clipRect, style.colour,
                                                                           style.amplitude, *clip,
                                                                           *readyCache, vp);
                }
            }
        }
        else
        {
            if (state.waveformCacheLookup)
            {
                ++stats.pendingWaveformClips;
                ++stats.placeholderWaveformClips;
            }
            drawClip (g, clipRect, style, rect.id);
        }
    }

    drawPlayhead (g, ruler, clipArea, state, vp);

    return stats;
}

} // namespace yesdaw::ui
