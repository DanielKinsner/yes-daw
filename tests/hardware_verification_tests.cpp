// YES DAW — deterministic, device-free tests for the packaged hardware-verification contract
// (H17 packaged verifier U1): schema v1, child-document acceptance, the pass-record consistency
// policy, KTD11 aggregation, locale-invariant JSON, and atomic replacement.
//
// The fixture sweep replays tests/fixtures/hardware-verification/*.json — the SAME files that
// tools/verify-hardware.ps1 -SelfTest replays with its PowerShell reimplementation of this policy.
// Both harnesses assert the exact fixture set, so policy drift between C++ and the packaged script
// fails mechanically instead of silently.

#include "app/HardwareVerification.h"

#include <catch2/catch_test_macros.hpp>

#include <clocale>
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

    CHECK (hw::kSchemaVersion == 1);
    CHECK (hw::kRequiredSampleRateHz == 48000.0);
    CHECK (hw::kMaxGrantedBlockFrames == 128);
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
