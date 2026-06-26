---
title: H3 close-out plan — finish the horizon to its exit gate, zero stragglers
type: plan
date: 2026-06-26
status: active
---

# H3 close-out plan

**Why this exists.** H3 ("mixer + plugin hosting") was being built *breadth-first* — a little of every
sub-feature, each commit green against the **generic** CI gates — but H3's **exit criterion was never
turned into a CI test**, and the `loop/` infrastructure the build plan designed (`loop/horizon.md` =
"exit criterion + exact green commands") was never created. So "is this commit green?" was checked 50
times; "is H3 done?" was never checkable. Result: a hosting *skeleton* that passes the generic gates while
the horizon's real finish line (a crashing plugin is isolated with no dropout) is untested and unmet.

This plan closes H3 the only way that prevents stragglers and backtracking: **define the exit gate as a
real, initially-RED CI test, build depth-first until it is green, and do not close H3 — or touch H4 —
until it is.** Done becomes mechanical, not a judgment call.

## The rules this plan runs under (the process fix)

1. **Exit gate first, and RED.** Item 1 below is the H3 exit-gate CI test. It starts failing (no real
   hosting exists). H3 is "done" *iff* it is green. No optimistic "complete."
2. **Every test negative-controlled.** A test counts only once it has been shown to FAIL when the code it
   guards is broken (as done for the fader/mute/block-size fixes; as the old mono-blind render harness was
   *not*). Green-but-meaningless tests are banned.
3. **Independent adversarial review before green.** The builder does not grade its own homework; an
   independent adversarial pass (ground-truth + test-integrity hunt) signs off before the gate is declared
   green. Correlated blind spots are why self-review missed the skeleton.
4. **Depth-first to the gate.** Build the shortest path that turns the gate green, not the widest set of
   features. Anything not on the path to the gate is dispositioned (below), never left implicit.
5. **`loop/horizon.md` is created** with H3's exit criterion + the exact green command (the gate), so any
   loop runs against the *right* oracle from here on.

---

## 1. The H3 exit gate (write FIRST; starts RED) — `YesDawHostIsolationCheck`

A deterministic, dependency-free CI test (pure of external plugins) that, against an **in-repo synthetic
test plugin**, asserts the H3 exit criterion (roadmap + build plan §H3):

- **(a) PDC against a live high-latency plugin.** Two parallel paths, one through the test plugin reporting
  a non-trivial latency, stay sample-aligned — an impulse lands at the predicted compensated sample
  (extends the existing PDC-impulse test from a stub to a *hosted* plugin).
- **(b) Crash/hang isolation, no dropout.** A plugin that **crashes** or **hangs** mid-session is detected;
  the audio thread **fails open within the block budget** (last-good → silence → bypass), **never waits on
  the child, zero underruns**; a **"plugin crashed" placeholder is swapped in via recompile**; the offender
  is **blacklisted** (persisted, ADR-0013). All asserted mechanically (xrun count = 0; output defined every
  block; placeholder present after recovery; blacklist entry exists).
- **(c) Opaque state across IPC.** Plugin state chunk save → restore survives a round-trip across the real
  process boundary.
- **pluginval L8–10 + `auval`** run on runners that have real plugins (**non-blocking**); the in-repo test
  plugin is the **always-on** blocking gate (ADR-0015).

Green command recorded in `loop/horizon.md`. This file is the contract for "H3 done."

---

## 2. Gap to the gate — depth-first build order

Each item: *what exists → what's missing → why the gate needs it.* Build in this order.

