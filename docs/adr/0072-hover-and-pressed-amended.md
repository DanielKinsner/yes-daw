# 0072. ADR-0067 amended: hover and pressed as the code and the contrast law allow

- **Status:** Accepted (agent, 2026-10-08, under ADR-0049's implementation-ADR clause - ADR-0063, 0066 and 0067 are
  agent-decided - after a design workflow (two amendments, two judges) and a separate agent critic; its blocker and
  should-fixes resolved below)
- **Date:** 2026-10-08
- **Deciders:** build agent (proposer), a design workflow, separate agent critic
- **Related:** amends ADR-0067 cp2 ("Hover and pressed") in
  [`docs/adr/0067-interaction-and-motion-states.md`](0067-interaction-and-motion-states.md); the plan's G6.4 in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md); ADR-0063 (tokens
  and contrast, standing 4.5:1 for text); ADR-0066 (every painted control is a record and a Control target with its own
  accessible element — one geometry law for paint, hit-test, record); ADR-0064 (the scaling matrix,
  `UiThemeLayout::leftRailMinWidth`); ADR-0065 (scrolled-out strips have no parts); the planner's working note
  `.git/yesdaw-wip/adr-0067-cp2-plan.md`.

## Context

ADR-0067 is Accepted. Its cp1 (meters) has shipped. Its cp2 ("Hover and pressed") specifies pointer tints painted by the
shell in `paintOverChildren` over each `PaintedControl` record from ADR-0066: a white fill at
`UiTheme::Tone::hoverHighlightAlpha` (0.10) over the hovered one, a second white fill at `pressedHighlightAlpha` (0.16)
over the pressed one, **stacked** when a control is both. A `PointerTracker` + `MainComponent::pointerEvent` seam feeds
every pointer event; hover is "the record whose bounds contain the position **and** whose surface is, or contains, that
component"; a UI tick re-resolves hover from the last position; a primary press names the pressed record while that
button is down; native widgets gain the same two tints in `YesDawLookAndFeel`. Seven gates and a real-app sweep stand.

The planner's cp2 note (verified pointers against HEAD `2871be5`) found **five** places where ADR-0067 does not match
the code we would ship it on. Each must be settled before cp2 code lands. The numbers below are computed from the
actual tokens in `src/ui/UiTheme.h` with the gamma-correct WCAG 2.x relative-luminance formula; arithmetic is shown in
the Decision for the five text pairs that fail.

**A. Contrast under the specified tints.** Muted text IS drawn inside records (empty-pad key names,
`InstrumentPanelComponent.h:242`; bypassed insert labels, `MainComponentMixer.cpp:1689`; several send / insert / I/O
rows after ADR-0063 shrank them). The ADR requires 4.5:1 for text pairs **under the hover tint and under both tints**.
Under the stacked `(1 - 0.10)(1 - 0.16) = 0.244` white composite, the planner's measurements are
`mutedText / panelRaised` 4.25 (hover) / **2.55** (hover+pressed); `/ buttonSurface` 3.90 / **2.37**; `/ darkControl`
4.49 / **2.68**; `/ samplerPadEmpty` 3.82 / **2.32**; `text / samplerPadLoaded` 5.92 / **4.02**. Four of five muted
pairs fail 4.5:1 under hover **alone**, and all five fail under the stacked state. Pressing what you hover is the normal
case, so stacking is common.

**B. The overlay rule for shell-surfaced records.** "Hovered record = one whose surface IS or CONTAINS the event's
component" is wrong for records whose surface is `MainComponent` itself: the shell contains every overlay (the New
Project dialog, the FX editor, the keymap editor). An event on an overlay carries `component == overlay`, which the
shell `contains`, so the header's gear and time readout would phantom-hover through an open dialog.

**C. Tick re-resolution of the component.** The ADR re-resolves hover each tick from "the last position and the current
records" but keeps the last event's component. That component goes stale when an overlay opens over a stationary pointer
or when the dock swaps tabs (Instrument -> Mixer): the cache then names a now-hidden sibling.
`juce::Component::getComponentAt` depends on the shell being shown, which the headless gates' shell is not, so the
resolution needs an in-house walk.

