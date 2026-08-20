# Mixer-surface & arrangement backlog — stop shipping a debug console (2026-08-15)

Carved after a fresh adversarial re-audit of current `main` (head `db361b4`, the M1–M14 mix-truth run
complete) against Pro Tools / Logic-class usability. The audit read the REAL paint and control code
and judged rendered screenshots of the shipped shell at 1920×1080 (`yesdaw-timeline-large.png`,
`yesdaw-mixer-large.png`, `yesdaw-automation-default.png`); the load-bearing findings are quoted with
`file:line` in the item hints below.

The headline finding: **the mixer's control surface is a debug console, and it is wired to the wrong
track.** Down the left of both the Mixer and the timeline dock sits a stack of buttons reading
`Audio 1 meters: peak n/a`, `Audio 1 sends: none`, `Audio 1 FX: none`, `Audio 1 GR: none`,
`Bus FX: no Bus` — and every one of them reads `surface.tracks.front()`
(`src/ui/MainComponent.cpp:7699`, `:7749`, `:7778`), NOT the selected strip. Select track 3 and the
panel still reports track 1. One of them prints a raw engine node id into the UI
(`:7705` — `" node " + juce::String (send.faderNodeId)`). Beside them the real verbs (add FX, add/remove
Bus, output routing, add Send, per-FX param editors) live as a column of unlabelled `…` buttons. No
shipping DAW looks like this, and the labels are the same class of lie M1–M9 spent a whole backlog
killing elsewhere.

Second headline: **the selected strip's Mute and Solo are the wrong widget.** `mixerMute` and
`mixerSolo` are `juce::ToggleButton`s (`src/ui/MainComponent.cpp:9859`–`:9860`) configured with
`setButtonText("M")` and `juce::TextButton::buttonColourId` colours (`:5326`–`:5359`) — a ToggleButton
ignores TextButton colour ids and draws a tick-box plus text, so at strip width the shipped controls
render as two empty checkboxes with a truncated `..` label. Both screenshots show it: unselected
strips paint a clean `S`/`M`, the SELECTED strip shows two blank boxes.

## How to work this list (non-negotiable)

The full protocol lives in `docs/goals/2026-08-11-overnight-backlog-run-brief.md` and the 2026-08-12
brief, and applies verbatim: one item at a time, strict top-to-bottom order, audit-before-build,
shipped-boundary gates that fail before and pass after, full local ctest with the owner-file
isolation ritual, one feature commit + exact-head nine-job CI green + a separate docs-only evidence
commit per item, never edit ADRs / goldens / `[[clang::nonblocking]]` annotations /
`.github/workflows/ci.yml`, never weaken or delete an existing gate (re-pinning legacy assertions to
NEW semantics is allowed and expected), stop after 3 consecutive red CI rounds on one item.
Multi-track behavior is gated on 3+ track fixtures. For UI work: render the real shipped shell at
real window sizes, judge it yourself against PT/Logic, iterate until it looks legit, THEN lock the
fix with a mechanical token/layout gate.

Items are numbered **N1–N8**; tick each here with the commit SHA + run id in the docs-only evidence
commit.

## Phase 1 — the mixer stops looking (and lying) like a debug console

