// YES DAW - in-memory Project edit command/diff undo surface (H2).
//
// This is control-side document state only: commands wrap the existing Clip edit helpers and record
// exact Clip row before/after diffs for bit-identical undo/redo. SQLite durability is deliberately
// outside this layer.

#pragma once

#include "engine/Project.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace yesdaw::engine {

enum class ProjectEditVerb : std::uint8_t
{
    MoveClip = 0,
    TrimClip,
    SplitClip,
    SetClipGain,
    SetClipFades
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
};

static_assert (std::is_trivially_copyable_v<ProjectEditCommand>,
               "ProjectEditCommand must stay a simple command payload");

struct ProjectClipRowsDiff
{
    std::size_t firstClipIndex = 0;
    std::vector<Clip> before;
    std::vector<Clip> after;
};

struct ProjectEditTransaction
{
    ProjectEditCommand command;
    ProjectClipRowsDiff diff;
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

[[nodiscard]] inline bool canCoalesceProjectEditVerb (ProjectEditVerb verb) noexcept
{
    return verb == ProjectEditVerb::MoveClip
           || verb == ProjectEditVerb::TrimClip
           || verb == ProjectEditVerb::SetClipGain
           || verb == ProjectEditVerb::SetClipFades;
}

[[nodiscard]] inline bool canCoalesceProjectEditTransactions (const ProjectEditTransaction& older,
                                                              const ProjectEditTransaction& newer)
{
    return older.command.verb == newer.command.verb
           && older.command.clipId == newer.command.clipId
           && canCoalesceProjectEditVerb (older.command.verb)
           && older.diff.firstClipIndex == newer.diff.firstClipIndex
           && older.diff.before.size() == 1u
           && older.diff.after.size() == 1u
           && newer.diff.before.size() == 1u
           && newer.diff.after.size() == 1u
           && older.diff.after == newer.diff.before;
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

    ProjectClipRowsDiff diff;
    if (! detail::buildProjectClipRowsDiff (before, project, command, diff))
    {
        project = before;
        return ProjectEditApplyResult { ProjectEditStatus::InvalidProject, false };
    }

    out = ProjectEditTransaction { command, diff };
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
        if (! detail::applyClipRowsDiff (project, transaction.diff, transaction.diff.after, transaction.diff.before))
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
        if (! detail::applyClipRowsDiff (project, transaction.diff, transaction.diff.before, transaction.diff.after))
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
