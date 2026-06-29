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

The **App shell + action registry** checkpoint is local-green and remote-CI pending: the H0 sine-spike
window is replaced by the native single-window shell, the visible toolbar consumes `UiActionRegistry`, and
`YesDawUiActionCheck` is in the full `ci` preset. Local gates are green: `cmake --preset ci`, VS DevShell
`cmake --build --preset ci`, `ctest --test-dir build-ci -R YesDawUiActionCheck --output-on-failure`, and
`ctest --preset ci --output-on-failure` **246/246**.

The next checkpoint after remote CI is **Project-load smoke + transport controls**: wire `.yesdaw` Project
bundle loading and H8 playback transport through the same action IDs, then land `YesDawAppSmokeCheck`.

## The plan

Full build order:
[`docs/plans/2026-06-29-h11-single-window-timeline-ui-plan.md`](../docs/plans/2026-06-29-h11-single-window-timeline-ui-plan.md).
