# Reality run backlog — R1–R34 (2026-08-25)

Carved from four parallel adversarial code-path audits of current `main` (head `71827dc`), run
2026-08-25 under the pre-authorized fallback in STATUS.md ("If he's unavailable, a fresh
adversarial audit carve instead" — Dan confirmed he cannot dogfood-test right now and ordered
the parked edge-zone finding to the top). Every finding below was verified against the actual
code with file:line evidence, not recycled from older carves.

Theme: **the app must survive a real song and a real user.** The model layer is genuinely
honest (typed refusals, near-complete undo verbs, real persistence). The dishonesty has
migrated to the shell boundary (results `(void)`-discarded, silent failure paths) and to two
scope decisions the gates document rather than challenge (every edit resets the transport;
fixed 48 kHz with no rate guard).

## How to work this list

The protocol is the 2026-08-11 brief's loop, unchanged, as chained by the 2026-08-12 brief:
one item at a time, strict top-to-bottom order, audit-before-build, shipped-boundary gates
that fail before and pass after, full local ctest with the owner-file isolation ritual, one
feature commit + exact-head nine-job CI green + separate docs-only evidence commit per item,
never edit ADRs / goldens / `[[clang::nonblocking]]` / `.github/workflows/ci.yml`, never
weaken or delete a gate, stop after 3 consecutive red CI rounds on one item. Machine
specifics and known traps: `docs/goals/2026-08-11-overnight-backlog-run-brief.md`. The run
brief for this backlog is `docs/goals/2026-08-25-reality-run-brief.md`.

New engine-visible persisted state (R3, R33 if persisted) needs an additive schema bump +
migration gate, following the locate-points pattern.

## Verified as NOT gaps — do not re-carve, do not re-fix

- All seven 2026-08-12 mixer findings are fixed and gated: EQ bands reachable via param pages
  (`UiTheme.h:632`, gate `ui_input_tests.cpp:4454`), bus strips selectable (:4577), bus
  rename/remove UI (:4683), send tap/destination/level/remove rows (:4754), master fader
  undoable (:4829), mixer/rail/inspector edits ride undoable verbs with drag grouping
  (`UiAppModel.h:6934`), live bus meters with peak-hold + clip latch (:13199). FX reorder
  exists (gate :4353).
- Editing: crossfade (`X`), clip gain (three paths), select-all-on-track, marker
  drag/rename/Alt-delete, loop brace drag/move, playhead follow, group-aware keyboard clip
  nudge, cross-track group drag with clamping, track height resize, and strong piano-roll
  parity (shared tool palette, marquee, velocity lane, both-edge resize, scroll/zoom,
  copy-drag).
- Recording: real device adoption gates Record honestly, input device/channel choosers exist,
  monitoring is audible (DirectInput and LatencyCompensated), live input meters pre-roll,
  loop/punch/count-in capture, take chooser + delete, MIDI note capture from real inputs.
- No shipped-lie TODO/FIXMEs remain in `src/`; no duplicate key chords (all 141 descriptors
  checked).

## Phase 1 — first-session traps (R1–R9)

Near-certain hits the moment a real song replaces the fixture-shaped happy path.

- [x] **R1 — narrow clips must stay movable (edge-zone cap).** *(certified: feature `69c4ade`,
  nine-job CI run `32869090850` green first try, local 355/355)* Dan-ordered top item; the
  parked HONEST FINDING from the dogfood-readiness walk. `dragModeForPointer`
  (`src/ui/MainComponent.cpp:1277-1323`) applies ±8 px trim zones
  (`timelineClipEdgeHitWidth = 8`, `UiTheme.h:710`) unconditionally, so a clip painted under
  ~32 px has no Move body (right edge wins first); the 0.085 s fixture paints ~9 px at fit
  view — 100 % edge zone. The proven fix pattern already exists: the piano roll's E12 guard
  (`MainComponent.cpp:2010-2023`, `pianoRollNoteEdgeMinGrabWidth = 24`, `UiTheme.h:827`).
  Clone it for the timeline (guard trim AND Alt-fade zones on painted width
  `clipRightX - clipLeftX`). Gate to clone: `ui_input_tests.cpp:9738-9763` (narrow note must
  MOVE, never resize). The `[dogfood-readiness]` zoom-in workaround at
  `ui_input_tests.cpp:6887-6899` stays valid. Out of scope: changing the 8 px width for wide
  clips.
