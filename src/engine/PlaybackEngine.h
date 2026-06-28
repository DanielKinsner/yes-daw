// YES DAW - H8 playback engine: play a Project through the realtime Runtime.
//
// A production caller for the lock-free Runtime / RuntimeAudioDriver: builds the same CompiledGraph the
// offline renderer uses (buildProjectGraph) and publishes it to the engine, so the device callback can
// pump processBlock and hear exactly what the offline render produces. ADR-0022 adds the headless
// transport surface (play / stop / locate / loop) around that same audio-thread call.

#pragma once

#include "engine/OfflineRenderer.h"
#include "engine/Recording.h"
#include "engine/RuntimeAudioDriver.h"
#include "rt/RtHot.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
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
            new PlaybackEngine (built.sampleRate, built.channels, built.frames, built.maxBlockSize));
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
        YESDAW_RT_FATAL (numFrames >= 0);
        YESDAW_RT_FATAL (numFrames <= maxBlockSize_);
        YESDAW_RT_FATAL (numOutputChannels >= 0);
        YESDAW_RT_FATAL (numOutputChannels <= kMaxDeviceOutputChannels);
        if (numOutputChannels > 0)
            YESDAW_RT_FATAL (outChannels != nullptr);

        if (! playing_)
        {
            zeroOutputChannels (outChannels, numOutputChannels, numFrames);
            return;
        }

        int offset = 0;
        while (offset < numFrames)
        {
            if (loopEnabled_ && playheadFrame_ >= loopEndFrame_)
                playheadFrame_ = loopStartFrame_;

            int segment = numFrames - offset;
            if (loopEnabled_)
            {
                const std::int64_t untilLoopEnd = loopEndFrame_ - playheadFrame_;
                YESDAW_RT_FATAL (untilLoopEnd > 0);
                // Clamp in 64-bit space BEFORE narrowing. untilLoopEnd can exceed INT_MAX for a wide loop
                // region; a raw static_cast<int> would truncate to a zero/negative segment and hang or trap
                // the audio thread. segment <= numFrames <= maxBlockSize_, so the result always fits in int.
                segment = static_cast<int> (std::min<std::int64_t> (static_cast<std::int64_t> (segment), untilLoopEnd));
                YESDAW_RT_FATAL (segment >= 1);
            }

            processTransportSegment (outChannels, numOutputChannels, offset, segment);
            playheadFrame_ += static_cast<std::int64_t> (segment);
            offset += segment;

            if (loopEnabled_ && playheadFrame_ >= loopEndFrame_)
                playheadFrame_ = loopStartFrame_;
        }
    }

    // TRANSPORT CONTROLS. These mutate plain (non-atomic) transport state that processBlock reads on the
    // audio thread, so for H8 the caller MUST NOT invoke them concurrently with processBlock — drive them
    // from the same thread, or publish them with external synchronization. Lock-free concurrent transport
    // (an SPSC command seam, ADR-0006 pattern) is a tracked follow-up gated by a TSan concurrency test.
    void play() noexcept { playing_ = true; }
    void stop() noexcept { playing_ = false; }

    [[nodiscard]] bool locate (std::int64_t timelineFrame) noexcept
    {
        if (timelineFrame < 0 || timelineFrame > kMaxTransportFrame)
            return false;

        playheadFrame_ = timelineFrame;
        return true;
    }

    [[nodiscard]] bool setLoop (std::int64_t startFrame, std::int64_t endFrame) noexcept
    {
        if (startFrame < 0 || endFrame <= startFrame || endFrame > kMaxTransportFrame)
            return false;

        loopStartFrame_ = startFrame;
        loopEndFrame_ = endFrame;
        loopEnabled_ = true;
        if (playheadFrame_ >= loopEndFrame_)
            playheadFrame_ = loopStartFrame_;
        return true;
    }

    void clearLoop() noexcept { loopEnabled_ = false; }

    [[nodiscard]] RecordingCaptureResult captureRecordingInputBlock (
        RecordingChunkFifo& fifo,
        const RecordingConfig& config,
        const float* const* inputChannels,
        int numInputChannels,
        int numFrames) noexcept YESDAW_RT_HOT
    {
        return yesdaw::engine::captureRecordingInputBlock (
            fifo, config, playheadFrame_, inputChannels, numInputChannels, numFrames);
    }

    // JANITOR / CONTROL THREAD: never call from the audio thread.
    std::size_t reclaim() noexcept { return driver_.reclaim(); }

    [[nodiscard]] std::uint16_t channels() const noexcept { return channels_; }
    [[nodiscard]] std::uint64_t frames() const noexcept { return frames_; }   // full timeline incl tail
    [[nodiscard]] int           maxBlockSize() const noexcept { return maxBlockSize_; }
    [[nodiscard]] std::uint64_t processedGen() const noexcept { return driver_.processedGen(); }
    [[nodiscard]] bool          isPlaying() const noexcept { return playing_; }
    [[nodiscard]] bool          loopEnabled() const noexcept { return loopEnabled_; }
    [[nodiscard]] std::int64_t  playheadFrame() const noexcept { return playheadFrame_; }
    [[nodiscard]] std::int64_t  loopStartFrame() const noexcept { return loopStartFrame_; }
    [[nodiscard]] std::int64_t  loopEndFrame() const noexcept { return loopEndFrame_; }
    [[nodiscard]] bool          needsAutosave() const noexcept { return editRevision_ != autosavedRevision_; }

    void markProjectEdited() noexcept
    {
        if (editRevision_ < std::numeric_limits<std::uint64_t>::max())
            ++editRevision_;
    }

    void markAutosaved() noexcept { autosavedRevision_ = editRevision_; }

