# Arrangement-view visual parity backlog — the V-run (2026-08-20)

Carved per Phase 2 Step 1 of `docs/plans/2026-08-20-visual-parity-and-dogfood-execution-plan.md`
("the plan"), against `docs/design/arrangement-view-reference.png` ("the reference"). The audit read
the REAL paint/control code (not the plan's own assumptions — one premise below turned out wrong,
logged at V3) and judged fresh screenshots at the three D7 sizes
(`build-ci/ui-screenshots/yesdaw-timeline-large.png`, `yesdaw-mixer-large.png`) against the
reference. Findings are quoted with `file:line` in each item.

**"Match" means structural/layout/legibility parity judged element-by-element (D2) — not a pixel
clone. No fake data, ever (D3): a reference cell whose backing model doesn't exist ships as an
honest omission, logged in STATUS.md, never invented.**

## How to work this list (non-negotiable — inherited from the plan verbatim)

`docs/goals/2026-08-11-overnight-backlog-run-brief.md` and the 2026-08-12 brief apply verbatim. One
item at a time, strict top-to-bottom order, audit-before-build, shipped-boundary gates that fail
before and pass after, full local ctest with the owner-file isolation ritual, one feature commit +
exact-head nine-job CI green + a separate docs-only evidence commit per item, never edit ADRs /
goldens / `[[clang::nonblocking]]` annotations / `.github/workflows/ci.yml`, never weaken or delete
an existing gate (re-pinning to new semantics per D2 is allowed and expected), stop after 3
consecutive red CI rounds on one item. Every visual item: **D7 judgment loop first** (render the
real shipped shell at 1152×720/1536×960/1920×1080 with `YESDAW_UI_SCREENSHOT_DIR` set, judge against
the reference, iterate until it genuinely reads like the reference) **THEN** lock the mechanical
token/layout gate second — never the other way around.

**CP-B fires after V1, V2, V3 land** (the plan's "first topology+theme wave," at minimum theme +
transport + the D6 dock skeleton): stop, send Dan the three-size screenshots beside the reference,
wait per the plan's C-fallback clause. **CP-C fires when V8 lands**: same shape, full-view
screenshots.

Items are numbered **V1–V8**; tick each here with the commit SHA + run id in the docs-only evidence
commit, per house style.

---

1. [x] **V1 — Theme & typography: verify and close legibility gaps at all three sizes.** DONE —
   feature `5038872`, exact-head nine-job CI run `32427930017` green (first try), local 355/355.
   D7 judgment loop rendered the real shipped shell at all three sizes and compared token-by-token
   against the reference: the near-black palette and single type scale were ALREADY genuinely
   close (`appBackground()` = `0xff070a0d`, `mixerBack()` = `0xff080c10`, one `UiTheme::Type` scale)
   — no token needed changing. This turned out to be the "mostly a judgment pass" outcome the item
   itself predicted, not a rewrite. Locked the finding in mechanically rather than leaving it as an
   unverified visual impression: a new WCAG-style relative-luminance contrast check
   (`maxContrastInRegion`, `tests/ui_screenshot_tests.cpp`) samples real painted text (the header
   time readout, the rail track name) against its own live-rendered panel background at all three
   D7 sizes, requiring ≥3.0:1 contrast — a real floor under the judgment pass, not a rubber stamp.
   `[theme-legibility]` (37 assertions) red before, proven by dropping the header time readout's
   text alpha to 2% — contrast fell to 2.20:1, correctly failing the gate.
   Original spec: Audited
   first: the token layer already matches the reference's aesthetic — `UiTheme::Color` is
   genuinely near-black (`appBackground()` = `0xff070a0d`, `panel()` = `0xff0e1318`, `mixerBack()` =
   `0xff080c10`, `src/ui/UiTheme.h:16-79`), one consistent type scale exists (`UiTheme::Type`, tiny
   9pt → transportClock 27pt, `UiTheme.h:165-207`), and `YesDawThemeAuditCheck`
   (`tests/theme_audit_tests.cpp`) already gates that every UI colour/font constant lives in
   `UiTheme.h` rather than being inlined ad hoc. **That gate is textual (regex-scanned), not
   perceptual — it proves tokens are centralized, not that they read well.** This item is therefore
   a **judgment-first slice**: render the real shell at all three D7 sizes, compare token-by-token
   against the reference (header gradient top `panelRaised()` = `0xff141a20` reads slightly
   lighter/blue-grey than the reference's more uniform near-black header — the first concrete
   candidate), fix any token found genuinely illegible or off-tone, and only then lock a
   pixel-sampling legibility gate (e.g. a minimum contrast ratio between text tokens and their
   panel background, sampled at real painted text locations) alongside the existing textual audit.
   *Gate:* `[theme-legibility]` — a new mechanical contrast check (exact shape decided during
   implementation, once the judgment pass identifies which tokens if any need to change) plus the
   existing `YesDawThemeAuditCheck` staying green. Expected outcome: mostly a judgment pass with a
   small number of token tweaks, not a rewrite — audited near-black-ness and one type scale are
   already real.

