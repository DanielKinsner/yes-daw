// YES DAW — deterministic, device-free tests for the packaged hardware-verification contract
// (H17 packaged verifier U1): schema v1, child-document acceptance, the pass-record consistency
// policy, KTD11 aggregation, locale-invariant JSON, and atomic replacement.
//
// The fixture sweep replays tests/fixtures/hardware-verification/*.json — the SAME files that
// tools/verify-hardware.ps1 -SelfTest replays with its PowerShell reimplementation of this policy.
// Both harnesses assert the exact fixture set, so policy drift between C++ and the packaged script
// fails mechanically instead of silently.

#include "app/HardwareVerification.h"
#include "app/PlaybackCheckFixture.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <clocale>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace hw = yesdaw::app::hardware;

namespace {

// The committed fixture set, alphabetical. tools/verify-hardware.ps1 hardcodes the same list;
// adding a fixture means updating BOTH lists in the same commit (that is the lockstep).
const std::vector<std::string> kFixtureNames = {
    "capture-only-pass",
    "child-crash",
    "child-invalid-result",
    "child-missing-result",
    "child-version-mismatch",
    "child-wrong-run-id",
    "child-wrong-schema",
    "child-wrong-stage",
    "clean-pass",
    "frame-claim-mismatch",
    "invented-alignment-rejected",
    "measured-fail-retains-timeout",
    "mixed-fail-and-setup",
    "pass-contradicts-measurement",
    "setup-incomplete",
    "stage-skipped",
};

// Mirror of the orchestrator's per-child handling: accept+normalize a document, or synthesize the
// terminal record for timeout/crash/missing/skipped outcomes.
hw::StageRecord recordFromChild (const juce::var& child,
                                 const juce::String& runId,
                                 const juce::String& checkerVersion)
{
    const juce::String slot    = child["stage"].toString();
    const juce::String outcome = child["outcome"].toString();

    if (outcome == "document")
    {
        auto record = hw::acceptStageDocument (child["document"], { runId, slot, checkerVersion });
        hw::normalizeStageRecord (record);
        return record;
    }
    if (outcome == "timeout")
        return hw::synthesizeTerminalRecord (slot, hw::StageState::crash,
                                             hw::kFailureChildTimeout, "child hit its timeout");
    if (outcome == "crash")
        return hw::synthesizeTerminalRecord (slot, hw::StageState::crash,
                                             hw::kFailureChildCrash, "child exited abnormally");
    if (outcome == "missing")
        return hw::synthesizeTerminalRecord (slot, hw::StageState::crash,
                                             hw::kFailureChildMissingResult, "child wrote no result");
    if (outcome == "skipped")
        return hw::synthesizeTerminalRecord (slot, hw::StageState::skipped, {}, "stage skipped");

    FAIL ("fixture child has unknown outcome: " + outcome.toStdString());
    return {};
}

const hw::StageRecord* findRecord (const std::vector<hw::StageRecord>& records,
                                   const juce::String& stage)
{
    for (const auto& r : records)
        if (r.stage == stage)
            return &r;
    return nullptr;
}

hw::StageRecord makeAcceptedPassRecord (const char* stage)
{
    hw::StageRecord r;
    r.stage = stage;
    r.state = hw::StageState::pass;
    r.checkerVersion = "0.0.0-test";
    r.startedAt = "2026-07-28T12:00:00Z";
    r.completedAt = "2026-07-28T12:01:00Z";
    r.durationMs = 60000.0;

    auto* meas = new juce::DynamicObject();
    if (juce::String { stage } == hw::kStagePlayback)
    {
        r.claimLevel = hw::kClaimLockedPlayback;
        meas->setProperty (hw::kMeasGrantedSampleRateHz, 48000);
        meas->setProperty (hw::kMeasGrantedBlockFrames, 128);
    }
    else if (juce::String { stage } == hw::kStageRecording)
    {
        r.claimLevel = hw::kClaimFullAlignment;
        meas->setProperty (hw::kMeasAlignmentStatus, hw::kAlignmentClaimed);
        meas->setProperty (hw::kMeasInputRoute, hw::kRouteDeviceLoopback);
        meas->setProperty (hw::kMeasAlignmentFrames, 2);
    }
    else
    {
        r.claimLevel = hw::kClaimHeadlessDenseTimeline;
        meas->setProperty ("max_frame_ms", 6.4);
    }
    r.measurements = juce::var { meas };
    return r;
}

} // namespace

TEST_CASE ("stage states and exit codes are the KTD11 contract")
{
    using hw::StageState;
    for (const auto s : { StageState::pass, StageState::fail, StageState::setup,
                          StageState::crash, StageState::skipped })
    {
        const auto round = hw::stageStateFromString (juce::String { hw::toString (s) });
        REQUIRE (round.has_value());
        CHECK (*round == s);
    }
    CHECK (! hw::stageStateFromString ("PASS").has_value());
    CHECK (! hw::stageStateFromString ("").has_value());

    CHECK (hw::exitCodeFor (hw::OverallState::pass) == 0);
    CHECK (hw::exitCodeFor (hw::OverallState::fail) == 1);
    CHECK (hw::exitCodeFor (hw::OverallState::setup) == 2);
}

