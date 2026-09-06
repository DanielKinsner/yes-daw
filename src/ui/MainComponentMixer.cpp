// YES DAW — the app shell: the mixer dock, the strips, inserts, sends, the FX editor, the instrument panel.
//
// Plan §5.1 (the shell topology), checkpoint 2 (2026-09-05): member bodies of MainComponent, carved
// verbatim from the inline class. The declaration is ui/MainComponentShell.h.

#include "ui/MainComponentShell.h"

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

// V3: a real toggle for the always-on bottom mixer dock — collapsing it reclaims vertical
// space for the timeline/rail/inspector; the full-view Mixer panel is unaffected.
void MainComponent::configureMixerDockToggle()
{
    constexpr yesdaw::ui::UiActionId action = yesdaw::ui::UiActionId::TimelineToggleMixerDock;
    configureActionComponent (mixerDockToggle, action, "Mixer dock");
    mixerDockToggle.setButtonText ("X");   // G2.1 cp3: the view cluster's X (tooltip carries the name + chord)
    mixerDockToggle.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
    mixerDockToggle.setColour (juce::TextButton::buttonOnColourId, kPurple.darker (0.45f));
    mixerDockToggle.setColour (juce::TextButton::textColourOffId, kText);
    mixerDockToggle.setColour (juce::TextButton::textColourOnId, kText);
    mixerDockToggle.onClick = [this] {
        (void) appModel.dispatch (yesdaw::ui::UiActionId::TimelineToggleMixerDock);
        refreshActionState();
        resized();
        repaintAll();
    };
    addAndMakeVisible (mixerDockToggle);
}

