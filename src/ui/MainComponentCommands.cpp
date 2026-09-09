// YES DAW — the app shell: menus, actions, the keymap, project lifecycle, devices.
//
// Plan §5.1 (the shell topology), checkpoint 2 (2026-09-05): member bodies of MainComponent, carved
// verbatim from the inline class. The declaration is ui/MainComponentShell.h.

#include "ui/MainComponentShell.h"
#include "ui/DesktopAudioStartup.h"
#include <fstream>

using namespace yesdaw::ui::shell;

namespace yesdaw::ui {

void MainComponent::refreshStatusLine()
{
    // G1.6: a status message wins; otherwise the hovered zone's gesture hint.
    statusLine.setText (appModel.statusLineText().empty() ? hoverHintOrModeHint() : juce::String (appModel.statusLineText()),
                        juce::dontSendNotification);
    statusLine.setColour (juce::Label::textColourId,
                          appModel.statusLineIsError()
                              ? yesdaw::ui::UiTheme::Color::dangerRed()
                              : kMutedText);
}

// E34: real MIDI inputs — note on/off pairs collected on the message thread and stamped
// with the capture session's published device-frame cursor; outside a session the model
// refuses them, so this is inert until Record rolls.
void MainComponent::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    if (! message.isNoteOn() && ! message.isNoteOff())
        return;

    const std::int64_t frame = appModel.captureDeviceFrameApprox();
    const bool noteOn = message.isNoteOn();
    const int note = message.getNoteNumber();
    const float velocity = message.getFloatVelocity();
    // G3.10: MIDI thru — straight from this (device) thread into the engine's lane, no message-
    // thread hop; the selected Track's Instrument plays it. The capture path below stays as it was.
    (void) postMidiInputFromDevice (noteOn, note, static_cast<double> (velocity), message.getChannel() - 1);
    juce::Component::SafePointer<MainComponent> safeThis (this);
    juce::MessageManager::callAsync ([safeThis, frame, noteOn, note, velocity] {
        if (safeThis != nullptr)
            safeThis->handleCapturedMidiNote (frame, noteOn, note, velocity);
    });
}

// G3.10: the one entry every played note takes (the device callback and the harness alike).
bool MainComponent::postMidiInputFromDevice (bool noteOn, int note, double velocity, int channel) noexcept
{
    return appModel.postMidiInputNote (noteOn, note, velocity, channel);
}

void MainComponent::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        appModel.setPlaybackMaxBlockSize (device->getCurrentBufferSizeSamples());
        // E28: the model adopts the REAL device profile — actual input count, a stable id
        // hashed from the device name, and the driver-reported latencies — so Record
        // unlocks from real hardware and take provenance records the real device.
        yesdaw::ui::UiRealRecordingDeviceProfile profile;
        const auto nameHash = static_cast<std::uint32_t> (device->getName().hashCode());
        profile.stableDeviceId = nameHash != 0u ? nameHash : 0xFFFFFFFFu;
        profile.sampleRateHz = device->getCurrentSampleRate();
        profile.inputChannels = device->getActiveInputChannels().countNumberOfSetBits();
        profile.maxBlockSize = device->getCurrentBufferSizeSamples();
        profile.inputLatencyFrames = std::max (0, device->getInputLatencyInSamples());
        profile.outputLatencyFrames = std::max (0, device->getOutputLatencyInSamples());
        (void) appModel.adoptRealRecordingDevice (profile);
    }
    desktopAudioOpen.store (device != nullptr, std::memory_order_release);
    // G0.1 probe: the block budget the deadline-miss counter measures against, and the
    // driver's own xrun baseline (-1 when the driver cannot report one).
    deviceSampleRateHz.store (device != nullptr ? device->getCurrentSampleRate() : 0.0,
                              std::memory_order_relaxed);
    deviceXRunBaseline.store (device != nullptr ? device->getXRunCount() : -1,
                              std::memory_order_relaxed);
}

void MainComponent::audioDeviceIOCallbackWithContext (const float* const* inputChannels,
                                       int numInputChannels,
                                       float* const* outputChannels,
                                       int numOutputChannels,
                                       int numFrames,
                                       const juce::AudioIODeviceCallbackContext&)
{
    const auto blockStart = std::chrono::steady_clock::now();
    (void) appModel.processDeviceAudioBlock (
        inputChannels, numInputChannels, outputChannels, numOutputChannels, numFrames);
    accountDeviceBlockPeaks (outputChannels, numOutputChannels, numFrames);

    // G0.1 probe (B5): a block that took longer than the audio it produced is a deadline miss.
    // Atomics only — the device thread never allocates, locks, or logs here.
    const auto elapsed = std::chrono::steady_clock::now() - blockStart;
    const auto elapsedNs = static_cast<std::uint64_t> (
        std::chrono::duration_cast<std::chrono::nanoseconds> (elapsed).count());
    const double rateHz = deviceSampleRateHz.load (std::memory_order_relaxed);
    if (rateHz > 0.0 && numFrames > 0)
    {
        const double budgetNs = 1.0e9 * static_cast<double> (numFrames) / rateHz;
        if (static_cast<double> (elapsedNs) > budgetNs)
            deviceDeadlineMisses.fetch_add (1u, std::memory_order_relaxed);
    }
    std::uint64_t previousMax = deviceMaxCallbackNs.load (std::memory_order_relaxed);
    while (elapsedNs > previousMax
           && ! deviceMaxCallbackNs.compare_exchange_weak (previousMax, elapsedNs,
                                                           std::memory_order_relaxed))
    {
    }
}

void MainComponent::audioDeviceStopped()
{
    desktopAudioOpen.store (false, std::memory_order_release);
}

void MainComponent::audioDeviceError (const juce::String&)
{
    desktopAudioOpen.store (false, std::memory_order_release);
    // R4: device thread — flag only; the UI timer reports it on the message thread.
    deviceErrorPending.store (true, std::memory_order_release);
}

bool MainComponent::processDeviceAudioBlock (float* const* outputChannels,
                                            int numOutputChannels,
                                            int numFrames) noexcept
{
    const bool processed = appModel.processDeviceAudioBlock (
        outputChannels, numOutputChannels, numFrames);
    accountDeviceBlockPeaks (outputChannels, numOutputChannels, numFrames);
    return processed;
}

void MainComponent::accountDeviceBlockPeaks (float* const* outputChannels,
                              int numOutputChannels,
                              int numFrames) noexcept
{
    float peak = 0.0f;
    float leftPeak = 0.0f;
    float rightPeak = 0.0f;
    if (outputChannels != nullptr && numFrames > 0)
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannels[channel] != nullptr)
                for (int frame = 0; frame < numFrames; ++frame)
                {
                    const float samplePeak = std::fabs (outputChannels[channel][frame]);
                    peak = std::max (peak, samplePeak);
                    if (channel == 0)
                        leftPeak = std::max (leftPeak, samplePeak);
                    else if (channel == 1)
                        rightPeak = std::max (rightPeak, samplePeak);
                }
    }

    liveMasterPeakLeft.store (leftPeak, std::memory_order_release);
    liveMasterPeakRight.store (rightPeak, std::memory_order_release);

    deviceAudioCallbackBlockCount.fetch_add (1u, std::memory_order_relaxed);
    if (peak > 0.000001f)
        deviceAudioNonSilentBlockCount.fetch_add (1u, std::memory_order_relaxed);
}

// G3.6: while a keyboard mode is on and nothing is hovered, the status line says so — the mode
// is never silent (a swallowed key would otherwise look like a dead one).
juce::String MainComponent::hoverHintOrModeHint() const
{
    if (! hoverHint.isEmpty())
        return hoverHint;
    const auto& context = appModel.context();
    if (context.musicalTypingOn)
        return "Musical typing ON: A W S E D F T G Y H U J K O L P ; play "
             + juce::String (yesdaw::ui::pianoRollKeyName (context.typingBaseKey)) + " up \u00b7 Z / X octave \u00b7 C / V velocity ("
             + juce::String (context.typingVelocityPercent) + " %)" + (context.stepInputOn ? " \u00b7 step input: notes enter at the playhead" : "") + " \u00b7 Ctrl+K off";
    if (context.stepInputOn)
        return "Step input ON: a typed or clicked note enters at the playhead with the snap length, Right = rest, Left = back";
    return {};
}

void MainComponent::configureAutosaveRecoveryButton (juce::TextButton& button, yesdaw::ui::UiActionId action)
{
    const auto* descriptor = appModel.registry().descriptor (action);
    if (descriptor == nullptr)
        return;

    button.setButtonText (actionButtonText (action));
    button.setComponentID (descriptor->stableId);
    button.setName (descriptor->accessibleName);
    button.setTooltip (juce::String (descriptor->stableId) + "  " + descriptor->defaultKey);
    button.setColour (juce::TextButton::buttonColourId, yesdaw::ui::UiTheme::Color::warningButton());
    button.setColour (juce::TextButton::textColourOffId, kText);
    button.onClick = [this, action] {
        (void) appModel.dispatch (action);
        refreshActionState();
        repaintAll();
    };
    button.setVisible (false);
    addAndMakeVisible (button);
}

// The view state follows the project: a different bundle loads its own record (or the
// defaults); every splitter release writes the record. Polled from refreshActionState so
// every open / new / restore path is covered by the one law.
void MainComponent::loadViewStateIfBundleChanged()
{
    const std::filesystem::path& bundle = appModel.bundlePath();
    if (bundle == viewStateBundle)
        return;
    viewStateBundle = bundle;
    viewState = {};
    appModel.setMixerStripsNarrow (false);   // G4.1: the record's default
    for (const juce::String& line : juce::StringArray::fromLines (juce::String (appModel.readViewStateRecord())))
    {
        const juce::String key = line.upToFirstOccurrenceOf ("\t", false, false);
        const int value = line.fromFirstOccurrenceOf ("\t", false, false).getIntValue();
        if (key == "rail")
            viewState.railWidth = juce::jlimit (yesdaw::ui::UiTheme::Layout::leftRailMinWidth,
                                                yesdaw::ui::UiTheme::Layout::leftRailMaxWidth, value);
        else if (key == "inspector")
            viewState.inspectorWidth = juce::jlimit (yesdaw::ui::UiTheme::Layout::inspectorMinWidth,
                                                     yesdaw::ui::UiTheme::Layout::inspectorMaxWidth, value);
        else if (key == "dock")
            viewState.dockHeight = juce::jmax (yesdaw::ui::UiTheme::Layout::editorDockMinHeight, value);
        else if (key == "narrow")
            appModel.setMixerStripsNarrow (value != 0);   // G4.1
    }
    resized();
}

std::string MainComponent::viewStateRecordText() const
{
    return "rail\t" + std::to_string (viewState.railWidth)
         + "\ninspector\t" + std::to_string (viewState.inspectorWidth)
         + "\ndock\t" + std::to_string (viewState.dockHeight)
         + "\nnarrow\t" + std::string (appModel.context().mixerStripsNarrow ? "1" : "0") + "\n";   // G4.1
}

void MainComponent::saveViewState()
{
    appModel.writeViewStateRecord (viewStateRecordText());
}

// Open a project bundle at a known path (B39): shared by File > Open and Open Recent.
void MainComponent::openProjectBundleAtPath (const std::filesystem::path& path)
{
    StoredProjectAssetsResult stored = decodeStoredProjectAssets (path);
    if (stored.assets && ! stored.assets->empty())
        (void) appModel.loadPreparedProjectBundle (
            std::move (stored.prepared), std::move (*stored.assets));
    else if (stored.assets)
        (void) appModel.openPreparedProjectBundle (std::move (stored.prepared));
    else
        // R5: a project that cannot open says WHY (naming the bad audio file when one is
        // the cause) and refuses to half-open — the shell state stays untouched.
        appModel.reportStatus (
            "Open failed: " + stored.failureReason
                + " (" + path.filename().string() + ")",
            true);
}

void MainComponent::suspendDesktopAudioCallback()
{
    ++audioSuspendRequests;   // G0.3 probe: every request counts, registered or not
    if (desktopAudioCallbackSuspendDepth++ != 0)
        return;

    resumeDesktopAudioAfterSuspend = desktopAudioCallbackRegistered;
    if (desktopAudioCallbackRegistered)
    {
        audioDeviceManager.removeAudioCallback (this);
        ++audioCallbackRemovals;   // G0.1 probe: B3 counts every one of these after startup
        desktopAudioCallbackRegistered = false;
        appModel.setDeviceCallbackLive (false);
    }
}

void MainComponent::resumeDesktopAudioCallback()
{
    if (desktopAudioCallbackSuspendDepth <= 0 || --desktopAudioCallbackSuspendDepth != 0)
        return;

    if (resumeDesktopAudioAfterSuspend && audioDeviceManager.getCurrentAudioDevice() != nullptr)
    {
        audioDeviceManager.addAudioCallback (this);
        ++audioCallbackAdds;
        desktopAudioCallbackRegistered = true;
        appModel.setDeviceCallbackLive (true);
    }
    resumeDesktopAudioAfterSuspend = false;
}

