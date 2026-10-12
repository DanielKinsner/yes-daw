// YES DAW - headless checks for ADR-0010 time model value types.
//
// This locks the storage-facing type surface before the Project round-trip code starts using it.

#include "engine/Node.h"
#include "engine/Time.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
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
