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
- **Commit in small chunks** with clear messages — one logical step per commit, not big batches.
- **Break work down in plain English** — `STATUS.md` holds small, plainly-worded, committable steps.

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
  via agentic loops". H0 is hands-on (no gates yet); GUI visual feel is human-eyeballed.
