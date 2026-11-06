# 0030. Time-stretch Node

- **Status:** Accepted
- **Date:** 2026-06-29
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0002 (real-time foundations), ADR-0005 (mechanical verification), ADR-0008
  (Node contract), ADR-0020 (H7-H11 roadmap), ADR-0027 (scheduler block-parallel safety guard),
  H10 mixing/mastering and interchange plan, `CONTEXT.md` (Time-stretch Node).

## Context

H10 needs a real time-stretch path before the H11 UI exposes clip stretch controls. The research already
rejected writing a stretcher from scratch and recommends an embeddable library. This decision is
architectural because time-stretching touches the Node contract, render determinism, latency accounting,
and scheduler safety.

The hard constraints are:

- The gate must be mechanical: no listening and no subjective quality judgement.
- The audio thread must never allocate, lock, log, or call third-party code that may allocate internally.
- The current engine already has source Nodes, such as `DecodedClipNode`, that read pre-owned samples by
  absolute timeline frame and can be block-parallel-safe.
- The block-parallel scheduler must never run a stateful live stretcher out of order by accident.
- H10 is not the warp-marker UI, transient detection, pitch-shift UI, or per-clip automation decision.

## Options considered

1. **Use Signalsmith Stretch to pre-render stretched clip audio on the control side, then expose it through
   a source-style `TimeStretchNode`.**
   - Pros: keeps third-party DSP and allocation out of `process()`; matches the existing source Node
     pattern; makes block-split determinism straightforward; lets H11 rebuild the Node when a clip's
     stretch factor changes.
   - Cons: a stretch-factor change rebuilds prepared audio instead of being a live audio-thread
     parameter; this is a clip/source stretch path, not a generic insert effect.
2. **Run Signalsmith Stretch as a live one-input insert Node.**
   - Pros: closer to a realtime insert effect and future automation.
   - Cons: requires input lookahead, internal variable-rate buffering, and careful latency reporting; it is
     inherently stateful and not block-parallel-safe. This is too much surface for H10's first gate.
3. **Use Rubber Band, SoundTouch, or another external stretcher.**
   - Pros: mature alternatives; some have stronger quality modes or simpler time-domain behavior.
   - Cons: licensing and deployment are less clean for the current product constraints, or quality is too
     narrow for polyphonic material. Rejected for the first H10 implementation.
4. **Implement a small in-house resampler/OLA stretcher.**
   - Pros: no dependency.
   - Cons: risks shipping a weak algorithm and fails the research guidance to wrap a proven engine.
     Rejected.

## Decision

H10 implements **a Signalsmith-backed, offline-prepared `TimeStretchNode`**.

Dependency policy:

- Pin upstream `signalsmith-stretch` at tag `1.1.0` (`44c8f865af9da8c29cc4a70a2d5a3ec83639c711`).
- Treat dependency bumps like golden changes: deliberate review, updated reference evidence, and a new
  green `YesDawTimeStretchCheck`.

Node contract:

- `TimeStretchNode` is a source-style audio Node, not an insert effect in H10.
- It owns stretched float samples prepared on the control side. `process()` only reads those samples by
  absolute timeline frame, applies no third-party DSP, and performs no allocation.
- Stretch factor means `outputFrames / sourceFrames`: `1.5` makes the clip 50% longer; `0.75` makes it
  shorter. H10 supports finite factors in `[0.5, 2.0]`.
- H10 supports mono and stereo. Wider layouts return an explicit unsupported-layout result until the
  channel-layout model broadens.
- The prepared output length is `round(sourceFrames * stretchFactor)`. Start pre-roll and end tail from
  Signalsmith are folded/trimmed during preparation, so the Node reports zero runtime latency to PDC and
  emits exactly the prepared duration.
- `process()` follows the `DecodedClipNode` timeline rule: if `Transport::hasTimelineFrame` is true, output
  is a pure function of that absolute frame; otherwise it advances an internal fallback cursor for simple
  sequential tests.
- Because the audio-thread path is an absolute-frame read over immutable prepared samples, the Node may set
  `blockParallelSafe = true`. A future live/automated stretcher must write a new ADR and default unsafe
  until separately proven.

`YesDawTimeStretchCheck` is the mechanical gate:

- Prepare deterministic mono and stereo fixtures through the pinned Signalsmith wrapper at fixed factors
  including shorter-than-source and longer-than-source cases.
- Verify exact prepared duration, finite samples, and bounded error against checked-in reference/golden
  output for the pinned dependency.
- Render the same `TimeStretchNode` through multiple block splits and absolute timeline offsets; outputs
  must match the prepared reference within tolerance.
- Verify silence before/after the stretched clip window, reset/fallback-cursor behavior, and
  block-parallel-safety metadata.
- Negative controls must bite: non-finite source samples, unsupported channel count, empty source, and
  factors outside `[0.5, 2.0]` must fail preparation.

## Consequences

- **Positive:** H10 gets a real, standards-facing time-stretch feature without putting third-party DSP on
  the audio thread.
- **Positive:** the first UI-facing stretch model is deterministic and compatible with the scheduler's
  block-parallel guard.
- **Positive:** prepared audio can later be cached per clip/asset/stretch factor without changing the
  audio-thread Node shape.
- **Negative / accepted costs:** realtime stretch automation and insert-style stretching are deferred.
- **Negative / accepted costs:** mono/stereo and `[0.5, 2.0]` are intentionally bounded first-slice limits.
- **Follow-ups:** add the pinned dependency, implement the control-side stretcher plus `TimeStretchNode`,
  and land `YesDawTimeStretchCheck`; leave warp markers, transient detection, pitch-shift controls, and
  live stretch automation to later ADRs.
