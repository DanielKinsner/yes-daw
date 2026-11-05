// YES DAW — headless checks for the H0 spike #2 timeline layout (viewport virtualization).
//
// Pure C++ + Catch2, no JUCE/GPU/display — proves the per-frame CPU work is correct AND cheap, so the
// "can we afford 100+ elements at 60 fps?" risk is answered mechanically. The GPU rendering smoothness
// is the real-hardware soak's job; this is the part that can regress silently and must be gated.

#include "ui/MusicalGrid.h"   // the layout pass, step 13
#include "ui/TimelineLayout.h"
#include "ui/TimelineReadout.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <vector>

using yesdaw::ui::Clip;
using yesdaw::ui::Viewport;
using yesdaw::ui::ElementRect;
using yesdaw::ui::hitTestVisibleClip;
using yesdaw::ui::layoutVisible;

namespace {
bool approx (double a, double b, double tol = 1e-3) { return std::fabs (a - b) <= tol; }
}

TEST_CASE ("native readouts project absolute musical endpoints before rounding duration",
           "[timeline][native-readout]")
{
    using namespace yesdaw;
    engine::Project project;
    project.sampleRate = { 48000.0 };
    project.tempoMap = { { 0, 120.0, engine::TempoCurve::Jump }, { 15360, 60.0, engine::TempoCurve::Jump } };
    project.meterMap = { { 0, 4, 4 } };
    const auto clock = ui::timelineReadoutClock (project);
    REQUIRE (clock != nullptr);
    ui::TimelineReadout readout;
    REQUIRE (ui::timelineReadout (clock, { engine::TimeBase::TempoLocked, 15360 }, 15360,
        project.sampleRate, readout) == ui::TimelineCoordinateStatus::Ok);
    CHECK (approx (readout.startSeconds, 0.5, 1e-12));
    CHECK (approx (readout.endSeconds, 1.5, 1e-12));
    CHECK (approx (readout.lengthSeconds, 1.0, 1e-12));
    engine::Tick rounded = 0;
    REQUIRE (ui::timelineNativeFrameTarget (nullptr, engine::TimeBase::SampleLocked, readout.duration, rounded)
             == ui::TimelineCoordinateStatus::Ok);
    CHECK (rounded == 48000);
    engine::Tick end = 0;
    REQUIRE (ui::timelineReadoutNativeEnd (clock, { engine::TimeBase::TempoLocked, 15360 }, 15360,
        project.sampleRate, 0.25, true, end) == ui::TimelineCoordinateStatus::Ok);
    CHECK (end == 19200);   // .5 s start + .25 s duration, at the 60 BPM segment
    REQUIRE (ui::timelineReadoutNativeEnd (clock, { engine::TimeBase::TempoLocked, 15360 }, 15360,
        project.sampleRate, 0.75, false, end) == ui::TimelineCoordinateStatus::Ok);
    CHECK (end == 19200);
    project.tempoMap = { { 0, 240.0, engine::TempoCurve::Jump } };
    const auto fast = ui::timelineReadoutClock (project);
    REQUIRE (ui::timelineReadout (fast, { engine::TimeBase::TempoLocked, 2 }, 1,
        project.sampleRate, readout) == ui::TimelineCoordinateStatus::Ok);
    CHECK (approx (readout.startSeconds * 48000.0, 1.5625, 1e-12));
    CHECK (approx (readout.endSeconds * 48000.0, 2.34375, 1e-12));
    CHECK (approx (readout.lengthSeconds * 48000.0, 0.78125, 1e-12));
    engine::Tick startRounded = 0, endRounded = 0;
    REQUIRE (ui::timelineNativeFrameTarget (nullptr, engine::TimeBase::SampleLocked, readout.span.start, startRounded)
             == ui::TimelineCoordinateStatus::Ok);
    REQUIRE (ui::timelineNativeFrameTarget (nullptr, engine::TimeBase::SampleLocked, readout.span.end, endRounded)
             == ui::TimelineCoordinateStatus::Ok);
    REQUIRE (ui::timelineNativeFrameTarget (nullptr, engine::TimeBase::SampleLocked, readout.duration, rounded)
             == ui::TimelineCoordinateStatus::Ok);
    CHECK (startRounded == 2);
    CHECK (endRounded == 2);
    CHECK (rounded == 1);
}

TEST_CASE ("native readout ramp duration follows its actual placement",
           "[timeline][native-readout]")
{
    using namespace yesdaw;
    engine::Project project;
    project.sampleRate = { 48000.0 };
    project.tempoMap = { { 0, 120.0, engine::TempoCurve::LinearRamp }, { 30720, 240.0, engine::TempoCurve::Jump } };
    project.meterMap = { { 0, 4, 4 } };
    const auto clock = ui::timelineReadoutClock (project);
    REQUIRE (clock != nullptr);
    ui::TimelineReadout first, moved;
    REQUIRE (ui::timelineReadout (clock, { engine::TimeBase::TempoLocked, 0 }, 15360,
        project.sampleRate, first) == ui::TimelineCoordinateStatus::Ok);
    REQUIRE (ui::timelineReadout (clock, { engine::TimeBase::TempoLocked, 15360 }, 15360,
        project.sampleRate, moved) == ui::TimelineCoordinateStatus::Ok);
    CHECK (approx (first.lengthSeconds, std::log (1.5), 1e-12));
    CHECK (approx (moved.lengthSeconds, std::log (4.0 / 3.0), 1e-12));
    CHECK (moved.lengthSeconds < first.lengthSeconds);
    engine::Tick end = 0;
    REQUIRE (ui::timelineReadoutNativeEnd (clock, { engine::TimeBase::TempoLocked, 15360 }, 15360,
        project.sampleRate, std::log (7.0 / 6.0), true, end) == ui::TimelineCoordinateStatus::Ok);
    CHECK (end == 23040);   // bpm ratio 210/180; length is integrated at the moved start
}

