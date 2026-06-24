// YES DAW — headless checks for the H0 spike #3 Node-trait stub (block-size independence).
//
// Pure C++ + Catch2, no JUCE. Proves a stateful node behind the trait produces the SAME audio
// regardless of how it's sliced into process() calls — the contract property a real host stresses
// with odd/tiny/huge block sizes. A node that reset or dropped state at a block boundary would fail.

#include "engine/Node.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

namespace {

constexpr double kSr    = 48000.0;
constexpr int    kTotal = 8192;   // total samples to render each way

// Drive a fresh ToneNode through the Node interface in fixed-size blocks until `kTotal` is filled.
std::vector<float> renderInBlocks (int blockSize)
{
    yesdaw::engine::ToneNode node;
    yesdaw::engine::Node& iface = node;   // exercise the polymorphic trait, not the concrete type
    iface.prepare (kSr);

    std::vector<float> out (static_cast<size_t> (kTotal), 0.0f);
    int done = 0;
    while (done < kTotal)
    {
        const int n = std::min (blockSize, kTotal - done);
        iface.process (out.data() + done, n);
        done += n;
    }
    return out;
}

} // namespace

TEST_CASE ("a node's output is identical across block sizes", "[node][blocksize]")
{
    // Odd, tiny, power-of-two, and larger-than-output block sizes — the awkward cases a host throws.
    const std::vector<float> reference = renderInBlocks (512);

    for (const int blockSize : { 1, 31, 128, 512, 4096, 9000 })
    {
        const std::vector<float> got = renderInBlocks (blockSize);
        REQUIRE (got.size() == reference.size());

        double maxDiff = 0.0;
        for (size_t i = 0; i < got.size(); ++i)
            maxDiff = std::max (maxDiff, std::fabs (static_cast<double> (got[i] - reference[i])));

        INFO ("block size " << blockSize << " vs reference: max diff = " << maxDiff);
        REQUIRE (maxDiff == 0.0);   // block slicing must not change a single sample
    }
}

TEST_CASE ("a node's output is finite and free of denormals", "[node][finite]")
{
    const std::vector<float> out = renderInBlocks (333);
    for (float v : out)
    {
        REQUIRE (std::isfinite (v));
        const float a = std::fabs (v);
        REQUIRE ((a == 0.0f || a > 1.0e-30f));   // no denormals to stall the audio thread
    }
}
