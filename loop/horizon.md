# Current horizon — H7 (Offline render / export to file) — CLOSED (reviewed + hardened)

> This file is the oracle for "is the horizon done?". H7 closes iff the exit gate below is green.

## Exit criterion (the finish line)

A Project can be bounced to an audio file, and it is mechanically proven that the file holds exactly what
the engine renders: the offline render equals an **independent** reference render within tolerance (not
the engine compared to itself), the canonical 32-bit-float WAV round-trips bit-exactly, and the exported
file re-imports to an Asset whose decoded samples match the render.

**`YesDawOfflineRenderCheck`** proves this with an in-repo deterministic harness:

- Renders a known Project offline to a float buffer over the full timeline (length from Clips/markers) via
  a real `OfflineRenderer` built on `ProjectMixerProjection` + `CompiledGraph::process`, **through the same
  `DecodedClipNode` the realtime path uses** — so the exported render equals what playback produces (linear
  fade; equal-power deferred per H2). The PDC/effect-tail flush exists but is inert until a latency
  node/plugin is reachable (H8+).
- Asserts that buffer equals an **independent** reference (Clips summed at their timeline positions with
  gain, the canonical linear fade, and center pan) within tolerance.
- Proves the render is **block-size independent** (ADR-0008): bit-identical output at block sizes that
  force 9..1 blocks, plus a renderer-input mutation control (the render itself changes when a clip's gain
  changes — not merely a perturbed reference).
- Writes and reads a canonical float32 WAV bit-exactly across the full float range, denormals, a known byte
  layout, and an ancillary-chunk skip; rejects malformed/oversized chunks without over-allocating.
- Exports the render to a `.wav`, stores it as an Asset through the normal bundle import path, decodes it,
  and asserts the decoded samples match the render.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --test-dir build-ci -R "YesDawOfflineRenderCheck" --output-on-failure
```

## Status: **CLOSED — reviewed + hardened (local gate green 2026-06-28)**

H7 opened at the H6->H7 boundary; ADR-0020 carves the post-H6 work into horizons H7–H11 (feature-first,
UI as the H11 capstone). Codex implemented H7 (ADR-0021 float32-WAV format, `src/io/WavFile.h`,
`src/engine/OfflineRenderer.h`, `YesDawOfflineRenderCheck`); Claude then adversarially reviewed it
(5 lenses, 24 confirmed findings, adjudicated by hand) and fixed two blockers + WAV-robustness gaps:
(1) the offline render used a different fade curve than the realtime node (equal-power vs linear —
export != playback) — now rendered through the same `DecodedClipNode`; (2) block-size independence was
unproven (the whole 9-frame timeline fit one 128-frame block) — now swept bit-identically. Known-inert
until H8+: the PDC/tail-flush and marker-extension paths await a latency node. Local verification:
`YesDawOfflineRenderCheck` = 6 cases / 143 assertions; `ctest --preset ci` = 238/238. Checkpoint complete
after the review commits' remote CI is green; then stop for Dan's H7->H8 boundary call.

## The plan

Full build order:
[`docs/plans/2026-06-28-h7-offline-render-export-plan.md`](../docs/plans/2026-06-28-h7-offline-render-export-plan.md).
