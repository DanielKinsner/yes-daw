// YES DAW - real offline Project renderer (H7) + shared Project->graph builder (H8).
//
// Control-side entrypoint: build a ProjectMixerProjection with decoded Asset samples (buildProjectGraph),
// then either process the full timeline through CompiledGraph for offline export (renderOfflineProject) or
// publish it to the realtime Runtime for playback (PlaybackEngine, H8). Offline render and playback share
// the EXACT same graph, so the H7 gate's independent-reference proof carries to playback.

#pragma once

#include "engine/ClipEnvelope.h"
#include "engine/ClipSchedule.h"
#include "engine/Midi.h"
#include "engine/MixerGraphProjection.h"
#include "engine/ProjectMixerProjection.h"
#include "engine/TimeStretch.h"
#include "engine/MixerMutePolicy.h"
#include "engine/nodes/SimpleSynthNode.h"
#include "engine/nodes/DecodedClipNode.h"
#include "engine/nodes/DecodedMidiClipNode.h"
#include "engine/nodes/ImpulseInstrumentNode.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

namespace yesdaw::engine {

struct DecodedAssetAudio
{
    EntityId          assetId;
    SampleRate        sampleRate;
    std::uint64_t     frames = 0;
    std::uint16_t     channels = 0;
    std::span<const float> interleavedSamples;
};

// G0.5: shared storage an asset's samples live in. When a build is handed one for an asset
// (OfflineRenderOptions::assetOwners, keyed by asset id), a ClipSchedule keeps the storage alive by
// reference instead of copying the window; without one the projection copies what it needs.
struct AssetOwnership
{
    EntityId                            assetId;
    std::shared_ptr<const AssetSamples> samples;
};

// G2.9: one stretched Clip's prepared samples (ADR-0030: prepared on the control side, read by
// absolute frame on the audio thread). Keyed by everything the preparation depends on, so a
// cache entry is reused only for the identical source window and factor.
struct StretchedOwnership
{
    EntityId                            clipId;
    EntityId                            assetId;
    std::uint64_t                       srcOffset = 0;
    std::uint64_t                       srcLen = 0;
    float                               stretchFactor = 1.0f;
    std::shared_ptr<const AssetSamples> samples;
};

struct OfflineRenderOptions
{
    GraphId graphId = 7000;
    NodeId  masterSumNodeId = GraphBuilder::kDefaultMasterNodeId - 101u;
    NodeId  masterNodeId = GraphBuilder::kDefaultMasterNodeId - 100u;
    int     maxBlockSize = 128;
    std::vector<ProjectMixerSendRoute> sendRoutes;
    std::vector<AssetOwnership> assetOwners;   // G0.5: optional shared storage per asset id
    std::vector<StretchedOwnership> stretchOwners;   // G2.9: optional prepared stretches per clip
};

enum class OfflineRenderStatus : std::uint8_t
{
    Ok = 0,
    InvalidProject,
    InvalidTimeline,
    EmptyTimeline,
    MissingAssetAudio,
    AssetMetadataMismatch,
    UnsupportedAssetChannels,
    UnsupportedTimeBase,
    SourceDecodeFailed,
    MidiProjectionFailed,
    ProjectProjectionFailed,
    MixerProjectionFailed,
    OutputTooLarge,
    RenderProducedNonFinite,
    TimeStretchFailed,          // G2.9: a stretched Clip's control-side preparation failed
    GraphNotBlockParallelSafe   // ADR-0027: graph has a cross-Block-stateful node; use the serial renderer
};

struct OfflineRenderResult
{
    OfflineRenderStatus status = OfflineRenderStatus::Ok;
    ProjectMixerProjectionError projectError;
    MixerProjectionError        mixerError;
    SampleRate                  sampleRate;
    std::uint16_t               channels = 0;
    std::uint64_t               frames = 0;
    std::vector<float>          interleavedSamples;

