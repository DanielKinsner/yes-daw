// YES DAW - H11 headless app model.
//
// Keeps the JUCE shell and smoke tests on the same action IDs while Project loading and transport are
// wired to the real bundle reader and PlaybackEngine.

#pragma once

#include "app/RecordingAssetCommit.h"
#include "engine/OfflineRenderer.h"
#include "engine/PlaybackEngine.h"
#include "engine/ProjectUndo.h"
#include "engine/Recording.h"
#include "io/WavFile.h"
#include "persistence/AutosaveRecovery.h"
#include "persistence/PlaybackAutosave.h"
#include "persistence/ProjectBundle.h"
#include "ui/UiActions.h"
#include "ui/UiThemeLayout.h"
#include "ui/WaveformPeakService.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace yesdaw::ui {

struct UiDecodedAsset
{
    engine::EntityId      assetId;
    engine::SampleRate    sampleRate;
    std::uint64_t         frames = 0;
    std::uint16_t         channels = 0;
    std::vector<float>    interleavedSamples;
};

enum class UiAppLoadStatus : std::uint8_t
{
    Ok = 0,
    BundleOpenFailed,
    ProjectReadFailed,
    PlaybackBuildFailed
};

struct UiAppLoadResult
{
    UiAppLoadStatus              status = UiAppLoadStatus::Ok;
    persistence::BundleResult    bundleResult;
    engine::OfflineRenderStatus  playbackStatus = engine::OfflineRenderStatus::Ok;
    engine::ProjectMixerProjectionError projectError;
    engine::MixerProjectionError mixerError;

    [[nodiscard]] bool ok() const noexcept { return status == UiAppLoadStatus::Ok; }
};

enum class UiAppImportStatus : std::uint8_t
{
    Ok = 0,
    NoBundleOpen,
    InvalidDecodedAudio,
    AssetImportFailed,
    ProjectWriteFailed,
    PlaybackBuildFailed
};

struct UiAppImportResult
{
    UiAppImportStatus            status = UiAppImportStatus::Ok;
    persistence::BundleResult    bundleResult;
    engine::OfflineRenderStatus  playbackStatus = engine::OfflineRenderStatus::Ok;
    engine::ProjectMixerProjectionError projectError;
    engine::MixerProjectionError mixerError;

    [[nodiscard]] bool ok() const noexcept { return status == UiAppImportStatus::Ok; }
};

enum class UiAppRecordStatus : std::uint8_t
{
    Ok = 0,
    PreconditionsNotMet,
    SourceWriteFailed,
    AssetImportFailed,
    ProjectWriteFailed,
    PlaybackBuildFailed
};

struct UiRecordedAudioTake
{
    engine::EntityId assetId;
    engine::EntityId clipId;
    engine::EntityId trackId;
    engine::EntityId takeId;
    engine::Tick timelineStart = 0;
    std::uint64_t frames = 0;
    std::uint16_t channels = 0;
};

struct UiRecordedMidiTake
{
    engine::EntityId midiClipId;
    engine::EntityId trackId;
    engine::Tick timelineStart = 0;
    engine::Tick timelineLength = 0;
    std::size_t noteCount = 0;
};

struct UiAppRecordResult
{
    UiAppRecordStatus status = UiAppRecordStatus::Ok;
    UiAppImportResult importResult;
    UiRecordedAudioTake take;
    UiRecordedMidiTake midiTake;
    UiActionState actionState {};

    [[nodiscard]] bool ok() const noexcept { return status == UiAppRecordStatus::Ok; }
};

struct UiRecordingDeviceSelection
{
    bool selected = false;
    std::uint32_t stableDeviceId = 0;
    std::uint32_t generation = 0;
    engine::SampleRate sampleRate;
    std::uint16_t inputChannels = 0;
    std::uint32_t maxBlockSize = 0;
    bool latencyCalibrated = false;
    std::int64_t inputLatencyFrames = 0;
    std::int64_t outputLatencyFrames = 0;
};

struct UiRecordingTrackInputSelection
{
    bool armed = false;
    engine::EntityId trackId;
    std::size_t trackIndex = 0;
    std::uint16_t inputChannel = 0;
};

struct UiRecordingCompSelection
{
    bool selected = false;
    std::size_t segmentCount = 0;
    engine::EntityId firstTakeId;
    engine::EntityId secondTakeId;
    engine::Tick firstTimelineStart = 0;
    engine::Tick firstTimelineLength = 0;
    engine::Tick gapStart = 0;
    engine::Tick gapLength = 0;
    engine::Tick secondTimelineStart = 0;
    engine::Tick secondTimelineLength = 0;
};

struct UiAutosaveRecoveryPrompt
{
    bool pending = false;
    std::filesystem::path bundlePath;
    std::size_t trackCount = 0;
    std::size_t assetCount = 0;
    std::size_t clipCount = 0;
    std::size_t recordingTakeCount = 0;
    std::size_t midiClipCount = 0;
    std::size_t recordingCompSegmentCount = 0;
};

class UiAppModel
{
public:
    static constexpr engine::Tick kFirstTrackAutomationBreakpointAddTick = 1'920;
    static constexpr double kFirstTrackAutomationBreakpointAddValue = 0.50;
    static constexpr double kFirstTrackSendLevelEditValue = 0.80;

    [[nodiscard]] const UiActionRegistry& registry() const noexcept { return registry_; }
    [[nodiscard]] const UiActionContext& context() const noexcept { return context_; }
    [[nodiscard]] UiActionContext contextSnapshot() const noexcept
    {
        UiActionContext snapshot = context_;
        if (playback_ != nullptr)
        {
            snapshot.isPlaying = playback_->isPlaying();
            snapshot.loopEnabled = playback_->loopEnabled();
            snapshot.playheadFrame = playback_->playheadFrame();
            snapshot.shuttlePlaybackRate = playback_->playbackRate();
        }
        return snapshot;
    }
    void refreshTransportSnapshot() noexcept
    {
        syncContextFromPlayback();
    }

    // CONTROL THREAD: finish a pending one-bar count-in only after the audio-owned transport has
    // reached the exact head-tempo/meter boundary. The real capture FIFO is already punch-gated at
    // that frame; the deterministic test-device path commits its canonical Take here.
    void serviceRecordingCountIn()
    {
        if (! context_.recordCountInActive)
            return;

        syncContextFromPlayback();
        if (context_.playheadFrame < recordCountInEndFrame_)
            return;

        context_.recordCountInActive = false;
        if (deterministicRecordCountInPending_)
        {
            deterministicRecordCountInPending_ = false;
            applyMetronomeToPlayback();
            (void) recordDeterministicTestAudioTake (
                static_cast<engine::Tick> (recordCountInEndFrame_), true);
            return;
        }

        context_.isRecording = captureActive_.load (std::memory_order_acquire);
        applyMetronomeToPlayback();
    }
    [[nodiscard]] const engine::Project& project() const noexcept { return project_; }
    [[nodiscard]] engine::EntityId selectedTimelineClipId() const noexcept { return selectedTimelineClipId_; }
    [[nodiscard]] std::size_t selectedTimelineClipCount() const noexcept { return selectedTimelineClipIds_.size(); }
    [[nodiscard]] bool isTimelineClipSelected (engine::EntityId clipId) const noexcept
    {
        return std::find (selectedTimelineClipIds_.begin(), selectedTimelineClipIds_.end(), clipId)
            != selectedTimelineClipIds_.end();
    }
    [[nodiscard]] engine::EntityId selectedMidiClipId() const noexcept { return selectedMidiClipId_; }
    [[nodiscard]] engine::EntityId selectedMidiNoteId() const noexcept { return selectedMidiNoteId_; }
    [[nodiscard]] const std::filesystem::path& bundlePath() const noexcept { return bundlePath_; }
    [[nodiscard]] bool playbackReady() const noexcept { return playback_ != nullptr; }
    void setPlaybackReplacementCallbacks (std::function<void()> willReplace,
                                          std::function<void()> didReplace)
    {
        playbackReplacementWillBegin_ = std::move (willReplace);
        playbackReplacementDidEnd_ = std::move (didReplace);
    }
    void setPlaybackMaxBlockSize (int maxBlockSize) noexcept
    {
        if (maxBlockSize > 0)
            playbackMaxBlockSize_ = maxBlockSize;
    }
    [[nodiscard]] bool locatePlaybackFrame (std::int64_t timelineFrame) noexcept
    {
        if (playback_ == nullptr || ! playback_->locate (timelineFrame))
            return false;

        drainTransport (*playback_);
        syncContextFromPlayback();
        context_.lastLocateFrame = context_.playheadFrame;
        ++context_.commandDispatchCount;
        return true;
    }

    [[nodiscard]] bool processDeviceAudioBlock (float* const* outputChannels,
                                                int numOutputChannels,
                                                int numFrames) noexcept YESDAW_RT_HOT
    {
        return processDeviceAudioBlock (nullptr, 0, outputChannels, numOutputChannels, numFrames);
    }

    // Input-aware device block (usable-DAW P0-1): while a capture session is live, the callback's
    // input channels run through the proven H5 capture pipeline (bounded FIFO, latency-compensated
    // placement) BEFORE playback renders — same RT contract as the hardware recording checker.
    [[nodiscard]] bool processDeviceAudioBlock (const float* const* inputChannels,
                                                int numInputChannels,
                                                float* const* outputChannels,
                                                int numOutputChannels,
                                                int numFrames) noexcept YESDAW_RT_HOT
    {
        engine::PlaybackEngine* const playback = audioPlayback_.load (std::memory_order_acquire);
        if (playback == nullptr || numFrames < 0 || numFrames > playback->maxBlockSize())
        {
            zeroAudioOutputs (outputChannels, numOutputChannels, numFrames);
            return false;
        }

        if (captureActive_.load (std::memory_order_acquire)
            && inputChannels != nullptr
            && numInputChannels > 0)
        {
            (void) playback->captureRecordingInputBlock (
                captureFifo_, captureConfig_, inputChannels, numInputChannels, numFrames);
        }

        playback->processBlock (outputChannels, numOutputChannels, numFrames);
        return true;
    }

    // CONTROL THREAD: arm-and-roll a real capture session. The config is published before the active
    // flag so the audio thread never reads a torn config.
    [[nodiscard]] bool startRealRecordingCapture (int deviceInputChannels,
                                                  double deviceSampleRateHz,
                                                  std::int64_t inputLatencyFrames,
                                                  std::int64_t outputLatencyFrames)
    {
        if (! context_.projectLoaded || playback_ == nullptr
            || captureActive_.load (std::memory_order_acquire)
            || deviceInputChannels <= 0 || deviceSampleRateHz <= 0.0
            || inputLatencyFrames < 0 || outputLatencyFrames < 0)
            return false;

        captureConfig_ = {};
        captureConfig_.sampleRateHz = deviceSampleRateHz;
        captureConfig_.channels = std::min (deviceInputChannels, engine::kMaxRecordingChannels);
        captureConfig_.latency.inputLatencyFrames = inputLatencyFrames;
        captureConfig_.latency.outputLatencyFrames = outputLatencyFrames;
        std::optional<std::int64_t> countInEnd;
        if (context_.recordCountInEnabled)
        {
            countInEnd = nextRecordCountInEndFrame();
            if (! countInEnd)
                return false;
            captureConfig_.window.punchStartFrame = *countInEnd;
        }
        if (! captureConfig_.isValid())
            return false;

        captureInterleaved_.clear();
        captureTimelineStartFrame_ = -1;
        captureChannels_ = static_cast<std::uint16_t> (captureConfig_.channels);
        captureActive_.store (true, std::memory_order_release);
        if (countInEnd)
        {
            if (! beginRecordingCountIn (*countInEnd, false))
            {
                captureActive_.store (false, std::memory_order_release);
                return false;
            }
        }
        else
        {
            if (! playback_->play())
            {
                captureActive_.store (false, std::memory_order_release);
                return false;
            }
            context_.isRecording = true;
            context_.isPlaying = true;
        }
        ++context_.recordingCommandCount;
        ++context_.commandDispatchCount;
        return true;
    }

    // CONTROL THREAD (shell timer): drain captured chunks out of the RT FIFO into the session buffer.
    void drainRealRecordingCapture()
    {
        engine::RecordingChunk chunk;
        while (captureFifo_.pop (chunk))
        {
            if (chunk.frameCount == 0 || chunk.channels == 0)
                continue;

            if (captureTimelineStartFrame_ < 0)
                captureTimelineStartFrame_ = chunk.timelineStartFrame;

            const std::size_t samples = static_cast<std::size_t> (chunk.frameCount)
                                      * static_cast<std::size_t> (chunk.channels);
            captureInterleaved_.insert (captureInterleaved_.end(),
                                        chunk.samples.begin(),
                                        chunk.samples.begin() + static_cast<std::ptrdiff_t> (samples));
        }
    }

    [[nodiscard]] bool realRecordingCaptureActive() const noexcept
    {
        return captureActive_.load (std::memory_order_acquire);
    }

    // CONTROL THREAD: end the capture session and commit the recorded audio as a real Asset + Take +
    // Clip at the latency-compensated Project frame, through the same shared commit service as the
    // packaged hardware checker. Honest failure when nothing was captured — silent/unrouted input is
    // never masked with a synthetic take.
    [[nodiscard]] UiAppRecordResult stopRealRecordingCaptureAndCommit()
    {
        UiAppRecordResult result;
        result.actionState = registry_.stateFor (UiActionId::TransportRecord, context_);

        if (! captureActive_.load (std::memory_order_acquire))
        {
            result.status = UiAppRecordStatus::PreconditionsNotMet;
            return result;
        }

        captureActive_.store (false, std::memory_order_release);
        if (playback_ != nullptr)
        {
            (void) playback_->stop();
            drainTransport (*playback_);
        }
        drainRealRecordingCapture();
        context_.recordCountInActive = false;
        deterministicRecordCountInPending_ = false;
        context_.isRecording = false;
        context_.isPlaying = false;
        applyMetronomeToPlayback();
        ++context_.recordingCommandCount;
        ++context_.commandDispatchCount;

        const std::uint16_t channels = captureChannels_ > 0 ? captureChannels_ : 1;
        const std::uint64_t frames = captureInterleaved_.size() / channels;
        if (frames == 0 || captureTimelineStartFrame_ < 0)
        {
            result.status = UiAppRecordStatus::PreconditionsNotMet;
            return result;
        }

        app::RecordedAudioTakeRequest request;
        request.sampleRate = engine::SampleRate { captureConfig_.sampleRateHz };
        request.frames = frames;
        request.channels = channels;
        request.interleavedSamples = std::span<const float> (
            captureInterleaved_.data(), static_cast<std::size_t> (frames) * channels);
        request.targetTrackId = recordingTrackInput_.trackId;
        request.timelineStart = static_cast<engine::Tick> (captureTimelineStartFrame_);
        request.deviceStableId = recordingDevice_.stableDeviceId;
        request.inputChannel = recordingTrackInput_.inputChannel;
        request.takeOrdinal = nextRecordingTakeOrdinal (recordingTrackInput_.trackId);
        request.monitoringPolicy = engineMonitoringPolicyForUi (context_.selectedRecordingMonitoringPolicy);

        app::RecordedTakeCommitResult commit = app::commitRecordedAudioTake (
            bundleDb_,
            project_,
            request,
            [this] (std::uint8_t seedByte, const engine::Project& project)
            { return allocateSessionEntityId (seedByte, project); },
            [this] (engine::Project& project) -> engine::Track&
            { return ensureDefaultAudioTrack (project); });

        result.importResult.bundleResult = commit.bundleResult;
        if (! commit.ok())
        {
            result.status = UiAppRecordStatus::AssetImportFailed;
            return result;
        }

        // Adopt the committed project exactly like the deterministic take path (no synthetic MIDI).
        UiDecodedAsset decoded;
        decoded.assetId = commit.importedAsset.id;
        decoded.sampleRate = commit.importedAsset.sampleRate;
        decoded.frames = commit.importedAsset.frames;
        decoded.channels = commit.importedAsset.channels;
        decoded.interleavedSamples.assign (captureInterleaved_.begin(), captureInterleaved_.end());

        std::vector<UiDecodedAsset> nextDecoded = decodedAssets_;
        upsertDecodedAsset (nextDecoded, std::move (decoded));

        std::vector<engine::DecodedAssetAudio> decodedViews = makeDecodedViews (nextDecoded);
        engine::PlaybackEngine::Result built = engine::PlaybackEngine::create (
            commit.project,
            std::span<const engine::DecodedAssetAudio> (decodedViews.data(), decodedViews.size()),
            playbackBuildOptions());

        if (! built.ok())
        {
            result.status = UiAppRecordStatus::PlaybackBuildFailed;
            return result;
        }

        (void) built.engine->stop();
        drainTransport (*built.engine);

        project_ = std::move (commit.project);
        decodedAssets_ = std::move (nextDecoded);
        decodedAssetViews_ = makeDecodedViews (decodedAssets_);
        replacePlayback (std::move (built.engine));
        ++editSerial_;   // a committed recording is an edit since the last explicit Save (B37)
        selectedTimelineClipIds_.assign (1, commit.clipId);
        selectedTimelineClipId_ = commit.clipId;
        context_.timelineClipSelected = true;
        context_.canUndo = false;
        context_.canRedo = false;
        syncProjectEditContext();
        resetContextForFreshPlayback();

        lastRecordedAudioTake_ = {
            commit.importedAsset.id,
            commit.clipId,
            commit.trackId,
            commit.takeId,
            commit.timelineStart,
            commit.importedAsset.frames,
            commit.importedAsset.channels
        };
        result.take = lastRecordedAudioTake_;
        enqueueWaveformBuildsForDecodedAssets();
        result.status = UiAppRecordStatus::Ok;
        return result;
    }
    [[nodiscard]] const UiRecordingDeviceSelection& recordingDeviceSelection() const noexcept { return recordingDevice_; }
    [[nodiscard]] const UiRecordingTrackInputSelection& recordingTrackInputSelection() const noexcept { return recordingTrackInput_; }
    [[nodiscard]] const UiRecordedAudioTake& lastRecordedAudioTake() const noexcept { return lastRecordedAudioTake_; }
    [[nodiscard]] const UiRecordedMidiTake& lastRecordedMidiTake() const noexcept { return lastRecordedMidiTake_; }
    [[nodiscard]] const UiRecordingCompSelection& recordingCompSelection() const noexcept { return recordingCompSelection_; }
    [[nodiscard]] const UiAutosaveRecoveryPrompt& autosaveRecoveryPrompt() const noexcept { return autosaveRecovery_; }
    [[nodiscard]] const WaveformPeakService& waveformService() const noexcept { return waveformService_; }
    [[nodiscard]] const engine::AutomationLaneData* firstTrackFaderAutomationLane() const noexcept
    {
        return firstTrackFaderAutomationLane (project_);
    }

    [[nodiscard]] const engine::AutomationLaneData* firstTrackFirstSendAutomationLane() const noexcept
    {
        return firstTrackFirstSendAutomationLane (project_);
    }

    [[nodiscard]] static engine::Project makeDefaultSessionProject()
    {
        engine::Project project;
        project.id = allocateDefaultProjectId();
        project.sampleRate = engine::SampleRate { 48000.0 };
        project.tempoMap.push_back ({ 0, 120.0, engine::TempoCurve::Jump });
        project.meterMap.push_back ({ 0, 4, 4 });
        project.tracks.push_back (makeDefaultAudioTrack());
        return project;
    }

    [[nodiscard]] persistence::BundleResult createProjectBundle (const std::filesystem::path& bundlePath)
    {
        return createProjectBundle (bundlePath, makeDefaultSessionProject());
    }

    [[nodiscard]] persistence::BundleResult createProjectBundle (
        const std::filesystem::path& bundlePath,
        engine::Project project)
    {
        persistence::ProjectBundleDb opened;
        persistence::BundleResult result = persistence::ProjectBundleDb::openOrCreateBundle (bundlePath, opened);
        if (! result.ok())
            return result;

        result = opened.writeProjectSnapshot (project);
        if (! result.ok())
            return result;

        attachProjectBundle (std::move (opened), bundlePath, std::move (project));
        ++context_.commandDispatchCount;
        return result;
    }

    [[nodiscard]] persistence::BundleResult openProjectBundle (const std::filesystem::path& bundlePath)
    {
        persistence::ProjectBundleDb opened;
        persistence::BundleResult result = persistence::ProjectBundleDb::openExistingBundle (bundlePath, opened);
        if (! result.ok())
            return result;

        engine::Project loadedProject;
        result = opened.readProjectSnapshot (loadedProject);
        if (! result.ok())
            return result;

        attachProjectBundle (std::move (opened), bundlePath, std::move (loadedProject));
        ++context_.commandDispatchCount;
        detectAutosaveRecoveryPrompt();
        return result;
    }

    // Last-project record (usable-DAW P1): a one-line UTF-8 file in the shell-supplied session-state
    // directory remembering the most recently opened/created bundle. The native shell reopens it at
    // launch so a crash-then-relaunch reaches the autosave recovery prompt without manual navigation.
    // The injected-choices harness never sets a directory, so tests stay deterministic by default.
    void setSessionStateDirectory (const std::filesystem::path& directory)
    {
        sessionStateDirectory_ = directory;
    }

    [[nodiscard]] std::filesystem::path readLastProjectRecord() const
    {
        if (sessionStateDirectory_.empty())
            return {};

        std::ifstream input (sessionStateDirectory_ / kLastProjectRecordFileName);
        if (! input.good())
            return {};

        std::string line;
        std::getline (input, line);
        if (line.empty())
            return {};

        const auto* bytes = reinterpret_cast<const char8_t*> (line.data());
        std::filesystem::path recorded { std::u8string (bytes, bytes + line.size()) };
        std::error_code existsError;
        return std::filesystem::exists (recorded, existsError) ? recorded : std::filesystem::path {};
    }

    static constexpr std::size_t kRecentProjectsLimit = 5;

    // Open Recent MRU (B39): up to five bundle paths, most recent first, one UTF-8 line each in
    // recent-projects.txt beside the last-project record (whose format stays untouched).
    [[nodiscard]] std::vector<std::filesystem::path> recentProjectBundles() const
    {
        std::vector<std::filesystem::path> recents;
        if (sessionStateDirectory_.empty())
            return recents;

        std::ifstream input (sessionStateDirectory_ / kRecentProjectsRecordFileName);
        std::string line;
        while (recents.size() < kRecentProjectsLimit && std::getline (input, line))
        {
            if (line.empty())
                continue;

            const auto* bytes = reinterpret_cast<const char8_t*> (line.data());
            recents.emplace_back (std::u8string (bytes, bytes + line.size()));
        }

        return recents;
    }

    [[nodiscard]] persistence::BundleResult saveProjectBundle()
    {
        if (! bundleDb_.isOpen())
            return persistence::BundleResult {
                persistence::BundleStatus::SqliteError,
                SQLITE_MISUSE,
                0,
                "no Project bundle is open"
            };

        persistence::BundleResult result = bundleDb_.writeProjectSnapshot (project_);
        if (result.ok())
        {
            lastSavedEditSerial_ = editSerial_;   // an explicit Save marks the session clean (B37)
            ++context_.saveCount;
            ++context_.commandDispatchCount;
        }

        return result;
    }

    // Edits since the last explicit Save (B37/B38). The bundle on disk is always current — every
    // edit persists synchronously — so this tracks the user's saved-state intent, never data risk.
    [[nodiscard]] bool hasUnsavedChanges() const noexcept
    {
        return context_.projectLoaded && editSerial_ != lastSavedEditSerial_;
    }

    // Save-As (usable-DAW P0): persist the current Project, copy the whole bundle (SQLite + immutable
    // Assets) to the chosen path, and continue working in the copy. The original bundle stays intact
    // on disk. On any failure the model reopens the ORIGINAL bundle and reports the error.
    [[nodiscard]] UiActionDispatchResult saveProjectBundleAs (const std::filesystem::path& newBundlePath)
    {
        const UiActionId id = UiActionId::ProjectSaveAs;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (newBundlePath.empty())
            return { id, { false, "save-as path required" }, false };

        std::error_code equivalence;
        if (bundlePath_ == newBundlePath
            || (std::filesystem::exists (newBundlePath, equivalence)
                && std::filesystem::equivalent (bundlePath_, newBundlePath, equivalence)))
            return { id, { false, "save-as target is the current bundle" }, false };

        if (! saveProjectBundle().ok())
            return { id, { false, "save before copy failed" }, false };

        // Release the SQLite handle so Windows lets the file copy; reopen (old or new) below.
        bundleDb_ = persistence::ProjectBundleDb {};

        std::error_code copyError;
        std::filesystem::remove_all (newBundlePath, copyError);
        copyError.clear();
        std::filesystem::copy (bundlePath_, newBundlePath,
                               std::filesystem::copy_options::recursive, copyError);

        const std::filesystem::path reopenPath = copyError ? bundlePath_ : newBundlePath;
        persistence::ProjectBundleDb reopened;
        if (! persistence::ProjectBundleDb::openExistingBundle (reopenPath, reopened).ok())
            return { id, { false, "bundle reopen failed after save-as" }, false };

        bundleDb_ = std::move (reopened);
        if (copyError)
            return { id, { false, "bundle copy failed" }, false };

        bundlePath_ = newBundlePath;
        writeLastProjectRecord();
        waveformService_.start (bundlePath_);
        enqueueWaveformBuildsForDecodedAssets();
        ++context_.saveCount;
        ++context_.commandDispatchCount;
        return { id, state, true };
    }

    // H17 CP4: the shipped shell's autosave-scheduling policy (ON by default). The GUI shell drives a
    // juce::Timer off this; keeping it here means the shell never reaches into the engine/bundle itself.
    [[nodiscard]] const AutosaveSchedulePolicy& autosaveSchedule() const noexcept { return autosaveSchedule_; }

