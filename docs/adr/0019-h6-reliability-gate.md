# 0019. H6 reliability gate

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent (H6)
- **Related:** ADR-0002 (real-time engine foundations), ADR-0005 (mechanical verification),
  ADR-0006 (immutable snapshot concurrency), ADR-0012 (SQLite bundle schema and atomicity),
  [roadmap](../goals/roadmap.md), `CONTEXT.md`.

## Context

H6 is broad: reliability, polish, device hardening, multicore scheduling, interchange, metering,
time-stretch, accessibility, soak, and fuzz. The roadmap exit is narrower and must stay mechanical:

- a heavy session represents 60 minutes of audio at a 64-128 frame block with zero underruns and 99.9th
  percentile block time under the block period;
- a hard kill mid-edit recovers to the last autosave with no corruption.

The project already has lower-level durability pieces from H1/H2: SQLite transactional rollback,
migration rerun, semantic validation, asset intent reconciliation, and bundle open-time integrity checks.
H6 needs a user-facing last-good autosave contract plus a headless deadline oracle that CI can run without
audio hardware or human listening.

## Options considered

1. **Make H6 a large feature sweep.**
   - Pros: covers every roadmap bullet at once.
   - Cons: too broad for a bisectable checkpoint; mixes product polish with the irreversible reliability
     gate. Rejected.
2. **Use a real-time, wall-clock, 60-minute device soak as the only H6 gate.**
   - Pros: closest to live use.
   - Cons: blocks cloud CI and reintroduces a hardware-only finish line. Rejected as the only gate.
3. **Add a deterministic headless reliability gate, with real hardware soak remaining an additional lane.**
   - Pros: CI can assert the exit contract without a device; the gate exercises the current engine graph
     path and the Project bundle recovery path. Accepted.

## Decision

**H6 closes on a deterministic `YesDawReliabilityCheck` gate.**

- The deadline half builds a heavy synthetic engine session and processes 60 minutes of audio frames at a
  64-128 frame block. It records per-block processing times and fails unless the 99.9th percentile stays
  under the block period. The headless harness has no device callback, so the underrun counter is
  deterministically zero; real-device xrun checks remain the hardware soak lane from ADR-0005.
- The recovery half writes a last-good autosave snapshot into the bundle's `autosave/` area, simulates a
  hard kill during a later edit by abandoning an open transaction, then restores the last autosave and
  reopens the bundle through the normal integrity, foreign-key, asset, and semantic validators.
- Autosaves are bundle-shaped snapshots, not ad hoc partial rows. Asset bytes referenced by the autosaved
  Project are copied into the autosave snapshot before it is accepted, so the normal open-time validation
  can reject missing or corrupt references.

## Consequences

- **Positive:** H6 gets a mechanical, cloud-runnable exit gate. The autosave path is recoverable by code,
  not by asking Dan to inspect a file. The deadline path is measured against the same graph compiler and
  `CompiledGraph::process` hot path used by earlier horizons.
- **Negative / accepted costs:** this is not the final multicore work-stealing scheduler, DAWproject export,
  loudness metering, time-stretch, accessibility, or device hot-swap implementation. Those remain product
  follow-ups after the H6 exit contract is green.
- **Follow-ups:** add the true multicore scheduler, extend the soak lane to a self-hosted 60-minute
  real-device run, and build the remaining H6 polish features as separate ADR-backed checkpoints.
