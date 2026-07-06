# Current horizon - H15 (Automation) - CLOSED

> This file is the oracle for "is the horizon done?". H15 opens after H14 closed remote-green on
> `main`: CP10 implementation `5cf3574` passed GitHub Actions run `28729589346`, and H14 closeout
> docs `a886711` passed run `28729985374`, both across Linux, Windows, macOS, RTSan, and TSan.

## Exit criterion (the finish line)

Automation lanes are Project data, persisted and undoable, compiled into the Snapshot, evaluated
sample-accurately on the audio thread, and delivered to fader, pan, send, and H14 FX parameters without
violating the real-time contract. The lane editor UI waits for H16; H15 is headless read-mode automation.

The H15 focused gates are:

- **`YesDawAutomationCheck`**: first, characterize the existing ADR-0009 evaluator behavior, including
  Linear/Hold block slicing, cursor reuse, locate/re-seek, hostile inputs, and quarantined Bezier/Log cases.
- **Persistence v8 gates**: automation lane and breakpoint rows round-trip through save/reopen; validators
  reject orphan targets, unknown roles, duplicate ticks, out-of-range values, and quarantined curve types.
- **Undo property gates**: randomized Project edit sequences include automation verbs and fully undo/redo
  to bit-identical Project values.
- **Consumer gates**: FaderNode migrates to ParamSpec-normalized dB mapping, PanNode consumes events, send
  levels get a real FaderNode target, and each H14 FX node proves one lane reaches it.
- **Runtime gates**: compiled frame-domain lanes emit through the `ProcessArgs::automationEvents` side-band,
  root-slot injection is rejected by a downstream event-producing negative control, compiled lanes force
  `blockParallelSafe = false`, event budgets reject impossible projects, and locate/loop/tempo/block-size
  cases stay mechanically green.
- **Integration closeout gates**: offline Render == RT for an automated full mix, precedence favors lane
  events over manual scalar posts, scheduler refusal is proven by a zero-latency fader-only automated graph,
  RTSan/TSan stay green, and roadmap/STATUS closeout plus adversarial review are recorded.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R YesDawAutomationCheck --output-on-failure
ctest --test-dir build-ci -R YesDawFxAutomationCheck --output-on-failure
ctest --test-dir build-ci -R YesDawBuilderCheck --output-on-failure
ctest --test-dir build-ci -R YesDawMixerProjectionCheck --output-on-failure
ctest --test-dir build-ci -R YesDawFaderCheck --output-on-failure
ctest --test-dir build-ci -R YesDawPanCheck --output-on-failure
ctest --test-dir build-ci -R YesDawSchedulerCheck --output-on-failure
ctest --test-dir build-ci -R YesDawPlaybackCheck --output-on-failure
```

As new H15 gates land, update this command list in the same checkpoint.

## Status: CLOSED

H15 CP0, CP1, CP2, CP3, and CP4 implementation gates are closed remote-green on `main` through the
Limiter FX-param RT/offline parity slice. The verified CP4 closeout surface includes scheduler refusal,
fader/pan RT/offline parity, send-ride RT/offline parity, and one FX-param RT/offline parity case for
EQ, compressor, delay, reverb, and limiter. Final adversarial closeout review is recorded in
`docs/reviews/2026-07-06-h15-closeout-adversarial-review.md` and found no H15 closeout-blocking defect.
Do not start H16 UI work from this horizon.

## The plan

Full build order:
[`docs/plans/2026-07-03-h15-automation-plan.md`](../docs/plans/2026-07-03-h15-automation-plan.md).
