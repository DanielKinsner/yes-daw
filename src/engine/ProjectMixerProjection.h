// YES DAW - Project to mixer projection bridge (H3 item 4a).
//
// Control-thread-only: translate saved Project Clip and Track strip state into the existing
// MixerProjectionInputs shape. The caller supplies the source Node for each Clip/Asset pair; this bridge
// owns deterministic mixer node identity and validation.

#pragma once

#include "engine/MixerGraphProjection.h"
#include "engine/Project.h"
#include "engine/nodes/SumNode.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace yesdaw::engine {

enum class ProjectMixerNodeRole : std::uint8_t
{
    Source = 0,
    Fader,
    Pan,
    Meter,
    MidiSource,
    Instrument
};

struct ProjectMixerProjectionConfig
{
    GraphId id = 0;
    NodeId masterSumNodeId = GraphBuilder::kDefaultMasterNodeId - 1u;
    NodeId masterNodeId = GraphBuilder::kDefaultMasterNodeId;
    int maxBlockSize = 512;
    const CompiledGraph* previousForCarryOver = nullptr;
};

struct ProjectMixerProjectionError
{
    enum class Code
    {
        None,
        InvalidProject,
        InvalidClipGain,
        InvalidTrackGain,
        DuplicateNodeId,
        SourceFactoryFailed,
        SourceNodeIdMismatch
    };

    Code code = Code::None;
    std::size_t clipIndex = 0;
    NodeId nodeId = 0;
    ProjectMixerNodeRole role = ProjectMixerNodeRole::Source;
};

[[nodiscard]] inline NodeId projectMixerNodeIdForEntity (EntityId entityId,
                                                         ProjectMixerNodeRole role) noexcept
{
    constexpr std::uint32_t kFnvOffset = 2166136261u;
    constexpr std::uint32_t kFnvPrime = 16777619u;

    std::uint32_t h = kFnvOffset;
    for (const std::uint8_t byte : entityId.bytes)
    {
        h ^= byte;
        h *= kFnvPrime;
    }

    h ^= static_cast<std::uint32_t> (role) + 0x9E3779B9u;
    h *= kFnvPrime;

    return h == 0u ? 1u : h;
}

[[nodiscard]] inline NodeId projectMixerNodeIdForClip (EntityId clipId,
                                                       ProjectMixerNodeRole role) noexcept
{
    return projectMixerNodeIdForEntity (clipId, role);
}

[[nodiscard]] inline NodeId projectMixerNodeIdForTrack (EntityId trackId,
                                                        ProjectMixerNodeRole role) noexcept
{
    return projectMixerNodeIdForEntity (trackId, role);
}

namespace detail {

[[nodiscard]] inline bool registerProjectMixerNodeId (std::vector<NodeId>& used,
                                                      NodeId id,
                                                      std::size_t clipIndex,
                                                      ProjectMixerNodeRole role,
                                                      ProjectMixerProjectionError* error)
{
    if (std::find (used.begin(), used.end(), id) != used.end())
    {
        if (error != nullptr)
            *error = { ProjectMixerProjectionError::Code::DuplicateNodeId, clipIndex, id, role };
        return false;
    }

    used.push_back (id);
    return true;
}

} // namespace detail