    // One scheduled autosave attempt, called from the shell's control-tick Timer. No-ops safely when no
    // bundle/engine is live yet; the underlying write itself no-ops unless the project is dirty
    // (PlaybackEngine::needsAutosave). CONTROL-THREAD ONLY (heavy SQLite/asset I/O) — never the audio callback.
    [[nodiscard]] persistence::AutosaveResult writeAutosaveTick()
    {
        if (playback_ == nullptr || ! bundleDb_.isOpen())
            return persistence::autosave_detail::ok();

        return persistence::writeAutosaveFromControlTick (*playback_, bundleDb_, project_);
    }

    // Export options (usable-DAW P1): bit depth and range are user-chosen; the sample rate stays the
    // project rate (honest scope — a real sample-rate converter is its own slice, not a resample hack).
    enum class UiExportBitDepth : std::uint8_t { Float32, Int24, Int16 };

    void setExportBitDepth (UiExportBitDepth depth) noexcept { exportBitDepth_ = depth; }
    [[nodiscard]] UiExportBitDepth exportBitDepth() const noexcept { return exportBitDepth_; }
    void setExportLoopRangeOnly (bool loopOnly) noexcept { exportLoopRangeOnly_ = loopOnly; }
    [[nodiscard]] bool exportLoopRangeOnly() const noexcept { return exportLoopRangeOnly_; }

    [[nodiscard]] UiActionDispatchResult exportAudioFile (const std::filesystem::path& destinationPath)
    {
        const UiActionId id = UiActionId::ProjectExportAudio;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (destinationPath.empty())
            return { id, { false, "audio export path required" }, false };

        context_.audioExportCancelRequested = false;
        context_.audioExportInProgress = true;
        context_.audioExportProgressPercent = 0;

        std::vector<engine::DecodedAssetAudio> decodedViews = makeDecodedViews (decodedAssets_);
        const engine::OfflineRenderResult rendered = engine::renderOfflineProject (
            project_,
            std::span<const engine::DecodedAssetAudio> (decodedViews.data(), decodedViews.size()));
        if (! rendered.ok())
        {
            context_.audioExportInProgress = false;
            return { id, { false, "audio export render failed" }, false };
        }

        // Range selection: loop-only slices the rendered frames to the transport loop region.
        // The ruler range selection doubles as the source and wins when set (parity item 25).
        std::uint64_t exportFrames = rendered.frames;
        std::span<const float> exportSamples (rendered.interleavedSamples.data(),
                                              rendered.interleavedSamples.size());
        if (exportLoopRangeOnly_)
        {
            const bool rangeSet = timelineRangeStartFrame_ >= 0
                               && timelineRangeEndFrame_ > timelineRangeStartFrame_;
            const std::int64_t loopStart = rangeSet ? timelineRangeStartFrame_ : playbackLoopStartFrame();
            const std::int64_t loopEnd = rangeSet ? timelineRangeEndFrame_ : playbackLoopEndFrame();
            if (loopStart < 0 || loopEnd <= loopStart)
            {
                context_.audioExportInProgress = false;
                return { id, { false, "loop range export requires a loop region or ruler range selection" }, false };
            }

            const std::uint64_t start = std::min<std::uint64_t> (
                static_cast<std::uint64_t> (loopStart), rendered.frames);
            const std::uint64_t end = std::min<std::uint64_t> (
                static_cast<std::uint64_t> (loopEnd), rendered.frames);
            if (end <= start)
            {
                context_.audioExportInProgress = false;
                return { id, { false, "loop range is outside the rendered project" }, false };
            }

            exportFrames = end - start;
            exportSamples = exportSamples.subspan (
                static_cast<std::size_t> (start) * rendered.channels,
                static_cast<std::size_t> (exportFrames) * rendered.channels);
        }

        const io::WavResult written =
            exportBitDepth_ == UiExportBitDepth::Float32
                ? io::writeFloat32WavFile (destinationPath, rendered.sampleRate, rendered.channels,
                                           exportFrames, exportSamples)
                : io::writePcmWavFile (destinationPath, rendered.sampleRate, rendered.channels,
                                       exportFrames, exportSamples,
                                       exportBitDepth_ == UiExportBitDepth::Int24 ? 24u : 16u);
        if (! written.ok())
        {
            context_.audioExportInProgress = false;
            return { id, { false, "audio export write failed" }, false };
        }

        ++context_.audioExportCount;
        context_.audioExportProgressPercent = 100;
        context_.audioExportInProgress = false;
        ++context_.commandDispatchCount;
        return { id, state, true };
    }

    [[nodiscard]] UiAppImportResult importAudioFile (const std::filesystem::path& sourcePath,
                                                     UiDecodedAsset decoded)
    {
        UiAppImportResult result = addAudioAssetClipFromSource (
            sourcePath,
            std::move (decoded),
            std::nullopt,
            0xA1u,
            0xC1u);

        if (result.ok())
        {
            ++context_.importCount;
            ++context_.commandDispatchCount;
        }

        return result;
    }

    // Multi-track import (usable-DAW P0): place the imported Clip on a chosen Track instead of the
    // default. An invalid/unknown target falls back to the default-track path inside the shared verb.
    [[nodiscard]] UiAppImportResult importAudioFileToTrack (const std::filesystem::path& sourcePath,
                                                            UiDecodedAsset decoded,
                                                            engine::EntityId targetTrackId)
    {
        UiAppImportResult result = addAudioAssetClipFromSource (
            sourcePath,
            std::move (decoded),
            targetTrackId,
            0xA1u,
            0xC1u);

        if (result.ok())
        {
            ++context_.importCount;
            ++context_.commandDispatchCount;
        }

        return result;
    }

    [[nodiscard]] UiAppRecordResult recordDeterministicTestAudioTake (
        std::optional<engine::Tick> deferredTimelineStart = std::nullopt,
        bool servicingCountIn = false)
    {
        UiAppRecordResult result;
        syncRecordingContext();

        const UiActionId id = UiActionId::TransportRecord;
        result.actionState = registry_.stateFor (id, context_);
        if (! result.actionState.enabled)
        {
            result.status = UiAppRecordStatus::PreconditionsNotMet;
            return result;
        }

        if (! servicingCountIn && context_.recordCountInActive)
        {
            cancelRecordingCountIn();
            ++context_.commandDispatchCount;
            ++context_.recordingCommandCount;
            return result;
        }

        if (! servicingCountIn && context_.isRecording)
        {
            context_.isRecording = false;
            ++context_.commandDispatchCount;
            ++context_.recordingCommandCount;
            return result;
        }

        if (! servicingCountIn && context_.recordCountInEnabled)
        {
            const std::optional<std::int64_t> countInEnd = nextRecordCountInEndFrame();
            if (! countInEnd || ! beginRecordingCountIn (*countInEnd, true))
            {
                result.status = UiAppRecordStatus::PreconditionsNotMet;
                return result;
            }

            ++context_.commandDispatchCount;
            ++context_.recordingCommandCount;
            return result;
        }

        UiDecodedAsset decoded = makeDeterministicRecordedAudio();

        // The canonical Asset/Clip/Take persistence runs through the SHARED control-side service
        // (app::commitRecordedAudioTake) — the same code path the packaged hardware recording
        // checker uses (H17 U4 / KTD7). This model only decorates the commit with its paired
        // synthetic MIDI take and then adopts the committed project into playback + UI state.
        app::RecordedAudioTakeRequest request;
        request.sampleRate = decoded.sampleRate;
        request.frames = decoded.frames;
        request.channels = decoded.channels;
        request.interleavedSamples = std::span<const float> (decoded.interleavedSamples.data(),
                                                             decoded.interleavedSamples.size());
        request.targetTrackId = recordingTrackInput_.trackId;
        request.timelineStart = deferredTimelineStart;
        request.deviceStableId = recordingDevice_.stableDeviceId;
        request.inputChannel = recordingTrackInput_.inputChannel;
        request.takeOrdinal = nextRecordingTakeOrdinal (recordingTrackInput_.trackId);
        request.monitoringPolicy = engineMonitoringPolicyForUi (context_.selectedRecordingMonitoringPolicy);

        UiRecordedMidiTake placedMidiTake;
        app::RecordedTakeCommitResult commit = app::commitRecordedAudioTake (
            bundleDb_,
            project_,
            request,
            [this] (std::uint8_t seedByte, const engine::Project& project)
            { return allocateSessionEntityId (seedByte, project); },
            [this] (engine::Project& project) -> engine::Track&
            { return ensureDefaultAudioTrack (project); },
            [this, &placedMidiTake, &request] (engine::Project& nextProject)
            { placedMidiTake = appendDeterministicMidiTake (nextProject, request.inputChannel); });

        result.importResult.bundleResult = commit.bundleResult;
        switch (commit.status)
        {
            case app::RecordedTakeCommitStatus::Ok:
                break;
            case app::RecordedTakeCommitStatus::SourceWriteFailed:
                result.importResult.status = UiAppImportStatus::InvalidDecodedAudio;
                result.status = UiAppRecordStatus::SourceWriteFailed;
                return result;
            case app::RecordedTakeCommitStatus::AssetImportFailed:
                result.importResult.status = UiAppImportStatus::AssetImportFailed;
                result.status = UiAppRecordStatus::AssetImportFailed;
                return result;
            case app::RecordedTakeCommitStatus::ProjectWriteFailed:
                result.importResult.status = UiAppImportStatus::ProjectWriteFailed;
                result.status = UiAppRecordStatus::ProjectWriteFailed;
                return result;
            case app::RecordedTakeCommitStatus::NoBundleOpen:
                result.importResult.status = UiAppImportStatus::NoBundleOpen;
                result.status = UiAppRecordStatus::PreconditionsNotMet;
                return result;
            case app::RecordedTakeCommitStatus::InvalidDecodedAudio:
            case app::RecordedTakeCommitStatus::InvalidProjectIndirection:
                result.importResult.status = UiAppImportStatus::InvalidDecodedAudio;
                result.status = UiAppRecordStatus::PreconditionsNotMet;
                return result;
        }

        // Adopt the committed project: rebuild playback over the new decoded set, then swap.
        decoded.assetId = commit.importedAsset.id;
        std::vector<UiDecodedAsset> nextDecoded = decodedAssets_;
        upsertDecodedAsset (nextDecoded, std::move (decoded));

        std::vector<engine::DecodedAssetAudio> decodedViews = makeDecodedViews (nextDecoded);
        engine::PlaybackEngine::Result built = engine::PlaybackEngine::create (
            commit.project,
            std::span<const engine::DecodedAssetAudio> (decodedViews.data(), decodedViews.size()),
            playbackBuildOptions());

        result.importResult.playbackStatus = built.status;
        result.importResult.projectError = built.projectError;
        result.importResult.mixerError = built.mixerError;
        if (! built.ok())
        {
            result.importResult.status = UiAppImportStatus::PlaybackBuildFailed;
            result.status = UiAppRecordStatus::PlaybackBuildFailed;
            return result;
        }

        (void) built.engine->stop();
        drainTransport (*built.engine);

        project_ = std::move (commit.project);
        decodedAssets_ = std::move (nextDecoded);
        decodedAssetViews_ = makeDecodedViews (decodedAssets_);
        replacePlayback (std::move (built.engine));
        ++editSerial_;   // a committed recording is an edit since the last explicit Save (B37)
        context_.projectLoaded = true;
        selectedTimelineClipIds_.assign (1, commit.clipId);
        selectedTimelineClipId_ = commit.clipId;
        context_.timelineClipSelected = true;
        if (placedMidiTake.midiClipId.isValid())
            selectedMidiClipId_ = placedMidiTake.midiClipId;
        context_.canUndo = false;
        context_.canRedo = false;
        syncProjectEditContext();
        resetContextForFreshPlayback();

        pendingAudioPlacement_ = {
            commit.importedAsset.id,
            commit.clipId,
            commit.trackId,
            commit.takeId,
            commit.timelineStart,
            commit.importedAsset.frames,
            commit.importedAsset.channels
        };
        pendingMidiPlacement_ = placedMidiTake;
        enqueueWaveformBuildsForDecodedAssets();
        result.importResult.status = UiAppImportStatus::Ok;

        lastRecordedAudioTake_ = pendingAudioPlacement_;
        lastRecordedMidiTake_ = pendingMidiPlacement_;
        result.take = lastRecordedAudioTake_;
        result.midiTake = lastRecordedMidiTake_;
        context_.isRecording = true;
        if (! servicingCountIn)
        {
            ++context_.commandDispatchCount;
            ++context_.recordingCommandCount;
        }
        syncRecordingContext();
        return result;
    }

private:
    // The paired synthetic MIDI take the H13 recording skeleton places next to a recorded audio
    // take. UI-model-only scaffolding: it rides the shared commit's decoration hook and is never
    // part of the canonical recorded-audio service (the packaged checker must not produce it).
    [[nodiscard]] UiRecordedMidiTake appendDeterministicMidiTake (engine::Project& nextProject,
                                                                  std::uint16_t inputChannel)
    {
        const engine::Clip& clip = nextProject.clips.back();
        const engine::EntityId placedTrackId = clip.trackId;

        engine::MidiClip midiClip;
        midiClip.id = allocateSessionEntityId (0xD1u, nextProject);
        midiClip.trackId = placedTrackId;
        midiClip.timelineStart = clip.timelineStart;
        midiClip.timelineLength = clip.timelineLength;
        midiClip.timeBase = engine::TimeBase::SampleLocked;

        engine::Note noteA;
        noteA.id = allocateSessionEntityId (0xD2u, nextProject);
        noteA.startTick = 32;
        noteA.lengthTicks = 48;
        noteA.key = 60;
        noteA.pitchNote = 60.0;
        noteA.normalizedVelocity = 0.75;
        noteA.portIndex = 0;
        noteA.channel = static_cast<std::int16_t> (inputChannel);

        engine::Note noteB;
        noteB.id = allocateSessionEntityId (0xD3u, nextProject);
        noteB.startTick = 128;
        noteB.lengthTicks = 64;
        noteB.key = 67;
        noteB.pitchNote = 67.0;
        noteB.normalizedVelocity = 0.5;
        noteB.portIndex = 0;
        noteB.channel = static_cast<std::int16_t> (inputChannel);

        midiClip.notes.push_back (noteA);
        midiClip.notes.push_back (noteB);
        const UiRecordedMidiTake placed {
            midiClip.id,
            placedTrackId,
            midiClip.timelineStart,
            midiClip.timelineLength,
            midiClip.notes.size()
        };
        nextProject.midiClips.push_back (std::move (midiClip));
        return placed;
    }

    [[nodiscard]] UiAppImportResult addAudioAssetClipFromSource (const std::filesystem::path& sourcePath,
                                                                 UiDecodedAsset decoded,
                                                                 std::optional<engine::EntityId> targetTrackId,
                                                                 std::uint8_t assetSeed,
                                                                 std::uint8_t clipSeed)
    {
        UiAppImportResult result;

        if (! bundleDb_.isOpen())
        {
            result.status = UiAppImportStatus::NoBundleOpen;
            return result;
        }

        if (! decodedAudioIsValid (decoded))
        {
            result.status = UiAppImportStatus::InvalidDecodedAudio;
            return result;
        }

        const engine::EntityId requestedAssetId = allocateSessionEntityId (assetSeed);
        engine::Asset imported;
        const persistence::AssetImportRequest request {
            sourcePath,
            requestedAssetId,
            decoded.frames,
            decoded.sampleRate,
            decoded.channels
        };

        result.bundleResult = bundleDb_.importAssetBytes (request, imported);
        if (! result.bundleResult.ok())
        {
            result.status = UiAppImportStatus::AssetImportFailed;
            return result;
        }

        engine::Project nextProject = project_;
        if (nextProject.findAsset (imported.id) == nullptr)
            nextProject.assets.push_back (imported);

        engine::Track* targetTrack = nullptr;
        if (targetTrackId && targetTrackId->isValid())
        {
            for (engine::Track& track : nextProject.tracks)
                if (track.id == *targetTrackId)
                    targetTrack = &track;
        }

        if (targetTrack == nullptr)
            targetTrack = &ensureDefaultAudioTrack (nextProject);

        const engine::EntityId placedTrackId = targetTrack->id;

        engine::Clip clip;
        clip.id = allocateSessionEntityId (clipSeed, nextProject);
        clip.assetId = imported.id;
        clip.trackId = placedTrackId;
        // Import lands at the PLAYHEAD (usable-DAW P1), the Pro Tools insertion-point model; the
        // playhead starts at zero, so a fresh project's first import still begins the timeline.
        clip.timelineStart = static_cast<engine::Tick> (std::max<std::int64_t> (0, context_.playheadFrame));
        clip.timelineLength = static_cast<engine::Tick> (decoded.frames);
        clip.srcOffset = 0;
        clip.srcLen = decoded.frames;
        clip.gain = 1.0f;
        clip.fadeIn = 0;
        clip.fadeOut = 0;
        clip.timeBase = engine::TimeBase::SampleLocked;
        nextProject.clips.push_back (clip);

        if (! nextProject.hasValidAssetClipIndirection())
        {
            result.status = UiAppImportStatus::InvalidDecodedAudio;
            return result;
        }

        result.bundleResult = bundleDb_.writeProjectSnapshot (nextProject);
        if (! result.bundleResult.ok())
        {
            result.status = UiAppImportStatus::ProjectWriteFailed;
            return result;
        }

        decoded.assetId = imported.id;
        std::vector<UiDecodedAsset> nextDecoded = decodedAssets_;
        upsertDecodedAsset (nextDecoded, std::move (decoded));

        std::vector<engine::DecodedAssetAudio> decodedViews = makeDecodedViews (nextDecoded);
        engine::PlaybackEngine::Result built = engine::PlaybackEngine::create (
            nextProject,
            std::span<const engine::DecodedAssetAudio> (decodedViews.data(), decodedViews.size()),
            playbackBuildOptions());

        result.playbackStatus = built.status;
        result.projectError = built.projectError;
        result.mixerError = built.mixerError;
        if (! built.ok())
        {
            result.status = UiAppImportStatus::PlaybackBuildFailed;
            return result;
        }

        (void) built.engine->stop();
        drainTransport (*built.engine);

        project_ = std::move (nextProject);
        decodedAssets_ = std::move (nextDecoded);
        decodedAssetViews_ = makeDecodedViews (decodedAssets_);
        replacePlayback (std::move (built.engine));
        ++editSerial_;   // an import is an edit since the last explicit Save (B37)
        context_.projectLoaded = true;
        selectedTimelineClipIds_.assign (1, clip.id);
        selectedTimelineClipId_ = clip.id;
        context_.timelineClipSelected = true;
        context_.canUndo = false;
        context_.canRedo = false;
        syncProjectEditContext();
        resetContextForFreshPlayback();

        pendingAudioPlacement_ = {
            imported.id,
            clip.id,
            placedTrackId,
            engine::EntityId {},
            clip.timelineStart,
            imported.frames,
            imported.channels
        };
        pendingMidiPlacement_ = {};
        enqueueWaveformBuildsForDecodedAssets();

        result.status = UiAppImportStatus::Ok;
        return result;
    }

public:
    [[nodiscard]] std::vector<float> renderPlaybackFrames (std::uint64_t frames, int blockSize)
    {
        if (playback_ == nullptr || frames == 0 || blockSize <= 0)
            return {};

        // The engine's processBlock contract caps numFrames at its build-time maxBlockSize;
        // render in engine-sized chunks when the caller asks for more.
        blockSize = std::min (blockSize, playback_->maxBlockSize());
        if (blockSize <= 0)
            return {};

        const int channels = static_cast<int> (playback_->channels());
        if (channels <= 0)
            return {};

        std::vector<float> interleaved (
            static_cast<std::size_t> (frames) * static_cast<std::size_t> (channels),
            0.0f);
        std::vector<float> channelStorage (
            static_cast<std::size_t> (channels) * static_cast<std::size_t> (blockSize),
            0.0f);
        std::vector<float*> channelPtrs (static_cast<std::size_t> (channels), nullptr);

        std::uint64_t offset = 0;
        while (offset < frames)
        {
            const int n = static_cast<int> (std::min<std::uint64_t> (
                frames - offset,
                static_cast<std::uint64_t> (blockSize)));

            for (int channel = 0; channel < channels; ++channel)
                channelPtrs[static_cast<std::size_t> (channel)] =
                    channelStorage.data() + static_cast<std::size_t> (channel) * static_cast<std::size_t> (blockSize);

            playback_->processBlock (channelPtrs.data(), channels, n);

            for (int frame = 0; frame < n; ++frame)
            {
                const std::size_t outFrame = static_cast<std::size_t> (offset) + static_cast<std::size_t> (frame);
                for (int channel = 0; channel < channels; ++channel)
                    interleaved[outFrame * static_cast<std::size_t> (channels) + static_cast<std::size_t> (channel)] =
                        channelPtrs[static_cast<std::size_t> (channel)][frame];
            }

            offset += static_cast<std::uint64_t> (n);
            serviceRecordingCountIn();
        }

        (void) playback_->reclaim();
        syncContextFromPlayback();
        return interleaved;
    }

    [[nodiscard]] bool selectTimelineClip (engine::EntityId clipId) noexcept
    {
        if (! timelineEntityExists (clipId))
        {
            clearTimelineClipSelection();
            return false;
        }

        selectedTimelineClipIds_.assign (1, clipId);
        selectedTimelineClipId_ = clipId;
        context_.timelineClipSelected = true;
        context_.activePanel = UiPanel::Timeline;
        return true;
    }

    [[nodiscard]] bool selectTimelineClipForGesture (engine::EntityId clipId, bool toggle) noexcept
    {
        if (! timelineEntityExists (clipId))
            return false;

        const auto selected = std::find (selectedTimelineClipIds_.begin(), selectedTimelineClipIds_.end(), clipId);
        if (toggle)
        {
            if (selected == selectedTimelineClipIds_.end())
            {
                selectedTimelineClipIds_.push_back (clipId);
                selectedTimelineClipId_ = clipId;
            }
            else
            {
                selectedTimelineClipIds_.erase (selected);
                selectedTimelineClipId_ = selectedTimelineClipIds_.empty()
                    ? engine::EntityId {}
                    : selectedTimelineClipIds_.back();
            }
        }
        else if (selected == selectedTimelineClipIds_.end())
        {
            selectedTimelineClipIds_.assign (1, clipId);
            selectedTimelineClipId_ = clipId;
        }
        else
        {
            selectedTimelineClipId_ = clipId;
        }

        context_.timelineClipSelected = ! selectedTimelineClipIds_.empty();
        context_.activePanel = UiPanel::Timeline;
        return true;
    }

    [[nodiscard]] bool selectTimelineClips (std::span<const engine::EntityId> clipIds) noexcept
    {
        std::vector<engine::EntityId> nextSelection;
        nextSelection.reserve (clipIds.size());
        for (engine::EntityId clipId : clipIds)
        {
            if (! timelineEntityExists (clipId))
                return false;
            if (std::find (nextSelection.begin(), nextSelection.end(), clipId) == nextSelection.end())
                nextSelection.push_back (clipId);
        }

        selectedTimelineClipIds_ = std::move (nextSelection);
        selectedTimelineClipId_ = selectedTimelineClipIds_.empty()
            ? engine::EntityId {}
            : selectedTimelineClipIds_.back();
        context_.timelineClipSelected = ! selectedTimelineClipIds_.empty();
        context_.activePanel = UiPanel::Timeline;
        return true;
    }

    void clearTimelineClipSelection() noexcept
    {
        selectedTimelineClipIds_.clear();
        selectedTimelineClipId_ = {};
        context_.timelineClipSelected = false;
        if (context_.activePanel != UiPanel::PianoRoll)
            context_.activePanel = UiPanel::Timeline;
    }