TEST_CASE ("native readout ordering preserves exact Frames and refuses ambiguous timing",
           "[timeline][native-readout]")
{
    using namespace yesdaw;
    engine::Project project;
    project.sampleRate = { 48000.0 };
    project.tempoMap = { { 0, 120.0, engine::TempoCurve::Jump }, { 15360, 60.0, engine::TempoCurve::Jump } };
    project.meterMap = { { 0, 4, 4 } };
    const auto clock = ui::timelineReadoutClock (project);
    ui::TimelineReadout musical;
    REQUIRE (ui::timelineReadout (clock, { engine::TimeBase::TempoLocked, 15360 }, 15360,
        project.sampleRate, musical) == ui::TimelineCoordinateStatus::Ok);
    std::vector<ui::TimelineFramePoint> starts { musical.span.start, ui::TimelineFramePoint::exact (20000),
        ui::TimelineFramePoint::exact ((engine::Tick { 1 } << 53) + 1),
        ui::TimelineFramePoint::exact (engine::Tick { 1 } << 53) };
    std::vector<std::size_t> order;
    REQUIRE (ui::timelineReadoutOrder (starts, order) == ui::TimelineCoordinateStatus::Ok);
    CHECK (order == std::vector<std::size_t> { 1, 0, 3, 2 });
    const auto retained = order;
    auto uncertain = ui::TimelineFramePoint::exact (0);
    uncertain.offset = { 0.2, 0.8, false };
    auto overlap = ui::TimelineFramePoint::exact (0);
    overlap.offset = { 0.4, 0.6, false };
    starts = { uncertain, overlap };
    CHECK (ui::timelineReadoutOrder (starts, order) == ui::TimelineCoordinateStatus::Precision);
    CHECK (order == retained);
}

TEST_CASE ("fixed native readouts need no musical clock and reject overflowing endpoints",
           "[timeline][native-readout]")
{
    using namespace yesdaw;
    engine::Project project;
    project.sampleRate = { 48000.0 };
    project.tempoMap = { { 0, 1.0e-300, engine::TempoCurve::LinearRamp },
        { engine::Tick { 1 } << 48, std::nextafter (1.0e-300, INFINITY), engine::TempoCurve::Jump } };
    const auto clock = ui::timelineReadoutClock (project);
    REQUIRE (clock == nullptr);
    constexpr engine::Tick large = (engine::Tick { 1 } << 53) + 1;
    ui::TimelineReadout readout;
    REQUIRE (ui::timelineReadout (clock, { engine::TimeBase::SampleLocked, large }, 1,
        project.sampleRate, readout) == ui::TimelineCoordinateStatus::Ok);
    engine::Tick rounded = 0;
    REQUIRE (ui::timelineNativeFrameTarget (nullptr, engine::TimeBase::SampleLocked, readout.span.start, rounded)
             == ui::TimelineCoordinateStatus::Ok);
    CHECK (rounded == large);
    REQUIRE (ui::timelineNativeFrameTarget (nullptr, engine::TimeBase::SampleLocked, readout.duration, rounded)
             == ui::TimelineCoordinateStatus::Ok);
    CHECK (rounded == 1);
    CHECK_FALSE (ui::timelineReadoutCanEdit (clock, { engine::TimeBase::SampleLocked, large }, 1, project.sampleRate, readout));
    CHECK (ui::timelineReadout (clock, { engine::TimeBase::SampleLocked, INT64_MAX }, 1,
        project.sampleRate, readout) == ui::TimelineCoordinateStatus::Overflow);
    CHECK (ui::timelineReadout (clock, { engine::TimeBase::TempoLocked, 2 }, 1,
        project.sampleRate, readout) == ui::TimelineCoordinateStatus::Precision);
}

TEST_CASE ("seconds editability certifies Length independently of its exact endpoints",
           "[timeline][native-readout]")
{
    using namespace yesdaw;
    constexpr engine::Tick start = 3;
    constexpr engine::Tick end = engine::Tick { 1 } << 54;
    const engine::SampleRate rate { 32768.0 };
    ui::TimelineReadout readout;
    REQUIRE (ui::timelineReadout (nullptr, { engine::TimeBase::SampleLocked, start }, end - start,
        rate, readout) == ui::TimelineCoordinateStatus::Ok);
    engine::Tick durationEnd = 0;
    REQUIRE (ui::timelineReadoutNativeEnd (nullptr, { engine::TimeBase::SampleLocked, start }, end - start,
        rate, readout.lengthSeconds, true, durationEnd) == ui::TimelineCoordinateStatus::Ok);
    CHECK (durationEnd == end - 1);
    CHECK_FALSE (ui::timelineReadoutCanEdit (nullptr, { engine::TimeBase::SampleLocked, start }, end - start, rate, readout));
}

