// YES DAW - Project to mixer projection bridge (H3 item 4a).
//
// Control-thread-only: translate saved Project Clip and Track strip state into the existing
// MixerProjectionInputs shape. The caller supplies the source Node for each Clip/Asset pair; this bridge
// owns deterministic mixer node identity and validation.

#pragma once

#include "engine/MixerGraphProjection.h"
#include "engine/Project.h"
#include "engine/nodes/CompressorNode.h"
#include "engine/nodes/EqNode.h"
#include "engine/nodes/FxDelayNode.h"
#include "engine/nodes/LimiterNode.h"
#include "engine/nodes/ReverbNode.h"
#include "engine/nodes/SumNode.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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
    Instrument,
    Fx
};

struct ProjectMixerSendRoute
{
    EntityId trackId;
    EntityId busId;
    MixerSendTap tap = MixerSendTap::PostFader;
    float linearGain = 1.0f;
};

struct ProjectMixerProjectionConfig
{
    GraphId id = 0;
    NodeId masterSumNodeId = GraphBuilder::kDefaultMasterNodeId - 1u;
    NodeId masterNodeId = GraphBuilder::kDefaultMasterNodeId;
    int maxBlockSize = 512;
    const CompiledGraph* previousForCarryOver = nullptr;
    std::vector<ProjectMixerSendRoute> sendRoutes;
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
        SourceNodeIdMismatch,
        FxNodeFactoryFailed,
        InvalidAutomationTarget,
        InvalidAutomationLane
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

[[nodiscard]] inline NodeId projectMixerSendLevelNodeIdForTrack (EntityId trackId,
                                                                 std::uint32_t sendOrdinal) noexcept
{
    constexpr std::uint32_t kFnvOffset = 2166136261u;
    constexpr std::uint32_t kFnvPrime = 16777619u;

    std::uint32_t h = kFnvOffset;
    for (const std::uint8_t byte : trackId.bytes)
    {
        h ^= byte;
        h *= kFnvPrime;
    }

    h ^= static_cast<std::uint32_t> (AutomationTargetRole::SendLevel) + 0x85EBCA6Bu;
    h *= kFnvPrime;
    h ^= sendOrdinal;
    h *= kFnvPrime;

    return h == 0u ? 1u : h;
}

namespace detail {

struct ProjectedAutomationTarget
{
    EntityId ownerEntity;
    AutomationTargetRole role = AutomationTargetRole::TrackFader;
    NodeId targetNode = 0;
    std::uint32_t parameterId = 0;
    bool parameterIdMustMatch = false;
    ParameterId compiledParameterId = 0;
    bool compiledParameterIdMustOverride = false;
};

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

inline void applyFxInsertParams (Node& node, const FxInsert& insert) noexcept
{
    if (! insert.enabled)
        return;

    for (const auto& [paramId, normalizedValue] : insert.normalizedParams)
    {
        if (auto* eq = dynamic_cast<EqNode*> (&node))
            eq->setNormalizedParameter (paramId, normalizedValue);
        else if (auto* compressor = dynamic_cast<CompressorNode*> (&node))
            compressor->setNormalizedParameter (paramId, normalizedValue);
        else if (auto* delay = dynamic_cast<FxDelayNode*> (&node))
            delay->setNormalizedParameter (paramId, normalizedValue);
        else if (auto* reverb = dynamic_cast<ReverbNode*> (&node))
            reverb->setNormalizedParameter (paramId, normalizedValue);
        else if (auto* limiter = dynamic_cast<LimiterNode*> (&node))
            limiter->setNormalizedParameter (paramId, normalizedValue);
    }
}

[[nodiscard]] inline std::unique_ptr<Node> makeProjectFxInsertNode (const FxInsert& insert, NodeId nodeId)
{
    std::unique_ptr<Node> node;
    switch (insert.kind)
    {
        case FxKind::Eq:
            node = std::make_unique<EqNode> (nodeId);
            break;
        case FxKind::Compressor:
            node = std::make_unique<CompressorNode> (nodeId);
            break;
        case FxKind::Delay:
            node = std::make_unique<FxDelayNode> (nodeId);
            break;
        case FxKind::Reverb:
            node = std::make_unique<ReverbNode> (nodeId);
            break;
        case FxKind::Limiter:
            node = std::make_unique<LimiterNode> (nodeId);
            break;
    }

    if (node != nullptr)
        applyFxInsertParams (*node, insert);
    return node;
}

[[nodiscard]] inline bool appendProjectFxChainNodes (const std::vector<FxInsert>& chain,
                                                     std::vector<NodeId>& usedIds,
                                                     std::vector<std::unique_ptr<Node>>& out,
                                                     std::size_t ownerIndex,
                                                     ProjectMixerProjectionError* error)
{
    out.reserve (out.size() + chain.size());
    for (const FxInsert& insert : chain)
    {
        const NodeId fxNodeId = projectMixerNodeIdForEntity (insert.id, ProjectMixerNodeRole::Fx);
        if (! registerProjectMixerNodeId (usedIds, fxNodeId, ownerIndex, ProjectMixerNodeRole::Fx, error))
            return false;

        std::unique_ptr<Node> node = makeProjectFxInsertNode (insert, fxNodeId);
        if (node == nullptr)
        {
            if (error != nullptr)
                *error = { ProjectMixerProjectionError::Code::FxNodeFactoryFailed, ownerIndex, fxNodeId, ProjectMixerNodeRole::Fx };
            return false;
        }

        out.push_back (std::move (node));
    }

    return true;
}

[[nodiscard]] inline const ProjectedAutomationTarget* findProjectedAutomationTarget (
    const std::vector<ProjectedAutomationTarget>& targets,
    EntityId ownerEntity,
    AutomationTargetRole role,
    std::uint32_t parameterId) noexcept
{
    for (const ProjectedAutomationTarget& target : targets)
        if (target.ownerEntity == ownerEntity
            && target.role == role
            && (! target.parameterIdMustMatch || target.parameterId == parameterId))
            return &target;

    return nullptr;
}

[[nodiscard]] inline bool busHasSendRoute (const ProjectMixerProjectionConfig& config,
                                           EntityId busId) noexcept
{
    for (const ProjectMixerSendRoute& route : config.sendRoutes)
        if (route.busId == busId)
            return true;

    return false;
}

[[nodiscard]] inline std::size_t projectedBusIndexForRoute (
    const Project& project,
    const std::vector<std::size_t>& projectedBusIndices,
    EntityId busId) noexcept
{
    constexpr std::size_t kMissing = std::numeric_limits<std::size_t>::max();
    for (std::size_t i = 0; i < project.buses.size(); ++i)
        if (project.buses[i].id == busId)
            return projectedBusIndices[i];

    return kMissing;
}

[[nodiscard]] inline bool appendCompiledAutomationLane (const AutomationLaneData& lane,
                                                        NodeId targetNode,
                                                        ParameterId parameterId,
                                                        const CompiledTempoMap& tempoMap,
                                                        std::vector<CompiledAutomationLane>& out)
{
    CompiledAutomationLane compiled;
    compiled.targetNode = targetNode;
    compiled.parameterId = parameterId;
    compiled.frames.reserve (lane.points.size());
    compiled.values.reserve (lane.points.size());
    compiled.curveTypes.reserve (lane.points.size());

    std::int64_t previousFrame = 0;
    bool havePrevious = false;
    for (const AutomationBreakpoint& point : lane.points)
    {
        double frame = 0.0;
        if (! tempoMap.frameForTick (point.tick, frame) || ! std::isfinite (frame) || frame < 0.0)
            return false;

        constexpr double kMaxFrame =
            static_cast<double> (std::numeric_limits<std::int64_t>::max()) - 1.0;
        if (frame > kMaxFrame)
            return false;

        const std::int64_t roundedFrame = static_cast<std::int64_t> (std::llround (frame));
        if (havePrevious && roundedFrame <= previousFrame)
            return false;

        compiled.frames.push_back (roundedFrame);
        compiled.values.push_back (point.value);
        compiled.curveTypes.push_back (point.curveType);

        previousFrame = roundedFrame;
        havePrevious = true;
    }

    out.push_back (std::move (compiled));
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
    std::vector<detail::ProjectedAutomationTarget> automationTargets;
    constexpr std::size_t kMissingProjectedBus = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> projectedBusIndices (project.buses.size(), kMissingProjectedBus);
    std::size_t fxInsertCount = 0;
    for (const Track& track : project.tracks)
        fxInsertCount += track.strip.fxChain.size();
    for (const Bus& bus : project.buses)
        fxInsertCount += bus.strip.fxChain.size();
    std::size_t projectedBusCount = 0;
    for (std::size_t busIndex = 0; busIndex < project.buses.size(); ++busIndex)
    {
        const Bus& bus = project.buses[busIndex];
        if (! bus.strip.fxChain.empty() || detail::busHasSendRoute (config, bus.id))
            projectedBusIndices[busIndex] = projectedBusCount++;
    }

    automationTargets.reserve (project.tracks.size() * 2u + config.sendRoutes.size() + projectedBusCount + fxInsertCount);
    usedIds.reserve (project.tracks.size() * 4u + config.sendRoutes.size() + projectedBusCount * 3u + project.clips.size() + fxInsertCount + 2u);
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
        if (! detail::appendProjectFxChainNodes (owningTrack.strip.fxChain,
                                                 usedIds,
                                                 track.insertNodes,
                                                 trackIndex,
                                                 error))
            return false;
        std::uint32_t sendOrdinal = 0;
        for (const ProjectMixerSendRoute& route : config.sendRoutes)
        {
            if (! (route.trackId == owningTrack.id))
                continue;

            if (! mixerGainIsValid (route.linearGain))
            {
                if (error != nullptr)
                    *error = { ProjectMixerProjectionError::Code::InvalidTrackGain, trackIndex, 0, ProjectMixerNodeRole::Fader };
                return false;
            }

            const std::size_t projectedBusIndex =
                detail::projectedBusIndexForRoute (project, projectedBusIndices, route.busId);
            if (projectedBusIndex == kMissingProjectedBus)
            {
                if (error != nullptr)
                    *error = { ProjectMixerProjectionError::Code::InvalidProject, trackIndex, 0, ProjectMixerNodeRole::Source };
                return false;
            }

            const NodeId sendFaderId = projectMixerSendLevelNodeIdForTrack (owningTrack.id, sendOrdinal);
            if (! detail::registerProjectMixerNodeId (usedIds, sendFaderId, trackIndex, ProjectMixerNodeRole::Fader, error))
                return false;

            track.sends.push_back (MixerSendProjection { projectedBusIndex, route.tap, sendFaderId, route.linearGain });
            automationTargets.push_back ({ owningTrack.id,
                                           AutomationTargetRole::SendLevel,
                                           sendFaderId,
                                           sendOrdinal,
                                           true,
                                           FaderNode::kGainParameterId,
                                           true });
            ++sendOrdinal;
        }

        automationTargets.push_back ({ owningTrack.id, AutomationTargetRole::TrackFader, faderId });
        automationTargets.push_back ({ owningTrack.id, AutomationTargetRole::TrackPan, panId });
        for (const FxInsert& insert : owningTrack.strip.fxChain)
            automationTargets.push_back ({ insert.id, AutomationTargetRole::FxInsertParam,
                                           projectMixerNodeIdForEntity (insert.id, ProjectMixerNodeRole::Fx) });
        projection.tracks.push_back (std::move (track));
    }

