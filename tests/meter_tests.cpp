// YES DAW — headless checks for MeterNode (ADR-0008), driven through the Node trait.
//
// Pure C++ + Catch2, no JUCE — runs on the normal matrix AND the RTSan/TSan legs. Proves the meter is a
// true passthrough (it never alters the signal), reports the correct Block peak and RMS, and publishes
// them through the acquire/release atomics the UI reads.

#include "engine/nodes/MeterNode.h"
#include "engine/PeakSinceRead.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <atomic>
#include <cmath>
#include <thread>
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

    REQUIRE (node.readPeak().peak == Approx (0.8f).margin (1.0e-6));
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

TEST_CASE ("MeterNode reset() leaves the peak reading intact and clears RMS",
           "[meter][reset][g6-motion][peak-since-read]")
{
    // ADR-0067 §5: a reset is NOT a block — it runs on the audio thread between blocks and must
    // not drop the running window that spans several blocks between two UI reads. RMS is "this
    // block's", so it still falls on reset.
    std::vector<float> buf (64, 0.9f);
    MeterNode node (1);
    meter (node, buf);
    REQUIRE (node.rms() > 0.0f);

    node.reset();
    REQUIRE (node.rms()  == 0.0f);
    // The peak reading is still the block's loud 0.9: a reset did not swallow it.
    const auto reading = node.readPeak();
    REQUIRE (reading.fresh);
    REQUIRE (reading.peak == Approx (0.9f).margin (1.0e-6));
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

    REQUIRE (node.readPeak().peak == Approx (0.9f).margin (1.0e-6));   // the peak spans BOTH channels

    const double sumSq = (2.0 * n - 1.0) * (0.2 * 0.2) + (0.9 * 0.9);
    const double expectedRms = std::sqrt (sumSq / (2.0 * n));
    REQUIRE (node.rms() == Approx (expectedRms).margin (1.0e-4));   // RMS pools both channels
}

TEST_CASE ("MeterNode publishes independent per-channel peak and RMS", "[meter][multichannel][per-channel]")
{
    // A real stereo meter shows L and R independently, not just one pooled scalar (H3 stereo metering).
    constexpr int n = 256;
    std::vector<float> left  (static_cast<std::size_t> (n), 0.5f);   // L: a flat 0.5
    std::vector<float> right (static_cast<std::size_t> (n), 0.1f);   // R: a flat 0.1...
    right[10] = -0.8f;                                                // ...with one spike

    MeterNode node (7, 2);
    Node& iface = node;
    iface.prepare (48000.0, n);
    EventStream events;
    Transport   transport;
    float* const channels[2] = { left.data(), right.data() };
    iface.process (ProcessArgs { AudioBlock { channels, 2 }, events, transport, n });

    // Per-channel peaks are distinct: L is flat 0.5, R peaks at its 0.8 spike.
    REQUIRE (node.readPeak (0).peak == Approx (0.5f).margin (1.0e-6));
    REQUIRE (node.readPeak (1).peak == Approx (0.8f).margin (1.0e-6));

    // Per-channel RMS is each channel's own energy, not the pooled value.
    REQUIRE (node.rms (0) == Approx (0.5).margin (1.0e-4));   // constant 0.5 -> rms 0.5
    const double rRms = std::sqrt (((n - 1) * (0.1 * 0.1) + (0.8 * 0.8)) / static_cast<double> (n));
    REQUIRE (node.rms (1) == Approx (rRms).margin (1.0e-4));

    // The aggregate stays the max across channels, and an out-of-range channel reads 0.
    REQUIRE (node.readPeak().peak == Approx (0.8f).margin (1.0e-6));
    REQUIRE (node.readPeak (5).peak == 0.0f);
    REQUIRE_FALSE (node.readPeak (5).fresh);
    REQUIRE (node.rms  (5) == 0.0f);
}

// ---- ADR-0067 §5: PeakSinceRead — every block since the last read, silence when none ran -------------------------

