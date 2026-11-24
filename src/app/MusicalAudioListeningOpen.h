// Explicit full-open preparation for the opt-in listening build.
#pragma once

#include "engine/MusicalAudioPrepared.h"
#include "engine/OfflineRenderer.h"

namespace yesdaw::app {

inline constexpr std::uint64_t listeningBatchBytes = std::uint64_t { 1 } << 30;

inline const char* listeningFailure (engine::ListeningPrepareStatus status)
{
    using enum engine::ListeningPrepareStatus;
    switch (status)
    {
        case Ok: return "";
        case InvalidInput: return "invalid Musical clip or source window";
        case GeometryRefused: return "the Musical clip boundaries cannot be resolved";
        case ChannelRefused: return "the Musical source or channel layout is unavailable";
        case CutRefused: return "the requested Musical stretch cannot be planned";
        case ConfigurationUnsupported: return "unsupported Musical stretch configuration";
        case ResourceLimit: return "Musical audio exceeds the preparation budget";
        case MusicalQuietPathUnsupported: return "the stretcher cannot place this quiet passage accurately";
        case MusicalBoundaryContextUnsupported: return "the stretcher cannot place this boundary accurately";
        case EnvelopeRefused: return "the Musical fade or prepared audio is invalid";
    }
    return "Musical audio preparation failed";
}

inline bool containsMusicalAudio (const engine::Project& p)
{
    return std::any_of (p.clips.begin(), p.clips.end(), [] (const engine::Clip& clip) {
        return clip.timeBase == engine::TimeBase::TempoLocked;
    });
}

inline const engine::AssetOwnership* listeningOwner (std::span<const engine::AssetOwnership> owners,
                                                     engine::EntityId id)
{
    const auto it = std::find_if (owners.begin(), owners.end(), [id] (const auto& o) { return o.assetId == id; });
    return it == owners.end() ? nullptr : &*it;
}

// Full open builds all project-rate views before creating a new engine. Account each new
// cross-rate view once, including views used by Fixed tracks in the same project.
inline const char* checkListeningOpen (const engine::Project& project, std::uint64_t& remaining)
{
    using namespace engine;
    for (const auto& take : project.recordingTakes)
    {
        const auto clip = std::find_if (project.clips.begin(), project.clips.end(),
            [&] (const Clip& item) { return item.id == take.clipId; });
        if (clip != project.clips.end() && clip->timeBase == TimeBase::TempoLocked) return "a linked recording take cannot follow tempo";
    }
    for (const auto& asset : project.assets)
    {
        if (!assetNeedsRateMatchedView (asset, project.sampleRate)) continue;
        const double frames = static_cast<double> (asset.frames) * project.sampleRate.hz / asset.sampleRate.hz;
        if (!std::isfinite (frames) || frames < 0 || frames >= 0x1p63 || !asset.channels)
            return "invalid project-rate source view";
        const auto count = resampledFrameCount (asset.frames, asset.sampleRate.hz, project.sampleRate.hz);
        if (count > std::min (remaining, kMusicalAudioMaxReservedBytes) / (sizeof (float) * asset.channels))
            return "the project-rate audio exceeds the listening budget";
        remaining -= count * sizeof (float) * asset.channels;
    }
    return nullptr;
}

inline const char* prepareListeningOpen (const engine::Project& canonical, const engine::Project& projected,
    std::span<const engine::AssetOwnership> originals, std::span<const engine::DecodedAssetAudio> decoded,
    std::uint64_t remaining, engine::OfflineRenderOptions& options)
{
    using namespace engine;
    std::shared_ptr<const MusicalAudioClock> clock;
    if (MusicalAudioClock::build ({canonical.tempoMap.data(), canonical.tempoMap.size()}, canonical.sampleRate,
                                  { std::min (remaining, kMusicalAudioMaxReservedBytes), kMusicalAudioMaxWorkUnits },
                                  clock) != MusicalAudioStatus::Ok) return "the song tempo map cannot be prepared";
    auto ready = std::make_shared<ListeningClipOwners>();
    for (const auto& clip : canonical.clips)
    {
        if (clip.timeBase != TimeBase::TempoLocked) continue;
        const auto* original = listeningOwner (originals, clip.assetId);
        const auto* mapped = listeningOwner (options.assetOwners, clip.assetId);
        const auto* asset = canonical.findAsset (clip.assetId);
        if (!original || !mapped || !asset) return "the Musical source audio is missing";
        auto allowance = std::min (remaining, kMusicalAudioMaxReservedBytes);
        if (original->samples != mapped->samples)
        {
            const auto viewBytes = mapped->samples->interleaved.size() * sizeof (float);
            if (viewBytes >= kMusicalAudioMaxReservedBytes) return "the Musical view exceeds the clip budget";
            allowance = std::min (allowance, kMusicalAudioMaxReservedBytes - viewBytes);
        }
        ListeningClipAudio candidate;
        const auto status = prepareListeningClip (clip, *asset, original->samples, mapped->samples, clock,
                                                 { allowance, kMusicalAudioMaxWorkUnits }, candidate);
        if (status != ListeningPrepareStatus::Ok) return listeningFailure (status);
        if (candidate.retainedBytes() > remaining) return "prepared Musical audio exceeds the batch budget";
        remaining -= candidate.retainedBytes();
        ready->push_back (std::move (candidate));
    }
    // The graph consumes ready inputs only. Prepare any audible Fixed stretch before entering it.
    for (const auto& clip : projected.clips)
    {
        if (clip.timeBase != TimeBase::SampleLocked || clip.muted || clip.stretchFactor == 1.0f) continue;
        const auto* asset = projected.findAsset (clip.assetId);
        engine::detail::ResolvedClipWindow geometry;
        if (!asset || !engine::detail::resolveClipWindow (clip, geometry)) return "invalid Fixed stretch";
        const double frames = std::round (static_cast<double> (clip.srcLen) * clip.stretchFactor);
        if (!std::isfinite (frames) || frames < 1 || !asset->channels
            || frames > static_cast<double> (remaining / (asset->channels * sizeof (float))))
            return "the Fixed stretch exceeds the listening budget";
        // The scalar implementation holds planar input, planar output and the finished PCM together.
        const double window = std::floor (static_cast<double> (static_cast<float> (projected.sampleRate.hz)) * 0.12);
        const double scratchFrames = static_cast<double> (clip.srcLen) + window
            + std::max (frames + window, 2.0 * window) + frames;
        if (!std::isfinite (scratchFrames) || scratchFrames > static_cast<double> (
            std::min (remaining, kMusicalAudioMaxReservedBytes) / (asset->channels * sizeof (float))))
            return "the Fixed stretch working buffers exceed the listening budget";
        const auto* source = listeningOwner (options.assetOwners, clip.assetId);
        if (!source || !source->samples) return "missing Fixed stretch source";
        std::vector<std::pair<EntityId, std::shared_ptr<const AssetSamples>>> copied;
        ScheduledClipSource scheduled;
        OfflineRenderStatus status = OfflineRenderStatus::Ok;
        if (!engine::detail::makeScheduledClipSource (decoded, options.assetOwners, clip, *asset, asset->channels, geometry,
            copied, scheduled, status, projected.sampleRate.hz, {}, &options.stretchOwners))
            return "the Fixed stretch could not be prepared";
        remaining -= scheduled.owner->interleaved.size() * sizeof (float);
    }
    options.listeningClips = std::move (ready);
    return nullptr;
}

} // namespace yesdaw::app
