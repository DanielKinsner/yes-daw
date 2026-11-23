# Kickoff: resume YES DAW on Musical audio (2026-11-23 redirect)

Paste everything in the block below into a fresh Codex or Claude session.

```text
Repo: yes-daw. cd into wherever it is cloned on this machine; don't assume a path. Run `git pull` first.

READ FIRST, in this order:
1. STATUS.md: the 2026-11-23 entry at the top and the numbered scoreboard below it. The 2026-11-14 entries
   are history; do NOT continue their "Next" lines.
2. CLAUDE.md (working method; AGENTS.md is identical).
3. docs/adr/0092-park-clock-research-resume-musical-audio.md (Dan's redirect) and docs/adr/0078-ship-first-cadence.md.
4. docs/adr/0083-musical-audio-prepared-lane.md and docs/plans/2026-11-04-musical-audio-prepared-lane-plan.md.

WHAT HAPPENED: from 2026-11-05 to 2026-11-14, 153 commits went into Clock, a timing-oracle and supervisor research
rig that never ran, while no product code changed. Dan has parked Clock (ADR-0092). The product goal is still the
roadmap's active item: Musical audio tempo-follow, meaning an audio clip that stays in time when the song's tempo
changes. Foundations exist (checked windows/keys, conversions and row history, cut plans, channel classification,
whole-step history guard). Audible tempo-follow is not enabled.

TASK, step 1 (then STOP):
Write a one-screen, plain-English plan for the SHORTEST path to the first moment Dan can hear an audio clip
follow a tempo change in the app. For each of the plan's 12 units, say: done / needed for that first audible
moment / can wait until after. List the needed steps in order. Each step must change src/ or tests/ and be one
logical commit or a small group of them. Put the plan in a new top STATUS entry, commit, push, and STOP. Show
Dan the plan in chat with lettered options, one marked (Recommended). Write no code until he says go.

TASK, step 2 (after Dan's go): build the steps in order, one at a time, finishing each before starting the next.

GUARDRAILS:
- Do not touch, continue, review or run anything Clock: ClockOracle, the supervisor, R13-R22 runners, their
  censuses, pricing or resource proofs, or anything under docs/research/ about them.
- No new oracles, supervisors, resource-bound proofs, out-of-process harnesses, census or pricing work, or any
  other research tooling or verification gate. Reopening any of that needs Dan's go in his own words, quoted in STATUS.
- New ADRs only for a real architecture or file-format decision (ADR-0078). Never rewrite Accepted ADRs.
- CLAUDE.md's hard rules stand (ADR-0002): the audio thread never allocates, locks, logs or does I/O; clips stay
  non-destructive; LF line endings.
- Never change the saved-project format or an owner-settled contract without asking Dan first.
- Another agent may be working on this repo: `git pull` before every commit.
- The unrun Nov 5 public-recipe test is preserved in
  docs/research/checkpoints/2026-11-06-user-test-stop/paused-work.json. Reuse it if it helps; don't build tooling around it.

PROGRESS RULE: every batch of work changes src/ or tests/. If a batch would only add docs or research, stop
and tell Dan in plain English what is blocking you, instead of writing more proof.

VERIFICATION (ADR-0078 ship-first): the build passes and the existing suite stays green; one smoke test per new
feature; one regression test per bug found; always test save/load/autosave/recovery changes; one critic pass
for engine, audio-thread or saved-data changes; CI green, and fix red promptly. Never report anything green
that you did not run. Paste the command and its result.

COMMITS: small, one logical change each, straight to main as CLAUDE.md says; push when done. STATUS gets a
3-5 line entry per batch, and keep the scoreboard at the top current.

STOP AND ASK DAN (plain English, bottom line first, lettered options with one Recommended):
1. After step 1's plan, before any code.
2. When an audio clip audibly follows a tempo change: build a test build and tell Dan exactly what to click and
   what he should hear.
3. When three honest attempts at the same problem have failed: report the problem; don't build tooling around it.
4. Before changing the saved-project format or any owner-settled contract.
```
