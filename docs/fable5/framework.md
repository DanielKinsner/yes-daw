# Scarce-Model Plan Elicitation — a reusable framework

> **Purpose.** Extract the deepest possible *plans* from a scarce, top-tier reasoning model
> (here: Fable 5) so that cheaper models can implement against them. This file is repo-agnostic —
> copy it anywhere. The per-repo instance (the filled-in prompts) lives beside it, e.g.
> [`yes-daw.md`](yes-daw.md).

## The idea in one line

A scarce genius is wasted on task breakdowns. Spend it on the things **only** a top reasoner does
well — your blind spots, the irreversible decisions, cross-cutting risk, and hard sequencing — and
let it hand a plan to models that are cheap enough to grind out the implementation.

## Five principles (the "wisdom")

1. **Scarcity → altitude.** Don't ask it to do what a cheap model already nails (boilerplate task
   lists, restating best practices). Ask it for judgement that spans the whole system and doesn't go
   stale: what to build, in what order, and which decisions you can't cheaply undo.
2. **Narrow vs broad is an *order*, not a choice.** Open/generative questions first (let it map the
   territory and surface options you didn't know existed), *then* narrow/closed questions to drill
   the specifics it raised. Ask narrow first and you *anchor* it — you kill the creativity you're
   paying a premium for. Broad to discover, narrow to lock.
3. **Make it generate the questions you didn't know to ask.** The cure for "questions I wish I'd
   asked" is meta-elicitation: before it plans, have it tell you *the decisions you don't know you
   need to make* and *the highest-leverage questions you're not asking*. This is your insurance
   against unknown-unknowns, and it's cheap.
4. **A model is only as good as its context pack.** Output quality is bounded by the fuel. For a
   mature repo the leverage isn't a cleverer prompt — it's feeding it your real decisions (the
   constitution, the ADRs, the roadmap) so it reasons from *your* system, not generic knowledge.
5. **Think first, format last.** Specify the output *shape* only at the end. Ask for tidy structure
   early and it optimizes for neatness over hard thinking.

## The pipeline (each stage = one dense one-shot prompt)

```
Stage 0  Context Pack      (you assemble — not a model turn)
   │
Stage 1  Blind-Spot Finder (cheap recon — ALWAYS run) ── forbid planning
   │
Stage 2  Open Exploration  ("let it cook")            ── broad, permission to disagree
   │
Stage 3  Narrow Drill      (precision)                ── irreversible decisions → artifacts
   │
Stage 4  Structured Handoff                           ── your house format, for the implementer
   │
Stage 5  Red-Team (optional)                          ── "find 3 ways this plan fails"
```

- **Stage 0 — Context Pack.** The fuel: constitution (`CLAUDE.md`/`CONTEXT.md`), decisions ledger
  (ADRs), roadmap/goals, current status, a key-module map, and **one paragraph defining "shippable."**
  Curate — feed the ~high-signal 10%, not everything. This is where care pays off most.
- **Stage 1 — Blind-Spot Finder.** *Forbid planning.* Ask for the decisions you don't know you need
  to make, where the architecture bites before ship, the 10 highest-leverage questions you're not
  asking, and what it would need to plan well. Output: a ranked blind-spot + question register. You
  review this; it steers everything after it.
- **Stage 2 — Open Exploration.** Feed Stage 1 back. High-agency and generative: map the space to
  shippable, propose 2–3 strategic arcs with trade-offs, challenge your framing, tell you where
  you're wrong on scope/order/risk. Output: strategic options + a recommended arc.
- **Stage 3 — Narrow Drill.** Pick an arc. Turn irreversible decisions into decision-record stubs,
  give risky items concrete mitigations + mechanical tests, give the next slice a plan with a
  testable exit criterion. This is where narrow questions belong — *after* the map exists.
- **Stage 4 — Structured Handoff.** Assemble into your house format so a cheaper model can execute:
  ADRs, plans with mechanical exit criteria, a risk register, and a "for the implementing model"
  brief of constraints it must never violate.
- **Stage 5 — Red-Team (optional).** Have it (or a fresh session) attack its own plan: "here is the
  plan — find the 3 most likely ways it fails and what you'd change." Catches confident-but-wrong.

## Two dials you set per repo before firing

- **Altitude.** *Whole-arc-to-shippable* (high; ages slowly; the **best** use of a scarce model)
  vs *next-slice-deep* (implementation-grade; ages fast — cheaper models + your normal process
  often serve this better). Run both if a repo is worth it, but spend the premium budget high.
- **Early-exit rule.** Stage 1 is roughly **60% of the value for ~15% of the cost.** *Always* run
  Stage 1. Only pay for Stages 3–4 if Stages 1–2 surfaced something worth the spend.

## Applying it to a new repo (the transferable checklist)

1. Assemble the **Stage 0 context pack** (curate to high-signal).
2. Write your **one-paragraph "shippable"** definition.
3. Set the **two dials** (altitude, early-exit).
4. Run **Stage 1**; review the blind-spot register; decide whether to continue.
5. Run **Stages 2 → 4** as budget allows, feeding each output into the next.
6. Optionally **Stage 5** red-team before you trust the plan.
7. Hand the Stage 4 pack to your implementing models.

Only the fuel changes between repos. The pipeline is the same — that's what makes it a skill.