    [[nodiscard]] bool ok() const noexcept { return status == OfflineRenderStatus::Ok; }
};

// The compiled Project graph plus the resolved output geometry (full timeline length including the graph
// tail, and the Master channel count). Shared by the offline renderer and the realtime PlaybackEngine.
struct ProjectGraphResult
{
    OfflineRenderStatus            status = OfflineRenderStatus::Ok;
    ProjectMixerProjectionError    projectError;
    MixerProjectionError           mixerError;
    std::unique_ptr<CompiledGraph> graph;
    SampleRate                     sampleRate;
    std::uint16_t                  channels = 0;
    std::uint64_t                  frames = 0;          // full timeline, including the graph/PDC tail
    int                            maxBlockSize = 128;

    [[nodiscard]] bool ok() const noexcept { return status == OfflineRenderStatus::Ok; }
};

namespace detail {

struct ResolvedClipWindow
{
    EntityId      clipId;
    std::uint64_t startFrame = 0;
    std::uint64_t lengthFrames = 0;
    std::uint64_t sourceFrames = 0;
};

[[nodiscard]] inline bool checkedAddFrames (std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept
{
    if (b > std::numeric_limits<std::uint64_t>::max() - a)
        return false;

    out = a + b;
    return true;
}

[[nodiscard]] inline bool tickAsSampleLockedFrame (Tick tick, std::uint64_t& out) noexcept
{
    if (tick < 0)
        return false;

    out = static_cast<std::uint64_t> (tick);
    return true;
}

[[nodiscard]] inline bool frameDoubleToU64Ceil (double frame, std::uint64_t& out) noexcept
{
    if (! std::isfinite (frame) || frame < 0.0
        || frame > static_cast<double> (std::numeric_limits<std::uint64_t>::max()))
        return false;

    out = static_cast<std::uint64_t> (std::ceil (frame));
    return true;
}

[[nodiscard]] inline const DecodedAssetAudio* findDecodedAsset (std::span<const DecodedAssetAudio> assets,
                                                                EntityId id) noexcept
{
    for (const DecodedAssetAudio& audio : assets)
        if (audio.assetId == id)
            return &audio;

    return nullptr;
}

[[nodiscard]] inline const ResolvedClipWindow* findResolvedClip (std::span<const ResolvedClipWindow> clips,
                                                                 EntityId id) noexcept
{
    for (const ResolvedClipWindow& clip : clips)
        if (clip.clipId == id)
            return &clip;

    return nullptr;
}

[[nodiscard]] inline bool decodedAssetMetadataMatches (const DecodedAssetAudio& decoded,
                                                       const Asset& asset) noexcept
{
    if (! (decoded.assetId == asset.id)
        || decoded.sampleRate != asset.sampleRate
        || decoded.frames != asset.frames
        || decoded.channels != asset.channels)
        return false;

    if (decoded.channels == 0
        || decoded.frames > std::numeric_limits<std::uint64_t>::max() / decoded.channels)
        return false;

    const std::uint64_t expectedSamples = decoded.frames * decoded.channels;
    return expectedSamples <= static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max())
        && decoded.interleavedSamples.size() == static_cast<std::size_t> (expectedSamples);
}

[[nodiscard]] inline bool midiClipEndFrame (const MidiClip& clip,
                                            TempoMapView tempoMap,
                                            SampleRate sampleRate,
                                            std::uint64_t& outFrame) noexcept
{
    Tick endTick = 0;
    if (! addMidiTickChecked (clip.timelineStart, clip.timelineLength, endTick))
        return false;

    if (clip.timeBase == TimeBase::SampleLocked)
        return tickAsSampleLockedFrame (endTick, outFrame);

    if (clip.timeBase != TimeBase::TempoLocked)
        return false;

    double frame = 0.0;
    return tickToFrame (tempoMap, sampleRate, endTick, frame) && frameDoubleToU64Ceil (frame, outFrame);
}

} // namespace detail

// ADR-0014 mute/solo policy over the whole Project, published into a compiled graph's ATOMIC
// mute mask: explicit Mute wins, active solo mutes every non-soloed non-solo-safe strip, and the
// mask applies identically to playback and offline render (export == playback). A Track's
// contribution node is its per-Track source Sum — which since M1 carries the Track's MIDI as
// well as its audio, so MIDI rides the Track's mute/solo through the same node; a Bus
// contributes through its Sum. An unprojected strip (no mute-capable node) is skipped honestly.
// CONTROL THREAD; safe on a LIVE graph too — setMuted only touches the atomic mute words the
// audio thread reads (ADR-0014 designed exactly this), which is how the R12 live path republishes
// mute/solo/solo-safe without an engine rebuild. ONE law for build-time and live.
inline void applyProjectStripMuteMask (CompiledGraph& graph, const Project& project) noexcept
{
    bool anyActiveSolo = false;
    for (const Track& track : project.tracks)
        if (track.strip.soloed && ! track.strip.muted)
            anyActiveSolo = true;
    for (const Bus& bus : project.buses)
        if (bus.strip.soloed && ! bus.strip.muted)
            anyActiveSolo = true;

    const auto applyStripMute = [&graph, anyActiveSolo] (const MixerStripState& strip, NodeId contributionNodeId)
    {
        if (! graph.isMuteCapable (contributionNodeId))
            return;

        const MixerMuteTarget target { contributionNodeId, strip.muted, strip.soloed, strip.soloSafe };
        (void) graph.setMuted (contributionNodeId,
                               mixerTargetIsEffectivelyMuted (target, anyActiveSolo));
    };

    for (const Track& track : project.tracks)
        applyStripMute (track.strip, projectMixerNodeIdForTrack (track.id, ProjectMixerNodeRole::Source));

    for (const Bus& bus : project.buses)
        applyStripMute (bus.strip, projectMixerNodeIdForEntity (bus.id, ProjectMixerNodeRole::Source));
}

// Build the compiled Project graph (mixer projection over decoded Asset sources). Pure control-side.
namespace detail {

// G0.5: resolve ONE Clip into its schedule entry — validation identical to the old per-clip source
// factory (decoded asset present, metadata match, channel law, window inside the asset), then the
// samples: an owned asset (the model's shared storage) is referenced in place; an unowned view is
// copied ONCE per asset for this build (into `copiedAssets`) with the finite-sample scan the old
// window copy performed.
[[nodiscard]] inline bool makeScheduledClipSource (std::span<const DecodedAssetAudio> decodedAssets,
                                                   std::span<const AssetOwnership> assetOwners,
                                                   const Clip& clip,
                                                   const Asset& asset,
                                                   int stripChannels,
                                                   const ResolvedClipWindow& window,
                                                   std::vector<std::pair<EntityId, std::shared_ptr<const AssetSamples>>>& copiedAssets,
                                                   ScheduledClipSource& out,
                                                   OfflineRenderStatus& status,
                                                   double sampleRate = 0.0,
                                                   std::span<const StretchedOwnership> stretchOwners = {},
                                                   std::vector<StretchedOwnership>* preparedStretches = nullptr)
{
    const DecodedAssetAudio* const decoded = findDecodedAsset (decodedAssets, asset.id);
    if (decoded == nullptr)
    {
        status = OfflineRenderStatus::MissingAssetAudio;
        return false;
    }
    if (! decodedAssetMetadataMatches (*decoded, asset))
    {
        status = OfflineRenderStatus::AssetMetadataMismatch;
        return false;
    }
    if (asset.channels == 0u || asset.channels > 2u || stripChannels < static_cast<int> (asset.channels))
    {
        status = OfflineRenderStatus::UnsupportedAssetChannels;
        return false;
    }
    if (clip.srcOffset > asset.frames || window.sourceFrames > asset.frames - clip.srcOffset)
    {
        status = OfflineRenderStatus::SourceDecodeFailed;
        return false;
    }
    if (! clipEditMetadataIsStorageSafe (clip))
    {
        status = OfflineRenderStatus::SourceDecodeFailed;
        return false;
    }

    // An owner handed in for this asset is used only when it describes the SAME audio the view
    // does (frames + channels); anything else falls back to the build's own copy.
    std::shared_ptr<const AssetSamples> owner;
    for (const AssetOwnership& ownership : assetOwners)
        if (ownership.assetId == asset.id && ownership.samples != nullptr
            && ownership.samples->frames == decoded->frames
            && ownership.samples->channels == static_cast<int> (decoded->channels))
            owner = ownership.samples;
    if (owner == nullptr)
    {
        for (const auto& copied : copiedAssets)
            if (copied.first == asset.id)
                owner = copied.second;
        if (owner == nullptr)
        {
            const std::uint64_t assetChannels = asset.channels;
            const std::uint64_t total = decoded->frames * assetChannels;
            if (total > static_cast<std::uint64_t> (decoded->interleavedSamples.size())
                || total > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max()))
            {
                status = OfflineRenderStatus::OutputTooLarge;
                return false;
            }
            auto copy = std::make_shared<AssetSamples>();
            copy->channels = static_cast<int> (assetChannels);
            copy->frames = decoded->frames;
            copy->interleaved.resize (static_cast<std::size_t> (total));
            for (std::size_t n = 0; n < copy->interleaved.size(); ++n)
            {
                const float source = decoded->interleavedSamples[n];
                if (! std::isfinite (source))
                {
                    status = OfflineRenderStatus::SourceDecodeFailed;
                    return false;
                }
                copy->interleaved[n] = source;
            }
            owner = copy;
            copiedAssets.emplace_back (asset.id, owner);
        }
    }

