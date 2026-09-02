// YES DAW - G1.3 (plan §3.3): the context menus, one table driven by the action registry.
// A right-click classifies what was clicked, makes it the selection (Logic behaviour), and
// shows this target's list. The list names registered verbs only, in §3.3's order; verbs the
// plan names that do not exist yet are absent, not invented.
#pragma once

#include "ui/UiActions.h"

#include <array>
#include <cstdint>
#include <span>

namespace yesdaw::ui {

enum class ContextMenuTarget : std::uint8_t
{
    Clip,
    EmptyLane,
    TrackHeader,
    Ruler,
    Marker,
    Note,
    MixerStrip,
    InsertSlot
};

[[nodiscard]] constexpr const char* contextMenuTargetName (ContextMenuTarget target) noexcept
{
    switch (target)
    {
        case ContextMenuTarget::Clip:        return "Clip";
        case ContextMenuTarget::EmptyLane:   return "EmptyLane";
        case ContextMenuTarget::TrackHeader: return "TrackHeader";
        case ContextMenuTarget::Ruler:       return "Ruler";
        case ContextMenuTarget::Marker:      return "Marker";
        case ContextMenuTarget::Note:        return "Note";
        case ContextMenuTarget::MixerStrip:  return "MixerStrip";
        case ContextMenuTarget::InsertSlot:  return "InsertSlot";
    }
    return "Clip";
}

struct ContextMenuEntry
{
    UiActionId action;
    bool separatorBefore = false;
};

namespace context_menu_detail {

inline constexpr std::array<ContextMenuEntry, 19> kClip {{
    { UiActionId::TimelineClipCut }, { UiActionId::TimelineClipCopy }, { UiActionId::TimelineClipPaste },
    { UiActionId::TimelineClipDuplicate }, { UiActionId::TimelineClipDelete },
    { UiActionId::TimelineClipSplit, true }, { UiActionId::TimelineClipHeal }, { UiActionId::TimelineClipCrossfade },
    { UiActionId::EditRenameSelection, true }, { UiActionId::TimelineClipSetGain },
    { UiActionId::TimelineClipSetFades }, { UiActionId::TimelineClipTimeStretch },
    { UiActionId::TimelineClipToggleMute, true }, { UiActionId::TimelineClipColourNext },   // G2.12
    { UiActionId::TimelineClipReverse, true }, { UiActionId::TimelineClipNormalize }, { UiActionId::TimelineClipStripSilence },   // G2.13
    { UiActionId::TimelineClipSelectAllTrack, true },
    { UiActionId::TimelineRangeToLoop, true },
}};
inline constexpr std::array<ContextMenuEntry, 5> kEmptyLane {{
    { UiActionId::TimelineClipPaste }, { UiActionId::TimelineMidiClipAdd }, { UiActionId::ProjectImportAudio },
    { UiActionId::TimelineClipSelectAllTrack, true },
    { UiActionId::TrackAdd, true },
}};
inline constexpr std::array<ContextMenuEntry, 11> kTrackHeader {{
    { UiActionId::TrackRename }, { UiActionId::TrackDuplicate }, { UiActionId::TrackRemove },
    { UiActionId::TrackAdd, true }, { UiActionId::MixerBusAdd },
    { UiActionId::TrackToggleMute, true }, { UiActionId::TrackToggleSolo },
    { UiActionId::MixerTargetToggleSoloSafe }, { UiActionId::TrackToggleArm },
    { UiActionId::MixerTrackSetOutput, true },
    { UiActionId::TimelineAutomationToggleTrackLane, true },
}};
inline constexpr std::array<ContextMenuEntry, 5> kRuler {{
    { UiActionId::TimelineMarkerAdd }, { UiActionId::TransportSetTempo }, { UiActionId::TransportSetMeter },
    { UiActionId::TimelineRangeToLoop, true }, { UiActionId::TransportToggleLoop },
}};
inline constexpr std::array<ContextMenuEntry, 1> kMarker {{
    { UiActionId::TimelineMarkerRemove },
}};
inline constexpr std::array<ContextMenuEntry, 9> kNote {{
    { UiActionId::PianoRollNoteDuplicate }, { UiActionId::PianoRollNoteDelete },
    { UiActionId::PianoRollNoteQuantizeSelection, true }, { UiActionId::PianoRollNoteTranspose },
    { UiActionId::PianoRollNoteOctaveUp }, { UiActionId::PianoRollNoteOctaveDown },
    { UiActionId::PianoRollNoteSetVelocity }, { UiActionId::PianoRollNoteSetLength },
    { UiActionId::PianoRollNoteSelectAll, true },
}};
inline constexpr std::array<ContextMenuEntry, 8> kMixerStrip {{
    { UiActionId::TrackRename },
    { UiActionId::MixerFxInsertAdd, true }, { UiActionId::MixerSendAdd },
    { UiActionId::TrackToggleMute, true }, { UiActionId::TrackToggleSolo },
    { UiActionId::MixerTargetToggleSoloSafe }, { UiActionId::TrackToggleArm },
    { UiActionId::MixerTrackSetOutput, true },
}};
inline constexpr std::array<ContextMenuEntry, 4> kInsertSlot {{
    { UiActionId::MixerFxInsertToggle }, { UiActionId::MixerFxInsertRemove },
    { UiActionId::MixerFxInsertReorder, true },
    { UiActionId::MixerFxInsertParamSet, true },
}};

} // namespace context_menu_detail

[[nodiscard]] inline std::span<const ContextMenuEntry> contextMenuEntries (ContextMenuTarget target) noexcept
{
    using namespace context_menu_detail;
    switch (target)
    {
        case ContextMenuTarget::Clip:        return kClip;
        case ContextMenuTarget::EmptyLane:   return kEmptyLane;
        case ContextMenuTarget::TrackHeader: return kTrackHeader;
        case ContextMenuTarget::Ruler:       return kRuler;
        case ContextMenuTarget::Marker:      return kMarker;
        case ContextMenuTarget::Note:        return kNote;
        case ContextMenuTarget::MixerStrip:  return kMixerStrip;
        case ContextMenuTarget::InsertSlot:  return kInsertSlot;
    }
    return kClip;
}

} // namespace yesdaw::ui
