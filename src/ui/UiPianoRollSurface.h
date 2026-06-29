// YES DAW - H11 piano-roll / MIDI Clip UI surface.
//
// Pure control-side adapter: exposes MIDI Clip Notes and expression readback to the UI action surface,
// then routes note edits through the existing ProjectUndoStack commands.

#pragma once

#include "engine/ProjectUndo.h"
#include "ui/UiActions.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace yesdaw::ui {

enum class UiPianoRollExpressionLaneKind : std::uint8_t
{
    Velocity,
    Pitch
};

enum class UiPianoRollActionStatus : std::uint8_t
{
    Ok,
    InvalidAction,
    InvalidSelection
};

struct UiPianoRollNoteView
{
    engine::EntityId noteId {};
    engine::Tick startTick = 0;
    engine::Tick lengthTicks = 0;
    std::int16_t key = 60;
    double pitchNote = 60.0;
    double normalizedVelocity = 1.0;
    std::int16_t portIndex = -1;
    std::int16_t channel = -1;
    bool selected = false;
};

struct UiPianoRollExpressionPoint
{
    engine::EntityId noteId {};
    engine::Tick tick = 0;
    double value = 0.0;
};

struct UiPianoRollExpressionLaneReadout
{
    UiPianoRollExpressionLaneKind kind = UiPianoRollExpressionLaneKind::Velocity;
    std::vector<UiPianoRollExpressionPoint> points;
    bool valid = false;
};

struct UiPianoRollSurfaceSnapshot
{
    bool projectLoaded = false;
    bool midiClipSelected = false;
    engine::EntityId midiClipId {};
    engine::Tick timelineStart = 0;
    engine::Tick timelineLength = 0;
    std::vector<UiPianoRollNoteView> notes;
    std::vector<UiPianoRollExpressionLaneReadout> expressionLanes;
};

struct UiPianoRollActionPayload
{
    engine::EntityId midiClipId {};
    engine::EntityId noteId {};
    engine::Tick noteStartTick = 0;
    engine::Tick noteLengthTicks = 0;
    engine::Tick snapGridTicks = 0;
    std::int32_t semitones = 0;

    [[nodiscard]] static constexpr UiPianoRollActionPayload selectNote (engine::EntityId midiClipId,
                                                                        engine::EntityId noteId) noexcept
    {
        UiPianoRollActionPayload payload;
        payload.midiClipId = midiClipId;
        payload.noteId = noteId;
        return payload;
    }

    [[nodiscard]] static constexpr UiPianoRollActionPayload moveNoteTo (engine::EntityId midiClipId,
                                                                        engine::EntityId noteId,
                                                                        engine::Tick startTick) noexcept
    {
        UiPianoRollActionPayload payload = selectNote (midiClipId, noteId);
        payload.noteStartTick = startTick;
        return payload;
    }

    [[nodiscard]] static constexpr UiPianoRollActionPayload setNoteLength (engine::EntityId midiClipId,
                                                                           engine::EntityId noteId,
                                                                           engine::Tick lengthTicks) noexcept
    {
        UiPianoRollActionPayload payload = selectNote (midiClipId, noteId);
        payload.noteLengthTicks = lengthTicks;
        return payload;
    }

    [[nodiscard]] static constexpr UiPianoRollActionPayload transposeNoteBy (engine::EntityId midiClipId,
                                                                             engine::EntityId noteId,
                                                                             std::int32_t semitones) noexcept
    {
        UiPianoRollActionPayload payload = selectNote (midiClipId, noteId);
        payload.semitones = semitones;
        return payload;
    }

    [[nodiscard]] static constexpr UiPianoRollActionPayload quantizeNoteTo (engine::EntityId midiClipId,
                                                                            engine::EntityId noteId,
                                                                            engine::SnapGrid grid) noexcept
    {
        UiPianoRollActionPayload payload = selectNote (midiClipId, noteId);
        payload.snapGridTicks = grid.intervalTicks;
        return payload;
    }
};

