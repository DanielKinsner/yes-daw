// YES DAW — headless checks for SumNode / Bus f64 summing (ADR-0007 #6 / ADR-0008), via the Node trait.
//
// Pure C++ + Catch2, no JUCE — runs on the normal matrix AND the RTSan/TSan legs. Proves: plain N-input
// summing is correct; the f64 accumulator preserves a quiet signal a float accumulator would annihilate
// (catastrophic cancellation); and the sum is bit-identical no matter what order the inputs are bound in
// (the canonical sort by producer NodeId — the order-shuffle invariance the compiler relies on).

#include "engine/nodes/SumNode.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::EventStream;
using yesdaw::engine::Node;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::SumNode;
using yesdaw::engine::Transport;

namespace {

SumNode::Input monoInput (yesdaw::engine::NodeId id, const float* buf)
{
    SumNode::Input in;
    in.producerId  = id;
    in.channels[0] = buf;
    return in;
}

// Drive a mono SumNode for one Block over the already-bound inputs; returns the output.
std::vector<float> sumOnce (SumNode& node, int frames)
{
    Node& iface = node;
    iface.prepare (48000.0, frames);

    EventStream events;
    Transport   transport;

    std::vector<float> out (static_cast<std::size_t> (frames), -999.0f);
    float* const channels[1] = { out.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 1 }, events, transport, frames });
    return out;
}

} // namespace

TEST_CASE ("SumNode sums its inputs", "[sum][basic]")
{
    std::vector<float> a (16, 0.5f), b (16, 0.25f), c (16, 0.125f);

    SumNode node (1, /*channels*/ 1);
    node.bindInputs ({ monoInput (10, a.data()), monoInput (20, b.data()), monoInput (30, c.data()) });

    const std::vector<float> out = sumOnce (node, 16);
    for (float v : out)
        REQUIRE (v == 0.875f);          // 0.5 + 0.25 + 0.125, exact
}

TEST_CASE ("SumNode f64 accumulator survives catastrophic cancellation", "[sum][f64]")
{
    // Summed in the canonical NodeId order [big, small, -big]: a float accumulator computes
    // (1e8f + 1.0f) == 1e8f (the float gap at 1e8 is 8, so the 1.0 is lost), then - 1e8f == 0. The f64
    // accumulator has ~1e-8 resolution at 1e8, so it keeps the 1.0. (1e8 is exactly representable in both
    // float and double, so the difference is purely the accumulator's precision, not input rounding.)
    std::vector<float> big (4, 1.0e8f), small (4, 1.0f), negBig (4, -1.0e8f);

    SumNode node (1, /*channels*/ 1);
    node.bindInputs ({ monoInput (1, big.data()), monoInput (2, small.data()), monoInput (3, negBig.data()) });

    const std::vector<float> out = sumOnce (node, 4);
    for (float v : out)
        REQUIRE (v == 1.0f);            // exactly 1.0 — would be 0.0 under float accumulation
}

TEST_CASE ("SumNode output is independent of input bind order", "[sum][determinism]")
{
    // 32 distinct sine sources; summing them in different float-add orders can differ in the last ULP, so
    // SumNode canonicalises by producer NodeId. Binding the same inputs in a shuffled order must produce a
    // bit-identical result.
    constexpr int kSources = 32;
    constexpr int kFrames  = 64;
    std::vector<std::vector<float>> bufs (kSources, std::vector<float> (kFrames));
    for (int k = 0; k < kSources; ++k)
        for (int i = 0; i < kFrames; ++i)
            bufs[static_cast<std::size_t> (k)][static_cast<std::size_t> (i)]
                = static_cast<float> (0.1 * std::sin (0.37 * k + 0.019 * i));

    std::vector<SumNode::Input> forward, reversed;
    for (int k = 0; k < kSources; ++k)
        forward.push_back (monoInput (static_cast<yesdaw::engine::NodeId> (k), bufs[static_cast<std::size_t> (k)].data()));
    for (int k = kSources - 1; k >= 0; --k)
        reversed.push_back (monoInput (static_cast<yesdaw::engine::NodeId> (k), bufs[static_cast<std::size_t> (k)].data()));

    SumNode na (1, 1), nb (2, 1);
    na.bindInputs (forward);
    nb.bindInputs (reversed);

    const std::vector<float> outA = sumOnce (na, kFrames);
    const std::vector<float> outB = sumOnce (nb, kFrames);

    for (int i = 0; i < kFrames; ++i)
        REQUIRE (outA[static_cast<std::size_t> (i)] == outB[static_cast<std::size_t> (i)]);   // bit-identical
}

TEST_CASE ("SumNode sums each channel of a stereo bus independently", "[sum][stereo]")
{
    std::vector<float> aL (8, 0.5f), aR (8, -0.5f);
    std::vector<float> bL (8, 0.25f), bR (8, 0.75f);

    SumNode::Input ia, ib;
    ia.producerId = 10; ia.channels[0] = aL.data(); ia.channels[1] = aR.data();
    ib.producerId = 20; ib.channels[0] = bL.data(); ib.channels[1] = bR.data();

    SumNode node (1, /*channels*/ 2);
    Node& iface = node;
    iface.prepare (48000.0, 8);
    node.bindInputs ({ ia, ib });

    EventStream events;
    Transport   transport;
    std::vector<float> outL (8, 0.0f), outR (8, 0.0f);
    float* const channels[2] = { outL.data(), outR.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 2 }, events, transport, 8 });

    for (int i = 0; i < 8; ++i)
    {
        REQUIRE (outL[static_cast<std::size_t> (i)] == 0.75f);    // 0.5 + 0.25
        REQUIRE (outR[static_cast<std::size_t> (i)] == 0.25f);    // -0.5 + 0.75
    }
}
