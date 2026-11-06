#pragma once

// ADR0088/0089 private application source successor. SOURCE ONLY / UNRUN.
// All R7 prices/envelopes/native proof data are UNBOUND. The concrete conditional
// query source grants NO binding, forward-equivalence or execution credit.
// R4 source and its whole first-GREEN proof remain separate immutable artifacts.
#include "engine/ClipSchedule.h"
#include "engine/Project.h"
#include "engine/MusicalAudioChannelPlan.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

namespace yesdaw::app::musical_origin_source_r7 {
using U64 = std::uint64_t;
using SampleOwner = std::shared_ptr<const engine::AssetSamples>;
inline constexpr U64 workCap = U64 { 1 } << 28;
inline constexpr U64 byteCap = U64 { 512 } << 20;
static_assert (std::numeric_limits<std::size_t>::digits <= std::numeric_limits<U64>::digits);
enum class Phase : std::uint8_t { Formation, Capture, Ownership, Context, Cleanup, Settlement, Count };
enum class Role : std::uint8_t { Control, Worker, Retained, Reader, Result, Count };
enum class Context : std::uint8_t
{ Control, WorkerArmed, WorkerRunning, ControlReturn, DetachedControl, ReaderArmed, ReaderRunning, DetachedReturn };
enum class Signal : std::uint8_t { Empty, Armed, Running, Terminal };
enum class CaptureStatus : std::uint8_t
{ RequestCaptured, InvalidIdentity, MissingClip, MissingAsset, InvalidSource, InvalidMap, ResourceLimit, AllocationFailed };
enum class Disposition : std::uint8_t { Initial, PreservedOnFailure, ReplacedPending, KeptRetiring };
enum class Site : std::uint16_t
{
    RootBegin, // SITE
    RootSessionInit, // SITE
    RootNextEntry, // SITE
    RootCounterAlias, // SITE
    RootCounterIssue, // SITE
    RootCounterExhausted, // SITE
    RootIdentity, // SITE
    RootIdentityRefused, // SITE
    RootAllocate, // SITE
    RootAllocateFailed, // SITE
    RootObject, // SITE
    RootInstall, // SITE
    RootNoState, // SITE
    RootFailedAdopt, // SITE
    RootFailedConstructed, // SITE
    RootFailedReadout, // SITE
    RootFailedDiscard, // SITE
    RootFailedSnapshot, // SITE
    RootFailedDestroy, // SITE
    RootFailedSlotErase, // SITE
    RootFailedSessionCheck, // SITE
    RootFailedSessionRetiring, // SITE
    RootFailedSessionFinish, // SITE
    Distribute, // SITE
    CaptureDispatch, // SITE
    CaptureCheck, // SITE
    CaptureSizes, // SITE
    CaptureProducts, // SITE
    CaptureNeed, // SITE
    CaptureTooLarge, // SITE
    CaptureOverflow, // SITE
    CaptureBytesRefused, // SITE
    CaptureReserveRefused, // SITE
    CaptureWorkBegin, // SITE
    ClipLoopEntry, // SITE
    ClipLoopTest, // SITE
    ClipRow, // SITE
    ClipDecision, // SITE
    ClipFound, // SITE
    ClipContinue, // SITE
    ClipEnd, // SITE
    ClipMissingTest, // SITE
    ClipMissing, // SITE
    AssetLoopEntry, // SITE
    AssetLoopTest, // SITE
    AssetRow, // SITE
    AssetDecision, // SITE
    AssetFound, // SITE
    AssetContinue, // SITE
    AssetEnd, // SITE
    AssetMissingTest, // SITE
    AssetMissing, // SITE
    CaptureMetadata, // SITE
    CaptureParents, // SITE
    SourceCheck, // SITE
    SourceMissing, // SITE
    SourceLayout, // SITE
    SourceLayoutRefused, // SITE
    SourceExtent, // SITE
    SourceExtentRefused, // SITE
    SourceProduct, // SITE
    SourceProductRefused, // SITE
    MapNeededTest, // SITE
    MapAllocate, // SITE
    MapAllocateFailed, // SITE
    MapInstall, // SITE
    MapLoopEntry, // SITE
    MapLoopTest, // SITE
    MapRow, // SITE
    MapFirst, // SITE
    MapLater, // SITE
    MapDecision, // SITE
    MapInvalid, // SITE
    MapStore, // SITE
    MapContinue, // SITE
    MapEnd, // SITE
    CaptureFinish, // SITE
    CaptureResultTest, // SITE
    CaptureRefused, // SITE
    ReserveScan, // SITE
    ReserveFail, // SITE
    ReserveInstall, // SITE
    SnapshotFixed, // SITE
    SnapshotRow, // SITE
    OwnerAdopt, // SITE
    OwnerConstructed, // SITE
    ReadoutCommit, // SITE
    NewFailureTest, // SITE
    AbandonNewState, // SITE
    SessionSelect, // SITE
    SessionKeepRunning, // SITE
    SessionReusePending, // SITE
    SessionInstall, // SITE
    SlotErase, // SITE
    DrainRetiring, // SITE
    SessionDestroyCheck, // SITE
    SessionDestroyRetiring, // SITE
    SessionDestroyFinish, // SITE
    ArmWorker, // SITE
    AbortWorker, // SITE
    ReceiveWorker, // SITE
    ReceiveReader, // SITE
    ContextCancel, // SITE
    ContextInspect, // SITE
    ContextDecision, // SITE
    ContextReturn, // SITE
    AcquireWorker, // SITE
    AcquireCurrentCaller, // SITE
    AcquireReader, // SITE
    RetainExternal, // SITE
    ArmReader, // SITE
    AbortReader, // SITE
    ControlDestroy, // SITE
    ControlDetachTest, // SITE
    ControlDetach, // SITE
    ControlDropCaller, // SITE
    RetainedDestroy, // SITE
    RetainedDropCaller, // SITE
    DropProbe, // SITE
    DropNonLast, // SITE
    DropLast, // SITE
    DropMapTest, // SITE
    DropMapRetire, // SITE
    DropParentTest, // SITE
    DropParentRetire, // SITE
    DropRoleRetire, // SITE
    MapDestroy, // SITE
    ParentDestroy, // SITE
    StateDestroy, // SITE
    // R5 additions: original ordinals0..128 are retained. Every new source price
    // and every affected original price/envelope needs an independent census.
    LegacyRootInstall, // SITE
    LegacyDistribute, // SITE
    LegacyCaptureOptions, // SITE
    LegacyFinalProbe, // SITE
    LegacyArtifactsDestroy, // SITE
    LegacySlicesRetire, // SITE
    LegacyEntry, // SITE
    LegacyArtifactsConstruct, // SITE
    LegacyCommonIdentity, // SITE
    LegacyMappedPresent, // SITE
    LegacyMappedLayout, // SITE
    LegacyMappedExtent, // SITE
    LegacyMappedProduct, // SITE
    LegacyByteHeadroom, // SITE
    LegacyClockExtent, // SITE
    LegacyClockBytes, // SITE
    LegacyClockAllocatorLimits, // SITE
    LegacyQueryReserve, // SITE
    LegacyQueryReserveRefused, // SITE
    LegacyBudgetEntry, // SITE
    LegacyBudgetTest, // SITE
    LegacyBudgetRow, // SITE
    LegacyBudgetContinue, // SITE
    LegacyBudgetEnd, // SITE
    LegacyAttemptPrepare, // SITE
    LegacyAttemptLimit, // SITE
    LegacyAttemptReserve, // SITE
    LegacyAttemptReserveRefused, // SITE
    LegacyCancel, // SITE
    LegacyClockPrepare, // SITE
    LegacyClockCommitInvoke, // SITE
    LegacyClockReturned, // SITE
    LegacyClockStore, // SITE
    LegacyClockCumulative, // SITE
    LegacyClockException, // SITE
    LegacyQueryPrepare, // SITE
    LegacyQueryInvoke, // SITE
    LegacyQueryReturned, // SITE
    LegacyRequestPreflight, // SITE
    LegacyRequestBytes, // SITE
    LegacyRequestPrepare, // SITE
    LegacyRequestCommitInvoke, // SITE
    LegacyRequestReturned, // SITE
    LegacyRequestStore, // SITE
    LegacyRequestCumulative, // SITE
    LegacyRequestException, // SITE
    LegacyPlanPreflight, // SITE
    LegacyPlanOwner, // SITE
    LegacyPlanExtent, // SITE
    LegacyPlanProduct, // SITE
    LegacyPlanBytes, // SITE
    LegacyPlanTariff, // SITE
    LegacyPlanPrepare, // SITE
    LegacyPlanCommitInvoke, // SITE
    LegacyPlanReturned, // SITE
    LegacyPlanStore, // SITE
    LegacyPlanCumulative, // SITE
    LegacyPlanFullScan, // SITE
    LegacyPlanException, // SITE
    LegacyFinish, // SITE
    LegacyWorkerCaller, // SITE
    LegacyLocalOwnerReturn, // SITE
    LegacyRefusal, // SITE
    LegacyStop, // SITE
    LegacyClockNumericDomain, // SITE
    LegacyAttemptFeeLimit, // SITE
    LegacyClockEligibility, // SITE
    LegacyAttemptReady, // SITE
    LegacyCancellationTaken, // SITE
    LegacySucceeded, // SITE
    LegacyReady, // SITE
    LegacyRequestWindow, // SITE
    LegacyClockFailure, // SITE
    LegacyByteCompute, // SITE
    LegacyClockByteStore, // SITE
    LegacyAttemptKind, // SITE
    LegacyRequestByteTail, // SITE
    LegacyPlanFee, // SITE
    // R6 additions retain all207 R5 ordinals. No old numeric value is inherited.
    LegacyClockOutputConstruct, // SITE
    LegacyClockOutputDestroy, // SITE
    LegacyRequestOutputConstruct, // SITE
    LegacyRequestOutputDestroy, // SITE
    LegacyPlanOutputConstruct, // SITE
    LegacyPlanOutputDestroy, // SITE
    LegacyClockStageConstruct, // SITE
    LegacyClockStageDestroy, // SITE
    LegacyWindowStageConstruct, // SITE
    LegacyWindowStageDestroy, // SITE
    LegacyRequestStageConstruct, // SITE
    LegacyRequestStageDestroy, // SITE
    LegacyCellConstruct, // SITE
    LegacyCellDestroy, // SITE
    LegacyCellDestroyOwned, // SITE
    LegacyCellDestroyBorrowed, // SITE
    LegacyCellMove, // SITE
    LegacyResultPlanMove, // SITE
    LegacyCellPublish, // SITE
    LegacyCellRetainOrigin, // SITE
    LegacyControlResultProbe, // SITE
    LegacyControlResultDestroy, // SITE
    LegacyControlResultDetach, // SITE
    LegacyArmResult, // SITE
    LegacyAbortResult, // SITE
    LegacyTakeResult, // SITE
    LegacyRetainedResultCheck, // SITE
    QueryQuote, // SITE
    QueryQuoteTerm, // SITE
    QueryQuoteOverflow, // SITE
    QueryQuoteReady, // SITE
    QueryNativeScope, // SITE
    QueryNativeBaseline, // SITE
    QueryNativeConstructComplete, // SITE
    QueryNativeCheckEntry, // SITE
    QueryNativeIdentity, // SITE
    QueryNativeFactTest, // SITE
    QueryNativeFactRow, // SITE
    QueryNativeDigestTest, // SITE
    QueryNativeDigestEntry, // SITE
    QueryNativeDigestByte, // SITE
    QueryNativeDigestEnd, // SITE
    QueryNativeDigestZeroTest, // SITE
    QueryNativeFactContinue, // SITE
    QueryNativeFactEnd, // SITE
    QueryNativeRead, // SITE
    QueryNativeCompare, // SITE
    QueryNativeAccepted, // SITE
    QueryNativeRejected, // SITE
    QueryNativeBeginReplay, // SITE
    QueryNativeReplayReady, // SITE
    QueryNativeRestore, // SITE
    QueryNativeRestoreResult, // SITE
    QueryNativeScopeDestroy, // SITE
    QueryVectorEntry, // SITE
    QueryVectorTest, // SITE
    QueryVectorCell, // SITE
    QueryVectorContinue, // SITE
    QueryVectorEnd, // SITE
    QuerySumEntry, // SITE
    QuerySumTest, // SITE
    QuerySumCell, // SITE
    QuerySumContinue, // SITE
    QuerySumEnd, // SITE
    QueryOldPrepare, // SITE
    QueryOldPrepared, // SITE
    QueryOldAppendInvoke, // SITE
    QueryOldAppendReturned, // SITE
    QueryEntry, // SITE
    QueryScalarNull, // SITE
    QueryScalarNegative, // SITE
    QueryScalarLength, // SITE
    QueryScalarN, // SITE
    QueryScalarOverflow, // SITE
    QueryPredictEntry, // SITE
    QuerySearchEntry, // SITE
    QuerySearchTest, // SITE
    QuerySearchRow, // SITE
    QuerySearchTrue, // SITE
    QuerySearchFalse, // SITE
    QuerySearchEnd, // SITE
    QueryPrefixEntry, // SITE
    QueryPrefixTest, // SITE
    QueryPrefixRow, // SITE
    QueryPrefixContinue, // SITE
    QueryPrefixEnd, // SITE
    QueryAppendClassify, // SITE
    QueryAppendJump, // SITE
    QueryAppendEqual, // SITE
    QueryAppendFullRamp, // SITE
    QueryAppendSlope, // SITE
    QueryAppendSlopeZero, // SITE
    QueryAppendSlopeFinite, // SITE
    QueryAppendSlopeBad, // SITE
    QueryAppendRelative, // SITE
    QueryAppendRelativeBad, // SITE
    QueryAppendRelativeRange, // SITE
    QueryAppendRangeBad, // SITE
    QueryAppendRamp, // SITE
    QueryEndpointEntry, // SITE
    QueryEndpointEmpty, // SITE
    QueryEndpointMap, // SITE
    QueryEndpointPartial, // SITE
    QueryEndpointAtRow, // SITE
    QueryEndpointFtFailed, // SITE
    QueryEndpointNative, // SITE
    QueryEndpointNonfinite, // SITE
    QueryEndpointNegative, // SITE
    QueryEndpointOverflow, // SITE
    QueryEndpointOk, // SITE
    QueryPredictionAFailed, // SITE
    QueryPredictionB, // SITE
    QueryPredictionBFailed, // SITE
    QueryPredictionCollapsed, // SITE
    QueryPredictionOk, // SITE
    QueryResolvePrepare, // SITE
    QueryResolveCommitInvoke, // SITE
    QueryResolveReturned, // SITE
    QueryReconcileEntry, // SITE
    QueryReconcileFailure, // SITE
    QueryReconcileOk, // SITE
    QueryNoInvocation, // SITE
    QueryWindowPreScope, // SITE
    QueryWindowAfterScope, // SITE
    QueryPacket, // SITE
    QueryAppendEqualTest, // SITE
    QueryAppendFullTest, // SITE
    QueryOldPrefixReturned, // SITE
    QueryOldPartialReturned, // SITE
    QueryPrefixFailed, // SITE
    QueryEndpointResource, // SITE
    QueryEndpointAfterFt, // SITE
    QueryPredictionScalarChecks, // SITE
    QueryPredictionLengthCheck, // SITE
    QueryPredictionNCheck, // SITE
    QueryPredictionOverflowCheck, // SITE
    QueryPredictionEndpoints, // SITE
    QueryReconcileStatus, // SITE
    QueryReconcileSuccess, // SITE
    QueryReconcileEmpty, // SITE
    QueryQuoteRefused, // SITE
    QueryCallNotEntered, // SITE
    QueryCallEntered, // SITE
    QueryCallOuterFailure, // SITE
    QueryEndpointNegativeTest, // SITE
    QueryEndpointRangeTest, // SITE
    QueryPartialNextArgs, // SITE
    QueryPartialLastArgs, // SITE
    QueryReconcileMismatch, // SITE
    QueryNativeRestoreVerify, // SITE
    QueryNativeRestoreReady, // SITE
    QueryRecordEntry, // SITE
    QueryRecordTest, // SITE
    QueryRecordCell, // SITE
    QueryRecordContinue, // SITE
    QueryRecordEnd, // SITE
    Count
};
inline constexpr auto phaseCount = static_cast<std::size_t> (Phase::Count);
inline constexpr auto roleCount = static_cast<std::size_t> (Role::Count);
inline constexpr auto siteCount = static_cast<std::size_t> (Site::Count);

struct Bounds final
{
    U64 sites[siteCount]; // fixed literal event prices only; no unused perItem model.
    U64 rootEnvelope, captureCheck, stateCleanup, mapCleanup, parentCleanup;
    U64 roleLifetime[roleCount];
    U64 variableCaptureFixed, perSearchRow, perMapRow;
    // R7 complete fixed schedules are part of rootEnvelope BEFORE any physical
    // request/argument/State initialization. None is backed by R4 Worker874.
    U64 legacyFixed, legacyCheck, legacyCleanup;
    // NEW source-derived R-dependent ceilings only, NEVER actual debits. Values
    // remain absent until full census/forward proof. Old callee ceilings are in
    // the separate unbound immutable OldQueryCatalogue.
    U64 pendingWindowQueryFixed, pendingWindowQueryPerPrefix, pendingWindowQueryPerSearch;
};
class Ledger;
class Formation;
class ProofAccess;
enum class LegacyStage : std::uint8_t { Clock, Request, Plan, None };
enum class LegacyOutcome : std::uint8_t
{
    NotStarted, Ok, Cancelled, InvalidCapturedRequest, InvalidMappedOwner, UnsupportedPolicy,
    ResourceLimit, LegacyReturnedFailure, UnexpectedLegacyException,
    NativeQualificationFailed, NativeRestoreFailed, QueryReconciliationFailed
};
enum class LegacyEvidence : std::uint8_t { Unavailable, SourceProved, ReturnedOwner, SourceDerived };
struct LegacyRaw final
{
    U64 invocations[3], attemptChargeConsumed[3], returned[3], exceptions[3];
    U64 cancellationLoads, budgetRows, queryCalls, resultCopies;
    U64 ownerInitializations, ownerMoves, ownerDestructions;
    U64 cumulativeReads, cumulativeComparisons, knownFullScanOutcomes;
    // Direct new source events, not legacy private body/allocation observations.
    U64 resultCellsConstructed, resultCellsDestroyed, resultOriginRetains;
    U64 resultOriginTransfers, resultOriginDrops, resultPublications;
    U64 queryDerivedUnits, extraAppendCalls, extraAppendDerivedUnits;
    U64 queryDerivedVector[10], extraAppendDerivedVector[10];
    U64 prefixReplayCalls, partialReplayCalls, derivedSearchTrue, derivedSearchFalse;
    U64 nativeSnapshots, nativeStatusRestorations;
    LegacyEvidence queryPathEvidence, extraAppendPathEvidence, queryInternalVisitsEvidence;
};
struct LegacyObservation final
{
    LegacyEvidence cumulativeEvidence, framesVisitedEvidence, samplesVisitedEvidence;
    U64 cumulative, frames, samples;
};
struct LegacyReadout final
{
    LegacyOutcome outcome;
    LegacyStage stage;
    bool clockStatusKnown, windowStatusKnown, requestStatusKnown, planStatusKnown;
    engine::MusicalAudioStatus clockStatus, windowStatus;
    engine::MusicalAudioChannelResult requestStatus, planStatus;
    LegacyObservation observations[3];
    LegacyRaw raw;
    U64 attemptChargeConsumed;
};
static_assert (std::is_trivially_copyable_v<LegacyReadout>);
class Catalogue final
{
    friend class ProofAccess;
    friend class Ledger;
    friend class Formation;
    // Future independent definition: constinit const Bounds Catalogue::reviewedData_.
    // No values, factory, dynamic initialization, guard, setter or caller tariff.
    static const Bounds reviewedData_;
    static const Bounds& reviewed() noexcept { return reviewedData_; }
};
struct Raw final
{
    U64 sites[siteCount];
    // Normalized bounded-loop diagnostics, not compiler/runtime measurements.
    // Bodies: actual selected row bodies, or actual fixed-helper loop bodies.
    // The for(;;) carrier's extra final test is a condition, not a selected row.
    // FinalFalse is the actual false i!=count predicate. Breaks counts EVERY
    // explicit break, including normal exhaustion; an exit can increment both.
    // Increments are actual logical index increments; returns are row-body exits.
    // SiteLoopTest/End still price every actual carrier/test/exit source action.
    U64 loopEntries, loopConditions, loopBodies, loopIncrements, loopFinalFalse, loopBreaks, loopReturns;
    U64 allocationAttempts, allocationSuccesses, allocationReleases;
    U64 requestedBytes, currentBytes, peakBytes, copiedMapRows, searchRows;
    U64 ownerConstructions, ownerDestructions, referenceRetains, referenceDrops, terminalStores, terminalLoads;
    LegacyRaw legacy;
};
struct Slice final { U64 remaining; Phase phase; };
struct Canonical final { U64 actual[phaseCount], reserved[phaseCount], terminal[phaseCount]; };
struct Snapshot final { U64 cap, actual, reserved, total, actualByPhase[phaseCount], reservedByPhase[phaseCount]; };
struct FormationReceipt final { CaptureStatus status; Snapshot origin; Raw raw; bool physicalState; };
struct RootCensus final { Raw raw; U64 actual, unused; bool handedOff; };
class Readout final
{
    friend class ProofAccess;
    const FormationReceipt receipt_;
    const Disposition disposition_;
    friend class ApplicationRequests;
    Readout (const FormationReceipt& receipt, Disposition disposition) noexcept
        : receipt_ (receipt), disposition_ (disposition) {}
public:
    Readout (const Readout&) = delete;
    Readout (Readout&&) = delete;
    // Trivial immutable historical data. These reads grant no current authority.
    CaptureStatus status() const noexcept { return receipt_.status; }
    Disposition disposition() const noexcept { return disposition_; }
};
static_assert (std::is_trivially_destructible_v<Readout>);
class NativeStorage final
{
    friend class Formation;
    friend class Ledger;
    static void* allocate (std::size_t n) noexcept { return ::operator new (n, std::nothrow); }
    static void release (void* p) noexcept { ::operator delete (p); }
};
struct Parents final
{
    // O records handle operations only. A nonnull get is not proof that its
    // control owns/protects that get; ingress/lifetime/ABI/resources stay HOLD.
    const SampleOwner original, readyView;
    Parents (const SampleOwner& source, const SampleOwner& view) noexcept : original (source), readyView (view) {}
};
struct CapturedMetadata final
{
    engine::EntityId projectId;
    engine::Clip clip;
    engine::Asset asset;
    engine::SampleRate projectRate;
    U64 requestId, requestVersion, attachGeneration;
};
// Caller intent values are copied only under the genuine new request's fixed
// schedule. No caller price/Fund/Plan/clock/result or ownership callback exists.
struct LegacyPreparationOptions final
{
    engine::ViewWindow mappedWindow;
    engine::ResampleQuality tier;
    engine::MusicalAudioPreparationIdentity identity;
    engine::MusicalAudioChannelPolicy policy;
    engine::MusicalAudioLimits limits;
};
static_assert (std::is_nothrow_copy_constructible_v<LegacyPreparationOptions>);
static_assert (std::is_trivially_destructible_v<LegacyPreparationOptions>);
// Staging and completed results are deliberately NOT members of request State.
// Their concrete private classes are defined below / in worker-result.h.
static_assert (sizeof (engine::MusicalAudioClock) <= byteCap - 256);
static_assert (sizeof (engine::TempoChange) <= byteCap / 3);
static_assert (sizeof (engine::MusicalAudioKey) <= byteCap - 64);
static_assert (sizeof (engine::MusicalAudioChannelRequest) <= byteCap - 64);
static_assert (sizeof (engine::MusicalAudioChannelPlan) <= byteCap - 64);
static_assert (static_cast<U64> (std::numeric_limits<std::ptrdiff_t>::max()) >= byteCap);
static_assert (std::is_nothrow_copy_constructible_v<engine::TempoMapView>);
static_assert (std::is_nothrow_copy_constructible_v<engine::SampleRate>);
static_assert (std::is_nothrow_copy_constructible_v<engine::ViewWindow>);
static_assert (std::is_nothrow_copy_constructible_v<engine::MusicalAudioLimits>);
static_assert (std::is_nothrow_copy_constructible_v<engine::MusicalAudioPreparationIdentity>);
static_assert (std::is_nothrow_copy_constructible_v<engine::MusicalAudioChannelPolicy>);
class ApplicationRequests;
class ControlRequestOwner;
class RetainedOriginOwner;
class FundedLegacyStageAdapter;
class FundedWindowQuery;
class FundedNativeSession;
class QueryAccounting;
class WorkerLegacyResult;
class WorkerArtifacts;
template<class, Site, Site> class FundedOwner;
template<bool Reader> class ContextScope;
class CapturedIntent final
{
    friend class Formation;
    const engine::Project& project_;
    const engine::EntityId clipId_;
    const SampleOwner& source_;
    const SampleOwner& readyView_;
    const LegacyPreparationOptions& legacyOptions_;
    U64 requestId_, version_, generation_;
    bool consumed_;
    CapturedIntent (const engine::Project& project, engine::EntityId clip,
                    const SampleOwner& source, const SampleOwner& view,
                    U64 id, U64 version, U64 generation, const LegacyPreparationOptions& legacyOptions) noexcept
        : project_ (project), clipId_ (clip), source_ (source), readyView_ (view),
          legacyOptions_ (legacyOptions), requestId_ (id), version_ (version), generation_ (generation), consumed_ (false) {}
public:
    CapturedIntent (const CapturedIntent&) = delete;
    CapturedIntent (CapturedIntent&&) = delete;
};
class MusicalPreparationRequestState final
{
    friend class ProofAccess;
    friend class Ledger;
    friend class Formation;
    friend class ApplicationRequests;
    friend class ControlRequestOwner;
    friend class RetainedOriginOwner;
    friend class FundedLegacyStageAdapter;
    friend class FundedWindowQuery;
    friend class FundedNativeSession;
    friend class QueryAccounting;
    friend class WorkerLegacyResult;
    friend class WorkerArtifacts;
    template<class, Site, Site> friend class FundedOwner;
    template<bool> friend class ContextScope;
    Canonical ledger_;
    Raw raw_;
    Slice root_, captureCheck_, stateCleanup_, mapCleanup_, parentCleanup_;
    Slice roleLife_[roleCount];
    Slice legacyFixed_, legacyCheck_, legacyCleanup_, legacyQuery_, queryCallee_, legacyAttempt_[3];
    std::uint32_t references_;
    Context context_;
    bool workerIssued_, retainedIssued_, readerIssued_, parentLive_, metadataLive_;
    bool legacyOptionsLive_, legacyIssued_, resultIssued_, resultOriginLive_;
    CaptureStatus captureStatus_;
    engine::TempoChange* map_;
    U64 mapCapacity_, mapConstructed_, legacyEnteredContribution_;
    alignas (Parents) std::byte parentBytes_[sizeof (Parents)];
    alignas (CapturedMetadata) std::byte metadataBytes_[sizeof (CapturedMetadata)];
    alignas (LegacyPreparationOptions) std::byte legacyOptionsBytes_[sizeof (LegacyPreparationOptions)];
    struct Uninitialized final {};
    explicit MusicalPreparationRequestState (Uninitialized) noexcept {}
    ~MusicalPreparationRequestState() = default;
    Parents& parents() noexcept { return *std::launder (reinterpret_cast<Parents*> (parentBytes_)); }
    const CapturedMetadata& metadata() const noexcept
    { return *std::launder (reinterpret_cast<const CapturedMetadata*> (metadataBytes_)); }
    const LegacyPreparationOptions& legacyOptions() const noexcept
    { return *std::launder (reinterpret_cast<const LegacyPreparationOptions*> (legacyOptionsBytes_)); }
};
static_assert (alignof (MusicalPreparationRequestState) <= alignof (std::max_align_t));
static_assert (sizeof (MusicalPreparationRequestState) <= byteCap);
static_assert (alignof (engine::TempoChange) <= alignof (std::max_align_t));
static_assert (std::is_trivially_copyable_v<engine::TempoChange>);
static_assert (std::is_trivially_destructible_v<CapturedMetadata>);
static_assert (std::is_nothrow_copy_constructible_v<engine::Clip>);

class Ledger final
{
    friend class Formation;
    friend class ApplicationRequests;
    friend class ControlRequestOwner;
    friend class RetainedOriginOwner;
    friend class FundedLegacyStageAdapter;
    friend class FundedWindowQuery;
    friend class FundedNativeSession;
    friend class QueryAccounting;
    friend class WorkerLegacyResult;
    friend class WorkerArtifacts;
    template<class, Site, Site> friend class FundedOwner;
    template<bool> friend class ContextScope;
    static constexpr std::size_t ix (Phase p) noexcept { return static_cast<std::size_t> (p); }
    static constexpr std::size_t ix (Role p) noexcept { return static_cast<std::size_t> (p); }
    static constexpr std::size_t ix (Site p) noexcept { return static_cast<std::size_t> (p); }
    static const Bounds& bounds() noexcept { return Catalogue::reviewed(); }
    static U64 add (U64 a, U64 b) noexcept
    {
        if (b > std::numeric_limits<U64>::max() - a) std::terminate();
        return a + b;
    }
    static U64 multiply (U64 a, U64 b) noexcept
    {
        // Same executed operations for a==0/nonzero; divisor is never zero.
        const auto divisor = a | static_cast<U64> (a == 0);
        if (b > std::numeric_limits<U64>::max() / divisor) std::terminate();
        return a * b;
    }
    static U64 cost (Site site) noexcept { return bounds().sites[ix (site)]; }
    static Slice empty (Phase phase) noexcept { return { 0, phase }; }
    static void pay (MusicalPreparationRequestState& s, Slice& fund, Site site) noexcept
    {
        // Its complete finite own/helper/caller expansion is funded nonrecursively
        // by this held typed slice. No price here has yet been independently bound.
        const auto amount = cost (site);
        const auto p = ix (fund.phase);
        if ((amount > fund.remaining) | (amount > s.ledger_.reserved[p])
            | (amount > std::numeric_limits<U64>::max() - s.ledger_.actual[p])) std::terminate();
        fund.remaining -= amount;
        s.ledger_.reserved[p] -= amount;
        s.ledger_.actual[p] += amount;
        s.raw_.sites[ix (site)] += 1;
    }
    static void split (MusicalPreparationRequestState& s, Slice& from, Slice& into,
                       Phase phase, U64 amount) noexcept
    {
        if ((amount > from.remaining) | (amount > s.ledger_.reserved[ix (from.phase)])) std::terminate();
        from.remaining -= amount;
        s.ledger_.reserved[ix (from.phase)] -= amount;
        s.ledger_.reserved[ix (phase)] = add (s.ledger_.reserved[ix (phase)], amount);
        into = { amount, phase };
    }
    static void retire (MusicalPreparationRequestState& s, Slice& fund) noexcept
    {
        s.ledger_.terminal[ix (fund.phase)] = add (s.ledger_.terminal[ix (fund.phase)], fund.remaining);
        fund.remaining = 0; // Never lower canonical reserved or grant credit.
    }
    static bool reserve (MusicalPreparationRequestState& s, Slice& check, Slice& out,
                         Phase phase, U64 amount) noexcept
    {
        pay (s, check, Site::ReserveScan);
        U64 total = 0;
        s.raw_.loopEntries += 1;
        // Fixed six-row loop: every iteration and final false always executes.
        for (std::size_t i = 0; i != phaseCount; ++i)
        {
            total = add (total, add (s.ledger_.actual[i], s.ledger_.reserved[i]));
            s.raw_.loopConditions += 1; s.raw_.loopBodies += 1; s.raw_.loopIncrements += 1;
        }
        s.raw_.loopConditions += 1; s.raw_.loopFinalFalse += 1;
        if (total > workCap) std::terminate();
        if (amount > workCap - total)
        {
            pay (s, check, Site::ReserveFail);
            return false;
        }
        pay (s, check, Site::ReserveInstall);
        s.ledger_.reserved[ix (phase)] = add (s.ledger_.reserved[ix (phase)], amount);
        out = { amount, phase };
        return true;
    }
    static void snapshot (MusicalPreparationRequestState& s, FormationReceipt& out) noexcept
    {
        pay (s, s.root_, Site::SnapshotFixed);
        s.raw_.loopEntries += 1;
        for (std::size_t i = 0; i != phaseCount; ++i)
        {
            pay (s, s.root_, Site::SnapshotRow);
            s.raw_.loopConditions += 1; s.raw_.loopBodies += 1; s.raw_.loopIncrements += 1;
        }
        s.raw_.loopConditions += 1; s.raw_.loopFinalFalse += 1;
        out.origin.cap = workCap; out.origin.actual = 0; out.origin.reserved = 0;
        s.raw_.loopEntries += 1;
        // All snapshot debits precede EVERY read of these canonical phase cells.
        for (std::size_t i = 0; i != phaseCount; ++i)
        {
            out.origin.actualByPhase[i] = s.ledger_.actual[i];
            out.origin.reservedByPhase[i] = s.ledger_.reserved[i];
            out.origin.actual = add (out.origin.actual, out.origin.actualByPhase[i]);
            out.origin.reserved = add (out.origin.reserved, out.origin.reservedByPhase[i]);
            s.raw_.loopConditions += 1; s.raw_.loopBodies += 1; s.raw_.loopIncrements += 1;
        }
        s.raw_.loopConditions += 1; s.raw_.loopFinalFalse += 1;
        out.origin.total = add (out.origin.actual, out.origin.reserved);
        out.status = s.captureStatus_; out.physicalState = true; out.raw = s.raw_;
    }
    static void drop (MusicalPreparationRequestState*& owner, Role role) noexcept
    {
        auto& s = *owner;
        auto& life = s.roleLife_[ix (role)];
        pay (s, life, Site::DropProbe);
        if (s.references_ == 0) std::terminate();
        if (s.references_ != 1)
        {
            pay (s, life, Site::DropNonLast);
            s.references_ -= 1; s.raw_.referenceDrops += 1;
            retire (s, life); owner = nullptr;
            return;
        }
        pay (s, life, Site::DropLast);
        s.references_ = 0; s.raw_.referenceDrops += 1;
        // R7 extra cleanup has its OWN pre-physical fixed slice. No debit here
        // comes from an original role or from a semantic builder attempt.
        pay (s, s.legacyCleanup_, Site::LegacyFinalProbe);
        if (s.resultOriginLive_) std::terminate();
        pay (s, s.legacyCleanup_, Site::LegacySlicesRetire);
        retire (s, s.legacyQuery_);
        retire (s, s.queryCallee_);
        s.raw_.loopEntries += 1;
        for (std::size_t i = 0; i != 3; ++i)
        {
            retire (s, s.legacyAttempt_[i]);
            s.raw_.loopConditions += 1; s.raw_.loopBodies += 1; s.raw_.loopIncrements += 1;
        }
        s.raw_.loopConditions += 1; s.raw_.loopFinalFalse += 1;
        retire (s, s.legacyFixed_); retire (s, s.legacyCheck_); retire (s, s.legacyCleanup_);
        pay (s, life, Site::DropMapTest);
        if (s.map_ != nullptr)
        {
            pay (s, s.mapCleanup_, Site::MapDestroy);
            NativeStorage::release (s.map_);
            s.raw_.allocationReleases += 1;
            s.raw_.currentBytes -= s.mapCapacity_ * sizeof (engine::TempoChange);
            s.map_ = nullptr; s.mapConstructed_ = 0; s.mapCapacity_ = 0;
        }
        pay (s, life, Site::DropMapRetire);
        retire (s, s.mapCleanup_);
        pay (s, life, Site::DropParentTest);
        if (s.parentLive_)
        {
            pay (s, s.parentCleanup_, Site::ParentDestroy);
            s.parents().~Parents(); // readyView then original; two real handle drops.
            s.parentLive_ = false;
        }
        pay (s, life, Site::DropParentRetire);
        retire (s, s.parentCleanup_);
        pay (s, life, Site::DropRoleRetire);
        retire (s, life);
        pay (s, s.stateCleanup_, Site::StateDestroy);
        // No owner remains. Classify every unused fixed schedule before its
        // storage dies; do not lower canonical reserved or manufacture credit.
        retire (s, s.root_); retire (s, s.captureCheck_);
        s.raw_.loopEntries += 1;
        for (std::size_t i = 0; i != roleCount; ++i)
        {
            retire (s, s.roleLife_[i]);
            s.raw_.loopConditions += 1; s.raw_.loopBodies += 1; s.raw_.loopIncrements += 1;
        }
        s.raw_.loopConditions += 1; s.raw_.loopFinalFalse += 1;
        retire (s, s.stateCleanup_);
        s.raw_.allocationReleases += 1;
        s.raw_.currentBytes -= sizeof (MusicalPreparationRequestState);
        auto* allocation = owner; owner = nullptr;
        allocation->~MusicalPreparationRequestState();
        NativeStorage::release (allocation);
        // No state/provider-context read after native free; only prepaid returns.
    }
};

struct Formed final
{
    MusicalPreparationRequestState* state;
    CaptureStatus status;
    FormationReceipt receipt;
    RootCensus failureTail;
};
static_assert (std::is_trivially_destructible_v<Formed>);
static_assert (std::is_trivially_destructible_v<RootCensus>);
class Formation final
{
    friend class ApplicationRequests;
    friend class ControlRequestOwner;
    static void rootEvent (RootCensus& trace, Site site) noexcept
    {
        const auto amount = Ledger::cost (site);
        if (trace.handedOff | (amount > trace.unused)) std::terminate();
        trace.actual = Ledger::add (trace.actual, amount);
        trace.unused -= amount;
        trace.raw.sites[Ledger::ix (site)] += 1;
    }
    static void beginRoot (RootCensus& trace) noexcept
    {
        // One complete ADR0088 logical root reservation exists BEFORE the first
        // actual argument/local/byte/catalogue write; no alleged existing Fund.
        auto* bytes = reinterpret_cast<unsigned char*> (&trace.raw);
        for (std::size_t i = 0; i != sizeof (Raw); ++i) bytes[i] = 0;
        trace.actual = 0; trace.unused = Catalogue::reviewed().rootEnvelope; trace.handedOff = false;
        trace.raw.loopEntries = 1; trace.raw.loopConditions = sizeof (Raw) + 1;
        trace.raw.loopBodies = sizeof (Raw); trace.raw.loopIncrements = sizeof (Raw); trace.raw.loopFinalFalse = 1;
        rootEvent (trace, Site::RootBegin);
    }
    static bool validId (const engine::EntityId& id, Raw& raw) noexcept
    {
        std::uint8_t bits = 0;
        raw.loopEntries += 1;
        for (std::size_t i = 0; i != engine::EntityId::kNumBytes; ++i)
        {
            bits |= id.bytes[i];
            raw.loopConditions += 1; raw.loopBodies += 1; raw.loopIncrements += 1;
        }
        raw.loopConditions += 1; raw.loopFinalFalse += 1;
        return bits != 0;
    }
    static bool equalId (const engine::EntityId& a, const engine::EntityId& b, Raw& raw) noexcept
    {
        bool equal = true;
        raw.loopEntries += 1;
        for (std::size_t i = 0; i != engine::EntityId::kNumBytes; ++i)
        {
            equal = equal & (a.bytes[i] == b.bytes[i]);
            raw.loopConditions += 1; raw.loopBodies += 1; raw.loopIncrements += 1;
        }
        raw.loopConditions += 1; raw.loopFinalFalse += 1;
        return equal;
    }
    static void install (MusicalPreparationRequestState& s, RootCensus& trace) noexcept
    {
        rootEvent (trace, Site::RootInstall);
        s.raw_ = trace.raw;
        for (std::size_t i = 0; i != phaseCount; ++i)
        { s.ledger_.actual[i] = 0; s.ledger_.reserved[i] = 0; s.ledger_.terminal[i] = 0; }
        s.ledger_.actual[Ledger::ix (Phase::Formation)] = trace.actual;
        s.ledger_.reserved[Ledger::ix (Phase::Formation)] = trace.unused;
        s.root_ = { trace.unused, Phase::Formation };
        s.captureCheck_ = Ledger::empty (Phase::Capture);
        s.stateCleanup_ = Ledger::empty (Phase::Cleanup);
        s.mapCleanup_ = Ledger::empty (Phase::Cleanup);
        s.parentCleanup_ = Ledger::empty (Phase::Cleanup);
        for (std::size_t i = 0; i != roleCount; ++i) s.roleLife_[i] = Ledger::empty (Phase::Ownership);
        s.references_ = 1; s.context_ = Context::Control;
        s.workerIssued_ = false; s.retainedIssued_ = false; s.readerIssued_ = false;
        s.parentLive_ = false; s.metadataLive_ = false; s.captureStatus_ = CaptureStatus::InvalidIdentity;
        s.map_ = nullptr; s.mapCapacity_ = 0; s.mapConstructed_ = 0; s.legacyEnteredContribution_ = 0;
        // R7 additions are backed by rootEnvelope's complete source expansion.
        // LegacyRootInstall includes these actual writes/loop and its own debit.
        Ledger::pay (s, s.root_, Site::LegacyRootInstall);
        s.legacyFixed_ = Ledger::empty (Phase::Context);
        s.legacyCheck_ = Ledger::empty (Phase::Context);
        s.legacyCleanup_ = Ledger::empty (Phase::Cleanup);
        s.legacyQuery_ = Ledger::empty (Phase::Context);
        s.queryCallee_ = Ledger::empty (Phase::Context);
        s.raw_.loopEntries += 1;
        for (std::size_t i = 0; i != 3; ++i)
        {
            s.legacyAttempt_[i] = Ledger::empty (Phase::Context);
            s.raw_.loopConditions += 1; s.raw_.loopBodies += 1; s.raw_.loopIncrements += 1;
        }
        s.raw_.loopConditions += 1; s.raw_.loopFinalFalse += 1;
        s.legacyOptionsLive_ = false; s.legacyIssued_ = false;
        s.resultIssued_ = false; s.resultOriginLive_ = false;
        s.raw_.loopEntries += 2; s.raw_.loopConditions += phaseCount + roleCount + 2;
        s.raw_.loopBodies += phaseCount + roleCount; s.raw_.loopIncrements += phaseCount + roleCount;
        s.raw_.loopFinalFalse += 2;
        trace.handedOff = true; // ONE handoff. Scratch is permanently dead authority.
        Ledger::pay (s, s.root_, Site::Distribute);
        const auto& b = Catalogue::reviewed();
        Ledger::split (s, s.root_, s.stateCleanup_, Phase::Cleanup, b.stateCleanup);
        Ledger::split (s, s.root_, s.captureCheck_, Phase::Capture, b.captureCheck);
        for (std::size_t i = 0; i != roleCount; ++i)
            Ledger::split (s, s.root_, s.roleLife_[i], Phase::Ownership, b.roleLifetime[i]);
        s.raw_.loopEntries += 1; s.raw_.loopConditions += roleCount + 1;
        s.raw_.loopBodies += roleCount; s.raw_.loopIncrements += roleCount; s.raw_.loopFinalFalse += 1;
        Ledger::pay (s, s.root_, Site::LegacyDistribute);
        Ledger::split (s, s.root_, s.legacyFixed_, Phase::Context, b.legacyFixed);
        Ledger::split (s, s.root_, s.legacyCheck_, Phase::Context, b.legacyCheck);
        Ledger::split (s, s.root_, s.legacyCleanup_, Phase::Cleanup, b.legacyCleanup);
    }
    static CaptureStatus captureVariable (MusicalPreparationRequestState& s, CapturedIntent& in) noexcept
    {
        Ledger::pay (s, s.captureCheck_, Site::CaptureCheck);
        const auto& b = Catalogue::reviewed();
        const auto clipCount = static_cast<U64> (in.project_.clips.size());
        const auto assetCount = static_cast<U64> (in.project_.assets.size());
        const auto mapCount = static_cast<U64> (in.project_.tempoMap.size());
        if ((clipCount > workCap) | (assetCount > workCap)
            | (mapCount > byteCap / sizeof (engine::TempoChange)))
        {
            Ledger::pay (s, s.captureCheck_, Site::CaptureTooLarge);
            return CaptureStatus::ResourceLimit;
        }
        Ledger::pay (s, s.captureCheck_, Site::CaptureSizes);
        if (clipCount > workCap - assetCount)
        {
            Ledger::pay (s, s.captureCheck_, Site::CaptureOverflow);
            return CaptureStatus::ResourceLimit;
        }
        Ledger::pay (s, s.captureCheck_, Site::CaptureProducts);
        const auto searchCount = clipCount + assetCount;
        const auto mapBytes = mapCount * sizeof (engine::TempoChange);
        if (mapBytes > byteCap - sizeof (MusicalPreparationRequestState))
        {
            Ledger::pay (s, s.captureCheck_, Site::CaptureBytesRefused);
            return CaptureStatus::ResourceLimit;
        }
        Ledger::pay (s, s.captureCheck_, Site::CaptureNeed);
        const auto needed = Ledger::add (b.variableCaptureFixed,
            Ledger::add (Ledger::multiply (searchCount, b.perSearchRow),
                Ledger::add (Ledger::multiply (mapCount, b.perMapRow), Ledger::add (b.mapCleanup, b.parentCleanup))));
        Slice work = Ledger::empty (Phase::Capture);
        if (! Ledger::reserve (s, s.captureCheck_, work, Phase::Capture, needed))
        {
            Ledger::pay (s, s.captureCheck_, Site::CaptureReserveRefused);
            return CaptureStatus::ResourceLimit;
        }
        Ledger::pay (s, work, Site::CaptureWorkBegin);
        Ledger::split (s, work, s.mapCleanup_, Phase::Cleanup, b.mapCleanup);
        Ledger::split (s, work, s.parentCleanup_, Phase::Cleanup, b.parentCleanup);
        Ledger::pay (s, work, Site::ClipLoopEntry);
        const engine::Clip* clip = nullptr;
        U64 ci = 0; s.raw_.loopEntries += 1;
        for (;;)
        {
            Ledger::pay (s, work, Site::ClipLoopTest);
            s.raw_.loopConditions += 1;
            if (ci != clipCount)
            {
                Ledger::pay (s, work, Site::ClipRow);
                s.raw_.loopBodies += 1; s.raw_.searchRows += 1;
                const bool found = equalId (in.project_.clips[static_cast<std::size_t> (ci)].id, in.clipId_, s.raw_);
                Ledger::pay (s, work, Site::ClipDecision);
                if (found)
                {
                    Ledger::pay (s, work, Site::ClipFound);
                    clip = &in.project_.clips[static_cast<std::size_t> (ci)]; s.raw_.loopBreaks += 1;
                    break;
                }
                Ledger::pay (s, work, Site::ClipContinue);
                s.raw_.loopIncrements += 1; ++ci;
            }
            else
            {
                Ledger::pay (s, work, Site::ClipEnd);
                s.raw_.loopFinalFalse += 1; s.raw_.loopBreaks += 1;
                break;
            }
        }
        Ledger::pay (s, work, Site::ClipMissingTest);
        if (clip == nullptr)
        {
            Ledger::pay (s, work, Site::ClipMissing);
            Ledger::retire (s, work);
            return CaptureStatus::MissingClip;
        }
        Ledger::pay (s, work, Site::AssetLoopEntry);
        const engine::Asset* asset = nullptr;
        U64 ai = 0; s.raw_.loopEntries += 1;
        for (;;)
        {
            Ledger::pay (s, work, Site::AssetLoopTest);
            s.raw_.loopConditions += 1;
            if (ai != assetCount)
            {
                Ledger::pay (s, work, Site::AssetRow);
                s.raw_.loopBodies += 1; s.raw_.searchRows += 1;
                const bool found = equalId (in.project_.assets[static_cast<std::size_t> (ai)].id, clip->assetId, s.raw_);
                Ledger::pay (s, work, Site::AssetDecision);
                if (found)
                {
                    Ledger::pay (s, work, Site::AssetFound);
                    asset = &in.project_.assets[static_cast<std::size_t> (ai)]; s.raw_.loopBreaks += 1;
                    break;
                }
                Ledger::pay (s, work, Site::AssetContinue);
                s.raw_.loopIncrements += 1; ++ai;
            }
            else
            {
                Ledger::pay (s, work, Site::AssetEnd);
                s.raw_.loopFinalFalse += 1; s.raw_.loopBreaks += 1;
                break;
            }
        }
        Ledger::pay (s, work, Site::AssetMissingTest);
        if (asset == nullptr)
        {
            Ledger::pay (s, work, Site::AssetMissing);
            Ledger::retire (s, work);
            return CaptureStatus::MissingAsset;
        }
        Ledger::pay (s, work, Site::CaptureMetadata);
        ::new (static_cast<void*> (s.metadataBytes_)) CapturedMetadata {
            in.project_.id, *clip, *asset, in.project_.sampleRate, in.requestId_, in.version_, in.generation_ };
        s.metadataLive_ = true;
        Ledger::pay (s, s.legacyFixed_, Site::LegacyCaptureOptions);
        ::new (static_cast<void*> (s.legacyOptionsBytes_)) LegacyPreparationOptions (in.legacyOptions_);
        s.legacyOptionsLive_ = true;
        Ledger::pay (s, work, Site::CaptureParents);
        ::new (static_cast<void*> (s.parentBytes_)) Parents (in.source_, in.readyView_);
        s.parentLive_ = true;
        Ledger::pay (s, work, Site::SourceCheck);
        const auto& source = s.parents().original;
        if (! source)
        {
            Ledger::pay (s, work, Site::SourceMissing); Ledger::retire (s, work);
            return CaptureStatus::InvalidSource;
        }
        Ledger::pay (s, work, Site::SourceLayout);
        if ((clip->timeBase != engine::TimeBase::TempoLocked) | (clip->srcLen == 0)
            | (asset->channels == 0) | (asset->channels > 2) | (source->channels != asset->channels)
            | (source->frames != asset->frames) | (clip->srcOffset > asset->frames))
        {
            Ledger::pay (s, work, Site::SourceLayoutRefused); Ledger::retire (s, work);
            return CaptureStatus::InvalidSource;
        }
        Ledger::pay (s, work, Site::SourceExtent);
        if ((clip->srcLen > asset->frames - clip->srcOffset)
            | (asset->frames > std::numeric_limits<std::size_t>::max() / asset->channels))
        {
            Ledger::pay (s, work, Site::SourceExtentRefused); Ledger::retire (s, work);
            return CaptureStatus::InvalidSource;
        }
        Ledger::pay (s, work, Site::SourceProduct);
        if (source->interleaved.size() != asset->frames * asset->channels)
        {
            Ledger::pay (s, work, Site::SourceProductRefused); Ledger::retire (s, work);
            return CaptureStatus::InvalidSource;
        }
        Ledger::pay (s, work, Site::MapNeededTest);
        if (mapCount != 0)
        {
            Ledger::pay (s, work, Site::MapAllocate);
            s.raw_.allocationAttempts += 1; s.raw_.requestedBytes = Ledger::add (s.raw_.requestedBytes, mapBytes);
            void* memory = NativeStorage::allocate (static_cast<std::size_t> (mapBytes));
            if (memory == nullptr)
            {
                Ledger::pay (s, work, Site::MapAllocateFailed); Ledger::retire (s, work);
                return CaptureStatus::AllocationFailed;
            }
            Ledger::pay (s, work, Site::MapInstall);
            s.map_ = static_cast<engine::TempoChange*> (memory); s.mapCapacity_ = mapCount;
            s.raw_.allocationSuccesses += 1; s.raw_.currentBytes += mapBytes;
            // The only prior live native allocation is state. Positive mapBytes
            // makes current>old peak on every admitted map-success path.
            s.raw_.peakBytes = s.raw_.currentBytes;
        }
        Ledger::pay (s, work, Site::MapLoopEntry);
        U64 mi = 0; s.raw_.loopEntries += 1;
        for (;;)
        {
            Ledger::pay (s, work, Site::MapLoopTest);
            s.raw_.loopConditions += 1;
            if (mi != mapCount)
            {
                Ledger::pay (s, work, Site::MapRow);
                const auto& row = in.project_.tempoMap[static_cast<std::size_t> (mi)];
                s.raw_.loopBodies += 1;
                const bool validBpm = (row.bpm > 0.0) & (row.bpm <= std::numeric_limits<double>::max());
                const bool validCurve = (row.curveToNext == engine::TempoCurve::Jump)
                                      | (row.curveToNext == engine::TempoCurve::LinearRamp);
                bool validTick;
                if (mi == 0)
                {
                    Ledger::pay (s, work, Site::MapFirst);
                    validTick = row.tick == 0;
                }
                else
                {
                    Ledger::pay (s, work, Site::MapLater);
                    validTick = row.tick > s.map_[mi - 1].tick;
                }
                Ledger::pay (s, work, Site::MapDecision);
                if ((! validBpm) | (! validCurve) | (! validTick))
                {
                    Ledger::pay (s, work, Site::MapInvalid);
                    s.raw_.loopReturns += 1; Ledger::retire (s, work);
                    return CaptureStatus::InvalidMap;
                }
                Ledger::pay (s, work, Site::MapStore);
                ::new (static_cast<void*> (s.map_ + mi)) engine::TempoChange (row);
                s.mapConstructed_ += 1; s.raw_.copiedMapRows += 1;
                Ledger::pay (s, work, Site::MapContinue);
                s.raw_.loopIncrements += 1; ++mi;
            }
            else
            {
                Ledger::pay (s, work, Site::MapEnd);
                s.raw_.loopFinalFalse += 1; s.raw_.loopBreaks += 1;
                break;
            }
        }
        Ledger::pay (s, work, Site::CaptureFinish);
        Ledger::retire (s, work);
        return CaptureStatus::RequestCaptured;
    }
    static void noState (RootCensus& trace, CaptureStatus status, Formed& out) noexcept
    {
        rootEvent (trace, Site::RootNoState);
        out.state = nullptr; out.status = status;
        out.failureTail = trace; // sole continuing failed logical root.
        trace.handedOff = true;
    }
    static void failedSnapshot (Formed& out) noexcept
    {
        auto& trace = out.failureTail;
        rootEvent (trace, Site::RootFailedSnapshot);
        out.receipt.status = out.status; out.receipt.physicalState = false;
        out.receipt.origin.cap = workCap;
        out.receipt.origin.actual = trace.actual; out.receipt.origin.reserved = trace.unused;
        out.receipt.origin.total = Ledger::add (trace.actual, trace.unused);
        trace.raw.loopEntries += 1;
        for (std::size_t i = 0; i != phaseCount; ++i)
        {
            out.receipt.origin.actualByPhase[i] = 0; out.receipt.origin.reservedByPhase[i] = 0;
            trace.raw.loopConditions += 1; trace.raw.loopBodies += 1; trace.raw.loopIncrements += 1;
        }
        trace.raw.loopConditions += 1; trace.raw.loopFinalFalse += 1;
        out.receipt.origin.actualByPhase[Ledger::ix (Phase::Formation)] = trace.actual;
        out.receipt.origin.reservedByPhase[Ledger::ix (Phase::Formation)] = trace.unused;
        out.receipt.raw = trace.raw;
    }
    static void form (const engine::Project& project, engine::EntityId clip,
                      const SampleOwner& original, const SampleOwner& ready,
                      U64& nextId, U64& version, U64 generation, const LegacyPreparationOptions& legacyOptions,
                      RootCensus& trace, Formed& out) noexcept
    {
        // Whole logical initial region already funds common alias/check source.
        U64 capturedId, capturedVersion;
        if (&nextId == &version)
        {
            rootEvent (trace, Site::RootCounterAlias);
            capturedId = 0; capturedVersion = 0;
        }
        else if ((nextId == 0) | (nextId == std::numeric_limits<U64>::max())
                 | (version == std::numeric_limits<U64>::max()))
        {
            rootEvent (trace, Site::RootCounterExhausted);
            capturedId = 0; capturedVersion = 0;
        }
        else
        {
            rootEvent (trace, Site::RootCounterIssue);
            capturedId = nextId; nextId += 1; version += 1; capturedVersion = version;
        }
        CapturedIntent intent (project, clip, original, ready, capturedId, capturedVersion, generation, legacyOptions);
        rootEvent (trace, Site::RootIdentity);
        if (intent.consumed_) std::terminate();
        intent.consumed_ = true;
        // Explicit integral masks preserve eager validation and both Raw effects.
        const auto invalidIdentity = static_cast<U64> (! validId (project.id, trace.raw))
            | static_cast<U64> (! validId (clip, trace.raw))
            | static_cast<U64> (capturedId == 0) | static_cast<U64> (capturedVersion == 0)
            | static_cast<U64> (generation == 0);
        if (invalidIdentity != 0)
        {
            rootEvent (trace, Site::RootIdentityRefused);
            noState (trace, CaptureStatus::InvalidIdentity, out);
            return;
        }
        rootEvent (trace, Site::RootAllocate);
        trace.raw.allocationAttempts += 1; trace.raw.requestedBytes = sizeof (MusicalPreparationRequestState);
        void* memory = NativeStorage::allocate (sizeof (MusicalPreparationRequestState));
        if (memory == nullptr)
        {
            rootEvent (trace, Site::RootAllocateFailed);
            noState (trace, CaptureStatus::AllocationFailed, out);
            return;
        }
        rootEvent (trace, Site::RootObject);
        auto* s = ::new (memory) MusicalPreparationRequestState (MusicalPreparationRequestState::Uninitialized {});
        trace.raw.allocationSuccesses += 1; trace.raw.currentBytes = sizeof (MusicalPreparationRequestState);
        trace.raw.peakBytes = trace.raw.currentBytes;
        install (*s, trace);
        Ledger::pay (*s, s->root_, Site::CaptureDispatch);
        s->captureStatus_ = captureVariable (*s, intent);
        Ledger::pay (*s, s->root_, Site::CaptureResultTest);
        if (s->captureStatus_ != CaptureStatus::RequestCaptured)
            Ledger::pay (*s, s->root_, Site::CaptureRefused);
        out.state = s; out.status = s->captureStatus_;
        out.failureTail = trace; out.failureTail.handedOff = true;
    }
};

struct Exchange final { std::atomic<Signal> signal { Signal::Empty }; };
// One real separately owned result. Control's inline cell and the caller-owned
// detached value are distinct physical objects. State has no pointer back to
// either. Definitions in worker-result.h make every ownership action concrete.
class WorkerLegacyResult final
{
    friend class ProofAccess;
    friend class ApplicationRequests;
    friend class ControlRequestOwner;
    friend class FundedLegacyStageAdapter;
    using PlanOwner = std::shared_ptr<const engine::MusicalAudioChannelPlan>;
    struct Cell final {};
    struct Take final {};
    MusicalPreparationRequestState* state_;
    bool ownsOrigin_, external_, taken_, complete_;
    LegacyReadout readout_;
    alignas (PlanOwner) std::byte planBytes_[sizeof (PlanOwner)];
    explicit WorkerLegacyResult (Cell, MusicalPreparationRequestState&) noexcept;
    WorkerLegacyResult (Take, WorkerLegacyResult&) noexcept;
    PlanOwner& plan() noexcept
    { return *std::launder (reinterpret_cast<PlanOwner*> (planBytes_)); }
    void publish (MusicalPreparationRequestState&, LegacyReadout&) noexcept;
public:
    WorkerLegacyResult (const WorkerLegacyResult&) = delete;
    WorkerLegacyResult (WorkerLegacyResult&&) = delete;
    WorkerLegacyResult& operator= (const WorkerLegacyResult&) = delete;
    WorkerLegacyResult& operator= (WorkerLegacyResult&&) = delete;
    ~WorkerLegacyResult() noexcept;
};
static_assert (std::is_nothrow_destructible_v<WorkerLegacyResult>);
class ControlRequestOwner final
{
    friend class ProofAccess;
    friend class ApplicationRequests;
    friend class RetainedOriginOwner;
    friend class FundedLegacyStageAdapter;
    template<bool> friend class ContextScope;
    MusicalPreparationRequestState* state_;
    const FormationReceipt receipt_;
    RootCensus failureTail_;
    Exchange exchange_;
    bool armed_, terminalAcquired_, resultLive_;
    alignas (WorkerLegacyResult) std::byte resultBytes_[sizeof (WorkerLegacyResult)];
    WorkerLegacyResult& result() noexcept
    { return *std::launder (reinterpret_cast<WorkerLegacyResult*> (resultBytes_)); }
    explicit ControlRequestOwner (Formed& formed) noexcept
        : state_ (formed.state), receipt_ (formed.receipt), failureTail_ (formed.failureTail),
          armed_ (false), terminalAcquired_ (false), resultLive_ (false)
    {
        formed.state = nullptr; formed.failureTail.handedOff = true;
    }
    // Administrative prefix is already backed by either the live Control role
    // slice or the same failed logical root. No public generic funding/query API.
    void ownerEvent (Site physical, Site failed) noexcept
    {
        if (state_ != nullptr)
        {
            Ledger::pay (*state_, state_->roleLife_[Ledger::ix (Role::Control)], physical);
            return;
        }
        Formation::rootEvent (failureTail_, failed);
    }
    void constructedInSlot (bool& live) noexcept
    {
        // Actual constructor has returned. Each path records that real invocation
        // and makes its inline cell live under its own existing sponsor.
        if (state_ != nullptr)
        {
            auto& s = *state_;
            Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::OwnerConstructed);
            s.raw_.ownerConstructions += 1; live = true;
            return;
        }
        Formation::rootEvent (failureTail_, Site::RootFailedConstructed);
        failureTail_.raw.ownerConstructions += 1; live = true;
    }
    ~ControlRequestOwner() noexcept
    {
        // Called exactly once by the private inline slot, never on a cleared owner.
        if (state_ == nullptr)
        {
            Formation::rootEvent (failureTail_, Site::RootFailedDestroy);
            failureTail_.raw.ownerDestructions += 1;
            failureTail_.handedOff = true;
            return;
        }
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::ControlDestroy);
        if ((armed_ & ! terminalAcquired_) | (s.context_ != Context::Control)) std::terminate();
        Ledger::pay (s, s.legacyFixed_, Site::LegacyControlResultProbe);
        if (resultLive_)
        {
            Ledger::pay (s, s.legacyCleanup_, Site::LegacyControlResultDestroy);
            resultLive_ = false;
            result().~WorkerLegacyResult();
        }
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::ControlDetachTest);
        if (s.retainedIssued_)
        {
            Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::ControlDetach);
            s.context_ = Context::DetachedControl;
        }
        Ledger::pay (s, s.legacyFixed_, Site::LegacyControlResultDetach);
        if (s.resultOriginLive_) s.context_ = Context::DetachedControl;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::ControlDropCaller);
        s.raw_.ownerDestructions += 1;
        Ledger::drop (state_, Role::Control);
        // Only the one prepaid destructor/member/caller return tail follows.
    }
    void armWorker() noexcept
    {
        auto& s = *state_;
        auto& life = s.roleLife_[Ledger::ix (Role::Control)];
        Ledger::pay (s, life, Site::ArmWorker);
        if ((s.context_ != Context::Control) | s.workerIssued_
            | (s.captureStatus_ != CaptureStatus::RequestCaptured)) std::terminate();
        Ledger::pay (s, s.legacyFixed_, Site::LegacyArmResult);
        if (resultLive_) std::terminate();
        // This already-entered caller sponsors the direct LegacyReadout's
        // four nested status DMI writes/implicit constructors BEFORE Cell's
        // body, separately from that body's later initialization/copy.
        ::new (static_cast<void*> (resultBytes_)) WorkerLegacyResult (WorkerLegacyResult::Cell {}, s);
        resultLive_ = true;
        s.workerIssued_ = true; s.references_ += 1; s.raw_.referenceRetains += 1;
        s.context_ = Context::WorkerArmed; armed_ = true; terminalAcquired_ = false;
        exchange_.signal.store (Signal::Armed, std::memory_order_release);
    }
    void abortWorker() noexcept
    {
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::AbortWorker);
        if (s.context_ != Context::WorkerArmed) std::terminate();
        auto* ref = state_; Ledger::drop (ref, Role::Worker);
        s.context_ = Context::Control; armed_ = false;
        Ledger::pay (s, s.legacyCleanup_, Site::LegacyAbortResult);
        if (! resultLive_) std::terminate();
        resultLive_ = false;
        result().~WorkerLegacyResult();
        exchange_.signal.store (Signal::Empty, std::memory_order_release);
    }
    void acquireWorker() noexcept
    {
        // Prefix is reserved before context handoff; caller knows terminal from
        // its real task. No ledger mutation precedes the successful acquire.
        if (exchange_.signal.load (std::memory_order_acquire) != Signal::Terminal) std::terminate();
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::AcquireWorker);
        s.raw_.terminalLoads += 1;
        if (s.context_ != Context::ControlReturn) std::terminate();
        s.context_ = Context::Control; terminalAcquired_ = true;
    }
    ContextScope<false> receiveWorker (const std::atomic<bool>& cancel) noexcept;
