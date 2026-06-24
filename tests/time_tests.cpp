// YES DAW - headless checks for ADR-0010 time model value types.
//
// This locks the storage-facing type surface before the Project round-trip code starts using it.

#include "engine/Node.h"
#include "engine/Time.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>

using yesdaw::engine::MeterChange;
using yesdaw::engine::MeterMapView;
using yesdaw::engine::MusicalTime;
using yesdaw::engine::ResampleQuality;
using yesdaw::engine::SampleRate;
using yesdaw::engine::TempoChange;
using yesdaw::engine::TempoCurve;
using yesdaw::engine::TempoMapView;
using yesdaw::engine::Tick;
using yesdaw::engine::TimeBase;
using yesdaw::engine::Transport;
using yesdaw::engine::kTicksPerQuarter;

static_assert (std::is_same_v<Tick, std::int64_t>);
static_assert (kTicksPerQuarter == 15360);
static_assert (std::is_trivially_copyable_v<MusicalTime>);
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
