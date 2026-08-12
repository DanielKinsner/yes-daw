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
22. [x] **Tool keys** — landed in `202eea9` (exact-head run `31542586504`, nine jobs
    green). The existing unique `V/P/S/H/Z` bindings select Pointer/Pencil/Scissors/Hand/Zoom;
    no duplicate actions or replacement chords were added. The existing unique `Esc` action is
    now honestly context-sensitive: it cancels an active audio export without changing tools, or
    otherwise returns to Pointer. The shipped-boundary gate proves idle Escape enables a real
    Pointer marquee, persisted Delete, silent playback, and one-step Undo restoration.
23. [x] **Locate points** — landed in `182f255` (exact-head run `31547337686`, nine jobs green).
    Because the existing view and snap bindings conflict, unique `Ctrl+Shift+1..5` chords store the
    playhead and `Alt+1..5` recalls it. Schema v11 additively persists all five slots; the real-input
    reopen gate proves exact audible playback recall and an honest no-op for an empty slot.
24. [x] **Next/prev marker** — landed in `012bf18` (exact-head run `31551471745`, nine jobs
    green). Unique `Ctrl+Right` / `Ctrl+Left` actions locate the strictly next/previous persisted
    Marker without wrapping; the real-input reopen gate proves exact Marker frames, honest end
    no-ops, and distinct audible playback at both positions.
25. [x] **Ruler range selection** — landed in `4f98a30` (exact-head run `31557950879`, nine jobs
    green). Plain ruler drag selects a painted transient time range (click still locates and
    collapses it; Escape cancels a drag); unique `Shift+L` converts the range to the real transport
    loop; the export "Loop Region" source prefers the range when set. The shipped-boundary gate
    proves the sliced export is sample-identical to the whole-Project slice and `project.db` stays
    byte-identical because the range is honestly transient.

## C. Tracks, mixer, MIDI, polish

26. [x] **Duplicate track** — landed in `1b91a64` (exact-head run `31565388312`, nine jobs green).
    Ctrl+Alt+T duplicates the selected track — clips, MIDI clips/notes, scalar strip state (new
    SetTrackMixScalars engine verb), FX chain with params, and sends — as one transaction group of
    undoable verbs with fresh entity ids, named "<name> copy", placed directly below the source
    (the dev-only test-device action moved to Ctrl+Alt+Shift+T to free the chord). The
    shipped-boundary gate proves the persisted copy, exactly-doubled playback, one-step undo/redo,
    and the non-last-track reorder path. Takes and automation lanes honestly stay on the source.
27. [x] **Move track up/down** — landed in `cefd864` (exact-head run `31566327317`, nine jobs
    green). Ctrl+Shift+Up/Down move the selected track one row through the existing undoable
    ReorderTrack verb; the rail selection follows the moved row and top/bottom boundary presses are
    honest no-ops. The shipped-boundary gate proves persisted order at every step, painted rail
    follow, the boundary no-op, per-move undo, and the shared mute control landing on the moved
    track to exact silence.
28. [x] **Selected-track keys** — landed in `3ce2306` (exact-head run `31567061090`, nine jobs
    green). Shift+M mutes and Shift+S solos the selected track as the same persisted strip edit as
    the mixer controls but panel-preserving (the Timeline stays in front); Shift+R toggles the
    honestly-transient recording arm onto the selected track with cross-track retargeting.
    Marker-remove's conflicting chord was reassigned to unique Ctrl+Shift+M and re-pinned in the
    gate. The shipped-boundary gate proves persisted mute/solo with exact silence through the real
    solo policy, byte-identical `project.db` across arm/retarget/disarm, and both marker chords.
29. [x] **Alt+click resets** — landed in `5e4dfde` + macOS repair `e0dc4df` (exact-head run
    `31568595210`, nine jobs green). Alt+click resets the mixer fader and send levels to unity,
    pan to center, each FX param to its own re-bound `ParamSpec.normalizedDefault`, and the rail
    VOL/PAN minis to unity/center, all through each control's existing persisted edit path. One
    red round: ARM FMA contraction in JUCE's slider snap left ~2e-17 dust where x64 landed exactly
    on centered pan; the pan handler now grid-snaps with cancellation-free arithmetic. The
    shipped-boundary gate proves every control with a real off-default edit then a real Alt+click
    reset, including exactly-doubled playback for the fader-to-unity reset.
30. [x] **Fine drag** — landed in `44185e2` (exact-head run `31569576522`, nine jobs green).
    Shift while dragging is exactly 10x finer (manual `FineDragSlider` subclass; JUCE velocity mode
    is not exactly 10x): anchored at the current value with no jump-to-pointer, unsnapped
    accumulation, mid-drag engage, latch until mouse-up, and Alt excluded so the B29 reset law is
    untouched. Covers the mixer fader/pan, sends, FX params, header tempo, inspector sliders, and
    the rail VOL/PAN minis (persisted-value anchoring). The shipped-boundary gate proves exact
    per-style math, interval-grid landing, the no-jump anchor, and a >5x coarse/fine ratio.