private:
    static constexpr int kMaxDeviceOutputChannels = 64;

    // Transport frames are bounded well below INT64_MAX so playhead arithmetic and the loop-split narrowing
    // in processBlock can never overflow. ~800 years at 48 kHz — far past any real timeline, so this never
    // rejects a musical position; it only rejects nonsense like locate(INT64_MAX).
    static constexpr std::int64_t kMaxTransportFrame = std::int64_t { 1 } << 60;

    PlaybackEngine (SampleRate sampleRate, std::uint16_t channels, std::uint64_t frames, int maxBlockSize) noexcept
        : sampleRate_ (sampleRate), channels_ (channels), frames_ (frames), maxBlockSize_ (maxBlockSize) {}

    static void zeroOutputChannels (float* const* outChannels,
                                    int numOutputChannels,
                                    int numFrames) noexcept YESDAW_RT_HOT
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            float* const dst = outChannels[channel];
            YESDAW_RT_FATAL (dst != nullptr);
            for (int frame = 0; frame < numFrames; ++frame)
                dst[frame] = 0.0f;
        }
    }

    void processTransportSegment (float* const* outChannels,
                                  int numOutputChannels,
                                  int offset,
                                  int numFrames) noexcept YESDAW_RT_HOT
    {
        YESDAW_RT_FATAL (numOutputChannels <= kMaxDeviceOutputChannels);

        std::array<float*, kMaxDeviceOutputChannels> segmentChannels {};
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            YESDAW_RT_FATAL (outChannels[channel] != nullptr);
            segmentChannels[static_cast<std::size_t> (channel)] = outChannels[channel] + offset;
        }

        Transport transport;
        transport.projectSampleRate = sampleRate_;
        transport.timelineFrame = playheadFrame_;
        transport.hasTimelineFrame = true;
        transport.isPlaying = true;
        driver_.processDeviceBlock (segmentChannels.data(), numOutputChannels, numFrames, transport);
    }

    RuntimeAudioDriver driver_;
    SampleRate         sampleRate_ {};
    std::uint16_t      channels_ = 0;
    std::uint64_t      frames_ = 0;
    int                maxBlockSize_ = 128;
    std::int64_t       playheadFrame_ = 0;
    std::int64_t       loopStartFrame_ = 0;
    std::int64_t       loopEndFrame_ = 0;
    std::uint64_t      editRevision_ = 0;
    std::uint64_t      autosavedRevision_ = 0;
    bool               playing_ = true;
    bool               loopEnabled_ = false;
};

} // namespace yesdaw::engine
