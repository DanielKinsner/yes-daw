# 0020. Roadmap extension — horizons H7–H10

- **Status:** Proposed
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0003 (full multi-track DAW), ADR-0004 (JUCE + own engine), ADR-0005 (mechanical
  verification — CI is the gate), ADR-0012 (SQLite bundle), ADR-0019 (H6 reliability gate),
  [roadmap](../goals/roadmap.md), the build plan, `CONTEXT.md`.

## Context

The roadmap stops at **H6**, which the build plan labels *"Reliability & polish (ongoing,
long-horizon)"* — a deliberately broad bucket that bundles eight distinct features under one heading:
autosave + crash recovery, device hot-swap, multicore work-stealing, DAWproject export, loudness
metering, time-stretch, full accessibility, and a soak/fuzz harness. Only autosave + crash recovery and
the headless deadline soak are actually built (they are H6's exit criterion). Everything else, plus the
biggest gap of all — **there is no runnable app; `src/Main.cpp` is still the H0 sine spike** — sits
behind that one open-ended horizon.

That structure has no finish lines. The agentic loop runs against *"the current horizon's exit
criterion"*; with everything piled into an "ongoing" H6, there is nothing crisp left to converge on.
This ADR carves the remaining work into numbered horizons **H7–H10**, each with one mechanical exit
criterion, ordered value-first toward a DAW Dan can actually use.

**The UI question.** A single-window timeline UI is the highest user value and is now *unblocked* — the
project/graph/PDC/time/event contracts are frozen and round-trip-tested (H1), satisfying the hard rule
"no mixer-UI / MIDI-editor / plugin-UI before the format and graph+PDC model are frozen." But today the
engine only renders **in-memory**: nothing drives a real audio device through a transport, so a UI would
be a shell around a non-running engine. The UI is therefore sequenced **after** a real playback runtime,
not before it. The UI is also the one horizon whose *visual feel* (layout, 60fps smoothness) cannot be a
mechanical gate — it is the sole human spot-check, on a one-command launch; everything mechanical about
it (agent-native parity, load-a-bundle smoke) still gates in CI.

## Options considered

1. **Leave everything in H6 (status quo).**
   - Pros: no new docs.
   - Cons: no finish lines; the loop has nothing crisp to converge on; "done" stays ambiguous. Rejected.
2. **One catch-all "H7: everything else."**
   - Pros: simple.
   - Cons: not bisectable; mixes a UI shell with a multicore scheduler with file export. Rejected — same
     mistake H6 made.
3. **Carve the backlog into H7–H10, each with a mechanical exit, ordered value-first toward a runnable,
   audible, visible DAW; park the remaining polish features as an explicit post-H10 backlog.** Accepted.

## Decision

Define four new horizons. Each keeps the roadmap's contract: **one testable exit criterion the loop runs
against**, mechanical wherever possible.

- **H7 — Offline render / export to file.** A real offline-render module that bounces a Project to an
  audio file (WAV). *Exit:* the offline render of a Project to a file is sample-accurate vs the RT engine
  path (golden-file compare within tolerance), and the exported file re-imports to an Asset that
  round-trips. Fully headless and mechanical — closeable with no human in the loop. (Also discharges the
  H2-deferred "offline Render/Export" clause and the plan's "golden-file render test diffing RT vs
  offline" — its highest-value DAW test.)

- **H8 — Playback runtime (device I/O + transport).** Wire the engine to a real audio device behind a
  transport (play / stop / locate / loop), and give **recording (H5) and autosave (H6) their first
  production callers** (paying down the "no production caller" debt). *Exit:* a headless transport test
  proves play/stop/locate are sample-accurate against the offline render of the same Project, **and** a
  one-command self-asserting hardware smoke plays a known Project out the real device with zero Underruns
  at a 128-frame Block (the ADR-0005 self-asserting script pattern — this absorbs the still-open H0
  real-hardware soak).

- **H9 — Single-window timeline UI shell.** The first real application window: load a Project bundle,
  draw and scroll the timeline, transport controls, per-track metering — all driving the H8 runtime.
  Requires the still-pending **UI-stack ADR** (native JUCE Components + GPU timeline canvas vs WebView;
  README fork #2). *Exit:* mechanical — an agent-native-parity check (every UI action has an engine /
  command-layer equivalent) plus a headless smoke that the app loads a bundle and starts/stops the
  transport; the GPU timeline holds 60fps while scrolling. *Visual feel is the single human spot-check,*
  via a one-command launch.

- **H10 — Engine scaling & robustness.** The multicore work-stealing scheduler plus soak/fuzz hardening,
  and the cross-horizon debt (H3 worker misbehavior modes + blacklist-on-failure wiring; H4 CP2b MIDI
  auto-wire). *Exit:* the CompiledGraph produces **bit-identical output across 1..N worker threads**
  (determinism gate) under RTSan/TSan, the heavy-session soak holds the deadline with the parallel
  scheduler, and structure-aware fuzzing of the bundle / plugin-state parsers runs clean.

**Post-H10 backlog** (tracked, not yet numbered horizons): loudness metering (libebur128), DAWproject
export (interchange insurance), time-stretch Node (Signalsmith), device hot-swap policy/UI, and the full
accessibility pass. Each lands as its own ADR-backed checkpoint when promoted.

## Consequences

- **Positive:** the loop gets four crisp finish lines; the build order is value-first (export → it plays
  → you can see it → it scales); the H5/H6 "capability not wired" honesty debt is discharged by H8; the
  open H0 hardware soak gets a home (H8); UI is sequenced onto a real running transport instead of a
  hollow shell.
- **Negative / accepted costs:** H9 (UI) is the one horizon not fully autonomous-closeable — its visual
  feel needs Dan's eyes; everything mechanical about it still gates in CI. The polish features
  (loudness, DAWproject, time-stretch, hot-swap, a11y) are explicitly deferred past H10 rather than
  crammed into a numbered horizon.
- **Follow-ups:** write the UI-stack ADR (native JUCE + GPU canvas vs WebView) before H9 code lands;
  write a focused per-horizon plan in `docs/plans/` at each kickoff, as H1–H6 did. This ADR is **Proposed**
  pending Dan's confirmation of the ordering (especially the UI placement at H9).
