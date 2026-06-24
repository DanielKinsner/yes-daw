# 0005. Mechanical verification — CI is the gate, not a human

- **Status:** Accepted
- **Date:** 2026-06-23
- **Deciders:** Dan (owner), build agent
- **Related:** `docs/ci-mechanical-verification.md` (the operating-model plan this records), `CLAUDE.md`
  ("Verification is mechanical"), ADR-0002 (real-time engine foundations), roadmap H0–H6 exit criteria.

## Context

The owner does not read code and works across several machines, often away from the keyboard. The
original verification model — "Dan builds it and listens/watches on real hardware" (early STATUS.md /
CLAUDE.md) — does not scale, cannot run unattended, and blocks the H1 agentic loop, whose precondition
is "CI gates exist." We need a verification model where the **only** human action is reading a green or
red check.

This is architecturally significant and hard to reverse: it dictates how *every* horizon (H0–H6) is
gated, how commits flow to `main`, and what "done" means. The research and ADR-0002 already require
real-time-safety and render-correctness to be *tested, not assumed*; this ADR makes that a hard,
project-wide operating rule with a concrete gate stack.

## Options considered

1. **Human-confirmed verification (status quo at kickoff)** — Dan builds, listens, watches.
   - Pros: zero tooling; trivially "tests" the real end-to-end experience.
   - Cons: doesn't scale; non-mechanical; can't run unattended; a non-coder can't judge a diff, 60fps,
     or a subtle dropout reliably; blocks the H1 loop.
2. **CI-as-the-gate, with a scripted real-hardware soak for the two irreducibly-physical checks** —
   every exit criterion becomes an automated check that exits 0/1.
   - Pros: mechanical; unattended-capable; bisectable; a green check is the whole verdict; unblocks H1.
   - Cons: upfront tooling; the two genuinely-physical checks (sound actually leaves the device; no
     dropouts over a long real-time run) still need one real machine — handled by a self-asserting
     soak script with loopback capture, not by ears.

## Decision

**CI is the source of truth. A commit is "good" iff CI is green.** Verification is a stack of
mechanical gates; no exit criterion anywhere in the project may read "human confirms / listens /
watches." The gate stack:

1. **Build matrix** (Windows + Linux + macOS) — it compiles and links everywhere.
2. **Headless self-asserting tests** via `ctest` (Catch2 `YesDawCheck`) — DSP/data/file logic is
   correct, asserted to numbers (e.g. the tone *is* 440 Hz, by Goertzel + zero-crossings, not by ear).
3. **Golden render-diff** — output didn't change unintentionally; an intended change is *blessed* once
   via `cmake --build build --target bless-goldens` (the single human moment, and it's "approve," not
   "listen").
4. **RTSan** (`-fsanitize=realtime`, Clang leg) — the audio hot path never allocates/locks/does I/O
   (ADR-0002's hard rule), enforced by a machine, not by inspection.
5. **Warnings-as-errors / format** — `/WX` · `-Werror` on our code; (clang-format to follow).
6. **Real-machine soak** (`tools/soak.sh`, one machine, scheduled) — the only physical checks: sound
   actually leaves the device and there are zero dropouts over a long run; made mechanical via loopback
   capture (assert captured RMS/FFT) and xrun/frame counters → exit 0/1.

**Commit model — "green-small," trunk-based.** Every commit on `main` must be independently green
(`cmake --build --preset ci && ctest --preset ci`), so `git bisect run ctest --preset ci` mechanically
finds any regression. The smallest change that still builds and passes is the unit, not the
human-readable change. Don't bundle a breaking refactor with its feature. Goldens, ADRs, and
`[[clang::nonblocking]]` annotations are protected: the agent never edits them silently to make a build
pass; a changed golden is an explicit `bless-goldens` commit.

## Consequences

- **Positive:** the owner's entire job becomes "push, glance at the check." Unattended H1 loop is
  unblocked (its precondition "CI gates exist" is met from H0). Every horizon inherits "exit = a CI
  check goes green." Regressions are bisectable without anyone reading code.
- **Negative / accepted costs:** upfront CI/test tooling; one real machine is still required for the
  soak (loopback wiring is a one-time owner setup); `-Werror` across three compilers means a warning on
  any one OS reds that leg (mitigated by `fail-fast: false`).
- **Follow-ups:** `docs/ci-mechanical-verification.md` holds the living detail. Rewrite every horizon's
  exit criterion as an automated check (begun at H0). Add the clang-format gate. CLAUDE.md /
  STATUS.md / AGENT.md updated to drop "listen/watch" as a verification mechanism.
