#pragma once

// SOURCE ONLY / UNBOUND / UNCOMPILED. The three old builder calls and conditional
// smaller-B query are concrete. Native/data/price/forward proofs are NOT bound;
// this package has no execution or production admission.
#include "window-query.h"
#include <bit>

namespace yesdaw::app::musical_origin_source_r7 {

class FundedLegacyStageAdapter final
{
    friend class ApplicationRequests;
    struct Common final
    {
        U64 rows, clockBytes, byteLimit;
        engine::MusicalAudioLimits requested;
    };
    struct PreparedAttempt final
    {
        U64 index, fee;
        engine::MusicalAudioLimits limits;
    };

    static U64 minimum (U64 a, U64 b) noexcept { return a < b ? a : b; }
    static void fixed (MusicalPreparationRequestState& s, Site site) noexcept
    { Ledger::pay (s, s.legacyFixed_, site); }
    static bool refuse (MusicalPreparationRequestState& s, LegacyReadout& result,
                        LegacyOutcome outcome) noexcept
    {
        fixed (s, Site::LegacyRefusal);
        result.outcome = outcome;
        return false;
    }
    static bool stop (MusicalPreparationRequestState& s) noexcept
    {
        fixed (s, Site::LegacyStop);
        return false;
    }
    static bool ready (MusicalPreparationRequestState& s) noexcept
    {
        fixed (s, Site::LegacyReady);
        return true;
    }

    static U64 total (MusicalPreparationRequestState& s) noexcept
    {
        fixed (s, Site::LegacyBudgetEntry);
        U64 debit = 0;
        std::size_t i = 0;
        s.raw_.loopEntries += 1;
        for (;;)
        {
            fixed (s, Site::LegacyBudgetTest);
            s.raw_.loopConditions += 1;
            if (i == phaseCount)
            {
                fixed (s, Site::LegacyBudgetEnd);
                s.raw_.loopFinalFalse += 1; s.raw_.loopBreaks += 1;
                return debit;
            }
            fixed (s, Site::LegacyBudgetRow);
            debit = Ledger::add (debit, Ledger::add (s.ledger_.actual[i], s.ledger_.reserved[i]));
            s.raw_.loopBodies += 1; s.raw_.legacy.budgetRows += 1;
            fixed (s, Site::LegacyBudgetContinue);
            ++i; s.raw_.loopIncrements += 1;
        }
    }

    static bool cancelled (ContextScope<false>& context, MusicalPreparationRequestState& s,
                           LegacyReadout& result) noexcept
    {
        fixed (s, Site::LegacyCancel);
        s.raw_.legacy.cancellationLoads += 1;
        const bool value = context.cancel_->load (std::memory_order_acquire);
        if (value)
        {
            fixed (s, Site::LegacyCancellationTaken);
            result.outcome = LegacyOutcome::Cancelled;
        }
        return value;
    }

