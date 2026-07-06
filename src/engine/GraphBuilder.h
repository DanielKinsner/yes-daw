// YES DAW — GraphBuilder (ADR-0007): control-thread compiler for immutable CompiledGraph snapshots.
//
// Pass 5 owns mute metadata/state, delay carry-over, and multi-input bind checks. Runtime scalar command
// routing is handled by CompiledGraph's id-index lookup.

#pragma once

#include "engine/CompiledGraph.h"
#include "engine/nodes/DelayNode.h"
#include "engine/nodes/DecodedClipNode.h"
#include "engine/nodes/DecodedMidiClipNode.h"
#include "engine/nodes/CompressorNode.h"
#include "engine/nodes/EqNode.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/FxDelayNode.h"
#include "engine/nodes/IdentityDcNode.h"
#include "engine/nodes/ImpulseInstrumentNode.h"
#include "engine/nodes/LimiterNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/MeterNode.h"
#include "engine/nodes/MidiEffectNode.h"
#include "engine/nodes/OscillatorNode.h"
#include "engine/nodes/PanNode.h"
#include "engine/nodes/PlaceholderNode.h"
#include "engine/nodes/ReverbNode.h"
#include "engine/nodes/SidechainGainNode.h"
#include "engine/plugin/PluginNode.h"
#include "engine/nodes/SumNode.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace yesdaw::engine {

struct GraphBuildError
{
    struct DuplicateNodeId   { NodeId nodeId = 0; };
    struct MissingNode       { NodeId nodeId = 0; };
    struct LatencyOutOfRange { NodeId nodeId = 0; };
    struct CyclicGraph       { NodeId nodeId = 0; };
    struct GraphTooLarge     { NodeId nodeId = 0; };
    struct PluginBlockSizeMismatch { NodeId nodeId = 0; };
    struct InvalidAutomationLane { NodeId nodeId = 0; };
    struct AutomationEventBudgetExceeded { NodeId nodeId = 0; };

    using Detail = std::variant<std::monostate, DuplicateNodeId, MissingNode, LatencyOutOfRange, CyclicGraph, GraphTooLarge, PluginBlockSizeMismatch, InvalidAutomationLane, AutomationEventBudgetExceeded>;

    enum class Code
    {
        None,
        DuplicateNodeId,
        MissingNode,
        LatencyOutOfRange,
        CyclicGraph,
        GraphTooLarge,
        PluginBlockSizeMismatch,
        InvalidAutomationLane,
        AutomationEventBudgetExceeded
    };

    Detail detail;

    GraphBuildError() = default;
    GraphBuildError (DuplicateNodeId e)   : detail (e) {}
    GraphBuildError (MissingNode e)       : detail (e) {}
    GraphBuildError (LatencyOutOfRange e) : detail (e) {}
    GraphBuildError (CyclicGraph e)       : detail (e) {}
    GraphBuildError (GraphTooLarge e)     : detail (e) {}
    GraphBuildError (PluginBlockSizeMismatch e) : detail (e) {}
    GraphBuildError (InvalidAutomationLane e) : detail (e) {}
    GraphBuildError (AutomationEventBudgetExceeded e) : detail (e) {}

    Code code() const noexcept
    {
        switch (detail.index())
        {
            case 1: return Code::DuplicateNodeId;
            case 2: return Code::MissingNode;
            case 3: return Code::LatencyOutOfRange;
            case 4: return Code::CyclicGraph;
            case 5: return Code::GraphTooLarge;
            case 6: return Code::PluginBlockSizeMismatch;
            case 7: return Code::InvalidAutomationLane;
            case 8: return Code::AutomationEventBudgetExceeded;
            default: return Code::None;
        }
    }

    NodeId nodeId() const noexcept
    {
        struct Visitor
        {
            NodeId operator() (std::monostate) const noexcept { return 0; }
            NodeId operator() (DuplicateNodeId e) const noexcept { return e.nodeId; }
            NodeId operator() (MissingNode e) const noexcept { return e.nodeId; }
            NodeId operator() (LatencyOutOfRange e) const noexcept { return e.nodeId; }
            NodeId operator() (CyclicGraph e) const noexcept { return e.nodeId; }
            NodeId operator() (GraphTooLarge e) const noexcept { return e.nodeId; }
            NodeId operator() (PluginBlockSizeMismatch e) const noexcept { return e.nodeId; }
            NodeId operator() (InvalidAutomationLane e) const noexcept { return e.nodeId; }
            NodeId operator() (AutomationEventBudgetExceeded e) const noexcept { return e.nodeId; }
        };

        return std::visit (Visitor{}, detail);
    }
};

class GraphBuilder final
{
public:
    static constexpr int          kMaxChannelsPerNode = 8;
    static constexpr std::int64_t kMaxLatencyCap      = 60ll * 192000ll;
    static constexpr NodeId       kDefaultMasterNodeId = 0xFFFF'FFFEu;
    static constexpr std::size_t  kMaxCompiledNodes    = static_cast<std::size_t> (kNoSlot) - 1u;
    static constexpr std::size_t  kMaxInputsPerNode    = static_cast<std::size_t> (std::numeric_limits<std::uint16_t>::max());

    struct Inputs
    {
        GraphId id           = 0;
        NodeId  masterNodeId = kDefaultMasterNodeId;
        double  sampleRate   = 48000.0;
        int     maxBlockSize = 512;
        const CompiledGraph* previousForCarryOver = nullptr;
        std::vector<CompiledAutomationLane> automationLanes;
        std::vector<std::unique_ptr<Node>> nodes;
    };

