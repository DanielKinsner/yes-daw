# 0073. Empty states offer the next action by its live chord; one dismissable first-run tip

- **Status:** Accepted (agent, 2026-10-08, under ADR-0049's implementation-ADR clause - it implements the plan's G6.5
  and changes no owner-settled contract - after a design workflow (three proposals, two judges, a synthesis) and a
  separate agent critic; its two blockers and five should-fixes are resolved below)
- **Date:** 2026-10-08
- **Deciders:** build agent (proposer), a design workflow, separate agent critic
- **Related:** the plan's G6.5 ("Empty states and first-run tips. Explain the next available action using its actual
  current chord (for example, import audio); dismiss tips and preserve normal keyboard/mouse use. Gate: empty-project
  and missing-selection paths expose valid actions, with no obscured controls.") and the G6 exit in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md); ADR-0046 (keys
  go to the Command router); ADR-0049 (the Control target; Space stays transport); ADR-0061 (prefs.json,
  add-and-deprecate); ADR-0063 (the tooltip chord style `Play  (Space)`, contrast); ADR-0066 (every painted control is
  a record, a Control target and its own accessible element); ADR-0067 / ADR-0072 (hover and pressed on records);
  ADR-0068 (the autosave recovery card).

## Context

Every empty surface paints inert prose today - no record, no Control target, nothing in the accessibility tree, no
action and no chord:

- the rail with no project or no tracks: "No Project" (`src/ui/MainComponentArrange.cpp:1151-1157`);
- the inspector's Track tab with no selected track: "No track" (`src/ui/MainComponentInspector.cpp:815`);
- the inspector's Clip tab with no clip: "No clip selected" (`:948`);
- the piano roll with no MIDI clip: "No MIDI Clip selected" (`src/ui/MainComponentPianoRoll.cpp:69`);
- the browser with nothing to list: "Nothing here" (`src/ui/BrowserPanelComponent.h:251`).

Launch reopens the last project or an Untitled session (a loaded project with no tracks). There is no first-run
guidance. Dan's dogfood verdict (2026-09-01) called the app "confusing"; an empty window that names no next step is
part of that. No test or drive asserts the old strings.

The pieces exist: `Keymap::chordFor` and `Keymap::actionForChord` (`src/ui/UiActions.h:1287, 1323, 1345`, aliases and
the global fallback included) are the router's own lookup; `menuShortcutFor` (`src/ui/MainComponentCommands.cpp:1253`)
already asks it whether a chord works in the current focus; the menu bar is data (`getMenuBarNames`,
`menuActionsForIndex`, `:619-732`) and a menu item's text is its descriptor's label (`:1268-1273`);
`collectPaintedControls` turns a painted zone into a record, a Control target and an accessible element; `prefs.json`
round-trips sections.

**Reference DAWs.** Logic routes an empty project through its New Tracks dialog and paints no prose in the Arrange;
Pro Tools, Cubase and Reaper leave empty windows bare (a double-click or a menu adds a track); Ableton labels a drop
zone and opens a dismissable Learn view on first launch. None bakes a shortcut into empty-state copy. The plan's "its
actual current chord" asks one thing more; this ADR gives it in the menu-entry form a DAW user already reads
(`Add audio track  (Ctrl+Shift+N)`), one short row per honest next action - no headlines, no cards.

## Decision

### 1. An empty surface offers its next action as a row

Where a surface is empty, or lacks the selection it needs, and one action is the honest next step, the inert label is
replaced by a **row**: one line `Noun  (how)` that is a painted control - a record with an `empty.<surface>.<action>`
id, role Button, a Control target (Tab reaches it, Enter activates it; Space stays transport, ADR-0049), its own
accessible element, ADR-0072's hover and pressed strokes, and a click that runs the action through the router exactly
as its menu item does. The noun is the row's copy as the table below fixes it.

**Enabled.** `PaintedControl` gains `std::function<bool()> enabled` (absent: enabled); a row's reads
`registry().stateFor (action, context).enabled`. A disabled row paints in the muted text token, its activation does
nothing, and its accessible element is disabled (the proxy component follows the record).

### 2. "How" is the chord that works here, else the menu path

`how` is resolved each paint by one helper (`src/ui/EmptyStateRow.h`):

1. the action's chord (`chordFor`), **if the router, asked with the current focus context, maps that chord to this
   action** (`actionForChord (chord, focus) == action`) - shown as `Ctrl+Shift+N`;
2. else the menu path from the menu bar's data - `Clip > Add MIDI Clip` - the top-level name (`getMenuBarNames`) of the
   menu whose `menuActionsForIndex` holds the action, then the action's label;