    [[nodiscard]] UiActionDispatchResult selectAllTimelineClipsOnSelectedTrack()
    {
        const UiActionId id = UiActionId::TimelineClipSelectAllTrack;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const int trackIndex = selectedMixerTrackStripIndex();
        if (trackIndex < 0 || trackIndex >= static_cast<int> (project_.tracks.size()))
            return { id, { false, "no selected track" }, false };

        const engine::EntityId trackId = project_.tracks[static_cast<std::size_t> (trackIndex)].id;
        selectedTimelineClipIds_.clear();
        for (const engine::Clip& clip : project_.clips)
            if (clip.trackId == trackId)
                selectedTimelineClipIds_.push_back (clip.id);
        for (const engine::MidiClip& midiClip : project_.midiClips)
            if (midiClip.trackId == trackId)
                selectedTimelineClipIds_.push_back (midiClip.id);

        selectedTimelineClipId_ = selectedTimelineClipIds_.empty()
            ? engine::EntityId {}
            : selectedTimelineClipIds_.back();
        context_.timelineClipSelected = ! selectedTimelineClipIds_.empty();
        context_.activePanel = UiPanel::Timeline;
        ++context_.commandDispatchCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult selectAllTimelineClipsInProject()
    {
        const UiActionId id = UiActionId::TimelineClipSelectAllProject;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        selectedTimelineClipIds_.clear();
        selectedTimelineClipIds_.reserve (project_.clips.size() + project_.midiClips.size());
        for (const engine::Clip& clip : project_.clips)
            selectedTimelineClipIds_.push_back (clip.id);
        for (const engine::MidiClip& midiClip : project_.midiClips)
            selectedTimelineClipIds_.push_back (midiClip.id);

        selectedTimelineClipId_ = selectedTimelineClipIds_.empty()
            ? engine::EntityId {}
            : selectedTimelineClipIds_.back();
        context_.timelineClipSelected = ! selectedTimelineClipIds_.empty();
        context_.activePanel = UiPanel::Timeline;
        ++context_.commandDispatchCount;
        return { id, state, true };
    }

    // E9: the piano roll follows the timeline — a double-clicked MIDI clip becomes ITS clip.
    [[nodiscard]] bool openPianoRollOnMidiClip (engine::EntityId midiClipId) noexcept
    {
        if (findMidiClip (midiClipId) == nullptr)
            return false;

        selectedMidiClipId_ = midiClipId;
        selectedMidiNoteId_ = {};
        selectedMidiNoteIds_.clear();
        context_.activePanel = UiPanel::PianoRoll;
        syncProjectEditContext();
        return true;
    }

    [[nodiscard]] bool selectFirstMidiClip() noexcept
    {
        if (! context_.projectLoaded || project_.midiClips.empty())
        {
            selectedMidiClipId_ = {};
            selectedMidiNoteId_ = {};
            syncProjectEditContext();
            return false;
        }

        if (findMidiClip (selectedMidiClipId_) == nullptr)
        {
            selectedMidiClipId_ = project_.midiClips.front().id;
            selectedMidiNoteId_ = {};
        }

        context_.activePanel = UiPanel::PianoRoll;
        syncProjectEditContext();
        return true;
    }

    [[nodiscard]] UiActionDispatchResult selectPianoRollNote (engine::EntityId midiClipId,
                                                              engine::EntityId noteId) noexcept
    {
        const UiActionId id = UiActionId::PianoRollNoteSelect;
        if (! midiClipId.isValid() || ! noteId.isValid())
            return { id, { false, "invalid piano roll selection" }, false };

        const engine::MidiClip* const midiClip = findMidiClip (midiClipId);
        if (midiClip == nullptr)
            return { id, { false, "MIDI clip missing" }, false };

        selectedMidiClipId_ = midiClipId;
        selectedMidiNoteId_ = {};
        context_.activePanel = UiPanel::PianoRoll;
        syncProjectEditContext();

        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (findNote (*midiClip, noteId) == nullptr)
            return { id, { false, "MIDI note missing" }, false };

        selectedMidiNoteId_ = noteId;
        selectedMidiNoteIds_.assign (1, noteId);
        syncProjectEditContext();
        ++context_.commandDispatchCount;
        return { id, state, true };
    }

    // Select every Note in the selected MIDI Clip (B34); the first Note becomes the primary so
    // single-note edit paths stay valid.
    [[nodiscard]] bool selectAllPianoRollNotes() noexcept
    {
        const engine::MidiClip* const midiClip = findMidiClip (selectedMidiClipId_);
        if (midiClip == nullptr || midiClip->notes.empty())
            return false;

        selectedMidiNoteIds_.clear();
        for (const engine::Note& note : midiClip->notes)
            selectedMidiNoteIds_.push_back (note.id);
        selectedMidiNoteId_ = midiClip->notes.front().id;
        context_.activePanel = UiPanel::PianoRoll;
        syncProjectEditContext();
        ++context_.commandDispatchCount;
        return true;
    }

    [[nodiscard]] std::span<const engine::EntityId> selectedMidiNoteIds() const noexcept
    {
        return { selectedMidiNoteIds_.data(), selectedMidiNoteIds_.size() };
    }

    // Piano-roll selection tools (E11): Shift+click toggles a note in the multi-selection, the
    // Pointer marquee replaces it, and a Pointer empty click clears it.
    [[nodiscard]] bool togglePianoRollNoteSelection (engine::EntityId midiClipId,
                                                     engine::EntityId noteId) noexcept
    {
        const engine::MidiClip* const midiClip = findMidiClip (midiClipId);
        if (midiClip == nullptr || findNote (*midiClip, noteId) == nullptr)
            return false;

        selectedMidiClipId_ = midiClipId;
        const auto selected = std::find (selectedMidiNoteIds_.begin(), selectedMidiNoteIds_.end(), noteId);
        if (selected == selectedMidiNoteIds_.end())
        {
            selectedMidiNoteIds_.push_back (noteId);
            selectedMidiNoteId_ = noteId;
        }
        else
        {
            selectedMidiNoteIds_.erase (selected);
            selectedMidiNoteId_ = selectedMidiNoteIds_.empty()
                ? engine::EntityId {}
                : selectedMidiNoteIds_.back();
        }
        context_.activePanel = UiPanel::PianoRoll;
        syncProjectEditContext();
        ++context_.commandDispatchCount;
        return true;
    }

    [[nodiscard]] bool selectPianoRollNotes (std::span<const engine::EntityId> noteIds) noexcept
    {
        const engine::MidiClip* const midiClip = findMidiClip (selectedMidiClipId_);
        if (midiClip == nullptr)
            return false;

        std::vector<engine::EntityId> nextSelection;
        nextSelection.reserve (noteIds.size());
        for (engine::EntityId noteId : noteIds)
        {
            if (findNote (*midiClip, noteId) == nullptr)
                return false;
            if (std::find (nextSelection.begin(), nextSelection.end(), noteId) == nextSelection.end())
                nextSelection.push_back (noteId);
        }

        selectedMidiNoteIds_ = std::move (nextSelection);
        selectedMidiNoteId_ = selectedMidiNoteIds_.empty()
            ? engine::EntityId {}
            : selectedMidiNoteIds_.back();
        context_.activePanel = UiPanel::PianoRoll;
        syncProjectEditContext();
        ++context_.commandDispatchCount;
        return true;
    }

    void clearPianoRollNoteSelection() noexcept
    {
        selectedMidiNoteId_ = {};
        selectedMidiNoteIds_.clear();
        syncProjectEditContext();
    }

    // Transpose the whole note selection as one atomic undo group (B34): any out-of-range note
    // refuses the entire group.
    [[nodiscard]] UiActionDispatchResult transposeSelectedPianoRollNotes (std::int32_t semitones)
    {
        const UiActionId id = UiActionId::PianoRollNoteTranspose;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const std::vector<engine::EntityId> targets = selectedMidiNoteIds_.empty()
            ? std::vector<engine::EntityId> { selectedMidiNoteId_ }
            : selectedMidiNoteIds_;

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const bool grouped = nextUndo.beginTransactionGroup();
        for (const engine::EntityId noteId : targets)
        {
            if (! nextUndo.apply (nextProject,
                                  engine::ProjectEditCommand::transposeNote (selectedMidiClipId_,
                                                                             noteId,
                                                                             semitones)).applied())
            {
                if (grouped)
                    (void) nextUndo.endTransactionGroup();
                return { id, { false, "transpose out of range" }, false };
            }
        }
        if (grouped)
            (void) nextUndo.endTransactionGroup();

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "note edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.midiEditCount;
        return { id, state, true };
    }

    // Delete the whole note selection as one atomic undo group (B34).
    [[nodiscard]] UiActionDispatchResult deleteSelectedPianoRollNotes()
    {
        const UiActionId id = UiActionId::PianoRollNoteDelete;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const std::vector<engine::EntityId> targets = selectedMidiNoteIds_.empty()
            ? std::vector<engine::EntityId> { selectedMidiNoteId_ }
            : selectedMidiNoteIds_;

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const bool grouped = nextUndo.beginTransactionGroup();
        for (const engine::EntityId noteId : targets)
        {
            if (! nextUndo.apply (nextProject,
                                  engine::ProjectEditCommand::cutNote (selectedMidiClipId_, noteId)).applied())
            {
                if (grouped)
                    (void) nextUndo.endTransactionGroup();
                return { id, state, false };
            }
        }
        if (grouped)
            (void) nextUndo.endTransactionGroup();

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "note edit did not persist" }, false };

