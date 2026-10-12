# 0075. Checkpoint cadence: one verified change set per unit

- **Status:** Accepted (owner decision, Dan, 2026-10-12)
- **Date:** 2026-10-12
- **Deciders:** Dan (owner); build agent (proposer)
- **Related:** amends [ADR-0049](0049-autonomous-delivery-and-usable-song-milestone.md)'s autonomous-execution cadence
  (a critic pass per step; local checks before each commit and push; exact-code-SHA CI before checkpoint completion)
  and the active plan's §8.1 steps 3-4; keeps [ADR-0005](0005-mechanical-verification-and-ci-gates.md) (CI is the
  gate) whole.

## Context

The UI correctness pass ran every commit as its own checkpoint: the full local suite (about eight minutes) once per
commit, so twice per unit; a separate critic per commit; three to five named mutations per gate; a push per commit; and
push timing around CI's queue (a queued run is replaced by the next push, so a commit could lose its own run). CI itself
runs in the background and was not the bottleneck; the local repetition was, at roughly a third of each unit's time.
The rule that every green chunk is its own commit and none are squashed served a commit-count contest that has ended.

The checks themselves earn their place: unit 6's critics found a real defect in each of its commits (the inverse's
float off-by-one, and a ruler that stopped labelling across sharply shrinking bars). What changes is how often they run.

## Options considered

1. **Keep the per-commit cadence.** Most proof per commit; slowest, and the commit-count reason is gone.
2. **One verified change set per unit (chosen).** Targeted tests per commit; the full suite, the critic and the push
   once per unit; one named mutation per gate unless the plan names distinct failure modes. About 30-40% faster.
   Intermediate commits get less proof, so `git bisect` inside a unit may land on a commit that only ran its targeted
   tests.
3. **Push several units at once.** Faster again, but a red CI run is harder to attribute to one unit.
4. **Drop CI jobs (macOS, sanitizers, packaging).** They catch what this Windows machine cannot; little time saved.
   Rejected. Moving slow or noisy jobs to a nightly tier stays open for later if CI latency starts to block work.

## Decision

Option 2.

- **Commits.** One per logical change, straight to `main` - usually one to three per unit. No artificial splits and no
  squashing of real ones. A refactor-first commit only where it lets a new gate run red on the old code. Each commit
  builds and passes the tests it touches.
- **Per unit (the checkpoint).**
  - Each new gate runs red on the code before the fix, and at least one named mutation turns it red. More mutations
    only when the plan names distinct failure modes.
  - The full local build, ctest and Clang check run once, before the push.
  - Current and earlier Session drives as the plan requires; drive proof may still be batched on one packaged build.
  - One separate agent critic for a unit that changes logic, engine behaviour, saved data or a shared law. A small
    layout, label or text-only unit may skip it, and its STATUS entry says so. Findings are verified against the
    project, not by consensus.
  - One push. Every expected CI job on the unit's head SHA finishes before the unit is certified or a unit that
    depends on it starts; independent work continues meanwhile. Red gets corrective commits; a docs-only green run
    never certifies changed code.
- **Unchanged:** CI is the gate (ADR-0005); verification is mechanical; the narrow macOS GPU timing exception (plan
  §8.2); no manual PASS rows; three failed corrective attempts call for a critic and a new hypothesis.

## Consequences

- Units close faster; the proof per unit is the same, the proof per intermediate commit is lighter.
- A CI red is attributed to the unit's change set, then bisected inside it if needed.
- CLAUDE.md and AGENTS.md state this cadence; the active plan's §8.1 and the UI correctness pass's checkpoint rule
  point here.