public:
    ControlRequestOwner (const ControlRequestOwner&) = delete;
    ControlRequestOwner (ControlRequestOwner&&) = delete;
};

class RetainedOriginOwner final
{
    friend class ProofAccess;
    friend class ApplicationRequests;
    template<bool> friend class ContextScope;
    MusicalPreparationRequestState* state_;
    Exchange exchange_;
    bool armed_, terminalAcquired_;
    explicit RetainedOriginOwner (ControlRequestOwner& owner) noexcept
        : state_ (owner.state_), armed_ (false), terminalAcquired_ (false) {}
public:
    RetainedOriginOwner (const RetainedOriginOwner&) = delete;
    RetainedOriginOwner (RetainedOriginOwner&&) = delete;
    ~RetainedOriginOwner() noexcept
    {
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Retained)], Site::RetainedDestroy);
        if ((armed_ & ! terminalAcquired_)
            | ((s.context_ != Context::Control) & (s.context_ != Context::DetachedControl))) std::terminate();
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Retained)], Site::RetainedDropCaller);
        Ledger::drop (state_, Role::Retained);
    }
    void armReader() noexcept
    {
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Retained)], Site::ArmReader);
        if ((s.context_ != Context::DetachedControl) | s.readerIssued_) std::terminate();
        s.readerIssued_ = true; s.references_ += 1; s.raw_.referenceRetains += 1;
        s.context_ = Context::ReaderArmed; armed_ = true; terminalAcquired_ = false;
        exchange_.signal.store (Signal::Armed, std::memory_order_release);
    }
    void abortUnstartedReader() noexcept
    {
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Retained)], Site::AbortReader);
        if (s.context_ != Context::ReaderArmed) std::terminate();
        auto* ref = state_; Ledger::drop (ref, Role::Reader);
        s.context_ = Context::DetachedControl; armed_ = false;
        exchange_.signal.store (Signal::Empty, std::memory_order_release);
    }
    void acquireReturnedReader() noexcept
    {
        if (exchange_.signal.load (std::memory_order_acquire) != Signal::Terminal) std::terminate();
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Retained)], Site::AcquireReader);
        s.raw_.terminalLoads += 1;
        if (s.context_ != Context::DetachedReturn) std::terminate();
        s.context_ = Context::DetachedControl; terminalAcquired_ = true;
    }
    ContextScope<true> receiveReader (const std::atomic<bool>& cancel) noexcept;
};