    static bool common (MusicalPreparationRequestState& s, Common& c, LegacyReadout& result) noexcept
    {
        fixed (s, Site::LegacyCommonIdentity);
        const auto& metadata = s.metadata();
        const auto& options = s.legacyOptions();
        c.requested = options.limits;
        c.rows = s.mapConstructed_;
        const double projectHz = metadata.projectRate.hz;
        if ((! std::isfinite (projectHz)) | (projectHz <= 0.0) | (s.mapConstructed_ != s.mapCapacity_)
            | ((c.rows != 0) & (s.map_ == nullptr)))
            return refuse (s, result, LegacyOutcome::InvalidCapturedRequest);
        fixed (s, Site::LegacyMappedPresent);
        const auto& mapped = s.parents().readyView;
        if (! mapped)
            return refuse (s, result, LegacyOutcome::InvalidMappedOwner);
        fixed (s, Site::LegacyMappedLayout);
        const auto channels = metadata.asset.channels;
        if (((channels != 1) & (channels != 2)) | (mapped->channels != channels))
            return refuse (s, result, LegacyOutcome::InvalidMappedOwner);
        fixed (s, Site::LegacyMappedExtent);
        const auto selected = options.mappedWindow;
        if ((selected.srcLen == 0) | (selected.srcOffset > mapped->frames)
            | (mapped->frames > std::numeric_limits<std::size_t>::max() / channels))
            return refuse (s, result, LegacyOutcome::InvalidMappedOwner);
        fixed (s, Site::LegacyMappedProduct);
        if ((selected.srcLen > mapped->frames - selected.srcOffset)
            | (mapped->interleaved.size() != mapped->frames * channels))
            return refuse (s, result, LegacyOutcome::InvalidMappedOwner);
        fixed (s, Site::LegacyByteHeadroom);
        // This subtracts only the genuinely known new State+captured-map payload.
        // The old sizeof+64 graph declarations below remain DECLARED SUBTOTALS,
        // not complete actual allocation/control-block/whole-Clip measurements.
        if (s.raw_.currentBytes > byteCap)
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        fixed (s, Site::LegacyByteCompute);
        c.byteLimit = minimum (minimum (options.limits.maxReservedBytes,
                                       engine::kMusicalAudioMaxReservedBytes),
                               byteCap - s.raw_.currentBytes);
        fixed (s, Site::LegacyClockExtent);
        constexpr U64 base = sizeof (engine::MusicalAudioClock) + U64 { 4 } * 64;
        constexpr U64 row = U64 { 2 } * sizeof (engine::TempoChange) + sizeof (double);
        if ((c.byteLimit < base) | (c.rows > (workCap - 1) / 6))
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        fixed (s, Site::LegacyClockBytes);
        if (c.rows > (c.byteLimit - base) / row)
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        fixed (s, Site::LegacyClockByteStore);
        c.clockBytes = base + c.rows * row;
        fixed (s, Site::LegacyClockAllocatorLimits);
        // Prevent a vector length_error before/inside the real Clock call using
        // the actual default allocator domain, not a guessed vector ABI value.
        const std::allocator<engine::TempoChange> rowAllocator;
        const std::allocator<double> frameAllocator;
        if ((c.rows > std::allocator_traits<std::allocator<engine::TempoChange>>::max_size (rowAllocator))
            | (c.rows > std::allocator_traits<std::allocator<double>>::max_size (frameAllocator)))
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        return ready (s);
    }

    static bool reserveQuery (MusicalPreparationRequestState& s, const Common& c, LegacyReadout& result) noexcept
    {
        fixed (s, Site::LegacyQueryReserve);
        U64 amount;
        if (! FundedWindowQuery::quote (s, c.rows, amount))
        {
            fixed (s, Site::QueryQuoteRefused);
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        }
        if (! Ledger::reserve (s, s.legacyCheck_, s.legacyQuery_, Phase::Context, amount))
        {
            fixed (s, Site::LegacyQueryReserveRefused);
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        }
        return ready (s);
    }

