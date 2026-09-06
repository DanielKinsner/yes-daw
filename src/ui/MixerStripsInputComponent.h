// YES DAW — the mixer strips' input overlay.
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
#include "ui/ShellWidgets.h"

namespace yesdaw::ui {

// The painted I/O cells' rows (moved with the component; the shell's paint and hit-test share them).
inline constexpr int kMixerIoInputRow = 0;
inline constexpr int kMixerIoOutputRow = 1;

// Transparent overlay across the mixer strip region: forwards a click to the strip index under the
// pointer (geometry owned by MainComponent so paint and hits share one source of truth).
class MixerStripsInputComponent final : public juce::Component,
                                        public juce::SettableTooltipClient
{
public:
    std::function<int (juce::Point<int>)> stripAtPosition;   // position in SHELL coordinates
    std::function<void (int)> onStripClicked;
    // E17: double-click opens the inline bus rename editor when the strip is a bus.
    std::function<void (int)> onStripDoubleClicked;
    // Painted meter hit test (B32): a click inside a track strip's painted meter clears its
    // held peak and latched clip light instead of retargeting the strip.
    std::function<int (juce::Point<int>)> meterStripAtPosition;
    std::function<void (int)> onMeterClicked;
    // M4: painted insert-slot hit test — a click on a slot row selects the strip and that slot.
    std::function<std::pair<int, int> (juce::Point<int>)> insertSlotAtPosition;
    std::function<bool (int, int)> insertSlotFilled;   // G1.7: an empty slot hints and menus "add"
    std::function<void (int, int)> onInsertSlotClicked;
    // G4.1 cp2: the lane is gone — a filled slot's double-click opens the effect's editor; an EMPTY
    // slot's left-click opens the add menu (the same list the right-click offers).
    std::function<void (int, int)> onInsertSlotDoubleClicked;
    // M5: painted send rows — press picks the row, drag sets the level, release commits ONE
    // undoable edit (a per-pixel commit would bury the undo stack).
    std::function<std::pair<int, int> (juce::Point<int>)> sendRowAtPosition;
    std::function<double (int, int, juce::Point<int>)> sendLevelForPosition;
    std::function<void (int, int, double, bool)> onSendRowDragged;
    // G4.1 cp2: an EMPTY send well's left-click opens the add menu (the buses); a routed row's
    // right-click is the send's menu (tap, destination, remove).
    std::function<bool (int, int)> sendRowFilled;
    // G4.1 cp2: the values the painted controls hold now — the anchor a Shift-fine drag starts from.
    std::function<float (int)> faderGainForStrip;
    std::function<float (int)> panForStrip;
    std::function<float (int, int)> sendLevelForRow;
    // N1: painted Mute/Solo cells — a click toggles THAT strip without stealing the selection.
    std::function<std::pair<int, int> (juce::Point<int>)> muteSoloCellAtPosition;
    std::function<void (int, int)> onMuteSoloCellClicked;
    // G4.1: the painted I/O slots — a click opens THAT slot's choices (a menu of inputs / outputs).
    std::function<std::pair<int, int> (juce::Point<int>)> ioRowAtPosition;   // strip, row (kMixerIo*Row)
    std::function<void (int, int, juce::Point<int>)> onIoRowClicked;         // strip, row, position (strips-local)
    // 2026-09-04 sweep: the painted fader rail and pan knob on an UNSELECTED strip looked
    // draggable and were not (only the selected strip carried live controls). A press on either
    // selects that strip and drags ITS value through the same verbs the control lane uses;
    // Alt-click resets (unity / centre). `ended` closes the one-undo-step gesture.
    std::function<int (juce::Point<int>)> faderRailAtPosition;             // shell position -> strip, -1 none
    std::function<float (int, juce::Point<int>)> faderGainForPosition;     // strip, shell position -> linear gain
    std::function<void (int, float, bool)> onFaderDragged;                 // strip, gain, ended
    std::function<int (juce::Point<int>)> panKnobAtPosition;
    std::function<float (int, juce::Point<int>)> panForPosition;           // strip, shell position -> pan -1..1
    std::function<void (int, float, bool)> onPanDragged;                   // strip, pan, ended

    // G1.6: the gesture hint for the hovered zone — the status line shows it while no status
    // message is active. hintAt() is the one law mouseMove and the harness share.
    std::function<void (const juce::String&)> onHoverHint;

    [[nodiscard]] juce::String hintAt (juce::Point<int> position, juce::ModifierKeys) const
    {
        if (getParentComponent() == nullptr)
            return {};
        const juce::Point<int> shellPosition = position + getPosition();
        if (muteSoloCellAtPosition)
            if (const auto [cellStrip, cellIndex] = muteSoloCellAtPosition (shellPosition); cellStrip >= 0 && cellIndex >= 0)
                return cellIndex == 0 ? "Solo: click toggles"
                     : cellIndex == 1 ? "Mute: click toggles"
                                      : "Arm: click toggles record arm";   // G4.1
        if (ioRowAtPosition)
            if (const auto [ioStrip, ioRow] = ioRowAtPosition (shellPosition); ioStrip >= 0 && ioRow >= 0)
                return ioRow == kMixerIoInputRow ? "Input: click to pick the input this track records from"
                                                 : "Output: click to route the strip to Master or a bus";
        if (sendRowAtPosition)
            if (const auto [sendStrip, sendIndex] = sendRowAtPosition (shellPosition); sendStrip >= 0 && sendIndex >= 0)
            {
                const bool routed = sendRowFilled == nullptr || sendRowFilled (sendStrip, sendIndex);
                return routed ? "Send: drag to set its level \u00b7 Shift for fine \u00b7 right-click for tap, destination, remove"
                              : "Empty send: click to add a send to a bus";   // G4.1 cp2
            }
        if (insertSlotAtPosition)
            if (const auto [slotStrip, slotIndex] = insertSlotAtPosition (shellPosition); slotStrip >= 0 && slotIndex >= 0)
            {
                const bool filled = insertSlotFilled == nullptr || insertSlotFilled (slotStrip, slotIndex);
                return filled ? "Insert slot: double-click to open its editor \u00b7 right-click to bypass, remove or move"
                              : "Empty insert: click to add an effect";   // G4.1 cp2
            }
        if (faderRailAtPosition && faderRailAtPosition (shellPosition) >= 0)
            return "Fader: drag the knob to set the level \u00b7 Shift for fine \u00b7 Alt-click resets to unity";
        if (panKnobAtPosition && panKnobAtPosition (shellPosition) >= 0)
            return "Pan: drag \u00b7 Shift for fine \u00b7 Alt-click recentres";
        if (meterStripAtPosition && meterStripAtPosition (shellPosition) >= 0)
            return "Meter: click clears the clip light";
        if (stripAtPosition && stripAtPosition (shellPosition) >= 0)
            return "Strip: click to select \u00b7 right-click for the strip menu";
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

    // G1.3: a right-click on a strip selects it and asks for the Mixer strip menu (the master
    // strip has none; insert slots come with their own list in the next checkpoint).
    std::function<void (yesdaw::ui::ContextMenuTarget, int, juce::Point<int>)> onContextMenuRequested;
    std::function<int ()> stripCountProvider;   // track + bus strips (the master is the next index)
    std::function<int ()> trackCountProvider;   // G4.1: the strips before it are Tracks, then Buses

    void requestContextMenu (juce::Point<int> position)
    {
        if (! onContextMenuRequested || ! stripAtPosition || getParentComponent() == nullptr)
            return;
        const juce::Point<int> shellPosition = position + getPosition();
        // An insert slot first: the click selects the strip and the slot (the same law as the
        // left-click), and the slot's own menu follows.
        if (insertSlotAtPosition && onInsertSlotClicked)
        {
            const auto [slotStrip, slotIndex] = insertSlotAtPosition (shellPosition);
            if (slotStrip >= 0 && slotIndex >= 0)
            {
                onInsertSlotClicked (slotStrip, slotIndex);
                onContextMenuRequested (yesdaw::ui::ContextMenuTarget::InsertSlot, slotIndex, position);
                return;
            }
        }
        // G4.1 cp2: a send row — routed: the send's menu; empty: Add Send (the buses). The click
        // selects the strip first, like every strip gesture.
        if (sendRowAtPosition)
        {
            const auto [sendStrip, sendIndex] = sendRowAtPosition (shellPosition);
            if (sendStrip >= 0 && sendIndex >= 0)
            {
                if (onStripClicked)
                    onStripClicked (sendStrip);
                onContextMenuRequested (yesdaw::ui::ContextMenuTarget::MixerSendRow, sendIndex, position);
                return;
            }
        }
        const int strip = stripAtPosition (shellPosition);
        const int stripCount = stripCountProvider ? stripCountProvider() : 0;
        const int trackCount = trackCountProvider ? trackCountProvider() : stripCount;
        // G4.1: the menu is per strip kind — the Tracks, then the Buses, then the master's lane.
        if (strip < 0 || strip > stripCount)
            return;
        if (onStripClicked)
            onStripClicked (strip);
        onContextMenuRequested (strip < trackCount ? yesdaw::ui::ContextMenuTarget::MixerStrip
                                : strip < stripCount ? yesdaw::ui::ContextMenuTarget::MixerBusStrip
                                                     : yesdaw::ui::ContextMenuTarget::MixerMasterStrip,
                                strip, position);
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const juce::Point<int> shellPosition =
            event.getEventRelativeTo (getParentComponent()).getPosition();

        if (event.mods.isRightButtonDown())   // the right button itself: on macOS isPopupMenu() also fires for Ctrl+click, which is a gesture modifier here
        {
            requestContextMenu (event.getPosition());
            return;
        }

        if (muteSoloCellAtPosition && onMuteSoloCellClicked)
        {
            const auto [cellStrip, cellIndex] = muteSoloCellAtPosition (shellPosition);
            if (cellStrip >= 0 && cellIndex >= 0)
            {
                onMuteSoloCellClicked (cellStrip, cellIndex);
                return;
            }
        }

        // G4.1: the I/O slots open their choices — before the slot rows they sit among.
        if (ioRowAtPosition && onIoRowClicked)
        {
            const auto [ioStrip, ioRow] = ioRowAtPosition (shellPosition);
            if (ioStrip >= 0 && ioRow >= 0)
            {
                onIoRowClicked (ioStrip, ioRow, event.getPosition());
                return;
            }
        }

        if (sendRowAtPosition && sendLevelForPosition && onSendRowDragged)
        {
            const auto [sendStrip, sendIndex] = sendRowAtPosition (shellPosition);
            if (sendStrip >= 0 && sendIndex >= 0)
            {
                // G4.1 cp2: an EMPTY well is the add menu (the lane's + Send chooser is gone).
                if (sendRowFilled && ! sendRowFilled (sendStrip, sendIndex))
                {
                    if (onStripClicked)
                        onStripClicked (sendStrip);
                    if (onContextMenuRequested)
                        onContextMenuRequested (yesdaw::ui::ContextMenuTarget::MixerSendRow, sendIndex, event.getPosition());
                    return;
                }
                if (event.mods.isAltDown())
                {
                    onSendRowDragged (sendStrip, sendIndex, 1.0, true);   // Alt-click: unity, one step (the live slider's reset)
                    return;
                }
                draggingSendStrip = sendStrip;
                draggingSendIndex = sendIndex;
                const float plain = static_cast<float> (sendLevelForPosition (sendStrip, sendIndex, shellPosition));
                const float current = sendLevelForRow ? sendLevelForRow (sendStrip, sendIndex) : plain;
                onSendRowDragged (sendStrip, sendIndex,
                                  static_cast<double> (fineDragValue (event.mods, plain, current, sendFine)), false);
                return;
            }
        }

        if (insertSlotAtPosition && onInsertSlotClicked)
        {
            const auto [slotStrip, slotIndex] = insertSlotAtPosition (shellPosition);
            if (slotStrip >= 0 && slotIndex >= 0)
            {
                onInsertSlotClicked (slotStrip, slotIndex);
                // G4.1 cp2: an EMPTY slot's click is the add menu (the lane's Add FX chooser is gone).
                if (insertSlotFilled && ! insertSlotFilled (slotStrip, slotIndex) && onContextMenuRequested)
                    onContextMenuRequested (yesdaw::ui::ContextMenuTarget::InsertSlot, slotIndex, event.getPosition());
                return;
            }
        }

        if (faderRailAtPosition && faderGainForPosition && onFaderDragged)
        {
            const int strip = faderRailAtPosition (shellPosition);
            if (strip >= 0)
            {
                if (event.mods.isAltDown())
                {
                    onFaderDragged (strip, 1.0f, true);   // Alt-click: unity, one step
                    return;
                }
                draggingFaderStrip = strip;
                const float plain = faderGainForPosition (strip, shellPosition);
                const float current = faderGainForStrip ? faderGainForStrip (strip) : plain;
                onFaderDragged (strip, fineDragValue (event.mods, plain, current, faderFine), false);
                return;
            }
        }

        if (panKnobAtPosition && panForPosition && onPanDragged)
        {
            const int strip = panKnobAtPosition (shellPosition);
            if (strip >= 0)
            {
                if (event.mods.isAltDown())
                {
                    onPanDragged (strip, 0.0f, true);   // Alt-click: centre, one step
                    return;
                }
                draggingPanStrip = strip;
                const float plain = panForPosition (strip, shellPosition);
                const float current = panForStrip ? panForStrip (strip) : plain;
                onPanDragged (strip, fineDragValue (event.mods, plain, current, panFine), false);
                return;
            }
        }

        if (meterStripAtPosition && onMeterClicked)
        {
            const int meterStrip = meterStripAtPosition (shellPosition);
            if (meterStrip >= 0)
            {
                onMeterClicked (meterStrip);
                return;
            }
        }

        if (! stripAtPosition || ! onStripClicked)
            return;

        const int strip = stripAtPosition (shellPosition);
        if (strip >= 0)
            onStripClicked (strip);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const juce::Point<int> shellPosition =
            event.getEventRelativeTo (getParentComponent()).getPosition();
        forwardDrag (event.mods, shellPosition, false);
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        const juce::Point<int> shellPosition =
            event.getEventRelativeTo (getParentComponent()).getPosition();
        forwardDrag (event.mods, shellPosition, true);
        draggingFaderStrip = -1;
        draggingPanStrip = -1;
        draggingSendStrip = -1;
        draggingSendIndex = -1;
        faderFine = {};
        panFine = {};
        sendFine = {};
    }

    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (getParentComponent() == nullptr)
            return;
        const juce::Point<int> shellPosition =
            event.getEventRelativeTo (getParentComponent()).getPosition();
        // G4.1 cp2: a FILLED insert slot's double-click opens the effect's editor; an empty slot's
        // double-click is nothing (its first click already opened the add menu) — never a rename.
        if (insertSlotAtPosition)
        {
            const auto [slotStrip, slotIndex] = insertSlotAtPosition (shellPosition);
            if (slotStrip >= 0 && slotIndex >= 0)
            {
                const bool filled = insertSlotFilled == nullptr || insertSlotFilled (slotStrip, slotIndex);
                if (filled && onInsertSlotDoubleClicked)
                    onInsertSlotDoubleClicked (slotStrip, slotIndex);
                return;
            }
        }
        if (! stripAtPosition || ! onStripDoubleClicked)
            return;
        const int strip = stripAtPosition (shellPosition);
        if (strip >= 0)
            onStripDoubleClicked (strip);
    }

private:
    // G4.1 cp2: Shift while dragging is ten times finer (fineDragScale, the FineDragSlider law the
    // lane's live sliders had): the value is anchored where Shift ARRIVED — the control's value then
    // and the pointer's plain value then — so a Shift press that does not move never jumps, and
    // releasing Shift mid-drag returns to the plain law.
    struct FineAnchor
    {
        bool active = false;
        float value = 0.0f;        // the control's value when Shift arrived
        float plainAtAnchor = 0.0f;   // the pointer's plain value when Shift arrived
    };
    static float fineDragValue (juce::ModifierKeys mods, float plain, float current, FineAnchor& anchor) noexcept
    {
        if (! mods.isShiftDown())
        {
            anchor.active = false;
            return plain;
        }
        if (! anchor.active)
            anchor = { true, current, plain };
        return anchor.value
             + (plain - anchor.plainAtAnchor) * static_cast<float> (yesdaw::ui::UiTheme::Layout::fineDragScale);
    }
    // One forward for the drag and the release: the fader, the pan, or the send being dragged.
    void forwardDrag (juce::ModifierKeys mods, juce::Point<int> shellPosition, bool ended)
    {
        if (draggingFaderStrip >= 0 && faderGainForPosition && onFaderDragged)
        {
            const float plain = faderGainForPosition (draggingFaderStrip, shellPosition);
            const float current = faderGainForStrip ? faderGainForStrip (draggingFaderStrip) : plain;
            onFaderDragged (draggingFaderStrip, fineDragValue (mods, plain, current, faderFine), ended);
            return;
        }
        if (draggingPanStrip >= 0 && panForPosition && onPanDragged)
        {
            const float plain = panForPosition (draggingPanStrip, shellPosition);
            const float current = panForStrip ? panForStrip (draggingPanStrip) : plain;
            onPanDragged (draggingPanStrip, fineDragValue (mods, plain, current, panFine), ended);
            return;
        }
        if (draggingSendStrip < 0 || ! sendLevelForPosition || ! onSendRowDragged)
            return;
        const float plain = static_cast<float> (sendLevelForPosition (draggingSendStrip, draggingSendIndex, shellPosition));
        const float current = sendLevelForRow ? sendLevelForRow (draggingSendStrip, draggingSendIndex) : plain;
        onSendRowDragged (draggingSendStrip, draggingSendIndex,
                          static_cast<double> (fineDragValue (mods, plain, current, sendFine)), ended);
    }

    int draggingSendStrip = -1;
    int draggingFaderStrip = -1;   // the painted fader being dragged (2026-09-04)
    int draggingPanStrip = -1;
    int draggingSendIndex = -1;
    FineAnchor faderFine, panFine, sendFine;
};

} // namespace yesdaw::ui