TEST_CASE ("a fully-visible clip maps to the expected pixel rect", "[timeline][geometry]")
{
    Viewport vp; vp.scrollSeconds = 0.0; vp.pixelsPerSecond = 100.0; vp.widthPixels = 1280.0;
    vp.laneHeightPixels = 64.0;
    const Clip clips[] = { { 7, 2, 1.0, 3.0 } };   // id 7, lane 2, starts 1 s, lasts 3 s

    ElementRect out[4];
    const int n = layoutVisible (clips, 1, vp, out, 4);

    REQUIRE (n == 1);
    REQUIRE (out[0].id == 7);
    REQUIRE (approx (out[0].x, 100.0));   // (1 - 0) * 100
    REQUIRE (approx (out[0].w, 300.0));   // 3 * 100
    REQUIRE (approx (out[0].y, 128.0));   // lane 2 * 64
    REQUIRE (approx (out[0].h, 64.0));
}

TEST_CASE ("virtualization returns only the clips intersecting the viewport", "[timeline][virtualization]")
{
    Viewport vp; vp.scrollSeconds = 100.0; vp.pixelsPerSecond = 100.0; vp.widthPixels = 1000.0;
    // window covers song time [100, 110) s.
    const Clip clips[] = {
        { 1, 0,   0.0, 1.0 },    // way before  -> skipped
        { 2, 0,  99.5, 1.0 },    // straddles left edge (99.5..100.5) -> visible
        { 3, 0, 105.0, 1.0 },    // inside -> visible
        { 4, 0, 109.5, 1.0 },    // straddles right edge (109.5..110.5) -> visible
        { 5, 0, 200.0, 1.0 },    // way after -> skipped
    };

    ElementRect out[8];
    const int n = layoutVisible (clips, 5, vp, out, 8);

    REQUIRE (n == 3);
    REQUIRE (out[0].id == 2);
    REQUIRE (out[1].id == 3);
    REQUIRE (out[2].id == 4);
    // every returned rect stays inside the viewport horizontally
    for (int i = 0; i < n; ++i)
    {
        REQUIRE (out[i].x >= 0.0f);
        REQUIRE (out[i].x + out[i].w <= (float) vp.widthPixels + 1e-3f);
    }
}

TEST_CASE ("clips straddling an edge are clipped, not dropped", "[timeline][clipping]")
{
    Viewport vp; vp.scrollSeconds = 100.0; vp.pixelsPerSecond = 100.0; vp.widthPixels = 1000.0;
    const Clip clips[] = {
        { 2, 0,  99.5, 1.0 },    // left edge: visible span 100.0..100.5 -> x=0, w=50
        { 4, 0, 109.5, 1.0 },    // right edge: visible span 109.5..110.0 -> x=950, w=50
    };
    ElementRect out[4];
    const int n = layoutVisible (clips, 2, vp, out, 4);

    REQUIRE (n == 2);
    REQUIRE (approx (out[0].x, 0.0));     // clamped to the left edge
    REQUIRE (approx (out[0].w, 50.0));    // half the clip is off-screen
    REQUIRE (approx (out[1].x, 950.0));
    REQUIRE (approx (out[1].w, 50.0));    // clamped to the right edge
}

TEST_CASE ("output capacity is respected", "[timeline][capacity]")
{
    Viewport vp; vp.scrollSeconds = 0.0; vp.pixelsPerSecond = 100.0; vp.widthPixels = 100000.0;
    std::vector<Clip> clips;
    for (int i = 0; i < 50; ++i) clips.push_back ({ i, 0, (double) i, 0.5 });

    ElementRect out[10];
    const int n = layoutVisible (clips.data(), (int) clips.size(), vp, out, 10);
    REQUIRE (n == 10);   // never writes past the buffer
}

TEST_CASE ("timeline hit-testing maps pixels back to the topmost visible clip", "[timeline][hit-test]")
{
    Viewport vp; vp.scrollSeconds = 10.0; vp.pixelsPerSecond = 100.0; vp.widthPixels = 640.0;
    vp.laneHeightPixels = 50.0;

    const Clip clips[] = {
        { 10, 0, 10.0, 2.0 },
        { 11, 0, 10.5, 2.0 },
        { 12, 1, 10.0, 2.0 },
    };

    const auto overlapped = hitTestVisibleClip (clips, 3, vp, 75.0, 20.0);
    REQUIRE (overlapped.hit);
    REQUIRE (overlapped.id == 11);
    REQUIRE (overlapped.lane == 0);
    REQUIRE (overlapped.clipIndex == 1);

    const auto secondLane = hitTestVisibleClip (clips, 3, vp, 75.0, 70.0);
    REQUIRE (secondLane.hit);
    REQUIRE (secondLane.id == 12);
    REQUIRE (secondLane.lane == 1);

    REQUIRE_FALSE (hitTestVisibleClip (clips, 3, vp, 630.0, 20.0).hit);
    REQUIRE_FALSE (hitTestVisibleClip (clips, 3, vp, -1.0, 20.0).hit);
}

TEST_CASE ("laying out a big project costs far less than one 60 fps frame", "[timeline][perf]")
{
    // 5000 clips across a long timeline; the viewport shows ~50 of them. The per-frame cost is the
    // virtualization scan + geometry, and it must be a small fraction of the 16.6 ms frame budget so
    // the GPU gets the rest.
    std::vector<Clip> clips;
    for (int i = 0; i < 5000; ++i) clips.push_back ({ i, i % 8, (double) i * 0.5, 0.4 });

    Viewport vp; vp.scrollSeconds = 1000.0; vp.pixelsPerSecond = 100.0; vp.widthPixels = 1920.0;
    std::vector<ElementRect> out (256);

    const int frames = 20000;
    int sink = 0;
    for (int w = 0; w < frames / 10; ++w)   // warm up
        sink += layoutVisible (clips.data(), (int) clips.size(), vp, out.data(), (int) out.size());

    const auto t0 = std::chrono::steady_clock::now();
    for (int f = 0; f < frames; ++f)
        sink += layoutVisible (clips.data(), (int) clips.size(), vp, out.data(), (int) out.size());
    const auto t1 = std::chrono::steady_clock::now();

    double totalSec = std::chrono::duration<double> (t1 - t0).count();
    if (totalSec <= 1e-9) totalSec = 1e-9;
    const double msPerFrame = totalSec / frames * 1000.0;

    INFO ("layout cost = " << msPerFrame << " ms/frame over 5000 clips (budget 16.6 ms); sink=" << sink);
    REQUIRE (msPerFrame < 1.0);   // comfortably under a frame, leaving the budget for the GPU
}

