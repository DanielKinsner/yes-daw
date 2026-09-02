// YES DAW - H8 playback engine: play a Project through the realtime Runtime.
//
// A production caller for the lock-free Runtime / RuntimeAudioDriver: builds the same CompiledGraph the
// offline renderer uses (buildProjectGraph) and publishes it to the engine, so the device callback can
// pump processBlock and hear exactly what the offline render produces. ADR-0022 adds the headless
// transport surface (play / stop / locate / loop) around that same audio-thread call.

#pragma once

#include "engine/OfflineRenderer.h"
#include "engine/ProjectMixerProjection.h"
#include "engine/Recording.h"
#include "engine/RuntimeAudioDriver.h"
#include "engine/nodes/MeterNode.h"
#include "rt/RtHot.h"

#include "choc/containers/choc_SingleReaderSingleWriterFIFO.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

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

        // Harvest the per-track MeterNode taps before the graph moves into the Runtime. The engine
        // publishes exactly this one graph and owns the Runtime that owns it, so these control-side
        // pointers stay valid for the engine's whole life; reads are the single acquire-load the
        // MeterNode contract designs for.
        for (const Track& track : project.tracks)
        {
            const NodeId meterNodeId = projectMixerNodeIdForTrack (track.id, ProjectMixerNodeRole::Meter);
            if (const auto* meter = dynamic_cast<const MeterNode*> (built.graph->nodeForId (meterNodeId)))
                engine->trackMeters_.push_back ({ track.id, meter });
        }

        // E22: harvest the per-bus MeterNode taps on the same contract (an unrouted bus with no
        // FX projects no meter node and simply stays absent — its peak reads 0).
        for (const Bus& bus : project.buses)
        {
            const NodeId meterNodeId = projectMixerNodeIdForEntity (bus.id, ProjectMixerNodeRole::Meter);
            if (const auto* meter = dynamic_cast<const MeterNode*> (built.graph->nodeForId (meterNodeId)))
                engine->busMeters_.push_back ({ bus.id, meter });
        }

        // R12: keep a control-side handle to the ONE graph this engine will ever run. Within a
        // PlaybackEngine's life exactly one graph is published (structural edits build a whole
        // new engine), so — like the meter taps above — this pointer stays valid for the
        // engine's whole life. It powers the live scalar lane: the atomic mute-mask
        // republication runs through it directly; gain/pan/FX-param posts ride the driver's
        // ordered command queue and are applied by the audio thread.
        engine->liveGraph_ = built.graph.get();

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

    // CONTROL THREAD: a loaded Project has a real transport before it has media. The empty Runtime is the
    // canonical silence path, so no hidden Asset/Clip/Node is needed and the audio callback stays identical.
    [[nodiscard]] static std::unique_ptr<PlaybackEngine> createTransportOnly (SampleRate sampleRate,
                                                                              int maxBlockSize)
    {
        if (! sampleRate.isValid() || maxBlockSize <= 0)
            return nullptr;

        return std::unique_ptr<PlaybackEngine> (
            new PlaybackEngine (sampleRate, 2u, 0u, maxBlockSize));
    }

    PlaybackEngine (const PlaybackEngine&)            = delete;
    PlaybackEngine& operator= (const PlaybackEngine&) = delete;

    // CONTROL THREAD: the latest per-track meter peak the audio thread published (0 for unknown
    // Tracks or a transport-only engine). One atomic acquire-load; never blocks the audio thread.
    [[nodiscard]] float trackMeterPeak (EntityId trackId) const noexcept
    {
        for (const auto& [id, meter] : trackMeters_)
            if (id == trackId)
                return meter->peak();

        return 0.0f;
    }

    // V5: the per-channel twin — the rail's stereo L/R meter reads channel 0/1 through the same
    // single acquire-load contract (0 for unknown Tracks, out-of-range channels, or a
    // transport-only engine).
    [[nodiscard]] float trackMeterPeakChannel (EntityId trackId, int channel) const noexcept
    {
        for (const auto& [id, meter] : trackMeters_)
            if (id == trackId)
                return meter->peak (channel);

        return 0.0f;
    }

    // E22: the per-bus twin — 0 for unknown or unrouted buses.
    [[nodiscard]] float busMeterPeak (EntityId busId) const noexcept
    {
        for (const auto& [id, meter] : busMeters_)
            if (id == busId)
                return meter->peak();

        return 0.0f;
    }

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

        drainTransportCommands();

        // G3.2 / ADR-0047: this Block's live notes (audition; later G3.10 input), block-top.
        const std::size_t liveCount = drainLiveEvents();
        const std::span<const Event> liveEvents (liveEventStorage_.data(), liveCount);

        if (! playing_)
        {
            // Stopped: the graph still runs while a live note is held or its release rings, with the clip
            // sources silenced and the playhead untouched; with nothing live, the exact zeros of old.
            if (liveCount > 0 || heldLiveNotes_ > 0 || auditionTailFrames_ > 0)
                processLiveOnlyBlock (outChannels, numOutputChannels, numFrames, liveEvents);
            else
                zeroOutputChannels (outChannels, numOutputChannels, numFrames);
            publishTransportSnapshot();
            return;
        }

        auditionTailFrames_ = std::max<std::int64_t> (0, auditionTailFrames_ - static_cast<std::int64_t> (numFrames));

        if (playbackRate_ > 1)
        {
            processForwardShuttleBlock (outChannels, numOutputChannels, numFrames);
            publishTransportSnapshot();
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

            processTransportSegment (outChannels, numOutputChannels, offset, segment,
                                     offset == 0 ? liveEvents : std::span<const Event> {});
            overlayMetronome (outChannels, numOutputChannels, offset, segment, playheadFrame_);
            playheadFrame_ += static_cast<std::int64_t> (segment);
            offset += segment;

            if (loopEnabled_ && playheadFrame_ >= loopEndFrame_)
                playheadFrame_ = loopStartFrame_;
        }

        publishTransportSnapshot();
    }

    // CONTROL THREAD: transport changes travel through one bounded SPSC command queue. The audio callback
    // drains it at the top of each Block and remains the sole owner of the live transport fields.
    bool play() noexcept { return postTransportCommand (TransportCommand { TransportCommandType::Play }); }
    bool stop() noexcept { return postTransportCommand (TransportCommand { TransportCommandType::Stop }); }

    [[nodiscard]] bool locate (std::int64_t timelineFrame) noexcept
    {
        if (timelineFrame < 0 || timelineFrame > kMaxTransportFrame)
            return false;

        return postTransportCommand (TransportCommand { TransportCommandType::Locate, timelineFrame });
    }

    [[nodiscard]] bool setLoop (std::int64_t startFrame, std::int64_t endFrame) noexcept
    {
        if (startFrame < 0 || endFrame <= startFrame || endFrame > kMaxTransportFrame)
            return false;

        return postTransportCommand (TransportCommand { TransportCommandType::SetLoop, startFrame, endFrame });
    }

    bool clearLoop() noexcept { return postTransportCommand (TransportCommand { TransportCommandType::ClearLoop }); }

    // CONTROL THREAD (G3.2 / ADR-0047): the LIVE note lane. Audition (and later G3.10 input) posts a
    // NoteOn / NoteOff addressed to one Instrument (NotePayload::targetNode != 0); the audio thread hands
    // it to the graph at the top of its next Block, transport playing or stopped. Bounded SPSC: false when
    // the lane is full or the event is not an addressed note - never blocks, never allocates.
    [[nodiscard]] bool postLiveEvent (const Event& event) noexcept
    {
        if (event.type != EventType::NoteOn && event.type != EventType::NoteOff)
            return false;
        if (event.payload.note.targetNode == 0)
            return false;
        return liveEvents_.push (event);
    }

    [[nodiscard]] bool setPlaybackRate (int rate) noexcept
    {
        if (rate != 1 && rate != 2 && rate != 4)
            return false;

        return postTransportCommand (
            TransportCommand { TransportCommandType::SetPlaybackRate, static_cast<std::int64_t> (rate) });
    }

    // CONTROL THREAD: metronome click overlay (usable-DAW P1). The click tables are precomputed at
    // construction for this engine's sample rate; only the beat grid parameters travel through
    // atomics, so the audio thread never allocates or computes trig. The click is a MONITORING
    // overlay summed after the graph — offline Render/export never contains it. Head-tempo grid
    // (constant BPM) is the alpha scope, matching the header tempo control.
    void setMetronome (bool enabled, double bpm, int beatsPerBar, int beatDenominator) noexcept
    {
        const double sampleRateHz = sampleRate_.isValid() ? sampleRate_.hz : 48000.0;
        const double clampedBpm = std::isfinite (bpm) ? std::clamp (bpm, 20.0, 400.0) : 120.0;
        const double beatScale = 4.0 / static_cast<double> (std::clamp (beatDenominator, 1, 64));
        const std::int64_t framesPerBeat =
            std::max<std::int64_t> (
                1,
                static_cast<std::int64_t> (sampleRateHz * 60.0 / clampedBpm * beatScale + 0.5));
        metronomeFramesPerBeat_.store (framesPerBeat, std::memory_order_relaxed);
        metronomeBeatsPerBar_.store (std::clamp (beatsPerBar, 1, 32), std::memory_order_relaxed);
        metronomeEnabled_.store (enabled, std::memory_order_release);
    }

    [[nodiscard]] bool metronomeEnabled() const noexcept
    {
        return metronomeEnabled_.load (std::memory_order_acquire);
    }

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
    // G3.2: how long a stopped transport keeps the graph running after the last live note releases.
    static constexpr double kAuditionTailSeconds = 4.0;
    [[nodiscard]] std::uint64_t processedGen() const noexcept { return driver_.processedGen(); }
    // R12: how many live scalar commands the audio thread has applied to the running graph.
    [[nodiscard]] std::uint64_t liveScalarsApplied() const noexcept { return driver_.scalarsApplied(); }

    // R12 — the live scalar lane (CONTROL THREAD). A transport-only engine has no graph and
    // refuses honestly; the caller falls back to the rebuild path.
    [[nodiscard]] bool hasLiveGraph() const noexcept { return liveGraph_ != nullptr; }

    [[nodiscard]] bool postLiveSetGain (NodeId node, float linearGain) noexcept
    {
        return liveGraph_ != nullptr && driver_.postSetGain (node, linearGain);
    }

    [[nodiscard]] bool postLiveSetPan (NodeId node, float pan) noexcept
    {
        return liveGraph_ != nullptr && driver_.postSetPan (node, pan);
    }

    [[nodiscard]] bool postLiveSetFxParam (NodeId node, ParameterId paramId, double normalizedValue) noexcept
    {
        return liveGraph_ != nullptr && driver_.postSetFxParam (node, paramId, normalizedValue);
    }

    // G0.5 — the live placement lane (CONTROL THREAD): publish a Track's new ClipSchedule to its
    // schedule node through the ordered command queue. The audio thread installs it and retires
    // the previous one to the janitor (reclaim()). Refused on a transport-only engine.
    [[nodiscard]] bool postLiveClipSchedule (NodeId trackScheduleNode,
                                             std::unique_ptr<const ClipSchedule> schedule) noexcept
    {
        return liveGraph_ != nullptr && driver_.postSetClipSchedule (trackScheduleNode, std::move (schedule));
    }
    [[nodiscard]] std::uint64_t liveSchedulesApplied() const noexcept { return driver_.schedulesApplied(); }

    // Republish the whole ADR-0014 mute/solo/solo-safe mask from the Project's strip state onto
    // the RUNNING graph — the same one law the build path applies (applyProjectStripMuteMask),
    // through the graph's atomic mute words, so no command lane and no rebuild is needed.
    [[nodiscard]] bool applyLiveStripMuteMask (const Project& project) noexcept
    {
        if (liveGraph_ == nullptr)
            return false;

        applyProjectStripMuteMask (*liveGraph_, project);
        return true;
    }
    [[nodiscard]] bool          isPlaying() const noexcept
    {
        return publishedPlaying_.load (std::memory_order_acquire);
    }
    [[nodiscard]] bool          loopEnabled() const noexcept
    {
        return publishedLoopEnabled_.load (std::memory_order_acquire);
    }
    [[nodiscard]] std::int64_t  playheadFrame() const noexcept
    {
        return publishedPlayheadFrame_.load (std::memory_order_acquire);
    }
    [[nodiscard]] std::int64_t  loopStartFrame() const noexcept
    {
        return publishedLoopStartFrame_.load (std::memory_order_acquire);
    }
    [[nodiscard]] std::int64_t  loopEndFrame() const noexcept
    {
        return publishedLoopEndFrame_.load (std::memory_order_acquire);
    }
    [[nodiscard]] int playbackRate() const noexcept
    {
        return publishedPlaybackRate_.load (std::memory_order_acquire);
    }
    [[nodiscard]] bool          needsAutosave() const noexcept { return editRevision_ != autosavedRevision_; }

    // CONTROL THREAD ONLY (like reclaim()): needsAutosave / markProjectEdited / markAutosaved are plain
    // non-atomic edit-revision counters — drive them from the control loop, never the audio thread. The
    // autosave write they gate does disk I/O; see persistence/PlaybackAutosave.h.
    void markProjectEdited() noexcept
    {
        if (editRevision_ < std::numeric_limits<std::uint64_t>::max())
            ++editRevision_;
    }

    void markAutosaved() noexcept { autosavedRevision_ = editRevision_; }

