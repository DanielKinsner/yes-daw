# 0006. Immutable compiled-snapshot concurrency — atomic swap + janitor reclamation

- **Status:** Accepted
- **Date:** 2026-06-23
- **Deciders:** Dan (owner), build agent (H1)
- **Related:** ADR-0002 (#1 audio thread never blocks, #5 lock-free UI↔audio), ADR-0007 (the
  `CompiledGraph` being swapped),
  [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) decision #1,
  [deepening notes](../plans/2026-06-23-yes-daw-deepening-notes.md) → *Threading & the real-time
  boundary* and ADOPT #1/#2/#10, `CONTEXT.md` (Audio thread, Control thread, Snapshot — new).

## Context

The audio thread must never allocate, lock, log, or do I/O (ADR-0002 #1), yet topology and parameters
change live from the control thread. This is the single hardest correctness problem in the engine and
the classic "passes review, glitches under load" bug. The mechanism — how a new graph is published and
how the old one is freed — is irreversible: it is woven through every Node, the compiler, and the
command path. STATUS flags an independent **Codex re-verify** of this model in flight; this ADR records
the resolved design as the frozen target. Any divergence the re-verify surfaces supersedes via a new
ADR (append-only); it does not silently edit this one.

## Options considered

1. **`std::atomic<std::shared_ptr<CompiledGraph>>`.**
   - Pros: lifetime "just works."
   - Cons: lock-free on almost no platform (libstdc++/MSVC fall back to a spinlock table →
     priority inversion); `load()` bumps a refcount whose decrement-to-zero runs a **destructor on the
     audio thread**. Rejected — this is exactly the bug class.
2. **Lock around the graph.** Rejected outright (ADR-0002 #1).
3. **Raw `atomic<const CompiledGraph*>` + off-thread janitor reclamation (RCU-style).**
   - Pros: lock-free on every platform; the audio thread only ever does one `acquire`-load per Block and
     never writes lifetime; reclamation happens on a low-priority thread.
   - Cons: we own the grace-period logic. Accepted.

## Decision

**The audio thread reads an immutable `CompiledGraph` via `std::atomic<const CompiledGraph*>`; it never
writes lifetime.** One `acquire`-load per Block; never `delete`, never drop the last reference, never
allocate.

- **One ordered SPSC queue carries both topology and scalars.** `SwapGraph{const CompiledGraph* next}`
  is a **variant in the same queue** as `SetGain`/`SetPan` — two queues would reorder and apply a
  `SetGain` to a node that only exists post-swap. Control→audio = `choc::SingleReaderSingleWriterFIFO`
  (named, battle-tested — not three hand-rolled FIFOs).
- **Audio→control comms are named, not categorized:** a single scalar (meter Level, playhead samples) =
  `std::atomic` with `release`/`acquire`; a multi-word coherent snapshot (playhead `{ppqPos, barBeat,
  isPlaying}`) = a **SeqLock** (wait-free for the audio-thread writer).
- **Publish ordering is release/acquire** — `release` on the store, `acquire` on the load. Not
  `relaxed` (audio thread could see the pointer before the writes that built the graph), not blanket
  `seq_cst` (needlessly slows the per-Block load).
- **Janitor = grace-period generation counter** (start here, simplest correct): the audio thread
  publishes a monotonic `processedGen` at end of Block (`store(release)`); a low-priority **own thread**
  (~20 Hz — *not* a `juce::Timer` on a possibly-blocked message thread) frees a retired snapshot only
  once `processedGen > retiredAtGen` (RCU quiescent state). Escalate to a deferred-release "zombie"
  queue only for many small retirements; hazard pointers are overkill for a single reader.
- **Bound everything explicitly:** queue depth, recompile-swap rate, janitor backlog. Overflow policy is
  decided up front — coalesce repeated `SetGain` to the latest value, or backpressure the **control**
  thread, never the audio thread. A growing retired-graph list is a **bug signal** (audio thread
  stalled), surfaced not silently grown.
- **FTZ/DAZ** is set via `juce::ScopedNoDenormals` at the top of every callback and as the first action
  of every audio/worker thread (thread-local; on ARM64 *not* inherited by child threads — missing it is
  full-volume noise, not a stall).

## Consequences

- **Positive:** the audio thread is provably non-blocking on the publish path (RTSan-enforced); one
  ordered seam eliminates the post-swap reordering bug; reclamation is off the audio thread; the model
  is one named primitive at one altitude, not "RCU + double-buffer" in two places.
- **Negative / accepted costs:** we own and must test grace-period reclamation; a dedicated janitor
  thread; bounded queues mean a defined overflow/backpressure policy to implement.
- **Follow-ups:** CONTEXT.md gains **Snapshot**. Tests: RTSan on the whole publish/process path; a
  compile-and-publish stress test vs control-thread mutation asserts the published snapshot is never
  logically stale and that autosave only ever serializes control-thread truth (never a value read back
  from a snapshot); a latency-change-storm soak (no backpressure, no Underruns, no clicks). Narrow
  blueprint to internalize: Rowland's ADC20 graph + Doumler's two RT-comms patterns.

## Implementation note (2026-06-23) — how the publish mechanism was resolved

The Decision above names two candidate publication mechanisms — an `atomic<const CompiledGraph*>` the
audio thread acquire-loads per Block, **and** `SwapGraph` as a variant in the one ordered SPSC queue. A
4-design adversarial panel (+3 judges) resolved this tension in favour of the **queue-applied swap**, and
that is what `src/engine/Runtime.h` implements:

- The swap is applied **on the audio thread, drained from the ordered command queue** (so it stays
  ordered with the future `SetGain`/`SetPan` scalars — the whole point of one queue). The audio thread's
  current-graph pointer is therefore a **plain, audio-thread-local** `current_` — it needs no atomic
  because no other thread reads or writes it.
- The **release/acquire edge** the Decision requires is provided by the SPSC FIFO itself: `publish()`'s
  push is the release; the audio thread's pop is the acquire. The new graph's contents are fully visible
  the instant the audio thread installs it.
- A separate `atomic<const CompiledGraph*>` "what is the audio thread currently running" channel for
  observers (meters/UI) is **deferred** — handing observers a raw pointer needs a pin/hazard protocol to
  be use-after-free-safe, which is out of scope for this chunk.

All the load-bearing invariants of the Decision are unchanged: the audio thread **reads, never writes
lifetime**; reclamation is the off-thread generation-counter janitor (`processedGen > retiredAtGen`);
everything is bounded with backpressure on the control side. This note records the mechanism choice; it
does not change the decision.
