// YES DAW — headless checks for the H0 spike #2 timeline layout (viewport virtualization).
//
// Pure C++ + Catch2, no JUCE/GPU/display — proves the per-frame CPU work is correct AND cheap, so the
// "can we afford 100+ elements at 60 fps?" risk is answered mechanically. The GPU rendering smoothness
// is the real-hardware soak's job; this is the part that can regress silently and must be gated.

#include "ui/TimelineLayout.h"

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
