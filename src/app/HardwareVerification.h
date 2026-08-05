// YES DAW — packaged hardware verification contract (H17 packaged verifier, U1).
//
// One place for schema v1: stage states, claim levels, the stable failure-code registry,
// child-document acceptance, the pass-record consistency policy, and the KTD11 aggregate verdict
// (any measured fail -> exit 1; else any setup/crash/skipped -> exit 2; only all-pass -> exit 0).
//
// The C++ checkers (U2-U4) emit stage documents through this header. tools/verify-hardware.ps1
// reimplements the same acceptance/normalization/aggregation policy in PowerShell for the packaged
// orchestrator; both replay the committed fixtures in tests/fixtures/hardware-verification/ (the
// Catch2 target here, the script via -SelfTest), and both assert the exact same fixture set, so the
// two implementations cannot drift silently. Change policy here => update the script + fixtures in
// the same commit.
//
// Deliberately NOT part of YesDawSelfCheck: that contract stays device/display-free (KTD2).
// Plan: docs/plans/2026-07-28-h17-packaged-hardware-verifier-plan.md (KTD2, KTD3, KTD10, KTD11).

#pragma once

#include <juce_core/juce_core.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace yesdaw::app::hardware {

inline constexpr int kSchemaVersion = 1;

// --- Stage names (the three packaged stages; the orchestrator always runs all three) ------------
inline constexpr const char* kStagePlayback  = "playback";
inline constexpr const char* kStageRecording = "recording";
inline constexpr const char* kStageFrame     = "frame";

// --- Claim levels (what a passing stage is allowed to assert; R14, R17, R19) --------------------
inline constexpr const char* kClaimLockedPlayback        = "locked_playback";
inline constexpr const char* kClaimFullAlignment         = "full_alignment";
inline constexpr const char* kClaimCaptureOnly           = "capture_only";
inline constexpr const char* kClaimHeadlessDenseTimeline = "headless_dense_timeline";

// --- Measurement keys the verdict policy reads (checkers may add more; readers ignore unknowns) --
inline constexpr const char* kMeasGrantedSampleRateHz = "granted_sample_rate_hz";
inline constexpr const char* kMeasGrantedBlockFrames  = "granted_block_frames";
inline constexpr const char* kMeasAlignmentStatus     = "alignment_status";
inline constexpr const char* kMeasAlignmentFrames     = "alignment_frames";
inline constexpr const char* kMeasInputRoute          = "input_route";

inline constexpr const char* kAlignmentClaimed    = "claimed";
inline constexpr const char* kAlignmentNotClaimed = "not_claimed";
inline constexpr const char* kRouteDeviceLoopback = "device_loopback";

// --- The locked playback gate (ADR-0037 / H17; the verifier never revises these) ----------------
inline constexpr double kRequiredSampleRateHz  = 48000.0;
inline constexpr int    kMaxGrantedBlockFrames = 128;

// --- Stable failure codes (additive registry; renaming/removing one is a schema change) ---------
// Orchestrator/child boundary:
inline constexpr const char* kFailureChildCrash           = "child_crash";
inline constexpr const char* kFailureChildTimeout         = "child_timeout";
inline constexpr const char* kFailureChildMissingResult   = "child_missing_result";
inline constexpr const char* kFailureChildInvalidResult   = "child_invalid_result";
inline constexpr const char* kFailureChildWrongSchema     = "child_wrong_schema";
inline constexpr const char* kFailureChildWrongRunId      = "child_wrong_run_id";
inline constexpr const char* kFailureChildWrongStage      = "child_wrong_stage";
inline constexpr const char* kFailureChildVersionMismatch = "child_version_mismatch";
// Verdict policy:
inline constexpr const char* kFailureClaimInvalid         = "claim_invalid";
inline constexpr const char* kFailureAggregateIncomplete  = "aggregate_incomplete";
inline constexpr const char* kFailureDeviceUnavailable    = "device_unavailable";
inline constexpr const char* kFailurePlaybackEvidenceMissing   = "playback_evidence_missing";
inline constexpr const char* kFailurePlaybackWrongSampleRate   = "playback_wrong_sample_rate";
inline constexpr const char* kFailurePlaybackBlockExceedsTarget = "playback_block_exceeds_target";
inline constexpr const char* kFailurePlaybackXrun               = "playback_xrun";
inline constexpr const char* kFailurePlaybackDeviceError        = "playback_device_error";
inline constexpr const char* kFailurePlaybackSilentOutput       = "playback_silent_output";
inline constexpr const char* kFailurePlaybackCallbackBudget     = "playback_callback_budget";
inline constexpr const char* kFailureRecordingInventedAlignment = "recording_invented_alignment";
inline constexpr const char* kFailureRecordingAlignmentUnproved = "recording_alignment_unproved";
inline constexpr const char* kFailureFrameClaimMismatch         = "frame_claim_mismatch";
inline constexpr const char* kFailureFrameBlankOutput           = "frame_blank_output";
inline constexpr const char* kFailureFrameInsufficientDensity   = "frame_insufficient_density";
inline constexpr const char* kFailureFrameCapacityExceeded      = "frame_capacity_exceeded";
inline constexpr const char* kFailureFrameOverBudget            = "frame_over_budget";

