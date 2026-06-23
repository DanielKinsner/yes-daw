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

## Decision backlog (the open forks from research)

These are the architecture decisions the brainstorm + grilling must resolve. Each becomes an ADR
once decided. They are listed in rough dependency order — #1 constrains the rest.

| Fork | Decision | The two poles (from `docs/research/`) | Depends on |
|---|---|---|---|
| **#1** | **Product wedge & identity** | Local-first stem **finishing/mastering** tool · vs · stem-based **remix** workstation · vs · AI-mastering **console** | — |
| **#2** | **UI stack** | Rust + **Tauri/WebView** (+ canvas/WebGPU) · vs · **native Rust GUI** (egui / Slint / Iced) | #1 |
| **#3** | **Engine language & core deps** | **Rust** + `cpal` + `rtrb` (both reports' default) · vs · C++ + JUCE/Tracktion (fast-but-abandons-Rust escape) | #1 |
| **#4** | **Plugin hosting timing & isolation** | **Defer** past v0, in-process + watchdog · vs · **early**, out-of-process sandbox | #1, #3 |
| **#5** | **Session format detail** | SQLite **bundle** + BLOBs + JSON export · vs · SQLite **JSONB** + partial load + CRDT sync + DAWproject | #3 |

**Decisions both reports already agree on** (likely fast-tracked ADRs, low contention): real-time-safe
audio thread separated from UI; DAG routing graph with per-node latency + PDC from day one;
format-neutral node contract; sample-accurate block-sliced events; lock-free UI↔audio messaging;
SQLite-centered persistence with autosave/crash recovery; CLAP as the first plugin format;
local stem separation as a first-class workflow; AI assistance always user-overridable.
