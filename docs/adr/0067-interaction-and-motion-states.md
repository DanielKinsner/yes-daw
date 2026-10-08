# 0067. Interaction and motion states: hover and pressed on every control, meters that miss no block, a playhead from the published frame

- **Status:** Accepted (2026-10-07, after two separate agent critic rounds — their blockers resolved below — and an
  exhaustive interleaving check of the meter protocol: 2.6 million interleavings, no block lost, no false silence)
- **Date:** 2026-10-07
- **Deciders:** build agent (proposer), separate agent critics (two rounds), a model-checking agent
- **Related:** the plan's G6.4 ("Hover, pressed and logical-target states; playhead and meter ballistics from real state.
  Gate: state transitions and existing frame/action/audio budgets; no animation or timer may make static controls lie or
  interrupt playback.") and the G6 exit in [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md);
  ADR-0002 (the audio thread never allocates, locks, logs or does I/O — unchanged); ADR-0006 / ADR-0008 (a meter tap
  publishes with a single writer and no compare-exchange — kept); ADR-0049 (the Control target and its key priority —
  owner-settled, unchanged); ADR-0063 (tokens and contrast); ADR-0064 (the scaling matrix); ADR-0065 (scrolled-out strips
  have no parts); ADR-0066 (every painted control is a `PaintedControl` record and a Control target with its own
  accessible element — unchanged, one record added here); the B32 meter law (`UiTheme::Meter::peakHoldTicks`,
  `clipThreshold`).

## Context

**Pointer states.** Native buttons paint hover and pressed (`YesDawLookAndFeel::drawButtonBackground` with
`UiTheme::Tone::hoverHighlightAlpha` 0.10 and `pressedHighlightAlpha` 0.16); `drawComboBox` honours only a press; the
slider drawers neither; toggle buttons use JUCE's default drawer. The splitters paint hover. Nothing else does: the rail's
cells and knobs, the mixer strips' and the master's zones, the tool strip, the header's gear and time readout and the
Sampler's pads — every control ADR-0066 made a record — look the same under the pointer, pressed or not. Their surfaces'
`mouseMove` only publishes a status hint and a cursor. Canvas objects (clips, notes, breakpoints, markers, the ruler) are
not controls (ADR-0066) and keep their own cursors and hints; G6.4 does not tint them.

**The logical target** (the keyboard's Control target) already shows its state: the ring, drawn by the shell in
`paintOverChildren`, heavier while interacting (ADR-0066). It stays as it is.

**Meters.** Every peak the UI meters is the **last** block's: `MeterNode` stores "this Block's peak" per channel and
aggregate (`src/engine/nodes/MeterNode.h`); the device callback stores the master's left and right block peak
(`MainComponent::accountDeviceBlockPeaks`); the armed inputs' peaks are block peaks (`UiAppModel`'s `armedInputPeaks_`).
The UI reads them every 33 ms tick (`kUiRefreshIntervalMs`). A tick spans three 512-frame blocks at 48 kHz and twelve
128-frame ones, and the engine processes a device block as two segments at a loop wrap or while shuttling — so a meter
shows one block in several; a peak or a clip in the others never reaches it, and the clip latch can miss a clip.

Stopped with nothing live, the engine does not run the graph (`PlaybackEngine::processBlock`), so the taps hold the last
playing block; `updateTrackMeterHoldStates` hides that with `playing ? peak : 0`, which also blacks out a stopped strip
that **is** sounding (an audition tail, a held note: `processLiveOnlyBlock`). The master is read raw in four places — the
header meter (`MainComponentMixer.cpp`, the left channel only), the mixer's master pane (left and right) and two probe
accessors — with no hold or latch, and a device that stops calling back leaves it lit on a dead value. The armed inputs'
peaks have a second writer: the control thread stores 0 on un-arm and on audio close.

**The playhead** paints each tick from the published `playheadFrame` — in Arrange directly, in the piano roll through the
open clip's tick mapping (`playheadTickForClip`) — a pure function of that frame and the viewport. Nothing animates it;
that has only to stay true.

**Paint scope.** The ring already repaints by rectangle; the shell's own paint draws every region (header, rail,
inspector, dock) whatever the rectangle, so each such repaint re-runs drawing the clip throws away.

## Decision

### 1. Two channels, one meaning each

- **The ring** is the keyboard's Control target — unchanged (ADR-0066).
- **The tint** is the pointer's: a fill of white at `Tone::hoverHighlightAlpha` over the hovered control and a second at
  `Tone::pressedHighlightAlpha` over the pressed one — stacked when a control is both. The shell paints both in
  `paintOverChildren`, under the ring, over the record's bounds with the ring's corner radius. A control that is the
  keyboard target and under the pointer shows both. No new colour or motion token.
