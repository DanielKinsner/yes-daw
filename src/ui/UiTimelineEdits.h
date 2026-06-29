// YES DAW - H11 timeline edit action adapter.
//
// UI controls resolve through stable UiActionIds, then this adapter turns the selected-clip payload into
// the existing Project edit/undo commands. It does not introduce a second edit model.

#pragma once

#include "engine/ProjectUndo.h"
#include "ui/UiActions.h"

#include <cstdint>
#include <utility>

namespace yesdaw::ui {

struct UiTimelineEditPayload
{
    engine::EntityId clipId;
    engine::EntityId rightClipId;
    engine::Tick timelineStart = 0;
    engine::Tick timelineLength = 0;
    std::uint64_t srcOffset = 0;
    std::uint64_t srcLen = 0;
    float gain = 1.0f;
    engine::Tick fadeIn = 0;
    engine::Tick fadeOut = 0;

    [[nodiscard]] static constexpr UiTimelineEditPayload moveTo (engine::EntityId clipId,
                                                                 engine::Tick timelineStart) noexcept
    {
        UiTimelineEditPayload payload;
        payload.clipId = clipId;
        payload.timelineStart = timelineStart;
        return payload;
    }

    [[nodiscard]] static constexpr UiTimelineEditPayload trimTo (engine::EntityId clipId,
                                                                 engine::Tick timelineStart,
                                                                 engine::Tick timelineLength,
                                                                 std::uint64_t srcOffset,
                                                                 std::uint64_t srcLen) noexcept
    {
        UiTimelineEditPayload payload;
        payload.clipId = clipId;
        payload.timelineStart = timelineStart;
        payload.timelineLength = timelineLength;
        payload.srcOffset = srcOffset;
        payload.srcLen = srcLen;
        return payload;
    }

    [[nodiscard]] static constexpr UiTimelineEditPayload splitAt (engine::EntityId clipId,
                                                                  engine::EntityId rightClipId,
                                                                  engine::Tick leftTimelineLength,
                                                                  std::uint64_t leftSourceLength) noexcept
    {
        UiTimelineEditPayload payload;
        payload.clipId = clipId;
        payload.rightClipId = rightClipId;
        payload.timelineLength = leftTimelineLength;
        payload.srcLen = leftSourceLength;
        return payload;
    }

    [[nodiscard]] static constexpr UiTimelineEditPayload setGain (engine::EntityId clipId, float gain) noexcept
    {
        UiTimelineEditPayload payload;
        payload.clipId = clipId;
        payload.gain = gain;
        return payload;
    }

    [[nodiscard]] static constexpr UiTimelineEditPayload setFades (engine::EntityId clipId,
                                                                   engine::Tick fadeIn,
                                                                   engine::Tick fadeOut) noexcept
    {
        UiTimelineEditPayload payload;
        payload.clipId = clipId;
        payload.fadeIn = fadeIn;
        payload.fadeOut = fadeOut;
        return payload;
    }

    [[nodiscard]] static constexpr UiTimelineEditPayload timeStretchToLength (engine::EntityId clipId,
                                                                              engine::Tick timelineLength) noexcept
    {
        UiTimelineEditPayload payload;
        payload.clipId = clipId;
        payload.timelineLength = timelineLength;
        return payload;
    }
};

struct UiTimelineEditResult
{
    UiActionId action = UiActionId::Count;
    UiActionState state {};
    engine::ProjectEditStatus editStatus = engine::ProjectEditStatus::InvalidProject;
    engine::ProjectUndoStatus undoStatus = engine::ProjectUndoStatus::NothingToUndo;
    bool dispatched = false;
    bool recorded = false;
    bool coalesced = false;
};

class UiTimelineEditModel
{
public:
    explicit UiTimelineEditModel (engine::Project project = {}) : project_ (std::move (project))
    {
        syncContext();
    }

    [[nodiscard]] const UiActionRegistry& registry() const noexcept { return registry_; }
    [[nodiscard]] const UiActionContext& context() const noexcept { return context_; }
    [[nodiscard]] const engine::Project& project() const noexcept { return project_; }
    [[nodiscard]] const engine::ProjectUndoStack& undoStack() const noexcept { return undo_; }

    [[nodiscard]] bool selectClip (engine::EntityId clipId) noexcept
    {
        if (findClip (clipId) == nullptr)
        {
            selectedClipId_ = {};
            syncContext();
            return false;
        }

        selectedClipId_ = clipId;
        syncContext();
        return true;
    }

    void clearClipSelection() noexcept
    {
        selectedClipId_ = {};
        syncContext();
    }

    [[nodiscard]] UiTimelineEditResult dispatch (UiActionId id,
                                                 const UiTimelineEditPayload& payload = {})
    {
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state };

