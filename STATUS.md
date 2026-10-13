# YES DAW — STATUS (live handoff)

**Read this first on any machine.** This is the single source of truth for *where we are right now*.
The [active plan](docs/plans/2026-09-01-real-daw-ground-up-plan.md) and
[roadmap](docs/goals/roadmap.md) define the work; **this** file's newest entry is the live handoff.
Older entries below are dated history, not competing "Now" instructions.

> **Cross-machine rule:** `git pull` at the start of a session. At the end, update this file, commit in
> small chunks, and `git push`. Then the next machine — or the next session — is never lost.

## 2026-10-13 — UI correctness pass unit 28: a size shot is the window alone

**The defect (FIX 28, tooling):** the drives' window-size shots were taken with the pointer wherever the last step left
it. Its resting tooltip was composited into the shot: in `ss3-2560x1440` one covered the ruler at the tempo change, the
very spot the rubric judges.

**What changes:**
- `SizeShot <w> <h> <name>` in `tools/session-drive.ps1` resizes, parks the pointer on the window's own caption (left
  of the title, clear of the caption buttons), waits half a second, then takes the shot.
- It asserts that the resize settled, that the pointer is on the caption (`WM_NCHITTEST` says so), and that no app
  window (a tooltip, a menu) lies over the main window when the shot is taken.
- Every size shot uses it: SS1 (both loops), SS2-SS6 (the end-of-drive shots) and SS7 (the EQ editor at three sizes).
  The drive self-test takes its shot through it too, so the check runs on every self-test.

**Gate:** the size shot's own assertion. Self-test green; **M1** (the pointer left resting on the Play button, the old
behaviour) fails it with one app window, the tooltip, over the shot.

**Process:** drive tooling only; no product code, so no critic, per ADR-0075. The drives run again in the close-out
batch.

## 2026-10-13 — UI correctness pass unit 27: a tempo change hides only the bar numbers its label meets

**The defect (FIX 27):** bars beside a tempo label went unlabelled: 265 px between "33" and "57" at 1920. A tempo or
meter change's label was a fixed 76 px box right of its tick. Any bar number whose 36 px box met it was left out, so a
short "120" hid one or two neighbouring numbers with its empty right part. The empty part also took clicks and hover
there ("Tempo / meter change"), so a click on the ruler there jumped to the change.

**What changes:**
- A change's label is as wide as its text (plus 2 px), never wider than the old box. That rect is one law for the paint,
  the click and hover test, and the probe.
- A bar number is left out only when its tick-to-text-end comes within 4 px of a change's tick-to-label-end, on either
  side. A number on the change's own tick still gives way (ADR-0064: the change's label names the tempo there).
- The text is measured only when a number's whole box comes near a change, so a frame measures one or two.
- A change whose label is not drawn (out of view, or cut at the right edge, unit 25) takes no room but its tick, no
  click or hover, and no probe key. Before, it still hid a bar number beside it and answered clicks with nothing drawn
  there. The click test now rejects a change by position before measuring its text.
- The probe now uses the same law as the paint. It had re-derived it and ignored unit 25's right-edge rule, so it could
  report a number the paint left out. A probed number's rect is now its text's width, not 36 px.

**Gate `[ruler-map-gap]`:** bars every 80 px, with four "150~" changes:
- 20 px past bar 3's tick: bars 3 and 4 keep their numbers and are drawn (the old box hid both);
- on bar 7's tick: bar 7 gives way;
- 2 px past bar 10's number: bar 10 gives way, and its spot is drawn blank;
- a whole gap past bar 13's number: bar 13 keeps its number;
- its label ending 2 px before bar 15's tick: bar 15 gives way, bar 14 keeps its number.
- A click on bar 4's number is not a click on the change; a click on the change's text is.
- A change whose label would end 4 px past the right edge has no rect, takes no click, and spans its tick alone.

**Bites:**
- M1 a change takes the fixed 76 px box again (the old law);
- M2 no gap between a number and a change;
- M3 a change hides no bar number;
- M4 the click target keeps the 76 px box;
- M5 the paint draws every bar number, ignoring the law;
- M6 no gap right of a change;
- M7 an undrawn label still takes its whole span;
- M8 a label cut at the edge counts as drawn.

**Critic:**
One pass, no blockers or should-fix findings. It confirmed:
- the interval logic (both sides, ties, the unmeasured early-out);
- every consumer of the rects (hit-test, probe keys, the ADR-0064, unit 21 and unit 25 tests, SS3, the screenshot gate);
- the click targets: about 21-27 px wide, the old box's empty part now the ruler's own click;
- the frame cost: the timeline is a cached image, re-rendered only on edits and scrolls;
- the gate on any OS font;
- no warnings.

Of its nits, three were taken here:
- the gate's missing right-side gap case (bar 15, M6);
- an undrawn label at the right edge still hid a number and took clicks (M7, M8);
- the click test measured every change in the project on each mouse move; it now rejects by position first.

Left as they were:
- a click on a change's amber tick itself still does a snapped locate (the label starts 4 px right of it);
- the 16-change cap on the spans;
- a change's text is measured twice per render.

## 2026-10-13 — UI correctness pass unit 26: an undo takes down the message about the edit it reversed

**The defect (FIX 26):** the status line kept the last edit's message after that edit was undone. "Routed to new Bus 2"
stayed up after the undo removed Bus 2, as if still true.

**What changes:**
- A successful undo or redo takes down a success message. The line is then empty, so the hover hints and the "Monitor
  MUTED" mode hint show again.
- The undo or redo says nothing itself. The status line's rule (R4, cited by ADR-0058's amendment) is that success stays
  quiet, and Undo is the most frequent command.
- An error message stays up until it decays: it was about an action that failed, which no undo reverses.

**First design, replaced after the critic:** each undo said "Undo: <step>" in the undo history's words. The critic
showed it broke the quiet-success rule an Accepted ADR cites, hid the mute hint for 5 s on every Ctrl+Z, and named a
grouped step differently from the open history window. So the line is cleared instead, and the rule stays as written.

**Gate `[status-undo]`:**
- Shell: a track is routed to a new bus ("Routed to new Bus 1"); Ctrl+Z, which removes the bus, must leave the line
  empty.
- Model: a redo takes a success message down too, and neither an undo nor a redo takes down an error.

**Bites:**
- M1 an undo keeps the message (the old behaviour): the shell and model checks fail;
- M2 a redo keeps the message: the model check fails;
- M3 an undo takes down an error too: the error checks fail.

**Critic:** one pass on the first design (above). The revision is its own option (b): one small function and two calls.

**Noted, not changed:** the undo history window names a multi-edit group by its newest edit on the undo side and its
oldest on the redo side (`ProjectUndo.h` `foldSteps`), so a group's row changes name as it is undone. Parked.

## 2026-10-13 — UI correctness pass unit 25: a ruler label is drawn whole or not at all

**The defect (FIX 25):** the ruler culled its labels by their tick's position only. A label whose tick sat just inside the
right edge drew its text past it, where the canvas's edge and its scroll bar cut it: "12" left as "4:", "0:19." losing its
end.

**What changes:**
- A bar number, a time or a tempo / meter change is drawn only if its text ends inside the clip area's right edge,
  otherwise left out. This is the same rule as a bar number under a tempo label.
- The clip area's edge is the bound because the scroll bar column covers the ruler beyond it.
- A label whose whole box fits is not measured, so only the one or two by the edge cost a text measurement. The
  frame-time gate is tight.

**Gate `[ruler-edge]`:**
- A canvas is scrolled until a two-digit bar's tick lands 10-14 px short of the edge. It is tried bare, then with a
  "150~" change on that tick.
- The margin past the edge, in the bars and time rows' label bands, must stay the ruler's background: nothing runs into
  it to be cut. A label may end right at the edge.
- Labels further left are drawn, read inside their own boxes so ticks never count.
- The first version checked the edge columns themselves. It wrongly failed a whole "5", and three mutations stayed green
  because of how the scenario was set up. It was rewritten until all bit.

**Bites:**
- M1 bar numbers drawn past the edge;
- M2 times drawn past the edge;
- M3 tempo changes drawn past the edge;
- M4 no label ever fits (the inner labels vanish).

**Process:** a paint rule in the ruler, so no critic, per ADR-0075.

## 2026-10-13 — UI correctness pass unit 24: a meter's yellow begins at a level

**The defect (FIX 24):** every meter painted the top 22 % (vertical) or 18 % (horizontal) of its live bar yellow, so any bar,
even one at about -41 dBFS, wore a yellow cap.

**What changes:**
- A meter's yellow zone is a level, `UiTheme::Meter::hotFromDb` = -6 dBFS (the clip light marks 0 dBFS). It sits on each
  meter's own scale:
  - half of a linear-peak meter (the rail, strips, buses and the header);
  - the top 17 % of the master's dB scale (unit 7's law).
- A segment reaching the line paints yellow.
- The held-peak marker takes its zone's colour, by the bar's own pixel rule.
- No ADR fixes meter colours (ADR-0063 asks only 3:1 for indicators, which these tokens have), so the level is a theme
  token, named here and in the plan.
- The painters moved to `src/ui/MeterPaint.h` so gates can draw them directly.

**Gates `[meter-zones]`:**
- **The master meter,** through unit 7's seam: a -40 dB block paints no yellow at all (bar or marker), and a -3 dB block
  paints yellow only above, and green only below, the -6 dB line. The line comes from the published "0" and "-12" labels.
- **A linear meter, vertical (with its marker) and horizontal,** drawn directly at -40, -12 and -3 dB: yellow only past
  `10^(-6/20)` of the fill, and only when the level is past it.

**Bites:**
- M1 the old law, the top of any bar hot (a -40 dB bar wears yellow);
- M2 the held marker always yellow;
- M3 the master on the linear threshold (yellow below its -6 dB line);
- M4 a linear meter never hot;
- M5 a linear meter always hot;
- M6 the horizontal meter on its old band.

**Critic (separate agent), each finding verified against the code:**
- **Blocker, fixed:** B32's hold phase holds about -12 dBFS (fader 0.35, post-pan). Its marker is now green, and the gate
  counted only yellow.
  - The suite confirmed it: red at line 16917.
  - The gate now counts the marker in either fill, with its expiry check intact.
- **Should-fix, taken:** only the master meter was gated. The linear and horizontal meters now have their own gates.
- **Nit, taken:** the marker used a value test where the bar used a pixel test, so they could disagree at the line. Both
  now use the pixel test.
- **Nit, taken:** the level is named in the plan.
- **Clean:**
  - every caller's value is on the scale its threshold assumes;
  - the rounding is consistent;
  - unit 7's gate, the drives and the theme audit are unaffected.

## 2026-10-13 — UI correctness pass unit 23: a send row's PRE / PST reads over its level bar

**The defect (FIX 23):** a send row's tap label (PRE / PST) was drawn in the muted colour. At a high level the row's level
bar lies under it: the strip's accent at `mixerHeaderAlpha` over the row's inset. Measured on the five accents, the muted
colour gets 2.86-3.59:1 there (the judge's 3.59:1 was blue); the text colour gets 7.83-9.83:1.

**What changes:**
- The tap is drawn in the text colour, as the bus name beside it already is.
- New contrast pairs require 4.5:1 for a send row's text on each accent's level bar.

**Gate `[send-tap-contrast]`:**
- On a real shell (a track, a bus, a send dragged to full level), the send row is rendered in software.
- The tap cell's brightest ink must lie past halfway from the muted colour toward the text colour. The 11 px glyphs never
  cover a whole pixel, so the gate reads the brightest one: 0.85 measured, against a 0.77 line.
- Plus the contrast pairs.

**Bites:** M1, the tap back in the muted colour.

**Process:** a colour change in one label, so no critic, per ADR-0075.

## 2026-10-12 — Verification repair: the Windows timeline frame gate (YesDawTimelineGpuCheck)

**The broken verification:** the Windows job's `YesDawTimelineGpuCheck` went red on its first attempt at `733910b`,
`ae427f6`, `96367b8`, `9160004` and `cc9f5e3`, each at 16.7-20.9 ms against 16.6, each green on a same-code re-run.
That is broken required verification, so it interrupts the FIX list (CLAUDE.md).

**Supported cause** (a separate investigating agent; CI logs of 244 Windows jobs plus Linux and macOS controls):
- Commit `dd1ab73` (10-09) excluded the toolbar, ruler and clip areas from the clip before filling the timeline's panel.
- JUCE's Direct2D turns a clip of several rectangles into a full-surface masked layer. On CI's GPU-less Windows runner that
  cost about 1.5 ms a frame (speed-corrected against `YesDawReverbCheck`; Mann-Whitney p = 5e-8). Linux and macOS got
  faster from the same commit.
- Windows' red rate on the gate rose from 5.7 % to 18 % (Fisher p = 0.037). The runner image did not change, and today's
  ruler, playhead and tempo-map commits add nothing measurable.
- This machine measures 4.6-5.4 ms (a real GPU).

**What changes:** on Windows, a native (non-software) context fills the panel whole, as before `dd1ab73`; one opaque fill
is a single pass there. The software renderer and macOS keep the exclusion, with its pixel-identity gate.

**Tried first and rejected, with evidence:** the investigator's suggestion, the panel in one-rectangle pieces.
- **Not pixel-identical in JUCE's software renderer.** A path's coverage comes out a step short in its clip's last column:
  289 pixels at 1x, in a 2 px strip.
- **At fractional scales neighbouring pieces overlap one device row,** double-painting the stroke's edge: 12 pixels at
  1.25x.
- Banded pieces fixed the first problem but not the second.

**Gate** (Windows-only, new): `[simple-clips]`. A forwarding context paints a 240-clip timeline at 1x and 1.5x. It is not
the software renderer, so it takes the native path as Direct2D does. Nothing may be drawn under a clip of several
rectangles. The existing `[canvas-panel-occlusion]` pixel-identity gate stays green.

**Bites:**
- M1 the exclusion on every context (`dd1ab73`'s behaviour) reddens the new gate;
- M2 no exclusion anywhere reddens the occlusion gate.

**Expected effect:** Windows' red rate back to the old slow-runner tail (about 6 %), not zero. Reaching zero would take
further Direct2D work, never a budget change. CI on this push and the next runs will show it.

**Critic (separate agent), each finding verified against the code:**
- **No blockers.**
- **The app is unaffected.** It paints the timeline into its software cache (`SoftwareCanvasCache`, a software image),
  so on Windows it keeps the exclusion and its measured win (SS1 p95 8.2 to 5.82 ms). Only the GPU check and the packaged
  `YesDawFrameCheck`, which use native Direct2D images, take the full fill. If Direct2D cannot start, JUCE falls back to
  software, which keeps the exclusion.
- **No better option within JUCE 8.0.4.** Its Direct2D has no cheap clip for a list of rectangles; only one axis-aligned
  rectangle is cheap.
- **Should-fix, taken:** two comments said the gate runs the app's own paint path. On Windows it never did (the app paints
  in software), and now it takes the other branch too. Both comments now say so.
- **Should-fix, taken as process:** one green Windows run proves little at an 18 % red rate. The fix is recorded as an
  expected effect and is judged on the red rate over the next CI runs, not on a single green.
- **Nits:** the gate counts any exclusion as complex (strict side). Overlapping named clips still take a geometry layer
  on Direct2D, but nothing paints them there.

## 2026-10-12 — UI correctness pass unit 22: a MIDI clip's notes stay under its name

**The defect (FIX 22):** a MIDI clip's note preview used the whole clip body, so a high note over the clip's start ran
through its name ("MIDI").

**What changes:**
- A named clip's notes stay under the strip its name takes (`timelineCanvasClipNameStripHeight`: the name's inset and
  type).
- The preview places note rows by fractions of its area. With whole-pixel rows, the shorter area left a two-octave chord
  in its top half; now the pitch range spans the whole area at any height.
- Audio waveforms are unchanged here.

**Gate `[clip-notes-name]`,** pixels of a named clip with a high note over its start:
- the name's ink lies inside the strip;
- every note's ink lies under it;
- the notes are drawn.

**M7's older pitch check now measures its top band under the strip.** Its intent is unchanged: a chord reaches the top and
the bottom of the notes' own area.

**Bites:**
- M1 notes over the whole body (no strip);
- M2 whole-pixel rows (the chord no longer reaches the bottom band);
- M3 notes never drawn. Its first spelling did not compile (an unused variable under `/WX`), so it was rewritten to bite.

**Process:** a paint change in the clip body, so no critic, per ADR-0075.

**Found while waiting on CI, and next (it interrupts the FIX list, per CLAUDE.md):**
- The Windows `YesDawTimelineGpuCheck` reds have a supported cause (a separate investigating agent; 244 Windows jobs plus
  Linux and macOS controls).
- Commit `dd1ab73` (10-09) cut the covered areas out of the timeline's panel fill with a clip of several rectangles.
  JUCE's Direct2D turns that into a full-surface masked layer, so on CI's GPU-less Windows runner every frame pays about
  1.5 ms more. Linux and macOS got faster from the same commit.
- Windows' red rate on this gate rose from 5.7 % to 18 % (Fisher p = 0.037), with the runners no slower. Today's ruler,
  playhead and tempo-map commits add nothing measurable.
- The fix keeps the occlusion but paints the panel through single rectangles, with a gate that no fill happens under a
  clip of several rectangles.

## 2026-10-12 — UI correctness pass unit 21: the loop never covers a ruler label

**The defect (FIX 21):** the loop brace filled the top 12 px of the ruler's bars row, the same rows as that row's labels.
Its near-opaque end handle hid the tempo change label "150~" in the SS3 song at 1280, and any bar number under a handle.

**What changes:**
- The brace paints a 6 px strip at the top of the bars row.
- The row's labels (bar numbers, tempo and meter changes) sit under it, 7 px down and 14 px tall. A `static_assert` keeps
  them clear of the strip.
- The handles keep their 12 px grab zones, so dragging the loop is unchanged.
- The time row keeps its own label inset.

**Gate `[ruler-loop-strip]`,** on a canvas whose loop ends just past a "150~" change, so the end handle's zone lies over
the label (as in SS3):
- the labels sit under the strip;
- the bars row below the strip is pixel-identical with the loop on and off;
- the strip shows the loop, with its handles brighter than the band;
- the label is drawn.

**Bites:**
- M1 the brace painted at its old 12 px;
- M2 a change label at the old inset;
- M3 the handles not painted;
- M4 no brace at all.

M3's first run stayed green: the band's wash alone still changed the strip. The gate gained the handle-brightness check,
and M3 then bit.

**Seen:** a scratch render of the ruler. The loop is a thin teal strip with brighter ends; "150~" and the bar numbers sit
whole beneath it.

**Process:** a layout and paint change in the ruler, so no critic, per ADR-0075.

## 2026-10-12 — UI correctness pass unit 20: piano-roll key names whole in their keys

**The defect (FIX 20):** every white key's name (C3-B3) lost its top and bottom.
- A key row was inset 1 px top and bottom and then stroked 1 px inside. At the roll's 11-12 px rows that left a 7-8 px
  light face, under an 11 px caption.
- The letters ran onto the dark edges, where dark text vanishes; the next row also painted over any overflow.

**What changes:**
- **Keys:** the row is no longer inset. The key's own 1 px stroke is its only separator, so the light face is the row less
  2 px.
- **Names:** a name's baseline centres the ink of a key name inside that face (`pianoRollKeyLabelBaseline`). The ink is
  measured once per font from the outlines of every character a name starts with. A face shorter than the ink gets no
  name: a name is never cut.
- **Pad names in drum mode:** the same placement, still shortened with an ellipsis.

**Gate `[piano-key-labels]`:**
- **Placement:** for faces of 4-40 px, in plain and bold, a placed name's ink lies inside the face, and rows of the roll's
  target height (11 px) always name their keys.
- **The painted roll,** through the new probe of the keyboard as painted:
  - no key's stroke rows carry ink where a name sits, and a named key's face carries its name;
  - at the default dock, names show;
  - at a dock the gate finds with short but present rows (5-9 px), no name is drawn.

**Bites:**
- M1 the old 1 px gaps between keys (a stroke row holds something else);
- M2 the fit rule removed (a placed name's ink leaves its face);
- M3 names never drawn (a named face carries no ink);
- M4 the ink taken from the font box instead of the outlines (no name fits an 11 px row).

**Seen:** the roll at the 300 px and 160 px docks (scratch renders). The names are whole and C is bold.
- At 160 px the roll has no key rows at all; that is new **FIX 30** in the plan.
- That is also why the gate looks for its own short-row dock: at 160 px its "too short" half would have checked
  nothing.

**Process:** this unit changes the keyboard's drawing only (no logic, data or shared law), so it has no critic, per
ADR-0075.

**CI on `f4d4849`:**
- **macOS red in this gate,** at its first step: the roll was not visible after the MIDI clip's add. The test had made
  its project with a button click, which posts the click and pumps the message loop for 50 ms; on that runner the New had
  likely not run when the clip was added, so there was no track.
- **Corrective commit:** the steps are now the shell's synchronous dispatch (exactly a click's action). Each is required,
  so a repeat names the step that did not happen.
- **Windows:** its red there was the GPU gate, fixed in `87dd116`.

## 2026-10-12 — UI correctness pass unit 19: device names whole, or shortened in the middle, never squashed

**The defect (FIX 19):** the settings row's 150 px device choosers squashed a device's name to 70 %, then cut it with "..."
("Speakers (Focusrite USB A..."). The 64 px recorded-channel chooser beside them squashed "Input" and "No Inputs" too
(found here, the same defect).

**What changes:**
- **Device names are OS text, so they take ADR-0076's rule:** whole when they fit; otherwise shortened in the middle,
  with the lead and the end kept and U+2026 between; never squashed. The tooltip names the whole device, e.g. "Audio output
  device: Headphones (HyperX Cloud Alpha Wireless)".
  - `OsTextComboBox` marks a chooser of OS text. The theme gives it an `OsTextLabel` text box, which paints that form and
    owns that tooltip (a JUCE chooser's tooltip is its text box's).
- **The two device choosers grow:** they share the row's spare width, up to 300 px each. 1280 shows "Speakers (Focusrite
  USB Audio)" whole; 1536 and up show both long names whole.
- **Widths:** the base width is 134 px (was 150), to pay for the channel chooser at 96 px (was 64), which shows all its
  texts whole. A `static_assert` keeps the row inside the narrowest window.

**Gate `[device-choosers]`,** on injected long names at 1152, 1280 and 1920:
- the choosers' widths, and the row's end inside the window;
- each name whole, or the middle form, which fits;
- whole at 1920;
- the chooser's text box painted pixel-identical to a plain label showing that form at full width (so never squashed);
- the tooltip, over the chooser and over its text;
- the channel chooser's texts whole.

**Bites:**
- M1 JUCE's squashing label for the device names;
- M2 the flag ignored (a plain text box);
- M3 no growth;
- M4 each chooser taking all the spare width (the row overflows);
- M5 the tooltip without the name;
- M6 the channel chooser at 64 px;
- M8 the tooltip's subject taken from the text box (it compounds at each rebuild).

Two mutations needed a second look:
- **M1's first spelling** turned out to be the same condition as the original, so it could not bite. Rewritten, it bites.
- **M7, the subclass forcing a rebuild of its text box, stayed green, and the rebuild was removed.** The theme's text box
  is built anyway: JUCE builds a chooser's first text box in the base constructor, but joining the shell (whose look is
  its own) always rebuilds it.

**Critic (separate agent), each finding verified against the code:**
- **Blocker, fixed: the tooltip compounded.** JUCE copies a text box's tooltip into the box it rebuilds, and the app
  rebuilds three times as its window comes up. My text box built its tooltip from its own, so the result would have been
  "Audio output device: X: X: X", the screen reader's help text too.
  - The subject now comes from the chooser's own stored tooltip.
  - The gate forces two rebuilds and asserts the exact form. My first check only used `endsWith`, so it could not see
    this.
  - M8 (the subject from the text box) bites.
- **Blocker, fixed: a null reference in the test helper.** The local Clang check confirmed it (one finding on the test
  file); it would have broken the macOS build. The helper now returns a pointer, required non-null.
- **Nit, taken:** "No Inputs" appears only in the chooser's popup, never in the box. The gate now checks what the box
  shows: "Input" and "In 31+32".
- **Clean:**
  - the theme's text box is always rebuilt under the shell's look (as M7 showed);
  - the drawing matches JUCE's;
  - the layout is sound from 1152 to 8192 (no overflow; the existing `[shell-sizes]` gate covers overlap);
  - no drive or pin depended on the old widths;
  - the ADRs: ADR-0076's rule names device names, and ADR-0063 asks for a wider box, never a smaller size;
  - pixel identity, and warnings.

**CI on `96367b8` (units 17 and 18, and the Linux fix):**
- Linux green.
- macOS: only the standing GPU exception.
- Windows: first attempt red only on `YesDawTimelineGpuCheck`, 20.93 ms against 16.6; the same-code re-run was green,
  445/445.
- **Watch item:** that is the second Windows first-attempt GPU red today (`ae427f6`: 16.95 ms), both green on re-run.
  This machine measures 5.2 ms alone. My local suite went red on it once only while running six tests in parallel; local
  suites now run it alone, as CI does. A third Windows red brings a critic and a new hypothesis, per the policy.

## 2026-10-12 — UI correctness pass unit 17: the window is painted under the launch-time question

**The defect (FIX 17):** launching with a project whose audio is missing (ADR-0062) asks "Missing audio" before the open.
Under that question the main window was white: 100 % of a drive shot, against 0.1 % once answered.

**Measured** (a scratch drive on the `3f8dc35` exe, reading the probe under the question): the shell's paint count held at 1
while its timer kept ticking, and rose to 30 within 0.8 s of Cancel.

**Why, in JUCE 8's source** (the separate critic's reading, which corrected my first one):
1. JUCE dispatches its posted messages in batches, and a modal loop started inside one holds back the rest of that batch.
2. The shell posted the question from its constructor, before the window existed.
3. So the question sat in the first batch, ahead of the message that lets the Direct2D window put its first frame on
   screen. That frame was painted but never shown until the question was answered.

**What changes:**
- The question is kept until the first UI tick that finds the window showing, and that tick posts it; the posting hop puts
  it in a later batch, after those messages. ADR-0067 §7 allows only two timers, and my first version's vblank hook broke
  that law (its gate caught it), so the existing tick does the posting.
- If a project was opened or made in the moment before the question, the question is dropped.
- A launch minimised waits until the window shows.

**Gate, in the Session drive:**
- SS8 Step 4 shoots the main window alone under the launch-time question, and under the question asked again after the
  Locate chooser. It asserts the shot is fresh (`MainShotPainted`, new in `tools/session-drive.ps1`), under 25 %
  near-white, and that its menu row carries its words (0.5-40 % light). A black capture cannot pass either.
- **Red on the old exe:** SS8 on `3f8dc35`, 2 failed (100 % white), 212 passed.
- **Green on this build:** 214/214 (0.3 % white, menu row 2.0 % light), and on each earlier build of this unit.

**Bites** (a short drive with the same assertion plus SS8's probe check, rebuilt per mutation):
- M1 the question posted from the constructor, the old timing (100 % white);
- M3 the question run inside the UI tick instead of posted (100 % white: the tick's batch is held).
- M2, the first tick posting it whether or not the window shows, stays green: the posting hop is what matters, and the
  showing check only serves a minimised launch.

**Critic (separate agent), each finding verified against the code:**
- **Should-fix, taken:** the mechanism is the batch holding back the frame's on-screen message, not the vblank message;
  the comments now say so. The fix works through the posting hop.
- **Should-fix, new unit:** with the question up, the window is frozen after its first frame. Verified: the UI tick asks
  for repaints every tick (`repaintDynamicLayers`), yet the paint count holds at 2 for 3 s. The question's own buttons
  then do not show hover or Tab focus. This predates this unit (the old build was frozen and white), so it is FIX 29 in
  the plan, its fix being questions without a nested modal loop.
- **Should-fix, taken:** the gate could pass on a stale or black capture. It now removes the old file and checks the menu
  row.
- **Nit, taken:** a launch-time gap for input. The question is dropped if a project is already loaded.
- **Clean:** lifetime, the posting order, macOS / Linux, and warnings.

**Also:** the drive-activation error seen once in unit 18 ("could not restore the topmost state") did not recur in these
runs: five SS8 runs and thirteen short drives.

## 2026-10-12 — UI correctness pass unit 18: alerts draw in the theme

**The defect (FIX 18):** every alert used JUCE's stock V4 look. That look draws a 130 px translucent icon bleeding off
the alert's top-left corner and under about 35 px of the title and text. The app had never made its own look the
default, and the stock alerts are top-level windows built with no component.

**What changes:**
- **Drawing:** `YesDawLookAndFeel` draws alerts in the theme:
  - `panelRaised`, `text` and `panelStroke`, in the title and body type;
  - a 28 px icon whole in the 80 px gutter JUCE leaves left of the text, beside the title (an amber triangle with "!"
    for a warning; a blue circle with "?" or "i" otherwise);
  - a `static_assert` keeps the icon clear of the text.
- **Wiring:** the shell makes its look the app's default when it is built. JUCE holds the default by weak reference,
  so nothing is left dangling when the shell goes.

**Gate `[alert-style]`:**
- A shell's look is the default.
- On a display (Windows / macOS; the Linux runner has none), a warning, a question and a note alert, built as
  `showOkCancelBox` builds them, draw their icon:
  - in its colour;
  - whole, at least 8 px inside the alert;
  - at least 8 px left of the text;
  - on the theme's panel.
- Once the shell is gone, the default is JUCE's own again.
- Two contrast pairs: the icons at 3:1 on `panelRaised`.

**Bites:**
- M1 JUCE's V4 alert (the icon's ink);
- M2 the icon clipped at the corner;
- M3 the icon under the text;
- M4 the shell not the default;
- M6 JUCE's alert colours.
- M5 (the shell leaves the default pointing at its look) stays green, correctly: JUCE's weak reference makes that state
  harmless, so it has no behaviour to bite.

**Critic (separate agent), each finding verified against the code:**
- **Blocker, fixed:** an alert is a native top-level window, and the Linux runner has no display. JUCE would dereference
  a null display (`getParentMonitorArea`), so the drawing part now runs on Windows / macOS only, as the other
  native-window tests do (`ci.yml`: "runners have no display").
- **Should-fix, fixed:** clearing the default in the shell's destructor broadcast a look change while the closing window
  still existed. `ResizableWindow::lookAndFeelChanged` then re-adds it to the desktop with different style flags, so
  the window would rebuild itself on every quit. Removed; JUCE's weak reference makes it unnecessary.
- **Nits:** the last check guards JUCE's weak reference rather than our code (its comment now says so). An alert's text
  field keeps JUCE's stock colours (parked: theming `TextEditor` is app-wide).

**CI on `ed17058`:**
- **Linux red, fixed by the next commit:** GCC's `-Werror=unused-local-typedefs`. The test's two type aliases were used
  only inside the Windows / macOS guard, so on Linux they were unused. They now sit inside it.
  - Reproduced here with Clang by forcing the guard false (the same two errors, lines 31058 / 31059); clean with the
    fix.
  - The Windows-side Clang check compiles the guarded block, so it could not see this.
- **macOS:** only the standing GPU exception (`timeline_gpu_tests.cpp:172`, `sustained < kFrameBudgetMs`). The
  `[alert-style]` drawing checks passed there.
- **Windows** and the rest green.

**Drive:** the built app at launch over a missing-audio bundle (`x17b`, scratch): the themed alert is on screen, its icon
whole. The window under it is still white; that is unit 17. One drive run failed in the drive's own activation step,
"could not restore the YES DAW window topmost state". It came before any assertion and was the first such line in any
log this pass. The identical rerun passed. It is watched in unit 17's SS8 runs.

## 2026-10-12 — UI correctness pass unit 16: the export progress readout reads whole

**The defect (FIX 16):** while an export runs, its readout stands in the Export button's 88 px slot beside the 28 px
cancel cross. That leaves 48 px for text, but "Export 37%" needs about 64-72 px and "Cancelling…" about 73-76 px, in
Verdana / DejaVu, so both were squashed.

**Why the slot cannot grow:** at the minimum window (1152) the master card needs every pixel of the header.

**What changes:**
- **Running:** the readout reads the percent alone ("37%", at most 25 px). A 3 px bar along its foot is as wide as the
  share done (`accentBlue` on `darkControl`, held to 3:1 in the contrast gate). Its name and tooltip still say "Export
  audio progress"; it stands where the Export button was.
- **Cancelling:** the cross has nothing left to do and hides. "Cancelling…" (ADR-0058's word, unchanged) takes the whole
  slot, whole, with 78 px of room.
- **The next export** gets its cross back.
- New: `src/ui/ExportProgressReadout.h`.

**Gate:** `[export-progress]` at the minimum window, on a real paced export.
- **Running:** the text is a bare percent, whole at its widest ("100%"); the readout and the cross are inside the slot
  and do not overlap; the bar's share is the job's.
- **Cancelling,** through the cross's own dispatch and through Esc: the cross is hidden, the readout is exactly the
  slot, "Cancelling…" is whole, and there is no bar.
- **The bar as painted:** only its 3 foot rows are lit, half the width at half done, clear of the text.
- Plus the contrast pair. The H16 export test now reads "100%".

**Bites, all red for their own reason:**
- M1 today's text, "Export N%" (the bare-percent check);
- M2 the cross stays while cancelling (the cross-hidden check);
- M3 the readout stays narrow while cancelling (the readout-is-the-slot check);
- M4 no bar painted (the foot rows' lit count);
- M5 the bar never fed the job's share (the fraction check);
- M6 the bar the readout's whole height (no bar above the foot).

**Also in this push:** unit 11's status-line gate now ticks before it reads the export failure too. That was macOS CI's
red on `4b8f080`: the same cause `9c092be` fixed for the import refusal, at the gate's second read.

**Process:** layout and text with a small paint, so no critic, per ADR-0075.

## 2026-10-12 — UI correctness pass unit 15: the drive batch, the rubric, FIXes 1-15 closed

**Candidate:** `dist/YesDaw-3f8dc35-win64-portable.zip` (units 1-14), `YesDaw.exe` SHA256
`CEC920BEA4266504944D27DF5F85E1C091F66858A2B47EB3CBE25659B69CCA24`; its packaged self-check passes (make-demo, then
selfcheck; render, export and reimport bit-exact) and reports `3f8dc35`. Drives on Dan's shared desktop, hands-off with
his go-ahead (he was away). Shots: `build-ci/session-shots/2026-10-12-ui-pass/` (the 10-11 before-shots untouched).
Evidence: `docs/evidence/2026-10-12-ui-pass-drives-and-hardware.json`.

**Batch:**
- **Green:** SS1 59/59, SS3 84/84, SS4 22/22, SS5 77/77, SS8 212/212.
- **Green on re-run:** SS2 42/42 (first run 39/42), SS6 58/58 (first run 45/47).
- **SS7 pending - no clean run.** First run 180/199; re-runs 104/124, 123/124, 48/51, 48/55.
- **Why the reds were the environment, not the product:** every failing step in SS2, SS6 and SS7, across eight runs,
  followed Google Chrome taking the foreground from YES DAW within 5 s, with the app out of the foreground at the failing
  step (the drive's foreground monitor: pid 31700, once 41840; 5 ms to 3195 ms). Chrome's visible windows were "Sign In"
  and "Success!", raising themselves every few seconds; they are Dan's browser and a sign-in, so they were not touched.
  SS2 and SS6 re-ran with no flashes and passed whole. SS7 had no flash-free run, so it stays pending until one.
- **Unit drive checks inside the batch:**
  - SS3: the counter reads bar 57 one pixel right of its label's tick (`057|01`; before the fix, about bar 81 there);
  - unit 14: the menu and alert evidence shots log `(+1 app windows)`, and SS7 Step 16's pointer shots log "main only".

**Rubric (§7.4),** judged by a separate agent on the batch's shots and checked against the 10-11 before-shots, plus my
own look at the ctest matrix renders at 150 % (`yesdaw-g62-display-1706x896-150`, `-window-*-150`, regenerated on this
build): **FIXes 1-15 are all fixed at 1280, 1920 and 2560**, each with its shot cited:
- the tempo press, the ruler bars and times, and the playhead head;
- the master meter (-18 dBTP between "-12" and "-24");
- the rail, the dock re-layout, the export cross;
- the status line whole in the menu row;
- the TAKES card and the Curve row (whole at 150 % too);
- the strip readout, narrow I/O and lit cells;
- the composited popups.
The table, by line:
- line 2 (eight tracks at 1080p) PASS;
- lines 3, 5, 6, 7 PASS;
- **lines 1 and 4 FIX** - on older defects not on the FIX list, now new units 16-28 in the plan.

**New FIXes (16-28, in the plan, worst first):**
- 16 the export progress label squashes "Export 10%"-"99%" and "Cancelling..." (about 64-69 px in a 48 px box; unit
  15's measurement);
- 17 the main window is white under the launch alert;
- 18 the stock JUCE alert's clipped icon;
- 19 squashed device choosers;
- 20 clipped piano-roll key labels;
- 21 the loop handle over the tempo label;
- 22 the MIDI clip name;
- 23 PRE / PST at 3.59:1;
- 24 the meter's yellow band follows height, not level;
- 25 the last ruler label cut;
- 26 a stale status after undo;
- 27 unlabelled bars beside a tempo label;
- 28 resting tooltips in the size shots.

**Packaged hardware verifier** (the candidate's own `verify-hardware.ps1`; rows committed verbatim in
`docs/reality-lane.md`):
- **playback FAIL** `playback_block_exceeds_target`. The default output is now **Speakers (Focusrite USB Audio)**,
  and three backend attempts each granted 480 frames at 48 kHz against the locked 128-frame target - as the HyperX
  headset did on 10-12. So the device is not the cause: the shared-mode audio path grants the system engine's 10 ms
  period whatever the interface.
- **recording FAIL** `recording_silent`: a round trip with nothing feeding the default input (the HyperX microphone)
  while Dan was away.
- **frame PASS** (headless dense timeline).
- The playback target therefore needs an owner decision (see Next).

**CI:**
- units 7-10 certified;
- unit 11 (`ae427f6`): its macOS status-gate timing is fixed in `9c092be`; its Windows GPU red gets one same-code
  re-run, queued;
- unit 12 (`5d686a9`): red only on that macOS status-gate timing (fixed in the next push), Windows green;
- units 13 and 14 running.

**Next:**
1. SS7 one clean run, once the Chrome sign-in windows are closed.
2. Units 16-28.
3. The close-out's unit 7 (the Usable-song milestone close) - unblocked by FIXes 1-15, with the hardware playback
   result above.
4. An owner choice on the playback target: an exclusive-mode / ASIO path for 128-frame blocks, or a target that the
   shared-mode path can meet.

## 2026-10-12 — UI correctness pass unit 14: evidence shots show the app's popups and alerts

**What changes (tools only):** the drive's `Shot` captured the main window alone, so a shot of an open menu or an
alert showed neither - the rubric could not judge them. `CaptureClient` now composites the app's own windows above the
main window over its client area (the canvas stays the client's size) and returns how many it drew; `Shot` logs
`(+N app windows)`. `AppWindowsAbove` walks up the z-order (`GW_HWNDPREV`, at most 1024 steps) and keeps a window only if
it is the same process, visible, not minimised, not cloaked and not layered (JUCE's drop-shadow strips). A plain screen
copy was rejected: it pulls in other apps' windows (D3). `Shot -MainOnly` keeps the old capture; SS7 Step 16's three
pointer shots use it (a reappearing JUCE tooltip would trip `HoverStrokeShown`).

**Gate:** the Windows-only ctest `YesDawSessionDriveCaptureCheck` (`tools/test-session-drive-capture.ps1`) on the real
`YesDawDrive` type, with real windows: a main window; above it a topmost "menu", an unowned non-topmost "alert" (a JUCE
alert's shape) and a layered window; below it a same-process window; over it another process's window.
- **Preconditions** come from `WindowFromPoint`, plus the alert's owner and topmost state; a failed precondition is a
  FAIL naming the window in the way.
- **Asserts:** the PNG is exactly main's client size; the menu and alert show; the layered, below and foreign windows'
  places show main's colour; 2 composited; and main only leaves the menu's place main's colour.
- **Run here** (the desktop free, Dan away): PASS, 14 checks.

**Bites, M1-M7, all red for their own reason:**
- M1 today's code (0 composited);
- M2 the z-order walked the other way (the window below shows);
- M3 a screen copy (the layered and foreign windows show);
- M4 topmost only (no alert);
- M5 owned or topmost only (no alert);
- M6 no layered filter (3 composited);
- M7 no process filter (the foreign window, 3 composited).

The first M5 run did not bite: WinForms had given the test's "alert" an owner, so it was no longer a JUCE alert's shape.
The test now clears the owner and checks it as a precondition. The first M6 run's red was a compile error (an
unreachable-code warning); the mutation was rewritten so the compiler cannot see the condition is false, and it then
went red for the right reason.

**Process:** tools only, no critic, per ADR-0075. **Risk the plan names:** the first exact-code CI run must show the
Windows runner has an interactive desktop; if not, the gate is redesigned with a critic, never skipped.

**Drive:** the capture shots (SS3/SS4/SS8 evidence with N >= 1, SS7 Step 16 "main only") in unit 15's batch.

## 2026-10-12 — UI correctness pass unit 13: mixer strip parts - the drag readout in its cell, narrow I/O whole, lit cells legible

**What changes:**
- **(a2) The drag readout sits in its cell.** A strip fader drag's transient floated over the strip's I/O rows above the
  rail. It now sits on the strip's own readout cell (`paintedFaderReadoutBoundsForLane`, one law for the paint and the
  drag), drawn as the cell is: its numeric caption font (the theme's label font is now overridable per label,
  `YesDawLookAndFeel::kOwnFontLabel`), the controlInset fill, centred, no border. The rail's VOL readout keeps its
  place and look.
- **(b) Narrow I/O reads whole.** The I/O rows' text box clears the well on both sides and ends in an ellipsis rather
  than squashing. Narrow strips drop the "In: " / "Out: " prefix and read their target ("Bus 1", "Master").
- **(c) Lit cells use textOnFill.** A lit R cell letters in textOnFill (ADR-0063). A lit S or M cell is filled with the
  strip's colour and lettered in textOnFill. This is an **agent rubric call**: a deliberate departure from the
  reference's tinted letter, which read only 1.64:1 against an unlit one; the rail already fills its lit cells.

**Gate `[mixer-states][strip-parts]`:**
- **The drag readout:** held at mid-rail on a Track and a Bus strip, wide and narrow, at 1280/1920/2560. It equals the
  readout cell, lies inside the strip, and misses the I/O rows, sends, inserts, S/M/R, pan, rail and thumb. Its pixels
  equal the cell's own paint (the cell shows the ridden gain), corners aside.
- **Narrow I/O:** "Bus 1" and "Master", fitting their box, with ink-free clearance columns.
- **Lit cells:** S and M are filled with the strip colour, with at least 6 textOnFill pixels and no kText inside the
  rounded cell. The Bus strip's unlit S stays controlInset. R is textOnFill, never kText.

**Red before:** the readout floated at y 551, over the output row, with no cell to sit in.

**Bites (6):**
- the transient back above the rail;
- the "Out: " prefix back on narrow strips;
- the lit fill reverted;
- kText on R;
- the readout in the theme's font (162 px differ);
- S and M lettered in kText.

**Critic** (the readout law is shared by paint and drag): it found
- the new `.readout` probe key broke the ADR-0066 control walk - it is a display, now excluded;
- the label's font was ignored by the look-and-feel, so the readout drew 13 px body type over an 11 px numeric cell -
  fixed and gated by pixels;
- the lit-ink check could pass on corner pixels - now counted inside the rounded cell.
It confirmed the geometry, the exit paths, accessibility (the I/O names are unchanged) and the warnings. Parked: a dark
custom track colour would give a lit cell about 1.3:1 (pick the ink by luminance).

**Local:** full build, ctest 444/444, Clang clean (11 files).

**CI:** unit 11 (`ae427f6`, run 38320663121): macOS red on [status-whole] - the gate read the label before the UI tick that hands it an import refusal on macOS (fixed in `9c092be`), and on `YesDawTimelineGpuCheck`'s sustained frame (raw 24.01 ms, inside the exception); **Windows red only on `YesDawTimelineGpuCheck`'s sustained-frame `REQUIRE`** (raw: sustained 16.95 ms, max 19.17 ms, 9 slow frames; 443 of 444 passed) - not covered by the exception; unit 11 changed nothing the GPU check paints, and unit 3's same red did not repeat on a re-run, so one same-code re-run follows (a second red calls for a critic and a new hypothesis). Unit 12 (`5d686a9`) running.

**Drive:** SS7 `ss7-sweep-drag-*`, `ss7-narrow` and `ss7-solo`, in the batched drive proof.

## 2026-10-12 — UI correctness pass unit 12: inspector cards drop whole and fit

**TAKES card (FIX 12).** The card took whatever room was left up to its 74 px slot, so with 30 px of room it painted
its heading over the band below the content. It now shows only when its tallest content fits (heading 20 + chart 4 + 44
= 68 px, a `static_assert` keeping that inside the slot), and layout and paint share that one rule (the dead
`takesNeededHeight` check is gone). Accepted: with takes and 50-67 px of room the chooser and Delete now drop with the
card (the scrolling inspector stays parked, D50).
- **Gate `[inspector-takes-card]`:** a recorded take; at 1536x960 with the settings row open (rooms 30/10/50/67/68/108)
  and closed (70/10/50/67/68/108), reached through the dock's height. The band below the card is pixel-identical between
  the Clip and Track tabs; a whole card has its heading and contains the chooser and Delete; a dropped card leaves both
  empty. 5 whole cells, 7 dropped.
- **Red before:** at room 30 the band differed by 7668 px. That run is the named mutation (the old rule).

**Fade Curve row (FIX 9).** The shape chooser and the amount slider split the row in half past the fade sliders' 150 px
inset: at the minimum inspector the chooser was 23 px (no text box), and the chooser's only live tooltip read "H14
canonical fade law". The row now starts just past the painted "Curve" word (`inspectorFadeCurveControlLeftInset` 48),
the chooser has a fixed 120 px (`inspectorFadeCurveChooserWidth`), the slider takes the rest, and the controls sit on
their painted row (top gap 4 -> 1). The stale tooltip override is gone: the chooser says "Clip fade curve shape".
- **Gate** (`[ui][screenshot][tokens]`, a PNG per cell): 1280x720 (with the smallest dock, since the default one leaves
  no room for the card), 1920x1080 and 2560x1440, each at the inspector's minimum, default and maximum width. The
  controls are disjoint; every shape name fits its text box with 2 px spare; the painted "Curve" word ends before the
  chooser; tooltips start with each control's title. The rail is at least two thumbs at the default width (58 px) and
  three at the widest - three at the default would need a chooser under 106 px, too narrow for "Equal power" in DejaVu
  Sans (about 71 px in a 78 px box).
- **Red before:** a 23 px chooser at the minimum inspector. **Bites:** the H14 tooltip back.
- **Judged by eye** (agent): at 1920 the row reads "Curve", "Equal power" whole, and a slider with rail on both sides of
  the thumb. At 240 px the slider is only its thumb - the parked under-42 px rail at widths 240-283.

**Records corrected:**
- The G6.1 cp1 record that closed the "fade" truncation row was wrong: the chooser still cut its names at the minimum
  inspector.
- The painted "Curve" word and the worded tooltips label the amount slider.

**Process:** a small layout unit with no logic, data or shared-law change: no critic, per ADR-0075.

**Local:** full build, ctest 444/444, Clang clean (6 files).

**CI:** unit 11 (`ae427f6`) running.

**Drive:** SS8 and SS1/SS2/SS3 crops in the batched drive proof.

## 2026-10-12 — UI correctness pass unit 11: the status line shows its messages whole

**Decision:** ADR-0076, accepted after a separate critic. The critic ruled that shortening user or OS text in its middle
does not conflict with plan §7.4 line 1, on two conditions now written into the ADR:
- every string the app writes in full shows whole at 1280 on every platform font;
- a shortened message never paints something untrue.

**What changes:**
- **Placement.** The status line was a tool-row control squashed into about 106-133 px at 1280 (JUCE compressed its
  glyphs to 70 % before truncating). It now sits in the menu row's free right side - box W - 708 px, budget W - 720
  (560 px at 1280) - as part of the header layout, and is never squashed.
- **Layout pins.** The tool row's choosers no longer reserve room for it, so the layout matrix re-pins 1706x896
  (`clip.inspector.markers timeline.nudge.chooser`) and 1536x793 (fades + `clip.inspector.markers
  timeline.edit_mode.chooser timeline.nudge.chooser`), as the ADR computed.
- **Joined messages keep their parts.** The model's `reportStatusParts` is used by the five places that join facts
  with "; " (import refusals, the device lines, the device and rate warning, open-cancelled, the launch fallback).
- **One shortening rule** (`statusLineShownText`), applied in the paint only:
  - parts are never mixed: leading whole parts are shown, then "; … N more";
  - a single message's own "; " is never a join;
  - a first part too long on its own is shortened in its middle, by code point, with U+2026.
- **While a message is shortened** its tooltip is the whole text, the line takes the mouse (never keyboard focus), and
  the message's decay waits while the pointer is over it. The label's text, the probe and accessibility keep the whole
  message.
- **Six fixed texts reworded**, meaning kept, measured in Verdana and DejaVu Sans to leave the Linux font a margin:
  the Clip, Ruler, Note and Control lane hints, and the step input and musical typing mode hints. Musical typing no
  longer repeats "step input" (its button shows it lit). The phrases the hover test pins ("drag to move", "click to
  locate") stay.

**Gates:**
- `[status-whole]`:
  - placement at 1280/1920/2560: in the menu row beside the menu bar, visible, box W - 708, scale 1.0;
  - a 4 px hint sweep over the arrangement (with a marker, a tempo change and a clip), the rail, a mixer strip with an
    insert, and the piano roll with a pencilled note under every control-lane choice; it must meet twelve zone
    prefixes and reaches 41 distinct hints;
  - the mode hints at their longest (musical typing at C-1 and top velocity, step input, the monitor muted with Dim);
  - the 25 literal-only messages read from the sources;
  - every one of these within 560 px in each font present (Segoe UI Variable and Verdana here);
  - an import refusal and an export failure shown whole at 1280;
  - at 1152, a long file name painted by the rule (never "..."), tooltip whole, the line taking the mouse.
- `[status-shorten]`: the pure rule's unit cases - parts never mixed, a single message's "; " not split, lead and end
  kept, non-ASCII by code point.
- ADR-0061's preferences cases:
  - the joined device lines whole at 1920, and at 1280 whole where they fit, else their lead kept with the whole text in
    the tooltip;
  - the rate warning alone whole at 1280 and 1920.
- ADR-0063's tooltip code-word check skips the status line only while its tooltip is a shortened message.

**Red before:** five texts over the budget - Clip 579, Ruler 577, Control lane 717, step input 589, musical typing 839
(and, once the sweep reached it, Note 632 in Verdana). **Bites:** the old Ruler hint back (612 px); squashing allowed
again (scale 0.7).

**Critics:**
- The ADR critic, above.
- The code critic found:
  - the hover test's "click to locate" pin broken - fixed;
  - the step-input hint at about 572 px in DejaVu Sans, which Linux CI measures - reworded, with the six texts now at
    most about 537 px in DejaVu;
  - the "Note:" hint unreachable by the first sweep, and 632 px - reached and reworded;
  - a single message's "; " read as a join, which could invent "… 1 more" - fixed with parts.
  It confirmed the label paint, the decay pause, the layout move and the warnings clean. Parked: the message sweep
  covers only literal-only messages.

**Local:**
- full build, ctest 444/444, Clang clean (24 files).
- One MSVC internal compiler error (C1001, in an unchanged header) on a mid-unit build did not repeat on the retry.

**CI:** unit 9 (`7b0963e`): **all green** - certified. Unit 10 (`9d36a47`): every job green except macOS, red only on `YesDawTimelineGpuCheck`'s sustained-frame `REQUIRE` (raw: sustained 19.54 ms, max 25.81 ms, 22 slow frames; 440 of 441 passed) - inside the renewed exception - certified. Unit 8 (`4b1c4d7`) likewise (raw: 19.18 ms, 14 slow frames) - certified.

**Drive:** in the batched drive proof - SS8 `audition-on` / `refusal`, the SS7 hover sweep at three sizes, a
control-lane hover and musical typing on, at 100 % and 150 %.

## 2026-10-12 — UI correctness pass unit 10: the export Cancel control reads during an export

**What changes:** the header gives an export's Cancel a 28 px slot, and the "Cancel" text button squashed its word
into it (12 pixels of ink). A wider slot breaks the master card's 2 px slack at the minimum width and ADR-0063 forbids
smaller text, so the button is now a `ToolbarActionButton` painting a cross (plan §7.4 item 3: an unambiguous icon and
a tooltip; its accessible name and tooltip still say Cancel). **Gate** (`[export][tokens]`, a real export paced to
60 s at the minimum window): the control is visible, enabled, inside the window and has a tooltip; if it paints a word
the word fits with 2 px to spare, else more than 8 pixels of icon against a blank button of the same look; a click
cancels - one outcome, `cancelled`, none finished, no destination and no `.partial`. `[assets]` covers the cross.
**Red before the fix:** "Cancel" needs 43.6 + 4 px in 28 px. **Bites:** the text button back (red-before); the cross
removed (0 pixels of ink - the gate's first blank used JUCE's default look and passed this mutation; it now uses the
button's own). **Record corrected:** the H16 CP7 export test never clicked Cancel during an export; this gate does. A
small rendering unit: no critic, per ADR-0075. **Drive:** SS8 Step 5 `ss8-unsaved-during-export` in the batched drive
proof (SS8 stays 212/212).

## 2026-10-12 — UI correctness pass unit 9: hiding the dock by key gives its height to the arrangement

**What changes:** a chord that showed or hid the editor dock (Y the browser, X the mixer) changed the model but laid
nothing out until the next window resize: the lanes ended 300 px early over an empty band with the dock splitter still
showing, and a re-shown browser had an empty rect. `resized()` now records the dock height it laid out, and
`refreshActionState()` lays the shell out again when the dock's height differs (and skips its own mixer layout then,
as `resized()` does it). **Gate `[dock-layout]`**, driven by key presses (the harness's helpers call `resized()`
themselves): after each step - Y, Y, a resize, Y, X, X, then the Pencil tool on an empty part of a lane, which opens
the piano roll - the layout on screen equals a fresh layout, the dock's height goes to the lanes, scroll bars and rail,
and the browser and the roll fill the dock. **Red before the fix:** after the second Y the lanes ended at 646 where a
fresh layout puts them at 946, with the splitter shown. **Bites:** that is the fix removed. **Critic** (a logic change
in the action pipeline): every dock path is covered (chords, menus, buttons, model verbs, the splitter drag, project
open, the UI tick), no re-entrancy, no spurious re-layouts; it added the pencil step (a pencilled MIDI clip opened its
roll into an empty rect - fixed by the same line), the either-or ordering, and parked the same bug for the header height
after an autosave Restore. **Local:** full build, ctest 444/444, Clang clean (before the critic's two edits; their
targets re-ran). **CI:** unit 7 (`bd4377a`) running - macOS already green this time; unit 8 (`4b1c4d7`) queued.
**Drive:** SS8 `ss8-after-drops` / `ss8-refusal` in the batched drive proof.

## 2026-10-12 — UI correctness pass unit 8: the rail shows only real track data

**What changes:** each rail row drew an instrument picture picked by its index - a drum on the first row, a guitar on
the next two, a mic after - for tracks that have no instrument (wrong data, D3). The picture is gone; the icon square
stays empty and the audio/MIDI badge keeps its place. **Omission logged:** the rail shows number, swatch, name, the
audio/MIDI badge and M/S/O, and no instrument icon until a track-category model exists. **Gate `[rail-icons]`**
(screenshot test, software renderer, five empty audio tracks at 1920x1080, selected and primary rows skipped): each
row's swatch is the track purple, its badge's middle bar differs from the gap, and every unselected row's icon cell is
pixel-identical: **red before the fix** - two rows' cells differed in 293 pixels. The `[assets]` loop now pins
`drawTrackKindBadge` (both kinds paint, and differently). **Bites:** the picture back (293 px). A small rendering unit
with no logic, data or shared-law change: no critic, per ADR-0075. **Local:** UiScreenshotCheck 22/22 test cases;
full suite with the push. **Drive:** SS1 and SS6 shots in the batched drive proof.

## 2026-10-12 — UI correctness pass unit 7: the master meter fills to the level its scale names

**Process:** from here units follow ADR-0075 (Dan, 2026-10-12): targeted tests per commit; the full suite, one critic
and one push per unit; commits per logical change (the commit-count rule is retired). Unit 6 is **certified**: CI on
`84d9f61` and `f4de0ac` - every job green except macOS, red only on `YesDawTimelineGpuCheck`'s sustained-frame
`REQUIRE` (raw: 17.14 ms / 18.01 ms sustained; 440 of 441 passed) - inside the renewed exception.
**What changes:** the master pane's meters filled by the linear peak while the scale beside them is evenly spaced dB
marks (0, -12, -24, -60), so a bar sat far below its label. They now fill piecewise-linearly in dB between the marks
(`masterMeterFractionForDb` / `masterMeterFractionForPeak`), and each label is centred on its level in the fill
(`masterScaleLabelBounds`, shared by the paint and the probe's `mixer.master.scale.<dB>`). The hold and latch numbers
are unchanged; the other meters stay linear (parked). Commit 1 (`196db9a`, no behaviour change) published the labels
where the old paint drew them, so the gate ran on the old paint: **red** - "0" 5 px below the fill top; at 1920x1080 the
-42 dB bar 79.5 px and the -24 dB bar 132 px below their points on the scale. **Gate `[master-meter-scale]`** (343
assertions): three steps - a louder block, a tick, the live block, a tick, so the held marker and the live bar are
measured apart (L held -24/-6/0, live -42/-18/-9; R held -12/-3/0, live -30/-12/-3: every segment) - at 1280x720,
1920x1080, 2560x1440 (dock at its maximum) and 1280x720 with a 300 px dock: labels present, disjoint, evenly spaced,
"0" at the fill top and "-60" at the fill bottom; ink only inside the label rects and in each one; the bar within 2 px
of its level (a segment gap plus rounding) and the marker within 1.5 px; at 0 dB the clip light. A `static_assert` pins
the "-60" label's 1 px clearance above the "L R" row. **Bites:** both readings linear (19.5 px), the live bar alone
linear (19.5 px), the marker alone linear (below the bar), a law right only at the marks (20.5 px), the probe moved
but not the paint, one channel fixed (33 px). **Critic** (separate agent): the law, paint/probe/label agreement,
hold/latch and warnings clean; it found the first gate measured the held marker where the bar should be (a live-only
revert passed), never measured the bar at 0 dB, skipped the top segment and could miss a dropped label - all fixed as
above. It also found the labels collide on docks under about 276 px - pre-existing (the old law collided under a 56 px
column), parked in the plan. **Local:** full build, ctest 444/444, Clang clean (on the law; the gate rework and the
`static_assert` re-ran their targets). **Drive:** SS7 Step 13's shot in the batched drive proof. **Still open (parked):**
M6's master clause - the master fader's unity point reads about -18 on the scale beside it.

## 2026-10-12 — UI correctness pass unit 6 (commit 2): the ruler's bar labels follow the tempo and meter maps

**What changes:** the ruler numbered bars by the head tempo and meter only, so after a tempo or meter change every
label drifted from the header counter (the rubric's shot: label 57 at 1:52, where the counter reads about bar 81). The
canvas state now carries the compiled tempo map and the meter map (`makeTimelineState` builds the map once a state, as
it already did for the map labels), and `computeRulerBarLabels` places bar N at its first tick's frame - the counter's
own law. A label's step comes from a full bar of the meter in force, never the next bar's start. A bar a meter change
cuts under one tick has no label (no tick reads it). The search for the next label starts at the bar under the
minimum spacing, and spacing ties at exactly 56 px clear it, as the head law's single step does. With no map (no
project), the V4 head law is unchanged. **Gates `[ruler-bars]`:** red before the fix - Shell A's bar 21 at 40.02 s,
where its start is 36 s. (1) Three shells - 120 -> 240 at bar 17 and 3/4 at bar 33; a 60 -> 120 ramp; a 100-tick bar -
against closed-form starts: every label within a pixel, ascending, spaced, running to the right edge, past bar 60. (2)
A phantom bar at 400 px/s: bar 3 at 26332/30720 s and no bar 2. (3) A pure sweep - five maps (both shells' maps, 1/64
at 240 from bar 9, the phantom, a meter change every 500 ticks) at six zooms and four scrolls: each label's frame reads
its bar on the counter and four frames earlier an earlier bar; spaced; no stretch unlabelled - plus a flat map
matching the head law at 960 views, the 150 BPM 7/2 tie included. **Bites (5):** tempo map left null (Shell A off by 4
s); the step from the next bar's start (Shell C stops at bar 9); phantoms labelled ("2" where the counter reads 3); no
seek (one label across 1/64 at 240: 1240 px bare); no tie tolerance (19 labels where the head law has 23). **Critic**
(separate agent, the law compiled against the real `Time.h`, 20,000 random maps, every label checked against the
counter): found the stopping bug (13 of 20,000 maps; one view drew no labels), the tie wobble and the coverage gap -
all fixed and gated as above; lifetime, per-frame cost (no new allocation; at most 140 us a paint for a 1000-change map)
and warnings clean. Parked: the frame-time gates paint with no tempo map. **Local:** full build, ctest 444/444,
Clang clean. **Drive:** SS3 Step 17 now sets Snap: Off, clicks one pixel right of the first bar label past the
tempo change and asserts the counter reads that bar, then restores Snap: Grid - written, parse-checked, and run in the
batched drive proof. **CI:** unit 5 (`70e5aef`, run 38280748963): nine jobs green; macOS red only on
`YesDawTimelineGpuCheck`'s sustained-frame `REQUIRE` (raw: sustained 18.18 ms, max 21.83 ms, 19 slow frames; 439 of
440 passed) - inside the renewed exception. Unit 3's Windows re-run (`733910b`): **green**, so its GPU red did not
repeat on the same code. Unit 6 commit 1 (`84d9f61`) is running. **Next:** unit 7 (the master meter's fill matches its
scale).

## 2026-10-12 — UI correctness pass unit 6 (commit 1): the bar start of any bar under the meter map

**What changes (engine only, no visible change yet):** the G2.15 meter walk under the header's bar|beat counter is now
one shared law (`walkMeterMap`), read by `barBeatAtTick` (the walk itself, unchanged; `computeBarBeatPiecewise` now
calls it), its new inverse `barStartTick` (the first tick that reads as bar N), and `nominalBarTicks` (a full bar of the
meter in force, the step commit 2's ruler thins its labels by). The stale `computeBarBeat` comment that said no
piecewise bar law exists is corrected. **Finding:** the plan's "round the fractional start up" is not the exact
inverse - in doubles, bar x length lands on the wrong side of a whole tick (3/39 at bar 66: 307200.00000000006, so the
rounded-up start is one tick late); a Python sweep found 2,284 such cases in 821,248. `barStartTick` therefore checks its
estimate against the walk's own arithmetic and moves by a tick. The plan's 7/7 example has a whole-tick bar (61440), so
the rounding case is 3/7 (26331 3/7 ticks). **Gate `[time][g2][tempo][inverse]`** (new test case): hand-written starts
for a map with a 7/8 change mid-bar and a 100-tick bar; bars 1..200 round-trip to {N, 1} with the tick before reading
N-1; the 3/7 starts 87772 and 114103; every meter the walk accepts (1..32 over 1..64, from tick 0 and after a change
100 ticks into bar 2) for 400 bars each - 1,638,400 exact inverses; a phantom bar (a change 4/7 of a tick after a 3/7
bar line: no tick reads bar 2, which starts where bar 3 does); bar < 1 refused; the nominal bar in force. **Bites:**
without the check against the walk, 4,568 failures (first 3/39 bar 66 -> 307201); with floor and no check, the 3/7 start
is 87771. **Critic** (separate agent, a Python port of the doubles plus fuzzing): no blocker; old and new walk agree on
240k fuzzed ticks; the inverse exact on 540k random maps; it asked for the phantom-bar literals, a corrected plan (the
7/7 wording and the bite) and two comment fixes - all applied. **Local:** full build, ctest 444/444, Clang clean. **CI:** unit 5 (`70e5aef`) (run 38280748963): macOS red only on `YesDawTimelineGpuCheck`'s sustained-frame `REQUIRE` (raw: sustained 18.18 ms, max 21.83 ms, 19 slow frames; 439 of 440 passed) - inside the renewed macOS exception; Windows still running, the other eight green; unit 3's
Windows re-run is still queued. **Next:** unit 6 commit 2 (the ruler's bar labels on the maps).

## 2026-10-12 — UI correctness pass unit 5 (commits 1-2): the ruler's time row shows each bar's exact time

**What changes:** each time-row label read its time back from the tick's rounded pixel, up to half a pixel early or
late, and the formatter truncated whole seconds - so about half the labels read a second early (bar 9 at 120 BPM showed
"0:15" for 16 s) or late, and the tenths format could round to "1:60.0". Labels now carry their bar's exact start, and
whole units are floored with SMPTE's 1e-9 guard and split as integers. Accepted change: the tenths mode floors too (1.96 s
reads 0:01.9). Commit 1 (`88ac8e3`) was the no-behaviour-change refactor, so the gates ran unchanged on the old code:
**red there** - the formatter read "0:59" for 11 bars at 44 BPM (exactly one minute), and the sweep found **442,176 wrong
labels of 816,509** (first: bar 33 at 44 BPM, 1 px/s, "2:55" for 174.5 s). **Gates `[ruler-time-labels]`:** the
formatter cases, and a sweep of every zoom from 1 to 55 px/s in 0.01 steps at two scroll positions - 120 BPM in min:sec,
samples and SMPTE, 44 BPM in min:sec - against expected text from integer arithmetic only: zero mismatches, plus the
rubric's pins (at 5.02 px/s bar 9 reads 0:16, bar 33 reads 1:04). **Local:** full build, ctest 443/443, Clang clean.
**Commit 3 - contrast and a scan gate:** the time row's labels were drawn in `rulerTick`, a decoration token, at 3.19:1
(ADR-0063's text floor is 4.5:1); they are now `kMutedText` (6.19:1) and `rulerTick` is set again only for the tick. The
new `[tokens]` gate "text is drawn in text tokens" reads every `setColour` of a surface, indicator or decoration token in
`src/ui` - any spelling of `Color::`, and every `k`-alias of one (an alias naming two tokens fails) - and finds any
text-drawing call before the next `setColour` or block end: red before the fix with exactly one finding (that time row),
102 sites seen; a mutation narrowing it to the `UiTheme::Color::` spelling sees 29 and fails its floor of 60. **Local:**
full build, ctest 443/443, Clang clean. **CI:** unit 3 (`733910b`, run 38274309482): nine jobs green; **Windows** red
only on `YesDawTimelineGpuCheck`'s sustained-frame `REQUIRE` (raw: sustained 17.63 ms, max 18.99 ms, 17 slow frames;
442 of 443 passed). Not covered by the macOS exception. The check compiles only `timeline_gpu_tests.cpp` ->
`TimelineFrameCheck.h` -> `TimelineCanvas.h`, which unit 3 did not touch, so its code is identical to unit 2b's
(`68e9761`), where Windows passed - the earlier same-SHA Windows reds (10-09) did not repeat either. One same-SHA re-run
of the failed job follows; a second red would call for a critic and a new hypothesis, not another re-run.
**Next:** unit 6 (bar labels that follow the tempo and meter maps).

## 2026-10-12 — UI correctness pass unit 4: the playhead shows no invented number and covers no ruler label

**What changes:** the playhead's purple pill on the bars row printed round(seconds) + 1 as if it were a bar number (bar
5 read "10" - wrong data, D3), covered the bar label under it and was cut at the canvas edge. It is gone: a small white
head (6 x 8 px) rides the marker lane at the ruler's bottom, as the reference image and Logic place it, and the line runs
from the head down; the header counter already shows bar|beat. The head stops short of a marker's label when the
playhead rests on the marker. Seven badge tokens and an unused purple are deleted; white on the ruler (about 19:1) is a
new token-contrast pair; the parking-lot entry is closed. **Gates**, both red before: `[playhead-head]` (the canvas at
900 x 300 / 40 px/s with a marker, the playhead every 5 px, both ends and the marker: what it paints above the ruler's
bottom starts in the marker lane - it started at the ruler's top, y=29 against 73 - stays inside the ruler and misses
the marker's label) and `[rubric-shots]` now checks bar 1's tick column too (the pill put 3.1:1 purple there). Two
mutations bite (the line from the ruler's top; a head wide enough to reach a label). Looked at: the 1080p timeline shows
bar "1" and "0:00.0" readable and the head beside "Marker 1". **Local:** full build, ctest 443/443, Clang clean.
**Critic (Opus 5.5):** no blocker; in - STATUS items (2) and (8) closed, the clamp's comment made honest (the clip
inset already keeps the head inside the ruler), its argument order guarded, the gate's pixel scan 0.9 s. Parked: a
playhead just right of a marker crosses the bottom of its label's first glyph (the plan's known residual, a little
wider); the head is smaller than the reference's (the rubric judges it in the batched drives). Committed as `73522dd`;
this entry followed separately.
**CI:** unit 2b (`68e9761`, run 38270400380): nine jobs green; macOS red only on `YesDawTimelineGpuCheck`'s
sustained-frame `REQUIRE` (raw: sustained 17.52 ms, max 20.38 ms, 14 slow frames; 439 of 440 macOS tests passed) - inside
the exception Dan renewed this morning; 2b changed no renderer. **Next:** unit 5 (the ruler's time labels and contrast).

## 2026-10-12 — UI correctness pass unit 3: a fader ride shows and writes the value being ridden

**What changes:** in a Touch / Latch / Write ride the model keeps the saved gain while the ride buffers its points, but
the strip's thumb, its dB readout, its accessible value, the fader hit-test and the probe all read the saved gain - so
mid-ride the strip said 0.0 dB beside a -1.3 dB transient readout, and keyboard rides stepped from and released at the
saved gain, writing the wrong automation. The gain being ridden is now remembered for the strip under the drag
(cleared on release and when the strip goes) and every reader shows it (ADR-0052: the riding control's live value
wins). Keyboard rides now build up and release at the held value. The strip readout, the transient readout and the
accessible value share one dB text (ADR-0066), with the painted readout's -80 dB floor as -inf for all three. **Gate
`[mixer-states][ride]`**, red before (strip 0.0 dB vs transient -1.3 dB): a held pointer ride shows the ridden text on
the strip, the transient and the seam, the saved gain stays 1.0, the cap follows the pointer and nothing stale remains
after release; a keyboard ride (Tab to the fader, Enter, Up x3, Enter) reads 3.0 dB on the strip, the transient and the
accessible value and commits a last point that maps back to exactly +3 dB. The rail's VOL and the inspector's Fader
row for the same track show the ridden gain too, the gain is clamped to the rail's range once, and the probe publishes
each strip's painted readout (`mixer.strips[i].readout`) for drives. **Critic (Opus 5.5):** no blocker; in - a pointer
press on another strip during a keyboard ride released that ride at the saved gain (the press runs before the shell
closes the gesture): the open ride now releases at its ridden gain first, gated (+2 dB written); the rail / inspector
readers; the accessible-value and rail checks mid-ride; the clamp. Four mutations bite (the helper showing the saved
gain, the ridden gain outliving the release, no cross-strip close, the rail VOL reading the saved gain). Parked (old
behaviour): the drag state follows a strip index if a track is added or removed mid-drag; Esc during a keyboard Touch
ride saves the pass ending at the original value rather than discarding it. **Local:** full build, ctest 443/443, Clang
clean. **Drive:** SS7's sweep-drag shots in the batched run (the sweep drags sideways, not a ride; the probe's readout
field makes a ride drive step possible). **Next:** unit 4 (the playhead's invented number and the label it covers).

## 2026-10-12 — UI correctness pass unit 2b: a tempo drag is one undo step

**What changes:** every move of a tempo-cell drag wrote its own undo step (a 50 px drag left about thirty; feel law 6:
one step per gesture). The engine's E21 gesture coalescing - the mechanism fader, trim and note drags already use -
now admits the two tempo verbs: the head tempo, and one tempo change scrubbed at its own tick (never merged across
ticks), each continuing from where the last edit ended; the time-map rows are carried forward in both coalescing paths;
and ADR-0066's rule extends to them (a scrub let go where it began leaves no step). The cell brackets its drag as one
gesture. No new contract: the existing E21 / ADR-0066 facility, extended verb by verb as before. **Gates**, both red
before the fix: `[tempo-scrub]` (engine, `tests/project_tests.cpp`: three edits in a gesture left four steps, not two)
and `[tempo-press][one-step]` (UI: one Ctrl+Z after a three-move drag stepped back only to 170.5, not the pre-drag
140). Now a three-move drag persists 185.5 (140 + 30 x 380 / 250) and one Ctrl+Z returns 140, a second 120. A drag
also keeps editing the tempo it started on when the playhead crosses another change mid-drag (as in playback) - it
switched rows and split into two steps; gate `[tempo-press][target]`, red before (the crossed change took the edit).
A ramp toggle at the scrubbed tick keeps its own step (the curve is part of the match), and a change scrubbed away
and back leaves none with the redo history restored. **Eight mutations bite** (no drag bracket, verbs not coalescable,
time-map rows not carried, the no-op rule gone for either verb, a merge across ticks, no curve check, no target lock).
**Local:** full build, ctest 443/443, Clang clean. **Critic (Opus 5.5):** no blocker; in - the target lock, the
curve rule, meter continuity, the two missing gate cases; parked - a keyboard interaction then a mouse press on the
cell ends coalescing (the master fader has the same pre-existing gap). **Next:** unit 3 (a fader ride shows and writes
the value being ridden).

## 2026-10-12 — UI correctness pass unit 2: Shift fine-drag reaches the tempo cell (B30's record corrected)

**What changes:** B30 (Shift = exactly 10x finer drag) was certified as covering the header tempo, but
`FineDragSlider` excluded its `LinearBar` style from the day it landed (44185e2) and no gate exercised it - broken
verification, repaired. `LinearBar` is now supported, and its fine movement is measured against the same drag extent
as its plain drag (`getMouseDragSensitivity()` since unit 1; the width only for a bar that snaps). The backlog's B30
note carries the correction. **Gate `[tempo-press][fine-drag]`**, red before the fix (**150.5**: Shift ignored): a
20 px Shift drag persists exactly 123.0 BPM (a plain 20 px is 30.4; a tenth is 3.04 -> 123.04, snapped to 0.5), and
Shift pressed mid-drag anchors at the tempo reached (a plain 20 px to 153.5, then 250 px with Shift: exactly 191.5).
Mutations dividing by the width or by 240 px turn it red (the second slipped through a weaker first version - the
critic's catch). **Local:** full build, ctest 442/442, Clang clean. **Critic (Opus 5.5):** no blocker; in - the tighter
gate, the drag-extent assertion in both tempo gates, the corrected comment, the unused snap branch dropped; parked - a
Shift+right-drag also fine-edits the cell (a plain right-drag already edits it). **Next:** unit 2b (a tempo drag is
one undo step).

## 2026-10-12 — UI correctness pass unit 1: a press on the tempo cell never changes the tempo

**What changes:** the header tempo cell is a scrub cell, not a fader. A still press, plain or with Shift, no longer
jumps the tempo to the pointer (it wrote 120 -> 213 BPM and an undo step); a drag scrubs relative to the tempo in force
(about 1.5 BPM a pixel). Ctrl / Alt no longer swap it into JUCE's velocity drag, whose key release mid-drag would now
jump it (found by the critic). ADR-0046 law 1: Logic and Pro Tools change tempo by dragging or typing, never by a click.
**Gate `[tempo-press]`** (`tests/ui_input_tests.cpp`), run before the fix and red: a still press at x=2 set **26.0**
BPM, and the velocity swap was on. Now: still presses at three points, plain and Shift, leave the value, the persisted
tempo and the dispatch count unchanged; a 20 px drag persists exactly 150.5 (120 + 20 x 380 / 250, snapped to the 0.5
grid) and a Ctrl drag 181.0. **Drives (proof batched later on one packaged build):** SS7 Step 16 asserts a still press
leaves tempo and dispatch count alone (its Ctrl+Z repair is gone); SS3 Step 17 asserts its tempo drag edits.
**Local:** full build, ctest 442/442, Clang clean. **Critic (Opus 5.5):** no blocker; in - the velocity swap, an
empty-map guard; added to the plan - unit 2's fine-drag must divide by the drag extent, and a parked item for the
cell's mouse wheel (about 27 BPM a notch). **Found on the way:** each value change during a tempo drag is its own undo
step and bundle write (feel law 6) - now plan unit 2b. **Next:** unit 2 (B30's fine-drag certification on this cell).

## 2026-10-12 — UI correctness pass planned: the rubric's fifteen FIXes, worst first

**Plan:** [`docs/plans/2026-10-12-ui-correctness-pass.md`](docs/plans/2026-10-12-ui-correctness-pass.md). A workflow
(Opus 5.5, 31 agents) investigated each of the fifteen rubric FIXes in the code and screenshots, and a separate agent
challenged every proposal (13 of 15 corrected); all fifteen are confirmed, none needs an owner decision, and the plan
uses the corrected versions. Fifteen small units, each with its fix, a gate that a named mutation turns red, and drive
proof batched on one packaged build: data-changing behaviour first (the tempo cell press; B30's untrue fine-drag
certification on that cell; a fader ride's readout), then wrong data in the frame (the playhead's invented number, the
ruler's early time labels and their contrast, bar labels ignoring the tempo map, the master meter's scale, the rail's
invented instrument icons), then layout and cut text (the dock-hidden dead band, the export Cancel label, a whole-text
status line - ADR-0075, the inspector cards, mixer strips), then drive tooling (evidence shots that show popups and
alerts). **Next:** unit 1 (the tempo cell press).

## 2026-10-12 — Dan's three calls: macOS exception renewed, Vera task headless, Focusrite check next

**macOS frame exception: renewed** (Dan, 2026-10-12) in the plan's §8.2 - the same narrow scope (macOS, the timeline
GPU check, failing only its sustained-frame `REQUIRE`, everything else passing), now re-evaluated when the renderer,
the test or the runner image changes or Dan's machine measures above 8 ms sustained. Unit 3's `9604299` red (sustained
19.36 ms, failing at that `REQUIRE`) falls inside it. **Vera WSL Autostart: made headless** (Dan's yes) - the same
`wsl.exe -d Ubuntu --exec /bin/true` every 5 minutes, now launched by `conhost.exe --headless` so no window takes the
foreground; the original task is backed up at `%USERPROFILE%era-wsl-autostart.task-backup-2026-10-12.xml`
(restore: `Register-ScheduledTask -Xml (Get-Content <file> -Raw) -TaskName 'Vera WSL Autostart' -Force`).
Verified at its 06:39:06 run: result 0 (WSL ran) and a foreground trace saw no change at all. **Hardware playback on the Focusrite:** Dan chose the check on that device; switching Windows' default
output is his click (a system setting), then the packaged verifier runs again. **Next:** that run, then the UI
correctness pass.

## 2026-10-11 — Units 3-6 certified by exact-code CI; ADR-0069 cp2 closed

**CI on `c56e8fe`** (run 38208010592, the head carrying units 3-6 and the drive tooling): **all ten jobs green** -
Linux, Windows, macOS, TSan, RTSan, both package jobs with their packaged self-checks, both alpha-verify self-tests.
macOS's GPU frame check passed this run (its unexcused red on unit 3's `9604299` stays recorded above; the exception
renewal still awaits Dan). That certifies units 3, 4 (writes on), 5 (the drive proof's code is this code, byte for byte
under `src/` and `tests/`) and 6 (one writer), and closes **ADR-0069 cp2** (restore links back or refuses; the probe
carries the writer's real cost). The close-out checklist's per-unit box and cp2's box are ticked. **Open:** unit 7, the
Usable-song milestone close - the visual rubric's FIXes (ranked in the unit 5 entry below, worst first), the packaged
hardware-playback result, the montage and roadmap pointers. **Packaged hardware verifier run** (the candidate's own
`verify-hardware.ps1`, generated rows committed verbatim to `docs/reality-lane.md`, evidence
`docs/evidence/2026-10-11-writes-on-drives-and-hardware.json`): frame PASS; **playback FAIL
`playback_block_exceeds_target`** - the default output is now the HyperX Cloud Alpha Wireless headset, which grants
480-frame blocks in every WASAPI mode against the locked 128 (callbacks healthy: worst 0.14 ms, zero deadline misses,
real output); recording FAIL `recording_silent` (no loopback route; recording is not part of this milestone). The
128-frame target has never been granted on this machine (the Focusrite gave 480 in shared mode on 07-28); the checker
uses only the default device. An equipment decision for Dan, not a code change or a threshold change. **Next:** the UI correctness
pass on those FIXes, each with its gate.

## 2026-10-11 — Close-out unit 5: writes-on proven through the packaged app (SS8 green; full batch green)

**Candidate:** `dist/YesDaw-b28437e-win64-portable.zip` from `b28437e` (units 4-6), `YesDaw.exe` SHA256
`2DC987F0445EA97CCF942A547744D0232C289D1789E6E42C0BB851C0B15FDCEF`; its packaged self-check (make-demo, then
selfcheck) passes and reports `b28437e`. Every drive below ran on that exe, on Dan's shared desktop with his go-ahead
(hands off), on the machine's current audio defaults (no endpoint switching). Local shots/logs:
`build-ci/session-shots/2026-10-11-writes-on/`.
**SS8 (SS-6) 209/209** - the first full pass (171/177 while writes were off). The three laps on the linkable fixture:
each edit's autosave confirmed with `lastAutosavedEditSerial == editSerial` from a pre-edit baseline, **45.1 / 42.0
ms** peak (bound 100), **7 linked, 0 copied**; kill + reopen asks nothing and equals the confirmed autosave (5,0,0,7);
lost writes raise the question naming it (6,0,0,7; bundle stamp 19 behind snapshot 21) and Restore brings it back;
Save + close + reopen asks nothing and the autosave is retired. **Step 8** (hydrated cloud placeholders the OS will not
hard-link): the app creates the project in a fresh sync root and imports real audio, the edit's autosave copies 1 of 1
and links none, the copies and the user's audio hash true and stay distinct, lost writes ask and Restore recovers
(2,1,0,1), the audio is still the same hydrated placeholders after Restore and Save, and the root is unregistered and
removed (none left).
**Batch:** SS1 59/59, SS2 42/42, SS4 22/22, SS5 77/77, SS6 58/58, **SS7 197/197** (196/197 on 10-09), SS3 72/72 after
the investigation below. Feel: paint p95 **3.15 ms** (SS1) and **2.99 ms** (SS7 sweep) against 6.98 / 4.98 ms on 10-09,
zero underruns and deadline misses in the measured bursts (SS1 and SS7 each report one counted xrun since launch,
outside the bursts - recorded raw; the audio endpoints were not logged), launch 1.06-1.40 s.
**SS3's red, root-caused (not a product regression):** the batch's SS3 failed one clip-menu pick (56/57) and a later
run another: both times the menu had opened, then closed by itself before the pick. Autosave is ruled out (ticks 588 /
716, before the writer's first possible write at ~909). An investigation workflow (Opus 5.5: JUCE, app code, history,
synthesis) named the cause, then it was proven: the scheduled task **'Vera WSL Autostart'** (Dan's other project) runs
`wsl.exe` in his session every 5 minutes and Windows Terminal takes the foreground for ~140-160 ms; JUCE closes any open
popup when another process takes the foreground (correct desktop behaviour). Both failing menu shots were within 0.2 s
of the task's recorded run time (hh:mm:06) and no passing menu shot closer than 0.5 s; a raw foreground trace caught a
141 ms Terminal flash (`fg-trace-2144.txt`); and a controlled reproduction, twice, held the File menu open across a
fire - it closed by itself 10 ms and 28 ms after Terminal took the foreground (137 / 158 ms flashes), while a control
hold away from a fire stayed open.
**Drive tooling (agent scope):** `session-drive.ps1` hooks `EVENT_SYSTEM_FOREGROUND` on its own message-loop thread and
records every foreground change exactly; a FAIL prints any window that took the foreground from YES DAW in the 5 s
before it (including one still holding), and every summary lists the flashes over a running YES DAW (launch, close and
relaunch gaps excluded); each log starts with the exe path and SHA256. Diagnostics only - a FAIL stays a FAIL. The
monitor's reading is gated headlessly in `test-session-drive-chooser.ps1` (ctest; three mutations bite).
`MenuPickByName` asserts uniqueness only when the item was found, never clicks a guess between two, says when an item is
there but disabled, and survives a popup vanishing mid-search. SS3's five positional menu picks now pick by label
(57 -> 72 assertions). SS8's Lap 3 now proves the save (window title) and the reload, checks every snapshot slot is
gone, and the stamp check can no longer skip silently; its stale "known-red" comments are gone. **Dan's call (not
changed):** making that task windowless would stop it closing menus - in drives and in his own use of the app.
**Review (workflow, Opus 5.5, every finding put to a refuting verifier):** the evidence audit found every unit-5
requirement met and every number in the logs; the tooling critic's and the audit's fixes are the tooling changes above.
The **visual rubric** found real defects that predate this work (no drawing code changed in units 4-6) - parked as the
next UI correctness pass, ranked: (1) pressing the header tempo cell without dragging changes the tempo (120 -> 213 BPM);
(2) ~~the playhead badge prints round(seconds)+1 as if it were a bar number~~ (closed by unit 4); (3) ruler min:sec labels
read 1 s early at about half the bar ticks; (4) after a tempo change, bar labels keep the old spacing while the counter
follows the tempo map; (5) the master meter's dB labels are evenly spaced over a linear-amplitude fill; (6) the rail's
instrument icons are chosen by track index, not the track (D3); (7) ruler time-row text is 3.19:1 (rulerTick on
timelineRuler), under ADR-0063's 4.5:1 text floor, and the token gate misses it; (8) ~~the playhead badge covers the bar
label under it~~ (closed by unit 4); (9) the inspector's fade Curve chooser shows 'E...' beside a bare 30 px slider; (10) with the dock hidden
the arrange keeps its docked height (dead band); (11) the export Cancel button renders as '...'; (12) the TAKES card is
cut by the dock; (13) status-line errors and hover hints ellipsize the useful half; (14) mixer drag readout disagrees
with the strip's own dB for a moment and overlaps the Out slot; narrow strips overprint their Out label; solo is weak on
strips; (15) evidence shots cannot show popups or alerts (the drive captures only the main window). Each FIX gets a gate
when fixed (plan section 7.4); they gate the milestone close (unit 7), not this push.
**Final batch on the committed tooling** (`fe42284`, same exe): SS1 59/59, SS2 42/42, SS3 72/72, SS4 22/22, SS5 77/77,
SS6 58/58 (the monitor caught one Terminal flash at 22:19:06, 172 ms - no menu was open), SS7 197/197, **SS8 212/212**
(209 + Lap 3's proven save and reload, every snapshot slot, the stamp field). Its summary listed FileDrop's own drag
window nine times; `f2a77b0` leaves the drive's own process out of the summary (labelled 'this drive' on a FAIL), gated
headlessly; SS8 re-run on it: 212/212, summary 0 flashes. **Unit 5 closed** (its three boxes ticked). **Next:** rebase onto
main (this push answers main's pointer note, now removed), push, exact-code CI on that SHA; then unit 7 (milestone
close: the rubric FIXes above, the montage, roadmap pointers).

## 2026-10-11 — Close-out unit 6: one autosave writer - the engine's old trigger is deleted (committed locally)

**What changes:** ADR-0068 cp7. The engine's `needsAutosave` / `markProjectEdited` / `markAutosaved`, their two edit
counters, and the bridge header `persistence/PlaybackAutosave.h` (`writeAutosaveFromControlTick`) are deleted, with the
engine test that was their last caller (its law - a clean project writes nothing, an edit writes one snapshot that
recovers - is the model's `[due]` gate since unit 4). Nothing else changes behaviour.
**Gate `[autosave][one-writer]`** (static, final form): reads `src/`, `tests/` and `tools/` at run time - none of the
retired names and no bridge header remain, and outside the tests the snapshot writer is named only by its definition and
two callers, each pinned to its function: `UiAppModel::writeAutosaveTick` and the self-check's `makeDemo`
(`src/app/SelfCheck.h`: the scratch fixture alpha-verify inspects, never a user's project - recorded in the close-out
plan as the reading of the ADR's "its own tests"). Any spelling counts (no space, a line break, a pointer to it); a
comment line does not. Ten negative controls behave: eight breakages go red (a retired name back; a second writer in
Save, spaced, unspaced, across a line break or by pointer; a caller in the defining header; a caller under `tools/`; the
header restored), a comment and a longer name stay green. `docs/alpha-gate.md` now names the model's writer.
**Critic (separate, Opus 5.5):** no blocker; in - the three gate holes above (one spelling, the defining header skipped,
`tools/` unscanned), the SelfCheck allowance recorded durably and pinned to `makeDemo`, a leftover include dropped.
**Local:** full build, ctest **442/442**, Clang clean (12 translation units). **Not pushed:** it sits on unit 4's local commits (same readiness rule). **Next:** as unit 4.

## 2026-10-11 — Close-out unit 4: the model owns the autosave writer (writes on; committed locally, not pushed)

**What changes:** the app now really autosaves. Every 30 s the model writes a snapshot when there is an edit since the
last Save that no autosave holds yet and no recovery question is up (ADR-0068 §1-§2: edit serials, checked in that
order; the old engine trigger is no longer consulted and goes in unit 6). A failed write says "Autosave failed", keeps
the edit due so the next tick retries, and raises a sticky `notDurable` flag that only a real successful write clears
(a test's canned result never does). Save, Save As and Don't Save make the clean point the autosaved one too; Save a
Copy does not. Don't Save marks the tail autosaved only when it really retired the autosave, and if the New / Open /
Open Recent it was for is then cancelled or fails, the shell ends the attempt and the open project's tail is due again.
A retirement that fails reports and raises the sticky flag; the Save still stands. **Held, never overwritten:** a
snapshot whose question was raised but never answered, and that a later open cannot read, is the user's lost work
(ADR-0068 §5): that open asks nothing, says "An earlier autosave could not be read (...) - autosave waits until a
later open can read it", and neither writes over it nor retires it (Save As leaves it with the bundle left behind).
The probe gains ADR-0068 §7's `editSerial`, `lastSavedEditSerial`, `lastAutosavedEditSerial`, `skips`, `notDurable`;
CONTEXT gains **Save** and **Autosaved vs saved** (§8). Unit 3's test-only `YESDAW_UI_TEST_HOOKS` seam is gone (its
gates now make real edits); `YESDAW_PERSISTENCE_TEST_HOOKS` stays target-wide, the shipped app macro-free.
**Gates:** `[due]`, `[pending-gate]` / `[no-write-while-pending]` (each gated tick a skip, the asking snapshot's bytes
unchanged), `[status-sticky]` (canned and real filesystem failure; the flag holds across no-op ticks after Save and
after re-attach and across a canned success), `[held-unreadable]`, `[retire-fails]`, `[pending-save-as]`; the existing
cp3-cp5 retirement / stamp / Restore / refusal gates re-grounded on real scheduled writes (no planted snapshots), Save
and Save As advance the autosaved serial, a pending Don't Save does not, Discard leaves the edited tail due, and two
shell sections prove New-then-dialog-Cancel and Open-then-chooser-Cancel autosave the tail again. **Ten mutations bite**
(an autosaved edit rewritten, the pending gate or the held gate removed, a held snapshot retired by Save, a canned
success or a real failure mishandling the sticky flag, Save not advancing the serial - green at first, the test now
edits before the Save -, Don't Save advancing under a question, a cancelled replace not re-armed, a failed retirement
not flagged).
**Critic (separate, Opus 5.5, two rounds):** round 1 had no blocker and six should-fixes, all in (the hold, Don't
Save's honesty, canned success, failed retirement, real scheduled writes, a stronger sticky gate). Round 2 found one
**blocker, fixed**: the hold followed the user into a New project, which then never autosaved - any attach now drops it
(an open re-detects at once); gated, a project created after a held bundle autosaves and clears the flag. Also in: a
held bundle with a due edit raises `notDurable` and re-reports "Autosave paused: an earlier autosave could not be read
and is kept" on each due tick (it went silent after 5 s); gates for the overlay's Esc re-arm and a Don't Save whose
retirement fails. Three more mutations bite (the hold surviving an attach, a held due edit silent, Esc not ending the
attempt). **Parked (each an ADR-0068 owner call, not taken here):** an unanswered snapshot that stays unreadable keeps
that bundle's autosave paused with no in-app way out but Save As (a Discard-only path would change §5); a snapshot with
no question raised yet that one open cannot read is replaced by the next write (§5 is silent; the same lost-work class);
Open Recent's failed-open re-arm has no gate of its own.
**Unit 5 prepared (needs the desktop):** SS-6's `WaitAutosave` now reads `autosave.writes` **before** each edit (it read
after, and could miss the only due write at the 1 s cadence), requires `lastAutosavedEditSerial == editSerial`, and on
the linkable fixture asserts `writeMsPeak < 100` and `linked > 0` (non-vacuous: one write must carry assets). New
**Step 8** runs the real app over hydrated cloud placeholders the OS refuses to hard-link: `YesDawPersistenceCheck`'s
hidden `[cloud-drive-setup/convert/verify/cleanup]` cases hold `autosave_cloud_tests.cpp`'s own fixture across the
step (a fresh sync root under USERPROFILE, adopted back only by its exact name and parent); the app creates the project
there and imports real audio, the edit's autosave must copy every asset and link none, the copies hash true and are not
links, lost writes ask and Restore recovers, the user's audio is still the same hydrated placeholders, and the root is
unregistered and removed whatever happens (cleanup retries a held handle). Convert and verify accept only a canonical
bundle directly inside the root the state file names, directly under USERPROFILE, and read it read-only (ADR-0062's
inspection, never a normal open). Each close is preceded by a proven clean save (the window title). Run headless here:
setup/convert/verify/cleanup pass on a copied bundle, verify goes red on never-converted audio and on a damaged
autosave copy, no root left behind; the original `[cloud-placeholder]` gate still passes.
**Local:** full build, ctest **442/442**, Clang clean (26 translation units). **Not pushed, on purpose:** the close-out's writes-on
readiness rule - the first enabled-writer push waits for the three SS8 laps and Step 8 on the packaged candidate,
which need Dan's desktop (asked; no answer yet). **Unit 3's CI** (`9604299`, run 38191384767): nine jobs green, macOS
red only on `YesDawTimelineGpuCheck` (raw: max 44.4 ms, sustained 19.4 ms, 18 slow frames). Not excused - the renewal
still awaits Dan; unit 3 changed no renderer code (probe, model, tests). ADR-0069 cp2's gates all pass; its close waits
on that decision. **Recorded, not changed:** scale / loop / locate settings do not bump the edit serial (the bundle is
written at once either way; parked for ADR-0068's owner); `[status-sticky]`'s real failure is a plain file named
`autosave` blocking the directory, not read-only permissions (Windows does not refuse writes into a read-only
directory). **Next:** Dan's desktop answer → unit 5's drives → push units 4 and 5 and check CI; unit 6 (cp7: delete the
engine trigger) after.
**Found on the way (fixed in its own commit):** unit 3's `[write-cost]` gates were flaky under disk contention - they
assumed the write after the paused one is cheaper and that the paused one is the highest so far; four concurrent suite
runs failed 3 of 12 (an ordinary write took 600-850 ms against a 250 ms paused one). Now each sample is checked against
the running maximum and ordinary writes repeat (at most 20) until one is cheaper than the peak, which must leave it
standing: 0 of 20 contended runs fail, and both peak mutations (the peak as the last sample; the probe publishing the
peak as `writeMs`) still bite.

## 2026-10-11 — Close-out unit 3: the autosave writer's real cost and carries reach the probe

**What changes:** each autosave write that was due now records its own wall time (milliseconds, from the control-thread
writer), the session's highest, and how many assets it linked and copied; the probe's `autosave` block gains exactly
ADR-0069 §4's `writeMs`, `writeMsPeak`, `linked` and `copied`. A tick with nothing due keeps the last sample; a failed
attempt is one coherent sample (its own time, no carries kept). The shipped trigger stays off (unit 4 turns it on), so
the gates make a write due through a test-only seam: `YESDAW_UI_TEST_HOOKS` and `YESDAW_PERSISTENCE_TEST_HOOKS` are
defined for the whole `YesDawUiInputCheck` target (every translation unit, the shell sources it compiles included,
sees one definition); the shipped app has neither.
**Gates (`[write-cost]`):** through the model - nothing due writes and records nothing; a due write's time and two links;
a no-op tick keeps the sample; an impossible link makes a real copy (counted) and the next all-link write returns
`copied` to zero; a write that really takes 200 ms longer (a pause inside the timed write) becomes the peak and a cheaper
write leaves it standing; a failed attempt replaces the time with its own and carries nothing. Through the shell's own
UI tick at the drive's 250 ms cadence - the probe's four fields, a real copy, back to zero, and `writeMs` below
`writeMsPeak` after the slow write. Eight mutations bite (a no-op tick replacing the sample, the peak as the last
sample, copies unreported, a failed attempt keeping old carries or the old time, the probe dropping `writeMs` or
publishing the peak in its place).
**Local:** full build, ctest **442/442**, Clang clean; the hidden `[hardware-cost]` check run alone: ten 50 MB assets
autosave in **57.4 ms** (bound 100 ms). **Next:** unit 2's and this commit's exact-code CI; when both are green, ADR-0069
cp2 closes and unit 4 (the model-owned writer, writes on) begins.
**Critic (separate, Opus 5.5):** no blocker; in - the peak's retention and the failed attempt's time are now proven
deterministically (they bit only by timing luck before), and the JSON tells `writeMs` from `writeMsPeak`.

## 2026-10-11 — Close-out unit 2: Restore links swept audio back, or refuses before writing and says which

**What changes:** when an autosave is restored after lost writes, audio whose name the bundle's open swept away is put
back by a hard link to the autosave's file (no audio bytes copied; a copy, hashed, only where a link is impossible);
a name holding other bytes keeps ADR-0062's adoption (hash, temporary, rename - shared bytes never overwritten). Every
autosave asset is still validated in full before any name changes, and the rows and the unresolved marker are written
together only after every name is back. If an asset is missing or damaged in the autosave (or cannot be put back),
Restore changes nothing, the recovery question stays, and the status line names it the way the missing-audio question
does: `Autosave not restored: <clip name> is missing from the autosave (<detail>)`. **Unit 1 certified:** its normal
exact-code CI (`c6b2e8a`, run 38007691530) passed all ten jobs; ticked in the close-out checklist.
**Gates:** `[restore-after-sweep]` (a real lost-writes bundle swept by its own open: one name linked back, zero bytes
copied, recovery's hash reads only, the marker answered, an ordinary reopen hashes everything and passes);
`[restore-refuses-missing-asset]` (missing and damaged: refused naming the asset, and the earlier asset that needed a
carry is still absent - validation precedes every name change; bundle, snapshot, rows, marker and output unchanged);
`[restore-carries]` (copy fallback with its bytes counted; a whole foreign name left alone; a damaged name shared with an
older name replaced without touching the shared bytes; a link's or a copy's failed flush refuses before the rows);
`[restore-named-refusal]` through the shipped action (audio that only the autosave holds goes missing: named from the
asking autosave, question kept, marker and project unchanged, Discard still answers).
**Critic (separate, Opus 5.5):** one blocker, fixed - the new result field lacked a default initializer, which GCC and
Clang reject under `-Wextra -Werror` (MSVC is silent; the local Clang check confirmed all 75 findings were that one);
in - a name replaced by adoption is flushed and makes the directory flush run, the app gate's missing audio exists only
in the autosave (so only the asking autosave can name it), a copy-back's failed flush is gated, the after-sweep marker
check is no longer vacuous, attach drops the asking autosave's copy. Parked: when both slots fail, only the first
slot's asset is named; a valid previous slot is still restored when the live slot is missing an asset (unchanged).
**Local:** fresh full build, ctest **442/442**, Clang clean on every changed translation unit; eight mutations bite
(copy instead of link, validation skipped, flush failure ignored, a foreign name overwritten in place, the marker cleared
before the carries, the name dropped from the status line, the name taken from the open project, the question cleared on
refusal). **Not run:** the real-app drive batch - this change reaches only Restore's carry and refusal paths, which no
drive exercises until writes are on (unit 4/5's writes-on proof runs the full batch and SS-6); no layout changed, so no
new visual rubric. **Next:** exact-code CI on this commit; then unit 3 (the probe's write-cost fields).
**Found on the way:** this machine's `build-ci` had been deleted (today, not by this session) - reconfigured from the
`ci` preset; local-only evidence STATUS cites under `build-ci/` from 10-09 (CI logs, drive shots) is gone here.

## 2026-10-11 — SoundCraft reviewed: its design and checks are taken, not its code

**Now:** [storytold/soundcraft](https://github.com/storytold/soundcraft) (Rust/egui, 4 days old, a Pro Tools clone) was
compared and audited for reuse (two independent five-dimension reviews with skeptics, the second on Opus 5.5; its build,
its tests 533/540 on Windows, its demo bounce). Its code does not come over (Rust; its engine breaks ADR-0002; egui), but
its UI design, a set of features and a set of checks are worth rebuilding natively: the list, reasons and landing spots
are in [`docs/research/2026-10-11-soundcraft-takeaways.md`](docs/research/2026-10-11-soundcraft-takeaways.md), parked
in `docs/goals/parking-lot.md`. Recommended order (Dan's call): finish the Usable-song milestone, then a UI density pass
built from that list (an ADR first), then G4.8 plugins. Unit 1's normal CI (c6b2e8a, run 38007691530) is green on all
ten jobs; **next: close-out unit 2** (restore by link or refuse before writing).

## 2026-10-09 — Recovery unit 1: diagnostic complete; normal CI still required

**Now:** all ten jobs of instrumented CI `38006384224` pass on `687f986`. The four-line temporary
profiling switch is removed in pushed cleanup `c6b2e8a370f3b4559f5d177568954d7ad7f2284f`;
effective OFF configuration, the full restamped build and all seven focused GPU/version gates pass.
Normal exact-code CI `38007691530` is queued/running. Unit 1 remains uncertified,
autosave remains off, and dependent feature implementation has not advanced.

**Preserved failures:** close-out checklist unit 1 was pushed at `f1048a0f5d5a8dbe1ba9ae0da9188d5dfdbf5b9a`;
exact-code CI `38000812015` completed with eight jobs passing, macOS compilation failing and
Windows failing only the dense Timeline frame gate (**438/439**, 648.81 s). Windows measured
**17.2344 ms** sustained against 16.6 ms, maximum **19.7969 ms**, and **18** slow frames against
8 allowed; 336 visible / 20640 total clips, checksum **680587763889**. The raw failure is preserved
in `build-ci/ci-38000812015-windows.log`; no Windows timing exception exists. Investigation and
an independent critic take priority over dependent feature work. No renderer cause is established.
Linux passed **436/436 (210.76 s)** on this original code SHA.
The original macOS build failed when Catch2 tried to print libc++'s `__int128` filesystem timestamp
in the scratch-preservation assertion.
Correction `0b1c6b47387a4cc4128e01b4a519cab422cc0e8d` is pushed; its CI `38002137946` completed:
nine of ten jobs passed, including macOS **436/436 (303.31 s)** and Linux **436/436 (210.06 s)**.
Windows again failed only the frame gate (**438/439**, 580.95 s): **18.4398 ms** sustained,
**21.7346 ms** maximum, **19** slow frames against 8 allowed, same fixture and checksum.
The second raw failure is in `build-ci/ci-38002137946-windows.log`.
It was queued for the macOS correction before the Windows result and contains no renderer repair.
It compares the same native times inside parentheses, passing only a bool to Catch;
no cast, precision loss or production change. Fresh focused checks pass **7484/19** and Clang is clean.
A separate critic found no residual issue; replacement macOS CI now proves the compilation repair.
No timing exception applies and unit 1 is not yet certified.
The existing optional stage profiler now has prepared quiet-pass capture: Catch writes one owned
report, CTest clears it before the run and prints/validates it afterwards. Actual local diagnostic
checks pass **16/3**, sustained **6.0907 ms**, native image type 1; the fixture/checksum is unchanged.
Missing/stale/truncated report and failed-producer controls pass. Independent focused review found
one stale-hook bug when switching to pure-test builds; it was reproduced and repaired. Normal OFF
configuration has 439 tests, the original GPU command and no profiling definitions. Full OFF CTest
initially passed 434/439: five helper version checks found stale binaries. The full checkout has been
rebuilt with matching stamps and the replacement suite passes **439/439 (271.82 s)**. One reviewed temporary switch
enables profiling only for the Windows GitHub Actions GPU target. Diagnostic code
`687f986cb9f18a4b1c239b05e6a3190c800a4cce` is pushed; exact-code CI `38006384224` completed with all
ten jobs passing. Windows passes **439/439 (372.68 s)** in diagnostic mode: sustained
**10.1143 ms**, maximum **11.2174 ms**, zero slow frames, unchanged fixture/checksum and native image
type 1. On that sustained frame, clips cost **3.6355 ms** and context teardown **5.4496 ms**;
geometry/layout together cost **0.033 ms**. This passing profile does not attribute either earlier
failure or justify a speculative geometry/layout repair. macOS passes **436/436 (268.27 s)**.
This is not a renderer repair or clean CI timing credit. Independent interpretation agrees with
removing only the temporary switch and obtaining clean exact-code CI once. That four-line removal
is now local and separately reviewed. A formerly ON build returns to the fresh OFF default even under
`GITHUB_ACTIONS=true`: 439 discovered tests, original GPU command, zero profiling compile definitions,
inactive hooks and no diagnostic post-build action. The full restamped build and seven focused
GPU/version gates pass (**2.15 s** whole focused suite; not a frame measurement). Preserve the
older full-suite and app evidence's original identities; no renderer change or new app drive is claimed.
The measured diagnostic, plumbing controls, review repair and rebuilt normal suite are recorded in
[the Windows capture record](docs/evidence/2026-10-09-windows-frame-capture.json).
Recovery copies only database metadata into private owned scratch, applies existing migrations and
validators there, and hashes the original audio. It preserves source DB/WAL/SHM, audio and extra files,
retains concrete refusal reasons, and reuses the validated selection within each invocation. Restore
validates afresh after the question; its asset-carry behavior is unchanged until unit 2.

**Measured so far:** the source-preservation negative control failed before implementation. Final
Windows focused checks pass **7484 assertions / 19 cases**; the real hydrated-placeholder gate passes
**121 / 1**, and all four affected Clang translation-unit checks are clean. The reviewed-source full build
and CTest pass **439/439 (260.47 s)**; earlier source versions passed **438/438 (295.27 s)** and
**437/437 (260.72 s)**.
Review caught ambient temp-permission exposure: the new
Windows privacy negative control failed seven assertions before the fix. Private directory creation
now protects copied DB and actual SQLite sidecars; real create-denial and cleanup-denial tests pass.
POSIX creation also checks actual ownership/mode, and macOS rejects noowners mounts. Linux checks
pass on the original SHA; Linux and macOS runtime checks now pass on the correction. Unsupported mounts
and second-account access were not tested locally.

**Final app batch:** SS1-SS7 pass **512 assertions** on the reviewed, repaired binary (SHA256
`AFF49E96569AA63173F7448241ADEB2E17FF03785912441686399F02291AEF06`).
SS8 is **171/177**; all six failure lines exactly match the earlier autosave-off baseline. Local logs/shots
are under `build-ci/session-shots/2026-10-09-recovery-reviewed/`. SS1/SS7 paint p95 is **6.98/4.98 ms**;
both editing bursts report zero underruns and deadline misses. This is the Realtek Stereo Mix profile,
not proof that the earlier ASUS capture delivery gaps are fixed. All six original audio defaults are
mechanically verified restored and Stereo Mix was disabled after the batch. The earlier pre-repair batch
remains under `build-ci/session-shots/2026-10-09-recovery-read/` with its actual different executable identity.

**Visual rubric:** separate agent review found no new FIX in the recorded binary. (1) overlap/clipping:
PASS in three SS1 sizes and inspected SS7 states; (2) eight-track density: prior unchanged-UI G6 coverage,
not re-proved by these one-to-three-track shots; (3) labels/tooltips: visible labels PASS, exhaustive
tooltip coverage inherited; (4) readable type/contrast: observed PASS, numerical token gates inherited;
(5) selection/playhead/hover/pressed/focus: observed PASS, loop appearance inherited; (6) reference
structure: PASS; (7) no fake data: PASS for inspected frames coupled to drive logs. Full matrix evidence
is not renewed. Final-binary addendum: `build-ci/recovery-unit1-reviewed-visual-review.md`; the earlier
`build-ci/recovery-unit1-visual-review.md` is preserved separately.

**Review repairs:** completed CE review `20261009-recovery-unit1-review` confirmed two issues. Both
are repaired and the separate critic reports no residual finding by inspection. Cleanup failure now
retains scratch ownership and stops fallback, preserving the newer slot and caller outputs. Its real
Windows deletion-denial negative control failed **20 assertions** before the fix; repaired public
read/Restore checks pass **286/2**. macOS now removes and verifies inherited ACLs on its owned empty
scratch directory before copying; its inherited-ACL characterization and privacy fixture now pass macOS CI.
Final focused Windows checks pass **7484/19**, and actual cloud-placeholder checks pass **121/1**.

**Next:** the three simplification reviews found no worthwhile changes. Local checks, final app drives,
separate visual addendum and bounded review-repair critic are complete. Parsed measurements and source,
executable and log hashes are in [the recovery-read record](docs/evidence/2026-10-09-recovery-read.json).
The single instrumented observation and all ten jobs are complete; preserve both normal-mode failures.
Inspect all ten exact-code jobs of normal cleanup CI `38007691530`. No speculative renderer repair is
supported by this passing observation. The rebuild restamps the app without changing production C++ or UI;
earlier app drives retain their actual binary identity and are not relabeled as fresh drive evidence.
Only after green CI, advance to unit 2: restore missing audio names by link and report named refusals
through the shipped Restore action. Autosave remains off; the Usable-song milestone is incomplete.
No app drive is running. Dan's hands-off authorization remains active for this session.

## 2026-10-09 — Normal-build CI passes; cheap autosave cp1 complete

**Now:** all ten expected jobs of normal-build CI `37988272626` passed on exact code SHA
`3106943fab162a68aea9e8d74d0c4cbb851d2173`. macOS passed **426/426 (288.30 s)**, including the unchanged
dense Timeline frame gate. Profiling is default-OFF and no timing exception was used. The quiet passing log
does not report an exact sustained frame time; its 2.46 s GPU result is whole-test runtime, not frame latency.
This closes the rendering interruption and ADR-0069 cp1, together with the local, app and independent-review
evidence recorded below. The docs successor `008c286` also passed its docs-only run `37988465906`.

**Evidence:** [checkpoint record](docs/evidence/2026-10-09-cheap-autosave-cp1-closeout.json) binds the ten job
results to the code SHA and preserves the raw macOS log hash. SS1-SS7 passed 512 assertions; SS8 remains
171/177 with the six known autosave-off failures. The real hydrated-placeholder persistence gate passed;
the real-app placeholder drive remains pending before writes-on. Earlier rejected timings remain below.

**Next small unit:** read recovery sources without changing them (close-out checklist unit 1 / ADR-0069 cp2).
Clone only metadata into owned scratch for migration and validation, hash original audio, preserve detailed
refusals and source bytes, and reuse the validated selection within each read/restore call. Then restore-link
carry, write-cost probes, and ADR-0068's model writer follow as separate green checkpoints. Autosave writes
are still off; the Usable-song milestone is not complete. Dan's hands-off app-test authorization remains active
for this session. Current audio defaults have been restored and no app drive is running.

## 2026-10-09 — Diagnostic CI complete; normal build running in CI

**Now:** the clean rendering correction `dd1ab7304e3689bc429b456be739ef43f103a550` completed exact-code
CI `37977947876`: nine jobs succeeded; macOS failed only the dense Timeline sustained-frame assertion at
**17.740833 ms** (16.6 ms limit), maximum **23.9218 ms**, **16** slow frames against **8** allowed. Its other
425/426 cases passed. The docs successor `d496fdf` also passed its docs-only CI, which supplies no code proof.
No timing exception was renewed. ADR-0069 recovery/probe cp2 and ADR-0068 writes-on remain dependent on
required checkpoint proof; the Usable-song milestone is incomplete.
The reviewed diagnostic is pushed at `5454b39f1c5a2f76525a1e7f6f77dce1331531bd`; exact-code CI
`37983993880` is complete: **all ten expected jobs succeeded**. macOS passed **426/426 (267.97 s)**,
including the dense frame gate. Passing output is quiet, so it emitted no stage summary, exact sustained time,
native image type or fixture checksum. The record leaves those fields unavailable. Its diagnostic binary SHA256 is
`80429e359ff2efbd9ecdb9148ab4ff4504e1c5c5fac187985f6d25db3a22f3ed` on macOS 14.8.9 / image
`macos-14-arm64` `20260831.0302.1`. No job from this instrumented run is clean macOS timing certification.

**Diagnostic only:** a target-private compile definition adds seven painter timestamps and two Graphics-context
markers solely to `YesDawTimelineGpuCheck`. Original fixture, outer timer, pixel sampling, checksum, assertions,
budgets and CTest invocation are unchanged. Records are preallocated before warmup and copied after the timer;
aggregation reports instrumented stage costs and the same frame selected by the existing sustained statistic.
The build log records the diagnostic executable SHA256. Temporary APPLE auto-enablement is for one measurement
and must be removed before clean certification. App and packaged-checker compile guards reject profiling.

**Local verification:** OFF app/packaged/GPU builds and the GPU gate passed. ON passes **16 assertions / 3 cases**;
OFF/ON dense output has the same checksum **680587763889**, 336 visible / 20640 total clips and 300 distinct samples.
The preprocessed OFF painter and measurement-loop bodies match the clean baseline exactly (1084 / 513 tokens).
All 426 OFF compile commands are macro-free; all nine ON GPU commands carry the macro and no other target does.
OFF three-TU and ON GPU-TU Clang checks are clean. Full local Release build and CTest **428/428 (284.42 s)** passed.
Profiling is OFF again locally. Independent correctness/adversarial review is complete with no findings
(`20261009-macos-stage-diagnostic-final`); final source hashes match its receipt. Simplification reused JUCE's
allocation-free scope guard and made diagnostic include dependencies explicit. Existing SS1-SS7 / 512-assertion
app evidence applies to the unchanged production painter; this diagnostic claims no new app drive or visual proof.
Parsed local measurements and identities are in `docs/evidence/2026-10-09-macos-stage-diagnostic.json`.

**Cleanup pushed:** `3106943fab162a68aea9e8d74d0c4cbb851d2173` is running in exact-code CI `37988272626`.
Automatic APPLE enablement is removed; the default-OFF option is now the only path to
profiling. No C++ source, fixture, assertion, threshold, workflow or production painting changed. Fresh local
configure/build/GPU CTest and OFF body/compile-command isolation passed. Independent correctness/adversarial
review is complete with no findings (`20261009-macos-profile-cleanup-critic`). The prior full 428-case suite covers
the same C++ source. This is the planned return to normal mode, not a repeated diagnostic run seeking failure.

**Next:** wait for every expected job of normal-build CI `37988272626` on its exact code SHA.
A clean result is still required before dependent feature
work. The quiet instrumented pass supplies no attribution and will not be repeated to seek a failure. No new
renderer repair hypothesis is claimed from it.

## 2026-10-09 — Cheap snapshot implementation passes locally; earlier app verification interrupts advancement

**Now:** ADR-0069 cp1 is implemented and separately reviewed, but not certified. Restore/probe cp2 and ADR-0068's
writes-on remain next, after the current checkpoint's required evidence. Dan authorized hands-off app tests for this
session. The first full drive batch exposed an earlier visual defect and an audio-device gate failure; these take
priority under plan section 8.2.
The measured paint correction is pushed at `dd1ab73` (CI `37977947876`, now running). Independent
preparation is recorded in [the close-out checklist](docs/plans/2026-10-09-cheap-autosave-closeout.md); it preserves
the active plan and ADRs. Its critic's sole ordering finding is fixed: separate cp7 cleanup precedes final sign-off,
without blocking cp6. No dependent feature implementation has started.

**Cheap snapshot:** links immutable audio, copies on link failure, checks linked identity and copied hashes, and keeps
full recovery validation. Five new default gates cover identity/zero copied bytes, 200-write link counts, fallback,
failed publication, and preserving other names of a replaced shared inode. Local Release build, Clang on the three
affected translation units, and full CTest **428/428** passed. The actual 10 x 50 MB hidden cost check passed at
**72.0686 ms** against 100 ms (46 assertions). Separate focused/adversarial review found no issue. These are local
working-tree results, not exact-SHA CI certification. The latest full CTest took 289.67 s. A real Windows
hydrated-placeholder gate passed 121 assertions, including the actual OS hard-link rejection, copied/hash byte
counts, recovery, mode restoration and cleanup; its Clang pass is clean. This proves that fallback path, not
OneDrive/Dropbox synchronization or a real-app drive. Core persistence is pushed at `2c8c7bd`, exact-code run
`37971282150`. The cloud fixture is pushed at `8108cda` (run `37971433126`); independent correctness/adversarial
review is complete with no findings (`20261009-cloud-fixture-critic`). Neither closes the checkpoint. GitHub's
concurrency group superseded the core run while it was pending despite preserving running main jobs. Requeued
the core run first, then the cloud run after core started. Core is complete: nine jobs succeeded, macOS failed only
the frame bound detailed below. Cloud `37971433126` is also complete: nine jobs succeeded; macOS failed only
the same frame assertion at **22.137125 ms** sustained (maximum 30.7078 ms, 48 slow frames; other 425/426 cases
passed). Repair `37977947876` is running. No queued,
cancelled or docs-only result is code certification.

**App batch:** `build-ci/session-shots/2026-10-09-adr0069-cp1/` (local only). ss1 59/59, ss2 42/42, ss3 57/57, ss4 22/22,
ss5 77/77 and ss6 58/58 passed. ss7 **196/197**: four driver-reported capture discontinuities during the pointer sweep;
zero measured callback deadline misses, maximum callback 1.3001 ms, paint p95 4.89 ms. ss8 **171/177**: exactly the six
known autosave-off assertions in step 6. The app still writes no autosave; no missing recovery evidence is certified.

**Markers repair pushed at `2bd0b71`:** exact-code CI run `37971162055` completed: nine jobs passed; macOS failed
only `YesDawTimelineGpuCheck`'s sustained-frame assertion, **24.162042 ms**, with its other 420 CTest cases passing.
No exception renewal was approved and this red is not certified. The large ss1 screenshot's
unlabeled 244 x 120 box is the real Markers list. The Takes
card painted over the Markers heading. Bound the Takes card at the existing Markers section in shared layout/paint;
new `[inspector-markers-card]` checks actual heading pixels with zero/two rows and at the whole-section fit boundary.
The old paint fails with zero heading pixels; the repair passes 69 assertions, and four focused inspector cases pass
281 assertions. Two-TU Clang is clean. Separate correctness/adversarial review found no issue
(`20261009-104633-inspector-45be84c7`). Independent review of the fresh app screenshots at all three sizes passes
the relevant rubric: Markers is readable at 2560 x 1440 and lower cards drop whole at smaller sizes. This is not a
new full scaling-matrix review. The repaired SS1 run is 58/59, with only B5 failing (nine capture discontinuities).

**Audio diagnosis (not a waiver):** current defaults are Realtek Speakers + ASUS AI Noise-Canceling Microphone.
Pinned JUCE WASAPI's xrun counter counts input discontinuity flags, not specifically output underruns. A direct
30-second WASAPI capture measured 3,000 packets, only its first packet discontinuous. An empty app measured two
capture counts while stopped and one while playing (15 seconds each), no callback deadline misses. The cause remains
unresolved. A copied-JUCE metadata-only input/duplex comparison then measured real 384-frame gaps in all three
30-second phases (8, 1, 29 post-startup discontinuities); no repeated packets, full reservoir, or zero release.
The UI/engine are not needed to reproduce it, but event delivery versus scheduling is not resolved. Instrumentation
influence is not excluded. Local evidence: `build-ci/wasapi-duplex-diagnostic/REPORT-20261009.md` and hashes.json.
The original input default (all three roles) is ASUS; output is Realtek. Stereo Mix was disabled. It was temporarily
enabled/defaulted for unchanged app checks. On this profile SS1 B5 passes with zero xruns since launch, but B2 failed
at 8.20 ms (8 ms bound), so SS1 remained 58/59. SS7 passed 197/197 (zero xruns since launch, paint p95 4.83 ms).
After the final batch below, all six default endpoint roles match the originals and Stereo Mix is disabled again;
the Core Audio inventory confirms restoration. This alternate profile proves its own results, not an ASUS repair.
No threshold/counter changes or runner-noise waiver. Logs/shots under `build-ci/` are local only.

**Next small steps:** inspect every job of the rendering correction's exact-code CI. If it remains red only on
macOS timing, collect a bounded CoreGraphics stage profile before another implementation hypothesis; no timing
exception has been renewed. The prepared diagnostic remains inactive and changes no production code or gate.
Restore/probe and writes-on remain dependent on
required checkpoint proof. The Usable-song milestone remains incomplete.

**B2 repair selected under ADR-0067 (2026-10-09):** the unchanged SS1 failed twice at 8.20 and 8.08 ms. Metadata
traces show the rolling window correctly retaining expensive edit frames. Temporary stage timing measured panel
painting at 2.14-2.50 ms, toolbar/ruler/grid at 1.32-1.64 ms, and clips at 0.39-0.48 ms. Most panel pixels are then
covered by opaque toolbar/ruler/grid fills. Skip only these guaranteed-covered interiors, preserving a logical-pixel
fringe and every original visible drawing operation. This is clip-scoped painting within ADR-0067, with no token,
cache, invalidation, measurement-window, or threshold change. Alternatives rejected for this repair: wider partial
edit invalidation and another cache layer. Gates: old/new pixel equality including fractional scaling, existing
canvas/cache/golden checks, full CTest, unchanged app journeys and independent review. Diagnostic timings alone do
not certify a repair; the temporary timing code was removed before implementation. The implementation's focused
cache gate passes 174 assertions in four cases, including 48 old/new pixel comparisons and a control that detects
unnecessary painting underneath opaque fills. Final Release build and full CTest passed **428/428 (289.00 s)**;
two-TU Clang is clean. Separate correctness/adversarial review is complete with no findings
(`20261009-timeline-occlusion-critic`). The uninstrumented app batch in
`build-ci/session-shots/2026-10-09-panel-occlusion/` passes SS1-SS7: **512 assertions** (59/42/57/22/77/58/197).
SS1 paint p95 is **5.82 ms**, SS7 **4.78 ms**, both zero xruns since launch and zero measured callback misses.
SS8 remains **171/177**, exactly the six known autosave-off assertions. Agent visual judgment passes the scoped
rubric at all three window sizes at 150%; unchanged timeline/inspector pixels, apart from live timestamp/LUFS text.
This is not a new full scaling matrix. Local app SHA-256:
`8AF4FD4F4F7695DAD4AAEC1C57181B3F6A3F8E6277D8B2D63EA2C661D39833CE`; the artifact is a working-tree build,
not an exact-commit CI package. No timing instrumentation, threshold, golden, or CI workflow change remains.
Parsed drive summaries, measured counters, source hashes and verified endpoint restoration are in
`docs/evidence/2026-10-09-panel-occlusion.json`. The simplification pass changed only an equivalent rectangle
construction in the test; the final 174-assertion gate and test-TU Clang passed again, and the review addendum
confirms the verdict remains unchanged. Core run `37971282150`'s macOS job also completed red only on the frame
bound: sustained **23.787291 ms**, maximum 32.1861 ms, 83 slow frames; other 425/426 cases passed. This predates
the panel repair; all nine other jobs passed, including Windows. No noise exception is applied.

## 2026-10-09 — Dan accepted ADR-0069: resume the cheap autosave checkpoints

**Now:** Dan explicitly approved hard links, including the shared-audio consequence, for ADR-0069. The remaining
Usable-song work resumes: (1) carry autosave assets by link and validate identities, (2) restore safely and expose
write measurements, (3) enable ADR-0068's edit-serial autosaves, then (4) obtain SS-6, packaged and hardware evidence
before certifying the milestone. The accepted recovery and audio-thread contracts stay in force.
**Verified at session start:** main is current and clean at aba1819; exact code SHA cb0bc2e's CI run 37879082179 passed,
and aba1819's docs-only run 37881898052 passed. The parked `.git/yesdaw-wip/adr-0068-cp1-cp2-writes-on.patch` is absent
on this machine; reconstruct that step from the accepted ADR and tests after the cheap snapshot gates pass.
**Pending:** macOS GPU exception renewal has not been approved. Current CI is green without it. Real-app drives need
a verified isolated input surface or a current hands-off window; missing evidence is not certification.

## 2026-10-08 — G6 close-out recorded: every item in; certification waits only on SS-6 (ADR-0069, Dan)

**Now:** every G6 item is in (G6.1-G6.5) and the close-out evidence is committed:
[record](docs/evidence/2026-10-08-g6-closeout.json) · [montage](docs/evidence/2026-10-08-g6.png). The rubric ran across
the whole scaling matrix (20 cells) and the real app; its two FIXes are fixed and gated (cb0bc2e). Drives on cb0bc2e's
exe: ss1-ss7 green (509 assertions); ss8 (SS-6) 171/177, its six reds all the autosave step. cb0bc2e's CI: green on
every job, macOS's GPU frame check included.
**Blocked on Dan (one decision unblocks G5, G6 and the Usable-song milestone):** ADR-0069 - let each autosave *link*
the project's audio files instead of copying them (~11 ms instead of 3-5 s of UI freeze per autosave on a 500 MB
song; what the autosave protects is unchanged). Recommended: **accept**. Then ADR-0068 cp1/cp2 (writes on, built and
parked) land, SS-6 goes green, and G5/G6 certify. Also his: the macOS GPU frame exception renewal (recommended: renew).
**Next (agent, meanwhile):** nothing in the active milestone is left that does not need that answer; parked ideas stay
parked.

## 2026-10-08 — G6 close-out rubric: two FIXes found and gated (a partial row's rail header; covered clip names)

**The rubric pass (plan §7.4, agent judges):** the scaling matrix rendered at every cell (the plan's 3 window sizes x
100/125/150/200 % and the 8 maximized-display cells) plus the real app's shots (ss1's tip at three sizes, ss1, ss5,
the song fixture) - five judges, each FIX claim then checked by a separate verifier against the pixels. Two real
defects, both confirmed, both fixed here with a gate that is red without its fix:
- **A partial lane beside an empty rail** (rubric 1; the 1080p maximized window at 100 % among others): the arrange draws
  the lane the panel cuts off at its bottom, the rail stopped at the last whole row - clips with no header beside them,
  and the rail's empty space there still took clicks for that row. Now the rail paints the row's visible part (its whole
  geometry clipped to the panel), and only its controls wholly on screen are records. Gate `[rail-partial-row]`: over a
  height sweep with 16 tracks, every lane shown 40-71 px has its row's text in the rail beside it; every rail record lies
  inside the rows. Two mutations bite (the old break; a cut-off control's record).
- **Covered clip names printing into each other** (rubric 1; "MIDIAudio Clip" where the song fixture's MIDI clip lies
  over audio clips on one lane - clip bodies are translucent): a clip's name is now hidden where a clip drawn later
  overlaps it (the later is on top, as in Logic). Gate `[clip-names]`: a lone name prints; covered wholly - none shows
  through; covered on the right - its name still prints at the left and nothing inside the cover; two named clips over
  each other show only the top one's. The mutation bites. The canvas the GPU frame check times is unchanged in cost:
  locally 5.66 / 5.95 / 5.56 ms sustained before, 5.53 / 5.68 / 5.35 after.
Refuted or not defects: none needed refuting; one "FIX" was a shot path the judge was given that the drive does not
write (ss7 names its shots by step).
**Critic:** no blocker; in - a control the partial row cuts off no longer responds (the rail's hit-test keeps the
records' law: only a control wholly on screen is operable; a click on the visible part of a cut-off M cell changes
nothing - gated, and the mutation bites), and the "two named clips" check no longer compares text-pixel counts across
backgrounds (font rasterisers differ off Windows): a long covered name must not run on past a short top one.
**CI:** cp4b (d3f270d) green on every job, macOS's GPU check included - **cp4b certified**; 2aa46c9's Windows GPU red
did not repeat on its re-run; cp5 (19cbc14) green but macOS's GPU frame check (raw: max 25.5 ms, sustained 20.2 ms, 29
slow frames - the exception awaiting Dan).

## 2026-10-08 — ADR-0073 cp5: SS-1 meets the first-run tip on the real app - G6.5 done

**Now:** **G6.5 is complete** (ADR-0073 cp1-cp5, ADR-0074). Every G6 item (G6.1-G6.5) is in. **Next:** the G6
close-out (plan §8.5: the rubric across the scaling matrix, the montage, the evidence record). **G6 and the Usable-song
milestone cannot be certified yet:** G6's exit keeps SS-1-SS-6 as gates, and SS-6 (`ss8`) still fails only its
autosave step - ADR-0068's writes wait on ADR-0069, which waits on Dan (one point: linking the project's audio into
the autosave instead of copying it; recommended: accept). The macOS GPU frame exception renewal also still awaits him.

**What changed:** `ss1-first-minute.ps1` launches as a first launch (`Launch -FirstRunTips`). Step 1's second half: the
tip shows under the ruler; its Dismiss and the empty Arrange's Import audio row are controls (the startup project has
one empty track, so ADR-0074's row - ADR-0073's cp5 text named the no-tracks Add track row, amended by ADR-0074); the
tip at 1280x720, 1920x1080 and 2560x1440 (the rubric's shots); the real Tab key reaches the Import row and Enter opens
the import chooser (cancelled - Step 2 imports by the plan's chord); then Step 2's first clip dismisses the tip and
`prefs.json` lists it. **Drives (real app, this commit's exe, desktop free):** ss1 59/59, ss2 42/42, ss3 57/57, ss4
22/22, ss5 77/77, ss6 58/58, ss7 197/197, ss8 171/177 - its 6 reds are all Step 6, autosave writes off
(`autosave.writes=0`: no confirmed autosave, so no recovery to prove), the known ADR-0068 state; none touches the tip
or `prefs.json`. **Visual check (agent):** the three tip shots - the strip legible under the ruler, the Import row centred
below the lane, the inspector's Add MIDI clip row, nothing overlapping: PASS.

## 2026-10-08 — ADR-0073 cp4b: the first-run tip under the ruler

**What changes (visible):** a first launch (and every empty arrange until it is dismissed) shows one strip just under
the Arrange ruler: `Tip: drop audio files onto the arrange to import them, or open the browser  (Y)` with **Dismiss** at
its right end. The chord is the browser's live one (else its menu path). Dismiss (a click, or Tab + Enter) hides it
for good and writes it to `prefs.json`; so does the first clip that appears (imported, dropped, recorded, drawn, or a
project with clips opened - ADR-0074 §2), and undoing that clip does not bring it back. Help > Show Tips Again brings it
back for the next empty arrange (asked with a clip on screen, it is not undone by that clip). It never shows while the
autosave recovery question is up (the card wins), never over a clip (it needs every lane empty), and the Arrange's rows
sit below it. Too narrow for the sentence and Dismiss on one line, it is absent, not clipped. Nothing starts under the
strip but Dismiss: no lane gesture, no lane menu, no double-click. Dismiss is a real control: hover and pressed strokes,
an accessible button whose help text is the tip itself. Session drives write `prefs.json` with the tip dismissed for a
fresh session unless a step asks for it (`Launch -FirstRunTips`); the probe adds `layout."tip.strip"`.
**Gates:** `[g65][tip]` - the lifecycle across two launches on one `prefs.json`, the element, the hover stroke, Tab +
Enter, a click on the sentence (nothing) and on Dismiss, right clicks on and below the strip, and the canvas layout
(whole or absent at every width; rows below it at every height); the recovery card wins (`[project-lifecycle]`);
`[g65][contrast]` (new, closing a cp2 gap) - every row's noun and how, the tip's sentence and how, and Dismiss, as
rendered, reach 4.5:1 on the surface under them, and the disabled ink on each row's surface; `[g65][no-obscured]` runs
a second pass with the tip: the strip and Dismiss meet no record and no visible widget at any size. 13 mutations bite
(one survived at first - rows centred onto the strip on a short canvas - and gained its layout check).
**Critic:** no blocker; fixed - a right or middle press on the strip reached the lane under it (the strip now takes
every press first). Not changed: the New Project overlay (launch opens the last project or an Untitled session, never
the overlay - ADR-0073 keeps that launch flow - and the overlay is raised above the strip). **Visual check (agent, the
real app):** a fresh launch at 1280x720, 1920x1080 and 2560x1440 shows the strip legible under the ruler, the Import
row centred below the lane and Dismiss's hover outline under the real pointer (drive 8/8). **Local:** build, 423/423
ctest, Clang clean.
**CI so far:** cp3 (26d0587) green but macOS's GPU frame check (raw: max 24.8 ms, sustained 20.0 ms, 42 slow frames).
cp4a (2aa46c9): macOS GPU only again (27.7 / 21.1 / 49) - and a second **Windows** GPU-frame red (max 23.1 ms, sustained
17.5 ms against 16.6, 13 slow frames). Not excused; the evidence so far says runner variance, not our renderer: the
measured path (`paintTimelineCanvas`, all the check calls) has no change since the last steady-green Windows run - the
row and tip code is additive and drawn by the input component; CI runs ctest serially (no self-inflicted contention);
every run used the same runner image (20260925.250.1); and eda3e2f's red and its green re-run ran in the same region
(westus3) on byte-identical code. 2aa46c9's Windows job is re-run on the same SHA; a third red on a re-run would call
for an agent critic and a new hypothesis (the plan's three-attempt rule).

## 2026-10-08 — ADR-0073 cp4a: dismissed tips live in prefs.json; Help > Show Tips Again

**What changes (visible):** the Help menu gains **Show Tips Again** (after Keymap, no chord), enabled only while a tip
is dismissed; it brings every dismissed tip back. Nothing shows a tip yet - the strip itself is cp4b. `prefs.json` gains
`"tips": { "dismissed": [ ... ] }` (ADR-0061: a file without it reads as nothing dismissed and is written back without
loss; ids this version does not know are kept; a malformed entry falls back alone). The native app shows first-run tips;
the headless harness starts with the welcome tip dismissed (in memory, and again after any read of `prefs.json`), so no
earlier gate meets one. The probe reports `prefs.tipsDismissed`. **Gates:** `[prefs][g65][tip]` - the read, the write at
dismissal, a relaunch, Show Tips Again (enabled only with something to bring back), unknown ids and a newer version's
keys kept, a malformed list rejected alone, and the harness's start - four mutations bite. The Help menu is re-pinned at 2 items, the keymap
doc gains the unbound action, and the accessibility gate's fully reachable context gains a dismissed tip (as it holds a
solo for Clear All Solos). **Local:** build, 423/423 ctest (after that one fixture line), Clang clean.
**CI evidence (eda3e2f):** the Windows job re-run on the same SHA passed 423/423 - its one GPU-frame red (code identical
to dfbeadc's for that binary) did not repeat.

## 2026-10-08 — ADR-0073 cp3: the inspector and the piano roll offer Add MIDI clip when they are empty

**What changes (visible):** the inspector's Clip tab with no clip selected, and the piano roll with no MIDI clip open,
show `Add MIDI clip  (Clip > Add MIDI Clip)` (it has no chord, so its menu path) while the project has no clip of that
kind - a real control (click or Tab + Enter adds the clip and opens it); with clips but none selected the inspector says
"Select a clip" (no single action is honest there); with no tracks both say "No tracks yet". "No track" (track inspector)
became "No tracks yet", the browser's "Nothing here" became "No files here", and the roll's header no longer says "No
MIDI Clip selected". ADR-0073's "Select a track" rows are not built: Add MIDI Clip needs no selection (it picks a track),
so that state never occurs. **Bug found and fixed on the way:** after an undo removed a MIDI clip the inspector kept
showing that clip's quantize panel (its id outlived it); the panel now needs the clip to exist (gated - red without the
fix). **Gates (`[g65]`, 7 cases):** each panel's row and text, its element, Tab + Enter and a click on the inspector row,
a click on the roll's row, the plain states (an audio song just opened: "Select a clip", no control), no-tracks plain
text, and the old strings gone from `src/`. Five mutations bite. **Visual check (agent):** a new project with the roll
open shows the three rows - Arrange, inspector, roll - each legible in its own empty surface.
**Critic:** no blocker; in - a pointer resting on each row (Arrange, inspector, roll) is gated to hover it through the
shell's own child walk (as JUCE routes an event), the no-obscured sweep also covers list boxes and labels and runs with
the piano roll open, the unreachable "Select a MIDI clip" branch is gone (the roll opens a MIDI clip whenever one
exists), the menu table is built once, and a comment pins why the roll's row never sits over its keyboard column.
The full suite then caught three older pointer gates (ADR-0067/0072) that now meet the inspector's row: their rig sent
an `empty.*` record's events through the instrument panel; it now routes each to the surface that paints it.
**Local:** build, 423/423 ctest, Clang clean. **CI on the way:** cp2's commit (dfbeadc) failed the macOS build - Apple
Clang rejects capturing a structured binding by reference (`-Wunused-lambda-capture`); corrected in eda3e2f (an
init-capture). eda3e2f's CI: macOS builds and fails only the GPU frame check (raw: max 32.4 ms, sustained 23.6 ms, 57
slow frames - the standing exception still awaits Dan's renewal); **Windows** also failed the GPU frame check once (max
23.3 ms, sustained 18.2 ms, 18 slow frames) on code byte-identical to dfbeadc's for that binary (eda3e2f changed only
`tests/ui_input_tests.cpp`, which it does not build) where Windows was green - not excused: the Windows job is re-run on
the same SHA as evidence.

## 2026-10-08 — ADR-0073 cp2 + ADR-0074: the empty Arrange offers its next action

**What changes (visible):** the Arrange no longer says "Create or open a Project" (which it said even with a project
open) and the rail's "No Project" is gone. A **new project** (it starts with one empty audio track) shows
`Import audio  (Ctrl+Shift+I)` centred in the empty lanes; a project whose tracks were all deleted shows
`Add audio track  (Ctrl+Shift+N)` and `Import audio  (Ctrl+Shift+I)`; with no project at all, `New project  (Ctrl+N)`
and `Open project  (Ctrl+O)`. Each row is a real control: a click or Tab + Enter runs the action exactly as its menu
item does, a screen reader hears it as a button with the chord as its help, hover shows its outline; the chord shown is
the one that works there (else the menu path). The first clip makes the rows give way to the lanes.
**ADR-0074 (accepted, critic folded in):** ADR-0073 had assumed a new project has no tracks; it has one (the Default
template), so the state a new user actually sees gets the Import row and the first-run tip (cp4) will last until the
first clip. **Gates:** `[g65][empty-rows]` (each state's rows and text; records and elements; Tab order after the tools;
Enter, the named chord and a click run them; a click beside the row is the lane's; a MIDI clip alone also clears them),
`[g65][no-obscured]` (every size x dock: inside the lanes, no record or widget met; all-or-none by the canvas's layout
over every height), the old prose gone from the source. Six mutations bite. **Visual check (agent):** renders of the
new-project, no-tracks and hovered states read cleanly.
**Critic:** its blocker was ADR-0074's own rationale - lanes keep a fixed height (G0.7) and never fill the area, so the
row now sits in the free space **below the last lane** (never over one; gone once empty tracks leave no room - gated by
adding tracks until it goes); also in: a middle click no longer runs a row (gated), the old-prose grep covers all of
`src/`, one timeline state per record collection, two invariant comments.

## 2026-10-08 — ADR-0073 cp1: how an empty-state row's action is taken (the chord that works here, else the menu)

**What (no visible change yet):** `src/ui/EmptyStateRow.h` resolves, for an action, the chord to show - only if the
router, asked with the current focus, maps that chord back to the action - else `Menu > Item` from the menu bar's own
data, else nothing; the shell feeds it its real menus and focus. Painted controls gain `enabled` (a disabled one's
keyboard activation and accessible press do nothing; its element is disabled) and a `description` their accessible
element reports as help. **Gates:** a pure resolver test (the default chord; Del, which the piano roll owns, names
`Edit > Delete Clip` there; Down works everywhere; rebind to F4; unbind -> `Track > Add Track`; unbound and in no menu ->
nothing) and a shell test that reads the menu bar the shell builds (an unbound action and a focus-shadowed one name
real menu items). Two mutations bite (a focus-blind resolver; the path from the accessible name). **Caught on the way:**
the ADR's example "the piano roll owns Down" was wrong (Down selects tracks there too) - an erratum line is added; Del is
the real shadowed chord; and Delete Clip lives in the Edit menu - the path comes from the menu data, never typed.
**Critic:** no blocker; in - the router takes some keys before the keymap (the Control target's Tab / Enter / Esc /
arrows while adjusting, a plain letter while musical typing is on, plain Left / Right during step input), so the resolver
now asks the router's own law with its live state and names the menu path for a chord it would swallow (three more
mutations bite); every action a row names is gated to resolve to a chord or a path (none sits only in a submenu). The
painted controls' `enabled` / description reach the shell's elements through real rows in cp2, where they are gated.

## 2026-10-08 — ADR-0073 accepted: empty states name the next action; one first-run tip (G6.5)

**Decided (design workflow: three proposals - Logic-minimal, actionable cards, a help surface - two judges, a synthesis;
then a critic whose two blockers and five should-fixes are folded in):** an empty surface (Arrange with no project or no
tracks, inspector and piano roll with no track or no clip) offers its next action as one **row** - `Add audio track
(Ctrl+Shift+N)` - a real control (Tab, Enter, click, screen reader, hover/pressed) that names the chord that works
where it is, else the menu path read from the menu bar (`Clip > Add MIDI Clip`); one first-run **tip** under the ruler
(drag-and-drop and the browser) with Dismiss, gone for good once dismissed or once a track exists, persisted in
prefs.json, back via Help > Show Tips Again; nothing is obscured at any size. Rejected: cards on every empty surface, a
permanent help band, Esc to dismiss, the New Project dialog on first launch (a launch change beyond G6.5). CONTEXT.md
gains **Empty-state row** and **Tip**. **Next:** ADR-0073 cp1 (the chord/menu-path resolver).

## 2026-10-08 — ADR-0067 cp3c: the real pointer sweeps every control while the song plays (SS-5 drive)

**What:** `ss7` Step 17 plays the song and moves the real Win32 mouse to the centre of every rail, strip and master
control the shell keeps a record for - the probe now lists them (`pointer.records`) - and requires the probe to name
each as hovered; across the sweep: no full invalidation, paint p95 <= 8 ms (B2), no underrun (B5). Then at 1280x720,
1920x1080 and 2560x1440 (ADR-0072's sweep): a hover, a press held on a fader's cap, a drag from it sideways onto the next
strip's fader (the press stays on the first, the hover follows; the release leaves the hover), and a right press that
presses nothing; their shots and the keyboard ring with a hover go to the rubric. **Result on this desktop:** ss7
**197/197 PASS** - 97 controls, 0 missed, full invalidations 239 -> 239, p95 3.82 ms (Direct2D), 0 underruns, every
size's press / drag / right-press law held. **Visual check (agent):** the hover line reads on knobs and cells, the pressed
fader's line runs under its cap. **Found on the way (parked as its own task):** holding the press on a fader's cap
without moving showed a "-0.1 dB" readout - whether the press itself nudges the gain is to be checked and fixed.
**ADR-0067 is now built through cp3** (meters, hover and pressed, the playhead, nothing animates, the real-app sweep).
**Next:** G6.5 per the plan; the macOS GPU timing exception renewal and ADR-0069 still await Dan.

## 2026-10-08 — ADR-0067 cp3a/b: the playhead is the published frame, and a still shell never animates

**What (gates; no product change needed - the code already obeyed):** `[playhead]` - while the song plays, at four
published frames the Arrange playhead's white line is where that frame sits in the view (the canvas's time-to-x law,
+-1 px), and the piano roll's line is at that frame's position in the open clip (computed from the frame through the
clip's own time base, then placed by the roll's grid lines, +-1 px); stopped, sixty ticks paint the same pixels.
`[static]` - a fresh project at 1280x720, 1920x1080 and 2560x1440 paints the same pixels a tick apart; after the fixture
plays and stops, once the meters' held peaks and the status line run out (24 px of held-peak marks go out - the check
is not vacuous), thirty ticks in a row paint the same; and `src/ui` starts exactly two timers (the UI tick and the
timeline's auto-scroll), no `startTimerHz`, no `VBlankAttachment`. Five mutations bite (a playhead leading the frame, one
moving while stopped, the roll leading the tick, a blink, a third timer); one gate was made independent on the way (it
had read the roll's expected tick from the value under test).
**Critic:** no blocker; in - (1) **the header's MIDI-in lamp was a clock-driven visual** (lit until a `steady_clock`
instant, read in paint: a repaint could change it with no state change, against ADR-0067 §7): it now holds for whole UI
ticks (150 ms -> 5 ticks, as the meters' holds count), and `[static]` plays a note, sees the lamp, and requires it out
within its hold's ticks and the shell as before (red with a lamp that never goes out); (2) the settled-after-sound check
runs at all three sizes, and every pixel that went out lies in a meter's record; (3) a second `[playhead]` case walks a
tempo-locked clip (the song fixture's) through the tempo map (red with the roll leading the tick there); (4) the
stopped check covers the header's clock too. ctest 423/423, Clang clean.

## 2026-10-08 — ADR-0067 cp2 S5c: the real app shows the lines under the real mouse (SS-5 drive)

**What:** the `ss7` session drive (the real `YesDaw.exe`, real Win32 mouse) gains Step 16: it moves the pointer over the
header's meter chooser and tempo cell - the two native widgets whose hover only the real mouse sets - and reads the
window's own pixels: along every edge the 1 px band brightened and the middle unchanged; then holds the button down on
each and finds both pixels of the 2 px pressed line far brighter than the pixel just inside (so not the hover's 1 px).
The drive harness gains `Hover`, `HoverStrokeShown` and `PressedStrokeShown` (tools/session-drive.ps1); the probe lays
out `header.tempo` / `header.meter` and reports `project.meter`. **Result:** ss7 168/168 PASS on this desktop. The
first run caught the drive measuring the tempo cell 1 px too far out (JUCE draws a bar slider 1 px inside its bounds);
the cell's visible box is what it now measures. **Visual check (agent, 4x crops):** hover is a thin line inside each
box, pressed a heavy white one; no text crossed. **Found on the way (parked as its own task):** a plain click on the
tempo cell jumps the tempo to the click point (120 -> 213); the step undoes it and asserts the tempo is back.
**Next:** ADR-0067 cp3 ([playhead], [static], the full real-pointer sweep at three sizes), then G6.5.

## 2026-10-08 — ADR-0067 cp2 S5b: native buttons, toggles, combo boxes and sliders show the same lines

**What changes (visible):** the app's native widgets drop the old white wash on hover and press and show the painted
controls' lines instead: a 1 px white line just inside the edge while the pointer is over them, a heavier 2 px one while
held (over a button's own pressed colour), never both. A widget too short for its text to clear a 2 px line (the
master's 14 px DIM / MUTE) presses at 1 px - ADR-0072's text-clearance rule, by height for native widgets. On a slider
the line goes under the thumb (as on the painted fader); on a knob it rings the knob. One shared form
(`src/ui/PointerStroke.h`) draws both the shell's painted controls and the look-and-feel's widgets. The retired fill
tokens are gone. **Gates:** `[widget-states]` - a text button and a toggle over and held (JUCE's own states), a 14 px
button's close width, a combo box held: the line along every edge, nothing else changed; a sweep renders every native
button, toggle, combo box and value cell the shell shows (default size and the window minimum) with and without text,
and no text sits inside its line's band (it caught DIM). Six mutations bite. **Not headless:** a combo box's hover and a
slider's hover / press read the real mouse; S5c's real-app drive step proves them. ctest 423/423, Clang clean.
**Critic:** no blocker; in - the pressed check reads all four edges, a disabled slider never shows pressed, the toggle's
tick box is gated clear of the line, and comments say the height rule is a hint (the text sweep is the authority) and
that a native slider's right-press shows pressed by design (ADR-0072 §8 reads the widget's own state).

## 2026-10-08 — ADR-0067 cp2 S5a: the selection washes get their own token (no visual change)

ADR-0072 retires the old white-fill pointer tints (`hoverHighlightAlpha` 0.10, `pressedHighlightAlpha` 0.16) and
assumed the look-and-feel was their last user; eight other painters used them for something else. They now read
**`regionWashAlpha`** (0.16: marquees, the time selection, loop and punch bands, the keymap's selected row) and
**`recordIconHaloAlpha`** (0.10: the record icon's outer ring) - same values, so nothing looks different. Only the
look-and-feel still uses the old pair; S5b replaces it with the strokes and removes them. ctest 423/423.

## 2026-10-08 — ADR-0067 cp2 S4: hover and press show as white lines inside the control

**What changes (visible):** the painted control under the mouse (rail cells, knobs, faders, meters, sends, inserts, I/O
rows, tool cells, the header's gear and time, the Sampler's pads) shows a thin white line 1 px inside its edge; the one
being pressed shows a heavier one (2 px; 1 px on the mixer's 15 px insert, send and I/O rows, whose text sits 2 px from
their edges). Never a fill: no text or the colour behind it changes (ADR-0072). Pressed replaces hover on one control;
during a drag both show (pressed where it began, hover where the pointer is). A fader's line runs round its rail and
passes under the cap, where the cap is now, mid-drag too. Thin controls keep a straight, full-strength edge (the corner
radius shrinks to fit). A control that moves (a scroll) takes its line along in the same frame. Probe: `pointer.
hoveredBounds / pressedBounds`. CONTEXT.md gains **Hovered control**, **Pressed control**, **Peak since read**.
**Gates (`[g6-motion]`, 13 cases with S3's):** `[pointer-strokes]` - one control of every family (pads too), hovered
then pressed, rendered by JUCE's software renderer: the line is white at its token's alpha over the resting pixel along
every edge, and nothing else changed; `[pressed-replaces-hover]` - pixel-identical; `[hover-follows-drag]` - both lines
at once; `[fader-record-tint]` - the rail's line, the cap untouched, a real drag; `[pointer-text-contrast]` - every
control at two window sizes is rendered with and without its text, and no text pixel changes under hover or press;
`[hover-follows-state]` - a scrolled control's line follows before the tick. Eight mutations bite (no lines, wrong
alpha, stacked, no close width, a fill, no inset, no same-frame follow, full radius on thin controls).
**Visual check (agent):** crops at 4x of each state read clearly; it caught the line crossing the fader cap's face -
fixed and gated. **Critic:** no blocker; the thin-swatch case is the stroke by definition (every pixel is within its
reach), and the text gate now drops every cached image, not just the canvas's. ctest 423/423, Clang clean.
**Next:** S5 - native buttons, combo boxes and sliders take the same lines; the old fill tokens retire (eight
non-pointer uses get their own names first).

## 2026-10-08 — ADR-0067 cp2 S3: the shell knows which painted control the pointer hovers and presses

**What changes:** nothing visible yet - the shell now tracks, for every painted control (rail cells, strip knobs and
faders, sends, inserts, tool cells, the header's gear and time, the Sampler's pads), which one the mouse is over and
which one is held down; S4 paints them. Rules (ADR-0072): an open dialog over the header blocks its hover; a right or
middle press presses nothing; during a drag the press stays on the control it began on while hover follows the
pointer; with no button down, every UI tick re-finds what is under a still pointer (a dialog that opened, a dock tab
that switched, a strip that scrolled away); the rail's volume slop still starts a volume press. Each change repaints
only the old and new controls' rects. The probe reports `pointer.{hovered, pressed, lastRepaint}`.
**Gates (`[g6-motion]`, 8 cases):** `[hover]` names every record family at its centre, an exit clears;
`[overlay-blocks-hover]`; `[pressed][modifiers-in-seam][hover-follows-drag]` (every button x modifier mask, a lost
release, a strip scrolled out mid-press, a drag from the rail into the mixer); `[hover-follows-state]
[tick-walks-children]`; `[repaint-scope][a11y-steady]` (8 rounds: exactly two rects per move, no full repaint, the
accessible elements untouched); `[rail-volume-strict-bounds]`. Seven mutations bite. ctest 423/423, Clang clean.
**Critic:** no blocker; its middle-click leak (the rail seeded a volume press on a middle-click for up to one tick) is
fixed and gated, and a seed can no longer outlive the button. Parked for S4: re-sync a moved record's painted bounds in
the same frame (visible once strokes paint).

## 2026-10-08 — ADR-0067 cp2 S2: painted controls never overlap; the rail's floor is 204

**What changes:** the track rail can no longer be dragged narrower than 204 px (was 180): below that a row's record-arm
cell met its volume slider and meter, so one point named two controls. A saved or dragged rail below the floor clamps up.
The shell keeps the painted controls' records (id, bounds, the surface that paints each) from the same sync that places
their accessible elements - what hover (S3) will resolve against. **Gate (`[records-disjoint]`, screenshot suite):** at
the window minimum and every logical size of the scaling matrix, every rail width from the floor to the maximum, the
inspector at its narrowest, default and widest, a tall dock with the mixer and a bus, then the Sampler's pads - no two
records overlap, and every family is present. Red with the floor token at 180 (`rail.row.0.arm` overlaps its volume).
The scroll test now also checks the records follow a scroll; the prefs test witnesses the clamp. ctest 423/423, Clang
clean. **Critic:** no correctness issue; its coverage notes (window minimum, inspector range, the clamp) are in.

## 2026-10-08 — ADR-0067 cp2 S1: the shell draws only the regions a repaint reaches

**What changes:** the shell's own paint (header, track rail, inspector, dock) now draws a region only when the repaint
rectangle reaches it, including the drop shadow its panels cast just below them; the probe reports the regions the last
pass drew (`frame.regionsPainted`). A hover or a ring repaint of a few pixels will re-run only its own region's
drawing - the ground ADR-0067's pointer states (S3-S5) stand on. Nothing looks different. **Gate (`[repaint-scope]`):**
a full render draws all four; a render clipped to a small rect inside each draws only that one, and its pixels are the
full render's (+-2 per channel: a drop shadow's blur, computed over the clip, moves an edge pixel one step); a clip only
in the shadow band under the rail still draws the rail. Red with a region drawn outside its clip; red without the
shadow reach. **Caught on the way (Clang):** the new paint-region constants shadowed `MainComponentControls.cpp`'s own
`kRegion*` (the Tab-order regions) and silently renumbered them - renamed `kPaintRegion*` before commit. ctest 423/423,
Clang clean. **Critic:** nothing wrong; its shadow-band note is the fix above.

## 2026-10-08 — ADR-0072 accepted: ADR-0067's hover and pressed, made true to the code and the contrast law

**Why:** planning ADR-0067 cp2 (pointer hover and press) found its white-fill tints would push muted text below 4.5:1
(down to 2.3:1 when hover and press stack), its overlay rule let the header's controls hover through an open dialog,
its tick kept a stale component, rail widths 180-203 made controls overlap, and its seam could not tell a right-click.
**Decided (design workflow: two amendments, two judges; critic, then a re-critic):** hover is a 1 px white inner edge,
press a 2 px one - never a fill, so no text or its background changes (a 1 px press for the mixer's insert, send and
I/O rows, whose text sits closer to the edge); header controls hover only on an exact match; the tick walks the
children; the rail's floor rises to 204; the seam carries modifiers. **Next:** ADR-0067 cp2 in five slices (S1, the
shell painting only the regions a repaint reaches, is built and under review).

## 2026-10-08 — ADR-0069 proposed: a cheap autosave by hard link (awaits Dan on one point)

**Now:** `docs/adr/0069-cheap-autosave-hard-links.md` - from a design workflow (link vs a background worker vs a
database-only snapshot; three judges; a critic whose valid findings are folded in). Each autosave carries the project's
audio into the snapshot by a same-volume hard link (a copy on drives that cannot link), checks it by identity instead of
re-hashing, and still validates every hash at Restore. Measured here: **~11 ms** for the 500 MB case that took
**2.8-4.9 s** by copying; no audio bytes written per autosave. **Waiting on Dan:** ADR-0019 (which he co-decided) says
the audio is *copied* into the autosave; linking means the autosave shares the project's audio bytes instead of
holding a second copy. Recommended: accept (what the autosave protects - lost project edits - is unchanged). Until he
answers, ADR-0068 cp1/cp2 stays parked and ADR-0069 is not built. **Meanwhile:** ADR-0072 (settling ADR-0067 cp2's
open points: pointer tints vs text contrast, overlays over the header, rail overlap) is being designed.

## 2026-10-08 — Fix: a screen reader read stale positions after a mixer or rail scroll, and lost its place

**What was wrong (found planning ADR-0067 cp2):** ADR-0066's accessible elements for painted controls (the rail's
cells and knobs, the strips' zones) were re-placed only on a layout pass or an action. A wheel or scroll-bar scroll of
the mixer strips or the rail moved the controls but left the elements where the controls had been. And whenever the
set of visible controls changed (every scroll notch brings one in), the whole pool (~600 elements at 24 tracks) was
rebuilt, so a screen reader's focus fell off a control that was still on screen. **Fix:** every strip / rail scroll
re-places the elements at once (not the horizontal timeline scroll - nothing painted moves there); a control that
stays keeps its element, only the controls that came or went get new or retired ones, and Tab order is restored when a
control comes in ahead. **Gate (`[accessibility]`):** after one mixer notch and one rail notch, with no tick or action,
each element sits on its control's new bounds; a kept control keeps the same element object; one notch creates only
the entering strip's elements. Red with the re-sync removed, red with the full rebuild forced. ctest 423/423, Clang
clean. **Critic:** confirmed the bite and the paths (dock, rail-width, inspector and tab changes already re-sync).

## 2026-10-08 — ADR-0068 cp1/cp2 (autosave writes ON) is ready and held: one autosave freezes the UI for seconds

**Now:** the last ADR-0068 step - the app actually writing autosaves, with the pending-question gate, the sticky
not-durable flag and the stale-marker clear - is built, gated (five mutations red) and passes the real-app SS-6 drive
**178 of 178** (run 9: all three autosave laps green, every launch under 1.4 s). It is **not committed**: measured on
this machine, one autosave of a project carrying 10 x 50 MB of audio takes **2.8-4.9 s on the UI thread** (each
autosave copies and fsyncs every asset file, then re-hashes every asset to validate the snapshot - ADR-0019's
mechanism). A real song (~1 GB of audio) would freeze the app for several seconds every 30 s while editing and write
~1 GB per autosave. Shipping that would trade a missing safety net for the lag Dan already flagged; the safe order
says writes go on last, so they wait for a cheap snapshot. The work is parked as a patch
(`.git/yesdaw-wip/adr-0068-cp1-cp2-writes-on.patch`, local to this machine; it reapplies onto 7c5d660).
**Next:** ADR-0069 - an autosave cheap enough to run every 30 s (a design workflow is choosing between linking
assets instead of copying, a background writer, and a database-only snapshot); then ADR-0068 cp1/cp2 on top.
Also measured: a plain project open hashes every asset - 3.0 s for 500 MB (pre-existing; noted for later).

## 2026-10-08 — ADR-0068 cp3/cp4: a Save, a Save As and a Don't Save retire the autosave

**What changes:** once its work is in a bundle (Save; Save As - the bundle left behind) or the user named it abandoned
("Don't Save" / "Close without saving" on New, Open, Open Recent or closing the window), the autosave is removed, so it
can never be offered later as a rollback. Never while a recovery question is up: that autosave is the user's lost work
and only Restore or Discard decides it. Save a Copy keeps the source's (its unsaved tail is still open there). A crash or
a kill removes nothing. A removal that fails fails nothing (the Save stands): it is reported, and the stamp rule retires
the snapshot at the next open. Still no autosave is *written* by the app (cp1/cp2 next, last by design).
**Gates:** `[save-retires][save-as-retires][save-copy-keeps]`, a pending-question test (Save and Don't Save leave the
autosave and its marker), `[dont-save-retires]` (New: Don't Save removes it, Cancel keeps it; closing: the same, through
the real confirm seam). One ADR-0060 Save As assertion ("the source keeps its autosave") now reads ADR-0068 §3 ("Save As
retires it"); the source's project.db is still never written. Five mutations, each red. ctest 423/423, Clang clean.
**Critic:** nothing blocking; noted - macOS Cmd+Q skips the close question entirely (pre-existing, B37; the stamp rule
covers its autosave), and the cleanup message reads like a failed Save (reworded in cp1/cp2).

## 2026-10-08 — ADR-0068 cp5c: the recovery question is a card that says what each side holds

**What a user sees:** when an open finds an autosave holding work the project lacks, a card over the top of the arrange
asks "A snapshot of this project was autosaved after your last Save. Restore it, or discard it and keep the saved
version?" and lists **Autosaved** and **Saved** side by side (tracks, audio clips, MIDI clips, takes), with Restore
Autosave (warmer, the primary) and Discard Autosave in its right-hand end. It is not modal: the lanes stay in view and
every control stays live. The two buttons used to float alone at fixed coordinates over the header's master meter.
**Details:** the card is as tall as its text needs (the question wraps at the narrowest window, never clips); the
buttons carry the question as their accessible description; nouns are singular for one ("1 track" - the §6 template
read as English, not "1 tracks"; the [prompt-text] gate pins that rendering). **Judged on the real app** (an ad-hoc
drive on a bundle with an unanswered question; shots in the session scratchpad): four rounds - a phantom blank line
(a bold face measured bold but drawn regular; the question is regular weight now, lines counted with the label's own
fitting routine), weak hierarchy (now a bright key column beside muted counts), a passive primary. **Gates:**
`[autosave][prompt-text]` (the exact text; the shell's card holds both buttons, sits in the timeline, carries the text
to both buttons, goes on Discard); the shell's child-count pin (+1, deliberate); the tooltip rule and the token
contrast table caught the card and its new Restore colour (`recoveryPrimaryButton`, 4.7:1 for muted text) - both
satisfied, not loosened. ctest 423/423, Clang clean. **Critic:** its should-fixes (narrow-window clipping, screen-reader
context on the buttons) and two polish notes are done; the card covering the ruler is deliberate (the lanes stay
visible; nothing on the ruler is needed to answer). **Next:** cp3/cp4 - a Save, a Save As and a Don't Save retire the
autosave (never while a question is up).

## 2026-10-08 — ADR-0068 cp5b: an open asks about an autosave only when it holds something the bundle lacks

**What changes for a user:** opening a project no longer asks "restore the autosave?" when the project already holds
everything the autosave does (after a plain crash the project file holds every edit) - that autosave is retired
silently. It asks when the autosave is ahead of the project (writes lost, e.g. a power cut), and keeps asking at every
open until Restore or Discard answers, even if the user works on, saves and quits meanwhile (a marker in the bundle).
Nothing writes autosaves yet (cp1/cp2 switch the writes on last), so the shipped app's behaviour is unchanged today.
**How:** the snapshot's write stamp against the bundle's; raising the question records the marker; Discard clears it
before deleting the snapshot; Restore clears it in the same transaction as the recovered rows; Save As / Save a Copy
clear it in the copy (no autosave there), and Save As hands the open question back to the bundle left behind; a marker
whose autosave is gone is cleared at open. Probe: `autosave.bundleWriteStamp`, `autosave.snapshotWriteStamp`.
**Gates (`[autosave]`):** `[round-trip-bundle-newer]` (bundle past or equal: no question, autosave gone);
`[round-trip-bundle-lost-writes]` (asks with both stamps; Restore recovers, Discard keeps the bundle; the launch probe);
`[unresolved-across-restart]` (asked again after edits + Save + restart; Save As while asked; a dangling marker);
`[migration-silent-pre-v35]`. The recording fixture now plants real lost writes (its old shape is retired silently by
design). Nine mutations, each red. ctest 423/423, Clang clean. **Critic:** no data-loss path; its Save As finding is
fixed (with a gate); its "a corrupt autosave keeps the marker" finding is answered where it matters - cp1's autosave
write must clear any marker still set (no question is up then, so it is stale); clearing at open would let a transient
read failure turn into a silent discard. **Next:** cp5c - the question's text on a card over the arrange (ADR-0068 §6).

## 2026-10-08 — ADR-0068 cp5a: every bundle write counts itself (schema v35, the write stamp)

**Now:** ADR-0068 lands in its safe order - the stamp and the recovery rule first, the autosave writes switched on
last. **This slice:** schema v35 adds `project_write_stamp` (add-only, ADR-0012: an older bundle lands at 0). Every
snapshot write advances it by one inside its own transaction - it commits, checkpoints and rolls back with the rows;
an autosave snapshot carries its source bundle's count instead. Nothing reads it yet: behaviour is unchanged until
cp5b, where an open compares the two counts and only asks about an autosave that holds something the bundle lacks.
**Gates (`YesDawPersistenceCheck`):** `[v35]` fresh bundle at 0, three writes reach 3, a v34 bundle migrates to 0 and
reads back unchanged; `[stamp-shared-transaction]` a refused stamp (a trigger, a missing row) fails the whole write and
leaves no transaction open; `[stamp-override-durable]` an autosave's project.db holds its source's count with no WAL
beside it, and the bytes of a live bundle already hold an override. Each red under a mutation: the stamp moved after
COMMIT, after the checkpoint, and the autosave carry removed. ctest 423/423, Clang clean. **Critic:** nothing to fix
(no project-row writer bypasses the stamp; Save As lands at source + 1 with an empty autosave folder; the frozen
fixtures are only ever opened as copies). **Next:** cp5b - the recovery predicate and the unresolved-question marker.

## 2026-10-08 — SS-6 run 8: 171 of 176 pass; the five left are ADR-0068's autosave (known red until it lands)

**Now:** the project-lifecycle drive (`tools/session-scripts/ss8-project-lifecycle.ps1`) passes everything except the
autosave laps, on the real exe with nothing else running (run 6's three B6 timing failures were a build running beside
it; void). **Drive fixes in this commit:** audition selects the row by its name - its press at x 24 had landed on the
row's play mark (x 8..26), starting an audition the Audition button then stopped; the mark is now tested on purpose,
with "auditioning never imports" (it caught the double-press import, fixed in 54c5d1a); a 6 s WAV of the drive's own
so audition outlives a probe read; the replacement-under-export check compares finished-job outcomes (the per-project
export count starts again with the new project); a launch Cancel now expects ADR-0062's untitled session (the real
app's deferred native question path - its fallback runs there). **Next:** ADR-0068 cp5a (schema v35, the write stamp).

## 2026-10-08 — Fix (found by SS-6): a double press on a browser row's play mark imported the file

**What a user saw:** pressing a file's ▶ mark in the browser twice quickly (start, then stop the audition) also
imported the file as a new clip - the second press's double-click ran the row's "import" gesture. ADR-0056 keeps them
apart: a double-click on a row imports, the play mark auditions. **Fix:** a double-click whose press landed on the mark
is the audition's stop and nothing else. **Found by:** SS-6 run 7 on the real app (two clips, one asset after the
audition step). **Gate:** `[audition][browser]` drives the real mouse sequence (down, up, down, up, double-click) on
the list: two audition toggles and no import on the mark; one import on the row's name. Red before the fix (an
import), green after. The older harness reached "double-click" by calling the import directly, which is why no gate
saw it. ctest 420/420, Clang clean. **Critic:** nothing wrong (JUCE delivers the second press before the double-click,
so the guard reads the right press).

## 2026-10-08 — Fix: a launch that cannot open the last project leaves the untitled session, not nothing

**What a user saw:** when the project YES DAW reopens at launch could not open (a missing or damaged bundle, one the
engine refuses, or Cancel on its missing-audio question), the app came up with no project at all - only File > New or
Open did anything. **Now (ADR-0062: "At launch, Cancel leaves the untitled session"):** the untitled session opens and
the reason stays on the status line ("Open failed: ... (<name>)" / "Open cancelled: ..."). The last-project record
still names the project that failed, so the next launch tries it again (a drive plugged back in, a file restored) -
the fallback session never takes its place until a Save, New or Open names a project. If the fallback itself cannot be
created, both reasons show. The deferred (native) question's fallback also sets the device to the session's rate.
**Gates:** `[empty-startup]` - a missing explicit bundle at launch opens an untitled session, says why by name, keeps the
earlier untitled bundle, and the next plain launch reopens the last real project (red with the record overwritten);
`[project-lifecycle]` an engine refusal at launch and `[relink]` a launch Cancel both leave the untitled session.
ctest 420/420, Clang clean. **Critic:** no blockers; its two should-fixes (the deferred path's device rate, a failed
fallback hiding its own failure) and test-bite gaps are fixed. Untested: the native deferred path (a modal question;
no headless seam). **Next:** SS-6 drive fixes (audition, export replacement) and a re-run; then ADR-0068 cp5.

## 2026-10-08 — Fix (found by SS-6): a project whose clips were all deleted reopened as nothing, silently

**What a user saw:** delete every clip of a project (its tracks and audio files stay), quit or crash, open it again —
nothing opened and nothing said why: no project, an empty status line. **Why:** the audio engine refuses to build a
graph for an empty timeline, and the open path had no transport-only fallback (the edit path already had one,
ADR-0041); both open paths (the launch reopen and File > Open) ignored the failed load. **Fix:** an empty timeline opens
with a transport, as after an edit; any load that fails says "Open failed: <reason> (<name>)" (R5). **See-it:** SS-6
run 5 — after the kill the project reopens with its 5 tracks and 6 audio files. **Gates:** `[project-lifecycle][reopen]`
— a bundle with audio and no clips reopens through the model and through the launch path; an engine refusal (a
tempo-locked clip) names its reason at launch and on File > Open and leaves the current project. Each red with its fix
reverted. ctest 420/420, Clang clean. **Critic:** no blockers. Next, its note: after a failed launch-time open the user
is left with no project at all (and the next launch tries the same bundle again) — the untitled session should open
with the reason shown.

## 2026-10-08 — SS-6 is built: the project-lifecycle drive runs on the real app (151 of 165 on its fourth run)

**Now:** `tools/session-scripts/ss8-project-lifecycle.ps1` — the plan's SS-6, G5's exit — exists and runs end to end on
the real exe. **Next:** a real bug it found (below) is being root-caused; then ADR-0068 (the autosave and recovery
contract) in a safe order — the stamp and the recovery rule first, the writes switched on last — so no commit ever
writes autosaves that the old "ask whenever one exists" rule could offer as a rollback; then ADR-0067 cp2.
**Decision still pending (Dan, plan §8.2):** the macOS frame exception (unchanged).

**What the drive does, on the real app:** creates a 44.1 kHz / 100 BPM project from the New Project dialog; drops a
file with a real Windows drag and auditions it from the browser's Recent list; drops AIFF, FLAC, Ogg, a 44.1 kHz WAV and
an MP3 on chosen lanes and times and checks every Asset's hash names its bundle file; refuses a malformed WAV with its
reason and declines an .m4a while it is dragged; reopens a copy with a missing asset — Cancel, a wrong file refused,
the original relinked; exports 24-bit PCM mix + stems (headers read back), cancels an export mid-job (no file, no
partial), refuses a second export while one runs, and replaces the project under a running export (the old job retired
silently); then the three autosave laps (ADR-0068); then Save As, Save a Copy and a relaunch keeping the preferences.

**Run 4: 151 passed, 14 failed.** Known-red until ADR-0068: the autosave laps (the shipped autosave never writes).
Script fixes since (run 5 pending): the browser dock's settle, the concurrent-export refusal (the chooser still asks
where; the refusal comes with the path), the replacement case (a real New, Don't Save, Create — it used to Cancel).
**A real bug found:** after Save As, deleting every clip and a kill, relaunching with that bundle opens nothing and
says nothing (no project, no status) — the launch and Open paths ignore a failed load. Being root-caused with a gate.

**Runner (`tools/session-drive.ps1`):** `FileDrop` (a real OLE file drag from a small topmost source form, checked to be
under the press; an aborted drag closes its form), `WaitAlert` / `AlertButton` / `AlertText` (JUCE alerts through UI
Automation), `KillApp` (not `Kill` — PowerShell resolves that to its Stop-Process alias first and waited on input),
`Launch -AutosaveIntervalMs`, and `Focus` accepts the app's own modal window holding the foreground (the missing-audio
alert at a relaunch). Fixtures: `tests/fixtures/import/drive_48k_stereo.{aiff,flac,ogg}`, `drive_44k_mono.wav` (ffmpeg
commands in its README). Lessons in the drives README: no Home key in JUCE popups (pick by name), no Alt mnemonics in
its menu bar, never name a helper after a PowerShell alias.

## 2026-10-08 — Fix: a drop that adds tracks left the master fader over the new strip

**Now:** found by the SS-6 smoke drive and fixed. **Next:** the SS-6 drive tooling and its live runs, then ADR-0068's
checkpoints, then ADR-0067 cp2. **Decision still pending (Dan, plan §8.2):** the macOS frame exception (unchanged).

**What a user saw:** dropping two files onto the first lane of a one-track project added a track, and the master pane's
fader stayed where the master pane used to be — drawn over the new track's strip (two fader knobs on that strip, none
on the master). The rail's + Track button had the same fault. **Why:** the mixer's live widgets (the master fader, the
strip scroll bar, the FX editor's rows) were laid out only on a resize; a drop and the + Track button refresh the
state without one. **Fix:** every action refresh lays them out (a no-op when nothing moved); the strip scroll bar shows
only when the dock shows the mixer, whoever lays it out.

**Gates:** `[file-drop][layout]` — a two-file drop and then the + Track button each move the master pane right and the
master fader stays inside it (red without the fix). The critic's other find — the inspector's widgets have the same
layout-only-on-resize shape — is the next fix after it is shown to bite. ctest 420/420, Clang clean.

## 2026-10-07 — G6.4 cp1: meters read every block since the last look (ADR-0067)

**Now:** ADR-0067 cp1 (meters from real state) is in; ADR-0068 (the recovery contract) is accepted (8336ef0). **Next:** a
stale-layout fix the SS-6 smoke drive found (a drop that adds a track leaves the master pane's fader drawn over the new
strip), then the SS-6 drive tooling and its first live run, then ADR-0068's checkpoints, then ADR-0067 cp2 (hover and
pressed). **Decision still pending (Dan, plan §8.2):** the macOS frame exception — 505ad13 failed only that check on macOS
(sustained 23.8 ms); RTSan and TSan green with the new meter primitive. Reported raw, not excused.

**What a user gets:** a peak or a clip in ANY audio block reaches the meters — before, a meter showed one block in three
to twelve (only the last before each 33 ms screen tick), so a short peak could be missed and the clip light could miss a
clip. Stopped with nothing playing, meters fall to silence because no block ran (not because a flag said so), while a
stopped track that IS sounding — a note's release, an audition — meters it. The master pane has the strips' peak hold and
clip latch, its clip indicator is a real control (clickable, keyboard-reachable, read by a screen reader as "Master clip
indicator"), and the header's master meter shows the louder channel (a clip on the right alone used to be missed).

**How:** every metered peak (each track and bus meter tap, the master's left and right, the armed inputs) publishes
through `PeakSinceRead`, read exactly once per UI tick into the hold states that every painter and the probe read; the
control thread no longer writes an input's peak.

**Gates:** `[g6-motion][meters]` — a track, a bus, the master and an armed input each latch a clip in a block that is
not the tick's last; a stopped strip reads silence after a tick though its last block was loud; a stopped instrument
track sounding an audition reads its signal; an un-armed-then-re-armed input reads silence; no device block reads
silence and keeps the latch; the master clip indicator (by key and by mouse) clears the latch; a source inventory finds
each meter source read only in its tick step. Each case was turned red by its own mutation and back. `meter_tests`
re-grounded (a reset leaves the reading intact). ctest 420/420, Clang clean. **Critic:** the first gate covered the master
and the input only; the strip, bus, stopped, audition and re-arm cases were added and mutation-checked.

## 2026-10-07 — SS-6 groundwork: the probe sees the lifecycle; finding: the shipped autosave never writes

**Now:** the state probe carries what the SS-6 drive (`ss8`, drafted) asserts the project lifecycle by. ADR-0067 (G6.4) is
accepted (149d0c1). **Next:** the recovery contract (ADR-0068, in a design workflow — see the finding below), then
ADR-0067 cp1 (meters) on the `PeakSinceRead` primitive, then the rest of `ss8`'s needs (an alert-window helper, a hard
kill, fixtures, a file drop). **Decision still pending (Dan, plan §8.2):** the macOS frame exception — b39cbc2 was green
on every job, macOS included; 523b320 failed only that check (sustained 20.0 ms). Reported raw, not excused.

**Finding (2026-10-07, by this checkpoint's gate):** the shipped app has **never written an autosave**. The 30 s cadence
asks, but the write runs only when `PlaybackEngine::needsAutosave()` is true, and nothing in the app calls
`markProjectEdited()`. Every edit is written straight through to the bundle (SQLite WAL: a killed process keeps every
edit; a power cut can lose the last ones), so a crash does not lose work — but the power-cut-safe snapshot ADR-0019
promises does not exist, and once it does, the recovery question as written ("an autosave exists") would offer a
snapshot older than the bundle. ADR-0068 (the recovery contract) is being designed and reviewed before any fix lands;
the first draft assumed a save-based model and was withdrawn.

**What the probe carries (for drives):** `project.{sampleRateHz, tempoBpm, trackCount, clipCount, midiClipCount,
assets[].{hash, sampleRateHz, channels, frames}}` (the hash names the bundle's `audio/<hash>.asset`);
`autosave.{enabled, intervalMs, writes, failures, lastWritten, recovery.{pending, prompts, restores, discards, tracks,
clips, midiClips, assets, takes, compSegments, bundlePath}}`; `relink.{asking, name, refusal, damaged, questions,
lastOutcome}` (the missing-audio question while it is up); `export.{lastResult, outcomes, destination, retiring}`. A
drive shortens the autosave cadence with `YESDAW_AUTOSAVE_INTERVAL_MS` (250 .. 600000; anything else ignored).

**Gates:** `[project-lifecycle][probe]` — the project's rate, tempo, counts and its Asset by the bundle file's hash; the
cadence seam; the export's `none` then `succeeded`, its outcome count and destination; `[relink][probe]` — the question
in the probe while it is up (its name, the refusal of the wrong file, the count) and `relinked` / `cancelled` after;
`[export-job]` — `cancelled` and the destination on the model. ctest 420/420 (with the next commit's primitive), Clang clean. **Critic:** no blockers;
the relink question now clears even if the chooser throws, the model floors the cadence at 250 ms, the destination is
the started job's, the recovery block names its bundle and the takes / comp segments.

## 2026-10-07 — G6.3 live: a screen reader's view checked on the real app, and its focus move now adopts the target

**Now:** G6.3's desktop proof is in: `ss7` Step 15 reads the app through UI Automation (the tree Narrator and NVDA read)
and passes. **Next:** ADR-0067 (G6.4) is through its design workflow and under a second critic round; then the SS-6
drive's missing pieces (`ss8`, drafted: probe fields, an alert-window helper, a hard kill, fixtures, a file drop).
**Decision still pending (Dan, plan §8.2):** the macOS frame exception — 523b320 fails only that check on macOS
(sustained 20.0 ms); every other job green; reported raw, not excused.

**What a user gets:** a screen reader that moves its focus onto a control — a strip's mute, a fader, a widget — now hands
the keyboard that control as its target (the ring goes there; Enter and the arrows act on it). It never worked on a real
window: JUCE parents an element on its nearest focus container, the window, not the shell, so the shell's own handler
never saw the focus move (headless gates could not see it: there is no handler without a native window).

**Found by:** the live step — "a screen reader's focus move adopts the target" was red on the real exe. **Fixed:** the
shell asks the window's handler for the focused element and keeps only what lies inside the shell.

**Gates:** `[native][accessibility]` (new, Windows) — the shell as the content of a hidden, off-screen window: a screen
reader's focus move onto a painted element and onto a widget each become the target; the router's own Tab moves the
screen reader's focus and the next ticks leave it; Esc does not restart navigation (red without the fix). **See-it:**
`ss7` Step 15 — Tab to track 1's rail mute: UI Automation's focus is that element, by name and type (check box), on the
painted cell, the keyboard still the shell's; its Toggle mutes the track and reads the new state; strip 2's fader is a
slider with a range whose set is one undo step; a SetFocus on strip 3's mute moves the router there; every one of the 38
painted zones laid out has exactly one element standing on it. `ss7` 161/161 (Step 11's playhead read now waits for the
probe: it raced the transport once). ctest 417/417, Clang clean. **Critic:** no blockers; two checks sharpened (the
element count matched by position, a tautological assert dropped).

## 2026-10-07 — The desktop is free: every earlier real-app drive passes again (SS-1 .. SS-5, seven scripts)

**Now:** Dan freed the desktop. The seven existing drives ran on the real exe: `ss1` passed as it was; `ss2` .. `ss7` went
red at their first step and are repaired; all seven pass. **Next:** G6.3's live screen-reader step (UI Automation, in
`ss7`), then G6.4 (its implementation ADR is being designed and critic-reviewed), and the SS-6 script (`ss8`, being
drafted). **Decision still pending (Dan, plan §8.2):** the macOS frame exception — e0f5d7f and 6c9a3ed fail only that
check on macOS (sustained 21.5 ms and 22.5 ms against 16.6 ms; every other job green); reported raw, not excused.

**What was wrong:** the drives had not run since G5 (the desktop was busy), and three G5 changes had moved under them:
File > New now shows the New Project dialog before the location chooser (G5.5), the keymap lives in `prefs.json` (G5.6),
and the File menu grew (G5.5: Save a Copy, Save as Template), so a "7th item" pick became Import Audio. A script that
waited for the chooser straight after New left the dialog open over the shell, and every later click and Tab landed in
it — `ss7`'s 25 failures were almost all that one dialog. **One product bug:** the state probe published the dialog's
controls only while the mixer dock showed (a drive with the dock hidden could not find Create); fixed, with a gate.

**Repairs:** the runner gains `NewProjectChooser` (New -> the dialog -> Create -> the native chooser, each hop asserted)
and `MenuPickByName` (a popup item by its text through UI Automation, the tree a screen reader reads; FAIL when missing
or disabled), plus `UiaFocused` / `UiaFind` for G6.3's live check. Every script's New goes through the helper; `ss5`'s
and `ss6`'s File-menu picks are by name; `ss2`'s keymap check reads `prefs.json`'s `keymap`. README: the new primitives
and three lessons.

**Gates:** `[project-lifecycle]` — the New Project dialog's controls are in the probe's layout with the dock hidden (red
without the fix). **See-it:** ss1 45, ss2 42, ss3 57, ss4 22, ss5 77, ss6 58, ss7 144 assertions — all PASS on this
machine (Windows 11, the shared desktop, Dan away); every launch 1.2 – 1.5 s to the first interactive tick (B6: 3 s).

## 2026-10-10 — G6.3 done headless: targets that go away, overlays, a full walk, the ring as drawn (ADR-0066 cp3)

**Now:** G6.3's three checkpoints are in, headless-certified locally. **Next: G6.4** (interaction and motion states —
hover, pressed and target states; playhead and meter ballistics from real state; its implementation ADR first), then
G6.5 (empty states and first-run tips). **Pending the desktop:** SS-6 and the earlier real-app drives, and a screen
reader's live focus on the painted elements. **Decision still pending (Dan, plan §8.2):** the macOS frame exception —
CI keeps failing only that check on macOS (3daa429: sustained 25.5 ms); reported raw, not claimed excused.

**What a user gets (cp3):** nothing new to see — the gates prove what the router already did: when the control under the
keyboard target goes away (its strip scrolled out of the mixer, its track deleted, its inspector section dropped by a
smaller window) the target moves to a live control on the next tick; closing the keymap editor or Cancelling the New
Project dialog puts the target back where it was; Tab walks a real session end to end; the target ring keeps 3:1
contrast as it is actually drawn (at its alpha), not only as an opaque token.

**Gates:** `[g6-keyboard]` (new case) — the three going-away cases each leave the target on a live control in the walk;
the keymap editor and the New Project dialog (a shell that shows it) each take the walk inside and give the target back
when their close / Cancel is pressed, and Cancel creates nothing; a Tab walk from the first control to the last lands
on the walk's order at every step and reaches buttons, toggles, choosers and values and every surface's zones (rail,
tool strip, header, mixer strips and master slots, transport, inspector); `[tokens]` — the ring blended at its alpha
over every one of the 20 surfaces it can land on is >= 3:1. 417/417 local, Clang clean.

**Critic:** one blocker, fixed — "moved to a live control" was weaker than the ADR's law; each going-away case now
asserts the exact retarget (the control at the vanished one's Tab index, clamped). In: the walk checks every surface's
zones, the drawn ring over every surface, the deleted-track case first requires its target in the walk. The FX editor's
restore stays G4.0b's EQ test (the same overlay law).

## 2026-10-09 (night) — G6.3 checkpoint 2: every painted control has its own accessible element (ADR-0066)

**Now:** G6.3 **cp2** is in, headless-certified locally. **Next: G6.3 cp3** — targets that go away (a deleted track, a
strip scrolled out, a dropped section), overlays restoring the target, a representative full-shell walk, the ring's
contrast. **Decision still pending (Dan, plan §8.2):** the macOS frame exception — CI keeps showing it alone: 813391b
(cp1a) sustained 22.1 ms, c3df53b (the undo fix) 18.0 ms, every other test green; reported raw, not claimed excused.

**What a user gets:** a screen reader now sees each painted control as its own element — "Stem 3 mute, toggle button,
checked", "Stem 3 pan, slider, R10", "Mixer strip 2 send 1 level, slider, -6.0 dB", "Pad 1: load a sample, button" —
instead of one element per surface; pressing or setting it does what Enter (and the mouse) does; the router's moves
speak through it while the keyboard stays with the shell. The master fader reads "0.0 dB" to a screen reader (it said
"1.00").

**Found and fixed:** a screen reader pressing a painted control could crash the app — the press refreshed the shell,
which handed that same element a new model while its function was still running; the element now runs a copy, and a
pool rebuild retires old elements to the next message-loop turn instead of destroying them under a running call.

**Gates:** `[accessibility]` (new) — for every control in the walk (70 painted, 39 widgets at 1920x1080 with a tall
dock): a painted control's element is click-through, id-less, never focusable, exactly on its rect, with its role,
title, value text, range (values) and checked state (toggles) matching the target and press / toggle where they
belong; a widget's own JUCE handler has the right role, a title, the target's value text (values) and checked state
(toggles); an element's press mutes a track; a pan set through the element is one undo step; an insert slot's element
offers its menu; the router's Tab announces through the element (a widget target clears it) and the keyboard stays with
the shell; 50 refreshes that change nothing create no element. Ten runs in a row clean. Pins moved, deliberately: the
shell's child count +1 (the header's element layer); `[header-flex]` skips a child that takes no clicks and has no id
(that layer). 417/417 local, Clang clean.

**Critic:** no blockers. In: the widget half checks JUCE's handlers (role, value, checked); the announced element clears
for a widget; duplicate ids asserted. Kept: the value interface is JUCE's base one (its ranged one forces a bare number,
not the readout "+3.0 dB"); the pool's shape key is rebuilt per refresh (refreshes are not per tick; the walk costs
0.52 ms). Pending the desktop: the live screen-reader focus grab (a native window's), checked by a drive.

## 2026-10-09 (late) — G6.3 checkpoint 1b: the mixer's zones, the pads and Shift+F10 (ADR-0066)

**Now:** G6.3 **cp1** (both halves) is in, headless-certified locally. **Next: G6.3 cp2** — an accessible element per
painted control (click-through, focus-less proxies with role, name, value, state and actions). **Decision still
pending (Dan, plan §8.2):** the macOS frame exception.

**What a user gets:** Tab reaches every painted control on each mixer strip shown — S / M / R, pan, fader, meter, the
I/O rows, each send row and insert slot — the master's insert slots and the Sampler's pads; Enter mutes, opens an
empty slot's or well's add menu, opens a filled slot's editor, opens an I/O row's choices, loads a pad; a routed send's
level and the pan adjust with the arrows (Esc restores). **Shift+F10** opens the target's right-click menu — an insert
slot's Bypass / Remove / Move had no other home. An adjustment Esc puts back now leaves nothing in the undo history,
and a drag released where it began no longer adds an empty step either (the redo history survives both).

**Gates:** `[g6-keyboard]` — every mixer zone the probe names on screen (cells, pan, fader, meter, I/O, sends, inserts;
the master's slots) is in the walk, each kind at least once; strip mute toggles; pan reads L10 and Esc restores it with
no undo step of its own (one undo then takes back the previous real edit); the meter clears its clip light; Enter on an
empty insert slot / the output row / an empty send well opens that zone's menu; the add menu's kind fills the slot;
Shift+F10 on the filled slot opens its menu with Bypass; Shift+F10 on a rail cell opens the row's menu; Enter on the
filled slot opens its editor; all 16 pads are in the walk and Enter loads the first. `[mix-scalars]` (engine): a
gesture that comes back leaves no step, one that changes leaves exactly one, an empty one changes nothing, and the redo
history survives the first and last but not the second. 417/417 local, Clang clean.

**Amendment (ADR-0066):** a pad's click loads a sample (not an audition); JUCE never delivers the Menu key, so Shift+F10
is the context-menu key. **Parked:** the pads' Shift / Ctrl verbs (mode, clear) still need a keyboard route (a pad menu);
the "edited" marker stays set after an adjustment that came back (pre-existing: the dirty flag counts edits, not net
change).

**Critics:** UI — no blockers (the master slots' surface is the strips' input overlay, which covers the master pane).
Engine — no blockers; in: the redo history set aside and returned.

## 2026-10-09 (later) — G6.3 checkpoint 1a: the rail, tool strip and header's painted controls are keyboard targets (ADR-0066)

**Now:** G6.3 **cp1a** is in, headless-certified locally; **G6.2 certified** (4dc95a2, the cp2 compile fix, green on
every job). **Next: G6.3 cp1b** — the mixer's painted zones (S / M / R, pan, meter, sends, insert slots, I/O rows, the
master's slots), the Sampler's pads and the context-menu key. **Decision still pending (Dan, plan §8.2):** the macOS
frame exception (see below).

**What a user gets:** Tab now reaches every painted control on the track rail (each row's M, S, O, pan, volume, colour
swatch and meter), the timeline's seven tool cells and the header's gear and time readout — Enter mutes / picks a tool /
cycles the time display as a click does; on pan and volume, Enter then the arrows adjust, Enter keeps it as one undo
step, Esc puts it back exactly. Space stays play / stop throughout.

**Gates:** `[g6-keyboard]` (new) — every one of the 23 painted zones the probe names on screen (two rows x 7, seven tool
cells, gear, time) is in the Tab walk, row 0's before row 1's; toggles flip the model and read on / off; the rail volume
steps in dB (capped at unity like its drag, never snapping a higher gain down), Enter keeps one undo step, Esc
restores exactly; pan reads R10 after two steps; colour advances (one undo step); the meter clears its clip light; a
tool cell picks the tool; the gear toggles the settings row; the time readout cycles; Space toggles play before, while
navigating, while adjusting, after Enter and after Esc; a track removed mid-adjustment closes the gesture with no
empty undo step. Walk cost: 154 controls at 24 tracks build in 0.52 ms (gate < 4 ms). Bite: with the painted
collection off, coverage fails. 416/416 local, Clang clean.

**Critic:** no blockers. In: a vanished row / strip closes its gesture without an edit; the rail volume reads the true
gain; the announcement falls back to the shell; undo through the action; row order checked; the walk's cost measured
instead of cached.

## 2026-10-09 — G6.2 checkpoint 3: the macOS frame exception re-measured (ADR-0064) — a decision for Dan

**Now:** G6.2's three checkpoints are in; **ADR-0066 accepted** (G6.3: every painted control a Control target with its
own accessible element). **Decision pending (Dan, plan §8.2):** renew the macOS frame exception or not. **Next:** G6.3
cp1 — painted controls as targets. SS-6 still waits for a free desktop.

**The baseline** ([evidence](docs/evidence/2026-10-g62-gpu-baseline.md)): on Dan's machine the timeline frame check
holds 4.1–4.5 ms sustained in ten runs (budget 16.6) — the renderer has about 4x headroom. On CI, macOS failed the check
in 41 of 56 runs since the exception was written (39 with nothing else failing), sustained 16.7–30.5 ms (median 22.4),
and it passes and fails on adjacent commits with no change to anything the check compiles (9ed2983 passed, 86a0b31
failed); Windows CI failed 2 of 62 (17.1, 17.9 ms). The check prints its numbers only when it fails, so passing runs
add counts, not numbers.

**Recommendation:** renew the exception with this baseline, in the same narrow scope (macOS, this check, the sustained
budget only, nothing else failing), re-evaluated when the renderer, the test or the runner image changes or Dan's
machine measures above 8 ms sustained. Alternatives: make the macOS frame check informational in CI (the owner-machine
`verify-hardware.ps1` frame stage stays binding), or renderer work for the macOS runner (not justified by the data).
Until Dan decides, macOS GPU-only reds are reported raw with their numbers at each checkpoint and not claimed as excused
by a renewal.

## 2026-10-09 — G6.2 checkpoint 2: the scaling matrix, the 1152x640 window, everything reachable (ADR-0064)

**Now:** G6.2 **cp2** is in, headless-certified locally. **Next: G6.2 cp3** — the macOS frame exception re-measured
(the baseline is gathered: `docs/evidence/2026-10-g62-gpu-baseline.md`; the decision is Dan's under plan §8.2). Then
G6.3 (keyboard navigation and accessibility; research done — the router already owns a control target, the gaps are
painted controls and what accessibility sees). SS-6 still waits for a free desktop.

**What a user gets:** the window can be made as small as 1152x640, so a maximized window fits a 1920x1080 laptop at
150 % or a 1440x900 one at 125 % (it overflowed under the taskbar before); the status line (messages, refusal reasons,
hover hints) stays on screen at every size — it used to vanish at 1280 wide, and the Snap-mode / Edit-mode / Nudge
choosers (all in the Edit and View menus) now drop before it does; the Snap chooser no longer disappears once the mixer
was used (it hid whenever the Mixer panel was active, a rule from the modal-mixer days, leaving its SNAP caption over
nothing); a bar number in the ruler sits right of its tick (the tick ran through the digit) and gives way to a tempo
or meter change's label at the same bar.

**Gates:** `[layout][scaling]` (new) — 20 cells: the plan's 3 window sizes x 4 scales, and the maximized client of each
of the plan's displays at Windows' scales (floor(W/s) x floor((H-48)/s - 32): 1280x640, 1920x1000, 1536x793, 1280x656,
2560x1360, 2048x1081, 1706x896, 1280x664). At each: the header's rects inside it and disjoint; the dock and the master
inside the window; the tool row disjoint; every shown identified control at any depth inside the window and hit-testing
to itself at its centre; every control shown at 2560x1440 (with a clip selected) but dropped here has its actions in a
menu (the four toolbar choosers, the zoom controls and the marker list mapped to theirs) or, for an inspector section,
comes back whole with the dock collapsed; the exact drop set per cell pinned; the render at the cell's scale is the
physical size; on Windows (JUCE's software rasteriser) the render box-filtered back matches the 100 % render per panel
(mean difference <= 6; measured max 3.13) and at 200 % carries real detail (difference from a bilinear upscale of the
1x render >= 0.8, measured 1.63-3.75; a 1x image drawn at 2x scores 0, asserted). `[shell-sizes]` floor at 1152x640:
the FADES section is back with the dock collapsed. The dock clamp at the window minimum. `[rubric-shots]`: the column
above each bar tick is ink-free. `[ruler]` (new): a tempo change at a labelled bar hides that bar's number and touches
no other. Bites: the status line dropping (before the fix), a 1x-rasterised canvas cache (0.32 < 0.8), the centred bar
label, an un-skipped bar number under a tempo label. 416/416 local, Clang clean.

**Visual judgment (agent):** all 20 renders written; 1280x656 @ 150 % (the common 1080p laptop) — crisp text and lines,
nothing cut, the pinned master's columns clean, two whole lanes with the dock open; 2560x1440 @ 200 % at native
resolution — text, icons and waveforms rasterised at full resolution (no 1x blur).

**Critic:** no blockers. In: the matrix selects a clip (the inspector's sections were never in the reference before),
walks descendants, renders in software for CI determinism, pins the drop sets; map labels win the bars row. Parked: a
scrollable inspector (Logic-style) would bring the marker card back at 720p; today the marker list's job is reachable
through Transport > Previous / Next Marker.

**CI recorded:** 08be04a (ADR-0065) green on every job, the macOS GPU check included — **ADR-0065 certified**.
1497d09 (this checkpoint) failed to COMPILE on Linux and macOS: `meanAbsDifference` is used only inside the
Windows-only raster gates, so `-Wunused-function` fired under `-Werror` there (the local Clang check runs with Windows
defines and could not see it); Windows passed. Corrective commit: the helper is `[[maybe_unused]]`.

## 2026-10-08 (night) — the mixer's strips scroll and the master stays reachable (ADR-0065, amended)

**Now:** ADR-0065 is in, headless-certified locally. **Next: G6.2 cp2** — the scaling matrix (plan cells at
100/125/150/200 % and the maximized-display cells), the window minimum 1152x640, reachability (every control
hit-tests to itself; every droppable control's action in a menu; dropped inspector sections back with the dock
collapsed), scaled renders; then cp3 (the macOS frame exception re-measured). SS-6 still waits for a free desktop.

**What a user gets:** with more strips than the dock holds (16 tracks at 1280x720, 24 at 1920x1080) the master no
longer falls off the right edge: it stays pinned there, whole, and the strips scroll beside it in whole strips — by a
scroll bar under them, by the wheel (a strip a notch; nothing over the master), and by the selection (a new track, a
rail click, Up / Down and a bus rename bring the strip into view; a manual scroll is never undone). With few strips
nothing changes. The master pane's meter row now reads scale | fader | L R meters side by side — the fader used to
sit over the dB numbers and the meters even at 1080p, and the narrow-strips master (64 px) could not hold them at all.

**Amendment (recorded in the ADR):** the master is never narrower than 122 px (its scale, a 24 px fader, its meters)
nor than the strips' share; the strips share what it leaves. Two pins moved with it, deliberately: the shell's child
count (+ the scroll bar) and the narrow-strips master width (64 -> 122).

**Gates:** `[mixer-scroll]` (new) — few strips: the N3 layout, no bar; 24 tracks + 2 buses at the window minimum,
1920x1080 and 1280x720: the master pane's right / bottom at the panel's, the master fader inside it, the bar shown
and left of the master, every shown strip whole at the minimum width, disjoint, left of the master, its bottom plus
the bar at the master's bottom, every hidden strip without parts; the wheel moves one strip, the master pane not at
all; at the end the last bus whole; a mute click on the first shown (deep) strip mutes that track; a clip latch
survives a scroll out and back; Up / Down x20 and a new track bring their strips into view; a manual scroll survives
an action refresh (the refresh counter proves it ran); a hidden bus's rename brings it into view with the editor on
its strip; the master's scale, fader (>= 24 px) and meters pairwise disjoint inside the pane, fit and overflow.
Bites: the never-overflow law fails the master-inside-the-panel check; a fader column over its neighbours fails the
disjoint check. 416/416 local, Clang clean.

**Visual judgment (agent):** 1280x720 / 16 tracks — 13 whole strips, the bar under them, the master whole at the
right with INTEGRATED / TRUE PEAK and scale | fader | meters clean; 1080p / 16 tracks — all strips fit, the master's
columns clean (the scale's numbers were under the fader before).

**Critic:** one blocker, fixed — the funnel followed the mixer target and the rail lane separately, so a refresh that
changed both to different strips scrolled the first back out; now the mixer target wins. In: the follow memory and
the offset reset when the project changes. Parked: trackpad momentum moves a strip per event in the mixer, the rail
and the timeline alike (a shell-wide wheel accumulation is its own item); a window shrink can leave the selected strip
hidden until the next selection (the ADR follows new selections only).

**CI recorded:** ea8c596 (G6.2 cp1) green on every job, the macOS GPU check included — **G6.2 cp1 certified**;
55b5013 (ADR-0065) green.

## 2026-10-08 (evening) — G6.2 checkpoint 1: eight lanes at 1080p, track headers level with their lanes (ADR-0064)

**Now:** G6.2 **cp1** is in, headless-certified locally; **ADR-0065 accepted** (mixer overflow). **Next: ADR-0065** (mixer strips scroll, the master pinned at
the right — found in this checkpoint's judgment, accepted after a critic pass), then **G6.2 cp2** (the scaling matrix,
the 1152x640 window minimum, reachability, scaled renders, the mixer scroll), then cp3 (the macOS frame exception
re-measured). SS-6 still waits for a free desktop.

**What a user gets:** at a 1920x1080 window with the dock open, eight whole tracks show (seven before); every track
header on the left sits level with its lane (they were 15 px high since the ruler grew) and stays level after
scrolling to the bottom at any height (the two panels clamped the scroll separately); the timeline's tool row is a
compact 28 px with its controls centred; the inspector's section captions (GAIN, FADES, FADE CURVE, MARKERS) sit
inside their cards instead of on the edge.

**Gates:** `[layout]` (new) — at the window minimum, 1280x720, 1920x1080, 2560x1440 and a token-derived height where
the two row areas would round apart: every control on the tool row lies inside it with equal gaps (the painted tool
cells too), every on-screen rail row's top equals its lane's (heights too for whole lanes), a rail row whose lane is
off the canvas lies outside the rail's row area, at the top and scrolled to the bottom (the last lane reached); density
`>= 8` at 1080p in both density gates (was `>= 7`); the GAIN caption leaves the card's first 8 px empty and paints just
after. Bites checked: the scroll clamp fails without the rail footer (at the straddle height, scrolled), the row
containment fails with the old 8/26 control inset, the caption gate fails with a 0 inset. 416/416 local, Clang clean.

**Visual judgment (agent):** 1080p — the rail rows and lanes share every separator, lane 8 ends 4 px above the canvas;
the tool row reads tools · SNAP Beat · zoom · slider · Snap / Edit / Nudge · I X P A, centred; 720p — three whole lanes
(two before); the floor shot keeps the tools and the view cluster (the rest drops whole). **Found:** with 16 tracks at
1280x720 the mixer's later strips and the whole master run off the dock's right edge with no way to reach them (24
tracks at 1920 the same) — ADR-0065.

**Critic:** no blockers. In: the dead automation-row tokens (`automationLaneRowTopInset = 92` and the helpers built on
it) removed; the rail-level gate also checks rows whose lanes are hidden and that the bottom reaches the last lane; the
straddle height asserts its default-dock premise.

**CI recorded:** 379b6ae and 1bc4a6f (G6.1 cp1, cp2) fail only the standing macOS GPU exception (sustained 22.37 /
22.64 ms; 413/414 green otherwise) — **G6.1 certified**. fd5eaed (ADR-0064) green.

**Baseline gathered for cp3** (CI history since 2026-09-08): macOS ran the GPU check 56 times and failed 41, all on
the sustained budget (16.7–30.5 ms, median 22.4 in the failures), passing and failing on adjacent commits with the same
renderer; Windows failed 2 of 62 (17.1, 17.9 ms). The check prints its numbers only when it fails.

## 2026-10-08 (afternoon) — G6.2 decided: ADR-0064 (layout density and scaling)

**Now:** ADR-0064 accepted (two critic passes). **Next: G6.2 cp1** — the 28 px tool row and 4 px panel inset (eight
whole 72 px lanes at 1920x1080), the rail's rows level with their lanes (header 93 = outer inset + tool row + ruler,
footer 11 = scroll bar + inset; one `rowArea()`), the density gates back to >= 8, row-containment and rail-level gates.
Then cp2 (the scaling matrix, the window minimum 1152x640 so a maximized window fits 125 % / 150 % laptop displays,
reachability, scaled renders) and cp3 (the macOS frame exception re-measured). SS-6 still waits for a free desktop.

**Found while measuring (defects, fixed in cp1):** the track headers sit 15 px above their lanes (the ruler grew to
64 px in G0.7 cp3; the rail's 86 px header was never moved); the two panels clamp the vertical scroll over row areas
11 px apart, so at some heights a scroll to the bottom leaves the headers a row off; a maximized window on a 1920x1080
display at 150 % (client ~1280x656) cannot fit the 720 px minimum height.

**CI recorded:** 379b6ae (G6.1 cp1) macOS fails only `YesDawTimelineGpuCheck` (sustained 22.37 ms, max 33.03 ms;
413/414 otherwise green) — the standing exception; Windows still running. 1bc4a6f (G6.1 cp2) queued.

## 2026-10-08 (midday) — G6.1 done: colours once, no fake data, labels and tooltips in words (ADR-0063 cp2)

**Now:** **G6.1** is in, headless-certified (both checkpoints). **Next: G6.2 — layout and scaling** (the window /
display scaling matrix, the 1280x720 floor vs the code's 1152 minimum, eight tracks at 1080p; its implementation ADR
first). SS-6 still waits for a free desktop.

**What a user gets:** every tooltip reads as words with its live chord ("Play  (Space)", "Clip start: drag to change")
— the toolbar's said `transport.play`, the autosave buttons theirs, the clip time and fade fields `clip.inspector.*`,
and the automation lane row its component id; the inspector's stretch slider now has a "Stretch" label and its value
("100%", muted while the slider is disabled); the window's own background is the theme's; each track colour is defined
once.

**Gates:** `[tokens]` / theme audit — no raw colour in `src/ui` or `src/Main.cpp` outside the token headers (the audit
now catches a `u` suffix; `UiColourValues.h` is the JUCE-free home of the shared values); every `Tone` array is read
somewhere (the unused fake `inspectorAutomationValues` is gone); `[tooltips]` — every toolbar button's tooltip is its
accessible name and live chord (shown or hidden) and no tooltip anywhere contains a dotted code word or a stable id;
screenshot — the stretch label and value are drawn legibly at 1280x720 and 1920x1080. 416/416 local, Clang clean.

**Deviation (recorded):** ADR-0063 names the raw-colour pattern `0x[0-9A-Fa-f]{8}[uU]?`; the audit uses
`0xff[0-9A-Fa-f]{6}[uU]?` (opaque ARGB, as before, plus the suffix) so 8-digit non-colour constants such as a
`0xFFFFFFFFu` sentinel are not flagged — it still bites the five accents and the window background the ADR targets.

**Visual judgment (agent):** the GAIN card reads "Stretch [slider] 100%", aligned with the gain row. Noted for G6.2:
the card's "GAIN" caption sits tight to the card's left edge.

**Critic:** no blockers. In: the tooltip gate rejects any dotted code word, not only registered action ids (it found
five inspector fields and the lane row); the autosave buttons' tooltips follow live chord rebinds; the stretch value is
muted while its slider is disabled.

**CI recorded:** 86a0b31 (G5.7 cp2) fails only the standing macOS GPU exception — **G5.7 certified**. 9ed2983 (G5.7
cp1): Windows `YesDawTimelineGpuCheck` sustained 17.44 ms vs 16.6 on the runner — the commit touches only
`ProjectBundle.h` and relink tests (the GPU test includes neither) and 86a0b31, which contains it, passed that test on
Windows; a re-run of the same SHA was requested for the record. 379b6ae (G6.1 cp1) re-queued after a re-run cancelled
its pending run.

## 2026-10-08 (morning) — G6.1 checkpoint 1: the type scale and WCAG contrast (ADR-0063)

**Now:** G6.1 **cp1** is in, headless-certified. **Next: G6.1 cp2** — every colour defined once (the track accents and
the window background into one JUCE-free header; the raw-colour audit catches `0x...u` and covers `src/Main.cpp`), the
unused fake `Tone` array gone, toolbar tooltips in words ("Play  (Space)", never `transport.play`), the inspector's
second gain slider labelled. Then G6.2 (layout and scaling). SS-6 still waits for a free desktop.

**What a user gets:** every label is at least 11 px and ordinary UI text 12 px (the plan's section 3.4), with WCAG AA
contrast where it is actually drawn: the faint third text level is gone; the rail's lit S / M / O cells now draw dark
ink on the track colour (they were light text at 2.3-3.4:1); clip names stay readable on every track colour (the clip's
bright top is a little calmer); the drum-mode key names, the delay taps' stems and the ruler ticks are solid colours.
On a narrow timeline the zoom trio, its slider and the Snap chooser (with its caption) drop whole instead of sliding
under the I / X / P / A buttons (Snap stays in Options, its modes in View); the piano roll's Key and Scale choosers show
"Scale: Off" whole.

**Gates:** `[tokens]` (4 cases) — the type scale; every one of the 64 colour tokens classified (text, surface, fill,
indicator, decoration — a new token fails until it is), parsed from `UiTheme.h`; ~180 WCAG pairs (text 4.5:1 on every
surface and fill it is drawn on, including a clip name on each track colour's body and top and the lit cells' ink;
indicators 3:1, the playhead line included); no `withAlpha` on a text token in `src/ui`. Screenshot `[tokens]`: the
timeline toolbar row never overlaps at 1152x720 / 1280x720 / 1366x768 / 1920x1080 / 2560x1440 with the inspector open
and closed, the four view toggles always there; every Key / Scale item fits its box with 2 px to spare. V1 measures
with the WCAG formula and also samples a clip name and a lit rail cell. 416/416 local, Clang clean.

**Visual judgment (agent):** the rubric shots at 1280x720 and 1920x1080 and the 1152x720 laptop shots read cleanly at
the new sizes (mixer rows, insert names, the master meter's scale, clip names, header cards). They found the two layout
faults above (both gated now) — the rubric's "truncated Scale / fade / master labels" row closes here.

**Critic:** no blockers. In: `soloActiveText` on the lit DIM and MUTE fills asserted; the playhead (`white`) is an
indicator, not decoration; the chooser-fit gate keeps 2 px to spare. Not taken: "Snap unreachable when its chooser
drops" (verified: Options holds Snap Off / Bar / Beat / Sixteenth and View the snap modes).

**CI recorded:** 0655b5e (G5.6 cp3) fails only the standing macOS GPU exception — G5.6 certified; 9ed2983, 86a0b31
(G5.7) and the ADR-0063 commit pushed or queued.

## 2026-10-08 (early) — G5.7 done: missing audio is asked for at open and put back from its original (ADR-0062)

**Now:** **G5.7** is in, headless-certified (both checkpoints) — **every G5 item (G5.1–G5.7) is headless-certified.**
G5's exit is the logical **SS-6** drive (`tools/session-scripts/ss8-project-lifecycle.ps1`, still to be written and
run on the real app): it **waits for a free desktop** (Dan's go-ahead covered one night only; drives stay paused
until he frees it). **Next:** safe headless work continues — G6's implementation ADRs (tokens and icons, layout and
scaling, keyboard/accessibility, interaction states, empty states) and their headless gates, with the SS-6 script
written alongside so it can run the moment the desktop is free.

**What a user gets:** opening a project whose audio files were moved or damaged no longer just fails with a hash file
name: each missing or damaged file is asked about in words — "Audio Clip - 0:01.0 48 kHz mono, used by 1 clip" —
with **Locate...** (the file chooser) or **Cancel**. Only the original file is taken (the exact bytes the project
was made with, so it sounds exactly as before); another file is refused with the reason and the same file asked
again; a file that is right but cannot be written says what failed instead. Cancel leaves the current project as it
was and the status line names what is still missing; files already put back stay, so the next open asks only about
the rest. File > Open, Open Recent and the launch reopen all ask (the native launch asks once the window exists).

**Gates:** `[relink]` +4 (7 cases) — the open asks about a missing and a damaged file in turn, refuses the wrong tone
with its reason and asks again, takes the originals, opens, and the reopened project renders byte-identical to before
the files were lost; it saves and reopens with no question; Cancel keeps the current project (title, undo) and the
bundle byte-identical, naming both; one put back then Cancel, then the next open asks only about the other; the
launch reopen asks the same way (Cancel: no project, as a failed launch open); a pad-only and an unused Asset are
described; a right file that cannot be written is not called the wrong audio. `[missing-asset-open]` and
`[prepared-project-load]` pass unchanged (with no answerer the open refuses as before — the re-pin the ADR expected
was not needed). 416/416 local, Clang clean. Visual: the questions are JUCE's modal box and file chooser (drive
pending).

**Critic:** no blockers. In: an I/O failure while putting a right file back now says what failed ("could not be put
back: ...") instead of calling it the wrong audio (gated); the native deferred launch open keeps an earlier launch
reason (a missing device) beside its Cancel line.

**Parked:** imported clips are all named "Audio Clip" (ADR-0056's name for an Asset is its first clip's), so the
names in these questions and the browser rarely tell files apart — the length, rate and channels do; naming an
imported clip after its source file (as Logic does) is a separate improvement. Offline Assets, searching a folder
for the others, hashing off the message thread (ADR-0062).

**CI recorded:** bb8dfd6 (G5.6 cp2) fails only the standing macOS GPU exception; 0655b5e and 9ed2983 pushed.

## 2026-10-08 (small hours) — G5.7 checkpoint 1: inspecting a bundle's missing audio and adopting original bytes (ADR-0062)

**Now:** G5.7 **cp1** is in, headless-certified. **Next: G5.7 cp2 — the open's questions**: an open refused over its
audio asks about each missing or damaged file (Locate... / Cancel), takes only the original, opens again; Cancel keeps
the current project and names what remains missing; the launch reopen too. Then SS-6 and G6.

**What it adds (persistence, no UI yet):** a read-only inspection of a bundle that refused to open over its audio — it
lists **every** missing or damaged Asset file (the open stops at the first) and the stored project, changes no byte
(no migration, reconcile or sweep; an older schema is not inspected), and works on read-only media; and the adoption of
an Asset's **original bytes only** — another file is refused ("its content differs") with nothing written, a file
already whole is left alone, a damaged one is copied to `.trash` under a free name (never over earlier evidence) and
then replaced in one rename, so a failure leaves it where it was.

**Gates:** `[relink]` (3 cases) — the inspection lists both a missing and a damaged file past the first and leaves
every bundle byte as it was; an old-schema bundle is not inspected and not migrated; adoption refuses another tone
writing nothing, takes the originals, keeps the first damaged copy when a second is trashed, leaves a whole file
alone, leaves no temporary, and the open validator then accepts the bundle. 416/416 local, Clang clean. (Earlier in
this checkpoint `YesDawTimelineGpuCheck` read 21-24 ms under the -j6 suite with other load on the machine; its binary
is unchanged since 19:18 and passes alone — load, not code.)

**Critic:** no blockers. In: the inspection opens the database truly read-only (no journal-mode write, works on
read-only media); a damaged file is copied aside and replaced in one rename (a failed rename used to leave it missing);
a file that already holds the bytes is left alone.

**CI recorded:** bb8dfd6 (G5.6 cp2) and 0655b5e (cp3) pushed; results pending.

## 2026-10-07 (late night) — G5.6 done: the chosen audio devices (ADR-0061 cp3); persistent preferences complete

**Now:** **G5.6** is in, headless-certified (all three checkpoints). **ADR-0062** (missing audio relink) is accepted.
**Next: G5.7 — relink**, in two checkpoints: cp1 the bundle inspection (every missing or damaged Asset file, nothing
written; an old schema not inspected) and the adoption of an Asset's original bytes only; cp2 the open's
Locate…/Cancel questions, the status line, the re-pinned refusal gates and the render-identity gate. Then SS-6
(`ss8-project-lifecycle`, needs a free desktop) and G6.

**What a user gets:** the output and input devices chosen in the device choosers are remembered and reopened at the
next launch (before the project asks for its sample rate). A remembered device that is not there leaves the open one
and the status line says so in words — "Audio output Interface is not available - using Speakers", "- no input", or
"No audio device could be opened (Interface is not available)" — and stays remembered, so it is used again once it
is back. A device of another type (ASIO next to Windows Audio) is switched to through its own type.

**Gates:** `[prefs]` +2 (11 cases, 12 with the keymap gate) — a chosen output and input (one with a non-ASCII name)
are reopened at the next launch in the order output, input, then the rate request; both missing, listed-but-refusing,
no input open, no device at all: each named exactly, the open device kept, the choice never overwritten; with the
rate request refused the device reason and the rate warning share one line; back again, the remembered pair is used
and nothing is reported. 416/416 local (the GPU timing test green again: the earlier reds were load), Clang clean.
Visual: no layout change.

**Critic:** its blocker is fixed — the rate request's warning overwrote the device reason (now one line holds both,
gated with a refusing device); also in: the native switch goes through the device type that lists the name (the
chooser path too); an implicitly-used default input is named so a remembered default is not switched to again; the
callback is suspended once around both switches; the chooser is refreshed and the block size re-read only after a
real switch. Not taken: a gate for the restore's do-not-remember flag (it can only re-write the same name, so no
behaviour can show it).

**CI recorded:** 67d61fc (G5.6 cp1) fails only the standing macOS GPU exception; bb8dfd6 (cp2) running.

## 2026-10-07 (night) — G5.6 checkpoint 2: view, dock, editing and export preferences (ADR-0061)

**Now:** G5.6 **cp2** is in, headless-certified. **Next: G5.6 cp3 — the chosen audio devices** (reopened at launch
before the project-rate request; a missing one leaves the open device and the status line names both; never
overwritten by a fallback), then G5.7 (missing-asset relink) and SS-6.

**What a user gets:** the dock's tab and visibility, the inspector's visibility and tab, the snap unit and mode, the
metronome and the export bit depth, dither and normalize now follow the user across opens and launches instead of
resetting on every open. Panel sizes: a project keeps its own (its view-state record wins); a new project, or one
without a record, starts from the last arrangement the user made. The export controls remember a change as it is
made and show the remembered choice after a relaunch.

**Gates:** `[prefs]` +4 (9 cases) — dock tab, inspector, snap (bar), snap mode and metronome survive a New, a
relaunch and an Open, while with no session folder every New is factory (the harness unchanged); a project keeps
its sizes, a new one starts from the last arrangement, and A and B keep separate values across a relaunch (SS-6 step
7's logic); the export controls write on the click itself and show the choice after a relaunch; export choices
round-trip; a malformed view, editing or export key falls back alone (counted). `[keymap-editor]` re-pinned: the
click is remembered too, so the rebound chord is judged by what it toggles. 415/416 local, Clang clean (22 files);
the one red is `YesDawTimelineGpuCheck` (sustained 21-24 ms vs 16.6) from an unchanged binary (built 19:18, before
cp1; passed in today's earlier full runs; it links neither changed file) while another session renders video with
headless Chrome on this machine — load, recorded; CI's Windows runner is the gate. Visual: no layout change.

**Critic:** no blockers; it verified the snap mapping, no write-every-tick oscillation, the no-folder path and the
other session-folder tests. In: the export controls change model state with no action, so a change was saved only
by a later unrelated action — they now note it at once (gated through the real controls); the bit-depth chooser was
fixed at 32-bit float at construction while preferences load later — the controls now show the model's choices on
every refresh; the harness sets the inspector width through its own setter.

**CI recorded:** 2bb136d (G5.5 cp3) fails only the standing macOS GPU exception — G5.5 fully certified; 50e1253
(docs) cancelled by the next push; 67d61fc (G5.6 cp1) running.

## 2026-10-07 (evening) — G5.6 checkpoint 1: prefs.json and the keymap (ADR-0061)

**Now:** G5.6 **cp1** is in, headless-certified. **Next: G5.6 cp2 — view, dock, editing and export preferences**
(the last arrangement as the default for projects without their own view state; dock/inspector visibility and
tab, snap and metronome applied on every open; export bit depth, dither and normalize), then cp3 (the chosen audio
devices, reopened at launch, an honest reason when one is missing), then G5.7 and SS-6.

**What a user gets:** the user's settings now live in one `prefs.json` in the per-user folder; this checkpoint moves
the keymap into it (an old `keymap-overrides.txt` is imported once, silently, and retired). A broken preferences
file never stops a launch: the defaults load, the file is kept as `prefs.json.unreadable` and the status line says
so; a single bad setting falls back alone; settings a newer version wrote survive. A file an editor saved with a
byte-order mark still reads. Two actions that traded chords both keep them. The last-project, recent-projects and
per-project view-state records are now written whole (a crash can no longer leave half a record).

**Gates:** `[prefs]` (5 cases) — a rebind lives in `prefs.json` across launches; the old record imported once,
renamed, silently, never again once `prefs.json` holds a keymap; the same folder set twice reads nothing; no folder
writes nothing; empty, truncated, non-JSON, array and string files each give the defaults, are kept aside and
reported, and the next change writes a fresh readable file; a wrong-typed binding and a duplicate chord are each
rejected alone (counted) while the rest loads; unknown keys, a newer action's binding and a higher version survive
a rewrite; a swapped pair binds in either the file or the old record; a BOM and `7.0` read; an unreadable file never
imports the old record behind "defaults are in use"; the three records written whole, no temporary left.
`[keymap-editor]` re-pinned from `keymap-overrides.txt` to `prefs.json`. 416/416 local, Clang clean; the theme
audit caught a high-byte escape in the first BOM check (now a byte comparison). Visual: no surface change.

**Critic:** no blockers. In: a BOM is skipped (Notepad); an unreadable file plus the old record no longer imports
behind a "defaults" message; bindings apply as one set (swaps were rejected in either order, also in the old
importer); a version written `7.0` reads; a failed save says so; the no-folder test proves nothing is written. Not
taken: the probe's "none" (it is the no-folder state, not dead code).

**ADR-0061** accepted after a critic pass (its blocker: the keymap-editor gate asserted the retired file).

**CI recorded:** c5704cb and b871300 (G5.5 cp1, cp2) fail only the standing macOS GPU exception; 2bb136d (cp3) and
50e1253 (ADR-0061) pushed, CI running.

## 2026-10-07 (afternoon) — G5.5 done: templates (ADR-0060 cp3); new project, templates and copies complete

**Now:** **G5.5** is in, headless-certified (all three checkpoints). **Next: G5.6 — preferences** (its
implementation ADR first: `prefs.json`, the settings that belong to the user rather than the project), then G5.7
(missing-asset relink) and SS-6 (`ss8-project-lifecycle`, the real-app drive of G5.1-G5.7 — waits for a free
desktop).

**What a user gets:** **File > Save as Template...** saves the project's layout — tracks (names, colours, heights,
instruments and their settings, strips with inserts, sends, outputs), buses, the master strip, rate, tempo, meter
and markers — as a template in the per-user templates folder, never its content (no clips, audio, MIDI, takes,
automation, Sampler pads, loop/punch, locate points or scale; nothing soloed). A template of the same name is
replaced only after asking (the question names the template on disk). The New Project dialog lists Default first,
then the templates by name; one that cannot be used (it does not open, or it holds content) is listed with its
reason and cannot be chosen; choosing a template fills in its rate, tempo and meter, which can still be changed. A
project made from a template gets a new identity for everything — the project, every track, bus, insert, send and
marker — with its routing (outputs, sends, sidechain keys) rewired, so two projects from one template never share
an ID (ADR-0011).

**Gates:** `[project-lifecycle]` +4 (18 cases) — a template holds exactly the layout and none of the content (and a
Sampler keeps its kind with no pads); a bundle dropped into the folder by hand brings only its layout; New from a
template: no ID of the template anywhere in the project, the routing rewired, and a tone through it renders within
1e-6 of a hand-built twin of the layout; the dialog lists the folder (Default first, a broken and a content-holding
bundle refused with reasons and unselectable, `.partial` folders never listed) and picking a template by keyboard
sets its rate, tempo and meter, then creates; Save as Template asks before replacing (No keeps, Yes replaces, no
temporary folders left), a cancelled name writes nothing, two names sharing a file name are named as the file; a
template that cannot be used refuses New with a reason. File menu re-pinned to 13; `docs/keymap-v2.md` regenerated.
416/416 local, Clang clean (22 files). Visual: the dialog's layout is unchanged; templates are text items in its existing chooser (refused ones greyed by JUCE).

**Critic:** no blockers; it confirmed the ID remap complete against `engine::Project`. In: instantiation always
passes through the layout filter (a hand-copied bundle cannot carry loop, punch, scale, locate points or an
automation mode into new projects); the replace question names the template file it would replace; a failed restore
says where the old template is; fresh IDs avoid every ID of the template file, not just its layout. Not taken:
forcing the first tempo point to a jump (it would flatten a template's tempo ramp; the dialog sets where it starts).
Parked: case-insensitive sorting of the list.

**CI recorded:** 8cd395d (G5.4) green on every job; 83a3649 and 91cef0f (docs) green; a618c5c fails only the
standing macOS GPU exception. c5704cb (cp1) and b871300 (cp2) pushed, CI running.

## 2026-10-07 (midday) — G5.5 checkpoint 2: Save As and Save a Copy through one atomic bundle copy (ADR-0060)

**Now:** G5.5 **cp2** is in, headless-certified. **Next: G5.5 cp3 — templates** (File > Save as Template..., a
layout-only bundle in the session-state folder; New from a template gives every entity a fresh ID through one
map), then G5.6 (preferences), G5.7 (relink) and SS-6.

**What a user gets:** **Save As** and the new **File > Save a Copy...** share one copy routine. It builds the whole
bundle in a temporary `<name>.yesdaw.<n>.partial` folder next to the target — the database copied consistently
from the open project (SQLite `VACUUM INTO`; the original is never written), the audio, peak caches and view state,
but never the autosave folder or the trash — writes the current project into it, checks it (opening it validates
every stored rule and every audio file against its content hash) and only then renames it into place. A failure
anywhere leaves no target and no temporary folder, and the original stays open and working. Save As then continues
in the copy (clean, undo intact); Save a Copy leaves you in the original, still unsaved if it was, its undo and any
running export carrying on. Before, Save As wrote the original first, copied straight into the final folder (a
failure left half a bundle), copied the autosave folder (the copy could open with a stale recovery prompt), and
there was no Save a Copy.

**Gates:** `[project-lifecycle]` +5 (14 cases) — Save As: the target appears only at the rename, no `.partial`
remains, the copy is clean and current, its Asset files byte-identical, no autosave carried, the copy reopened from
disk renders byte-identical audio to the in-memory project, undo continues into the copy and the original's stored
project reads back unchanged. Save a Copy: the original current and unsaved, the copy closed (its folder moves),
reopened it renders the same audio, undo and a later Save work on the original, a peak cache mid-write is not copied.
An injected corruption mid-copy fails both with "the copy did not validate", leaving no target, no `.partial`, the
original working; an occupied target is refused before anything is written. An export held mid-render completes
through both. File > Save a Copy through its chooser; a refusal says why. The old Save As safety gate rewritten to
this law (it pinned a half-copied target). File menu re-pinned to 12 items; `docs/keymap-v2.md` regenerated.
416/416 local, Clang clean (33 files). Visual: the only new surface is one File menu item (no layout change).

**Critic:** no blockers. In: the copy goes file by file — a peak cache the builder renames under the walk is
skipped, temporary files are never copied, links never followed (it flagged a Save-right-after-import race); a
failed target check says so instead of "already exists". Not taken: a success message (the status line's law is
that success stays quiet); POSIX `rename` replacing a directory created empty in the same instant (nothing to
lose; a non-empty one still refuses). Parked: sweeping stale `.partial` folders left by a crash; seeding the Save a
Copy chooser with the project's name. ADR-0060's "new validateBundleAssetFiles (presence and size)" is met by the
open path's existing check, which is stronger (content hash).

**CI recorded:** 69ababc fails only the standing macOS GPU exception; every other job green.

## 2026-10-07 (morning) — G5.5 checkpoint 1: the New Project dialog, the device at the project's rate (ADR-0060)

**Now:** G5.5 **cp1** is in, headless-certified. **Next: G5.5 cp2 — Save As and Save a Copy through one atomic
bundle copy** (`VACUUM INTO` into `<target>.<n>.partial`, the open path's content-hash check validates it, rename;
autosave and trash never copied), then cp3 (templates), G5.6 (preferences), G5.7 (relink) and SS-6.

**What a user gets:** `File > New` opens a **New Project** dialog — sample rate (44.1 / 48 / 88.2 / 96 kHz), tempo,
meter and template (Default for now; templates are cp3) — and remembers the last choices. New never opens or
overwrites a project already at the chosen place. New, Open and Open Recent ask **Save / Don't Save / Cancel** when
there are edits since the last Save (Save on the untitled session goes through Save As; cancelling it cancels).
When a project is created or opened — including the one opened at launch — the audio device is asked to run at the
project's rate (a reopen); a device that can't is put back as it was and the warning names both rates. Before, every
project was 48 kHz / 120 / 4/4, New silently overwrote an existing bundle, and a 44.1 kHz project on a 48 kHz device
played 9 % fast.

**Gates:** `[project-lifecycle]` (9 cases) — the chosen rate, tempo and meter reach the reopened bundle and are
remembered; the keyboard alone creates a project through the overlay, Tab stays inside it, Esc closes it (one press;
while navigating, ADR-0049's order: the first Esc ends navigation); an occupied target is refused and left
byte-identical; a failed creation keeps the current project and its undo; Save / Don't Save / Cancel on a named
project and on the untitled session (Save As cancelled cancels New); the device asked once, not again at the same
rate, the warning naming both rates when refused, and the launch project asked too; the dialog's choices apply to an
injected project; out-of-range choices refused; the remembered record is radix '.' both ways and a bad value falls
back to the defaults whole. Screenshot gate `[new-project]`: the dialog inside the window, every control visible and
inside it, nothing the layout raises above it (it bit: the dock splitter was drawn across the dialog). Agent visual
judgment of the 1280x720 shot: pass (cosmetic nit parked: the meter reads "4 4" with no "/" between the choosers).
416/416 local, Clang clean.

**Critic:** no blockers; its five should-fixes are in (the dialog's choices win over an injected project; a refused
native reopen restores the previous device setup so the callback is never lost; the launch project asks for its
rate; the untitled-session gate; a locale-proof, strict record) plus the nits worth taking (a 0.5 Hz rate tolerance,
invalid choices refused). **ADR-0060** amended before publication: every edit already persists, so the copy law is
"never writes the source", not "the source stays as last saved".

**CI recorded:** cb528a6 and 2b636f0 fail only the standing macOS GPU exception (`timeline_gpu_tests.cpp:84`);
every other job green.

## 2026-10-07 (early) — G5.4 done: each Asset's audio is one shared buffer (ADR-0059, closes R30)

**Now:** **G5.4** is in, headless-certified. **Next: G5.5 — new project, templates and copies** (sample rate, tempo
and template in the new-project dialog; Save As and Save a Copy; its implementation ADR first), then G5.6
(preferences), G5.7 (relink) and SS-6.

**What a user gets:** memory per imported or recorded file is its audio once (a cross-rate file adds its resampled
view). Before, an idle file sat in memory at least twice; every recording pass copied all the project's audio on the
message thread; a running export copied everything twice more; the peak builder copied every file on each request;
and a closed project's audio stayed held by a cache. Now the model, the engine's clips and Sampler pads, export
jobs and the peak builder all share one immutable buffer per Asset, freed when the last holder lets go (on the
control thread — the audio thread never holds one).

**Gates:** `[asset-sharing]` — an Asset's buffer is the same object across an import of another file, an undo, a
recording and a refused drop and a save; ten clips on one Asset leave exactly one buffer and the engine's build
copied nothing (it reports how many Assets it copied: 0); a cross-rate Asset has its buffer and one live view; an
export job holds the model's buffer while it runs, keeps a closed project's audio alive until it ends, then the
audio is freed. Every existing playback, render, export and reopen gate passes unchanged. Decodes are checked
finite once, where they are made (import, reopen, recording — a non-finite take is not kept, with a reason); the
open path also checks outside decodes and now hands the engine the opened project's buffers (it used to copy every
Asset at open). 416/416 local, Clang clean; the critic found no blockers (its should-fixes are in: a slot whose
takes were all refused records nothing; exact no-copy and lifetime gates).

**ADR-0059** accepted after a critic pass; amended before publication (the export-job gate follows ADR-0058's
cancel-on-replace law; the device hot-swap clause waits for a model call site).

## 2026-10-07 (small hours) — G5.3 done: Export v2 (ADR-0058)

**Now:** **G5.3** is in, headless-certified (all three checkpoints). **Next: G5.4 — decoded-asset sharing** (one
decoded buffer per Asset shared by reference, which also removes the export job's per-Asset copy), then G5.5–G5.7
and SS-6. The real-app drive of G5.1–G5.3 (SS-6 steps 1 and 5) waits for the desktop.

**Export stems (the last cp3 step):** the settings row's stems chooser offers **Mix Only / Mix + Stems / Stems
Only**. A stems export writes one file per **top-level** track and bus (those routed to the master) beside the mix,
named `<song> - <strip>.wav` (file-safe on every platform; duplicates get " (2)"); a track inside a bus is part of
that bus's stem, so the stems sum to the master's input. Each stem is what its strip contributes at the master's
input: the master sum with every other input at zero gain (so delay compensation is the mix's), the master's
inserts replaced by a pure delay of their latency (stems start exactly where the mix does), the master fader at
unity. One normalize gain spans every file; every file has its own dither noise.

**Gates:** `[export-options]` — float stems of a project with a track into a bus, a bus into a bus, a post-fader
send and a muted track sum to the mix within 1e-6, and the muted track's stem is silent; with a latent master
Limiter the stems start on the mix's frame; a master Compressor keyed from a track renders; one normalize gain
(the loudest file at -1 dBFS); identical stems in 16-bit carry different dither; cancel mid-stems leaves nothing;
the file-name rules (reserved names, illegal characters, trailing dots, the 100-character cap, accented case
variants kept apart); through the model, Mix + Stems and Stems Only write their files. The settings row at
1280x720 is gated and judged (the stems chooser's first item now reads "Mix Only"). 416/416 local, Clang clean;
the cp3 critic found no blockers and its should-fixes are in (a stem's helper-node ids retry with another salt on a
collision; the exception path removes every output's temporaries; the name cap and accented duplicates).

## 2026-10-06 (night) — G5.3 cp3, step 1: dithered 16 / 24-bit export (ADR-0058)

**Now:** cp3 lands in four small steps: **dither (in)**, **ranges (in)** — the render stops at the range's end, a
range past the end is refused before anything renders (the cp1 follow-up) — **normalize (in)**: a "Normalize"
toggle brings the export's peak to -1 dBFS with one gain (silence is never boosted) — then export stems.
Range gates: a range exports exactly its frames (equal to the full render's slice); a range running past the end
exports to the end; a past-the-end range fails without the render ever running; through the model the loop region
and the ruler range (which wins) export their lengths. **What a user gets:** a 16 or 24-bit export is TPDF
dithered (a "Dither" toggle in the settings row, on by default); float exports are never dithered and stay
bit-exact. **Gates:** `[export-options]` — 16 and 24-bit files equal a reference built in the test by an
independent implementation of the same TPDF law; dither off equals the one-shot writer's plain rounding; no two
files or channels share noise. 416/416 local, Clang clean. **CI:** 6ccd0b6 (audition) fails only the standing
macOS GPU exception (Windows green).

## 2026-10-06 (night) — G5.3 cp2: an export never destroys an earlier file (ADR-0058)

**Now:** **G5.3 checkpoint 2** is in, headless-certified. **Next:** G5.3 cp3 — WAV 16/24/32 with TPDF dither, the
range options (rendering only up to the range, which also refuses a range past the end before rendering), export
stems and normalize.

**What a user gets:** an export writes `<name>.<n>.partial` next to the destination and only renames it over the
destination when the whole file is written. Cancel at any moment — or a failure, a project switch or quitting the
app mid-export — leaves no partial file behind and an earlier export with the same name exactly as it was. A
`.partial` a crash left behind is cleared the next time you export to that name. A failure names its cause.

**Gates:** `[export-commit]` — the chunked writer's bytes equal the one-shot writers' (float, 24 and 16-bit, odd
chunking); cancel during the render and during the write (held one chunk into the temporary, which exists while the
destination is untouched) each leave no `.partial` and the earlier file byte for byte; success replaces the earlier
file with the reference bytes; an unwritable destination fails with its cause and leaves nothing; stale temporaries
of the destination go while another destination's stay; through the model, a replaced project's job and a job cut
off mid-write at exit leave no temporary and no file; a still-running replaced job's temporary survives a new
export's sweep. 416/416 local, Clang clean. The critic found no blockers; its hardening is in (the sweep skips live
jobs' temporaries; an unexpected exception fails the job instead of the app).

## 2026-10-06 (evening) — G5.3 cp1: export runs as a worker job (ADR-0058)

**Now:** **G5.3 checkpoint 1** is in, headless-certified. **Next:** G5.3 cp2 — the `.partial` sibling committed by
rename only on success (cancel or failure leaves an earlier file untouched), streamed writing with cancel per chunk.
Then cp3 (WAV 16/24/32 with TPDF dither, ranges, export stems, normalize).

**What a user gets:** Export no longer freezes the app. It renders and writes on a worker; the readout counts up
(render, then write), Cancel (or Esc) stops it — "Cancelling…", then "Export cancelled" with no file — and editing,
importing, undoing and even recording keep working while it runs; the file is the song as it was when you pressed
Export. A second Export while one runs is refused with its reason; opening another project quietly abandons the
running one; success stays quiet as before (the readout shows 100 %).

**Gates:** `[export-job]` — held at a test latch mid-render, the UI tick services the job (Rendering, progress
inside the render half) while a clip-gain edit, an import, an undo and a recording commit all complete, and the
released job's file equals the synchronous export made before them, byte for byte; Cancel ends it with no file and
no count; a second export is refused; opening another project retires it with nothing reported; destroying the
model with a held job returns. `YesDawExportJobCheck` (pure; RTSan and TSan legs) — the job's file equals the
synchronous render for a same-rate and a cross-rate Asset; held, cancelled, released and destroyed jobs.

**ADR-0058** (Export v2) is accepted after a critic pass; amended before publication so a successful export stays
quiet on the status line (the existing law). The cp1 critic's blocker (a test latch outlived by its job) and its
cancel-during-write finding are fixed. A ruler range wholly past the project's end was briefly refused only after the
render; cp3's range step refuses it before rendering again.

**CI:** d4ce08c's Windows re-run passed (the first attempt's 16.72 ms GPU frame was a runner sample; the renderer
was unchanged); d4ce08c and a77c6eb fail only the standing macOS GPU exception.

## 2026-10-06 (late afternoon) — G5.2 done: audition in the media browser (ADR-0056 cp2)

**Now:** **G5.2** is in, headless-certified (both checkpoints). **Next: G5.3 — Export v2** (its implementation ADR
first: a worker with progress and cancel, an immutable job snapshot, a temporary sibling committed only on success,
then WAV 16/24/32 with dither, range, stems and normalize). The real-app drive of G5.1 + G5.2 (SS-6 step 1: "open
the browser and audition a supported file") waits for the desktop — another session holds it for its drives now.

**What a user gets:** every file row in the browser has a play mark; click it (or select the file and press
Audition) to hear the file from its start on the monitor. It plays at the project's rate even when the file's rate
differs. Click again (the Audition button reads Stop), audition another file, press Play or Record, or close the
browser and it stops; it stops by itself at the file's end. Monitor Mute and Dim apply to it, the header meters show
it, but it is never in the mix: the loudness readout, an export, a bounce and a recording never contain it. With no
audio device it says "no audio device".

**Gates:** `[audition]` — two models in lockstep, one auditioning while the song plays: their device outputs differ
by exactly the file's samples from frame 0 (mono to both outputs), their live loudness readouts are identical, and
their exports are identical sample for sample; a new audition, Stop and Play end it; Mute silences it after its
5 ms ramp; no device refuses it with nothing published; a 44.1 kHz second plays 48 000 frames; the voice retires at
its end and is freed under the device-block watermark; the browser's button, play marks, second press, closing the
browser and Space all behave. `YesDawAuditionCheck` (pure, on the RTSan leg) proves the device-thread read never
allocates or locks. The browser shot gates the play mark (a file row paints one, a folder row none). 415/415 local,
Clang clean. The critic found no blockers; its should-fixes are in: Record's no-input path now stops an audition
too (gated with a real Record), and opening another project stops it.

**CI:** d4ce08c's Windows job failed the timeline GPU frame gate at 16.72 ms against 16.6 ms. Not the standing
macOS exception, so not excused: that commit changed only 12 lines of new browser layout constants in the
renderer's include set (7f2303a passed Windows with the same renderer). Re-run on the same SHA: Windows passed.

## 2026-10-06 (afternoon) — repair: a shortcut pressed while the app is busy means what you pressed (ADR-0057)

**Now:** done, outside the G5 line (Dan asked for the product fix of the drive finding below). **Next:** unchanged —
G5.2 cp2 per the entry below. [Evidence](docs/evidence/2026-10-06-key-time-modifiers.json).

**What a user gets:** a quick Ctrl+Z, Ctrl+S or Alt+arrow tapped while the app is busy (an edit that rebuilds the
engine, a project load) does what was pressed. Before, Windows' JUCE read Ctrl/Shift/Alt when the app got round to
the key, so the tap could arrive as a bare Z (zoom) — or a bare Z followed by Ctrl became an accidental Undo.

**How:** the shell's window publishes the modifiers Windows recorded with each key (its message-synchronised state)
while that key is handled, and the Command router builds the chord from them. No JUCE patch. JUCE text fields,
popup menus and Ctrl+click keep JUCE's own reading (out of scope, written in the ADR).

**Gates:** `[key-time-modifiers]` (the real shell on a hidden native window; the thread's key state says "Ctrl
held", the physical keyboard says "up"; a real key through JUCE's own message loop) — red with the router line
reverted (`"timeline.zoom.selection" == "edit.undo"`), green with it, 20/20 repeats. Real app: the drive's new
`KeyWhileBusy` holds the app's UI thread still while the chord goes in, so the whole chord waits in the queue every
time; SS-2 step 10 fails on the code without the fix (Ctrl+Z -> zoom, Ctrl+Shift+Z -> zoom, Z then Ctrl -> Undo)
and passes with it. Quiet batch on the fix: **ss1–ss7 PASS, 420 assertions**. ctest 414/414, Clang clean. The
drive's `Key` keeps its idle wait and modifier holds.

**Lessons:** a timing drive needs a quiet machine — another session's build pushed launches from 1.3 s to 6–19 s
(SS-3's B6 FAIL), and a foreground app refused the drive focus; both re-ran clean. Recreating a window resets the
thread's key state to the physical keyboard, so a test that sets that state must let the window settle first.

**CI:** the fix `f8174d7` (run 37534088888) and the `KeyWhileBusy` primitive `1dbd0d2` (37534125736) are green on
all 10 jobs, macOS included; the ADR `7f14c16` and this entry `365a55d` passed the docs path. `e3102ab` (the SS-2
script only) went red on Windows alone: `YesDawTimelineGpuCheck` sustained frame 17.91 ms vs 16.6 (max 20.04,
13 slow frames; run 37537021589). Every input to that check is byte-identical to the green `1dbd0d2` (it includes
nothing this repair changed), and the next two code runs on top, `6ccd0b6` and `0ab9c84`, are Windows-green:
runner timing variance near a thin margin, as with `b26c9e5`. Not rerun, not called green, threshold unchanged;
noted for the G6.2 runner baseline.

## 2026-10-06 (afternoon) — G5.2 cp1: the media browser (ADR-0056)

**Now:** **G5.2 checkpoint 1** is in, headless-certified. **Next:** G5.2 cp2 — the **audition voice** (hear a file
before importing it, on the monitor path only), then the real-app drive check of G5.1 + G5.2 when the desktop is
free (drives take the mouse and keyboard; the overnight hands-off grant has ended).

**What a user gets:** press **Y** (or View > Browser) and the editor dock shows a **Browser** tab:
- **Files** — one folder at a time: `..`, its folders, then only the audio files YES DAW can import, with each
  file's format, rate, channels and length (read from the header only when the row is on screen). A file that
  cannot be read says why, in red ("not a readable MP3 file"). It remembers the last folder.
- **Project** — this project's audio, named by the clip that uses it, ready to place again without re-importing.
- **Recent** — the last 20 audio files imported from anywhere, newest first; a file that is gone says "missing".
- Double-click, the Import button or Enter imports a file on the selected track at the playhead (exactly what
  Ctrl+Shift+I does); Ctrl / Shift+click several and drag them onto the lanes (exactly an OS drop: consecutive
  tracks, one undo step; below the last track makes new ones); a project Asset placed this way is a new clip on
  the same audio, no copy.
- Keyboard only: Tab to the list, Enter, arrows, Enter opens a folder or imports, Esc puts the selection back.

**Gates:** `[browser]` (8 cases) — keep equals Ctrl+Shift+I (same track, tick and Asset row); a two-row drag equals
an OS drop and undoes in one step; a project Asset adds a clip and no Asset; unreadable and missing files show
their reasons in the row and on the status line and change nothing; Files order with non-ASCII folder and file
names; Recent and the browser state survive a new shell; the keyboard path; a 1000-file folder reads only the
painted rows' headers (and a scroll only the new ones); a rebuild after another surface's import keeps the
selected rows. `[ui][screenshot][browser]` paints a folder row, a file's facts and a red reason in their
columns (agent visual judgment: rows read name -> facts; the first draft put facts 1,700 px from their names at
1920 wide — fixed). 414/414 local, Clang clean. The critic found no blockers; its should-fixes (over-eager
rebuild resetting the selection, ANSI file names on the status line, an unbounded header cache) are fixed.

**CI:** Linux and macOS failed to build `YesDawResampleCheck` from dd439e8 (a copying structured-binding loop
under -Werror); fixed in 7f2303a. 79c75c8 fixed a real macOS-only flake (two files of one drop sharing an id).

**Fixed: names outside the ANSI code page crashed the app on Windows.** `path.string()` converts through the ANSI
code page and throws on a character it cannot hold, so a project named in kanji (window title, Open Recent), a
drop of a kanji-named file (every drop named each file), a refused file, a Sampler pad or a MIDI export with such
a name threw. All 27 sites now go through `io::utf8Text`; the gate (a kanji project, a kanji WAV dropped, a kanji
junk file refused by name, Open Recent) threw "No mapping for the Unicode character" before the fix.

## 2026-10-06 (day) — G5.1 in: import formats (ADR-0054) and cross-rate audio (ADR-0055)

**Now:** **G5.1** (both checkpoints) is in, headless-certified. **Next:** the real-app drive check of the new
import surfaces (pending: the drives take the mouse and keyboard, and the overnight hands-off grant has ended —
they run when the desktop is free again), then **G5.2 — the media browser** (its ADR first).

**What a user gets:**
- WAV, AIFF, FLAC, Ogg Vorbis and MP3 import through Ctrl+Shift+I, a drop on the timeline and the Sampler pads,
  and reopen the same on every platform; the project keeps each file's own bytes. Every refusal says why
  ("junk.mp3: not a readable MP3 file", "6 channels (mono or stereo only)", "AIFF compression 'ulaw' is not
  supported", "unsupported sample rate (4000 Hz)"). A drop of several files lands them on consecutive tracks —
  making the tracks it needs — as one undo step.
- **Files at another sample rate import and play at their true speed and pitch** (8 kHz to 384 kHz): a 44.1 kHz
  file in a 48 kHz project plays through a resampled view built once on import (live tier) and exports through
  a long-kernel one (offline tier); the clip keeps its source window in the file's own frames; split, trim,
  slip and stretch work through the rate ratio; a 44.1 kHz Sampler pad sounds at its own pitch.
- Copy and paste keep every clip setting (stretch, reverse, fade shapes, colour, mute were dropped before).

**Gates:** `[import-formats]`, `[cross-rate]`, `[resample]`: lossless decodes equal the source PCM; MP3 lengths
pinned on every platform; a 44.1 kHz sine plays live at -95.5 dB and exports at -132.7 dB error against the
analytic 48 kHz sine (gates -60 / -80); a 96 kHz file's 30 kHz tone is kept from aliasing (-85 / -116 dB);
a click lands where the ratio puts it; a split renders bit-identically; slip moves 441 Asset frames for 480
ticks; reopen and the self-check play cross-rate bundles. A three-minute stereo 44.1 kHz file's live view
builds in 0.88 s (export view 3.2 s). 414/414 local, Clang clean.

**CI:** a CI-only race in the G4.7 master-inserts gate (the Limiter face filled on the next UI tick) is fixed
at the source in 130d184; macOS otherwise fails only on the standing GPU sustained-frame exception.

**Fixed after the G5.1 critic and CI (2026-10-06 afternoon):**
- **A multi-file drop could refuse a file** (macOS CI on 2665f20: 2 tracks, not 3). Every file in one drop took
  its Asset id from the same project state in the same millisecond, so two fast copies got one id and the second
  was refused. Session ids now carry a per-model serial; the gate makes 1000 ids back to back (all differ) and
  drops 24 tiny files at once (24 Assets).
- **Opening a cross-rate project while another rate's project was open** built its views at the old rate and
  refused the open; views are now built at the opened project's rate (gate: open a 48 kHz bundle with a
  44.1 kHz Asset over a 44.1 kHz project; it plays exactly as before). Views of Assets that left the project
  are freed.
- **A drop below the last track** landed on the last track; it now starts a new track there, and a MIDI file
  after the audio gets its own track instead of stacking.
- Trim and reverse on a 44.1 kHz clip keep the rate ratio (gated); the demo self-check renders through the
  view-frame project.

## 2026-10-06 (late night) — G4 done: SS-5 "Mix the song" passes with built-ins

**Now:** **G4 is certified** (plan §6 exit: logical SS-5 with built-ins, every earlier journey restored).
**Next: G5 — project lifecycle**, starting with **G5.1** (import formats and cross-rate audio; its
implementation ADR first, under ADR-0010). [Evidence](docs/evidence/2026-10-06-g4-exit.json) ·
[montage](docs/evidence/2026-10-06-g4.png).

**What a user can now do, end to end on the real app (ss7):** route a vocal to a bus with EQ and a
compressor, send it to a new reverb bus, ride the bus fader in Write while the song plays (a Bus Fader lane
appears; Write returns to Touch), keep the reverb solo-safe, put a Limiter on the master, read the mix's
loudness, dim or mute the monitor, and export the mix to a WAV.

**Gates:** quiet batch **413/413** (ss1–ss7) on 36e471b; ss7 now drives SS-5 as the plan writes it.
The probe adds each strip's soloSafe, the export count / progress and each fader's knob.

**Open observation:** one ss7 run lost the Compressor's editor between a Threshold drag and the Presets
click (the click fell through to the timeline); the next three runs passed it. Unknown cause — if it
recurs, capture fxEditor.visible per tick around the drag before acting.

**CI:** e1d0f43 green on every job but Windows (running at writing; macOS passed this time); the later heads
are pushed one at a time so each code commit gets its own run.

## 2026-10-06 (late night) — G4.7 done: the master strip (ADR-0053)

**Now:** **G4.7** is done. **Next: the G4 exit** — ss7 does SS-5 as the plan writes it (a reverb bus fed by
a send, Write automation of the bus fader while playing, the reverb bus solo-safe, export), then G5 → G6.
[Evidence](docs/evidence/2026-10-06-g47-master-strip.json).

**What a user gets:**
- **The LUFS readouts are real.** The header's LUFS button and the mixer's INTEGRATED / TRUE PEAK cards show
  the loudness of what you played — measured from Play or from a locate while playing, held when stopped,
  one listen across a loop wrap or an edit, "~" if the UI stalled long enough to drop audio.
- **Master Dim / Mute**: DIM (amber) and MUTE (red) on the header MASTER card, in the Transport menu and the
  master's menu. The speakers get 20 dB less or nothing after a 5 ms ramp; the header meter still shows the
  signal; exports and renders are never touched; the status line names them; nothing is saved or undoable.
- **The master's inserts are on its pane** (under TRUE PEAK, above the meters): an empty slot lists the
  kinds with the Limiter first, a double-click opens the editor with the Limiter's gain-reduction meter, the
  slot menu bypasses.

**Gates:** `[loudness-live]`, `[loudness-tap]`, `[g47]`, `[master-inserts]`. Quiet batch **396/396** on the
final build (489b653); ss7 Step 13 drives the master strip on the real app (the readout read -28.4 LUFS on
the song stem). One critic pass: both should-fixes taken (the compact pill face is opt-in; the ramp is timed
at the device rate); one claim rejected with a test (shuttle does not feed the meter).

**CI:** a8ce649 and b785626 green except macOS, red only on the standing GPU sustained-frame exception (plan
§8.2); e1d0f43 in progress and 489b653 pending at writing.

## 2026-10-06 (night) — G4.6 done: automation you can write, hear, draw and move with clips

**Now:** **G4.6** (ADR-0052) is done. **Next: G4.7 master strip** (dim / mute, a header loudness readout
that is actually fed, a limiter editor reachable from the master) → G5 → G6. [Evidence](docs/evidence/2026-10-06-g46-automation-v2.json).

**What a user gets:**
- **Modes Read, Touch, Latch, Write, Off.** Touch writes while you hold a control; **Latch** keeps writing
  your last value until stop; **Write** writes every lane the selected strip owns from play to stop (touch a
  control to ride it) and then returns to Touch — the pass and the mode change are one undo step.
- **You hear a ride while it lasts** — fader, pan, sends, insert parameters and instrument parameters.
- **A second pass rewrites the bars it covers** (it used to be refused); the automation outside a pass is
  untouched. A loop wrap or jump back commits the pass so far and carries on.
- **Lanes per track, stacked under the track** (the A key / button shows the selected track's lanes; saved
  with the project's view): each lane the track owns, named, one under another; the last lane's chooser
  starts a lane for another target; other shown tracks offer **+ Lane** there.
- **Pencil** draws (one point per snap step), **Shift+Pencil** draws a straight line, the **Eraser** sweeps
  points away — one undo step each.
- **Edit > Automation Follows Clips**: moving clips in time (including neighbours Shuffle moves) carries
  their automation, locked to the audio across tempo changes; one undo restores clips and automation.

**Gates:** `[automation-v2]`, `[automation-ride]`, `[automation-tools]`, `[follow-clips]`, `[budget]` and
render gates (the audio outside a replaced span is unchanged, a Write pass sounds its values, a clip moved
with its automation renders the same audio shifted). Quiet batch **385/385** on the final build; ss7 drives
the lanes (A, Shift+Pencil line, Eraser, Ctrl+Z) on the real app. Two critic passes; every should-fix taken.

**Repairs found on the way:** the engine's event budget undercounted dense lanes (a possible audio-thread
assert — now refused with a reason); a latched ride snapped back when another edit rebuilt the engine;
Shuffle neighbours left their automation behind; the drives sent key chords before the app was idle
("Ctrl+Z" read as "Z" — the drive now waits; the product side is its own task).

**CI:** heads through 677ab52 are green except macOS, red only on the standing GPU sustained-frame exception
(plan §8.2); e005b43 (the critic fixes) in progress at writing.

## 2026-10-06 (evening) — G4.5 done; four real bugs fixed; G4.6 automation v2 decided (ADR-0052)

**Now:** **G4.5** is done and **ADR-0052** (automation v2) is accepted. **Next: G4.6** — stacked lanes,
real Write and Latch, span-replacing rides you can hear, Pencil / Shift+Pencil line / Eraser, "automation
follows clips" — then G4.7 master strip → G5 → G6. [Evidence](docs/evidence/2026-10-06-g45-and-repairs.json).

**G4.5 — what a user gets:** Ctrl-click (Cmd on Mac) a Solo cell, in the mixer or the track list, solos only
that strip (one undo step; Ctrl-click the only soloed strip to clear it). The header's **SOLO** beside Loop
lights amber while anything is soloed; a click clears every solo. The strip and track-header menus carry
Solo Exclusively, Clear All Solos and Solo Safe. Quiet batch **377/377**; critic acceptable.

**Bugs found and fixed on the way (each proven by a test that failed first):**
- A **bus** strip's S / M cell did nothing unless some strip was already selected.
- **Instrument-parameter automation could not be saved**, and a project with **bus-send automation could not
  reopen** — schema **v33** rebuilds the automation tables (every existing lane and point kept).
- **Automation played 1.56x later than drawn** at 48 kHz / 120 BPM: the lane stored frames where the engine
  reads musical ticks. Existing projects keep their stored points — the sound is unchanged; the lane now
  shows each point where it really plays.
- **MIDI clips' notes were drawn ~1.56x late** in the arrange view (display only).

**Open observation:** one ss7 run inside a busy batch (two background agents working) missed six steps
(clicks/keys landing without effect); the same build passed ss7 alone twice and the whole batch when quiet.
Load is the supported suspect, not proven — if it recurs, the drive's input waits are next.

## 2026-10-06 (later) — G4.3 and G4.4 done: New Bus, Route to New Bus, compressor sidechain

**Now:** **G4.3** and **G4.4** are done. **Next: G4.5** solo/mute UX (solo-clear control, Ctrl-click
exclusive solo; solo-safe is already in the strip and header menus) → G4.6 automation v2 → G4.7 master
strip → G5 → G6. [Evidence](docs/evidence/2026-10-06-g43-g44.json).

**What a user gets:**
- **G4.3:** an empty send well's chooser (and the strip menu's Add Send) ends with **New Bus** — the bus and
  a unity post-fader send to it in one undo step; the OUTPUT chooser ends with New Bus too; the track
  header has **Route to New Bus**. New buses are named "Bus N" past any name already taken.
- **G4.4 (ADR-0051, accepted after a critic, committed before any code):** a Compressor's editor has a
  **Sidechain** chooser — None, the tracks, the buses; its own strip and anything that would loop the
  routing are disabled. The key is the source's pre-fader signal after its inserts, so a kick with its
  fader all the way down still ducks the bus (the "ghost trigger"); muting or solo-muting the source
  silences the key (ADR-0014). Saved in schema **v32**; one undo step; the strip's **SC** badge is now real;
  a keyed track or bus cannot be removed until its sidechain is cleared (the status line says so).

**Proof:** G4.3 batch 367/367 (worst launch 1,500 ms, paint p95 3.96 ms); G4.4 batch **372/372** (1,385 ms,
3.81 ms); local suite 384/384; Clang clean. Render gate: the ghost kick ducks the Pad's bus by 27 dB;
unkeyed, muted, solo-muted and bypassed renders stay steady; set-then-cleared renders bit-identically to
never keyed; a keyed Compressor is a latency-compensation convergence point. Both critics acceptable.
CI: `cb7b0ad` (G4.3) and `0ac78ad` (G4.4 engine) green except macOS on the standing GPU timing exception.

## 2026-10-06 — G4.2 complete (faces, drag reorder, FX presets); G4.3 sends and buses next

**Now:** **G4.2 is done.** One Release build (code `b26c9e5`, harness `a01a90e`) passed **all seven
journeys, 362/362** (SS7 91: drag-reorder + undo, and a Compressor preset saved through the name prompt,
loaded back after an edit, undone in one step). Worst launch 1,406 ms, paint p95 3.88 ms; local suite
380/380; Clang warnings clean. Exact-code CI (`a01a90e`, same app source): every job green except macOS
on the standing GPU sustained-frame exception (18.77 ms). **Next:** G4.3 — New Bus in the send and
output choosers, Route to New Bus on the track header (code and `[sends-v2]` gate written, being
verified) → G4.4 sidechain → G4.5–G4.7 → G5 → G6. [Evidence](docs/evidence/2026-10-06-g42-close.json).

**Shipped since the last entry (each its own commit):**
- **cp5** transfer curves for the compressor and limiter; faces drop whole when rows run short.
- **cp6** drag an insert slot to a new place in its chain — one undo step; a stale-carry bug that merged
  fader undo steps was found and fixed with a test that fails without the fix.
- **cp7 FX presets** under the new **ADR-0050** (accepted after two critic passes, committed before any
  code): per-user `.yesfx` files of real values by stable parameter name (the EQ's six bands carry an
  ordinal), all nine kinds, a Presets button and menu in the editor, Save Preset… asks for a name, a load
  is one undo step, and a bad file is refused whole with one reason (21 refusal cases gated). Critic on
  the code: acceptable; its NUL-byte suggestion was taken.
- **Product fix — garbled text.** The compressor face showed "Gain reduction (dB) â□□ not running" (seen
  in a drive screenshot). Root cause: JUCE reads a plain `const char*` as ASCII. Six more such strings
  (two tooltips, a piano-roll hint, the Edit menu's "Undo History…") were broken on every platform, and
  24 hover hints using `\u00b7` were right on an English Windows machine only by luck ("Â·" on Mac and
  Linux). All fixed; action labels are now ASCII ("Undo History..."); a new source audit with a negative
  control makes any such literal a red test.
- **Tools:** `clang-warnings.ps1` no longer dies before printing a finding; the native file chooser that
  drops typed characters (seen once in ss1: `sers\...` for `C:\Users\...`) is retyped, bounded, and
  Enter is pressed only on an exact readback.

**CI notes (raw numbers in the evidence file):** `b26c9e5` went red on **Windows and macOS**, each only
on the GPU sustained-frame budget (Windows 17.06 ms vs 16.6). Windows is not covered by the macOS-only
exception, so it was investigated: every input to that check is byte-identical to the green `394d752`,
and the next run on the same app source is Windows-green — runner timing variance near a thin margin.
Not rerun, not called green, threshold unchanged; flagged for the G6.2 runner-baseline review.

## 2026-10-05 (late) — G4.0b proven in the real app; G4.2 cp2–cp4 faces in

**Now:** one Release build of `765d2e6` passed **all seven journeys, 349/349** (SS7 78 incl. the
keyboard-only Step 11 and the compressor face), worst launch 1,379 ms, paint p95 4.00 ms, and the full
local suite 380/380. G4.0b's real-app proof is done; **exact-code CI on `765d2e6` is the open item**
(GitHub was still building it at this update). Dan granted the shared desktop for the whole night.
**Next:** G4.2 cp5 transfer curves (limiter + compressor) → cp6 drag-reorder of insert slots → cp7
presets → G4.3–G4.7 → G5 → G6. [Evidence](docs/evidence/2026-10-05-g40b-g42-drives.json).

**Shipped since cp1 (each its own commit, local suite green each time):**
- **G4.2 cp2** compressor/limiter face: a live gain-reduction meter fed by taps the engine harvests at
  create (one acquire-load; RT-safe per critic). Peak hold ~1.5 s; honest "not running" without a graph.
- **G4.2 cp3** delay face and **cp4** reverb face, each drawn from a private copy of the real node fed a
  click (EQ face's law). The reverb gate checks the DSP against its settings: 1 s decay falls 60 dB in
  960 ms, 0.5 s in 480 ms, 100 ms pre-delay starts at 120 ms. Window caps keep an edit's work ~3 ms (B1).
- Parameter rows read human names ("Threshold", "Time L", "Decay") instead of stable ids.
- **Product fix:** screen-reader adoption is edge-triggered (a level-triggered poll dragged Tab back).
- **Harness:** `WaitPopup` waits on the app's modal popup menus then 200 ms (JUCE's popup mouse tracker
  undoes a key sent before its first tick — proven from the source); native choosers re-establish
  verified focus while a dialog is still building; every FAIL prints the app's state (`[ctx]`).
- **CI:** the docs-only fast path now diffs the whole pushed range; it had reported three code commits
  green in 7 s under a docs head. Two Linux/macOS `-Wsign-compare` reds were fixed (`a608efc`).

**Raw failures on the way** are listed with causes and repairs in the evidence index; all have a
supported cause except one ss5 Scissors click (no `[ctx]` existed yet; not reproduced in two batches).

## 2026-10-05 — G4.0b cp1: the keyboard Control target (code + gates; drive pending)

**Now:** G4.0b's code and headless gate are in; the real-app proof (the new ss7 Step 11) and exact-code
CI are pending. **Next:** run ss7 on the built app in a hands-off desktop window, record it, then G4.2 cp2.

**What a user gets:** Tab / Shift+Tab walk every visible, enabled control in reading order (header, rail,
arrange, inspector, dock), with a purple ring on the target. Enter clicks a button, or starts a choice,
value or text interaction. Arrows adjust only that interaction: a chooser previews and Enter applies it;
a slider or the painted mixer fader moves live (fader by 1 dB, Shift 0.1 dB) as one undo step. Esc
restores the starting value; a second Esc ends navigation. Space is transport throughout. An open FX
editor, keymap editor or undo history keeps Tab inside it; closing it returns the earlier target. A
mouse press ends navigation. A screen reader moving onto a control makes it the target.

**How:** pure rules in `src/ui/ControlTarget.h` (priority table, reading order, traversal, stale-target
recovery, dB step); the shell half in `src/ui/MainComponentControls.cpp`, which performs every effect
through the control's own path (click, `onChange`, the slider's drag bracket, the painted fader's drag
verb). The router runs first in `keyPressed`; the probe gains `controlTarget` and each strip's
`linearGain`; ring tokens live in `UiTheme::Layout`.

**Gates:** six `[control-navigation]` cases (336 assertions): the priority table; a full Tab lap with
unique ids; Space while navigating; Enter on Play making exactly a mouse click's dispatches without
returning to zero; Esc dispatching nothing; chooser preview/apply/restore; a fader stepping +3.1 dB as
one undo step while Right/Left/Up do not leak (negative control: Right locates before Enter); the EQ
panel scope and Close restoring the target; text entry on Enter; a hidden control moving the target.
Full local suite 380/380 on the final code. ss7 Step 11 drives the same gestures with real keys.

**Critic:** no blocking finding. Taken: a cached target widget so the per-tick screen-reader poll does
not walk the tree, an O(n) reorder, and the focus-loop invariant documented. Rejected after checking the
code: an "unpaired" gesture end (the gesture always opens inside `onFaderDragged`; the flag is
idempotent) and ScrollBar/ListBox stops (Viewport scroll bars are `ScrollBar`s, already skipped).
A self-review negative control also caught a test that could not bite (Right is unbound in Mixer focus);
the test now holds Arrange focus and proves Right is live before the interaction.

**Deviation log:** choosers preview and apply on Enter, so stepping past an audio device never reopens it.
FX-parameter keys keep the mouse drag's undo granularity (that verb does not coalesce). The painted pan,
sends, M/S cells and insert slots are not targets yet; G6.3 audits whole-shell coverage. A text field is
announced on Tab, not given accessibility focus, because JUCE would also hand it the keyboard. Headless
gates reach screen-reader adoption through the harness, since no window means no native handlers.
The 2026-10-05 waveform observation is closed: the stem is a constant −19 dBFS signal, drawn correctly.

## 2026-10-05 — G4.0a certified: all seven journeys pass on one build

**Now:** G4.0a is **certified**. **Next: G4.0b** (keyboard Control target, ADR-0049), then G4.2–G4.7 →
G5 → G6 → Usable-song certification. Dan asked the agent to keep delivering autonomously and granted
one hands-off shared-desktop window for this batch; that window is now released.

**Result:** on DESKTOP-7LC339R, one Release build of `76ffbac` was driven serially, with no concurrent
build. Its code is byte-identical to `30bc84c`, whose
[CI 34315558059](https://github.com/DanielKinsner/yes-daw/actions/runs/34315558059) passed all ten jobs
with no timing exception. The unchanged driver passed **SS1 45, SS2 33, SS3 55, SS4 19, SS5 70, SS6 49,
SS7 40: 311/311 assertions**. All eleven launches met B6 (worst **1,404 ms** of 3,000). B2 paint p95 was
**4.05 ms** (limit 8), B3 had zero callback removals, B4 had no placement-edit rebuilds and B5 had zero
burst underruns. The same build passed the full local suite, **380/380** (107 s). App SHA-256
`88F896E6…62A6`; the [certification index](docs/evidence/2026-10-05-g40a-certification.json) lists
every log and number.

**Refuted on the way (kept as a raw result):** setting the chooser filename by `WM_SETTEXT`, so that
keyboard focus could not drop typed characters, does **not** reach Windows' own filename model
reliably. SS1 Save As read back the exact path but saved the dialog default `Documents\Untitled.yesdaw`
(44/45), while SS2's identical New/Import choosers accepted it (33/33). The experiment was reverted
before the certifying batch. Its stray bundle was moved out of Documents into the agent scratchpad.
The original 2026-09-08 SS7 blank readback stays unexplained and was not reproduced (SS7 40/40). The
structured readback added then remains in place to capture any recurrence.

**Critic:** a separate agent verified every count, budget, hash, diff and CI claim against the raw
logs and `gh`; it asked for the local-suite row (added). Its suggestion to credit `30bc84c` with fixing
the old readback was rejected: that failure occurred on that same candidate, so no cause is claimed.

**Rubric:** no new defect in the reviewed screenshots; rows 1–4 FIX (G6) stay open. Parked observation
to verify: song-fixture clips draw a near-flat waveform at default zoom (quiet passage vs paint bug).

## 2026-09-08 — G4.0a: remove duplicate saved-song loading, assert every startup

**Now:** G4.0a remains open. Reviewed repairs are pushed as
`30bc84c6f2a8add75da18f0fbd18276aa3237cdf`.
[CI 34315558059](https://github.com/DanielKinsner/yes-daw/actions/runs/34315558059) completed
**success, all ten expected jobs**, on that exact code SHA. macOS passed **378/378**, including
`YesDawTimelineGpuCheck` (2.80 s test duration, not a frame measurement). **No timing exception was
applied.** This certifies those CI results, not the incomplete G4.0a native journey. Dan authorized the shared
desktop for the night; input access is available, and no app drive is currently running. The
corrective `9060aeb` app passed all seven journeys' 300 assertions, but the
critic found an unasserted **3,002 ms** saved SS3 launch. Three diagnostic launches of that same
saved song then measured **3,010 / 3,079 / 2,944 ms**, with the new common B6 gate correctly failing
the first two. These are raw failures, not a tooling exception or certification reruns.

**Current repair:** the shell decoded a validated bundle, then the model reopened it, rehashed its
69 MB audio asset and copied the decoded audio. A move-only prepared bundle now carries the same
validated DB/snapshot and owned audio into the existing adoption path. Full integrity checks,
failed-load preservation, empty-project playback and recovery remain required. Every drive
`Launch` now asserts the unchanged **3,000 ms** B6 budget; existing SS1 assertions stay intact.
This implements the accepted G4.0a startup contract, with local tests and the same real-song
journeys as its gates. Release build and CTest passed **380/380** (198.39 s), with separate
correctness/adversarial/testing critics finding no actionable issue. Same-song diagnostic launches
improved to **2,837 / 2,717 / 2,700 ms**. The final repair's exact-code CI result is recorded above.

**Earlier verification interruption:** the same repaired app passed SS2–SS7, but SS1 reported one
ASUS input discontinuity during the edit burst (B5); startup and other budgets passed. Direct
WASAPI capture outside YES DAW/JUCE independently measured three post-start discontinuities from
the default ASUS virtual microphone in 30 seconds. That proves an environment contribution, not
the cause of every extra JUCE counter increment. Its raw FAIL remains recorded.

Realtek Stereo Mix was available but disabled. The agent enabled it through Windows Sound and
verified a 30-second direct capture with zero post-start discontinuities. A temporary Stereo Mix
default-input profile then passed SS1 B5 with **zero** underruns and B3 with **zero** callback
removals, but failed B2 at **11.45 ms p95** against the unchanged **8 ms** budget. The batch stopped.
An unchanged-script trace reproduced **11.75 ms**: full edit redraws were slow, and correctly
remained in the rolling 256-paint sample during subsequent 1–2 ms playback paints. Attribution
measured only ~1.7 ms inside canvas drawing; pinned JUCE's native offscreen context performs a
full GPU-to-CPU image readback after that drawing finishes.

**Paint repair:** Windows now keeps the same static canvas in an explicit software image cache,
preserving logical invalidation, transparency, physical scaling and the separate playhead layer.
Other platforms retain their existing cache. The unchanged traced SS1 passed **45/45**, with
**7.65 ms** B2 p95 (0.35 ms margin), zero B5 underruns and **2,606 / 2,808 ms** startup. Independent
cache critic and three-size visual comparison found no new defect. The final Release candidate
passed **380/380** local tests (192.05 s). Its untraced batch passed **SS1 45, SS2 33, SS3 55,
SS4 19, SS5 70 and SS6 49** assertions. **SS7 failed** at the native New filename readback,
before confirmation or project creation. The original log conflated unavailable readback with
empty text and sampled focus later; its cause remains **unknown**, so the batch is incomplete.
All eleven startup measurements were within 3,000 ms (worst **2,905 ms**); SS1 measured **7.39 ms**
paint p95 against 8 ms, with zero underruns and callback removals.

**Chooser investigation:** a traced SS7 completed all 40 assertions, and 20 same-process New/type/
cancel attempts delivered their paths correctly. These are diagnostic non-reproductions, not a
repair or certification. A fresh-process comparison delivered eight exact paths, then stopped
because its diagnostic Escape cleanup did not close the chooser; that separate failure is retained.
One further fresh-process diagnostic reproduced the Escape cleanup failure with an exact filename,
stable focus and all modifiers released; it stayed open through another 1,135 ms of read-only
observation. That is separate from the original filename refusal. Structured readback now captures
the resolved Edit HWND, success, character count, elapsed time,
last-error and text together. The exact gate-time focus/readback is reported, and tests distinguish
empty text, unavailable readback and focus loss while refusing confirmation for every failure.
The 500 ms readback timeout, all input timing, path equality and downstream assertions are unchanged.
Separate critic review covers these diagnostics. A final, single SS6 → SS7 predecessor diagnostic
used separate harness processes, unchanged tracked scripts and a bounded passive WinEvent observer.
SS6 passed **49** and SS7 **40** assertions; the observer recorded **4,494 / 787** events, zero drops,
and successful registration/unhooking of all 12 hooks. This is another non-reproduction, not an
explanation. The independent critic found **no evidence-supported corrective hypothesis** in the
trace. Per §8.2, stop repeating the same approach and keep dependent work blocked. The original
failure cannot be attributed retrospectively from its insufficient log. Its next useful evidence
is an actual refusal captured by the structured readback/observer, or a controlled environment
comparison that reproduces it; do not retype blindly, widen timeouts or rerun for a lucky PASS.
No G4.0a certification or later feature advancement is claimed.

**Host setup restored:** the agent restored ASUS AI Noise-Canceling Microphone in all three capture
roles and Stereo Mix's original disabled state through Windows Sound. Read-only endpoint queries
verified all six capture/render roles match their original IDs and Stereo Mix is state 2 (disabled).
Render remains Realtek Speakers. No app or observer remains running. Both profiles, raw failures,
and restoration measurements are indexed in `docs/evidence/2026-09-08-g40a-single-open.json` and
`docs/evidence/2026-09-08-g40a-predecessor-diagnostic.json`.

**Evidence:** `build-ci/g40a-corrective-*.log` and
`build-ci/session-shots/2026-09-08-g40a-corrective/` retain the 300-assertion batch, including all
**eleven** startup measurements. The earlier handoff incorrectly said twelve; its JSON contains
eleven. `build-ci/g40a-saved-baseline-timing.log` retains the three diagnostic results. These
artifacts are local only. No new visual regression was found; existing G6 rubric FIX rows remain.

**Hardware preflight (not final package proof):** Realtek at 48 kHz granted **480 frames** on all
three WASAPI modes, exceeding the locked **128-frame** playback limit. The measurement-generated
result is FAIL (`playback_block_exceeds_target`); `build-ci/g40a-hardware-preflight/20260908-205959/`
contains raw JSON/logs and checker identity. No hardware PASS is claimed. Software work can
continue; Usable-song certification still requires a passing device/machine and final package proof.

**Blocked handoff:** code, local tests, independent review and exact-code CI are complete for this
repair; the original native filename refusal remains unknown. Investigation exhausted the current
supported hypotheses, so dependent implementation stops under §8.2. Preserve the failing log and
capture any actual recurrence with the committed structured readback before choosing another
correction. A controlled environment comparison needs a stated hypothesis and verified setup.
Only after restored required journeys may work proceed to G4.0b, remaining G4.2–G4.7, G5, G6 and
Usable-song certification. Those later implementations remain unstarted. No owner code/visual
review is needed. Logs, screenshots, observer sources and local fixtures named in the evidence
indexes are local-only; git transfers the code, indexes and CI links, not those files.

## 2026-09-08 — G4.0a handoff: code CI green, corrective native rerun awaits access

**Now:** the authorized **Usable-song milestone is unfinished**. Current code is
`9060aeb0407e2cec0ddfe2a5de1cb0031d8758b7`. [CI 34297336345](https://github.com/DanielKinsner/yes-daw/actions/runs/34297336345)
completed **success, all ten jobs** on that exact SHA. Corrective Release build and local CTest
passed **380/380** (194.10 s). macOS passed all 378 tests, including the previously failing Save As
cases and `YesDawTimelineGpuCheck` (2.61 s test duration, not a frame measurement). **No macOS timing
exception was applied.** Separate corrective critic review completed with no blocking findings.

**Concrete blocker:** the previous shared-desktop hands-off window was released after its native
batch. A new window was requested for the macOS correction's required real-app rerun; no answer has
arrived. No further mouse/keyboard input was sent. **G4.0a is not yet certified:** the prior code's
seven scripts passed 300 assertions, but those results do not certify the changed corrective code.
Build/headless checks and exact-code CI were completed while native input access remained pending.

**Next when the window is available:** run `tools/session-drive.ps1` serially with each existing
`tools/session-scripts/ss1-*.ps1` through `ss7-*.ps1`, with distinct output folders and no concurrent
build. Preserve SS1's fresh and saved-fixture 3,000 ms assertions. Fix any failure from evidence;
then record the same-build results, update STATUS, commit/push and verify the applicable CI.
Only then proceed **G4.0b keyboard control navigation → remaining G4.2–G4.7 → G5 → G6 → Usable-song
certification**. None of those later implementations has started.

The current local app SHA-256 is `AA73848280971F15B187C3AD75C9F243862E8DC95636288ED06093550CA8C916`.
[Corrective evidence index](docs/evidence/2026-09-08-g40a-correction.json) binds CI and local logs to
this code; the [earlier drive index](docs/evidence/2026-09-08-g40a-recovery.json) retains all eleven
startup measurements (worst 2,994 ms, only 6 ms margin). Logs/screenshots under `build-ci/g40a-*`,
`build-ci/session-shots/2026-09-08-g40a-*` and critic receipts under `build-ci/ce-code-review/` are
local only. Existing G6 rubric FIX rows below remain open; no full visual/milestone proof is claimed.

## 2026-09-08 — G4.0a corrective checkpoint: macOS Save As portability

**Now:** G4.0a remains open; no G4.0b implementation. Code `3beeda21db172a9864913721d44eae4b9ea2f51e`
was pushed after the local evidence below. [CI 34295697790](https://github.com/DanielKinsner/yes-daw/actions/runs/34295697790)
found three macOS Save As failures in `YesDawUiInputCheck`. This is a product regression, not the
macOS timing exception: `YesDawTimelineGpuCheck` actually **passed** (2.76 s test duration; successful
CTest output does not publish its sustained-frame number). Other finished lanes passed; Windows
is still running at this update.

**Cause and correction:** libc++ `equivalent()` returns `not_supported` when either path is absent,
whereas the new containment guard expected missing-path errors. Every valid new Save As path was
therefore refused on macOS before destination creation. Identity comparison now runs only for two
existing paths; canonical lexical containment, occupied-target refusal and real error handling remain.
The separate critic verified the correction against [LLVM's implementation](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-18.1.8/libcxx/src/filesystem/operations.cpp)
and the three failing tests. Their assertions remain intact, with refusal diagnostics added.
Corrective Release build and **380/380 local tests** passed (194.10 s); completed corrective CE
review found no blocking issues (`build-ci/ce-code-review/g40a-libcpp-review/review.json`).
Corrective exact-code CI and the new real-app batch are pending. No exception is applied.

**Next:** push the reviewed correction for platform verification, complete the required real-app
batch when input access is available, and wait for all jobs on the corrective SHA. This safe CI
verification proceeds while native evidence remains explicitly pending; it does not certify the
checkpoint. The previous shared-desktop batch was released; a new hands-off
window has been requested, with safe build/headless work continuing meanwhile. Keep G4.0b and later
features unstarted until G4.0a is verified. Raw macOS log: `build-ci/g40a-ci-macos-job.log`; corrective
local artifacts use `build-ci/g40a-libcpp-*`.

## 2026-09-08 — G4.0a recovery: local gates green, CI pending

**Now:** implementing the authorized Usable-song milestone, beginning with restored SS-1–SS-3.
G4.0a is not certified; G4.0b and later features have not started. Main was clean and safely
fast-forward checked at `2f48962`; no pre-existing work was changed.

**Confirmed causes and repairs under verification:**
- Empty launch was missing app behavior. Native fresh sessions now get a real backed Untitled
  project; first Save names it, canceled/failed naming preserves the working session, and reopen
  retains its content. Injected test shells keep their existing unloaded default.
- Missed early chords were a verified harness/environment focus failure: Explorer retained
  foreground while the drive assumed the app had it. Exact foreground verification plus a guarded
  app-caption click with temporary app-only elevation restored the normal New/Import path (6/6).
  The original topmost state is restored; no other window or global setting is changed.
- Chooser targeting assumed legacy control IDs and a fuzzy YES DAW title, which cannot identify
  Import WAV Audio. The harness now targets the observed filename-control ancestry, verifies
  native focus/path readback and exact chooser closure, and checks SendInput acceptance.
- Startup baseline SS-1 measured 5,137 ms against the unchanged
  3,000 ms budget. Instrumentation measured about 3,895 ms in audio initialization, versus about
  143 ms in enumeration. A guarded Windows shared-audio path avoids JUCE's duplicate endpoint
  capability construction only after proving its exact default pair shares a supported mix rate;
  missing proof or failed opening retains the existing full search/output-only fallback.
  The first integrated SS-1 reached 3,433 ms (still FAIL). Finer instrumentation then measured
  1,331 ms creating the native window: native-titlebar/resizability changes recreated its peer.
  Configuring styles before one peer creation reduced that stage to 490 ms and first probe to
  2,643 ms (bounded diagnostic PASS). The full SS-1 subsequently passed 42 assertions but its
  unasserted saved-fixture reopen measured 3,042 ms: still over B6. SS-1 now also asserts that
  reopen budget. Removing redundant native device scans reduced final fresh/reopen results to
  2,550 / 2,947 ms. SS-2 and SS-3 reopen measured 2,994 / 2,945 ms. All are below the unchanged
  3,000 ms limit, with a narrow worst-case 6 ms margin on this machine; no broad timing guarantee.
  Input/output chooser lists now share one backend scan and refresh after failed audio opening too.
- Critic review found a touched Save As data-loss risk: recursive destination deletion accepted
  occupied/related paths. The repair refuses them before mutation, preserves dirty state, and
  attempts to restore the original DB if the copied destination cannot reopen.

**Evidence so far:** original SS-1 30 passed/12 failed; SS-2 2/3; SS-3 1/2. Empty-startup red was
observed before implementation; first lifecycle-focused green was 160 assertions in 6 cases.
Both new Save As safety cases failed before their repair. Harness headless negative controls pass.
Integrated SS-1 passed 41/42 (only B6 failed); SS-2 passed 31/31 and SS-3 passed 53/53.
Full Release build passed. First full CTest passed 379/380; its sole failing native-startup test
expected no project, contrary to G4.0a. It now asserts the real single empty track; a separate
injected-shell test retains the old unloaded assertions. The corrected UI gate passed.
Critic-requested tests exercise optimized audio failure/default/output-only recovery and a real
SQLite reopen failure after Save As copy, proving the original still accepts edits and Save.
After the final enumeration fix, all seven real-app scripts
passed: SS1 **43**, SS2 **31**, SS3 **53**, instrument **18**, piano roll **69**, beat **47**, mixer
**39** (300 assertions total). These include real New/Import, naming, playback/editing, unchanged
database bytes after close/reopen and byte-identical MIDI-song render after reopen.
Final full CTest passed **380/380** in 199.88 seconds. Exact-code CI remains pending.
The first integrated audio-helper compile exposed a Windows `small` macro collision; native SDK
headers were moved into a separate implementation file. No failed result is covered by the macOS
exception, and no timing threshold or existing assertion has been relaxed.

**Next:** commit/push and wait for every expected exact-code CI job.
Only then advance to G4.0b. The user authorized one hands-off window for this test batch.
The pre-existing density test proves seven complete lanes at 1080p, below the plan's eight;
G6 layout certification must resolve this rather than claiming small-fixture screenshots prove it.
**Rubric:** rows 1–4 remain FIX for existing truncated Scale/fade/master labels, unlabeled gain
slider, font tokens below 11 px and missing eight-track proof (G6.1/G6.2/G6.3; master G4.7).
Rows 5–7 PASS within the observed fixture states: distinct selection/playhead/loop/hover, reference
shell structure, and actual project data. No full visual or Usable-song certification is claimed.
Separate critic reviews found no blocking source findings after fixes; requested recovery tests
passed in the UI gate. External Claude review was unavailable (missing jq), so only the completed
local adversarial review receives credit. Review receipts are local under
`build-ci/ce-code-review/g40a-20260908/`.
All current logs/screenshots are local in `build-ci/g40a-*` and
`build-ci/session-shots/2026-09-08-g40a-*`; generators/scripts are checked in, artifacts are not yet
portable certification evidence.
The checked-in [measurement index](docs/evidence/2026-09-08-g40a-recovery.json) records the final
app/fixture/log hashes and all eleven measured startup times. It does not imply those local logs
or screenshots are available on another machine.

## 2026-09-08 — Plan revised for autonomous delivery

**Now:** Dan accepted the targeted plan revision and asked to keep human involvement minimal.
[ADR-0049](docs/adr/0049-autonomous-delivery-and-usable-song-milestone.md) records that decision.
This checkpoint changes documentation only; no new app behavior, hardware proof or workflow PASS
is claimed. Document consistency/review and docs-only CI are the checks for this revision.

**Revision checks:** separate agent reviews completed. Fixed the Enter activation/Return-to-zero
priority conflict and labeled the recording alignment/result-emission harness as future work.
The local document check passes: 14 documentation files, 90 local links, LF/UTF-8, byte-identical
AGENTS/CLAUDE, preserved existing ADR text and unchanged hardware result history. The pushed
revision's CI uses the documentation classifier; it runs no new app or hardware tests.

**Current implementation milestone:** **Usable-song** — a packaged create/import → edit/MIDI →
built-in mix → save/reopen → export/recovery path, with automated journeys, feel/rubric evidence
and the required real hardware-playback result. It is not alpha. Execution order is G4.0a/G4.0b →
G4.2–G4.7 → G5 → G6 → Usable-song close; separate G4.8/H18 plugins then G7 recording and G8 alpha.
The full DAW scope and existing engine contracts are retained.

**Next, in small commits:**
1. **G4.0a — Restore SS-1–SS-3.** Diagnose New/Import, empty launch and startup failures from the
   actual app/harness; preserve assertions and budgets. Unknown cause stays blocking. Restore
   these earlier journeys before more FX faces; earlier "keep this parked" instructions are superseded.
2. **G4.0b — Prove keyboard control navigation.** The Command router owns a Control target separate
   from Focus context. Keyboard operation and global transport must coexist, with one dispatch per key.
3. Resume G4.2 cp2 compressor meter, remaining faces, slot behavior and presets, then G4.3–G4.7.

**Operating rule:** on a build/continue mandate, agents review, repair, test, commit/push and advance
within the current named milestone without routine checkpoint approval. Stop at its finish line
unless a broader mandate exists. Three failed corrective attempts trigger a separate agent critic,
not a request for Dan to debug. Product regressions and required harness repairs take priority;
unrelated scope remains parked. The exact macOS GPU-only timing exception remains narrowly recorded
in the active plan; it does not excuse the missing SS-1–SS-3 proof.

**Human dependencies only:** a consequential decision outside accepted contracts, inaccessible
equipment/credentials/input surface, or actions outside authorization such as spending/publication
or destructive user-data changes. Agents judge the visual rubric and run available self-asserting
hardware commands; only scripts generate result rows. First verify a separate input session for
unattended app drives; otherwise use an already-authorized hands-off window on the shared desktop.
Older per-launch permission and owner-only operator wording yields to ADR-0049; this is not a
blanket permission to take over Dan's mouse. No isolated-input setup is claimed present yet.

**Handoff boundary:** this request authorizes revising and syncing the plan, not starting feature
implementation, a scheduled automation or a new goal. The code remains `93eea07` (with its recorded
CI result below); later docs-only green does not re-certify that code. New future scripts are
explicitly marked planned in the session README; the old measurement rows remain unchanged.

## 2026-09-08 — G4.2 cp1: EQ response display

**Now:** G4.2 cp1 implemented, locally verified and pushed as `93eea07`. Exact-head CI is complete:
nine jobs green; macOS red only on the parked GPU timing failure. **Next:** G4.2 cp2 — compressor
gain-reduction meter, then the remaining per-kind faces and presets. Stop here; G4.2 and the G4
phase are not closed.

**Remote evidence.** Code SHA `93eea074666ce8740cf98efc64f326e4be9e6d3c`,
[CI run 34282374416](https://github.com/DanielKinsner/yes-daw/actions/runs/34282374416).
Windows, Linux, both sanitizer jobs, both packaging jobs, both verifier self-tests and classification passed.
macOS passed 377 / 378 tests; only `YesDawTimelineGpuCheck` failed at
`tests/timeline_gpu_tests.cpp:84` (19.4233 ms sustained vs 16.6 ms). This is the parked frame-budget
exception under Dan's 2026-09-04 instruction; no rerun or threshold change. Overall GitHub conclusion
is `failure`, not all-green. This docs-only evidence update does not replace that exact-code run.

**Story / precedent.** Opening an EQ shows its six bands' combined configured response above the
existing editable controls. Named Band 1–6 Type / Frequency / Gain / Q readouts and Bands 1–2 /
3–4 / 5–6 pages replace raw ParamSpec names. The precedent is Logic Pro's Channel EQ display
([Apple's guide](https://support.apple.com/guide/logicpro/channel-eq-overview-lgcef1edce5b/mac)).
This is a response display; the parameter controls remain the editing surface (no spectrum-analyzer claim).

**Implementation.** `EqNode::magnitudeAtFrequency()` evaluates the actual current TPT coefficients;
processing and RT annotations are untouched. `EqResponseComponent` owns a separate message-thread
EQ copy, prepared at the project rate and refreshed on settings changes. It caches a log-frequency
path bounded by visible columns, 20 Hz to min(20 kHz, 0.49 x rate), with a +/-24 dB view. Bypass
shows the flat effective response; unbypass restores the configured curve. No live audio-node state
is read. The EQ frame is 660 x 460 max and can float over the mixer dock to keep all eight rows
reachable at 720p. Splitters stay behind it without raising it over Keymap / Undo History. The probe
exports the parameter widgets and `fxEditor.eqResponseVisible` / `eqResponseDb1000` for real gestures.

**Mechanical evidence.** Full final Release build and `ctest --preset ci`: **379 / 379**, 169.21 s.
`[fx-editors]`: **435 assertions / 2 cases** (edits, undo, bypass, slot resets, non-EQ hiding,
all 24 labeled controls at 720p/1080p/1440p, probe targets, overlay order). `YesDawEqCheck`:
**17,600 assertions / 11 cases**; the three added display cases contribute 382 assertions comparing
all six shapes and a combined normalized chain to FFT/settled-sine renders at 44.1/48/96 kHz,
including DC/Nyquist and invalid-frequency bounds. Red first: missing response API; absent graph;
missing probe slider; clipped 720p rows; Keymap behind EQ. Every corresponding final gate is green.

**See-it.** Dan authorized the hands-off batch this session. Final `ss7-mix-the-song.ps1`:
**38 / 38** on the rebuilt real exe, including a gain drag, bypass/unbypass and the three window
sizes. `ss5-piano-roll`: **68 / 68**; `ss6-write-a-beat`: **46 / 46**; `ss4-track-instrument`:
**17 / 17** on its spaced retry (first attempt missed the lane-menu dispatch, 16 / 17).

**Earlier-drive limits — do not call the whole batch green.** Unchanged ss1/ss2/ss3 still failed
in native New/Import setup, before exercising this EQ. First pass: ss1 21 passed/17 failed,
ss2 0/5, ss3 1/2. One spaced retry: ss1 30/12, ss2 1/4, ss3 0/3. ss1 also retains D3 (no empty
project at launch) and exceeded its 3 s startup budget (4.831 s initially; 7.729 s on the retry
while rebuilding). No assertion or threshold was weakened. The New/Import behavior matches the
already parked native-chooser reliability family; these failures are recorded, not fixed or
claimed to be baseline-proven. The current checkpoint's local suite + ss7 are green; this is not
full-arc session-drive certification. Keep the drive tooling issue parked per the phase rules.

**Visual rubric.** Inspected the actual final EQ at logical 1280x720, 1920x1080 and 2560x1440
against the arrangement reference and the existing tokens. PASS for EQ containment, no clipped
rows/readouts, named controls/tooltips, readable 11.5 px graph/readout text and theme contrast,
visible selected strip / bypass state, the existing shell structure, and honest calculated data.
The three-track ss7 fixture with an intentionally enlarged mixer does not re-certify the global
eight-track density requirement. The only display fixes needed were the row fit, numbered labels
and axis type size; the layout/label regressions are pinned in `[fx-editors]`.

**Review / handoff.** Compound simplification and six-lens code review complete; the overlay-order
finding was fixed and tested. Receipt `20260908-141407-9409b0ef`: no remaining actionable findings.
The attempted Claude peer did not run (missing jq); the local adversarial pass completed. Nonblocking
coverage limit: no UI assertion yet varies project rate, although the engine response is measured
at three rates and the shell passes the project rate explicitly. Per-checkpoint screenshots are local
under `build-ci/session-shots/2026-09-08-g42-certified/ss7/`; earlier drive shots/logs and the review
receipt are local too, not synced by git. Re-run the checked-in script to recreate them. HEADLESS
remains the default for a new session; request a hands-off window before drives.

## 2026-09-01 the Real-DAW arc (G0–G8) — plan written, waiting for Dan's "go"

**What happened.** Dan used the app for real on 2026-09-01 and reported: laggy, things not working
or hard to understand, Space-to-play "worked half the time depending on focus", no visible cut
tools or familiar hotkeys, "obscure details instead of obvious things", UI "just not there". He
asked for a turnaround plan: read what's there, know what a DAW is, make them meet — editing and
MIDI first, recording later, rules that keep agents from wandering without him.

**Reproduced mechanically the same evening** (real exe, injected Win32 input, screenshots): Space
after clicking the timeline did nothing twice; Space after clicking a toolbar button started the
transport. Causes in code: Space is Play-only (Stop is `K`), no widget declines keyboard focus,
every action removes and re-adds the audio callback, whole-window 30 Hz repaint, every non-scalar
edit rebuilds the engine, invented chords, menus without shortcuts, zero context menus. Two shipped
lies found on the way: the Time Stretch action is a trim (node exists, unwired) and the RT clip fade
is linear while the UI law is equal-power.

**Decided and written (this session):**
- [ADR-0046 — the feel-first shell arc](docs/adr/0046-feel-first-shell-arc.md): reference-DAW
  rule, no invented chords, focus contexts, command router, everything reachable by mouse, nothing
  blind, feel budgets as gates, selection model, density from the reference, **Session drive** as a
  new mechanical gate class on the real exe, agent visual judgment at every UI checkpoint, editing
  and MIDI before recording, anti-wander (no audit carves; parking lot).
- [The Real-DAW plan (G0–G8)](docs/plans/2026-09-01-real-daw-ground-up-plan.md): verdict, laws,
  target wireframes, keymap v2 decision table (Logic first, Pro Tools second), the code moves
  (carve `MainComponent`, layered rendering, three engine edit lanes), phases with per-item
  story/precedent/gate/see-it, Session scripts SS-1…SS-7, feel budgets B1–B6, the visual rubric,
  process rules, where every R-item went, risks.
- `CONTEXT.md` gained the arc's vocabulary; `docs/goals/parking-lot.md` opened; the roadmap points
  at the plan for everything shell-side.

**The 2026-08-25 reality-run backlog is closed as a list.** R1–R17 are certified (below); R18–R34
are mapped into phases by the plan §9. Do not work R-items from that document any more.

**Now:** **G3.7 — MIDI file import / export** (plan §6 G3; this commit). `src/interchange/Smf.h` reads and
writes Standard MIDI Files (formats 0 / 1, running status, the tempo / meter / name metas) and bridges
them to the edit model in quarter notes; File > Import MIDI File and a `.mid` dropped on a lane land the
file's tracks as SampleLocked MIDI clips at the project's tempo (the first on the target lane, the rest on
new tracks named from the file with the synth on); File > Export MIDI File writes the selection (else every
MIDI clip) as a format-1 file at 960 PPQ. Gates `[smf]` (`YesDawSmfCheck`: byte golden, round-trip, the
tolerant reader, the bridge) and `[midi-file]` in `YesDawUiInputCheck`; ss5 Step 15 authored (the real
chooser by path). ss5 63 / 63 (twice), rubric PASS with one FIX applied; the G3.7 docs-evidence commit
waits for CI on `6c4473e`. **G3.8 — MIDI FX reachable + Arpeggiator + Chord: cp1 (this commit) — the engine
half**: `FxKind` gains `MidiTranspose` / `MidiScaleMap` / `MidiArpeggiator` / `MidiChord`; a Track's
MIDI-kind inserts sit on the MIDI path between its merged clips and its instrument in chain order; the
Arpeggiator (rate / order / octaves / gate) and Chord Trigger (three intervals / velocity) nodes; every
MIDI FX addressed by ParamSpec like the audio inserts; Track-only (`MidiFxNeedsTrack`); the bundle takes
kinds 5–8 in the same column. `YesDawMidiFxCheck` `[midi-fx]` (render goldens by equivalence); cp1
`317211b`. **cp2 (this commit) — the shell**: the strip's Add FX chooser and the slot menu list the four
MIDI FX (a Bus refuses by name on the status line); the slot names them; the param rows render their
choice specs as real choosers (E15's law, unchanged); the roll header's Key / Scale choosers set the
project's key / scale (schema v30, one row), the in-scale rows lift off the grid and the pencil lands on
the nearest in-scale key; the live lane posts a MIDI FX param to the running node through the new
`MidiEffectNode` contract. `[midi-fx-shell]`; ss5 68 / 68; cp2 `3e56c7d` (its push carried the G3.7
evidence commit on top, so CI classified the head docs-only and ran nothing on cp2's code — the G3.9
cp1 head below is the first full run over it; a finding for the parking lot). **G3.9 — Sampler
instrument: ADR-0048 (Proposed) + cp1 (this commit) — the engine half**: `TrackInstrumentKind::Sampler`;
pads as Track rows referencing Project Assets (`SamplerPad`, schema v31 `sampler_pads` with foreign
keys); `SamplerNode` (one-shot + pitched, sixteen voices, ADSR + gain as ParamSpecs); the projection's
`assetSamplesProvider` seam hands a pad its Asset's samples through the G0.5 ownership law; the pad
verbs (set / clear, undoable as Track row edits). `YesDawSamplerCheck` `[sampler]` (render goldens by
equivalence); cp1 `0fe4fad`. **cp2 (this commit) — the shell**: Sampler in both kind choosers; the
instrument panel's pad grid (two rows of eight, C2–D#3) — a click loads a WAV through the chooser seam,
a WAV dropped on a cell loads it, Shift+click toggles one-shot / pitched, Ctrl+click clears, every pad
edit one undo step; the drum-mode roll names a pad's key; `[sampler-shell]`; ss4 15 / 15; cp2 `2998374`.
**G3.10 — RT-safe MIDI input and thru (this commit; the last G3 item)**: `MidiInputQueue` — the lock-free
device→engine lane the model owns and every live engine drains at block top (no message-thread hop),
its thru target the selected Track's Instrument (an atomic the control thread keeps current); the
shell's MIDI callback posts straight into it; the header's MIDI lamp; `YesDawMidiInputCheck`
`[midi-input]` (the one-block latency law, the target law, the SPSC stress; on the RTSan leg) and
`[midi-input]` in the shell; ss4 17 / 17; G3.10 `25fc379`. **G3 close-out (this commit):** the exit
drive **SS-4 "Write a beat and a chord progression"** (`tools/session-scripts/ss6-write-a-beat.ps1`)
is **46 / 46** on the real exe; the montage is `docs/evidence/2026-09-05-g3.png`; the parking-lot
promotion is decided (one item into G4.1); the G3.7 export law is amended (found by SS-4: the roll's
clip no longer stands in for "everything" — selection-else-all, Logic's law). **Next:** the G3.10
evidence commit once CI is green on `25fc379`, then **G4 — The mixer and routing**, starting at
**G4.1 — Mixer dock v2** (plan §6; the exit is SS-5 "Mix the song"). Dan's word (2026-09-05): run G3
to its end and stop when confident — G3 is closed here (ADR-0048 Accepted `b28b2a9`). **G4 — The mixer
and routing (Dan's go 2026-09-05: "pickup at g4"). G4.1 — Mixer dock v2, cp1 (this commit): the strip's
anatomy** — an INPUT slot and an OUTPUT slot on the strip (Logic's I/O rows: input above the inserts, output
below the sends; a click opens the choice), the R cell beside S / M on Track strips (record arm, lit while
armed), narrow / wide strips (View > Narrow Strips; the strip menu; persisted with the view state), the
strip menu per strip kind (Track / Bus / master — three lists, the Add Insert / Add Send / Output / Input
submenus real), the promoted parking-lot item (a Bus or the master is offered the five audio FX only —
`fxKindsForStrip`), and the tools lane's dead readout rows deleted (plan §8.2 "delete before you add").
`[mixer-v2]` in `YesDawUiInputCheck`; ss7 21 / 21; cp1 `36bb3f5` + the Clang / GCC fix `7547e41` — run
`33984907368` green on nine jobs (macOS red = the parked GPU frame-budget flake; the `36bb3f5` run also
hit that flake on WINDOWS and an sccache outage on Alpha-verify Linux — both noise, both parking-lot).
G4.1 cp1 ✅, **cp2 ✅ (the tools lane folded into the strip: empty-slot / send-well clicks are the add
menus, a slot's double-click the FX editor, the send row's menu, Shift-fine + Touch rides on the painted
drags, the lane column and 79 widgets deleted; `[mixer-v2-fold]`; ss7 30 / 30, ss6 46 / 46; cp2 `51184b9` + the Clang / GCC fix `7b126af`, run
`34002327473` green on nine jobs; `tools/session-scripts/README.md` opened).** **Plan §5.1 carve cp1 ✅**
(the ten helper classes out of `MainComponent.cpp` into their own headers, 18 000 → 13 305 lines;
`1e80e52`, run `34004677311`) **and cp2 ✅** (the shell class declared in `MainComponentShell.h`, its
304 member bodies in seven `MainComponent*.cpp` by domain, none over 2 600 lines; `[shell-topology]`
re-pinned; behaviour unchanged; `53fe6d3`, run `34005526151`). The handover blocker is cleared: no shell file is over 2 600 lines
and the declaration is the map. **Next:** G4.2 (FX editors: per-kind faces in the cp2 frame, bypass /
reorder / remove exist, presets). Rules in force: HEADLESS by default (drives on Dan's go — given
2026-09-05 for this PC), the macOS GPU frame-budget red is noise, the local suite + drives are the working
gate; drive scripts launched back-to-back flake at Step 0 (pause between scripts).
G3.2 ✅ — piano roll dock v2: cp1 (`bdf0c36` + `e770d23`), cp2 (`c5be1f8`, audition via the live
note lane), the UI checkpoint (`71efc63`, `f20be6a`, `fabb781`); the head run `33672370057` is green on
all ten jobs (the intermediate run `33668863266` failed only on the macOS GPU frame-budget flake,
twice — parking lot; never weakened). The G3.2 drive is 31 / 31 on the real exe; the §7.4 rubric is
below.
**G3.2 follow-up (2026-09-04, found by Dan's hand-test on the office PC):** the timeline tool strip was
paint-only — it always lit the Pointer and no click reached it (the keys 1–6 worked; every drive picked
tools by key, so the buttons were never exercised). Fixed as one checkpoint: the strip is seven cells in
key order (1 Pointer · 2 Pencil · 3 Scissors · 4 Eraser · 5 Velocity · 6 Zoom) plus the mouse-only Hand;
a click dispatches the same select action the key does; the active tool's cell is lit; the hover hint
names the tool; the probe publishes `tool.<name>` cells. Pins: `[tool-strip]` in `YesDawUiInputCheck`
(a click on each cell selects that tool; the paint lights the active cell) and ss5 Step 10 (drive 38/38).
**The same-family sweep (2026-09-04, Dan's "a bunch of wins like that"):** an agent swept for painted-not-hit-tested
controls, key-only verbs and blind hover zones; the top claims were verified by reading the missing handlers.
Shipped, one commit each, every one with a `YesDawUiInputCheck` pin: the rail's **O record-arm badge** clicks
(`973a227`); the **header gear** toggles the settings row and lights (`bd234f5`); the painted **"SNAP: Bar"**
mockup field is gone (`4290d24`); **Transport ▸ Locate Points** submenu reaches the ten store / recall verbs
(`606b0cd`); a **mixer track strip double-click renames** inline (`c3ae41c`); the **piano roll keyboard column**
names itself on hover (`15acdfb`). Structural: the probe now names `rail.row.N.{mute,solo,arm}`,
`header.gear`, `mixer.strip.N.{solo,mute,fader,insert.K,send.K}`, and ss2 Step 8 clicks them on the real exe
(ss2 29/29). Also `7b25ebd`: the H12 shell test builds against a temp session dir (Dan's hand-made
Untitled.yesdaw had turned it red locally). **Sweep closed (2026-09-04, later):** the mixer's painted fader THUMB and pan knob on an unselected strip
drag that strip (`b55c656`, `[strip-fader]`); a tempo / meter change label click locates exactly on the
change so the ruler menu's verbs act on it (`[map-label]`); the ruler time-format switch was ALREADY in
the ruler menu ("Time Display" submenu, G2.2) — the sweep's claim was wrong, verified, no change. Still
decorative by design until G4.4: the mixer SC badge. **Follow-up to reproduce under a debugger:** an
earlier draft of the strip-fader fix hit-tested the whole rail; a plain strip click on a BUS strip then
segfaulted inside the shipped bus-strip test (`bus strips select and edit like real strips`). The thumb
target sidesteps that path and a bus thumb drag is pinned green, but the rail-click crash itself is
unexplained — parked, not fixed. Every macOS red in this batch was the GPU frame-budget flake (rerun). **Dan's call (2026-09-04): stop spending
time on that flake** — it is parked; treat a macOS red on `YesDawTimelineGpuCheck` alone as noise, no reruns,
the local suite + drives are the working gate until the runner floor is measured.
**Waveform lie fixed (2026-09-04, Dan's screenshot: identical waveforms after a split).** The canvas `Clip` had
no source window; `drawClipCachedWaveform` set its sample rate as `sourceFrames / clip.lengthSeconds` — the
WHOLE file squeezed into every clip, so every split, trim, slip and stretch painted the same waveform since
G2. Now `Clip` carries `sourceStartFrame` / `sourceFrameCount` (engine `srcOffset` / `srcLen`; 0 count =
whole source keeps every golden byte-identical), the painter maps clip time into that window (stretch and
reverse honoured inside it). Pins: `[source-window]` in `YesDawWaveformCacheCheck` (silent-half / loud-half
source) and `YesDawUiInputCheck` (a real split's halves carry adjacent windows that add up). Related, still
open: the 64× zoom cap (`timelineZoomMax`) is ~1–3 ms per pixel, no sample-level view — not in the plan;
and split / trim snap to the grid by default (Ctrl inverts, Snap Off exists) — by design, matches the
reference DAWs.
**Workflow rule (Dan, 2026-09-04): the agent works HEADLESS by default on Dan's PC.** The build, ctest and the
in-memory UI harness need no screen. The Session drives inject real input into the real window and fought
Dan's mouse and focus twice today — so **no drive, no app launch, no screenshot without asking Dan first**
and getting a yes ("run drives", "go ahead"). Batch the drives into one hands-off window per checkpoint.
Per-machine setup the drives need (not in git, not in the agent's memory on another PC): a `build-ci`
Release build via a vcvars64 wrapper; the song fixture once —
`build-ci\YesDawMakeSongFixture.exe --out "%LOCALAPPDATA%\YES DAW\fixtures"` (without it the 0.09 s sine
fixture makes ss3's split-while-playing fail every time); the app closed before a drive or a relink
(single-instance; LNK1104). The drive harness is Windows-only: the MacBook runs the headless suite only.
**G2.19 — zoom to sample level (Dan's call, 2026-09-04; "64× is hella restrictive"). Now.** The cap
`timelineZoomMax = 64` (× fit) is ~1–3 ms per pixel. Decision (recommended, Dan silent = accepted; say
"millisecond floor" to change): the ceiling is ONE SAMPLE PER PIXEL, with a floor on the visible window
(never less than the width in samples); past the peak cache's finest tier (256 frames / peak) the painter
reads decoded audio directly for the visible window only. Gates: the zoom slider / Ctrl+wheel reach the
ceiling and clamp there; a headless paint at the ceiling shows the actual wave shape (a synthetic ramp
paints monotonic columns); the frame-budget gate stays green (paint cost is bounded by the window width,
never the file); saved zooms above the old cap round-trip. **Done (2026-09-04):** `timelineZoomCeiling()` =
max(64, rate / fit px-per-second) replaces every `timelineZoomMax` clamp (wheel, tool, slider range, fit-loop,
zoom-to-selection, restore); the canvas `Clip` state carries a `waveformSampleLookup` (the decoded asset the
playback reads) and below tier 0 (256 frames / px) `drawClipCachedWaveform` projects columns from the samples
(`computeWaveformColumnsFromSamples`, one whole pixel per single-sample column); `TimelineCanvasPaintStats
.sampleWaveformClips` counts it. Pins: `[sample-zoom]` in `YesDawWaveformCacheCheck` (a ramp at one sample
per pixel: column n is sample n, the painted tops climb monotonically, the cache path differs) and in
`YesDawUiInputCheck` (Ctrl+wheel climbs past 64x and clamps at the ceiling; ceiling x fit px/s == the
project rate; Ctrl+0 returns to 1x; a 0.09 s project honestly stays at the 64 floor). Frame-budget gate
untouched (no sample lookup in the headless checks).
**Done:** G3.10 ✅ — RT-safe MIDI input and thru: `25fc379`; run `33952079484` green on nine jobs (the RTSan
leg included), macOS red only on the parked GPU frame-budget flake (`YesDawTimelineGpuCheck` alone). ss4
17 / 17; rubric PASS. **G3 closed** on SS-4 46 / 46 (`ss6-write-a-beat.ps1`), the montage
`docs/evidence/2026-09-05-g3.png`, the rubric and the promotion decision (the G3 close-out section).
G3.9 ✅ — Sampler instrument (ADR-0048 Accepted, Dan 2026-09-05): cp1 `0fe4fad` (run `33950273738` green on nine
jobs, macOS red only on the parked GPU frame-budget flake — `YesDawTimelineGpuCheck` alone), cp2 `2998374`;
certified by exact-head run `33951039513` (green on all ten jobs). ss4 15 / 15 (then 17 / 17 with G3.10's
step) on the real exe; rubric PASS.
G3.8 ✅ — MIDI FX reachable + Arpeggiator + Chord: cp1 `317211b` (run `33948586824` green on all ten jobs),
cp2 `3e56c7d` (its own push ran the docs-only fast path — the parking-lot CI finding — so cp2's code is
certified by the next code heads' full runs: `0fe4fad` on nine jobs and `2998374` on all ten). ss5 68 / 68;
rubric PASS.
G3.7 ✅ — MIDI file import / export: `6c4473e`; certified by exact-head run `33947932021` (green
on all ten jobs). ss5 63 / 63 on the real exe (twice) with the G3.7 Step 15; rubric PASS with one FIX
applied (the audio-only inspector controls under a MIDI clip's rows).
G3.6 ✅ — Step input and musical typing: `234bfcd`; certified by exact-head run `33946774937`
(green on all ten jobs). ss5 59 / 59 on the real exe with the G3.6 Step 14 (the whole roll script, Steps
1–14, in one launch); rubric PASS.
G3.5 ✅ — MIDI clips at arrange level: cp1 `a58391a` (the engine half; run `33944717371` green), cp2
`2f9571b` (the arrangement's split / heal / mute / copy / paste / repeat, the four inspector rows; its
run `33945749306` was red only on the Clang exhaustive-switch class, closed in `234bfcd`, whose run
`33946774937` is green on all ten jobs — the certifying run for cp2's content). ss5 51 / 51 with Step 13.
G3.4 ✅ — Quantize v2: cp1 `d139cb3` (the engine half), cp2 `603faa0` (the quantize panel);
head run `33944162506` green on nine jobs, macOS red only on the parked GPU frame-budget flake
(`YesDawTimelineGpuCheck` alone, 21.7 ms vs 16.6 — Dan's 2026-09-04 rule: noise, no rerun). ss5 48 / 48
on the real exe with the G3.4 Step 12; rubric PASS with the one FIX applied.
G3.3 ✅ — MIDI CC, pitch bend, aftertouch, program change: cp1 `17e3802` (the engine half), cp2
`b0a1c2f` (the control lane), the switch fix `63b06ab`, the drive-window fixes `e9d2c38`; certified by
exact-head run `33942955586` (green on all ten jobs) for `e9d2c38`. Earlier heads were red only on the
parked macOS GPU frame-budget flake (`33939222022`), the GCC / Clang switch warning (`33940220844`) and
a Clang dangling-reference warning in the gate (`33941926652`), each fixed in the next commit. Drives on
the real exe: ss5 43 / 43 (with the G3.3 Step 11), ss1 41 / 42 (D3), ss2 29 / 29, ss3 51 / 51, ss4 11 / 11.
Rubric PASS on every line (the long lane hint truncates in the status line — noted).
G0.1 ✅ — certified: exact-head GitHub Actions run `33587446396` green on all ten jobs for
full SHA `a6a5cf8807874347ada80b8919190cac37a3022c` (first try). Local suite 363/363.
G0.2 ✅ — certified by exact-head run `33589636898` (green on all ten jobs) for full SHA
`ea4dfea9351773eaca732cc79f3cb2996ef4f5a1`; its own commit's run `33588671718` was red on macOS
only (compile hide, D11), green everywhere else.
G0.3 ✅ — certified: exact-head run `33589636898` green on all ten jobs for full SHA
`ea4dfea9351773eaca732cc79f3cb2996ef4f5a1` (first try). Local suite 363/363.
G0.4 ✅ — certified: exact-head run `33592155901` green on all ten jobs for full SHA
`f608d3ddfc6e1b09a35bc9eb7eb19e3c52dafad8` (first try). Local suite 363/363.
G0.5 ✅ — certified by the fix commit's exact-head run `33596503075` (green on all ten jobs, full SHA `23d3e5382fc868c98abe092f59efffbe483f723b`): its own
run `33595208144` was red on Linux/macOS (D22); code `96e9829` + fix `23d3e53`.
G0.6 ✅ — certified by the same fix-commit run; its own run `33595972964` carried the D22 red;
code `24095d8` + fix `23d3e53`. Local suite 364/364 with the fix.
G0.7 ✅ — three checkpoints (`8795c42` header + row law, `8817813` panels + rail row, `4e9932a`
ruler rows); certified by exact-head run `33604946633` (green on all ten jobs) for full SHA
`171363ae4143c29bd6519069c0a4067a5e98976c` (the G1.1 cp1 commit, which carries every G0.7/G0.8 change). Its own
checkpoint runs were red on macOS only (D30 clang capture warning; D31 the inspector
exact-restore compare — did not recur; D32 the lane-budget gate's vacuous Windows pass).
SS-1 see-it steps stay pending the drive (D14).
G0.8 ✅ — `f117c42`; certified by the same run `33604946633` / `171363ae4143c29bd6519069c0a4067a5e98976c`. Its own run
`33604218038` was red on macOS only in the GPU frame-budget gate (18.1 ms sustained vs 16.7 —
runner noise: the same paint passed the run before and the run after; the failed job was rerun).
G1.1 ✅ — keymap v2 + focus contexts (`171363a` infrastructure, `708daed` the §4 remap); certified
by exact-head run `33608447371` (green on all ten jobs) for full SHA
`92a9f5464593995e6c21cf3ea0672a098f5f0c90` (the G1.2 commit, which carries the D41 fix). cp2's own
run was red on Linux/macOS at the compile step (D41).
G1.2 ✅ — `92a9f54`; certified by the same run `33608447371`.
G1.3 ✅ — context menus (`74ca3a2` seven targets, `4aa49b4` the insert slot, `998cd21` the
right-button fix for macOS Ctrl+click); certified by exact-head run `33611170410` (green on all ten
jobs) for full SHA `35814d5b51f1096a19c7f62c498cc0b62f53e555` (the G1.4 cp1 commit). Its own runs: cp1/cp2 red on
macOS only (D44), the fix's run cancelled by the next push (D47).
G1.4 ✅ — toolbar v2 (`35814d5`: nudge value chooser + verbs, the inspector toggle on I, the
two-readout counter); certified by the same run `33611170410`. Deferred inside the item, recorded:
Edit mode / Snap mode choosers (their model is G2), the X / P / A regrouping (G1.7 sweep), nudge
in ms / samples / frames (G2.15). The two defects its rubric shot exposed (D45 menu bar width,
D46 the caption clipped since G0.7) are fixed in `bd1117a`, whose run is watched.
G1.5 ✅ — the keymap editor (`1baa2ed`); certified by exact-head run `33612568972` (green on all
ten jobs, first try).
G1.6 ✅ — status hints + live tooltips (`15ce6e7`); its own run `33614073659` was red on macOS
only (D49); certified by G1.7's exact-head run `33615096877`.
G1.7 ✅ — the dead-affordance sweep (`96c93bd`); certified by exact-head run `33615096877`
(green on all ten jobs, first try). **G1 headless work complete**; SS-2 pending Dan's go (D14).
G2.1 cp1 ✅ — splitters + per-project view state (`98d1a89`); certified by exact-head run
`33616439236` (green on all ten jobs, first try).
G2.1 cp2 + cp3 ✅ — dock tabs (`669d155`), the letter cluster (`3a0ca74`), its paint fixes and
the Linux fix; certified by exact-head run `33625472284` (green on all ten jobs) — G2.1 complete.
G2.2 ✅ — ruler v2 (`9618882`); certified by the same run `33625472284`.
G2.3 ✅ — drag ghosts, landing line, Esc, auto-scroll (`83e8c5e`); G2.4 ✅ — the Smart tool
(`c56e0e9`); certified by run `33627456994` after a rerun of its Windows job (the GPU frame-budget
benchmark's hosted-runner noise, parking lot) — every job green on the same head.
G2.5 ✅ — Time selection first-class (`c6568e5`); G2.6 ✅ — Edit modes (`e25e008`); both certified
by run `33630615703` on `af3c034` (the one-label fix; D51 in G2.7's section) — green on all ten jobs.
G2.7 ✅ — Snap modes (`02ea877`); certified by exact-head run `33632069331` (green on all ten jobs).
G2.8 ✅ — Nudge value in clock units (`463646e`); certified by run `33633084596` after a rerun of its
macOS job (GPU frame-budget noise, parking lot) — every job green on the same head.
G2.9a ✅ — time-stretch engine half, schema v22 (`4e24714`); certified by exact-head run `33634707297`
(green on all ten jobs).
G2.9b ✅ — time-stretch gesture (`15e16e1`); G2.10 ✅ — fades v2, schema v23 (`d0800c6`); certified by
exact-head run `33637897699` on `d0800c6` (green on all ten jobs).
G2.11 ✅ — slip (`080c6b8`); G2.12 ✅ — clip properties, schema v24 (`0c39c05`); certified by run
`33639686386` on `0c39c05` after a rerun of its macOS job (GPU frame-budget noise, parking lot) — every
job green on the same head.
G2.13 ✅ — clip processing, schema v25 (`05472a5`); G2.14 ✅ — markers v2, schema v26 (`dba562f`);
certified by exact-head run `33641728768` on `dba562f` (green on all ten jobs).
G2.15 ✅ — tempo and meter map editing (`8beb692`); G2.16 ✅ — zoom and navigation (`a8f609c`); certified by run `33645105611` on `a8f609c` after a rerun of its macOS job (GPU frame-budget noise, parking lot) — every job green on the same head.
G2.17 ✅ — track headers v2 (`a5b2dbe`); G2.18 ✅ — the undo history window (`6f46a5e`); certified by exact-head run `33649032858` on `2eddd07` (the label fix for GCC/Clang -Wswitch, D51's lesson again — the engine label switch is now covered by the checker) — green on all ten jobs. **G2 headless work complete**; SS-3 pending Dan's go (D14).
G3.2 ✅ — piano roll dock v2: cp1 `bdf0c36`+`e770d23`, cp2 `c5be1f8`, the UI checkpoint `71efc63` /
`f20be6a` / `fabb781` (head run `33672370057` green on all ten jobs); drive ss5 31/31 — 38/38 with the
tool-strip follow-up of 2026-09-04 (see **Now**).
G3.1 ✅ — Track instrument (ADR-0047 Accepted): cp1 `381e8db` (run `33652552912`), cp2 `fa6c67e` (run
`33654084069`), cp3 `4c26a17` (run `33655909259`), the UI checkpoint `dc60b42` (run `33660679423`) —
every run green on all ten jobs; SS-1 41/42 (D3), SS-2 23/23, SS-3 51/51, the G3.1 see-it 11/11 on
the real exe.
**Next:** see **Now** above (the Done list is in order; the plan is the map).


## Older history

Per-checkpoint stories of the Real-DAW arc (G0–G4.1, plan §5.1) and every earlier handoff entry
(H0–H17, the August runs) moved verbatim to [`STATUS-HISTORY.md`](STATUS-HISTORY.md) on 2026-10-05.
