# Current horizon - H13 (Recording and device UX) - CLOSED REMOTE-GREEN

> This file is the oracle for "is the horizon done?". H13 opened on 2026-06-30 after H12 closeout was
> remote-green on `main` (`2dbb257`, GitHub Actions run `28459661398`).

## Exit criterion (the finish line)

A scripted record flow can select a test/fake device, arm a Track, choose a monitoring policy, record audio
and MIDI through the shipped `MainComponent`, create bundle-persistent Take/Clip data, save, reopen, drive
autosave recovery restore/discard choices, and prove recording alignment plus device survival mechanically.

The H13 focused gates are:

- **`YesDawRecordingUxCheck`**: the shipped-shell harness for device selection, Track arming, monitoring
  policy, audio/MIDI recording, Take/Clip persistence, take-lane/Comp basics, save/reopen, and autosave
  recovery. If implementation folds this into an existing target, update this file in the same checkpoint.
- **`YesDawRecordingCheck`**: the H5 recording alignment gate stays green, including latency-compensation
  negative controls.
- **`YesDawDeviceHotSwapCheck`**: the H10 control-side device survival gate stays green as H13 wires device
  selection/refresh into the UI.
- **`YesDawUiInputCheck`**, **`YesDawUiActionCheck`**, **`YesDawAppSmokeCheck`**,
  **`YesDawTimelineGpuCheck`**, and **`YesDawAccessibilityCheck`**: H12/H11 shipped-shell, action,
  smoke, frame-time, and accessibility gates remain green as recording controls become real Components.

Plugin scanner, plugin validation, plugin editor embedding, advanced comping, driver crash recovery,
sample-rate/channel remap on device changes, and visual-only polish are deferred unless an H13 gate proves
a hard dependency.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R YesDawRecordingUxCheck --output-on-failure
ctest --test-dir build-ci -R YesDawRecordingCheck --output-on-failure
ctest --test-dir build-ci -R YesDawDeviceHotSwapCheck --output-on-failure
ctest --test-dir build-ci -R "YesDaw(UiInput|UiAction|AppSmoke|TimelineGpu|Accessibility)Check" --output-on-failure
```

`YesDawRecordingUxCheck` is part of the focused lane from the H13 recording UX skeleton checkpoint.

## Status: CLOSED REMOTE-GREEN

ADR-0035 (H13 recording and device UX) is accepted by the kickoff docs checkpoint. The focused H13 plan is
`docs/plans/2026-06-30-h13-recording-device-ux-plan.md`. H13 implementation is complete on `main`:
the shipped-shell recording harness, deterministic test-device selection/refresh, Track arming,
canonical recorded-WAV Assets, schema-v5 Take metadata, shipped-shell deterministic MIDI recording,
scripted monitoring policy plus fake-device latency calibration, schema-v6 take-lane/Comp basics, and
autosave recovery restore/discard choices are implemented as small checkpoints. The final implementation
checkpoint is remote-green on `main` (`43280d8`, GitHub Actions run `28693226908`) across Linux, Windows,
macOS, RTSan, and TSan.

## The plan

Full build order:
[`docs/plans/2026-06-30-h13-recording-device-ux-plan.md`](../docs/plans/2026-06-30-h13-recording-device-ux-plan.md).
