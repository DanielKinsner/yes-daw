# 0027. Scheduler block-parallel safety guard

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0007 (CompiledGraph), ADR-0008 (Node contract), ADR-0024 (deterministic engine
  scheduler), ADR-0020 (H7–H11 roadmap), H9 engine scaling plan.

## Context

ADR-0024's scheduler renders Project Blocks in parallel across independent `CompiledGraph` snapshots,
dispatching Blocks to workers out of order. That is only correct when every node's output for a Block
depends solely on the absolute transport frame and that Block's inputs — i.e. the node carries **no
cross-Block state**. Today's Project graph satisfies this (clip sources keyed by absolute frame, faders/pans
settled to constants, the impulse instrument at zero latency), so the H9 determinism gate is green.

But nothing *enforces* it. A node with cross-Block state — a feedback `DelayNode` ring, a PDC `LatencyNode`
ring, an automated fader mid-ramp, a hosted plugin, an instrument with non-zero latency — would be
**silently mis-rendered** if routed through the scheduler, because each worker advances that state in
whatever Block order it happened to steal. The H9 review flagged this as a latent landmine for H10
(loudness/time-stretch/device features) and H11: the moment an effect node lands, `renderProjectWithScheduler`
could quietly diverge from the serial render, and a bisect would blame the new effect, not the missing guard.

## Options considered

1. **Document the constraint, no code.** Cheapest, but relies on every future node author remembering — the
   exact failure the project's "verification is mechanical" rule rejects. Rejected.
2. **Default-true safety bit (opt out for known-stateful nodes).** Less churn, but a *new* stateful node
   defaults to "safe" and reintroduces the silent landmine. Rejected.
3. **Default-false safety bit (opt in), aggregated to the graph, enforced by the scheduler.** Every node
   declares whether it is block-parallel-safe; the default is unsafe, so a new node is refused until proven.
   The scheduler refuses any graph that isn't all-safe. Fail-safe and mechanical. Accepted.

## Decision

Add `NodeProperties::blockParallelSafe` (default **false**). Each node opts in only if its `process()` output
is independent of Block dispatch order under the scheduler's calling convention (absolute transport frame,
empty per-Block event stream).

- **Marked safe (true):** `DecodedClipNode`, `DecodedMidiClipNode`, `SumNode`, `FaderNode`, `PanNode`,
  `MeterNode`, `MasterNode`, `IdentityDcNode`, and `ImpulseInstrumentNode` **only when its latency is 0**.
  These are exactly the nodes in the currently-green determinism graph — empirical evidence they are
  order-independent.
- **Left unsafe (false):** `DelayNode`, `LatencyNode`/PDC splices, `PluginNode`, `SidechainGainNode`,
  `OscillatorNode`, `MidiEffectNode`, `PlaceholderNode`, and any future node until proven.
- `GraphBuilder` aggregates the AND of every node's bit into `CompiledGraph::isBlockParallelSafe()`.
- `renderProjectWithScheduler` returns the new `OfflineRenderStatus::GraphNotBlockParallelSafe` for any graph
  that is not all-safe. Stateful graphs must use the serial `renderOfflineProject` until the per-node DAG
  scheduler (the ADR-0024 follow-up) lands.

## Consequences

- **Positive:** the H10 landmine becomes a build-time refusal instead of silent wrong audio. A new effect
  node is unsafe by default and cannot be block-parallelized until someone proves and marks it.
- **Positive:** the guard is mechanical — a focused test asserts the Project graph is safe and a graph with a
  `DelayNode` is not.
- **Negative / accepted:** graphs with any unsafe node (sidechain, hosted plugins, future effects) fall back
  to the serial renderer for now — correct, just not yet parallel. The per-node scheduler removes that limit.
- **Follow-up:** when the per-node DAG scheduler + parallel buffer pool land (ADR-0024 follow-up), revisit
  whether stateful nodes can be parallelized within a live graph rather than refused.