    for (std::size_t busIndex = 0; busIndex < project.buses.size(); ++busIndex)
    {
        const Bus& bus = project.buses[busIndex];
        if (projectedBusIndices[busIndex] == kMissingProjectedBus)
            continue;

        const NodeId sumId = projectMixerNodeIdForEntity (bus.id, ProjectMixerNodeRole::Source);
        const NodeId panId = projectMixerNodeIdForEntity (bus.id, ProjectMixerNodeRole::Pan);
        const NodeId meterId = projectMixerNodeIdForEntity (bus.id, ProjectMixerNodeRole::Meter);
        if (! detail::registerProjectMixerNodeId (usedIds, sumId, busIndex, ProjectMixerNodeRole::Source, error)
            || ! detail::registerProjectMixerNodeId (usedIds, panId, busIndex, ProjectMixerNodeRole::Pan, error)
            || ! detail::registerProjectMixerNodeId (usedIds, meterId, busIndex, ProjectMixerNodeRole::Meter, error))
            return false;

        MixerBusProjection projectedBus;
        projectedBus.sumNodeId = sumId;
        projectedBus.panNodeId = panId;
        projectedBus.meterNodeId = meterId;
        projectedBus.pan = bus.strip.pan;
        if (! detail::appendProjectFxChainNodes (bus.strip.fxChain,
                                                 usedIds,
                                                 projectedBus.insertNodes,
                                                 busIndex,
                                                 error))
            return false;
        automationTargets.push_back ({ bus.id, AutomationTargetRole::BusPan, panId });
        for (const FxInsert& insert : bus.strip.fxChain)
            automationTargets.push_back ({ insert.id, AutomationTargetRole::FxInsertParam,
                                           projectMixerNodeIdForEntity (insert.id, ProjectMixerNodeRole::Fx) });

        projection.buses.push_back (std::move (projectedBus));
    }

