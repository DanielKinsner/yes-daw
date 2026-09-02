#pragma once

#include "engine/Node.h"

#include <span>
#include <utility>
#include <vector>

namespace yesdaw::engine {

// G3.1 / ADR-0047: the Track's ONE merged MIDI stream. Every MIDI source on the Track (each
// Clip's DecodedMidiClipNode today; the live input in G3.10) is a direct input of this node, and
// the compiled graph delivers their streams to it already merged in block-time order (the
// executor's N-event-input law: stable by input order on ties, bounded by the per-block event
// budget, no allocation). This node's own work is nil — it exists so the merged stream has a
// stable graph identity keyed by the Track (projectMixerNodeIdForTrack (track, MidiMerge)) that
// the Track's Instrument reads, and that later MIDI FX can sit behind.
//
// Stateless and block-parallel safe: it carries nothing across blocks.
class MidiMergeNode final : public Node
{
public:
    MidiMergeNode (NodeId id, std::vector<Node*> inputs) noexcept
        : id_ (id), inputs_ (std::move (inputs))
    {
    }

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ false, /*producesEvents*/ true,
                                /*channels*/ 1, /*latencySamples*/ 0, id_, /*blockParallelSafe*/ true };
    }

    std::span<Node* const> directInputs() const noexcept override { return inputs_; }

    void prepare (double, int) override {}

    // The executor has already placed the merged input events in args.events (a producing node's
    // stream starts as a copy of its input stream); nothing to add. The audio block is silence.
    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        for (int c = 0; c < args.audio.numChannels; ++c)
        {
            float* const out = args.audio.channels[c];
            for (int i = 0; i < args.numFrames; ++i)
                out[i] = 0.0f;
        }
    }

    void reset() noexcept override {}
    void release() override {}

    [[nodiscard]] std::size_t inputCount() const noexcept { return inputs_.size(); }

private:
    NodeId id_;
    std::vector<Node*> inputs_;
};

} // namespace yesdaw::engine
