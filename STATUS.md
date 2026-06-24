# YES DAW — STATUS (live handoff)

**Read this first on any machine.** This is the single source of truth for *where we are right now*.
The [plan](docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) and
[roadmap](docs/goals/roadmap.md) are the stable reference; **this** file is the live, constantly-updated
worklog.

> **Cross-machine rule:** `git pull` at the start of a session. At the end, update this file, commit in
> small chunks, and `git push`. Then the next machine — or the next session — is never lost.

**Last updated:** 2026-06-24
**Current horizon:** **H1 (the spine)** — in progress

> **Verification = CI.** A change is done when CI is green, not when Dan listens or watches. The only
> human step is blessing a golden on an intended audio change (`cmake --build --preset ci --target bless-goldens`).

---

## Now — between chunks (every engine commit to date is CI-green)
- **Latest: REVIEW/FIX of compiler slice F is in & locally green.** The review found one real lifecycle
  gap: `CompiledGraph` owns prepared Nodes but did not call `Node::release()` before destruction. That is
  fixed on the janitor/control-side destructor path and covered by a new `YesDawGraphCheck` lifecycle
  test. Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (48/48). **Next:**
  compiler slice G — `GraphBuilder` Pass 1+2 + `MasterNode`/`IdentityDcNode` + first real end-to-end
  render.
- **Previous: `CompiledGraph` compiler slice F landed, then the macOS warning was fixed.** The graph has
  the additive ADR-0007 state/layout surface (`Payload`, flat compiled-node metadata, input-slot table,
  buffer-pool layout, mute mask, master output bookkeeping, id index) behind the preserved legacy
  `(GraphId, dc)` degenerate fast path. `src/dsp/ScopedNoDenormals.h` landed with the written R1–R7
  buffer-pool contract; no builder/audio executor path is reachable yet. AppleClang's
  `-Wunused-lambda-capture` warning in `tests/pan_tests.cpp` is removed.
- **Previous: the five built-in Nodes are in & green.** `DelayNode` (the one PDC+feedback primitive;
  `LatencyNode` is an alias), `FaderNode` (ramped gain), `PanNode` (equal-power mono→stereo, LUT),
  `SumNode` (f64 Bus summing, canonical NodeId order), `MeterNode` (peak/RMS, lock-free publish) — each
  its own independently-green commit behind the frozen Node trait, each `YESDAW_RT_HOT` with a
  cross-block-size invariance gate. `src/dsp/LinearRamp.h` is the per-frame ramp helper. The locked
  compiler implementation design remains
  [docs/plans/2026-06-23-compiledgraph-compiler-design.md](docs/plans/2026-06-23-compiledgraph-compiler-design.md);
  build commits G–K from there.
- **The Node contract (ADR-0008) is frozen + green.** `src/engine/Node.h` is the CLAP-shaped
  trait (`NodeProperties`/`AudioBlock`/`ProcessArgs` + `prepare`/`process`/`reset`/`release`/`directInputs`);
  `process` is `noexcept` + `YESDAW_RT_HOT` (RTSan-clean). First built-in `OscillatorNode` (wraps
  `SineSource`); the H0 throwaway Node stub is retired and block-size independence is re-asserted through
  the real trait. `EventStream`/`Transport` are placeholders fleshed out by ADR-0009/0010. CI green on
  `787d854` (RTSan/TSan/3-OS).
- **Foundation: the RT-safe graph-swap core (ADR-0006) is in and green.** `src/engine/Runtime.h`
  is the seam between the control thread and the one audio thread: one ordered **choc SPSC** command
  queue carries `SwapGraph` (with a `SetGain`/`SetPan` seam reserved); the audio thread owns `current_`
  and reads an immutable `CompiledGraph`; retired graphs go to an audio→control queue and a
  **generation-counter janitor** frees them on the strict-greater `processedGen > retiredAtGen`
  fence-post. Design was chosen by a **4-design adversarial panel + 3 judges**; the must-fix grafts are
  in (retire-queue backpressure, trivially-copyable POD command, `static_assert` lock-free, a debug
  canary, INVARIANT comments). 25/25 Catch2 tests pass locally (MSVC); a 2-thread stress test is the
  **new TSan leg's** target. RTSan covers `processBlock`. *(A real bug surfaced + fixed: choc's
  `getFreeSlots()` over-reports by one, so the backpressure gate now uses `getUsedSlots()`.)*
