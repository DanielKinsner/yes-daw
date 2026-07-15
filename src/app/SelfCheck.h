// YES DAW — self-check logic (H17 CP1), shared by the YesDawSelfCheck CLI and its tests.
//
// Open a .yesdaw bundle → read snapshot → validate → decode mono assets → renderOfflineProject.
// Pure: returns a SelfCheckResult and prints NOTHING (the CLI does the printing). Lifting this out
// of main() lets a Catch2 test generate a real bundle and assert the render path, since the
// committed .yesdaw fixtures ship stub (non-audio) asset files.
//
// Reuses the H7 offline path exactly as tests/bundle_render_tests.cpp does (JUCE WavAudioFormat
// decode + renderOfflineProject). See docs/plans/2026-07-13-h17-cp1-selfcheck-notes.md.

#pragma once

#include "analysis/LoudnessMeter.h"
#include "engine/OfflineRenderer.h"
#include "engine/Project.h"
#include "io/WavFile.h"
#include "persistence/ProjectBundle.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace yesdaw::app {

struct SelfCheckResult
{
    bool          ok = false;
    std::string   message;              // failure reason, or "ok"
    std::size_t   assetCount = 0;
    std::size_t   clipCount = 0;
    std::size_t   midiClipCount = 0;
    double        sampleRateHz = 0.0;
    std::uint64_t renderedFrames = 0;
    std::uint16_t renderedChannels = 0;
    std::uint64_t exportedFrames = 0;   // frames written to WAV and re-imported bit-exact
};