3. else nothing: the row is the noun alone. Never empty parentheses, never a placeholder.

A rebind or an unbind shows on the next state refresh (the path tooltips and menus take). The accessible element's
model gains a **description** (`PaintedAccessibleProxy::Model::description`, reported through the handler's
`getHelp()`), set to `how`, so a screen reader hears "Add audio track, button" and then the chord or path.

### 3. The rows

| Surface and state | Row id | Text (default keymap) | Action |
|---|---|---|---|
| Arrange lanes, no project | `empty.arrange.new_project` | `New project  (Ctrl+N)` | `ProjectNew` |
| Arrange lanes, no project | `empty.arrange.open_project` | `Open project  (Ctrl+O)` | `ProjectOpen` |
| Arrange lanes, a project with no tracks | `empty.arrange.add_track` | `Add audio track  (Ctrl+Shift+N)` | `TrackAdd` |
| Arrange lanes, a project with no tracks | `empty.arrange.import_audio` | `Import audio  (Ctrl+Shift+I)` | `ProjectImportAudio` |
| Inspector Track tab, tracks but none selected | `empty.inspector.select_track` | `Select a track  (Down)` | `TrackSelectNext` |
| Inspector Clip tab, a track selected, no clip | `empty.inspector.add_midi_clip` | `Add MIDI clip  (Clip > Add MIDI Clip)` | `TimelineMidiClipAdd` |
| Inspector Clip tab, tracks but none selected | `empty.inspector.select_track` | as above | `TrackSelectNext` |
| Piano roll, a track selected, no MIDI clip | `empty.pianoroll.add_midi_clip` | as the Clip tab's | `TimelineMidiClipAdd` |
| Piano roll, tracks but none selected | `empty.pianoroll.select_track` | `Select a track  (Track > Next Track)` | `TrackSelectNext` |

(The piano roll's own focus maps Down to its notes, so the resolver shows the menu path there - the law at work.)

The Arrange rows sit centred in the empty lanes, one under the other - where content will appear and where a dropped
file lands; the rail's "No Project" label goes (an empty rail). States with no tracks at all keep plain text in the
inspector and piano roll ("No tracks yet"): the Arrange rows, always on screen, carry the next step. Kept as they are,
by name: the **mixer** in an empty project (its master strip is there; the Arrange rows carry the step), the
**automation lane**'s "No Track automation" (a name, not an empty state), the **instrument panel**'s kind line "No
track" (a readout beside its chooser), and the **browser**'s empty list, which becomes "No files here" (its Up button
is beside it; no single action is honest there).

### 4. One first-run tip, dismissable, persisted

Exactly one tip: a one-line strip just under the Arrange ruler, overlaying the top of the empty lanes as the autosave
recovery card does (ADR-0068: a child raised over the lanes), never shown with it (the card wins):

> `Tip: drop audio files onto the arrange to import them, or open the browser  (Y)`  `[Dismiss]`

It teaches what the rows do not (drag and drop, the browser); its chord is resolved by §2 (`ViewBrowser`). The sentence
is not a control; **Dismiss** is a row (`tip.welcome.dismiss`, role Button, Tab, Enter, click). It claims no chord: Esc
keeps its meanings.

- **Shows** while `prefs.tips.dismissed` lacks `"welcome"`, no autosave recovery question is pending, and the lanes are
  empty (no project, or a project with no tracks) - so it covers no control; the Arrange rows are laid out below it.
- **Goes for good** on Dismiss, or the first time a track appears (added, imported, dropped, or a project with tracks
  opened): `"welcome"` is added to `prefs.tips.dismissed` and written.
- **Comes back** through **Help > Show Tips Again**: a new `UiActionId::HelpShowTipsAgain` (at the end of the enum and
  the descriptor table, unbound, in the Help menu after Keyboard Shortcuts, enabled while any tip is dismissed) empties
  the list.
