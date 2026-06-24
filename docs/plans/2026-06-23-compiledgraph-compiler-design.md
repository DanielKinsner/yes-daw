# CompiledGraph compiler — locked implementation design (ADR-0007)

**Status:** built-in Nodes DONE + CI-green; the 5-pass compiler is the remaining work.
**Source:** chosen by a 4-design adversarial panel + 3 judges (like the concurrency core). Spine =
*Testability / Incremental Landing*; grafts folded in from *PDC-Correctness*, *RT-Safety/Buffer-Pool*,
and *Simplest-Correct*. This note is the single source of truth to build the compiler from. It does **not**
change ADR-0007/0008 (those are frozen); it is the implementation plan under them.

## What is already landed (CI-green)
The built-in Nodes, each its own independently-green commit, all pure C++ behind the frozen Node trait
(ADR-0008), all `process()` `YESDAW_RT_HOT`, all with cross-block-size invariance gates:
- `src/engine/nodes/DelayNode.h` — the ONE delay primitive (PDC + feedback); `using LatencyNode = DelayNode`.
  Pow-2 ring, mask-indexed, **write-then-read** so delay 0 is a true pass-through.
- `src/dsp/LinearRamp.h` — per-frame linear ramp (JUCE-free `SmoothedValue<Linear>` stand-in).
- `src/engine/nodes/FaderNode.h` — ramped linear gain; atomic target = the SetGain seam; in-place eligible.
- `src/engine/nodes/PanNode.h` — equal-power mono→stereo; per-instance cos LUT (no per-frame trig).
- `src/engine/nodes/SumNode.h` — f64 Bus summing; sorts inputs by producer NodeId (canonical order);
  stores resolved `const float*` (pool addresses are stable) → decoupled from CompiledGraph.
- `src/engine/nodes/MeterNode.h` — peak/RMS tap; single atomic release-store per metric (no CAS); in-place.