TEST_CASE ("failure-code registry strings are frozen")
{
    // Renaming any of these is a schema change (new schema version + new fixtures), never a rename.
    CHECK (std::string { hw::kFailureChildCrash } == "child_crash");
    CHECK (std::string { hw::kFailureChildTimeout } == "child_timeout");
    CHECK (std::string { hw::kFailureChildMissingResult } == "child_missing_result");
    CHECK (std::string { hw::kFailureChildInvalidResult } == "child_invalid_result");
    CHECK (std::string { hw::kFailureChildWrongSchema } == "child_wrong_schema");
    CHECK (std::string { hw::kFailureChildWrongRunId } == "child_wrong_run_id");
    CHECK (std::string { hw::kFailureChildWrongStage } == "child_wrong_stage");
    CHECK (std::string { hw::kFailureChildVersionMismatch } == "child_version_mismatch");
    CHECK (std::string { hw::kFailureClaimInvalid } == "claim_invalid");
    CHECK (std::string { hw::kFailureAggregateIncomplete } == "aggregate_incomplete");
    CHECK (std::string { hw::kFailureDeviceUnavailable } == "device_unavailable");
    CHECK (std::string { hw::kFailurePlaybackEvidenceMissing } == "playback_evidence_missing");
    CHECK (std::string { hw::kFailurePlaybackWrongSampleRate } == "playback_wrong_sample_rate");
    CHECK (std::string { hw::kFailurePlaybackBlockExceedsTarget } == "playback_block_exceeds_target");
    CHECK (std::string { hw::kFailureRecordingInventedAlignment } == "recording_invented_alignment");
    CHECK (std::string { hw::kFailureRecordingAlignmentUnproved } == "recording_alignment_unproved");
    CHECK (std::string { hw::kFailureFrameClaimMismatch } == "frame_claim_mismatch");
    CHECK (std::string { hw::kFailureFrameBlankOutput } == "frame_blank_output");
    CHECK (std::string { hw::kFailureFrameInsufficientDensity } == "frame_insufficient_density");
    CHECK (std::string { hw::kFailureFrameCapacityExceeded } == "frame_capacity_exceeded");
    CHECK (std::string { hw::kFailureFrameOverBudget } == "frame_over_budget");

    CHECK (hw::kSchemaVersion == 1);
    CHECK (hw::kRequiredSampleRateHz == 48000.0);
    CHECK (hw::kMaxGrantedBlockFrames == 128);

    // The frame owner policy is FIXED (R19). Changing any of these numbers is an ADR-level gate
    // change, not a tweak — this pin makes that mechanical.
    CHECK (hw::kFrameBudgetMs == 16.6);
    CHECK (hw::kFrameAllowedOutlierFrames == 2);
    CHECK (hw::kFrameMinVisibleClips == 250);
    CHECK (hw::kFrameMinDistinctSamples == 20);

    // Playback stage codes and route reason codes (U3).
    CHECK (std::string { hw::kFailurePlaybackXrun } == "playback_xrun");
    CHECK (std::string { hw::kFailurePlaybackDeviceError } == "playback_device_error");
    CHECK (std::string { hw::kFailurePlaybackSilentOutput } == "playback_silent_output");
    CHECK (std::string { hw::kFailurePlaybackCallbackBudget } == "playback_callback_budget");
    CHECK (std::string { hw::kRouteMetTarget } == "met_target");
    CHECK (std::string { hw::kRouteOpenError } == "open_error");
    CHECK (std::string { hw::kRouteBlockAboveTarget } == "block_above_target");
    CHECK (std::string { hw::kRouteWrongSampleRate } == "wrong_sample_rate");
    CHECK (std::string { hw::kRouteTypeUnavailable } == "type_unavailable");
    CHECK (std::string { hw::kRouteNoDevice } == "no_device");
    CHECK (hw::kPlaybackMinOutputRms == 0.01);

    // Recording stage codes and route vocabulary (U4).
    CHECK (std::string { hw::kFailureRecordingFifoDrop } == "recording_fifo_drop");
    CHECK (std::string { hw::kFailureRecordingSilent } == "recording_silent");
    CHECK (std::string { hw::kFailureRecordingInvalidWav } == "recording_invalid_wav");
    CHECK (std::string { hw::kFailureRecordingBrokenLinkage } == "recording_broken_linkage");
    CHECK (std::string { hw::kFailureRecordingHashMismatch } == "recording_hash_mismatch");
    CHECK (std::string { hw::kFailureRecordingBadAlignment } == "recording_bad_alignment");
    CHECK (std::string { hw::kRouteMicrophone } == "microphone");
    CHECK (std::string { hw::kRouteUnclassified } == "unclassified");
    CHECK (hw::kRecordingMinCaptureRms == 1.0e-4);
    CHECK (hw::kRecordingAlignmentToleranceFrames == 128);
}