        selectedMidiNoteId_ = {};
        selectedMidiNoteIds_.clear();
        context_.midiNoteSelected = false;
        ++context_.commandDispatchCount;
        ++context_.midiEditCount;
        return { id, state, true };
    }

    // Duplicate one Note as a fresh-id copy at a caller-chosen start tick (B35): Ctrl+drag lands
    // it at the drag target and Ctrl+D one grid step later. The copy becomes the selection; a
    // copy that does not fit the clip window is an honest refusal.
    [[nodiscard]] UiActionDispatchResult duplicatePianoRollNote (engine::EntityId midiClipId,
                                                                 engine::EntityId noteId,
                                                                 engine::Tick newStartTick)
    {
        const UiActionId id = UiActionId::PianoRollNoteDuplicate;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const engine::MidiClip* const midiClip = findMidiClip (midiClipId);
        const engine::Note* const source = midiClip != nullptr ? findNote (*midiClip, noteId) : nullptr;
        if (source == nullptr)
            return { id, { false, "note to duplicate missing" }, false };

        if (newStartTick < 0)
            return { id, { false, "duplicate before clip start" }, false };

        engine::Project nextProject = project_;
        const engine::EntityId copyId = allocateSessionEntityId (0xC7u, nextProject);
        engine::ProjectEditCommand command = engine::ProjectEditCommand::addNote (
            midiClipId, copyId, newStartTick, source->lengthTicks, source->key,
            source->normalizedVelocity);
        command.notePitch = source->pitchNote;
        command.notePort = source->portIndex;
        command.noteChannel = source->channel;

        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject, command).applied())
            return { id, { false, "duplicate does not fit the clip" }, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "note edit did not persist" }, false };

        selectedMidiClipId_ = midiClipId;
        selectedMidiNoteId_ = copyId;
        selectedMidiNoteIds_.assign (1, copyId);
        syncProjectEditContext();
        ++context_.commandDispatchCount;
        ++context_.midiEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult duplicateSelectedPianoRollNote (engine::Tick gridStepTicks)
    {
        const engine::MidiClip* const midiClip = findMidiClip (selectedMidiClipId_);
        const engine::Note* const source =
            midiClip != nullptr ? findNote (*midiClip, selectedMidiNoteId_) : nullptr;
        if (source == nullptr)
            return { UiActionId::PianoRollNoteDuplicate, { false, "no selected note" }, false };

        return duplicatePianoRollNote (selectedMidiClipId_,
                                       selectedMidiNoteId_,
                                       source->startTick + gridStepTicks);
    }

    // Quantize the whole note selection to the current snap grid as one atomic undo group (B36).
    // Notes already on the grid are skipped so mixed selections quantize the rest; a selection
    // entirely on the grid is an honest no-op.
    [[nodiscard]] UiActionDispatchResult quantizeSelectedPianoRollNotesToCurrentGrid()
    {
        const UiActionId id = UiActionId::PianoRollNoteQuantizeSelection;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const engine::MidiClip* const midiClip = findMidiClip (selectedMidiClipId_);
        if (midiClip == nullptr)
            return { id, { false, "MIDI clip missing" }, false };

        const engine::SnapGrid grid { std::max<engine::Tick> (1, context_.snapGridTicks) };
        const std::vector<engine::EntityId> targets = selectedMidiNoteIds_.empty()
            ? std::vector<engine::EntityId> { selectedMidiNoteId_ }
            : selectedMidiNoteIds_;

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const bool grouped = nextUndo.beginTransactionGroup();
        bool anyQuantized = false;
        for (const engine::EntityId noteId : targets)
        {
            const engine::Note* const note = findNote (*midiClip, noteId);
            engine::Tick snapped = 0;
            if (note == nullptr
                || ! engine::snapTick (note->startTick, grid, snapped)
                || snapped == note->startTick)
                continue;   // already on the grid: nothing to quantize for this note

            if (! nextUndo.apply (nextProject,
                                  engine::ProjectEditCommand::quantizeNote (selectedMidiClipId_,
                                                                            noteId,
                                                                            grid)).applied())
            {
                if (grouped)
                    (void) nextUndo.endTransactionGroup();
                return { id, { false, "quantize does not fit the clip" }, false };
            }
            anyQuantized = true;
        }
        if (grouped)
            (void) nextUndo.endTransactionGroup();

        if (! anyQuantized)
            return { id, { false, "selection already on the grid" }, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "note edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.midiEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult moveSelectedPianoRollNoteTo (engine::Tick startTick)
    {
        const UiActionId id = UiActionId::PianoRollNoteMove;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        return editSelectedMidiNote (
            id,
            state,
            engine::ProjectEditCommand::moveNote (selectedMidiClipId_, selectedMidiNoteId_, startTick));
    }

    [[nodiscard]] UiActionDispatchResult setSelectedPianoRollNoteLength (engine::Tick lengthTicks)
    {
        const UiActionId id = UiActionId::PianoRollNoteSetLength;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        return editSelectedMidiNote (
            id,
            state,
            engine::ProjectEditCommand::setNoteLength (selectedMidiClipId_, selectedMidiNoteId_, lengthTicks));
    }

    [[nodiscard]] UiActionDispatchResult transposeSelectedPianoRollNote (std::int32_t semitones)
    {
        const UiActionId id = UiActionId::PianoRollNoteTranspose;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        return editSelectedMidiNote (
            id,
            state,
            engine::ProjectEditCommand::transposeNote (selectedMidiClipId_, selectedMidiNoteId_, semitones));
    }

    [[nodiscard]] UiActionDispatchResult setSelectedPianoRollNoteVelocity (double normalizedVelocity)
    {
        const UiActionId id = UiActionId::PianoRollNoteSetVelocity;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        return editSelectedMidiNote (
            id,
            state,
            engine::ProjectEditCommand::setNoteVelocity (selectedMidiClipId_,
                                                         selectedMidiNoteId_,
                                                         normalizedVelocity));
    }

    [[nodiscard]] UiActionDispatchResult quantizeSelectedPianoRollNoteTo (engine::SnapGrid grid)
    {
        const UiActionId id = UiActionId::PianoRollNoteQuantize;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        return editSelectedMidiNote (
            id,
            state,
            engine::ProjectEditCommand::quantizeNote (selectedMidiClipId_, selectedMidiNoteId_, grid));
    }

    [[nodiscard]] UiActionDispatchResult readPianoRollExpressionLanes()
    {
        const UiActionId id = UiActionId::PianoRollReadExpressionLanes;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        context_.activePanel = UiPanel::PianoRoll;
        ++context_.commandDispatchCount;
        ++context_.midiReadCount;
        return { id, state, true };
    }

    // Latest per-track meter peak published by the live playback graph (B32): one control-thread
    // acquire-load of the MeterNode tap; 0 when no engine or no tap exists for the Track.
    [[nodiscard]] float trackMeterPeak (engine::EntityId trackId) const noexcept
    {
        return playback_ != nullptr ? playback_->trackMeterPeak (trackId) : 0.0f;
    }

    // Which Track strip (surface order) the mixer target currently points at; -1 when the target is
    // unset or a Bus. The shell positions its interactive strip controls from this.
    [[nodiscard]] int selectedMixerTrackStripIndex() const noexcept
    {
        if (! context_.mixerTargetSelected || selectedMixerTarget_.kind != MixerTargetKind::Track)
            return -1;

        return static_cast<int> (selectedMixerTarget_.index);
    }

    // showMixerPanel=false selects the strip target without stealing the active panel — the Track
    // rail uses it so arrangement clicks keep the Timeline in front while mixer controls retarget.
    [[nodiscard]] bool selectMixerTrack (std::size_t index, bool showMixerPanel = true) noexcept
    {
        if (! context_.projectLoaded || index >= project_.tracks.size())
        {
            clearMixerTargetSelection();
            return false;
        }

        selectedMixerTarget_ = { MixerTargetKind::Track, index };
        context_.mixerTargetSelected = true;
        if (showMixerPanel)
            context_.activePanel = UiPanel::Mixer;
        return true;
    }

    [[nodiscard]] UiActionDispatchResult setSelectedMixerFader (float linearGain)
    {
        const UiActionId id = UiActionId::MixerTargetSetFader;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (! engine::mixerGainIsValid (linearGain))
            return { id, { false, "invalid mixer fader" }, false };

        return editSelectedMixerStrip (id, state, [linearGain] (engine::MixerStripState& strip) {
            strip.linearGain = linearGain;
        });
    }

    [[nodiscard]] UiActionDispatchResult setSelectedMixerPan (float pan)
    {
        const UiActionId id = UiActionId::MixerTargetSetPan;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (! engine::mixerPanIsValid (pan))
            return { id, { false, "invalid mixer pan" }, false };

        return editSelectedMixerStrip (id, state, [pan] (engine::MixerStripState& strip) {
            strip.pan = pan;
        });
    }

    [[nodiscard]] UiActionDispatchResult toggleSelectedMixerMute()
    {
        const UiActionId id = UiActionId::MixerTargetToggleMute;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        return editSelectedMixerStrip (id, state, [] (engine::MixerStripState& strip) {
            strip.muted = ! strip.muted;
        });
    }

    [[nodiscard]] UiActionDispatchResult toggleSelectedMixerSolo()
    {
        const UiActionId id = UiActionId::MixerTargetToggleSolo;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        return editSelectedMixerStrip (id, state, [] (engine::MixerStripState& strip) {
            strip.soloed = ! strip.soloed;
        });
    }

    // Selected-track strip toggles (B28): the same persisted strip edit as the mixer controls, but
    // panel-preserving and targeted by Track id — the shell passes the selected rail row so the
    // Timeline stays in front and the mixer never opens.
    [[nodiscard]] UiActionDispatchResult toggleTrackMute (engine::EntityId trackId)
    {
        return editTrackStripPanelPreserving (UiActionId::TrackToggleMute, trackId,
                                              [] (engine::MixerStripState& strip) {
                                                  strip.muted = ! strip.muted;
                                              });
    }

    [[nodiscard]] UiActionDispatchResult toggleTrackSolo (engine::EntityId trackId)
    {
        return editTrackStripPanelPreserving (UiActionId::TrackToggleSolo, trackId,
                                              [] (engine::MixerStripState& strip) {
                                                  strip.soloed = ! strip.soloed;
                                              });
    }

    // Toggle the transient recording arm onto a specific Track (B28). Arming an unarmed session or
    // retargeting from another Track arms this Track's input 0; pressing again on the armed Track
    // disarms. Arm state is honestly transient session state, exactly like the default-track arm.
    [[nodiscard]] UiActionDispatchResult toggleRecordingArmForTrack (std::size_t trackIndex)
    {
        syncRecordingContext();

        const UiActionId id = UiActionId::TrackToggleArm;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (trackIndex >= project_.tracks.size())
            return { id, { false, "selected track missing" }, false };

        if (! recordingDevice_.selected || recordingDevice_.inputChannels == 0u)
            return { id, { false, "no armed recording Track/input" }, false };

        const engine::EntityId trackId = project_.tracks[trackIndex].id;
        if (recordingTrackInput_.armed && recordingTrackInput_.trackId == trackId)
        {
            clearRecordingTrackInput();
        }
        else
        {
            recordingTrackInput_.armed = true;
            recordingTrackInput_.trackId = trackId;
            recordingTrackInput_.trackIndex = trackIndex;
            recordingTrackInput_.inputChannel = 0;
        }

        ++context_.commandDispatchCount;
        ++context_.recordingArmCount;
        syncRecordingContext();
        return { id, state, true };
    }

    // FX insert chain on the SELECTED strip (usable-DAW P0): add/remove/bypass are undoable engine
    // commands; the audible graph rebuilds through the same adopt path as every other edit.
    [[nodiscard]] bool selectedMixerOwnerId (engine::EntityId& out) const noexcept
    {
        if (! context_.mixerTargetSelected)
            return false;

        if (selectedMixerTarget_.kind == MixerTargetKind::Track)
        {
            if (selectedMixerTarget_.index >= project_.tracks.size())
                return false;
            out = project_.tracks[selectedMixerTarget_.index].id;
            return true;
        }

        if (selectedMixerTarget_.index >= project_.buses.size())
            return false;
        out = project_.buses[selectedMixerTarget_.index].id;
        return true;
    }

    [[nodiscard]] UiActionDispatchResult addFxInsertToSelectedStrip (engine::FxKind kind)
    {
        const UiActionId id = UiActionId::MixerFxInsertAdd;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::EntityId ownerId;
        if (! selectedMixerOwnerId (ownerId))
            return { id, { false, "no mixer strip selected" }, false };

        const engine::MixerStripState* const strip = engine::detail::findMixerStrip (project_, ownerId);
        if (strip == nullptr)
            return { id, { false, "selected strip missing" }, false };

        engine::Project nextProject = project_;
        const engine::EntityId insertId = allocateSessionEntityId (0xF1u, nextProject);
        engine::ProjectEditCommand command;
        command.verb = engine::ProjectEditVerb::AddFxInsert;
        command.fxOwnerId = ownerId;
        command.fxInsertId = insertId;
        command.fxKind = kind;
        command.fxEnabled = true;
        command.fxPosition = strip->fxChain.size();

        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject, command).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "FX edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult removeFxInsertFromSelectedStrip (std::size_t slotIndex)
    {
        const UiActionId id = UiActionId::MixerFxInsertRemove;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::EntityId ownerId;
        if (! selectedMixerOwnerId (ownerId))
            return { id, { false, "no mixer strip selected" }, false };

        const engine::MixerStripState* const strip = engine::detail::findMixerStrip (project_, ownerId);
        if (strip == nullptr || slotIndex >= strip->fxChain.size())
            return { id, { false, "no FX slot at index" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        engine::ProjectEditCommand command;
        command.verb = engine::ProjectEditVerb::RemoveFxInsert;
        command.fxOwnerId = ownerId;
        command.fxInsertId = strip->fxChain[slotIndex].id;
        if (! nextUndo.apply (nextProject, command).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "FX edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult toggleFxInsertEnabledOnSelectedStrip (std::size_t slotIndex)
    {
        const UiActionId id = UiActionId::MixerFxInsertToggle;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::EntityId ownerId;
        if (! selectedMixerOwnerId (ownerId))
            return { id, { false, "no mixer strip selected" }, false };

        const engine::MixerStripState* const strip = engine::detail::findMixerStrip (project_, ownerId);
        if (strip == nullptr || slotIndex >= strip->fxChain.size())
            return { id, { false, "no FX slot at index" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        engine::ProjectEditCommand command;
        command.verb = engine::ProjectEditVerb::SetFxInsertEnabled;
        command.fxOwnerId = ownerId;
        command.fxInsertId = strip->fxChain[slotIndex].id;
        command.fxEnabled = ! strip->fxChain[slotIndex].enabled;
        if (! nextUndo.apply (nextProject, command).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "FX edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    // Send routing on the SELECTED Track (ADR-0044): bus creation and per-track send rows are
    // undoable engine commands through the same adopt path as every other edit.
    [[nodiscard]] UiActionDispatchResult addBusToMixer()
    {
        const UiActionId id = UiActionId::MixerBusAdd;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        const engine::EntityId busId = allocateSessionEntityId (0xE1u, nextProject);
        const std::string name = "Bus " + std::to_string (nextProject.buses.size() + 1u);
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::addBus (busId, name)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "bus edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    [[nodiscard]] bool selectedTrackIdForSends (engine::EntityId& out) const noexcept
    {
        if (! context_.mixerTargetSelected
            || selectedMixerTarget_.kind != MixerTargetKind::Track
            || selectedMixerTarget_.index >= project_.tracks.size())
            return false;

        out = project_.tracks[selectedMixerTarget_.index].id;
        return true;
    }

    [[nodiscard]] std::vector<engine::SendRow> selectedTrackSends() const
    {
        engine::EntityId trackId;
        if (! selectedTrackIdForSends (trackId))
            return {};

        const engine::Track* const track = findTrack (trackId);
        return track != nullptr ? track->sends : std::vector<engine::SendRow> {};
    }

    [[nodiscard]] UiActionDispatchResult addSendOnSelectedTrack (std::size_t busIndex)
    {
        const UiActionId id = UiActionId::MixerSendAdd;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::EntityId trackId;
        if (! selectedTrackIdForSends (trackId))
            return { id, { false, "no track strip selected" }, false };

        if (busIndex >= project_.buses.size())
            return { id, { false, "no bus at index" }, false };

        engine::Project nextProject = project_;
        const engine::EntityId sendId = allocateSessionEntityId (0xE2u, nextProject);
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject,
                              engine::ProjectEditCommand::addSend (
                                  trackId, sendId, project_.buses[busIndex].id,
                                  engine::SendTap::PostFader, 1.0f)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "send edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult removeSendOnSelectedTrack (std::size_t sendIndex)
    {
        const UiActionId id = UiActionId::MixerSendRemove;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::EntityId trackId;
        if (! selectedTrackIdForSends (trackId))
            return { id, { false, "no track strip selected" }, false };

        const engine::Track* const track = findTrack (trackId);
        if (track == nullptr || sendIndex >= track->sends.size())
            return { id, { false, "no send at index" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject,
                              engine::ProjectEditCommand::removeSend (
                                  trackId, track->sends[sendIndex].id)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "send edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult setSendLevelOnSelectedTrack (std::size_t sendIndex, float linearGain)
    {
        const UiActionId id = UiActionId::MixerSendSetLevel;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::EntityId trackId;
        if (! selectedTrackIdForSends (trackId))
            return { id, { false, "no track strip selected" }, false };

        const engine::Track* const track = findTrack (trackId);
        if (track == nullptr || sendIndex >= track->sends.size())
            return { id, { false, "no send at index" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject,
                              engine::ProjectEditCommand::setSendLevel (
                                  trackId, track->sends[sendIndex].id, linearGain)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "send edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    // FX parameter editing (usable-DAW P1): one undoable SetFxInsertParam per committed gesture.
    [[nodiscard]] UiActionDispatchResult setFxInsertParamOnSelectedStrip (std::size_t slotIndex,
                                                                          std::uint32_t paramId,
                                                                          double normalizedValue)
    {
        const UiActionId id = UiActionId::MixerFxInsertParamSet;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::EntityId ownerId;
        if (! selectedMixerOwnerId (ownerId))
            return { id, { false, "no mixer strip selected" }, false };

        const engine::MixerStripState* const strip = engine::detail::findMixerStrip (project_, ownerId);
        if (strip == nullptr || slotIndex >= strip->fxChain.size())
            return { id, { false, "no FX slot at index" }, false };

        if (! engine::fxKindAcceptsParameterId (strip->fxChain[slotIndex].kind, paramId))
            return { id, { false, "FX kind does not accept parameter" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditCommand command = engine::ProjectEditCommand::setFxInsertParam (
            ownerId, strip->fxChain[slotIndex].id, paramId, normalizedValue);
        if (! nextUndo.apply (nextProject, command).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "FX edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    // Current normalized value of an FX param on the selected strip (spec default when unset).
    [[nodiscard]] double fxInsertParamValueOnSelectedStrip (std::size_t slotIndex, std::uint32_t paramId) const
    {
        engine::EntityId ownerId;
        if (! selectedMixerOwnerId (ownerId))
            return 0.0;

        const engine::MixerStripState* const strip = engine::detail::findMixerStrip (project_, ownerId);
        if (strip == nullptr || slotIndex >= strip->fxChain.size())
            return 0.0;

        const engine::FxInsert& insert = strip->fxChain[slotIndex];
        for (const auto& [id_, value] : insert.normalizedParams)
            if (id_ == paramId)
                return value;

        return engine::normalizedDefault (engine::fxParamSpecForKind (insert.kind, paramId));
    }

    // The selected strip's FX chain for UI display (empty when no selection).
    [[nodiscard]] std::vector<engine::FxInsert> selectedStripFxChain() const
    {
        engine::EntityId ownerId;
        if (! selectedMixerOwnerId (ownerId))
            return {};

        const engine::MixerStripState* const strip = engine::detail::findMixerStrip (project_, ownerId);
        return strip != nullptr ? strip->fxChain : std::vector<engine::FxInsert> {};
    }

    // User-defined loop region (usable-DAW P1): shift-drag on the ruler sets the transport loop to an
    // arbitrary frame range; the loop toggle then clears/re-enables it.
    [[nodiscard]] UiActionDispatchResult setPlaybackLoopRegion (std::int64_t startFrame, std::int64_t endFrame)
    {
        const UiActionId id = UiActionId::TransportToggleLoop;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (playback_ == nullptr || startFrame < 0 || endFrame <= startFrame)
            return { id, { false, "loop region must be a positive range" }, false };

        if (! playback_->setLoop (startFrame, endFrame))
            return { id, { false, "loop region rejected" }, false };

        drainTransport (*playback_);
        refreshTransportSnapshot();
        context_.loopEnabled = true;
        ++context_.commandDispatchCount;
        return { id, state, true };
    }

    [[nodiscard]] std::int64_t playbackLoopStartFrame() const noexcept
    {
        return playback_ != nullptr ? playback_->loopStartFrame() : 0;
    }

    [[nodiscard]] std::int64_t playbackLoopEndFrame() const noexcept
    {
        return playback_ != nullptr ? playback_->loopEndFrame() : 0;
    }

    // Ruler range selection (parity item 25): plain ruler drag paints a transient time range.
    // It is honestly view/transport state — never persisted — so it lives beside the loop region,
    // converts to it on Shift+L, and stands in for it as the export "Loop Region" source when set.
    [[nodiscard]] bool setTimelineRangeSelection (std::int64_t startFrame, std::int64_t endFrame)
    {
        if (! context_.projectLoaded || startFrame < 0 || endFrame <= startFrame)
            return false;

        timelineRangeStartFrame_ = startFrame;
        timelineRangeEndFrame_ = endFrame;
        context_.timelineRangeSelected = true;
        return true;
    }

    void clearTimelineRangeSelection() noexcept
    {
        timelineRangeStartFrame_ = -1;
        timelineRangeEndFrame_ = -1;
        context_.timelineRangeSelected = false;
    }

    [[nodiscard]] std::int64_t timelineRangeStartFrame() const noexcept { return timelineRangeStartFrame_; }
    [[nodiscard]] std::int64_t timelineRangeEndFrame() const noexcept { return timelineRangeEndFrame_; }

    [[nodiscard]] UiActionDispatchResult setProjectTempoBpm (double bpm)
    {
        const UiActionId id = UiActionId::TransportSetTempo;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::setProjectTempo (bpm)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "tempo edit did not persist" }, false };

        refreshSnapGrid();
        applyMetronomeToPlayback();
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult setProjectMeterSignature (std::uint16_t numerator,
                                                                   std::uint16_t denominator)
    {
        const UiActionId id = UiActionId::TransportSetMeter;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::setProjectMeter (numerator, denominator)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "meter edit did not persist" }, false };

        refreshSnapGrid();
        applyMetronomeToPlayback();
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult toggleFirstTrackFxSlotEnabled()
    {
        const UiActionId id = UiActionId::MixerToggleFirstFxSlotEnabled;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (project_.tracks.empty() || project_.tracks.front().strip.fxChain.empty())
            return { id, { false, "no first Track FX slot" }, false };

        const engine::EntityId ownerId = project_.tracks.front().id;
        const engine::FxInsert& firstInsert = project_.tracks.front().strip.fxChain.front();

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::setFxInsertEnabled (ownerId, firstInsert.id, ! firstInsert.enabled));

        if (! applied.applied())
            return { id, state, false };

        if (decodedAssets_.empty() && project_.clips.empty() && nextProject.clips.empty())
        {
            if (! adoptEditedProjectWithoutPlaybackRebuild (std::move (nextProject), std::move (nextUndo)))
                return { id, { false, "FX slot edit did not persist" }, false };
        }
        else if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
        {
            return { id, { false, "FX slot edit did not persist" }, false };
        }

        context_.activePanel = UiPanel::Mixer;
        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult setFirstTrackFirstSendLevel()
    {
        const UiActionId id = UiActionId::MixerSetFirstSendLevel;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const engine::AutomationLaneData* const lane = firstTrackFirstSendAutomationLane();
        if (lane == nullptr)
            return { id, { false, "first Track send level lane missing" }, false };

        if (lane->points.empty())
            return { id, { false, "first Track send level lane has no breakpoints" }, false };

        const engine::Tick tick = lane->points.back().tick;
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::setAutomationBreakpointValue (
                lane->id, tick, kFirstTrackSendLevelEditValue));

        if (! applied.applied())
            return { id, state, false };

        if (canAdoptEditWithoutPlaybackRebuild (nextProject))
        {
            if (! adoptEditedProjectWithoutPlaybackRebuild (std::move (nextProject), std::move (nextUndo)))
                return { id, { false, "send level edit did not persist" }, false };
        }
        else if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
        {
            return { id, { false, "send level edit did not persist" }, false };
        }

        context_.activePanel = UiPanel::Mixer;
        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult moveSelectedTimelineClipTo (engine::Tick timelineStart)
    {
        const UiActionId id = UiActionId::TimelineClipMove;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const UiTimelineEntityView anchor = timelineEntityView (selectedTimelineClipId_);
        if (! anchor.valid)
            return { id, state, false };

        engine::Tick earliestStart = anchor.timelineStart;
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            if (view.valid)
                earliestStart = std::min (earliestStart, view.timelineStart);
        }

        const engine::Tick requestedDelta = timelineStart - anchor.timelineStart;
        const engine::Tick delta = std::max (requestedDelta, -earliestStart);
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            const engine::ProjectEditCommand command = view.isMidi
                ? engine::ProjectEditCommand::moveMidiClip (clipId, view.timelineStart + delta)
                : engine::ProjectEditCommand::moveClip (clipId, view.timelineStart + delta);
            if (! view.valid || ! nextUndo.apply (nextProject, command).applied())
                return { id, state, false };
        }
        if (! nextUndo.endTransactionGroup())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult deleteSelectedTimelineClip()
    {
        const UiActionId id = UiActionId::TimelineClipDelete;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            const engine::ProjectEditCommand command = view.isMidi
                ? engine::ProjectEditCommand::removeMidiClip (clipId)
                : engine::ProjectEditCommand::deleteClip (clipId);
            if (! view.valid || ! nextUndo.apply (nextProject, command).applied())
                return { id, state, false };
        }
        if (! nextUndo.endTransactionGroup())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        clearTimelineClipSelection();
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult moveSelectedTimelineClipToTrack (engine::EntityId targetTrackId,
                                                                          engine::Tick timelineStart)
    {
        const UiActionId id = UiActionId::TimelineClipMove;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const UiTimelineEntityView anchor = timelineEntityView (selectedTimelineClipId_);
        const auto targetTrack = std::find_if (project_.tracks.begin(), project_.tracks.end(), [targetTrackId] (const auto& track) {
            return track.id == targetTrackId;
        });
        if (! anchor.valid || targetTrack == project_.tracks.end())
            return { id, state, false };

        const auto trackIndexFor = [this] (engine::EntityId trackId) {
            const auto track = std::find_if (project_.tracks.begin(), project_.tracks.end(), [trackId] (const auto& candidate) {
                return candidate.id == trackId;
            });
            return track == project_.tracks.end()
                ? -1
                : static_cast<int> (std::distance (project_.tracks.begin(), track));
        };
        const int anchorLane = trackIndexFor (anchor.trackId);
        const int requestedLaneDelta = static_cast<int> (std::distance (project_.tracks.begin(), targetTrack)) - anchorLane;
        int minimumLane = anchorLane;
        int maximumLane = anchorLane;
        engine::Tick earliestStart = anchor.timelineStart;
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            if (view.valid)
            {
                const int lane = trackIndexFor (view.trackId);
                minimumLane = std::min (minimumLane, lane);
                maximumLane = std::max (maximumLane, lane);
                earliestStart = std::min (earliestStart, view.timelineStart);
            }
        }

        const int laneDelta = std::clamp (requestedLaneDelta,
                                          -minimumLane,
                                          static_cast<int> (project_.tracks.size()) - 1 - maximumLane);
        const engine::Tick requestedTimeDelta = timelineStart - anchor.timelineStart;
        const engine::Tick timeDelta = std::max (requestedTimeDelta, -earliestStart);
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            const int sourceLane = view.valid ? trackIndexFor (view.trackId) : -1;
            if (! view.valid || sourceLane < 0)
                return { id, state, false };
            const engine::EntityId nextTrackId = project_.tracks[static_cast<std::size_t> (sourceLane + laneDelta)].id;
            const engine::ProjectEditCommand command = view.isMidi
                ? engine::ProjectEditCommand::moveMidiClipToTrack (
                      clipId, nextTrackId, view.timelineStart + timeDelta)
                : engine::ProjectEditCommand::moveClipToTrack (
                      clipId, nextTrackId, view.timelineStart + timeDelta);
            if (! nextUndo.apply (nextProject, command).applied())
                return { id, state, false };
        }
        if (! nextUndo.endTransactionGroup())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    // E8: append a fresh-id copy of a MIDI clip (with all its notes) inside the caller's
    // transaction group — the same emission the track duplicate uses.
    [[nodiscard]] bool appendMidiClipCopy (engine::Project& nextProject,
                                           engine::ProjectUndoStack& nextUndo,
                                           const engine::MidiClip& source,
                                           engine::EntityId targetTrackId,
                                           engine::Tick timelineStart,
                                           engine::EntityId& copiedId)
    {
        const engine::EntityId newMidiClipId = allocateSessionEntityId (0xC5u, nextProject);
        if (! nextUndo.apply (nextProject,
                              engine::ProjectEditCommand::addMidiClip (newMidiClipId,
                                                                       targetTrackId,
                                                                       timelineStart,
                                                                       source.timelineLength,
                                                                       source.timeBase)).applied())
            return false;

        for (const engine::Note& note : source.notes)
        {
            engine::ProjectEditCommand noteCommand = engine::ProjectEditCommand::addNote (
                newMidiClipId,
                allocateSessionEntityId (0xC6u, nextProject),
                note.startTick,
                note.lengthTicks,
                note.key,
                note.normalizedVelocity);
            noteCommand.notePitch = note.pitchNote;
            noteCommand.notePort = note.portIndex;
            noteCommand.noteChannel = note.channel;
            if (! nextUndo.apply (nextProject, noteCommand).applied())
                return false;
        }

        copiedId = newMidiClipId;
        return true;
    }

    // Center Alt+drag copy (E2): copies the WHOLE selection by the gesture anchor's delta — the
    // same lane-clamp and time-clamp laws as moveSelectedTimelineClipToTrack — as one undo
    // transaction of fresh-id AddClips, then selects the copies.
    [[nodiscard]] UiActionDispatchResult copySelectedTimelineClipsTo (engine::EntityId targetTrackId,
                                                                      engine::Tick timelineStart)
    {
        const UiActionId id = UiActionId::TimelineClipDuplicate;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const UiTimelineEntityView anchor = timelineEntityView (selectedTimelineClipId_);
        const auto targetTrack = std::find_if (project_.tracks.begin(), project_.tracks.end(), [targetTrackId] (const auto& track) {
            return track.id == targetTrackId;
        });
        if (! anchor.valid || targetTrack == project_.tracks.end())
            return { id, state, false };

        const auto trackIndexFor = [this] (engine::EntityId trackId) {
            const auto track = std::find_if (project_.tracks.begin(), project_.tracks.end(), [trackId] (const auto& candidate) {
                return candidate.id == trackId;
            });
            return track == project_.tracks.end()
                ? -1
                : static_cast<int> (std::distance (project_.tracks.begin(), track));
        };
        const int anchorLane = trackIndexFor (anchor.trackId);
        const int requestedLaneDelta = static_cast<int> (std::distance (project_.tracks.begin(), targetTrack)) - anchorLane;
        int minimumLane = anchorLane;
        int maximumLane = anchorLane;
        engine::Tick earliestStart = anchor.timelineStart;
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            if (view.valid)
            {
                const int lane = trackIndexFor (view.trackId);
                minimumLane = std::min (minimumLane, lane);
                maximumLane = std::max (maximumLane, lane);
                earliestStart = std::min (earliestStart, view.timelineStart);
            }
        }

        const int laneDelta = std::clamp (requestedLaneDelta,
                                          -minimumLane,
                                          static_cast<int> (project_.tracks.size()) - 1 - maximumLane);
        const engine::Tick requestedTimeDelta = timelineStart - anchor.timelineStart;
        const engine::Tick timeDelta = std::max (requestedTimeDelta, -earliestStart);

        constexpr engine::Tick maxTick = std::numeric_limits<engine::Tick>::max();
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };
        std::vector<engine::EntityId> copiedClipIds;
        copiedClipIds.reserve (selectedTimelineClipIds_.size());
        engine::EntityId copiedAnchorId;
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            const int sourceLane = view.valid ? trackIndexFor (view.trackId) : -1;
            if (! view.valid || sourceLane < 0)
                return { id, state, false };
            if (timeDelta > 0 && view.timelineStart > maxTick - timeDelta)
                return { id, { false, "copy-drag would overflow timeline" }, false };
            const engine::Tick copyStart = view.timelineStart + timeDelta;
            if (view.timelineLength > maxTick - copyStart)
                return { id, { false, "copy-drag would overflow timeline" }, false };

            const engine::EntityId nextTrackId =
                project_.tracks[static_cast<std::size_t> (sourceLane + laneDelta)].id;
            engine::EntityId copiedId;
            if (view.isMidi)
            {
                const engine::MidiClip* const source = findMidiClip (clipId);
                if (source == nullptr
                    || ! appendMidiClipCopy (nextProject, nextUndo, *source, nextTrackId, copyStart, copiedId))
                    return { id, state, false };
            }
            else
            {
                engine::Clip copy = *findClip (clipId);
                copy.id = allocateSessionEntityId (0xC3u, nextProject);
                copy.trackId = nextTrackId;
                copy.timelineStart = copyStart;
                if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::addClip (copy)).applied())
                    return { id, state, false };
                copiedId = copy.id;
            }
            copiedClipIds.push_back (copiedId);
            if (clipId == selectedTimelineClipId_)
                copiedAnchorId = copiedId;
        }
        if (! nextUndo.endTransactionGroup())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline copy-drag did not persist" }, false };

        selectedTimelineClipIds_ = std::move (copiedClipIds);
        selectedTimelineClipId_ = copiedAnchorId.isValid() ? copiedAnchorId : selectedTimelineClipIds_.back();
        context_.timelineClipSelected = true;
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult addAudioTrack()
    {
        const UiActionId id = UiActionId::TrackAdd;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        const engine::EntityId trackId = allocateSessionEntityId (0xB1u, nextProject);
        const std::string name = "Audio " + std::to_string (nextProject.tracks.size() + 1u);

        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied =
            nextUndo.apply (nextProject, engine::ProjectEditCommand::addTrack (trackId, name));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "track edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.trackEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult renameProjectTrack (engine::EntityId trackId, const std::string& newName)
    {
        const UiActionId id = UiActionId::TrackRename;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (newName.empty() || newName.size() > engine::ProjectEditCommand::kMaxTrackNameLength)
            return { id, { false, "track name must be 1-127 characters" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied =
            nextUndo.apply (nextProject, engine::ProjectEditCommand::renameTrack (trackId, newName));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "track edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.trackEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult renameSelectedTimelineClip (std::string_view newName)
    {
        const UiActionId id = UiActionId::EditRenameSelection;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled || ! context_.timelineClipSelected)
            return { id, state, false };

        if (newName.empty() || newName.size() > engine::ProjectEditCommand::kMaxClipNameLength)
            return { id, { false, "clip name must be 1-127 characters" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::renameClip (selectedTimelineClipId_, newName));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "clip rename did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult reorderProjectTrack (engine::EntityId trackId, std::size_t newIndex)
    {
        const UiActionId id = UiActionId::TrackReorder;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied =
            nextUndo.apply (nextProject, engine::ProjectEditCommand::reorderTrack (trackId, newIndex));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "track edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.trackEditCount;
        return { id, state, true };
    }

    // Duplicate a Track with its Clips, MIDI Clips, scalar strip state, FX chain, and sends as one
    // transaction group of existing undoable verbs; every duplicated entity gets a fresh session
    // EntityId. Recording Takes and automation lanes stay on the source Track (no duplicate verbs
    // exist for those); the copy lands directly below the source as "<name> copy".
    [[nodiscard]] UiActionDispatchResult duplicateProjectTrack (engine::EntityId trackId)
    {
        const UiActionId id = UiActionId::TrackDuplicate;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        std::size_t sourceIndex = project_.tracks.size();
        for (std::size_t i = 0; i < project_.tracks.size(); ++i)
        {
            if (project_.tracks[i].id == trackId)
            {
                sourceIndex = i;
                break;
            }
        }
        if (sourceIndex >= project_.tracks.size())
            return { id, { false, "track to duplicate missing" }, false };

        const engine::Track source = project_.tracks[sourceIndex];

        constexpr std::string_view copySuffix = " copy";
        std::string copyName = source.strip.name;
        const std::size_t maxBaseLength = engine::ProjectEditCommand::kMaxTrackNameLength - copySuffix.size();
        if (copyName.size() > maxBaseLength)
            copyName.resize (maxBaseLength);
        copyName += copySuffix;

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const bool grouped = nextUndo.beginTransactionGroup();

        const engine::EntityId newTrackId = allocateSessionEntityId (0xB2u, nextProject);
        if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::addTrack (newTrackId, copyName)).applied())
            return { id, { false, "duplicate track add failed" }, false };

        if (! nextUndo.apply (nextProject,
                              engine::ProjectEditCommand::setTrackMixScalars (newTrackId,
                                                                              source.strip.linearGain,
                                                                              source.strip.pan,
                                                                              source.strip.muted,
                                                                              source.strip.soloed,
                                                                              source.strip.soloSafe)).applied())
            return { id, { false, "duplicate strip copy failed" }, false };

        for (std::size_t position = 0; position < source.strip.fxChain.size(); ++position)
        {
            const engine::FxInsert& insert = source.strip.fxChain[position];
            const engine::EntityId newInsertId = allocateSessionEntityId (0xD2u, nextProject);
            if (! nextUndo.apply (nextProject,
                                  engine::ProjectEditCommand::addFxInsert (newTrackId,
                                                                           newInsertId,
                                                                           insert.kind,
                                                                           insert.enabled,
                                                                           position)).applied())
                return { id, { false, "duplicate FX insert failed" }, false };

            for (const auto& [paramId, value] : insert.normalizedParams)
            {
                if (! nextUndo.apply (nextProject,
                                      engine::ProjectEditCommand::setFxInsertParam (newTrackId,
                                                                                    newInsertId,
                                                                                    paramId,
                                                                                    value)).applied())
                    return { id, { false, "duplicate FX param failed" }, false };
            }
        }

        for (const engine::SendRow& send : source.sends)
        {
            const engine::EntityId newSendId = allocateSessionEntityId (0xE2u, nextProject);
            if (! nextUndo.apply (nextProject,
                                  engine::ProjectEditCommand::addSend (newTrackId,
                                                                       newSendId,
                                                                       send.busId,
                                                                       send.tap,
                                                                       send.linearGain)).applied())
                return { id, { false, "duplicate send failed" }, false };
        }

        for (const engine::Clip& clip : project_.clips)
        {
            if (clip.trackId != trackId)
                continue;

            engine::Clip copy = clip;
            copy.id = allocateSessionEntityId (0xC4u, nextProject);
            copy.trackId = newTrackId;
            if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::addClip (copy)).applied())
                return { id, { false, "duplicate clip failed" }, false };
        }

        for (const engine::MidiClip& midiClip : project_.midiClips)
        {
            if (midiClip.trackId != trackId)
                continue;

            const engine::EntityId newMidiClipId = allocateSessionEntityId (0xC5u, nextProject);
            if (! nextUndo.apply (nextProject,
                                  engine::ProjectEditCommand::addMidiClip (newMidiClipId,
                                                                           newTrackId,
                                                                           midiClip.timelineStart,
                                                                           midiClip.timelineLength,
                                                                           midiClip.timeBase)).applied())
                return { id, { false, "duplicate MIDI clip failed" }, false };

            for (const engine::Note& note : midiClip.notes)
            {
                engine::ProjectEditCommand noteCommand = engine::ProjectEditCommand::addNote (
                    newMidiClipId,
                    allocateSessionEntityId (0xC6u, nextProject),
                    note.startTick,
                    note.lengthTicks,
                    note.key,
                    note.normalizedVelocity);
                noteCommand.notePitch = note.pitchNote;
                noteCommand.notePort = note.portIndex;
                noteCommand.noteChannel = note.channel;
                if (! nextUndo.apply (nextProject, noteCommand).applied())
                    return { id, { false, "duplicate note failed" }, false };
            }
        }

        // AddTrack appends at the back; only reorder when the copy is not already directly below
        // the source (a same-position reorder is a no-op the diff recorder rightly refuses).
        if (sourceIndex + 2u < nextProject.tracks.size())
        {
            const engine::ProjectEditApplyResult reordered =
                nextUndo.apply (nextProject, engine::ProjectEditCommand::reorderTrack (newTrackId, sourceIndex + 1u));
            if (! reordered.applied())
            {
                if (grouped)
                    (void) nextUndo.endTransactionGroup();
                return { id, { false, "duplicate track reorder failed" }, false };
            }
        }
        if (grouped)
            (void) nextUndo.endTransactionGroup();

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "track duplicate did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.trackEditCount;
        return { id, state, true };
    }

    // Pro Tools-style remove-with-contents: deletes the Track's audio Clips and automation lanes as
    // their own undoable commands inside one transaction group, then removes the Track. Tracks that
    // still own MIDI Clips or recording Takes are refused (no delete verbs exist for those yet).
    [[nodiscard]] UiActionDispatchResult removeProjectTrack (engine::EntityId trackId)
    {
        const UiActionId id = UiActionId::TrackRemove;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        for (const engine::MidiClip& midiClip : project_.midiClips)
            if (midiClip.trackId == trackId)
                return { id, { false, "track still owns MIDI clips" }, false };

        for (const engine::RecordingTake& take : project_.recordingTakes)
            if (take.trackId == trackId)
                return { id, { false, "track still owns recorded takes" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const bool grouped = nextUndo.beginTransactionGroup();

        std::vector<engine::EntityId> ownedClipIds;
        for (const engine::Clip& clip : nextProject.clips)
            if (clip.trackId == trackId)
                ownedClipIds.push_back (clip.id);
        for (const engine::EntityId clipId : ownedClipIds)
        {
            if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::deleteClip (clipId)).applied())
                return { id, { false, "track clip delete failed" }, false };
        }

        std::vector<engine::EntityId> ownedLaneIds;
        for (const engine::AutomationLaneData& lane : nextProject.automationLanes)
            if (lane.ownerEntity == trackId)
                ownedLaneIds.push_back (lane.id);
        for (const engine::EntityId laneId : ownedLaneIds)
        {
            if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::removeAutomationLane (laneId)).applied())
                return { id, { false, "track lane delete failed" }, false };
        }

        const engine::ProjectEditApplyResult removed =
            nextUndo.apply (nextProject, engine::ProjectEditCommand::removeTrack (trackId));
        if (grouped)
            (void) nextUndo.endTransactionGroup();

        if (! removed.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "track edit did not persist" }, false };

        if (selectedTimelineClipId_.isValid() && findClip (selectedTimelineClipId_) == nullptr)
        {
            selectedTimelineClipIds_.clear();
            selectedTimelineClipId_ = {};
            context_.timelineClipSelected = false;
        }

        ++context_.commandDispatchCount;
        ++context_.trackEditCount;
        return { id, state, true };
    }

    // Create a MIDI Clip on the selected Track at the playhead (usable-DAW P1): one bar at the head
    // tempo/meter, selected for immediate piano-roll editing — MIDI without recording.
    [[nodiscard]] UiActionDispatchResult addMidiClipAtPlayhead()
    {
        if (project_.tracks.empty())
            return { UiActionId::TimelineMidiClipAdd, { false, "no track for the MIDI clip" }, false };

        const int selectedStrip = selectedMixerTrackStripIndex();
        const std::size_t trackIndex = selectedStrip >= 0
                && selectedStrip < static_cast<int> (project_.tracks.size())
            ? static_cast<std::size_t> (selectedStrip)
            : std::size_t {};
        return addMidiClipOnTrackAt (
            project_.tracks[trackIndex].id,
            static_cast<engine::Tick> (std::max<std::int64_t> (0, context_.playheadFrame)));
    }

    // Shared Ctrl+M / Pencil-tool law (E3): a one-bar MIDI clip (head tempo/meter) on the given
    // Track at the given tick, opening the piano roll on the new clip.
    [[nodiscard]] UiActionDispatchResult addMidiClipOnTrackAt (engine::EntityId trackId, engine::Tick startTick)
    {
        const UiActionId id = UiActionId::TimelineMidiClipAdd;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (findTrack (trackId) == nullptr || startTick < 0)
            return { id, { false, "no track for the MIDI clip" }, false };

        const double sampleRateHz = project_.sampleRate.isValid() ? project_.sampleRate.hz : 48000.0;
        const double bpm = ! project_.tempoMap.empty() ? project_.tempoMap.front().bpm : 120.0;
        const double beatsPerBar = ! project_.meterMap.empty()
            ? static_cast<double> (project_.meterMap.front().numerator)
            : 4.0;
        const engine::Tick barTicks = std::max<engine::Tick> (
            1, static_cast<engine::Tick> (sampleRateHz * 60.0 / std::clamp (bpm, 20.0, 400.0) * beatsPerBar + 0.5));

        engine::Project nextProject = project_;
        const engine::EntityId midiClipId = allocateSessionEntityId (0xD5u, nextProject);
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject,
                              engine::ProjectEditCommand::addMidiClip (
                                  midiClipId,
                                  trackId,
                                  startTick,
                                  barTicks,
                                  engine::TimeBase::SampleLocked)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "MIDI clip did not persist" }, false };

        selectedMidiClipId_ = midiClipId;
        selectedMidiNoteId_ = {};
        context_.activePanel = UiPanel::PianoRoll;
        syncProjectEditContext();
        ++context_.commandDispatchCount;
        ++context_.midiEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult addPianoRollNoteAt (engine::Tick startTick,
                                                             engine::Tick lengthTicks,
                                                             std::int16_t key)
    {
        const UiActionId id = UiActionId::PianoRollNoteAdd;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        const engine::EntityId noteId = allocateSessionEntityId (0xB2u, nextProject);

        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::addNote (selectedMidiClipId_, noteId, startTick, lengthTicks, key));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "note edit did not persist" }, false };

        selectedMidiNoteId_ = noteId;
        context_.midiNoteSelected = true;
        ++context_.commandDispatchCount;
        ++context_.midiEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult deleteSelectedPianoRollNote()
    {
        const UiActionId id = UiActionId::PianoRollNoteDelete;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::cutNote (selectedMidiClipId_, selectedMidiNoteId_));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "note edit did not persist" }, false };

        selectedMidiNoteId_ = {};
        context_.midiNoteSelected = false;
        ++context_.commandDispatchCount;
        ++context_.midiEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult splitSelectedTimelineClipAt (engine::Tick timelineTick)
    {
        const UiActionId id = UiActionId::TimelineClipSplit;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };

        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const engine::Clip* const clip = findClip (clipId);
            if (clip == nullptr || timelineTick <= clip->timelineStart)
                return { id, { false, "split must be inside every selected clip" }, false };

            const engine::Tick leftTimelineLength = timelineTick - clip->timelineStart;
            const std::optional<std::uint64_t> leftSourceLength =
                sourceLengthForSplit (*clip, leftTimelineLength);
            if (! leftSourceLength)
                return { id, { false, "split must be inside every selected clip" }, false };

            const engine::EntityId rightClipId = allocateSessionEntityId (0xC2u, nextProject);
            const engine::ProjectEditApplyResult applied = nextUndo.apply (
                nextProject,
                engine::ProjectEditCommand::splitClip (
                    clipId, rightClipId, leftTimelineLength, *leftSourceLength));
            if (! applied.applied())
                return { id, state, false };
        }

        if (! nextUndo.endTransactionGroup())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult healSelectedTimelineClips()
    {
        const UiActionId id = UiActionId::TimelineClipHeal;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (selectedTimelineClipIds_.size() != 2u)
            return { id, { false, "select exactly two clips" }, false };

        const engine::Clip* first = findClip (selectedTimelineClipIds_[0]);
        const engine::Clip* second = findClip (selectedTimelineClipIds_[1]);
        if (first == nullptr || second == nullptr)
            return { id, { false, "selected clip missing" }, false };
        if (second->timelineStart < first->timelineStart)
            std::swap (first, second);

        const engine::Clip left = *first;
        const engine::Clip right = *second;
        if (left.trackId != right.trackId || left.assetId != right.assetId)
            return { id, { false, "clips must share a track and asset" }, false };
        if (left.gain != right.gain
            || left.fadeIn != right.fadeIn
            || left.fadeOut != right.fadeOut
            || left.timeBase != right.timeBase)
        {
            return { id, { false, "clip playback settings differ" }, false };
        }

        constexpr engine::Tick maxTick = std::numeric_limits<engine::Tick>::max();
        constexpr std::uint64_t maxSource = std::numeric_limits<std::uint64_t>::max();
        if (left.timelineLength > maxTick - left.timelineStart
            || right.timelineLength > maxTick - left.timelineLength
            || left.srcLen > maxSource - left.srcOffset
            || right.srcLen > maxSource - left.srcLen)
        {
            return { id, { false, "clip window overflow" }, false };
        }

        const engine::Tick leftTimelineEnd = left.timelineStart + left.timelineLength;
        const std::uint64_t leftSourceEnd = left.srcOffset + left.srcLen;
        if (leftTimelineEnd != right.timelineStart || leftSourceEnd != right.srcOffset)
            return { id, { false, "clip windows are not contiguous" }, false };

        const engine::Tick joinedTimelineLength = left.timelineLength + right.timelineLength;
        const std::uint64_t joinedSourceLength = left.srcLen + right.srcLen;
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };
        if (! nextUndo.apply (
                 nextProject,
                 engine::ProjectEditCommand::trimClip (
                     left.id, left.timelineStart, joinedTimelineLength, left.srcOffset, joinedSourceLength)).applied()
            || ! nextUndo.apply (nextProject, engine::ProjectEditCommand::deleteClip (right.id)).applied()
            || ! nextUndo.endTransactionGroup())
        {
            return { id, state, false };
        }

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "healed clips did not persist" }, false };

        selectedTimelineClipIds_.assign (1, left.id);
        selectedTimelineClipId_ = left.id;
        context_.timelineClipSelected = true;
        context_.activePanel = UiPanel::Timeline;
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult crossfadeSelectedTimelineClips()
    {
        const UiActionId id = UiActionId::TimelineClipCrossfade;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (selectedTimelineClipIds_.size() != 2u)
            return { id, { false, "select exactly two clips" }, false };

        const engine::Clip* first = findClip (selectedTimelineClipIds_[0]);
        const engine::Clip* second = findClip (selectedTimelineClipIds_[1]);
        if (first == nullptr || second == nullptr)
            return { id, { false, "selected clip missing" }, false };
        if (second->timelineStart < first->timelineStart)
            std::swap (first, second);

        const engine::Clip left = *first;
        const engine::Clip right = *second;
        if (left.trackId != right.trackId)
            return { id, { false, "clips must share a track" }, false };
        if (left.timelineStart == right.timelineStart
            || left.timelineLength <= 0
            || right.timelineLength <= 0)
        {
            return { id, { false, "clips must form a staggered overlap" }, false };
        }

        constexpr engine::Tick maxTick = std::numeric_limits<engine::Tick>::max();
        if (left.timelineStart < 0
            || right.timelineStart < 0
            || left.timelineLength > maxTick - left.timelineStart
            || right.timelineLength > maxTick - right.timelineStart)
        {
            return { id, { false, "clip window overflow" }, false };
        }

        const engine::Tick leftEnd = left.timelineStart + left.timelineLength;
        const engine::Tick rightEnd = right.timelineStart + right.timelineLength;
        if (right.timelineStart >= leftEnd || leftEnd >= rightEnd)
            return { id, { false, "clips must overlap at their tail and head" }, false };

        const engine::Tick overlap = leftEnd - right.timelineStart;
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };
        if (! nextUndo.apply (
                 nextProject,
                 engine::ProjectEditCommand::setClipFades (left.id, left.fadeIn, overlap)).applied()
            || ! nextUndo.apply (
                 nextProject,
                 engine::ProjectEditCommand::setClipFades (right.id, overlap, right.fadeOut)).applied()
            || ! nextUndo.endTransactionGroup())
        {
            return { id, state, false };
        }

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "crossfade did not persist" }, false };

        context_.activePanel = UiPanel::Timeline;
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult nudgeSelection (UiActionId id)
    {
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const bool right = id == UiActionId::EditNudgeRight
                        || id == UiActionId::EditNudgeRightFine;
        const bool fine = id == UiActionId::EditNudgeLeftFine
                       || id == UiActionId::EditNudgeRightFine;
        const engine::Tick grid = std::max<engine::Tick> (1, context_.snapGridTicks);
        const engine::Tick amount = fine ? std::max<engine::Tick> (1, grid / 8) : grid;

        if (context_.activePanel == UiPanel::PianoRoll)
        {
            const engine::MidiClip* const midiClip = findMidiClip (selectedMidiClipId_);
            const engine::Note* const note = midiClip != nullptr
                ? findNote (*midiClip, selectedMidiNoteId_)
                : nullptr;
            if (midiClip == nullptr || note == nullptr)
                return { id, { false, "MIDI note missing" }, false };

            const engine::Tick maximumStart = midiClip->timelineLength - note->lengthTicks;
            const engine::Tick target = right
                ? note->startTick + std::min (amount, maximumStart - note->startTick)
                : note->startTick - std::min (amount, note->startTick);
            if (target == note->startTick)
                return { id, { false, "MIDI note is at the nudge boundary" }, false };

            UiActionDispatchResult result = moveSelectedPianoRollNoteTo (target);
            result.action = id;
            return result;
        }

        const UiTimelineEntityView anchor = timelineEntityView (selectedTimelineClipId_);
        if (! anchor.valid)
            return { id, { false, "timeline clip missing" }, false };

        engine::Tick earliestStart = anchor.timelineStart;
        if (right)
        {
            constexpr engine::Tick maxTick = std::numeric_limits<engine::Tick>::max();
            for (engine::EntityId clipId : selectedTimelineClipIds_)
            {
                const UiTimelineEntityView view = timelineEntityView (clipId);
                if (! view.valid
                    || view.timelineStart > maxTick - amount
                    || view.timelineLength > maxTick - (view.timelineStart + amount))
                {
                    return { id, { false, "nudge would overflow timeline" }, false };
                }
            }
        }
        else
        {
            for (engine::EntityId clipId : selectedTimelineClipIds_)
            {
                const UiTimelineEntityView view = timelineEntityView (clipId);
                if (! view.valid)
                    return { id, { false, "timeline clip missing" }, false };
                earliestStart = std::min (earliestStart, view.timelineStart);
            }
            if (earliestStart == 0)
                return { id, { false, "selection is at the nudge boundary" }, false };
        }

        const engine::Tick target = right
            ? anchor.timelineStart + amount
            : anchor.timelineStart - std::min (earliestStart, amount);
        UiActionDispatchResult result = moveSelectedTimelineClipTo (target);
        result.action = id;
        return result;
    }

    [[nodiscard]] UiActionDispatchResult trimSelectedTimelineClipRightTo (engine::Tick timelineEnd)
    {
        const UiActionId id = UiActionId::TimelineClipTrim;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const engine::Clip* const clip = findClip (selectedTimelineClipId_);
        if (clip == nullptr)
            return { id, { false, "timeline clip missing" }, false };

        if (timelineEnd <= clip->timelineStart)
            return { id, { false, "trim must leave a positive clip length" }, false };

        const engine::Tick nextTimelineLength = timelineEnd - clip->timelineStart;
        const std::optional<std::uint64_t> nextSourceLength =
            sourceLengthForShortenedRightEdge (*clip, nextTimelineLength);
        if (! nextSourceLength)
            return { id, { false, "trim must shorten selected clip" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::trimClip (
                selectedTimelineClipId_, clip->timelineStart, nextTimelineLength, clip->srcOffset, *nextSourceLength));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    // Trim the selected Clip's LEFT edge inward (usable-DAW P1): the head moves later on the
    // timeline while the source window advances proportionally, so the audio under the playhead
    // never shifts. Extending the head earlier than the recorded window is out of alpha scope.
    [[nodiscard]] UiActionDispatchResult trimSelectedTimelineClipLeftTo (engine::Tick timelineStart)
    {
        const UiActionId id = UiActionId::TimelineClipTrim;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const engine::Clip* const clip = findClip (selectedTimelineClipId_);
        if (clip == nullptr)
            return { id, { false, "timeline clip missing" }, false };

        if (timelineStart <= clip->timelineStart
            || timelineStart >= clip->timelineStart + clip->timelineLength)
            return { id, { false, "left trim must stay inside the clip" }, false };

        const engine::Tick trimmedTicks = timelineStart - clip->timelineStart;
        const engine::Tick nextTimelineLength = clip->timelineLength - trimmedTicks;
        const std::optional<std::uint64_t> keptSourceLength =
            sourceLengthForShortenedRightEdge (*clip, nextTimelineLength);
        if (! keptSourceLength)
            return { id, { false, "left trim must keep a positive source window" }, false };

        const std::uint64_t consumedSource = clip->srcLen - *keptSourceLength;
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::trimClip (
                selectedTimelineClipId_,
                timelineStart,
                nextTimelineLength,
                clip->srcOffset + consumedSource,
                *keptSourceLength));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    // Clip clipboard (usable-DAW P1): Copy captures the selected Clip's non-identity fields; Paste
    // creates a fresh Clip at the playhead on the selected (or owning/first) Track; Duplicate appends
    // a copy right after the source. All paste/duplicate paths run the undoable AddClip command.
    // Metronome toggle (usable-DAW P1): a monitoring click overlay in the playback engine, following
    // the project's head tempo and meter. Never part of offline Render/export. Reapplied whenever the
    // engine is replaced (edits rebuild playback) so the click survives every edit.
    // Tempo-derived snap grid (usable-DAW P1). The registry keeps abstract placeholder tick values;
    // the model overwrites snapGridTicks with REAL frame counts from the head tempo/meter so a "bar"
    // is a bar at the current BPM. Re-derived whenever tempo, meter, or the unit changes.
    enum class UiSnapUnit : std::uint8_t { Off, Bar, Beat, Sixteenth };

    [[nodiscard]] UiSnapUnit snapUnit() const noexcept { return snapUnit_; }

    static constexpr int kMinRepeatPasteCount = 2;
    static constexpr int kMaxRepeatPasteCount = 8;
    static constexpr int kDefaultRepeatPasteCount = 2;

    [[nodiscard]] int repeatPasteCount() const noexcept { return repeatPasteCount_; }

    void setRepeatPasteCount (int count) noexcept
    {
        repeatPasteCount_ = std::clamp (count, kMinRepeatPasteCount, kMaxRepeatPasteCount);
    }

    void refreshSnapGrid() noexcept
    {
        if (snapUnit_ == UiSnapUnit::Off)
        {
            context_.snapEnabled = false;
            return;
        }

        context_.snapEnabled = true;
        context_.snapGridTicks = snapFramesForUnit (snapUnit_);
    }

    void setSnapUnit (UiSnapUnit unit) noexcept
    {
        snapUnit_ = unit;
        refreshSnapGrid();
    }

    // Timeline markers (usable-DAW P1): add at an arbitrary tick (ruler double-click or M at the
    // playhead), remove the nearest marker to a tick (Alt+click or Shift+M). Undoable, persisted.
    [[nodiscard]] UiActionDispatchResult addTimelineMarkerAtTick (engine::Tick tick)
    {
        const UiActionId id = UiActionId::TimelineMarkerAdd;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (tick < 0)
            return { id, { false, "marker tick must be non-negative" }, false };

        engine::Project nextProject = project_;
        const engine::EntityId markerId = allocateSessionEntityId (0xE1u, nextProject);
        const std::string name = "Marker " + std::to_string (nextProject.markers.size() + 1u);

        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::addMarker (markerId, tick, name)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "marker edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult removeTimelineMarkerNearestTick (engine::Tick tick)
    {
        const UiActionId id = UiActionId::TimelineMarkerRemove;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (project_.markers.empty())
            return { id, { false, "no markers to remove" }, false };

        const engine::Marker* nearest = &project_.markers.front();
        for (const engine::Marker& marker : project_.markers)
            if (std::llabs (marker.tick - tick) < std::llabs (nearest->tick - tick))
                nearest = &marker;

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::removeMarker (nearest->id)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "marker edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    // Marker editing (E7): drag-move on the ruler and inline rename, both undoable.
    [[nodiscard]] UiActionDispatchResult moveTimelineMarkerTo (engine::EntityId markerId, engine::Tick newTick)
    {
        const UiActionId id = UiActionId::TimelineMarkerAdd;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const auto marker = std::find_if (project_.markers.begin(), project_.markers.end(),
                                          [markerId] (const engine::Marker& candidate) {
                                              return candidate.id == markerId;
                                          });
        if (marker == project_.markers.end() || newTick < 0 || marker->tick == newTick)
            return { id, { false, "marker move needs a real new tick" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::moveMarker (markerId, newTick)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "marker edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult renameTimelineMarker (engine::EntityId markerId, const std::string& newName)
    {
        const UiActionId id = UiActionId::TimelineMarkerAdd;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (newName.empty() || newName.size() > engine::ProjectEditCommand::kMaxTrackNameLength)
            return { id, { false, "marker name must be 1-127 characters" }, false };

        const auto marker = std::find_if (project_.markers.begin(), project_.markers.end(),
                                          [markerId] (const engine::Marker& candidate) {
                                              return candidate.id == markerId;
                                          });
        if (marker == project_.markers.end() || marker->name == newName)
            return { id, { false, "marker rename needs a real new name" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::renameMarker (markerId, newName)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "marker edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult toggleMetronome()
    {
        const UiActionId id = UiActionId::TransportToggleMetronome;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        context_.metronomeEnabled = ! context_.metronomeEnabled;
        applyMetronomeToPlayback();
        ++context_.commandDispatchCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult copySelectedTimelineClip()
    {
        const UiActionId id = UiActionId::TimelineClipCopy;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        UiClipClipboard clipboard = makeClipboardForSelection();
        if (clipboard.clips.empty())
            return { id, { false, "timeline clip missing" }, false };

        clipClipboard_ = std::move (clipboard);
        context_.clipboardHasClip = true;
        ++context_.commandDispatchCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult cutSelectedTimelineClip()
    {
        const UiActionId id = UiActionId::TimelineClipCut;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        UiClipClipboard clipboard = makeClipboardForSelection();
        if (clipboard.clips.empty())
            return { id, { false, "timeline clip missing" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };
        for (engine::EntityId clipId : selectedTimelineClipIds_)
            if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::deleteClip (clipId)).applied())
                return { id, state, false };
        if (! nextUndo.endTransactionGroup())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        clipClipboard_ = clipboard;
        selectedTimelineClipIds_.clear();
        selectedTimelineClipId_ = {};
        context_.clipboardHasClip = true;
        context_.timelineClipSelected = false;
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult pasteClipboardClipAtPlayhead()
    {
        const UiActionId id = UiActionId::TimelineClipPaste;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (clipClipboard_.clips.empty())
            return { id, { false, "clipboard has no clip" }, false };

        // Target: the selected mixer/rail Track, else the copied Clip's own Track family, else first.
        engine::EntityId targetTrackId;
        const int selectedStrip = selectedMixerTrackStripIndex();
        if (selectedStrip >= 0 && selectedStrip < static_cast<int> (project_.tracks.size()))
            targetTrackId = project_.tracks[static_cast<std::size_t> (selectedStrip)].id;
        else if (const engine::Clip* const sourceClip = findClip (selectedTimelineClipId_))
            targetTrackId = sourceClip->trackId;
        else if (! project_.tracks.empty())
            targetTrackId = project_.tracks.front().id;
        else
            return { id, { false, "no track to paste onto" }, false };

        return addClipsFromClipboard (id, state, targetTrackId,
                                      static_cast<engine::Tick> (std::max<std::int64_t> (0, context_.playheadFrame)),
                                      1);
    }

    [[nodiscard]] UiActionDispatchResult repeatPasteClipboardAtPlayhead()
    {
        const UiActionId id = UiActionId::TimelineClipRepeatPaste;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (clipClipboard_.clips.empty())
            return { id, { false, "clipboard has no clip" }, false };

        engine::EntityId targetTrackId;
        const int selectedStrip = selectedMixerTrackStripIndex();
        if (selectedStrip >= 0 && selectedStrip < static_cast<int> (project_.tracks.size()))
            targetTrackId = project_.tracks[static_cast<std::size_t> (selectedStrip)].id;
        else if (const engine::Clip* const sourceClip = findClip (selectedTimelineClipId_))
            targetTrackId = sourceClip->trackId;
        else if (! project_.tracks.empty())
            targetTrackId = project_.tracks.front().id;
        else
            return { id, { false, "no track to paste onto" }, false };

        return addClipsFromClipboard (
            id,
            state,
            targetTrackId,
            static_cast<engine::Tick> (std::max<std::int64_t> (0, context_.playheadFrame)),
            repeatPasteCount_);
    }

    [[nodiscard]] UiActionDispatchResult duplicateSelectedTimelineClip()
    {
        const UiActionId id = UiActionId::TimelineClipDuplicate;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const UiTimelineEntityView anchor = timelineEntityView (selectedTimelineClipId_);
        if (! anchor.valid)
            return { id, { false, "timeline clip missing" }, false };

        // Duplicate the WHOLE selection (E2/E8): fresh-id copies of every selected audio or MIDI
        // clip land directly after the selection's span, preserving relative time and track
        // offsets, as one undo transaction. A single-clip selection keeps the historical law
        // (its copy appended right after the source on the same track).
        engine::Tick earliestStart = anchor.timelineStart;
        engine::Tick selectionSpan = 0;
        constexpr engine::Tick maxTick = std::numeric_limits<engine::Tick>::max();
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            if (! view.valid)
                return { id, { false, "timeline clip missing" }, false };
            earliestStart = std::min (earliestStart, view.timelineStart);
        }
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            selectionSpan = std::max (selectionSpan,
                                      view.timelineStart - earliestStart + view.timelineLength);
        }
        if (selectionSpan <= 0)
            return { id, { false, "timeline clip missing" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };
        std::vector<engine::EntityId> copiedClipIds;
        copiedClipIds.reserve (selectedTimelineClipIds_.size());
        engine::EntityId copiedAnchorId;
        for (engine::EntityId clipId : selectedTimelineClipIds_)
        {
            const UiTimelineEntityView view = timelineEntityView (clipId);
            if (view.timelineStart > maxTick - selectionSpan
                || view.timelineLength > maxTick - (view.timelineStart + selectionSpan))
                return { id, { false, "duplicate would overflow timeline" }, false };
            const engine::Tick copyStart = view.timelineStart + selectionSpan;

            engine::EntityId copiedId;
            if (view.isMidi)
            {
                const engine::MidiClip* const source = findMidiClip (clipId);
                if (source == nullptr
                    || ! appendMidiClipCopy (nextProject, nextUndo, *source, view.trackId, copyStart, copiedId))
                    return { id, state, false };
            }
            else
            {
                engine::Clip copy = *findClip (clipId);
                copy.id = allocateSessionEntityId (0xC3u, nextProject);
                copy.timelineStart = copyStart;
                if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::addClip (copy)).applied())
                    return { id, state, false };
                copiedId = copy.id;
            }
            copiedClipIds.push_back (copiedId);
            if (clipId == selectedTimelineClipId_)
                copiedAnchorId = copiedId;
        }
        if (! nextUndo.endTransactionGroup())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        selectedTimelineClipIds_ = std::move (copiedClipIds);
        selectedTimelineClipId_ = copiedAnchorId.isValid() ? copiedAnchorId : selectedTimelineClipIds_.back();
        context_.timelineClipSelected = true;
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult setSelectedTimelineClipGain (float newGain)
    {
        return applySelectedTimelineClipGain (UiActionId::TimelineClipSetGain, newGain);
    }

    [[nodiscard]] UiActionDispatchResult stepSelectedTimelineClipGain (UiActionId id)
    {
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const engine::Clip* const clip = findClip (selectedTimelineClipId_);
        if (clip == nullptr)
            return { id, state, false };

        constexpr float oneDecibelRatio = 1.1220184543f;
        const float nextGain = id == UiActionId::TimelineClipGainIncrease
                                 ? clip->gain * oneDecibelRatio
                                 : clip->gain / oneDecibelRatio;
        return applySelectedTimelineClipGain (id, nextGain);
    }

    [[nodiscard]] UiActionDispatchResult setSelectedTimelineClipFades (engine::Tick fadeIn, engine::Tick fadeOut)
    {
        return applySelectedTimelineClipFades (UiActionId::TimelineClipSetFades, fadeIn, fadeOut);
    }

    [[nodiscard]] UiActionDispatchResult applyDefaultTimelineClipFades()
    {
        const UiActionId id = UiActionId::TimelineClipApplyDefaultFades;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const engine::Clip* const clip = findClip (selectedTimelineClipId_);
        if (clip == nullptr || ! project_.sampleRate.isValid())
            return { id, state, false };

        const double requestedTicks = UiThemeLayout::timelineClipDefaultFadeSeconds * project_.sampleRate.hz;
        if (! std::isfinite (requestedTicks)
            || requestedTicks > static_cast<double> (std::numeric_limits<engine::Tick>::max()))
            return { id, { false, "default fade length is invalid" }, false };

        const engine::Tick defaultTicks = std::max<engine::Tick> (1, static_cast<engine::Tick> (std::llround (requestedTicks)));
        const engine::Tick fadeTicks = std::min (defaultTicks, std::max<engine::Tick> (0, clip->timelineLength / 2));
        return applySelectedTimelineClipFades (id, fadeTicks, fadeTicks);
    }

    // Automation lane canvas verbs (usable-DAW P1): target ANY track's fader lane by owner id,
    // creating the lane on first use (grouped with the first breakpoint into one undo step).
    [[nodiscard]] const engine::AutomationLaneData* trackFaderAutomationLane (engine::EntityId trackId) const noexcept
    {
        for (const engine::AutomationLaneData& lane : project_.automationLanes)
            if (lane.ownerEntity == trackId
                && lane.role == engine::AutomationTargetRole::TrackFader
                && lane.paramId == engine::FaderNode::kGainParameterId)
                return &lane;

        return nullptr;
    }

    [[nodiscard]] UiActionDispatchResult addAutomationBreakpointToTrackLane (engine::EntityId trackId,
                                                                             engine::Tick tick,
                                                                             double value)
    {
        const UiActionId id = UiActionId::TimelineAutomationAddBreakpoint;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (tick < 0 || ! std::isfinite (value))
            return { id, { false, "breakpoint payload invalid" }, false };

        const double clampedValue = std::clamp (value, 0.0, 1.0);
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const bool grouped = nextUndo.beginTransactionGroup();

        engine::EntityId laneId;
        if (const engine::AutomationLaneData* const lane = trackFaderAutomationLane (trackId))
        {
            laneId = lane->id;
        }
        else
        {
            laneId = allocateSessionEntityId (0xE2u, nextProject);
            engine::ProjectEditCommand createLane;
            createLane.verb = engine::ProjectEditVerb::AddAutomationLane;
            createLane.automationLaneId = laneId;
            createLane.automationOwnerId = trackId;
            createLane.automationRole = engine::AutomationTargetRole::TrackFader;
            createLane.automationParamId = engine::FaderNode::kGainParameterId;
            if (! nextUndo.apply (nextProject, createLane).applied())
            {
                if (grouped)
                    (void) nextUndo.endTransactionGroup();
                return { id, { false, "automation lane create failed" }, false };
            }
        }

        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::addAutomationBreakpoint (laneId, tick, clampedValue,
                                                                 engine::AutomationCurveType::Linear));
        if (grouped)
            (void) nextUndo.endTransactionGroup();
        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "automation edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineAutomationBreakpointEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult moveAutomationBreakpointTo (engine::EntityId laneId,
                                                                     engine::Tick oldTick,
                                                                     engine::Tick newTick,
                                                                     double newValue)
    {
        const UiActionId id = UiActionId::TimelineAutomationAddBreakpoint;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (oldTick < 0 || newTick < 0 || ! std::isfinite (newValue))
            return { id, { false, "breakpoint payload invalid" }, false };

        const double clampedValue = std::clamp (newValue, 0.0, 1.0);
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const bool grouped = nextUndo.beginTransactionGroup();

        bool ok = true;
        if (newTick != oldTick)
            ok = nextUndo.apply (nextProject,
                                 engine::ProjectEditCommand::moveAutomationBreakpoint (laneId, oldTick, newTick))
                     .applied();
        if (ok)
            ok = nextUndo.apply (nextProject,
                                 engine::ProjectEditCommand::setAutomationBreakpointValue (laneId, newTick, clampedValue))
                     .applied();
        if (grouped)
            (void) nextUndo.endTransactionGroup();
        if (! ok)
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "automation edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineAutomationBreakpointEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult removeAutomationBreakpointAtTick (engine::EntityId laneId,
                                                                           engine::Tick tick)
    {
        const UiActionId id = UiActionId::TimelineAutomationDeleteBreakpoint;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.apply (nextProject,
                              engine::ProjectEditCommand::removeAutomationBreakpoint (laneId, tick)).applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "automation edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineAutomationBreakpointEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult addFirstTrackAutomationBreakpoint (
        engine::Tick tick = kFirstTrackAutomationBreakpointAddTick,
        double value = kFirstTrackAutomationBreakpointAddValue,
        engine::AutomationCurveType curve = engine::AutomationCurveType::Linear)
    {
        const UiActionId id = UiActionId::TimelineAutomationAddBreakpoint;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (! context_.timelineAutomationTrackLaneVisible)
            return { id, { false, "automation lane hidden" }, false };

        const engine::AutomationLaneData* const lane = firstTrackFaderAutomationLane();
        if (lane == nullptr)
            return { id, { false, "first Track fader automation lane missing" }, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::addAutomationBreakpoint (lane->id, tick, value, curve));

        if (! applied.applied())
            return { id, state, false };

        if (canAdoptEditWithoutPlaybackRebuild (nextProject))
        {
            if (! adoptEditedProjectWithoutPlaybackRebuild (std::move (nextProject), std::move (nextUndo)))
                return { id, { false, "automation breakpoint edit did not persist" }, false };
        }
        else if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
        {
            return { id, { false, "automation breakpoint edit did not persist" }, false };
        }

        context_.activePanel = UiPanel::Timeline;
        ++context_.commandDispatchCount;
        ++context_.timelineAutomationBreakpointEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult deleteLastFirstTrackAutomationBreakpoint()
    {
        const UiActionId id = UiActionId::TimelineAutomationDeleteBreakpoint;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (! context_.timelineAutomationTrackLaneVisible)
            return { id, { false, "automation lane hidden" }, false };

        const engine::AutomationLaneData* const lane = firstTrackFaderAutomationLane();
        if (lane == nullptr)
            return { id, { false, "first Track fader automation lane missing" }, false };

        if (lane->points.empty())
            return { id, { false, "first Track fader automation lane has no breakpoints" }, false };

        const engine::Tick tick = lane->points.back().tick;
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::removeAutomationBreakpoint (lane->id, tick));

        if (! applied.applied())
            return { id, state, false };

        if (canAdoptEditWithoutPlaybackRebuild (nextProject))
        {
            if (! adoptEditedProjectWithoutPlaybackRebuild (std::move (nextProject), std::move (nextUndo)))
                return { id, { false, "automation breakpoint edit did not persist" }, false };
        }
        else if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
        {
            return { id, { false, "automation breakpoint edit did not persist" }, false };
        }

        context_.activePanel = UiPanel::Timeline;
        ++context_.commandDispatchCount;
        ++context_.timelineAutomationBreakpointEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiAppLoadResult loadProjectBundle (
        const std::filesystem::path& bundlePath,
        std::span<const UiDecodedAsset> decodedAssets)
    {
        return loadProjectBundle (bundlePath, decodedAssets, playbackBuildOptions());
    }

    [[nodiscard]] UiAppLoadResult loadProjectBundle (
        const std::filesystem::path& bundlePath,
        std::span<const UiDecodedAsset> decodedAssets,
        engine::OfflineRenderOptions options)
    {
        UiAppLoadResult result;

        persistence::ProjectBundleDb opened;
        result.bundleResult = persistence::ProjectBundleDb::openExistingBundle (bundlePath, opened);
        if (! result.bundleResult.ok())
        {
            result.status = UiAppLoadStatus::BundleOpenFailed;
            return result;
        }

        engine::Project loadedProject;
        result.bundleResult = opened.readProjectSnapshot (loadedProject);
        if (! result.bundleResult.ok())
        {
            result.status = UiAppLoadStatus::ProjectReadFailed;
            return result;
        }

        std::vector<UiDecodedAsset> ownedDecoded (decodedAssets.begin(), decodedAssets.end());
        std::vector<engine::DecodedAssetAudio> decodedViews = makeDecodedViews (ownedDecoded);

        engine::PlaybackEngine::Result built = engine::PlaybackEngine::create (
            loadedProject,
            std::span<const engine::DecodedAssetAudio> (decodedViews.data(), decodedViews.size()),
            options);

        result.playbackStatus = built.status;
        result.projectError = built.projectError;
        result.mixerError = built.mixerError;
        if (! built.ok())
        {
            result.status = UiAppLoadStatus::PlaybackBuildFailed;
            return result;
        }

        (void) built.engine->stop();
        drainTransport (*built.engine);

        attachProjectBundle (std::move (opened), bundlePath, std::move (loadedProject));
        decodedAssets_ = std::move (ownedDecoded);
        decodedAssetViews_ = makeDecodedViews (decodedAssets_);
        replacePlayback (std::move (built.engine));
        enqueueWaveformBuildsForDecodedAssets();

        resetContextForFreshPlayback();
        detectAutosaveRecoveryPrompt();

        result.status = UiAppLoadStatus::Ok;
        return result;
    }

    [[nodiscard]] UiActionDispatchResult dispatch (UiActionId id)
    {
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        switch (id)
        {
            case UiActionId::ProjectNew:
                return { id, { false, "new project not wired" }, false };

            case UiActionId::ProjectOpen:
                return { id, { false, "open path required" }, false };

            case UiActionId::ProjectImportAudio:
                return { id, { false, "audio import path required" }, false };

            case UiActionId::ProjectExportAudio:
                return { id, { false, "audio export path required" }, false };

            case UiActionId::ProjectExportDawproject:
                return { id, { false, "DAWproject export path required" }, false };

            case UiActionId::ProjectSaveAs:
                return { id, { false, "save-as path required" }, false };

            case UiActionId::ProjectSave:
            {
                const persistence::BundleResult saved = saveProjectBundle();
                return { id, state, saved.ok() };
            }

            case UiActionId::TransportPlay:
            {
                const bool startingPlayback = ! context_.isPlaying;
                const std::int64_t playbackStart = context_.playheadFrame;
                UiActionDispatchResult result =
                    dispatchTransport (id, [this] { return playback_ != nullptr && playback_->play(); });
                if (result.dispatched && startingPlayback)
                    context_.playbackStartFrame = playbackStart;
                return result;
            }

            case UiActionId::TransportPlayFromLastLocate:
            {
                const std::int64_t targetFrame = context_.lastLocateFrame;
                UiActionDispatchResult result = dispatchTransport (id, [this, targetFrame] {
                    return playback_ != nullptr
                        && playback_->locate (targetFrame)
                        && playback_->play();
                });
                if (result.dispatched)
                    context_.playbackStartFrame = targetFrame;
                return result;
            }

            case UiActionId::TransportShuttleFaster:
            {
                const bool startingPlayback = ! context_.isPlaying;
                const std::int64_t playbackStart = context_.playheadFrame;
                UiActionDispatchResult result = dispatchTransport (id, [this] {
                    if (playback_ == nullptr)
                        return false;
                    if (! context_.isPlaying)
                        return playback_->play();

                    const int nextRate = std::min (4, context_.shuttlePlaybackRate * 2);
                    return playback_->setPlaybackRate (nextRate);
                });
                if (result.dispatched && startingPlayback)
                    context_.playbackStartFrame = playbackStart;
                return result;
            }

            case UiActionId::TransportShuttleSlower:
                return dispatchTransport (id, [this] {
                    if (playback_ == nullptr || ! context_.isPlaying)
                        return false;
                    if (context_.shuttlePlaybackRate > 1)
                        return playback_->setPlaybackRate (context_.shuttlePlaybackRate / 2);

                    // Reverse playback is outside the engine contract. At 1x, J reaches the honest
                    // zero-speed boundary; the configured Stop policy owns the resulting position.
                    return stopPlaybackAtConfiguredPosition();
                });

            case UiActionId::TransportStop:
                if (context_.recordCountInActive)
                {
                    cancelRecordingCountIn();
                    ++context_.commandDispatchCount;
                    return { id, { true, "" }, true };
                }
                return dispatchTransport (id, [this] { return stopPlaybackAtConfiguredPosition(); });

            case UiActionId::TransportLocateStart:
            case UiActionId::TransportReturnToZero:
                return locatePlaybackAbsolute (id, 0);

            case UiActionId::TransportLocatePreviousGrid:
                return locatePlaybackRelative (id, -context_.snapGridTicks);

            case UiActionId::TransportLocateNextGrid:
                return locatePlaybackRelative (id, context_.snapGridTicks);

            case UiActionId::TransportLocatePreviousBar:
                return locatePlaybackRelative (id, -snapFramesForUnit (UiSnapUnit::Bar));

            case UiActionId::TransportLocateNextBar:
                return locatePlaybackRelative (id, snapFramesForUnit (UiSnapUnit::Bar));

            case UiActionId::TransportStoreLocatePoint1:
            case UiActionId::TransportStoreLocatePoint2:
            case UiActionId::TransportStoreLocatePoint3:
            case UiActionId::TransportStoreLocatePoint4:
            case UiActionId::TransportStoreLocatePoint5:
                return storeLocatePoint (id, state);

            case UiActionId::TransportRecallLocatePoint1:
            case UiActionId::TransportRecallLocatePoint2:
            case UiActionId::TransportRecallLocatePoint3:
            case UiActionId::TransportRecallLocatePoint4:
            case UiActionId::TransportRecallLocatePoint5:
                return recallLocatePoint (id, state);

            case UiActionId::TransportLocatePreviousMarker:
                return locateAdjacentTimelineMarker (id, false);

            case UiActionId::TransportLocateNextMarker:
                return locateAdjacentTimelineMarker (id, true);

            case UiActionId::TimelineRangeToLoop:
                return convertTimelineRangeToLoop (id, state);

            case UiActionId::TransportToggleLoop:
                return dispatchTransport (id, [this] {
                    if (playback_ == nullptr)
                        return false;

                    if (playback_->loopEnabled())
                        return playback_->clearLoop();

                    if (playback_->frames() == 0
                        || playback_->frames() > static_cast<std::uint64_t> (std::numeric_limits<std::int64_t>::max()))
                        return false;

                    return playback_->setLoop (0, static_cast<std::int64_t> (playback_->frames()));
                });

            case UiActionId::DeviceRefreshAudio:
                return refreshAudioDevices();

            case UiActionId::DeviceSelectTestAudio:
                return selectTestAudioDevice();

            case UiActionId::RecordingArmTrack:
                return toggleDefaultTrackRecordingArm();

            case UiActionId::RecordingSetMonitoringPolicy:
                return selectInputMonitoringPolicy();

            case UiActionId::TransportRecord:
            {
                const UiAppRecordResult recorded = recordDeterministicTestAudioTake();
                return { id, recorded.actionState, recorded.ok() };
            }

            case UiActionId::RecordingAssembleComp:
                return assembleBasicRecordingCompSelection();

            case UiActionId::AutosaveRecoveryRestore:
                return restorePendingAutosaveSnapshot();

            case UiActionId::AutosaveRecoveryDiscard:
                return discardPendingAutosaveSnapshot();

            case UiActionId::EditUndo:
                return dispatchUndo (id, state);

            case UiActionId::EditRedo:
                return dispatchRedo (id, state);

            case UiActionId::ViewTimeline:
            case UiActionId::ViewMixer:
            case UiActionId::MixerReadMeters:
            case UiActionId::MixerReadLoudness:
            case UiActionId::MixerReadSends:
            case UiActionId::MixerReadFxSlots:
            case UiActionId::MixerReadGainReduction:
            case UiActionId::MixerReadBusFxSlots:
            case UiActionId::ProjectExportAudioCancel:
            case UiActionId::HelpShowKeymap:
            case UiActionId::TimelineToolSelectPointer:
            case UiActionId::TimelineToolSelectPencil:
            case UiActionId::TimelineToolSelectScissors:
            case UiActionId::TimelineToolSelectHand:
            case UiActionId::TimelineToolSelectZoom:
            case UiActionId::TimelineZoomFitProject:
            case UiActionId::TimelineZoomFitLoop:
            case UiActionId::TimelineZoomIn:
            case UiActionId::TimelineZoomOut:
            case UiActionId::TrackSelectPrevious:
            case UiActionId::TrackSelectNext:
            case UiActionId::TimelineTogglePlayheadFollow:
            case UiActionId::TransportToggleReturnToStartOnStop:
            case UiActionId::TransportToggleRecordCountIn:
            case UiActionId::TimelineAutomationToggleTrackLane:
            {
                return registry_.dispatch (id, context_);
            }

            case UiActionId::TimelineSnapDisable:
            case UiActionId::TimelineSnapSetBar:
            case UiActionId::TimelineSnapSetBeat:
            case UiActionId::TimelineSnapSetSixteenth:
            {
                // The registry flips the abstract snap state; the model then overwrites the grid with
                // REAL frame counts derived from the head tempo/meter (a bar is a bar at this BPM).
                UiActionDispatchResult snapResult = registry_.dispatch (id, context_);
                if (snapResult.dispatched)
                {
                    snapUnit_ = id == UiActionId::TimelineSnapDisable ? UiSnapUnit::Off
                              : id == UiActionId::TimelineSnapSetBar ? UiSnapUnit::Bar
                              : id == UiActionId::TimelineSnapSetBeat ? UiSnapUnit::Beat
                              : UiSnapUnit::Sixteenth;
                    refreshSnapGrid();
                }
                return snapResult;
            }

            case UiActionId::MixerToggleFirstFxSlotEnabled:
                return toggleFirstTrackFxSlotEnabled();

            case UiActionId::MixerFxInsertAdd:
            case UiActionId::MixerFxInsertRemove:
            case UiActionId::MixerFxInsertToggle:
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "FX slot payload required" }, false };
            }

            case UiActionId::TransportSetTempo:
            case UiActionId::TransportSetMeter:
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "time map payload required" }, false };
            }

            case UiActionId::TimelineClipCopy:
                return copySelectedTimelineClip();

            case UiActionId::TimelineClipCut:
                return cutSelectedTimelineClip();

            case UiActionId::TimelineClipPaste:
                return pasteClipboardClipAtPlayhead();

            case UiActionId::TimelineClipRepeatPaste:
                return repeatPasteClipboardAtPlayhead();

            case UiActionId::TimelineClipDuplicate:
                return duplicateSelectedTimelineClip();

            case UiActionId::TimelineClipSelectAllTrack:
                return selectAllTimelineClipsOnSelectedTrack();

            case UiActionId::TimelineClipSelectAllProject:
                return selectAllTimelineClipsInProject();

            case UiActionId::TimelineClipHeal:
                return healSelectedTimelineClips();

            case UiActionId::TimelineClipCrossfade:
                return crossfadeSelectedTimelineClips();

            case UiActionId::EditNudgeLeft:
            case UiActionId::EditNudgeRight:
            case UiActionId::EditNudgeLeftFine:
            case UiActionId::EditNudgeRightFine:
                return nudgeSelection (id);

            case UiActionId::TimelineClipGainIncrease:
            case UiActionId::TimelineClipGainDecrease:
                return stepSelectedTimelineClipGain (id);

            case UiActionId::TimelineClipApplyDefaultFades:
                return applyDefaultTimelineClipFades();

            case UiActionId::TransportToggleMetronome:
                return toggleMetronome();

            case UiActionId::TimelineMarkerAdd:
                return addTimelineMarkerAtTick (
                    static_cast<engine::Tick> (std::max<std::int64_t> (0, context_.playheadFrame)));

            case UiActionId::TimelineMarkerRemove:
                return removeTimelineMarkerNearestTick (
                    static_cast<engine::Tick> (std::max<std::int64_t> (0, context_.playheadFrame)));

            case UiActionId::TimelineMidiClipAdd:
                return addMidiClipAtPlayhead();

            case UiActionId::MixerFxInsertParamSet:
                return { id, { false, "FX param payload required" }, false };

            case UiActionId::MixerBusAdd:
                return addBusToMixer();

            case UiActionId::MixerSendAdd:
            case UiActionId::MixerSendRemove:
            case UiActionId::MixerSendSetLevel:
                return { id, { false, "send payload required" }, false };

            case UiActionId::MixerSetFirstSendLevel:
                return setFirstTrackFirstSendLevel();

            case UiActionId::TimelineAutomationAddBreakpoint:
                return addFirstTrackAutomationBreakpoint();

            case UiActionId::TimelineAutomationDeleteBreakpoint:
                return deleteLastFirstTrackAutomationBreakpoint();

            case UiActionId::TimelineClipDelete:
                return deleteSelectedTimelineClip();

            case UiActionId::TrackAdd:
                return addAudioTrack();

            case UiActionId::TrackRename:
            case UiActionId::TrackRemove:
            case UiActionId::TrackReorder:
            case UiActionId::TrackDuplicate:
            case UiActionId::TrackMoveUp:
            case UiActionId::TrackMoveDown:
            case UiActionId::TrackToggleMute:
            case UiActionId::TrackToggleSolo:
            case UiActionId::TrackToggleArm:
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "track edit payload required" }, false };
            }

            case UiActionId::EditRenameSelection:
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "rename payload required" }, false };
            }

            case UiActionId::PianoRollNoteAdd:
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "note payload required" }, false };
            }

            case UiActionId::PianoRollNoteDelete:
                return deleteSelectedPianoRollNotes();

            case UiActionId::PianoRollNoteOctaveUp:
                return transposeSelectedPianoRollNotes (12);

            case UiActionId::PianoRollNoteOctaveDown:
                return transposeSelectedPianoRollNotes (-12);

            case UiActionId::PianoRollNoteQuantizeSelection:
                return quantizeSelectedPianoRollNotesToCurrentGrid();

            case UiActionId::ViewPianoRoll:
            {
                UiActionDispatchResult result = registry_.dispatch (id, context_);
                if (result.dispatched && ! context_.midiClipSelected)
                    (void) selectFirstMidiClip();
                return result;
            }

            case UiActionId::TimelineClipMove:
            case UiActionId::TimelineClipTrim:
            case UiActionId::TimelineClipSplit:
            case UiActionId::TimelineClipSetGain:
            case UiActionId::TimelineClipSetFades:
            case UiActionId::TimelineClipTimeStretch:
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "timeline edit payload required" }, false };
            }

            case UiActionId::MixerTargetSetFader:
            case UiActionId::MixerTargetSetPan:
            case UiActionId::MixerTargetToggleMute:
            case UiActionId::MixerTargetToggleSolo:
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "mixer control payload required" }, false };
            }

            case UiActionId::PianoRollNoteSelect:
            case UiActionId::PianoRollNoteMove:
            case UiActionId::PianoRollNoteSetLength:
            case UiActionId::PianoRollNoteTranspose:
            case UiActionId::PianoRollNoteQuantize:
            case UiActionId::PianoRollNoteSetVelocity:
            case UiActionId::PianoRollNoteDuplicate:
            case UiActionId::PianoRollReadExpressionLanes:
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "piano roll payload required" }, false };
            }

            case UiActionId::Count:
                break;
        }

        return { id, { false, "unknown action" }, false };
    }