// SourceFactory signature:
//   std::unique_ptr<Node> factory(const Project&, const Clip&, const Asset&, NodeId expectedSourceNodeId)
template <typename SourceFactory>
[[nodiscard]] inline bool projectToMixerProjectionInputs (const Project& project,
                                                          const ProjectMixerProjectionConfig& config,
                                                          SourceFactory&& sourceFactory,
                                                          MixerProjectionInputs& out,
                                                          ProjectMixerProjectionError* error = nullptr)
{
    if (error != nullptr)
        *error = {};

    if (! project.hasValidAssetClipIndirection())
    {
        if (error != nullptr)
            error->code = ProjectMixerProjectionError::Code::InvalidProject;
        return false;
    }

    MixerProjectionInputs projection;
    projection.id = config.id;
    projection.masterSumNodeId = config.masterSumNodeId;
    projection.masterNodeId = config.masterNodeId;
    projection.sampleRate = project.sampleRate.hz;
    projection.maxBlockSize = config.maxBlockSize;
    projection.previousForCarryOver = config.previousForCarryOver;
    projection.tracks.reserve (project.tracks.size());

    std::vector<NodeId> usedIds;
    usedIds.reserve (project.tracks.size() * 4u + project.clips.size() + 2u);
    if (! detail::registerProjectMixerNodeId (usedIds, projection.masterSumNodeId, 0, ProjectMixerNodeRole::Source, error)
        || ! detail::registerProjectMixerNodeId (usedIds, projection.masterNodeId, 0, ProjectMixerNodeRole::Source, error))
        return false;

    for (std::size_t trackIndex = 0; trackIndex < project.tracks.size(); ++trackIndex)
    {
        const Track& owningTrack = project.tracks[trackIndex];

        bool ownsAudioClip = false;
        for (const Clip& clip : project.clips)
        {
            if (clip.trackId == owningTrack.id)
            {
                ownsAudioClip = true;
                break;
            }
        }

        if (! ownsAudioClip)
            continue;

        if (! mixerGainIsValid (owningTrack.strip.linearGain))
        {
            if (error != nullptr)
                *error = { ProjectMixerProjectionError::Code::InvalidTrackGain, trackIndex, 0, ProjectMixerNodeRole::Fader };
            return false;
        }

        const NodeId sourceId = projectMixerNodeIdForTrack (owningTrack.id, ProjectMixerNodeRole::Source);
        const NodeId faderId = projectMixerNodeIdForTrack (owningTrack.id, ProjectMixerNodeRole::Fader);
        const NodeId panId = projectMixerNodeIdForTrack (owningTrack.id, ProjectMixerNodeRole::Pan);
        const NodeId meterId = projectMixerNodeIdForTrack (owningTrack.id, ProjectMixerNodeRole::Meter);

        if (! detail::registerProjectMixerNodeId (usedIds, sourceId, trackIndex, ProjectMixerNodeRole::Source, error)
            || ! detail::registerProjectMixerNodeId (usedIds, faderId, trackIndex, ProjectMixerNodeRole::Fader, error)
            || ! detail::registerProjectMixerNodeId (usedIds, panId, trackIndex, ProjectMixerNodeRole::Pan, error)
            || ! detail::registerProjectMixerNodeId (usedIds, meterId, trackIndex, ProjectMixerNodeRole::Meter, error))
            return false;

        std::vector<std::unique_ptr<Node>> clipSources;
        std::vector<Node*> sumInputs;

        for (std::size_t i = 0; i < project.clips.size(); ++i)
        {
            const Clip& clip = project.clips[i];
            if (! (clip.trackId == owningTrack.id))
                continue;

            const Asset* const asset = project.findAsset (clip.assetId);
            if (asset == nullptr)
            {
                if (error != nullptr)
                    *error = { ProjectMixerProjectionError::Code::InvalidProject, i, 0, ProjectMixerNodeRole::Source };
                return false;
            }

            if (! mixerGainIsValid (clip.gain))
            {
                if (error != nullptr)
                    *error = { ProjectMixerProjectionError::Code::InvalidClipGain, i, 0, ProjectMixerNodeRole::Source };
                return false;
            }

            const NodeId clipSourceId = projectMixerNodeIdForClip (clip.id, ProjectMixerNodeRole::Source);
            if (! detail::registerProjectMixerNodeId (usedIds, clipSourceId, i, ProjectMixerNodeRole::Source, error))
                return false;

            std::unique_ptr<Node> source = sourceFactory (project, clip, *asset, clipSourceId);
            if (source == nullptr)
            {
                if (error != nullptr)
                    *error = { ProjectMixerProjectionError::Code::SourceFactoryFailed, i, clipSourceId, ProjectMixerNodeRole::Source };
                return false;
            }

            if (source->properties().id != clipSourceId)
            {
                if (error != nullptr)
                    *error = { ProjectMixerProjectionError::Code::SourceNodeIdMismatch, i, clipSourceId, ProjectMixerNodeRole::Source };
                return false;
            }

            sumInputs.push_back (source.get());
            clipSources.push_back (std::move (source));
        }

        auto sum = std::make_unique<SumNode> (sourceId, 1);
        sum->setInputNodes (std::move (sumInputs));

        MixerTrackProjection track;
        track.supportNodes = std::move (clipSources);
        track.source = std::move (sum);
        track.faderNodeId = faderId;
        track.panNodeId = panId;
        track.meterNodeId = meterId;
        track.linearGain = owningTrack.strip.linearGain;
        track.pan = owningTrack.strip.pan;
        projection.tracks.push_back (std::move (track));
    }

    out = std::move (projection);
    return true;
}

} // namespace yesdaw::engine
