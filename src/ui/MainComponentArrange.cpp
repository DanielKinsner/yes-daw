// YES DAW — the app shell: the arrangement (timeline, tracks, clips, ruler, automation, zoom).
//
// Plan §5.1 (the shell topology), checkpoint 2 (2026-09-05): member bodies of MainComponent, carved
// verbatim from the inline class. The declaration is ui/MainComponentShell.h.

#include "ui/MainComponentShell.h"

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

// V8: a visible toolbar zoom control. The buttons dispatch the EXISTING zoom actions through
// handleAction (the same playhead-anchored law the menu/keyboard path runs), and the readout
// shows the one shared timelineZoomFactor every zoom gesture mutates — never a second zoom
// concept.
void MainComponent::configureTimelineZoomControls()
{
    const auto configureStep = [this] (juce::TextButton& button,
                                       yesdaw::ui::UiActionId action,
                                       const char* stepText)
    {
        configureActionComponent (button, action, stepText);
        button.setButtonText (stepText);
        button.setColour (juce::TextButton::buttonColourId,
                          yesdaw::ui::UiTheme::Color::buttonSurface());
        button.setColour (juce::TextButton::textColourOffId, kText);
        button.setColour (juce::TextButton::textColourOnId, kText);
        button.onClick = [this, action] {
            handleAction (action);
            refreshActionState();
            repaintAll();
        };
        addAndMakeVisible (button);
    };
    configureStep (timelineZoomOutButton, yesdaw::ui::UiActionId::TimelineZoomOut, "-");
    configureStep (timelineZoomInButton, yesdaw::ui::UiActionId::TimelineZoomIn, "+");

    timelineZoomReadout.setComponentID ("timeline.zoom.readout");
    timelineZoomReadout.setName ("Timeline zoom factor");
    timelineZoomReadout.setTooltip ("Current timeline zoom factor (1.0x fits the project)");
    timelineZoomReadout.setJustificationType (juce::Justification::centred);
    timelineZoomReadout.setColour (juce::Label::textColourId, kMutedText);
    timelineZoomReadout.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (timelineZoomReadout);

    // G2.16: the zoom slider (log2 of the factor, 0..6 = 1x..64x) drives the ONE zoom law at the playhead.
    timelineZoomSlider.setComponentID ("timeline.zoom.slider");
    timelineZoomSlider.setName ("Timeline zoom");
    timelineZoomSlider.setTitle ("Timeline zoom");
    timelineZoomSlider.setTooltip ("Timeline zoom: drag to zoom at the playhead (1x fits the project)");
    timelineZoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    timelineZoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, false,
                                        yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                        yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
    timelineZoomSlider.setRange (0.0, std::log2 (timelineZoomCeiling()), 0.01);
    timelineZoomSlider.setValue (0.0, juce::dontSendNotification);
    timelineZoomSlider.onValueChange = [this] {
        if (refreshingZoomSlider)
            return;
        const double wanted = std::exp2 (timelineZoomSlider.getValue());
        const double playheadSeconds = appModel.project().sampleRate.isValid()
            ? static_cast<double> (std::max<std::int64_t> (0, appModel.context().playheadFrame)) / appModel.project().sampleRate.hz
            : 0.0;
        zoomTimelineAtAnchor (playheadSeconds, wanted / std::max (1.0e-9, timelineZoomFactor));
        repaintAll();
    };
    addAndMakeVisible (timelineZoomSlider);

    // G2.16: real scroll bars — horizontal in seconds under the timeline, vertical in rows beside it.
    timelineHScroll.setComponentID ("timeline.scroll.h");
    timelineHScroll.setName ("Timeline scroll");
    timelineHScroll.setTitle ("Timeline scroll");
    timelineHScroll.setTooltip ("Scroll the timeline in time (drag the thumb; the wheel over the timeline scrolls too)");
    timelineHScroll.setAutoHide (false);
    timelineHScroll.addListener (this);
    addAndMakeVisible (timelineHScroll);
    timelineVScroll.setComponentID ("timeline.scroll.v");
    timelineVScroll.setName ("Track scroll");
    timelineVScroll.setTitle ("Track scroll");
    timelineVScroll.setTooltip ("Scroll the tracks (drag the thumb)");
    timelineVScroll.setAutoHide (false);
    timelineVScroll.addListener (this);
    addAndMakeVisible (timelineVScroll);

    // R4: the shared status line — failures from save/export/create/autosave/device paint
    // here from real model state; success stays quiet and the UI timer decays the text.
    statusLine.setComponentID ("shell.statusline");
    statusLine.setName ("Status line");
    statusLine.setTooltip ("Status messages: failures from save, export, project create, autosave, and the audio device");
    statusLine.setJustificationType (juce::Justification::centredLeft);
    statusLine.setColour (juce::Label::textColourId, kMutedText);
    statusLine.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (statusLine);
}

void MainComponent::configureAutomationLaneControls()
{
    constexpr yesdaw::ui::UiActionId action = yesdaw::ui::UiActionId::TimelineAutomationToggleTrackLane;
    configureActionComponent (automationLaneToggle, action, "Automation lanes");
    automationLaneToggle.setButtonText ("A");   // G2.1 cp3: the view cluster's A (the tooltip carries the name + chord)
    automationLaneToggle.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
    automationLaneToggle.setColour (juce::TextButton::buttonOnColourId, kPurple.darker (0.45f));
    automationLaneToggle.setColour (juce::TextButton::textColourOffId, kText);
    automationLaneToggle.setColour (juce::TextButton::textColourOnId, kText);
    automationLaneToggle.onClick = [this] {
        (void) appModel.dispatch (yesdaw::ui::UiActionId::TimelineAutomationToggleTrackLane);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (automationLaneToggle);

    // E20: the lane-target chooser — the canvas edits whatever target it names, creating
    // the lane on demand (fader / pan / each send level / each FX param).
    automationTargetChooser.setComponentID ("timeline.automation.target");
    automationTargetChooser.setTooltip ("Choose which automation lane the canvas edits");
    automationTargetChooser.onChange = [this] {
        if (refreshingAutomationTarget)
            return;

        const int selected = automationTargetChooser.getSelectedId();
        if (selected <= 0)
            return;

        selectedAutomationTargetIndex = selected - 1;
        refreshActionState();
        repaintAll();
    };
    addChildComponent (automationTargetChooser);

    // N5/R15: the automation write mode — Read (default, playback only), Touch/Latch (a
    // control drag during playback writes breakpoints instead of a plain edit), or Off
    // (lanes stay stored and editable but playback IGNORES them and nothing ever writes).
    automationModeChooser.setComponentID ("timeline.automation.mode");
    automationModeChooser.setTooltip ("Automation write mode: Read plays back; Touch/Latch "
                                      "record a control ride while the transport rolls; "
                                      "Off ignores every lane and writes nothing");
    automationModeChooser.addItem ("Read", 1);
    automationModeChooser.addItem ("Touch", 2);
    automationModeChooser.addItem ("Latch", 3);
    automationModeChooser.addItem ("Off", 4);   // id - 1 == AutomationMode::Off
    automationModeChooser.onChange = [this] {
        if (refreshingAutomationTarget)
            return;

        const int selected = automationModeChooser.getSelectedId();
        if (selected <= 0)
            return;

        (void) appModel.setAutomationMode (
            static_cast<yesdaw::engine::AutomationMode> (selected - 1));
        refreshActionState();
        repaintAll();
    };
    addChildComponent (automationModeChooser);

    automationLaneRow.setComponentID (kAutomationLaneRowComponentId);
    automationLaneRow.setTooltip ("First Track automation lane row");
    automationLaneRow.setName ("First Track automation lane");
    automationLaneRow.setTitle ("First Track automation lane");
    automationLaneRow.setTooltip (kAutomationLaneRowComponentId);
    automationLaneRow.setJustificationType (juce::Justification::centredLeft);
    automationLaneRow.setColour (juce::Label::backgroundColourId, yesdaw::ui::UiTheme::Color::selectedLane());
    automationLaneRow.setColour (juce::Label::textColourId, kText);
    automationLaneRow.setVisible (false);
    addAndMakeVisible (automationLaneRow);

    constexpr yesdaw::ui::UiActionId addAction = yesdaw::ui::UiActionId::TimelineAutomationAddBreakpoint;
    configureActionComponent (automationBreakpointAddButton, addAction, "Add automation breakpoint");
    if (const auto* descriptor = appModel.registry().descriptor (addAction))
        automationBreakpointAddButton.setButtonText (descriptor->label);
    automationBreakpointAddButton.setColour (juce::TextButton::buttonColourId,
                                             yesdaw::ui::UiTheme::Color::buttonSurface());
    // N4: adds to the SELECTED target's lane (creating it on first use), matching the canvas
    // click path — never the first track's fader regardless of what is chosen.
    automationBreakpointAddButton.onClick = [this] {
        const AutomationTargetOption target = currentAutomationTarget();
        if (target.ownerEntity.isValid())
            (void) appModel.addAutomationBreakpointToLane (
                target.ownerEntity, target.role, target.paramId,
                yesdaw::ui::UiAppModel::kFirstTrackAutomationBreakpointAddTick,
                yesdaw::ui::UiAppModel::kFirstTrackAutomationBreakpointAddValue);
        refreshActionState();
        repaintAll();
    };
    automationBreakpointAddButton.setVisible (false);
    addAndMakeVisible (automationBreakpointAddButton);

    constexpr yesdaw::ui::UiActionId deleteAction = yesdaw::ui::UiActionId::TimelineAutomationDeleteBreakpoint;
    configureActionComponent (automationBreakpointDeleteButton, deleteAction, "Delete automation breakpoint");
    if (const auto* descriptor = appModel.registry().descriptor (deleteAction))
        automationBreakpointDeleteButton.setButtonText (descriptor->label);
    automationBreakpointDeleteButton.setColour (juce::TextButton::buttonColourId,
                                                yesdaw::ui::UiTheme::Color::buttonSurface());
    // N4: deletes the SELECTED target's last breakpoint, matching the canvas — never the
    // first track's fader regardless of what is chosen.
    automationBreakpointDeleteButton.onClick = [this] {
        const AutomationTargetOption target = currentAutomationTarget();
        if (target.ownerEntity.isValid())
        {
            if (const yesdaw::engine::AutomationLaneData* const lane = appModel.automationLaneForTarget (
                    target.ownerEntity, target.role, target.paramId);
                lane != nullptr && ! lane->points.empty())
            {
                (void) appModel.removeAutomationBreakpointAtTick (lane->id, lane->points.back().tick);
            }
        }
        refreshActionState();
        repaintAll();
    };
    automationBreakpointDeleteButton.setVisible (false);
    addAndMakeVisible (automationBreakpointDeleteButton);
}

// G2.16: the timeline PANEL holds the canvas plus the scroll-bar strips below and beside it;
// timelineBounds() is the canvas alone — the ONE rect the input, the playhead layer, the
// toolbar cluster, the fit law and the probe share, so a pixel means the same time everywhere.
juce::Rectangle<int> MainComponent::timelinePanelBounds() const
{
    auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
    work.removeFromBottom (dockedMixerHeight());
    work.removeFromLeft (viewState.railWidth);
    work.removeFromRight (inspectorWidthNow());
    return work.reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                         yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
}

juce::Rectangle<int> MainComponent::timelineBounds() const
{
    return timelinePanelBounds()
        .withTrimmedBottom (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness)
        .withTrimmedRight (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness);
}

// G2.17: multi-select — Ctrl toggles a lane in the set, Shift extends from the primary; a plain
// click and the Up / Down verbs collapse to one. The primary stays the lane the strip verbs act on.
void MainComponent::toggleTrackLaneSelection (int lane)
{
    const int trackCount = static_cast<int> (appModel.project().tracks.size());
    if (! appModel.context().projectLoaded || lane < 0 || lane >= trackCount)
        return;
    if (selectedTrackLane >= 0 && selectedTrackLanes.empty())
        selectedTrackLanes.insert (selectedTrackLane);
    if (selectedTrackLanes.count (lane) > 0 && selectedTrackLanes.size() > 1)
        selectedTrackLanes.erase (lane);
    else
        selectedTrackLanes.insert (lane);
    dismissTrackRenameEditor();
    selectedTrackLane = lane;
    (void) appModel.selectMixerTrack (static_cast<std::size_t> (lane), /*showMixerPanel*/ false);
    refreshActionState();
    resized();   // G3.1: the inspector's TRACK tab lays out per selected Track (the instrument row)
    repaintAll();
}

void MainComponent::extendTrackLaneSelection (int lane)
{
    const int trackCount = static_cast<int> (appModel.project().tracks.size());
    if (! appModel.context().projectLoaded || lane < 0 || lane >= trackCount)
        return;
    const int anchor = selectedTrackLane >= 0 ? selectedTrackLane : lane;
    selectedTrackLanes.clear();
    for (int row = std::min (anchor, lane); row <= std::max (anchor, lane); ++row)
        selectedTrackLanes.insert (row);
    dismissTrackRenameEditor();
    selectedTrackLane = lane;
    (void) appModel.selectMixerTrack (static_cast<std::size_t> (lane), /*showMixerPanel*/ false);
    refreshActionState();
    resized();   // G3.1: the inspector's TRACK tab lays out per selected Track (the instrument row)
    repaintAll();
}

bool MainComponent::trackLaneIsSelected (int lane) const noexcept
{
    return lane == selectedTrackLane || selectedTrackLanes.count (lane) > 0;
}

void MainComponent::selectTrackLane (int lane)
{
    const int trackCount = static_cast<int> (appModel.project().tracks.size());
    if (! appModel.context().projectLoaded || lane < 0 || lane >= trackCount)
        return;

    dismissTrackRenameEditor();
    selectedTrackLanes.clear();   // G2.17: a plain selection is one lane
    selectedTrackLane = lane;
    (void) appModel.selectMixerTrack (static_cast<std::size_t> (lane), /*showMixerPanel*/ false);
    refreshActionState();
    resized();   // G3.1: the inspector's TRACK tab lays out per selected Track (the instrument row)
    repaintAll();
}

// G2.17: a track is a MIDI track when it holds MIDI clips; audio otherwise (an empty track is audio).
bool MainComponent::trackHoldsMidi (std::size_t trackIndex) const noexcept
{
    const auto& tracks = appModel.project().tracks;
    if (trackIndex >= tracks.size())
        return false;
    for (const yesdaw::engine::MidiClip& clip : appModel.project().midiClips)
        if (clip.trackId == tracks[trackIndex].id)
            return true;
    return false;
}

void MainComponent::selectAdjacentTrackLane (yesdaw::ui::UiActionId action)
{
    const int trackCount = static_cast<int> (appModel.project().tracks.size());
    if (trackCount <= 0 || ! appModel.dispatch (action).dispatched)
        return;

    const int delta = action == yesdaw::ui::UiActionId::TrackSelectPrevious ? -1 : 1;
    const int initialLane = delta < 0 ? trackCount - 1 : 0;
    const int nextLane = selectedTrackLane < 0 || selectedTrackLane >= trackCount
        ? initialLane
        : std::clamp (selectedTrackLane + delta, 0, trackCount - 1);
    selectTrackLane (nextLane);
}

void MainComponent::openTrackRenameEditor()
{
    dismissClipRenameEditor();
    const auto& tracks = appModel.project().tracks;
    if (selectedTrackLane < 0 || selectedTrackLane >= static_cast<int> (tracks.size()))
        return;

    const juce::Rectangle<int> row = trackListInput.rowBounds (selectedTrackLane);
    if (row.isEmpty())
        return;

    trackRenameEditor.setBounds (row.translated (trackListInput.getX(), trackListInput.getY())
                                    .reduced (yesdaw::ui::UiTheme::Layout::trackListRowHorizontalInset,
                                              yesdaw::ui::UiTheme::Layout::trackListRowVerticalInset)
                                    .withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListIconLeftInset)
                                    .withHeight (yesdaw::ui::UiTheme::Layout::trackListRenameEditorHeight));
    trackRenameEditor.setText (juce::String (tracks[static_cast<std::size_t> (selectedTrackLane)].strip.name),
                               juce::dontSendNotification);
    trackRenameEditor.setVisible (true);
    trackRenameEditor.grabKeyboardFocus();
}

