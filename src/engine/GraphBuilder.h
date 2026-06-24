// YES DAW — GraphBuilder (ADR-0007): control-thread compiler for immutable CompiledGraph snapshots.
//
// Slice H implements Pass 3 PDC on top of the slice G scaffolding: longest-path latency metadata plus
// synthetic LatencyNode splices at convergence points. Greedy pool allocation, carry-over, mute, and
// scalar routing land in later slices.

#pragma once

#include "engine/CompiledGraph.h"
#include "engine/nodes/DelayNode.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/IdentityDcNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/MeterNode.h"
#include "engine/nodes/OscillatorNode.h"
#include "engine/nodes/PanNode.h"
#include "engine/nodes/SumNode.h"

#include <algorithm>
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

    using Detail = std::variant<std::monostate, DuplicateNodeId, MissingNode, LatencyOutOfRange, CyclicGraph, GraphTooLarge>;

    enum class Code
    {
        None,
        DuplicateNodeId,
        MissingNode,
        LatencyOutOfRange,
        CyclicGraph,
        GraphTooLarge
    };

    Detail detail;

    GraphBuildError() = default;
    GraphBuildError (DuplicateNodeId e)   : detail (e) {}
    GraphBuildError (MissingNode e)       : detail (e) {}
    GraphBuildError (LatencyOutOfRange e) : detail (e) {}
    GraphBuildError (CyclicGraph e)       : detail (e) {}
    GraphBuildError (GraphTooLarge e)     : detail (e) {}

    Code code() const noexcept
    {
        switch (detail.index())
        {
            case 1: return Code::DuplicateNodeId;
            case 2: return Code::MissingNode;
            case 3: return Code::LatencyOutOfRange;
            case 4: return Code::CyclicGraph;
            case 5: return Code::GraphTooLarge;
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

        const int maxBlockSize = inputs.maxBlockSize > 0 ? inputs.maxBlockSize : 1;
        buildCompiledMetadata (compileItems, maxBlockSize, payload);
        bindBusNodes (payload);

        for (CompiledNode& cn : payload.compiledNodes)
            if (cn.node != nullptr)
                cn.node->prepare (inputs.sampleRate, maxBlockSize);

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
        std::vector<std::size_t> inputs;
    };

    static std::unique_ptr<CompiledGraph> fail (GraphBuildError* error, GraphBuildError value)
    {
        if (error != nullptr)
            *error = value;
        return nullptr;
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
        if (dynamic_cast<OscillatorNode*> (&node) != nullptr)
            return CompiledNodeKind::Oscillator;
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
        const NodeId spliceId = syntheticLatencyId (consumer.props.id, producer.props.id, items.size());

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
        splice.inputs.push_back (inputItemIdx);

        const std::size_t spliceIdx = items.size();
        items.push_back (std::move (splice));
        return spliceIdx;
    }

    static NodeId syntheticLatencyId (NodeId consumerId, NodeId producerId, std::size_t ordinal) noexcept
    {
        std::uint64_t h = 14695981039346656037ull;
        h = fnv1a (h, static_cast<std::uint64_t> (consumerId));
        h = fnv1a (h, static_cast<std::uint64_t> (producerId));
        h = fnv1a (h, static_cast<std::uint64_t> (ordinal));
        return static_cast<NodeId> (0x8000'0000u | (static_cast<NodeId> (h) & 0x7FFF'FFFFu));
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

    static void buildCompiledMetadata (const std::vector<CompileItem>& items,
                                       int maxBlockSize,
                                       CompiledGraph::Payload& payload)
    {
        payload.compiledNodes.reserve (items.size());
        payload.inputSlotIndices.reserve (items.size());

        std::uint16_t maxChannels = 1;
        for (const CompileItem& item : items)
        {
            const int ch = item.channels;
            if (ch > static_cast<int> (maxChannels))
                maxChannels = static_cast<std::uint16_t> (ch);
        }

        payload.poolLayout.maxBlockSize       = static_cast<std::uint32_t> (maxBlockSize);
        payload.poolLayout.maxChannelsPerSlot = maxChannels;
        payload.poolLayout.numFloatSlots      = checkedSlotCount (items.size() + 1u);
        payload.poolLayout.numDoubleSlots     = 0;

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

        for (std::size_t compiledIdx = 0; compiledIdx < items.size(); ++compiledIdx)
        {
            const CompileItem& item = items[compiledIdx];

            CompiledNode cn;
            cn.node        = item.node;
            cn.id          = item.props.id;
            cn.numChannels = static_cast<std::uint16_t> (item.channels);
            cn.inputsBegin = static_cast<std::uint32_t> (payload.inputSlotIndices.size());
            cn.outputSlot  = checkedSlotCount (compiledIdx + 1u);
            cn.pathLatency = item.pathLatency;
            cn.kind        = item.kind;

            for (const std::size_t inputIdx : item.inputs)
            {
                payload.inputSlotIndices.push_back (InputSlot {
                    payload.compiledNodes[inputIdx].outputSlot,
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
        }

        std::sort (payload.idIndex.begin(), payload.idIndex.end(),
                   [] (const auto& a, const auto& b) { return a.first < b.first; });
    }

    static std::uint16_t checkedSlotCount (std::size_t value) noexcept
    {
        return value > static_cast<std::size_t> (std::numeric_limits<std::uint16_t>::max())
            ? std::numeric_limits<std::uint16_t>::max()
            : static_cast<std::uint16_t> (value);
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
        }
    }

    static std::vector<SumNode::Input> sumInputsFor (const CompiledGraph::Payload& payload, const CompiledNode& cn)
    {
        std::vector<SumNode::Input> inputs;
        inputs.reserve (cn.numInputs);

        for (std::uint16_t i = 0; i < cn.numInputs; ++i)
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

        for (std::uint16_t i = 0; i < cn.numInputs; ++i)
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