| # | Item | Exists today | Missing (the work) | Gate dep |
|---|---|---|---|---|
| 1 | **In-repo synthetic test plugin** (passthrough / fixed-latency / emit-NaN / hang / crash-on-cue) | nothing | the whole thing — the deterministic gate's subject | (a)(b)(c) |
| 2 | **Real cross-process IPC** | in-process `RtLaneRing` stub + `YesDawPluginHost` worker skeleton (sleeps) | promote the ring to real shared memory (mmap / CreateFileMapping); the worker runs the ring as a child against a hosted processor | (a)(b)(c) |
| 3 | **Coordinator executes** crash→bypass→recompile and watchdog→kill→**blacklist** | metadata skeleton; every `*Executed/*Applied/*Persisted` flag hardcoded `false`; no plugin identity | make the actions REAL: a graph recompile that swaps a placeholder; a persisted blacklist keyed by plugin UID (ADR-0013); a running watchdog on a live child | (b) |
| 4 | **Mixer projection → runtime** | `buildMixerGraphProjection` built + fixed, **no production caller** | wire it to a live `Runtime`/`Project` so mute/solo/sends/automation actually drive playback | (a) (host in a real mix) |
| 5 | **Automation lanes** honoring per-Block offsets | `FaderNode` event seam only | automation-curve storage → render events at per-Block offsets feeding param changes (ADR-0009) | (a) gate uses automation? confirm |
| 6 | **Out-of-process scanner + VST3/AU load** | nothing | `AudioPluginFormatManager`/`KnownPluginList` scan in the worker; blacklist-on-crash cache | real-plugin pluginval/auval leg |
| 7 | **CLAP hosting** | nothing | after VST3+AU green; alpha loader in the sandboxed child only | H3 tail (post-gate) |

**Note:** items 1–4 are the critical path to a GREEN gate. 5–7 widen coverage; 7 (CLAP) is explicitly the
tail. If any of 5–7 is not needed to turn the gate green, it is dispositioned, not silently carried.

---

## 3. Findings disposition (nothing implicit)

From the H3 adversarial review (`yesdaw-h3-complete-review.md`), each open finding is fixed *as part of the
item that touches its surface* — or deferred to a named horizon with a reason. No straggler.

| Finding | Disposition |
|---|---|
| **C** PluginNode block-size decoupling | **DONE** (`6f46986`/`b70ed02`, CI green) |
| **D** `RtLaneRing::reset()` races a live child | fix in **item 2** (real IPC — make reset quiescent vs the child) |
| **E** `exchangeBlock` channel-count debug-only guard | fix in **item 2** (release-safe bound) |
| **F** sidechain-only-main silent / no 2-input build check | fix in **item 4/5** (projection wiring guard) |
| **G** duplicate send doubles level | fix in **item 4** (projection dedup/guard) |
| **H** coordinator `resetState` leaves pipelines non-empty | fix in **item 3** (coordinator made real) |
| **I** blacklist carries no plugin identity / dedup key | fix in **item 3** (blacklist keyed by plugin UID) |

**Carried-forward deferrals from H1/H2 (one-time sweep — surface + disposition, don't lose them):**
- H1 test-coverage gaps: concurrency-stress-degenerate, PDC feedback/SumNode untested, `reset()` RT
  un-enforced, meter mono-only → **disposition each: fix-now if cheap, else tag → H6 (reliability) with a
  reason.**
- H2 deferred: macOS `F_FULLFSYNC`, open re-hashes every file, split/trim fade re-clamp, property-based
  undo, stereo decode → **mostly → H6 / H2-tail; each tagged, none implicit.**
- (Persistence cross-file atomicity stub already landed in H2 — resolved.)

*This table is the anti-straggler ledger. An item is closed only when it is DONE or has a named-horizon tag
+ reason.*

---

## 4. Cadence

- Build items 1→4 depth-first; each session ends green on the generic gates, the **exit gate stays RED**
  until the cumulative work turns each assertion green.
- The exit gate goes green **assertion by assertion** (PDC → fail-open/no-dropout → recompile-placeholder →
  blacklist → state-round-trip). When the whole `YesDawHostIsolationCheck` is green and an independent
  adversarial pass signs off, **H3 closes.**
- Only then does H4 begin. The roadmap's "no horizon closes until its exit gate is green" becomes literal.

## Acceptance (H3 is done when ALL are true)
- [ ] `YesDawHostIsolationCheck` green in CI (the in-repo crash-test plugin gate: PDC + no-dropout
      isolation + placeholder + blacklist + state round-trip).
- [ ] Every new test negative-controlled.
- [ ] Every review finding (C–I) DONE or named-horizon-deferred with a reason.
- [ ] H1/H2 carried-forward deferrals each dispositioned.
- [ ] `loop/horizon.md` records the H3 exit criterion + green command.
- [ ] Independent adversarial review signed off.
