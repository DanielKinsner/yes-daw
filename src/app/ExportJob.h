// YES DAW — the export job (ADR-0058 cp1): an export rendered and written on a worker thread over a snapshot it owns.
//
// The message thread builds the snapshot — a copy of the project (in view frames when it has cross-rate Assets), the
// options, and owning references to every Asset's audio — and starts the job. The worker reads only its snapshot and
// writes only its atomics, its result text and its file: never the model, the shell, the project database or the
// status line. Progress, cancel and the terminal state are atomics; the result text is stored before the terminal
// state (release) and read after it (acquire). The owner polls the job from the UI tick and joins it once terminal
// (the join is then immediate); destroying a job cancels and joins it — bounded by one render block or one write.
//
// cp2: the file is written in chunks to `<destination name>.<job id>.partial` beside the destination and renamed
// over it only on success; a cancel or a failure before that removes the temporary and leaves the destination as it
// was. Starting a job removes stale `<destination name>.*.partial` files a crash left behind.
//
// Pure C++ (no JUCE), so the TSan leg exercises the handoff.

#pragma once

#include "engine/OfflineRenderer.h"
#include "engine/RateMatchedView.h"
#include "io/PathText.h"
#include "io/WavFile.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace yesdaw::app {

enum class ExportJobState : std::uint8_t
{
    Preparing,
    Rendering,
    Writing,
    Committing,
    Succeeded,
    Failed,
    Cancelled
};

[[nodiscard]] constexpr bool exportJobTerminal (ExportJobState state) noexcept
{
    return state == ExportJobState::Succeeded || state == ExportJobState::Failed || state == ExportJobState::Cancelled;
}

enum class ExportFormat : std::uint8_t
{
    Float32,
    Int24,
    Int16
};

// Why a job failed (the owner keeps its historic dispatch reasons per kind).
enum class ExportFailure : std::uint8_t
{
    None,
    Render,
    Range,
    Write
};

// One Asset's audio, owned by the job: a same-rate Asset as the samples the render reads; a cross-rate Asset as its
// decoded source, from which the worker builds the offline tier's view (ADR-0055).
struct ExportAssetAudio
{
    engine::EntityId assetId;
    std::uint16_t channels = 0;
    std::shared_ptr<const engine::AssetSamples> samples;   // at the project rate (same-rate Assets)
    std::shared_ptr<const std::vector<float>> source;      // a cross-rate Asset's decoded source
    double sourceRateHz = 0.0;
};

struct ExportSnapshot
{
    engine::Project project;   // a copy; in view frames when it has cross-rate Assets
    std::vector<ExportAssetAudio> assets;
    std::filesystem::path destination;
    ExportFormat format = ExportFormat::Float32;
    std::optional<std::pair<std::uint64_t, std::uint64_t>> range;   // [start, end) frames; none = the whole project
    std::vector<std::uint64_t> liveJobIds;   // jobs still winding down (a replaced project's): their temporaries are kept
    bool dither = true;                      // cp3: TPDF dither for 16 / 24-bit output (float is never dithered)
};

class ExportJob
{
public:
    // The latches are the gates' hooks (hold the render, or the write, after a frame until released); null in the app.
    ExportJob (std::uint64_t id, ExportSnapshot snapshot, engine::OfflineRenderLatch* latch = nullptr,
               engine::OfflineRenderLatch* writeLatch = nullptr)
        : id_ (id), snapshot_ (std::move (snapshot)), latch_ (latch), writeLatch_ (writeLatch)
    {
    }

    // The temporary a job writes before its commit (ADR-0058 cp2).
    [[nodiscard]] static std::filesystem::path partialPathFor (const std::filesystem::path& destination, std::uint64_t id)
    {
        std::filesystem::path partial = destination;
        partial += "." + std::to_string (id) + ".partial";
        return partial;
    }

    ExportJob (const ExportJob&) = delete;
    ExportJob& operator= (const ExportJob&) = delete;

    ~ExportJob()
    {
        cancel();
        join();
    }

    void start()
    {
        if (! worker_.joinable())
            worker_ = std::thread ([this] {
                try
                {
                    run();
                }
                catch (...)   // e.g. out of memory: the job fails, the app does not
                {
                    std::error_code removed;
                    std::filesystem::remove (partialPathFor (snapshot_.destination, id_), removed);
                    finish (ExportJobState::Failed, ExportFailure::Write, "Export failed: an unexpected error");
                }
            });
    }

    void cancel() noexcept { cancelRequested_.store (true, std::memory_order_release); }
    [[nodiscard]] bool cancelRequested() const noexcept { return cancelRequested_.load (std::memory_order_acquire); }

    // Blocks until the worker has finished (immediate once the state is terminal).
    void join()
    {
        if (worker_.joinable())
            worker_.join();
    }

    [[nodiscard]] std::uint64_t id() const noexcept { return id_; }
    [[nodiscard]] const std::filesystem::path& destination() const noexcept { return snapshot_.destination; }
    [[nodiscard]] ExportJobState state() const noexcept { return state_.load (std::memory_order_acquire); }
    [[nodiscard]] bool terminal() const noexcept { return exportJobTerminal (state()); }

