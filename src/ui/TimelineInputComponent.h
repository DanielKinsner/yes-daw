// YES DAW — the arrangement's input overlay (clips, lanes, ruler, marquee, drops).
//
// Plan §5.1 (the shell topology), checkpoint 1 (2026-09-05): carved verbatim out of MainComponent.cpp,
// where it sat above the shell class. The shell owns the instance and wires the std::function hooks;
// this file owns the class. Behaviour unchanged; the gate is [shell-topology] in the theme audit.

#pragma once

#include "engine/Time.h"
#include "ui/ContextMenus.h"
#include "ui/TimelineCanvas.h"
#include "ui/UiAppModel.h"
#include "ui/UiMixerSurface.h"
#include "ui/UiPianoRollSurface.h"
#include "ui/UiTheme.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace yesdaw::ui {

class TimelineInputComponent final : public juce::Component,
                                     public juce::FileDragAndDropTarget,
                                     public juce::SettableTooltipClient,
                                     private juce::Timer   // G2.3: edge-band auto-scroll
{
public:
    // M10: dropping files from the OS. The drop POINT picks the track and the start tick; the
    // shell decides what is importable and reports refusals honestly.
    std::function<bool (const juce::StringArray&)> filesAreImportable;
    std::function<void (const juce::StringArray&, int, double)> onFilesDropped;   // files, lane, seconds

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return filesAreImportable && filesAreImportable (files);
    }

    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        if (! onFilesDropped || ! stateProvider)
            return;

        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        if (state.trackCount <= 0 || geometry.laneHeight <= 0)
            return;

        const juce::Point<int> position { x, y };
        const int lane = std::clamp (
            geometry.laneAtPixel (position.y - geometry.clipArea.getY() + geometry.viewport.laneScrollPixels),
            0, state.trackCount - 1);
        const double seconds = timelineSecondsAt (state, getLocalBounds(), position).value_or (0.0);
        onFilesDropped (files, lane, seconds);
    }

    std::function<yesdaw::ui::TimelineCanvasState()> stateProvider;
    std::function<yesdaw::ui::TimelineTool()> activeToolProvider;
    std::function<void (int, bool)> onClipClicked;
    std::function<void()> onEmptyClicked;
    std::function<void (std::span<const int>)> onMarqueeSelection;
    std::function<void (int, double, bool)> onClipMoved;
    std::function<void (int, int, double, bool)> onClipMovedToLane;   // layoutClipId, targetLane, startSeconds, snap
    std::function<void (int, int, double, bool)> onClipCopied;        // layoutClipId, targetLane (-1 = same), startSeconds, snap
    // Time-gestures carry the gesture's Ctrl flag so the shell can apply the snap chooser with
    // Ctrl inversion (E4); fades are durations and stay honestly unsnapped.
    std::function<void (int, double, bool)> onClipSplit;         // layoutClipId, seconds, snapInvert
    std::function<void (int, double, bool)> onClipTrimmedRight;  // layoutClipId, seconds, snapInvert
    std::function<void (int, double, bool)> onClipStretchedRight; // G2.9b: layoutClipId, new end seconds, snapInvert
    std::function<void (int, double)> onClipSlipped;              // G2.11: layoutClipId, delta seconds (content moves right for +)
    std::function<void (int)> onClipRenameRequested;              // G2.12: layoutClipId (a double-click in the name band)
    std::function<void (int, double, bool)> onClipTrimmedLeft;   // layoutClipId, seconds, snapInvert
    std::function<void (int, int)> onClipGainAdjusted;
    std::function<void (int, bool, double, double)> onClipFadeAdjusted;   // G2.10: + the curve bend
    std::function<void (double)> onTimelineLocated;
    std::function<void (double, double, bool)> onLoopRegionDragged;   // startSeconds, endSeconds, snapInvert
    // N8: Alt+Shift-drag on the ruler — startSeconds, endSeconds, snapInvert. A degenerate span
    // (end <= start, a click rather than a real drag) means "clear the punch region", not "set a
    // zero-length one".
    std::function<void (double, double, bool)> onPunchRegionDragged;
    std::function<void (double, double, bool)> onRulerRangeSelected;  // startSeconds, endSeconds, snapInvert (plain drag)
    std::function<void()> onRulerRangeCleared;                   // plain ruler click collapses the range
    std::function<void (double, double)> onZoomWheel;            // anchorSeconds, wheelDelta
    std::function<void (double)> onRulerAltClicked;              // seconds: remove nearest marker
    std::function<void (double)> onScrollWheel;                  // wheelDelta (view-widths per notch)
    std::function<void (double, bool)> onZoomToolClicked;        // anchorSeconds, zoomOut (Alt) — E3
    std::function<void (double)> onHandToolScrolled;             // secondsDelta from a Hand drag — E3
    std::function<void (int)> onClipErased;                      // G3.2: the Eraser tool's click (layout clip id)
    std::function<void (int, double)> onPencilEmptyLane;         // lane, seconds: pencil a MIDI clip — E3
    std::function<void (yesdaw::ui::TimelineTool)> onToolSelected;   // a click on a tool-strip cell
    std::function<void (int)> onVerticalScrollRows;              // +1 down / -1 up, plain wheel — E5

    // Loop brace editing (E6): drag either handle to resize, drag the band to move.
    enum class LoopBraceEdit : std::uint8_t { None, Start, End, Move };
    std::function<void (LoopBraceEdit, double, double, bool)> onLoopBraceEdited;
    // kind, pointerSeconds, grabOffsetSeconds (Move only), snapInvert

    // Marker editing (E7): drag a ruler marker label to move it; double-click to rename.
    std::function<void (int, double, bool)> onMarkerDragged;      // markerIndex, seconds, snapInvert
    std::function<void (int)> onMarkerRenameRequested;            // markerIndex
    // G2.15 labels, clickable (2026-09-04): a click on a painted tempo / meter change label
    // locates the playhead exactly to that change, so the ruler menu's "at the playhead" verbs
    // (remove, toggle ramp) act on it. mapIndex into the canvas state's mapLabels.
    std::function<void (int)> onMapLabelClicked;

    // E9: double-click on a clip, fired before the split path; returning true consumes the
    // gesture (a MIDI clip opens its piano roll instead of attempting the audio split).
    std::function<bool (int)> onClipDoubleClicked;                // layoutClipId -> consumed

    [[nodiscard]] bool cancelInProgressEdit()
    {
        stopTimer();   // G2.3: an Esc mid-drag ends the auto-scroll with the drag
        if (! dragState.active && ! marqueeState.active && ! rulerRangeDragActive && ! handDragActive
            && loopBraceDrag == LoopBraceEdit::None && markerDragIndex < 0)
            return false;

        dragState = {};
        marqueeState = {};
        rulerRangeDragActive = false;
        handDragActive = false;
        loopBraceDrag = LoopBraceEdit::None;
        markerDragIndex = -1;
        repaint();
        return true;
    }

    // G2.3: the drag ghost — what the release WILL do, painted from dragState + the pointer while
    // the model stays untouched: move / copy (with the lane change and the snap landing line),
    // trims (the moved edge), fades (the wedge), gain (the level line). Same arithmetic as the
    // release path (mouseUp) and the hit test.
    void paintDragGhost (juce::Graphics& g, const yesdaw::ui::TimelineCanvasState& state)
    {
        const yesdaw::ui::Clip* clip = findClipByLayoutId (state, dragState.layoutClipId);
        if (clip == nullptr)
            return;
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        const yesdaw::ui::Viewport vp = yesdaw::ui::viewportForClipLayout (geometry);
        const double pps = std::max (yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
                                     geometry.viewport.pixelsPerSecond);
        const yesdaw::ui::ClipPixelRect base = yesdaw::ui::visibleClipPixelRect (*clip, vp);
        const juce::Rectangle<float> clipRect (static_cast<float> (geometry.clipArea.getX() + base.x),
                                               static_cast<float> (geometry.clipArea.getY() + base.y),
                                               static_cast<float> (base.w), static_cast<float> (base.h));
        const int deltaX = dragState.currentPosition.x - dragState.downPosition.x;
        const int deltaY = dragState.currentPosition.y - dragState.downPosition.y;
        const auto fill = yesdaw::ui::UiTheme::Color::accentPurple().withAlpha (yesdaw::ui::UiTheme::Tone::timelineDragGhostFillAlpha);
        const auto outline = yesdaw::ui::UiTheme::Color::accentPurple().withAlpha (yesdaw::ui::UiTheme::Tone::timelineDragGhostOutlineAlpha);
        const auto snapped = [this] (double seconds, bool snapping)
        {
            return snapping && snapSecondsForPreview && ! dragState.snapInvert ? snapSecondsForPreview (seconds) : seconds;
        };
        g.saveState();
        g.reduceClipRegion (geometry.clipArea);
        switch (dragState.mode)
        {
            case TimelineDragMode::Move:
            case TimelineDragMode::SnapMove:
            {
                const double rawStart = std::max (yesdaw::ui::UiTheme::Layout::timelineCoordinateSecondsFloor,
                                                  clip->startSeconds + static_cast<double> (deltaX) / pps);
                const double start = snapped (rawStart, dragState.mode == TimelineDragMode::SnapMove);
                int lane = clip->lane;
                if (state.trackCount > 0 && geometry.laneHeight > 0
                    && std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                    lane = std::clamp (geometry.laneAtPixel (dragState.currentPosition.y - geometry.clipArea.getY()
                                                             + geometry.viewport.laneScrollPixels),
                                       0, state.trackCount - 1);
                const float x = static_cast<float> (geometry.clipArea.getX() + (start - vp.scrollSeconds) * pps);
                const float y = static_cast<float> (geometry.clipArea.getY() + yesdaw::ui::laneTopPixelsFor (lane, vp));
                const juce::Rectangle<float> ghost (x, y, clipRect.getWidth(), clipRect.getHeight());
                g.setColour (fill);
                g.fillRect (ghost);
                g.setColour (outline);
                if (dragState.copy)
                {
                    const float dashes[] = { 4.0f, 3.0f };
                    juce::Path p;
                    p.addRectangle (ghost);
                    juce::PathStrokeType (yesdaw::ui::UiTheme::Layout::timelineDragGhostOutlineWidth)
                        .createDashedStroke (p, p, dashes, 2);
                    g.fillPath (p);
                }
                else
                    g.drawRect (ghost, yesdaw::ui::UiTheme::Layout::timelineDragGhostOutlineWidth);
                // The snap landing line: where the start WILL land, across every lane.
                g.fillRect (juce::Rectangle<float> (x, static_cast<float> (geometry.clipArea.getY()),
                                                    static_cast<float> (yesdaw::ui::UiTheme::Space::hairline),
                                                    static_cast<float> (geometry.clipArea.getHeight())));
                break;
            }
            case TimelineDragMode::TrimRight:
            case TimelineDragMode::TrimLeft:
            case TimelineDragMode::StretchRight:   // G2.9b: the ghost is the trim ghost — the end moves
            {
                const bool right = dragState.mode != TimelineDragMode::TrimLeft;
                const double edgeSeconds = right ? clip->startSeconds + clip->lengthSeconds : clip->startSeconds;
                const double moved = snapped (std::max (0.0, edgeSeconds + static_cast<double> (deltaX) / pps), true);
                const float edgeX = static_cast<float> (geometry.clipArea.getX() + (moved - vp.scrollSeconds) * pps);
                const juce::Rectangle<float> ghost = right
                    ? clipRect.withRight (std::max (clipRect.getX() + 1.0f, edgeX))
                    : clipRect.withLeft (std::min (clipRect.getRight() - 1.0f, edgeX));
                g.setColour (fill);
                g.fillRect (ghost);
                g.setColour (outline);
                g.drawRect (ghost, yesdaw::ui::UiTheme::Layout::timelineDragGhostOutlineWidth);
                g.fillRect (juce::Rectangle<float> (edgeX, static_cast<float> (geometry.clipArea.getY()),
                                                    static_cast<float> (yesdaw::ui::UiTheme::Space::hairline),
                                                    static_cast<float> (geometry.clipArea.getHeight())));
                break;
            }
            case TimelineDragMode::FadeIn:
            case TimelineDragMode::FadeOut:
            {
                const bool fadeIn = dragState.mode == TimelineDragMode::FadeIn;
                const double fadeSeconds = std::clamp ((fadeIn ? 1.0 : -1.0) * static_cast<double> (deltaX) / pps,
                                                       0.0, clip->lengthSeconds);
                const float fadeW = static_cast<float> (fadeSeconds * pps);
                juce::Path wedge;
                if (fadeIn)
                {
                    wedge.startNewSubPath (clipRect.getX(), clipRect.getBottom());
                    wedge.lineTo (clipRect.getX() + fadeW, clipRect.getY());
                    wedge.lineTo (clipRect.getX(), clipRect.getY());
                }
                else
                {
                    wedge.startNewSubPath (clipRect.getRight(), clipRect.getBottom());
                    wedge.lineTo (clipRect.getRight() - fadeW, clipRect.getY());
                    wedge.lineTo (clipRect.getRight(), clipRect.getY());
                }
                wedge.closeSubPath();
                g.setColour (fill);
                g.fillPath (wedge);
                g.setColour (outline);
                g.strokePath (wedge, juce::PathStrokeType (yesdaw::ui::UiTheme::Layout::timelineDragGhostOutlineWidth));
                break;
            }
            case TimelineDragMode::TimeSelect:
            {
                const float x0 = static_cast<float> (std::min (dragState.downPosition.x, dragState.currentPosition.x));
                const float x1 = static_cast<float> (std::max (dragState.downPosition.x, dragState.currentPosition.x));
                g.setColour (fill);
                g.fillRect (juce::Rectangle<float> (x0, static_cast<float> (geometry.clipArea.getY()), x1 - x0,
                                                    static_cast<float> (geometry.clipArea.getHeight())));
                g.setColour (outline);
                for (const float x : { x0, x1 })
                    g.fillRect (juce::Rectangle<float> (x, static_cast<float> (geometry.clipArea.getY()),
                                                        static_cast<float> (yesdaw::ui::UiTheme::Space::hairline),
                                                        static_cast<float> (geometry.clipArea.getHeight())));
                break;
            }
            case TimelineDragMode::Gain:
            {
                const float y = juce::jlimit (clipRect.getY(), clipRect.getBottom() - 1.0f,
                                              clipRect.getCentreY() + static_cast<float> (deltaY));
                g.setColour (fill);
                g.fillRect (clipRect);
                g.setColour (outline);
                g.fillRect (juce::Rectangle<float> (clipRect.getX(), y, clipRect.getWidth(),
                                                    static_cast<float> (yesdaw::ui::UiTheme::Space::hairline)));
                break;
            }
            default:
                break;
        }
        g.restoreState();
    }

    void paint (juce::Graphics& g) override
    {
        if (stateProvider)
        {
            yesdaw::ui::TimelineCanvasState state = stateProvider();
            // Loop brace drag preview (E6): the in-flight brace follows the raw pointer; the
            // committed edit applies the snap chooser on release.
            if (loopBraceDrag != LoopBraceEdit::None && state.loopActive)
            {
                const double span = state.loopEndSeconds - state.loopStartSeconds;
                if (loopBraceDrag == LoopBraceEdit::Start && loopBracePointerSeconds < state.loopEndSeconds)
                    state.loopStartSeconds = std::max (0.0, loopBracePointerSeconds);
                else if (loopBraceDrag == LoopBraceEdit::End && loopBracePointerSeconds > state.loopStartSeconds)
                    state.loopEndSeconds = loopBracePointerSeconds;
                else if (loopBraceDrag == LoopBraceEdit::Move)
                {
                    state.loopStartSeconds = std::max (0.0, loopBracePointerSeconds - loopBraceGrabOffsetSeconds);
                    state.loopEndSeconds = state.loopStartSeconds + span;
                }
            }
            (void) yesdaw::ui::paintTimelineCanvas (g, getLocalBounds(), state);
            if (dragState.active && dragState.moved)
                paintDragGhost (g, state);   // G2.3
            if (state.trackCount == 0 && state.clipCount == 0)
            {
                const auto geometry = yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
                g.setColour (yesdaw::ui::UiTheme::Color::text());
                g.setFont (yesdaw::ui::UiTheme::Type::font (
                    yesdaw::ui::UiTheme::Type::title,
                    juce::Font::bold));
                const int centreY = geometry.clipArea.getCentreY();
                g.drawText ("Create or open a Project",
                            juce::Rectangle<int> { geometry.clipArea.getX(), centreY - 30,
                                                   geometry.clipArea.getWidth(), 24 },
                            juce::Justification::centred,
                            false);
                g.setColour (yesdaw::ui::UiTheme::Color::mutedText());
                g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::body));
                g.drawText ("Use New or Open in the top-left toolbar",
                            juce::Rectangle<int> { geometry.clipArea.getX(), centreY + 2,
                                                   geometry.clipArea.getWidth(), 20 },
                            juce::Justification::centred,
                            false);
            }

            if (marqueeState.active)
            {
                const auto marquee = marqueeBounds().getIntersection (
                    yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state).clipArea);
                g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
                    yesdaw::ui::UiTheme::Tone::pressedHighlightAlpha));
                g.fillRect (marquee);
                g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
                    yesdaw::ui::UiTheme::Tone::focusRingAlpha));
                g.drawRect (marquee.toFloat(), yesdaw::ui::UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
            }

            if (rulerRangeDragActive)
            {
                const auto geometry = yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
                const int left = std::max (std::min (rulerRangeDownPosition.x, rulerRangeCurrentPosition.x),
                                           geometry.clipArea.getX());
                const int right = std::min (std::max (rulerRangeDownPosition.x, rulerRangeCurrentPosition.x),
                                            geometry.clipArea.getRight());
                if (right > left)
                {
                    const juce::Rectangle<int> band { left, geometry.rulerArea.getY(), right - left,
                                                      geometry.clipArea.getBottom() - geometry.rulerArea.getY() };
                    g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
                        yesdaw::ui::UiTheme::Tone::pressedHighlightAlpha));
                    g.fillRect (band);
                    g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
                        yesdaw::ui::UiTheme::Tone::focusRingAlpha));
                    g.drawRect (band.toFloat(), yesdaw::ui::UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
                }
            }
        }
    }

    // G1.6: the gesture hint for the hovered zone — the status line shows it while no status
    // message is active. hintAt() is the one law mouseMove and the harness share.
    std::function<void (const juce::String&)> onHoverHint;

    [[nodiscard]] juce::String hintAt (juce::Point<int> position, juce::ModifierKeys modifiers) const
    {
        if (! stateProvider)
            return {};
        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        if (const std::optional<yesdaw::ui::TimelineTool> tool =
                yesdaw::ui::timelineToolAtPoint (getLocalBounds(), state, position))
        {
            // G1.6: nothing blind — the strip cell names its tool like every other hovered zone.
            const auto* descriptor = yesdaw::ui::descriptorFor (yesdaw::ui::timelineToolSelectAction (*tool));
            return descriptor != nullptr
                ? juce::String (descriptor->label) + ": " + descriptor->accessibleName
                : juce::String ("Tool");
        }
        if (geometry.rulerArea.contains (position))
        {
            if (yesdaw::ui::timelineMapLabelAt (getLocalBounds(), state, position) >= 0)
                return "Tempo / meter change: click to locate the playhead on it \u00b7 right-click to edit or remove";
            for (int markerIndex = 0; markerIndex < state.markerCount; ++markerIndex)
                if (yesdaw::ui::timelineMarkerLabelRect (getLocalBounds(), state, markerIndex).contains (position))
                    return "Marker: drag to move \u00b7 double-click to rename \u00b7 Alt-click removes";
            return "Ruler: click to locate \u00b7 drag to select a time range \u00b7 Shift-drag sets the loop \u00b7 Alt+Shift-drag sets the punch";
        }
        if (! geometry.clipArea.contains (position))
            return {};
        const yesdaw::ui::TimelineHitTestResult hit =
            yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, position);
        if (! hit.hit)
            return "Lane: click to select the track \u00b7 drag a marquee to select clips \u00b7 right-click for the lane menu";
        switch (dragModeForPointer (state, getLocalBounds(), hit.id, position, modifiers))
        {
            case TimelineDragMode::TrimLeft:
            case TimelineDragMode::TrimRight: return "Clip edge: drag to trim \u00b7 Alt on the right edge time-stretches \u00b7 Ctrl defeats snap";
            case TimelineDragMode::StretchRight: return "Clip edge: Alt-drag to time-stretch \u00b7 the length changes, the source does not";
            case TimelineDragMode::FadeIn:
            case TimelineDragMode::FadeOut:   return "Fade: drag to set its length";
            case TimelineDragMode::Gain:      return "Clip: drag up or down to set gain";
            case TimelineDragMode::TimeSelect: return "Clip: drag here to select a time range \u00b7 the body above moves";
            case TimelineDragMode::SnapMove:  return "Clip: drag to move without snap";
            case TimelineDragMode::Slip:      return "Clip: Ctrl+Alt-drag slips the audio under the clip \u00b7 the clip stays put";
            case TimelineDragMode::Move:      break;
        }
        return "Clip: drag to move \u00b7 Shift-drag sets gain \u00b7 Ctrl-drag defeats snap \u00b7 edges trim \u00b7 right-click for the clip menu";
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        setMouseCursor (cursorAt (event.getPosition(), event.mods));   // G2.4: the zone shows before the press
        if (onHoverHint)
            onHoverHint (hintAt (event.getPosition(), event.mods));
    }

    enum class TimelineDragMode;   // defined below (the class-scope enum the zone law returns)

    // G2.4: the zone under a point, as a drag mode (nullopt off any clip) — the hint, the cursor,
    // the press and the harness share it.
    [[nodiscard]] std::optional<TimelineDragMode> zoneAt (juce::Point<int> position, juce::ModifierKeys modifiers) const
    {
        if (! stateProvider)
            return std::nullopt;
        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineHitTestResult hit = yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, position);
        if (! hit.hit)
            return std::nullopt;
        return dragModeForPointer (state, getLocalBounds(), hit.id, position, modifiers);
    }

    [[nodiscard]] juce::MouseCursor cursorAt (juce::Point<int> position, juce::ModifierKeys modifiers) const
    {
        if (const std::optional<TimelineDragMode> zone = zoneAt (position, modifiers))
            return cursorForDragMode (*zone);
        return juce::MouseCursor::NormalCursor;
    }

    [[nodiscard]] juce::String zoneNameAt (juce::Point<int> position, juce::ModifierKeys modifiers) const
    {
        if (const std::optional<TimelineDragMode> zone = zoneAt (position, modifiers))
            return dragModeName (*zone);
        return "none";
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);   // G2.4
        if (onHoverHint)
            onHoverHint ({});
    }

    // G1.3: a right-click classifies what was clicked (clip / marker / ruler / empty lane),
    // makes it the selection, and asks the shell for that target's context menu.
    std::function<void (yesdaw::ui::ContextMenuTarget, int, juce::Point<int>)> onContextMenuRequested;

    void requestContextMenu (juce::Point<int> position)
    {
        if (! stateProvider || ! onContextMenuRequested)
            return;
        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        if (geometry.rulerArea.contains (position))
        {
            // A tempo / meter change label: the ruler menu's change verbs act at the playhead, so
            // the right-click locates there first (the marker law below, for changes).
            if (const int mapIndex = yesdaw::ui::timelineMapLabelAt (getLocalBounds(), state, position); mapIndex >= 0)
            {
                if (onMapLabelClicked)
                    onMapLabelClicked (mapIndex);
                onContextMenuRequested (yesdaw::ui::ContextMenuTarget::Ruler, -1, position);
                return;
            }
            for (int markerIndex = 0; markerIndex < state.markerCount; ++markerIndex)
            {
                if (yesdaw::ui::timelineMarkerLabelRect (getLocalBounds(), state, markerIndex).contains (position))
                {
                    // The marker's own verbs act on the marker nearest the playhead: go there first.
                    if (onTimelineLocated)
                        onTimelineLocated (state.markers[markerIndex].seconds);
                    onContextMenuRequested (yesdaw::ui::ContextMenuTarget::Marker, markerIndex, position);
                    return;
                }
            }
            onContextMenuRequested (yesdaw::ui::ContextMenuTarget::Ruler, -1, position);
            return;
        }
        if (! geometry.clipArea.contains (position))
            return;
        const yesdaw::ui::TimelineHitTestResult hit =
            yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, position);
        if (hit.hit)
        {
            // Logic: a right-click on an unselected clip selects it; one on a selected clip keeps
            // the selection (so a multi-selection's menu acts on all of it).
            bool alreadySelected = false;
            for (int c = 0; c < state.clipCount; ++c)
                if (state.clips[c].id == hit.id && state.clipStyles != nullptr && state.clipStyles[c].selected)
                    alreadySelected = true;
            if (! alreadySelected && onClipClicked)
                onClipClicked (hit.id, false);
            onContextMenuRequested (yesdaw::ui::ContextMenuTarget::Clip, hit.id, position);
            return;
        }
        if (state.trackCount <= 0 || geometry.laneHeight <= 0)
            return;
        const int lane = std::clamp (
            geometry.laneAtPixel (position.y - geometry.clipArea.getY() + geometry.viewport.laneScrollPixels),
            0, state.trackCount - 1);
        onContextMenuRequested (yesdaw::ui::ContextMenuTarget::EmptyLane, lane, position);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        if (event.mods.isRightButtonDown())   // the right button itself: on macOS isPopupMenu() also fires for Ctrl+click, which is a gesture modifier here
        {
            requestContextMenu (event.getPosition());
            return;
        }

        playheadLocateActive = false;
        rulerRangeDragActive = false;
        handDragActive = false;
        marqueeState = {};
        const yesdaw::ui::TimelineCanvasState state = stateProvider();

        // The tool strip: a click on a cell picks that tool and nothing else (the strip was
        // paint-only before — clicks fell through as a click on nothing).
        if (const std::optional<yesdaw::ui::TimelineTool> picked =
                yesdaw::ui::timelineToolAtPoint (getLocalBounds(), state, event.getPosition()))
        {
            if (onToolSelected)
                onToolSelected (*picked);
            return;
        }

        const yesdaw::ui::TimelineHitTestResult hit =
            yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, event.getPosition());

        // The tool palette owns the clip-area gesture (E3): Hand pans, Zoom clicks zoom, Scissors
        // splits the hit clip, Pencil creates a MIDI clip on the clicked empty lane (or just
        // selects a hit clip). The ruler keeps its locate/loop/range behavior for every tool, and
        // Pointer keeps the full historical gesture map below.
        const yesdaw::ui::TimelineTool tool =
            activeToolProvider ? activeToolProvider() : yesdaw::ui::TimelineTool::Pointer;
        if (tool != yesdaw::ui::TimelineTool::Pointer)
        {
            const yesdaw::ui::TimelineCanvasGeometry toolGeometry =
                yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
            if (toolGeometry.clipArea.contains (event.getPosition()))
            {
                if (tool == yesdaw::ui::TimelineTool::Hand)
                {
                    handDragActive = true;
                    handDragLastX = event.getPosition().x;
                    return;
                }

                if (tool == yesdaw::ui::TimelineTool::Zoom)
                {
                    if (const std::optional<double> seconds =
                            timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                        if (onZoomToolClicked)
                            onZoomToolClicked (*seconds, event.mods.isAltDown());
                    return;
                }

                if (tool == yesdaw::ui::TimelineTool::Scissors)
                {
                    if (hit.hit)
                        if (const std::optional<double> seconds =
                                timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                            if (onClipSplit)
                                onClipSplit (hit.id, *seconds, event.mods.isCtrlDown());
                    return;
                }

                if (tool == yesdaw::ui::TimelineTool::Eraser)   // G3.2: a click deletes the clip under it
                {
                    if (hit.hit && onClipErased)
                        onClipErased (hit.id);
                    return;
                }

                if (tool == yesdaw::ui::TimelineTool::Pencil)
                {
                    if (hit.hit)
                    {
                        if (onClipClicked)
                            onClipClicked (hit.id, event.mods.isShiftDown());
                        return;
                    }

                    if (state.trackCount > 0 && toolGeometry.laneHeight > 0)
                    {
                        const int lane = std::clamp (
                            toolGeometry.laneAtPixel (event.getPosition().y - toolGeometry.clipArea.getY()
                                                      + toolGeometry.viewport.laneScrollPixels),
                            0, state.trackCount - 1);
                        if (const std::optional<double> seconds =
                                timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                            if (onPencilEmptyLane)
                                onPencilEmptyLane (lane, *seconds);
                    }
                    return;
                }
            }
        }

        if (hit.hit)
        {
            if (onClipClicked)
                onClipClicked (hit.id, event.mods.isShiftDown());

            dragState = {};
            dragState.active = true;
            dragState.layoutClipId = hit.id;
            dragState.downPosition = event.getPosition();
            dragState.mode = dragModeForPointer (state, getLocalBounds(), hit.id, event.getPosition(), event.mods);
            dragState.copy = event.mods.isAltDown() && ! event.mods.isCtrlDown()   // G2.11: Ctrl+Alt is a slip, not a copy
                          && (dragState.mode == TimelineDragMode::Move
                              || dragState.mode == TimelineDragMode::SnapMove);
            if (const yesdaw::ui::Clip* clip = findClipByLayoutId (state, hit.id))
            {
                dragState.startSeconds = clip->startSeconds;
                dragState.lengthSeconds = clip->lengthSeconds;
            }
            return;
        }

        dragState = {};
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        if (geometry.rulerArea.contains (event.getPosition()))
        {
            // N8: Alt+Shift-drag defines the punch region — checked before plain Alt (marker
            // removal) so the more specific combo wins. Ctrl is already reserved, across every
            // ruler drag gesture below, as a release-time "invert snap" modifier — it is NOT
            // available as a gesture selector at mouse-down, so punch cannot use it alone.
            if (event.mods.isAltDown() && event.mods.isShiftDown())
            {
                punchDragActive = true;
                punchDragStartSeconds =
                    timelineSecondsAt (state, getLocalBounds(), event.getPosition()).value_or (0.0);
                return;
            }

            // Alt+click removes the nearest marker; Shift-drag defines a loop region; a plain click
            // locates while a plain drag selects a painted time range (parity item 25).
            if (event.mods.isAltDown())
            {
                if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                    if (onRulerAltClicked)
                        onRulerAltClicked (*seconds);
                return;
            }

            if (event.mods.isShiftDown())
            {
                loopDragActive = true;
                loopDragStartSeconds =
                    timelineSecondsAt (state, getLocalBounds(), event.getPosition()).value_or (0.0);
                return;
            }

            // Loop brace editing (E6): a press on the painted brace edits the loop instead of
            // locating; the handle/band rects come from the same law the painter uses.
            if (const yesdaw::ui::TimelineLoopBraceRects loopRects =
                    yesdaw::ui::timelineLoopBraceRects (getLocalBounds(), state);
                loopRects.valid)
            {
                if (const std::optional<double> seconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                {
                    if (loopRects.startHandle.contains (event.getPosition()))
                    {
                        loopBraceDrag = LoopBraceEdit::Start;
                        loopBracePointerSeconds = *seconds;
                        repaint();
                        return;
                    }
                    if (loopRects.endHandle.contains (event.getPosition()))
                    {
                        loopBraceDrag = LoopBraceEdit::End;
                        loopBracePointerSeconds = *seconds;
                        repaint();
                        return;
                    }
                    if (loopRects.band.contains (event.getPosition()))
                    {
                        loopBraceDrag = LoopBraceEdit::Move;
                        loopBracePointerSeconds = *seconds;
                        loopBraceGrabOffsetSeconds = *seconds - state.loopStartSeconds;
                        repaint();
                        return;
                    }
                }
            }

            // A tempo / meter change label (G2.15, clickable since 2026-09-04): the press locates
            // the playhead exactly on the change — not the nearest snap — so the change verbs
            // find it.
            if (const int mapIndex = yesdaw::ui::timelineMapLabelAt (getLocalBounds(), state, event.getPosition());
                mapIndex >= 0)
            {
                if (onMapLabelClicked)
                    onMapLabelClicked (mapIndex);
                return;
            }

            // Marker editing (E7): a press on a painted marker label starts a marker drag
            // instead of locating; the label rects come from the shared geometry law.
            for (int markerIndex = 0; markerIndex < state.markerCount; ++markerIndex)
            {
                if (yesdaw::ui::timelineMarkerLabelRect (getLocalBounds(), state, markerIndex)
                        .contains (event.getPosition()))
                {
                    markerDragIndex = markerIndex;
                    markerDragDownX = event.getPosition().x;
                    return;
                }
            }

            // G2.2 (plan §3.1): the UPPER row (bars) is the cycle row — click locates, a drag
            // sets the loop (Logic); the lower rows keep click = locate, drag = Time selection
            // (Pro Tools). Shift-drag stays the loop gesture on any row.
            if (yesdaw::ui::timeline_canvas_detail::rulerRows (geometry.rulerArea).bars.contains (event.getPosition()))
            {
                if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                {
                    if (onTimelineLocated)
                        onTimelineLocated (*seconds);
                    loopDragActive = true;
                    loopDragStartSeconds = *seconds;
                }
                return;
            }
            playheadLocateActive = true;
            rulerRangeDownPosition = event.getPosition();
            rulerRangeCurrentPosition = event.getPosition();
            rulerRangeDragStartSeconds =
                timelineSecondsAt (state, getLocalBounds(), event.getPosition()).value_or (0.0);
            if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                if (onTimelineLocated)
                    onTimelineLocated (*seconds);
            return;
        }

        playheadLocateActive = false;
        if (geometry.clipArea.contains (event.getPosition()))
        {
            marqueeState.active = true;
            marqueeState.downPosition = event.getPosition();
            marqueeState.currentPosition = event.getPosition();
            if (onEmptyClicked)
                onEmptyClicked();
            repaint();
            return;
        }

        if (onEmptyClicked)
            onEmptyClicked();
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (markerDragIndex >= 0)
            return;

        if (loopBraceDrag != LoopBraceEdit::None && stateProvider)
        {
            const yesdaw::ui::TimelineCanvasState state = stateProvider();
            if (const std::optional<double> seconds =
                    timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                loopBracePointerSeconds = *seconds;
            repaint();
            return;
        }

        if (marqueeState.active && stateProvider)
        {
            const yesdaw::ui::TimelineCanvasGeometry geometry =
                yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), stateProvider());
            marqueeState.currentPosition = geometry.clipArea.getConstrainedPoint (event.getPosition());
            const int deltaX = marqueeState.currentPosition.x - marqueeState.downPosition.x;
            const int deltaY = marqueeState.currentPosition.y - marqueeState.downPosition.y;
            marqueeState.moved = std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels
                              || std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels;
            repaint();
            return;
        }

        if (playheadLocateActive && stateProvider)
        {
            // A plain ruler drag past the dead zone becomes a range selection; the playhead stays at
            // the mouse-down locate instead of scrubbing (parity item 25).
            const int deltaX = event.getPosition().x - rulerRangeDownPosition.x;
            if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;

            playheadLocateActive = false;
            rulerRangeDragActive = true;
        }

        if (rulerRangeDragActive)
        {
            rulerRangeCurrentPosition = event.getPosition();
            repaint();
            return;
        }

        if (handDragActive && stateProvider)
        {
            const yesdaw::ui::TimelineCanvasState state = stateProvider();
            const yesdaw::ui::TimelineCanvasGeometry geometry =
                yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
            const double pixelsPerSecond = std::max (
                yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
                geometry.viewport.pixelsPerSecond);
            const int deltaX = event.getPosition().x - handDragLastX;
            handDragLastX = event.getPosition().x;
            if (deltaX != 0 && onHandToolScrolled)
                onHandToolScrolled (-static_cast<double> (deltaX) / pixelsPerSecond);
            return;
        }

        if (dragState.active)
        {
            dragState.moved = true;
            dragState.currentPosition = event.getPosition();
            dragState.snapInvert = event.mods.isCtrlDown();
            updateAutoScroll (event.getPosition());   // G2.3
            repaint();                                // the ghost
        }
    }

    // G2.3: edge-band auto-scroll. A clip drag near the clip area's left / right edge scrolls
    // the view by a fraction of the visible window per tick; the drag's press anchor shifts
    // with it so the release lands where the ghost shows. The harness ticks it directly.
    std::function<void (double)> onAutoScrolled;   // secondsDelta (the hand tool's law)
    std::function<double (double)> snapSecondsForPreview;   // the shell's snap-to-grid, for the landing line

    void updateAutoScroll (juce::Point<int> position)
    {
        autoScrollDirection = 0;
        if (dragState.active && stateProvider)
        {
            const yesdaw::ui::TimelineCanvasState state = stateProvider();
            const yesdaw::ui::TimelineCanvasGeometry geometry =
                yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
            const int band = yesdaw::ui::UiTheme::Layout::timelineAutoScrollEdgeBandPx;
            if (position.x >= geometry.clipArea.getRight() - band)
                autoScrollDirection = 1;
            else if (position.x <= geometry.clipArea.getX() + band)
                autoScrollDirection = -1;
        }
        if (autoScrollDirection != 0)
        {
            if (! isTimerRunning())
                startTimer (yesdaw::ui::UiTheme::Layout::timelineAutoScrollIntervalMs);
        }
        else
            stopTimer();
    }

    // One auto-scroll step; returns the seconds scrolled (0 when nothing to do).
    double autoScrollTick()
    {
        if (! dragState.active || autoScrollDirection == 0 || ! stateProvider || ! onAutoScrolled)
        {
            stopTimer();
            return 0.0;
        }
        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
        const double pps = std::max (yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
                                     geometry.viewport.pixelsPerSecond);
        const double visibleSeconds = static_cast<double> (geometry.clipArea.getWidth()) / pps;
        double delta = autoScrollDirection * visibleSeconds * yesdaw::ui::UiTheme::Layout::timelineAutoScrollStepFraction;
        if (delta < 0.0)
            delta = std::max (delta, -state.viewport.scrollSeconds);   // never before zero
        if (std::abs (delta) <= 0.0)
            return 0.0;
        onAutoScrolled (delta);
        dragState.downPosition.x -= juce::roundToInt (delta * pps);   // the anchor rides the scroll
        repaint();
        return delta;
    }

    void timerCallback() override { (void) autoScrollTick(); }

    void mouseUp (const juce::MouseEvent& event) override
    {
        stopTimer();   // G2.3
        autoScrollDirection = 0;
        if (markerDragIndex >= 0)
        {
            const int markerIndex = markerDragIndex;
            markerDragIndex = -1;
            if (stateProvider)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                const std::optional<double> seconds =
                    timelineSecondsAt (state, getLocalBounds(), event.getPosition());
                if (std::abs (event.getPosition().x - markerDragDownX)
                        < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                {
                    // A plain click on a marker label keeps the historical ruler-click locate.
                    if (seconds && onTimelineLocated)
                        onTimelineLocated (*seconds);
                }
                else if (seconds && onMarkerDragged)
                {
                    onMarkerDragged (markerIndex, *seconds, event.mods.isCtrlDown());
                }
            }
            return;
        }

        if (loopBraceDrag != LoopBraceEdit::None)
        {
            const LoopBraceEdit kind = loopBraceDrag;
            loopBraceDrag = LoopBraceEdit::None;
            if (stateProvider && onLoopBraceEdited)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                if (const std::optional<double> seconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                    onLoopBraceEdited (kind, *seconds, loopBraceGrabOffsetSeconds, event.mods.isCtrlDown());
            }
            repaint();
            return;
        }

        if (handDragActive)
        {
            handDragActive = false;
            return;
        }

        if (marqueeState.active)
        {
            const TimelineMarqueeState marquee = marqueeState;
            marqueeState = {};
            repaint();

            if (marquee.moved && stateProvider && onMarqueeSelection)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                std::array<int, yesdaw::ui::UiTheme::Layout::timelineCanvasVisibleClipCapacity> selectedIds {};
                const int selectedCount = clipIdsIntersectingMarquee (
                    getLocalBounds(), state, marqueeBounds (marquee), selectedIds.data(),
                    static_cast<int> (selectedIds.size()));
                onMarqueeSelection (std::span<const int> (selectedIds.data(),
                                                         static_cast<std::size_t> (selectedCount)));
            }
            return;
        }

        if (loopDragActive)
        {
            loopDragActive = false;
            if (stateProvider && onLoopRegionDragged)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                if (const std::optional<double> endSeconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                {
                    const double first = std::min (loopDragStartSeconds, *endSeconds);
                    const double second = std::max (loopDragStartSeconds, *endSeconds);
                    if (second > first)
                        onLoopRegionDragged (first, second, event.mods.isCtrlDown());
                }
            }
            return;
        }

        if (punchDragActive)
        {
            punchDragActive = false;
            if (stateProvider && onPunchRegionDragged)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                if (const std::optional<double> endSeconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                {
                    const double first = std::min (punchDragStartSeconds, *endSeconds);
                    const double second = std::max (punchDragStartSeconds, *endSeconds);
                    // second <= first means "clear"
                    onPunchRegionDragged (first, second, event.mods.isCtrlDown());
                }
            }
            return;
        }

        if (rulerRangeDragActive)
        {
            rulerRangeDragActive = false;
            repaint();
            if (stateProvider && onRulerRangeSelected)
            {
                const yesdaw::ui::TimelineCanvasState state = stateProvider();
                if (const std::optional<double> endSeconds =
                        timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                {
                    const double first = std::min (rulerRangeDragStartSeconds, *endSeconds);
                    const double second = std::max (rulerRangeDragStartSeconds, *endSeconds);
                    if (second > first)
                        onRulerRangeSelected (first, second, event.mods.isCtrlDown());
                }
            }
            return;
        }

        if (playheadLocateActive)
        {
            playheadLocateActive = false;
            // A plain ruler click collapses any committed range selection (the locate already
            // happened on mouse-down).
            if (onRulerRangeCleared)
                onRulerRangeCleared();
            return;
        }

        if (! dragState.active)
            return;

        const TimelineDragState drag = dragState;
        dragState = {};

        const int deltaX = event.getPosition().x - drag.downPosition.x;
        const int deltaY = event.getPosition().y - drag.downPosition.y;
        if (! drag.moved || ! stateProvider)
            return;

        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const std::optional<double> eventSeconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition());
        if (drag.mode == TimelineDragMode::TimeSelect)
        {
            // G2.4: the lower band's drag is the ruler's Time selection, made from the lanes.
            if (eventSeconds && onRulerRangeSelected)
            {
                const std::optional<double> downSeconds = timelineSecondsAt (state, getLocalBounds(), drag.downPosition);
                if (downSeconds)
                {
                    const double first = std::min (*downSeconds, *eventSeconds);
                    const double second = std::max (*downSeconds, *eventSeconds);
                    if (second > first)
                        onRulerRangeSelected (first, second, event.mods.isCtrlDown());
                }
            }
            return;
        }
        if (drag.mode == TimelineDragMode::Slip)   // G2.11
        {
            if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;
            const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
            const double pixelsPerSecond = std::max (yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
                                                     geometry.viewport.pixelsPerSecond);
            if (onClipSlipped)
                onClipSlipped (drag.layoutClipId, static_cast<double> (deltaX) / pixelsPerSecond);
            return;
        }

        if (drag.mode == TimelineDragMode::TrimRight)
        {
            if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;

            if (eventSeconds)
                if (onClipTrimmedRight)
                    onClipTrimmedRight (drag.layoutClipId, *eventSeconds, event.mods.isCtrlDown());
            return;
        }

        if (drag.mode == TimelineDragMode::StretchRight)   // G2.9b
        {
            if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;

            if (eventSeconds)
                if (onClipStretchedRight)
                    onClipStretchedRight (drag.layoutClipId, *eventSeconds, event.mods.isCtrlDown());
            return;
        }

        if (drag.mode == TimelineDragMode::TrimLeft)
        {
            if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;

            if (eventSeconds)
                if (onClipTrimmedLeft)
                    onClipTrimmedLeft (drag.layoutClipId, *eventSeconds, event.mods.isCtrlDown());
            return;
        }

        if (drag.mode == TimelineDragMode::Gain)
        {
            if (std::abs (deltaY) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
                return;

            if (onClipGainAdjusted)
                onClipGainAdjusted (drag.layoutClipId, -deltaY);
            return;
        }

        if (drag.mode == TimelineDragMode::FadeIn || drag.mode == TimelineDragMode::FadeOut)
        {
            // G2.10: the horizontal travel is the fade's length; the vertical travel bends its curve
            // (up = a faster rise, Logic's curve drag). Either past the dead zone is a gesture.
            const bool movedX = std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels;
            const bool movedY = std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels;
            if ((! movedX && ! movedY) || ! eventSeconds)
                return;

            const double fadeSeconds = ! movedX ? -1.0   // the sentinel: keep the current fade
                                     : drag.mode == TimelineDragMode::FadeIn
                                         ? *eventSeconds - drag.startSeconds
                                         : (drag.startSeconds + drag.lengthSeconds) - *eventSeconds;
            const double curveDelta = movedY
                ? -static_cast<double> (deltaY) / yesdaw::ui::UiTheme::Layout::timelineFadeCurveDragPixelsPerUnit
                : 0.0;

            if (onClipFadeAdjusted)
                onClipFadeAdjusted (
                    drag.layoutClipId,
                    drag.mode == TimelineDragMode::FadeIn,
                    fadeSeconds < 0.0 ? -1.0 : std::clamp (fadeSeconds, 0.0, drag.lengthSeconds),
                    curveDelta);
            return;
        }

        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);

        // A Move drag is two-dimensional: horizontal drag repositions in time, and a vertical drag past
        // the dead zone drops the Clip on another Track lane (the Pro Tools/Logic cross-track move).
        int targetLane = -1;
        if (state.trackCount > 0 && geometry.laneHeight > 0
            && std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
        {
            const int lane = geometry.laneAtPixel (event.getPosition().y - geometry.clipArea.getY()
                                                   + geometry.viewport.laneScrollPixels);
            targetLane = std::clamp (lane, 0, state.trackCount - 1);
            if (const yesdaw::ui::Clip* clip = findClipByLayoutId (state, drag.layoutClipId))
                if (clip->lane == targetLane)
                    targetLane = -1;   // dropped back on its own lane: a plain horizontal move
        }

        if (targetLane < 0 && std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels)
            return;

        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const double nextStartSeconds = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinateSecondsFloor,
            drag.startSeconds + static_cast<double> (deltaX) / pixelsPerSecond);

        if (drag.copy)
        {
            if (onClipCopied)
                onClipCopied (drag.layoutClipId, targetLane, nextStartSeconds,
                              drag.mode == TimelineDragMode::SnapMove);
            return;
        }

        if (targetLane >= 0)
        {
            if (onClipMovedToLane)
                onClipMovedToLane (drag.layoutClipId, targetLane, nextStartSeconds,
                                   drag.mode == TimelineDragMode::SnapMove);
            return;
        }

        if (onClipMoved)
            onClipMoved (drag.layoutClipId, nextStartSeconds, drag.mode == TimelineDragMode::SnapMove);
    }

    // E5 wheel map: Ctrl zooms, Shift scrolls horizontally, and the plain wheel scrolls the
    // shared track-row offset vertically.
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (! stateProvider)
            return;

        const double delta = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
        if (delta == 0.0)
            return;

        if (event.mods.isCtrlDown() || event.mods.isCommandDown())
        {
            const yesdaw::ui::TimelineCanvasState state = stateProvider();
            const double anchor =
                timelineSecondsAt (state, getLocalBounds(), event.getPosition()).value_or (state.viewport.scrollSeconds);
            if (onZoomWheel)
                onZoomWheel (anchor, static_cast<double> (wheel.deltaY));
            return;
        }

        if (event.mods.isShiftDown())
        {
            if (onScrollWheel)
                onScrollWheel (delta);
            return;
        }

        if (onVerticalScrollRows)
            onVerticalScrollRows (delta > 0.0 ? -1 : 1);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        const yesdaw::ui::TimelineCanvasState state = stateProvider();
        const yesdaw::ui::TimelineHitTestResult hit =
            yesdaw::ui::hitTestTimelineCanvas (getLocalBounds(), state, event.getPosition());
        if (! hit.hit)
        {
            const yesdaw::ui::TimelineCanvasGeometry geometry =
                yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
            if (geometry.rulerArea.contains (event.getPosition()))
            {
                // Marker editing (E7): double-click on a marker label opens the inline rename.
                for (int markerIndex = 0; markerIndex < state.markerCount; ++markerIndex)
                {
                    if (yesdaw::ui::timelineMarkerLabelRect (getLocalBounds(), state, markerIndex)
                            .contains (event.getPosition()))
                    {
                        if (onMarkerRenameRequested)
                            onMarkerRenameRequested (markerIndex);
                        return;
                    }
                }

                if (const std::optional<double> seconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
                    if (onTimelineLocated)
                        onTimelineLocated (*seconds);
            }
            return;
        }

        if (onClipClicked)
            onClipClicked (hit.id, false);

        // G2.12: a double-click in the clip's NAME band renames it inline; the body keeps its split.
        if (const yesdaw::ui::Clip* const namedClip = findClipByLayoutId (state, hit.id))
        {
            const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (getLocalBounds(), state);
            const yesdaw::ui::ClipPixelRect rect = yesdaw::ui::visibleClipPixelRect (*namedClip, yesdaw::ui::viewportForClipLayout (geometry));
            const double clipTopY = static_cast<double> (geometry.clipArea.getY()) + rect.y;
            if (static_cast<double> (event.getPosition().y) < clipTopY + yesdaw::ui::UiTheme::Layout::timelineClipNameBandHeight)
            {
                if (onClipRenameRequested)
                    onClipRenameRequested (hit.id);
                return;
            }
        }
        if (onClipDoubleClicked && onClipDoubleClicked (hit.id))
            return;

        if (const std::optional<double> splitSeconds = timelineSecondsAt (state, getLocalBounds(), event.getPosition()))
            if (onClipSplit)
                onClipSplit (hit.id, *splitSeconds, event.mods.isCtrlDown());
    }

public:
    enum class TimelineDragMode   // public: the zone law's return type is part of the harness contract (G2.4)
    {
        Move,
        SnapMove,
        TrimLeft,
        TrimRight,
        Gain,
        FadeIn,
        FadeOut,
        TimeSelect,  // G2.4: the clip body's lower band drags a Time selection (Pro Tools smart tool)
        StretchRight, // G2.9b: Alt on the right edge time-stretches the clip (Logic's Option-drag)
        Slip         // G2.11: Ctrl+Alt on the body slips the source under a fixed window (Logic slip)
    };
private:

    // G2.4: one name per zone, for the hint, the harness and the gate.
    [[nodiscard]] static const char* dragModeName (TimelineDragMode mode) noexcept
    {
        switch (mode)
        {
            case TimelineDragMode::Move:       return "move";
            case TimelineDragMode::SnapMove:   return "snap-move";
            case TimelineDragMode::TrimLeft:   return "trim-left";
            case TimelineDragMode::TrimRight:  return "trim-right";
            case TimelineDragMode::Gain:       return "gain";
            case TimelineDragMode::FadeIn:     return "fade-in";
            case TimelineDragMode::FadeOut:    return "fade-out";
            case TimelineDragMode::TimeSelect: return "time-select";
            case TimelineDragMode::StretchRight: return "stretch-right";
            case TimelineDragMode::Slip:       return "slip";
        }
        return "move";
    }

    // G2.4: the cursor announces the zone BEFORE the press (every DAW).
    [[nodiscard]] static juce::MouseCursor cursorForDragMode (TimelineDragMode mode) noexcept
    {
        switch (mode)
        {
            case TimelineDragMode::TrimLeft:
            case TimelineDragMode::TrimRight:
            case TimelineDragMode::StretchRight: return juce::MouseCursor::LeftRightResizeCursor;
            case TimelineDragMode::FadeIn:     return juce::MouseCursor::TopLeftCornerResizeCursor;
            case TimelineDragMode::FadeOut:    return juce::MouseCursor::TopRightCornerResizeCursor;
            case TimelineDragMode::TimeSelect: return juce::MouseCursor::IBeamCursor;
            case TimelineDragMode::Slip:       return juce::MouseCursor::DraggingHandCursor;   // G2.11
            case TimelineDragMode::Gain:       return juce::MouseCursor::UpDownResizeCursor;
            case TimelineDragMode::Move:
            case TimelineDragMode::SnapMove:   return juce::MouseCursor::NormalCursor;
        }
        return juce::MouseCursor::NormalCursor;
    }

    struct TimelineDragState
    {
        bool active = false;
        bool moved = false;
        bool copy = false;
        int layoutClipId = -1;
        double startSeconds = 0.0;
        double lengthSeconds = 0.0;
        TimelineDragMode mode = TimelineDragMode::Move;
        juce::Point<int> downPosition;
        juce::Point<int> currentPosition;   // G2.3: the ghost follows the pointer
        bool snapInvert = false;            // G2.3: Ctrl held mid-drag (trims / fades read it at release)
    };

    struct TimelineMarqueeState
    {
        bool active = false;
        bool moved = false;
        juce::Point<int> downPosition;
        juce::Point<int> currentPosition;
    };

    [[nodiscard]] static juce::Rectangle<int> marqueeBounds (const TimelineMarqueeState& marquee) noexcept
    {
        return juce::Rectangle<int>::leftTopRightBottom (
            std::min (marquee.downPosition.x, marquee.currentPosition.x),
            std::min (marquee.downPosition.y, marquee.currentPosition.y),
            std::max (marquee.downPosition.x, marquee.currentPosition.x),
            std::max (marquee.downPosition.y, marquee.currentPosition.y));
    }

    [[nodiscard]] juce::Rectangle<int> marqueeBounds() const noexcept
    {
        return marqueeBounds (marqueeState);
    }

    [[nodiscard]] static int clipIdsIntersectingMarquee (juce::Rectangle<int> area,
                                                         const yesdaw::ui::TimelineCanvasState& state,
                                                         juce::Rectangle<int> marquee,
                                                         int* outIds,
                                                         int outCapacity)
    {
        if (state.clips == nullptr || state.clipCount <= 0 || outIds == nullptr || outCapacity <= 0)
            return 0;

        const yesdaw::ui::TimelineCanvasGeometry geometry =
            yesdaw::ui::timelineCanvasGeometry (area, state);
        marquee = marquee.getIntersection (geometry.clipArea);
        if (marquee.isEmpty())
            return 0;

        std::array<yesdaw::ui::ElementRect,
                   yesdaw::ui::UiTheme::Layout::timelineCanvasVisibleClipCapacity> visible {};
        const int visibleCount = yesdaw::ui::layoutVisible (
            state.clips, state.clipCount, yesdaw::ui::viewportForClipLayout (geometry),
            visible.data(), static_cast<int> (visible.size()));

        int count = 0;
        for (int i = 0; i < visibleCount && count < outCapacity; ++i)
        {
            const yesdaw::ui::ElementRect& rect = visible[static_cast<std::size_t> (i)];
            const auto hitBounds = juce::Rectangle<int> (
                                       geometry.clipArea.getX() + juce::roundToInt (rect.x),
                                       geometry.clipArea.getY() + juce::roundToInt (rect.y),
                                       juce::roundToInt (rect.w),
                                       juce::roundToInt (rect.h))
                                       .getIntersection (geometry.clipArea);
            if (hitBounds.intersects (marquee))
                outIds[count++] = rect.id;
        }

        return count;
    }

    [[nodiscard]] static const yesdaw::ui::Clip* findClipByLayoutId (const yesdaw::ui::TimelineCanvasState& state,
                                                                     int layoutClipId) noexcept
    {
        if (state.clips == nullptr)
            return nullptr;

        for (int i = 0; i < state.clipCount; ++i)
            if (state.clips[i].id == layoutClipId)
                return &state.clips[i];

        return nullptr;
    }

    [[nodiscard]] static std::optional<double> timelineSecondsAt (const yesdaw::ui::TimelineCanvasState& state,
                                                                  juce::Rectangle<int> bounds,
                                                                  juce::Point<int> position) noexcept
    {
        const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (bounds, state);
        if (! geometry.clipArea.getHorizontalRange().contains (position.x)
            || (! geometry.clipArea.contains (position) && ! geometry.rulerArea.contains (position)))
            return std::nullopt;

        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const double seconds = geometry.viewport.scrollSeconds
                             + static_cast<double> (position.x - geometry.clipArea.getX()) / pixelsPerSecond;
        return std::max (yesdaw::ui::UiTheme::Layout::timelineCoordinateSecondsFloor, seconds);
    }

    [[nodiscard]] static TimelineDragMode dragModeForPointer (const yesdaw::ui::TimelineCanvasState& state,
                                                              juce::Rectangle<int> bounds,
                                                              int layoutClipId,
                                                              juce::Point<int> position,
                                                              juce::ModifierKeys modifiers) noexcept
    {
        const yesdaw::ui::Clip* const clip = findClipByLayoutId (state, layoutClipId);
        if (clip == nullptr)
            return TimelineDragMode::Move;

        const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (bounds, state);
        const double pixelsPerSecond = std::max (
            yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
            geometry.viewport.pixelsPerSecond);
        const double clipLeftX = static_cast<double> (geometry.clipArea.getX())
                               + (clip->startSeconds - geometry.viewport.scrollSeconds) * pixelsPerSecond;
        const double clipRightX = static_cast<double> (geometry.clipArea.getX())
                                + ((clip->startSeconds + clip->lengthSeconds) - geometry.viewport.scrollSeconds)
                                      * pixelsPerSecond;

        // R1: the edge zones only bite on a clip painted wide enough to keep a grabbable
        // middle (the piano roll's E12 law) — otherwise a short clip at a wide zoom is all
        // edge and could never be moved. Modifier gestures (Shift gain, Ctrl snap-invert,
        // Alt body copy-drag) stay available at any width.
        // G2.4 Smart tool (plan §3.1; Pro Tools smart tool, Logic pointer zones): on a clip wide
        // enough to grab, the edges trim, the TOP corners fade (Alt on an edge still fades, the
        // old gesture), the LOWER band drags a Time selection, the rest moves. A narrow clip is
        // all body, so it can always be grabbed.
        const yesdaw::ui::ClipPixelRect rect = yesdaw::ui::visibleClipPixelRect (*clip, yesdaw::ui::viewportForClipLayout (geometry));
        const double clipTopY = static_cast<double> (geometry.clipArea.getY()) + rect.y;
        const double clipHeight = rect.h;
        const bool inTopBand = clipHeight > 0.0
                            && static_cast<double> (position.y) < clipTopY + clipHeight * yesdaw::ui::UiTheme::Layout::timelineClipFadeCornerBandFraction;
        const bool inLowerBand = clipHeight > 0.0
                              && static_cast<double> (position.y) >= clipTopY + clipHeight * (1.0 - yesdaw::ui::UiTheme::Layout::timelineClipTimeSelectBandFraction);
        if (clipRightX - clipLeftX
            >= static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineClipEdgeMinGrabWidth))
        {
            const bool nearLeft = std::fabs (static_cast<double> (position.x) - clipLeftX)
                               <= static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineClipEdgeHitWidth);
            const bool nearRight = std::fabs (static_cast<double> (position.x) - clipRightX)
                                <= static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineClipEdgeHitWidth);
            if (nearLeft && (inTopBand || modifiers.isAltDown()))
                return TimelineDragMode::FadeIn;
            if (nearRight && inTopBand)
                return TimelineDragMode::FadeOut;
            if (nearRight && modifiers.isAltDown())
                return TimelineDragMode::StretchRight;   // G2.9b: Logic's Option-drag on the right edge
            if (nearRight)
                return TimelineDragMode::TrimRight;
            if (nearLeft)
                return TimelineDragMode::TrimLeft;
            if (inLowerBand && ! modifiers.isShiftDown())
                return TimelineDragMode::TimeSelect;   // Ctrl only defeats snap here, as on the ruler
        }

        if (modifiers.isShiftDown())
            return TimelineDragMode::Gain;

        if (modifiers.isCtrlDown() && modifiers.isAltDown())
            return TimelineDragMode::Slip;   // G2.11: the source moves under the window

        if (modifiers.isCtrlDown())
            return TimelineDragMode::SnapMove;

        return TimelineDragMode::Move;
    }

    TimelineDragState dragState;
    int autoScrollDirection = 0;   // G2.3: -1 left band, +1 right band, 0 none
    TimelineMarqueeState marqueeState;
    // Hand tool (E3): a press-drag pans the viewport horizontally; transient view state only.
    bool handDragActive = false;
    int handDragLastX = 0;
    // Loop brace drag (E6): raw-pointer preview state; the commit applies the snap chooser.
    LoopBraceEdit loopBraceDrag = LoopBraceEdit::None;
    double loopBracePointerSeconds = 0.0;
    double loopBraceGrabOffsetSeconds = 0.0;
    // Marker drag (E7): a below-dead-zone release keeps the historical ruler-click locate.
    int markerDragIndex = -1;
    int markerDragDownX = 0;
    bool playheadLocateActive = false;
    bool loopDragActive = false;
    double loopDragStartSeconds = 0.0;
    bool punchDragActive = false;
    double punchDragStartSeconds = 0.0;
    bool rulerRangeDragActive = false;
    double rulerRangeDragStartSeconds = 0.0;
    juce::Point<int> rulerRangeDownPosition;
    juce::Point<int> rulerRangeCurrentPosition;
};

} // namespace yesdaw::ui
