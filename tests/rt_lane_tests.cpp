// YES DAW — headless checks for the RT-lane shared-memory ring (ADR-0015): the lock-free,
// double-buffered audio + Event ring that implements the one-Block plugin-hosting handshake.
//
// Pure C++ + Catch2, no JUCE, so this runs on the normal matrix AND the RTSan leg (exchangeBlock under
// -fsanitize=realtime proves the audio-thread role never allocates/locks/syscalls) AND the TSan leg
// (the free-running producer/consumer stress test proves the release/acquire protocol is race-free).
//
// This is a HEADLESS, in-process primitive: the same atomic protocol that will later live in OS shared
// memory, exercised here with the "child" simulated by a second thread. No real cross-process mmap,
// PluginNode, scanner, watchdog, or JUCE yet — those are later chunks (STATUS.md "Next").

#include "engine/plugin/RtLaneRing.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <span>
#include <thread>
#include <vector>

using Catch::Approx;
using yesdaw::engine::Event;
using yesdaw::engine::makeParameterChangeEvent;
using yesdaw::engine::RtLaneConfig;
using yesdaw::engine::RtLaneExchangeResult;
using yesdaw::engine::RtLaneOutput;
using yesdaw::engine::RtLaneRing;

namespace {

// A single-channel block whose samples are a deterministic function of the block index, so any delivered
// output can be checked against the exact input block it must have come from.
float sampleFor (std::uint64_t blockIndex, int frame) noexcept
{
    return static_cast<float> (blockIndex) * 1000.0f + static_cast<float> (frame);
}

// The child "plugin": identity passthrough (out = in). Ignores events.
auto identityProcess = [] (std::span<const Event>,
                           const float* const* in, float* const* out, int channels, int numFrames) noexcept
{
    for (int c = 0; c < channels; ++c)
        for (int f = 0; f < numFrames; ++f)
            out[c][f] = in[c][f];
};

} // namespace

TEST_CASE ("the RT lane delivers identity output one Block late", "[rtlane][pipeline]")
{
    RtLaneConfig cfg;
    cfg.channels     = 1;
    cfg.maxBlockSize = 64;

    RtLaneRing ring;
    ring.prepare (cfg);

    constexpr int numFrames = 64;
    std::vector<float> in (numFrames, 0.0f);
    std::vector<float> out (numFrames, -1.0f);
    float* inCh[1]  = { in.data() };
    float* outCh[1] = { out.data() };

    constexpr std::uint64_t kBlocks = 16;
    for (std::uint64_t n = 0; n < kBlocks; ++n)
    {
        for (int f = 0; f < numFrames; ++f)
            in[f] = sampleFor (n, f);

        const RtLaneExchangeResult r =
            ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);

        REQUIRE (r.inputBlockIndex == n);

        if (n == 0)
        {
            // Nothing has been produced yet: the one-Block pipeline primes with silence.
            REQUIRE (r.source == RtLaneOutput::Silence);
            for (int f = 0; f < numFrames; ++f)
                REQUIRE (out[f] == 0.0f);
        }
        else
        {
            // Block N delivers the child's processing of input Block N-1 (one-Block delay).
            REQUIRE (r.source == RtLaneOutput::Fresh);
            REQUIRE (r.outputBlockIndex == n - 1);
            for (int f = 0; f < numFrames; ++f)
                REQUIRE (out[f] == Approx (sampleFor (n - 1, f)));
        }

        // The child processes the freshly-published input, off the audio thread.
        const bool produced = ring.pollOnce (identityProcess);
        REQUIRE (produced);
    }
}

