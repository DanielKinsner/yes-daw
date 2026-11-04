// YES DAW - headless checks for ADR-0010 time model value types.
//
// This locks the storage-facing type surface before the Project round-trip code starts using it.

#include "engine/Node.h"
#include "engine/Time.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

using yesdaw::engine::MeterChange;
using yesdaw::engine::MeterMapView;
using yesdaw::engine::MusicalTime;
using yesdaw::engine::ResampleQuality;
using yesdaw::engine::SampleRate;
using yesdaw::engine::SnapGrid;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::TempoMapView;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Transport;
using yesdaw::engine::CompiledTempoMap;
using yesdaw::engine::gridIndexForTick;
using yesdaw::engine::kTicksPerQuarter;
using yesdaw::engine::snapTick;
using yesdaw::engine::tickForGridIndex;
using yesdaw::engine::tickToFrame;
using yesdaw::engine::BarBeat;               // G2.15
using yesdaw::engine::computeBarBeat;
using yesdaw::engine::computeBarBeatPiecewise;
using yesdaw::engine::barBeatAtTick;        // UI correctness pass unit 6
using yesdaw::engine::barStartTick;
using yesdaw::engine::nominalBarTicks;

static_assert (std::is_same_v<Tick, std::int64_t>);
static_assert (kTicksPerQuarter == 15360);
static_assert (std::is_trivially_copyable_v<MusicalTime>);
static_assert (std::is_trivially_copyable_v<SnapGrid>);
static_assert (std::is_trivially_copyable_v<TempoChange>);
static_assert (std::is_trivially_copyable_v<MeterChange>);
static_assert (std::is_trivially_copyable_v<SampleRate>);
static_assert (std::is_trivially_copyable_v<Transport>);

TEST_CASE ("ADR-0010 locks PPQ and storage-facing enum values", "[time][adr0010]")
{
    REQUIRE (kTicksPerQuarter == 15360);
    REQUIRE (static_cast<std::uint8_t> (TimeBase::TempoLocked) == 0u);
    REQUIRE (static_cast<std::uint8_t> (TimeBase::SampleLocked) == 1u);
    REQUIRE (static_cast<std::uint8_t> (TempoCurve::Jump) == 0u);
    REQUIRE (static_cast<std::uint8_t> (TempoCurve::LinearRamp) == 1u);
    REQUIRE (static_cast<std::uint8_t> (ResampleQuality::LivePlayback) == 0u);
    REQUIRE (static_cast<std::uint8_t> (ResampleQuality::OfflineRender) == 1u);
}

TEST_CASE ("MusicalTime carries exact ticks plus render-only fractional ticks", "[time][musical]")
{
    const MusicalTime downbeat { kTicksPerQuarter * 4, 0.0 };
    const MusicalTime betweenTicks { kTicksPerQuarter * 4, 0.75 };
    const MusicalTime negativeFrac { 0, -0.01 };
    const MusicalTime oneWholeFrac { 0, 1.0 };

    REQUIRE (downbeat.hasValidFraction());
    REQUIRE (betweenTicks.hasValidFraction());
    REQUIRE_FALSE (negativeFrac.hasValidFraction());
    REQUIRE_FALSE (oneWholeFrac.hasValidFraction());
    REQUIRE (downbeat.tick == 61440);
}

TEST_CASE ("SnapGrid snaps ticks with deterministic integer nearest-grid behavior", "[time][snap]")
{
    const SnapGrid sixteenth { kTicksPerQuarter / 4 };
    Tick snapped = -1;

    REQUIRE (sixteenth.isValid());
    REQUIRE (snapTick (0, sixteenth, snapped));
    REQUIRE (snapped == 0);

    REQUIRE (snapTick (sixteenth.intervalTicks / 2 - 1, sixteenth, snapped));
    REQUIRE (snapped == 0);

    REQUIRE (snapTick (sixteenth.intervalTicks / 2, sixteenth, snapped));
    REQUIRE (snapped == sixteenth.intervalTicks);

    REQUIRE (snapTick (sixteenth.intervalTicks / 2 + 1, sixteenth, snapped));
    REQUIRE (snapped == sixteenth.intervalTicks);

    REQUIRE (snapTick (kTicksPerQuarter + 17, sixteenth, snapped));
    REQUIRE (snapped == kTicksPerQuarter);

    const Tick once = snapped;
    REQUIRE (snapTick (once, sixteenth, snapped));
    REQUIRE (snapped == once);

    REQUIRE (snapTick (-1, sixteenth, snapped));
    REQUIRE (snapped == 0);

    REQUIRE (snapTick (-sixteenth.intervalTicks / 2, sixteenth, snapped));
    REQUIRE (snapped == 0);

    REQUIRE (snapTick (-sixteenth.intervalTicks / 2 - 1, sixteenth, snapped));
    REQUIRE (snapped == -sixteenth.intervalTicks);
}

TEST_CASE ("SnapGrid round-trips snapped ticks through integer grid indexes", "[time][snap]")
{
    const SnapGrid tripletEighth { kTicksPerQuarter / 3 };

    for (const Tick index : { Tick { -8 }, Tick { -1 }, Tick { 0 }, Tick { 1 }, Tick { 37 } })
    {
        Tick tick = 0;
        Tick readbackIndex = 0;

        REQUIRE (tickForGridIndex (index, tripletEighth, tick));
        REQUIRE (gridIndexForTick (tick, tripletEighth, readbackIndex));
        REQUIRE (readbackIndex == index);

        Tick snapped = 0;
        REQUIRE (snapTick (tick, tripletEighth, snapped));
        REQUIRE (snapped == tick);
    }

    Tick index = 0;
    REQUIRE_FALSE (gridIndexForTick (tripletEighth.intervalTicks + 1, tripletEighth, index));
}

TEST_CASE ("SnapGrid rejects invalid grids and overflowing derived ticks", "[time][snap]")
{
    const SnapGrid zero { 0 };
    const SnapGrid negative { -kTicksPerQuarter };
    const SnapGrid twoTicks { 2 };
    Tick out = 123;

    REQUIRE_FALSE (zero.isValid());
    REQUIRE_FALSE (negative.isValid());

    REQUIRE_FALSE (snapTick (kTicksPerQuarter, zero, out));
    REQUIRE (out == 123);

    REQUIRE_FALSE (gridIndexForTick (kTicksPerQuarter, negative, out));
    REQUIRE (out == 123);

    REQUIRE_FALSE (tickForGridIndex (std::numeric_limits<Tick>::max(), twoTicks, out));
    REQUIRE (out == 123);

    REQUIRE (snapTick (std::numeric_limits<Tick>::max() - 1, twoTicks, out));
    REQUIRE (out == std::numeric_limits<Tick>::max() - 1);

    REQUIRE_FALSE (snapTick (std::numeric_limits<Tick>::max(), twoTicks, out));
}

TEST_CASE ("Tempo and meter changes are point records for later map conversion", "[time][maps]")
{
    const TempoChange tempoA { 0, 120.0, TempoCurve::Jump };
    const TempoChange tempoB { kTicksPerQuarter * 8, 96.0, TempoCurve::LinearRamp };
    const TempoChange invalidTempo { 0, 0.0, TempoCurve::Jump };
    const TempoChange infiniteTempo { 0, std::numeric_limits<double>::infinity(), TempoCurve::Jump };
    const TempoChange nanTempo { 0, std::numeric_limits<double>::quiet_NaN(), TempoCurve::Jump };
    const MeterChange meterA { 0, 4, 4 };
    const MeterChange meterB { kTicksPerQuarter * 16, 7, 8 };
    const MeterChange invalidMeter { 0, 0, 4 };

    REQUIRE (tempoA.hasValidBpm());
    REQUIRE (tempoB.hasValidBpm());
    REQUIRE_FALSE (invalidTempo.hasValidBpm());
    REQUIRE_FALSE (infiniteTempo.hasValidBpm());
    REQUIRE_FALSE (nanTempo.hasValidBpm());
    REQUIRE (meterA.isValid());
    REQUIRE (meterB.isValid());
    REQUIRE_FALSE (invalidMeter.isValid());
}

