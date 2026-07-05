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
#include "engine/Automation.h"
#include "engine/Node.h"
#include "engine/nodes/DelayNode.h"
#include "engine/nodes/FaderNode.h"
#include "engine/nodes/MasterNode.h"
#include "engine/nodes/PanNode.h"
#include "engine/nodes/SidechainGainNode.h"
#include "engine/nodes/SumNode.h"
#include "rt/RtHot.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace yesdaw::engine {

using GraphId = std::uint64_t;
using DelayCacheKey = std::uint64_t;

using SlotIndex  = std::uint16_t;
using DSlotIndex = std::uint16_t;
using EventSlotIndex = std::uint16_t;

inline constexpr SlotIndex kSilenceSlot = 0;
inline constexpr SlotIndex kNoSlot      = 0xFFFFu;
inline constexpr EventSlotIndex kRootEventSlot = 0;
inline constexpr EventSlotIndex kNoEventSlot   = 0xFFFFu;
inline constexpr std::uint32_t kNoMuteBit = 0xFFFFFFFFu;

// Number of 64-bit words a mute mask needs to carry one bit per compiled node (ADR-0016). Sized once on
// the control thread at CompiledGraph construction; the audio thread only ever loads from it.
[[nodiscard]] inline constexpr std::size_t muteWordCount (std::size_t numNodes) noexcept
{
    return (numNodes + 63u) / 64u;
}

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
    Sidechain,
    Placeholder,
    MidiEffect,
    MidiSource,
    Eq,
    Compressor,
    FxDelay,
    Reverb,
    Limiter,
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
    EventSlotIndex   eventInputSlot = kRootEventSlot;
    EventSlotIndex   eventOutputSlot = kNoEventSlot;
    DSlotIndex       busAccumSlot  = kNoSlot;
    std::int64_t     pathLatency   = 0;
    DelayCacheKey    delayCacheKey = 0;
    std::uint32_t    muteBit       = 0;   // == this node's compiled index; indexes the mute-mask words (ADR-0016)
    CompiledNodeKind kind          = CompiledNodeKind::IdentityDc;
    bool             aliasOk       = false;
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

struct DelayCacheEntry
{
    DelayCacheKey key              = 0;
    std::int64_t  delaySamples     = 0;
    int           channels         = 0;
    std::uint32_t framesPerChannel = 0;
    std::uint32_t writePos         = 0;
    std::vector<float> ring;
};

struct CompiledAutomationLane
{
    NodeId      targetNode  = 0;
    ParameterId parameterId = 0;
    std::vector<std::int64_t> frames;
    std::vector<double> values;
    std::vector<AutomationCurveType> curveTypes;
};

class CompiledGraph
{
public:
    static constexpr std::uint32_t kMaxEventsPerBlock = 1024;

    struct Payload
    {
        GraphId id         = 0;
        float   identityDc = 0.0f;

        std::vector<std::unique_ptr<Node>>              nodeStorage;
        std::vector<CompiledNode>                       compiledNodes;
        std::vector<InputSlot>                          inputSlotIndices;
        std::unique_ptr<float[]>                        floatStorage;
        std::unique_ptr<double[]>                       doubleStorage;
        std::unique_ptr<Event[]>                        eventStorage;
        std::unique_ptr<std::uint32_t[]>                eventSlotCounts;
        std::unique_ptr<Event[]>                        automationEventStorage;
        std::vector<float*>                             floatSlotPtrs;
        std::vector<double*>                            doubleSlotPtrs;
        std::vector<Event*>                             eventSlotPtrs;
        BufferPoolLayout                                poolLayout;
        std::uint16_t                                   numEventSlots = 1;
        std::uint32_t                                   maxEventsPerBlock = kMaxEventsPerBlock;
        std::int64_t                                    totalLatency    = 0;
        bool                                            blockParallelSafe = false;   // ADR-0027
        SlotIndex                                       masterOutputSlot = kSilenceSlot;
        std::uint16_t                                   masterChannels   = 1;
        std::vector<std::pair<NodeId, std::uint32_t>>   idIndex;
        std::vector<CompiledAutomationLane>             automationLanes;
    };

