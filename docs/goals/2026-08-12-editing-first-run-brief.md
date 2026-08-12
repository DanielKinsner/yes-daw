# Editing-first parity run — operating brief (2026-08-12)

This brief directs the run started by `/goal` on 2026-08-12. It extends the completed
shortcut run's manual — read `docs/goals/2026-08-11-overnight-backlog-run-brief.md` FIRST and
keep every process rule in it: one item at a time, strict order, audit-before-build,
shipped-boundary gates that fail before and pass after, full local ctest with the owner-file
isolation ritual, one feature commit + exact-head nine-job CI green + separate docs-only
evidence commit per item, never edit ADRs / goldens / `[[clang::nonblocking]]` annotations /
`.github/workflows/ci.yml`, never weaken or delete an existing gate, stop after 3 consecutive
red CI rounds on one item and record honest state in `STATUS.md`.

## Visual judgment is yours

You have full permission to use every tool available (Dan's standing mandate, 2026-08-12).
For UI work, build and launch the real app, take screenshots at real window sizes, and judge
the result yourself against Pro Tools / Logic-class quality: layout, alignment, spacing,
readability, clipping, theming consistency, whether interactions feel like a professional
DAW. Iterate on what looks wrong until it looks legit — do not defer visual calls to Dan.
Then lock every visual fix with a mechanical gate (token/layout assertions) so it can never
regress: judgment finds the defect, the gate keeps it fixed.

## Phase 1 — re-audit and carve an editing-first backlog

`docs/reviews/2026-08-09-shipped-parity-gap-audit.md` ranks the parity gaps, but the B1–B25
and B26–B41 backlogs have landed since it was written — re-verify every item against current
`main` by auditing the real code paths before carving. Write a new numbered backlog doc in
`docs/goals/` in the same format as `2026-08-10-shortcut-and-workflow-parity-backlog.md`,
ordered by these priorities:

1. **EDITING TOOLS to Pro Tools / Logic class** — everything needed to edit a real
   MULTI-TRACK song, not a single lane. Multi-track arrangement is first-class: import to
   the selected track, vertical clip drag between tracks, stacking/layering clips across
   many tracks, multi-clip selection spanning tracks (rubber-band + modifier clicks), and
   group move/copy/delete of that selection. Full clip editing (trim both edges, split,
   move across tracks, copy/paste/duplicate, delete, fades/crossfades from the UI, clip
   gain), full piano-roll editing (pencil add/delete, lengths, velocity, selection tools),
   a working tool palette (pointer/pencil/scissors/etc. actually switching behavior),
   timeline zoom + scroll, snap toggle + grid picker actually consulted by every drag,
   loop region, markers. Gates must prove behavior on multi-track projects (3+ tracks),
   not just track 0.
2. **FX TOOLS** — the full insert workflow from the UI: add/remove/reorder FX with a chooser
   panel, edit every parameter, bypass, per-strip sends create/route/level, bus
   create/delete/rename, and a real automation lane canvas (click-to-add, drag points,
   per-track targeting).
3. **HARDENING + VISUAL SWEEP of everything already shipped** — audit each shipped feature
   for honest gaps, functional and visual. Functional: missing undo coverage (direct strip
   edits bypass the undo stack), edge cases with no gate, controls that work on track 0 but
   not every track/strip/slot. Visual: screenshot every view (Timeline, Mixer, Piano roll,
   automation) at multiple real window sizes, judge them yourself, and fix what a Pro Tools
   user would call amateur — collisions, misalignment, inconsistent tokens, cramped or
   clipped text. Every fix still lands with a mechanical gate.
4. **RECORDING last** — wire real device recording behind the shipped Record button using
   the proven pipeline in `tools/hardware/RecordingCheckMain.cpp` +
   `app/RecordingAssetCommit.h`. CI-provable parts first; the real-hardware part ends as a
   one-command self-asserting PASS/FAIL script for Dan, never "listen and check."

Each item must be independently committable with honest scope only, plus an explicit
"out of scope — do not fake" section. Commit the backlog doc (docs-only) and certify it.

## Phase 2 — execute

Work the new backlog strictly top-to-bottom under the rules above. DO NOT stop after
carving, after any single item, or at any phase boundary — keep working item after item
until the backlog is complete or a hard-stop condition is hit (3 consecutive red CI rounds
on one item, or a decision only Dan can make — record it in `STATUS.md` and move to the next
item if it is independent). Whenever you do stop, leave `STATUS.md`'s Now/Next accurate and
everything committed and pushed.
