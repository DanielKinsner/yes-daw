// YES DAW — headless self-check console app (H17 CP1).
//
//   YesDawSelfCheck --selfcheck <bundle.yesdaw>   open + validate + render -> PASS/FAIL, exit 0/1
//   YesDawSelfCheck --version                      print the git-describe build version, exit 0
//
// The machine-independent proof that a build can open AND render a real Project. No audio device, no
// display — so it runs in CI on all three OSes and against a packaged exe on a clean machine (CP3).
// See docs/plans/2026-07-03-h17-distribution-alpha-plan.md and -h17-cp1-selfcheck-notes.md.
//
// Slices (built incrementally, each CI-verified):
//   1 (done): open + read snapshot + validate (hasValidEntityIds / hasValidAssetClipIndirection).
//   2 (here): decode the bundle's mono assets and run renderOfflineProject; assert the render
//             succeeds with finite, non-empty output. Reuses the H7 offline path exactly as
//             tests/bundle_render_tests.cpp decodes assets (JUCE WavAudioFormat) and renders.
//   3 (next): write the render to a temp WAV, re-read it, assert a bit-exact round-trip, then delete.
//
// The plan's negative control ("a corrupted/invalid bundle is rejected by validators") is covered by
// the YesDawSelfCheckRejectsNonBundle ctest: openExistingBundle fails on a non-bundle path -> exit 1.
//
// Implemented as a dedicated console app (mirroring tools/soak/SoakMain.cpp) rather than a
// `YesDaw --selfcheck` GUI-exe mode: a Windows GUI-subsystem exe cannot reliably print to a parent
// console. Decision flagged in STATUS.md / the CP1 note (implementer-brief hard-stop #9).

#include "engine/OfflineRenderer.h"
#include "engine/Project.h"
#include "persistence/ProjectBundle.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
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

// Decode every (mono) Asset in the Project from the bundle's audio/ files into DecodedAssetAudio.
// The decoded samples live in `storage`; DecodedAssetAudio holds a std::span into them, so `storage`
// is reserved up front (no reallocation) and must outlive `out`'s use in renderOfflineProject.
bool decodeBundleAssets (const yesdaw::engine::Project& project,
                         const std::filesystem::path& bundlePath,
                         std::vector<std::vector<float>>& storage,
                         std::vector<yesdaw::engine::DecodedAssetAudio>& out,
                         std::string& err)
{
    namespace engine = yesdaw::engine;

    storage.reserve (project.assets.size());
    out.reserve (project.assets.size());

    juce::WavAudioFormat wav;

    for (const engine::Asset& asset : project.assets)
    {
        if (asset.channels != 1u)
        {
            err = "asset is not mono (offline render requires mono assets)";
            return false;
        }
        if (asset.frames == 0
            || asset.frames > static_cast<std::uint64_t> (std::numeric_limits<int>::max()))
        {
            err = "asset frame count out of range";
            return false;
        }

        const std::filesystem::path assetPath =
            bundlePath / yesdaw::persistence::detail::assetRelativePathForHash (asset.contentHash);

        const juce::File file { juce::String { assetPath.string() } };
        std::unique_ptr<juce::AudioFormatReader> reader (
            wav.createReaderFor (new juce::FileInputStream (file), true));

        if (reader == nullptr)
        {
            err = "could not open asset audio file";
            return false;
        }
        if (reader->numChannels != asset.channels
            || reader->lengthInSamples != static_cast<juce::int64> (asset.frames))
        {
            err = "asset audio metadata mismatch";
            return false;
        }

        const int frames = static_cast<int> (asset.frames);
        juce::AudioBuffer<float> buffer (1, frames);
        if (! reader->read (&buffer, 0, frames, 0, true, false))
        {
            err = "asset audio decode failed";
            return false;
        }

        const float* const channel = buffer.getReadPointer (0);
        storage.emplace_back (channel, channel + frames);

        engine::DecodedAssetAudio decoded;
        decoded.assetId = asset.id;
        decoded.sampleRate = asset.sampleRate;
        decoded.frames = asset.frames;
        decoded.channels = asset.channels;
        decoded.interleavedSamples =
            std::span<const float> (storage.back().data(), storage.back().size());
        out.push_back (decoded);
    }

    return true;
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

    // Slice 2: decode assets and render the whole timeline through the H7 offline path.
    std::vector<std::vector<float>> assetStorage;
    std::vector<engine::DecodedAssetAudio> decodedAssets;
    std::string decodeErr;
    if (! decodeBundleAssets (project, bundlePath, assetStorage, decodedAssets, decodeErr))
        return fail (decodeErr.c_str());

    const engine::OfflineRenderResult render = engine::renderOfflineProject (project, decodedAssets);
    if (! render.ok())
    {
        const std::string msg = "render failed (status "
                              + std::to_string (static_cast<int> (render.status)) + ")";
        return fail (msg.c_str());
    }
    if (render.frames == 0)
        return fail ("render produced zero frames");

    std::printf ("SELFCHECK PASS: %s (sr=%.0f, assets=%zu, clips=%zu, midiClips=%zu, "
                 "rendered %llu frames x %u ch)\n",
                 bundlePath.string().c_str(),
                 static_cast<double> (project.sampleRate.hz),
                 project.assets.size(),
                 project.clips.size(),
                 project.midiClips.size(),
                 static_cast<unsigned long long> (render.frames),
                 static_cast<unsigned> (render.channels));
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
