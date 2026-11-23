#include "engine/MusicalAudioEnvelope.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <memory>
#include <vector>

TEST_CASE ("Musical fades follow fractional ticks across a tempo change on both channels", "[musical-audio]")
{
    using namespace yesdaw::engine;
    const Tick start = 1, half = 4 * kTicksPerQuarter;
    const std::array<TempoChange, 2> rows {{ { 0, 120.0 }, { start + half, 90.0 } }};
    const TempoMapView map { rows.data(), rows.size() };
    std::shared_ptr<const MusicalAudioClock> clock;
    REQUIRE (MusicalAudioClock::build (map, { 48000.0 }, {}, clock) == MusicalAudioStatus::Ok);
    MusicalAudioWindow window;
    REQUIRE (resolveMusicalAudioWindow (clock, start, 2 * half, 192000, window) == MusicalAudioStatus::Ok);
    CompiledTempoMap inverse;
    REQUIRE (CompiledTempoMap::build (map, { 48000.0 }, inverse));
    Clip clip;
    clip.timeBase = TimeBase::TempoLocked;
    clip.timelineStart = start;
    clip.timelineLength = 2 * half;
    clip.fadeIn = 3 * half / 2;
    clip.fadeOut = 3 * half;
    clip.fadeInShape = clip.fadeOutShape = FadeShape::Linear;
    clip.gain = 0.0f;
    std::vector<float> audio (static_cast<std::size_t> (window.frameCount()) * 2, 0.25f);
    REQUIRE (applyMusicalAudioEnvelope (clip, window, inverse, 2, audio) == MusicalEnvelopeStatus::Ok);
    double worst = 0;
    for (std::size_t i = 0; i < audio.size() / 2; ++i)
    {
        const double local = std::max (0.0, static_cast<double> (i) - 0.5625);
        const double ticks = local < 96000 ? local * 0.64 : 61440.0 + (local - 96000) * 0.48;
        const double expected = 0.25 * std::min (ticks / 92160.0, 1 - ticks / 184320.0);
        for (std::size_t c = 0; c < 2; ++c)
            worst = std::max (worst, std::abs (audio[2 * i + c] - expected));
    }
    CHECK (worst <= 2e-6);
    CHECK (audio.front() == 0.0f);
}
