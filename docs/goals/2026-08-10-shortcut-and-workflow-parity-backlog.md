# Shortcut & workflow parity backlog — the "feels like a real DAW" list

Owner directive (Dan, 2026-08-10): the feature parity stack is landed and certified
(`STATUS.md`, run 31372922569+). What separates a demo from a daily driver is the hundred
little behaviors — shortcuts, nudges, resets, selections. This is the canonical list.

## How to work this list (read first, non-negotiable)

- **Read `STATUS.md` and the 2026-08-10 entries first.** Multiple agent sessions push to
  `main`: `git pull --rebase` before EVERY chunk, small commits, push when green.
- **Shipped-boundary standard**: an item counts ONLY when a real control or key drives a real
  model mutation that persists and affects playback, proven by a mechanical gate
  (ui_input/app_smoke/render test). No harness-only seams.
- **Audit before adding**: check `uiActionDescriptors()` in `src/ui/UiActions.h` and
  `MainComponent::keyPressed`/`chordForKeyPress` — several items may partially exist. Re-pin
  legacy assertions to NEW semantics; never loosen a gate.
- **Known traps** (each cost a red CI round already): (1) new `UiActionId` values need their
  descriptor at the END of the table (order == enum order) AND cases in BOTH exhaustive
  switches (registry + UiAppModel::dispatch) or GCC/AppleClang fail `-Werror=switch` while
  MSVC stays silent; (2) AppleClang alone flags unused namespace-scope consts; (3) the
  theme-audit gate rejects raw numeric literals in audited layout constructs — use UiTheme
  tokens or ternaries; (4) the ui_input childCount assertion tracks shell children — bump it
  deliberately; (5) the header pixel-invariance screenshot gate requires header pixels
  identical across Timeline/Mixer/PianoRoll views; (6) the mixer tools column only has full
  height in Mixer view — gate tests must `ViewMixer` first; (7) key chords must be UNIQUE —
  grep the descriptor table before assigning any new chord.
- **Hard stops**: never edit accepted ADRs, golden outputs, or `[[clang::nonblocking]]`
  annotations. New engine-visible state (e.g. clip mute) needs a schema bump + migration gate
  like ADR-0044 did, and an ADR if it changes a decided contract.
- Work priority order: A → B → C. Within a section, top to bottom.

## A. Everyday editing (highest value)

1. [x] **Ctrl+X cut clip** — landed in `b8544f2` (exact-head run `31422183559`, nine jobs green).
   Copy to clipboard + delete is one undoable edit; cut → paste at playhead reproduces the clip,
   and the shipped-boundary gate proves the playback result is bit-identical.
2. [x] **Multi-select clips** — landed in `12b1d1f` (exact-head run `31428021616`, nine jobs green).
   Shift+click adds/removes from selection; Ctrl+A selects all clips on the selected track;
   Ctrl+Shift+A selects all clips in the project. Delete/copy/move act on the whole selection as
   one atomic undo group, with persisted playback-affecting coverage at the shipped boundary.
3. [x] **Marquee (rubber-band) selection** — landed in `ce263e2` (exact-head run `31441353769`,
   nine jobs green). Pointer-tool drag on empty timeline paints a marquee and selects every Clip
   whose hit rectangle it touches. The shipped-boundary gate selects exactly two of three Clips,
   persists a grouped Delete, proves the resulting playback is silent, and undoes the group.
4. [x] **Split at playhead** — landed in `93108f2` (exact-head run `31446312029`, nine jobs
   green). `B` splits every selected Clip crossed by the playhead in one atomic undo group.
   The shipped-boundary gate proves exact adjacent timeline/source windows for two selected
   Clips, bit-identical playback through the split boundary, and one Undo rejoins both.
5. [x] **Heal/join** — landed in `8f0906c` (exact-head run `31450029917`, nine jobs green).
   Ctrl+J joins exactly two adjacent Clips only when they share a Track and Asset, their source
   windows are contiguous, and their playback settings match; every other case is an honest no-op.
   The shipped-boundary gate proves refusal leaves persistence and undo untouched, then heals a real
   split back to the original persisted Clip and bit-identical playback in one undoable transaction.
