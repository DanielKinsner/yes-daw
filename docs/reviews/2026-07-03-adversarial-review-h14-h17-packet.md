# Adversarial review - H14-H17 planning packet

- **Date:** 2026-07-03
- **Reviewer:** Codex (adversarial pass)
- **Repo state reviewed:** `main` @ `189a855`; `git pull --ff-only` was already up to date.
- **Scope:** ADR-0037/0038/0039, H14-H17 plans, `STATUS.md`, `CONTEXT.md`, `docs/reality-lane.md`, `docs/goals/risk-register.md`, `docs/fable5/implementer-brief.md`, and source at HEAD.
- **Method:** fact-checked the packet's named code claims against source first, then attacked event routing, schema numbering, gate bite, implementer-guess points, DSP math, and accepted-ADR/house-rule contracts.

---

## Ranked findings

### 1. Root automation events are not a broadcast bus once an upstream input produces events

- **SEVERITY:** BLOCKER
- **STATUS:** CONFIRMED
- **Packet claim:** H15 CP3 says automation emits normalized `ParameterChange` events "into the root event slot" (`docs/plans/2026-07-03-h15-automation-plan.md:91-94`), and ADR-0039 makes the same runtime decision (`docs/adr/0039-automation-lanes-runtime.md:35-41`).
- **Evidence:** `CompiledGraph::process` gives a node root events only when `cn.eventInputSlot == kRootEventSlot`; otherwise it gives the node the upstream event slot (`src/engine/CompiledGraph.h:335-345`). `GraphBuilder::eventInputSlotFor` returns the first direct input's produced event slot if any input has one, and returns root only when no input produced events (`src/engine/GraphBuilder.h:680-690`). That means root automation and upstream MIDI/event output are not merged. A consumer whose direct input produces events receives the upstream slot, not root automation.
- **Concrete fix:** Specify the event routing contract before H15 implementation. Either add a true root automation side-band merged into every consumer's input stream, or make `CompiledGraph` deliver a target-filtered automation stream independently of node-produced event streams. Add a gate where an event-producing upstream node feeds an automated downstream consumer; the gate must prove both upstream events and root automation reach the consumer in the same block.

### 2. The H15 scheduler guard can pass for the wrong reason and miss unsafe automation

- **SEVERITY:** BLOCKER
- **STATUS:** CONFIRMED
- **Packet claim:** H15 CP4 says automated graphs refusing the parallel path is "already implied by `totalLatency > 0` or `blockParallelSafe = false`" (`docs/plans/2026-07-03-h15-automation-plan.md:99-102`).
- **Evidence:** The scheduler only checks `first.graph->isBlockParallelSafe()` and returns `GraphNotBlockParallelSafe` when that bit is false (`src/engine/GraphScheduler.h:70-76`). `GraphBuilder` currently computes the bit from `payload.totalLatency == 0` and each node's `properties().blockParallelSafe` (`src/engine/GraphBuilder.h:229-237`). A fader-only automated graph has zero latency, and current `FaderNode` opts into `blockParallelSafe = true` (`src/engine/nodes/FaderNode.h:42-45`). H14 also makes all five FX nodes unsafe (`docs/adr/0038-built-in-fx-suite.md:83-84`), so an H15 "automated full mix" that includes FX can refuse the scheduler because of the FX, not because automation itself is correctly marked unsafe.
- **Concrete fix:** H15 CP3 must explicitly set `CompiledGraph::blockParallelSafe = false` when compiled automation lanes are present, independent of node properties and latency. The scheduler gate's negative control must be a zero-latency, fader-only automated graph with no H14 FX; otherwise the test can go green while automated scalar-only projects are still parallel-rendered out of order.

### 3. H14 CP2 says clip gain stays at each source, but the current source path has no such owner