void MainComponent::commitTrackRenameEditor()
{
    const auto& tracks = appModel.project().tracks;
    if (selectedTrackLane >= 0 && selectedTrackLane < static_cast<int> (tracks.size()))
    {
        const std::string newName = trackRenameEditor.getText().toStdString();
        (void) appModel.renameProjectTrack (tracks[static_cast<std::size_t> (selectedTrackLane)].id, newName);
    }

    dismissTrackRenameEditor();
    refreshActionState();
    repaintAll();
}

void MainComponent::dismissTrackRenameEditor()
{
    trackRenameEditor.setVisible (false);
}

void MainComponent::openClipRenameEditor()
{
    const yesdaw::engine::EntityId selectedId = appModel.selectedTimelineClipId();
    const yesdaw::engine::Clip* const selectedClip = findProjectClipById (selectedId);
    const auto view = std::find (timelineClipIds.begin(), timelineClipIds.end(), selectedId);
    if (selectedClip == nullptr || view == timelineClipIds.end())
        return;

    dismissTrackRenameEditor();
    const std::size_t viewIndex = static_cast<std::size_t> (std::distance (timelineClipIds.begin(), view));
    const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
    const yesdaw::ui::Clip& clip = timelineClips[viewIndex];
    const int left = geometry.clipArea.getX()
                   + juce::roundToInt ((clip.startSeconds - geometry.viewport.scrollSeconds)
                                       * geometry.viewport.pixelsPerSecond);
    const int top = geometry.clipArea.getY()
                  + static_cast<int> (std::llround (
                        geometry.laneTop (clip.lane) - geometry.viewport.laneScrollPixels));
    const int width = juce::roundToInt (clip.lengthSeconds * geometry.viewport.pixelsPerSecond);
    juce::Rectangle<int> bounds {
        left, top, width, static_cast<int> (std::llround (geometry.laneHeightFor (clip.lane))) };
    bounds = bounds.getIntersection (geometry.clipArea)
                   .reduced (yesdaw::ui::UiTheme::Space::sm)
                   .withHeight (yesdaw::ui::UiTheme::Layout::trackListRenameEditorHeight)
                   .translated (timelineInput.getX(), timelineInput.getY());
    if (bounds.isEmpty())
        return;

    clipRenameEditor.setBounds (bounds);
    clipRenameEditor.setText (juce::String (selectedClip->name.c_str()), juce::dontSendNotification);
    clipRenameEditor.setVisible (true);
    clipRenameEditor.grabKeyboardFocus();
}

void MainComponent::commitClipRenameEditor()
{
    (void) appModel.renameSelectedTimelineClip (clipRenameEditor.getText().toStdString());
    dismissClipRenameEditor();
    refreshActionState();
    repaintAll();
}

// Marker rename (E7): positioned over the painted label through the shared rect law.
void MainComponent::openMarkerRenameEditor (int markerIndex)
{
    const auto& markers = appModel.project().markers;
    if (markerIndex < 0 || markerIndex >= static_cast<int> (markers.size()))
        return;

    dismissTrackRenameEditor();
    dismissClipRenameEditor();
    const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
    juce::Rectangle<int> bounds = yesdaw::ui::timelineMarkerLabelRect (
        timelineInput.getLocalBounds(), state, markerIndex)
                                      .translated (timelineInput.getX(), timelineInput.getY());
    if (bounds.isEmpty())
        return;

    markerRenameIndex = markerIndex;
    markerRenameEditor.setBounds (bounds);
    markerRenameEditor.setText (juce::String (markers[static_cast<std::size_t> (markerIndex)].name.c_str()),
                                juce::dontSendNotification);
    markerRenameEditor.setVisible (true);
    markerRenameEditor.grabKeyboardFocus();
}

void MainComponent::commitMarkerRenameEditor()
{
    const auto& markers = appModel.project().markers;
    if (markerRenameIndex >= 0 && markerRenameIndex < static_cast<int> (markers.size()))
        (void) appModel.renameTimelineMarker (
            markers[static_cast<std::size_t> (markerRenameIndex)].id,
            markerRenameEditor.getText().toStdString());
    dismissMarkerRenameEditor();
    refreshActionState();
    repaintAll();
}

void MainComponent::dismissMarkerRenameEditor()
{
    markerRenameIndex = -1;
    markerRenameEditor.setVisible (false);
}

void MainComponent::dismissClipRenameEditor()
{
    clipRenameEditor.setVisible (false);
}

void MainComponent::removeSelectedTrack()
{
    const auto& tracks = appModel.project().tracks;
    if (selectedTrackLane < 0 || selectedTrackLane >= static_cast<int> (tracks.size()))
        return;

    dismissTrackRenameEditor();
    if (appModel.removeProjectTrack (tracks[static_cast<std::size_t> (selectedTrackLane)].id).dispatched)
        selectedTrackLane = std::min (selectedTrackLane,
                                      static_cast<int> (appModel.project().tracks.size()) - 1);

    refreshActionState();
    repaintAll();
}

// Selected-track strip/arm toggles (B28): the rail row is the target; the mixer never opens.
void MainComponent::toggleSelectedTrackKey (yesdaw::ui::UiActionId action)
{
    const auto& tracks = appModel.project().tracks;
    if (selectedTrackLane < 0 || selectedTrackLane >= static_cast<int> (tracks.size()))
        return;

    const std::size_t lane = static_cast<std::size_t> (selectedTrackLane);
    switch (action)
    {
        case yesdaw::ui::UiActionId::TrackToggleMute:
            (void) appModel.toggleTrackMute (tracks[lane].id);
            break;
        case yesdaw::ui::UiActionId::TrackToggleSolo:
            (void) appModel.toggleTrackSolo (tracks[lane].id);
            break;
        case yesdaw::ui::UiActionId::TrackToggleArm:
            (void) appModel.toggleRecordingArmForTrack (lane);
            break;
        default:
            return;
    }

    refreshActionState();
    repaintAll();
}

void MainComponent::moveSelectedTrack (int delta)
{
    const auto& tracks = appModel.project().tracks;
    const int trackCount = static_cast<int> (tracks.size());
    if (selectedTrackLane < 0 || selectedTrackLane >= trackCount)
        return;

    const int targetLane = selectedTrackLane + delta;
    if (targetLane < 0 || targetLane >= trackCount)
        return;   // honest boundary no-op: the first row cannot move up, the last cannot move down

    dismissTrackRenameEditor();
    if (appModel.reorderProjectTrack (tracks[static_cast<std::size_t> (selectedTrackLane)].id,
                                      static_cast<std::size_t> (targetLane)).dispatched)
        selectTrackLane (targetLane);   // the rail highlight follows the moved row

    refreshActionState();
    repaintAll();
}

void MainComponent::duplicateSelectedTrack()
{
    const auto& tracks = appModel.project().tracks;
    if (selectedTrackLane < 0 || selectedTrackLane >= static_cast<int> (tracks.size()))
        return;

    dismissTrackRenameEditor();
    const yesdaw::ui::UiActionDispatchResult result =
        appModel.duplicateProjectTrack (tracks[static_cast<std::size_t> (selectedTrackLane)].id);
    if (result.dispatched)
        selectTrackLane (selectedTrackLane + 1);   // the copy lands directly below the source
    else if (result.state.disabledReason != nullptr && result.state.disabledReason[0] != '\0')
        appModel.reportStatus (std::string ("Duplicate Track refused: ") + result.state.disabledReason, true);   // R4: a refusal says why

    refreshActionState();
    repaintAll();
}