// The keymap's declared chords are live application shortcuts: any KeyPress whose chord matches a
// registered action dispatches through the SAME handleAction path the toolbar uses, so Space plays,
// Ctrl+Z undoes, Del deletes the selected Clip, and every binding stays mechanically listable.
bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && fxEditorOpen)   // G4.1 cp2
    {
        closeFxEditor();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::escapeKey && appModel.context().undoHistoryVisible)   // G2.18
    {
        handleAction (yesdaw::ui::UiActionId::EditShowUndoHistory);
        refreshActionState();
        resized();
        repaintAll();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::escapeKey && appModel.context().keymapVisible)
    {
        handleAction (yesdaw::ui::UiActionId::HelpShowKeymap);
        refreshActionState();
        resized();
        repaintAll();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::escapeKey && cancelInProgressEdit())
        return true;

    // G3.6: musical typing takes its note and control keys BEFORE the keymap (a plain letter,
    // no Ctrl / Alt); everything else — Space, the tools, Ctrl+K itself — still reaches the keymap.
    if (appModel.context().musicalTypingOn
        && ! key.getModifiers().isCtrlDown() && ! key.getModifiers().isCommandDown() && ! key.getModifiers().isAltDown())
    {
        const int code = key.getKeyCode();
        if (code > 32 && code < 127)
        {
            const auto press = appModel.musicalTypingPress (static_cast<char> (code));
            if (press.handled)
            {
                if (press.key >= 0)
                    typedKeyCodes[code] = press.key;
                refreshActionState();
                repaintAll();
                return true;
            }
        }
    }
    // G3.6: with step input on, Right is a rest and Left steps back (the roll's note walk waits).
    if (appModel.context().stepInputOn && key.getModifiers().isAnyModifierKeyDown() == false)
    {
        if (key.getKeyCode() == juce::KeyPress::rightKey) { (void) appModel.stepInputRest(); refreshActionState(); repaintAll(); return true; }
        if (key.getKeyCode() == juce::KeyPress::leftKey)  { (void) appModel.stepInputBack(); refreshActionState(); repaintAll(); return true; }
    }

    const std::string chord = chordForKeyPress (key);
    if (chord.empty())
        return false;

    // G1.1: the chord is looked up in the CURRENT Focus context (the editor that has focus),
    // then Global — never in another editor's bindings.
    const yesdaw::ui::UiActionId action = appModel.registry().keymap().actionForChord (
        chord, yesdaw::ui::focusContextForPanel (appModel.context().activePanel));
    if (action == yesdaw::ui::UiActionId::Count)
        return false;

    handleAction (action);
    refreshActionState();
    repaintAll();
    return true;
}

// G0.2 Command router (ADR-0046 §4). Only an active text field consumes keys: every other
// widget declines keyboard focus, so a click on a button, combo, or slider hands focus to
// THIS component (JUCE walks a click's focus grab up to the first ancestor that wants it)
// and the next chord dispatches through keyPressed above. The KeyListener half below catches
// the one remaining hole: right after launch (and whenever focus falls back to the window
// itself) the DocumentWindow, not the shell, is the focused component — its key listeners
// run before its own keyPressed, so the chord still reaches the router.
void MainComponent::applyKeyboardFocusLaw()
{
    applyKeyboardFocusLawTo (*this);
}

void MainComponent::applyKeyboardFocusLawTo (juce::Component& parent)
{
    for (int i = 0; i < parent.getNumChildComponents(); ++i)
    {
        juce::Component* child = parent.getChildComponent (i);
        if (child == nullptr)
            continue;
        if (dynamic_cast<juce::TextEditor*> (child) == nullptr)
            child->setWantsKeyboardFocus (false);
        applyKeyboardFocusLawTo (*child);
    }
}

// KeyListener half of the router: chords that reach the top-level window (focus on the
// window itself, or on a child that did not consume them) dispatch exactly as if this
// component were focused. A text editor that originated the event keeps its keys.
bool MainComponent::keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent)
{
    if (originatingComponent == this
        || dynamic_cast<juce::TextEditor*> (originatingComponent) != nullptr)
        return false;
    return keyPressed (key);
}

// Declared alongside the KeyListener overload so neither hides the other (Clang's
// -Woverloaded-virtual is an error here); the Component half keeps its default behaviour.
bool MainComponent::keyStateChanged (bool isKeyDown)
{
    // G3.6: a typed note holds while its key is down and releases when it lifts (JUCE reports
    // the state change, not the key: every held code is checked).
    for (auto it = typedKeyCodes.begin(); it != typedKeyCodes.end();)
    {
        if (! juce::KeyPress::isKeyCurrentlyDown (it->first))
        {
            appModel.musicalTypingRelease (it->second);
            it = typedKeyCodes.erase (it);
        }
        else
            ++it;
    }
    return juce::Component::keyStateChanged (isKeyDown);
}

// G3.3: a mouse gesture that lands on a model verb without passing handleAction still names
// itself to the probe (a drive asserts on lastAction; nothing is blind).
void MainComponent::recordLastAction (yesdaw::ui::UiActionId action)
{
    if (const auto* descriptor = appModel.registry().descriptor (action))
        lastActionStableId = descriptor->stableId;
}

void MainComponent::handleAction (yesdaw::ui::UiActionId action)
{
    // G0.1 probe: the last dispatched action by stable id, and the stamp the B1
    // action-to-paint budget is measured from (closed by the next completed paint).
    if (const auto* descriptor = appModel.registry().descriptor (action))
        lastActionStableId = descriptor->stableId;
    pendingActionStamp = std::chrono::steady_clock::now();
    actionStampPending = true;

    // G0.3 (ADR-0046 §6): no suspend/resume bracket — the device callback is never removed by
    // a UI action. Every branch below rides the atomic engine publish + retire law, the
    // transport command queue, or the live scalar lane. The only legitimate suspends left are
    // the device choosers (device (re)open).
    handleActionWhileAudioStopped (action);

    // G1.5: Alt+K shows / hides the keymap editor over the arrangement.
    if (action == yesdaw::ui::UiActionId::HelpShowKeymap)
    {
        keymapEditor.setVisible (appModel.context().keymapVisible);
        if (appModel.context().keymapVisible)
        {
            keymapEditor.refreshRows();
            keymapEditor.toFront (false);
        }
    }
    if (action == yesdaw::ui::UiActionId::EditShowUndoHistory)   // G2.18
    {
        undoHistory.setVisible (appModel.context().undoHistoryVisible);
        if (appModel.context().undoHistoryVisible)
        {
            undoHistory.refreshRows();
            undoHistory.toFront (false);
        }
    }
    else if (undoHistory.isVisible())
        undoHistory.refreshRows();   // an edit while the window shows: the rows follow

    // G0.7 / G1.4: the settings row and the inspector change the layout — the whole shell
    // re-lays out.
    if (action == yesdaw::ui::UiActionId::ViewToggleSettingsRow
        || action == yesdaw::ui::UiActionId::ViewToggleInspector
        || action == yesdaw::ui::UiActionId::HelpShowKeymap
        || action == yesdaw::ui::UiActionId::EditShowUndoHistory)
    {
        resized();
        repaintAll();
    }
}

// Real menu bar (usable-DAW P1): the painted FILE/EDIT/VIEW text is gone; a juce::MenuBarComponent
// over the same header spot dispatches registered actions through the SAME handleAction path the
// toolbar and keymap use. The model is mechanically testable without opening popups.
// G1.2 (plan §3, Logic's order): File · Edit · Track · Clip · MIDI · View · Transport ·
// Options · Help. Every item paints the chord that fires it in the CURRENT Focus context.
juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Track", "Clip", "MIDI", "View", "Transport", "Options", "Help" };
}

std::span<const yesdaw::ui::UiActionId> MainComponent::menuActionsForIndex (int topLevelMenuIndex)
{
    using yesdaw::ui::UiActionId;
    static constexpr std::array<UiActionId, 10> kFileMenu {
        UiActionId::ProjectNew,        UiActionId::ProjectOpen,        UiActionId::ProjectSave,
        UiActionId::ProjectSaveAs,     UiActionId::ProjectImportAudio, UiActionId::ProjectExportAudio,
        UiActionId::ProjectImportMidi, UiActionId::ProjectExportMidi,   // G3.7
        UiActionId::ProjectExportDawproject, UiActionId::ProjectExportAudioCancel,
    };
    static constexpr std::array<UiActionId, 33> kEditMenu {
        UiActionId::EditUndo,          UiActionId::EditRedo,           UiActionId::EditShowUndoHistory,   // G2.18
        UiActionId::TimelineClipCut,
        UiActionId::TimelineClipCopy,  UiActionId::TimelineClipPaste,  UiActionId::TimelineClipDuplicate,
        UiActionId::TimelineClipRepeatPaste, UiActionId::TimelineClipDelete,
        UiActionId::TimelineRangeCut,  UiActionId::TimelineRangeCopy,  UiActionId::TimelineRangeDelete,
        UiActionId::TimelineRangeSilence, UiActionId::TimelineRangeSplitEdges, UiActionId::TimelineSelectAllFollowing,
        UiActionId::EditModeOverlap,   UiActionId::EditModeNoOverlap,  UiActionId::EditModeShuffle,
        UiActionId::TimelineClipSelectAllProject, UiActionId::TimelineClipSelectAllTrack,
        UiActionId::EditRenameSelection,
        UiActionId::EditNudgeLeft,     UiActionId::EditNudgeRight,
        UiActionId::EditNudgeLeftFine, UiActionId::EditNudgeRightFine,
        UiActionId::EditNudgeValueGrid, UiActionId::EditNudgeValueBar,
        UiActionId::EditNudgeValueBeat, UiActionId::EditNudgeValueSixteenth,
        UiActionId::EditNudgeValueMs1, UiActionId::EditNudgeValueMs10,          // G2.8
        UiActionId::EditNudgeValueFrame, UiActionId::EditNudgeValueSample,
    };
    static constexpr std::array<UiActionId, 12> kTrackMenu {
        UiActionId::TrackAdd,          UiActionId::TrackDuplicate,     UiActionId::TrackRemove,
        UiActionId::TrackRename,       UiActionId::TrackMoveUp,        UiActionId::TrackMoveDown,
        UiActionId::TrackSelectPrevious, UiActionId::TrackSelectNext,
        UiActionId::TrackToggleMute,   UiActionId::TrackToggleSolo,    UiActionId::TrackToggleArm,
        UiActionId::MixerTrackSetOutput,
    };
    static constexpr std::array<UiActionId, 18> kClipMenu {
        UiActionId::TimelineClipSplit, UiActionId::TimelineClipHeal,
        UiActionId::TimelineClipApplyDefaultFades, UiActionId::TimelineClipSetFades,
        UiActionId::TimelineClipCrossfade, UiActionId::TimelineClipSetGain,
        UiActionId::TimelineClipGainIncrease, UiActionId::TimelineClipGainDecrease,
        UiActionId::TimelineClipMove,  UiActionId::TimelineClipTrim,
        UiActionId::TimelineClipTimeStretch, UiActionId::TimelineClipStretchToLoop,   // G2.9b
        UiActionId::TimelineClipToggleMute, UiActionId::TimelineClipColourNext,      // G2.12
        UiActionId::TimelineClipReverse, UiActionId::TimelineClipNormalize, UiActionId::TimelineClipStripSilence,   // G2.13
        UiActionId::TimelineMidiClipAdd,
    };
    static constexpr std::array<UiActionId, 14> kMidiMenu {
        UiActionId::PianoRollNoteAdd,  UiActionId::PianoRollNoteDelete, UiActionId::PianoRollNoteSelectAll,
        UiActionId::PianoRollNoteQuantizeSelection, UiActionId::PianoRollNoteTranspose,
        UiActionId::PianoRollMusicalTypingToggle, UiActionId::PianoRollStepInputToggle,   // G3.6
        UiActionId::PianoRollNoteOctaveUp, UiActionId::PianoRollNoteOctaveDown,
        UiActionId::PianoRollNoteDuplicate, UiActionId::PianoRollNoteSetLength,
        UiActionId::PianoRollNoteSetVelocity,
        UiActionId::PianoRollNoteSelectPrevious, UiActionId::PianoRollNoteSelectNext,   // G3.2
    };
    static constexpr std::array<UiActionId, 32> kViewMenu {
        UiActionId::ViewTimeline,      UiActionId::ViewMixer,          UiActionId::ViewPianoRoll,
        UiActionId::ViewInstrument,   // G3.1
        UiActionId::ViewToggleInspector,
        UiActionId::TimelineToggleMixerDock, UiActionId::MixerStripsNarrowToggle,   // G4.1
        UiActionId::InspectorShowClipTab, UiActionId::InspectorShowTrackTab,
        UiActionId::TimelineAutomationToggleTrackLane,
        UiActionId::TimelineZoomIn,    UiActionId::TimelineZoomOut,
        UiActionId::TimelineZoomTracksIn, UiActionId::TimelineZoomTracksOut, UiActionId::TimelineZoomBack,   // G2.16
        UiActionId::TimelinePlayheadFollowContinuous,
        UiActionId::TimelineZoomFitProject, UiActionId::TimelineZoomFitLoop, UiActionId::TimelineZoomToSelection,
        UiActionId::TimelineTogglePlayheadFollow,
        UiActionId::TimelineToolSelectPointer, UiActionId::TimelineToolSelectPencil,
        UiActionId::TimelineToolSelectScissors, UiActionId::TimelineToolSelectHand,
        UiActionId::TimelineToolSelectZoom, UiActionId::TimelineToolSelectEraser, UiActionId::TimelineToolSelectVelocity,   // G3.2
        UiActionId::ViewToggleSettingsRow,
        UiActionId::TimelineSnapModeGrid, UiActionId::TimelineSnapModeRelative,
        UiActionId::TimelineSnapModeEvents, UiActionId::TimelineSnapModeOff,
    };
    static constexpr std::array<UiActionId, 24> kTransportMenu {
        UiActionId::TransportTogglePlayStop, UiActionId::TransportPlay, UiActionId::TransportStop,
        UiActionId::TransportPlayFromLastLocate, UiActionId::TransportRecord,
        UiActionId::TransportReturnToZero, UiActionId::TransportLocateStart,
        UiActionId::TransportLocatePreviousBar, UiActionId::TransportLocateNextBar,
        UiActionId::TransportLocatePreviousGrid, UiActionId::TransportLocateNextGrid,
        UiActionId::TransportLocatePreviousMarker, UiActionId::TransportLocateNextMarker,
        UiActionId::TimelineMarkerAdd, UiActionId::TimelineMarkerRemove,
        UiActionId::TransportToggleLoop, UiActionId::TimelineRangeToLoop,
        UiActionId::TransportToggleMetronome, UiActionId::TransportToggleRecordCountIn,
        UiActionId::TransportToggleReturnToStartOnStop,
        UiActionId::TransportSetTempo, UiActionId::TransportSetMeter,
        UiActionId::TransportShuttleFaster, UiActionId::TransportShuttleSlower,
    };
    static constexpr std::array<UiActionId, 6> kOptionsMenu {
        UiActionId::TimelineSnapDisable,      UiActionId::TimelineSnapSetBar,
        UiActionId::TimelineSnapSetBeat,      UiActionId::TimelineSnapSetSixteenth,
        UiActionId::MixerTargetToggleSoloSafe,
        UiActionId::DeviceRefreshAudio,   // G0.8: Options ▸ Refresh Device (no toolbar button)
    };
    static constexpr std::array<UiActionId, 1> kHelpMenu { UiActionId::HelpShowKeymap };

    switch (topLevelMenuIndex)
    {
        case 0: return kFileMenu;
        case 1: return kEditMenu;
        case 2: return kTrackMenu;
        case 3: return kClipMenu;
        case 4: return kMidiMenu;
        case 5: return kViewMenu;
        case 6: return kTransportMenu;
        case 7: return kOptionsMenu;
        case 8: return kHelpMenu;
        default: return {};
    }
}

