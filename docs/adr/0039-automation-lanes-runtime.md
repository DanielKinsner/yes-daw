# 0039. Automation lanes: storage, targeting, and the compiled runtime path

- **Status:** Accepted (2026-07-03, after the Codex adversarial review was applied)
- **Date:** 2026-07-03
- **Deciders:** Dan + Fable 5 (2026-07-03 planning session)
- **Related:** ADR-0009 (event stream + automation curves — implemented here, not changed),
  ADR-0038 (ParamSpec — the mapping these lanes drive), ADR-0010 (ticks/tempo), ADR-0006
  (snapshot concurrency), ADR-0037 (re-carve), plan:
  [`2026-07-03-h15-automation-plan.md`](../plans/2026-07-03-h15-automation-plan.md),
  amended per the adversarial review
  [`2026-07-03-adversarial-review-h14-h17-packet.md`](../reviews/2026-07-03-adversarial-review-h14-h17-packet.md)
  (findings 1, 2, 6)

## Context

The audit of HEAD (2026-07-03) shows automation is three disconnected pieces: (1)
`src/engine/Automation.h` — lane/point types and block-sliced evaluators that emit
`ParameterChange` events, with no caller outside tests; (2) a schema-v1 `automation_points`
table with **no reader or writer anywhere**; (3) `FaderNode`, the only node that consumes
parameter events on the audio thread. There is no `Project` field, no persistence path, and no
runtime pass connecting lanes to the engine. H15 must connect them without violating the frozen
contracts: sample-accurate block-sliced events (ADR-0009), tick as the stored time authority
(ADR-0010), and an audio thread that never allocates or takes locks (ADR-0002).

Hard to reverse: lane/breakpoint rows enter saved user projects; the targeting scheme (how a lane
names the thing it automates) must survive future mixer-projection changes; and the runtime
evaluation strategy determines whether automation is sample-accurate forever or block-quantized
forever.

## Options considered

1. **Targeting — persist `NodeId` (as the v1 table does) vs persist Entity + role.** NodeIds are
   derived FNV hashes of projection structure; they change when the projection changes (as it
   will in H14's per-Track consolidation). *Chosen: lanes target `(owner EntityId, target role,
   ParamID)` — e.g. (Track X, Fader, gain) or (FxInsert Y, Node, param 3) — resolved to a NodeId
   at graph-build time. The orphaned v1 `automation_points` table is left in place (append-only
   migrations) and documented as superseded; it never had a writer, so no data exists.*
2. **Runtime evaluation — control-thread posting vs compiled-into-Snapshot.** Control-thread
   evaluation (post events/scalars per block) breaks sample-accuracy whenever the control thread
   stalls, and contradicts "the audio thread owns live transport state." *Chosen: lanes compile
   into the Snapshot: at build time, breakpoints convert tick→frame through `CompiledTempoMap`
   into flat, sorted, frame-domain arrays; the audio thread walks a preallocated cursor per lane
   and emits normalized `ParameterChange` events into a dedicated automation side-band (see
   Decision — NOT the root event slot) — no allocation, no locks, locate/loop = binary-search
   cursor reset.*
3. **Curve fidelity — dense per-sample events vs fixed control-interval emission vs extending the
   event payload with ramp targets.** Extending `ParameterChangePayload` changes a frozen
   contract (ADR-0009); per-sample events blow the per-block event budget. *Chosen: emit along
   curve segments every 64 samples (plus exactly at breakpoints), letting the consuming node's
   ParamSpec smoothing bridge the gaps. The approximation is bounded and mechanically asserted:
   worst-case error ≤ max(segment slope × 6 ms, 1e-4 normalized). A future ADR may add ramp-target
   events; nothing here blocks it.*
4. **Curve types in storage — all four evaluator types vs Linear+Hold.** *Chosen: Linear + Hold
   only in alpha storage (CHECK-constrained); Bezier/Log stay evaluator-supported but
   storage-rejected until an ADR accepts them (append-only widening later).*

## Decision

- **Storage (the next free schema version after H14's — verify at kickoff; H13 is still open):**
  `automation_lanes(id, owner_entity, target_role, param_id)` unique per target, +
  `automation_breakpoints(lane_id, tick, value CHECK 0..1, curve_type CHECK IN (0,1))`, values
  normalized per ParamSpec. `Project` gains `automationLanes`; full undo verbs through the
  command-diff pattern; open-time validators reject orphan targets, unsorted/duplicate ticks,
  out-of-range values.
- **Runtime:** compiled frame-domain lanes in the Snapshot; RT cursor walk; control-interval
  event emission (64 samples) + exact breakpoint events. **Delivery is a dedicated automation
  side-band**: an additive `automationEvents` `EventStream` view on `ProcessArgs`, carrying the
  block's evaluated automation events to **every** node, each filtering by its
  `NodeId`+`ParamID` (the existing FaderNode pattern). Root-slot injection is rejected:
  `GraphBuilder::eventInputSlotFor` hands a node its upstream producer's event slot whenever one
  exists, so root injection would silently miss any consumer downstream of an event-producing
  node — automation would work on plain audio tracks and fail on instrument tracks (review
  finding 1). **Compiled lanes force the published graph `blockParallelSafe` bit to false**,
  independent of node properties and `totalLatency`, so a zero-latency fader-only automated
  graph can never be parallel-rendered (review finding 2). A compile-time event-budget check
  rejects projects whose worst-case per-block event count exceeds the existing
  `kMaxEventsPerBlock` budget (explicit error, never silent drop). All emission and consumer
  smoothing cadences anchor to absolute timeline frames (shared rule with H14; review finding 6).
- **Precedence:** where a lane exists for a target, automation events win over manual scalar
  posts (`postSetGain`/`postSetPan`) — read-mode semantics; touch/latch/write recording is
  deferred past alpha (ADR-0037).
- **Consumers:** `FaderNode` migrates to its ParamSpec mapping (normalized → dB → linear; its
  current raw-linear event shim and gates are updated in the same commit — a flagged behavior
  change with no saved-data impact since no writer ever existed); `PanNode` gains event
  consumption via the same pattern; send levels become automatable by giving each send tap its
  own `FaderNode`; all five H14 FX nodes consume events by construction.
- **Audit-first:** H15's first checkpoint locks the existing evaluators under a biting
  characterization gate before anything builds on them.

## Consequences

- **Positive:** sample-accurate automation with the same guarantees as the rest of the engine
  (block-size independent, tempo-correct, locate/loop-safe, RTSan-covered); one targeting scheme
  that survives projection refactors; the mix step gets volume/pan/send/FX automation for alpha.
- **Negative / accepted costs:** 64-sample staircase + smoothing lag as the documented, asserted
  approximation; lane count per project bounded by the event budget (explicitly rejected, not
  degraded); Bezier/Log curves wait for a later ADR; write-mode recording waits past alpha.
- **Follow-ups:** H16 lane editor UI on this data model; extend the randomized edit property test
  with automation verbs; the v8 fixture joins the forever-gate; CONTEXT terms already added
  (Automation lane, Breakpoint, ParamSpec).
