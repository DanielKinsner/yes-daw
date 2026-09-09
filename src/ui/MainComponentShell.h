// YES DAW — the app shell's declaration.
//
// Plan §5.1 (the shell topology), checkpoint 2 (2026-09-05): the class that was defined inline in a
// 12 000-line MainComponent.cpp now declares here (fields, nested types, every member's signature,
// the one-line accessors) and defines its members by domain: MainComponent.cpp (lifecycle, the
// harness tail), MainComponentArrange.cpp, MainComponentMixer.cpp, MainComponentPianoRoll.cpp,
// MainComponentInspector.cpp, MainComponentCommands.cpp, MainComponentProbe.cpp. Behaviour unchanged.

#pragma once

#include "ui/MainComponentInternal.h"

namespace yesdaw::ui {

class MainComponent : public juce::Component,
                      public juce::ScrollBar::Listener,   // G2.16: the real scroll bars
                      public juce::MenuBarModel,
                      public juce::KeyListener,    // G0.2: the command router on the top-level window
                      private juce::Timer,
                      private juce::AudioIODeviceCallback,
                      private juce::MidiInputCallback
{
public:
    explicit MainComponent (yesdaw::ui::MainComponentFileChoices choices, bool enableDesktopAudio);

    ~MainComponent() override;

    // The UI polls the lock-free audio-thread transport snapshot at ~30 Hz. Autosave remains on its
    // independent slow schedule and never runs in the device callback.
    void timerCallback() override;

    void serviceUiTick();

    void refreshStatusLine();

    // E34: real MIDI inputs — note on/off pairs collected on the message thread and stamped
    // with the capture session's published device-frame cursor; outside a session the model
    // refuses them, so this is inert until Record rolls.
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message) override;

    // G3.10: the one entry every played note takes (the device callback and the harness alike).
    [[nodiscard]] bool postMidiInputFromDevice (bool noteOn, int note, double velocity, int channel) noexcept;

    void handleCapturedMidiNote (std::int64_t frame, bool noteOn, int note, float velocity);

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;

    void audioDeviceIOCallbackWithContext (const float* const* inputChannels,
                                           int numInputChannels,
                                           float* const* outputChannels,
                                           int numOutputChannels,
                                           int numFrames,
                                           const juce::AudioIODeviceCallbackContext&) override;

    void audioDeviceStopped() override;

    void audioDeviceError (const juce::String&) override;

    [[nodiscard]] bool processDeviceAudioBlock (float* const* outputChannels,
                                                int numOutputChannels,
                                                int numFrames) noexcept;

    void accountDeviceBlockPeaks (float* const* outputChannels,
                                  int numOutputChannels,
                                  int numFrames) noexcept;

    [[nodiscard]] yesdaw::ui::UiActionContext harnessContext() const noexcept { return appModel.contextSnapshot(); }
    [[nodiscard]] const yesdaw::ui::UiRecordingDeviceSelection& harnessRecordingDevice() const noexcept;
    [[nodiscard]] float harnessInputMeterPeak() const noexcept;
    [[nodiscard]] const yesdaw::ui::UiRecordingTrackInputSelection& harnessRecordingTrackInput() const noexcept;
    // M11: the whole arm set, so gates can pin that several rows are armed at once.
    [[nodiscard]] const std::vector<yesdaw::ui::UiRecordingTrackInputSelection>&
        harnessArmedRecordingTrackInputs() const noexcept;
    [[nodiscard]] const yesdaw::ui::UiRecordedAudioTake& harnessLastRecordedAudioTake() const noexcept;
    [[nodiscard]] const yesdaw::ui::UiRecordedMidiTake& harnessLastRecordedMidiTake() const noexcept;
    [[nodiscard]] const yesdaw::ui::UiRecordingCompSelection& harnessRecordingComp() const noexcept;
    [[nodiscard]] const yesdaw::ui::UiAutosaveRecoveryPrompt& harnessAutosaveRecovery() const noexcept;
    [[nodiscard]] const std::filesystem::path& harnessBundlePath() const noexcept { return appModel.bundlePath(); }
    [[nodiscard]] bool harnessPrimaryFileChoicesReady() const noexcept;
    [[nodiscard]] bool harnessPlaybackReady() const noexcept { return appModel.playbackReady(); }
    [[nodiscard]] std::uint64_t harnessPlaybackReplaceCount() const noexcept { return appModel.playbackReplaceCount(); }
    [[nodiscard]] std::uint64_t harnessPlaybackLiveScalarsApplied() const noexcept { return appModel.playbackLiveScalarsApplied(); }
    [[nodiscard]] long long harnessPlaybackLoopStartFrame() const noexcept { return appModel.playbackLoopStartFrame(); }
    [[nodiscard]] long long harnessPlaybackLoopEndFrame() const noexcept { return appModel.playbackLoopEndFrame(); }
    [[nodiscard]] std::string harnessStatusLineText() const { return appModel.statusLineText(); }

    // G3.6: while a keyboard mode is on and nothing is hovered, the status line says so — the mode
    // is never silent (a swallowed key would otherwise look like a dead one).
    [[nodiscard]] juce::String hoverHintOrModeHint() const;
    void harnessReleaseTypedKeysForTest() { harnessReleaseTypedKeys(); }
    [[nodiscard]] bool harnessStatusLineIsError() const noexcept { return appModel.statusLineIsError(); }
    [[nodiscard]] long long harnessTimelineRangeStartFrame() const noexcept { return appModel.timelineRangeStartFrame(); }
    [[nodiscard]] long long harnessTimelineRangeEndFrame() const noexcept { return appModel.timelineRangeEndFrame(); }
    [[nodiscard]] double harnessTimelineZoomFactor() const noexcept { return timelineZoomFactor; }
    [[nodiscard]] double harnessTimelineScrollSeconds() const noexcept { return timelineScrollSeconds; }
    [[nodiscard]] int harnessTimelineTrackScrollRows() const noexcept { return timelineTrackScrollRows; }
    [[nodiscard]] int harnessPianoRollViewLowKey() const noexcept { return pianoRollViewLowKey; }
    [[nodiscard]] double harnessPianoRollViewZoom() const noexcept { return pianoRollViewZoom; }
    [[nodiscard]] long long harnessPianoRollViewScrollTicks() const noexcept;
    [[nodiscard]] int harnessTimelineMaxTrackScrollRows() const;
    [[nodiscard]] int harnessVisibleTimelineTrackCount() const;
    [[nodiscard]] int harnessVisibleTimelineClipCount() const;
    [[nodiscard]] std::string harnessVisibleFirstTimelineClipName() const;
    [[nodiscard]] int harnessSelectedTimelineClipCount() const;
    [[nodiscard]] double harnessVisibleTimelineTotalSeconds() const noexcept;
    [[nodiscard]] int harnessVisibleMixerTrackCount() const;
    [[nodiscard]] int harnessVisibleMixerBusCount() const;
    // E23: which strip the painted mixer highlights (tracks first, then buses; -1 = none).
    [[nodiscard]] int harnessSelectedMixerStripOrdinal() const;
    [[nodiscard]] bool harnessVisibleMixerLoudnessValid() const;
    [[nodiscard]] int harnessVisiblePianoRollNoteCount() const;
    [[nodiscard]] float harnessVisibleMasterPeakLeft() const noexcept;
    [[nodiscard]] float harnessVisibleMasterPeakRight() const noexcept;
    [[nodiscard]] bool harnessDesktopAudioRequested() const noexcept { return desktopAudioRequested; }
    [[nodiscard]] bool harnessDesktopAudioOpen() const noexcept;
    [[nodiscard]] std::uint64_t harnessDeviceAudioCallbackBlockCount() const noexcept;
    [[nodiscard]] std::uint64_t harnessDeviceAudioNonSilentBlockCount() const noexcept;
    [[nodiscard]] bool harnessProcessDeviceAudioBlock (float* const* outputChannels,
                                                       int numOutputChannels,
                                                       int numFrames) noexcept;
    // E30: input-carrying harness block — drives the same input-aware model path the native
    // device callback uses, so input metering is CI-deterministic.
    [[nodiscard]] bool harnessProcessDeviceAudioBlock (const float* const* inputChannels,
                                                       int numInputChannels,
                                                       float* const* outputChannels,
                                                       int numOutputChannels,
                                                       int numFrames) noexcept;
    // N1: the painted Mute/Solo cell rect for a strip, in SHELL coordinates (cell 0 = Solo,
    // 1 = Mute) — the same law the paint, the click hit-test and the live buttons read.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedMuteSoloCellBounds (int stripIndex, int cellIndex) const;

    // G4.1: the painted I/O row rect for a strip (shell coordinates) — the same law the paint and the
    // click read; empty where the strip has no such slot or is too short to carry it.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedIoRowBounds (int stripIndex, int row) const;

    // G4.1: the two slots' texts as painted.
    [[nodiscard]] yesdaw::ui::MainComponentMixerStripIo harnessMixerStripIo (int stripIndex) const;

    // M4: the painted insert-slot rect for a strip, in SHELL coordinates — the same law the paint
    // and the click hit-test read.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedInsertSlotBounds (int stripIndex, int slotIndex) const;

    // M5: the painted send-row rect for a strip, in SHELL coordinates.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedSendRowBounds (int stripIndex, int sendIndex) const;

    // M6: the painted fader rail and the y the thumb sits at for a given gain — the same law the
    // paint uses, so a gate can prove unity is NOT at the top of the rail.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedFaderRailBounds (int stripIndex) const;

    [[nodiscard]] juce::Rectangle<int> harnessPaintedPanKnobBounds (int stripIndex) const;

