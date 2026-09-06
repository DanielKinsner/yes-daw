// YES DAW — the piano roll's geometry and input overlay.
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

// The snap grid the piano roll's quantize verbs use (moved with the component).
inline constexpr yesdaw::engine::Tick kPianoRollSnapGridTicks =
    yesdaw::ui::UiTheme::Layout::pianoRollGridTickStep;

struct PianoRollCanvasGeometry
{
    juce::Rectangle<int> expression;
    juce::Rectangle<int> keyboard;
    juce::Rectangle<int> grid;
    float rowHeight = 1.0f;
    int visibleKeys = yesdaw::ui::UiTheme::Layout::pianoRollKeyCount;   // G3.2 FIX 3: the key window
};

[[nodiscard]] inline PianoRollCanvasGeometry pianoRollCanvasGeometry (juce::Rectangle<int> area) noexcept
{
    area.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollHeaderHeight);
    area.reduce (yesdaw::ui::UiTheme::Layout::pianoRollPanelInsetX,
                 yesdaw::ui::UiTheme::Layout::pianoRollPanelInsetY);
    PianoRollCanvasGeometry geometry;
    geometry.expression = area.removeFromBottom (yesdaw::ui::UiTheme::Layout::pianoRollExpressionHeight);
    geometry.keyboard = area.removeFromLeft (yesdaw::ui::UiTheme::Layout::pianoRollKeyboardWidth);
    geometry.grid = area.reduced (yesdaw::ui::UiTheme::Layout::pianoRollGridInsetX,
                                  yesdaw::ui::UiTheme::Layout::pianoRollGridInsetY);
    geometry.visibleKeys = yesdaw::ui::UiTheme::Layout::pianoRollVisibleKeys (geometry.grid.getHeight());
    geometry.rowHeight = static_cast<float> (juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollGridMinHeight,
                                                         geometry.grid.getHeight()))
                       / static_cast<float> (geometry.visibleKeys);
    return geometry;
}

[[nodiscard]] inline yesdaw::engine::Tick pianoRollTimelineLength (
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface) noexcept
{
    return juce::jmax<yesdaw::engine::Tick> (1, surface.timelineLength);
}

// Piano-roll viewport (E10): the visible horizontal window is the clip length divided by the
// zoom, offset by the scroll ticks; the vertical window is the 25 keys above viewLowKey. With
// the default view this reproduces the historical stretched full-clip C3-C5 mapping exactly.
[[nodiscard]] inline yesdaw::engine::Tick pianoRollVisibleTicks (
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface) noexcept
{
    const double zoom = std::clamp (surface.viewZoom,
                                    yesdaw::ui::UiThemeLayout::pianoRollZoomMin,
                                    yesdaw::ui::UiThemeLayout::pianoRollZoomMax);
    return juce::jmax<yesdaw::engine::Tick> (
        1, static_cast<yesdaw::engine::Tick> (
               std::llround (static_cast<double> (pianoRollTimelineLength (surface)) / zoom)));
}

[[nodiscard]] inline int pianoRollViewHighKey (const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface) noexcept
{
    return surface.viewLowKey + surface.viewKeyCount - 1;   // G3.2 FIX 3: the window in force
}


[[nodiscard]] inline int pianoRollKeyY (const PianoRollCanvasGeometry& geometry,
                                 const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
                                 int key) noexcept
{
    return geometry.grid.getY()
         + juce::roundToInt (
             static_cast<float> (pianoRollViewHighKey (surface) - key) * geometry.rowHeight);
}

// G3.2: the key under a y in the roll (the keyboard column shares the grid's rows); -1 outside the
// view or off the keyboard.
[[nodiscard]] inline int pianoRollKeyAtY (const PianoRollCanvasGeometry& geometry,
                                   const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
                                   int y) noexcept
{
    const float rowHeight = geometry.rowHeight > 1.0f ? geometry.rowHeight : 1.0f;
    const int key = pianoRollViewHighKey (surface)
        - static_cast<int> (static_cast<float> (y - geometry.grid.getY()) / rowHeight);
    if (key < surface.viewLowKey || key > pianoRollViewHighKey (surface)
        || key < yesdaw::ui::UiThemeLayout::pianoRollKeyMin || key > yesdaw::ui::UiThemeLayout::pianoRollKeyMax)
        return -1;
    return key;
}

[[nodiscard]] inline int pianoRollTickX (const PianoRollCanvasGeometry& geometry,
                                  const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
                                  yesdaw::engine::Tick tick) noexcept
{
    const double visibleTicks = static_cast<double> (pianoRollVisibleTicks (surface));
    const double normalized = static_cast<double> (tick - surface.viewScrollTicks) / visibleTicks;
    return geometry.grid.getX()
         + juce::roundToInt (static_cast<float> (normalized) * static_cast<float> (geometry.grid.getWidth()));
}

// G3.2: the roll's grid lines — bars and beats from the meter, snap subdivisions only while a
// cell is at least pianoRollGridMinLinePx wide; ONE law for the paint and the gate.
[[nodiscard]] inline std::vector<yesdaw::ui::PianoRollGridLine> pianoRollGridLines (const PianoRollCanvasGeometry& geometry,
                                                                             const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface)
{
    std::vector<yesdaw::ui::PianoRollGridLine> lines;
    if (geometry.grid.getWidth() <= 0 || surface.timelineLength <= 0)
        return lines;
    const yesdaw::engine::Tick beat = std::max<yesdaw::engine::Tick> (1, surface.beatTicks);
    const yesdaw::engine::Tick bar = std::max<yesdaw::engine::Tick> (beat, surface.barTicks);
    const double pixelsPerTick = static_cast<double> (geometry.grid.getWidth()) / static_cast<double> (std::max<yesdaw::engine::Tick> (1, pianoRollVisibleTicks (surface)));
    yesdaw::engine::Tick step = beat;
    if (surface.snapEnabled && surface.snapGridTicks > 0 && surface.snapGridTicks < beat
        && static_cast<double> (surface.snapGridTicks) * pixelsPerTick >= yesdaw::ui::UiTheme::Layout::pianoRollGridMinLinePx)
        step = surface.snapGridTicks;
    for (yesdaw::engine::Tick tick = 0; tick <= surface.timelineLength; tick += step)
    {
        const int x = pianoRollTickX (geometry, surface, tick);
        if (x < geometry.grid.getX() || x > geometry.grid.getRight())
            continue;
        yesdaw::ui::PianoRollGridLine line;
        line.tick = tick;
        line.x = x;
        line.kind = (tick % bar) == 0 ? yesdaw::ui::PianoRollGridLineKind::Bar
                  : (tick % beat) == 0 ? yesdaw::ui::PianoRollGridLineKind::Beat
                  : yesdaw::ui::PianoRollGridLineKind::Snap;
        lines.push_back (line);
    }
    return lines;
}