    // Valid once terminal (written before the terminal state is published).
    [[nodiscard]] const std::string& message() const noexcept { return message_; }
    [[nodiscard]] ExportFailure failure() const noexcept { return failure_; }

    // 0..100: the render is the first half, the write the second (ADR-0058: work in frames over two phases).
    [[nodiscard]] int percent() const noexcept
    {
        if (state() == ExportJobState::Succeeded)
            return 100;
        const std::uint64_t renderTotal = renderTotal_.load (std::memory_order_relaxed);
        const std::uint64_t rendered = rendered_.load (std::memory_order_relaxed);
        const std::uint64_t writeTotal = writeTotal_.load (std::memory_order_relaxed);
        const std::uint64_t written = written_.load (std::memory_order_relaxed);
        const double renderShare = renderTotal > 0u ? std::min (1.0, static_cast<double> (rendered) / static_cast<double> (renderTotal)) : 0.0;
        const double writeShare = writeTotal > 0u ? std::min (1.0, static_cast<double> (written) / static_cast<double> (writeTotal)) : 0.0;
        return std::clamp (static_cast<int> (50.0 * renderShare + 50.0 * writeShare), 0, 99);
    }

private:
    void finish (ExportJobState state, ExportFailure failure, std::string message)
    {
        message_ = std::move (message);
        failure_ = failure;
        state_.store (state, std::memory_order_release);
    }

    [[nodiscard]] bool cancelled() const noexcept { return cancelRequested_.load (std::memory_order_acquire); }

    void run()
    {
        const std::string name = yesdaw::io::utf8Text (snapshot_.destination.filename());
        const double projectRateHz = snapshot_.project.sampleRate.hz;

        // Preparing: stale temporaries of this destination (a crash's), then the offline tier's views for cross-rate
        // Assets — both here, off the message thread.
        removeStalePartials();
        std::vector<std::shared_ptr<const engine::AssetSamples>> owners;
        std::vector<engine::DecodedAssetAudio> views;
        owners.reserve (snapshot_.assets.size());
        views.reserve (snapshot_.assets.size());
        for (const ExportAssetAudio& asset : snapshot_.assets)
        {
            if (cancelled())
                return finish (ExportJobState::Cancelled, ExportFailure::None, "Export cancelled");
            std::shared_ptr<const engine::AssetSamples> samples = asset.samples;
            if (samples == nullptr && asset.source != nullptr)
                samples = engine::buildRateMatchedSamples (*asset.source, asset.channels, asset.sourceRateHz, projectRateHz,
                                                           engine::ResampleQuality::OfflineRender);
            if (samples == nullptr)
                continue;
            owners.push_back (samples);
            views.push_back (engine::DecodedAssetAudio {
                asset.assetId, snapshot_.project.sampleRate, samples->frames, asset.channels,
                std::span<const float> (samples->interleaved.data(), samples->interleaved.size()) });
        }

        // Rendering.
        state_.store (ExportJobState::Rendering, std::memory_order_release);
        engine::OfflineRenderOptions options;
        options.progressFrames = &rendered_;
        options.progressTotalFrames = &renderTotal_;
        options.cancel = &cancelRequested_;
        options.latch = latch_;
        const engine::OfflineRenderResult rendered = engine::renderOfflineProject (
            snapshot_.project, std::span<const engine::DecodedAssetAudio> (views.data(), views.size()), std::move (options));
        if (rendered.status == engine::OfflineRenderStatus::Cancelled || cancelled())
            return finish (ExportJobState::Cancelled, ExportFailure::None, "Export cancelled");
        if (! rendered.ok())
            return finish (ExportJobState::Failed, ExportFailure::Render, "Export failed: the project render failed");

        // The range: a slice of the render.
        std::uint64_t frames = rendered.frames;
        std::span<const float> samples (rendered.interleavedSamples.data(), rendered.interleavedSamples.size());
        if (snapshot_.range.has_value())
        {
            const std::uint64_t start = std::min (snapshot_.range->first, rendered.frames);
            const std::uint64_t end = std::min (snapshot_.range->second, rendered.frames);
            if (end <= start)
                return finish (ExportJobState::Failed, ExportFailure::Range,
                               "Export failed: the loop range is outside the rendered project");
            frames = end - start;
            samples = samples.subspan (static_cast<std::size_t> (start) * rendered.channels,
                                       static_cast<std::size_t> (frames) * rendered.channels);
        }

        // Writing: in chunks, to the temporary; Cancel is checked between chunks.
        if (cancelled())
            return finish (ExportJobState::Cancelled, ExportFailure::None, "Export cancelled");
        state_.store (ExportJobState::Writing, std::memory_order_release);
        writeTotal_.store (frames, std::memory_order_relaxed);
        const std::filesystem::path partial = partialPathFor (snapshot_.destination, id_);
        const auto abandon = [&partial] (io::WavStreamWriter& writer) {
            writer.abandon();
            std::error_code removed;
            std::filesystem::remove (partial, removed);
        };
        io::WavStreamWriter writer;
        const std::uint16_t bits = snapshot_.format == ExportFormat::Float32 ? 32u : snapshot_.format == ExportFormat::Int24 ? 24u : 16u;
        if (const io::WavResult opened = writer.open (partial, rendered.sampleRate, rendered.channels, frames, bits); ! opened.ok())
        {
            abandon (writer);
            return finish (ExportJobState::Failed, ExportFailure::Write, "Export failed: could not write " + name + ": " + opened.message);
        }
        std::optional<io::TpdfDither> dither;
        if (snapshot_.dither && bits != 32u)
            dither.emplace (0u, rendered.channels);   // the mix is file 0 of its export
        std::uint64_t done = 0;
        while (done < frames)
        {
            if (writeLatch_ != nullptr && done >= writeLatch_->holdAfterFrames && ! writeLatch_->released.load (std::memory_order_acquire))
            {
                writeLatch_->held.store (true, std::memory_order_release);
                while (! writeLatch_->released.load (std::memory_order_acquire) && ! cancelled())
                    std::this_thread::sleep_for (std::chrono::milliseconds (1));
            }
            if (cancelled())
            {
                abandon (writer);
                return finish (ExportJobState::Cancelled, ExportFailure::None, "Export cancelled");
            }
            const std::uint64_t chunk = std::min<std::uint64_t> (kWriteChunkFrames, frames - done);
            const io::WavResult appended = writer.append (samples.subspan (static_cast<std::size_t> (done) * rendered.channels,
                                                                          static_cast<std::size_t> (chunk) * rendered.channels),
                                                          dither.has_value() ? &*dither : nullptr);
            if (! appended.ok())
            {
                abandon (writer);
                return finish (ExportJobState::Failed, ExportFailure::Write, "Export failed: could not write " + name + ": " + appended.message);
            }
            done += chunk;
            written_.store (done, std::memory_order_relaxed);
        }
        if (const io::WavResult finished = writer.finish(); ! finished.ok())
        {
            abandon (writer);
            return finish (ExportJobState::Failed, ExportFailure::Write, "Export failed: could not write " + name + ": " + finished.message);
        }
        if (cancelled())   // the last chance: past this point the job commits
        {
            abandon (writer);
            return finish (ExportJobState::Cancelled, ExportFailure::None, "Export cancelled");
        }

        // Committing: the temporary replaces the destination in one rename (same folder, same volume). Cancel is
        // ignored from here: the job ends Succeeded, or Failed with the destination untouched.
        state_.store (ExportJobState::Committing, std::memory_order_release);
        std::error_code renamed;
        std::filesystem::rename (partial, snapshot_.destination, renamed);
        if (renamed)
        {
            std::error_code removed;
            std::filesystem::remove (partial, removed);
            return finish (ExportJobState::Failed, ExportFailure::Write,
                           "Export failed: could not replace " + name + ": " + renamed.message());
        }
        finish (ExportJobState::Succeeded, ExportFailure::None, "Exported " + name);
    }

