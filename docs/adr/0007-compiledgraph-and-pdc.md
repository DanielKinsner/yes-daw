# 0007. Own CompiledGraph with compile-time PDC; variable Block; f64 Bus summing

- **Status:** Accepted
- **Date:** 2026-06-23
- **Deciders:** Dan (owner), build agent (H1)
- **Related:** ADR-0002 (#2 DAG + per-node latency + PDC from day one), ADR-0004 (not
  `AudioProcessorGraph`), ADR-0006 (the snapshot that is published), ADR-0008 (the Nodes it compiles),
  [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) decisions #2/#9/#13,
  [deepening notes](../plans/2026-06-23-yes-daw-deepening-notes.md) → *The graph*,
  `CONTEXT.md` (Graph, Bus, PDC).

## Context

Routing is a DAG with per-node latency and plugin delay compensation from day one (ADR-0002 #2); we
build our own graph rather than use `juce::AudioProcessorGraph`, which forces rebuilds onto the message
thread and lacks first-class PDC (ADR-0004). How the editable routing **compiles** to the immutable
thing the audio thread runs — the passes, their complexity bounds, how PDC is computed, how buffers are
allocated, and the internal sample type — is irreversible: it is the engine. Getting PDC's complexity
wrong (path enumeration) or the buffer model wrong (O(edges)) is a rewrite, so the shape is fixed now.

## Options considered

1. **`juce::AudioProcessorGraph` as the real engine graph.** Rejected at ADR-0004 (message-thread
   rebuilds; no first-class PDC).
2. **Mutable graph the audio thread walks and edits in place.**
   - Pros: no compile step.
   - Cons: the audio thread would allocate/lock to mutate topology — violates ADR-0002 #1. Rejected.
3. **A compiler that produces a flat, contiguous, immutable `CompiledGraph` the audio thread only
   reads.**
   - Pros: audio thread just iterates a vector in order — no scheduling logic, no allocation; PDC and
     buffer layout are baked at compile time on the control thread.
   - Cons: a recompile-and-publish step (owned by ADR-0006). Accepted.

## Decision

**The control thread compiles routing into an immutable `CompiledGraph`; the audio thread only reads
it.** The compiler is a **5-pass O(V+E)** transform (never path enumeration):

1. Flatten & assign stable IDs (a Send materializes as an **edge** to a Bus `SumNode`; pre/post-fader =
   tap index).
2. **Iterative** DFS/topo order from Master backward (explicit work-stack — recursion blows deep bus
   trees); any non-`DelayNode` back-edge is a **compile error**.
3. **PDC = single-pass longest-path:** `pathLatency[v] = max over inputs(pathLatency[u]) +
   ownLatency[v]`; at each convergence node splice a `LatencyNode(target − pathLatency[u])` onto each
   shorter input. Single-input chains get **no** delay. (Never enumerate source→sink paths — exponential.)
4. Buffer-pool **greedy liveness/interval allocation,** sized to graph *width* not edge count; the
   aliasing/lifetime contract (when in-place is allowed; no pooled buffer reused while a reader is
   pending) is written, not implicit.
5. Mute mask + delay-state carry-over + publish.

- **One `DelayNode` primitive** serves both PDC (computed value) and feedback (fixed one-Block value) —
  one RT-critical component to harden.
- **Cache delay-line state across recompiles** (keyed by stable Node ID) so a latency change neither
  clicks nor page-faults.
- **`CompiledGraph` is flat, contiguous, immutable-after-publish:** `vector<CompiledNode>` in DFS order,
  flat `edges[]`, `BufferPoolLayout`, `vector<DelayLine> pdcDelays`, `atomic<uint64_t> muteMask`
  (flipped without recompile), `int totalLatency` reported to Transport.
- **Variable / renegotiable Block size** (decision #9): `prepare(maxBlockSize)`, `process(numFrames)`;
  Block size is never hardcoded.
- **f64 summing on every Bus** (decision #13): accumulate into a `double` temp buffer, narrow to `float`
  on output. The internal sample type is fixed now.
- **Empty-graph safety:** `published` starts `nullptr`; the callback outputs silence until the first
  install; the compiler emits a valid silence-outputting graph for a project with no tracks.
- **All built-ins report 0 latency at H1** — the PDC machinery exists and is exercised by a stub-latency
  Node + impulse test, so it is proven before any real plugin needs it.

## Consequences

- **Positive:** the audio thread never schedules or allocates — it iterates a flat vector; PDC is O(V+E)
  and correct by construction; the buffer pool is sized to width; adding real plugin latency at H3 is
  data, not a rewrite; the order-shuffle parallel-safety invariant (H1 test) has a written aliasing
  contract to check against.
- **Negative / accepted costs:** a real compiler with five passes to build and test; delay-state
  carry-over bookkeeping; f64 temp buffers per Bus (cost invisible against plugin cost).
- **Follow-ups:** CONTEXT.md gains **CompiledGraph**. Tests: PDC impulse test (stub-latency Node);
  cross-buffer-size invariance (64/128/512/prime-113); order-shuffle within topo-level == identical
  output; compile-cost bound curve-fits O(V+E) on the large-session fixture.
