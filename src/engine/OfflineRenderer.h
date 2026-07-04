// YES DAW - real offline Project renderer (H7) + shared Project->graph builder (H8).
//
// Control-side entrypoint: build a ProjectMixerProjection with decoded Asset samples (buildProjectGraph),
// then either process the full timeline through CompiledGraph for offline export (renderOfflineProject) or
// publish it to the realtime Runtime for playback (PlaybackEngine, H8). Offline render and playback share
// the EXACT same graph, so the H7 gate's independent-reference proof carries to playback.

#pragma once

#include "engine/ClipEnvelope.h"
#include "engine/Midi.h"
#include "engine/MixerGraphProjection.h"
#include "engine/ProjectMixerProjection.h"
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

struct OfflineRenderOptions
{
    GraphId graphId = 7000;
    NodeId  masterSumNodeId = GraphBuilder::kDefaultMasterNodeId - 101u;
    NodeId  masterNodeId = GraphBuilder::kDefaultMasterNodeId - 100u;
    int     maxBlockSize = 128;
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

[[nodiscard]] inline MidiFlattenStatus flattenMidiClipForProjection (const MidiClip& clip,
                                                                     TempoMapView tempoMap,
                                                                     SampleRate sampleRate,
                                                                     std::vector<ScheduledMidiEvent>& outTimeline)
{
    if (clip.timeBase == TimeBase::SampleLocked)
    {
        return flattenMidiClipToTimeline (
            clip,
            [] (Tick tick, double& frame) noexcept
            {
                if (tick < 0)
                    return false;
                frame = static_cast<double> (tick);
                return std::isfinite (frame);
            },
            outTimeline);
    }

    if (clip.timeBase == TimeBase::TempoLocked)
        return flattenMidiClipToTimeline (clip, tempoMap, sampleRate, outTimeline);

    outTimeline.clear();
    return MidiFlattenStatus::InvalidInput;
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

// Build the compiled Project graph (mixer projection over decoded Asset sources). Pure control-side.
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

    MixerProjectionInputs projection;
    OfflineRenderStatus factoryStatus = OfflineRenderStatus::Ok;
    const bool projected = projectToMixerProjectionInputs (
        project,
        config,
        [&decodedAssets, &resolved, &factoryStatus] (const Project&, const Clip& clip, const Asset& asset, NodeId expectedSourceId)
            -> std::unique_ptr<Node>
        {
            const DecodedAssetAudio* const decoded = detail::findDecodedAsset (decodedAssets, asset.id);
            if (decoded == nullptr)
            {
                factoryStatus = OfflineRenderStatus::MissingAssetAudio;
                return nullptr;
            }
            if (! detail::decodedAssetMetadataMatches (*decoded, asset))
            {
                factoryStatus = OfflineRenderStatus::AssetMetadataMismatch;
                return nullptr;
            }
            if (asset.channels != 1u)
            {
                factoryStatus = OfflineRenderStatus::UnsupportedAssetChannels;
                return nullptr;
            }

            const detail::ResolvedClipWindow* const window = detail::findResolvedClip (resolved, clip.id);
            if (window == nullptr)
            {
                factoryStatus = OfflineRenderStatus::InvalidTimeline;
                return nullptr;
            }
            if (clip.srcOffset > asset.frames || window->sourceFrames > asset.frames - clip.srcOffset)
            {
                factoryStatus = OfflineRenderStatus::SourceDecodeFailed;
                return nullptr;
            }

            std::vector<float> samples;
            if (window->sourceFrames > static_cast<std::uint64_t> (std::numeric_limits<std::size_t>::max()))
            {
                factoryStatus = OfflineRenderStatus::OutputTooLarge;
                return nullptr;
            }

            // Hand the RAW windowed source to DecodedClipNode and let IT apply Clip gain/fades. The realtime
            // engine plays Clips through DecodedClipNode's own (linear) fade, so pre-baking a different
            // (equal-power) curve on the control side here would make the exported file diverge from what
            // playback produces. Rendering through the same node keeps export == playback by construction.
            // Validate the fade/gain metadata first, as the envelope evaluator did, so malformed Clips are
            // still rejected.
            if (! detail::clipEditMetadataIsStorageSafe (clip))
            {
                factoryStatus = OfflineRenderStatus::SourceDecodeFailed;
                return nullptr;
            }

            samples.resize (static_cast<std::size_t> (window->sourceFrames));
            for (std::uint64_t frame = 0; frame < window->sourceFrames; ++frame)
            {
                const std::uint64_t sourceFrame = clip.srcOffset + frame;
                const float source = decoded->interleavedSamples[static_cast<std::size_t> (sourceFrame)];
                if (! std::isfinite (source))
                {
                    factoryStatus = OfflineRenderStatus::SourceDecodeFailed;
                    return nullptr;
                }

                samples[static_cast<std::size_t> (frame)] = source;
            }

            return std::make_unique<DecodedClipNode> (expectedSourceId,
                                                      std::move (samples),
                                                      1,
                                                      static_cast<std::int64_t> (window->startFrame),
                                                      static_cast<std::int64_t> (clip.fadeIn),
                                                      static_cast<std::int64_t> (clip.fadeOut),
                                                      clip.gain);
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

    for (std::size_t i = 0; i < project.midiClips.size(); ++i)
    {
        const MidiClip& midiClip = project.midiClips[i];

        std::vector<ScheduledMidiEvent> timeline;
        if (detail::flattenMidiClipForProjection (midiClip,
                                                  TempoMapView { project.tempoMap.data(), project.tempoMap.size() },
                                                  project.sampleRate,
                                                  timeline)
            != MidiFlattenStatus::Ok)
        {
            result.status = OfflineRenderStatus::MidiProjectionFailed;
            return result;
        }

        const NodeId midiSourceId = projectMixerNodeIdForClip (midiClip.id, ProjectMixerNodeRole::MidiSource);
        const NodeId instrumentId = projectMixerNodeIdForClip (midiClip.id, ProjectMixerNodeRole::Instrument);
        auto midiSource = std::make_unique<DecodedMidiClipNode> (midiSourceId, std::move (timeline));
        auto instrument = std::make_unique<ImpulseInstrumentNode> (instrumentId, 0, 1);
        instrument->setEventInput (midiSource.get());

        MixerTrackProjection track;
        track.supportNodes.push_back (std::move (midiSource));
        track.source = std::move (instrument);
        track.faderNodeId = projectMixerNodeIdForClip (midiClip.id, ProjectMixerNodeRole::Fader);
        track.panNodeId = projectMixerNodeIdForClip (midiClip.id, ProjectMixerNodeRole::Pan);
        track.meterNodeId = projectMixerNodeIdForClip (midiClip.id, ProjectMixerNodeRole::Meter);
        track.linearGain = 1.0f;
        track.pan = 0.0f;
        projection.tracks.push_back (std::move (track));
        (void) i;
    }

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
        graph.process (outputs.data(), channels, blockFrames);

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