template<bool Reader> class ContextScope final
{
    friend class ControlRequestOwner;
    friend class RetainedOriginOwner;
    friend class ApplicationRequests;
    friend class FundedLegacyStageAdapter;
    MusicalPreparationRequestState* state_;
    Exchange* exchange_;
    const std::atomic<bool>* cancel_;
    bool inspected_, cancelChecked_;
    ContextScope (MusicalPreparationRequestState* state, Exchange& exchange,
                  const std::atomic<bool>& cancel) noexcept
        : state_ (state), exchange_ (&exchange), cancel_ (&cancel), inspected_ (false), cancelChecked_ (false)
    {
        if (exchange.signal.load (std::memory_order_acquire) != Signal::Armed) std::terminate();
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Reader ? Role::Reader : Role::Worker)],
                     Reader ? Site::ReceiveReader : Site::ReceiveWorker);
        if (s.context_ != (Reader ? Context::ReaderArmed : Context::WorkerArmed)) std::terminate();
        s.context_ = Reader ? Context::ReaderRunning : Context::WorkerRunning;
        exchange.signal.store (Signal::Running, std::memory_order_release);
    }
public:
    ContextScope (const ContextScope&) = delete;
    ContextScope (ContextScope&&) = delete;
    ~ContextScope() noexcept
    {
        auto& s = *state_;
        auto& life = s.roleLife_[Ledger::ix (Reader ? Role::Reader : Role::Worker)];
        Ledger::pay (s, life, Site::ContextReturn);
        if (s.references_ < 2) std::terminate();
        s.context_ = Reader ? Context::DetachedReturn : Context::ControlReturn;
        s.raw_.terminalStores += 1;
        Ledger::drop (state_, Reader ? Role::Reader : Role::Worker);
        auto* publication = exchange_; exchange_ = nullptr; cancel_ = nullptr;
        publication->signal.store (Signal::Terminal, std::memory_order_release);
        // No origin/ref/context work after terminal publication.
    }
    bool cancellationRequestedOnce() noexcept
    {
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Reader ? Role::Reader : Role::Worker)], Site::ContextCancel);
        if (cancelChecked_) std::terminate();
        cancelChecked_ = true;
        return cancel_->load (std::memory_order_acquire);
    }
    bool inspectCapturedRequestOnce() noexcept
    {
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Reader ? Role::Reader : Role::Worker)], Site::ContextInspect);
        if (inspected_) std::terminate();
        inspected_ = true;
        // Successful arm proves live metadata/Parents; no unsafe short circuit or
        // fake legacy matching. Access all fixed identity fields uniformly.
        const auto validIdentity = static_cast<U64> (s.captureStatus_ == CaptureStatus::RequestCaptured)
            & static_cast<U64> (s.metadataLive_) & static_cast<U64> (s.parentLive_)
            & static_cast<U64> (s.metadata().requestId != 0)
            & static_cast<U64> (s.metadata().requestVersion != 0);
        return validIdentity != 0;
    }
    void checkForReceiverOnce() noexcept
    {
        const bool cancelled = cancellationRequestedOnce();
        auto& s = *state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Reader ? Role::Reader : Role::Worker)], Site::ContextDecision);
        if (! cancelled)
            if (! inspectCapturedRequestOnce()) std::terminate();
    }
};
inline ContextScope<false> ControlRequestOwner::receiveWorker (const std::atomic<bool>& cancel) noexcept
{ return ContextScope<false> (state_, exchange_, cancel); }
inline ContextScope<true> RetainedOriginOwner::receiveReader (const std::atomic<bool>& cancel) noexcept
{ return ContextScope<true> (state_, exchange_, cancel); }