TEST_CASE ("JSON serialization is locale-invariant and escapes hostile strings")
{
    // Force a comma-decimal locale if this machine has one; the JSON writer must not care.
    const std::string previousLocale = std::setlocale (LC_ALL, nullptr);
    const char* candidates[] = { "de_DE.UTF-8", "de_DE", "German_Germany.1252",
                                 "fr_FR.UTF-8", "French_France.1252" };
    bool localeApplied = false;
    for (const char* candidate : candidates)
        if (std::setlocale (LC_ALL, candidate) != nullptr)
        {
            localeApplied = true;
            break;
        }
    INFO ("comma-decimal locale applied: " << (localeApplied ? "yes" : "no (C locale fallback)"));

    hw::StageRecord r = makeAcceptedPassRecord (hw::kStageFrame);
    r.durationMs = 1234.5;
    r.detail = juce::String::fromUTF8 ("quote \" backslash \\ newline \n tab \t cafe\xC3\xA9");

    const juce::var document = hw::stageDocumentToVar (r, "run-locale");
    const juce::String text = hw::toJsonText (document);

    CHECK (text.contains ("1234.5"));
    CHECK (! text.contains ("1234,5"));
    CHECK (text.contains ("\\\""));
    CHECK (text.contains ("\\\\"));
    CHECK (text.contains ("\\n"));
    CHECK (text.contains ("\\t"));

    const auto reparsed = hw::parseJsonText (text);
    REQUIRE (reparsed.has_value());
    CHECK ((*reparsed)["detail"].toString() == r.detail);
    CHECK (static_cast<double> ((*reparsed)["duration_ms"]) == 1234.5);

    std::setlocale (LC_ALL, previousLocale.c_str());
}

TEST_CASE ("atomic JSON write replaces the target and leaves no temp file")
{
    const auto dir = std::filesystem::temp_directory_path() / "yesdaw-hwverify-u1-test";
    std::filesystem::create_directories (dir);
    const auto target = dir / "result.json";

    std::string error;
    REQUIRE (hw::writeJsonAtomically (target,
                                      hw::stageDocumentToVar (makeAcceptedPassRecord (hw::kStagePlayback), "run-1"),
                                      error));
    CHECK (error.empty());
    CHECK (std::filesystem::exists (target));
    CHECK (! std::filesystem::exists (dir / "result.json.tmp"));

    // Overwrite must replace, not append or fail.
    REQUIRE (hw::writeJsonAtomically (target,
                                      hw::stageDocumentToVar (makeAcceptedPassRecord (hw::kStagePlayback), "run-2"),
                                      error));
    const juce::File file { juce::String { target.string() } };
    const auto parsed = hw::parseJsonText (file.loadFileAsString());
    REQUIRE (parsed.has_value());
    CHECK ((*parsed)["run_id"].toString() == "run-2");
    CHECK (! std::filesystem::exists (dir / "result.json.tmp"));

    // A missing parent directory is a reported error, not a crash or a silent success.
    CHECK (! hw::writeJsonAtomically (dir / "does-not-exist" / "x.json",
                                      juce::var { new juce::DynamicObject() }, error));
    CHECK (! error.empty());

    std::filesystem::remove_all (dir);
}

TEST_CASE ("a checker-authored document is accepted verbatim")
{
    const hw::StageRecord original = makeAcceptedPassRecord (hw::kStageRecording);
    const juce::var document = hw::stageDocumentToVar (original, "run-round");

    auto accepted = hw::acceptStageDocument (document, { "run-round", hw::kStageRecording, "0.0.0-test" });
    hw::normalizeStageRecord (accepted);

    CHECK (accepted.state == hw::StageState::pass);
    CHECK (accepted.claimLevel == hw::kClaimFullAlignment);
    CHECK (accepted.failureCodes.isEmpty());
    CHECK (accepted.durationMs == original.durationMs);
}

TEST_CASE ("acceptance rejects untrustworthy child documents with named codes")
{
    const hw::ChildExpectation expected { "run-1", hw::kStagePlayback, "0.0.0-test" };

    SECTION ("not an object")
    {
        const auto r = hw::acceptStageDocument (juce::var { "not an object" }, expected);
        CHECK (r.state == hw::StageState::crash);
        CHECK (r.failureCodes.contains (hw::kFailureChildInvalidResult));
        CHECK (r.stage == hw::kStagePlayback);
    }
    SECTION ("fail with no failure code is unattributable")
    {
        hw::StageRecord failing = makeAcceptedPassRecord (hw::kStagePlayback);
        failing.state = hw::StageState::fail;
        failing.claimLevel = "";
        const auto r = hw::acceptStageDocument (hw::stageDocumentToVar (failing, "run-1"), expected);
        CHECK (r.state == hw::StageState::crash);
        CHECK (r.failureCodes.contains (hw::kFailureChildInvalidResult));
    }
    SECTION ("missing duration_ms")
    {
        auto document = hw::stageDocumentToVar (makeAcceptedPassRecord (hw::kStagePlayback), "run-1");
        document.getDynamicObject()->removeProperty ("duration_ms");
        const auto r = hw::acceptStageDocument (document, expected);
        CHECK (r.state == hw::StageState::crash);
        CHECK (r.failureCodes.contains (hw::kFailureChildInvalidResult));
    }
}

TEST_CASE ("pass-record consistency policy bites")
{
    SECTION ("playback pass without granted evidence cannot be believed")
    {
        hw::StageRecord r = makeAcceptedPassRecord (hw::kStagePlayback);
        r.measurements = juce::var { new juce::DynamicObject() };
        hw::normalizeStageRecord (r);
        CHECK (r.state == hw::StageState::crash);
        CHECK (r.failureCodes.contains (hw::kFailurePlaybackEvidenceMissing));
    }
    SECTION ("playback pass at 44100 becomes a measured FAIL, never setup")
    {
        hw::StageRecord r = makeAcceptedPassRecord (hw::kStagePlayback);
        r.measurements.getDynamicObject()->setProperty (hw::kMeasGrantedSampleRateHz, 44100);
        hw::normalizeStageRecord (r);
        CHECK (r.state == hw::StageState::fail);
        CHECK (r.failureCodes.contains (hw::kFailurePlaybackWrongSampleRate));
    }
    SECTION ("full_alignment without device-loopback provenance is unproved")
    {
        hw::StageRecord r = makeAcceptedPassRecord (hw::kStageRecording);
        r.measurements.getDynamicObject()->setProperty (hw::kMeasInputRoute, "microphone");
        hw::normalizeStageRecord (r);
        CHECK (r.state == hw::StageState::crash);
        CHECK (r.failureCodes.contains (hw::kFailureRecordingAlignmentUnproved));
    }
    SECTION ("an unknown recording claim is invalid")
    {
        hw::StageRecord r = makeAcceptedPassRecord (hw::kStageRecording);
        r.claimLevel = "vibes";
        hw::normalizeStageRecord (r);
        CHECK (r.state == hw::StageState::crash);
        CHECK (r.failureCodes.contains (hw::kFailureClaimInvalid));
    }
}

