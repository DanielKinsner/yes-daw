// YES DAW — the track header list's input overlay.
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

class TrackListInputComponent final : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    std::function<int()> rowCountProvider;
    // N6: persisted per-track heights, one entry per row (0 = auto-shared). Empty or unset means
    // every row auto-shares, exactly like every row did before this field existed.
    std::function<std::vector<int>()> rowHeightsProvider;
    std::function<double()> rowZoomProvider;   // G2.16: the row zoom the canvas geometry also multiplies by
    std::function<void (int)> onRowClicked;
    std::function<void (int, juce::ModifierKeys)> onRowClickedWithModifiers;   // G2.17: Ctrl toggles, Shift extends
    std::function<void (int, int)> onRowReordered;                             // G2.17: from row, to row
    std::function<void (int)> onRowDoubleClicked;
    // Mini controls (usable-DAW P2): the painted PAN knob, VOL slider, and M/S cells become live.
    std::function<void (int, float)> onPanEdited;      // row, pan in [-1, 1]
    std::function<void (int, float)> onVolumeEdited;   // row, linear gain in [0, 1]
    std::function<void (int)> onMuteToggled;
    std::function<void (int)> onSoloToggled;
    std::function<void (int)> onArmToggled;   // the "O" cell: the real record-arm badge, clickable
    // Current strip values, used to anchor Shift fine drags without a jump (B30).
    std::function<float (int)> panValueProvider;
    std::function<float (int)> volumeValueProvider;
    // Fired on mouse-up after any mini-control gesture, so transient drag readouts can hide (B31).
    std::function<void()> onMiniDragEnded;
    // Click on the row's meter clears its held peak and latched clip light (B32).
    std::function<void (int)> onMeterClicked;
    // N6: a row-boundary drag — row index, the LIVE candidate height in pixels (already clamped
    // to the legible band). Fired on every drag tick; onRowResizeEnded closes the gesture (E21
    // coalescing, mirroring the mixer fader drag).
    std::function<void (int, int)> onRowResized;
    std::function<void()> onRowResizeEnded;
    // N7: a click on the row's colour swatch (the left accent bar) — row index only, the caller
    // decides the next colour (a fixed-palette cycle, mirroring how a single click commits one
    // undo step for M/S/O).
    std::function<void (int)> onColourSwatchClicked;

    // G1.6: the gesture hint for the hovered zone — the status line shows it while no status
    // message is active. hintAt() is the one law mouseMove and the harness share.
    std::function<void (const juce::String&)> onHoverHint;

    [[nodiscard]] juce::String hintAt (juce::Point<int> position, juce::ModifierKeys) const
    {
        if (rowResizeHandleAt (position) >= 0)
            return "Row edge: drag to resize the track";
        const int row = rowAt (position);
        if (row < 0)
            return {};
        switch (zoneAt (row, position))
        {
            case MiniZone::Pan:    return "Pan: drag \u00b7 Alt-click or double-click recentres \u00b7 Shift for fine";
            case MiniZone::Volume: return "Volume: drag \u00b7 Alt-click resets to unity \u00b7 Shift for fine";
            case MiniZone::Mute:   return "Mute: click toggles";
            case MiniZone::Solo:   return "Solo: click toggles";
            case MiniZone::Arm:    return "Arm: click arms this track for recording (needs an input device)";
            case MiniZone::Meter:  return "Meter: click clears the clip light";
            case MiniZone::Colour: return "Colour: click cycles the track colour";
            case MiniZone::None:   break;
        }
        return "Track: click to select \u00b7 double-click to rename \u00b7 right-click for the track menu";
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (onHoverHint)
            onHoverHint ({});
    }

    void mouseMove (const juce::MouseEvent& event) override
    {
        if (onHoverHint)
            onHoverHint (hintAt (event.getPosition(), event.mods));
        setMouseCursor (rowResizeHandleAt (event.getPosition()) >= 0
                             ? juce::MouseCursor::UpDownResizeCursor
                             : juce::MouseCursor::NormalCursor);
    }

    // G1.3: a right-click on a row selects its track and asks for the Track header menu.
    std::function<void (yesdaw::ui::ContextMenuTarget, int, juce::Point<int>)> onContextMenuRequested;

    void requestContextMenu (juce::Point<int> position)
    {
        if (! onContextMenuRequested)
            return;
        const int row = rowAt (position);
        if (row < 0)
            return;
        // A right-click selects the row it lands on before its menu opens (Logic), so the header
        // verbs act on THAT track. G3.1 checkpoint: G2.17 had moved the click to the modifier-aware
        // callback and left this path on the unset plain one — SS-2's Duplicate Track dispatched
        // on no selected lane (found by the session drive).
        if (onRowClickedWithModifiers)
            onRowClickedWithModifiers (row, juce::ModifierKeys());
        else if (onRowClicked)
            onRowClicked (row);
        onContextMenuRequested (yesdaw::ui::ContextMenuTarget::TrackHeader, row, position);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (event.mods.isRightButtonDown())   // the right button itself: on macOS isPopupMenu() also fires for Ctrl+click, which is a gesture modifier here
        {
            requestContextMenu (event.getPosition());
            return;
        }

        dragRow = -1;
        dragZone = MiniZone::None;
        resizeDragRow = -1;

        // N6: a row-boundary grab wins over whatever content sits under it — a real, discoverable
        // resize gesture, not a hidden hotspot.
        if (const int handleRow = rowResizeHandleAt (event.getPosition()); handleRow >= 0)
        {
            const int rows = rowCountProvider ? rowCountProvider() : 0;
            auto area = getLocalBounds();
            area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
            resizeDragRow = handleRow;
            resizeDragStartY = event.getPosition().y;
            resizeDragStartHeightPx = static_cast<int> (
                std::llround (rowGeometry (rows, area.getHeight()).heightFor (handleRow)));
            return;
        }

        const int row = rowAt (event.getPosition());
        if (row < 0)
            return;

        switch (zoneAt (row, event.getPosition()))
        {
            case MiniZone::Pan:
                if (event.mods.isAltDown())
                {
                    if (onPanEdited)
                        onPanEdited (row, 0.0f);   // Alt+click recentres, matching double-click
                    return;
                }
                dragRow = row;
                dragZone = MiniZone::Pan;
                if (beginFineDragIfWanted (event))
                    return;   // fine mode anchors at the current value; no jump
                applyPan (row, event.getPosition());
                return;

            case MiniZone::Volume:
                if (event.mods.isAltDown())
                {
                    if (onVolumeEdited)
                        onVolumeEdited (row, 1.0f);   // Alt+click resets the mini VOL to unity
                    return;
                }
                dragRow = row;
                dragZone = MiniZone::Volume;
                if (beginFineDragIfWanted (event))
                    return;
                applyVolume (row, event.getPosition());
                return;

            case MiniZone::Mute:
                if (onMuteToggled)
                    onMuteToggled (row);
                return;

            case MiniZone::Solo:
                if (onSoloToggled)
                    onSoloToggled (row);
                return;

            case MiniZone::Arm:
                if (onArmToggled)
                    onArmToggled (row);
                return;

            case MiniZone::Meter:
                if (onMeterClicked)
                    onMeterClicked (row);
                return;

            case MiniZone::Colour:
                if (onColourSwatchClicked)
                    onColourSwatchClicked (row);
                return;

            case MiniZone::None:
                break;
        }

        if (onRowClickedWithModifiers)
            onRowClickedWithModifiers (row, event.mods);   // G2.17
        else if (onRowClicked)
            onRowClicked (row);
        // G2.17: a vertical drag from the name band reorders (the drop row follows the pointer).
        reorderDragRow = row;
        reorderDropRow = -1;
        reorderDragStartY = event.getPosition().y;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (resizeDragRow >= 0)
        {
            const int candidate = std::clamp (
                resizeDragStartHeightPx + (event.getPosition().y - resizeDragStartY),
                yesdaw::engine::kTrackHeightMinPx, yesdaw::engine::kTrackHeightMaxPx);
            if (onRowResized)
                onRowResized (resizeDragRow, candidate);
            return;
        }

        if (reorderDragRow >= 0 && dragRow < 0)   // G2.17: the reorder drag
        {
            if (std::abs (event.getPosition().y - reorderDragStartY) >= yesdaw::ui::UiTheme::Layout::trackListReorderDeadZonePx)
            {
                const int rows = rowCountProvider ? rowCountProvider() : 0;
                int drop = rowAt (event.getPosition());
                if (drop < 0 && rows > 0)
                    drop = event.getPosition().y < getHeight() / 2 ? 0 : rows - 1;
                reorderDropRow = drop;
                repaint();
            }
            return;
        }
        if (dragRow < 0)
            return;

        if (! fineDragActive && event.mods.isShiftDown() && ! event.mods.isAltDown())
            (void) beginFineDragIfWanted (event);   // Shift pressed mid-drag: anchor here

        if (fineDragActive)
        {
            applyFineDrag (event);
            return;
        }

        if (dragZone == MiniZone::Pan)
            applyPan (dragRow, event.getPosition());
        else if (dragZone == MiniZone::Volume)
            applyVolume (dragRow, event.getPosition());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (resizeDragRow >= 0)
        {
            resizeDragRow = -1;
            if (onRowResizeEnded)
                onRowResizeEnded();
            return;
        }

        if (reorderDragRow >= 0)   // G2.17
        {
            const int from = reorderDragRow;
            const int to = reorderDropRow;
            reorderDragRow = -1;
            reorderDropRow = -1;
            if (to >= 0 && to != from && onRowReordered)
                onRowReordered (from, to);
            repaint();
        }
        dragRow = -1;
        dragZone = MiniZone::None;
        fineDragActive = false;
        if (onMiniDragEnded)
            onMiniDragEnded();
    }

    // G2.17: the reorder drop line (the input is transparent otherwise; the shell paints the rail).
    void paint (juce::Graphics& g) override
    {
        if (reorderDragRow < 0 || reorderDropRow < 0)
            return;
        const juce::Rectangle<int> target = rowBounds (reorderDropRow);
        if (target.isEmpty())
            return;
        const int y = reorderDropRow > reorderDragRow ? target.getBottom() - 1 : target.getY();
        g.setColour (yesdaw::ui::UiTheme::Color::accentBlue());
        g.fillRect (target.getX(), y - 1, target.getWidth(), 3);
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        const int row = rowAt (event.getPosition());
        if (row < 0)
            return;

        // Double-click on the pan knob recentres; elsewhere the row rename applies.
        if (zoneAt (row, event.getPosition()) == MiniZone::Pan)
        {
            if (onPanEdited)
                onPanEdited (row, 0.0f);
            return;
        }

        if (onRowDoubleClicked)
            onRowDoubleClicked (row);
    }

    // N6: the shared cumulative row law — every row without a persisted height auto-shares
    // whatever space is left, exactly like the historical uniform law when nothing is customized.
    [[nodiscard]] yesdaw::ui::CumulativeRowGeometry rowGeometry (int rows, int availablePixels) const
    {
        const std::vector<int> heights = rowHeightsProvider ? rowHeightsProvider() : std::vector<int> {};
        const double rowZoom = std::clamp (rowZoomProvider ? rowZoomProvider() : 1.0,
                                           yesdaw::ui::UiTheme::Layout::timelineRowZoomMin,
                                           yesdaw::ui::UiTheme::Layout::timelineRowZoomMax);   // G2.16
        return yesdaw::ui::computeCumulativeRowGeometry (
            rows, availablePixels, juce::roundToInt (yesdaw::ui::UiTheme::Layout::trackListRowMinHeight * rowZoom),
            static_cast<int> (heights.size()) >= rows ? heights.data() : nullptr);
    }

    [[nodiscard]] juce::Rectangle<int> rowBounds (int row) const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0 || row < 0 || row >= rows)
            return {};

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        const yesdaw::ui::CumulativeRowGeometry geometry = rowGeometry (rows, area.getHeight());
        const int scrollRows = effectiveScrollRows();
        const int rowHeight = static_cast<int> (std::llround (geometry.heightFor (row)));
        const int rowTop = area.getY()
                         + static_cast<int> (std::llround (geometry.top (row) - geometry.top (scrollRows)));
        return { area.getX(), rowTop, area.getWidth(), rowHeight };
    }

    // Shared row-geometry law: these mirror drawTrackList's control rectangles exactly, so
    // hit-testing and paint cannot drift. The paint code trims the separator first.
    [[nodiscard]] juce::Rectangle<int> panKnobBounds (int row) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        return bounds.withRight (bounds.getRight() - yesdaw::ui::UiTheme::Layout::trackListPanRightInset)
                     .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListPanDiameter)
                     .withY (bounds.getY() + yesdaw::ui::UiTheme::Layout::trackListPanTopInset)
                     .withHeight (yesdaw::ui::UiTheme::Layout::trackListPanDiameter);
    }

    // V5: a VERTICAL rect — y controls gain (top = loud), the same orientation law as the mixer
    // strip fader, so the two controls feel like one instrument.
    [[nodiscard]] juce::Rectangle<int> volumeSliderBounds (int row) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        return bounds.withRight (bounds.getRight()
                                 - yesdaw::ui::UiTheme::Layout::trackListLevelColumnRightInset)
                     .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListLevelColumnWidth)
                     .reduced (yesdaw::ui::UiTheme::Space::none,
                               yesdaw::ui::UiTheme::Layout::trackListLevelColumnVerticalInset);
    }

    [[nodiscard]] juce::Rectangle<int> muteCellBounds (int row) const { return buttonCellBounds (row, 0); }
    [[nodiscard]] juce::Rectangle<int> soloCellBounds (int row) const { return buttonCellBounds (row, 1); }
    // The third painted cell is the "O" record-arm badge (E30). It was painted, lit red on an armed
    // track, and dead to the mouse until 2026-09-04: no zone claimed cell 2, so a click selected
    // the track instead of arming it.
    [[nodiscard]] juce::Rectangle<int> armCellBounds (int row) const { return buttonCellBounds (row, 2); }

    [[nodiscard]] juce::Rectangle<int> meterZoneBounds (int row) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        return bounds.withRight (bounds.getRight() - yesdaw::ui::UiTheme::Layout::trackListMeterRightInset)
                     .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListMeterWidth)
                     .reduced (yesdaw::ui::UiTheme::Layout::trackListMeterHorizontalInset,
                               yesdaw::ui::UiTheme::Layout::trackListMeterVerticalInset);
    }

    // N7: the row's colour swatch IS the left accent bar — mirrors drawTrackList's accent-bar
    // rectangle exactly, so paint and hit-test cannot drift (the same law N6 established for
    // every other rail control).
    [[nodiscard]] juce::Rectangle<int> colourSwatchBounds (int row) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        return bounds.withWidth (yesdaw::ui::UiTheme::Layout::trackListAccentWidth)
                     .reduced (yesdaw::ui::UiTheme::Layout::trackListAccentHorizontalInset,
                               yesdaw::ui::UiTheme::Layout::trackListAccentVerticalInset);
    }

