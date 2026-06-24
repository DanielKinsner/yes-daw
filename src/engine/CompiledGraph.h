// YES DAW — CompiledGraph (H1).
//
// The immutable, read-only thing the audio thread runs (ADR-0006 publishes it; ADR-0007's compiler fills
// it with nodes/edges/PDC delays). The legacy `(GraphId, identityDc)` constructor remains a permanent
// degenerate test seam: it lets Runtime prove graph swaps without requiring the full compiler path.
//
// Pure C++ — no JUCE — so it builds on the RTSan/TSan legs (YESDAW_BUILD_APPS=OFF) where the real-time
// guarantees are actually verified.
//
// Lifetime/threading: CompiledGraph owns Nodes. Its destructor, each Node destructor, and Node::release()
// are NOT real-time safe and must run only on Runtime's janitor/control side, never the audio thread.
//
// Buffer-pool aliasing/lifetime contract (ADR-0007):
//   R1. Exactly one producer writes each non-silence slot per Block.
//   R2. A slot may be reused only after its last consumer in topo order has run.
//   R3. In-place reuse is allowed only for the compiler's explicit whitelist and last-reader predicate.
//   R4. Sidechain and multi-input readers count in last-reader analysis; no pending reader is skipped.
//   R5. f64 Bus scratch slots are per-SumNode temporaries, never aliased with f32 audio slots.
//   R6. Slot 0 is permanent read-only silence and is never allocated as a producer output.
//   R7. CompiledGraph::process() enables FTZ/DAZ before running real nodes.

#pragma once

#include "dsp/ScopedNoDenormals.h"
#include "engine/Node.h"
#include "rt/RtHot.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace yesdaw::engine {

using GraphId = std::uint64_t;

using SlotIndex  = std::uint16_t;
using DSlotIndex = std::uint16_t;

inline constexpr SlotIndex kSilenceSlot = 0;
inline constexpr SlotIndex kNoSlot      = 0xFFFFu;

enum class CompiledNodeKind : std::uint8_t
{
    IdentityDc,
    Oscillator,
    Source,
    Fader,
    Pan,
    Sum,
    Meter,
    Delay,
    Latency,
    Master,
    Plugin
};

struct InputSlot
{
    SlotIndex     fromSlot        = kNoSlot;
    std::uint16_t producerNodeIdx = 0;
};

struct CompiledNode
{
    Node*            node          = nullptr;
    NodeId           id            = 0;
    std::uint16_t    numInputs     = 0;
    std::uint16_t    numChannels   = 0;
    std::uint32_t    inputsBegin   = 0;
    SlotIndex        outputSlot    = kNoSlot;
    DSlotIndex       busAccumSlot  = kNoSlot;
    std::int64_t     pathLatency   = 0;
    std::uint8_t     muteBit       = 0;
    CompiledNodeKind kind          = CompiledNodeKind::IdentityDc;
    bool             aliasOk       = false;
    std::uint8_t     _pad          = 0;
};

static_assert (sizeof (CompiledNode) <= 64, "CompiledNode must stay cache-small for the audio thread");
static_assert (std::is_trivially_copyable_v<InputSlot>, "InputSlot must be flat compiler metadata");
static_assert (std::is_trivially_copyable_v<CompiledNode>, "CompiledNode must be flat hot-path metadata");

struct BufferPoolLayout
{
    std::uint16_t numFloatSlots      = 1; // slot 0 is permanent silence
    std::uint16_t numDoubleSlots     = 0;
    std::uint16_t maxChannelsPerSlot = 1;
    std::uint32_t maxBlockSize       = 0;
};

class CompiledGraph
{
public:
    struct Payload
    {
        GraphId id         = 0;
        float   identityDc = 0.0f;

        std::vector<std::unique_ptr<Node>>              nodeStorage;
        std::vector<CompiledNode>                       compiledNodes;
        std::vector<InputSlot>                          inputSlotIndices;
        std::unique_ptr<float[]>                        floatStorage;
        std::unique_ptr<double[]>                       doubleStorage;
        std::vector<float*>                             floatSlotPtrs;
        std::vector<double*>                            doubleSlotPtrs;
        BufferPoolLayout                                poolLayout;
        std::int64_t                                    totalLatency    = 0;
        std::uint64_t                                   muteMask        = 0;
        SlotIndex                                       masterOutputSlot = kSilenceSlot;
        std::uint16_t                                   masterChannels   = 1;
        std::vector<std::pair<NodeId, std::uint32_t>>   idIndex;
    };