**D. Rail record overlap at narrow widths.** The scaling matrix helper
(`tests/ui_screenshot_tests.cpp:838-868`) never varies rail width. At the default 260 px rail every record is disjoint.
At rail widths 180-203 `rail.row.N.arm` overlaps `.volume`; at widths below 190 `.meter` overlaps too
(`UiThemeLayout::leftRailMinWidth` is 180). Users can drag the splitter to any width in that range today.

**E. Seam and small ones.** `MainComponent::pointerEvent(kind, shellPoint, component)` carries no modifiers, so a
right-only or middle-only press cannot be told from a primary press at the resolver. Four related points are
unresolved: restoring `juce::ModifierKeys::currentModifiers` across gates; whether hover follows the pointer during a
drag or stays pinned to the pressed record; whether native widgets stack both tints or paint one state; the fader
record (18 px rail, `paintedFaderRailForLane`) vs the 28 px thumb; the rail volume's +/-6 px x-slop
(`trackListLevelHitSlopX`) reaching 2 px into the meter.

## Compatibility with Accepted ADRs

This ADR **amends** ADR-0067 cp2 only. ADR-0067's Accepted body (two channels one meaning each; records-disjoint;
the `pointerEvent` seam and `PointerTracker`; tick re-resolution from the last position; two-rect repaint under the
ring; press-is-the-record-that-took-the-primary-press-while-that-button-is-down; pointer states never touch
accessibility or the model; cp1 meters) stands unchanged. Four edges in Section 2 and 3 get the narrow clauses below
(overlay rule for shell-surfaced records; tick walks children; seam carries modifiers; hover and pressed independent
during drag). The **visual form** of the tints and the **native widget tint rule** change in cp2 and are stated in full
below; ADR-0067's `[g6-motion][hover]`, `[g6-motion][pressed]` and `[g6-motion][widget-states]` gates are rewritten here
and supersede their text in ADR-0067.

ADR-0063's standing law — text ≥ 4.5:1 against every surface it is drawn on, with the opaque-token rule — is
**preserved by construction**: both pointer states are inner edge strokes that stay outside a record's text inset, so no
text pixel and no surface pixel under text changes under hover or press. No new text pair is declared; `mutedText` is
not re-brightened.

ADR-0066's record contract — one record per painted hit-zone, built by the surface that paints it, from the same
geometry its paint and hit-test read — is **preserved**. The painted fader's record stays the 18 px rail bounds the
surface paints and hit-tests on. The strict-bounds / slop separation (point E6) keeps records disjoint without touching
the ergonomic +/-6 px drag capture.

ADR-0064's scaling matrix stays as it is; `UiTheme::Layout::leftRailMinWidth` rises (point 4) and the rail splitter's
existing gate (`tests/ui_input_tests.cpp`, "drags it within 180-400") follows the token.

## Decision

**1. The tint form is a geometric change: inner edge strokes, not a fill.** ADR-0067's two white fill tints retire. In
their place:

- **Hover** paints a **1 px white inner stroke** at the record's bounds, inset `L::pointerStrokeInset = 1` px, with the
  ring's corner radius, at `Tone::hoverStrokeAlpha = 0.60`.
- **Pressed** paints a **2 px white inner stroke** at the record's bounds, inset `L::pointerStrokeInset = 1` px, with
  the ring's corner radius, at `Tone::pressedStrokeAlpha = 0.95`. **No fill**: a darken painted in `paintOverChildren`
  would darken the record's text with its surface (the shell paints after the children) and drop `mutedText` below
  4.5:1 on `buttonSurface` (4.36) and `samplerPadEmpty` (4.30) - the critic's computation, both pixels darkened.