void MainComponent::layoutAutomationLaneControls()
{
    using L = yesdaw::ui::UiTheme::Layout;
    const auto timeline = timelineBounds();
    // V8: the zoom cluster shares the automation toggle's toolbar row.
    timelineZoomOutButton.setBounds (L::timelineZoomOutButtonBounds (timeline));
    timelineZoomReadout.setBounds (L::timelineZoomReadoutBounds (timeline));
    timelineZoomInButton.setBounds (L::timelineZoomInButtonBounds (timeline));
    timelineZoomSlider.setBounds (L::timelineZoomSliderBounds (timeline));   // G2.16
    // G1.4 toolbar v2: [nudge] … status … [Inspector]; the nudge chooser drops whole when
    // the row cannot hold it next to the toggle.
    {
        juce::Rectangle<int> status = L::statusLineBounds (timeline);
        const juce::Rectangle<int> toggle = status.withX (timeline.getRight() - L::statusLineRightInset - L::inspectorToggleWidth)
                                                  .withWidth (L::inspectorToggleWidth);
        const int clusterLeft = toggle.getX() - (L::inspectorToggleWidth + L::inspectorToggleGap) * 3;   // G2.1 cp3: [I][X][P][A]
        // G2.6 / G2.7: [Snap mode][Edit mode][Nudge] lead the status row; each drops whole when
        // the row cannot hold it.
        const juce::Rectangle<int> snapMode = status.withWidth (L::timelineSnapModeChooserWidth);
        const bool snapModeFits = snapMode.getRight() + L::timelineNudgeChooserGap + L::inspectorToggleGap <= clusterLeft;
        snapModeChooser.setBounds (snapModeFits ? snapMode : juce::Rectangle<int>());
        const juce::Rectangle<int> editMode = status.withX (snapModeFits ? snapMode.getRight() + L::timelineNudgeChooserGap : status.getX())
                                                    .withWidth (L::timelineEditModeChooserWidth);
        const bool editModeFits = editMode.getRight() + L::timelineNudgeChooserGap + L::inspectorToggleGap <= clusterLeft;
        editModeChooser.setBounds (editModeFits ? editMode : juce::Rectangle<int>());
        juce::Rectangle<int> nudge = status.withX (editModeFits ? editMode.getRight() + L::timelineNudgeChooserGap
                                                   : snapModeFits ? snapMode.getRight() + L::timelineNudgeChooserGap : status.getX())
                                           .withWidth (L::timelineNudgeChooserWidth);
        const bool nudgeFits = nudge.getRight() + L::timelineNudgeChooserGap + L::inspectorToggleGap <= clusterLeft;
        nudgeValueChooser.setBounds (nudgeFits ? nudge : juce::Rectangle<int>());
        // G2.1 cp3: the view cluster [I][X][P][A] (plan §3.1) — inspector, dock (mixer) toggle,
        // piano-roll toggle, automation lane toggle — one row of letters at the status line's
        // right, above the timeline canvas in z. The zoom trio took the old Automation slot.
        const int step = L::inspectorToggleWidth + L::inspectorToggleGap;
        const juce::Rectangle<int> automationToggle = toggle;
        const juce::Rectangle<int> pianoToggle = toggle.translated (-step, 0);
        const juce::Rectangle<int> mixerToggle = toggle.translated (-step * 2, 0);
        const juce::Rectangle<int> inspectorToggleBounds = toggle.translated (-step * 3, 0);
        inspectorToggle.setBounds (inspectorToggleBounds);
        mixerDockToggle.setBounds (mixerToggle);
        setToolbarButtonBounds (yesdaw::ui::UiActionId::ViewPianoRoll, pianoToggle);
        automationLaneToggle.setBounds (automationToggle);
        inspectorToggle.toFront (false);
        mixerDockToggle.toFront (false);
        if (juce::Component* piano = toolbarButtonFor (yesdaw::ui::UiActionId::ViewPianoRoll))
            piano->toFront (false);
        automationLaneToggle.toFront (false);
        const int statusLeft = nudgeFits ? nudge.getRight() + L::timelineNudgeChooserGap
                             : editModeFits ? editMode.getRight() + L::timelineNudgeChooserGap
                             : snapModeFits ? snapMode.getRight() + L::timelineNudgeChooserGap : status.getX();
        const int statusRight = clusterLeft - L::inspectorToggleGap;
        statusLine.setBounds (statusRight > statusLeft ? status.withLeft (statusLeft).withRight (statusRight)
                                                      : juce::Rectangle<int>());
    }

    // E26: the lane lives in the geometry law's reserved band — a header row (lane label
    // left, breakpoint controls right) above a FULL-WIDTH canvas, so the curve is never
    // hidden behind its own controls and never overlaps clip content.
    auto band = yesdaw::ui::timelineCanvasGeometry (timeline, makeTimelineState())
                    .automationLaneArea;
    auto header = band.removeFromTop (L::timelineCanvasAutomationHeaderHeight);
    automationTargetChooser.setBounds (
        header.removeFromRight (L::automationTargetChooserWidth));
    header.removeFromRight (L::timelineCanvasAutomationHeaderGap);
    automationModeChooser.setBounds (
        header.removeFromRight (L::automationModeChooserWidth));
    header.removeFromRight (L::timelineCanvasAutomationHeaderGap);
    automationBreakpointDeleteButton.setBounds (
        header.removeFromRight (L::automationBreakpointDeleteButtonWidth));
    header.removeFromRight (L::timelineCanvasAutomationHeaderGap);
    automationBreakpointAddButton.setBounds (
        header.removeFromRight (L::automationBreakpointAddButtonWidth));
    automationLaneRow.setBounds (header.withTrimmedLeft (L::timelineCanvasClipAreaInsetX));
    // N4: the band's X already equals clipArea's X (timelineCanvasGeometry carves it from
    // the target row, which lives inside clipArea) — no extra horizontal inset here, or the
    // canvas would drift off clipArea's span and breakpoints would stop lining up with clip
    // time positions.
    automationLaneCanvas.setBounds (
        band.withTrimmedBottom (L::timelineCanvasAutomationHeaderGap / 2));
}

// Vertical track scroll (E5): one shared whole-row offset moves the timeline lanes and the
// track rail together. The shared clamp honors WHICHEVER surface overflows more, and each
// surface pins its own applied offset so its last row never scrolls past the window bottom.
void MainComponent::scrollTrackRowsBy (int rowDelta)
{
    const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (
        timelineInput.getLocalBounds(), makeTimelineState());
    const int maxRows = std::max (geometry.maxTrackScrollRows, trackListInput.maxScrollRows());
    timelineTrackScrollRows = std::clamp (timelineTrackScrollRows + rowDelta, 0, maxRows);
    repaintAll();
}

// V8: the ONE place the toolbar readout learns the current factor — called from every path
// that mutates timelineZoomFactor, so the visible number can never go stale against the
// gestures (wheel, zoom tool, actions, and the fit verbs all funnel here or call it).
void MainComponent::refreshTimelineZoomReadout()
{
    timelineZoomReadout.setText (juce::String (timelineZoomFactor, 1) + "x",
                                 juce::dontSendNotification);
    refreshingZoomSlider = true;   // G2.16: the slider follows the factor (log scale)
    timelineZoomSlider.setRange (0.0, std::log2 (timelineZoomCeiling()), 0.01);   // G2.19: to the ceiling
    timelineZoomSlider.setValue (std::log2 (std::clamp (timelineZoomFactor, yesdaw::ui::UiTheme::Layout::timelineZoomMin,
                                                         timelineZoomCeiling())),
                                 juce::dontSendNotification);
    refreshingZoomSlider = false;
    refreshTimelineScrollBars();
}

void MainComponent::pushZoomHistory()
{
    if (! zoomHistory.empty() && zoomHistory.back().zoom == timelineZoomFactor && zoomHistory.back().scroll == timelineScrollSeconds)
        return;
    zoomHistory.push_back ({ timelineZoomFactor, timelineScrollSeconds });
    if (zoomHistory.size() > static_cast<std::size_t> (yesdaw::ui::UiTheme::Layout::timelineZoomHistoryDepth))
        zoomHistory.erase (zoomHistory.begin());
}

bool MainComponent::popZoomHistory()
{
    if (zoomHistory.empty())
        return false;
    const ZoomView view = zoomHistory.back();
    zoomHistory.pop_back();
    timelineZoomFactor = std::clamp (view.zoom, yesdaw::ui::UiTheme::Layout::timelineZoomMin, timelineZoomCeiling());
    timelineScrollSeconds = std::max (0.0, view.scroll);
    refreshTimelineZoomReadout();
    return true;
}

// G2.16: the vertical zoom — every auto-height row scales; the rail and the canvas share it.
void MainComponent::zoomTracksBy (double factor)
{
    timelineRowZoom = std::clamp (timelineRowZoom * factor, yesdaw::ui::UiTheme::Layout::timelineRowZoomMin,
                                  yesdaw::ui::UiTheme::Layout::timelineRowZoomMax);
    resized();
    refreshTimelineScrollBars();
}

// G2.16: the scroll bars mirror the view (seconds horizontally, rows vertically) and drive it.
void MainComponent::refreshTimelineScrollBars()
{
    refreshingScrollBars = true;
    const double visible = std::max (0.0, timelineVisibleSecondsFor (timelineTotalSeconds));
    timelineHScroll.setRangeLimits (0.0, std::max (timelineTotalSeconds, timelineScrollSeconds + visible), juce::dontSendNotification);
    timelineHScroll.setCurrentRange (timelineScrollSeconds, visible, juce::dontSendNotification);
    const yesdaw::ui::TimelineCanvasGeometry geometry = yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), makeTimelineState());
    const int maxRows = std::max (geometry.maxTrackScrollRows, trackListInput.maxScrollRows());
    const double visibleRows = std::max (1.0, static_cast<double> (std::max (1, appModel.context().projectLoaded ? static_cast<int> (appModel.project().tracks.size()) : 1) - maxRows));
    timelineVScroll.setRangeLimits (0.0, static_cast<double> (maxRows) + visibleRows, juce::dontSendNotification);
    timelineVScroll.setCurrentRange (static_cast<double> (timelineTrackScrollRows), visibleRows, juce::dontSendNotification);
    refreshingScrollBars = false;
}

void MainComponent::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (refreshingScrollBars)
        return;
    if (bar == &timelineHScroll)
        timelineScrollSeconds = std::max (0.0, newRangeStart);
    else if (bar == &timelineVScroll)
        timelineTrackScrollRows = std::max (0, juce::roundToInt (newRangeStart));
    repaintAll();
}

// G2.19: the zoom ceiling — one sample per pixel at the project's rate (never below the old
// 64x floor). It follows the fit width, so it is a function, not a token.
double MainComponent::timelineZoomCeiling() const noexcept
{
    using L = yesdaw::ui::UiTheme::Layout;
    const double width = static_cast<double> (juce::jmax (L::timelineViewportMinPixelWidth,
                                                          timelineInput.getWidth() - L::timelineViewportRightGutter));
    const double fitPixelsPerSecond = width / std::max (L::timelineMinVisibleSeconds, timelineTotalSeconds);
    const double rate = appModel.context().projectLoaded && appModel.project().sampleRate.isValid()
                          ? appModel.project().sampleRate.hz : 0.0;
    if (fitPixelsPerSecond <= 0.0 || rate <= 0.0)
        return L::timelineZoomMax;
    return std::max (L::timelineZoomMax, (rate / L::timelineZoomSamplesPerPixelCeiling) / fitPixelsPerSecond);
}

void MainComponent::zoomTimelineAtAnchor (double anchorSeconds, double factor)
{
    pushZoomHistory();   // G2.16
    const double previousZoom = timelineZoomFactor;
    timelineZoomFactor = std::clamp (timelineZoomFactor * factor,
                                     yesdaw::ui::UiTheme::Layout::timelineZoomMin,
                                     timelineZoomCeiling());
    if (timelineZoomFactor != previousZoom)
    {
        const double zoomRatio = previousZoom / timelineZoomFactor;
        timelineScrollSeconds = anchorSeconds - (anchorSeconds - timelineScrollSeconds) * zoomRatio;
    }
    if (timelineZoomFactor == yesdaw::ui::UiTheme::Layout::timelineZoomMin)
        timelineScrollSeconds = yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds;
    refreshTimelineZoomReadout();
}

double MainComponent::timelinePixelsPerSecondFor (double totalSeconds) const noexcept
{
    const double fitPixelsPerSecond = static_cast<double> (juce::jmax (
                                          yesdaw::ui::UiTheme::Layout::timelineViewportMinPixelWidth,
                                          timelineInput.getWidth()
                                              - yesdaw::ui::UiTheme::Layout::timelineViewportRightGutter))
                                    / std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                totalSeconds);
    return fitPixelsPerSecond * timelineZoomFactor;
}

double MainComponent::timelineVisibleSecondsFor (double totalSeconds) const noexcept
{
    return static_cast<double> (juce::jmax (1, timelineInput.getWidth()))
         / std::max (1.0, timelinePixelsPerSecondFor (totalSeconds));
}

void MainComponent::followPlaybackPlayhead()
{
    if (! appModel.context().playheadFollowEnabled
        || ! appModel.context().isPlaying
        || ! appModel.project().sampleRate.isValid())
        return;

    const double visibleSeconds = timelineVisibleSecondsFor (timelineTotalSeconds);
    if (visibleSeconds <= 0.0)
        return;

    const double playheadSeconds = static_cast<double> (
                                       std::max<std::int64_t> (0, appModel.context().playheadFrame))
                                 / appModel.project().sampleRate.hz;
    if (appModel.context().playheadFollowContinuous)   // G2.16: the playhead stays at the middle
    {
        // Pro Tools' continuous scroll: the playhead runs from the left edge to the middle
        // and then the view moves under it; a playhead behind the window (a locate) recentres.
        if (playheadSeconds > timelineScrollSeconds + visibleSeconds * 0.5
            || playheadSeconds < timelineScrollSeconds)
            timelineScrollSeconds = std::max (0.0, playheadSeconds - visibleSeconds * 0.5);
        refreshTimelineScrollBars();
        return;
    }
    if (playheadSeconds >= timelineScrollSeconds + visibleSeconds)
    {
        const double elapsedPages = std::floor (
            (playheadSeconds - timelineScrollSeconds) / visibleSeconds);
        timelineScrollSeconds += std::max (1.0, elapsedPages) * visibleSeconds;
    }
    else if (playheadSeconds < timelineScrollSeconds)
    {
        const double pagesBack = std::ceil (
            (timelineScrollSeconds - playheadSeconds) / visibleSeconds);
        timelineScrollSeconds -= std::max (1.0, pagesBack) * visibleSeconds;
    }

    const double maxScroll = std::max (0.0, timelineTotalSeconds - visibleSeconds);
    timelineScrollSeconds = std::clamp (timelineScrollSeconds, 0.0, maxScroll);
}

