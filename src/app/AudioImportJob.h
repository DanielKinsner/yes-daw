// YES DAW - owned, one-shot audio import preparation (ADR-0080).
// Control alone adopts the move-only persistence handoff. No model, database or UI callback is owned here.
// Cancellation invalidates adoption immediately; a monolithic codec/resample call may still finish.
#pragma once

#include "engine/RateMatchedView.h"
#include "io/AudioImportFormat.h"
#include "persistence/ProjectBundle.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace yesdaw::app {

enum class AudioImportState : std::uint8_t { Preparing, Succeeded, Failed, Cancelled };
enum class AudioImportPhase : std::uint8_t { Waiting, Staging, Decoding, Resampling, Completing, Finished };

struct AudioImportKey
{
    std::uint64_t requestId = 0;
    std::uint64_t requestVersion = 0;
    std::uint64_t attachGeneration = 0;
    engine::EntityId projectId;
    engine::Tick intendedPosition = 0;
    std::vector<engine::EntityId> targetTrackIds;
    bool appendTracks = false;

    friend bool operator== (const AudioImportKey&, const AudioImportKey&) = default;
};

struct AudioImportFile
{
    std::filesystem::path sourcePath;
    engine::EntityId assetId;
};

using AudioImportDecoder = io::AudioDecodeResult (*) (const std::filesystem::path&);

struct AudioImportRequest
{
    AudioImportKey key;
    std::filesystem::path destinationBundle;
    engine::EntityId stageId;
    double viewRateHz = 0.0;
    std::vector<AudioImportFile> files;
    // The shell supplies io::decodeAudioFile. A capture-free function cannot retain the live model.
    AudioImportDecoder decoder = nullptr;
};

struct AudioImportItem
{
    std::filesystem::path sourcePath;
    std::optional<std::size_t> preparedIndex;
    std::string reason; // named per-file refusal; empty exactly when preparedIndex is present
};

struct PreparedAudioImport
{
    persistence::PreparedAssetImport preparedAsset;
    std::shared_ptr<const engine::AssetSamples> source;
    std::shared_ptr<const engine::AssetSamples> view;
};

struct AudioImportResult
{
    AudioImportResult() = default;
    AudioImportResult (AudioImportResult&&) noexcept = default;
    AudioImportResult& operator= (AudioImportResult&&) noexcept = default;
    AudioImportResult (const AudioImportResult&) = delete;
    AudioImportResult& operator= (const AudioImportResult&) = delete;

    AudioImportKey key;
    double viewRateHz = 0.0;
    std::vector<AudioImportItem> items; // original logical order, including refusals
    std::vector<PreparedAudioImport> prepared; // one immutable source/view owner per unique successful Asset
};

class AudioImportJob final
{
public:
    explicit AudioImportJob (AudioImportRequest request) : request_ (std::move (request)) {}
    AudioImportJob (const AudioImportJob&) = delete;
    AudioImportJob& operator= (const AudioImportJob&) = delete;

    ~AudioImportJob()
    {
        requestCancel();
        join();
    }

    // Control thread only, once. Thread creation/worker exceptions never escape into a UI caller.
    [[nodiscard]] bool start() noexcept
    {
        if (started_)
            return false;
        started_ = true;
        try
        {
            worker_ = std::thread ([this] {
                try { run(); }
                catch (...)
                {
                    result_.reset();
                    pendingState_ = cancelRequested() ? AudioImportState::Cancelled : AudioImportState::Failed;
                }
                // run's local snapshots, failed buffers and exception stack have unwound before terminal.
                phase_.store (AudioImportPhase::Finished, std::memory_order_relaxed);
                state_.store (pendingState_, std::memory_order_release);
            });
        }
        catch (...)
        {
            pendingState_ = AudioImportState::Failed;
            phase_.store (AudioImportPhase::Finished, std::memory_order_relaxed);
            state_.store (pendingState_, std::memory_order_release);
            return false;
        }
        return true;
    }

