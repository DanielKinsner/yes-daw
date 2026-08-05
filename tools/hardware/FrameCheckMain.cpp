// YES DAW — packaged headless dense-Timeline frame checker (H17 packaged verifier, U2).
//
//   YesDawFrameCheck [--run-id <id>] [--json-out <path>]   run the owner measurement, exit 0/1/2
//   YesDawFrameCheck --version                             print the git-describe build version, exit 0
//
// Thin shell: the measurement lives in src/ui/TimelineFrameCheck.h (shared with the H11 Catch2
// gate) and the FIXED owner verdict policy + stage-document schema live in
// src/app/HardwareVerification.h. This binary only wires them together, prints one summary line,
// and optionally writes the schema-v1 stage JSON atomically for the verify-hardware.ps1
// orchestrator (U5). It never reads ambient CI and has no flag that relaxes a threshold.
//
// Exit codes mirror the per-stage meaning of R10: 0 = the owner policy passed, 1 = a completed
// measurement violated it, 2 = the run could not produce a complete result (bad usage, write
// failure). The orchestrator treats the JSON document, not the exit code, as the evidence.
//
// The claim is headless_dense_timeline — the accepted offscreen proxy, never a window/GPU proof.

#include "app/HardwareVerification.h"
#include "ui/TimelineFrameCheck.h"

#include <cstdio>
#include <string>

#ifndef YESDAW_VERSION_STRING
  #define YESDAW_VERSION_STRING "0.0.0-dev"
#endif

namespace hw = yesdaw::app::hardware;

namespace {

int printUsage()
{
    std::puts ("usage: YesDawFrameCheck [--run-id <id>] [--json-out <path>]");
    std::puts ("       YesDawFrameCheck --version");
    return 2;
}

const char* argValue (int argc, char** argv, const char* name)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string (argv[i]) == name)
            return argv[i + 1];
    return nullptr;
}

bool hasFlag (int argc, char** argv, const char* name)
{
    for (int i = 1; i < argc; ++i)
        if (std::string (argv[i]) == name)
            return true;
    return false;
}

} // namespace

int main (int argc, char** argv)
{
    if (hasFlag (argc, argv, "--version"))
    {
        std::printf ("YesDawFrameCheck %s\n", YESDAW_VERSION_STRING);
        return 0;
    }
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--run-id" || arg == "--json-out") { ++i; continue; }
        return printUsage();
    }

    const char* runIdArg = argValue (argc, argv, "--run-id");
    const char* jsonOutArg = argValue (argc, argv, "--json-out");
    const juce::String runId = runIdArg != nullptr ? juce::String { runIdArg }
                                                   : juce::String { "standalone" };

    const juce::String startedAt = juce::Time::getCurrentTime().toISO8601 (true);
    const auto t0 = juce::Time::getMillisecondCounterHiRes();

    // The owner measurement: the exact dense H11 fixture, judged by the fixed policy. No flag and
    // no environment variable changes either.
    const yesdaw::ui::TimelineFrameCheckConfig config;
    const yesdaw::ui::TimelineFrameCheckResult run = yesdaw::ui::runTimelineFrameCheck (config);

    hw::FrameMeasurement measurement;
    measurement.sustainedFrameMs = yesdaw::ui::sustainedFrameMs (run.frameTimesMs,
                                                                 hw::kFrameAllowedOutlierFrames);
    measurement.maxFrameMs = run.maxFrameMs;
    measurement.slowFrameCount = yesdaw::ui::countFramesAtOrOverBudget (run.frameTimesMs,
                                                                        hw::kFrameBudgetMs);
    measurement.measuredFrames = static_cast<int> (run.frameTimesMs.size());
    measurement.maxVisibleClips = run.maxVisibleClips;
    measurement.distinctSamples = run.distinctSamples;
    measurement.hitVisibleClipCapacity = run.hitVisibleClipCapacity;
    measurement.checksum = run.checksum;
    measurement.totalClips = run.totalClips;

    const hw::FrameVerdict verdict = hw::evaluateFrameMeasurement (measurement);

    const double durationMs = juce::Time::getMillisecondCounterHiRes() - t0;
    const juce::String completedAt = juce::Time::getCurrentTime().toISO8601 (true);

    const hw::StageRecord record = hw::makeFrameStageRecord (measurement, verdict,
                                                             YESDAW_VERSION_STRING,
                                                             startedAt, completedAt, durationMs);
    const juce::var document = hw::stageDocumentToVar (record, runId);

    if (jsonOutArg != nullptr)
    {
        std::string error;
        if (! hw::writeJsonAtomically (std::filesystem::path { jsonOutArg }, document, error))
        {
            std::printf ("FRAME ERROR: could not write result JSON: %s\n", error.c_str());
            return 2;
        }
    }

    juce::String codes;
    for (const auto& code : verdict.failureCodes)
        codes += (codes.isEmpty() ? "" : ",") + code;

    std::printf ("FRAME %s: claim=%s sustained_ms=%.3f max_ms=%.3f slow=%d/%d visible=%d distinct=%d%s%s\n",
                 verdict.state == hw::StageState::pass ? "PASS" : "FAIL",
                 record.claimLevel.isEmpty() ? "none" : record.claimLevel.toRawUTF8(),
                 measurement.sustainedFrameMs,
                 measurement.maxFrameMs,
                 measurement.slowFrameCount,
                 measurement.measuredFrames,
                 measurement.maxVisibleClips,
                 measurement.distinctSamples,
                 codes.isEmpty() ? "" : " codes=",
                 codes.toRawUTF8());

    return verdict.state == hw::StageState::pass ? 0 : 1;
}
