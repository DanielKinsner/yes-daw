// YES DAW - H3 mixer graph projection foundation.
//
// Control-thread-only helper: project/mixer state is projected into the frozen Node/GraphBuilder
// contracts. This slice covers mono track sources through Fader -> Pan -> Meter -> master Sum ->
// Master, plus Send edges into Bus SumNodes whose Returns feed the master bus. Sidechains/solo stay
// future H3 projection work.

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
        InvalidSendDestination,
        InvalidBusPan,
        GraphBuildFailed
    };

    Code            code       = Code::None;
    std::size_t     trackIndex = 0;
    std::size_t     sendIndex  = 0;
    std::size_t     busIndex   = 0;
    GraphBuildError graphError;
};

enum class MixerSendTap
{
    PreFader,
    PostFader
};

struct MixerSendProjection
{
    std::size_t  busIndex = 0;
    MixerSendTap tap      = MixerSendTap::PostFader;
};

struct MixerTrackProjection
{
    std::unique_ptr<Node> source;
    NodeId faderNodeId = 0;
    NodeId panNodeId   = 0;
    NodeId meterNodeId = 0;
    float  linearGain  = 1.0f;
    float  pan         = 0.0f;
    std::vector<MixerSendProjection> sends;
};

struct MixerBusProjection
{
    NodeId sumNodeId   = 0;
    NodeId panNodeId   = 0;
    NodeId meterNodeId = 0;
    float  pan         = 0.0f;   // centre by default; equal-power, like a Track Return (ADR-0014)
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
    std::vector<MixerBusProjection> buses;
};

inline void pushUniqueMixerInput (std::vector<Node*>& inputs, Node* node)
{
    if (std::find (inputs.begin(), inputs.end(), node) == inputs.end())
        inputs.push_back (node);
}

[[nodiscard]] inline bool mixerGainIsValid (float gain) noexcept
{
    // The upper bound is FaderNode's shared linear-gain ceiling, NOT float max: `<= float max` is a
    // tautology for any finite float and rejected nothing, so a pathological gain (e.g. 1e30) would
    // reach FaderNode::processRange and yield inf/NaN. Bounding here and clamping in FaderNode are the
    // two halves of the same guard.
    return std::isfinite (gain) && gain >= 0.0f && gain <= FaderNode::kMaxLinearGain;
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
    inputs.nodes.reserve (projection.tracks.size() * 4u + projection.buses.size() * 3u + 2u);

    std::vector<Node*> masterBusInputs;
    masterBusInputs.reserve (projection.tracks.size() + projection.buses.size());

    std::vector<std::vector<Node*>> busInputs (projection.buses.size());

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

        for (std::size_t sendIndex = 0; sendIndex < track.sends.size(); ++sendIndex)
        {
            const MixerSendProjection& send = track.sends[sendIndex];
            if (send.busIndex >= projection.buses.size())
            {
                if (error != nullptr)
                {
                    error->code = MixerProjectionError::Code::InvalidSendDestination;
                    error->trackIndex = i;
                    error->sendIndex = sendIndex;
                }
                return nullptr;
            }

            Node* const tap = send.tap == MixerSendTap::PreFader ? sourcePtr : static_cast<Node*> (faderPtr);
            pushUniqueMixerInput (busInputs[send.busIndex], tap);
        }

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

    for (std::size_t i = 0; i < projection.buses.size(); ++i)
    {
        const MixerBusProjection& bus = projection.buses[i];

        if (! mixerPanIsValid (bus.pan))
        {
            if (error != nullptr)
            {
                error->code = MixerProjectionError::Code::InvalidBusPan;
                error->busIndex = i;
            }
            return nullptr;
        }

        // The Bus sums its (mono) Send taps in mono; the Return then widens to centred stereo through its
        // own Pan -> Meter, mirroring the Track chain (ADR-0014). A mono Return summed straight into the
        // stereo master is audible in the left channel only — a mono signal into a stereo master must be
        // centred, the way a mono Track centres via its Pan node.
        auto busSum = std::make_unique<SumNode> (bus.sumNodeId, 1);
        SumNode* const busSumPtr = busSum.get();
        busSum->setInputNodes (std::move (busInputs[i]));

        auto busPan = std::make_unique<PanNode> (bus.panNodeId);
        PanNode* const busPanPtr = busPan.get();
        busPanPtr->setInput (busSumPtr);
        busPanPtr->setPan (bus.pan);

        auto busMeter = std::make_unique<MeterNode> (bus.meterNodeId, 2);
        MeterNode* const busMeterPtr = busMeter.get();
        busMeterPtr->setInput (busPanPtr);

        inputs.nodes.push_back (std::move (busSum));
        inputs.nodes.push_back (std::move (busPan));
        inputs.nodes.push_back (std::move (busMeter));
        masterBusInputs.push_back (busMeterPtr);
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
