# Current horizon — H6 (Reliability & polish) — CLOSED

> This file is the oracle for "is the horizon done?". H6 closes iff the exit gate below is green.

## Exit criterion (the finish line)

A heavy session runs 60 minutes at a 64-128 frame Block with zero Underruns and 99.9th percentile Block
time under the Block period; a hard kill mid-edit recovers to the last autosave with no corruption.

**`YesDawReliabilityCheck`** proves this with an in-repo deterministic harness:

- Builds a 100-track synthetic mixer session through `GraphBuilder` — each track is a real strip
  (DC source -> `FaderNode` gain ramp -> `MeterNode` tap), so the soak exercises the node `process()`
  paths, not a single constant store.
- Processes 60 minutes of audio frames at a 128-frame Block: 1,350,000 Blocks at 48 kHz.
- Records every Block's processing time and fails if the 99.9th percentile exceeds the Block period.
  A synthetic negative control proves the deadline oracle actually bites (over-budget, underrun, and
  empty soaks all fail `passesDeadline()`); the live session runs with a large margin by design, so
  real-device timing remains ADR-0005's hardware soak lane.
- Keeps the headless Underrun counter at zero — by design (no device callback), not a measured result;
  the real-device xrun lane remains ADR-0005's hardware soak.
- Writes a bundle-shaped last-good autosave snapshot under `autosave/last.yesdaw`, with a crash-safe
  publish (keeps `last.previous` until the new snapshot is fsync'd; recovery falls back to it so the
  two-rename window never leaves zero valid snapshots).
- Simulates a hard kill by abandoning an open edit transaction, which SQLite rolls back on next open.
  The headless gate does **not** exercise OS-level crash / hot-WAL recovery — that is the ADR-0005 soak.
- Restores the last autosave, then reopens the Project bundle through the normal integrity,
  foreign-key, asset-file, and semantic validators.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --test-dir build-ci -R "H6" --output-on-failure
```

## Status: **CLOSED (focused H6 gate + full local ci + remote CI green)**

H6 opened on 2026-06-28 after H5 was rechecked against current docs, local focused tests, and remote CI.
ADR-0019 records the H6 reliability gate decision. The implementation adds:

- `src/engine/Reliability.h` for deadline soak summaries.
- `src/persistence/AutosaveRecovery.h` for bundle-shaped last-good autosave snapshots and restore.
- `tests/reliability_tests.cpp` plus target `YesDawReliabilityCheck`.

Focused local verification: `ctest --test-dir build-ci -R "H6" --output-on-failure` passed 2/2.
Full local verification: VS DevShell `cmake --build --preset ci`; `ctest --preset ci` passed 233/233.
Remote verification: CI passed on the H6 implementation close-out commit.

### Scope boundary (what "CLOSED" does and does not mean)

The roadmap H6 *exit criterion* is mechanically gated: the current engine graph path processes a
100-track, 60-minute audio-frame workload under the 128-frame Block deadline at p99.9, and autosave
restore survives a simulated hard kill with normal bundle validation. The autosave write/restore
surface (`AutosaveRecovery.h`) is exercised by the gate but has **no production caller yet** — nothing
in the Runtime / audio driver / `Main.cpp` schedules an autosave or prompts a recovery on launch;
wiring a background autosave + a real crash-recovery flow is H6 follow-up / H7. What is **not** built
yet: the final multicore work-stealing scheduler, DAWproject export, loudness metering, time-stretch
Node, full accessibility, device hot-swap policy/UI, or a self-hosted 60-minute real-device soak. Those
are H6 follow-up product slices, not blockers for this exit gate.

## The plan

Full build order:
[`docs/plans/2026-06-28-h6-reliability-plan.md`](../docs/plans/2026-06-28-h6-reliability-plan.md).
