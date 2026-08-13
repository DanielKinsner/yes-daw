# Editing-first parity backlog — multi-track editing, FX tools, hardening, recording (2026-08-12)

Carved per `docs/goals/2026-08-12-editing-first-run-brief.md` after a fresh adversarial re-audit of
current `main` (head `ffd8971`; four parallel code-path audits: timeline/clip editing, piano roll,
FX/mixer/automation, recording — the load-bearing findings are quoted in the item hints below).
The 2026-08-09 parity audit's P0/P1/P2 and backlogs A1–B41 are all landed and certified; this list
is what an adversarial read of the REAL gesture/dispatch code paths says is still missing or
dishonest against Pro Tools / Logic-class expectations.

## How to work this list (non-negotiable)

The full protocol lives in `docs/goals/2026-08-11-overnight-backlog-run-brief.md` and applies
verbatim: one item at a time, strict top-to-bottom order, audit-before-build, shipped-boundary
gates that fail before and pass after, full local ctest with the owner-file isolation ritual, one
feature commit + exact-head nine-job CI green + separate docs-only evidence commit per item, never
edit ADRs / goldens / `[[clang::nonblocking]]` annotations / `.github/workflows/ci.yml`, never
weaken or delete an existing gate (re-pinning legacy assertions to NEW semantics is allowed and
expected), stop after 3 consecutive red CI rounds on one item. Machine specifics and known traps:
same document. Items are numbered **E1–E35**; tick each here with the commit SHA + run id in the
docs-only evidence commit.

Additional standing rule for this run (from the 2026-08-12 brief): for UI work, build and launch
the real app, screenshot at real window sizes, judge the result yourself against Pro Tools /
Logic-class quality, iterate until it looks legit, THEN lock the fix with a mechanical
token/layout gate. Multi-track behavior must be gated on projects with 3+ tracks, not track 0.

## Phase 1 — multi-track editing tools

1. [x] **E1 — Three-track arrangement proof gates.** Landed in `e9f68e3` (exact-head run
   `31620834337`, nine jobs green). New shipped-boundary `[three-track]` gate (115 assertions) on a
   real 3-track project: import to the selected middle/third track at zero and at a located
   playhead, marquee spanning all three lanes, both-direction clamp of an all-lane group vertical
   drag, a two-clip group move THROUGH the middle lane preserving offsets with one-step undo, and
   project-wide copy/paste preserving per-clip track + relative time with audible playback change
   and bit-identical undo. Honest finding: the gate passed on its first run — no defect existed;
   the previously untested middle-lane clamp and offset-preservation laws are now pinned.
2. [x] **E2 — Group duplicate + group copy-drag.** Landed in `4ea1151` (exact-head run
   `31621801153`, nine jobs green — one Linux infra flake, sccache "socket hang up" before any
   compile, rerun green on the same head). Ctrl+D duplicates the WHOLE selection after its span
   through the shared clipboard paste; center Alt+drag on a selected member copies the WHOLE
   selection by the anchor's snapped delta via the new `copySelectedTimelineClipsTo` verb sharing
   the move gesture's lane/time clamp laws, one undo transaction, copies selected; single-clip
   laws and edge-fade Alt+drag unchanged. The `[group-duplicate]` gate failed before (4 clips,
   not 6) and passes after with 114 assertions on a 3-track project.