31. [x] **dB readout** — landed in `6e95696` (exact-head run `31570461464`, nine jobs green).
    A shared click-transparent label shows `20*log10(gain)` to one decimal (`-inf dB` at exact
    silence) above the mixer fader or rail VOL while its drag is held, live-updating through the
    real drag/value paths (`FineDragSlider` fires the drag callbacks on its fine path; the rail
    minis gained a gesture-end signal). Honestly transient view state. The shipped-boundary gate
    proves hidden-at-rest, no-show on programmatic changes, exact live text, `-inf` at zero, and
    hide-on-release.
32. [x] **Meter peak-hold + clip light** — landed in `ffa7aa0` (exact-head run `31573011101`,
    nine jobs green). Live per-track MeterNode peaks now reach the shell (control-side
    `CompiledGraph::nodeForId`, pointers harvested at `PlaybackEngine::create`,
    `UiAppModel::trackMeterPeak`); the shared per-track readout state holds peaks for 60 UI ticks
    (~2 s, never wall-clock), latches the clip light at 1.0 (0 dBFS), reads live silence when
    stopped, and paints on both the rail and painted strip meters; clicking either meter clears
    the shared latch (rail click zone + paint-mirrored strip hit test). Buses honestly keep their
    surface meters (no live tap). The gate proves latch, persistence, both click-clears, and
    exact tick-law hold expiry from real pixels.
33. [x] **Piano-roll velocity editing** — landed in `22b0092` (exact-head run `31574711380`, nine
    jobs green). Alt+wheel on the note under the cursor adjusts its velocity by the exact wheel law
    (clamped to [0, 1]) through the new undoable SetNoteVelocity engine verb, added per the AddNote
    pattern including the randomized generated-edit-sequence property test; velocity tints the
    painted note body from a theme tint floor to full brightness. The shipped-boundary gate proves
    the persisted wheel law, tint pixels, velocity-scaled synth loudness, honest clamp-to-silence,
    and per-edit undo.
34. [x] **Piano-roll transpose keys** — landed in `26b5bc8` + macOS repair `cd2f781` (exact-head
    run `31576683772`, nine jobs green). Multi-note selection (deferred by B6) lands here: Ctrl+A
    in the Piano Roll selects every note in the clip, painted selected and pruned like timeline
    clips. Up/Down transpose the selection ±1 semitone and Del deletes it, honestly
    context-sensitive in the Piano Roll only; unique Shift+Up/Shift+Down are new octave actions;
    Backspace deletes the selection. Group edits are one atomic undo transaction, an out-of-range
    note refuses the whole transpose group. One red round: AppleClang's unused-lambda-capture (the
    known trap). The gate proves exact semitone/octave persistence, untouched rail arrows, one-step
    group undo, audible transposition, and silent playback after delete-all.
35. [x] **Note duplicate** — landed in `f3ab261` (exact-head run `31578215018`, nine jobs green).
    Ctrl+drag copy-drags a note (one fresh-id AddNote, every payload field preserved, Ctrl wins
    over the narrow-note resize-edge zone); Ctrl+D goes context-sensitive in the Piano Roll and
    lands the copy one grid step later via the new unique Alt+Shift+D action; non-fitting copies
    are honest refusals. A gate flake (same-frame same-key off/on order tie-broken by fresh
    wall-clock ULIDs) was made deterministic by proving audibility through the temporally
    separated drag copy; the engine's per-project event order was not touched.
36. [x] **Quantize selected** — landed in `22ef6f0` (exact-head run `31579179874`, nine jobs
    green). Unique `Q` quantizes the whole note selection to the current snap grid (real
    tempo/meter-derived frames) as one atomic undo group through the existing QuantizeNote verb,
    skipping already-aligned notes so mixed selections quantize the rest and refusing honestly
    when the entire selection is aligned. The gate derives expectations from the same engine
    snapTick law and proves exact grid landing, one-step group undo with bit-identical playback,
    audible change, and the honest no-op.
37. [x] **Confirm on close** — landed in `155e36a` (exact-head run `31580310432`, nine jobs
    green). "Unsaved changes" honestly means edits since the last explicit Save (an edit serial
    across every mutation path, cleared by Save/Save-As and fresh attach) — every edit already
    persists synchronously, so closing never rolls back data. The window close button asks through
    the new harness-injectable `confirmCloseUnsavedChanges` seam (native three-way box otherwise):
    Save records the state and closes, Close-without-saving closes with the bundle current, Cancel
    stays; autosave untouched. The gate proves all flows through the injected seam.
38. [x] **Dirty marker + title** — landed in `fa0b779` (exact-head run `31581331754`, nine jobs
    green). The shell computes "<bundle stem>[*] - YES DAW" from the B37 edit serial as pure
    state-derived data, exposed through the harness snapshot and pushed deduplicated to the parent
    DocumentWindow on the UI tick; without a project the versioned startup title stays. The gate
    proves the exact law: empty pre-project, clean stem title on a fresh project, starred after a
    real edit, clean after Ctrl+S, dirty again after undo.
39. [x] **Open Recent** — landed in `57dd070` (exact-head run `31582341126`, nine jobs green).
    A `recent-projects.txt` MRU (up to five, most recent first, same UTF-8 encoding; the
    last-project record's format untouched) updates on every bundle attach and Save-As; the File
    menu gains an Open Recent submenu whose entries reopen bundles through the shared open-by-path
    helper, and the injectable choices gain a session-directory seam so gates stay test-local. The
    gate proves MRU order across two real New flows, real-menu reopening confirmed by the title,
    and the reopened bundle returning to the top.
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
