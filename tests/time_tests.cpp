// YES DAW - headless checks for ADR-0010 time model value types.
//
// This locks the storage-facing type surface before the Project round-trip code starts using it.

#include "engine/Node.h"
#include "engine/Time.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
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
