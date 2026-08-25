# Reality run — operating brief (2026-08-25)

This brief directs the run started 2026-08-25. It extends the proven manual — read
`docs/goals/2026-08-11-overnight-backlog-run-brief.md` FIRST (the loop, machine specifics,
known traps, hard stops) and `docs/goals/2026-08-12-editing-first-run-brief.md` (visual
judgment mandate) and keep every process rule in them: one item at a time, strict order,
audit-before-build, shipped-boundary gates that fail before and pass after, full local ctest
with the owner-file isolation ritual, one feature commit + exact-head nine-job CI green +
separate docs-only evidence commit per item, never edit ADRs / goldens /
`[[clang::nonblocking]]` annotations / `.github/workflows/ci.yml`, never weaken or delete an
existing gate, stop after 3 consecutive red CI rounds on one item and record honest state in
`STATUS.md`.

## Context for this run

Dan cannot dogfood-test right now — the pre-authorized fallback (STATUS.md, "a fresh
adversarial audit carve instead") was exercised on 2026-08-25: four parallel adversarial
code-path audits of head `71827dc` produced
`docs/goals/2026-08-25-reality-run-backlog.md` — **R1–R34 in five phases**. Dan ordered R1
(the edge-zone bug) to the top. Work the backlog strictly top-to-bottom.

The theme is reality: the fixture-shaped happy path is solid and gated; this run makes the
app survive a real song and a real user. Two findings are architectural, not cosmetic — R2
(the transport must survive edits) and R12 (live scalar edits without an engine rebuild) —
treat their audits with extra care and keep their slices honest and minimal as specced.

## Rules of engagement specific to this run

- **The backlog's per-item evidence is a map, not gospel.** File:line references were
  verified on 2026-08-25 at head `71827dc`; re-verify during each item's audit step — the
  head will have moved by the time you get there.
- **R4 (status surface) is load-bearing**: R5, R6, R7, R8, R25, R31, R34 all paint through
  it. Build it once, well, with a deterministic timer law (`serviceMainComponentUiTimer`,
  never wall-clock).
- **Schema bumps** (R3, R11, R13): additive only, migration gate per the locate-points
  pattern. Current schema version is found in `src/persistence/` — audit before bumping.
- **A decision only Dan can make**: record it in STATUS.md and move to the next item if
  independent. Two known candidates: R17 (refuse vs scroll the 5th send — recommend refuse,
  it is smaller and honest) and R29's CC persistence question (capture notes only if the
  schema fight is big — say so honestly).
- **Do not touch the parked section** of the backlog (owner-hardware items, future carves).

## End of run

Same as always: whenever you stop — item boundary, red-CI stop rule, or end of session —
STATUS.md tail updated with an accurate Now / Next, everything committed and pushed, no
uncommitted work, no isolated files. The next agent or Dan must be able to pick up from
STATUS.md alone.