TEST_CASE ("aggregation requires exactly the three canonical stages")
{
    SECTION ("empty input is incomplete")
    {
        const auto verdict = hw::aggregateStages ({});
        CHECK (verdict.overall == hw::OverallState::setup);
        CHECK (verdict.exitCode == 2);
        CHECK (verdict.failureCodes.contains (hw::kFailureAggregateIncomplete));
    }
    SECTION ("a missing stage is incomplete")
    {
        const auto verdict = hw::aggregateStages ({ makeAcceptedPassRecord (hw::kStagePlayback),
                                                    makeAcceptedPassRecord (hw::kStageFrame) });
        CHECK (verdict.exitCode == 2);
        CHECK (verdict.failureCodes.contains (hw::kFailureAggregateIncomplete));
    }
    SECTION ("a duplicate stage is incomplete")
    {
        const auto verdict = hw::aggregateStages ({ makeAcceptedPassRecord (hw::kStagePlayback),
                                                    makeAcceptedPassRecord (hw::kStagePlayback),
                                                    makeAcceptedPassRecord (hw::kStageRecording),
                                                    makeAcceptedPassRecord (hw::kStageFrame) });
        CHECK (verdict.exitCode == 2);
        CHECK (verdict.failureCodes.contains (hw::kFailureAggregateIncomplete));
    }
    SECTION ("an unknown extra stage is incomplete")
    {
        auto bogus = makeAcceptedPassRecord (hw::kStageFrame);
        bogus.stage = "bogus";
        const auto verdict = hw::aggregateStages ({ makeAcceptedPassRecord (hw::kStagePlayback),
                                                    makeAcceptedPassRecord (hw::kStageRecording),
                                                    makeAcceptedPassRecord (hw::kStageFrame),
                                                    bogus });
        CHECK (verdict.exitCode == 2);
        CHECK (verdict.failureCodes.contains (hw::kFailureAggregateIncomplete));
    }
    SECTION ("all three passing aggregate to exit 0")
    {
        const auto verdict = hw::aggregateStages ({ makeAcceptedPassRecord (hw::kStagePlayback),
                                                    makeAcceptedPassRecord (hw::kStageRecording),
                                                    makeAcceptedPassRecord (hw::kStageFrame) });
        CHECK (verdict.overall == hw::OverallState::pass);
        CHECK (verdict.exitCode == 0);
        CHECK (verdict.failureCodes.isEmpty());
    }
}

namespace {

hw::FrameMeasurement makeHealthyFrameMeasurement()
{
    hw::FrameMeasurement m;
    m.sustainedFrameMs = 6.0;
    m.maxFrameMs = 12.0;
    m.slowFrameCount = 1;
    m.measuredFrames = 160;
    m.maxVisibleClips = 300;
    m.distinctSamples = 40;
    m.hitVisibleClipCapacity = false;
    m.checksum = 12345;
    m.totalClips = 20640;
    return m;
}

} // namespace

TEST_CASE ("frame owner evaluation fails each violation with its own code")
{
    SECTION ("healthy measurement passes with no codes")
    {
        const auto v = hw::evaluateFrameMeasurement (makeHealthyFrameMeasurement());
        CHECK (v.state == hw::StageState::pass);
        CHECK (v.failureCodes.isEmpty());
    }
    SECTION ("blank output")
    {
        auto m = makeHealthyFrameMeasurement();
        m.distinctSamples = hw::kFrameMinDistinctSamples - 1;
        const auto v = hw::evaluateFrameMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailureFrameBlankOutput });
    }
    SECTION ("insufficient density")
    {
        auto m = makeHealthyFrameMeasurement();
        m.maxVisibleClips = hw::kFrameMinVisibleClips - 1;
        const auto v = hw::evaluateFrameMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailureFrameInsufficientDensity });
    }
    SECTION ("capacity clamp invalidates the density claim")
    {
        auto m = makeHealthyFrameMeasurement();
        m.hitVisibleClipCapacity = true;
        const auto v = hw::evaluateFrameMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailureFrameCapacityExceeded });
    }
    SECTION ("sustained frame time at the budget fails (>= is the contract)")
    {
        auto m = makeHealthyFrameMeasurement();
        m.sustainedFrameMs = hw::kFrameBudgetMs;
        const auto v = hw::evaluateFrameMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailureFrameOverBudget });
    }
    SECTION ("too many slow frames fails even with a good sustained value")
    {
        auto m = makeHealthyFrameMeasurement();
        m.slowFrameCount = hw::kFrameAllowedOutlierFrames + 1;
        const auto v = hw::evaluateFrameMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailureFrameOverBudget });
    }
    SECTION ("multiple violations accumulate")
    {
        auto m = makeHealthyFrameMeasurement();
        m.distinctSamples = 0;
        m.sustainedFrameMs = 99.0;
        const auto v = hw::evaluateFrameMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes.contains (hw::kFailureFrameBlankOutput));
        CHECK (v.failureCodes.contains (hw::kFailureFrameOverBudget));
    }
}

