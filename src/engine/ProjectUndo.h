// YES DAW - in-memory Project edit command/diff undo surface (H2).
//
// This is control-side document state only: commands wrap the existing Clip/Note edit helpers and record
// exact row before/after diffs for bit-identical undo/redo. SQLite durability is deliberately outside
// this layer.

#pragma once

#include "engine/Project.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace yesdaw::engine {

enum class ProjectEditVerb : std::uint8_t
{
    MoveClip = 0,
    TrimClip,
    SplitClip,
    SetClipGain,
    SetClipFades,
    MoveNote,
    SetNoteLength,
    SplitNote,
    CutNote,
    QuantizeNote,
    TransposeNote,
    SetRecordingCompSelection,
    AddFxInsert,
    RemoveFxInsert,
    ReorderFxInsert,
    SetFxInsertEnabled,
    SetFxInsertParam,
    AddAutomationLane,
    RemoveAutomationLane,
    AddAutomationBreakpoint,
    MoveAutomationBreakpoint,
    SetAutomationBreakpointValue,
    SetAutomationBreakpointCurve,
    RemoveAutomationBreakpoint,
    // Arrangement verbs (usable-DAW P0, 2026-08-09)
    AddClip,
    DeleteClip,
    MoveClipToTrack,
    AddNote,
    AddTrack,
    RenameTrack,
    ReorderTrack,
    RemoveTrack,
    SetProjectTempo,
    SetProjectMeter,
    AddMarker,
    RemoveMarker,
    AddMidiClip,
    // ADR-0044 persisted send routing verbs
    AddBus,
    RemoveBus,
    AddSend,
    RemoveSend,
    SetSendLevel,
    RenameClip,
    SetTrackMixScalars,
    SetNoteVelocity,
    // E7: marker editing beyond add/remove
    MoveMarker,
    RenameMarker,
    // E8: MIDI clips as first-class timeline citizens
    MoveMidiClip,
    MoveMidiClipToTrack,
    RemoveMidiClip,
    // E16: bus strips are real strips — scalar edits mirror SetTrackMixScalars
    SetBusMixScalars,
    // E17: bus rename (the name rides the shared trackName array like markers do)
    RenameBus,
    // E18: send tap point (pre/post fader) — the first mutating verb for the persisted column
    SetSendTap,
    // E19: persisted master strip gain
    SetMasterGain,
    // E33: take management — removes the take, its clip, and comp segments referencing it
    RemoveRecordingTake,
    // M3: a Track's main output target (invalid bus id = master)
    SetTrackOutput
};

struct ProjectEditCommand
{
    ProjectEditVerb verb = ProjectEditVerb::MoveClip;
    EntityId clipId;
    EntityId rightClipId;
    Tick timelineStart = 0;
    Tick timelineLength = 0;
    std::uint64_t srcOffset = 0;
    std::uint64_t srcLen = 0;
    float gain = 1.0f;
    Tick fadeIn = 0;
    Tick fadeOut = 0;
    EntityId midiClipId;
    EntityId noteId;
    EntityId rightNoteId;
    Tick noteStartTick = 0;
    Tick noteLengthTicks = 0;
    Tick snapGridTicks = 0;
    std::int32_t semitones = 0;
    EntityId firstCompSegmentId;
    EntityId firstCompTakeId;
    Tick firstCompTimelineStart = 0;
    Tick firstCompTimelineLength = 0;
    std::uint64_t firstCompSourceOffset = 0;
    EntityId secondCompSegmentId;
    EntityId secondCompTakeId;
    Tick secondCompTimelineStart = 0;
    Tick secondCompTimelineLength = 0;
    std::uint64_t secondCompSourceOffset = 0;
    EntityId fxOwnerId;
    EntityId fxInsertId;
    FxKind fxKind = FxKind::Eq;
    bool fxEnabled = true;
    std::size_t fxPosition = 0;
    std::uint32_t fxParamId = 0;
    double fxParamValue = 0.0;
    EntityId automationLaneId;
    EntityId automationOwnerId;
    AutomationTargetRole automationRole = AutomationTargetRole::TrackFader;
    std::uint32_t automationParamId = 0;
    Tick automationTick = 0;
    Tick automationNewTick = 0;
    double automationValue = 0.0;
    AutomationCurveType automationCurveType = AutomationCurveType::Linear;
    // Arrangement verb payloads. trackName is a fixed array so the command stays trivially copyable;
    // names longer than the array are rejected at the factory, never silently truncated.
    EntityId trackId;
    std::size_t trackPosition = 0;
    char trackName[128] = {};
    char clipName[128] = {};
    std::int16_t noteKey = 60;
    double notePitch = 60.0;
    double noteVelocity = 1.0;
    std::int16_t notePort = -1;
    std::int16_t noteChannel = -1;

    double tempoBpm = 120.0;
    std::uint16_t meterNumerator = 4;
    std::uint16_t meterDenominator = 4;
    EntityId clipAssetId;
    TimeBase clipTimeBase = TimeBase::SampleLocked;
    EntityId markerId;
    Tick markerTick = 0;
    // ADR-0044 send routing payloads
    EntityId busId;
    EntityId sendId;
    SendTap sendTap = SendTap::PostFader;
    float sendLinearGain = 1.0f;
    // Scalar strip-state payload (SetTrackMixScalars; the linear gain rides the shared `gain` field)
    float trackPan = 0.0f;
    bool trackMuted = false;
    bool trackSoloed = false;
    bool trackSoloSafe = false;

    static constexpr std::size_t kMaxTrackNameLength = 127;   // trackName holds this + NUL
    static constexpr std::size_t kMaxClipNameLength = ClipName::kMaxLength;

    [[nodiscard]] static constexpr bool copyClipName (ProjectEditCommand& command, std::string_view name) noexcept
    {
        if (name.empty() || name.size() > kMaxClipNameLength)
            return false;

        for (std::size_t i = 0; i < name.size(); ++i)
            command.clipName[i] = name[i];
        command.clipName[name.size()] = '\0';
        return true;
    }

    [[nodiscard]] static constexpr ProjectEditCommand moveClip (EntityId clipId, Tick newTimelineStart) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::MoveClip;
        command.clipId = clipId;
        command.timelineStart = newTimelineStart;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand trimClip (EntityId clipId,
                                                                Tick newTimelineStart,
                                                                Tick newTimelineLength,
                                                                std::uint64_t newSrcOffset,
                                                                std::uint64_t newSrcLen) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::TrimClip;
        command.clipId = clipId;
        command.timelineStart = newTimelineStart;
        command.timelineLength = newTimelineLength;
        command.srcOffset = newSrcOffset;
        command.srcLen = newSrcLen;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand splitClip (EntityId clipId,
                                                                 EntityId rightClipId,
                                                                 Tick leftTimelineLength,
                                                                 std::uint64_t leftSourceLength) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SplitClip;
        command.clipId = clipId;
        command.rightClipId = rightClipId;
        command.timelineLength = leftTimelineLength;
        command.srcLen = leftSourceLength;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setClipGain (EntityId clipId, float newGain) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetClipGain;
        command.clipId = clipId;
        command.gain = newGain;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setClipFades (EntityId clipId,
                                                                    Tick newFadeIn,
                                                                    Tick newFadeOut) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetClipFades;
        command.clipId = clipId;
        command.fadeIn = newFadeIn;
        command.fadeOut = newFadeOut;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand moveNote (EntityId midiClipId,
                                                                EntityId noteId,
                                                                Tick newStartTick) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::MoveNote;
        command.midiClipId = midiClipId;
        command.noteId = noteId;
        command.noteStartTick = newStartTick;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setNoteLength (EntityId midiClipId,
                                                                     EntityId noteId,
                                                                     Tick newLengthTicks) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetNoteLength;
        command.midiClipId = midiClipId;
        command.noteId = noteId;
        command.noteLengthTicks = newLengthTicks;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand splitNote (EntityId midiClipId,
                                                                 EntityId noteId,
                                                                 EntityId rightNoteId,
                                                                 Tick leftLengthTicks) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SplitNote;
        command.midiClipId = midiClipId;
        command.noteId = noteId;
        command.rightNoteId = rightNoteId;
        command.noteLengthTicks = leftLengthTicks;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand cutNote (EntityId midiClipId, EntityId noteId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::CutNote;
        command.midiClipId = midiClipId;
        command.noteId = noteId;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand quantizeNote (EntityId midiClipId,
                                                                    EntityId noteId,
                                                                    SnapGrid grid) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::QuantizeNote;
        command.midiClipId = midiClipId;
        command.noteId = noteId;
        command.snapGridTicks = grid.intervalTicks;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand transposeNote (EntityId midiClipId,
                                                                     EntityId noteId,
                                                                     std::int32_t semitones) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::TransposeNote;
        command.midiClipId = midiClipId;
        command.noteId = noteId;
        command.semitones = semitones;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setRecordingCompSelection (
        EntityId firstSegmentId,
        EntityId firstTakeId,
        Tick firstTimelineStart,
        Tick firstTimelineLength,
        std::uint64_t firstSourceOffset,
        EntityId secondSegmentId,
        EntityId secondTakeId,
        Tick secondTimelineStart,
        Tick secondTimelineLength,
        std::uint64_t secondSourceOffset) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetRecordingCompSelection;
        command.firstCompSegmentId = firstSegmentId;
        command.firstCompTakeId = firstTakeId;
        command.firstCompTimelineStart = firstTimelineStart;
        command.firstCompTimelineLength = firstTimelineLength;
        command.firstCompSourceOffset = firstSourceOffset;
        command.secondCompSegmentId = secondSegmentId;
        command.secondCompTakeId = secondTakeId;
        command.secondCompTimelineStart = secondTimelineStart;
        command.secondCompTimelineLength = secondTimelineLength;
        command.secondCompSourceOffset = secondSourceOffset;
        return command;
    }