// Raw inline cell: no checking/clearing destructor and no automatic contained-owner
// destruction. ApplicationRequests alone constructs/destroys each live owner once.
struct OwnerSlot final
{
    alignas (ControlRequestOwner) std::byte bytes[sizeof (ControlRequestOwner)];
};
static_assert (std::is_trivially_destructible_v<OwnerSlot>);

class ApplicationRequests final
{
    friend class ProofAccess;
    OwnerSlot slots_[2];
    bool live_[2];
    std::size_t current_, retiring_;
    bool hasRetiring_;
    ControlRequestOwner& owner (std::size_t index) noexcept
    { return *std::launder (reinterpret_cast<ControlRequestOwner*> (slots_[index].bytes)); }
    static void prepareAdoption (Formed& formed) noexcept
    {
        if (formed.state != nullptr)
        {
            auto& s = *formed.state;
            Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::OwnerAdopt);
            Ledger::snapshot (s, formed.receipt);
            return;
        }
        Formation::rootEvent (formed.failureTail, Site::RootFailedAdopt);
        Formation::failedSnapshot (formed);
    }
    void erase (std::size_t index) noexcept
    {
        // The live owner funds this exact prefix and final caller return tail.
        // No generic destroy-on-empty/repeated-clear API is exposed.
        auto& old = owner (index);
        old.ownerEvent (Site::SlotErase, Site::RootFailedSlotErase);
        if (! live_[index]) std::terminate();
        live_[index] = false; // before sponsor death, not a future empty-owner check.
        old.~ControlRequestOwner();
    }