TEST_CASE ("SampleRate rejects non-finite or non-positive project rates", "[time][sample-rate]")
{
    REQUIRE (SampleRate { 44100.0 }.isValid());
    REQUIRE_FALSE (SampleRate { 0.0 }.isValid());
    REQUIRE_FALSE (SampleRate { -48000.0 }.isValid());
    REQUIRE_FALSE (SampleRate { std::numeric_limits<double>::infinity() }.isValid());
    REQUIRE_FALSE (SampleRate { std::numeric_limits<double>::quiet_NaN() }.isValid());
}

TEST_CASE ("Transport exposes non-owning tempo and meter map views", "[time][transport]")
{
    const TempoChange tempo[] = {
        { 0, 120.0, TempoCurve::Jump },
        { kTicksPerQuarter * 4, 140.0, TempoCurve::LinearRamp }
    };
    const MeterChange meters[] = {
        { 0, 4, 4 },
        { kTicksPerQuarter * 12, 3, 4 }
    };

    Transport transport;
    REQUIRE (transport.tempoMap.empty());
    REQUIRE (transport.meterMap.empty());
    REQUIRE (transport.projectSampleRate == SampleRate { 48000.0 });
    REQUIRE_FALSE (transport.isPlaying);

    transport.playhead = MusicalTime { kTicksPerQuarter * 2, 0.5 };
    transport.tempoMap = TempoMapView { tempo, 2 };
    transport.meterMap = MeterMapView { meters, 2 };
    transport.projectSampleRate = SampleRate { 96000.0 };
    transport.isPlaying = true;

    REQUIRE (transport.playhead == MusicalTime { kTicksPerQuarter * 2, 0.5 });
    REQUIRE (transport.tempoMap.count == 2u);
    REQUIRE (transport.tempoMap.changes[1] == tempo[1]);
    REQUIRE (transport.meterMap.count == 2u);
    REQUIRE (transport.meterMap.changes[1] == meters[1]);
    REQUIRE (transport.projectSampleRate.isValid());
    REQUIRE (transport.isPlaying);
}

TEST_CASE ("CompiledTempoMap prefix-sum frameForTick is bit-identical to tickToFrame", "[time][adr0010][tempo]")
{
    constexpr double sr = 30720.0;

    // Exercises a logarithmic ramp segment (120 -> 60), a Jump, and a trailing constant segment.
    const std::array<TempoChange, 3> changes {
        TempoChange { 0, 120.0, TempoCurve::LinearRamp },
        TempoChange { 1000, 60.0, TempoCurve::Jump },
        TempoChange { 2500, 90.0, TempoCurve::Jump },
    };
    const TempoMapView view { changes.data(), changes.size() };

    CompiledTempoMap compiled;
    REQUIRE (CompiledTempoMap::build (view, SampleRate { sr }, compiled));
    REQUIRE (compiled.segmentCount() == 3u);

    // O(log n) prefix-sum lookup must match the O(n) closed-form scan exactly, across every segment and at
    // every segment boundary (the boundaries are where a per-event scan and a prefix sum can disagree).
    for (Tick tick = 0; tick <= 4000; ++tick)
    {
        double naive = -1.0;
        double prefix = -2.0;
        const bool naiveOk = tickToFrame (view, SampleRate { sr }, tick, naive);
        const bool prefixOk = compiled.frameForTick (tick, prefix);
        REQUIRE (naiveOk == prefixOk);
        REQUIRE (naiveOk);
        REQUIRE (prefix == naive); // bit-identical, not merely approximately equal
    }

    // Empty map => default 120 BPM; the prefix-sum path must agree there too.
    CompiledTempoMap emptyCompiled;
    REQUIRE (CompiledTempoMap::build (TempoMapView {}, SampleRate { sr }, emptyCompiled));
    REQUIRE (emptyCompiled.empty());
    for (Tick tick = 0; tick <= 5000; tick += 137)
    {
        double naive = 0.0;
        double prefix = 0.0;
        REQUIRE (tickToFrame (TempoMapView {}, SampleRate { sr }, tick, naive));
        REQUIRE (emptyCompiled.frameForTick (tick, prefix));
        REQUIRE (prefix == naive);
    }

    // Negative ticks rejected the same way both paths do.
    double scratch = 1.0;
    REQUIRE_FALSE (compiled.frameForTick (-1, scratch));
    REQUIRE_FALSE (tickToFrame (view, SampleRate { sr }, -1, scratch));

    // Invalid maps fail to build, mirroring tickToFrame returning false (first change not at tick 0).
    const std::array<TempoChange, 2> nonZeroStart {
        TempoChange { 5, 120.0, TempoCurve::Jump },
        TempoChange { 1000, 60.0, TempoCurve::Jump },
    };
    CompiledTempoMap invalid;
    REQUIRE_FALSE (CompiledTempoMap::build (TempoMapView { nonZeroStart.data(), nonZeroStart.size() }, SampleRate { sr }, invalid));
    double naiveInvalid = 0.0;
    REQUIRE_FALSE (tickToFrame (TempoMapView { nonZeroStart.data(), nonZeroStart.size() }, SampleRate { sr }, 100, naiveInvalid));
}

TEST_CASE ("Long shallow linear Tempo ramps match independent analytic clocks", "[time][tempo][adr0082][ramp-kernel]")
{
    constexpr Tick length = Tick { 1 } << 48;
    constexpr double k = 187.5;   // 60 * 48000 / 15360, frames per Tick times BPM
    for (const double endBpm : { 128.0, 32.0 })
    {
        const std::array<TempoChange, 2> changes {
            TempoChange { 0, 64.0, TempoCurve::LinearRamp },
            TempoChange { length, endBpm, TempoCurve::Jump },
        };
        const TempoMapView view { changes.data(), changes.size() };
        CompiledTempoMap compiled;
        REQUIRE (CompiledTempoMap::build (view, SampleRate { 48000.0 }, compiled));
        for (const Tick quarter : { Tick { 0 }, Tick { 1 }, Tick { 2 }, Tick { 3 }, Tick { 4 } })
        {
            const double p = static_cast<double> (quarter) / 4.0;
            const Tick tick = quarter * (length / 4);
            // These clocks come directly from the specified 64->128 / 64->32 BPM laws,
            // independently of either conversion path and its computed slope or prefix.
            const double expected = endBpm == 128.0
                ? (k * static_cast<double> (length) / 64.0) * std::log (1.0 + p)
                : -(k * static_cast<double> (length) / 32.0) * std::log (1.0 - p / 2.0);
            const double tolerance = 8.0 * (std::nextafter (expected, std::numeric_limits<double>::infinity()) - expected);
            CAPTURE (endBpm, p, tick, expected, tolerance);
            double freeFrame = -1.0;
            double compiledFrame = -1.0;
            REQUIRE (tickToFrame (view, SampleRate { 48000.0 }, tick, freeFrame));
            REQUIRE (compiled.frameForTick (tick, compiledFrame));
            CHECK (std::abs (freeFrame - expected) <= tolerance);
            CHECK (std::abs (compiledFrame - expected) <= tolerance);
            Tick inverseTick = -1;
            REQUIRE (compiled.tickForFrame (expected, inverseTick));
            CHECK (std::llabs (inverseTick - tick) <= 2);
        }
    }
}

TEST_CASE ("Linear Tempo ramps retain a one-Tick change over very long segments", "[time][tempo][adr0082][ramp-kernel]")
{
    constexpr Tick length = Tick { 1 } << 60;
    constexpr double expected = 187.5 / 64.0;
    const double tolerance = 8.0 * (std::nextafter (expected, std::numeric_limits<double>::infinity()) - expected);
    for (const double endBpm : { 128.0, 32.0 })
    {
        const std::array<TempoChange, 2> changes {
            TempoChange { 0, 64.0, TempoCurve::LinearRamp },
            TempoChange { length, endBpm, TempoCurve::Jump },
        };
        const TempoMapView view { changes.data(), changes.size() };
        CompiledTempoMap compiled;
        REQUIRE (CompiledTempoMap::build (view, SampleRate { 48000.0 }, compiled));
        CAPTURE (endBpm);
        double freeFrame = -1.0;
        double compiledFrame = -1.0;
        REQUIRE (tickToFrame (view, SampleRate { 48000.0 }, 1, freeFrame));
        REQUIRE (compiled.frameForTick (1, compiledFrame));
        CHECK (std::abs (freeFrame - expected) <= tolerance);
        CHECK (std::abs (compiledFrame - expected) <= tolerance);
        Tick inverseTick = -1;
        REQUIRE (compiled.tickForFrame (expected, inverseTick));
        CHECK (inverseTick == 1);
    }
}

