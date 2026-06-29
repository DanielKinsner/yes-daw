# Current horizon - H11 (Single-window timeline UI shell + accessibility) - OPEN

> This file is the oracle for "is the horizon done?". H11 opened on 2026-06-29 after H10 and the H10
> adversarial-review patch batch were remote-green on `main` (`dd3b257`, GitHub Actions run
> `28379340005`).

## Exit criterion (the finish line)

The first real YES DAW app replaces the H0 sine-spike window with a single-window native UI that loads a
Project bundle, draws and scrolls the timeline, drives H8 playback transport, exposes mixer/meter/piano
roll surfaces, and wires H7-H10 features through an agent-native action surface.

The H11 focused gates are:

- **`YesDawUiActionCheck`**: every shipped visible UI action has a stable action ID, label, default key
  binding where relevant, enabled/disabled reason, accessible role/name, and a command-layer
  implementation or explicit read-only query.
- **`YesDawAppSmokeCheck`**: the app model loads a `.yesdaw` Project bundle and drives play/stop/locate/loop
  through the same action IDs used by menus, buttons, shortcuts, and accessibility.
- **`YesDawTimelineGpuCheck`**: the Timeline canvas scrolls a large arrangement fixture with
  `max_frame_ms < 16.6`.
- **`YesDawAccessibilityCheck`**: visible controls have semantic roles/names, keyboard reachability, and
  action-registry backing.

The one human spot-check is visual feel through a single launch command after mechanical gates pass.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure
```

The focused regex becomes fully active as the H11 gate targets land.

## Status: OPEN

ADR-0032 (H11 UI stack and app shell) is accepted. H11 uses native JUCE Components for the app shell,
a dedicated Timeline canvas for dense rendering, and a UI action registry as the command/keymap/
accessibility seam. The main app shell does not use WebView.

Kickoff docs are green: local `cmake --preset ci`, VS DevShell `cmake --build --preset ci`, and
`ctest --preset ci --output-on-failure` **245/245**; remote CI run `28382745216` passed across Linux,
Windows, macOS, RTSan, and TSan.

The **App shell + action registry** checkpoint is remote-green: the H0 sine-spike window is replaced by
the native single-window shell, the visible toolbar consumes `UiActionRegistry`, and `YesDawUiActionCheck`
is in the full `ci` preset. Local gates are green: `cmake --preset ci`, VS DevShell
`cmake --build --preset ci`, `ctest --test-dir build-ci -R YesDawUiActionCheck --output-on-failure`, and
`ctest --preset ci --output-on-failure` **246/246**. Remote CI run `28385990090` passed across Linux,
Windows, macOS, RTSan, and TSan.

The **Project-load smoke + transport controls** checkpoint is remote-green: `UiAppModel` opens an existing
`.yesdaw` Project bundle, reads its Project snapshot, builds H8 playback from owned decoded audio, and
routes play/stop/locate/loop through the same `UiActionId`s as menus, buttons, shortcuts, and
accessibility. `YesDawAppSmokeCheck` is in the full `ci` preset. Local gates are green: VS DevShell
`cmake --build --preset ci --target YesDawAppSmokeCheck`,
`ctest --preset ci -R YesDawAppSmokeCheck --output-on-failure`, and VS DevShell full
`cmake --build --preset ci` + `ctest --preset ci --output-on-failure` **247/247**. Remote CI run
`28388490955` passed across Linux, Windows, macOS, RTSan, and TSan.

The **Timeline canvas GPU/perf** checkpoint is remote-green: `src/ui/TimelineCanvas.h` is the shared
native Timeline canvas used by the app shell and `YesDawTimelineGpuCheck`; the gate scrolls a 20,640-clip
arrangement fixture and measured `max_frame_ms=3.2874` with 336 visible clips. Local gates are green:
VS DevShell `cmake --build --preset ci --target YesDawTimelineGpuCheck`,
`ctest --preset ci -R YesDawTimelineGpuCheck --output-on-failure`, focused H11
`ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**3/3**, and full `ctest --preset ci --output-on-failure` **248/248**. Remote CI run `28391576711`
passed across Linux, Windows, macOS, RTSan, and TSan.

The **Timeline editing and clip affordances** checkpoint is remote-green: `UiActionRegistry` now exposes
selected-clip move/trim/split/gain/fade/time-stretch actions, `UiTimelineEditModel` maps them to the
existing Project edit/undo commands, and `YesDawUiActionCheck` proves action-to-command parity,
undo/redo, no-Project, no-selection, and failed-edit rejection. Local gates are green: `cmake --preset ci`;
VS DevShell `cmake --build --preset ci --target YesDawUiActionCheck`;
`ctest --preset ci -R YesDawUiActionCheck --output-on-failure`; focused H11
`ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**3/3**; and full `cmake --build --preset ci` + `ctest --preset ci --output-on-failure` **248/248**.
Remote CI run `28393896442` passed across Linux, Windows, macOS, RTSan, and TSan.

The **Mixer, meters, and loudness surface** checkpoint is remote-green: `UiActionRegistry` now exposes
track/bus fader, pan, mute, solo, meter-read, and loudness-read actions; `UiMixerSurface` projects
track/bus strips, sidechain-visible state, solo-safe/effective mute state, per-strip meter values, and H10
loudness readouts without changing Project or engine policy; and the app shell consumes that projection
for the mockup-aligned mixer and master loudness readout. Local gates are green: `cmake --preset ci`;
VS DevShell `cmake --build --preset ci --target YesDawUiActionCheck`;
`ctest --preset ci -R YesDawUiActionCheck --output-on-failure`; VS DevShell
`cmake --build --preset ci --target YesDaw`; focused H11
`ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure`
**3/3**; and full `cmake --build --preset ci` + `ctest --preset ci --output-on-failure` **248/248**.
Remote CI found macOS timing reds in pre-existing perf/deadline gates; the dense Timeline clip paint fix
and macOS scheduler fixture adjustment are green on remote CI run `28398414664` across Linux, Windows,
macOS, RTSan, and TSan.

The **Piano roll and MIDI Clip surface** checkpoint is remote-green: `UiActionRegistry` now exposes Note
selection, move, length, transpose, quantize, and expression-lane readback actions; `UiPianoRollSurface`
projects H4 MIDI Clips/Notes and applies edits through `ProjectUndoStack`; and the app shell paints a
Piano Roll panel from that snapshot shape. Local gates are green: VS DevShell
`cmake --build --preset ci --target YesDawUiActionCheck`;
`ctest --preset ci -R YesDawUiActionCheck --output-on-failure`; VS DevShell
`cmake --build --preset ci --target YesDaw`; focused H11
`ctest --preset ci -R "YesDaw(UiAction|AppSmoke|TimelineGpu)Check" --output-on-failure` **3/3**; and
full `cmake --build --preset ci` + `ctest --preset ci --output-on-failure` **248/248**. Initial remote CI
run `28400668189` failed Linux/macOS build on missing `UiAppModel::dispatch` switch cases for the new
Piano Roll action IDs; follow-up commit `61efd1a` fixed the switch. Remote CI run `28401313658` is green
across Linux, Windows, macOS, RTSan, and TSan.

The next checkpoint is **Accessibility pass + launch script**: visible controls have semantic roles/names,
keyboard reachability, action-registry backing, and the one-command launch Dan uses for visual-feel review.

## The plan

Full build order:
[`docs/plans/2026-06-29-h11-single-window-timeline-ui-plan.md`](../docs/plans/2026-06-29-h11-single-window-timeline-ui-plan.md).