6. [x] **Nudge** — landed in `fe79428` (exact-head run `31453353993`, nine jobs green).
   `,` / `.` move selected Clips or the selected Piano Roll Note left/right by the current snap-grid
   unit; Shift+`,`/`.` use exactly 1/8 grid. The shipped-boundary gates prove persisted group Clip
   movement, Note movement, playback timing changes, bit-identical round trips, and one-step Undo.
   Multi-Note selection remains owned by item 34; no unsupported selection behavior is faked here.
7. [x] **Alt+drag = copy-drag** — landed in `29af223` (exact-head run `31456726490`, nine
   jobs green). Center Alt+drag leaves the original unchanged and moves one fresh-ID copy in time
   and across Tracks through exactly one persisted AddClip; edge Alt+drag remains the existing fade
   gesture. The shipped-boundary gate proves persistence, every playback field, audible output, and
   one-step Undo with a bit-identical playback round trip.
8. [x] **Clip gain keys** — landed in `4a48cd5` (exact-head run `31460002977`, nine jobs
   green). Alt+Up/Alt+Down adjust the selected Clip gain by exactly ±1 dB through one persisted
   edit. The shipped-boundary gate proves bundle readback, matching rendered-amplitude ratios, and
   one-step Undo restoring bit-identical playback.
9. [x] **Default fades** — landed in `b4dc4bc` (exact-head run `31463488384`, nine jobs
   green). Ctrl+F replaces both selected-Clip fades with the named 10 ms UI token through one
   persisted edit. The shipped-boundary gate starts from non-default fades, proves bundle readback
   and the full equal-power rendered envelope at both edges, then proves one-step Undo restores the
   prior fades and bit-identical playback.
10. [x] **Crossfade** — landed in `51b0f39` (exact-head run `31468250237`, nine jobs green).
    `X` on exactly two staggered, overlapping Clips on one Track creates one persisted equal-power
    crossfade across the exact overlap. The shipped-boundary gate proves constant-power unity,
    independently matches every rendered overlap sample, and proves one-step Undo.
11. [x] **Clip rename** — landed in `9de946c` (exact-head run `31474044198`, nine jobs green).
    F2 with a selected Clip opens the real inline Clip-name editor; otherwise it retains Track
    rename, with explicit Track rename on unique Ctrl+F2. Schema v10 additively migrates persisted
    names, and the shipped-boundary gate proves bundle round-trip, painted name, undo/redo,
    duplicate preservation, fresh reopen, and audio-invariant playback through the real rebuild.
12. [x] **Esc cancels** — landed in `531872c` (exact-head run `31479071195`, nine jobs green).
    Escape clears any pending Timeline move/trim/fade/marquee before mouse-up can persist it and
    dismisses both inline rename editors without saving draft text. The shipped-boundary gate proves
    unchanged bundle state, selection, and playback; idle Escape retains audio-export cancellation.
13. [x] **Repeat paste** — landed in `8b534f1` (exact-head run `31483925623`, nine jobs
    green). Ctrl+R pastes the Clipboard N times back-to-back at the playhead through one persisted
    undo transaction; the adjacent chooser offers 2x/3x/4x/8x and defaults to 2x. The shipped-
    boundary gate proves disabled empty-Clipboard behavior, default and selected counts, exact
    placement and payload preservation, audible playback change, and one-step bit-identical Undo.

## B. Navigation, view, transport

14. [x] **Zoom to fit** — landed in `eb1c2fc` (exact-head run `31488913430`, nine jobs green).
    Ctrl+0 fits the whole project horizontally; Ctrl+Shift+0 zooms to the current loop region.
    The shipped-boundary gate proves exact viewport scroll+zoom snapshots while persisted Clips and
    the playback loop remain unchanged; this is honestly view-only.
