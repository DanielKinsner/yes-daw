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
| [0005](0005-mechanical-verification-and-ci-gates.md) | Mechanical verification — CI is the gate | Accepted |
| [0006](0006-immutable-snapshot-concurrency.md) | Immutable compiled-snapshot concurrency | Accepted |
| [0007](0007-compiledgraph-and-pdc.md) | Own CompiledGraph + compile-time PDC | Accepted |
| [0008](0008-node-contract.md) | CLAP-shaped, format-neutral Node contract | Accepted |
| [0009](0009-event-stream-and-automation.md) | Sample-accurate event stream + automation curves | Accepted |
| [0010](0010-time-model-and-sample-rate.md) | Time model (ticks/PPQ/tempo) + sample-rate policy | Accepted |
| [0011](0011-asset-clip-project-and-ids.md) | Asset→Clip→Project indirection + 128-bit ULID | Accepted |
| [0012](0012-sqlite-bundle-schema-and-atomicity.md) | SQLite `.yesdaw` bundle, schema v1 + migrations | Accepted |
| [0013](0013-plugin-state-and-hosting-isolation.md) | Plugin state chunks + out-of-process hosting isolation | Accepted |
| [0014](0014-mixer-policy-solo-mute-sidechain.md) | Mixer policy: mute, SIP solo, solo-safe, and Sidechain pins | Accepted |
| [0015](0015-plugin-hosting-runtime-ipc-and-process-model.md) | Plugin hosting runtime: process model, IPC transport, isolation gates | Accepted |

## Decision status (the five research forks)

The brainstorm and build plan resolved these. They are recorded as ADRs as each is locked.

| Fork | Decision | Resolution | Recorded |
|---|---|---|---|
| **#1** | Product identity | **Full general-purpose multi-track DAW** (not a stem/finishing wedge) — the research's narrow-wedge premise didn't apply (owner already ships standalone stem + mastering apps). | ✅ ADR-0003 |
| **#3** | Engine stack | **C++ / JUCE 8 framework + our own engine** (not Rust, not Tracktion Engine; not `AudioProcessorGraph`). | ✅ ADR-0004 |
| **#2** | UI stack | Build plan recommends **native JUCE Components** + a GPU timeline canvas (not WebView) for data-dense surfaces. | ADR pending (**H2** — folded in with the deferred GPU render shell) |
| **#4** | Plugin hosting | **Out-of-process / sandboxed from the start** — VST3 + AU first, then CLAP (H3). | ✅ ADR-0013 |
| **#5** | Project format | Build plan: **SQLite `.yesdaw` bundle**, normalized tables (not JSONB), WAL, migration harness (H1). | ✅ ADR-0012 |

The build plan's **15 irreversible engine decisions** extend ADR-0002 and become individual ADRs as
each milestone (H1, H3, …) is planned. See
[`docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md`](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md).

### Engine decisions → ADR tracking

So none is silently skipped (ADR numbers are provisional until written). **Note (2026-06-23):** ADR
**0005** is the mechanical-verification/CI-gates decision (above), so the engine-decision target numbers
below start at **0006** (shifted up one from an earlier draft to avoid colliding with 0005).

| # | Decision | Target ADR | Milestone |
|---|---|---|---|
| 1 | Immutable compiled-snapshot concurrency model | 0006 ✅ | H1 |
| 2 | Own CompiledGraph + compile-time PDC (audio+MIDI+automation) | 0007 ✅ | H1 |
| 3 | CLAP-shaped format-neutral Node contract | 0008 ✅ | H1 |
| 4 | Sample-accurate, block-sliced, generic event stream (UMP-superset) | 0009 ✅ | H1 |
| 5 | Dual time representation + tempo-map curve (**PPQ = 15360**, resolved) | 0010 ✅ | H1 |
| 6 | Per-clip `time_base` | 0010 ✅ | H1 |
| 7 | Asset→Clip→Project non-destructive indirection | 0011 ✅ | H1 |
| 8 | Stable persistent IDs (**128-bit ULID**, resolved) | 0011 ✅ | H1 |
| 9 | Variable / renegotiable Block size | 0007 ✅ | H1 |
| 10 | SQLite `.yesdaw` bundle, normalized tables, migrations (+ bundle-atomicity) | 0012 ✅ | H1 |
| 11 | Plugin state as opaque chunks | 0013 ✅ | H3 |
| 12 | Hosting isolation (**out-of-process / sandboxed**, resolved) | 0013 ✅ (runtime impl: 0015 ✅) | H3 |
| 13 | f64 Bus summing | 0007 ✅ | H1 |
| 14 | Sample-rate policy | 0010 ✅ | H1 |
| 15 | Automation curve representation | 0009 ✅ | H1 |

The three substantive conflicts (PPQ-freeze, stable-ID, hosting isolation) were **resolved 2026-06-23**
(see the plan's enhancement summary). The deepening notes retain the full debate.

**Cross-cutting foundations** (locked in ADR-0002): real-time-safe audio thread separated from UI;
DAG routing graph with per-node latency + PDC from day one; format-neutral node contract;
sample-accurate block-sliced events; lock-free UI↔audio messaging; SQLite-centered persistence with
autosave/crash recovery. Plugin hosting is **out-of-process, VST3 + AU first then CLAP** (not
CLAP-first, not in-process — decision #12). YES DAW is a **general-purpose DAW, not a stem/finishing
tool** (ADR-0003); stem separation and mastering are separate apps.
