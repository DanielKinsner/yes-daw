// YES DAW - H11 UI action registry.
//
// Pure C++ on purpose: menus, toolbar buttons, shortcuts, accessibility, tests, and future agents all
// resolve through the same action IDs without requiring a display.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace yesdaw::ui {

enum class UiActionId : std::uint8_t
{
    ProjectNew = 0,
    ProjectOpen,
    ProjectSave,
    ProjectImportAudio,
    ProjectExportAudio,
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
    Count
};

constexpr std::size_t kUiActionCount = static_cast<std::size_t> (UiActionId::Count);

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
    bool timelineAutomationTrackLaneVisible = false;
    int timelineAutomationTrackIndex = -1;
    int timelineAutomationShowHideCount = 0;
    int timelineAutomationBreakpointEditCount = 0;
    std::int64_t playheadFrame = 0;
    int commandDispatchCount = 0;
    int saveCount = 0;
    int importCount = 0;
    int audioExportCount = 0;
    int dawprojectExportCount = 0;
    int deviceRefreshCount = 0;
    int deviceSelectCount = 0;
    std::uint32_t recordingDeviceGeneration = 0;
    std::uint32_t selectedRecordingDeviceId = 0;
    int selectedRecordingTrackIndex = -1;
    int selectedRecordingInputChannel = -1;
    int recordingArmCount = 0;
    int recordingMonitoringCount = 0;
    int recordingCommandCount = 0;
    bool recordingCompTakesAvailable = false;
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
    int mixerEditCount = 0;
    int mixerReadCount = 0;
    int midiEditCount = 0;
    int midiReadCount = 0;
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