TEST_CASE ("the reader fails open when the child is late: last-good -> silence -> bypass", "[rtlane][failopen]")
{
    RtLaneConfig cfg;
    cfg.channels           = 1;
    cfg.maxBlockSize       = 8;
    cfg.lastGoodHoldBlocks = 2;   // 2 misses still re-serve last-good, then silence...
    cfg.bypassAfterMisses  = 5;   // ...and 5 consecutive misses latch bypass

    RtLaneRing ring;
    ring.prepare (cfg);

    constexpr int numFrames = 8;
    std::vector<float> in (numFrames, 0.0f);
    std::vector<float> out (numFrames, -1.0f);
    float* inCh[1]  = { in.data() };
    float* outCh[1] = { out.data() };

    auto exchange = [&] (std::uint64_t n) {
        for (int f = 0; f < numFrames; ++f)
            in[f] = sampleFor (n, f);
        return ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);
    };

    // Prime: run the child in lockstep for a few Blocks so a valid last-good exists. The child stays one
    // Block ahead (it processed Block 2 during Block 2's poll), so Block 3 is still Fresh and delivers
    // the child's processing of input Block 2 — which becomes last-good once the child stops.
    for (std::uint64_t n = 0; n <= 2; ++n)
    {
        exchange (n);
        REQUIRE (ring.pollOnce (identityProcess));
    }
    RtLaneExchangeResult r3 = exchange (3);   // no poll from here on: the child has gone silent
    REQUIRE (r3.source == RtLaneOutput::Fresh);
    REQUIRE (r3.outputBlockIndex == 2);
    for (int f = 0; f < numFrames; ++f)
        REQUIRE (out[f] == Approx (sampleFor (2, f)));   // last-good is now input Block 2

    // Miss 1 and 2: re-serve last-good (the child's last output, == input Block 2).
    for (int miss = 1; miss <= 2; ++miss)
    {
        RtLaneExchangeResult r = exchange (3 + static_cast<std::uint64_t> (miss));
        REQUIRE (r.source == RtLaneOutput::LastGood);
        for (int f = 0; f < numFrames; ++f)
            REQUIRE (out[f] == Approx (sampleFor (2, f)));
    }

    // Miss 3 and 4: last-good has been held long enough -> silence.
    for (int miss = 3; miss <= 4; ++miss)
    {
        RtLaneExchangeResult r = exchange (3 + static_cast<std::uint64_t> (miss));
        REQUIRE (r.source == RtLaneOutput::Silence);
        for (int f = 0; f < numFrames; ++f)
            REQUIRE (out[f] == 0.0f);
    }

    // Miss 5: bypass latches (the coordinator's cue to swap in a placeholder, a later chunk).
    RtLaneExchangeResult r5 = exchange (9);
    REQUIRE (r5.source == RtLaneOutput::Bypass);
    REQUIRE (ring.bypassActive());
    for (int f = 0; f < numFrames; ++f)
        REQUIRE (out[f] == 0.0f);

    // Recovery: the child comes back, drains the backlog, and the audio thread sees Fresh again.
    while (ring.pollOnce (identityProcess)) { }   // process whatever inputs piled up (off the audio thread)
    RtLaneExchangeResult rr = exchange (10);
    REQUIRE (rr.source == RtLaneOutput::Fresh);
    REQUIRE_FALSE (ring.bypassActive());
}

TEST_CASE ("the control words expose validated latency and the last delivered status", "[rtlane][control]")
{
    RtLaneConfig cfg;
    cfg.channels     = 1;
    cfg.maxBlockSize = 8;

    RtLaneRing ring;
    ring.prepare (cfg);

    // Validated latency is a control word written on the control thread (e.g. plugin-reported, validated
    // by the coordinator) and read by the audio/PDC side.
    REQUIRE (ring.validatedLatencySamples() == 0);
    ring.setValidatedLatencySamples (256);
    REQUIRE (ring.validatedLatencySamples() == 256);

    constexpr int numFrames = 8;
    std::vector<float> in (numFrames, 1.0f);
    std::vector<float> out (numFrames, 0.0f);
    float* inCh[1]  = { in.data() };
    float* outCh[1] = { out.data() };

    // The status control word mirrors the source the audio thread last received.
    RtLaneExchangeResult r0 = ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);
    REQUIRE (r0.source == RtLaneOutput::Silence);
    REQUIRE (ring.lastStatus() == RtLaneOutput::Silence);

    REQUIRE (ring.pollOnce (identityProcess));
    RtLaneExchangeResult r1 = ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);
    REQUIRE (r1.source == RtLaneOutput::Fresh);
    REQUIRE (ring.lastStatus() == RtLaneOutput::Fresh);
}

