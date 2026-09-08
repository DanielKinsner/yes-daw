# YES DAW — project instructions

A from-scratch, general-purpose multi-track DAW (Logic / Pro Tools / Cubase / Sonar class), C++/JUCE +
our own engine. Product and stack are **decided** (ADR-0003 / ADR-0004). The active shell plan is
`docs/plans/2026-09-01-real-daw-ground-up-plan.md`; ADR-0049 governs autonomous delivery and the
Usable-song milestone. The older engine roadmap remains background. Grounded in `docs/research/`.

## Talk to me like this
- Be concise. Plain English. Lead with the answer, then a little reasoning.
- For any decision, give multiple-choice options with a clear recommendation. Don't make me read essays.
- Skip the throat-clearing and long recaps.

## How we work
- Measure twice, cut once. Order: research → brainstorm → grill → ADR → plan → build → review.
- No code lands before the decisions it depends on are written as ADRs (`docs/adr/`).
- `CONTEXT.md` is the shared vocabulary. Use those exact terms; update it when a decision changes one.
- Long-horizon goals live in `docs/goals/`. `/loop` runs against the current horizon's exit criterion.

## Working across machines
Dan works on multiple machines; git is the sync. Keep the handoff clean:
- **`STATUS.md` is the live handoff** — the single source of truth for "where are we right now." Read
  it first; update it (current task, done, next) before committing.
- **Pull at session start, push at session end.** Never leave finished work uncommitted on one machine.
- **Commit frequently, straight to `main`** — every green chunk is its own commit (Dan wants a high
  commit count and is in a commit contest; **do not squash**). Each commit should be independently
  green so `git bisect` works; CI runs on every push as the safety net.
- **Break work down in plain English** — `STATUS.md` holds small, plainly-worded, committable steps.

## Autonomous checkpoints (ADR-0049)
Work in small, verifiable chunks. A build/continue request defaults to the current named milestone
in STATUS unless Dan specifies a smaller scope. A plan-edit request alone does not start feature
work, a goal, a scheduler, or later background runs. At each checkpoint:
1. Run applicable local checks and current/earlier Session drives; an agent judges the visual rubric.
   Obtain a separate agent critic pass; verify its findings against the project, not by consensus.
2. Update STATUS, commit small, and push. Then wait for every expected CI job on that exact code
   SHA to finish. Fix red with corrective commits; docs-only green never certifies changed code.
3. Record results and report useful progress. Continue within the authorized milestone without a
   routine approval request. Phase advancement is automatic when its required evidence is complete.

Stop at the requested finish line, a concrete external blocker, or host execution/budget limits.
If unfinished, leave exact status and next steps; do not certify missing proof. Earlier product
regressions and broken required verification interrupt feature work and are repaired immediately.
Unrelated ideas stay parked. Unknown failures are not automatically tooling flakes. The sole
standing macOS GPU timing exception is narrowly defined in the active plan §8.2; report the raw
failure and never extend it to another failure. After three unsuccessful corrective attempts,
require an agent critic and a new supported hypothesis rather than asking Dan to debug.

Human input is only for a consequential choice outside accepted scope/ADRs, genuinely unavailable
access/equipment, or an action outside existing authority (spending, publication, destructive user
data changes). Prepare the concrete result and recommendation first. Code review, ordinary
implementation choices, testing, screenshot judgment and checkpoint advancement belong to agents.

## Verification is mechanical (Dan can't read code, and is busy)
Dan does not read diffs and won't hand-verify by ear or eye. Every check must be **mechanical** — CI
gates and self-asserting tests (exit 0/1), never "human reviews the diff" or "human confirms 60fps /
the tone." If something can be checked in code (frame-time budget, deadline-miss / xrun count,
golden-output compare, RTSan, warnings-as-errors), it must be. **CI is the gate**; the agent fixes red
before certification, under the exact exception policy above. A check that needs real hardware is a
**one-command self-asserting script** on a single machine — it prints PASS/FAIL, never asks Dan to judge.

Agents may run available hardware checks and commit genuine measurement-generated results. No
manual PASS rows, synthetic hardware credit or silent threshold changes. For app drives, prefer a
verified separate machine/VM/logon-input session that cannot steal Dan's focus/mouse; a Windows
virtual desktop or hidden process alone is insufficient. On the shared desktop use an existing
authorized hands-off window, not a new permission question per script. Missing access leaves the
affected evidence pending while safe independent work continues.

## Hard rules (both research reports agree; locked in ADR-0002)
- Once the audio engine exists: the audio thread never allocates, locks, logs, or does I/O. Tested, not assumed.
- Routing is a DAG. Per-node latency + plugin delay compensation exist from day one.
- Built-in DSP and hosted plugins share one format-neutral node contract.
- Events are sample-accurate and block-sliced from the start.
- Clips reference assets (non-destructive); never edit the underlying audio in place.
- LF line endings everywhere (`.gitattributes`).

## Agents & workflows
- Prefer compound-engineering workflows (brainstorm / plan / work / review) over basic skills.
- `/grill-with-docs` to sharpen language and lock decisions into ADRs.
- Custom DAW review agents (real-time safety, render correctness, etc.) come once there's engine code.
- **Agentic loop workflow adopted:** follow the active plan §8 and ADR-0049 within authorized scope.
  Do not rewrite Accepted ADRs, goldens or RT annotations. New in-scope implementation ADRs called
  for by the plan can be written and accepted with agent critic review when they preserve owner-settled
  contracts; replacing such a contract needs the concrete human decision described above.
  UI visual judgment uses the agent rubric; Dan is not the routine tester or approver.
- Keep `AGENTS.md` and `CLAUDE.md` byte-identical when either changes.