1. [x] **N1 — Mute and Solo are real buttons, on every strip.** DONE — feature `2f4aadc`,
   exact-head nine-job CI run `31873919992` green (first try), local 352/352. ONE law
   (`paintedMuteSoloCellBoundsForLane`) now drives the paint, the click law, the harness export and
   the gate; every strip paints its cells and a click on ANY strip's cell toggles THAT strip — track
   or bus — through the same undoable verb without stealing the selection. Buses got the Track twins
   (`toggleBusMute`/`toggleBusSolo` on a shared `editBusStripPanelPreserving` helper riding the
   existing `SetBusMixScalars` verb). The Solo/Mute verbs kept a labelled home in the control lane
   and their widgets are TextButtons, so the colours the code always configured finally apply.
   Judged visually before gating: all three strips now render identical, legible S/M cells at
   1920×1080. `[strip-mute-solo]` (87 assertions) red before at assertion 31 — the third strip's
   click did nothing. **Process note:** the repo's `build-ci/ui-screenshots/*.png` are only rewritten
   when `YESDAW_UI_SCREENSHOT_DIR` points there; without it the harness writes to TEMP, so a stale
   PNG can look like an unfixed bug. Set it when judging.
   Original spec: The selected strip's M/S are
   mis-typed `juce::ToggleButton`s styled as TextButtons, so they paint as two blank checkboxes with
   a `..` label (`src/ui/MainComponent.cpp:5326`–`:5359`, `:9859`–`:9860`) — the one strip the user is
   working on is the one whose controls look broken. Make them the same painted, correctly-typed
   control the other strips already show, and make M/S clickable on ANY strip (today only the
   selected strip's widgets act; the painted `S`/`M` on other strips are decoration — audit the hit
   law before building). *Gate:* `[strip-mute-solo]` — on a 3-track project, clicking the painted M
   of the THIRD strip mutes the third track (not the selected one) and changes the render; the same
   for S; the widget carries its label at the shipped strip width (no truncation); one undo restores;
   the selected strip's controls are the same type and bounds as every other strip's. Expected to
   fail before on the third-strip click doing nothing.
2. [x] **N2 — Every mixer readout names the strip you selected.** DONE — feature `035ea93`,
   exact-head nine-job CI run `31910424452` green (first try), local 352/352. One law
   (`readoutStripFor`) picks the strip all five readouts describe: the SELECTED strip, tracks first
   then buses, falling back to the first strip only when nothing is selected — and every readout
   NAMES the strip it describes, so it can never claim to be about another one. Buses report their
   own meters and FX, and report sends as `n/a` because a Bus carries no sends in this model rather
   than borrowing a Track's. The Bus FX readout stays Bus-specific but follows the selected bus.
   Every `" node <id>"` is gone. `[mixer-readouts]` (120 assertions, 3 tracks + a bus with distinct
   FX per strip) red before at assertion 66 (meters said "Audio 1 meters: peak n/a" with track 3
   selected). Four legacy assertions that pinned the node id INTO the readout are re-pinned to prove
   its absence.
   Original spec: Five shipped readouts are
   hardwired to `surface.tracks.front()` — meters (`:7690`), sends (`:7699`), FX (`:7749`), gain
   reduction (`:7778`) — and one prints a raw engine node id (`:7705`). Point every readout at the
   SELECTED mixer target (track or bus), and drop the node id: an engine identity is not a user
   readout. *Gate:* `[mixer-readouts]` — on a 3-track project with distinct FX/sends per track,
   selecting each strip in turn makes every readout report THAT strip (name, send count, FX names,
   GR, meter), a bus selection reports the bus, and no readout string contains a node id. Expected to
   fail before at the first non-first-track selection.