    // E33: removes the take, its clip, and comp segments referencing it — one undoable step.
    [[nodiscard]] static constexpr ProjectEditCommand removeRecordingTake (EntityId takeId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RemoveRecordingTake;
        command.firstCompTakeId = takeId;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand addFxInsert (EntityId ownerId,
                                                                    EntityId insertId,
                                                                    FxKind kind,
                                                                    bool enabled,
                                                                    std::size_t position) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddFxInsert;
        command.fxOwnerId = ownerId;
        command.fxInsertId = insertId;
        command.fxKind = kind;
        command.fxEnabled = enabled;
        command.fxPosition = position;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand removeFxInsert (EntityId ownerId, EntityId insertId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RemoveFxInsert;
        command.fxOwnerId = ownerId;
        command.fxInsertId = insertId;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand reorderFxInsert (EntityId ownerId,
                                                                       EntityId insertId,
                                                                       std::size_t newPosition) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::ReorderFxInsert;
        command.fxOwnerId = ownerId;
        command.fxInsertId = insertId;
        command.fxPosition = newPosition;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setFxInsertEnabled (EntityId ownerId,
                                                                          EntityId insertId,
                                                                          bool enabled) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetFxInsertEnabled;
        command.fxOwnerId = ownerId;
        command.fxInsertId = insertId;
        command.fxEnabled = enabled;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setFxInsertParam (EntityId ownerId,
                                                                        EntityId insertId,
                                                                        std::uint32_t paramId,
                                                                        double normalizedValue) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetFxInsertParam;
        command.fxOwnerId = ownerId;
        command.fxInsertId = insertId;
        command.fxParamId = paramId;
        command.fxParamValue = normalizedValue;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand addAutomationLane (EntityId laneId,
                                                                         EntityId ownerId,
                                                                         AutomationTargetRole role,
                                                                         std::uint32_t paramId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddAutomationLane;
        command.automationLaneId = laneId;
        command.automationOwnerId = ownerId;
        command.automationRole = role;
        command.automationParamId = paramId;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand removeAutomationLane (EntityId laneId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RemoveAutomationLane;
        command.automationLaneId = laneId;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand addAutomationBreakpoint (EntityId laneId,
                                                                              Tick tick,
                                                                              double value,
                                                                              AutomationCurveType curve) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddAutomationBreakpoint;
        command.automationLaneId = laneId;
        command.automationTick = tick;
        command.automationValue = value;
        command.automationCurveType = curve;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand moveAutomationBreakpoint (EntityId laneId,
                                                                               Tick oldTick,
                                                                               Tick newTick) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::MoveAutomationBreakpoint;
        command.automationLaneId = laneId;
        command.automationTick = oldTick;
        command.automationNewTick = newTick;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setAutomationBreakpointValue (EntityId laneId,
                                                                                   Tick tick,
                                                                                   double value) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetAutomationBreakpointValue;
        command.automationLaneId = laneId;
        command.automationTick = tick;
        command.automationValue = value;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setAutomationBreakpointCurve (EntityId laneId,
                                                                                   Tick tick,
                                                                                   AutomationCurveType curve) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetAutomationBreakpointCurve;
        command.automationLaneId = laneId;
        command.automationTick = tick;
        command.automationCurveType = curve;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand removeAutomationBreakpoint (EntityId laneId, Tick tick) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RemoveAutomationBreakpoint;
        command.automationLaneId = laneId;
        command.automationTick = tick;
        return command;
    }

    // --- Arrangement verbs (usable-DAW P0, 2026-08-09) ---

    [[nodiscard]] static constexpr ProjectEditCommand addClip (const Clip& clip) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddClip;
        command.clipId = clip.id;
        command.clipAssetId = clip.assetId;
        command.trackId = clip.trackId;
        command.timelineStart = clip.timelineStart;
        command.timelineLength = clip.timelineLength;
        command.srcOffset = clip.srcOffset;
        command.srcLen = clip.srcLen;
        command.gain = clip.gain;
        command.fadeIn = clip.fadeIn;
        command.fadeOut = clip.fadeOut;
        command.clipTimeBase = clip.timeBase;
        (void) copyClipName (command, clip.name.asView());
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand deleteClip (EntityId clipId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::DeleteClip;
        command.clipId = clipId;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand moveClipToTrack (EntityId clipId,
                                                                       EntityId targetTrackId,
                                                                       Tick newTimelineStart) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::MoveClipToTrack;
        command.clipId = clipId;
        command.trackId = targetTrackId;
        command.timelineStart = newTimelineStart;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand addNote (EntityId midiClipId,
                                                               EntityId noteId,
                                                               Tick startTick,
                                                               Tick lengthTicks,
                                                               std::int16_t key,
                                                               double normalizedVelocity = 1.0) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddNote;
        command.midiClipId = midiClipId;
        command.noteId = noteId;
        command.noteStartTick = startTick;
        command.noteLengthTicks = lengthTicks;
        command.noteKey = key;
        command.notePitch = static_cast<double> (key);
        command.noteVelocity = normalizedVelocity;
        return command;
    }

    // Returns a command whose verb is only valid when `name` fits kMaxTrackNameLength; oversized or
    // empty names yield a command that the apply path rejects (never silent truncation).
    [[nodiscard]] static constexpr bool copyTrackName (ProjectEditCommand& command, std::string_view name) noexcept
    {
        if (name.empty() || name.size() > kMaxTrackNameLength)
            return false;

        for (std::size_t i = 0; i < name.size(); ++i)
            command.trackName[i] = name[i];
        command.trackName[name.size()] = '\0';
        return true;
    }

    [[nodiscard]] static constexpr ProjectEditCommand addTrack (EntityId trackId, std::string_view name) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddTrack;
        command.trackId = trackId;
        (void) copyTrackName (command, name);   // invalid names leave trackName empty -> apply rejects
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand renameTrack (EntityId trackId, std::string_view name) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RenameTrack;
        command.trackId = trackId;
        (void) copyTrackName (command, name);
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand renameClip (EntityId clipId, std::string_view name) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RenameClip;
        command.clipId = clipId;
        (void) copyClipName (command, name);
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand reorderTrack (EntityId trackId, std::size_t newIndex) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::ReorderTrack;
        command.trackId = trackId;
        command.trackPosition = newIndex;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand removeTrack (EntityId trackId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RemoveTrack;
        command.trackId = trackId;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setNoteVelocity (EntityId midiClipId,
                                                                       EntityId noteId,
                                                                       double normalizedVelocity) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetNoteVelocity;
        command.midiClipId = midiClipId;
        command.noteId = noteId;
        command.noteVelocity = normalizedVelocity;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setTrackMixScalars (EntityId trackId,
                                                                          float linearGain,
                                                                          float pan,
                                                                          bool muted,
                                                                          bool soloed,
                                                                          bool soloSafe) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetTrackMixScalars;
        command.trackId = trackId;
        command.gain = linearGain;
        command.trackPan = pan;
        command.trackMuted = muted;
        command.trackSoloed = soloed;
        command.trackSoloSafe = soloSafe;
        return command;
    }

    // Marker name rides the trackName array (same trivially-copyable command constraint).
    [[nodiscard]] static constexpr ProjectEditCommand addMidiClip (EntityId midiClipId,
                                                                    EntityId trackId,
                                                                    Tick timelineStart,
                                                                    Tick timelineLength,
                                                                    TimeBase timeBase) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddMidiClip;
        command.midiClipId = midiClipId;
        command.trackId = trackId;
        command.timelineStart = timelineStart;
        command.timelineLength = timelineLength;
        command.clipTimeBase = timeBase;
        return command;
    }

    // E8: MIDI clips move, cross tracks, and delete like audio clips.
    [[nodiscard]] static constexpr ProjectEditCommand moveMidiClip (EntityId midiClipId,
                                                                    Tick newTimelineStart) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::MoveMidiClip;
        command.midiClipId = midiClipId;
        command.timelineStart = newTimelineStart;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand moveMidiClipToTrack (EntityId midiClipId,
                                                                           EntityId targetTrackId,
                                                                           Tick newTimelineStart) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::MoveMidiClipToTrack;
        command.midiClipId = midiClipId;
        command.trackId = targetTrackId;
        command.timelineStart = newTimelineStart;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand removeMidiClip (EntityId midiClipId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RemoveMidiClip;
        command.midiClipId = midiClipId;
        return command;
    }

    // ADR-0044 send routing factories
    // E16: bus scalar edits ride the same trivially-copyable payload as the track twin (the
    // linear gain shares the `gain` field; pan/mute/solo/soloSafe share the track fields).
    [[nodiscard]] static constexpr ProjectEditCommand setBusMixScalars (EntityId busId,
                                                                        float linearGain,
                                                                        float pan,
                                                                        bool muted,
                                                                        bool soloed,
                                                                        bool soloSafe) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetBusMixScalars;
        command.busId = busId;
        command.gain = linearGain;
        command.trackPan = pan;
        command.trackMuted = muted;
        command.trackSoloed = soloed;
        command.trackSoloSafe = soloSafe;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand addBus (EntityId busId, std::string_view name) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddBus;
        command.busId = busId;
        (void) copyTrackName (command, name);   // invalid names leave trackName empty -> apply rejects
        return command;
    }

    // E17: the bus name rides the shared trackName array (trivially-copyable command law).
    [[nodiscard]] static constexpr ProjectEditCommand renameBus (EntityId busId,
                                                                 std::string_view name) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RenameBus;
        command.busId = busId;
        const std::size_t length = name.size() < sizeof (command.trackName) - 1
            ? name.size()
            : sizeof (command.trackName) - 1;
        for (std::size_t i = 0; i < length; ++i)
            command.trackName[i] = name[i];
        command.trackName[length] = '\0';
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand removeBus (EntityId busId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RemoveBus;
        command.busId = busId;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand addSend (EntityId trackId,
                                                               EntityId sendId,
                                                               EntityId busId,
                                                               SendTap tap,
                                                               float linearGain) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddSend;
        command.trackId = trackId;
        command.sendId = sendId;
        command.busId = busId;
        command.sendTap = tap;
        command.sendLinearGain = linearGain;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand removeSend (EntityId trackId, EntityId sendId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RemoveSend;
        command.trackId = trackId;
        command.sendId = sendId;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setSendLevel (EntityId trackId,
                                                                    EntityId sendId,
                                                                    float linearGain) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetSendLevel;
        command.trackId = trackId;
        command.sendId = sendId;
        command.sendLinearGain = linearGain;
        return command;
    }

    // E18: send tap point (pre/post fader).
    [[nodiscard]] static constexpr ProjectEditCommand setSendTap (EntityId trackId,
                                                                  EntityId sendId,
                                                                  SendTap tap) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetSendTap;
        command.trackId = trackId;
        command.sendId = sendId;
        command.sendTap = tap;
        return command;
    }

    // M3: a Track's main output target. An invalid busId routes the Track to master.
    [[nodiscard]] static constexpr ProjectEditCommand setTrackOutput (EntityId trackId,
                                                                      EntityId busId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetTrackOutput;
        command.trackId = trackId;
        command.busId = busId;
        return command;
    }

    // E19: persisted master strip gain (rides the shared `gain` field).
    [[nodiscard]] static constexpr ProjectEditCommand setMasterGain (float linearGain) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetMasterGain;
        command.gain = linearGain;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand addMarker (EntityId markerId,
                                                                 Tick tick,
                                                                 std::string_view name) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::AddMarker;
        command.markerId = markerId;
        command.markerTick = tick;
        (void) copyTrackName (command, name);
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand removeMarker (EntityId markerId) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RemoveMarker;
        command.markerId = markerId;
        return command;
    }

