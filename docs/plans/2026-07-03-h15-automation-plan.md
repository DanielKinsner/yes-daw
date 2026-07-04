# H15 — Automation: focused plan

> Decisions: [ADR-0039](../adr/0039-automation-lanes-runtime.md) (storage, targeting, compiled
> runtime), [ADR-0038](../adr/0038-built-in-fx-suite.md) (ParamSpec), ADR-0009 (event contract —
> unchanged). Guardrails: [`docs/fable5/implementer-brief.md`](../fable5/implementer-brief.md).
> Precondition: H14 closed remote-green (ParamSpec + per-Track strips + schema v7 exist).

**Goal.** Automation lanes as Project data, persisted and undoable, compiled into the Snapshot,
evaluated sample-accurately on the audio thread, driving fader/pan/send/FX parameters. Headless
only — the lane editor UI is H16. Read-mode only — no touch/latch/write recording (ADR-0037).

**Mechanical exit criterion.** An automated parameter renders to its closed-form expected curve
within the stated tolerance, bit-robust across block-size schedules and across a mid-curve tempo
change; lanes round-trip save/reopen through the new schema version; the randomized edit property
test extended with automation verbs stays green; offline Render == RT with automation; RTSan
clean; the side-band delivery and scheduler-refusal gates (below) green.

> **Review amendments (2026-07-03).** This plan was amended per
> [`docs/reviews/2026-07-03-adversarial-review-h14-h17-packet.md`](../reviews/2026-07-03-adversarial-review-h14-h17-packet.md):
> automation delivery is a `ProcessArgs` side-band, never the root event slot (finding 1);
> compiled lanes force `blockParallelSafe = false` (finding 2); all cadences anchor to absolute
> frames (finding 6); schema numbers are "next free version" (H13 still open).

---

## Current-state audit (verified against HEAD 2026-07-03 — re-verify at kickoff)

- `src/engine/Automation.h`: `AutomationCurveType { Linear, Hold, Bezier, Log }`,
  `AutomationPoint { tick, value, curveType }`, `AutomationTarget { NodeId, ParameterId }`,
  `AutomationLane`, `AutomationLaneCursor`; evaluators
  `evaluateAutomationLaneForBlock/evaluateAutomationPointsForBlock` emit `ParameterChange` events
  into a caller span. **No caller outside tests.**
- Schema v1 `automation_points` table: validators only (`validateFiniteReals`,
  `validateStoredRanges`); **no reader, no writer, no data has ever existed.** Leave in place,
  document as superseded by v8 (append-only migrations; do not drop).
- `FaderNode` is the only event-consuming node (raw-linear interpretation — migrates to ParamSpec
  here). `PanNode` reads its scalar at block start only. `Runtime::postSetGain/postSetPan` is the
  scalar seam; `CompiledGraph` root event slot is `kRootEventSlot = 0` with
  `kMaxEventsPerBlock = 1024` per slot.

## Gates that must BITE

| Gate | Proves | Named negative control |
|---|---|---|
| Evaluator characterization | Linear/Hold segment math, half-open block slicing, cursor reuse | Shift emission by one frame at a block boundary → cross-block compare fails |
| Curve-accuracy render | End-to-end lane → node → audio matches closed form within max(slope×6 ms, 1e-4) | Drop the control-interval emission (breakpoints only) → a long fade exceeds tolerance |
| Side-band delivery | Automation reaches a consumer whose upstream produces events (MidiSource → Instrument → Fader) in the same block as the upstream events | Deliver automation via the root event slot instead → the downstream-consumer case fails (`eventInputSlotFor` hands the node the upstream slot) |
| Scheduler refusal | A zero-latency, fader-only automated graph (no FX) refuses the parallel path because compiled lanes force the unsafe bit | Remove the automation force (compute safety from node properties + latency only) → the fader-only graph parallel-renders and this gate fails |
| Block-size independence | Bit-identical across schedules incl. 1..9-frame forcing | Anchor emission to block starts instead of absolute frames → fails |
| Tempo correctness | Tick-domain breakpoints land on correct frames across a mid-curve tempo change | Evaluate ticks against the pre-change tempo only → fails |
| Locate/loop correctness | Cursor reset lands the parameter at the curve value for the new position | Skip cursor reset on locate → stale-value assertion fails |
| Event budget | Over-budget projects are rejected at compile with an explicit error | A generated worst-case project must produce the rejection, not silence or drops |
| Precedence | Lane events win over `postSetGain/postSetPan` scalars | Reverse the ordering → gate fails |
| Persistence v8 | Round-trip + validators (orphan target, unsorted/duplicate ticks, range, curve type) | One failing fixture per validator |
| Undo property | Randomized sequences incl. automation verbs; full undo → bit-identical | (Existing property-test negative pattern) |
| FaderNode migration | Normalized→dB→linear mapping per ParamSpec | Old raw-linear fixture value now produces the mapped result — old expectation must fail |

## Checkpoints

**CP0 — Evaluator characterization gate** (`YesDawAutomationCheck`, new). Lock
`Automation.h` behavior with biting cases before building on it: Linear and Hold segments across
block boundaries (half-open `[0, numFrames)` exactness); first/last breakpoint edge behavior
(before-first = first value; after-last = last value); cursor reuse across sequential blocks ==
fresh-cursor evaluation (bit-equal); locate (cursor re-seek) equivalence; hostile inputs (NaN
value clamped finite, unsorted points rejected or documented); Bezier/Log evaluator output
recorded as characterization cases but marked storage-quarantined. *This checkpoint changes no
production code.* If characterization reveals defects, fix them here with their own negative
controls before proceeding — report findings in `STATUS.md` either way.

