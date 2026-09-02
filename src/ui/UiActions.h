// YES DAW - H11 UI action registry.
//
// Pure C++ on purpose: menus, toolbar buttons, shortcuts, accessibility, tests, and future agents all
// resolve through the same action IDs without requiring a display.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace yesdaw::ui {

enum class UiActionId : std::uint8_t
{
    ProjectNew = 0,
    ProjectOpen,
    ProjectSave,
    ProjectSaveAs,
    ProjectImportAudio,
    ProjectExportAudio,
    ProjectExportAudioCancel,
    ProjectExportDawproject,
    TransportPlay,
    TransportStop,
    TransportLocateStart,
    TransportToggleLoop,
    DeviceRefreshAudio,
    DeviceSelectTestAudio,
    RecordingArmTrack,
    RecordingSetMonitoringPolicy,
    TransportRecord,
    RecordingAssembleComp,
    EditUndo,
    EditRedo,
    ViewTimeline,
    ViewMixer,
    ViewPianoRoll,
    TimelineClipMove,
    TimelineClipTrim,
    TimelineClipSplit,
    TimelineClipSetGain,
    TimelineClipSetFades,
    TimelineClipTimeStretch,
    MixerTargetSetFader,
    MixerTargetSetPan,
    MixerTargetToggleMute,
    MixerTargetToggleSolo,
    MixerReadMeters,
    MixerReadLoudness,
    MixerReadSends,
    MixerSetFirstSendLevel,
    MixerReadFxSlots,
    MixerToggleFirstFxSlotEnabled,
    MixerReadGainReduction,
    MixerReadBusFxSlots,
    PianoRollNoteSelect,
    PianoRollNoteMove,
    PianoRollNoteSetLength,
    PianoRollNoteTranspose,
    PianoRollNoteQuantize,
    PianoRollReadExpressionLanes,
    AutosaveRecoveryRestore,
    AutosaveRecoveryDiscard,
    HelpShowKeymap,
    TimelineToolSelectPointer,
    TimelineToolSelectPencil,
    TimelineToolSelectScissors,
    TimelineToolSelectHand,
    TimelineToolSelectZoom,
    TimelineSnapDisable,
    TimelineSnapSetBar,
    TimelineSnapSetBeat,
    TimelineSnapSetSixteenth,
    TimelineAutomationToggleTrackLane,
    TimelineAutomationAddBreakpoint,
    TimelineAutomationDeleteBreakpoint,
    // Arrangement actions (usable-DAW P0, 2026-08-09)
    TimelineClipDelete,
    TrackAdd,
    TrackRename,
    TrackRemove,
    TrackReorder,
    PianoRollNoteAdd,
    PianoRollNoteDelete,
    MixerFxInsertAdd,
    MixerFxInsertRemove,
    MixerFxInsertToggle,
    MixerFxInsertReorder,
    MixerBusRename,
    MixerBusRemove,
    MixerSendSetTap,
    MixerSendSetDestination,
    MixerMasterSetFader,
    TransportSetTempo,
    TransportSetMeter,
    TimelineClipCopy,
    TimelineClipPaste,
    TimelineClipDuplicate,
    TransportToggleMetronome,
    TimelineMarkerAdd,
    TimelineMarkerRemove,
    TimelineMidiClipAdd,
    MixerFxInsertParamSet,
    // ADR-0044 send routing actions
    MixerBusAdd,
    MixerSendAdd,
    MixerSendRemove,
    MixerSendSetLevel,
    TimelineClipCut,
    TimelineClipSelectAllTrack,
    TimelineClipSelectAllProject,
    TimelineClipHeal,
    EditNudgeLeft,
    EditNudgeRight,
    EditNudgeLeftFine,
    EditNudgeRightFine,
    TimelineClipGainIncrease,
    TimelineClipGainDecrease,
    TimelineClipApplyDefaultFades,
    TimelineClipCrossfade,
    EditRenameSelection,
    TimelineClipRepeatPaste,
    TimelineZoomFitProject,
    TimelineZoomFitLoop,
    TimelineZoomIn,
    TimelineZoomOut,
    TrackSelectPrevious,
    TrackSelectNext,
    TransportLocatePreviousGrid,
    TransportLocateNextGrid,
    TransportLocatePreviousBar,
    TransportLocateNextBar,
    TimelineTogglePlayheadFollow,
    TransportShuttleFaster,
    TransportShuttleSlower,
    TransportToggleReturnToStartOnStop,
    TransportReturnToZero,
    TransportPlayFromLastLocate,
    TransportToggleRecordCountIn,
    TransportStoreLocatePoint1,
    TransportStoreLocatePoint2,
    TransportStoreLocatePoint3,
    TransportStoreLocatePoint4,
    TransportStoreLocatePoint5,
    TransportRecallLocatePoint1,
    TransportRecallLocatePoint2,
    TransportRecallLocatePoint3,
    TransportRecallLocatePoint4,
    TransportRecallLocatePoint5,
    TransportLocatePreviousMarker,
    TransportLocateNextMarker,
    TimelineRangeToLoop,
    // G2.5: the Time selection is a first-class object — verbs on the range.
    TimelineRangeSplitEdges,
    TimelineRangeCut,
    TimelineRangeCopy,
    TimelineRangeDelete,
    TimelineRangeSilence,
    TimelineZoomToSelection,
    TimelineSelectAllFollowing,
    // G2.6: the Edit mode — what happens to neighbours when a Clip is placed or removed.
    EditModeOverlap,
    EditModeNoOverlap,
    EditModeShuffle,
    TrackDuplicate,
    TrackMoveUp,
    TrackMoveDown,
    TrackToggleMute,
    TrackToggleSolo,
    TrackToggleArm,
    PianoRollNoteSetVelocity,
    PianoRollNoteOctaveUp,
    PianoRollNoteOctaveDown,
    PianoRollNoteDuplicate,
    PianoRollNoteQuantizeSelection,
    // M3: a Track's main output destination (master or a Bus)
    MixerTrackSetOutput,
    // V3: show/hide the always-on bottom mixer dock inside the Timeline/Piano Roll views
    TimelineToggleMixerDock,
    // V7: the right inspector's real CLIP/TRACK tabs (the painted tabs used to be decorative)
    InspectorShowClipTab,
    InspectorShowTrackTab,
    // R10: solo-safe on the selected strip — a solo elsewhere never mutes a safe strip
    // (buses default safe so a soloed bus-routed track stays audible through its bus).
    MixerTargetToggleSoloSafe,
    // G0.2 (ADR-0046 §4): Space toggles play/stop in every Focus context — the one transport
    // verb every reference DAW puts on the space bar.
    TransportTogglePlayStop,
    ViewToggleSettingsRow,
    PianoRollNoteSelectAll,
    ViewToggleInspector,
    EditNudgeValueGrid,
    EditNudgeValueBar,
    EditNudgeValueBeat,
    EditNudgeValueSixteenth,
    Count
};

constexpr std::size_t kUiActionCount = static_cast<std::size_t> (UiActionId::Count);
constexpr std::size_t kTransportLocatePointCount = 5;

enum class UiActionKind : std::uint8_t
{
    Command,
    Toggle,
    Query
};

enum class AccessibilityRole : std::uint8_t
{
    Button,
    ToggleButton,
    MenuItem,
    Panel
};

enum class UiPanel : std::uint8_t
{
    Timeline,
    Mixer,
    PianoRoll
};

// G2.1: which editor the Editor dock shows (ADR-0046: one tabbed editor at a time, never a
// modal view). UiPanel stays the FOCUS context — which editor receives non-global keys.
enum class UiEditorDockTab : std::uint8_t
{
    Mixer,
    PianoRoll
};

// G2.6 (CONTEXT.md "Edit mode"): the rule for neighbouring Clips when one is placed or removed —
// Overlap (default, neighbours untouched), No overlap (the placed Clip trims what it covers),
// Shuffle (neighbours close up on a removal, move aside on a placement).
enum class UiEditMode : std::uint8_t
{
    Overlap,
    NoOverlap,
    Shuffle
};

// G1.1 (plan §4): the Focus context a chord is looked up in. Global chords work everywhere; an
// Arrange / PianoRoll / Mixer chord only while that editor has focus, and the same chord may
// mean different things in different contexts (Alt+Up: clip gain in Arrange, transpose in the
// piano roll). Uniqueness is per context, and Global against every context.
enum class UiFocusContext : std::uint8_t
{
    Global,
    Arrange,
    PianoRoll,
    Mixer
};

[[nodiscard]] constexpr const char* focusContextName (UiFocusContext context) noexcept
{
    switch (context)
    {
        case UiFocusContext::Global:    return "Global";
        case UiFocusContext::Arrange:   return "Arrange";
        case UiFocusContext::PianoRoll: return "PianoRoll";
        case UiFocusContext::Mixer:     return "Mixer";
    }
    return "Global";
}