    static bool prepare (MusicalPreparationRequestState& s, const Common& c,
                         LegacyStage stage, U64 fee, U64 inheritedWorkLimit,
                         U64 inheritedByteLimit, PreparedAttempt& out,
                         LegacyReadout& result) noexcept
    {
        fixed (s, Site::LegacyAttemptPrepare);
        const auto index = static_cast<std::size_t> (stage);
        // All private invariants are proved by one real Worker, one adapter
        // issuance and the straight Clock/Request/Plan controller below.
        if (index >= 3) std::terminate();
        if ((s.raw_.legacy.invocations[index] != 0)
            | (s.legacyAttempt_[index].remaining != 0)) std::terminate();
        const auto existingTotal = total (s);
        if (existingTotal > workCap) std::terminate();
        fixed (s, Site::LegacyAttemptLimit);
        const auto headroom = workCap - existingTotal;
        const auto existingLegacy = s.legacyEnteredContribution_;
        // BEFORE reserving this fee, existingLegacy+headroom is exactly
        // priorLegacy + heldCurrentFee + genuine headroom AFTER reservation.
        // Other actual work and ALL other reservations have already been removed.
        if (existingLegacy > workCap - headroom) std::terminate();
        const auto originCumulativeCeiling = existingLegacy + headroom;
        const auto effectiveWork = minimum (minimum (minimum (c.requested.maxWorkUnits,
                                                             engine::kMusicalAudioMaxWorkUnits),
                                                    inheritedWorkLimit), originCumulativeCeiling);
        if ((existingLegacy > effectiveWork) | (fee > headroom))
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        fixed (s, Site::LegacyAttemptFeeLimit);
        if (fee > effectiveWork - existingLegacy)
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        fixed (s, Site::LegacyAttemptKind);
        // Clock stores a cumulative ceiling usable by the later unchanged
        // endpoint lookup. This is ELIGIBILITY only, with no extra debit.
        if (stage == LegacyStage::Clock)
        {
            fixed (s, Site::LegacyClockEligibility);
            const auto eligibility = U64 { 512 } + U64 { 2 }
                * static_cast<U64> (std::bit_width (static_cast<std::size_t> (c.rows)));
            if (effectiveWork < eligibility)
                return refuse (s, result, LegacyOutcome::ResourceLimit);
        }
        fixed (s, Site::LegacyAttemptReserve);
        if (! Ledger::reserve (s, s.legacyCheck_, s.legacyAttempt_[index], Phase::Context, fee))
        {
            fixed (s, Site::LegacyAttemptReserveRefused);
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        }
        fixed (s, Site::LegacyAttemptReady);
        out.index = index; out.fee = fee;
        out.limits = { minimum (c.byteLimit, inheritedByteLimit), effectiveWork };
        return ready (s);
    }

    static void commitPrepared (MusicalPreparationRequestState& s, const PreparedAttempt& prepared) noexcept
    {
        // NO checks, cancellation, callbacks, allocations, exceptions or early
        // returns occur here. prepare proved every bound/cell/nonoverflow and
        // the same exclusive context remains live through the immediately next
        // one actual frozen call. Its new literal work was prepaid by the caller.
        const auto phase = Ledger::ix (Phase::Context);
        s.legacyAttempt_[prepared.index].remaining -= prepared.fee;
        s.ledger_.reserved[phase] -= prepared.fee;
        s.ledger_.actual[phase] += prepared.fee;
        s.legacyEnteredContribution_ += prepared.fee;
        s.raw_.legacy.attemptChargeConsumed[prepared.index] += prepared.fee;
        s.raw_.legacy.invocations[prepared.index] += 1;
    }

