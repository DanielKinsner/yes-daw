// YES DAW - H3 mixer graph projection foundation.
//
// Control-thread-only helper: project/mixer state is projected into the frozen Node/GraphBuilder
// contracts. This slice covers mono track sources through Fader -> Pan -> Meter -> master Sum ->
// Master, plus Send edges into Bus SumNodes whose Returns feed the master bus. A track may also carry
// control-built support Nodes feeding its source, which lets the host-isolation gate place a PluginNode
// source chain inside the projected mixer graph without changing the frozen Node contract.

#pragma once

#include "engine/nodes/DelayNode.h"   // ADR-0058 cp3: a stem's master latency
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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
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
        InvalidBusGain,
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
    // M3: where the strip's MAIN output lands. kOutputToMaster (the default) is the historical
    // straight-to-master path; any other value is an index into `buses`, and the strip's post-meter
    // output feeds THAT bus's sum instead of the master sum (a submix group, not a parallel send).
    static constexpr std::size_t kOutputToMaster = static_cast<std::size_t> (-1);
    std::size_t outputBusIndex = kOutputToMaster;
};

struct MixerBusProjection
{
    NodeId sumNodeId   = 0;
    NodeId panNodeId   = 0;
    NodeId meterNodeId = 0;
    float  pan         = 0.0f;   // centre by default; equal-power, like a Track Return (ADR-0014)
    std::vector<std::unique_ptr<Node>> insertNodes;
    NodeId faderNodeId = 0;
    float  linearGain  = 1.0f;
    // R13: buses route and send like tracks — parallel send taps into OTHER projected buses,
    // and a MAIN output that lands on another bus's sum instead of master. The projection is
    // DAG-validated upstream (edit verbs + bundle open); the graph build wires buses in
    // topological order and refuses a cycle honestly if one ever reaches it. The explicit `{}`
    // keeps existing aggregate inits clean under -Wmissing-field-initializers (NSDMI exempt).
    std::vector<MixerSendProjection> sends {};
    static constexpr std::size_t kOutputToMaster = static_cast<std::size_t> (-1);
    std::size_t outputBusIndex = kOutputToMaster;
};

// ADR-0051: one Compressor key — the Compressor insert's node, and the fader of the strip whose
// pre-fader signal (after its inserts) keys it. The build wires the edge once every strip exists.
struct MixerSidechainKey
{
    NodeId compressorNodeId = 0;
    NodeId sourceFaderNodeId = 0;
};

struct MixerProjectionInputs
{
    GraphId id = 0;
    NodeId  masterSumNodeId = GraphBuilder::kDefaultMasterNodeId - 1u;
    NodeId  masterNodeId    = GraphBuilder::kDefaultMasterNodeId;
    double  sampleRate      = 48000.0;
    int     maxBlockSize    = 512;
    // E19: persisted master strip gain, applied between the final sum and the master stage.
    float   masterLinearGain = 1.0f;
    // R11: the master strip's FX chain, wired between the master sum and the master fader
    // (pre-fader, matching the track/bus insert order).
    std::vector<std::unique_ptr<Node>> masterInsertNodes;
    const CompiledGraph* previousForCarryOver = nullptr;
    std::vector<MixerTrackProjection> tracks;
    std::vector<MixerBusProjection> buses;
    std::vector<CompiledAutomationLane> automationLanes;
    std::vector<MixerSidechainKey> sidechainKeys {};   // ADR-0051

    // ADR-0058 cp3: an export stem. Only this top-level strip reaches the master sum audibly — every other master
    // input rides a zero-gain fader, so the sum's delay compensation is the mix's — the master's inserts give way to
    // a pure delay of their latency, and the master fader is unity. An index past the end selects nothing (silence).
    struct StemTap
    {
        bool isBus = false;
        std::size_t index = 0;
        std::uint32_t salt = 0;   // varies the helper nodes' ids (a retry after an id collision)
    };
    std::optional<StemTap> stemTap {};

    // E19/R12: the master fader sits ONE node id below the master sum — one law shared by the
    // graph build below and the live scalar lane's control-side addressing (UiAppModel).
    [[nodiscard]] static constexpr NodeId masterFaderNodeIdFor (NodeId masterSumNodeId) noexcept
    {
        return masterSumNodeId - 1u;
    }
};

