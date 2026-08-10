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
#include "ui/WaveformPeakService.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
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
        }
        return snapshot;
    }
    void refreshTransportSnapshot() noexcept
    {
        syncContextFromPlayback();
    }
    [[nodiscard]] const engine::Project& project() const noexcept { return project_; }
    [[nodiscard]] engine::EntityId selectedTimelineClipId() const noexcept { return selectedTimelineClipId_; }
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
        ++context_.commandDispatchCount;
        return true;
    }

    [[nodiscard]] bool processDeviceAudioBlock (float* const* outputChannels,
                                                int numOutputChannels,
                                                int numFrames) noexcept YESDAW_RT_HOT
    {
        engine::PlaybackEngine* const playback = audioPlayback_.load (std::memory_order_acquire);
        if (playback == nullptr || numFrames < 0 || numFrames > playback->maxBlockSize())
        {
            zeroAudioOutputs (outputChannels, numOutputChannels, numFrames);
            return false;
        }

        playback->processBlock (outputChannels, numOutputChannels, numFrames);
        return true;
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
            ++context_.saveCount;
            ++context_.commandDispatchCount;
        }

        return result;
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

        const io::WavResult written = io::writeFloat32WavFile (
            destinationPath,
            rendered.sampleRate,
            rendered.channels,
            rendered.frames,
            std::span<const float> (rendered.interleavedSamples.data(), rendered.interleavedSamples.size()));
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

    [[nodiscard]] UiAppRecordResult recordDeterministicTestAudioTake()
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

        if (context_.isRecording)
        {
            context_.isRecording = false;
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
        context_.projectLoaded = true;
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
        ++context_.commandDispatchCount;
        ++context_.recordingCommandCount;
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
        clip.timelineStart = timelineEnd (nextProject);
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
        context_.projectLoaded = true;
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
        }

        (void) playback_->reclaim();
        syncContextFromPlayback();
        return interleaved;
    }

    [[nodiscard]] bool selectTimelineClip (engine::EntityId clipId) noexcept
    {
        if (findClip (clipId) == nullptr)
        {
            clearTimelineClipSelection();
            return false;
        }

        selectedTimelineClipId_ = clipId;
        context_.timelineClipSelected = true;
        context_.activePanel = UiPanel::Timeline;
        return true;
    }

    void clearTimelineClipSelection() noexcept
    {
        selectedTimelineClipId_ = {};
        context_.timelineClipSelected = false;
        if (context_.activePanel != UiPanel::PianoRoll)
            context_.activePanel = UiPanel::Timeline;
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
        syncProjectEditContext();
        ++context_.commandDispatchCount;
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

    // The selected strip's FX chain for UI display (empty when no selection).
    [[nodiscard]] std::vector<engine::FxInsert> selectedStripFxChain() const
    {
        engine::EntityId ownerId;
        if (! selectedMixerOwnerId (ownerId))
            return {};

        const engine::MixerStripState* const strip = engine::detail::findMixerStrip (project_, ownerId);
        return strip != nullptr ? strip->fxChain : std::vector<engine::FxInsert> {};
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

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied =
            nextUndo.apply (nextProject, engine::ProjectEditCommand::moveClip (selectedTimelineClipId_, timelineStart));

        if (! applied.applied())
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
        const engine::ProjectEditApplyResult applied =
            nextUndo.apply (nextProject, engine::ProjectEditCommand::deleteClip (selectedTimelineClipId_));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        selectedTimelineClipId_ = {};
        context_.timelineClipSelected = false;
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

        engine::Project nextProject = project_;
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::moveClipToTrack (selectedTimelineClipId_, targetTrackId, timelineStart));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

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
            selectedTimelineClipId_ = {};
            context_.timelineClipSelected = false;
        }

        ++context_.commandDispatchCount;
        ++context_.trackEditCount;
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

        const engine::Clip* const clip = findClip (selectedTimelineClipId_);
        if (clip == nullptr)
            return { id, { false, "timeline clip missing" }, false };

        if (timelineTick <= clip->timelineStart)
            return { id, { false, "split must be inside selected clip" }, false };

        const engine::Tick leftTimelineLength = timelineTick - clip->timelineStart;
        const std::optional<std::uint64_t> leftSourceLength = sourceLengthForSplit (*clip, leftTimelineLength);
        if (! leftSourceLength)
            return { id, { false, "split must be inside selected clip" }, false };

        engine::Project nextProject = project_;
        const engine::EntityId rightClipId = allocateSessionEntityId (0xC2u, nextProject);
        engine::ProjectUndoStack nextUndo = undo_;
        const engine::ProjectEditApplyResult applied = nextUndo.apply (
            nextProject,
            engine::ProjectEditCommand::splitClip (
                selectedTimelineClipId_, rightClipId, leftTimelineLength, *leftSourceLength));

        if (! applied.applied())
            return { id, state, false };

        if (! adoptEditedProject (std::move (nextProject), std::move (nextUndo)))
            return { id, { false, "timeline edit did not persist" }, false };

        ++context_.commandDispatchCount;
        ++context_.timelineEditCount;
        return { id, state, true };
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

    [[nodiscard]] UiActionDispatchResult setSelectedTimelineClipGain (float newGain)
    {
        const UiActionId id = UiActionId::TimelineClipSetGain;
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

    [[nodiscard]] UiActionDispatchResult setSelectedTimelineClipFades (engine::Tick fadeIn, engine::Tick fadeOut)
    {
        const UiActionId id = UiActionId::TimelineClipSetFades;
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
                return dispatchTransport (id, [this] { return playback_ != nullptr && playback_->play(); });

            case UiActionId::TransportStop:
                return dispatchTransport (id, [this] { return playback_ != nullptr && playback_->stop(); });

            case UiActionId::TransportLocateStart:
                return dispatchTransport (id, [this] { return playback_ != nullptr && playback_->locate (0); });

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
            case UiActionId::TimelineSnapDisable:
            case UiActionId::TimelineSnapSetBar:
            case UiActionId::TimelineSnapSetBeat:
            case UiActionId::TimelineSnapSetSixteenth:
            case UiActionId::TimelineAutomationToggleTrackLane:
            {
                return registry_.dispatch (id, context_);
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
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "track edit payload required" }, false };
            }

            case UiActionId::PianoRollNoteAdd:
            {
                const UiActionState currentState = registry_.stateFor (id, context_);
                if (! currentState.enabled)
                    return { id, currentState, false };

                return { id, { false, "note payload required" }, false };
            }

            case UiActionId::PianoRollNoteDelete:
                return deleteSelectedPianoRollNote();

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
        context_.projectLoaded = project_.hasValidAssetClipIndirection();
        context_.canUndo = undo_.canUndo();
        context_.canRedo = undo_.canRedo();
        context_.timelineClipSelected = selectedTimelineClipId_.isValid() && findClip (selectedTimelineClipId_) != nullptr;

        const engine::MidiClip* const midiClip = context_.projectLoaded ? findMidiClip (selectedMidiClipId_) : nullptr;
        if (midiClip == nullptr)
            selectedMidiNoteId_ = {};

        context_.midiClipSelected = midiClip != nullptr;
        context_.midiNoteSelected = midiClip != nullptr
            && selectedMidiNoteId_.isValid()
            && findNote (*midiClip, selectedMidiNoteId_) != nullptr;
        syncRecordingContext();
        syncRecordingCompContext();
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
        syncProjectEditContext();
        resetContextForFreshPlayback();
        return true;
    }

    [[nodiscard]] bool canAdoptEditWithoutPlaybackRebuild (const engine::Project& nextProject) const noexcept
    {
        return decodedAssets_.empty()
            && project_.clips.empty()
            && nextProject.clips.empty();
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

    [[nodiscard]] static bool projectContainsEntityId (const engine::Project& project,
                                                       engine::EntityId id) noexcept
    {
        if (! id.isValid())
            return false;

        if (project.id == id)
            return true;

        for (const engine::Asset& asset : project.assets)
            if (asset.id == id)
                return true;

        for (const engine::Track& track : project.tracks)
            if (track.id == id)
                return true;

        for (const engine::Bus& bus : project.buses)
            if (bus.id == id)
                return true;

        for (const engine::Clip& clip : project.clips)
            if (clip.id == id)
                return true;

        for (const engine::RecordingTake& take : project.recordingTakes)
            if (take.id == id)
                return true;

        for (const engine::ProjectRecordingCompSegment& segment : project.recordingCompSegments)
            if (segment.id == id)
                return true;

        for (const engine::MidiClip& midiClip : project.midiClips)
        {
            if (midiClip.id == id)
                return true;

            for (const engine::Note& note : midiClip.notes)
                if (note.id == id)
                    return true;
        }

        return false;
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

    void attachProjectBundle (
        persistence::ProjectBundleDb opened,
        const std::filesystem::path& bundlePath,
        engine::Project project)
    {
        bundleDb_ = std::move (opened);
        bundlePath_ = bundlePath;
        project_ = std::move (project);
        waveformService_.start (bundlePath_);
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

    void syncContextFromPlayback() noexcept
    {
        if (playback_ == nullptr)
            return;

        context_.isPlaying = playback_->isPlaying();
        context_.loopEnabled = playback_->loopEnabled();
        context_.playheadFrame = playback_->playheadFrame();
    }

    void resetContextForFreshPlayback() noexcept
    {
        context_.isPlaying = false;
        context_.loopEnabled = false;
        context_.playheadFrame = 0;
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

        if (playbackReplacementDidEnd_)
            playbackReplacementDidEnd_();
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
    engine::EntityId selectedTimelineClipId_;
    engine::EntityId selectedMidiClipId_;
    engine::EntityId selectedMidiNoteId_;
    MixerTargetSelection selectedMixerTarget_ {};
    UiRecordingDeviceSelection recordingDevice_;
    UiRecordingTrackInputSelection recordingTrackInput_;
    UiRecordedAudioTake lastRecordedAudioTake_;
    UiRecordedAudioTake pendingAudioPlacement_;
    UiRecordedMidiTake lastRecordedMidiTake_;
    UiRecordedMidiTake pendingMidiPlacement_;
    UiRecordingCompSelection recordingCompSelection_;
    UiAutosaveRecoveryPrompt autosaveRecovery_;
    AutosaveSchedulePolicy autosaveSchedule_ {};
    std::vector<UiDecodedAsset> decodedAssets_;
    std::vector<engine::DecodedAssetAudio> decodedAssetViews_;
    std::unique_ptr<engine::PlaybackEngine> playback_;
    std::atomic<engine::PlaybackEngine*> audioPlayback_ { nullptr };
    int playbackMaxBlockSize_ = 128;
    std::function<void()> playbackReplacementWillBegin_;
    std::function<void()> playbackReplacementDidEnd_;
    WaveformPeakService waveformService_;
};

} // namespace yesdaw::ui