15. [x] **Keyboard zoom** — landed in `330b90d` (exact-head run `31494171745`, nine jobs
    green). `+`/`-` zoom in/out through the same exact viewport math as Ctrl-wheel while keeping
    the playhead at the same pixel. The shipped-boundary gate proves the physical shifted-plus key,
    unique chords, exact zoom/scroll snapshots, whole-Project clamp, unchanged persisted Clips, and
    bit-identical playback; this is honestly view-only.
16. [x] **Arrow navigation** — landed in `eb21550` (exact-head run `31500822063`, nine jobs
    green). Up/Down select the previous/next Track through the shared rail/mixer target and the
    painted highlight follows; Left/Right locate the real playhead by the current grid unit, while
    Shift+Left/Right locate by one tempo/meter-derived Bar. The shipped-boundary gate proves
    persisted, playback-silencing mixer retargeting, distinct rendered audio at grid/bar locations,
    frame-zero clamping, and unchanged Project persistence for transient navigation state.
17. [x] **Playhead follow (auto-scroll)** — landed in `aa6e1f0` (exact-head run `31509481842`,
    nine jobs green). The default-on, ticked Options toggle controls Timeline paging from the real
    playback clock; after the playhead crosses a viewport edge, the 33 ms UI refresh advances
    `scrollSeconds` by whole pages. The shipped-boundary gate proves off suppresses paging, on restores
    it, rendered audio stays bit-identical, and `project.db` stays byte-identical because this is
    honestly transient view state rather than fake Project persistence.
18. [x] **JKL shuttle** — landed in `827e86b` (exact-head run `31515933335`, nine jobs
    green). `L` starts real forward playback at 1x and advances to true 2x/4x sample-striding
    playback; `J` halves 4x → 2x → 1x, then stops, and `K` stops/reset. Loop moved to unique
    Ctrl+Alt+Shift+L. Reverse playback remains honestly out of scope because the engine does not
    support it; no reverse behavior or persistence is faked. The shipped-boundary gate proves
    rendered samples, proportional playhead movement, stop/reset behavior, unique chords, audible
    output, and byte-identical Project persistence for the transient shuttle rate.
19. [x] **Return-to-zero on stop option** — landed in `ae16f93` (exact-head run
    `31522035255`, nine jobs green). The default-off Options toggle captures the playhead when
    playback starts; `K`/the `J` stop boundary either stay at the stopped frame or return to that
    captured start. Unique `Enter` always locates timeline zero while preserving playing/stopped
    state; `Home` remains available. The shipped-boundary gates prove real menu state, nonzero
    start capture, exact replay/RTZ audio, both Stop modes, unique chords, and byte-identical
    Project persistence because the option is honestly transient transport state.
20. [x] **Play from click** — landed in `eb8e01c` (exact-head run `31528182517`, nine jobs
    green). Ruler double-click now performs a real transport locate instead of persisting a Marker;
    `M` remains the explicit Marker-add action. Unique Shift+Space queues real Locate then Play from
    the last explicit locate, independently of later plain playback starts and compatibly with the
    return-to-start-on-Stop option. The shipped-boundary gates prove exact locate, no accidental
    Marker persistence, sample-identical replay, audible playback, unique chords, and a byte-identical
    `project.db` because the remembered locate is honestly transient transport state.
21. [x] **Count-in for record** — landed in `255a960`; deterministic macOS gate repair
    `404ecbd` (exact-head run `31537060191`, nine jobs green). The default-off Options toggle
    makes `R` roll one audible head-tempo/meter bar before deterministic or real-device capture;
    `K` cancels pending count-in. In 150 BPM 7/8 at 48 kHz, the shipped-boundary gates prove no
    pre-roll input persists and the real Take/Clip/MIDI playback mutation starts at bar 2, frame
    67,200, with unique chords and denominator-correct click spacing.
22. **Tool keys** — single-key tool switching that doesn't collide with existing chords
    (audit first; e.g. F1–F5 or `A`=pointer, `D`=pencil, `G`=scissors — grep for free keys).
    Esc also returns to pointer.
