// YES DAW - shipped JUCE app shell surface.
//
// H12's input harness constructs this shell directly so CI can prove real Components,
// not only the headless model underneath them.

#pragma once

#include "engine/Project.h"
#include "ui/ContextMenus.h"
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
    // G3.7: the MIDI file choosers (the native shell opens a file box; the harness injects a path).
    std::function<std::filesystem::path()> chooseImportMidiFile;
    std::function<std::filesystem::path()> chooseExportMidiFile;
    // G3.9: the Sampler pad's WAV chooser (a click on a pad cell; the harness injects a path).
    std::function<std::filesystem::path()> chooseSamplerPadFile;
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
    // G0.1 State probe (ADR-0046 §10): when set, the shell writes a JSON snapshot of its state to
    // this file on every UI tick (schema in the plan §7.2). The native shell fills it from the
    // YESDAW_STATE_PROBE environment variable; a normal launch leaves it empty and writes nothing.
    std::filesystem::path stateProbePath;
    // G0.1 Session drive: open this bundle at launch instead of the last-project record (the
    // native shell fills it from the command line: `YesDaw.exe <path.yesdaw>`).
    std::filesystem::path openBundleAtLaunch;
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
    // R12: engine-rebuild counter and live-scalar-applied counter — the mechanical proof that a
    // scalar strip edit rode the live command lane instead of rebuilding the PlaybackEngine.
    std::uint64_t playbackReplaceCount = 0;
    std::uint64_t playbackLiveScalarsApplied = 0;
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
// G0.1: the native launch path. Desktop audio + native choosers (as createMainComponent()), plus
// the launch-time seams the Session drive relies on: `openBundleAtLaunch` (from the command
// line), and the environment variables YESDAW_STATE_PROBE (probe file) and
// YESDAW_SESSION_STATE_DIR (last-project / recent-projects directory), so a driven launch never
// reads or writes the owner's real session records. An explicit `sessionStateDirectory` wins
// over the variable: the harness constructs the shipped shell against a temp directory so a
// test never reopens whatever project the machine's owner last had open.
[[nodiscard]] std::unique_ptr<juce::Component> createNativeMainComponent (std::filesystem::path openBundleAtLaunch,
                                                                          std::filesystem::path sessionStateDirectory = {});
// G0.1: the State probe document for the shell as it stands right now (the SAME string the
// timer tick writes to `stateProbePath`), so a gate can assert the schema without a file race.
// Empty for a non-MainComponent.
[[nodiscard]] std::string mainComponentStateProbeJson (juce::Component& component);
[[nodiscard]] MainComponentSnapshot snapshotMainComponent (const juce::Component& component);
[[nodiscard]] std::vector<float> renderMainComponentPlayback (juce::Component& component,
                                                              std::uint64_t frames,
                                                              int blockSize);
[[nodiscard]] bool serviceMainComponentUiTimer (juce::Component& component);
// Ask the shell whether the app may close (B37): true when the session is clean or the user chose
// Save or Close through the confirm seam; false when the user cancelled.
[[nodiscard]] bool mainComponentConfirmsClose (juce::Component& component);

// R9: one instance only. Every edit writes a full project snapshot and both instances
// auto-open the same last project, so two instances editing one bundle is last-writer-wins
// data loss (WAL + busy-timeout lets both "succeed"). Main.cpp consults this policy for
// JUCE's single-instance machinery; the [single-instance] gate pins the value.
[[nodiscard]] constexpr bool shellAllowsMultipleInstances() noexcept { return false; }
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
// 2026-09-04: the painted pan knob's disc for a strip (shell coordinates) — the drag's hit rect.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedPanKnobBounds (const juce::Component& component,
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