- **SEVERITY:** MAJOR
- **STATUS:** CONFIRMED
- **Packet claim:** H14 CP2 says to group clips by Track, remove `combinedGain = clip.gain x track.strip.linearGain`, and keep "clip gain ... applied at each source" (`docs/plans/2026-07-03-h14-fx-suite-plan.md:49-58`).
- **Evidence:** Current `ProjectMixerProjection` walks `project.clips` (`src/engine/ProjectMixerProjection.h:133`), computes `combinedGain = clip.gain * owningTrack->strip.linearGain` (`src/engine/ProjectMixerProjection.h:159`), derives source/fader/pan/meter IDs from the clip ID (`src/engine/ProjectMixerProjection.h:167-170`), and stores the combined gain on the per-clip `MixerTrackProjection` fader (`src/engine/ProjectMixerProjection.h:193-199`). The source factory in `OfflineRenderer` deliberately hands raw samples to `DecodedClipNode`; it says "clip.gain stays a projection fader" and constructs `DecodedClipNode` without a gain parameter (`src/engine/OfflineRenderer.h:359-390`). Accepted ADR-0034 already requires `ProjectMixerProjection` to derive mixer strips from Tracks/Buses, not Clips (`docs/adr/0034-mixer-state-schema-and-persistence.md:58-60`).
- **Concrete fix:** CP2 must name the source-level gain mechanism: add a per-clip gain node before the Track sum, extend `DecodedClipNode` with clip gain, or explicitly pre-scale decoded source buffers and prove that remains non-destructive Project behavior. The parity gate should assert audio equality for two clips on one Track, meter identity changes expected by ADR-0034, and no double application of track gain.

### 4. The EQ appendix omits normative shelf formulas while requiring exact closed-form gates

- **SEVERITY:** MAJOR
- **STATUS:** CONFIRMED
- **Packet claim:** H14 A1 says LowShelf uses one partial formula, then "`out = ...` use the cytomic SVF cookbook forms exactly" while the gate must match "the closed-form `H(z)`" (`docs/plans/2026-07-03-h14-fx-suite-plan.md:131-142`).
- **Evidence:** No in-repo Cytomic reference file or complete LowShelf/HighShelf equations are provided in the packet, and the implementing brief forbids silent taste/spec choices when a spec is ambiguous (`docs/fable5/implementer-brief.md:34-36`). The gate asks for closed-form response matching, but the packet does not provide the exact closed forms for every band type it requires.
- **Concrete fix:** Put the complete Bell, LowShelf, HighShelf, HPF, LPF, and Notch equations in the plan or add a committed reference file the plan cites. Include the exact coefficient clamp rule and a small table of expected response values for the gate so the implementer does not choose among cookbook variants.

### 5. The limiter spec contradicts itself on min/max and gives two attack algorithms

- **SEVERITY:** MAJOR
- **STATUS:** CONFIRMED
- **Packet claim:** ADR-0038 calls `LimiterNode` a "sliding-window-minimum peak limiter" (`docs/adr/0038-built-in-fx-suite.md:67-69`). H14 A5 says `peak[n] = max over window [n, n+D)` and says to maintain that with a "sliding-window maximum" (`docs/plans/2026-07-03-h14-fx-suite-plan.md:190-195`). The same appendix then gives two attack forms: a ramp based on `samplesUntilPeak`, and "simplest correct form: `g = min(g, t[n])` then release" (`docs/plans/2026-07-03-h14-fx-suite-plan.md:196-199`).
- **Evidence:** A limiter can track either the maximum future peak and derive a target gain, or track the minimum future target gain. The packet names both conventions without declaring which one is normative. It also offers two different gain update laws; they produce different gain-reduction envelopes and transparency behavior, so an implementer must choose.
- **Concrete fix:** Rename the data structure to the chosen convention and specify one attack law. For example: maintain a sliding maximum of future absolute peaks, compute `t[n]`, then hard-clamp `g = min(g, t[n])` before release. Or choose the ramp-to-peak law, but then define `samplesUntilPeak`, `minStep`, tie behavior for multiple equal peaks, and the exact expected envelope tests.

### 6. The block-size independence gates do not fully define the phase anchors they depend on