    CompiledGraph (GraphId id, float identityDc) noexcept
        : id_ (id), identityDc_ (identityDc), isDegenerate_ (true), blockParallelSafe_ (true)
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
          eventStorage_ (std::move (payload.eventStorage)),
          eventSlotCounts_ (std::move (payload.eventSlotCounts)),
          automationEventStorage_ (std::move (payload.automationEventStorage)),
          floatSlotPtrs_ (std::move (payload.floatSlotPtrs)),
          doubleSlotPtrs_ (std::move (payload.doubleSlotPtrs)),
          eventSlotPtrs_ (std::move (payload.eventSlotPtrs)),
          poolLayout_ (payload.poolLayout),
          numEventSlots_ (payload.numEventSlots),
          maxEventsPerBlock_ (payload.maxEventsPerBlock),
          totalLatency_ (payload.totalLatency),
          muteWords_ (muteWordCount (compiledNodes_.size())),
          masterOutputSlot_ (payload.masterOutputSlot),
          masterChannels_ (payload.masterChannels),
          idIndex_ (std::move (payload.idIndex)),
          automationLanes_ (std::move (payload.automationLanes)),
          isDegenerate_ (false),
          blockParallelSafe_ (payload.blockParallelSafe)
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

    // Compatibility mono wrapper for older tests/drivers. The real device-callback path below can surface
    // every master channel.
    void process (float* out, int numFrames) const noexcept YESDAW_RT_HOT
    {
        float* outputs[1] = { out };
        process (outputs, 1, numFrames);
    }

    void process (float* out, int numFrames, std::span<const Event> events) const noexcept YESDAW_RT_HOT
    {
        float* outputs[1] = { out };
        process (outputs, 1, numFrames, events);
    }

    void process (float* out, int numFrames, std::span<Event> events) const noexcept YESDAW_RT_HOT
    {
        float* outputs[1] = { out };
        process (outputs, 1, numFrames, events);
    }

    // The audio hot path. Immutable read; allocation/lock free; RTSan-covered.
    void process (float* const* outChannels, int numOutputChannels, int numFrames) const noexcept YESDAW_RT_HOT
    {
        EventStream events;
        process (outChannels, numOutputChannels, numFrames, events);
    }

    void process (float* const* outChannels,
                  int numOutputChannels,
                  int numFrames,
                  std::span<const Event> events) const noexcept YESDAW_RT_HOT
    {
        EventStream stream { events };
        process (outChannels, numOutputChannels, numFrames, stream);
    }

    void process (float* const* outChannels,
                  int numOutputChannels,
                  int numFrames,
                  std::span<Event> events) const noexcept YESDAW_RT_HOT
    {
        EventStream stream { events };
        process (outChannels, numOutputChannels, numFrames, stream);
    }

    void process (float* const* outChannels,
                  int numOutputChannels,
                  int numFrames,
                  EventStream& events) const noexcept YESDAW_RT_HOT
    {
        Transport transport;
        process (outChannels, numOutputChannels, numFrames, events, transport);
    }