void MainComponent::configureMixerControls()
{
    // G4.1: no "select first track" button — the strip's header IS the name (a click selects,
    // a double-click renames), as on every reference mixer.

    // Every mixer strip is selectable (usable-DAW P0): clicking a Track strip retargets the shared
    // fader/pan/mute/solo controls and moves them onto that strip.
    mixerStripsInput.setComponentID ("shell.mixer.strips.input");
    mixerStripsInput.setName ("Mixer Strips");
    mixerStripsInput.setTitle ("Mixer Strips");
    mixerStripsInput.setTooltip ("Mixer strips: click a strip to retarget the shared controls, click a meter to clear its clip light");
    mixerStripsInput.onContextMenuRequested = [this] (yesdaw::ui::ContextMenuTarget target, int index, juce::Point<int> position) {
        openContextMenu (target, index, mixerStripsInput, position);
    };
    mixerStripsInput.stripCountProvider = [this] {
        const auto surface = currentMixerSurface();
        return static_cast<int> (surface.tracks.size() + surface.buses.size());
    };
    mixerStripsInput.trackCountProvider = [this] {   // G4.1
        return static_cast<int> (currentMixerSurface().tracks.size());
    };
    mixerStripsInput.onStripClicked = [this] (int stripIndex) {
        const auto surface = currentMixerSurface();
        const int trackCount = static_cast<int> (surface.tracks.size());
        const int busCount = static_cast<int> (surface.buses.size());
        // R11: the lane after the buses is the MASTER strip, selectable for its FX chain.
        if (stripIndex < 0 || stripIndex > trackCount + busCount)
            return;

        // E16: strips past the tracks are the buses, selectable in their own right.
        if (stripIndex < trackCount)
        {
            (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
            selectedTrackLane = stripIndex;   // rail selection follows the mixer strip
        }
        else if (stripIndex < trackCount + busCount)
        {
            (void) appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount));
        }
        else
        {
            (void) appModel.selectMixerMaster();
        }
        layoutMixerControls();
        refreshActionState();
        repaintAll();
    };
    // E17: double-clicking a BUS strip opens the inline rename editor over its header.
    // A double-click on a strip's name band renames it inline: a bus through its own editor,
    // a track through the rail's editor placed over the strip (until 2026-09-04 only buses
    // renamed here; a track strip's double-click did nothing).
    mixerStripsInput.onStripDoubleClicked = [this] (int stripIndex) {
        const auto surface = currentMixerSurface();
        const int trackCount = static_cast<int> (surface.tracks.size());
        const int busCount = static_cast<int> (surface.buses.size());
        if (stripIndex < 0 || stripIndex >= trackCount + busCount)
            return;

        if (stripIndex < trackCount)
        {
            openTrackRenameEditorOverStrip (stripIndex);
            return;
        }
        openBusRenameEditor (stripIndex - trackCount, stripIndex);
    };
    mixerStripsInput.meterStripAtPosition = [this] (juce::Point<int> positionInShell) {
        // E22: bus meters are clickable like track meters — the index spans tracks then buses.
        const std::size_t stripTotal = appModel.context().projectLoaded
                                           ? appModel.project().tracks.size()
                                                 + appModel.project().buses.size()
                                           : 0u;
        for (std::size_t i = 0; i < stripTotal; ++i)
            if (paintedMeterBoundsForLane (paintedMixerLaneBounds (i), stripIoRows (i)).contains (positionInShell))
                return static_cast<int> (i);
        return -1;
    };
    mixerStripsInput.onMeterClicked = [this] (int stripIndex) {
        const int trackCount = static_cast<int> (appModel.project().tracks.size());
        if (stripIndex < trackCount)
            clearTrackMeterHold (stripIndex);
        else
            clearBusMeterHold (stripIndex - trackCount);   // E22
    };
    // N1: a click on a painted Mute/Solo cell toggles THAT strip. It is not a selection
    // gesture — the mixer target the control lane edits stays where the user put it.
    mixerStripsInput.muteSoloCellAtPosition = [this] (juce::Point<int> positionInShell) {
        const auto surface = currentMixerSurface();
        const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
        for (std::size_t i = 0; i < stripTotal; ++i)
        {
            const auto lane = paintedMixerLaneBounds (i);
            const std::size_t cellCount = stripCellCount (i);   // G4.1: S / M / R on a Track, S / M on a Bus
            for (std::size_t cell = 0; cell < cellCount; ++cell)
                if (paintedMuteSoloCellBoundsForLane (lane, cell, cellCount).contains (positionInShell))
                    return std::pair<int, int> { static_cast<int> (i), static_cast<int> (cell) };
        }
        return std::pair<int, int> { -1, -1 };
    };
    // 2026-09-04 sweep: every strip's painted fader and pan drag their OWN strip (the selected
    // strip carries the live controls; the others only looked draggable). The press retargets
    // the mixer to that strip, then the drag rides the same scalar verbs the control lane
    // uses, coalesced into one undo step (beginStripGesture ... endStripGesture).
    mixerStripsInput.faderRailAtPosition = [this] (juce::Point<int> positionInShell) {
        const auto surface = currentMixerSurface();
        const std::size_t trackCount = surface.tracks.size();
        const std::size_t stripTotal = trackCount + surface.buses.size();
        for (std::size_t i = 0; i < stripTotal; ++i)
        {
            const float gain = i < trackCount ? surface.tracks[i].linearGain
                                              : surface.buses[i - trackCount].linearGain;
            if (paintedFaderThumbForLane (paintedMixerLaneBounds (i), gain, stripIoRows (i))
                    .expanded (0, yesdaw::ui::UiTheme::Layout::trackListLevelHitSlopX)
                    .contains (positionInShell))
                return static_cast<int> (i);
        }
        return -1;
    };
    mixerStripsInput.faderGainForPosition = [this] (int stripIndex, juce::Point<int> positionInShell) {
        const auto rail = paintedFaderRailForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                                   stripIoRows (static_cast<std::size_t> (stripIndex)));
        if (rail.getHeight() <= 0)
            return 1.0f;
        const float fraction = juce::jlimit (0.0f, 1.0f,
                                             static_cast<float> (rail.getBottom() - positionInShell.y)
                                                 / static_cast<float> (rail.getHeight()));
        return fraction * static_cast<float> (yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax);
    };
    mixerStripsInput.panKnobAtPosition = [this] (juce::Point<int> positionInShell) {
        const auto surface = currentMixerSurface();
        const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
        for (std::size_t i = 0; i < stripTotal; ++i)
            if (paintedPanKnobForLane (paintedMixerLaneBounds (i)).contains (positionInShell))
                return static_cast<int> (i);
        return -1;
    };
    mixerStripsInput.panForPosition = [this] (int stripIndex, juce::Point<int> positionInShell) {
        const auto knob = paintedPanKnobForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)));
        if (knob.getWidth() <= 0)
            return 0.0f;
        const float normalized = static_cast<float> (positionInShell.x - knob.getX())
                               / static_cast<float> (knob.getWidth());
        return juce::jlimit (-1.0f, 1.0f, normalized + normalized - 1.0f);
    };
    const auto retargetMixerStrip = [this] (int stripIndex) {
        const auto surface = currentMixerSurface();
        const int trackCount = static_cast<int> (surface.tracks.size());
        if (stripIndex < trackCount)
        {
            selectedTrackLane = stripIndex;   // the rail follows, as a strip click does
            return appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex), false);
        }
        return appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount), false);
    };
    // G4.1 cp2: the painted drags are THE fader and pan now (the lane's live sliders are gone), so
    // they carry what those did: one undo step per drag, the dB readout, and the Touch / Latch ride
    // (R15 / N5 — an armed ride buffers the points and commits ONE lane edit on release).
    mixerStripsInput.onFaderDragged = [this, retargetMixerStrip] (int stripIndex, float linearGain, bool ended) {
        const bool pressed = paintedFaderDragStrip != stripIndex;
        appModel.beginStripGesture();
        if (retargetMixerStrip (stripIndex))
        {
            if (pressed)
            {
                paintedFaderDragStrip = stripIndex;
                beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::TrackFader,
                                                 yesdaw::engine::FaderNode::kGainParameterId);
            }
            if (automationTouchRideActive)
                recordAutomationTouchSample (automationNormalizedForFaderGain (linearGain));
            else
                (void) appModel.setSelectedMixerFader (linearGain);
            showDragDbReadout (paintedFaderRailForLane (paintedMixerLaneBounds (static_cast<std::size_t> (stripIndex)),
                                                        stripIoRows (static_cast<std::size_t> (stripIndex))),
                               linearGain);
        }
        if (ended)
        {
            paintedFaderDragStrip = -1;
            endAutomationTouchRideIfActive();
            appModel.endStripGesture();
            hideDragDbReadout();
        }
        refreshActionState();
        resized();
        repaintAll();
    };
    mixerStripsInput.onPanDragged = [this, retargetMixerStrip] (int stripIndex, float pan, bool ended) {
        const bool pressed = paintedPanDragStrip != stripIndex;
        appModel.beginStripGesture();
        if (retargetMixerStrip (stripIndex))
        {
            if (pressed)
            {
                paintedPanDragStrip = stripIndex;
                beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::TrackPan,
                                                 yesdaw::engine::PanNode::kPanParameterId);
            }
            if (automationTouchRideActive)
                recordAutomationTouchSample (automationNormalizedForPan (pan));
            else
                (void) appModel.setSelectedMixerPan (pan);
        }
        if (ended)
        {
            paintedPanDragStrip = -1;
            endAutomationTouchRideIfActive();
            appModel.endStripGesture();
        }
        refreshActionState();
        resized();
        repaintAll();
    };
    mixerStripsInput.onMuteSoloCellClicked = [this] (int stripIndex, int cellIndex) {
        const auto& tracks = appModel.project().tracks;
        const auto& buses = appModel.project().buses;
        if (stripIndex < 0)
            return;

        const std::size_t strip = static_cast<std::size_t> (stripIndex);
        const bool solo = cellIndex == 0;
        if (strip < tracks.size())
        {
            if (cellIndex == static_cast<int> (kMixerPaintedTrackCellCount) - 1)
            {
                // G4.1: the R cell — the arm set (M11), on THAT track; transient like the rail's badge.
                (void) appModel.toggleRecordingArmForTrack (strip);
                refreshActionState();
                repaintAll();
                return;
            }
            const yesdaw::engine::EntityId trackId = tracks[strip].id;
            (void) (solo ? appModel.toggleTrackSolo (trackId) : appModel.toggleTrackMute (trackId));
        }
        else if (strip - tracks.size() < buses.size())
        {
            const yesdaw::engine::EntityId busId = buses[strip - tracks.size()].id;
            (void) (solo ? appModel.toggleBusSolo (busId) : appModel.toggleBusMute (busId));
        }

        refreshActionState();
        repaintAll();
    };
    // M4: a click on a painted insert row selects the strip and opens THAT slot's params.
    mixerStripsInput.insertSlotAtPosition = [this] (juce::Point<int> positionInShell) {
        const auto surface = currentMixerSurface();
        const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
        for (std::size_t i = 0; i < stripTotal; ++i)
        {
            const auto lane = paintedMixerLaneBounds (i);
            for (std::size_t slot = 0; slot < static_cast<std::size_t> (
                     paintedInsertRowCountForLane (lane)); ++slot)
                if (paintedInsertRowBoundsForLane (lane, slot, stripIoRows (i)).contains (positionInShell))
                    return std::pair<int, int> { static_cast<int> (i), static_cast<int> (slot) };
        }
        return std::pair<int, int> { -1, -1 };
    };
    // G4.1: the I/O slots — the click selects the strip and opens the slot's choices as a menu
    // (headless: the record the harness reads, like every other menu).
    mixerStripsInput.ioRowAtPosition = [this] (juce::Point<int> positionInShell) {
        const auto surface = currentMixerSurface();
        const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
        for (std::size_t i = 0; i < stripTotal; ++i)
        {
            const auto lane = paintedMixerLaneBounds (i);
            const int ioRows = stripIoRows (i);
            if (paintedInputRowBoundsForLane (lane, ioRows).contains (positionInShell))
                return std::pair<int, int> { static_cast<int> (i), kMixerIoInputRow };
            if (paintedOutputRowBoundsForLane (lane, ioRows).contains (positionInShell))
                return std::pair<int, int> { static_cast<int> (i), kMixerIoOutputRow };
        }
        return std::pair<int, int> { -1, -1 };
    };
    mixerStripsInput.onIoRowClicked = [this] (int stripIndex, int row, juce::Point<int> positionInStrips) {
        if (mixerStripsInput.onStripClicked)
            mixerStripsInput.onStripClicked (stripIndex);
        openContextMenu (row == kMixerIoInputRow ? yesdaw::ui::ContextMenuTarget::MixerStripInput
                                                 : yesdaw::ui::ContextMenuTarget::MixerStripOutput,
                         stripIndex, mixerStripsInput, positionInStrips);
    };
    mixerStripsInput.insertSlotFilled = [this] (int strip, int slot) {
        const auto surface = currentMixerSurface();
        const std::size_t trackCount = surface.tracks.size();
        const auto stripIndex = static_cast<std::size_t> (strip);
        if (strip < 0 || stripIndex >= trackCount + surface.buses.size())
            return false;
        const auto& state = stripIndex < trackCount ? surface.tracks[stripIndex]
                                                    : surface.buses[stripIndex - trackCount];
        return slot >= 0 && static_cast<std::size_t> (slot) < state.fxSlots.size();
    };
    // G4.1 cp2: an empty send well is the add menu; the values the fine-drag anchors read.
    mixerStripsInput.sendRowFilled = [this] (int strip, int row) {
        const auto surface = currentMixerSurface();
        const std::size_t trackCount = surface.tracks.size();
        const auto stripIndex = static_cast<std::size_t> (strip);
        if (strip < 0 || stripIndex >= trackCount + surface.buses.size())
            return false;
        const auto& state = stripIndex < trackCount ? surface.tracks[stripIndex]
                                                    : surface.buses[stripIndex - trackCount];
        return row >= 0 && static_cast<std::size_t> (row) < state.sends.size();
    };
    mixerStripsInput.faderGainForStrip = [this] (int strip) {
        const auto surface = currentMixerSurface();
        const std::size_t trackCount = surface.tracks.size();
        const auto stripIndex = static_cast<std::size_t> (juce::jmax (0, strip));
        if (stripIndex < trackCount)
            return surface.tracks[stripIndex].linearGain;
        if (stripIndex - trackCount < surface.buses.size())
            return surface.buses[stripIndex - trackCount].linearGain;
        return 1.0f;
    };
    mixerStripsInput.panForStrip = [this] (int strip) {
        const auto surface = currentMixerSurface();
        const std::size_t trackCount = surface.tracks.size();
        const auto stripIndex = static_cast<std::size_t> (juce::jmax (0, strip));
        if (stripIndex < trackCount)
            return surface.tracks[stripIndex].pan;
        if (stripIndex - trackCount < surface.buses.size())
            return surface.buses[stripIndex - trackCount].pan;
        return 0.0f;
    };
    mixerStripsInput.sendLevelForRow = [this] (int strip, int row) {
        const auto surface = currentMixerSurface();
        const std::size_t trackCount = surface.tracks.size();
        const auto stripIndex = static_cast<std::size_t> (juce::jmax (0, strip));
        const yesdaw::ui::UiMixerStrip* state = stripIndex < trackCount ? &surface.tracks[stripIndex]
                                              : stripIndex - trackCount < surface.buses.size() ? &surface.buses[stripIndex - trackCount]
                                                                                                : nullptr;
        if (state == nullptr || row < 0 || static_cast<std::size_t> (row) >= state->sends.size())
            return 1.0f;
        return state->sends[static_cast<std::size_t> (row)].linearGain;
    };
    // G4.1 cp2: a filled slot's double-click opens the editor on THAT slot.
    mixerStripsInput.onInsertSlotDoubleClicked = [this] (int stripIndex, int slotIndex) {
        openFxEditor (stripIndex, slotIndex);
    };
    // M5: painted send rows. The press selects the strip and previews the level; the release
    // commits ONE undoable SetSendLevel through the same model verb the control lane uses.
    // G4.1 cp2: a Bus strip's rows drag too (R13: sends originate on Tracks AND Buses), and the
    // drag rides Touch / Latch (R15: the SendLevel lane, the live slider's law).
    mixerStripsInput.sendRowAtPosition = [this] (juce::Point<int> positionInShell) {
        const auto surface = currentMixerSurface();
        const std::size_t stripTotal = surface.tracks.size() + surface.buses.size();
        for (std::size_t i = 0; i < stripTotal; ++i)
        {
            const auto lane = paintedMixerLaneBounds (i);
            const int ioRows = stripIoRows (i);
            for (std::size_t sendIndex = 0;
                 sendIndex < static_cast<std::size_t> (paintedSendRowCountForLane (lane, ioRows));
                 ++sendIndex)
                if (paintedSendRowBoundsForLane (lane, sendIndex, ioRows).contains (positionInShell))
                    return std::pair<int, int> { static_cast<int> (i), static_cast<int> (sendIndex) };
        }
        return std::pair<int, int> { -1, -1 };
    };
    mixerStripsInput.sendLevelForPosition = [this] (int stripIndex, int sendIndex, juce::Point<int> positionInShell) {
        const auto row = paintedSendRowBoundsForLane (
            paintedMixerLaneBounds (static_cast<std::size_t> (juce::jmax (0, stripIndex))),
            static_cast<std::size_t> (juce::jmax (0, sendIndex)),
            stripIoRows (static_cast<std::size_t> (juce::jmax (0, stripIndex))));
        const auto bar = row.reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX,
                                      yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX);
        if (bar.getWidth() <= 0)
            return 0.0;

        return std::clamp (static_cast<double> (positionInShell.x - bar.getX())
                               / static_cast<double> (bar.getWidth()),
                           0.0,
                           1.0);
    };
    mixerStripsInput.onSendRowDragged = [this] (int stripIndex, int sendIndex, double level, bool commit) {
        const auto surface = currentMixerSurface();
        const int trackCount = static_cast<int> (surface.tracks.size());
        const int busCount = static_cast<int> (surface.buses.size());
        if (stripIndex < 0 || stripIndex >= trackCount + busCount || sendIndex < 0)
            return;
        const yesdaw::ui::UiMixerStrip& state = stripIndex < trackCount
            ? surface.tracks[static_cast<std::size_t> (stripIndex)]
            : surface.buses[static_cast<std::size_t> (stripIndex - trackCount)];
        if (static_cast<std::size_t> (sendIndex) >= state.sends.size())
            return;                                   // an empty send well has nothing to set

        const bool pressed = paintedSendDragPreview.stripIndex != stripIndex
                          || paintedSendDragPreview.sendIndex != sendIndex;
        if (stripIndex < trackCount)
        {
            (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
            selectedTrackLane = stripIndex;
        }
        else
            (void) appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount));
        if (pressed)
            beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::SendLevel,
                                             static_cast<std::uint32_t> (sendIndex));
        if (! commit)
        {
            paintedSendDragPreview = { stripIndex, sendIndex, static_cast<float> (level) };
            if (automationTouchRideActive)
                recordAutomationTouchSample (automationNormalizedForFaderGain (level));
            repaintAll();
            return;
        }

        paintedSendDragPreview = {};
        if (automationTouchRideActive)
        {
            recordAutomationTouchSample (automationNormalizedForFaderGain (level));
            endAutomationTouchRideIfActive();   // the ride is the edit (one undo step)
        }
        else
            (void) appModel.setSendLevelOnSelectedTrack (static_cast<std::size_t> (sendIndex),
                                                        static_cast<float> (level));
        layoutMixerControls();
        refreshActionState();
        repaintAll();
    };
    mixerStripsInput.onInsertSlotClicked = [this] (int stripIndex, int slotIndex) {
        lastContextMenu = {};   // a slot click opens no menu of its own (the empty slot's add menu follows separately)
        const auto surface = currentMixerSurface();
        const int trackCount = static_cast<int> (surface.tracks.size());
        const int busCount = static_cast<int> (surface.buses.size());
        if (stripIndex < 0 || stripIndex >= trackCount + busCount)
            return;

        if (stripIndex < trackCount)
        {
            (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
            selectedTrackLane = stripIndex;
        }
        else
        {
            (void) appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount));
        }

        // The click selects the slot (the strip paints it selected); an empty slot selects nothing.
        // G4.1 cp2: the editor, when open, follows the selected slot of the selected strip.
        const std::size_t chainSize = appModel.selectedStripFxChain().size();
        selectedFxParamSlot = static_cast<std::size_t> (slotIndex) < chainSize ? slotIndex : -1;
        selectedFxParamPage = 0;
        fxEditorStripOrdinal = appModel.selectedMixerStripOrdinal();
        layoutMixerControls();
        refreshActionState();
        resized();
        repaintAll();
    };
    // E25: clicks hit-test the PAINTED lanes — the same geometry the eye sees.
    mixerStripsInput.stripAtPosition = [this] (juce::Point<int> positionInShell) {
        const auto surface = currentMixerSurface();
        // R11: one lane past the buses — the master strip's lane — hit-tests too.
        const std::size_t stripTotal = surface.tracks.size() + surface.buses.size() + 1u;
        for (std::size_t i = 0; i < stripTotal; ++i)
            if (paintedMixerLaneBounds (i).contains (positionInShell))
                return static_cast<int> (i);
        return -1;
    };
    addAndMakeVisible (mixerStripsInput);
    mixerStripsInput.toBack();   // the shared strip controls stay on top and keep their own clicks

    // G4.1 cp2: the lane's Add FX chooser and slot rows are gone — an empty painted slot's click is
    // the add menu, a filled slot's double-click the editor, its right-click the slot menu.
    // Send routing (ADR-0044): + Bus creates a persisted Bus; the send chooser routes the
    // selected track to a bus; each visible send row edits its level and removes undoably.
    // E19: the master fader edits the persisted master gain undoably.
    configureActionComponent (mixerMasterFader, yesdaw::ui::UiActionId::MixerMasterSetFader, "Master fader");
    mixerMasterFader.setSliderStyle (juce::Slider::LinearVertical);
    mixerMasterFader.setTextBoxStyle (juce::Slider::NoTextBox,
                                      false,
                                      yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                      yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
    mixerMasterFader.setRange (yesdaw::ui::UiTheme::Layout::mixerFaderSliderMin,
                               yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax,
                               yesdaw::ui::UiTheme::Layout::mixerFaderSliderInterval);
    mixerMasterFader.setValue (yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault,
                               juce::dontSendNotification);
    mixerMasterFader.setDoubleClickReturnValue (true, yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault);
    // E21: a master fader drag is ONE undo step.
    mixerMasterFader.onDragStart = [this] { appModel.beginStripGesture(); };
    mixerMasterFader.onDragEnd = [this] { appModel.endStripGesture(); };
    mixerMasterFader.onValueChange = [this] {
        if (refreshingMixerControls || ! mixerMasterFader.isEnabled())
            return;

        if (mixerMasterFader.isMouseButtonDown())
            appModel.beginStripGesture();

        (void) appModel.setMasterFader (static_cast<float> (mixerMasterFader.getValue()));
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (mixerMasterFader);

    // G4.1 cp2: + Bus / - Bus are the strip menus' Add Bus / Remove Bus (cp1).
    // E17: inline bus rename editor, the marker/clip editor pattern on the mixer panel.
    busRenameEditor.setComponentID ("shell.mixer.bus.rename");
    busRenameEditor.setTooltip ("Rename bus: Enter commits, Escape cancels");
    busRenameEditor.setName ("Rename bus");
    busRenameEditor.setSelectAllWhenFocused (true);
    busRenameEditor.onReturnKey = [this] { commitBusRenameEditor(); };
    busRenameEditor.onEscapeKey = [this] { dismissBusRenameEditor(); };
    busRenameEditor.onFocusLost = [this] { dismissBusRenameEditor(); };
    addChildComponent (busRenameEditor);

    // G4.1 cp2: + Send, Out: and the send rows are the strip's wells and slots (an empty send well's
    // click adds; the routed row drags its level and right-clicks its menu; the OUTPUT slot routes).

    // G4.1 cp2: the FX editor hosts the parameter rows; Bypass and Close act on the slot it shows.
    fxEditor.onClose = [this] { closeFxEditor(); };
    fxEditor.onBypass = [this] {
        if (selectedFxParamSlot < 0)
            return;
        (void) appModel.toggleFxInsertEnabledOnSelectedStrip (static_cast<std::size_t> (selectedFxParamSlot));
        refreshActionState();
        repaintAll();
    };
    addChildComponent (fxEditor);

    // FX parameter editing (usable-DAW P1): the selected slot's ParamSpecs become live sliders;
    // every committed value is one undoable SetFxInsertParam through the model.
    for (std::size_t index = 0; index < mixerFxParamSliders.size(); ++index)
    {
        auto& label = mixerFxParamLabels[index];
        label.setComponentID ("mixer.fx.param." + juce::String (static_cast<int> (index)) + ".label");
        label.setTooltip ("FX parameter " + juce::String (static_cast<int> (index) + 1) + " readout");
        label.setColour (juce::Label::textColourId, kText);
        label.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
        label.setInterceptsMouseClicks (false, false);
        fxEditor.addChildComponent (label);   // G4.1 cp2: the rows live in the editor

        auto& slider = mixerFxParamSliders[index];
        configureActionComponent (slider, yesdaw::ui::UiActionId::MixerFxInsertParamSet, "FX parameter");
        slider.setComponentID ("mixer.fx.param." + juce::String (static_cast<int> (index)));
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox,
                                false,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        slider.setRange (0.0, 1.0, 0.0);
        // R15: an FX-param drag rides Touch/Latch too. The ride owner is the INSERT's own
        // id (the FxInsertParam lane law); the slider value is already the normalized 0..1
        // the lane and the node both speak.
        slider.onDragStart = [this, index] {
            const std::vector<yesdaw::engine::FxInsert> chain = appModel.selectedStripFxChain();
            if (selectedFxParamSlot >= 0
                && static_cast<std::size_t> (selectedFxParamSlot) < chain.size())
                beginAutomationTouchRideIfArmed (
                    yesdaw::engine::AutomationTargetRole::FxInsertParam,
                    mixerFxParamSliderIds[index],
                    chain[static_cast<std::size_t> (selectedFxParamSlot)].id);
        };
        slider.onDragEnd = [this] { endAutomationTouchRideIfActive(); };
        slider.onValueChange = [this, index] {
            if (refreshingFxParamControls || selectedFxParamSlot < 0)
                return;

            if (automationTouchRideActive)
                recordAutomationTouchSample (mixerFxParamSliders[index].getValue());
            else
                (void) appModel.setFxInsertParamOnSelectedStrip (
                    static_cast<std::size_t> (selectedFxParamSlot),
                    mixerFxParamSliderIds[index],
                    mixerFxParamSliders[index].getValue());
            refreshActionState();
            repaintAll();
        };
        fxEditor.addChildComponent (slider);

        // E15: choice-shaped params (EQ band type, delay ping-pong) get a real chooser in
        // place of the raw slider.
        auto& choiceChooser = mixerFxParamChoosers[index];
        configureActionComponent (choiceChooser, yesdaw::ui::UiActionId::MixerFxInsertParamSet,
                                  "FX parameter choice");
        choiceChooser.setComponentID ("mixer.fx.param." + juce::String (static_cast<int> (index))
                                      + ".choice");
        choiceChooser.onChange = [this, index] {
            if (refreshingFxParamControls || selectedFxParamSlot < 0)
                return;

            const int choice = mixerFxParamChoosers[index].getSelectedId() - 1;
            const std::vector<yesdaw::engine::FxInsert> chain = appModel.selectedStripFxChain();
            if (choice < 0 || static_cast<std::size_t> (selectedFxParamSlot) >= chain.size())
                return;

            const yesdaw::engine::ParamSpec spec = yesdaw::engine::fxParamSpecForKind (
                chain[static_cast<std::size_t> (selectedFxParamSlot)].kind,
                mixerFxParamSliderIds[index]);
            (void) appModel.setFxInsertParamOnSelectedStrip (
                static_cast<std::size_t> (selectedFxParamSlot),
                mixerFxParamSliderIds[index],
                yesdaw::engine::normalizedForChoice (spec, static_cast<std::uint8_t> (choice)));
            refreshActionState();
            repaintAll();
        };
        fxEditor.addChildComponent (choiceChooser);
    }

    // E15: params beyond one panel's worth page through this chooser.
    configureActionComponent (mixerFxParamPageChooser, yesdaw::ui::UiActionId::MixerFxInsertParamSet,
                              "FX parameter page");
    mixerFxParamPageChooser.setComponentID ("mixer.fx.param.page");
    mixerFxParamPageChooser.onChange = [this] {
        if (refreshingFxParamControls)
            return;

        const int page = mixerFxParamPageChooser.getSelectedId() - 1;
        if (page < 0 || page == selectedFxParamPage)
            return;

        selectedFxParamPage = page;
        refreshActionState();
        resized();
        repaintAll();
    };
    fxEditor.addChildComponent (mixerFxParamPageChooser);

    // G4.1 cp2: the lane's live fader and pan are gone — the painted fader rail and pan knob on
    // EVERY strip drag the same verbs (Shift fine, Alt-click resets, the Touch / Latch ride).
    dragDbReadout.setComponentID ("shell.drag.db");
    dragDbReadout.setTooltip ("Live gain in dB while dragging");
    dragDbReadout.setInterceptsMouseClicks (false, false);
    dragDbReadout.setJustificationType (juce::Justification::centred);
    dragDbReadout.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
    dragDbReadout.setColour (juce::Label::textColourId, kText);
    dragDbReadout.setColour (juce::Label::backgroundColourId,
                             yesdaw::ui::UiTheme::Color::darkControl());
    addChildComponent (dragDbReadout);

    // G4.1 cp2: the lane's M / S buttons are gone — the painted cells on every strip (N1).
    // G4.1: the seven readout rows ("Audio 1 meters: peak n/a" …), the solo-safe button and the
    // first-track select button are gone — the strip carries every one of those as a painted
    // section or a menu verb (plan §8.2: delete before you add). The read verbs stay registered
    // for the harness.
}

// G4.1 cp2: the FX editor opens on ONE slot of ONE strip (the double-click, the slot menu's Open
// Editor, the harness) and closes on Close / Escape / the strip or slot going away (the refresh law).
void MainComponent::openFxEditor (int stripIndex, int slotIndex)
{
    const auto surface = currentMixerSurface();
    const int trackCount = static_cast<int> (surface.tracks.size());
    const int busCount = static_cast<int> (surface.buses.size());
    if (stripIndex < 0 || stripIndex > trackCount + busCount || slotIndex < 0)
        return;
    if (stripIndex < trackCount)
    {
        (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripIndex));
        selectedTrackLane = stripIndex;
    }
    else if (stripIndex < trackCount + busCount)
        (void) appModel.selectMixerBus (static_cast<std::size_t> (stripIndex - trackCount));
    else
        (void) appModel.selectMixerMaster();   // the lane past the buses (R11: the master's chain)
    if (static_cast<std::size_t> (slotIndex) >= appModel.selectedStripFxChain().size())
        return;   // an empty slot has nothing to edit
    selectedFxParamSlot = slotIndex;
    selectedFxParamPage = 0;
    fxEditorStripOrdinal = appModel.selectedMixerStripOrdinal();
    fxEditorOpen = true;
    refreshActionState();
    resized();
    repaintAll();
}

