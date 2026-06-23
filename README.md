# YES DAW

A from-scratch audio production workstation, built deliberately.

The precise product wedge (finishing/mastering vs. stem-remix vs. AI-mastering console) is
**still being decided** — that decision, and the engine/UI/plugin/format decisions beneath it,
are the subject of the brainstorming and grilling sessions that open this project. Everything
here is grounded in the consolidated research under [`docs/research/`](docs/research/).

## Why this repo looks like docs before it looks like code

This is intentional. The research is emphatic that a DAW's foundational choices — the audio-node
abstraction, the event model, the thread-separation contract, the routing graph — are the ones you
**cannot refactor your way out of later**. So we measure twice before we cut once:

```
research  →  brainstorm  →  grill (sharpen language vs. docs)  →  ADR (record the decision)
          →  plan  →  build (TDD)  →  review  →  capture learnings
```

Source code lands only after the decisions that shape it are recorded as ADRs.

## Repository map

| Path | What lives here | Maintained by |
|---|---|---|
| [`CONTEXT.md`](CONTEXT.md) | Ubiquitous language — the shared vocabulary for the whole project | grilling sessions, ongoing |
| [`docs/research/`](docs/research/) | The source research reports this project is grounded in | reference (read-only) |
| [`docs/adr/`](docs/adr/) | Architecture Decision Records + the backlog of pending decisions | every architecture choice |
| [`docs/goals/`](docs/goals/) | Long-horizon roadmap and goals for `/goals` and `/loop` | as horizons evolve |
| `docs/brainstorms/` | Brainstorm session outputs (created on first use) | `/workflows:brainstorm` |
| `docs/plans/` | Implementation plans (created on first use) | `/workflows:plan` |
| `docs/solutions/` | Reusable learnings and post-mortems (created on first use) | as we learn |

## Current status

**Phase: orientation complete → brainstorming next.** No engine code yet. The five open
architecture forks are tracked in [`docs/adr/README.md`](docs/adr/README.md) under "Decision backlog".

## How we work

- **Measure twice, cut once.** Decisions get recorded as ADRs before code depends on them.
- **LF line endings everywhere** (`.gitattributes`), Windows-friendly.
- **Real-time safety is non-negotiable** once the engine exists: no allocations, locks, logging,
  or I/O on the audio thread — verified in tests, not just by convention.