// --- Stage states (KTD11). A child may only report pass/fail/setup about itself; crash and -------
// skipped are synthesized by the orchestrator (timeout, abnormal exit, untrustworthy output).
enum class StageState { pass, fail, setup, crash, skipped };

[[nodiscard]] inline const char* toString (StageState s)
{
    switch (s)
    {
        case StageState::pass:    return "pass";
        case StageState::fail:    return "fail";
        case StageState::setup:   return "setup";
        case StageState::crash:   return "crash";
        case StageState::skipped: return "skipped";
    }
    return "setup";
}

[[nodiscard]] inline std::optional<StageState> stageStateFromString (const juce::String& s)
{
    if (s == "pass")    return StageState::pass;
    if (s == "fail")    return StageState::fail;
    if (s == "setup")   return StageState::setup;
    if (s == "crash")   return StageState::crash;
    if (s == "skipped") return StageState::skipped;
    return std::nullopt;
}

// --- Overall verdict (aggregate precedence; exit-code contract R10) ------------------------------
enum class OverallState { pass, fail, setup };

[[nodiscard]] inline const char* toString (OverallState s)
{
    switch (s)
    {
        case OverallState::pass:  return "pass";
        case OverallState::fail:  return "fail";
        case OverallState::setup: return "setup";
    }
    return "setup";
}

[[nodiscard]] inline int exitCodeFor (OverallState s)
{
    switch (s)
    {
        case OverallState::pass:  return 0;
        case OverallState::fail:  return 1;   // at least one COMPLETED measurement violated a gate
        case OverallState::setup: return 2;   // no measured failure, but no complete verdict either
    }
    return 2;
}

// --- One stage's record, after acceptance (or synthesis) -----------------------------------------
struct StageRecord
{
    juce::String      stage;                       // kStagePlayback / kStageRecording / kStageFrame
    StageState        state = StageState::setup;
    juce::String      claimLevel;                  // empty unless the stage passes
    juce::String      startedAt;                   // ISO-8601 UTC; empty on synthesized records
    juce::String      completedAt;
    double            durationMs = 0.0;
    juce::var         measurements;                // JSON object; requested vs granted kept distinct
    juce::StringArray failureCodes;
    juce::String      detail;                      // bounded human text, never parsed for policy
    juce::String      checkerVersion;
};

struct ChildExpectation
{
    juce::String runId;
    juce::String stage;
    juce::String checkerVersion;
};

// --- JSON helpers (locale-invariant via juce::JSON; never printf-formatted) ----------------------
[[nodiscard]] inline juce::String toJsonText (const juce::var& value)
{
    return juce::JSON::toString (value);
}

[[nodiscard]] inline std::optional<juce::var> parseJsonText (const juce::String& text)
{
    juce::var parsed;
    if (juce::JSON::parse (text, parsed).wasOk())
        return parsed;
    return std::nullopt;
}

