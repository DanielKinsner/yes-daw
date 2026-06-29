# Current horizon - H10 (Mixing/mastering features & interchange) - OPEN

> This file is the oracle for "is the horizon done?". H10 opened on 2026-06-28
> after H9 remote CI went green on `a5a1db4` (run `28339991428`).

## Exit criterion (the finish line)

The headless engine gains the mixing/mastering and interchange feature set that H11 will surface:
loudness metering, DAWproject export, a time-stretch Node, and device hot-swap survival. Each feature
lands as its own ADR-backed checkpoint with a mechanical gate.

The H10 focused gates are:

- **`YesDawLoudnessCheck`**: loudness metering matches the libebur128/BS.1770 reference within tolerance,
  with biting controls for channel weighting, silence, and non-finite input.
- **`YesDawDawprojectCheck`**: a Project exports to DAWproject and round-trips through an independent
  reference reader for tracks, clips, timing, gain/pan, and asset references.
- **`YesDawTimeStretchCheck`**: the time-stretch Node preserves timing and produces samples that match a
  checked-in golden/reference for fixed ratios, block splits, and edge cases.
- **`YesDawDeviceHotSwapCheck`**: a simulated device change during playback is survived without an
  Underrun, with frame continuity and deterministic recovery.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R "YesDaw(Loudness|Dawproject|TimeStretch|DeviceHotSwap)Check" --output-on-failure
```

The focused regex becomes fully active as the four H10 gate targets land.

## Status: OPEN

H10 kickoff docs are landing first: this horizon file, the live handoff, the roadmap status, and
`docs/plans/2026-06-28-h10-mixing-mastering-interchange-plan.md`.

No H10 feature code has landed yet. The first feature checkpoint is the loudness-metering ADR and gate.

## The plan

Full build order:
[`docs/plans/2026-06-28-h10-mixing-mastering-interchange-plan.md`](../docs/plans/2026-06-28-h10-mixing-mastering-interchange-plan.md).