3. [x] **E3 — Tool palette does real work (timeline).** Landed in `e315dce` (exact-head run
   `31623384703`, nine jobs green — one Windows alpha-verify infra flake, sccache "socket hang
   up" before any compile, rerun green on the same head). Hand press-drags pan the viewport by
   the exact pixel→seconds law; Zoom clicks double / Alt+clicks halve the zoom through the
   existing anchored wheel math (new `timelineZoomToolClickFactor` token); Scissors clicks split
   the hit clip through the same persisted verb as `B`; Pencil clicks select a hit clip or create
   a snapped one-bar MIDI clip on the clicked lane through the new shared `addMidiClipOnTrackAt`
   verb (Ctrl+M law generalized); Pointer and the ruler keep their full historical behavior. The
   `[tool-palette]` gate failed before (Zoom click was a no-op) and passes after with 125
   assertions; the B22 `[tool-keys]` gate was re-pinned stronger to the new Pencil semantics.
4. [x] **E4 — Snap chooser consulted by every timeline time-gesture.** Landed in `6c133fe`
   (exact-head run `31624516457`, nine jobs green — one package-job infra flake, sccache "socket
   hang up" before any compile, twice; rerun green on the same head). Trim-left, trim-right,
   double-click split, scissors split, ruler loop drag, and ruler range drag now route through the
   shared `snappedTimelineTick` law with the gesture's Ctrl flag inverting the grid; verb legality
   (positive length, in-body split, source bounds) wins by honest refusal; fades stay honestly
   unsnapped. The `[snap-gestures]` gate failed before (a snapped loop drag persisted raw 9600
   instead of 0) and passes after; four legacy gates were re-pinned strictly stronger to the
   snapped semantics.
5. [x] **E5 — Vertical track scroll.** Landed in `7728f83` + real-red repair `dd1e32f`
   (exact-head run `31628338076`, nine jobs green). A shared whole-row offset scrolls the timeline
   lanes and the rail together; lanes stretch to fill until rows would fall below the fixed 36px
   `timelineCanvasLaneRowHeight`, then hold and scroll; plain wheel = vertical, Shift+wheel =
   horizontal, Ctrl+wheel = zoom on both surfaces; the clamp honors whichever surface overflows
   more with per-surface pinning. The `[vertical-scroll]` gate failed before (plain wheel left the
   row offset at 0 of 10) and passes after with 63 assertions on an 18-track project. The repair
   round was a REAL red: 36px rows pushed ~340 dense-fixture clips onto the antialiased
   gradient/rounded clip frame and blew the 16.6ms GPU budget on CI (sustained 33.66ms); a flat
   mid-tier clip frame below 48px (waveform kept) plus an empty-rect paint skip restored sustained
   to 8.80ms locally with `maxVisibleClips` 336 preserving the locked frame-verdict census.
6. [x] **E6 — Loop brace editing.** Landed across `05ff909` + `ad66a98` (exact-head run
   `31629082426`, nine jobs green). NOTE: a staging slip put the shell/gate files in the
   docs-labeled `05ff909` and the canvas scaffolding in `ad66a98` — the PAIR is the atomic E6
   change and `05ff909` does not build standalone (recorded honestly; pushed history is never
   rewritten). The transport loop paints as an accent brace band with end handles on the upper
   ruler from one shared `timelineLoopBraceRects` law; handle drags resize with the dragged edge
   snapped (Ctrl inverts) and the fixed edge exact, band drags move rigidly with the span
   preserved, Escape cancels, and commits ride `setPlaybackLoopRegion` (honestly transient). The
   `[loop-brace]` gate failed before (the end-handle drag fell through to a plain locate) and
   passes after with 58 assertions.
7. [x] **E7 — Marker move + rename.** Landed in `423f50a` (exact-head run green, nine jobs).
   Undoable `MoveMarker` (sorted re-insert) and `RenameMarker` engine verbs join the marker
   whole-vector diff family and the randomized property test; ruler marker labels drag-move
   through the shared `timelineMarkerLabelRect` law with E4 snapping (Ctrl inverts, Escape
   cancels, below-dead-zone release keeps the locate), and double-click opens the new inline
   rename editor (childCount bumped deliberately). The `[marker-edit]` gate failed before (the
   drag fell through to a locate) and passes after with 70 assertions.
8. [x] **E8 — MIDI clips are first-class timeline citizens.** Landed in `f4f7a31` (exact-head
   run green, nine jobs). New undoable engine verbs MoveMidiClip / MoveMidiClipToTrack /
   RemoveMidiClip (removal = the add diff-shape inverted) join the randomized property test;
   the timeline selection model is kind-aware through one `timelineEntityView` law
   (select/marquee/prune/group move/cross-track clamp/delete/nudge/duplicate/Alt-copy all handle
   both kinds in one transaction; duplicate now emits fresh ids directly, MIDI copies carry
   every note); MIDI clips paint on their lanes in the MIDI accent colour. Honest scope:
   trim/split refuse on MIDI; the Ctrl+C/X clipboard stays audio-only (cut with MIDI members is
   an honest no-op). The `[midi-clip]` gate failed before (a click selected nothing) and passes
   after with 104 assertions.
9. [x] **E9 — Piano roll follows the selected MIDI clip.** Landed in `6310154` + real-red repair
   `f2f4ce3` (exact-head run green, nine jobs). Double-clicking a timeline MIDI clip opens the
   roll on THAT clip through a consuming double-click seam (the audio split path no longer snaps
   the panel back); the roll header names the open clip's owning track; View Piano Roll retains
   the last opened clip. The `[roll-follow]` gate failed before and passes after (pencil notes
   land in exactly the targeted clip across three MIDI tracks). The repair round was the GPU
   frame gate on a slower CI runner (sustained 18.62ms): row-height clips now stride their
   waveform coarser and draw integer fills — local sustained 8.80ms → 3.65ms with the
   visible-clip census preserved.
10. [x] **E10 — Piano roll zoom, scroll, and all 128 keys.** Landed in `3aea76b` (exact-head run
    `31634369465`, nine jobs green). The surface snapshot carries a clamped roll viewport
    (viewLowKey / viewZoom / viewScrollTicks) driving one law for key rows, note paint, hit
    tests, the pencil, and drag deltas; the wheel map matches the timeline (plain = keys,
    Shift = time scroll, Ctrl = anchored time zoom, Alt keeps velocity); defaults reproduce the
    historical view so every legacy roll gate holds. The `[roll-viewport]` gate failed before and
    passes after with 44 assertions including real pencils landing persisted notes at key 0 and
    key 127.
11. [x] **E11 — Piano roll selection tools.** Landed in `f6518a4` (exact-head run green after a
    same-head rerun of the day's sccache-outage red on Linux alpha-verify). The empty roll grid
    is tool-aware (Pencil adds — every pencil gate re-pinned to press `P`; Pointer deselects and
    marquee-selects notes), Shift+click toggles via a movement-free-mouse-up law preserving the
    Shift+drag length edit, plain double-click deletes the note under the mouse, Escape cancels
    the note marquee, and tool selection became PANEL-PRESERVING (picking a tool no longer kicks
    the user out of the roll). The `[roll-select]` gate failed before (the pointer empty click
    pencilled) and passes after with 59 assertions.
12. [x] **E12 — Piano roll drag upgrades.** Landed in `6df8bdd` (run `31639813947` green for the
    full SHA across all nine jobs after spaced same-head reruns of the day's sccache-503 outage).
    A vertical drag transposes (a row is a semitone; pure pitch drags keep the start), a drag on
    a selected member moves the WHOLE selection by the anchor's snapped tick delta plus the key
    delta as one undo transaction (refused whole if any member leaves the clip window or the
    0-127 key range), the LEFT edge trims the note head with the end fixed, and move/resize/
    pencil follow the REAL snap chooser (Off = raw; the pencil floors to the grid). Flushed a
    real defect: the 8px edge zones swallowed narrow notes whole (unmovable); edge zones now
    need `pianoRollNoteEdgeMinGrabWidth` and narrow notes always MOVE. Two legacy roll gates
    re-pinned to chooser-Off for their raw pixel-exact drags. The `[roll-drag]` gate failed
    before (inert pitch drag) and passes after with 90 assertions.
13. [x] **E13 — Velocity lane editing.** Landed in `b80717c` (run `31642136695` green for the
    full SHA across all nine jobs, first try). A drag in the velocity lane paints velocities: x
    maps back to ticks with the grid law, y inverts the lane paint's value law (shared theme
    tokens), every note whose column overlaps the swept range takes the drag line's velocity at
    its own start tick (a real ramp, clamped at the segment ends), and a crossed selected note
    paints the WHOLE selection together — all through the new `paintPianoRollNoteVelocities`
    batch verb as ONE undo transaction (unknown notes or out-of-range velocities refuse the
    batch). Escape cancels; Alt+wheel is untouched. The `[roll-velocity]` gate failed before
    (inert lane drag) and passes after with 57 assertions.

## Phase 2 — FX tools

14. [x] **E14 — FX reorder from the UI.** Landed in `061f863` (run `31643567315` green for the
    full SHA across all nine jobs, first try). Every visible FX slot row carries `^`/`v` buttons
    moving the insert through the new `moveFxInsertOnSelectedStrip` verb — the first UI caller of
    the engine's `ReorderFxInsert` — under the new `MixerFxInsertReorder` action id. Up disabled
    on the first slot, down on the last; out-of-range refuses. Shell childCount re-pinned 93→103.
    The `[fx-reorder]` gate failed before (no up buttons) and passes after with 65 assertions:
    a genuinely non-commuting EQ(+24 dB band gain)+Limiter(-9 dBFS ceiling) chain, an AUDIBLY
    different render after the swap, params traveling with the inserts, and one undo restoring
    order and bit-identical audio in both directions.
15. [x] **E15 — Every FX param reachable, with the right control type.** Landed in `a33a9fc`
    (run `31644849599` green for the full SHA across all nine jobs after one spaced sccache
    rerun). `ParamSpec` gained additive choice metadata (`choiceCount`/`choiceNames` +
    `normalizedForChoice`); EQ band type and delay ping-pong declare their choices; the probe
    limit rose 32→96 so EQ bands 2–5 (ids up to 83) are reachable; params page through the new
    `mixer.fx.param.page` chooser; choice-shaped rows swap the raw slider for a real chooser
    persisting through the same `SetFxInsertParam` verb. Shell childCount re-pinned 103→112.
    The `[fx-params-all]` gate failed before (no pager) and passes after with 119 assertions:
    the FULL inventory of all five FxKinds walked page by page (EQ 24, Compressor 6, Delay 6,
    Reverb 5, Limiter 3), the EQ type chooser persisting HPF's exact 0.6, the band-5 gain
    (id 82) edited on page 3, ping-pong via its chooser, and three reverse-order undos.
16. [x] **E16 — Bus strips are real strips.** Landed in `3d4e987` (run `31647560865` green for
    the full SHA across all nine jobs after one same-head rerun of a macOS runner loss that died
    mid-build with zero compile output). New engine verb `SetBusMixScalars` mirrors the track
    twin (whole-vector bus diff family; `pick(22)` property arm). The strip click law routes
    strips past the tracks to `selectMixerBus`; bus fader/pan/mute/solo are UNDOABLE from day
    one (`editSelectedScalarStrip` routes by target kind — tracks keep the direct edit until
    E21); the FX chain works on the bus through the owner-aware plumbing; the control lane reads
    the SELECTED strip and follows its display ordinal; the send chooser refuses a bus target
    (honest scope: the engine has no bus-side send rows). The `[bus-strip]` gate failed before
    (bus unselectable) and passes after with 84 assertions.
17. [x] **E17 — Bus rename + remove from the UI.** Landed in `e9d1e88` (run `31651580430` green
    for the full SHA across all nine jobs, first try). New engine verb `RenameBus` (name rides
    the shared `trackName` array, empty names refused, whole-vector bus diff family, property
    arm). Double-clicking a bus strip opens the inline `shell.mixer.bus.rename` editor (Enter
    commits undoably, Escape cancels); the new `- Bus` button removes the SELECTED bus and the
    engine's routed-send refusal surfaces honestly — the click reports failure and the bus
    stays. Action ids `MixerBusRename`/`MixerBusRemove`; childCount re-pinned 112→114. The
    `[bus-rename-remove]` gate failed before (no rename editor) and passes after with 63
    assertions including the refused removal and the undo restoring the removed bus.
18. [x] **E18 — Send tap + destination editing.** Landed in `3115daf` (run `31652951911` green
    for the full SHA across all nine jobs, first try). New engine verb `SetSendTap` (track diff
    family, `pick(4)` property arm) — the first mutating verb for the tap column persisted
    since schema v9. Each send row grew a tap toggle naming the CURRENT tap and a destination
    chooser (disabled when there is nowhere else to route) re-routing as remove+add of the SAME
    send id in ONE undo group, preserving tap and level; routing to the current bus refuses.
    Action ids `MixerSendSetTap`/`MixerSendSetDestination`; childCount re-pinned 114→122. The
    `[send-tap-dest]` gate failed before (no tap toggle) and passes after with 67 assertions.
19. [x] **E19 — Master fader.** Landed in `5f7ea32` (run `31654737005` green for the full SHA
    across all nine jobs, first try). `masterLinearGain` (default 1.0) on Project with the
    additive v12 schema (`master_strip` single-row table, locate-points pattern — the row is
    written only off unity, so default projects round-trip byte-identically with legacy
    bundles). New verb `SetMasterGain` in its own tiny master diff family (`pick(23)` property
    arm). The projection applies the gain through a FaderNode before the master stage (four
    fader-count pins and both migration gates re-pinned; at unity the fader is a bit-exact
    passthrough so no golden moved). Interactive, undoable `mixer.master.fader` on the master
    column; childCount 122→123. The `[master-fader]` gate FAILS BEFORE AT THE COMPILER
    (`masterLinearGain` not a member pre-E19) and passes after with 41 assertions: persisted 0.5
    round trip, render peak halving EXACTLY, one undo restoring unity and bit-identical audio.
    Honest scope: gain only — no master FX chain, no master pan.
20. [x] **E20 — Automation targeting.** Landed in `b8050d4` (run `31656108777` green for the
    full SHA across all nine jobs, first try). The model's lane accessors generalized to
    (owner, role, paramId) with the create-on-demand one-undo-group law; the new
    `timeline.automation.target` chooser lists Fader, Pan, each send level, then EVERY FX param
    of every insert (owner = the INSERT id), and the canvas edits whatever it names; breakpoints
    land on the REAL snap chooser's grid (Off = raw). Legacy `[automation-canvas]` re-pinned
    chooser-Off. childCount 123→124. The `[automation-target]` gate failed before (no target
    chooser) and passes after with 66 assertions: an on-demand Pan lane AUDIBLY changing the
    render, an EQ band-gain lane owned by the insert audibly changing it again, the raw-tick
    law, and the one-undo-group law dropping the on-demand lane whole.

## Phase 3 — hardening + visual sweep of everything shipped

21. [x] **E21 — Undo covers direct strip edits.** Landed in `c9aac96` (run `31657624258` green
    for the full SHA across all nine jobs, first try). Every scalar strip edit rides an
    undoable verb (`SetTrackMixScalars` on the mixer lane AND the panel-preserving rail path,
    `SetBusMixScalars` for buses); the dead direct-edit helper removed. The undo stack grew safe
    scalar-gesture coalescing: consecutive strip-scalar entries merge into ONE step while a
    gesture is open (chain-checked; mute/solo flag equality keeps toggles separate), any
    non-coalescable verb AUTO-ENDS the gesture (the track-duplicate gate caught the
    group-swallowing hazard in the first design, which was replaced), and a new gesture SEALS
    the previous entry so two drags stay two steps. The `[strip-undo]` gate failed before (the
    old law ate the sentinel marker) and passes after with 95 assertions.
22. [x] **E22 — Bus meters live.** Landed in `8267576` (run `31658312970` green for the full
    SHA across all nine jobs, first try). `PlaybackEngine` harvests the per-bus MeterNode taps
    at create on the track-meter contract (an unrouted FX-less bus projects no meter node and
    honestly reads 0); `busMeterPeak` flows through the model; `busMeterHold` advances on the
    B32 tick law; bus strips paint held-peak/clip-latch instead of permanent zero; a bus meter
    click clears exactly its own hold. The `[bus-meter]` gate failed before (only the track's
    clip light existed) and passes after with 40 assertions.
23. [x] **E23 — Cross-strip/slot coverage gates.** Landed in `c3d0fce` (run `31659706970` green
    for the full SHA across all nine jobs, first try). The audit BIT: extending `[fxparam]` to
    a third track flushed a REAL CRASH — the control-lane refresh guarded the first-track FX
    bypass readout with track 0's chain but dereferenced the SELECTED strip's chain (an E16
    seam), segfaulting the shell when a chain-less non-zero strip was selected while track 0
    had FX. Fixed to the control's own first-track law. The painted-mixer selected highlight
    now keys on the E16 strip ordinal (bus strips finally highlight), pinned via the new
    `selectedMixerStripOrdinal` snapshot field. Gates EXTENDED rather than duplicated:
    `[fxparam]` non-zero strip + non-zero slot, `[sendsui]` third-track sends, `[inspector]`
    third-track clip with the first clip bit-identical.
24. [x] **E24 — Visual sweep: Timeline view.** Landed in `75be649` (run `31661114262` green for
    the full SHA across all nine jobs, first try). The real app WAS launched against a
    three-track bundle (owner file isolated + restored), but the desktop-screenshot permission
    dialog auto-denied with nobody at the keyboard — the sweep ran on the shipped-component
    render path (`paintEntireComponent` at real window sizes, same paint pipeline). New
    `[timeline-sizes]` gate renders a real three-track fixture at 1152×720/1536×960/1920×1080
    with size-relative coverage + inspector-bounds assertions. Judged and FIXED (re-verified in
    pixels): the illegible header tempo readout (L&F thumb over clipped digits → LinearBar is
    now a filled scrub cell drawing the whole value), the 1152×720 inspector bleeding over the
    bottom mixer panel (layout clamps every control to its column — GATED at every size), the
    fade rows' value/thumb collisions and double-printed Curve value (inset 78→150,
    label-only Curve), and a RAW ENGINE NODE ID in a shipped readout (humanized; gate re-pinned
    to reject "meter node"). Deferred honestly to E25: the mixer strip/lane geometry mismatch.
25. [ ] **E25 — Visual sweep: Mixer view.** Same protocol: strip widths/alignment, meter scales,
    FX/send row spacing, bus/master panes, dB readouts, overflow behavior with many strips.
26. [ ] **E26 — Visual sweep: Piano roll + automation lane.** Same protocol: key column, grid
    contrast, note/velocity readability, expression lanes, automation handles/curve legibility.
27. [ ] **E27 — Visual sweep: whole-shell resize + consistency.** Min/max window sizes, header
    row behavior under width pressure, menu/tooltip/theming consistency, token drift; lock with
    layout gates (the B41 SNAP-label gate is the model).

## Phase 4 — recording (last)

28. [ ] **E28 — Real devices honestly unlock Record.** Today the Record button unlocks ONLY after
    clicking "Test Device", which stamps a FAKE profile (`UiAppModel.h:4670-4686` is the only
    setter of `recordingDevice_.selected`); every real take then persists `deviceStableId = 1` (a
    provenance lie), and `startRealRecordingCapture` never checks the arm state (silent fallback to
    "Audio 1"). Populate `recordingDevice_` from the REAL desktop device (real input count, stable
    id, latencies) when it opens; enforce arm before capture; take provenance records the real
    device. CI via the injected device seams; the synthetic path remains only for
    harness/inputless devices.
29. [ ] **E29 — Input device + channel chooser.** The header chooser is output-only
    (`MainComponent.cpp:4462-4494`); input is whatever the OS default gives and `inputChannel` is
    hardcoded 0 (`UiAppModel.h:1858,4705`). Add an input device chooser and a channel picker
    (mono channel N or stereo pair) driving the JUCE setup input side and the capture config.
30. [ ] **E30 — Input metering + arm visibility.** No input level is computed or shown anywhere —
    you cannot see signal before recording. Live input peak meter while armed (through
    `processDeviceAudioBlock`), painted on the armed track's rail/strip meter plus a transport arm
    indicator; CI-deterministic via injected input blocks.
31. [ ] **E31 — Direct input monitoring.** The Monitor button cycles a metadata-only enum; nothing
    ever routes input to output (`UiAppModel.h:288-292`). Make DirectInput policy actually sum the
    armed input into the output RT-safely (no allocation/locks on the audio thread; RTSan-clean);
    Off is truly off. Honest scope: LatencyCompensated monitoring stays a no-op and says so.
32. [ ] **E32 — Loop recording.** The H5 primitive supports loop takes and is unit-tested
    (`recording_tests.cpp:240-295`) but `startRealRecordingCapture` never sets
    `window.loopEnabled`. Wire transport loop + record: each cycle commits a take
    (`maxLoopTakes` capped), placed identically; CI via the model-level capture API.
33. [ ] **E33 — Take management UI.** Takes exist engine-side with ordinals but there is no way to
    see, switch, or delete a take. Per-clip take chooser (list takes on the clip's track/window,
    switch the audible take, delete a take) — honest scope: no comping UI beyond the existing Comp
    button.
34. [ ] **E34 — MIDI recording.** The engine primitive `recordMidiEventsToTimeline`
    (`Recording.h:556-585`) is never called from production code and no MIDI input is ever opened.
    Open MIDI inputs, collect during a capture session, commit to a MIDI clip on the armed track at
    stop; CI with injected MIDI events through the model API.
35. [ ] **E35 — Shipped-path hardware record proof.** `verify-hardware.ps1` drives
    `RecordingCheckMain.cpp`'s OWN callback — the shipped Record path (`MainComponent.cpp:4584-4615`
    → `UiAppModel::startRealRecordingCapture/stopRealRecordingCaptureAndCommit`) has NO automated
    proof on real hardware. Add a one-command self-asserting PASS/FAIL checker that drives the SAME
    model verbs the Record button calls against the real device (loopback), plus a CI gate pinning
    the button→verb dispatch path. Never "listen and check."

## Explicitly out of scope — do not fake

- Multi-track (multi-arm) recording — the arm model is single-track by construction; honest
  single-arm stays.
- Latency measurement/calibration — driver-reported latency is trusted; no fake calibration.
- Punch-in/out UI, comping UI beyond the existing Comp button.
- Latency-compensated input monitoring (DirectInput only in E31).
- MIDI CC / sustain / pitch-bend / aftertouch recording or editing (no CC model exists).
- MIDI clip trim/split (note source-window semantics need a product decision first).
- Same-track clip overlap policy (today overlapping clips sum; changing that is a product
  decision — do not silently change it, do not gate-bless it as "designed" either).
- Reverse playback, time-stretch/elastic audio, tab-to-transient, ripple editing, track
  freeze/bounce-in-place, plugin hosting UI (H18), video.

If an item above collides with one of these, land the honest subset and say so in `STATUS.md`.

## Definition of done (whole list)

Every landed item: gate-covered at the shipped boundary (failing before, passing after; 3+ track
fixtures for multi-track behavior), suite green locally with the owner-file isolation ritual, one
feature commit, EXACT-HEAD GitHub Actions run green across all nine jobs, then a separate
docs-only evidence commit ticking the item here with the SHA + run id and adding the STATUS
certification paragraph. Visual-sweep items additionally require real-app screenshots judged
before the gate is written.