- **Paint scope.** The shell's paint and `paintOverChildren` skip every region whose bounds the clip does not reach, so a
  pointer repaint draws only the region it touches.

### 2. Hover is the painted record under the pointer, re-resolved every tick

- **Records are disjoint.** No two records' bounds intersect, at every size of the scaling matrix — a gate here, so a
  point names at most one record. Hover, the router, the accessible elements and the paint read the same bounds: the
  shell keeps the records' `(id, bounds, surface)` from the points where it already syncs the elements
  (`syncPaintedAccessibilityProxies`, at the end of `resized()` and `refreshActionState()`).
- **One input, one seam.** `MainComponent::pointerEvent (kind, shellPoint, component)` takes every pointer event — enter,
  move, drag, exit, down, up — with the position in shell coordinates and the component the event was for. A
  `PointerTracker` (a `juce::MouseListener` the shell owns, added with `addMouseListener (&tracker, true)`) feeds it from
  the real window; the gates feed it through `mainComponentPointer (shell, kind, point, component)`. The hovered record is
  the one whose bounds contain the position **and** whose surface is, or contains, that component — so a control under an
  overlay (the New Project dialog, the FX editor, the keymap editor) is never hovered. An exit from the component last
  under the pointer clears the position (the pointer left the window, or a popup took it).
- **Re-resolved every UI tick** from the last position and the current records — a stationary pointer over a strip that
  scrolled out, a track that was deleted or a dock that switched tabs hovers what is under it **now**, or nothing. Events
  re-resolve at once (B1); the tick is the safety net, never a clock for a visual.
- **Repaint is two rectangles:** the old and the new record's bounds, expanded by `L::controlTargetRingRepaintMargin`;
  never a surface, never a full invalidation.
- **Native widgets** take the same two alphas in the look-and-feel: buttons as today; toggle buttons, combo boxes and
  sliders gain the hover and pressed tints from the widget's own mouse-over and mouse-down state.

### 3. Pressed is the record that took the primary press, while that button is down