    void process (float* const* outChannels,
                  int numOutputChannels,
                  int numFrames,
                  EventStream& events,
                  const Transport& transport) const noexcept YESDAW_RT_HOT
    {
        YESDAW_RT_FATAL (canary_ == kCanary);   // UAF tripwire — ALWAYS live (incl. RTSan/TSan/Release).
        YESDAW_RT_FATAL (numFrames >= 0);
        YESDAW_RT_FATAL (numOutputChannels >= 0);
        YESDAW_RT_FATAL (numOutputChannels <= static_cast<int> (std::numeric_limits<std::uint16_t>::max()));
        if (numOutputChannels > 0)
            YESDAW_RT_FATAL (outChannels != nullptr);

        if (isDegenerate_)
        {
            fillOutputChannels (outChannels, static_cast<std::uint16_t> (numOutputChannels), numFrames, identityDc_);
            return;
        }

        const yesdaw::dsp::ScopedNoDenormals noDenormals;

        YESDAW_RT_FATAL (numFrames >= 0);
        YESDAW_RT_FATAL (static_cast<std::uint32_t> (numFrames) <= poolLayout_.maxBlockSize);

        const CompiledNode* const nodes   = compiledNodes_.data();
        const std::size_t         nNodes  = compiledNodes_.size();
        const InputSlot* const    inputs  = inputSlotIndices_.data();
        float* const* const       slots   = floatSlotPtrs_.data();
        Event* const* const       eventSlots = eventSlotPtrs_.data();
        std::uint32_t* const      eventCounts = eventSlotCounts_.get();
        const std::uint16_t       maxCh   = poolLayout_.maxChannelsPerSlot;
        const std::atomic<std::uint64_t>* const muteWords = muteWords_.data();   // ceil(nNodes/64) words; loads only
        EventStream automationEvents;
        const EventStream* automationEventsForBlock = nullptr;

        if (eventCounts != nullptr)
            for (EventSlotIndex slot = 1; slot < numEventSlots_; ++slot)
                eventCounts[slot] = 0;

        if (! automationLanes_.empty() && transport.hasTimelineFrame)
        {
            YESDAW_RT_FATAL (automationEventStorage_ != nullptr);
            const std::size_t automationCount =
                emitAutomationEventsForBlock (automationLanes_,
                                              transport.timelineFrame,
                                              static_cast<std::uint32_t> (numFrames),
                                              std::span<Event> (automationEventStorage_.get(), maxEventsPerBlock_));
            automationEvents = EventStream {
                std::span<const Event> (automationEventStorage_.get(), automationCount)
            };
            automationEventsForBlock = &automationEvents;
        }

#if ! defined (NDEBUG) || defined (YESDAW_TEST_DEBUG_POOL_PAINT)
        debugPaintPooledSlots (slots, poolLayout_.numFloatSlots, maxCh, numFrames);
#endif

        for (std::size_t i = 0; i < nNodes; ++i)
        {
            const CompiledNode& cn = nodes[i];
            if (cn.node == nullptr || cn.outputSlot == kNoSlot)
                continue;

            float* const* const nodeOutChannels = slots + static_cast<std::size_t> (cn.outputSlot) * static_cast<std::size_t> (maxCh);
            const bool muted = cn.muteBit != kNoMuteBit
                && (muteWords[cn.muteBit >> 6u].load (std::memory_order_relaxed) & (1ull << (cn.muteBit & 63u))) != 0;

            if (! cn.aliasOk || muted)
                zeroChannels (nodeOutChannels, cn.numChannels, numFrames);
            if (muted)
                continue;

            const bool busLike = cn.kind == CompiledNodeKind::Sum || cn.kind == CompiledNodeKind::Master;
            if (cn.numInputs == 1 && ! busLike && ! cn.aliasOk)
            {
                const InputSlot& input = inputs[cn.inputsBegin];
                if (input.fromSlot != kNoSlot)
                {
                    const CompiledNode& producer = nodes[input.producerNodeIdx];
                    float* const* const inChannels = slots + static_cast<std::size_t> (input.fromSlot) * static_cast<std::size_t> (maxCh);
                    copyChannels (inChannels, producer.numChannels, nodeOutChannels, cn.numChannels, numFrames);
                }
            }

            std::span<const Event> inputEvents;
            if (cn.eventInputSlot == kRootEventSlot)
            {
                inputEvents = events.events();
            }
            else
            {
                YESDAW_RT_FATAL (eventCounts != nullptr);
                YESDAW_RT_FATAL (eventSlots != nullptr);
                YESDAW_RT_FATAL (cn.eventInputSlot < numEventSlots_);
                inputEvents = std::span<const Event> (eventSlots[cn.eventInputSlot], eventCounts[cn.eventInputSlot]);
            }

            if (cn.eventOutputSlot != kNoEventSlot)
            {
                YESDAW_RT_FATAL (eventCounts != nullptr);
                YESDAW_RT_FATAL (eventSlots != nullptr);
                YESDAW_RT_FATAL (cn.eventOutputSlot < numEventSlots_);
                YESDAW_RT_FATAL (inputEvents.size() <= maxEventsPerBlock_);

                EventStream nodeEvents {
                    std::span<Event> (eventSlots[cn.eventOutputSlot], maxEventsPerBlock_),
                    inputEvents.size(),
                    events.sysexBytes()
                };
                YESDAW_RT_FATAL (nodeEvents.replaceEvents (inputEvents));

                const ProcessArgs args { AudioBlock { nodeOutChannels, static_cast<int> (cn.numChannels) },
                                         nodeEvents, transport, numFrames, automationEventsForBlock };
                cn.node->process (args);
                eventCounts[cn.eventOutputSlot] = static_cast<std::uint32_t> (nodeEvents.size());
            }
            else
            {
                EventStream nodeEvents { inputEvents, events.sysexBytes() };
                const ProcessArgs args { AudioBlock { nodeOutChannels, static_cast<int> (cn.numChannels) },
                                         nodeEvents, transport, numFrames, automationEventsForBlock };
                cn.node->process (args);
            }
        }

        if (masterOutputSlot_ == kSilenceSlot || floatSlotPtrs_.empty())
        {
            zeroOutputChannels (outChannels, static_cast<std::uint16_t> (numOutputChannels), numFrames);
            return;
        }

        const std::size_t masterBase = static_cast<std::size_t> (masterOutputSlot_) * static_cast<std::size_t> (maxCh);
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            float* const dst = outChannels[channel];
            YESDAW_RT_FATAL (dst != nullptr);

            if (channel < static_cast<int> (masterChannels_))
            {
                const float* const src = slots[masterBase + static_cast<std::size_t> (channel)];
                YESDAW_RT_FATAL (src != nullptr);
                for (int i = 0; i < numFrames; ++i)
                    dst[i] = src[i];
            }
            else
            {
                for (int i = 0; i < numFrames; ++i)
                    dst[i] = 0.0f;
            }
        }
    }

    GraphId id()         const noexcept { return id_; }
    float   identityDc() const noexcept { return identityDc_; }
    bool    isDegenerate() const noexcept { return isDegenerate_; }
    std::int64_t totalLatency() const noexcept { return totalLatency_; }
    [[nodiscard]] bool totalTailSamples (std::uint64_t& out) const noexcept
    {
        out = 0;
        if (isDegenerate_)
            return true;

        for (const CompiledNode& cn : compiledNodes_)
        {
            if (cn.node == nullptr)
                continue;

            const std::int64_t tail = cn.node->properties().tailSamples;
            if (tail < 0)
                return false;

            const std::uint64_t uTail = static_cast<std::uint64_t> (tail);
            if (uTail > std::numeric_limits<std::uint64_t>::max() - out)
                return false;

            out += uTail;
        }

        return true;
    }
    // ADR-0027: true iff EVERY node is order-independent across Blocks, so the parallel scheduler may
    // dispatch this graph's Blocks out of order without changing a sample. False if any node is stateful.
    bool    isBlockParallelSafe() const noexcept { return blockParallelSafe_; }

    [[nodiscard]] bool setMuted (NodeId id, bool muted) noexcept
    {
        const CompiledNode* const node = findCompiledNode (id);
        if (node == nullptr || node->muteBit == kNoMuteBit)
            return false;

        const std::uint64_t          bit  = 1ull << (node->muteBit & 63u);
        std::atomic<std::uint64_t>&  word = muteWords_[node->muteBit >> 6u];
        if (muted)
            word.fetch_or (bit, std::memory_order_relaxed);
        else
            word.fetch_and (~bit, std::memory_order_relaxed);

        return true;
    }

    [[nodiscard]] bool isMuted (NodeId id) const noexcept
    {
        const CompiledNode* const node = findCompiledNode (id);
        if (node == nullptr || node->muteBit == kNoMuteBit)
            return false;

        return (muteWords_[node->muteBit >> 6u].load (std::memory_order_relaxed) & (1ull << (node->muteBit & 63u))) != 0;
    }

    // True iff `id` is a compiled node that can carry a mute bit (exists and within the 64-bit mask). The
    // mixer mute policy pre-checks every target with this so it can fail before publishing a partial mask,
    // rather than silently leaving a target unmutable (ADR-0014).
    [[nodiscard]] bool isMuteCapable (NodeId id) const noexcept
    {
        const CompiledNode* const node = findCompiledNode (id);
        return node != nullptr && node->muteBit != kNoMuteBit;
    }

    [[nodiscard]] bool applySetGain (NodeId id, float linearGain) const noexcept YESDAW_RT_HOT
    {
        const CompiledNode* const node = findCompiledNode (id);
        if (node == nullptr || node->kind != CompiledNodeKind::Fader || node->node == nullptr)
            return false;

        static_cast<FaderNode*> (node->node)->setTargetGain (linearGain);
        return true;
    }

    [[nodiscard]] bool applySetPan (NodeId id, float pan) const noexcept YESDAW_RT_HOT
    {
        const CompiledNode* const node = findCompiledNode (id);
        if (node == nullptr || node->kind != CompiledNodeKind::Pan || node->node == nullptr)
            return false;

        static_cast<PanNode*> (node->node)->setPan (pan);
        return true;
    }

    void snapshotDelayCache() const
    {
        delayCache_.clear();
        delayCache_.reserve (debugCountNodesOfKind (CompiledNodeKind::Delay)
                             + debugCountNodesOfKind (CompiledNodeKind::Latency));

        for (const CompiledNode& cn : compiledNodes_)
        {
            if (cn.kind != CompiledNodeKind::Delay && cn.kind != CompiledNodeKind::Latency)
                continue;

            const DelayNode* const delay = dynamic_cast<const DelayNode*> (cn.node);
            if (delay == nullptr)
                continue;

            DelayCacheEntry entry;
            entry.key              = cn.delayCacheKey;
            entry.delaySamples     = delay->delaySamples();
            entry.channels         = delay->channels();
            entry.framesPerChannel = delay->framesPerChannel();
            entry.writePos         = delay->writePos();

            const std::span<const float> ring = delay->ringState();
            entry.ring.assign (ring.begin(), ring.end());
            delayCache_.push_back (std::move (entry));
        }

        std::sort (delayCache_.begin(), delayCache_.end(),
                   [] (const DelayCacheEntry& a, const DelayCacheEntry& b) { return a.key < b.key; });
    }

    bool debugMultiInputNodesBound() const noexcept
    {
        for (const CompiledNode& cn : compiledNodes_)
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

    const BufferPoolLayout& debugPoolLayout() const noexcept { return poolLayout_; }
    std::span<const CompiledNode> debugCompiledNodes() const noexcept { return compiledNodes_; }
    std::span<const InputSlot> debugInputSlots() const noexcept { return inputSlotIndices_; }
    std::span<const CompiledAutomationLane> debugAutomationLanes() const noexcept { return automationLanes_; }
    std::span<const DelayCacheEntry> debugDelayCache() const noexcept { return delayCache_; }
    std::uint64_t debugMuteMask() const noexcept { return muteWords_.empty() ? 0ull : muteWords_[0].load (std::memory_order_relaxed); }

    int debugMasterChannels() const noexcept { return static_cast<int> (masterChannels_); }

    // Test/debug only: the master output slot's channel-`channel` buffer, valid immediately after a
    // process() call returns (single-threaded). process() copies channel 0 to its mono `out`; the full
    // master width is computed into the pool but never surfaced, so this exposes it so a test can assert
    // stereo placement (e.g. a centred bus Return must be present in BOTH L and R). Returns nullptr if the
    // channel is out of range or the master is silent.
    const float* debugMasterChannel (int channel) const noexcept
    {
        if (channel < 0 || channel >= static_cast<int> (masterChannels_)
            || masterOutputSlot_ == kSilenceSlot || floatSlotPtrs_.empty())
            return nullptr;

        const std::size_t maxCh = poolLayout_.maxChannelsPerSlot;
        return floatSlotPtrs_[static_cast<std::size_t> (masterOutputSlot_) * maxCh + static_cast<std::size_t> (channel)];
    }

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

    static void zeroOutputChannels (float* const* channels, std::uint16_t numChannels, int numFrames) noexcept YESDAW_RT_HOT
    {
        fillOutputChannels (channels, numChannels, numFrames, 0.0f);
    }

    static void fillOutputChannels (float* const* channels,
                                    std::uint16_t numChannels,
                                    int numFrames,
                                    float value) noexcept YESDAW_RT_HOT
    {
        for (std::uint16_t c = 0; c < numChannels; ++c)
        {
            float* const dst = channels[c];
            YESDAW_RT_FATAL (dst != nullptr);
            for (int i = 0; i < numFrames; ++i)
                dst[i] = value;
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

    [[nodiscard]] static std::int64_t saturatedBlockEnd (std::int64_t blockStart,
                                                         std::uint32_t numFrames) noexcept YESDAW_RT_HOT
    {
        const std::int64_t n = static_cast<std::int64_t> (numFrames);
        if (blockStart > std::numeric_limits<std::int64_t>::max() - n)
            return std::numeric_limits<std::int64_t>::max();
        return blockStart + n;
    }

    [[nodiscard]] static double interpolateCompiledAutomationValue (const CompiledAutomationLane& lane,
                                                                    std::size_t segment,
                                                                    std::int64_t frame) noexcept YESDAW_RT_HOT
    {
        const std::int64_t startFrame = lane.frames[segment];
        const std::int64_t endFrame = lane.frames[segment + 1u];
        if (endFrame <= startFrame)
            return lane.values[segment + 1u];

        const double t = static_cast<double> (frame - startFrame) / static_cast<double> (endFrame - startFrame);
        const double u = automationCurveProgress (lane.curveTypes[segment], t);
        return lane.values[segment] + (lane.values[segment + 1u] - lane.values[segment]) * u;
    }

    static void insertAutomationEventSorted (std::span<Event> out,
                                             std::size_t& count,
                                             Event event) noexcept YESDAW_RT_HOT
    {
        YESDAW_RT_FATAL (count < out.size());

        std::size_t insertAt = count;
        while (insertAt > 0 && event.timeInBlock < out[insertAt - 1u].timeInBlock)
        {
            out[insertAt] = out[insertAt - 1u];
            --insertAt;
        }

        out[insertAt] = event;
        ++count;
    }

    static void emitAutomationEvent (const CompiledAutomationLane& lane,
                                     std::int64_t blockStart,
                                     std::int64_t frame,
                                     double value,
                                     std::span<Event> out,
                                     std::size_t& count) noexcept YESDAW_RT_HOT
    {
        YESDAW_RT_FATAL (frame >= blockStart);
        const std::int64_t offset = frame - blockStart;
        YESDAW_RT_FATAL (offset >= 0);
        YESDAW_RT_FATAL (offset <= static_cast<std::int64_t> (std::numeric_limits<std::uint32_t>::max()));
        insertAutomationEventSorted (
            out,
            count,
            makeParameterChangeEvent (static_cast<std::uint32_t> (offset),
                                      lane.targetNode,
                                      lane.parameterId,
                                      value));
    }

    static void emitCompiledLaneAutomationEvents (const CompiledAutomationLane& lane,
                                                  std::int64_t blockStart,
                                                  std::int64_t blockEnd,
                                                  std::span<Event> out,
                                                  std::size_t& count) noexcept YESDAW_RT_HOT
    {
        for (std::size_t i = 0; i < lane.frames.size(); ++i)
        {
            const std::int64_t frame = lane.frames[i];
            if (frame >= blockStart && frame < blockEnd)
                emitAutomationEvent (lane, blockStart, frame, lane.values[i], out, count);
        }

        if (lane.frames.size() < 2u)
            return;

        for (std::size_t i = 0; i + 1u < lane.frames.size(); ++i)
        {
            if (lane.curveTypes[i] != AutomationCurveType::Linear)
                continue;

            const std::int64_t segmentStart = lane.frames[i];
            const std::int64_t segmentEnd = lane.frames[i + 1u];
            if (segmentEnd <= blockStart || segmentStart >= blockEnd)
                continue;

            const std::int64_t firstFrame = segmentStart > blockStart ? segmentStart : blockStart;
            std::int64_t controlFrame = ((firstFrame + 63) / 64) * 64;
            if (controlFrame < firstFrame)
                controlFrame = firstFrame;

            for (; controlFrame < segmentEnd && controlFrame < blockEnd; controlFrame += 64)
            {
                const double value = interpolateCompiledAutomationValue (lane, i, controlFrame);
                emitAutomationEvent (lane, blockStart, controlFrame, value, out, count);
            }
        }
    }

    [[nodiscard]] static std::size_t emitAutomationEventsForBlock (
        std::span<const CompiledAutomationLane> lanes,
        std::int64_t blockStart,
        std::uint32_t numFrames,
        std::span<Event> out) noexcept YESDAW_RT_HOT
    {
        std::size_t count = 0;
        if (numFrames == 0)
            return count;

        const std::int64_t blockEnd = saturatedBlockEnd (blockStart, numFrames);
        for (const CompiledAutomationLane& lane : lanes)
            emitCompiledLaneAutomationEvents (lane, blockStart, blockEnd, out, count);

        return count;
    }

    static void debugPaintPooledSlots (float* const* slots,
                                       std::uint16_t numSlots,
                                       std::uint16_t maxChannels,
                                       int numFrames) noexcept YESDAW_RT_HOT
    {
        const float poison = std::numeric_limits<float>::signaling_NaN();
        for (std::uint16_t slot = 1; slot < numSlots; ++slot) // slot 0 remains permanent silence
            for (std::uint16_t c = 0; c < maxChannels; ++c)
            {
                float* const dst = slots[static_cast<std::size_t> (slot) * static_cast<std::size_t> (maxChannels)
                                       + static_cast<std::size_t> (c)];
                YESDAW_RT_FATAL (dst != nullptr);
                for (int i = 0; i < numFrames; ++i)
                    dst[i] = poison;
            }
    }

    const CompiledNode* findCompiledNode (NodeId id) const noexcept
    {
        const auto it = std::lower_bound (idIndex_.begin(), idIndex_.end(), id,
                                          [] (const auto& item, NodeId value) { return item.first < value; });
        if (it == idIndex_.end() || it->first != id)
            return nullptr;

        const std::uint32_t compiledIdx = it->second;
        if (compiledIdx >= compiledNodes_.size())
            return nullptr;

        return &compiledNodes_[compiledIdx];
    }

    GraphId       id_;
    float         identityDc_;
    std::uint32_t canary_ = kCanary;

    std::vector<std::unique_ptr<Node>>            nodeStorage_;
    std::vector<CompiledNode>                     compiledNodes_;
    std::vector<InputSlot>                        inputSlotIndices_;
    std::unique_ptr<float[]>                      floatStorage_;
    std::unique_ptr<double[]>                     doubleStorage_;
    std::unique_ptr<Event[]>                      eventStorage_;
    std::unique_ptr<std::uint32_t[]>              eventSlotCounts_;
    std::unique_ptr<Event[]>                      automationEventStorage_;
    std::vector<float*>                           floatSlotPtrs_;
    std::vector<double*>                          doubleSlotPtrs_;
    std::vector<Event*>                           eventSlotPtrs_;
    BufferPoolLayout                              poolLayout_;
    std::uint16_t                                 numEventSlots_      = 1;
    std::uint32_t                                 maxEventsPerBlock_  = kMaxEventsPerBlock;
    std::int64_t                                  totalLatency_     = 0;
    std::vector<std::atomic<std::uint64_t>>       muteWords_;   // ceil(numNodes/64) words; bit i = node i (ADR-0016)
    SlotIndex                                     masterOutputSlot_ = kSilenceSlot;
    std::uint16_t                                 masterChannels_   = 1;
    std::vector<std::pair<NodeId, std::uint32_t>> idIndex_;
    std::vector<CompiledAutomationLane>           automationLanes_;
    mutable std::vector<DelayCacheEntry>          delayCache_;
    bool                                          isDegenerate_     = true;
    bool                                          blockParallelSafe_ = false;   // ADR-0027

    static inline std::atomic<std::uint64_t> alive_ { 0 };
};

} // namespace yesdaw::engine
