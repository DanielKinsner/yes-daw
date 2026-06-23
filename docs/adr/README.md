# Architecture Decision Records (ADRs)

An ADR captures **one** architecturally significant decision: the context that forced it, the
options weighed, the choice made, and the consequences accepted. We write them because the research
is emphatic that a DAW's foundational choices are the ones you cannot cheaply undo later. An ADR is
how "measure twice, cut once" leaves a paper trail.

## Conventions

- One decision per file: `NNNN-short-kebab-title.md`, numbered sequentially from `0001`.
- Use [`template.md`](template.md).
- **Status** is one of: `Proposed` → `Accepted` → (`Superseded by NNNN` | `Deprecated`).
- ADRs are append-only history. To change a decision, write a new ADR that supersedes the old one;
  don't rewrite the old file (mark it `Superseded`).
- When an ADR is accepted, update any term it touches in [`../../CONTEXT.md`](../../CONTEXT.md).

## Accepted / proposed ADRs

| # | Title | Status |
|---|---|---|
| [0001](0001-record-architecture-decisions.md) | Record architecture decisions | Accepted |
| [0002](0002-realtime-engine-foundations.md) | Real-time engine foundations | Accepted |
| [0003](0003-product-full-multitrack-daw.md) | Product: a full multi-track DAW | Accepted |
| [0004](0004-stack-juce-framework-own-engine.md) | Stack: C++/JUCE framework + our own engine | Accepted |

## Decision status (the five research forks)

The brainstorm and build plan resolved these. They are recorded as ADRs as each is locked.

| Fork | Decision | Resolution | Recorded |
|---|---|---|---|
| **#1** | Product identity | **Full general-purpose multi-track DAW** (not a stem/finishing wedge) — the research's narrow-wedge premise didn't apply (owner already ships standalone stem + mastering apps). | ✅ ADR-0003 |
| **#3** | Engine stack | **C++ / JUCE 8 framework + our own engine** (not Rust, not Tracktion Engine; not `AudioProcessorGraph`). | ✅ ADR-0004 |
| **#2** | UI stack | Build plan recommends **native JUCE Components** + a GPU timeline canvas (not WebView) for data-dense surfaces. | ADR pending (H0/H1) |
| **#4** | Plugin hosting | Build plan: in-process **VST3 + AU first, then CLAP** (H3); Nodes kept proxy-able for later sandboxing. | ADR pending (H3) |
| **#5** | Project format | Build plan: **SQLite `.yesdaw` bundle**, normalized tables (not JSONB), WAL, migration harness (H1). | ADR pending (H1) |

The build plan's **13 irreversible engine decisions** extend ADR-0002 and become individual ADRs as
each milestone (H1, H3, …) is planned. See
[`docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md`](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md).

**Decisions both reports already agree on** (likely fast-tracked ADRs, low contention): real-time-safe
audio thread separated from UI; DAG routing graph with per-node latency + PDC from day one;
format-neutral node contract; sample-accurate block-sliced events; lock-free UI↔audio messaging;
SQLite-centered persistence with autosave/crash recovery; CLAP as the first plugin format;
local stem separation as a first-class workflow; AI assistance always user-overridable.
