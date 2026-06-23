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

The build plan's **14 irreversible engine decisions** extend ADR-0002 and become individual ADRs as
each milestone (H1, H3, …) is planned. See
[`docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md`](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md).

### Engine decisions → ADR tracking

So none is silently skipped (ADR numbers are provisional until written):

| # | Decision | Target ADR | Milestone |
|---|---|---|---|
| 1 | Immutable compiled-snapshot concurrency model | 0005 | H1 |
| 2 | Own CompiledGraph + compile-time PDC (audio+MIDI+automation) | 0006 | H1 |
| 3 | CLAP-shaped format-neutral Node contract | 0007 | H1 |
| 4 | Sample-accurate, block-sliced, generic event stream (UMP-superset) | 0008 | H1 |
| 5 | Dual time representation + tempo-map curve (**PPQ-freeze — open conflict**) | 0009 | H1 |
| 6 | Per-clip `time_base` | 0009 | H1 |
| 7 | Asset→Clip→Project non-destructive indirection | 0010 | H1 |
| 8 | Stable persistent IDs (**mechanism — open conflict**) | 0010 | H1 |
| 9 | Variable / renegotiable Block size | 0006 | H1 |
| 10 | SQLite `.yesdaw` bundle, normalized tables, migrations (+ bundle-atomicity) | 0011 | H1 |
| 11 | Plugin state as opaque chunks | 0012 | H3 |
| 12 | Hosting isolation (**in- vs out-of-process — open conflict**) | 0012 | H3 |
| 13 | f64 Bus summing | 0006 | H1 |
| 14 | Sample-rate policy | 0009 | H1/H2 |

Open conflicts (PPQ-freeze, stable-ID, hosting isolation) are detailed in the build plan's deepening
notes → "Conflicts flagged for human review".

**Decisions both reports already agree on** (likely fast-tracked ADRs, low contention): real-time-safe
audio thread separated from UI; DAG routing graph with per-node latency + PDC from day one;
format-neutral node contract; sample-accurate block-sliced events; lock-free UI↔audio messaging;
SQLite-centered persistence with autosave/crash recovery; CLAP as the first plugin format;
local stem separation as a first-class workflow; AI assistance always user-overridable.
