#pragma once

// SOURCE ONLY / UNBOUND / UNCOMPILED. Ownership source; conditional query separate.
#include "formation-r8.h"

namespace yesdaw::app::musical_origin_source_r8 {

// Raw storage prevents an implicit second T destructor after the sponsored
// destruction below. Each wrapper pays at its actual destructor entry, BEFORE
// physical T destruction, and records completion AFTER that physical return.
// No future owner destruction is consumed by a stage's Prepare partition.
template<class T, Site Construct, Site Destroy> class FundedOwner final
{
    friend class FundedLegacyStageAdapter;
    friend class WorkerArtifacts;
    MusicalPreparationRequestState* state_;
    alignas (T) std::byte bytes_[sizeof (T)];
    explicit FundedOwner (MusicalPreparationRequestState& s) noexcept : state_ (&s)
    {
        Ledger::pay (s, s.legacyFixed_, Construct);
        ::new (static_cast<void*> (bytes_)) T;
        s.raw_.legacy.ownerInitializations += 1;
    }
    T& value() noexcept { return *std::launder (reinterpret_cast<T*> (bytes_)); }
public:
    FundedOwner (const FundedOwner&) = delete;
    FundedOwner (FundedOwner&&) = delete;
    ~FundedOwner() noexcept
    {
        auto& s = *state_;
        Ledger::pay (s, s.legacyCleanup_, Destroy);
        value().~T();
        s.raw_.legacy.ownerDestructions += 1;
    }
};

using ClockStageOwner = FundedOwner<std::shared_ptr<const engine::MusicalAudioClock>,
    Site::LegacyClockStageConstruct, Site::LegacyClockStageDestroy>;
using WindowStageOwner = FundedOwner<engine::MusicalAudioWindow,
    Site::LegacyWindowStageConstruct, Site::LegacyWindowStageDestroy>;
using RequestStageOwner = FundedOwner<std::shared_ptr<const engine::MusicalAudioChannelRequest>,
    Site::LegacyRequestStageConstruct, Site::LegacyRequestStageDestroy>;
using ClockOutputOwner = FundedOwner<std::shared_ptr<const engine::MusicalAudioClock>,
    Site::LegacyClockOutputConstruct, Site::LegacyClockOutputDestroy>;
using RequestOutputOwner = FundedOwner<std::shared_ptr<const engine::MusicalAudioChannelRequest>,
    Site::LegacyRequestOutputConstruct, Site::LegacyRequestOutputDestroy>;
using PlanOutputOwner = FundedOwner<std::shared_ptr<const engine::MusicalAudioChannelPlan>,
    Site::LegacyPlanOutputConstruct, Site::LegacyPlanOutputDestroy>;

static_assert (std::is_nothrow_default_constructible_v<engine::MusicalAudioWindow>);
static_assert (std::is_nothrow_destructible_v<engine::MusicalAudioWindow>);
static_assert (std::is_nothrow_default_constructible_v<std::shared_ptr<const engine::MusicalAudioClock>>);
static_assert (std::is_nothrow_default_constructible_v<std::shared_ptr<const engine::MusicalAudioChannelRequest>>);
static_assert (std::is_nothrow_default_constructible_v<std::shared_ptr<const engine::MusicalAudioChannelPlan>>);
static_assert (std::is_nothrow_move_assignable_v<std::shared_ptr<const engine::MusicalAudioClock>>);
static_assert (std::is_nothrow_move_assignable_v<std::shared_ptr<const engine::MusicalAudioChannelRequest>>);
static_assert (std::is_nothrow_move_assignable_v<std::shared_ptr<const engine::MusicalAudioChannelPlan>>);
static_assert (std::is_nothrow_move_constructible_v<std::shared_ptr<const engine::MusicalAudioChannelPlan>>);
static_assert (std::is_trivially_destructible_v<LegacyReadout>);
static_assert (std::is_nothrow_copy_assignable_v<LegacyReadout>);

// Lives ONLY on the actual worker's stack, after ContextScope's acquire and
// before that scope's release publication. Request,Window,Clock die in that order.
// The completed Plan is moved directly into the separate result cell.
class WorkerArtifacts final
{
    friend class FundedLegacyStageAdapter;
    MusicalPreparationRequestState* state_;
    ClockStageOwner clock_;
    WindowStageOwner window_;
    RequestStageOwner request_;
    explicit WorkerArtifacts (MusicalPreparationRequestState& s) noexcept
        : state_ (&s), clock_ (s), window_ (s), request_ (s) {}
public:
    WorkerArtifacts (const WorkerArtifacts&) = delete;
    WorkerArtifacts (WorkerArtifacts&&) = delete;
    ~WorkerArtifacts() noexcept
    {
        auto& s = *state_;
        // Group destructor's own body/member-call/return tails only. Each actual
        // member owner separately prepays its physical drop below this body.
        Ledger::pay (s, s.legacyCleanup_, Site::LegacyArtifactsDestroy);
    }
};

inline WorkerLegacyResult::WorkerLegacyResult (Cell, MusicalPreparationRequestState& s) noexcept
{
    Ledger::pay (s, s.legacyFixed_, Site::LegacyCellConstruct);
    if ((s.context_ != Context::Control) | s.resultOriginLive_ | s.resultIssued_) std::terminate();
    state_ = &s; ownsOrigin_ = false; external_ = false; taken_ = false; complete_ = false;
    readout_ = {};
    ::new (static_cast<void*> (planBytes_)) PlanOwner;
    s.raw_.legacy.ownerInitializations += 1;
    s.raw_.legacy.resultCellsConstructed += 1;
}

inline WorkerLegacyResult::WorkerLegacyResult (Take, WorkerLegacyResult& source) noexcept
{
    auto& s = *source.state_;
    Ledger::pay (s, s.legacyFixed_, Site::LegacyCellMove);
    if ((s.context_ != Context::Control) | ! source.complete_ | ! source.ownsOrigin_
        | source.external_ | source.taken_ | ! s.resultOriginLive_) std::terminate();
    // This construction is the ONE finite origin/Plan handoff. The actual
    // returned prvalue is materialized directly in caller-owned storage.
    state_ = &s; ownsOrigin_ = true; external_ = true; taken_ = false; complete_ = true;
    readout_ = source.readout_;
    ::new (static_cast<void*> (planBytes_)) PlanOwner (std::move (source.plan()));
    source.ownsOrigin_ = false; source.taken_ = true;
    s.raw_.legacy.ownerInitializations += 1; s.raw_.legacy.ownerMoves += 1;
    s.raw_.legacy.resultCopies += 1;
    s.raw_.legacy.resultCellsConstructed += 1; s.raw_.legacy.resultOriginTransfers += 1;
}

inline void WorkerLegacyResult::publish (MusicalPreparationRequestState& s, LegacyReadout& readout) noexcept
{
    Ledger::pay (s, s.legacyFixed_, Site::LegacyCellRetainOrigin);
    if ((state_ != &s) | (s.context_ != Context::WorkerRunning) | external_ | taken_
        | complete_ | ownsOrigin_ | s.resultIssued_ | s.resultOriginLive_ | s.retainedIssued_
        | (s.references_ != 2)) std::terminate();
    // Control+Worker -> Control+Worker+Result. No allocation/control block and
    // no new Fund. Its complete role lifetime was held before physical formation.
    s.references_ += 1; s.raw_.referenceRetains += 1;
    s.resultIssued_ = true; s.resultOriginLive_ = true; ownsOrigin_ = true;
    s.raw_.legacy.resultOriginRetains += 1;
    Ledger::pay (s, s.legacyFixed_, Site::LegacyCellPublish);
    complete_ = true;
    s.raw_.legacy.resultPublications += 1; s.raw_.legacy.resultCopies += 1;
    readout.attemptChargeConsumed = s.legacyEnteredContribution_;
    readout.raw = s.raw_.legacy;
    readout_ = readout;
}

inline WorkerLegacyResult::~WorkerLegacyResult() noexcept
{
    auto& s = *state_;
    Ledger::pay (s, s.legacyCleanup_, Site::LegacyCellDestroy);
    if ((s.context_ != Context::Control) & (s.context_ != Context::DetachedControl)) std::terminate();
    // BEFORE the actual direct PlanOwner destruction, including empty owners
    // and moved-from embedded cells. There is no owning automatic member.
    plan().~PlanOwner();
    s.raw_.legacy.ownerDestructions += 1; s.raw_.legacy.resultCellsDestroyed += 1;
    if (ownsOrigin_)
    {
        Ledger::pay (s, s.legacyCleanup_, Site::LegacyCellDestroyOwned);
        if (! s.resultOriginLive_) std::terminate();
        s.resultOriginLive_ = false;
        s.raw_.legacy.resultOriginDrops += 1;
        ownsOrigin_ = false;
        auto* origin = state_; state_ = nullptr;
        Ledger::drop (origin, Role::Result);
        // Result's one already-funded caller/member/return tail only. The last
        // drop may have freed State. No state/counter/ledger access follows.
        return;
    }
    Ledger::pay (s, s.legacyCleanup_, Site::LegacyCellDestroyBorrowed);
    // Only Control-owned embedded cells can be borrowed; Control still owns
    // the request throughout this destructor. Never a detached empty shell.
    if (external_) std::terminate();
    state_ = nullptr;
}

inline WorkerLegacyResult ApplicationRequests::takeCurrentResultAfterTerminal() noexcept
{
    // This actual caller performs the acquire, rather than trusting a Boolean
    // supplied by the caller. Its receive-safe prefix is already held. This is
    // the finite alternative to acquireCurrentWorkerAfterTerminal, not a second
    // acquire after that path. Caller knows the task really reached terminal.
    auto& current = owner (current_);
    current.acquireWorker();
    auto& s = *current.state_;
    Ledger::pay (s, s.legacyFixed_, Site::LegacyTakeResult);
    if ((! current.terminalAcquired_) | (! current.resultLive_)) std::terminate();
    // Sponsor direct LegacyReadout's four nested DMI writes/constructors before
    // Take's body; its later real history copy remains a separate action.
    return WorkerLegacyResult (WorkerLegacyResult::Take {}, current.result());
}

} // namespace yesdaw::app::musical_origin_source_r8