[[nodiscard]] inline yesdaw::engine::Tick pianoRollTickDeltaForPixels (
    const PianoRollCanvasGeometry& geometry,
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
    int deltaPixels) noexcept
{
    const int gridWidth = juce::jmax (1, geometry.grid.getWidth());
    const double ticks = static_cast<double> (deltaPixels)
                       * static_cast<double> (pianoRollVisibleTicks (surface))
                       / static_cast<double> (gridWidth);
    return static_cast<yesdaw::engine::Tick> (std::llround (ticks));
}

[[nodiscard]] inline juce::Rectangle<int> pianoRollNoteBounds (
    const PianoRollCanvasGeometry& geometry,
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
    const yesdaw::ui::UiPianoRollNoteView& note) noexcept
{
    const int x = pianoRollTickX (geometry, surface, note.startTick);
    const int width = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollNoteMinWidth,
                                  pianoRollTickX (geometry, surface, note.startTick + note.lengthTicks) - x);
    const int y = pianoRollKeyY (geometry, surface, note.key)
                + yesdaw::ui::UiTheme::Layout::pianoRollNoteTopInset;
    const int height = juce::jmax (yesdaw::ui::UiTheme::Layout::pianoRollNoteMinHeight,
                                   juce::roundToInt (geometry.rowHeight)
                                       - yesdaw::ui::UiTheme::Layout::pianoRollNoteHeightTrim);
    return juce::Rectangle<int> (x, y, width, height)
        .reduced (yesdaw::ui::UiTheme::Layout::pianoRollNoteInsetX,
                  yesdaw::ui::UiTheme::Layout::pianoRollNoteInsetY);
}

// E13: the velocity lane is the FIRST expression lane; this mirrors the paint loop's inset
// chain exactly so lane input and lane paint share one law.
[[nodiscard]] inline juce::Rectangle<int> pianoRollVelocityLaneArea (
    const PianoRollCanvasGeometry& geometry) noexcept
{
    juce::Rectangle<int> expression = geometry.expression.reduced (
        yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetX,
        yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetY);
    return expression.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneHeight)
        .reduced (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetX,
                  yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetY);
}

// E13: invert the lane paint's value law — the y that painted a velocity maps back to it.
[[nodiscard]] inline double pianoRollVelocityForLaneY (juce::Rectangle<int> lane, int y) noexcept
{
    const double usable = static_cast<double> (
        juce::jmax (1, lane.getHeight() - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathVerticalInset));
    const double bottom = static_cast<double> (
        lane.getBottom() - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathBottomInset);
    return juce::jlimit (0.0, 1.0, (bottom - static_cast<double> (y)) / usable);
}

// E13: map a lane x back to a tick with the grid's time law.
[[nodiscard]] inline yesdaw::engine::Tick pianoRollTickForX (
    const PianoRollCanvasGeometry& geometry,
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
    int x) noexcept
{
    const int gridWidth = juce::jmax (1, geometry.grid.getWidth());
    const double normalized = juce::jlimit (
        0.0, 1.0, static_cast<double> (x - geometry.grid.getX()) / static_cast<double> (gridWidth));
    return surface.viewScrollTicks
         + static_cast<yesdaw::engine::Tick> (
               std::llround (normalized * static_cast<double> (pianoRollVisibleTicks (surface))));
}

// G3.3: the control lane is the SECOND expression lane. Its chooser sits in the keyboard gutter and
// its data area spans exactly the grid's x range, so a tick maps to the same x as a note above it.
// One inset chain with the paint loop (like the velocity lane's).
[[nodiscard]] inline juce::Rectangle<int> pianoRollControlLaneArea (const PianoRollCanvasGeometry& geometry) noexcept
{
    juce::Rectangle<int> expression = geometry.expression.reduced (
        yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetX,
        yesdaw::ui::UiTheme::Layout::pianoRollExpressionInsetY);
    expression.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneHeight);
    return expression.removeFromTop (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneHeight)
        .reduced (yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetX,
                  yesdaw::ui::UiTheme::Layout::pianoRollExpressionLaneInsetY);
}

[[nodiscard]] inline juce::Rectangle<int> pianoRollControlLaneChooserArea (const PianoRollCanvasGeometry& geometry) noexcept
{
    const juce::Rectangle<int> lane = pianoRollControlLaneArea (geometry);
    return lane.withWidth (juce::jmax (1, geometry.grid.getX() - lane.getX()
                                          - yesdaw::ui::UiTheme::Layout::pianoRollControlLaneChooserGap));
}

[[nodiscard]] inline juce::Rectangle<int> pianoRollControlLaneDataArea (const PianoRollCanvasGeometry& geometry) noexcept
{
    const juce::Rectangle<int> lane = pianoRollControlLaneArea (geometry);
    return lane.withLeft (geometry.grid.getX()).withRight (geometry.grid.getRight());
}

// The lane's value law: the velocity lane's pixel law stretched over [valueMin, valueMax].
[[nodiscard]] inline double pianoRollControlValueForLaneY (juce::Rectangle<int> lane, int y, double valueMin, double valueMax) noexcept
{
    return valueMin + pianoRollVelocityForLaneY (lane, y) * (valueMax - valueMin);
}

[[nodiscard]] inline int pianoRollControlLaneYForValue (juce::Rectangle<int> lane, double value, double valueMin, double valueMax) noexcept
{
    const double span = valueMax > valueMin ? valueMax - valueMin : 1.0;
    const double normalized = juce::jlimit (0.0, 1.0, (value - valueMin) / span);
    const double usable = static_cast<double> (
        juce::jmax (1, lane.getHeight() - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathVerticalInset));
    const double bottom = static_cast<double> (
        lane.getBottom() - yesdaw::ui::UiTheme::Layout::pianoRollExpressionPathBottomInset);
    return juce::roundToInt (bottom - normalized * usable);
}

[[nodiscard]] inline const yesdaw::ui::UiPianoRollExpressionLaneReadout* pianoRollControlLaneOf (
    const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface) noexcept
{
    for (const yesdaw::ui::UiPianoRollExpressionLaneReadout& lane : surface.expressionLanes)
        if (lane.kind == yesdaw::ui::UiPianoRollExpressionLaneKind::Control)
            return &lane;
    return nullptr;
}