- [x] **R2 — the transport survives edits.** *(certified: feature `e7efec6`, nine-job CI run
  `32871156908` green first try, local 355/355)* The single biggest workflow trap, found
  independently by two audits. Every mutation — fader tick, mute, clip move, undo/redo —
  funnels through `adoptEditedProject` (`UiAppModel.h:7150-7195`) →
  `resetContextForFreshPlayback()`: `isPlaying=false, loopEnabled=false, playheadFrame=0`.
  "Loop a chorus and tweak while it plays" is impossible. The rebuild-free path
  (`canAdoptEditWithoutPlaybackRebuild`, :7197) fires only for zero-clip projects. Honest
  minimal slice: capture playhead/isPlaying/loop before the engine swap, seek + re-arm the
  new engine after — a brief audible seam per edit is acceptable and honest at this slice
  (R12 owns seamlessness). Gate: playing + looped project, apply a mute toggle and a clip
  move, assert transport still playing, playhead ≥ pre-edit position, loop region intact.
  Re-pin the gates that document the reset (e.g. `ui_input_tests.cpp:10786`'s in-comment
  admission) to the new law. Out of scope: parameter changes without engine rebuild (R12).
- [x] **R3 — the loop region is project state.** *(certified: feature `b2ddb1a`, nine-job CI
  run `32873671818` green first try, local 356/356)* The loop lives only in the replaced
  `PlaybackEngine` — no loop field in `src/engine/Project.h` or `ProjectBundle.h` — so it
  dies on save/reopen (and, pre-R2, on every edit). Persist loop enabled + range in Project
  with an additive schema bump + migration gate (locate-points pattern); reopen restores it.
  Out of scope: multiple loop/cycle memories.
- [x] **R4 — a shared status surface; the shell stops discarding results.** *(certified:
  feature `73a5d1f`, nine-job CI run `32876060409` green first try, local 356/356)* The model layer
  returns typed statuses and refusal strings; the shell `(void)`-discards every one: save
  (`MainComponent.cpp:7423`, dispatch path `UiAppModel.h:5743-5745`), create (:7372), export
  (:7393), autosave tick (:4051), `audioDeviceError` (:4133-4136), failed startup device
  open (:3982-3993, app left soundless wordlessly). Disk-full save looks identical to
  success. Ship one status/toast line (timeline-header strip or toolbar slot) fed by every
  currently-discarded result, painted from real model state with a deterministic
  service-timer decay. Gate: inject a failing save/export via the harness, assert the
  painted message; assert success paths stay quiet. Out of scope: a notification center /
  log window.
- [x] **R5 — a missing or corrupt asset on open is reported, never silent.** *(certified:
  feature `ad4015d`, nine-job CI run `32877783205` green first try, local 356/356)*
  `openProjectBundleAtPath` (`MainComponent.cpp:6386-6395`) and the startup auto-open
  (:3968-3978) skip BOTH branches when `decodeStoredProjectAssets` returns `nullopt`
  (any missing/mismatched asset, :372-400) — the app sits empty with no explanation, on the
  exact reopen step of the scripted first session. Minimal honest slice: a message box (or
  R4 status line) naming the bundle and the first missing asset path; the project still
  refuses to half-open. Gate: open a bundle with a deleted asset file, assert the painted
  report and the unchanged shell state. Out of scope: a relink/ignore chooser (future carve;
  do not fake one).
