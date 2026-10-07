# 0065. Mixer strips scroll; the master is pinned at the right

- **Status:** Accepted (2026-10-08, after a separate agent critic pass; its two blockers and the funnel are resolved below)
- **Date:** 2026-10-08
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** ADR-0064 (G6.2: reachable controls at every size of the matrix; each defect found gains an assertion);
  ADR-0053 (the master pane); the plan's §3.1 sketch (strips left, buses, then Master at the right of the dock) and
  §3.4 (strip width 84 narrow / 120 wide; "Everything reachable") in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md).

## Context

Found while judging the G6.2 rubric shots: every mixer strip (tracks, then buses) and the master pane come from one
geometry law, `paintedMixerLaneBounds (i)` — the master is simply lane `n` after the last strip (N3). The strip width is
the panel's width shared out, clamped to 84–220 px (64 in narrow mode). When the strips do not fit at that minimum,
they run past the dock's right edge with nothing to scroll them: the 16-track rubric fixture at 1280×720 shows fifteen
strips and part of a sixteenth, and **no master** (its fader, meters and loudness are off the window); 24 tracks at
1920×1080 do the same. Every hit-test and paint goes through that one law, and the master fader is the dock's only
component.

## Options considered

1. **Strips scroll horizontally in whole strips; the master is pinned at the right when they overflow (chosen).** What
   Logic, Pro Tools and Cubase do: the outputs/master stay put and the channel strips scroll beside them.
2. **Shrink strips below the minimum to fit.** Unreadable past a point (16 strips at 1280 is ~70 px each, 24 at 1280
   ~46 px). Rejected.
3. **Wrap strips onto a second row.** No reference DAW does it, and it halves fader travel. Rejected.

## Decision

- **One width, as today:** every lane, the master's included, keeps the one width G4.1 computes — the panel shared
  out over `n + 1` lanes, clamped to 84–220 px, or the narrow 64 px. (The plan's §3.4 says 84 narrow / 120 wide; the
  code's 64 / 84–220 predates this ADR and is not changed here.)
- **Fit, unchanged:** when `(n + 1) × width ≤ the panel's width` — always the case when the width is the computed
  share or the 220 px cap — the layout is today's: the master right after the last strip (N3), no scroll bar.
- **Overflow:** only when the width is held at its minimum (84, or 64 narrow) and `(n + 1) × width` exceeds the panel:
  the master pane is pinned to the dock's right edge at that same width and full height; the strips get the rest (the
  strip viewport), and `v` = the whole strips that fit in it. Strips `[offset, offset + v)` are laid out in whole slots
  from the viewport's left; every other strip's lane is **empty** — not painted, not hit-tested (the existing loops
  treat an empty lane as absent). No strip is ever painted under the master or cut at the viewport's edge.
- **The offset** is a whole-strip count held by the shell for the session, clamped to `[0, n − v]` at every layout (so
  a wider window or a deleted track never leaves blank slots), not persisted.
- **Scrolling:** a horizontal scroll bar along the bottom of the strip viewport (the timeline scroll bar's thickness and
  look), shown only on overflow; the strip lanes give up that height to it, the master pane keeps the panel's full
  height. The mouse wheel over the strips moves one strip per notch (vertical or horizontal delta, as Logic's mixer
  does); over the master pane it does nothing.
- **Follow the selection, through one funnel:** the shell remembers the strip it last followed. Wherever the action
  state is refreshed (`refreshActionState`, which every selection path — the rail, the strips, keys, a new track, undo
  — already reaches), a selected strip different from the remembered one is brought into view by the least offset
  change, and remembered. A manual scroll does not change the selection, so it is never undone.
- **Editors over a strip:** a rename (track or bus) on a hidden strip first brings it into view; an editor never opens
  over an empty lane (the bus rename gains the empty-band guard the track rename has).
- **Meters keep running:** peak hold and the clip latch advance for every strip, shown or not (as today).
- Narrow strips and bus strips follow the same law; the piano roll, instrument and browser dock tabs are untouched.

### Gates (`[mixer-scroll]`)

At 1280×720, 1920×1080 and the window minimum, with 24 tracks and 2 buses:
- fit: with few tracks the master sits right after the last strip at the shared width (as N3), no scroll bar;
- overflow: the master pane's width is the strips' width, its right edge the dock panel's, its height the panel's;
  the master fader lies inside it and the window; every hidden strip's lane is empty; every shown lane is whole,
  inside the strip viewport (right edge ≤ the master's left), disjoint, its bottom plus the scroll bar's thickness at
  the panel's bottom;
- the scroll bar is shown exactly when the strips overflow; scrolled to its end the last bus's lane is whole and
  visible, at its start the first track's;
- a wheel notch over the strips moves the offset by one, over the master pane not at all; a click on a visible strip's
  painted mute cell mutes that strip's track (the offset reaches the hit-test);
- adding a track, a rail click on a hidden track and a key that selects one each bring that strip into view; a manual
  scroll away from the selected strip stays where it was put through a later action refresh;
- renaming a hidden bus brings it into view first; the editor is never over an empty lane;
- a strip whose clip latch is set keeps it after being scrolled out and back;
- with few tracks the layout is today's (master right after the last strip, no scroll bar) — the existing
  `[mixer-v2]` and master-pane gates keep passing unchanged.
- The agent judges the 1280×720 shot with 16 tracks; each defect found gains an assertion.

## Consequences

- **Positive:** the master and every strip are reachable at every size; the overflow behaves like the reference DAWs.
- **Negative / accepted costs:** an overflowing mixer loses the scroll bar's height; the offset is not remembered
  between sessions.
- **Follow-ups:** none required; a "reveal strip" on the strip context menu is not needed with selection following.

## Amendment (2026-10-08, from the implementation's visual judgment)

The decision gave the pinned master the strips' one width. Judged at 1280×720 with 16 tracks, an 84 px master pane
cannot hold its meter row: the dB scale (28 px), the fader (42 px) and the L / R meters (36 px) overlapped, the scale's
numbers under the fader. The same overlap existed before this ADR at 112 px (1920×1080, 16 tracks): the fader was
centred in a column narrower than itself. Amended:

- **The master pane is never narrower than `mixerMasterMinWidth`** (122 px: its insets, the scale, a 24 px fader and
  the meters with their gaps) nor than the strips' share. The strips share what it leaves (still clamped 84–220, or
  the narrow 64); the strips overflow, as decided, only when that width is held at its minimum.
- **The meter row is three disjoint columns** — scale, fader, meters (the meters at the right) — one law for the paint
  and the master fader's bounds.
- **Gate:** at every size, fit and overflow, the master fader lies inside its column and is at least 24 px wide, and
  the scale, fader and meters are pairwise disjoint inside the master pane.