    if (owner->frames < asset.frames || owner->channels != static_cast<int> (asset.channels))
    {
        status = OfflineRenderStatus::AssetMetadataMismatch;
        return false;
    }

    // G2.9: a stretched Clip plays PREPARED samples (ADR-0030) — reused from the caller's owners or
    // this build's own preparations, prepared once otherwise. The window law is the unstretched
    // one with the prepared length in place of srcLen.
    if (clip.stretchFactor != 1.0f)
    {
        if (! clipStretchIsStorageSafe (clip.stretchFactor) || sampleRate <= 0.0
            || clip.srcOffset > asset.frames || clip.srcLen > asset.frames - clip.srcOffset)
        {
            status = OfflineRenderStatus::TimeStretchFailed;
            return false;
        }
        const auto matches = [&] (const StretchedOwnership& entry)
        {
            return entry.samples != nullptr && entry.clipId == clip.id && entry.assetId == asset.id
                && entry.srcOffset == clip.srcOffset && entry.srcLen == clip.srcLen
                && entry.stretchFactor == clip.stretchFactor && entry.samples->channels == owner->channels;
        };
        std::shared_ptr<const AssetSamples> stretched;
        for (const StretchedOwnership& entry : stretchOwners)
            if (stretched == nullptr && matches (entry))
                stretched = entry.samples;
        if (preparedStretches != nullptr)
            for (const StretchedOwnership& entry : *preparedStretches)
                if (stretched == nullptr && matches (entry))
                    stretched = entry.samples;
        if (stretched == nullptr)
        {
            const std::size_t first = static_cast<std::size_t> (clip.srcOffset) * static_cast<std::size_t> (owner->channels);
            const std::size_t count = static_cast<std::size_t> (clip.srcLen) * static_cast<std::size_t> (owner->channels);
            PreparedTimeStretch prepared = prepareTimeStretch (std::span<const float> (owner->interleaved.data() + first, count),
                                                               static_cast<std::uint32_t> (owner->channels),
                                                               sampleRate,
                                                               static_cast<double> (clip.stretchFactor));
            if (! prepared.ok())
            {
                status = OfflineRenderStatus::TimeStretchFailed;
                return false;
            }
            auto samples = std::make_shared<AssetSamples>();
            samples->channels = owner->channels;
            samples->frames = prepared.outputFrames;
            samples->interleaved = std::move (prepared.interleavedSamples);
            stretched = samples;
            if (preparedStretches != nullptr)
                preparedStretches->push_back ({ clip.id, asset.id, clip.srcOffset, clip.srcLen, clip.stretchFactor, stretched });
        }
        out.owner = stretched;
        out.clip.samples = stretched->interleaved.data();
        out.clip.sourceFrames = static_cast<std::int64_t> (std::min<std::uint64_t> (stretched->frames, window.lengthFrames));
        out.clip.sourceChannels = stretched->channels;
        out.clip.startFrame = static_cast<std::int64_t> (window.startFrame);
        out.clip.fadeInFrames = static_cast<std::int64_t> (clip.fadeIn);
        out.clip.fadeOutFrames = static_cast<std::int64_t> (clip.fadeOut);
        out.clip.fadeInShape = clip.fadeInShape;     // G2.10
        out.clip.fadeOutShape = clip.fadeOutShape;
        out.clip.fadeInCurve = clip.fadeInCurve;
        out.clip.fadeOutCurve = clip.fadeOutCurve;
        out.clip.gain = clip.gain;
        return true;
    }