private:
    enum class MixerTargetKind : std::uint8_t
    {
        Track,
        Bus
    };

    struct MixerTargetSelection
    {
        MixerTargetKind kind = MixerTargetKind::Track;
        std::size_t index = 0;
    };

    [[nodiscard]] UiActionDispatchResult applySelectedTimelineClipGain (UiActionId id, float newGain)
    {
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied =
            nextUndo.apply (nextProject, engine::ProjectEditCommand::setClipGain (selectedTimelineClipId_, newGain));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult applySelectedTimelineClipFades (UiActionId id,
                                                                         engine::Tick fadeIn,
                                                                         engine::Tick fadeOut)
    {
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::setClipFades (selectedTimelineClipId_, fadeIn, fadeOut));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    [[nodiscard]] const engine::Clip* findClip (engine::EntityId clipId) const noexcept
    {
        if (! clipId.isValid())
            return nullptr;

        for (const engine::Clip& clip : project_.clips)
            if (clip.id == clipId)
                return &clip;

        return nullptr;
    }

    [[nodiscard]] const engine::MidiClip* findMidiClip (engine::EntityId midiClipId) const noexcept
    {
        if (! midiClipId.isValid())
            return nullptr;

        for (const engine::MidiClip& midiClip : project_.midiClips)
            if (midiClip.id == midiClipId)
                return &midiClip;

        return nullptr;
    }

    // E8: a timeline selection entry can be an audio Clip or a MIDI clip — one view law for
    // existence, window, and owning track keeps every group gesture kind-aware.
    struct UiTimelineEntityView
    {
        bool valid = false;
        bool isMidi = false;
        engine::Tick timelineStart = 0;
        engine::Tick timelineLength = 0;
        engine::EntityId trackId;
    };

    [[nodiscard]] UiTimelineEntityView timelineEntityView (engine::EntityId entityId) const noexcept
    {
        if (const engine::Clip* const clip = findClip (entityId))
            return { true, false, clip->timelineStart, clip->timelineLength, clip->trackId };
        if (const engine::MidiClip* const midiClip = findMidiClip (entityId))
            return { true, true, midiClip->timelineStart, midiClip->timelineLength, midiClip->trackId };
        return {};
    }

    [[nodiscard]] bool timelineEntityExists (engine::EntityId entityId) const noexcept
    {
        return timelineEntityView (entityId).valid;
    }

    [[nodiscard]] const engine::RecordingTake* findRecordingTake (engine::EntityId takeId) const noexcept
    {
        return project_.findRecordingTake (takeId);
    }

    [[nodiscard]] static const engine::Note* findNote (const engine::MidiClip& midiClip,
                                                       engine::EntityId noteId) noexcept
    {
        if (! noteId.isValid())
            return nullptr;

        for (const engine::Note& note : midiClip.notes)
            if (note.id == noteId)
                return &note;

        return nullptr;
    }

    void clearMixerTargetSelection() noexcept
    {
        selectedMixerTarget_ = {};
        context_.mixerTargetSelected = false;
        if (context_.activePanel == UiPanel::Mixer)
            context_.activePanel = UiPanel::Timeline;
    }

    [[nodiscard]] const engine::Track* findTrack (engine::EntityId trackId) const noexcept
    {
        if (! trackId.isValid())
            return nullptr;

        for (const engine::Track& track : project_.tracks)
            if (track.id == trackId)
                return &track;

        return nullptr;
    }

    [[nodiscard]] static const engine::AutomationLaneData* firstTrackFirstSendAutomationLane (
        const engine::Project& project) noexcept
    {
        if (project.tracks.empty())
            return nullptr;

        const engine::EntityId trackId = project.tracks.front().id;
        for (const engine::AutomationLaneData& lane : project.automationLanes)
        {
            if (lane.ownerEntity == trackId
                && lane.role == engine::AutomationTargetRole::SendLevel
                && lane.paramId == 0u)
            {
                return &lane;
            }
        }

        return nullptr;
    }

    void clearRecordingTrackInput() noexcept
    {
        recordingTrackInput_ = {};
        context_.recordingTrackArmed = false;
        context_.recordingInputSelected = false;
        context_.selectedRecordingTrackIndex = -1;
        context_.selectedRecordingInputChannel = -1;
        context_.isRecording = false;
    }

    static void applyDeterministicTestDeviceProfile (UiRecordingDeviceSelection& device) noexcept
    {
        device.stableDeviceId = 1u;
        device.sampleRate = engine::SampleRate { 48000.0 };
        device.inputChannels = 2u;
        device.maxBlockSize = 128u;
        device.latencyCalibrated = true;
        device.inputLatencyFrames = 40;
        device.outputLatencyFrames = 60;
    }

    [[nodiscard]] static engine::RecordingMonitoringPolicy engineMonitoringPolicyForUi (
        UiRecordingMonitoringPolicy policy) noexcept
    {
        switch (policy)
        {
            case UiRecordingMonitoringPolicy::DirectInput:
                return engine::RecordingMonitoringPolicy::DirectInput;
            case UiRecordingMonitoringPolicy::LatencyCompensated:
                return engine::RecordingMonitoringPolicy::LatencyCompensated;
            case UiRecordingMonitoringPolicy::Off:
            case UiRecordingMonitoringPolicy::Unselected:
                return engine::RecordingMonitoringPolicy::Off;
        }

        return engine::RecordingMonitoringPolicy::Off;
    }

    void syncRecordingContext() noexcept
    {
        context_.recordingDeviceSelected = recordingDevice_.selected;
        context_.recordingDeviceGeneration = recordingDevice_.generation;
        context_.selectedRecordingDeviceId = recordingDevice_.stableDeviceId;
        context_.recordingTrackAvailable = context_.projectLoaded && ! project_.tracks.empty();

        if (! recordingTrackInput_.armed
            || ! context_.recordingTrackAvailable
            || findTrack (recordingTrackInput_.trackId) == nullptr
            || ! recordingDevice_.selected
            || recordingTrackInput_.inputChannel >= recordingDevice_.inputChannels)
        {
            clearRecordingTrackInput();
            return;
        }

        context_.recordingTrackArmed = true;
        context_.recordingInputSelected = true;
        context_.selectedRecordingTrackIndex = static_cast<int> (recordingTrackInput_.trackIndex);
        context_.selectedRecordingInputChannel = static_cast<int> (recordingTrackInput_.inputChannel);
    }

    void syncRecordingCompContext() noexcept
    {
        context_.recordingCompTakesAvailable = project_.recordingTakes.size() >= 2u;
        context_.recordingCompSelected = ! project_.recordingCompSegments.empty();
        context_.recordingCompSegmentCount = static_cast<int> (project_.recordingCompSegments.size());

        recordingCompSelection_ = {};
        recordingCompSelection_.selected = context_.recordingCompSelected;
        recordingCompSelection_.segmentCount = project_.recordingCompSegments.size();

        if (project_.recordingCompSegments.size() >= 2u)
        {
            const engine::ProjectRecordingCompSegment& first = project_.recordingCompSegments[0];
            const engine::ProjectRecordingCompSegment& second = project_.recordingCompSegments[1];
            recordingCompSelection_.firstTakeId = first.takeId;
            recordingCompSelection_.secondTakeId = second.takeId;
            recordingCompSelection_.firstTimelineStart = first.timelineStart;
            recordingCompSelection_.firstTimelineLength = first.timelineLength;
            recordingCompSelection_.secondTimelineStart = second.timelineStart;
            recordingCompSelection_.secondTimelineLength = second.timelineLength;

            const engine::Tick firstEnd = first.timelineStart + first.timelineLength;
            if (second.timelineStart > firstEnd)
            {
                recordingCompSelection_.gapStart = firstEnd;
                recordingCompSelection_.gapLength = second.timelineStart - firstEnd;
            }
        }
    }

    void clearAutosaveRecoveryPrompt() noexcept
    {
        autosaveRecovery_ = {};
        context_.autosaveRecoveryPending = false;
    }

    void setAutosaveRecoveryPrompt (const engine::Project& autosaved)
    {
        autosaveRecovery_ = {
            true,
            bundlePath_,
            autosaved.tracks.size(),
            autosaved.assets.size(),
            autosaved.clips.size(),
            autosaved.recordingTakes.size(),
            autosaved.midiClips.size(),
            autosaved.recordingCompSegments.size()
        };
        context_.autosaveRecoveryPending = true;
        ++context_.autosaveRecoveryPromptCount;
    }

    void detectAutosaveRecoveryPrompt()
    {
        clearAutosaveRecoveryPrompt();
        if (bundlePath_.empty())
            return;

        engine::Project autosaved;
        const persistence::AutosaveResult result = persistence::readAutosaveSnapshot (bundlePath_, autosaved);
        if (! result.ok())
            return;

        setAutosaveRecoveryPrompt (autosaved);
    }

    [[nodiscard]] UiActionDispatchResult restorePendingAutosaveSnapshot()
    {
        const UiActionId id = UiActionId::AutosaveRecoveryRestore;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (! bundleDb_.isOpen() || bundlePath_.empty())
            return { id, { false, "no Project bundle is open" }, false };

        const std::filesystem::path bundlePath = bundlePath_;
        const int previousCommandCount = context_.commandDispatchCount;
        const int previousPromptCount = context_.autosaveRecoveryPromptCount;
        const int previousRestoreCount = context_.autosaveRecoveryRestoreCount;
        const int previousDiscardCount = context_.autosaveRecoveryDiscardCount;

        engine::Project restored;
        const persistence::AutosaveResult restoredResult = persistence::restoreAutosaveSnapshot (bundleDb_, restored);
        if (! restoredResult.ok())
            return { id, { false, "autosave restore failed" }, false };

        const persistence::AutosaveResult discardedResult = persistence::discardAutosaveSnapshot (bundlePath);
        if (! discardedResult.ok())
            return { id, { false, "autosave cleanup failed" }, false };

        persistence::ProjectBundleDb opened = std::move (bundleDb_);
        attachProjectBundle (std::move (opened), bundlePath, std::move (restored));
        context_.commandDispatchCount = previousCommandCount + 1;
        context_.autosaveRecoveryPromptCount = previousPromptCount;
        context_.autosaveRecoveryRestoreCount = previousRestoreCount + 1;
        context_.autosaveRecoveryDiscardCount = previousDiscardCount;
        clearAutosaveRecoveryPrompt();
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult discardPendingAutosaveSnapshot()
    {
        const UiActionId id = UiActionId::AutosaveRecoveryDiscard;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (bundlePath_.empty())
            return { id, { false, "no Project bundle is open" }, false };

        const persistence::AutosaveResult result = persistence::discardAutosaveSnapshot (bundlePath_);
        if (! result.ok())
            return { id, { false, "autosave discard failed" }, false };

        clearAutosaveRecoveryPrompt();
        ++context_.commandDispatchCount;
        ++context_.autosaveRecoveryDiscardCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult refreshAudioDevices()
    {
        const UiActionId id = UiActionId::DeviceRefreshAudio;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        ++recordingDevice_.generation;
        if (recordingDevice_.selected)
        {
            applyDeterministicTestDeviceProfile (recordingDevice_);
        }

        ++context_.commandDispatchCount;
        ++context_.deviceRefreshCount;
        syncRecordingContext();
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult selectTestAudioDevice()
    {
        const UiActionId id = UiActionId::DeviceSelectTestAudio;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        recordingDevice_.selected = true;
        if (recordingDevice_.generation == 0u)
            recordingDevice_.generation = 1u;
        applyDeterministicTestDeviceProfile (recordingDevice_);

        ++context_.commandDispatchCount;
        ++context_.deviceSelectCount;
        syncRecordingContext();
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult toggleDefaultTrackRecordingArm()
    {
        syncRecordingContext();

        const UiActionId id = UiActionId::RecordingArmTrack;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        if (! recordingTrackInput_.armed)
        {
            if (project_.tracks.empty() || ! recordingDevice_.selected || recordingDevice_.inputChannels == 0u)
                return { id, { false, "no armed recording Track/input" }, false };

            recordingTrackInput_.armed = true;
            recordingTrackInput_.trackId = project_.tracks.front().id;
            recordingTrackInput_.trackIndex = 0;
            recordingTrackInput_.inputChannel = 0;
        }
        else
        {
            clearRecordingTrackInput();
        }

        ++context_.commandDispatchCount;
        ++context_.recordingArmCount;
        syncRecordingContext();
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult selectInputMonitoringPolicy()
    {
        syncRecordingContext();

        const UiActionId id = UiActionId::RecordingSetMonitoringPolicy;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        context_.selectedRecordingMonitoringPolicy =
            nextRecordingMonitoringPolicy (context_.selectedRecordingMonitoringPolicy);
        context_.recordingMonitoringSelected =
            context_.selectedRecordingMonitoringPolicy != UiRecordingMonitoringPolicy::Unselected;
        ++context_.commandDispatchCount;
        ++context_.recordingMonitoringCount;
        syncRecordingContext();
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult assembleBasicRecordingCompSelection()
    {
        syncProjectEditContext();

        const UiActionId id = UiActionId::RecordingAssembleComp;
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        const engine::RecordingTake* firstTake = nullptr;
        const engine::RecordingTake* secondTake = nullptr;
        for (const engine::RecordingTake& take : project_.recordingTakes)
        {
            if (recordingTrackInput_.armed && take.trackId != recordingTrackInput_.trackId)
                continue;

            if (firstTake == nullptr)
                firstTake = &take;
            else
            {
                secondTake = &take;
                break;
            }
        }

        if (firstTake == nullptr || secondTake == nullptr)
        {
            firstTake = nullptr;
            secondTake = nullptr;
            for (const engine::RecordingTake& take : project_.recordingTakes)
            {
                if (firstTake == nullptr)
                    firstTake = &take;
                else
                {
                    secondTake = &take;
                    break;
                }
            }
        }

        if (firstTake == nullptr || secondTake == nullptr)
            return { id, { false, "not enough recording Takes" }, false };

        const std::uint64_t shortestTakeFrames = std::min (firstTake->frameCount, secondTake->frameCount);
        if (shortestTakeFrames < 4u || shortestTakeFrames > static_cast<std::uint64_t> (std::numeric_limits<engine::Tick>::max()))
            return { id, { false, "recording Takes are too short" }, false };

        const auto segmentLength = static_cast<engine::Tick> (std::min<std::uint64_t> (96u, shortestTakeFrames / 2u));
        const auto gapLength = static_cast<engine::Tick> (std::max<std::uint64_t> (1u, std::min<std::uint64_t> (64u, shortestTakeFrames / 4u)));
        if (segmentLength <= 0 || gapLength <= 0 || segmentLength > std::numeric_limits<engine::Tick>::max() - gapLength)
            return { id, { false, "recording Comp window is invalid" }, false };

        engine::Project nextProject = project_;
        const engine::EntityId firstSegmentId = allocateSessionEntityId (0xE1u, nextProject);
        const engine::EntityId secondSegmentId = allocateSessionEntityId (0xE2u, nextProject);
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::setRecordingCompSelection (
                firstSegmentId,
                firstTake->id,
                0,
                segmentLength,
                0,
                secondSegmentId,
                secondTake->id,
                segmentLength + gapLength,
                segmentLength,
                0));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "recording Comp selection did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.recordingCompCommandCount;
        syncRecordingCompContext();
        return { id, state, true };
    }

    [[nodiscard]] engine::MixerStripState* selectedMixerStrip (engine::Project& project) const noexcept
    {
        if (! context_.mixerTargetSelected)
            return nullptr;

        if (selectedMixerTarget_.kind == MixerTargetKind::Track)
        {
            if (selectedMixerTarget_.index >= project.tracks.size())
                return nullptr;

            return &project.tracks[selectedMixerTarget_.index].strip;
        }

        if (selectedMixerTarget_.index >= project.buses.size())
            return nullptr;

        return &project.buses[selectedMixerTarget_.index].strip;
    }

    template <typename Fn>
    [[nodiscard]] UiActionDispatchResult editSelectedMixerStrip (UiActionId id,
                                                                 UiActionState state,
                                                                 Fn&& fn)
    {
        engine::Project nextProject = project_;
        engine::MixerStripState* const strip = selectedMixerStrip (nextProject);
        if (strip == nullptr)
            return { id, { false, "selected mixer target missing" }, false };

        fn (*strip);
        if (! nextProject.hasValidAssetClipIndirection())
            return { id, { false, "invalid mixer strip" }, false };

        engine::ProjectUndoStack nextUndo = undo_;
        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "mixer edit did not persist" }, false };

        context_.mixerTargetSelected = true;
        context_.activePanel = UiPanel::Mixer;
        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    // Same persisted strip edit as editSelectedMixerStrip, but addressed by Track id and
    // panel-preserving: neither the active panel nor the mixer target selection changes (B28).
    template <typename Fn>
    [[nodiscard]] UiActionDispatchResult editTrackStripPanelPreserving (UiActionId id,
                                                                        engine::EntityId trackId,
                                                                        Fn&& fn)
    {
        const UiActionState state = registry_.stateFor (id, context_);
        if (! state.enabled)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::MixerStripState* strip = nullptr;
        for (engine::Track& track : nextProject.tracks)
            if (track.id == trackId)
                strip = &track.strip;
        if (strip == nullptr)
            return { id, { false, "selected track missing" }, false };

        fn (*strip);
        if (! nextProject.hasValidAssetClipIndirection())
            return { id, { false, "invalid strip edit" }, false };

        engine::ProjectUndoStack nextUndo = undo_;
        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "strip edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.mixerEditCount;
        return { id, state, true };
    }

    [[nodiscard]] static std::optional<std::uint64_t> sourceLengthForSplit (
        const engine::Clip& clip,
        engine::Tick leftTimelineLength) noexcept
    {
        if (clip.timelineLength <= 0
            || leftTimelineLength <= 0
            || leftTimelineLength >= clip.timelineLength
            || clip.srcLen <= 1u)
        {
            return std::nullopt;
        }

        const long double scaled =
            (static_cast<long double> (leftTimelineLength) * static_cast<long double> (clip.srcLen))
            / static_cast<long double> (clip.timelineLength);
        if (scaled <= 0.0L || scaled >= static_cast<long double> (clip.srcLen))
            return std::nullopt;

        const auto sourceLength = static_cast<std::uint64_t> (scaled + 0.5L);
        if (sourceLength == 0u || sourceLength >= clip.srcLen)
            return std::nullopt;

        return sourceLength;
    }

    [[nodiscard]] static std::optional<std::uint64_t> sourceLengthForShortenedRightEdge (
        const engine::Clip& clip,
        engine::Tick nextTimelineLength) noexcept
    {
        return sourceLengthForSplit (clip, nextTimelineLength);
    }

    void syncProjectEditContext() noexcept
    {
        static_assert (engine::Project::kLocatePointCount == kTransportLocatePointCount);
        context_.projectLoaded = project_.hasValidAssetClipIndirection();
        context_.canUndo = undo_.canUndo();
        context_.canRedo = undo_.canRedo();
        for (std::size_t index = 0; index < project_.locatePoints.size(); ++index)
            context_.locatePoints[index] = project_.locatePoints[index];
        std::erase_if (selectedTimelineClipIds_, [this] (engine::EntityId clipId) {
            return ! timelineEntityExists (clipId);
        });
        if (selectedTimelineClipIds_.empty())
            selectedTimelineClipId_ = {};
        else if (! isTimelineClipSelected (selectedTimelineClipId_))
            selectedTimelineClipId_ = selectedTimelineClipIds_.back();
        context_.timelineClipSelected = ! selectedTimelineClipIds_.empty();

        const engine::MidiClip* const midiClip = context_.projectLoaded ? findMidiClip (selectedMidiClipId_) : nullptr;
        if (midiClip == nullptr)
            selectedMidiNoteId_ = {};

        // Multi-note selection (B34) follows the same pruning law as timeline clips: dead notes
        // fall out, and losing the primary drops the whole selection.
        std::erase_if (selectedMidiNoteIds_, [midiClip] (engine::EntityId noteId) {
            return midiClip == nullptr || findNote (*midiClip, noteId) == nullptr;
        });
        if (! selectedMidiNoteId_.isValid()
            || midiClip == nullptr
            || findNote (*midiClip, selectedMidiNoteId_) == nullptr)
            selectedMidiNoteIds_.clear();

        context_.midiClipSelected = midiClip != nullptr;
        context_.midiNoteSelected = midiClip != nullptr
            && selectedMidiNoteId_.isValid()
            && findNote (*midiClip, selectedMidiNoteId_) != nullptr;
        syncRecordingContext();
        syncRecordingCompContext();
        refreshSnapGrid();
    }

    [[nodiscard]] UiActionDispatchResult editSelectedMidiNote (UiActionId id,
                                                               UiActionState state,
                                                               const engine::ProjectEditCommand& command)
    {
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (nextProject, command);

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "piano roll edit did not persist" }, false };

        context_.activePanel = UiPanel::PianoRoll;
        ++context_.commandDispatchCount;
        ++context_.midiEditCount;
        return { id, state, true };
    }

    [[nodiscard]] bool adoptEditedProject (engine::Project nextProject,
                                           engine::ProjectUndoStack nextUndo)
    {
        std::vector<engine::DecodedAssetAudio> decodedViews = makeDecodedViews (decodedAssets_);
        engine::PlaybackEngine::Result built = engine::PlaybackEngine::create (
            nextProject,
            std::span<const engine::DecodedAssetAudio> (decodedViews.data(), decodedViews.size()),
            playbackBuildOptions());

        std::unique_ptr<engine::PlaybackEngine> nextEngine;
        if (built.ok())
        {
            nextEngine = std::move (built.engine);
        }
        else if (built.status == engine::OfflineRenderStatus::EmptyTimeline)
        {
            // ADR-0041: an edit that empties the timeline (deleting the last Clip) still leaves the
            // Project with a real transport rendering exact silence.
            nextEngine = engine::PlaybackEngine::createTransportOnly (nextProject.sampleRate,
                                                                      playbackMaxBlockSize_);
        }

        if (nextEngine == nullptr)
            return false;

        if (bundleDb_.isOpen())
        {
            persistence::BundleResult written = bundleDb_.writeProjectSnapshot (nextProject);
            if (! written.ok())
                return false;
        }

        (void) nextEngine->stop();
        drainTransport (*nextEngine);

        project_ = std::move (nextProject);
        undo_ = std::move (nextUndo);
        decodedAssetViews_ = makeDecodedViews (decodedAssets_);
        replacePlayback (std::move (nextEngine));
        ++editSerial_;
        syncProjectEditContext();
        resetContextForFreshPlayback();
        return true;
    }

    [[nodiscard]] bool canAdoptEditWithoutPlaybackRebuild (const engine::Project& nextProject) const noexcept
    {
        // MIDI clips are renderable content too (ADR-0043): an edit touching a project that has or
        // gains MIDI must rebuild playback, or penciled notes would stay silent.
        return decodedAssets_.empty()
            && project_.clips.empty()
            && nextProject.clips.empty()
            && project_.midiClips.empty()
            && nextProject.midiClips.empty();
    }

    [[nodiscard]] bool adoptEditedProjectWithoutPlaybackRebuild (
        engine::Project nextProject,
        engine::ProjectUndoStack nextUndo)
    {
        if (bundleDb_.isOpen())
        {
            persistence::BundleResult written = bundleDb_.writeProjectSnapshot (nextProject);
            if (! written.ok())
                return false;
        }

        project_ = std::move (nextProject);
        undo_ = std::move (nextUndo);
        std::unique_ptr<engine::PlaybackEngine> transport =
            engine::PlaybackEngine::createTransportOnly (project_.sampleRate, playbackMaxBlockSize_);
        if (transport != nullptr)
        {
            (void) transport->stop();
            drainTransport (*transport);
        }
        replacePlayback (std::move (transport));
        ++editSerial_;
        syncProjectEditContext();
        resetContextForFreshPlayback();
        return true;
    }

    [[nodiscard]] UiActionDispatchResult dispatchUndo (UiActionId id, UiActionState state)
    {
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectUndoStatus undoStatus = nextUndo.undo (nextProject);
        if (undoStatus != engine::ProjectUndoStatus::Applied)
            return { id, state, false };

        if (canAdoptEditWithoutPlaybackRebuild (nextProject))
        {
            if (! adoptEditedProjectWithoutPlaybackRebuild (std::move (nextProject), std::move (nextUndo)))
                return { id, { false, "undo did not persist" }, false };
        }
        else if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
        {
            return { id, { false, "undo did not persist" }, false };
        }

        ++context_.commandDispatchCount;
        ++context_.undoCount;
        return { id, state, true };
    }

    [[nodiscard]] UiActionDispatchResult dispatchRedo (UiActionId id, UiActionState state)
    {
        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectUndoStatus undoStatus = nextUndo.redo (nextProject);
        if (undoStatus != engine::ProjectUndoStatus::Applied)
            return { id, state, false };

        if (canAdoptEditWithoutPlaybackRebuild (nextProject))
        {
            if (! adoptEditedProjectWithoutPlaybackRebuild (std::move (nextProject), std::move (nextUndo)))
                return { id, { false, "redo did not persist" }, false };
        }
        else if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
        {
            return { id, { false, "redo did not persist" }, false };
        }

        ++context_.commandDispatchCount;
        ++context_.redoCount;
        return { id, state, true };
    }

    [[nodiscard]] static bool decodedAudioIsValid (const UiDecodedAsset& decoded) noexcept
    {
        if (! decoded.sampleRate.isValid() || decoded.frames == 0 || decoded.channels == 0)
            return false;

        if (decoded.frames > static_cast<std::uint64_t> (std::numeric_limits<engine::Tick>::max()))
            return false;

        if (decoded.frames > std::numeric_limits<std::uint64_t>::max() / decoded.channels)
            return false;

        const std::uint64_t expectedSamples = decoded.frames * decoded.channels;
        return expectedSamples <= static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max())
            && decoded.interleavedSamples.size() == static_cast<std::size_t> (expectedSamples);
    }

    [[nodiscard]] UiDecodedAsset makeDeterministicRecordedAudio() const
    {
        constexpr std::uint64_t kFrames = 256;

        UiDecodedAsset decoded;
        decoded.sampleRate = recordingDevice_.sampleRate.isValid()
            ? recordingDevice_.sampleRate
            : engine::SampleRate { 48000.0 };
        decoded.frames = kFrames;
        decoded.channels = 1;
        decoded.interleavedSamples.reserve (static_cast<std::size_t> (kFrames));

        for (std::uint64_t frame = 0; frame < kFrames; ++frame)
        {
            const int phase = static_cast<int> (frame % 16u);
            decoded.interleavedSamples.push_back ((static_cast<float> (phase) - 7.5f) / 16.0f);
        }

        return decoded;
    }

    static std::vector<engine::DecodedAssetAudio> makeDecodedViews (const std::vector<UiDecodedAsset>& decodedAssets)
    {
        std::vector<engine::DecodedAssetAudio> views;
        views.reserve (decodedAssets.size());

        for (const UiDecodedAsset& asset : decodedAssets)
        {
            views.push_back (engine::DecodedAssetAudio {
                asset.assetId,
                asset.sampleRate,
                asset.frames,
                asset.channels,
                std::span<const float> (asset.interleavedSamples.data(), asset.interleavedSamples.size())
            });
        }

        return views;
    }

    static void drainTransport (engine::PlaybackEngine& playback) noexcept
    {
        playback.processBlock (nullptr, 0, 0);
    }

    [[nodiscard]] static engine::EntityId allocateDefaultProjectId()
    {
        engine::UlidEntropy entropy {};
        entropy[0] = 0x59u; // "YESDAW" seed prefix, enough entropy for one allocation per new Project.
        entropy[1] = 0x45u;
        entropy[2] = 0x53u;
        entropy[3] = 0x44u;
        entropy[4] = 0x41u;
        entropy[5] = 0x57u;

        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds> (now).count();
        const auto timestamp = millis > 0 ? static_cast<std::uint64_t> (millis) : std::uint64_t { 0 };

        engine::EntityIdAllocator allocator (entropy);
        engine::EntityId id = allocator.allocate (timestamp);
        if (id.isValid())
            return id;

        return engine::EntityId::fromBigEndianParts (0x5945534441570000ull, 1ull);
    }

    // Delegates to the engine's authoritative scan. A private near-copy used to live here and had
    // drifted stale — it never scanned FX-insert or automation-lane ids, so two same-millisecond FX
    // adds could collide and the second was silently rejected as DuplicateEntityId.
    [[nodiscard]] static bool projectContainsEntityId (const engine::Project& project,
                                                       engine::EntityId id) noexcept
    {
        return engine::detail::projectContainsEntityId (project, id);
    }

    [[nodiscard]] engine::EntityId allocateSessionEntityId (std::uint8_t seedByte) const
    {
        return allocateSessionEntityId (seedByte, project_);
    }

    [[nodiscard]] engine::EntityId allocateSessionEntityId (std::uint8_t seedByte,
                                                            const engine::Project& project) const
    {
        engine::UlidEntropy entropy {};
        entropy[0] = 0x59u;
        entropy[1] = 0x44u;
        entropy[2] = 0x49u;
        entropy[3] = seedByte;
        entropy[4] = static_cast<std::uint8_t> (project.assets.size() & 0xffu);
        entropy[5] = static_cast<std::uint8_t> (project.clips.size() & 0xffu);

        const auto now = std::chrono::system_clock::now().time_since_epoch();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds> (now).count();
        const auto timestamp = millis > 0 ? static_cast<std::uint64_t> (millis) : std::uint64_t { 0 };

        engine::EntityIdAllocator allocator (entropy);
        for (std::uint64_t attempt = 0; attempt < 256; ++attempt)
        {
            engine::EntityId id = allocator.allocate (timestamp + attempt);
            if (id.isValid() && ! projectContainsEntityId (project, id))
                return id;
        }

        return engine::EntityId::fromBigEndianParts (0x5944490000000000ull | static_cast<std::uint64_t> (seedByte),
                                                     static_cast<std::uint64_t> (project.assets.size() + project.clips.size() + 1u));
    }

    [[nodiscard]] std::uint32_t nextRecordingTakeOrdinal (engine::EntityId trackId) const noexcept
    {
        std::uint32_t ordinal = 0;
        for (const engine::RecordingTake& take : project_.recordingTakes)
        {
            if (take.trackId != trackId || take.takeOrdinal < ordinal)
                continue;

            if (take.takeOrdinal == std::numeric_limits<std::uint32_t>::max())
                return take.takeOrdinal;

            ordinal = take.takeOrdinal + 1u;
        }

        return ordinal;
    }

    [[nodiscard]] static engine::Track makeDefaultAudioTrack (engine::EntityId id = engine::kDefaultAudioTrackId)
    {
        engine::Track track;
        track.id = id;
        track.strip.name = "Audio 1";
        return track;
    }

    [[nodiscard]] engine::Track& ensureDefaultAudioTrack (engine::Project& project) const
    {
        for (engine::Track& track : project.tracks)
            if (track.id == engine::kDefaultAudioTrackId || track.strip.name == "Audio 1")
                return track;

        engine::EntityId id = engine::kDefaultAudioTrackId;
        if (projectContainsEntityId (project, id))
            id = allocateSessionEntityId (0x54u, project);

        project.tracks.push_back (makeDefaultAudioTrack (id));
        return project.tracks.back();
    }

    [[nodiscard]] static engine::Tick timelineEnd (const engine::Project& project) noexcept
    {
        // One definition, shared with the canonical recorded-audio commit service (U4).
        return app::projectTimelineEnd (project);
    }

    [[nodiscard]] static const engine::AutomationLaneData* firstTrackFaderAutomationLane (
        const engine::Project& project) noexcept
    {
        if (project.tracks.empty())
            return nullptr;

        const engine::EntityId trackId = project.tracks.front().id;
        for (const engine::AutomationLaneData& lane : project.automationLanes)
        {
            if (lane.ownerEntity == trackId
                && lane.role == engine::AutomationTargetRole::TrackFader
                && lane.paramId == engine::FaderNode::kGainParameterId)
            {
                return &lane;
            }
        }

        return nullptr;
    }

    static void upsertDecodedAsset (std::vector<UiDecodedAsset>& decodedAssets,
                                    UiDecodedAsset decoded)
    {
        for (UiDecodedAsset& existing : decodedAssets)
        {
            if (existing.assetId == decoded.assetId)
            {
                existing = std::move (decoded);
                return;
            }
        }

        decodedAssets.push_back (std::move (decoded));
    }

    void enqueueWaveformBuildsForDecodedAssets()
    {
        for (const UiDecodedAsset& decoded : decodedAssets_)
        {
            if (! decodedAudioIsValid (decoded))
                continue;

            const engine::Asset* const asset = project_.findAsset (decoded.assetId);
            if (asset == nullptr)
                continue;

            waveformService_.requestBuild (
                *asset,
                interleavedToChannelMajor (
                    std::span<const float> (decoded.interleavedSamples.data(),
                                            decoded.interleavedSamples.size()),
                    decoded.frames,
                    decoded.channels));
        }
    }

    void writeLastProjectRecord()
    {
        if (sessionStateDirectory_.empty() || bundlePath_.empty())
            return;

        std::error_code directoryError;
        std::filesystem::create_directories (sessionStateDirectory_, directoryError);
        std::ofstream output (sessionStateDirectory_ / kLastProjectRecordFileName,
                              std::ios::binary | std::ios::trunc);
        if (! output.good())
            return;

        const std::u8string utf8 = bundlePath_.u8string();
        output.write (reinterpret_cast<const char*> (utf8.data()),
                      static_cast<std::streamsize> (utf8.size()));

        // MRU update (B39): the current bundle moves to the top; the list keeps at most five.
        std::vector<std::filesystem::path> recents = recentProjectBundles();
        std::erase (recents, bundlePath_);
        recents.insert (recents.begin(), bundlePath_);
        if (recents.size() > kRecentProjectsLimit)
            recents.resize (kRecentProjectsLimit);

        std::ofstream recentOutput (sessionStateDirectory_ / kRecentProjectsRecordFileName,
                                    std::ios::binary | std::ios::trunc);
        if (! recentOutput.good())
            return;

        for (const std::filesystem::path& recent : recents)
        {
            const std::u8string recentUtf8 = recent.u8string();
            recentOutput.write (reinterpret_cast<const char*> (recentUtf8.data()),
                                static_cast<std::streamsize> (recentUtf8.size()));
            recentOutput.put ('\n');
        }
    }

    void attachProjectBundle (
        persistence::ProjectBundleDb opened,
        const std::filesystem::path& bundlePath,
        engine::Project project)
    {
        bundleDb_ = std::move (opened);
        bundlePath_ = bundlePath;
        writeLastProjectRecord();
        project_ = std::move (project);
        editSerial_ = 0;
        lastSavedEditSerial_ = 0;   // a freshly attached bundle starts clean (B37)
        waveformService_.start (bundlePath_);
        selectedTimelineClipIds_.clear();
        selectedTimelineClipId_ = {};
        selectedMidiClipId_ = {};
        selectedMidiNoteId_ = {};
        selectedMixerTarget_ = {};
        undo_ = {};
        decodedAssets_.clear();
        decodedAssetViews_.clear();
        std::unique_ptr<engine::PlaybackEngine> transport =
            engine::PlaybackEngine::createTransportOnly (project_.sampleRate, playbackMaxBlockSize_);
        if (transport != nullptr)
        {
            (void) transport->stop();
            drainTransport (*transport);
        }
        replacePlayback (std::move (transport));

        context_ = {};
        context_.projectLoaded = true;
        context_.activePanel = UiPanel::Timeline;
        timelineRangeStartFrame_ = -1;
        timelineRangeEndFrame_ = -1;
        recordingDevice_ = {};
        recordingTrackInput_ = {};
        lastRecordedAudioTake_ = {};
        lastRecordedMidiTake_ = {};
        pendingAudioPlacement_ = {};
        pendingMidiPlacement_ = {};
        recordingCompSelection_ = {};
        autosaveRecovery_ = {};
        syncProjectEditContext();
    }

    template <typename Fn>
    UiActionDispatchResult dispatchTransport (UiActionId id, Fn&& fn)
    {
        if (! fn())
            return { id, { true, "" }, false };

        drainTransport (*playback_);
        syncContextFromPlayback();
        ++context_.commandDispatchCount;
        return { id, { true, "" }, true };
    }

    [[nodiscard]] bool stopPlaybackAtConfiguredPosition()
    {
        if (playback_ == nullptr || ! playback_->stop())
            return false;

        return ! context_.returnToStartOnStopEnabled
            || playback_->locate (context_.playbackStartFrame);
    }

    UiActionDispatchResult locatePlaybackRelative (UiActionId id, std::int64_t deltaFrames)
    {
        const std::int64_t currentFrame = std::max<std::int64_t> (0, context_.playheadFrame);
        std::int64_t targetFrame = 0;
        if (deltaFrames < 0)
        {
            const std::int64_t magnitude = -deltaFrames;
            targetFrame = currentFrame > magnitude ? currentFrame - magnitude : 0;
        }
        else
        {
            if (currentFrame > std::numeric_limits<std::int64_t>::max() - deltaFrames)
                return { id, { true, "" }, false };
            targetFrame = currentFrame + deltaFrames;
        }

        return locatePlaybackAbsolute (id, targetFrame);
    }

    UiActionDispatchResult storeLocatePoint (UiActionId id, UiActionState state)
    {
        const int index = storeLocatePointIndex (id);
        if (index < 0)
            return { id, { false, "unknown locate point" }, false };

        engine::Project nextProject = project_;
        nextProject.locatePoints[static_cast<std::size_t> (index)] =
            static_cast<engine::Tick> (std::max<std::int64_t> (0, context_.playheadFrame));

        if (bundleDb_.isOpen())
        {
            const persistence::BundleResult written = bundleDb_.writeProjectSnapshot (nextProject);
            if (! written.ok())
                return { id, { false, "locate point did not persist" }, false };
        }

        project_ = std::move (nextProject);
        context_.locatePoints[static_cast<std::size_t> (index)] = project_.locatePoints[static_cast<std::size_t> (index)];
        context_.activePanel = UiPanel::Timeline;
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    UiActionDispatchResult convertTimelineRangeToLoop (UiActionId id, UiActionState state)
    {
        if (playback_ == nullptr
            || timelineRangeStartFrame_ < 0
            || timelineRangeEndFrame_ <= timelineRangeStartFrame_)
            return { id, { false, "no ruler range selection" }, false };

        if (! playback_->setLoop (timelineRangeStartFrame_, timelineRangeEndFrame_))
            return { id, { false, "loop region rejected" }, false };

        drainTransport (*playback_);
        syncContextFromPlayback();
        context_.loopEnabled = true;
        ++context_.commandDispatchCount;
        return { id, state, true };
    }

    UiActionDispatchResult recallLocatePoint (UiActionId id, UiActionState state)
    {
        const int index = recallLocatePointIndex (id);
        if (index < 0)
            return { id, { false, "unknown locate point" }, false };

        const std::optional<engine::Tick>& point = project_.locatePoints[static_cast<std::size_t> (index)];
        if (! point.has_value())
            return { id, state, false };

        return locatePlaybackAbsolute (id, *point);
    }

    UiActionDispatchResult locateAdjacentTimelineMarker (UiActionId id, bool next)
    {
        const std::int64_t currentFrame = std::max<std::int64_t> (0, context_.playheadFrame);
        if (next)
        {
            for (const engine::Marker& marker : project_.markers)
                if (marker.tick > currentFrame)
                    return locatePlaybackAbsolute (id, marker.tick);
        }
        else
        {
            for (auto marker = project_.markers.rbegin(); marker != project_.markers.rend(); ++marker)
                if (marker->tick < currentFrame)
                    return locatePlaybackAbsolute (id, marker->tick);
        }

        return { id, { false, next ? "no next marker" : "no previous marker" }, false };
    }

    UiActionDispatchResult locatePlaybackAbsolute (UiActionId id, std::int64_t targetFrame)
    {
        UiActionDispatchResult result = dispatchTransport (id, [this, targetFrame] {
            return playback_ != nullptr && playback_->locate (targetFrame);
        });
        if (result.dispatched)
            context_.lastLocateFrame = context_.playheadFrame;
        return result;
    }

    [[nodiscard]] std::int64_t snapFramesForUnit (UiSnapUnit unit) const noexcept
    {
        const double quarterNoteFrames = headQuarterNoteFrames();
        double gridFrames = headMeterBeatFrames();
        if (unit == UiSnapUnit::Bar)
            gridFrames = headBarFramesExact();
        else if (unit == UiSnapUnit::Sixteenth)
            gridFrames = quarterNoteFrames / 4.0;

        return std::max<std::int64_t> (1, static_cast<std::int64_t> (gridFrames + 0.5));
    }

    [[nodiscard]] double headQuarterNoteFrames() const noexcept
    {
        const double sampleRateHz = project_.sampleRate.isValid() ? project_.sampleRate.hz : 48000.0;
        const double bpm = ! project_.tempoMap.empty() ? project_.tempoMap.front().bpm : 120.0;
        return sampleRateHz * 60.0 / std::clamp (bpm, 20.0, 400.0);
    }

    [[nodiscard]] double headMeterBeatFrames() const noexcept
    {
        const double denominator = ! project_.meterMap.empty()
            ? static_cast<double> (project_.meterMap.front().denominator)
            : 4.0;
        return headQuarterNoteFrames() * 4.0 / std::clamp (denominator, 1.0, 64.0);
    }

    [[nodiscard]] double headBarFramesExact() const noexcept
    {
        const double numerator = ! project_.meterMap.empty()
            ? static_cast<double> (project_.meterMap.front().numerator)
            : 4.0;
        return headMeterBeatFrames() * std::clamp (numerator, 1.0, 32.0);
    }

    [[nodiscard]] std::int64_t headBarFrames() const noexcept
    {
        return std::max<std::int64_t> (
            1, static_cast<std::int64_t> (headBarFramesExact() + 0.5));
    }

    [[nodiscard]] std::optional<std::int64_t> nextRecordCountInEndFrame() noexcept
    {
        syncContextFromPlayback();
        const std::int64_t duration = headBarFrames();
        if (context_.playheadFrame < 0
            || context_.playheadFrame > std::numeric_limits<std::int64_t>::max() - duration)
            return std::nullopt;
        return context_.playheadFrame + duration;
    }

    [[nodiscard]] bool beginRecordingCountIn (std::int64_t endFrame, bool deterministic)
    {
        if (playback_ == nullptr || endFrame <= context_.playheadFrame)
            return false;

        recordCountInEndFrame_ = endFrame;
        deterministicRecordCountInPending_ = deterministic;
        context_.recordCountInActive = true;
        context_.isRecording = false;
        applyMetronomeToPlayback (true);
        if (! playback_->play())
        {
            context_.recordCountInActive = false;
            deterministicRecordCountInPending_ = false;
            applyMetronomeToPlayback();
            return false;
        }

        drainTransport (*playback_);
        syncContextFromPlayback();
        return true;
    }

    void cancelRecordingCountIn()
    {
        captureActive_.store (false, std::memory_order_release);
        drainRealRecordingCapture();
        captureInterleaved_.clear();
        captureTimelineStartFrame_ = -1;
        context_.recordCountInActive = false;
        deterministicRecordCountInPending_ = false;
        if (playback_ != nullptr)
        {
            (void) stopPlaybackAtConfiguredPosition();
            drainTransport (*playback_);
            syncContextFromPlayback();
        }
        context_.isRecording = false;
        applyMetronomeToPlayback();
    }

    void syncContextFromPlayback() noexcept
    {
        if (playback_ == nullptr)
            return;

        context_.isPlaying = playback_->isPlaying();
        context_.loopEnabled = playback_->loopEnabled();
        context_.playheadFrame = playback_->playheadFrame();
        context_.shuttlePlaybackRate = playback_->playbackRate();
    }

    void resetContextForFreshPlayback() noexcept
    {
        context_.isPlaying = false;
        context_.loopEnabled = false;
        context_.playheadFrame = 0;
        context_.playbackStartFrame = 0;
        context_.lastLocateFrame = 0;
        context_.recordCountInActive = false;
        recordCountInEndFrame_ = 0;
        deterministicRecordCountInPending_ = false;
        context_.shuttlePlaybackRate = 1;
    }

    [[nodiscard]] engine::OfflineRenderOptions playbackBuildOptions() const
    {
        engine::OfflineRenderOptions options;
        options.maxBlockSize = playbackMaxBlockSize_;
        return options;
    }

    void replacePlayback (std::unique_ptr<engine::PlaybackEngine> replacement)
    {
        if (playbackReplacementWillBegin_)
            playbackReplacementWillBegin_();

        audioPlayback_.store (nullptr, std::memory_order_release);
        playback_ = std::move (replacement);
        audioPlayback_.store (playback_.get(), std::memory_order_release);
        applyMetronomeToPlayback();

        if (playbackReplacementDidEnd_)
            playbackReplacementDidEnd_();
    }

    void applyMetronomeToPlayback (bool forceEnabled = false) noexcept
    {
        if (playback_ == nullptr)
            return;

        const double bpm = ! project_.tempoMap.empty() ? project_.tempoMap.front().bpm : 120.0;
        const int beatsPerBar = ! project_.meterMap.empty()
            ? static_cast<int> (project_.meterMap.front().numerator)
            : 4;
        const int beatDenominator = ! project_.meterMap.empty()
            ? static_cast<int> (project_.meterMap.front().denominator)
            : 4;
        playback_->setMetronome (
            forceEnabled || context_.metronomeEnabled, bpm, beatsPerBar, beatDenominator);
    }

    static void zeroAudioOutputs (float* const* outputChannels,
                                  int numOutputChannels,
                                  int numFrames) noexcept YESDAW_RT_HOT
    {
        if (outputChannels == nullptr || numOutputChannels <= 0 || numFrames <= 0)
            return;

        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannels[channel] != nullptr)
                std::fill_n (outputChannels[channel], numFrames, 0.0f);
    }

    UiActionRegistry registry_;
    UiActionContext context_;
    persistence::ProjectBundleDb bundleDb_;
    std::filesystem::path bundlePath_;
    engine::Project project_;
    engine::ProjectUndoStack undo_;
    std::vector<engine::EntityId> selectedTimelineClipIds_;
    engine::EntityId selectedTimelineClipId_;
    engine::EntityId selectedMidiClipId_;
    engine::EntityId selectedMidiNoteId_;
    std::vector<engine::EntityId> selectedMidiNoteIds_;   // multi-note selection (B34)
    // Edits since the last explicit Save (B37): every project mutation bumps the serial; Save and
    // a fresh bundle attach mark the session clean. Data is never at risk — the bundle persists
    // every edit synchronously — this tracks the user's saved-state intent only.
    std::uint64_t editSerial_ = 0;
    std::uint64_t lastSavedEditSerial_ = 0;
    MixerTargetSelection selectedMixerTarget_ {};
    UiRecordingDeviceSelection recordingDevice_;
    UiRecordingTrackInputSelection recordingTrackInput_;
    UiRecordedAudioTake lastRecordedAudioTake_;
    UiRecordedAudioTake pendingAudioPlacement_;
    UiRecordedMidiTake lastRecordedMidiTake_;
    UiRecordedMidiTake pendingMidiPlacement_;
    UiRecordingCompSelection recordingCompSelection_;
    std::int64_t recordCountInEndFrame_ = 0;
    bool deterministicRecordCountInPending_ = false;
    UiAutosaveRecoveryPrompt autosaveRecovery_;
    AutosaveSchedulePolicy autosaveSchedule_ {};
    struct UiClipClipboardEntry
    {
        engine::EntityId assetId;
        engine::EntityId trackId;
        engine::Tick timelineOffset = 0;
        engine::Tick timelineLength = 0;
        std::uint64_t srcOffset = 0;
        std::uint64_t srcLen = 0;
        float gain = 1.0f;
        engine::Tick fadeIn = 0;
        engine::Tick fadeOut = 0;
        engine::TimeBase timeBase = engine::TimeBase::SampleLocked;
        engine::ClipName name;
    };
    struct UiClipClipboard
    {
        std::vector<UiClipClipboardEntry> clips;
    };
    UiClipClipboard clipClipboard_;
    std::filesystem::path sessionStateDirectory_;
    UiSnapUnit snapUnit_ = UiSnapUnit::Beat;
    int repeatPasteCount_ = kDefaultRepeatPasteCount;
    static constexpr const char* kLastProjectRecordFileName = "last-project.txt";
    static constexpr const char* kRecentProjectsRecordFileName = "recent-projects.txt";

    [[nodiscard]] static UiClipClipboardEntry clipboardEntryForClip (const engine::Clip& clip,
                                                                     engine::Tick anchorStart) noexcept
    {
        return {
            clip.assetId,
            clip.trackId,
            clip.timelineStart - anchorStart,
            clip.timelineLength,
            clip.srcOffset,
            clip.srcLen,
            clip.gain,
            clip.fadeIn,
            clip.fadeOut,
            clip.timeBase,
            clip.name,
        };
    }

    [[nodiscard]] UiClipClipboard makeClipboardForSelection() const
    {
        UiClipClipboard clipboard;
        engine::Tick anchorStart = std::numeric_limits<engine::Tick>::max();
        for (engine::EntityId clipId : selectedTimelineClipIds_)
            if (const engine::Clip* const clip = findClip (clipId))
                anchorStart = std::min (anchorStart, clip->timelineStart);

        if (anchorStart == std::numeric_limits<engine::Tick>::max())
            return clipboard;

        clipboard.clips.reserve (selectedTimelineClipIds_.size());
        for (engine::EntityId clipId : selectedTimelineClipIds_)
            if (const engine::Clip* const clip = findClip (clipId))
                clipboard.clips.push_back (clipboardEntryForClip (*clip, anchorStart));
        return clipboard;
    }

    [[nodiscard]] UiActionDispatchResult addClipsFromClipboard (UiActionId id,
                                                                const UiActionState& state,
                                                                engine::EntityId targetTrackId,
                                                                engine::Tick timelineStart,
                                                                int repeatCount)
    {
        if (clipClipboard_.clips.empty()
            || repeatCount < 1
            || timelineStart < 0)
            return { id, state, false };

        engine::Tick clipboardSpan = 0;
        constexpr engine::Tick tickMax = std::numeric_limits<engine::Tick>::max();
        for (const UiClipClipboardEntry& entry : clipClipboard_.clips)
        {
            if (entry.timelineOffset < 0
                || entry.timelineLength <= 0
                || entry.timelineOffset > tickMax - entry.timelineLength)
                return { id, state, false };
            clipboardSpan = std::max (clipboardSpan, entry.timelineOffset + entry.timelineLength);
        }
        const engine::Tick repeatCountTicks = static_cast<engine::Tick> (repeatCount);
        if (clipboardSpan <= 0 || clipboardSpan > (tickMax - timelineStart) / repeatCountTicks)
            return { id, state, false };

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        if (! nextUndo.beginTransactionGroup())
            return { id, state, false };

        std::vector<engine::EntityId> pastedClipIds;
        const std::size_t repeatCountSize = static_cast<std::size_t> (repeatCount);
        if (clipClipboard_.clips.size() > pastedClipIds.max_size() / repeatCountSize)
            return { id, state, false };
        pastedClipIds.reserve (clipClipboard_.clips.size() * repeatCountSize);
        for (int repeatIndex = 0; repeatIndex < repeatCount; ++repeatIndex)
        {
            const engine::Tick repeatOffset = clipboardSpan * static_cast<engine::Tick> (repeatIndex);
            for (const UiClipClipboardEntry& entry : clipClipboard_.clips)
            {
                const bool sourceTrackExists = std::any_of (
                    project_.tracks.begin(), project_.tracks.end(), [&entry] (const engine::Track& track) {
                        return track.id == entry.trackId;
                    });
                engine::Clip clip;
                clip.id = allocateSessionEntityId (0xC3u, nextProject);
                clip.assetId = entry.assetId;
                clip.trackId = clipClipboard_.clips.size() > 1u && sourceTrackExists
                    ? entry.trackId
                    : targetTrackId;
                clip.timelineStart = timelineStart + repeatOffset + entry.timelineOffset;
                clip.timelineLength = entry.timelineLength;
                clip.srcOffset = entry.srcOffset;
                clip.srcLen = entry.srcLen;
                clip.gain = entry.gain;
                clip.fadeIn = entry.fadeIn;
                clip.fadeOut = entry.fadeOut;
                clip.timeBase = entry.timeBase;
                clip.name = entry.name;

                if (! nextUndo.apply (nextProject, engine::ProjectEditCommand::addClip (clip)).applied())
                    return { id, state, false };
                pastedClipIds.push_back (clip.id);
            }
        }
        if (! nextUndo.endTransactionGroup())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        selectedTimelineClipIds_ = std::move (pastedClipIds);
        selectedTimelineClipId_ = selectedTimelineClipIds_.back();
        context_.timelineClipSelected = true;
        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
    }

    std::vector<UiDecodedAsset> decodedAssets_;
    // RT capture session state (P0-1). Config is published before the active flag; the FIFO is
    // drained on the shell's control timer, never the audio thread.
    engine::RecordingChunkFifo captureFifo_;
    engine::RecordingConfig captureConfig_ {};
    std::atomic<bool> captureActive_ { false };
    std::vector<float> captureInterleaved_;
    std::int64_t captureTimelineStartFrame_ = -1;
    std::uint16_t captureChannels_ = 0;
    std::vector<engine::DecodedAssetAudio> decodedAssetViews_;
    std::unique_ptr<engine::PlaybackEngine> playback_;
    std::atomic<engine::PlaybackEngine*> audioPlayback_ { nullptr };
    int playbackMaxBlockSize_ = 128;
    UiExportBitDepth exportBitDepth_ = UiExportBitDepth::Float32;
    bool exportLoopRangeOnly_ = false;
    // Ruler range selection (parity item 25): transient, never persisted; -1 means no range.
    std::int64_t timelineRangeStartFrame_ = -1;
    std::int64_t timelineRangeEndFrame_ = -1;
    std::function<void()> playbackReplacementWillBegin_;
    std::function<void()> playbackReplacementDidEnd_;
    WaveformPeakService waveformService_;
};

} // namespace yesdaw::ui