// G1.1 (plan §4): the Focus context an action's default chord lives in. Arrange verbs act on
// the arrangement's selection, PianoRoll verbs on notes; the same chord may serve both
// (Alt+Up: clip gain / transpose, Del: clip / note). The tool digits are Global: the piano roll
// shares the tool palette (the pencil draws notes); the nudges act on whichever editor's
// selection has focus (Logic nudges notes too). Everything else is Global.
[[nodiscard]] constexpr UiFocusContext defaultFocusContext (UiActionId id) noexcept
{
    switch (id)
    {
        case UiActionId::TimelineClipApplyDefaultFades:
        case UiActionId::TimelineClipCopy:
        case UiActionId::TimelineClipCut:
        case UiActionId::TimelineClipDelete:
        case UiActionId::TimelineClipGainDecrease:
        case UiActionId::TimelineClipGainIncrease:
        case UiActionId::TimelineClipHeal:
        case UiActionId::TimelineClipPaste:
        case UiActionId::TimelineClipRepeatPaste:
        case UiActionId::TimelineClipSelectAllProject:
        case UiActionId::TimelineClipSelectAllTrack:
        case UiActionId::TimelineClipSplit:
        case UiActionId::TimelineRangeSplitEdges:
        case UiActionId::TimelineRangeCut:
        case UiActionId::TimelineRangeCopy:
        case UiActionId::TimelineRangeDelete:
        case UiActionId::TimelineRangeSilence:
        case UiActionId::TimelineZoomToSelection:
        case UiActionId::TimelineSelectAllFollowing:
        case UiActionId::TimelineZoomIn:
        case UiActionId::TimelineZoomOut:
        case UiActionId::TransportLocateNextGrid:
        case UiActionId::TransportLocatePreviousGrid:
            return UiFocusContext::Arrange;
        case UiActionId::PianoRollNoteDelete:
        case UiActionId::PianoRollNoteOctaveDown:
        case UiActionId::PianoRollNoteOctaveUp:
        case UiActionId::PianoRollNoteQuantizeSelection:
        case UiActionId::PianoRollNoteTranspose:
        case UiActionId::PianoRollNoteSelectAll:
            return UiFocusContext::PianoRoll;
        default:
            return UiFocusContext::Global;
    }
}

[[nodiscard]] constexpr UiFocusContext focusContextForPanel (UiPanel panel) noexcept
{
    switch (panel)
    {
        case UiPanel::Timeline:  return UiFocusContext::Arrange;
        case UiPanel::Mixer:     return UiFocusContext::Mixer;
        case UiPanel::PianoRoll: return UiFocusContext::PianoRoll;
    }
    return UiFocusContext::Arrange;
}

enum class TimelineTool : std::uint8_t
{
    Pointer,
    Pencil,
    Scissors,
    Hand,
    Zoom
};

enum class UiRecordingMonitoringPolicy : std::uint8_t
{
    Unselected = 0,
    DirectInput,
    LatencyCompensated,
    Off
};

struct UiActionDescriptor
{
    UiActionId id;
    const char* stableId;
    const char* label;
    const char* defaultKey;
    const char* accessibleName;
    AccessibilityRole accessibleRole;
    UiActionKind kind;
    bool requiresProject;
    bool requiresUndo;
    bool requiresRedo;
    bool requiresTimelineClip;
    bool requiresMixerTarget = false;
    bool requiresMidiClip = false;
    bool requiresMidiNote = false;
    bool requiresRecordingDevice = false;
    bool requiresRecordingTrackInput = false;
    bool requiresRecordingMonitoring = false;
    bool requiresRecordingTrackAvailable = false;
    bool requiresRecordingCompTakes = false;
    bool requiresAutosaveRecovery = false;
};

struct UiActionContext
{
    bool projectLoaded = false;
    bool isPlaying = false;
    bool loopEnabled = false;
    bool timelineRangeSelected = false;
    bool canUndo = false;
    bool canRedo = false;
    bool timelineClipSelected = false;
    bool mixerTargetSelected = false;
    bool midiClipSelected = false;
    bool midiNoteSelected = false;
    bool recordingDeviceSelected = false;
    bool recordingTrackAvailable = false;
    bool recordingTrackArmed = false;
    bool recordingInputSelected = false;
    bool recordingMonitoringSelected = false;
    UiRecordingMonitoringPolicy selectedRecordingMonitoringPolicy = UiRecordingMonitoringPolicy::Unselected;
    bool isRecording = false;
    bool keymapVisible = false;
    UiPanel activePanel = UiPanel::Timeline;
    TimelineTool activeTimelineTool = TimelineTool::Pointer;
    bool snapEnabled = true;
    std::int64_t snapGridTicks = 512;
    bool clipboardHasClip = false;
    bool metronomeEnabled = false;
    bool recordCountInEnabled = false;
    bool settingsRowVisible = false;   // G0.7: the collapsible Audio & Export settings row
    bool inspectorVisible = true;      // G1.4: I shows / hides the inspector
    int nudgeValue = 0;                // G1.4: 0 grid, 1 bar, 2 beat, 3 sixteenth
    bool recordCountInActive = false;
    bool playheadFollowEnabled = true;
    bool returnToStartOnStopEnabled = false;
    // V3: the always-on bottom mixer dock (inside Timeline/Piano Roll) can be collapsed to
    // reclaim vertical space. True (visible) matches today's historical always-on behaviour, so
    // no existing screenshot/layout gate changes unless a test explicitly toggles it off.
    bool mixerDockVisible = true;
    UiEditorDockTab editorDockTab = UiEditorDockTab::Mixer;   // G2.1: the tab the dock shows
    UiEditMode editMode = UiEditMode::Overlap;                // G2.6
    // V7: which inspector tab is active — false = CLIP (the historical content), true = TRACK.
    bool inspectorTrackTabActive = false;
    bool timelineAutomationTrackLaneVisible = false;
    int timelineAutomationTrackIndex = -1;
    int timelineAutomationShowHideCount = 0;
    int timelineAutomationBreakpointEditCount = 0;
    std::int64_t playheadFrame = 0;
    std::int64_t playbackStartFrame = 0;
    std::int64_t lastLocateFrame = 0;
    std::array<std::optional<std::int64_t>, kTransportLocatePointCount> locatePoints {};
    int shuttlePlaybackRate = 1;
    int commandDispatchCount = 0;
    int saveCount = 0;
    int importCount = 0;
    int audioExportCount = 0;
    int audioExportProgressPercent = -1;
    bool audioExportInProgress = false;
    bool audioExportCancelRequested = false;
    int audioExportCancelCount = 0;
    int dawprojectExportCount = 0;
    int deviceRefreshCount = 0;
    int deviceSelectCount = 0;
    std::uint32_t recordingDeviceGeneration = 0;
    std::uint32_t selectedRecordingDeviceId = 0;
    int selectedRecordingTrackIndex = -1;
    int selectedRecordingInputChannel = -1;
    // E29: the armed pick records a stereo pair (channel, channel+1) instead of mono.
    bool selectedRecordingInputStereoPair = false;
    int recordingArmCount = 0;
    int recordingMonitoringCount = 0;
    int recordingCommandCount = 0;
    bool recordingCompTakesAvailable = false;
    // G0.8: the first Track's first send / first FX slot exist (the dock's "Send" and "FX On"
    // verbs act on them) — so a disabled control's registry state names why.
    bool firstTrackSendAvailable = false;
    bool firstTrackFxSlotAvailable = false;
    bool recordingCompSelected = false;
    int recordingCompSegmentCount = 0;
    int recordingCompCommandCount = 0;
    bool autosaveRecoveryPending = false;
    int autosaveRecoveryPromptCount = 0;
    int autosaveRecoveryRestoreCount = 0;
    int autosaveRecoveryDiscardCount = 0;
    int undoCount = 0;
    int redoCount = 0;
    int timelineEditCount = 0;
    int trackEditCount = 0;
    int mixerEditCount = 0;
    int mixerReadCount = 0;
    int midiEditCount = 0;
    int midiReadCount = 0;

    // G0.4: the shell refreshes its action state only when the context CHANGED (not every tick).
    [[nodiscard]] bool operator== (const UiActionContext&) const = default;
};

// H17 CP4 — crash-safety default. The shipped shell schedules an autosave on a control-tick
// cadence, and it is ON by default (a fresh install crash-recovers out of the box). Kept in this
// pure, JUCE-free action layer so the default is asserted by a headless test; the GUI shell drives
// a juce::Timer from it and the write itself is gated on the engine's needs-autosave revision.
struct AutosaveSchedulePolicy
{
    bool enabled = true;       // scheduling ON by default (CP4)
    int  intervalMs = 30000;   // control-tick cadence between autosave attempts (30s)
};

struct UiActionState
{
    bool enabled = false;
    const char* disabledReason = "";
};

struct UiActionDispatchResult
{
    UiActionId action = UiActionId::Count;
    UiActionState state {};
    bool dispatched = false;
};

enum class KeymapRebindStatus : std::uint8_t
{
    Ok,
    UnknownAction,
    EmptyChord,
    DuplicateChord
};

constexpr std::size_t actionIndex (UiActionId id)
{
    return static_cast<std::size_t> (id);
}

constexpr int storeLocatePointIndex (UiActionId id) noexcept
{
    const std::size_t index = actionIndex (id);
    const std::size_t first = actionIndex (UiActionId::TransportStoreLocatePoint1);
    return index >= first && index < first + kTransportLocatePointCount
        ? static_cast<int> (index - first)
        : -1;
}

constexpr int recallLocatePointIndex (UiActionId id) noexcept
{
    const std::size_t index = actionIndex (id);
    const std::size_t first = actionIndex (UiActionId::TransportRecallLocatePoint1);
    return index >= first && index < first + kTransportLocatePointCount
        ? static_cast<int> (index - first)
        : -1;
}