    out.owner = owner;
    out.clip.samples = owner->interleaved.data()
                     + static_cast<std::size_t> (clip.srcOffset) * static_cast<std::size_t> (owner->channels);
    out.clip.sourceFrames = static_cast<std::int64_t> (window.sourceFrames);
    out.clip.sourceChannels = owner->channels;
    out.clip.startFrame = static_cast<std::int64_t> (window.startFrame);
    out.clip.fadeInFrames = static_cast<std::int64_t> (clip.fadeIn);
    out.clip.fadeOutFrames = static_cast<std::int64_t> (clip.fadeOut);
    out.clip.fadeInShape = clip.fadeInShape;     // G2.10
    out.clip.fadeOutShape = clip.fadeOutShape;
    out.clip.fadeInCurve = clip.fadeInCurve;
    out.clip.fadeOutCurve = clip.fadeOutCurve;
    out.clip.gain = clip.gain;
    return true;
}

// G0.5: resolve one Clip's window with the SAME law buildProjectGraph applies to every Clip.
[[nodiscard]] inline bool resolveClipWindow (const Clip& clip, ResolvedClipWindow& out) noexcept
{
    if (clip.timeBase != TimeBase::SampleLocked)
        return false;
    std::uint64_t startFrame = 0;
    std::uint64_t lengthFrames = 0;
    if (! tickAsSampleLockedFrame (clip.timelineStart, startFrame)
        || ! tickAsSampleLockedFrame (clip.timelineLength, lengthFrames))
        return false;
    std::uint64_t clipEnd = 0;
    if (! checkedAddFrames (startFrame, lengthFrames, clipEnd))
        return false;
    out = { clip.id, startFrame, lengthFrames, std::min<std::uint64_t> (clip.srcLen, lengthFrames) };
    return true;
}

} // namespace detail

