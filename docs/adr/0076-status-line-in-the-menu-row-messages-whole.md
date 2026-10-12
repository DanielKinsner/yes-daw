# 0076. The status line moves to the menu row and shows its messages whole

- **Status:** Accepted (agent, 2026-10-12, under ADR-0049's implementation-ADR clause - it amends an agent-accepted
  ADR's row list and changes no owner-settled contract - after a separate agent critic. The critic ruled that middle
  elision of user or OS text does not conflict with plan §7.4 line 1, on two conditions now in the Decision: every string
  the app writes in full shows whole at 1280, and a shortened message never paints something untrue)
- **Date:** 2026-10-12
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** amends [ADR-0064](0064-layout-density-and-scaling.md)'s timeline tool-row list (the status line leaves
  the row) without editing ADR-0064; the UI correctness pass, unit 11 (rubric FIX 13); plan §7.4 lines 1, 3 and 4;
  [ADR-0063](0063-tokens-contrast-readable-labels.md) (text tokens and the text floor); [ADR-0061](0061-persistent-preferences.md)
  (the device messages).

## Context

The status line carries every failure message (save, export, project create, autosave, the audio device) and every
hover and mode hint. ADR-0064 placed it on the timeline's tool row, after the snap-mode, edit-mode and nudge choosers
and before the view cluster, with an 80 px minimum (`statusLineMinWidth`) below which it drops. In the rubric shots it
was squashed: at 1280 the box was about 133 px against a 228 px Pan hint, and the label's default minimum horizontal
scale let JUCE compress the glyphs toward unreadable before truncating them. Messages that embed a device name or a
path cannot fit any box at 1280 by width alone.

The menu-bar row has free room: the nine menus sit in `headerMenuBarBounds()` = {8, 2, 680, 24} and nothing else is in
that 28 px row, so its right side is empty from x 688 to the window's edge - about 570 px at 1280 and about 1210 px at
1920. Using it costs no height, so ADR-0064's eight-lanes-at-1080p density holds.

What is hard to reverse: where every message and hint appears is a habit users learn, and the text budget it sets
binds every future hint.

## Options considered

1. **Keep the line on the tool row, widen it by dropping choosers earlier.** No move, but the box is still a few
   hundred pixels at 1280 and competes with the choosers ADR-0064 placed there; long user text still cannot fit.
2. **The empty right side of the menu row (chosen).** No height cost, the widest box available at every size, and
   the tool row's choosers get their room back.
3. **Two 13 px lines in the full 28 px menu row.** Holds longer text, but two-line messages read badly and 13 px lines
   leave no leading at the 11 px text floor's line height.
4. **A 20 px status bar at the window bottom.** The classic place, but it costs 20 px of height and breaks the
   eight-lanes-at-1080p gate.

## Decision

Option 2.

- **Placement.** The status line sits in the menu row: from `statusLineMenuBarGap` (12 px) right of the menu bar's
  right edge (x 688) to `statusLineMenuRightInset` (8 px) from the window's right edge, at the menu bar's own y 2 and
  height 24 - one line. It is part of the header layout (`HeaderLayout`, and the probe's header rects, so the layout
  matrix's inside-the-header, no-overlap gate covers it at every size and scale). It has its own tokens: the tool row's
  `statusLineLeftGap` and `statusLineRightInset` stay with the choosers and the view cluster they still place.
- **The box and the budget.** Box = W − 708 px; text area = box − 10 (the label's 5 px border each side); budget = text
  area − 2 px spare. At 1152, 1280 and 1920: box 444 / 572 / 1212, budget 432 / 560 / 1200.
- **It leaves the tool row.** `statusLineMinWidth` and the tool row's `statusReserve` go; the snap-mode, edit-mode and
  nudge choosers drop only for their own and the view cluster's room. The chooser placement and its row anchor (renamed
  `toolRowTailBounds`) stay; the harness's tool-row list loses "status". The tool row's tail is then empty from the last
  shown chooser to the view cluster (about 126 px at 1280x720, 802 px at 2560): not a dead region under §7.4 line 1 - an
  idle status line looked the same and passed, and the reference has about 400 px of empty tool row.
- **Never squashed.** `setMinimumHorizontalScale (1.0f)`: glyphs keep their width; text that does not fit is shortened
  only by the rule below, never compressed.
- **Fixed text** is every string the app writes in full: hover hints, mode hints, and every status message with no user
  or OS field. Each fits the 1280 budget (560 px) on every platform's font - Segoe UI Variable, its Verdana fallback,
  and the Linux sans - with the longest variant of any that takes app values (numbers, key names, the lane choice). A
  gate sweeps the hints and the message catalogue.
- **User and OS text** (device names, file names, paths, joined refusals) is shortened by one pure function with unit
  cases:
  - a message's "; "-joined parts are never mixed: as many leading whole parts as fit are shown, then "; … N more";
  - when even the first part does not fit, that part alone is shortened in its middle (its lead and its end kept);
  - shortening is by code point, with U+2026, measured by the routine that paints, keeping the 2 px spare.
  It happens only in the paint: the label's text, the probe and accessibility keep the full text. While shortened, the
  tooltip is the full text, the label takes the mouse so the tooltip can show (it never takes keyboard focus), and the
  message's decay pauses while the pointer is over it.
- **From 1152 to 1279 px wide** (below the rubric's 1280 floor) fixed texts may take the shortening path too. A hover
  hint's full text is then out of reach (the hint clears when the pointer leaves its zone): an accepted cost below the
  floor.
- **Contrast.** `dangerRed` and `mutedText` on both ends of the header's gradient (`panelRaised` to `canvasLayer`) are
  in the contrast table at ADR-0063's 4.5:1.
- **Layout pins.** Leaving the tool row frees room there, so the layout matrix's dropped-control pins change where the
  arithmetic says: at 1706x896 `clip.inspector.markers timeline.nudge.chooser`; at 1536x793 the fades plus
  `clip.inspector.markers timeline.edit_mode.chooser timeline.nudge.chooser`; every other cell unchanged.

## Consequences

- Messages and hints get a box several times wider, and nothing is squashed.
- The tool row is controls only; ADR-0064's row-containment gate no longer lists the status line.
- Every new fixed hint must fit the 1280 budget; the sweep gate fails a hint that does not.
- Long user or OS text is shortened without mixing a message's parts, with its full text one hover away, and in the
  probe and accessibility.
- ADR-0063's tooltip code-word check exempts the status line (its tooltip is the message, which may name a file).
