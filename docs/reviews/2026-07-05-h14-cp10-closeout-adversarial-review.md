# H14 CP10 / H14 closeout adversarial review

Date: 2026-07-05

Scope: close out H14 after CP10. This pass re-checked the equal-power crossfade implementation,
the H14 plan boundary, the live handoff, and the required GitHub Actions runs. It did not inspect
or change H15 implementation code.

## Verdict

No H14 closeout-blocking defect proven.

H14 can close remote-green. The next legitimate implementation chunk is H15's first checkpoint:
the plan labels it **CP0 - Evaluator characterization gate**. If a baton calls that "H15 CP1",
it still must not skip the audit-first evaluator characterization work.

## Evidence

- Local `git pull --ff-only` was already up to date.
- Local `HEAD`, `main`, and `origin/main` all resolve to `a886711dee4ad7b4ca5b3cde403ac8a95ba42af3`.
- CP10 implementation commit `5cf357411f4fdb741956b5dfe6b1266c1c901f7b` changed the shared Clip fade path
  and the render/project/playback/bundle tests, with no FX UI, automation, plugin-hosting, ADR, reality-lane,
  golden, or RT annotation edits.
- GitHub Actions run `28729589346` for `5cf3574` is completed/successful across Linux, Windows, macOS,
  RTSan, and TSan.
- CP10 closeout docs commit `a886711dee4ad7b4ca5b3cde403ac8a95ba42af3` changes only `STATUS.md`.
- GitHub Actions run `28729985374` for `a886711` is completed/successful across Linux, Windows, macOS,
  RTSan, and TSan.
- Local closeout gates passed: `git diff --check`; focused CP10 CTest lane
  `ctest --preset ci -R "Equal-power clip crossfade|Clip gain envelope|Adjacent Clip envelopes|RT path and offline Render path match|bundled Asset bytes decode|split Clip with crossfade metadata|YesDawOfflineRenderCheck|YesDawPlaybackCheck" --output-on-failure`
  passed 9/9.
- `docs/plans/2026-07-03-h14-fx-suite-plan.md` stops H14 implementation at CP10 and schedules only roadmap,
  status, and `docs/reviews/` closeout follow-up after the equal-power crossfade gate.
- `src/engine/ClipEnvelope.h` now evaluates fade-in with `sin((pi/2)*t/T)` and fade-out through the same
  helper with reversed progress.
- `src/engine/nodes/DecodedClipNode.h` applies `evaluateClipFadeEnvelopeGain(...)` in the realtime source
  node; offline render constructs the same node for clip playback, so offline and realtime share the path.
- `YesDawRenderCheck` contains the CP10 crossfade gate: offline/realtime equality, equal-power energy within
  the +/-0.1 dB tolerance, and an old-linear-law negative control.
- `YesDawProjectCheck`, `YesDawPlaybackCheck`, `YesDawBundleRenderCheck`, and `YesDawOfflineRenderCheck`
  carry independent equal-power references or the shared envelope gate for the affected surfaces.

## Findings

None.

One non-blocking cleanup note: `src/engine/OfflineRenderer.h` still has a stale explanatory comment that says
`DecodedClipNode` uses a linear fade. The code now calls the equal-power shared path, and the gates prove the
behavior. This review did not touch runtime files because the current closeout boundary is docs-only unless a
real defect is proven.

## Closeout decision

Close H14 as remote-green on `a886711` / run `28729985374`. Point the handoff at H15's first checkpoint,
the evaluator characterization gate, and keep H15 implementation out of this closeout commit.