    static bool clock (ContextScope<false>& context, MusicalPreparationRequestState& s,
                       WorkerArtifacts& artifacts, FundedNativeSession& native,
                       const Common& c, LegacyReadout& result) noexcept
    {
        fixed (s, Site::LegacyClockPrepare);
        result.stage = LegacyStage::Clock;
        ClockOutputOwner outputOwner (s);
        auto& output = outputOwner.value();
        const engine::TempoMapView map { s.map_, static_cast<std::size_t> (c.rows) };
        const engine::SampleRate rate = s.metadata().projectRate;
        const U64 fee = 1 + 6 * c.rows; // common proved the exact checked domain.
        PreparedAttempt prepared;
        if (! prepare (s, c, LegacyStage::Clock, fee, workCap, c.byteLimit, prepared, result)) return stop (s);
        if (cancelled (context, s, result)) return stop (s);
        if (! native.check()) return stop (s);
        try
        {
            fixed (s, Site::LegacyClockCommitInvoke);
            commitPrepared (s, prepared);
            const auto status = engine::MusicalAudioClock::build (map, rate, prepared.limits, output);
            fixed (s, Site::LegacyClockReturned);
            s.raw_.legacy.returned[0] += 1;
            result.clockStatusKnown = true; result.clockStatus = status;
            if (! native.check()) return stop (s);
            if (status != engine::MusicalAudioStatus::Ok)
            {
                fixed (s, Site::LegacyClockFailure);
                if (status == engine::MusicalAudioStatus::NumericDomain)
                {
                    fixed (s, Site::LegacyClockNumericDomain);
                    result.observations[0].cumulativeEvidence = LegacyEvidence::SourceProved;
                    result.observations[0].cumulative = fee;
                }
                return refuse (s, result, LegacyOutcome::LegacyReturnedFailure);
            }
            fixed (s, Site::LegacyClockStore);
            if (! output) std::terminate();
            artifacts.clock_.value() = std::move (output); s.raw_.legacy.ownerMoves += 1;
            fixed (s, Site::LegacyClockCumulative);
            const auto stored = artifacts.clock_.value()->constructionWorkUnits();
            s.raw_.legacy.cumulativeReads += 1; s.raw_.legacy.cumulativeComparisons += 1;
            if ((stored != s.legacyEnteredContribution_)
                | (artifacts.clock_.value()->reservedBytes() != c.clockBytes)
                | (artifacts.clock_.value()->rows().size() != c.rows)) std::terminate();
            result.observations[0].cumulativeEvidence = LegacyEvidence::ReturnedOwner;
            result.observations[0].cumulative = stored;
            return ready (s);
        }
        catch (...)
        {
            fixed (s, Site::LegacyClockException);
            s.raw_.legacy.exceptions[0] += 1;
            result.outcome = LegacyOutcome::UnexpectedLegacyException;
            native.check();
            return false;
        }
    }

    static bool window (ContextScope<false>& context, MusicalPreparationRequestState& s,
                        WorkerArtifacts& artifacts, FundedNativeSession& native, LegacyReadout& result) noexcept
    {
        fixed (s, Site::LegacyQueryPrepare);
        if (cancelled (context, s, result)) return stop (s);
        fixed (s, Site::LegacyQueryInvoke);
        const auto called = FundedWindowQuery::resolve (s, native, artifacts.clock_.value(), artifacts.window_.value(), result);
        fixed (s, Site::LegacyQueryReturned);
        if (! called.invoked)
        { fixed (s, Site::QueryCallNotEntered); return stop (s); }
        fixed (s, Site::QueryCallEntered);
        result.windowStatusKnown = true; result.windowStatus = called.status;
        if ((result.outcome == LegacyOutcome::NativeQualificationFailed)
            | (result.outcome == LegacyOutcome::NativeRestoreFailed)
            | (result.outcome == LegacyOutcome::QueryReconciliationFailed))
        { fixed (s, Site::QueryCallOuterFailure); return stop (s); }
        if (called.status != engine::MusicalAudioStatus::Ok)
            return refuse (s, result, LegacyOutcome::LegacyReturnedFailure);
        return ready (s);
    }