void MainComponent::closeFxEditor()
{
    if (! fxEditorOpen)
        return;
    fxEditorOpen = false;
    refreshActionState();
    resized();
    repaintAll();
}

// G2.1 cp2: which editor tab the dock shows.
bool MainComponent::dockShowsMixer() const noexcept
{
    return appModel.context().mixerDockVisible
        && appModel.context().editorDockTab == yesdaw::ui::UiEditorDockTab::Mixer;
}

bool MainComponent::dockShowsInstrument() const noexcept   // G3.1
{
    return appModel.context().mixerDockVisible
        && appModel.context().editorDockTab == yesdaw::ui::UiEditorDockTab::Instrument;
}

// The mixer's control lane (every widget the mixer tab owns). While another tab shows, they
// are hidden as a set and restored to what their own laws last chose when the mixer returns:
// restore at the start of every refresh / layout, hide at the end. The one list is the law —
// the [dock-tabs] gate walks the dock rect and refuses any stray visible widget.
std::vector<juce::Component*> MainComponent::mixerLaneControls()
{
    // G4.1 cp2: the strips' input surface, the master fader and the FX editor (its rows are its
    // children, so hiding it hides them) — everything the mixer tab shows that another tab must not.
    return { &mixerStripsInput, &mixerMasterFader, &fxEditor };
}

void MainComponent::hideMixerControlsBehindDockTab()
{
    if (dockShowsMixer())
        return;
    for (juce::Component* control : mixerLaneControls())
    {
        hiddenByDockTab.try_emplace (control, control->isVisible());
        control->setVisible (false);
    }
}

// G3.9: a WAV onto a Sampler pad — the WAV reader's refusal is the shell's to name (R6), every
// other refusal the model's (R7).
void MainComponent::loadSamplerPadFromPath (std::int16_t key, const std::filesystem::path& path)
{
    auto decoded = decodeProjectWav (path);
    if (! decoded)
    {
        appModel.reportStatus ("Sampler pad refused (WAV only, stereo max): " + path.filename().string(), true);
        return;
    }
    if (appModel.importSamplerPadFromSource (path, std::move (*decoded), key).ok())
        recordLastAction (yesdaw::ui::UiActionId::SamplerPadLoad);
}

// G3.1: the instrument kind's name (the probe, the panel title, the inspector row).
const char* MainComponent::instrumentKindName (yesdaw::engine::TrackInstrumentKind kind) noexcept
{
    switch (kind)
    {
        case yesdaw::engine::TrackInstrumentKind::None: return "None (auto)";
        case yesdaw::engine::TrackInstrumentKind::SimpleSynth: return "SimpleSynth";
        case yesdaw::engine::TrackInstrumentKind::Sampler: return "Sampler";   // G3.9
    }
    return "?";
}

// G3.1: the panel's rows — one per ParamSpec of the selected Track's effective instrument,
// the value read back from the project (an unset id shows the spec default).
std::vector<InstrumentPanelComponent::Row> MainComponent::instrumentPanelRows() const
{
    std::vector<InstrumentPanelComponent::Row> rows;
    const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument();
    if (track == nullptr)
        return rows;
    for (std::uint32_t paramId = 1; paramId <= yesdaw::engine::SimpleSynthNode::kParameterCount; ++paramId)
    {
        if (! yesdaw::engine::instrumentKindAcceptsParameterId (track->instrumentKind, paramId))
            continue;
        const yesdaw::engine::ParamSpec spec = yesdaw::engine::instrumentParamSpecForKind (track->instrumentKind, paramId);
        InstrumentPanelComponent::Row row;
        row.paramId = paramId;
        row.label = juce::String (spec.name).fromLastOccurrenceOf (".", false, false).replaceCharacter ('_', ' ');
        row.normalized = track->instrumentParamNormalized (paramId);
        const double real = yesdaw::engine::mapNormalized (spec, row.normalized);
        row.readout = juce::String (real, real >= 100.0 ? 0 : 3) + (spec.unit[0] != '\0' ? juce::String (" ") + spec.unit : juce::String());
        rows.push_back (std::move (row));
    }
    return rows;
}

