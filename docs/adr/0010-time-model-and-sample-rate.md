# 0010. Time model — ticks/PPQ, tempo map, per-clip time_base, sample-rate policy

- **Status:** Accepted
- **Date:** 2026-06-23
- **Deciders:** Dan (owner), build agent (H1)
- **Related:** ADR-0002 (engine foundations), ADR-0009 (event stream rides this time base),
  [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) decisions #5/#6/#14,
  [deepening notes](../plans/2026-06-23-yes-daw-deepening-notes.md) → *Time & event model*,
  `CONTEXT.md` (Tick, PPQ, Tempo map, Frame, Block).

## Context

Every other H1 contract references time: clips, events, automation, and the render boundary all need
one canonical way to say *where* a thing is. The choice is irreversible because position values are
written into the schema and into every saved Project — changing the canonical unit later is a
file-format break and a re-quantization of everyone's music. Two of the three resolved forks live
here (PPQ, time_base); the third sub-decision (sample-rate policy) was flagged in the deepening notes
as an unowned irreversible-class gap and is locked now because asset import + schema v1 depend on it.

## Options considered

1. **Canonical position as `int64` ticks at a fixed PPQ; samples derived.**
   - Pros: bit-exact round-trip (no float drift over long projects); musical operations (snap, quantize)
     are exact integer math; tempo-independent storage.
   - Cons: needs a tempo map to reach samples; PPQ must be large enough to never be the limiting grid.
2. **Canonical position as `double` beats / seconds.**
   - Pros: trivially simple; no tempo map needed for seconds.
   - Cons: float accumulates rounding over a long timeline → breaks bit-exact round-trip; equality and
     snap become fuzzy. Rejected.
3. **Rational beats (`int64 num/den`).**
   - Pros: deletes the "is PPQ fine enough" worry entirely.
   - Cons: heavier math everywhere; no real-world need once PPQ is large; the reviewer who raised it
     agreed `frac`/`double` at the render boundary recovers all precision anyway. Rejected for storage.

**Sample-rate sub-decision** — when an imported Asset's rate ≠ the project rate:
keep-original-and-resample-at-read (chosen) vs resample-on-import (bakes a derived copy, fights
non-destructive) vs per-asset native rate (graph sees mixed rates, complicates summing/PDC). Chosen by
the owner: **keep original, resample at read.**

## Decision

**Canonical position is `int64` ticks at `PPQ = 15360`** (resolved 2026-06-23; a large fixed grid so
resolution is never a real limit). Samples are **derived, never stored as authoritative** — if a
derived-sample cache is kept for performance it stores the tempo-map version/hash it was computed
against and recomputes on mismatch.

- **Tempo map** converts ticks↔samples by exact **closed-form per segment** (constant = linear;
  ramp = logarithmic), with a precomputed cumulative-`startSample` prefix sum so lookup is O(log n)
  binary search — never a per-event linear scan. The **meter map is parallel and independent**: it
  drives bar/beat labels and the snap grid only, never `tick↔samples`.
- **Render-time `MusicalTime { int64 tick; double frac; }`** (`frac ∈ [0,1)`) is populated only at
  flatten/render for groove/warp/MPE; sample-accuracy comes from `double` math at the sample boundary,
  not from a finer integer store grid.
- **Per-clip `time_base`** is in the schema from clip #1: `TempoLocked` (follows the tempo map) vs
  `SampleLocked` (fixed sample duration, ignores tempo). Editing tempo must leave every `SampleLocked`
  clip's sample window byte-identical (property test).
- **Sample-rate policy:** a Project has one sample rate. An Asset whose rate differs **keeps its
  original bytes** (content-hashed as imported — honors ADR-0002 "never edit the underlying audio")
  and is **resampled at the read boundary**: a fast tier for live playback/scrub, a high-quality
  windowed-sinc tier for offline Render/Export. A **mid-project SR change is safe** because ticks are
  canonical and positions are tempo-derived, not sample-stored.

## Consequences

- **Positive:** bit-exact round-trip of musical position; snap/quantize are exact; tempo and SR edits
  never corrupt stored positions; the resampler choice stays fully non-destructive and keeps bundles
  small.
- **Negative / accepted costs:** every sample position goes through the tempo map (mitigated by the
  prefix-sum O(log n) lookup); a read-time resampler must exist before any cross-rate Render is correct;
  two resampler quality tiers to build and test.
- **Follow-ups:** CONTEXT.md gains **Tick**, **PPQ**, **time_base**. H1 exit gate covers tempo/meter-map
  round-trip; a dedicated SR gate (a 44.1k Asset in a 48k Project renders within golden tolerance of an
  offline reference) lands with asset import. Property test: edit tempo → all `SampleLocked` windows
  byte-identical.