struct UiPianoRollActionResult
{
    UiActionId action = UiActionId::Count;
    UiActionState state {};
    engine::ProjectEditStatus editStatus = engine::ProjectEditStatus::InvalidProject;
    engine::ProjectUndoStatus undoStatus = engine::ProjectUndoStatus::NothingToUndo;
    UiPianoRollActionStatus pianoStatus = UiPianoRollActionStatus::Ok;
    bool dispatched = false;
    bool recorded = false;
    bool coalesced = false;
};

namespace detail {

inline const engine::MidiClip* findMidiClip (const engine::Project& project,
                                             engine::EntityId midiClipId) noexcept
{
    for (const engine::MidiClip& midiClip : project.midiClips)
        if (midiClip.id == midiClipId)
            return &midiClip;

    return nullptr;
}

inline const engine::Note* findNote (const engine::MidiClip& midiClip,
                                     engine::EntityId noteId) noexcept
{
    for (const engine::Note& note : midiClip.notes)
        if (note.id == noteId)
            return &note;

    return nullptr;
}

inline void appendExpressionLane (UiPianoRollSurfaceSnapshot& snapshot,
                                  UiPianoRollExpressionLaneKind kind,
                                  std::span<const engine::Note> notes)
{
    UiPianoRollExpressionLaneReadout lane;
    lane.kind = kind;
    lane.valid = true;
    lane.points.reserve (notes.size());

    for (const engine::Note& note : notes)
    {
        const double value = kind == UiPianoRollExpressionLaneKind::Velocity
            ? note.normalizedVelocity
            : note.pitchNote;
        lane.points.push_back ({ note.id, note.startTick, value });
    }

    snapshot.expressionLanes.push_back (std::move (lane));
}

} // namespace detail

inline UiPianoRollSurfaceSnapshot projectUiPianoRollSurface (const engine::Project& project,
                                                             engine::EntityId selectedMidiClipId = {},
                                                             engine::EntityId selectedNoteId = {})
{
    UiPianoRollSurfaceSnapshot snapshot;
    snapshot.projectLoaded = project.hasValidAssetClipIndirection();
    if (! snapshot.projectLoaded || ! selectedMidiClipId.isValid())
        return snapshot;

    const engine::MidiClip* const midiClip = detail::findMidiClip (project, selectedMidiClipId);
    if (midiClip == nullptr)
        return snapshot;

    snapshot.midiClipSelected = true;
    snapshot.midiClipId = midiClip->id;
    snapshot.timelineStart = midiClip->timelineStart;
    snapshot.timelineLength = midiClip->timelineLength;
    snapshot.notes.reserve (midiClip->notes.size());

    for (const engine::Note& note : midiClip->notes)
    {
        snapshot.notes.push_back ({
            note.id,
            note.startTick,
            note.lengthTicks,
            note.key,
            note.pitchNote,
            note.normalizedVelocity,
            note.portIndex,
            note.channel,
            note.id == selectedNoteId
        });
    }

    detail::appendExpressionLane (
        snapshot,
        UiPianoRollExpressionLaneKind::Velocity,
        std::span<const engine::Note> (midiClip->notes.data(), midiClip->notes.size()));
    detail::appendExpressionLane (
        snapshot,
        UiPianoRollExpressionLaneKind::Pitch,
        std::span<const engine::Note> (midiClip->notes.data(), midiClip->notes.size()));
    return snapshot;
}

class UiPianoRollSurfaceModel
{
public:
    explicit UiPianoRollSurfaceModel (engine::Project project = {}) : project_ (std::move (project))
    {
        syncContext();
    }

    [[nodiscard]] const UiActionRegistry& registry() const noexcept { return registry_; }
    [[nodiscard]] const UiActionContext& context() const noexcept { return context_; }
    [[nodiscard]] const engine::Project& project() const noexcept { return project_; }
    [[nodiscard]] const engine::ProjectUndoStack& undoStack() const noexcept { return undo_; }