// The same editor and commit path as the rail's rename, placed over the mixer strip's name
// band (the bus editor's law) — the strip's track becomes the selected lane first, since
// commitTrackRenameEditor renames selectedTrackLane.
void MainComponent::openTrackRenameEditorOverStrip (int stripOrdinal)
{
    dismissClipRenameEditor();
    const auto& tracks = appModel.project().tracks;
    if (stripOrdinal < 0 || stripOrdinal >= static_cast<int> (tracks.size()))
        return;
    selectTrackLane (stripOrdinal);
    (void) appModel.selectMixerTrack (static_cast<std::size_t> (stripOrdinal));

    const juce::Rectangle<int> band =
        mixerStripBounds (stripOrdinal).removeFromTop (yesdaw::ui::UiTheme::Layout::mixerTrackSelectHeight);
    if (band.isEmpty())
        return;

    trackRenameEditor.setBounds (band);
    trackRenameEditor.setText (juce::String (tracks[static_cast<std::size_t> (stripOrdinal)].strip.name),
                               juce::dontSendNotification);
    trackRenameEditor.setVisible (true);
    trackRenameEditor.grabKeyboardFocus();
}

// E17: inline bus rename — the editor sits over the bus strip's header area.
void MainComponent::openBusRenameEditor (int busIndex, int stripOrdinal)
{
    const auto& buses = appModel.project().buses;
    if (busIndex < 0 || busIndex >= static_cast<int> (buses.size()))
        return;

    busRenameIndex = busIndex;
    busRenameEditor.setBounds (
        mixerStripBounds (stripOrdinal)
            .removeFromTop (yesdaw::ui::UiTheme::Layout::mixerTrackSelectHeight));
    busRenameEditor.setText (juce::String (buses[static_cast<std::size_t> (busIndex)].strip.name),
                             juce::dontSendNotification);
    busRenameEditor.setVisible (true);
    busRenameEditor.grabKeyboardFocus();
}

void MainComponent::commitBusRenameEditor()
{
    if (busRenameIndex >= 0)
        (void) appModel.renameBusAt (static_cast<std::size_t> (busRenameIndex),
                                     busRenameEditor.getText().toStdString());
    dismissBusRenameEditor();
    refreshActionState();
    repaintAll();
}

void MainComponent::dismissBusRenameEditor()
{
    busRenameIndex = -1;
    busRenameEditor.setVisible (false);
}

void MainComponent::advanceMeterHold (MeterHoldState& state, float livePeak)
{
    state.livePeak = livePeak;
    if (livePeak >= state.heldPeak || state.holdTicksRemaining <= 0)
    {
        state.heldPeak = livePeak;
        state.holdTicksRemaining = yesdaw::ui::UiTheme::Meter::peakHoldTicks;
    }
    else
    {
        --state.holdTicksRemaining;
    }

    if (livePeak >= yesdaw::ui::UiTheme::Meter::clipThreshold)
        state.clipLatched = true;
}

void MainComponent::updateTrackMeterHoldStates()
{
    // A stopped transport reads live silence: the MeterNode atomics keep the last processed
    // Block's peak, but a meter must fall when playback stops (the held peak still decays on
    // its own ~2 s law and the clip latch stays until clicked).
    const bool playing = appModel.context().isPlaying;
    const auto& tracks = appModel.project().tracks;
    trackMeterHold.resize (tracks.size());
    trackMeterHoldLR.resize (tracks.size());
    for (std::size_t i = 0; i < tracks.size(); ++i)
    {
        float peak = playing ? appModel.trackMeterPeak (tracks[i].id) : 0.0f;
        // V5: the rail meters L and R independently from the MeterNode's per-channel peaks;
        // the aggregate hold stays for the mixer strip's single-column meter.
        float peakL = playing ? appModel.trackMeterPeakChannel (tracks[i].id, 0) : 0.0f;
        float peakR = playing ? appModel.trackMeterPeakChannel (tracks[i].id, 1) : 0.0f;
        // E30: the ARMED track's rail meter also shows the live input peak, so signal is
        // visible before recording — playing or stopped. M11: each armed track shows its
        // OWN picked input, so a whole armed kit meters honestly. V5: the picked input is a
        // single pre-track signal, so it honestly lights both channels.
        if (appModel.isRecordingTrackIndexArmed (i))
        {
            const float inputPeak = appModel.inputMeterPeakForTrackIndex (i);
            peak = std::max (peak, inputPeak);
            peakL = std::max (peakL, inputPeak);
            peakR = std::max (peakR, inputPeak);
        }
        advanceMeterHold (trackMeterHold[i], peak);
        advanceMeterHold (trackMeterHoldLR[i][0], peakL);
        advanceMeterHold (trackMeterHoldLR[i][1], peakR);
    }

    // E22: bus meters live on exactly the same B32 law.
    const auto& buses = appModel.project().buses;
    busMeterHold.resize (buses.size());
    for (std::size_t i = 0; i < buses.size(); ++i)
        advanceMeterHold (busMeterHold[i],
                          playing ? appModel.busMeterPeak (buses[i].id) : 0.0f);
}

void MainComponent::clearTrackMeterHold (int trackIndex)
{
    if (trackIndex < 0 || trackIndex >= static_cast<int> (trackMeterHold.size()))
        return;

    MeterHoldState& state = trackMeterHold[static_cast<std::size_t> (trackIndex)];
    state.clipLatched = false;
    state.heldPeak = state.livePeak;
    state.holdTicksRemaining = 0;
    // V5: one click on the meter zone clears the L/R columns with the aggregate.
    if (trackIndex < static_cast<int> (trackMeterHoldLR.size()))
        for (MeterHoldState& channel : trackMeterHoldLR[static_cast<std::size_t> (trackIndex)])
        {
            channel.clipLatched = false;
            channel.heldPeak = channel.livePeak;
            channel.holdTicksRemaining = 0;
        }
    repaintAll();
}

// E22: a click on a painted BUS meter clears its hold and latch, like the track law.
void MainComponent::clearBusMeterHold (int busIndex)
{
    if (busIndex < 0 || busIndex >= static_cast<int> (busMeterHold.size()))
        return;

    MeterHoldState& state = busMeterHold[static_cast<std::size_t> (busIndex)];
    state.clipLatched = false;
    state.heldPeak = state.livePeak;
    state.holdTicksRemaining = 0;
    repaintAll();
}

// V3: the always-on bottom dock's height — 0 collapses it entirely, reclaiming the space for
// the timeline/rail/inspector, when the user has toggled it off (the full-view Mixer panel
// is unaffected: it never reserves this space in the first place). ONE law shared by every
// layout function below, so paint and every interactive component's bounds can never drift.
int MainComponent::dockedMixerHeight() const
{
    if (! appModel.context().mixerDockVisible)
        return 0;
    // G2.1: the dragged height, never eating the arrangement below its minimum.
    const int maxDock = getHeight() - headerHeightNow() - yesdaw::ui::UiTheme::Layout::arrangeMinHeight;
    return juce::jlimit (yesdaw::ui::UiTheme::Layout::editorDockMinHeight,
                         juce::jmax (yesdaw::ui::UiTheme::Layout::editorDockMinHeight, maxDock),
                         viewState.dockHeight);
}

juce::Rectangle<int> MainComponent::mixerPanelBounds() const
{
    auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
    auto mixer = work.removeFromBottom (dockedMixerHeight());   // G2.1 cp2: always the dock
    return mixer.reduced (yesdaw::ui::UiTheme::Layout::mixerPanelHorizontalInset,
                          yesdaw::ui::UiTheme::Layout::mixerPanelVerticalInset);
}

// E25: ONE strip geometry — the interactive control lane, the click law, and the paint all
// share the painted-strip law (the old width/(count+1) law visibly diverged from the
// painted lanes at real window sizes, floating the control lane off its strip).
juce::Rectangle<int> MainComponent::mixerStripBounds (int stripIndex) const
{
    return paintedMixerLaneBounds (static_cast<std::size_t> (juce::jmax (0, stripIndex)));
}

// N3: the painted MASTER pane rect. Master is lane index stripCount in the SAME
// paintedMixerLaneBounds law every track/bus strip uses — it is the strip immediately after
// the last one, never a detached island computed from the far right of a stale area. Before
// N3 this peeled its slice off the right edge of the FULL panel independently of how many
// strips were drawn from the left, so a clamped strip width (max 112px) left ~1250px of dead
// black between the last strip and master at 1920x1080.
juce::Rectangle<int> MainComponent::paintedMixerMasterBounds() const
{
    const auto surface = currentMixerSurface();
    const std::size_t stripCount = surface.tracks.size() + surface.buses.size();
    return paintedMixerLaneBounds (stripCount);
}

// Shared painted-strip geometry law (B32): hit-testing must mirror drawMixer's lane math
// exactly so a meter click can never drift from the painted meter.
juce::Rectangle<int> MainComponent::paintedMixerLaneBounds (std::size_t stripIndex) const
{
    const auto area = mixerPanelBounds();   // G4.1 cp2: no tools column — the strips start at the panel's edge

    const auto surface = currentMixerSurface();
    const std::size_t stripCount = surface.tracks.size() + surface.buses.size();
    // G4.1: View > Narrow Strips is ONE fixed width for every lane (the master's too).
    const int stripWidth = appModel.context().mixerStripsNarrow
        ? yesdaw::ui::UiTheme::Layout::mixerPaintedStripNarrowWidth
        : std::clamp (
            area.getWidth() / (juce::jmax (yesdaw::ui::UiTheme::Layout::mixerPaintedStripMinCount,
                                           static_cast<int> (stripCount))
                               + yesdaw::ui::UiTheme::Layout::mixerPaintedStripExtraSlotCount),
            yesdaw::ui::UiTheme::Layout::mixerPaintedStripMinWidth,
            yesdaw::ui::UiTheme::Layout::mixerPaintedStripMaxWidth);
    return juce::Rectangle<int> (area.getX() + static_cast<int> (stripIndex) * stripWidth,
                                 area.getY(),
                                 stripWidth,
                                 area.getHeight())
               .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetX,
                         yesdaw::ui::UiTheme::Layout::mixerPaintedStripInsetY);
}

// M4: how many insert rows this strip can afford. A tall mixer-view strip shows the whole
// column; the timeline view's short mini-mixer drops rows rather than starving the fader, and a
// strip with no room at all falls back to the exact historical fader top.
int MainComponent::paintedInsertRowCountForLane (juce::Rectangle<int> lane) noexcept
{
    using L = yesdaw::ui::UiTheme::Layout;
    const int available = lane.getHeight() - L::mixerPaintedInsertsTop
                        - L::mixerPaintedFaderBottomInset - L::mixerPaintedFaderMinHeight
                        - L::mixerPaintedInsertsFaderGap;
    return std::clamp (available / L::mixerPaintedInsertRowPitch, 0, L::mixerPaintedInsertRowCount);
}

// G4.1: how many I/O slot rows a strip's KIND carries — a Track two (input and output), a Bus one
// (output), the master none — and how many cells its S / M / R row has.
int MainComponent::stripIoRows (std::size_t stripIndex) const noexcept
{
    if (! appModel.context().projectLoaded)
        return 0;
    const auto& project = appModel.project();
    if (stripIndex < project.tracks.size())
        return 2;
    if (stripIndex < project.tracks.size() + project.buses.size())
        return 1;
    return 0;
}

std::size_t MainComponent::stripCellCount (std::size_t stripIndex) const noexcept
{
    if (! appModel.context().projectLoaded)
        return 0;
    const auto& project = appModel.project();
    if (stripIndex < project.tracks.size())
        return kMixerPaintedTrackCellCount;
    if (stripIndex < project.tracks.size() + project.buses.size())
        return kMixerPaintedMuteSoloCellCount;
    return 0;
}

