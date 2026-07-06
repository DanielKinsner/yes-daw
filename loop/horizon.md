# Current horizon - H16 (Real UI) - OPEN

> This file is the oracle for "is the horizon done?". H16 opens from Dan's explicit 2026-07-06
> H16 chained-thread instruction after H15 closed remote-green on `main`.
> H15 final closeout commit `f1b093abe2f0e4f70b1266c88b61c168f98b1a10` passed GitHub Actions run
> `28769456779` across Linux, Windows, macOS, RTSan, and TSan.

## Exit criterion (the finish line)

The UI reaches structural parity with the product mockup: ruler section markers, waveform clips,
clip/track inspector, mixer sends view, FX slots, automation lanes, async waveform peak cache,
LookAndFeel/design-token system, and one batched polish pass. The mockup-inventory checklist is
mechanically covered by the UI input harness; the async-cache gate proves the UI thread never decodes;
the token-audit gate proves no raw colors outside the theme; the real-GPU windowed frame smoke records
a PASS in the reality lane; accessibility and all H11-H13 UI gates stay green.

The H16 focused gates are:

- **`YesDawThemeAuditCheck`**: source-scan `src/ui/` so colors/spacing flow through `UiTheme.h`; its
  scratch negative control must fail on an inline raw color.
- **`YesDawWaveformCacheCheck`**: async peak building is off the UI thread, persists under `peaks/*.ypeaks`,
  reloads, and fails if paint performs a synchronous build.
- **Waveform column gate**: rendered min/max columns match `WaveformPeakCache` fixture values at each zoom tier.
- **Mockup-inventory UI harness gates**: each new affordance works through real input/action paths and
  asserts Project or engine state, not only widget state.
- **Agent-native parity**: every new UI affordance has an action ID, key/accessibility coverage where
  applicable, and no click-only path.
- **Real-GPU frame smoke**: `tools/ui-frame-smoke.ps1` records owner-machine PASS/FAIL in
  `docs/reality-lane.md`; the headless frame gate remains the CI proxy.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci --output-on-failure
ctest --test-dir build-ci -R YesDawUiActionCheck --output-on-failure
ctest --test-dir build-ci -R YesDawUiInputCheck --output-on-failure
ctest --test-dir build-ci -R YesDawTimelineGpuCheck --output-on-failure
```

As new H16 gates land, update this command list in the same checkpoint. The first pending additions are
`YesDawThemeAuditCheck` and `YesDawWaveformCacheCheck`. Until a new gate exists, run the focused gates
that exist for the touched files plus `git diff --check` and the full `ci` preset.

## Status: OPEN

H16 kickoff is the first docs-only checkpoint. It opens the horizon after verifying H15 closeout commit
`f1b093abe2f0e4f70b1266c88b61c168f98b1a10` and GitHub Actions run `28769456779`. No production H16 code
has landed yet.

Next checkpoint: CP1 design tokens. Add the smallest independently green `UiTheme.h` token surface and
biting `YesDawThemeAuditCheck` scan before broad UI migration.

## The plan

Full build order:
[`docs/plans/2026-07-03-h16-real-ui-plan.md`](../docs/plans/2026-07-03-h16-real-ui-plan.md).