- **Text clearance.** Both strokes stay within 3 px of the record's edge (inset 1 + the 2 px pressed width). A family
  whose painted text comes closer than that (S4's `[pointer-text-contrast]` finds it) gets a **1 px** pressed stroke at
  `Tone::pressedStrokeAlpha` - still distinct from hover by its alpha - decided here so S4 needs no new ADR. The critic
  found three such families: the mixer's insert, send and I/O rows (15 px rows carrying 11 px text,
  `UiTheme.h:595-606`); every other family (rail and strip cells, pads, the tool cells, the gear) keeps the 2 px stroke.
- **Hover and pressed are mutually exclusive VISUALLY.** A record that is both hovered and pressed paints the pressed
  form only (a thicker, brighter stroke - or the same width at a brighter alpha where the text-clearance fallback applies). The probe's `pointer.hovered`
  and `pointer.pressed` fields are still set **independently** when both apply, so mechanical gates see the semantic
  signal.
- **The shell paints both in `paintOverChildren`, under the ring**, at the record's rounded corners, with the same
  clip-aware shell paint ADR-0067 adds.
- **New tokens** in `UiTheme.h`: `Tone::hoverStrokeAlpha = 0.60`, `Tone::pressedStrokeAlpha = 0.95`,
  `L::pointerStrokeInset = 1`, `L::pointerHoverStrokeWidth = 1`, `L::pointerPressedStrokeWidth = 2`. The old
  `Tone::hoverHighlightAlpha` and `Tone::pressedHighlightAlpha` are removed in **S5**, with the look-and-feel rewrite
  that is their last user (removing them earlier breaks the build). Within `paintOverChildren` the order is: the pressed
  stroke, the hover stroke (on a different record during a drag), then the keyboard ring on top.

