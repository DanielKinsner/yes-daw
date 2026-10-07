# 0064. Layout density and scaling: eight whole lanes at 1080p, track headers level with their lanes, a window that fits every display of the matrix, and the matrix rendered at its scales

- **Status:** Accepted (2026-10-08, after two separate agent critic passes)
- **Date:** 2026-10-08
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G6.2 ("Layout and scaling. Preserve reference density and the 1280×720 operating floor across
  the stated window/scaling matrix. Gate: geometry, reachable controls and screenshot rubric at every combination; each
  discovered clipping/overlap defect gains a mechanical regression assertion."), the G6 exit (rubric at 100 %, 125 %,
  150 %, 200 % Windows scaling and 1280×720, 1920×1080, 2560×1440), §3.4 (default track height 72 px, "9 tracks visible
  in a 1080p window with a 300 px dock", "Minimum operable window 1280×720 — Everything reachable; dock collapsible"),
  §7.4 (rubric line 2: ≥ 8 tracks at 1080p with the dock open) and §8.2 (the macOS `YesDawTimelineGpuCheck`
  sustained-frame exception, "re-evaluate at G6.2 on a measured runner/machine baseline … no automatic renewal") in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md); ADR-0046 (the
  feel-first shell); ADR-0063 (the type scale; the drop-whole law on the timeline toolbar row).

## Context

Measured at a 1920×1080 window with twelve tracks and the dock at its 300 px default: the header ends at 88, the arrange
panel starts at 98 (`shellPanelVerticalInset` = 10 above and below), the canvas's 1 px outer inset and its 36 px tool row
end at 135, the ruler at 199; seven lanes are whole and the eighth gets 56 of its 72 px. The plan's arithmetic (1080 −
28 − 60 − 64 − 300 = 628 px, 8.7 lanes) did not count the tool row or the insets; the density gates were lowered to
"≥ 7" (`tests/ui_input_tests.cpp:16875-16877`, `tests/ui_screenshot_tests.cpp:1444-1451`).

Found while measuring: **the track headers are 15 px above their lanes.** The rail reserves `trackListHeaderHeight` = 86
above its first row; the timeline puts its first lane 1 + 36 + 64 = 101 px below the same panel top (the ruler grew to
64 px in G0.7 cp3 and the rail was not moved). Both use the same fixed row law, so every row is off by the same 15 px.
No gate compares them. And the two panels clamp the vertical scroll separately over row areas 11 px apart (the rail's
runs to its panel's bottom, the lanes stop above the timeline's scroll bar; `scrollTrackRowsBy` takes the larger of
the two maxima), so at some heights a scroll to the bottom leaves the headers a whole row off.

The tool row's controls take their row from `automationLaneToggleTopInset` = 8 and `automationLaneToggleHeight` = 26
(`src/ui/UiTheme.h:798-800`): the zoom trio and slider, the snap chooser, the status choosers and the view cluster all
derive from it. The painted tool cells are the row reduced by `timelineCanvasToolbarInsetY` = 6.

Scaling: JUCE lays the shell out in logical pixels; a display's scale changes only the raster. A display at a scale is
therefore a logical size plus a render scale. Nothing renders or judges the shell at a scale today. The one cache that
knows the scale (`SoftwareCanvasCache`, the timeline on Windows) rasterises at the context's physical scale.

