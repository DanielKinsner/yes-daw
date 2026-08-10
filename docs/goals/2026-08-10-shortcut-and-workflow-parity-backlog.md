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
2. **Multi-select clips** — Shift+click adds/removes from selection; Ctrl+A selects all clips
   on the selected track; Ctrl+Shift+A selects all clips in the project. Delete/copy/move act
   on the whole selection as one undo group.
3. **Marquee (rubber-band) selection** — pointer-tool drag on empty timeline selects every
   clip it touches. Gate: drag rectangle over two of three clips → exactly those selected.
4. **Split at playhead** — `B` splits the selected clip (or all selected clips) at the
   playhead. Gate: split → two clips, sample-accurate boundary, undo rejoins.
5. **Heal/join** — Ctrl+J merges two adjacent clips that reference the same asset with
   contiguous source windows. Refuse otherwise (honest status).
6. **Nudge** — `,` / `.` move the selected clip(s) left/right by one snap-grid unit;
   Shift+`,`/`.` = fine nudge (1/8 grid). Works in the piano roll on selected notes too.
7. **Alt+drag = copy-drag** — dragging a clip with Alt held leaves the original and moves a
   copy (one undoable AddClip).
8. **Clip gain keys** — Alt+Up/Alt+Down adjust selected clip gain ±1 dB. Gate: render
   amplitude follows.
9. **Default fades** — Ctrl+F applies the default fade in+out (token length) to the selected
   clip; applying to a clip with fades replaces them. Undoable.
10. **Crossfade** — `X` on two overlapping clips on one track creates equal-power
    complementary fades across the overlap. Gate: render sums to unity through the overlap.
11. **Clip rename** — F2 with a clip selected edits a clip display name (needs `Clip::name`,
    schema bump + migration gate; name shows on the painted clip).
12. **Esc cancels** — any in-progress drag (move/trim/fade/marquee) and any inline editor
    reverts on Escape. Gate: drag, Esc, project unchanged.
13. **Repeat paste** — Ctrl+R pastes the clipboard clip N times back-to-back at the playhead
    (N from a small chooser next to the snap control; default 2). One undo group.

## B. Navigation, view, transport

14. **Zoom to fit** — Ctrl+0 fits the whole project horizontally; Ctrl+Shift+0 zooms to the
    current loop region. Gate: viewport math (scroll+zoom snapshot) exact.
15. **Keyboard zoom** — `+`/`-` zoom in/out anchored at the playhead.
16. **Arrow navigation** — Up/Down select previous/next track (rail highlight follows);
    Left/Right move the playhead by one grid unit; Shift+Left/Right by one bar.
17. **Playhead follow (auto-scroll)** — during playback the viewport pages so the playhead
    stays visible; Options-menu toggle, default on. Gate: transport past right edge →
    scrollSeconds advanced.
18. **JKL shuttle** — K stops, L plays, L again = 2x, J plays reverse... reverse playback is
    NOT in the engine: implement L=play/2x/4x cycle and J=halve, K=stop, and document that
    reverse is out of scope (no fake).
19. **Return-to-zero on stop option** — Options toggle: stop returns playhead to start
    position vs stays. Enter = RTZ always.
20. **Play from click** — double-click on the ruler locates the playhead there (exists?
    audit); Shift+Space plays from the last locate point.
21. **Count-in for record** — Options toggle: Record waits one bar of metronome before
    capture starts (head tempo/meter). Gate: captured take's first frame aligns to bar 2.
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