inline constexpr std::array<UiActionDescriptor, kUiActionCount> kUiActionDescriptors {{
    { UiActionId::ProjectNew, "project.new", "New", "Ctrl+N", "New project",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::ProjectOpen, "project.open", "Open", "Ctrl+O", "Open project",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::ProjectSave, "project.save", "Save", "Ctrl+S", "Save project",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::ProjectSaveAs, "project.save_as", "Save As", "Ctrl+Shift+S", "Save project as",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::ProjectImportAudio, "project.import_audio", "Import WAV", "Ctrl+Shift+I", "Import audio",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::ProjectExportAudio, "project.export_audio", "Export Audio", "Ctrl+B", "Export audio",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::ProjectExportAudioCancel, "project.export_audio.cancel", "Cancel / Pointer", "Esc", "Cancel an active audio export or return to the Pointer tool",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::ProjectExportDawproject, "project.export_dawproject", "Export DAWproject", "", "Export DAWproject package",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    // G0.2: no default chord — Space is the toggle; the Play button is this verb's path
    // (ADR-0046 §2: no invented chords; a chord-less action is reached by mouse).
    { UiActionId::TransportPlay, "transport.play", "Play", "", "Play transport",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportStop, "transport.stop", "Stop", "", "Stop transport",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportLocateStart, "transport.locate_start", "Locate", "Home", "Locate start",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportToggleLoop, "transport.toggle_loop", "Loop", "C", "Toggle loop",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false },
    { UiActionId::DeviceRefreshAudio, "device.refresh_audio", "Refresh Device", "", "Refresh audio device",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::DeviceSelectTestAudio, "device.select_test_audio", "Test Device", "", "Select test audio device",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::RecordingArmTrack, "record.track.arm", "Arm Track", "", "Arm selected Track for recording",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, false, false, false, true, false, false, true },
    { UiActionId::RecordingSetMonitoringPolicy, "record.monitoring_policy", "Monitor", "", "Choose recording monitoring policy",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, false, false, false, true },
    { UiActionId::TransportRecord, "transport.record", "Record", "R", "Record transport",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, false, false, true, true, true },
    { UiActionId::RecordingAssembleComp, "record.comp.assemble", "Comp", "", "Assemble recording Comp selection",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, false, false, false, false, false, false, true },
    { UiActionId::EditUndo, "edit.undo", "Undo", "Ctrl+Z", "Undo",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, true, false, false },
    { UiActionId::EditRedo, "edit.redo", "Redo", "Ctrl+Shift+Z", "Redo",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, true, false },
    { UiActionId::ViewTimeline, "view.timeline", "Timeline", "", "Show timeline",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::ViewMixer, "view.mixer", "Mixer", "", "Show the mixer in the editor dock",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::ViewPianoRoll, "view.piano_roll", "Piano Roll", "P", "Show or hide the piano roll in the editor dock",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineClipMove, "timeline.clip.move", "Move Clip", "", "Move selected clip",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipTrim, "timeline.clip.trim", "Trim Clip", "", "Trim selected clip",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipSplit, "timeline.clip.split", "Split Clip", "Ctrl+T", "Split selected clip",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipSetGain, "timeline.clip.set_gain", "Clip Gain", "", "Set selected clip gain",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipSetFades, "timeline.clip.set_fades", "Clip Fades", "", "Set selected clip fades",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipTimeStretch, "timeline.clip.time_stretch", "Time Stretch", "", "Time-stretch selected clip",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::MixerTargetSetFader, "mixer.target.set_fader", "Fader", "", "Set selected mixer fader",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerTargetSetPan, "mixer.target.set_pan", "Pan", "", "Set selected mixer pan",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerTargetToggleMute, "mixer.target.toggle_mute", "Mute", "", "Toggle selected mixer mute",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, true },
    { UiActionId::MixerTargetToggleSolo, "mixer.target.toggle_solo", "Solo", "", "Toggle selected mixer solo",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, true },
    { UiActionId::MixerReadMeters, "mixer.meters.read", "Meters", "", "Read mixer meters",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::MixerReadLoudness, "mixer.loudness.read", "Loudness", "", "Read loudness",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::MixerReadSends, "mixer.sends.read", "Sends", "", "Read mixer sends",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::MixerSetFirstSendLevel, "mixer.sends.first.set_level", "Send Level", "", "Set first Track send level",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false },
    { UiActionId::MixerReadFxSlots, "mixer.fx_slots.read", "FX Slots", "", "Read mixer FX slots",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::MixerToggleFirstFxSlotEnabled, "mixer.fx_slots.first.toggle_enabled", "FX On", "", "Toggle first Track FX insert enabled",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, false },
    { UiActionId::MixerReadGainReduction, "mixer.gr.read", "GR", "", "Read mixer gain reduction",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::MixerReadBusFxSlots, "mixer.fx_slots.bus.read", "Bus FX", "", "Read mixer Bus FX slots",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::PianoRollNoteSelect, "piano_roll.note.select", "Select Note", "", "Select piano-roll note",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, false },
    { UiActionId::PianoRollNoteMove, "piano_roll.note.move", "Move Note", "", "Move selected note",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteSetLength, "piano_roll.note.set_length", "Note Length", "", "Set selected note length",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteTranspose, "piano_roll.note.transpose", "Transpose", "Alt+Up", "Transpose selected note",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteQuantize, "piano_roll.note.quantize", "Quantize", "", "Quantize selected note",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollReadExpressionLanes, "piano_roll.expression.read", "Expression", "", "Read MIDI expression lanes",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false, true, false },
    { UiActionId::AutosaveRecoveryRestore, "autosave.recovery.restore", "Restore Autosave", "", "Restore autosave recovery snapshot",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, false, false, false, false, false, false, false, true },
    { UiActionId::AutosaveRecoveryDiscard, "autosave.recovery.discard", "Discard Autosave", "", "Discard autosave recovery snapshot",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, false, false, false, false, false, false, false, true },
    { UiActionId::HelpShowKeymap, "help.show_keymap", "Keymap", "Alt+K", "Show keymap",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::TimelineToolSelectPointer, "timeline.tool.pointer", "Pointer", "1", "Select pointer tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineToolSelectPencil, "timeline.tool.pencil", "Pencil", "2", "Select pencil tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineToolSelectScissors, "timeline.tool.scissors", "Scissors", "3", "Select scissors tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineToolSelectHand, "timeline.tool.hand", "Hand", "", "Select hand tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineToolSelectZoom, "timeline.tool.zoom", "Zoom", "6", "Select zoom tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineSnapDisable, "timeline.snap.disable", "Snap Off", "", "Disable timeline snap",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineSnapSetBar, "timeline.snap.bar", "Snap Bar", "", "Set timeline snap to bar",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineSnapSetBeat, "timeline.snap.beat", "Snap Beat", "", "Set timeline snap to beat",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineSnapSetSixteenth, "timeline.snap.sixteenth", "Snap 1/16", "", "Set timeline snap to sixteenth note",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineAutomationToggleTrackLane, "timeline.automation.track_lane.toggle", "Automation", "A", "Toggle first Track automation lane",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false },
    { UiActionId::TimelineAutomationAddBreakpoint, "timeline.automation.breakpoint.add", "Add Point", "", "Add breakpoint to first Track automation lane",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineAutomationDeleteBreakpoint, "timeline.automation.breakpoint.delete", "Delete Point", "", "Delete breakpoint from first Track automation lane",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineClipDelete, "timeline.clip.delete", "Delete Clip", "Del", "Delete selected timeline clip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, true },
    { UiActionId::TrackAdd, "track.add", "Add Track", "Ctrl+Shift+N", "Add audio track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TrackRename, "track.rename", "Rename Track", "", "Rename track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TrackRemove, "track.remove", "Remove Track", "", "Remove track and its clips",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TrackReorder, "track.reorder", "Reorder Track", "", "Reorder track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::PianoRollNoteAdd, "pianoroll.note.add", "Add Note", "", "Add note to selected MIDI clip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, false, true },
    { UiActionId::PianoRollNoteDelete, "pianoroll.note.delete", "Delete Note", "Del", "Delete selected note",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::MixerFxInsertAdd, "mixer.fx.insert.add", "Add FX", "", "Add FX insert to selected strip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerFxInsertRemove, "mixer.fx.insert.remove", "Remove FX", "", "Remove FX insert from selected strip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerFxInsertToggle, "mixer.fx.insert.toggle", "Bypass FX Slot", "", "Toggle FX insert bypass on selected strip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerFxInsertReorder, "mixer.fx.insert.reorder", "Move FX Slot", "", "Move FX insert within the selected strip chain",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerBusRename, "mixer.bus.rename", "Rename Bus", "", "Rename the selected bus",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerBusRemove, "mixer.bus.remove", "Remove Bus", "", "Remove the selected bus (refused while sends route to it)",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerSendSetTap, "mixer.send.set_tap", "Send Tap", "", "Toggle send pre/post fader tap",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerSendSetDestination, "mixer.send.set_destination", "Send Destination", "", "Re-route send to another bus",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerMasterSetFader, "mixer.master.fader", "Master Fader", "", "Set persisted master gain",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportSetTempo, "transport.set_tempo", "Set Tempo", "", "Set project tempo",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportSetMeter, "transport.set_meter", "Set Time Signature", "", "Set project time signature",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineClipCopy, "timeline.clip.copy", "Copy Clip", "Ctrl+C", "Copy selected timeline clip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipPaste, "timeline.clip.paste", "Paste Clip", "Ctrl+V", "Paste clip at playhead",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineClipDuplicate, "timeline.clip.duplicate", "Duplicate Clip", "Ctrl+D", "Duplicate selected timeline clip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, true },
    { UiActionId::TransportToggleMetronome, "transport.toggle_metronome", "Metronome", "K", "Toggle metronome click",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false },
    { UiActionId::TimelineMarkerAdd, "timeline.marker.add", "Add Marker", "M", "Add marker at playhead",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineMarkerRemove, "timeline.marker.remove", "Remove Marker", "", "Remove nearest marker",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineMidiClipAdd, "timeline.midi_clip.add", "Add MIDI Clip", "", "Add MIDI clip on selected track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::MixerFxInsertParamSet, "mixer.fx.insert.param.set", "Set FX Param", "", "Set FX insert parameter on selected strip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerBusAdd, "mixer.bus.add", "Add Bus", "", "Add a mixer bus",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::MixerSendAdd, "mixer.send.add", "Add Send", "", "Add send from selected track to a bus",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerSendRemove, "mixer.send.remove", "Remove Send", "", "Remove send from selected track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerSendSetLevel, "mixer.send.set_level", "Send Level", "", "Set send level on selected track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::TimelineClipCut, "timeline.clip.cut", "Cut Clip", "Ctrl+X", "Cut selected timeline clip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipSelectAllTrack, "timeline.clip.select_all_track", "Select Track Clips", "Ctrl+Shift+A", "Select all clips on selected track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineClipSelectAllProject, "timeline.clip.select_all_project", "Select Project Clips", "Ctrl+A", "Select all clips in project",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineClipHeal, "timeline.clip.heal", "Heal Clips", "Ctrl+J", "Heal selected clips",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, true },
    { UiActionId::EditNudgeLeft, "edit.nudge_left", "Nudge Left", "Alt+Left", "Nudge selection left",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::EditNudgeRight, "edit.nudge_right", "Nudge Right", "Alt+Right", "Nudge selection right",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::EditNudgeLeftFine, "edit.nudge_left_fine", "Fine Nudge Left", "Alt+Shift+Left", "Fine nudge selection left",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::EditNudgeRightFine, "edit.nudge_right_fine", "Fine Nudge Right", "Alt+Shift+Right", "Fine nudge selection right",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineClipGainIncrease, "timeline.clip.gain_increase", "Clip Gain +1 dB", "Alt+Up", "Increase selected clip gain by one decibel",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipGainDecrease, "timeline.clip.gain_decrease", "Clip Gain -1 dB", "Alt+Down", "Decrease selected clip gain by one decibel",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipApplyDefaultFades, "timeline.clip.apply_default_fades", "Apply Default Fades", "Ctrl+F", "Apply default fades to selected clip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipCrossfade, "timeline.clip.crossfade", "Crossfade Clips", "", "Crossfade two overlapping clips",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, true },
    { UiActionId::EditRenameSelection, "edit.rename_selection", "Rename", "F2", "Rename selected clip or track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineClipRepeatPaste, "timeline.clip.repeat_paste", "Repeat Paste", "Ctrl+R", "Repeat clipboard clips at playhead",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineZoomFitProject, "timeline.zoom.fit_project", "Zoom to Fit Project", "Ctrl+0", "Fit the whole Project in the Timeline",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineZoomFitLoop, "timeline.zoom.fit_loop", "Zoom to Fit Loop", "", "Fit the current loop region in the Timeline",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineZoomIn, "timeline.zoom.in", "Zoom In", "Ctrl+Right", "Zoom the Timeline in at the playhead",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineZoomOut, "timeline.zoom.out", "Zoom Out", "Ctrl+Left", "Zoom the Timeline out at the playhead",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TrackSelectPrevious, "track.select_previous", "Previous Track", "Up", "Select previous Track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TrackSelectNext, "track.select_next", "Next Track", "Down", "Select next Track",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportLocatePreviousGrid, "transport.locate_previous_grid", "Previous Grid", "Left", "Move playhead left by one grid unit",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportLocateNextGrid, "transport.locate_next_grid", "Next Grid", "Right", "Move playhead right by one grid unit",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportLocatePreviousBar, "transport.locate_previous_bar", "Previous Bar", ",", "Move playhead left by one bar",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportLocateNextBar, "transport.locate_next_bar", "Next Bar", ".", "Move playhead right by one bar",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineTogglePlayheadFollow, "timeline.toggle_playhead_follow", "Playhead Follow", "Ctrl+Shift+F", "Toggle playhead follow",
      AccessibilityRole::MenuItem, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::TransportShuttleFaster, "transport.shuttle_faster", "Shuttle Faster", "", "Play or increase shuttle speed",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportShuttleSlower, "transport.shuttle_slower", "Shuttle Slower", "", "Reduce shuttle speed or stop",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportToggleReturnToStartOnStop, "transport.toggle_return_to_start_on_stop", "Return to Start on Stop", "", "Toggle return to playback start on stop",
      AccessibilityRole::MenuItem, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::TransportReturnToZero, "transport.return_to_zero", "Return to Zero", "Enter", "Return transport to timeline zero",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportPlayFromLastLocate, "transport.play_from_last_locate", "Play from Last Locate", "Shift+Space", "Play from last locate point",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportToggleRecordCountIn, "transport.toggle_record_count_in", "Count-in for Record", "Shift+K", "Toggle one-bar count-in before recording",
      AccessibilityRole::MenuItem, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::TransportStoreLocatePoint1, "transport.locate_point.store.1", "Store Locate 1", "", "Store playhead in locate point 1",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportStoreLocatePoint2, "transport.locate_point.store.2", "Store Locate 2", "", "Store playhead in locate point 2",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportStoreLocatePoint3, "transport.locate_point.store.3", "Store Locate 3", "", "Store playhead in locate point 3",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportStoreLocatePoint4, "transport.locate_point.store.4", "Store Locate 4", "", "Store playhead in locate point 4",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportStoreLocatePoint5, "transport.locate_point.store.5", "Store Locate 5", "", "Store playhead in locate point 5",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportRecallLocatePoint1, "transport.locate_point.recall.1", "Recall Locate 1", "", "Recall playhead from locate point 1",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportRecallLocatePoint2, "transport.locate_point.recall.2", "Recall Locate 2", "", "Recall playhead from locate point 2",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportRecallLocatePoint3, "transport.locate_point.recall.3", "Recall Locate 3", "", "Recall playhead from locate point 3",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportRecallLocatePoint4, "transport.locate_point.recall.4", "Recall Locate 4", "", "Recall playhead from locate point 4",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportRecallLocatePoint5, "transport.locate_point.recall.5", "Recall Locate 5", "", "Recall playhead from locate point 5",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportLocatePreviousMarker, "transport.locate_previous_marker", "Previous Marker", "Alt+,", "Move playhead to the previous Marker",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportLocateNextMarker, "transport.locate_next_marker", "Next Marker", "Alt+.", "Move playhead to the next Marker",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineRangeToLoop, "timeline.range_to_loop", "Range To Loop", "Ctrl+U", "Convert the ruler range selection to the loop region",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineRangeSplitEdges, "timeline.range.split_edges", "Split at Selection Edges", "Ctrl+E", "Split every clip at the Time selection's edges, on all tracks",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineRangeCut, "timeline.range.cut", "Cut Selection", "", "Cut what lies inside the Time selection to the clipboard, on all tracks",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineRangeCopy, "timeline.range.copy", "Copy Selection", "", "Copy what lies inside the Time selection to the clipboard, on all tracks",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineRangeDelete, "timeline.range.delete", "Delete Selection", "", "Remove what lies inside the Time selection, on all tracks",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineRangeSilence, "timeline.range.silence", "Silence Selection", "", "Silence the Time selection: remove its audio and keep its time, on all tracks",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineZoomToSelection, "timeline.zoom.selection", "Zoom to Selection", "Z", "Fit the Time selection in the Timeline",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineSelectAllFollowing, "timeline.select.following", "Select All Following", "Shift+F", "Select everything from the playhead to the end, on all tracks",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::EditModeOverlap, "edit.mode.overlap", "Overlap", "", "Edit mode: neighbours stay put when a Clip is placed or removed",
      AccessibilityRole::MenuItem, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::EditModeNoOverlap, "edit.mode.no_overlap", "No Overlap", "", "Edit mode: a placed Clip trims what it covers",
      AccessibilityRole::MenuItem, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::EditModeShuffle, "edit.mode.shuffle", "Shuffle", "", "Edit mode: neighbours close up on a removal and move aside on a placement",
      AccessibilityRole::MenuItem, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::TrackDuplicate, "track.duplicate", "Duplicate Track", "", "Duplicate selected track with clips and strip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TrackMoveUp, "track.move_up", "Move Track Up", "", "Move selected track up one row",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TrackMoveDown, "track.move_down", "Move Track Down", "", "Move selected track down one row",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TrackToggleMute, "track.toggle_mute", "Mute Track", "Shift+M", "Toggle mute on selected track",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false },
    { UiActionId::TrackToggleSolo, "track.toggle_solo", "Solo Track", "Shift+S", "Toggle solo on selected track",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false },
    { UiActionId::TrackToggleArm, "record.track.toggle_arm", "Arm Selected Track", "Shift+R", "Toggle recording arm on selected track",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, false, false, false, true, false, false, true },
    { UiActionId::PianoRollNoteSetVelocity, "piano_roll.note.set_velocity", "Note Velocity", "", "Set selected note velocity",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteOctaveUp, "piano_roll.note.octave_up", "Octave Up", "Alt+Shift+Up", "Transpose selected notes up one octave",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteOctaveDown, "piano_roll.note.octave_down", "Octave Down", "Alt+Shift+Down", "Transpose selected notes down one octave",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteDuplicate, "piano_roll.note.duplicate", "Duplicate Note", "", "Duplicate selected note one grid step later",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteQuantizeSelection, "piano_roll.note.quantize_selection", "Quantize Notes", "Q", "Quantize selected notes to the snap grid",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::MixerTrackSetOutput, "mixer.track.output", "Track Output", "", "Route the selected Track's main output to master or a bus",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::TimelineToggleMixerDock, "timeline.mixer_dock.toggle", "Mixer Dock", "X", "Show or hide the mixer in the editor dock",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::InspectorShowClipTab, "inspector.tab.clip", "Clip", "", "Show the clip inspector tab",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::InspectorShowTrackTab, "inspector.tab.track", "Track", "", "Show the track inspector tab",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::MixerTargetToggleSoloSafe, "mixer.target.toggle_solo_safe", "Solo Safe", "", "Toggle solo-safe on the selected mixer strip",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, true },
    // G0.2: Space = play/stop toggle (Logic, Pro Tools, everyone).
    { UiActionId::TransportTogglePlayStop, "transport.toggle_play_stop", "Play/Stop", "Space", "Toggle play and stop",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    // G0.7: the collapsible Audio & Export settings row under the toolbar (Options menu).
    { UiActionId::ViewToggleSettingsRow, "view.toggle_settings_row", "Audio & Export Settings", "", "Show or hide the audio and export settings row",
      AccessibilityRole::MenuItem, UiActionKind::Toggle, false, false, false, false },
    // G1.1 (§4 "select all in the focused editor"): every note of the selected MIDI clip.
    { UiActionId::PianoRollNoteSelectAll, "piano_roll.note.select_all", "Select All Notes", "Ctrl+A", "Select every note in the selected MIDI clip",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false, false, true },
    // G1.4 (§4.3): the inspector shows and hides on I.
    { UiActionId::ViewToggleInspector, "view.toggle_inspector", "Inspector", "I", "Show or hide the inspector",
      AccessibilityRole::MenuItem, UiActionKind::Toggle, false, false, false, false },
    // G1.4: the Nudge value — the distance the nudge keys move the selection (CONTEXT: never
    // hard-wired to the snap grid). Four verbs, one chooser, no default chords.
    { UiActionId::EditNudgeValueGrid, "edit.nudge.value.grid", "Nudge: Grid", "", "Nudge by the snap grid",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::EditNudgeValueBar, "edit.nudge.value.bar", "Nudge: Bar", "", "Nudge by one bar",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::EditNudgeValueBeat, "edit.nudge.value.beat", "Nudge: Beat", "", "Nudge by one beat",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::EditNudgeValueSixteenth, "edit.nudge.value.sixteenth", "Nudge: 1/16", "", "Nudge by a sixteenth",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false }
}};

// G0.8: no Refresh / Test Device buttons in the shell. Refresh lives in the Options menu; the
// test device is a harness-only verb (no chord, no control) so fake provenance cannot be stamped
// from the UI.
inline constexpr std::array<UiActionId, 15> kMainShellToolbarActions {{
    UiActionId::ProjectNew,
    UiActionId::ProjectOpen,
    UiActionId::ProjectSave,
    UiActionId::ProjectImportAudio,
    UiActionId::RecordingArmTrack,
    UiActionId::RecordingSetMonitoringPolicy,
    UiActionId::TransportRecord,
    UiActionId::RecordingAssembleComp,
    UiActionId::EditUndo,
    UiActionId::EditRedo,
    UiActionId::TransportPlay,
    UiActionId::TransportStop,
    UiActionId::TransportLocateStart,
    UiActionId::TransportToggleLoop,
    UiActionId::ViewPianoRoll   // G2.1 cp3: the Mixer button retired — the cluster's X (the dock toggle) is the control
}};

inline const std::array<UiActionDescriptor, kUiActionCount>& uiActionDescriptors()
{
    return kUiActionDescriptors;
}

inline const std::array<UiActionId, 15>& mainShellToolbarActions()
{
    return kMainShellToolbarActions;
}

inline bool isKnownAction (UiActionId id)
{
    return actionIndex (id) < kUiActionCount;
}

inline const UiActionDescriptor* descriptorFor (UiActionId id)
{
    if (! isKnownAction (id))
        return nullptr;
    return &kUiActionDescriptors[actionIndex (id)];
}

inline const UiActionDescriptor* descriptorForStableId (std::string_view stableId)
{
    for (const auto& descriptor : kUiActionDescriptors)
        if (descriptor.stableId == stableId)
            return &descriptor;
    return nullptr;
}

inline const char* roleName (AccessibilityRole role)
{
    switch (role)
    {
        case AccessibilityRole::Button: return "button";
        case AccessibilityRole::ToggleButton: return "toggle_button";
        case AccessibilityRole::MenuItem: return "menu_item";
        case AccessibilityRole::Panel: return "panel";
    }
    return "unknown";
}

inline constexpr UiRecordingMonitoringPolicy nextRecordingMonitoringPolicy (
    UiRecordingMonitoringPolicy policy) noexcept
{
    switch (policy)
    {
        case UiRecordingMonitoringPolicy::Unselected:
        case UiRecordingMonitoringPolicy::Off:
            return UiRecordingMonitoringPolicy::DirectInput;
        case UiRecordingMonitoringPolicy::DirectInput:
            return UiRecordingMonitoringPolicy::LatencyCompensated;
        case UiRecordingMonitoringPolicy::LatencyCompensated:
            return UiRecordingMonitoringPolicy::Off;
    }

    return UiRecordingMonitoringPolicy::DirectInput;
}

class Keymap
{
public:
    Keymap()
    {
        for (const auto& descriptor : kUiActionDescriptors)
            chords_[actionIndex (descriptor.id)] = descriptor.defaultKey;
    }

    const std::string& chordFor (UiActionId id) const
    {
        static const std::string empty;
        if (! isKnownAction (id))
            return empty;
        return chords_[actionIndex (id)];
    }

    // G1.1: chord aliases — a second spelling that means the canonical chord everywhere
    // (Ctrl+Y for Redo on Windows; Return and Enter are one key).
    struct ChordAlias
    {
        const char* alias;
        const char* canonical;
    };
    static constexpr std::array<ChordAlias, 3> kChordAliases {{
        { "Ctrl+Y", "Ctrl+Shift+Z" },
        { "Return", "Enter" },
        { "Backspace", "Del" },   // §4: Delete / Backspace are one verb
    }};

    [[nodiscard]] static std::string_view canonicalChord (std::string_view chord) noexcept
    {
        for (const ChordAlias& alias : kChordAliases)
            if (chord == alias.alias)
                return alias.canonical;
        return chord;
    }

    [[nodiscard]] static UiFocusContext contextOf (UiActionId id) noexcept
    {
        return defaultFocusContext (id);
    }

    // The action a chord dispatches in a Focus context: that context's own binding first, then
    // a Global one; another context's binding never fires. Aliases resolve first.
    UiActionId actionForChord (std::string_view chord, UiFocusContext focus) const
    {
        if (chord.empty())
            return UiActionId::Count;
        const std::string_view canonical = canonicalChord (chord);

        UiActionId global = UiActionId::Count;
        for (std::size_t i = 0; i < chords_.size(); ++i)
        {
            if (chords_[i] != canonical)
                continue;
            const auto id = static_cast<UiActionId> (i);
            const UiFocusContext context = contextOf (id);
            if (context == focus)
                return id;
            if (context == UiFocusContext::Global && global == UiActionId::Count)
                global = id;
        }
        return global;
    }

    // Context-free lookup (the pre-G1.1 law): the first action carrying the chord in any context.
    UiActionId actionForChord (std::string_view chord) const
    {
        if (chord.empty())
            return UiActionId::Count;
        const std::string_view canonical = canonicalChord (chord);
        for (std::size_t i = 0; i < chords_.size(); ++i)
            if (chords_[i] == canonical)
                return static_cast<UiActionId> (i);
        return UiActionId::Count;
    }

    // Two bindings conflict when they share a chord within one context, or one of them is Global.
    [[nodiscard]] static bool contextsConflict (UiFocusContext a, UiFocusContext b) noexcept
    {
        return a == b || a == UiFocusContext::Global || b == UiFocusContext::Global;
    }

    KeymapRebindStatus rebind (UiActionId id, std::string_view chord)
    {
        if (! isKnownAction (id))
            return KeymapRebindStatus::UnknownAction;
        if (chord.empty())
            return KeymapRebindStatus::EmptyChord;

        const std::string_view canonical = canonicalChord (chord);
        for (std::size_t i = 0; i < chords_.size(); ++i)
        {
            const auto other = static_cast<UiActionId> (i);
            if (other != id && chords_[i] == canonical && contextsConflict (contextOf (id), contextOf (other)))
                return KeymapRebindStatus::DuplicateChord;
        }

        chords_[actionIndex (id)] = std::string (canonical);
        return KeymapRebindStatus::Ok;
    }

    // G1.5: the action that blocks binding `chord` to `id` in id's context (Count = none).
    [[nodiscard]] UiActionId conflictingAction (UiActionId id, std::string_view chord) const
    {
        const std::string_view canonical = canonicalChord (chord);
        for (std::size_t i = 0; i < chords_.size(); ++i)
        {
            const auto other = static_cast<UiActionId> (i);
            if (other != id && chords_[i] == canonical && contextsConflict (contextOf (id), contextOf (other)))
                return other;
        }
        return UiActionId::Count;
    }

    // G1.5: an action may carry no chord at all (the editor's Unbind).
    void unbind (UiActionId id)
    {
        if (isKnownAction (id))
            chords_[actionIndex (id)].clear();
    }

    [[nodiscard]] bool isDefault (UiActionId id) const
    {
        const UiActionDescriptor* descriptor = descriptorFor (id);
        return descriptor != nullptr && chords_[actionIndex (id)] == descriptor->defaultKey;
    }

    // G1.1 gate law: every pair of bound actions that would conflict. Empty = the keymap is sound.
    struct Conflict
    {
        UiActionId first;
        UiActionId second;
    };

    [[nodiscard]] std::vector<Conflict> conflicts() const
    {
        std::vector<Conflict> out;
        for (std::size_t i = 0; i < chords_.size(); ++i)
        {
            if (chords_[i].empty())
                continue;
            for (std::size_t j = i + 1; j < chords_.size(); ++j)
                if (chords_[i] == chords_[j]
                    && contextsConflict (contextOf (static_cast<UiActionId> (i)), contextOf (static_cast<UiActionId> (j))))
                    out.push_back ({ static_cast<UiActionId> (i), static_cast<UiActionId> (j) });
        }
        return out;
    }

    // G1.1: the keymap as the markdown table docs/keymap-v2.md carries — generated, never typed,
    // so the document and the code cannot drift ([keymap-v2] compares byte for byte).
    [[nodiscard]] std::string renderMarkdown() const
    {
        std::string out;
        out += "# YES DAW keymap v2\n\n";
        out += "Generated by `YesDawKeymapDoc` from the action descriptors (plan §4; G1.1). Do not edit by hand:\n";
        out += "run the tool and commit the result. `Ctrl+Y` is an alias of `Ctrl+Shift+Z`; `Return` of `Enter`;\n";
        out += "the numpad's digits and operators spell the same chords as the main keys.\n\n";
        for (const UiFocusContext context : { UiFocusContext::Global, UiFocusContext::Arrange,
                                              UiFocusContext::PianoRoll, UiFocusContext::Mixer })
        {
            out += std::string ("## ") + focusContextName (context) + "\n\n";
            out += "| Chord | Action | Stable id |\n|---|---|---|\n";
            for (const auto& descriptor : kUiActionDescriptors)
            {
                if (defaultFocusContext (descriptor.id) != context)
                    continue;
                const std::string& chord = chords_[actionIndex (descriptor.id)];
                if (chord.empty())
                    continue;
                out += "| `" + chord + "` | " + descriptor.label + " | `" + descriptor.stableId + "` |\n";
            }
            out += "\n";
        }
        out += "## No default chord\n\n";
        for (const auto& descriptor : kUiActionDescriptors)
            if (chords_[actionIndex (descriptor.id)].empty())
                out += std::string ("- ") + descriptor.label + " (`" + descriptor.stableId + "`)\n";
        return out;
    }

private:
    std::array<std::string, kUiActionCount> chords_ {};
};

class UiActionRegistry
{
public:
    const std::array<UiActionDescriptor, kUiActionCount>& actions() const
    {
        return kUiActionDescriptors;
    }

    const Keymap& keymap() const { return keymap_; }
    Keymap& keymap() { return keymap_; }

    const UiActionDescriptor* descriptor (UiActionId id) const
    {
        return descriptorFor (id);
    }

    UiActionState stateFor (UiActionId id, const UiActionContext& context) const
    {
        const UiActionDescriptor* descriptor = descriptorFor (id);
        if (descriptor == nullptr)
            return { false, "unknown action" };

        // G0.8: the verb stays registered but disabled with its reason until G2.9 wires the
        // TimeStretchNode into the projection and the renderer (today it is a trim in disguise).
        if (id == UiActionId::TimelineClipTimeStretch)
            return { false, "coming in G2.9: the time-stretch node is not wired yet" };
        if (id == UiActionId::MixerSetFirstSendLevel && context.projectLoaded && ! context.firstTrackSendAvailable)
            return { false, "no send on the first Track" };
        if (id == UiActionId::MixerToggleFirstFxSlotEnabled && context.projectLoaded && ! context.firstTrackFxSlotAvailable)
            return { false, "no FX in the first Track's first slot" };

        if (descriptor->requiresProject && ! context.projectLoaded)
            return { false, "no project loaded" };
        if (descriptor->requiresUndo && ! context.canUndo)
            return { false, "nothing to undo" };
        if (descriptor->requiresRedo && ! context.canRedo)
            return { false, "nothing to redo" };
        if (descriptor->requiresTimelineClip && ! context.timelineClipSelected)
            return { false, "no clip selected" };
        if (descriptor->requiresMixerTarget && ! context.mixerTargetSelected)
            return { false, "no mixer target selected" };
        if (descriptor->requiresMidiClip && ! context.midiClipSelected)
            return { false, "no MIDI clip selected" };
        if (descriptor->requiresMidiNote && ! context.midiNoteSelected)
            return { false, "no MIDI note selected" };
        if (descriptor->requiresRecordingDevice && ! context.recordingDeviceSelected)
            return { false, "no recording device selected" };
        if (descriptor->requiresRecordingTrackAvailable && ! context.recordingTrackAvailable)
            return { false, "no recording Track available" };
        if (descriptor->requiresRecordingTrackInput
            && (! context.recordingTrackArmed || ! context.recordingInputSelected))
            return { false, "no armed recording Track/input" };
        if (descriptor->requiresRecordingMonitoring && ! context.recordingMonitoringSelected)
            return { false, "no recording monitoring policy" };
        if (descriptor->requiresRecordingCompTakes && ! context.recordingCompTakesAvailable)
            return { false, "not enough recording Takes" };
        if (descriptor->requiresAutosaveRecovery && ! context.autosaveRecoveryPending)
            return { false, "no autosave recovery snapshot" };
        if ((id == UiActionId::TimelineClipPaste || id == UiActionId::TimelineClipRepeatPaste)
            && ! context.clipboardHasClip)
            return { false, "clipboard has no clip" };
        if (id == UiActionId::TimelineZoomFitLoop && ! context.loopEnabled)
            return { false, "no loop region" };
        if (id == UiActionId::TimelineRangeToLoop && ! context.timelineRangeSelected)
            return { false, "no ruler range selection" };
        if ((id == UiActionId::TimelineRangeSplitEdges || id == UiActionId::TimelineRangeCut
             || id == UiActionId::TimelineRangeCopy || id == UiActionId::TimelineRangeDelete
             || id == UiActionId::TimelineRangeSilence || id == UiActionId::TimelineZoomToSelection)
            && ! context.timelineRangeSelected)
            return { false, "no Time selection" };   // G2.5
        if (const int index = recallLocatePointIndex (id);
            index >= 0 && ! context.locatePoints[static_cast<std::size_t> (index)].has_value())
            return { false, "locate point is empty" };

        if (id == UiActionId::EditNudgeLeft
            || id == UiActionId::EditNudgeRight
            || id == UiActionId::EditNudgeLeftFine
            || id == UiActionId::EditNudgeRightFine)
        {
            if (context.activePanel == UiPanel::Timeline && ! context.timelineClipSelected)
                return { false, "no clip selected" };
            if (context.activePanel == UiPanel::PianoRoll && ! context.midiNoteSelected)
                return { false, "no MIDI note selected" };
            if (context.activePanel == UiPanel::Mixer)
                return { false, "nudge unavailable in mixer" };
        }

        return { true, "" };
    }

    UiActionDispatchResult dispatch (UiActionId id, UiActionContext& context) const
    {
        const UiActionState state = stateFor (id, context);
        if (! state.enabled)
            return { id, state, false };

        switch (id)
        {
            case UiActionId::ProjectNew:
            case UiActionId::ProjectOpen:
                context.projectLoaded = true;
                context.isPlaying = false;
                context.loopEnabled = false;
                context.timelineRangeSelected = false;
                context.playheadFrame = 0;
                context.playbackStartFrame = 0;
                context.lastLocateFrame = 0;
                context.shuttlePlaybackRate = 1;
                context.activePanel = UiPanel::Timeline;
                context.canUndo = false;
                context.canRedo = false;
                context.timelineAutomationTrackLaneVisible = false;
                context.timelineAutomationTrackIndex = -1;
                break;

            case UiActionId::ProjectSave:
            case UiActionId::ProjectSaveAs:
                ++context.saveCount;
                break;

            case UiActionId::ProjectImportAudio:
                ++context.importCount;
                break;

            case UiActionId::ProjectExportAudio:
                ++context.audioExportCount;
                break;

            case UiActionId::ProjectExportAudioCancel:
                if (context.audioExportInProgress)
                {
                    context.audioExportCancelRequested = true;
                    context.audioExportInProgress = false;
                    ++context.audioExportCancelCount;
                }
                else
                {
                    context.activePanel = UiPanel::Timeline;
                    context.activeTimelineTool = TimelineTool::Pointer;
                }
                break;

            case UiActionId::ProjectExportDawproject:
                ++context.dawprojectExportCount;
                break;

            case UiActionId::TransportPlay:
                if (! context.isPlaying)
                    context.playbackStartFrame = context.playheadFrame;
                context.isPlaying = true;
                context.shuttlePlaybackRate = 1;
                break;

            case UiActionId::TransportStop:
                context.isPlaying = false;
                context.recordCountInActive = false;
                if (context.returnToStartOnStopEnabled)
                    context.playheadFrame = context.playbackStartFrame;
                context.shuttlePlaybackRate = 1;
                break;

            case UiActionId::TransportTogglePlayStop:
                // G0.2: exactly the Play or the Stop law above, chosen by the current state.
                if (context.isPlaying)
                {
                    context.isPlaying = false;
                    context.recordCountInActive = false;
                    if (context.returnToStartOnStopEnabled)
                        context.playheadFrame = context.playbackStartFrame;
                }
                else
                {
                    context.playbackStartFrame = context.playheadFrame;
                    context.isPlaying = true;
                }
                context.shuttlePlaybackRate = 1;
                break;

            case UiActionId::TransportLocateStart:
            case UiActionId::TransportReturnToZero:
                context.playheadFrame = 0;
                context.lastLocateFrame = 0;
                break;

            case UiActionId::TransportPlayFromLastLocate:
                context.playheadFrame = context.lastLocateFrame;
                context.playbackStartFrame = context.lastLocateFrame;
                context.isPlaying = true;
                context.shuttlePlaybackRate = 1;
                break;

            case UiActionId::TransportToggleLoop:
                context.loopEnabled = ! context.loopEnabled;
                break;

            case UiActionId::DeviceRefreshAudio:
                ++context.recordingDeviceGeneration;
                ++context.deviceRefreshCount;
                break;

            case UiActionId::DeviceSelectTestAudio:
                context.recordingDeviceSelected = true;
                context.selectedRecordingDeviceId = 1u;
                if (context.recordingDeviceGeneration == 0u)
                    context.recordingDeviceGeneration = 1u;
                ++context.deviceSelectCount;
                break;

            case UiActionId::RecordingArmTrack:
            case UiActionId::TrackToggleArm:
                context.recordingTrackArmed = ! context.recordingTrackArmed;
                context.recordingInputSelected = context.recordingTrackArmed;
                context.selectedRecordingTrackIndex = context.recordingTrackArmed ? 0 : -1;
                context.selectedRecordingInputChannel = context.recordingTrackArmed ? 0 : -1;
                if (! context.recordingTrackArmed)
                    context.isRecording = false;
                ++context.recordingArmCount;
                break;

            case UiActionId::RecordingSetMonitoringPolicy:
                context.selectedRecordingMonitoringPolicy =
                    nextRecordingMonitoringPolicy (context.selectedRecordingMonitoringPolicy);
                context.recordingMonitoringSelected =
                    context.selectedRecordingMonitoringPolicy != UiRecordingMonitoringPolicy::Unselected;
                ++context.recordingMonitoringCount;
                break;

            case UiActionId::TransportRecord:
                if (context.recordCountInActive)
                {
                    context.recordCountInActive = false;
                    context.isPlaying = false;
                }
                else if (context.recordCountInEnabled && ! context.isRecording)
                {
                    context.recordCountInActive = true;
                    context.isPlaying = true;
                }
                else
                {
                    context.isRecording = ! context.isRecording;
                }
                ++context.recordingCommandCount;
                break;

            case UiActionId::RecordingAssembleComp:
                context.recordingCompSelected = true;
                context.recordingCompSegmentCount = 2;
                ++context.recordingCompCommandCount;
                context.canUndo = true;
                context.canRedo = false;
                break;

            case UiActionId::EditUndo:
                context.canUndo = false;
                context.canRedo = true;
                ++context.undoCount;
                break;

            case UiActionId::EditRedo:
                context.canRedo = false;
                context.canUndo = true;
                ++context.redoCount;
                break;

            case UiActionId::ViewTimeline:
                context.activePanel = UiPanel::Timeline;
                break;

            // G2.1 cp2: the mixer and the piano roll are Editor-dock TABS, not modal views. Mixer
            // shows the mixer tab (idempotent, the menu verb); X toggles it; P toggles the piano
            // roll tab (Logic). The Focus context follows the tab shown and returns to the
            // arrangement when the dock closes on the focused editor.
            case UiActionId::ViewMixer:
                context.mixerDockVisible = true;
                context.editorDockTab = UiEditorDockTab::Mixer;
                context.activePanel = UiPanel::Mixer;
                break;

            case UiActionId::ViewPianoRoll:
                if (context.mixerDockVisible && context.editorDockTab == UiEditorDockTab::PianoRoll)
                {
                    context.mixerDockVisible = false;
                    if (context.activePanel == UiPanel::PianoRoll)
                        context.activePanel = UiPanel::Timeline;
                }
                else
                {
                    context.mixerDockVisible = true;
                    context.editorDockTab = UiEditorDockTab::PianoRoll;
                    context.activePanel = UiPanel::PianoRoll;
                }
                break;

            case UiActionId::TimelineClipMove:
            case UiActionId::TimelineClipTrim:
            case UiActionId::TimelineClipSplit:
            case UiActionId::TimelineClipHeal:
            case UiActionId::TimelineClipSetGain:
            case UiActionId::TimelineClipGainIncrease:
            case UiActionId::TimelineClipGainDecrease:
            case UiActionId::TimelineClipSetFades:
            case UiActionId::TimelineClipApplyDefaultFades:
            case UiActionId::TimelineClipCrossfade:
            case UiActionId::TimelineClipTimeStretch:
                context.activePanel = UiPanel::Timeline;
                context.canUndo = true;
                context.canRedo = false;
                ++context.timelineEditCount;
                break;

            case UiActionId::MixerTargetSetFader:
            case UiActionId::MixerTargetSetPan:
            case UiActionId::MixerTargetToggleMute:
            case UiActionId::MixerTargetToggleSolo:
            case UiActionId::MixerTargetToggleSoloSafe:
            case UiActionId::MixerSetFirstSendLevel:
            case UiActionId::MixerToggleFirstFxSlotEnabled:
                context.activePanel = UiPanel::Mixer;
                context.canUndo = true;
                context.canRedo = false;
                ++context.mixerEditCount;
                break;

            // Selected-track strip toggles: the same strip edit as the mixer controls, but
            // panel-preserving — the active panel stays wherever the user is working.
            case UiActionId::TrackToggleMute:
            case UiActionId::TrackToggleSolo:
                context.canUndo = true;
                context.canRedo = false;
                ++context.mixerEditCount;
                break;

            case UiActionId::MixerReadMeters:
            case UiActionId::MixerReadLoudness:
            case UiActionId::MixerReadSends:
            case UiActionId::MixerReadFxSlots:
            case UiActionId::MixerReadGainReduction:
            case UiActionId::MixerReadBusFxSlots:
                context.activePanel = UiPanel::Mixer;
                ++context.mixerReadCount;
                break;

            case UiActionId::PianoRollNoteSelect:
                context.activePanel = UiPanel::PianoRoll;
                context.midiNoteSelected = true;
                break;

            case UiActionId::PianoRollNoteMove:
            case UiActionId::PianoRollNoteSetLength:
            case UiActionId::PianoRollNoteTranspose:
            case UiActionId::PianoRollNoteQuantize:
            case UiActionId::PianoRollNoteSetVelocity:
            case UiActionId::PianoRollNoteOctaveUp:
            case UiActionId::PianoRollNoteOctaveDown:
            case UiActionId::PianoRollNoteDuplicate:
            case UiActionId::PianoRollNoteQuantizeSelection:
                context.activePanel = UiPanel::PianoRoll;
                context.canUndo = true;
                context.canRedo = false;
                ++context.midiEditCount;
                break;

            case UiActionId::PianoRollReadExpressionLanes:
                context.activePanel = UiPanel::PianoRoll;
                ++context.midiReadCount;
                break;

            case UiActionId::PianoRollNoteSelectAll:
                context.activePanel = UiPanel::PianoRoll;
                break;

            case UiActionId::AutosaveRecoveryRestore:
                context.autosaveRecoveryPending = false;
                ++context.autosaveRecoveryRestoreCount;
                break;

            case UiActionId::AutosaveRecoveryDiscard:
                context.autosaveRecoveryPending = false;
                ++context.autosaveRecoveryDiscardCount;
                break;

            case UiActionId::HelpShowKeymap:
                context.keymapVisible = ! context.keymapVisible;
                break;

            // Tool selection is panel-preserving (E11): the palette drives the piano roll's
            // gestures too, so picking a tool must not kick the user out of the roll.
            case UiActionId::TimelineToolSelectPointer:
                context.activeTimelineTool = TimelineTool::Pointer;
                break;

            case UiActionId::TimelineToolSelectPencil:
                context.activeTimelineTool = TimelineTool::Pencil;
                break;

            case UiActionId::TimelineToolSelectScissors:
                context.activeTimelineTool = TimelineTool::Scissors;
                break;

            case UiActionId::TimelineToolSelectHand:
                context.activeTimelineTool = TimelineTool::Hand;
                break;

            case UiActionId::TimelineToolSelectZoom:
                context.activeTimelineTool = TimelineTool::Zoom;
                break;

            case UiActionId::TimelineZoomFitProject:
            case UiActionId::TimelineZoomFitLoop:
            case UiActionId::TimelineZoomIn:
            case UiActionId::TimelineZoomOut:
                context.activePanel = UiPanel::Timeline;
                break;

            case UiActionId::TrackSelectPrevious:
            case UiActionId::TrackSelectNext:
            case UiActionId::TransportLocatePreviousGrid:
            case UiActionId::TransportLocateNextGrid:
            case UiActionId::TransportLocatePreviousBar:
            case UiActionId::TransportLocateNextBar:
            case UiActionId::TransportLocatePreviousMarker:
            case UiActionId::TransportLocateNextMarker:
                break;

            case UiActionId::TimelineTogglePlayheadFollow:
                context.playheadFollowEnabled = ! context.playheadFollowEnabled;
                break;

            case UiActionId::TransportShuttleFaster:
                if (! context.isPlaying)
                {
                    context.playbackStartFrame = context.playheadFrame;
                    context.isPlaying = true;
                    context.shuttlePlaybackRate = 1;
                }
                else
                {
                    context.shuttlePlaybackRate = std::min (4, context.shuttlePlaybackRate * 2);
                }
                break;

            case UiActionId::TransportShuttleSlower:
                if (context.isPlaying && context.shuttlePlaybackRate > 1)
                    context.shuttlePlaybackRate /= 2;
                else
                {
                    context.isPlaying = false;
                    if (context.returnToStartOnStopEnabled)
                        context.playheadFrame = context.playbackStartFrame;
                    context.shuttlePlaybackRate = 1;
                }
                break;

            case UiActionId::TransportToggleReturnToStartOnStop:
                context.returnToStartOnStopEnabled = ! context.returnToStartOnStopEnabled;
                break;

            // The snap chooser is panel-preserving (E12): it now governs piano-roll note
            // gestures too, so changing the grid must not kick the user out of the roll.
            case UiActionId::TimelineSnapDisable:
                context.snapEnabled = false;
                break;

            case UiActionId::TimelineSnapSetBar:
                context.snapEnabled = true;
                context.snapGridTicks = 2048;
                break;

            case UiActionId::TimelineSnapSetBeat:
                context.snapEnabled = true;
                context.snapGridTicks = 512;
                break;

            case UiActionId::TimelineSnapSetSixteenth:
                context.snapEnabled = true;
                context.snapGridTicks = 128;
                break;

            case UiActionId::TimelineAutomationToggleTrackLane:
                context.activePanel = UiPanel::Timeline;
                context.timelineAutomationTrackLaneVisible = ! context.timelineAutomationTrackLaneVisible;
                context.timelineAutomationTrackIndex = context.timelineAutomationTrackLaneVisible ? 0 : -1;
                ++context.timelineAutomationShowHideCount;
                break;

            case UiActionId::TimelineToggleMixerDock:
                if (context.mixerDockVisible && context.editorDockTab == UiEditorDockTab::Mixer)
                {
                    context.mixerDockVisible = false;
                    if (context.activePanel == UiPanel::Mixer)
                        context.activePanel = UiPanel::Timeline;
                }
                else
                {
                    context.mixerDockVisible = true;
                    context.editorDockTab = UiEditorDockTab::Mixer;
                    context.activePanel = UiPanel::Mixer;
                }
                break;

            case UiActionId::InspectorShowClipTab:
                context.inspectorTrackTabActive = false;
                break;

            case UiActionId::InspectorShowTrackTab:
                context.inspectorTrackTabActive = true;
                break;

            case UiActionId::TimelineAutomationAddBreakpoint:
            case UiActionId::TimelineAutomationDeleteBreakpoint:
                context.activePanel = UiPanel::Timeline;
                context.canUndo = true;
                context.canRedo = false;
                ++context.timelineAutomationBreakpointEditCount;
                break;

            case UiActionId::TimelineClipDelete:
                context.timelineClipSelected = false;
                context.canUndo = true;
                context.canRedo = false;
                ++context.timelineEditCount;
                break;

            case UiActionId::TrackAdd:
            case UiActionId::TrackRename:
            case UiActionId::TrackRemove:
            case UiActionId::TrackReorder:
            case UiActionId::TrackDuplicate:
            case UiActionId::TrackMoveUp:
            case UiActionId::TrackMoveDown:
                context.canUndo = true;
                context.canRedo = false;
                ++context.trackEditCount;
                break;

            case UiActionId::EditRenameSelection:
                context.canUndo = true;
                context.canRedo = false;
                if (context.timelineClipSelected)
                    ++context.timelineEditCount;
                else
                    ++context.trackEditCount;
                break;

            case UiActionId::PianoRollNoteAdd:
                context.activePanel = UiPanel::PianoRoll;
                context.midiNoteSelected = true;
                context.canUndo = true;
                context.canRedo = false;
                ++context.midiEditCount;
                break;

            case UiActionId::PianoRollNoteDelete:
                context.activePanel = UiPanel::PianoRoll;
                context.midiNoteSelected = false;
                context.canUndo = true;
                context.canRedo = false;
                ++context.midiEditCount;
                break;

            case UiActionId::MixerFxInsertAdd:
            case UiActionId::MixerFxInsertRemove:
            case UiActionId::MixerFxInsertToggle:
            case UiActionId::MixerFxInsertReorder:
            case UiActionId::MixerBusRename:
            case UiActionId::MixerBusRemove:
            case UiActionId::MixerSendSetTap:
            case UiActionId::MixerSendSetDestination:
            case UiActionId::MixerMasterSetFader:
            case UiActionId::MixerFxInsertParamSet:
            case UiActionId::MixerBusAdd:
            case UiActionId::MixerSendAdd:
            case UiActionId::MixerSendRemove:
            case UiActionId::MixerSendSetLevel:
            case UiActionId::MixerTrackSetOutput:
                context.activePanel = UiPanel::Mixer;
                context.canUndo = true;
                context.canRedo = false;
                ++context.mixerEditCount;
                break;

            case UiActionId::TransportSetTempo:
            case UiActionId::TransportSetMeter:
                context.canUndo = true;
                context.canRedo = false;
                ++context.timelineEditCount;
                break;

            case UiActionId::TimelineClipCopy:
                context.clipboardHasClip = true;
                break;

            case UiActionId::TimelineClipSelectAllTrack:
            case UiActionId::TimelineClipSelectAllProject:
                context.activePanel = UiPanel::Timeline;
                break;

            case UiActionId::TimelineClipCut:
                context.clipboardHasClip = true;
                context.timelineClipSelected = false;
                context.canUndo = true;
                context.canRedo = false;
                ++context.timelineEditCount;
                break;

            case UiActionId::TimelineClipPaste:
            case UiActionId::TimelineClipRepeatPaste:
            case UiActionId::TimelineClipDuplicate:
                context.timelineClipSelected = true;
                context.canUndo = true;
                context.canRedo = false;
                ++context.timelineEditCount;
                break;

            case UiActionId::EditNudgeLeft:
            case UiActionId::EditNudgeRight:
            case UiActionId::EditNudgeLeftFine:
            case UiActionId::EditNudgeRightFine:
                context.canUndo = true;
                context.canRedo = false;
                if (context.activePanel == UiPanel::PianoRoll)
                    ++context.midiEditCount;
                else
                    ++context.timelineEditCount;
                break;

            case UiActionId::TransportToggleMetronome:
                context.metronomeEnabled = ! context.metronomeEnabled;
                break;

            case UiActionId::TransportToggleRecordCountIn:
                context.recordCountInEnabled = ! context.recordCountInEnabled;
                break;

            case UiActionId::ViewToggleSettingsRow:
                context.settingsRowVisible = ! context.settingsRowVisible;
                break;

            case UiActionId::ViewToggleInspector:
                context.inspectorVisible = ! context.inspectorVisible;
                break;

            case UiActionId::EditNudgeValueGrid:      context.nudgeValue = 0; break;
            case UiActionId::EditNudgeValueBar:       context.nudgeValue = 1; break;
            case UiActionId::EditNudgeValueBeat:      context.nudgeValue = 2; break;
            case UiActionId::EditNudgeValueSixteenth: context.nudgeValue = 3; break;

            case UiActionId::TransportStoreLocatePoint1:
            case UiActionId::TransportStoreLocatePoint2:
            case UiActionId::TransportStoreLocatePoint3:
            case UiActionId::TransportStoreLocatePoint4:
            case UiActionId::TransportStoreLocatePoint5:
            {
                const int index = storeLocatePointIndex (id);
                context.locatePoints[static_cast<std::size_t> (index)] =
                    std::max<std::int64_t> (0, context.playheadFrame);
                context.activePanel = UiPanel::Timeline;
                ++context.timelineEditCount;
                break;
            }

            case UiActionId::TransportRecallLocatePoint1:
            case UiActionId::TransportRecallLocatePoint2:
            case UiActionId::TransportRecallLocatePoint3:
            case UiActionId::TransportRecallLocatePoint4:
            case UiActionId::TransportRecallLocatePoint5:
            {
                const int index = recallLocatePointIndex (id);
                context.playheadFrame = *context.locatePoints[static_cast<std::size_t> (index)];
                context.lastLocateFrame = context.playheadFrame;
                context.activePanel = UiPanel::Timeline;
                break;
            }

            case UiActionId::TimelineMarkerAdd:
            case UiActionId::TimelineMarkerRemove:
                context.activePanel = UiPanel::Timeline;
                context.canUndo = true;
                context.canRedo = false;
                ++context.timelineEditCount;
                break;

            case UiActionId::TimelineMidiClipAdd:
                context.midiClipSelected = true;
                context.canUndo = true;
                context.canRedo = false;
                ++context.midiEditCount;
                break;

            case UiActionId::TimelineRangeToLoop:
                context.loopEnabled = true;
                break;

            // G2.5: the range verbs edit the project in the model; the registry only counts them.
            case UiActionId::TimelineRangeSplitEdges:
            case UiActionId::TimelineRangeCut:
            case UiActionId::TimelineRangeCopy:
            case UiActionId::TimelineRangeDelete:
            case UiActionId::TimelineRangeSilence:
            case UiActionId::TimelineZoomToSelection:
            case UiActionId::TimelineSelectAllFollowing:
                break;

            case UiActionId::EditModeOverlap:   context.editMode = UiEditMode::Overlap;   break;   // G2.6
            case UiActionId::EditModeNoOverlap: context.editMode = UiEditMode::NoOverlap; break;
            case UiActionId::EditModeShuffle:   context.editMode = UiEditMode::Shuffle;   break;

            case UiActionId::Count:
                return { id, { false, "unknown action" }, false };
        }

        ++context.commandDispatchCount;
        return { id, state, true };
    }

private:
    Keymap keymap_;
};

} // namespace yesdaw::ui
