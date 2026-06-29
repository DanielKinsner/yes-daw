// YES DAW - H10 device hot-swap survival coordinator.
//
// This is a control-side wrapper around PlaybackEngine. Device callbacks still call one RT-hot
// processBlock path only; stopping the old callback, rebuilding for a new device max Block size, and
// restoring transport all happen off the audio thread.

#pragma once

#include "engine/PlaybackEngine.h"
#include "rt/RtHot.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace yesdaw::engine {

struct DeviceHotSwapFormat
{
    std::uint32_t deviceId = 0;
    SampleRate    sampleRate;
    std::uint16_t outputChannels = 0;
    int           maxBlockSize = 0;
};

enum class DeviceHotSwapStatus : std::uint8_t
{
    Ok = 0,
    InvalidMaxBlockSize,
    UnsupportedSampleRate,
    UnsupportedChannelCount,
    CallbackAlreadyActive,
    CallbackStopped,
    NoEngine,
    BuildFailed
};

struct DeviceHotSwapResult
{
    DeviceHotSwapStatus status = DeviceHotSwapStatus::Ok;
    OfflineRenderStatus buildStatus = OfflineRenderStatus::Ok;
    ProjectMixerProjectionError projectError;
    MixerProjectionError mixerError;

    [[nodiscard]] bool ok() const noexcept { return status == DeviceHotSwapStatus::Ok; }
};

class DeviceHotSwapCoordinator
{
public:
    struct CreateResult
    {
        DeviceHotSwapResult result;
        std::unique_ptr<DeviceHotSwapCoordinator> coordinator;

        [[nodiscard]] bool ok() const noexcept
        {
            return result.ok() && coordinator != nullptr;
        }
    };

    [[nodiscard]] static CreateResult create (Project project,
                                              std::span<const DecodedAssetAudio> decodedAssets,
                                              DeviceHotSwapFormat format,
                                              OfflineRenderOptions options = {})
    {
        CreateResult out;
        if (format.maxBlockSize <= 0)
        {
            out.result.status = DeviceHotSwapStatus::InvalidMaxBlockSize;
            return out;
        }

        if (format.sampleRate != project.sampleRate)
        {
            out.result.status = DeviceHotSwapStatus::UnsupportedSampleRate;
            return out;
        }

        std::vector<DecodedAssetAudio> decodedCopy (decodedAssets.begin(), decodedAssets.end());
        options.maxBlockSize = format.maxBlockSize;
        PlaybackEngine::Result built = PlaybackEngine::create (
            project,
            std::span<const DecodedAssetAudio> (decodedCopy.data(), decodedCopy.size()),
            options);

        out.result.buildStatus = built.status;
        out.result.projectError = built.projectError;
        out.result.mixerError = built.mixerError;
        if (! built.ok())
        {
            out.result.status = DeviceHotSwapStatus::BuildFailed;
            return out;
        }

        if (built.engine->channels() != format.outputChannels)
        {
            out.result.status = DeviceHotSwapStatus::UnsupportedChannelCount;
            return out;
        }

        out.coordinator.reset (new DeviceHotSwapCoordinator (
            std::move (project),
            std::move (decodedCopy),
            format,
            options,
            std::move (built.engine)));
        return out;
    }

    DeviceHotSwapCoordinator (const DeviceHotSwapCoordinator&) = delete;
    DeviceHotSwapCoordinator& operator= (const DeviceHotSwapCoordinator&) = delete;

    [[nodiscard]] DeviceHotSwapStatus startCallback() noexcept
    {
        if (engine_ == nullptr)
            return DeviceHotSwapStatus::NoEngine;

        callbackActive_.store (true, std::memory_order_release);
        return DeviceHotSwapStatus::Ok;
    }

    [[nodiscard]] DeviceHotSwapStatus stopCallback() noexcept
    {
        if (engine_ == nullptr)
            return DeviceHotSwapStatus::NoEngine;

        callbackActive_.store (false, std::memory_order_release);
        return DeviceHotSwapStatus::Ok;
    }