    if (! project.automationLanes.empty())
    {
        CompiledTempoMap tempoMap;
        if (! CompiledTempoMap::build (TempoMapView { project.tempoMap.data(), project.tempoMap.size() },
                                       project.sampleRate,
                                       tempoMap))
        {
            if (error != nullptr)
                *error = { ProjectMixerProjectionError::Code::InvalidAutomationLane, 0, 0, ProjectMixerNodeRole::Source };
            return false;
        }

        projection.automationLanes.reserve (project.automationLanes.size());
        for (std::size_t laneIndex = 0; laneIndex < project.automationLanes.size(); ++laneIndex)
        {
            const AutomationLaneData& lane = project.automationLanes[laneIndex];
            const detail::ProjectedAutomationTarget* const target =
                detail::findProjectedAutomationTarget (automationTargets, lane.ownerEntity, lane.role, lane.paramId);
            if (target == nullptr)
            {
                if (error != nullptr)
                    *error = { ProjectMixerProjectionError::Code::InvalidAutomationTarget,
                               laneIndex,
                               0,
                               ProjectMixerNodeRole::Source };
                return false;
            }

            const ParameterId compiledParameterId = target->compiledParameterIdMustOverride
                ? target->compiledParameterId
                : static_cast<ParameterId> (lane.paramId);
            if (! detail::appendCompiledAutomationLane (lane,
                                                        target->targetNode,
                                                        compiledParameterId,
                                                        tempoMap,
                                                        projection.automationLanes))
            {
                if (error != nullptr)
                    *error = { ProjectMixerProjectionError::Code::InvalidAutomationLane,
                               laneIndex,
                               target->targetNode,
                               ProjectMixerNodeRole::Source };
                return false;
            }
        }
    }

    out = std::move (projection);
    return true;
}

} // namespace yesdaw::engine