    static std::unique_ptr<CompiledGraph> build (Inputs&& inputs, GraphBuildError* error = nullptr)
    {
        if (error != nullptr)
            *error = GraphBuildError{};

        if (inputs.nodes.empty())
            inputs.nodes.push_back (std::make_unique<MasterNode> (inputs.masterNodeId, 1));

        std::vector<NodeInfo> infos;
        infos.reserve (inputs.nodes.size());

        std::unordered_map<NodeId, std::size_t> idToIndex;
        idToIndex.reserve (inputs.nodes.size());

        std::unordered_map<Node*, std::size_t> ptrToIndex;
        ptrToIndex.reserve (inputs.nodes.size());

        for (std::size_t i = 0; i < inputs.nodes.size(); ++i)
        {
            Node* const node = inputs.nodes[i].get();
            if (node == nullptr)
                return fail (error, GraphBuildError::MissingNode { 0 });

            const NodeProperties props = node->properties();
            const auto inserted = idToIndex.emplace (props.id, i);
            if (! inserted.second)
                return fail (error, GraphBuildError::DuplicateNodeId { props.id });

            if (props.latencySamples < 0 || props.latencySamples > kMaxLatencyCap)
                return fail (error, GraphBuildError::LatencyOutOfRange { props.id });

            ptrToIndex.emplace (node, i);

            NodeInfo info;
            info.node     = node;
            info.props    = props;
            info.channels = clampChannels (props.channels);
            info.kind     = detectKind (*node);
            info.isDelay  = info.kind == CompiledNodeKind::Delay || info.kind == CompiledNodeKind::Latency;
            infos.push_back (info);
        }

        for (std::size_t i = 0; i < infos.size(); ++i)
        {
            const std::span<Node* const> direct = infos[i].node->directInputs();
            if (direct.size() > kMaxInputsPerNode)
                return fail (error, GraphBuildError::GraphTooLarge { infos[i].props.id });

            infos[i].inputs.reserve (direct.size());

            for (Node* const in : direct)
            {
                if (in == nullptr)
                    return fail (error, GraphBuildError::MissingNode { infos[i].props.id });

                const auto found = ptrToIndex.find (in);
                if (found == ptrToIndex.end())
                    return fail (error, GraphBuildError::MissingNode { in->properties().id });

                infos[i].inputs.push_back (found->second);
            }
        }

        const auto masterFound = idToIndex.find (inputs.masterNodeId);
        if (masterFound == idToIndex.end())
            return fail (error, GraphBuildError::MissingNode { inputs.masterNodeId });

        std::vector<std::size_t> order;
        if (! topoFromMaster (infos, masterFound->second, order, error))
            return nullptr;
        if (order.size() > kMaxCompiledNodes)
            return fail (error, GraphBuildError::GraphTooLarge { inputs.masterNodeId });

        std::vector<CompileItem> compileItems;
        std::size_t masterItemIdx = 0;
        if (! buildPdcPlan (infos, order, masterFound->second, inputs.nodes, compileItems, masterItemIdx, error))
            return nullptr;

        if (compileItems.size() > kMaxCompiledNodes)
            return fail (error, GraphBuildError::GraphTooLarge { inputs.masterNodeId });

        CompiledGraph::Payload payload;
        payload.id           = inputs.id;
        payload.identityDc   = 0.0f;
        payload.totalLatency = compileItems[masterItemIdx].pathLatency;
        payload.nodeStorage = std::move (inputs.nodes);
        payload.automationLanes = std::move (inputs.automationLanes);
        const int maxBlockSize = inputs.maxBlockSize > 0 ? inputs.maxBlockSize : 1;
        if (! validateCompiledAutomationLanes (payload.automationLanes, idToIndex, error))
            return nullptr;
        if (! validateAutomationEventBudget (payload.automationLanes, maxBlockSize, error))
            return nullptr;

        if (! buildCompiledMetadata (compileItems, maxBlockSize, payload, error))
            return nullptr;
        if (! payload.automationLanes.empty())
        {
            payload.automationEventStorage = std::make_unique<Event[]> (payload.maxEventsPerBlock);
            payload.automationLaneCursors =
                std::make_unique<CompiledAutomationLaneCursor[]> (payload.automationLanes.size());
        }
        bindBusNodes (payload);
        assert (allMultiInputNodesBound (payload));

        // A PluginNode reports its one-Block IPC latency for a construction-time pipeline Block, but the
        // compiler reads properties() and locks PDC BEFORE prepare() learns the real maxBlockSize. So that
        // latency is only correct when the node's pipeline Block equals the graph's maxBlockSize; a mismatch
        // would be a silent, fixed phase error against every other path. Reject it loudly here (ADR-0015).
        for (const CompiledNode& cn : payload.compiledNodes)
            if (const auto* const plugin = dynamic_cast<const PluginNode*> (cn.node))
                if (plugin->pipelineBlockSamples() != maxBlockSize)
                    return fail (error, GraphBuildError::PluginBlockSizeMismatch { cn.id });

        for (CompiledNode& cn : payload.compiledNodes)
            if (cn.node != nullptr)
                cn.node->prepare (inputs.sampleRate, maxBlockSize);

        carryOverDelayState (payload, inputs.previousForCarryOver);

        // ADR-0027: the graph is block-parallel-safe only if EVERY compiled node (including any PDC
        // LatencyNode the splice inserted) is order-independent, and there is no path latency (a delay/
        // latency ring is inherently cross-Block state). Default-false node props mean a new/stateful node
        // flips this off, so the parallel scheduler refuses it instead of mis-rendering it.
        bool blockParallelSafe = payload.totalLatency == 0;
        for (const CompiledNode& cn : payload.compiledNodes)
            if (cn.node != nullptr && ! cn.node->properties().blockParallelSafe)
                blockParallelSafe = false;
        if (! payload.automationLanes.empty())
            blockParallelSafe = false;
        payload.blockParallelSafe = blockParallelSafe;

        return std::make_unique<CompiledGraph> (std::move (payload));
    }

private:
    struct NodeInfo
    {
        Node* node = nullptr;
        NodeProperties props;
        int channels = 1;
        CompiledNodeKind kind = CompiledNodeKind::Plugin;
        bool isDelay = false;
        std::vector<std::size_t> inputs;
    };