class PianoRollInputComponent final : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    std::function<yesdaw::ui::UiPianoRollSurfaceSnapshot()> stateProvider;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId)> onNoteClicked;
    // E12: a drag moves the WHOLE selection by (tickDelta, keyDelta) anchored on the dragged
    // note; the left edge trims the note head with the end fixed.
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick, int)> onNotesDragged;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteHeadTrimmed;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteLengthChanged;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, std::int32_t)> onNoteTransposed;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteQuantized;
    std::function<void()> onExpressionRead;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::Tick, std::int16_t)> onNoteAdded;
    // Alt+wheel on a note adjusts its velocity (B33): clip, note, new normalized velocity.
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, double)> onNoteVelocityAdjusted;
    // E13: a drag in the velocity lane paints the crossed notes' velocities (the whole selection
    // paints together when a crossed note is selected) as one undo transaction.
    std::function<void (yesdaw::engine::EntityId,
                        std::span<const std::pair<yesdaw::engine::EntityId, double>>)> onVelocityLanePainted;
    // Ctrl+drag copy-drags a note (B35): clip, source note, copy's start tick.
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteCopyDragged;
    // Piano-roll selection tools (E11): the empty grid is tool-aware (Pencil adds, Pointer
    // deselects and marquee-selects), Shift+click toggles, plain double-click deletes a note.
    std::function<yesdaw::ui::TimelineTool()> activeToolProvider;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId)> onNoteToggled;
    std::function<void (yesdaw::engine::EntityId, std::span<const yesdaw::engine::EntityId>)> onNotesMarqueeSelected;
    std::function<void()> onSelectionCleared;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId)> onNoteDeleted;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick)> onNoteSplit;   // G3.2: the Scissors tool
    std::function<void (std::int16_t, bool)> onKeyAuditioned;   // G3.2: audition - key, on / off (keyboard column, note press, drawn note)
    // G3.3: the control lane's gestures — every edit lands on the release (the E13 velocity-lane law):
    // clip, tick, value / clip, point id, tick, value / clip, point id / clip, points, first tick, last tick.
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::Tick, double)> onControlPointAdded;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId, yesdaw::engine::Tick, double)> onControlPointMoved;
    std::function<void (yesdaw::engine::EntityId, yesdaw::engine::EntityId)> onControlPointDeleted;
    std::function<void (yesdaw::engine::EntityId,
                        std::span<const std::pair<yesdaw::engine::Tick, double>>,
                        yesdaw::engine::Tick,
                        yesdaw::engine::Tick)> onControlLanePainted;
    [[nodiscard]] int heldAuditionKey() const noexcept { return auditionKey; }   // G3.2: -1 = none
    // G3.3: the control lane's last gesture phase, for the probe ("down:pencil", "drag", "up:paint 7", "miss 12,34") —
    // a drive can see where a lane sweep stopped instead of guessing.
    [[nodiscard]] juce::String lastLaneGesture() const { return laneGesture; }
    [[nodiscard]] int lastAuditionKey() const noexcept { return lastAuditioned; }   // G3.2: the probe's record of the last press; -1 = never
    // Piano-roll viewport (E10): plain wheel scrolls keys, Shift+wheel scrolls time, Ctrl+wheel
    // zooms time anchored at the pointer tick. Alt+wheel keeps the B33 velocity law.
    std::function<void (int)> onViewKeysScrolled;                          // +1 up / -1 down
    std::function<void (yesdaw::engine::Tick, double)> onViewZoomWheel;    // anchorTick, wheelDelta
    std::function<void (double)> onViewTicksScrolled;                      // fraction of the visible window

    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (! stateProvider)
            return;

        const double delta = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
        if (delta == 0.0)
            return;

        if (event.mods.isAltDown())
        {
            if (! onNoteVelocityAdjusted)
                return;

            const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
            const auto hit = noteAt (surface, event.getPosition());
            if (! hit)
                return;

            const double adjusted = juce::jlimit (
                0.0,
                1.0,
                hit->normalizedVelocity
                    + static_cast<double> (wheel.deltaY)
                          * yesdaw::ui::UiTheme::Layout::pianoRollVelocityWheelScale);
            if (adjusted != hit->normalizedVelocity)
                onNoteVelocityAdjusted (surface.midiClipId, hit->noteId, adjusted);
            return;
        }

        if (event.mods.isCtrlDown() || event.mods.isCommandDown())
        {
            const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            const int gridWidth = juce::jmax (yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                              geometry.grid.getWidth());
            const double normalized = juce::jlimit (
                0.0, 1.0,
                static_cast<double> (event.getPosition().x - geometry.grid.getX())
                    / static_cast<double> (gridWidth));
            const yesdaw::engine::Tick anchorTick = surface.viewScrollTicks
                + static_cast<yesdaw::engine::Tick> (
                    normalized * static_cast<double> (pianoRollVisibleTicks (surface)));
            if (onViewZoomWheel)
                onViewZoomWheel (anchorTick, delta);
            return;
        }

        if (event.mods.isShiftDown())
        {
            if (onViewTicksScrolled)
                onViewTicksScrolled (delta);
            return;
        }

        if (onViewKeysScrolled)
            onViewKeysScrolled (delta > 0.0 ? 1 : -1);
    }

    // G1.6: the gesture hint for the hovered zone — the status line shows it while no status
    // message is active. hintAt() is the one law mouseMove and the harness share.
    std::function<void (const juce::String&)> onHoverHint;

    [[nodiscard]] juce::String hintAt (juce::Point<int> position, juce::ModifierKeys modifiers) const
    {
        if (! stateProvider)
            return {};
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
        // G1.6: the keyboard column IS interactive (a press auditions its key, G3.2), so it is
        // named like every other hovered zone — it was blind until 2026-09-04.
        if (geometry.keyboard.contains (position))
            return "Keyboard: press a key to hear it through the track's instrument · release to stop";
        if (surface.midiClipSelected && pianoRollControlLaneDataArea (geometry).contains (position))
        {
            // G3.3: the control lane names its controller and its three tools.
            juce::String hint ("Control lane");
            if (const auto* lane = pianoRollControlLaneOf (surface))
                hint << " (" << yesdaw::ui::pianoRollControlLaneChoice (lane->controlLaneChoice).name << ")";
            hint << ": the pointer (1) places or drags a point \u00b7 the pencil (2) paints \u00b7 Shift+pencil draws a line \u00b7 the eraser (4) removes";
            return hint;
        }
        if (surface.midiClipSelected && pianoRollVelocityLaneArea (geometry).contains (position))
            return "Velocity lane: drag to paint velocities";
        if (const auto hit = noteAt (surface, position))
        {
            switch (dragModeForPointer (surface, *hit, position, modifiers))
            {
                case PianoDragMode::TrimHead:  return "Note head: drag to trim";
                case PianoDragMode::SetLength: return "Note end: drag to set its length";
                case PianoDragMode::Move:      break;
                case PianoDragMode::VelocityDrag: break;   // never a pointer verdict; the Velocity tool's own mode
            }
            return "Note: drag to move \u00b7 up or down transposes \u00b7 Ctrl-drag copies \u00b7 Shift-drag sets length \u00b7 right-click for the note menu";
        }
        if (surface.midiClipSelected && geometry.grid.contains (position))
            return "Grid: the pencil (2) draws a note \u00b7 the pointer (1) drags a marquee";
        return {};
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        if (onHoverHint)
            onHoverHint (hintAt (event.getPosition(), event.mods));
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (onHoverHint)
            onHoverHint ({});
    }

    // G1.3: a right-click on a note selects it and asks for the Note context menu.
    std::function<void (yesdaw::ui::ContextMenuTarget, int, juce::Point<int>)> onContextMenuRequested;

    void requestContextMenu (juce::Point<int> position)
    {
        if (! stateProvider || ! onContextMenuRequested)
            return;
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const auto hit = noteAt (surface, position);
        if (! hit)
            return;
        if (onNoteClicked)
            onNoteClicked (surface.midiClipId, hit->noteId);
        onContextMenuRequested (yesdaw::ui::ContextMenuTarget::Note, -1, position);
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

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();

        // G3.2: the keyboard column auditions its key through the Track's Instrument (Logic's roll);
        // the press holds the note, the mouse-up releases it.
        {
            const PianoRollCanvasGeometry keyGeometry = pianoRollCanvasGeometry (getLocalBounds());
            if (keyGeometry.keyboard.contains (event.getPosition()))
            {
                beginAudition (pianoRollKeyAtY (keyGeometry, surface, event.getPosition().y));
                return;
            }
        }

        const auto hit = noteAt (surface, event.getPosition());
        if (! hit)
        {
            dragState = {};
            marqueeState = {};
            velocityDragState = {};
            controlDragState = {};
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            // G3.3: a press in the control lane — the eraser removes the point under it, the pencil
            // starts a paint (Shift: a line), the pointer grabs a point or places one on release.
            if (surface.midiClipSelected && pianoRollControlLaneDataArea (geometry).contains (event.getPosition()))
            {
                beginControlLaneGesture (surface, geometry, event);
                return;
            }
            laneGesture = "miss " + juce::String (event.getPosition().x) + "," + juce::String (event.getPosition().y)
                        + (surface.midiClipSelected ? "" : " (no clip)") + " lane " + pianoRollControlLaneDataArea (geometry).toString();
            // E13: a press in the velocity lane starts a velocity paint drag.
            if (surface.midiClipSelected
                && pianoRollVelocityLaneArea (geometry).contains (event.getPosition()))
            {
                const juce::Rectangle<int> lane = pianoRollVelocityLaneArea (geometry);
                velocityDragState.active = true;
                velocityDragState.midiClipId = surface.midiClipId;
                velocityDragState.downPosition = event.getPosition();
                velocityDragState.downTick = pianoRollTickForX (geometry, surface, event.getPosition().x);
                velocityDragState.downVelocity = pianoRollVelocityForLaneY (lane, event.getPosition().y);
                velocityDragState.currentTick = velocityDragState.downTick;
                velocityDragState.currentVelocity = velocityDragState.downVelocity;
                return;
            }
            if (! surface.midiClipSelected
                || ! geometry.grid.contains (event.getPosition())
                || geometry.grid.getWidth() <= 0)
                return;

            // The empty grid is tool-aware (E11): the Pencil adds a note at the clicked tick and
            // key; the Pointer clears the selection and starts a note marquee.
            const yesdaw::ui::TimelineTool tool =
                activeToolProvider ? activeToolProvider() : yesdaw::ui::TimelineTool::Pointer;
            if (tool == yesdaw::ui::TimelineTool::Pencil)
            {
                if (! onNoteAdded)
                    return;

                const double normalized =
                    static_cast<double> (event.getPosition().x - geometry.grid.getX())
                    / static_cast<double> (geometry.grid.getWidth());
                const auto visibleTicks = static_cast<double> (pianoRollVisibleTicks (surface));
                yesdaw::engine::Tick tick = surface.viewScrollTicks
                    + static_cast<yesdaw::engine::Tick> (normalized * visibleTicks);
                // E12: the pencil floors to the REAL snap chooser grid (chooser Off = raw).
                if (surface.snapEnabled && surface.snapGridTicks > 0)
                    tick -= tick % surface.snapGridTicks;

                const float rowHeight = geometry.rowHeight > 1.0f ? geometry.rowHeight : 1.0f;
                const int key = pianoRollViewHighKey (surface)
                    - static_cast<int> ((event.getPosition().y - geometry.grid.getY()) / rowHeight);
                if (key >= surface.viewLowKey && key <= pianoRollViewHighKey (surface)
                    && key >= yesdaw::ui::UiThemeLayout::pianoRollKeyMin
                    && key <= yesdaw::ui::UiThemeLayout::pianoRollKeyMax)
                {
                    onNoteAdded (surface.midiClipId, tick, static_cast<std::int16_t> (key));
                    beginAudition (key);   // G3.2: a drawn note sounds while the pencil holds it
                }
                return;
            }

            if (tool == yesdaw::ui::TimelineTool::Pointer)
            {
                if (onSelectionCleared)
                    onSelectionCleared();
                marqueeState.active = true;
                marqueeState.downPosition = geometry.grid.getConstrainedPoint (event.getPosition());
                marqueeState.currentPosition = marqueeState.downPosition;
                repaint();
            }
            return;
        }

        // G3.2: the roll honours the shared tools on a note hit — the Eraser deletes it, the
        // Scissors split it at the clicked tick, the Velocity tool starts a vertical velocity drag.
        {
            const yesdaw::ui::TimelineTool tool =
                activeToolProvider ? activeToolProvider() : yesdaw::ui::TimelineTool::Pointer;
            if (tool == yesdaw::ui::TimelineTool::Eraser)
            {
                if (onNoteDeleted)
                    onNoteDeleted (surface.midiClipId, hit->noteId);
                return;
            }
            if (tool == yesdaw::ui::TimelineTool::Scissors)
            {
                const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
                yesdaw::engine::Tick tick = pianoRollTickForX (geometry, surface, event.getPosition().x);
                // The split snaps like the arrangement's scissors; Ctrl defeats the snap (G2.7).
                if (surface.snapEnabled && surface.snapGridTicks > 0 && ! event.mods.isCtrlDown())
                    tick -= tick % surface.snapGridTicks;
                if (onNoteSplit)
                    onNoteSplit (surface.midiClipId, hit->noteId, tick);
                return;
            }
            if (tool == yesdaw::ui::TimelineTool::Velocity)
            {
                if (onNoteClicked)
                    onNoteClicked (surface.midiClipId, hit->noteId);
                dragState = {};
                dragState.active = true;
                dragState.noteId = hit->noteId;
                dragState.midiClipId = surface.midiClipId;
                dragState.startTick = hit->startTick;
                dragState.lengthTicks = hit->lengthTicks;
                dragState.downPosition = event.getPosition();
                dragState.mode = PianoDragMode::VelocityDrag;
                dragState.downVelocity = hit->normalizedVelocity;
                return;
            }
        }

        // Shift+click toggles the note in the multi-selection (E11); a Shift+DRAG keeps the
        // historical length-edit law, so the toggle is resolved on a movement-free mouse-up.
        if (event.mods.isShiftDown())
        {
            pendingShiftToggleNoteId = hit->noteId;
        }
        else if (onNoteClicked)
        {
            onNoteClicked (surface.midiClipId, hit->noteId);
        }
        beginAudition (hit->key);   // G3.2: a pressed note sounds

        dragState = {};
        dragState.active = true;
        dragState.noteId = hit->noteId;
        dragState.midiClipId = surface.midiClipId;
        dragState.startTick = hit->startTick;
        dragState.lengthTicks = hit->lengthTicks;
        dragState.downPosition = event.getPosition();
        dragState.mode = dragModeForPointer (surface, *hit, event.getPosition(), event.mods);
        // Ctrl+drag copy-drags the note (B35), mirroring the timeline's copy-drag law. Ctrl is an
        // explicit copy request, so it wins over the narrow note's resize-edge zone.
        dragState.copy = event.mods.isCtrlDown() && ! event.mods.isShiftDown();
        if (dragState.copy)
            dragState.mode = PianoDragMode::Move;
    }

    // Note marquee overlay (E11): painted above the roll canvas with the shared marquee style.
    void paint (juce::Graphics& g) override
    {
        if (! marqueeState.active || ! marqueeState.moved)
            return;

        const juce::Rectangle<int> marquee (marqueeState.downPosition, marqueeState.currentPosition);
        g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
            yesdaw::ui::UiTheme::Tone::pressedHighlightAlpha));
        g.fillRect (marquee);
        g.setColour (yesdaw::ui::UiTheme::Color::accentBlue().withAlpha (
            yesdaw::ui::UiTheme::Tone::focusRingAlpha));
        g.drawRect (marquee.toFloat(), yesdaw::ui::UiTheme::Layout::timelineCanvasOutlineStrokeWidth);
    }

    [[nodiscard]] bool cancelInProgressEdit()
    {
        if (! dragState.active && ! marqueeState.active && ! velocityDragState.active
            && ! controlDragState.active && ! pendingShiftToggleNoteId.isValid())
            return false;

        dragState = {};
        marqueeState = {};
        velocityDragState = {};
        controlDragState = {};
        pendingShiftToggleNoteId = {};
        repaint();
        return true;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (controlDragState.active)
        {
            if (! stateProvider)
                return;
            const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            controlDragState.currentTick = controlLaneTickAt (surface, geometry, event.getPosition().x, controlDragState.kind != ControlDragKind::Pencil);
            controlDragState.currentValue = controlLaneValueAt (surface, geometry, event.getPosition().y);
            const int deltaX = event.getPosition().x - controlDragState.downPosition.x;
            const int deltaY = event.getPosition().y - controlDragState.downPosition.y;
            controlDragState.moved = controlDragState.moved
                || std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels
                || std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels;
            if (controlDragState.kind == ControlDragKind::Pencil)
                controlDragState.samples.emplace_back (controlDragState.currentTick, controlDragState.currentValue);
            laneGesture = controlDragState.moved ? "drag" : "drag (dead zone)";
            return;
        }

        if (velocityDragState.active)
        {
            if (! stateProvider)
                return;
            const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            const juce::Rectangle<int> lane = pianoRollVelocityLaneArea (geometry);
            velocityDragState.currentTick = pianoRollTickForX (geometry, surface, event.getPosition().x);
            velocityDragState.currentVelocity = pianoRollVelocityForLaneY (lane, event.getPosition().y);
            const int deltaX = event.getPosition().x - velocityDragState.downPosition.x;
            const int deltaY = event.getPosition().y - velocityDragState.downPosition.y;
            velocityDragState.moved = velocityDragState.moved
                || std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels
                || std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels;
            return;
        }

        if (marqueeState.active)
        {
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            marqueeState.currentPosition = geometry.grid.getConstrainedPoint (event.getPosition());
            const int deltaX = marqueeState.currentPosition.x - marqueeState.downPosition.x;
            const int deltaY = marqueeState.currentPosition.y - marqueeState.downPosition.y;
            marqueeState.moved = std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels
                              || std::abs (deltaY) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels;
            repaint();
            return;
        }

        if (dragState.active)
            dragState.moved = true;
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        endAudition();   // G3.2: the audition ends with the press

        if (controlDragState.active)
        {
            const ControlDragState drag = controlDragState;
            controlDragState = {};
            finishControlLaneGesture (drag);
            return;
        }

        if (velocityDragState.active)
        {
            const VelocityDragState drag = velocityDragState;
            velocityDragState = {};
            if (! drag.moved || ! stateProvider || ! onVelocityLanePainted)
                return;

            // E13: crossed notes are the ones whose COLUMN (note span) overlaps the swept tick
            // range; each takes the drag line's velocity at its own start tick (clamped to the
            // segment ends). If any crossed note is selected, the whole selection paints together.
            const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
            const yesdaw::engine::Tick tickLo = juce::jmin (drag.downTick, drag.currentTick);
            const yesdaw::engine::Tick tickHi = juce::jmax (drag.downTick, drag.currentTick);
            const auto velocityAt = [&drag] (yesdaw::engine::Tick tick) {
                if (drag.currentTick == drag.downTick)
                    return drag.currentVelocity;
                const double t = juce::jlimit (
                    0.0, 1.0,
                    static_cast<double> (tick - drag.downTick)
                        / static_cast<double> (drag.currentTick - drag.downTick));
                return drag.downVelocity + t * (drag.currentVelocity - drag.downVelocity);
            };

            bool crossedSelected = false;
            std::vector<const yesdaw::ui::UiPianoRollNoteView*> crossed;
            for (const yesdaw::ui::UiPianoRollNoteView& note : surface.notes)
            {
                if (note.startTick <= tickHi && note.startTick + note.lengthTicks > tickLo)
                {
                    crossed.push_back (&note);
                    crossedSelected = crossedSelected || note.selected;
                }
            }

            std::vector<std::pair<yesdaw::engine::EntityId, double>> edits;
            if (crossedSelected)
            {
                for (const yesdaw::ui::UiPianoRollNoteView& note : surface.notes)
                    if (note.selected)
                        edits.emplace_back (note.noteId, velocityAt (note.startTick));
            }
            else
            {
                for (const yesdaw::ui::UiPianoRollNoteView* note : crossed)
                    edits.emplace_back (note->noteId, velocityAt (note->startTick));
            }

            if (! edits.empty())
                onVelocityLanePainted (
                    drag.midiClipId,
                    std::span<const std::pair<yesdaw::engine::EntityId, double>> (edits.data(),
                                                                                  edits.size()));
            return;
        }

        if (marqueeState.active)
        {
            const NoteMarqueeState marquee = marqueeState;
            marqueeState = {};
            repaint();

            if (marquee.moved && stateProvider && onNotesMarqueeSelected)
            {
                const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
                const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
                const juce::Rectangle<int> rect (marquee.downPosition, marquee.currentPosition);
                std::vector<yesdaw::engine::EntityId> noteIds;
                for (const yesdaw::ui::UiPianoRollNoteView& note : surface.notes)
                {
                    if (note.key < surface.viewLowKey || note.key > pianoRollViewHighKey (surface))
                        continue;
                    if (pianoRollNoteBounds (geometry, surface, note).intersects (rect))
                        noteIds.push_back (note.noteId);
                }
                onNotesMarqueeSelected (surface.midiClipId,
                                        std::span<const yesdaw::engine::EntityId> (noteIds.data(),
                                                                                   noteIds.size()));
            }
            return;
        }

        if (pendingShiftToggleNoteId.isValid())
        {
            const yesdaw::engine::EntityId toggledNoteId = pendingShiftToggleNoteId;
            pendingShiftToggleNoteId = {};
            if ((! dragState.active || ! dragState.moved) && stateProvider && onNoteToggled)
            {
                dragState = {};
                onNoteToggled (stateProvider().midiClipId, toggledNoteId);
                return;
            }
        }

        if (! dragState.active)
            return;

        const PianoDragState drag = dragState;
        dragState = {};

        if (! drag.moved || ! stateProvider)
            return;

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
        const int deltaX = event.getPosition().x - drag.downPosition.x;
        const int deltaY = event.getPosition().y - drag.downPosition.y;
        // G3.2: the Velocity tool — up raises, down lowers; 100 px is the full range; one edit on release.
        if (drag.mode == PianoDragMode::VelocityDrag)
        {
            const double velocity = juce::jlimit (0.0, 1.0,
                drag.downVelocity - static_cast<double> (deltaY) / static_cast<double> (yesdaw::ui::UiTheme::Layout::pianoRollVelocityDragPixelsPerUnit));
            if (onNoteVelocityAdjusted)
                onNoteVelocityAdjusted (drag.midiClipId, drag.noteId, velocity);
            return;
        }
        // E12: vertical drag transposes — a row of movement is a semitone.
        const float rowHeight = geometry.rowHeight > 1.0f ? geometry.rowHeight : 1.0f;
        const int keyDelta = drag.mode == PianoDragMode::Move && ! drag.copy
            ? -static_cast<int> (std::llround (static_cast<double> (deltaY) / static_cast<double> (rowHeight)))
            : 0;
        if (std::abs (deltaX) < yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels && keyDelta == 0)
            return;

        const yesdaw::engine::Tick deltaTicks = pianoRollTickDeltaForPixels (geometry, surface, deltaX);

        if (drag.mode == PianoDragMode::SetLength)
        {
            const yesdaw::engine::Tick maxLength =
                juce::jmax<yesdaw::engine::Tick> (0, surface.timelineLength - drag.startTick);
            const yesdaw::engine::Tick snappedEnd = snappedRollTick (
                surface, drag.startTick + drag.lengthTicks + deltaTicks);
            const yesdaw::engine::Tick nextLength =
                std::clamp<yesdaw::engine::Tick> (snappedEnd - drag.startTick, 0, maxLength);
            if (nextLength != drag.lengthTicks && onNoteLengthChanged)
                onNoteLengthChanged (drag.midiClipId, drag.noteId, nextLength);
            return;
        }

        if (drag.mode == PianoDragMode::TrimHead)
        {
            if (onNoteHeadTrimmed)
                onNoteHeadTrimmed (drag.midiClipId, drag.noteId,
                                   snappedRollTick (surface, drag.startTick + deltaTicks));
            return;
        }

        const yesdaw::engine::Tick maxStart =
            juce::jmax<yesdaw::engine::Tick> (0, surface.timelineLength - drag.lengthTicks);
        // A pure pitch drag (below the horizontal dead zone) must not snap the start sideways.
        const yesdaw::engine::Tick nextStart =
            std::abs (deltaX) >= yesdaw::ui::UiTheme::Layout::inputDragDeadZonePixels
                ? std::clamp<yesdaw::engine::Tick> (snappedRollTick (surface, drag.startTick + deltaTicks),
                                                    0, maxStart)
                : drag.startTick;
        if (nextStart == drag.startTick && keyDelta == 0)
            return;

        if (drag.copy)
        {
            if (onNoteCopyDragged)
                onNoteCopyDragged (drag.midiClipId, drag.noteId, nextStart);
            return;
        }

        if (onNotesDragged)
            onNotesDragged (drag.midiClipId, drag.noteId, nextStart - drag.startTick, keyDelta);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! stateProvider)
            return;

        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const auto hit = noteAt (surface, event.getPosition());
        if (! hit)
        {
            // G3.2: a double-click on the empty grid adds a note there (Logic), one snap step long,
            // with any tool — the Pencil's single click is the fast path, this is the discoverable one.
            const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
            if (! surface.midiClipSelected || ! geometry.grid.contains (event.getPosition()) || ! onNoteAdded)
                return;
            yesdaw::engine::Tick tick = pianoRollTickForX (geometry, surface, event.getPosition().x);
            if (surface.snapEnabled && surface.snapGridTicks > 0)
                tick -= tick % surface.snapGridTicks;
            const float rowHeight = geometry.rowHeight > 1.0f ? geometry.rowHeight : 1.0f;
            const int key = pianoRollViewHighKey (surface)
                - static_cast<int> ((event.getPosition().y - geometry.grid.getY()) / rowHeight);
            if (key >= yesdaw::ui::UiThemeLayout::pianoRollKeyMin && key <= yesdaw::ui::UiThemeLayout::pianoRollKeyMax)
                onNoteAdded (surface.midiClipId, std::max<yesdaw::engine::Tick> (0, tick), static_cast<std::int16_t> (key));
            return;
        }

        if (onNoteClicked)
            onNoteClicked (surface.midiClipId, hit->noteId);

        if (event.mods.isShiftDown())
        {
            if (onExpressionRead)
                onExpressionRead();
            return;
        }

        if (event.mods.isCtrlDown())
        {
            if (onNoteQuantized)
                onNoteQuantized (surface.midiClipId, hit->noteId, kPianoRollSnapGridTicks);
            return;
        }

        if (event.mods.isAltDown())
        {
            if (onNoteTransposed)
                onNoteTransposed (surface.midiClipId, hit->noteId, 1);
            return;
        }

        // Plain double-click deletes the note (E11): the mouse finally has a delete gesture.
        if (onNoteDeleted)
        {
            onNoteDeleted (surface.midiClipId, hit->noteId);
        }
    }