    [[nodiscard]] int harnessPaintedFaderThumbY (int stripIndex, float linearGain) const;

    // ---- G0.1 State probe ------------------------------------------------------------------
    // One JSON document of what the shell is right now: transport, selection, focus, view,
    // frame/audio counters, and a `layout` map of shell-coordinate hit rects keyed by element
    // id so a Session script clicks by NAME (`widget.transport.play`, `lane.0`, `clip.<hex>`),
    // never by pixel. Every rect comes from the SAME law the paint and hit-test paths use.

    [[nodiscard]] static juce::var probeRect (juce::Rectangle<int> rect);

    [[nodiscard]] static const char* probeFocusContextName (yesdaw::ui::UiPanel panel) noexcept;

    [[nodiscard]] static const char* probeToolName (yesdaw::ui::TimelineTool tool) noexcept;

    [[nodiscard]] juce::String probeRendererName() const;

    [[nodiscard]] double probePaintP95Ms() const;

    [[nodiscard]] juce::var buildProbeLayout();

    // G0.4: what identified widgets SAY (combo / button / label text by component id) — the
    // drive asserts on words, never on pixels, and a stale control is visible in the document.
    [[nodiscard]] juce::var buildProbeText() const;

    [[nodiscard]] juce::String buildStateProbeJson();

    void writeStateProbeIfEnabled();

    // N6: the rail's painted row rect (shell coordinates), the SAME law rowBounds/rowAt/paint
    // share — so a gate can prove a height drag moved exactly one row and left every other row's
    // position/height alone.
    // G2.18: the undo history window for the harness.
    [[nodiscard]] UndoHistoryComponent& harnessUndoHistory() noexcept { return undoHistory; }
    [[nodiscard]] InstrumentPanelComponent& harnessInstrumentPanel() noexcept { return instrumentPanel; }   // G3.1
    [[nodiscard]] yesdaw::ui::UiPianoRollSurfaceSnapshot harnessPianoRollSurface() const { return currentPianoRollSurface(); }   // G3.2
    [[nodiscard]] juce::Rectangle<int> harnessPianoRollBounds() const { return pianoRollInput.getBounds(); }
    [[nodiscard]] int harnessPianoRollAuditionKey() const { return pianoRollInput.heldAuditionKey(); }   // G3.2
    void harnessSelectPianoRollControlLane (int choice) { pianoRollLaneChooser.setSelectedId (choice + 1, juce::sendNotificationSync); }   // G3.3
    [[nodiscard]] bool harnessPostMidiInput (bool on, int key, double velocity) noexcept;   // G3.10: the device callback's path
    void harnessSelectPianoRollScale (int rootKey, int scaleChoice);   // G3.8: through the real choosers
    void harnessServiceUiTick() { serviceUiTick(); }   // G3.2 checkpoint: the timer's own refresh path
    [[nodiscard]] bool harnessAuditionNote (std::int16_t key, bool on) { return appModel.auditionNote (key, on); }   // G3.2
    [[nodiscard]] std::vector<float> harnessRenderPlayback (std::uint64_t frames, int blockSize);   // G3.2: the engine's own blocks

    [[nodiscard]] juce::Rectangle<int> harnessPaintedRailRowBounds (int row) const;

    // N7: the painted colour-swatch rect for a rail row (the left accent bar), in shell
    // coordinates — the same law the click-to-cycle gesture hit-tests against.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedColourSwatchBounds (int row) const;

    [[nodiscard]] double harnessTimelineZoomCeiling() const noexcept { return timelineZoomCeiling(); }   // G2.19

    // The source window the canvas was handed for a painted clip (the waveform painter's law).
    [[nodiscard]] yesdaw::ui::TimelineClipSourceWindow harnessTimelineClipSourceWindow (int layoutClipId) const;

    // The rail's three painted cells (M / S / O) in shell coordinates — the rects the rail's
    // hit-test claims, so a test or a drive clicks the badge it sees.
    [[nodiscard]] juce::Rectangle<int> harnessPaintedRailCellBounds (int row, int cell) const;

    // V2: the ACTUAL bar|beat the header paints — reads the same law drawTransportReadouts uses,
    // so a test can never duplicate the formula.
    [[nodiscard]] yesdaw::engine::BarBeat harnessHeaderBarBeat() const { return headerBarBeat(); }

    // V4: the ruler's painted bar labels — the SAME state build, geometry, and label law the
    // paint path runs (makeTimelineState → timelineCanvasGeometry → computeRulerBarLabels), so a
    // gate can never re-derive the formula.
    [[nodiscard]] std::vector<yesdaw::ui::RulerBarLabel> harnessRulerBarLabels();

    // V5: the rail's live L/R meter peaks for one row — the SAME hold-state values the paint
    // passes to shell::drawMeterWithHold, so a gate can prove the two channels really diverge.
    [[nodiscard]] std::pair<float, float> harnessRailMeterChannelPeaks (int row) const;

    // V5: the rail VOL fader's rect in SHELL coordinates — the SAME law paint and the click/drag
    // hit-test share, so a gate can prove the control is genuinely vertical.
    [[nodiscard]] juce::Rectangle<int> harnessRailVolumeSliderBounds (int row) const;

    // V7: the fade chart's inner rect in SHELL coordinates — the SAME law the paint uses.
    [[nodiscard]] juce::Rectangle<int> harnessInspectorFadeChartBounds() const;

    // V4: the inverse pixel→seconds mapping of the SAME viewport the ruler paints with, so a
    // gate can cross-check a label's x against the tempo map without duplicating the paint math.
    [[nodiscard]] double harnessRulerSecondsAtX (int x);

    // N7: the ACTUAL colour the timeline canvas will paint for one clip (by id) — reads the same
    // cached timelineClipStyles/timelineClipIds arrays paintTimelineCanvas() paints from,
    // refreshing them first so this can never report a stale value from before the caller's last
    // edit.
    [[nodiscard]] juce::Colour harnessTimelineClipColour (yesdaw::engine::EntityId clipId);

    // N3: the painted mixer-strip lane rect for a track/bus strip, and the master pane's rect —
    // exposed so a gate can prove they share ONE law (master is always the next contiguous slot
    // after the last strip, never a detached island computed independently of it).
    [[nodiscard]] juce::Rectangle<int> harnessPaintedMixerStripBounds (int stripIndex) const;

    [[nodiscard]] juce::Rectangle<int> harnessPaintedMixerMasterBounds() const;

    // V3: the dock's OWN reserved rect — height collapses to (near) zero when the toggle hides
    // it, the same law every layout function (timelineBounds/leftRailPanelBounds/inspectorBounds
    // /this) shares via dockedMixerHeight().
    [[nodiscard]] juce::Rectangle<int> harnessMixerPanelBounds() const;

    [[nodiscard]] juce::Rectangle<int> harnessTimelineBounds() const;

    [[nodiscard]] std::vector<float> harnessRenderPlaybackFrames (std::uint64_t frames, int blockSize);

    // Window title with the dirty marker (B38): "<bundle stem>[*] - YES DAW" once a project is
    // open; empty otherwise so the app keeps its versioned startup title. State-derived, so the
    // harness snapshot reads it directly and the UI tick pushes it to the native window.
    [[nodiscard]] juce::String computedWindowTitle() const;

    void pushWindowTitle();

    // Close-confirm flow (B37): a clean session closes silently; edits since the last explicit
    // Save ask through the injectable seam (native three-way box otherwise). Closing never rolls
    // back the always-persisted bundle; Save records this state as the saved version.
    [[nodiscard]] bool confirmClose();
    [[nodiscard]] bool isUnnamedLaunchProject() const;
    static constexpr const char* unnamedBundleName = "Untitled.yesdaw";
    static constexpr const char* unnamedMarkerName = ".yesdaw-untitled";
    [[nodiscard]] bool saveCurrentProject (bool chooseDestination);

    // G0.1 probe: paint() opens the frame stamp and paintOverChildren() closes it — JUCE paints
    // this component, then every child, then paintOverChildren on the same component, so the
    // pair brackets the whole shell's paint work for one frame (the B2 budget).
    void paintOverChildren (juce::Graphics&) override;

    void paint (juce::Graphics& g) override;

    void resized() override;

private:
    template <typename Component>
    void configureActionComponent (Component& component,
                                   yesdaw::ui::UiActionId action,
                                   const juce::String& fallbackName)
    {
        actionComponents.emplace_back (&component, action);   // G1.6: tooltips follow the live keymap
        if (const auto* descriptor = appModel.registry().descriptor (action))
        {
            component.setComponentID (descriptor->stableId);
            component.setName (descriptor->accessibleName);
            component.setTitle (descriptor->label);
            // The tooltip names the action and its chord straight from the descriptor table so it
            // can never drift from the keymap (B40).
            component.setTooltip (juce::String (descriptor->accessibleName)
                                  + "  (" + descriptor->defaultKey + ")");
            return;
        }

        component.setName (fallbackName);
    }

    void configureAutosaveRecoveryButton (juce::TextButton& button, yesdaw::ui::UiActionId action);

    // V3: a real toggle for the always-on bottom mixer dock — collapsing it reclaims vertical
    // space for the timeline/rail/inspector; the full-view Mixer panel is unaffected.
    void configureMixerDockToggle();

    // V7: the inspector's CLIP/TRACK tabs become real buttons — each dispatches a genuine
    // UiActionId, the model owns the active-tab state, and layout/paint follow it.
    void configureInspectorTabs();