TEST_CASE ("the Event ring carries events sample-accurately, one Block late", "[rtlane][events]")
{
    RtLaneConfig cfg;
    cfg.channels          = 1;
    cfg.maxBlockSize      = 16;
    cfg.maxEventsPerBlock = 8;

    RtLaneRing ring;
    ring.prepare (cfg);

    constexpr int numFrames = 16;

    // The child applies a ParameterChange's value from its timeInBlock offset onward — so a wrong offset
    // or a dropped/corrupted event would change the output and fail the assertion below.
    auto eventGainProcess = [] (std::span<const Event> events,
                                const float* const* in, float* const* out, int channels, int nf) noexcept
    {
        std::uint32_t offset = 0;
        double        gain   = 1.0;
        if (! events.empty())
        {
            offset = events.back().timeInBlock;
            gain   = events.back().payload.parameter.normalizedValue;
        }
        for (int c = 0; c < channels; ++c)
            for (int f = 0; f < nf; ++f)
                out[c][f] = (static_cast<std::uint32_t> (f) >= offset)
                              ? in[c][f] * static_cast<float> (gain)
                              : in[c][f];
    };

    std::vector<float> in (numFrames, 0.0f);
    std::vector<float> out (numFrames, -1.0f);
    float* inCh[1]  = { in.data() };
    float* outCh[1] = { out.data() };

    auto offsetFor = [] (std::uint64_t n) { return static_cast<std::uint32_t> (n % numFrames); };
    auto valueFor  = [] (std::uint64_t n) { return 0.1 + 0.2 * static_cast<double> (n % 4); };  // in [0.1, 0.7]

    constexpr std::uint64_t kBlocks = 12;
    for (std::uint64_t n = 0; n < kBlocks; ++n)
    {
        for (int f = 0; f < numFrames; ++f)
            in[f] = sampleFor (n, f);

        const Event ev = makeParameterChangeEvent (offsetFor (n), /*targetNode*/ 7, /*paramId*/ 3, valueFor (n));
        std::array<Event, 1> evs { ev };

        const RtLaneExchangeResult r = ring.exchangeBlock (inCh, 1, numFrames, evs, outCh, 1);

        if (n >= 1)
        {
            REQUIRE (r.source == RtLaneOutput::Fresh);
            REQUIRE (r.outputBlockIndex == n - 1);
            REQUIRE (r.outputEventCount == 1);   // the child reported applying exactly one event for that Block

            const std::uint32_t off = offsetFor (n - 1);
            const float         g   = static_cast<float> (valueFor (n - 1));
            for (int f = 0; f < numFrames; ++f)
            {
                const float expected = (static_cast<std::uint32_t> (f) >= off)
                                         ? sampleFor (n - 1, f) * g
                                         : sampleFor (n - 1, f);
                REQUIRE (out[f] == Approx (expected));
            }
        }

        REQUIRE (ring.pollOnce (eventGainProcess));
    }
}

TEST_CASE ("the pipeline is correct across channel counts and varying Block sizes", "[rtlane][sizes]")
{
    auto runGainPipeline = [] (int channels, int numFrames, float gain)
    {
        RtLaneConfig cfg;
        cfg.channels     = channels;
        cfg.maxBlockSize = 64;
        REQUIRE (numFrames <= cfg.maxBlockSize);

        RtLaneRing ring;
        ring.prepare (cfg);

        auto gainProcess = [gain] (std::span<const Event>,
                                   const float* const* in, float* const* out, int ch, int nf) noexcept
        {
            for (int c = 0; c < ch; ++c)
                for (int f = 0; f < nf; ++f)
                    out[c][f] = in[c][f] * gain;
        };

        std::vector<std::vector<float>> inBuf (static_cast<std::size_t> (channels), std::vector<float> (numFrames, 0.0f));
        std::vector<std::vector<float>> outBuf (static_cast<std::size_t> (channels), std::vector<float> (numFrames, -1.0f));
        std::vector<float*> inCh (static_cast<std::size_t> (channels));
        std::vector<float*> outCh (static_cast<std::size_t> (channels));
        for (int c = 0; c < channels; ++c) { inCh[c] = inBuf[static_cast<std::size_t> (c)].data(); outCh[c] = outBuf[static_cast<std::size_t> (c)].data(); }

        // Each channel gets a distinct deterministic signal so a channel-stride bug can't hide.
        auto chanSample = [] (int c, std::uint64_t n, int f) {
            return static_cast<float> (c) * 1.0e6f + sampleFor (n, f);
        };

        constexpr std::uint64_t kBlocks = 10;
        for (std::uint64_t n = 0; n < kBlocks; ++n)
        {
            for (int c = 0; c < channels; ++c)
                for (int f = 0; f < numFrames; ++f)
                    inBuf[static_cast<std::size_t> (c)][static_cast<std::size_t> (f)] = chanSample (c, n, f);

            const RtLaneExchangeResult r =
                ring.exchangeBlock (inCh.data(), channels, numFrames, {}, outCh.data(), channels);

            if (n >= 1)
            {
                REQUIRE (r.source == RtLaneOutput::Fresh);
                REQUIRE (r.outputBlockIndex == n - 1);
                REQUIRE (r.outputNumFrames == static_cast<std::uint32_t> (numFrames));
                for (int c = 0; c < channels; ++c)
                    for (int f = 0; f < numFrames; ++f)
                        REQUIRE (outBuf[static_cast<std::size_t> (c)][static_cast<std::size_t> (f)]
                                 == Approx (chanSample (c, n - 1, f) * gain));
            }

            REQUIRE (ring.pollOnce (gainProcess));
        }
    };

    runGainPipeline (1, 1,  0.5f);
    runGainPipeline (1, 7,  2.0f);
    runGainPipeline (1, 64, -1.0f);
    runGainPipeline (2, 16, 0.75f);
    runGainPipeline (2, 1,  3.0f);
}

