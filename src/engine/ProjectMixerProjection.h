// YES DAW - Project to mixer projection bridge (H3 item 4a).
//
// Control-thread-only: translate saved Project Clip and Track strip state into the existing
// MixerProjectionInputs shape. The caller supplies the source Node for each Clip/Asset pair; this bridge
// owns deterministic mixer node identity and validation.

#pragma once

#include "engine/MixerGraphProjection.h"
#include "engine/Project.h"

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

[[nodiscard]] inline NodeId projectMixerNodeIdForClip (EntityId clipId,
                                                       ProjectMixerNodeRole role) noexcept
{
    constexpr std::uint32_t kFnvOffset = 2166136261u;
    constexpr std::uint32_t kFnvPrime = 16777619u;

    std::uint32_t h = kFnvOffset;
    for (const std::uint8_t byte : clipId.bytes)
    {
        h ^= byte;
        h *= kFnvPrime;
    }

    h ^= static_cast<std::uint32_t> (role) + 0x9E3779B9u;
    h *= kFnvPrime;

    return h == 0u ? 1u : h;
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
    projection.tracks.reserve (project.clips.size());

    std::vector<NodeId> usedIds;
    usedIds.reserve (project.clips.size() * 4u + 2u);
    if (! detail::registerProjectMixerNodeId (usedIds, projection.masterSumNodeId, 0, ProjectMixerNodeRole::Source, error)
        || ! detail::registerProjectMixerNodeId (usedIds, projection.masterNodeId, 0, ProjectMixerNodeRole::Source, error))
        return false;

    for (std::size_t i = 0; i < project.clips.size(); ++i)
    {
        const Clip& clip = project.clips[i];
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
                *error = { ProjectMixerProjectionError::Code::InvalidClipGain, i, 0, ProjectMixerNodeRole::Fader };
            return false;
        }

        const Track* const owningTrack = project.findTrack (clip.trackId);
        if (owningTrack == nullptr || ! mixerGainIsValid (owningTrack->strip.linearGain))
        {
            if (error != nullptr)
                *error = { ProjectMixerProjectionError::Code::InvalidTrackGain, i, 0, ProjectMixerNodeRole::Fader };
            return false;
        }

        const double combinedGain = static_cast<double> (clip.gain) * static_cast<double> (owningTrack->strip.linearGain);
        if (combinedGain > static_cast<double> (FaderNode::kMaxLinearGain))
        {
            if (error != nullptr)
                *error = { ProjectMixerProjectionError::Code::InvalidTrackGain, i, 0, ProjectMixerNodeRole::Fader };
            return false;
        }

        const NodeId sourceId = projectMixerNodeIdForClip (clip.id, ProjectMixerNodeRole::Source);
        const NodeId faderId = projectMixerNodeIdForClip (clip.id, ProjectMixerNodeRole::Fader);
        const NodeId panId = projectMixerNodeIdForClip (clip.id, ProjectMixerNodeRole::Pan);
        const NodeId meterId = projectMixerNodeIdForClip (clip.id, ProjectMixerNodeRole::Meter);

        if (! detail::registerProjectMixerNodeId (usedIds, sourceId, i, ProjectMixerNodeRole::Source, error)
            || ! detail::registerProjectMixerNodeId (usedIds, faderId, i, ProjectMixerNodeRole::Fader, error)
            || ! detail::registerProjectMixerNodeId (usedIds, panId, i, ProjectMixerNodeRole::Pan, error)
            || ! detail::registerProjectMixerNodeId (usedIds, meterId, i, ProjectMixerNodeRole::Meter, error))
            return false;

        std::unique_ptr<Node> source = sourceFactory (project, clip, *asset, sourceId);
        if (source == nullptr)
        {
            if (error != nullptr)
                *error = { ProjectMixerProjectionError::Code::SourceFactoryFailed, i, sourceId, ProjectMixerNodeRole::Source };
            return false;
        }

        if (source->properties().id != sourceId)
        {
            if (error != nullptr)
                *error = { ProjectMixerProjectionError::Code::SourceNodeIdMismatch, i, sourceId, ProjectMixerNodeRole::Source };
            return false;
        }

        MixerTrackProjection track;
        track.source = std::move (source);
        track.faderNodeId = faderId;
        track.panNodeId = panId;
        track.meterNodeId = meterId;
        track.linearGain = static_cast<float> (combinedGain);
        track.pan = owningTrack->strip.pan;
        projection.tracks.push_back (std::move (track));
    }

    out = std::move (projection);
    return true;
}

} // namespace yesdaw::engine