void MainComponent::refreshAutomationLaneControls()
{
    constexpr yesdaw::ui::UiActionId action = yesdaw::ui::UiActionId::TimelineAutomationToggleTrackLane;
    const auto state = appModel.registry().stateFor (action, appModel.context());
    const bool timelineVisible = true;   // G2.1 cp2/cp3: the arrangement never leaves (no modal views)
    automationLaneToggle.setVisible (timelineVisible);
    automationLaneToggle.setEnabled (state.enabled);
    // V8: the zoom cluster lives and dies with the same toolbar row; the readout re-reads
    // the ONE shared zoom factor every gesture mutates.
    timelineZoomOutButton.setVisible (timelineVisible);
    timelineZoomInButton.setVisible (timelineVisible);
    timelineZoomSlider.setVisible (timelineVisible);   // G2.16
    timelineHScroll.setVisible (timelineVisible);
    timelineVScroll.setVisible (timelineVisible);
    timelineZoomReadout.setVisible (timelineVisible);
    statusLine.setVisible (timelineVisible);
    refreshTimelineZoomReadout();
    automationLaneToggle.setToggleState (appModel.context().timelineAutomationTrackLaneVisible,
                                         juce::dontSendNotification);
    const bool laneVisible = timelineVisible && appModel.context().timelineAutomationTrackLaneVisible;

    // E20/N4: rebuild the lane-target list for the automation-target track FIRST — the
    // header text and the add/delete button enablement below both read currentAutomationTarget(),
    // so they must see this frame's target, not the previous frame's stale options.
    refreshingAutomationTarget = true;
    automationTargetOptions = buildAutomationTargetOptions();
    if (selectedAutomationTargetIndex < 0
        || selectedAutomationTargetIndex >= static_cast<int> (automationTargetOptions.size()))
        selectedAutomationTargetIndex = 0;
    automationTargetChooser.clear (juce::dontSendNotification);
    for (std::size_t option = 0; option < automationTargetOptions.size(); ++option)
        automationTargetChooser.addItem (automationTargetOptions[option].label,
                                         static_cast<int> (option) + 1);
    if (! automationTargetOptions.empty())
        automationTargetChooser.setSelectedId (selectedAutomationTargetIndex + 1,
                                               juce::dontSendNotification);
    automationTargetChooser.setVisible (laneVisible);
    automationTargetChooser.setEnabled (laneVisible && ! automationTargetOptions.empty());

    // N5: the mode chooser reflects the persisted project.automationMode.
    automationModeChooser.setSelectedId (
        static_cast<int> (appModel.project().automationMode) + 1, juce::dontSendNotification);
    automationModeChooser.setVisible (laneVisible);
    automationModeChooser.setEnabled (laneVisible && appModel.context().projectLoaded);
    refreshingAutomationTarget = false;

    automationLaneRow.setText (automationLaneRowText(), juce::dontSendNotification);
    automationLaneRow.setVisible (laneVisible);
    automationLaneCanvas.setVisible (laneVisible);
    // N4: the lane's Y position (which track row it sits under) is part of the geometry law
    // now, not just its visibility — re-lay out on every refresh while visible so a track or
    // target switch (or a track-row scroll) moves it immediately, not just on the open/close
    // transition.
    if (laneVisible)
        layoutAutomationLaneControls();
    automationLaneLaidOutVisible = laneVisible;
    if (laneVisible)
        automationLaneCanvas.repaint();

    // N4: enablement and the click handlers below all key on the SAME selected target the
    // canvas already edits — never the first track's fader regardless of what is chosen.
    const AutomationTargetOption target = currentAutomationTarget();
    const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
        ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
        : nullptr;

    const auto addState = appModel.registry().stateFor (
        yesdaw::ui::UiActionId::TimelineAutomationAddBreakpoint,
        appModel.context());
    automationBreakpointAddButton.setVisible (laneVisible);
    automationBreakpointAddButton.setEnabled (laneVisible
                                              && addState.enabled
                                              && target.ownerEntity.isValid());

    const auto deleteState = appModel.registry().stateFor (
        yesdaw::ui::UiActionId::TimelineAutomationDeleteBreakpoint,
        appModel.context());
    automationBreakpointDeleteButton.setVisible (laneVisible);
    automationBreakpointDeleteButton.setEnabled (laneVisible
                                                 && deleteState.enabled
                                                 && lane != nullptr
                                                 && ! lane->points.empty());
}

juce::String MainComponent::automationLaneRowText() const
{
    const yesdaw::engine::Project& project = appModel.project();
    if (! appModel.context().projectLoaded || project.tracks.empty())
        return "No Track automation";

    // N4: name the REAL owner and the REAL target — automationTargetTrackId() is the same
    // track the target chooser and the canvas already edit, and currentAutomationTarget()
    // carries that target's own label ("Fader", "Pan", "Send: X", "FX1 ..."), never a
    // hardcoded "Track fader" regardless of what is actually selected.
    const yesdaw::engine::EntityId trackId = automationTargetTrackId();
    const yesdaw::engine::Track* track = nullptr;
    for (const yesdaw::engine::Track& candidate : project.tracks)
    {
        if (candidate.id == trackId)
        {
            track = &candidate;
            break;
        }
    }
    if (track == nullptr)
        return "No Track automation";

    const AutomationTargetOption target = currentAutomationTarget();
    const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
        ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
        : nullptr;

    const juce::String trackName = track->strip.name.empty() ? "Track 1" : juce::String (track->strip.name);
    const int breakpointCount = lane == nullptr ? 0 : static_cast<int> (lane->points.size());
    // R14: a bus-owned target's label already names the bus — the track prefix would lie.
    if (target.busOwned)
        return target.label + " - " + juce::String (breakpointCount) + " breakpoints";
    return trackName + " - " + target.label + " - " + juce::String (breakpointCount) + " breakpoints";
}

// N5: arm a Touch/Latch ride if the mode is armed AND the transport was already rolling when
// the drag started — moving a control while stopped, even in Touch/Latch mode, is just a
// normal edit (matches real-DAW semantics: Touch/Latch only writes DURING playback).
// R15: the ride owner is the selected TRACK or BUS strip (a bus strip's fader/pan ride
// writes the Bus roles), or an explicit owner (an FX insert's id for param rides); ONLY
// Touch/Latch arm — Read plays back, Off ignores lanes entirely and writes nothing.
void MainComponent::beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole role,
                                      std::uint32_t paramId,
                                      yesdaw::engine::EntityId ownerOverride)
{
    automationTouchRideActive = false;
    automationTouchRideSamples.clear();
    if (! appModel.context().projectLoaded || ! appModel.context().isPlaying)
        return;
    const yesdaw::engine::AutomationMode mode = appModel.project().automationMode;
    if (mode != yesdaw::engine::AutomationMode::Touch
        && mode != yesdaw::engine::AutomationMode::Latch)
        return;

    yesdaw::engine::EntityId ownerId = ownerOverride;
    if (! ownerId.isValid())
    {
        ownerId = appModel.selectedSendOwnerEntityId();
        if (! ownerId.isValid())
            return;

        if (appModel.project().findBus (ownerId) != nullptr)
        {
            if (role == yesdaw::engine::AutomationTargetRole::TrackFader)
                role = yesdaw::engine::AutomationTargetRole::BusFader;
            else if (role == yesdaw::engine::AutomationTargetRole::TrackPan)
                role = yesdaw::engine::AutomationTargetRole::BusPan;
        }
    }

    automationTouchRideActive = true;
    automationTouchRideRole = role;
    automationTouchRideParamId = paramId;
    automationTouchRideTrackId = ownerId;
}

// N5: sample the live playhead tick and the control's current value into the ride buffer.
// Deliberately does NOT touch project_/adoptEditedProject — every edit adoption resets the
// transport to stopped (resetContextForFreshPlayback), so committing per-tick would collapse
// every point in the ride to tick 0 after the very first write. Buffering client-side and
// committing once, at the end of the ride, is what makes "breakpoints across a moved span"
// possible at all.
void MainComponent::recordAutomationTouchSample (double normalizedValue)
{
    if (! automationTouchRideActive || ! appModel.project().sampleRate.isValid())
        return;

    const yesdaw::engine::Tick tick = static_cast<yesdaw::engine::Tick> (
        std::max<std::int64_t> (0, appModel.context().playheadFrame));
    // G4.1 cp2: one sample per tick — the painted drags sample on the release too (the live slider
    // spoke only on a value change), and a second breakpoint at one tick refuses the whole commit.
    if (! automationTouchRideSamples.empty() && automationTouchRideSamples.back().tick == tick)
    {
        automationTouchRideSamples.back().value = normalizedValue;
        return;
    }
    automationTouchRideSamples.push_back ({ tick, normalizedValue });
}

// N5: commit the whole buffered ride as ONE undo step (the actual project write happens
// here, and only here — see recordAutomationTouchSample's note on why).
void MainComponent::endAutomationTouchRideIfActive()
{
    if (! automationTouchRideActive)
        return;

    automationTouchRideActive = false;
    if (! automationTouchRideSamples.empty())
        (void) appModel.commitAutomationTouchRide (
            automationTouchRideTrackId, automationTouchRideRole, automationTouchRideParamId,
            automationTouchRideSamples);
    automationTouchRideSamples.clear();
    refreshActionState();
    repaintAll();
}