    [[nodiscard]] UiPianoRollSurfaceSnapshot snapshot() const
    {
        return projectUiPianoRollSurface (project_, selectedMidiClipId_, selectedNoteId_);
    }

    [[nodiscard]] bool selectMidiClip (engine::EntityId midiClipId) noexcept
    {
        if (detail::findMidiClip (project_, midiClipId) == nullptr)
        {
            selectedMidiClipId_ = {};
            selectedNoteId_ = {};
            syncContext();
            return false;
        }

        selectedMidiClipId_ = midiClipId;
        selectedNoteId_ = {};
        syncContext();
        return true;
    }

    [[nodiscard]] bool selectNote (engine::EntityId noteId) noexcept
    {
        const engine::MidiClip* const midiClip = detail::findMidiClip (project_, selectedMidiClipId_);
        if (midiClip == nullptr || detail::findNote (*midiClip, noteId) == nullptr)
        {
            selectedNoteId_ = {};
            syncContext();
            return false;
        }

        selectedNoteId_ = noteId;
        syncContext();
        return true;
    }

    [[nodiscard]] UiPianoRollActionResult dispatch (UiActionId id,
                                                    const UiPianoRollActionPayload& payload = {})
    {
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state };

        if (id == UiActionId::EditUndo)
            return dispatchUndo (id, state);
        if (id == UiActionId::EditRedo)
            return dispatchRedo (id, state);
        if (id == UiActionId::PianoRollNoteSelect)
            return dispatchSelectNote (id, state, payload);
        if (id == UiActionId::PianoRollReadExpressionLanes)
            return dispatchExpressionRead (id, state);

        engine::ProjectEditCommand command;
        if (! commandFor (id, payload, command))
            return { id, { false, "unsupported piano roll action" }, engine::ProjectEditStatus::InvalidProject,
                     engine::ProjectUndoStatus::NothingToUndo, UiPianoRollActionStatus::InvalidAction };

        const engine::ProjectEditApplyResult applied = undo_.apply (project_, command);
        syncContext();
        if (! applied.applied())
            return { id, state, applied.editStatus, engine::ProjectUndoStatus::NothingToUndo,
                     UiPianoRollActionStatus::Ok, false, false, applied.coalesced };

        ++context_.commandDispatchCount;
        ++context_.midiEditCount;
        return { id, state, applied.editStatus, engine::ProjectUndoStatus::NothingToUndo,
                 UiPianoRollActionStatus::Ok, true, applied.recorded, applied.coalesced };
    }

    [[nodiscard]] const engine::ProjectEditCommand* lastAppliedCommand() const noexcept
    {
        const engine::ProjectEditTransaction* transaction = undo_.nextUndo();
        return transaction == nullptr ? nullptr : &transaction->command;
    }

