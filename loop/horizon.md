# Current horizon — H7 (Offline render / export to file) — READY FOR CLAUDE REVIEW

> This file is the oracle for "is the horizon done?". H7 closes iff the exit gate below is green.

## Exit criterion (the finish line)

A Project can be bounced to an audio file, and it is mechanically proven that the file holds exactly what
the engine renders: the offline render equals an **independent** reference render within tolerance (not
the engine compared to itself), the canonical 32-bit-float WAV round-trips bit-exactly, and the exported
file re-imports to an Asset whose decoded samples match the render.

**`YesDawOfflineRenderCheck`** proves this with an in-repo deterministic harness:

- Renders a known Project offline to a float buffer over the full timeline (length from Clips/markers;
  PDC/effect tail flushed) via a real `OfflineRenderer` built on `ProjectMixerProjection` +
  `CompiledGraph::process`.
- Asserts that buffer equals an independent reference (Clips summed at their timeline positions with
  gain/fades) within tolerance, with a negative control that a renderer mutation fails the compare.
- Writes and reads a canonical float32 WAV and asserts a bit-exact round-trip, with a negative control
  that a writer mutation fails it.
- Exports the render to a `.wav`, re-imports it as an Asset through the normal bundle import path, decodes
  it, and asserts the decoded samples match the render.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --test-dir build-ci -R "YesDawOfflineRenderCheck" --output-on-failure
```

## Status: **IMPLEMENTED (local gate green 2026-06-28; awaiting Claude review before H8)**

H7 opened at the H6->H7 boundary after the H6 reliability gate was adversarially reviewed, hardened, and
closed (full ci 237/237, remote CI green). ADR-0020 carves the post-H6 work into horizons H7–H11
(feature-first, UI as the H11 capstone). H7's implementation is now in place: ADR-0021 locks the
canonical float32-WAV export format, `src/io/WavFile.h` provides the pure reader/writer,
`src/engine/OfflineRenderer.h` renders the current Project mixer surface to interleaved Master-bus
samples, and `YesDawOfflineRenderCheck` proves render/reference, WAV round-trip, and export/import.
Local verification: `ctest --test-dir build-ci -R "YesDawOfflineRenderCheck" --output-on-failure` passed
1/1 and `ctest --preset ci --output-on-failure` passed 238/238.

Claude should adversarially review the H7 close-out before H8 opens.

## The plan

Full build order:
[`docs/plans/2026-06-28-h7-offline-render-export-plan.md`](../docs/plans/2026-06-28-h7-offline-render-export-plan.md).
