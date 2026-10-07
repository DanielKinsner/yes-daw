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
#include <cmath>
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

// One Asset's audio, owned by the job by reference (ADR-0059): the Asset's own decoded buffer at its own rate. A
// same-rate Asset's buffer is what the render reads (as its owner — no copy); a cross-rate Asset's is the source the
// worker builds the offline tier's view from (ADR-0055).
struct ExportAssetAudio
{
    engine::EntityId assetId;
    std::uint16_t channels = 0;
    std::shared_ptr<const engine::AssetSamples> buffer;
    double sourceRateHz = 0.0;   // the buffer's rate
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
    std::optional<double> normalizePeakDbfs; // cp3: one gain bringing the loudest file's peak here (none = off)

    // cp3: export stems — one file per top-level strip (a Track or Bus whose output is the master), named beside the
    // destination; the mix file is written too when includeMix. Empty = an ordinary single-file export.
    struct Stem
    {
        bool isBus = false;
        engine::EntityId stripId;
        std::string name;   // UTF-8, the strip's name
    };
    std::vector<Stem> stems;
    bool includeMix = true;
};

// ADR-0058 cp3: a strip's name as a file-name part — one rule set on every platform: `< > : " / \ | ? *` and control
// characters become `_`; trailing dots and spaces go; a Windows reserved name (CON, PRN, AUX, NUL, COM1-9, LPT1-9,
// any case, with or without an extension) gains a leading `_`; at most 100 characters; empty becomes "Strip".
[[nodiscard]] inline std::string exportSafeName (const std::string& utf8Name)
{
    std::string out;
    std::size_t characters = 0;
    for (std::size_t i = 0; i < utf8Name.size() && characters < 100u;)
    {
        const auto lead = static_cast<unsigned char> (utf8Name[i]);
        const std::size_t length = lead < 0x80u ? 1u : (lead >> 5u) == 0x6u ? 2u : (lead >> 4u) == 0xEu ? 3u : (lead >> 3u) == 0x1Eu ? 4u : 1u;
        if (length == 1u)
        {
            const char c = static_cast<char> (lead);
            const bool illegal = lead < 0x20u || lead == 0x7Fu || lead >= 0x80u
                              || c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*';
            out.push_back (illegal ? '_' : c);
        }
        else
        {
            out.append (utf8Name, i, std::min (length, utf8Name.size() - i));
        }
        i += length;
        ++characters;
    }
    while (! out.empty() && (out.back() == '.' || out.back() == ' '))
        out.pop_back();
    if (out.empty())
        return "Strip";
    std::string base = out.substr (0, out.find ('.'));
    for (char& c : base)
        c = static_cast<char> (c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    const bool reserved = base == "CON" || base == "PRN" || base == "AUX" || base == "NUL"
                       || (base.size() == 4u && (base.rfind ("COM", 0) == 0 || base.rfind ("LPT", 0) == 0) && base[3] >= '1' && base[3] <= '9');
    if (! reserved)
        return out;
    if (characters >= 100u)   // keep the cap with the prefix: drop the last character (whole UTF-8 sequence)
    {
        std::size_t cut = out.size() - 1u;
        while (cut > 0u && (static_cast<unsigned char> (out[cut]) & 0xC0u) == 0x80u)
            --cut;
        out.erase (cut);
    }
    return "_" + out;
}

// The stem files of an export to `destination`: `<destination stem> - <strip name>.wav` beside it; a name already
// used (case-insensitively) gets ` (2)`, ` (3)`.
[[nodiscard]] inline std::vector<std::filesystem::path> exportStemPaths (const std::filesystem::path& destination,
                                                                       const std::vector<std::string>& stripNames)
{
    const std::u8string stem = destination.stem().u8string();
    const std::string base (reinterpret_cast<const char*> (stem.data()), stem.size());
    std::vector<std::string> used;
    std::vector<std::filesystem::path> out;
    // The duplicate key: ASCII folded to lower case, every other character a wildcard — so "Caf\u00e9" and its upper-case
    // form (one file on a case-insensitive disk) never share a name; a false match only adds a " (2)".
    const auto lower = [] (const std::string& text) {
        std::string key;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const auto byte = static_cast<unsigned char> (text[i]);
            if (byte < 0x80u)
                key.push_back (static_cast<char> (byte >= 'A' && byte <= 'Z' ? byte - 'A' + 'a' : byte));
            else if ((byte & 0xC0u) != 0x80u)
                key.push_back ('\x01');
        }
        return key;
    };
    for (const std::string& name : stripNames)
    {
        const std::string safe = exportSafeName (name);
        std::string file = base + " - " + safe;
        for (int n = 2; std::find (used.begin(), used.end(), lower (file)) != used.end(); ++n)
            file = base + " - " + safe + " (" + std::to_string (n) + ")";
        used.push_back (lower (file));
        file += ".wav";
        const auto* bytes = reinterpret_cast<const char8_t*> (file.data());
        out.push_back (destination.parent_path() / std::filesystem::path (std::u8string (bytes, bytes + file.size())));
    }
    return out;
}