// Atomic replacement (KTD3): write a sibling temp file, then rename over the target. rename()
// replaces an existing file on the same volume on both POSIX and Windows, so a reader never sees
// a half-written document.
[[nodiscard]] inline bool writeJsonAtomically (const std::filesystem::path& target,
                                               const juce::var& value,
                                               std::string& error)
{
    const std::filesystem::path temp { target.string() + ".tmp" };
    {
        std::ofstream out (temp, std::ios::binary | std::ios::trunc);
        if (! out.is_open())
        {
            error = "could not open temp file for writing: " + temp.string();
            return false;
        }
        const juce::String text = toJsonText (value);
        out.write (text.toRawUTF8(), static_cast<std::streamsize> (text.getNumBytesAsUTF8()));
        out.flush();
        if (! out.good())
        {
            error = "write to temp file failed: " + temp.string();
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename (temp, target, ec);
    if (ec)
    {
        error = "atomic rename failed: " + ec.message();
        std::filesystem::remove (temp, ec);
        return false;
    }
    error.clear();
    return true;
}

// --- Child stage document (what a checker writes; the envelope carries run identity) -------------
[[nodiscard]] inline juce::var stageDocumentToVar (const StageRecord& r, const juce::String& runId)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("schema_version", kSchemaVersion);
    obj->setProperty ("run_id", runId);
    obj->setProperty ("stage", r.stage);
    obj->setProperty ("checker_version", r.checkerVersion);
    obj->setProperty ("state", juce::String { toString (r.state) });
    obj->setProperty ("claim_level", r.claimLevel);
    obj->setProperty ("started_at", r.startedAt);
    obj->setProperty ("completed_at", r.completedAt);
    obj->setProperty ("duration_ms", r.durationMs);
    obj->setProperty ("measurements", r.measurements.isObject() ? r.measurements
                                                                : juce::var { new juce::DynamicObject() });
    juce::Array<juce::var> codes;
    for (const auto& code : r.failureCodes)
        codes.add (code);
    obj->setProperty ("failure_codes", codes);
    obj->setProperty ("detail", r.detail);
    return juce::var { obj };
}

namespace detail {

[[nodiscard]] inline bool isJsonNumber (const juce::var& v)
{
    return v.isDouble() || v.isInt() || v.isInt64();
}

[[nodiscard]] inline bool getFiniteNumber (const juce::var& object, const char* key, double& out)
{
    if (! object.isObject() || ! object.hasProperty (key))
        return false;
    const juce::var v = object[key];
    if (! isJsonNumber (v))
        return false;
    out = static_cast<double> (v);
    return std::isfinite (out);
}

[[nodiscard]] inline juce::String getStringOrEmpty (const juce::var& object, const char* key)
{
    if (object.isObject() && object.hasProperty (key) && object[key].isString())
        return object[key].toString();
    return {};
}

} // namespace detail

// Orchestrator-side synthesis for a child that produced no trustworthy result (crash, timeout,
// missing/invalid output). Skipped stages synthesize with no failure code.
[[nodiscard]] inline StageRecord synthesizeTerminalRecord (const juce::String& stage,
                                                           StageState state,
                                                           const juce::String& failureCode,
                                                           const juce::String& detail)
{
    StageRecord r;
    r.stage = stage;
    r.state = state;
    r.measurements = juce::var { new juce::DynamicObject() };
    if (failureCode.isNotEmpty())
        r.failureCodes.add (failureCode);
    r.detail = detail;
    return r;
}

// Child output is accepted only when its schema, run ID, stage name, and checker version match the
// invocation (Failure and Durability Policy). Anything else becomes a synthesized crash record with
// a named code. Validation order is part of the contract — tools/verify-hardware.ps1 mirrors it.
[[nodiscard]] inline StageRecord acceptStageDocument (const juce::var& document,
                                                      const ChildExpectation& expected)
{
    const auto reject = [&expected] (const char* code, const juce::String& why)
    {
        return synthesizeTerminalRecord (expected.stage, StageState::crash, code, why);
    };

    if (! document.isObject())
        return reject (kFailureChildInvalidResult, "child result is not a JSON object");

    if (! document.hasProperty ("schema_version") || ! detail::isJsonNumber (document["schema_version"]))
        return reject (kFailureChildInvalidResult, "child result has no numeric schema_version");
    if (static_cast<int> (document["schema_version"]) != kSchemaVersion)
        return reject (kFailureChildWrongSchema,
                       "child schema_version is not " + juce::String { kSchemaVersion });

    if (detail::getStringOrEmpty (document, "run_id").isEmpty())
        return reject (kFailureChildInvalidResult, "child result has no run_id");
    if (detail::getStringOrEmpty (document, "run_id") != expected.runId)
        return reject (kFailureChildWrongRunId, "child run_id does not match this invocation");

    if (detail::getStringOrEmpty (document, "stage").isEmpty())
        return reject (kFailureChildInvalidResult, "child result has no stage name");
    if (detail::getStringOrEmpty (document, "stage") != expected.stage)
        return reject (kFailureChildWrongStage, "child stage does not match this invocation");

    if (detail::getStringOrEmpty (document, "checker_version").isEmpty())
        return reject (kFailureChildInvalidResult, "child result has no checker_version");
    if (detail::getStringOrEmpty (document, "checker_version") != expected.checkerVersion)
        return reject (kFailureChildVersionMismatch, "child checker_version does not match the package");

    const auto state = stageStateFromString (detail::getStringOrEmpty (document, "state"));
    // A child may never report crash/skipped about itself — those are orchestrator judgments.
    if (! state.has_value()
        || (*state != StageState::pass && *state != StageState::fail && *state != StageState::setup))
        return reject (kFailureChildInvalidResult, "child state must be pass, fail, or setup");

    if (detail::getStringOrEmpty (document, "started_at").isEmpty()
        || detail::getStringOrEmpty (document, "completed_at").isEmpty())
        return reject (kFailureChildInvalidResult, "child result is missing started_at/completed_at");

    double durationMs = 0.0;
    if (! detail::getFiniteNumber (document, "duration_ms", durationMs) || durationMs < 0.0)
        return reject (kFailureChildInvalidResult, "child result has no valid duration_ms");

    juce::var measurements { new juce::DynamicObject() };
    if (document.hasProperty ("measurements"))
    {
        if (! document["measurements"].isObject())
            return reject (kFailureChildInvalidResult, "child measurements is not an object");
        measurements = document["measurements"];
    }

    juce::StringArray failureCodes;
    if (document.hasProperty ("failure_codes"))
    {
        const juce::var codes = document["failure_codes"];
        if (! codes.isArray())
            return reject (kFailureChildInvalidResult, "child failure_codes is not an array");
        for (const auto& code : *codes.getArray())
        {
            if (! code.isString())
                return reject (kFailureChildInvalidResult, "child failure_codes entry is not a string");
            failureCodes.add (code.toString());
        }
    }

    // A fail/setup verdict with no named reason is unattributable evidence — reject it.
    if ((*state == StageState::fail || *state == StageState::setup) && failureCodes.isEmpty())
        return reject (kFailureChildInvalidResult, "child fail/setup carries no failure code");

    StageRecord r;
    r.stage          = expected.stage;
    r.state          = *state;
    r.claimLevel     = detail::getStringOrEmpty (document, "claim_level");
    r.startedAt      = detail::getStringOrEmpty (document, "started_at");
    r.completedAt    = detail::getStringOrEmpty (document, "completed_at");
    r.durationMs     = durationMs;
    r.measurements   = measurements;
    r.failureCodes   = failureCodes;
    r.detail         = detail::getStringOrEmpty (document, "detail");
    r.checkerVersion = detail::getStringOrEmpty (document, "checker_version");
    return r;
}

// The pass-record consistency policy (R13, R14, R16, R17, R19, R20): a stage may claim pass only
// when its own evidence supports the claim. A completed measurement that violates a locked gate is
// converted to a measured FAIL (never setup); a structurally untrustworthy claim (invented
// alignment, mislabeled frame claim) is converted to crash, because its measurement cannot be
// believed either way. No-op for non-pass states.
inline void normalizeStageRecord (StageRecord& r)
{
    if (r.state != StageState::pass)
        return;

    const auto toCrash = [&r] (const char* code, const char* why)
    {
        r.state = StageState::crash;
        r.failureCodes.add (code);
        r.detail = (r.detail.isEmpty() ? juce::String {} : r.detail + "; ") + "verdict policy: " + why;
    };
    const auto toFail = [&r] (const char* code, const char* why)
    {
        r.state = StageState::fail;
        r.failureCodes.add (code);
        r.detail = (r.detail.isEmpty() ? juce::String {} : r.detail + "; ") + "verdict policy: " + why;
    };

    if (r.stage == kStagePlayback)
    {
        if (r.claimLevel != kClaimLockedPlayback)
        {
            toCrash (kFailureClaimInvalid, "playback pass must claim locked_playback");
            return;
        }
        double grantedRate = 0.0, grantedBlock = 0.0;
        if (! detail::getFiniteNumber (r.measurements, kMeasGrantedSampleRateHz, grantedRate)
            || ! detail::getFiniteNumber (r.measurements, kMeasGrantedBlockFrames, grantedBlock))
        {
            toCrash (kFailurePlaybackEvidenceMissing, "playback pass lacks granted rate/block evidence");
            return;
        }
        // A completed measurement violating the locked gate is a measured FAIL, never setup (R14).
        if (grantedRate != kRequiredSampleRateHz)
        {
            toFail (kFailurePlaybackWrongSampleRate, "granted sample rate is not 48000");
            return;
        }
        if (grantedBlock > static_cast<double> (kMaxGrantedBlockFrames))
        {
            toFail (kFailurePlaybackBlockExceedsTarget, "granted block exceeds 128 frames");
            return;
        }
        return;
    }

    if (r.stage == kStageRecording)
    {
        const juce::String alignmentStatus = detail::getStringOrEmpty (r.measurements, kMeasAlignmentStatus);
        if (r.claimLevel == kClaimCaptureOnly)
        {
            // Capture-only must say so and must NOT carry an alignment value (R17: no invented zero).
            if (alignmentStatus != kAlignmentNotClaimed
                || (r.measurements.isObject() && r.measurements.hasProperty (kMeasAlignmentFrames)))
                toCrash (kFailureRecordingInventedAlignment,
                         "capture_only pass may not carry alignment evidence");
            return;
        }
        if (r.claimLevel == kClaimFullAlignment)
        {
            double alignmentFrames = 0.0;
            if (alignmentStatus != kAlignmentClaimed
                || detail::getStringOrEmpty (r.measurements, kMeasInputRoute) != kRouteDeviceLoopback
                || ! detail::getFiniteNumber (r.measurements, kMeasAlignmentFrames, alignmentFrames))
                toCrash (kFailureRecordingAlignmentUnproved,
                         "full_alignment pass requires device loopback route and a measured alignment");
            return;
        }
        toCrash (kFailureClaimInvalid, "recording pass must claim full_alignment or capture_only");
        return;
    }

    if (r.stage == kStageFrame)
    {
        // The packaged frame stage proves the headless dense-Timeline proxy, never window/GPU (R19).
        if (r.claimLevel != kClaimHeadlessDenseTimeline)
            toCrash (kFailureFrameClaimMismatch, "frame pass must claim headless_dense_timeline");
        return;
    }
}

// --- KTD11 aggregate verdict ----------------------------------------------------------------------
struct AggregateVerdict
{
    OverallState      overall = OverallState::setup;
    int               exitCode = 2;
    juce::StringArray failureCodes;   // union in canonical stage order, first occurrence kept
};

[[nodiscard]] inline AggregateVerdict aggregateStages (const std::vector<StageRecord>& records)
{
    AggregateVerdict verdict;

    // The orchestrator always runs exactly the three known stages; anything else is incomplete.
    const char* canonical[] = { kStagePlayback, kStageRecording, kStageFrame };
    std::vector<const StageRecord*> ordered;
    for (const char* name : canonical)
    {
        const StageRecord* found = nullptr;
        for (const auto& r : records)
        {
            if (r.stage == name)
            {
                if (found != nullptr)
                {
                    verdict.failureCodes.add (kFailureAggregateIncomplete);
                    return verdict;   // duplicate stage: overall setup, exit 2
                }
                found = &r;
            }
        }
        if (found == nullptr)
        {
            verdict.failureCodes.add (kFailureAggregateIncomplete);
            return verdict;           // missing stage: overall setup, exit 2
        }
        ordered.push_back (found);
    }
    if (records.size() != ordered.size())
    {
        verdict.failureCodes.add (kFailureAggregateIncomplete);
        return verdict;               // unknown extra stage: overall setup, exit 2
    }

    bool anyFail = false, anyIncomplete = false;
    for (const StageRecord* r : ordered)
    {
        anyFail       |= (r->state == StageState::fail);
        anyIncomplete |= (r->state == StageState::setup
                          || r->state == StageState::crash
                          || r->state == StageState::skipped);
        for (const auto& code : r->failureCodes)
            verdict.failureCodes.addIfNotAlreadyThere (code);
    }

    verdict.overall  = anyFail ? OverallState::fail
                               : (anyIncomplete ? OverallState::setup : OverallState::pass);
    verdict.exitCode = exitCodeFor (verdict.overall);
    return verdict;
}

// --- Playback stage owner policy (U3; R5-R7, R12-R14) ---------------------------------------------
// The locked gate: 48 kHz granted exactly, Block granted at or below 128 frames, no authoritative
// Underrun, callback work inside the block budget, and non-silent Project output into the device
// buffer. The 1.5x inter-arrival heuristic (deadline_misses) is DIAGNOSTIC ONLY — recorded, never
// gating — until an independent negative control proves it maps to a real budget breach (KTD6).
inline constexpr double kPlaybackMinOutputRms = 0.01;   // fixture tone RMS is ~0.127

// Stable reason codes for every backend/device route attempt (R7). These live in the attempt
// records, not in failure_codes; U6 adds the ASIO ambiguity/busy vocabulary.
inline constexpr const char* kRouteMetTarget        = "met_target";
inline constexpr const char* kRouteOpenError        = "open_error";
inline constexpr const char* kRouteBlockAboveTarget = "block_above_target";
inline constexpr const char* kRouteWrongSampleRate  = "wrong_sample_rate";
inline constexpr const char* kRouteTypeUnavailable  = "type_unavailable";
inline constexpr const char* kRouteNoDevice         = "no_device";

struct PlaybackBackendAttempt
{
    juce::String backend;       // JUCE device-type name ("ASIO", "Windows Audio (Exclusive Mode)", ...)
    juce::String device;        // device name, empty if none opened
    juce::String openError;     // JUCE error text, empty on success
    juce::String reasonCode;    // one of the kRoute* codes above
    double       grantedSampleRateHz = 0.0;
    int          grantedBlockFrames = 0;
};

[[nodiscard]] inline juce::var backendAttemptsToVar (const std::vector<PlaybackBackendAttempt>& attempts)
{
    juce::Array<juce::var> array;
    for (const auto& attempt : attempts)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("backend", attempt.backend);
        obj->setProperty ("device", attempt.device);
        obj->setProperty ("open_error", attempt.openError);
        obj->setProperty ("reason_code", attempt.reasonCode);
        obj->setProperty ("granted_sample_rate_hz", attempt.grantedSampleRateHz);
        obj->setProperty ("granted_block_frames", attempt.grantedBlockFrames);
        array.add (juce::var { obj });
    }
    return juce::var { array };
}

