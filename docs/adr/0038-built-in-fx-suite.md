# 0038. Built-in FX suite: five Nodes, the ParamSpec model, insert chains, and tails

- **Status:** Proposed
- **Date:** 2026-07-03
- **Deciders:** Dan + Fable 5 (2026-07-03 planning session)
- **Related:** ADR-0037 (re-carve — why FX before hosting), ADR-0008 (Node contract — extended
  additively here), ADR-0007 (PDC), ADR-0009 (event stream — the automation seam these nodes must
  honor), ADR-0014 (mixer policy), ADR-0034 (mixer-state persistence — extended by schema v7),
  plan: [`2026-07-03-h14-fx-suite-plan.md`](../plans/2026-07-03-h14-fx-suite-plan.md)

## Context

Alpha (ADR-0037) requires mixing, and YES DAW has no audio effects: built-in DSP is utility-only
(`FaderNode`, `PanNode`, `SumNode`, `MeterNode`, `DelayNode`-as-PDC-splice, `TimeStretchNode`).
Three facts about the current code shape this decision (verified against HEAD 2026-07-03):

1. **The automation seam already exists and has one model consumer.** `FaderNode`
   (`src/engine/nodes/FaderNode.h`) consumes `EventType::ParameterChange` events targeted at its
   `NodeId`+`ParameterId` from `args.events`, running a `dsp::LinearRamp` piecewise between event
   offsets — sample-accurate and block-size-independent. New FX nodes must follow this exact
   pattern so H15 automation drives them for free.
2. **Mixer strips are per-clip today.** `ProjectMixerProjection` walks `project.clips` and builds
   one source→Fader→Pan→Meter chain *per clip* (node ids derived per-clip via
   `projectMixerNodeIdForClip`). A Track-level insert chain — and any Track-level automation
   target — needs a **per-Track strip**: the track's clip sources summed, then one chain per
   Track. This consolidation is a prerequisite checkpoint, with parity gates.
3. **There is no tail concept.** `NodeProperties` has only `latencySamples`; offline render
   extends the timeline by `CompiledGraph::totalLatency()` alone. A reverb/delay must NOT report
   its ring-out as latency — PDC would insert alignment splices that delay sibling paths.

Hard to reverse: **ParamIDs, normalized-value mappings, and FX-chain rows enter saved user
projects** (the one-way door in the risk register, R3); the `tailSamples` contract addition
becomes part of the frozen Node contract surface.

## Options considered

1. **Suite scope — five vs more.** Five processors (parametric EQ, compressor, delay, reverb,
   lookahead limiter) cover the demo-song mix; gate/saturation/chorus/de-esser add surface without
   unblocking anything (De-Esser is YES Voice's turf; a limiter overlaps YES Master's domain and
   its DSP can inform ours later). *Chosen: five, clean/surgical character.*
2. **Tails — reuse `latencySamples` vs additive `tailSamples`.** Reusing latency corrupts PDC
   (splices would delay dry siblings by the reverb tail). *Chosen: add
   `NodeProperties::tailSamples` (int64, default 0) — an additive, backward-compatible extension
   of ADR-0008; offline render extends the timeline by `totalLatency() + Σ tailSamples` (a
   documented conservative bound).*
3. **Parameter events — real-valued vs normalized.** `automation_points` (schema v1) already
   CHECK-constrains stored values to 0..1, and hosted plugins speak normalized. *Chosen: events
   and storage carry normalized 0..1; each node maps normalized→real through its own ParamSpec
   table. (`FaderNode`'s current treat-as-linear-gain shim gets superseded by its ParamSpec
   mapping in H15, with its gates updated — flagged, not silent.)*
4. **EQ topology — RBJ direct-form biquads vs TPT SVF.** Direct-form biquads misbehave under
   coefficient modulation; the TPT/Zavalishin SVF stays stable under per-sample coefficient
   updates, which automation will cause. *Chosen: TPT SVF cores.*

## Decision

Build **five built-in FX Nodes** under the existing Node contract, an **insert-chain** on
per-Track/Bus strips, and the **ParamSpec** parameter model, per the H14 plan:

- **`EqNode`** — 6 bands, each a TPT SVF (Bell, Low/High Shelf, HPF, LPF, Notch selectable).
- **`CompressorNode`** — feed-forward, log-domain detector, soft knee, branching attack/release
  one-pole ballistics, manual makeup. (Sidechain input deferred.)
- **`FxDelayNode`** — stereo delay with feedback, damping, ping-pong; dual-tap crossfade on time
  changes. Named to avoid collision with the PDC `DelayNode`/`LatencyNode`.
- **`ReverbNode`** — 8-line FDN, Householder feedback, input diffusion allpasses, pre-delay,
  RT60-mapped per-line gains + damping.
- **`LimiterNode`** — lookahead sliding-window-minimum peak limiter; **reports
  `latencySamples = lookahead`** — the first real nonzero-latency built-in node, making the PDC
  parallel-path alignment clause (H3 exit) a live test. Sample-peak in alpha; true-peak deferred.

**ParamSpec:** each node publishes a static table of
`{ ParamID (u32, append-only forever), name, unit, min/max/default (real), mapping (linear | log | dB), smoothing class }`.
Events carry normalized 0..1; the node maps and smooths (LinearRamp / one-pole per class) from
event offsets, per the FaderNode pattern. Hostile values (NaN/Inf/out-of-range) clamp finitely.

**Insert chain:** after the per-Track strip consolidation, inserts sit between the strip's
`chainHead` (source sum / sidechain VCA) and its `FaderNode`; Buses get the same slot before
their `PanNode`. Enabled/bypass toggles crossfade over ~5 ms (click-free), never allocate.
Persistence: **schema v7** adds `fx_inserts` (ULID id, owner entity, position, kind, enabled) and
`fx_insert_params` (insert id, param id, normalized value), with undo verbs
(add/remove/reorder/set-param) through the existing command-diff pattern.

**Engine placement:** all five set `blockParallelSafe = false` (the H9 scheduler correctly
refuses them to the serial path — accepted for alpha); new `CompiledNodeKind` entries (no
fall-through to `Plugin`); `process()` is `YESDAW_RT_HOT`, allocation only in `prepare()`;
denormals covered by the existing engine-wide `ScopedNoDenormals`. Equal-power crossfade (the
deferred H2 fade-law item) lands in this horizon with an explicit golden-update procedure.

## Consequences

- **Positive:** the mix step exists; automation (H15) gets five real consumers through the seam
  FaderNode already proves; PDC and tail machinery gain real consumers (H3/H7 clauses go live);
  the YES family inherits a parameter-identity model; per-Track strips fix the latent per-clip
  mixer-shape before UI and automation build on it.
- **Negative / accepted costs:** five DSP surfaces to own; serial-only scheduling for FX graphs
  until a later scheduler deepening; `FaderNode`'s parameter-event semantics change under
  ParamSpec (gates updated in H15, flagged in the plan); conservative tail bound may render some
  silence at the end (trimmed by export logic later if it matters).
- **Follow-ups:** CONTEXT terms (done with ADR-0037: Insert, FX chain, ParamSpec); the frozen
  FX-era fixture bundle forever-gate (risk R3); H15 reuses ParamSpec for automation targets;
  H16 surfaces the chains in the mixer UI.