The window: `windowMinWidth` × `windowMinHeight` = 1152 × 720 (E27: "the judged laptop size", a 1440×900 display at
125 %). That size is the **client** area; a maximized window's client area is the display's logical size minus the
taskbar (48 physical px by default on Windows 11) and the title bar (about 32 logical px). On a 1920×1080 display at
150 % that is about 1280 × 656, on a 1440×900 display at 125 % about 1152 × 650: below the 720 px minimum, so the
window cannot fit and its bottom (the dock's faders) sits under the taskbar.

The macOS GPU frame exception is due for its G6.2 re-evaluation.

What is hard to reverse: the density target, the window minimum and the matrix become gates every later surface keeps.

## Options considered

1. **Compact the chrome to the plan's density; level the rail; size the window minimum from the matrix's displays;
   render the matrix at its scales (chosen).**
2. **Fold the timeline's tool row into the header** (the parked idea). Recovers the most height, but the header is full
   at 1280 (file, transport, counter, tempo, meter, solo, master) and every reference DAW keeps a local tool row above
   its tracks (Logic's tracks-area bar is about 28 px). Rejected.
3. **Shrink the dock's default or the lanes** (284 px dock, 64 px lanes). Meets the count by leaving the plan's 300 px
   dock and 72 px lanes. Rejected.
4. **Raise the window minimum to 1280 × 720** (the plan's §3.4 number read as a minimum size). Then the window fits none
   of the 150 %/125 % laptop displays above, and 1152-wide displays not at all. §3.4's row says the shell must be
   operable with everything reachable at 1280×720; it does not forbid smaller windows. Rejected in favour of (5).
5. **Keep 1280×720 as the size that must be fully operable, and let the window go as small as the matrix's displays
   need, with the same reachability law there (chosen, part of 1).** Stricter than §3.4, never looser: every size the
   window allows, 1280×720 included, keeps everything reachable.

## Decision

### Checkpoint 1 — eight whole lanes at 1080p, the rail level with its lanes

Tokens (`src/ui/UiTheme.h`):

| Token | Was | Now | Why |
|---|---|---|---|
| `timelineCanvasToolbarHeight` | 36 | **28** | the reference tool-row height |
| `timelineCanvasToolbarInsetY` | 6 (`Space::sm`) | **2** (`Space::xxs`) | painted tool cells stay 24 px |
| `automationLaneToggleTopInset` | 8 | **3** | row controls centred: 1 outer + 2 |
| `automationLaneToggleHeight` | 26 | **24** | every row control fits the row |
| `shellPanelVerticalInset` | 10 | **4** | rail, timeline, inspector move together |
| `trackListHeaderHeight` | 86 | **outer inset + tool row + ruler (93)** | derived, so the rail cannot drift again |
| `trackListFooterHeight` (new) | — | **scroll bar + outer inset (11)** | the rows end where the lanes end |

The rail's every row law (paint, hit-test, resize handle, scroll maximum) reads one `rowArea()`: its bounds less that
header and footer, the same height as the canvas's clip area, so both panels show and clamp the same rows.

Arithmetic at 1920×1080, dock 300: the timeline panel runs 92–776; after its 10 px horizontal scroll bar the canvas
runs 92–766 and its content (1 px outer inset) 93–765: tool row to 121, ruler to 185, eight lanes to 761. Eight whole
lanes, 4 px spare.

Gates (`[layout]`, plus the existing tests named):

- **Density:** `[g0][rubric-shots]` (`ui_screenshot_tests.cpp:1449`) and the first-minute density test
  (`ui_input_tests.cpp:16877`) go from `>= 7` to `>= 8` at 1920×1080, comments rewritten with the arithmetic above.
- **Row containment:** every control laid out on the timeline toolbar row (each zoom control, the slider, the snap,
  snap-mode, edit-mode and nudge choosers, the four view toggles, the status line) lies inside the canvas's
  `toolbarArea`, top and bottom gaps equal within 1 px; the painted tool cells too. At each matrix size.
- **Rail level:** for every on-screen track, `rail.row.N`'s top equals `lane.N`'s top, and its height equals the lane's
  (within the separator), at each size, at the top and after a scroll to the bottom; the rail's scroll maximum equals
  the canvas's.
- Existing pins that move with the tokens (`[header-flex]`, `[shell-sizes]`, the tool-cell and chooser-fit tests) read
  the tokens, not literals; any literal found is converted.
- The agent judges the three rubric shots; each clipping or overlap found gains an assertion.

### Checkpoint 2 — the matrix, the window minimum, reachability

**Two sets of cells, both logical:**

- **Plan cells** (the G6 exit's literal matrix): windows of 1280×720, 1920×1080 and 2560×1440, each rendered at 100,
  125, 150 and 200 %: twelve renders.
- **Display cells** (what a maximized window gets on those displays at Windows' scales): logical client =
  ⌊W/s⌋ × ⌊(H − 48)/s − 32⌋. 1280×720@100 → 1280×640; 1920×1080@100/125/150 → 1920×1000, 1536×793, 1280×656;
  2560×1440@100/125/150/200 → 2560×1360, 2048×1081, 1706×896, 1280×664. Each rendered at its own scale. Combinations
  whose client would fall below the window minimum (1280×720 above 100 %, 1920×1080 at 200 %) are outside
  the matrix: no window fits there, and §3.4's floor puts them out of scope.

**The window minimum becomes 1152 × 640** (height was 720), the smallest display cell, so a maximized window fits every
display in the matrix. 1280×720 stays the plan's operable floor; the same reachability law holds at every size the
window allows.

**How a cell is rendered:** the shell is laid out at the logical size and painted with `paintEntireComponent` under a
`scale` transform into a software image of ⌈logical × scale⌉ pixels — the same component paint code and the same
`SoftwareCanvasCache` path (it reads the context's physical scale) the window runs at that scale. The on-screen
Direct2D renderer (JUCE 8) is covered by the real-app drive, which stays pending until the desktop is free.

Gates at every cell (`[layout][scaling]`):

- **Geometry** (logical): the header's sections disjoint and inside the window; the toolbar row without overlap
  (ADR-0063) and inside its row; the inspector's whole-section law; the dock inside the window; the rail level with its
  lanes.
- **Reachable controls:** every visible shell control is inside the window and hit-tests to itself at its centre
  (`getComponentAt` returns it or a descendant, never a sibling over it). Every control the drop-whole laws can drop
  has a menu item for its action, checked against the shell's menu model; any missing item is added. An inspector
  section dropped at a size is shown once the dock is collapsed (§3.4's "dock collapsible"); the `[shell-sizes]` floor
  case asserts exactly that at 1152×640. The look-up is the shell's own `getComponentAt` from its root, so a control
  covered by a component from another branch fails rather than being skipped.
- **The dock never starves the arrangement at the floor:** the existing clamp test (a stored dock height larger than
  fits leaves the timeline at least `arrangeMinHeight` less the insets, `ui_input_tests.cpp:17143`) also runs at
  1152×640.
- **Raster:** the image is ⌈logical × scale⌉ in size on every platform. On Windows (the platform the drives and
  `SoftwareCanvasCache` target, and whose text raster the bounds are measured on) two more: fidelity — the scaled
  render box-filtered back to logical size matches the 100 % render per panel (header, rail, timeline, inspector,
  dock) within a mean-difference bound; resolution — at 200 % the mean difference between the 200 % render and a
  bilinear 2× upscale of the 100 % render, in the timeline and header panels, is above a floor, so a cache or image
  drawn at 1× (blurred at scale) fails. Both bounds are set from the first measurement and written in the
  checkpoint's commit and STATUS. macOS and Linux keep the size check and the logical gates.
- The agent judges the twelve plan-cell renders and the 1280×656 @ 150 % display cell (crisp lines and text, nothing
  cut at a fractional scale); each defect found gains an assertion. The G6 montage (one PNG ≤ 300 KB under
  `docs/evidence/`) is made at G6's close, not here.

### Checkpoint 3 — the macOS frame exception, re-measured

- **Baseline:** the `YesDawTimelineGpuCheck` lines `max_frame_ms` / `sustained_frame_ms` from every CI run since the
  exception was written (2f48962, 2026-09-08), read with `gh run list` and `gh run view --log` for the macOS and Windows
  jobs, plus the local Windows machine alone (and its spread over ten runs). The table goes to
  `docs/evidence/2026-10-g62-gpu-baseline.md` and its summary to STATUS.
- **Profile:** the check's paint on the local machine, to name its dominant cost. If a renderer change brings the macOS
  runner's sustained frame under the budget (no threshold change), it is made and the exception is no longer needed.
- Otherwise the numbers and a recommendation go to Dan; the plan's §8.2 text is his and is not edited here. No reruns
  for luck, no widened threshold, no renewal claimed by this ADR.

## Consequences

- **Positive:** the plan's density at 1080p; track headers level with their lanes again, and gated; the window fits a
  maximized laptop display at 125 % and 150 %; every scale of the matrix is rendered and checked, not assumed.
- **Negative / accepted costs:** a tighter tool row and panel spacing; a 640 px-high window shows one whole lane with
  the dock open (the dock collapses); rendering the matrix costs headless test time; the exception's fate may need Dan.
- **Follow-ups:** the G6 montage at the phase close; the Direct2D on-screen check with the real-app drive; any renderer
  work the baseline calls for.
