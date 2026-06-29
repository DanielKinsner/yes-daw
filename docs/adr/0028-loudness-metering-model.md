# 0028. Loudness metering model

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0002 (real-time foundations), ADR-0005 (mechanical verification), ADR-0020
  (H7-H11 roadmap), H10 mixing/mastering and interchange plan, `CONTEXT.md` (Loudness meter).

## Context

H10 needs a real loudness meter before the UI exposes mastering-oriented decisions. This is architectural
because loudness is a standards-facing measurement, not a display preference: once Projects, exports, and
future mastering features report LUFS, the numbers need to be stable, reproducible, and mechanically
verifiable.

The hard constraints are:

- The gate must be objective: no listening and no eyeballing meter movement.
- The audio thread must never run the loudness analysis or call third-party code that may allocate, lock,
  or branch through heavyweight state.
- The result must match the accepted BS.1770 / EBU R128 behavior closely enough that later UI and export
  features can rely on it.
- YES DAW does not yet have a general channel-layout model beyond the current mono/stereo Project surface.

## Options considered

1. **Use `libebur128` as the canonical H10 loudness engine and wrap it behind a YES DAW API.**
   - Pros: mature BS.1770 / EBU R128 implementation; small dependency; gives CI an external reference;
     keeps the domain policy in one headless wrapper.
   - Cons: adds a C dependency and dependency-pinning work; the wrapper gate must still prove channel
     mapping and input validation, or it can become a pass-through smoke test.
2. **Implement BS.1770 in-house and compare against `libebur128`.**
   - Pros: no runtime third-party loudness dependency; full control over allocations and data layout.
   - Cons: more bug surface in a standards algorithm; slower path to H10; higher chance of subtle LUFS
     drift before the UI exists.
3. **Ship an RMS/peak meter now and defer LUFS.**
   - Pros: simplest implementation.
   - Cons: fails H10's mastering requirement; RMS is not an interchange/mastering loudness answer.
     Rejected.

## Decision

H10 uses **`libebur128` as the canonical loudness implementation and reference**, pinned deliberately
through CMake. The first target is `v1.2.6`, verified from the upstream tag list before this ADR landed.

The YES DAW surface is a headless **Loudness meter** wrapper:

- Input is interleaved `float` samples plus sample rate and channel count.
- H10 supports mono and stereo only. Wider layouts return an explicit unsupported-layout result until a
  channel-layout ADR exists.
- Non-finite samples are rejected before reaching the library.
- The wrapper reports integrated loudness, short-term loudness, momentary loudness, loudness range, and
  peak/true-peak data exposed by the pinned library.
- The wrapper is **control/offline only**. It is not a Node, and the audio thread never calls it. A future
  live UI meter may feed it from non-audio-thread snapshots or a bounded analysis worker, but that worker
  boundary is outside this ADR.

`YesDawLoudnessCheck` is the mechanical gate:

- Compare wrapper results against direct `libebur128` reference calls on deterministic mono and stereo
  fixtures.
- Include biting controls for wrong channel weighting/order, silence, clipped/peak-heavy input, short
  buffers, chunked feeding vs whole-buffer feeding, and non-finite input rejection.
- Use tight but standards-tolerant thresholds: LU-family values within `0.1 LU`, and peak-family values
  within `1e-4 dB` unless the dependency's documented precision forces a narrower later update.

## Consequences

- **Positive:** H10 gets standards-aligned loudness numbers behind a mechanical CI gate, with a clean API
  the H11 UI can display later.
- **Positive:** dependency drift is explicit: a future `libebur128` bump is a code review decision plus a
  golden/reference update, not an accidental transitive change.
- **Negative / accepted costs:** mono/stereo-only is intentionally narrow; surround/immersive layouts need
  a later channel-layout ADR.
- **Negative / accepted costs:** the initial gate compares a wrapper to direct library calls, so it must
  include controls that prove the wrapper validates input and maps channels correctly.
- **Follow-ups:** add the pinned dependency, implement the wrapper under a headless analysis namespace, add
  `YesDawLoudnessCheck`, and keep any live UI meter/worker design out of H10's first slice.
