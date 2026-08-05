// YES DAW — canonical recorded-audio commit service (H17 packaged verifier U4; ADR-0036 surface).
//
// One control-side path for turning captured audio samples into canonical bundle-owned
// persistence: the samples become float-WAV Asset bytes imported into the bundle, a Clip placed at
// the timeline end of a real Track, and a RecordingTake linking asset/track/clip — then ONE
// project snapshot write. Shared by UiAppModel's recording path and
// tools/hardware/RecordingCheckMain.cpp, so the packaged checker persists REAL captured audio
// through exactly the code the app uses (KTD7), never through a parallel path, and the synthetic
// UI test helper cannot satisfy the hardware stage.
//
// Control-thread only: file I/O and Project mutation never touch the audio thread (the bounded
// capture FIFO in src/engine/Recording.h is the only audio-thread surface).
//
// Callers own identity and layout policy via callbacks: the UI model passes its session ULID
// allocator and its "Audio 1" fallback-track rule; the checker passes small deterministic ones.

#pragma once

#include "engine/Project.h"
#include "io/WavFile.h"
#include "persistence/ProjectBundle.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace yesdaw::app {

// Timeline append position: end of the last valid clip. Moved from UiAppModel so the service and
// the model share one definition.
[[nodiscard]] inline engine::Tick projectTimelineEnd (const engine::Project& project) noexcept
{
    engine::Tick end = 0;
    for (const engine::Clip& clip : project.clips)
    {
        if (clip.timelineStart < 0 || clip.timelineLength < 0)
            continue;

        if (clip.timelineStart > std::numeric_limits<engine::Tick>::max() - clip.timelineLength)
            continue;

        const engine::Tick clipEnd = clip.timelineStart + clip.timelineLength;
        if (clipEnd > end)
            end = clipEnd;
    }

    return end;
}

struct RecordedAudioTakeRequest
{
    engine::SampleRate sampleRate;
    std::uint64_t frames = 0;
    std::uint16_t channels = 0;
    std::span<const float> interleavedSamples;

    std::optional<engine::EntityId> targetTrackId;
    std::uint32_t deviceStableId = 0;
    std::uint16_t inputChannel = 0;
    std::uint32_t takeOrdinal = 0;
    engine::RecordingMonitoringPolicy monitoringPolicy = engine::RecordingMonitoringPolicy::Off;
};

enum class RecordedTakeCommitStatus : std::uint8_t
{
    Ok = 0,
    NoBundleOpen,
    InvalidDecodedAudio,
    SourceWriteFailed,
    AssetImportFailed,
    InvalidProjectIndirection,
    ProjectWriteFailed
};

struct RecordedTakeCommitResult
{
    RecordedTakeCommitStatus  status = RecordedTakeCommitStatus::Ok;
    persistence::BundleResult bundleResult;
    engine::Asset             importedAsset;
    engine::EntityId          trackId;
    engine::EntityId          clipId;
    engine::EntityId          takeId;
    engine::Tick              timelineStart = 0;
    engine::Project           project;   // the committed project state on Ok

    [[nodiscard]] bool ok() const noexcept { return status == RecordedTakeCommitStatus::Ok; }
};

// seedByte mirrors the session-ULID entropy convention the UI model has always used.
using AllocateEntityId = std::function<engine::EntityId (std::uint8_t seedByte, const engine::Project&)>;
// Resolves the fallback Track when the request names none (the UI's "Audio 1" rule).
using EnsureFallbackTrack = std::function<engine::Track& (engine::Project&)>;
// Optional caller mutation between take insertion and the single snapshot write (the UI model
// appends its paired synthetic MIDI take here). Structural breakage is caught by the indirection
// validation below, so the hook has no failure channel.
using DecorateProjectBeforeWrite = std::function<void (engine::Project&)>;

namespace commit_detail {

[[nodiscard]] inline bool requestAudioIsValid (const RecordedAudioTakeRequest& request) noexcept
{
    if (! request.sampleRate.isValid() || request.frames == 0 || request.channels == 0)
        return false;

    if (request.frames > static_cast<std::uint64_t> (std::numeric_limits<engine::Tick>::max()))
        return false;

    if (request.frames > std::numeric_limits<std::uint64_t>::max() / request.channels)
        return false;

    const std::uint64_t expectedSamples = request.frames * request.channels;
    return expectedSamples <= static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max())
        && request.interleavedSamples.size() == static_cast<std::size_t> (expectedSamples);
}