private:
    static constexpr int kMaxDeviceOutputChannels = 64;
    static constexpr std::uint32_t kTransportCommandCapacity = 512;
    static constexpr std::uint32_t kLiveEventCapacity = 256;         // G3.2: the live note lane
    static constexpr std::uint32_t kMaxLiveEventsPerBlock = 64;
    static constexpr std::uint32_t kMaxTransportCommandsPerBlock = 64;

    // Transport frames are bounded well below INT64_MAX so playhead arithmetic and the loop-split narrowing
    // in processBlock can never overflow. ~800 years at 48 kHz — far past any real timeline, so this never
    // rejects a musical position; it only rejects nonsense like locate(INT64_MAX).
    static constexpr std::int64_t kMaxTransportFrame = std::int64_t { 1 } << 60;

    enum class TransportCommandType : std::uint8_t
    {
        Play,
        Stop,
        Locate,
        SetLoop,
        ClearLoop,
        SetPlaybackRate
    };

    struct TransportCommand
    {
        TransportCommandType type = TransportCommandType::Play;
        std::int64_t a = 0;
        std::int64_t b = 0;
    };
    static_assert (std::is_trivially_copyable_v<TransportCommand>,
                   "TransportCommand must pass through the SPSC queue losslessly");

    PlaybackEngine (SampleRate sampleRate, std::uint16_t channels, std::uint64_t frames, int maxBlockSize)
        : sampleRate_ (sampleRate),
          channels_ (channels),
          frames_ (frames),
          maxBlockSize_ (maxBlockSize),
          shuttleScratchStorage_ (
              static_cast<std::size_t> (kMaxDeviceOutputChannels)
                  * static_cast<std::size_t> (maxBlockSize),
              0.0f)
    {
        transportCommands_.reset (kTransportCommandCapacity);
        liveEvents_.reset (kLiveEventCapacity);
        buildMetronomeClickTables();
    }

    // CONTROL THREAD (constructor): two short decaying sine bursts — beat and accented downbeat. The
    // audio thread only indexes these; it never computes trig or allocates.
    void buildMetronomeClickTables()
    {
        const double sampleRateHz = sampleRate_.isValid() ? sampleRate_.hz : 48000.0;
        const std::size_t clickFrames =
            std::max<std::size_t> (8, static_cast<std::size_t> (sampleRateHz * kMetronomeClickSeconds));
        metronomeBeatClick_.resize (clickFrames);
        metronomeDownbeatClick_.resize (clickFrames);
        constexpr double kTwoPi = 6.283185307179586;
        for (std::size_t i = 0; i < clickFrames; ++i)
        {
            const double t = static_cast<double> (i) / sampleRateHz;
            const double envelope = std::exp (-t * kMetronomeDecayPerSecond);
            metronomeBeatClick_[i] = static_cast<float> (
                std::sin (kTwoPi * kMetronomeBeatHz * t) * envelope * kMetronomeGain);
            metronomeDownbeatClick_[i] = static_cast<float> (
                std::sin (kTwoPi * kMetronomeDownbeatHz * t) * envelope * kMetronomeGain);
        }
    }

    // AUDIO THREAD: overlay clicks onto the rendered segment. Integer beat-grid math per frame only.
    void overlayMetronome (float* const* outChannels,
                           int numOutputChannels,
                           int blockOffset,
                           int segmentFrames,
                           std::int64_t segmentStartFrame) noexcept YESDAW_RT_HOT
    {
        if (! metronomeEnabled_.load (std::memory_order_acquire)
            || outChannels == nullptr || numOutputChannels <= 0)
            return;

        const std::int64_t framesPerBeat = metronomeFramesPerBeat_.load (std::memory_order_relaxed);
        const std::int64_t beatsPerBar =
            std::max<std::int64_t> (1, metronomeBeatsPerBar_.load (std::memory_order_relaxed));
        if (framesPerBeat <= 0 || metronomeBeatClick_.empty())
            return;

        const std::int64_t clickFrames = static_cast<std::int64_t> (metronomeBeatClick_.size());
        for (int i = 0; i < segmentFrames; ++i)
        {
            const std::int64_t timelineFrame = segmentStartFrame + static_cast<std::int64_t> (i);
            const std::int64_t phase = timelineFrame % framesPerBeat;
            if (phase >= clickFrames)
                continue;

            const std::int64_t beatIndex = timelineFrame / framesPerBeat;
            const float click = (beatIndex % beatsPerBar) == 0
                ? metronomeDownbeatClick_[static_cast<std::size_t> (phase)]
                : metronomeBeatClick_[static_cast<std::size_t> (phase)];

            for (int channel = 0; channel < numOutputChannels; ++channel)
                if (outChannels[channel] != nullptr)
                    outChannels[channel][blockOffset + i] += click;
        }
    }

    [[nodiscard]] bool postTransportCommand (TransportCommand command) noexcept
    {
        return transportCommands_.push (command);
    }

    // AUDIO THREAD (G3.2): pop this Block's live notes into the preallocated storage, block-top; keep the
    // held count and arm the audition tail on the last release so a stopped transport keeps rendering
    // the Instrument's release and then falls back to exact silence.
    [[nodiscard]] std::size_t drainLiveEvents() noexcept YESDAW_RT_HOT
    {
        std::size_t count = 0;
        Event event;
        while (count < liveEventStorage_.size() && liveEvents_.pop (event))
        {
            event.timeInBlock = 0;
            if (event.type == EventType::NoteOn)
            {
                ++heldLiveNotes_;
            }
            else
            {
                heldLiveNotes_ = std::max (0, heldLiveNotes_ - 1);
                if (heldLiveNotes_ == 0)
                    auditionTailFrames_ = static_cast<std::int64_t> (kAuditionTailSeconds * sampleRate_.hz);
            }
            liveEventStorage_[count++] = event;
        }
        return count;
    }

    void drainTransportCommands() noexcept YESDAW_RT_HOT
    {
        TransportCommand command;
        for (std::uint32_t i = 0; i < kMaxTransportCommandsPerBlock; ++i)
        {
            if (! transportCommands_.pop (command))
                break;

            applyTransportCommand (command);
        }
    }

    void applyTransportCommand (TransportCommand command) noexcept YESDAW_RT_HOT
    {
        switch (command.type)
        {
            case TransportCommandType::Play:
                playing_ = true;
                playbackRate_ = 1;
                break;

            case TransportCommandType::Stop:
                playing_ = false;
                playbackRate_ = 1;
                break;

            case TransportCommandType::Locate:
                playheadFrame_ = command.a;
                break;

            case TransportCommandType::SetLoop:
                loopStartFrame_ = command.a;
                loopEndFrame_ = command.b;
                loopEnabled_ = true;
                if (playheadFrame_ >= loopEndFrame_)
                    playheadFrame_ = loopStartFrame_;
                break;

            case TransportCommandType::ClearLoop:
                loopEnabled_ = false;
                break;

            case TransportCommandType::SetPlaybackRate:
                playbackRate_ = static_cast<int> (command.a);
                break;
        }
    }

    void publishTransportSnapshot() noexcept YESDAW_RT_HOT
    {
        publishedPlaying_.store (playing_, std::memory_order_release);
        publishedLoopEnabled_.store (loopEnabled_, std::memory_order_release);
        publishedPlayheadFrame_.store (playheadFrame_, std::memory_order_release);
        publishedLoopStartFrame_.store (loopStartFrame_, std::memory_order_release);
        publishedLoopEndFrame_.store (loopEndFrame_, std::memory_order_release);
        publishedPlaybackRate_.store (playbackRate_, std::memory_order_release);
    }

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

    // AUDIO THREAD (G3.2): a stopped Block that carries live notes - the graph runs with the clip sources
    // silenced (Transport::clipsSilenced) and no timeline frame, so nothing scheduled sounds and no source
    // cursor moves; the playhead is untouched. The tail counts down here.
    void processLiveOnlyBlock (float* const* outChannels,
                               int numOutputChannels,
                               int numFrames,
                               std::span<const Event> liveEvents) noexcept YESDAW_RT_HOT
    {
        Transport transport;
        transport.projectSampleRate = sampleRate_;
        transport.timelineFrame = playheadFrame_;
        transport.hasTimelineFrame = false;
        transport.isPlaying = false;
        transport.clipsSilenced = true;
        driver_.processDeviceBlock (outChannels, numOutputChannels, numFrames, transport, liveEvents);
        auditionTailFrames_ = std::max<std::int64_t> (0, auditionTailFrames_ - static_cast<std::int64_t> (numFrames));
    }

    void processTransportSegment (float* const* outChannels,
                                  int numOutputChannels,
                                  int offset,
                                  int numFrames,
                                  std::span<const Event> liveEvents = {}) noexcept YESDAW_RT_HOT
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
        driver_.processDeviceBlock (segmentChannels.data(), numOutputChannels, numFrames, transport, liveEvents);
    }

    void processForwardShuttleBlock (float* const* outChannels,
                                     int numOutputChannels,
                                     int numFrames) noexcept YESDAW_RT_HOT
    {
        std::array<float*, kMaxDeviceOutputChannels> scratchChannels {};
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            scratchChannels[static_cast<std::size_t> (channel)] = shuttleScratchStorage_.data()
                + static_cast<std::size_t> (channel) * static_cast<std::size_t> (maxBlockSize_);
        }

        const int rate = playbackRate_;
        std::int64_t sourceFramesRemaining = static_cast<std::int64_t> (numFrames)
                                           * static_cast<std::int64_t> (rate);
        int sourcePhase = 0;
        int outputOffset = 0;
        while (sourceFramesRemaining > 0)
        {
            if (loopEnabled_ && playheadFrame_ >= loopEndFrame_)
                playheadFrame_ = loopStartFrame_;

            int segment = static_cast<int> (std::min<std::int64_t> (
                sourceFramesRemaining, static_cast<std::int64_t> (maxBlockSize_)));
            if (loopEnabled_)
            {
                const std::int64_t untilLoopEnd = loopEndFrame_ - playheadFrame_;
                YESDAW_RT_FATAL (untilLoopEnd > 0);
                segment = static_cast<int> (std::min<std::int64_t> (
                    static_cast<std::int64_t> (segment), untilLoopEnd));
            }
            YESDAW_RT_FATAL (segment >= 1);

            processTransportSegment (scratchChannels.data(), numOutputChannels, 0, segment);
            overlayMetronome (scratchChannels.data(), numOutputChannels, 0, segment, playheadFrame_);
            for (int sourceOffset = 0; sourceOffset < segment; ++sourceOffset)
            {
                if (sourcePhase == 0)
                {
                    YESDAW_RT_FATAL (outputOffset < numFrames);
                    for (int channel = 0; channel < numOutputChannels; ++channel)
                    {
                        outChannels[channel][outputOffset] =
                            scratchChannels[static_cast<std::size_t> (channel)][sourceOffset];
                    }
                    ++outputOffset;
                }

                if (++sourcePhase == rate)
                    sourcePhase = 0;
            }

            playheadFrame_ += static_cast<std::int64_t> (segment);
            sourceFramesRemaining -= static_cast<std::int64_t> (segment);
            if (loopEnabled_ && playheadFrame_ >= loopEndFrame_)
                playheadFrame_ = loopStartFrame_;
        }

        YESDAW_RT_FATAL (outputOffset == numFrames);
        YESDAW_RT_FATAL (sourcePhase == 0);
    }

    RuntimeAudioDriver driver_;
    CompiledGraph* liveGraph_ = nullptr;   // R12: the one graph this engine runs (harvested at create())
    std::vector<std::pair<EntityId, const MeterNode*>> trackMeters_;   // harvested at create()
    std::vector<std::pair<EntityId, const MeterNode*>> busMeters_;     // harvested at create() (E22)
    choc::fifo::SingleReaderSingleWriterFIFO<TransportCommand> transportCommands_;
    choc::fifo::SingleReaderSingleWriterFIFO<Event> liveEvents_;                  // G3.2: the live note lane
    std::array<Event, kMaxLiveEventsPerBlock> liveEventStorage_ {};             // audio-thread block storage
    int          heldLiveNotes_ = 0;                                              // audio thread
    std::int64_t auditionTailFrames_ = 0;                                         // audio thread
    static constexpr double kMetronomeClickSeconds = 0.005;
    static constexpr double kMetronomeDecayPerSecond = 600.0;
    static constexpr double kMetronomeBeatHz = 1000.0;
    static constexpr double kMetronomeDownbeatHz = 1500.0;
    static constexpr double kMetronomeGain = 0.5;

    std::atomic<bool>          metronomeEnabled_ { false };
    std::atomic<std::int64_t>  metronomeFramesPerBeat_ { 24000 };
    std::atomic<std::int64_t>  metronomeBeatsPerBar_ { 4 };
    std::vector<float>         metronomeBeatClick_;
    std::vector<float>         metronomeDownbeatClick_;

    SampleRate         sampleRate_ {};
    std::uint16_t      channels_ = 0;
    std::uint64_t      frames_ = 0;
    int                maxBlockSize_ = 128;
    std::vector<float> shuttleScratchStorage_;
    std::int64_t       playheadFrame_ = 0;
    std::int64_t       loopStartFrame_ = 0;
    std::int64_t       loopEndFrame_ = 0;
    std::uint64_t      editRevision_ = 0;
    std::uint64_t      autosavedRevision_ = 0;
    bool               playing_ = true;
    bool               loopEnabled_ = false;
    int                playbackRate_ = 1;
    std::atomic<bool> publishedPlaying_ { true };
    std::atomic<bool> publishedLoopEnabled_ { false };
    std::atomic<std::int64_t> publishedPlayheadFrame_ { 0 };
    std::atomic<std::int64_t> publishedLoopStartFrame_ { 0 };
    std::atomic<std::int64_t> publishedLoopEndFrame_ { 0 };
    std::atomic<int> publishedPlaybackRate_ { 1 };
};

static_assert (std::atomic<std::int64_t>::is_always_lock_free,
               "published transport frames must stay lock-free on the audio thread");
static_assert (std::atomic<int>::is_always_lock_free,
               "published playback rate must stay lock-free on the audio thread");

} // namespace yesdaw::engine
