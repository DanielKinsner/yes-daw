# UI correctness pass (rubric FIXes 1-15)

Preparation only, 2026-10-12. [STATUS](../../STATUS.md) is still the live handoff, and the
[active shell plan](2026-09-01-real-daw-ground-up-plan.md) is still the authority. On 2026-10-11 the
visual rubric was run on the packaged writes-on batch (shots in
`build-ci/session-shots/2026-10-11-writes-on/`). It found fifteen defects. All of them predate the
writes-on work: units 4-6 of the [cheap autosave close-out](2026-10-09-cheap-autosave-closeout.md)
changed no drawing code. Plan section 7.4 says "Each FIX becomes a token/layout gate in the same
item so it stays fixed", and STATUS makes these FIXes a gate on that close-out's unit 7 (the
Usable-song milestone close).

One agent investigated each defect and a separate agent challenged it against the code and the
shots. Where the challenge corrected a proposal, this plan uses the corrected version. The plan adds
no scope. It calls for one implementation ADR (ADR-0076, unit 11) under ADR-0049's clause. Unticked
items are pending, not certified.

## Stop rule and frozen list (owner direction, Dan, 2026-10-13, relayed to this session by the "YES DAW progress review" session after its review of the week)

- The pass ends with FIX 29 (29d, 29e). The close-out's unit 7 (the Usable-song milestone close) comes next; no new
  correctness units open before it.
- The fix list is frozen. A new rubric or screenshot finding goes to
  [the parking lot](../goals/parking-lot.md) unless it is broken behaviour, wrong data, or unreadable or missing text.
  Cosmetic polish (pixel nudges, contrast tweaks, label trimming) does not block the milestone.
- Lighter paperwork: a STATUS entry is about 5-10 lines (what changed, gate, CI, next); an ADR only when a change alters
  a contract. The verification rules (gates, mutations, CI) are unchanged.

## Checkpoint rule

Cadence per ADR-0075 (2026-10-12): targeted tests per commit; the full local suite, the critic (skippable for a small
layout or text-only unit, said in STATUS) and the push once per unit; at least one named mutation per gate, more only
where a unit names distinct failure modes.

- [ ] Every unit follows the same steps:
  1. Run the new gate on the code before the fix and record the failing assertion in STATUS.
  2. Fix.
  3. Run local checks and the earlier and current Session drives.
  4. Get the agent rubric judgment of the named shots and a separate critic pass.
  5. Update STATUS, commit, push.
  6. Inspect every job of exact-code CI.
- [ ] A gate counts only if its named mutation turns it red. Expected values come from literals or
  first-principles arithmetic, never from the law under test. Text-fit gates measure each CI
  platform's own font.
- [ ] Drive proof may be batched on one packaged build. A unit's drive evidence counts only for a
  build whose tree contains that unit's fix. Shots go to a new folder. Never overwrite
  `2026-10-11-writes-on`, which is the before-evidence.
- [ ] Drive assertion counts may rise as assertions are added. None may fall or go red.
- Order is worst first: behaviour that changes data, then wrong data in the frame (D3), then cut or
  unreadable text and layout, then tooling. Units are independent except where a dependency is
  stated:
  - Unit 6 needs unit 5.
  - Unit 13 follows unit 3.
  - Units 1 and 6 both edit SS3 Step 17.
  - Units 1 and 14 both edit SS7 Step 16.

## Data-changing behaviour

### 1. A press on the tempo cell never changes the tempo (FIX 1)

The header tempo cell is a JUCE `LinearBar` slider. It still uses JUCE's default of jumping to the
pointer, so a still press writes an undoable tempo edit (120 -> 213 BPM).

- [x] **Fix.** In `src/ui/MainComponent.cpp`, right after
  `headerTempoControl.setSliderStyle (juce::Slider::LinearBar)` (~:810), add
  `headerTempoControl.setSliderSnapsToMousePosition (false);`.
  - Write the ADR-0046 law 1 precedent into the comment and the commit. Logic changes the LCD
    tempo by dragging or by double-clicking to type. Pro Tools' tempo field is click to type or
    drag. Neither changes the value on a single click.
  - A drag now scrubs relative to the tempo in force, at about 1.5 BPM per pixel.
  - Nothing else in `src/` changes.
- [x] **Gate.** `tests/ui_input_tests.cpp`, after the case at :7765, tagged
  `[ui][input][shell][timemap][tempo-press]`.
  - Still presses at x = 2, w/2 and w-3, each plain and with Shift. After each, the slider value,
    the persisted head bpm and `commandDispatchCount` are unchanged.
  - A 20 px drag persists exactly `snapToLegalValue (120 + 20 * 380 / getMouseDragSensitivity())`,
    which is 150.5.
  - **Bites:** today the first press lands near 26 BPM and the drag lands at 324. Deleting the one
    new line turns it red again.
- [ ] **Drive.**
  - SS7 Step 16: record `commandDispatchCount` before the tempo press. After the release, assert
    `project.tempoBpm == $bpm` and that the count is unchanged. Delete the conditional Ctrl+Z and
    its wait, and fix the comment.
  - SS3 Step 17: assert that `commandDispatchCount` rises across the 20 px `DragWithin`. Its current
    readout check compares two playhead positions, so it cannot see the drag.
  - `ss7-pointer-pressed-header-tempo` must match the hover shot: 120.0, 002|02, bars 9/17/25.
    Today it shows 213.0, 003|02, bars 17/33/49.
- Size S. Backlog B30's "no jump-to-pointer" is the Shift fine-drag spec, so it supports only the
  Shift half of this gate.

### 2. Correct B30's false certification: Shift fine-drag on the tempo cell

B30 is ticked and says it covers the header tempo. But `FineDragSlider::supportsFineDrag` and
`axisProportionDelta` (`src/ui/ShellWidgets.h:170-176`) have excluded `LinearBar` since the day B30
landed (44185e2). Broken verification is repaired at once. This is its own commit after unit 1.

