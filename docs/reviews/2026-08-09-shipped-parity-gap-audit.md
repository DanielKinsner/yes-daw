# Shipped-boundary parity gap audit — 2026-08-09

Adversarial audit of what the shipped `YesDaw.exe` can actually do through real UI controls,
against the basic Pro Tools/Logic working-musician workflow. Verified in `src/ui/MainComponent.cpp`
(the code `Main.cpp` starts) and `src/ui/UiAppModel.h` — not harness overloads, not the separate
`YesDawHardwareRecordingCheck` binary. An action counts only if a real control drives a real model
mutation that persists and affects playback.

## One-line summary

Through the shipped app a user can today: create a single-track project; import mono/stereo WAVs
that all pile onto "Audio 1"; move / snap-move / trim-right / gain-drag / fade-drag /
split-by-double-click / undo those clips; drive Play / Stop / whole-project Loop / Locate; edit
strip-0 fader/pan/mute/solo/bypass; adjust one hard-coded send level; add/remove one hard-coded
automation point; move/set-length/transpose/quantize an existing MIDI note; save in place; export a
canonical float WAV. Almost every other basic — creating tracks, moving a clip between tracks,
deleting or copying anything, keyboard shortcuts, real recording, adding an FX, drawing an
automation breakpoint, loop endpoints, tempo editing, zooming, markers, save-as — is painted-but-
inert in the shell or missing at the engine level.

## Key structural findings

- **No keyboard input at all**: `MainComponent` has no `keyPressed`, no `KeyListener`, no
  command-target. The keymap in `UiActions.h:451-497` only fills tooltips.
- **Record is synthetic**: the shipped Record button commits a fixed 256-frame sawtooth
  (`UiAppModel.h:404-543` → `makeDeterministicRecordedAudio` `:2215-2233`); desktop audio opens
  with **zero input channels** (`MainComponent.cpp:1074`). The real capture pipeline lives only in
  `tools/hardware/RecordingCheckMain.cpp`.
- **Track lifecycle does not exist anywhere**: no engine command, no model verb, no UI.
  `ensureDefaultAudioTrack` (`UiAppModel.h:2385`) is the only creation path.
- **The left track rail and all mixer strips except index 0 are pure paint** — including a
  hard-coded `i == 3` selected-row highlight (`MainComponent.cpp:2748`) and a strip-0-only
  interactive gate (`:3489`, `:1620`).
- **FX insert UI is absent** while the engine (`ProjectUndo.h:250-311`, `Project.h:1960-2036`,
  five H14 nodes) is fully ready; the Inspector "CLIP FX" section is a literal `"None"`.
- **Automation UI adds a hard-coded (1920, 0.50) breakpoint** (`UiAppModel.h:175-176`) — no
  click-to-place, no drag.
- **Piano roll cannot create or delete notes**; MIDI clips only appear as a side-effect of the
  synthetic Record.
- **No zoom, no scroll, no markers, no tempo/time-sig editing, no metronome, no save-as,
  no loop range, no snap UI** (drag snaps only via Ctrl to a fixed 512-tick grid).
- **Fake chrome still shipping**: painted "FILE EDIT VIEW OPTIONS HELP" menu text
  (`MainComponent.cpp:2595-2607`), decorative fade-curve combo (`:1560-1568`), track-row meters
  hard-coded 0.0 (`:2907`).

## Ranked backlog

### P0 — blocks recording/editing/mixing/exporting one real song
1. Real audio recording from the device behind the shipped Record button (wire the proven
   FIFO/take pipeline from `tools/hardware/RecordingCheckMain.cpp` + `app/RecordingAssetCommit.h`;
   open input channels).
2. Track create/rename/delete/reorder — engine commands + model verbs + interactive track rail.
3. Multi-track arrangement: import to selected track; vertical clip drag between tracks
   (needs engine `moveClipToTrack` — `ProjectUndo.h:92 moveClip` is start-only).
4. Delete clip (no engine verb) and delete note (`cutNote` exists, no UI caller).
5. Bind the declared keyboard shortcuts (Space, Ctrl+Z/S/O/I, R, L, K, Home, Ctrl+E, Del…).
6. Mixer strip interactivity for every strip (click-to-select minimum), not just index 0.
7. FX inserts from the UI: add/remove/reorder/param/bypass with a chooser panel.
8. Save-As / Save Copy.
9. Tempo and time-signature editing (engine commands + header controls).

### P1 — daily-driver essentials
Trim-left; copy/paste/duplicate clip; user-defined loop region; markers (paint + commands);
timeline zoom + scroll; snap toggle/grid picker consulted by drags; real automation lane canvas
(click-to-add anywhere, drag, per-track targeting); piano-roll pencil add + delete; MIDI clip
creation without recording + MIDI recording; real device chooser; send create/route/level; bus
create/delete/rename; metronome; launch-time autosave recovery; tool palette; import at playhead;
export options; DAWproject export button; real menu bar (or remove the painted one); track-rail
row interactivity incl. real meters.

### P2 — parity polish
Undo history panel; more fade curves; per-clip FX; VST3/AU hosting UI (H18); musical instrument
node (impulse clicks today); track color/grouping; time-stretch UI verb; keymap help; etc.

Full file:line evidence for every item lives in the audit session transcript (2026-08-09); the
load-bearing lines are quoted above.