[[nodiscard]] inline std::filesystem::path makeCommitSourceTempPath()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds> (now).count();
    return std::filesystem::temp_directory_path()
         / ("yesdaw-recorded-take-" + std::to_string (nanos) + ".wav");
}

} // namespace commit_detail

[[nodiscard]] inline RecordedTakeCommitResult commitRecordedAudioTake (
    persistence::ProjectBundleDb& bundle,
    const engine::Project& currentProject,
    const RecordedAudioTakeRequest& request,
    const AllocateEntityId& allocateId,
    const EnsureFallbackTrack& ensureFallbackTrack,
    const DecorateProjectBeforeWrite& decorateBeforeWrite = {})
{
    RecordedTakeCommitResult result;

    if (! bundle.isOpen())
    {
        result.status = RecordedTakeCommitStatus::NoBundleOpen;
        return result;
    }

    if (! commit_detail::requestAudioIsValid (request))
    {
        result.status = RecordedTakeCommitStatus::InvalidDecodedAudio;
        return result;
    }

    // Canonical float-WAV source bytes, written to a temp file only long enough for the bundle's
    // content-addressed import to copy them in.
    const std::filesystem::path sourcePath = commit_detail::makeCommitSourceTempPath();
    if (! io::writeFloat32WavFile (sourcePath,
                                   request.sampleRate,
                                   request.channels,
                                   request.frames,
                                   request.interleavedSamples).ok())
    {
        result.status = RecordedTakeCommitStatus::SourceWriteFailed;
        return result;
    }

    const engine::EntityId requestedAssetId = allocateId (0xA2u, currentProject);
    const persistence::AssetImportRequest importRequest {
        sourcePath,
        requestedAssetId,
        request.frames,
        request.sampleRate,
        request.channels
    };

    engine::Asset imported;
    result.bundleResult = bundle.importAssetBytes (importRequest, imported);

    std::error_code removeError;
    std::filesystem::remove (sourcePath, removeError);

    if (! result.bundleResult.ok())
    {
        result.status = RecordedTakeCommitStatus::AssetImportFailed;
        return result;
    }

    engine::Project nextProject = currentProject;
    if (nextProject.findAsset (imported.id) == nullptr)
        nextProject.assets.push_back (imported);

    engine::Track* targetTrack = nullptr;
    if (request.targetTrackId && request.targetTrackId->isValid())
    {
        for (engine::Track& track : nextProject.tracks)
            if (track.id == *request.targetTrackId)
                targetTrack = &track;
    }

    if (targetTrack == nullptr)
        targetTrack = &ensureFallbackTrack (nextProject);

    const engine::EntityId placedTrackId = targetTrack->id;

    engine::Clip clip;
    clip.id = allocateId (0xC3u, nextProject);
    clip.assetId = imported.id;
    clip.trackId = placedTrackId;
    clip.timelineStart = projectTimelineEnd (nextProject);
    clip.timelineLength = static_cast<engine::Tick> (request.frames);
    clip.srcOffset = 0;
    clip.srcLen = request.frames;
    clip.gain = 1.0f;
    clip.fadeIn = 0;
    clip.fadeOut = 0;
    clip.timeBase = engine::TimeBase::SampleLocked;
    nextProject.clips.push_back (clip);

    engine::RecordingTake take;
    take.id = allocateId (0xA4u, nextProject);
    take.assetId = imported.id;
    take.trackId = placedTrackId;
    take.clipId = clip.id;
    take.timelineStart = clip.timelineStart;
    take.frameCount = imported.frames;
    take.takeOrdinal = request.takeOrdinal;
    take.inputChannel = request.inputChannel;
    take.deviceStableId = request.deviceStableId;
    take.monitoringPolicy = request.monitoringPolicy;
    nextProject.recordingTakes.push_back (take);

    if (decorateBeforeWrite)
        decorateBeforeWrite (nextProject);

    if (! nextProject.hasValidAssetClipIndirection())
    {
        result.status = RecordedTakeCommitStatus::InvalidProjectIndirection;
        return result;
    }

    result.bundleResult = bundle.writeProjectSnapshot (nextProject);
    if (! result.bundleResult.ok())
    {
        result.status = RecordedTakeCommitStatus::ProjectWriteFailed;
        return result;
    }

    result.status = RecordedTakeCommitStatus::Ok;
    result.importedAsset = imported;
    result.trackId = placedTrackId;
    result.clipId = clip.id;
    result.takeId = take.id;
    result.timelineStart = clip.timelineStart;
    result.project = std::move (nextProject);
    return result;
}

} // namespace yesdaw::app
