// YES DAW - shipped JUCE app shell surface.
//
// H12's input harness constructs this shell directly so CI can prove real Components,
// not only the headless model underneath them.

#pragma once

#include "engine/Project.h"
#include "ui/TimelineCanvas.h"
#include "ui/UiAppModel.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <filesystem>
#include <functional>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace yesdaw::ui {

struct MainComponentFileChoices
{
    std::function<std::filesystem::path()> chooseNewProjectBundle;
    std::function<std::filesystem::path()> chooseOpenProjectBundle;
    std::function<std::filesystem::path()> chooseSaveAsProjectBundle;
    std::function<std::filesystem::path()> chooseImportAudioFile;
    std::function<std::filesystem::path()> chooseExportAudioFile;
    std::function<engine::Project()> makeNewProject;
    // Audio device seams (usable-DAW P1 real device chooser): the native shell defaults these to the
    // JUCE device manager; the harness injects deterministic fakes so the chooser is gate-testable.
    std::function<std::vector<std::string>()> listAudioOutputDevices;
    std::function<bool (const std::string&)> selectAudioOutputDevice;
    // E29: input-side seams for the input device chooser (native shell = JUCE device manager).
    std::function<std::vector<std::string>()> listAudioInputDevices;
    std::function<bool (const std::string&)> selectAudioInputDevice;
    // Close-confirm seam (B37): asked when the app closes with edits since the last explicit Save.
    // Returns kCloseChoiceSave, kCloseChoiceClose, or kCloseChoiceCancel; the native shell shows a
    // three-way box when unset.
    std::function<int()> confirmCloseUnsavedChanges;
    // Session-state directory seam (B39): the harness points last-project/recent-projects records
    // at a test-local directory; the native shell keeps its real per-user location when unset.
    std::filesystem::path sessionStateDirectory;
};

inline constexpr int kCloseChoiceSave = 0;
inline constexpr int kCloseChoiceClose = 1;
inline constexpr int kCloseChoiceCancel = 2;

struct MainComponentSnapshot
{
    bool isMainComponent = false;
    bool primaryFileChoicesReady = false;
    // Window title with the dirty marker (B38): "<project>[*] - YES DAW"; empty without a project.
    std::string windowTitle;
    bool desktopAudioRequested = false;
    bool desktopAudioOpen = false;
    std::uint64_t deviceAudioCallbackBlockCount = 0;
    std::uint64_t deviceAudioNonSilentBlockCount = 0;
    bool playbackReady = false;
    long long playbackLoopStartFrame = 0;
    long long playbackLoopEndFrame = 0;
    // R4: the shared status line as the model reports it (empty = quiet).
    std::string statusLineText;
    bool statusLineIsError = false;
    long long timelineRangeStartFrame = -1;
    long long timelineRangeEndFrame = -1;
    double timelineZoomFactor = 1.0;
    double timelineScrollSeconds = 0.0;
    int timelineTrackScrollRows = 0;
    int timelineMaxTrackScrollRows = 0;
    int pianoRollViewLowKey = 0;
    double pianoRollViewZoom = 1.0;
    long long pianoRollViewScrollTicks = 0;
    int visibleTimelineTrackCount = 0;
    int visibleTimelineClipCount = 0;
    std::string visibleFirstTimelineClipName;
    int selectedTimelineClipCount = 0;
    double visibleTimelineTotalSeconds = 0.0;
    int visibleMixerTrackCount = 0;
    int visibleMixerBusCount = 0;
    // E23: which strip the painted mixer highlights (tracks first, then buses; -1 = none).
    int selectedMixerStripOrdinal = -1;
    bool visibleMixerLoudnessValid = false;
    float visibleMasterPeakLeft = 0.0f;
    float visibleMasterPeakRight = 0.0f;
    int visiblePianoRollNoteCount = 0;
    int width = 0;
    int height = 0;
    int childCount = 0;
    std::filesystem::path bundlePath;
    UiActionContext context;
    UiRecordingDeviceSelection recordingDevice;
    UiRecordingTrackInputSelection recordingTrackInput;
    // M11: the whole arm SET as the shell reads it, in arm order (recordingTrackInput above is
    // its first entry — the primary).
    std::vector<UiRecordingTrackInputSelection> armedRecordingTrackInputs;
    // E30: the armed input's live meter peak as the shell reads it.
    float liveInputMeterPeak = 0.0f;
    UiRecordedAudioTake lastRecordedAudioTake;
    UiRecordedMidiTake lastRecordedMidiTake;
    UiRecordingCompSelection recordingComp;
    UiAutosaveRecoveryPrompt autosaveRecovery;
};

[[nodiscard]] std::unique_ptr<juce::Component> createMainComponent();
[[nodiscard]] std::unique_ptr<juce::Component> createMainComponent (MainComponentFileChoices fileChoices);
[[nodiscard]] MainComponentSnapshot snapshotMainComponent (const juce::Component& component);
[[nodiscard]] std::vector<float> renderMainComponentPlayback (juce::Component& component,
                                                              std::uint64_t frames,
                                                              int blockSize);
[[nodiscard]] bool serviceMainComponentUiTimer (juce::Component& component);
// Ask the shell whether the app may close (B37): true when the session is clean or the user chose
// Save or Close through the confirm seam; false when the user cancelled.
[[nodiscard]] bool mainComponentConfirmsClose (juce::Component& component);
[[nodiscard]] bool processMainComponentDeviceAudioBlock (juce::Component& component,
                                                         float* const* outputChannels,
                                                         int numOutputChannels,
                                                         int numFrames);
