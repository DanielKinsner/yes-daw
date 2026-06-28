# 0024. Deterministic engine scheduler

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0002, ADR-0006, ADR-0007, ADR-0020, H9 engine scaling and robustness plan.

## Context

H7/H8 made one Project render and play correctly, but the engine still needs a scaling model that cannot
change the samples. The hard part is not starting threads; it is keeping floating-point order and graph
state deterministic while work is spread across cores.

## Options considered

1. **Arrival-order node execution.**
   - Pros: simple work queue.
   - Cons: summing and state publication can depend on thread timing, so bit identity is lost. Rejected.
2. **Static topological levels inside one graph.**
   - Pros: close to the eventual realtime DAG scheduler.
   - Cons: the current buffer-pool allocator was built for serial liveness; widening it safely requires a
     parallel-aware slot plan.
3. **Deterministic scheduled render jobs over immutable graph snapshots now, then widen the buffer pool.**
   - Pros: immediately proves the H9 determinism contract over real Project graphs and keeps summing order
     inside each graph fixed by ADR-0007.
   - Cons: it is a first scheduler, not the final per-node work-stealing executor. Accepted for H9's gate.

## Decision

H9 introduces a deterministic scheduler that partitions render Blocks as jobs and runs them through
immutable `CompiledGraph` snapshots with explicit absolute transport frames.

- Worker counts of 1, 2, 4, and 8 must produce bit-identical interleaved output.
- The serial H7 offline render remains the reference.
- The scheduler owns job dispatch; each graph's internal sum order remains the compiled, canonical order.
- The negative control is an intentionally arrival-order-dependent sum that must differ from canonical
  summing, proving the gate can bite nondeterminism.
- The next scheduler deepening is a parallel-aware CompiledGraph buffer-pool plan, after the current gate
  protects bit identity.

## Consequences

- **Positive:** multicore execution is introduced behind a bit-identity gate before the UI depends on it.
- **Positive:** transport-frame-aware source nodes make scheduled render chunks independent for current
  audio and MIDI source surfaces.
- **Negative / accepted costs:** stateful cross-block effects are still rendered through the serial graph
  path until the per-node scheduler and parallel buffer pool land.
- **Follow-ups:** add a scheduler-specific buffer allocation mode before parallelizing nodes inside one
  live `CompiledGraph`.
