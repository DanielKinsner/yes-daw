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
2. [ ] **E2 — Group duplicate + group copy-drag.** `duplicateSelectedTimelineClip`
   (`UiAppModel.h:3540-3560`) duplicates only a single clip; Alt+drag copy (A7) copies only the
   dragged clip. Make Ctrl+D and center Alt+drag act on the WHOLE selection: fresh-ID copies
   preserving relative time and track offsets, one undo transaction, selection moves to the copies.
   Single-clip behavior and edge Alt+drag fades stay unchanged.
3. [ ] **E3 — Tool palette does real work (timeline).** The tool state exists but only the marquee
   branch reads it (`MainComponent.cpp:538-539`); Scissors/Pencil/Hand/Zoom change nothing.
   Wire honest behaviors: Scissors click splits the hit clip at the snapped click tick (same law as
   `B`); Pencil click on an empty lane creates a snapped one-bar MIDI clip on THAT lane at the click
   tick (engine `addMidiClip` exists); Hand drag scrolls the viewport; Zoom click zooms in around
   the click, Alt+click zooms out (same viewport math as Ctrl+wheel); Pointer unchanged.
4. [ ] **E4 — Snap chooser consulted by every timeline time-gesture.** Only clip move/cross-track
   move/copy-drag snap today (`MainComponent.cpp:6073/6112/6139`); trim-left
   (`MainComponent.cpp:2104-2114`), trim-right (`:6158-6169`), double-click split (`:6145-6156`),
   ruler loop drag (`:2143-2152`), and ruler range drag (`:2161-2170`) all ignore the snap chooser.
   Route every one of them through `snappedTimelineTick` (Ctrl still inverts; edge legality — zero
   length, source-window bounds — wins over the grid). Fades honestly stay unsnapped — they are
   durations, not grid positions; say so in the gate.
5. [ ] **E5 — Vertical track scroll.** No vertical scroll exists anywhere; lanes shrink to the 8px
   floor and clip (`TimelineCanvas.h:522-546`, `UiTheme.h:717`), so big projects are unusable.
   Adopt the standard mapping: plain wheel scrolls vertically (rail + timeline share one row
   offset), Shift+wheel scrolls horizontally, Ctrl+wheel keeps zoom — re-pin the existing wheel
   gates to the new semantics. Lanes get a sane minimum height; a 12-track project must reach and
   edit its last track through the real viewport.
6. [ ] **E6 — Loop brace editing.** The loop region can only be replaced wholesale by a new
   Shift+drag; it is not painted as a draggable overlay and has no handles
   (`MainComponent.cpp:518-524, 613-628`). Paint a real loop brace on the ruler; drag either end to
   resize, drag the middle to move, all snap-aware and persisted through the existing
   `setPlaybackLoopRegion` path.
7. [ ] **E7 — Marker move + rename.** Markers persist a name (`Project.h:451-455`) but there is no
   MoveMarker/RenameMarker verb, no drag, no rename UI, no way to fix a misplaced marker short of
   remove+re-add. Add both verbs (randomized property test included), drag-move on the ruler
   (snap-aware), and double-click inline rename on the marker label.
8. [ ] **E8 — MIDI clips are first-class timeline citizens.** MIDI clips are never drawn on the
   timeline, cannot be hit-tested, selected, moved, or deleted; the ONLY MIDI-clip verb in the
   entire engine is `addMidiClip` (`ProjectUndo.h:609`) — no move/remove exists anywhere. Add
   engine verbs `MoveMidiClip`, `MoveMidiClipToTrack`, `RemoveMidiClip` (randomized property test
   per the AddNote pattern), paint MIDI clips on their track lanes, and wire hit-test / selection
   (joining the multi-selection) / snapped move / cross-track move / delete / duplicate through
   the same gesture paths audio clips use, one undo per gesture. Honest scope: MIDI clip
   trim/split stays out (source-window semantics for notes need a decision — see out-of-scope).
9. [ ] **E9 — Piano roll follows the selected MIDI clip.** The roll always shows
   `midiClips.front()` (`MainComponent.cpp:6945-6956`); with several MIDI tracks there is NO way
   to open the second clip. Double-click a timeline MIDI clip (E8) opens the piano roll on THAT
   clip; the roll header shows which clip/track is open; switching works across 3+ MIDI tracks.
10. [ ] **E10 — Piano roll zoom, scroll, and all 128 keys.** The roll is a hardwired 25-key
    (C3–C5) window with the clip stretched edge-to-edge (`UiTheme.h:667-669`,
    `MainComponent.cpp:1028-1036`); notes outside are invisible and uneditable. Add vertical pitch
    scroll (wheel) and horizontal zoom/scroll (Ctrl+wheel / Shift+wheel, same laws as the
    timeline); every key 0–127 reachable; keyboard column paints correctly at any scroll.
11. [ ] **E11 — Piano roll selection tools.** Only Ctrl+A exists; a click on empty grid PENCILS A
    NOTE unconditionally (`MainComponent.cpp:1115-1137`) so you cannot even deselect. Make the
    gesture map tool-aware: Pointer click on empty grid deselects, Pointer drag marquee-selects
    notes, Shift+click toggles a note in/out of the selection, Pencil tool click adds (the current
    behavior, moved behind the Pencil tool), double-click a note deletes it (mouse delete).