// ADR-0058 cp3: the zero-gain fader a stem render puts between a non-selected strip's meter and the master sum.
[[nodiscard]] inline NodeId mixerStemMuteNodeId (NodeId meterNodeId, std::uint32_t salt = 0) noexcept
{
    std::uint32_t h = 2166136261u;
    for (const std::uint32_t value : { static_cast<std::uint32_t> (meterNodeId), 0x5354454Du, 0xA15C0DEu, salt })
    {
        h ^= value;
        h *= 16777619u;
    }
    return h == 0u ? 1u : h;
}

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

// ADR-0042: a mono tap feeding a stereo Bus passes through a fixed centre widen stage so its centred
// loudness matches what the mono-bus return path produces. The widen node's id is derived from the tap
// and the bus, like the hashed send-level ids.
[[nodiscard]] inline NodeId mixerBusMonoWidenNodeId (NodeId tapNodeId, NodeId busSumNodeId) noexcept
{
    std::uint32_t h = 2166136261u;
    const auto mix = [&h] (std::uint32_t value) noexcept
    {
        h ^= value;
        h *= 16777619u;
    };

    mix (tapNodeId);
    mix (busSumNodeId);
    mix (0xB05713D0u);

    return h == 0u ? 1u : h;
}

[[nodiscard]] inline NodeId mixerBusFaderNodeId (NodeId busSumNodeId) noexcept
{
    std::uint32_t h = 2166136261u;
    const auto mix = [&h] (std::uint32_t value) noexcept
    {
        h ^= value;
        h *= 16777619u;
    };

    mix (busSumNodeId);
    mix (0xB05FAD3u);

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

    // ADR-0058 cp3: a stem render has no master inserts and a fixed unity master fader, so nothing may automate or
    // key them; their combined latency becomes a pure delay after the sum (stems line up with the mix file).
    std::int64_t stemMasterLatency = 0;
    if (projection.stemTap.has_value())
    {
        std::vector<NodeId> removed { MixerProjectionInputs::masterFaderNodeIdFor (projection.masterSumNodeId) };
        for (std::unique_ptr<Node>& insertNode : projection.masterInsertNodes)
            if (insertNode != nullptr)
            {
                insertNode->prepare (projection.sampleRate, projection.maxBlockSize);   // its latency at this rate
                stemMasterLatency += std::max<std::int64_t> (0, insertNode->properties().latencySamples);
                removed.push_back (insertNode->properties().id);
            }
        const auto isRemoved = [&removed] (NodeId id) { return std::find (removed.begin(), removed.end(), id) != removed.end(); };
        std::erase_if (projection.automationLanes, [&isRemoved] (const CompiledAutomationLane& lane) { return isRemoved (lane.targetNode); });
        std::erase_if (projection.sidechainKeys, [&isRemoved] (const MixerSidechainKey& key) { return isRemoved (key.compressorNodeId); });
        projection.masterInsertNodes.clear();
        projection.masterLinearGain = 1.0f;
    }
    const auto stemSilences = [&projection] (bool isBus, std::size_t index) {
        return projection.stemTap.has_value() && (projection.stemTap->isBus != isBus || projection.stemTap->index != index);
    };

    GraphBuilder::Inputs inputs;
    inputs.id = projection.id;
    inputs.masterNodeId = projection.masterNodeId;
    inputs.sampleRate = projection.sampleRate;
    inputs.maxBlockSize = projection.maxBlockSize;
    inputs.previousForCarryOver = projection.previousForCarryOver;
    inputs.automationLanes = std::move (projection.automationLanes);

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
    inputs.nodes.reserve (projection.tracks.size() * 7u + supportNodeCount + insertNodeCount + sendNodeCount + projection.buses.size() * 5u + 2u);

    std::vector<Node*> masterBusInputs;
    std::vector<std::pair<NodeId, Node*>> preFaderTaps;   // ADR-0051: fader id -> the node feeding it
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

        // ADR-0042: a Track source is mono or stereo. The strip runs at the source's width; the pan
        // slot widens a mono strip (equal-power Pan) or balances a stereo strip.
        const NodeProperties sourceProps = track.source->properties();
        if (! sourceProps.producesAudio || sourceProps.channels < 1 || sourceProps.channels > 2)
        {
            if (error != nullptr)
            {
                error->code = MixerProjectionError::Code::UnsupportedTrackSource;
                error->trackIndex = i;
            }
            return nullptr;
        }

        const int stripWidth = sourceProps.channels;
        const PanNode::Mode stripPanMode = stripWidth == 2 ? PanNode::Mode::Balance : PanNode::Mode::Widen;

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

            sidechain = std::make_unique<SidechainGainNode> (track.sidechainNodeId, stripWidth);
            sidechain->setMainInput (sourcePtr);
            sidechain->setSidechainInput (track.sidechainSource.get());
            chainHead = sidechain.get();
        }

        std::unique_ptr<PanNode> pan;
        PanNode* panPtr = nullptr;
        int chainChannels = sourceProps.channels;

        if (! track.insertNodes.empty())
        {
            pan = std::make_unique<PanNode> (track.panNodeId, stripPanMode);
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
        preFaderTaps.push_back ({ track.faderNodeId, chainHead });   // ADR-0051: a key's tap point
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
            pan = std::make_unique<PanNode> (track.panNodeId, stripPanMode);
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

        // M3: a routed strip feeds its destination bus's sum instead of the master sum. Buses never
        // feed tracks, so this cannot make a cycle; an out-of-range index is an honest refusal.
        if (track.outputBusIndex == MixerTrackProjection::kOutputToMaster)
        {
            if (stemSilences (false, i))   // ADR-0058 cp3: in the graph (its alignment holds), silent at the sum
            {
                auto mute = std::make_unique<FaderNode> (mixerStemMuteNodeId (track.meterNodeId, projection.stemTap->salt),
                                                         std::max (1, meterPtr->properties().channels));
                mute->setInput (meterPtr);
                mute->setTargetGain (0.0f);
                masterBusInputs.push_back (mute.get());
                inputs.nodes.push_back (std::move (mute));
            }
            else
            {
                masterBusInputs.push_back (meterPtr);
            }
        }
        else if (track.outputBusIndex < projection.buses.size())
        {
            pushUniqueMixerInput (busInputs[track.outputBusIndex], meterPtr);
        }
        else
        {
            if (error != nullptr)
            {
                error->code = MixerProjectionError::Code::InvalidSendDestination;
                error->trackIndex = i;
            }
            return nullptr;
        }
    }

    // R13: buses feed buses now, and each bus's Sum CONSUMES its input list when it is built —
    // so wire projected buses in TOPOLOGICAL order (every source before its destination). The
    // routing is DAG-validated upstream (edit verbs + bundle-open); if a cycle ever reaches
    // here anyway, Kahn's queue starves and the build refuses honestly instead of consuming a
    // half-wired input list.
    std::vector<std::size_t> busOrder;
    {
        const std::size_t busCount = projection.buses.size();
        std::vector<std::size_t> indegree (busCount, 0);
        for (const MixerBusProjection& bus : projection.buses)
        {
            for (const MixerSendProjection& send : bus.sends)
                if (send.busIndex < busCount)
                    ++indegree[send.busIndex];
            if (bus.outputBusIndex != MixerBusProjection::kOutputToMaster && bus.outputBusIndex < busCount)
                ++indegree[bus.outputBusIndex];
        }

        std::vector<std::size_t> ready;
        for (std::size_t i = 0; i < busCount; ++i)
            if (indegree[i] == 0)
                ready.push_back (i);

        busOrder.reserve (busCount);
        while (! ready.empty())
        {
            const std::size_t i = ready.back();
            ready.pop_back();
            busOrder.push_back (i);

            const MixerBusProjection& bus = projection.buses[i];
            for (const MixerSendProjection& send : bus.sends)
                if (send.busIndex < busCount && --indegree[send.busIndex] == 0)
                    ready.push_back (send.busIndex);
            if (bus.outputBusIndex != MixerBusProjection::kOutputToMaster
                && bus.outputBusIndex < busCount
                && --indegree[bus.outputBusIndex] == 0)
                ready.push_back (bus.outputBusIndex);
        }

        if (busOrder.size() != busCount)
        {
            if (error != nullptr)
                error->code = MixerProjectionError::Code::GraphBuildFailed;   // a routing cycle
            return nullptr;
        }
    }

    for (const std::size_t i : busOrder)
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

        if (! mixerGainIsValid (bus.linearGain))
        {
            if (error != nullptr)
            {
                error->code = MixerProjectionError::Code::InvalidBusGain;
                error->busIndex = i;
            }
            return nullptr;
        }

        // Bus width derives from its taps (ADR-0042): all-mono taps keep the original mono Sum whose
        // Return widens to centred stereo through its own Pan -> Meter (ADR-0014) — that path is
        // untouched and bit-identical. Any stereo tap makes the Bus stereo: the Sum runs at width 2,
        // mono taps pass through a fixed centre widen stage, and the Return's pan slot runs in Balance
        // mode. A mono Return summed straight into the stereo master would be audible in the left
        // channel only — a mono signal into a stereo master must be centred.
        int busWidth = 1;
        for (Node* const tap : busInputs[i])
            if (tap != nullptr && tap->properties().channels >= 2)
                busWidth = 2;

        std::vector<std::unique_ptr<PanNode>> monoTapWideners;
        if (busWidth == 2)
        {
            for (Node*& tap : busInputs[i])
            {
                if (tap == nullptr || tap->properties().channels >= 2)
                    continue;

                auto widen = std::make_unique<PanNode> (
                    mixerBusMonoWidenNodeId (tap->properties().id, bus.sumNodeId), PanNode::Mode::Widen);
                widen->setInput (tap);
                widen->setPan (0.0f);
                tap = widen.get();
                monoTapWideners.push_back (std::move (widen));
            }
        }

        auto busSum = std::make_unique<SumNode> (bus.sumNodeId, busWidth);
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

        const NodeId busFaderId = bus.faderNodeId != 0u ? bus.faderNodeId : mixerBusFaderNodeId (bus.sumNodeId);
        const int busFaderChannels = busChainHead->properties().channels > 0 ? busChainHead->properties().channels : 1;
        auto busFader = std::make_unique<FaderNode> (busFaderId, busFaderChannels);
        FaderNode* const busFaderPtr = busFader.get();
        busFaderPtr->setInput (busChainHead);
        preFaderTaps.push_back ({ busFaderId, busChainHead });   // ADR-0051: a key's tap point
        busFaderPtr->setTargetGain (bus.linearGain);

        auto busPan = std::make_unique<PanNode> (bus.panNodeId,
                                                 busWidth == 2 ? PanNode::Mode::Balance : PanNode::Mode::Widen);
        PanNode* const busPanPtr = busPan.get();
        busPanPtr->setInput (busFaderPtr);
        busPanPtr->setPan (bus.pan);

        auto busMeter = std::make_unique<MeterNode> (bus.meterNodeId, 2);
        MeterNode* const busMeterPtr = busMeter.get();
        busMeterPtr->setInput (busPanPtr);

        // R13: this bus's own parallel sends — the track send law verbatim: pre-fader taps the
        // post-insert chain head, post-fader taps the bus fader, each send is its own FaderNode
        // feeding the destination bus's (not-yet-consumed, thanks to topo order) input list.
        std::vector<std::unique_ptr<FaderNode>> busSendFaders;
        busSendFaders.reserve (bus.sends.size());
        for (std::size_t sendIndex = 0; sendIndex < bus.sends.size(); ++sendIndex)
        {
            const MixerSendProjection& send = bus.sends[sendIndex];
            if (send.busIndex >= projection.buses.size() || send.busIndex == i)
            {
                if (error != nullptr)
                {
                    error->code = MixerProjectionError::Code::InvalidSendDestination;
                    error->busIndex = i;
                    error->sendIndex = sendIndex;
                }
                return nullptr;
            }

            if (! mixerGainIsValid (send.linearGain))
            {
                if (error != nullptr)
                {
                    error->code = MixerProjectionError::Code::InvalidSendGain;
                    error->busIndex = i;
                    error->sendIndex = sendIndex;
                }
                return nullptr;
            }

            Node* const tap = send.tap == MixerSendTap::PreFader ? busChainHead : static_cast<Node*> (busFaderPtr);
            const int sendChannels = tap->properties().channels > 0 ? tap->properties().channels : 1;
            auto sendFader = std::make_unique<FaderNode> (send.faderNodeId, sendChannels);
            sendFader->setInput (tap);
            sendFader->setTargetGain (send.linearGain);
            pushUniqueMixerInput (busInputs[send.busIndex], sendFader.get());
            busSendFaders.push_back (std::move (sendFader));
        }

        for (std::unique_ptr<PanNode>& widen : monoTapWideners)
            inputs.nodes.push_back (std::move (widen));
        inputs.nodes.push_back (std::move (busSum));
        for (std::unique_ptr<Node>& insertNode : bus.insertNodes)
            inputs.nodes.push_back (std::move (insertNode));
        inputs.nodes.push_back (std::move (busFader));
        for (std::unique_ptr<FaderNode>& sendFader : busSendFaders)
            inputs.nodes.push_back (std::move (sendFader));
        inputs.nodes.push_back (std::move (busPan));
        inputs.nodes.push_back (std::move (busMeter));

        // R13: the bus's MAIN output — master (the historical path) or another bus's sum.
        if (bus.outputBusIndex == MixerBusProjection::kOutputToMaster)
        {
            if (stemSilences (true, i))   // ADR-0058 cp3
            {
                auto mute = std::make_unique<FaderNode> (mixerStemMuteNodeId (bus.meterNodeId, projection.stemTap->salt),
                                                         std::max (1, busMeterPtr->properties().channels));
                mute->setInput (busMeterPtr);
                mute->setTargetGain (0.0f);
                masterBusInputs.push_back (mute.get());
                inputs.nodes.push_back (std::move (mute));
            }
            else
            {
                masterBusInputs.push_back (busMeterPtr);
            }
        }
        else if (bus.outputBusIndex < projection.buses.size() && bus.outputBusIndex != i)
        {
            pushUniqueMixerInput (busInputs[bus.outputBusIndex], busMeterPtr);
        }
        else
        {
            if (error != nullptr)
            {
                error->code = MixerProjectionError::Code::InvalidSendDestination;
                error->busIndex = i;
            }
            return nullptr;
        }
    }

    auto masterSum = std::make_unique<SumNode> (projection.masterSumNodeId, 2);
    SumNode* const masterSumPtr = masterSum.get();
    masterSum->setInputNodes (std::move (masterBusInputs));

    // R11: the master FX chain sits between the final sum and the master fader — the same
    // pre-fader insert order every track and bus strip uses.
    Node* masterChainHead = masterSumPtr;
    std::unique_ptr<DelayNode> stemLatency;   // ADR-0058 cp3: a stem's stand-in for the master inserts' latency
    if (stemMasterLatency > 0)
    {
        stemLatency = std::make_unique<DelayNode> (mixerStemMuteNodeId (projection.masterSumNodeId, projection.stemTap->salt),
                                                   stemMasterLatency, 2);
        stemLatency->setInput (masterChainHead);
        masterChainHead = stemLatency.get();
    }
    for (std::unique_ptr<Node>& insertNode : projection.masterInsertNodes)
    {
        if (insertNode == nullptr || ! setMixerInsertInput (*insertNode, masterChainHead))
        {
            if (error != nullptr)
                error->code = MixerProjectionError::Code::GraphBuildFailed;
            return nullptr;
        }

        const NodeProperties insertProps = insertNode->properties();
        if (! insertProps.producesAudio || insertProps.channels != 2)
        {
            if (error != nullptr)
                error->code = MixerProjectionError::Code::GraphBuildFailed;
            return nullptr;
        }

        masterChainHead = insertNode.get();
    }

    // E19: the persisted master gain rides a FaderNode between the final sum and the master
    // stage — one node id below the master sum, mirroring the sum/master id convention.
    auto masterFader = std::make_unique<FaderNode> (
        MixerProjectionInputs::masterFaderNodeIdFor (projection.masterSumNodeId), 2);
    FaderNode* const masterFaderPtr = masterFader.get();
    masterFaderPtr->setInput (masterChainHead);
    masterFaderPtr->setTargetGain (projection.masterLinearGain);

    auto master = std::make_unique<MasterNode> (projection.masterNodeId, 2);
    master->setInputNodes ({ masterFaderPtr });

    inputs.nodes.push_back (std::move (masterSum));
    if (stemLatency != nullptr)
        inputs.nodes.push_back (std::move (stemLatency));
    for (std::unique_ptr<Node>& insertNode : projection.masterInsertNodes)
        inputs.nodes.push_back (std::move (insertNode));
    inputs.nodes.push_back (std::move (masterFader));
    inputs.nodes.push_back (std::move (master));

    // ADR-0051: every Compressor key, now that every strip's pre-fader tap exists. A key whose
    // Compressor or source strip is not in the graph is a projection error, never a silent drop.
    for (const MixerSidechainKey& key : projection.sidechainKeys)
    {
        CompressorNode* compressor = nullptr;
        for (const std::unique_ptr<Node>& node : inputs.nodes)
            if (node != nullptr && node->properties().id == key.compressorNodeId)
            {
                compressor = dynamic_cast<CompressorNode*> (node.get());
                break;
            }
        Node* tap = nullptr;
        for (const auto& [faderNodeId, preFader] : preFaderTaps)
            if (faderNodeId == key.sourceFaderNodeId)
            {
                tap = preFader;
                break;
            }
        if (compressor == nullptr || tap == nullptr)
        {
            if (error != nullptr)
                error->code = MixerProjectionError::Code::GraphBuildFailed;
            return nullptr;
        }
        compressor->setSidechainInput (tap);
    }

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
