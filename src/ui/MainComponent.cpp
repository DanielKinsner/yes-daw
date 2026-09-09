// YES DAW — the app shell: lifecycle, layout, paint, the tick, the device callbacks, and the harness entry points.
//
// Plan §5.1 (the shell topology), checkpoint 2 (2026-09-05): MainComponent.cpp was the whole shell in
// one 18 000-line file (cp1 carved the helper components, cp2 the class itself). The declaration is
// ui/MainComponentShell.h; the other domains are MainComponent<Domain>.cpp beside this file.

#include "ui/MainComponentShell.h"
#include "ui/DesktopAudioStartup.h"
#include "ui/SoftwareCanvasCache.h"

#include <fstream>
#include <sstream>

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

MainComponent::MainComponent (yesdaw::ui::MainComponentFileChoices choices, bool enableDesktopAudio)
    : fileChoices (std::move (choices)), desktopAudioRequested (enableDesktopAudio)
{
    if (! fileChoices.sessionStateDirectory.empty())
        appModel.setSessionStateDirectory (fileChoices.sessionStateDirectory);

    // G0.1 State probe: debug-only; a normal launch leaves the path empty and writes nothing.
    stateProbePath = fileChoices.stateProbePath;
    launchStamp = std::chrono::steady_clock::now();
    auto startupStageStamp = launchStamp;
    std::optional<std::ostringstream> startupTimings;
    if (! stateProbePath.empty())
        startupTimings.emplace();
    const auto recordStartupStage = [&] (const char* name) {
        if (! startupTimings)
            return;
        const auto now = std::chrono::steady_clock::now();
        *startupTimings << name << '\t'
            << std::chrono::duration<double, std::milli> (now - startupStageStamp).count() << '\n';
        startupStageStamp = now;
    };

    setOpaque (true);
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);   // the declared keymap chords dispatch through keyPressed
    setSize (yesdaw::ui::UiTheme::Layout::defaultWindowWidth,
             yesdaw::ui::UiTheme::Layout::defaultWindowHeight);

    const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        const yesdaw::ui::UiActionId action = toolbarActions[i];
        const auto* descriptor = appModel.registry().descriptor (action);
        if (descriptor == nullptr)
            continue;

        auto& button = buttons[i];
        button.setAction (action);
        button.setButtonText (actionButtonText (action));
        button.setComponentID (descriptor->stableId);
        button.setName (descriptor->accessibleName);
        button.setTooltip (juce::String (descriptor->stableId) + "  " + descriptor->defaultKey);
        button.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
        button.setColour (juce::TextButton::buttonOnColourId, descriptor->accessibleRole == yesdaw::ui::AccessibilityRole::ToggleButton
                                                              ? kPurple.darker (0.45f)
                                                              : kBlue.darker (0.25f));
        button.setColour (juce::TextButton::textColourOffId, kText);
        button.setColour (juce::TextButton::textColourOnId, kText);
        button.onClick = [this, action] {
            handleAction (action);
            refreshActionState();
            resized();
            repaintAll();
        };
        addAndMakeVisible (button);
    }

    configureAutosaveRecoveryButton (autosaveRestoreButton, yesdaw::ui::UiActionId::AutosaveRecoveryRestore);
    configureAutosaveRecoveryButton (autosaveDiscardButton, yesdaw::ui::UiActionId::AutosaveRecoveryDiscard);

    configureActionComponent (exportAudioButton, yesdaw::ui::UiActionId::ProjectExportAudio, "Export audio");
    exportAudioButton.setButtonText ("Export WAV");
    exportAudioButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
    exportAudioButton.setColour (juce::TextButton::textColourOffId, kText);
    exportAudioButton.onClick = [this] {
        handleAction (yesdaw::ui::UiActionId::ProjectExportAudio);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (exportAudioButton);

    // Export options (usable-DAW P1): bit depth and range feed the model before Export runs.
    exportBitDepthChooser.setComponentID ("shell.export.bitdepth");
    exportBitDepthChooser.setTooltip ("Export bit depth");
    exportBitDepthChooser.setName ("Export bit depth");
    exportBitDepthChooser.setTitle ("Export bit depth");
    exportBitDepthChooser.addItem ("32-bit float", 1);
    exportBitDepthChooser.addItem ("24-bit PCM", 2);
    exportBitDepthChooser.addItem ("16-bit PCM", 3);
    exportBitDepthChooser.setSelectedId (1, juce::dontSendNotification);
    exportBitDepthChooser.onChange = [this] {
        const int selected = exportBitDepthChooser.getSelectedId();
        appModel.setExportBitDepth (selected == 2 ? yesdaw::ui::UiAppModel::UiExportBitDepth::Int24
                                    : selected == 3 ? yesdaw::ui::UiAppModel::UiExportBitDepth::Int16
                                                    : yesdaw::ui::UiAppModel::UiExportBitDepth::Float32);
    };
    addAndMakeVisible (exportBitDepthChooser);

    exportRangeChooser.setComponentID ("shell.export.range");
    exportRangeChooser.setTooltip ("Export range: whole project or the loop/range selection");
    exportRangeChooser.setName ("Export range");
    exportRangeChooser.setTitle ("Export range");
    exportRangeChooser.addItem ("Whole Project", 1);
    exportRangeChooser.addItem ("Loop Region", 2);
    exportRangeChooser.setSelectedId (1, juce::dontSendNotification);
    exportRangeChooser.onChange = [this] {
        appModel.setExportLoopRangeOnly (exportRangeChooser.getSelectedId() == 2);
    };
    addAndMakeVisible (exportRangeChooser);

    exportAudioProgress.setComponentID (kExportAudioProgressComponentId);
    exportAudioProgress.setTooltip ("Audio export progress");
    exportAudioProgress.setName ("Export audio progress");
    exportAudioProgress.setText ("Export --", juce::dontSendNotification);
    exportAudioProgress.setJustificationType (juce::Justification::centred);
    exportAudioProgress.setColour (juce::Label::backgroundColourId, yesdaw::ui::UiTheme::Color::darkControl());
    exportAudioProgress.setColour (juce::Label::textColourId, kMutedText);
    addAndMakeVisible (exportAudioProgress);

    configureActionComponent (exportAudioCancelButton,
                              yesdaw::ui::UiActionId::ProjectExportAudioCancel,
                              "Cancel audio export");
    exportAudioCancelButton.setButtonText ("Cancel");
    exportAudioCancelButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
    exportAudioCancelButton.setColour (juce::TextButton::textColourOffId, kText);
    exportAudioCancelButton.onClick = [this] {
        handleAction (yesdaw::ui::UiActionId::ProjectExportAudioCancel);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (exportAudioCancelButton);

    configureActionComponent (masterLoudnessReadout, yesdaw::ui::UiActionId::MixerReadLoudness, "Master loudness");
    masterLoudnessReadout.setButtonText ("-- LUFS");
    masterLoudnessReadout.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::darkControl());
    masterLoudnessReadout.setColour (juce::TextButton::textColourOffId, kText);
    masterLoudnessReadout.onClick = [this] {
        (void) appModel.dispatch (yesdaw::ui::UiActionId::MixerReadLoudness);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (masterLoudnessReadout);

    timelineInput.setComponentID (kTimelineComponentId);
    timelineInput.setTooltip ("Timeline: drag clips, drag the ruler to select a range, Shift-drag for the loop");
    timelineInput.setName ("Timeline");
    timelineInput.setTitle ("Timeline");
    // G0.4: the canvas is the STATIC layer — cached as an image and re-rendered only when
    // repaintAll() (any model/view change) or the canvas's own gesture repaints invalidate
    // it. It never paints the playhead; the layer above does, every tick, from the same law.
    timelineInput.stateProvider = [this] {
        yesdaw::ui::TimelineCanvasState state = makeTimelineState();
        state.paintPlayhead = false;
        return state;
    };
   #if JUCE_WINDOWS
    timelineInput.setCachedComponentImage (new yesdaw::ui::SoftwareCanvasCache (timelineInput));
   #else
    timelineInput.setBufferedToImage (true);
   #endif
    playheadLayer.stateProvider = [this] { return makeTimelineState(); };
    // A paint layer, not a control: no component id (the tooltip / dead-affordance laws
    // enumerate identified children), a name for the render-budget gate.
    playheadLayer.setName ("Playhead layer");
    timelineInput.activeToolProvider = [this] {
        return appModel.context().activeTimelineTool;
    };
    timelineInput.onToolSelected = [this] (yesdaw::ui::TimelineTool tool) {
        handleAction (yesdaw::ui::timelineToolSelectAction (tool));
        refreshActionState();
        repaintAll();
    };
    timelineInput.onZoomToolClicked = [this] (double anchorSeconds, bool zoomOut) {
        const double factor = yesdaw::ui::UiTheme::Layout::timelineZoomToolClickFactor;
        zoomTimelineAtAnchor (anchorSeconds, zoomOut ? 1.0 / factor : factor);
        repaintAll();
    };
    timelineInput.onClipErased = [this] (int layoutClipId) {   // G3.2: select, then the one delete verb
        if (layoutClipId < 0 || static_cast<std::size_t> (layoutClipId) >= timelineClipIds.size())
            return;
        (void) appModel.selectTimelineClipForGesture (timelineClipIds[static_cast<std::size_t> (layoutClipId)], false);
        handleAction (yesdaw::ui::UiActionId::TimelineClipDelete);
        refreshActionState();
        repaintAll();
    };
    timelineInput.onHandToolScrolled = [this] (double secondsDelta) {
        // G2.16: clamped to the view's range like the scroll bar — no transient overshoot.
        const double visible = timelineVisibleSecondsFor (timelineTotalSeconds);
        const double maxScroll = std::max (0.0, timelineTotalSeconds - visible);
        timelineScrollSeconds = std::clamp (timelineScrollSeconds + secondsDelta, 0.0, maxScroll);
        repaintAll();
    };
    timelineInput.onAutoScrolled = [this] (double secondsDelta) {   // G2.3: the hand tool's law
        timelineScrollSeconds = std::max (0.0, timelineScrollSeconds + secondsDelta);
        repaintAll();
    };
    timelineInput.snapSecondsForPreview = [this] (double seconds) {   // G2.3: the release's snap law
        if (const auto tick = timelineTickFromSeconds (seconds))
            if (appModel.project().sampleRate.isValid())
                return static_cast<double> (snappedTimelineTick (*tick, true)) / appModel.project().sampleRate.hz;
        return seconds;
    };
    timelineInput.onVerticalScrollRows = [this] (int rowDelta) {
        scrollTrackRowsBy (rowDelta);
    };
    // M10: OS file drops land on the track under the pointer at the snapped tick under the
    // pointer. Several files go onto consecutive lanes, each import its own undo step
    // (R8); anything the WAV reader refuses is reported on the status line (R6) and
    // changes nothing.
    timelineInput.filesAreImportable = [this] (const juce::StringArray& files) {
        if (! appModel.context().projectLoaded || files.isEmpty())
            return false;

        for (const juce::String& file : files)
            if (juce::File (file).hasFileExtension ("wav") || juce::File (file).hasFileExtension ("mid;midi"))   // G3.7
                return true;

        return false;
    };
    timelineInput.onFilesDropped = [this] (const juce::StringArray& files, int lane, double seconds) {
        const auto& tracks = appModel.project().tracks;
        if (lane < 0 || lane >= static_cast<int> (tracks.size()))
            return;

        const auto tick = timelineTickFromSeconds (seconds);
        if (! tick.has_value())
            return;

        const yesdaw::engine::Tick start = snappedTimelineTick (*tick, false);
        int laneOffset = 0;
        bool anyImported = false;
        std::string refusedNames;
        for (const juce::String& file : files)
        {
            const std::filesystem::path path (file.toStdString());
            // G3.7: a .mid lands on the lane under the pointer as MIDI clips (the model names
            // its own refusals on the status line; further file tracks add lanes below).
            if (juce::File (file).hasFileExtension ("mid;midi"))
            {
                const int midiLane = std::min (lane + laneOffset, static_cast<int> (appModel.project().tracks.size()) - 1);
                const yesdaw::engine::EntityId midiTrackId = appModel.project().tracks[static_cast<std::size_t> (midiLane)].id;
                if (appModel.importMidiFileAt (path, midiTrackId, start).dispatched)
                {
                    anyImported = true;
                    ++laneOffset;
                }
                continue;
            }
            auto decoded = decodeProjectWav (path);
            if (! decoded)
            {
                refusedNames += (refusedNames.empty() ? "" : ", ") + path.filename().string();
                continue;
            }

            const int targetLane = std::min (lane + laneOffset,
                                             static_cast<int> (appModel.project().tracks.size()) - 1);
            const yesdaw::engine::EntityId trackId =
                appModel.project().tracks[static_cast<std::size_t> (targetLane)].id;
            // R7: a verb failure (rate mismatch, bundle copy, …) reports its own precise
            // reason inside the model — only decoder refusals are the shell's to name.
            if (appModel.importAudioFileAt (path, std::move (*decoded), trackId, start).ok())
            {
                anyImported = true;
                ++laneOffset;
            }
        }

        // R6: every file the WAV reader refused is named immediately — the good files
        // still landed.
        if (! refusedNames.empty())
            appModel.reportStatus ("Import refused (WAV only, stereo max): " + refusedNames, true);

        if (anyImported)
            selectedTrackLane = lane;

        refreshActionState();
        repaintAll();
    };
    timelineInput.onPencilEmptyLane = [this] (int lane, double seconds) {
        const auto& tracks = appModel.project().tracks;
        if (lane < 0 || lane >= static_cast<int> (tracks.size()))
            return;
        if (const auto tick = timelineTickFromSeconds (seconds))
            (void) appModel.addMidiClipOnTrackAt (
                tracks[static_cast<std::size_t> (lane)].id,
                snappedTimelineTick (*tick, false));
        refreshActionState();
        repaintAll();
    };
    const auto hintSink = [this] (const juce::String& hint) { setHoverHint (hint); };
    timelineInput.onHoverHint = hintSink;
    pianoRollInput.onHoverHint = hintSink;
    trackListInput.onHoverHint = hintSink;
    mixerStripsInput.onHoverHint = hintSink;
    timelineInput.onContextMenuRequested = [this] (yesdaw::ui::ContextMenuTarget target, int index, juce::Point<int> position) {
        if (target == yesdaw::ui::ContextMenuTarget::EmptyLane)
            selectTrackLane (index);   // the lane's track becomes the selection first
        openContextMenu (target, index, timelineInput, position);
    };
    timelineInput.onClipClicked = [this] (int timelineClipId, bool toggle) {
        selectTimelineClipByLayoutId (timelineClipId, toggle);
    };
    timelineInput.onEmptyClicked = [this] {
        appModel.clearTimelineClipSelection();
        refreshActionState();
        repaintAll();
    };
    timelineInput.onMarqueeSelection = [this] (std::span<const int> timelineClipLayoutIds) {
        std::vector<yesdaw::engine::EntityId> selectedClipIds;
        selectedClipIds.reserve (timelineClipLayoutIds.size());
        for (const int timelineClipLayoutId : timelineClipLayoutIds)
        {
            if (timelineClipLayoutId < 0
                || timelineClipLayoutId >= static_cast<int> (timelineClipIds.size()))
                return;
            selectedClipIds.push_back (timelineClipIds[static_cast<std::size_t> (timelineClipLayoutId)]);
        }

        (void) appModel.selectTimelineClips (
            std::span<const yesdaw::engine::EntityId> (selectedClipIds.data(), selectedClipIds.size()));
        refreshActionState();
        repaintAll();
    };
    timelineInput.onClipMoved = [this] (int timelineClipId, double startSeconds, bool snapToGrid) {
        moveTimelineClipByLayoutId (timelineClipId, startSeconds, snapToGrid);
    };
    timelineInput.onClipMovedToLane = [this] (int timelineClipId, int targetLane, double startSeconds, bool snapToGrid) {
        moveTimelineClipToLaneByLayoutId (timelineClipId, targetLane, startSeconds, snapToGrid);
    };
    timelineInput.onClipCopied = [this] (int timelineClipId, int targetLane, double startSeconds, bool snapToGrid) {
        copyTimelineClipByLayoutId (timelineClipId, targetLane, startSeconds, snapToGrid);
    };
    timelineInput.onClipSplit = [this] (int timelineClipId, double splitSeconds, bool snapInvert) {
        splitTimelineClipByLayoutId (timelineClipId, splitSeconds, snapInvert);
    };
    // E9: a double-clicked MIDI clip opens the piano roll on THAT clip (audio clips keep the
    // historical double-click split behavior through onClipSplit).
    timelineInput.onClipDoubleClicked = [this] (int timelineClipId) {
        if (timelineClipId < 0 || timelineClipId >= static_cast<int> (timelineClipIds.size()))
            return false;
        const yesdaw::engine::EntityId entityId = timelineClipIds[static_cast<std::size_t> (timelineClipId)];
        if (! appModel.openPianoRollOnMidiClip (entityId))
            return false;

        refreshActionState();
        resized();
        repaintAll();
        return true;
    };
    timelineInput.onClipTrimmedRight = [this] (int timelineClipId, double endSeconds, bool snapInvert) {
        trimTimelineClipRightByLayoutId (timelineClipId, endSeconds, snapInvert);
    };
    timelineInput.onClipStretchedRight = [this] (int timelineClipId, double endSeconds, bool snapInvert) {   // G2.9b
        stretchTimelineClipRightByLayoutId (timelineClipId, endSeconds, snapInvert);
    };
    timelineInput.onClipSlipped = [this] (int timelineClipId, double deltaSeconds) {   // G2.11
        slipTimelineClipByLayoutId (timelineClipId, deltaSeconds);
    };
    timelineInput.onClipRenameRequested = [this] (int timelineClipId) {   // G2.12
        if (timelineClipId < 0 || timelineClipId >= static_cast<int> (timelineClipIds.size()))
            return;
        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (timelineClipId)]);
        refreshActionState();
        openClipRenameEditor();
        repaintAll();
    };
    timelineInput.onClipTrimmedLeft = [this] (int timelineClipId, double startSeconds, bool snapInvert) {
        if (timelineClipId < 0 || timelineClipId >= static_cast<int> (timelineClipIds.size()))
            return;

        (void) appModel.selectTimelineClip (timelineClipIds[static_cast<std::size_t> (timelineClipId)]);
        if (const auto tick = timelineTickFromSeconds (startSeconds))
            (void) appModel.trimSelectedTimelineClipLeftTo (snappedTimelineTick (*tick, snapInvert));

        refreshActionState();
        repaintAll();
    };
    timelineInput.onClipGainAdjusted = [this] (int timelineClipId, int deltaPixels) {
        adjustTimelineClipGainByLayoutId (timelineClipId, deltaPixels);
    };
    timelineInput.onClipFadeAdjusted = [this] (int timelineClipId, bool fadeIn, double fadeSeconds, double curveDelta) {
        adjustTimelineClipFadeByLayoutId (timelineClipId, fadeIn, fadeSeconds, curveDelta);
    };
    timelineInput.onZoomWheel = [this] (double anchorSeconds, double wheelDelta) {
        const double factor = wheelDelta > 0.0
            ? yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep
            : 1.0 / yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep;
        zoomTimelineAtAnchor (anchorSeconds, factor);
        repaintAll();
    };
    timelineInput.onScrollWheel = [this] (double wheelDelta) {
        const double visibleSeconds = std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                                timelineTotalSeconds / std::max (1.0, timelineZoomFactor));
        timelineScrollSeconds -= wheelDelta * visibleSeconds
                               * yesdaw::ui::UiTheme::Layout::timelineScrollWheelFraction;
        repaintAll();
    };
    timelineInput.onRulerAltClicked = [this] (double seconds) {
        if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds))
        {
            (void) appModel.removeTimelineMarkerNearestTick (*tick);
            refreshActionState();
            repaintAll();
        }
    };
    timelineInput.onLoopRegionDragged = [this] (double startSeconds, double endSeconds, bool snapInvert) {
        const std::optional<yesdaw::engine::Tick> startFrame = timelineTickFromSeconds (startSeconds);
        const std::optional<yesdaw::engine::Tick> endFrame = timelineTickFromSeconds (endSeconds);
        if (startFrame && endFrame)
        {
            const yesdaw::engine::Tick snappedStart = snappedTimelineTick (*startFrame, snapInvert);
            const yesdaw::engine::Tick snappedEnd = snappedTimelineTick (*endFrame, snapInvert);
            if (snappedEnd > snappedStart)
            {
                (void) appModel.setPlaybackLoopRegion (snappedStart, snappedEnd);
                refreshActionState();
                repaintAll();
            }
        }
    };
    // N8: Alt+Shift-drag on the ruler sets the punch region; a degenerate (non-positive)
    // span — dragging back onto the start point, effectively a click — clears it instead, so
    // the SAME gesture that creates a punch region can remove one.
    timelineInput.onPunchRegionDragged = [this] (double startSeconds, double endSeconds, bool snapInvert) {
        const std::optional<yesdaw::engine::Tick> startFrame = timelineTickFromSeconds (startSeconds);
        const std::optional<yesdaw::engine::Tick> endFrame = timelineTickFromSeconds (endSeconds);
        if (! startFrame || ! endFrame)
            return;
        const yesdaw::engine::Tick snappedStart = snappedTimelineTick (*startFrame, snapInvert);
        const yesdaw::engine::Tick snappedEnd = snappedTimelineTick (*endFrame, snapInvert);
        if (snappedEnd > snappedStart)
            (void) appModel.setPunchRegion (true, snappedStart, snappedEnd);
        else
            (void) appModel.setPunchRegion (false, 0, 0);
        refreshActionState();
        repaintAll();
    };
    timelineInput.onMapLabelClicked = [this] (int mapIndex) {
        if (mapIndex < 0 || mapIndex >= static_cast<int> (timelineMapLabelFrames.size()))
            return;
        (void) appModel.locatePlaybackFrame (timelineMapLabelFrames[static_cast<std::size_t> (mapIndex)]);
        refreshActionState();
        repaintAll();
    };
    timelineInput.onTimelineLocated = [this] (double seconds) {
        if (const std::optional<yesdaw::engine::Tick> frame = timelineTickFromSeconds (seconds))
        {
            (void) appModel.locatePlaybackFrame (*frame);
            refreshActionState();
            repaintAll();
        }
    };
    // Marker edits (E7): the dragged label commits a snapped MoveMarker; double-click opens
    // the inline rename editor over the painted label.
    timelineInput.onMarkerDragged = [this] (int markerIndex, double seconds, bool snapInvert) {
        const auto& markers = appModel.project().markers;
        if (markerIndex < 0 || markerIndex >= static_cast<int> (markers.size()))
            return;
        if (const auto tick = timelineTickFromSeconds (std::max (0.0, seconds)))
        {
            (void) appModel.moveTimelineMarkerTo (
                markers[static_cast<std::size_t> (markerIndex)].id,
                snappedTimelineTick (*tick, snapInvert));
            refreshActionState();
            repaintAll();
        }
    };
    timelineInput.onMarkerRenameRequested = [this] (int markerIndex) {
        openMarkerRenameEditor (markerIndex);
    };
    // Loop brace edits (E6): the dragged edge (or the move anchor) snaps through the snap
    // chooser; the fixed edge keeps its exact frames, and a move preserves the span exactly.
    timelineInput.onLoopBraceEdited = [this] (TimelineInputComponent::LoopBraceEdit kind,
                                              double pointerSeconds,
                                              double grabOffsetSeconds,
                                              bool snapInvert) {
        const std::int64_t loopStart = appModel.playbackLoopStartFrame();
        const std::int64_t loopEnd = appModel.playbackLoopEndFrame();
        if (loopEnd <= loopStart)
            return;

        bool edited = false;
        if (kind == TimelineInputComponent::LoopBraceEdit::Start)
        {
            if (const auto tick = timelineTickFromSeconds (std::max (0.0, pointerSeconds)))
            {
                const yesdaw::engine::Tick snapped = snappedTimelineTick (*tick, snapInvert);
                if (static_cast<std::int64_t> (snapped) < loopEnd)
                    edited = appModel.setPlaybackLoopRegion (snapped, loopEnd).dispatched;
            }
        }
        else if (kind == TimelineInputComponent::LoopBraceEdit::End)
        {
            if (const auto tick = timelineTickFromSeconds (std::max (0.0, pointerSeconds)))
            {
                const yesdaw::engine::Tick snapped = snappedTimelineTick (*tick, snapInvert);
                if (static_cast<std::int64_t> (snapped) > loopStart)
                    edited = appModel.setPlaybackLoopRegion (loopStart, snapped).dispatched;
            }
        }
        else if (kind == TimelineInputComponent::LoopBraceEdit::Move)
        {
            if (const auto tick = timelineTickFromSeconds (
                    std::max (0.0, pointerSeconds - grabOffsetSeconds)))
            {
                const std::int64_t span = loopEnd - loopStart;
                const yesdaw::engine::Tick snapped = snappedTimelineTick (*tick, snapInvert);
                edited = appModel.setPlaybackLoopRegion (snapped,
                                                         static_cast<std::int64_t> (snapped) + span)
                             .dispatched;
            }
        }

        if (edited)
        {
            refreshActionState();
            repaintAll();
        }
    };
    timelineInput.onRulerRangeSelected = [this] (double startSeconds, double endSeconds, bool snapInvert) {
        const std::optional<yesdaw::engine::Tick> startFrame = timelineTickFromSeconds (startSeconds);
        const std::optional<yesdaw::engine::Tick> endFrame = timelineTickFromSeconds (endSeconds);
        if (startFrame && endFrame)
        {
            const yesdaw::engine::Tick snappedStart = snappedTimelineTick (*startFrame, snapInvert);
            const yesdaw::engine::Tick snappedEnd = snappedTimelineTick (*endFrame, snapInvert);
            if (snappedEnd > snappedStart)
            {
                (void) appModel.setTimelineRangeSelection (snappedStart, snappedEnd);
                refreshActionState();
                repaintAll();
            }
        }
    };
    timelineInput.onRulerRangeCleared = [this] {
        appModel.clearTimelineRangeSelection();
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (timelineInput);
    addAndMakeVisible (playheadLayer);   // G0.4: z-order above the buffered canvas

    // Interactive Track rail (usable-DAW P0): row click selects the Track for import/mixer/remove
    // targeting, double-click (or F2) opens the inline rename editor, and the Add Track button
    // drives the same undoable verb as Ctrl+T.
    trackListInput.setComponentID ("shell.tracklist.input");
    trackListInput.setName ("Track List");
    trackListInput.setTitle ("Track List");
    trackListInput.setTooltip ("Track rail: click to select, drag PAN/VOL minis, click M/S/meter");
    trackListInput.rowCountProvider = [this] {
        return appModel.context().projectLoaded ? static_cast<int> (appModel.project().tracks.size()) : 0;
    };
    trackListInput.rowZoomProvider = [this] { return timelineRowZoom; };   // G2.16
    trackListInput.rowHeightsProvider = [this] {
        std::vector<int> heights;
        if (appModel.context().projectLoaded)
        {
            heights.reserve (appModel.project().tracks.size());
            for (const yesdaw::engine::Track& track : appModel.project().tracks)
                heights.push_back (track.heightPx);
        }
        return heights;
    };
    trackListInput.rowScrollProvider = [this] { return timelineTrackScrollRows; };
    trackListInput.onVerticalScrollRows = [this] (int rowDelta) { scrollTrackRowsBy (rowDelta); };
    trackListInput.onRowClickedWithModifiers = [this] (int row, juce::ModifierKeys mods) {   // G2.17
        if (mods.isCtrlDown())
            toggleTrackLaneSelection (row);
        else if (mods.isShiftDown())
            extendTrackLaneSelection (row);
        else
            selectTrackLane (row);
    };
    trackListInput.onRowReordered = [this] (int from, int to) {   // G2.17
        const auto& tracks = appModel.project().tracks;
        if (from < 0 || to < 0 || from >= static_cast<int> (tracks.size()) || to >= static_cast<int> (tracks.size()))
            return;
        if (appModel.reorderProjectTrack (tracks[static_cast<std::size_t> (from)].id, static_cast<std::size_t> (to)).dispatched)
            selectTrackLane (to);
        refreshActionState();
        resized();
        repaintAll();
    };
    trackListInput.onContextMenuRequested = [this] (yesdaw::ui::ContextMenuTarget target, int index, juce::Point<int> position) {
        openContextMenu (target, index, trackListInput, position);
    };
    trackListInput.panValueProvider = [this] (int row) {
        const auto& tracks = appModel.project().tracks;
        return row >= 0 && row < static_cast<int> (tracks.size())
                   ? tracks[static_cast<std::size_t> (row)].strip.pan
                   : 0.0f;
    };
    trackListInput.volumeValueProvider = [this] (int row) {
        const auto& tracks = appModel.project().tracks;
        return row >= 0 && row < static_cast<int> (tracks.size())
                   ? juce::jlimit (0.0f, 1.0f, tracks[static_cast<std::size_t> (row)].strip.linearGain)
                   : 0.0f;
    };
    trackListInput.onRowDoubleClicked = [this] (int row) {
        selectTrackLane (row);
        openTrackRenameEditor();
    };
    // Rail mini controls (usable-DAW P2): the painted PAN/VOL/M/S become live per-track edits
    // through the same selected-strip verbs the mixer uses (rail selection stays on the rail).
    // E21: rail mini drags bracket a strip gesture so one drag is ONE undo step; the rail's
    // every-mouse-up signal closes it (plain clicks stay single steps either way).
    trackListInput.onPanEdited = [this] (int row, float pan) {
        appModel.beginStripGesture();
        selectTrackLane (row);
        if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
            (void) appModel.setSelectedMixerPan (pan);
        refreshActionState();
        repaintAll();
    };
    trackListInput.onVolumeEdited = [this] (int row, float linearGain) {
        appModel.beginStripGesture();
        selectTrackLane (row);
        if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
            (void) appModel.setSelectedMixerFader (linearGain);
        showDragDbReadout (trackListInput.volumeSliderBounds (row)
                               .translated (trackListInput.getX(), trackListInput.getY()),
                           linearGain);
        refreshActionState();
        repaintAll();
    };
    trackListInput.onMiniDragEnded = [this] {
        appModel.endStripGesture();
        hideDragDbReadout();
    };
    // N6: the row-boundary height drag — E21 coalescing (beginStripGesture on every tick,
    // closed once on release) so the whole drag is one undo step, matching the fader pattern.
    trackListInput.onRowResized = [this] (int row, int heightPx) {
        const auto& tracks = appModel.project().tracks;
        if (row < 0 || row >= static_cast<int> (tracks.size()))
            return;
        appModel.beginStripGesture();
        (void) appModel.setTrackHeight (tracks[static_cast<std::size_t> (row)].id, heightPx);
        refreshActionState();
        repaintAll();
    };
    trackListInput.onRowResizeEnded = [this] { appModel.endStripGesture(); };
    trackListInput.onMeterClicked = [this] (int row) { clearTrackMeterHold (row); };
    trackListInput.onMuteToggled = [this] (int row) {
        selectTrackLane (row);
        if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
            (void) appModel.toggleSelectedMixerMute();
        refreshActionState();
        repaintAll();
    };
    trackListInput.onSoloToggled = [this] (int row) {
        selectTrackLane (row);
        if (appModel.selectMixerTrack (static_cast<std::size_t> (row), false))
            (void) appModel.toggleSelectedMixerSolo();
        refreshActionState();
        repaintAll();
    };
    // The "O" badge arms THIS row through the same verb the lane menu's Arm uses (M11: one
    // more member of the arm set, or one fewer). The verb refuses honestly without an input
    // device; the badge simply stays unlit.
    trackListInput.onArmToggled = [this] (int row) {
        selectTrackLane (row);
        if (row >= 0 && row < static_cast<int> (appModel.project().tracks.size()))
            (void) appModel.toggleRecordingArmForTrack (static_cast<std::size_t> (row));
        refreshActionState();
        repaintAll();
    };
    // N7: one click on a row's colour swatch commits ONE undo step, advancing THAT track
    // (not necessarily the selected one) to the next colour in the fixed cycle.
    trackListInput.onColourSwatchClicked = [this] (int row) {
        const auto& tracks = appModel.project().tracks;
        if (row < 0 || row >= static_cast<int> (tracks.size()))
            return;
        const auto& track = tracks[static_cast<std::size_t> (row)];
        (void) appModel.setTrackColour (track.id, nextTrackColourInCycle (track.colour));
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (trackListInput);

    // Header tempo + time-signature editing (usable-DAW P0): the painted readouts become real
    // undoable controls. Tempo is a drag/scrub bar over the TEMPO cell; meter picks common signatures.
    configureActionComponent (headerTempoControl, yesdaw::ui::UiActionId::TransportSetTempo, "Set tempo");
    headerTempoControl.setSliderStyle (juce::Slider::LinearBar);
    // E24: the LinearBar shows its value inside the bar; the old separate TextBoxLeft was
    // narrower than "120.00" and the thumb painted over the clipped digits.
    headerTempoControl.setTextBoxStyle (juce::Slider::NoTextBox,
                                        false,
                                        yesdaw::ui::UiTheme::Layout::headerTempoTextWidth,
                                        yesdaw::ui::UiTheme::Layout::headerTempoTextHeight);
    headerTempoControl.setRange (yesdaw::ui::UiTheme::Layout::headerTempoMinBpm,
                                 yesdaw::ui::UiTheme::Layout::headerTempoMaxBpm,
                                 yesdaw::ui::UiTheme::Layout::headerTempoStepBpm);
    headerTempoControl.setValue (yesdaw::ui::UiTheme::Layout::headerTempoDefaultBpm,
                                 juce::dontSendNotification);
    headerTempoControl.setColour (juce::Slider::trackColourId, yesdaw::ui::UiTheme::Color::darkControl());
    headerTempoControl.setColour (juce::Slider::textBoxTextColourId, kText);
    headerTempoControl.onValueChange = [this] {
        if (refreshingTimeMapControls || ! headerTempoControl.isEnabled())
            return;

        (void) appModel.setProjectTempoBpm (headerTempoControl.getValue());
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (headerTempoControl);

    configureActionComponent (headerMeterChooser, yesdaw::ui::UiActionId::TransportSetMeter, "Set time signature");
    for (std::size_t i = 0; i < kHeaderMeterChoices.size(); ++i)
        headerMeterChooser.addItem (juce::String (kHeaderMeterChoices[i].first)
                                        + "/" + juce::String (kHeaderMeterChoices[i].second),
                                    static_cast<int> (i) + 1);
    headerMeterChooser.onChange = [this] {
        if (refreshingTimeMapControls)
            return;

        const int selected = headerMeterChooser.getSelectedId();
        if (selected <= 0)
            return;

        const auto& choice = kHeaderMeterChoices[static_cast<std::size_t> (selected - 1)];
        (void) appModel.setProjectMeterSignature (choice.first, choice.second);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (headerMeterChooser);

    configureActionComponent (trackAddButton, yesdaw::ui::UiActionId::TrackAdd, "Add audio track");
    trackAddButton.setButtonText ("+ Track");
    trackAddButton.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::buttonSurface());
    trackAddButton.setColour (juce::TextButton::textColourOffId, kText);
    trackAddButton.onClick = [this] {
        if (appModel.addAudioTrack().dispatched)
            selectedTrackLane = static_cast<int> (appModel.project().tracks.size()) - 1;
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (trackAddButton);

    trackRenameEditor.setComponentID ("shell.tracklist.rename");
    trackRenameEditor.setTooltip ("Rename track: Enter commits, Escape cancels");
    trackRenameEditor.setName ("Rename track");
    trackRenameEditor.setSelectAllWhenFocused (true);
    trackRenameEditor.onReturnKey = [this] { commitTrackRenameEditor(); };
    trackRenameEditor.onEscapeKey = [this] { dismissTrackRenameEditor(); };
    trackRenameEditor.onFocusLost = [this] { dismissTrackRenameEditor(); };
    addChildComponent (trackRenameEditor);

    clipRenameEditor.setComponentID ("shell.timeline.clip.rename");
    clipRenameEditor.setTooltip ("Rename clip: Enter commits, Escape cancels");
    clipRenameEditor.setName ("Rename clip");
    clipRenameEditor.setSelectAllWhenFocused (true);
    clipRenameEditor.onReturnKey = [this] { commitClipRenameEditor(); };
    clipRenameEditor.onEscapeKey = [this] { dismissClipRenameEditor(); };
    clipRenameEditor.onFocusLost = [this] { dismissClipRenameEditor(); };
    addChildComponent (clipRenameEditor);

    // Marker rename editor (E7): same inline pattern as the clip and track editors.
    markerRenameEditor.setComponentID ("shell.timeline.marker.rename");
    markerRenameEditor.setTooltip ("Rename marker: Enter commits, Escape cancels");
    markerRenameEditor.setName ("Rename marker");
    markerRenameEditor.setSelectAllWhenFocused (true);
    markerRenameEditor.onReturnKey = [this] { commitMarkerRenameEditor(); };
    markerRenameEditor.onEscapeKey = [this] { dismissMarkerRenameEditor(); };
    markerRenameEditor.onFocusLost = [this] { dismissMarkerRenameEditor(); };
    addChildComponent (markerRenameEditor);

    // Snap grid picker (usable-DAW P1): the four registered snap actions surfaced as one control;
    // the model derives real frame grids from the head tempo/meter.
    configureActionComponent (timelineSnapChooser, yesdaw::ui::UiActionId::TimelineSnapSetBeat, "Snap grid");
    timelineSnapChooser.setComponentID ("timeline.snap.chooser");
    timelineSnapChooser.addItem ("Snap Off", 1);
    timelineSnapChooser.addItem ("Bar", 2);
    timelineSnapChooser.addItem ("Beat", 3);
    timelineSnapChooser.addItem ("1/16", 4);
    timelineSnapChooser.setSelectedId (3, juce::dontSendNotification);
    timelineSnapChooser.onChange = [this] {
        if (refreshingSnapChooser)
            return;

        const int selected = timelineSnapChooser.getSelectedId();
        const yesdaw::ui::UiActionId action =
            selected == 1 ? yesdaw::ui::UiActionId::TimelineSnapDisable
            : selected == 2 ? yesdaw::ui::UiActionId::TimelineSnapSetBar
            : selected == 4 ? yesdaw::ui::UiActionId::TimelineSnapSetSixteenth
            : yesdaw::ui::UiActionId::TimelineSnapSetBeat;
        (void) appModel.dispatch (action);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (timelineSnapChooser);

    // G1.4: the Nudge value chooser — four registered verbs as one control.
    // G2.7: the Snap mode chooser — Grid / Relative / Events / Off (the unit stays in the
    // toolbar's snap chooser; Ctrl inverts during a drag; the G2.3 landing line shows it).
    configureActionComponent (snapModeChooser, yesdaw::ui::UiActionId::TimelineSnapModeGrid, "Snap mode");
    snapModeChooser.setComponentID ("timeline.snap_mode.chooser");
    snapModeChooser.setName ("Snap mode");
    snapModeChooser.setTitle ("Snap mode");
    snapModeChooser.addItem ("Snap: Grid", 1);
    snapModeChooser.addItem ("Snap: Relative", 2);
    snapModeChooser.addItem ("Snap: Events", 3);
    snapModeChooser.addItem ("Snap: Off", 4);
    snapModeChooser.setSelectedId (1, juce::dontSendNotification);
    snapModeChooser.onChange = [this] {
        if (refreshingSnapModeChooser)
            return;
        const int selected = snapModeChooser.getSelectedId();
        handleAction (selected == 2 ? yesdaw::ui::UiActionId::TimelineSnapModeRelative
                      : selected == 3 ? yesdaw::ui::UiActionId::TimelineSnapModeEvents
                      : selected == 4 ? yesdaw::ui::UiActionId::TimelineSnapModeOff
                                      : yesdaw::ui::UiActionId::TimelineSnapModeGrid);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (snapModeChooser);

    // G2.6: the Edit mode chooser — Overlap / No Overlap / Shuffle — one setting the placing
    // and removing verbs consult; the Edit menu carries the same three, ticked.
    configureActionComponent (editModeChooser, yesdaw::ui::UiActionId::EditModeOverlap, "Edit mode");
    editModeChooser.setComponentID ("timeline.edit_mode.chooser");
    editModeChooser.setName ("Edit mode");
    editModeChooser.setTitle ("Edit mode");
    editModeChooser.addItem ("Edit: Overlap", 1);
    editModeChooser.addItem ("Edit: No Overlap", 2);
    editModeChooser.addItem ("Edit: Shuffle", 3);
    editModeChooser.setSelectedId (1, juce::dontSendNotification);
    editModeChooser.onChange = [this] {
        if (refreshingEditModeChooser)
            return;
        const int selected = editModeChooser.getSelectedId();
        handleAction (selected == 2 ? yesdaw::ui::UiActionId::EditModeNoOverlap
                      : selected == 3 ? yesdaw::ui::UiActionId::EditModeShuffle
                                      : yesdaw::ui::UiActionId::EditModeOverlap);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (editModeChooser);

    nudgeValueChooser.setComponentID ("timeline.nudge.chooser");
    nudgeValueChooser.setName ("Nudge value");
    nudgeValueChooser.setTitle ("Nudge value");
    nudgeValueChooser.setTooltip ("Nudge value: the distance Alt+Left / Alt+Right move the selection");
    nudgeValueChooser.addItem ("Nudge: Grid", 1);
    nudgeValueChooser.addItem ("Nudge: Bar", 2);
    nudgeValueChooser.addItem ("Nudge: Beat", 3);
    nudgeValueChooser.addItem ("Nudge: 1/16", 4);
    nudgeValueChooser.addItem ("Nudge: 1 ms", 5);      // G2.8
    nudgeValueChooser.addItem ("Nudge: 10 ms", 6);
    nudgeValueChooser.addItem ("Nudge: 1 Frame", 7);
    nudgeValueChooser.addItem ("Nudge: 1 Sample", 8);
    nudgeValueChooser.setSelectedId (1, juce::dontSendNotification);
    nudgeValueChooser.onChange = [this] {
        if (refreshingNudgeChooser)
            return;
        const int selected = nudgeValueChooser.getSelectedId();
        const yesdaw::ui::UiActionId action =
            selected == 2 ? yesdaw::ui::UiActionId::EditNudgeValueBar
            : selected == 3 ? yesdaw::ui::UiActionId::EditNudgeValueBeat
            : selected == 4 ? yesdaw::ui::UiActionId::EditNudgeValueSixteenth
            : selected == 5 ? yesdaw::ui::UiActionId::EditNudgeValueMs1      // G2.8
            : selected == 6 ? yesdaw::ui::UiActionId::EditNudgeValueMs10
            : selected == 7 ? yesdaw::ui::UiActionId::EditNudgeValueFrame
            : selected == 8 ? yesdaw::ui::UiActionId::EditNudgeValueSample
                            : yesdaw::ui::UiActionId::EditNudgeValueGrid;
        (void) appModel.dispatch (action);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (nudgeValueChooser);

    // G1.4: the Inspector toggle (I) at the toolbar's right end.
    configureActionComponent (inspectorToggle, yesdaw::ui::UiActionId::ViewToggleInspector, "Inspector");
    inspectorToggle.setButtonText ("I");   // G2.1 cp3: the letter cluster (tooltip carries the name + chord)
    inspectorToggle.setClickingTogglesState (false);
    inspectorToggle.onClick = [this] {
        handleAction (yesdaw::ui::UiActionId::ViewToggleInspector);
        refreshActionState();
        resized();
        repaintAll();
    };
    addAndMakeVisible (inspectorToggle);

    // G2.1: three draggable splitters (header | lanes | inspector, and the dock above).
    railSplitter.setComponentID ("shell.splitter.rail");
    railSplitter.setTooltip ("Drag to resize the track headers");
    inspectorSplitter.setTooltip ("Drag to resize the inspector");
    dockSplitter.setTooltip ("Drag to resize the editor dock");
    inspectorSplitter.setComponentID ("shell.splitter.inspector");
    dockSplitter.setComponentID ("shell.splitter.dock");
    railSplitter.onDrag = [this] (juce::Point<int> pointer) { setRailWidth (pointer.x); };
    inspectorSplitter.onDrag = [this] (juce::Point<int> pointer) { setInspectorWidth (getWidth() - pointer.x); };
    dockSplitter.onDrag = [this] (juce::Point<int> pointer) { setDockHeight (getHeight() - pointer.y); };
    for (yesdaw::ui::SplitterComponent* splitter : { &railSplitter, &inspectorSplitter, &dockSplitter })
    {
        splitter->onDragEnd = [this] { saveViewState(); };
        addAndMakeVisible (*splitter);
    }

    // G1.5: the keymap editor — hidden until Alt+K; every seam is the registry / the model.
    keymapEditor.rowsProvider = [this] (const juce::String& filter) {
        std::vector<yesdaw::ui::UiActionId> rows;
        const juce::String needle = filter.trim().toLowerCase();
        for (const auto& descriptor : appModel.registry().actions())
        {
            const juce::String haystack = (juce::String (descriptor.label) + " " + descriptor.stableId + " "
                                           + appModel.registry().keymap().chordFor (descriptor.id) + " "
                                           + yesdaw::ui::focusContextName (yesdaw::ui::defaultFocusContext (descriptor.id))).toLowerCase();
            if (needle.isEmpty() || haystack.contains (needle))
                rows.push_back (descriptor.id);
        }
        return rows;
    };
    keymapEditor.keymapProvider = [this] () -> const yesdaw::ui::Keymap& { return appModel.registry().keymap(); };
    keymapEditor.onRebind = [this] (yesdaw::ui::UiActionId action, const juce::String& chord) -> juce::String {
        const yesdaw::ui::KeymapRebindStatus status = appModel.rebindChord (action, chord.toStdString());
        refreshActionState();
        repaintAll();
        switch (status)
        {
            case yesdaw::ui::KeymapRebindStatus::Ok:             return "Bound " + chord + " to " + appModel.registry().descriptor (action)->label;
            case yesdaw::ui::KeymapRebindStatus::EmptyChord:     return "Type a chord first";
            case yesdaw::ui::KeymapRebindStatus::DuplicateChord: return juce::String (appModel.statusLineText());
            case yesdaw::ui::KeymapRebindStatus::UnknownAction:  break;
        }
        return "Unknown verb";
    };
    keymapEditor.onUnbind = [this] (yesdaw::ui::UiActionId action) {
        appModel.unbindChord (action);
        refreshActionState();
        repaintAll();
    };
    keymapEditor.onRestoreDefaults = [this] {
        appModel.restoreDefaultKeymap();
        refreshActionState();
        repaintAll();
    };
    keymapEditor.onClose = [this] {
        handleAction (yesdaw::ui::UiActionId::HelpShowKeymap);
        refreshActionState();
        resized();
        repaintAll();
    };
    addChildComponent (keymapEditor);
    // G3.1: the instrument panel — a dock tab fed by the selected Track's slot.
    instrumentPanel.kindProvider = [this] {
        const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument();
        return track != nullptr ? juce::String (instrumentKindName (track->instrumentKind)) : juce::String ("No track");
    };
    instrumentPanel.kindChoicesProvider = [] { return std::vector<juce::String> { "None (auto)", "SimpleSynth", "Sampler" }; };   // G3.9
    // G3.9: the Sampler's pad grid and its verbs.
    instrumentPanel.padsProvider = [this] {
        const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument();
        return track != nullptr && track->instrumentKind == yesdaw::engine::TrackInstrumentKind::Sampler;
    };
    instrumentPanel.padRowsProvider = [this] {
        std::vector<InstrumentPanelComponent::Pad> pads;
        if (const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument())
            for (const yesdaw::engine::SamplerPad& pad : track->samplerPads)
                pads.push_back ({ static_cast<int> (pad.key), juce::String (std::string (pad.nameView())), pad.oneShot, true });
        return pads;
    };
    instrumentPanel.onPadClicked = [this] (int key, bool shift, bool ctrl) {
        const std::int16_t padKey = static_cast<std::int16_t> (key);
        if (ctrl)
        {
            (void) appModel.clearSamplerPadOnSelectedTrack (padKey);
            recordLastAction (yesdaw::ui::UiActionId::SamplerPadClear);
        }
        else if (shift)
        {
            (void) appModel.toggleSamplerPadModeOnSelectedTrack (padKey);
            recordLastAction (yesdaw::ui::UiActionId::SamplerPadModeToggle);
        }
        else if (fileChoices.chooseSamplerPadFile)
        {
            const std::filesystem::path path = fileChoices.chooseSamplerPadFile();
            if (! path.empty())
                loadSamplerPadFromPath (padKey, path);
        }
        refreshActionState();
        resized();
        repaintAll();
    };
    instrumentPanel.onPadFilesDropped = [this] (int key, const juce::StringArray& files) {
        for (const juce::String& file : files)
            if (juce::File (file).hasFileExtension ("wav;wave"))
            {
                loadSamplerPadFromPath (static_cast<std::int16_t> (key), std::filesystem::path (file.toStdString()));
                break;   // one file, one pad
            }
        refreshActionState();
        resized();
        repaintAll();
    };
    instrumentPanel.kindIndexProvider = [this] {
        const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument();
        return track != nullptr ? static_cast<int> (track->instrumentKind) : -1;
    };
    instrumentPanel.onKindChosen = [this] (int index) {
        (void) appModel.setInstrumentOnSelectedTrack (static_cast<yesdaw::engine::TrackInstrumentKind> (index));
        refreshActionState();
        repaintAll();
    };
    instrumentPanel.rowsProvider = [this] { return instrumentPanelRows(); };
    instrumentPanel.onRowDragStart = [this] (std::uint32_t paramId) {
        appModel.beginStripGesture();   // G3.1 checkpoint (SS-4 found it): one drag = one undo step (E21)
        if (const yesdaw::engine::Track* const track = appModel.selectedTrackForInstrument())
            beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole::InstrumentParam, paramId, track->id);
    };
    instrumentPanel.onRowDragEnd = [this] {
        endAutomationTouchRideIfActive();
        appModel.endStripGesture();
    };
    instrumentPanel.onRowValue = [this] (std::uint32_t paramId, double normalized) {
        if (automationTouchRideActive)
            recordAutomationTouchSample (normalized);
        else
            (void) appModel.setInstrumentParamOnSelectedTrack (paramId, normalized);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (instrumentPanel);

    // G3.1: the inspector's TRACK tab carries the kind chooser and an Edit button (opens the tab).
    configureActionComponent (inspectorInstrumentChooser, yesdaw::ui::UiActionId::TrackSetInstrument, "Track instrument");
    inspectorInstrumentChooser.setComponentID ("track.inspector.instrument");
    inspectorInstrumentChooser.addItem ("None (auto)", 1);
    inspectorInstrumentChooser.addItem ("SimpleSynth", 2);
    inspectorInstrumentChooser.addItem ("Sampler", 3);   // G3.9
    inspectorInstrumentChooser.onChange = [this] {
        if (refreshingInspectorControls)
            return;
        const int selected = inspectorInstrumentChooser.getSelectedId();
        if (selected <= 0)
            return;
        (void) appModel.setInstrumentOnSelectedTrack (static_cast<yesdaw::engine::TrackInstrumentKind> (selected - 1));
        refreshActionState();
        repaintAll();
    };
    addChildComponent (inspectorInstrumentChooser);
    configureActionComponent (inspectorInstrumentEdit, yesdaw::ui::UiActionId::ViewInstrument, "Instrument panel");
    inspectorInstrumentEdit.setComponentID ("track.inspector.instrument.edit");
    inspectorInstrumentEdit.setButtonText ("Edit");
    inspectorInstrumentEdit.onClick = [this] {
        handleAction (yesdaw::ui::UiActionId::ViewInstrument);
        refreshActionState();
        resized();
        repaintAll();
    };
    addChildComponent (inspectorInstrumentEdit);

    // G3.5: the MIDI clip's settings rows (Logic's region inspector: Mute, Transpose, Velocity, Loop)
    // sit above the quantize rows on the same CLIP tab; every control is an undoable Clip edit.
    configureActionComponent (inspectorMidiMute, yesdaw::ui::UiActionId::MidiClipMuteToggle, "Mute MIDI clip");
    inspectorMidiMute.setComponentID ("clip.inspector.midi.mute");
    inspectorMidiMute.setButtonText ("");
    inspectorMidiMute.onClick = [this] {
        if (refreshingInspectorControls)
            return;
        // The row mutes the clip the rows show (not the timeline selection, which may be empty).
        (void) appModel.toggleSelectedMidiClipMute();
        recordLastAction (yesdaw::ui::UiActionId::MidiClipMuteToggle);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (inspectorMidiMute);
    const auto setUpMidiClipSlider = [this] (juce::Slider& slider, yesdaw::ui::UiActionId action, const char* id,
                                             double minimum, double maximum, std::function<void (int)> post) {
        configureActionComponent (slider, action, "MIDI clip");
        slider.setComponentID (id);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        slider.setRange (minimum, maximum, 1.0);
        slider.setValue (0.0, juce::dontSendNotification);
        slider.onValueChange = [this, &slider, action, post] {
            if (refreshingInspectorControls || ! slider.isEnabled())
                return;
            post (juce::roundToInt (slider.getValue()));
            recordLastAction (action);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (slider);
    };
    setUpMidiClipSlider (inspectorMidiTranspose, yesdaw::ui::UiActionId::MidiClipTransposeSet, "clip.inspector.midi.transpose",
                         -static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipTransposeMax),
                         static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipTransposeMax),
                         [this] (int v) { (void) appModel.setSelectedMidiClipTranspose (v); });
    setUpMidiClipSlider (inspectorMidiVelocity, yesdaw::ui::UiActionId::MidiClipVelocityOffsetSet, "clip.inspector.midi.velocity",
                         -static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipVelocityOffsetMax),
                         static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipVelocityOffsetMax),
                         [this] (int v) { (void) appModel.setSelectedMidiClipVelocityOffset (static_cast<double> (v) / 100.0); });
    configureActionComponent (inspectorMidiLoop, yesdaw::ui::UiActionId::MidiClipLoopSelect, "MIDI clip loop");
    inspectorMidiLoop.setComponentID ("clip.inspector.midi.loop");
    inspectorMidiLoop.addItem ("Off", 1);
    inspectorMidiLoop.addItem ("1 beat", 2);
    inspectorMidiLoop.addItem ("1 bar", 3);
    inspectorMidiLoop.addItem ("2 bars", 4);
    inspectorMidiLoop.addItem ("4 bars", 5);
    inspectorMidiLoop.setSelectedId (1, juce::dontSendNotification);
    inspectorMidiLoop.onChange = [this] {
        if (refreshingInspectorControls || inspectorMidiLoop.getSelectedId() <= 0)
            return;
        (void) appModel.setSelectedMidiClipLoopChoice (inspectorMidiLoop.getSelectedId() - 1);
        recordLastAction (yesdaw::ui::UiActionId::MidiClipLoopSelect);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (inspectorMidiLoop);

    // G3.4: the quantize panel — the CLIP tab's content for a MIDI clip (Logic's region
    // inspector: Quantize, Q-Strength, Q-Swing, Q-Length; humanize). Settings, not edits:
    // each control posts its value to the context and Q (or Apply) applies them.
    configureActionComponent (inspectorQuantizeGrid, yesdaw::ui::UiActionId::QuantizeGridSelect, "Quantize grid");
    inspectorQuantizeGrid.setComponentID ("clip.inspector.quantize.grid");
    inspectorQuantizeGrid.addItem ("Snap grid", 1);
    inspectorQuantizeGrid.addItem ("1/8", 2);
    inspectorQuantizeGrid.addItem ("1/16", 3);
    inspectorQuantizeGrid.addItem ("1/32", 4);
    inspectorQuantizeGrid.setSelectedId (1, juce::dontSendNotification);
    inspectorQuantizeGrid.onChange = [this] {
        if (refreshingInspectorControls || inspectorQuantizeGrid.getSelectedId() <= 0)
            return;
        (void) appModel.selectQuantizeGrid (inspectorQuantizeGrid.getSelectedId() - 1);
        recordLastAction (yesdaw::ui::UiActionId::QuantizeGridSelect);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (inspectorQuantizeGrid);
    const auto setUpQuantizeSlider = [this] (juce::Slider& slider, yesdaw::ui::UiActionId action, const char* id,
                                             double maximum, double initial, std::function<void (int)> post) {
        configureActionComponent (slider, action, "Quantize");
        slider.setComponentID (id);
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxWidth,
                                yesdaw::ui::UiTheme::Layout::hiddenSliderTextBoxHeight);
        slider.setRange (0.0, maximum, 1.0);
        slider.setValue (initial, juce::dontSendNotification);
        slider.onValueChange = [this, &slider, action, post] {
            if (refreshingInspectorControls || ! slider.isEnabled())
                return;
            post (juce::roundToInt (slider.getValue()));
            recordLastAction (action);
            refreshActionState();
            repaintAll();
        };
        addChildComponent (slider);
    };
    setUpQuantizeSlider (inspectorQuantizeStrength, yesdaw::ui::UiActionId::QuantizeStrengthSet, "clip.inspector.quantize.strength",
                         100.0, 100.0, [this] (int v) { (void) appModel.setQuantizeStrength (v); });
    setUpQuantizeSlider (inspectorQuantizeSwing, yesdaw::ui::UiActionId::QuantizeSwingSet, "clip.inspector.quantize.swing",
                         static_cast<double> (yesdaw::ui::UiTheme::Layout::inspectorQuantizeSwingMax), 0.0,
                         [this] (int v) { (void) appModel.setQuantizeSwing (v); });
    setUpQuantizeSlider (inspectorQuantizeHumanize, yesdaw::ui::UiActionId::QuantizeHumanizeSet, "clip.inspector.quantize.humanize",
                         100.0, 0.0, [this] (int v) { (void) appModel.setQuantizeHumanize (v); });
    configureActionComponent (inspectorQuantizeEnds, yesdaw::ui::UiActionId::QuantizeNoteEndsToggle, "Quantize note ends");
    inspectorQuantizeEnds.setComponentID ("clip.inspector.quantize.ends");
    inspectorQuantizeEnds.setButtonText ("");   // the painted row label names it (rubric: no double "Note ends")
    inspectorQuantizeEnds.onClick = [this] {
        if (refreshingInspectorControls)
            return;
        (void) appModel.toggleQuantizeNoteEnds();
        recordLastAction (yesdaw::ui::UiActionId::QuantizeNoteEndsToggle);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (inspectorQuantizeEnds);
    configureActionComponent (inspectorQuantizeApply, yesdaw::ui::UiActionId::PianoRollNoteQuantizeSelection, "Quantize");
    inspectorQuantizeApply.setComponentID ("clip.inspector.quantize.apply");
    inspectorQuantizeApply.setButtonText ("Apply (Q)");
    inspectorQuantizeApply.onClick = [this] {
        handleAction (yesdaw::ui::UiActionId::PianoRollNoteQuantizeSelection);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (inspectorQuantizeApply);

    // G2.18: the undo history window rides the same overlay law.
    undoHistory.rowsProvider = [this] {
        std::vector<juce::String> rows;
        for (const auto& step : appModel.undoHistory().steps)
        {
            juce::String label (yesdaw::engine::projectEditVerbLabel (step.verb));
            if (step.entryCount > 1)
                label << " (" << static_cast<int> (step.entryCount) << " edits)";
            rows.push_back (label);
        }
        return rows;
    };
    undoHistory.currentProvider = [this] { return static_cast<int> (appModel.undoHistory().current); };
    undoHistory.onRowClicked = [this] (int row) {
        if (row < 0)
            return;
        (void) appModel.jumpToUndoHistoryStep (static_cast<std::size_t> (row));
        undoHistory.refreshRows();
        refreshActionState();
        resized();
        repaintAll();
    };
    undoHistory.onClose = [this] {
        handleAction (yesdaw::ui::UiActionId::EditShowUndoHistory);
        refreshActionState();
        resized();
        repaintAll();
    };
    addChildComponent (undoHistory);


    configureAutomationLaneControls();
    configureMixerDockToggle();
    configureInspectorTabs();
    configureTimelineZoomControls();

    // Automation lane canvas (usable-DAW P1): breakpoints drawn and edited against the SAME
    // timeline viewport math as the arrangement; targets the selected track's fader lane.
    automationLaneCanvas.setComponentID ("timeline.automation.canvas");
    automationLaneCanvas.setTooltip ("Automation lane: click to add a breakpoint, drag to move it");
    automationLaneCanvas.setName ("Automation Lane");
    automationLaneCanvas.setTitle ("Automation Lane");
    // E20: the canvas reads and edits the CHOSEN target's lane (fader/pan/send/FX param).
    automationLaneCanvas.pointsProvider = [this] {
        std::vector<AutomationLaneCanvasComponent::CanvasPoint> points;
        const AutomationTargetOption target = currentAutomationTarget();
        if (! target.ownerEntity.isValid() || ! appModel.project().sampleRate.isValid())
            return points;

        if (const yesdaw::engine::AutomationLaneData* const lane =
                appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId))
        {
            const double sampleRateHz = appModel.project().sampleRate.hz;
            points.reserve (lane->points.size());
            for (const yesdaw::engine::AutomationBreakpoint& point : lane->points)
                points.push_back ({ static_cast<double> (point.tick) / sampleRateHz,
                                    point.value,
                                    point.curveType });
        }
        return points;
    };
    // R16: Alt+click a breakpoint handle cycles its curve shape through the undoable
    // SetAutomationBreakpointCurve verb — Linear → Hold → Bezier → Log → Linear.
    automationLaneCanvas.onCycleCurvePoint = [this] (double seconds) {
        const AutomationTargetOption target = currentAutomationTarget();
        const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
            ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
            : nullptr;
        if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds);
            lane != nullptr && tick)
        {
            (void) appModel.cycleAutomationBreakpointCurveAtTick (lane->id, *tick);
            refreshActionState();
            repaintAll();
        }
    };
    automationLaneCanvas.secondsForLocalX = [this] (int localX) {
        return automationCanvasSecondsForLocalX (localX);
    };
    automationLaneCanvas.localXForSeconds = [this] (double seconds) {
        return automationCanvasLocalXForSeconds (seconds);
    };
    // E20: added and dragged breakpoints land on the snap chooser's grid (chooser Off = raw).
    automationLaneCanvas.onAddPoint = [this] (double seconds, double value) {
        const AutomationTargetOption target = currentAutomationTarget();
        if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds);
            tick && target.ownerEntity.isValid())
        {
            (void) appModel.addAutomationBreakpointToLane (
                target.ownerEntity, target.role, target.paramId,
                snappedTimelineTick (*tick, false), value);
            refreshActionState();
            repaintAll();
        }
    };
    automationLaneCanvas.onMovePoint = [this] (double oldSeconds, double newSeconds, double newValue) {
        const AutomationTargetOption target = currentAutomationTarget();
        const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
            ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
            : nullptr;
        const std::optional<yesdaw::engine::Tick> oldTick = timelineTickFromSeconds (oldSeconds);
        const std::optional<yesdaw::engine::Tick> newTick = timelineTickFromSeconds (newSeconds);
        if (lane != nullptr && oldTick && newTick)
        {
            (void) appModel.moveAutomationBreakpointTo (lane->id, *oldTick,
                                                        snappedTimelineTick (*newTick, false),
                                                        newValue);
            refreshActionState();
            repaintAll();
        }
    };
    automationLaneCanvas.onDeletePoint = [this] (double seconds) {
        const AutomationTargetOption target = currentAutomationTarget();
        const yesdaw::engine::AutomationLaneData* const lane = target.ownerEntity.isValid()
            ? appModel.automationLaneForTarget (target.ownerEntity, target.role, target.paramId)
            : nullptr;
        if (const std::optional<yesdaw::engine::Tick> tick = timelineTickFromSeconds (seconds);
            lane != nullptr && tick)
        {
            (void) appModel.removeAutomationBreakpointAtTick (lane->id, *tick);
            refreshActionState();
            repaintAll();
        }
    };
    addChildComponent (automationLaneCanvas);

    pianoRollInput.setComponentID (kPianoRollComponentId);
    pianoRollInput.setTooltip ("Piano roll: click to pencil a note, drag to move, Ctrl+drag to copy, Alt+wheel for velocity");
    pianoRollInput.setName ("Piano Roll");
    pianoRollInput.setTitle ("Piano Roll");
    pianoRollInput.stateProvider = [this] { return currentPianoRollSurface(); };
    pianoRollInput.onContextMenuRequested = [this] (yesdaw::ui::ContextMenuTarget target, int index, juce::Point<int> position) {
        openContextMenu (target, index, pianoRollInput, position);
    };
    pianoRollInput.onNoteClicked = [this] (yesdaw::engine::EntityId midiClipId,
                                           yesdaw::engine::EntityId noteId) {
        // E12: a plain press on a selected member keeps the group for the drag.
        (void) appModel.selectPianoRollNoteForGesture (midiClipId, noteId);
        refreshActionState();
        repaintAll();
    };
    // Piano-roll selection tools (E11).
    pianoRollInput.activeToolProvider = [this] {
        return appModel.context().activeTimelineTool;
    };
    pianoRollInput.onNoteToggled = [this] (yesdaw::engine::EntityId midiClipId,
                                           yesdaw::engine::EntityId noteId) {
        (void) appModel.togglePianoRollNoteSelection (midiClipId, noteId);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNotesMarqueeSelected = [this] (yesdaw::engine::EntityId midiClipId,
                                                    std::span<const yesdaw::engine::EntityId> noteIds) {
        juce::ignoreUnused (midiClipId);
        (void) appModel.selectPianoRollNotes (noteIds);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onSelectionCleared = [this] {
        appModel.clearPianoRollNoteSelection();
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNoteDeleted = [this] (yesdaw::engine::EntityId midiClipId,
                                           yesdaw::engine::EntityId noteId) {
        if (appModel.selectPianoRollNote (midiClipId, noteId).dispatched)
            (void) appModel.deleteSelectedPianoRollNotes();
        refreshActionState();
        repaintAll();
    };
    // Piano-roll viewport wheel map (E10): plain wheel scrolls keys, Shift+wheel scrolls
    // time, Ctrl+wheel zooms time anchored at the pointer tick.
    pianoRollInput.onViewKeysScrolled = [this] (int keyDelta) {
        pianoRollViewLowKey = std::clamp (
            pianoRollViewLowKey + keyDelta,
            yesdaw::ui::UiThemeLayout::pianoRollKeyMin,
            yesdaw::ui::UiThemeLayout::pianoRollKeyMax
                - (currentPianoRollSurface().viewKeyCount - 1));   // G3.2 FIX 3
        repaintAll();
    };
    pianoRollInput.onViewZoomWheel = [this] (yesdaw::engine::Tick anchorTick, double wheelDelta) {
        const double factor = wheelDelta > 0.0
            ? yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep
            : 1.0 / yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep;
        const double previousZoom = pianoRollViewZoom;
        pianoRollViewZoom = std::clamp (pianoRollViewZoom * factor,
                                        yesdaw::ui::UiThemeLayout::pianoRollZoomMin,
                                        yesdaw::ui::UiThemeLayout::pianoRollZoomMax);
        if (pianoRollViewZoom != previousZoom)
        {
            const double zoomRatio = previousZoom / pianoRollViewZoom;
            pianoRollViewScrollTicks = anchorTick
                - static_cast<yesdaw::engine::Tick> (
                    std::llround (static_cast<double> (anchorTick - pianoRollViewScrollTicks) * zoomRatio));
        }
        if (pianoRollViewZoom == yesdaw::ui::UiThemeLayout::pianoRollZoomMin)
            pianoRollViewScrollTicks = 0;
        repaintAll();
    };
    pianoRollInput.onViewTicksScrolled = [this] (double wheelDelta) {
        const yesdaw::ui::UiPianoRollSurfaceSnapshot surface = currentPianoRollSurface();
        pianoRollViewScrollTicks -= static_cast<yesdaw::engine::Tick> (
            std::llround (wheelDelta
                          * static_cast<double> (pianoRollVisibleTicks (surface))
                          * yesdaw::ui::UiTheme::Layout::timelineScrollWheelFraction));
        pianoRollViewScrollTicks = juce::jmax<yesdaw::engine::Tick> (0, pianoRollViewScrollTicks);
        repaintAll();
    };
    // E12: the drag moves the whole selection (anchored on the dragged note) by the snapped
    // tick delta and the row-derived key delta as one undo transaction.
    pianoRollInput.onNotesDragged = [this] (yesdaw::engine::EntityId midiClipId,
                                            yesdaw::engine::EntityId noteId,
                                            yesdaw::engine::Tick tickDelta,
                                            int keyDelta) {
        (void) appModel.selectPianoRollNoteForGesture (midiClipId, noteId);
        (void) appModel.moveSelectedPianoRollNotesBy (tickDelta, keyDelta);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNoteHeadTrimmed = [this] (yesdaw::engine::EntityId midiClipId,
                                               yesdaw::engine::EntityId noteId,
                                               yesdaw::engine::Tick newStart) {
        (void) appModel.selectPianoRollNote (midiClipId, noteId);
        (void) appModel.trimSelectedPianoRollNoteHeadTo (newStart);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNoteLengthChanged = [this] (yesdaw::engine::EntityId midiClipId,
                                                 yesdaw::engine::EntityId noteId,
                                                 yesdaw::engine::Tick lengthTicks) {
        (void) appModel.selectPianoRollNote (midiClipId, noteId);
        (void) appModel.setSelectedPianoRollNoteLength (lengthTicks);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNoteTransposed = [this] (yesdaw::engine::EntityId midiClipId,
                                              yesdaw::engine::EntityId noteId,
                                              std::int32_t semitones) {
        (void) appModel.selectPianoRollNote (midiClipId, noteId);
        (void) appModel.transposeSelectedPianoRollNote (semitones);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNoteQuantized = [this] (yesdaw::engine::EntityId midiClipId,
                                             yesdaw::engine::EntityId noteId,
                                             yesdaw::engine::Tick snapGridTicks) {
        (void) appModel.selectPianoRollNote (midiClipId, noteId);
        (void) appModel.quantizeSelectedPianoRollNoteTo (yesdaw::engine::SnapGrid { snapGridTicks });
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNoteAdded = [this] (yesdaw::engine::EntityId midiClipId, yesdaw::engine::Tick tick, std::int16_t key) {
        (void) midiClipId;
        // G3.2 checkpoint FIX 2: a sixteenth of the beat in force (Logic's default division), not
        // the pre-G3.2 512-tick grid step, which drew a dot. The roll's own division chooser is G3.4.
        const yesdaw::ui::UiPianoRollSurfaceSnapshot rollSurface = currentPianoRollSurface();
        const yesdaw::engine::Tick sixteenth =
            std::max<yesdaw::engine::Tick> (1, rollSurface.beatTicks / 4);
        // G3.8: scale assist — the pencil lands on the nearest key inside the project's scale.
        (void) appModel.addPianoRollNoteAt (tick, sixteenth, yesdaw::ui::pianoRollScaleSnappedKey (key, rollSurface.scaleRoot, rollSurface.scaleChoice));
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNoteSplit = [this] (yesdaw::engine::EntityId midiClipId, yesdaw::engine::EntityId noteId, yesdaw::engine::Tick tick) {   // G3.2
        (void) appModel.splitPianoRollNoteAt (midiClipId, noteId, tick);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onKeyAuditioned = [this] (std::int16_t key, bool on) {   // G3.2: audition through the Track's Instrument
        (void) appModel.auditionNote (key, on);
    };
    pianoRollInput.onExpressionRead = [this] {
        (void) appModel.readPianoRollExpressionLanes();
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNoteVelocityAdjusted = [this] (yesdaw::engine::EntityId midiClipId,
                                                    yesdaw::engine::EntityId noteId,
                                                    double normalizedVelocity) {
        (void) appModel.selectPianoRollNote (midiClipId, noteId);
        (void) appModel.setSelectedPianoRollNoteVelocity (normalizedVelocity);
        refreshActionState();
        repaintAll();
    };
    // E13: the lane paint gesture-selects its anchor (keeping a group the anchor belongs to)
    // and paints the batch as one undo transaction.
    pianoRollInput.onVelocityLanePainted =
        [this] (yesdaw::engine::EntityId midiClipId,
                std::span<const std::pair<yesdaw::engine::EntityId, double>> edits) {
            if (edits.empty())
                return;
            (void) appModel.selectPianoRollNoteForGesture (midiClipId, edits.front().first);
            (void) appModel.paintPianoRollNoteVelocities (midiClipId, edits);
            refreshActionState();
            repaintAll();
        };
    // G3.3: the control lane's gestures land on the model's Clip verbs (each one undo step).
    pianoRollInput.onControlPointAdded = [this] (yesdaw::engine::EntityId midiClipId,
                                                 yesdaw::engine::Tick tick,
                                                 double value) {
        (void) appModel.addPianoRollControlPoint (midiClipId, appModel.context().pianoRollControlLaneChoice, tick, value);
        recordLastAction (yesdaw::ui::UiActionId::PianoRollControlPointAdd);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onControlPointMoved = [this] (yesdaw::engine::EntityId midiClipId,
                                                 yesdaw::engine::EntityId pointId,
                                                 yesdaw::engine::Tick tick,
                                                 double value) {
        (void) appModel.movePianoRollControlPoint (midiClipId, pointId, tick, value);
        recordLastAction (yesdaw::ui::UiActionId::PianoRollControlPointMove);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onControlPointDeleted = [this] (yesdaw::engine::EntityId midiClipId,
                                                   yesdaw::engine::EntityId pointId) {
        (void) appModel.deletePianoRollControlPoint (midiClipId, pointId);
        recordLastAction (yesdaw::ui::UiActionId::PianoRollControlPointDelete);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onControlLanePainted = [this] (yesdaw::engine::EntityId midiClipId,
                                                  std::span<const std::pair<yesdaw::engine::Tick, double>> points,
                                                  yesdaw::engine::Tick firstTick,
                                                  yesdaw::engine::Tick lastTick) {
        (void) appModel.paintPianoRollControlLane (midiClipId, appModel.context().pianoRollControlLaneChoice, points, firstTick, lastTick);
        recordLastAction (yesdaw::ui::UiActionId::PianoRollControlLanePaint);
        refreshActionState();
        repaintAll();
    };
    pianoRollInput.onNoteCopyDragged = [this] (yesdaw::engine::EntityId midiClipId,
                                               yesdaw::engine::EntityId noteId,
                                               yesdaw::engine::Tick newStartTick) {
        (void) appModel.duplicatePianoRollNote (midiClipId, noteId, newStartTick);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (pianoRollInput);

    // G3.3: the control lane's chooser sits in the lane's keyboard gutter (plan §3.2 "CC1 Mod ▾").
    pianoRollLaneChooser.setComponentID ("pianoroll.lane.chooser");
    pianoRollLaneChooser.setName ("Control lane");
    pianoRollLaneChooser.setTooltip ("Which controller the piano roll's control lane shows: Mod (CC1), Sustain (CC64), Bend (pitch bend), Touch (aftertouch), Program (program change)");
    for (std::size_t i = 0; i < yesdaw::ui::kPianoRollControlLaneChoices.size(); ++i)
        pianoRollLaneChooser.addItem (yesdaw::ui::kPianoRollControlLaneChoices[i].name, static_cast<int> (i) + 1);
    pianoRollLaneChooser.setSelectedId (1, juce::dontSendNotification);
    pianoRollLaneChooser.onChange = [this] {
        const int selected = pianoRollLaneChooser.getSelectedId();
        if (selected <= 0 || selected - 1 == appModel.context().pianoRollControlLaneChoice)
            return;
        (void) appModel.selectPianoRollControlLane (selected - 1);
        recordLastAction (yesdaw::ui::UiActionId::PianoRollControlLaneSelect);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (pianoRollLaneChooser);

    // G3.8: the roll header's Key / Scale choosers — the project's scale assist.
    pianoRollKeyChooser.setComponentID ("pianoroll.key");
    pianoRollKeyChooser.setName ("Key");
    pianoRollKeyChooser.setTooltip ("The project's key: the root of the scale assist");
    {
        static constexpr const char* kKeyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
            pianoRollKeyChooser.addItem (kKeyNames[i], i + 1);
    }
    pianoRollKeyChooser.setSelectedId (1, juce::dontSendNotification);
    pianoRollKeyChooser.onChange = [this] {
        const int selected = pianoRollKeyChooser.getSelectedId();
        if (selected <= 0)
            return;
        (void) appModel.setProjectScale (selected - 1, appModel.context().pianoRollScaleChoice, true);
        recordLastAction (yesdaw::ui::UiActionId::PianoRollScaleRootSelect);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (pianoRollKeyChooser);
    pianoRollScaleChooser.setComponentID ("pianoroll.scale");
    pianoRollScaleChooser.setName ("Scale");
    pianoRollScaleChooser.setTooltip ("Scale assist: Off shows every key; Major / Minor lift the keys inside the project's scale and the pencil lands on them");
    pianoRollScaleChooser.addItem ("Scale: Off", 1);
    pianoRollScaleChooser.addItem ("Major", 2);
    pianoRollScaleChooser.addItem ("Minor", 3);
    pianoRollScaleChooser.setSelectedId (1, juce::dontSendNotification);
    pianoRollScaleChooser.onChange = [this] {
        const int selected = pianoRollScaleChooser.getSelectedId();
        if (selected <= 0)
            return;
        (void) appModel.setProjectScale (appModel.context().pianoRollScaleRoot, selected - 1, false);
        recordLastAction (yesdaw::ui::UiActionId::PianoRollScaleSelect);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (pianoRollScaleChooser);

    // G3.6: the roll header's Typing and Step toggles (plan §3.2 "[step ⏺]"); Ctrl+K is Typing's chord.
    configureActionComponent (pianoRollTypingButton, yesdaw::ui::UiActionId::PianoRollMusicalTypingToggle, "Musical typing");
    pianoRollTypingButton.setComponentID ("pianoroll.typing");
    pianoRollTypingButton.setButtonText ("Typing");
    pianoRollTypingButton.setClickingTogglesState (false);
    pianoRollTypingButton.onClick = [this] {
        handleAction (yesdaw::ui::UiActionId::PianoRollMusicalTypingToggle);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (pianoRollTypingButton);
    configureActionComponent (pianoRollStepButton, yesdaw::ui::UiActionId::PianoRollStepInputToggle, "Step input");
    pianoRollStepButton.setComponentID ("pianoroll.step");
    pianoRollStepButton.setButtonText ("Step");
    pianoRollStepButton.setClickingTogglesState (false);
    pianoRollStepButton.onClick = [this] {
        handleAction (yesdaw::ui::UiActionId::PianoRollStepInputToggle);
        refreshActionState();
        repaintAll();
    };
    addChildComponent (pianoRollStepButton);

    menuBar.setModel (this);
    menuBar.setComponentID ("shell.menubar");
    menuBar.setName ("Menu bar");
    menuBar.setTitle ("Menu bar");
    menuBar.setTooltip ("Application menus: File, Edit, View, Options, Help");
    addAndMakeVisible (menuBar);

    // Real audio device chooser (usable-DAW P1): lists the machine's output devices and switches
    // the live device on selection. The harness injects deterministic device seams; the native
    // shell enumerates and switches through the JUCE device manager.
    audioDeviceChooser.setComponentID ("shell.device.chooser");
    audioDeviceChooser.setTooltip ("Audio output device");
    audioDeviceChooser.setName ("Audio output device");
    audioDeviceChooser.setTitle ("Audio output device");
    audioDeviceChooser.setTextWhenNothingSelected ("Audio Device");
    audioDeviceChooser.setTextWhenNoChoicesAvailable ("No Devices");
    audioDeviceChooser.onChange = [this] {
        if (refreshingAudioDeviceChooser)
            return;

        const int selected = audioDeviceChooser.getSelectedId();
        if (selected <= 0
            || static_cast<std::size_t> (selected - 1) >= audioDeviceChooserNames.size())
            return;

        suspendDesktopAudioCallback();
        const bool switched =
            selectAudioOutputDeviceByName (audioDeviceChooserNames[static_cast<std::size_t> (selected - 1)]);
        resumeDesktopAudioCallback();
        if (switched)
            if (juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
                appModel.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
        refreshAudioDeviceChooser();
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (audioDeviceChooser);

    // E29: the INPUT side gets the same treatment — a real input device chooser plus the
    // recorded-channel pick (mono channel N or the stereo pair) driving the model verb.
    audioInputDeviceChooser.setComponentID ("shell.device.input.chooser");
    audioInputDeviceChooser.setTooltip ("Audio input device");
    audioInputDeviceChooser.setName ("Audio input device");
    audioInputDeviceChooser.setTitle ("Audio input device");
    audioInputDeviceChooser.setTextWhenNothingSelected ("Input Device");
    audioInputDeviceChooser.setTextWhenNoChoicesAvailable ("No Inputs");
    audioInputDeviceChooser.onChange = [this] {
        if (refreshingAudioDeviceChooser)
            return;

        const int selected = audioInputDeviceChooser.getSelectedId();
        if (selected <= 0
            || static_cast<std::size_t> (selected - 1) >= audioInputDeviceChooserNames.size())
            return;

        suspendDesktopAudioCallback();
        (void) selectAudioInputDeviceByName (
            audioInputDeviceChooserNames[static_cast<std::size_t> (selected - 1)]);
        resumeDesktopAudioCallback();
        refreshAudioDeviceChooser();
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (audioInputDeviceChooser);

    recordingInputChannelChooser.setComponentID ("shell.device.input.channel");
    recordingInputChannelChooser.setTooltip ("Recorded input: mono channel or stereo pair");
    recordingInputChannelChooser.setName ("Recorded input channel");
    recordingInputChannelChooser.setTitle ("Recorded input channel");
    recordingInputChannelChooser.setTextWhenNothingSelected ("Input");
    recordingInputChannelChooser.setTextWhenNoChoicesAvailable ("No Inputs");
    recordingInputChannelChooser.onChange = [this] {
        if (refreshingAudioDeviceChooser)
            return;

        const int selected = recordingInputChannelChooser.getSelectedId();
        if (selected <= 0)
            return;

        // Ids: mono channel N -> N+1; stereo pair (N, N+1) -> 1000 + N + 1.
        const bool stereo = selected > 1000;
        const int base = stereo ? selected - 1001 : selected - 1;
        (void) appModel.setRecordingInputChannel (static_cast<std::uint16_t> (base), stereo);
        refreshActionState();
        repaintAll();
    };
    addAndMakeVisible (recordingInputChannelChooser);
    recordStartupStage ("configure-controls");
    // The native audio initialiser already scans/selects the backend; publish its
    // device lists once after that attempt. Injected shells still populate now.
    if (! desktopAudioRequested)
        refreshAudioDeviceChooser();
    recordStartupStage ("initial-device-enumeration");

    configureInspectorControls();
    configureMixerControls();
    resized();
    refreshActionState();
    hideMixerControlsBehindDockTab();   // G3.2 checkpoint FIX 1

    recordStartupStage ("configure-mixer-layout");
    if (desktopAudioRequested || fileChoices.initialiseSessionAtLaunch)
    {
        // Native shell only: remember and reopen the last project so a crash-then-relaunch reaches
        // the autosave recovery prompt with no manual navigation (usable-DAW P1). The harness never
        // takes this path, so injected-choice tests stay deterministic.
        // G0.1: a Session drive redirects the records (YESDAW_SESSION_STATE_DIR) so a driven
        // launch never reads or rewrites the owner's real last-project record.
        if (fileChoices.sessionStateDirectory.empty())
        {
            const std::string sessionUtf8 =
                juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                    .getChildFile ("YES DAW").getFullPathName().toStdString();
            const auto* sessionBytes = reinterpret_cast<const char8_t*> (sessionUtf8.data());
            fileChoices.sessionStateDirectory =
                std::filesystem::path { std::u8string (sessionBytes, sessionBytes + sessionUtf8.size()) };
            appModel.setSessionStateDirectory (fileChoices.sessionStateDirectory);
        }
        // G0.1: a bundle named on the command line wins over the last-project record.
        const std::filesystem::path lastProject = ! fileChoices.openBundleAtLaunch.empty()
                                                      ? fileChoices.openBundleAtLaunch
                                                      : appModel.readLastProjectRecord();
        if (! lastProject.empty())
        {
            StoredProjectAssetsResult stored = decodeStoredProjectAssets (lastProject);
            if (stored.assets && ! stored.assets->empty())
                (void) appModel.loadPreparedProjectBundle (
                    std::move (stored.prepared), std::move (*stored.assets));
            else if (stored.assets)
                (void) appModel.openPreparedProjectBundle (std::move (stored.prepared));
            else
                // R5: the last project failing to reopen is a fact, not a shrug.
                appModel.reportStatus (
                    "Open failed: " + stored.failureReason
                        + " (" + lastProject.filename().string() + ")",
                    true);
        }
        else
        {
            // A fresh native launch is a real, recoverable session. Keep its backing bundle:
            // edits already persist here, including across an interrupted first Save.
            const auto directory = fileChoices.sessionStateDirectory / "Untitled"
                / juce::Uuid().toString().toStdString();
            std::error_code error;
            std::filesystem::create_directories (directory, error);
            std::ofstream marker (directory / unnamedMarkerName, std::ios::binary);
            marker << unnamedBundleName << '\n';
            marker.close();
            if (error || ! marker)
                appModel.reportStatus ("New project failed: cannot create the session backing directory", true);
            else
            {
                const auto created = appModel.createProjectBundle (
                    directory / unnamedBundleName, UiAppModel::makeDefaultSessionProject(), recordStartupStage);
                if (! created.ok())
                    appModel.reportStatus ("New project failed: " + created.message, true);
            }
        }
    }
    recordStartupStage ("open-or-create-session");

    if (desktopAudioRequested)
    {
        // Request stereo input so the shipped Record button can capture real audio (P0-1); fall
        // back to output-only when no input device exists so playback never regresses.
        const juce::String error = initialiseDesktopAudio (audioDeviceManager, recordStartupStage);
        recordStartupStage ("initialise-audio-device");
        if (error.isEmpty())
        {
            if (juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
                appModel.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
            audioDeviceManager.addAudioCallback (this);
            ++audioCallbackAdds;   // G0.1 probe: the one legitimate startup registration
            desktopAudioCallbackRegistered = true;
            appModel.setDeviceCallbackLive (true);
            desktopAudioOpen.store (true, std::memory_order_release);
        }
        else
        {
            // R4: a soundless app must say why instead of sitting silent.
            appModel.reportStatus (
                "No audio device could be opened: " + error.toStdString(), true);
        }
        // Also expose available choices when opening failed, so device selection
        // remains a recovery path for a soundless launch.
        refreshAudioDeviceChooser();
        recordStartupStage ("adopt-and-enumerate-device");
    }

    // E34: open every MIDI input so played notes reach a live capture session (native
    // shell only — harness runs stay deterministic with the injected model seam).
    if (desktopAudioRequested)
    {
        for (const auto& midiDevice : juce::MidiInput::getAvailableDevices())
        {
            if (auto midiInput = juce::MidiInput::openDevice (midiDevice.identifier, this))
            {
                midiInput->start();
                midiInputs.push_back (std::move (midiInput));
            }
        }
    }
    recordStartupStage ("open-midi-inputs");

    // H17 CP4: scheduled autosave is ON by default (policy lives in the headless app model, so the
    // default is covered by a headless test). The Timer fires on the message thread — which is this
    // app's control thread — so writeAutosaveTick()'s heavy SQLite/asset I/O is on the right thread.
    startTimer (kUiRefreshIntervalMs);

    // G0.2: keys go to the command router, not to widgets (ADR-0046 §4).
    applyKeyboardFocusLaw();
    refreshActionState();
    resized();
    hideMixerControlsBehindDockTab();
    recordStartupStage ("finish-constructor");
    if (! stateProbePath.empty())
    {
        auto timingPath = stateProbePath;
        timingPath += ".startup.tsv";
        std::ofstream timingOutput (timingPath, std::ios::binary | std::ios::trunc);
        timingOutput << startupTimings->str();
    }
}

MainComponent::~MainComponent()
{
    if (routedTopLevel != nullptr)
        routedTopLevel->removeKeyListener (this);
    menuBar.setModel (nullptr);
    stopTimer();
    for (auto& midiInput : midiInputs)
        if (midiInput != nullptr)
            midiInput->stop();
    midiInputs.clear();
    if (desktopAudioCallbackRegistered)
        audioDeviceManager.removeAudioCallback (this);
    audioDeviceManager.closeAudioDevice();
    setLookAndFeel (nullptr);
}

// The UI polls the lock-free audio-thread transport snapshot at ~30 Hz. Autosave remains on its
// independent slow schedule and never runs in the device callback.
void MainComponent::timerCallback()
{
    const auto tickStart = std::chrono::steady_clock::now();
    serviceUiTick();
    lastTickMs = std::chrono::duration<double, std::milli> (
                     std::chrono::steady_clock::now() - tickStart).count();
    ++probeTick;
    writeStateProbeIfEnabled();
}

void MainComponent::serviceUiTick()
{
    // G0.3: the janitor runs on the control thread every tick — retired engines / monitor
    // chains are freed once the device thread is provably past them.
    appModel.setDeviceCallbackLive (desktopAudioCallbackRegistered
                                    && desktopAudioOpen.load (std::memory_order_acquire));
    appModel.reclaimRetiredAudioObjects();
    appModel.refreshTransportSnapshot();
    // G3.10: the thru target follows the selection every tick; the input lamp holds for a moment
    // after the last played note (the counter is the device thread's, read relaxed).
    appModel.updateMidiThruTarget();
    {
        const std::uint32_t seen = appModel.midiInputQueue().seen();
        if (seen != midiInSeenLast)
        {
            midiInSeenLast = seen;
            midiInLitUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds (yesdaw::ui::UiTheme::Layout::headerMidiInLampHoldMs);
            repaint (headerLayout().midiIn);
        }
        else if (midiInLitUntil != std::chrono::steady_clock::time_point {} && std::chrono::steady_clock::now() >= midiInLitUntil)
        {
            midiInLitUntil = {};
            repaint (headerLayout().midiIn);
        }
    }
    appModel.serviceRecordingCountIn();
    if (appModel.realRecordingCaptureActive())
        appModel.drainRealRecordingCapture();
    // R4: promote a device-thread error flag to a status message, then decay and paint the
    // shared status line from real model state.
    if (deviceErrorPending.exchange (false, std::memory_order_acq_rel))
        appModel.reportStatus ("Audio device error - output stopped", true);
    appModel.serviceStatusLineDecay();
    refreshStatusLine();
    updateTrackMeterHoldStates();
    pushWindowTitle();

    // G0.4: the 391-line action-state refresh runs only when the context CHANGED (the
    // playhead position is not a change — it moves every tick while playing), never as a
    // 30 Hz habit. Meter-dependent chrome is painted, not refreshed, so it needs no tick here.
    {
        yesdaw::ui::UiActionContext context = appModel.contextSnapshot();
        context.playheadFrame = 0;
        if (! lastRefreshedContextValid || ! (context == lastRefreshedContext))
        {
            lastRefreshedContext = context;
            lastRefreshedContextValid = true;
            refreshActionState();
            hideMixerControlsBehindDockTab();   // G3.2 checkpoint FIX 1: the tick's refresh restores the lane; hide it again
        }
    }

    // G0.4: a follow-scroll moves the whole canvas — that IS a view change; otherwise only the
    // dynamic layers (playhead, meters, transport counter) repaint this tick.
    const double scrollBefore = timelineScrollSeconds;
    followPlaybackPlayhead();
    const bool rollScrolled = followPianoRollPlayhead();   // G3.2
    if (timelineScrollSeconds != scrollBefore || rollScrolled)
        repaintAll();
    else
        repaintDynamicLayers();

    if (! appModel.autosaveSchedule().enabled)
        return;

    autosaveElapsedMs += kUiRefreshIntervalMs;
    if (autosaveElapsedMs >= appModel.autosaveSchedule().intervalMs)
    {
        autosaveElapsedMs = 0;
        const yesdaw::persistence::AutosaveResult ticked = appModel.writeAutosaveTick();
        if (! ticked.ok())
            appModel.reportStatus ("Autosave failed", true);
    }
}

// Window title with the dirty marker (B38): "<bundle stem>[*] - YES DAW" once a project is
// open; empty otherwise so the app keeps its versioned startup title. State-derived, so the
// harness snapshot reads it directly and the UI tick pushes it to the native window.
juce::String MainComponent::computedWindowTitle() const
{
    if (! appModel.context().projectLoaded || appModel.bundlePath().empty())
        return {};

    const juce::String stem (appModel.bundlePath().stem().string());
    return stem + (appModel.hasUnsavedChanges() ? "*" : "") + " - YES DAW";
}

void MainComponent::pushWindowTitle()
{
    const juce::String title = computedWindowTitle();
    if (title.isEmpty() || title == lastPushedWindowTitle)
        return;

    lastPushedWindowTitle = title;
    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        window->setName (title);
}

// Close-confirm flow (B37): a clean session closes silently; edits since the last explicit
// Save ask through the injectable seam (native three-way box otherwise). Closing never rolls
// back the always-persisted bundle; Save records this state as the saved version.
bool MainComponent::confirmClose()
{
    if (! appModel.hasUnsavedChanges())
        return true;

    int choice = yesdaw::ui::kCloseChoiceCancel;
    if (fileChoices.confirmCloseUnsavedChanges)
    {
        choice = fileChoices.confirmCloseUnsavedChanges();
    }
    else
    {
        const int native = juce::AlertWindow::showYesNoCancelBox (
            juce::MessageBoxIconType::QuestionIcon,
            "Unsaved changes",
            "Save this state as your saved version before closing?\n"
            "(Every edit is already stored in the project bundle.)",
            "Save",
            "Close without saving",
            "Cancel");
        choice = native == 1 ? yesdaw::ui::kCloseChoiceSave
               : native == 2 ? yesdaw::ui::kCloseChoiceClose
                             : yesdaw::ui::kCloseChoiceCancel;
    }

    if (choice == yesdaw::ui::kCloseChoiceSave)
    {
        return saveCurrentProject (false);   // canceled naming or a failed save keeps the app open
    }

    return choice == yesdaw::ui::kCloseChoiceClose;
}

// G0.1 probe: paint() opens the frame stamp and paintOverChildren() closes it — JUCE paints
// this component, then every child, then paintOverChildren on the same component, so the
// pair brackets the whole shell's paint work for one frame (the B2 budget).
void MainComponent::paintOverChildren (juce::Graphics&)
{
    const auto now = std::chrono::steady_clock::now();
    lastPaintMs = std::chrono::duration<double, std::milli> (now - paintStartStamp).count();
    paintRing[paintRingIndex] = lastPaintMs;
    paintRingIndex = (paintRingIndex + 1u) % paintRing.size();
    paintRingCount = std::min (paintRingCount + 1u, paintRing.size());
    ++paintCount;
    if (actionStampPending)
    {
        actionStampPending = false;
        lastActionToPaintMs =
            std::chrono::duration<double, std::milli> (now - pendingActionStamp).count();
    }
}

void MainComponent::paint (juce::Graphics& g)
{
    paintStartStamp = std::chrono::steady_clock::now();
    g.fillAll (kBackground);
    drawHeader (g);

    const auto bounds = getLocalBounds();
    const auto top = bounds.withHeight (headerHeightNow());
    g.setColour (yesdaw::ui::UiTheme::Color::separator());
    g.fillRect (top.withBottom (headerHeightNow())
                    .removeFromBottom (yesdaw::ui::UiTheme::Layout::shellHeaderSeparatorHeight));

    auto work = bounds.withTrimmedTop (headerHeightNow());

    work.removeFromBottom (dockedMixerHeight());
    auto left = work.removeFromLeft (viewState.railWidth)
                    .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                              yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
    auto inspector = work.removeFromRight (inspectorWidthNow())
                         .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                                   yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
    drawTrackList (g, left);
    drawInspector (g, inspector);
    // V3: a collapsed dock paints NOTHING (the "drop whole" law this codebase already uses
    // elsewhere for sections that don't fit) rather than relying on a zero/negative-height
    // rect to degrade gracefully.
    if (appModel.context().mixerDockVisible)
    {
        // G2.1 cp2: the dock shows ONE editor tab. G3.1: the instrument panel paints itself.
        if (dockShowsPianoRoll())
            drawPianoRoll (g, mixerPanelBounds());
        else if (! dockShowsInstrument())
            drawMixer (g, mixerPanelBounds());
    }
    lastParentPaintMs = std::chrono::duration<double, std::milli> (
        std::chrono::steady_clock::now() - paintStartStamp).count();
}

void MainComponent::resized()
{
    restoreControlsHiddenByDockTab();   // G2.1 cp2
    const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
    const HeaderLayout h = headerLayout();

    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        const auto action = toolbarActions[i];
        if (isSettingsRowAction (action))
            buttons[i].setVisible (h.settingsVisible
                                   && action != yesdaw::ui::UiActionId::RecordingAssembleComp);   // G1.7: hidden until G7
        switch (action)
        {
            case yesdaw::ui::UiActionId::ProjectNew:         buttons[i].setBounds (h.newButton); break;
            case yesdaw::ui::UiActionId::ProjectOpen:        buttons[i].setBounds (h.openButton); break;
            case yesdaw::ui::UiActionId::ProjectSave:        buttons[i].setBounds (h.saveButton); break;
            case yesdaw::ui::UiActionId::ProjectImportAudio: buttons[i].setBounds (h.importButton); break;
            case yesdaw::ui::UiActionId::RecordingArmTrack:  buttons[i].setBounds (h.arm); break;
            case yesdaw::ui::UiActionId::RecordingSetMonitoringPolicy: buttons[i].setBounds (h.monitor); break;
            case yesdaw::ui::UiActionId::TransportRecord:    buttons[i].setBounds (h.record); break;
            case yesdaw::ui::UiActionId::RecordingAssembleComp: buttons[i].setBounds (h.comp); break;
            case yesdaw::ui::UiActionId::EditUndo:           buttons[i].setBounds (h.undoButton); break;
            case yesdaw::ui::UiActionId::EditRedo:           buttons[i].setBounds (h.redoButton); break;
            case yesdaw::ui::UiActionId::TransportLocateStart: buttons[i].setBounds (h.locateStart); break;
            case yesdaw::ui::UiActionId::TransportPlay:      buttons[i].setBounds (h.play); break;
            case yesdaw::ui::UiActionId::TransportStop:      buttons[i].setBounds (h.stop); break;
            case yesdaw::ui::UiActionId::TransportToggleLoop: buttons[i].setBounds (h.loop); break;
            // G2.1 cp2: ViewMixer / ViewPianoRoll sit in the status row's view cluster (below).
            default: buttons[i].setBounds ({});
        }
    }

    autosaveRestoreButton.setBounds (yesdaw::ui::UiTheme::Layout::autosaveRestoreButtonBounds());
    autosaveDiscardButton.setBounds (yesdaw::ui::UiTheme::Layout::autosaveDiscardButtonBounds());
    audioDeviceChooser.setBounds (h.outputDevice);
    audioInputDeviceChooser.setBounds (h.inputDevice);
    recordingInputChannelChooser.setBounds (h.inputChannel);
    exportAudioButton.setBounds (h.exportButton);
    exportAudioProgress.setBounds (h.exportProgress);
    exportAudioCancelButton.setBounds (h.exportCancel);
    exportBitDepthChooser.setBounds (h.bitDepth);
    exportRangeChooser.setBounds (h.range);
    for (juce::Component* settingsControl : { static_cast<juce::Component*> (&audioDeviceChooser),
                                              static_cast<juce::Component*> (&audioInputDeviceChooser),
                                              static_cast<juce::Component*> (&recordingInputChannelChooser),
                                              static_cast<juce::Component*> (&exportBitDepthChooser),
                                              static_cast<juce::Component*> (&exportRangeChooser) })
        settingsControl->setVisible (h.settingsVisible);
    menuBar.setBounds (h.menuBar);
    // M9: the LUFS readout rides the master card — it drops with it instead of being clipped.
    masterLoudnessReadout.setBounds (headerMasterLufsBounds());
    masterLoudnessReadout.setVisible (! headerMasterLufsBounds().isEmpty());
    {
        // G2.16: the scroll bars take a strip below and beside the timeline; the input keeps the rest.
        juce::Rectangle<int> area = timelinePanelBounds();
        const juce::Rectangle<int> hBar = area.removeFromBottom (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness);
        const juce::Rectangle<int> vBar = area.removeFromRight (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness);
        timelineHScroll.setBounds (hBar.withTrimmedRight (yesdaw::ui::UiTheme::Layout::timelineScrollBarThickness));
        timelineVScroll.setBounds (vBar);
        timelineInput.setBounds (area);
        jassert (area == timelineBounds());
    }
    playheadLayer.setBounds (timelineBounds());
    {
        // G2.1: the splitters sit on the panel edges, above every panel (last in z-order).
        const int thickness = yesdaw::ui::UiTheme::Layout::splitterThickness;
        const int half = thickness / 2;
        const int top = headerHeightNow();
        const int dockTop = getHeight() - dockedMixerHeight();
        dockSplitter.setBounds (getLocalBounds().withY (dockTop - half).withHeight (thickness));
        dockSplitter.setVisible (dockedMixerHeight() > 0);
        railSplitter.setBounds (viewState.railWidth - half, top, thickness, dockTop - top);
        railSplitter.setVisible (true);
        inspectorSplitter.setBounds (getWidth() - inspectorWidthNow() - half, top, thickness, dockTop - top);
        inspectorSplitter.setVisible (inspectorWidthNow() > 0);
        for (yesdaw::ui::SplitterComponent* splitter : { &railSplitter, &inspectorSplitter, &dockSplitter })
        {
            if (fxEditor.isVisible()) splitter->toBehind (&fxEditor);
            else splitter->toFront (false);
        }
    }
    // G1.5: the keymap editor floats over the arrangement, centred, at most 760×520.
    {
        const juce::Rectangle<int> work = getLocalBounds().withTrimmedTop (headerHeightNow()).withTrimmedBottom (dockedMixerHeight());
        using L = yesdaw::ui::UiTheme::Layout;
        const int width = std::min (L::keymapEditorMaxWidth, work.getWidth() - L::keymapEditorMargin);
        const int height = std::min (L::keymapEditorMaxHeight, work.getHeight() - L::keymapEditorMargin);
        keymapEditor.setBounds (work.withSizeKeepingCentre (std::max (L::keymapEditorMinWidth, width), std::max (L::keymapEditorMinHeight, height)));
        // G2.18: the undo history window — the same centred law, narrower.
        const int historyWidth = std::min (L::undoHistoryMaxWidth, work.getWidth() - L::keymapEditorMargin);
        const int historyHeight = std::min (L::undoHistoryMaxHeight, work.getHeight() - L::keymapEditorMargin);
        undoHistory.setBounds (work.withSizeKeepingCentre (std::max (L::keymapEditorMinWidth, historyWidth), std::max (L::keymapEditorMinHeight, historyHeight)));
        // G4.1 cp2: the FX editor — the same centred law, sized for its parameter rows.
        // The EQ face needs its full parameter page even with the mixer grown at 720p.
        // Like a plug-in window, it may float over the dock; keep it above the splitters too.
        const auto fxWork = fxEditor.showsEqResponse() ? getLocalBounds().withTrimmedTop (headerHeightNow()) : work;
        const int fxWidth = std::min (fxEditor.preferredWidth(), fxWork.getWidth() - L::keymapEditorMargin);
        const int fxHeight = std::min (fxEditor.preferredHeight(), fxWork.getHeight() - L::keymapEditorMargin);
        fxEditor.setBounds (fxWork.withSizeKeepingCentre (std::max (L::fxEditorMinWidth, fxWidth), std::max (L::fxEditorMinHeight, fxHeight)));
    }
    pianoRollInput.setBounds (mixerPanelBounds());   // G2.1 cp2: the piano roll is a dock tab
    pianoRollLaneChooser.setBounds (pianoRollControlLaneChooserArea (pianoRollCanvasGeometry (pianoRollInput.getBounds().withZeroOrigin()))
                                        .translated (pianoRollInput.getX(), pianoRollInput.getY()));   // G3.3
    {
        // G3.6: the header toggles sit after the "PIANO ROLL" label on the header row.
        using L = yesdaw::ui::UiTheme::Layout;
        auto header = pianoRollInput.getBounds().withHeight (L::pianoRollHeaderHeight)
                          .withTrimmedLeft (L::pianoRollHeaderButtonLeft)
                          .reduced (L::pianoRollHeaderButtonInsetX, L::pianoRollHeaderButtonInsetY);
        pianoRollTypingButton.setBounds (header.removeFromLeft (L::pianoRollHeaderButtonWidth));
        header.removeFromLeft (L::pianoRollHeaderButtonGap);
        pianoRollStepButton.setBounds (header.removeFromLeft (L::pianoRollHeaderButtonWidth));
        // G3.8: the Key / Scale choosers follow the toggles.
        header.removeFromLeft (L::pianoRollHeaderButtonGap);
        pianoRollKeyChooser.setBounds (header.removeFromLeft (L::pianoRollHeaderChooserWidth));
        header.removeFromLeft (L::pianoRollHeaderButtonGap);
        pianoRollScaleChooser.setBounds (header.removeFromLeft (L::pianoRollHeaderChooserWidth));
    }
    instrumentPanel.setBounds (mixerPanelBounds());   // G3.1: so is the instrument panel
    trackListInput.setBounds (leftRailPanelBounds());
    mixerStripsInput.setBounds (mixerPanelBounds());   // G4.1 cp2: the strips are the whole dock
    {
        auto box = h.tempoMeterBox;
        auto tempoCell = box.removeFromLeft (yesdaw::ui::UiTheme::Layout::headerTransportCellWidth);
        headerTempoControl.setBounds (
            tempoCell.reduced (yesdaw::ui::UiTheme::Layout::headerTransportCellInsetX,
                               yesdaw::ui::UiTheme::Layout::headerTransportValueInsetY)
                .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportValueHeight));
        auto meterCell = box.removeFromLeft (yesdaw::ui::UiTheme::Layout::headerTransportCellWidth);
        headerMeterChooser.setBounds (
            meterCell.reduced (yesdaw::ui::UiTheme::Layout::headerTransportCellInsetX,
                               yesdaw::ui::UiTheme::Layout::headerTransportValueInsetY)
                .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportValueHeight));
    }
    {
        const auto rail = leftRailPanelBounds();
        trackAddButton.setBounds (
            rail.getRight() - yesdaw::ui::UiTheme::Layout::trackListAddButtonWidth
                - yesdaw::ui::UiTheme::Layout::trackListAddButtonInset,
            rail.getY() + yesdaw::ui::UiTheme::Layout::trackListAddButtonInset,
            yesdaw::ui::UiTheme::Layout::trackListAddButtonWidth,
            yesdaw::ui::UiTheme::Layout::trackListAddButtonHeight);
    }
    {
        const auto automationBounds =
            yesdaw::ui::UiTheme::Layout::automationLaneToggleBounds (timelineBounds());
        const juce::Rectangle<int> snapBounds {
            automationBounds.getX() - yesdaw::ui::UiTheme::Layout::timelineSnapChooserWidth
                - yesdaw::ui::UiTheme::Layout::timelineSnapChooserGap,
            automationBounds.getY(),
            yesdaw::ui::UiTheme::Layout::timelineSnapChooserWidth,
            automationBounds.getHeight()
        };
        timelineSnapChooser.setBounds (snapBounds);
    }
    layoutAutomationLaneControls();
    layoutInspectorControls();
    layoutMixerControls();
    hideMixerControlsBehindDockTab();   // G2.1 cp2
}

// G2.1: the splitters set these; each clamps to the plan's §3.4 ranges, lays out and repaints.
void MainComponent::setRailWidth (int width)
{
    viewState.railWidth = juce::jlimit (yesdaw::ui::UiTheme::Layout::leftRailMinWidth,
                                        yesdaw::ui::UiTheme::Layout::leftRailMaxWidth, width);
    resized();
    repaintAll();
}

void MainComponent::setDockHeight (int height)
{
    viewState.dockHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::editorDockMinHeight, height);
    resized();
    repaintAll();
}

juce::Component* MainComponent::toolbarButtonFor (yesdaw::ui::UiActionId action)
{
    const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
    for (std::size_t i = 0; i < buttons.size() && i < toolbarActions.size(); ++i)
        if (toolbarActions[i] == action)
            return &buttons[i];
    return nullptr;
}

void MainComponent::setToolbarButtonBounds (yesdaw::ui::UiActionId action, juce::Rectangle<int> bounds)
{
    const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
    for (std::size_t i = 0; i < buttons.size() && i < toolbarActions.size(); ++i)
        if (toolbarActions[i] == action)
            buttons[i].setBounds (bounds);
}

void MainComponent::restoreControlsHiddenByDockTab()
{
    for (auto& [control, wasVisible] : hiddenByDockTab)
        control->setVisible (wasVisible);
    hiddenByDockTab.clear();
}

// The exact rect drawTrackList paints into; the rail input overlay shares it so hits match paint.
juce::Rectangle<int> MainComponent::leftRailPanelBounds() const
{
    auto work = getLocalBounds().withTrimmedTop (headerHeightNow());
    work.removeFromBottom (dockedMixerHeight());
    return work.removeFromLeft (viewState.railWidth)
               .reduced (yesdaw::ui::UiTheme::Layout::shellPanelHorizontalInset,
                         yesdaw::ui::UiTheme::Layout::shellPanelVerticalInset);
}

// Live gain readout in dB (B31): 20*log10(linear gain), "-inf dB" at silence.
juce::String MainComponent::dbReadoutText (double linearGain)
{
    if (linearGain <= 0.0)
        return "-inf dB";

    return juce::String (20.0 * std::log10 (linearGain), 1) + " dB";
}

void MainComponent::showDragDbReadout (juce::Rectangle<int> anchorBounds, double linearGain)
{
    dragDbReadout.setText (dbReadoutText (linearGain), juce::dontSendNotification);
    dragDbReadout.setBounds (
        juce::Rectangle<int> (yesdaw::ui::UiTheme::Layout::dbReadoutWidth,
                              yesdaw::ui::UiTheme::Layout::dbReadoutHeight)
            .withCentre ({ anchorBounds.getCentreX(),
                           anchorBounds.getY()
                               - yesdaw::ui::UiTheme::Layout::dbReadoutHeight / 2 })
            .constrainedWithin (getLocalBounds()));
    dragDbReadout.setVisible (true);
    dragDbReadout.toFront (false);
}

void MainComponent::hideDragDbReadout()
{
    dragDbReadout.setVisible (false);
}

// G0.4 layered invalidation. repaintAll() is what every model/view change calls (the old
// whole-window repaint()), and it also invalidates the buffered timeline canvas so the static
// layer can never go stale. repaintDynamicLayers() is what the tick calls: the playhead layer,
// the rail (meters), the dock (meters, master), and the header band (transport counter) — the
// expensive clip/waveform canvas is left to its cache.
void MainComponent::repaintAll()
{
    ++fullInvalidations;
    hideMixerControlsBehindDockTab();   // G2.1 cp2: after every action's refresh
    timelineInput.repaint();
    repaint();
}

void MainComponent::repaintDynamicLayers()
{
    ++dynamicInvalidations;
    playheadLayer.repaint();
    repaint (getLocalBounds().withHeight (headerHeightNow()));
    repaint (leftRailPanelBounds());
    if (appModel.context().mixerDockVisible)
        repaint (mixerPanelBounds());
}

void MainComponent::childrenChanged()
{
    applyKeyboardFocusLaw();
}

void MainComponent::parentHierarchyChanged()
{
    juce::Component* top = getTopLevelComponent();
    if (top == this)
        top = nullptr;
    if (top != routedTopLevel)
    {
        if (routedTopLevel != nullptr)
            routedTopLevel->removeKeyListener (this);
        routedTopLevel = top;
        if (routedTopLevel != nullptr)
            routedTopLevel->addKeyListener (this);
    }

    // Take focus once the window is showing so the very first chord after launch lands here.
    juce::Component::SafePointer<MainComponent> safeThis (this);
    juce::MessageManager::callAsync ([safeThis] {
        if (safeThis != nullptr && safeThis->isShowing() && ! safeThis->hasKeyboardFocus (true))
            safeThis->grabKeyboardFocus();
    });
}

bool MainComponent::cancelInProgressEdit()
{
    bool cancelled = timelineInput.cancelInProgressEdit();
    cancelled = pianoRollInput.cancelInProgressEdit() || cancelled;
    if (trackRenameEditor.isVisible())
    {
        dismissTrackRenameEditor();
        cancelled = true;
    }
    if (clipRenameEditor.isVisible())
    {
        dismissClipRenameEditor();
        cancelled = true;
    }
    if (markerRenameEditor.isVisible())
    {
        dismissMarkerRenameEditor();
        cancelled = true;
    }

    if (cancelled)
    {
        refreshActionState();
        repaintAll();
    }
    return cancelled;
}

void MainComponent::drawHeader (juce::Graphics& g) const
{
    const auto headerBounds = getLocalBounds().withHeight (headerHeightNow());
    juce::ColourGradient headerGradient (
        yesdaw::ui::UiTheme::Color::panelRaised(),
        static_cast<float> (headerBounds.getCentreX()),
        static_cast<float> (headerBounds.getY()),
        yesdaw::ui::UiTheme::Color::canvasLayer(),
        static_cast<float> (headerBounds.getCentreX()),
        static_cast<float> (headerBounds.getBottom()),
        false);
    g.setGradientFill (headerGradient);
    g.fillRect (headerBounds);
    g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
        yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
    g.fillRect (headerBounds.withHeight (
        yesdaw::ui::UiTheme::Layout::controlInnerHighlightHeight));

    const HeaderLayout h = headerLayout();
    if (h.settingsVisible)
    {
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRect (h.settingsRow);
    }
    const std::array headerSections { h.toolsSection, h.transportSection, h.masterSection };
    for (const auto section : headerSections)
    {
        if (section.isEmpty())
            continue;
        g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
        g.fillRoundedRectangle (section.toFloat(), yesdaw::ui::UiTheme::Radius::panel);
        g.setColour (yesdaw::ui::UiTheme::Color::panelInnerHighlight().withAlpha (
            yesdaw::ui::UiTheme::Tone::innerHighlightAlpha));
        g.drawRoundedRectangle (
            section.toFloat().reduced (
                yesdaw::ui::UiTheme::Layout::panelOutlineInset),
            yesdaw::ui::UiTheme::Radius::panel,
            yesdaw::ui::UiTheme::Layout::panelOutlineStrokeWidth);
    }

    drawTransportReadouts (g);
    drawMasterMeter (g);
    g.setColour (kPanelStroke);
    g.fillRect (getLocalBounds()
                    .withHeight (headerHeightNow())
                    .removeFromBottom (yesdaw::ui::UiTheme::Space::hairline));
}

// V2: bar|beat at the current playhead — a single-tempo/meter law (the project's head
// values, matching the existing headBarFrames() family's own scope). Shared by the paint
// path below and the harness accessor, so a test can never duplicate this formula.
yesdaw::engine::BarBeat MainComponent::headerBarBeat() const
{
    const double sampleRate = appModel.project().sampleRate.isValid()
                                  ? appModel.project().sampleRate.hz
                                  : 48000.0;
    // G2.15: the FULL maps — frame -> tick through the compiled tempo map's inverse, then the
    // meter walk; the single-tempo law only when the map cannot be built.
    yesdaw::engine::CompiledTempoMap compiled;
    yesdaw::engine::BarBeat piecewise;
    if (appModel.project().sampleRate.isValid() && appModel.compiledTempoMap (compiled)
        && yesdaw::engine::computeBarBeatPiecewise (
               compiled,
               yesdaw::engine::MeterMapView { appModel.project().meterMap.data(), appModel.project().meterMap.size() },
               appModel.context().playheadFrame, piecewise))
        return piecewise;
    const HeadTempoMeter head = headTempoMeter();
    return yesdaw::engine::computeBarBeat (
        head.bpm, head.numerator, head.denominator, sampleRate, appModel.context().playheadFrame);
}

MainComponent::CounterStrings MainComponent::counterStrings() const
{
    const yesdaw::engine::BarBeat barBeat = headerBarBeat();
    const juce::String bars = juce::String::formatted (
        "%03lld|%02lld", static_cast<long long> (barBeat.bar), static_cast<long long> (barBeat.beat));
    const double sampleRate = appModel.project().sampleRate.isValid() ? appModel.project().sampleRate.hz : 48000.0;
    const double seconds = std::max (0.0, static_cast<double> (appModel.context().playheadFrame) / sampleRate);
    const int minutes = static_cast<int> (seconds / 60.0);
    const double rest = seconds - 60.0 * minutes;
    const juce::String minSec = juce::String::formatted ("%d:%06.3f", minutes, rest);
    // G2.2: SMPTE and samples share the ruler's formatters (one law, two readouts).
    if (timeDisplayMode == yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySmpte)
        return CounterStrings { yesdaw::ui::timeline_canvas_detail::formatSmpte (seconds), bars, "smpte" };
    if (timeDisplayMode == yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySamples)
        return CounterStrings { yesdaw::ui::timeline_canvas_detail::formatRulerTime (seconds, timeDisplayMode, 0.0, sampleRate), bars, "samples" };
    return timeDisplayMode == 0 ? CounterStrings { bars, minSec, "bars" }
                                : CounterStrings { minSec, bars, "minsec" };
}

void MainComponent::mouseDown (const juce::MouseEvent& event)
{
    const HeaderLayout header = headerLayout();
    if (header.timeReadout.contains (event.getPosition()))
    {
        timeDisplayMode = (timeDisplayMode + 1) % (yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySamples + 1);   // G2.2: bars → min:sec → SMPTE → samples
        repaintAll();   // the ruler's time row follows
        return;
    }
    // The header's gear: painted since G0.7, dead to the mouse until 2026-09-04. It is the
    // settings row's toggle — the same action the View menu carries — and lights while the
    // row shows.
    if (header.gear.contains (event.getPosition()))
    {
        handleAction (yesdaw::ui::UiActionId::ViewToggleSettingsRow);
        refreshActionState();
    }
}

void MainComponent::drawTransportReadouts (juce::Graphics& g) const
{
    const HeaderLayout h = headerLayout();
    auto time = h.timeReadout;
    fillPanel (g, time, yesdaw::ui::UiTheme::Radius::panel);
    g.setColour (kText);
    g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
        yesdaw::ui::UiTheme::Type::transportClock));
    // V2: bar|beat, not a stopwatch clock — the SAME single-tempo/meter law V4's ruler
    // reuses, so the header readout and the ruler's bar numbers can never disagree.
    const CounterStrings counter = counterStrings();
    g.drawText (counter.primary,
                time.reduced (yesdaw::ui::UiTheme::Layout::headerTransportTextInsetX,
                              yesdaw::ui::UiTheme::Layout::headerTransportClockInsetY)
                    .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportClockHeight),
                juce::Justification::centred,
                false);
    // The caption row under the clock: trimmed from the TOP by the label inset (reducing on
    // both sides left nothing at the 44 px readout — the caption had been clipped since G0.7).
    drawSmallLabel (g,
                    counter.secondary,
                    time.withTrimmedTop (yesdaw::ui::UiTheme::Layout::headerTransportLabelInsetY)
                        .reduced (yesdaw::ui::UiTheme::Layout::headerTransportTextInsetX, yesdaw::ui::UiTheme::Space::hairline),
                    juce::Justification::centred);

    const juce::String tempo = appModel.context().projectLoaded && ! appModel.project().tempoMap.empty()
                                 ? juce::String (appModel.project().tempoMap.front().bpm, 2)
                                 : juce::String ("--");
    const juce::String meter = appModel.context().projectLoaded && ! appModel.project().meterMap.empty()
                                 ? juce::String (appModel.project().meterMap.front().numerator)
                                     + "/" + juce::String (appModel.project().meterMap.front().denominator)
                                 : juce::String ("--");
    // V2: the KEY cell is gone — D3 (no fake data): no key-signature model exists anywhere in
    // engine::Project, so a permanent "--" was a dead literal, not an honest empty state.
    const std::array<std::pair<juce::String, const char*>, 2> readouts {{
        { tempo, "TEMPO" },
        { meter, "TIME SIG" }
    }};

    // G3.10: the MIDI input lamp in the time readout's corner — lit while a played note is fresh.
    if (! h.midiIn.isEmpty())
    {
        const bool lit = midiInLitUntil != std::chrono::steady_clock::time_point {}
                      && std::chrono::steady_clock::now() < midiInLitUntil;
        g.setColour (lit ? yesdaw::ui::UiTheme::Color::midiInLampLit() : yesdaw::ui::UiTheme::Color::midiInLampOff());
        g.fillRoundedRectangle (h.midiIn.toFloat(), yesdaw::ui::UiTheme::Layout::headerMidiInLampCornerRadius);
        g.setColour (lit ? yesdaw::ui::UiTheme::Color::pianoWhiteKeyText() : yesdaw::ui::UiTheme::Color::mutedText());
        g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::tiny, juce::Font::bold));
        g.drawText ("MIDI", h.midiIn, juce::Justification::centred, false);
    }
    auto box = h.tempoMeterBox;
    for (const auto& readout : readouts)
    {
        auto cell = box.removeFromLeft (yesdaw::ui::UiTheme::Layout::headerTransportCellWidth);
        fillPanel (g, cell, yesdaw::ui::UiTheme::Radius::none);
        g.setColour (kText);
        g.setFont (yesdaw::ui::UiTheme::Type::numericFont (
            yesdaw::ui::UiTheme::Type::readout));
        g.drawText (readout.first,
                    cell.reduced (yesdaw::ui::UiTheme::Layout::headerTransportCellInsetX,
                                  yesdaw::ui::UiTheme::Layout::headerTransportValueInsetY)
                        .removeFromTop (yesdaw::ui::UiTheme::Layout::headerTransportValueHeight),
                    juce::Justification::centred,
                    false);
        drawSmallLabel (g,
                        readout.second,
                        cell.reduced (yesdaw::ui::UiTheme::Layout::headerTransportCellInsetX,
                                      yesdaw::ui::UiTheme::Layout::headerTransportLabelInsetY),
                        juce::Justification::centred);
    }

}

MainComponent::HeaderLayout MainComponent::headerLayout() const
{
    using L = yesdaw::ui::UiTheme::Layout;
    HeaderLayout h;
    const int width = getWidth();
    h.menuBar = L::headerMenuBarBounds();
    const int controlY = L::menuBarHeight + (L::toolbarHeight - L::headerControlHeight) / 2;
    const int bigY = L::menuBarHeight + (L::toolbarHeight - L::headerTransportButtonSize) / 2;

    // Tools, left: New Open Save Import · Undo Redo · Export.
    int x = L::headerEdgeInset;
    const auto small = [&x] (int w)
    {
        const juce::Rectangle<int> r (x, controlY, w, L::headerControlHeight);
        x += w + L::headerButtonGap;
        return r;
    };
    h.newButton = small (L::headerSmallButtonWidth);
    h.openButton = small (L::headerSmallButtonWidth);
    h.saveButton = small (L::headerSmallButtonWidth);
    h.importButton = small (L::headerSmallButtonWidth);
    x += L::headerClusterGap - L::headerButtonGap;
    h.undoButton = small (L::headerUndoButtonWidth);
    h.redoButton = small (L::headerUndoButtonWidth);
    x += L::headerClusterGap - L::headerButtonGap;
    h.exportButton = small (L::headerExportButtonWidth);
    h.exportProgress = h.exportButton.withWidth (L::headerExportProgressWidth);
    h.exportCancel = juce::Rectangle<int> (h.exportButton.getRight() - L::headerExportCancelWidth,
                                           controlY, L::headerExportCancelWidth, L::headerControlHeight);
    const int toolsRight = x - L::headerButtonGap;
    h.toolsSection = juce::Rectangle<int> (L::headerEdgeInset, bigY, toolsRight - L::headerEdgeInset,
                                           L::headerTransportButtonSize)
                         .expanded (L::headerSectionPad);

    // Gear, right edge.
    h.gear = juce::Rectangle<int> (width - L::headerStatusIconRightInset, L::headerStatusIconY,
                                   L::headerStatusIconSize, L::headerStatusIconSize);

    // Transport, centred on the window; pushed right of the tools when the window is narrow
    // and never past the gear.
    const int centreWidth = 4 * L::headerTransportButtonSize + 3 * L::headerButtonGap
                          + L::headerClusterGap + L::headerTransportTimeWidth
                          + L::headerClusterGap + L::headerTransportBoxWidth
                          + L::headerClusterGap + L::headerLoopButtonWidth;
    const int minStart = toolsRight + L::headerGroupGap;
    int cx = juce::jmax (minStart, width / 2 - centreWidth / 2);
    cx = juce::jmax (minStart, juce::jmin (cx, h.gear.getX() - L::headerMasterGearGap - centreWidth));

    // Master card: right-anchored against the gear, shrinks toward the transport group,
    // drops WHOLE below its minimum (M9's law, now relative to the centred group).
    const int cardRight = h.gear.getX() - L::headerMasterGearGap;
    const int cardWidth = juce::jmin (L::headerMasterWidth, cardRight - (cx + centreWidth + L::headerGroupGap));
    if (cardWidth >= L::headerMasterMinWidth)
        h.masterCard = juce::Rectangle<int> (cardRight - cardWidth, L::headerMasterY, cardWidth, L::headerMasterHeight);

    x = cx;
    const auto big = [&x] (int w)
    {
        const juce::Rectangle<int> r (x, bigY, w, L::headerTransportButtonSize);
        x += w + L::headerButtonGap;
        return r;
    };
    h.locateStart = big (L::headerTransportButtonSize);
    h.play = big (L::headerTransportButtonSize);
    h.stop = big (L::headerTransportButtonSize);
    h.record = big (L::headerTransportButtonSize);
    x += L::headerClusterGap - L::headerButtonGap;
    h.timeReadout = big (L::headerTransportTimeWidth);
    x += L::headerClusterGap - L::headerButtonGap;
    h.tempoMeterBox = big (L::headerTransportBoxWidth);
    x += L::headerClusterGap - L::headerButtonGap;
    h.loop = big (L::headerLoopButtonWidth);
    // G3.10: the MIDI input lamp lives in the time readout's top-right corner (Logic's LCD carries
    // its MIDI activity the same way) — no width added to the centred cluster.
    h.midiIn = juce::Rectangle<int> (h.timeReadout.getRight() - L::headerMidiInLampInset - L::headerMidiInLampWidth,
                                     h.timeReadout.getY() + L::headerMidiInLampInset,
                                     L::headerMidiInLampWidth, L::headerMidiInLampHeight);
    h.transportSection = juce::Rectangle<int> (cx, bigY, centreWidth, L::headerTransportButtonSize)
                             .expanded (L::headerSectionPad);
    if (! h.masterCard.isEmpty())
        h.masterSection = juce::Rectangle<int> (h.masterCard.getX(), bigY,
                                                h.gear.getRight() - h.masterCard.getX(),
                                                L::headerTransportButtonSize)
                              .expanded (L::headerSectionPad);

    // The settings row (export choosers, device choosers, the recording cluster).
    h.settingsVisible = appModel.context().settingsRowVisible;
    if (h.settingsVisible)
    {
        h.settingsRow = juce::Rectangle<int> (0, kHeaderHeight, width, L::settingsRowHeight);
        const int rowY = kHeaderHeight + (L::settingsRowHeight - L::headerControlHeight) / 2;
        x = L::headerEdgeInset;
        const auto cell = [&x] (int w)
        {
            const juce::Rectangle<int> r (x, rowY, w, L::headerControlHeight);
            x += w + L::headerButtonGap;
            return r;
        };
        h.bitDepth = cell (L::settingsBitDepthWidth);
        h.range = cell (L::settingsRangeWidth);
        h.outputDevice = cell (L::settingsDeviceWidth);
        h.inputDevice = cell (L::settingsDeviceWidth);
        h.inputChannel = cell (L::settingsChannelWidth);
        h.arm = cell (L::settingsArmWidth);
        h.monitor = cell (L::settingsMonitorWidth);
        h.comp = cell (L::settingsCompWidth);
    }
    return h;
}

// G0.7: the header's height right now — the fixed menu + toolbar, plus the settings row when
// it is shown. Every work-area layout trims THIS, never the constant.
int MainComponent::headerHeightNow() const
{
    return kHeaderHeight + (appModel.context().settingsRowVisible ? yesdaw::ui::UiTheme::Layout::settingsRowHeight : 0);
}

} // namespace yesdaw::ui