- **Verification: GREEN.** CI on `747f46a` passed every leg — Windows/Linux/macOS build + ctest,
  **RTSan** (audio hot path never allocates/locks) and the **new TSan** leg (the release/acquire
  reclamation contract has no data race). The concurrency core is now mechanically proven, not argued.
  A 4-design panel + 3-judge design pass and a 3-reviewer adversarial code review (7 findings, all
  fixed) preceded green. *(One CI-only bug fixed post-push: a `Config cfg = {}` default arg MSVC
  accepts but Clang/GCC reject.)*
- **H0 carry-over decided:** the native GPU render shell + `max_frame_ms<16.6` soak gate is **folded
  into H2** (UI work). H1's exit is 100% headless CI, so it does not block. The audio soak still stands.

## Current-horizon checklist — H1 (the spine; plain English, small steps)
> Exit gate (all green in CI): a Project round-trips (tempo/meter map, markers, clips intact); the RT
> path matches an offline Render within golden tolerance; the audio path is RTSan-clean; **and** a kill
> during save/migration reopens cleanly (WAL recovery + `integrity_check`).
- [x] **Freeze the irreversible contracts as ADRs 0006–0012** ✓ — graph+PDC, time model, event model,
  Node contract, concurrency, data-model indirection, persistence. (docs-only; CI green by construction.)
- [x] RT-safe audio callback skeleton (`YESDAW_RT_HOT` + RTSan coverage) — `Runtime::processBlock`
  outputs silence from a `nullptr` graph, renders the installed graph otherwise. ✓
- [x] SPSC command queue + queue-applied graph swap + generation-counter janitor (ADR-0006) ✓ — one
  ordered choc queue (`SwapGraph`), audio-thread-local `current_`, audio→control retire queue, strict
  `processedGen > retiredAtGen` fence-post; backpressure not leak. RTSan + TSan legs cover it in CI.
  *(`src/engine/{CompiledGraph,Command,Runtime}.h`, `tests/{compiledgraph,runtime}_tests.cpp`.)*
- [ ] `CompiledGraph` 5-pass compiler with PDC wired in; all built-ins report 0 latency (ADR-0007);
  PDC impulse test + cross-buffer-size invariance + order-shuffle invariant as Catch2 gates. **Design
  locked** ([compiler-design note](docs/plans/2026-06-23-compiledgraph-compiler-design.md)); build
  commit F (CompiledGraph state) is done; next commits are G (Pass 1+2 + Master/IdentityDc) → H (PDC) →
  I (pool) → J (carry-over) → K (SetGain seam).
- [x] Built-in Nodes behind the contract (ADR-0008) — **all five in & green**: `OscillatorNode`,
  `DelayNode`/`LatencyNode`, `FaderNode`, `PanNode`, `SumNode` (f64 Bus summing), `MeterNode`. Each a
  separate green commit. *(Master = a top-level SumNode + device-wiring land with the compiler / H2.)*
- [ ] Generic event stream flowing param-changes (ADR-0009); automation evaluated sample-accurately.
- [ ] SQLite `.yesdaw` bundle: schema v1 + FKs + migration harness + intent-log atomicity (ADR-0012).
- [ ] **Exit gates green:** Project round-trip · RT-vs-offline golden diff · RTSan-clean ·
  kill-during-save/migration reopen-clean. H1 done when all four are green in CI.

