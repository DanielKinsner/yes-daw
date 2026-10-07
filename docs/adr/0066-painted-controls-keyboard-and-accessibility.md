# 0066. Every painted control is a Control target with its own accessible element

- **Status:** Accepted (2026-10-09, after a separate agent critic pass; its blockers resolved below)
- **Date:** 2026-10-09
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G6.3 ("The command router owns a logical control target distinct from Arrange/Piano
  roll/Mixer focus context … Expose the target, name, role, value, enabled state and supported actions to
  accessibility; clear or restore the target predictably when a control disappears or an editor closes. Gate:
  traverse and operate every visible control class, assert target/highlight/accessibility state, and verify transport
  before/after control use and text editing. No action becomes mouse-only through this change.") and the G6 exit
  ("every operable control is keyboard-reachable through the command router and represented accurately in the
  accessibility tree") in [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md);
  ADR-0049 (the Control target and its key priority — owner-settled, unchanged here); ADR-0046 (keys go to the router,
  never to widgets); ADR-0057; ADR-0063 (the ring's contrast); ADR-0064 / ADR-0065 (what is visible at a size; the
  mixer's scrolled-out strips have no parts).

## Context

G4.0b built the router's Control target (`src/ui/ControlTarget.h`, `src/ui/MainComponentControls.cpp`): Tab / Shift+Tab
walk the visible enabled **native** widgets (buttons, choosers, sliders, text fields, the browser list) in
region-then-reading order; Enter / arrows / Esc activate, adjust, keep and restore through each widget's own path;
overlays scope the walk; a screen reader's focus move adopts a target. The only **painted** control it knows is the
mixer strip's fader (`ShellControl::paintedStrip`).

Everything else the mouse can operate on a painted surface is keyboard-unreachable and invisible to accessibility
(`STATUS.md` records this as G6.3's job): the rail's per-row M / S / O cells, pan and volume, colour swatch; each
mixer strip's S / M / R cells, pan knob, send rows, insert slots and I/O rows, and the master pane's insert slots; the
timeline's seven tool cells; the header's gear and time readout; the Sampler's pads. Most have a menu or chord
elsewhere, but not per-row / per-strip (muting track 7 by keyboard means selecting it and using the menu).

Accessibility: no component overrides `createAccessibilityHandler`; native widgets use JUCE's defaults. A painted
control "speaks" by overwriting its surface's title and description (`announceControlTarget`), so a screen reader
sees one element per surface, can't enumerate the controls on it, and reads no role or state. The probe's target
reports id, name, role, value and bounds, not the enabled state, the supported actions or the drawn ring.

## Options considered

1. **One painted-control record per hit-zone; a proxy accessible element for each (chosen).**
2. **Make painted controls real JUCE components** (a button per M cell, a slider per pan knob). Hundreds of components
   at 24 tracks, undoing E25's one-geometry-law painting that keeps paint, hit-test and harness from drifting.
   Rejected.
3. **A custom accessibility handler on each surface exposing virtual children.** JUCE 8's handler tree is built from
   components; virtual children are not supported without patching JUCE. Rejected.

## Decision

### The painted-control record

One record per painted hit-zone, built by the surface that paints it, from the same geometry its paint and hit-test
read:

- **id** — the probe's layout name (`rail.row.3.mute`, `mixer.strip.12.pan`, `tool.pencil`, `header.gear` …); the
  probe lists every painted hit-zone a mouse can use (names added where missing).
- **name** — words ("Stem 3 mute", "Bus 1 send 2 level", "Pencil tool"), never a code word (ADR-0063).
- **role** — Toggle (M / S / O / R cells; the tool cells, as a radio group), Value (pan, volume, send level, the
  fader), Button (the colour swatch cycles, a meter clears its clip light, the gear shows the settings row, a filled
  insert slot opens its editor, a pad auditions with no modifier, and every zone whose click opens a menu: an I/O row,
  an empty insert slot or send well, the time readout). A menu opened by Enter is JUCE's popup with its own keys; no
  arrow previews it (the Chooser role stays the native combo's).
- **enabled**, **value text** (the readout the paint shows: "+3.0 dB", "L 12", "on"), a Value's **range**, and the
  **effect**: the same callback the mouse path calls (`onMuteToggled`, `onPanEdited`, `onMuteSoloCellClicked`,
  `onPanDragged`, `onMeterClicked`, `onColourSwatchClicked`, …), so undo, Touch / Latch rides and refusals are the
  mouse's. A Value interaction is bracketed exactly as that control's mouse drag is (the strip gesture
  `beginStripGesture` / `endStripGesture` for the mixer's painted drags, the rail's mini-drag end for its pan and
  volume), so one keyboard interaction is one undo step; Esc restores the value captured at Enter.

**The controls** (cp1's coverage list): rail rows — M, S, O, pan, volume, colour swatch, meter; mixer strips — S, M,
R, pan, fader, meter, each send row, each insert slot, each I/O row; the master pane's insert slots (its fader is a
native slider, its meters are indicators with no click); the seven tool
cells; the header's gear and time readout; the Sampler's pads. **Not records — canvas gestures with their keyboard
routes:** selecting a row or strip (Up / Down, the selection keys), renaming (Rename actions), clips, lanes and
notes (selection, nudge, the edit keys), automation breakpoints, the ruler (locate, loop, punch keys), markers
(marker keys), the loop brace, splitters and scroll bars (view keys).

**Right-click menus:** while navigating, the context-menu key and Shift+F10 open the target's right-click menu (the
same menu, e.g. an insert slot's Bypass / Remove / Move, which are in no main menu); neither chord is bound in the
keymap, and outside navigation they do nothing new.

The router's walk takes these alongside native widgets, in the same region / reading order. Only what is visible is
walked (a rail row scrolled out or a mixer strip hidden by ADR-0065 has no record).

### Accessible elements

Each record gets a **proxy**: a component that paints nothing, sits exactly on the painted rect, and carries a custom
`AccessibilityHandler` — the record's role (button, toggle button, slider), title, value interface (text, and the
numeric range for a Value), state (enabled, checked) and actions (press / toggle, and show menu where a right-click
menu exists), which run the record's effect. A proxy is **click-through** (`setInterceptsMouseClicks (false,
false)`, so the surface under it keeps every mouse gesture and ADR-0064's hit-test gate is untouched), **never takes
keyboard focus** (`setWantsKeyboardFocus (false)` and `setMouseClickGrabsKeyboardFocus (false)` — JUCE gives an
accessibility focus move the keyboard only when the component wants it, so G0.2's law holds) and carries **no
component id** (the router's widget walk and the matrix's control inventory skip it). Proxies are children of the
surface that paints them, in Tab order, created only when the record set changes and re-laid out otherwise. The
router's announcement grabs the proxy's handler focus instead of borrowing the surface's title. Native widgets keep
JUCE's handlers; any whose default misreports what the paint shows (a dB fader's value, a toggle's state) gets a
corrected handler.

### Targets that go away

When the target's control disappears (its track is deleted, its strip scrolls out, its section drops), the existing
`ControlNavigator::revalidate` law applies unchanged: the target becomes the control now at the vanished one's Tab
index (clamped to the walk), else navigation ends; an editor or overlay closing restores the target that opened it
(as G4.0b does). The probe reports the target's enabled state, its actions, the ring's rect and a proxy-creation
counter.

### Checkpoints and gates (`[g6-keyboard]`)

- **cp1 — painted controls are targets.** Gates: coverage — every zone of the list above that the probe names on screen
  is in the Tab walk (no action mouse-only); operate — for each role, a representative of each surface: Enter on a
  toggle changes the model as a click does; a Value: Enter, arrows change the model value, Esc restores it exactly,
  Enter keeps it as one undo step (undo once restores the start); a Button performs the click's effect (a menu zone
  records its headless menu); the context-menu key opens the target's right-click menu (an insert slot's Bypass then
  works); transport — Space toggles play in each state {before Tab, navigating, interacting on each role, after Esc,
  after Enter} and goes to a text field while it is editing.
- **cp2 — accessible elements.** Gates: for every target in the walk, its accessibility handler (native or proxy)
  exists; role, title, value text, a Value's numeric range and enabled / checked state match the target's; its actions
  include the role's; invoking the handler's action does what Enter does (model compared); the router's move gives
  the handler focus and the shell keeps keyboard focus; every proxy is click-through, id-less and not focusable; 50
  action refreshes with no layout change create no proxy.
- **cp3 — clear and restore, and the whole shell.** Gates: deleting the targeted track, scrolling the targeted strip
  out and dropping a targeted section each move or clear the target as decided; closing the FX editor, the keymap
  editor and the New Project dialog restores the target; a representative full-shell walk at 1920×1080 with the
  16-track fixture and a clip selected reaches every control class (cross-size reach is ADR-0064's matrix gate), and
  the ring's contrast holds (ADR-0063, ≥ 3:1).

## Consequences

- **Positive:** every control the mouse operates is reachable by keyboard and named, typed and stated for a screen
  reader; the probe and the drives can address every painted control by name.
- **Negative / accepted costs:** proxy components (pooled; about 600 at 24 tracks with the mixer shown) and their
  layout; a longer Tab walk on large sessions (regions and selection-follow keep it navigable); the painted surfaces
  gain a provider each.
- **Follow-ups:** a real screen-reader drive (NVDA / Narrator) when the desktop is free; type-ahead or region-jump
  keys if the walk proves long in dogfooding.