    void requestCancel() noexcept { stop_.request_stop(); }
    [[nodiscard]] bool cancelRequested() const noexcept { return stop_.stop_requested(); }
    [[nodiscard]] AudioImportState state() const noexcept { return state_.load (std::memory_order_acquire); }
    [[nodiscard]] bool terminal() const noexcept { return state() != AudioImportState::Preparing; }
    [[nodiscard]] AudioImportPhase phase() const noexcept { return phase_.load (std::memory_order_relaxed); }
    [[nodiscard]] std::size_t completedFiles() const noexcept { return completedFiles_.load (std::memory_order_relaxed); }
    [[nodiscard]] std::size_t totalFiles() const noexcept { return request_.files.size(); }
    [[nodiscard]] const AudioImportKey& key() const noexcept { return request_.key; }

    // The owner must also revalidate current Project/generation, request version and captured target intent.
    [[nodiscard]] bool eligibleForAdoption() const noexcept
    {
        return state() == AudioImportState::Succeeded && ! cancelRequested() && result_ != nullptr;
    }

    // Control thread only; one move-only handoff, after acquiring terminal. Cancellation is still not adoption.
    [[nodiscard]] std::unique_ptr<AudioImportResult> takeResult() noexcept
    {
        return state() == AudioImportState::Succeeded ? std::move (result_) : nullptr;
    }

    [[nodiscard]] std::string failureReason() const
    {
        return state() == AudioImportState::Failed
            ? (failureReason_.empty() ? "audio import worker could not complete" : failureReason_) : std::string {};
    }

    // Ordinary ticks/retirement join only after terminal. Final teardown may wait for a monolithic stage.
    void join()
    {
        if (worker_.joinable())
            worker_.join();
    }

private:
    [[nodiscard]] bool stopIfCancelled() noexcept
    {
        if (! cancelRequested())
            return false;
        pendingState_ = AudioImportState::Cancelled;
        return true;
    }

    [[nodiscard]] static std::string decodedRefusal (const io::DecodedAudioFile& audio)
    {
        if (audio.channels != 1 && audio.channels != 2)
            return std::to_string (audio.channels) + " channels (mono or stereo only)";
        if (! io::importSampleRateSupported (audio.sampleRateHz))
            return "sample rate must be between 8000 and 384000 Hz";
        if (audio.frames == 0)
            return "no audio in the file";
        if (audio.frames > static_cast<std::uint64_t> (std::numeric_limits<int>::max()))
            return "too long to import";
        if (audio.interleaved.size() % audio.channels != 0 || audio.interleaved.size() / audio.channels != audio.frames)
            return "decoded audio does not match its recorded length and channels";
        for (const float sample : audio.interleaved)
            if (! std::isfinite (sample))
                return "decoded audio contains non-finite samples";
        return {};
    }

    void run()
    {
        if (stopIfCancelled())
            return;
        if (request_.decoder == nullptr || ! std::isfinite (request_.viewRateHz) || request_.viewRateHz <= 0.0)
        {
            failureReason_ = "audio import requires a decoder and a valid live sample rate";
            pendingState_ = AudioImportState::Failed;
            return;
        }
        persistence::AssetImportStaging staging;
        if (const auto created = persistence::AssetImportStaging::create (
                request_.destinationBundle, request_.stageId, staging); ! created.ok())
        {
            failureReason_ = created.message;
            pendingState_ = AudioImportState::Failed;
            return;
        }
        auto result = std::make_unique<AudioImportResult>();
        result->key = request_.key;
        result->viewRateHz = request_.viewRateHz;
        for (std::size_t index = 0; index < request_.files.size(); ++index)
        {
            if (stopIfCancelled())
                return;
            const AudioImportFile& file = request_.files[index];
            AudioImportItem item { file.sourcePath, std::nullopt, {} };
            // Scope each snapshot/refusal so completed counts never include still-unwinding private files.
            {
                phase_.store (AudioImportPhase::Staging, std::memory_order_relaxed);
                persistence::StagedAssetImport snapshot;
                const auto staged = staging.stageSource (file.sourcePath, index, snapshot, stop_.get_token());
                if (stopIfCancelled())
                    return;
                if (! staged.ok())
                    item.reason = staged.message;
                else
                    prepareFile (snapshot, file.assetId, *result, item);
                if (stopIfCancelled())
                    return;
            }
            result->items.push_back (std::move (item));
            completedFiles_.store (index + 1, std::memory_order_relaxed);
        }
        if (stopIfCancelled())
            return;
        result_ = std::move (result);
        pendingState_ = AudioImportState::Succeeded;
    }