// G0.7: the header layout law, read back. Section 0 = tools, 1 = transport, 2 = master (empty
// when the card dropped). mainComponentHeaderRects lists every laid-out header rect (the
// settings-row controls only while the row is shown). The settings row is toggled through the
// real action; mainComponentRevealSettingsRowFor shows it only for an action that lives there.
[[nodiscard]] juce::Rectangle<int> mainComponentHeaderSectionBounds (const juce::Component& component, int section);
[[nodiscard]] std::vector<juce::Rectangle<int>> mainComponentHeaderRects (const juce::Component& component);
[[nodiscard]] juce::Rectangle<int> mainComponentHeaderTimeReadoutBounds (const juce::Component& component);
[[nodiscard]] int mainComponentHeaderHeight (const juce::Component& component);
void mainComponentSetSettingsRowVisible (juce::Component& component, bool visible);
// G1.3: the context menu a right-click at a shell point produces — the clicked object becomes
// the selection first, and the list (registered verbs in plan §3.3's order) is returned WITHOUT
// showing it, so headless gates can assert it. `shown` is false where nothing has a menu.
struct MainComponentContextMenu
{
    juce::String route;   // harness diagnostic: which surface the request reached
    bool shown = false;
    ContextMenuTarget target = ContextMenuTarget::Clip;
    int index = -1;   // lane / row / strip / marker index the click resolved to, when any
    std::vector<UiActionId> actions;
    // G4.1: the FX kinds the Add Insert submenu offered (a Track: every kind; a Bus / the master: the
    // audio kinds) — the promoted parking-lot item's pin.
    std::vector<int> addInsertKinds;
};
[[nodiscard]] MainComponentContextMenu mainComponentRequestContextMenu (juce::Component& component, juce::Point<int> shellPoint);
// G4.1: the LAST menu the shell built — a left-click on a strip's input / output slot opens one too
// (targets MixerStripInput / MixerStripOutput), so a gate reads it back the way the right-click law's
// caller does.
[[nodiscard]] MainComponentContextMenu mainComponentLastContextMenu (const juce::Component& component);
// G4.1: the ids above the action range for the strip menus' choices — an input (mono channel, or the
// pair starting at it), an output (0 = Master, n = bus n-1), a send destination (the bus index).
[[nodiscard]] int mainComponentMixerInputMenuId (int channel, bool stereoPair);
[[nodiscard]] int mainComponentMixerOutputMenuId (int choice);
[[nodiscard]] int mainComponentMixerSendMenuId (int busIndex);
// G4.1: the painted I/O row rect for a strip (shell coordinates): row 0 the input slot (Track strips
// only), row 1 the output slot (Track and Bus strips). Empty where the strip has none or the strip is
// too short to carry it — the same law the paint and the click read.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedIoRowBounds (const juce::Component& component, int stripIndex, int row);
// G4.1: the two slots' texts as painted ("In: 1+2" / "In: —", "Out: Master" / "Out: <bus>").
struct MainComponentMixerStripIo
{
    juce::String input;
    juce::String output;
};
[[nodiscard]] MainComponentMixerStripIo mainComponentMixerStripIo (const juce::Component& component, int stripIndex);
// G4.1: the view-state record the shell would write now (rail / inspector / dock / narrow).
[[nodiscard]] juce::String mainComponentViewStateRecord (const juce::Component& component);
// G1.3 cp2: invoke an item of the LAST requested context menu the way the popup's callback
// would — the insert-slot verbs route through the shell's per-slot handlers with the clicked
// slot; every other target dispatches the action. `direction` picks Move Up (-1) / Move Down
// (+1) for the reorder verb.
void mainComponentInvokeContextMenuItem (juce::Component& component, UiActionId action, int direction = 0);

