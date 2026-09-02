// YES DAW - tools/fixtures: build the G0.6 song fixture on disk (the 16-track three-minute song
// the Session drive and the feel budgets run against). Deterministic and hash-stable; never
// committed as audio (see src/app/SongFixture.h).
//
//   YesDawMakeSongFixture --out <dir> [--tracks 16] [--seconds 180] [--rate 48000] [--midi 4]
//
// Prints the stem and placement hashes and the bundle path; exit 0 only when the bundle was
// written and reopens.

#include "app/SongFixture.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

int main (int argc, char** argv)
{
    std::filesystem::path outDir;
    yesdaw::app::fixture::SongFixtureSpec spec;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const bool hasValue = i + 1 < argc;
        if (arg == "--out" && hasValue)           outDir = argv[++i];
        else if (arg == "--tracks" && hasValue)   spec.tracks = std::atoi (argv[++i]);
        else if (arg == "--seconds" && hasValue)  spec.seconds = std::atof (argv[++i]);
        else if (arg == "--rate" && hasValue)     spec.sampleRateHz = static_cast<std::uint32_t> (std::atoi (argv[++i]));
        else if (arg == "--midi" && hasValue)     spec.midiTracks = std::atoi (argv[++i]);
        else
        {
            std::fprintf (stderr, "usage: YesDawMakeSongFixture --out <dir> [--tracks N] [--seconds S] [--rate HZ] [--midi N]\n");
            return 2;
        }
    }
    if (outDir.empty())
    {
        std::fprintf (stderr, "--out <dir> is required\n");
        return 2;
    }

    const yesdaw::app::fixture::SongFixtureResult result = yesdaw::app::fixture::buildSongFixture (outDir, spec);
    if (! result.ok)
    {
        std::fprintf (stderr, "FAIL: %s\n", result.error.c_str());
        return 1;
    }

    // Reopen to prove the bundle is a bundle.
    yesdaw::persistence::ProjectBundleDb db;
    if (auto opened = yesdaw::persistence::ProjectBundleDb::openExistingBundle (result.bundlePath, db); ! opened.ok())
    {
        std::fprintf (stderr, "FAIL: reopen: %s\n", opened.message.c_str());
        return 1;
    }
    yesdaw::engine::Project project;
    if (auto read = db.readProjectSnapshot (project); ! read.ok())
    {
        std::fprintf (stderr, "FAIL: snapshot: %s\n", read.message.c_str());
        return 1;
    }

    std::printf ("bundle=%s\n", result.bundlePath.string().c_str());
    std::printf ("firstStem=%s\n", result.stemPaths.empty() ? "" : result.stemPaths.front().string().c_str());
    std::printf ("tracks=%zu clips=%zu midiClips=%zu notes=%zu stemFrames=%llu\n",
                 project.tracks.size(), project.clips.size(), project.midiClips.size(), result.noteCount,
                 static_cast<unsigned long long> (result.stemFrames));
    std::printf ("stemHash=%016llx projectHash=%016llx\n",
                 static_cast<unsigned long long> (result.stemHash),
                 static_cast<unsigned long long> (result.projectHash));
    std::printf ("PASS\n");
    return 0;
}
