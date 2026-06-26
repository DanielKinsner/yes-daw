---
title: H3 close-out plan — finish the horizon to its exit gate, zero stragglers
type: plan
date: 2026-06-26
status: active
revision: 2 (completeness-checked by an independent adversarial pass; 9 blocker gaps folded in)
---

# H3 close-out plan

**Why this exists.** H3 ("mixer + plugin hosting") was built *breadth-first* — a little of every sub-feature,
each commit green against the **generic** CI gates — but H3's **exit criterion was never turned into a CI
test**, and the `loop/` infrastructure the build plan designed (`loop/horizon.md` = "exit criterion + exact
green commands") was never created. So "is this commit green?" was checked 50 times; "is H3 done?" was never
checkable. Result: a hosting *skeleton* that passes the generic gates while the horizon's real finish line
(a crashing plugin is isolated with no dropout) is untested and unmet.

This plan closes H3 the only way that prevents stragglers and backtracking: **define the exit gate as a
real, initially-RED CI test, build depth-first until it is green, and do not close H3 — or touch H4 — until
it is.** Done becomes mechanical, not a judgment call.

> **Revision 2 note.** Rev 1 of this plan was itself caught making the original H3 mistake: one-line "wire
> it" steps that hide three unbuilt subsystems, and gate clauses asserting against artifacts that don't
> exist (a placeholder node, a blacklist store). An independent adversarial completeness pass found 9
> blocker gaps; they are folded in below. That catch — before a line was built — is the process working.

## The rules this plan runs under (the process fix)

1. **Exit gate first, and RED.** Section 1 is the H3 exit-gate CI test. It starts failing (no real hosting
   exists). H3 is "done" *iff* it is green. No optimistic "complete."
2. **Every test negative-controlled.** A test counts only once shown to FAIL when the code it guards is
   broken (as done for the fader/mute/block-size fixes; as the old mono-blind render harness was *not*).
3. **Independent adversarial review before green** — the builder does not grade its own homework.
4. **Deterministic oracles, never wall-clock.** "Never waits/allocates/locks" = **RTSan-clean** over
   `PluginNode::process()` + the fail-open path. "Zero xrun" = a **deadline-miss counter == 0** under a
   forced-late child. CI timing is flaky; these are not.
5. **Depth-first to the gate**, and **`loop/horizon.md` is created** with H3's exit criterion + the green
   command, so any loop runs against the right oracle.

---

## 1. The H3 exit gate (write FIRST; starts RED) — `YesDawHostIsolationCheck`

A deterministic, dependency-free CI test that runs an **in-repo synthetic test plugin** (a real hosted
`juce::AudioProcessor` in the worker child) **inside a `buildMixerGraphProjection` graph published through
`Runtime`**. Every clause is a **negative-controlled** assertion.

**(a) Tri-stream PDC through the *hosted* plugin.** An audio impulse, an automation-ramp value, AND a
synthetic event each land at the predicted compensated sample (extends the H1 PDC-impulse gate from a stub
to a real hosted node; uses the input Event ring + control-lane automation — **not** an in-process stub).

**(b) Crash/hang isolation, mechanically asserted, no dropout:**
- a child that **hangs after handshake, mid-processing** is detected by a **running watchdog** (driven off
  RT-lane output-sequence progress / a periodic liveness ping), **killed (assert the child PID is killed)**,
  and the kill **auto-enqueues bypass + recompile** — with **no manual pipeline drive**;
- a child that **crashes on cue** (a real crash, not a self-label) takes the same path;
- the audio thread **fails open within the block budget**: **RTSan-clean** over `process()` + fail-open;
  **deadline-miss counter == 0** under a forced-late child;
- after recovery the **compiled graph contains a `Placeholder` kind at the offender's slot** (not merely the
  ring's internal bypass latch), swapped via the ADR-0006 ordered publish/janitor path;
- the offender is **blacklisted**: a row keyed `{format, plugin_uid, plugin_version}` **survives a
  coordinator restart/reopen** (this assertion must **FAIL when persisted/policy is false** — inverting the
  current always-false check);
- **emit-NaN mode:** bus output is **finite every block** (no NaN/Inf reaches Master); offender degraded on
  the last-good→silence→bypass path;
- **impossible/negative reported latency** is clamped/quarantined (negative-control: remove the clamp).

**(c) Opaque state across IPC.** Plugin `getStateInformation`/`setStateInformation` chunk pushed/pulled over
the **control lane**, `{chunk_len, crc32}` validated (ADR-0013), surviving a round-trip across the **real
process boundary**.

**pluginval L8–10 + `auval`:** **non-blocking** (gated on runner plugin availability + the GPL license
gate); the **in-repo crash-test plugin is the always-on blocking gate**, which **supersedes** the roadmap's
"pluginval/auval pass in CI" wording (dispositioned here, not silently dropped).

Green command recorded in `loop/horizon.md`. This file is the contract for "H3 done."

---

## 2. Gap to the gate — depth-first **critical path: 1 → 2 → 3 → 4 → 5**

Each item: *what exists → what's missing.* Items split where rev 1 hid multiple subsystems.

**1. In-repo synthetic test plugin** — *exists: nothing.* A **real hosted `juce::AudioProcessor`** in the
worker child with modes: passthrough / fixed-reported-latency / emit-NaN / hang-after-handshake /
crash-on-cue. (Authorable early; its **cross-process** gate role depends on **2c**.)

**2. Real cross-process IPC** — *exists: in-process `RtLaneRing` (a `std::vector`) + a sleep-loop worker;
`plugin_host/` has zero references to the ring.* Split, ordered:
- **2a** lift the ring region into **OS shared memory with a fixed byte layout** (ADR-0015 lists the layout
  as still-open); move ring **ownership** from `PluginNode` inline → the shared region.
- **2b** coordinator **allocates + passes the region handle** over the control channel at plugin load.
- **2c** worker **maps it and runs `pollOnce`** against the real hosted processor.
- **Gate-blocking fixes land here:** Finding **D** (`RtLaneRing::reset()` must be quiescent vs a live child
  before the crash/recompile path calls it) and Finding **E** (`exchangeBlock` channel-count guard must be a
  **release-safe clamp**, not `YESDAW_RT_ASSERT`, before a hostile child supplies the count).

**3. PlaceholderNode + the coordinator actually executes** — *exists: metadata skeleton; every
`*Executed/*Applied/*Persisted` flag hardcoded false; watchdog is a one-shot launch probe; no placeholder
kind; no blacklist store.* Split, ordered:
- **3a** new **`CompiledNodeKind::Placeholder`** (silent passthrough, frozen Node contract) + GraphBuilder
  compile path. **Must precede 3c** (which swaps it in).
- **3b** a **running watchdog on the live child** (off RT-lane output-seq progress / liveness ping during
  processing) — replacing the launch-probe.
- **3c** crash/hang → kill → **auto-enqueue** bypass + recompile that swaps the Placeholder at the
  offender's slot (ADR-0006 ordered publish/janitor). Finding **H** (resetState leaves pipelines non-empty).
- **3d** a **persistent blacklist store + schema** — a table keyed `{format, plugin_uid, plugin_version}`
  (ADR-0013); row survives restart. Finding **I** (blacklist keyed by plugin UID).

**4. Mixer projection → runtime** — *exists: `buildMixerGraphProjection` + `applyMixerMutePolicy` built &
fixed, **zero production callers** (test-only); `Runtime` only ever driven from tests.* Split:
- **4a** `Project` → `MixerProjectionInputs` projector.
- **4b** device-callback → `Runtime.processBlock` **driver** (the production caller of `publish()`/
  `processBlock()`).
- **4c** the exit-gate test runs its plugin **inside the projected graph through `Runtime`** (not a bespoke
  harness). Finding **G** (projection dedup + a GraphBuilder identical-edge decision; negative-controlled
  "two identical sends sum once"). Wire `applyMixerMutePolicy` onto the **live published graph** + a
  **SIP-leakage + mask-capacity** assertion (ADR-0014/0016). **Meter mono-only** (H1 carry-over) is
  re-assessed and fixed **here** — Meter is an H3 mixer node and a stereo mixer needs stereo metering.

**5. Automation lanes — ON THE CRITICAL PATH (gate (a) depends on it)** — *exists: `FaderNode` event seam;
`Automation.h` does no interpolation; `AutomationCurveType` stored but never read.* Build the real ADR-0009
surface: curve **interpolation reading `AutomationCurveType`**; lane storage/ownership/persistence; a
per-source **monotonic read cursor + loop/seek re-seek**; PDC shift; render events at per-Block offsets
feeding param changes **through the hosted node**. Ship the negative-controlled **NaN/inf automation-event**
test here (the coverage Finding B's clamp still lacks).

**6. (coverage, post-gate)** Out-of-process **scanner** + VST3/AU load via `AudioPluginFormatManager` —
enables the real-plugin pluginval/auval leg.

**7. (tail, post-gate)** **CLAP** hosting — alpha loader in the sandboxed child only.

**8. Finding F — sidechain in the projection.** `MixerGraphProjection` has **no sidechain pin path at all**
(the `SidechainGainNode` exists but the projection never wires it). **Either** add projection work (sidechain
field → `SidechainGainNode` aux edge + 2-input build check + PDC-alignment assertion, ADR-0014) **or**
name-defer with a reason. It is a **missing feature, not a guard** — do not "close" it over a path that
doesn't exist.

---

## 3. Findings & deferrals ledger — nothing implicit (per-item)

**Review findings:**
| Finding | Disposition |
|---|---|
| **A** mute 64-node ceiling | **DONE** (ADR-0016). *(Re-flagged by a critic; verified already fixed — rejected.)* |
| **B** fader event-path clamp | **DONE** (`FaderNode.h:80`). *(Re-flagged; verified fixed — rejected. Its missing NaN/inf **test** is folded into item 5.)* |
| **C** PluginNode block-size | **DONE** (`b70ed02`, CI green) |
| **D** `reset()` races live child | item **2** — gate-blocking |
| **E** channel-count debug-only guard | item **2** — gate-blocking (release-safe clamp) |
| **F** sidechain not in projection | item **8** — add the path OR name-defer (missing feature, not a guard) |
| **G** duplicate send doubles | item **4** — projection dedup + GraphBuilder edge decision + neg-control test |
| **H** coordinator resetState leaks pipelines | item **3c** |
| **I** blacklist no plugin identity | item **3d** (keyed `{format,uid,version}`) |
| **J** layering gate checks only direct links; GUI app transitively links hosting | harden gate to transitive/symbol closure + assert the app target, **OR** amend ADR-0015 "engine AND app" wording |
| **K** test-integrity (self-labeled crash; always-false persistence; no PID-kill assert) | folded into gate **(b)** as named negative-controlled assertions + item **3** |
| editor-UI embedding | **DEFER** (ADR-0015 reason) — explicit, on the record |
| pluginval/auval blocking-ness | **non-blocking**; in-repo plugin supersedes (ADR-0015) — explicit line |

**Carried-forward H1/H2 deferrals (one-time sweep, per-item — no "mostly"):**
| Item | Disposition |
|---|---|
| H1 concurrency-stress-degenerate untested | → **H6** (reliability/soak) |
| H1 PDC feedback/SumNode untested | fix-now if cheap during item 3/4 graph work, else → **H6** |
| H1 `reset()` RT-unenforced | folded into item **2** (reset quiescence) |
| H1 **meter mono-only** | **H3 NOW** — item **4** (stereo mixer needs stereo metering) |
| H2 macOS `F_FULLFSYNC` | → **H6** (durability hardening) |
| H2 open re-hashes every file | → **H6** (open-time perf) |
| H2 split/trim fade re-clamp | → **H2-tail / H6** |
| H2 property-based undo | → **H6** (test depth) |
| H2 stereo decode | → **H4/H6** (needed when stereo assets are user-facing) |

*An item is closed only when DONE or carrying a named-horizon tag + reason.*

## 4. Cadence

Build **1 → 2 → 3 → 4 → 5** depth-first; each session ends green on the generic gates, the **exit gate stays
RED** until the cumulative work turns each clause green (PDC tri-stream → running-watchdog kill → auto
recompile+placeholder → blacklist-persists → emit-NaN-finite → state round-trip). When the whole
`YesDawHostIsolationCheck` is green **and an independent adversarial pass signs off**, **H3 closes.** Only
then does H4 begin. Items 6–8 are post-gate coverage/tail.

## Acceptance (H3 is done when ALL are true)
- [ ] `YesDawHostIsolationCheck` green in CI — every clause of §1 (a)/(b)/(c), each negative-controlled.
- [ ] `CompiledNodeKind::Placeholder`, OS-shared-memory IPC, a running watchdog, and a persistent blacklist
      store all exist (the gate's asserted artifacts are real).
- [ ] Mixer projection driven by `Runtime` from a production caller (not test-only).
- [ ] Automation lanes evaluate curves through the hosted node (gate (a) tri-stream green).
- [ ] Every review finding A–K DONE or named-horizon-deferred with a reason.
- [ ] Every H1/H2 carried-forward deferral dispositioned per-item.
- [ ] `loop/horizon.md` records the H3 exit criterion + green command.
- [ ] Independent adversarial review signed off.
