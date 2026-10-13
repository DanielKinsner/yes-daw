# 0078. Ship-first cadence

- **Status:** Proposed - awaiting Dan's direct confirmation. It was relayed on 2026-10-13 by the "YES DAW progress
  review" session as Dan's decision. The build session does not change CLAUDE.md, or accept a change to an owner
  decision, on a relay from another session; Dan accepts it by saying so in the build session, or by applying the
  CLAUDE.md text below himself.
- **Date:** 2026-10-13
- **Deciders:** Dan (owner, pending confirmation); build agent (proposer)
- **Related:** supersedes, when accepted, [ADR-0075](0075-checkpoint-cadence-one-verified-change-set-per-unit.md)'s
  cadence and the per-unit parts of [ADR-0049](0049-autonomous-delivery-and-usable-song-milestone.md). It does not
  touch ADR-0002's hard rules or ADR-0005's "CI is the gate".

## Context

Dan, as relayed: "This build has been going for months. I'm considering downloading an open-source DAW because it
seems our project is stuck, and I don't even know where we are. I'll have plenty of changes once I use it anyway... I'd
rather build it without so much testing, even with a high quality aim, and see where we land."

Under ADR-0075 each unit carries:
- a gate red on the old code, plus named mutations;
- a critic pass;
- a full local suite and Session drives;
- an agent rubric;
- per-unit CI certification;
- a long STATUS entry.

The UI correctness pass shows the cost: about 30 small fixes over two days, each with that whole apparatus.

## Decision

**Keep (the safety net):**
- CI builds and the existing suite stays green. Fix red promptly, but do not hold the next piece of work for every CI
  job.
- Audio-thread safety: the RTSan and TSan CI jobs stay as they are, and so does ADR-0002's rule that the audio thread
  never allocates, locks, logs or does I/O.
- Never lose user data: save, load, autosave and recovery keep their tests, and any change there gets a test.
- Do not regress what works: a bug Dan or a drive finds gets one regression test.
- CLAUDE.md's hard rules (audio thread, DAG, latency/PDC, one node contract, sample-accurate events, non-destructive
  clips, LF) are unchanged.

**Drop or cut back:**
- Mutation runs for every new gate: stop.
- A critic pass: only for engine, audio-thread or saved-data changes; none for UI.
- ADRs: only for real architecture or file-format decisions; none before ordinary UI or feature work.
- The visual rubric and screenshot judging as a gate: stop. Cosmetic findings go to the parking lot.
- Real-app drive batches (SS1-SS8): once per milestone, not per unit.
- A new feature gets one smoke test, not exhaustive gates.
- STATUS entries are 3-5 lines; small fixes are batched into one entry.
- STATUS keeps a plain-English scoreboard at its top (what works, what is missing, pain points), kept current.

## Consequences

- **Positive:** features land faster; Dan can see where things stand.
- **Negative / accepted costs:**
  - a regression can reach main between milestone drive batches;
  - a test without a mutation check can be weaker than it looks;
  - UI polish relies on Dan's own use.
- **CLAUDE.md text proposed** to replace "Autonomous checkpoints (ADR-0049, cadence ADR-0075)", for Dan to apply or
  approve. AGENTS.md stays byte-identical:

```
## Autonomous checkpoints (ADR-0049, cadence ADR-0078 - ship-first)
Work in small chunks toward the current milestone in STATUS; features first. Per change: the build passes and the
existing suite stays green; a new feature gets one smoke test; a bug Dan or a drive finds gets one regression test;
save/load/autosave/recovery changes always get a test. A critic pass only for engine, audio-thread or saved-data
changes. ADRs only for architecture or file-format decisions. Real-app drives once per milestone. Commit per logical
change and push; fix CI red promptly without holding the next piece of work for it. STATUS: a 3-5 line entry per
batch of work, and keep the plain-English scoreboard at its top current. Cosmetic findings go to the parking lot.
```
