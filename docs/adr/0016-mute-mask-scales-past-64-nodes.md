# 0016. The post-compile mute mask scales past 64 compiled nodes

- **Status:** Accepted
- **Date:** 2026-06-26
- **Deciders:** Dan (owner), build agent (H3 review/fix)
- **Related:** ADR-0014 (mixer policy: mute / SIP-solo / solo-safe / Sidechain — pins the *post-compile
  mute mask, no graph rewrite on a solo toggle* mechanism this ADR implements at scale), ADR-0007
  (CompiledGraph + deterministic compile / bit-identical recompiles), ADR-0002 (#1 audio thread never
  allocates/locks/waits), ADR-0008 (frozen `Node` contract — unchanged here). Surfaced by the H3
  adversarial review (`yesdaw-h3-complete-review.md`, finding **mute-policy-1**).

## Context

ADR-0014 decided that mute / SIP-solo / solo-safe are published to the audio thread as a **post-compile
mute mask** — toggling solo never rewrites routing edges or recompiles; the audio thread only reads an
already-published atomic. The first implementation stored that mask as a **single `std::atomic<uint64_t>`**
in `CompiledGraph`, and `GraphBuilder` assigned each compiled node a mute bit by its **compiled-node
index**: `cn.muteBit = compiledIdx < 64u ? compiledIdx : kNoMuteBit`.

That ties the number of mutable nodes to **64 compiled nodes total**. The mixer projection emits ~4 nodes
per track (source → Fader → Pan → Meter), so a project crosses 64 compiled nodes at **~16 tracks**. Past
that, a track's source node gets `kNoMuteBit`, `isMuteCapable` returns false, and because
`applyMixerMutePolicy` is **all-or-nothing** (one incapable target aborts the whole publish), exceeding the
ceiling **silently disables mute/solo for the entire project**. For a Logic/Pro-Tools/Cubase/Sonar-class
DAW (ADR-0003), 16 tracks is nothing — this is a hard, silent scaling cliff that must be removed before the
mixer projection is wired to a runtime.

The forces: the audio thread reads the mute state per Block with **no allocation/lock/syscall** (ADR-0002);
toggling mute/solo must stay a **mask publish, not a recompile** (ADR-0014); the bit assignment must stay
**deterministic** so recompiles are bit-identical (ADR-0007); the fix must be **mechanically provable**
(the owner does not read code); and it should **not disturb** the near-frozen `Node`/`CompiledGraph`
contracts (ADR-0008) more than necessary.

## Options considered

1. **Wider *fixed* multi-word mask** (`std::array<std::atomic<uint64_t>, K>`, `muteBit = compiledIdx`).
   - Pros: trivial; audio read pattern unchanged; bit-identical.
   - Cons: still a fixed ceiling at `K·64` — it re-introduces *this exact silent-failure bug* at a higher
     number. A stopgap, not an ADR-grade answer. Rejected.
2. **Per-node `std::atomic<bool> muted` flag on `CompiledNode`** (drop the central mask).
   - Pros: unbounded; cleanest read model.
   - Cons: **breaks `CompiledNode` trivial-copyability** (verified: `static_assert` at `CompiledGraph.h`,
     and the `std::vector<CompiledNode>` build/relocation in `GraphBuilder`), the single biggest contract
     disruption. Rejected.
3. **Compact bitset, bits only for mute-*eligible* nodes** (a dense eligible-only counter).
   - Pros: unbounded; ~75% less mask memory.
   - Cons: introduces a **second** deterministic ordering (the eligible counter) that becomes its own
     golden-test surface — extra invariant to defend for a memory saving that is irrelevant (≈0.5 KB at
     1000 tracks). Rejected as not worth the risk.
4. **Per-node `muteBit` index (widened to `uint32_t`) into a compile-time-sized atomic *word array*:**
   `std::vector<std::atomic<uint64_t>> muteWords_`, sized `ceil(numCompiledNodes / 64)` at compile.
   `muteBit` stays `= compiledIdx`. **Accepted.**