    CompiledGraph (GraphId id, float identityDc) noexcept
        : id_ (id), identityDc_ (identityDc), isDegenerate_ (true)
    {
        alive_.fetch_add (1, std::memory_order_relaxed);
    }

    explicit CompiledGraph (Payload&& payload)
        : id_ (payload.id),
          identityDc_ (payload.identityDc),
          nodeStorage_ (std::move (payload.nodeStorage)),
          compiledNodes_ (std::move (payload.compiledNodes)),
          inputSlotIndices_ (std::move (payload.inputSlotIndices)),
          floatStorage_ (std::move (payload.floatStorage)),
          doubleStorage_ (std::move (payload.doubleStorage)),
          floatSlotPtrs_ (std::move (payload.floatSlotPtrs)),
          doubleSlotPtrs_ (std::move (payload.doubleSlotPtrs)),
          poolLayout_ (payload.poolLayout),
          totalLatency_ (payload.totalLatency),
          muteMask_ (payload.muteMask),
          masterOutputSlot_ (payload.masterOutputSlot),
          masterChannels_ (payload.masterChannels),
          idIndex_ (std::move (payload.idIndex)),
          isDegenerate_ (false)
    {
        alive_.fetch_add (1, std::memory_order_relaxed);
    }

    ~CompiledGraph()
    {
        // Poison the canary on the way out: if a use-after-free ever lets the audio thread call
        // process() on this freed object before the memory is reused, the canary check below traps.
        canary_ = kPoison;

        // Node::release() is part of the control-thread lifecycle contract: Nodes may allocate in
        // prepare(), and those resources are released by the janitor side before destruction.
        for (const std::unique_ptr<Node>& node : nodeStorage_)
            if (node != nullptr)
                node->release();

        alive_.fetch_sub (1, std::memory_order_relaxed);
    }

    CompiledGraph (const CompiledGraph&)            = delete;
    CompiledGraph& operator= (const CompiledGraph&) = delete;

    // The audio hot path. Immutable read; allocation/lock free; RTSan-covered.
    void process (float* out, int numFrames) const noexcept YESDAW_RT_HOT
    {
        YESDAW_RT_FATAL (canary_ == kCanary);   // UAF tripwire — ALWAYS live (incl. RTSan/TSan/Release).

        if (isDegenerate_)
        {
            for (int i = 0; i < numFrames; ++i)
                out[i] = identityDc_;
            return;
        }

        const yesdaw::dsp::ScopedNoDenormals noDenormals;

        YESDAW_RT_FATAL (numFrames >= 0);
        YESDAW_RT_FATAL (static_cast<std::uint32_t> (numFrames) <= poolLayout_.maxBlockSize);

        EventStream events;
        Transport   transport;

        const CompiledNode* const nodes   = compiledNodes_.data();
        const std::size_t         nNodes  = compiledNodes_.size();
        const InputSlot* const    inputs  = inputSlotIndices_.data();
        float* const* const       slots   = floatSlotPtrs_.data();
        const std::uint16_t       maxCh   = poolLayout_.maxChannelsPerSlot;

        for (std::size_t i = 0; i < nNodes; ++i)
        {
            const CompiledNode& cn = nodes[i];
            if (cn.node == nullptr || cn.outputSlot == kNoSlot)
                continue;

            float* const* const outChannels = slots + static_cast<std::size_t> (cn.outputSlot) * static_cast<std::size_t> (maxCh);
            if (! cn.aliasOk)
                zeroChannels (outChannels, cn.numChannels, numFrames);

            const bool busLike = cn.kind == CompiledNodeKind::Sum || cn.kind == CompiledNodeKind::Master;
            if (cn.numInputs == 1 && ! busLike && ! cn.aliasOk)
            {
                const InputSlot& input = inputs[cn.inputsBegin];
                if (input.fromSlot != kNoSlot)
                {
                    const CompiledNode& producer = nodes[input.producerNodeIdx];
                    float* const* const inChannels = slots + static_cast<std::size_t> (input.fromSlot) * static_cast<std::size_t> (maxCh);
                    copyChannels (inChannels, producer.numChannels, outChannels, cn.numChannels, numFrames);
                }
            }

            const ProcessArgs args { AudioBlock { outChannels, static_cast<int> (cn.numChannels) },
                                     events, transport, numFrames };
            cn.node->process (args);
        }

        if (masterOutputSlot_ == kSilenceSlot || floatSlotPtrs_.empty())
        {
            for (int i = 0; i < numFrames; ++i)
                out[i] = 0.0f;
            return;
        }

        const float* const master = slots[static_cast<std::size_t> (masterOutputSlot_) * static_cast<std::size_t> (maxCh)];
        for (int i = 0; i < numFrames; ++i)
            out[i] = master[i];
    }