The record under a primary-button press is the pressed one until that button comes up (a right-click opens its menu and
paints no press). Every tick it is also dropped when the primary button is not down (`juce::ModifierKeys::currentModifiers`,
JUCE's public static, which the gates set) or its id is no longer a record — a lost capture, a deleted track or a closed
panel never leaves a control pressed. The press and its action are unchanged (the header's gear still acts on
mouse-down). The keyboard's interaction keeps its own sign, the ring's heavier stroke; it paints no press.

### 4. Pointer states never touch accessibility or the model

Hover and pressed change no element's role, name, value or state, invalidate no handler and create no element
(`controlTarget.paintedElementCreations` stays flat). They write no project state and no undo step.

### 5. Meters read every block since the last look

- **`engine::PeakSinceRead`** (pure C++, RT-annotated) is every metered peak's source: `MeterNode`'s aggregate and
  per-channel peaks, the master's left and right in the device callback, the armed inputs' peaks. It publishes the max
  |sample| of every block processed since the UI last read it.
- **The protocol: count-stamped windows.** Each atomic has exactly one writer. The block writer (the thread processing the
  block) owns `value` (the window's running max) and `count` (blocks so far, `uint64`); the reader (the UI tick) stores its
  progress in the source's `consumed` (`uint64`). The writer also keeps, privately, `blocks`, `windowStart`, `runningMax`
  and `recent[4]` (the last four blocks' peaks).
  - *Writer, per processed block with peak p:* `c = consumed.load (acquire)`; if `c != windowStart`, the reader has taken
    the window through block `c`: `windowStart = c`, and `runningMax` becomes the max of `recent` over blocks `c + 1 ..
    blocks` (0 when there are none) if there are at most four, else it stays — the whole old window carried over. Then
    `blocks += 1`, `recent[blocks % 4] = p`, `runningMax = max (runningMax, p)`, `value.store (runningMax, release)`,
    `count.store (blocks, release)`.
  - *Reader, once per tick:* `n = count.load (acquire)`; if `n == consumed.load (relaxed)` no block ran since the last
    read and the reading is **silence**; else the reading is `value.load (acquire)`, then `consumed.store (n, release)`.
  - **Never under-reads.** The reader names the last block it took; the writer's next window starts after it, and blocks
    that ran while the reader was between its loads and its store are carried in from `recent`. A reader that fell more
    than four blocks behind is given the whole old window again — an over-read for one tick, never a dropped block. Four is
    deliberate: it covers the reader's few-instruction gap at one compare per slot.
  - **One reader.** Each source is read in exactly one place, the tick's meter step; painters and the probe read the hold
    states it fills. The master's four direct reads (the header meter, the master pane, two probe accessors) move to the
    hold states; the header meter shows the louder of the two held channels. A source inventory gate finds no other read.
  - **One writer.** The control thread never writes a source: the stores of 0 on un-arm and on audio close go (an
    un-armed input runs no blocks and reads silence), and `MeterNode::reset()` — which runs on the audio thread between
    blocks — leaves the peak source alone (a reset is not a block). Offline render workers each own their graph's nodes
    (`GraphScheduler`), so a source still has one writer there; a graph rebuilt from scratch starts its sources at zero.
  - The audio thread's added work per source per block: one acquire load, at most four float compares, two release stores.
    No compare-exchange, no read-modify-write, no allocation, lock, log or I/O (ADR-0002, ADR-0006).
- **No processed block reads silence.** Stopped with nothing live, a stalled or closed device, an un-armed input — the
  source stops counting and the meter falls on the B32 hold law. The `playing ? peak : 0` stand-ins go: a stopped strip
  that is sounding — an audition tail, a held note — meters it.
- **The master joins the law:** `masterMeterHold[2]` on the same `advanceMeterHold` (hold `Meter::peakHoldTicks`, latch at
  `Meter::clipThreshold`). Its clip indicator becomes a record, `mixer.master.meter` — a button ("Master clip indicator")
  whose activation clears the latch — so it is clickable, hoverable, keyboard-reachable and in the accessibility tree like
  every strip's (ADR-0066).
- Unchanged: the B32 hold and latch numbers, RMS (the last block's — nothing meters it as a peak), the gain-reduction
  meter's own ballistic, the loudness readouts (ADR-0028 / ADR-0053).

### 6. The playhead is the published frame, nothing else

The Arrange and piano-roll playheads paint each tick at the x of the published `playheadFrame` in the current viewport
(the piano roll through the open clip's tick mapping) — no interpolation, extrapolation or easing. While the published
frame does not change (stopped, paused), the painted playhead does not move. A smoother playhead (a device-time-stamped
frame and its clamp law) would be its own ADR.

### 7. No timers, no animation

The UI tick (`kUiRefreshIntervalMs`) and the timeline's auto-scroll timer stay the only timers in `src/ui/`; G6.4 adds
none, no `VBlankAttachment`, no fade, no tween. A static control repaints only when its state changes.

## Consequences

- Every operable control answers the pointer the way a DAW's does — a hover shade, a deeper shade while pressed — and can
  never keep a shade it no longer has: the tick re-resolves hover and drops a press with no button down.
- Meters stop under-reading: a clip in any block latches; a stopped transport, a dead device or an un-armed input reads
  silence because no block ran, not because a flag says so; a sounding stopped strip shows its signal. The master gets
  the strips' hold and latch and a clip indicator that is a real control. The cost is a few atomic operations per meter
  source per block.
- A pointer repaint draws one region of the shell, not all of them.
- The header's master meter shows the louder channel: a clip on the right alone, missed today, now lights it.
- Tests that relied on "the last block's peak" (a meter read twice in a tick, a harness that wrote one value and read it
  repeatedly, a control-thread store of 0) are re-grounded on "every block since the last read".
- CONTEXT.md gains **Hovered control**, **Pressed control** and **Peak since read**.

## Gates

Every gate fails on a build without its law; thresholds are computed from the tokens at test time, never retyped.

- **`[g6-motion][peak-since-read]`** (engine; runs in the RTSan and TSan jobs with the engine suites): blocks with peaks
  0.2, 0.9, 0.1 then one read gives 0.9; a read with no block since gives 0; with the writer forced (by a latch) to run
  blocks between the reader's loads and its store, every forced block is in the next reading; a reader five blocks
  behind still reads at least the max; a fresh source reads 0; `MeterNode` — three processed blocks, one read, the max of
  the three (red today: the last block's); `MeterNode::reset()` between blocks leaves the reading intact.
- **`[g6-motion][meters]`** (shell): a strip, a bus, the master and an armed input each latch a clip that sits in a
  block other than a tick's last; a stopped transport whose graph does not run reads silence after one tick though the
  tap's last value was loud; a stopped strip sounding an audition tail (the audition tests' live-note path) reads its
  signal; an input un-armed after a loud block and re-armed on silence reads silence; a device that stops calling back
  drops the master to silence on the hold law and keeps its latch; the master's hold and latch follow the strip law and
  `mixer.master.meter`'s activation clears it; a source inventory finds no read of a meter source outside the tick's meter
  step.
- **`[g6-motion][records-disjoint]`**: at every size of the scaling matrix, with a tall dock and every surface showing, no
  two records' bounds intersect.
- **`[g6-motion][hover]`** (through `mainComponentPointer`): one record of every family (the rail row's cells, knobs,
  slider, swatch and meter; a strip's cells, knobs, fader, send, insert and I/O row; a master slot and the master clip
  indicator; a tool cell; the header's gear and time; a pad) — a move to its centre names it in `pointer.hovered`, and the
  software render inside its bounds is the base composited with the hover tint (± 2 per channel), unchanged outside its
  expanded bounds; under an open overlay nothing is hovered; an exit clears it; text under the hover tint and under both
  tints keeps 4.5:1 (ADR-0063's pairs, extended).
- **`[g6-motion][hover-follows-state]`**: a pointer left on a strip that scrolls out, a track that is deleted and a dock
  that switches tabs — after one tick `pointer.hovered` names what is under it now (or nothing) and the old rect was
  repainted.
- **`[g6-motion][pressed]`**: a primary press names `pointer.pressed` and stacks both tints; a release clears it; a
  right-click sets none; with `ModifierKeys::currentModifiers` showing no button down, or the record gone, the next tick
  clears it; a keyboard interaction never sets it.
- **`[g6-motion][repaint-scope]`**: every pointer transition repaints exactly the old and new expanded rects
  (`pointer.lastRepaint`), adds no full invalidation, and the shell's paint draws only the regions the rects touch
  (`frame.regionsPainted`).
- **`[g6-motion][a11y-steady]`**: fifty hover and press transitions over every family leave `paintedElementCreations`
  and every element's description unchanged.
- **`[g6-motion][widget-states]`**: a button and a toggle button set to their over and down states paint the hover and
  pressed tints through the look-and-feel and none in their normal state. (Combo boxes and sliders take JUCE's own
  mouse-over, which a headless render cannot set: the drive below measures them.)
- **`[g6-motion][playhead]`**: renders during playback at several published frames put the playhead's column at the
  viewport x of that frame (± 1 px) in Arrange and in the piano roll; with the transport stopped, sixty ticks render
  identical frames.
- **`[g6-motion][static]`**: a fresh project that has never sounded, transport stopped, no pointer — two renders a tick
  apart are identical at the three plan sizes; and after any sound, once every hold has run out, the same; a source
  inventory of `startTimer`, `startTimerHz` and `VBlankAttachment` under `src/ui/` matches the two timers above exactly.
- **Real app (the drives):** `ss7` sweeps the real pointer over every strip and rail control and the master while the song
  plays, asserting `pointer.hovered` at each, the B1 / B2 / B5 frame and audio budgets and no full invalidation; it hovers
  a combo box and a slider and measures the tint in the window's pixels (the drive's shots, compared in place); shots of
  hover, pressed and ring together go to the rubric. The earlier drives stay green.

## Checkpoints

- **cp1 — Meters from real state.** `PeakSinceRead`; `MeterNode`, the master and the inputs on it; one read per tick and
  the four master reads moved; the control-thread stores gone; silence when no block ran; the master's hold, latch and
  clip-indicator record; `meter_tests`' "reset clears the published metrics" becomes "reset leaves the reading intact".
  Gates `[peak-since-read]`, `[meters]`.
- **cp2 — Hover and pressed.** Clip-aware shell paint; `records-disjoint`; the record cache, `pointerEvent` and
  `PointerTracker`; the tints; the tick re-resolution; the widgets' states; the probe's `pointer.{hovered, pressed,
  lastRepaint}` and `frame.regionsPainted`. Gates `[records-disjoint]`, `[hover]`, `[hover-follows-state]`, `[pressed]`,
  `[repaint-scope]`, `[a11y-steady]`, `[widget-states]`.
- **cp3 — The playhead pinned, nothing animates, the real-app sweep.** Gates `[playhead]`, `[static]`, the `ss7` sweep.

## Alternatives rejected

- **Fades and eases for hover and press.** A clock for a visual: a busy message thread paints a half-faded state that is
  neither true nor false, and the gate forbids timers that make static controls lie.
- **Per-surface hover fields resolved by each surface's own hit-test.** Five geometry laws where ADR-0066 built one; the
  records already carry every control's bounds.
- **Pressed derived from each surface's drag state.** Five gesture brackets, and the controls with no drag (cells, pads,
  the gear) would need new flags anyway; "the record that took the press, while its button is down" is one rule with no
  leak.
- **A tint layer as a separate child component.** A transparent child's repaint still repaints the shell beneath it;
  making the shell's paint clip-aware is what bounds the cost.
- **Dropping `playing ? peak : 0` alone.** The taps hold the last playing block when the graph stops, so meters would
  freeze lit — a worse lie. Silence must come from "no block ran", which only the source knows.
- **Clearing the peak on read with an exchange from the UI.** A second writer on the value races the audio thread's store
  and loses blocks; ADR-0006 keeps one writer.
- **Smoothing the playhead between published frames.** It paints a position the transport never reported — at a loop
  wrap or a stop, one it never will.
