// YES DAW — headless checks for PanNode (ADR-0008), driven through the Node trait.
//
// Pure C++ + Catch2, no JUCE — runs on the normal matrix AND the RTSan/TSan legs. Proves the equal-power
// law (constant power across the sweep), the hard-left / centre / hard-right placements, and Block-size
// invariance of a pan ramp (the LUT-per-frame ramp must not depend on slicing).

#include "engine/nodes/PanNode.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

using Catch::Approx;
using yesdaw::engine::AudioBlock;
using yesdaw::engine::Event;
using yesdaw::engine::EventStream;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::Node;
using yesdaw::engine::PanNode;
using yesdaw::engine::ProcessArgs;
using yesdaw::engine::Transport;

namespace {

constexpr double kSr = 48000.0;

// Render a settled pan (snapped before prepare) over a constant mono input; returns {L, R}.
std::pair<std::vector<float>, std::vector<float>> renderSettledPan (float pan, int frames)
{
    PanNode node (1);
    node.setPan (pan);                 // set BEFORE prepare so it starts settled (no ramp)
    Node& iface = node;
    iface.prepare (kSr, frames);

    EventStream events;
    Transport   transport;

    std::vector<float> left  (static_cast<std::size_t> (frames), 1.0f);   // mono input == 1.0 in channel 0
    std::vector<float> right (static_cast<std::size_t> (frames), 0.0f);
    float* const channels[2] = { left.data(), right.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 2 }, events, transport, frames });
    return { left, right };
}

std::pair<std::vector<float>, std::vector<float>> renderPanWithEvent (double normalizedValue, int frames = 512)
{
    PanNode node (1);
    Node& iface = node;
    iface.prepare (kSr, frames);

    const Event evs[1] = { makeParameterChangeEvent (/*timeInBlock*/ 0, /*targetNode*/ 1,
                                                     PanNode::kPanParameterId, normalizedValue) };
    EventStream events (std::span<const Event> (evs, 1));
    Transport   transport;

    std::vector<float> left  (static_cast<std::size_t> (frames), 1.0f);
    std::vector<float> right (static_cast<std::size_t> (frames), 0.0f);
    float* const channels[2] = { left.data(), right.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 2 }, events, transport, frames });
    return { left, right };
}

std::pair<std::vector<float>, std::vector<float>> renderPanWithAutomationSideBand (double normalizedValue,
                                                                                   int frames = 512)
{
    PanNode node (1);
    Node& iface = node;
    iface.prepare (kSr, frames);

    const Event regularEvents[1] = { makeParameterChangeEvent (/*timeInBlock*/ 0, /*targetNode*/ 99,
                                                               PanNode::kPanParameterId, 0.0) };
    const Event automationEvents[1] = { makeParameterChangeEvent (/*timeInBlock*/ 0, /*targetNode*/ 1,
                                                                  PanNode::kPanParameterId, normalizedValue) };
    EventStream regularStream (std::span<const Event> (regularEvents, 1));
    EventStream automationStream (std::span<const Event> (automationEvents, 1));
    Transport   transport;

    std::vector<float> left  (static_cast<std::size_t> (frames), 1.0f);
    std::vector<float> right (static_cast<std::size_t> (frames), 0.0f);
    float* const channels[2] = { left.data(), right.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 2 }, regularStream, transport, frames, &automationStream });
    return { left, right };
}

} // namespace

TEST_CASE ("PanNode centre is equal power (-3 dB both sides)", "[pan][centre]")
{
    auto [L, R] = renderSettledPan (0.0f, 256);
    const float g = std::sqrt (0.5f);                 // ~0.70710678
    for (std::size_t i = 0; i < L.size(); ++i)
    {
        REQUIRE (L[i] == Approx (g).margin (1.0e-4));
        REQUIRE (R[i] == Approx (g).margin (1.0e-4));
    }
}

TEST_CASE ("PanNode hard left and hard right route to one side", "[pan][extremes]")
{
    {
        auto [L, R] = renderSettledPan (-1.0f, 64);
        REQUIRE (L[0] == Approx (1.0f).margin (1.0e-4));
        REQUIRE (R[0] == Approx (0.0f).margin (1.0e-4));
    }
    {
        auto [L, R] = renderSettledPan (1.0f, 64);
        REQUIRE (L[0] == Approx (0.0f).margin (1.0e-4));
        REQUIRE (R[0] == Approx (1.0f).margin (1.0e-4));
    }
}

TEST_CASE ("PanNode holds constant power across the sweep", "[pan][constant-power]")
{
    for (const float p : { -1.0f, -0.5f, -0.2f, 0.0f, 0.3f, 0.7f, 1.0f })
    {
        auto [L, R] = renderSettledPan (p, 8);
        const float power = L[0] * L[0] + R[0] * R[0];
        INFO ("pan " << p << " power " << power);
        REQUIRE (power == Approx (1.0f).margin (1.0e-3));   // gL^2 + gR^2 == 1
    }
}