// E30: input-carrying harness block for CI-deterministic input metering.
[[nodiscard]] bool processMainComponentDeviceAudioBlock (juce::Component& component,
                                                         const float* const* inputChannels,
                                                         int numInputChannels,
                                                         float* const* outputChannels,
                                                         int numOutputChannels,
                                                         int numFrames);

// N1: the shell's OWN painted Mute/Solo cell rect (shell coordinates); cell 0 is Solo, 1 is Mute.
// The paint, the click law and the selected strip's live buttons all read this same law.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedMuteSoloCellBounds (const juce::Component& component,
                                                                           int stripIndex,
                                                                           int cellIndex);

// M4: the shell's OWN painted insert-slot rect (shell coordinates), so gates hit-test the exact
// geometry the paint and the click law use. Empty rect when the strip or slot is out of range.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedInsertSlotBounds (const juce::Component& component,
                                                                         int stripIndex,
                                                                         int slotIndex);

// M5: the shell's OWN painted send-row rect (shell coordinates), same law as the paint and the
// drag. Empty rect when the strip or row is out of range or the strip has no room for it.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedSendRowBounds (const juce::Component& component,
                                                                      int stripIndex,
                                                                      int sendIndex);

// M6: the painted fader rail, and the y its thumb sits at for a given linear gain — one law with
// the paint, so gates can pin that unity is at half travel and boost lives above it.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedFaderRailBounds (const juce::Component& component,
                                                                        int stripIndex);
[[nodiscard]] int mainComponentPaintedFaderThumbY (const juce::Component& component,
                                                   int stripIndex,
                                                   float linearGain);

// N3: the painted mixer-strip lane rect (shell coordinates) for a track/bus strip, and the
// master pane's rect — both come from the SAME single law, so master is always the next
// contiguous slot after the last strip rather than a detached island. Empty rect when the strip
// is out of range.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedMixerStripBounds (const juce::Component& component,
                                                                          int stripIndex);
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedMixerMasterBounds (const juce::Component& component);

// V3: the dock's own reserved rect — collapses (near) zero height when the show/hide toggle
// hides it. The SAME law every layout function (timeline/rail/inspector/this) shares.
[[nodiscard]] juce::Rectangle<int> mainComponentMixerPanelBounds (const juce::Component& component);
[[nodiscard]] juce::Rectangle<int> mainComponentTimelineBounds (const juce::Component& component);

// M9: the header's master card rect (shell coordinates). Empty when the window is too narrow to
// carry it — the card drops WHOLE rather than keeping a label over a clipped meter.
[[nodiscard]] juce::Rectangle<int> mainComponentHeaderMasterCardBounds (const juce::Component& component);

// N6: the rail's painted row rect (shell coordinates) — the SAME law the paint, rowBounds, and
// rowAt hit-testing all share, so a gate can prove a height drag moved exactly one row.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedRailRowBounds (const juce::Component& component,
                                                                       int row);

// N7: the rail row's painted colour-swatch rect (the left accent bar, shell coordinates) — the
// SAME law the click-to-cycle gesture hit-tests against.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedColourSwatchBounds (const juce::Component& component,
                                                                            int row);

// N7: the ACTUAL colour the timeline canvas paints for one clip (by id) — reads the same cached
// style array the paint code reads from, so it can never drift from what is on screen.
[[nodiscard]] juce::Colour mainComponentTimelineClipColour (juce::Component& component,
                                                             engine::EntityId clipId);

// V2: the ACTUAL bar|beat the header paints — reads the same law the paint code uses, so it can
// never drift from what is on screen.
[[nodiscard]] engine::BarBeat mainComponentHeaderBarBeat (const juce::Component& component);

// V4: the ruler's painted bar-number labels (bar, x in timeline-component coordinates) — read
// through the SAME state build + geometry + label law the paint path uses (computeRulerBarLabels),
// so a gate can never re-derive the formula; plus the inverse pixel→seconds mapping of that SAME
// viewport, so a gate can cross-check a label's x against the tempo map without duplicating the
// paint math.
[[nodiscard]] std::vector<RulerBarLabel> mainComponentRulerBarLabels (juce::Component& component);
[[nodiscard]] double mainComponentRulerSecondsAtX (juce::Component& component, int x);

// V5: the rail's live L/R meter peaks for one row (the SAME hold-state values the paint reads),
// and the rail VOL fader's shell-coordinate rect (the SAME law paint and hit-test share).
[[nodiscard]] std::pair<float, float> mainComponentRailMeterChannelPeaks (
    const juce::Component& component, int row);

// V7: the inspector fade chart's inner rect (shell coordinates) — the SAME law the paint uses,
// so a gate can cross-check the painted curve against the shared clipFadeCurvePoints law.
[[nodiscard]] juce::Rectangle<int> mainComponentInspectorFadeChartBounds (
    const juce::Component& component);
[[nodiscard]] juce::Rectangle<int> mainComponentRailVolumeSliderBounds (
    const juce::Component& component, int row);

[[nodiscard]] juce::Component* findMainComponentChildForAction (juce::Component& component, UiActionId action);
[[nodiscard]] const juce::Component* findMainComponentChildForAction (const juce::Component& component, UiActionId action);

} // namespace yesdaw::ui
