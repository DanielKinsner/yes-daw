// YES DAW - ADR-0058 cp1: the export job on its worker. Pure C++ + Catch2 (no JUCE), so the TSan leg checks the
// handoff (the worker's atomics, the result published before the terminal state) and the RTSan leg builds it too.
//
// Gates: a job's file equals the synchronous render written by the same writer, byte for byte (same-rate and
// cross-rate Assets); held at its latch the job is Rendering with progress inside the render half; Cancel ends it
// Cancelled with no file; released, it succeeds at 100 %; destroying a held job cancels and joins it.

#include "app/ExportJob.h"

#include <catch2/catch_test_macros.hpp>

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