    // V8: a visible toolbar zoom control. The buttons dispatch the EXISTING zoom actions through
    // handleAction (the same playhead-anchored law the menu/keyboard path runs), and the readout
    // shows the one shared timelineZoomFactor every zoom gesture mutates — never a second zoom
    // concept.
    void configureTimelineZoomControls();

    void configureAutomationLaneControls();

    void configureInspectorControls();

    void configureInspectorTimeSlider (juce::Slider& slider, const char* componentId, const juce::String& name);

    // G2.10: the chooser id <-> engine shape.
    [[nodiscard]] static yesdaw::engine::FadeShape fadeShapeForInspectorId (int id) noexcept;

    [[nodiscard]] static int inspectorIdForFadeShape (yesdaw::engine::FadeShape shape) noexcept;

    void configureInspectorFadeSlider (juce::Slider& slider, const char* componentId, const juce::String& name);

    void configureMixerControls();

    // G1.4: the inspector's width right now — the token, or nothing while it is hidden (I).
    [[nodiscard]] int inspectorWidthNow() const noexcept;

    // G4.1 cp2: the FX editor opens on ONE slot of ONE strip (the double-click, the slot menu's Open
    // Editor, the harness) and closes on Close / Escape / the strip or slot going away (the refresh law).
    void openFxEditor (int stripIndex, int slotIndex);

    void closeFxEditor();

    // G2.1: the splitters set these; each clamps to the plan's §3.4 ranges, lays out and repaints.
    void setRailWidth (int width);

    void setInspectorWidth (int width);

    void setDockHeight (int height);

    // The view state follows the project: a different bundle loads its own record (or the
    // defaults); every splitter release writes the record. Polled from refreshActionState so
    // every open / new / restore path is covered by the one law.
    void loadViewStateIfBundleChanged();

    // G2.1 cp2: which editor tab the dock shows.
    [[nodiscard]] bool dockShowsMixer() const noexcept;

    [[nodiscard]] bool dockShowsPianoRoll() const noexcept;

    [[nodiscard]] bool dockShowsInstrument() const noexcept;   // G3.1

    [[nodiscard]] juce::Component* toolbarButtonFor (yesdaw::ui::UiActionId action);

    void setToolbarButtonBounds (yesdaw::ui::UiActionId action, juce::Rectangle<int> bounds);

    // The mixer's control lane (every widget the mixer tab owns). While another tab shows, they
    // are hidden as a set and restored to what their own laws last chose when the mixer returns:
    // restore at the start of every refresh / layout, hide at the end. The one list is the law —
    // the [dock-tabs] gate walks the dock rect and refuses any stray visible widget.
    [[nodiscard]] std::vector<juce::Component*> mixerLaneControls();

    void restoreControlsHiddenByDockTab();

    void hideMixerControlsBehindDockTab();

    [[nodiscard]] std::string viewStateRecordText() const;

    void saveViewState();

    // G2.5: what Zoom to Selection needs from the view, in one place.
    struct MainComponentSnapshotLike
    {
        double rangeStartSeconds = 0.0, rangeSeconds = 0.0, widthPixels = 0.0, fitPixelsPerSecond = 0.0;
    };
    [[nodiscard]] MainComponentSnapshotLike snapshotForZoom() const;

    // G2.16: the timeline PANEL holds the canvas plus the scroll-bar strips below and beside it;
    // timelineBounds() is the canvas alone — the ONE rect the input, the playhead layer, the
    // toolbar cluster, the fit law and the probe share, so a pixel means the same time everywhere.
    [[nodiscard]] juce::Rectangle<int> timelinePanelBounds() const;

    [[nodiscard]] juce::Rectangle<int> timelineBounds() const;

    // The exact rect drawTrackList paints into; the rail input overlay shares it so hits match paint.
    [[nodiscard]] juce::Rectangle<int> leftRailPanelBounds() const;

    // G2.17: multi-select — Ctrl toggles a lane in the set, Shift extends from the primary; a plain
    // click and the Up / Down verbs collapse to one. The primary stays the lane the strip verbs act on.
    void toggleTrackLaneSelection (int lane);

    void extendTrackLaneSelection (int lane);

    [[nodiscard]] bool trackLaneIsSelected (int lane) const noexcept;

    void selectTrackLane (int lane);

    // G3.9: a WAV onto a Sampler pad — the WAV reader's refusal is the shell's to name (R6), every
    // other refusal the model's (R7).
    void loadSamplerPadFromPath (std::int16_t key, const std::filesystem::path& path);

    // G3.1: the instrument kind's name (the probe, the panel title, the inspector row).
    [[nodiscard]] static const char* instrumentKindName (yesdaw::engine::TrackInstrumentKind kind) noexcept;

    // G3.1: the panel's rows — one per ParamSpec of the selected Track's effective instrument,
    // the value read back from the project (an unset id shows the spec default).
    [[nodiscard]] std::vector<InstrumentPanelComponent::Row> instrumentPanelRows() const;

    // G2.17: a track is a MIDI track when it holds MIDI clips; audio otherwise (an empty track is audio).
    [[nodiscard]] bool trackHoldsMidi (std::size_t trackIndex) const noexcept;

    void selectAdjacentTrackLane (yesdaw::ui::UiActionId action);

    void openTrackRenameEditor();

    // The same editor and commit path as the rail's rename, placed over the mixer strip's name
    // band (the bus editor's law) — the strip's track becomes the selected lane first, since
    // commitTrackRenameEditor renames selectedTrackLane.
    void openTrackRenameEditorOverStrip (int stripOrdinal);

    void commitTrackRenameEditor();

    void dismissTrackRenameEditor();

    void openClipRenameEditor();

    void commitClipRenameEditor();

    // Marker rename (E7): positioned over the painted label through the shared rect law.
    void openMarkerRenameEditor (int markerIndex);

    // E17: inline bus rename — the editor sits over the bus strip's header area.
    void openBusRenameEditor (int busIndex, int stripOrdinal);

    void commitBusRenameEditor();

    void dismissBusRenameEditor();

    void commitMarkerRenameEditor();

    void dismissMarkerRenameEditor();

    void dismissClipRenameEditor();

    void removeSelectedTrack();

    // Selected-track strip/arm toggles (B28): the rail row is the target; the mixer never opens.
    void toggleSelectedTrackKey (yesdaw::ui::UiActionId action);

    void moveSelectedTrack (int delta);

    // Per-track meter peak-hold and clip-latch state (B32), advanced once per UI refresh tick so
    // gates can drive it deterministically through serviceMainComponentUiTimer.
    struct MeterHoldState
    {
        float livePeak = 0.0f;
        float heldPeak = 0.0f;
        int holdTicksRemaining = 0;
        bool clipLatched = false;
    };

    static void advanceMeterHold (MeterHoldState& state, float livePeak);

    void updateTrackMeterHoldStates();

    void clearTrackMeterHold (int trackIndex);

    // E22: a click on a painted BUS meter clears its hold and latch, like the track law.
    void clearBusMeterHold (int busIndex);

    // Live gain readout in dB (B31): 20*log10(linear gain), "-inf dB" at silence.
    [[nodiscard]] static juce::String dbReadoutText (double linearGain);

    void showDragDbReadout (juce::Rectangle<int> anchorBounds, double linearGain);

    void hideDragDbReadout();

    void duplicateSelectedTrack();

    // Open a project bundle at a known path (B39): shared by File > Open and Open Recent.
    void openProjectBundleAtPath (const std::filesystem::path& path);

    // V3: the always-on bottom dock's height — 0 collapses it entirely, reclaiming the space for
    // the timeline/rail/inspector, when the user has toggled it off (the full-view Mixer panel
    // is unaffected: it never reserves this space in the first place). ONE law shared by every
    // layout function below, so paint and every interactive component's bounds can never drift.
    [[nodiscard]] int dockedMixerHeight() const;

    [[nodiscard]] juce::Rectangle<int> mixerPanelBounds() const;

    // E25: ONE strip geometry — the interactive control lane, the click law, and the paint all
    // share the painted-strip law (the old width/(count+1) law visibly diverged from the
    // painted lanes at real window sizes, floating the control lane off its strip).
    [[nodiscard]] juce::Rectangle<int> mixerStripBounds (int stripIndex) const;

    // N3: the painted MASTER pane rect. Master is lane index stripCount in the SAME
    // paintedMixerLaneBounds law every track/bus strip uses — it is the strip immediately after
    // the last one, never a detached island computed from the far right of a stale area. Before
    // N3 this peeled its slice off the right edge of the FULL panel independently of how many
    // strips were drawn from the left, so a clamped strip width (max 112px) left ~1250px of dead
    // black between the last strip and master at 1920x1080.
    [[nodiscard]] juce::Rectangle<int> paintedMixerMasterBounds() const;

    // Shared painted-strip geometry law (B32): hit-testing must mirror drawMixer's lane math
    // exactly so a meter click can never drift from the painted meter.
    [[nodiscard]] juce::Rectangle<int> paintedMixerLaneBounds (std::size_t stripIndex) const;

    // M4: how many insert rows this strip can afford. A tall mixer-view strip shows the whole
    // column; the timeline view's short mini-mixer drops rows rather than starving the fader, and a
    // strip with no room at all falls back to the exact historical fader top.
    [[nodiscard]] static int paintedInsertRowCountForLane (juce::Rectangle<int> lane) noexcept;

    // G4.1: how many I/O slot rows a strip's KIND carries — a Track two (input and output), a Bus one
    // (output), the master none — and how many cells its S / M / R row has.
    [[nodiscard]] int stripIoRows (std::size_t stripIndex) const noexcept;