    // E7: move a marker to a new timeline tick (kept sorted like addMarker's insert law).
    [[nodiscard]] static constexpr ProjectEditCommand moveMarker (EntityId markerId, Tick newTick) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::MoveMarker;
        command.markerId = markerId;
        command.markerTick = newTick;
        return command;
    }

    // E7: rename a marker (same shared name buffer/limits as track and clip names).
    [[nodiscard]] static constexpr ProjectEditCommand renameMarker (EntityId markerId,
                                                                    std::string_view name) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::RenameMarker;
        command.markerId = markerId;
        (void) copyTrackName (command, name);
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setProjectTempo (double bpm) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetProjectTempo;
        command.tempoBpm = bpm;
        return command;
    }

    [[nodiscard]] static constexpr ProjectEditCommand setProjectMeter (std::uint16_t numerator,
                                                                       std::uint16_t denominator) noexcept
    {
        ProjectEditCommand command;
        command.verb = ProjectEditVerb::SetProjectMeter;
        command.meterNumerator = numerator;
        command.meterDenominator = denominator;
        return command;
    }
};

static_assert (std::is_trivially_copyable_v<ProjectEditCommand>,
               "ProjectEditCommand must stay a simple command payload");

struct ProjectClipRowsDiff
{
    std::size_t firstClipIndex = 0;
    std::vector<Clip> before;
    std::vector<Clip> after;
};

struct ProjectMidiClipRowsDiff
{
    std::size_t firstMidiClipIndex = 0;
    std::vector<MidiClip> before;
    std::vector<MidiClip> after;
};

struct ProjectRecordingCompRowsDiff
{
    std::vector<ProjectRecordingCompSegment> before;
    std::vector<ProjectRecordingCompSegment> after;
};

// E33: take removal touches THREE row families at once (takes + clips + comp segments), so
// its diff snapshots all three whole — counts are small and undo stays bit-exact.
struct ProjectRecordingTakeRowsDiff
{
    std::vector<RecordingTake> takesBefore;
    std::vector<RecordingTake> takesAfter;
    std::vector<Clip> clipsBefore;
    std::vector<Clip> clipsAfter;
    std::vector<ProjectRecordingCompSegment> compBefore;
    std::vector<ProjectRecordingCompSegment> compAfter;
};

struct ProjectFxChainRowsDiff
{
    EntityId ownerId;
    std::vector<FxInsert> before;
    std::vector<FxInsert> after;
};

struct ProjectAutomationLaneRowsDiff
{
    std::size_t firstAutomationLaneIndex = 0;
    std::vector<AutomationLaneData> before;
    std::vector<AutomationLaneData> after;
};

// Marker diffs snapshot the whole marker vector (ordered inserts move rows; counts are small).
struct ProjectMarkerRowsDiff
{
    std::vector<Marker> before;
    std::vector<Marker> after;
};

// Time-map diffs snapshot both maps whole: head edits are tiny and undo stays bit-exact.
struct ProjectTimeMapRowsDiff
{
    std::vector<TempoChange> tempoBefore;
    std::vector<TempoChange> tempoAfter;
    std::vector<MeterChange> meterBefore;
    std::vector<MeterChange> meterAfter;
};

// Track lifecycle diffs snapshot the WHOLE tracks vector: add/remove/reorder move rows across
// indices, and track counts are small, so whole-vector before/after keeps undo bit-exact and simple.
struct ProjectTrackRowsDiff
{
    std::vector<Track> before;
    std::vector<Track> after;
};

// Bus lifecycle diffs snapshot the whole buses vector (ADR-0044; same law as tracks).
struct ProjectBusRowsDiff
{
    std::vector<Bus> before;
    std::vector<Bus> after;
};

// E19: the master strip is one scalar; the diff stores both values whole.
struct ProjectMasterGainDiff
{
    float before = 1.0f;
    float after = 1.0f;
};

struct ProjectEditTransaction
{
    ProjectEditCommand command;
    ProjectClipRowsDiff diff;
    ProjectMidiClipRowsDiff midiDiff;
    ProjectRecordingCompRowsDiff recordingCompDiff;
    ProjectRecordingTakeRowsDiff recordingTakeDiff;
    ProjectFxChainRowsDiff fxDiff;
    ProjectAutomationLaneRowsDiff automationDiff;
    ProjectTrackRowsDiff trackDiff;
    ProjectBusRowsDiff busDiff;
    ProjectTimeMapRowsDiff timeMapDiff;
    ProjectMarkerRowsDiff markerDiff;
    ProjectMasterGainDiff masterDiff;
};

struct ProjectEditApplyResult
{
    ProjectEditStatus editStatus = ProjectEditStatus::InvalidProject;
    bool recorded = false;
    bool coalesced = false;

    [[nodiscard]] constexpr bool applied() const noexcept
    {
        return editStatus == ProjectEditStatus::Applied && recorded;
    }
};

enum class ProjectUndoStatus : std::uint8_t
{
    Applied = 0,
    NothingToUndo,
    NothingToRedo,
    ProjectMismatch
};

namespace detail {

[[nodiscard]] inline bool findClipIndex (const Project& project, EntityId clipId, std::size_t& out) noexcept
{
    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        if (project.clips[i].id == clipId)
        {
            out = i;
            return true;
        }
    }

    return false;
}

[[nodiscard]] inline bool findMidiClipIndex (const Project& project, EntityId midiClipId, std::size_t& out) noexcept
{
    for (std::size_t i = 0; i < project.midiClips.size(); ++i)
    {
        if (project.midiClips[i].id == midiClipId)
        {
            out = i;
            return true;
        }
    }

    return false;
}