- **SEVERITY:** MAJOR
- **STATUS:** CONFIRMED
- **Packet claim:** H14 says coefficient recomputation happens every 16 samples during active ramps and is "anchored to the event offset" (`docs/plans/2026-07-03-h14-fx-suite-plan.md:122-127`). H15 says linear automation emits at absolute-frame-anchored 64-sample intervals (`docs/plans/2026-07-03-h15-automation-plan.md:85-94`) and has a bit-identical block-size gate (`docs/plans/2026-07-03-h15-automation-plan.md:38-40`).
- **Evidence:** ADR-0009 requires half-open block edge rules and PDC-shifted event streams (`docs/adr/0009-event-stream-and-automation.md:51-55`). H14 only says "event offset", not whether a 5 ms ramp's 16-sample recompute phase persists across following blocks as `eventAbsoluteFrame + 16k`. Without that, two legal implementations can both obey the words but recompute at different frames under 1..9-frame block schedules. H15 is clearer on emission anchors but still depends on every consuming node carrying smoothing/recompute phase in absolute-frame terms, not block-relative terms.
- **Concrete fix:** Add a shared rule: every emitted automation/control event has an absolute timeline frame, and every smoothing or coefficient-recompute cadence is anchored to that absolute event frame until the ramp ends, across block boundaries and PDC shifts. The negative control should reset the recompute phase at each block boundary and prove the cross-schedule gate fails.

### 7. H17 makes human sign-off part of alpha closure, which conflicts with the mechanical-gate rule

- **SEVERITY:** MAJOR
- **STATUS:** CONFIRMED
- **Packet claim:** H17 says alpha closes only when mechanical sub-asserts pass plus "Dan's sign-off line" is committed (`docs/plans/2026-07-03-h17-distribution-alpha-plan.md:53-59`). ADR-0037 also says visual/audible feel is a human session at H16/H17 close (`docs/adr/0037-alpha-target-and-h14-h19-recarve.md:91-101`).
- **Evidence:** House rules say a checkpoint is done when its mechanical check is green (`CLAUDE.md:40-42`) and "never" by a human confirming tone or visual smoothness; hardware checks must be one-command PASS/FAIL scripts (`CLAUDE.md:44-50`). The packet can keep a batched human product review, but making that sign-off a close gate makes H17 non-mechanical.
- **Concrete fix:** Split the alpha gate in two: mechanical alpha close is `tools/alpha-verify.ps1` plus packaged playback/recording reality-lane PASS rows; the human session is a non-gating product-readiness note, or it is limited to the existing GUI visual-feel exception and converts findings into explicit token/layout tasks with mechanical checks.

---

## Single highest-value packet change

Rewrite the H15 runtime section before implementation: define automation as a merged or side-band event stream that reaches target nodes regardless of upstream event slots, and require compiled automation lanes to make the graph block-parallel-unsafe. Add the zero-latency automated-fader scheduler negative control in the same wording. That closes the largest "green gate, wrong engine" hole.

## Packet claims verified as TRUE

- `FaderNode` consumes targeted `ParameterChange` events at in-block offsets and ramps piecewise through the remaining ranges (`src/engine/nodes/FaderNode.h:70-84`, `src/engine/nodes/FaderNode.h:126-130`).
- `ProjectMixerProjection` still emits one strip per Clip, with node IDs derived from clip IDs (`src/engine/ProjectMixerProjection.h:133-200`).
- The schema-v1 `automation_points` table exists, but current code only includes it in validators; no writer/model reader was found (`src/persistence/ProjectBundle.h:950-957`, `src/persistence/ProjectBundle.h:2977-3030`).
- `CompiledGraph::kMaxEventsPerBlock` is 1024, and event storage allocates that capacity per non-root event slot (`src/engine/CompiledGraph.h:135-154`, `src/engine/GraphBuilder.h:793-813`).
- `ScopedNoDenormals` is created before the real compiled-node loop in `CompiledGraph::process` (`src/engine/CompiledGraph.h:286-308`).
- `NodeProperties` has no `tailSamples` field today (`src/engine/Node.h:36-44`).
- The sole CMake configure/build/test preset is `ci`, and it sets `CMAKE_BUILD_TYPE` to `Release` (`CMakePresets.json:4-24`).
- `WaveformPeakCache` is a synchronous helper with build/read/write functions, and no UI consumer was found; `TimelineCanvas` draws fake waveform bars from `(clipId * 37 + x * 13) & 31` (`src/persistence/WaveformPeakCache.h:255-541`, `src/ui/TimelineCanvas.h:124-136`).
- At HEAD, `kCodeSchemaVersion` is 6. H14's literal v7 and H15's literal v8 do not collide with current code, and the remaining H13 `STATUS.md` checkpoint is autosave recovery, not a named migration (`src/persistence/ProjectBundle.h:40-42`, `STATUS.md:41-86`). The safer wording is still "next free schema version" until H13 is actually closed.