    struct DfsFrame
    {
        std::size_t nodeIdx = 0;
        std::size_t nextInput = 0;
    };

    struct CompileItem
    {
        Node* node = nullptr;
        NodeProperties props;
        int channels = 1;
        CompiledNodeKind kind = CompiledNodeKind::Plugin;
        std::int64_t pathLatency = 0;
        DelayCacheKey delayCacheKey = 0;
        std::vector<std::size_t> inputs;
    };

    static std::unique_ptr<CompiledGraph> fail (GraphBuildError* error, GraphBuildError value)
    {
        if (error != nullptr)
            *error = value;
        return nullptr;
    }

    static bool validateCompiledAutomationLanes (
        const std::vector<CompiledAutomationLane>& lanes,
        const std::unordered_map<NodeId, std::size_t>& idToIndex,
        GraphBuildError* error)
    {
        for (const CompiledAutomationLane& lane : lanes)
        {
            if (idToIndex.find (lane.targetNode) == idToIndex.end()
                || lane.frames.size() != lane.values.size()
                || lane.frames.size() != lane.curveTypes.size())
                return failBool (error, GraphBuildError::InvalidAutomationLane { lane.targetNode });

            std::int64_t previousFrame = 0;
            bool havePrevious = false;
            for (std::size_t i = 0; i < lane.frames.size(); ++i)
            {
                const std::int64_t frame = lane.frames[i];
                const double value = lane.values[i];
                if (frame < 0 || (havePrevious && frame <= previousFrame)
                    || ! std::isfinite (value) || value < 0.0 || value > 1.0
                    || (lane.curveTypes[i] != AutomationCurveType::Linear
                        && lane.curveTypes[i] != AutomationCurveType::Hold))
                    return failBool (error, GraphBuildError::InvalidAutomationLane { lane.targetNode });

                previousFrame = frame;
                havePrevious = true;
            }
        }

        return true;
    }

    [[nodiscard]] static constexpr std::size_t automationEventsPerLaneForBlock (int maxBlockSize) noexcept
    {
        const std::size_t block = static_cast<std::size_t> (maxBlockSize > 0 ? maxBlockSize : 1);
        return block / 64u + 2u;
    }

    static bool validateAutomationEventBudget (
        const std::vector<CompiledAutomationLane>& lanes,
        int maxBlockSize,
        GraphBuildError* error)
    {
        const std::size_t perLane = automationEventsPerLaneForBlock (maxBlockSize);
        const std::size_t capacity = CompiledGraph::kMaxEventsPerBlock;
        if (perLane != 0u && lanes.size() > capacity / perLane)
        {
            const NodeId target = lanes.empty() ? 0u : lanes.front().targetNode;
            return failBool (error, GraphBuildError::AutomationEventBudgetExceeded { target });
        }

        return true;
    }

    static int clampChannels (int channels) noexcept
    {
        if (channels < 1)
            return 1;
        if (channels > kMaxChannelsPerNode)
            return kMaxChannelsPerNode;
        return channels;
    }

    static CompiledNodeKind detectKind (Node& node) noexcept
    {
        if (dynamic_cast<IdentityDcNode*> (&node) != nullptr)
            return CompiledNodeKind::IdentityDc;
        if (dynamic_cast<ImpulseInstrumentNode*> (&node) != nullptr)
            return CompiledNodeKind::Source;
        if (dynamic_cast<OscillatorNode*> (&node) != nullptr)
            return CompiledNodeKind::Oscillator;
        if (dynamic_cast<DecodedClipNode*> (&node) != nullptr)
            return CompiledNodeKind::Source;
        if (dynamic_cast<FaderNode*> (&node) != nullptr)
            return CompiledNodeKind::Fader;
        if (dynamic_cast<PanNode*> (&node) != nullptr)
            return CompiledNodeKind::Pan;
        if (dynamic_cast<SumNode*> (&node) != nullptr)
            return CompiledNodeKind::Sum;
        if (dynamic_cast<MeterNode*> (&node) != nullptr)
            return CompiledNodeKind::Meter;
        if (dynamic_cast<DelayNode*> (&node) != nullptr)
            return CompiledNodeKind::Delay;
        if (dynamic_cast<MasterNode*> (&node) != nullptr)
            return CompiledNodeKind::Master;
        if (dynamic_cast<SidechainGainNode*> (&node) != nullptr)
            return CompiledNodeKind::Sidechain;
        if (dynamic_cast<PlaceholderNode*> (&node) != nullptr)
            return CompiledNodeKind::Placeholder;
        if (dynamic_cast<MidiTransposeNode*> (&node) != nullptr
            || dynamic_cast<MidiScaleMapNode*> (&node) != nullptr)
            return CompiledNodeKind::MidiEffect;
        if (dynamic_cast<DecodedMidiClipNode*> (&node) != nullptr)
            return CompiledNodeKind::MidiSource;
        if (dynamic_cast<EqNode*> (&node) != nullptr)
            return CompiledNodeKind::Eq;
        if (dynamic_cast<CompressorNode*> (&node) != nullptr)
            return CompiledNodeKind::Compressor;
        if (dynamic_cast<FxDelayNode*> (&node) != nullptr)
            return CompiledNodeKind::FxDelay;
        if (dynamic_cast<ReverbNode*> (&node) != nullptr)
            return CompiledNodeKind::Reverb;
        if (dynamic_cast<LimiterNode*> (&node) != nullptr)
            return CompiledNodeKind::Limiter;
        return CompiledNodeKind::Plugin;
    }