- `prefs.json` gains `"tips": { "dismissed": [ ... ] }` (ADR-0061: a missing section reads as empty; unknown ids are kept
  on write). The headless harness and the session drives start with `"welcome"` dismissed unless a step clears it, so
  no earlier gate or drive sees a new strip (the shell's child-count pin included).

### 5. Nothing is obscured

A row is laid out inside its surface's empty area and is dropped whole (no paint, no record) when it does not fit; the
tip shows only over empty lanes. No row, the strip or its Dismiss may intersect any other control - record or widget -
at any size of the scaling matrix.

## Gates

Each self-asserting; each red without its law.

- **`[g65][empty-rows]` presence.** For every state in the table: exactly the expected `empty.*` records, role Button,
  inside their surface, with the table's text under the default keymap; a render has none of the five old strings.
- **`[g65][empty-rows][chord]` the shown chord works.** For every row whose `how` is a chord, pressing that chord
  through the router from the current focus dispatches the row's action. Rebind `TrackAdd` to F4: the row reads
  `Add audio track  (F4)` after the refresh; unbind it: `Add audio track  (Track > Add Track)`. In the piano roll the
  select-track row shows the path, not Down.
- **`[g65][empty-rows][menu-path]` the path is the menu's.** For every row whose `how` is a menu path, the named
  top-level menu, built as the menu bar builds it, holds an item with that text whose action is the row's.
- **`[g65][empty-rows][keyboard]` Tab and Enter.** The control traversal in each empty state is pinned: the Arrange
  rows follow the timeline toolbar's controls in the arrange region, in the table's order; the inspector's row follows
  its tab buttons. Enter on `empty.arrange.add_track` adds a track (the rows give way to the lanes); Enter on
  `empty.arrange.import_audio` imports the fixture WAV; Enter on a disabled row changes nothing.
- **`[g65][empty-rows][accessibility]`.** Each row's element: role Button, the noun as its title, `how` as its help
  text, disabled exactly when the action is.
- **`[g65][no-obscured]`.** At every size of the scaling matrix, in every empty state and with the tip showing: no row,
  strip or Dismiss intersects any other record or visible widget; a row that does not fit is absent, not clipped.
- **`[g65][tip]` lifecycle.** With `"welcome"` not dismissed: the strip shows on empty lanes; Dismiss hides it and
  `prefs.json` lists `"welcome"`; a relaunch shows none; Show Tips Again brings it back; adding a track hides it for
  good; with the recovery question pending the card shows and the strip and its Dismiss are absent. A `prefs.json`
  without `tips`, or with an unknown id, reads and writes back without loss.
- **`[g65][contrast]`.** Row text (enabled and disabled) and tip text meet ADR-0063's 4.5:1 on their surfaces.
- **Real app (`ss1`).** A first-minute step with the tip enabled: the strip and both Arrange rows show; the real
  keyboard Tabs to `empty.arrange.add_track` and presses Enter; a track appears and the strip is gone; shots at the
  three plan sizes for the rubric.

## Checkpoints

Each green on its own; STATUS, commit, push, CI.

- **cp1** - `EmptyStateRow.h` (the `how` resolver against the router and the menu data), `PaintedControl::enabled`,
  the proxy's description; `[chord]` and `[menu-path]` on the resolver.
- **cp2** - the Arrange rows (no project, no tracks); presence, keyboard, accessibility, no-obscured, contrast.
- **cp3** - the inspector and piano-roll rows and the plain-text states.
- **cp4** - `prefs.tips`, the tip strip, Dismiss, `HelpShowTipsAgain`; `[tip]`, no-obscured with the strip.
- **cp5** - the `ss1` real-app step.

## Alternatives rejected

- **Cards with a headline and several buttons on every empty surface.** Reads as a tutorial each time a project is
  closed; no reference DAW paints prose and buttons there.
- **A permanent help band (Quick Help style).** Takes lane height on every launch; no reference DAW keeps one.
- **Esc dismisses the tip.** Esc already ends control navigation and closes overlays; a strip that steals it while
  visible makes Esc mean two things.
- **Open the New Project dialog on first launch.** A launch-flow change beyond G6.5 (launch reopens the last project or
  an Untitled session, G5); not decided here.
- **Raw `chordFor` for the shown chord.** It can name a chord that does nothing where the row is (Down in the piano
  roll); the plan asks for the chord that works.
- **Hand-written menu hints.** One was already wrong in a draft (`Add MIDI Clip` is in the Clip menu, not Track); the
  path comes from the menu bar's data.
- **Rows on the rail where "No Project" was.** The rail is 204 px at its narrowest; two rows with chords do not fit,
  and the lanes are where the work will be.
- **Dismissable rows.** Hiding the only actionable thing on an empty surface brings back "stuck".

## Consequences

- An empty window always names its next step and how to take it, by keyboard or menu, live with the keymap; every row
  is a real control (Tab, Enter, a screen reader, hover and pressed).
- A new user sees one tip, once; it comes back only when asked for.
- `PaintedControl` gains `enabled`, the painted accessible element gains a description, `prefs.json` gains `tips`, the
  Help menu gains Show Tips Again.
- Follow-ups: a default chord for `TimelineMidiClipAdd` would turn its rows' path into a chord with no code change; a
  browser "add a location" action would let the browser's empty list take a row under this ADR's shape. CONTEXT.md
  gains **Empty-state row** and **Tip**.