    [[nodiscard]] std::size_t stripCellCount (std::size_t stripIndex) const noexcept;

    // G4.1: how many of the strip's I/O rows the lane can afford — after the inserts, before the sends.
    [[nodiscard]] static int paintedIoRowsShownForLane (juce::Rectangle<int> lane, int ioRows) noexcept;

    // The input row (a Track's) leads the slot column; the output row closes it.
    [[nodiscard]] static int paintedInputRowsForLane (juce::Rectangle<int> lane, int ioRows) noexcept;

    [[nodiscard]] static juce::Rectangle<int> paintedIoRowRect (juce::Rectangle<int> lane, int top);

    [[nodiscard]] static juce::Rectangle<int> paintedInputRowBoundsForLane (juce::Rectangle<int> lane, int ioRows);

    [[nodiscard]] static juce::Rectangle<int> paintedOutputRowBoundsForLane (juce::Rectangle<int> lane, int ioRows);

    // M6: ONE fader mapping. The sliders travel 0..mixerFaderSliderMax in LINEAR gain, so the
    // painted thumb, the unity mark and every dB tick must read the same law — before M6 the paint
    // put unity at the TOP of the rail while the live slider put it at half travel.
    [[nodiscard]] static float mixerFaderFractionForGain (float linearGain) noexcept;

    [[nodiscard]] static float mixerFaderFractionForDb (float db) noexcept;

    [[nodiscard]] static juce::Rectangle<int> paintedFaderRailForLane (juce::Rectangle<int> lane, int ioRows);

    // The painted pan knob's disc for a strip lane — the SAME rect the strip paint fills, so the
    // drag hit-test and the picture cannot drift.
    [[nodiscard]] static juce::Rectangle<int> paintedPanKnobForLane (juce::Rectangle<int> lane);

    // The painted fader THUMB for a strip lane at a gain — the grab target (the rail itself stays a
    // strip-select click: it overlaps the strip's centre line, which every strip click lands on).
    [[nodiscard]] static juce::Rectangle<int> paintedFaderThumbForLane (juce::Rectangle<int> lane, float linearGain, int ioRows);

    [[nodiscard]] static int mixerFaderThumbYForGain (juce::Rectangle<int> rail, float linearGain) noexcept;

    // M5: the send rows follow the inserts, and take space only after the inserts have taken
    // theirs — a short strip drops sends first, then inserts, and never starves the fader.
    [[nodiscard]] static int paintedSendRowCountForLane (juce::Rectangle<int> lane, int ioRows) noexcept;

    [[nodiscard]] static int paintedSendsTopForLane (juce::Rectangle<int> lane, int ioRows) noexcept;

    [[nodiscard]] static juce::Rectangle<int> paintedSendRowBoundsForLane (juce::Rectangle<int> lane,
                                                                           std::size_t sendIndex,
                                                                           int ioRows);

    [[nodiscard]] static int paintedFaderTopForLane (juce::Rectangle<int> lane, int ioRows) noexcept;

    // N1: ONE Mute/Solo cell law — the paint, the click hit-test, the SELECTED strip's live
    // buttons and the gates all read it, so the control you see is exactly the control you hit,
    // on every strip. Cell 0 is Solo, cell 1 is Mute (left to right, as painted).
    // G4.1: the row is S / M / R on a Track strip (cellCount 3), S / M on a Bus (2); a narrow strip
    // shrinks the cells so they still sit side by side inside the lane.
    [[nodiscard]] juce::Rectangle<int> paintedMuteSoloCellBoundsForLane (juce::Rectangle<int> lane,
                                                                         std::size_t cellIndex,
                                                                         std::size_t cellCount) const;

    // M4: ONE insert-slot row law — the paint, the click hit-test and the gates all read it, so a
    // painted slot can never drift from the slot a click selects. An empty rect means the strip has
    // no room for that row.
    [[nodiscard]] static juce::Rectangle<int> paintedInsertRowBoundsForLane (juce::Rectangle<int> lane,
                                                                             std::size_t slotIndex,
                                                                             int ioRows);

    [[nodiscard]] static juce::Rectangle<int> paintedMeterBoundsForLane (juce::Rectangle<int> lane, int ioRows);

    // G3.4: the quantize panel shows on the CLIP tab when a MIDI clip is selected and no audio clip
    // is (an audio clip's own card wins; the TRACK tab is the track's).
    [[nodiscard]] bool inspectorShowsQuantizePanel() const;

    // The MIDI clip's inspector rows below the title — G3.5's four (mute, transpose, velocity, loop)
    // then G3.4's six (grid, strength, swing, note ends, humanize, apply) — ONE law for the paint
    // (labels on the left) and the controls (on the right).
    static constexpr std::size_t kMidiClipInspectorRows = static_cast<std::size_t> (yesdaw::ui::UiTheme::Layout::inspectorMidiClipRowCount) + 6u;
    [[nodiscard]] static std::array<juce::Rectangle<int>, kMidiClipInspectorRows> midiClipInspectorRows (juce::Rectangle<int> content) noexcept;

    [[nodiscard]] juce::Rectangle<int> inspectorBounds() const;

    void layoutInspectorControls();

    void layoutMixerControls();

    void layoutAutomationLaneControls();

    void suspendDesktopAudioCallback();

    void resumeDesktopAudioCallback();

    // The keymap's declared chords are live application shortcuts: any KeyPress whose chord matches a
    // registered action dispatches through the SAME handleAction path the toolbar uses, so Space plays,
    // Ctrl+Z undoes, Del deletes the selected Clip, and every binding stays mechanically listable.
    bool keyPressed (const juce::KeyPress& key) override;

    // G0.4 layered invalidation. repaintAll() is what every model/view change calls (the old
    // whole-window repaint()), and it also invalidates the buffered timeline canvas so the static
    // layer can never go stale. repaintDynamicLayers() is what the tick calls: the playhead layer,
    // the rail (meters), the dock (meters, master), and the header band (transport counter) — the
    // expensive clip/waveform canvas is left to its cache.
    void repaintAll();

    void repaintDynamicLayers();

    // G0.2 Command router (ADR-0046 §4). Only an active text field consumes keys: every other
    // widget declines keyboard focus, so a click on a button, combo, or slider hands focus to
    // THIS component (JUCE walks a click's focus grab up to the first ancestor that wants it)
    // and the next chord dispatches through keyPressed above. The KeyListener half below catches
    // the one remaining hole: right after launch (and whenever focus falls back to the window
    // itself) the DocumentWindow, not the shell, is the focused component — its key listeners
    // run before its own keyPressed, so the chord still reaches the router.
    void applyKeyboardFocusLaw();

    static void applyKeyboardFocusLawTo (juce::Component& parent);

    void childrenChanged() override;

    void parentHierarchyChanged() override;

    // KeyListener half of the router: chords that reach the top-level window (focus on the
    // window itself, or on a child that did not consume them) dispatch exactly as if this
    // component were focused. A text editor that originated the event keeps its keys.
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    bool keyStateChanged (bool, juce::Component*) override { return false; }
    // Declared alongside the KeyListener overload so neither hides the other (Clang's
    // -Woverloaded-virtual is an error here); the Component half keeps its default behaviour.
    bool keyStateChanged (bool isKeyDown) override;

    // G3.6: the harness's key-up (the headless run has no real keyboard for isKeyCurrentlyDown).
    void harnessReleaseTypedKeys();

    [[nodiscard]] bool cancelInProgressEdit();

    // G3.3: a mouse gesture that lands on a model verb without passing handleAction still names
    // itself to the probe (a drive asserts on lastAction; nothing is blind).
    void recordLastAction (yesdaw::ui::UiActionId action);

    void handleAction (yesdaw::ui::UiActionId action);

    // Vertical track scroll (E5): one shared whole-row offset moves the timeline lanes and the
    // track rail together. The shared clamp honors WHICHEVER surface overflows more, and each
    // surface pins its own applied offset so its last row never scrolls past the window bottom.
    void scrollTrackRowsBy (int rowDelta);

    // V8: the ONE place the toolbar readout learns the current factor — called from every path
    // that mutates timelineZoomFactor, so the visible number can never go stale against the
    // gestures (wheel, zoom tool, actions, and the fit verbs all funnel here or call it).
    void refreshTimelineZoomReadout();

    // G2.16: the zoom history — every zoom change pushes the view it leaves; Zoom Back pops.
    struct ZoomView { double zoom; double scroll; };
    std::vector<ZoomView> zoomHistory;
    void pushZoomHistory();
    [[nodiscard]] bool popZoomHistory();

    // G2.16: the vertical zoom — every auto-height row scales; the rail and the canvas share it.
    void zoomTracksBy (double factor);

    // G2.16: the scroll bars mirror the view (seconds horizontally, rows vertically) and drive it.
    void refreshTimelineScrollBars();

    void scrollBarMoved (juce::ScrollBar* bar, double newRangeStart) override;

    // G2.19: the zoom ceiling — one sample per pixel at the project's rate (never below the old
    // 64x floor). It follows the fit width, so it is a function, not a token.
    [[nodiscard]] double timelineZoomCeiling() const noexcept;

    void zoomTimelineAtAnchor (double anchorSeconds, double factor);

    [[nodiscard]] double timelinePixelsPerSecondFor (double totalSeconds) const noexcept;

    [[nodiscard]] double timelineVisibleSecondsFor (double totalSeconds) const noexcept;

    // G3.2: the roll pages after the playhead while it shows and the transport rolls — the same
    // page law as the arrangement, in the clip's ticks. Returns true when the view moved.
    bool followPianoRollPlayhead();

    void followPlaybackPlayhead();

