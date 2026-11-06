#pragma once

// SOURCE ONLY / ALL DATA UNBOUND / NO EXECUTION ADMISSION.
#include "worker-result.h"
#include "native-profile.h"
#include <bit>

namespace yesdaw::app::musical_origin_source_r7 {
// Exact ordered identities of all190 pinned catalogue rows; no prices here.
enum class OldPacket : std::uint16_t
{
    E,
    Z,
    R_C,
    MAX,
    MATH1,
    EXT,
    VI,
    POSDIFF,
    BOOL_HANDLE,
    ARROW_HANDLE,
    HCOPY,
    HDROP,
    HDEFAULT,
    HMOVEASSIGN,
    HCOPYASSIGN,
    F0,
    F1,
    HF0,
    HF1,
    SR_LOCAL,
    STATUS,
    FB0,
    N_ENTER,
    N_FIN,
    N_NEG,
    N_RANGE,
    N_STORE,
    AS_ENTER,
    AS_SEG,
    AS_DELTA,
    AS_RANGE,
    AS_BPM_CALL,
    AS_ZERO,
    AS_MATH,
    AS_CURVE,
    AS_EQUAL,
    AS_LINEAR,
    AS_FINAL,
    AS_SLOPE,
    AS_SLOPE_ZERO,
    AS_FIN_TEST,
    AS_RELATIVE,
    AS_REL_RANGE,
    AS_LOGADD,
    UB_BASE,
    UB_TEST,
    UB_BODY,
    UB_TRUE,
    UB_FALSE,
    FT_ENTER,
    FT_RATE,
    FT_TICK,
    FT_EMPTY,
    FT_EMPTY_MATH,
    FT_SEARCH_LOCAL,
    FT_SEARCH_ADMIN,
    FT_PREFIX,
    FT_SEGMENT,
    FT_HAVE,
    FT_NEXT_TRUE,
    FT_NEXT_FALSE,
    FT_BPM_TRUE,
    FT_BPM_FALSE,
    FT_APPEND_CALLER,
    FT_STORE,
    FF_ENTER,
    FF_NEG,
    FF_FRACTION_CALL,
    FF_BUILT,
    FF_FAILURE_VALUE,
    FF_EMPTY,
    FF_VECEMPTY,
    FF_VECEMPTY_CALLER,
    FF_FRAMELOCAL,
    FF_FT_CALLER,
    FF_ZERO,
    FF_NATIVE_CALLER,
    FP_RESET,
    FA_ENTER,
    FA_WHOLE,
    FA_DENOM,
    FA_NUMERATOR,
    FA_ELIGIBILITY,
    FA_FRACTION_LOCALS,
    FA_FB_CALLER,
    FA_TIMES,
    FA_HIGH_ONE,
    FA_LOW_LOCAL,
    FA_TIME_CALLER,
    FA_STATUS_TEST,
    FA_STATUS_ARG,
    FA_HIGH_LOCAL,
    FA_EQUAL,
    FA_ORDER,
    FA_WIDTH,
    FA_FLOOR,
    FA_FLOOR_EQ,
    FA_OUTPUT_ARGS,
    WINDOW_VALUE_CTOR,
    WINDOW_PLAIN_CTOR,
    WINDOW_MOVE_ASSIGN,
    WINDOW_DTOR,
    WINDOW_RESET,
    RW_ENTER,
    RW_START,
    RW_LENGTH,
    RW_N,
    RW_OVERFLOW,
    RW_ENDPOINT_LOCALS,
    RW_A_CALLER,
    RW_A_ARGS,
    RW_B_CALLER,
    RW_B_ARGS,
    RW_STATUS,
    RW_RETURN_STATUS,
    RW_COLLAPSE,
    RW_SUCCESS_SCALARS,
    SR0,
    SR1,
    N_NONFINITE,
    N_NEGATIVE,
    N_OVERFLOW,
    N_OK,
    AS_BAD_SEG,
    AS_BAD_DELTA,
    AS_BAD_RANGE,
    AS_START0,
    AS_START1,
    AS_END0,
    AS_END1,
    AS_VALID_PREFIX,
    AS_ZERO_OK,
    AS_DISPATCH,
    AS_JUMP,
    AS_FLAT_RAMP,
    AS_RAMP_BASE,
    AS_SLOPE_ZERO_FAIL,
    AS_SLOPE_NONFINITE,
    AS_REL_BASE,
    AS_REL_NONFINITE,
    AS_REL_RANGE_FAIL,
    AS_RAMP,
    FT_RATE0_FAIL,
    FT_RATE1_FAIL,
    FT_VALID_RATE,
    FT_NEGATIVE,
    FT_VALID_BASE,
    FT_EMPTY_CASE,
    FT_NONEMPTY_LOCAL,
    FT_AT_ROW_LOCAL,
    FT_PART_NEXT,
    FT_PART_LAST,
    FF_NEGATIVE,
    FF_FRAC0_INVALID,
    FF_FRAC1_INVALID,
    FF_VALID_TIME,
    FF_UNBUILT,
    FF_BUILT_BASE,
    FF_INVALID_MAP,
    FF_EMPTY_BEFORE_FT,
    FF_MAP_BEFORE_FT,
    FF_AFTER_FT_OK,
    FA_BEFORE_WHOLE,
    FA_NEGATIVE,
    FA_INPUTS,
    FA_RESOURCE_LIMIT,
    FA_BEFORE_TIME,
    FA_FAILURE_TAIL,
    FA_OK_TAIL,
    RW_PRE_NULL,
    RW_EXIT,
    RW_NULL,
    RW_NEGATIVE,
    RW_NONPOS_LENGTH,
    RW_ZERO_N,
    RW_OVERFLOW_CASE,
    RW_BEFORE_A,
    RW_A_FAIL_TAIL,
    RW_B_LOCAL,
    RW_B_FAIL_TAIL,
    RW_COLLAPSED_TAIL,
    RW_SUCCESS_TAIL,
    UB_FIXED,
    UB_T,
    UB_F,
    RW_B_TOTAL,
    FF_EMPTY_OK_LOCAL,
    FF_MAP_OK_LOCAL,
    FA_EMPTY_OK,
    RW_EMPTY_OK,
    Count
};

struct ActionVector final { U64 cells[10]; };
class OldQueryCatalogue final
{
    friend class FundedWindowQuery;
    struct Data final
    {
        ActionVector rows[static_cast<std::size_t> (OldPacket::Count)];
        U64 appendCeiling, queryCeilingFixed, queryCeilingPerBit;
    };
    // NO definition. Must match every pinned190-row vector and the separately
    // reviewed ceilings exactly; it grants no native equivalence by itself.
    // Any future definition must be constinit const, not a runtime factory.
    static const Data data_;
    static const ActionVector& row (OldPacket id) noexcept
    { return data_.rows[static_cast<std::size_t> (id)]; }
};

class QueryAccounting final
{
    friend class FundedWindowQuery;
    friend class FundedNativeSession;
    friend class FundedLegacyStageAdapter;
    static void pay (MusicalPreparationRequestState& s, Site site) noexcept
    { Ledger::pay (s, s.legacyQuery_, site); }
    static void add (MusicalPreparationRequestState& s, ActionVector& into,
                     const ActionVector& from, U64 times = 1) noexcept
    {
        pay (s, Site::QueryVectorEntry);
        std::size_t i = 0;
        for (;;)
        {
            pay (s, Site::QueryVectorTest);
            if (i == 10) { pay (s, Site::QueryVectorEnd); return; }
            pay (s, Site::QueryVectorCell);
            into.cells[i] = Ledger::add (into.cells[i], Ledger::multiply (from.cells[i], times));
            pay (s, Site::QueryVectorContinue); ++i;
        }
    }
    static U64 sum (MusicalPreparationRequestState& s, const ActionVector& vector) noexcept
    {
        pay (s, Site::QuerySumEntry);
        U64 value = 0; std::size_t i = 0;
        for (;;)
        {
            pay (s, Site::QuerySumTest);
            if (i == 10) { pay (s, Site::QuerySumEnd); return value; }
            pay (s, Site::QuerySumCell);
            value = Ledger::add (value, vector.cells[i]);
            pay (s, Site::QuerySumContinue); ++i;
        }
    }
    static U64 prepareOld (MusicalPreparationRequestState& s, const ActionVector& vector) noexcept
    {
        pay (s, Site::QueryOldPrepare);
        const auto amount = sum (s, vector);
        pay (s, Site::QueryOldPrepared);
        const auto phase = Ledger::ix (s.legacyQuery_.phase);
        if ((amount > s.legacyQuery_.remaining) | (amount > s.ledger_.reserved[phase])
            | (amount > workCap) | (s.ledger_.actual[phase] > workCap - amount)
            | (s.queryCallee_.remaining != 0)) std::terminate();
        // Protect this exact future callee debit from all later new bookkeeping
        // and restoration payments. Split within the SAME held origin/phase.
        Ledger::split (s, s.legacyQuery_, s.queryCallee_, Phase::Context, amount);
        return amount;
    }
    static void record (MusicalPreparationRequestState& s, U64* destination,
                        const ActionVector& vector) noexcept
    {
        pay (s, Site::QueryRecordEntry); std::size_t i = 0;
        for (;;)
        {
            pay (s, Site::QueryRecordTest);
            if (i == 10) { pay (s, Site::QueryRecordEnd); return; }
            pay (s, Site::QueryRecordCell);
            destination[i] = Ledger::add (destination[i], vector.cells[i]);
            pay (s, Site::QueryRecordContinue); ++i;
        }
    }
    static void commitOld (MusicalPreparationRequestState& s, U64 amount) noexcept
    {
        // Branchless transfer only. Caller pays its complete commit+actual call
        // partition after all checks/arguments and immediately before this call.
        const auto phase = Ledger::ix (s.queryCallee_.phase);
        s.queryCallee_.remaining -= amount;
        s.ledger_.reserved[phase] -= amount;
        s.ledger_.actual[phase] += amount;
    }
};

class FundedNativeSession final
{
    friend class FundedLegacyStageAdapter;
    friend class FundedWindowQuery;
    MusicalPreparationRequestState* state_;
    LegacyReadout* result_;
    NativeSnapshot baseline_, replayStatus_;
    bool accepted_, restoring_;
    static void pay (MusicalPreparationRequestState& s, Site site) noexcept
    { QueryAccounting::pay (s, site); }
    bool reject (LegacyOutcome outcome) noexcept
    {
        auto& s = *state_; pay (s, Site::QueryNativeRejected);
        result_->outcome = outcome; accepted_ = false;
        s.raw_.legacy.queryPathEvidence = LegacyEvidence::Unavailable;
        s.raw_.legacy.extraAppendPathEvidence = LegacyEvidence::Unavailable;
        return false;
    }
    NativeSnapshot read() noexcept
    {
        auto& s = *state_; pay (s, Site::QueryNativeRead);
        const auto snapshot = NativeObserver::read();
        s.raw_.legacy.nativeSnapshots += 1;
        return snapshot;
    }
    bool matches (const NativeSnapshot& value) noexcept
    {
        auto& s = *state_; pay (s, Site::QueryNativeCompare);
        const auto& binding = NativeBinding::data_;
        return (value.kind == NativeKind::WindowsMsvcX64)
            & (value.compilerFull == binding.observerCompilerFull)
            & (value.controlError == 0) & (value.flagError == 0)
            & (value.mxcsrControl == binding.expectedMxcsrControl)
            & (value.crtControl == binding.expectedCrtControl)
            & (value.rounding == binding.expectedRounding)
            & (value.fma3Selection == binding.expectedFma3Selection)
            & (value.newHandler == binding.expectedNewHandler);
    }
    bool identity() noexcept
    {
        auto& s = *state_; pay (s, Site::QueryNativeIdentity);
        const auto actual = NativeBinding::actualComputeIdentity();
        const auto& expected = NativeBinding::data_.compute;
        if ((actual.kind != NativeKind::WindowsMsvcX64) | (actual.kind != expected.kind)
            | (actual.compilerFull != expected.compilerFull) | (actual.language != expected.language)
            | (actual.stlUpdate != expected.stlUpdate) | (actual.iteratorDebug != expected.iteratorDebug)
            | (actual.hardening != expected.hardening) | (actual.iteratorDebug != 0) | (actual.hardening != 0))
            return reject (LegacyOutcome::NativeQualificationFailed);
        // Finite completeness/identity check only. Reviewed private data must
        // already name real complete proofs, not merely arbitrary nonzero bytes.
        std::size_t i = 0;
        for (;;)
        {
            pay (s, Site::QueryNativeFactTest);
            if (i == static_cast<std::size_t> (NativeFact::Count))
            { pay (s, Site::QueryNativeFactEnd); return true; }
            pay (s, Site::QueryNativeFactRow);
            const auto& fact = NativeBinding::data_.facts[i];
            if (fact.kind != static_cast<NativeFact> (i))
                return reject (LegacyOutcome::NativeQualificationFailed);
            pay (s, Site::QueryNativeDigestEntry);
            std::uint8_t digestBits = 0; std::size_t byte = 0;
            for (;;)
            {
                pay (s, Site::QueryNativeDigestTest);
                if (byte == fact.evidenceDigest.size())
                { pay (s, Site::QueryNativeDigestEnd); break; }
                pay (s, Site::QueryNativeDigestByte);
                digestBits |= fact.evidenceDigest[byte]; ++byte;
            }
            pay (s, Site::QueryNativeDigestZeroTest);
            if (digestBits == 0) return reject (LegacyOutcome::NativeQualificationFailed);
            pay (s, Site::QueryNativeFactContinue); ++i;
        }
    }
    explicit FundedNativeSession (MusicalPreparationRequestState& s, LegacyReadout& result) noexcept
    {
        pay (s, Site::QueryNativeScope);
        state_ = &s; result_ = &result; accepted_ = false; restoring_ = false;
        if (! identity()) return;
        pay (s, Site::QueryNativeBaseline);
        baseline_ = read();
        if (! matches (baseline_)) { reject (LegacyOutcome::NativeQualificationFailed); return; }
        pay (s, Site::QueryNativeConstructComplete); accepted_ = true;
    }
    bool check() noexcept
    {
        auto& s = *state_; pay (s, Site::QueryNativeCheckEntry);
        const auto current = read();
        if (! matches (current)) return reject (LegacyOutcome::NativeQualificationFailed);
        pay (s, Site::QueryNativeAccepted);
        return accepted_;
    }
    bool beginReplay() noexcept
    {
        auto& s = *state_; pay (s, Site::QueryNativeBeginReplay);
        replayStatus_ = read();
        if (! matches (replayStatus_)) return reject (LegacyOutcome::NativeQualificationFailed);
        pay (s, Site::QueryNativeReplayReady); restoring_ = true;
        return true;
    }
    bool restore() noexcept
    {
        // No control-mode writes. Explicit restoration covers CRT exception
        // flags, all six raw MXCSR status bits and errno, under native proof.
        auto& s = *state_;
        if (! check()) return false;
        pay (s, Site::QueryNativeRestore);
        const bool restored = NativeObserver::restoreStatus (replayStatus_);
        s.raw_.legacy.nativeStatusRestorations += 1;
        restoring_ = false;
        pay (s, Site::QueryNativeRestoreResult);
        if (! restored) return reject (LegacyOutcome::NativeRestoreFailed);
        pay (s, Site::QueryNativeRestoreVerify);
        const auto after = read();
        if (! matches (after)) return reject (LegacyOutcome::NativeQualificationFailed);
        if ((after.mxcsrStatus != replayStatus_.mxcsrStatus)
            | (after.exceptions != replayStatus_.exceptions) | (after.errorNumber != replayStatus_.errorNumber))
            return reject (LegacyOutcome::NativeRestoreFailed);
        pay (s, Site::QueryNativeRestoreReady); return accepted_;
    }
public:
    FundedNativeSession (const FundedNativeSession&) = delete;
    FundedNativeSession (FundedNativeSession&&) = delete;
    ~FundedNativeSession() noexcept
    {
        auto& s = *state_; pay (s, Site::QueryNativeScopeDestroy);
        if (restoring_) restore();
        // After the real resolve, restoring_ is false. Preserve its own status
        // effects; no whole native/x87 preservation is claimed by this source.
    }
};

struct QueryCallResult final { bool invoked; engine::MusicalAudioStatus status; };

class FundedWindowQuery final
{
    friend class FundedLegacyStageAdapter;
    using Status = engine::MusicalAudioStatus;
    using Q = QueryAccounting;
    struct Search final { U64 index, t, f; };
    struct AppendArguments final
    {
        double rate; engine::Tick segment, delta; double startBpm, endBpm; engine::TempoCurve curve;
    };
    struct Endpoint final
    {
        Status status; double frame; std::int64_t floor; ActionVector path;
    };
    struct Prediction final
    {
        Status status; std::int64_t startFrame, endFrame; ActionVector path;
    };
    static void packet (MusicalPreparationRequestState& s, ActionVector& into,
                        OldPacket id, U64 times = 1) noexcept
    {
        Q::pay (s, Site::QueryPacket);
        Q::add (s, into, OldQueryCatalogue::row (id), times);
    }
    static bool quoteTerm (MusicalPreparationRequestState& s, U64 unit, U64 count, U64& total) noexcept
    {
        Ledger::pay (s, s.legacyFixed_, Site::QueryQuoteTerm);
        if (total > workCap) std::terminate();
        const auto divisor = count | static_cast<U64> (count == 0);
        if ((count != 0) & (unit > (workCap - total) / divisor))
        { Ledger::pay (s, s.legacyFixed_, Site::QueryQuoteOverflow); return false; }
        Ledger::pay (s, s.legacyFixed_, Site::QueryQuoteReady);
        total += unit * count;
        return true;
    }
    static bool quote (MusicalPreparationRequestState& s, U64 rows, U64& amount) noexcept
    {
        Ledger::pay (s, s.legacyFixed_, Site::QueryQuote);
        const auto& b = Ledger::bounds();
        const auto bits = static_cast<U64> (std::bit_width (static_cast<std::size_t> (rows)));
        const auto prefix = rows == 0 ? 0 : rows - 1;
        const auto append = rows == 0 ? 0 : rows + 1; // at most R-1 full +2 partial.
        amount = 0;
        // Execute all six finite quote terms even after one refuses. Each
        // refused term leaves total unchanged; no short-circuit caller work is
        // hidden in a maximum. The fixed quote source is backed by root funds.
        const bool a = quoteTerm (s, b.pendingWindowQueryFixed, 1, amount);
        const bool b1 = quoteTerm (s, b.pendingWindowQueryPerPrefix, prefix, amount);
        const bool c = quoteTerm (s, b.pendingWindowQueryPerSearch, 2 * bits, amount);
        const bool d = quoteTerm (s, OldQueryCatalogue::data_.appendCeiling, append, amount);
        const bool e = quoteTerm (s, OldQueryCatalogue::data_.queryCeilingFixed, 1, amount);
        const bool f = quoteTerm (s, OldQueryCatalogue::data_.queryCeilingPerBit, bits, amount);
        return a & b1 & c & d & e & f;
    }
    static Search search (MusicalPreparationRequestState& s, engine::Tick tick) noexcept
    {
        Q::pay (s, Site::QuerySearchEntry);
        Search found {}; U64 first = 0, count = s.mapConstructed_;
        for (;;)
        {
            Q::pay (s, Site::QuerySearchTest);
            if (count == 0)
            {
                Q::pay (s, Site::QuerySearchEnd);
                if ((first == 0) | (first > s.mapConstructed_)) std::terminate();
                found.index = first - 1;
                return found;
            }
            Q::pay (s, Site::QuerySearchRow);
            const auto half = count / 2; const auto mid = first + half;
            if (tick < s.map_[mid].tick)
            {
                Q::pay (s, Site::QuerySearchTrue);
                count = half; found.t += 1; s.raw_.legacy.derivedSearchTrue += 1;
            }
            else
            {
                Q::pay (s, Site::QuerySearchFalse);
                first = mid + 1; count -= half + 1; found.f += 1; s.raw_.legacy.derivedSearchFalse += 1;
            }
        }
    }
    static OldPacket classifyAppend (MusicalPreparationRequestState& s,
                                      const AppendArguments& a, bool full) noexcept
    {
        Q::pay (s, Site::QueryAppendClassify);
        if ((a.segment <= 0) | (a.delta <= 0) | (a.delta > a.segment)) std::terminate();
        // All old BPM guard results follow immutable validated input and the
        // qualified original Clock. No old helper is called recursively here.
        if (a.curve != engine::TempoCurve::LinearRamp)
        { Q::pay (s, Site::QueryAppendJump); return OldPacket::AS_JUMP; }
        Q::pay (s, Site::QueryAppendEqualTest);
        if (a.startBpm == a.endBpm)
        { Q::pay (s, Site::QueryAppendEqual); return OldPacket::AS_FLAT_RAMP; }
        Q::pay (s, Site::QueryAppendFullTest);
        if (full)
        { Q::pay (s, Site::QueryAppendFullRamp); return OldPacket::AS_RAMP; }
        Q::pay (s, Site::QueryAppendSlope);
        const double slope = (a.endBpm - a.startBpm) / static_cast<double> (a.segment);
        if (slope == 0.0)
        { Q::pay (s, Site::QueryAppendSlopeZero); return OldPacket::AS_SLOPE_ZERO_FAIL; }
        Q::pay (s, Site::QueryAppendSlopeFinite);
        if (! std::isfinite (slope))
        { Q::pay (s, Site::QueryAppendSlopeBad); return OldPacket::AS_SLOPE_NONFINITE; }
        Q::pay (s, Site::QueryAppendRelative);
        const double delta = static_cast<double> (a.delta);
        const double relativeChange = slope * delta / a.startBpm;
        if (! std::isfinite (relativeChange))
        { Q::pay (s, Site::QueryAppendRelativeBad); return OldPacket::AS_REL_NONFINITE; }
        Q::pay (s, Site::QueryAppendRelativeRange);
        if (relativeChange <= -1.0)
        { Q::pay (s, Site::QueryAppendRangeBad); return OldPacket::AS_REL_RANGE_FAIL; }
        Q::pay (s, Site::QueryAppendRamp); return OldPacket::AS_RAMP;
    }
    static bool append (MusicalPreparationRequestState& s, FundedNativeSession& native,
                        const AppendArguments& a, bool full, double& frame,
                        OldPacket& selected, bool& returned) noexcept
    {
        if (! native.check()) return false;
        selected = classifyAppend (s, a, full);
        if (! native.check()) return false;
        const auto amount = Q::prepareOld (s, OldQueryCatalogue::row (selected));
        // All argument values/references are prepared; frame is a real live
        // scalar. NO branch/check/cancel/callback between commit and old entry.
        Q::pay (s, Site::QueryOldAppendInvoke);
        Q::commitOld (s, amount);
        returned = engine::detail::appendTempoSegmentFrames (
            a.rate, a.segment, a.delta, a.startBpm, a.endBpm, a.curve, frame);
        Q::pay (s, Site::QueryOldAppendReturned);
        s.raw_.legacy.extraAppendCalls += 1;
        s.raw_.legacy.extraAppendDerivedUnits += amount;
        Q::record (s, s.raw_.legacy.extraAppendDerivedVector, OldQueryCatalogue::row (selected));
        s.raw_.legacy.extraAppendPathEvidence = LegacyEvidence::SourceDerived;
        if (full)
        { Q::pay (s, Site::QueryOldPrefixReturned); s.raw_.legacy.prefixReplayCalls += 1; }
        else
        { Q::pay (s, Site::QueryOldPartialReturned); s.raw_.legacy.partialReplayCalls += 1; }
        return native.check();
    }
    static bool prefixTo (MusicalPreparationRequestState& s, FundedNativeSession& native,
                          U64 target, U64& at, double& prefix, double rate) noexcept
    {
        Q::pay (s, Site::QueryPrefixEntry);
        if (target < at) std::terminate();
        for (;;)
        {
            Q::pay (s, Site::QueryPrefixTest);
            if (at == target) { Q::pay (s, Site::QueryPrefixEnd); return true; }
            Q::pay (s, Site::QueryPrefixRow);
            const auto next = at + 1;
            const auto& first = s.map_[at]; const auto& second = s.map_[next];
            const auto segment = second.tick - first.tick;
            const AppendArguments a { rate, segment, segment, first.bpm, second.bpm, first.curveToNext };
            double frame = prefix; OldPacket selected; bool returned;
            if (! append (s, native, a, true, frame, selected, returned)) return false;
            if (! returned)
            {
                Q::pay (s, Site::QueryPrefixFailed);
                return native.reject (LegacyOutcome::NativeQualificationFailed);
            }
            // Matches actual build's named frame and binary64 stored prefix
            // recurrence under the UNBOUND codegen/store forward proof.
            Q::pay (s, Site::QueryPrefixContinue); prefix = frame; at = next;
        }
    }
    static void finishEndpoint (MusicalPreparationRequestState& s, Endpoint& out,
                                bool empty, bool frameForTickReturned) noexcept
    {
        Q::pay (s, Site::QueryEndpointAfterFt);
        ActionVector compiled {};
        packet (s, compiled, empty ? OldPacket::FF_EMPTY_BEFORE_FT : OldPacket::FF_MAP_BEFORE_FT);
        Q::add (s, compiled, out.path);
        if (! frameForTickReturned)
        {
            Q::pay (s, Site::QueryEndpointFtFailed);
            packet (s, compiled, OldPacket::Z);
            out.status = Status::NumericDomain;
            out.path = {}; packet (s, out.path, OldPacket::FA_BEFORE_TIME);
            Q::add (s, out.path, compiled); packet (s, out.path, OldPacket::FA_FAILURE_TAIL);
            return;
        }
        Q::pay (s, Site::QueryEndpointNative);
        packet (s, compiled, OldPacket::FF_AFTER_FT_OK);
        if (! std::isfinite (out.frame))
        {
            Q::pay (s, Site::QueryEndpointNonfinite);
            packet (s, compiled, OldPacket::N_NONFINITE); out.status = Status::NumericDomain;
        }
        else
        {
            Q::pay (s, Site::QueryEndpointNegativeTest);
            if (out.frame < 0.0)
            {
                Q::pay (s, Site::QueryEndpointNegative);
                packet (s, compiled, OldPacket::N_NEGATIVE); out.status = Status::NumericDomain;
            }
            else
            {
                Q::pay (s, Site::QueryEndpointRangeTest);
                if (out.frame >= 0x1p63)
                {
                    Q::pay (s, Site::QueryEndpointOverflow);
                    packet (s, compiled, OldPacket::N_OVERFLOW); out.status = Status::Overflow;
                }
                else
                {
                    Q::pay (s, Site::QueryEndpointOk);
                    packet (s, compiled, OldPacket::N_OK); out.status = Status::Ok;
                    out.floor = static_cast<std::int64_t> (std::floor (out.frame));
                }
            }
        }
        out.path = {}; packet (s, out.path, OldPacket::FA_BEFORE_TIME);
        Q::add (s, out.path, compiled);
        packet (s, out.path, out.status == Status::Ok ? OldPacket::FA_OK_TAIL : OldPacket::FA_FAILURE_TAIL);
    }
    static bool endpoint (MusicalPreparationRequestState& s, FundedNativeSession& native,
                          engine::Tick tick, U64& at, double& prefix,
                          bool eligible, double rate, Endpoint& out) noexcept
    {
        Q::pay (s, Site::QueryEndpointEntry);
        out = {};
        if (! eligible)
        {
            Q::pay (s, Site::QueryEndpointResource);
            packet (s, out.path, OldPacket::FA_RESOURCE_LIMIT); out.status = Status::ResourceLimit;
            return true;
        }
        if (s.mapConstructed_ == 0)
        {
            Q::pay (s, Site::QueryEndpointEmpty);
            if (! native.check()) return false;
            // The existing empty formula, in its original operation order.
            out.frame = static_cast<double> (tick)
                * (60.0 * rate / (120.0 * static_cast<double> (engine::kTicksPerQuarter)));
            packet (s, out.path, OldPacket::FT_EMPTY_CASE);
            finishEndpoint (s, out, true, std::isfinite (out.frame));
            return native.check();
        }
        Q::pay (s, Site::QueryEndpointMap);
        const auto found = search (s, tick);
        if (! prefixTo (s, native, found.index, at, prefix, rate)) return false;
        packet (s, out.path, OldPacket::UB_FIXED);
        packet (s, out.path, OldPacket::UB_T, found.t); packet (s, out.path, OldPacket::UB_F, found.f);
        out.frame = prefix;
        if (tick > s.map_[found.index].tick)
        {
            Q::pay (s, Site::QueryEndpointPartial);
            const auto& row = s.map_[found.index];
            const bool next = found.index + 1 < s.mapConstructed_;
            AppendArguments a;
            if (next)
            {
                Q::pay (s, Site::QueryPartialNextArgs);
                const auto& second = s.map_[found.index + 1];
                a = { rate, second.tick - row.tick, tick - row.tick, row.bpm, second.bpm, row.curveToNext };
            }
            else
            {
                Q::pay (s, Site::QueryPartialLastArgs);
                a = { rate, tick - row.tick, tick - row.tick, row.bpm, row.bpm, row.curveToNext };
            }
            OldPacket selected; bool returned;
            if (! append (s, native, a, false, out.frame, selected, returned)) return false;
            packet (s, out.path, OldPacket::FT_NONEMPTY_LOCAL);
            packet (s, out.path, next ? OldPacket::FT_PART_NEXT : OldPacket::FT_PART_LAST);
            packet (s, out.path, selected);
            packet (s, out.path, returned ? OldPacket::FT_STORE : OldPacket::Z);
            finishEndpoint (s, out, false, returned && std::isfinite (out.frame));
            return native.check();
        }
        Q::pay (s, Site::QueryEndpointAtRow);
        packet (s, out.path, OldPacket::FT_AT_ROW_LOCAL);
        finishEndpoint (s, out, false, std::isfinite (out.frame));
        return native.check();
    }
    static bool predict (MusicalPreparationRequestState& s, FundedNativeSession& native,
                         const std::shared_ptr<const engine::MusicalAudioClock>& clock,
                         engine::Tick start, engine::Tick length, U64 frames, Prediction& predicted) noexcept
    {
        Q::pay (s, Site::QueryPredictEntry);
        predicted = {};
        if (! clock)
        { Q::pay (s, Site::QueryScalarNull); packet (s, predicted.path, OldPacket::RW_NULL); predicted.status = Status::InvalidClock; return true; }
        Q::pay (s, Site::QueryPredictionScalarChecks);
        if (start < 0)
        { Q::pay (s, Site::QueryScalarNegative); packet (s, predicted.path, OldPacket::RW_NEGATIVE); predicted.status = Status::InvalidInput; return true; }
        Q::pay (s, Site::QueryPredictionLengthCheck);
        if (length <= 0)
        { Q::pay (s, Site::QueryScalarLength); packet (s, predicted.path, OldPacket::RW_NONPOS_LENGTH); predicted.status = Status::InvalidInput; return true; }
        Q::pay (s, Site::QueryPredictionNCheck);
        if (frames == 0)
        { Q::pay (s, Site::QueryScalarN); packet (s, predicted.path, OldPacket::RW_ZERO_N); predicted.status = Status::InvalidInput; return true; }
        Q::pay (s, Site::QueryPredictionOverflowCheck);
        if (length > std::numeric_limits<engine::Tick>::max() - start)
        { Q::pay (s, Site::QueryScalarOverflow); packet (s, predicted.path, OldPacket::RW_OVERFLOW_CASE); predicted.status = Status::Overflow; return true; }
        Q::pay (s, Site::QueryPredictionEndpoints);
        U64 at = 0; double prefix = 0.0;
        const auto threshold = U64 { 512 } + U64 { 2 } * static_cast<U64> (std::bit_width (clock->rows().size()));
        const bool eligible = clock->limits().maxWorkUnits >= threshold;
        const auto rate = s.metadata().projectRate.hz;
        Endpoint a {}, b {};
        packet (s, predicted.path, OldPacket::RW_BEFORE_A);
        if (! endpoint (s, native, start, at, prefix, eligible, rate, a)) return false;
        Q::add (s, predicted.path, a.path);
        if (a.status != Status::Ok)
        { Q::pay (s, Site::QueryPredictionAFailed); packet (s, predicted.path, OldPacket::RW_A_FAIL_TAIL); predicted.status = a.status; return true; }
        Q::pay (s, Site::QueryPredictionB);
        packet (s, predicted.path, OldPacket::RW_B_LOCAL);
        if (! endpoint (s, native, start + length, at, prefix, eligible, rate, b)) return false;
        Q::add (s, predicted.path, b.path);
        if (b.status != Status::Ok)
        { Q::pay (s, Site::QueryPredictionBFailed); packet (s, predicted.path, OldPacket::RW_B_FAIL_TAIL); predicted.status = b.status; return true; }
        if (b.floor <= a.floor)
        { Q::pay (s, Site::QueryPredictionCollapsed); packet (s, predicted.path, OldPacket::RW_COLLAPSED_TAIL); predicted.status = Status::WindowCollapsed; return true; }
        Q::pay (s, Site::QueryPredictionOk);
        packet (s, predicted.path, OldPacket::RW_SUCCESS_TAIL);
        predicted.status = Status::Ok; predicted.startFrame = a.floor; predicted.endFrame = b.floor;
        return true;
    }
    static bool reconcile (MusicalPreparationRequestState& s, const Prediction& predicted, Status status,
                           const std::shared_ptr<const engine::MusicalAudioClock>& clock,
                           engine::Tick start, engine::Tick length, U64 frames,
                           const engine::MusicalAudioWindow& out) noexcept
    {
        Q::pay (s, Site::QueryReconcileEntry);
        if (status != predicted.status) { Q::pay (s, Site::QueryReconcileFailure); return false; }
        Q::pay (s, Site::QueryReconcileStatus);
        if (status == Status::Ok)
        {
            Q::pay (s, Site::QueryReconcileSuccess);
            return out.valid() & (out.clock().get() == clock.get())
                & ! out.clock().owner_before (clock) & ! clock.owner_before (out.clock())
                & (out.startTick() == start) & (out.lengthTicks() == length) & (out.inputFrames() == frames)
                & (out.startFrame() == predicted.startFrame) & (out.endFrame() == predicted.endFrame);
        }
        Q::pay (s, Site::QueryReconcileEmpty);
        return ! out.valid() & (out.clock().get() == nullptr) & (out.startTick() == 0)
            & (out.lengthTicks() == 0) & (out.inputFrames() == 0) & (out.startFrame() == 0) & (out.endFrame() == 0);
    }
    static QueryCallResult resolve (MusicalPreparationRequestState& s, FundedNativeSession& native,
                                    const std::shared_ptr<const engine::MusicalAudioClock>& clock,
                                    engine::MusicalAudioWindow& out, LegacyReadout& result) noexcept
    {
        Q::pay (s, Site::QueryEntry);
        if (! native.beginReplay()) { Q::pay (s, Site::QueryNoInvocation); return { false, Status::InvalidInput }; }
        const auto start = s.metadata().clip.timelineStart; const auto length = s.metadata().clip.timelineLength;
        const auto frames = s.legacyOptions().mappedWindow.srcLen;
        Prediction predicted {};
        if (! predict (s, native, clock, start, length, frames, predicted))
        { Q::pay (s, Site::QueryNoInvocation); return { false, Status::InvalidInput }; }
        Q::pay (s, Site::QueryResolvePrepare);
        const auto amount = Q::prepareOld (s, predicted.path);
        if (! native.restore()) { Q::pay (s, Site::QueryNoInvocation); return { false, Status::InvalidInput }; }
        // All preparation/restoration/control checks precede the exact debit.
        // Commit uses branchless scalar transfers, then ONE guaranteed real
        // resolve call with prepared nonthrowing scalar/reference arguments.
        Q::pay (s, Site::QueryResolveCommitInvoke);
        Q::commitOld (s, amount);
        const auto status = engine::resolveMusicalAudioWindow (clock, start, length, frames, out);
        Q::pay (s, Site::QueryResolveReturned);
        s.raw_.legacy.queryCalls += 1; s.raw_.legacy.queryDerivedUnits += amount;
        Q::record (s, s.raw_.legacy.queryDerivedVector, predicted.path);
        s.raw_.legacy.queryPathEvidence = LegacyEvidence::SourceDerived;
        // Do not restore pre-query flags/errno after the real old call.
        const bool nativeOk = native.check();
        const bool equal = reconcile (s, predicted, status, clock, start, length, frames, out);
        if ((! nativeOk) | (! equal))
        {
            Q::pay (s, Site::QueryReconcileMismatch);
            s.raw_.legacy.queryPathEvidence = LegacyEvidence::Unavailable;
            result.outcome = LegacyOutcome::QueryReconciliationFailed;
            return { true, status };
        }
        Q::pay (s, Site::QueryReconcileOk);
        return { true, status };
    }
};

} // namespace yesdaw::app::musical_origin_source_r7
