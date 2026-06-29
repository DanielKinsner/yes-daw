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

H10 kickoff docs are green on remote CI run `28340551455`: this horizon file, the live handoff, the
roadmap status, and `docs/plans/2026-06-28-h10-mixing-mastering-interchange-plan.md`.

ADR-0028 (loudness metering model) is accepted and green on remote CI run `28340956377`.
`YesDawLoudnessCheck` is implemented with the pinned `libebur128` wrapper, full local
`ctest --preset ci --output-on-failure` **241/241**, and remote CI run `28341446711` green on
`1d29c02`. The loudness remote-green docs are green on remote CI run `28341823599`.

ADR-0029 (DAWproject export subset) is accepted. `YesDawDawprojectCheck` writes a stored `.dawproject`
package with project/metadata XML plus canonical float32 WAV media, then verifies it through an
independent ZIP/XML/WAV summary reader for tracks, audio Clips, MIDI Clips, timing, gain/pan, source
windows, media paths, and decoded media bytes. Full local `ctest --preset ci --output-on-failure` is
**243/243**, the focused H10 regex is **2/2** for the currently landed gates, and remote CI run
`28348385319` is green on `910ea1c`. The next H10 checkpoint is ADR-0030 plus
`YesDawTimeStretchCheck`.

ADR-0030 (time-stretch Node) is accepted. H10 time-stretch uses pinned Signalsmith Stretch `1.1.0` to
prepare stretched clip/source audio on the control side, then exposes it through a source-style
`TimeStretchNode` whose audio-thread path is an absolute-frame read over immutable samples. ADR-0030 docs
are green on remote CI run `28349381664`.

`YesDawTimeStretchCheck` is locally green. It pins Signalsmith Stretch, validates control-side preparation,
checks exact duration and fixed-ratio golden fingerprints, and proves `TimeStretchNode` timeline/block-split
determinism, silence windows, fallback reset, and block-parallel-safe metadata. The focused H10 regex is
locally green **3/3** for the currently landed gates; full local `ctest --preset ci --output-on-failure`
is **244/244**. Remote CI run `28350136910` is green on `ad50721`. The next H10 checkpoint is ADR-0031
plus `YesDawDeviceHotSwapCheck`.

## The plan

Full build order:
[`docs/plans/2026-06-28-h10-mixing-mastering-interchange-plan.md`](../docs/plans/2026-06-28-h10-mixing-mastering-interchange-plan.md).
