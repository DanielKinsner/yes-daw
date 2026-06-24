# YES DAW

[![CI](https://github.com/DanielKinsner/yes-daw/actions/workflows/ci.yml/badge.svg)](https://github.com/DanielKinsner/yes-daw/actions/workflows/ci.yml)

A from-scratch, general-purpose **multi-track DAW** (Logic / Pro Tools / Cubase / Sonar class), built
deliberately.

Product and stack are **decided**: a full general-purpose DAW, not a stem/finishing wedge
([ADR-0003](docs/adr/0003-product-full-multitrack-daw.md)), built on C++/JUCE + our own engine
([ADR-0004](docs/adr/0004-stack-juce-framework-own-engine.md)). We're past planning and into **H0
(spikes)** — proving out the scariest unknowns behind a green CI gate (see [`STATUS.md`](STATUS.md)).
Everything is grounded in the consolidated research under [`docs/research/`](docs/research/).

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

**Phase: H0 spikes.** Spike #1 (audio device round-trip) plays a 440 Hz tone on real hardware, and
verification is now **mechanical**: GitHub Actions builds the app and runs a headless, self-asserting
DSP check (golden output + pitch/level + a throughput floor) on Windows + macOS — the green badge
above is the gate, not a human. Live state lives in [`STATUS.md`](STATUS.md). No engine code yet (the
spike DSP is throwaway); the irreversible contracts get frozen at H1.

## How we work

- **Measure twice, cut once.** Decisions get recorded as ADRs before code depends on them.
- **LF line endings everywhere** (`.gitattributes`), Windows-friendly.
- **Real-time safety is non-negotiable** once the engine exists: no allocations, locks, logging,
  or I/O on the audio thread — verified in tests, not just by convention.
