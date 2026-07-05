// YES DAW - H3 mixer graph projection foundation.
//
// Control-thread-only helper: project/mixer state is projected into the frozen Node/GraphBuilder
// contracts. This slice covers mono track sources through Fader -> Pan -> Meter -> master Sum ->
// Master, plus Send edges into Bus SumNodes whose Returns feed the master bus. A track may also carry
// control-built support Nodes feeding its source, which lets the host-isolation gate place a PluginNode
// source chain inside the projected mixer graph without changing the frozen Node contract.

#pragma once

#include "engine/GraphBuilder.h"
#include "engine/MixerValue.h"
#include "engine/Node.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/CompressorNode.h"
#include "engine/nodes/EqNode.h"
#include "engine/nodes/FxDelayNode.h"
#include "engine/nodes/LimiterNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/MeterNode.h"
#include "engine/nodes/PanNode.h"
#include "engine/nodes/ReverbNode.h"
#include "engine/nodes/SidechainGainNode.h"
#include "engine/nodes/SumNode.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace yesdaw::engine {

static_assert (kMixerMaxLinearGain == FaderNode::kMaxLinearGain);

struct MixerProjectionError
{
    enum class Code
    {
        None,
        MissingTrackSource,
        UnsupportedTrackSource,
        UnsupportedSidechainSource,
        InvalidTrackGain,
        InvalidTrackPan,
        InvalidSendDestination,
        InvalidSendGain,
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
    NodeId       faderNodeId = 0;
    float        linearGain  = 1.0f;
};

struct MixerTrackProjection
{
    std::unique_ptr<Node> source;
    std::vector<std::unique_ptr<Node>> supportNodes;
    NodeId faderNodeId = 0;
    NodeId panNodeId   = 0;
    NodeId meterNodeId = 0;
    float  linearGain  = 1.0f;
    float  pan         = 0.0f;
    std::vector<std::unique_ptr<Node>> insertNodes;
    std::vector<MixerSendProjection> sends;
    // Optional sidechain key. When set, a SidechainGainNode (id = sidechainNodeId) is inserted as a VCA on
    // the track source, keyed by this signal, ahead of the fader (ADR-0014). null = no sidechain. The key
    // is a real graph edge, so GraphBuilder PDC-aligns it with the main path exactly as the raw-graph
    // sidechain gate proves.
    std::unique_ptr<Node> sidechainSource;
    NodeId                sidechainNodeId = 0;
};

struct MixerBusProjection
{
    NodeId sumNodeId   = 0;
    NodeId panNodeId   = 0;
    NodeId meterNodeId = 0;
    float  pan         = 0.0f;   // centre by default; equal-power, like a Track Return (ADR-0014)
    std::vector<std::unique_ptr<Node>> insertNodes;
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

[[nodiscard]] inline NodeId mixerSendLevelNodeId (NodeId trackFaderNodeId,
                                                  std::size_t busIndex,
                                                  MixerSendTap tap) noexcept
{
    std::uint32_t h = 2166136261u;
    const auto mix = [&h] (std::uint32_t value) noexcept
    {
        h ^= value;
        h *= 16777619u;
    };

    mix (trackFaderNodeId);
    mix (static_cast<std::uint32_t> (busIndex & 0xFFFF'FFFFu));
    mix (static_cast<std::uint32_t> (tap == MixerSendTap::PreFader ? 0x53505245u : 0x53504F53u));
    mix (0xA15C0DEu);

    return h == 0u ? 1u : h;
}

[[nodiscard]] inline bool setMixerInsertInput (Node& node, Node* input) noexcept
{
    if (auto* fx = dynamic_cast<EqNode*> (&node))
    {
        fx->setInput (input);
        return true;
    }
    if (auto* fx = dynamic_cast<CompressorNode*> (&node))
    {
        fx->setInput (input);
        return true;
    }
    if (auto* fx = dynamic_cast<FxDelayNode*> (&node))
    {
        fx->setInput (input);
        return true;
    }
    if (auto* fx = dynamic_cast<ReverbNode*> (&node))
    {
        fx->setInput (input);
        return true;
    }
    if (auto* fx = dynamic_cast<LimiterNode*> (&node))
    {
        fx->setInput (input);
        return true;
    }

    return false;
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

    std::size_t supportNodeCount = 0;
    for (const MixerTrackProjection& track : projection.tracks)
        supportNodeCount += track.supportNodes.size();
    std::size_t insertNodeCount = 0;
    for (const MixerTrackProjection& track : projection.tracks)
        insertNodeCount += track.insertNodes.size();
    for (const MixerBusProjection& bus : projection.buses)
        insertNodeCount += bus.insertNodes.size();
    std::size_t sendNodeCount = 0;
    for (const MixerTrackProjection& track : projection.tracks)
        sendNodeCount += track.sends.size();
    inputs.nodes.reserve (projection.tracks.size() * 7u + supportNodeCount + insertNodeCount + sendNodeCount + projection.buses.size() * 4u + 2u);

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

        // Optional sidechain VCA insert (ADR-0014): the source becomes the MAIN input of a
        // SidechainGainNode keyed by track.sidechainSource, and the rest of the strip feeds from the VCA
        // output (the "chain head"). GraphBuilder treats the key as a real edge and PDC-aligns it with the
        // main path, so the projection inherits the alignment the raw-graph sidechain gate proves.
        std::unique_ptr<SidechainGainNode> sidechain;
        Node* chainHead = sourcePtr;
        if (track.sidechainSource != nullptr)
        {
            const NodeProperties sidechainProps = track.sidechainSource->properties();
            if (! sidechainProps.producesAudio || sidechainProps.channels != 1)
            {
                if (error != nullptr)
                {
                    error->code = MixerProjectionError::Code::UnsupportedSidechainSource;
                    error->trackIndex = i;
                }
                return nullptr;
            }

            sidechain = std::make_unique<SidechainGainNode> (track.sidechainNodeId, 1);
            sidechain->setMainInput (sourcePtr);
            sidechain->setSidechainInput (track.sidechainSource.get());
            chainHead = sidechain.get();
        }

        std::unique_ptr<PanNode> pan;
        PanNode* panPtr = nullptr;
        int chainChannels = sourceProps.channels;

        if (! track.insertNodes.empty())
        {
            pan = std::make_unique<PanNode> (track.panNodeId);
            panPtr = pan.get();
            panPtr->setInput (chainHead);
            panPtr->setPan (track.pan);
            chainHead = panPtr;
            chainChannels = 2;

            for (std::unique_ptr<Node>& insertNode : track.insertNodes)
            {
                if (insertNode == nullptr || ! setMixerInsertInput (*insertNode, chainHead))
                {
                    if (error != nullptr)
                    {
                        error->code = MixerProjectionError::Code::UnsupportedTrackSource;
                        error->trackIndex = i;
                    }
                    return nullptr;
                }

                const NodeProperties insertProps = insertNode->properties();
                if (! insertProps.producesAudio || insertProps.channels != 2)
                {
                    if (error != nullptr)
                    {
                        error->code = MixerProjectionError::Code::UnsupportedTrackSource;
                        error->trackIndex = i;
                    }
                    return nullptr;
                }

                chainHead = insertNode.get();
                chainChannels = insertProps.channels;
            }
        }

        auto fader = std::make_unique<FaderNode> (track.faderNodeId, chainChannels);
        FaderNode* const faderPtr = fader.get();
        faderPtr->setInput (chainHead);
        faderPtr->setTargetGain (track.linearGain);

        struct ActiveSendFader
        {
            std::size_t busIndex = 0;
            MixerSendTap tap = MixerSendTap::PostFader;
            float gain = 1.0f;
            Node* tapNode = nullptr;
            FaderNode* fader = nullptr;
            NodeId nodeId = 0;
        };
        std::vector<ActiveSendFader> activeSendFaders;
        activeSendFaders.reserve (track.sends.size());
        std::vector<std::unique_ptr<FaderNode>> sendFaders;
        sendFaders.reserve (track.sends.size());

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

            if (! mixerGainIsValid (send.linearGain))
            {
                if (error != nullptr)
                {
                    error->code = MixerProjectionError::Code::InvalidSendGain;
                    error->trackIndex = i;
                    error->sendIndex = sendIndex;
                }
                return nullptr;
            }

            // Pre-fader sends tap the chain head (post-sidechain VCA, pre-fader); post-fader taps the fader.
            Node* const tap = send.tap == MixerSendTap::PreFader ? chainHead : static_cast<Node*> (faderPtr);
            const NodeId sendFaderId = send.faderNodeId != 0u
                                     ? send.faderNodeId
                                     : mixerSendLevelNodeId (track.faderNodeId, send.busIndex, send.tap);

            FaderNode* sendFaderPtr = nullptr;
            for (const ActiveSendFader& active : activeSendFaders)
            {
                if (active.busIndex == send.busIndex
                    && active.tap == send.tap
                    && active.gain == send.linearGain
                    && active.tapNode == tap
                    && active.nodeId == sendFaderId)
                {
                    sendFaderPtr = active.fader;
                    break;
                }
            }

            if (sendFaderPtr == nullptr)
            {
                const int sendChannels = tap->properties().channels > 0 ? tap->properties().channels : 1;
                auto sendFader = std::make_unique<FaderNode> (sendFaderId, sendChannels);
                sendFaderPtr = sendFader.get();
                sendFaderPtr->setInput (tap);
                sendFaderPtr->setTargetGain (send.linearGain);
                activeSendFaders.push_back (ActiveSendFader { send.busIndex, send.tap, send.linearGain, tap, sendFaderPtr, sendFaderId });
                sendFaders.push_back (std::move (sendFader));
            }

            pushUniqueMixerInput (busInputs[send.busIndex], sendFaderPtr);
        }

        if (pan == nullptr)
        {
            pan = std::make_unique<PanNode> (track.panNodeId);
            panPtr = pan.get();
            panPtr->setInput (faderPtr);
            panPtr->setPan (track.pan);
        }

        auto meter = std::make_unique<MeterNode> (track.meterNodeId, 2);
        MeterNode* const meterPtr = meter.get();
        meterPtr->setInput (track.insertNodes.empty() ? static_cast<Node*> (panPtr) : static_cast<Node*> (faderPtr));

        for (std::unique_ptr<Node>& supportNode : track.supportNodes)
        {
            if (supportNode == nullptr)
            {
                if (error != nullptr)
                {
                    error->code = MixerProjectionError::Code::MissingTrackSource;
                    error->trackIndex = i;
                }
                return nullptr;
            }
            inputs.nodes.push_back (std::move (supportNode));
        }

        inputs.nodes.push_back (std::move (track.source));
        if (track.sidechainSource != nullptr)
            inputs.nodes.push_back (std::move (track.sidechainSource));
        if (sidechain != nullptr)
            inputs.nodes.push_back (std::move (sidechain));
        if (! track.insertNodes.empty())
            inputs.nodes.push_back (std::move (pan));
        for (std::unique_ptr<Node>& insertNode : track.insertNodes)
            inputs.nodes.push_back (std::move (insertNode));
        inputs.nodes.push_back (std::move (fader));
        for (std::unique_ptr<FaderNode>& sendFader : sendFaders)
            inputs.nodes.push_back (std::move (sendFader));
        if (track.insertNodes.empty())
            inputs.nodes.push_back (std::move (pan));
        inputs.nodes.push_back (std::move (meter));
        masterBusInputs.push_back (meterPtr);
    }

    for (std::size_t i = 0; i < projection.buses.size(); ++i)
    {
        MixerBusProjection& bus = projection.buses[i];

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

        Node* busChainHead = busSumPtr;
        for (std::unique_ptr<Node>& insertNode : bus.insertNodes)
        {
            if (insertNode == nullptr || ! setMixerInsertInput (*insertNode, busChainHead))
            {
                if (error != nullptr)
                {
                    error->code = MixerProjectionError::Code::GraphBuildFailed;
                    error->busIndex = i;
                }
                return nullptr;
            }

            const NodeProperties insertProps = insertNode->properties();
            if (! insertProps.producesAudio || insertProps.channels != 2)
            {
                if (error != nullptr)
                {
                    error->code = MixerProjectionError::Code::GraphBuildFailed;
                    error->busIndex = i;
                }
                return nullptr;
            }

            busChainHead = insertNode.get();
        }

        auto busPan = std::make_unique<PanNode> (bus.panNodeId);
        PanNode* const busPanPtr = busPan.get();
        busPanPtr->setInput (busChainHead);
        busPanPtr->setPan (bus.pan);

        auto busMeter = std::make_unique<MeterNode> (bus.meterNodeId, 2);
        MeterNode* const busMeterPtr = busMeter.get();
        busMeterPtr->setInput (busPanPtr);

        inputs.nodes.push_back (std::move (busSum));
        for (std::unique_ptr<Node>& insertNode : bus.insertNodes)
            inputs.nodes.push_back (std::move (insertNode));
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