public:
    ApplicationRequests (const engine::Project& project, engine::EntityId clip,
                         const SampleOwner& original, const SampleOwner& ready,
                         U64& nextId, U64& version, U64 generation,
                         const LegacyPreparationOptions& legacyOptions) noexcept
    {
        // The first request logically reserves BEFORE ctor/argument/first fields.
        // Raw cell arrays have no value initialization or owning member destructor.
        RootCensus trace;
        Formation::beginRoot (trace);
        Formation::rootEvent (trace, Site::RootSessionInit);
        current_ = 0; retiring_ = 1; hasRetiring_ = false; live_[0] = false; live_[1] = false;
        Formed formed;
        Formation::form (project, clip, original, ready, nextId, version, generation, legacyOptions, trace, formed);
        prepareAdoption (formed);
        ::new (static_cast<void*> (slots_[0].bytes)) ControlRequestOwner (formed);
        owner (0).constructedInSlot (live_[0]);
    }
    ApplicationRequests (const ApplicationRequests&) = delete;
    ApplicationRequests (ApplicationRequests&&) = delete;
    ~ApplicationRequests() noexcept
    {
        // One current owner ALWAYS remains live, including initial failed-root
        // owner. It funds this branch before either whole-owner destruction.
        auto& sponsor = owner (current_);
        sponsor.ownerEvent (Site::SessionDestroyCheck, Site::RootFailedSessionCheck);
        if (hasRetiring_)
        {
            sponsor.ownerEvent (Site::SessionDestroyRetiring, Site::RootFailedSessionRetiring);
            // Caller proves the actual task reached terminal. Perform the real
            // acquire here before its whole-owner destruction; this is no join,
            // poll or racing cancellation. An in-flight teardown is misuse.
            owner (retiring_).acquireWorker();
            hasRetiring_ = false;
            erase (retiring_);
        }
        sponsor.ownerEvent (Site::SessionDestroyFinish, Site::RootFailedSessionFinish);
        erase (current_);
        // No checking slot destructor or metadata/ledger read follows this free.
    }
    Readout captureNext (const engine::Project& project, engine::EntityId clip,
                         const SampleOwner& original, const SampleOwner& ready,
                         U64& nextId, U64& version, U64 generation,
                         const LegacyPreparationOptions& legacyOptions) noexcept
    {
        // A genuinely NEW request, not a retry on an existing intent or Plan.
        RootCensus trace;
        Formation::beginRoot (trace);
        Formation::rootEvent (trace, Site::RootNextEntry);
        Formed formed;
        Formation::form (project, clip, original, ready, nextId, version, generation, legacyOptions, trace, formed);
        if (formed.state == nullptr)
        {
            // New failure leaves both old slots unchanged. This same failed root
            // is closed synchronously instead of constructing a third owner.
            Formation::rootEvent (formed.failureTail, Site::RootFailedReadout);
            const auto disposition = Disposition::PreservedOnFailure;
            Formation::rootEvent (formed.failureTail, Site::RootFailedDiscard);
            Formation::failedSnapshot (formed);
            formed.failureTail.handedOff = true;
            return Readout (formed.receipt, disposition);
        }
        auto& s = *formed.state;
        Ledger::pay (s, s.root_, Site::NewFailureTest);
        if (formed.status != CaptureStatus::RequestCaptured)
        {
            Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::AbandonNewState);
            const auto disposition = Disposition::PreservedOnFailure;
            Ledger::pay (s, s.root_, Site::ReadoutCommit);
            Ledger::snapshot (s, formed.receipt);
            Ledger::drop (formed.state, Role::Control);
            return Readout (formed.receipt, disposition);
        }
        Ledger::pay (s, s.root_, Site::SessionSelect);
        auto& old = owner (current_);
        const bool running = old.armed_ & ! old.terminalAcquired_;
        std::size_t target;
        Disposition disposition;
        if (running)
        {
            Ledger::pay (s, s.root_, Site::SessionKeepRunning);
            if (hasRetiring_) std::terminate(); // at most ONE active/retiring worker.
            retiring_ = current_; hasRetiring_ = true;
            target = current_ ^ std::size_t { 1 };
            if (live_[target]) std::terminate();
            disposition = Disposition::KeptRetiring;
        }
        else
        {
            Ledger::pay (s, s.root_, Site::SessionReusePending);
            target = current_; disposition = Disposition::ReplacedPending;
            erase (target); // WHOLE old owner once, new captures already complete.
        }
        Ledger::pay (s, s.root_, Site::SessionInstall);
        Ledger::pay (s, s.root_, Site::ReadoutCommit);
        prepareAdoption (formed);
        ::new (static_cast<void*> (slots_[target].bytes)) ControlRequestOwner (formed);
        owner (target).constructedInSlot (live_[target]);
        current_ = target;
        // The owner's const receipt is its pre-construction history. This separate
        // attempt readout includes the actual completed owner installation.
        Ledger::snapshot (s, formed.receipt);
        return Readout (formed.receipt, disposition);
    }
    // One unique worker INCLUDING retiring (ADR0083). Launch/join are not here.
    // Returned reference stays at the same inline address through task terminal.
    ControlRequestOwner& armCurrentWorker() noexcept
    {
        auto& current = owner (current_);
        // ArmWorker pays actual caller/slot/hasRetiring check before handoff.
        if (hasRetiring_) std::terminate();
        current.armWorker();
        return current;
    }
    void abortUnstartedCurrentWorker() noexcept
    { owner (current_).abortWorker(); }
    void acquireCurrentWorkerAfterTerminal() noexcept
    {
        // This direct caller's receive-safe lookup/call prefix and return are
        // already reserved in the Control lifetime before handoff. Only the real
        // successful acquire permits its separate actual canonical debit.
        auto& current = owner (current_);
        current.acquireWorker();
        auto& s = *current.state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::AcquireCurrentCaller);
    }
    void drainRetiringAfterTerminal() noexcept
    {
        // Finite one-use caller knows a retiring task actually reached terminal.
        // DrainRetiring's held Control reserve covers its distinct caller prefix;
        // shared AcquireWorker covers only the common real acquire body.
        auto& old = owner (retiring_);
        old.acquireWorker();
        auto& s = *old.state_;
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::DrainRetiring);
        if (! hasRetiring_) std::terminate();
        hasRetiring_ = false;
        erase (retiring_);
    }
    RetainedOriginOwner retainCurrentAfterTerminal() noexcept
    {
        auto& current = owner (current_);
        auto& s = *current.state_;
        Ledger::pay (s, s.legacyFixed_, Site::LegacyRetainedResultCheck);
        if (s.resultOriginLive_) std::terminate();
        Ledger::pay (s, s.roleLife_[Ledger::ix (Role::Control)], Site::RetainExternal);
        if ((s.context_ != Context::Control) | s.retainedIssued_ | ! current.armed_ | ! current.terminalAcquired_)
            std::terminate();
        s.retainedIssued_ = true; s.references_ += 1; s.raw_.referenceRetains += 1;
        return RetainedOriginOwner (current);
    }
    static void workerRequestCheck (ControlRequestOwner& owner, const std::atomic<bool>& cancel) noexcept
    {
        auto context = owner.receiveWorker (cancel); // FIRST local guard.
        context.checkForReceiverOnce();
    }
    static void readerRequestCheck (RetainedOriginOwner& owner, const std::atomic<bool>& cancel) noexcept
    {
        auto context = owner.receiveReader (cancel); // FIRST local guard.
        context.checkForReceiverOnce();
    }
    // Defined only in legacy-adapter.h. It enters the existing real Worker
    // acquire/handoff first; its separate caller/adapter work uses R7 fixed funds.
    static LegacyReadout workerBuildLegacy (ControlRequestOwner& owner, const std::atomic<bool>& cancel) noexcept;
    // Performs the actual terminal acquire itself, then constructs ONE real
    // prvalue directly in caller-owned storage. Alternative to acquireCurrent:
    // this finite handoff path must not acquire twice. No arbitrary Plan/Fund.
    WorkerLegacyResult takeCurrentResultAfterTerminal() noexcept;
    CaptureStatus currentFormationStatus() const noexcept
    {
        // Immutable historical readout only. No origin/owner/task authority.
        const auto* current = std::launder (reinterpret_cast<const ControlRequestOwner*> (slots_[current_].bytes));
        return current->receipt_.status;
    }
};
} // namespace yesdaw::app::musical_origin_source_r7
