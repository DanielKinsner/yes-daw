// YES DAW — headless self-check console app (H17 CP1). Thin CLI over yesdaw::app::runSelfCheck.
//
//   YesDawSelfCheck --selfcheck <bundle.yesdaw>   open + validate + render -> PASS/FAIL, exit 0/1
//   YesDawSelfCheck --verify-wav <file.wav>        WAV round-trips bit-exact -> PASS/FAIL, exit 0/1
//   YesDawSelfCheck --loudness <file.wav>          print integrated LUFS (measure ok -> exit 0/1)
//   YesDawSelfCheck --version                      print the git-describe build version, exit 0
//
// The self-check logic lives in src/app/SelfCheck.h so a Catch2 test (tests/selfcheck_tests.cpp)
// can generate a real bundle and assert the render path — the committed .yesdaw fixtures ship stub
// (non-audio) asset files, so a full render can only be PASS-tested against a generated bundle.
//
// Console app (mirrors tools/soak/SoakMain.cpp) rather than a `YesDaw --selfcheck` GUI-exe mode: a
// Windows GUI-subsystem exe cannot reliably print to a parent console. Flagged in STATUS.md /
// docs/plans/2026-07-13-h17-cp1-selfcheck-notes.md (implementer-brief hard-stop #9).

#include "app/SelfCheck.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#ifndef YESDAW_VERSION_STRING
  #define YESDAW_VERSION_STRING "0.0.0-dev"
#endif

namespace {

int printUsage()
{
    std::puts ("usage: YesDawSelfCheck --selfcheck <bundle.yesdaw>");
    std::puts ("       YesDawSelfCheck --verify-wav <file.wav>");
    std::puts ("       YesDawSelfCheck --loudness <file.wav>");
    std::puts ("       YesDawSelfCheck --version");
    return 2;
}

int printVersion()
{
    std::printf ("YesDaw %s\n", YESDAW_VERSION_STRING);
    return 0;
}

int runSelfCheckCli (const std::string& bundle)
{
    const yesdaw::app::SelfCheckResult r = yesdaw::app::runSelfCheck (std::filesystem::path (bundle));
    if (! r.ok)
    {
        std::printf ("SELFCHECK FAIL: %s\n", r.message.c_str());
        return 1;
    }

    std::printf ("SELFCHECK PASS: %s (sr=%.0f, assets=%zu, clips=%zu, midiClips=%zu, "
                 "rendered %llu frames x %u ch, exported+reimported %llu frames bit-exact)\n",
                 bundle.c_str(),
                 r.sampleRateHz,
                 r.assetCount,
                 r.clipCount,
                 r.midiClipCount,
                 static_cast<unsigned long long> (r.renderedFrames),
                 static_cast<unsigned> (r.renderedChannels),
                 static_cast<unsigned long long> (r.exportedFrames));
    return 0;
}

int runVerifyWavCli (const std::string& wav)
{
    const yesdaw::app::WavVerifyResult r = yesdaw::app::verifyWavRoundTrip (std::filesystem::path (wav));
    if (! r.ok)
    {
        std::printf ("VERIFYWAV FAIL: %s\n", r.message.c_str());
        return 1;
    }

    std::printf ("VERIFYWAV PASS: %s (sr=%.0f, %llu frames x %u ch, round-trips bit-exact)\n",
                 wav.c_str(),
                 r.sampleRateHz,
                 static_cast<unsigned long long> (r.frames),
                 static_cast<unsigned> (r.channels));
    return 0;
}

int runLoudnessCli (const std::string& wav)
{
    const yesdaw::app::LoudnessCheckResult r = yesdaw::app::measureLoudness (std::filesystem::path (wav));
    if (! r.ok)
    {
        std::printf ("LOUDNESS FAIL: %s\n", r.message.c_str());
        return 1;
    }

    // Print the value; alpha-verify parses it and applies the -30..-6 LUFS range verdict.
    std::printf ("LOUDNESS OK: %s (integrated %.2f LUFS, sr=%.0f, %llu frames x %u ch)\n",
                 wav.c_str(),
                 r.integratedLufs,
                 r.sampleRateHz,
                 static_cast<unsigned long long> (r.frames),
                 static_cast<unsigned> (r.channels));
    return 0;
}

} // namespace

int main (int argc, char** argv)
{
    const std::vector<std::string_view> args (argv + 1, argv + argc);

    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--version")
            return printVersion();

        if (args[i] == "--selfcheck")
        {
            if (i + 1 >= args.size())
            {
                std::printf ("SELFCHECK FAIL: --selfcheck requires a <bundle> path\n");
                return 1;
            }

            return runSelfCheckCli (std::string (args[i + 1]));
        }

        if (args[i] == "--verify-wav")
        {
            if (i + 1 >= args.size())
            {
                std::printf ("VERIFYWAV FAIL: --verify-wav requires a <file.wav> path\n");
                return 1;
            }

            return runVerifyWavCli (std::string (args[i + 1]));
        }

        if (args[i] == "--loudness")
        {
            if (i + 1 >= args.size())
            {
                std::printf ("LOUDNESS FAIL: --loudness requires a <file.wav> path\n");
                return 1;
            }

            return runLoudnessCli (std::string (args[i + 1]));
        }
    }

    return printUsage();
}