**CP1 — Project model + schema v8 + undo.** `Project.automationLanes`
(`AutomationLaneData { EntityId id; EntityId ownerEntity; AutomationTargetRole role; std::uint32_t
paramId; std::vector<AutomationBreakpoint> points; }`, `AutomationTargetRole { TrackFader,
TrackPan, SendLevel, BusFader, BusPan, FxInsertParam }`). The **next free schema version** (v8 at
time of writing — H13 is still open; verify `kCodeSchemaVersion` at kickoff) per the append-only
recipe:
`automation_lanes(id BLOB PK, owner_entity BLOB, target_role INTEGER, param_id INTEGER,
UNIQUE(owner_entity, target_role, param_id))`,
`automation_breakpoints(lane_id BLOB, tick INTEGER, value REAL CHECK(value>=0 AND value<=1),
curve_type INTEGER CHECK(curve_type IN (0,1)), PK(lane_id, tick))`. Undo verbs `addAutomationLane
/ removeAutomationLane / addBreakpoint / moveBreakpoint / setBreakpointValue / setBreakpointCurve
/ removeBreakpoint` via the row-diff pattern; extend the randomized edit property test with these
verbs. Open-time validators + one failing fixture each (orphan owner entity, unknown role,
param id not in the target's ParamSpec, duplicate tick, out-of-range value, quarantined curve
type). The new-version fixture bundle joins the forever-gate.

**CP2 — Consumers.** (a) `FaderNode` ParamSpec migration: gain spec (dB domain, −60..+6, `Db`
mapping; normalized 0 = −60 dB treated as −inf/mute), event values interpreted normalized, and
parameter events consumed from BOTH `args.events` and the new `automationEvents` side-band
(one shared filtering/ramp helper — see CP3); update `YesDawFaderCheck` expectations in the same
commit (flagged behavior change; no saved data exists). (b) `PanNode` event consumption: same piecewise-ramp pattern
(`kPanParameterId = 1`, linear mapping −1..+1). (c) Send levels: per-send `FaderNode` spliced on
the send tap in the mixer projection (id from the Track entity + send ordinal role), giving
`SendLevel` lanes a real target; parity gate — send with level 1.0 renders identically to
pre-splice reference. (d) FX params already consume events (H14 contract) — one integration case
per FX node kind proving a lane reaches it.

**CP3 — Compile + RT evaluation.** At graph build: convert each lane tick→frame via
`CompiledTempoMap` into flat sorted frame-domain arrays stored in the `CompiledGraph` payload
(`CompiledAutomationLane { NodeId, ParamId, frames[], values[], curveTypes[] }` + a preallocated
cursor slot per lane); resolve `(ownerEntity, role, paramId)` → NodeId through the projection's
allocators, error on unresolvable targets. Compile-time event-budget check: worst-case events per
block = Σ over lanes of (blockSize/64 + 2) ≤ `kMaxEventsPerBlock` else reject with a new
`GraphBuildError` code. Audio thread, per block, before node processing: advance each cursor,
emit normalized `ParameterChange` events at absolute-frame-anchored 64-sample intervals along
active Linear segments plus exactly at breakpoints (Hold = breakpoint events only) into the
**automation side-band** — a preallocated per-block buffer owned by `CompiledGraph`, exposed to
every node as a new additive `ProcessArgs::automationEvents` `EventStream` view (empty span when
no lanes exist; existing nodes that ignore it are unaffected). **Root-slot injection is
forbidden:** `GraphBuilder::eventInputSlotFor` hands a node its upstream producer's event slot
whenever one exists (`src/engine/GraphBuilder.h:680-690`), so root injection silently misses any
consumer downstream of an event-producing node (review finding 1) — the side-band delivery gate
proves the MidiSource → Instrument → Fader case. Every node receives the same side-band span and
filters by its `NodeId` + `ParamId` through one shared helper also used for `args.events`
compatibility. **Compiled lanes force the graph's published `blockParallelSafe` bit to false in
the `GraphBuilder` payload — independent of node properties and `totalLatency`** (review finding
2). Anchoring rule (shared with H14, normative): every emitted event carries an absolute timeline
frame; all consumer smoothing and recompute cadences anchor to that absolute frame across block
boundaries and PDC-shifted streams. Locate/loop resets cursors by binary search (transport
integration follows the H8 loop/locate parity pattern). RT path is allocation-free
(`YESDAW_RT_HOT` on the walk), covered by the RTSan lane. Gates: curve-accuracy, side-band
delivery, scheduler refusal, block-size sweep, tempo-change, locate/loop, budget — per the master
list.

**CP4 — Integration + close-out.** Offline == RT with an automated full mix (fader fade, pan
sweep, send ride, one FX param per node kind); precedence gate; `YesDawSchedulerCheck`
extension: a **zero-latency, fader-only automated graph with no FX** must refuse the parallel
path purely because compiled lanes force the unsafe bit (negative control per the bite table —
removing the automation force must make this gate fail). A second case with FX present documents
that FX-induced refusal is NOT evidence of automation safety (review finding 2: the guard must
never pass for the wrong reason). STATUS/roadmap close-out + adversarial review pass scheduled.

## Not yet (guardrails)

Lane editor UI (H16); touch/latch/write recording; Bezier/Log storage acceptance; MIDI CC /
expression lanes; per-clip automation; modulation (CONTEXT: separate deferred concept); ramp-target
event payloads (future ADR); automation of stepped/bool params (mute, band type).