[[nodiscard]] constexpr bool isMidiNoteEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::MoveNote
           || verb == ProjectEditVerb::SetNoteLength
           || verb == ProjectEditVerb::SplitNote
           || verb == ProjectEditVerb::CutNote
           || verb == ProjectEditVerb::QuantizeNote
           || verb == ProjectEditVerb::TransposeNote
           || verb == ProjectEditVerb::AddNote
           || verb == ProjectEditVerb::AddMidiClip
           || verb == ProjectEditVerb::SetNoteVelocity
           || verb == ProjectEditVerb::MoveMidiClip
           || verb == ProjectEditVerb::MoveMidiClipToTrack
           || verb == ProjectEditVerb::RemoveMidiClip;
}

[[nodiscard]] constexpr bool isTrackEditVerb (ProjectEditVerb verb) noexcept
{
    // ADR-0044: send rows live on the Track, so send edits ride the whole-vector track diff.
    return verb == ProjectEditVerb::AddTrack
           || verb == ProjectEditVerb::RenameTrack
           || verb == ProjectEditVerb::ReorderTrack
           || verb == ProjectEditVerb::RemoveTrack
           || verb == ProjectEditVerb::AddSend
           || verb == ProjectEditVerb::RemoveSend
           || verb == ProjectEditVerb::SetSendLevel
           || verb == ProjectEditVerb::SetSendTap
           || verb == ProjectEditVerb::SetTrackOutput
           || verb == ProjectEditVerb::SetTrackMixScalars;
}

[[nodiscard]] constexpr bool isBusEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::AddBus
           || verb == ProjectEditVerb::RemoveBus
           || verb == ProjectEditVerb::SetBusMixScalars
           || verb == ProjectEditVerb::RenameBus;
}

[[nodiscard]] constexpr bool isTimeMapEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::SetProjectTempo
           || verb == ProjectEditVerb::SetProjectMeter;
}

[[nodiscard]] constexpr bool isMarkerEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::AddMarker
           || verb == ProjectEditVerb::RemoveMarker
           || verb == ProjectEditVerb::MoveMarker
           || verb == ProjectEditVerb::RenameMarker;
}

[[nodiscard]] constexpr bool isRecordingCompEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::SetRecordingCompSelection;
}

// E19: the master strip family (one scalar today: gain).
[[nodiscard]] constexpr bool isMasterEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::SetMasterGain;
}

// E33: the take-removal verb owns the combined takes+clips+comp diff family.
[[nodiscard]] constexpr bool isRecordingTakeEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::RemoveRecordingTake;
}

[[nodiscard]] constexpr bool isFxEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::AddFxInsert
           || verb == ProjectEditVerb::RemoveFxInsert
           || verb == ProjectEditVerb::ReorderFxInsert
           || verb == ProjectEditVerb::SetFxInsertEnabled
           || verb == ProjectEditVerb::SetFxInsertParam;
}

[[nodiscard]] constexpr bool isAutomationEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::AddAutomationLane
           || verb == ProjectEditVerb::RemoveAutomationLane
           || verb == ProjectEditVerb::AddAutomationBreakpoint
           || verb == ProjectEditVerb::MoveAutomationBreakpoint
           || verb == ProjectEditVerb::SetAutomationBreakpointValue
           || verb == ProjectEditVerb::SetAutomationBreakpointCurve
           || verb == ProjectEditVerb::RemoveAutomationBreakpoint;
}

[[nodiscard]] inline ProjectEditStatus applyProjectEditCommandToProject (Project& project,
                                                                         const ProjectEditCommand& command)
{
    switch (command.verb)
    {
        case ProjectEditVerb::MoveClip:
            return moveClip (project, command.clipId, command.timelineStart);

        case ProjectEditVerb::TrimClip:
            return trimClip (project, command.clipId, command.timelineStart, command.timelineLength, command.srcOffset, command.srcLen);

        case ProjectEditVerb::SplitClip:
            return splitClip (project, command.clipId, command.rightClipId, command.timelineLength, command.srcLen);

        case ProjectEditVerb::SetClipGain:
            return setClipGain (project, command.clipId, command.gain);

        case ProjectEditVerb::SetClipFades:
            return setClipFades (project, command.clipId, command.fadeIn, command.fadeOut);

        case ProjectEditVerb::MoveNote:
            return moveNote (project, command.midiClipId, command.noteId, command.noteStartTick);

        case ProjectEditVerb::SetNoteLength:
            return setNoteLength (project, command.midiClipId, command.noteId, command.noteLengthTicks);

        case ProjectEditVerb::SplitNote:
            return splitNote (project, command.midiClipId, command.noteId, command.rightNoteId, command.noteLengthTicks);

        case ProjectEditVerb::CutNote:
            return cutNote (project, command.midiClipId, command.noteId);

        case ProjectEditVerb::QuantizeNote:
            return quantizeNote (project, command.midiClipId, command.noteId, SnapGrid { command.snapGridTicks });

        case ProjectEditVerb::TransposeNote:
            return transposeNote (project, command.midiClipId, command.noteId, command.semitones);

        case ProjectEditVerb::SetRecordingCompSelection:
            return setRecordingCompSelection (
                project,
                command.firstCompSegmentId,
                command.firstCompTakeId,
                command.firstCompTimelineStart,
                command.firstCompTimelineLength,
                command.firstCompSourceOffset,
                command.secondCompSegmentId,
                command.secondCompTakeId,
                command.secondCompTimelineStart,
                command.secondCompTimelineLength,
                command.secondCompSourceOffset);

        case ProjectEditVerb::RemoveRecordingTake:
            return removeRecordingTake (project, command.firstCompTakeId);

        case ProjectEditVerb::AddFxInsert:
            return addFxInsert (
                project,
                command.fxOwnerId,
                FxInsert { command.fxInsertId, command.fxKind, command.fxEnabled, {} },
                command.fxPosition);

        case ProjectEditVerb::RemoveFxInsert:
            return removeFxInsert (project, command.fxOwnerId, command.fxInsertId);

        case ProjectEditVerb::ReorderFxInsert:
            return reorderFxInsert (project, command.fxOwnerId, command.fxInsertId, command.fxPosition);

        case ProjectEditVerb::SetFxInsertEnabled:
            return setFxInsertEnabled (project, command.fxOwnerId, command.fxInsertId, command.fxEnabled);

        case ProjectEditVerb::SetFxInsertParam:
            return setFxInsertParam (project, command.fxOwnerId, command.fxInsertId, command.fxParamId, command.fxParamValue);

        case ProjectEditVerb::AddAutomationLane:
            return addAutomationLane (
                project,
                AutomationLaneData { command.automationLaneId, command.automationOwnerId, command.automationRole, command.automationParamId, {} });

        case ProjectEditVerb::RemoveAutomationLane:
            return removeAutomationLane (project, command.automationLaneId);

        case ProjectEditVerb::AddAutomationBreakpoint:
            return addAutomationBreakpoint (
                project,
                command.automationLaneId,
                AutomationBreakpoint { command.automationTick, command.automationValue, command.automationCurveType });

        case ProjectEditVerb::MoveAutomationBreakpoint:
            return moveAutomationBreakpoint (project, command.automationLaneId, command.automationTick, command.automationNewTick);

        case ProjectEditVerb::SetAutomationBreakpointValue:
            return setAutomationBreakpointValue (project, command.automationLaneId, command.automationTick, command.automationValue);

        case ProjectEditVerb::SetAutomationBreakpointCurve:
            return setAutomationBreakpointCurve (project, command.automationLaneId, command.automationTick, command.automationCurveType);

        case ProjectEditVerb::RemoveAutomationBreakpoint:
            return removeAutomationBreakpoint (project, command.automationLaneId, command.automationTick);

        case ProjectEditVerb::AddClip:
        {
            Clip clip;
            clip.id = command.clipId;
            clip.assetId = command.clipAssetId;
            clip.trackId = command.trackId;
            clip.timelineStart = command.timelineStart;
            clip.timelineLength = command.timelineLength;
            clip.srcOffset = command.srcOffset;
            clip.srcLen = command.srcLen;
            clip.gain = command.gain;
            clip.fadeIn = command.fadeIn;
            clip.fadeOut = command.fadeOut;
            clip.timeBase = command.clipTimeBase;
            if (command.clipName[0] == '\0')
                return ProjectEditStatus::InvalidClipName;
            (void) clip.name.assign (std::string_view { command.clipName });
            return addClip (project, clip);
        }

        case ProjectEditVerb::DeleteClip:
            return deleteClip (project, command.clipId);

        case ProjectEditVerb::MoveClipToTrack:
            return moveClipToTrack (project, command.clipId, command.trackId, command.timelineStart);

        case ProjectEditVerb::AddNote:
        {
            Note note;
            note.id = command.noteId;
            note.startTick = command.noteStartTick;
            note.lengthTicks = command.noteLengthTicks;
            note.key = command.noteKey;
            note.pitchNote = command.notePitch;
            note.normalizedVelocity = command.noteVelocity;
            note.portIndex = command.notePort;
            note.channel = command.noteChannel;
            return addNote (project, command.midiClipId, note);
        }

        case ProjectEditVerb::AddTrack:
        {
            if (command.trackName[0] == '\0')
                return ProjectEditStatus::InvalidTrackName;

            Track track;
            track.id = command.trackId;
            track.strip.name = std::string { command.trackName };
            return addTrack (project, track);
        }

        case ProjectEditVerb::RenameTrack:
            if (command.trackName[0] == '\0')
                return ProjectEditStatus::InvalidTrackName;
            return renameTrack (project, command.trackId, std::string { command.trackName });

        case ProjectEditVerb::ReorderTrack:
            return reorderTrack (project, command.trackId, command.trackPosition);

        case ProjectEditVerb::RemoveTrack:
            return removeTrack (project, command.trackId);

        case ProjectEditVerb::SetProjectTempo:
            return setProjectTempo (project, command.tempoBpm);

        case ProjectEditVerb::SetProjectMeter:
            return setProjectMeter (project, command.meterNumerator, command.meterDenominator);

        case ProjectEditVerb::AddMarker:
        {
            Marker marker;
            marker.id = command.markerId;
            marker.tick = command.markerTick;
            marker.name = std::string { command.trackName };
            return addMarker (project, marker);
        }

        case ProjectEditVerb::RemoveMarker:
            return removeMarker (project, command.markerId);

        case ProjectEditVerb::MoveMarker:
            return moveMarker (project, command.markerId, command.markerTick);

        case ProjectEditVerb::RenameMarker:
            return renameMarker (project, command.markerId, std::string_view { command.trackName });

        case ProjectEditVerb::MoveMidiClip:
            return moveMidiClip (project, command.midiClipId, command.timelineStart);

        case ProjectEditVerb::MoveMidiClipToTrack:
            return moveMidiClipToTrack (project, command.midiClipId, command.trackId, command.timelineStart);

        case ProjectEditVerb::RemoveMidiClip:
            return removeMidiClip (project, command.midiClipId);

        case ProjectEditVerb::AddMidiClip:
        {
            MidiClip midiClip;
            midiClip.id = command.midiClipId;
            midiClip.trackId = command.trackId;
            midiClip.timelineStart = command.timelineStart;
            midiClip.timelineLength = command.timelineLength;
            midiClip.timeBase = command.clipTimeBase;
            return addMidiClip (project, midiClip);
        }

        case ProjectEditVerb::AddBus:
        {
            Bus bus;
            bus.id = command.busId;
            bus.strip.name = std::string { command.trackName };
            return addBus (project, bus);
        }

        case ProjectEditVerb::RemoveBus:
            return removeBus (project, command.busId);

        case ProjectEditVerb::AddSend:
        {
            SendRow send;
            send.id = command.sendId;
            send.busId = command.busId;
            send.tap = command.sendTap;
            send.linearGain = command.sendLinearGain;
            return addSend (project, command.trackId, send);
        }

        case ProjectEditVerb::RemoveSend:
            return removeSend (project, command.trackId, command.sendId);

        case ProjectEditVerb::SetSendLevel:
            return setSendLevel (project, command.trackId, command.sendId, command.sendLinearGain);

        case ProjectEditVerb::SetSendTap:
            return setSendTap (project, command.trackId, command.sendId, command.sendTap);

        case ProjectEditVerb::SetTrackOutput:
            return setTrackOutput (project, command.trackId, command.busId);

        case ProjectEditVerb::SetMasterGain:
            return setMasterGain (project, command.gain);

        case ProjectEditVerb::RenameClip:
            if (command.clipName[0] == '\0')
                return ProjectEditStatus::InvalidClipName;
            return renameClip (project, command.clipId, std::string_view { command.clipName });

        case ProjectEditVerb::SetTrackMixScalars:
            return setTrackMixScalars (project,
                                       command.trackId,
                                       command.gain,
                                       command.trackPan,
                                       command.trackMuted,
                                       command.trackSoloed,
                                       command.trackSoloSafe);

        case ProjectEditVerb::SetBusMixScalars:
            return setBusMixScalars (project,
                                     command.busId,
                                     command.gain,
                                     command.trackPan,
                                     command.trackMuted,
                                     command.trackSoloed,
                                     command.trackSoloSafe);

        case ProjectEditVerb::RenameBus:
            if (command.trackName[0] == '\0')
                return ProjectEditStatus::InvalidTrackName;
            return renameBus (project, command.busId, std::string { command.trackName });

        case ProjectEditVerb::SetNoteVelocity:
            return setNoteVelocity (project, command.midiClipId, command.noteId, command.noteVelocity);
    }

    return ProjectEditStatus::InvalidProject;
}