TEST_CASE ("Unequal Tempo ramps refuse an unrepresentable slope without rejecting stored BPM", "[time][tempo][adr0082][ramp-kernel]")
{
    constexpr Tick length = Tick { 1 } << 48;
    constexpr double tinyBpm = 1.0e-300;
    const double nextBpm = std::nextafter (tinyBpm, std::numeric_limits<double>::infinity());
    const TempoChange tiny { 0, tinyBpm, TempoCurve::LinearRamp };
    const TempoChange next { length, nextBpm, TempoCurve::Jump };
    REQUIRE (tiny.hasValidBpm());
    REQUIRE (next.hasValidBpm());
    REQUIRE (nextBpm > tinyBpm);
    REQUIRE ((nextBpm - tinyBpm) / static_cast<double> (length) == 0.0);

    const std::array<TempoChange, 2> equal { tiny, TempoChange { length, tinyBpm, TempoCurve::Jump } };
    double constantFrame = -1.0;
    REQUIRE (tickToFrame (TempoMapView { equal.data(), equal.size() }, SampleRate { 48000.0 }, 1, constantFrame));
    REQUIRE (std::isfinite (constantFrame));
    REQUIRE (constantFrame == 187.5 / tinyBpm);

    // Only the partial free lookup is requested: the full segment's frame count overflows.
    const std::array<TempoChange, 2> unequal { tiny, next };
    double frame = -1.0;
    CHECK_FALSE (tickToFrame (TempoMapView { unequal.data(), unequal.size() }, SampleRate { 48000.0 }, 1, frame));
}

TEST_CASE ("Linear Tempo inverse retains a nonzero exponent when its numerator underflows", "[time][tempo][adr0082][ramp-kernel][ramp-scaling]")
{
    constexpr Tick length = Tick { 1 } << 48;
    const SampleRate sampleRate { std::ldexp (1.0, -1000) };
    const double startBpm = std::ldexp (1.0, -700);
    const double endBpm = std::nextafter (startBpm, std::numeric_limits<double>::infinity());
    const std::array<TempoChange, 2> changes {
        TempoChange { 0, startBpm, TempoCurve::LinearRamp },
        TempoChange { length, endBpm, TempoCurve::Jump },
    };
    REQUIRE (sampleRate.isValid());
    REQUIRE (changes[0].hasValidBpm());
    REQUIRE (changes[1].hasValidBpm());
    const TempoMapView view { changes.data(), changes.size() };
    CompiledTempoMap compiled;
    REQUIRE (CompiledTempoMap::build (view, sampleRate, compiled));

    // k = 2^-1008 and the initial BPM is 2^-700. The ramp's relative change
    // during Tick 1 is 2^-100, so its analytic frame rounds exactly to 2^-308.
    const double expectedFrame = std::ldexp (1.0, -308);
    double freeFrame = -1.0;
    double compiledFrame = -1.0;
    REQUIRE (tickToFrame (view, sampleRate, 1, freeFrame));
    REQUIRE (compiled.frameForTick (1, compiledFrame));
    CHECK (freeFrame == expectedFrame);
    CHECK (compiledFrame == expectedFrame);
    Tick tick = -1;
    REQUIRE (compiled.tickForFrame (expectedFrame, tick));
    CHECK (tick == 1);
}

TEST_CASE ("Linear Tempo inverse retains a finite exponent when division first would overflow", "[time][tempo][adr0082][ramp-kernel][ramp-scaling]")
{
    const SampleRate sampleRate { std::ldexp (1.0, -1000) };
    const double startBpm = std::ldexp (1.0, -1050);
    const std::array<TempoChange, 2> changes {
        TempoChange { 0, startBpm, TempoCurve::LinearRamp },
        TempoChange { 1, std::nextafter (startBpm, std::numeric_limits<double>::infinity()), TempoCurve::Jump },
    };
    REQUIRE (sampleRate.isValid());
    REQUIRE (changes[0].hasValidBpm());
    REQUIRE (changes[1].hasValidBpm());
    CompiledTempoMap compiled;
    REQUIRE (CompiledTempoMap::build (TempoMapView { changes.data(), changes.size() }, sampleRate, compiled));

    // At frame 2^41 the exponent is 2^-25: delta = 2^24 * expm1(2^-25),
    // strictly above 1/2 and below 1 Tick. Dividing this frame by k first overflows.
    Tick tick = -1;
    REQUIRE (compiled.tickForFrame (std::ldexp (1.0, 41), tick));
    CHECK (tick == 1);
}

TEST_CASE ("Linear Tempo inverse refuses a nonzero exponent that cannot be represented", "[time][tempo][adr0082][ramp-kernel][ramp-scaling]")
{
    constexpr Tick length = Tick { 1 } << 48;
    const std::array<TempoChange, 2> changes {
        TempoChange { 0, 64.0, TempoCurve::LinearRamp },
        TempoChange { length, std::nextafter (64.0, std::numeric_limits<double>::infinity()), TempoCurve::Jump },
    };
    CompiledTempoMap compiled;
    REQUIRE (CompiledTempoMap::build (TempoMapView { changes.data(), changes.size() }, SampleRate { 48000.0 }, compiled));
    Tick tick = -1;
    REQUIRE (compiled.tickForFrame (0.0, tick));
    CHECK (tick == 0);
    CHECK_FALSE (compiled.tickForFrame (std::numeric_limits<double>::denorm_min(), tick));
}

// G2.15: the inverse and the piecewise readout — tickForFrame undoes frameForTick within a tick across
// a ramp, a jump and a constant tail, monotonically; bar|beat under a tempo AND a meter change equals
// the closed form written out by hand.
TEST_CASE ("CompiledTempoMap tickForFrame inverts frameForTick and computeBarBeatPiecewise matches the closed form", "[time][g2][tempo][inverse]")
{
    constexpr double sr = 48000.0;
    const std::array<TempoChange, 3> changes {
        TempoChange { 0, 120.0, TempoCurve::LinearRamp },
        TempoChange { 3840, 60.0, TempoCurve::Jump },   // two quarters of ramp 120 -> 60
        TempoChange { 7680, 90.0, TempoCurve::Jump },
    };
    const TempoMapView view { changes.data(), changes.size() };
    CompiledTempoMap compiled;
    REQUIRE (CompiledTempoMap::build (view, SampleRate { sr }, compiled));
    Tick previous = -1;
    for (Tick tick = 0; tick <= 12000; tick += 7)
    {
        double frame = 0.0;
        REQUIRE (compiled.frameForTick (tick, frame));
        Tick back = -1;
        REQUIRE (compiled.tickForFrame (frame, back));
        REQUIRE (std::llabs (back - tick) <= 1);
        REQUIRE (back >= previous);
        previous = back;
    }
    // The closed form at the segment starts: frames 0 and the prefix sums land on their ticks exactly.
    for (const TempoChange& change : changes)
    {
        double frame = 0.0;
        REQUIRE (compiled.frameForTick (change.tick, frame));
        Tick back = -1;
        REQUIRE (compiled.tickForFrame (frame, back));
        REQUIRE (back == change.tick);
    }
    Tick scratch = 0;
    REQUIRE_FALSE (compiled.tickForFrame (-1.0, scratch));
    CompiledTempoMap emptyCompiled;
    REQUIRE (CompiledTempoMap::build (TempoMapView {}, SampleRate { sr }, emptyCompiled));
    REQUIRE (emptyCompiled.tickForFrame (24000.0, scratch));   // 0.5 s at 120 BPM = one quarter
    REQUIRE (scratch == kTicksPerQuarter);

    // Piecewise bar|beat: 4/4 for two bars (8 quarters), then 3/4. Under the ramped tempo above, the
    // frame of tick T comes from the map; the bar|beat of that frame must be T's bar|beat by hand.
    const std::array<MeterChange, 2> meters { MeterChange { 0, 4, 4 }, MeterChange { 8 * kTicksPerQuarter, 3, 4 } };
    const MeterMapView meterView { meters.data(), meters.size() };
    const auto expectAt = [&] (Tick tick, std::int64_t bar, std::int64_t beat)
    {
        double frame = 0.0;
        REQUIRE (compiled.frameForTick (tick, frame));
        BarBeat result;
        REQUIRE (computeBarBeatPiecewise (compiled, meterView, static_cast<std::int64_t> (std::llround (frame)), result));
        INFO ("tick " << tick << " -> " << result.bar << "|" << result.beat);
        REQUIRE (result.bar == bar);
        REQUIRE (result.beat == beat);
    };
    expectAt (0, 1, 1);
    expectAt (kTicksPerQuarter, 1, 2);                       // inside the ramp
    expectAt (5 * kTicksPerQuarter, 2, 2);                   // past the ramp, 4/4 still
    expectAt (8 * kTicksPerQuarter, 3, 1);                   // the meter change starts bar 3 (3/4)
    expectAt (8 * kTicksPerQuarter + 2 * kTicksPerQuarter, 3, 3);
    expectAt (11 * kTicksPerQuarter, 4, 1);                  // three quarters later: bar 4 of 3/4
    expectAt (14 * kTicksPerQuarter + kTicksPerQuarter / 2, 5, 1);
    // With no meter change the piecewise law agrees with the single-tempo law at a constant tempo.
    const std::array<TempoChange, 1> flat { TempoChange { 0, 100.0, TempoCurve::Jump } };
    CompiledTempoMap flatCompiled;
    REQUIRE (CompiledTempoMap::build (TempoMapView { flat.data(), flat.size() }, SampleRate { sr }, flatCompiled));
    for (std::int64_t frame = 0; frame < 480000; frame += 12345)
    {
        BarBeat piecewise;
        REQUIRE (computeBarBeatPiecewise (flatCompiled, MeterMapView {}, frame, piecewise));
        const BarBeat single = computeBarBeat (100.0, 4, 4, sr, frame);
        REQUIRE (piecewise.bar == single.bar);
        REQUIRE (piecewise.beat == single.beat);
    }
}

