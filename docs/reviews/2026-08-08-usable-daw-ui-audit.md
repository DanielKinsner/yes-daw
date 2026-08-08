# YES DAW usable-product UI audit — 2026-08-08

## Scope and evidence

This audit compares the three fresh 1536x960 screenshots produced by
`tools/ui-screenshot.ps1` at `1ad0f10` with the accepted structural and visual reference in
`docs/design/arrangement-view-reference.png`. It also checks the current H16 plan, `UiTheme`,
`YesDawLookAndFeel`, `MainComponent`, and the screenshot/input/accessibility gates.

This document introduces no new product direction. Remediation must preserve the accepted native
JUCE architecture and action registry, reuse the existing vector assets and controls, and converge
on the existing reference rather than inventing a new look.

## Anti-pattern verdict

**AI-slop test: PASS. Premium-product test: FAIL.** The shell avoids excessive glass cards, glow,
generic hero layouts, and decorative animation; its restrained depth gradients fit the accepted
reference. Its problem is different: it still reads as an engineering validation surface. Repeated
text buttons, debug-state labels, under-resolved control hierarchy, and large inactive regions make
the product feel unfinished despite the underlying functionality.

## Executive summary

- **High:** 5 findings
- **Medium:** 3 findings
- **Critical:** 0 — the mechanical input, accessibility, screenshot, and frame gates are present
- **Overall visual readiness:** 5/10 against the accepted reference

The highest-leverage sequence is: repair mixer proportions and master-strip hierarchy; consolidate
project/transport chrome; deepen track-header controls; then rebuild inspector hierarchy and empty
states. Each slice must extend the screenshot gate with geometry or pixel-region assertions so a
future nonblank image cannot silently regress back to placeholder quality.

## High-severity findings

### H1 — Mixer geometry is dominated by unused vertical space

- **Location:** `src/ui/UiTheme.h:422`, `src/ui/MainComponent.cpp:3250`
- **Category:** Responsive design / interaction
- **Evidence:** The mixer-only capture stretches narrow rails and meters through most of a 960 px
  window while controls are clustered in the top 120 px. The accepted reference uses the same area
  for a readable channel identity, pan, routing state, fader scale, meter, numeric readout, and a
  distinct master strip.
- **Impact:** Channel state cannot be scanned quickly and the primary mix surface feels empty rather
  than focused.
- **Recommendation:** Define a bounded mixer control stack with reference-like proportions, add the
  master strip as a visually distinct terminal channel, and use remaining height for meaningful
  fader travel and scale rather than empty rail.

### H2 — Project commands and transport compete as one undifferentiated toolbar

- **Location:** `src/ui/UiTheme.h:319`, `src/ui/MainComponent.cpp:2447`
- **Category:** Interaction / visual hierarchy
- **Evidence:** New/open/save/import/export/undo/redo sit beside the dominant transport with similar
  small-button treatment. The reference separates menu/project work from large transport controls
  and reserves the strongest accent for Record.
- **Impact:** The most frequent performance actions do not have instant visual priority, and project
  setup reads like developer chrome.
- **Recommendation:** Keep existing commands and vector assets, but group project lifecycle actions
  into a restrained utility cluster, preserve large transport targets, and make Record the sole
  high-salience destructive action.

### H3 — Track headers do not expose the reference's usable channel-strip summary

- **Location:** `src/ui/MainComponent.cpp:2567`
- **Category:** Interaction / responsive design
- **Evidence:** Current rows show identity, M/S/record and a meter, but pan and fader state are reduced
  to small decorative marks. The reference exposes an immediately readable pan knob, level control,
  meter, and channel identity on every row.
- **Impact:** Arrangement editing requires switching context to understand or adjust basic mix state.
- **Recommendation:** Recompose each track row around identity plus compact real pan/fader controls,
  retain the existing action-backed M/S/arm behavior, and mechanically assert disjoint hit targets.

### H4 — Inspector hierarchy is too text-heavy and under-informative

- **Location:** `src/ui/UiTheme.h:349`, `src/ui/MainComponent.cpp:3068`
- **Category:** Interaction / accessibility
- **Evidence:** Timing values and fades are visually tiny, gain is a generic horizontal slider, FX
  rows have no bypass/reorder affordance treatment, and automation is a miniature line without a
  strong target/value hierarchy. The reference uses clear section rhythm, a gain control plus meter,
  fade-shape previews, and explicit FX states.
- **Impact:** Precise clip work is harder to scan and the most consequential values lack confidence.
- **Recommendation:** Preserve current action-backed controls while improving section spacing,
  numeric legibility, gain/meter pairing, fade previews, and FX state affordances.

### H5 — Debug-state copy is visible as primary mixer UI

- **Location:** `src/ui/MainComponent.cpp:1618`
- **Category:** UX writing / interaction
- **Evidence:** Labels such as `Meters: no project`, `Sends: no project`, `FX: no project`, `GR: no
  project`, and `Bus FX: no project` occupy the mixer utility rail in every screenshot fixture.
- **Impact:** The shipped surface reads like a test harness and gives no useful next action.
- **Recommendation:** Replace debug-state sentences with compact section labels and an instructive,
  action-backed empty state only when no Project is loaded.

## Medium-severity findings

### M1 — Type hierarchy is too compressed at the default capture size

- **Location:** `src/ui/UiTheme.h:190`, `src/ui/YesDawLookAndFeel.h:95`
- **Category:** Accessibility / theming
- **Impact:** Secondary labels, numeric readouts, and inspector values require unnecessary visual
  effort even at 1536x960.
- **Recommendation:** Keep the platform-safe font fallback, but raise the smallest shipped text and
  create a clearer distinction between labels, values, channel names, and transport time.

### M2 — Control styling repeats the same rectangular treatment too broadly

- **Location:** `src/ui/YesDawLookAndFeel.h:47`
- **Category:** Theming / interaction
- **Impact:** Different actions with different risk and frequency look interchangeable.
- **Recommendation:** Preserve the machined theme but define quiet utility, standard toggle, selected
  segment, transport, and destructive-record variants using existing semantic tokens.

### M3 — Screenshot coverage proves presence, not reference-grade composition

- **Location:** `tests/ui_screenshot_tests.cpp:228`
- **Category:** Quality gate
- **Impact:** A nonblank, collision-free surface can still regress to a visually sparse engineering
  layout.
- **Recommendation:** Add reference-derived geometry assertions for mixer stack proportions, master
  strip presence, track-header control regions, header grouping, and inspector section coverage.

## Systemic pattern

The implementation has strong functional seams but its visual gates stop one level too early. The
same action registry, controls, tokens, and screenshot harness can support the accepted design; the
remaining work is composition and hierarchy, not a framework rewrite.

## Positive findings to preserve

- Real Project actions and input paths are already component-backed and mechanically exercised.
- The central token system and platform-safe font fallback prevent raw-style drift.
- Timeline waveforms, section markers, automation, mixer data, and piano-roll surfaces are real.
- Screenshots are deterministic at the exact reference resolution.
- Accessibility, frame-time, input, collision, blank-surface, and cross-platform CI gates already
  give remediation a safe feedback loop.

## Remediation priority

1. **Immediate:** H1 + H5 — mixer composition, master hierarchy, and useful utility/empty states.
2. **Immediate:** H2 — project/transport grouping and action hierarchy.
3. **Short term:** H3 — compact arrangement-track mix controls.
4. **Short term:** H4 + M1 — inspector structure and readable numeric hierarchy.
5. **Then:** M2 + M3 — semantic control variants and stronger screenshot composition gates.