- [x] **R6 — import refusals are painted; `droppedFileRefusals` stops being dead state.**
  *(certified: feature `048cbd2`, nine-job CI run `32879354167` green first try, local
  356/356)*
  Ctrl+I discards decode refusals (`MainComponent.cpp:7397-7416`); the drag-drop path
  collects failures into `droppedFileRefusals` (:3094-3141, :10842) under a comment claiming
  they are "reported" — the list is appended and never read, painted, tested, or cleared (3
  repo references, all writes). A dropped MP3/FLAC does nothing wordlessly. Paint refusals
  through R4's surface ("WAV only, stereo max: <name>"), clear per drop. Gate: drop a
  non-WAV via the harness, assert the message; drop a valid WAV, assert none. Out of scope:
  decoding new formats.
- [x] **R7 — sample-rate honesty: refuse what would play at the wrong speed.** *(certified:
  feature `8c2e751`, nine-job CI run `32881100596` green first try, local 356/356)* Projects are
  fixed 48 kHz (`UiAppModel.h:1144`) and there is deliberately no resampler (:1338), but
  `addAudioAssetClipFromSource` (:1766+) never compares `decoded.sampleRate` to the project
  rate — a 44.1 kHz stem imports silently and plays ~9 % fast; likewise the device side
  (`MainComponent.cpp:3982`, `UiAppModel.h:394`) pulls frames 1:1 at whatever rate the
  Windows device opened, playing everything slow/flat with no warning. Refuse mismatched
  imports with a painted reason (R4), and paint a persistent warning while the device rate
  ≠ 48 kHz. Gate: import a 44.1 k fixture → refused with message, project untouched;
  harness device at 44.1 k → warning painted. Out of scope: actual sample-rate conversion
  (its own future slice; never a silent hack — `UiAppModel.h:1338`'s law stands).
- [x] **R8 — import is undoable.** *(certified: feature `951c018`, nine-job CI run
  `32883228997` green first try, local 356/356)* `addAudioAssetClipFromSource` (`UiAppModel.h:1804-1886`)
  and the record commit (:1614-1629) bypass `undo_.apply` (the dead `context_.canUndo=false`
  lines are recomputed away by `syncProjectEditContext`, :7094) — Ctrl+Z after an import
  leaves the clip and reverts the user's PREVIOUS edit instead. These are the only two
  persisted mutations off the stack (verb enum `ProjectUndo.h:20-96` diffed against all call
  sites). Route import through an AddClip(+asset) transaction; for the record commit, either
  make it a transaction too or make undo honestly refuse right after a take (painted reason
  via R4) — never silently eat an older edit. Gate: import, Ctrl+Z → the import reverts and
  the prior edit stays; redo restores. Out of scope: undoing the asset file write itself
  (bundle GC is a future carve).
- [x] **R9 — two instances can no longer clobber one project.** *(certified: feature
  `5f9b21d`, nine-job CI run `32884379498` green first try, local 356/356 — PHASE 1
  COMPLETE)* No
  `moreThanOneInstanceAllowed` override in `src/Main.cpp`, no `InterProcessLock` anywhere;
  WAL + busy-timeout (`ProjectBundle.h:1487,1499`) lets concurrent writers both "succeed",
  every edit writes a full snapshot, and both instances auto-open the same last project —
  last-writer-wins data loss from simply running `tools\run-yesdaw.ps1` twice
  (`run-yesdaw.ps1:85` always spawns fresh). Minimal honest slice: JUCE single-instance hook
  — second launch focuses the first (or exits with a painted reason). Gate: shell-level
  proof that a second init with the same session dir refuses/routes; keep it deterministic
  (no real process races in CI). Out of scope: multi-project multi-window.

## Phase 2 — mixing a real song (R10–R17)

- [x] **R10 — solo must not silence the soloed signal's own path.** *(certified: feature
  `6e75d9e`, nine-job CI run `32886357182` green first try, local 356/356)* Solo a vocal routed to a
  submix bus (M3 `outputBusId`) → the bus mutes → the vocal is SILENT; solo anything with a
  reverb send → the return dies. The SIP mask (`OfflineRenderer.h:441-465`,
  `MixerMutePolicy.h:52`) is correct, but buses are created `soloSafe=false`
  (`ProjectUndo.h:1405-1411`, default `Project.h:294`) and NO UI verb sets `soloSafe`
  (grepped; the field is only copied through). Fix: new buses default solo-safe (Logic law),
  a bus-strip control (e.g. Ctrl+click solo) toggles it, persisted + undoable. Gate: solo a
  bus-routed track → still audible through its bus; solo a sent track → return still
  audible; engine gate `mixer_mute_policy_tests.cpp:241` stays green. Out of scope: solo
  modes (AFL/PFL).
