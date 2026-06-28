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
- A zero-compensation negative control proves the gate would catch the missing-latency bug.
- Punch/loop recording produces stable take ordinals and comp selection reads the requested take.
- MIDI recording uses the same device-frame-to-timeline-frame compensation model as audio.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --test-dir build-ci -R "recorded take aligns|punch loop recording|MIDI recording uses" --output-on-failure
```

## Status: **CLOSED (H5 exit gate and full ci preset green locally; remote CI pending after push)**

H5 opened on 2026-06-28 when Dan asked to begin and finish H5. ADR-0018 records the recording latency
and take-writer decision. The implementation is pure engine code in `src/engine/Recording.h`, with the
blocking gate in `tests/recording_tests.cpp` and target `YesDawRecordingCheck`. Local verification:
`cmake --preset ci`; VS DevShell `cmake --build --preset ci`; focused H5 gate 3/3; full
`ctest --preset ci --output-on-failure` passed 225/225.

## The plan

Full build order:
[`docs/plans/2026-06-28-h5-recording-plan.md`](../docs/plans/2026-06-28-h5-recording-plan.md).