    // AUDIO THREAD / fake device callback: only dispatches to the active PlaybackEngine.
    [[nodiscard]] DeviceHotSwapStatus processBlock (float* const* outChannels,
                                                    int numOutputChannels,
                                                    int numFrames) noexcept YESDAW_RT_HOT
    {
        if (engine_ == nullptr)
        {
            zeroOutputs (outChannels, numOutputChannels, numFrames);
            return DeviceHotSwapStatus::NoEngine;
        }

        if (numFrames < 0 || numFrames > format_.maxBlockSize)
        {
            zeroOutputs (outChannels, numOutputChannels, numFrames);
            return DeviceHotSwapStatus::InvalidMaxBlockSize;
        }

        if (numOutputChannels != static_cast<int> (format_.outputChannels))
        {
            zeroOutputs (outChannels, numOutputChannels, numFrames);
            return DeviceHotSwapStatus::UnsupportedChannelCount;
        }

        if (! callbackActive_.load (std::memory_order_acquire))
        {
            callbackWhileStoppedCount_.fetch_add (1u, std::memory_order_relaxed);
            zeroOutputs (outChannels, numOutputChannels, numFrames);
            return DeviceHotSwapStatus::CallbackStopped;
        }

        engine_->processBlock (outChannels, numOutputChannels, numFrames);
        return DeviceHotSwapStatus::Ok;
    }

    // CONTROL THREAD: old callback must be stopped before this rebuilds and replaces the engine.
    [[nodiscard]] DeviceHotSwapResult swapDevice (DeviceHotSwapFormat nextFormat)
    {
        DeviceHotSwapResult result;
        if (engine_ == nullptr)
        {
            result.status = DeviceHotSwapStatus::NoEngine;
            return result;
        }

        if (callbackActive_.load (std::memory_order_acquire))
        {
            result.status = DeviceHotSwapStatus::CallbackAlreadyActive;
            return result;
        }

        if (nextFormat.maxBlockSize <= 0)
        {
            result.status = DeviceHotSwapStatus::InvalidMaxBlockSize;
            return result;
        }

        if (nextFormat.sampleRate != project_.sampleRate || nextFormat.sampleRate != format_.sampleRate)
        {
            result.status = DeviceHotSwapStatus::UnsupportedSampleRate;
            return result;
        }

        if (nextFormat.outputChannels != format_.outputChannels || nextFormat.outputChannels != engine_->channels())
        {
            result.status = DeviceHotSwapStatus::UnsupportedChannelCount;
            return result;
        }

        const TransportSnapshot snapshot = snapshotTransport();
        OfflineRenderOptions nextOptions = options_;
        nextOptions.maxBlockSize = nextFormat.maxBlockSize;
        PlaybackEngine::Result built = PlaybackEngine::create (
            project_,
            std::span<const DecodedAssetAudio> (decodedAssets_.data(), decodedAssets_.size()),
            nextOptions);

        result.buildStatus = built.status;
        result.projectError = built.projectError;
        result.mixerError = built.mixerError;
        if (! built.ok())
        {
            result.status = DeviceHotSwapStatus::BuildFailed;
            return result;
        }

        if (! primeEngine (*built.engine, snapshot))
        {
            result.status = DeviceHotSwapStatus::BuildFailed;
            return result;
        }

        engine_ = std::move (built.engine);
        format_ = nextFormat;
        options_ = nextOptions;
        result.status = DeviceHotSwapStatus::Ok;
        return result;
    }

    [[nodiscard]] bool playTransport() noexcept
    {
        return engine_ != nullptr && engine_->play();
    }

    [[nodiscard]] bool stopTransport() noexcept
    {
        return engine_ != nullptr && engine_->stop();
    }

    [[nodiscard]] bool locate (std::int64_t timelineFrame) noexcept
    {
        return engine_ != nullptr && engine_->locate (timelineFrame);
    }

    [[nodiscard]] bool setLoop (std::int64_t startFrame, std::int64_t endFrame) noexcept
    {
        return engine_ != nullptr && engine_->setLoop (startFrame, endFrame);
    }