TEST_CASE ("events beyond maxEventsPerBlock are dropped to the cap (first ones kept)", "[rtlane][events][overflow]")
{
    RtLaneConfig cfg;
    cfg.channels          = 1;
    cfg.maxBlockSize      = 8;
    cfg.maxEventsPerBlock = 4;

    RtLaneRing ring;
    ring.prepare (cfg);

    constexpr int numFrames = 8;
    std::vector<float> in (numFrames, 1.0f);
    std::vector<float> out (numFrames, 0.0f);
    float* inCh[1]  = { in.data() };
    float* outCh[1] = { out.data() };

    auto eventGain = [] (std::span<const Event> evs,
                         const float* const* i, float* const* o, int ch, int nf) noexcept
    {
        const double g = evs.empty() ? 1.0 : evs.back().payload.parameter.normalizedValue;
        for (int c = 0; c < ch; ++c)
            for (int f = 0; f < nf; ++f)
                o[c][f] = i[c][f] * static_cast<float> (g);
    };

    // Submit 7 events (cap is 4). The impl keeps the FIRST 4, so the surviving "last" is events[3] (0.4).
    std::array<Event, 7> evs{};
    for (std::uint32_t k = 0; k < 7; ++k)
        evs[k] = makeParameterChangeEvent (0, 1, 1, 0.1 * static_cast<double> (k + 1));

    ring.exchangeBlock (inCh, 1, numFrames, evs, outCh, 1);
    REQUIRE (ring.pollOnce (eventGain));
    const RtLaneExchangeResult r = ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);

    REQUIRE (r.source == RtLaneOutput::Fresh);
    REQUIRE (r.outputEventCount == 4);                       // clamped to maxEventsPerBlock, no overrun
    for (int f = 0; f < numFrames; ++f)
        REQUIRE (out[f] == Approx (0.4));                    // events[3] (the 4th) is the surviving last
}

TEST_CASE ("numFrames beyond maxBlockSize is clamped, not overrun", "[rtlane][sizes][clamp]")
{
    RtLaneConfig cfg;
    cfg.channels     = 1;
    cfg.maxBlockSize = 8;

    RtLaneRing ring;
    ring.prepare (cfg);

    constexpr int over = 20;                                 // caller asks for more than maxBlockSize
    std::vector<float> in (over, 0.0f);
    std::vector<float> out (over, -1.0f);
    float* inCh[1]  = { in.data() };
    float* outCh[1] = { out.data() };

    for (int f = 0; f < over; ++f) in[f] = sampleFor (0, f);
    ring.exchangeBlock (inCh, 1, over, {}, outCh, 1);        // internally clamped to 8
    REQUIRE (ring.pollOnce (identityProcess));

    for (int f = 0; f < over; ++f) in[f] = sampleFor (1, f);
    const RtLaneExchangeResult r = ring.exchangeBlock (inCh, 1, over, {}, outCh, 1);

    REQUIRE (r.source == RtLaneOutput::Fresh);
    REQUIRE (r.outputNumFrames <= 8);                        // delivered frame count never exceeds maxBlockSize
    for (int f = 0; f < 8; ++f)
        REQUIRE (out[f] == Approx (sampleFor (0, f)));       // the clamped frames are Block 0's data
}