3. [x] **N3 — The Mixer fills its window and the master is a strip.** DONE — feature `51882bc`,
   exact-head nine-job CI run `32402786217` green (first try), local 352/352. Audited first:
   `paintedMixerMasterBounds()` peeled its slice off the far right of the FULL panel independently
   of how many track/bus strips the loop had already drawn from the left, so a clamped strip width
   (84-112px) left ~1250px of dead black between the last strip and master at 1920×1080. ONE law
   (`paintedMixerLaneBounds`) now computes every strip including master — master is lane index
   `stripCount`, the slot immediately after the last strip, flush against it by construction. The
   legible band widened to 84-220px, judged visually against Logic's mixer at all three D7 sizes:
   1152×720 fills completely, 1536×960 and 1920×1080 cover a clear majority instead of a sliver.
   Honest subset (D3): master keeps its real fader (already existed, E19) and its LUFS/true-peak
   readouts; it does NOT gain pan or insert slots, since the engine model carries no
   `masterFxChain`/master-pan field to back them — inventing those cells would be exactly the fake
   data this plan forbids. `[mixer-layout]` (86 assertions, 3-track shell, all three D7 sizes) red
   before at the contiguity assertion (342px gap vs the ≤6px allowed), proven by reverting the
   master-bounds law while keeping the new geometry harness export
   (`mainComponentPaintedMixerStripBounds`/`MasterBounds`). Two pre-existing tests had pinned the
   OLD broken geometry as truth — a fixed master-fader position, a hardcoded right-edge screenshot
   sample region — and are re-pinned to the new law, which is strictly stronger (a detached island
   can never satisfy either any more).
   Original spec: At 1920×1080 the panel puts
   three ~100 px strips at the far left and the master island at the far right with ~1250 px of dead
   black between (`yesdaw-mixer-large.png`), and the strips are ~9:1 tall so the fader thumb is a
   speck on a 900 px rail. Give strips a real width band, lay them out from a single law, and place
   the master as the RIGHTMOST STRIP (same insert/fader/pan/meter shape as a track, keeping its
   LUFS/true-peak readouts) instead of a detached island. Judge it against Logic's mixer before
   gating. *Gate:* `[mixer-layout]` — the painted strip band covers a minimum fraction of the panel
   width at 1152×720 / 1536×960 / 1920×1080; strip width stays inside a legible band at every size;
   the master strip's painted bounds sit inside the strip band with its fader rail on the shared
   fader law (M6); nothing overlaps and nothing is clipped.

## Phase 2 — automation you can trust

4. [x] **N4 — The automation lane belongs to its track, and its header tells the truth.** DONE —
   feature `8e779a1`, exact-head nine-job CI run `32407151792` green (first try), local 352/352.
   Audited first: `automationLaneRowText()` always read `project.tracks.front()` and hardcoded the
   string "Track fader"; a DEEPER bug the original carve missed — the header's Add/Delete BUTTONS
   (unlike the canvas click path, which already read `currentAutomationTarget()`) were hardcoded to
   `addFirstTrackAutomationBreakpoint()`/`deleteLastFirstTrackAutomationBreakpoint()`, always
   writing to track 1's fader lane regardless of the selected target.
   **Deviation from the literal spec, logged:** "anchor the lane under its own track row" is built
   as a SUB-LANE carved from the BOTTOM of the target row's own rect
   (`TimelineCanvasGeometry::automationLaneArea`), not a new row inserted after it. Reason: the
   existing E5 law stretches a lone track's row to fill the entire viewport when there are few
   tracks (confirmed by a failing local build first — the original "insert a new row after the
   target" design left ZERO room whenever the target row already filled 100% of the clip area, the
   common case for every small test fixture in this repo). Carving from the row's own space works
   in every case (1 track or 20) without touching the golden-tested, allocation-free clip
   virtualization law in `TimelineLayout.h`, which a row-cascading insert would have required.
   `clipArea` also no longer shrinks by a fixed amount to reserve room — it uses its full height
   always, removing the visual "jump" the old toggle caused.
   `[automation-lane-owner]` (a 3-track shell, track 3 targeted with Pan): the painted lane rect
   sits under track 3's row (not track 1's, not a fixed top band), the header names track 3 and
   Pan, a breakpoint added via the CANVAS and via the header's ADD BUTTON both land on that same
   lane (never a stray track-1 lane), the DELETE button removes from it too, and switching track
   updates both the header text and the lane's position. Two pre-existing tests pinned the OLD
   geometry/text as truth and are re-pinned to the new law, which is strictly stronger.
   Original spec: The lane
   is one global strip floating above ALL clips, not a sub-lane of any track row
   (`yesdaw-automation-default.png`), and its header text is hardwired to `project.tracks.front()`
   and the string "Track fader" (`src/ui/MainComponent.cpp:7345`–`:7360`) while the canvas edits the
   SELECTED track's chosen target (`:8499` `buildAutomationTargetOptions`). Editing track 3's Pan
   reads "Audio 1 - Track fader". Anchor the lane under its own track row (aligned to that row's
   lane geometry) and make the header name the real owner and the real target. *Gate:*
   `[automation-lane-owner]` — on a 3-track project, opening the lane on track 3 with target Pan puts
   the lane rectangle under track 3's row and the header names track 3 and Pan; adding a breakpoint
   writes to THAT lane; switching target/track updates both. Expected to fail before on the header
   text and the lane's y position.