private:
    enum class MiniZone : std::uint8_t { None, Pan, Volume, Mute, Solo, Arm, Meter, Colour };

    [[nodiscard]] juce::Rectangle<int> buttonCellBounds (int row, int cellIndex) const
    {
        auto bounds = rowBounds (row);
        bounds.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight);
        auto buttonsArea = bounds.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListNameLeftInset)
                               .withTrimmedTop (yesdaw::ui::UiTheme::Layout::trackListButtonsTop)
                               .withHeight (yesdaw::ui::UiTheme::Layout::trackListButtonsHeight);
        for (int cell = 0; cell < cellIndex; ++cell)
            buttonsArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::trackListButtonWidth);
        return buttonsArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::trackListButtonWidth);
    }

    [[nodiscard]] MiniZone zoneAt (int row, juce::Point<int> position) const
    {
        if (panKnobBounds (row).contains (position))
            return MiniZone::Pan;
        // V5: the fader is vertical, so the hit slop widens it sideways (the vertical extent is
        // already the full column).
        if (volumeSliderBounds (row).expanded (yesdaw::ui::UiTheme::Layout::trackListLevelHitSlopX,
                                               yesdaw::ui::UiTheme::Space::none)
                .contains (position))
            return MiniZone::Volume;
        if (muteCellBounds (row).contains (position))
            return MiniZone::Mute;
        if (soloCellBounds (row).contains (position))
            return MiniZone::Solo;
        if (armCellBounds (row).contains (position))
            return MiniZone::Arm;
        if (meterZoneBounds (row).contains (position))
            return MiniZone::Meter;
        if (colourSwatchBounds (row).contains (position))
            return MiniZone::Colour;
        return MiniZone::None;
    }

    void applyPan (int row, juce::Point<int> position)
    {
        if (! onPanEdited)
            return;

        const auto knob = panKnobBounds (row);
        if (knob.getWidth() <= 0)
            return;

        const float normalized = static_cast<float> (position.x - knob.getX())
                               / static_cast<float> (knob.getWidth());
        onPanEdited (row, juce::jlimit (-1.0f, 1.0f, normalized + normalized - 1.0f));
    }

    void applyVolume (int row, juce::Point<int> position)
    {
        if (! onVolumeEdited)
            return;

        const auto slider = volumeSliderBounds (row);
        if (slider.getHeight() <= 0)
            return;

        // V5: vertical law — the top of the column is unity, the bottom is silence, exactly the
        // mixer fader's orientation.
        const float normalized = 1.0f
                               - static_cast<float> (position.y - slider.getY())
                                     / static_cast<float> (slider.getHeight());
        onVolumeEdited (row, juce::jlimit (0.0f, 1.0f, normalized));
    }

    // Shift fine drag (B30): anchor at the strip's current value and scale pointer movement by
    // the shared fine-drag token; the value never jumps to the pointer while fine mode is active.
    [[nodiscard]] bool beginFineDragIfWanted (const juce::MouseEvent& event)
    {
        if (! event.mods.isShiftDown() || event.mods.isAltDown())
            return false;

        const auto* provider = dragZone == MiniZone::Pan
                                   ? &panValueProvider
                                   : &volumeValueProvider;
        if (! (*provider))
            return false;

        fineDragActive = true;
        fineDragValue = (*provider) (dragRow);
        fineDragLastX = event.getPosition().x;
        fineDragLastY = event.getPosition().y;
        return true;
    }

    void applyFineDrag (const juce::MouseEvent& event)
    {
        const auto bounds = dragZone == MiniZone::Pan ? panKnobBounds (dragRow)
                                                      : volumeSliderBounds (dragRow);
        // V5: the fine axis follows each control's own coarse axis — pan stays horizontal, the
        // now-vertical VOL fader moves on y (upward = louder, matching applyVolume's law).
        double proportionDelta = 0.0;
        if (dragZone == MiniZone::Pan)
        {
            if (bounds.getWidth() <= 0)
                return;
            const int x = event.getPosition().x;
            proportionDelta = static_cast<double> (x - fineDragLastX)
                            / static_cast<double> (bounds.getWidth());
            fineDragLastX = x;
        }
        else
        {
            if (bounds.getHeight() <= 0)
                return;
            const int y = event.getPosition().y;
            proportionDelta = static_cast<double> (fineDragLastY - y)
                            / static_cast<double> (bounds.getHeight());
            fineDragLastY = y;
        }

        const bool isPan = dragZone == MiniZone::Pan;
        const double span = isPan ? 2.0 : 1.0;   // pan covers [-1, 1]; VOL covers [0, 1]
        fineDragValue = static_cast<float> (juce::jlimit (
            isPan ? -1.0 : 0.0,
            1.0,
            static_cast<double> (fineDragValue)
                + proportionDelta * span * yesdaw::ui::UiTheme::Layout::fineDragScale));

        if (isPan && onPanEdited)
            onPanEdited (dragRow, fineDragValue);
        else if (! isPan && onVolumeEdited)
            onVolumeEdited (dragRow, fineDragValue);
    }

    int dragRow = -1;
    MiniZone dragZone = MiniZone::None;
    int reorderDragRow = -1;      // G2.17
    int reorderDropRow = -1;
    int reorderDragStartY = 0;
    bool fineDragActive = false;
    float fineDragValue = 0.0f;
    int fineDragLastX = 0;
    int fineDragLastY = 0;   // V5: the vertical VOL fader's fine axis

    // N6: row-boundary height-resize gesture state — kept separate from dragRow/dragZone (the
    // mini-control drags above) since a boundary drag can start even when the pointer isn't over
    // any zone at all.
    int resizeDragRow = -1;
    int resizeDragStartY = 0;
    int resizeDragStartHeightPx = 0;

    // N6: which row's BOTTOM edge `position` is within the grab tolerance of, or -1. Checked
    // before ordinary row/zone hit-testing so the boundary always wins over whatever content sits
    // under it.
    [[nodiscard]] int rowResizeHandleAt (juce::Point<int> position) const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0)
            return -1;

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        if (position.x < area.getX() || position.x >= area.getRight())
            return -1;

        const yesdaw::ui::CumulativeRowGeometry geometry = rowGeometry (rows, area.getHeight());
        const int scrollRows = effectiveScrollRows();
        constexpr int kGrabTolerancePixels = 4;
        for (int row = scrollRows; row < rows; ++row)
        {
            const int rowBottom = area.getY()
                                 + static_cast<int> (std::llround (
                                       geometry.top (row + 1) - geometry.top (scrollRows)));
            if (rowBottom > area.getBottom())
                break;
            if (std::abs (position.y - rowBottom) <= kGrabTolerancePixels)
                return row;
        }
        return -1;
    }

    [[nodiscard]] int rowAt (juce::Point<int> position) const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0)
            return -1;

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        if (! area.contains (position))
            return -1;

        const yesdaw::ui::CumulativeRowGeometry geometry = rowGeometry (rows, area.getHeight());
        const int scrollRows = effectiveScrollRows();
        const int row = geometry.rowAtPixel (
            static_cast<double> (position.y - area.getY()) + geometry.top (scrollRows));
        return row >= 0 && row < rows ? row : -1;
    }