    static bool request (ContextScope<false>& context, MusicalPreparationRequestState& s,
                         WorkerArtifacts& artifacts, const Common& c, LegacyReadout& result) noexcept
    {
        fixed (s, Site::LegacyRequestPreflight);
        result.stage = LegacyStage::Request;
        const auto& options = s.legacyOptions();
        const auto& retainedWindow = artifacts.window_.value();
        if ((options.policy.channelRevision != 1) | (options.policy.materializationRevision != 1))
            return refuse (s, result, LegacyOutcome::UnsupportedPolicy);
        fixed (s, Site::LegacyRequestWindow);
        if (! retainedWindow.valid())
            return refuse (s, result, LegacyOutcome::InvalidCapturedRequest);
        fixed (s, Site::LegacyRequestBytes);
        const auto clockLimits = retainedWindow.clock()->limits();
        const U64 byteLimit = minimum (c.byteLimit, clockLimits.maxReservedBytes);
        const U64 clockBytes = retainedWindow.clock()->reservedBytes();
        constexpr U64 keyBytes = sizeof (engine::MusicalAudioKey) + 64;
        constexpr U64 requestBytes = sizeof (engine::MusicalAudioChannelRequest) + 64;
        if ((clockBytes > byteLimit) | (keyBytes > byteLimit - minimum (clockBytes, byteLimit)))
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        fixed (s, Site::LegacyRequestByteTail);
        const U64 withKey = clockBytes + keyBytes;
        if (requestBytes > byteLimit - withKey)
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        fixed (s, Site::LegacyRequestPrepare);
        const auto& metadata = s.metadata();
        const auto& parents = s.parents();
        const auto mapped = options.mappedWindow;
        const auto tier = options.tier;
        const auto identity = options.identity;
        const auto policy = options.policy;
        RequestOutputOwner outputOwner (s);
        auto& output = outputOwner.value();
        PreparedAttempt prepared;
        if (! prepare (s, c, LegacyStage::Request, 4096, clockLimits.maxWorkUnits,
                       byteLimit, prepared, result)) return stop (s);
        if (cancelled (context, s, result)) return stop (s);
        try
        {
            fixed (s, Site::LegacyRequestCommitInvoke);
            commitPrepared (s, prepared);
            const auto status = engine::makeMusicalAudioChannelRequest (
                metadata.clip, metadata.asset, parents.original, parents.readyView,
                mapped, tier, retainedWindow, identity, policy, prepared.limits, output);
            fixed (s, Site::LegacyRequestReturned);
            s.raw_.legacy.returned[1] += 1;
            result.requestStatusKnown = true; result.requestStatus = status;
            result.observations[1].cumulativeEvidence = LegacyEvidence::SourceProved;
            result.observations[1].cumulative = s.legacyEnteredContribution_;
            if (status.status != engine::MusicalAudioChannelStatus::Ok)
                return refuse (s, result, LegacyOutcome::LegacyReturnedFailure);
            fixed (s, Site::LegacyRequestStore);
            if (! output) std::terminate();
            artifacts.request_.value() = std::move (output); s.raw_.legacy.ownerMoves += 1;
            fixed (s, Site::LegacyRequestCumulative);
            const auto stored = artifacts.request_.value()->accounting().constructionWorkUnits;
            s.raw_.legacy.cumulativeReads += 1; s.raw_.legacy.cumulativeComparisons += 1;
            if (stored != s.legacyEnteredContribution_) std::terminate();
            result.observations[1].cumulativeEvidence = LegacyEvidence::ReturnedOwner;
            result.observations[1].cumulative = stored;
            return ready (s);
        }
        catch (...)
        {
            fixed (s, Site::LegacyRequestException);
            s.raw_.legacy.exceptions[1] += 1;
            result.outcome = LegacyOutcome::UnexpectedLegacyException;
            return false;
        }
    }

