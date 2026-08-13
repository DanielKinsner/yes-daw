// YES DAW - shipped JUCE app shell surface.
//
// H12's input harness constructs this shell directly so CI can prove real Components,
// not only the headless model underneath them.

#pragma once

#include "engine/Project.h"
#include "ui/UiAppModel.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <filesystem>
#include <functional>
#include <cstdint>
#include <memory>
#include <string>
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

[[nodiscard]] juce::Component* findMainComponentChildForAction (juce::Component& component, UiActionId action);
[[nodiscard]] const juce::Component* findMainComponentChildForAction (const juce::Component& component, UiActionId action);

} // namespace yesdaw::ui