5. [x] **N5 — Automation write from a control move (Touch/Latch).** DONE — feature `2a0aed3`,
   exact-head nine-job CI run `32411492541` green (first try), local 352/352. Adds a per-project
   `AutomationMode` (Read/Touch/Latch, schema v14, locate-points pattern) with a shipped mode
   chooser next to the automation lane's target chooser.
   The hard part was RT/architecture, not the mode itself: audited first and found EVERY existing
   edit-adoption path (`adoptEditedProject`/`WithoutPlaybackRebuild`) unconditionally calls
   `resetContextForFreshPlayback()`, stopping the transport and zeroing the playhead — a
   pre-existing, load-bearing characteristic of the WHOLE engine (every edit already does this,
   not something N5 introduced). Persisting a breakpoint on every drag tick would therefore reset
   playback after the FIRST write, collapsing every point in a ride to tick 0 — "breakpoints
   across a moved span" would be impossible. A first attempt confirmed this by building it the
   naive way and watching the ride collapse; the fix samples (tick, normalized value) into a
   client-side buffer during the drag, touching `project_`/the undo stack NOT AT ALL until the
   drag ends, then commits the whole ride as one transaction-grouped edit
   (`commitAutomationTouchRide`). Normalized-value conversions match the engine's own inverse
   mappings exactly: `FaderNode::parameterSpec`'s dB range for the fader, `PanNode`'s linear
   -1..1 for pan, so a recorded point plays back at the same gain/pan the control was actually at.
   `[automation-write]` (a 3-track shell, track 2 targeted, three real mouse-drag ticks with
   rendered playback advancing the playhead between them): one lane with breakpoints at
   increasing ticks and increasing values; the render audibly follows them; one undo removes the
   whole ride; Read mode writes nothing but still persists the plain fader edit exactly like
   today. RUNTIME red before, proven by disarming the ride while keeping the mode chooser and
   commit verb in place — the lane stayed empty.
   Original spec: Automation is draw-only: there
   is no Read/Touch/Latch/Write mode anywhere in the model (`grep AutomationMode` finds nothing), so
   a fader ride during playback is lost — the single biggest missing automation workflow. Add a
   per-project automation mode with at least Read (today's behaviour, the default) and Touch/Latch:
   with the transport rolling and the mode armed, moving the shipped fader/pan control writes
   breakpoints into that target's lane at the playhead, and Read plays them back. Never let a mode
   silently discard existing points. *Gate:* `[automation-write]` — rolling the transport in Touch,
   moving the selected track's fader across a span, then stopping leaves breakpoints in that track's
   Fader lane at the moved ticks with the moved values; the render follows them; Read mode writes
   nothing; one undo removes the whole ride as one step. RUNTIME red against today's no-mode model.

## Phase 3 — arrangement ergonomics

6. [x] **N6 — Track height (persisted, resizable).** DONE — feature `5e127c4`, exact-head
   nine-job CI run `32417198031` green (first try), local 353/353. Adds a persisted per-track
   `heightPx` (0 = auto-shared; schema v15, the v10-ALTER pattern) and a shared
   `computeCumulativeRowGeometry` law consumed by BOTH the track-list rail and the timeline
   canvas's per-lane clip geometry, so a resized row's rail height and its clips can never drift
   apart. `TimelineLayout.h`'s clip virtualization took optional per-lane top/height arrays,
   falling back to the old fixed-height math when null.
   **Deviation from the literal spec, logged:** the first auto-share formula divided
   `(availablePixels - customTotalPx)` by the count of NON-customized rows, so customizing one
   row's height silently changed every OTHER auto-shared row's height too — caught by the gate's
   own "changes THAT row's height and nothing else" assertion. Fixed by dividing the total
   available pixels by the TOTAL row count regardless of customization, so a customized row grows
   the panel's total content height instead of redistributing space taken from its neighbors — a
   real design decision, not just a bugfix.
   `[track-height]` (a 3-track project): a drag on track 2's row boundary changes track 2's height
   and no other track's; its clip rectangles and rail row move together; it survives save/reopen
   (including a legacy v10-bundle migration to the auto-shared default); it clamps at 56px/400px;
   one undo restores it as a single step. Red before at `project.tracks[1].heightPx > 0`, proven by
   disabling the row-boundary hit test while leaving persistence/undo/geometry in place.
   Original spec: Rail rows are a fixed ~200 px
   (`yesdaw-timeline-large.png` fits four tracks in a 1080p window) and `Track` carries no height
   field (`src/engine/Project.h:340`). Add a persisted per-track height with a drag gesture on the
   row boundary, clamped to a legible band, honoured by the rail, the timeline lanes, the clip
   geometry and the automation lane. Schema bump. *Gate:* `[track-height]` — on a 3-track project a
   drag on the row boundary changes THAT row's height and nothing else, the clip rectangles and the
   rail row follow it, it survives save/reopen, it clamps at both ends, and one undo restores it.
7. [ ] **N7 — Track colour (persisted, used everywhere).** Nothing in `Track`/`MixerStripState`
   carries a colour; the rail and mixer tint by index, so a user cannot organise a session visually.
   Add a persisted per-track colour, set from the shipped UI, applied to the rail row, the mixer
   strip nameplate and the clips on that track. Schema bump. *Gate:* `[track-colour]` — setting a
   colour on track 2 changes track 2's painted rail/strip/clip colours and no other track's, survives
   save/reopen, and one undo restores the previous colour.

## Phase 4 — recording completeness

8. [ ] **N8 — Punch in/out.** The recording window supports count-in and loop cycles but has no
   punch region: `RecordingWindow::punchStartFrame/punchEndFrame` exist in the engine
   (`src/engine/Recording.h:57`) and only ever carry the count-in boundary
   (`src/ui/UiAppModel.h:587`). Add a persisted punch region, a way to set it from the shipped UI
   (ruler gesture, like the loop region), and wire it into the capture window so audio and MIDI
   outside the punch are rejected by the SAME mapping that already rejects pre-roll. Schema bump.
   *Gate:* `[punch-record]` — with a punch region set, a capture that starts before it and ends after
   it commits a take covering EXACTLY the punch span (frame-exact at both edges), MIDI outside the
   punch never lands, the region survives save/reopen, and disabling punch restores today's
   behaviour bit-identically.

## Explicitly out of scope — do not fake

- **MIDI CC / sustain / pitch-bend / aftertouch** — still no CC model anywhere in the engine; it is a
  schema + editor + recording track of its own. Unchanged from the last backlog.
- **Same-track clip overlap policy** — overlapping clips still sum. A product decision only Dan can
  make. **Recommendation stands:** Logic-style "later clip wins, earlier clip trimmed
  non-destructively" behind an explicit toggle.
- **H18 plugin hosting proper** — gated on ADR-0037's Smoke 2, which needs a VST3 installed on the
  owner machine (M14 built the harness; the run is Dan's). H18 also needs its own kickoff ADR.
- **VCA / track groups, folder tracks** — a routing + selection model of their own; do not fake with
  a multi-select.
- MIDI clip trim/split, comping UI beyond the existing Comp button, per-track instrument choice /
  patch editing (ADR-0043 defers it), stems export, reverse playback, time-stretch/elastic audio,
  tab-to-transient, ripple editing, freeze/bounce-in-place, video.

If an item above collides with one of these, land the honest subset and say so in `STATUS.md`.

## Definition of done (whole list)

Every landed item: gate-covered at the shipped boundary (failing before, passing after; 3+ track
fixtures for multi-track behavior), suite green locally with the owner-file isolation ritual, one
feature commit, EXACT-HEAD GitHub Actions run green across all nine jobs, then a separate docs-only
evidence commit ticking the item here with the SHA + run id and adding the STATUS certification
paragraph. Visual items additionally require rendered shipped-shell screenshots judged before the
gate is written.