private:
    [[nodiscard]] engine::EntityId targetMidiClipId (const UiPianoRollActionPayload& payload) const noexcept
    {
        return payload.midiClipId.isValid() ? payload.midiClipId : selectedMidiClipId_;
    }

    [[nodiscard]] engine::EntityId targetNoteId (const UiPianoRollActionPayload& payload) const noexcept
    {
        return payload.noteId.isValid() ? payload.noteId : selectedNoteId_;
    }

    [[nodiscard]] bool commandFor (UiActionId id,
                                   const UiPianoRollActionPayload& payload,
                                   engine::ProjectEditCommand& out) const noexcept
    {
        const engine::EntityId midiClipId = targetMidiClipId (payload);
        const engine::EntityId noteId = targetNoteId (payload);

        switch (id)
        {
            case UiActionId::PianoRollNoteMove:
                out = engine::ProjectEditCommand::moveNote (midiClipId, noteId, payload.noteStartTick);
                return true;

            case UiActionId::PianoRollNoteSetLength:
                out = engine::ProjectEditCommand::setNoteLength (midiClipId, noteId, payload.noteLengthTicks);
                return true;

            case UiActionId::PianoRollNoteTranspose:
                out = engine::ProjectEditCommand::transposeNote (midiClipId, noteId, payload.semitones);
                return true;

            case UiActionId::PianoRollNoteQuantize:
                out = engine::ProjectEditCommand::quantizeNote (
                    midiClipId, noteId, engine::SnapGrid { payload.snapGridTicks });
                return true;

            default:
                break;
        }

        return false;
    }

    [[nodiscard]] UiPianoRollActionResult dispatchSelectNote (UiActionId id,
                                                              UiActionState state,
                                                              const UiPianoRollActionPayload& payload)
    {
        const engine::EntityId midiClipId = targetMidiClipId (payload);
        const engine::EntityId noteId = targetNoteId (payload);

        if (midiClipId.isValid() && midiClipId != selectedMidiClipId_ && ! selectMidiClip (midiClipId))
            return { id, state, engine::ProjectEditStatus::MidiClipNotFound,
                     engine::ProjectUndoStatus::NothingToUndo, UiPianoRollActionStatus::InvalidSelection };

        if (! selectNote (noteId))
            return { id, state, engine::ProjectEditStatus::NoteNotFound,
                     engine::ProjectUndoStatus::NothingToUndo, UiPianoRollActionStatus::InvalidSelection };

        ++context_.commandDispatchCount;
        return { id, state, engine::ProjectEditStatus::Applied, engine::ProjectUndoStatus::NothingToUndo,
                 UiPianoRollActionStatus::Ok, true };
    }

    [[nodiscard]] UiPianoRollActionResult dispatchExpressionRead (UiActionId id, UiActionState state)
    {
        ++context_.commandDispatchCount;
        ++context_.midiReadCount;
        return { id, state, engine::ProjectEditStatus::Applied, engine::ProjectUndoStatus::NothingToUndo,
                 UiPianoRollActionStatus::Ok, true };
    }

    [[nodiscard]] UiPianoRollActionResult dispatchUndo (UiActionId id, UiActionState state)
    {
        const engine::ProjectUndoStatus undoStatus = undo_.undo (project_);
        syncContext();
        if (undoStatus != engine::ProjectUndoStatus::Applied)
            return { id, state, engine::ProjectEditStatus::InvalidProject, undoStatus };

        ++context_.commandDispatchCount;
        ++context_.undoCount;
        return { id, state, engine::ProjectEditStatus::Applied, undoStatus, UiPianoRollActionStatus::Ok, true };
    }

    [[nodiscard]] UiPianoRollActionResult dispatchRedo (UiActionId id, UiActionState state)
    {
        const engine::ProjectUndoStatus undoStatus = undo_.redo (project_);
        syncContext();
        if (undoStatus != engine::ProjectUndoStatus::Applied)
            return { id, state, engine::ProjectEditStatus::InvalidProject, undoStatus };

        ++context_.commandDispatchCount;
        ++context_.redoCount;
        return { id, state, engine::ProjectEditStatus::Applied, undoStatus, UiPianoRollActionStatus::Ok, true };
    }

    void syncContext() noexcept
    {
        context_.projectLoaded = project_.hasValidAssetClipIndirection();
        context_.canUndo = undo_.canUndo();
        context_.canRedo = undo_.canRedo();
        context_.activePanel = UiPanel::PianoRoll;

        const engine::MidiClip* const midiClip = context_.projectLoaded
            ? detail::findMidiClip (project_, selectedMidiClipId_)
            : nullptr;
        context_.midiClipSelected = midiClip != nullptr;
        context_.midiNoteSelected = midiClip != nullptr
            && selectedNoteId_.isValid()
            && detail::findNote (*midiClip, selectedNoteId_) != nullptr;
    }

    UiActionRegistry registry_;
    UiActionContext context_;
    engine::Project project_;
    engine::ProjectUndoStack undo_;
    engine::EntityId selectedMidiClipId_;
    engine::EntityId selectedNoteId_;
};

} // namespace yesdaw::ui