namespace detail {

// Decode every (mono) Asset into DecodedAssetAudio. The decoded samples live in `storage`, which is
// reserved up front (no reallocation) so the std::span each DecodedAssetAudio holds stays valid.
[[nodiscard]] inline bool decodeBundleAssets (const engine::Project& project,
                                              const std::filesystem::path& bundlePath,
                                              std::vector<std::vector<float>>& storage,
                                              std::vector<engine::DecodedAssetAudio>& out,
                                              std::string& err)
{
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
            bundlePath / persistence::detail::assetRelativePathForHash (asset.contentHash);

        const juce::File file { juce::String { assetPath.string() } };
        std::unique_ptr<juce::AudioFormatReader> reader (
            wav.createReaderFor (new juce::FileInputStream (file), true));

        if (reader == nullptr)
        {
            err = "could not open asset audio file";
            return false;
        }
        // Validate the decoded audio against the STORED metadata, sample rate included, so a swapped
        // asset can't slip through (mirrors the read-back checks in tests/bundle_render_tests.cpp).
        if (reader->sampleRate != asset.sampleRate.hz
            || reader->numChannels != asset.channels
            || reader->lengthInSamples != static_cast<juce::int64> (asset.frames))
        {
            err = "asset audio metadata mismatch (sample rate / channels / length)";
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

} // namespace detail

// Open + validate + render a bundle. Never throws for the expected failure modes; reports them in
// `SelfCheckResult::message` with ok == false.
[[nodiscard]] inline SelfCheckResult runSelfCheck (const std::filesystem::path& bundlePath)
{
    SelfCheckResult r;

    std::error_code ec;
    if (! std::filesystem::exists (bundlePath, ec))
    {
        r.message = "bundle path does not exist";
        return r;
    }

    persistence::ProjectBundleDb db;
    if (! persistence::ProjectBundleDb::openExistingBundle (bundlePath, db).ok())
    {
        r.message = "could not open bundle (missing/corrupt project.db)";
        return r;
    }

    engine::Project project;
    if (! db.readProjectSnapshot (project).ok())
    {
        r.message = "could not read project snapshot";
        return r;
    }
    if (! project.hasValidEntityIds())
    {
        r.message = "project has invalid entity ids";
        return r;
    }
    if (! project.hasValidAssetClipIndirection())
    {
        r.message = "project has invalid asset/clip indirection";
        return r;
    }

    r.assetCount = project.assets.size();
    r.clipCount = project.clips.size();
    r.midiClipCount = project.midiClips.size();
    r.sampleRateHz = static_cast<double> (project.sampleRate.hz);

    std::vector<std::vector<float>> assetStorage;
    std::vector<engine::DecodedAssetAudio> decodedAssets;
    std::string decodeErr;
    if (! detail::decodeBundleAssets (project, bundlePath, assetStorage, decodedAssets, decodeErr))
    {
        r.message = decodeErr;
        return r;
    }

    const engine::OfflineRenderResult render = engine::renderOfflineProject (project, decodedAssets);
    if (! render.ok())
    {
        r.message = "render failed (status " + std::to_string (static_cast<int> (render.status)) + ")";
        return r;
    }
    if (render.frames == 0)
    {
        r.message = "render produced zero frames";
        return r;
    }

    r.renderedFrames = render.frames;
    r.renderedChannels = render.channels;

    // Export the render to a temp WAV, read it back, and assert a bit-exact round-trip (the H7 gate
    // pattern) — proving the full open -> validate -> render -> export -> re-import chain. Canonical
    // float32 WAV preserves exact bits for finite samples (the renderer guarantees finite), so an
    // exact != comparison is correct here.
    const std::filesystem::path exportPath =
        std::filesystem::temp_directory_path()
        / ("yesdaw-selfcheck-export-" + bundlePath.filename().string() + ".wav");

    const io::WavResult wrote = io::writeFloat32WavFile (
        exportPath, project.sampleRate, render.channels, render.frames,
        std::span<const float> (render.interleavedSamples.data(), render.interleavedSamples.size()));
    if (! wrote.ok())
    {
        std::error_code rmec;
        std::filesystem::remove (exportPath, rmec);
        r.message = "export write failed: " + wrote.message;
        return r;
    }

    io::Float32Wav reimported;
    const io::WavResult readBack = io::readFloat32WavFile (exportPath, reimported);
    std::error_code rmec;
    std::filesystem::remove (exportPath, rmec);
    if (! readBack.ok())
    {
        r.message = "export re-read failed: " + readBack.message;
        return r;
    }

    if (reimported.channels != render.channels
        || reimported.frames != render.frames
        || reimported.interleavedSamples.size() != render.interleavedSamples.size())
    {
        r.message = "export round-trip shape mismatch";
        return r;
    }
    for (std::size_t i = 0; i < render.interleavedSamples.size(); ++i)
    {
        if (reimported.interleavedSamples[i] != render.interleavedSamples[i])
        {
            r.message = "export round-trip not bit-exact";
            return r;
        }
    }

    r.exportedFrames = render.frames;
    r.ok = true;
    r.message = "ok";
    return r;
}

// --- H17 CP5 (alpha-verify) — WAV round-trip check -----------------------------------------------
// Proves an exported WAV is well-formed AND reversible (the H7 gate pattern) against an EXTERNAL
// file: read it, write it back through the canonical float32 writer, re-read, and assert bit-exact.
// Used by the alpha-verify "export re-imports bit-exact" assert on the mix a real session produced.
struct WavVerifyResult
{
    bool          ok = false;
    std::string   message;        // failure reason, or "ok"
    std::uint16_t channels = 0;
    std::uint64_t frames = 0;
    double        sampleRateHz = 0.0;
};

[[nodiscard]] inline WavVerifyResult verifyWavRoundTrip (const std::filesystem::path& wavPath)
{
    WavVerifyResult r;

    std::error_code ec;
    if (! std::filesystem::exists (wavPath, ec))
    {
        r.message = "wav path does not exist";
        return r;
    }

    io::Float32Wav original;
    const io::WavResult read1 = io::readFloat32WavFile (wavPath, original);
    if (! read1.ok())
    {
        r.message = "could not read wav: " + read1.message;
        return r;
    }
    if (original.channels == 0 || original.frames == 0)
    {
        r.message = "wav is empty (zero channels or frames)";
        return r;
    }

    const std::filesystem::path tempPath =
        std::filesystem::temp_directory_path()
        / ("yesdaw-verifywav-" + wavPath.filename().string() + ".roundtrip.wav");

    const io::WavResult wrote = io::writeFloat32WavFile (
        tempPath, original.sampleRate, original.channels, original.frames,
        std::span<const float> (original.interleavedSamples.data(), original.interleavedSamples.size()));
    if (! wrote.ok())
    {
        std::filesystem::remove (tempPath, ec);
        r.message = "round-trip write failed: " + wrote.message;
        return r;
    }

    io::Float32Wav reread;
    const io::WavResult read2 = io::readFloat32WavFile (tempPath, reread);
    std::filesystem::remove (tempPath, ec);
    if (! read2.ok())
    {
        r.message = "round-trip re-read failed: " + read2.message;
        return r;
    }

    if (reread.channels != original.channels
        || reread.frames != original.frames
        || reread.interleavedSamples.size() != original.interleavedSamples.size())
    {
        r.message = "round-trip shape mismatch";
        return r;
    }
    for (std::size_t i = 0; i < original.interleavedSamples.size(); ++i)
    {
        if (reread.interleavedSamples[i] != original.interleavedSamples[i])
        {
            r.message = "round-trip not bit-exact";
            return r;
        }
    }

    r.channels = original.channels;
    r.frames = original.frames;
    r.sampleRateHz = static_cast<double> (original.sampleRate.hz);
    r.ok = true;
    r.message = "ok";
    return r;
}

// --- H17 CP5 (alpha-verify) — integrated loudness read ------------------------------------------
// Report a WAV's integrated loudness (LUFS, BS.1770 / EBU R128 via the shared LoudnessMeter) so
// alpha-verify can assert the produced mix is neither silent nor clipped-to-hell. `ok` means the
// measurement succeeded — it is NOT a range verdict; the caller (alpha-verify) owns the range.
struct LoudnessCheckResult
{
    bool          ok = false;
    std::string   message;        // failure reason, or "ok"
    double        integratedLufs = 0.0;
    std::uint16_t channels = 0;
    std::uint64_t frames = 0;
    double        sampleRateHz = 0.0;
};

[[nodiscard]] inline LoudnessCheckResult measureLoudness (const std::filesystem::path& wavPath)
{
    LoudnessCheckResult r;

    std::error_code ec;
    if (! std::filesystem::exists (wavPath, ec))
    {
        r.message = "wav path does not exist";
        return r;
    }

    io::Float32Wav wav;
    const io::WavResult read = io::readFloat32WavFile (wavPath, wav);
    if (! read.ok())
    {
        r.message = "could not read wav: " + read.message;
        return r;
    }
    if (wav.channels == 0 || wav.frames == 0)
    {
        r.message = "wav is empty (zero channels or frames)";
        return r;
    }

    std::uint32_t sampleRateHz = 0;
    if (! io::detail::sampleRateToUint32 (wav.sampleRate, sampleRateHz))
    {
        r.message = "wav sample rate is not a usable integer";
        return r;
    }

    const analysis::LoudnessResult loudness = analysis::analyzeInterleavedLoudness (
        std::span<const float> (wav.interleavedSamples.data(), wav.interleavedSamples.size()),
        static_cast<std::uint32_t> (wav.channels),
        sampleRateHz);

    if (loudness.status != analysis::LoudnessStatus::Ok)
    {
        r.message = "loudness analysis failed (status "
                    + std::to_string (static_cast<int> (loudness.status)) + ")";
        return r;
    }

    r.integratedLufs = loudness.metrics.integratedLufs;
    r.channels = wav.channels;
    r.frames = wav.frames;
    r.sampleRateHz = static_cast<double> (wav.sampleRate.hz);
    r.ok = true;
    r.message = "ok";
    return r;
}

} // namespace yesdaw::app