// G1.2: the tick a menu item shows — the registry context's own flags, one law for every
// toggle and every "which one is current" group (views, inspector tabs, snap presets).
bool MainComponent::menuTickState (yesdaw::ui::UiActionId action) const noexcept
{
    using yesdaw::ui::UiActionId;
    const yesdaw::ui::UiActionContext& c = appModel.context();
    switch (action)
    {
        case UiActionId::TransportToggleLoop:               return c.loopEnabled;
        case UiActionId::TransportToggleMetronome:          return c.metronomeEnabled;
        case UiActionId::TimelineTogglePlayheadFollow:      return c.playheadFollowEnabled;
        case UiActionId::TransportToggleReturnToStartOnStop: return c.returnToStartOnStopEnabled;
        case UiActionId::TransportToggleRecordCountIn:      return c.recordCountInEnabled;
        case UiActionId::ViewToggleSettingsRow:             return c.settingsRowVisible;
        case UiActionId::MixerStripsNarrowToggle:           return c.mixerStripsNarrow;   // G4.1
        case UiActionId::ViewToggleInspector:               return c.inspectorVisible;
        case UiActionId::EditNudgeValueGrid:                return c.nudgeValue == 0;
        case UiActionId::EditNudgeValueBar:                 return c.nudgeValue == 1;
        case UiActionId::EditNudgeValueBeat:                return c.nudgeValue == 2;
        case UiActionId::EditNudgeValueSixteenth:           return c.nudgeValue == 3;
        case UiActionId::EditNudgeValueMs1:                 return c.nudgeValue == 4;   // G2.8
        case UiActionId::EditNudgeValueMs10:                return c.nudgeValue == 5;
        case UiActionId::EditNudgeValueFrame:               return c.nudgeValue == 6;
        case UiActionId::EditNudgeValueSample:              return c.nudgeValue == 7;
        case UiActionId::TimelineToggleMixerDock:           return c.mixerDockVisible;
        case UiActionId::EditModeOverlap:                   return c.editMode == yesdaw::ui::UiEditMode::Overlap;    // G2.6
        case UiActionId::EditModeNoOverlap:                 return c.editMode == yesdaw::ui::UiEditMode::NoOverlap;
        case UiActionId::EditModeShuffle:                   return c.editMode == yesdaw::ui::UiEditMode::Shuffle;
        case UiActionId::TimelineSnapModeGrid:              return c.snapMode == yesdaw::ui::UiSnapMode::Grid;      // G2.7
        case UiActionId::TimelineSnapModeRelative:          return c.snapMode == yesdaw::ui::UiSnapMode::Relative;
        case UiActionId::TimelineSnapModeEvents:            return c.snapMode == yesdaw::ui::UiSnapMode::Events;
        case UiActionId::TimelineSnapModeOff:               return c.snapMode == yesdaw::ui::UiSnapMode::Off;
        case UiActionId::TimelineClipToggleMute:            return c.timelineClipMuted;   // G2.12
        case UiActionId::TimelineClipReverse:               return c.timelineClipReversed;   // G2.13
        case UiActionId::TimelineTempoChangeToggleRamp:     return c.tempoChangeAtPlayheadRamps;   // G2.15
        case UiActionId::TimelinePlayheadFollowContinuous:  return c.playheadFollowContinuous;    // G2.16
        case UiActionId::TimelineAutomationToggleTrackLane: return c.timelineAutomationTrackLaneVisible;
        case UiActionId::ViewTimeline:                      return c.activePanel == yesdaw::ui::UiPanel::Timeline;
        case UiActionId::ViewMixer:                         return c.mixerDockVisible && c.editorDockTab == yesdaw::ui::UiEditorDockTab::Mixer;
        case UiActionId::ViewPianoRoll:                     return c.mixerDockVisible && c.editorDockTab == yesdaw::ui::UiEditorDockTab::PianoRoll;
        case UiActionId::ViewInstrument:                    return c.mixerDockVisible && c.editorDockTab == yesdaw::ui::UiEditorDockTab::Instrument;   // G3.1
        case UiActionId::InspectorShowClipTab:              return ! c.inspectorTrackTabActive;
        case UiActionId::InspectorShowTrackTab:             return c.inspectorTrackTabActive;
        case UiActionId::TimelineSnapDisable:               return ! c.snapEnabled;
        case UiActionId::TimelineSnapSetBar:                return c.snapEnabled && c.snapGridTicks == 2048;
        case UiActionId::TimelineSnapSetBeat:               return c.snapEnabled && c.snapGridTicks == 512;
        case UiActionId::TimelineSnapSetSixteenth:          return c.snapEnabled && c.snapGridTicks == 128;
        case UiActionId::TimelineToolSelectPointer:         return c.activeTimelineTool == yesdaw::ui::TimelineTool::Pointer;
        case UiActionId::TimelineToolSelectPencil:          return c.activeTimelineTool == yesdaw::ui::TimelineTool::Pencil;
        case UiActionId::TimelineToolSelectScissors:        return c.activeTimelineTool == yesdaw::ui::TimelineTool::Scissors;
        case UiActionId::TimelineToolSelectEraser:          return c.activeTimelineTool == yesdaw::ui::TimelineTool::Eraser;     // G3.2
        case UiActionId::TimelineToolSelectVelocity:        return c.activeTimelineTool == yesdaw::ui::TimelineTool::Velocity;
        case UiActionId::TimelineToolSelectHand:            return c.activeTimelineTool == yesdaw::ui::TimelineTool::Hand;
        case UiActionId::TimelineToolSelectZoom:            return c.activeTimelineTool == yesdaw::ui::TimelineTool::Zoom;
        default:                                            return false;
    }
}