// G4.6 / ADR-0052: a row whose automation lanes are shown stacks them UNDER its clips. Its clips (painted
// and hit) keep the row's top part; the rows below move down by the lanes' height.
TEST_CASE ("A row's clips keep its top part when automation lanes are stacked under it", "[timeline][layout][automation-v2]")
{
    const double tops[3] = { 0.0, 72.0 + 120.0, 72.0 + 120.0 + 72.0 };   // row 0 shows 120 px of lanes
    const double heights[3] = { 72.0 + 120.0, 72.0, 72.0 };
    const double clipHeights[3] = { 72.0, 72.0, 72.0 };
    Viewport vp;
    vp.pixelsPerSecond = 100.0;
    vp.widthPixels = 1'000.0;
    vp.laneHeightPixels = 72.0;
    vp.laneTopPixels = tops;
    vp.laneHeightPixelsPerLane = heights;
    vp.clipHeightPixelsPerLane = clipHeights;

    const Clip clips[2] = { { 1, 0, 0.0, 2.0 }, { 2, 1, 0.0, 2.0 } };
    ElementRect out[2] {};
    REQUIRE (layoutVisible (clips, 2, vp, out, 2) == 2);
    REQUIRE (out[0].y == 0.0f);
    REQUIRE (out[0].h == 72.0f);            // the clip part, not the 192 px row
    REQUIRE (out[1].y == 192.0f);           // the next row starts under row 0's lanes
    REQUIRE (out[1].h == 72.0f);

    // A press in row 0's lanes is not on its clip; a press on the clip part is.
    REQUIRE_FALSE (hitTestVisibleClip (clips, 2, vp, 50.0, 100.0).hit);
    REQUIRE (hitTestVisibleClip (clips, 2, vp, 50.0, 40.0).hit);
    REQUIRE (hitTestVisibleClip (clips, 2, vp, 50.0, 200.0).hit);

    // Without the clip heights, the whole row is the clip (every caller that predates G4.6).
    vp.clipHeightPixelsPerLane = nullptr;
    REQUIRE (layoutVisible (clips, 2, vp, out, 2) == 2);
    REQUIRE (out[0].h == 192.0f);
}

// The layout pass, step 13: the musical grid follows a tempo change - 120 BPM for bars 1-2, 60 BPM from bar 3 (4/4,
// 48 kHz): bars start at 0, 2, 4, 8 and 12 s, the beats between them halve their pace at bar 3, and the Grid snap lands
// on a 60 BPM beat there (the old uniform grid, 0.5 s beats from the head tempo, landed between them).
TEST_CASE ("the musical grid's bars and beats and its snap follow a tempo change", "[timeline][musical-grid]")
{
    using namespace yesdaw;
    const engine::Tick barTicks = engine::kTicksPerQuarter * 4;
    const engine::TempoChange tempo[] = { { 0, 120.0 }, { barTicks * 2, 60.0 } };
    const engine::MeterChange meter[] = { { 0, 4, 4 } };
    engine::CompiledTempoMap compiled;
    REQUIRE (engine::CompiledTempoMap::build ({ tempo, 2 }, engine::SampleRate { 48000.0 }, compiled));
    const engine::MeterMapView meterMap { meter, 1 };

    // 0.4 s spacing: every beat (0.5 s at least) shows, no sixteenth (0.25 s at most) does.
    const std::vector<ui::MusicalGridLine> lines = ui::musicalGridLines (compiled, meterMap, 0.0, 48000.0 * 12.5, 19200.0);
    std::vector<double> bars, beats;
    for (const ui::MusicalGridLine& line : lines)
    {
        REQUIRE (line.level != ui::MusicalGridLevel::Sixteenth);
        (line.level == ui::MusicalGridLevel::Bar ? bars : beats).push_back (line.frame / 48000.0);
    }
    REQUIRE (bars.size() == 5u);
    const double expectedBars[] = { 0.0, 2.0, 4.0, 8.0, 12.0 };
    for (std::size_t i = 0; i < bars.size(); ++i)
        REQUIRE (approx (bars[i], expectedBars[i], 1.0e-6));
    // Three beats inside each bar: 0.5 s apart in bars 1-2, 1 s apart from bar 3.
    REQUIRE (beats.size() == 12u);
    REQUIRE (approx (beats[0], 0.5, 1.0e-6));
    REQUIRE (approx (beats[6], 5.0, 1.0e-6));
    REQUIRE (approx (beats[7], 6.0, 1.0e-6));

    // Lines closer than the spacing thin out: at 2 s spacing only bars show.
    for (const ui::MusicalGridLine& line : ui::musicalGridLines (compiled, meterMap, 0.0, 48000.0 * 12.5, 48000.0 * 2.0))
        REQUIRE (line.level == ui::MusicalGridLevel::Bar);

    // The snap: 8.6 s lies between the beats at 8 s and 9 s (60 BPM there); it lands on 9 s. In bar 1 (120 BPM), 1.3 s
    // lands on the beat at 1.5 s, and a sixteenth snap at 1.3 s lands on 1.25 s.
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, meterMap, 48000.0 * 8.6, ui::MusicalGridLevel::Beat, 0) == std::int64_t { 432000 });
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, meterMap, 48000.0 * 1.3, ui::MusicalGridLevel::Beat, 0) == std::int64_t { 72000 });
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, meterMap, 48000.0 * 1.3, ui::MusicalGridLevel::Sixteenth, 0) == std::int64_t { 60000 });
    // A bar snap at 6.5 s (bar 3 runs 4-8 s) lands on 8 s; halved once, on 6 s.
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, meterMap, 48000.0 * 6.5, ui::MusicalGridLevel::Bar, 0) == std::int64_t { 384000 });
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, meterMap, 48000.0 * 6.5, ui::MusicalGridLevel::Bar, 1) == std::int64_t { 288000 });
}