TEST_CASE ("PanNode output is identical across Block sizes during a pan sweep", "[pan][blocksize]")
{
    const int total = 4096;

    auto render = [] (int blockSize)
    {
        PanNode node (1);
        Node& iface = node;
        iface.prepare (kSr, blockSize);
        node.setPan (1.0f);             // ramp from centre toward hard right

        EventStream events;
        Transport   transport;

        std::vector<float> L (static_cast<std::size_t> (total), 1.0f);
        std::vector<float> R (static_cast<std::size_t> (total), 0.0f);
        int done = 0;
        while (done < total)
        {
            const int n = std::min (blockSize, total - done);
            float* const ch[2] = { L.data() + done, R.data() + done };
            iface.process (ProcessArgs { AudioBlock { ch, 2 }, events, transport, n });
            done += n;
        }
        return std::pair { L, R };
    };

    auto [refL, refR] = render (512);
    for (const int blockSize : { 1, 31, 113, 128, 512, 9000 })
    {
        auto [gotL, gotR] = render (blockSize);
        double maxDiff = 0.0;
        for (std::size_t i = 0; i < refL.size(); ++i)
        {
            maxDiff = std::max (maxDiff, std::fabs (static_cast<double> (gotL[i] - refL[i])));
            maxDiff = std::max (maxDiff, std::fabs (static_cast<double> (gotR[i] - refR[i])));
        }
        INFO ("block size " << blockSize << " max diff " << maxDiff);
        REQUIRE (maxDiff == 0.0);
    }
}

TEST_CASE ("PanNode maps normalized pan events to the -1..+1 pan domain", "[pan][automation][paramspec]")
{
    REQUIRE (PanNode::panForNormalizedEvent (0.0) == -1.0f);
    REQUIRE (PanNode::panForNormalizedEvent (0.5) == 0.0f);
    REQUIRE (PanNode::panForNormalizedEvent (1.0) == 1.0f);

    auto [L, R] = renderPanWithEvent (1.0);

    REQUIRE (L.front() < std::sqrt (0.5f));
    REQUIRE (R.front() > std::sqrt (0.5f));
    REQUIRE (L.back() == Approx (0.0f).margin (1.0e-4));
    REQUIRE (R.back() == Approx (1.0f).margin (1.0e-4));

    // Negative control for the pre-H15 ignored-event path: this would remain centre-panned.
    REQUIRE (std::fabs (static_cast<double> (R.back()) - std::sqrt (0.5)) > 0.25);
}

TEST_CASE ("PanNode automation event target persists until a SetPan command changes it", "[pan][automation][persist]")
{
    PanNode node (1);
    Node& iface = node;
    iface.prepare (kSr, 512);

    const Event evs[1] = { makeParameterChangeEvent (/*timeInBlock*/ 0, /*targetNode*/ 1,
                                                     PanNode::kPanParameterId, 1.0) };
    EventStream events (std::span<const Event> (evs, 1));
    EventStream emptyEvents;
    Transport   transport;

    std::vector<float> firstL (512, 1.0f);
    std::vector<float> firstR (512, 0.0f);
    float* const firstChannels[2] = { firstL.data(), firstR.data() };
    iface.process (ProcessArgs { AudioBlock { firstChannels, 2 }, events, transport, 512 });

    std::vector<float> secondL (64, 1.0f);
    std::vector<float> secondR (64, 0.0f);
    float* const secondChannels[2] = { secondL.data(), secondR.data() };
    iface.process (ProcessArgs { AudioBlock { secondChannels, 2 }, emptyEvents, transport, 64 });

    REQUIRE (secondL.back() == Approx (0.0f).margin (1.0e-4));
    REQUIRE (secondR.back() == Approx (1.0f).margin (1.0e-4));

    node.setPan (-1.0f);
    std::vector<float> thirdL (512, 1.0f);
    std::vector<float> thirdR (512, 0.0f);
    float* const thirdChannels[2] = { thirdL.data(), thirdR.data() };
    iface.process (ProcessArgs { AudioBlock { thirdChannels, 2 }, emptyEvents, transport, 512 });

    REQUIRE (thirdL.back() == Approx (1.0f).margin (1.0e-4));
    REQUIRE (thirdR.back() == Approx (0.0f).margin (1.0e-4));
}

TEST_CASE ("PanNode consumes H15 automation side-band events", "[pan][automation][sideband]")
{
    auto [L, R] = renderPanWithAutomationSideBand (0.0);

    REQUIRE (L.front() > std::sqrt (0.5f));
    REQUIRE (R.front() < std::sqrt (0.5f));
    REQUIRE (L.back() == Approx (1.0f).margin (1.0e-4));
    REQUIRE (R.back() == Approx (0.0f).margin (1.0e-4));
}

TEST_CASE ("PanNode clamps a pathological automation event and never emits NaN/inf", "[pan][automation][robust]")
{
    const double inf = std::numeric_limits<double>::infinity();
    for (const double bad : { std::nan (""), inf, -inf, 1.0e30, -5.0 })
    {
        auto [L, R] = renderPanWithEvent (bad);
        for (std::size_t i = 0; i < L.size(); ++i)
        {
            REQUIRE (std::isfinite (L[i]));
            REQUIRE (std::isfinite (R[i]));
            REQUIRE (L[i] >= 0.0f);
            REQUIRE (R[i] >= 0.0f);
            REQUIRE (L[i] <= 1.0f);
            REQUIRE (R[i] <= 1.0f);
        }
    }
}