void MainComponent::openContextMenu (yesdaw::ui::ContextMenuTarget target, int index, juce::Component& source,
                      juce::Point<int> sourcePosition)
{
    lastContextMenu = {};
    lastContextMenu.shown = true;
    lastContextMenu.target = target;
    lastContextMenu.index = index;
    // The click's selection is part of the context the items are built from.
    refreshActionState();
    hideMixerControlsBehindDockTab();   // G3.2 checkpoint FIX 1
    juce::PopupMenu menu;
    // G1.7: an EMPTY insert slot offers exactly one thing — Add Insert (the kinds) — instead
    // of four disabled verbs; a strip's Add Insert is the same submenu, so a right-click
    // reaches every effect without the dock's chooser.
    const bool emptySlot = target == yesdaw::ui::ContextMenuTarget::InsertSlot
                        && ! (index >= 0 && static_cast<std::size_t> (index) < appModel.selectedStripFxChain().size());
    // G4.1 cp2: an EMPTY send well offers Add Send alone (the buses inline); a routed row its menu.
    const std::vector<yesdaw::engine::SendRow> sendRows = appModel.selectedTrackSends();
    const bool sendRowTarget = target == yesdaw::ui::ContextMenuTarget::MixerSendRow;
    const bool emptySendWell = sendRowTarget
                            && ! (index >= 0 && static_cast<std::size_t> (index) < sendRows.size());
    std::vector<yesdaw::ui::ContextMenuEntry> entries;
    if (emptySlot)
        entries.push_back ({ yesdaw::ui::UiActionId::MixerFxInsertAdd });
    else if (emptySendWell)
        entries.push_back ({ yesdaw::ui::UiActionId::MixerSendAdd });
    else
        for (const yesdaw::ui::ContextMenuEntry& entry : yesdaw::ui::contextMenuEntries (target))
            entries.push_back (entry);
    for (const yesdaw::ui::ContextMenuEntry& entry : entries)
    {
        if (entry.separatorBefore)
            menu.addSeparator();
        if (entry.action == yesdaw::ui::UiActionId::MixerFxInsertAdd)
        {
            // G4.1: the kinds THIS strip takes (a Bus / the master: the audio kinds only). G4.1 cp2: the
            // slot's own click lists the kinds inline (Logic's plug-in menu); the strip menu keeps its submenu.
            juce::PopupMenu kinds;
            for (const yesdaw::engine::FxKind kind : appModel.fxKindsForSelectedStrip())
            {
                kinds.addItem (kContextMenuAddInsertBase + static_cast<int> (kind), fxKindName (kind));
                lastContextMenu.addInsertKinds.push_back (static_cast<int> (kind));
            }
            if (target == yesdaw::ui::ContextMenuTarget::InsertSlot)
            {
                menu.addSectionHeader ("Add Insert");
                juce::PopupMenu::MenuItemIterator it (kinds);
                while (it.next())
                    menu.addItem (juce::PopupMenu::Item (it.getItem()));
            }
            else
                menu.addSubMenu ("Add Insert", kinds,
                                 appModel.registry().stateFor (entry.action, appModel.context()).enabled);
            lastContextMenu.actions.push_back (entry.action);
            continue;
        }
        // G4.1: the routing verbs carry their choices — a submenu on the strip menu, the items
        // themselves on the I/O slot's menu (one verb, expanded inline).
        if (entry.action == yesdaw::ui::UiActionId::MixerSendAdd
            || entry.action == yesdaw::ui::UiActionId::MixerTrackSetOutput
            || entry.action == yesdaw::ui::UiActionId::MixerTrackSetInput)
        {
            const bool inline_ = target == yesdaw::ui::ContextMenuTarget::MixerStripInput
                              || target == yesdaw::ui::ContextMenuTarget::MixerStripOutput
                              || emptySendWell;   // G4.1 cp2: the well's click lists the buses
            const bool enabled = appModel.registry().stateFor (entry.action, appModel.context()).enabled;
            juce::PopupMenu choices;
            int choiceCount = 0;
            const auto& project = appModel.project();
            const yesdaw::engine::EntityId self = appModel.selectedSendOwnerEntityId();
            if (entry.action == yesdaw::ui::UiActionId::MixerSendAdd)
            {
                for (std::size_t busIndex = 0; busIndex < project.buses.size() && busIndex < kContextMenuChoiceRange; ++busIndex)
                {
                    if (project.buses[busIndex].id == self)
                        continue;   // R13: never a self-route
                    choices.addItem (kContextMenuAddSendBase + static_cast<int> (busIndex),
                                     juce::String (project.buses[busIndex].strip.name));
                    ++choiceCount;
                }
            }
            else if (entry.action == yesdaw::ui::UiActionId::MixerTrackSetOutput)
            {
                const yesdaw::engine::EntityId routed = appModel.selectedTrackOutputBusId();
                juce::PopupMenu::Item master ("Master");
                master.itemID = kContextMenuOutputBase;
                master.isTicked = ! routed.isValid();
                choices.addItem (std::move (master));
                ++choiceCount;
                for (std::size_t busIndex = 0; busIndex < project.buses.size() && busIndex + 1 < kContextMenuChoiceRange; ++busIndex)
                {
                    if (project.buses[busIndex].id == self)
                        continue;
                    juce::PopupMenu::Item item (juce::String (project.buses[busIndex].strip.name));
                    item.itemID = kContextMenuOutputBase + 1 + static_cast<int> (busIndex);
                    item.isTicked = project.buses[busIndex].id == routed;
                    choices.addItem (std::move (item));
                    ++choiceCount;
                }
            }
            else
            {
                const auto& device = appModel.recordingDeviceSelection();
                const int inputs = device.selected ? static_cast<int> (device.inputChannels) : 0;
                const yesdaw::ui::UiRecordingTrackInputSelection* picked = nullptr;
                const int ordinal = appModel.selectedMixerStripOrdinal();
                for (const yesdaw::ui::UiRecordingTrackInputSelection& armed : appModel.armedRecordingTrackInputs())
                    if (armed.armed && ordinal >= 0 && armed.trackIndex == static_cast<std::size_t> (ordinal))
                        picked = &armed;
                for (int channel = 0; channel < inputs && channel < static_cast<int> (kContextMenuChoiceRange); ++channel)
                {
                    juce::PopupMenu::Item item ("In " + juce::String (channel + 1));
                    item.itemID = kContextMenuInputMonoBase + channel;
                    item.isTicked = picked != nullptr && ! picked->stereoPair && picked->inputChannel == channel;
                    choices.addItem (std::move (item));
                    ++choiceCount;
                }
                for (int channel = 0; channel + 1 < inputs && channel < static_cast<int> (kContextMenuChoiceRange); ++channel)
                {
                    juce::PopupMenu::Item item ("In " + juce::String (channel + 1) + "+" + juce::String (channel + 2));
                    item.itemID = kContextMenuInputPairBase + channel;
                    item.isTicked = picked != nullptr && picked->stereoPair && picked->inputChannel == channel;
                    choices.addItem (std::move (item));
                    ++choiceCount;
                }
                if (choiceCount == 0)
                {
                    juce::PopupMenu::Item none ("No inputs (Options > Audio Device)");
                    none.isEnabled = false;
                    choices.addItem (std::move (none));
                }
            }
            if (inline_)
            {
                menu.addSectionHeader (entry.action == yesdaw::ui::UiActionId::MixerTrackSetInput ? "Input"
                                       : entry.action == yesdaw::ui::UiActionId::MixerSendAdd ? "Send to" : "Output");
                if (entry.action == yesdaw::ui::UiActionId::MixerSendAdd && choiceCount == 0)
                {
                    juce::PopupMenu::Item none ("No buses (Add Bus on the strip menu)");
                    none.isEnabled = false;
                    choices.addItem (std::move (none));
                }
                juce::PopupMenu::MenuItemIterator it (choices);
                while (it.next())
                    menu.addItem (juce::PopupMenu::Item (it.getItem()));
            }
            else
            {
                menu.addSubMenu (entry.action == yesdaw::ui::UiActionId::MixerSendAdd ? "Add Send"
                                 : entry.action == yesdaw::ui::UiActionId::MixerTrackSetOutput ? "Output" : "Input",
                                 choices, enabled && choiceCount > 0);
            }
            lastContextMenu.actions.push_back (entry.action);
            continue;
        }
        const auto& descriptor = yesdaw::ui::uiActionDescriptors()[static_cast<std::size_t> (entry.action)];
        // G4.1 cp2: the routed send row's verbs act on THAT row — the tap ticks when pre-fader, the
        // destination is a submenu of the buses (the current one ticked, never the owner itself).
        if (sendRowTarget && ! emptySendWell)
        {
            const yesdaw::engine::SendRow& send = sendRows[static_cast<std::size_t> (index)];
            const bool enabled = appModel.registry().stateFor (entry.action, appModel.context()).enabled;
            if (entry.action == yesdaw::ui::UiActionId::MixerSendSetTap)
            {
                juce::PopupMenu::Item item ("Pre-fader");
                item.itemID = static_cast<int> (entry.action) + 1;
                item.isEnabled = enabled;
                item.isTicked = send.tap == yesdaw::engine::SendTap::PreFader;
                menu.addItem (std::move (item));
                lastContextMenu.actions.push_back (entry.action);
                continue;
            }
            if (entry.action == yesdaw::ui::UiActionId::MixerSendSetDestination)
            {
                juce::PopupMenu destinations;
                int destinationCount = 0;
                const auto& project = appModel.project();
                const yesdaw::engine::EntityId self = appModel.selectedSendOwnerEntityId();
                for (std::size_t busIndex = 0; busIndex < project.buses.size() && busIndex < kContextMenuChoiceRange; ++busIndex)
                {
                    if (project.buses[busIndex].id == self)
                        continue;   // R13: never a self-route
                    juce::PopupMenu::Item item (juce::String (project.buses[busIndex].strip.name));
                    item.itemID = kContextMenuSendDestBase + static_cast<int> (busIndex);
                    item.isTicked = project.buses[busIndex].id == send.busId;
                    destinations.addItem (std::move (item));
                    ++destinationCount;
                }
                menu.addSubMenu ("Destination", destinations, enabled && destinationCount > 0);
                lastContextMenu.actions.push_back (entry.action);
                continue;
            }
            juce::PopupMenu::Item item (juce::String (descriptor.label));
            item.itemID = static_cast<int> (entry.action) + 1;
            item.isEnabled = enabled;
            menu.addItem (std::move (item));
            lastContextMenu.actions.push_back (entry.action);
            continue;
        }
        const bool slotVerb = target == yesdaw::ui::ContextMenuTarget::InsertSlot;
        const bool slotEnabled = slotVerb && index >= 0
                              && static_cast<std::size_t> (index) < appModel.selectedStripFxChain().size();
        if (slotVerb && entry.action == yesdaw::ui::UiActionId::MixerFxInsertReorder)
        {
            // §3.3: Move Up · Move Down — one reorder verb, two directions.
            juce::PopupMenu::Item up ("Move Up");
            up.itemID = kContextMenuMoveUpId;
            up.isEnabled = slotEnabled && index > 0;
            menu.addItem (std::move (up));
            juce::PopupMenu::Item down ("Move Down");
            down.itemID = kContextMenuMoveDownId;
            down.isEnabled = slotEnabled && static_cast<std::size_t> (index + 1) < appModel.selectedStripFxChain().size();
            menu.addItem (std::move (down));
            lastContextMenu.actions.push_back (entry.action);
            continue;
        }
        juce::PopupMenu::Item item (slotVerb && entry.action == yesdaw::ui::UiActionId::MixerFxInsertParamSet
                                        ? juce::String ("Open Editor")
                                        : juce::String (descriptor.label));
        item.itemID = static_cast<int> (entry.action) + 1;
        item.isEnabled = slotVerb ? slotEnabled
                                  : appModel.registry().stateFor (entry.action, appModel.context()).enabled;
        item.isTicked = slotVerb && entry.action == yesdaw::ui::UiActionId::MixerFxInsertToggle && slotEnabled
                            ? ! appModel.selectedStripFxChain()[static_cast<std::size_t> (index)].enabled
                            : menuTickState (entry.action);
        item.shortcutKeyDescription = slotVerb ? juce::String() : menuShortcutFor (entry.action);
        menu.addItem (std::move (item));
        lastContextMenu.actions.push_back (entry.action);
    }
    if (target == yesdaw::ui::ContextMenuTarget::Ruler)
    {
        // G2.2: the time row's format — Min:Sec / SMPTE / Samples — one app-wide setting the
        // header counter shares (ids above the action range, like Add Insert).
        juce::PopupMenu formats;
        for (const auto& [mode, label] : std::array<std::pair<int, const char*>, 3> {
                 std::pair { yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplayMinSec, "Min:Sec" },
                 std::pair { yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySmpte, "SMPTE" },
                 std::pair { yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySamples, "Samples" } })
        {
            juce::PopupMenu::Item item (label);
            item.itemID = kContextMenuTimeDisplayBase + mode;
            item.isTicked = mode == yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplayMinSec
                                ? timeDisplayMode <= yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplayMinSec
                                : timeDisplayMode == mode;
            formats.addItem (std::move (item));
        }
        menu.addSubMenu ("Time Display", formats);
    }
    repaintAll();
    if (getPeer() == nullptr)
        return;   // headless: the record is the menu
    const juce::Point<int> screenPoint = source.localPointToGlobal (sourcePosition);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea (juce::Rectangle<int> (screenPoint.x, screenPoint.y, 1, 1))
                            .withParentComponent (nullptr),
                        [this] (int itemId) { invokeContextMenuItem (itemId); });
}

// The one path a picked context-menu item takes (the popup's callback and the harness): the
// insert-slot verbs act on the clicked slot through the shell's per-slot handlers; every
// other target dispatches the action.
void MainComponent::invokeContextMenuItem (int itemId)
{
    if (itemId > kContextMenuTimeDisplayBase && itemId <= kContextMenuTimeDisplayBase + yesdaw::ui::timeline_canvas_detail::kRulerTimeDisplaySamples)
    {
        timeDisplayMode = itemId - kContextMenuTimeDisplayBase;   // G2.2
        refreshActionState();
        repaintAll();
        return;
    }
    if (itemId >= kContextMenuAddInsertBase && itemId < kContextMenuAddInsertBase + kContextMenuAddInsertKindCount)
    {
        (void) appModel.addFxInsertToSelectedStrip (
            static_cast<yesdaw::engine::FxKind> (itemId - kContextMenuAddInsertBase));
        refreshActionState();
        resized();
        repaintAll();
        return;
    }
    // G4.1: the routing choices act on the SELECTED strip (the click that opened the menu selected it).
    const auto inChoiceRange = [itemId] (int base) {
        return itemId >= base && itemId < base + static_cast<int> (kContextMenuChoiceRange);
    };
    if (inChoiceRange (kContextMenuAddSendBase) || inChoiceRange (kContextMenuOutputBase)
        || inChoiceRange (kContextMenuInputMonoBase) || inChoiceRange (kContextMenuInputPairBase)
        || inChoiceRange (kContextMenuSendDestBase))
    {
        if (inChoiceRange (kContextMenuAddSendBase))
        {
            (void) appModel.addSendOnSelectedTrack (static_cast<std::size_t> (itemId - kContextMenuAddSendBase));
        }
        else if (inChoiceRange (kContextMenuSendDestBase))
        {
            // G4.1 cp2: the send row's Destination — re-routes THAT row (one undo group).
            if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::MixerSendRow && lastContextMenu.index >= 0)
                (void) appModel.setSendDestinationOnSelectedTrack (static_cast<std::size_t> (lastContextMenu.index),
                                                                  static_cast<std::size_t> (itemId - kContextMenuSendDestBase));
        }
        else if (inChoiceRange (kContextMenuOutputBase))
        {
            const int choice = itemId - kContextMenuOutputBase;
            const auto& buses = appModel.project().buses;
            if (choice == 0)
                (void) appModel.setOutputOnSelectedTrack ({});
            else if (static_cast<std::size_t> (choice - 1) < buses.size())
                (void) appModel.setOutputOnSelectedTrack (buses[static_cast<std::size_t> (choice - 1)].id);
        }
        else
        {
            const bool stereo = inChoiceRange (kContextMenuInputPairBase);
            const int channel = itemId - (stereo ? kContextMenuInputPairBase : kContextMenuInputMonoBase);
            const int ordinal = appModel.selectedMixerStripOrdinal();
            if (ordinal >= 0 && static_cast<std::size_t> (ordinal) < appModel.project().tracks.size())
                (void) appModel.setRecordingInputForTrack (static_cast<std::size_t> (ordinal),
                                                           static_cast<std::uint16_t> (channel), stereo);
        }
        refreshActionState();
        resized();
        repaintAll();
        return;
    }
    if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::InsertSlot)
    {
        const int slot = lastContextMenu.index;
        if (slot < 0)
            return;
        const auto slotIndex = static_cast<std::size_t> (slot);
        if (itemId == kContextMenuMoveUpId)
            (void) appModel.moveFxInsertOnSelectedStrip (slotIndex, -1);
        else if (itemId == kContextMenuMoveDownId)
            (void) appModel.moveFxInsertOnSelectedStrip (slotIndex, 1);
        else if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerFxInsertToggle) + 1)
            (void) appModel.toggleFxInsertEnabledOnSelectedStrip (slotIndex);
        else if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerFxInsertRemove) + 1)
            (void) appModel.removeFxInsertFromSelectedStrip (slotIndex);
        else if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerFxInsertParamSet) + 1)
        {
            // G4.1 cp2: Open Editor — the slot's editor over the arrangement.
            selectedFxParamSlot = slot;
            selectedFxParamPage = 0;
            fxEditorStripOrdinal = appModel.selectedMixerStripOrdinal();
            fxEditorOpen = true;
        }
        else
            return;
        refreshActionState();
        resized();
        repaintAll();
        return;
    }
    // G4.1 cp2: the routed send row's verbs act on THAT row.
    if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::MixerSendRow)
    {
        const int row = lastContextMenu.index;
        if (row < 0)
            return;
        if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerSendSetTap) + 1)
            (void) appModel.toggleSendTapOnSelectedTrack (static_cast<std::size_t> (row));
        else if (itemId == static_cast<int> (yesdaw::ui::UiActionId::MixerSendRemove) + 1)
            (void) appModel.removeSendOnSelectedTrack (static_cast<std::size_t> (row));
        else
            return;
        refreshActionState();
        resized();
        repaintAll();
        return;
    }
    if (itemId <= 0 || itemId > static_cast<int> (yesdaw::ui::kUiActionCount))
        return;
    // G2.14: a marker menu pick acts on the marker under the pointer (the canvas lists markers in
    // project order, so the menu's index IS the project index).
    if (lastContextMenu.target == yesdaw::ui::ContextMenuTarget::Marker
        && itemId == static_cast<int> (yesdaw::ui::UiActionId::TimelineMarkerColourNext) + 1)
    {
        const int index = lastContextMenu.index;
        if (index >= 0 && index < static_cast<int> (appModel.project().markers.size()))
            (void) appModel.cycleMarkerColour (appModel.project().markers[static_cast<std::size_t> (index)].id);
        refreshActionState();
        repaintAll();
        return;
    }
    handleAction (static_cast<yesdaw::ui::UiActionId> (itemId - 1));
    refreshActionState();
    resized();
    repaintAll();
}

