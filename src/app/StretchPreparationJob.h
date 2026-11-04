// YES DAW - owned, one-shot stretch preparation (ADR-0080). Pure C++; never used on the Audio thread.
//
// The Control thread captures immutable owners and intent without preparing a view. The worker prepares a
// missing live-tier view and the stretch, then publishes its immutable result before a terminal state. No model,
// Project, database or undo handle crosses this boundary. The owner alone revalidates and adopts the result.
// Cancellation invalidates adoption immediately; a monolithic resample/Signalsmith call may still finish.
// Keep cancelled running jobs in the owner's retiring list. Only final Control-thread teardown may join early.

#pragma once

#include "engine/RateMatchedView.h"
#include "engine/TimeStretch.h"

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <thread>
#include <utility>

namespace yesdaw::app {

enum class StretchPreparationState : std::uint8_t
{
    Preparing,
    Succeeded,
    Failed,
    Cancelled
};

[[nodiscard]] constexpr bool stretchPreparationTerminal (StretchPreparationState state) noexcept
{
    return state != StretchPreparationState::Preparing;
}

enum class StretchPreparationFailure : std::uint8_t
{
    None,
    InvalidRequest,
    TimeStretch,
    Unexpected
};

struct StretchPreparationKey
{
    std::uint64_t requestId = 0;
    std::uint64_t requestVersion = 0;
    std::uint64_t attachGeneration = 0;
    engine::EntityId projectId;
    engine::EntityId trackId;
    engine::EntityId clipId;
    engine::EntityId assetId;
    engine::AssetContentHash contentHash;
    // Both ownership and identity: retaining the source prevents its address being reused while this key lives.
    std::shared_ptr<const engine::AssetSamples> source;
    double sourceRateHz = 0.0;
    double viewRateHz = 0.0;
    std::uint16_t channels = 0;
    std::uint64_t srcOffset = 0; // Asset frames, never view frames
    std::uint64_t srcLen = 0;    // Asset frames, never view frames
    double stretchFactor = 1.0;

    friend bool operator== (const StretchPreparationKey&, const StretchPreparationKey&) noexcept = default;
};

struct StretchPreparationRequest
{
    StretchPreparationKey key;
    // Optional ready live-tier view for this exact source/rates key, obtained by lookup only. A missing cross-rate
    // view is an explicit request for worker-side preparation. Same-rate requests always use the source owner.
    std::shared_ptr<const engine::AssetSamples> readyView;
};

struct StretchPreparationResult
{
    StretchPreparationKey key;
    engine::ViewWindow window; // the key's Asset window mapped by both ends into view frames (ADR-0055)
    std::shared_ptr<const engine::AssetSamples> viewSamples;
    std::shared_ptr<const engine::AssetSamples> samples;
};

class StretchPreparationJob final
{
public:
    explicit StretchPreparationJob (StretchPreparationRequest request) noexcept : request_ (std::move (request)) {}

    StretchPreparationJob (const StretchPreparationJob&) = delete;
    StretchPreparationJob& operator= (const StretchPreparationJob&) = delete;

    ~StretchPreparationJob()
    {
        requestCancel();
        join();
    }

    // Control thread only, once. Thread creation failure is a terminal failure, not an escaping UI exception.
    [[nodiscard]] bool start() noexcept
    {
        if (started_)
            return false;
        started_ = true;
        try
        {
            worker_ = std::thread ([this] {
                try
                {
                    run();
                }
                catch (...)
                {
                    const bool cancelled = cancelRequested();
                    setOutcome (cancelled ? StretchPreparationState::Cancelled : StretchPreparationState::Failed,
                                cancelled ? StretchPreparationFailure::None : StretchPreparationFailure::Unexpected);
                }
                // Publish only after run's buffers and any exception stack have unwound. An ordinary owner
                // tick observing terminal must not join while a cancelled preparation is still freeing audio.
                state_.store (pendingState_, std::memory_order_release);
            });
        }
        catch (...)
        {
            setOutcome (StretchPreparationState::Failed, StretchPreparationFailure::Unexpected);
            state_.store (pendingState_, std::memory_order_release);
            return false;
        }
        return true;
    }

    void requestCancel() noexcept { cancelRequested_.store (true, std::memory_order_release); }
    [[nodiscard]] bool cancelRequested() const noexcept { return cancelRequested_.load (std::memory_order_acquire); }
    [[nodiscard]] StretchPreparationState state() const noexcept { return state_.load (std::memory_order_acquire); }
    [[nodiscard]] bool terminal() const noexcept { return stretchPreparationTerminal (state()); }
    [[nodiscard]] const StretchPreparationKey& key() const noexcept { return request_.key; }

    // Local eligibility only. The owner must also check its current Project/generation, target, request version
    // and dependencies, including geometry/time-base dependencies of an absolute-end gesture, before adoption.
    [[nodiscard]] bool eligibleForAdoption() const noexcept
    {
        return state() == StretchPreparationState::Succeeded && ! cancelRequested();
    }

    // The acquire observes the entire immutable handoff. Later cancellation leaves an observable successful
    // result intact but makes it ineligible; holding this pointer is never permission to adopt it.
    [[nodiscard]] std::shared_ptr<const StretchPreparationResult> result() const noexcept
    {
        return state() == StretchPreparationState::Succeeded ? result_ : nullptr;
    }

    [[nodiscard]] StretchPreparationFailure failure() const noexcept
    {
        return terminal() ? failure_ : StretchPreparationFailure::None;
    }

