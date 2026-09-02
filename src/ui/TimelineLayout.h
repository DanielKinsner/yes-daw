// YES DAW - production timeline layout and hit-testing.
//
// H12 made this geometry load-bearing for real UI input: paint, virtualization, and pointer hit-testing
// share the same pure pixel <-> clip mapping. The caller owns output buffers so dense timeline rendering
// stays predictable and allocation-free.
//
// Virtualization is the whole point: a timeline may hold thousands of clips, but only the handful
// intersecting the viewport are ever laid out, so the per-frame cost tracks what's VISIBLE, not the
// project size. Allocation-free: the caller owns the output buffer.

#pragma once

#include "ui/UiThemeLayout.h"

#include <algorithm>

namespace yesdaw::ui {

// A clip placed on the timeline: where it starts and how long it is, in seconds, plus a stable id and
// the lane (track row) it lives on.
struct Clip
{
    int    id;
    int    lane;
    double startSeconds;
    double lengthSeconds;
    const char* name = nullptr;
};

// The visible window onto the timeline.
struct Viewport
{
    double scrollSeconds    = UiThemeLayout::timelineLayoutZeroFloor;                  // song time at the left edge
    double pixelsPerSecond  = UiThemeLayout::timelineLayoutDefaultPixelsPerSecond;     // horizontal zoom
    double widthPixels      = UiThemeLayout::timelineLayoutDefaultWidthPixels;
    double laneHeightPixels = UiThemeLayout::timelineLayoutDefaultLaneHeightPixels;    // each lane's row height (lane 0 at y=0)
    double laneScrollPixels = UiThemeLayout::timelineLayoutZeroFloor;                  // vertical scroll: pixels of lane rows above the window (E5)
    // N6: optional PER-LANE geometry (persisted Track height). laneTopPixels[lane] is that lane's
    // cumulative top offset BEFORE laneScrollPixels is subtracted; laneHeightPixelsPerLane[lane]
    // is its own height. Both null (the default) keeps every lane on the uniform
    // `lane * laneHeightPixels` law byte-for-byte — every caller that predates per-track height
    // sees IDENTICAL behavior. Caller-owned, sized to at least (max lane index + 1), same
    // ownership contract as `out`/`outCapacity` below.
    const double* laneTopPixels = nullptr;
    const double* laneHeightPixelsPerLane = nullptr;
};

// One on-screen rectangle to draw, in pixels, clipped to the viewport's left/right edges.
struct ElementRect
{
    int   id;
    float x, y, w, h;
};

struct TimelineHitTestResult
{
    bool hit = false;
    int id = -1;
    int lane = -1;
    int clipIndex = -1;
};

// Lay out the clips that are visible in `vp` into `out` (capacity `outCapacity`); returns how many
// were written. Clips fully outside the horizontal window are skipped (virtualization); clips that
// straddle an edge are clipped to it so x/w never leave [0, widthPixels]. Stable: input order is
// preserved, so a golden compare is deterministic.
inline int layoutVisible (const Clip* clips, int n, const Viewport& vp,
                          ElementRect* out, int outCapacity)
{
    const double pps        = vp.pixelsPerSecond;
    const double leftSec     = vp.scrollSeconds;
    const double rightSec    = vp.scrollSeconds + vp.widthPixels / pps;

    int count = 0;
    for (int i = 0; i < n && count < outCapacity; ++i)
    {
        const Clip& c = clips[i];
        const double clipStart = c.startSeconds;
        const double clipEnd   = c.startSeconds + c.lengthSeconds;

        // Virtualize: skip anything entirely left of, or right of, the window. Vertically
        // scrolled-out clips still lay out (their rects clamp to empty at paint time) so the
        // visible-clip census keeps its historical meaning for the frame verdict policy.
        if (clipEnd <= leftSec || clipStart >= rightSec)
            continue;

        // N6: a per-lane geometry array (persisted Track height) overrides the uniform law for
        // ITS lane only; absent, every lane behaves exactly as before.
        const double laneTop = vp.laneTopPixels != nullptr
            ? vp.laneTopPixels[c.lane]
            : c.lane * vp.laneHeightPixels;
        const double laneH = vp.laneHeightPixelsPerLane != nullptr
            ? vp.laneHeightPixelsPerLane[c.lane]
            : vp.laneHeightPixels;
        const double laneY = laneTop - vp.laneScrollPixels;

        // Unclipped pixel span, then clamp to the viewport edges.
        double xPx = (clipStart - leftSec) * pps;
        double wPx = c.lengthSeconds * pps;
        if (xPx < UiThemeLayout::timelineLayoutZeroFloor)
        {
            wPx += xPx;
            xPx = UiThemeLayout::timelineLayoutZeroFloor;              // straddles the left edge
        }
        if (xPx + wPx > vp.widthPixels) wPx = vp.widthPixels - xPx;       // straddles the right edge
        wPx = std::max (wPx, UiThemeLayout::timelineLayoutZeroFloor);

        ElementRect& r = out[count++];
        r.id = c.id;
        r.x  = (float) xPx;
        r.y  = (float) laneY;
        r.w  = (float) wPx;
        r.h  = (float) laneH;
    }
    return count;
}

// Hit-test a viewport-local pixel against visible clips. Later input clips win, matching the paint order
// where later rectangles are drawn over earlier ones.
// G2.3: a clip's pixel rect in clip-area coordinates — the SAME arithmetic hitTestVisibleClip
// uses, so a drag ghost lands exactly where the hit test says the clip is.
struct ClipPixelRect
{
    double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
};

[[nodiscard]] inline ClipPixelRect visibleClipPixelRect (const Clip& c, const Viewport& vp) noexcept
{
    const double pps = vp.pixelsPerSecond;
    const double laneTop = vp.laneTopPixels != nullptr
        ? vp.laneTopPixels[c.lane]
        : static_cast<double> (c.lane) * vp.laneHeightPixels;
    const double laneH = vp.laneHeightPixelsPerLane != nullptr
        ? vp.laneHeightPixelsPerLane[c.lane]
        : vp.laneHeightPixels;
    return { (c.startSeconds - vp.scrollSeconds) * pps, laneTop - vp.laneScrollPixels,
             c.lengthSeconds * pps, laneH };
}

[[nodiscard]] inline double laneTopPixelsFor (int lane, const Viewport& vp) noexcept
{
    const double laneTop = vp.laneTopPixels != nullptr
        ? vp.laneTopPixels[lane]
        : static_cast<double> (lane) * vp.laneHeightPixels;
    return laneTop - vp.laneScrollPixels;
}

inline TimelineHitTestResult hitTestVisibleClip (const Clip* clips, int n, const Viewport& vp,
                                                 double xPixels, double yPixels)
{
    if (clips == nullptr
        || n <= 0
        || vp.pixelsPerSecond <= UiThemeLayout::timelineLayoutZeroFloor
        || vp.widthPixels <= UiThemeLayout::timelineLayoutZeroFloor
        || vp.laneHeightPixels <= UiThemeLayout::timelineLayoutZeroFloor
        || xPixels < UiThemeLayout::timelineLayoutZeroFloor
        || yPixels < UiThemeLayout::timelineLayoutZeroFloor
        || xPixels >= vp.widthPixels)
    {
        return {};
    }

    const double pps = vp.pixelsPerSecond;
    const double leftSec = vp.scrollSeconds;
    const double rightSec = vp.scrollSeconds + vp.widthPixels / pps;

    for (int i = n - 1; i >= 0; --i)
    {
        const Clip& c = clips[i];
        if (c.lengthSeconds <= UiThemeLayout::timelineLayoutZeroFloor || c.lane < 0)
            continue;

        const double clipStart = c.startSeconds;
        const double clipEnd = c.startSeconds + c.lengthSeconds;
        if (clipEnd <= leftSec || clipStart >= rightSec)
            continue;

        double xPx = (clipStart - leftSec) * pps;
        double wPx = c.lengthSeconds * pps;
        if (xPx < UiThemeLayout::timelineLayoutZeroFloor)
        {
            wPx += xPx;
            xPx = UiThemeLayout::timelineLayoutZeroFloor;
        }
        if (xPx + wPx > vp.widthPixels) wPx = vp.widthPixels - xPx;
        wPx = std::max (wPx, UiThemeLayout::timelineLayoutZeroFloor);

        // N6: mirrors layoutVisible's per-lane override exactly, so a hit-test can never drift
        // from the painted rect.
        const double laneTop = vp.laneTopPixels != nullptr
            ? vp.laneTopPixels[c.lane]
            : static_cast<double> (c.lane) * vp.laneHeightPixels;
        const double laneH = vp.laneHeightPixelsPerLane != nullptr
            ? vp.laneHeightPixelsPerLane[c.lane]
            : vp.laneHeightPixels;
        const double yPx = laneTop - vp.laneScrollPixels;
        if (xPixels >= xPx
            && xPixels < xPx + wPx
            && yPixels >= yPx
            && yPixels < yPx + laneH)
        {
            return { true, c.id, c.lane, i };
        }
    }

    return {};
}

} // namespace yesdaw::ui