// G1.5: the keymap editor through the harness — its visible rows (after a search), the status
// of the last rebind; search / select / bind drive the same paths the mouse and keyboard do.
struct MainComponentKeymapEditor
{
    bool visible = false;
    std::vector<UiActionId> rows;
    juce::String status;
};
[[nodiscard]] MainComponentKeymapEditor mainComponentKeymapEditor (juce::Component& component);
// G2.18: the undo history window's rows (oldest first) and the "now" row.
struct MainComponentUndoHistory
{
    bool visible = false;
    std::vector<juce::String> rows;
    int current = 0;
};
[[nodiscard]] MainComponentUndoHistory mainComponentUndoHistory (juce::Component& component);
void mainComponentUndoHistoryClickRow (juce::Component& component, int row);
// G3.1: the instrument panel's state for the harness — the dock tab, the kind, the rows.
struct MainComponentInstrumentPanel
{
    bool visible = false;
    juce::String kind;
    std::vector<std::pair<juce::String, double>> rows;   // label, normalized value
    bool padsVisible = false;                            // G3.9: the Sampler's pad grid is laid out
    std::vector<std::pair<int, juce::String>> pads;      // G3.9: key, name (loaded pads only)
    juce::Rectangle<int> padGrid;                        // G3.9: panel-local
};
// G3.9: the pad cell's mouse verbs through the panel itself — a plain click loads (the chooser seam),
// Shift+click toggles one-shot / pitched, Ctrl+click clears; a file dropped on a cell loads it.
void mainComponentInstrumentPanelClickPad (juce::Component& component, int key, bool shift, bool ctrl);
// G3.10: a MIDI input note as the device callback posts it (the same lane, no message-thread hop).
[[nodiscard]] bool mainComponentPostMidiInput (juce::Component& component, bool on, int key, double velocity);
void mainComponentInstrumentPanelDropFileOnPad (juce::Component& component, int key, const juce::String& path);
[[nodiscard]] MainComponentInstrumentPanel mainComponentInstrumentPanel (juce::Component& component);
void mainComponentInstrumentPanelSetRow (juce::Component& component, int row, double normalized);
void mainComponentInstrumentPanelDragRow (juce::Component& component, int row, double first, double second);   // one drag, two values
// G3.2: the roll's painted grid lines (tick, x, kind 0 = bar, 1 = beat, 2 = snap) and its clip-relative playhead tick.
struct MainComponentPianoRollGrid
{
    std::vector<std::tuple<std::int64_t, int, int>> lines;
    std::int64_t playheadTick = -1;
    std::int64_t viewScrollTicks = 0;
    std::int64_t visibleTicks = 0;
};
[[nodiscard]] MainComponentPianoRollGrid mainComponentPianoRollGrid (juce::Component& component);
// G3.2: audition - the key the roll's mouse holds, and where a key's row sits (the keyboard column's
// centre x, the row's centre y, the grid's x span) so a gate can press it as the mouse would.
struct MainComponentPianoRollAudition
{
    int heldKey = -1;
    int keyboardX = 0;
    int keyY = 0;
    int gridLeft = 0;
    int gridRight = 0;
};
[[nodiscard]] MainComponentPianoRollAudition mainComponentPianoRollAudition (juce::Component& component, int key);
// G3.3: the roll's control lane as painted — roll-local rects (the data area right of the chooser
// gutter, and the chooser), the lane's name and value range, and its points (tick, value) with
// their ids (hex) — so a gate can place, drag and erase what the user sees.
struct MainComponentPianoRollControlLane
{
    juce::Rectangle<int> lane;
    juce::Rectangle<int> chooser;
    juce::String name;
    double valueMin = 0.0;
    double valueMax = 1.0;
    std::vector<std::pair<std::int64_t, double>> points;
    std::vector<juce::String> pointIds;
};
[[nodiscard]] MainComponentPianoRollControlLane mainComponentPianoRollControlLane (juce::Component& component);
void mainComponentSelectPianoRollControlLane (juce::Component& component, int choice);   // through the real chooser
void mainComponentSelectPianoRollScale (juce::Component& component, int rootKey, int scaleChoice);   // G3.8: through the header's two choosers
// G3.6: the harness's key-up for musical typing (a headless run has no real key state to poll).
void mainComponentReleaseTypedKeys (juce::Component& component);
// The lane's value <-> y law, shared with the paint and the hit-test (roll-local y).
[[nodiscard]] double mainComponentPianoRollControlValueForY (juce::Component& component, int rollLocalY);
[[nodiscard]] int mainComponentPianoRollControlYForValue (juce::Component& component, double value);
[[nodiscard]] bool mainComponentAuditionNote (juce::Component& component, std::int16_t key, bool on);
void mainComponentServiceUiTick (juce::Component& component);   // G3.2 checkpoint: one UI tick (the timer's refresh path)
[[nodiscard]] std::vector<float> mainComponentRenderPlaybackFrames (juce::Component& component, std::uint64_t frames, int blockSize);
void mainComponentKeymapEditorSearch (juce::Component& component, const juce::String& text);
// G1.6: the gesture hint the status line shows for the zone under a shell point (the same law
// the surfaces' mouseMove uses); empty where nothing has a hint.
[[nodiscard]] juce::String mainComponentHoverHintAt (juce::Component& component, juce::Point<int> shellPoint);
// G2.1: set the Editor dock's height through the same clamp the splitter uses (tests that need
// the mixer's full control lane grow the dock first, as a user would drag it).
void mainComponentSetDockHeight (juce::Component& component, int height);
// G2.2: invoke a context-menu item by its id (the submenus above the action range: Add Insert,
// Time Display) and the id of the ruler menu's Time Display entry for a mode (1 min:sec, 2 SMPTE,
// 3 samples) — so the gate walks the REAL menu record.
void mainComponentInvokeContextMenuId (juce::Component& component, int itemId);
// G2.3: one edge-band auto-scroll step while a clip drag sits in the band (the timer's law,
// ticked directly); returns the seconds scrolled.
double mainComponentTimelineAutoScrollTick (juce::Component& component);
// G2.4: the Smart tool's zone ("move", "snap-move", "trim-left", "trim-right", "fade-in",
// "fade-out", "gain", "time-select", "none") and cursor ("normal", "left-right", "top-left",
// "top-right", "ibeam", "up-down") under a shell point with the given modifiers.
[[nodiscard]] juce::String mainComponentTimelineZoneAt (juce::Component& component, juce::Point<int> shellPoint, juce::ModifierKeys modifiers);
[[nodiscard]] juce::String mainComponentTimelineCursorAt (juce::Component& component, juce::Point<int> shellPoint, juce::ModifierKeys modifiers);
[[nodiscard]] int mainComponentTimeDisplayMenuId (int mode);
void mainComponentKeymapEditorSelectRow (juce::Component& component, int row);
void mainComponentKeymapEditorBind (juce::Component& component, const juce::String& chord);

