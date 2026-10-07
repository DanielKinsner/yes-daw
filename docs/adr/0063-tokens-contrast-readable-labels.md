# 0063. Tokens, contrast and readable labels: the plan's type scale with an 11 px floor, WCAG contrast for every token where it is drawn, colours defined once, no fake data, tooltips in words

- **Status:** Accepted (agent, 2026-10-08, under ADR-0049's implementation-ADR clause, after a separate agent critic
  pass whose findings are folded in: text on solid fills (the rail's lit cells) gets its own ink; every colour token
  is classified and the table cannot go stale; alpha text is retired; the type scale meets §3.4's 12 px base; the
  mixer rows that grow are named; the audit pattern, the shared colour header and the rubric rows' owners are pinned.
  Committed alone, before any G6.1 code.)
- **Date:** 2026-10-08
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G6.1 ("Tokens and icons. One colour/type/spacing system and icon set, including readable
  labels and tooltips. Gate: tokens, contrast/size assertions and agent rubric; no fake data or dead affordances."),
  §3.4 ("Base UI font 12 px; labels 11 px") and §7.4 (the rubric: text ≥ 11 px with readable contrast; a label or icon
  plus a tooltip on every control; no fake data) in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md); ADR-0046 (the
  feel-first shell: rubric and token gates); ADR-0049 (the Control target; the letter cluster's chords); the H16 theme
  audit (`tests/theme_audit_tests.cpp`); the 2026-08-08 UI audit's M1 (type scale).

## Context

The shell already has one token system (`UiTheme`: colours, type, spacing, radii, layout), an icon set
(`UiIcons.h`, which covers every toolbar action — the "|<" text is a fallback never shown), a theme audit that rejects
raw colours, sizes and geometry in `src/ui`, a tooltip gate and a no-dead-affordance gate. What G6.1 still lacks:

- **Type below the plan.** `Type::tiny` is 9 px and `caption` 10.5 px (eight files; the rail's cells and MIX label, the
  mixer's send / insert / I/O rows, the master meter's scale); the base UI text `small` is 11.5 px where §3.4 says 12;
  nothing asserts a floor.
- **No real contrast gate.** The one check (screenshot V1) uses a non-gamma luminance and a 3:1 floor on two rendered
  samples. Measured with WCAG 2.x relative luminance: every text token passes 4.5:1 on the dark surfaces except
  `faintText` (2.9–3.5:1, two labels and the fader ticks); and the rail's lit S / M / O cells draw light text on a
  **solid** track colour or the record red — 2.3–3.4:1. Two labels draw text with alpha (the drum-mode shelf label,
  the delay taps' dimmed label).
- **Colours defined twice.** The five track accents are repeated as raw numbers (the audit's pattern misses a trailing
  `u`), and `Main.cpp`'s window background (unaudited) does not match `appBackground`.
- **Fake data.** `Tone::inspectorAutomationValues` is an unused, fake-looking array.
- **Tooltips in code words.** The toolbar's tooltips are built from the action's stable id ("transport.play  Space").

What is hard to reverse: the type scale and the contrast rules become gates every later surface must meet.

## Options considered

1. **Tighten the existing tokens and gate them (chosen).** The plan's type scale, contrast asserted over a complete
   classification of the colour tokens, one place per colour, tooltips in words.
2. **A new semantic token layer (roles over a palette, light-theme ready).** Right for a themable product, but a large
   rename for no visible G6.1 gain; no light theme is planned. Parked.
3. **Contrast measured only on rendered screenshots.** Closest to the eye, but anti-aliasing and sampling make it
   unstable and it sees only what a shot shows. Kept as a second net (V1, extended); the token table is the gate.

## Decision

### Checkpoint 1 — the type scale and contrast

- **The type scale is the plan's**: labels **11 px** (`tiny` and `caption` become 11), base UI text **12 px**
  (`small` becomes 12); `body` (13) and the larger sizes are unchanged. A gate asserts every Type token ≥ 11 and
  `small` = 12 (the audit already forbids raw sizes, so the tokens are the only source).
- **Rows that draw text at the floor are at least 15 px** (11 px type and 2 px each side): the mixer's send row grows
  from 13 to 15 px (the insert and I/O rows are already 15); any other row the shots show clipping grows the same way.
  Gates that pin these numbers read the tokens; they are re-pinned to the new values, never relaxed. Text that no
  longer fits sideways is fixed in layout (wider box, fewer characters by design), never by a smaller size.
- **Every `UiTheme::Color` token is classified** in the gate as **text**, **surface**, **fill** (a solid colour that
  text or marks are drawn on, e.g. a lit cell), **indicator** (a mark, line or meter that carries meaning) or
  **decoration**. The gate reads the token names from `UiTheme.h` and fails on any token it has not classified, so the
  table cannot go stale.
- **WCAG 2.x contrast, gamma-correct**, over the declared pairs:
  - **text ≥ 4.5:1** — each text token against every surface or fill it is drawn on. The surfaces include
    `appBackground`, `panel`, `panelRaised`, `timelineCanvas`, `timelineToolbar`, `timelineRuler`, `canvasLayer`,
    `buttonSurface`, `toolButton`, `darkControl`, `warningButton`, `inspectorTab`, `mixerBack`, `controlInset`,
    `controlInsetBlack`, `selectedLane`, `selectedStrip`, `knobFace`; a clip's name is asserted against its body as
    painted (the accent at the clip fill's alpha over the canvas, and at the body's brighter top).
  - **text on fills**: text drawn on a solid track accent, the record red or the solo amber uses a new dark ink token,
    **`textOnFill`** (≥ 4.5:1 against every such fill); the rail's lit cells take it.
  - **indicators ≥ 3:1** against the surfaces they sit on (the focus ring and the Control-target ring, the meter
    colours and clip fill, the fader thumb on its track, the MIDI lamp lit and off, the loaded and empty Sampler pads,
    the record / solo / danger colours, the new **`scaleTick`**).
  - Decoration (grid lines, inner highlights, shadows) and disabled controls (WCAG's inactive-component exception) are
    listed as exempt.
- **Text is drawn in opaque tokens.** The audit rejects `withAlpha` on a text token; the drum-mode shelf label and the
  delay taps' dimmed label take opaque tokens.
- **`faintText` is retired as a text colour**: the rail's MIX label and the master meter's scale numbers take
  `mutedText`, the fader's ticks take `scaleTick`; `faintText` is removed.
- **The rendered net (V1) is extended**: besides the time readout and a track name, it samples a clip's name on its
  body and a lit rail cell, measured with the WCAG formula; its floor stays 3:1 (anti-aliased pixels cannot be held to
  4.5 by sampling — the token table holds 4.5).
- **The agent judges** the rubric shots (1280×720, 1920×1080, 2560×1440); each clipping found gains a mechanical
  assertion. The rubric's "truncated Scale / fade / master labels" row closes here.

### Checkpoint 2 — colours once, no fake data, labels and tooltips in words

- **Every colour is defined once.** The five track accents and the window background move to one JUCE-free header,
  **`src/ui/UiColourValues.h`** (ARGB constants), read by `UiTheme::Color`, by the JUCE-free model code that names
  track colours, and by `Main.cpp`. The audit's raw-colour pattern becomes `\b0x[0-9A-Fa-f]{8}[uU]?\b` outside the
  token headers, and the audit covers `src/Main.cpp`.
- **No fake data.** `Tone::inspectorAutomationValues` is removed, and the audit rejects a `Tone` array no code reads.
- **Tooltips in words.** Every toolbar button's tooltip is its action's accessible name and live chord, as every other
  control's ("Play  (Space)"); the tooltip gate also rejects a tooltip containing a stable id (a dotted lower-case word
  such as `transport.play`). The rubric's "unlabeled gain slider" row closes here: that control gets a visible label.
- **The letter cluster stays.** The narrow view buttons (I, M, P, A) are the plan's G2.1 design (each letter is its
  own chord); each tooltip names its view.

### Not here

- Window and display scaling (125 / 150 / 200 %) and the eight-tracks-at-1080p density row are **G6.2**.

### Gates (`[tokens]`)

- cp1: the Type scale (≥ 11, base 12); the classification is complete and every declared pair passes (it fails on
  today's `faintText` and the lit rail cells); no `withAlpha` on a text token; V1 extended; each clipping the shots show
  has an assertion.
- cp2: no raw colour in `src/ui` or `src/Main.cpp` outside the token headers (the new pattern bites the five accents and
  the window background today); no unread `Tone` array; every toolbar tooltip equals its accessible name and chord; no
  tooltip contains a stable id; the gain slider is labelled.

## Consequences

- **Positive:** every text the user reads is at least 11 px with WCAG AA contrast where it is actually drawn, the lit
  cells read clearly, and a new colour cannot slip in unclassified; one place per colour; tooltips read as words.
- **Negative / accepted costs:** tight layouts grow (the mixer's send row, the master meter's scale, the rail's MIX
  label); the faint third text level is gone (two text levels plus disabled); lit cells change from light to dark text;
  no light theme.
- **Follow-ups:** G6.2 builds on the scale; V1 stays as the rendered second net.