    [[nodiscard]] bool clearLoop() noexcept
    {
        return engine_ != nullptr && engine_->clearLoop();
    }

    [[nodiscard]] const DeviceHotSwapFormat& format() const noexcept { return format_; }
    [[nodiscard]] bool callbackActive() const noexcept { return callbackActive_.load (std::memory_order_acquire); }
    [[nodiscard]] std::uint32_t callbackWhileStoppedCount() const noexcept
    {
        return callbackWhileStoppedCount_.load (std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint16_t channels() const noexcept { return engine_ == nullptr ? 0 : engine_->channels(); }
    [[nodiscard]] std::uint64_t frames() const noexcept { return engine_ == nullptr ? 0 : engine_->frames(); }
    [[nodiscard]] int maxBlockSize() const noexcept { return engine_ == nullptr ? 0 : engine_->maxBlockSize(); }
    [[nodiscard]] bool isPlaying() const noexcept { return engine_ != nullptr && engine_->isPlaying(); }
    [[nodiscard]] bool loopEnabled() const noexcept { return engine_ != nullptr && engine_->loopEnabled(); }
    [[nodiscard]] std::int64_t playheadFrame() const noexcept { return engine_ == nullptr ? 0 : engine_->playheadFrame(); }
    [[nodiscard]] std::int64_t loopStartFrame() const noexcept { return engine_ == nullptr ? 0 : engine_->loopStartFrame(); }
    [[nodiscard]] std::int64_t loopEndFrame() const noexcept { return engine_ == nullptr ? 0 : engine_->loopEndFrame(); }

private:
    struct TransportSnapshot
    {
        std::int64_t playheadFrame = 0;
        std::int64_t loopStartFrame = 0;
        std::int64_t loopEndFrame = 0;
        bool playing = true;
        bool loopEnabled = false;
    };

    DeviceHotSwapCoordinator (Project project,
                              std::vector<DecodedAssetAudio> decodedAssets,
                              DeviceHotSwapFormat format,
                              OfflineRenderOptions options,
                              std::unique_ptr<PlaybackEngine> engine)
        : project_ (std::move (project)),
          decodedAssets_ (std::move (decodedAssets)),
          format_ (format),
          options_ (options),
          engine_ (std::move (engine))
    {
    }

    [[nodiscard]] TransportSnapshot snapshotTransport() const noexcept
    {
        TransportSnapshot snapshot;
        snapshot.playheadFrame = engine_->playheadFrame();
        snapshot.playing = engine_->isPlaying();
        snapshot.loopEnabled = engine_->loopEnabled();
        snapshot.loopStartFrame = engine_->loopStartFrame();
        snapshot.loopEndFrame = engine_->loopEndFrame();
        return snapshot;
    }

    [[nodiscard]] static bool primeEngine (PlaybackEngine& engine, TransportSnapshot snapshot) noexcept
    {
        if (! engine.locate (snapshot.playheadFrame))
            return false;

        if (snapshot.loopEnabled)
        {
            if (! engine.setLoop (snapshot.loopStartFrame, snapshot.loopEndFrame))
                return false;
        }
        else if (! engine.clearLoop())
        {
            return false;
        }

        return snapshot.playing ? engine.play() : engine.stop();
    }

    static void zeroOutputs (float* const* outChannels,
                             int numOutputChannels,
                             int numFrames) noexcept YESDAW_RT_HOT
    {
        if (outChannels == nullptr || numOutputChannels <= 0 || numFrames <= 0)
            return;

        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            float* const dst = outChannels[channel];
            if (dst == nullptr)
                continue;

            for (int frame = 0; frame < numFrames; ++frame)
                dst[frame] = 0.0f;
        }
    }

    Project                         project_;
    std::vector<DecodedAssetAudio>  decodedAssets_;
    DeviceHotSwapFormat             format_;
    OfflineRenderOptions            options_;
    std::unique_ptr<PlaybackEngine> engine_;
    std::atomic<std::uint32_t>      callbackWhileStoppedCount_ { 0 };
    std::atomic<bool>               callbackActive_ { false };
};

} // namespace yesdaw::engine