// G1.6: the hovered zone's gesture hint; the status line shows it while no message is active.
void MainComponent::setHoverHint (const juce::String& hint)
{
    if (hoverHint == hint)
        return;
    hoverHint = hint;
    if (appModel.statusLineText().empty())
        statusLine.setText (hoverHintOrModeHint(), juce::dontSendNotification);
}

// G1.6: every action-backed control's tooltip quotes its LIVE chord (a rebind in the keymap
// editor changes the tooltip), from the registry's descriptor name and the keymap.
void MainComponent::refreshActionTooltips()
{
    const auto& keymap = appModel.registry().keymap();
    for (const auto& [component, action] : actionComponents)
    {
        auto* client = dynamic_cast<juce::SettableTooltipClient*> (component);
        const auto* descriptor = appModel.registry().descriptor (action);
        if (client == nullptr || descriptor == nullptr)
            continue;
        const std::string& chord = keymap.chordFor (action);
        client->setTooltip (chord.empty() ? juce::String (descriptor->accessibleName)
                                          : juce::String (descriptor->accessibleName) + "  (" + chord + ")");
    }
    const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        const auto* descriptor = appModel.registry().descriptor (toolbarActions[i]);
        if (descriptor == nullptr)
            continue;
        const std::string& chord = keymap.chordFor (toolbarActions[i]);
        buttons[i].setTooltip (juce::String (descriptor->stableId) + (chord.empty() ? juce::String() : "  " + juce::String (chord)));
    }
}

// G1.2: the chord a menu item paints — the one that fires in the CURRENT Focus context
// (its own binding or a Global one); another editor's binding paints nothing.
juce::String MainComponent::menuShortcutFor (yesdaw::ui::UiActionId action) const
{
    const std::string& chord = appModel.registry().keymap().chordFor (action);
    if (chord.empty())
        return {};
    const yesdaw::ui::UiFocusContext context = yesdaw::ui::defaultFocusContext (action);
    const yesdaw::ui::UiFocusContext focus = yesdaw::ui::focusContextForPanel (appModel.context().activePanel);
    if (context != yesdaw::ui::UiFocusContext::Global && context != focus)
        return {};
    return juce::String (chord);
}

juce::PopupMenu MainComponent::getMenuForIndex (int topLevelMenuIndex, const juce::String&)
{
    juce::PopupMenu menu;
    for (const yesdaw::ui::UiActionId action : menuActionsForIndex (topLevelMenuIndex))
    {
        const auto& descriptor =
            yesdaw::ui::uiActionDescriptors()[static_cast<std::size_t> (action)];
        juce::PopupMenu::Item item (descriptor.label);
        item.itemID = static_cast<int> (action) + 1;
        item.isEnabled = appModel.registry().stateFor (action, appModel.context()).enabled;
        item.isTicked = menuTickState (action);
        item.shortcutKeyDescription = menuShortcutFor (action);
        menu.addItem (std::move (item));
    }

    // G1.7: the repeat-paste count (Ctrl+R) is a ticked Edit ▸ Repeat Count submenu — the
    // toolbar "2x" combo the sweep flagged is gone (the reference toolbar has no such thing).
    if (topLevelMenuIndex == 1)
    {
        juce::PopupMenu counts;
        for (const int count : { 2, 3, 4, 8 })
        {
            juce::PopupMenu::Item item (juce::String (count) + juce::String::charToString (0xd7));
            item.itemID = kRepeatCountMenuBaseId + count;
            item.isTicked = appModel.repeatPasteCount() == count;
            counts.addItem (std::move (item));
        }
        menu.addSubMenu ("Repeat Count", counts);
    }

    // Transport ▸ Locate Points: the five store / recall pairs. Fully implemented verbs that
    // had no chord (plan §4 assigns none — Logic has no default), no menu entry and no
    // button, so nothing but a test could reach them until 2026-09-04. Same item law as
    // the flat entries: the action's id, its live enabled state, its (empty) chord.
    if (topLevelMenuIndex == 6)
    {
        juce::PopupMenu locatePoints;
        for (const yesdaw::ui::UiActionId action : kLocatePointMenu)
        {
            const auto& descriptor =
                yesdaw::ui::uiActionDescriptors()[static_cast<std::size_t> (action)];
            juce::PopupMenu::Item item (descriptor.label);
            item.itemID = static_cast<int> (action) + 1;
            item.isEnabled = appModel.registry().stateFor (action, appModel.context()).enabled;
            item.shortcutKeyDescription = menuShortcutFor (action);
            if (action == yesdaw::ui::UiActionId::TransportRecallLocatePoint1)
                locatePoints.addSeparator();
            locatePoints.addItem (std::move (item));
        }
        menu.addSubMenu ("Locate Points", locatePoints);
    }

    // Open Recent (B39): the File menu lists the MRU bundles, most recent first, on item ids
    // above the action range.
    if (topLevelMenuIndex == 0)
    {
        juce::PopupMenu recent;
        const std::vector<std::filesystem::path> recents = appModel.recentProjectBundles();
        for (std::size_t i = 0; i < recents.size(); ++i)
            recent.addItem (kRecentMenuBaseId + static_cast<int> (i),
                            juce::String (recents[i].stem().string()));
        menu.addSubMenu ("Open Recent", recent, ! recents.empty());
    }

    return menu;
}

void MainComponent::menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/)
{
    if (menuItemID > kRepeatCountMenuBaseId && menuItemID <= kRepeatCountMenuBaseId + 8)
    {
        appModel.setRepeatPasteCount (menuItemID - kRepeatCountMenuBaseId);
        refreshActionState();
        repaintAll();
        return;
    }
    if (menuItemID >= kRecentMenuBaseId
        && menuItemID < kRecentMenuBaseId + static_cast<int> (yesdaw::ui::UiAppModel::kRecentProjectsLimit))
    {
        const std::vector<std::filesystem::path> recents = appModel.recentProjectBundles();
        const std::size_t index = static_cast<std::size_t> (menuItemID - kRecentMenuBaseId);
        if (index < recents.size())
            openProjectBundleAtPath (recents[index]);
        refreshActionState();
        repaintAll();
        return;
    }

    if (menuItemID <= 0 || menuItemID > static_cast<int> (yesdaw::ui::kUiActionCount))
        return;

    handleAction (static_cast<yesdaw::ui::UiActionId> (menuItemID - 1));
    refreshActionState();
    repaintAll();
}

// Device chooser plumbing (usable-DAW P1): harness seams win when injected; the native shell
// talks to the JUCE device manager.
bool MainComponent::selectAudioOutputDeviceByName (const std::string& name)
{
    if (fileChoices.selectAudioOutputDevice)
        return fileChoices.selectAudioOutputDevice (name);

    if (! desktopAudioRequested)
        return false;

    juce::AudioDeviceManager::AudioDeviceSetup setup = audioDeviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = juce::String (name);
    return audioDeviceManager.setAudioDeviceSetup (setup, true).isEmpty();
}

bool MainComponent::selectAudioInputDeviceByName (const std::string& name)
{
    if (fileChoices.selectAudioInputDevice)
        return fileChoices.selectAudioInputDevice (name);

    if (! desktopAudioRequested)
        return false;

    juce::AudioDeviceManager::AudioDeviceSetup setup = audioDeviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = juce::String (name);
    setup.useDefaultInputChannels = true;
    return audioDeviceManager.setAudioDeviceSetup (setup, true).isEmpty();
}

void MainComponent::refreshAudioDeviceChooser()
{
    refreshingAudioDeviceChooser = true;
    AudioDeviceNames nativeNames;
    if (desktopAudioRequested
        && (! fileChoices.listAudioOutputDevices || ! fileChoices.listAudioInputDevices))
        nativeNames = enumerateDesktopAudioDeviceNames (audioDeviceManager);
    audioDeviceChooserNames = fileChoices.listAudioOutputDevices
                                  ? fileChoices.listAudioOutputDevices() : std::move (nativeNames.outputs);
    audioDeviceChooser.clear (juce::dontSendNotification);

    juce::String current;
    if (juce::AudioIODevice* device = audioDeviceManager.getCurrentAudioDevice())
        current = device->getName();

    for (std::size_t index = 0; index < audioDeviceChooserNames.size(); ++index)
    {
        audioDeviceChooser.addItem (juce::String (audioDeviceChooserNames[index]),
                                    static_cast<int> (index) + 1);
        if (current.isNotEmpty() && current == juce::String (audioDeviceChooserNames[index]))
            audioDeviceChooser.setSelectedId (static_cast<int> (index) + 1, juce::dontSendNotification);
    }

    audioDeviceChooser.setEnabled (! audioDeviceChooserNames.empty());

    // E29: rebuild the input device list the same way...
    audioInputDeviceChooserNames = fileChoices.listAudioInputDevices
                                       ? fileChoices.listAudioInputDevices() : std::move (nativeNames.inputs);
    audioInputDeviceChooser.clear (juce::dontSendNotification);
    juce::String currentInput;
    if (desktopAudioRequested)
        currentInput = audioDeviceManager.getAudioDeviceSetup().inputDeviceName;
    for (std::size_t index = 0; index < audioInputDeviceChooserNames.size(); ++index)
    {
        audioInputDeviceChooser.addItem (juce::String (audioInputDeviceChooserNames[index]),
                                         static_cast<int> (index) + 1);
        if (currentInput.isNotEmpty()
            && currentInput == juce::String (audioInputDeviceChooserNames[index]))
            audioInputDeviceChooser.setSelectedId (static_cast<int> (index) + 1,
                                                   juce::dontSendNotification);
    }
    audioInputDeviceChooser.setEnabled (! audioInputDeviceChooserNames.empty());

    // ...and the channel pick from the ADOPTED device's real input count: mono "In N" for
    // each channel, "In N+M" for each adjacent stereo pair.
    refreshRecordingInputChannelChooser();
    refreshingAudioDeviceChooser = false;
}