// G4.1: how many of the strip's I/O rows the lane can afford — after the inserts, before the sends.
int MainComponent::paintedIoRowsShownForLane (juce::Rectangle<int> lane, int ioRows) noexcept
{
    using L = yesdaw::ui::UiTheme::Layout;
    const int available = lane.getHeight() - L::mixerPaintedInsertsTop
                        - L::mixerPaintedFaderBottomInset - L::mixerPaintedFaderMinHeight
                        - L::mixerPaintedInsertsFaderGap
                        - paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch;
    return available >= ioRows * L::mixerPaintedIoRowPitch ? ioRows : 0;
}

// The input row (a Track's) leads the slot column; the output row closes it.
int MainComponent::paintedInputRowsForLane (juce::Rectangle<int> lane, int ioRows) noexcept
{
    return paintedIoRowsShownForLane (lane, ioRows) == 2 ? 1 : 0;
}

juce::Rectangle<int> MainComponent::paintedIoRowRect (juce::Rectangle<int> lane, int top)
{
    using L = yesdaw::ui::UiTheme::Layout;
    return juce::Rectangle<int> (lane.getX() + L::mixerPaintedInsertsInsetX,
                                 top,
                                 juce::jmax (0, lane.getWidth() - 2 * L::mixerPaintedInsertsInsetX),
                                 L::mixerPaintedIoRowHeight);
}

juce::Rectangle<int> MainComponent::paintedInputRowBoundsForLane (juce::Rectangle<int> lane, int ioRows)
{
    if (paintedInputRowsForLane (lane, ioRows) == 0)
        return {};
    return paintedIoRowRect (lane, lane.getY() + yesdaw::ui::UiTheme::Layout::mixerPaintedInsertsTop);
}

juce::Rectangle<int> MainComponent::paintedOutputRowBoundsForLane (juce::Rectangle<int> lane, int ioRows)
{
    using L = yesdaw::ui::UiTheme::Layout;
    if (paintedIoRowsShownForLane (lane, ioRows) == 0)
        return {};
    const int top = lane.getY() + paintedSendsTopForLane (lane, ioRows)
                  + paintedSendRowCountForLane (lane, ioRows) * L::mixerPaintedSendRowPitch;
    return paintedIoRowRect (lane, top);
}

// M6: ONE fader mapping. The sliders travel 0..mixerFaderSliderMax in LINEAR gain, so the
// painted thumb, the unity mark and every dB tick must read the same law — before M6 the paint
// put unity at the TOP of the rail while the live slider put it at half travel.
float MainComponent::mixerFaderFractionForGain (float linearGain) noexcept
{
    const float span = static_cast<float> (yesdaw::ui::UiTheme::Layout::mixerFaderSliderMax);
    return span > 0.0f ? std::clamp (linearGain / span, 0.0f, 1.0f) : 0.0f;
}

float MainComponent::mixerFaderFractionForDb (float db) noexcept
{
    return mixerFaderFractionForGain (std::pow (10.0f, db / 20.0f));
}

juce::Rectangle<int> MainComponent::paintedFaderRailForLane (juce::Rectangle<int> lane, int ioRows)
{
    const auto faderArea = lane.withTrimmedTop (paintedFaderTopForLane (lane, ioRows))
                               .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderBottomInset);
    return faderArea.withWidth (yesdaw::ui::UiTheme::Layout::mixerPaintedRailWidth)
                    .withCentre ({ lane.getCentreX()
                                       - yesdaw::ui::UiTheme::Layout::mixerPaintedRailCenterOffsetX,
                                   faderArea.getCentreY() });
}

// The painted pan knob's disc for a strip lane — the SAME rect the strip paint fills, so the
// drag hit-test and the picture cannot drift.
juce::Rectangle<int> MainComponent::paintedPanKnobForLane (juce::Rectangle<int> lane)
{
    using L = yesdaw::ui::UiTheme::Layout;
    const auto knob = lane.withTrimmedTop (L::mixerPaintedPanTop).withHeight (L::mixerPaintedPanHeight);
    return juce::Rectangle<int> (knob.getCentreX() - L::mixerPaintedPanRadius,
                                 knob.getY() + L::mixerPaintedPanTopInset,
                                 L::mixerPaintedPanRadius * 2, L::mixerPaintedPanRadius * 2);
}

// The painted fader THUMB for a strip lane at a gain — the grab target (the rail itself stays a
// strip-select click: it overlaps the strip's centre line, which every strip click lands on).
juce::Rectangle<int> MainComponent::paintedFaderThumbForLane (juce::Rectangle<int> lane, float linearGain, int ioRows)
{
    using L = yesdaw::ui::UiTheme::Layout;
    const auto rail = paintedFaderRailForLane (lane, ioRows);
    if (rail.isEmpty())
        return {};
    return juce::Rectangle<int> (rail.getX() - L::mixerPaintedThumbWidthOverhang / 2,
                                 mixerFaderThumbYForGain (rail, linearGain) - L::mixerPaintedThumbCenterInset,
                                 rail.getWidth() + L::mixerPaintedThumbWidthOverhang,
                                 L::mixerPaintedThumbHeight);
}

int MainComponent::mixerFaderThumbYForGain (juce::Rectangle<int> rail, float linearGain) noexcept
{
    return rail.getBottom()
         - juce::roundToInt (mixerFaderFractionForGain (linearGain) * static_cast<float> (rail.getHeight()));
}

// M5: the send rows follow the inserts, and take space only after the inserts have taken
// theirs — a short strip drops sends first, then inserts, and never starves the fader.
int MainComponent::paintedSendRowCountForLane (juce::Rectangle<int> lane, int ioRows) noexcept
{
    using L = yesdaw::ui::UiTheme::Layout;
    const int used = paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch
                   + paintedIoRowsShownForLane (lane, ioRows) * L::mixerPaintedIoRowPitch;   // G4.1
    const int available = lane.getHeight() - L::mixerPaintedInsertsTop - used
                        - L::mixerPaintedFaderBottomInset - L::mixerPaintedFaderMinHeight
                        - L::mixerPaintedInsertsFaderGap;
    return std::clamp (available / L::mixerPaintedSendRowPitch, 0, L::mixerPaintedSendRowCount);
}

int MainComponent::paintedSendsTopForLane (juce::Rectangle<int> lane, int ioRows) noexcept
{
    using L = yesdaw::ui::UiTheme::Layout;
    return L::mixerPaintedInsertsTop
         + paintedInputRowsForLane (lane, ioRows) * L::mixerPaintedIoRowPitch   // G4.1
         + paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch;
}

juce::Rectangle<int> MainComponent::paintedSendRowBoundsForLane (juce::Rectangle<int> lane,
                                                                       std::size_t sendIndex,
                                                                       int ioRows)
{
    using L = yesdaw::ui::UiTheme::Layout;
    if (static_cast<int> (sendIndex) >= paintedSendRowCountForLane (lane, ioRows))
        return {};

    const int top = lane.getY() + paintedSendsTopForLane (lane, ioRows)
                  + static_cast<int> (sendIndex) * L::mixerPaintedSendRowPitch;
    return juce::Rectangle<int> (lane.getX() + L::mixerPaintedInsertsInsetX,
                                 top,
                                 juce::jmax (0, lane.getWidth() - 2 * L::mixerPaintedInsertsInsetX),
                                 L::mixerPaintedSendRowHeight);
}

int MainComponent::paintedFaderTopForLane (juce::Rectangle<int> lane, int ioRows) noexcept
{
    using L = yesdaw::ui::UiTheme::Layout;
    const int rows = paintedInsertRowCountForLane (lane) * L::mixerPaintedInsertRowPitch
                   + paintedIoRowsShownForLane (lane, ioRows) * L::mixerPaintedIoRowPitch   // G4.1
                   + paintedSendRowCountForLane (lane, ioRows) * L::mixerPaintedSendRowPitch;
    return rows == 0
        ? L::mixerPaintedFaderTop
        : L::mixerPaintedFaderTop + rows + L::mixerPaintedInsertsFaderGap;
}

// N1: ONE Mute/Solo cell law — the paint, the click hit-test, the SELECTED strip's live
// buttons and the gates all read it, so the control you see is exactly the control you hit,
// on every strip. Cell 0 is Solo, cell 1 is Mute (left to right, as painted).
// G4.1: the row is S / M / R on a Track strip (cellCount 3), S / M on a Bus (2); a narrow strip
// shrinks the cells so they still sit side by side inside the lane.
juce::Rectangle<int> MainComponent::paintedMuteSoloCellBoundsForLane (juce::Rectangle<int> lane,
                                                                     std::size_t cellIndex,
                                                                     std::size_t cellCount) const
{
    using L = yesdaw::ui::UiTheme::Layout;
    if (cellIndex >= cellCount)
        return {};

    const bool narrow = appModel.context().mixerStripsNarrow;
    const int cellWidth = narrow ? L::mixerPaintedButtonNarrowWidth : L::mixerPaintedButtonWidth;
    auto buttonsRow = lane.withTrimmedTop (L::mixerPaintedButtonsTop)
                          .withHeight (L::mixerPaintedButtonsHeight)
                          .reduced (narrow ? L::mixerPaintedButtonsNarrowInsetX : L::mixerPaintedButtonsInsetX,
                                    L::mixerPaintedButtonsInsetY);
    buttonsRow.removeFromLeft (static_cast<int> (cellIndex) * cellWidth);
    return buttonsRow.removeFromLeft (cellWidth)
                     .reduced (L::mixerPaintedButtonInsetX, L::mixerPaintedButtonInsetY);
}

// M4: ONE insert-slot row law — the paint, the click hit-test and the gates all read it, so a
// painted slot can never drift from the slot a click selects. An empty rect means the strip has
// no room for that row.
juce::Rectangle<int> MainComponent::paintedInsertRowBoundsForLane (juce::Rectangle<int> lane,
                                                                         std::size_t slotIndex,
                                                                         int ioRows)
{
    using L = yesdaw::ui::UiTheme::Layout;
    if (static_cast<int> (slotIndex) >= paintedInsertRowCountForLane (lane))
        return {};

    const int top = lane.getY() + L::mixerPaintedInsertsTop
                  + paintedInputRowsForLane (lane, ioRows) * L::mixerPaintedIoRowPitch   // G4.1: below the input row
                  + static_cast<int> (slotIndex) * L::mixerPaintedInsertRowPitch;
    return juce::Rectangle<int> (lane.getX() + L::mixerPaintedInsertsInsetX,
                                 top,
                                 juce::jmax (0, lane.getWidth() - 2 * L::mixerPaintedInsertsInsetX),
                                 L::mixerPaintedInsertRowHeight);
}

juce::Rectangle<int> MainComponent::paintedMeterBoundsForLane (juce::Rectangle<int> lane, int ioRows)
{
    auto faderArea = lane.withTrimmedTop (paintedFaderTopForLane (lane, ioRows))
                         .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::mixerPaintedFaderBottomInset);
    return faderArea.removeFromRight (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterWidth)
                    .reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetX,
                              yesdaw::ui::UiTheme::Layout::mixerPaintedMeterInsetY);
}