[[nodiscard]] inline bool buildProjectClipRowsDiff (const Project& before,
                                                    const Project& after,
                                                    const ProjectEditCommand& command,
                                                    ProjectClipRowsDiff& out)
{
    if (command.verb == ProjectEditVerb::AddClip)
    {
        std::size_t addedIndex = 0;
        if (! findClipIndex (after, command.clipId, addedIndex)
            || after.clips.size() != before.clips.size() + 1u)
            return false;

        out = {};
        out.firstClipIndex = addedIndex;
        out.before = {};
        out.after = { after.clips[addedIndex] };
        return true;
    }

    std::size_t index = 0;
    if (! findClipIndex (before, command.clipId, index))
        return false;

    out = {};
    out.firstClipIndex = index;
    out.before = { before.clips[index] };

    if (command.verb == ProjectEditVerb::SplitClip)
    {
        if (after.clips.size() != before.clips.size() + 1u || index + 1u >= after.clips.size())
            return false;

        if (after.clips[index].id != command.clipId || after.clips[index + 1u].id != command.rightClipId)
            return false;

        out.after = { after.clips[index], after.clips[index + 1u] };
        return true;
    }

    if (command.verb == ProjectEditVerb::DeleteClip)
    {
        if (after.clips.size() + 1u != before.clips.size())
            return false;

        out.after = {};
        return true;
    }

    if (after.clips.size() != before.clips.size() || index >= after.clips.size() || after.clips[index].id != command.clipId)
        return false;

    out.after = { after.clips[index] };
    return true;
}

[[nodiscard]] inline bool buildProjectMidiClipRowsDiff (const Project& before,
                                                        const Project& after,
                                                        const ProjectEditCommand& command,
                                                        ProjectMidiClipRowsDiff& out)
{
    if (command.verb == ProjectEditVerb::AddMidiClip)
    {
        std::size_t addedIndex = 0;
        if (! findMidiClipIndex (after, command.midiClipId, addedIndex)
            || after.midiClips.size() != before.midiClips.size() + 1u)
            return false;

        out = {};
        out.firstMidiClipIndex = addedIndex;
        out.before = {};
        out.after = { after.midiClips[addedIndex] };
        return true;
    }

    // E8: removal is the add shape inverted — undo re-inserts the snapshot at its old index.
    if (command.verb == ProjectEditVerb::RemoveMidiClip)
    {
        std::size_t removedIndex = 0;
        if (! findMidiClipIndex (before, command.midiClipId, removedIndex)
            || after.midiClips.size() + 1u != before.midiClips.size())
            return false;

        out = {};
        out.firstMidiClipIndex = removedIndex;
        out.before = { before.midiClips[removedIndex] };
        out.after = {};
        return true;
    }

    std::size_t index = 0;
    if (! findMidiClipIndex (before, command.midiClipId, index))
        return false;

    if (after.midiClips.size() != before.midiClips.size()
        || index >= after.midiClips.size()
        || after.midiClips[index].id != command.midiClipId)
        return false;

    out = {};
    out.firstMidiClipIndex = index;
    out.before = { before.midiClips[index] };
    out.after = { after.midiClips[index] };
    return true;
}

[[nodiscard]] inline bool buildProjectRecordingCompRowsDiff (const Project& before,
                                                             const Project& after,
                                                             ProjectRecordingCompRowsDiff& out)
{
    if (after.recordingCompSegments.empty()
        || after.recordingCompSegments == before.recordingCompSegments)
        return false;

    out = {};
    out.before = before.recordingCompSegments;
    out.after = after.recordingCompSegments;
    return true;
}

[[nodiscard]] inline bool buildProjectFxChainRowsDiff (const Project& before,
                                                       const Project& after,
                                                       const ProjectEditCommand& command,
                                                       ProjectFxChainRowsDiff& out)
{
    const MixerStripState* const beforeStrip = findMixerStrip (before, command.fxOwnerId);
    const MixerStripState* const afterStrip = findMixerStrip (after, command.fxOwnerId);
    if (beforeStrip == nullptr || afterStrip == nullptr || beforeStrip->fxChain == afterStrip->fxChain)
        return false;

    out = {};
    out.ownerId = command.fxOwnerId;
    out.before = beforeStrip->fxChain;
    out.after = afterStrip->fxChain;
    return true;
}

[[nodiscard]] inline bool buildProjectAutomationLaneRowsDiff (const Project& before,
                                                              const Project& after,
                                                              const ProjectEditCommand& command,
                                                              ProjectAutomationLaneRowsDiff& out)
{
    out = {};

    if (command.verb == ProjectEditVerb::AddAutomationLane)
    {
        std::size_t afterIndex = 0;
        if (! findAutomationLaneIndex (after, command.automationLaneId, afterIndex))
            return false;

        out.firstAutomationLaneIndex = afterIndex;
        out.after = { after.automationLanes[afterIndex] };
        return true;
    }

    std::size_t beforeIndex = 0;
    if (! findAutomationLaneIndex (before, command.automationLaneId, beforeIndex))
        return false;

    out.firstAutomationLaneIndex = beforeIndex;
    out.before = { before.automationLanes[beforeIndex] };

    if (command.verb == ProjectEditVerb::RemoveAutomationLane)
        return true;

    std::size_t afterIndex = 0;
    if (! findAutomationLaneIndex (after, command.automationLaneId, afterIndex) || afterIndex != beforeIndex)
        return false;

    out.after = { after.automationLanes[afterIndex] };
    return out.before != out.after;
}