struct PlaybackMeasurement
{
    double grantedSampleRateHz = 0.0;
    int    grantedBlockFrames = 0;
    double seconds = 0.0;
    int    xruns = -1;               // device-reported; -1 = the backend cannot report them
    int    deadlineMisses = 0;       // inter-arrival heuristic, DIAGNOSTIC only
    double maxCallbackMs = 0.0;      // authoritative: our own work per callback
    double blockBudgetMs = 0.0;
    bool   deviceError = false;
    double outputRms = -1.0;         // channel-0 RMS of what landed in the device buffer
    juce::String deviceName;
    juce::String backendName;
};

struct PlaybackVerdict
{
    StageState        state = StageState::fail;
    juce::StringArray failureCodes;
};

[[nodiscard]] inline PlaybackVerdict evaluatePlaybackMeasurement (const PlaybackMeasurement& m)
{
    PlaybackVerdict v;
    v.state = StageState::pass;
    // A completed measurement that violates the locked gate is a FAIL — never reinterpreted as
    // setup, and zero Underruns cannot rescue a relaxed Block (AE2).
    if (m.grantedSampleRateHz != kRequiredSampleRateHz)
        v.failureCodes.add (kFailurePlaybackWrongSampleRate);
    if (m.grantedBlockFrames > kMaxGrantedBlockFrames)
        v.failureCodes.add (kFailurePlaybackBlockExceedsTarget);
    if (m.xruns > 0)
        v.failureCodes.add (kFailurePlaybackXrun);
    if (m.deviceError)
        v.failureCodes.add (kFailurePlaybackDeviceError);
    if (m.blockBudgetMs > 0.0 && m.maxCallbackMs >= m.blockBudgetMs)
        v.failureCodes.add (kFailurePlaybackCallbackBudget);
    if (m.outputRms < kPlaybackMinOutputRms)
        v.failureCodes.add (kFailurePlaybackSilentOutput);
    // deadlineMisses deliberately absent: diagnostic, not gating (KTD6).
    if (! v.failureCodes.isEmpty())
        v.state = StageState::fail;
    return v;
}