    // Real menu bar (usable-DAW P1): the painted FILE/EDIT/VIEW text is gone; a juce::MenuBarComponent
    // over the same header spot dispatches registered actions through the SAME handleAction path the
    // toolbar and keymap use. The model is mechanically testable without opening popups.
    // G1.2 (plan §3, Logic's order): File · Edit · Track · Clip · MIDI · View · Transport ·
    // Options · Help. Every item paints the chord that fires it in the CURRENT Focus context.
    juce::StringArray getMenuBarNames() override;

    [[nodiscard]] static std::span<const yesdaw::ui::UiActionId> menuActionsForIndex (int topLevelMenuIndex);

    // G1.2: the tick a menu item shows — the registry context's own flags, one law for every
    // toggle and every "which one is current" group (views, inspector tabs, snap presets).
    [[nodiscard]] bool menuTickState (yesdaw::ui::UiActionId action) const noexcept;

    // G1.3: the context menu for a target — the same registry-driven item law the menu bar
    // uses (label, enabled, tick, chord for the focus context). Recorded for the harness, and
    // shown only when the shell is on a real desktop (headless gates read the record).
    struct LastContextMenu
    {
        bool shown = false;
        yesdaw::ui::ContextMenuTarget target = yesdaw::ui::ContextMenuTarget::Clip;
        int index = -1;
        std::vector<yesdaw::ui::UiActionId> actions;
        std::vector<int> addInsertKinds;   // G4.1: the kinds the Add Insert submenu offered

        [[nodiscard]] yesdaw::ui::MainComponentContextMenu toPublic (const juce::String& route = "none") const
        {
            yesdaw::ui::MainComponentContextMenu out;
            out.route = route;
            out.shown = shown;
            out.target = target;
            out.index = index;
            out.actions = actions;
            out.addInsertKinds = addInsertKinds;
            return out;
        }
    };
    LastContextMenu lastContextMenu;

    void openContextMenu (yesdaw::ui::ContextMenuTarget target, int index, juce::Component& source,
                          juce::Point<int> sourcePosition);

    static constexpr int kContextMenuMoveUpId = 2001;
    static constexpr int kContextMenuMoveDownId = 2002;
    static constexpr int kContextMenuAddInsertBase = 3001;      // + FxKind
    static constexpr int kContextMenuTimeDisplayBase = 3100;    // + time display mode (G2.2)
    static constexpr int kContextMenuAddInsertKindCount = 9;    // Eq … Limiter, the four MIDI FX (G3.8)
    // G4.1: the routing choices — a send destination (+ bus index), an output (0 Master, 1 + bus
    // index), an input (mono + channel; the pair starting at + channel). One range each.
    static constexpr int kContextMenuAddSendBase = 3200;
    static constexpr int kContextMenuOutputBase = 3300;
    static constexpr int kContextMenuInputMonoBase = 3400;
    static constexpr int kContextMenuInputPairBase = 3500;
    static constexpr int kContextMenuSendDestBase = 3600;      // G4.1 cp2: the send row's Destination (+ bus index)
    static constexpr std::size_t kContextMenuChoiceRange = 100;

    // The one path a picked context-menu item takes (the popup's callback and the harness): the
    // insert-slot verbs act on the clicked slot through the shell's per-slot handlers; every
    // other target dispatches the action.
    void invokeContextMenuItem (int itemId);

public:
    void harnessSetDockHeight (int height) { setDockHeight (height); }   // G2.1
    double harnessTimelineAutoScrollTick() { return timelineInput.autoScrollTick(); }   // G2.3
    // G2.4: the Smart tool's zone and cursor under a shell point.
    [[nodiscard]] juce::String harnessTimelineZoneAt (juce::Point<int> shellPoint, juce::ModifierKeys modifiers) const;
    [[nodiscard]] juce::String harnessTimelineCursorAt (juce::Point<int> shellPoint, juce::ModifierKeys modifiers) const;
    void harnessInvokeContextMenuId (int itemId) { invokeContextMenuItem (itemId); }   // G2.2
    [[nodiscard]] static constexpr int harnessTimeDisplayMenuId (int mode) noexcept { return kContextMenuTimeDisplayBase + mode; }
    // G4.1: the routing choices' ids, the last menu built, the view-state record.
    [[nodiscard]] static constexpr int harnessMixerInputMenuId (int channel, bool stereoPair) noexcept
    {
        return (stereoPair ? kContextMenuInputPairBase : kContextMenuInputMonoBase) + channel;
    }
    [[nodiscard]] static constexpr int harnessMixerOutputMenuId (int choice) noexcept { return kContextMenuOutputBase + choice; }
    [[nodiscard]] static constexpr int harnessMixerSendMenuId (int busIndex) noexcept { return kContextMenuAddSendBase + busIndex; }
    [[nodiscard]] static constexpr int harnessMixerSendDestinationMenuId (int busIndex) noexcept { return kContextMenuSendDestBase + busIndex; }
    // G4.1 cp2: the FX editor as the harness reads it, and the open the double-click performs.
    void harnessOpenFxEditor (int stripIndex, int slotIndex) { openFxEditor (stripIndex, slotIndex); }
    [[nodiscard]] yesdaw::ui::MainComponentFxEditor harnessFxEditor() const;
    [[nodiscard]] yesdaw::ui::MainComponentContextMenu harnessLastContextMenu() const { return lastContextMenu.toPublic(); }
    [[nodiscard]] juce::String harnessViewStateRecord() const { return juce::String (viewStateRecordText()); }

    void harnessInvokeContextMenuItem (yesdaw::ui::UiActionId action, int direction);
private:

public:
    // Harness: route a shell point to the input surface under it and run its right-click law.
    [[nodiscard]] yesdaw::ui::MainComponentContextMenu harnessRequestContextMenu (juce::Point<int> shellPoint);

private:
    // G1.6: the hovered zone's gesture hint; the status line shows it while no message is active.
    void setHoverHint (const juce::String& hint);

    // G1.6: every action-backed control's tooltip quotes its LIVE chord (a rebind in the keymap
    // editor changes the tooltip), from the registry's descriptor name and the keymap.
    void refreshActionTooltips();

public:
    [[nodiscard]] juce::String harnessHoverHintAt (juce::Point<int> shellPoint);
private:

    // G1.2: the chord a menu item paints — the one that fires in the CURRENT Focus context
    // (its own binding or a Global one); another editor's binding paints nothing.
    [[nodiscard]] juce::String menuShortcutFor (yesdaw::ui::UiActionId action) const;

