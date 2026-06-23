# 0001. Record architecture decisions

- **Status:** Accepted
- **Date:** 2026-06-23
- **Deciders:** Dan
- **Related:** root [`README.md`](../../README.md), [`CONTEXT.md`](../../CONTEXT.md)

## Context

YES DAW is a from-scratch audio workstation. The consolidated research (`docs/research/`) is
explicit that a DAW's foundational choices — the node abstraction, event model, thread-separation
contract, and routing graph — are effectively irreversible once code depends on them. Building such
a system without a durable, reviewable record of *why* each foundational choice was made would mean
re-litigating settled decisions and risking silent drift away from them.

We also intend to drive long-horizon work through brainstorming, grilling, planning, and looping
sessions. Those workflows need a single canonical place to read "what did we already decide, and
why," that survives outside of conversation history.

## Options considered

1. **Record decisions as ADRs (Michael Nygard style).**
   - Pros: low ceremony; one decision per file; append-only history; well-understood; the exact
     convention the compound-engineering tooling already reads (`docs/adr/` + `CONTEXT.md`).
   - Cons: requires the discipline to actually write them.
2. **Keep decisions in chat / commit messages / a single design doc.**
   - Pros: zero setup.
   - Cons: not durable, not reviewable, no status lifecycle, drifts immediately; defeats the
     "measure twice" intent of this project.

## Decision

We will record every architecturally significant decision as an ADR in `docs/adr/`, using
`template.md`, numbered sequentially, with a `Proposed → Accepted → Superseded/Deprecated`
lifecycle. ADRs are append-only; a decision is changed by superseding it, never by rewriting it.
When an ADR is accepted, the vocabulary it touches is updated in `CONTEXT.md`.

## Consequences

- **Positive:** every foundational choice is traceable; brainstorms and plans can cite accepted
  ADRs as fixed ground; reviewers and future contributors (human or agent) can reconstruct intent.
- **Negative / accepted costs:** a small, deliberate writing tax on each significant decision.
- **Follow-ups:** the five open forks in [`README.md`](README.md) ("Decision backlog") are the next
  ADRs, gated on the brainstorming and grilling sessions.
