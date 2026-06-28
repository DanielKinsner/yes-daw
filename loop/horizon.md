# Current horizon — H5 (Recording) — CLOSED

> This file is the oracle for "is the horizon done?". H5 closes iff the exit gate below is green.

## Exit criterion (the finish line)

A recorded take aligns within +/- 1 frame of a click reference at non-trivial input+output latency,
against deterministic ground truth.

**`YesDawRecordingCheck`** proves this with an in-repo deterministic loopback harness:

- Audio callback input is copied into a bounded `RecordingChunkFifo`.
- A writer thread drains the FIFO to a real temp take file; the audio callback never writes disk.
- The click impulse is captured at device frame `click + input latency + output latency`, then placed
  back at the original Project click frame after compensation.
- The missing-latency negative control runs the SAME capture→FIFO→writer→file→read pipeline with
  compensation removed and proves the recorded peak lands at the wrong (uncompensated) frame — so the
  gate bites if the subtraction is ever dropped, not just an arithmetic identity.
- Punch/loop recording produces stable take ordinals, `maxLoopTakes` rejects passes past the limit, and
  comp selection reads the requested take (across multiple segments, zero-filling gaps).
- Stereo (2-channel) capture round-trips each channel to its own frame; FIFO backpressure drops chunks,
  reports it, and keeps the accepted/dropped accounting exact.
- Direct-input recording (`includeOutputLatency=false`) compensates input latency only.
- The take-file reader rejects a missing file and a corrupt/truncated header.
- MIDI recording uses the same device-frame-to-timeline-frame compensation model as audio, filters
  out-of-window events, and reports an undersized output.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --test-dir build-ci -R "recorded take aligns|punch loop recording|MIDI recording uses" --output-on-failure
```

## Status: **CLOSED (H5 exit gate + full local ci green; review-commit remote CI is the gate)**

H5 opened on 2026-06-28 when Dan asked to begin and finish H5. ADR-0018 records the recording latency
and take-writer decision. The implementation is pure engine code in `src/engine/Recording.h`, with the
blocking gate in `tests/recording_tests.cpp` and target `YesDawRecordingCheck`. The initial close-out
(commit `92d8b7c`) was green on remote run `28309319816` (Windows, Linux, macOS, RTSan, TSan).

A later adversarial review hardened the gate (it had passed too easily): the latency negative control is
now a real broken-pipeline run, and the previously-unexercised stereo, backpressure (+ exact
accepted/dropped accounting), `maxLoopTakes`, direct-input, file-format-error, multi-segment comp, and
MIDI-edge paths each get a biting case; the audio-path mapping helpers now carry `YESDAW_RT_HOT` so RTSan
actually enforces nonblocking on them, and the gate's writer thread is exception-safe. Local
verification: `cmake --preset ci`; VS DevShell `cmake --build --preset ci`; focused H5 gate 9/9; full
`ctest --preset ci --output-on-failure` passed 231/231. The review commit's remote CI is the gate.

### Scope boundary (what "CLOSED" does and does not mean)

The roadmap H5 *exit criterion* — a recorded take aligned within ±1 frame of a click reference at
non-trivial input+output latency, against deterministic ground truth — is genuinely met and mechanically
gated. What is **not** built yet, and is deferred to H6+: `src/engine/Recording.h` has **no production
caller** — nothing in the Runtime, audio driver, `Main.cpp`, or Project surface invokes it (the gate
drives the spine from a test harness). Also deferred: latency-compensated **monitoring** (hearing input
through the app while recording), device arming/UI and real latency calibration, Project bundle take-lane
persistence, and the user-facing recorded-audio asset format (the `.ysdtake` file is an internal
deterministic test format with no playback-path decoder).

## The plan

Full build order:
[`docs/plans/2026-06-28-h5-recording-plan.md`](../docs/plans/2026-06-28-h5-recording-plan.md).