TEST_CASE ("ambient CI does not relax the frame owner evaluation")
{
    // The Catch2 GPU gate deliberately widens its outlier tolerance on CI runners; the packaged
    // owner policy must never do that. This pin runs the same borderline measurement with CI set
    // and asserts the verdict is byte-identical — it bites the moment someone teaches
    // evaluateFrameMeasurement (or its constants) to read the environment.
    auto borderline = makeHealthyFrameMeasurement();
    borderline.slowFrameCount = hw::kFrameAllowedOutlierFrames + 1;   // CI-tolerant logic would allow this

    const auto before = hw::evaluateFrameMeasurement (borderline);

#if defined(_WIN32)
    _putenv_s ("CI", "1");
#else
    setenv ("CI", "1", 1);
#endif
    const auto during = hw::evaluateFrameMeasurement (borderline);
#if defined(_WIN32)
    _putenv_s ("CI", "");
#else
    unsetenv ("CI");
#endif

    CHECK (before.state == hw::StageState::fail);
    CHECK (during.state == before.state);
    CHECK (during.failureCodes == before.failureCodes);
}

TEST_CASE ("frame stage record maps verdicts to honest claims")
{
    SECTION ("pass claims exactly headless_dense_timeline and survives acceptance")
    {
        const auto m = makeHealthyFrameMeasurement();
        const auto record = hw::makeFrameStageRecord (m, hw::evaluateFrameMeasurement (m), "1.0.0-t",
                                                      "2026-08-04T10:00:00Z", "2026-08-04T10:00:30Z",
                                                      30000.0);
        CHECK (record.claimLevel == hw::kClaimHeadlessDenseTimeline);

        auto accepted = hw::acceptStageDocument (hw::stageDocumentToVar (record, "run-f"),
                                                 { "run-f", hw::kStageFrame, "1.0.0-t" });
        hw::normalizeStageRecord (accepted);
        CHECK (accepted.state == hw::StageState::pass);
        CHECK (accepted.claimLevel == hw::kClaimHeadlessDenseTimeline);
        CHECK (static_cast<double> (accepted.measurements["frame_budget_ms"]) == hw::kFrameBudgetMs);
    }
    SECTION ("fail claims nothing and keeps its codes through acceptance")
    {
        auto m = makeHealthyFrameMeasurement();
        m.sustainedFrameMs = 40.0;
        const auto record = hw::makeFrameStageRecord (m, hw::evaluateFrameMeasurement (m), "1.0.0-t",
                                                      "2026-08-04T10:00:00Z", "2026-08-04T10:00:30Z",
                                                      30000.0);
        CHECK (record.claimLevel.isEmpty());

        auto accepted = hw::acceptStageDocument (hw::stageDocumentToVar (record, "run-f"),
                                                 { "run-f", hw::kStageFrame, "1.0.0-t" });
        hw::normalizeStageRecord (accepted);
        CHECK (accepted.state == hw::StageState::fail);
        CHECK (accepted.failureCodes == juce::StringArray { hw::kFailureFrameOverBudget });
    }
}

namespace {

hw::PlaybackMeasurement makeHealthyPlaybackMeasurement()
{
    hw::PlaybackMeasurement m;
    m.grantedSampleRateHz = 48000.0;
    m.grantedBlockFrames = 128;
    m.seconds = 30.0;
    m.xruns = 0;
    m.deadlineMisses = 0;
    m.maxCallbackMs = 0.4;
    m.blockBudgetMs = 128.0 / 48000.0 * 1000.0;
    m.deviceError = false;
    m.outputRms = 0.127;
    m.deviceName = "Test Device";
    m.backendName = "Windows Audio (Exclusive Mode)";
    return m;
}

} // namespace