void MainComponent::drawTrackList (juce::Graphics& g, juce::Rectangle<int> area) const
{
    fillPanel (g, area);
    auto header = area.removeFromTop (yesdaw::ui::UiTheme::Layout::trackListHeaderHeight);
    drawSmallLabel (g,
                    "TRACKS",
                    header.reduced (yesdaw::ui::UiTheme::Layout::trackListHeaderInsetX,
                                    yesdaw::ui::UiTheme::Layout::trackListHeaderInsetY)
                        .withHeight (yesdaw::ui::UiTheme::Layout::trackListHeaderLabelHeight));

    if (! appModel.context().projectLoaded || appModel.project().tracks.empty())
    {
        drawSmallLabel (g,
                        "No Project",
                        area.reduced (yesdaw::ui::UiTheme::Layout::trackListEmptyLabelInset),
                        juce::Justification::centred);
        return;
    }

    // N6: row heights come from the SAME cumulative law rowBounds/rowAt use — a resized row
    // paints at exactly the height/position hit-testing agrees on.
    const int rowCount = static_cast<int> (appModel.project().tracks.size());
    const yesdaw::ui::CumulativeRowGeometry rowLaw = trackListInput.rowGeometry (rowCount, area.getHeight());
    // Vertical track scroll (E5): the rail paints from its effective (clamped) shared row
    // offset; scrolled-out rows above the window are skipped so paint matches rowBounds/rowAt.
    for (std::size_t i = static_cast<std::size_t> (trackListInput.effectiveScrollRows());
         i < appModel.project().tracks.size(); ++i)
    {
        const int rowHeight = static_cast<int> (std::llround (rowLaw.heightFor (static_cast<int> (i))));
        auto row = area.removeFromTop (rowHeight);
        if (row.getHeight() < rowHeight)
            break;
        const auto& projectTrack = appModel.project().tracks[i];
        const juce::String fallbackName = "Track " + juce::String (static_cast<int> (i + 1));
        const juce::String trackName = projectTrack.strip.name.empty()
                                           ? fallbackName
                                           : juce::String (projectTrack.strip.name);
        // N7: a customized colour overrides the historical fixed purple everywhere this
        // variable is used below (accent bar / swatch, glyph tint, pan indicator, level fill).
        const juce::Colour trackColour = colourForTrack (projectTrack, kPurple);

        const auto rowSurface = row.reduced (
            yesdaw::ui::UiTheme::Layout::trackListRowHorizontalInset,
            yesdaw::ui::UiTheme::Layout::trackListRowVerticalInset);
        juce::ColourGradient rowGradient (
            trackLaneIsSelected (static_cast<int> (i))   // G2.17: every selected lane highlights
                ? yesdaw::ui::UiTheme::Color::selectedLane()
                : yesdaw::ui::UiTheme::Color::panelRaised(),
            static_cast<float> (rowSurface.getX()),
            static_cast<float> (rowSurface.getCentreY()),
            yesdaw::ui::UiTheme::Color::darkControl(),
            static_cast<float> (rowSurface.getRight()),
            static_cast<float> (rowSurface.getCentreY()),
            false);
        g.setGradientFill (rowGradient);
        g.fillRect (rowSurface);
        g.setColour (trackColour);
        g.fillRect (row.withWidth (yesdaw::ui::UiTheme::Layout::trackListAccentWidth)
                         .reduced (yesdaw::ui::UiTheme::Layout::trackListAccentHorizontalInset,
                                   yesdaw::ui::UiTheme::Layout::trackListAccentVerticalInset));
        g.setColour (kPanelStroke);
        g.fillRect (row.removeFromBottom (yesdaw::ui::UiTheme::Layout::trackListSeparatorHeight));

        yesdaw::ui::drawTrackGlyph (
            g,
            i,
            juce::Rectangle<float> (
                static_cast<float> (row.getX() + yesdaw::ui::UiTheme::Layout::trackListIconLeftInset),
                static_cast<float> (row.getY() + yesdaw::ui::UiTheme::Layout::trackListIconTopInset),
                static_cast<float> (yesdaw::ui::UiTheme::Layout::trackListIconSize),
                static_cast<float> (yesdaw::ui::UiTheme::Layout::trackListIconSize)),
            trackColour.withAlpha (yesdaw::ui::UiTheme::Tone::trackIconAlpha));
        // G2.17: the kind badge — MIDI when the track holds MIDI clips, audio otherwise.
        yesdaw::ui::drawTrackKindBadge (
            g,
            trackHoldsMidi (i),
            juce::Rectangle<float> (
                static_cast<float> (row.getX() + yesdaw::ui::UiTheme::Layout::trackListIconLeftInset
                                    + yesdaw::ui::UiTheme::Layout::trackListIconSize + yesdaw::ui::UiTheme::Layout::trackListKindBadgeGap),
                static_cast<float> (row.getY() + yesdaw::ui::UiTheme::Layout::trackListIconTopInset
                                    + yesdaw::ui::UiTheme::Layout::trackListIconSize - yesdaw::ui::UiTheme::Layout::trackListKindBadgeSize),
                static_cast<float> (yesdaw::ui::UiTheme::Layout::trackListKindBadgeSize),
                static_cast<float> (yesdaw::ui::UiTheme::Layout::trackListKindBadgeSize)),
            trackColour.withAlpha (yesdaw::ui::UiTheme::Tone::trackIconAlpha));

        auto mixSummary = row.withRight (
                                 row.getRight()
                                 - yesdaw::ui::UiTheme::Layout::trackListMixSummaryRightInset)
                              .removeFromRight (
                                  yesdaw::ui::UiTheme::Layout::trackListMixSummaryWidth)
                              .reduced (
                                  yesdaw::ui::UiTheme::Space::none,
                                  yesdaw::ui::UiTheme::Layout::trackListMixSummaryVerticalInset);
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (mixSummary.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
        g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
            yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
        g.drawRoundedRectangle (
            mixSummary.toFloat().reduced (
                yesdaw::ui::UiTheme::Layout::panelOutlineInset),
            yesdaw::ui::UiTheme::Radius::sm,
            yesdaw::ui::UiTheme::Layout::panelOutlineStrokeWidth);
        g.setColour (yesdaw::ui::UiTheme::Color::faintText());
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::tiny,
            juce::Font::bold));
        auto mixLabel = mixSummary.withTrimmedLeft (
                                      yesdaw::ui::UiTheme::Layout::trackListMixLabelLeftInset)
                            .withWidth (
                                yesdaw::ui::UiTheme::Layout::trackListMixLabelWidth)
                            .withHeight (
                                yesdaw::ui::UiTheme::Layout::trackListMixLabelHeight);
        g.drawText ("PAN",
                    mixLabel.translated (
                        yesdaw::ui::UiTheme::Space::none,
                        yesdaw::ui::UiTheme::Layout::trackListPanLabelTopInset),
                    juce::Justification::centredLeft,
                    false);
        g.drawText ("VOL",
                    mixLabel.translated (
                        yesdaw::ui::UiTheme::Space::none,
                        yesdaw::ui::UiTheme::Layout::trackListVolumeLabelTopInset),
                    juce::Justification::centredLeft,
                    false);

        auto pan = row.withRight (
                          row.getRight() - yesdaw::ui::UiTheme::Layout::trackListPanRightInset)
                       .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListPanDiameter)
                       .withY (row.getY() + yesdaw::ui::UiTheme::Layout::trackListPanTopInset)
                       .withHeight (yesdaw::ui::UiTheme::Layout::trackListPanDiameter);
        g.setColour (yesdaw::ui::UiTheme::Color::panelShadow().withAlpha (
            yesdaw::ui::UiTheme::Tone::shadowAlpha));
        g.fillEllipse (pan.toFloat().translated (
            0.0f,
            static_cast<float> (yesdaw::ui::UiTheme::Layout::controlShadowOffset)));
        g.setColour (yesdaw::ui::UiTheme::Color::knobFace());
        g.fillEllipse (pan.toFloat());
        g.setColour (yesdaw::ui::UiTheme::Color::knobArc());
        g.drawEllipse (pan.toFloat().reduced (yesdaw::ui::UiTheme::Layout::controlOutlineInset),
                       yesdaw::ui::UiTheme::Layout::iconFineStrokeWidth);
        // Live pan (usable-DAW P2): the knob indicator swings with strip.pan and the readout
        // shows C / L% / R%.
        const float panValue = juce::jlimit (-1.0f, 1.0f, projectTrack.strip.pan);
        const float panAngle = panValue * yesdaw::ui::UiTheme::Layout::trackListPanArcRadians;
        const float panRadius =
            static_cast<float> (pan.getCentreY()
                                - pan.getY()
                                - yesdaw::ui::UiTheme::Layout::trackListPanIndicatorInset);
        const juce::Point<float> panCentre = pan.toFloat().getCentre();
        g.setColour (trackColour);
        g.drawLine (panCentre.x,
                    panCentre.y,
                    panCentre.x + panRadius * std::sin (panAngle),
                    panCentre.y - panRadius * std::cos (panAngle),
                    yesdaw::ui::UiTheme::Layout::iconBoldStrokeWidth);
        const int panPercent = juce::roundToInt (std::abs (panValue) * 100.0f);
        const juce::String panText =
            panPercent == 0 ? juce::String ("C")
                            : (panValue < 0.0f ? juce::String ("L") : juce::String ("R"))
                                  + juce::String (panPercent);
        g.setColour (kMutedText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::tiny));
        g.drawText (panText,
                    pan.withY (
                           row.getY()
                           + yesdaw::ui::UiTheme::Layout::trackListPanValueTopInset)
                        .withHeight (
                            yesdaw::ui::UiTheme::Layout::trackListMixLabelHeight),
                    juce::Justification::centred,
                    false);

        // V5: the mini VOL is a VERTICAL fader (top = loud), the same rect law
        // volumeSliderBounds hit-tests and the same orientation the mixer strip fader uses.
        auto level = row.withRight (
                            row.getRight()
                            - yesdaw::ui::UiTheme::Layout::trackListLevelColumnRightInset)
                         .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListLevelColumnWidth)
                         .reduced (yesdaw::ui::UiTheme::Space::none,
                                   yesdaw::ui::UiTheme::Layout::trackListLevelColumnVerticalInset);
        g.setColour (yesdaw::ui::UiTheme::Color::meterTrack().withAlpha (
            yesdaw::ui::UiTheme::Tone::trackSliderRailAlpha));
        g.fillRoundedRectangle (level.toFloat(), yesdaw::ui::UiTheme::Radius::pill);
        const int liveHeight = juce::roundToInt (
            static_cast<float> (level.getHeight()) * projectTrack.strip.linearGain);
        g.setColour (trackColour.withAlpha (yesdaw::ui::UiTheme::Tone::trackSliderFillAlpha));
        g.fillRoundedRectangle (
            level.withTop (level.getBottom() - liveHeight).toFloat(),
            yesdaw::ui::UiTheme::Radius::pill);
        auto levelThumb = level.withHeight (yesdaw::ui::UiTheme::Layout::trackListLevelThumbHeight)
                              .withY (level.getBottom() - liveHeight
                                      - yesdaw::ui::UiTheme::Layout::trackListLevelThumbHeight / 2);
        g.setColour (yesdaw::ui::UiTheme::Color::faderThumbTop());
        g.fillRoundedRectangle (levelThumb.toFloat(), yesdaw::ui::UiTheme::Radius::sm);

        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::font (
            yesdaw::ui::UiTheme::Type::title,
            juce::Font::bold));
        // G0.7 cp2: the name cell ends where the mix cluster (PAN/VOL, knob, level, meter)
        // begins; a long name ellipsises instead of running under it.
        g.drawText (trackName,
                    row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListNameLeftInset)
                        .withRight (row.getRight() - yesdaw::ui::UiTheme::Layout::trackListMixSummaryRightInset
                                    - yesdaw::ui::UiTheme::Layout::trackListMixSummaryWidth
                                    - yesdaw::ui::UiTheme::Layout::trackListButtonInsetX)
                        .withHeight (yesdaw::ui::UiTheme::Layout::trackListNameHeight)
                        .translated (yesdaw::ui::UiTheme::Layout::trackListNameOffsetX,
                                     yesdaw::ui::UiTheme::Layout::trackListNameOffsetY),
                    juce::Justification::centredLeft, true);

        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::readout));
        g.drawText (juce::String (static_cast<int> (i + 1)),
                    row.withWidth (yesdaw::ui::UiTheme::Layout::trackListNumberWidth),
                    juce::Justification::centred,
                    false);

        // Live M/S cells (usable-DAW P2): the painted cells reflect the strip state; the rail
        // input layer toggles them through the same verbs as the mixer.
        auto buttonsArea = row.withTrimmedLeft (yesdaw::ui::UiTheme::Layout::trackListNameLeftInset)
                               .withTrimmedTop (yesdaw::ui::UiTheme::Layout::trackListButtonsTop)
                               .withHeight (yesdaw::ui::UiTheme::Layout::trackListButtonsHeight);
        // E30: the "O" cell is the REAL record-arm badge — lit red on the armed track.
        // M11: EVERY armed track's badge lights, not just the primary's.
        const bool rowArmed = appModel.isRecordingTrackIndexArmed (i);
        const std::array<std::pair<const char*, bool>, 3> railCells {{
            { "M", projectTrack.strip.muted },
            { "S", projectTrack.strip.soloed },
            { "O", rowArmed },
        }};
        for (const auto& [label, active] : railCells)
        {
            const bool armCell = label == std::string ("O");
            auto cell = buttonsArea.removeFromLeft (yesdaw::ui::UiTheme::Layout::trackListButtonWidth)
                            .reduced (yesdaw::ui::UiTheme::Layout::trackListButtonInsetX,
                                      yesdaw::ui::UiTheme::Layout::trackListButtonInsetY);
            g.setColour (active ? (armCell ? kRed : trackColour)
                                : yesdaw::ui::UiTheme::Color::mixerBack());
            g.fillRoundedRectangle (cell.toFloat(), yesdaw::ui::UiTheme::Radius::sm);
            g.setColour (active ? kText : (armCell ? kRed : kMutedText));
            g.setFont (yesdaw::ui::UiTheme::Type::font (
                yesdaw::ui::UiTheme::Type::caption,
                juce::Font::bold));
            g.drawText (label, cell, juce::Justification::centred, false);
        }

        // Live meter (usable-DAW P2 + B32 + V5): the rail meter renders INDEPENDENT L/R
        // columns from the MeterNode's per-channel peaks (the node taps post-pan, so a
        // hard-panned track honestly meters one-sided); each column runs the shared
        // hold/clip-latch law and a click on the zone clears both.
        auto meter = row.withRight (row.getRight() - yesdaw::ui::UiTheme::Layout::trackListMeterRightInset)
                         .removeFromRight (yesdaw::ui::UiTheme::Layout::trackListMeterWidth)
                         .reduced (yesdaw::ui::UiTheme::Layout::trackListMeterHorizontalInset,
                                   yesdaw::ui::UiTheme::Layout::trackListMeterVerticalInset);
        const std::array<MeterHoldState, 2> railHoldLR =
            i < trackMeterHoldLR.size() ? trackMeterHoldLR[i]
                                        : std::array<MeterHoldState, 2> {};
        const int channelWidth =
            (meter.getWidth() - yesdaw::ui::UiTheme::Layout::trackListMeterChannelGap) / 2;
        const auto meterLeft = meter.withWidth (channelWidth);
        const auto meterRight = meter.withTrimmedLeft (
            meter.getWidth() - channelWidth);
        drawMeterWithHold (g, meterLeft, railHoldLR[0].livePeak, railHoldLR[0].heldPeak,
                           railHoldLR[0].clipLatched);
        drawMeterWithHold (g, meterRight, railHoldLR[1].livePeak, railHoldLR[1].heldPeak,
                           railHoldLR[1].clipLatched);
    }
}