2. [x] **V2 — Transport bar: real bar\|beat primary readout; drop the dead KEY cell.** DONE —
   feature `911f6bc`, exact-head nine-job CI run `32429479613` green (first try), local 355/355.
   Audited first and found NO tick/frame-to-bar\|beat conversion existed anywhere in the codebase —
   the ruler's own "bar numbers" (`TimelineCanvas.h:647`, V4's subject) are the closest precedent
   and are themselves fake (`seconds + 1`). Added `engine::computeBarBeat` (`src/engine/Time.h`) —
   a single-tempo/meter frame-position law, honestly scoped to match the existing `headBarFrames()`
   family's own precedent (`Project.tempoMap.front()`/`meterMap.front()` only; no project in this
   app has ever supported piecewise tempo/meter changes in any bar-length law) — logged as a
   deliberate deviation from full piecewise correctness, not a shortfall: extending to piecewise
   would need accurate tempo-ramp inversion, a materially larger task than this presentation fix
   calls for. The header's primary readout now calls the SAME shared `headerBarBeat()` method
   (extracted so paint and the test harness can never disagree) instead of computing a stopwatch
   clock. The KEY cell (`{"--", "KEY"}`, a permanent dead literal — audited and confirmed NO
   key-signature model exists anywhere in `engine::Project`) is removed per D3; the transport box
   shrank from 248px (3 cells) to 164px (exactly 2 cells) so no dead space is left where it used to
   paint.
   `[transport-readout]` (150 BPM, 7/8 meter — the SAME fixture the existing count-in test uses):
   the header reads bar 1 beat 1 at the start; advancing playback by exactly 163,200 frames (2
   bars + 3 beats at this tempo/meter) reads bar 3 beat 4 — cross-checked via the harness accessor
   `mainComponentHeaderBarBeat`, which reads the SAME law the paint code calls, not a duplicated
   formula; the ex-KEY-cell region shows no fillPanel cell colour at all. Red before, proven by
   forcing `headerBarBeat()` to always return `{1,1}` while leaving persistence/paint/harness
   wiring in place — `afterMove.bar` stayed at 1 instead of reading 3.
   Original spec: Audited
   first: the return-to-start/play/stop/record/loop cluster is real and wired
   (`src/ui/MainComponent.cpp:4537-4564`), tempo and time-signature cells read real project data
   (`MainComponent.cpp:8489-8525`), and the master meter + LUFS readout are real
   (`drawMasterMeter`, `MainComponent.cpp:8559-8593`). Two concrete gaps: (a) the large primary time
   readout renders `HH:MM:SS:mmm` timecode (`MainComponent.cpp:8468-8476`), never bar\|beat, with no
   bar/beat computation anywhere in that function; (b) the KEY cell is a **hardcoded literal `"--"`**
   (`MainComponent.cpp:8499`, `{ "--", "KEY" }`) — confirmed **no key-signature model exists
   anywhere** in `engine::Project` (grepped the whole tree; only `tempoMap`/meter map exist,
   `Project.h:818-820`). Per D3, ship the honest subset: compute and paint a real bar\|beat primary
   readout from the tempo map (the SAME tempo-map math `TimelineCanvas.h`'s ruler will need for V4 —
   consider sharing the law), and **remove the KEY cell entirely** rather than leaving a permanent
   dead "--". Log the omission in STATUS.md per D3.
   *Gate:* `[transport-readout]` — on a project with a non-default tempo/meter, the primary readout
   shows the correct bar\|beat position at a known playhead frame (frame-exact, cross-checked against
   the tempo map, not eyeballed); the KEY cell is absent from the painted header (a pixel/geometry
   assertion that the cell's rect is empty or the region contains no "KEY" label), and the header's
   remaining cells (tempo, meter, loop) are unaffected. Expected to fail before on both counts.

3. [x] **V3 — Bottom mixer dock: visible tab labels + a show/hide toggle.** DONE — feature
   `66e187d`, exact-head nine-job CI run `32431990639` green (second try — the `YesDawThemeAuditCheck`
   gate correctly caught 3 raw geometry literals I'd left inline; moved to `UiTheme::Layout`
   tokens and re-ran green), local 355/355. **Second deviation from the literal spec, logged:**
   the reference's SENDS/RACKS/VIEW/OPTIONS are literal Logic-style VIEW-SWITCHING tabs — this
   app's `leftTools` column has no alternate views to switch between (it's ONE unified control
   set), so painting fake multi-tab widgets that switch nothing would itself be dishonest
   interactivity (D3). Shipped the honest equivalent: a real "MIXER" heading painted in the
   band `mixerUtilityTop` already reserved (no layout risk to the dense, fragile
   `layoutMixerControls()` row-visibility cascade below it). The show/hide toggle is a genuine new
   `UiActionId` (`TimelineToggleMixerDock`) — collapsing it reclaims vertical space for the
   timeline/rail/inspector via ONE shared `dockedMixerHeight()` law now threaded through every
   layout function (`mixerPanelBounds`/`timelineBounds`/`leftRailPanelBounds`/`inspectorBounds`),
   so paint and every interactive control's bounds can never drift apart; the full-view Mixer
   panel is unaffected (it never reserved dock space to begin with).
   `[mixer-dock]`: the `leftTools` header band paints real text where it used to be blank fill;
   clicking the shipped toggle collapses the dock (its own reserved rect drops to zero height) and
   the timeline's own rect grows to reclaim that space; the same toggle restores it exactly; the
   full Mixer view stays full-height regardless of the dock's toggle state. Red before, proven by
   forcing `dockedMixerHeight()` to always return the fixed height regardless of the toggle — the
   dock stayed at 244px instead of collapsing to ≤0.
   Original spec: **Deviation from the
   plan's own premise, logged:** the plan's D6/inventory-item-7 wording assumes no dock exists
   inside the arrangement view today ("the V-run adds/aligns the bottom mixer dock"). Audited first
   and found this is **wrong** — a full-width bottom mixer strip is ALREADY always-on inside the
   Timeline and Piano Roll panels: `MainComponent.cpp:4492` trims `kMixerHeight` (260px,
   `UiTheme.h:222`) off the work area and `MainComponent.cpp:4506` paints `drawMixer` into it — the
   EXACT SAME function used for the full-view Mixer panel. `paintedMixerLaneBounds`
   (`MainComponent.cpp:6216-6235`) takes only a `stripIndex` (not a lane rect as the plan assumed)
   and recomputes its own area from `mixerPanelBounds()` every call, which already branches on
   `activePanel` — so the strip-layout law is **already height-agnostic and already reused at the
   short height today**; D6's "build the dock so N3's law is reusable at a shorter height" is
   already satisfied by existing code, not something this item needs to build. The selected-strip
   highlight is also already real, following N2's `readoutStripFor` law
   (`MainComponent.cpp:8227`, consumed at `:8254,8272,8323,8341,8376`).
   The REAL gaps: the dock's left-edge tool column (`leftTools`, 180px wide,
   `MainComponent.cpp:9782-9785`) paints only a panel background — no visible SENDS/RACKS/VIEW/
   OPTIONS-style tab labels like the reference (the real buttons placed there by
   `layoutMixerControls()`, `MainComponent.cpp:6465-6514`, are functional but unlabelled
   accessibility/readout controls); and there is no show/hide affordance at all — grepped the whole
   tree for a toggle concept (`ToggleMixerDock`/`showMixer`/`mixerDockVisible`), zero hits, so the
   full-height Mixer view (`ViewMixer`) and the always-on 260px dock currently coexist with no way to
   collapse the dock when a user wants the timeline's full vertical space.
   *Gate:* `[mixer-dock]` — the `leftTools` column paints real, labelled sections (matching the
   reference's SENDS/RACKS/VIEW/OPTIONS grouping, honestly scoped to what this app actually has —
   D3 applies: no fake RACKS tab if there's no rack model) instead of blank panel fill; a real
   UiActionId toggles the dock's visibility, collapsing it to reclaim vertical space for the
   timeline/piano-roll and restoring it on toggle-back, persisting across the session; the full-view
   Mixer panel (`ViewMixer`) is unaffected by the toggle. Expected to fail before on the toggle
   action not existing and the tab column being blank.

4. [x] **V4 — Ruler: bar numbers driven by the real tempo map, not elapsed seconds.** DONE —
   feature `d510bab`, exact-head nine-job CI run `32750560415` green (first try), local 355/355.
   CP-B was cleared by Dan's verdict A ("closer — continue", 2026-08-24) before this item began.
   Shared law shipped as specified: `engine::computeBarGrid` extracted from V2's `computeBarBeat`
   with the exact same float-operation order (V2's gate stays green untouched), consumed by a new
   pure `computeRulerBarLabels` (labels at real bar starts, thinned to power-of-two bar steps
   until they clear a `timelineCanvasRulerMinBarLabelSpacingPx` spacing floor — the same sparse
   numbering the reference shows). `MainComponent` feeds the canvas state's `barSeconds` from the
   SAME `headTempoMeter()` read the transport readout uses. The retired fake seconds-step tokens
   were removed, not left dead. `[ruler-bars]` (73 assertions): every painted label lands on its
   tempo-map bar start at 150 BPM 7/8 (cross-checked through the shipped pixel→seconds mapping,
   tolerance one painted pixel), bars ascend, and the SAME bar number paints at a DIFFERENT x at
   100 BPM. **Red before**, proven by temporarily restoring the retired seconds law with all
   wiring intact — a label claiming bar 3 sat 1.61 s off bar 3's real start, failing exactly the
   bar-position cross-check.
   Original spec: Audited first
   and found a genuine functional bug, not merely a styling gap: `drawRuler`
   (`src/ui/TimelineCanvas.h:625-683`) computes `barNumber = std::max(1, roundToInt(seconds) + 1)`
   (`TimelineCanvas.h:647`) — raw elapsed SECONDS relabeled as bar numbers, with no bar/beat math
   anywhere in the function. The ruler can only show correct bar numbers by coincidence, at exactly
   60 BPM 4/4. Section-marker labels and the loop-region brace are both already real
   (`TimelineCanvas.h:666-681`, `:948-983`, fed from real `TimelineMarker`/loop state) — this item is
   scoped to the bar-number law only. Share the tempo-map-to-bar/beat conversion this item builds
   with V2's transport readout rather than duplicating the math (both need the same
   frame-or-tick-to-bar\|beat mapping).
   *Gate:* `[ruler-bars]` — on a project with a non-default tempo/meter (e.g. 150 BPM, 7/8, matching
   the existing count-in test's fixture pattern), the ruler's painted bar-number labels match the
   REAL tempo-map-computed bar positions at several known ruler x-coordinates (cross-checked against
   the tempo map, not against a fixed seconds-based formula); changing the tempo changes which
   x-coordinates the SAME bar numbers paint at. Expected to fail before at any tempo other than 60
   BPM / 4/4.

5. [x] **V5 — Track headers: stereo per-track meter, vertical mini-fader.** DONE — feature
   `097bdff`, exact-head nine-job CI run `32766547386` green (first try), local 355/355.
   The MeterNode already published per-channel peaks and taps POST-pan — the UI was discarding
   them. New per-channel path (`PlaybackEngine::trackMeterPeakChannel` → `UiAppModel` → a
   per-track L/R `MeterHoldState` pair on the same B32 hold/clip-latch law; one click clears
   all); the rail paints two independent columns split by `trackListMeterChannelGap`. The mini
   VOL became a VERTICAL column (top = loud) sharing the mixer fader's orientation — ONE
   `volumeSliderBounds` law drives click, drag, fine-drag (now y-axis for VOL, x for pan),
   paint, and Alt+reset; retired horizontal tokens removed; six existing VOL gates re-pinned to
   the vertical axis (same claims, never weakened). `[track-rail-meters]` (40 assertions):
   hard-pan left through the shipped knob → painted L/R diverge (left real, right silent,
   sampled inside the short fixture clip's own play window since MeterNode publishes per-Block);
   the fader rect is taller than wide and y-drags edit persisted gain top-to-bottom. **Red
   before on both counts**, proven separately: the horizontal law failed exactly the
   orientation assertion, the mono law failed exactly the channel-divergence assertion.
   Original spec: Audited first: track
   number, N7 colour chip, name, real M/S/record-arm buttons, the pan knob, and N6 drag-resize are
   ALL real and live-wired (`MainComponent.cpp:8595-8831`, hit-tested via `rowBounds`/`panKnobBounds`/
   `muteCellBounds`/`soloCellBounds`/`meterZoneBounds`). Two concrete gaps against the reference's
   per-track strip: (a) the live meter is **mono, a single float** (`TimelineCanvasTrack::meter`,
   `TimelineCanvas.h:33`), not a stereo L/R pair like the reference's track meters; (b) the
   mini-fader (`volumeSliderBounds`, drawn `MainComponent.cpp:8755-8772`) is a **horizontal** bar,
   where the reference shows a vertical fader matching the mixer strip's own fader orientation.
   *Gate:* `[track-rail-meters]` — on a 2+ channel asset, the rail meter shows independent L and R
   peak values (proven by feeding a hard-panned or L/R-asymmetric signal and asserting the two
   channels' painted heights/values differ); the mini-fader's hit-test and paint law both use a
   VERTICAL rect (y-axis controls gain), matching the mixer strip fader's orientation law so the two
   controls feel like the same instrument. Expected to fail before on both counts (today's meter law
   has no stereo split; today's `volumeSliderBounds` law is horizontal).

6. [x] **V6 — Clips: name label, fade curves, and real selection state.** DONE — feature
   `436a101`, exact-head nine-job CI run `32776482872` green (first try), local 355/355.
   **Two deviations from the carve, logged:** (1) the clip NAME is ALREADY painted on the body —
   that landed after this carve's audit (`TimelineCanvas.h` paint loop draws `clip->name` in
   kText/topLeft), so the gate pins it as coverage rather than new code; (2) a selection law DID
   exist by ship time — a colour swap to accent blue — but it was pixel-invisible on an
   accent-blue track (the exact false-positive risk the N7 gate's own comment works around), so
   V6 REPLACED it: a selected clip keeps its N7 track colour and paints an unmistakable ring.
   Fades shipped as specified: the wedge samples the engine's ONE envelope law
   (`evaluateClipFadeEnvelopeGain`) via the shared `clipFadeCurvePoints` helper V7 will reuse —
   never a re-derived formula. `[clip-identity]` (44 assertions): name text paints in the lane
   region; on an accent-blue track, selecting visibly changes the clip's pixels and deselecting
   restores them EXACTLY; a persisted 40% fade-in paints a wedge at the clip start while the
   mid-body stays pixel-identical. **Red before, proven separately**: under the retired law the
   selection assert failed (blue-on-blue changed zero pixels) and the fade assert failed (a
   persisted fade painted nothing).
   Original spec: Audited first and found
   the LARGEST hard gap in the whole carve: waveform rendering, MIDI note-preview, and N7 colour
   tinting are all real (`TimelineCanvas.h:340-568`), but (a) **no clip name is ever painted on the
   clip body** — the name exists on the `Clip` struct and reaches the inspector
   (`MainComponent.cpp:9621`) but never the timeline canvas itself (grepped `TimelineCanvas.h` for
   any clip-name draw call: zero hits); (b) **no fade curve is drawn on the clip body at all** —
   fade values exist only as inspector text rows (`MainComponent.cpp:9713-9722`); grepped
   `TimelineCanvas.h` for fade-related drawing: zero hits; (c) **no per-clip selection state exists
   in the paint path at all** — `TimelineCanvasState` has no `selectedClipIds` field, and
   `MainComponent.cpp`'s real `selectedClipIds` (`:3146-3157`) is never consumed by any paint
   function (grepped for selection-halo/outline drawing: zero hits). A selected clip today is
   pixel-identical to an unselected one on the timeline — confirmed the SAME underlying gap (no
   curve-drawing capability exists yet) is shared with V7's fade-curve requirement; build the curve
   law once, here, and have V7 reuse it.
   *Gate:* `[clip-identity]` — on a project with named clips and non-zero fades, each clip's name is
   legibly painted on its body (not clipped/invisible at the shipped minimum clip width); fade
   in/out are painted as visible curves on the clip body matching the persisted
   `fadeInSeconds`/`fadeOutSeconds`/curve-type values (not just inspector text); selecting a clip (a
   real click through the shipped gesture) changes its painted appearance in an unmistakable,
   assertable way (a pixel-diff before/after selecting, at the clip's own rect) and deselecting
   restores the original appearance exactly. Expected to fail before on all three: no name text
   found in the clip's painted rect, no curve pixels distinct from a flat fill, and before/after
   selection pixels identical.

7. [ ] **V7 — Right inspector: a real TRACK tab, a gain knob, wired fade curves and Clip FX, and an
   automation section.** Audited first and found this is shallower than it looks: the CLIP/TRACK
   tabs are **decorative only** — grepped for any `inspectorActiveTab` state or tab-click handler:
   zero hits; the panel shows clip data regardless of which tab is "selected," and
   `UiAccessibility.h` only inventories `"clip.inspector"`, no `"track.inspector"` entry at all.
   Clip name/start/end/length are real reads (`MainComponent.cpp:9603-9683`). Gain is a **text
   readout, not a knob widget** (`:9686-9703`) — the reference shows a rotary gain control matching
   the clip's own visual language. Fades are three text rows with no curve display
   (`:9705-9737`) — share V6's curve-drawing law here instead of duplicating it. The Clip FX list is
   a **hardcoded stub**: it always paints the literal `"None"` regardless of actual FX-chain state
   (`:9739-9746`) — not wired to real data at all. The reference's Automation section (target chooser
   + mini curve preview) has been **supplanted by a Takes section** in the current code
   (`:9748-9771`) — automation editing lives on the timeline canvas instead
   (`automationLaneToggleBounds`/`automationTargetChooserBounds`, `UiTheme.h:711-754`); per D3, do
   not invent an automation section that duplicates the timeline's real editor — either add a
   READ-ONLY mini curve preview backed by the SAME real automation lane data (honest, non-editable
   reflection) or omit it and log why, but never fake the curve. Takes stays (it's real, E33-shipped
   content) — this item adds automation preview alongside it, not instead of it.
   *Gate:* `[clip-track-inspector]` — clicking the TRACK tab actually shows track-scoped content
   (name, colour, output routing or whatever the honest subset supports) distinct from clip content,
   and a real `"track.inspector"` a11y entry exists; the gain control is a real draggable knob widget
   that edits `clip.gain` (not just displays it — REQUIRE a drag changes the persisted value); the
   fade rows show the SAME curve V6 draws on the clip body (cross-checked pixel/shape match, not a
   re-derived formula); the Clip FX list reflects the REAL fx chain (adding an FX via the existing
   verb changes what this list paints, removing it changes it back) instead of the "None" stub; if
   an automation mini-preview ships, it reflects REAL breakpoint data and updates when a breakpoint
   is added on the timeline. Expected to fail before on the TRACK tab (no distinct content), the
   gain readout (not draggable), and the FX list (always "None" regardless of chain state).

8. [ ] **V8 — Toolbar row: a visible zoom control.** Audited first: the tool palette
   (Pointer/Pencil/Scissors/Hand/Zoom, `UiActions.h:199-206`) and the SNAP chooser are ALREADY a
   single unified row (`drawToolbar`, `TimelineCanvas.h:570-623`, both painted in the same function
   and the same `toolbar` rect) — contrary to a worst-case "scattered across locations" assumption,
   this part of the reference's layout is already matched structurally. The repeat-paste chooser and
   the automation-lane toggle button also share the same horizontal band (`UiTheme.h:693-694`'s own
   B41 comment confirms this was a deliberate prior fit). The one concrete gap: **no zoom control
   widget exists anywhere in the toolbar row** — only `TimelineZoomIn`/`TimelineZoomOut` action
   handlers (likely menu/keyboard-bound, `MainComponent.cpp:7328-7346`) and Ctrl+wheel zoom
   (`MainComponent.cpp:3209-3210,3725-3726`) exist; there is no zoom %-readout or slider painted in
   the toolbar itself, so zoom is invisible/undiscoverable as a toolbar affordance even though it
   works via other gestures.
   *Gate:* `[toolbar-zoom]` — a real zoom control (slider, stepper, or %-readout — pick the honest
   minimal widget, no fake precision) is painted in the toolbar row, shows the CURRENT zoom factor
   live (matching what Ctrl+wheel/the Zoom tool already produce, proving it's one shared law not a
   second zoom concept), and dragging/clicking it changes the timeline's actual zoom the same way
   the existing gestures do. Expected to fail before on no such control existing at all.

## Explicitly out of scope — do not fake (inherited from the plan's D9)

- MIDI CC / sustain / pitch-bend / aftertouch.
- H18 plugin hosting (gated on Dan's Smoke 2 run).
- ASIO backend.
- Comping UI beyond the existing Comp button, time-stretch, tab-to-transient, ripple editing.
- The reference's SENDS/RACKS side-tab CONTENT beyond honest labelling (V3 ships real, labelled
  groupings of what already exists — it does not invent a sends-routing UI or a plugin-rack UI that
  isn't backed by a real model).
- Track instrument icons and a key-signature model (D3 honest-subset omissions from the plan;
  candidates for a later backlog, not sneak-ins here).

## Definition of done (whole list)

Every landed item: D7 judgment loop first (render real shipped shell at all three sizes, judge
against the reference, iterate), THEN a mechanical shipped-boundary gate (failing before, passing
after), suite green locally with the owner-file isolation ritual, one feature commit, EXACT-HEAD
GitHub Actions run green across all nine jobs, then a separate docs-only evidence commit ticking the
item here with the SHA + run id and adding the STATUS.md certification paragraph. CP-B fires after
V1–V3; CP-C fires after V8. Deviations (like V3's corrected premise) are logged in the item itself
and in STATUS.md, never silently absorbed.
