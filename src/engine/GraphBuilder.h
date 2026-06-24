// YES DAW — GraphBuilder (ADR-0007): control-thread compiler for immutable CompiledGraph snapshots.
//
// Slice G implements Pass 1+2 and the smallest Pass 3-5 stubs needed for an end-to-end render:
// validation, iterative topo from Master, one f32 slot per reachable node, bus binding, prepare, publish.
// PDC splicing, greedy pool allocation, carry-over, mute, and scalar routing land in later slices.

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

        CompiledGraph::Payload payload;
        payload.id         = inputs.id;
        payload.identityDc = 0.0f;

        payload.nodeStorage = std::move (inputs.nodes);

        const int maxBlockSize = inputs.maxBlockSize > 0 ? inputs.maxBlockSize : 1;
        buildCompiledMetadata (infos, order, maxBlockSize, payload);
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

    static void buildCompiledMetadata (const std::vector<NodeInfo>& infos,
                                       const std::vector<std::size_t>& order,
                                       int maxBlockSize,
                                       CompiledGraph::Payload& payload)
    {
        std::vector<std::uint32_t> nodeToCompiled (infos.size(), kUncompiled);
        for (std::size_t i = 0; i < order.size(); ++i)
            nodeToCompiled[order[i]] = static_cast<std::uint32_t> (i);

        payload.compiledNodes.reserve (order.size());
        payload.inputSlotIndices.reserve (order.size());

        std::uint16_t maxChannels = 1;
        for (const std::size_t nodeIdx : order)
        {
            const int ch = infos[nodeIdx].channels;
            if (ch > static_cast<int> (maxChannels))
                maxChannels = static_cast<std::uint16_t> (ch);
        }

        payload.poolLayout.maxBlockSize       = static_cast<std::uint32_t> (maxBlockSize);
        payload.poolLayout.maxChannelsPerSlot = maxChannels;
        payload.poolLayout.numFloatSlots      = checkedSlotCount (order.size() + 1u);
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

        for (std::size_t compiledIdx = 0; compiledIdx < order.size(); ++compiledIdx)
        {
            const std::size_t nodeIdx = order[compiledIdx];
            const NodeInfo& info = infos[nodeIdx];

            CompiledNode cn;
            cn.node        = info.node;
            cn.id          = info.props.id;
            cn.numChannels = static_cast<std::uint16_t> (info.channels);
            cn.inputsBegin = static_cast<std::uint32_t> (payload.inputSlotIndices.size());
            cn.outputSlot  = checkedSlotCount (compiledIdx + 1u);
            cn.kind        = info.kind;

            for (const std::size_t inputIdx : info.inputs)
            {
                const std::uint32_t producerCompiled = nodeToCompiled[inputIdx];
                if (producerCompiled == kUncompiled || producerCompiled >= compiledIdx)
                    continue; // allowed Delay feedback edge, or unreachable input, is silent in slice G.

                payload.inputSlotIndices.push_back (InputSlot {
                    payload.compiledNodes[producerCompiled].outputSlot,
                    static_cast<std::uint16_t> (producerCompiled) });
            }

            cn.numInputs = static_cast<std::uint16_t> (payload.inputSlotIndices.size() - cn.inputsBegin);
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

    static constexpr std::uint32_t kUncompiled = std::numeric_limits<std::uint32_t>::max();
};

} // namespace yesdaw::engine