    static bool plan (ContextScope<false>& context, MusicalPreparationRequestState& s,
                      WorkerArtifacts& artifacts, WorkerLegacyResult& destination,
                      const Common& c, LegacyReadout& result) noexcept
    {
        fixed (s, Site::LegacyPlanPreflight);
        result.stage = LegacyStage::Plan;
        const auto& input = artifacts.request_.value();
        if (! input) std::terminate();
        const auto policy = input->policy();
        const auto channels = input->logicalChannels();
        if ((policy.channelRevision != 1) | (policy.materializationRevision != 1)
            | ((channels != 1) & (channels != 2)) | (! input->window().valid())) std::terminate();
        fixed (s, Site::LegacyPlanOwner);
        const auto& mapped = input->mappedOwner();
        const auto selected = input->mappedWindow();
        if (! mapped) std::terminate();
        if (mapped->channels != channels) std::terminate();
        fixed (s, Site::LegacyPlanExtent);
        if ((selected.srcLen == 0) | (selected.srcOffset > mapped->frames)
            | (mapped->frames > std::numeric_limits<std::size_t>::max() / channels)) std::terminate();
        fixed (s, Site::LegacyPlanProduct);
        if ((selected.srcLen > mapped->frames - selected.srcOffset)
            | (mapped->interleaved.size() != mapped->frames * channels)
            | (selected.srcLen != input->window().inputFrames())) std::terminate();
        fixed (s, Site::LegacyPlanBytes);
        const auto clockLimits = input->window().clock()->limits();
        const auto requestLimits = input->limits();
        const U64 byteLimit = minimum (minimum (c.byteLimit, clockLimits.maxReservedBytes),
                                      requestLimits.maxReservedBytes);
        const auto retainedBytes = input->accounting().reservedBytes;
        constexpr U64 planBytes = sizeof (engine::MusicalAudioChannelPlan) + 64;
        if ((retainedBytes > byteLimit) | (planBytes > byteLimit - minimum (retainedBytes, byteLimit)))
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        fixed (s, Site::LegacyPlanTariff);
        const U64 perFrame = 8 + 16 * channels;
        if (selected.srcLen > (workCap - 1024) / perFrame)
            return refuse (s, result, LegacyOutcome::ResourceLimit);
        fixed (s, Site::LegacyPlanFee);
        const U64 fee = 1024 + selected.srcLen * perFrame;
        fixed (s, Site::LegacyPlanPrepare);
        PlanOutputOwner outputOwner (s);
        auto& output = outputOwner.value();
        PreparedAttempt prepared;
        if (! prepare (s, c, LegacyStage::Plan, fee,
                       minimum (clockLimits.maxWorkUnits, requestLimits.maxWorkUnits),
                       byteLimit, prepared, result)) return stop (s);
        if (cancelled (context, s, result)) return stop (s);
        try
        {
            fixed (s, Site::LegacyPlanCommitInvoke);
            commitPrepared (s, prepared);
            const auto status = engine::buildMusicalAudioChannelPlan (input, prepared.limits, output);
            fixed (s, Site::LegacyPlanReturned);
            s.raw_.legacy.returned[2] += 1;
            result.planStatusKnown = true; result.planStatus = status;
            result.observations[2].cumulativeEvidence = LegacyEvidence::SourceProved;
            result.observations[2].cumulative = s.legacyEnteredContribution_;
            if ((status.status == engine::MusicalAudioChannelStatus::Ok)
                | (status.status == engine::MusicalAudioChannelStatus::ResourceLimit))
            {
                // This is SOURCE-PROVED returned control flow, not a fake read
                // of discarded private accounting or an allocator-site guess.
                fixed (s, Site::LegacyPlanFullScan);
                result.observations[2].framesVisitedEvidence = LegacyEvidence::SourceProved;
                result.observations[2].samplesVisitedEvidence = LegacyEvidence::SourceProved;
                result.observations[2].frames = selected.srcLen;
                result.observations[2].samples = selected.srcLen * channels;
                s.raw_.legacy.knownFullScanOutcomes += 1;
            }
            if (status.status != engine::MusicalAudioChannelStatus::Ok)
                return refuse (s, result, LegacyOutcome::LegacyReturnedFailure);
            fixed (s, Site::LegacyPlanStore);
            if (! output) std::terminate();
            fixed (s, Site::LegacyResultPlanMove);
            destination.plan() = std::move (output); s.raw_.legacy.ownerMoves += 1;
            fixed (s, Site::LegacyPlanCumulative);
            const auto& stored = destination.plan()->accounting();
            s.raw_.legacy.cumulativeReads += 1; s.raw_.legacy.cumulativeComparisons += 1;
            if (stored.constructionWorkUnits != s.legacyEnteredContribution_) std::terminate();
            result.observations[2].cumulativeEvidence = LegacyEvidence::ReturnedOwner;
            result.observations[2].cumulative = stored.constructionWorkUnits;
            result.observations[2].framesVisitedEvidence = LegacyEvidence::ReturnedOwner;
            result.observations[2].samplesVisitedEvidence = LegacyEvidence::ReturnedOwner;
            result.observations[2].frames = stored.framesVisited;
            result.observations[2].samples = stored.samplesVisited;
            return ready (s);
        }
        catch (...)
        {
            fixed (s, Site::LegacyPlanException);
            s.raw_.legacy.exceptions[2] += 1;
            result.outcome = LegacyOutcome::UnexpectedLegacyException;
            return false;
        }
    }