[[nodiscard]] inline StageRecord makePlaybackStageRecord (const PlaybackMeasurement& m,
                                                          const PlaybackVerdict& verdict,
                                                          const std::vector<PlaybackBackendAttempt>& attempts,
                                                          const juce::String& checkerVersion,
                                                          const juce::String& startedAt,
                                                          const juce::String& completedAt,
                                                          double durationMs)
{
    StageRecord r;
    r.stage          = kStagePlayback;
    r.state          = verdict.state;
    r.claimLevel     = verdict.state == StageState::pass ? juce::String { kClaimLockedPlayback }
                                                         : juce::String {};
    r.startedAt      = startedAt;
    r.completedAt    = completedAt;
    r.durationMs     = durationMs;
    r.failureCodes   = verdict.failureCodes;
    r.checkerVersion = checkerVersion;
    r.detail         = verdict.state == StageState::pass
                         ? juce::String { "real-Project playback met the locked 48 kHz / 128-frame gate" }
                         : juce::String { "real-Project playback violated the locked gate" };

    auto* meas = new juce::DynamicObject();
    meas->setProperty ("requested_sample_rate_hz", kRequiredSampleRateHz);
    meas->setProperty (kMeasGrantedSampleRateHz, m.grantedSampleRateHz);
    meas->setProperty ("requested_block_frames", kMaxGrantedBlockFrames);
    meas->setProperty (kMeasGrantedBlockFrames, m.grantedBlockFrames);
    meas->setProperty ("seconds", m.seconds);
    meas->setProperty ("xruns", m.xruns);
    meas->setProperty ("xrun_reporting_supported", m.xruns >= 0);
    meas->setProperty ("deadline_misses_diagnostic", m.deadlineMisses);
    meas->setProperty ("max_callback_ms", m.maxCallbackMs);
    meas->setProperty ("block_budget_ms", m.blockBudgetMs);
    meas->setProperty ("device_error", m.deviceError);
    meas->setProperty ("output_rms", m.outputRms);
    meas->setProperty ("min_output_rms", kPlaybackMinOutputRms);
    meas->setProperty ("device", m.deviceName);
    meas->setProperty ("backend", m.backendName);
    meas->setProperty ("backend_attempts", backendAttemptsToVar (attempts));
    r.measurements = juce::var { meas };
    return r;
}

