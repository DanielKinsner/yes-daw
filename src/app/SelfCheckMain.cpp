// YES DAW — headless self-check console app (H17 CP1, slice 1: load + validate).
//
//   YesDawSelfCheck --selfcheck <bundle.yesdaw>   open + read snapshot + validate -> PASS/FAIL, 0/1
//   YesDawSelfCheck --version                      print the git-describe build version, 0
//
// The machine-independent proof that a build can open a real Project. No audio device, no display —
// so it runs in CI on all three OSes and against a packaged exe on a clean machine (CP3). Render-N-
// blocks (slice 2) and export-to-temp round-trip (slice 3) build on this; see
// docs/plans/2026-07-03-h17-distribution-alpha-plan.md and
// docs/plans/2026-07-13-h17-cp1-selfcheck-notes.md.
//
// Slice 1 deliberately stops at load + validate so it depends only on persistence + engine (no
// decode/format/DSP). The plan's named negative control — "a corrupted-fixture copy is rejected by
// validators" — is satisfied here: a corrupt project.db fails readProjectSnapshot; a malformed
// Project fails hasValidEntityIds / hasValidAssetClipIndirection.
//
// Implemented as a dedicated console app (mirroring tools/soak/SoakMain.cpp) rather than a
// `YesDaw --selfcheck` GUI-exe mode: a Windows GUI-subsystem exe cannot reliably print to a parent
// console. Decision flagged for Dan in STATUS.md / the CP1 note (implementer-brief hard-stop #9).
//
// DRAFTED on branch vera/h17 (Vera, Fable session, 2026-07-13) — authored from a Linux box that
// cannot build JUCE, so NOT yet compiled. Uses only surfaces verified against the tree
// (tests/bundle_render_tests.cpp uses the same open/readSnapshot path). Build-verify before merge.

#include "engine/Project.h"
#include "persistence/ProjectBundle.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifndef YESDAW_VERSION_STRING
  #define YESDAW_VERSION_STRING "0.0.0-dev"
#endif

namespace {

int printUsage()
{
    std::puts ("usage: YesDawSelfCheck --selfcheck <bundle.yesdaw>");
    std::puts ("       YesDawSelfCheck --version");
    return 2;
}

int printVersion()
{
    std::printf ("YesDaw %s\n", YESDAW_VERSION_STRING);
    return 0;
}

int fail (const char* reason)
{
    std::printf ("SELFCHECK FAIL: %s\n", reason);
    return 1;
}

int runSelfCheck (const std::filesystem::path& bundlePath)
{
    namespace persistence = yesdaw::persistence;
    namespace engine = yesdaw::engine;

    std::error_code ec;
    if (! std::filesystem::exists (bundlePath, ec))
        return fail ("bundle path does not exist");

    persistence::ProjectBundleDb db;
    if (! persistence::ProjectBundleDb::openExistingBundle (bundlePath, db).ok())
        return fail ("could not open bundle (missing/corrupt project.db)");

    engine::Project project;
    if (! db.readProjectSnapshot (project).ok())
        return fail ("could not read project snapshot");

    if (! project.hasValidEntityIds())
        return fail ("project has invalid entity ids");

    if (! project.hasValidAssetClipIndirection())
        return fail ("project has invalid asset/clip indirection");

    std::printf ("SELFCHECK PASS: %s (sr=%.0f, assets=%zu, clips=%zu, midiClips=%zu)\n",
                 bundlePath.string().c_str(),
                 static_cast<double> (project.sampleRate.hz),
                 project.assets.size(),
                 project.clips.size(),
                 project.midiClips.size());
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
                return fail ("--selfcheck requires a <bundle> path");

            return runSelfCheck (std::filesystem::path (std::string (args[i + 1])));
        }
    }

    return printUsage();
}
