# 0092. Park Clock research; Musical audio resumes without it

- **Status:** Accepted (owner decision, Dan, 2026-11-23). Relayed by a Claude Code session that reviewed the
  repository with Dan. His go, in chat: "yes-daw redirect next", approving the proposal to "park the 'Clock' rig
  and point the next agent back at tempo-follow".
- **Date:** 2026-11-23
- **Deciders:** Dan (owner); review session (proposer)
- **Related:** parks [ADR-0090](0090-clock-research-owned-process-supervisor.md) and
  [ADR-0091](0091-clock-supervisor-job-termination-diagnostic.md); reaffirms
  [ADR-0078](0078-ship-first-cadence.md); continues [ADR-0083](0083-musical-audio-prepared-lane.md) and its
  [delivery plan](../plans/2026-11-04-musical-audio-prepared-lane-plan.md).

## Context

Measured on `main` at `e7807d9`:

- The last commit that changed product code (`src/`, `tests/`, `tools/` or CMake) is `5785a3f` on 2026-11-05.
  Every one of the 153 commits since then is research or documentation.
- The 106 commits of 2026-11-14 changed 157 files, all under `docs/research/` plus `STATUS.md`.
- `docs/research/` is 568 MB; `src/` is 7.4 MB.
- Clock (ClockOracle: an out-of-process timing answer key plus a custom supervisor to launch and bound it) has
  reached runner revision R22 and has never run: "All Clock execution guards remain false; no supervisor/Clock run
  has started" (STATUS, 2026-11-14).
- ADR-0090 and ADR-0091 were accepted by "Root and independent source/resource critic", not by Dan. The musical
  audio plan's opening line ("Finish the currently authorized clock/native/MIDI interchange checkpoints first") made
  Clock a gate in front of the feature.
- The 2026-11-06 handoff said "Resume implementation only when Dan asks." No request from Dan is recorded before
  the 2026-11-14 run.
- ADR-0078 records Dan's direction: "I'd rather build it without so much testing, even with a high quality aim,
  and see where we land."

The audible result Dan is waiting for, an audio clip following a tempo change, is still not enabled.

## Options considered

1. **Finish Clock to its first run, then resume Musical audio.**
   - Pros: an independent timing check would exist.
   - Cons: no end date is visible after 22 revisions. The shipped timing code already passes the existing suite
     (491 local checks at `d3ec661`). It contradicts ADR-0078.
2. **Park Clock and resume Musical audio under ADR-0078 (chosen).**
   - Pros: product work restarts now on the roadmap's active item.
   - Cons: no independent out-of-process timing oracle; timing confidence rests on the existing suite plus new
     smoke and regression tests.
3. **Park Clock and also remove its research from the repository.**
   - Pros: a much smaller repository.
   - Cons: deletes history Dan has not reviewed. This is a separate owner decision and is not made here.

## Decision

1. **Clock is parked.** ClockOracle, its supervisor, the R-series runner revisions and the reviews, censuses,
   pricing and resource proofs around them get no further commits, reviews or runs. ADR-0090 and ADR-0091 are
   marked parked. Their artifacts stay in place as history; nothing is deleted.
2. **Clock is not a prerequisite** for Musical audio or for any other Option A item. The plan's opening "finish
   the clock/native/MIDI interchange checkpoints first" no longer applies.
3. **Musical audio resumes under ADR-0078.** Keep the existing suite green; one smoke test per new feature; one
   regression test per bug found; always test save/load/autosave/recovery changes; a critic pass for engine,
   audio-thread and saved-data changes (Musical audio touches all three, so it gets one). Do not add new oracles,
   supervisors, resource-bound proofs or out-of-process verification harnesses.
4. **Progress means product code.** Each batch of work changes `src/` or `tests/` toward Dan hearing an audio clip
   follow a tempo change. If a batch would only add research or documentation, stop and report instead.
5. **Reopening Clock, or adding any new research tooling or verification gate, needs Dan's explicit go in his own
   words**, quoted in STATUS. Agent or critic acceptance is not enough.

## Consequences

- **Positive:** the next session starts on the feature, not on infrastructure about tests.
- **Negative / accepted costs:** the Clock investment is shelved, not finished. If an independent timing oracle is
  ever wanted, it starts from the preserved R22 artifacts with Dan's go.
- **Follow-ups:** STATUS's scoreboard and the musical audio plan's opening line point here. Whether to move the
  568 MB of research out of the repository is a separate owner decision.
