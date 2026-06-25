// YES DAW - H3 mixer graph projection foundation.
//
// Control-thread-only helper: project/mixer state is projected into the frozen Node/GraphBuilder
// contracts. This first slice intentionally covers mono track sources through Fader -> Pan -> Meter ->
// master Sum -> Master. Sends/returns/sidechains/solo stay future H3 projection work.

#pragma once

#include "engine/GraphBuilder.h"
#include "engine/Node.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/MeterNode.h"
#include "engine/nodes/PanNode.h"
#include "engine/nodes/SumNode.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace yesdaw::engine {

struct MixerProjectionError
{
    enum class Code
    {
        None,
        MissingTrackSource,
        UnsupportedTrackSource,
        InvalidTrackGain,
        InvalidTrackPan,
        GraphBuildFailed
    };

    Code            code       = Code::None;
    std::size_t     trackIndex = 0;
    GraphBuildError graphError;
};

struct MixerTrackProjection
{
    std::unique_ptr<Node> source;
    NodeId faderNodeId = 0;
    NodeId panNodeId   = 0;
    NodeId meterNodeId = 0;
    float  linearGain  = 1.0f;
    float  pan         = 0.0f;
};

struct MixerProjectionInputs
{
    GraphId id = 0;
    NodeId  masterSumNodeId = GraphBuilder::kDefaultMasterNodeId - 1u;
    NodeId  masterNodeId    = GraphBuilder::kDefaultMasterNodeId;
    double  sampleRate      = 48000.0;
    int     maxBlockSize    = 512;
    const CompiledGraph* previousForCarryOver = nullptr;
    std::vector<MixerTrackProjection> tracks;
};

[[nodiscard]] inline bool mixerGainIsValid (float gain) noexcept
{
    return std::isfinite (gain) && gain >= 0.0f && gain <= std::numeric_limits<float>::max();
}

[[nodiscard]] inline bool mixerPanIsValid (float pan) noexcept
{
    return std::isfinite (pan) && pan >= -1.0f && pan <= 1.0f;
}

[[nodiscard]] inline std::unique_ptr<CompiledGraph> buildMixerGraphProjection (MixerProjectionInputs&& projection,
                                                                               MixerProjectionError* error = nullptr)
{
    if (error != nullptr)
        *error = MixerProjectionError {};

    GraphBuilder::Inputs inputs;
    inputs.id = projection.id;
    inputs.masterNodeId = projection.masterNodeId;
    inputs.sampleRate = projection.sampleRate;
    inputs.maxBlockSize = projection.maxBlockSize;
    inputs.previousForCarryOver = projection.previousForCarryOver;
    inputs.nodes.reserve (projection.tracks.size() * 4u + 2u);

    std::vector<Node*> masterBusInputs;
    masterBusInputs.reserve (projection.tracks.size());

    for (std::size_t i = 0; i < projection.tracks.size(); ++i)
    {
        MixerTrackProjection& track = projection.tracks[i];
        if (track.source == nullptr)
        {
            if (error != nullptr)
            {
                error->code = MixerProjectionError::Code::MissingTrackSource;
                error->trackIndex = i;
            }
            return nullptr;
        }

        const NodeProperties sourceProps = track.source->properties();
        if (! sourceProps.producesAudio || sourceProps.channels != 1)
        {
            if (error != nullptr)
            {
                error->code = MixerProjectionError::Code::UnsupportedTrackSource;
                error->trackIndex = i;
            }
            return nullptr;
        }

        if (! mixerGainIsValid (track.linearGain))
        {
            if (error != nullptr)
            {
                error->code = MixerProjectionError::Code::InvalidTrackGain;
                error->trackIndex = i;
            }
            return nullptr;
        }

        if (! mixerPanIsValid (track.pan))
        {
            if (error != nullptr)
            {
                error->code = MixerProjectionError::Code::InvalidTrackPan;
                error->trackIndex = i;
            }
            return nullptr;
        }

        Node* const sourcePtr = track.source.get();

        auto fader = std::make_unique<FaderNode> (track.faderNodeId, 1);
        FaderNode* const faderPtr = fader.get();
        faderPtr->setInput (sourcePtr);
        faderPtr->setTargetGain (track.linearGain);

        auto pan = std::make_unique<PanNode> (track.panNodeId);
        PanNode* const panPtr = pan.get();
        panPtr->setInput (faderPtr);
        panPtr->setPan (track.pan);

        auto meter = std::make_unique<MeterNode> (track.meterNodeId, 2);
        MeterNode* const meterPtr = meter.get();
        meterPtr->setInput (panPtr);

        inputs.nodes.push_back (std::move (track.source));
        inputs.nodes.push_back (std::move (fader));
        inputs.nodes.push_back (std::move (pan));
        inputs.nodes.push_back (std::move (meter));
        masterBusInputs.push_back (meterPtr);
    }

    auto masterSum = std::make_unique<SumNode> (projection.masterSumNodeId, 2);
    SumNode* const masterSumPtr = masterSum.get();
    masterSum->setInputNodes (std::move (masterBusInputs));

    auto master = std::make_unique<MasterNode> (projection.masterNodeId, 2);
    master->setInputNodes ({ masterSumPtr });

    inputs.nodes.push_back (std::move (masterSum));
    inputs.nodes.push_back (std::move (master));

    GraphBuildError graphError;
    std::unique_ptr<CompiledGraph> graph = GraphBuilder::build (std::move (inputs), &graphError);
    if (graph == nullptr || graphError.code() != GraphBuildError::Code::None)
    {
        if (error != nullptr)
        {
            error->code = MixerProjectionError::Code::GraphBuildFailed;
            error->graphError = graphError;
        }
        return nullptr;
    }

    return graph;
}

} // namespace yesdaw::engine
