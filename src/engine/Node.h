// YES DAW — the Node contract (ADR-0008): one CLAP-shaped, format-neutral processing unit.
//
// Built-in DSP and (at H3) hosted plugins implement this same trait, so adding plugin hosting later is
// an adapter, not an engine rewrite (ADR-0002 #3). The graph compiler (ADR-0007), PDC, the buffer pool,
// and the event router all program against this shape — changing it later touches every Node at once,
// so it is frozen here. process() is the audio hot path; prepare() is the ONLY place a Node allocates.
//
// Pure C++ — no JUCE — so every Node is covered by the RTSan/TSan legs. (PluginNode, the one adapter
// that wraps juce::AudioProcessor, lives behind a layering boundary — ADR-0008 — never in this header.)
//
// NOTE: this replaces the throwaway H0 spike trait that proved block-size independence; that property is
// a real contract rule and is re-asserted against the built-in Nodes (tests/node_tests.cpp).

#pragma once

#include "engine/Time.h"
#include "rt/RtHot.h"

#include <cstdint>
#include <span>

namespace yesdaw::engine {

using NodeId = std::uint32_t;

// What a Node advertises to the compiler. latencySamples drives PDC; channels/produces* drive routing.
struct NodeProperties
{
    bool         producesAudio  = false;
    bool         producesEvents = false;
    int          channels       = 0;
    std::int64_t latencySamples = 0;
    NodeId       id             = 0;
};

// A view over the per-channel audio buffers a Node reads/writes this Block. The frame count travels in
// ProcessArgs (variable Block, ADR-0010): each channels[c] points to at least ProcessArgs::numFrames floats.
struct AudioBlock
{
    float* const* channels    = nullptr;
    int           numChannels = 0;
};

// Placeholder for the event contract that flows through process() but is frozen in ADR-0009 — present
// now so ProcessArgs has its frozen shape from the first Node. A Node that consumes no events simply
// ignores it.
struct EventStream { /* ADR-0009: sample-accurate, block-sliced events. */ };

struct ProcessArgs
{
    AudioBlock       audio;
    EventStream&     events;
    const Transport& transport;
    int              numFrames = 0;   // <= the maxBlockSize passed to prepare()
};

// The trait every processing unit implements.
class Node
{
public:
    virtual ~Node() = default;

    // Advertised properties (the compiler reads these every recompile). Cheap, RT-safe.
    virtual NodeProperties properties() const noexcept = 0;

    // The Nodes feeding this one — the graph compiler walks these for topo order + PDC. A source/leaf
    // returns an empty span. Edges are wired by the graph builder (ADR-0007).
    virtual std::span<Node* const> directInputs() const noexcept = 0;

    // Allocate + size everything for a sample rate and maximum Block. The ONLY place a Node may allocate.
    // Not RT-safe; called from the control thread before the Node goes live.
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;

    // The audio hot path: read/transform/write args.audio for args.numFrames frames. RT-safe — no
    // allocation, lock, or syscall (enforced by RTSan). numFrames <= maxBlockSize.
    virtual void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT = 0;

    // Drop transient state (envelopes, delay lines) to zero without reallocating. RT-safe.
    virtual void reset() noexcept = 0;

    // Free what prepare() allocated. Not RT-safe; control thread.
    virtual void release() = 0;
};

} // namespace yesdaw::engine
