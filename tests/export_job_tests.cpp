// YES DAW - ADR-0058 cp1: the export job on its worker. Pure C++ + Catch2 (no JUCE), so the TSan leg checks the
// handoff (the worker's atomics, the result published before the terminal state) and the RTSan leg builds it too.
//
// Gates: a job's file equals the synchronous render written by the same writer, byte for byte (same-rate and
// cross-rate Assets); held at its latch the job is Rendering with progress inside the render half; Cancel ends it
// Cancelled with no file; released, it succeeds at 100 %; destroying a held job cancels and joins it.

#include "app/ExportJob.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace yesdaw;

engine::EntityId idFor (std::uint8_t low)
{
    engine::EntityId id;
    id.bytes.fill (0);
    id.bytes.front() = 0x01;
    id.bytes.back() = low;
    return id;
}

std::filesystem::path scratch (const char* name)
{
    const auto path = std::filesystem::temp_directory_path()
        / ("yesdaw-export-job-" + std::string (name) + "-"
           + std::to_string (std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories (path);
    return path;
}

std::vector<float> tone (std::size_t frames, double rate)
{
    std::vector<float> out (frames);
    for (std::size_t i = 0; i < frames; ++i)
        out[i] = static_cast<float> (0.4 * std::sin (2.0 * 3.14159265358979323846 * 440.0 * static_cast<double> (i) / rate));
    return out;
}

// A 48 kHz project with one mono clip of `source` (at `sourceRate`) on one Track, and the job's snapshot of it.
struct Fixture
{
    engine::Project project;
    std::vector<float> source;
    double sourceRate = 48'000.0;

    Fixture (std::size_t frames, double rate) : source (tone (frames, rate)), sourceRate (rate)
    {
        project.id = idFor (1);
        project.sampleRate = engine::SampleRate { 48'000.0 };
        engine::Asset asset;
        asset.id = idFor (2);
        asset.contentHash.bytes.fill (7);
        asset.frames = frames;
        asset.sampleRate = engine::SampleRate { rate };
        asset.channels = 1;
        project.assets = { asset };
        engine::Track track;
        track.id = idFor (3);
        track.strip.name = "Tone";
        project.tracks = { track };
        engine::Clip clip;
        clip.id = idFor (4);
        clip.assetId = asset.id;
        clip.trackId = track.id;
        clip.srcLen = frames;
        clip.timelineLength = engine::timelineLengthForSource (frames, engine::rateRatio (rate, 48'000.0));
        project.clips = { clip };
        REQUIRE (project.hasValidAssetClipIndirection());
    }

    app::ExportSnapshot snapshot (const std::filesystem::path& destination) const
    {
        app::ExportSnapshot out;
        out.destination = destination;
        out.project = engine::projectHasCrossRateAssets (project) ? engine::projectInViewFrames (project) : project;
        app::ExportAssetAudio audio;
        audio.assetId = project.assets[0].id;
        audio.channels = 1;
        if (sourceRate != 48'000.0)
        {
            audio.source = std::make_shared<const std::vector<float>> (source);
            audio.sourceRateHz = sourceRate;
        }
        else
        {
            auto samples = std::make_shared<engine::AssetSamples>();
            samples->interleaved = source;
            samples->channels = 1;
            samples->frames = source.size();
            audio.samples = std::move (samples);
        }
        out.assets = { audio };
        return out;
    }

    // The synchronous render of the snapshot, on this thread.
    engine::OfflineRenderResult renderReference() const
    {
        std::shared_ptr<const engine::AssetSamples> view;
        if (sourceRate != 48'000.0)
            view = engine::buildRateMatchedSamples (source, 1, sourceRate, 48'000.0, engine::ResampleQuality::OfflineRender);
        else
        {
            auto samples = std::make_shared<engine::AssetSamples>();
            samples->interleaved = source;
            samples->channels = 1;
            samples->frames = source.size();
            view = samples;
        }
        const engine::DecodedAssetAudio decoded { project.assets[0].id, project.sampleRate, view->frames, 1,
                                                  std::span<const float> (view->interleaved.data(), view->interleaved.size()) };
        const engine::Project rendered = engine::projectHasCrossRateAssets (project) ? engine::projectInViewFrames (project) : project;
        engine::OfflineRenderResult result = engine::renderOfflineProject (rendered, std::span<const engine::DecodedAssetAudio> (&decoded, 1));
        REQUIRE (result.ok());
        return result;
    }

    // The synchronous reference: the same render and the same writer, on this thread.
    void writeReference (const std::filesystem::path& path) const
    {
        std::shared_ptr<const engine::AssetSamples> view;
        if (sourceRate != 48'000.0)
            view = engine::buildRateMatchedSamples (source, 1, sourceRate, 48'000.0, engine::ResampleQuality::OfflineRender);
        else
        {
            auto samples = std::make_shared<engine::AssetSamples>();
            samples->interleaved = source;
            samples->channels = 1;
            samples->frames = source.size();
            view = samples;
        }
        const engine::DecodedAssetAudio decoded { project.assets[0].id, project.sampleRate, view->frames, 1,
                                                  std::span<const float> (view->interleaved.data(), view->interleaved.size()) };
        const engine::Project rendered = engine::projectHasCrossRateAssets (project) ? engine::projectInViewFrames (project) : project;
        const engine::OfflineRenderResult result = engine::renderOfflineProject (rendered, std::span<const engine::DecodedAssetAudio> (&decoded, 1));
        REQUIRE (result.ok());
        REQUIRE (io::writeFloat32WavFile (path, result.sampleRate, result.channels, result.frames,
                                          std::span<const float> (result.interleavedSamples.data(), result.interleavedSamples.size())).ok());
    }
};

std::string bytesOf (const std::filesystem::path& path)
{
    std::ifstream in (path, std::ios::binary);
    return std::string ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
}

void waitTerminal (const app::ExportJob& job)
{
    for (int i = 0; i < 20'000 && ! job.terminal(); ++i)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    REQUIRE (job.terminal());
}

void waitHeld (const engine::OfflineRenderLatch& latch)
{
    for (int i = 0; i < 20'000 && ! latch.held.load(); ++i)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    REQUIRE (latch.held.load());
}

} // namespace

TEST_CASE ("ADR-0058 an export job's file equals the synchronous render, byte for byte", "[export-job]")
{
    const auto directory = scratch ("equal");
    for (const double rate : { 48'000.0, 44'100.0 })   // a same-rate and a cross-rate Asset (the worker builds its view)
    {
        const Fixture f (24'000, rate);
        const auto reference = directory / ("reference-" + std::to_string (static_cast<int> (rate)) + ".wav");
        const auto exported = directory / ("job-" + std::to_string (static_cast<int> (rate)) + ".wav");
        f.writeReference (reference);
        app::ExportJob job (1, f.snapshot (exported));
        job.start();
        waitTerminal (job);
        job.join();
        INFO (job.message());
        REQUIRE (job.state() == app::ExportJobState::Succeeded);
        REQUIRE (job.percent() == 100);
        REQUIRE (bytesOf (exported) == bytesOf (reference));
    }
}

TEST_CASE ("ADR-0058 a held job is Rendering inside the render half; Cancel ends it with no file; release finishes it",
           "[export-job]")
{
    const auto directory = scratch ("latch");
    const Fixture f (96'000, 48'000.0);

    SECTION ("cancelled while held")
    {
        engine::OfflineRenderLatch latch;
        latch.holdAfterFrames = 4'096;
        const auto destination = directory / "cancelled.wav";
        app::ExportJob job (2, f.snapshot (destination), &latch);
        job.start();
        waitHeld (latch);
        REQUIRE (job.state() == app::ExportJobState::Rendering);
        const int held = job.percent();
        REQUIRE (held > 0);
        REQUIRE (held < 50);
        job.cancel();
        waitTerminal (job);
        job.join();
        REQUIRE (job.state() == app::ExportJobState::Cancelled);
        REQUIRE (job.message() == "Export cancelled");
        REQUIRE_FALSE (std::filesystem::exists (destination));
    }

    SECTION ("released")
    {
        engine::OfflineRenderLatch latch;
        latch.holdAfterFrames = 4'096;
        const auto destination = directory / "released.wav";
        app::ExportJob job (3, f.snapshot (destination), &latch);
        job.start();
        waitHeld (latch);
        int last = job.percent();
        latch.released.store (true);
        for (int i = 0; i < 20'000 && ! job.terminal(); ++i)
        {
            const int now = job.percent();
            REQUIRE (now >= last);   // progress never goes back
            last = now;
            std::this_thread::sleep_for (std::chrono::microseconds (200));
        }
        job.join();
        REQUIRE (job.state() == app::ExportJobState::Succeeded);
        REQUIRE (job.percent() == 100);
        REQUIRE (std::filesystem::exists (destination));
    }

    SECTION ("destroyed while held: cancelled and joined")
    {
        engine::OfflineRenderLatch latch;
        latch.holdAfterFrames = 4'096;
        const auto destination = directory / "destroyed.wav";
        {
            app::ExportJob job (4, f.snapshot (destination), &latch);
            job.start();
            waitHeld (latch);
        }   // ~ExportJob: cancel, join — returns
        REQUIRE_FALSE (std::filesystem::exists (destination));
    }
}

// ---- cp2: committing to disk ----

namespace {

std::vector<std::filesystem::path> partialsIn (const std::filesystem::path& folder)
{
    std::vector<std::filesystem::path> out;
    for (const auto& entry : std::filesystem::directory_iterator (folder))
        if (entry.path().extension() == ".partial")
            out.push_back (entry.path());
    return out;
}

void writeBytes (const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream out (path, std::ios::binary | std::ios::trunc);
    out << bytes;
}

} // namespace

TEST_CASE ("ADR-0058 the chunked WAV writer's bytes equal the one-shot writers'", "[export-commit]")
{
    const auto directory = scratch ("stream");
    std::vector<float> samples (2u * 50'001u);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = static_cast<float> (std::sin (static_cast<double> (i) * 0.013)) * 0.9f;
    for (const std::uint16_t bits : { std::uint16_t { 32 }, std::uint16_t { 24 }, std::uint16_t { 16 } })
    {
        const auto oneShot = directory / ("one-" + std::to_string (bits) + ".wav");
        const auto streamed = directory / ("stream-" + std::to_string (bits) + ".wav");
        REQUIRE ((bits == 32u ? io::writeFloat32WavFile (oneShot, engine::SampleRate { 48'000.0 }, 2, 50'001, samples)
                              : io::writePcmWavFile (oneShot, engine::SampleRate { 48'000.0 }, 2, 50'001, samples, bits)).ok());
        io::WavStreamWriter writer;
        REQUIRE (writer.open (streamed, engine::SampleRate { 48'000.0 }, 2, 50'001, bits).ok());
        for (std::size_t frame = 0; frame < 50'001u; frame += 7'000u)
        {
            const std::size_t count = std::min<std::size_t> (7'000u, 50'001u - frame);
            REQUIRE (writer.append (std::span<const float> (samples).subspan (frame * 2u, count * 2u)).ok());
        }
        REQUIRE (writer.finish().ok());
        REQUIRE (bytesOf (streamed) == bytesOf (oneShot));
    }
}

TEST_CASE ("ADR-0058 cancel during render or write leaves no .partial and an earlier file byte for byte", "[export-commit]")
{
    const auto directory = scratch ("commit-cancel");
    const Fixture f (96'000, 48'000.0);
    const auto destination = directory / "mix.wav";
    const std::string earlier = "an earlier export the user kept";

    SECTION ("during render")
    {
        writeBytes (destination, earlier);
        engine::OfflineRenderLatch latch;
        latch.holdAfterFrames = 4'096;
        app::ExportJob job (11, f.snapshot (destination), &latch);
        job.start();
        waitHeld (latch);
        job.cancel();
        waitTerminal (job);
        job.join();
        REQUIRE (job.state() == app::ExportJobState::Cancelled);
        REQUIRE (partialsIn (directory).empty());
        REQUIRE (bytesOf (destination) == earlier);
    }

    SECTION ("during write: the temporary exists, the destination is untouched, then neither changes")
    {
        writeBytes (destination, earlier);
        engine::OfflineRenderLatch writeLatch;
        writeLatch.holdAfterFrames = 16'384;   // one chunk written
        app::ExportJob job (12, f.snapshot (destination), nullptr, &writeLatch);
        job.start();
        waitHeld (writeLatch);
        REQUIRE (job.state() == app::ExportJobState::Writing);
        REQUIRE (std::filesystem::exists (app::ExportJob::partialPathFor (destination, 12)));
        REQUIRE (bytesOf (destination) == earlier);
        REQUIRE (job.percent() >= 50);
        job.cancel();
        waitTerminal (job);
        job.join();
        REQUIRE (job.state() == app::ExportJobState::Cancelled);
        REQUIRE (partialsIn (directory).empty());
        REQUIRE (bytesOf (destination) == earlier);
    }
}

TEST_CASE ("ADR-0058 success replaces an earlier file; an unwritable destination fails with its cause; stale temporaries go",
           "[export-commit]")
{
    const auto directory = scratch ("commit");
    const Fixture f (24'000, 48'000.0);

    // Stale temporaries of this destination (a crash's) are removed; another destination's are not.
    const auto destination = directory / "mix.wav";
    writeBytes (destination, "an earlier export");
    writeBytes (app::ExportJob::partialPathFor (destination, 77), "stale");
    writeBytes (directory / "mix.wav.crashed.partial", "stale");
    writeBytes (app::ExportJob::partialPathFor (directory / "other.wav", 5), "someone else's");
    writeBytes (app::ExportJob::partialPathFor (destination, 9), "a replaced project's job, still winding down");
    const auto reference = directory / "reference.wav";
    f.writeReference (reference);

    app::ExportSnapshot snapshot = f.snapshot (destination);
    snapshot.liveJobIds = { 9 };
    app::ExportJob job (21, std::move (snapshot));
    job.start();
    waitTerminal (job);
    job.join();
    INFO (job.message());
    REQUIRE (job.state() == app::ExportJobState::Succeeded);
    REQUIRE (bytesOf (destination) == bytesOf (reference));   // replaced
    std::vector<std::string> left;
    for (const auto& path : partialsIn (directory))
        left.push_back (path.filename().string());
    std::sort (left.begin(), left.end());
    REQUIRE (left == std::vector<std::string> { "mix.wav.9.partial", "other.wav.5.partial" });   // the live job's and another's

    // A destination whose folder cannot exist (its parent is a file): Failed, with the cause, and nothing left.
    writeBytes (directory / "blocker", "a file, not a folder");
    const auto unwritable = directory / "blocker" / "mix.wav";
    app::ExportJob failing (22, f.snapshot (unwritable));
    failing.start();
    waitTerminal (failing);
    failing.join();
    REQUIRE (failing.state() == app::ExportJobState::Failed);
    REQUIRE (failing.failure() == app::ExportFailure::Write);
    REQUIRE (failing.message().find ("could not write mix.wav: ") != std::string::npos);
    REQUIRE (bytesOf (directory / "blocker") == "a file, not a folder");
}

// ---- cp3: formats and options ----

namespace {

// The TPDF law, written out again here (not shared with the product): splitmix64 seeds, xorshift64 streams, two
// uniforms in [-0.5, 0.5) per sample, added to sample * fullScale before rounding and clamping.
std::uint64_t referenceSeed (std::uint64_t fileIndex, std::uint16_t channel)
{
    std::uint64_t z = (fileIndex << 16u) ^ static_cast<std::uint64_t> (channel) ^ 0x5945534441570000ull;
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30u)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27u)) * 0x94D049BB133111EBull;
    z ^= z >> 31u;
    return z == 0u ? 1u : z;
}

double referenceUniform (std::uint64_t& x)
{
    x ^= x << 13u;
    x ^= x >> 7u;
    x ^= x << 17u;
    return static_cast<double> (x >> 11u) / 9007199254740992.0 - 0.5;
}

// The data bytes an integer export of `samples` must hold, dithered or not.
std::string referencePcmData (const std::vector<float>& samples, std::uint16_t channels, std::uint16_t bits, bool dither)
{
    std::vector<std::uint64_t> state;
    for (std::uint16_t c = 0; c < channels; ++c)
        state.push_back (referenceSeed (0u, c));
    const double fullScale = bits == 16u ? 32767.0 : 8388607.0;
    std::string out;
    for (std::size_t i = 0; i < samples.size(); ++i)
    {
        const auto channel = static_cast<std::uint16_t> (i % channels);
        double noise = 0.0;
        if (dither)
        {
            const double first = referenceUniform (state[channel]);
            noise = first + referenceUniform (state[channel]);
        }
        const double q = std::clamp (std::round (static_cast<double> (samples[i]) * fullScale + noise), -fullScale - 1.0, fullScale);
        const auto bitsValue = static_cast<std::uint32_t> (static_cast<std::int32_t> (q));
        out.push_back (static_cast<char> (bitsValue & 0xFFu));
        out.push_back (static_cast<char> ((bitsValue >> 8u) & 0xFFu));
        if (bits == 24u)
            out.push_back (static_cast<char> ((bitsValue >> 16u) & 0xFFu));
    }
    return out;
}

} // namespace

TEST_CASE ("ADR-0058 integer exports equal the TPDF law's reference; dither off is plain rounding; float is never dithered",
           "[export-options]")
{
    const auto directory = scratch ("dither");
    const Fixture f (30'000, 48'000.0);
    const engine::OfflineRenderResult rendered = f.renderReference();
    for (const auto format : { app::ExportFormat::Int16, app::ExportFormat::Int24 })
        for (const bool dither : { true, false })
        {
            const std::uint16_t bits = format == app::ExportFormat::Int16 ? 16u : 24u;
            const auto destination = directory / ("mix-" + std::to_string (bits) + (dither ? "-d" : "-n") + ".wav");
            app::ExportSnapshot snapshot = f.snapshot (destination);
            snapshot.format = format;
            snapshot.dither = dither;
            app::ExportJob job (31, std::move (snapshot));
            job.start();
            waitTerminal (job);
            job.join();
            REQUIRE (job.state() == app::ExportJobState::Succeeded);
            const std::string file = bytesOf (destination);
            REQUIRE (file.size() > 44u);
            INFO ("bits " << bits << " dither " << dither);
            REQUIRE (file.substr (44) == referencePcmData (rendered.interleavedSamples, rendered.channels, bits, dither));
            if (! dither)   // and that equals the one-shot writer's plain rounding
            {
                const auto plain = directory / ("plain-" + std::to_string (bits) + ".wav");
                REQUIRE (io::writePcmWavFile (plain, rendered.sampleRate, rendered.channels, rendered.frames,
                                              rendered.interleavedSamples, bits).ok());
                REQUIRE (bytesOf (plain) == file);
            }
        }
}

TEST_CASE ("ADR-0058 no two files or channels share dither noise", "[export-options]")
{
    io::TpdfDither mix (0, 2);
    io::TpdfDither stem (1, 2);
    int sameAcrossFiles = 0;
    int sameAcrossChannels = 0;
    for (int i = 0; i < 1'000; ++i)
    {
        const double m0 = mix.next (0);
        const double m1 = mix.next (1);
        const double s0 = stem.next (0);
        sameAcrossFiles += m0 == s0 ? 1 : 0;
        sameAcrossChannels += m0 == m1 ? 1 : 0;
        REQUIRE (m0 > -1.0);
        REQUIRE (m0 < 1.0);
    }
    REQUIRE (sameAcrossFiles == 0);
    REQUIRE (sameAcrossChannels == 0);
}

TEST_CASE ("ADR-0058 a range exports exactly its frames; a range past the end is refused before any render", "[export-options]")
{
    const auto directory = scratch ("range");
    const Fixture f (48'000, 48'000.0);
    const engine::OfflineRenderResult full = f.renderReference();

    // [10 000, 30 000): exactly those frames, equal to the full render's slice.
    const auto ranged = directory / "range.wav";
    app::ExportSnapshot snapshot = f.snapshot (ranged);
    snapshot.range = std::pair<std::uint64_t, std::uint64_t> { 10'000u, 30'000u };
    app::ExportJob job (41, std::move (snapshot));
    job.start();
    waitTerminal (job);
    job.join();
    REQUIRE (job.state() == app::ExportJobState::Succeeded);
    io::Float32Wav wav;
    REQUIRE (io::readFloat32WavFile (ranged, wav).ok());
    REQUIRE (wav.frames == 20'000u);
    const std::vector<float> slice (full.interleavedSamples.begin() + 10'000 * full.channels,
                                    full.interleavedSamples.begin() + 30'000 * full.channels);
    REQUIRE (wav.interleavedSamples == slice);

    // A range ending past the render's end: from its start to the end.
    const auto tail = directory / "tail.wav";
    app::ExportSnapshot tailSnapshot = f.snapshot (tail);
    tailSnapshot.range = std::pair<std::uint64_t, std::uint64_t> { full.frames - 1'000u, full.frames + 50'000u };
    app::ExportJob tailJob (42, std::move (tailSnapshot));
    tailJob.start();
    waitTerminal (tailJob);
    tailJob.join();
    REQUIRE (tailJob.state() == app::ExportJobState::Succeeded);
    REQUIRE (io::readFloat32WavFile (tail, wav).ok());
    REQUIRE (wav.frames == 1'000u);

    // Past the end: refused with its reason, and the render never ran (a latch at frame 0 was never reached).
    engine::OfflineRenderLatch latch;
    latch.holdAfterFrames = 0;
    const auto past = directory / "past.wav";
    app::ExportSnapshot pastSnapshot = f.snapshot (past);
    pastSnapshot.range = std::pair<std::uint64_t, std::uint64_t> { full.frames + 10u, full.frames + 1'000u };
    app::ExportJob pastJob (43, std::move (pastSnapshot), &latch);
    pastJob.start();
    waitTerminal (pastJob);
    pastJob.join();
    REQUIRE (pastJob.state() == app::ExportJobState::Failed);
    REQUIRE (pastJob.failure() == app::ExportFailure::Range);
    REQUIRE_FALSE (latch.held.load());
    REQUIRE_FALSE (std::filesystem::exists (past));
}

TEST_CASE ("ADR-0058 normalize brings the peak to the target with one gain, and leaves silence silent", "[export-options]")
{
    const auto directory = scratch ("normalize");
    const Fixture f (48'000, 48'000.0);
    const engine::OfflineRenderResult full = f.renderReference();

    const auto plain = directory / "plain.wav";
    const auto normalized = directory / "normalized.wav";
    for (const bool normalize : { false, true })
    {
        app::ExportSnapshot snapshot = f.snapshot (normalize ? normalized : plain);
        if (normalize)
            snapshot.normalizePeakDbfs = -1.0;
        app::ExportJob job (normalize ? 52 : 51, std::move (snapshot));
        job.start();
        waitTerminal (job);
        job.join();
        REQUIRE (job.state() == app::ExportJobState::Succeeded);
    }
    io::Float32Wav before;
    io::Float32Wav after;
    REQUIRE (io::readFloat32WavFile (plain, before).ok());
    REQUIRE (io::readFloat32WavFile (normalized, after).ok());
    REQUIRE (before.interleavedSamples.size() == after.interleavedSamples.size());
    double peak = 0.0;
    std::size_t loudest = 0;
    for (std::size_t i = 0; i < after.interleavedSamples.size(); ++i)
        if (std::abs (after.interleavedSamples[i]) > peak)
        {
            peak = std::abs (after.interleavedSamples[i]);
            loudest = i;
        }
    INFO ("peak " << 20.0 * std::log10 (peak) << " dBFS");
    REQUIRE (std::abs (20.0 * std::log10 (peak) - (-1.0)) < 0.01);
    const double gain = static_cast<double> (after.interleavedSamples[loudest]) / static_cast<double> (before.interleavedSamples[loudest]);
    for (std::size_t i = 0; i < after.interleavedSamples.size(); i += 97)   // one gain everywhere
        REQUIRE (std::abs (static_cast<double> (after.interleavedSamples[i]) - gain * static_cast<double> (before.interleavedSamples[i])) < 1.0e-6);

    // Silence is never boosted.
    Fixture silent (24'000, 48'000.0);
    std::fill (silent.source.begin(), silent.source.end(), 0.0f);
    const auto quiet = directory / "silent.wav";
    app::ExportSnapshot snapshot = silent.snapshot (quiet);
    snapshot.normalizePeakDbfs = -1.0;
    app::ExportJob job (53, std::move (snapshot));
    job.start();
    waitTerminal (job);
    job.join();
    REQUIRE (job.state() == app::ExportJobState::Succeeded);
    io::Float32Wav silence;
    REQUIRE (io::readFloat32WavFile (quiet, silence).ok());
    REQUIRE (std::all_of (silence.interleavedSamples.begin(), silence.interleavedSamples.end(), [] (float s) { return s == 0.0f; }));
    REQUIRE (app::exportNormalizeGain (0.0, -1.0) == 1.0);
}