// --- Frame stage owner policy (U2; R18-R19) -------------------------------------------------------
// FIXED thresholds for the packaged YesDawFrameCheck. These constants are constexpr and the
// evaluation below is a pure function of its inputs, so ambient CI (or any environment variable)
// cannot relax the owner verdict — the CI-runner outlier tolerance lives ONLY in the Catch2
// regression configuration in tests/timeline_gpu_tests.cpp. The measurement itself comes from
// src/ui/TimelineFrameCheck.h; this side only judges the numbers.
inline constexpr double kFrameBudgetMs             = 16.6;
inline constexpr int    kFrameAllowedOutlierFrames = 2;
inline constexpr int    kFrameMinVisibleClips      = 250;
inline constexpr int    kFrameMinDistinctSamples   = 20;

struct FrameMeasurement
{
    double        sustainedFrameMs = 0.0;   // worst frame after discarding the allowed outliers
    double        maxFrameMs = 0.0;
    int           slowFrameCount = 0;       // frames at or over budget
    int           measuredFrames = 0;
    int           maxVisibleClips = 0;
    int           distinctSamples = 0;
    bool          hitVisibleClipCapacity = false;
    std::uint64_t checksum = 0;
    int           totalClips = 0;
};

struct FrameVerdict
{
    StageState        state = StageState::fail;
    juce::StringArray failureCodes;
};