// E29: options track the adopted profile's generation so a device change re-lists them.
void MainComponent::refreshRecordingInputChannelChooser()
{
    const auto& device = appModel.recordingDeviceSelection();
    recordingInputChannelChooser.clear (juce::dontSendNotification);
    for (int channel = 0; channel < static_cast<int> (device.inputChannels); ++channel)
        recordingInputChannelChooser.addItem ("In " + juce::String (channel + 1), channel + 1);
    for (int channel = 0; channel + 1 < static_cast<int> (device.inputChannels); ++channel)
        recordingInputChannelChooser.addItem (
            "In " + juce::String (channel + 1) + "+" + juce::String (channel + 2),
            1001 + channel);
    const auto& context = appModel.context();
    if (device.inputChannels > 0u)
    {
        const int pickBase = std::max (0, context.selectedRecordingInputChannel);
        recordingInputChannelChooser.setSelectedId (
            context.selectedRecordingInputStereoPair ? 1001 + pickBase : pickBase + 1,
            juce::dontSendNotification);
    }
    recordingInputChannelChooser.setEnabled (device.selected && device.inputChannels > 0u);
}

bool MainComponent::isUnnamedLaunchProject() const
{
    const auto& path = appModel.bundlePath();
    if (path.filename() != unnamedBundleName)
        return false;
    // Outside the bundle: Save As copies project content, never its unnamed-session intent.
    std::ifstream marker (path.parent_path() / unnamedMarkerName);
    std::string leaf;
    return std::getline (marker, leaf) && leaf == unnamedBundleName;
}

bool MainComponent::saveCurrentProject (bool chooseDestination)
{
    if (! chooseDestination && ! isUnnamedLaunchProject())
        return appModel.dispatch (UiActionId::ProjectSave).dispatched;
    if (! fileChoices.chooseSaveAsProjectBundle)
        return false;
    const auto path = fileChoices.chooseSaveAsProjectBundle();
    if (path.empty())
        return false;
    const auto saved = appModel.saveProjectBundleAs (path);
    if (! saved.dispatched)
        appModel.reportStatus (std::string ("Save As failed: ") + saved.state.disabledReason, true);
    return saved.dispatched;
}

void MainComponent::handleActionWhileAudioStopped (yesdaw::ui::UiActionId action)
{
    switch (action)
    {
        case yesdaw::ui::UiActionId::ProjectNew:
            if (fileChoices.chooseNewProjectBundle)
            {
                const std::filesystem::path path = fileChoices.chooseNewProjectBundle();
                if (! path.empty())
                {
                    // R4: a failed create paints its reason instead of vanishing.
                    const yesdaw::persistence::BundleResult created =
                        fileChoices.makeNewProject
                            ? appModel.createProjectBundle (path, fileChoices.makeNewProject())
                            : appModel.createProjectBundle (path);
                    if (! created.ok())
                        appModel.reportStatus ("New project failed: " + created.message, true);
                }
            }
            return;

        case yesdaw::ui::UiActionId::ProjectOpen:
            if (fileChoices.chooseOpenProjectBundle)
            {
                const std::filesystem::path path = fileChoices.chooseOpenProjectBundle();
                if (! path.empty())
                    openProjectBundleAtPath (path);
            }
            return;

        case yesdaw::ui::UiActionId::ProjectExportAudio:
            if (fileChoices.chooseExportAudioFile)
            {
                const std::filesystem::path path = fileChoices.chooseExportAudioFile();
                if (! path.empty())
                    (void) appModel.exportAudioFile (path);
            }
            return;

        // G3.7: a MIDI file lands on the SELECTED track (else the first) at the playhead; the
        // export takes the selection, else the whole project. The model names every refusal.
        case yesdaw::ui::UiActionId::ProjectImportMidi:
            if (fileChoices.chooseImportMidiFile)
            {
                const std::filesystem::path path = fileChoices.chooseImportMidiFile();
                const auto& tracks = appModel.project().tracks;
                if (! path.empty() && ! tracks.empty())
                {
                    const std::size_t lane = selectedTrackLane >= 0 && selectedTrackLane < static_cast<int> (tracks.size())
                        ? static_cast<std::size_t> (selectedTrackLane) : std::size_t {};
                    (void) appModel.importMidiFileAt (
                        path, tracks[lane].id,
                        static_cast<yesdaw::engine::Tick> (std::max<std::int64_t> (0, appModel.context().playheadFrame)));
                }
            }
            return;

        case yesdaw::ui::UiActionId::ProjectExportMidi:
            if (fileChoices.chooseExportMidiFile)
            {
                const std::filesystem::path path = fileChoices.chooseExportMidiFile();
                if (! path.empty())
                    (void) appModel.exportMidiFile (path);
            }
            return;

        case yesdaw::ui::UiActionId::ProjectImportAudio:
            if (fileChoices.chooseImportAudioFile)
            {
                const std::filesystem::path path = fileChoices.chooseImportAudioFile();
                if (! path.empty())
                {
                    if (auto decoded = decodeProjectWav (path))
                    {
                        // Import lands on the SELECTED Track when the rail has a selection.
                        const auto& tracks = appModel.project().tracks;
                        // R7: verb failures report their precise reason inside the model.
                        if (selectedTrackLane >= 0
                            && selectedTrackLane < static_cast<int> (tracks.size()))
                            (void) appModel.importAudioFileToTrack (
                                path, std::move (*decoded),
                                tracks[static_cast<std::size_t> (selectedTrackLane)].id);
                        else
                            (void) appModel.importAudioFile (path, std::move (*decoded));
                    }
                    else
                    {
                        // R6: a file the WAV reader refuses is named, never swallowed.
                        appModel.reportStatus (
                            "Import refused (WAV only, stereo max): "
                                + path.filename().string(),
                            true);
                    }
                }
            }
            return;

        case yesdaw::ui::UiActionId::ProjectSave:
        case yesdaw::ui::UiActionId::ProjectSaveAs:
            (void) saveCurrentProject (action == UiActionId::ProjectSaveAs);
            return;

        case yesdaw::ui::UiActionId::TransportRecord:
        {
            // Real capture when the desktop device has live inputs (P0-1); the deterministic
            // synthetic-take path remains for the injected-choices harness and inputless devices.
            if (appModel.realRecordingCaptureActive())
            {
                (void) appModel.stopRealRecordingCaptureAndCommit();
                return;
            }

            juce::AudioIODevice* const device = audioDeviceManager.getCurrentAudioDevice();
            const int activeInputs = device != nullptr
                ? device->getActiveInputChannels().countNumberOfSetBits()
                : 0;
            if (desktopAudioCallbackRegistered && device != nullptr && activeInputs > 0)
            {
                const bool armed = appModel.context().recordingTrackArmed
                                && appModel.context().recordingInputSelected;
                if (! armed)
                    (void) appModel.dispatch (yesdaw::ui::UiActionId::RecordingArmTrack);

                (void) appModel.startRealRecordingCapture (
                    activeInputs,
                    device->getCurrentSampleRate(),
                    static_cast<std::int64_t> (device->getInputLatencyInSamples()),
                    static_cast<std::int64_t> (device->getOutputLatencyInSamples()));
                return;
            }

            (void) appModel.dispatch (action);
            return;
        }

        case yesdaw::ui::UiActionId::DeviceRefreshAudio:
            refreshAudioDeviceChooser();
            (void) appModel.dispatch (action);
            return;

        case yesdaw::ui::UiActionId::TrackRename:
            if (selectedTrackLane >= 0)
                openTrackRenameEditor();
            return;

        case yesdaw::ui::UiActionId::EditRenameSelection:
            if (appModel.context().timelineClipSelected)
                openClipRenameEditor();
            else if (selectedTrackLane >= 0)
                openTrackRenameEditor();
            return;

        case yesdaw::ui::UiActionId::TrackRemove:
            removeSelectedTrack();
            return;

        case yesdaw::ui::UiActionId::TrackDuplicate:
            duplicateSelectedTrack();
            return;

        case yesdaw::ui::UiActionId::TrackMoveUp:
            moveSelectedTrack (-1);
            return;

        case yesdaw::ui::UiActionId::TrackMoveDown:
            moveSelectedTrack (1);
            return;

        case yesdaw::ui::UiActionId::TrackToggleMute:
        case yesdaw::ui::UiActionId::TrackToggleSolo:
        case yesdaw::ui::UiActionId::TrackToggleArm:
            toggleSelectedTrackKey (action);
            return;

        // G4.1: the strip menus' target verbs act on the SELECTED mixer target (a Bus's menu used
        // to reach the rail's track through the Track verbs); the bus verbs need the shell too.
        case yesdaw::ui::UiActionId::MixerTargetToggleMute:
            (void) appModel.toggleSelectedMixerMute();
            return;
        case yesdaw::ui::UiActionId::MixerTargetToggleSolo:
            (void) appModel.toggleSelectedMixerSolo();
            return;
        case yesdaw::ui::UiActionId::MixerTargetToggleSoloSafe:
            (void) appModel.toggleSelectedMixerSoloSafe();
            return;
        case yesdaw::ui::UiActionId::MixerBusRemove:
            (void) appModel.removeSelectedBus();
            layoutMixerControls();
            return;
        case yesdaw::ui::UiActionId::MixerBusRename:
            if (appModel.selectedMixerTargetIsBus())
            {
                const int ordinal = appModel.selectedMixerStripOrdinal();
                openBusRenameEditor (ordinal - static_cast<int> (appModel.project().tracks.size()), ordinal);
            }
            return;
        case yesdaw::ui::UiActionId::MixerStripsNarrowToggle:
            (void) appModel.dispatch (action);
            saveViewState();   // the view state follows the project, like the splitters
            resized();
            return;

        case yesdaw::ui::UiActionId::TrackSelectPrevious:
        case yesdaw::ui::UiActionId::TrackSelectNext:
            // Context-sensitive (B34): in the Piano Roll with a note selected, Up/Down
            // transpose the selection by one semitone; elsewhere they walk the track rail.
            if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll
                && appModel.context().midiNoteSelected)
            {
                (void) appModel.transposeSelectedPianoRollNotes (
                    action == yesdaw::ui::UiActionId::TrackSelectPrevious ? 1 : -1);
                return;
            }
            selectAdjacentTrackLane (action);
            return;

        case yesdaw::ui::UiActionId::TimelineClipSelectAllTrack:
            // Context-sensitive (B34): in the Piano Roll, Ctrl+A selects every note in the
            // selected MIDI clip; elsewhere it keeps selecting the track's clips.
            if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll
                && appModel.context().midiClipSelected)
            {
                (void) appModel.selectAllPianoRollNotes();
                return;
            }
            (void) appModel.dispatch (action);
            return;

        case yesdaw::ui::UiActionId::TimelineClipDelete:
            // Context-sensitive (B34): in the Piano Roll with a note selection, Del deletes
            // the selected notes; elsewhere it keeps deleting timeline clips.
            if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll
                && appModel.context().midiNoteSelected)
            {
                (void) appModel.deleteSelectedPianoRollNotes();
                return;
            }
            // G2.5: with a Time selection and no clip selected, Del clears the range.
            if (appModel.context().timelineRangeSelected && ! appModel.context().timelineClipSelected
                && appModel.context().activePanel != yesdaw::ui::UiPanel::PianoRoll)
            {
                (void) appModel.dispatch (yesdaw::ui::UiActionId::TimelineRangeDelete);
                return;
            }
            (void) appModel.dispatch (action);
            return;

        case yesdaw::ui::UiActionId::TimelineClipDuplicate:
            // Context-sensitive (B35): in the Piano Roll with a note selected, Ctrl+D lands a
            // fresh copy one grid step later; elsewhere it keeps duplicating timeline clips.
            if (appModel.context().activePanel == yesdaw::ui::UiPanel::PianoRoll
                && appModel.context().midiNoteSelected)
            {
                (void) appModel.duplicateSelectedPianoRollNote (0);   // G3.2 checkpoint: right after the note
                return;
            }
            (void) appModel.dispatch (action);
            return;

        case yesdaw::ui::UiActionId::PianoRollNoteDuplicate:
            (void) appModel.duplicateSelectedPianoRollNote (0);   // G3.2 checkpoint: right after the note
            return;


        case yesdaw::ui::UiActionId::TimelineClipSplit:
            (void) appModel.splitSelectedTimelineClipAt (
                static_cast<yesdaw::engine::Tick> (
                    std::max<std::int64_t> (0, appModel.context().playheadFrame)));
            return;

        case yesdaw::ui::UiActionId::TimelineZoomFitProject:
            if (appModel.dispatch (action).dispatched)
            {
                timelineZoomFactor = yesdaw::ui::UiTheme::Layout::timelineZoomMin;
                timelineScrollSeconds = yesdaw::ui::UiTheme::Layout::timelineViewportScrollSeconds;
                refreshTimelineZoomReadout();
            }
            return;

        case yesdaw::ui::UiActionId::TimelineZoomFitLoop:
            if (appModel.dispatch (action).dispatched && appModel.project().sampleRate.isValid())
            {
                const std::int64_t loopStart = appModel.playbackLoopStartFrame();
                const std::int64_t loopEnd = appModel.playbackLoopEndFrame();
                if (loopStart >= 0 && loopEnd > loopStart)
                {
                    const double sampleRateHz = appModel.project().sampleRate.hz;
                    const double loopDurationSeconds = static_cast<double> (loopEnd - loopStart)
                                                     / sampleRateHz;
                    timelineZoomFactor = std::clamp (
                        std::max (yesdaw::ui::UiTheme::Layout::timelineMinVisibleSeconds,
                                  timelineTotalSeconds)
                            / loopDurationSeconds,
                        yesdaw::ui::UiTheme::Layout::timelineZoomMin,
                        timelineZoomCeiling());
                    timelineScrollSeconds = static_cast<double> (loopStart) / sampleRateHz;
                    refreshTimelineZoomReadout();
                }
            }
            return;

        case yesdaw::ui::UiActionId::TimelineZoomIn:
            if (appModel.dispatch (action).dispatched && appModel.project().sampleRate.isValid())
            {
                const double playheadSeconds = static_cast<double> (
                    std::max<std::int64_t> (0, appModel.context().playheadFrame))
                                             / appModel.project().sampleRate.hz;
                zoomTimelineAtAnchor (
                    playheadSeconds, yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep);
            }
            return;

        case yesdaw::ui::UiActionId::TimelineZoomOut:
            if (appModel.dispatch (action).dispatched && appModel.project().sampleRate.isValid())
            {
                const double playheadSeconds = static_cast<double> (
                    std::max<std::int64_t> (0, appModel.context().playheadFrame))
                                             / appModel.project().sampleRate.hz;
                zoomTimelineAtAnchor (
                    playheadSeconds, 1.0 / yesdaw::ui::UiTheme::Layout::timelineZoomWheelStep);
            }
            return;

        case yesdaw::ui::UiActionId::TimelineZoomTracksIn:   // G2.16
            if (appModel.dispatch (action).dispatched)
                zoomTracksBy (yesdaw::ui::UiTheme::Layout::timelineRowZoomStep);
            return;
        case yesdaw::ui::UiActionId::TimelineZoomTracksOut:
            if (appModel.dispatch (action).dispatched)
                zoomTracksBy (1.0 / yesdaw::ui::UiTheme::Layout::timelineRowZoomStep);
            return;
        case yesdaw::ui::UiActionId::TimelineZoomBack:
            if (appModel.dispatch (action).dispatched)
                (void) popZoomHistory();
            return;
        case yesdaw::ui::UiActionId::TimelineZoomToSelection:
        {
            // G2.5 (R22): fit the Time selection with a small margin — the zoom law is the
            // view's (fit × factor), so the factor and scroll are set here, then clamped.
            // G2.16: Z toggles — when the view already IS the selection's, go back instead.
            if (lastSelectionZoom && lastSelectionZoom->zoom == timelineZoomFactor
                && lastSelectionZoom->scroll == timelineScrollSeconds && popZoomHistory())
            {
                lastSelectionZoom.reset();
                (void) appModel.dispatch (action);
                return;
            }
            const MainComponentSnapshotLike view = snapshotForZoom();
            if (view.rangeSeconds > 0.0 && view.fitPixelsPerSecond > 0.0)
            {
                pushZoomHistory();
                const double margin = view.rangeSeconds * yesdaw::ui::UiTheme::Layout::timelineZoomToSelectionMarginFraction;
                const double wanted = view.rangeSeconds + margin * 2.0;
                timelineZoomFactor = std::clamp (
                                                 (view.widthPixels / wanted) / view.fitPixelsPerSecond,
                                                 yesdaw::ui::UiTheme::Layout::timelineZoomMin,
                                                 timelineZoomCeiling());
                timelineScrollSeconds = std::max (0.0, view.rangeStartSeconds - margin);
                refreshTimelineZoomReadout();
                lastSelectionZoom = ZoomView { timelineZoomFactor, timelineScrollSeconds };
            }
            (void) appModel.dispatch (action);
            return;
        }

        case yesdaw::ui::UiActionId::TimelineClipCut:
        case yesdaw::ui::UiActionId::TimelineClipCopy:
            // G2.5: with a Time selection and no clip selected, the clip chords act on the range.
            if (appModel.context().timelineRangeSelected && ! appModel.context().timelineClipSelected
                && appModel.context().activePanel != yesdaw::ui::UiPanel::PianoRoll)
            {
                (void) appModel.dispatch (action == yesdaw::ui::UiActionId::TimelineClipCut ? yesdaw::ui::UiActionId::TimelineRangeCut
                                          : action == yesdaw::ui::UiActionId::TimelineClipCopy ? yesdaw::ui::UiActionId::TimelineRangeCopy
                                                                                                : yesdaw::ui::UiActionId::TimelineRangeDelete);
                return;
            }
            (void) appModel.dispatch (action);
            return;

        case yesdaw::ui::UiActionId::ViewPianoRoll:
            (void) appModel.dispatch (action);
            // G2.1 cp2: P toggles the tab — only a SHOWN roll wants a clip (selecting one
            // opens the editor, which would undo the close).
            if (dockShowsPianoRoll())
                (void) appModel.selectFirstMidiClip();
            return;

        default:
            (void) appModel.dispatch (action);
            return;
    }
}