TEST_CASE ("fractional Meter grid boundaries retain their rational origins", "[timeline][musical-grid][checked-grid]")
{
    using namespace yesdaw;
    const engine::TempoChange tempo[] { { 0, 120.0 } };
    const engine::MeterChange meter[] { { 0, 7, 39 } };
    engine::CompiledTempoMap compiled;
    REQUIRE (engine::CompiledTempoMap::build ({ tempo, 1 }, engine::SampleRate { 48000.0 }, compiled));
    // A denominator beat is exactly 4/39 quarters: 96000/39 frames at this clock.
    auto lines = ui::musicalGridLines (compiled, { meter, 1 }, 0.0, 449.0 * 96000.0 / 39.0, 400.0);
    lines.erase (std::remove_if (lines.begin(), lines.end(), [] (const auto& line) {
        return line.level == ui::MusicalGridLevel::Sixteenth;
    }), lines.end());
    REQUIRE (lines.size() >= 449u);
    CHECK (approx (lines[1].frame, 96000.0 / 39.0, 1.0e-8));
    CHECK (approx (lines[7].frame, 7.0 * 96000.0 / 39.0, 1.0e-8));
    CHECK (approx (lines[448].frame, 448.0 * 96000.0 / 39.0, 1.0e-8));
    CHECK (lines[448].level == ui::MusicalGridLevel::Bar);
}

TEST_CASE ("musical Grid snap chooses the nearest mapped frame across a ramp", "[timeline][musical-grid][checked-grid]")
{
    using namespace yesdaw;
    const engine::TempoChange tempo[] { { 0, 60.0, engine::TempoCurve::LinearRamp },
                                        { 4 * engine::kTicksPerQuarter, 600.0 } };
    const engine::MeterChange meter[] { { 0, 4, 4 } };
    engine::CompiledTempoMap compiled;
    REQUIRE (engine::CompiledTempoMap::build ({ tempo, 2 }, engine::SampleRate { 48000.0 }, compiled));
    // BPM(q) = 60 + 135q; seconds(q) = (4/9) log(1 + 9q/4).
    // q=.4 is nearer Tick zero, but its mapped frame is nearer the beat at q=1.
    const double query = 48000.0 * (4.0 / 9.0) * std::log (1.9);
    const auto expected = static_cast<std::int64_t> (std::llround (48000.0 * (4.0 / 9.0) * std::log (3.25)));
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, { meter, 1 }, query, ui::MusicalGridLevel::Beat, 0) == expected);
}

TEST_CASE ("musical grid unites incommensurate beats and sixteenths", "[timeline][musical-grid][checked-grid]")
{
    using namespace yesdaw;
    const engine::TempoChange tempo[] { { 0, 120.0 } };
    const engine::MeterChange meter[] { { 0, 7, 7 } };
    engine::CompiledTempoMap compiled;
    REQUIRE (engine::CompiledTempoMap::build ({ tempo, 1 }, engine::SampleRate { 48000.0 }, compiled));
    const auto lines = ui::musicalGridLines (compiled, { meter, 1 }, 0.0, 96001.0, 1.0);
    REQUIRE (lines.size() == 23u);   // 2 bars + 6 denominator beats + 15 sixteenths; only endpoints coincide.
    for (int beat = 1; beat < 7; ++beat)
        REQUIRE (std::count_if (lines.begin(), lines.end(), [beat] (const auto& line) {
            return line.level == ui::MusicalGridLevel::Beat && approx (line.frame, beat * 96000.0 / 7.0, 1.0e-8);
        }) == 1);
    for (int sixteenth = 1; sixteenth < 16; ++sixteenth)
        REQUIRE (std::count_if (lines.begin(), lines.end(), [sixteenth] (const auto& line) {
            return line.level == ui::MusicalGridLevel::Sixteenth && approx (line.frame, sixteenth * 6000.0, 1.0e-8);
        }) == 1);
    for (std::size_t i = 1; i < lines.size(); ++i)
        REQUIRE (lines[i].frame > lines[i - 1].frame);
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, { meter, 1 }, 96000.0 / 7.0,
        ui::MusicalGridLevel::Sixteenth, 0) == std::int64_t { 13714 });
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, { meter, 1 }, 6000.0,
        ui::MusicalGridLevel::Sixteenth, 0) == std::int64_t { 6000 });
}

