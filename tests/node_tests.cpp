// YES DAW — headless checks for the Node contract (ADR-0008), driven through the trait.
//
// Pure C++ + Catch2, no JUCE. Proves a stateful built-in Node (OscillatorNode) produces the SAME audio
// regardless of how it is sliced into process() Blocks — the contract property a real host stresses with
// odd/tiny/huge Block sizes. A Node that reset or dropped state at a Block boundary would fail. This is
// the H0 spike property, re-asserted against the real Node contract instead of the throwaway stub.

#include "engine/Node.h"
#include "engine/nodes/OscillatorNode.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr double kSr    = 48000.0;
constexpr int    kTotal = 8192;   // total frames to render each way

// Drive a fresh OscillatorNode through the Node interface in fixed-size Blocks until kTotal is filled.
std::vector<float> renderInBlocks (int blockSize)
{
    yesdaw::engine::OscillatorNode node;
    yesdaw::engine::Node& iface = node;          // exercise the polymorphic trait, not the concrete type
    iface.prepare (kSr, blockSize);

    yesdaw::engine::EventStream events;          // ignored by the oscillator (ADR-0009 fills these later)
    yesdaw::engine::Transport   transport;       // ignored (ADR-0010)

    std::vector<float> out (static_cast<size_t> (kTotal), 0.0f);
    int done = 0;
    while (done < kTotal)
    {
        const int n = std::min (blockSize, kTotal - done);
        float* const channels[1] = { out.data() + done };
        const yesdaw::engine::ProcessArgs args {
            yesdaw::engine::AudioBlock { channels, 1 }, events, transport, n };
        iface.process (args);
        done += n;
    }
    return out;
}

} // namespace

TEST_CASE ("a Node's output is identical across block sizes", "[node][blocksize]")
{
    // Odd, tiny, power-of-two, and larger-than-output Block sizes — the awkward cases a host throws.
    const std::vector<float> reference = renderInBlocks (512);

    for (const int blockSize : { 1, 31, 128, 512, 4096, 9000 })
    {
        const std::vector<float> got = renderInBlocks (blockSize);
        REQUIRE (got.size() == reference.size());

        double maxDiff = 0.0;
        for (size_t i = 0; i < got.size(); ++i)
            maxDiff = std::max (maxDiff, std::fabs (static_cast<double> (got[i] - reference[i])));

        INFO ("block size " << blockSize << " vs reference: max diff = " << maxDiff);
        REQUIRE (maxDiff == 0.0);   // Block slicing must not change a single sample
    }
}

TEST_CASE ("a Node's output is finite and free of denormals", "[node][finite]")
{
    const std::vector<float> out = renderInBlocks (333);
    for (float v : out)
    {
        REQUIRE (std::isfinite (v));
        const float a = std::fabs (v);
        REQUIRE ((a == 0.0f || a > 1.0e-30f));   // no denormals to stall the audio thread
    }
}

TEST_CASE ("a Node advertises its properties through the trait", "[node][properties]")
{
    yesdaw::engine::OscillatorNode node (7);
    const yesdaw::engine::Node& iface = node;
    const auto props = iface.properties();

    REQUIRE (props.producesAudio);
    REQUIRE_FALSE (props.producesEvents);
    REQUIRE (props.channels == 1);
    REQUIRE (props.latencySamples == 0);
    REQUIRE (props.id == 7u);
    REQUIRE (iface.directInputs().empty());      // a source/leaf has no inputs
}