23. **Locate points** — Ctrl+1..5 stores the playhead, 1..5 recalls it (if 1/2/3 view keys
    conflict, use Ctrl+Shift+1..5 store / Alt+1..5 recall; keep the keymap table honest).
24. **Next/prev marker** — Ctrl+Right/Ctrl+Left jump the playhead to the next/previous
    marker.
25. **Ruler range selection** — plain drag on the ruler (no Shift) selects a time range
    (paint it); Shift+L converts range → loop region; range doubles as the export
    "Loop Region" source when set.

## C. Tracks, mixer, MIDI, polish

26. **Duplicate track** — Ctrl+Alt+T duplicates the selected track WITH its clips, strip
    state, FX chain (fresh entity ids, "<name> copy"). One undo group.
27. **Move track up/down** — Ctrl+Shift+Up/Down reorders the selected track (verbs exist;
    wire keys + rail follows).
28. **Selected-track keys** — Shift+M toggles mute, Shift+S solo, Shift+R arm on the
    SELECTED track without opening the mixer (audit Shift+M conflict with marker-remove
    first; reassign marker-remove if needed — update its descriptor + gate).
29. **Alt+click resets** — Alt+click on any fader → unity, any pan → center, any send level
    → unity, any FX param slider → spec default. Gate per control.
30. **Fine drag** — Shift while dragging any slider/fader/knob = 10x finer. (JUCE
    setVelocityModeParameters or manual; must work on rail minis too.)
31. **dB readout** — the mixer fader and rail VOL show live dB (20*log10(gain)) in a tooltip
    or tiny label while dragging; -inf at zero.
32. **Meter peak-hold + clip light** — strip and rail meters hold peaks ~2s and latch a red
    clip indicator at >= 0 dBFS; click clears. Painted-only is fine; state in the meter
    readout path.
33. **Piano-roll velocity editing** — Alt+wheel (or Alt+vertical-drag) on a note adjusts its
    velocity; velocity tints the painted note. Undoable SetNoteVelocity (new engine verb if
    missing — follow the AddNote pattern incl. randomized property test).
34. **Piano-roll transpose keys** — Up/Down = ±1 semitone on selected note(s), Shift+Up/Down
    = ±1 octave (verbs exist: TransposeNote); Ctrl+A selects all notes in the clip;
    Delete/Backspace deletes selection.
35. **Note duplicate** — Ctrl+drag on a note copy-drags it; Ctrl+D duplicates the selected
    note one grid step later.
36. **Quantize selected** — `Q` quantizes selected notes to the snap grid (verb exists;
    audit the key + multi-note behavior).
37. **Confirm on close** — closing the app with unsaved changes prompts Save/Discard/Cancel
    (autosave stays independent). Harness-injectable chooser like the file dialogs.
38. **Dirty marker + title** — window title shows "<project> — YES DAW" with `*` when
    unsaved edits exist. Gate: edit → title dirty; save → clean.
39. **Open Recent** — File menu lists the last 5 project bundles (session state dir already
    records the last one; extend to a small MRU list). Gate: open two projects → both listed,
    most recent first.
40. **Tooltips everywhere** — every interactive control has a tooltip naming its action and
    chord (pull from the descriptor table so they can't drift). Gate: iterate shell children
    with a componentID → non-empty tooltip.
41. **SNAP label clip** — the "SNAP" label is clipped by the Beat chooser (cosmetic, known).
    Fix the header row spacing tokens.

## Explicitly out of scope (do NOT fake these)

- Reverse playback, time-stretch/elastic audio, tab-to-transient (needs onset detection),
  ripple editing, track freeze/bounce-in-place, plugin hosting UI (H18), video. If an item
  above collides with one of these, land the honest subset and say so in STATUS.md.

## Definition of done (whole list)

Every landed item: chord in the descriptor table (unique, shown by Ctrl+/ keymap overlay and
menus), gate-covered at the shipped boundary, suite green locally, and the EXACT-HEAD GitHub
Actions run green across all nine jobs before the list is called complete. Update `STATUS.md`
per chunk and tick items here with the commit sha.