TEST_CASE ("checked musical grids truncate Meter segments and retain fractional readouts", "[timeline][musical-grid][checked-grid]")
{
    using namespace yesdaw;
    const engine::TempoChange tempo[] { { 0, 120.0 } };
    const engine::MeterChange meter[] { { 0, 7, 39 }, { 10000, 3, 7 } };
    engine::CompiledTempoMap compiled;
    engine::CompiledMeterMap meters;
    REQUIRE (engine::CompiledTempoMap::build ({ tempo, 1 }, engine::SampleRate { 48000.0 }, compiled));
    REQUIRE (engine::CompiledMeterMap::build ({ meter, 2 }, meters) == engine::TimeLookupStatus::Ok);
    const auto grid = ui::checkedMusicalGridLines (compiled, meters, { meter, 2 }, 0.0, 30000.0, 1.0);
    REQUIRE (grid.status == ui::MusicalGridStatus::Ok);
    for (int beat = 0; beat < 7; ++beat)
        REQUIRE (std::count_if (grid.lines.begin(), grid.lines.end(), [beat] (const auto& line) {
            return approx (line.frame, beat * 96000.0 / 39.0, 1.0e-8);
        }) == 1);
    REQUIRE (std::count_if (grid.lines.begin(), grid.lines.end(), [] (const auto& line) {
        return line.level == ui::MusicalGridLevel::Bar && line.bar == 2 && line.frame == 15625.0;
    }) == 1);
    REQUIRE (std::none_of (grid.lines.begin(), grid.lines.end(), [] (const auto& line) {
        return approx (line.frame, 7.0 * 96000.0 / 39.0, 1.0e-8);
    }));
    const engine::MeterChange tiny[] { { 0, 3, 7 }, { 26332, 4, 4 } };
    REQUIRE (engine::CompiledMeterMap::build ({ tiny, 2 }, meters) == engine::TimeLookupStatus::Ok);
    engine::BarBeat position;
    REQUIRE (ui::checkedMusicalBarBeatForFrame (compiled, meters, 41143.0, position) == ui::MusicalGridStatus::Ok);
    REQUIRE (position.bar == 2);   // Actual fractional bar 2 exists before the change at Frame 41143.75.
    REQUIRE (position.beat == 1);
    REQUIRE (ui::checkedMusicalBarBeatForFrame (compiled, meters, 41144.0, position) == ui::MusicalGridStatus::Ok);
    REQUIRE (position.bar == 3);
}

TEST_CASE ("checked musical grids bound dense views and refuse numeric failures", "[timeline][musical-grid][checked-grid]")
{
    using namespace yesdaw;
    const engine::TempoChange tempo[] { { 0, 120.0 } };
    engine::CompiledTempoMap compiled;
    REQUIRE (engine::CompiledTempoMap::build ({ tempo, 1 }, engine::SampleRate { 48000.0 }, compiled));
    const engine::MeterChange dense[] { { 0, 65535, 65535 } };
    engine::CompiledMeterMap meters;
    REQUIRE (engine::CompiledMeterMap::build ({ dense, 1 }, meters) == engine::TimeLookupStatus::Ok);
    const auto sparse = ui::checkedMusicalGridLines (compiled, meters, { dense, 1 }, 0.0, 192000.0, 100.0);
    REQUIRE (sparse.status == ui::MusicalGridStatus::Ok);
    REQUIRE (std::count_if (sparse.lines.begin(), sparse.lines.end(), [] (const auto& line) {
        return line.level == ui::MusicalGridLevel::Bar;
    }) == 3);   // 65535/65535 is exactly a 4-quarter bar, without clamping.
    REQUIRE (std::any_of (sparse.lines.begin(), sparse.lines.end(), [] (const auto& line) {
        return line.level == ui::MusicalGridLevel::Bar && line.frame == 96000.0;
    }));
    const auto capped = ui::checkedMusicalGridLines (compiled, meters, { dense, 1 }, 0.0, 96000.0, 0.1, 8);
    REQUIRE (capped.status == ui::MusicalGridStatus::ResourceLimit);
    REQUIRE (capped.lines.empty());
    const engine::MeterChange shortBars[] { { 0, 1, 65535 } };
    REQUIRE (engine::CompiledMeterMap::build ({ shortBars, 1 }, meters) == engine::TimeLookupStatus::Ok);
    const auto thinned = ui::checkedMusicalGridLines (compiled, meters, { shortBars, 1 }, 0.0, 1000.0, 100.0);
    REQUIRE (thinned.status == ui::MusicalGridStatus::Ok);
    REQUIRE (thinned.lines.size() >= 5u);
    REQUIRE (thinned.lines.size() <= 11u);
    for (const auto& line : thinned.lines)
        REQUIRE (approx (line.frame, static_cast<double> (line.bar - 1) * 96000.0 / 65535.0, 1.0e-8));
    const auto invalid = ui::checkedMusicalGridLines (compiled, meters, { shortBars, 1 }, 0.0,
        std::numeric_limits<double>::infinity(), 100.0);
    REQUIRE (invalid.status == ui::MusicalGridStatus::InvalidInput);
    REQUIRE (invalid.lines.empty());
    REQUIRE (ui::checkedMusicalGridLines (compiled, meters, { shortBars, 1 }, 0.0, 0x1p63, 100.0).status
        == ui::MusicalGridStatus::Overflow);
    const auto clipped = ui::checkedMusicalGridLines (compiled, meters, { shortBars, 1 }, -1000.0, 1000.0, 100.0);
    REQUIRE (clipped.status == ui::MusicalGridStatus::Ok);
    REQUIRE (clipped.lines.front().frame == 0.0);
    std::int64_t frame = 42;
    REQUIRE (ui::checkedSnapFrameToMusicalGrid (compiled, meters, { shortBars, 1 }, 0x1p63,
        ui::MusicalGridLevel::Beat, 0, frame) == ui::MusicalGridStatus::Overflow);
    REQUIRE (frame == 0);
    engine::CompiledTempoMap unavailable;
    REQUIRE (ui::checkedSnapFrameToMusicalGrid (unavailable, meters, { shortBars, 1 }, 100.0,
        ui::MusicalGridLevel::Beat, 0, frame) == ui::MusicalGridStatus::InvalidMap);
    engine::BarBeat position;
    REQUIRE (ui::checkedMusicalBarBeatForFrame (unavailable, meters, 100.0, position) == ui::MusicalGridStatus::InvalidMap);
    REQUIRE (position.bar == 0);
    const engine::MeterChange longBars[] { { 0, 65535, 1 } };
    REQUIRE (engine::CompiledMeterMap::build ({ longBars, 1 }, meters) == engine::TimeLookupStatus::Ok);
    constexpr double secondBar = 65535.0 * 96000.0;
    const auto longView = ui::checkedMusicalGridLines (compiled, meters, { longBars, 1 },
        secondBar - 10000.0, secondBar + 10000.0, 1000.0);
    REQUIRE (longView.status == ui::MusicalGridStatus::Ok);
    REQUIRE (std::count_if (longView.lines.begin(), longView.lines.end(), [] (const auto& line) {
        return line.level == ui::MusicalGridLevel::Bar && line.bar == 2 && line.frame == secondBar;
    }) == 1);
}