void MainComponent::layoutMixerControls()
{
    // G4.1 cp2: the lane is gone. What is left to lay out live: the FX editor's parameter rows
    // (inside the editor's content area — the pager, then label + slider / chooser per row) and
    // the master pane's fader.
    {
        using L = yesdaw::ui::UiTheme::Layout;
        auto content = fxEditor.contentArea();
        if (mixerFxParamPageChooser.isVisible())
        {
            mixerFxParamPageChooser.setBounds (content.removeFromTop (L::mixerFxParamRowHeight));
            content.removeFromTop (L::mixerFxParamRowGap);
        }
        else
            mixerFxParamPageChooser.setBounds ({});
        for (std::size_t index = 0; index < mixerFxParamSliders.size(); ++index)
        {
            if (! mixerFxParamLabels[index].isVisible() || content.getHeight() < L::mixerFxParamRowHeight)
            {
                mixerFxParamSliders[index].setBounds ({});
                mixerFxParamChoosers[index].setBounds ({});
                mixerFxParamLabels[index].setBounds ({});
                continue;
            }
            auto paramRow = content.removeFromTop (L::mixerFxParamRowHeight);
            mixerFxParamLabels[index].setBounds (paramRow.removeFromLeft (L::mixerFxParamLabelWidth));
            if (mixerFxParamChoosers[index].isVisible())
            {
                mixerFxParamChoosers[index].setBounds (paramRow);
                mixerFxParamSliders[index].setBounds ({});
            }
            else
            {
                mixerFxParamSliders[index].setBounds (paramRow);
                mixerFxParamChoosers[index].setBounds ({});
            }
            content.removeFromTop (L::mixerFxParamRowGap);
        }
    }

    // E19/E25: the master fader lives on the PAINTED MASTER pane, inside its METER region —
    // the same walk drawMixer uses (content top, loudness card, gap, peak card, meter gap) —
    // so the fader rail can never cross the INTEGRATED / TRUE PEAK cards and the thumb
    // travels the same vertical span as the painted dB scale.
    auto masterContent = paintedMixerMasterBounds()
                             .reduced (yesdaw::ui::UiTheme::Layout::mixerMasterContentInsetX,
                                       yesdaw::ui::UiTheme::Space::none);
    masterContent.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerMasterContentTop
                                 + yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessCardHeight
                                 + yesdaw::ui::UiTheme::Layout::mixerMasterSectionGap
                                 + yesdaw::ui::UiTheme::Layout::mixerMasterPeakCardHeight
                                 + yesdaw::ui::UiTheme::Layout::mixerMasterMeterTopGap);
    auto masterFaderArea = masterContent.withTrimmedBottom (
        yesdaw::ui::UiTheme::Layout::mixerMasterMeterBottomInset);
    masterFaderArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerMasterScaleWidth);
    const int masterMeterPairWidth = 2 * yesdaw::ui::UiTheme::Layout::mixerMasterMeterWidth
                                   + yesdaw::ui::UiTheme::Layout::mixerMasterMeterGap;
    auto masterFaderColumn = masterFaderArea.withTrimmedRight (
        masterFaderArea.getWidth() / 2 + masterMeterPairWidth / 2);
    mixerMasterFader.setBounds (
        masterFaderColumn.withWidth (yesdaw::ui::UiTheme::Layout::mixerFaderWidth)
            .withCentre ({ masterFaderColumn.getCentreX(), masterFaderColumn.getCentreY() }));
}

// N5: normalized [0,1] breakpoint value for a live linear-gain fader read, matching
// FaderNode::linearGainForNormalizedEvent's dB-range mapping exactly (its inverse) — so a
// point recorded here plays back at the SAME gain the fader was actually at.
double MainComponent::automationNormalizedForFaderGain (double linearGain) noexcept
{
    const double gainDb = linearGain > 0.0
        ? 20.0 * std::log10 (linearGain)
        : yesdaw::engine::FaderNode::kMinGainDb;
    return yesdaw::engine::unmapToNormalized (
        yesdaw::engine::FaderNode::parameterSpec (yesdaw::engine::FaderNode::kGainParameterId),
        gainDb);
}

// N5: normalized [0,1] breakpoint value for a live pan read, the exact inverse of
// PanNode::panForNormalizedEvent (-1..1 maps linearly to 0..1).
double MainComponent::automationNormalizedForPan (double pan) noexcept
{
    return std::clamp ((pan + 1.0) / 2.0, 0.0, 1.0);
}

void MainComponent::refreshMixerControls()
{
    // G4.1 cp2: the lane's live fader / pan / M / S are gone; the master fader is the one live
    // strip control left, on the master pane.
    refreshingMixerControls = true;
    // E19: the master fader reflects the persisted master gain and enables with a project.
    mixerMasterFader.setEnabled (
        appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerMasterSetFader,
                                      appModel.context()).enabled);
    mixerMasterFader.setValue (appModel.context().projectLoaded
                                   ? static_cast<double> (appModel.project().masterLinearGain)
                                   : yesdaw::ui::UiTheme::Layout::mixerFaderSliderDefault,
                               juce::dontSendNotification);
    refreshingMixerControls = false;
}

// G4.1: the I/O slots' texts — ONE law for the paint and the harness.
juce::String MainComponent::stripInputText (std::size_t trackIndex) const
{
    for (const yesdaw::ui::UiRecordingTrackInputSelection& armed : appModel.armedRecordingTrackInputs())
        if (armed.armed && armed.trackIndex == trackIndex)
            return "In: " + juce::String (static_cast<int> (armed.inputChannel) + 1)
                 + (armed.stereoPair ? "+" + juce::String (static_cast<int> (armed.inputChannel) + 2) : juce::String());
    return juce::String::fromUTF8 ("In: \xe2\x80\x94");
}

juce::String MainComponent::stripOutputText (const yesdaw::ui::UiMixerStrip& strip) const
{
    if (strip.outputBusId.isValid())
        if (const yesdaw::engine::Bus* const bus = appModel.project().findBus (strip.outputBusId))
            return "Out: " + juce::String (bus->strip.name);
    return "Out: Master";
}

const char* MainComponent::fxKindStripName (yesdaw::engine::FxKind kind) noexcept
{
    switch (kind)
    {
        case yesdaw::engine::FxKind::Eq: return "EQ";
        case yesdaw::engine::FxKind::Compressor: return "Comp";
        case yesdaw::engine::FxKind::Delay: return "Delay";
        case yesdaw::engine::FxKind::Reverb: return "Reverb";
        case yesdaw::engine::FxKind::Limiter: return "Limiter";
        case yesdaw::engine::FxKind::MidiTranspose: return "Transpose";   // G3.8
        case yesdaw::engine::FxKind::MidiScaleMap: return "Scale";
        case yesdaw::engine::FxKind::MidiArpeggiator: return "Arp";
        case yesdaw::engine::FxKind::MidiChord: return "Chord";
    }

    return "FX";
}

const char* MainComponent::fxKindName (yesdaw::engine::FxKind kind) noexcept
{
    switch (kind)
    {
        case yesdaw::engine::FxKind::Eq: return "EQ";
        case yesdaw::engine::FxKind::Compressor: return "Compressor";
        case yesdaw::engine::FxKind::Delay: return "Delay";
        case yesdaw::engine::FxKind::Reverb: return "Reverb";
        case yesdaw::engine::FxKind::Limiter: return "Limiter";
        case yesdaw::engine::FxKind::MidiTranspose: return "MIDI Transpose";   // G3.8
        case yesdaw::engine::FxKind::MidiScaleMap: return "MIDI Scale";
        case yesdaw::engine::FxKind::MidiArpeggiator: return "Arpeggiator";
        case yesdaw::engine::FxKind::MidiChord: return "Chord Trigger";
    }

    return "Unknown";
}

juce::String MainComponent::masterLoudnessReadoutText() const
{
    const auto surface = currentMixerSurface();
    if (! surface.loudness.valid)
        return "-- LUFS";

    return juce::String (surface.loudness.integratedLufs, 1) + " LUFS";
}

MainComponent::HeadTempoMeter MainComponent::headTempoMeter() const
{
    HeadTempoMeter head;
    if (! appModel.project().tempoMap.empty())
        head.bpm = appModel.project().tempoMap.front().bpm;
    if (! appModel.project().meterMap.empty())
    {
        head.numerator = appModel.project().meterMap.front().numerator;
        head.denominator = appModel.project().meterMap.front().denominator;
    }
    return head;
}

// M9: the header's master card — right-anchored against the gear, drops WHOLE (empty rect)
// when it cannot keep its minimum width next to the centred transport group.
juce::Rectangle<int> MainComponent::headerMasterCardBounds() const
{
    return headerLayout().masterCard;
}

juce::Rectangle<int> MainComponent::headerMasterLufsBounds() const
{
    using L = yesdaw::ui::UiTheme::Layout;
    const auto card = headerMasterCardBounds();
    if (card.isEmpty())
        return {};

    return juce::Rectangle<int> (card.getRight() - L::headerMasterLufsWidth,
                                 L::headerMasterLufsY,
                                 L::headerMasterLufsWidth,
                                 L::headerMasterLufsHeight);
}

void MainComponent::drawMasterMeter (juce::Graphics& g) const
{
    auto master = headerMasterCardBounds();
    if (master.isEmpty())
    {
        // The card is gone; the gear still belongs to the window edge.
        yesdaw::ui::drawSettingsIcon (
            g,
            headerLayout().gear.toFloat(),
            appModel.context().settingsRowVisible ? kText : kMutedText);
        return;
    }

    drawSmallLabel (g, "MASTER", master.removeFromTop (yesdaw::ui::UiTheme::Layout::headerMasterLabelHeight));
    const int meterWidth = juce::jmin (yesdaw::ui::UiTheme::Layout::headerMasterMeterWidth,
                                       master.getWidth()
                                           - yesdaw::ui::UiTheme::Layout::headerMasterLufsWidth
                                           - yesdaw::ui::UiTheme::Layout::headerMasterLufsGap);
    auto meter = master.removeFromTop (yesdaw::ui::UiTheme::Layout::headerMasterMeterHeight)
                     .withWidth (juce::jmax (yesdaw::ui::UiTheme::Layout::headerMasterLufsGap, meterWidth));
    drawHorizontalMeter (g, meter, liveMasterPeakLeft.load (std::memory_order_acquire));

    yesdaw::ui::drawSettingsIcon (
        g,
        headerLayout().gear.toFloat(),
        appModel.context().settingsRowVisible ? kText : kMutedText);
}