    GraphId id()         const noexcept { return id_; }
    float   identityDc() const noexcept { return identityDc_; }
    bool    isDegenerate() const noexcept { return isDegenerate_; }
    std::int64_t totalLatency() const noexcept { return totalLatency_; }

    const BufferPoolLayout& debugPoolLayout() const noexcept { return poolLayout_; }
    std::span<const CompiledNode> debugCompiledNodes() const noexcept { return compiledNodes_; }

    std::size_t debugCountNodesOfKind (CompiledNodeKind kind) const noexcept
    {
        std::size_t count = 0;
        for (const CompiledNode& node : compiledNodes_)
            if (node.kind == kind)
                ++count;
        return count;
    }

    // Liveness instrumentation (cheap, always compiled — also a legitimate diagnostic). Every
    // construction/destruction adjusts this, so a test can assert it returns to its baseline (no leak)
    // and that exactly one graph survives steady state (reclamation actually ran).
    static std::uint64_t aliveCount() noexcept { return alive_.load (std::memory_order_relaxed); }

private:
    static constexpr std::uint32_t kCanary = 0xC0DEFACEu;
    static constexpr std::uint32_t kPoison = 0xDEADBEEFu;

    static void zeroChannels (float* const* channels, std::uint16_t numChannels, int numFrames) noexcept YESDAW_RT_HOT
    {
        for (std::uint16_t c = 0; c < numChannels; ++c)
        {
            float* const dst = channels[c];
            YESDAW_RT_FATAL (dst != nullptr);
            for (int i = 0; i < numFrames; ++i)
                dst[i] = 0.0f;
        }
    }

    static void copyChannels (float* const* src,
                              std::uint16_t srcChannels,
                              float* const* dst,
                              std::uint16_t dstChannels,
                              int numFrames) noexcept YESDAW_RT_HOT
    {
        const std::uint16_t n = srcChannels < dstChannels ? srcChannels : dstChannels;
        for (std::uint16_t c = 0; c < n; ++c)
        {
            const float* const s = src[c];
            float* const       d = dst[c];
            YESDAW_RT_FATAL (s != nullptr);
            YESDAW_RT_FATAL (d != nullptr);
            for (int i = 0; i < numFrames; ++i)
                d[i] = s[i];
        }
    }

    GraphId       id_;
    float         identityDc_;
    std::uint32_t canary_ = kCanary;

    std::vector<std::unique_ptr<Node>>            nodeStorage_;
    std::vector<CompiledNode>                     compiledNodes_;
    std::vector<InputSlot>                        inputSlotIndices_;
    std::unique_ptr<float[]>                      floatStorage_;
    std::unique_ptr<double[]>                     doubleStorage_;
    std::vector<float*>                           floatSlotPtrs_;
    std::vector<double*>                          doubleSlotPtrs_;
    BufferPoolLayout                              poolLayout_;
    std::int64_t                                  totalLatency_     = 0;
    std::atomic<std::uint64_t>                    muteMask_         { 0 };
    SlotIndex                                     masterOutputSlot_ = kSilenceSlot;
    std::uint16_t                                 masterChannels_   = 1;
    std::vector<std::pair<NodeId, std::uint32_t>> idIndex_;
    bool                                          isDegenerate_     = true;

    static inline std::atomic<std::uint64_t> alive_ { 0 };
};

} // namespace yesdaw::engine