- [x] **R11 — the master strip grows a real FX chain.** *(done: `bd205e1`, CI run
  `33442913800` green, all nine jobs, first try)* `Project.h:837-839` locks master to
  gain-only — no way to put the existing Limiter on the mix bus, and no workaround since
  buses only feed master. Give master a `MixerStripState` (or the FX-relevant subset) and
  reuse the existing strip FX law end-to-end (chooser, param pages, bypass, reorder, undo,
  persistence, render parity). Additive schema bump + migration gate. Gate: add Limiter on
  master → offline render provably limited vs without; undo restores; reopen keeps it. Out
  of scope: master pan; monitoring-only analyzers.
- [ ] **R12 — live scalar edits without an engine rebuild.** The deep fix behind R2's seam:
  `PlaybackEngine` has no live parameter API (`PlaybackEngine.h:219-243`), so every fader
  tick rebuilds the world. Route scalar strip params (gain/pan/mute/send level/FX param)
  through a control→audio command lane applied to the RUNNING graph (the
  seqlock/command-queue machinery already exists — RuntimeAudioDriver, RtLaneRing), with the
  same persisted result afterward. Gate: playing project, ride a fader → no rebuild
  (assert via engine identity/counter), audio continues frame-contiguous, final value
  persisted + undoable exactly as today. Out of scope: structural edits (add/remove
  FX/track/clip may still rebuild).
- [ ] **R13 — buses can route and send like tracks.** `struct Bus` (`Project.h:402-413`) has
  no `sends` and no `outputBusId`: no submix→reverb send, no bus→bus routing. Add both with
  the existing SendRow UI law and cycle-refusal (the routing DAG law — refuse honestly,
  never clamp). Additive schema bump + migration gate. Gate: bus→bus chain renders through
  PDC correctly; a created cycle is refused with a painted reason (R4); persistence
  round-trips. Out of scope: sidechain taps from buses.