yesdaw::ui::TimelineCanvasState MainComponent::makeTimelineState()
{
    rebuildTimelineClipViews();

    yesdaw::ui::TimelineCanvasState state;
    state.activeTool = appModel.context().activeTimelineTool;   // the strip lights this cell
    if (! appModel.context().projectLoaded)
    {
        state.tracks = nullptr;
        state.trackCount = 0;
        state.clips = nullptr;
        state.clipStyles = nullptr;
        state.clipCount = 0;
        state.totalSeconds = yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds;
        state.playheadSeconds = yesdaw::ui::UiTheme::Layout::timelineInitialPlayheadSeconds;
    }
    else
    {
        state.tracks = projectTimelineTracks.data();
        state.trackCount = static_cast<int> (projectTimelineTracks.size());
        state.clips = timelineClips.data();
        state.clipStyles = timelineClipStyles.data();
        state.clipCount = static_cast<int> (timelineClips.size());
        state.clipNotes = timelineClipNotes.empty() ? nullptr : timelineClipNotes.data();
        state.clipNoteCount = static_cast<int> (timelineClipNotes.size());
        state.waveformCacheLookup = [this] (int layoutClipId)
            -> std::shared_ptr<const yesdaw::persistence::WaveformPeakCache>
        {
            if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipAssetHashes.size()))
                return {};

            return appModel.waveformService().tryGetReady (
                timelineClipAssetHashes[static_cast<std::size_t> (layoutClipId)]);
        };
        // G2.19: decoded samples for the zoomed-in paint — the same buffers playback reads.
        state.waveformSampleLookup = [this] (int layoutClipId) -> yesdaw::ui::WaveformSampleSource
        {
            if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipAssetIds.size()))
                return {};
            const yesdaw::ui::UiDecodedAsset* decoded =
                appModel.findDecodedAsset (timelineClipAssetIds[static_cast<std::size_t> (layoutClipId)]);
            if (decoded == nullptr)
                return {};
            return { std::span<const float> (decoded->interleavedSamples.data(), decoded->interleavedSamples.size()),
                     decoded->channels, decoded->frames };
        };
        state.totalSeconds = timelineTotalSeconds;
        state.rowZoom = timelineRowZoom;   // G2.16
        state.playheadSeconds = appModel.project().sampleRate.isValid()
                                    ? static_cast<double> (appModel.context().playheadFrame)
                                        / appModel.project().sampleRate.hz
                                    : yesdaw::ui::UiTheme::Layout::timelineInitialPlayheadSeconds;
    }

    // V4: the ruler's bar length comes from the SAME head tempo/meter read the transport
    // readout uses, through the SAME engine grid law (sampleRateHz = 1.0 makes computeBarGrid
    // yield seconds) — the ruler's bar numbers and the header's bar|beat share one law.
    {
        const HeadTempoMeter head = headTempoMeter();
        state.barSeconds =
            yesdaw::engine::computeBarGrid (head.bpm, head.numerator, head.denominator, 1.0)
                .barFrames;
    }
    // G2.2: the time row follows the app-wide time display (the header counter's law).
    state.timeDisplayMode = timeDisplayMode;
    state.sampleRateHz = appModel.project().sampleRate.isValid() ? appModel.project().sampleRate.hz : 48000.0;

    timelineMarkerLabels.clear();
    timelineMarkerViews.clear();
    if (appModel.context().projectLoaded && appModel.project().sampleRate.isValid())
    {
        const double sampleRateHz = appModel.project().sampleRate.hz;
        timelineMarkerLabels.reserve (appModel.project().markers.size());
        timelineMarkerViews.reserve (appModel.project().markers.size());
        for (const yesdaw::engine::Marker& marker : appModel.project().markers)
        {
            timelineMarkerLabels.push_back (marker.name);
            timelineMarkerViews.push_back ({ static_cast<double> (marker.tick) / sampleRateHz,
                                             timelineMarkerLabels.back().c_str(),
                                             marker.colour });   // G2.14
        }
    }
    state.markers = timelineMarkerViews.empty() ? nullptr : timelineMarkerViews.data();
    state.markerCount = static_cast<int> (timelineMarkerViews.size());

    // G2.15: the tempo and meter changes after the head, placed by the compiled map (so a ramp
    // puts the next change where it really lands), labelled "120" / "120~" / "3/4".
    timelineMapLabelTexts.clear();
    timelineMapLabelViews.clear();
    timelineMapLabelFrames.clear();
    {
        yesdaw::engine::CompiledTempoMap compiled;
        if (appModel.context().projectLoaded && appModel.project().sampleRate.isValid()
            && appModel.compiledTempoMap (compiled))
        {
            const double sampleRateHz = appModel.project().sampleRate.hz;
            const auto& tempoMap = appModel.project().tempoMap;
            const auto& meterMap = appModel.project().meterMap;
            timelineMapLabelTexts.reserve (tempoMap.size() + meterMap.size());
            timelineMapLabelViews.reserve (tempoMap.size() + meterMap.size());
            for (std::size_t i = 1; i < tempoMap.size(); ++i)
            {
                double frame = 0.0;
                if (! compiled.frameForTick (tempoMap[i].tick, frame))
                    continue;
                timelineMapLabelTexts.push_back (juce::String (juce::roundToInt (tempoMap[i].bpm)).toStdString()
                                                 + (tempoMap[i].curveToNext == yesdaw::engine::TempoCurve::LinearRamp ? "~" : ""));
                timelineMapLabelViews.push_back ({ frame / sampleRateHz, timelineMapLabelTexts.back().c_str() });
                timelineMapLabelFrames.push_back (static_cast<std::int64_t> (std::llround (frame)));
            }
            for (std::size_t i = 1; i < meterMap.size(); ++i)
            {
                double frame = 0.0;
                if (! compiled.frameForTick (meterMap[i].tick, frame))
                    continue;
                timelineMapLabelTexts.push_back (std::to_string (meterMap[i].numerator) + "/" + std::to_string (meterMap[i].denominator));
                timelineMapLabelViews.push_back ({ frame / sampleRateHz, timelineMapLabelTexts.back().c_str() });
                timelineMapLabelFrames.push_back (static_cast<std::int64_t> (std::llround (frame)));
            }
        }
    }
    state.mapLabels = timelineMapLabelViews.empty() ? nullptr : timelineMapLabelViews.data();
    state.mapLabelCount = static_cast<int> (timelineMapLabelViews.size());

    // N4: the automation lane anchors under the SAME track automationTargetTrackId() resolves
    // (identical clamp), so the band's position can never disagree with the header/canvas
    // about which track is being edited.
    state.automationLaneVisible = appModel.context().timelineAutomationTrackLaneVisible;
    state.automationLaneTrackRow = appModel.context().projectLoaded && ! appModel.project().tracks.empty()
        ? std::clamp (selectedTrackLane, 0, static_cast<int> (appModel.project().tracks.size()) - 1)
        : -1;

    // Ruler range selection (parity item 25): painted from the model's transient range frames.
    if (appModel.context().timelineRangeSelected
        && appModel.context().projectLoaded
        && appModel.project().sampleRate.isValid())
    {
        const double sampleRateHz = appModel.project().sampleRate.hz;
        state.rangeSelectionActive = true;
        state.rangeStartSeconds = static_cast<double> (appModel.timelineRangeStartFrame()) / sampleRateHz;
        state.rangeEndSeconds = static_cast<double> (appModel.timelineRangeEndFrame()) / sampleRateHz;
    }

    // Live zoom + horizontal scroll (usable-DAW P1): zoom scales the fit-to-window density and
    // the scroll offset is clamped so the view never runs past the timeline end.
    state.viewport.pixelsPerSecond = timelinePixelsPerSecondFor (state.totalSeconds);
    const double visibleSeconds = timelineVisibleSecondsFor (state.totalSeconds);
    const double maxScroll = std::max (0.0, state.totalSeconds - visibleSeconds);
    timelineScrollSeconds = std::clamp (timelineScrollSeconds, 0.0, maxScroll);
    state.viewport.scrollSeconds = timelineScrollSeconds;
    // Vertical track scroll (E5): geometry clamps the shared row offset per paint/gesture.
    state.trackScrollRows = timelineTrackScrollRows;

    // Transport loop brace (E6): painted and hit-tested from the real transport loop.
    if (appModel.context().loopEnabled
        && appModel.context().projectLoaded
        && appModel.project().sampleRate.isValid())
    {
        const std::int64_t loopStart = appModel.playbackLoopStartFrame();
        const std::int64_t loopEnd = appModel.playbackLoopEndFrame();
        if (loopEnd > loopStart && loopStart >= 0)
        {
            state.loopActive = true;
            state.loopStartSeconds = static_cast<double> (loopStart) / appModel.project().sampleRate.hz;
            state.loopEndSeconds = static_cast<double> (loopEnd) / appModel.project().sampleRate.hz;
        }
    }

    // N8: the persisted punch region — painted from the real Project field, so an unset
    // region paints nothing (bit-identical to before this field existed).
    if (appModel.context().projectLoaded && appModel.project().sampleRate.isValid())
    {
        const yesdaw::engine::PunchRegion punch = appModel.punchRegion();
        if (punch.enabled && punch.endFrame > punch.startFrame)
        {
            state.punchActive = true;
            state.punchStartSeconds = static_cast<double> (punch.startFrame) / appModel.project().sampleRate.hz;
            state.punchEndSeconds = static_cast<double> (punch.endFrame) / appModel.project().sampleRate.hz;
        }
    }
    return state;
}

void MainComponent::rebuildTimelineClipViews()
{
    timelineClips.clear();
    timelineClipNotes.clear();
    timelineClipStyles.clear();
    timelineClipIds.clear();
    timelineClipAssetHashes.clear();
    timelineClipAssetIds.clear();
    projectTimelineTracks.clear();

    const yesdaw::engine::Project& project = appModel.project();
    if (! appModel.context().projectLoaded || ! project.sampleRate.isValid())
    {
        timelineTotalSeconds = yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds;
        return;
    }

    projectTimelineTracks.reserve (project.tracks.size());
    for (const yesdaw::engine::Track& track : project.tracks)
        projectTimelineTracks.push_back ({ track.strip.name.empty() ? "Track" : track.strip.name.c_str(),
                                           colourForTrack (track, kPurple),
                                           0.0f,
                                           track.heightPx });

    double endSeconds = 0.0;
    const double sampleRate = project.sampleRate.hz;

    for (const yesdaw::engine::Clip& clip : project.clips)
    {
        const yesdaw::engine::Asset* const asset = project.findAsset (clip.assetId);
        if (! clip.id.isValid()
            || clip.timelineStart < 0
            || clip.timelineLength <= 0
            || asset == nullptr)
        {
            continue;
        }

        const auto track = std::find_if (project.tracks.begin(), project.tracks.end(), [&clip] (const auto& candidate) {
            return candidate.id == clip.trackId;
        });
        if (track == project.tracks.end())
            continue;

        const int lane = static_cast<int> (std::distance (project.tracks.begin(), track));
        const double startSeconds = static_cast<double> (clip.timelineStart) / sampleRate;
        const double lengthSeconds = static_cast<double> (clip.timelineLength) / sampleRate;
        const int id = static_cast<int> (timelineClips.size());
        // The clip's source window rides along (srcOffset / srcLen, asset frames) so the
        // painter draws THIS clip's audio, not the whole file squeezed into its width.
        timelineClips.push_back ({ id, lane, startSeconds, lengthSeconds, clip.name.c_str(),
                                   clip.srcOffset, clip.srcLen });
        // V6: selection is a painted RING, not a colour swap — the clip keeps its N7 track
        // colour while selected (the old accent-blue swap was invisible on a blue track,
        // the exact false-positive risk the N7 gate had to work around).
        timelineClipStyles.push_back ({ clip.colour != yesdaw::engine::kTrackColourUnset
                                            ? juce::Colour (clip.colour)                 // G2.12: the clip's own colour
                                            : colourForTrack (*track, kPurple),
                                        yesdaw::ui::UiTheme::Tone::mainComponentProjectClipAlpha,
                                        appModel.isTimelineClipSelected (clip.id),
                                        static_cast<long long> (clip.timelineLength),
                                        static_cast<long long> (clip.fadeIn),
                                        static_cast<long long> (clip.fadeOut),
                                        static_cast<int> (clip.fadeInShape),      // G2.10
                                        static_cast<int> (clip.fadeOutShape),
                                        clip.fadeInCurve,
                                        clip.fadeOutCurve,
                                        clip.muted,      // G2.12
                                        clip.reversed });   // G2.13
        timelineClipIds.push_back (clip.id);
        timelineClipAssetHashes.push_back (asset->contentHash);
        timelineClipAssetIds.push_back (asset->id);
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    // MIDI clips are first-class timeline citizens (E8): painted on their track lanes in the
    // MIDI accent colour and hit-testable through the same layout ids as audio clips.
    for (const yesdaw::engine::MidiClip& midiClip : project.midiClips)
    {
        if (! midiClip.id.isValid() || midiClip.timelineStart < 0 || midiClip.timelineLength <= 0)
            continue;

        const auto track = std::find_if (project.tracks.begin(), project.tracks.end(), [&midiClip] (const auto& candidate) {
            return candidate.id == midiClip.trackId;
        });
        if (track == project.tracks.end())
            continue;

        const int lane = static_cast<int> (std::distance (project.tracks.begin(), track));
        const double startSeconds = static_cast<double> (midiClip.timelineStart) / sampleRate;
        const double lengthSeconds = static_cast<double> (midiClip.timelineLength) / sampleRate;
        const int id = static_cast<int> (timelineClips.size());
        timelineClips.push_back ({ id, lane, startSeconds, lengthSeconds, "MIDI" });
        // M7: hand the canvas this clip's real notes so it can paint what the clip CONTAINS
        // instead of falling through to the placeholder waveform.
        for (const yesdaw::engine::Note& note : midiClip.notes)
        {
            double noteStartFrame = 0.0;
            double noteEndFrame = 0.0;
            if (! yesdaw::engine::tickToFrame (
                    yesdaw::engine::TempoMapView { project.tempoMap.data(), project.tempoMap.size() },
                    project.sampleRate,
                    midiClip.timelineStart + note.startTick,
                    noteStartFrame)
                || ! yesdaw::engine::tickToFrame (
                    yesdaw::engine::TempoMapView { project.tempoMap.data(), project.tempoMap.size() },
                    project.sampleRate,
                    midiClip.timelineStart + note.startTick + note.lengthTicks,
                    noteEndFrame))
            {
                continue;
            }

            timelineClipNotes.push_back ({ id,
                                           noteStartFrame / sampleRate,
                                           std::max (0.0, (noteEndFrame - noteStartFrame) / sampleRate),
                                           static_cast<int> (note.key) });
        }
        // V6: same ring law as audio clips; MIDI clips have no fade model, so the fade tick
        // fields stay honestly zero (nothing paints).
        timelineClipStyles.push_back ({ colourForTrack (*track, yesdaw::ui::UiTheme::Color::accentCyan()),
                                        yesdaw::ui::UiTheme::Tone::mainComponentProjectClipAlpha,
                                        appModel.isTimelineClipSelected (midiClip.id),
                                        static_cast<long long> (midiClip.timelineLength),
                                        0,
                                        0,
                                        1, 1, 0.0f, 0.0f,
                                        midiClip.muted,   // G3.5: the same dim wash an audio clip gets
                                        false });
        timelineClipIds.push_back (midiClip.id);
        timelineClipAssetHashes.push_back ({});
        endSeconds = std::max (endSeconds, startSeconds + lengthSeconds);
    }

    timelineTotalSeconds = timelineClips.empty()
        ? yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds
        : std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                    endSeconds * yesdaw::ui::UiTheme::Layout::timelineProjectEndPaddingScale);
}