// G0.5 — the live placement lane's builder (CONTROL THREAD): the ClipSchedule for one Track of
// `project`, through the SAME per-clip resolver the graph build uses, so a schedule published
// live is byte-for-byte what a rebuild would install. Null (with `status`) when any Clip of the
// Track fails the law — the caller then converges by a real rebuild instead of guessing.
[[nodiscard]] inline std::unique_ptr<ClipSchedule> buildTrackClipSchedule (const Project& project,
                                                                            EntityId trackId,
                                                                            std::span<const DecodedAssetAudio> decodedAssets,
                                                                            OfflineRenderStatus& status,
                                                                            std::span<const AssetOwnership> assetOwners = {},
                                                                            std::span<const StretchedOwnership> stretchOwners = {},
                                                                            std::vector<StretchedOwnership>* preparedStretches = nullptr)
{
    status = OfflineRenderStatus::Ok;
    auto schedule = std::make_unique<ClipSchedule>();
    std::vector<std::pair<EntityId, std::shared_ptr<const AssetSamples>>> copiedAssets;
    std::vector<StretchedOwnership> localStretches;   // G2.9: this build's preparations when the caller keeps none
    if (preparedStretches == nullptr)
        preparedStretches = &localStretches;

    int stripChannels = 1;
    for (const Clip& clip : project.clips)
    {
        if (! (clip.trackId == trackId))
            continue;
        const Asset* const asset = project.findAsset (clip.assetId);
        if (asset == nullptr || asset->channels == 0u || asset->channels > 2u)
        {
            status = OfflineRenderStatus::InvalidProject;
            return nullptr;
        }
        if (asset->channels == 2u)
            stripChannels = 2;
    }

    for (const Clip& clip : project.clips)
    {
        if (! (clip.trackId == trackId))
            continue;
        const Asset* const asset = project.findAsset (clip.assetId);
        if (asset == nullptr || ! mixerGainIsValid (clip.gain))
        {
            status = OfflineRenderStatus::InvalidProject;
            return nullptr;
        }
        detail::ResolvedClipWindow window;
        if (! detail::resolveClipWindow (clip, window))
        {
            status = OfflineRenderStatus::InvalidTimeline;
            return nullptr;
        }
        ScheduledClipSource source;
        if (! detail::makeScheduledClipSource (decodedAssets, assetOwners, clip, *asset, stripChannels, window,
                                               copiedAssets, source, status,
                                               project.sampleRate.hz, stretchOwners, preparedStretches))
            return nullptr;
        schedule->clips.push_back (source.clip);
        if (source.owner != nullptr)
        {
            bool held = false;
            for (const auto& owner : schedule->keepAlive)
                held = held || owner == source.owner;
            if (! held)
                schedule->keepAlive.push_back (source.owner);
        }
    }
    return schedule;
}

