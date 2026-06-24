// YES DAW — headless checks for FaderNode (ADR-0008), driven through the Node trait.
//
// Pure C++ + Catch2, no JUCE — runs on the normal matrix AND the RTSan/TSan legs. Proves the gain ramp is
// click-free, settles exactly on target, applies the SAME gain to every channel per frame, and — the
// load-bearing property — is bit-identical regardless of how the host slices process() into Blocks.

#include "engine/nodes/FaderNode.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using yesdaw::engine::AudioBlock;
using yesdaw::engine::EventStream;
using yesdaw::engine::FaderNode;
using yesdaw::engine::Node;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Transport;

namespace {

constexpr double kSr = 48000.0;

// Render a mono FaderNode over a constant-1.0 stream in fixed-size Blocks. `snapBeforePrepare` chooses
// whether the target gain is set before prepare() (so it starts settled — the steady fast path) or after
// (so it ramps from unity — the ramp path).
std::vector<float> renderFader (int blockSize, float targetGain, int totalFrames, bool snapBeforePrepare)
{
    FaderNode node (1, /*channels*/ 1);
    if (snapBeforePrepare)
        node.setTargetGain (targetGain);

    Node& iface = node;
    iface.prepare (kSr, blockSize);

    if (! snapBeforePrepare)
        node.setTargetGain (targetGain);

    EventStream events;
    Transport   transport;

    std::vector<float> out (static_cast<std::size_t> (totalFrames), 1.0f);   // constant input
    int done = 0;
    while (done < totalFrames)
    {
        const int n = std::min (blockSize, totalFrames - done);
        float* const channels[1] = { out.data() + done };
        iface.process (ProcessArgs { AudioBlock { channels, 1 }, events, transport, n });
        done += n;
    }
    return out;
}

} // namespace

TEST_CASE ("FaderNode at a settled gain is a constant multiply", "[fader][steady]")
{
    const std::vector<float> out = renderFader (128, 0.5f, 512, /*snapBeforePrepare*/ true);
    for (float v : out)
        REQUIRE (v == 0.5f);          // settled before the first Block -> no ramp, exact
}

TEST_CASE ("FaderNode ramps to its target and settles there", "[fader][ramp]")
{
    const std::vector<float> out = renderFader (64, 0.5f, 2048, /*snapBeforePrepare*/ false);

    REQUIRE (out.front() < 1.0f);     // already moving away from unity on the first frame
    REQUIRE (out.front() > 0.5f);
    REQUIRE (out.back()  == 0.5f);    // landed exactly on target

    for (std::size_t i = 1; i < out.size(); ++i)
        REQUIRE (out[i] <= out[i - 1] + 1.0e-7f);   // monotonic non-increasing (no zipper/overshoot)
}

TEST_CASE ("FaderNode output is identical across Block sizes during a ramp", "[fader][blocksize]")
{
    const int total = 4096;
    const std::vector<float> reference = renderFader (512, 0.5f, total, /*snapBeforePrepare*/ false);

    for (const int blockSize : { 1, 31, 113, 128, 512, 9000 })
    {
        const std::vector<float> got = renderFader (blockSize, 0.5f, total, /*snapBeforePrepare*/ false);
        REQUIRE (got.size() == reference.size());

        double maxDiff = 0.0;
        for (std::size_t i = 0; i < got.size(); ++i)
            maxDiff = std::max (maxDiff, std::fabs (static_cast<double> (got[i] - reference[i])));

        INFO ("block size " << blockSize << " vs reference: max diff = " << maxDiff);
        REQUIRE (maxDiff == 0.0);     // the ramp advances per frame, so slicing changes nothing
    }
}

TEST_CASE ("FaderNode applies the same ramped gain to every channel", "[fader][multichannel]")
{
    FaderNode node (1, /*channels*/ 2);
    Node& iface = node;
    iface.prepare (kSr, 128);
    node.setTargetGain (0.25f);       // ramp from unity

    EventStream events;
    Transport   transport;

    // Both channels carry the same input; after a ramped Block they must remain identical sample-for-sample.
    std::vector<float> left  (1024, 1.0f);
    std::vector<float> right (1024, 1.0f);
    float* const channels[2] = { left.data(), right.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 2 }, events, transport, 1024 });

    for (std::size_t i = 0; i < left.size(); ++i)
        REQUIRE (left[i] == right[i]);   // same gain trajectory on both channels
    REQUIRE (left.back() == 0.25f);      // and the ramp settled within the Block
}