void MainComponent::selectTimelineClipByLayoutId (int layoutClipId, bool toggle)
{
    dismissClipRenameEditor();
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
    {
        appModel.clearTimelineClipSelection();
    }
    else
    {
        (void) appModel.selectTimelineClipForGesture (
            timelineClipIds[static_cast<std::size_t> (layoutClipId)], toggle);
    }

    refreshActionState();
    repaintAll();
}

yesdaw::engine::EntityId MainComponent::automationTargetTrackId() const noexcept
{
    const auto& tracks = appModel.project().tracks;
    if (tracks.empty())
        return {};

    const int lane = selectedTrackLane >= 0 && selectedTrackLane < static_cast<int> (tracks.size())
        ? selectedTrackLane
        : int {};
    return tracks[static_cast<std::size_t> (lane)].id;
}

// E20: enumerate the selected track's automation targets in a stable order — fader, pan,
// each send level, then each FX param of each insert.
std::vector<MainComponent::AutomationTargetOption> MainComponent::buildAutomationTargetOptions() const
{
    std::vector<AutomationTargetOption> options;
    const yesdaw::engine::EntityId trackId = automationTargetTrackId();
    if (! trackId.isValid())
        return options;

    options.push_back ({ yesdaw::engine::AutomationTargetRole::TrackFader,
                         yesdaw::engine::FaderNode::kGainParameterId, trackId, "Fader" });
    options.push_back ({ yesdaw::engine::AutomationTargetRole::TrackPan,
                         yesdaw::engine::PanNode::kPanParameterId, trackId, "Pan" });

    const yesdaw::engine::Track* track = nullptr;
    for (const yesdaw::engine::Track& candidate : appModel.project().tracks)
        if (candidate.id == trackId)
            track = &candidate;
    if (track == nullptr)
        return options;

    for (std::size_t sendIndex = 0; sendIndex < track->sends.size(); ++sendIndex)
    {
        juce::String busName ("Bus?");
        for (const auto& bus : appModel.project().buses)
            if (bus.id == track->sends[sendIndex].busId)
                busName = juce::String (bus.strip.name);
        options.push_back ({ yesdaw::engine::AutomationTargetRole::SendLevel,
                             static_cast<std::uint32_t> (sendIndex), trackId,
                             "Send: " + busName });
    }

    for (std::size_t slot = 0; slot < track->strip.fxChain.size(); ++slot)
    {
        const yesdaw::engine::FxInsert& insert = track->strip.fxChain[slot];
        for (std::uint32_t paramId = 0;
             paramId < yesdaw::ui::UiTheme::Layout::mixerFxParamProbeLimit;
             ++paramId)
        {
            if (! yesdaw::engine::fxKindAcceptsParameterId (insert.kind, paramId))
                continue;

            const yesdaw::engine::ParamSpec spec =
                yesdaw::engine::fxParamSpecForKind (insert.kind, paramId);
            options.push_back ({ yesdaw::engine::AutomationTargetRole::FxInsertParam,
                                 paramId, insert.id,
                                 "FX" + juce::String (static_cast<int> (slot) + 1)
                                     + " " + spec.name });
        }
    }

    // G3.1: the Track's instrument parameters (when it holds MIDI — the instrument exists).
    {
        bool holdsMidi = false;
        for (const yesdaw::engine::MidiClip& clip : appModel.project().midiClips)
            if (clip.trackId == trackId)
                holdsMidi = true;
        if (holdsMidi)
            for (std::uint32_t paramId = 1; paramId <= yesdaw::engine::SimpleSynthNode::kParameterCount; ++paramId)
            {
                if (! yesdaw::engine::instrumentKindAcceptsParameterId (track->instrumentKind, paramId))
                    continue;
                const yesdaw::engine::ParamSpec spec = yesdaw::engine::instrumentParamSpecForKind (track->instrumentKind, paramId);
                options.push_back ({ yesdaw::engine::AutomationTargetRole::InstrumentParam, paramId, trackId,
                                     "Inst " + juce::String (spec.name).fromLastOccurrenceOf (".", false, false) });
            }
    }

    // R14: bus automation is reachable — every bus's fader, pan, and (R13) send levels
    // enumerate after the track's own targets, labelled by bus name. The engine targets
    // (BusFader/BusPan since M-era, bus SendLevel since R13) were dead code from the shell
    // until this list carried them; the same canvas pencils their lanes unchanged.
    for (std::size_t busIndex = 0; busIndex < appModel.project().buses.size(); ++busIndex)
    {
        const yesdaw::engine::Bus& bus = appModel.project().buses[busIndex];
        const juce::String busName = bus.strip.name.empty()
            ? "Bus " + juce::String (static_cast<int> (busIndex) + 1)
            : juce::String (bus.strip.name);
        options.push_back ({ yesdaw::engine::AutomationTargetRole::BusFader,
                             yesdaw::engine::FaderNode::kGainParameterId, bus.id,
                             busName + " Fader", true });
        options.push_back ({ yesdaw::engine::AutomationTargetRole::BusPan,
                             yesdaw::engine::PanNode::kPanParameterId, bus.id,
                             busName + " Pan", true });
        for (std::size_t sendIndex = 0; sendIndex < bus.sends.size(); ++sendIndex)
        {
            juce::String destName ("Bus?");
            for (const auto& dest : appModel.project().buses)
                if (dest.id == bus.sends[sendIndex].busId)
                    destName = juce::String (dest.strip.name);
            options.push_back ({ yesdaw::engine::AutomationTargetRole::SendLevel,
                                 static_cast<std::uint32_t> (sendIndex), bus.id,
                                 busName + " Send: " + destName, true });
        }
    }
    return options;
}

MainComponent::AutomationTargetOption MainComponent::currentAutomationTarget() const
{
    if (selectedAutomationTargetIndex >= 0
        && selectedAutomationTargetIndex < static_cast<int> (automationTargetOptions.size()))
        return automationTargetOptions[static_cast<std::size_t> (selectedAutomationTargetIndex)];

    AutomationTargetOption fallback;
    fallback.ownerEntity = automationTargetTrackId();
    fallback.paramId = yesdaw::engine::FaderNode::kGainParameterId;
    fallback.label = "Fader";
    return fallback;
}

double MainComponent::automationCanvasSecondsForLocalX (int localX)
{
    const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
    const double pixelsPerSecond = std::max (
        yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
        geometry.viewport.pixelsPerSecond);
    const int timelineLocalX = localX + automationLaneCanvas.getX() - timelineInput.getX();
    return std::max (0.0,
                     state.viewport.scrollSeconds
                         + static_cast<double> (timelineLocalX - geometry.clipArea.getX()) / pixelsPerSecond);
}

int MainComponent::automationCanvasLocalXForSeconds (double seconds)
{
    const yesdaw::ui::TimelineCanvasState state = makeTimelineState();
    const yesdaw::ui::TimelineCanvasGeometry geometry =
        yesdaw::ui::timelineCanvasGeometry (timelineInput.getLocalBounds(), state);
    const double pixelsPerSecond = std::max (
        yesdaw::ui::UiTheme::Layout::timelineCoordinatePixelsPerSecondFloor,
        geometry.viewport.pixelsPerSecond);
    const int timelineLocalX = geometry.clipArea.getX()
        + juce::roundToInt ((seconds - state.viewport.scrollSeconds) * pixelsPerSecond);
    return timelineLocalX - (automationLaneCanvas.getX() - timelineInput.getX());
}

std::optional<yesdaw::engine::Tick> MainComponent::timelineTickFromSeconds (double seconds) const noexcept
{
    const yesdaw::engine::Project& project = appModel.project();
    if (! project.sampleRate.isValid() || ! std::isfinite (seconds) || seconds < 0.0)
        return std::nullopt;

    const double ticks = seconds * project.sampleRate.hz;
    if (ticks > static_cast<double> (std::numeric_limits<yesdaw::engine::Tick>::max()))
        return std::nullopt;

    return static_cast<yesdaw::engine::Tick> (std::llround (ticks));
}

void MainComponent::moveTimelineClipByLayoutId (int layoutClipId, double startSeconds, bool snapToGrid)
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        return;

    const yesdaw::engine::EntityId draggedClipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
    if (! appModel.isTimelineClipSelected (draggedClipId))
        (void) appModel.selectTimelineClip (draggedClipId);
    else
        (void) appModel.selectTimelineClipForGesture (draggedClipId, false);
    if (const auto tick = timelineTickFromSeconds (startSeconds))
    {
        const yesdaw::engine::Clip* moving = nullptr;   // G2.7: Relative / Events read the clip
        for (const yesdaw::engine::Clip& clip : appModel.project().clips)
            if (clip.id == draggedClipId)
                moving = &clip;
        (void) appModel.moveSelectedTimelineClipTo (snappedTimelineTickFrom (
            *tick, snapToGrid,
            moving != nullptr ? std::optional<yesdaw::engine::Tick> (moving->timelineStart) : std::nullopt,
            std::optional<yesdaw::engine::EntityId> (draggedClipId)));
    }

    refreshActionState();
    repaintAll();
}

// The active snap grid applied to a gesture tick. The gesture's Ctrl flag INVERTS the global
// grid: grid on -> Ctrl drags fine; grid off -> Ctrl snaps one-shot.
// G2.7: the grid a drag lands on. Grid mode subdivides the chosen unit while a cell stays at
// least timelineSnapMinGridPx wide at the current zoom (halving as you zoom in); the other
// modes use the unit as chosen.
std::int64_t MainComponent::effectiveSnapGridTicks() const
{
    std::int64_t grid = appModel.context().snapGridTicks;
    if (grid <= 0 || appModel.context().snapMode != yesdaw::ui::UiSnapMode::Grid)
        return grid;
    const yesdaw::engine::Project& project = appModel.project();
    if (! project.sampleRate.isValid())
        return grid;
    const double pps = timelinePixelsPerSecondFor (timelineTotalSeconds);
    if (pps <= 0.0)
        return grid;
    for (int k = 0; k < yesdaw::ui::UiTheme::Layout::timelineSnapMaxSubdivisions; ++k)
    {
        const std::int64_t half = grid / 2;
        if (half <= 0 || grid % 2 != 0)
            break;
        if (static_cast<double> (half) / project.sampleRate.hz * pps < static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineSnapMinGridPx))
            break;
        grid = half;
    }
    return grid;
}