## Previous-horizon checklist — H0 (closed; GPU render shell + 60fps gate folded into H2)
- [x] Install the C++ toolchain (CMake + MSVC via VS 2022 Build Tools). ✓
- [x] `cmake -B build` configures and fetches JUCE with no error. ✓
- [x] App builds and a window opens (`YesDaw.exe`). ✓ — *`Main.cpp` compiled clean first try.*
- [x] A 440 Hz tone plays out real hardware (spike #1: device round-trip core). ✓
- [x] **Stand up CI + a self-asserting check harness** ✓ — GitHub Actions (Win+Linux+mac) via the `ci`
  preset builds + runs Catch2 `YesDawCheck` (golden + Goertzel/zero-crossing 440 Hz + RMS/peak/symmetry/
  DC purity + fade + perf); RTSan leg (`-fsanitize=realtime`, Clang 20) enforces no-alloc on the hot
  path; warnings-as-errors; `bless-goldens`. Recorded in ADR-0005. *(green; see `docs/ci-mechanical-verification.md`)*
- [x] Tame the spike (fade-in / lower level) ✓ — 50 ms fade-in + `noteOn/noteOff` in `SineSource`,
  −20 dBFS default; asserted by the fade-in check. *(start-stop UI deferred — spike.)*
- [x] Real-machine soak harness built ✓ — `YesDawSoak` opens the real device, counts xruns/deadline-
  misses → PASS/FAIL; now enforces the **128-frame** target (`--block-size`, the roadmap stress case)
  and, with `--loopback`, that the captured tone is actually **440 Hz**. Run with `tools/soak.ps1`
  (native Windows, no Git Bash) or `tools/soak.sh`. Audio is clean (0 dropouts) on the owner's box, but
  the 128-frame target needs a **low-latency driver** (ASIO/WASAPI-exclusive — shared-mode Realtek
  forces 480). **Owner runs the 10-min gate; loopback needs an out→in jumper.**
- [x] Load + scrub one WAV ✓ — `YesDawAssetCheck` decodes a committed fixture WAV, golden-diffs the
  440 Hz sine (≤1e-4), recovers pitch (zero-crossings), and scrubs (sub-range read == slice, bit-
  identical). CI green on Win/Linux/mac. *(spike #1 complete)*
- [~] GPU timeline 100+ elements at 60fps (spike #2) — **CPU half done + green**: pure viewport
  virtualization (`src/ui/TimelineLayout.h`, `YesDawUiCheck`) lays out a 5000-clip viewport in
  **0.0069 ms/frame** (~2400× under the 16.6 ms budget), so the whole frame is the GPU's. *Remaining
  (real-hardware): a native GPU render shell + `max_frame_ms<16.6` in the soak (NOT yet implemented).*
  Native is the chosen direction (plan-recommended + this spike's cost validation); the formal UI-stack
  ADR (fork #2) is written at H1 — until then "native" is a strong lean, not a locked ADR.
- [x] One Node behind a stub of the format-neutral trait (spike #3) ✓ — `YesDawEngineCheck` drives a
  `ToneNode` via the trait at block sizes 1/31/128/512/4096/9000 → bit-identical output, finite, no
  denormals. *(throwaway stub; the real Node contract is frozen at H1.)*
- [ ] **Exit = two soak gates on a real machine** (no human judgment):
  - **(a) audio — IMPLEMENTED:** `soak.sh`/`soak.ps1` exits 0 with `xruns==0`, `deadline_misses==0`,
    `block_size<=128`, and (with `--loopback`) RMS>0.01 dominated by 440 Hz.
  - **(b) GPU 60 fps — NOT YET IMPLEMENTED:** `max_frame_ms<16.6` requires the native render shell that
    doesn't exist yet, so the soak does NOT check it — a soak PASS today is the AUDIO gate only.
  H0 is done when both are green on one machine at a 128-frame Block.

## Done recently
- 2026-06-23 — **Foundation** committed: research corpus, CONTEXT glossary, ADR-0001/0002, roadmap, CLAUDE.md.
- 2026-06-23 — **Brainstorm**: direction locked — full general-purpose DAW; C++/JUCE + our own engine;
  audio + MIDI co-equal; linear timeline; editing-first; long-horizon.
- 2026-06-23 — **Plan** written; ADR-0003 (product) + ADR-0004 (stack); roadmap rebuilt; docs reconciled.
- 2026-06-23 — **Deepen-plan** applied: deepening-notes companion; loops section; decision #14
  (sample-rate); 10 simplifications adopted (8 scope-cuts rejected — full scope kept); housekeeping.
- 2026-06-23 — **Loop workflow adopted in full**; **3 H1 conflicts resolved** (15360-tick grid /
  128-bit ULID / out-of-process hosting).
- 2026-06-23 — **Codex plan review applied** (all 7 findings, no scope cut): made the snapshot /
  state-ownership / graph-publication model exact; promoted bundle crash-recovery into H1's gate;
  fleshed the out-of-process host runtime + isolation gate; PDC test now covers automation + events;
  sample-rate → H1 + automation-curve added as decision #15; fixed stale docs (adr/README, CLAUDE.md).
- 2026-06-23 — **Codex review round 2 applied:** plugin-IPC nonblocking contract (audio thread never
  waits on a child — one-block pipeline + fail-open); per-run state arenas (RT vs offline never share
  state); fixed persistence contradiction; H1 recovery gate = save/migration (import-kill → H2);
  marked resolved conflicts historical.
- 2026-06-23 — **H0 kickoff:** committed CMake + JUCE scaffold + sine spike (`src/Main.cpp`), `AGENT.md`,
  `.gitignore`. Unverified until the toolchain is installed and it's built.
- 2026-06-23 — **H0 spike #1 core WORKING:** toolchain in (MSVC 19.44 / CMake), JUCE fetched + built,
  `Main.cpp` compiled clean **first try**, `YesDaw.exe` plays a 440 Hz sine out real hardware. Full
  stack proven end-to-end.
- 2026-06-23 — **Mechanical-first model + CI cheat-sheet** committed (`docs/ci-mechanical-verification.md`)
  + `bootstrap/windows.ps1` (idempotent one-command toolchain install; fixes the winget-quoting pain).
  Standing up CI is the agent's first H0 task. Commit rule: frequent, straight to main, no squash.
- 2026-06-23 — **CI + harness LIVE and GREEN** (the first H0 task, done in full): extracted a pure
  `SineSource` from the spike; Catch2 `YesDawCheck` (golden + pitch + level + purity + fade + perf);
  GitHub Actions 3-OS matrix via the `ci` preset; warnings-as-errors (SYSTEM-demoted deps); RTSan leg;
  `bless-goldens`; ADR-0005. An **adversarial multi-agent review** caught + closed two real gate holes
  (golden window inside the fade; asymmetric distortion passing) — both proven via injected-bug tests.
  Built the **real-machine soak** (`tools/soak.sh` + `YesDawSoak`); verified on this box.
- 2026-06-23 — **H1 contracts frozen as ADRs 0006–0012** (the precondition for engine code): time model
  + sample-rate (keep-original / resample-at-read), Node contract, event stream + automation (all four
  curves), CompiledGraph + PDC, immutable-snapshot concurrency, Asset→Clip→Project + 128-bit ULID,
  SQLite bundle + migrations. Two owner product calls made; the resolved forks recorded; CONTEXT.md +
  the ADR index synced. Docs-only checkpoint → CI green by construction. GPU render shell folded to H2.
- 2026-06-23 — **RT-safe graph-swap core landed (ADR-0006)** — `src/engine/{CompiledGraph,Command,Runtime}.h`:
  immutable graph + one ordered choc SPSC command queue (`SwapGraph` + scalar seam) + audio-thread-local
  `current_` + audio→control retire queue + generation-counter janitor (strict `processedGen>retiredAtGen`).
  Design from a 4-design/3-judge adversarial panel; grafts applied (backpressure, POD command, lock-free
  `static_assert`, canary, INVARIANT comments). New **TSan CI leg** added. 25/25 local; choc
  `getFreeSlots()` off-by-one found + fixed. choc pinned (`5685fb5`). Then a 3-reviewer adversarial code
  review (7 findings, all fixed: canary→always-on, dtor contract, null-publish guard, …) — CI green on
  `747f46a` (RTSan + TSan + 3-OS).
- 2026-06-23 — **Node contract landed (ADR-0008)** — `src/engine/Node.h` (CLAP-shaped trait) +
  `src/engine/nodes/OscillatorNode.h`; H0 stub retired; block-size independence re-asserted through the
  real trait; `process` RTSan-clean. CI green on `787d854`.
- 2026-06-23 — **CompiledGraph compiler design panel + all five built-in Nodes landed.** A 4-design
  adversarial panel + 3 judges chose the ADR-0007 compiler implementation (spine = incremental-landing;
  grafts from PDC-correctness / RT-safety / simplest-correct) → locked in
  `docs/plans/2026-06-23-compiledgraph-compiler-design.md`. Then five built-ins, each an
  independently-green commit behind the frozen Node trait: `DelayNode` (the one PDC+feedback primitive,
  write-then-read so delay 0 passes through), `FaderNode` + `LinearRamp`, `PanNode` (equal-power LUT),
  `SumNode` (f64 Bus summing, canonical NodeId order, f64-cancellation gate), `MeterNode` (lock-free
  peak/RMS). Each has a cross-block-size invariance gate; `ci` gate green at every commit (47/47 local).
  Fixed three real bugs in the panel's sketch (include convention, delay-0 read/write order, f64
  test using 1e30 instead of 1e8). The 5-pass compiler itself (commits F–K) is the next chunk.
- 2026-06-24 — **CompiledGraph compiler slice F landed.** `CompiledGraph` gained the additive ADR-0007
  state/layout surface and `Payload` constructor while preserving the legacy `(GraphId, dc)` degenerate
  fast path for existing Runtime/CompiledGraph tests. `ScopedNoDenormals` landed for the real node
  executor path. Local `ci` build + 47/47 tests green.
- 2026-06-24 — **macOS CI warning fix.** AppleClang rejected an unnecessary lambda capture in the
  PanNode block-size test under `-Werror`; removed the capture. Local `ci` build + 47/47 tests green.
- 2026-06-24 — **Slice F review/fix.** Reviewed `a642ce9` and `b8c8e7c` against the locked compiler
  design plus ADR-0007/0008. Fixed one lifecycle contract gap: `CompiledGraph` now calls
  `Node::release()` for owned Nodes on destruction, and a new graph lifecycle test asserts it. Local
  `ci` build + 48/48 tests green.

## Next
- ✅ **H1 contracts frozen** (ADRs 0006–0012); ✅ **RT-safe graph-swap core** (ADR-0006); ✅ **Node
  contract + all five built-in Nodes** (ADR-0008/0007) — all CI-green.
- **Next chunk: compiler slice G (ADR-0007).** Add `GraphBuilder` Pass 1+2 plus
  `MasterNode`/`IdentityDcNode`, then the first real end-to-end render. Existing
  Runtime/CompiledGraph/Node tests stay intact; the legacy `(id,dc)` constructor remains the
  `isDegenerate_` fast path. PDC (H), buffer pool (I), carry-over (J), and SetGain seam (K) follow.
  In parallel the **time model types (ADR-0010)** unblock the round-trip exit. Each new audio-thread
  function gets `YESDAW_RT_HOT` + RTSan; every commit green.

## Blocked / open threads
- Engine concurrency model (plan's *Threading & the real-time boundary* + *The graph* sections) is out
  for a **Codex re-verify** pass. H0 does not depend on it, so H0 proceeds in parallel.