// UI correctness pass unit 6: barStartTick is the exact inverse of the meter walk, in whole ticks. Expected starts are
// hand-written; every other check asks the walk itself (barBeatAtTick), never the inverse's own arithmetic.
TEST_CASE ("barStartTick inverts the meter walk exactly, and nominalBarTicks is the full bar in force", "[time][g2][tempo][inverse]")
{
    // 4/4; 7/8 from the middle of bar 2 (bar 2 is half a bar); 3/4 from 100 ticks into bar 5 (bar 5 is 100 ticks).
    const std::array<MeterChange, 3> meters { MeterChange { 0, 4, 4 }, MeterChange { 92160, 7, 8 }, MeterChange { 199780, 3, 4 } };
    const MeterMapView view { meters.data(), meters.size() };
    const std::array<Tick, 9> handStarts { 0, 61440, 92160, 145920, 199680, 199780, 245860, 291940, 338020 };
    for (std::size_t i = 0; i < handStarts.size(); ++i)
    {
        Tick start = -1;
        REQUIRE (barStartTick (view, static_cast<std::int64_t> (i) + 1, start));
        INFO ("bar " << i + 1);
        REQUIRE (start == handStarts[i]);
    }

    // Bars 1..200 round-trip to {N, 1}, and the tick before each start reads bar N - 1.
    Tick previous = -1;
    for (std::int64_t bar = 1; bar <= 200; ++bar)
    {
        Tick start = -1;
        INFO ("bar " << bar);
        REQUIRE (barStartTick (view, bar, start));
        INFO ("starts at " << start);
        REQUIRE (start > previous);
        previous = start;
        const BarBeat at = barBeatAtTick (view, start);
        REQUIRE (at.bar == bar);
        REQUIRE (at.beat == 1);
        REQUIRE (barBeatAtTick (view, start - 1).bar == bar - 1);
    }

    // A 3/7 bar is 26331 3/7 ticks, so bar 3 below starts between two ticks: at the next one, 61440 + 26332.
    const std::array<MeterChange, 2> sevenths { MeterChange { 0, 4, 4 }, MeterChange { 61440, 3, 7 } };
    const MeterMapView sevenView { sevenths.data(), sevenths.size() };
    Tick start = -1;
    REQUIRE (barStartTick (sevenView, 3, start));
    REQUIRE (start == 87772);
    REQUIRE (barStartTick (sevenView, 4, start));
    REQUIRE (start == 114103);   // 61440 + 52663 (52662 6/7)

    // A change 26332 ticks in, 4/7 of a tick after the first 3/7 bar line, cuts bar 2 to under one tick: no whole tick
    // reads bar 2 (26331 is bar 1, 26332 is bar 3), so bar 2 starts where bar 3 does - at the change.
    const std::array<MeterChange, 2> phantom { MeterChange { 0, 3, 7 }, MeterChange { 26332, 4, 4 } };
    const MeterMapView phantomView { phantom.data(), phantom.size() };
    REQUIRE (barStartTick (phantomView, 2, start));
    REQUIRE (start == 26332);
    REQUIRE (barStartTick (phantomView, 3, start));
    REQUIRE (start == 26332);
    REQUIRE (barBeatAtTick (phantomView, 26331).bar == 1);
    REQUIRE (barBeatAtTick (phantomView, 26332).bar == 3);
    REQUIRE (barStartTick (phantomView, 4, start));
    REQUIRE (start == 26332 + 61440);

    // Every meter the walk accepts (numerators 1..32, denominators 1..64), from tick 0 and from a change 100 ticks into
    // bar 2, bars 1..400: the start reads the bar and the tick before reads the bar before. Bar x length rounds onto the
    // wrong side of a whole tick for some (3/39 at bar 66), so a rounded-up product alone is not the inverse.
    long long checks = 0, failures = 0;
    std::string first;
    for (std::uint16_t numerator = 1; numerator <= 32; ++numerator)
        for (std::uint16_t denominator = 1; denominator <= 64; ++denominator)
            for (const Tick offset : { Tick { 0 }, Tick { 61540 } })
            {
                const std::array<MeterChange, 2> pair { MeterChange { 0, 4, 4 }, MeterChange { offset, numerator, denominator } };
                const MeterMapView pairView = offset == 0 ? MeterMapView { pair.data() + 1, 1 } : MeterMapView { pair.data(), 2 };
                const std::int64_t firstBar = offset == 0 ? 1 : 3;
                for (std::int64_t bar = firstBar; bar < firstBar + 400; ++bar)
                {
                    ++checks;
                    Tick barStart = -1;
                    const bool ok = barStartTick (pairView, bar, barStart) && barBeatAtTick (pairView, barStart).bar == bar
                                 && barBeatAtTick (pairView, barStart - 1).bar == bar - 1;
                    if (! ok && failures++ == 0)
                        first = std::to_string (numerator) + "/" + std::to_string (denominator) + " offset "
                              + std::to_string (offset) + " bar " + std::to_string (bar) + " -> " + std::to_string (barStart);
                }
            }
    CAPTURE (checks, failures, first);
    REQUIRE (checks == 32LL * 64 * 2 * 400);
    REQUIRE (failures == 0);

    // No bar 0 or below; an empty map is 4/4 from tick 0.
    REQUIRE_FALSE (barStartTick (view, 0, start));
    REQUIRE_FALSE (barStartTick (view, -3, start));
    REQUIRE (barStartTick (MeterMapView {}, 5, start));
    REQUIRE (start == 4 * 61440);

    // The nominal bar is the meter in force's full bar, however short the bar at that tick is.
    REQUIRE (nominalBarTicks (view, 0) == 61440.0);
    REQUIRE (nominalBarTicks (view, 92159) == 61440.0);
    REQUIRE (nominalBarTicks (view, 92160) == 53760.0);
    REQUIRE (nominalBarTicks (view, 199779) == 53760.0);   // bar 5, 100 ticks long
    REQUIRE (nominalBarTicks (view, 199780) == 46080.0);
    REQUIRE (nominalBarTicks (MeterMapView {}, 12345) == 61440.0);
}

using yesdaw::engine::CompiledMeterMap;
using yesdaw::engine::MeterPosition;
using yesdaw::engine::RationalTick;
using yesdaw::engine::TimeLookupStatus;
using yesdaw::engine::TimeRounding;
using yesdaw::engine::roundMusicalTime;
using yesdaw::engine::roundRationalTick;