    // Transport ▸ Locate Points (a submenu, not a flat run of ten): the store / recall pairs.
    static constexpr std::array<yesdaw::ui::UiActionId, 10> kLocatePointMenu {
        yesdaw::ui::UiActionId::TransportStoreLocatePoint1,  yesdaw::ui::UiActionId::TransportStoreLocatePoint2,
        yesdaw::ui::UiActionId::TransportStoreLocatePoint3,  yesdaw::ui::UiActionId::TransportStoreLocatePoint4,
        yesdaw::ui::UiActionId::TransportStoreLocatePoint5,
        yesdaw::ui::UiActionId::TransportRecallLocatePoint1, yesdaw::ui::UiActionId::TransportRecallLocatePoint2,
        yesdaw::ui::UiActionId::TransportRecallLocatePoint3, yesdaw::ui::UiActionId::TransportRecallLocatePoint4,
        yesdaw::ui::UiActionId::TransportRecallLocatePoint5,
    };

    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String&) override;

    void menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/) override;

    // Device chooser plumbing (usable-DAW P1): harness seams win when injected; the native shell
    // talks to the JUCE device manager.

    [[nodiscard]] bool selectAudioOutputDeviceByName (const std::string& name);

    // E29: input-side twins of the output plumbing. Switching the input device restarts the
    // JUCE device, which re-runs audioDeviceAboutToStart and re-adopts the E28 profile.

    [[nodiscard]] bool selectAudioInputDeviceByName (const std::string& name);

    void refreshAudioDeviceChooser();

    // E29: options track the adopted profile's generation so a device change re-lists them.
    void refreshRecordingInputChannelChooser();

    void handleActionWhileAudioStopped (yesdaw::ui::UiActionId action);

    void refreshActionState();

    void refreshAutomationLaneControls();

    [[nodiscard]] juce::String automationLaneRowText() const;

    // N5: normalized [0,1] breakpoint value for a live linear-gain fader read, matching
    // FaderNode::linearGainForNormalizedEvent's dB-range mapping exactly (its inverse) — so a
    // point recorded here plays back at the SAME gain the fader was actually at.
    [[nodiscard]] static double automationNormalizedForFaderGain (double linearGain) noexcept;

    // N5: normalized [0,1] breakpoint value for a live pan read, the exact inverse of
    // PanNode::panForNormalizedEvent (-1..1 maps linearly to 0..1).
    [[nodiscard]] static double automationNormalizedForPan (double pan) noexcept;

    // N5: arm a Touch/Latch ride if the mode is armed AND the transport was already rolling when
    // the drag started — moving a control while stopped, even in Touch/Latch mode, is just a
    // normal edit (matches real-DAW semantics: Touch/Latch only writes DURING playback).
    // R15: the ride owner is the selected TRACK or BUS strip (a bus strip's fader/pan ride
    // writes the Bus roles), or an explicit owner (an FX insert's id for param rides); ONLY
    // Touch/Latch arm — Read plays back, Off ignores lanes entirely and writes nothing.
    void beginAutomationTouchRideIfArmed (yesdaw::engine::AutomationTargetRole role,
                                          std::uint32_t paramId,
                                          yesdaw::engine::EntityId ownerOverride = {});

    // N5: sample the live playhead tick and the control's current value into the ride buffer.
    // Deliberately does NOT touch project_/adoptEditedProject — every edit adoption resets the
    // transport to stopped (resetContextForFreshPlayback), so committing per-tick would collapse
    // every point in the ride to tick 0 after the very first write. Buffering client-side and
    // committing once, at the end of the ride, is what makes "breakpoints across a moved span"
    // possible at all.
    void recordAutomationTouchSample (double normalizedValue);

    // N5: commit the whole buffered ride as ONE undo step (the actual project write happens
    // here, and only here — see recordAutomationTouchSample's note on why).
    void endAutomationTouchRideIfActive();

    void refreshAutosaveRecoveryControls();

    void refreshInspectorTakesVisibility();

    void refreshInspectorControls();

    void setInspectorTimeSliderRange (juce::Slider& slider, double maxSeconds);

    [[nodiscard]] std::optional<yesdaw::engine::Tick> inspectorTickFromSeconds (double seconds) const noexcept;

    void setSelectedInspectorStartFromSlider();

    void setSelectedInspectorEndFromSlider();

    void setSelectedInspectorLengthFromSlider();

    void setSelectedInspectorFadesFromSliders();

    void refreshMixerControls();

    // G4.1: the I/O slots' texts — ONE law for the paint and the harness.
    [[nodiscard]] juce::String stripInputText (std::size_t trackIndex) const;

    [[nodiscard]] juce::String stripOutputText (const yesdaw::ui::UiMixerStrip& strip) const;

    [[nodiscard]] static const char* fxKindStripName (yesdaw::engine::FxKind kind) noexcept;

    [[nodiscard]] static const char* fxKindName (yesdaw::engine::FxKind kind) noexcept;

    [[nodiscard]] juce::String masterLoudnessReadoutText() const;

    [[nodiscard]] juce::String exportAudioProgressText() const;

    void drawHeader (juce::Graphics& g) const;

    // V2/V4: the project's HEAD tempo/meter with the shared no-map fallbacks (120 BPM, 4/4) —
    // the ONE read both the transport readout and the ruler's bar-label law consume, so the
    // header and the painted ruler can never disagree about what a bar is.
    struct HeadTempoMeter
    {
        double bpm = 120.0;
        std::uint16_t numerator = 4;
        std::uint16_t denominator = 4;
    };

    [[nodiscard]] HeadTempoMeter headTempoMeter() const;

    // V2: bar|beat at the current playhead — a single-tempo/meter law (the project's head
    // values, matching the existing headBarFrames() family's own scope). Shared by the paint
    // path below and the harness accessor, so a test can never duplicate this formula.
    [[nodiscard]] yesdaw::engine::BarBeat headerBarBeat() const;

    // G1.4: the transport counter shows bars|beats AND minutes:seconds; a click on it swaps which
    // is the big one (Logic's display-mode click).
    struct CounterStrings
    {
        juce::String primary, secondary, mode;
    };

    [[nodiscard]] CounterStrings counterStrings() const;

    void mouseDown (const juce::MouseEvent& event) override;

    void drawTransportReadouts (juce::Graphics& g) const;

public:
    // G0.7 (plan §3.4): the header as a flex row — tools left, transport centred on the window,
    // master card right-anchored against the gear — computed from the window width by ONE law
    // that resized(), the paint, the probe and the harness all read. Nothing here has a fixed x.
    struct HeaderLayout
    {
        juce::Rectangle<int> menuBar, toolsSection, transportSection, masterSection, settingsRow;
        juce::Rectangle<int> newButton, openButton, saveButton, importButton, undoButton, redoButton;
        juce::Rectangle<int> exportButton, exportProgress, exportCancel;
        juce::Rectangle<int> locateStart, play, stop, record, timeReadout, tempoMeterBox, loop;
        juce::Rectangle<int> midiIn;   // G3.10: the MIDI input lamp
        juce::Rectangle<int> masterCard, gear;
        juce::Rectangle<int> bitDepth, range, outputDevice, inputDevice, inputChannel;
        juce::Rectangle<int> arm, monitor, comp;
        bool settingsVisible = false;
    };

    [[nodiscard]] HeaderLayout headerLayout() const;

    // G0.7: the header's height right now — the fixed menu + toolbar, plus the settings row when
    // it is shown. Every work-area layout trims THIS, never the constant.
    [[nodiscard]] int headerHeightNow() const;

    // Harness: show/hide the settings row through the real action (the Options menu's toggle).
    void harnessSetSettingsRowVisible (bool visible);

    // G0.8 harness: dispatch an action the way a menu item or chord would (the test device verb
    // has neither, by design); and read the registry's live state for one.
    KeymapEditorComponent& harnessKeymapEditor() noexcept { return keymapEditor; }

    void harnessDispatchAction (yesdaw::ui::UiActionId action);
    [[nodiscard]] yesdaw::ui::UiActionState harnessActionState (yesdaw::ui::UiActionId action) const;

    // M9: the header's master card — right-anchored against the gear, drops WHOLE (empty rect)
    // when it cannot keep its minimum width next to the centred transport group.
    [[nodiscard]] juce::Rectangle<int> headerMasterCardBounds() const;

    [[nodiscard]] juce::Rectangle<int> headerMasterLufsBounds() const;