TEST_CASE ("playback owner evaluation enforces the locked gate")
{
    SECTION ("healthy measurement passes with no codes")
    {
        const auto v = hw::evaluatePlaybackMeasurement (makeHealthyPlaybackMeasurement());
        CHECK (v.state == hw::StageState::pass);
        CHECK (v.failureCodes.isEmpty());
    }
    SECTION ("a 480-frame grant fails even with zero Underruns (AE2)")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.grantedBlockFrames = 480;
        const auto v = hw::evaluatePlaybackMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailurePlaybackBlockExceedsTarget });
    }
    SECTION ("a 144-frame grant fails too — the target is 128, not 'close'")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.grantedBlockFrames = 144;
        const auto v = hw::evaluatePlaybackMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes.contains (hw::kFailurePlaybackBlockExceedsTarget));
    }
    SECTION ("a 44.1 kHz grant fails a 48 kHz request")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.grantedSampleRateHz = 44100.0;
        const auto v = hw::evaluatePlaybackMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailurePlaybackWrongSampleRate });
    }
    SECTION ("an authoritative xrun fails")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.xruns = 1;
        const auto v = hw::evaluatePlaybackMeasurement (m);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailurePlaybackXrun });
    }
    SECTION ("unsupported xrun reporting is not a failure by itself")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.xruns = -1;   // backend cannot report; the callback-budget metric stays authoritative
        const auto v = hw::evaluatePlaybackMeasurement (m);
        CHECK (v.state == hw::StageState::pass);
    }
    SECTION ("a device error fails")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.deviceError = true;
        const auto v = hw::evaluatePlaybackMeasurement (m);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailurePlaybackDeviceError });
    }
    SECTION ("silent output fails — soaking zeros is not playback")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.outputRms = 0.0;
        const auto v = hw::evaluatePlaybackMeasurement (m);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailurePlaybackSilentOutput });
    }
    SECTION ("callback work at or over the block budget fails")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.maxCallbackMs = m.blockBudgetMs;
        const auto v = hw::evaluatePlaybackMeasurement (m);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailurePlaybackCallbackBudget });
    }
    SECTION ("the inter-arrival heuristic is diagnostic: misses alone never gate (KTD6)")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.deadlineMisses = 5000;   // bursty-but-healthy exclusive WASAPI looks exactly like this
        const auto v = hw::evaluatePlaybackMeasurement (m);
        CHECK (v.state == hw::StageState::pass);
    }
}

TEST_CASE ("playback stage record maps verdicts and preserves ordered backend attempts")
{
    std::vector<hw::PlaybackBackendAttempt> attempts;
    hw::PlaybackBackendAttempt asio;
    asio.backend = "ASIO";
    asio.reasonCode = hw::kRouteTypeUnavailable;
    attempts.push_back (asio);
    hw::PlaybackBackendAttempt shared;
    shared.backend = "Windows Audio (Low Latency Mode)";
    shared.device = "Speakers";
    shared.grantedSampleRateHz = 48000.0;
    shared.grantedBlockFrames = 480;
    shared.reasonCode = hw::kRouteBlockAboveTarget;
    attempts.push_back (shared);
    hw::PlaybackBackendAttempt exclusive;
    exclusive.backend = "Windows Audio (Exclusive Mode)";
    exclusive.device = "Speakers";
    exclusive.grantedSampleRateHz = 48000.0;
    exclusive.grantedBlockFrames = 128;
    exclusive.reasonCode = hw::kRouteMetTarget;
    attempts.push_back (exclusive);

    SECTION ("pass claims locked_playback and survives acceptance + normalization")
    {
        const auto m = makeHealthyPlaybackMeasurement();
        const auto record = hw::makePlaybackStageRecord (m, hw::evaluatePlaybackMeasurement (m),
                                                         attempts, "1.0.0-t",
                                                         "2026-08-04T11:00:00Z",
                                                         "2026-08-04T11:00:30Z", 30000.0);
        CHECK (record.claimLevel == hw::kClaimLockedPlayback);

        auto accepted = hw::acceptStageDocument (hw::stageDocumentToVar (record, "run-p"),
                                                 { "run-p", hw::kStagePlayback, "1.0.0-t" });
        hw::normalizeStageRecord (accepted);
        CHECK (accepted.state == hw::StageState::pass);
        CHECK (accepted.claimLevel == hw::kClaimLockedPlayback);

        // Every route attempt is retained, in order, with its reason code (R7).
        const juce::var recorded = accepted.measurements["backend_attempts"];
        REQUIRE (recorded.isArray());
        REQUIRE (recorded.getArray()->size() == 3);
        CHECK ((*recorded.getArray())[0]["reason_code"].toString() == hw::kRouteTypeUnavailable);
        CHECK ((*recorded.getArray())[1]["reason_code"].toString() == hw::kRouteBlockAboveTarget);
        CHECK ((*recorded.getArray())[2]["reason_code"].toString() == hw::kRouteMetTarget);
    }
    SECTION ("a relaxed 480-frame run claims nothing and cannot normalize into a pass")
    {
        auto m = makeHealthyPlaybackMeasurement();
        m.grantedBlockFrames = 480;
        const auto record = hw::makePlaybackStageRecord (m, hw::evaluatePlaybackMeasurement (m),
                                                         attempts, "1.0.0-t",
                                                         "2026-08-04T11:00:00Z",
                                                         "2026-08-04T11:00:30Z", 30000.0);
        CHECK (record.claimLevel.isEmpty());
        CHECK (record.state == hw::StageState::fail);

        auto accepted = hw::acceptStageDocument (hw::stageDocumentToVar (record, "run-p"),
                                                 { "run-p", hw::kStagePlayback, "1.0.0-t" });
        hw::normalizeStageRecord (accepted);
        CHECK (accepted.state == hw::StageState::fail);
        CHECK (accepted.failureCodes.contains (hw::kFailurePlaybackBlockExceedsTarget));
    }
}

namespace {

hw::RecordingMeasurement makeHealthyLoopbackRecording()
{
    hw::RecordingMeasurement m;
    m.sampleRateHz = 48000.0;
    m.channels = 1;
    m.capturedFrames = 96000;
    m.droppedFrames = 0;
    m.captureRms = 0.05;
    m.wavValid = true;
    m.linkageValid = true;
    m.hashMatch = true;
    m.inputRoute = hw::kRouteDeviceLoopback;
    m.correlationFound = true;
    m.correlationSnr = 40.0;
    m.alignmentErrorFrames = 3;
    m.deviceName = "Loopback (Test)";
    m.backendName = "Windows Audio";
    return m;
}

} // namespace