TEST_CASE ("reset() re-primes the ring for reuse", "[rtlane][reset]")
{
    RtLaneConfig cfg;
    cfg.channels           = 1;
    cfg.maxBlockSize       = 8;
    cfg.lastGoodHoldBlocks = 1;
    cfg.bypassAfterMisses  = 2;

    RtLaneRing ring;
    ring.prepare (cfg);

    constexpr int numFrames = 8;
    std::vector<float> in (numFrames, 1.0f);
    std::vector<float> out (numFrames, 0.0f);
    float* inCh[1]  = { in.data() };
    float* outCh[1] = { out.data() };

    // Drive the ring into latched bypass, then leave seq counters non-zero.
    ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);
    REQUIRE (ring.pollOnce (identityProcess));
    ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);   // Fresh
    ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);   // miss 1
    ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);   // miss 2 -> bypass
    REQUIRE (ring.bypassActive());
    REQUIRE (ring.inputSeq() != 0);

    ring.reset();
    REQUIRE_FALSE (ring.bypassActive());
    REQUIRE (ring.inputSeq() == 0);
    REQUIRE (ring.outputSeq() == 0);
    REQUIRE (ring.lastStatus() == RtLaneOutput::Silence);

    // A fresh run after reset behaves exactly like a new ring.
    const RtLaneExchangeResult r0 = ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);
    REQUIRE (r0.source == RtLaneOutput::Silence);
    REQUIRE (ring.pollOnce (identityProcess));
    const RtLaneExchangeResult r1 = ring.exchangeBlock (inCh, 1, numFrames, {}, outCh, 1);
    REQUIRE (r1.source == RtLaneOutput::Fresh);
    REQUIRE (r1.outputBlockIndex == 0);
}

TEST_CASE ("numFrames varying block-to-block within one run delivers + zero-fills correctly", "[rtlane][sizes][varying]")
{
    RtLaneConfig cfg;
    cfg.channels     = 1;
    cfg.maxBlockSize = 64;

    RtLaneRing ring;
    ring.prepare (cfg);

    const std::array<int, 7> frames { 64, 16, 48, 8, 32, 60, 1 };
    std::vector<float> in (64, 0.0f);
    std::vector<float> out (64, 0.0f);
    float* inCh[1]  = { in.data() };
    float* outCh[1] = { out.data() };

    for (std::size_t n = 0; n < frames.size(); ++n)
    {
        const int F = frames[n];
        for (int f = 0; f < F; ++f) in[f] = sampleFor (static_cast<std::uint64_t> (n), f);

        const RtLaneExchangeResult r = ring.exchangeBlock (inCh, 1, F, {}, outCh, 1);

        if (n >= 1)
        {
            const int prevF = frames[n - 1];
            const int wn    = std::min (prevF, F);   // delivered output = min(producing-block, current-block)
            REQUIRE (r.source == RtLaneOutput::Fresh);
            REQUIRE (r.outputBlockIndex == static_cast<std::uint64_t> (n) - 1);
            REQUIRE (r.outputNumFrames == static_cast<std::uint32_t> (wn));
            for (int f = 0; f < wn; ++f)
                REQUIRE (out[f] == Approx (sampleFor (static_cast<std::uint64_t> (n) - 1, f)));  // overlap delivered
            for (int f = wn; f < F; ++f)
                REQUIRE (out[f] == 0.0f);                                                        // tail zero-filled
        }

        REQUIRE (ring.pollOnce (identityProcess));
    }
}