## Decision

**Adopt option 4.** The mute mask becomes **as many 64-bit atomic words as the compiled graph needs**,
allocated once on the control thread when the `CompiledGraph` is constructed and never resized on the audio
thread. Each compiled node keeps `muteBit = compiledIdx` (widened to `uint32_t`); the audio thread reads
word `muteBit >> 6`, bit `muteBit & 63` with a single relaxed atomic load — the same branch-only pattern as
before, just indexed. The decided sub-points:

- **DP-1 — one bit per compiled node (not eligible-only).** Keep `muteBit = compiledIdx`: the bit is the
  *same* deterministic quantity ADR-0007 already guarantees, so no new ordering invariant exists to defend.
  The memory cost (≈0.5 KB at 1000 tracks) is negligible against audio buffers.
- **DP-2 — growable `std::vector<std::atomic<uint64_t>>`, sized once at compile.** Genuinely unbounded; the
  audio path only ever *loads* from it. No fixed inline cap (a cap is the very bug being removed).
- **DP-3 — there is no longer a *capacity* failure path.** With unbounded `muteBit`, `isMuteCapable`
  reduces to "this NodeId resolves to a real compiled node," so the only "not mute-capable" case is an
  **unknown/missing target id** — a projection bug, not a capacity limit. Keep `applyMixerMutePolicy`'s
  all-or-nothing pre-check (it is correct), and keep `kNoMuteBit` solely as the "no such node" sentinel. If
  a hard upper bound is ever wanted, it must surface as a **compile-time** `GraphBuildError` (loud,
  mechanical), never as a runtime mute no-op.
- **DP-4 — `muteBit` is `uint32_t`.** `uint16_t` (65 536 nodes) is still a real ceiling for thousands of
  plugin-heavy tracks; `uint32_t` is unbounded against the graph's other slot limits. A build-time
  `static_assert(sizeof(CompiledNode) <= 64)` keeps the cache-small contract, and
  `static_assert(std::is_trivially_copyable_v<CompiledNode>)` proves option 2's hit was *not* taken.

`MixerMutePolicy`, the `Node` contract (ADR-0008), and `ProcessArgs` are **unchanged**. The mask is
runtime-derived from mixer state every session and never serialized, so there is **no persistence
migration**.

**Mechanical proof (ADR-0005 — owner reads PASS/FAIL only):** a Catch2 test on the pure-C++ RTSan/TSan leg
projects **200 tracks + 50 bus returns** (≫ 64 and ≫ 256 compiled nodes) and asserts: every track-source
and bus-Return target is `isMuteCapable` (this `REQUIRE` fails on the pre-fix build at ~the 17th target);
`applyMixerMutePolicy` succeeds (no all-or-nothing abort); muting a high-index track zeroes exactly its
contribution in the master (a bit *beyond word 0* gates audio) while a low-index track still passes;
`muteBit == compiledIdx` for every compiled node (deterministic assignment); plus the two `static_assert`s.

## Consequences

- **Positive:** mute/solo scales to effectively-unbounded track/bus counts; the audio-thread read stays
  lock-free and branch-only; recompiles stay bit-identical (same `compiledIdx`); `CompiledNode` stays
  trivially-copyable; `MixerMutePolicy` and the `Node` contract are untouched; the silent capacity-failure
  path is *removed*, not merely raised.
- **Negative / accepted costs:** the mask is now a small heap allocation (one `vector` of atomic words,
  sized at compile on the control thread) rather than an inline scalar; `muteBit` widens `uint8_t`→
  `uint32_t` (absorbed into existing struct slack, guarded by the `sizeof <= 64` assert).
- **Follow-ups:** when the mixer projection gains a runtime caller, the all-or-nothing apply's only failure
  mode is an unknown target id; surface that as a loud projection error, never a silent no-op.