12. [ ] **E12 — Piano roll drag upgrades.** Note drag ignores deltaY (no pitch drag —
    `MainComponent.cpp:1176`), collapses multi-selections to one note (`UiAppModel.h:1420-1421`),
    only the right edge resizes (`:1288-1304`), and move/resize ignore snap entirely. Add vertical
    pitch drag, group drag of the whole selection (time+pitch, one undo transaction, refusal if any
    note would leave the clip), left-edge trim, and route move/resize/add through the REAL snap
    chooser grid instead of the hard-coded `kPianoRollSnapGridTicks`.
13. [ ] **E13 — Velocity lane editing.** The velocity expression lane is read-only paint
    (`MainComponent.cpp:6322-6372`); the only velocity edit is single-note Alt+wheel. Dragging in
    the velocity lane sets the velocity of the note(s) whose columns the drag crosses (multi-note
    selection edits together), persisted and undoable through `SetNoteVelocity`.

## Phase 2 — FX tools

14. [ ] **E14 — FX reorder from the UI.** `ReorderFxInsert` is fully wired engine-side
    (`ProjectUndo.h:36`, `Project.h:2725`) with ZERO UI callers. Add per-slot up/down controls;
    the gate proves persisted order and an audibly different render for a non-commuting chain.
15. [ ] **E15 — Every FX param reachable, with the right control type.** The param panel caps at 8
    sliders probing IDs 0–31 (`UiTheme.h:542-543`), so EQ bands 2–5 (16 of 24 params, IDs up to 83)
    are UNREACHABLE (`EqNode.h:35,183-186`); enum params (EQ band type, delay ping-pong) render as
    raw linear sliders. Page or scroll the param list so every `ParamSpec` of every FxKind is
    editable, and give choice-shaped params real choosers. Gate walks every FxKind × every param.
16. [ ] **E16 — Bus strips are real strips.** Bus strips cannot even be selected
    (`MainComponent.cpp:3284-3294` drops clicks past trackCount); `selectedMixerStrip`'s Bus branch
    is dead code from the shell. Make bus strips selectable; fader/pan/mute/solo and FX
    add/remove/bypass/param work on the selected bus through undoable verbs (new
    `SetBusMixScalars` mirroring `SetTrackMixScalars` where needed). Honest scope: buses cannot
    originate sends (engine has no bus-side send rows) — say so.
17. [ ] **E17 — Bus rename + remove from the UI.** No `RenameBus` verb exists anywhere; `RemoveBus`
    exists engine-side with no UI caller. Add RenameBus (property-tested), inline rename on the bus
    strip, and a remove control that honestly surfaces the routed-send refusal.
18. [ ] **E18 — Send tap + destination editing.** Tap is hardcoded PostFader at creation
    (`UiAppModel.h:2055`) with no engine verb to change it (the `tap` column IS already persisted in
    schema v9); destination is never re-editable. Add `SetSendTap` (property-tested) + a per-row
    pre/post toggle, and a destination chooser that re-routes as remove+add in one undo group.
19. [ ] **E19 — Master fader.** The master strip is decorative: no persisted master gain exists on
    `engine::Project` at all. Additive schema bump (v11 → v12, locate-points pattern): persisted
    master strip gain; projection applies it at the master stage; interactive, undoable master
    fader on the master pane. Honest scope: master gain only — no master FX chain, no master pan.
20. [ ] **E20 — Automation targeting.** The canvas hard-targets the selected track's FADER lane
    only (`MainComponent.cpp:6010-6020`) even though the engine compiles and plays back all six
    roles (`Project.h:583-598`, `CompiledGraph.h:340-431`). Add a lane-target chooser (track
    fader / pan / each send level / each FX param) creating lanes on demand; breakpoint drags snap
    through the snap chooser (today they emit raw pixel ticks — `MainComponent.cpp:2402-2413`);
    the gate proves audible playback follows a pan lane and an FX-param lane, not just fader.

## Phase 3 — hardening + visual sweep of everything shipped

21. [ ] **E21 — Undo covers direct strip edits.** THE correctness gap: mixer/rail fader, pan, mute,
    solo all route through `editSelectedMixerStrip` / `editTrackStripPanelPreserving`
    (`UiAppModel.h:4839-4894`) which mutate the project WITHOUT pushing an undo transaction —
    Ctrl+Z after a fader drag silently reverts some OTHER earlier edit. Route every scalar strip
    edit through the existing `SetTrackMixScalars` verb (and E16's bus twin), gesture-grouped so
    one drag = one undo step (fine drag and Alt+reset included). Gate pins the Ctrl+Z law for
    every control.
22. [ ] **E22 — Bus meters live.** Engine builds per-bus MeterNodes (`MixerGraphProjection.h:575-586`)
    but the shell never harvests them — bus strips paint permanent zero
    (`UiMixerSurface.h:400`, `MainComponent.cpp:6702-6703`). Surface bus peaks exactly like track
    peaks (B32 pattern: peak-hold, clip-latch, click-clear).
23. [ ] **E23 — Cross-strip/slot coverage gates.** Audit-first: prove (and fix where broken) that
    every per-strip/per-slot control works beyond index 0 — FX param editing on a non-zero strip
    and a non-zero slot, sends on the third track, inspector on a clip on the third track,
    Alt+reset / fine-drag on non-zero strips. Extend existing gates to 3-track / multi-slot
    fixtures rather than adding parallel ones.
24. [ ] **E24 — Visual sweep: Timeline view.** Launch the real app; screenshot at ≥3 real window
    sizes (small laptop, default, large); judge against Pro Tools/Logic-class quality — header
    crowding, ruler/marker/brace legibility, lane labels, clip name/fade/gain readouts, tool
    palette states; fix what reads amateur; lock every fix with token/layout gates.
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
