// YES DAW — H0 spike #2: pure timeline layout / viewport virtualization.
//
// THROWAWAY spike code (H0), not the real UI. Its job is to de-risk "can we draw 100+ elements at
// 60 fps?" by isolating the part that costs CPU every frame — deciding which clips are on-screen and
// where — into a PURE function with no JUCE / GPU / display dependency. That lets CI golden-check the
// geometry and benchmark the per-frame cost headlessly; the actual GPU rendering smoothness is proven
// by the real-hardware soak, and the visual feel is the one human-eyeballed bit.
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

} // namespace yesdaw::ui