**Numbers (actual UiTheme tokens, gamma-correct WCAG 2.x).** Base ratios for the five pairs ADR-0067's fills would
have broken: `mutedText / panelRaised = 5.67`, `/ buttonSurface = 5.27`, `/ darkControl = 5.90`,
`/ samplerPadEmpty = 5.17`, `text / samplerPadLoaded = 7.80`. Under hover and under press (edge strokes that never reach
a record's text inset) no pixel under or of the text changes, so the ratios stay **5.67 / 5.27 / 5.90 / 5.17 / 7.80**.
**Hover + pressed = pressed only** on the same record.

**2. The overlay rule gains an exact-match clause for shell-surfaced records.** ADR-0067 Section 2 is amended so that:

> The hovered record is the one whose bounds contain the position **and** either (a) the record's surface **is** the
> event's component **exactly** when the record's surface is `MainComponent` itself, or (b) the record's surface
> contains the event's component when the record's surface is a child of the shell.

Shell-surfaced records in today's code are `header.gear` and `header.time` only (the tool cells' surface is
`timelineInput`, `MainComponentControls.cpp:271`; every mixer record's is `mixerStripsInput`, `:393`); the record
carries its surface pointer, so the dispatch to clause (a) vs (b) is one branch at the hover resolver. No records are
reclassified.

**3. Tick re-resolution walks the children.** ADR-0067 Section 2's "re-resolved every UI tick from the last position
and the current records" is amended so that, at tick time **when no primary mouse button is down**, the shell ignores
the last event's cached component and performs a manual hit test: an in-house helper **`walkChildrenAt (shellPoint)`**
walks `MainComponent`'s children in **reverse z-order** (last-painted first), reading only `isVisible()` and
`getBounds()`, descending into the deepest visible child whose local bounds contain the point; the deepest match is the
current event component. If no child contains the point, the hover resolves to **none**. The amendment explicitly
**forbids `juce::Component::getComponentAt`** at this site (it answers through the component's own hit test and
visibility on screen, which the hidden, headless shell of the gates does not have - the same walk must serve both).
When a primary button **is** down, the drag's component is kept (point 7 below).

**4. `leftRailMinWidth` rises from 180 to 204.** `UiTheme::Layout::leftRailMinWidth` becomes **204** — the smallest rail
width at which `rail.row.N.arm`, `.volume` and `.meter` are pairwise disjoint. Narrower rail widths no longer exist
(the arrange splitter range becomes `[204, leftRailMaxWidth]`). No records are dropped by width; the
`[g6-motion][records-disjoint]` gate becomes a sweep across the full user-draggable range, pinned to the token.
`leftRailWidth` default (260) is unchanged.

**5. The seam signature carries modifiers.** The seam becomes:

```cpp
void MainComponent::pointerEvent (PointerKind kind,
                                  juce::Point<int> shellPoint,
                                  juce::Component* component,
                                  juce::ModifierKeys modifiers);

void mainComponentPointer (MainComponent& shell,
                           PointerKind kind,
                           juce::Point<int> point,
                           juce::Component* component,
                           juce::ModifierKeys modifiers);
```

Modifiers are passed by value from `juce::MouseEvent::mods` on every PointerTracker callback. `pointer.pressed` is set
**only** when `modifiers.isLeftButtonDown()` is true at the event (a right-only or middle-only mouse-down sets no
press); `modifiers.isPopupMenu()` clears any press. The gates feed modifiers through the parameter and never read
`juce::ModifierKeys::currentModifiers` for an assertion. The production `PointerTracker` still mirrors JUCE's static so
the production path and the gated path agree on the pressed-tick cleanup in ADR-0067 Section 3.

**6. Gates that write `juce::ModifierKeys::currentModifiers` wrap it in RAII.** A header-only
`ScopedCurrentModifiers` guard (in the gates' test file, beside its other helpers): construction captures the previous value
and sets the simulated modifiers; destruction restores the captured value. Every gate harness that mutates the static
uses the guard, and asserts `juce::ModifierKeys::getCurrentModifiers()` equals the captured baseline at the end of
every subtest.

**7. Hover and pressed are independent during a primary drag.** ADR-0067 Section 3 is amended so that during a
primary-button drag that began on record **A**, moving the pointer over record **B** sets `pointer.hovered = B` while
`pointer.pressed` stays **A** until mouse-up. The paint draws the **pressed form on A and the hover form on B**
simultaneously (when `A != B`); release clears pressed, and hover follows the pointer thereafter from the next event or
tick. The two probe fields stay orthogonal.

**8. Native widgets paint the same form, pressed XOR hover.** `YesDawLookAndFeel::drawButtonBackground`,
`drawToggleButton`, `drawComboBox` and the slider drawers paint the amendment's inner-stroke form from the widget's own
`isMouseOver` and `isDown` state: `isDown` draws the pressed form (2 px inner stroke) and no hover stroke (a native
button keeps its own pressed background, `UiTheme::Color::buttonPressed`, which it paints under its text); `isMouseOver && !isDown` draws the hover form (1 px inner stroke); neither state draws the retired
white-fill tints. Pressed and hover never stack on native widgets, matching painted records (point 1).

**9. The painted fader's record is the 18 px rail.** ADR-0066's record contract holds: the record returned by
`collectPaintedMixerControls` for the strip fader equals `paintedFaderRailForLane` (18 px). The 28 px thumb overhangs the
rail by 5 px on each side (`MainComponentMixer.cpp:1505-1508`); its centre is inside the record, its overhang is not.
Hovering the thumb's centre hovers the fader and paints the stroke at the rail's bounds - in line with the keyboard
target's ring, which follows the record; the overhang hovers what is under it (no record). That the mouse's grab
reaches the overhang while the record does not is ADR-0066's existing geometry, recorded as a follow-up.

**10. The volume record's bounds are the strict `volumeSliderBounds`; the +/-6 px x-slop is for the drag only.** The
`trackListLevelHitSlopX = 6` px kept for the primary-button drag gesture does **not** widen the record. The volume
record's bounds equal `trackListInput.volumeSliderBounds` exactly, so a point that is 2 px inside the meter but inside
the slop-expanded volume rect hovers `rail.row.N.meter`, not `.volume`. A gesture that begins anywhere in the slop
still writes `pointer.pressed = rail.row.N.volume` once the drag begins, through a `pointerEvent` emitted at the
gesture seed with the record's component.

## Gates

Every gate below fails on a build without its law; thresholds are computed from the tokens at test time, never retyped.
These gates **supersede** the matching lines in ADR-0067 (same test file, `tests/ui_screenshot_tests.cpp` / `tests/
token_contrast_tests.cpp` / the record and probe suites).

- **`[g6-motion][hover]` (rewritten).** For one record of every family (rail-row cells, knobs, slider, swatch, meter;
  strip cells, knobs, fader, send, insert, I/O; master slot and clip indicator; tool cell; `header.gear`,
  `header.time`; sampler pad), a move to its centre names it in `pointer.hovered` **and** the software render:
  - samples the 1 px inner-stroke pixel on **all four edges** of the record's bounds, inset `L::pointerStrokeInset` px,
    and asserts it matches `juce::Colour::white().withAlpha(Tone::hoverStrokeAlpha)` composited over the record's
    surface pixel (± 2 per channel; alpha read from the token, never retyped);
  - asserts the record's **interior** (inside the stroke) is unchanged (± 2 per channel) from the pre-hover render;
  - asserts `pointer.hovered == <record id>` and `pointer.pressed == ""`.
  An exit clears both.

- **`[g6-motion][pressed]` (rewritten).** A primary press on each family's record:
  - samples the 2 px inner-stroke pixel on all four edges and asserts it matches
    `white().withAlpha(Tone::pressedStrokeAlpha)` over the record's surface pixel (± 2 per channel);
  - asserts the record's interior inside the stroke is unchanged (± 2 per channel) from the resting render;
  - asserts `pointer.pressed == <record id>`;
  - asserts a right-only mouse-down (`modifiers.isPopupMenu()`, `!isLeftButtonDown()`) sets `pointer.pressed == ""`;
  - asserts a modifier-less release and a tick with `isLeftButtonDown() == false` clear pressed;
  - asserts a lost capture or a record that disappears clears pressed on the next tick.

- **`[g6-motion][pointer-text-contrast]` (new).** For each (text token, surface) pair declared in ADR-0063's cp1 table
  (`mutedText` on `panelRaised`, `buttonSurface`, `darkControl`, `samplerPadEmpty`, `controlInset`, `inspectorTab`,
  `selectedStrip`, `selectedLane`, `mixerBack`, `canvasLayer`, `knobFace`, `timelineToolbar`, `panel`; `text` on
  `samplerPadLoaded`; the clip-body pair), under the hover render and under the pressed render:
  - samples the painted text pixel and the surface pixel under it;
  - computes the WCAG 2.x gamma-correct contrast ratio and asserts ≥ 4.5 : 1;
  - asserts the text pixel and the surface pixel under it are unchanged from the resting render (no stroke reaches the
    text). If any painter draws text within `L::pointerStrokeInset + L::pointerPressedStrokeWidth` (3 px) of its
    record's edge, the gate goes red.

- **`[g6-motion][pressed-replaces-hover]` (new).** A record that is both hovered and pressed:
  - mean-abs-difference of a (hovered + pressed) render vs a (pressed-only) render is `0` inside the record and `0`
    outside;
  - `pointer.hovered == <id>` and `pointer.pressed == <id>` are both set (the probe fields are independent).

- **`[g6-motion][records-disjoint-matrix]` (rewritten and extended).** At every rail width in
  `{204, 220, 240, 260, 300, 340, 400}` (reading `UiTheme::Layout::leftRailMinWidth` from the token), with every scaling
  matrix cell, with the dock shown and hidden, with the 1-, 8- and 16-track fixtures:
  - no two admitted records' bounds intersect;
  - `UiTheme::Layout::leftRailMinWidth` is read from the token (the gate fails if a future token drift drops it below the
    disjoint-safe value computed from the actual rail cell geometry at test time).

- **`[g6-motion][overlay-blocks-hover]` (new; closes the shell-surfaced hole).** Open each overlay (`NewProjectDialog`,
  `FxEditorWindow`, `KeymapEditor`) over the header and over the arrange; move the pointer to the centre of each
  shell-surfaced record (`header.gear`, `header.time`):
  - `pointer.hovered == ""`;
  - no stroke is painted on the record (software-render MAD vs the pre-overlay base over the record's bounds is `0`
    ± 2 per channel).
  Also assert that for a child-surfaced record (a tool cell, a strip knob, a rail cell) under the overlay, hover is
  clear too (the event's component is the overlay, which the record's surface does not contain).

- **`[g6-motion][tick-walks-children]` (new).** With the pointer stationary over `rail.row.3.volume`, with no primary
  button down:
  - (a) open an overlay; after one tick `pointer.hovered` names the overlay (or `""` if the overlay has no record at
    that point), and the pre-overlay stale record is NOT reported;
  - (b) switch the dock tab (Instrument -> Mixer); after one tick `pointer.hovered` names the record of the newly
    shown tab under the pointer (or `""`);
  - (c) hide `trackListInput`; after one tick `pointer.hovered == ""`;
  - asserts the in-house `walkChildrenAt` was called and `juce::Component::getComponentAt` was not (a test shim counts
    calls on both sites).

- **`[g6-motion][modifiers-in-seam]` (new).** Fires every combination of primary / secondary / middle × every modifier
  mask (none, Shift, Ctrl, Alt, Cmd, Shift+Ctrl, Shift+Alt, Ctrl+Alt) through `mainComponentPointer`:
  - a secondary-only or middle-only mouse-down sets `pointer.pressed == ""` for every modifier mask;
  - a primary-button mouse-down sets `pointer.pressed == <record id>` for every modifier mask;
  - after each subtest, `juce::ModifierKeys::getCurrentModifiers()` equals the baseline (asserted by
    `ScopedCurrentModifiers` RAII).

- **`[g6-motion][hover-follows-drag]` (new).** Start a primary-button drag on `rail.row.3.volume`; move the pointer over
  `rail.row.5.pan`:
  - `pointer.hovered == "rail.row.5.pan"` and `pointer.pressed == "rail.row.3.volume"`;
  - the software render shows the **pressed form** on the volume record and the **hover form** on the pan record
    simultaneously;
  - release the primary button; pressed clears, `pointer.hovered` stays `"rail.row.5.pan"`.

- **`[g6-motion][fader-record-tint]` (new).** With the pointer over a mixer fader's **rail** (18 px) but off the thumb,
  the inner stroke paints at the record's rail bounds. With the pointer over the thumb's **centre**, `pointer.hovered` is
  the fader record and the stroke paints at the rail's bounds, **not** the thumb's; over the thumb's 5 px **overhang**,
  `pointer.hovered` is not the fader.

- **`[g6-motion][rail-volume-strict-bounds]` (new).** The volume record's bounds equal `volumeSliderBounds` exactly
  (no slop). A point 2 px INSIDE the meter but inside the slop-expanded volume rect hovers `rail.row.N.meter`, not
  `.volume`. A primary-button drag that begins inside the slop writes `pointer.pressed = rail.row.N.volume` through the
  gesture-seed `pointerEvent`.

- **`[g6-motion][widget-states]` (rewritten).** A `juce::Button` and a `juce::ToggleButton` with
  `isMouseOver == true && isDown == false` paint the hover form (1 px inner stroke) and no second tint. With
  `isDown == true` they paint the pressed form (2 px inner stroke) and NOT the hover stroke. The combo box and
  the slider in the real-app drive paint the same forms when the drive's real pointer is over / down (measured in the
  window's pixels).

- **`[g6-motion][hover-follows-state]` (unchanged, re-grounded).** A pointer left on a strip that scrolls out, a track
  that is deleted and a dock that switches tabs — after one tick `pointer.hovered` names what is under it now (via
  `walkChildrenAt`) or `""`, and the old rect was repainted.

- **`[g6-motion][repaint-scope]` (unchanged).** Every pointer transition repaints exactly the old and new expanded
  rects, adds no full invalidation, and the shell's paint draws only the regions the rects touch.

- **`[g6-motion][a11y-steady]` (unchanged).** Fifty hover and press transitions over every family leave
  `paintedElementCreations` and every element's description unchanged.

- **Real app (`ss7`).** The real-pointer sweep adds a modifier mask (right-click = no press) and a drag that leaves the
  pressed record's visible bounds; the rubric judges hover, pressed and (hover on B + pressed on A) shots at
  1280×720 / 1920×1080 / 2560×1440.

## Checkpoints (ADR-0067 cp2 sub-slices, adjusted)

Each slice goes green on its own.

- **S1 — Clip-aware shell paint + `frame.regionsPainted`.** The shell's `paint` and `paintOverChildren` skip regions
  whose bounds the clip does not reach; the probe exposes `frame.regionsPainted`. Gates: `[repaint-scope]` on a static
  pointer transition.
- **S2 — Record cache + `[records-disjoint-matrix]`.** `collectPaintedControls` / `collectPaintedMixerControls` feed a
  shell-owned record cache, filled where `syncPaintedAccessibilityProxies` runs (the scroll paths re-sync since
  `02a9d2c`, which fixed the planner's stale-after-scroll finding for the accessible elements). With the 204 floor,
  `tests/preferences_tests.cpp` (a 200 px rail round-trip) moves to a width at or above the floor, and the plan's
  "drag 180-400" line follows the token (a saved view state or preference below 204 already clamps up on load:
  `MainComponentCommands.cpp`'s `jlimit (L::leftRailMinWidth, ...)`). `leftRailMinWidth` becomes 204. Gates:
  `[records-disjoint-matrix]` across the full user-draggable range.
- **S3 — Pointer state and seam.** `PointerTracker`; `mainComponentPointer`/`pointerEvent` with modifiers;
  `walkChildrenAt`; `pointer.{hovered, pressed, lastRepaint}`; exact-match clause for shell-surfaced records; drag
  independence of hover and pressed; `ScopedCurrentModifiers` and the gate harness assertion. Gates:
  `[hover]` naming only (no tint yet — the paint of S4 asserts the form), `[hover-follows-state]`, `[pressed]`
  (semantic fields only), `[overlay-blocks-hover]`, `[tick-walks-children]`, `[modifiers-in-seam]`, `[hover-follows-drag]`
  (semantic fields only), `[rail-volume-strict-bounds]`, `[a11y-steady]`.
- **S4 — Tints paint (`paintPointerTints` before the ring).** The new tokens; shell `paintOverChildren` draws the hover
  stroke and the pressed stroke at the record's rounded corners, pressed replacing hover on one record. Gates: full
  `[hover]`, full `[pressed]`, `[pressed-replaces-hover]`, `[pointer-text-contrast]`, `[fader-record-tint]`, the paint
  half of `[hover-follows-drag]`.
- **S5 — Native widget tints (pressed XOR hover, same form).** `YesDawLookAndFeel::drawButtonBackground`,
  `drawToggleButton`, `drawComboBox` and the slider drawers paint the inner-stroke form from the widget's own state; the
  retired `hoverHighlightAlpha` / `pressedHighlightAlpha` tokens go. Gates: `[widget-states]`; the `ss7` real-app sweep
  over native widgets at the three plan sizes.

## Alternatives rejected

- **Lower the ADR-0067 alphas so stacked keeps 4.5 : 1 for every muted pair.** The hover alpha that keeps `mutedText`
  at 4.5 : 1 on `buttonSurface` is ~0.03 — a ~8/255 brightening on the darkest panel, acknowledged in the "minimal"
  amendment as potentially invisible. There is no mechanical gate for "the hover reads"; the rubric is the only
  backstop at ship time; a form change makes "reads clearly" a property of geometry, not of an alpha near the floor.
- **Narrow ADR-0063's 4.5 : 1 to 3 : 1 for muted text under the stacked state.** Defended under WCAG 1.4.11 (non-text
  UI 3 : 1) and 1.4.13 (hover-triggered content). 1.4.11 explicitly excludes text content; 1.4.13 is about
  dismissability and persistence of revealed content, not about relaxing text contrast under a tint. Relaxing ADR-0063's
  Accepted 4.5 : 1 for muted text under any visible state is the weakest WCAG posture available and rewrites a standing
  law of another Accepted ADR; the hard rule requires a "still-WCAG-defensible rule".
- **Brighten `mutedText`.** An ADR-0063 token rewrite, outside this amendment's scope, and would widen the surface a
  cp2 lands on.
- **Paint the tint under the record's text by each painter.** Needs every painter to collaborate on a text rect for its
  own record — five geometry laws ADR-0066 explicitly rejected ("one record per hit-zone from the same geometry its
  paint and hit-test read").
- **A pressed darken over the record.** Painted after the children, it darkens the text with its surface: `mutedText`
  falls to 4.36 on `buttonSurface` and 4.30 on `samplerPadEmpty` (the critic's computation). The heavier stroke alone
  carries the pressed state.
- **Stack both tints on native widgets.** Consistent with ADR-0067's as-written stacking rule, but inconsistent with
  this amendment's point 1 (geometric form, pressed replaces hover). Mixing the two rules in `YesDawLookAndFeel` would
  make a hovered native button and a hovered painted record look different; this amendment preserves one-feel.
- **Pin hover to pressed during a primary drag (Logic send-drag gives no drop-target highlight).** Contradicts
  ADR-0067's unqualified "re-resolved every UI tick from the last position"; also drops the drop-target feedback that
  send / insert drags benefit from. Independent fields keep both probe signals orthogonal.
- **The fader record = 28 px thumb hit-zone.** `collectPaintedMixerControls` builds the fader record from
  `paintedFaderRailForLane` (18 px); ADR-0066 defines the record from the paint and hit-test geometry (18 px). Promoting
  the record to the thumb's 28 px rewrites ADR-0066.
- **Raise `leftRailMinWidth` without pinning records-disjoint across the user-draggable range.** The point D fix is
  one constant, but without the sweep in `[records-disjoint-matrix]` a later `leftRailMaxWidth` or scaling-matrix
  change could reintroduce an overlap at a width that was never swept.
- **Replace shell paint with a transparent tint child component.** A transparent child's repaint still repaints the
  shell beneath it; making the shell's paint clip-aware (ADR-0067 S1) is what bounds the cost. ADR-0067 already
  rejected this; the amendment preserves that.
- **Keep `juce::Component::getComponentAt` for tick resolution.** It depends on the shell being shown, which the gates'
  headless shell is not; the gates would pass in the real app and never fire in the harness.

## Follow-ups

- The fader's grab reaches the thumb's 5 px overhang while its record (and so its hover, ring and accessible element)
  is the 18 px rail: a pre-existing ADR-0066 geometry mismatch, to settle with ADR-0066's owner surfaces.

- Update `CONTEXT.md` with **Hovered control**, **Pressed control** and the ADR-0067 cp1 term **Peak since read** (the
  planner note recorded these as missing).
- A per-fill override token `Tone::hoverStrokeOnFillAlpha` is parked: ADR-0067 cp3's real-app rubric is the backstop
  for "the stroke reads on bright fills (lit solo amber, record red)". If the rubric asks for a stronger stroke on
  bright fills, the follow-up is a token addition, not a shape change.
- The agent critic recipe gains a note on `ScopedCurrentModifiers`: any future gate that must mutate the JUCE static
  wraps it in the RAII guard and asserts baseline restoration.
- Narrowing `leftRailMinWidth` 180 -> 204 is a 24 px rail-affordance trim. If a later density target wants a narrower
  rail, that is a separate ADR that must take drop-whole (ADR-0066-compatible) for the overlapping records; this
  amendment deliberately does not reserve that machinery.
