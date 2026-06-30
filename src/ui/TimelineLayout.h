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
};

// The visible window onto the timeline.
struct Viewport
{
    double scrollSeconds    = 0.0;   // song time at the left edge
    double pixelsPerSecond  = 100.0; // horizontal zoom
    double widthPixels      = 1280.0;
    double laneHeightPixels = 64.0;  // each lane's row height (lane 0 at y=0)
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

        // Virtualize: skip anything entirely left of, or right of, the window.
        if (clipEnd <= leftSec || clipStart >= rightSec)
            continue;

        // Unclipped pixel span, then clamp to the viewport edges.
        double xPx = (clipStart - leftSec) * pps;
        double wPx = c.lengthSeconds * pps;
        if (xPx < 0.0)            { wPx += xPx; xPx = 0.0; }              // straddles the left edge
        if (xPx + wPx > vp.widthPixels) wPx = vp.widthPixels - xPx;       // straddles the right edge
        wPx = std::max (wPx, 0.0);

        ElementRect& r = out[count++];
        r.id = c.id;
        r.x  = (float) xPx;
        r.y  = (float) (c.lane * vp.laneHeightPixels);
        r.w  = (float) wPx;
        r.h  = (float) vp.laneHeightPixels;
    }
    return count;
}

// Hit-test a viewport-local pixel against visible clips. Later input clips win, matching the paint order
// where later rectangles are drawn over earlier ones.
inline TimelineHitTestResult hitTestVisibleClip (const Clip* clips, int n, const Viewport& vp,
                                                 double xPixels, double yPixels)
{
    if (clips == nullptr
        || n <= 0
        || vp.pixelsPerSecond <= 0.0
        || vp.widthPixels <= 0.0
        || vp.laneHeightPixels <= 0.0
        || xPixels < 0.0
        || yPixels < 0.0
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
        if (c.lengthSeconds <= 0.0 || c.lane < 0)
            continue;

        const double clipStart = c.startSeconds;
        const double clipEnd = c.startSeconds + c.lengthSeconds;
        if (clipEnd <= leftSec || clipStart >= rightSec)
            continue;

        double xPx = (clipStart - leftSec) * pps;
        double wPx = c.lengthSeconds * pps;
        if (xPx < 0.0) { wPx += xPx; xPx = 0.0; }
        if (xPx + wPx > vp.widthPixels) wPx = vp.widthPixels - xPx;
        wPx = std::max (wPx, 0.0);

        const double yPx = static_cast<double> (c.lane) * vp.laneHeightPixels;
        if (xPixels >= xPx
            && xPixels < xPx + wPx
            && yPixels >= yPx
            && yPixels < yPx + vp.laneHeightPixels)
        {
            return { true, c.id, c.lane, i };
        }
    }

    return {};
}

} // namespace yesdaw::ui