    static LegacyReadout finish (MusicalPreparationRequestState& s, WorkerLegacyResult& destination,
                                 LegacyReadout& result) noexcept
    {
        fixed (s, Site::LegacyFinish);
        s.raw_.legacy.resultCopies += 1;
        destination.publish (s, result);
        return result;
    }

    static void chain (ContextScope<false>& context, MusicalPreparationRequestState& s,
                       WorkerArtifacts& artifacts, WorkerLegacyResult& destination,
                       LegacyReadout& result) noexcept
    {
        Common commonValues;
        if (cancelled (context, s, result)) return;
        if (! common (s, commonValues, result)) return;
        if (! reserveQuery (s, commonValues, result)) return;
        {
            QueryAccounting::pay (s, Site::QueryWindowPreScope);
            FundedNativeSession native (s, result);
            if (! native.accepted_) return;
            if (! clock (context, s, artifacts, native, commonValues, result)) return;
            if (! window (context, s, artifacts, native, result)) return;
        }
        QueryAccounting::pay (s, Site::QueryWindowAfterScope);
        if ((result.outcome == LegacyOutcome::NativeQualificationFailed)
            | (result.outcome == LegacyOutcome::NativeRestoreFailed)
            | (result.outcome == LegacyOutcome::QueryReconciliationFailed)) return;
        if (! request (context, s, artifacts, commonValues, result)) return;
        if (! plan (context, s, artifacts, destination, commonValues, result)) return;
        fixed (s, Site::LegacySucceeded);
        result.outcome = LegacyOutcome::Ok;
    }

    static LegacyReadout run (ContextScope<false>& context, WorkerLegacyResult& destination) noexcept
    {
        auto& s = *context.state_;
        fixed (s, Site::LegacyEntry);
        if ((s.context_ != Context::WorkerRunning) | (s.captureStatus_ != CaptureStatus::RequestCaptured)
            | (! s.metadataLive_) | (! s.parentLive_) | (! s.legacyOptionsLive_)
            | s.legacyIssued_ | (destination.state_ != &s) | destination.complete_
            | destination.external_ | destination.taken_ | destination.ownsOrigin_) std::terminate();
        s.legacyIssued_ = true;
        LegacyReadout result {};
        result.stage = LegacyStage::None;
        {
            fixed (s, Site::LegacyArtifactsConstruct);
            WorkerArtifacts artifacts (s);
            chain (context, s, artifacts, destination, result);
            // Every local/staging owner physically drops, with debit BEFORE and
            // completion AFTER, while the genuine Worker context remains live.
        }
        return finish (s, destination, result);
    }
};

inline LegacyReadout ApplicationRequests::workerBuildLegacy (ControlRequestOwner& owner,
                                                            const std::atomic<bool>& cancel) noexcept
{
    auto context = owner.receiveWorker (cancel); // Real acquire precedes ledger access.
    auto& s = *context.state_;
    Ledger::pay (s, s.legacyFixed_, Site::LegacyWorkerCaller);
    if (! owner.resultLive_) std::terminate();
    return FundedLegacyStageAdapter::run (context, owner.result());
    // Existing ContextScope performs actual cleanup/drop/release publication.
    // R7 caller/return/tail actions have their separate fixed source reservation.
}

} // namespace yesdaw::app::musical_origin_source_r7