inline constexpr std::array<UiActionDescriptor, kUiActionCount> kUiActionDescriptors {{
    { UiActionId::ProjectNew, "project.new", "New", "Ctrl+N", "New project",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::ProjectOpen, "project.open", "Open", "Ctrl+O", "Open project",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::ProjectSave, "project.save", "Save", "Ctrl+S", "Save project",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::ProjectImportAudio, "project.import_audio", "Import WAV", "Ctrl+I", "Import audio",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::ProjectExportAudio, "project.export_audio", "Export Audio", "Ctrl+Shift+E", "Export audio",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::ProjectExportDawproject, "project.export_dawproject", "Export DAWproject", "Ctrl+Shift+D", "Export DAWproject package",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportPlay, "transport.play", "Play", "Space", "Play transport",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportStop, "transport.stop", "Stop", "K", "Stop transport",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportLocateStart, "transport.locate_start", "Locate", "Home", "Locate start",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::TransportToggleLoop, "transport.toggle_loop", "Loop", "L", "Toggle loop",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false },
    { UiActionId::DeviceRefreshAudio, "device.refresh_audio", "Refresh Device", "Ctrl+Alt+D", "Refresh audio device",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, false, false },
    { UiActionId::DeviceSelectTestAudio, "device.select_test_audio", "Test Device", "Ctrl+Alt+T", "Select test audio device",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::RecordingArmTrack, "record.track.arm", "Arm Track", "Ctrl+Alt+A", "Arm selected Track for recording",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, false, false, false, true, false, false, true },
    { UiActionId::RecordingSetMonitoringPolicy, "record.monitoring_policy", "Monitor", "Ctrl+Alt+O", "Choose recording monitoring policy",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, false, false, false, true },
    { UiActionId::TransportRecord, "transport.record", "Record", "R", "Record transport",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, false, false, true, true, true },
    { UiActionId::RecordingAssembleComp, "record.comp.assemble", "Comp", "Ctrl+Alt+C", "Assemble recording Comp selection",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, false, false, false, false, false, false, true },
    { UiActionId::EditUndo, "edit.undo", "Undo", "Ctrl+Z", "Undo",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, true, false, false },
    { UiActionId::EditRedo, "edit.redo", "Redo", "Ctrl+Shift+Z", "Redo",
      AccessibilityRole::MenuItem, UiActionKind::Command, true, false, true, false },
    { UiActionId::ViewTimeline, "view.timeline", "Timeline", "1", "Show timeline",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::ViewMixer, "view.mixer", "Mixer", "2", "Show mixer",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::ViewPianoRoll, "view.piano_roll", "Piano Roll", "3", "Show piano roll",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineClipMove, "timeline.clip.move", "Move Clip", "Alt+M", "Move selected clip",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipTrim, "timeline.clip.trim", "Trim Clip", "Alt+T", "Trim selected clip",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipSplit, "timeline.clip.split", "Split Clip", "Ctrl+E", "Split selected clip",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipSetGain, "timeline.clip.set_gain", "Clip Gain", "Alt+G", "Set selected clip gain",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipSetFades, "timeline.clip.set_fades", "Clip Fades", "Alt+F", "Set selected clip fades",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::TimelineClipTimeStretch, "timeline.clip.time_stretch", "Time Stretch", "Alt+R", "Time-stretch selected clip",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, true },
    { UiActionId::MixerTargetSetFader, "mixer.target.set_fader", "Fader", "Ctrl+Alt+F", "Set selected mixer fader",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerTargetSetPan, "mixer.target.set_pan", "Pan", "Ctrl+Alt+P", "Set selected mixer pan",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, true },
    { UiActionId::MixerTargetToggleMute, "mixer.target.toggle_mute", "Mute", "Ctrl+Alt+M", "Toggle selected mixer mute",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, true },
    { UiActionId::MixerTargetToggleSolo, "mixer.target.toggle_solo", "Solo", "Ctrl+Alt+S", "Toggle selected mixer solo",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, true },
    { UiActionId::MixerReadMeters, "mixer.meters.read", "Meters", "Ctrl+Alt+V", "Read mixer meters",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::MixerReadLoudness, "mixer.loudness.read", "Loudness", "Ctrl+Alt+L", "Read loudness",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::MixerReadSends, "mixer.sends.read", "Sends", "Ctrl+Alt+Shift+V", "Read mixer sends",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::MixerSetFirstSendLevel, "mixer.sends.first.set_level", "Send Level", "Ctrl+Alt+Shift+N", "Set first Track send level",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false },
    { UiActionId::MixerReadFxSlots, "mixer.fx_slots.read", "FX Slots", "Ctrl+Alt+Shift+X", "Read mixer FX slots",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false },
    { UiActionId::MixerToggleFirstFxSlotEnabled, "mixer.fx_slots.first.toggle_enabled", "FX On", "Ctrl+Alt+Shift+B", "Toggle first Track FX insert enabled",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false, false },
    { UiActionId::PianoRollNoteSelect, "piano_roll.note.select", "Select Note", "Alt+N", "Select piano-roll note",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, false },
    { UiActionId::PianoRollNoteMove, "piano_roll.note.move", "Move Note", "Alt+Shift+M", "Move selected note",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteSetLength, "piano_roll.note.set_length", "Note Length", "Alt+Shift+L", "Set selected note length",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteTranspose, "piano_roll.note.transpose", "Transpose", "Alt+Shift+Up", "Transpose selected note",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollNoteQuantize, "piano_roll.note.quantize", "Quantize", "Alt+Shift+Q", "Quantize selected note",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, true, true },
    { UiActionId::PianoRollReadExpressionLanes, "piano_roll.expression.read", "Expression", "Alt+Shift+E", "Read MIDI expression lanes",
      AccessibilityRole::Panel, UiActionKind::Query, true, false, false, false, false, true, false },
    { UiActionId::AutosaveRecoveryRestore, "autosave.recovery.restore", "Restore Autosave", "Ctrl+Alt+R", "Restore autosave recovery snapshot",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, false, false, false, false, false, false, false, true },
    { UiActionId::AutosaveRecoveryDiscard, "autosave.recovery.discard", "Discard Autosave", "Ctrl+Alt+X", "Discard autosave recovery snapshot",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false, false, false, false, false, false, false, false, false, true },
    { UiActionId::HelpShowKeymap, "help.show_keymap", "Keymap", "Ctrl+/", "Show keymap",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, false, false, false, false },
    { UiActionId::TimelineToolSelectPointer, "timeline.tool.pointer", "Pointer", "V", "Select pointer tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineToolSelectPencil, "timeline.tool.pencil", "Pencil", "P", "Select pencil tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineToolSelectScissors, "timeline.tool.scissors", "Scissors", "S", "Select scissors tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineToolSelectHand, "timeline.tool.hand", "Hand", "H", "Select hand tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineToolSelectZoom, "timeline.tool.zoom", "Zoom", "Z", "Select zoom tool",
      AccessibilityRole::Button, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineSnapDisable, "timeline.snap.disable", "Snap Off", "Ctrl+0", "Disable timeline snap",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineSnapSetBar, "timeline.snap.bar", "Snap Bar", "Ctrl+1", "Set timeline snap to bar",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineSnapSetBeat, "timeline.snap.beat", "Snap Beat", "Ctrl+2", "Set timeline snap to beat",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineSnapSetSixteenth, "timeline.snap.sixteenth", "Snap 1/16", "Ctrl+3", "Set timeline snap to sixteenth note",
      AccessibilityRole::MenuItem, UiActionKind::Command, false, false, false, false },
    { UiActionId::TimelineAutomationToggleTrackLane, "timeline.automation.track_lane.toggle", "Automation", "A", "Toggle first Track automation lane",
      AccessibilityRole::ToggleButton, UiActionKind::Toggle, true, false, false, false },
    { UiActionId::TimelineAutomationAddBreakpoint, "timeline.automation.breakpoint.add", "Add Point", "Shift+A", "Add breakpoint to first Track automation lane",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false },
    { UiActionId::TimelineAutomationDeleteBreakpoint, "timeline.automation.breakpoint.delete", "Delete Point", "Shift+D", "Delete breakpoint from first Track automation lane",
      AccessibilityRole::Button, UiActionKind::Command, true, false, false, false }
}};