// The Events mode's candidates: every clip edge (except the dragged clip's own), every marker,
// the playhead and the loop edges; the nearest within the tolerance wins, else no snap.
std::optional<yesdaw::engine::Tick> MainComponent::snapTickToEvents (yesdaw::engine::Tick tick,
                                                                   std::optional<yesdaw::engine::EntityId> excludeClip) const
{
    const yesdaw::engine::Project& project = appModel.project();
    if (! project.sampleRate.isValid())
        return std::nullopt;
    const double pps = timelinePixelsPerSecondFor (timelineTotalSeconds);
    if (pps <= 0.0)
        return std::nullopt;
    const auto tolerance = static_cast<yesdaw::engine::Tick> (
        std::llround (static_cast<double> (yesdaw::ui::UiTheme::Layout::timelineSnapEventTolerancePx) / pps * project.sampleRate.hz));
    std::optional<yesdaw::engine::Tick> best;
    const auto consider = [&] (yesdaw::engine::Tick candidate)
    {
        const auto distance = static_cast<yesdaw::engine::Tick> (std::llabs (static_cast<long long> (candidate) - static_cast<long long> (tick)));
        if (distance > tolerance)
            return;
        if (! best || distance < static_cast<yesdaw::engine::Tick> (std::llabs (static_cast<long long> (*best) - static_cast<long long> (tick))))
            best = candidate;
    };
    for (const yesdaw::engine::Clip& clip : project.clips)
    {
        if (excludeClip && clip.id == *excludeClip)
            continue;
        consider (clip.timelineStart);
        consider (clip.timelineStart + clip.timelineLength);
    }
    for (const yesdaw::engine::Marker& marker : project.markers)
        consider (marker.tick);
    consider (static_cast<yesdaw::engine::Tick> (std::max<std::int64_t> (0, appModel.context().playheadFrame)));
    if (appModel.playbackLoopEndFrame() > appModel.playbackLoopStartFrame())
    {
        consider (static_cast<yesdaw::engine::Tick> (appModel.playbackLoopStartFrame()));
        consider (static_cast<yesdaw::engine::Tick> (appModel.playbackLoopEndFrame()));
    }
    return best;
}

yesdaw::engine::Tick MainComponent::snappedTimelineTick (yesdaw::engine::Tick tick, bool invertSnap) const
{
    return snappedTimelineTickFrom (tick, invertSnap, std::nullopt, std::nullopt);
}

// G2.7: ONE snap law for every drop. `origin` is the dragged clip's start before the drag
// (Relative mode snaps the distance from it); `movingClip` is excluded from the Events.
yesdaw::engine::Tick MainComponent::snappedTimelineTickFrom (yesdaw::engine::Tick tick, bool invertSnap,
                                                           std::optional<yesdaw::engine::Tick> origin,
                                                           std::optional<yesdaw::engine::EntityId> movingClip) const
{
    const yesdaw::ui::UiSnapMode mode = appModel.context().snapMode;
    const bool unitOn = appModel.context().snapEnabled;
    // Off (or the unit chooser's Off) snaps nothing; Ctrl inverts: it snaps to the grid.
    const bool modeOff = mode == yesdaw::ui::UiSnapMode::Off || ! unitOn;
    const bool shouldSnap = modeOff ? invertSnap : ! invertSnap;
    if (! shouldSnap)
        return tick;
    if (! modeOff && mode == yesdaw::ui::UiSnapMode::Events)
    {
        if (const std::optional<yesdaw::engine::Tick> hit = snapTickToEvents (tick, movingClip))
            return std::max<yesdaw::engine::Tick> (0, *hit);
        return tick;
    }
    const std::int64_t gridTicks = modeOff ? appModel.context().snapGridTicks : effectiveSnapGridTicks();
    if (gridTicks <= 0)
        return tick;
    yesdaw::engine::Tick snapped = 0;
    if (! modeOff && mode == yesdaw::ui::UiSnapMode::Relative && origin)
    {
        const auto delta = static_cast<yesdaw::engine::Tick> (static_cast<long long> (tick) - static_cast<long long> (*origin));
        const long long rounded = std::llround (static_cast<double> (delta) / static_cast<double> (gridTicks)) * gridTicks;
        return std::max<yesdaw::engine::Tick> (0, static_cast<yesdaw::engine::Tick> (static_cast<long long> (*origin) + rounded));
    }
    if (! yesdaw::engine::snapTick (tick, yesdaw::engine::SnapGrid { gridTicks }, snapped))
        return tick;
    return std::max<yesdaw::engine::Tick> (0, snapped);
}

void MainComponent::moveTimelineClipToLaneByLayoutId (int layoutClipId, int targetLane, double startSeconds, bool snapToGrid)
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        return;

    const yesdaw::engine::Project& project = appModel.project();
    if (targetLane < 0 || targetLane >= static_cast<int> (project.tracks.size()))
        return;

    const yesdaw::engine::EntityId draggedClipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
    if (! appModel.isTimelineClipSelected (draggedClipId))
        (void) appModel.selectTimelineClip (draggedClipId);
    else
        (void) appModel.selectTimelineClipForGesture (draggedClipId, false);
    if (const auto tick = timelineTickFromSeconds (startSeconds))
        (void) appModel.moveSelectedTimelineClipToTrack (
            project.tracks[static_cast<std::size_t> (targetLane)].id,
            snappedTimelineTick (*tick, snapToGrid));

    refreshActionState();
    repaintAll();
}

void MainComponent::copyTimelineClipByLayoutId (int layoutClipId, int targetLane, double startSeconds, bool snapToGrid)
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        return;

    const yesdaw::engine::Project& project = appModel.project();
    const yesdaw::engine::EntityId sourceClipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
    const yesdaw::engine::Clip* const sourceClip = findProjectClipById (sourceClipId);
    if (sourceClip == nullptr)
        return;

    yesdaw::engine::EntityId targetTrackId = sourceClip->trackId;
    if (targetLane >= 0)
    {
        if (targetLane >= static_cast<int> (project.tracks.size()))
            return;
        targetTrackId = project.tracks[static_cast<std::size_t> (targetLane)].id;
    }

    // The dragged clip is the gesture anchor; a copy-drag on a selected member carries the
    // whole selection, exactly like the move gesture (E2).
    if (! appModel.isTimelineClipSelected (sourceClipId))
        (void) appModel.selectTimelineClip (sourceClipId);
    else
        (void) appModel.selectTimelineClipForGesture (sourceClipId, false);
    if (const auto tick = timelineTickFromSeconds (startSeconds))
        (void) appModel.copySelectedTimelineClipsTo (
            targetTrackId, snappedTimelineTick (*tick, snapToGrid));

    refreshActionState();
    repaintAll();
}

// Snap law for edge gestures (E4): the snapped tick goes straight to the verb, whose legality
// rules (positive length, in-body split, source-window bounds) win by honest refusal.
void MainComponent::splitTimelineClipByLayoutId (int layoutClipId, double splitSeconds, bool snapInvert)
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        return;

    (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
    if (const auto tick = timelineTickFromSeconds (splitSeconds))
        (void) appModel.splitSelectedTimelineClipAt (snappedTimelineTick (*tick, snapInvert));

    refreshActionState();
    repaintAll();
}

void MainComponent::trimTimelineClipRightByLayoutId (int layoutClipId, double endSeconds, bool snapInvert)
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        return;

    (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
    if (const auto tick = timelineTickFromSeconds (endSeconds))
        (void) appModel.trimSelectedTimelineClipRightTo (snappedTimelineTick (*tick, snapInvert));

    refreshActionState();
    repaintAll();
}

// G2.11: the slip — the dragged distance, snapped to the effective grid when snap is on (Ctrl is
// part of the gesture, so it cannot defeat snap here; Snap: Off does), moves the source.
void MainComponent::slipTimelineClipByLayoutId (int layoutClipId, double deltaSeconds)
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        return;
    const yesdaw::engine::Project& project = appModel.project();
    if (! project.sampleRate.isValid())
        return;
    auto deltaTicks = static_cast<yesdaw::engine::Tick> (std::llround (deltaSeconds * project.sampleRate.hz));
    const bool snapOn = appModel.context().snapEnabled && appModel.context().snapMode != yesdaw::ui::UiSnapMode::Off;
    const std::int64_t grid = effectiveSnapGridTicks();
    if (snapOn && grid > 0)
        deltaTicks = static_cast<yesdaw::engine::Tick> (std::llround (static_cast<double> (deltaTicks) / static_cast<double> (grid)) * grid);
    if (deltaTicks == 0)
        return;

    (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
    (void) appModel.slipSelectedTimelineClipBy (deltaTicks);
    refreshActionState();
    repaintAll();
}

// G2.9b: the Alt-drag on the right edge lands a NEW END; the model turns it into the factor.
void MainComponent::stretchTimelineClipRightByLayoutId (int layoutClipId, double endSeconds, bool snapInvert)
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        return;

    (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (layoutClipId)]);
    if (const auto tick = timelineTickFromSeconds (endSeconds))
        (void) appModel.stretchSelectedTimelineClipTo (snappedTimelineTick (*tick, snapInvert));

    refreshActionState();
    repaintAll();
}

void MainComponent::adjustTimelineClipGainByLayoutId (int layoutClipId, int deltaPixels)
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        return;

    const yesdaw::engine::EntityId clipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
    const yesdaw::engine::Clip* const clip = findProjectClipById (clipId);
    if (clip == nullptr)
        return;

    const float nextGain = std::clamp (
        clip->gain + static_cast<float> (deltaPixels) * yesdaw::ui::UiTheme::Layout::timelineClipGainPerDragPixel,
        0.0f,
        yesdaw::ui::UiTheme::Layout::timelineClipMaxGestureGain);

    if (std::fabs (nextGain - clip->gain) <= 0.000001f)
        return;

    (void) appModel.selectTimelineClip (clipId);
    (void) appModel.setSelectedTimelineClipGain (nextGain);

    refreshActionState();
    repaintAll();
}

void MainComponent::adjustTimelineClipFadeByLayoutId (int layoutClipId, bool fadeIn, double fadeSeconds, double curveDelta)
{
    if (layoutClipId < 0 || layoutClipId >= static_cast<int> (timelineClipIds.size()))
        return;

    const yesdaw::engine::EntityId clipId = timelineClipIds[static_cast<std::size_t> (layoutClipId)];
    const yesdaw::engine::Clip* const clip = findProjectClipById (clipId);
    if (clip == nullptr)
        return;

    // G2.10: a negative fadeSeconds is the sentinel for "keep the current fade" (a pure bend).
    const std::optional<yesdaw::engine::Tick> fadeTicks = fadeSeconds < 0.0
        ? std::optional<yesdaw::engine::Tick> (fadeIn ? clip->fadeIn : clip->fadeOut)
        : timelineTickFromSeconds (fadeSeconds);
    if (! fadeTicks)
        return;

    const yesdaw::engine::Tick clampedFade =
        std::clamp<yesdaw::engine::Tick> (*fadeTicks, 0, std::max<yesdaw::engine::Tick> (0, clip->timelineLength));
    const yesdaw::engine::Tick nextFadeIn = fadeIn ? clampedFade : clip->fadeIn;
    const yesdaw::engine::Tick nextFadeOut = fadeIn ? clip->fadeOut : clampedFade;
    const bool bends = std::fabs (curveDelta) > 0.0;
    if (nextFadeIn == clip->fadeIn && nextFadeOut == clip->fadeOut && ! bends)
        return;

    (void) appModel.selectTimelineClip (clipId);
    if (bends)   // G2.10: length + curve bend as one undo step
        (void) appModel.adjustSelectedTimelineClipFade (nextFadeIn, nextFadeOut, fadeIn, static_cast<float> (curveDelta));
    else
        (void) appModel.setSelectedTimelineClipFades (nextFadeIn, nextFadeOut);

    refreshActionState();
    repaintAll();
}

const yesdaw::engine::Clip* MainComponent::findProjectClipById (yesdaw::engine::EntityId clipId) const noexcept
{
    for (const yesdaw::engine::Clip& candidate : appModel.project().clips)
        if (candidate.id == clipId)
            return &candidate;

    return nullptr;
}

} // namespace yesdaw::ui