    // Removes `<destination name>.*.partial` in the destination's folder (temporaries a crash left behind).
    void removeStalePartials() const
    {
        const std::filesystem::path folder = snapshot_.destination.has_parent_path() ? snapshot_.destination.parent_path()
                                                                                     : std::filesystem::path (".");
        const std::u8string prefix = snapshot_.destination.filename().u8string() + u8".";
        const std::u8string suffix = u8".partial";
        std::vector<std::u8string> live;
        for (const std::uint64_t id : snapshot_.liveJobIds)
            live.push_back (partialPathFor (snapshot_.destination, id).filename().u8string());
        std::error_code error;
        std::vector<std::filesystem::path> stale;
        for (std::filesystem::directory_iterator it (folder, error), end; ! error && it != end; it.increment (error))
        {
            const std::u8string file = it->path().filename().u8string();
            if (file.size() > prefix.size() + suffix.size() && file.starts_with (prefix) && file.ends_with (suffix)
                && std::find (live.begin(), live.end(), file) == live.end())
                stale.push_back (it->path());
        }
        for (const std::filesystem::path& path : stale)
        {
            std::error_code removed;
            std::filesystem::remove (path, removed);
        }
    }

    static constexpr std::uint64_t kWriteChunkFrames = 16'384;

    const std::uint64_t id_;
    const ExportSnapshot snapshot_;
    engine::OfflineRenderLatch* const latch_;
    engine::OfflineRenderLatch* const writeLatch_;
    std::atomic<ExportJobState> state_ { ExportJobState::Preparing };
    std::atomic<bool> cancelRequested_ { false };
    std::atomic<std::uint64_t> rendered_ { 0 };
    std::atomic<std::uint64_t> renderTotal_ { 0 };
    std::atomic<std::uint64_t> written_ { 0 };
    std::atomic<std::uint64_t> writeTotal_ { 0 };
    std::string message_;
    ExportFailure failure_ = ExportFailure::None;
    std::thread worker_;   // last: started after every member it reads exists, joined before any is destroyed
};

} // namespace yesdaw::app