    static bool topoFromMaster (const std::vector<NodeInfo>& infos,
                                std::size_t masterIdx,
                                std::vector<std::size_t>& order,
                                GraphBuildError* error)
    {
        std::vector<std::uint8_t> colour (infos.size(), 0);
        std::vector<DfsFrame> stack;
        stack.reserve (infos.size());
        order.reserve (infos.size());

        colour[masterIdx] = 1;
        stack.push_back (DfsFrame { masterIdx, 0 });

        while (! stack.empty())
        {
            DfsFrame& frame = stack.back();
            const NodeInfo& current = infos[frame.nodeIdx];

            if (frame.nextInput < current.inputs.size())
            {
                const std::size_t inputIdx = current.inputs[frame.nextInput];
                ++frame.nextInput;

                if (colour[inputIdx] == 0)
                {
                    colour[inputIdx] = 1;
                    stack.push_back (DfsFrame { inputIdx, 0 });
                }
                else if (colour[inputIdx] == 1)
                {
                    if (! current.isDelay)
                    {
                        fail (error, GraphBuildError::CyclicGraph { current.props.id });
                        return false;
                    }
                }

                continue;
            }

            colour[frame.nodeIdx] = 2;
            order.push_back (frame.nodeIdx);
            stack.pop_back();
        }

        return true;
    }

    static bool buildPdcPlan (const std::vector<NodeInfo>& infos,
                              const std::vector<std::size_t>& order,
                              std::size_t masterIdx,
                              std::vector<std::unique_ptr<Node>>& ownedNodes,
                              std::vector<CompileItem>& items,
                              std::size_t& masterItemIdx,
                              GraphBuildError* error)
    {
        std::vector<std::size_t> originalToItem (infos.size(), kNoItem);
        items.reserve (order.size());

        for (const std::size_t nodeIdx : order)
        {
            const NodeInfo& info = infos[nodeIdx];

            std::vector<std::size_t> effectiveInputs;
            effectiveInputs.reserve (info.inputs.size());

            for (const std::size_t inputIdx : info.inputs)
            {
                const std::size_t itemIdx = originalToItem[inputIdx];
                if (itemIdx != kNoItem)
                    effectiveInputs.push_back (itemIdx);
            }

            std::int64_t targetLatency = 0;
            if (! effectiveInputs.empty())
            {
                targetLatency = items[effectiveInputs.front()].pathLatency;
                for (const std::size_t inputItemIdx : effectiveInputs)
                    if (items[inputItemIdx].pathLatency > targetLatency)
                        targetLatency = items[inputItemIdx].pathLatency;
            }

            if (effectiveInputs.size() >= 2u)
            {
                for (std::size_t& inputItemIdx : effectiveInputs)
                {
                    const std::int64_t inputLatency = items[inputItemIdx].pathLatency;
                    const std::int64_t delta = targetLatency - inputLatency;
                    if (delta == 0)
                        continue;

                    if (delta < 0 || delta > kMaxLatencyCap)
                        return failBool (error, GraphBuildError::LatencyOutOfRange { info.props.id });

                    inputItemIdx = appendLatencySplice (info, inputItemIdx, delta, ownedNodes, items);
                    if (items.size() > kMaxCompiledNodes)
                        return failBool (error, GraphBuildError::GraphTooLarge { info.props.id });
                }
            }

            std::int64_t pathLatency = info.props.latencySamples;
            if (! effectiveInputs.empty())
            {
                if (! safeAddI64 (targetLatency, info.props.latencySamples, pathLatency))
                    return failBool (error, GraphBuildError::LatencyOutOfRange { info.props.id });
            }

            CompileItem item;
            item.node        = info.node;
            item.props       = info.props;
            item.channels    = info.channels;
            item.kind        = info.kind;
            item.pathLatency = pathLatency;
            item.delayCacheKey = static_cast<DelayCacheKey> (info.props.id);
            item.inputs      = std::move (effectiveInputs);

            const std::size_t itemIdx = items.size();
            items.push_back (std::move (item));
            originalToItem[nodeIdx] = itemIdx;

            if (nodeIdx == masterIdx)
                masterItemIdx = itemIdx;

            if (items.size() > kMaxCompiledNodes)
                return failBool (error, GraphBuildError::GraphTooLarge { info.props.id });
        }

        return true;
    }