void MainComponent::drawMixer (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const auto surface = currentMixerSurface();
    const std::size_t stripCount = surface.tracks.size() + surface.buses.size();

    g.setColour (yesdaw::ui::UiTheme::Color::mixerBack());
    g.fillRect (area);

    // G4.1 cp2: no "MIXER" column — the strips are the mixer (plan §8.2: delete before you add).

    for (std::size_t stripIndex = 0; stripIndex < stripCount; ++stripIndex)
    {
        const bool isBus = stripIndex >= surface.tracks.size();
        const int ioRows = stripIoRows (stripIndex);   // G4.1
        const auto& state = isBus ? surface.buses[stripIndex - surface.tracks.size()]
                                  : surface.tracks[stripIndex];
        // N7: a Track strip's own persisted colour overrides the historical index-cycled
        // palette; a Bus strip (which carries no colour field) always keeps it.
        const juce::Colour stripColour = state.colour != yesdaw::engine::kTrackColourUnset
                                              ? juce::Colour (state.colour)
                                              : stripColourForIndex (stripIndex);
        // E23: the selected highlight keys on the E16 strip ordinal, so a selected BUS
        // strip highlights exactly like a selected track.
        const int selectedOrdinal = appModel.selectedMixerStripOrdinal();
        const bool selected = appModel.context().mixerTargetSelected
                           && selectedOrdinal >= 0
                           && stripIndex == static_cast<std::size_t> (selectedOrdinal);
        // G4.1 cp2: EVERY strip paints its pan knob and fader rail — the selected strip used to leave
        // those to the lane's live pan / fader, which are gone (the painted drags are the controls).

        // G4.1: the lane from the ONE geometry law (paintedMixerLaneBounds) — the paint, the
        // hit-tests and the harness can never disagree on where a strip is (narrow or wide).
        auto lane = paintedMixerLaneBounds (stripIndex);
        g.setColour (yesdaw::ui::UiTheme::Color::panelShadow().withAlpha (
            yesdaw::ui::UiTheme::Tone::shadowAlpha));
        g.fillRoundedRectangle (
            lane.toFloat().translated (
                0.0f,
                static_cast<float> (yesdaw::ui::UiTheme::Layout::controlShadowOffset)),
            yesdaw::ui::UiTheme::Radius::panel);
        juce::ColourGradient laneGradient (
            selected ? yesdaw::ui::UiTheme::Color::selectedStrip()
                     : yesdaw::ui::UiTheme::Color::panelRaised(),
            static_cast<float> (lane.getCentreX()),
            static_cast<float> (lane.getY()),
            yesdaw::ui::UiTheme::Color::panel(),
            static_cast<float> (lane.getCentreX()),
            static_cast<float> (lane.getBottom()),
            false);
        g.setGradientFill (laneGradient);
        g.fillRoundedRectangle (lane.toFloat(), yesdaw::ui::UiTheme::Radius::panel);
        g.setColour (selected ? kPurple : kPanelStroke);
        g.drawRoundedRectangle (lane.toFloat().reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                yesdaw::ui::UiTheme::Radius::panel,
                                selected
                                    ? yesdaw::ui::UiTheme::Layout::mixerPaintedStripSelectedStrokeWidth
                                    : yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);

        g.setColour (stripColour.withAlpha (yesdaw::ui::UiTheme::Tone::mixerHeaderAlpha));
        g.fillRect (lane.withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedHeaderHeight));
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::small,
            juce::Font::bold));
        g.drawFittedText (state.name,
                          lane.reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedNameInsetX,
                                        yesdaw::ui::UiTheme::Layout::mixerPaintedNameInsetY)
                              .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedNameHeight),
                          juce::Justification::centred,
                          1);

        // (every strip, the selected one included — G4.1 cp2)
        {
            const juce::Rectangle<int> panDisc = paintedPanKnobForLane (lane);
            const int panDiameter = panDisc.getWidth();
            const int panX = panDisc.getX();
            const int panY = panDisc.getY();
            g.setColour (yesdaw::ui::UiTheme::Color::panelShadow().withAlpha (
                yesdaw::ui::UiTheme::Tone::shadowAlpha));
            g.fillEllipse (static_cast<float> (panX),
                           static_cast<float> (panY + yesdaw::ui::UiTheme::Layout::controlShadowOffset),
                           static_cast<float> (panDiameter),
                           static_cast<float> (panDiameter));
            g.setColour (yesdaw::ui::UiTheme::Color::knobFace());
            g.fillEllipse (static_cast<float> (panX),
                           static_cast<float> (panY),
                           static_cast<float> (panDiameter),
                           static_cast<float> (panDiameter));
            g.setColour (stripColour.withAlpha (
                yesdaw::ui::UiTheme::Tone::mixerKnobHighlightAlpha));
            g.drawEllipse (static_cast<float> (panX),
                           static_cast<float> (panY),
                           static_cast<float> (panDiameter),
                           static_cast<float> (panDiameter),
                           yesdaw::ui::UiTheme::Layout::mixerPaintedPanStrokeWidth);
            const float panAngle = juce::MathConstants<float>::pi
                                 * (0.5f + state.pan * 0.35f);
            const float panCentreX = static_cast<float> (
                panX + yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius);
            const float panCentreY = static_cast<float> (
                panY + yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius);
            const float panIndicatorRadius = static_cast<float> (
                yesdaw::ui::UiTheme::Layout::mixerPaintedPanRadius
                - yesdaw::ui::UiTheme::Layout::trackListPanIndicatorInset);
            g.drawLine (panCentreX,
                        panCentreY,
                        panCentreX + std::cos (panAngle) * panIndicatorRadius,
                        panCentreY - std::sin (panAngle) * panIndicatorRadius,
                        yesdaw::ui::UiTheme::Layout::iconBoldStrokeWidth);
        }

        // N1: EVERY strip paints its Mute/Solo cells — the selected strip used to skip them
        // and show two mis-typed ToggleButtons instead, so the strip you were working on was
        // the one whose controls looked broken. The selected strip's live buttons now sit on
        // exactly these rects (paintedMuteSoloCellBoundsForLane), so the two agree by law.
        {
            // G4.1: S / M / R on a Track strip (the R cell lit while the track is in the arm set),
            // S / M on a Bus — the cell count is the strip kind's.
            const std::array<const char*, kMixerPaintedTrackCellCount> cellLabels { "S", "M", "R" };
            const std::size_t cellCount = stripCellCount (stripIndex);
            for (std::size_t cellIndex = 0; cellIndex < cellCount && cellIndex < cellLabels.size(); ++cellIndex)
            {
                const auto cell = paintedMuteSoloCellBoundsForLane (lane, cellIndex, cellCount);
                if (cell.isEmpty())
                    continue;

                const bool armCell = cellIndex == kMixerPaintedTrackCellCount - 1;
                const bool on = cellIndex == 0 ? state.soloed
                              : cellIndex == 1 ? state.muted
                                               : (! isBus && appModel.isRecordingTrackIndexArmed (stripIndex));
                g.setColour (armCell && on ? yesdaw::ui::UiTheme::Color::recordArm()
                                           : yesdaw::ui::UiTheme::Color::controlInset());
                g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::md);
                g.setColour (on && ! armCell ? stripColour.brighter (0.55f) : kText);
                g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small, juce::Font::bold));
                g.drawText (cellLabels[cellIndex], cell, juce::Justification::centred, false);
            }
        }

        if (state.sidechainVisible)
        {
            auto badge = lane.withTrimmedTop (yesdaw::ui::UiTheme::Layout::mixerPaintedSidechainTop)
                             .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedSidechainHeight)
                             .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::mixerPaintedSidechainLeftInset)
                             .withWidth (yesdaw::ui::UiTheme::Layout::mixerPaintedSidechainWidth);
            g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
            g.fillRoundedRectangle (badge.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
            g.setColour (kMutedText);
            g.setFont (yesdaw::ui::UiTheme::Type::font (
                yesdaw::ui::UiTheme::Type::tiny,
                juce::Font::bold));
            g.drawText ("SC", badge, juce::Justification::centred, false);
        }

        // M4: the strip's FX chain, ON the strip. One row per slot: the insert's name, a
        // bypass dot when it is disabled, and an empty well when the chain is shorter. The
        // selected slot of the selected strip reads as selected — clicking a row opens exactly
        // these params in the panel (shared law: paintedInsertRowBoundsForLane).
        {
            const std::vector<yesdaw::ui::UiMixerFxSlotReadout>& chain = state.fxSlots;
            for (std::size_t slot = 0;
                 slot < static_cast<std::size_t> (paintedInsertRowCountForLane (lane));
                 ++slot)
            {
                const auto row = paintedInsertRowBoundsForLane (lane, slot, ioRows);
                const bool filled = slot < chain.size();
                const bool slotSelected = selected && selectedFxParamSlot >= 0
                                       && static_cast<std::size_t> (selectedFxParamSlot) == slot;
                g.setColour (filled ? yesdaw::ui::UiTheme::Color::darkControl()
                                    : yesdaw::ui::UiTheme::Color::controlInset());
                g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                // An empty slot is a visible WELL, not a smudge — a mixer strip should read as
                // "four inserts, none used", the way every DAW draws it.
                g.setColour (kPanelStroke);
                g.drawRoundedRectangle (row.toFloat().reduced (
                                            yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                        yesdaw::ui::UiTheme::Radius::sm,
                                        yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);
                if (slotSelected)
                {
                    g.setColour (kPurple);
                    g.drawRoundedRectangle (row.toFloat().reduced (
                                                yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                            yesdaw::ui::UiTheme::Radius::sm,
                                            yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);
                }

                if (! filled)
                    continue;

                const yesdaw::ui::UiMixerFxSlotReadout& insert = chain[slot];
                auto dot = juce::Rectangle<int> (
                    row.getX() + yesdaw::ui::UiTheme::Layout::mixerPaintedInsertBypassDotInset,
                    row.getCentreY() - yesdaw::ui::UiTheme::Layout::mixerPaintedInsertBypassDotSize / 2,
                    yesdaw::ui::UiTheme::Layout::mixerPaintedInsertBypassDotSize,
                    yesdaw::ui::UiTheme::Layout::mixerPaintedInsertBypassDotSize);
                g.setColour (insert.enabled ? yesdaw::ui::UiTheme::Color::accentTeal()
                                            : yesdaw::ui::UiTheme::Color::mutedText());
                g.fillEllipse (dot.toFloat());

                g.setColour (insert.enabled ? kText : kMutedText);
                g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
                g.drawFittedText (
                    juce::String (fxKindStripName (insert.kind)),
                    row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::mixerPaintedInsertLabelInsetX),
                    juce::Justification::centredLeft,
                    1);
            }
        }

        // M5: the strip's sends, ON the strip: destination bus, pre/post tap, and a level bar
        // you can drag. Empty rows are wells, exactly like the insert slots above.
        {
            const std::vector<yesdaw::ui::UiMixerSendReadout>& sends = state.sends;
            for (std::size_t sendIndex = 0;
                 sendIndex < static_cast<std::size_t> (paintedSendRowCountForLane (lane, ioRows));
                 ++sendIndex)
            {
                const auto row = paintedSendRowBoundsForLane (lane, sendIndex, ioRows);
                const bool routed = sendIndex < sends.size();
                g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                g.setColour (kPanelStroke);
                g.drawRoundedRectangle (row.toFloat().reduced (
                                            yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                        yesdaw::ui::UiTheme::Radius::sm,
                                        yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);
                if (! routed)
                    continue;

                const yesdaw::ui::UiMixerSendReadout& send = sends[sendIndex];
                // The level paints as a filled bar across the row — the drag law reads the same
                // rect, so what you see is what you set.
                auto levelBar = row.reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX,
                                             yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX);
                const bool previewing = paintedSendDragPreview.stripIndex == static_cast<int> (stripIndex)
                                     && paintedSendDragPreview.sendIndex == static_cast<int> (sendIndex);
                const double paintedLevel = previewing
                    ? static_cast<double> (paintedSendDragPreview.level)
                    : static_cast<double> (send.linearGain);
                levelBar = levelBar.withWidth (juce::roundToInt (
                    static_cast<double> (levelBar.getWidth()) * std::clamp (paintedLevel, 0.0, 1.0)));
                g.setColour (stripColour.withAlpha (yesdaw::ui::UiTheme::Tone::mixerHeaderAlpha));
                g.fillRoundedRectangle (levelBar.toFloat(), yesdaw::ui::UiTheme::Radius::sm);

                auto tapCell = row.withTrimmedLeft (
                    juce::jmax (yesdaw::ui::UiTheme::Space::none,
                                row.getWidth() - yesdaw::ui::UiTheme::Layout::mixerPaintedSendTapWidth));
                g.setColour (kMutedText);
                g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
                g.drawText (send.preFader ? "PRE" : "PST", tapCell, juce::Justification::centred, false);

                g.setColour (kText);
                g.drawFittedText (
                    juce::String (send.busName.empty() ? std::string ("Bus") : send.busName),
                    row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::mixerPaintedSendLevelInsetX)
                       .withTrimmedRight (yesdaw::ui::UiTheme::Layout::mixerPaintedSendTapWidth),
                    juce::Justification::centredLeft,
                    1);
            }
        }

        // G4.1: the I/O slots — the input row leads the slot column (Track strips: the input the
        // track records from, "—" until one is picked), the output row closes it (Master or a bus).
        // Both are wells like the slot rows; a click opens the slot's choices.
        {
            const auto drawIoSlot = [&g] (juce::Rectangle<int> row, const juce::String& text, bool picked)
            {
                g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                g.fillRoundedRectangle (row.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
                g.setColour (kPanelStroke);
                g.drawRoundedRectangle (row.toFloat().reduced (yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
                                        yesdaw::ui::UiTheme::Radius::sm,
                                        yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);
                g.setColour (picked ? kText : kMutedText);
                g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
                g.drawFittedText (text,
                                  row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::mixerPaintedIoLabelInsetX),
                                  juce::Justification::centredLeft, 1);
            };
            const auto input = paintedInputRowBoundsForLane (lane, ioRows);
            if (! input.isEmpty())
                drawIoSlot (input, stripInputText (stripIndex), ! isBus && appModel.isRecordingTrackIndexArmed (stripIndex));
            const auto output = paintedOutputRowBoundsForLane (lane, ioRows);
            if (! output.isEmpty())
                drawIoSlot (output, stripOutputText (state), state.outputBusId.isValid());
        }

        // M6: the rail and the meter each derive from the shared lane laws now — there is no
        // local fader rect left to keep in sync.
        const auto meter = paintedMeterBoundsForLane (lane, ioRows);
        if (! isBus && stripIndex < trackMeterHold.size())
        {
            const MeterHoldState& hold = trackMeterHold[stripIndex];
            drawMeterWithHold (g, meter, hold.livePeak, hold.heldPeak, hold.clipLatched);
        }
        else if (isBus && stripIndex - surface.tracks.size() < busMeterHold.size())
        {
            // E22: bus meters live — same held-peak/clip-latch painting as tracks.
            const MeterHoldState& hold = busMeterHold[stripIndex - surface.tracks.size()];
            drawMeterWithHold (g, meter, hold.livePeak, hold.heldPeak, hold.clipLatched);
        }
        else
        {
            drawMeter (g, meter, state.meter.valid ? state.meter.peakLeft : 0.0f);
        }

        auto rail = paintedFaderRailForLane (lane, ioRows);
        // (every strip, the selected one included — G4.1 cp2)
        {
            g.setColour (yesdaw::ui::UiTheme::Color::controlInsetDeep());
            g.fillRoundedRectangle (rail.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
            g.setColour (yesdaw::ui::UiTheme::Color::faintText());
            for (const float markDb : yesdaw::ui::UiTheme::Layout::mixerPaintedScaleDbMarks)
            {
                const float tickY = static_cast<float> (rail.getBottom())
                                  - mixerFaderFractionForDb (markDb) * static_cast<float> (rail.getHeight());
                g.drawHorizontalLine (
                    juce::roundToInt (tickY),
                    static_cast<float> (rail.getX()
                                        - yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickGap
                                        - yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickWidth),
                    static_cast<float> (rail.getX()
                                        - yesdaw::ui::UiTheme::Layout::mixerPaintedScaleTickGap));
            }

            // Unity reads at a glance: a wider, brighter mark straight across the rail.
            {
                const float unityY = static_cast<float> (rail.getBottom())
                                   - mixerFaderFractionForDb (0.0f) * static_cast<float> (rail.getHeight());
                g.setColour (kMutedText);
                g.fillRect (juce::Rectangle<float> (
                    static_cast<float> (rail.getX() - yesdaw::ui::UiTheme::Layout::mixerPaintedUnityMarkOverhang),
                    unityY - yesdaw::ui::UiTheme::Layout::mixerPaintedUnityMarkThickness * 0.5f,
                    static_cast<float> (rail.getWidth() + 2 * yesdaw::ui::UiTheme::Layout::mixerPaintedUnityMarkOverhang),
                    yesdaw::ui::UiTheme::Layout::mixerPaintedUnityMarkThickness));
            }

            const int thumbY =
                mixerFaderThumbYForGain (rail, state.linearGain)
                - yesdaw::ui::UiTheme::Layout::mixerPaintedThumbCenterInset;
            auto thumb = juce::Rectangle<int> (
                rail.getX() - yesdaw::ui::UiTheme::Layout::mixerPaintedThumbWidthOverhang / 2,
                thumbY,
                rail.getWidth() + yesdaw::ui::UiTheme::Layout::mixerPaintedThumbWidthOverhang,
                yesdaw::ui::UiTheme::Layout::mixerPaintedThumbHeight);
            juce::ColourGradient thumbGradient (
                yesdaw::ui::UiTheme::Color::faderThumbTop(),
                static_cast<float> (thumb.getCentreX()),
                static_cast<float> (thumb.getY()),
                yesdaw::ui::UiTheme::Color::faderThumb(),
                static_cast<float> (thumb.getCentreX()),
                static_cast<float> (thumb.getBottom()),
                false);
            g.setGradientFill (thumbGradient);
            g.fillRoundedRectangle (thumb.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
        }

        auto readout = lane.removeFromBottom (
                                yesdaw::ui::UiTheme::Layout::mixerPaintedReadoutBottomInset)
                           .translated (0,
                                       -yesdaw::ui::UiTheme::Layout::mixerPaintedReadoutHeight)
                           .withHeight (yesdaw::ui::UiTheme::Layout::mixerPaintedReadoutHeight)
                           .reduced (
                               yesdaw::ui::UiTheme::Layout::mixerPaintedReadoutHorizontalInset,
                               yesdaw::ui::UiTheme::Space::xxs);
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (readout.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::caption));
        const float gainDb = 20.0f * std::log10 (std::max (
            yesdaw::ui::UiTheme::Mixer::paintedReadoutGainFloor,
            state.linearGain));
        // M6: a bare "0.0" is not a level. Silence reads as -inf, everything else carries dB.
        // G4.1: fitted, so a narrow strip's readout shrinks instead of clipping.
        g.drawFittedText (state.linearGain <= yesdaw::ui::UiTheme::Mixer::paintedReadoutGainFloor
                              ? juce::String ("-inf dB")
                              : juce::String (gainDb, 1) + " dB",
                          readout,
                          juce::Justification::centred,
                          1);
    }

    // N3: master's lane comes from the SAME single law as every track/bus strip
    // (paintedMixerLaneBounds via paintedMixerMasterBounds) — it is always the next
    // contiguous slot after the last strip, so it can never drift into a detached island.
    auto masterLane = paintedMixerMasterBounds();
    g.setColour (yesdaw::ui::UiTheme::Color::panelShadow().withAlpha (
        yesdaw::ui::UiTheme::Tone::shadowAlpha));
    g.fillRoundedRectangle (
        masterLane.toFloat().translated (
            0.0f,
            static_cast<float> (yesdaw::ui::UiTheme::Layout::controlShadowOffset)),
        yesdaw::ui::UiTheme::Radius::panel);
    juce::ColourGradient masterGradient (
        yesdaw::ui::UiTheme::Color::panelInnerHighlight(),
        static_cast<float> (masterLane.getCentreX()),
        static_cast<float> (masterLane.getY()),
        yesdaw::ui::UiTheme::Color::panel(),
        static_cast<float> (masterLane.getCentreX()),
        static_cast<float> (masterLane.getBottom()),
        false);
    g.setGradientFill (masterGradient);
    g.fillRoundedRectangle (masterLane.toFloat(), yesdaw::ui::UiTheme::Radius::panel);
    g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight());
    g.drawRoundedRectangle (
        masterLane.toFloat().reduced (
            yesdaw::ui::UiTheme::Layout::mixerPaintedStripOutlineInset),
        yesdaw::ui::UiTheme::Radius::panel,
        yesdaw::ui::UiTheme::Layout::mixerPaintedStripStrokeWidth);

    g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
        yesdaw::ui::UiTheme::Tone::mixerHeaderAlpha));
    g.fillRect (masterLane.withHeight (
        yesdaw::ui::UiTheme::Layout::mixerPaintedHeaderHeight));
    g.setColour (kText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (
        yesdaw::ui::UiTheme::Type::small,
        juce::Font::bold));
    g.drawText ("MASTER",
                masterLane.withHeight (
                    yesdaw::ui::UiTheme::Layout::mixerPaintedHeaderHeight),
                juce::Justification::centred,
                false);

    auto masterContent = masterLane.reduced (
        yesdaw::ui::UiTheme::Layout::mixerMasterContentInsetX,
        yesdaw::ui::UiTheme::Space::none);
    masterContent.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerMasterContentTop);

    // G4.1 rubric FIX: the master's card texts are FITTED — a narrow master pane shrinks them
    // instead of clipping "INTEGRATED" to "INTEGRA".
    auto loudnessCard = masterContent.removeFromTop (
        yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessCardHeight);
    g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
    g.fillRoundedRectangle (loudnessCard.toFloat(), yesdaw::ui::UiTheme::Radius::md);
    g.setColour (kMutedText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (
        yesdaw::ui::UiTheme::Type::tiny,
        juce::Font::bold));
    g.drawFittedText ("INTEGRATED", loudnessCard.withHeight (
                    yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueTop),
                juce::Justification::centred,
                1);
    g.setColour (kText);
    g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
        yesdaw::ui::UiTheme::Type::readout,
        juce::Font::bold));
    const juce::String integrated = surface.loudness.valid
        ? juce::String (surface.loudness.integratedLufs, 1)
        : juce::String ("--");
    g.drawFittedText (integrated,
                loudnessCard.withTrimmedTop (
                    yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueTop)
                    .withHeight (
                        yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueHeight),
                juce::Justification::centred,
                1);
    g.setColour (kMutedText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
    g.drawFittedText ("LUFS-I",
                loudnessCard.withTrimmedTop (
                    yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueTop
                    + yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessValueHeight)
                    .withHeight (
                        yesdaw::ui::UiTheme::Layout::mixerMasterLoudnessUnitHeight),
                juce::Justification::centred,
                1);

    masterContent.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerMasterSectionGap);
    auto peakCard = masterContent.removeFromTop (
        yesdaw::ui::UiTheme::Layout::mixerMasterPeakCardHeight);
    g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
    g.fillRoundedRectangle (peakCard.toFloat(), yesdaw::ui::UiTheme::Radius::md);
    g.setColour (kMutedText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (
        yesdaw::ui::UiTheme::Type::tiny,
        juce::Font::bold));
    g.drawFittedText ("TRUE PEAK",
                peakCard.withHeight (
                    yesdaw::ui::UiTheme::Layout::mixerMasterPeakValueTop),
                juce::Justification::centred,
                1);
    g.setColour (surface.loudness.valid && surface.loudness.truePeakDbtp > 0.0
                     ? yesdaw::ui::UiTheme::Color::dangerRed()
                     : kText);
    g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
        yesdaw::ui::UiTheme::Type::body,
        juce::Font::bold));
    const juce::String truePeak = surface.loudness.valid
        ? juce::String (surface.loudness.truePeakDbtp, 1) + " dBTP"
        : juce::String ("-- dBTP");
    g.drawFittedText (truePeak,
                peakCard.withTrimmedTop (
                    yesdaw::ui::UiTheme::Layout::mixerMasterPeakValueTop)
                    .withHeight (
                        yesdaw::ui::UiTheme::Layout::mixerMasterPeakValueHeight),
                juce::Justification::centred,
                1);

    const float masterPeakLeft = liveMasterPeakLeft.load (std::memory_order_acquire);
    const float masterPeakRight = liveMasterPeakRight.load (std::memory_order_acquire);

    masterContent.removeFromTop (yesdaw::ui::UiTheme::Layout::mixerMasterMeterTopGap);
    auto meterArea = masterContent.withTrimmedBottom (
        yesdaw::ui::UiTheme::Layout::mixerMasterMeterBottomInset);
    auto scale = meterArea.removeFromLeft (
        yesdaw::ui::UiTheme::Layout::mixerMasterScaleWidth);
    g.setColour (yesdaw::ui::UiTheme::Color::faintText());
    g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
        yesdaw::ui::UiTheme::Type::tiny));
    for (std::size_t i = 0;
         i < yesdaw::ui::UiTheme::Layout::mixerMasterScaleDb.size();
         ++i)
    {
        const float fraction = static_cast<float> (i)
                             / static_cast<float> (
                                   yesdaw::ui::UiTheme::Layout::mixerMasterScaleDb.size() - 1u);
        const int y = scale.getY()
                    + juce::roundToInt (fraction * static_cast<float> (
                          scale.getHeight()
                          - yesdaw::ui::UiTheme::Layout::mixerMasterScaleLabelHeight));
        g.drawText (juce::String (yesdaw::ui::UiTheme::Layout::mixerMasterScaleDb[i]),
                    juce::Rectangle<int> {
                        scale.getX(), y, scale.getWidth(),
                        yesdaw::ui::UiTheme::Layout::mixerMasterScaleLabelHeight },
                    juce::Justification::centredRight,
                    false);
    }

    const int meterPairWidth = 2 * yesdaw::ui::UiTheme::Layout::mixerMasterMeterWidth
                             + yesdaw::ui::UiTheme::Layout::mixerMasterMeterGap;
    auto meterPair = meterArea.withWidth (meterPairWidth)
                         .withCentre ({ meterArea.getCentreX(), meterArea.getCentreY() });
    auto leftMeter = meterPair.removeFromLeft (
        yesdaw::ui::UiTheme::Layout::mixerMasterMeterWidth);
    meterPair.removeFromLeft (yesdaw::ui::UiTheme::Layout::mixerMasterMeterGap);
    auto rightMeter = meterPair.removeFromLeft (
        yesdaw::ui::UiTheme::Layout::mixerMasterMeterWidth);
    drawMeter (g, leftMeter, masterPeakLeft);
    drawMeter (g, rightMeter, masterPeakRight);
    g.setColour (kMutedText);
    g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny));
    auto channelLabels = masterLane.withTrimmedTop (
                             masterLane.getHeight()
                             - yesdaw::ui::UiTheme::Layout::mixerMasterMeterChannelLabelHeight)
                             .reduced (
                                 yesdaw::ui::UiTheme::Layout::mixerMasterContentInsetX,
                                 yesdaw::ui::UiTheme::Space::none);
    g.drawText ("L     R", channelLabels, juce::Justification::centred, false);
}

yesdaw::ui::UiMixerSurfaceSnapshot MainComponent::currentMixerSurface() const
{
    if (appModel.context().projectLoaded)
        return yesdaw::ui::projectUiMixerSurface (appModel.project());

    return {};
}

} // namespace yesdaw::ui
