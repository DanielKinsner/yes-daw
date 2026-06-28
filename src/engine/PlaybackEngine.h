// YES DAW - H8 playback engine: play a Project through the realtime Runtime.
//
// A production caller for the lock-free Runtime / RuntimeAudioDriver: builds the same CompiledGraph the
// offline renderer uses (buildProjectGraph) and publishes it to the engine, so the device callback can
// pump processBlock and hear exactly what the offline render produces. play-from-0 only for H8 checkpoint
// 1; transport controls (locate / loop / stop) land in later checkpoints under ADR-0022.

#pragma once

#include "engine/OfflineRenderer.h"
#include "engine/RuntimeAudioDriver.h"
#include "rt/RtHot.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace yesdaw::engine {

class PlaybackEngine
{
public:
    struct Result
    {
        OfflineRenderStatus             status = OfflineRenderStatus::Ok;
        ProjectMixerProjectionError     projectError;
        MixerProjectionError            mixerError;
        std::unique_ptr<PlaybackEngine> engine;

        [[nodiscard]] bool ok() const noexcept
        {
            return status == OfflineRenderStatus::Ok && engine != nullptr;
        }
    };

    // CONTROL THREAD: build the Project graph and publish it to a fresh Runtime. On success, `engine` owns
    // a runtime with the graph queued; the first processBlock installs it (ADR-0006 ordered swap).
    [[nodiscard]] static Result create (const Project& project,
                                        std::span<const DecodedAssetAudio> decodedAssets,
                                        OfflineRenderOptions options = {})
    {
        Result out;
        ProjectGraphResult built = buildProjectGraph (project, decodedAssets, options);
        out.projectError = built.projectError;
        out.mixerError = built.mixerError;
        if (! built.ok())
        {
            out.status = built.status;
            return out;
        }

        std::unique_ptr<PlaybackEngine> engine (
            new PlaybackEngine (built.channels, built.frames, built.maxBlockSize));
        if (! engine->driver_.publish (std::move (built.graph)))
        {
            // Only a null graph or a full command queue can fail here; a fresh queue cannot be full.
            out.status = OfflineRenderStatus::MixerProjectionFailed;
            return out;
        }

        out.engine = std::move (engine);
        out.status = OfflineRenderStatus::Ok;
        return out;
    }

    PlaybackEngine (const PlaybackEngine&)            = delete;
    PlaybackEngine& operator= (const PlaybackEngine&) = delete;

    // AUDIO THREAD / device callback: no allocation, locking, logging, or I/O. The queued graph installs
    // on the first call (the command drain runs before the render, ADR-0006).
    void processBlock (float* const* outChannels, int numOutputChannels, int numFrames) noexcept YESDAW_RT_HOT
    {
        driver_.processDeviceBlock (outChannels, numOutputChannels, numFrames);
    }

    // JANITOR / CONTROL THREAD: never call from the audio thread.
    std::size_t reclaim() noexcept { return driver_.reclaim(); }

    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] std::uint64_t frames() const noexcept { return frames_; }   // full timeline incl tail
    [[nodiscard]] int           maxBlockSize() const noexcept { return maxBlockSize_; }
    [[nodiscard]] std::uint64_t processedGen() const noexcept { return driver_.processedGen(); }

private:
    PlaybackEngine (std::uint16_t channels, std::uint64_t frames, int maxBlockSize) noexcept
        : channels_ (channels), frames_ (frames), maxBlockSize_ (maxBlockSize) {}

    RuntimeAudioDriver driver_;
    std::uint16_t      channels_ = 0;
    std::uint64_t      frames_ = 0;
    int                maxBlockSize_ = 128;
};

} // namespace yesdaw::engine