TEST_CASE ("recording owner evaluation gates alignment credit on proved loopback")
{
    SECTION ("loopback-proved, in-tolerance capture claims full alignment")
    {
        const auto v = hw::evaluateRecordingMeasurement (makeHealthyLoopbackRecording());
        CHECK (v.state == hw::StageState::pass);
        CHECK (v.claimLevel == hw::kClaimFullAlignment);
        CHECK (v.alignmentStatus == hw::kAlignmentClaimed);
        CHECK (v.failureCodes.isEmpty());
    }
    SECTION ("loopback out of tolerance is a measured FAIL")
    {
        auto m = makeHealthyLoopbackRecording();
        m.alignmentErrorFrames = hw::kRecordingAlignmentToleranceFrames + 1;
        const auto v = hw::evaluateRecordingMeasurement (m);
        CHECK (v.state == hw::StageState::fail);
        CHECK (v.failureCodes == juce::StringArray { hw::kFailureRecordingBadAlignment });
    }
    SECTION ("loopback with no correlation degrades to capture_only (AE3)")
    {
        auto m = makeHealthyLoopbackRecording();
        m.correlationFound = false;
        const auto v = hw::evaluateRecordingMeasurement (m);
        CHECK (v.state == hw::StageState::pass);
        CHECK (v.claimLevel == hw::kClaimCaptureOnly);
        CHECK (v.alignmentStatus == hw::kAlignmentNotClaimed);
    }
    SECTION ("microphone correlation cannot earn alignment credit (R16)")
    {
        auto m = makeHealthyLoopbackRecording();
        m.inputRoute = hw::kRouteMicrophone;
        m.alignmentErrorFrames = 0;   // even a perfect-looking number is not proof
        const auto v = hw::evaluateRecordingMeasurement (m);
        CHECK (v.state == hw::StageState::pass);
        CHECK (v.claimLevel == hw::kClaimCaptureOnly);
        CHECK (v.alignmentStatus == hw::kAlignmentNotClaimed);
    }
    SECTION ("unclassified route degrades to capture_only")
    {
        auto m = makeHealthyLoopbackRecording();
        m.inputRoute = hw::kRouteUnclassified;
        const auto v = hw::evaluateRecordingMeasurement (m);
        CHECK (v.claimLevel == hw::kClaimCaptureOnly);
    }
    SECTION ("each capture/persistence violation fails with its own code")
    {
        auto drop = makeHealthyLoopbackRecording();
        drop.droppedFrames = 1;
        CHECK (hw::evaluateRecordingMeasurement (drop).failureCodes
               == juce::StringArray { hw::kFailureRecordingFifoDrop });

        auto silent = makeHealthyLoopbackRecording();
        silent.captureRms = 0.0;
        CHECK (hw::evaluateRecordingMeasurement (silent).failureCodes
               == juce::StringArray { hw::kFailureRecordingSilent });

        auto badWav = makeHealthyLoopbackRecording();
        badWav.wavValid = false;
        CHECK (hw::evaluateRecordingMeasurement (badWav).failureCodes
               == juce::StringArray { hw::kFailureRecordingInvalidWav });

        auto badLink = makeHealthyLoopbackRecording();
        badLink.linkageValid = false;
        CHECK (hw::evaluateRecordingMeasurement (badLink).failureCodes
               == juce::StringArray { hw::kFailureRecordingBrokenLinkage });

        auto badHash = makeHealthyLoopbackRecording();
        badHash.hashMatch = false;
        CHECK (hw::evaluateRecordingMeasurement (badHash).failureCodes
               == juce::StringArray { hw::kFailureRecordingHashMismatch });
    }
}

TEST_CASE ("recording stage record carries honest claims through acceptance")
{
    SECTION ("full alignment pass carries the claimed value and tolerance")
    {
        const auto m = makeHealthyLoopbackRecording();
        const auto record = hw::makeRecordingStageRecord (m, hw::evaluateRecordingMeasurement (m),
                                                          "1.0.0-t", "2026-08-04T12:00:00Z",
                                                          "2026-08-04T12:01:00Z", 60000.0);
        auto accepted = hw::acceptStageDocument (hw::stageDocumentToVar (record, "run-r"),
                                                 { "run-r", hw::kStageRecording, "1.0.0-t" });
        hw::normalizeStageRecord (accepted);
        CHECK (accepted.state == hw::StageState::pass);
        CHECK (accepted.claimLevel == hw::kClaimFullAlignment);
        CHECK (static_cast<int> (accepted.measurements[hw::kMeasAlignmentFrames]) == 3);
    }
    SECTION ("capture_only pass NEVER carries an alignment value (R17), even with a correlation")
    {
        auto m = makeHealthyLoopbackRecording();
        m.inputRoute = hw::kRouteMicrophone;   // correlated, but provenance unproved
        const auto record = hw::makeRecordingStageRecord (m, hw::evaluateRecordingMeasurement (m),
                                                          "1.0.0-t", "2026-08-04T12:00:00Z",
                                                          "2026-08-04T12:01:00Z", 60000.0);
        CHECK (! record.measurements.hasProperty (hw::kMeasAlignmentFrames));
        CHECK (record.measurements.hasProperty ("correlation_offset_frames_diagnostic"));

        auto accepted = hw::acceptStageDocument (hw::stageDocumentToVar (record, "run-r"),
                                                 { "run-r", hw::kStageRecording, "1.0.0-t" });
        hw::normalizeStageRecord (accepted);
        CHECK (accepted.state == hw::StageState::pass);   // NOT rejected as invented alignment
        CHECK (accepted.claimLevel == hw::kClaimCaptureOnly);
    }
}

