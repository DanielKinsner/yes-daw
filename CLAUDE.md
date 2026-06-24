# YES DAW — project instructions

A from-scratch, general-purpose multi-track DAW (Logic / Pro Tools / Cubase / Sonar class), C++/JUCE +
our own engine. Product and stack are **decided** (ADR-0003 / ADR-0004); the build plan and H0–H6
roadmap live in `docs/plans/` and `docs/goals/`. Grounded in `docs/research/`.

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

## Stop checkpoints (how a session ends cleanly)
Work in small, verifiable chunks. At each **checkpoint** — one coherent, committable unit — stop and
hand back. Every checkpoint, in order:
1. **Update `STATUS.md`** — tick what's done; set "Now" and "Next".
2. **Commit small, then push.**
3. **Report plainly:** what you did, what the CI gate checks (on red, the failing job — never "listen/watch"),
   and what's next. Then **stop and wait** — do not roll into the next chunk on your own.

At a checkpoint Dan verifies and may review the diff (sometimes with an outside tool). Treat any
review findings as input to **verify against the project, not gospel** — don't rubber-stamp, don't get
swayed by strong wording; apply what's correct, report changes in plain English, commit small.

A checkpoint is "done" when its **mechanical** check is green (CI / a self-asserting script) — fix red
before stopping. Once CI + the self-check harness exist (the first H0 task), an **unattended loop** can
chain checkpoints back-to-back with little human input; H1+ just formalises it.

## Verification is mechanical (Dan can't read code, and is busy)
Dan does not read diffs and won't hand-verify by ear or eye. Every check must be **mechanical** — CI
gates and self-asserting tests (exit 0/1), never "human reviews the diff" or "human confirms 60fps /
the tone." If something can be checked in code (frame-time budget, deadline-miss / xrun count,
golden-output compare, RTSan, warnings-as-errors), it must be. **CI is the gate**; the agent fixes red
before stopping. The rare check that needs real hardware (sound actually out, real-GPU smoothness) is a
**one-command self-asserting script** on a single machine — it prints PASS/FAIL, never asks Dan to judge.

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
- **Agentic loop workflow adopted in full** (from H1): the loop implements toward the current horizon's
  exit criterion and commits only when CI gates pass, with an automated critic pass + hard-stops (never
  edits ADRs, goldens, or `[[clang::nonblocking]]` annotations). See the plan's "Long-horizon execution
  via agentic loops". H0's gates exist now (build + Catch2 + golden + RTSan + soak; ADR-0005), so the
  loop's precondition is met from H0 — only GUI *visual feel* is still human-eyeballed.
