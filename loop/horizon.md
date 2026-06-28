# Current horizon — H9 (Engine scaling & robustness) — CLOSED

> This file is the oracle for "is the horizon done?". H9 closed locally on 2026-06-28.

## Exit criterion (the finish line)

The engine gains its first deterministic parallel scheduler and the robustness debts called out at the
H8 boundary are paid down mechanically: concurrent transport controls are safe to drive against the audio
callback, scheduled render workers produce bit-identical output, the parallel scheduler feeds the H6
deadline oracle, parser fuzz replays stay clean, plugin failures persist blacklist actions, and MIDI clips
auto-wire into the Project mixer with transport-aware locate/loop behavior.

**`YesDawSchedulerCheck`** is the headless CI gate:

- Deterministic scheduled render output is bit-identical across 1, 2, 4, and 8 workers and matches the H7
  serial offline render. **[done: ADR-0024]**
- A negative control proves arrival-order-dependent floating-point summing changes bits. **[done]**
- `PlaybackEngine` transport controls run through an SPSC command queue so the audio thread owns live
  transport state while a control thread drives `locate`/`setLoop`/`stop`. **[done: ADR-0023]**
- Project MIDI clips auto-wire through a built-in impulse instrument and follow `Transport::timelineFrame`
  for locate/loop parity. **[done: ADR-0026]**
- Parallel scheduled Blocks feed the H6 deadline oracle; seeded parser mutations and corrupt plugin-state
  rows are rejected/degraded cleanly; plugin crash/watchdog failure actions persist blacklist rows across
  reopen. **[done: ADR-0025]**

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R "YesDawSchedulerCheck" --output-on-failure
```

## Status: **CLOSED — local headless gate green**

H9 is implemented by ADR-0023 through ADR-0026. `PlaybackEngine` now posts transport commands through a
bounded lock-free SPSC queue drained at the top of `processBlock`. `GraphScheduler` partitions scheduled
render Blocks across worker threads while keeping each immutable graph snapshot's internal sum order
canonical, so multicore execution cannot change samples for the current Project surface. `OfflineRenderer`
now projects MIDI clips into the mixer through a built-in impulse instrument, and `DecodedMidiClipNode`
honors absolute transport frames for located and looped playback.

Robustness coverage landed in the same gate: scheduled Blocks feed the H6 soak oracle, deterministic
seeded mutations cover bundle and plugin-state parser degradation, and plugin crash/watchdog failure
actions write durable blacklist rows keyed by plugin identity. The live host coordinator still needs stable
plugin-identity plumbing before it can run that persistence action automatically from a child-process
failure.

Honest scope: ADR-0024 deliberately chose scheduled render jobs over immutable graph snapshots as the
first scheduler. Per-node DAG work-stealing inside one live `CompiledGraph` still needs a parallel-aware
buffer-pool allocation plan and remains the next scheduler deepening.

**Reviewed + hardened (2026-06-28, Claude).** Adversarial review replaced a tautological determinism
negative control (float math that never ran the scheduler) with one that drives the real graph, and gave
the 100-track soak a deterministic parallel↔serial bit-identity check at scale. **Known deferrals
(tracked):**
- **Scheduler is stateless-only, ungated (the gating decision before H10).** Block-level dispatch is
  correct only when every node is keyed by absolute frame; a DelayNode/automated-fader/PDC/instrument-
  pending graph is silently mis-rendered and the all-stateless determinism fixture can't catch it. Land a
  `blockParallelSafe` node property + a scheduler refusal + a DelayNode test (small ADR-0024 follow-up)
  before H10 wires stateful effects.
- **Transport concurrency round 2.** Control-side getters read non-atomic fields the audio thread writes
  (UB on a concurrent UI read); the SPSC queue is single-writer. Make the transport fields atomic + add a
  concurrent-reader TSan test before H11 binds a playhead readout.
- **Blacklist is the persistence action only** (no live crash triggers it — H3 wiring debt unmoved); the
  "parser fuzz" is a hand-written validator-regression corpus, not byte-level fuzzing.

Local verification: `cmake --preset ci`; VS DevShell `cmake --build --preset ci`; focused H8/H9 lane
`YesDawMidiTimingCheck|YesDawOfflineRenderCheck|YesDawPlaybackCheck|YesDawSchedulerCheck` = **4/4**; full
`ctest --preset ci --output-on-failure` = **240/240**.

**Next:** push; remote CI is the gate; then Dan's call on the stateless-graph scheduler guard before H10
opens with mixing/mastering features and interchange (`docs/goals/roadmap.md`).

## The plan

Full build order:
[`docs/plans/2026-06-28-h9-engine-scaling-robustness-plan.md`](../docs/plans/2026-06-28-h9-engine-scaling-robustness-plan.md).
