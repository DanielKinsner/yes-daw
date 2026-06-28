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

**Reviewed + hardened (2026-06-28, Claude).** Adversarial review of the close-out fixed one real hot-path
safety hole (unbounded `locate`/`setLoop` frames truncated to a hung/trapped audio thread) and four gates
that passed without biting (autosave negative control, a circular recording test, loop block-size
invariance, and `locate(N)` == offline-render-slice parity), and clarified the autosave helper's
control-thread affinity. **Known deferrals (tracked, out of H8's exercised surface):**
- **Concurrent transport safety** — transport state (`playheadFrame_`/loop/`playing_`) is plain non-atomic;
  safe single-threaded today (documented in code), but a data race once a control thread drives it
  concurrently with the audio callback. Needs a small ADR (SPSC command seam, ADR-0006 pattern) + a TSan
  test that drives `locate`/`setLoop` while another thread pumps `processBlock`. First H9 checkpoint.
- **MIDI transport** — `DecodedMidiClipNode` ignores `Transport::timelineFrame`, so MIDI desyncs on
  locate/loop once MIDI playback is wired into a runtime graph.
- **Transport tempo/meter** — `PlaybackEngine` leaves `Transport.tempoMap`/`meterMap` default; fine until a
  node reads them on the audio thread (the H7↔H8 bit-identity gate catches divergence meanwhile).
- **Stopped-on-create** — `PlaybackEngine` defaults to playing; real DAWs open stopped (UX, H11 wiring).

Local verification: `YesDawPlaybackCheck` = **9 cases / 271 assertions**; `cmake --build --preset ci`;
`ctest --preset ci --output-on-failure` = **239/239**.

**Next:** stop for Dan's H8 close-out review + the concurrent-transport decision; H9 needs its focused
plan/ADR work before H9 code lands.

## The plan

Full build order:
[`docs/plans/2026-06-28-h8-playback-runtime-plan.md`](../docs/plans/2026-06-28-h8-playback-runtime-plan.md).