[[nodiscard]] inline ProjectGraphResult buildProjectGraph (const Project& project,
                                                           std::span<const DecodedAssetAudio> decodedAssets,
                                                           OfflineRenderOptions options = {})
{
    ProjectGraphResult result;
    result.sampleRate = project.sampleRate;
    result.maxBlockSize = options.maxBlockSize;

    if (! project.hasValidAssetClipIndirection() || options.maxBlockSize <= 0)
    {
        result.status = OfflineRenderStatus::InvalidProject;
        return result;
    }

    std::vector<detail::ResolvedClipWindow> resolved;
    resolved.reserve (project.clips.size());

    std::uint64_t timelineEndFrames = 0;
    for (const Clip& clip : project.clips)
    {
        if (clip.timeBase != TimeBase::SampleLocked)
        {
            result.status = OfflineRenderStatus::UnsupportedTimeBase;
            return result;
        }

        std::uint64_t startFrame = 0;
        std::uint64_t lengthFrames = 0;
        if (! detail::tickAsSampleLockedFrame (clip.timelineStart, startFrame)
            || ! detail::tickAsSampleLockedFrame (clip.timelineLength, lengthFrames))
        {
            result.status = OfflineRenderStatus::InvalidTimeline;
            return result;
        }

        const std::uint64_t sourceFrames = std::min<std::uint64_t> (clip.srcLen, lengthFrames);
        std::uint64_t clipEnd = 0;
        if (! detail::checkedAddFrames (startFrame, lengthFrames, clipEnd))
        {
            result.status = OfflineRenderStatus::InvalidTimeline;
            return result;
        }

        timelineEndFrames = std::max (timelineEndFrames, clipEnd);
        resolved.push_back ({ clip.id, startFrame, lengthFrames, sourceFrames });
    }

    for (const MidiClip& midiClip : project.midiClips)
    {
        if (! midiClip.isValid())
        {
            result.status = OfflineRenderStatus::InvalidProject;
            return result;
        }

        std::uint64_t clipEnd = 0;
        if (! detail::midiClipEndFrame (midiClip,
                                        TempoMapView { project.tempoMap.data(), project.tempoMap.size() },
                                        project.sampleRate,
                                        clipEnd))
        {
            result.status = OfflineRenderStatus::InvalidTimeline;
            return result;
        }

        timelineEndFrames = std::max (timelineEndFrames, clipEnd);
    }

    for (const Marker& marker : project.markers)
    {
        double markerFrame = 0.0;
        if (! tickToFrame (TempoMapView { project.tempoMap.data(), project.tempoMap.size() },
                           project.sampleRate,
                           marker.tick,
                           markerFrame)
            || markerFrame < 0.0
            || markerFrame > static_cast<double> (std::numeric_limits<std::uint64_t>::max()))
        {
            result.status = OfflineRenderStatus::InvalidTimeline;
            return result;
        }

        timelineEndFrames = std::max (timelineEndFrames, static_cast<std::uint64_t> (std::llround (markerFrame)));
    }

    if (timelineEndFrames == 0)
    {
        result.status = OfflineRenderStatus::EmptyTimeline;
        return result;
    }

    ProjectMixerProjectionConfig config;
    config.id = options.graphId;
    config.masterSumNodeId = options.masterSumNodeId;
    config.masterNodeId = options.masterNodeId;
    config.maxBlockSize = options.maxBlockSize;
    config.sendRoutes = options.sendRoutes;
    // ADR-0044: persisted send rows on the Track are the product's routing source of truth; the
    // options seam stays for engine tests and is concatenated in front (no overlap in practice).
    for (const Track& track : project.tracks)
        for (const SendRow& send : track.sends)
            config.sendRoutes.push_back (ProjectMixerSendRoute {
                track.id,
                send.busId,
                send.tap == SendTap::PreFader ? MixerSendTap::PreFader : MixerSendTap::PostFader,
                send.linearGain });

    MixerProjectionInputs projection;
    OfflineRenderStatus factoryStatus = OfflineRenderStatus::Ok;
    // G0.5: the SAME per-clip law the live placement lane uses (buildTrackClipSchedule) — one
    // resolver, so what the engine plays after a live edit is exactly what a rebuild would bake.
    std::vector<std::pair<EntityId, std::shared_ptr<const AssetSamples>>> copiedAssets;
    std::vector<StretchedOwnership> preparedStretches;   // G2.9: prepared once per stretched Clip per build
    const bool projected = projectToMixerProjectionInputs (
        project,
        config,
        [&decodedAssets, &resolved, &factoryStatus, &copiedAssets, &options, &preparedStretches] (const Project& projected,
                                                                                                  const Clip& clip, const Asset& asset,
                                                                                                  int stripChannels, ScheduledClipSource& out) -> bool
        {
            const detail::ResolvedClipWindow* const window = detail::findResolvedClip (resolved, clip.id);
            if (window == nullptr)
            {
                factoryStatus = OfflineRenderStatus::InvalidTimeline;
                return false;
            }
            return detail::makeScheduledClipSource (decodedAssets,
                                                    std::span<const AssetOwnership> (options.assetOwners.data(),
                                                                                     options.assetOwners.size()),
                                                    clip, asset, stripChannels, *window,
                                                    copiedAssets, out, factoryStatus,
                                                    projected.sampleRate.hz,
                                                    std::span<const StretchedOwnership> (options.stretchOwners.data(),
                                                                                         options.stretchOwners.size()),
                                                    &preparedStretches);
        },
        projection,
        &result.projectError);

    if (! projected)
    {
        result.status = factoryStatus == OfflineRenderStatus::Ok
            ? OfflineRenderStatus::ProjectProjectionFailed
            : factoryStatus;
        return result;
    }

    // M1 (ADR-0045): MIDI Clips are projected BY the mixer projection, into their owning Track's
    // strip Sum — there is no per-Clip MIDI strip any more, so nothing to append here.

    std::unique_ptr<CompiledGraph> graph = buildMixerGraphProjection (std::move (projection), &result.mixerError);
    if (graph == nullptr || result.mixerError.code != MixerProjectionError::Code::None)
    {
        result.status = OfflineRenderStatus::MixerProjectionFailed;
        return result;
    }

    const std::int64_t graphTail = graph->totalLatency();
    if (graphTail > 0)
    {
        const std::uint64_t tail = static_cast<std::uint64_t> (graphTail);
        if (! detail::checkedAddFrames (timelineEndFrames, tail, timelineEndFrames))
        {
            result.status = OfflineRenderStatus::OutputTooLarge;
            return result;
        }
    }

    std::uint64_t nodeTail = 0;
    if (! graph->totalTailSamples (nodeTail)
        || ! detail::checkedAddFrames (timelineEndFrames, nodeTail, timelineEndFrames))
    {
        result.status = OfflineRenderStatus::OutputTooLarge;
        return result;
    }

    const int masterChannels = graph->debugMasterChannels();
    if (masterChannels <= 0 || masterChannels > std::numeric_limits<std::uint16_t>::max())
    {
        result.status = OfflineRenderStatus::MixerProjectionFailed;
        return result;
    }

    // ADR-0014 mute/solo policy, finally WIRED to the Project's strip state — the shared
    // applyProjectStripMuteMask law below, so build-time and the R12 live path can never drift.
    applyProjectStripMuteMask (*graph, project);

    result.graph = std::move (graph);
    result.channels = static_cast<std::uint16_t> (masterChannels);
    result.frames = timelineEndFrames;
    result.status = OfflineRenderStatus::Ok;
    return result;
}