[[nodiscard]] inline bool buildProjectMarkerRowsDiff (const Project& before,
                                                      const Project& after,
                                                      ProjectMarkerRowsDiff& out)
{
    if (before.markers == after.markers)
        return false;

    out = {};
    out.before = before.markers;
    out.after = after.markers;
    return true;
}

[[nodiscard]] inline bool applyMarkerRowsDiff (Project& project,
                                               const std::vector<Marker>& expected,
                                               const std::vector<Marker>& replacement)
{
    if (! (project.markers == expected))
        return false;

    Project edited = project;
    edited.markers = replacement;
    if (! edited.hasValidAssetClipIndirection())
        return false;

    project = std::move (edited);
    return true;
}

// E19: master gain diff — bit-exact scalar swap with the same expected-state guard.
[[nodiscard]] inline bool buildProjectMasterGainDiff (const Project& before,
                                                      const Project& after,
                                                      ProjectMasterGainDiff& out)
{
    if (before.masterLinearGain == after.masterLinearGain)
        return false;

    out = {};
    out.before = before.masterLinearGain;
    out.after = after.masterLinearGain;
    return true;
}

[[nodiscard]] inline bool applyMasterGainDiff (Project& project, float expected, float replacement)
{
    if (project.masterLinearGain != expected || ! mixerGainIsValid (replacement))
        return false;

    project.masterLinearGain = replacement;
    return true;
}

[[nodiscard]] inline bool buildProjectTimeMapRowsDiff (const Project& before,
                                                       const Project& after,
                                                       ProjectTimeMapRowsDiff& out)
{
    if (before.tempoMap == after.tempoMap && before.meterMap == after.meterMap)
        return false;

    out = {};
    out.tempoBefore = before.tempoMap;
    out.tempoAfter = after.tempoMap;
    out.meterBefore = before.meterMap;
    out.meterAfter = after.meterMap;
    return true;
}

[[nodiscard]] inline bool applyTimeMapRowsDiff (Project& project,
                                                const std::vector<TempoChange>& expectedTempo,
                                                const std::vector<TempoChange>& replacementTempo,
                                                const std::vector<MeterChange>& expectedMeter,
                                                const std::vector<MeterChange>& replacementMeter)
{
    if (! (project.tempoMap == expectedTempo) || ! (project.meterMap == expectedMeter))
        return false;

    Project edited = project;
    edited.tempoMap = replacementTempo;
    edited.meterMap = replacementMeter;
    if (! edited.hasValidAssetClipIndirection())
        return false;

    project = std::move (edited);
    return true;
}

[[nodiscard]] inline bool buildProjectTrackRowsDiff (const Project& before,
                                                     const Project& after,
                                                     ProjectTrackRowsDiff& out)
{
    if (before.tracks == after.tracks)
        return false;

    out = {};
    out.before = before.tracks;
    out.after = after.tracks;
    return true;
}

[[nodiscard]] inline bool applyTrackRowsDiff (Project& project,
                                              const std::vector<Track>& expected,
                                              const std::vector<Track>& replacement)
{
    if (! (project.tracks == expected))
        return false;

    Project edited = project;
    edited.tracks = replacement;
    if (! edited.hasValidAssetClipIndirection())
        return false;

    project = std::move (edited);
    return true;
}

[[nodiscard]] inline bool buildProjectBusRowsDiff (const Project& before,
                                                   const Project& after,
                                                   ProjectBusRowsDiff& out)
{
    if (before.buses == after.buses)
        return false;

    out = {};
    out.before = before.buses;
    out.after = after.buses;
    return true;
}

[[nodiscard]] inline bool applyBusRowsDiff (Project& project,
                                            const std::vector<Bus>& expected,
                                            const std::vector<Bus>& replacement)
{
    if (! (project.buses == expected))
        return false;

    Project edited = project;
    edited.buses = replacement;
    if (! edited.hasValidAssetClipIndirection())
        return false;

    project = std::move (edited);
    return true;
}

[[nodiscard]] inline bool clipRowsEqualAt (const Project& project,
                                           std::size_t firstClipIndex,
                                           const std::vector<Clip>& expected) noexcept
{
    if (firstClipIndex > project.clips.size() || expected.size() > project.clips.size() - firstClipIndex)
        return false;

    for (std::size_t i = 0; i < expected.size(); ++i)
        if (! (project.clips[firstClipIndex + i] == expected[i]))
            return false;

    return true;
}

[[nodiscard]] inline bool midiClipRowsEqualAt (const Project& project,
                                               std::size_t firstMidiClipIndex,
                                               const std::vector<MidiClip>& expected) noexcept
{
    if (firstMidiClipIndex > project.midiClips.size() || expected.size() > project.midiClips.size() - firstMidiClipIndex)
        return false;

    for (std::size_t i = 0; i < expected.size(); ++i)
        if (! (project.midiClips[firstMidiClipIndex + i] == expected[i]))
            return false;

    return true;
}

[[nodiscard]] inline bool recordingCompRowsEqual (const Project& project,
                                                  const std::vector<ProjectRecordingCompSegment>& expected) noexcept
{
    return project.recordingCompSegments == expected;
}

[[nodiscard]] inline bool fxChainRowsEqual (const Project& project,
                                            EntityId ownerId,
                                            const std::vector<FxInsert>& expected) noexcept
{
    const MixerStripState* const strip = findMixerStrip (project, ownerId);
    return strip != nullptr && strip->fxChain == expected;
}

[[nodiscard]] inline bool automationLaneRowsEqualAt (const Project& project,
                                                     std::size_t firstAutomationLaneIndex,
                                                     const std::vector<AutomationLaneData>& expected) noexcept
{
    if (firstAutomationLaneIndex > project.automationLanes.size()
        || expected.size() > project.automationLanes.size() - firstAutomationLaneIndex)
        return false;

    for (std::size_t i = 0; i < expected.size(); ++i)
        if (! (project.automationLanes[firstAutomationLaneIndex + i] == expected[i]))
            return false;

    return true;
}

[[nodiscard]] inline bool applyClipRowsDiff (Project& project,
                                             const ProjectClipRowsDiff& diff,
                                             const std::vector<Clip>& expected,
                                             const std::vector<Clip>& replacement)
{
    if (! clipRowsEqualAt (project, diff.firstClipIndex, expected))
        return false;

    Project edited = project;
    const auto first = edited.clips.begin() + static_cast<std::ptrdiff_t> (diff.firstClipIndex);
    edited.clips.erase (first, first + static_cast<std::ptrdiff_t> (expected.size()));
    edited.clips.insert (edited.clips.begin() + static_cast<std::ptrdiff_t> (diff.firstClipIndex),
                         replacement.begin(),
                         replacement.end());

    project = edited;
    return true;
}

[[nodiscard]] inline bool applyMidiClipRowsDiff (Project& project,
                                                 const ProjectMidiClipRowsDiff& diff,
                                                 const std::vector<MidiClip>& expected,
                                                 const std::vector<MidiClip>& replacement)
{
    if (! midiClipRowsEqualAt (project, diff.firstMidiClipIndex, expected))
        return false;

    Project edited = project;
    const auto first = edited.midiClips.begin() + static_cast<std::ptrdiff_t> (diff.firstMidiClipIndex);
    edited.midiClips.erase (first, first + static_cast<std::ptrdiff_t> (expected.size()));
    edited.midiClips.insert (edited.midiClips.begin() + static_cast<std::ptrdiff_t> (diff.firstMidiClipIndex),
                             replacement.begin(),
                             replacement.end());

    project = edited;
    return true;
}

[[nodiscard]] inline bool applyRecordingCompRowsDiff (Project& project,
                                                      const std::vector<ProjectRecordingCompSegment>& expected,
                                                      const std::vector<ProjectRecordingCompSegment>& replacement)
{
    if (! recordingCompRowsEqual (project, expected))
        return false;

    Project edited = project;
    edited.recordingCompSegments = replacement;
    if (! edited.hasValidAssetClipIndirection())
        return false;

    project = std::move (edited);
    return true;
}

// E33: the combined takes+clips+comp diff — verify all three families, then replace all three.
[[nodiscard]] inline bool buildProjectRecordingTakeRowsDiff (const Project& before,
                                                             const Project& after,
                                                             ProjectRecordingTakeRowsDiff& out)
{
    if (before.recordingTakes == after.recordingTakes
        && before.clips == after.clips
        && before.recordingCompSegments == after.recordingCompSegments)
        return false;

    out = {};
    out.takesBefore = before.recordingTakes;
    out.takesAfter = after.recordingTakes;
    out.clipsBefore = before.clips;
    out.clipsAfter = after.clips;
    out.compBefore = before.recordingCompSegments;
    out.compAfter = after.recordingCompSegments;
    return true;
}

