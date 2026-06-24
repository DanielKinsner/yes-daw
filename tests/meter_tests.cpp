// YES DAW — headless checks for MeterNode (ADR-0008), driven through the Node trait.
//
// Pure C++ + Catch2, no JUCE — runs on the normal matrix AND the RTSan/TSan legs. Proves the meter is a
// true passthrough (it never alters the signal), reports the correct Block peak and RMS, and publishes
// them through the acquire/release atomics the UI reads.

#include "engine/nodes/MeterNode.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <vector>

using Catch::Approx;
using yesdaw::engine::AudioBlock;
using yesdaw::engine::EventStream;
using yesdaw::engine::MeterNode;
using yesdaw::engine::Node;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Transport;

namespace {

void meter (MeterNode& node, std::vector<float>& buf)
{
    Node& iface = node;
    iface.prepare (48000.0, static_cast<int> (buf.size()));
    EventStream events;
    Transport   transport;
    float* const channels[1] = { buf.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 1 }, events, transport, static_cast<int> (buf.size()) });
}

} // namespace

TEST_CASE ("MeterNode leaves the signal untouched", "[meter][passthrough]")
{
    std::vector<float> buf (256);
    for (std::size_t i = 0; i < buf.size(); ++i)
        buf[i] = static_cast<float> (std::sin (0.05 * static_cast<double> (i)));
    const std::vector<float> before = buf;

    MeterNode node (1);
    meter (node, buf);

    for (std::size_t i = 0; i < buf.size(); ++i)
        REQUIRE (buf[i] == before[i]);     // a meter is a tap, not a processor
}

TEST_CASE ("MeterNode reports the Block peak", "[meter][peak]")
{
    std::vector<float> buf (128, 0.1f);
    buf[64] = -0.8f;                       // a single negative spike sets the peak (max |sample|)

    MeterNode node (1);
    meter (node, buf);

    REQUIRE (node.peak() == Approx (0.8f).margin (1.0e-6));
}

TEST_CASE ("MeterNode reports RMS", "[meter][rms]")
{
    SECTION ("constant signal")
    {
        std::vector<float> buf (512, 0.5f);
        MeterNode node (1);
        meter (node, buf);
        REQUIRE (node.rms() == Approx (0.5f).margin (1.0e-6));   // rms of a constant c is |c|
    }

    SECTION ("full-cycle sine")
    {
        const int n = 2000;
        std::vector<float> buf (static_cast<std::size_t> (n));
        const double amp = 0.6;
        for (int i = 0; i < n; ++i)
            buf[static_cast<std::size_t> (i)] = static_cast<float> (amp * std::sin (2.0 * 3.14159265358979 * i / n));

        MeterNode node (1);
        meter (node, buf);
        REQUIRE (node.rms() == Approx (amp / std::sqrt (2.0)).margin (1.0e-3));   // sine rms is A/sqrt(2)
    }
}

TEST_CASE ("MeterNode reset() clears the published metrics", "[meter][reset]")
{
    std::vector<float> buf (64, 0.9f);
    MeterNode node (1);
    meter (node, buf);
    REQUIRE (node.peak() > 0.0f);

    node.reset();
    REQUIRE (node.peak() == 0.0f);
    REQUIRE (node.rms()  == 0.0f);
}

TEST_CASE ("MeterNode aggregates peak and RMS across channels", "[meter][multichannel]")
{
    // The H1 coverage review found every meter test used a 1-channel block, so the multichannel
    // aggregation loop (peak/RMS across c = 0..channels) was never exercised. Stereo meters appear in
    // real graphs (PanNode outputs 2 channels), so prove the loop: a spike in the RIGHT channel only
    // must still set the overall peak, and RMS must pool BOTH channels' energy.
    constexpr int n = 256;
    std::vector<float> left  (static_cast<std::size_t> (n), 0.2f);
    std::vector<float> right (static_cast<std::size_t> (n), 0.2f);
    right[100] = -0.9f;                          // a spike in the right channel only

    MeterNode node (5, 2);                        // id 5, TWO channels
    Node& iface = node;
    iface.prepare (48000.0, n);
    EventStream events;
    Transport   transport;
    float* const channels[2] = { left.data(), right.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 2 }, events, transport, n });

    REQUIRE (node.peak() == Approx (0.9f).margin (1.0e-6));   // the peak spans BOTH channels

    const double sumSq = (2.0 * n - 1.0) * (0.2 * 0.2) + (0.9 * 0.9);
    const double expectedRms = std::sqrt (sumSq / (2.0 * n));
    REQUIRE (node.rms() == Approx (expectedRms).margin (1.0e-4));   // RMS pools both channels
}