namespace {

struct StressResult
{
    std::uint64_t freshCount      = 0;
    std::uint64_t missCount       = 0;
    bool          allFreshCorrect = true;   // every Fresh delivery == its source Block's processing
    bool          allFreshOneLate = true;   // every Fresh delivery is EXACTLY Block N-1 (deterministic)
};

constexpr std::uint64_t kStressBlocks = 3000;
// The final Blocks are ALWAYS paced (the audio thread waits for the child), so even the flat-out run is
// guaranteed to land some CONCURRENT Fresh deliveries through the output seqlock — which keeps its
// allFreshCorrect / allFreshOneLate assertions from passing vacuously on a zero-Fresh run.
constexpr std::uint64_t kStressCoda = 128;

// Run the audio role (this thread) and the child role (a worker thread) concurrently. `paced` models the
// audio device's one-Block-per-period cadence by letting the child publish each Block before the next
// exchange; unpaced lets the audio thread outrun the child, which is what drives same-slot lapping reads
// (the case the relaxed-atomic seqlock must keep race-free — the whole point of the TSan leg).
StressResult runStress (bool paced)
{
    constexpr int   channels  = 2;
    constexpr int   numFrames = 48;
    constexpr float gain      = 0.5f;

    RtLaneConfig cfg;
    cfg.channels     = channels;
    cfg.maxBlockSize = numFrames;

    RtLaneRing ring;
    ring.prepare (cfg);

    auto chanSample = [] (int c, std::uint64_t n, int f) {
        return static_cast<float> (c) * 1.0e6f + static_cast<float> (n) * 1000.0f + static_cast<float> (f);
    };
    auto gainProcess = [] (std::span<const Event>,
                           const float* const* in, float* const* out, int ch, int nf) noexcept
    {
        for (int c = 0; c < ch; ++c)
            for (int f = 0; f < nf; ++f)
                out[c][f] = in[c][f] * gain;
    };

    std::atomic<bool> stop { false };
    std::thread child ([&]
    {
        while (! stop.load (std::memory_order_acquire))
            (void) ring.pollOnce (gainProcess);
        while (ring.pollOnce (gainProcess)) { }     // final drain off the audio thread
    });

    std::vector<std::vector<float>> inBuf (channels, std::vector<float> (numFrames, 0.0f));
    std::vector<std::vector<float>> outBuf (channels, std::vector<float> (numFrames, 0.0f));
    std::vector<float*> inCh (channels), outCh (channels);
    for (int c = 0; c < channels; ++c)
    {
        inCh[c]  = inBuf[static_cast<std::size_t> (c)].data();
        outCh[c] = outBuf[static_cast<std::size_t> (c)].data();
    }

    StressResult result;
    constexpr int kSpinCap = 2'000'000;   // bound the pace wait so a starved child can't hang us

    for (std::uint64_t n = 0; n < kStressBlocks; ++n)
    {
        for (int c = 0; c < channels; ++c)
            for (int f = 0; f < numFrames; ++f)
                inBuf[static_cast<std::size_t> (c)][static_cast<std::size_t> (f)] = chanSample (c, n, f);

        const RtLaneExchangeResult r =
            ring.exchangeBlock (inCh.data(), channels, numFrames, {}, outCh.data(), channels);

        if (r.source == RtLaneOutput::Fresh)
        {
            ++result.freshCount;
            if (r.outputBlockIndex != r.inputBlockIndex - 1)
                result.allFreshOneLate = false;
            for (int c = 0; c < channels; ++c)
                for (int f = 0; f < numFrames; ++f)
                    if (outBuf[static_cast<std::size_t> (c)][static_cast<std::size_t> (f)]
                        != Approx (chanSample (c, r.outputBlockIndex, f) * gain))
                        result.allFreshCorrect = false;
        }
        else
        {
            ++result.missCount;
        }

        // Pace this Block if requested, OR if we are in the always-paced coda (so flat-out still lands
        // concurrent Fresh deliveries — the child is live, the audio thread just waits for Block n).
        if (paced || n >= kStressBlocks - kStressCoda)
            for (int s = 0; ring.outputSeq() <= n && s < kSpinCap; ++s)
                std::this_thread::yield();
    }

    stop.store (true, std::memory_order_release);
    child.join();
    return result;
}

} // namespace

TEST_CASE ("stress: the audio thread and child thread exchange Blocks race-free", "[rtlane][stress]")
{
    // Catch2's REQUIRE is not thread-safe, so ALL assertions stay on this (main) thread; the worker only
    // calls pollOnce (no assertions there) — mirroring tests/runtime_tests.cpp's stress structure.

    SECTION ("flat-out: lapping reads stay race-free and never deliver garbage")
    {
        // The audio thread outruns the child, so it repeatedly reads slots the child is mid-writing. The
        // seqlock + relaxed-atomic payload must make every such read either a correct Fresh Block or a
        // miss — never torn garbage. This is the case the TSan leg verifies has no data race. The paced
        // coda guarantees freshCount > 0, so the no-garbage assertions below are never vacuous.
        const StressResult r = runStress (/*paced*/ false);
        REQUIRE (r.allFreshCorrect);
        REQUIRE (r.allFreshOneLate);
        REQUIRE (r.freshCount > 0);                       // at least the coda delivered Fresh concurrently
        REQUIRE (r.freshCount + r.missCount == kStressBlocks);
    }

    SECTION ("paced: a child that keeps up delivers every Block exactly one Block late")
    {
        const StressResult r = runStress (/*paced*/ true);
        REQUIRE (r.allFreshCorrect);
        REQUIRE (r.allFreshOneLate);
        REQUIRE (r.freshCount > 1000);                    // sustained correct delivery (kStressBlocks == 3000)
        REQUIRE (r.freshCount + r.missCount == kStressBlocks);
    }
}