## Invariants (do not relax)
- `src/engine/Node.h` is FROZEN. No fields/methods added, no `ProcessArgs` widening. Multi-input plumbing
  uses **non-virtual** `bindInputs()/bindEdges()` setters on concrete subclasses, called once by the
  builder on the control thread. (This is why `processedOutput()` is NOT added to the trait — buffer
  assignment lives in the compiler's `CompiledNode`, not on `Node`.)
- The legacy `CompiledGraph(GraphId, float identityDc)` ctor, `process(float*, int)` signature,
  `identityDc()`, `aliveCount()`, and the `canary_/kPoison` UAF tripwire are PRESERVED unchanged. Every
  existing runtime/compiledgraph/node test stays byte-identical green at every commit. No existing test is
  modified in this chunk; the legacy ctor is a permanent test seam behind an `isDegenerate_` fast path.
- Engine stays JUCE-free; header-first; builds clean on MSVC /W4 /WX, Clang 20 -Wall -Wextra -Werror, GCC.
  Pure-test legs (`YESDAW_BUILD_APPS=OFF`) compile every new file; RTSan covers every `process()`, TSan
  covers Runtime publish/swap/reclaim.
- Audio thread never allocates/locks/logs/syscalls. The compiler runs on the control thread (may
  allocate). `~CompiledGraph`, `~unique_ptr<Node>`, and `Node::release()` run on the janitor (control)
  thread — document this at the top of CompiledGraph.h so a future PluginNode's `releaseResources()` is safe.

## CompiledGraph layout (extend the existing header, additive)
Add (legacy fields/ctor unchanged; new members zero-init on the degenerate path):
- `enum class CompiledNodeKind { IdentityDc, Oscillator, Source, Fader, Pan, Sum, Meter, Delay, Latency, Master, Plugin };`
- `struct InputSlot { SlotIndex fromSlot; uint16 producerNodeIdx; }` (`SlotIndex = uint16`, `kSilenceSlot=0`, `kNoSlot=0xFFFF`).
- `struct CompiledNode { Node* node; NodeId id; uint16 numInputs, numChannels; uint32 inputsBegin;
  SlotIndex outputSlot; DSlotIndex busAccumSlot; int64 pathLatency; uint8 muteBit; CompiledNodeKind kind;
  bool aliasOk; uint8 _pad; }` with `static_assert(sizeof(CompiledNode) <= 64)`.
- `struct BufferPoolLayout { uint16 numFloatSlots(>=1); uint16 numDoubleSlots; uint16 maxChannelsPerSlot; uint32 maxBlockSize; }`
- members: `vector<unique_ptr<Node>> nodeStorage_`, `vector<CompiledNode> compiledNodes_` (topo order, Master
  last), `vector<InputSlot> inputSlotIndices_`, `unique_ptr<float[]> floatStorage_`,
  `unique_ptr<double[]> doubleStorage_`, `vector<float*> floatSlotPtrs_` (`[slot*maxCh+ch]`),
  `vector<double*> doubleSlotPtrs_`, `BufferPoolLayout poolLayout_`, `int64 totalLatency_`,
  `atomic<uint64> muteMask_`, `SlotIndex masterOutputSlot_`, `uint16 masterChannels_`,
  `vector<pair<NodeId,uint32>> idIndex_` (sorted, for O(log V) command routing).
- `struct Payload { ... }` moved-in by `explicit CompiledGraph(Payload&&)`. `bool isDegenerate_`.
- Slot 0 (`kSilenceSlot`) is permanently zero (R6): the allocator never hands it out as an output;
  unwired inputs / empty-graph master read it.

## The 5 passes — `src/engine/GraphBuilder.h` (header-first, control thread)
`static unique_ptr<CompiledGraph> GraphBuilder::build(Inputs&&, GraphBuildError* = nullptr)`. Whole
pipeline is **O(V+E)** — never path enumeration.
1. **Flatten & validate.** id→index map; reject DuplicateNodeId / MissingNode; clamp channels to
   `[1, kMaxChannelsPerNode=8]`; clamp ownLatency to `[0, kMaxLatencyCap = 60*192000]` and **reject**
   out-of-range LOUDLY (`GraphBuildError::LatencyOutOfRange{nodeId}` — not a silent clamp). A Send = an
   edge to a Bus SumNode (tap index = pre/post-fader). Empty project → a single Master reading silence.
2. **Iterative DFS topo from Master backward.** Explicit work-stack reserved to `nodes.size()` (NO
   recursion — deep bus trees blow MSVC's stack). 3-colour; a non-DelayNode back-edge = `CyclicGraph`;
   DelayNode back-edges allowed. Output = reverse post-order (producers first, Master last).
3. **PDC longest-path + splice.** `pathLatency[v]`: source = ownLatency; single-input = accumulate (NO
   splice); convergence (≥2 inputs) `target = max(pathLatency[u])`, splice `LatencyNode(target − pathLatency[u])`
   onto each shorter input (skip if delta 0), `pathLatency[v] = safeAddI64(target, ownLatency[v])`.
   `safeAddI64`: `__builtin_add_overflow` on Clang/GCC, manual sign check on MSVC; overflow → LatencyOutOfRange.
   Synthetic LatencyNode key `= FNV1a_64(consumerIdx, producerIdx) | (1<<63)` (high-bit namespace so it can
   never collide with a user NodeId; CompiledNode stores low 32 bits, full 64-bit key in a side map for the
   DelayCache). Spliced delays are single-input → can't create back-edges → Pass 2's invariant survives.
4. **Buffer pool — greedy liveness/interval allocation, sized to WIDTH.** `lastReader[v] = max topoIdx of
   any consumer` (Master = order.size()). Free list per channel-count (LIFO for L1). Walk topo: release each
   input slot whose producer's `lastReader == topoIdx`; **R3 in-place** if (1 input) ∧ (this == its last
   reader) ∧ (channels match) ∧ (neither is a SumNode) ∧ (kind ∈ {Fader, Meter}) → reuse input slot,
   `aliasOk=true`; else pop/allocate a slot (never slot 0). SumNode also gets a `busAccumSlot` from the
   double pool. Throw if any output lands on slot 0.
5. **Mute mask + carry-over + bind + publish.** Assign `muteBit`; allocate + zero `floatStorage_`/
   `doubleStorage_`, build pointer tables; for each multi-input node sort inputs by producer NodeId then
   call its `bindInputs/bindEdges`; carry delay-line state from `previousForCarryOver`'s DelayCache by key
   (matching length → memcpy; mismatch → longest-common-prefix + zero-fill); `prepare()` every node; build
   `idIndex_`; DEBUG-assert every multi-input node was bound. Construct via the Payload ctor.

### Written buffer-pool aliasing/lifetime contract (R1–R7, doc-comment in CompiledGraph.h)
R1 exactly one producer per slot per Block. R2 a slot's last consumer runs before reuse
(`slotLastReader[s] < currentTopoIdx`). R3 in-place only under the predicate above. R4 no pooled buffer
reused while a reader is pending (sidechains respected via `lastReader` over ALL readers). R5 f64 bus temps
are per-SumNode scratch (1:1), never aliased with f32. R6 slot 0 read-only silence. R7 FTZ/DAZ at the top
of `CompiledGraph::process()` via `src/dsp/ScopedNoDenormals.h` (new, RAII, SSE MXCSR + ARM FPCR).
DEBUG-only canary: paint non-silence slots with signalling-NaN at process() entry so any R1/R2 violation
propagates NaN and trips the order-shuffle test mechanically.

### `CompiledGraph::process(out, numFrames)` — audio hot path
`if (isDegenerate_) { fill identityDc_; return; }` → `ScopedNoDenormals` → (DEBUG canary fill) → load mute
mask → for each `CompiledNode` in order: resolve output channel ptrs from `floatSlotPtrs_`; if muted, memset
its slot; for a single-input non-aliased non-multi-input node, pre-copy its input slot into its output slot
(SumNode/Delay read the pool directly via their bound ptrs, no pre-copy); build `ProcessArgs` over the
output slot; one virtual `node->process(args)`. Finally copy the Master's output slot channel 0 to `out`
(mono-out at H1).

## Remaining commit slices (each independently green; do NOT break existing tests)
- **F. CompiledGraph state extension** — add the types/members/Payload ctor behind `isDegenerate_`; legacy
  path unchanged; add `ScopedNoDenormals.h`; `static_assert(sizeof(CompiledNode)<=64)`; R1–R7 doc-comment.
  Gate: all existing tests byte-identical green (it adds no reachable audio path yet).
- **G. GraphBuilder Pass 1+2 + MasterNode + IdentityDcNode** — validate + iterative topo; Pass 3–5 stubbed
  (one slot/node, no splice, no carry-over). New `YesDawBuilderCheck`: IdentityDc→Master renders the DC;
  Osc→Master is non-DC; 1000-node chain doesn't blow the stack; non-Delay back-edge = CyclicGraph.
- **H. Pass 3 PDC + StubLatencyNode + impulse test** — POSITIVE impulse (peak 2.0 at exactly frame N),
  NEGATIVE (splice disabled → two peaks), `totalLatency()==N`, single-input chain has no spurious
  LatencyNode, INT64_MAX/negative latency rejected.
- **I. Pass 4 buffer pool + order-shuffle invariance** — greedy allocator; diamond compiled two ways →
  bit-identical; width (not edge) sizing; in-place R3 verified; compile-cost O(V+E) curve-fit;
  DEBUG canary.
- **J. Pass 5 mute + carry-over + bind-check** — mute flip without recompile; carry-over continuity
  (match) and no-NaN (mismatch); deterministic input-sort; `DelayCache` (Runtime::reclaim snapshots rings
  before delete).
- **K. SetGain/SetPan seam** — Runtime routes commands to `CompiledGraph::applySetGain/applySetPan` via
  `idIndex_` binary search; degenerate graphs return false (back-compat); existing `scalarsApplied` test
  byte-identical.

## Open decisions for Dan (recommended defaults already chosen; override anytime)
1. **Mono-out at H1?** → **A: keep `process(float*, int)` mono** (Master copies channel 0; stereo is a clean
   later overload). Keeps existing tests byte-identical; H1 is engine+PDC+built-ins, not device-out.
2. **Legacy `(id, dc)` ctor fate?** → **A: permanent test seam** (one predictable branch; future migration
   to a `makeConstantDcGraph` helper is a separate focused commit).
3. **R3 in-place whitelist?** → **B: Fader + Meter** (both safe passthroughs; avoids a per-block memcpy on
   the common Fader→Meter→Master tail; Latency-delay-0 is too rare to bother).

## Corrections already applied vs the raw panel output
- Include convention is rooted at `src/` (`#include "engine/Node.h"`), NOT `"../Node.h"` (the panel's
  sketch would fail — only `${YESDAW_SRC_DIR}` is on the include path).
- DelayNode writes-then-reads (panel sketched read-then-write, which breaks delay 0).
- SumNode stores resolved `const float*` + sorts inputs itself (panel had it hold `InputSlot + CompiledGraph*`);
  cleaner layering, same correctness (pool addresses stable).
- f64 cancellation test uses 1e8 (not 1e30 — at 1e30 the double gap also swallows 1.0, so it proved nothing).
- Built-ins split into one commit per node (panel had a single "built-ins" commit) for higher commit count
  and smaller blast radius.
- `ScopedNoDenormals` lands with slice F (where `CompiledGraph::process` first uses it), not with the nodes
  (a header nothing includes wouldn't be compiled/tested).
