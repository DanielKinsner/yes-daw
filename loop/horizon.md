# Current horizon — H8 (Playback runtime: device I/O + transport) — OPEN

> This file is the oracle for "is the horizon done?". H8 closes iff the exit gate below is green.

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
  and freezes the playhead. **[ADR-0022 + later checkpoints]**
- Recording (H5) driven from the transport aligns a take to the click reference; autosave (H6) fires on a
  transport/edit tick and recovers through the normal validators. **[later checkpoints]**

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --test-dir build-ci -R "YesDawPlaybackCheck" --output-on-failure
```

## Status: **OPEN — checkpoint 1 (play-from-0) landed + reviewed; CI green**

H8 opened at the H7->H8 boundary (no stop — headless, per Dan). ADR-0020 sequences H7–H11 (UI is the H11
capstone). Checkpoint 1 is in: `buildProjectGraph` is shared between offline render and playback;
`src/engine/PlaybackEngine.h` plays a Project through the real `Runtime` / `RuntimeAudioDriver`; and
`YesDawPlaybackCheck` proves it against the independent reference, block-size independent, and bit-for-bit
equal to the offline render. Local: `YesDawPlaybackCheck` = 3 cases / 41 assertions; `ctest --preset ci`
= 239/239. **Next checkpoints:** ADR-0022 (transport model) -> play/stop/locate/loop -> recording +
autosave production callers -> the tracked real-device smoke (absorbs the open H0 soak).

## The plan

Full build order:
[`docs/plans/2026-06-28-h8-playback-runtime-plan.md`](../docs/plans/2026-06-28-h8-playback-runtime-plan.md).