TEST_CASE ("tone playback fixture renders non-silence through a Track (device-free)")
{
    namespace eng = yesdaw::engine;

    auto fixture = hw::buildTonePlaybackFixture (48000.0, 128, 0.25);
    REQUIRE (fixture.ok);
    REQUIRE (fixture.project.tracks.size() == 1);

    eng::OfflineRenderOptions options;
    options.maxBlockSize = 128;

    SECTION ("with its Track the Project renders the tone")
    {
        auto created = eng::PlaybackEngine::create (
            fixture.project,
            std::span<const eng::DecodedAssetAudio> (fixture.decodedAssets.data(),
                                                     fixture.decodedAssets.size()),
            options);
        REQUIRE (created.ok());

        std::vector<float> left (128, 0.0f), right (128, 0.0f);
        std::array<float*, 2> channels { left.data(), right.data() };
        double sumSq = 0.0;
        std::size_t count = 0;
        for (int block = 0; block < 32; ++block)
        {
            created.engine->processBlock (channels.data(), 2, 128);
            for (const float sample : left)
            {
                sumSq += static_cast<double> (sample) * static_cast<double> (sample);
                ++count;
            }
        }
        const double rms = std::sqrt (sumSq / static_cast<double> (count));
        INFO ("rendered rms=" << rms);
        CHECK (rms >= hw::kPlaybackMinOutputRms);
    }
    SECTION ("track-less, the same Project cannot even build an engine (the 2026-07-27 bug class)")
    {
        auto broken = hw::buildTonePlaybackFixture (48000.0, 128, 0.25);
        REQUIRE (broken.ok);
        broken.project.tracks.clear();
        auto created = eng::PlaybackEngine::create (
            broken.project,
            std::span<const eng::DecodedAssetAudio> (broken.decodedAssets.data(),
                                                     broken.decodedAssets.size()),
            options);
        CHECK (! created.ok());
    }
    SECTION ("degenerate parameters are reported, not rendered")
    {
        CHECK (! hw::buildTonePlaybackFixture (0.0, 128, 0.25).ok);
        CHECK (! hw::buildTonePlaybackFixture (48000.0, 0, 0.25).ok);
        CHECK (! hw::buildTonePlaybackFixture (48000.0, 128, 0.0).ok);
    }
}

TEST_CASE ("committed fixtures replay to their expected verdicts (lockstep with -SelfTest)")
{
    const juce::File fixtureDir { juce::String { YESDAW_HWVERIFY_FIXTURE_DIR } };
    REQUIRE (fixtureDir.isDirectory());

    // Exact-set check: a fixture added on only one side of the C++/PowerShell mirror fails here.
    auto files = fixtureDir.findChildFiles (juce::File::findFiles, false, "*.json");
    juce::StringArray found;
    for (const auto& f : files)
        found.add (f.getFileNameWithoutExtension());
    found.sort (false);
    juce::StringArray wanted;
    for (const auto& name : kFixtureNames)
        wanted.add (juce::String { name });
    REQUIRE (found == wanted);

    for (const auto& name : kFixtureNames)
    {
        INFO ("fixture: " << name);
        const juce::File file = fixtureDir.getChildFile (juce::String { name } + ".json");
        const auto parsed = hw::parseJsonText (file.loadFileAsString());
        REQUIRE (parsed.has_value());
        const juce::var& fixture = *parsed;

        const juce::String runId = fixture["run_id"].toString();
        const juce::String checkerVersion = fixture["checker_version"].toString();
        REQUIRE (runId.isNotEmpty());
        REQUIRE (checkerVersion.isNotEmpty());

        REQUIRE (fixture["children"].isArray());
        std::vector<hw::StageRecord> records;
        for (const auto& child : *fixture["children"].getArray())
            records.push_back (recordFromChild (child, runId, checkerVersion));

        const auto verdict = hw::aggregateStages (records);

        const juce::var expected = fixture["expected"];
        REQUIRE (expected.isObject());
        CHECK (juce::String { hw::toString (verdict.overall) } == expected["overall_state"].toString());
        CHECK (verdict.exitCode == static_cast<int> (expected["exit_code"]));

        const juce::var expectedStates = expected["stage_states"];
        REQUIRE (expectedStates.isObject());
        for (const auto& prop : expectedStates.getDynamicObject()->getProperties())
        {
            const auto* record = findRecord (records, prop.name.toString());
            REQUIRE (record != nullptr);
            INFO ("stage: " << prop.name.toString().toStdString());
            CHECK (juce::String { hw::toString (record->state) } == prop.value.toString());
        }

        juce::StringArray expectedCodes;
        REQUIRE (expected["failure_codes"].isArray());
        for (const auto& code : *expected["failure_codes"].getArray())
            expectedCodes.add (code.toString());
        CHECK (verdict.failureCodes == expectedCodes);

        if (expected.hasProperty ("stage_claims"))
        {
            for (const auto& prop : expected["stage_claims"].getDynamicObject()->getProperties())
            {
                const auto* record = findRecord (records, prop.name.toString());
                REQUIRE (record != nullptr);
                CHECK (record->claimLevel == prop.value.toString());
            }
        }
    }
}