TEST_CASE ("7/39 paints and snaps both denominator beats and sixteenths", "[timeline][musical-grid][checked-grid]")
{
    using namespace yesdaw;
    const engine::TempoChange tempo[] { { 0, 120.0 } };
    const engine::MeterChange meter[] { { 0, 7, 39 } };
    engine::CompiledTempoMap compiled;
    REQUIRE (engine::CompiledTempoMap::build ({ tempo, 1 }, engine::SampleRate { 48000.0 }, compiled));
    const auto lines = ui::musicalGridLines (compiled, { meter, 1 }, 0.0, 17000.0, 100.0);
    REQUIRE (std::any_of (lines.begin(), lines.end(), [] (const auto& line) {
        return line.level == ui::MusicalGridLevel::Sixteenth && line.frame == 6000.0;
    }));
    REQUIRE (std::any_of (lines.begin(), lines.end(), [] (const auto& line) {
        return line.level == ui::MusicalGridLevel::Beat && approx (line.frame, 96000.0 / 39.0, 1.0e-8);
    }));
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, { meter, 1 }, 6000.0,
        ui::MusicalGridLevel::Sixteenth, 0) == std::int64_t { 6000 });
    REQUIRE (ui::snapFrameToMusicalGrid (compiled, { meter, 1 }, 96000.0 / 39.0,
        ui::MusicalGridLevel::Sixteenth, 0) == std::int64_t { 2462 });
    const auto spaced = ui::musicalGridLines (compiled, { meter, 1 }, 0.0, 17000.0, 400.0);
    for (std::size_t i = 1; i < spaced.size(); ++i)
        REQUIRE (spaced[i].frame - spaced[i - 1].frame >= 400.0);
}

#include "ui/PianoRollGrid.h"

namespace {
yesdaw::ui::PianoRollGrid nativePianoGrid (yesdaw::engine::TimeBase base, yesdaw::engine::Tick origin,
                                         const std::vector<yesdaw::engine::TempoChange>& tempo,
                                         const std::vector<yesdaw::engine::MeterChange>& meter)
{
    auto clock = std::make_shared<yesdaw::engine::MusicalClickMap>();
    REQUIRE (yesdaw::engine::MusicalClickMap::build ({ tempo.data(), tempo.size() }, { meter.data(), meter.size() },
             { 48000.0 }, *clock) == yesdaw::engine::TimeLookupStatus::Ok);
    return { clock, base, origin, 1'000'000, meter };
}
}

TEST_CASE ("native piano grid follows Clip placement Tempo jumps and Meter resets",
           "[timeline][native-piano-grid]")
{
    using namespace yesdaw;
    const std::vector<engine::TempoChange> tempo { { 0, 120.0 }, { 30720, 60.0 } };
    const std::vector<engine::MeterChange> meter { { 0, 4, 4 }, { 38400, 3, 8 } };
    for (const bool musical : { false, true })
    {
        const auto grid = nativePianoGrid (musical ? engine::TimeBase::TempoLocked : engine::TimeBase::SampleLocked,
                                           musical ? 23040 : 36000, tempo, meter);
        const std::vector<engine::Tick> expected = musical ? std::vector<engine::Tick> { 7680, 15360, 23040, 30720, 38400 }
            : std::vector<engine::Tick> { 12000, 36000, 60000, 84000, 108000 };
        const auto lines = grid.lines (0, musical ? 40000 : 110000, 1.0, {}, true);
        REQUIRE (lines.size() == expected.size());
        for (std::size_t i = 0; i < lines.size(); ++i) CHECK (lines[i].tick == expected[i]);
        CHECK (lines[1].level == ui::MusicalGridLevel::Bar);
        CHECK (lines[4].level == ui::MusicalGridLevel::Bar);
    }
}

