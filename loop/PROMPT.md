# YES DAW builder — marching orders (point an agent here)

You are the YES DAW builder, closing horizon **H3 (mixer + plugin hosting)**. Work in the repo at its
current `main`.

## Orient (before touching anything)
1. `git pull` (Dan works across ~3 machines; git is the sync).
2. Read **`STATUS.md`** (live handoff), **`CLAUDE.md`** (how Dan works), **`CONTEXT.md`** (shared vocabulary),
   **`loop/horizon.md`** (the exit gate = the definition of "done"), and the close-out plan
   **`docs/plans/2026-06-26-h3-close-out-plan.md`** (the full build order + findings ledger).
3. Skim ADR-0013/0014/0015/0016 (`docs/adr/`).

## Execute the close-out plan, depth-first
Build its **critical path in order: item 1 → 2 → 3 → 4 → 5.** Item 1 is the RED exit gate
(`YesDawHostIsolationCheck`) + the in-repo synthetic test plugin. Items 6–8 are post-gate. Pick up wherever
`STATUS.md` says the last checkpoint left off.

## Non-negotiable rules (from CLAUDE.md + the plan)
- **Exit-gate-first.** Write `YesDawHostIsolationCheck` as a Catch2 **`[!shouldfail]`** gate so `main` stays
  green while it is correctly-red; **flip it to blocking the moment the hosting makes it pass.** H3 closes
  **iff** that gate is green — never a "feels done."
- **Every test negative-controlled.** Before trusting a test, prove it **FAILS** when the code it guards is
  broken (revert the fix, watch it go red, restore). A green test that doesn't fail on a real break is theater.
- **Verification is MECHANICAL.** Dan cannot read diffs. "Done" = CI green or a self-asserting test exits 0 —
  never "I confirmed by listening/looking." Fix red before stopping.
- **Commit small, straight to `main`, push often** — each commit independently green (so `git bisect` works);
  high commit count, do not squash. CI is the safety net (full RTSan/TSan/3-OS matrix).
- **Local build is MSVC-only here and misses GCC/Clang strictness — CI is the real gate.** Don't trust a
  local-green commit until CI confirms it across the matrix.
- **Disposition, never defer silently.** Every finding/old-deferral in the plan's ledger is DONE or
  named-horizon-tagged with a reason. Add new ones the same way.
- **Independent adversarial review before declaring the gate green** — the builder does not grade its own
  homework.
- **Never edit** goldens, ADR decisions you aren't deliberately revising, or `[[clang::nonblocking]]` /
  `YESDAW_RT_HOT` annotations as a shortcut. The audio thread never allocates/locks/logs/does I/O. **LF**
  line endings.

## Checkpoint (how a session ends cleanly)
At each coherent, committable unit: update **`STATUS.md`** (done / Now / Next), commit small, push, confirm
CI, then **report plainly** (what you did, what the CI gate checks, what's next) and **STOP** — don't roll
into the next chunk. Do not close H3 or touch H4 until `loop/horizon.md`'s gate is green.
