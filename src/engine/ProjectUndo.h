// YES DAW - in-memory Project edit command/diff undo surface (H2).
//
// This is control-side document state only: commands wrap the existing Clip/Note edit helpers and record
// exact row before/after diffs for bit-identical undo/redo. SQLite durability is deliberately outside
// this layer.

#pragma once

#include "engine/Project.h"

#include <cstddef>
#include <cstdint>
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
    RemoveAutomationBreakpoint
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

struct ProjectEditTransaction
{
    ProjectEditCommand command;
    ProjectClipRowsDiff diff;
    ProjectMidiClipRowsDiff midiDiff;
    ProjectRecordingCompRowsDiff recordingCompDiff;
    ProjectFxChainRowsDiff fxDiff;
    ProjectAutomationLaneRowsDiff automationDiff;
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
           || verb == ProjectEditVerb::TransposeNote;
}

[[nodiscard]] constexpr bool isRecordingCompEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::SetRecordingCompSelection;
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
    }

    return ProjectEditStatus::InvalidProject;
}

[[nodiscard]] inline bool buildProjectClipRowsDiff (const Project& before,
                                                    const Project& after,
                                                    const ProjectEditCommand& command,
                                                    ProjectClipRowsDiff& out)
{
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
           || verb == ProjectEditVerb::SetNoteLength;
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
    else if (detail::isFxEditVerb (command.verb))
        diffBuilt = detail::buildProjectFxChainRowsDiff (before, project, command, transaction.fxDiff);
    else if (detail::isAutomationEditVerb (command.verb))
        diffBuilt = detail::buildProjectAutomationLaneRowsDiff (before, project, command, transaction.automationDiff);
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

    [[nodiscard]] ProjectEditApplyResult apply (Project& project, const ProjectEditCommand& command)
    {
        ProjectEditTransaction transaction;
        ProjectEditApplyResult result = applyProjectEditCommand (project, command, transaction);
        if (! result.applied())
            return result;

        if (activeGroupId_ != 0 && ! undo_.empty())
        {
            UndoEntry& previous = undo_.back();
            if (previous.groupId == activeGroupId_
                && detail::canCoalesceProjectEditTransactions (previous.transaction, transaction))
            {
                previous.transaction.command = transaction.command;
                if (detail::isMidiNoteEditVerb (transaction.command.verb))
                    previous.transaction.midiDiff.after = transaction.midiDiff.after;
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

        const UndoEntry entry = undo_.back();
        const ProjectEditTransaction& transaction = entry.transaction;
        if (! detail::applyProjectEditTransactionDiff (project, transaction, false))
            return ProjectUndoStatus::ProjectMismatch;

        redo_.push_back (entry);
        undo_.pop_back();
        return ProjectUndoStatus::Applied;
    }

    [[nodiscard]] ProjectUndoStatus redo (Project& project)
    {
        if (redo_.empty())
            return ProjectUndoStatus::NothingToRedo;

        const UndoEntry entry = redo_.back();
        const ProjectEditTransaction& transaction = entry.transaction;
        if (! detail::applyProjectEditTransactionDiff (project, transaction, true))
            return ProjectUndoStatus::ProjectMismatch;

        undo_.push_back (entry);
        redo_.pop_back();
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
    };

    std::vector<UndoEntry> undo_;
    std::vector<UndoEntry> redo_;
    std::uint64_t activeGroupId_ = 0;
    std::uint64_t nextGroupId_ = 1;
};

} // namespace yesdaw::engine
