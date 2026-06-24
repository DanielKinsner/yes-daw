# 0008. The Node contract — one CLAP-shaped, format-neutral processing unit

- **Status:** Accepted
- **Date:** 2026-06-23
- **Deciders:** Dan (owner), build agent (H1)
- **Related:** ADR-0002 (#3 one format-neutral node contract), ADR-0007 (the graph compiles Nodes),
  ADR-0009 (the event stream flows through `process`), ADR-0010 (variable Block / time),
  [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) decision #3,
  [deepening notes](../plans/2026-06-23-yes-daw-deepening-notes.md) → *The Node contract*,
  `CONTEXT.md` (Node, Plugin, Graph).

## Context

Built-in DSP and hosted third-party plugins must share **one** interface, so that adding plugin
hosting later (H3) is an adapter, not an engine rewrite (ADR-0002 #3). The exact shape of that
interface — the `process` signature, the lifecycle, and the properties a Node advertises — is
load-bearing: the graph compiler, PDC, the buffer pool, and the event router all program against it,
and every built-in Node implements it. Changing it later touches every Node at once, so it is frozen
at H1 even though most Nodes don't exist yet.

## Options considered

1. **Adopt `juce::AudioProcessor` as the internal Node contract.**
   - Pros: free; every JUCE plugin already is one.
   - Cons: drags the whole hosting/`AudioProcessorGraph` model into the engine core; message-thread
     coupling; no first-class place for our event stream or compile-time PDC. Rejected (ADR-0004).
2. **A CLAP-shaped, format-neutral trait of our own; `juce::AudioProcessor` only as an adapter target.**
   - Pros: matches the format we most respect (CLAP), keeps the engine free of hosting headers, gives
     our event stream and PDC first-class slots; built-ins and the `PluginNode` adapter look identical
     to the graph.
   - Cons: we own and must test the contract. Accepted.

## Decision

**Every processing unit implements one CLAP-shaped `Node` trait.** Shape (frozen; field names may be
refined, the contract may not):

- **`NodeProperties { bool producesAudio; bool producesEvents; int channels; int64 latencySamples;
  NodeId id; }`** — what the Node advertises to the compiler.
- **`ProcessArgs { AudioBlock audio; EventStream& events; const Transport& transport;
  int32 numFrames; }`** with `numFrames ≤ maxBlock` (variable Block, ADR-0010).
- **Methods:** `properties()`, `directInputs()`, `prepare(sampleRate, maxBlock)`, `process(args)`,
  `reset()`, `release()`, plus a compiler-only `processedOutput()` (where this Node's output landed in
  the buffer pool this Block).
- **`process()` is real-time-safe (`YESDAW_RT_HOT` / `[[clang::nonblocking]]`, `noexcept`).
  `prepare()` is the only place a Node allocates.**
- **Per-block evaluation is a rule, not a suggestion:** fades, clip-gain, pan laws, and automation are
  evaluated per-Block with a ramp/smoother or lookup table — never `std::cos`/`std::pow` per frame in a
  read path (`juce::SmoothedValue` Multiplicative for dB/Hz with a clamped floor, Linear for pan and
  fade ramps).
- **`PluginNode` is the only place `juce::AudioProcessor` exists** — enforced by a layering test (the
  engine target must not link the hosting headers). It caches latency *after* `prepareToPlay` (scan-time
  latency is a lie) and translates our event stream ↔ `MidiBuffer` at that boundary only.
- **Validate plugin-reported `NodeProperties` before they reach PDC/pool:** clamp `latencySamples` to a
  sane max, reject negatives, checked arithmetic in the latency walk, validate channel/block claims —
  a plugin reporting `INT_MAX` latency must be quarantined, not crash the engine.
- **One driver, one `Clock` interface, two implementations** (device-paced RT vs free-wheeling offline)
  plus a capture-FIFO flag for record — so the golden RT-vs-offline render test stays honest (same
  driver, swapped clock) and the offline path **is** the shipping Render/Export path.

## Consequences

- **Positive:** plugin hosting becomes an adapter behind one trait; the engine core never includes
  hosting headers; PDC and events have first-class slots; RT-safety is a type-system property on
  `process`; offline Render can never silently diverge from RT.
- **Negative / accepted costs:** we own the contract and its tests; the per-block-evaluation rule needs
  a review/debug gate to enforce; the layering test must exist from the first `PluginNode`.
- **Follow-ups:** CONTEXT.md Node entry already says built-ins and plugins are both Nodes — no change
  needed. Tests this implies: RTSan coverage on every `process`; a layering test (engine ⇏ hosting);
  the cross-buffer-size invariance test (already green at H0) extends to every new Node.