private:
    enum class PianoDragMode
    {
        VelocityDrag,   // G3.2: the Velocity tool
        Move,
        SetLength,
        TrimHead    // E12: drag the left edge — the note end stays fixed
    };

    // E12: note gestures snap through the REAL chooser (no Ctrl inversion in the roll — Ctrl on
    // notes means copy-drag; raw edits come from switching the chooser off).
    [[nodiscard]] static yesdaw::engine::Tick snappedRollTick (
        const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface, yesdaw::engine::Tick tick) noexcept
    {
        if (! surface.snapEnabled || surface.snapGridTicks <= 0)
            return juce::jmax<yesdaw::engine::Tick> (0, tick);

        yesdaw::engine::Tick snapped = tick;
        if (! yesdaw::engine::snapTick (tick, yesdaw::engine::SnapGrid { surface.snapGridTicks }, snapped))
            return juce::jmax<yesdaw::engine::Tick> (0, tick);

        return juce::jmax<yesdaw::engine::Tick> (0, snapped);
    }

    struct PianoDragState
    {
        bool active = false;
        bool moved = false;
        yesdaw::engine::EntityId midiClipId {};
        yesdaw::engine::EntityId noteId {};
        yesdaw::engine::Tick startTick = 0;
        yesdaw::engine::Tick lengthTicks = 0;
        PianoDragMode mode = PianoDragMode::Move;
        bool copy = false;   // Ctrl+drag copy-drag (B35)
        double downVelocity = 1.0;   // G3.2: the Velocity tool's anchor
        juce::Point<int> downPosition;
    };

    [[nodiscard]] std::optional<yesdaw::ui::UiPianoRollNoteView> noteAt (
        const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
        juce::Point<int> position) const noexcept
    {
        if (! surface.midiClipSelected)
            return std::nullopt;

        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
        for (auto it = surface.notes.rbegin(); it != surface.notes.rend(); ++it)
        {
            if (it->key < surface.viewLowKey || it->key > pianoRollViewHighKey (surface))
                continue;

            if (pianoRollNoteBounds (geometry, surface, *it).contains (position))
                return *it;
        }

        return std::nullopt;
    }

    [[nodiscard]] PianoDragMode dragModeForPointer (
        const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
        const yesdaw::ui::UiPianoRollNoteView& note,
        juce::Point<int> position,
        juce::ModifierKeys modifiers) const noexcept
    {
        if (modifiers.isShiftDown())
            return PianoDragMode::SetLength;

        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());
        const juce::Rectangle<int> bounds = pianoRollNoteBounds (geometry, surface, note);
        // E12: the edge zones only bite on a note wide enough to keep a grabbable middle —
        // otherwise they would swallow a narrow note whole and it could never be moved.
        // Shift+drag stays the length edit for notes of any width.
        if (bounds.getWidth() >= yesdaw::ui::UiTheme::Layout::pianoRollNoteEdgeMinGrabWidth)
        {
            if (std::abs (position.x - bounds.getRight())
                <= yesdaw::ui::UiTheme::Layout::pianoRollNoteEdgeHitWidth)
                return PianoDragMode::SetLength;

            // The LEFT edge trims the note head (end fixed).
            if (std::abs (position.x - bounds.getX())
                <= yesdaw::ui::UiTheme::Layout::pianoRollNoteEdgeHitWidth)
                return PianoDragMode::TrimHead;
        }

        return PianoDragMode::Move;
    }

    // G3.3: an in-flight control-lane gesture — a point placed / dragged, or a pencil paint
    // (freehand samples, or the straight line from the press to the release).
    enum class ControlDragKind { Point, Pencil, Line };
    struct ControlDragState
    {
        bool active = false;
        bool moved = false;
        ControlDragKind kind = ControlDragKind::Point;
        yesdaw::engine::EntityId midiClipId {};
        yesdaw::engine::EntityId pointId {};   // the grabbed point; invalid = placing a new one
        juce::Point<int> downPosition;
        yesdaw::engine::Tick downTick = 0;
        double downValue = 0.0;
        yesdaw::engine::Tick currentTick = 0;
        double currentValue = 0.0;
        std::vector<std::pair<yesdaw::engine::Tick, double>> samples;
    };
    ControlDragState controlDragState;
    juce::String laneGesture;

    // The lane's tick under an x: snapped to the chooser's grid (the roll's note law: Ctrl is not an
    // inversion here) unless the caller asks for the raw tick (the freehand pencil samples raw).
    [[nodiscard]] static yesdaw::engine::Tick controlLaneTickAt (const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
                                                                  const PianoRollCanvasGeometry& geometry,
                                                                  int x,
                                                                  bool snap) noexcept
    {
        yesdaw::engine::Tick tick = pianoRollTickForX (geometry, surface, x);
        if (snap && surface.snapEnabled && surface.snapGridTicks > 0)
            tick -= tick % surface.snapGridTicks;
        return std::clamp<yesdaw::engine::Tick> (tick, 0, juce::jmax<yesdaw::engine::Tick> (0, surface.timelineLength));
    }

    [[nodiscard]] static double controlLaneValueAt (const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
                                                    const PianoRollCanvasGeometry& geometry,
                                                    int y) noexcept
    {
        const auto* lane = pianoRollControlLaneOf (surface);
        const double valueMin = lane != nullptr ? lane->valueMin : 0.0;
        const double valueMax = lane != nullptr ? lane->valueMax : 1.0;
        return pianoRollControlValueForLaneY (pianoRollControlLaneDataArea (geometry), y, valueMin, valueMax);
    }

    // The painted point under a position (the last-painted wins, like notes).
    [[nodiscard]] static std::optional<yesdaw::ui::UiPianoRollExpressionPoint> controlPointAt (
        const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
        const PianoRollCanvasGeometry& geometry,
        juce::Point<int> position) noexcept
    {
        const auto* lane = pianoRollControlLaneOf (surface);
        if (lane == nullptr)
            return std::nullopt;
        const juce::Rectangle<int> area = pianoRollControlLaneDataArea (geometry);
        const int radius = yesdaw::ui::UiTheme::Layout::pianoRollControlPointHitRadius;
        for (auto it = lane->points.rbegin(); it != lane->points.rend(); ++it)
        {
            const int x = pianoRollTickX (geometry, surface, it->tick);
            const int y = pianoRollControlLaneYForValue (area, it->value, lane->valueMin, lane->valueMax);
            if (std::abs (x - position.x) <= radius && std::abs (y - position.y) <= radius)
                return *it;
        }
        return std::nullopt;
    }

    void beginControlLaneGesture (const yesdaw::ui::UiPianoRollSurfaceSnapshot& surface,
                                  const PianoRollCanvasGeometry& geometry,
                                  const juce::MouseEvent& event)
    {
        const yesdaw::ui::TimelineTool tool =
            activeToolProvider ? activeToolProvider() : yesdaw::ui::TimelineTool::Pointer;
        const auto point = controlPointAt (surface, geometry, event.getPosition());

        if (tool == yesdaw::ui::TimelineTool::Eraser)
        {
            if (point && onControlPointDeleted)
                onControlPointDeleted (surface.midiClipId, point->entityId);
            return;
        }

        controlDragState = {};
        controlDragState.active = true;
        controlDragState.midiClipId = surface.midiClipId;
        controlDragState.downPosition = event.getPosition();
        laneGesture = tool == yesdaw::ui::TimelineTool::Pencil ? "down:pencil" : "down:point";
        if (tool == yesdaw::ui::TimelineTool::Pencil)
        {
            controlDragState.kind = event.mods.isShiftDown() ? ControlDragKind::Line : ControlDragKind::Pencil;
            controlDragState.downTick = controlLaneTickAt (surface, geometry, event.getPosition().x, controlDragState.kind == ControlDragKind::Line);
            controlDragState.downValue = controlLaneValueAt (surface, geometry, event.getPosition().y);
            controlDragState.samples.emplace_back (controlDragState.downTick, controlDragState.downValue);
        }
        else
        {
            controlDragState.kind = ControlDragKind::Point;
            if (point)
            {
                controlDragState.pointId = point->entityId;
                controlDragState.downTick = point->tick;
                controlDragState.downValue = point->value;
            }
            else
            {
                controlDragState.downTick = controlLaneTickAt (surface, geometry, event.getPosition().x, true);
                controlDragState.downValue = controlLaneValueAt (surface, geometry, event.getPosition().y);
            }
        }
        controlDragState.currentTick = controlDragState.downTick;
        controlDragState.currentValue = controlDragState.downValue;
    }

    void finishControlLaneGesture (const ControlDragState& drag)
    {
        if (! stateProvider)
            return;
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = stateProvider();
        const PianoRollCanvasGeometry geometry = pianoRollCanvasGeometry (getLocalBounds());

        if (drag.kind == ControlDragKind::Point)
        {
            if (drag.pointId.isValid())
            {
                laneGesture = drag.moved ? "up:move" : "up:point click";
                if (drag.moved && onControlPointMoved)
                    onControlPointMoved (drag.midiClipId, drag.pointId, drag.currentTick, drag.currentValue);
            }
            else if (onControlPointAdded)
            {
                // A click places a point; a press-and-drag places it where the drag ends.
                laneGesture = "up:add";
                onControlPointAdded (drag.midiClipId, drag.currentTick, drag.currentValue);
            }
            return;
        }

        if (! drag.moved)
        {
            // A pencil click is a single point (Logic's pencil).
            laneGesture = "up:pencil click";
            if (onControlPointAdded)
                onControlPointAdded (drag.midiClipId, controlLaneTickAt (surface, geometry, drag.downPosition.x, true), drag.downValue);
            return;
        }

        // The pencil paints one point per grid step across the sweep (chooser Off: one per pixel,
        // never more): freehand takes the sampled path's value at each step, a line interpolates
        // the press and the release.
        const yesdaw::engine::Tick lo = juce::jmin (drag.downTick, drag.currentTick);
        const yesdaw::engine::Tick hi = juce::jmax (drag.downTick, drag.currentTick);
        const yesdaw::engine::Tick pixelTicks = juce::jmax<yesdaw::engine::Tick> (
            1, pianoRollTickDeltaForPixels (geometry, surface, 1));
        const yesdaw::engine::Tick step = surface.snapEnabled && surface.snapGridTicks > 0
            ? surface.snapGridTicks
            : pixelTicks;

        std::vector<std::pair<yesdaw::engine::Tick, double>> samples = drag.samples;
        std::stable_sort (samples.begin(), samples.end(),
                          [] (const auto& a, const auto& b) { return a.first < b.first; });
        const auto valueAt = [&] (yesdaw::engine::Tick tick)
        {
            if (drag.kind == ControlDragKind::Line || samples.size() < 2u)
            {
                if (drag.currentTick == drag.downTick)
                    return drag.currentValue;
                const double t = juce::jlimit (0.0, 1.0,
                    static_cast<double> (tick - drag.downTick) / static_cast<double> (drag.currentTick - drag.downTick));
                return drag.downValue + t * (drag.currentValue - drag.downValue);
            }
            if (tick <= samples.front().first)
                return samples.front().second;
            for (std::size_t i = 1; i < samples.size(); ++i)
            {
                if (tick <= samples[i].first)
                {
                    const auto& a = samples[i - 1];
                    const auto& b = samples[i];
                    if (b.first == a.first)
                        return b.second;
                    const double t = static_cast<double> (tick - a.first) / static_cast<double> (b.first - a.first);
                    return a.second + t * (b.second - a.second);
                }
            }
            return samples.back().second;
        };

        std::vector<std::pair<yesdaw::engine::Tick, double>> points;
        const yesdaw::engine::Tick first = ((lo + step - 1) / step) * step;
        for (yesdaw::engine::Tick tick = first; tick <= hi; tick += step)
            points.emplace_back (tick, valueAt (tick));
        if (points.empty())
            points.emplace_back (lo, valueAt (lo));
        laneGesture = "up:paint " + juce::String (static_cast<int> (points.size())) + " step " + juce::String (static_cast<juce::int64> (step))
                    + " " + juce::String (static_cast<juce::int64> (lo)) + ".." + juce::String (static_cast<juce::int64> (hi));
        if (onControlLanePainted)
            onControlLanePainted (drag.midiClipId,
                                  std::span<const std::pair<yesdaw::engine::Tick, double>> (points.data(), points.size()),
                                  lo, hi);
    }

    // E13: an in-flight velocity-lane paint drag.
    struct VelocityDragState
    {
        bool active = false;
        bool moved = false;
        yesdaw::engine::EntityId midiClipId {};
        juce::Point<int> downPosition;
        yesdaw::engine::Tick downTick = 0;
        yesdaw::engine::Tick currentTick = 0;
        double downVelocity = 0.0;
        double currentVelocity = 0.0;
    };
    VelocityDragState velocityDragState;

    PianoDragState dragState;
    int auditionKey = -1;   // G3.2: the key the mouse holds (keyboard column / note press); -1 = none
    int lastAuditioned = -1;   // G3.2: the last key auditioned (the state probe reads it)

    // G3.2 / ADR-0047: audition - one held key at a time, ended by the mouse-up that ends the press.
    void beginAudition (int key)
    {
        if (key < 0)
            return;
        if (auditionKey >= 0)
            endAudition();
        auditionKey = key;
        lastAuditioned = key;
        if (onKeyAuditioned)
            onKeyAuditioned (static_cast<std::int16_t> (key), true);
    }

    void endAudition()
    {
        if (auditionKey < 0)
            return;
        const int key = auditionKey;
        auditionKey = -1;
        if (onKeyAuditioned)
            onKeyAuditioned (static_cast<std::int16_t> (key), false);
    }
    // Piano-roll selection tools (E11): Pointer note marquee + the pending Shift+click toggle
    // that resolves on a movement-free mouse-up (a Shift+DRAG keeps the length-edit law).
    struct NoteMarqueeState
    {
        bool active = false;
        bool moved = false;
        juce::Point<int> downPosition;
        juce::Point<int> currentPosition;
    };
    NoteMarqueeState marqueeState;
    yesdaw::engine::EntityId pendingShiftToggleNoteId;
};

} // namespace yesdaw::ui
