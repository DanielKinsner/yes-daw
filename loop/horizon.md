# Current horizon - H12 (Operable Session UX) - OPEN

> This file is the oracle for "is the horizon done?". H12 opened on 2026-06-29 after H11 closeout was
> remote-green on `main` (`e9436af`, GitHub Actions run `28405529686`).

## Exit criterion (the finish line)

The H11 native app shell becomes an operable session workflow. A user or harness can create/open/save a
Project bundle, import a WAV into the bundle as an immutable Asset, create and edit timeline Clips, edit
MIDI Clip Notes in the Piano roll, adjust inspector and mixer values, drive play/stop/locate/loop, use
undo/redo, save, reopen, and prove the same Project state mechanically.

The H12 focused gates are:

- **`YesDawUiInputCheck`**: a self-asserting UI input harness creates or opens a `.yesdaw` Project,
  imports a WAV, edits Clips/Notes/mixer state, drives transport, saves, reopens, and asserts parity.
- **`YesDawUiActionCheck`**: every H12 visible mutation has action/input parity, enabled/disabled reasons,
  command/query backing, undo/redo coverage where applicable, and negative controls.
- **`YesDawAppSmokeCheck`**: the app model keeps opening Projects and driving H8 transport through the same
  action IDs used by the UI.
- **`YesDawTimelineGpuCheck`**: Timeline input wiring does not regress the dense Timeline frame-time gate.
- **`YesDawAccessibilityCheck`**: new H12 controls keep stable IDs, names, roles, keyboard reachability,
  and action/input backing.

Plugin scanner, plugin validation, plugin editor embedding, and third-party plugin insert UX are deferred
unless an H12 gate proves a hard dependency.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R YesDawUiActionCheck --output-on-failure
ctest --test-dir build-ci -R YesDawAppSmokeCheck --output-on-failure
ctest --test-dir build-ci -R YesDawTimelineGpuCheck --output-on-failure
ctest --test-dir build-ci -R YesDawAccessibilityCheck --output-on-failure
```

The `YesDawUiInputCheck` target lands during H12. Add it to the focused lane as soon as it exists.

## Status: OPEN

ADR-0033 (H12 operable session UX) is accepted. The first checkpoint is docs-only: ADR-0033, the H12
focused plan, roadmap/status/horizon updates, ADR index, and glossary terms. No H12 implementation code
has landed yet.

## The plan

Full build order:
[`docs/plans/2026-06-29-h12-operable-session-ux-plan.md`](../docs/plans/2026-06-29-h12-operable-session-ux-plan.md).