TEST_CASE ("Checked time rounding preserves integer origins and refuses endpoint overflow", "[time][checked-time]")
{
    constexpr Tick maximum = std::numeric_limits<Tick>::max();
    Tick out = -1;
    for (const TimeRounding rounding : { TimeRounding::Floor, TimeRounding::Nearest, TimeRounding::Ceil })
    {
        REQUIRE (roundMusicalTime ({ maximum, 0.0 }, rounding, out) == TimeLookupStatus::Ok);
        CHECK (out == maximum);
        REQUIRE (roundRationalTick ({ maximum, 0, 39 }, rounding, out) == TimeLookupStatus::Ok);
        CHECK (out == maximum);
    }
    REQUIRE (roundMusicalTime ({ maximum, 0.49 }, TimeRounding::Nearest, out) == TimeLookupStatus::Ok);
    CHECK (out == maximum);
    CHECK (roundMusicalTime ({ maximum, 0.5 }, TimeRounding::Nearest, out) == TimeLookupStatus::Overflow);
    CHECK (out == 0);
    CHECK (roundMusicalTime ({ maximum, 0.01 }, TimeRounding::Ceil, out) == TimeLookupStatus::Overflow);
    CHECK (out == 0);
    REQUIRE (roundRationalTick ({ 11027, 27, 39 }, TimeRounding::Floor, out) == TimeLookupStatus::Ok);
    CHECK (out == 11027);
    REQUIRE (roundRationalTick ({ 11027, 27, 39 }, TimeRounding::Nearest, out) == TimeLookupStatus::Ok);
    CHECK (out == 11028);
    REQUIRE (roundRationalTick ({ 8, 1, 2 }, TimeRounding::Nearest, out) == TimeLookupStatus::Ok);
    CHECK (out == 9);
    CHECK (roundRationalTick ({ maximum, 1, 2 }, TimeRounding::Nearest, out) == TimeLookupStatus::Overflow);
    CHECK (out == 0);
    CHECK (roundRationalTick ({ 3, 1, 0 }, TimeRounding::Floor, out) == TimeLookupStatus::InvalidInput);
    CHECK (out == 0);
    CHECK (roundRationalTick ({ 3, 39, 39 }, TimeRounding::Floor, out) == TimeLookupStatus::InvalidInput);
    CHECK (out == 0);
    for (const MusicalTime invalid : { MusicalTime { -1, 0.0 }, MusicalTime { 1, -0.1 },
                                      MusicalTime { 1, 1.0 }, MusicalTime { 1, std::numeric_limits<double>::quiet_NaN() } })
    {
        out = 123;
        CHECK (roundMusicalTime (invalid, TimeRounding::Floor, out) == TimeLookupStatus::InvalidInput);
        CHECK (out == 0);
    }
    MusicalTime alias { 8, 0.75 };
    REQUIRE (roundMusicalTime (alias, TimeRounding::Nearest, alias.tick) == TimeLookupStatus::Ok);
    CHECK (alias.tick == 9);
    RationalTick rationalAlias { 8, 3, 4 };
    REQUIRE (roundRationalTick (rationalAlias, TimeRounding::Ceil, rationalAlias.whole) == TimeLookupStatus::Ok);
    CHECK (rationalAlias.whole == 9);
    CHECK (roundMusicalTime ({ 1, 0.0 }, static_cast<TimeRounding> (255), out) == TimeLookupStatus::InvalidInput);
    CHECK (out == 0);
    CHECK (roundRationalTick ({ -1, 0, 1 }, TimeRounding::Floor, out) == TimeLookupStatus::InvalidInput);
    CHECK (out == 0);
}

TEST_CASE ("Checked fractional Tempo conversion uses local time and independent analytic frames", "[time][checked-time]")
{
    constexpr Tick origin = Tick { 1 } << 60;
    const std::array<TempoChange, 2> huge {
        TempoChange { 0, static_cast<double> (origin), TempoCurve::Jump },
        TempoChange { origin, 120.0, TempoCurve::Jump }
    };
    CompiledTempoMap compiled;
    REQUIRE (CompiledTempoMap::build ({ huge.data(), huge.size() }, { 48000.0 }, compiled));
    double frame = -1.0;
    REQUIRE (compiled.frameForMusicalTime ({ origin, 0.5 }, frame) == TimeLookupStatus::Ok);
    // The prefix is exactly 187.5, then half a Tick at 120 BPM adds 0.78125 frames.
    CHECK (frame == 188.28125);
    MusicalTime time { -1, 0.1 };
    REQUIRE (compiled.musicalTimeForFrame (188.28125, time) == TimeLookupStatus::Ok);
    CHECK (time == MusicalTime { origin, 0.5 });
    MusicalTime alias { origin, 0.5 };
    REQUIRE (compiled.frameForMusicalTime (alias, alias.frac) == TimeLookupStatus::Ok);
    CHECK (alias.frac == 188.28125);
    alias = { 0, 188.28125 };
    REQUIRE (compiled.musicalTimeForFrame (alias.frac, alias) == TimeLookupStatus::Ok);
    CHECK (alias == MusicalTime { origin, 0.5 });

    const std::array<TempoChange, 3> ramps {
        TempoChange { 0, 64.0, TempoCurve::LinearRamp },
        TempoChange { 1000, 128.0, TempoCurve::LinearRamp },
        TempoChange { 2000, 32.0, TempoCurve::Jump }
    };
    REQUIRE (CompiledTempoMap::build ({ ramps.data(), ramps.size() }, { 48000.0 }, compiled));
    for (const MusicalTime point : { MusicalTime { 0, 0.25 }, MusicalTime { 499, 0.5 }, MusicalTime { 999, 0.75 },
                                     MusicalTime { 1000, 0.25 }, MusicalTime { 1500, 0.5 }, MusicalTime { 2000, 0.5 } })
    {
        const double t = static_cast<double> (point.tick) + point.frac; // small, independent oracle only
        const double prefix = (187500.0 / 64.0) * std::log (2.0);
        const double rampEnd = prefix - (187500.0 / 96.0) * std::log (0.25);
        const double expected = t < 1000.0 ? (187500.0 / 64.0) * std::log (1.0 + t / 1000.0)
            : t < 2000.0 ? prefix - (187500.0 / 96.0) * std::log (1.0 - 0.75 * ((t - 1000.0) / 1000.0))
                         : rampEnd + (t - 2000.0) * 187.5 / 32.0;
        CAPTURE (point.tick, point.frac, expected);
        REQUIRE (compiled.frameForMusicalTime (point, frame) == TimeLookupStatus::Ok);
        CHECK (std::abs (frame - expected) <= 2.0e-11);
        REQUIRE (compiled.musicalTimeForFrame (expected, time) == TimeLookupStatus::Ok);
        CHECK (std::abs (static_cast<double> (time.tick - point.tick) + time.frac - point.frac) <= 1.0e-10);
    }
    for (Tick tick = 0; tick <= 2200; tick += 11)
    {
        double whole = -1.0;
        REQUIRE (compiled.frameForTick (tick, whole));
        REQUIRE (compiled.frameForMusicalTime ({ tick, 0.0 }, frame) == TimeLookupStatus::Ok);
        CHECK (frame == whole);
    }
}