- [x] **Fix.** Add `LinearBar` to both functions. Since unit 1 a plain drag on the cell moves 1/250 of the
  range per pixel (`getMouseDragSensitivity()`, snapping off), so for `LinearBar` `axisProportionDelta` must
  divide by that sensitivity, not by `getWidth()` (67 px) - otherwise Shift moves about 0.37x, not 0.1x
  (unit 1's critic). Correct the B30 note in
  `docs/goals/2026-08-10-shortcut-and-workflow-parity-backlog.md` (:194-197) in the same commit.
- [x] **Gate.** Same file, `[tempo-press][fine-drag]`: a 20 px Shift drag on the tempo cell moves the
  tempo by exactly 0.1x the plain amount, snapped to the 0.5 grid and anchored at the tempo in force.
  **Bites:** take `LinearBar` out of `supportsFineDrag` and the drag moves the full amount.
- [ ] **Drive.** No new step; the ctest gate is the proof. The SS3 and SS7 batches stay green.
- Size S.

### 2b. A tempo drag is one undo step (found during unit 1)

`UiAppModel::setProjectTempoBpm` writes an undoable edit and the bundle for every value change, and the
cell's `onValueChange` calls it on each drag move, so a 50 px drag leaves about thirty undo steps
(feel law 6: "Undo everything, one step per gesture").

- [x] **Design first.** Read `engine::ProjectUndoStack` for a merge or group facility. If one exists,
  bracket the cell's drag (`juce::Slider::onDragStart` / `onDragEnd`) as one gesture. If none exists and
  grouping needs a new undo-stack contract, write an implementation ADR before code.
- [x] **Gate.** `tests/ui_input_tests.cpp` `[tempo-press][one-step]`: a drag sent as several moves
  persists its final tempo and leaves exactly one undo step - one Ctrl+Z restores the tempo in force
  before the drag, a second Ctrl+Z restores the step before that. **Bites:** without the bracket, one
  Ctrl+Z only steps back one move.
- [ ] **Drive.** SS3 Step 18's twenty-step undo and redo stays green.
- Size M.

### 3. A fader ride shows and writes the value being ridden (FIX 14, first part)

During a Touch, Latch or Write ride, `onFaderDragged` only records. The strip's thumb, its dB
readout, its accessible value and the Shift anchor all read the saved gain. So the strip shows 0.0
while the transient readout shows -0.1. Keyboard rides start every step from the saved gain and
release at it, so they write the wrong automation.

- [x] **Fix.**
  - Add `paintedFaderDragGain` beside `paintedFaderDragStrip`.
  - Set it only in the `retargetMixerStrip` success branch, with the gain clamped to >= 0. Clear it
    wherever the strip index resets: on release, and in the deleted-strip branch
    (`MainComponentControls.cpp:957`).
  - Add one helper, `paintedStripFaderGain (strip, model)`. Use it for the drawMixer thumb and
    readout, `faderGainForStrip`, `faderRailAtPosition` and the probe's `fader.thumb`.
  - The strip readout paints `dbReadoutText (gain)`. That gives one text law for the strip readout,
    the transient and the accessible value (ADR-0066).
  - Add the harness seam `mainComponentPaintedFaderReadoutText`.
  - Record in STATUS that keyboard rides now build up and release at the held value (ADR-0052:
    the riding control's live value wins).
- [x] **Gate.** `tests/ui_input_tests.cpp`, `[mixer-states][ride]`.
  - **Pointer Touch ride, held:**
    - The transient readout does not read "0.0 dB".
    - Seam text == transient text == the fader's accessible value text.
    - The thumb's pixels are at the pointer, and the unity position is clear.
    - The saved gain is still 1.0.
    - After release, the transient is hidden and the seam reads the saved gain (no stale preview).
  - **Keyboard Touch ride:** three Up presses read "3.0 dB" on both the transient and the seam. On
    close, the lane's last point == `automationNormalizedForFaderGain (10^(3/20))`.
  - **Bites:** make the helper return the model gain. The seam then stays at "0.0 dB", and the
    keyboard leg reads "1.0 dB" and writes unity.
- [ ] **Drive.** SS7 Step 17 `ss7-sweep-drag-{1280x720,1920x1080,2560x1440}`: the strip's own
  readout equals the transient readout.
- Size M.

## Wrong data in the frame (D3)

### 4. The playhead shows no invented number and covers no ruler label (FIX 2 + FIX 8)

Both defects are in `timeline_canvas_detail::drawPlayhead`. Its purple pill:
- prints `round(seconds) + 1` as if it were a bar number (bar 5 reads "10");
- covers the bar label under it;
- is cut by the canvas edge at 0 s;
- draws its text at about 2.8:1 contrast.

The two investigations disagreed: one kept the badge with a corrected number, the other removed it.
**This plan removes it**, for four reasons:
- In the reference image, the playhead is a light head in the marker row, and nothing of the
  playhead touches the bar-number row. Pixel sampling shows the purple "33" is a separate tag on its
  own tick.
- Logic's playhead triangle rides the lower ruler edge.
- The parked note at `docs/goals/parking-lot.md:55-57` recommends the same.
- The header counter already shows bar|beat.

If a number is ever brought back, it must read `headerBarBeat().bar` (the piecewise law) and carry
tempo-change and meter-change gate legs. The head-tempo law is wrong after a change.

- [x] **Fix.**
  - Add `playheadHeadRect (ruler, playheadX)`: a whole-pixel rect, centred on the line, with its
    bottom on the ruler separator, constrained to the ruler.
  - Delete the pill and its `drawText`.
  - The line runs from the head's top to the clip bottom.
  - Fill a white downward triangle built from the integer rect, so no anti-aliasing spills sideways.
  - Delete the 7 badge and text tokens. Add `timelineCanvasPlayheadHeadWidth =
    2 * timelineCanvasRulerMarkerLabelLeftInset - timelineCanvasPlayheadLineWidth` (6) and
    `timelineCanvasPlayheadHeadHeight = 8`.
  - Delete the unused `kPurple` (TimelineCanvas.h:522).
  - Add a `token_contrast_tests` pair: white on `timelineRuler` >= 3.0.
  - Close the parking-lot entry and STATUS items (2) and (8).
- [x] **Gate A.** `tests/ui_input_tests.cpp`, `[ui][timeline][ruler][layout][playhead-head][paint]`.
  - Setup: `paintTimelineCanvas` at 900x300 and 40 px/s, with one marker. Paint one reference with
    no playhead.
  - Positions: every 5 px, plus both ends, plus the marker.
  - At each position, scan every column, rows from the top down to the ruler's bottom. The footprint
    of changed pixels must be non-empty, start no higher than the marker row, stay inside
    `rulerArea`, and, when resting on the marker, miss the marker's label.
  - **Bites:** restore the pill, or start the line at `ruler.getY()`. Today it fails at every
    position, at both ends and at the marker.
- [x] **Gate B.** `tests/ui_screenshot_tests.cpp` G0.7 `[rubric-shots]` (:1931-1934): start the bar
  loop at 1 and drop the "bar 1 is under the playhead" exemption. **Bites:** restore the pill, which
  puts purple at about 3.1:1 in bar 1's above-tick column.
- [ ] **Drive.**
  - SS3 at all three sizes: bar "1" visible, no pill, a small white head inside the canvas.
  - SS2: no "91". SS1: no "10". `ss7-write-ride`: no "4" beside 002|02.
- Size S. Known residual, not a regression: a playhead stopped inside a marker label's span still
  crosses that label, as the line already does today.

### 5. The ruler time row shows each tick's exact time, in a text colour (FIX 3 + FIX 7)

Three commits, each green on its own.

- [x] **Commit 1, refactor with no behaviour change.**
  - `RulerBarLabel` gains `double seconds`. For now it holds today's read-back of the time from the
    rounded pixel.
  - Add `timeline_canvas_detail::rulerTimeLabelText (label, state, vp)`, already in its final
    3-argument signature.
  - `drawRuler` draws through that function.
- [x] **Commit 2, the fix plus its gate.**
  - Fill `seconds` with the exact bar start.
  - `formatRulerSeconds` floors whole units with formatSmpte's `+1e-9` guard and splits integers
    (`"%lld:%02lld"`, and `"%lld:%02lld.%lld"` for tenths).
  - Gate: two TEST_CASEs tagged `[ruler-time-labels]`.
    - **TC1, formatter:** 11 bars at 44 BPM reads "1:00"; 119.96 s reads "1:59.9"; 61.5 s reads
      "1:01.5".
    - **TC2, sweep:**
      - Zoom levels pps = i/100 for i = 100..5500, at scroll 0 and 7.3.
      - At 120 BPM, check min:sec, samples and SMPTE modes; at 44 BPM, check min:sec.
      - Expected values come from integer arithmetic. Require zero mismatches and more than 10,000
        checks, and CAPTURE the first mismatch.
      - Pins: at pps 5.02, bar 9 reads "0:16" and bar 33 reads "1:04".
  - **Bites:** run commit 2's tests on commit 1's code (they compile unchanged):
    - TC1 returns "0:59".
    - TC2 at 120 BPM: about 47% of min:sec labels are wrong and nearly every samples label is wrong.
    - TC2 at 44 BPM: 3,369 labels are wrong.

    Record this red run in STATUS. Accepted change: tenths mode now floors (1.96 s reads 0:01.9).
- [x] **Commit 3, contrast plus a scan gate.**
  - The time-row text uses `kMutedText`, which is 6.19:1 on `timelineRuler`. `rulerTick` is set
    again only for the tick's `fillRect`.
  - Gate in `tests/token_contrast_tests.cpp` `[tokens]`, "text is drawn in text tokens":
    - Guarded tokens: the Decoration, Surface and Indicator roles.
    - Build one global alias map across `src/ui`, and require that each alias maps to a single token.
    - Accept any qualification of `Color::`.
    - From each guarded `setColour`, scan the rest of that line and the following lines, stopping at
      the next `setColour` or a line starting with `}`. Any `drawText`-family call in that span is a
      finding.
    - Require `sitesSeen >= 60` and no findings.
  - **Bites:** today there is exactly one finding: the `rulerTick` time-row text in
    `TimelineCanvas.h`. Narrowing the regex to the `UiTheme::Color::` spelling makes the scan see
    only 31 sites, which fails the floor.
- [ ] **Drive.** SS1 at three sizes: every time label equals (bar - 1) x 2 s at 120 BPM 4/4.
  - 1920: bars 57/65/73/81/89/97 read 1:52/2:08/2:24/2:40/2:56/3:12.
  - 2560: the bars 37-65 and 105-125 runs read correctly.
  - 1280: bar 113 reads 3:44.
  - The brightest time-row pixel is #89949f (today #5c646d).
  - Recommended: export `view.rulerTimeLabels` from the probe through the same functions, and add a
    PASS/FAIL row to SS1 at all three sizes, so the packaged evidence is mechanical.
- Size S.

### 6. Ruler bar labels follow the tempo and meter maps (FIX 4)

Plan G2.15 asked for a "piecewise `computeBarBeat` and ruler", but only the header counter was moved
to the piecewise law. Depends on unit 5: label `seconds` becomes the piecewise bar start.

- [x] **Commit 1, engine.**
  - Extract the meter walk from `computeBarBeatPiecewise`, unchanged, into `detail::walkMeterMap`,
    shared by `barBeatAtTick` (the walk's reading), `barStartTick` and `nominalBarTicks`.
  - Add `barStartTick (meterMap, bar, out)`: the exact inverse, the first tick that reads bar N,
    returning false for bar < 1. Rounding the fractional start up is only the estimate: in doubles
    bar x length lands on the wrong side of a whole tick (3/39 at bar 66), so the start is checked
    against the walk's own arithmetic and moved by a tick. A bar a change cuts under one tick starts
    at that change.
  - Add `nominalBarTicks`.
  - Fix the stale comment at `Time.h:445-449`.
  - Gate: `tests/time_tests.cpp` `[time][g2][tempo][inverse]`, entirely in ticks.
    - Hand-written starts for a meter map with a 7/8 change mid-bar and a 100-tick partial bar.
    - N = 1..200 round-trips to {N, 1}, and the tick before each start reads bar N-1.
    - A 3/7 case checks the rounding up (a 7/7 bar is a whole 61440 ticks, so it rounds nothing).
    - A phantom bar (a 3/7 bar line, then a change 4/7 of a tick later).
    - Every meter the walk accepts, bars 1..400, from tick 0 and after a change.
  - **Bites:** without the check against the walk, the sweep fails 4,568 cases (first 3/39 bar
    66); with floor and no check, the 3/7 case is red.
- [x] **Commit 2, canvas and shell.**
  - `TimelineCanvasState` gains a `tempoMap` pointer and a `meterMap`. A null pointer keeps the V4
    head law.
  - Under the map, `computeRulerBarLabels` places bar N at `frameForTick (barStartTick (N)) / sr`.
  - The label step comes from the nominal full bar of the meter in force, never from the next bar's
    start. A short partial bar would otherwise push the step past the view.
  - `makeTimelineState` keeps the compiled map in a member, `timelineCompiledTempoMap`.
  - Fix the stale comments at `TimelineCanvas.h:233-237` and `:384-387`, and at
    `MainComponentArrange.cpp:1486-1488`.
  - Gate: `tests/ui_input_tests.cpp` `[ui][input][shell][g2][tempo-map][ruler-bars]`.
    - Shell A: tempo jumps 120 -> 240 at bar 17; meter changes 4/4 -> 3/4 at bar 33.
    - Shell B: a tempo ramp.
    - Shell C: a 100-tick partial bar.
    - Expected starts are closed-form. Each label is within one pixel of its start. Bars ascend,
      label spacing holds, some bar is above 60, and the last label is near the right edge.
  - **Bites:** with `state.tempoMap` left null, Shell A's bar 25 sits at 48 s instead of 40 s.
    Stepping from the next bar's start makes Shell C's labels vanish.
  - Added after the critic:
    - A bar cut under one tick (a phantom) is never labelled.
    - The search for the next label starts at the bar under the minimum spacing; without it a map
      whose bars shrink 64-fold stopped at bar 1.
    - Ties at exactly 56 px clear the spacing, as the head law's one step does.
    - Gate: a pure sweep of five maps x six zooms x four scrolls. Each label sits on the counter's
      bar boundary, ascends and clears the spacing, and no stretch is unlabelled. A flat map
      matches the head law at 960 views, including the 150 BPM 7/2 tie.
  - **Bites, added:** no seek leaves one label across 1/64 at 240 (1240 px unlabelled). Phantoms
    labelled read 2 where the counter reads 3. No tie tolerance gives 19 labels where the head law
    gives 23.
- [ ] **Drive.** Written in SS3 Step 17; it runs in the batched drive proof. SS3 Step 17, after its existing assertion:
  1. Set Snap: Off through `widget.timeline.snap_mode.chooser`.
  2. Take the first `ruler.bar.N` to the right of `ruler.map.0`.
  3. Click the time row 1 px right of that tick.
  4. Assert that the counter's bar part is N (`'{0:D3}|*'`).
  5. Restore Snap: Grid.

  Today this fails: label 57 sits at 1:52, where the header reads about bar 81. In the end shots,
  the labels right of "330~" are denser and their times are compressed.
- Size M.

### 7. The master meter fills to the level its scale names (FIX 5)

- [x] **Fix**, master pane paint only.
  - Add `masterMeterFractionForDb`: piecewise-linear in dB between the evenly spaced marks
    {0, -12, -24, -60}. A `static_assert` requires the marks to be strictly descending, with at
    least 2.
  - Add `masterMeterFractionForPeak`, which returns 0 when `!(p > 0)` (this also catches NaN).
  - Add `masterScaleLabelBounds`, with each label centred on its level in the fill.
  - The labels and the L/R fills use these. The probe publishes `mixer.master.scale.<dB>`.
  - Unchanged: ADR-0067 section 5 hold and latch numbers, and the linear `MeterHoldState`.
  - The scale is the meter's own (ADR-0063).
- [x] **Gate.** `tests/ui_input_tests.cpp` `[ui][input][shell][mixer][meters][master-meter-scale]`. As built after the
  critic: the held marker and the live bar are measured apart in every segment, plus a 300 px dock.
  - Levels are the outer loop: -42/-24, then -18/-12, then 1.0/1.0. Sizes are the inner loop: 1280,
    1920 and 2560.
  - Precondition: the held peak equals the live peak. Make no message-loop calls after the first
    publish.
  - Asserts:
    - The label rects are present, disjoint and evenly spaced, with "0" at the fill top and "-60" at
      the fill bottom.
    - Painted label ink appears only inside the label rects.
    - Each bar top is within 1 px of its label centre or segment midpoint.
    - At 1.0 the clip is latched, checked with an explicit tolerance.
  - **Bites:**
    - Passing the linear peak puts the bar about 25% of the fill off its label.
    - Moving the probe but not the paint fails the ink check.
    - A law right only at the marks fails the midpoints.
    - Fixing one channel fails the L/R pair.
- [ ] **Drive.** SS7 Step 13 `ss7-loudness`: the held marker for -16.3 dBTP sits at y ≈ 910, between
  the "-12" and "-24" labels. Today it is at y 1008, below "-24". The rubric checks label overlap at
  all three sizes.
- [ ] **Record in STATUS.**
  - M6's master clause is still open: the master fader's unity point reads about -18 on the scale
    beside it.
  - The master meters now use the dB law while the other meters stay linear.

  Both are parked below.
- Size S.

### 8. The rail shows only real track data (FIX 6)

- [x] **Fix.**
  - Delete the `drawTrackGlyph` call (`MainComponentArrange.cpp:1213-1221`) and the function
    (`UiIcons.h:378-460`).
  - Keep `drawTrackKindBadge` exactly where it is, and fix the comments.
  - Re-pin the `[assets]` loop (`ui_screenshot_tests.cpp:1571-1583`) to `drawTrackKindBadge` with
    midi false and true. Each paints more than 8 pixels, and the two images differ.
  - Log the D3 omission in STATUS: the rail shows number, swatch, name, audio/MIDI badge and M/S/O,
    and no instrument icon until a track-category model exists.
- [x] **Gate.** `tests/ui_screenshot_tests.cpp` `[ui][screenshot][tokens][rail-icons]`.
  - Setup: five empty audio tracks at 1920x1080, rendered with `renderShellAtScale (*shell, 1.0f)`
    (software renderer).
  - Skip the selected and primary lanes, and any clipped rows.
  - Per row: the swatch is accentPurple, and the badge's middle bar differs from its gap.
  - The icon-plus-badge cells must be pixel-identical across rows, with at least 3 cells compared.
  - **Bites:** re-adding the index glyph makes the cells differ. Deleting the badge call fails the
    badge check.
- [ ] **Drive.** SS1 and SS6, with `-Exe` set to the exe under test and `-Shots` set to a new folder:
  no drum, guitar or mic drawings. Compare against the 2026-10-11 shots.
- Size S.

## Layout and readable text

### 9. Hiding the dock by key gives its height to the arrangement (FIX 10)

- [x] **Fix.**
  - Add `int laidOutDockHeight = -1` to `MainComponentShell.h`.
  - `resized()` records `dockedMixerHeight()` as its first statement.
  - `refreshActionState()` ends with a re-layout when the two differ.

  This covers chords, model verbs that open a dock tab, and the UI tick. The button path now lays
  out twice, which is harmless.
- [x] **Gate.** A new `[ui][input][shell][g2][dock-layout]` case, driven through `shell->keyPressed`
  rather than the harness (the harness calls `resized()` itself).
  - Sequence: Y, then Y to hide, then a resize, then Y to show, then an X round trip.
  - Check: the bottoms of the lanes, both scroll bars and the rail input; the splitter's visibility;
    the browser panel's bounds.
  - **Bites:** without the guard, the lanes end 300 px early with the splitter visible. After a
    re-show, the browser has an empty rect.
- [ ] **Drive.** SS8 `ss8-after-drops` and `ss8-refusal`: the horizontal scroll bar sits at
  y ≈ 1066-1076, level with the rail and inspector bottoms, and there is no line at y = 780.
  `ss8-audition-on` is unchanged.
- Size S.

### 10. The export Cancel control reads during an export (FIX 11)

A wider slot would break the master card's 2 px slack at `windowMinWidth`, and ADR-0063 forbids
smaller text. So the 28 px slot gets an icon, which plan section 7.4 item 3 allows ("a label or an
unambiguous icon **and** a tooltip").

- [x] **Fix.**
  - Add a cross icon for `ProjectExportAudioCancel` in `hasActionIcon`, `actionUsesIconOnlyChrome`
    and `drawActionIcon`. Use `juce::jmin`, with geometry as fractions of the bounds.
  - Make `exportAudioCancelButton` a `ToolbarActionButton`.
  - Call `setAction (ProjectExportAudioCancel)` before the first paint.
  - No token or layout change.
- [x] **Gate.** `tests/ui_input_tests.cpp` `[ui][input][shell][export][tokens]`, with a real paced
  export (`exportPaceMs = 60'000`) at `windowMinWidth`.
  - The export is in progress, and Cancel is visible, enabled and has a tooltip.
  - Render Cancel with and without glyphs. If it paints text, the text must fit with 2 px spare.
    Otherwise more than 8 pixels of icon ink are required.
  - Click Cancel: one cancel, not in progress, no destination file.
  - Extend `[assets]` to cover the new icon.
  - **Bites:** reverting to `juce::TextButton` (a 12 px box for a 40 px word), or removing the cross.
- [ ] **Drive.** SS8 Step 5 `ss8-unsaved-during-export` shows "Export N%" plus a cross. The mid-job
  click still ends with `cancelled`, no destination and no `.partial`. SS8 stays 212/212.
- [x] **Correct the STATUS record:** the H16 CP7 test never exercised Cancel during an export (recorded in unit
  10's STATUS entry).
- Size S. Measure the 48 px progress label in the close-out rubric (unit 15).

### 11. The status line shows messages whole (FIX 13)

Moving the line needs ADR-0076, an implementation ADR under ADR-0049 with a separate critic pass.
It amends ADR-0064's tool-row list without editing ADR-0064.

- [x] **ADR-0076 states** (accepted 2026-10-12; its critic ruled no conflict with §7.4 line 1 on two conditions now in the
  ADR - every app-written string fits the 1280 budget on every platform font, Verdana included, and shortening never
  mixes a message's "; "-joined parts - and set the budget at W - 720 px: 432 / 560 / 1200 at 1152 / 1280 / 1920):
  - **Placement:** the line moves to the empty right side of the menu-bar row. This costs no height
    and gives about 558 px of text at 1280 and about 1198 at 1920.
  - **Never squashed:** `setMinimumHorizontalScale (1.0f)`.
  - **Fixed texts** (hover hints, mode hints) fit the 1280 box by design, reworded to the measured
    budget on every CI font.
  - **User and OS text** (device names, paths, joined refusals) uses one defined middle elision.
    While elided, the tooltip is the full text, and the label intercepts the mouse so the tooltip
    can appear. The probe and accessibility keep the full text.
  - **From 1152 to 1279 px wide,** fixed texts may take the elision path too.
  - **DROPSET re-pin arithmetic:**
    - 1706x896 becomes `clip.inspector.markers timeline.nudge.chooser`.
    - 1536x793 becomes fades + `clip.inspector.markers timeline.edit_mode.chooser
      timeline.nudge.chooser`.
    - Other cells are unchanged.
  - **Contrast:** a dangerRed-on-header contrast pair.

  If the critic finds that the elision conflicts with section 7.4 line 1, stop and escalate (see
  Owner decisions).
- [x] **Commit: move.**
  - Set scale 1.0, re-pin the DROPSET and add the contrast pair.
  - Delete only `statusReserve` and its terms (`MainComponentArrange.cpp:558/560/564/569`) and lines
    588-603.
  - Keep the chooser placement, the renamed row anchor and `statusLineRightInset`.
  - Remove "status" from `harnessTimelineToolbarControls`.
- [x] **Commit: reword over-budget fixed texts:** the Clip and Note right-click tails, Ruler, Control
  lane, musical typing and step input.
- [x] **Commit: elision.** Add the elision label and the `mainComponentStatusLinePaintedText` seam.
- [x] **Gate.** `tests/ui_input_tests.cpp` `[ui][input][shell][g6][status-whole]`.
  - **Placement**, at 1280, 1920 and 2560:
    - The label is visible all the way up its parent chain (not `isShowing()`, which is false for a
      test shell with no window).
    - It sits inside the menu row, disjoint from the menu bar, at scale 1.0.
  - **Hint sweep:**
    - A 4 px grid covers the tool strip, ruler, lane, rail, mixer strip and piano roll.
    - Every zone prefix must be present, so an empty sweep cannot pass.
    - The mode hints are included.
    - Every fixed text fits with 2 px spare.
  - **Messages:** the import refusal and the export failure show whole.
  - **At 1152x640:** the elision rule holds.
  - **`preferences_tests` ADR-0061 cases:**
    - The rate warning shows whole at 1280x800 and at 1920.
    - The joined device line shows whole where it fits. Otherwise it is elided, starts with "Audio
      output", and its tooltip is the full text.
  - **Bites:**
    - Scale 0 today.
    - A 133 px box against a 228 px Pan hint.
    - Reverting any one reworded hint puts it over budget.
- [ ] **Drive.**
  - SS8 `audition-on` and `refusal` show their text whole in the menu row.
  - SS7 hover sweep at three sizes.
  - A piano-roll control-lane hover, and one shot with musical typing on.
  - Check at 100% and 150%.
- Size M.

### 12. Inspector cards drop whole and fit (FIX 12 + FIX 9)

Two commits.

- [x] **TAKES card (FIX 12).**
  - **Fix:** `inspectorTakesSectionBounds` clamps the card height to [68, 74] (constexpr, with
    `static_assert (whole <= most)`). Delete the dead `takesNeededHeight` check, so paint and layout
    share one rule.
  - **Accepted:** when takes exist and there is 50-67 px of room, the chooser and Delete now drop.
    The scrolling inspector stays parked (D50).
  - **Gate:** `[ui][input][shell][inspector-takes-card]`.
    - Use an explicit `setSize (1536, 960)` per settings-row state, then heights giving room
      30/10/50/67/68/108 with the row open and 70/10/50/67/68/108 with it closed.
    - The band below the content is pixel-identical between the Clip and Track tabs.
    - A whole card has text and contains the chooser and Delete. A dropped card has no heading ink,
      and both controls are empty.
    - At least 5 whole cells and at least 7 dropped cells.
  - **Bites:** reverting to `min (room, 74)` paints the heading and well into the band at room 30.
    The room-67 cell pins the threshold.
- [x] **Fade Curve row (FIX 9).** (As built: the rail is at least two thumbs at the default inspector and three at
  the widest - three at the default would need the chooser under 106 px, too narrow for "Equal power" in DejaVu Sans;
  at 1280x720 the gate uses the smallest dock, as the default one drops the whole FADES card.)
  - **Fix:**
    - Add tokens `inspectorFadeCurveControlLeftInset = 48` and `inspectorFadeCurveChooserWidth = 120`.
      The chooser takes the fixed width and the slider takes the rest. Update the comment at :515.
    - Delete the tooltip override at `MainComponentInspector.cpp:224`. This is required: today the
      chooser's only live tooltip is "H14 canonical fade law".
    - Optional: change `inspectorFadeCurveControlTopGap` from 4 to 1, so the controls sit on their
      painted row.
  - **Gate:** `ui_screenshot_tests.cpp` `[ui][screenshot][tokens]`, after :1647.
    - Three windows, each at inspector min, default and max width.
    - The two controls are disjoint, and every chooser item fits its text box with 2 px spare.
    - The painted "Curve" word is clear of the chooser.
    - At default and max width, the slider rail is at least 3 thumb diameters long.
    - Tooltips are worded and start with the control's title.
    - Capture a PNG per cell, and restore the view sizes at the end.
  - **Bites:** reverting the half-split gives a 13 px text box and a 23 px rail. Restoring line 224
    fails the tooltip check.
- [ ] **Drive.**
  - SS8 `ss8-unsaved-during-export` (row open): the inspector ends at FADE CURVE, with no TAKES
    fragment above the dock.
  - Still whole TAKES cards: `ss8-audition-on`, `ss8-after-drops`, and SS1 and SS2 at 1920.
  - At 1280x720 there is no TAKES card.
  - The ctest render `yesdaw-g62-display-1706x896-150.png` has no TAKES fragment.
  - FADES crops of SS3 (1920) and SS2 (2560): the shape name is whole, rail shows on both sides of
    the thumb, and the tooltip is worded.
- [x] **STATUS:**
  - Correct the G6.1 cp1 record that closed the "fade" truncation row.
  - Record that the "Curve" word and worded tooltips label the amount slider.
  - Park the under-42 px rail at inspector widths 240-283.
- Size S.

### 13. Mixer strip layout and lit cells (FIX 14, rest)

- [x] **Fix.**
  - **(a2) The drag readout moves into the strip.** Add `paintedFaderReadoutBoundsForLane`. A strip
    drag shows the transient in that cell, drawn like the cell itself (caption numeric font,
    controlInset fill, centred, border `Space::none`). The rail VOL readout keeps its place.
  - **(b) Narrow I/O text fits.** `drawIoSlot` trims its text box on both sides (no literal 0: the
    theme audit rejects it) and draws with `drawText (..., true)`. Narrow strips drop "In: " and
    "Out: ".
  - **(c) Lit cells.**
    - A lit R cell draws textOnFill ink. ADR-0063 requires this.
    - A lit S or M cell fills with the strip colour and draws textOnFill. This is an agent rubric
      call: lit and unlit letters differ by only 1.64:1 today, and the rail already fills its lit
      cells. Record it in STATUS as a deliberate departure from the reference's tinted letter.
- [x] **Gate.** `tests/ui_input_tests.cpp` `[mixer-states]`.
  - **Readout placement:** at three sizes, wide and narrow, on a Track strip and a Bus strip, with the
    drag held at mid-rail. The readout equals the cell, lies inside the strip, and misses the I/O
    rows, sends, inserts, M/S cells, pan knob, rail and thumb.
  - **Narrow I/O:** the output texts read "Bus 1" and "Master", fit their box, and leave the
    clearance columns free of ink.
  - **Lit cells:** a lit S or M cell's fill equals the strip colour (±2) with at least 6 textOnFill
    pixels. An unlit S stays controlInset. The R cell shows textOnFill and no kText.
  - **Bites:**
    - The transient back above the rail (8 px overlap with the Out row).
    - The "Out: " prefix back in narrow strips.
    - The lit fill reverted.
    - kText ink on R.
- [ ] **Drive.** SS7 `ss7-sweep-drag-*`, `ss7-narrow` and `ss7-solo`. Optional: publish `drag.db` in
  the probe, and have Step 17 assert it lies inside `mixer.strip.0` and clear of `.output`.
- Size S.

## Tooling

### 14. Evidence shots show the app's popups and alerts (FIX 15)

This unit touches only `tools/` and is independent of the product units. It must land before the
close-out rubric pass.

- [x] **Fix**, `tools/session-drive.ps1` C#.
  - Add `AppWindowsAbove (main)`. It walks up the z-order with `GW_HWNDPREV`, capped at 1024 steps.
    It keeps a window only if it is in the same process, visible, not minimised, not cloaked and not
    `WS_EX_LAYERED`. Layered windows are JUCE's drop-shadow strips.
  - `CaptureClient (h, path, composite)` composites those windows over the main client area
    (`PrintWindow`, `Format32bppRgb`, explicit rect, clipped to the client). The canvas size is
    unchanged. It returns the number composited.
  - `Shot -MainOnly` keeps today's capture. The log reads `(+N app windows)`.
  - SS7 Step 16's three pointer shots use `-MainOnly`. A JUCE tooltip can reappear within 500 ms and
    would trip `HoverStrokeShown`.
  - Update the header at line 31.
  - A plain screen copy is rejected: it pulls in other apps' windows (D3).
- [x] **Gate.** A new Windows-only ctest, `YesDawSessionDriveCaptureCheck`, running
  `tools/test-session-drive-capture.ps1`. The headless chooser check stays untouched.
  - **Windows** (WinForms, on the real `YesDawDrive` type):
    - a main window;
    - a topmost "menu";
    - an unowned, non-topmost "alert", the same shape as a real JUCE alert;
    - a layered window;
    - a same-process window below main;
    - a window from a foreign process.
  - **Preconditions** use `WindowFromPoint`. A failed precondition is a FAIL that names the window in
    the way, never a skip.
  - **Asserts:**
    - The PNG is exactly main's client size.
    - The menu and alert colours show.
    - The layered, below and foreign windows' centres show main's colour.
    - The return value is 2.
    - With `composite = false`, the menu's centre shows main's colour.
  - **Bites, M1-M7:**
    - M1: today's code.
    - M2: ignoring z-order.
    - M3: a screen copy.
    - M4: topmost windows only.
    - M5: owned or topmost windows only.
    - M6: no layered filter.
    - M7: no process filter.
- [ ] **Drive.** Re-run on the same exe into `build-ci/session-shots/<date>-capture/`.
  - These shots each log N >= 1 and show the menu or alert: SS3 `ss3-evidence-*`, SS4
    `ss4-evidence-instrument-popup`, SS8 `ss8-missing-*` and `ss8-unsaved-during-export`.
  - SS7 Step 16 logs "main only".
  - Any other +N gets its window named in STATUS.
  - If `ss8-missing-*` still logs +0, record it as a finding (the alert may sit off the main client).
    Whether the main window is white under the launch-time alert is a separate question, judged from
    the new shots.
- Risk: the first exact-code CI run must prove that the Windows runner has an interactive desktop. If
  it does not, redesign the gate with a critic pass; never skip it or move it.
- Size S.

## Close

### 15. Re-run the rubric and close the FIX list

- [x] After units 1-14, run the full SS1-SS8 batch on one packaged candidate with the new capture,
  into a fresh folder. (2026-10-12, candidate `3f8dc35`: all green but SS7, which had no clean run - a
  foreign window kept taking the foreground; see STATUS.) Judge the rubric at 1280, 1920 and 2560 and at 150% scaling. Record each
  section 7.4 line in STATUS as PASS or FIX, with its gate name. Any new FIX gets its own unit.
- [x] Measure the export progress label with unit 10's text-fit method. (It squashes: new unit 16.) While an export runs it
  shows "Export 10%" to "Export 99%" and "Cancelling...", and two-digit values probably squash. If
  they do, it becomes a new FIX.
- [x] Tick STATUS rubric FIXes 1-15. This unblocks the close-out's
  [unit 7](2026-10-09-cheap-autosave-closeout.md).

### Units 16-28: the close-out rubric's new FIXes (2026-10-12, not yet built)

The unit 15 rubric pass found these, each in the before-shots too except 18, which only became visible once alerts
were composited. Each is its own unit, worst first, under the same checkpoint rule (ADR-0075 cadence).

- [x] **16.** The export progress label squashes "Export 10%"-"Export 99%" and "Cancelling..." (about 64-69 px of
  text in its 48 px box, in Verdana / DejaVu). Line 1. **Done 2026-10-12:** the percent alone over a foot bar;
  "Cancelling…" takes the whole slot with the cross hidden; gate `[export-progress]` (STATUS).
- [x] **17.** The main window is white under the launch-time missing-media alert (`ss8-missing-cancel` 91.5 % white,
  `ss8-missing-rejected` 90.3 %). Line 1 / 6. **Done 2026-10-12:** the first UI tick with the window showing posts the
  question; SS8 Step 4 asserts the window painted under it (STATUS).
- [x] **18.** The stock JUCE alert style: its icon is clipped at the alert's top-left corner and sits under about 35 px of
  the title and body text (`ss8-unsaved-during-export`, `ss8-missing-*`). Line 1. **Done 2026-10-12:** the theme draws
  alerts (a 28 px icon whole in the gutter, theme colours and type) and is the app's default look; gate `[alert-style]`.
- [x] **19.** The settings row's device choosers are squashed, then elided ("Speakers (Focusrite USB A...").
  Line 1 / 4. **Done 2026-10-12:** device names whole or shortened in the middle (ADR-0076's rule), never squashed; the
  choosers grow into the row's spare width; the channel chooser's texts whole; gate `[device-choosers]` (STATUS).
- [x] **20.** The piano roll's key labels C3-B3 lose their tops and bottoms to the key rows. Line 1. **Done
  2026-10-12:** a key's name sits whole in its light face, placed on the font's own outlines, or is not drawn; gate
  `[piano-key-labels]` (STATUS).
- [x] **21.** The loop's right end-handle covers the tempo label ("150~") at 1280. Line 1. **Done 2026-10-12:** the
  loop paints a strip at the top of the bars row, the row's labels sit under it; gate `[ruler-loop-strip]` (STATUS).
- [x] **22.** A MIDI clip's note-preview bar cuts through its name. Line 1. **Done 2026-10-12:** a named clip's notes
  stay under its name strip, their rows placed by fractions; gate `[clip-notes-name]` (STATUS).
- [x] **23.** The send rows' PRE / PST text is 3.59:1 on the strip header colour. Line 4. **Done 2026-10-13:** the tap
  is drawn in the text colour; contrast pairs for a send row's level bar; gate `[send-tap-contrast]` (STATUS).
- [x] **24.** A meter's yellow band follows the bar's height, not the level (yellow at about -41 dBFS). Line 7. **Done
  2026-10-13:** yellow from -6 dBFS on each meter's own scale (half a linear meter, the top 17 % of the master's), the
  held marker in its zone's colour; gates `[meter-zones]` (STATUS).
- [x] **25.** The last ruler label is cut at the right edge ("12" / "4:", "0:19."). Line 1. **Done 2026-10-13:** a
  ruler label is drawn whole or not at all; gate `[ruler-edge]` (STATUS).
- [x] **26.** A status message outlives its undo ("Routed to new Bus 2" after Bus 2 was undone). Line 7. **Done
  2026-10-13:** an undo or redo takes a success message down and itself stays quiet; an error stays up to decay; gate
  `[status-undo]` (STATUS).
- [x] **27.** Bars beside a tempo label go unlabelled (265 px between "33" and "57" at 1920). Information. **Done
  2026-10-13:** a change's label is as wide as its text, and a bar number gives way only within a 4 px gap of a change's
  tick or text; gate `[ruler-map-gap]` (STATUS).
- **29.** (found by unit 17's critic, measured) While a modal question runs inside a batch of JUCE's posted messages,
  later messages of that batch wait - on Windows the window's vblank among them - so the main window and the question's
  own alert stop repainting: hover, press and Tab focus on its buttons are not drawn. Measured on the unit 17 build: the
  shell's paint count holds at 2 for 3 s under the launch-time question, while the UI tick asks for about 30 repaints a
  second (30 in 0.8 s once it is answered). Before unit 17 the same question also left the window white. The likely
  fix is asking without a nested modal loop (`AlertWindow::showAsync`, `FileChooser::launchAsync`) for the ADR-0062
  relink questions. Gate: the paint count climbs while a question is up. Line 1 / 4. **Decided 2026-10-13:**
  [ADR-0077](../adr/0077-questions-without-a-nested-modal-loop.md) (two critic passes): every question, not only the
  relink ones. Units, each landing whole:
  - [x] **29a** the `Question` type and reply, the ask helper (open-question mark, focus restore), `attachGeneration()`; the
    single-question flows (Import / Export Audio and MIDI, the Sampler pad, Save a Copy, Save as Template). **Done
    2026-10-13**; gates `[questions]` (STATUS). Its focus restore is gated by 29e's drive.
  - [x] **29b** Save / Save As, both unsaved-changes questions, New / Open / Open Recent with ADR-0068's closing steps, and
    quit (`mainComponentRequestQuit`, the close button, `systemRequestedQuit`). **Done 2026-10-13**; gates
    `[project-lifecycle][questions]` (STATUS). Notes from 29a's critic: on Linux the
    zenity chooser is not modal, so a quit is refused by the open-question mark, not by modal state; `MainComponent.cpp`
    is at 2995 of its 3000-line pin (`theme_audit_tests.cpp`), so 29b's flows go in the other shell files.
  - [x] **29c** the relink loop, an open completion, the launch-question tail; SS8 runs. **Done 2026-10-13**; gates
    `[relink][questions]`, SS8 214/214 (STATUS).
  - **29d** `JUCE_MODAL_LOOPS_PERMITTED=0` on `YesDaw` and the text gate; the gate also bans calling a seam outside the
    ask helper (`fileChoices.choose...(` / `fileChoices.confirm...(` in `src/`): `Question`'s friend is the whole shell
    class, so the compiler alone does not keep every ask on the helper (unit 29a critic); and `Main.cpp`'s close button
    calls `systemRequestedQuit` while its quit lambda calls `quit()`, never `systemRequestedQuit()` (unit 29b critic).
  - **29e** (Windows drive, off CI) the paint count climbs under the SS8 launch alert and File-menu questions opened by
    mouse; Space works after a chooser without a Focus step.
- [x] **30.** (found in unit 20) At the shortest editor dock (160 px) the piano roll has no key rows: its header and the
  velocity / control lanes take the whole dock, so the grid is about 1 px. Line 1 / 3. **Done 2026-10-13:** the lanes
  give way, whole and the last first, while the grid would hold fewer than 10 keys at the 11 px target row; gate
  `[pianoroll-dock-min]` (STATUS).
- [x] **28.** Evidence hygiene: the end-of-drive size shots now include the resting pointer's tooltip (it hides the ruler at
  the tempo change in `ss3-2560`); park the pointer or use `-MainOnly` for size shots. Tooling. **Done 2026-10-13:**
  `SizeShot` parks the pointer on the window's caption and asserts no app window is over the shot (STATUS).

## Parked, recorded in STATUS / parking lot, not in this pass

- (unit 26 critic) The undo history window names a multi-edit group by its newest edit while it can be undone and by
  its oldest once undone (`ProjectUndo.h` `foldSteps`), so the row "Track Output (2 edits)" becomes "Add Bus (2 edits)"
  on Ctrl+Z. Naming the redo side by the group's newest entry would fix it; it changes the engine's history view.

- (unit 18 critic) An alert's text field (Save as Template, Save Preset) keeps JUCE's stock slate field inside the
  themed alert: readable, not in theme colours. Theming `TextEditor` colours is app-wide, so it waits for its own unit.

- The tempo cell's mouse wheel (found by unit 1's critic): JUCE's default wheel moves it about 27 BPM per
  Windows notch, each notch its own undo step. Decide between no wheel on the cell and a fine, one-step
  wheel when unit 2b's gesture grouping exists.

- ~~Tempo drags make one undo step per pixel~~ - done as unit 2b (2026-10-12).
- The painted pan knob has the same Touch-ride paint and value bug as unit 3.
- The header counter's `%06.3f` min:sec can roll over to "0:60.000".
- Lane grid lines use fixed 4 s / 16 s steps rather than bars, and the snap grid uses only the head
  tempo.
- The master fader needs M6's ticks and a unity mark, with `[fader-scale]` extended to the master.
  Separately, one meter law should cover every meter.
- The mixer's index-cycled fallback colour disagrees with the rail's purple for tracks with no
  colour set.
- Narrow strips:
  - The readout squashes values of -10 dB and below.
  - Insert and send text is squashed.
  - The fader cap slides under the readout cell at low gain.
- Fader display:
  - The Latch/Write display after release.
  - The 1 px jump when pressing the cap's centre.
- Inspector:
  - The fade amount has no numeric readout.
  - The rail is under 42 px at inspector widths 240-283.
  - The TAKES heading is missing its label inset.
- Add the whole-section paint check to every `[layout][scaling]` matrix cell.
- The master scale's four labels collide when the dock is under about 276 px (the meter column under 46 px), and the
  INTEGRATED / TRUE PEAK cards overflow at the minimum dock. Pre-existing (unit 7's critic); thin the labels by the
  height available.
- The header height has unit 9's bug: restoring an autosave reattaches the same bundle, which resets the settings
  row to hidden without a re-layout, so the shell keeps the taller header's layout until the next resize. Compare a
  combined key (header height, dock height, inspector width) instead of the dock height alone (unit 9's critic).
- ADR-0076's message sweep reads only `reportStatus` calls whose text is one literal (25 of about 97). App-written
  messages built from a literal plus an app reason or number (Duplicate Track / Save As / preset refusals, the sends
  limit, the key-binding conflict, the fixed export failures) are not measured; a catalogue of their longest variants
  would close it (unit 11's critic).
- A lit S / M cell letters textOnFill on the strip's colour; a project file may carry any opaque track colour, and a dark
  one would give about 1.3:1 (the old tint failed there too; the colour cycle's accents pass). Pick the ink by the
  fill's luminance (unit 13's critic).
- The frame-time gates (`TimelineFrameCheck.h`) paint with no tempo map, so unit 6's map path is not
  under the frame budget. The unit 6 critic measured at most 140 us a paint for a 1000-change meter
  map.

## Owner decisions

None are needed to start. One is conditional:

- **Unit 11, only if ADR-0076's critic rules that eliding user or OS text conflicts with section 7.4
  line 1** ("Nothing overlaps, clips, or is cut off"). Question: a message containing a long device
  name or path cannot fit the box at 1280. How should it show?
  - **A (recommended):** elide the middle, keep the lead and the final clause, and show the full text
    in a tooltip. The probe and accessibility keep the full text.
  - **B:** two 13 px lines in the full 28 px menu row.
  - **C:** a 20 px status bar at the window bottom. This breaks the eight-lanes-at-1080p gate.

These are agent choices recorded in STATUS, not questions for Dan:
- Unit 1: the tempo scrub is now relative.
- Unit 3: keyboard rides now write the held value.
- Unit 4: the playhead badge is removed rather than renumbered.
- Unit 5: tenths mode floors.
- Unit 13: lit S/M cells are filled, a departure from the reference.