inline constexpr std::array<UiActionId, 18> kMainShellToolbarActions {{
    UiActionId::ProjectNew,
    UiActionId::ProjectOpen,
    UiActionId::ProjectSave,
    UiActionId::ProjectImportAudio,
    UiActionId::DeviceRefreshAudio,
    UiActionId::DeviceSelectTestAudio,
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
    UiActionId::ViewMixer,
    UiActionId::ViewPianoRoll
}};

inline const std::array<UiActionDescriptor, kUiActionCount>& uiActionDescriptors()
{
    return kUiActionDescriptors;
}

inline const std::array<UiActionId, 18>& mainShellToolbarActions()
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

    UiActionId actionForChord (std::string_view chord) const
    {
        if (chord.empty())
            return UiActionId::Count;

        for (std::size_t i = 0; i < chords_.size(); ++i)
            if (chords_[i] == chord)
                return static_cast<UiActionId> (i);

        return UiActionId::Count;
    }

    KeymapRebindStatus rebind (UiActionId id, std::string_view chord)
    {
        if (! isKnownAction (id))
            return KeymapRebindStatus::UnknownAction;
        if (chord.empty())
            return KeymapRebindStatus::EmptyChord;

        const UiActionId existing = actionForChord (chord);
        if (existing != UiActionId::Count && existing != id)
            return KeymapRebindStatus::DuplicateChord;

        chords_[actionIndex (id)] = std::string (chord);
        return KeymapRebindStatus::Ok;
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
                context.playheadFrame = 0;
                context.activePanel = UiPanel::Timeline;
                context.canUndo = false;
                context.canRedo = false;
                context.timelineAutomationTrackLaneVisible = false;
                context.timelineAutomationTrackIndex = -1;
                break;

            case UiActionId::ProjectSave:
                ++context.saveCount;
                break;

            case UiActionId::ProjectImportAudio:
                ++context.importCount;
                break;

            case UiActionId::ProjectExportAudio:
                ++context.audioExportCount;
                break;

            case UiActionId::ProjectExportDawproject:
                ++context.dawprojectExportCount;
                break;

            case UiActionId::TransportPlay:
                context.isPlaying = true;
                break;

            case UiActionId::TransportStop:
                context.isPlaying = false;
                break;

            case UiActionId::TransportLocateStart:
                context.playheadFrame = 0;
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
                context.isRecording = ! context.isRecording;
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

            case UiActionId::ViewMixer:
                context.activePanel = UiPanel::Mixer;
                break;

            case UiActionId::ViewPianoRoll:
                context.activePanel = UiPanel::PianoRoll;
                break;

            case UiActionId::TimelineClipMove:
            case UiActionId::TimelineClipTrim:
            case UiActionId::TimelineClipSplit:
            case UiActionId::TimelineClipSetGain:
            case UiActionId::TimelineClipSetFades:
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
            case UiActionId::MixerSetFirstSendLevel:
            case UiActionId::MixerToggleFirstFxSlotEnabled:
                context.activePanel = UiPanel::Mixer;
                context.canUndo = true;
                context.canRedo = false;
                ++context.mixerEditCount;
                break;

            case UiActionId::MixerReadMeters:
            case UiActionId::MixerReadLoudness:
            case UiActionId::MixerReadSends:
            case UiActionId::MixerReadFxSlots:
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
                context.activePanel = UiPanel::PianoRoll;
                context.canUndo = true;
                context.canRedo = false;
                ++context.midiEditCount;
                break;

            case UiActionId::PianoRollReadExpressionLanes:
                context.activePanel = UiPanel::PianoRoll;
                ++context.midiReadCount;
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

            case UiActionId::TimelineToolSelectPointer:
                context.activePanel = UiPanel::Timeline;
                context.activeTimelineTool = TimelineTool::Pointer;
                break;

            case UiActionId::TimelineToolSelectPencil:
                context.activePanel = UiPanel::Timeline;
                context.activeTimelineTool = TimelineTool::Pencil;
                break;

            case UiActionId::TimelineToolSelectScissors:
                context.activePanel = UiPanel::Timeline;
                context.activeTimelineTool = TimelineTool::Scissors;
                break;

            case UiActionId::TimelineToolSelectHand:
                context.activePanel = UiPanel::Timeline;
                context.activeTimelineTool = TimelineTool::Hand;
                break;

            case UiActionId::TimelineToolSelectZoom:
                context.activePanel = UiPanel::Timeline;
                context.activeTimelineTool = TimelineTool::Zoom;
                break;

            case UiActionId::TimelineSnapDisable:
                context.activePanel = UiPanel::Timeline;
                context.snapEnabled = false;
                break;

            case UiActionId::TimelineSnapSetBar:
                context.activePanel = UiPanel::Timeline;
                context.snapEnabled = true;
                context.snapGridTicks = 2048;
                break;

            case UiActionId::TimelineSnapSetBeat:
                context.activePanel = UiPanel::Timeline;
                context.snapEnabled = true;
                context.snapGridTicks = 512;
                break;

            case UiActionId::TimelineSnapSetSixteenth:
                context.activePanel = UiPanel::Timeline;
                context.snapEnabled = true;
                context.snapGridTicks = 128;
                break;

            case UiActionId::TimelineAutomationToggleTrackLane:
                context.activePanel = UiPanel::Timeline;
                context.timelineAutomationTrackLaneVisible = ! context.timelineAutomationTrackLaneVisible;
                context.timelineAutomationTrackIndex = context.timelineAutomationTrackLaneVisible ? 0 : -1;
                ++context.timelineAutomationShowHideCount;
                break;

            case UiActionId::TimelineAutomationAddBreakpoint:
            case UiActionId::TimelineAutomationDeleteBreakpoint:
                context.activePanel = UiPanel::Timeline;
                context.canUndo = true;
                context.canRedo = false;
                ++context.timelineAutomationBreakpointEditCount;
                break;

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