private:
    void drawMasterMeter (juce::Graphics& g) const;

    void drawTrackList (juce::Graphics& g, juce::Rectangle<int> area) const;

    yesdaw::ui::TimelineCanvasState makeTimelineState();

    void rebuildTimelineClipViews();

    void selectTimelineClipByLayoutId (int layoutClipId, bool toggle);

    [[nodiscard]] yesdaw::engine::EntityId automationTargetTrackId() const noexcept;

    // E20: the automation lane target — what the canvas edits. R14: a target may be owned by a
    // BUS (fader/pan/send) — its label carries the bus name, and the lane-row header drops the
    // track prefix for it.
    struct AutomationTargetOption
    {
        yesdaw::engine::AutomationTargetRole role = yesdaw::engine::AutomationTargetRole::TrackFader;
        std::uint32_t paramId = 0;
        yesdaw::engine::EntityId ownerEntity {};
        juce::String label;
        bool busOwned = false;
    };

    // E20: enumerate the selected track's automation targets in a stable order — fader, pan,
    // each send level, then each FX param of each insert.
    [[nodiscard]] std::vector<AutomationTargetOption> buildAutomationTargetOptions() const;

    [[nodiscard]] AutomationTargetOption currentAutomationTarget() const;

    [[nodiscard]] double automationCanvasSecondsForLocalX (int localX);

    [[nodiscard]] int automationCanvasLocalXForSeconds (double seconds);

    [[nodiscard]] std::optional<yesdaw::engine::Tick> timelineTickFromSeconds (double seconds) const noexcept;

    void moveTimelineClipByLayoutId (int layoutClipId, double startSeconds, bool snapToGrid);

    // The active snap grid applied to a gesture tick. The gesture's Ctrl flag INVERTS the global
    // grid: grid on -> Ctrl drags fine; grid off -> Ctrl snaps one-shot.
    // G2.7: the grid a drag lands on. Grid mode subdivides the chosen unit while a cell stays at
    // least timelineSnapMinGridPx wide at the current zoom (halving as you zoom in); the other
    // modes use the unit as chosen.
    [[nodiscard]] std::int64_t effectiveSnapGridTicks() const;

    // The Events mode's candidates: every clip edge (except the dragged clip's own), every marker,
    // the playhead and the loop edges; the nearest within the tolerance wins, else no snap.
    [[nodiscard]] std::optional<yesdaw::engine::Tick> snapTickToEvents (yesdaw::engine::Tick tick,
                                                                       std::optional<yesdaw::engine::EntityId> excludeClip) const;

    [[nodiscard]] yesdaw::engine::Tick snappedTimelineTick (yesdaw::engine::Tick tick, bool invertSnap) const;

    // G2.7: ONE snap law for every drop. `origin` is the dragged clip's start before the drag
    // (Relative mode snaps the distance from it); `movingClip` is excluded from the Events.
    [[nodiscard]] yesdaw::engine::Tick snappedTimelineTickFrom (yesdaw::engine::Tick tick, bool invertSnap,
                                                               std::optional<yesdaw::engine::Tick> origin,
                                                               std::optional<yesdaw::engine::EntityId> movingClip) const;

    void moveTimelineClipToLaneByLayoutId (int layoutClipId, int targetLane, double startSeconds, bool snapToGrid);

    void copyTimelineClipByLayoutId (int layoutClipId, int targetLane, double startSeconds, bool snapToGrid);

    // Snap law for edge gestures (E4): the snapped tick goes straight to the verb, whose legality
    // rules (positive length, in-body split, source-window bounds) win by honest refusal.
    void splitTimelineClipByLayoutId (int layoutClipId, double splitSeconds, bool snapInvert = false);

    void trimTimelineClipRightByLayoutId (int layoutClipId, double endSeconds, bool snapInvert = false);

    // G2.11: the slip — the dragged distance, snapped to the effective grid when snap is on (Ctrl is
    // part of the gesture, so it cannot defeat snap here; Snap: Off does), moves the source.
    void slipTimelineClipByLayoutId (int layoutClipId, double deltaSeconds);

    // G2.9b: the Alt-drag on the right edge lands a NEW END; the model turns it into the factor.
    void stretchTimelineClipRightByLayoutId (int layoutClipId, double endSeconds, bool snapInvert = false);

    void adjustTimelineClipGainByLayoutId (int layoutClipId, int deltaPixels);

    void adjustTimelineClipFadeByLayoutId (int layoutClipId, bool fadeIn, double fadeSeconds, double curveDelta = 0.0);

    [[nodiscard]] const yesdaw::engine::Clip* findProjectClipById (yesdaw::engine::EntityId clipId) const noexcept;

    void drawPianoRoll (juce::Graphics& g, juce::Rectangle<int> area) const;

    // V7: the TRACK tab's painted content — the honest track-scoped subset that already exists
    // in the model: name + N7 colour, fader/pan/mute/solo strip state, and the REAL track FX
    // chain (a clip-level FX model does not exist, so the old always-"None" CLIP FX stub is
    // gone; the reference's FX list maps to this real one).
    // V7: the fade-chart card and its inner chart rect — ONE law shared by paint and the
    // harness accessor, so a gate can cross-check the painted curve against the shared
    // clipFadeCurvePoints law without re-deriving the geometry.
    [[nodiscard]] juce::Rectangle<int> inspectorFadeChartCardBounds() const;

    [[nodiscard]] juce::Rectangle<int> inspectorFadeChartBounds() const;

    void drawTrackInspector (juce::Graphics& g, juce::Rectangle<int> area) const;

    // G3.4: the MIDI clip's inspector card — the title and the quantize panel's row labels with
    // the values in force (the controls themselves are children placed by layoutInspectorControls).
    void drawMidiClipInspector (juce::Graphics& g, juce::Rectangle<int> area) const;

    void drawInspector (juce::Graphics& g, juce::Rectangle<int> area) const;

    void drawMixer (juce::Graphics& g, juce::Rectangle<int> area) const;

    [[nodiscard]] yesdaw::ui::UiMixerSurfaceSnapshot currentMixerSurface() const;

    [[nodiscard]] yesdaw::ui::UiPianoRollSurfaceSnapshot currentPianoRollSurface() const;

    yesdaw::ui::YesDawLookAndFeel lookAndFeel;
    yesdaw::ui::UiAppModel appModel;
    yesdaw::ui::MainComponentFileChoices fileChoices;
    juce::AudioDeviceManager audioDeviceManager;
    shell::TooltippedMenuBar menuBar;
    juce::TooltipWindow tooltipWindow { nullptr };   // native tooltip display (B40)
    juce::ComboBox audioDeviceChooser;
    std::vector<std::string> audioDeviceChooserNames;
    // E29: input device chooser + recorded-channel pick.
    juce::ComboBox audioInputDeviceChooser;
    std::vector<std::string> audioInputDeviceChooserNames;
    juce::ComboBox recordingInputChannelChooser;
    std::uint32_t recordingChannelChooserGeneration = 0xFFFFFFFFu;
    bool refreshingAudioDeviceChooser = false;
    const bool desktopAudioRequested = false;
    bool desktopAudioCallbackRegistered = false;
    int desktopAudioCallbackSuspendDepth = 0;
    bool resumeDesktopAudioAfterSuspend = false;
    std::atomic<bool> desktopAudioOpen { false };
    std::atomic<std::uint32_t> deviceAudioCallbackBlockCount { 0u };
    std::atomic<std::uint32_t> deviceAudioNonSilentBlockCount { 0u };
    std::atomic<float> liveMasterPeakLeft { 0.0f };
    std::atomic<float> liveMasterPeakRight { 0.0f };
    std::vector<shell::TrackRow> projectTimelineTracks;
    std::vector<yesdaw::ui::Clip> timelineClips;
    std::vector<yesdaw::ui::TimelineClipNote> timelineClipNotes;   // M7: MIDI clip note previews
    std::vector<shell::TimelineClipStyle> timelineClipStyles;
    std::vector<yesdaw::engine::EntityId> timelineClipIds;
    std::vector<yesdaw::engine::AssetContentHash> timelineClipAssetHashes;
    std::vector<yesdaw::engine::EntityId> timelineClipAssetIds;   // G2.19: the decoded audio for the zoomed-in paint
    double timelineTotalSeconds = yesdaw::ui::UiTheme::Layout::timelineDefaultTotalSeconds;
    std::vector<std::string> timelineMarkerLabels;
    std::vector<yesdaw::ui::TimelineMarker> timelineMarkerViews;
    std::vector<std::string> timelineMapLabelTexts;                // G2.15
    std::vector<yesdaw::ui::TimelineMapLabel> timelineMapLabelViews;
    std::vector<std::int64_t> timelineMapLabelFrames;   // the change's exact frame per label (a click locates there)
    double timelineZoomFactor = 1.0;   // 1.0 == whole timeline fits the window
    double timelineRowZoom = 1.0;      // G2.16: multiplies every auto-height row
    std::optional<ZoomView> lastSelectionZoom;   // G2.16: the view Z produced (Z again goes back)
    bool refreshingZoomSlider = false;
    bool refreshingScrollBars = false;
    FineDragSlider timelineZoomSlider;
    // G2.16: a scroll bar that carries a tooltip like every other identified control.
    struct TooltipScrollBar final : juce::ScrollBar, juce::SettableTooltipClient
    {
        using juce::ScrollBar::ScrollBar;
    };
    TooltipScrollBar timelineHScroll { false };
    TooltipScrollBar timelineVScroll { true };
    mutable double timelineScrollSeconds = yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds;
    // Vertical track scroll (E5): whole lane rows above the viewport, shared by the timeline
    // lanes and the track rail; geometry clamps it against the current lane count.
    int timelineTrackScrollRows = 0;
    // Piano-roll viewport (E10): transient view state; the surface builder is the clamp
    // authority (mutable because paint-side snapshots re-clamp against the current clip).
    mutable int pianoRollViewLowKey = yesdaw::ui::UiThemeLayout::pianoRollDefaultLowKey;
    mutable double pianoRollViewZoom = 1.0;
    mutable yesdaw::engine::Tick pianoRollViewScrollTicks = 0;
    TimelineInputComponent timelineInput;
    PlayheadLayerComponent playheadLayer;   // G0.4: above the buffered canvas
    PianoRollInputComponent pianoRollInput;
    juce::ComboBox pianoRollLaneChooser;   // G3.3: the control lane's chooser (a child over the lane's gutter)
    juce::ComboBox pianoRollKeyChooser;    // G3.8: the roll header's Key chooser (the project's key)
    juce::ComboBox pianoRollScaleChooser;  // G3.8: the roll header's Scale chooser (Off / Major / Minor)
    juce::TextButton pianoRollTypingButton;   // G3.6: the roll header's Typing toggle (Ctrl+K)
    juce::TextButton pianoRollStepButton;     // G3.6: the roll header's Step toggle
    std::map<int, std::int16_t> typedKeyCodes;   // G3.6: key code -> the note it holds
    // G3.5: the MIDI clip's settings rows (the inspector's CLIP tab)
    juce::ToggleButton inspectorMidiMute;
    FineDragSlider inspectorMidiTranspose;
    FineDragSlider inspectorMidiVelocity;
    juce::ComboBox inspectorMidiLoop;
    // G3.4: the quantize panel's controls (the inspector's CLIP tab for a MIDI clip)
    juce::ComboBox inspectorQuantizeGrid;
    FineDragSlider inspectorQuantizeStrength;
    FineDragSlider inspectorQuantizeSwing;
    juce::ToggleButton inspectorQuantizeEnds;
    FineDragSlider inspectorQuantizeHumanize;
    juce::TextButton inspectorQuantizeApply;
    TrackListInputComponent trackListInput;
    MixerStripsInputComponent mixerStripsInput;
    FineDragSlider headerTempoControl;
    juce::ComboBox headerMeterChooser;
    // G4.1 cp2: the FX editor — the lane's parameter widgets live inside it now (their ids unchanged);
    // it shows the SELECTED strip's selectedFxParamSlot while open and closes when that strip or slot goes.
    FxEditorComponent fxEditor;
    bool fxEditorOpen = false;
    int fxEditorStripOrdinal = -1;
    std::array<FineDragSlider, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamSliders;
    std::array<juce::Label, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamLabels;
    std::array<std::uint32_t, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamSliderIds {};
    // E15: choice-shaped params render as real choosers; big param lists page through the pager.
    std::array<juce::ComboBox, yesdaw::ui::UiTheme::Layout::mixerFxParamSliderCount> mixerFxParamChoosers;
    juce::ComboBox mixerFxParamPageChooser;
    int selectedFxParamPage = 0;
    bool lastFxParamPagerVisible = false;
    int selectedFxParamSlot = -1;
    bool refreshingFxParamControls = false;
    // E19: the interactive, undoable master fader on the master pane.
    FineDragSlider mixerMasterFader;
    // E17: the inline bus rename editor (the strip header's double-click).
    juce::TextEditor busRenameEditor;
    int busRenameIndex = -1;

    // M5: transient painted-send drag preview (strip, send, level). Nothing persists until the
    // release commits, so a drag is exactly one undo step.
    struct PaintedSendDragPreview
    {
        int stripIndex = -1;
        int sendIndex = -1;
        float level = 0.0f;
    };
    PaintedSendDragPreview paintedSendDragPreview;
    int paintedFaderDragStrip = -1;   // G4.1 cp2: the strip whose painted fader / pan is mid-drag (the press begins the ride)
    int paintedPanDragStrip = -1;
    std::size_t lastVisibleFxParamRows = 0;
    juce::TextButton trackAddButton;
    juce::TextEditor trackRenameEditor;
    juce::TextEditor clipRenameEditor;
    juce::TextEditor markerRenameEditor;
    int markerRenameIndex = -1;
    int selectedTrackLane = -1;
    std::set<int> selectedTrackLanes;   // G2.17: the multi-selection (empty = just the primary)
    juce::TextButton exportAudioButton;
    juce::ComboBox exportBitDepthChooser;
    juce::ComboBox exportRangeChooser;
    juce::Label exportAudioProgress;
    juce::TextButton exportAudioCancelButton;
    juce::Label dragDbReadout;
    std::vector<MeterHoldState> trackMeterHold;   // by Track index; advanced per UI tick (B32)
    std::vector<std::array<MeterHoldState, 2>> trackMeterHoldLR;   // V5: rail L/R columns
    std::vector<MeterHoldState> busMeterHold;     // by Bus index; same tick law (E22)
    juce::String lastPushedWindowTitle;           // dirty-title push dedupe (B38)
    // (G4.1: the seven readout buttons and the solo-safe button are gone — the strip and its menu.
    //  G4.1 cp2: the lane's live fader / pan / M / S went with the lane — the painted strip is the mixer.)
    juce::TextButton masterLoudnessReadout;
    juce::TextButton autosaveRestoreButton;
    juce::TextButton autosaveDiscardButton;
    juce::ComboBox timelineSnapChooser;
    juce::ComboBox nudgeValueChooser;      // G1.4
    juce::String hoverHint;                // G1.6: the status line's gesture hint
    std::vector<std::pair<juce::Component*, yesdaw::ui::UiActionId>> actionComponents;   // G1.6: live tooltips
    KeymapEditorComponent keymapEditor;    // G1.5
    UndoHistoryComponent undoHistory;      // G2.18
    InstrumentPanelComponent instrumentPanel;   // G3.1
    juce::ComboBox inspectorInstrumentChooser;   // G3.1
    juce::TextButton inspectorInstrumentEdit;
    juce::TextButton inspectorToggle;      // G1.4
    juce::ComboBox editModeChooser;        // G2.6
    bool refreshingEditModeChooser = false;
    juce::ComboBox snapModeChooser;        // G2.7
    bool refreshingSnapModeChooser = false;
    // G2.1: the Arrange window's splitter sizes (persisted per project as view-state.txt).
    struct ViewState
    {
        int railWidth = yesdaw::ui::UiTheme::Layout::leftRailWidth;
        int inspectorWidth = yesdaw::ui::UiTheme::Layout::inspectorWidth;
        int dockHeight = yesdaw::ui::UiTheme::Layout::mixerHeight;
    };
    ViewState viewState;
    std::filesystem::path viewStateBundle;
    std::map<juce::Component*, bool> hiddenByDockTab;   // G2.1 cp2
    yesdaw::ui::SplitterComponent railSplitter { yesdaw::ui::SplitterComponent::Axis::Vertical };
    yesdaw::ui::SplitterComponent inspectorSplitter { yesdaw::ui::SplitterComponent::Axis::Vertical };
    yesdaw::ui::SplitterComponent dockSplitter { yesdaw::ui::SplitterComponent::Axis::Horizontal };
    int timeDisplayMode = 0;               // G1.4: 0 bars|beats primary, 1 min:sec primary
    AutomationLaneCanvasComponent automationLaneCanvas;
    // E20: the automation lane target — what the canvas edits (struct declared with the
    // target helpers earlier in the class).
    std::vector<AutomationTargetOption> automationTargetOptions;
    int selectedAutomationTargetIndex = 0;
    bool refreshingAutomationTarget = false;
    juce::ComboBox automationTargetChooser;
    juce::TextButton automationLaneToggle;
    juce::TextButton mixerDockToggle;
    // V7: the inspector's REAL tab buttons (the painted CLIP/TRACK cells used to be decorative).
    juce::TextButton inspectorClipTab;
    juce::TextButton inspectorTrackTab;
    // V8: the toolbar zoom cluster — stepper buttons bound to the EXISTING zoom actions around
    // a live readout of the one shared timelineZoomFactor.
    juce::TextButton timelineZoomOutButton;
    juce::TextButton timelineZoomInButton;
    juce::Label timelineZoomReadout;
    juce::Label statusLine;
    // R4: audioDeviceError fires on the device thread — it may only flip this flag; the UI
    // timer promotes it to a status message on the message thread.
    std::atomic<bool> deviceErrorPending { false };
    juce::Label automationLaneRow;
    // N5: the client-side Touch/Latch ride buffer — see beginAutomationTouchRideIfArmed().
    juce::ComboBox automationModeChooser;
    bool automationTouchRideActive = false;
    yesdaw::engine::AutomationTargetRole automationTouchRideRole =
        yesdaw::engine::AutomationTargetRole::TrackFader;
    std::uint32_t automationTouchRideParamId = 0;
    yesdaw::engine::EntityId automationTouchRideTrackId;
    std::vector<yesdaw::ui::UiAppModel::AutomationTouchSample> automationTouchRideSamples;
    juce::TextButton automationBreakpointAddButton;
    juce::TextButton automationBreakpointDeleteButton;
    // E26: whether the lane controls were last laid out with the band reserved.
    bool automationLaneLaidOutVisible = false;
    // E33: the inspector take stack — chooser + delete over the TAKES section.
    juce::ComboBox inspectorTakeChooser;
    // G2.14: the inspector's marker list — every marker in tick order; a click locates the playhead.
    struct MarkerListModel final : public juce::ListBoxModel
    {
        std::function<int()> rowCount;
        std::function<juce::String (int)> rowText;
        std::function<void (int)> onRowClicked;
        int getNumRows() override { return rowCount ? rowCount() : 0; }
        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
        {
            if (selected)
            {
                g.setColour (yesdaw::ui::UiTheme::Color::controlInset());
                g.fillRect (0, 0, width, height);
            }
            g.setColour (yesdaw::ui::UiTheme::Color::text());
            g.setFont (yesdaw::ui::UiTheme::Type::font (yesdaw::ui::UiTheme::Type::small));
            g.drawText (rowText ? rowText (row) : juce::String(), yesdaw::ui::UiTheme::Space::sm, 0,
                        width - 2 * yesdaw::ui::UiTheme::Space::sm, height, juce::Justification::centredLeft, true);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            if (onRowClicked)
                onRowClicked (row);
        }
    };
    MarkerListModel inspectorMarkerListModel;
    juce::ListBox inspectorMarkerList;
    juce::TextButton inspectorTakeDelete;
    std::vector<yesdaw::ui::UiClipTakeView> inspectorTakeViews;
    // E34: open MIDI inputs + the message-thread note-on pairing map (note -> frame, velocity).
    std::vector<std::unique_ptr<juce::MidiInput>> midiInputs;
    std::uint32_t midiInSeenLast = 0;                                   // G3.10: the lamp's last seen count
    std::chrono::steady_clock::time_point midiInLitUntil {};            // G3.10: lit until this instant
    std::map<int, std::pair<std::int64_t, float>> pendingMidiNoteOns;
    FineDragSlider inspectorStart;
    FineDragSlider inspectorEnd;
    FineDragSlider inspectorLength;
    FineDragSlider inspectorGain;
    FineDragSlider inspectorStretch;   // G2.9b: percent of the source length
    FineDragSlider inspectorFadeIn;
    FineDragSlider inspectorFadeOut;
    juce::ComboBox inspectorFadeCurve;
    FineDragSlider inspectorFadeCurveAmount;   // G2.10
    std::array<ToolbarActionButton, yesdaw::ui::kMainShellToolbarActions.size()> buttons;
    bool refreshingInspectorControls = false;
    bool refreshingTimeMapControls = false;
    bool refreshingSnapChooser = false;
    bool refreshingNudgeChooser = false;   // G1.4
    bool refreshingMixerControls = false;
    int autosaveElapsedMs = 0;

    // G0.2: the top-level component this shell is registered on as a KeyListener (null in the
    // headless harness, where the shell is its own top level).
    juce::Component* routedTopLevel = nullptr;

    // G0.1 State probe (ADR-0046 §10; plan §7.2). Debug-only: `stateProbePath` is empty in a
    // normal launch and nothing below is ever written. Counters are the feel-budget inputs.
    std::filesystem::path stateProbePath;
    std::uint64_t probeTick = 0;
    std::chrono::steady_clock::time_point launchStamp {};
    std::uint64_t audioCallbackAdds = 0;
    std::uint64_t audioCallbackRemovals = 0;
    std::uint64_t audioSuspendRequests = 0;   // G0.3
    // G0.4: invalidation and refresh counters, and the context the last refresh ran against.
    std::uint64_t fullInvalidations = 0;
    std::uint64_t dynamicInvalidations = 0;
    std::uint64_t actionStateRefreshes = 0;
    yesdaw::ui::UiActionContext lastRefreshedContext {};
    bool lastRefreshedContextValid = false;
    std::atomic<double> deviceSampleRateHz { 0.0 };
    std::atomic<int> deviceXRunBaseline { -1 };
    std::atomic<std::uint32_t> deviceDeadlineMisses { 0u };
    std::atomic<std::uint64_t> deviceMaxCallbackNs { 0u };
    std::string lastActionStableId;
    std::chrono::steady_clock::time_point pendingActionStamp {};
    bool actionStampPending = false;
    double lastActionToPaintMs = -1.0;
    std::chrono::steady_clock::time_point paintStartStamp {};
    double lastPaintMs = 0.0;
    std::array<double, shell::kStateProbePaintRingSize> paintRing {};
    std::size_t paintRingIndex = 0;
    std::size_t paintRingCount = 0;
    std::uint64_t paintCount = 0;
    double lastTickMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace yesdaw::ui