        if (id == UiActionId::EditUndo)
            return dispatchUndo (id, state);
        if (id == UiActionId::EditRedo)
            return dispatchRedo (id, state);

        engine::ProjectEditCommand command;
        if (! commandFor (id, payload, command))
            return { id, { false, "unsupported timeline edit action" } };

        const engine::ProjectEditApplyResult applied = undo_.apply (project_, command);
        syncContext();
        if (! applied.applied())
            return { id, state, applied.editStatus, engine::ProjectUndoStatus::NothingToUndo, false, false, applied.coalesced };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, applied.editStatus, engine::ProjectUndoStatus::NothingToUndo, true, applied.recorded, applied.coalesced };
    }

    [[nodiscard]] const engine::ProjectEditCommand* lastAppliedCommand() const noexcept
    {
        const engine::ProjectEditTransaction* transaction = undo_.nextUndo();
        return transaction == nullptr ? nullptr : &transaction->command;
    }

private:
    [[nodiscard]] const engine::Clip* findClip (engine::EntityId clipId) const noexcept
    {
        for (const engine::Clip& clip : project_.clips)
            if (clip.id == clipId)
                return &clip;

        return nullptr;
    }

    [[nodiscard]] engine::EntityId targetClipId (const UiTimelineEditPayload& payload) const noexcept
    {
        return payload.clipId.isValid() ? payload.clipId : selectedClipId_;
    }

    [[nodiscard]] bool commandFor (UiActionId id,
                                   const UiTimelineEditPayload& payload,
                                   engine::ProjectEditCommand& out) const noexcept
    {
        const engine::EntityId clipId = targetClipId (payload);

        switch (id)
        {
            case UiActionId::TimelineClipMove:
                out = engine::ProjectEditCommand::moveClip (clipId, payload.timelineStart);
                return true;

            case UiActionId::TimelineClipTrim:
                out = engine::ProjectEditCommand::trimClip (
                    clipId, payload.timelineStart, payload.timelineLength, payload.srcOffset, payload.srcLen);
                return true;

            case UiActionId::TimelineClipSplit:
                out = engine::ProjectEditCommand::splitClip (
                    clipId, payload.rightClipId, payload.timelineLength, payload.srcLen);
                return true;

            case UiActionId::TimelineClipSetGain:
                out = engine::ProjectEditCommand::setClipGain (clipId, payload.gain);
                return true;

            case UiActionId::TimelineClipSetFades:
                out = engine::ProjectEditCommand::setClipFades (clipId, payload.fadeIn, payload.fadeOut);
                return true;

            case UiActionId::TimelineClipTimeStretch:
            {
                const engine::Clip* clip = findClip (clipId);
                if (clip == nullptr)
                    return false;

                out = engine::ProjectEditCommand::trimClip (
                    clipId, clip->timelineStart, payload.timelineLength, clip->srcOffset, clip->srcLen);
                return true;
            }

            default:
                break;
        }

        return false;
    }

    [[nodiscard]] UiTimelineEditResult dispatchUndo (UiActionId id, UiActionState state)
    {
        const engine::ProjectUndoStatus undoStatus = undo_.undo (project_);
        syncContext();
        if (undoStatus != engine::ProjectUndoStatus::Applied)
            return { id, state, engine::ProjectEditStatus::InvalidProject, undoStatus };

        ++context_.commandDispatchCount;
        ++context_.undoCount;
        return { id, state, engine::ProjectEditStatus::Applied, undoStatus, true };
    }

    [[nodiscard]] UiTimelineEditResult dispatchRedo (UiActionId id, UiActionState state)
    {
        const engine::ProjectUndoStatus undoStatus = undo_.redo (project_);
        syncContext();
        if (undoStatus != engine::ProjectUndoStatus::Applied)
            return { id, state, engine::ProjectEditStatus::InvalidProject, undoStatus };

        ++context_.commandDispatchCount;
        ++context_.redoCount;
        return { id, state, engine::ProjectEditStatus::Applied, undoStatus, true };
    }

    void syncContext() noexcept
    {
        context_.projectLoaded = project_.hasValidAssetClipIndirection();
        context_.canUndo = undo_.canUndo();
        context_.canRedo = undo_.canRedo();
        context_.timelineClipSelected = context_.projectLoaded
            && selectedClipId_.isValid()
            && findClip (selectedClipId_) != nullptr;
        context_.activePanel = UiPanel::Timeline;
    }

    UiActionRegistry registry_;
    UiActionContext context_;
    engine::Project project_;
    engine::ProjectUndoStack undo_;
    engine::EntityId selectedClipId_;
};

} // namespace yesdaw::ui
