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
ctest --test-dir build-ci -R YesDawThemeAuditCheck --output-on-failure
ctest --test-dir build-ci -R YesDawUiInputCheck --output-on-failure
ctest --test-dir build-ci -R YesDawTimelineGpuCheck --output-on-failure
```

As new H16 gates land, update this command list in the same checkpoint. The first pending addition is
`YesDawWaveformCacheCheck`. Until a new gate exists, run the focused gates
that exist for the touched files plus `git diff --check` and the full `ci` preset.

## Status: OPEN

H16 CP1 is partially underway. The first design-token checkpoint opened `src/ui/UiTheme.h`, moved
Timeline/MainComponent raw UI colors behind named tokens, and added `YesDawThemeAuditCheck` with a raw
color negative control. Follow-up CP1 slices moved Timeline canvas type/radius/spacing values,
MainComponent typography values, MainComponent rounded radii, and MainComponent shell layout dimensions
behind `UiTheme` tokens; the theme audit now also rejects raw `FontOptions` sizes, raw rounded radii, and
raw `constexpr` UI width/height tokens outside `UiTheme.h` with scratch negative controls. This
checkpoint moved MainComponent meter fill colors and hot-band split fractions behind `UiTheme::Meter`;
the theme audit now rejects raw meter split fractions with a scratch negative control. This checkpoint
moved MainComponent top-level shell panel inset spacing behind `UiTheme::Layout`; the theme audit now
rejects raw shell `.reduced(x, y)` spacing values with a scratch negative control. This checkpoint moved
MainComponent inspector control internal spacing behind `UiTheme::Layout`; the theme audit now rejects raw
`layoutInspectorControls` spacing values with a scratch negative control. This checkpoint moved MainComponent
mixer control internal spacing behind `UiTheme::Layout`; the theme audit now rejects raw `layoutMixerControls`
spacing values with a scratch negative control. This checkpoint moved MainComponent painted track-list spacing
behind `UiTheme::Layout`; the theme audit now rejects raw `drawTrackList` spacing values with a scratch
negative control. This checkpoint moved MainComponent shared meter fill inset spacing behind
`UiTheme::Layout`; the theme audit now rejects raw `drawMeter` inset spacing values with a scratch negative
control. This checkpoint moved MainComponent painted header, transport-readout, and master-meter geometry
behind `UiTheme::Layout`; the theme audit now rejects raw `drawHeader` geometry values with a scratch
negative control. This checkpoint moved MainComponent painted piano-roll canvas, note, key-row, grid-line,
and expression-lane geometry behind `UiTheme::Layout`; the theme audit now rejects raw `drawPianoRoll`
geometry values with a scratch negative control. This checkpoint moved MainComponent painted inspector-panel
tab, title, stats, gain, fades, and clip-FX geometry behind `UiTheme::Layout`; the theme audit now rejects
raw `drawInspector` geometry values with a scratch negative control. This checkpoint moved MainComponent
painted mixer-panel tool, strip, pan-knob, button, sidechain, meter, rail, and fader thumb geometry behind
`UiTheme::Layout`; the theme audit now rejects raw `drawMixer` geometry values with a scratch negative
control. This checkpoint moved MainComponent `resized()` toolbar and autosave button geometry behind
`UiTheme::Layout`; the theme audit now rejects raw `setBounds` button geometry values in `resized()` with
a scratch negative control. This checkpoint moved MainComponent timeline clip and piano-roll note edge-hit
geometry behind `UiTheme::Layout`; the theme audit now rejects raw `*EdgePixels` local constants with a
scratch negative control. This checkpoint moved MainComponent timeline viewport pixel-width/gutter geometry
behind `UiTheme::Layout`; the theme audit now rejects raw `makeTimelineState()` viewport geometry with a
scratch negative control. This checkpoint moved MainComponent timeline and piano-roll input drag dead-zone
geometry behind `UiTheme::Layout`; the theme audit now rejects raw input drag threshold geometry with a
scratch negative control. This checkpoint moved MainComponent piano-roll expression point and curve-stroke
geometry behind `UiTheme::Layout`; the theme audit now rejects raw expression `fillEllipse` and
`PathStrokeType` geometry with a scratch negative control. This checkpoint moved MainComponent default
window-size geometry behind `UiTheme::Layout`; the theme audit now rejects raw `setSize` window geometry
with a scratch negative control. This checkpoint moved MainComponent shared panel-outline inset and
stroke-width geometry behind `UiTheme::Layout`; the theme audit now rejects raw `fillPanel` panel-chrome
geometry in `MainComponent.cpp` with a scratch negative control. This checkpoint moved MainComponent
piano-roll key-range and grid-cadence geometry behind `UiTheme::Layout`; the theme audit now rejects raw
piano-roll key-range and grid-cadence geometry with scratch negative controls. This checkpoint moved
MainComponent timeline clip gain-drag gesture geometry behind `UiTheme::Layout`; the theme audit now
rejects raw gain-drag gesture constants with scratch negative controls. This checkpoint moved MainComponent
hidden slider text-box geometry behind `UiTheme::Layout`; the theme audit now rejects raw
`setTextBoxStyle(..., 0, 0)` geometry with a scratch negative control.

Next checkpoint: Continue CP1 design tokens. Migrate the next narrow UI surface from legacy local tokens
to `UiTheme.h` before broad UI migration, likely another raw MainComponent spacing/geometry surface not
yet covered by the theme audit.

## The plan

Full build order:
[`docs/plans/2026-07-03-h16-real-ui-plan.md`](../docs/plans/2026-07-03-h16-real-ui-plan.md).
