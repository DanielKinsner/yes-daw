# H9 plan — Engine scaling & robustness

**Why this exists.** The engine renders and plays a Project correctly, but single-threaded, and several
robustness debts are still open. H9 makes the engine **scale across cores without changing a single
sample**, hardens it against bad input and misbehaving plugins, and pays down the cross-horizon debt
(H3 worker misbehavior + blacklist-on-failure; H4 MIDI auto-wire). It also closes the one structural
finding from the H8 review: transport state is not yet safe to drive concurrently with the audio thread.
Everything here is **headless and mechanically gated** — no human in the loop, no boundary stop between
checkpoints (continue headless, per Dan).

## Exit gate (headless)

Three legs, per ADR-0020:

- **Determinism (`YesDawSchedulerCheck`, the headline blocking gate):** the same Project graph produces
  **bit-identical** output across 1, 2, 4, and 8 worker threads — identical to the single-thread reference
  **and** to the H7 offline render — under RTSan/TSan. Parallelism must not move one sample.
- **Parallel soak:** the H6 heavy 100-track session, run through the multicore scheduler, holds p99.9 under
  the 128-frame block deadline with zero headless Underruns (the H6 oracle, parallel executor).
- **Clean fuzz:** structure-aware fuzzing of the `.yesdaw` bundle parser and the plugin opaque-state parser
  runs clean (no crash / UB / leak) under ASan+UBSan, via a deterministic seeded-corpus replay in CI
  (longer fuzzing is an owner-machine/scheduled run, like the soak).

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --test-dir build-ci -R "YesDawSchedulerCheck" --output-on-failure
```

## Build order

Each checkpoint is one small, independently-green commit; CI is the gate. **No code lands before the
decision it depends on is written as an ADR** — Codex writes each new ADR (grilled) before its dependent
code, and stops for Dan only on an ADR-level call it cannot resolve from the existing ADRs. No boundary
stop between checkpoints — continue headless.

1. **Kickoff docs.** This plan; switch `loop/horizon.md` to H9 + `YesDawSchedulerCheck`; add the H9 terms
   to `CONTEXT.md` (work-stealing scheduler, determinism gate, transport command queue).

2. **Concurrent transport safety — ADR-0023 + SPSC command seam + TSan gate.** *(Folds in the H8-review
   finding.)* Today `PlaybackEngine`'s `play/stop/locate/setLoop/clearLoop` mutate plain non-atomic state
   that `processBlock` reads on the audio thread — safe only single-threaded. Route those controls through a
   lock-free **SPSC command queue** (the ADR-0006 pattern the Runtime already uses for graph swaps), drained
   at the top of `processBlock`, so the audio thread becomes the **sole owner** of `playheadFrame_` / loop /
   `playing_`. Controls still validate synchronously (return `bool`) and enqueue; they apply at the next
   block boundary — which is the same observable timing as today, so the H8 single-thread sample-accuracy
   gates stay green. **Gate:** a new TSan test drives `locate`/`setLoop` from a writer thread while another
   thread pumps `processBlock` for thousands of small blocks — asserts no RT_FATAL trap, a consistent final
   playhead/loop state, and that the queued controls take effect. ADR-0023 decides the command set, queue
   capacity, and coalescing (e.g. last-locate-wins) before code.

3. **Multicore work-stealing scheduler — ADR-0024.** A parallel executor that runs the `CompiledGraph` DAG
   across a fixed worker pool, respecting node dependencies (topological readiness) and PDC. **RT-safe:** the
   pool is created in `prepare()`; steady-state workers never allocate, lock, log, or do I/O. The crux for
   determinism is that **summation order is fixed by graph structure, not thread arrival** — each node owns
   its output slot, and mixing/sum nodes accumulate inputs in a deterministic (compiled) order. ADR-0024
   decides the scheduler model (per-worker deque + work-stealing), pool lifecycle, and the determinism
   contract before code. Single-thread behavior must stay bit-identical (1 worker == today's serial path).

4. **Determinism gate — `YesDawSchedulerCheck` (H9 headline).** The blocking CI gate: build a non-trivial
   Project graph and assert its output is **bit-identical across worker counts {1, 2, 4, 8}**, equal to the
   serial reference and to the H7 offline render. Add it to the RTSan/TSan engine-layer target set so the
   parallel hot path is sanitizer-covered. A negative control: a deliberately arrival-order-dependent sum
   must FAIL the gate (proves the gate bites non-determinism, not just smoke-tests the happy path).

5. **Parallel-scheduler soak.** Run the H6 heavy 100-track session through the multicore scheduler and hold
   p99.9 under the block deadline with zero headless Underruns — either extend `YesDawReliabilityCheck` with
   a parallel mode or add a sibling gate sharing the H6 oracle. Proves the scheduler holds (ideally shows
   headroom) under real load, not just on a toy graph.

6. **Structure-aware fuzzing of the parsers.** A fuzz harness over the `.yesdaw` bundle (SQLite) parser and
   the plugin opaque-state parser, run under ASan+UBSan. **CI determinism:** the CI gate is a seeded
   corpus-replay (deterministic, exit 0/1); a longer libFuzzer run is an owner-machine/scheduled job, like
   the real-device soak. Seed the corpus with valid bundles plus the known malformed cases from H6/H7.

7. **H3 debt — worker misbehavior across IPC + blacklist-on-failure.** *(ADR-gated.)* Drive the worker's
   emit-NaN / fixed-latency / hang modes across the **real** IPC boundary in the gate (today only
   passthrough crosses), and wire the missing exit clause: a blacklist `FailureAction` that persists the
   candidate through the coordinator on crash/hang. **Gate:** a real failure → a blacklist row written by
   the coordinator → survives restart, while the host's fail-open reader keeps audio finite and
   sample-aligned through the real process. Needs an ADR (amend/extend ADR-0015 for the blacklist action)
   before code.

8. **H4 debt — auto-wire MIDI tracks + MIDI transport.** *(ADR-gated.)* Extend `ProjectMixerProjection` to
   walk `midiClips` (source → instrument → Fader/Pan/Meter), and make `DecodedMidiClipNode` honor
   `Transport::timelineFrame` so MIDI locates/loops sample-accurately like audio (the H8-review deferral —
   today the MIDI node only advances its own monotonic cursor). **Gate:** a Project with a MIDI clip plays
   through the runtime, and `locate(N)` / loop reproduce the expected events at the right frames. Needs the
   instrument-track modeling decision (what instrument, how chosen/persisted) as a small ADR before code.

9. **Close.** H9 exit green: `YesDawSchedulerCheck` (determinism, RTSan/TSan), the parallel soak, and the
   fuzz replay, all green on the full CI matrix.

## Non-goals (H9)

- No UI / timeline / mixer surface — that is H11.
- No new mastering/interchange features (loudness, DAWproject, time-stretch, device hot-swap) — that is H10.
- No sample-rate conversion or aggregate-device handling.
- Fuzzing targets the parsers (bundle + plugin state); full engine fuzzing beyond that is out of scope.

## Decisions to write (ADRs, in order)

- **ADR-0023 — transport command model** (CP2): SPSC control→audio command seam; command set; coalescing.
- **ADR-0024 — multicore graph scheduler** (CP3): work-stealing model; pool lifecycle; determinism contract.
- **Blacklist-on-failure action** (CP7): amend/extend ADR-0015 with a blacklist `FailureAction`.
- **Instrument-track modeling** (CP8): the small decision behind MIDI auto-wire.

## Status

Not started. Kickoff is CP1 (docs). Build order above is the long-horizon task: each checkpoint small +
independently green, CI the gate, ADRs before their dependent code, no boundary stop. Claude reviews the
close-out adversarially before H9 is called done.