    [[nodiscard]] engine::TimeStretchStatus preparationStatus() const noexcept
    {
        return terminal() ? preparationStatus_ : engine::TimeStretchStatus::Ok;
    }

    // Control thread only: ordinary service/replacement calls this only after observing a terminal state.
    // Final teardown may wait for an in-flight monolithic stage; it never detaches the worker.
    void join()
    {
        if (worker_.joinable())
            worker_.join();
    }

private:
    void setOutcome (StretchPreparationState state, StretchPreparationFailure failure = StretchPreparationFailure::None,
                     engine::TimeStretchStatus preparationStatus = engine::TimeStretchStatus::Ok) noexcept
    {
        failure_ = failure;
        preparationStatus_ = preparationStatus;
        pendingState_ = state;
    }

    [[nodiscard]] bool stopIfCancelled() noexcept
    {
        if (! cancelRequested())
            return false;
        setOutcome (StretchPreparationState::Cancelled);
        return true;
    }

    [[nodiscard]] static bool bufferMatches (const std::shared_ptr<const engine::AssetSamples>& samples,
                                            std::uint16_t channels, std::uint64_t frames) noexcept
    {
        return samples != nullptr && samples->channels == static_cast<int> (channels) && samples->frames == frames
            && frames > 0 && samples->interleaved.size() % channels == 0
            && samples->interleaved.size() / channels == frames;
    }

    void run()
    {
        if (stopIfCancelled())
            return;
        const StretchPreparationKey& key = request_.key;
        if ((key.channels != 1 && key.channels != 2) || key.source == nullptr
            || ! bufferMatches (key.source, key.channels, key.source->frames)
            || ! std::isfinite (key.sourceRateHz) || key.sourceRateHz <= 0.0
            || ! std::isfinite (key.viewRateHz) || key.viewRateHz <= 0.0
            || key.srcLen == 0 || key.srcOffset > key.source->frames
            || key.srcLen > key.source->frames - key.srcOffset)
            return setOutcome (StretchPreparationState::Failed, StretchPreparationFailure::InvalidRequest);

        if (! std::isfinite (key.stretchFactor) || key.stretchFactor < 0.5 || key.stretchFactor > 2.0)
            return setOutcome (StretchPreparationState::Failed, StretchPreparationFailure::TimeStretch,
                               engine::TimeStretchStatus::InvalidStretchFactor);

        const double viewFramesExact = static_cast<double> (key.source->frames) * key.viewRateHz / key.sourceRateHz;
        if (! std::isfinite (viewFramesExact) || viewFramesExact < 0.5
            || viewFramesExact >= static_cast<double> (std::numeric_limits<std::int64_t>::max()))
            return setOutcome (StretchPreparationState::Failed, StretchPreparationFailure::InvalidRequest);
        const std::uint64_t viewFrames = key.sourceRateHz == key.viewRateHz ? key.source->frames
            : engine::resampledFrameCount (key.source->frames, key.sourceRateHz, key.viewRateHz);

        std::shared_ptr<const engine::AssetSamples> view = key.source;
        if (key.sourceRateHz != key.viewRateHz)
        {
            view = request_.readyView;
            if (view == nullptr)
                view = engine::buildRateMatchedSamples (key.source->interleaved, key.channels,
                    key.sourceRateHz, key.viewRateHz, engine::ResampleQuality::LivePlayback);
        }
        if (stopIfCancelled())
            return;
        if (! bufferMatches (view, key.channels, viewFrames))
            return setOutcome (StretchPreparationState::Failed, StretchPreparationFailure::InvalidRequest);

        const engine::ViewWindow window = engine::mapWindowToView (
            key.srcOffset, key.srcLen, engine::rateRatio (key.sourceRateHz, key.viewRateHz), viewFrames);
        if (window.srcLen == 0)
            return setOutcome (StretchPreparationState::Failed, StretchPreparationFailure::InvalidRequest);
        const std::size_t first = static_cast<std::size_t> (window.srcOffset) * key.channels;
        const std::size_t count = static_cast<std::size_t> (window.srcLen) * key.channels;
        engine::PreparedTimeStretch prepared = engine::prepareTimeStretch (
            std::span<const float> (view->interleaved).subspan (first, count),
            key.channels, key.viewRateHz, key.stretchFactor);
        if (stopIfCancelled())
            return;
        if (! prepared.ok())
            return setOutcome (StretchPreparationState::Failed, StretchPreparationFailure::TimeStretch, prepared.status);

        auto samples = std::make_shared<engine::AssetSamples>();
        samples->channels = static_cast<int> (key.channels);
        samples->frames = prepared.outputFrames;
        samples->interleaved = std::move (prepared.interleavedSamples);
        result_ = std::make_shared<const StretchPreparationResult> (
            StretchPreparationResult { key, window, std::move (view), std::move (samples) });
        setOutcome (StretchPreparationState::Succeeded);
    }

    const StretchPreparationRequest request_;
    bool started_ = false; // Control-thread start/join/destruction only
    std::thread worker_;
    std::atomic<bool> cancelRequested_ { false };
    std::atomic<StretchPreparationState> state_ { StretchPreparationState::Preparing };
    // Written before the release terminal state; read only after its acquire. Never changed once terminal.
    std::shared_ptr<const StretchPreparationResult> result_;
    StretchPreparationState pendingState_ = StretchPreparationState::Preparing;
    StretchPreparationFailure failure_ = StretchPreparationFailure::None;
    engine::TimeStretchStatus preparationStatus_ = engine::TimeStretchStatus::Ok;
};

}