[[nodiscard]] inline bool applyRecordingTakeRowsDiff (Project& project,
                                                      const ProjectRecordingTakeRowsDiff& diff,
                                                      bool redo)
{
    const auto& takesExpected = redo ? diff.takesBefore : diff.takesAfter;
    const auto& clipsExpected = redo ? diff.clipsBefore : diff.clipsAfter;
    const auto& compExpected = redo ? diff.compBefore : diff.compAfter;
    if (project.recordingTakes != takesExpected
        || project.clips != clipsExpected
        || project.recordingCompSegments != compExpected)
        return false;

    Project edited = project;
    edited.recordingTakes = redo ? diff.takesAfter : diff.takesBefore;
    edited.clips = redo ? diff.clipsAfter : diff.clipsBefore;
    edited.recordingCompSegments = redo ? diff.compAfter : diff.compBefore;
    if (! edited.hasValidAssetClipIndirection())
        return false;

    project = std::move (edited);
    return true;
}

[[nodiscard]] inline bool applyFxChainRowsDiff (Project& project,
                                                EntityId ownerId,
                                                const std::vector<FxInsert>& expected,
                                                const std::vector<FxInsert>& replacement)
{
    if (! fxChainRowsEqual (project, ownerId, expected))
        return false;

    Project edited = project;
    MixerStripState* const strip = findMixerStrip (edited, ownerId);
    if (strip == nullptr)
        return false;

    strip->fxChain = replacement;
    if (! edited.hasValidAssetClipIndirection())
        return false;

    project = std::move (edited);
    return true;
}

[[nodiscard]] inline bool applyAutomationLaneRowsDiff (Project& project,
                                                       const ProjectAutomationLaneRowsDiff& diff,
                                                       const std::vector<AutomationLaneData>& expected,
                                                       const std::vector<AutomationLaneData>& replacement)
{
    if (! automationLaneRowsEqualAt (project, diff.firstAutomationLaneIndex, expected))
        return false;

    Project edited = project;
    const auto first = edited.automationLanes.begin() + static_cast<std::ptrdiff_t> (diff.firstAutomationLaneIndex);
    edited.automationLanes.erase (first, first + static_cast<std::ptrdiff_t> (expected.size()));
    edited.automationLanes.insert (edited.automationLanes.begin() + static_cast<std::ptrdiff_t> (diff.firstAutomationLaneIndex),
                                   replacement.begin(),
                                   replacement.end());
    if (! edited.hasValidAssetClipIndirection())
        return false;

    project = std::move (edited);
    return true;
}

[[nodiscard]] inline bool canCoalesceProjectEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::MoveClip
           || verb == ProjectEditVerb::TrimClip
           || verb == ProjectEditVerb::SetClipGain
           || verb == ProjectEditVerb::SetClipFades
           || verb == ProjectEditVerb::MoveNote
           || verb == ProjectEditVerb::SetNoteLength
           // E21: continuous strip-scalar gestures (fader/pan drags) coalesce inside a group.
           || verb == ProjectEditVerb::SetTrackMixScalars
           || verb == ProjectEditVerb::SetBusMixScalars
           || verb == ProjectEditVerb::SetMasterGain;
}

[[nodiscard]] inline bool canCoalesceProjectEditTransactions (const ProjectEditTransaction& older,
                                                              const ProjectEditTransaction& newer)
{
    if (older.command.verb != newer.command.verb || ! canCoalesceProjectEditVerb (older.command.verb))
        return false;

    if (isMidiNoteEditVerb (older.command.verb))
    {
        return older.command.midiClipId == newer.command.midiClipId
               && older.command.noteId == newer.command.noteId
               && older.midiDiff.firstMidiClipIndex == newer.midiDiff.firstMidiClipIndex
               && older.midiDiff.before.size() == 1u
               && older.midiDiff.after.size() == 1u
               && newer.midiDiff.before.size() == 1u
               && newer.midiDiff.after.size() == 1u
               && older.midiDiff.after == newer.midiDiff.before;
    }

    // E21: strip-scalar gestures coalesce only while the flags stay put — a drag moves gain or
    // pan continuously; mute/solo toggles always keep their own undo steps.
    if (older.command.verb == ProjectEditVerb::SetTrackMixScalars)
    {
        return older.command.trackId == newer.command.trackId
               && older.command.trackMuted == newer.command.trackMuted
               && older.command.trackSoloed == newer.command.trackSoloed
               && older.command.trackSoloSafe == newer.command.trackSoloSafe
               && older.trackDiff.after == newer.trackDiff.before;
    }

    if (older.command.verb == ProjectEditVerb::SetBusMixScalars)
    {
        return older.command.busId == newer.command.busId
               && older.command.trackMuted == newer.command.trackMuted
               && older.command.trackSoloed == newer.command.trackSoloed
               && older.command.trackSoloSafe == newer.command.trackSoloSafe
               && older.busDiff.after == newer.busDiff.before;
    }

    if (older.command.verb == ProjectEditVerb::SetMasterGain)
        return older.masterDiff.after == newer.masterDiff.before;

    return older.command.clipId == newer.command.clipId
           && older.diff.firstClipIndex == newer.diff.firstClipIndex
           && older.diff.before.size() == 1u
           && older.diff.after.size() == 1u
           && newer.diff.before.size() == 1u
           && newer.diff.after.size() == 1u
           && older.diff.after == newer.diff.before;
}

[[nodiscard]] inline bool applyProjectEditTransactionDiff (Project& project,
                                                           const ProjectEditTransaction& transaction,
                                                           bool redo)
{
    if (isMidiNoteEditVerb (transaction.command.verb))
    {
        return redo ? applyMidiClipRowsDiff (project, transaction.midiDiff, transaction.midiDiff.before, transaction.midiDiff.after)
                    : applyMidiClipRowsDiff (project, transaction.midiDiff, transaction.midiDiff.after, transaction.midiDiff.before);
    }

    if (isRecordingCompEditVerb (transaction.command.verb))
    {
        return redo ? applyRecordingCompRowsDiff (project, transaction.recordingCompDiff.before, transaction.recordingCompDiff.after)
                    : applyRecordingCompRowsDiff (project, transaction.recordingCompDiff.after, transaction.recordingCompDiff.before);
    }

    // E33: take removal replays its combined takes+clips+comp snapshot.
    if (isRecordingTakeEditVerb (transaction.command.verb))
        return applyRecordingTakeRowsDiff (project, transaction.recordingTakeDiff, redo);

    if (isFxEditVerb (transaction.command.verb))
    {
        return redo ? applyFxChainRowsDiff (project, transaction.fxDiff.ownerId, transaction.fxDiff.before, transaction.fxDiff.after)
                    : applyFxChainRowsDiff (project, transaction.fxDiff.ownerId, transaction.fxDiff.after, transaction.fxDiff.before);
    }

    if (isAutomationEditVerb (transaction.command.verb))
    {
        return redo ? applyAutomationLaneRowsDiff (project, transaction.automationDiff, transaction.automationDiff.before, transaction.automationDiff.after)
                    : applyAutomationLaneRowsDiff (project, transaction.automationDiff, transaction.automationDiff.after, transaction.automationDiff.before);
    }

    if (isTrackEditVerb (transaction.command.verb))
    {
        return redo ? applyTrackRowsDiff (project, transaction.trackDiff.before, transaction.trackDiff.after)
                    : applyTrackRowsDiff (project, transaction.trackDiff.after, transaction.trackDiff.before);
    }

    if (isBusEditVerb (transaction.command.verb))
    {
        return redo ? applyBusRowsDiff (project, transaction.busDiff.before, transaction.busDiff.after)
                    : applyBusRowsDiff (project, transaction.busDiff.after, transaction.busDiff.before);
    }

    if (isTimeMapEditVerb (transaction.command.verb))
    {
        const ProjectTimeMapRowsDiff& diff = transaction.timeMapDiff;
        return redo ? applyTimeMapRowsDiff (project, diff.tempoBefore, diff.tempoAfter, diff.meterBefore, diff.meterAfter)
                    : applyTimeMapRowsDiff (project, diff.tempoAfter, diff.tempoBefore, diff.meterAfter, diff.meterBefore);
    }

    if (isMarkerEditVerb (transaction.command.verb))
    {
        const ProjectMarkerRowsDiff& diff = transaction.markerDiff;
        return redo ? applyMarkerRowsDiff (project, diff.before, diff.after)
                    : applyMarkerRowsDiff (project, diff.after, diff.before);
    }

    if (isMasterEditVerb (transaction.command.verb))
    {
        const ProjectMasterGainDiff& diff = transaction.masterDiff;
        return redo ? applyMasterGainDiff (project, diff.before, diff.after)
                    : applyMasterGainDiff (project, diff.after, diff.before);
    }

    return redo ? applyClipRowsDiff (project, transaction.diff, transaction.diff.before, transaction.diff.after)
                : applyClipRowsDiff (project, transaction.diff, transaction.diff.after, transaction.diff.before);
}

} // namespace detail