    void prepareFile (persistence::StagedAssetImport& snapshot, engine::EntityId assetId,
                      AudioImportResult& result, AudioImportItem& item)
    {
        phase_.store (AudioImportPhase::Decoding, std::memory_order_relaxed);
        // Always honor this original extension's canonical decoder policy, even for identical content.
        auto decoded = request_.decoder (snapshot.snapshotPath());
        if (cancelRequested())
            return;
        if (! decoded.audio)
        {
            item.reason = decoded.reason.empty() ? "audio file could not be decoded" : std::move (decoded.reason);
            return;
        }
        auto& audio = *decoded.audio;
        item.reason = decodedRefusal (audio);
        if (! item.reason.empty() || cancelRequested())
            return;
        const auto* format = io::importAudioFormatForPath (snapshot.originalFilename());
        if (format == nullptr || format->kind != audio.format)
        {
            item.reason = "decoded audio does not match its original import format";
            return;
        }
        phase_.store (AudioImportPhase::Completing, std::memory_order_relaxed);
        persistence::PreparedAssetImport prepared;
        const auto completed = std::move (snapshot).complete (
            assetId, audio.frames, engine::SampleRate { audio.sampleRateHz }, audio.channels, prepared);
        if (! completed.ok())
        {
            item.reason = completed.message;
            return;
        }
        const auto& asset = prepared.asset();
        for (std::size_t index = 0; index < result.prepared.size(); ++index)
        {
            const auto& existing = result.prepared[index].preparedAsset.asset();
            if (existing.contentHash != asset.contentHash)
                continue;
            if (existing.frames != asset.frames || existing.sampleRate != asset.sampleRate || existing.channels != asset.channels)
                item.reason = "same content hash has conflicting Asset metadata";
            else
                item.preparedIndex = index;
            return; // only the redundant private snapshot is discarded
        }
        const double viewFrames = static_cast<double> (audio.frames) * request_.viewRateHz / audio.sampleRateHz;
        if (! std::isfinite (viewFrames) || viewFrames < 0.5
            || viewFrames >= static_cast<double> (std::numeric_limits<std::int64_t>::max()))
        {
            item.reason = "audio has an unsupported live view length";
            return;
        }
        auto source = std::make_shared<engine::AssetSamples>();
        source->channels = static_cast<int> (audio.channels);
        source->frames = audio.frames;
        source->interleaved = std::move (audio.interleaved);
        std::shared_ptr<const engine::AssetSamples> view = source;
        if (audio.sampleRateHz != request_.viewRateHz)
        {
            if (cancelRequested())
                return;
            phase_.store (AudioImportPhase::Resampling, std::memory_order_relaxed);
            view = engine::buildRateMatchedSamples (source->interleaved, audio.channels, audio.sampleRateHz,
                                                   request_.viewRateHz, engine::ResampleQuality::LivePlayback);
        }
        if (cancelRequested())
            return;
        item.preparedIndex = result.prepared.size();
        result.prepared.push_back ({ std::move (prepared), std::move (source), std::move (view) });
    }

    const AudioImportRequest request_;
    bool started_ = false;
    std::thread worker_;
    std::stop_source stop_;
    std::atomic<AudioImportState> state_ { AudioImportState::Preparing };
    std::atomic<AudioImportPhase> phase_ { AudioImportPhase::Waiting };
    std::atomic<std::size_t> completedFiles_ { 0 };
    // Only read after acquire of terminal state; takeResult is an owner-thread operation.
    AudioImportState pendingState_ = AudioImportState::Preparing;
    std::string failureReason_;
    std::unique_ptr<AudioImportResult> result_;
};

} // namespace yesdaw::app