TEST_CASE ("native piano grid uses Frame distance on ramps and Meter origins across bars",
           "[timeline][native-piano-grid]")
{
    using namespace yesdaw;
    const auto ramp = nativePianoGrid (engine::TimeBase::SampleLocked, 10000,
        { { 0, 120.0, engine::TempoCurve::LinearRamp }, { 61440, 240.0 } }, { { 0, 4, 4 } });
    engine::Tick target = 0;
    REQUIRE (ramp.snapped (20300, {}, false, target));
    CHECK (target == 28925); // 30300 Frames is nearer F(2) than F(1), despite being before 1.5 quarters.
    const auto rampLines = ramp.lines (0, 60000, 1.0, {}, true);
    const std::vector<engine::Tick> expected { 11422, 28925, 43723, 56542 };
    REQUIRE (rampLines.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) CHECK (rampLines[i].tick == expected[i]);
    const auto fractional = nativePianoGrid (engine::TimeBase::TempoLocked, 0, { { 0, 120.0 } }, { { 0, 7, 39 } });
    REQUIRE (fractional.snapped (12000, { ui::MusicalGridLevel::Sixteenth, 0 }, false, target));
    CHECK (target == 11520);
    const auto odd = nativePianoGrid (engine::TimeBase::TempoLocked, 0, { { 0, 120.0 } }, { { 0, 3, 4 } });
    REQUIRE (odd.snapped (61440, {}, false, target, 50));
    CHECK (target == 61440);
    REQUIRE (odd.snapped (46080, {}, false, target, 50));
    CHECK (target == 53760); // odd global Beat slot 3 swings across the bar line.
    const auto reset = nativePianoGrid (engine::TimeBase::TempoLocked, 0,
        { { 0, 120.0 } }, { { 0, 4, 4 }, { 20000, 3, 8 } });
    REQUIRE (reset.snapped (19800, {}, false, target));
    CHECK (target == 20000);
    REQUIRE (reset.snapped (20000, {}, false, target, 50));
    CHECK (target == 20000);
}

TEST_CASE ("native piano grids retain sub-Frame musical cells and bound dense viewport work",
           "[timeline][native-piano-grid]")
{
    using namespace yesdaw;
    const auto fast = nativePianoGrid (engine::TimeBase::TempoLocked, 0, { { 0, 100000000.0 } }, { { 0, 4, 4 } });
    engine::Tick target = 0;
    REQUIRE (fast.snapped (2000, { ui::MusicalGridLevel::Sixteenth, 0 }, false, target));
    CHECK (target == 3840);
    const auto dense = fast.lines (0, 1'000'000'000'000LL, 1'000'000'000.0, { ui::MusicalGridLevel::Sixteenth, 0 }, true);
    REQUIRE_FALSE (dense.empty());
    CHECK (dense.size() <= 4096u);
    const auto fractional = nativePianoGrid (engine::TimeBase::TempoLocked, 0, { { 0, 120.0 } }, { { 0, 7, 39 } });
    engine::RationalTick cursor;
    REQUIRE (fractional.advance (0, {}, true, target, &cursor));
    CHECK (target == 1575);
    REQUIRE (fractional.advance (target, {}, true, target, &cursor));
    CHECK (target == 3151);
    REQUIRE (fractional.advance (0, { ui::MusicalGridLevel::Beat, 3 }, true, target));
    CHECK (target == 197);
}

TEST_CASE ("native piano percent rounding retains signed rounding headroom",
           "[timeline][native-piano-grid][native-piano-percent-boundary]")
{
    constexpr yesdaw::engine::Tick boundary = INT64_MAX / 100;
    CHECK (yesdaw::ui::pianoQuantizeTowards (0, boundary, 100) == boundary);
    CHECK (yesdaw::ui::pianoQuantizeTowards (boundary, 0, 100) == 0);
}

TEST_CASE ("native piano grid retains exact long-range Tick phase with tiny rational Meter cells",
           "[timeline][native-piano-grid][native-piano-long-range]")
{
    using namespace yesdaw;
    const auto grid = nativePianoGrid (engine::TimeBase::TempoLocked, 0, { { 0, 120.0 } }, { { 0, 7, 65535 } });
    const engine::Tick at = (engine::Tick { 1 } << 62) + 123;
    engine::Tick target = 0;
    REQUIRE (grid.snapped (at, {}, false, target));
    CHECK (target == at); // 61440/65535 < one Tick; the nearest cell rounds back to this exact integer.
    REQUIRE (grid.snapped (INT64_MAX - 100, { ui::MusicalGridLevel::Beat, 1 }, false, target));
    CHECK (target == INT64_MAX - 100); // half-beats stay below one Tick too, beyond uint64 global slot indexing.
}

TEST_CASE ("nonlegacy piano quantize keeps final endpoint ties consistent with native snap",
           "[timeline][native-piano-grid][native-piano-final-tie]")
{
    using namespace yesdaw;
    CHECK (ui::pianoQuantizeTowards (501, 0, 50) == 251);
    const auto grid = nativePianoGrid (engine::TimeBase::TempoLocked, 0, { { 0, 120.0 } }, { { 0, 7, 32768 } });
    engine::Tick snapped = 0, quantized = 0;
    REQUIRE (grid.snapped (8, {}, false, snapped)); // nearest rational boundary is 7.5 Ticks
    REQUIRE (grid.quantized (8, {}, 0, 100, quantized));
    CHECK (snapped == 8);
    CHECK (quantized == snapped);
    const auto frames = nativePianoGrid (engine::TimeBase::SampleLocked, 0, { { 0, 1920000.0 } }, { { 0, 4, 4 } });
    REQUIRE (frames.snapped (2, {}, false, snapped)); // nearest musical boundary is Frame 1.5
    REQUIRE (frames.quantized (2, {}, 0, 100, quantized));
    CHECK (snapped == 2);
    CHECK (quantized == snapped);
}
