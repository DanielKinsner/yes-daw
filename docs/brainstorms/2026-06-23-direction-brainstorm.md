# YES DAW — Direction Brainstorm

**Date:** 2026-06-23
**Status:** captured — ready for planning
**Supersedes:** the stem/finishing "wedge" framing in `docs/goals/roadmap.md`, the `CONTEXT.md`
"wedge" entry, and ADR fork #1. Those were drawn from the research papers and no longer apply
(see "Key decisions"). They need revising during planning.

## What we're building

A full, general-purpose **multi-track DAW** in the class of Logic / Pro Tools / Cubase / Sonar —
a real, usable DAW the owner is proud of and uses daily (they currently have none). **Audio and
MIDI are co-equal.** The **linear timeline** is the core time model. Built on **C++ / JUCE** for
the framework (audio I/O, plugin hosting, DSP utilities, UI) with **our own engine and data model**
on top — not Tracktion Engine, not Rust.

This is a deliberate **long-horizon, "pie in the sky"** project: planned thoroughly up front so that
day-to-day work is mostly bug-fixing and steady progress, driven over time via `/goals` and `/loop`.

It is **not** a stem/finishing/mastering tool. The owner already ships standalone Demucs-stemming
and mastering-DSP apps. Those stay standalone and individually tuned; the DAW may integrate them
**later, only if clean**, via interfaces — host the mastering DSP as a plugin, call stemming as an
external tool/source node — never by merging codebases into a disjointed whole.

## Why this approach

- **The research's "narrow wedge, don't build a full DAW" advice doesn't apply.** It assumed a small
  team with no existing tools. The owner already has the stem + master pieces and wants the DAW itself.
  The research's *architecture* lessons (real-time engine, graph, PDC, plugin hosting, session format)
  still apply in full; only its *product-wedge* advice is discarded.
- **C++ / JUCE + own engine** is the middle path between building from scratch in Rust and adopting
  Tracktion Engine wholesale: proven framework (plugin hosting and device I/O are battle-tested across
  thousands of products) with full control of the engine and data model. Owner's explicit preference.
- **Playable spine first.** Build the thinnest end-to-end path that makes sound — audio I/O → transport
  → multi-track playback through the node graph → basic mixer (gain/pan/meter) → save/load — then widen.
  Proves the riskiest part (the real-time engine) early and yields something real fast.
- **Editing-first, not recording-first.** The owner's primary need is multi-track *editing*
  (cut/split, fades, gain) on imported audio. Recording is in scope but lower priority — added after
  editing, possibly gated by a user test.

## Key decisions

- **Product:** full general-purpose multi-track DAW; audio + MIDI co-equal; **linear timeline core**.
  Clip-launcher / Ableton-style session view dropped (owner doesn't use Ableton).
- **Stack:** C++ / JUCE framework + **custom engine and data model** (not Tracktion Engine, not Rust).
- **Foundations:** ADR-0002 (real-time engine invariants) applies unchanged.
- **First focus:** import + **multi-track editing** (cut/split, fades, gain) over playback, with
  save/load. Recording follows.
- **MIDI:** in scope and co-equal in the model from day one; sequenced during planning.
- **Integration of stem/mastering apps:** deferred; via plugin/tool interfaces only, if ever.
- **Cadence:** long-horizon; plan thoroughly, then iterate via `/goals` + `/loop`.

## Open questions (for planning)

- The first concrete milestone's exact feature list + acceptance criteria.
- How much JUCE scaffolding to adopt vs build (e.g. JUCE `AudioProcessorGraph` vs custom graph;
  `ValueTree` vs custom document model). — architecture call for the plan.
- Plugin format priority (VST3 + AU + CLAP) and when hosting lands.
- Project file format specifics (SQLite schema). — fork #5.
- UI approach: JUCE native vs JUCE + WebView. — fork #2.
- Where MIDI / instrument hosting lands in the roadmap relative to audio editing.