[[nodiscard]] inline ProjectEditApplyResult applyProjectEditCommand (Project& project,
                                                                     const ProjectEditCommand& command,
                                                                     ProjectEditTransaction& out)
{
    const Project before = project;

    const ProjectEditStatus status = detail::applyProjectEditCommandToProject (project, command);
    if (status != ProjectEditStatus::Applied)
        return ProjectEditApplyResult { status, false };

    ProjectEditTransaction transaction;
    transaction.command = command;
    bool diffBuilt = false;
    if (detail::isMidiNoteEditVerb (command.verb))
        diffBuilt = detail::buildProjectMidiClipRowsDiff (before, project, command, transaction.midiDiff);
    else if (detail::isRecordingCompEditVerb (command.verb))
        diffBuilt = detail::buildProjectRecordingCompRowsDiff (before, project, transaction.recordingCompDiff);
    else if (detail::isRecordingTakeEditVerb (command.verb))
        diffBuilt = detail::buildProjectRecordingTakeRowsDiff (before, project, transaction.recordingTakeDiff);
    else if (detail::isFxEditVerb (command.verb))
        diffBuilt = detail::buildProjectFxChainRowsDiff (before, project, command, transaction.fxDiff);
    else if (detail::isAutomationEditVerb (command.verb))
        diffBuilt = detail::buildProjectAutomationLaneRowsDiff (before, project, command, transaction.automationDiff);
    else if (detail::isTrackEditVerb (command.verb))
        diffBuilt = detail::buildProjectTrackRowsDiff (before, project, transaction.trackDiff);
    else if (detail::isBusEditVerb (command.verb))
        diffBuilt = detail::buildProjectBusRowsDiff (before, project, transaction.busDiff);
    else if (detail::isTimeMapEditVerb (command.verb))
        diffBuilt = detail::buildProjectTimeMapRowsDiff (before, project, transaction.timeMapDiff);
    else if (detail::isMarkerEditVerb (command.verb))
        diffBuilt = detail::buildProjectMarkerRowsDiff (before, project, transaction.markerDiff);
    else if (detail::isMasterEditVerb (command.verb))
        diffBuilt = detail::buildProjectMasterGainDiff (before, project, transaction.masterDiff);
    else
        diffBuilt = detail::buildProjectClipRowsDiff (before, project, command, transaction.diff);

    if (! diffBuilt)
    {
        project = before;
        return ProjectEditApplyResult { ProjectEditStatus::InvalidProject, false };
    }

    out = transaction;
    return ProjectEditApplyResult { ProjectEditStatus::Applied, true };
}

class ProjectUndoStack final
{
public:
    [[nodiscard]] bool beginTransactionGroup() noexcept
    {
        if (activeGroupId_ != 0)
            return false;

        activeGroupId_ = nextGroupId_++;
        return true;
    }

    [[nodiscard]] bool endTransactionGroup() noexcept
    {
        if (activeGroupId_ == 0)
            return false;

        activeGroupId_ = 0;
        return true;
    }

    // E21: scalar-gesture coalescing — while active, consecutive strip-scalar entries merge
    // into one undo step. Any non-coalescable verb auto-ends it, so an unclosed gesture can
    // never swallow unrelated edits (entries stay groupless and undo one at a time).
    void beginScalarCoalescing() noexcept
    {
        // A NEW gesture never merges into the previous one: seal the current top entry.
        if (! scalarCoalescing_ && ! undo_.empty())
            undo_.back().sealed = true;
        scalarCoalescing_ = true;
    }

    void endScalarCoalescing() noexcept
    {
        if (scalarCoalescing_ && ! undo_.empty())
            undo_.back().sealed = true;
        scalarCoalescing_ = false;
    }

    [[nodiscard]] ProjectEditApplyResult apply (Project& project, const ProjectEditCommand& command)
    {
        ProjectEditTransaction transaction;
        ProjectEditApplyResult result = applyProjectEditCommand (project, command, transaction);
        if (! result.applied())
            return result;

        if (! detail::canCoalesceProjectEditVerb (command.verb))
            scalarCoalescing_ = false;

        if (scalarCoalescing_ && activeGroupId_ == 0 && ! undo_.empty())
        {
            UndoEntry& previous = undo_.back();
            if (previous.groupId == 0
                && ! previous.sealed
                && detail::canCoalesceProjectEditTransactions (previous.transaction, transaction))
            {
                previous.transaction.command = transaction.command;
                if (detail::isTrackEditVerb (transaction.command.verb))
                    previous.transaction.trackDiff.after = transaction.trackDiff.after;
                else if (detail::isBusEditVerb (transaction.command.verb))
                    previous.transaction.busDiff.after = transaction.busDiff.after;
                else if (detail::isMasterEditVerb (transaction.command.verb))
                    previous.transaction.masterDiff.after = transaction.masterDiff.after;
                else
                    previous.transaction.diff.after = transaction.diff.after;

                redo_.clear();
                result.coalesced = true;
                return result;
            }
        }

        if (activeGroupId_ != 0 && ! undo_.empty())
        {
            UndoEntry& previous = undo_.back();
            if (previous.groupId == activeGroupId_
                && detail::canCoalesceProjectEditTransactions (previous.transaction, transaction))
            {
                previous.transaction.command = transaction.command;
                if (detail::isMidiNoteEditVerb (transaction.command.verb))
                    previous.transaction.midiDiff.after = transaction.midiDiff.after;
                else if (detail::isTrackEditVerb (transaction.command.verb))
                    previous.transaction.trackDiff.after = transaction.trackDiff.after;
                else if (detail::isBusEditVerb (transaction.command.verb))
                    previous.transaction.busDiff.after = transaction.busDiff.after;
                else if (detail::isMasterEditVerb (transaction.command.verb))
                    previous.transaction.masterDiff.after = transaction.masterDiff.after;
                else
                    previous.transaction.diff.after = transaction.diff.after;

                redo_.clear();
                result.coalesced = true;
                return result;
            }
        }

        undo_.push_back (UndoEntry { transaction, activeGroupId_ });
        redo_.clear();
        return result;
    }

    [[nodiscard]] ProjectUndoStatus undo (Project& project)
    {
        if (undo_.empty())
            return ProjectUndoStatus::NothingToUndo;

        const std::size_t entryCount = trailingGroupEntryCount (undo_);
        Project next = project;
        for (std::size_t index = 0; index < entryCount; ++index)
        {
            const UndoEntry& entry = undo_[undo_.size() - 1 - index];
            if (! detail::applyProjectEditTransactionDiff (next, entry.transaction, false))
                return ProjectUndoStatus::ProjectMismatch;
        }

        project = std::move (next);
        for (std::size_t index = 0; index < entryCount; ++index)
        {
            redo_.push_back (std::move (undo_.back()));
            undo_.pop_back();
        }
        return ProjectUndoStatus::Applied;
    }

    [[nodiscard]] ProjectUndoStatus redo (Project& project)
    {
        if (redo_.empty())
            return ProjectUndoStatus::NothingToRedo;

        const std::size_t entryCount = trailingGroupEntryCount (redo_);
        Project next = project;
        for (std::size_t index = 0; index < entryCount; ++index)
        {
            const UndoEntry& entry = redo_[redo_.size() - 1 - index];
            if (! detail::applyProjectEditTransactionDiff (next, entry.transaction, true))
                return ProjectUndoStatus::ProjectMismatch;
        }

        project = std::move (next);
        for (std::size_t index = 0; index < entryCount; ++index)
        {
            undo_.push_back (std::move (redo_.back()));
            redo_.pop_back();
        }
        return ProjectUndoStatus::Applied;
    }

    [[nodiscard]] bool canUndo() const noexcept { return ! undo_.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return ! redo_.empty(); }
    [[nodiscard]] bool transactionGroupOpen() const noexcept { return activeGroupId_ != 0; }
    [[nodiscard]] std::size_t undoDepth() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t redoDepth() const noexcept { return redo_.size(); }

    [[nodiscard]] const ProjectEditTransaction* nextUndo() const noexcept
    {
        return undo_.empty() ? nullptr : &undo_.back().transaction;
    }

    [[nodiscard]] const ProjectEditTransaction* nextRedo() const noexcept
    {
        return redo_.empty() ? nullptr : &redo_.back().transaction;
    }

private:
    struct UndoEntry
    {
        ProjectEditTransaction transaction;
        std::uint64_t groupId = 0;
        bool sealed = false;   // E21: a gesture boundary — later scalar edits never merge in
    };

    [[nodiscard]] static std::size_t trailingGroupEntryCount (const std::vector<UndoEntry>& entries) noexcept
    {
        if (entries.empty() || entries.back().groupId == 0)
            return entries.empty() ? 0 : 1;

        const std::uint64_t groupId = entries.back().groupId;
        std::size_t count = 1;
        while (count < entries.size() && entries[entries.size() - 1 - count].groupId == groupId)
            ++count;
        return count;
    }

    std::vector<UndoEntry> undo_;
    std::vector<UndoEntry> redo_;
    std::uint64_t activeGroupId_ = 0;
    std::uint64_t nextGroupId_ = 1;
    bool scalarCoalescing_ = false;   // E21: an open strip-scalar gesture
};

} // namespace yesdaw::engine