    static std::size_t appendLatencySplice (const NodeInfo& consumer,
                                            std::size_t inputItemIdx,
                                            std::int64_t delaySamples,
                                            std::vector<std::unique_ptr<Node>>& ownedNodes,
                                            std::vector<CompileItem>& items)
    {
        const CompileItem& producer = items[inputItemIdx];
        const DelayCacheKey spliceKey = syntheticLatencyKey (consumer.props.id, producer.delayCacheKey, items.size());
        const NodeId spliceId = nodeIdFromDelayCacheKey (spliceKey);

        auto latency = std::make_unique<LatencyNode> (spliceId, delaySamples, producer.channels);
        latency->setInput (producer.node);
        Node* const latencyPtr = latency.get();
        ownedNodes.push_back (std::move (latency));

        CompileItem splice;
        splice.node        = latencyPtr;
        splice.props       = latencyPtr->properties();
        splice.channels    = producer.channels;
        splice.kind        = CompiledNodeKind::Latency;
        splice.pathLatency = producer.pathLatency + delaySamples;
        splice.delayCacheKey = spliceKey;
        splice.inputs.push_back (inputItemIdx);

        const std::size_t spliceIdx = items.size();
        items.push_back (std::move (splice));
        return spliceIdx;
    }

    static DelayCacheKey syntheticLatencyKey (NodeId consumerId, DelayCacheKey producerKey, std::size_t ordinal) noexcept
    {
        std::uint64_t h = 14695981039346656037ull;
        h = fnv1a (h, static_cast<std::uint64_t> (consumerId));
        h = fnv1a (h, producerKey);
        h = fnv1a (h, static_cast<std::uint64_t> (ordinal));
        return 0x8000'0000'0000'0000ull | (h & 0x7FFF'FFFF'FFFF'FFFFull);
    }

    static NodeId nodeIdFromDelayCacheKey (DelayCacheKey key) noexcept
    {
        return static_cast<NodeId> (0x8000'0000u | (static_cast<NodeId> (key) & 0x7FFF'FFFFu));
    }

    static std::uint64_t fnv1a (std::uint64_t h, std::uint64_t value) noexcept
    {
        for (int i = 0; i < 8; ++i)
        {
            h ^= (value >> (i * 8)) & 0xFFu;
            h *= 1099511628211ull;
        }
        return h;
    }

    static bool safeAddI64 (std::int64_t a, std::int64_t b, std::int64_t& out) noexcept
    {
#if defined(__GNUC__) || defined(__clang__)
        return ! __builtin_add_overflow (a, b, &out);
#else
        if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b)
            || (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b))
            return false;

        out = a + b;
        return true;
