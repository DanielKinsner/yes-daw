# Current horizon — H8 (Playback runtime: device I/O + transport) — CLOSED

> This file is the oracle for "is the horizon done?". H8 closed locally on 2026-06-28.

## Exit criterion (the finish line)

A Project plays through the real lock-free `Runtime` / `RuntimeAudioDriver` production path behind a
transport, mechanically proven against the same independent reference H7 uses; recording (H5) and autosave
(H6) get their first production callers. The only part needing real hardware — literal device output with
zero Underruns — is a tracked one-command smoke (ADR-0005 pattern), **not** a CI gate.

**`YesDawPlaybackCheck`** is the headless CI gate (grows checkpoint by checkpoint):

- Plays a known Project through the real `Runtime` (publish a graph, drain the command queue, pump
  `processDeviceBlock`) and asserts the output equals the **independent reference** (Clips summed at their
  timeline positions with linear fade, gain, center pan) — not the engine vs itself. **[done]**
- Proves the played output is block-size independent (bit-identical across device block sizes) and matches
  the offline render bit-for-bit; the Runtime frees the graph on teardown. **[done]**
- Transport: `locate(N)` plays the reference from frame N; a loop region repeats it; `stop` zeroes output
  and freezes the playhead. **[done: ADR-0022]**
- Recording (H5) driven from the transport aligns a take to the click reference; autosave (H6) fires on a
  transport/edit tick and recovers through the normal validators. **[done]**

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --test-dir build-ci -R "YesDawPlaybackCheck" --output-on-failure
```

## Status: **CLOSED — local headless gate green**

H8 opened at the H7->H8 boundary (no stop — headless, per Dan) and is now complete locally. ADR-0022
accepted the absolute-frame transport model. `src/engine/PlaybackEngine.h` now plays a Project through the
real `Runtime` / `RuntimeAudioDriver`, supports play/stop/locate/loop, drives H5 recording capture from
the transport playhead, and exposes an edit revision for H6 autosave ticks. `persistence/PlaybackAutosave.h`
writes autosaves from that tick through the normal H6 bundle validators. `tools/playback-smoke.ps1` /
`tools/playback-smoke.sh` run the tracked real-device smoke through `YesDawSoak --playback-project`; this
is build-checked locally and remains an owner-machine hardware smoke, not CI.

Local verification: `YesDawPlaybackCheck` = 6 cases / 125 assertions; `cmake --build --preset ci`; `ctest
--preset ci --output-on-failure` = 239/239.

**Next:** stop for Dan's H8 close-out review; H9 needs its focused plan/ADR work before H9 code lands.

## The plan

Full build order:
[`docs/plans/2026-06-28-h8-playback-runtime-plan.md`](../docs/plans/2026-06-28-h8-playback-runtime-plan.md).