TEST_CASE ("Checked Tempo states and native Frame bounds fail with reset outputs", "[time][checked-time]")
{
    CompiledTempoMap compiled;
    double frame = 123.0;
    MusicalTime time { 123, 0.5 };
    CHECK (compiled.frameForMusicalTime ({ 0, 0.0 }, frame) == TimeLookupStatus::InvalidMap);
    CHECK (frame == 0.0);
    CHECK (compiled.musicalTimeForFrame (0.0, time) == TimeLookupStatus::InvalidMap);
    CHECK (time == MusicalTime {});
    REQUIRE (CompiledTempoMap::build ({}, { 48000.0 }, compiled));
    REQUIRE (compiled.frameForMusicalTime ({ 1, 0.5 }, frame) == TimeLookupStatus::Ok);
    CHECK (frame == 2.34375);
    for (const double invalid : { -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() })
    {
        time = { 123, 0.5 };
        CHECK (compiled.musicalTimeForFrame (invalid, time) == TimeLookupStatus::InvalidInput);
        CHECK (time == MusicalTime {});
    }
    CHECK (compiled.musicalTimeForFrame (0x1p63, time) == TimeLookupStatus::Overflow);
    CHECK (time == MusicalTime {});
    CHECK (compiled.frameForMusicalTime ({ std::numeric_limits<Tick>::max(), 0.5 }, frame) == TimeLookupStatus::Overflow);
    CHECK (frame == 0.0);
    CHECK (compiled.frameForMusicalTime ({ 1, 1.0 }, frame) == TimeLookupStatus::InvalidInput);
    CHECK (frame == 0.0);
    const TempoChange invalid { 1, 120.0, TempoCurve::Jump };
    REQUIRE_FALSE (CompiledTempoMap::build ({ &invalid, 1 }, { 48000.0 }, compiled));
    CHECK (compiled.frameForMusicalTime ({ 0, 0.0 }, frame) == TimeLookupStatus::InvalidMap);
    CHECK (frame == 0.0);

    const std::array<TempoChange, 2> tiny {
        TempoChange { 0, 1.0e-300, TempoCurve::LinearRamp },
        TempoChange { Tick { 1 } << 48, std::nextafter (1.0e-300, 1.0), TempoCurve::Jump }
    };
    REQUIRE (tiny[0].hasValidBpm());
    REQUIRE (tiny[1].hasValidBpm());
    REQUIRE_FALSE (CompiledTempoMap::build ({ tiny.data(), tiny.size() }, { 48000.0 }, compiled));
    CHECK (compiled.frameForMusicalTime ({ 0, 0.0 }, frame) == TimeLookupStatus::NumericDomain);
    CHECK (frame == 0.0);
    CHECK (compiled.musicalTimeForFrame (0.0, time) == TimeLookupStatus::NumericDomain);
    CHECK (time == MusicalTime {});

    // Native Frame inputs remain in range, but these local Tick results do not.
    const TempoChange fast { 0, 187.5, TempoCurve::Jump };
    REQUIRE (CompiledTempoMap::build ({ &fast, 1 }, { 48000.0 }, compiled));
    REQUIRE (compiled.musicalTimeForFrame (std::nextafter (0x1p63, 0.0), time) == TimeLookupStatus::Ok);
    CHECK (time.tick == std::numeric_limits<Tick>::max() - 1023);
    const std::array<TempoChange, 2> atEnd {
        TempoChange { 0, static_cast<double> (std::numeric_limits<Tick>::max()), TempoCurve::Jump },
        TempoChange { std::numeric_limits<Tick>::max(), 120.0, TempoCurve::Jump }
    };
    REQUIRE (CompiledTempoMap::build ({ atEnd.data(), atEnd.size() }, { 48000.0 }, compiled));
    REQUIRE (compiled.musicalTimeForFrame (188.28125, time) == TimeLookupStatus::Ok);
    CHECK (time == MusicalTime { std::numeric_limits<Tick>::max(), 0.5 });
    CHECK (compiled.musicalTimeForFrame (189.0625, time) == TimeLookupStatus::Overflow);
    CHECK (time == MusicalTime {});
    CompiledTempoMap moved = std::move (compiled);
    REQUIRE (moved.frameForMusicalTime ({ 0, 0.0 }, frame) == TimeLookupStatus::Ok);
    CHECK (compiled.frameForMusicalTime ({ 0, 0.0 }, frame) == TimeLookupStatus::InvalidMap);
    CHECK (frame == 0.0);
    CHECK (compiled.musicalTimeForFrame (0.0, time) == TimeLookupStatus::InvalidMap);
    CHECK (time == MusicalTime {});
}

TEST_CASE ("Checked fractional Tempo conversion preserves tiny valid clocks or names their numeric domain", "[time][checked-time]")
{
    const double startBpm = std::ldexp (1.0, -700);
    const std::array<TempoChange, 2> tiny {
        TempoChange { 0, startBpm, TempoCurve::LinearRamp },
        TempoChange { Tick { 1 } << 48, std::nextafter (startBpm, 1.0), TempoCurve::Jump }
    };
    CompiledTempoMap compiled;
    REQUIRE (CompiledTempoMap::build ({ tiny.data(), tiny.size() }, { std::ldexp (1.0, -1000) }, compiled));
    double frame = -1.0;
    // k/BPM = 2^-308; the fractional ramp correction is below the expected frame's ULP.
    REQUIRE (compiled.frameForMusicalTime ({ 0, 0.5 }, frame) == TimeLookupStatus::Ok);
    CHECK (frame == std::ldexp (1.0, -309));
    MusicalTime time;
    REQUIRE (compiled.musicalTimeForFrame (std::ldexp (1.0, -309), time) == TimeLookupStatus::Ok);
    CHECK (time == MusicalTime { 0, 0.5 });
    const std::array<TempoChange, 2> shallow {
        TempoChange { 0, 64.0, TempoCurve::LinearRamp },
        TempoChange { Tick { 1 } << 48, std::nextafter (64.0, 128.0), TempoCurve::Jump }
    };
    REQUIRE (CompiledTempoMap::build ({ shallow.data(), shallow.size() }, { 48000.0 }, compiled));
    CHECK (compiled.musicalTimeForFrame (std::numeric_limits<double>::denorm_min(), time) == TimeLookupStatus::NumericDomain);
    CHECK (time == MusicalTime {});
    const TempoChange tinyConstant { 0, std::numeric_limits<double>::denorm_min(), TempoCurve::Jump };
    REQUIRE (tinyConstant.hasValidBpm());
    REQUIRE (CompiledTempoMap::build ({ &tinyConstant, 1 }, { 48000.0 }, compiled));
    CHECK (compiled.frameForMusicalTime ({ 1, 0.0 }, frame) == TimeLookupStatus::NumericDomain);
    CHECK (frame == 0.0);
}