#endif
    }

    static bool failBool (GraphBuildError* error, GraphBuildError value)
    {
        if (error != nullptr)
            *error = value;
        return false;
    }

    static bool buildCompiledMetadata (const std::vector<CompileItem>& items,
                                       int maxBlockSize,
                                       CompiledGraph::Payload& payload,
                                       GraphBuildError* error)
    {
        payload.compiledNodes.reserve (items.size());

        std::size_t totalInputs = 0;
        for (const CompileItem& item : items)
            totalInputs += item.inputs.size();
        payload.inputSlotIndices.reserve (totalInputs);

        std::uint16_t maxChannels = 1;
        for (const CompileItem& item : items)
        {
            const int ch = item.channels;
            if (ch > static_cast<int> (maxChannels))
                maxChannels = static_cast<std::uint16_t> (ch);
        }

        std::vector<std::size_t> lastReader (items.size(), kNoItem);
        for (std::size_t consumerIdx = 0; consumerIdx < items.size(); ++consumerIdx)
            for (const std::size_t producerIdx : items[consumerIdx].inputs)
                lastReader[producerIdx] = consumerIdx;

        std::vector<SlotIndex> outputSlots (items.size(), kNoSlot);
        std::vector<EventSlotIndex> eventOutputSlots (items.size(), kNoEventSlot);
        std::vector<std::vector<SlotIndex>> freeSlotsByChannels (static_cast<std::size_t> (maxChannels) + 1u);
        std::vector<std::size_t> releaseStamp (items.size(), 0);

        SlotIndex nextFloatSlot = 1; // slot 0 is permanent silence
        DSlotIndex nextDoubleSlot = 0;
        EventSlotIndex nextEventSlot = 1; // slot 0 is the caller-provided root Event stream

        for (std::size_t compiledIdx = 0; compiledIdx < items.size(); ++compiledIdx)
        {
            const CompileItem& item = items[compiledIdx];
            const std::uint16_t channels = static_cast<std::uint16_t> (item.channels);

            bool aliasOk = false;
            SlotIndex outputSlot = kNoSlot;

            if (canAliasInPlace (items, outputSlots, lastReader, compiledIdx))
            {
                outputSlot = outputSlots[item.inputs.front()];
                aliasOk = true;
            }
            else if (! allocateFloatSlot (freeSlotsByChannels, channels, nextFloatSlot, outputSlot))
            {
                return failBool (error, GraphBuildError::GraphTooLarge { item.props.id });
            }

            if (outputSlot == kSilenceSlot || outputSlot == kNoSlot)
                return failBool (error, GraphBuildError::GraphTooLarge { item.props.id });

            outputSlots[compiledIdx] = outputSlot;

            CompiledNode cn;
            cn.node        = item.node;
            cn.id          = item.props.id;
            cn.numChannels = channels;
            cn.inputsBegin = static_cast<std::uint32_t> (payload.inputSlotIndices.size());
            cn.outputSlot  = outputSlot;
            cn.eventInputSlot = eventInputSlotFor (item, eventOutputSlots);
            cn.pathLatency = item.pathLatency;
            cn.delayCacheKey = item.delayCacheKey;
            cn.muteBit     = static_cast<std::uint32_t> (compiledIdx);   // every compiled node is mute-capable (ADR-0016)
            cn.kind        = item.kind;
            cn.aliasOk     = aliasOk;

            if (item.props.producesEvents)
            {
                if (nextEventSlot == kNoEventSlot)
                    return failBool (error, GraphBuildError::GraphTooLarge { item.props.id });

                cn.eventOutputSlot = nextEventSlot;
                eventOutputSlots[compiledIdx] = nextEventSlot;
                ++nextEventSlot;
            }

            if (cn.kind == CompiledNodeKind::Sum || cn.kind == CompiledNodeKind::Master)
            {
                if (nextDoubleSlot == kNoSlot)
                    return failBool (error, GraphBuildError::GraphTooLarge { item.props.id });

                cn.busAccumSlot = nextDoubleSlot;
                ++nextDoubleSlot;
            }

            // Multi-input nodes record their inputs in a canonical producer-id order so the output is
            // bit-identical across recompiles regardless of declaration order (SumNode/Master re-sort too,
            // and summing is commutative). A Sidechain consumer is the exception: input 0 is the MAIN and
            // input 1 the sidechain, so order is semantic. Keep its declared (directInputs) order, which the
            // PDC pass preserves positionally even when it splices a LatencyNode onto the shorter path.
            std::vector<std::size_t> orderedInputs = item.inputs;
            if (orderedInputs.size() > 1u && item.kind != CompiledNodeKind::Sidechain)
            {
                std::sort (orderedInputs.begin(), orderedInputs.end(),
                           [&items] (std::size_t a, std::size_t b)
                           {
                               return items[a].props.id < items[b].props.id;
                           });
            }

            for (const std::size_t inputIdx : orderedInputs)
            {
                payload.inputSlotIndices.push_back (InputSlot {
                    outputSlots[inputIdx],
                    static_cast<std::uint16_t> (inputIdx) });
            }

            cn.numInputs = static_cast<std::uint16_t> (payload.inputSlotIndices.size() - cn.inputsBegin);
            if (cn.kind != CompiledNodeKind::Latency)
                payload.idIndex.push_back (std::make_pair (cn.id, static_cast<std::uint32_t> (compiledIdx)));

            if (cn.kind == CompiledNodeKind::Master)
            {
                payload.masterOutputSlot = cn.outputSlot;
                payload.masterChannels   = cn.numChannels;
            }

            payload.compiledNodes.push_back (cn);

            const std::size_t stamp = compiledIdx + 1u;
            for (const std::size_t inputIdx : item.inputs)
            {
                if (lastReader[inputIdx] != compiledIdx || releaseStamp[inputIdx] == stamp)
                    continue;

                releaseStamp[inputIdx] = stamp;

                const SlotIndex releasedSlot = outputSlots[inputIdx];
                if (releasedSlot != kNoSlot && releasedSlot != kSilenceSlot && releasedSlot != outputSlot)
                {
                    const std::uint16_t releasedChannels = static_cast<std::uint16_t> (items[inputIdx].channels);
                    freeSlotsByChannels[releasedChannels].push_back (releasedSlot);
                }
            }
        }

        std::sort (payload.idIndex.begin(), payload.idIndex.end(),
                   [] (const auto& a, const auto& b) { return a.first < b.first; });

        payload.poolLayout.maxBlockSize       = static_cast<std::uint32_t> (maxBlockSize);
        payload.poolLayout.maxChannelsPerSlot = maxChannels;
        payload.poolLayout.numFloatSlots      = nextFloatSlot;
        payload.poolLayout.numDoubleSlots     = nextDoubleSlot;
        payload.numEventSlots                 = nextEventSlot;
        payload.maxEventsPerBlock             = CompiledGraph::kMaxEventsPerBlock;

        allocateFloatStorage (payload);
        allocateDoubleStorage (payload);
        allocateEventStorage (payload);
        return true;
    }

    static EventSlotIndex eventInputSlotFor (const CompileItem& item,
                                             const std::vector<EventSlotIndex>& eventOutputSlots) noexcept
    {
        for (const std::size_t inputIdx : item.inputs)
        {
            const EventSlotIndex inputEventSlot = eventOutputSlots[inputIdx];
            if (inputEventSlot != kNoEventSlot)
                return inputEventSlot;
        }

        return kRootEventSlot;
    }

    static bool allocateFloatSlot (std::vector<std::vector<SlotIndex>>& freeSlotsByChannels,
                                   std::uint16_t channels,
                                   SlotIndex& nextFloatSlot,
                                   SlotIndex& out) noexcept
    {
        std::vector<SlotIndex>& bucket = freeSlotsByChannels[channels];
        if (! bucket.empty())
        {
            out = bucket.back();
            bucket.pop_back();
            return true;
        }

        if (nextFloatSlot == kNoSlot)
            return false;

        out = nextFloatSlot;
        ++nextFloatSlot;
        return true;
    }

    static bool canAliasInPlace (const std::vector<CompileItem>& items,
                                 const std::vector<SlotIndex>& outputSlots,
                                 const std::vector<std::size_t>& lastReader,
                                 std::size_t itemIdx) noexcept
    {
        const CompileItem& item = items[itemIdx];
        if (item.inputs.size() != 1u)
            return false;
        if (item.kind != CompiledNodeKind::Fader && item.kind != CompiledNodeKind::Meter)
            return false;

        const std::size_t producerIdx = item.inputs.front();
        const CompileItem& producer = items[producerIdx];
        if (lastReader[producerIdx] != itemIdx)
            return false;
        if (producer.channels != item.channels)
            return false;
        if (producer.kind == CompiledNodeKind::Sum || item.kind == CompiledNodeKind::Sum)
            return false;

        const SlotIndex producerSlot = outputSlots[producerIdx];
        return producerSlot != kNoSlot && producerSlot != kSilenceSlot;
    }

    static void allocateFloatStorage (CompiledGraph::Payload& payload)
    {
        const std::size_t totalSamples = static_cast<std::size_t> (payload.poolLayout.numFloatSlots)
                                       * static_cast<std::size_t> (payload.poolLayout.maxChannelsPerSlot)
                                       * static_cast<std::size_t> (payload.poolLayout.maxBlockSize);
        payload.floatStorage = std::make_unique<float[]> (totalSamples);
        for (std::size_t i = 0; i < totalSamples; ++i)
            payload.floatStorage[i] = 0.0f;

        payload.floatSlotPtrs.resize (static_cast<std::size_t> (payload.poolLayout.numFloatSlots)
                                      * static_cast<std::size_t> (payload.poolLayout.maxChannelsPerSlot),
                                      nullptr);

        for (std::uint16_t slot = 0; slot < payload.poolLayout.numFloatSlots; ++slot)
            for (std::uint16_t ch = 0; ch < payload.poolLayout.maxChannelsPerSlot; ++ch)
            {
                const std::size_t sampleOffset = (static_cast<std::size_t> (slot)
                                                  * static_cast<std::size_t> (payload.poolLayout.maxChannelsPerSlot)
                                                  + static_cast<std::size_t> (ch))
                                               * static_cast<std::size_t> (payload.poolLayout.maxBlockSize);
                payload.floatSlotPtrs[static_cast<std::size_t> (slot)
                                      * static_cast<std::size_t> (payload.poolLayout.maxChannelsPerSlot)
                                      + static_cast<std::size_t> (ch)] = payload.floatStorage.get() + sampleOffset;
            }
    }

    static void allocateDoubleStorage (CompiledGraph::Payload& payload)
    {
        if (payload.poolLayout.numDoubleSlots == 0)
            return;

        const std::size_t totalSamples = static_cast<std::size_t> (payload.poolLayout.numDoubleSlots)
                                       * static_cast<std::size_t> (payload.poolLayout.maxChannelsPerSlot)
                                       * static_cast<std::size_t> (payload.poolLayout.maxBlockSize);
        payload.doubleStorage = std::make_unique<double[]> (totalSamples);
        for (std::size_t i = 0; i < totalSamples; ++i)
            payload.doubleStorage[i] = 0.0;

        payload.doubleSlotPtrs.resize (static_cast<std::size_t> (payload.poolLayout.numDoubleSlots)
                                       * static_cast<std::size_t> (payload.poolLayout.maxChannelsPerSlot),
                                       nullptr);

        for (std::uint16_t slot = 0; slot < payload.poolLayout.numDoubleSlots; ++slot)
            for (std::uint16_t ch = 0; ch < payload.poolLayout.maxChannelsPerSlot; ++ch)
            {
                const std::size_t sampleOffset = (static_cast<std::size_t> (slot)
                                                  * static_cast<std::size_t> (payload.poolLayout.maxChannelsPerSlot)
                                                  + static_cast<std::size_t> (ch))
                                               * static_cast<std::size_t> (payload.poolLayout.maxBlockSize);
                payload.doubleSlotPtrs[static_cast<std::size_t> (slot)
                                       * static_cast<std::size_t> (payload.poolLayout.maxChannelsPerSlot)
                                       + static_cast<std::size_t> (ch)] = payload.doubleStorage.get() + sampleOffset;
            }
    }

    static void allocateEventStorage (CompiledGraph::Payload& payload)
    {
        const std::size_t numEventSlots = static_cast<std::size_t> (payload.numEventSlots);
        payload.eventSlotPtrs.resize (numEventSlots, nullptr);

        if (payload.numEventSlots <= 1)
            return;

        payload.eventSlotCounts = std::make_unique<std::uint32_t[]> (numEventSlots);
        for (std::size_t i = 0; i < numEventSlots; ++i)
            payload.eventSlotCounts[i] = 0;

        const std::size_t eventCapacity = static_cast<std::size_t> (payload.maxEventsPerBlock);
        const std::size_t totalEvents = (numEventSlots - 1u) * eventCapacity;
        payload.eventStorage = std::make_unique<Event[]> (totalEvents);

        for (std::uint16_t slot = 1; slot < payload.numEventSlots; ++slot)
        {
            const std::size_t eventOffset = (static_cast<std::size_t> (slot) - 1u) * eventCapacity;
            payload.eventSlotPtrs[slot] = payload.eventStorage.get() + eventOffset;
        }
    }

    static void bindBusNodes (CompiledGraph::Payload& payload)
    {
        for (CompiledNode& cn : payload.compiledNodes)
        {
            if (cn.kind == CompiledNodeKind::Sum)
            {
                if (SumNode* sum = dynamic_cast<SumNode*> (cn.node))
                    sum->bindInputs (sumInputsFor (payload, cn));
            }
            else if (cn.kind == CompiledNodeKind::Master)
            {
                if (MasterNode* master = dynamic_cast<MasterNode*> (cn.node))
                    master->bindInputs (masterInputsFor (payload, cn));
            }
            else if (cn.kind == CompiledNodeKind::Sidechain)
            {
                if (SidechainGainNode* sc = dynamic_cast<SidechainGainNode*> (cn.node))
                    sc->bindInputs (sidechainInputsFor (payload, cn));
            }
        }
    }

    static bool allMultiInputNodesBound (const CompiledGraph::Payload& payload) noexcept
    {
        for (const CompiledNode& cn : payload.compiledNodes)
        {
            if (cn.kind == CompiledNodeKind::Sum)
            {
                const SumNode* const sum = dynamic_cast<const SumNode*> (cn.node);
                if (sum == nullptr || ! sum->isBound())
                    return false;
            }
            else if (cn.kind == CompiledNodeKind::Master)
            {
                const MasterNode* const master = dynamic_cast<const MasterNode*> (cn.node);
                if (master == nullptr || ! master->isBound())
                    return false;
            }
            else if (cn.kind == CompiledNodeKind::Sidechain)
            {
                const SidechainGainNode* const sc = dynamic_cast<const SidechainGainNode*> (cn.node);
                if (sc == nullptr || ! sc->isBound())
                    return false;
            }
            else if (cn.numInputs > 1u)
            {
                return false;
            }
        }

        return true;
    }

    static void carryOverDelayState (CompiledGraph::Payload& payload, const CompiledGraph* previous)
    {
        if (previous == nullptr)
            return;

        previous->snapshotDelayCache();
        const std::span<const DelayCacheEntry> cache = previous->debugDelayCache();

        for (CompiledNode& cn : payload.compiledNodes)
        {
            if (cn.kind != CompiledNodeKind::Delay && cn.kind != CompiledNodeKind::Latency)
                continue;

            DelayNode* const delay = dynamic_cast<DelayNode*> (cn.node);
            const DelayCacheEntry* const entry = findDelayCacheEntry (cache, cn.delayCacheKey);
            if (delay != nullptr && entry != nullptr)
                delay->restoreState (std::span<const float> (entry->ring.data(), entry->ring.size()), entry->writePos);
        }
    }

    static const DelayCacheEntry* findDelayCacheEntry (std::span<const DelayCacheEntry> cache, DelayCacheKey key) noexcept
    {
        const auto it = std::lower_bound (cache.begin(), cache.end(), key,
                                          [] (const DelayCacheEntry& entry, DelayCacheKey value)
                                          {
                                              return entry.key < value;
                                          });
        if (it == cache.end() || it->key != key)
            return nullptr;

        return &(*it);
    }

    static std::vector<SumNode::Input> sumInputsFor (const CompiledGraph::Payload& payload, const CompiledNode& cn)
    {
        std::vector<SumNode::Input> inputs;
        inputs.reserve (cn.numInputs);

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t> (cn.numInputs); ++i)
        {
            const InputSlot& slot = payload.inputSlotIndices[static_cast<std::size_t> (cn.inputsBegin) + i];
            const CompiledNode& producer = payload.compiledNodes[slot.producerNodeIdx];

            SumNode::Input input;
            input.producerId = producer.id;
            fillInputChannels (payload, slot.fromSlot, producer.numChannels, input.channels);
            inputs.push_back (input);
        }

        return inputs;
    }

    static std::vector<MasterNode::Input> masterInputsFor (const CompiledGraph::Payload& payload, const CompiledNode& cn)
    {
        std::vector<MasterNode::Input> inputs;
        inputs.reserve (cn.numInputs);

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t> (cn.numInputs); ++i)
        {
            const InputSlot& slot = payload.inputSlotIndices[static_cast<std::size_t> (cn.inputsBegin) + i];
            const CompiledNode& producer = payload.compiledNodes[slot.producerNodeIdx];

            MasterNode::Input input;
            input.producerId = producer.id;
            fillInputChannels (payload, slot.fromSlot, producer.numChannels, input.channels);
            inputs.push_back (input);
        }

        return inputs;
    }

    // Resolved inputs for a Sidechain consumer, in the SAME order they appear in inputSlotIndices. Because
    // the slot loop skips its producer-id sort for the Sidechain kind, that order is the declared
    // directInputs() order: index 0 is the main signal, index 1 the sidechain pin.
    static std::vector<SidechainGainNode::Input> sidechainInputsFor (const CompiledGraph::Payload& payload, const CompiledNode& cn)
    {
        std::vector<SidechainGainNode::Input> inputs;
        inputs.reserve (cn.numInputs);

        for (std::uint32_t i = 0; i < static_cast<std::uint32_t> (cn.numInputs); ++i)
        {
            const InputSlot& slot = payload.inputSlotIndices[static_cast<std::size_t> (cn.inputsBegin) + i];
            const CompiledNode& producer = payload.compiledNodes[slot.producerNodeIdx];

            SidechainGainNode::Input input;
            input.producerId = producer.id;
            fillInputChannels (payload, slot.fromSlot, producer.numChannels, input.channels);
            inputs.push_back (input);
        }

        return inputs;
    }

    template <typename ChannelArray>
    static void fillInputChannels (const CompiledGraph::Payload& payload,
                                   SlotIndex slot,
                                   std::uint16_t channels,
                                   ChannelArray& out)
    {
        const std::uint16_t maxChannels = payload.poolLayout.maxChannelsPerSlot;
        const std::uint16_t n = channels < maxChannels ? channels : maxChannels;

        for (std::uint16_t c = 0; c < n; ++c)
            out[c] = payload.floatSlotPtrs[static_cast<std::size_t> (slot) * static_cast<std::size_t> (maxChannels)
                                           + static_cast<std::size_t> (c)];
    }

    static constexpr std::size_t kNoItem = std::numeric_limits<std::size_t>::max();
};

} // namespace yesdaw::engine