TEST_CASE ("PeakSinceRead reads the max of every block since the last read, and silence when none ran",
           "[meter][g6-motion][peak-since-read]")
{
    yesdaw::engine::PeakSinceRead source;
    REQUIRE_FALSE (source.read().fresh);   // a fresh source: nothing ran
    REQUIRE (source.read().peak == 0.0f);

    source.publishBlock (0.2f);
    source.publishBlock (0.9f);   // the loud block is not the last
    source.publishBlock (0.1f);
    const auto first = source.read();
    REQUIRE (first.fresh);
    REQUIRE (first.peak == 0.9f);
    REQUIRE (first.throughBlock == 3u);

    const auto none = source.read();   // no block since: silence, whatever the last value was
    REQUIRE_FALSE (none.fresh);
    REQUIRE (none.peak == 0.0f);

    source.publishBlock (0.3f);   // a new window starts after the block the reader took
    REQUIRE (source.read().peak == 0.3f);
}

TEST_CASE ("PeakSinceRead never drops a block that ran while the reader was between its loads and its store",
           "[meter][g6-motion][peak-since-read]")
{
    yesdaw::engine::PeakSinceRead source;
    source.publishBlock (0.2f);
    // The writer runs two blocks inside the reader's gap: the reading is taken through block 1, the two are carried.
    const auto first = source.read ([&source] {
        source.publishBlock (0.8f);
        source.publishBlock (0.4f);
    });
    REQUIRE (first.peak >= 0.2f);
    REQUIRE (first.throughBlock == 1u);
    source.publishBlock (0.1f);
    const auto second = source.read();
    REQUIRE (second.fresh);
    REQUIRE (second.peak == 0.8f);   // the gap's loud block, not lost to the new window

    // A reader five blocks behind (more than the carry holds) still reads at least every block's peak.
    yesdaw::engine::PeakSinceRead behind;
    behind.publishBlock (0.1f);
    (void) behind.read ([&behind] {
        for (const float peak : { 0.2f, 0.95f, 0.2f, 0.2f, 0.2f })
            behind.publishBlock (peak);
    });
    behind.publishBlock (0.1f);
    REQUIRE (behind.read().peak >= 0.95f);
}

TEST_CASE ("PeakSinceRead under two threads: the first reading through each loud block is at least its peak",
           "[meter][g6-motion][peak-since-read][threads]")
{
    constexpr std::uint64_t kBlocks = 20'000;
    yesdaw::engine::PeakSinceRead source;
    std::atomic<bool> done { false };
    std::vector<yesdaw::engine::PeakSinceRead::Reading> readings;
    readings.reserve (kBlocks + 1);
    const auto peakOf = [] (std::uint64_t block) { return block % 97u == 0u ? 0.5f + static_cast<float> (block % 1000u) * 1.0e-4f : 0.1f; };
    std::thread reader ([&] {
        std::uint64_t spin = 0;
        while (! done.load (std::memory_order_acquire))
        {
            const auto reading = source.read();
            if (reading.fresh)
                readings.push_back (reading);
            for (std::uint64_t i = 0; i < (++spin % 7u) * 13u; ++i)   // uneven pacing
                std::atomic_signal_fence (std::memory_order_seq_cst);
        }
    });
    for (std::uint64_t block = 1; block <= kBlocks; ++block)
        source.publishBlock (peakOf (block));
    done.store (true, std::memory_order_release);
    reader.join();
    if (const auto last = source.read(); last.fresh)
        readings.push_back (last);

    REQUIRE_FALSE (readings.empty());
    REQUIRE (readings.back().throughBlock == kBlocks);
    std::size_t at = 0;
    for (std::uint64_t block = 97; block <= kBlocks; block += 97)
    {
        while (at < readings.size() && readings[at].throughBlock < block)
            ++at;
        REQUIRE (at < readings.size());
        INFO ("block " << block << " read through " << readings[at].throughBlock);
        REQUIRE (readings[at].peak >= peakOf (block));
    }
}
