# 0052. Automation v2: stacked lanes, real Write and Latch, span-replacing rides, clip-following points

- **Status:** Accepted (agent, 2026-10-06, under ADR-0049's implementation-ADR clause, after two separate
  agent critic passes: the first pass's eight required changes are folded in; of the second pass's two, the
  ride mechanism is now specified, and the claim that the evaluator extrapolates outside a lane's points was
  checked and refuted — `automationCurveProgress` clamps progress to 0..1 (`src/engine/Automation.h`) and the
  `[characterization]` test "clamps before the first and after the last breakpoint" pins the hold.
  Committed alone, before any G4.6 code.)
- **Date:** 2026-10-06
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G4.6 ("stacked per-track lanes, real Write mode, pencil/line tools, region-follow
  toggle, instrument parameters as targets; gate: render goldens, `[automation-v2]`") in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md);
  **ADR-0039** (lanes, targeting, runtime — kept; it scoped "automation wins over manual posts" to
  read-mode semantics and deferred Write recording to "a future ADR": this is it); ADR-0009 (event format —
  unchanged); ADR-0010 (time model); ADR-0047 (instrument parameters are targets — persistence repaired in
  schema v33); ADR-0046 (reference-DAW parity is the UI law). Write was parked only in code
  (`Project.h`), not by an Accepted ADR.

## Context

Automation today: lanes keyed by (owner, role, param) hold breakpoints in **musical ticks** that the engine
compiles through the tempo map; the evaluator shapes each segment by its **left** point's curve. One band
overlays the selected track's row, so a track shows one target at a time. The project-wide mode is Off,
Read, Touch or Latch; Latch behaves exactly like Touch and Write is parked. A Touch ride only *adds* points
(a ride landing on an existing point's tick is refused whole) and is not heard while it records. Moving a
clip never moves automation. The arrange view's per-project view state is a sidecar text file of
tab-separated keys whose reader ignores unknown keys. Decisions here are hard to reverse: stored modes, a
stored project setting, stored view keys, and what a ride or a tool erases.

## Options considered

**Lanes on screen**
1. **Every lane a track owns, stacked under its row (chosen)** — Logic's "show all automation" / Pro Tools'
   stacked playlists. Pros: two targets seen and edited together. Cons: a tall arrange area.
2. **One lane per track with a chooser (today).**

**What a ride writes**
1. **A ride replaces the span it writes, edges anchored (chosen)** — what Logic and Pro Tools do. Cons: a
   pass can erase earlier work (one undo restores it).
2. **Add-only (today).** Cons: a second pass over a span is refused.

**Automation and clips**
1. **A saved project setting, "Automation follows clips", default off (chosen)** — Logic's "move automation
   with regions" without its prompt.
2. **Always / never.** Cons: surprises whoever wanted the other.

## Decision

**Lanes.** Under each track's row stack every lane that track owns — its fader, pan, sends, its inserts'
parameters and its instrument's parameters — one fixed lane height (a theme token), plus a last row whose
chooser starts a lane for another target (the lane is created by its first point, as today). Bus targets
stay reachable through that chooser. Per-track visibility and height are view keys in the existing sidecar
record: `auto.<track id in 32 hex>` `\t` `<lane height px>`; the key's presence means shown; the height is
clamped to the lane's min..max tokens; unknown or malformed keys are ignored, as today.

**Modes.** Off, Read, Touch, Latch, **Write** — still one project-wide mode (per-lane modes deferred).
The stored integers are Read 0, Touch 1, Latch 2, Off 3 (today) and **Write 4**. Schema **v34**
widens the stored mode's CHECK from `mode <= 3` to `mode <= 4` and adds a separate one-row table
`automation_follow_clips (slot INTEGER PRIMARY KEY CHECK (slot = 1), enabled INTEGER NOT NULL CHECK
(enabled IN (0, 1)))` — the codebase's precedent for a project-wide scalar; a missing row means off.

**Rides (only while the transport plays; a press while stopped edits the control as usual).**
- **Touch** writes from the first touch to the release. **Latch** writes from the first touch to stop,
  holding the last value after the release. **Write** writes from play to stop: every target of the
  selected track that already has a lane (its current value, or its ride once touched) plus any target
  touched during the pass (a target never touched and never laned gets no lane). After a Write pass the
  mode returns to Touch (Pro Tools' default) — the mode change is part of the pass's own undo step.
- **What you hear.** ADR-0039's rule — "where a lane exists for a target, automation events win over manual
  scalar posts — read-mode semantics" — stays verbatim in Read and outside a ride. While a ride lasts, the
  riding control's live value wins for its one target, and the lane plays again when the ride ends. The
  mechanism is real-time safe and mirrors the mute mask (ADR-0016): the compiled graph carries an atomic
  bitset over its compiled automation targets; touch and release post "suspend target" / "resume target"
  through the existing ordered command lane; while a target's bit is set the lane evaluator emits no events
  for it and the live scalar set (`applySetGain` / `applySetPan` / `applySetFxParam`) accepts posts for it.
  No allocation, no lock; a rebuilt graph starts with no target suspended.
- **Span replacement with edge anchors.** A pass that wrote span [t0, t1] deletes the lane's points with
  t0 <= tick <= t1 and inserts the ride's points. If the lane had any point before t0 (resp. after t1), an
  anchor with the OLD lane's value at t0 - 1 (resp. t1 + 1) is inserted first, carrying the old left
  point's curve, so the curve outside the span keeps its values; for Linear and Hold segments it is
  exactly unchanged, and a Bezier or Log segment cut by an edge is re-shaped between the same values (an
  accepted approximation). An empty lane holds its first and last written values outside the span — the
  evaluator already holds a lane's first value before its first point and its last value after its last
  (`automationCurveProgress` clamps; pinned by the `[characterization]` test).
- **Loop and punch.** A loop wrap ends the current pass (it commits as written so far); a Latch or Write
  pass continues into the next cycle as a new pass. The punch region does not limit automation writing (it
  bounds audio and MIDI capture only).
- **Density and budget.** A ride samples at most once per 64-frame control interval (ADR-0039) and keeps a
  sample only when it differs from the last kept value by at least 1/512 of full scale or 250 ms have
  passed. A pass whose commit would exceed ADR-0039's compile-time event budget is refused whole with the
  reason on the status line — never silently thinned further or dropped.
- **Undo.** Each pass is one undo step (one transaction group over its lanes).

**Tools on a lane.** Pointer — click adds, drag moves, double-click deletes (today). **Pencil** — a
freehand drag replaces the swept span (edge anchors as above) with points, one per snap step, or by the
ride density rule with snap off. **Shift+Pencil** draws a **line**: the span between press and release
becomes a straight two-point ramp (Logic and Reaper precedent; no new toolbar tool). **Eraser** — a drag
deletes the points it sweeps. Each gesture is one undo step.

**Automation follows clips (when the setting is on).** Moving a clip in time on its own track moves the
points of that track's lanes that lie in the clip's span, by the clip's own time law: for a SampleLocked
clip (every UI-made clip) each point's tick becomes a frame, shifts by the clip's frame delta and becomes a
tick again, so the automation stays locked to the audio across tempo changes; for a TempoLocked clip each
point shifts by the clip's tick delta, so it stays locked to the music. Points already in the destination span (other than the moving ones) are
replaced, and both the vacated and the destination spans get edge anchors by the rule above. Points
belong to time, not to clips: an overlapping clip's points in the span move too. Copy, split, trim, delete
and moves to another track leave automation alone. The clip move and the lane edits are one undo step
(one transaction group across both families, as track removal already groups clip, lane and track edits).

**Targets.** Every role ADR-0039/0047 defines, instrument parameters included (persisted since v33).

**Time.** Breakpoints stay musical ticks; every UI path converts through the tempo map.

## Consequences

- **Positive:** reference-DAW automation editing and recording; repeated passes behave; you hear your
  rides; clip moves can carry their automation.
- **Negative / accepted costs:** Write and Latch can erase (undoable); a Bezier/Log segment cut by a pass
  edge is re-shaped; no per-lane modes, no automation across track moves, no trim or relative modes; the
  64-sample staircase of ADR-0039 stays; the plan's note that view state lives in a schema table is out of
  date (it is the sidecar record).
- **Follow-ups:** `CONTEXT.md` gains **Automation follows clips** and **Ride** (Touch / Latch / Write).
  Gates (`[automation-v2]` and render goldens): a Write pass's values play at the written frames; Touch and
  Latch span replacement with the curve outside the span bit-identical for Linear/Hold; a ride is heard
  while it lasts; the budget refusal; pencil, Shift+Pencil line and eraser spans; follow-clips moves (render
  before and after, edges preserved, one undo); stacked lanes' geometry and the view keys; v33 → v34
  migration; an automation journey in the session drives.