TEST_CASE ("Compiled Meter boundaries are exact rationals and changes truncate the old bar", "[time][checked-time][exact-meter]")
{
    const MeterChange seven { 0, 7, 39 };
    CompiledMeterMap compiled;
    REQUIRE (CompiledMeterMap::build ({ &seven, 1 }, compiled) == TimeLookupStatus::Ok);
    RationalTick start;
    REQUIRE (compiled.beatStart (0, 1, start) == TimeLookupStatus::Ok);
    CHECK (start.whole == 1575);
    CHECK (start.numerator * 13u == start.denominator * 5u);
    REQUIRE (compiled.barStart (2, start) == TimeLookupStatus::Ok);
    CHECK (start.whole == 11027);
    CHECK (start.numerator * 13u == start.denominator * 9u);
    REQUIRE (compiled.barStart (3, start) == TimeLookupStatus::Ok);
    CHECK (start.whole == 22055);
    CHECK (start.numerator * 13u == start.denominator * 5u);
    REQUIRE (compiled.barStart (14, start) == TimeLookupStatus::Ok);
    CHECK (start.whole == 143360);
    CHECK (start.numerator == 0);
    MeterPosition position;
    REQUIRE (compiled.positionForTick (11027, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 1);
    CHECK (position.barBeat.beat == 7);
    CHECK (position.beatOrdinal == 6);
    CHECK_FALSE (position.downbeat);
    REQUIRE (compiled.positionForTick (11028, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 2);
    CHECK (position.barBeat.beat == 1);
    CHECK (position.beatOrdinal == 7);
    CHECK (position.downbeat);
    const std::array<MeterChange, 2> phantom { MeterChange { 0, 3, 7 }, MeterChange { 26332, 4, 4 } };
    REQUIRE (CompiledMeterMap::build ({ phantom.data(), phantom.size() }, compiled) == TimeLookupStatus::Ok);
    REQUIRE (compiled.barStart (2, start) == TimeLookupStatus::Ok);
    CHECK (start.whole == 26331);
    CHECK (start.numerator * 7u == start.denominator * 3u);
    REQUIRE (compiled.barStart (3, start) == TimeLookupStatus::Ok);
    CHECK (start.whole == 26332);
    CHECK (start.numerator == 0);
    REQUIRE (compiled.positionForTick (26331, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 1);
    REQUIRE (compiled.positionForTick (26332, position) == TimeLookupStatus::Ok);
    CHECK (position.segment == 1);
    CHECK (position.barBeat.bar == 3);
    CHECK (position.barBeat.beat == 1);
    CHECK (position.downbeat);
    REQUIRE (compiled.beatStart (0, 3, start) == TimeLookupStatus::Ok);
    CHECK (start.whole == 26331);
    CHECK (compiled.beatStart (0, 4, start) == TimeLookupStatus::InvalidInput);
    CHECK (start == RationalTick {});
    const std::array<MeterChange, 2> onBar { MeterChange { 0, 4, 4 }, MeterChange { 61440, 3, 4 } };
    REQUIRE (CompiledMeterMap::build ({ onBar.data(), onBar.size() }, compiled) == TimeLookupStatus::Ok);
    CHECK (compiled.beatStart (0, 4, start) == TimeLookupStatus::InvalidInput);
    REQUIRE (compiled.positionForTick (61439, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 1);
    REQUIRE (compiled.positionForTick (61440, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 2);
    REQUIRE (compiled.positionForTick (61441, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 2);
}

TEST_CASE ("Compiled Meter accepts stored extremes and checks long range and failed builds", "[time][checked-time][exact-meter]")
{
    CompiledMeterMap compiled;
    MeterPosition position { 7, 9, { 8, 3 }, false };
    RationalTick start { 123, 1, 2 };
    CHECK (compiled.positionForTick (0, position) == TimeLookupStatus::InvalidMap);
    CHECK (position.segment == 0);
    CHECK (position.beatOrdinal == 0);
    CHECK (position.barBeat.bar == 1);
    CHECK (compiled.barStart (1, start) == TimeLookupStatus::InvalidMap);
    CHECK (start == RationalTick {});
    REQUIRE (CompiledMeterMap::build ({}, compiled) == TimeLookupStatus::Ok);
    REQUIRE (compiled.barStart (5, start) == TimeLookupStatus::Ok);
    CHECK (start.whole == 245760);
    CHECK (compiled.positionForTick (-1, position) == TimeLookupStatus::InvalidInput);
    CHECK (compiled.barStart (0, start) == TimeLookupStatus::InvalidInput);
    CHECK (compiled.beatStart (1, 0, start) == TimeLookupStatus::InvalidInput);
    CHECK (start == RationalTick {});
    CHECK (compiled.barStart (std::numeric_limits<Tick>::max(), start) == TimeLookupStatus::Overflow);
    CHECK (start == RationalTick {});

    for (const MeterChange meter : { MeterChange { 0, 65535, 65535 }, MeterChange { 0, 65535, 1 }, MeterChange { 0, 1, 65535 } })
    {
        CAPTURE (meter.numerator, meter.denominator);
        REQUIRE (meter.isValid());
        REQUIRE (CompiledMeterMap::build ({ &meter, 1 }, compiled) == TimeLookupStatus::Ok);
        REQUIRE (compiled.beatStart (0, meter.numerator, start) == TimeLookupStatus::Ok);
        const std::uint64_t dividend = 61440ULL * meter.numerator;
        CHECK (start.whole == static_cast<Tick> (dividend / meter.denominator));
        CHECK (static_cast<std::uint64_t> (start.numerator) * meter.denominator
               == (dividend % meter.denominator) * start.denominator);
    }
    const MeterChange wide { 0, 65535, 65535 };
    REQUIRE (CompiledMeterMap::build ({ &wide, 1 }, compiled) == TimeLookupStatus::Ok);
    constexpr Tick longTick = (Tick { 1 } << 60) + 123;
    REQUIRE (compiled.positionForTick (longTick, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == longTick / 61440 + 1);
    REQUIRE (compiled.barStart (position.barBeat.bar, start) == TimeLookupStatus::Ok);
    CHECK (start.whole == (longTick / 61440) * 61440);
    CHECK (start.numerator == 0);
    CHECK (compiled.beatStart (0, std::numeric_limits<std::uint64_t>::max(), start) == TimeLookupStatus::Overflow);
    CHECK (start == RationalTick {});
    const std::array<MeterChange, 2> tooManyBars { MeterChange { 0, 1, 65535 },
        MeterChange { std::numeric_limits<Tick>::max(), 4, 4 } };
    CHECK (CompiledMeterMap::build ({ tooManyBars.data(), tooManyBars.size() }, compiled) == TimeLookupStatus::Overflow);
    CHECK (compiled.positionForTick (0, position) == TimeLookupStatus::Overflow);
    const std::array<MeterChange, 2> bad[] = {
        { MeterChange { 1, 4, 4 }, MeterChange { 2, 4, 4 } },
        { MeterChange { 0, 4, 4 }, MeterChange { 0, 3, 4 } },
        { MeterChange { 0, 4, 4 }, MeterChange { 100, 0, 4 } },
        { MeterChange { 0, 4, 4 }, MeterChange { -1, 4, 4 } }
    };
    for (const auto& map : bad)
    {
        REQUIRE (CompiledMeterMap::build ({}, compiled) == TimeLookupStatus::Ok);
        CHECK (CompiledMeterMap::build ({ map.data(), map.size() }, compiled) == TimeLookupStatus::InvalidMap);
        CHECK (compiled.barStart (1, start) == TimeLookupStatus::InvalidMap);
        CHECK (start == RationalTick {});
    }
    CHECK (CompiledMeterMap::build ({ nullptr, 1 }, compiled) == TimeLookupStatus::InvalidMap);
    REQUIRE (CompiledMeterMap::build ({}, compiled) == TimeLookupStatus::Ok);
    CompiledMeterMap moved = std::move (compiled);
    REQUIRE (moved.barStart (1, start) == TimeLookupStatus::Ok);
    CHECK (compiled.positionForTick (0, position) == TimeLookupStatus::InvalidMap);
    CHECK (compiled.barStart (1, start) == TimeLookupStatus::InvalidMap);
    CHECK (compiled.beatStart (0, 0, start) == TimeLookupStatus::InvalidMap);
    CHECK (start == RationalTick {});
}

TEST_CASE ("Exact Meter queries agree with bounded independent integer rational clocks", "[time][checked-time][exact-meter]")
{
    for (const std::uint16_t numerator : { std::uint16_t { 1 }, std::uint16_t { 3 }, std::uint16_t { 7 }, std::uint16_t { 65535 } })
        for (const std::uint16_t denominator : { std::uint16_t { 7 }, std::uint16_t { 39 }, std::uint16_t { 65535 } })
        {
            const std::array<MeterChange, 2> map { MeterChange { 0, 4, 4 }, MeterChange { 61540, numerator, denominator } };
            CompiledMeterMap compiled;
            REQUIRE (CompiledMeterMap::build ({ map.data(), map.size() }, compiled) == TimeLookupStatus::Ok);
            for (const Tick local : { Tick { 0 }, Tick { 1 }, Tick { 1575 }, Tick { 11027 }, Tick { 11028 }, Tick { 1000000 } })
            {
                // Products are intentionally bounded in this independent test oracle.
                const auto ordinal = static_cast<std::uint64_t> (local) * denominator / 61440ULL;
                MeterPosition position;
                REQUIRE (compiled.positionForTick (61540 + local, position) == TimeLookupStatus::Ok);
                CHECK (position.segment == 1);
                CHECK (position.beatOrdinal == ordinal);
                CHECK (position.barBeat.bar == static_cast<Tick> (ordinal / numerator) + 3);
                CHECK (position.barBeat.beat == static_cast<Tick> (ordinal % numerator) + 1);
                CHECK (position.downbeat == (ordinal % numerator == 0));
            }
        }
}

TEST_CASE ("Fractional Meter positions cross exact rational boundaries without flooring first", "[time][checked-time][exact-meter]")
{
    const MeterChange seven { 0, 7, 39 };
    CompiledMeterMap compiled;
    REQUIRE (CompiledMeterMap::build ({ &seven, 1 }, compiled) == TimeLookupStatus::Ok);
    MeterPosition position;
    REQUIRE (compiled.positionForMusicalTime ({ 11027, 0.5 }, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 1);
    CHECK (position.barBeat.beat == 7);
    REQUIRE (compiled.positionForMusicalTime ({ 11027, 0.75 }, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 2);
    CHECK (position.barBeat.beat == 1);
    CHECK (position.beatOrdinal == 7);
    const MeterChange binary { 0, 1, 32768 };
    REQUIRE (CompiledMeterMap::build ({ &binary, 1 }, compiled) == TimeLookupStatus::Ok);
    REQUIRE (compiled.positionForMusicalTime ({ 1, std::nextafter (0.875, 0.0) }, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 1);
    REQUIRE (compiled.positionForMusicalTime ({ 1, 0.875 }, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 2);
    REQUIRE (compiled.positionForMusicalTime ({ 1, std::nextafter (0.875, 1.0) }, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 2);
    const MeterChange shortest { 0, 7, 65535 };
    REQUIRE (CompiledMeterMap::build ({ &shortest, 1 }, compiled) == TimeLookupStatus::Ok);
    // Beat ordinals 16 and 17 begin at 15.0002... and 15.9377...; both follow whole Tick 15.
    REQUIRE (compiled.positionForMusicalTime ({ 15, 0.999 }, position) == TimeLookupStatus::Ok);
    CHECK (position.beatOrdinal == 17);
    CHECK (position.barBeat.bar == 3);
    CHECK (position.barBeat.beat == 4);
    const std::array<MeterChange, 2> phantom { MeterChange { 0, 3, 7 }, MeterChange { 26332, 4, 4 } };
    REQUIRE (CompiledMeterMap::build ({ phantom.data(), phantom.size() }, compiled) == TimeLookupStatus::Ok);
    REQUIRE (compiled.positionForMusicalTime ({ 26331, 0.5 }, position) == TimeLookupStatus::Ok);
    CHECK (position.barBeat.bar == 2);
    REQUIRE (compiled.positionForMusicalTime ({ 26331, 0.99 }, position) == TimeLookupStatus::Ok);
    CHECK (position.segment == 0);
    CHECK (position.barBeat.bar == 2);
    for (const Tick tick : { Tick { 0 }, Tick { 26331 }, Tick { 26332 }, Tick { 99999 } })
    {
        MeterPosition whole;
        REQUIRE (compiled.positionForTick (tick, whole) == TimeLookupStatus::Ok);
        REQUIRE (compiled.positionForMusicalTime ({ tick, 0.0 }, position) == TimeLookupStatus::Ok);
        CHECK (position.segment == whole.segment);
        CHECK (position.beatOrdinal == whole.beatOrdinal);
        CHECK (position.barBeat.bar == whole.barBeat.bar);
        CHECK (position.barBeat.beat == whole.barBeat.beat);
        CHECK (position.downbeat == whole.downbeat);
    }
    CHECK (compiled.positionForMusicalTime ({ 1, 1.0 }, position) == TimeLookupStatus::InvalidInput);
    CHECK (position.beatOrdinal == 0);
    CHECK (position.barBeat.bar == 1);
    REQUIRE (CompiledMeterMap::build ({}, compiled) == TimeLookupStatus::Ok);
    MeterPosition finalWhole;
    REQUIRE (compiled.positionForTick (std::numeric_limits<Tick>::max(), finalWhole) == TimeLookupStatus::Ok);
    RationalTick unavailableNext;
    CHECK (compiled.beatStart (0, finalWhole.beatOrdinal + 1u, unavailableNext) == TimeLookupStatus::Overflow);
    REQUIRE (compiled.positionForMusicalTime ({ std::numeric_limits<Tick>::max(), 0.5 }, position) == TimeLookupStatus::Ok);
    CHECK (position.segment == finalWhole.segment);
    CHECK (position.beatOrdinal == finalWhole.beatOrdinal);
    CHECK (position.barBeat.bar == finalWhole.barBeat.bar);
    CHECK (position.barBeat.beat == finalWhole.barBeat.beat);
    CHECK (position.downbeat == finalWhole.downbeat);
    CompiledMeterMap unbuilt;
    CHECK (unbuilt.positionForMusicalTime ({ 1, 0.5 }, position) == TimeLookupStatus::InvalidMap);
    CHECK (position.beatOrdinal == 0);
}

#include "rt/RtHot.h"

namespace {
// Catch assertions stay outside the RT scope. The existing RTSan CI lane instruments this
// wrapper and rejects allocations, locks or I/O anywhere under the immutable Meter queries.
bool meterQueriesInRealtimeScope (const CompiledMeterMap& map) noexcept YESDAW_RT_HOT
{
    for (Tick tick = 11027; tick < 11027 + 257; ++tick)
    {
        MeterPosition position;
        RationalTick bar, beat;
        if (map.positionForTick (tick, position) != TimeLookupStatus::Ok
            || map.positionForMusicalTime ({ tick, 0.75 }, position) != TimeLookupStatus::Ok
            || map.barStart (position.barBeat.bar, bar) != TimeLookupStatus::Ok
            || map.beatStart (position.segment, position.beatOrdinal, beat) != TimeLookupStatus::Ok
            || bar.whole > tick || beat.whole > tick)
            return false;
    }
    RationalTick refused;
    return map.beatStart (0, 1000, refused) == TimeLookupStatus::InvalidInput
        && refused == RationalTick {};
}
} // namespace

TEST_CASE ("Compiled Meter queries stay inside a real-time-safe scope", "[time][checked-time][exact-meter][rtsan]")
{
    const std::array<MeterChange, 2> map { MeterChange { 0, 7, 39 }, MeterChange { 26332, 65535, 65535 } };
    CompiledMeterMap compiled;
    REQUIRE (CompiledMeterMap::build ({ map.data(), map.size() }, compiled) == TimeLookupStatus::Ok);
    REQUIRE (meterQueriesInRealtimeScope (compiled));
}

TEST_CASE ("Checked Tempo inverse refuses a collapsed prefix without suppressing forward clocks", "[time][checked-time][collapsed-prefix]")
{
    // At 48 kHz, one quarter at 120 BPM is exactly 24000 Frames. The next Tick
    // takes 1.875e-298 Frames: positive in the specified clock, lost in its prefix sum.
    const std::array<TempoChange, 3> changes {
        TempoChange { 0, 120.0, TempoCurve::Jump },
        TempoChange { kTicksPerQuarter, 1.0e300, TempoCurve::Jump },
        TempoChange { kTicksPerQuarter + 1, 120.0, TempoCurve::Jump }
    };
    CompiledTempoMap map;
    REQUIRE (CompiledTempoMap::build ({ changes.data(), changes.size() }, { 48000.0 }, map));
    double frame = -1.0;
    REQUIRE (map.frameForTick (kTicksPerQuarter, frame));
    CHECK (frame == 24000.0);
    REQUIRE (map.frameForTick (kTicksPerQuarter + 1, frame));
    CHECK (frame == 24000.0);
    REQUIRE (map.frameForMusicalTime ({ kTicksPerQuarter, 0.5 }, frame) == TimeLookupStatus::Ok);
    CHECK (frame == 24000.0);
    MusicalTime time { 123, 0.5 };
    CHECK (map.musicalTimeForFrame (24000.0, time) == TimeLookupStatus::NumericDomain);
    CHECK (time == MusicalTime {});
    REQUIRE (map.musicalTimeForFrame (23999.0, time) == TimeLookupStatus::Ok);
    CHECK (time.tick == kTicksPerQuarter - 1);
    CHECK (std::abs (time.frac - 0.36) < 1.0e-9);
    REQUIRE (map.musicalTimeForFrame (24001.0, time) == TimeLookupStatus::Ok);
    CHECK (time.tick == kTicksPerQuarter + 1);
    CHECK (std::abs (time.frac - 0.64) < 1.0e-12);

    // An ordinary exact boundary still resolves to its own origin, including the
    // first boundary. Replacing upper_bound with lower_bound would break this case.
    auto strict = changes;
    strict[1].bpm = 240.0;
    REQUIRE (CompiledTempoMap::build ({ strict.data(), strict.size() }, { 48000.0 }, map));
    REQUIRE (map.musicalTimeForFrame (24000.0, time) == TimeLookupStatus::Ok);
    CHECK (time == MusicalTime { kTicksPerQuarter, 0.0 });
    REQUIRE (map.musicalTimeForFrame (24000.78125, time) == TimeLookupStatus::Ok);
    CHECK (time == MusicalTime { kTicksPerQuarter + 1, 0.0 });
}
