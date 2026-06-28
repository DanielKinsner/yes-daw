// YES DAW — PanNode: equal-power mono->stereo placement behind the Node contract (ADR-0008).
//
// Reads one mono input channel and writes a stereo pair using a constant-power pan law (gL = cos t,
// gR = sin t, with t sweeping 0..pi/2 as pan goes -1..+1, so gL^2 + gR^2 == 1 at every position). The
// pan position is ramped per frame (no zipper, Block-size-independent), and the cos/sin are taken from a
// lookup table built once in prepare() — never std::cos/std::sin in the per-frame read path (the ADR-0008
// per-Block-evaluation rule). The LUT is a per-instance member, so it is ready before any audio-thread
// call with no static-init-order or first-use-on-audio-thread hazard.
//
// Pure C++ — no JUCE — so RTSan/TSan cover process(). NOT in-place eligible: it widens 1 channel to 2.

#pragma once

#include "engine/Node.h"
#include "dsp/LinearRamp.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <span>
#include <vector>

namespace yesdaw::engine {

class PanNode final : public Node
{
public:
    explicit PanNode (NodeId id = 0) noexcept : id_ (id) {}

    NodeProperties properties() const noexcept override
    {
        return NodeProperties { /*producesAudio*/ true, /*producesEvents*/ false,
                                /*channels*/ 2, /*latencySamples*/ 0, id_,     // always widens to stereo
                                /*blockParallelSafe*/ true };
    }

    std::span<Node* const> directInputs() const noexcept override
    {
        return std::span<Node* const> (&input_, input_ != nullptr ? 1u : 0u);
    }

    void prepare (double sampleRate, int /*maxBlockSize*/) override
    {
        const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
        pan_.setRampLength (static_cast<int> (kRampSeconds * sr));
        pan_.snap (std::clamp (requestedPan_.load (std::memory_order_relaxed), -1.0f, 1.0f));

        // Quarter-cosine table over [0, pi/2]: cosTable_[k] = cos(k/(N-1) * pi/2). gL reads it forward,
        // gR reads it mirrored (sin t == cos(pi/2 - t)). Built on the control thread; read-only after.
        cosTable_.resize (kTableSize);
        const double scale = (kPiOver2) / static_cast<double> (kTableSize - 1);
        for (int k = 0; k < kTableSize; ++k)
            cosTable_[static_cast<std::size_t> (k)] = static_cast<float> (std::cos (static_cast<double> (k) * scale));
    }

    void process (const ProcessArgs& args) noexcept YESDAW_RT_HOT override
    {
        if (args.audio.numChannels < 2)
            return;                                  // needs a stereo output pair

        pan_.setTarget (std::clamp (requestedPan_.load (std::memory_order_relaxed), -1.0f, 1.0f));

        float* const L   = args.audio.channels[0];   // mono input arrives here; becomes the left output
        float* const R   = args.audio.channels[1];
        const float* lut = cosTable_.data();
        const int    last = kTableSize - 1;

        for (int i = 0; i < args.numFrames; ++i)
        {
            const float p   = pan_.next();                                   // per-frame ramp -> Block-invariant
            const float t   = (p + 1.0f) * 0.5f;                             // [-1,+1] -> [0,1]
            int         j   = static_cast<int> (t * static_cast<float> (last) + 0.5f);
            j               = j < 0 ? 0 : (j > last ? last : j);
            const float gL  = lut[j];
            const float gR  = lut[last - j];

            const float in  = L[i];                                          // capture mono input first
            L[i] = in * gL;
            R[i] = in * gR;
        }
    }

    void reset() noexcept override
    {
        pan_.snap (std::clamp (requestedPan_.load (std::memory_order_relaxed), -1.0f, 1.0f));
    }

    void release() override { cosTable_.clear(); cosTable_.shrink_to_fit(); }

    // Control-thread setters (the SetPan seam, ADR-0006). Builder wires the input.
    void setPan (float pan) noexcept { requestedPan_.store (std::clamp (pan, -1.0f, 1.0f), std::memory_order_relaxed); }
    void setInput (Node* in) noexcept { input_ = in; }

private:
    static constexpr double kRampSeconds = 0.010;
    static constexpr int    kTableSize   = 2049;   // odd -> centre pan hits an exact LUT entry
    static constexpr double kPiOver2     = 1.5707963267948966;

    NodeId             id_;
    Node*              input_ = nullptr;
    std::atomic<float> requestedPan_ { 0.0f };     // centre by default
    yesdaw::dsp::LinearRamp pan_;
    std::vector<float> cosTable_;
};

} // namespace yesdaw::engine