// ADR-0058 cp3: the normalize gain — the target peak over the measured one; silence (peak 0) is never boosted.
[[nodiscard]] inline double exportNormalizeGain (double peak, double targetDbfs) noexcept
{
    return peak > 0.0 ? std::pow (10.0, targetDbfs / 20.0) / peak : 1.0;
}

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
                catch (...)   // e.g. out of memory: the job fails, the app does not, and no temporary stays
                {
                    std::vector<std::filesystem::path> outputs { snapshot_.destination };
                    try
                    {
                        std::vector<std::string> names;
                        for (const ExportSnapshot::Stem& stem : snapshot_.stems)
                            names.push_back (stem.name);
                        for (const std::filesystem::path& path : exportStemPaths (snapshot_.destination, names))
                            outputs.push_back (path);
                    }
                    catch (...)
                    {
                    }
                    for (const std::filesystem::path& output : outputs)
                    {
                        std::error_code removed;
                        std::filesystem::remove (partialPathFor (output, id_), removed);
                        std::filesystem::remove (renderPathFor (output, id_), removed);
                    }
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
        const double files = static_cast<double> (std::max<std::uint64_t> (1u, fileCount_.load (std::memory_order_relaxed)));
        const double done = static_cast<double> (filesRendered_.load (std::memory_order_relaxed));
        const double current = renderTotal > 0u ? std::min (1.0, static_cast<double> (rendered) / static_cast<double> (renderTotal)) : 0.0;
        const double renderShare = std::min (1.0, (done + current) / files);
        const double writeShare = writeTotal > 0u ? std::min (1.0, static_cast<double> (written) / static_cast<double> (writeTotal)) : 0.0;
        const int now = std::clamp (static_cast<int> (50.0 * renderShare + 50.0 * writeShare), 0, 99);
        int shown = lastPercent_.load (std::memory_order_relaxed);   // never goes back
        while (now > shown && ! lastPercent_.compare_exchange_weak (shown, now, std::memory_order_relaxed)) {}
        return std::max (now, shown);
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
        if (! snapshot_.stems.empty())
            return runFiles();
        const std::string name = yesdaw::io::utf8Text (snapshot_.destination.filename());
        const double projectRateHz = snapshot_.project.sampleRate.hz;

        // Preparing: stale temporaries of this destination (a crash's), then the offline tier's views for cross-rate
        // Assets — both here, off the message thread.
        removeStalePartials (snapshot_.destination);
        std::vector<std::shared_ptr<const engine::AssetSamples>> owners;
        std::vector<engine::AssetOwnership> ownership;
        std::vector<engine::DecodedAssetAudio> views;
        owners.reserve (snapshot_.assets.size());
        views.reserve (snapshot_.assets.size());
        for (const ExportAssetAudio& asset : snapshot_.assets)
        {
            if (cancelled())
                return finish (ExportJobState::Cancelled, ExportFailure::None, "Export cancelled");
            if (asset.buffer == nullptr)
                continue;
            std::shared_ptr<const engine::AssetSamples> samples = asset.buffer;
            if (asset.sourceRateHz != projectRateHz && asset.sourceRateHz > 0.0)
                samples = engine::buildRateMatchedSamples (std::span<const float> (asset.buffer->interleaved), asset.channels,
                                                           asset.sourceRateHz, projectRateHz, engine::ResampleQuality::OfflineRender);
            if (samples == nullptr)
                continue;
            owners.push_back (samples);
            ownership.push_back ({ asset.assetId, samples });   // ADR-0059: the render reads these, it copies nothing
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
        options.exportRange = snapshot_.range;   // cp3: refused before rendering when past the end; rendered only to its end
        options.assetOwners = ownership;         // ADR-0059
        const engine::OfflineRenderResult rendered = engine::renderOfflineProject (
            snapshot_.project, std::span<const engine::DecodedAssetAudio> (views.data(), views.size()), std::move (options));
        if (rendered.status == engine::OfflineRenderStatus::Cancelled || cancelled())
            return finish (ExportJobState::Cancelled, ExportFailure::None, "Export cancelled");
        if (rendered.status == engine::OfflineRenderStatus::RangeOutsideRender)
            return finish (ExportJobState::Failed, ExportFailure::Range,
                           "Export failed: the loop range is outside the rendered project");
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
        // Normalize: one gain from the file's peak, applied before dither.
        double gain = 1.0;
        if (snapshot_.normalizePeakDbfs.has_value())
        {
            double peak = 0.0;
            for (const float sample : samples)
                peak = std::max (peak, static_cast<double> (std::abs (sample)));
            gain = exportNormalizeGain (peak, *snapshot_.normalizePeakDbfs);
        }
        std::vector<float> scaled;
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
            std::span<const float> chunkSamples = samples.subspan (static_cast<std::size_t> (done) * rendered.channels,
                                                                   static_cast<std::size_t> (chunk) * rendered.channels);
            if (gain != 1.0)
            {
                scaled.resize (chunkSamples.size());
                for (std::size_t i = 0; i < chunkSamples.size(); ++i)
                    scaled[i] = static_cast<float> (static_cast<double> (chunkSamples[i]) * gain);
                chunkSamples = std::span<const float> (scaled.data(), scaled.size());
            }
            const io::WavResult appended = writer.append (chunkSamples, dither.has_value() ? &*dither : nullptr);
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

    // Removes `<destination name>.*.partial` in the destination's folder (temporaries a crash left behind), sparing
    // those of jobs still winding down.
    void removeStalePartials (const std::filesystem::path& destination) const
    {
        const std::filesystem::path folder = destination.has_parent_path() ? destination.parent_path()
                                                                           : std::filesystem::path (".");
        const std::u8string prefix = destination.filename().u8string() + u8".";
        const std::u8string suffix = u8".partial";
        std::vector<std::u8string> live;
        for (const std::uint64_t id : snapshot_.liveJobIds)
        {
            live.push_back (partialPathFor (destination, id).filename().u8string());
            live.push_back (renderPathFor (destination, id).filename().u8string());
        }
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

    // cp3: a stems export's float render of one file, before its normalize gain and dither (also a `.partial`).
    [[nodiscard]] static std::filesystem::path renderPathFor (const std::filesystem::path& destination, std::uint64_t id)
    {
        std::filesystem::path partial = destination;
        partial += "." + std::to_string (id) + ".render.partial";
        return partial;
    }

    // Writes `samples` x `gain` (dithered when given) through `writer` in chunks, checking Cancel between them.
    // Returns false (with `failure` set when it is a write failure, empty when cancelled).
    [[nodiscard]] bool writeChunks (io::WavStreamWriter& writer, std::span<const float> samples, std::uint16_t channels,
                                    double gain, io::TpdfDither* dither, std::string& failure, bool countsAsWritten = true)
    {
        std::vector<float> scaled;
        const std::uint64_t frames = samples.size() / std::max<std::uint16_t> (1u, channels);
        for (std::uint64_t done = 0; done < frames;)
        {
            if (cancelled())
                return false;
            const std::uint64_t chunk = std::min<std::uint64_t> (kWriteChunkFrames, frames - done);
            std::span<const float> part = samples.subspan (static_cast<std::size_t> (done) * channels, static_cast<std::size_t> (chunk) * channels);
            if (gain != 1.0)
            {
                scaled.resize (part.size());
                for (std::size_t i = 0; i < part.size(); ++i)
                    scaled[i] = static_cast<float> (static_cast<double> (part[i]) * gain);
                part = std::span<const float> (scaled.data(), scaled.size());
            }
            if (const io::WavResult appended = writer.append (part, dither); ! appended.ok())
            {
                failure = appended.message;
                return false;
            }
            done += chunk;
            if (countsAsWritten)
                written_.fetch_add (chunk, std::memory_order_relaxed);
        }
        return true;
    }

    // cp3: a stems export — every file rendered (as float, to its own `.render.partial` when it needs a second pass),
    // then the one normalize gain (from the loudest file) and dither applied into each file's `.partial`, then every
    // file committed. Cancel or failure before the commit removes every temporary and changes no destination.
    void runFiles()
    {
        struct Output
        {
            std::filesystem::path destination;
            std::optional<std::pair<bool, engine::EntityId>> stem;
            std::uint64_t index = 0;
            std::uint64_t frames = 0;
            std::uint16_t channels = 2;
            double peak = 0.0;
            bool direct = false;   // float, no normalize: the render is the file
        };
        std::vector<Output> outputs;
        if (snapshot_.includeMix)
            outputs.push_back ({ snapshot_.destination, std::nullopt, 0u });
        std::vector<std::string> names;
        for (const ExportSnapshot::Stem& stem : snapshot_.stems)
            names.push_back (stem.name);
        const std::vector<std::filesystem::path> stemPaths = exportStemPaths (snapshot_.destination, names);
        for (std::size_t i = 0; i < snapshot_.stems.size(); ++i)
            outputs.push_back ({ stemPaths[i], std::pair<bool, engine::EntityId> { snapshot_.stems[i].isBus, snapshot_.stems[i].stripId },
                                 static_cast<std::uint64_t> (i + 1u) });
        fileCount_.store (outputs.size(), std::memory_order_relaxed);

        const auto cleanUp = [this, &outputs] {
            for (const Output& output : outputs)
            {
                std::error_code removed;
                std::filesystem::remove (partialPathFor (output.destination, id_), removed);
                std::filesystem::remove (renderPathFor (output.destination, id_), removed);
            }
        };
        const auto fail = [&] (ExportFailure failure, std::string message) {
            cleanUp();
            finish (ExportJobState::Failed, failure, std::move (message));
        };
        const auto cancelledOut = [&] {
            cleanUp();
            finish (ExportJobState::Cancelled, ExportFailure::None, "Export cancelled");
        };

        for (const Output& output : outputs)
            removeStalePartials (output.destination);

        // Preparing: the offline views, as for one file.
        const double projectRateHz = snapshot_.project.sampleRate.hz;
        std::vector<std::shared_ptr<const engine::AssetSamples>> owners;
        std::vector<engine::AssetOwnership> ownership;
        std::vector<engine::DecodedAssetAudio> views;
        for (const ExportAssetAudio& asset : snapshot_.assets)
        {
            if (cancelled())
                return cancelledOut();
            if (asset.buffer == nullptr)
                continue;
            std::shared_ptr<const engine::AssetSamples> samples = asset.buffer;
            if (asset.sourceRateHz != projectRateHz && asset.sourceRateHz > 0.0)
                samples = engine::buildRateMatchedSamples (std::span<const float> (asset.buffer->interleaved), asset.channels,
                                                           asset.sourceRateHz, projectRateHz, engine::ResampleQuality::OfflineRender);
            if (samples == nullptr)
                continue;
            owners.push_back (samples);
            ownership.push_back ({ asset.assetId, samples });   // ADR-0059: the render reads these, it copies nothing
            views.push_back (engine::DecodedAssetAudio {
                asset.assetId, snapshot_.project.sampleRate, samples->frames, asset.channels,
                std::span<const float> (samples->interleaved.data(), samples->interleaved.size()) });
        }

        const std::uint16_t bits = snapshot_.format == ExportFormat::Float32 ? 32u : snapshot_.format == ExportFormat::Int24 ? 24u : 16u;
        const bool secondPass = bits != 32u || snapshot_.normalizePeakDbfs.has_value();

        // Rendering: each file in turn; its frames go to its render temporary (or straight to its file).
        state_.store (ExportJobState::Rendering, std::memory_order_release);
        for (Output& output : outputs)
        {
            // A stem's helper nodes carry hashed ids; in the (vanishingly rare) case one collides with a project node the
            // build fails, and another salt is tried.
            engine::OfflineRenderResult rendered;
            for (std::uint32_t salt = 0; salt < 3u; ++salt)
            {
                engine::OfflineRenderOptions options;
                options.progressFrames = &rendered_;
                options.progressTotalFrames = &renderTotal_;
                options.cancel = &cancelRequested_;
                options.latch = latch_;
                options.exportRange = snapshot_.range;
                options.stemStrip = output.stem;
                options.stemSalt = salt;
                options.assetOwners = ownership;   // ADR-0059
                rendered_.store (0, std::memory_order_relaxed);
                rendered = engine::renderOfflineProject (
                    snapshot_.project, std::span<const engine::DecodedAssetAudio> (views.data(), views.size()), std::move (options));
                if (rendered.status != engine::OfflineRenderStatus::MixerProjectionFailed || ! output.stem.has_value())
                    break;
            }
            if (rendered.status == engine::OfflineRenderStatus::Cancelled || cancelled())
                return cancelledOut();
            if (rendered.status == engine::OfflineRenderStatus::RangeOutsideRender)
                return fail (ExportFailure::Range, "Export failed: the loop range is outside the rendered project");
            if (! rendered.ok())
                return fail (ExportFailure::Render, "Export failed: the project render failed");

            std::span<const float> samples (rendered.interleavedSamples.data(), rendered.interleavedSamples.size());
            std::uint64_t frames = rendered.frames;
            if (snapshot_.range.has_value())
            {
                const std::uint64_t start = std::min (snapshot_.range->first, rendered.frames);
                frames = rendered.frames - start;
                samples = samples.subspan (static_cast<std::size_t> (start) * rendered.channels);
            }
            output.frames = frames;
            output.channels = rendered.channels;
            for (const float sample : samples)
                output.peak = std::max (output.peak, static_cast<double> (std::abs (sample)));
            output.direct = ! secondPass;
            writeTotal_.fetch_add (frames, std::memory_order_relaxed);

            const std::filesystem::path target = output.direct ? partialPathFor (output.destination, id_)
                                                               : renderPathFor (output.destination, id_);
            io::WavStreamWriter writer;
            if (const io::WavResult opened = writer.open (target, rendered.sampleRate, rendered.channels, frames, 32u); ! opened.ok())
                return fail (ExportFailure::Write, "Export failed: could not write " + yesdaw::io::utf8Text (output.destination.filename()) + ": " + opened.message);
            std::string failure;
            if (! writeChunks (writer, samples, rendered.channels, 1.0, nullptr, failure, output.direct))
            {
                writer.abandon();
                return failure.empty() ? cancelledOut()
                                       : fail (ExportFailure::Write, "Export failed: could not write " + yesdaw::io::utf8Text (output.destination.filename()) + ": " + failure);
            }
            if (const io::WavResult finished = writer.finish(); ! finished.ok())
                return fail (ExportFailure::Write, "Export failed: could not write " + yesdaw::io::utf8Text (output.destination.filename()) + ": " + finished.message);
            filesRendered_.fetch_add (1u, std::memory_order_relaxed);
        }

        // Writing: one gain for every file (from the loudest), then each file's format and dither.
        state_.store (ExportJobState::Writing, std::memory_order_release);
        double gain = 1.0;
        if (snapshot_.normalizePeakDbfs.has_value())
        {
            double loudest = 0.0;
            for (const Output& output : outputs)
                loudest = std::max (loudest, output.peak);
            gain = exportNormalizeGain (loudest, *snapshot_.normalizePeakDbfs);
        }
        for (const Output& output : outputs)
        {
            if (output.direct)
                continue;
            io::Float32Wav rendered;
            if (const io::WavResult read = io::readFloat32WavFile (renderPathFor (output.destination, id_), rendered); ! read.ok())
                return fail (ExportFailure::Write, "Export failed: could not read back " + yesdaw::io::utf8Text (output.destination.filename()) + ": " + read.message);
            io::WavStreamWriter writer;
            if (const io::WavResult opened = writer.open (partialPathFor (output.destination, id_), rendered.sampleRate, rendered.channels, rendered.frames, bits); ! opened.ok())
                return fail (ExportFailure::Write, "Export failed: could not write " + yesdaw::io::utf8Text (output.destination.filename()) + ": " + opened.message);
            std::optional<io::TpdfDither> dither;
            if (snapshot_.dither && bits != 32u)
                dither.emplace (output.index, rendered.channels);
            std::string failure;
            if (! writeChunks (writer, rendered.interleavedSamples, rendered.channels, gain, dither.has_value() ? &*dither : nullptr, failure))
            {
                writer.abandon();
                return failure.empty() ? cancelledOut()
                                       : fail (ExportFailure::Write, "Export failed: could not write " + yesdaw::io::utf8Text (output.destination.filename()) + ": " + failure);
            }
            if (const io::WavResult finished = writer.finish(); ! finished.ok())
                return fail (ExportFailure::Write, "Export failed: could not write " + yesdaw::io::utf8Text (output.destination.filename()) + ": " + finished.message);
            std::error_code removed;
            std::filesystem::remove (renderPathFor (output.destination, id_), removed);
        }
        if (cancelled())
            return cancelledOut();

        // Committing: every file renamed into place; Cancel is ignored from here.
        state_.store (ExportJobState::Committing, std::memory_order_release);
        std::string committed;
        for (std::size_t i = 0; i < outputs.size(); ++i)
        {
            std::error_code renamed;
            std::filesystem::rename (partialPathFor (outputs[i].destination, id_), outputs[i].destination, renamed);
            if (renamed)
            {
                cleanUp();
                return finish (ExportJobState::Failed, ExportFailure::Write,
                               "Export failed: could not replace " + yesdaw::io::utf8Text (outputs[i].destination.filename()) + ": "
                                   + renamed.message() + (committed.empty() ? std::string() : " (committed: " + committed + ")"));
            }
            committed += (committed.empty() ? "" : ", ") + yesdaw::io::utf8Text (outputs[i].destination.filename());
        }
        finish (ExportJobState::Succeeded, ExportFailure::None, "Exported " + std::to_string (outputs.size()) + " files");
    }

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
    std::atomic<std::uint64_t> fileCount_ { 1 };        // cp3: the files a stems export writes
    std::atomic<std::uint64_t> filesRendered_ { 0 };
    mutable std::atomic<int> lastPercent_ { 0 };
    std::string message_;
    ExportFailure failure_ = ExportFailure::None;
    std::thread worker_;   // last: started after every member it reads exists, joined before any is destroyed
};

} // namespace yesdaw::app