public:
    // Vertical track scroll (E5): the rail shares the timeline's whole-row offset, clamped to its
    // own overflow so its last row always pins to the window bottom.
    std::function<int()> rowScrollProvider;
    std::function<void (int)> onVerticalScrollRows;

    [[nodiscard]] int maxScrollRows() const
    {
        const int rows = rowCountProvider ? rowCountProvider() : 0;
        if (rows <= 0)
            return 0;

        auto area = getLocalBounds();
        area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
        // N6: visibleRows stays an AUTO-SHARE-height approximation (matching the timeline's own
        // scroll-count heuristic) even with custom heights present — an honest, minor imprecision
        // rather than a full non-uniform scroll-extent solve, which the gate does not require.
        const yesdaw::ui::CumulativeRowGeometry geometry = rowGeometry (rows, area.getHeight());
        const int visibleRows = std::max (1, area.getHeight() / std::max (1, geometry.autoShare));
        return std::max (0, rows - visibleRows);
    }

    [[nodiscard]] int effectiveScrollRows() const
    {
        return std::clamp (rowScrollProvider ? rowScrollProvider() : 0, 0, maxScrollRows());
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        const double delta = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
        if (delta == 0.0 || ! onVerticalScrollRows)
            return;

        onVerticalScrollRows (delta > 0.0 ? -1 : 1);
    }
};

} // namespace yesdaw::ui