void MainComponent::refreshActionState()
{
    ++actionStateRefreshes;   // G0.4 probe: how often the 391-line refresh actually runs
    loadViewStateIfBundleChanged();   // G2.1
    restoreControlsHiddenByDockTab();   // G2.1 cp2: the laws below decide afresh
    rebuildTimelineClipViews();
    // E29: a device change (adoption, Test Device, refresh) re-lists the channel pick.
    if (recordingChannelChooserGeneration != appModel.context().recordingDeviceGeneration)
    {
        recordingChannelChooserGeneration = appModel.context().recordingDeviceGeneration;
        refreshingAudioDeviceChooser = true;
        refreshRecordingInputChannelChooser();
        refreshingAudioDeviceChooser = false;
    }
    const auto& toolbarActions = yesdaw::ui::mainShellToolbarActions();
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        const auto action = toolbarActions[i];
        // G0.7: the device + recording cluster lives in the collapsible settings row; every
        // other toolbar button is visible in every view.
        // G1.7: Comp belongs to the G7 take-lane UI — hidden until then (the verb stays
        // dispatchable through the menu and the harness).
        buttons[i].setVisible ((! isSettingsRowAction (action) || appModel.context().settingsRowVisible)
                               && action != yesdaw::ui::UiActionId::RecordingAssembleComp);
        const auto state = appModel.registry().stateFor (action, appModel.context());
        const bool hasRequiredPlayback = ! toolbarActionRequiresPlayback (action) || appModel.playbackReady();
        buttons[i].setEnabled (state.enabled && hasRequiredPlayback);
        buttons[i].setToggleState ((action == yesdaw::ui::UiActionId::TransportToggleLoop && appModel.context().loopEnabled)
                                       || (action == yesdaw::ui::UiActionId::RecordingArmTrack
                                           && appModel.context().recordingTrackArmed)
                                       || (action == yesdaw::ui::UiActionId::RecordingSetMonitoringPolicy
                                           && appModel.context().recordingMonitoringSelected)
                                       || (action == yesdaw::ui::UiActionId::RecordingAssembleComp
                                           && appModel.context().recordingCompSelected)
                                       || (action == yesdaw::ui::UiActionId::ViewMixer && dockShowsMixer())
                                       || (action == yesdaw::ui::UiActionId::ViewPianoRoll && dockShowsPianoRoll()),
                                   juce::dontSendNotification);
    }
    refreshAutosaveRecoveryControls();
    const bool exportInProgress = appModel.context().audioExportInProgress;
    exportAudioButton.setVisible (! exportInProgress);
    exportAudioProgress.setVisible (exportInProgress);
    exportAudioCancelButton.setVisible (exportInProgress);
    exportAudioButton.setEnabled (
        appModel.registry().stateFor (yesdaw::ui::UiActionId::ProjectExportAudio,
                                      appModel.context()).enabled);
    exportAudioCancelButton.setEnabled (
        exportInProgress
        && appModel.registry().stateFor (yesdaw::ui::UiActionId::ProjectExportAudioCancel,
                                         appModel.context()).enabled);
    exportAudioProgress.setText (exportAudioProgressText(), juce::dontSendNotification);
    masterLoudnessReadout.setEnabled (
        appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerReadLoudness,
                                      appModel.context()).enabled);
    masterLoudnessReadout.setButtonText (masterLoudnessReadoutText());
    // G2.1 cp2: the arrangement is always there; the dock shows one editor tab.
    timelineInput.setVisible (true);
    playheadLayer.setVisible (true);
    pianoRollInput.setVisible (dockShowsPianoRoll());
    pianoRollLaneChooser.setVisible (dockShowsPianoRoll());   // G3.3
    pianoRollKeyChooser.setVisible (dockShowsPianoRoll());    // G3.8
    pianoRollScaleChooser.setVisible (dockShowsPianoRoll());
    pianoRollKeyChooser.setSelectedId (appModel.context().pianoRollScaleRoot + 1, juce::dontSendNotification);
    pianoRollScaleChooser.setSelectedId (appModel.context().pianoRollScaleChoice + 1, juce::dontSendNotification);
    pianoRollLaneChooser.setSelectedId (appModel.context().pianoRollControlLaneChoice + 1, juce::dontSendNotification);
    pianoRollTypingButton.setVisible (dockShowsPianoRoll());   // G3.6
    pianoRollTypingButton.setToggleState (appModel.context().musicalTypingOn, juce::dontSendNotification);
    pianoRollStepButton.setVisible (dockShowsPianoRoll());
    pianoRollStepButton.setToggleState (appModel.context().stepInputOn, juce::dontSendNotification);
    pianoRollStepButton.setEnabled (appModel.registry().stateFor (yesdaw::ui::UiActionId::PianoRollStepInputToggle, appModel.context()).enabled);
    mixerStripsInput.setVisible (dockShowsMixer());
    instrumentPanel.setVisible (dockShowsInstrument());   // G3.1
    if (dockShowsInstrument())
        instrumentPanel.refresh();
    {
        refreshingTimeMapControls = true;
        const bool tempoEnabled =
            appModel.registry().stateFor (yesdaw::ui::UiActionId::TransportSetTempo, appModel.context()).enabled;
        headerTempoControl.setEnabled (tempoEnabled);
        headerMeterChooser.setEnabled (
            appModel.registry().stateFor (yesdaw::ui::UiActionId::TransportSetMeter, appModel.context()).enabled);
        if (appModel.context().projectLoaded && ! appModel.project().tempoMap.empty())
            headerTempoControl.setValue (appModel.tempoAtPlayhead(), juce::dontSendNotification);   // G2.15: the tempo in force
        if (appModel.context().projectLoaded && ! appModel.project().meterMap.empty())
        {
            const auto& head = appModel.project().meterMap.front();
            for (std::size_t i = 0; i < kHeaderMeterChoices.size(); ++i)
                if (kHeaderMeterChoices[i].first == head.numerator
                    && kHeaderMeterChoices[i].second == head.denominator)
                    headerMeterChooser.setSelectedId (static_cast<int> (i) + 1, juce::dontSendNotification);
        }
        refreshingTimeMapControls = false;
    }
    {
        const std::vector<yesdaw::engine::FxInsert> chain = appModel.selectedStripFxChain();
        // G4.1 cp2: the editor follows the SELECTED strip's selected slot; a strip change, an empty
        // selection or a removed insert closes it (it never shows another strip's effect by accident).
        if (selectedFxParamSlot >= 0 && static_cast<std::size_t> (selectedFxParamSlot) >= chain.size())
            selectedFxParamSlot = -1;
        if (fxEditorOpen && (selectedFxParamSlot < 0 || appModel.selectedMixerStripOrdinal() != fxEditorStripOrdinal))
            fxEditorOpen = false;
        const bool editorShown = fxEditorOpen && dockShowsMixer();
        if (editorShown)
        {
            const yesdaw::engine::FxInsert& insert = chain[static_cast<std::size_t> (selectedFxParamSlot)];
            const yesdaw::ui::UiMixerStrip* strip = nullptr;
            const auto surface = currentMixerSurface();
            const int ordinal = appModel.selectedMixerStripOrdinal();
            if (ordinal >= 0 && static_cast<std::size_t> (ordinal) < surface.tracks.size())
                strip = &surface.tracks[static_cast<std::size_t> (ordinal)];
            else if (ordinal >= 0 && static_cast<std::size_t> (ordinal) - surface.tracks.size() < surface.buses.size())
                strip = &surface.buses[static_cast<std::size_t> (ordinal) - surface.tracks.size()];
            fxEditor.setTitleText (juce::String (fxKindName (insert.kind))
                                   + juce::String::fromUTF8 (" \xc2\xb7 ") + (strip != nullptr ? juce::String (strip->name) : juce::String ("Master"))
                                   + juce::String::fromUTF8 (" \xc2\xb7 slot ") + juce::String (selectedFxParamSlot + 1));
            fxEditor.setBypassed (! insert.enabled);
            fxEditor.setInsert (insert, appModel.project().sampleRate.hz);
        }
        if (fxEditor.isVisible() != editorShown)
        {
            fxEditor.setVisible (editorShown);
            if (editorShown)
                fxEditor.toFront (false);
        }

        refreshingFxParamControls = true;
        std::size_t used = 0;
        std::size_t pageCount = 0;
        if (selectedFxParamSlot >= 0)
        {
            const yesdaw::engine::FxKind kind =
                chain[static_cast<std::size_t> (selectedFxParamSlot)].kind;
            const bool paramEditEnabled =
                appModel.registry().stateFor (yesdaw::ui::UiActionId::MixerFxInsertParamSet,
                                              appModel.context()).enabled;

            // E15: collect EVERY accepted param id, then show the selected page of rows.
            std::vector<std::uint32_t> acceptedIds;
            for (std::uint32_t paramId = 0;
                 paramId < yesdaw::ui::UiTheme::Layout::mixerFxParamProbeLimit;
                 ++paramId)
            {
                if (yesdaw::engine::fxKindAcceptsParameterId (kind, paramId))
                    acceptedIds.push_back (paramId);
            }
            pageCount = (acceptedIds.size() + mixerFxParamSliders.size() - 1)
                      / std::max<std::size_t> (1, mixerFxParamSliders.size());
            if (selectedFxParamPage < 0
                || static_cast<std::size_t> (selectedFxParamPage) >= pageCount)
                selectedFxParamPage = 0;

            mixerFxParamPageChooser.clear (juce::dontSendNotification);
            for (std::size_t page = 0; page < pageCount; ++page)
            {
                const std::size_t firstParam = page * mixerFxParamSliders.size();
                const std::size_t lastParam = std::min (firstParam + mixerFxParamSliders.size(),
                                                        acceptedIds.size());
                const auto pageName = kind == yesdaw::engine::FxKind::Eq
                    ? "Bands " + juce::String (static_cast<int> (acceptedIds[firstParam] / yesdaw::engine::EqNode::kParamsPerBand) + 1)
                        + "-" + juce::String (static_cast<int> (acceptedIds[lastParam - 1] / yesdaw::engine::EqNode::kParamsPerBand) + 1)
                    : "Params " + juce::String (static_cast<int> (firstParam) + 1)
                        + "-" + juce::String (static_cast<int> (lastParam));
                mixerFxParamPageChooser.addItem (pageName, static_cast<int> (page) + 1);
            }
            mixerFxParamPageChooser.setSelectedId (selectedFxParamPage + 1, juce::dontSendNotification);
            mixerFxParamPageChooser.setEnabled (paramEditEnabled);

            const std::size_t firstShown =
                static_cast<std::size_t> (selectedFxParamPage) * mixerFxParamSliders.size();
            for (std::size_t i = firstShown;
                 i < acceptedIds.size() && used < mixerFxParamSliders.size();
                 ++i)
            {
                const std::uint32_t paramId = acceptedIds[i];
                const yesdaw::engine::ParamSpec spec = yesdaw::engine::fxParamSpecForKind (kind, paramId);
                juce::String parameterName (spec.name);
                if (kind == yesdaw::engine::FxKind::Eq)
                {
                    static constexpr std::array<const char*, 4> names { "Type", "Frequency", "Gain", "Q" };
                    parameterName = "Band " + juce::String (static_cast<int> (paramId / yesdaw::engine::EqNode::kParamsPerBand) + 1)
                                  + " " + names[paramId % yesdaw::engine::EqNode::kParamsPerBand];
                }
                const double normalized = appModel.fxInsertParamValueOnSelectedStrip (
                    static_cast<std::size_t> (selectedFxParamSlot), paramId);
                mixerFxParamSliderIds[used] = paramId;
                if (spec.choiceCount >= 2 && spec.choiceNames != nullptr)
                {
                    // E15: choice-shaped param — a real chooser replaces the raw slider.
                    auto& choiceChooser = mixerFxParamChoosers[used];
                    choiceChooser.clear (juce::dontSendNotification);
                    for (int choice = 0; choice < static_cast<int> (spec.choiceCount); ++choice)
                        choiceChooser.addItem (spec.choiceNames[choice], choice + 1);
                    const double real = yesdaw::engine::mapNormalized (spec, normalized);
                    const double step = (spec.max - spec.min)
                                      / static_cast<double> (spec.choiceCount - 1);
                    const int currentChoice = juce::jlimit (
                        0, static_cast<int> (spec.choiceCount) - 1,
                        static_cast<int> (std::llround ((real - spec.min) / step)));
                    choiceChooser.setSelectedId (currentChoice + 1, juce::dontSendNotification);
                    choiceChooser.setEnabled (paramEditEnabled);
                    choiceChooser.setVisible (true);
                    mixerFxParamSliders[used].setVisible (false);
                    mixerFxParamLabels[used].setText (
                        parameterName + " " + spec.choiceNames[currentChoice],
                        juce::dontSendNotification);
                }
                else
                {
                    // Alt+click resets the bound parameter to its ParamSpec default.
                    mixerFxParamSliders[used].setDoubleClickReturnValue (
                        true, yesdaw::engine::normalizedDefault (spec));
                    mixerFxParamSliders[used].setValue (normalized, juce::dontSendNotification);
                    mixerFxParamSliders[used].setEnabled (paramEditEnabled);
                    mixerFxParamSliders[used].setVisible (true);
                    mixerFxParamChoosers[used].setVisible (false);
                    mixerFxParamLabels[used].setText (
                        parameterName
                            + " " + juce::String (yesdaw::engine::mapNormalized (spec, normalized), 1)
                            + spec.unit,
                        juce::dontSendNotification);
                }
                mixerFxParamLabels[used].setVisible (true);
                ++used;
            }
        }
        for (std::size_t index = used; index < mixerFxParamSliders.size(); ++index)
        {
            mixerFxParamSliders[index].setVisible (false);
            mixerFxParamChoosers[index].setVisible (false);
            mixerFxParamLabels[index].setVisible (false);
        }
        const bool pagerVisible = pageCount > 1;
        mixerFxParamPageChooser.setVisible (pagerVisible);
        refreshingFxParamControls = false;
        if (used != lastVisibleFxParamRows || pagerVisible != lastFxParamPagerVisible)
        {
            lastVisibleFxParamRows = used;
            lastFxParamPagerVisible = pagerVisible;
            resized();
        }
    }
    {
        refreshingSnapChooser = true;
        timelineSnapChooser.setVisible (appModel.context().activePanel == yesdaw::ui::UiPanel::Timeline);
        timelineSnapChooser.setEnabled (appModel.context().projectLoaded);
        const int snapId = appModel.snapUnit() == yesdaw::ui::UiAppModel::UiSnapUnit::Off ? 1
                         : appModel.snapUnit() == yesdaw::ui::UiAppModel::UiSnapUnit::Bar ? 2
                         : appModel.snapUnit() == yesdaw::ui::UiAppModel::UiSnapUnit::Sixteenth ? 4
                         : 3;
        timelineSnapChooser.setSelectedId (snapId, juce::dontSendNotification);
        refreshingNudgeChooser = true;
        nudgeValueChooser.setSelectedId (appModel.context().nudgeValue + 1, juce::dontSendNotification);
        refreshingEditModeChooser = true;   // G2.6
        editModeChooser.setSelectedId (static_cast<int> (appModel.context().editMode) + 1, juce::dontSendNotification);
        refreshingEditModeChooser = false;
        refreshingSnapModeChooser = true;   // G2.7
        snapModeChooser.setSelectedId (static_cast<int> (appModel.context().snapMode) + 1, juce::dontSendNotification);
        refreshingSnapModeChooser = false;
        refreshingNudgeChooser = false;
        refreshActionTooltips();
        inspectorToggle.setToggleState (appModel.context().inspectorVisible, juce::dontSendNotification);
        refreshingSnapChooser = false;
    }
    const bool railVisible = true;   // G2.1 cp2: no modal mixer view
    trackListInput.setVisible (railVisible);
    trackAddButton.setVisible (railVisible);
    trackAddButton.setEnabled (
        appModel.registry().stateFor (yesdaw::ui::UiActionId::TrackAdd, appModel.context()).enabled);
    if (! railVisible)
        dismissTrackRenameEditor();
    if (! appModel.context().timelineClipSelected)
        dismissClipRenameEditor();
    if (selectedTrackLane >= static_cast<int> (appModel.project().tracks.size()))
        selectedTrackLane = static_cast<int> (appModel.project().tracks.size()) - 1;
    const bool inspectorVisible = appModel.context().timelineClipSelected
                               && ! appModel.context().inspectorTrackTabActive;
    inspectorStart.setVisible (inspectorVisible);
    inspectorEnd.setVisible (inspectorVisible);
    inspectorLength.setVisible (inspectorVisible);
    inspectorGain.setVisible (inspectorVisible);
    inspectorStretch.setVisible (inspectorVisible);
    inspectorFadeIn.setVisible (inspectorVisible);
    inspectorFadeOut.setVisible (inspectorVisible);
    inspectorFadeCurve.setVisible (inspectorVisible);
    inspectorFadeCurveAmount.setVisible (inspectorVisible);
    inspectorMarkerList.setVisible (inspectorVisible);   // G2.14
    refreshAutomationLaneControls();
    refreshInspectorControls();
    refreshMixerControls();
    {
        // G3.1: the inspector's instrument chooser mirrors the selected Track's slot; the
        // panel re-reads its rows whenever it shows.
        refreshingInspectorControls = true;
        const yesdaw::engine::Track* const instrumentTrack = appModel.selectedTrackForInstrument();
        const bool instrumentEnabled = instrumentTrack != nullptr
            && appModel.registry().stateFor (yesdaw::ui::UiActionId::TrackSetInstrument, appModel.context()).enabled;
        inspectorInstrumentChooser.setEnabled (instrumentEnabled);
        inspectorInstrumentChooser.setVisible (appModel.context().inspectorTrackTabActive);
        inspectorInstrumentEdit.setVisible (appModel.context().inspectorTrackTabActive);
        inspectorInstrumentEdit.setEnabled (instrumentTrack != nullptr);
        inspectorInstrumentEdit.setToggleState (dockShowsInstrument(), juce::dontSendNotification);
        if (instrumentTrack != nullptr)
            inspectorInstrumentChooser.setSelectedId (static_cast<int> (instrumentTrack->instrumentKind) + 1, juce::dontSendNotification);
        refreshingInspectorControls = false;
        if (dockShowsInstrument())
            instrumentPanel.refresh();
    }
    mixerDockToggle.setToggleState (dockShowsMixer(), juce::dontSendNotification);   // G2.1 cp2: the mixer TAB
    // No effect in the full-view Mixer panel (it never reserves dock space to begin with).
    mixerDockToggle.setVisible (true);
    // V7: the tab buttons live wherever the inspector panel does; the active tab lights.
    const bool inspectorPanelVisible = true;   // G2.1 cp2
    inspectorClipTab.setVisible (inspectorPanelVisible);
    inspectorTrackTab.setVisible (inspectorPanelVisible);
    inspectorClipTab.setToggleState (! appModel.context().inspectorTrackTabActive,
                                     juce::dontSendNotification);
    inspectorTrackTab.setToggleState (appModel.context().inspectorTrackTabActive,
                                      juce::dontSendNotification);
}

void MainComponent::refreshAutosaveRecoveryControls()
{
    const bool visible = appModel.context().autosaveRecoveryPending;
    const auto restoreState = appModel.registry().stateFor (yesdaw::ui::UiActionId::AutosaveRecoveryRestore,
                                                            appModel.context());
    const auto discardState = appModel.registry().stateFor (yesdaw::ui::UiActionId::AutosaveRecoveryDiscard,
                                                            appModel.context());

    autosaveRestoreButton.setVisible (visible);
    autosaveDiscardButton.setVisible (visible);
    autosaveRestoreButton.setEnabled (visible && restoreState.enabled);
    autosaveDiscardButton.setEnabled (visible && discardState.enabled);
}

juce::String MainComponent::exportAudioProgressText() const
{
    const int percent = appModel.context().audioExportProgressPercent;
    if (percent < 0)
        return "Export --";

    return "Export " + juce::String (percent) + "%";
}

} // namespace yesdaw::ui