[[nodiscard]] inline FrameVerdict evaluateFrameMeasurement (const FrameMeasurement& m)
{
    FrameVerdict v;
    v.state = StageState::pass;
    if (m.distinctSamples < kFrameMinDistinctSamples)
        v.failureCodes.add (kFailureFrameBlankOutput);
    if (m.maxVisibleClips < kFrameMinVisibleClips)
        v.failureCodes.add (kFailureFrameInsufficientDensity);
    if (m.hitVisibleClipCapacity)
        v.failureCodes.add (kFailureFrameCapacityExceeded);
    // R19: fail on sustained frame time at or above budget. Too many single-frame spikes is the
    // same verdict — a genuinely slow renderer, not a scheduler blip.
    if (m.sustainedFrameMs >= kFrameBudgetMs || m.slowFrameCount > kFrameAllowedOutlierFrames)
        v.failureCodes.add (kFailureFrameOverBudget);
    if (! v.failureCodes.isEmpty())
        v.state = StageState::fail;
    return v;
}

// The exact stage record the packaged checker emits (and tests validate). A passing frame stage
// claims headless_dense_timeline and nothing grander; a failing one claims nothing.
[[nodiscard]] inline StageRecord makeFrameStageRecord (const FrameMeasurement& m,
                                                       const FrameVerdict& verdict,
                                                       const juce::String& checkerVersion,
                                                       const juce::String& startedAt,
                                                       const juce::String& completedAt,
                                                       double durationMs)
{
    StageRecord r;
    r.stage          = kStageFrame;
    r.state          = verdict.state;
    r.claimLevel     = verdict.state == StageState::pass ? juce::String { kClaimHeadlessDenseTimeline }
                                                         : juce::String {};
    r.startedAt      = startedAt;
    r.completedAt    = completedAt;
    r.durationMs     = durationMs;
    r.failureCodes   = verdict.failureCodes;
    r.checkerVersion = checkerVersion;
    r.detail         = verdict.state == StageState::pass
                         ? juce::String { "headless dense-Timeline proxy within budget" }
                         : juce::String { "headless dense-Timeline proxy violated the owner policy" };

    auto* meas = new juce::DynamicObject();
    meas->setProperty ("sustained_frame_ms", m.sustainedFrameMs);
    meas->setProperty ("max_frame_ms", m.maxFrameMs);
    meas->setProperty ("slow_frame_count", m.slowFrameCount);
    meas->setProperty ("measured_frames", m.measuredFrames);
    meas->setProperty ("frame_budget_ms", kFrameBudgetMs);
    meas->setProperty ("allowed_outlier_frames", kFrameAllowedOutlierFrames);
    meas->setProperty ("max_visible_clips", m.maxVisibleClips);
    meas->setProperty ("distinct_samples", m.distinctSamples);
    meas->setProperty ("hit_visible_clip_capacity", m.hitVisibleClipCapacity);
    meas->setProperty ("total_clips", m.totalClips);
    meas->setProperty ("checksum", juce::String { m.checksum });
    r.measurements = juce::var { meas };
    return r;
}

} // namespace yesdaw::app::hardware