// G0.8: dispatch an action as a menu item or chord would, and read its live registry state.
void mainComponentDispatchAction (juce::Component& component, UiActionId action);
[[nodiscard]] UiActionState mainComponentActionState (const juce::Component& component, UiActionId action);
void mainComponentRevealSettingsRowFor (juce::Component& component, UiActionId action);

// N6: the rail's painted row rect (shell coordinates) — the SAME law the paint, rowBounds, and
// rowAt hit-testing all share, so a gate can prove a height drag moved exactly one row.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedRailRowBounds (const juce::Component& component,
                                                                       int row);

// N7: the rail row's painted colour-swatch rect (the left accent bar, shell coordinates) — the
// SAME law the click-to-cycle gesture hit-tests against.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedColourSwatchBounds (const juce::Component& component,
                                                                            int row);

// The rail row's painted M (0) / S (1) / O record-arm (2) cell rect (shell coordinates) — the
// SAME law the rail's click hit-test claims, so a test clicks the badge it sees.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedRailCellBounds (const juce::Component& component,
                                                                        int row, int cell);

// The header's painted gear rect (shell coordinates) — the settings-row toggle's click target.
[[nodiscard]] juce::Rectangle<int> mainComponentPaintedHeaderGearBounds (const juce::Component& component);

// The source window (asset frames: start, count) the timeline canvas is handed for one painted
// clip (by layout id) — the SAME window the waveform painter reads, so a gate can prove a split's
// right half paints where the left half stopped.
struct TimelineClipSourceWindow
{
    std::uint64_t startFrame = 0;
    std::uint64_t frameCount = 0;
};
[[nodiscard]] TimelineClipSourceWindow mainComponentTimelineClipSourceWindow (const juce::Component& component,
                                                                             int layoutClipId);

// G2.19: the zoom ceiling the shell clamps to — one sample per pixel at the project's rate over
// the fit width (never below UiTheme's timelineZoomMax).
[[nodiscard]] double mainComponentTimelineZoomCeiling (const juce::Component& component);

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