[[nodiscard]] inline OfflineRenderResult renderOfflineProject (const Project& project,
                                                               std::span<const DecodedAssetAudio> decodedAssets,
                                                               OfflineRenderOptions options = {})
{
    OfflineRenderResult result;
    result.sampleRate = project.sampleRate;

    ProjectGraphResult built = buildProjectGraph (project, decodedAssets, options);
    result.projectError = built.projectError;
    result.mixerError = built.mixerError;
    if (! built.ok())
    {
        result.status = built.status;
        return result;
    }

    const std::uint16_t channels = built.channels;
    const std::uint64_t timelineEndFrames = built.frames;
    CompiledGraph& graph = *built.graph;

    if (timelineEndFrames > std::numeric_limits<std::uint64_t>::max() / channels)
    {
        result.status = OfflineRenderStatus::OutputTooLarge;
        return result;
    }

    const std::uint64_t sampleCount = timelineEndFrames * channels;
    if (sampleCount > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max()))
    {
        result.status = OfflineRenderStatus::OutputTooLarge;
        return result;
    }

    result.interleavedSamples.assign (static_cast<std::size_t> (sampleCount), 0.0f);
    std::vector<float> channelStorage (static_cast<std::size_t> (channels) * static_cast<std::size_t> (options.maxBlockSize), 0.0f);
    std::vector<float*> outputs (channels, nullptr);
    for (std::uint16_t c = 0; c < channels; ++c)
        outputs[c] = channelStorage.data() + static_cast<std::size_t> (c) * static_cast<std::size_t> (options.maxBlockSize);

    std::uint64_t offset = 0;
    while (offset < timelineEndFrames)
    {
        const std::uint64_t remaining = timelineEndFrames - offset;
        const int blockFrames = static_cast<int> (std::min<std::uint64_t> (remaining, static_cast<std::uint64_t> (options.maxBlockSize)));
        Transport transport;
        transport.projectSampleRate = project.sampleRate;
        transport.timelineFrame = static_cast<std::int64_t> (offset);
        transport.hasTimelineFrame = true;
        transport.isPlaying = true;
        EventStream events;
        graph.process (outputs.data(), channels, blockFrames, events, transport);

        for (int frame = 0; frame < blockFrames; ++frame)
        {
            const std::size_t outFrame = static_cast<std::size_t> (offset + static_cast<std::uint64_t> (frame));
            for (std::uint16_t channel = 0; channel < channels; ++channel)
            {
                const float value = outputs[channel][frame];
                if (! std::isfinite (value))
                {
                    result.status = OfflineRenderStatus::RenderProducedNonFinite;
                    return result;
                }

                result.interleavedSamples[outFrame * channels + channel] = value;
            }
        }

        offset += static_cast<std::uint64_t> (blockFrames);
    }

    result.status = OfflineRenderStatus::Ok;
    result.channels = channels;
    result.frames = timelineEndFrames;
    return result;
}

} // namespace yesdaw::engine
