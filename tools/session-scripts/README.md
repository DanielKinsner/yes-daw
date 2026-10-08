# Session drives (the "see-it" half of every gate)

A drive runs a session script against the **real exe** — the shipped `YesDaw.exe`, a real window,
real mouse and keyboard events, the state probe read over the harness pipe — and takes the screenshots
the plan's §7.4 rubric is judged on. The headless gates (`ctest`) are the mechanical half; a drive is
the proof that the mechanical half describes the thing Dan sees. Every G-item's story in `STATUS.md`
names its drive under **See-it**.

```
powershell -ExecutionPolicy Bypass -File tools/session-drive.ps1 -Script tools/session-scripts/ssN-<name>.ps1 -Shots build-ci/session-shots/ssN
```

The runner (`tools/session-drive.ps1`) gives a script these primitives: `Launch` / `Close`, `Step n
'title'`, `Assert cond 'message'`, `Probe` (the state JSON), `WaitProbe { param($q) … } -TimeoutMs`,
`LayoutRect 'id'`, `Click 'id' [-Right] [-Double] [-OffsetX/-OffsetY]`, `DragWithin 'id' x0 y0 x1 y1`,
`Key 'Chord' [-Repeat n]`, `Focus`, `Shot 'name'`, `Resize w h`, `WaitDialog 'title'`,
`FileDialogEnter path`, `NewProjectChooser [-ViaKey]` (File > New through the New Project dialog to the
native chooser), `MenuPickByName 'text'` (an open popup's item by its text), and UI Automation's view —
`UiaFocused` (the element a screen reader reads) and `UiaFind 'name' [-Type CheckBox]`. A click target is a **layout id** from the probe's `layout` map — a painted
zone (`mixer.strip.2.insert.0`, `header.gear`, `timeline`) or `widget.<componentId>` for a live
component (`widget.project.new`, `mixer.fx.editor.close` (the editor's buttons are grandchildren, exported by name)).

## The scripts

| script | plan exit | grown by |
| --- | --- | --- |
| `ss1-first-minute.ps1` | SS-1 "the first minute" | G0 / G1 |
| `ss2-mouse-then-keys.ps1` | SS-2 "mouse, then keys" | G1 |
| `ss3-edit-a-song.ps1` | SS-3 "edit a song" | G2 |
| `ss4-track-instrument.ps1` | the Track instrument checkpoint | G3.1 |
| `ss5-piano-roll.ps1` | the piano roll checkpoint | G3.2 – G3.8 |
| `ss6-write-a-beat.ps1` | SS-4 "write a beat and a chord progression" | G3 (the exit) |
| `ss7-mix-the-song.ps1` | SS-5 "mix the song" | G4 (grown per item) |
| `ss8-project-lifecycle.ps1` | SS-6 project lifecycle / Usable-song journey | G5–G6 (Step 6 known-red until ADR-0068 lands) |
| `ss9-record-a-take.ps1` | SS-7 recording journey | G7 (**planned; not built**) |
| `ss10-plugin-lifecycle.ps1` | separate plugin lifecycle journey | G4.8/H18 (**planned; not built**) |

A script is grown, never forked: each G-item appends its steps to the script of its exit session and
the header comment logs the item and any deviation from the plan's text (the same deviations go in the
STATUS story's **Deviation log**).

SS-1 through SS-3 are current prerequisites, not historical decoration: G4.0a repairs their setup and
startup paths before more editor work. The Usable-song milestone requires the applicable existing drives
plus the future SS-6 lifecycle journey; planned filenames above are routing commitments, not evidence that
the scripts or features already exist.

Every `Launch`, including saved-project relaunches, asserts B6's unchanged 3,000 ms budget from
process start to the first interactive probe. A script's other assertions cannot hide an over-budget
launch. Run timing drives serially with no concurrent build or other drive; retain every raw result.

For paint diagnosis, `frame.parentPaintMs` measures the shell's own drawing and
`frame.canvasPaintMs` measures the latest static-canvas rebuild; use `canvasPaintCount` to tell
whether that cache rebuilt. B2 still measures the whole shell through `paintOverChildren`, including
cache preparation/compositing and children, over the same rolling 256 paints. These attribution
fields never reset or replace its sample or threshold.

## Keyboard navigation contract

G4.0b proves ADR-0049's shared **Control target** before more FX editors extend the surface. Native
widgets do not own DAW shortcuts. Tab and Shift+Tab traverse visible enabled controls in a stable order;
Tab or accessibility targeting starts control navigation; returning to an editor canvas ends it.
Enter activates or enters/confirms control interaction; arrows adjust only that interaction; Esc cancels
it and restores the prior editor context. Priority is active text entry, Space transport, Control-target
Enter/Esc/Tab navigation, other global transport, active control adjustment, then the editor command.
Enter outside control navigation remains Return to zero. One key dispatches once; hiding/closing a control
must leave no stale target. Drives assert the resulting probe/model state, including that Space still
controls transport and a slider arrow does not also nudge a Clip.

## Lessons (each one cost a red drive)

- **JUCE popup keyboard navigation SKIPS disabled items.** A count from the top of a menu is not
  stable when an item above the target can be disabled (Add Send ▸ with no bus, Arm with no device).
  The structural verbs sit at the BOTTOM of every strip menu and are always enabled — count UP from
  the end (`Up` × n from a bare menu lands on the n-th item from the bottom). `ss7`'s
  `MenuPickFromEnd` is the helper.
- **A popup needs time before it takes keys.** ~350 ms for a strip menu, ~600 ms for a slot's popup
  (the I/O slots, the insert slot's kinds, the send well's buses). Send `Key` after the sleep, not
  before.
- **A submenu opens with `Right`** on its parent item and lands on the submenu's first item; count
  `Down` from there.
- **A combo box's popup opens on its CURRENT item**, not the first — count relative to what is
  selected.
- **Native chooser input needs verified targeting.** `FileDialogEnter` verifies the exact dialog,
  filename control, focus and path readback before one Enter; scripts then assert the requested
  bundle path or imported clip in the probe, rather than treating dialog closure as success.
- **Resize to 1920×1080 before dock work** (`Resize 1920 1080`) so the strips paint their full slot
  columns; grow the dock by dragging `widget.shell.splitter.dock` upward (the default dock is the
  mini-mixer and drops slot rows).
- **Let the dock settle before dragging its splitter** (~400 ms after the tab switch): a drag that
  starts mid-layout is dropped and the dock stays at its default height.
- **The dock toggle (`X`) may need two presses** when another tab is in the dock: the first press hides
  the dock, the second shows the mixer. `ss7`'s `OpenMixer` waits on `view.dock`.
- **A painted fader press must land on the THUMB** (the rail's law is "grab the knob"); the probe's
  `mixer.strip.N.fader` is the rail — drag from the thumb's current position, not from an arbitrary
  point on the rail.
- **The recording device on the drive machine decides the input steps.** A device with no inputs makes
  the input-slot popup honest (no items) and the R cell refuse — scripts assert the refusal in that
  case instead of the pick (`ss7` Step 6).
- **Every assertion reads the probe**, never a screenshot: `mixer.strips[i].inserts / sends / input /
  output / armed`, `fxEditor.{visible, kind, strip, slot, rows}`, `ride.{active, samples}`,
  `recording.armedTrackCount`, `view.{dock, mixerNarrow, trackCount}`; for the lifecycle (SS-6):
  `project.{sampleRateHz, tempoBpm, trackCount, clipCount, midiClipCount, assets[].{hash, sampleRateHz, channels,
  frames}}`, `autosave.{enabled, intervalMs, writes, failures, lastWritten, recovery}`, `relink.{asking, name, refusal,
  damaged, questions, lastOutcome}`, `export.{lastResult, outcomes, destination, retiring}`. A drive shortens the
  autosave cadence with `YESDAW_AUTOSAVE_INTERVAL_MS` (250 .. 600000), and paces every export with
  `YESDAW_EXPORT_PACE_MS` (a pause after the first chunk, 250 .. 60000; `export.paceMs`) so it can cancel, refuse a
  second export and replace the project while a job is mid-write — `Launch -AutosaveIntervalMs` / `-ExportPaceMs`. If a step needs a fact the probe
  does not carry, add it to the probe (the shell's `mainComponentStateProbeJson`) — that is a code
  change with a gate, not a screenshot read.
- **File > New goes through the New Project dialog (G5.5).** New shows the in-app dialog (rate, tempo,
  meter, template); its Create opens the native location chooser. Use `NewProjectChooser` — a script that
  waits for the chooser straight after the click leaves the dialog open over the shell, and every later
  click and Tab lands in it (2026-10-07: SS-2..SS-5 went red this way at G6).
- **Pick a popup item by its text, not its position.** Menus grow (G5 put Save a Copy and Save as
  Template above Import MIDI File, and SS-5's "7th item" became Import Audio). `MenuPickByName` finds the
  item through UI Automation and clicks it; it FAILs when the item is missing or disabled.
- **JUCE popups have no Home key and its menu bar no Alt mnemonics.** A chooser's popup opens on its CURRENT item and
  ignores Home, so "Home, then n Downs" picks the wrong item (SS-6's first run created a 48 kHz project, not 44.1):
  pick by name (`MenuPickByName`). Open a menu by clicking its title in `widget.shell.menubar`, never Alt+F.
- **Never name a helper after a PowerShell alias.** Aliases win over functions: `Kill` ran `Stop-Process` with no id
  and waited on input forever (2026-10-07), as `Type` once did. The runner's are `KillApp` and `TypeText`.
- **UI Automation is what a screen reader reads.** `UiaFocused` after a router move is the painted
  control's own element (ADR-0066); compare its name / type / rect with the probe's `controlTarget` and
  `layout`, and act through its patterns (Toggle, RangeValue, SetFocus) as a screen reader would.
- **Shots are evidence, not assertions.** Name them for the rubric (`ss7-fx-editor`), take one per
  distinct state, and judge them against §7.4 in the STATUS story's **Rubric** paragraph.