- [ ] **R14 — bus automation is reachable from the UI.** Engine targets BusFader/BusPan
  exist (`ProjectMixerProjection.h:644-645`) but `buildAutomationTargetOptions`
  (`MainComponent.cpp:9448-9497`) requires a track id — the bus targets are dead code from
  the shell. Let the target chooser enumerate the selected bus's fader/pan (and R13 sends
  once landed). Gate: pencil a bus-fader ramp from the UI → rendered output follows it. Out
  of scope: master automation (needs R11's strip first; park honestly if not).
- [ ] **R15 — automation Write/Off modes; rides beyond fader+pan.** `AutomationMode` is
  Read/Touch/Latch only (`Project.h:647-652`, chooser `MainComponent.cpp:4928-4930`), and
  `beginAutomationTouchRideIfArmed` is called from exactly two places (fader :5786, pan
  :5824) — send levels and FX params can never be ridden. Add Off (+ Write only if honest
  end-to-end), and arm rides on send-level and FX-param drags through the same law. Gate:
  ride a send level in Touch → breakpoints written; Off mode writes nothing and plays
  nothing back. Out of scope: trim automation.
- [ ] **R16 — automation curve shapes reach the UI.** The engine evaluates
  Linear/Hold/Bezier/Log (`Automation.h:21-27, 92-118`) but every UI write hardcodes Linear
  (`UiAppModel.h:5348, 5550`); no curve chooser exists. Add a per-breakpoint curve picker +
  `SetBreakpointCurve` verb (undoable, persisted). Gate: set Hold on a breakpoint from the
  UI → rendered value provably steps instead of ramping; persistence round-trips. Out of
  scope: freehand curve drawing.
- [ ] **R17 — the 5th send is not stranded.** `mixerSendVisibleRowCount = 4`
  (`UiTheme.h:634`) but `addSendOnSelectedTrack` (`UiAppModel.h:3241-3270`) never caps — a
  5th send exists in the project with no row to edit or remove it. Either refuse the add
  with a painted reason (R4) or scroll/page the rows; pick one, honestly. Gate: the chosen
  law at the boundary (refused add, or 5th row reachable). Out of scope: reordering sends.

## Phase 3 — editing depth (R18–R23)

- [ ] **R18 — clip drags paint an in-flight preview.** All clip gestures are
  commit-on-release: `mouseDrag` only sets `dragState.moved` (`MainComponent.cpp:843-844`)
  and `paint` (:498-573) previews loop-brace/marquee/ruler drags but never `dragState` —
  clips do not move on screen until mouse-up, and the snap landing is invisible. Paint a
  ghost (move/trim/fade/gain) from `dragState` + current pointer, same pattern as the E6
  loop-brace preview (:503-517); transient only, `project.db` byte-identical until release,
  Escape still cancels. Gate: mid-drag paint changes in the ghost region and the model is
  untouched; release persists exactly as today. Out of scope: waveform-accurate ghosts.
- [ ] **R19 — dragging near the viewport edge auto-scrolls.** Nothing in `mouseDrag`
  (:781-845) pans when the pointer nears/leaves `clipArea`; the marquee is even hard-clamped
  to the visible area (:800). Moving a clip a screenful away requires drop → pan → re-grab.
  Timer-driven edge-band pan reusing the existing scroll callback (:838-839), deterministic
  in gates via `serviceMainComponentUiTimer`. Gate: drag held in the edge band scrolls the
  viewport a provable amount; drop lands at the scrolled position. Out of scope:
  acceleration curves.
- [ ] **R20 — slip the clip's source window.** The model fully supports it
  (`Clip::srcOffset/srcLen`, `Project.h:471-472`; `trimClip` writes arbitrary offsets,
  `UiTimelineEdits.h:197-199`) but no gesture or action exposes slip —
  `TimelineDragMode` (`MainComponent.cpp:1164-1173`) has no Slip and `UiActions.h` no verb.
  Add Ctrl+Alt+drag on the clip body: same start/length, shifted `srcOffset`, clamped by
  `sourceWindowFits` (`Project.h:484-487`), undoable, snap-aware (Ctrl inverts per the E4
  law — pick the chord so it cannot collide; audit `chordForKeyPress` first). Gate: slip
  changes rendered audio with unchanged clip window; refusal at the source bounds; undo
  restores bit-identical. Out of scope: slide (moving window over fixed source).
- [ ] **R21 — piano-roll keyboard nudge moves the whole selection.** `nudgeSelection`'s roll
  branch (`UiAppModel.h:4715-4733`) moves only `selectedMidiNoteId_` while the timeline
  branch (:4740-4771) and every other roll verb are group-aware — marquee ten notes, press
  nudge, one moves. Route through `moveSelectedPianoRollNotesBy` (:2266). The singular gate
  (`ui_input_tests.cpp:10122`) pins only the single-note case — extend, don't weaken. Gate:
  group nudge moves all selected notes in one undo step. Out of scope: none (small item).
- [ ] **R22 — zoom to selection.** Zoom verbs are FitProject/FitLoop/In/Out only
  (`UiActions.h:595-601`); the workaround destroys the loop region as a side effect. Add
  `TimelineZoomFitSelection` reading the clip selection's span through the existing fit law
  (`MainComponent.cpp:7059-7084`); remember the UiActionId traps (descriptor at table END,
  both exhaustive switches, unique chord, tooltip, a11y context). Gate: exact viewport math
  for a known selection; disabled with no selection. Out of scope: zoom history.
- [ ] **R23 — select-to-end and razor-all-tracks.** Selection verbs stop at
  SelectAllTrack/Project (`UiActions.h:115-116`); split is single-clip
  (`UiAppModel.h:4519`). Add select-from-playhead-to-end (track-scoped and project-scoped)
  and split-all-tracks-at-playhead over the existing group machinery, one undo transaction.
  Gate: multi-track razor splits every intersecting clip in one undo step; selection spans
  prove exact membership. Out of scope: ripple.

## Phase 4 — recording honesty (R24–R29)

- [ ] **R24 — Test Device / Refresh can no longer stamp fake provenance on real takes.**
  Both shipped toolbar buttons call `applyDeterministicTestDeviceProfile`
  (`UiAppModel.h:6741, 6760`): stableDeviceId=1, `latencyCalibrated=true` (a false claim —
  real adoption sets false, :506), nothing re-adopts until the device restarts, so takes
  after a "Refresh" click commit `deviceStableId=1` (`:837` → `ProjectBundle.h:2238`); with
  an inputless device, Test Device unlocks Record and the shell commits a SYNTHETIC take
  into the real project (`MainComponent.cpp:7456` → `UiAppModel.h:5878-5882`). Fix: in the
  desktop shell, Refresh re-adopts the REAL device profile; the test profile becomes
  harness-only (hidden/disabled in the shell — gates keep using it through the registry
  context). Gate: after Refresh on an adopted real profile, the profile is unchanged; a
  real-shell record path can never commit the synthetic take. Out of scope: deleting the
  harness profile (tests need it).
- [ ] **R25 — no silent sample drops during recording.** Every action wraps in
  `suspendDesktopAudioCallback`/resume (`MainComponent.cpp:7031-7036`, removes the callback
  :6955-6965); during capture the device-frame cursor only advances inside the callback
  (`UiAppModel.h:381-385`), so blocks missed during a Save or zoom keystroke are seamlessly
  concatenated — the take is silently time-shifted. Fix: while capture is active, don't
  suspend for actions that can't touch the engine, and refuse/queue the ones that must
  (painted reason via R4). Gate: inject a suspend window mid-capture, assert frame
  continuity of the committed take (or the honest refusal). Also fixes the general
  every-keypress audio stutter for the non-recording case where the bracket is needless —
  narrow it. Out of scope: lock-free full concurrency of all actions.
- [ ] **R26 — the monitoring policy is visible.** The Monitor button cycles
  Unselected→DirectInput→LatencyCompensated→Off (`UiActions.h:750-765`) but paints a binary
  toggle (`MainComponent.cpp:7647-7648`) — Off lights identically to DirectInput. Paint the
  policy name/abbreviation on the control from real model state. Gate: label follows each
  cycle state. Out of scope: per-track monitor buttons.
- [ ] **R27 — take lanes exist in the timeline.** Zero take-aware paint in
  `TimelineCanvas.h`; stacked loop takes are overlapping clips distinguishable only via the
  inspector dropdown. Paint an expandable take-lane stack under the track for overlapping
  take groups (read-only display + click-to-choose-audible is the honest minimal slice,
  reusing the inspector's undoable chooser law `UiAppModel.h:1067-1092`). Gate: lane
  geometry from a real loop-record project; click switches the audible take undoably; paint
  matches model. Out of scope: swipe comping (R28), drag-editing takes.
- [ ] **R28 — real comping.** The Comp button assembles a hard-coded selection — first two
  takes, fixed 96-tick segments, 64-tick gap (`UiAppModel.h:6872-6879`). Replace with
  region-drag comp selection over R27's lanes (drag a range on a take lane → that range
  becomes audible from that take), persisted + undoable, arbitrary segment boundaries. Gate:
  a dragged comp region renders that take's audio in that window (audible-proof), one undo
  step. Out of scope: crossfade-at-comp-seams tuning (fixed default fade is fine — say so).
- [ ] **R29 — MIDI-only recording, and CC capture.** `captureMidiEventDuringRecording`
  refuses outside an audio capture session (`UiAppModel.h:673-676`) — recording MIDI without
  an armed audio input is impossible; CC/pitch-bend are discarded
  (`MainComponent.cpp:4060-4061`). Allow a MIDI-armed-only capture session (same
  start/stop/count-in law, no audio FIFO), and capture CC into the existing event model if
  the persisted schema honestly supports it — otherwise capture notes only and SAY so in
  the doc; never half-persist CC. Gate: MIDI-only session commits a MIDI clip at the
  compensated position; CC round-trips iff shipped. Out of scope: hardware MIDI alignment
  proof (owner-machine script; parked with the loopback cable item).

## Phase 5 — scale & polish (R30–R34)

- [ ] **R30 — imports stop deep-copying all audio.** Each import deep-copies every
  previously imported stem's samples (`std::vector<UiDecodedAsset> nextDecoded =
  decodedAssets_;`, `UiAppModel.h:1855`) on the message thread with audio suspended —
  ~115 MB per 5-min stereo stem, O(total-audio) per import, multi-second freezes by stem
  #10. Cheap immediate fix: `shared_ptr<const UiDecodedAsset>` elements (copy the vector,
  share the samples). Gate: an import leaves prior assets' sample buffers at the same
  addresses (harness-observable) and behavior unchanged. Out of scope: streaming from disk
  (its own future carve).
- [ ] **R31 — export runs off the message thread with real progress and a reachable
  Cancel.** `exportAudioFile` (`UiAppModel.h:1346-1420`) runs render→write synchronously in
  one message-thread call, so the shipped progress readout (`MainComponent.cpp:3044`) and
  Esc/Cancel branch (`UiActions.h:929-935`) are unreachable by a real user — the gate
  currently proves dead code. Worker-thread export with progress ticks marshalled to the
  UI; Cancel actually aborts and cleans up the partial file. Gate: deterministic
  harness-driven progress observed mid-export; cancel leaves no partial file and a painted
  status (R4). Out of scope: export queue.
- [ ] **R32 — UI prefs persist across launches.** Session state stores only last-project +
  recents (`UiAppModel.h:1196-1240`); mixer-dock collapse, inspector tab, snap unit,
  metronome, export bit-depth, and keymap rebinds reset every launch. Persist them in the
  session-state dir record (NOT the project bundle; no schema bump). Gate: round-trip
  through a fresh shell in the harness. Out of scope: per-project view state.
- [ ] **R33 — the keymap overlay stops listing dead chords.** Payload-required actions
  (`MixerTargetSetFader` Ctrl+Alt+F, `TimelineClipMove` Alt+M, all `PianoRollNote*` chords,
  etc.) hard-refuse in `dispatch` ("payload required", `UiAppModel.h:6118-6158`) yet the
  overlay lists their chords as if pressing them worked. Either give them honest
  keyboard-context behavior or mark/hide them in the overlay — pick per action, honestly.
  Gate: the overlay's painted list contains no chord that dispatch refuses from the
  keyboard. Out of scope: rebinding UI changes.
- [ ] **R34 — failed device switches say why.** The chooser reverts honestly but wordlessly
  (`MainComponent.cpp:3877-3897`). Route the failure string through R4's status surface.
  Gate: harness-injected device-open failure paints the reason and reverts. Out of scope:
  auto-retry.

## Parked (owner hardware / future carves — not items, do not absorb)

- **Hardware PASS for the shipped record path**: the one owner run captured pure silence (no
  loopback cable) — `docs/reality-lane.md:95`. Needs Dan: route an output into input 1, run
  `pwsh tools/shipped-record-check.ps1`. The checker itself is honest and ready. Also:
  `tools/verify-hardware.ps1:98` runs the engine-level check, not the shipped-path checker —
  fold `YesDawShippedRecordCheck` into the aggregate when the cable session happens.
- Loop-record cycles beyond 8 silently dropped (`UiAppModel.h:718`) — surface via R4 when
  convenient, or carve later.
- Third-party plugin insertion in strips (`FxKind` has five built-ins and no plugin case,
  `Project.h:205-212`, despite `src/engine/plugin/PluginNode` existing) — a product-scale
  feature, its own planning moment.
- FX presets, VCA/track grouping, stereo width, polarity/input trim, pan-law choice,
  multi-lane automation view, relink UI beyond R5's report, sample-rate conversion,
  streaming audio from disk.
