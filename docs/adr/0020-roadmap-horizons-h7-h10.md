# 0020. Roadmap extension — horizons H7–H11

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0003 (full multi-track DAW), ADR-0004 (JUCE + own engine), ADR-0005 (mechanical
  verification — CI is the gate), ADR-0012 (SQLite bundle), ADR-0019 (H6 reliability gate),
  [roadmap](../goals/roadmap.md), the build plan, `CONTEXT.md`.

## Context

The roadmap stopped at **H6**, which the build plan labels *"Reliability & polish (ongoing,
long-horizon)"* — a deliberately broad bucket that bundles eight distinct features under one heading:
autosave + crash recovery, device hot-swap, multicore work-stealing, DAWproject export, loudness
metering, time-stretch, full accessibility, and a soak/fuzz harness. Only autosave + crash recovery and
the headless deadline soak are actually built (they are H6's exit criterion). Everything else, plus the
biggest gap of all — **there is no runnable app; `src/Main.cpp` is still the H0 sine spike** — sat behind
that one open-ended horizon, with no finish lines for the loop to converge on.

This ADR carves the remaining work into numbered horizons **H7–H11**, each with one mechanical exit
criterion, ordered so the most value and the most *autonomous* (headless, mechanically-gated) work comes
first, and the single human-eyeballed horizon comes last.

**The UI is the capstone, not an early step.** A single-window timeline UI is the highest user value and
is *unblocked* — the project/graph/PDC/time/event contracts are frozen and round-trip-tested (H1),
satisfying the hard rule "no mixer-UI / MIDI-editor / plugin-UI before the format and graph+PDC model are
frozen." But the UI is a *view over the command/engine layer*, so it is built **after** the full feature
set exists: that way it wires up a complete, stable surface in one pass instead of being re-touched every
time a later feature lands, and the one horizon whose *visual feel* (layout, 60fps smoothness) cannot be a
mechanical gate is the finale rather than a mid-stream interruption. An **audible "it plays" milestone at
H8** gives early end-to-end validation without needing pixels. The UI's mechanical aspects (agent-native
parity, load-a-bundle smoke, 60fps scroll) still gate in CI; only visual feel is the human spot-check, on
a one-command launch.

## Options considered

1. **Leave everything in H6 (status quo).** No finish lines; "done" stays ambiguous. Rejected.
2. **One catch-all "H7: everything else."** Not bisectable; mixes a UI shell with a multicore scheduler
   with file export — the same mistake H6 made. Rejected.
3. **Carve into H7–H10 with the UI mid-stream (H9).** Gets pixels sooner, but the UI then gets re-touched
   as H10+ features land, and the human-eyeballed horizon interrupts the autonomous run. Rejected in
   favour of option 4.
4. **Carve into H7–H11 feature-first, UI as the capstone (H11).** All backend/feature horizons are
   headless and autonomous; the UI wires up the complete set last, once, with agent-native parity already
   proven. Accepted.

## Decision

Define five new horizons. Each keeps the roadmap's contract: **one testable exit criterion the loop runs
against**, mechanical wherever possible. H7–H10 are fully headless/autonomous; H11 is the capstone.

- **H7 — Offline render / export to file.** A real offline-render module that bounces a Project to an
  audio file (canonical 32-bit-float WAV). *Exit:* the offline render of a Project to a file equals an
  **independent** reference render of the same Project within tolerance (golden-file compare — not the
  engine compared to itself), and the exported file re-imports to an Asset whose decoded samples
  round-trip. Fully headless — closeable with no human in the loop. Discharges the H2-deferred "offline
  Render/Export" clause and the plan's highest-value DAW test (RT vs offline).

- **H8 — Playback runtime (device I/O + transport).** Wire the engine to a real audio device behind a
  transport (play / stop / locate / loop), and give **recording (H5) and autosave (H6) their first
  production callers** (paying down the "no production caller" debt). *Exit:* a headless transport test
  proves play/stop/locate are sample-accurate against the offline render of the same Project, **and** a
  one-command self-asserting hardware smoke plays a known Project out the real device with zero Underruns
  at a 128-frame Block (the ADR-0005 script pattern — absorbs the still-open H0 real-hardware soak). This
  is the **audible milestone**.

- **H9 — Engine scaling & robustness.** The multicore work-stealing scheduler plus soak/fuzz hardening,
  and the cross-horizon debt (H3 worker misbehavior modes + blacklist-on-failure wiring; H4 CP2b MIDI
  auto-wire). *Exit:* the CompiledGraph produces **bit-identical output across 1..N worker threads**
  (determinism gate) under RTSan/TSan, the heavy-session soak holds the deadline with the parallel
  scheduler, and structure-aware fuzzing of the bundle / plugin-state parsers runs clean.

- **H10 — Mixing/mastering features & interchange.** Loudness metering (libebur128), DAWproject export
  (interchange insurance), a time-stretch Node (Signalsmith), and device hot-swap survival. *Exit:* each
  lands as its own ADR-backed checkpoint with a mechanical gate (loudness matches the libebur128 reference
  within tolerance; a DAWproject export round-trips through a reference reader; the time-stretch Node is
  sample-accurate vs a golden; a device change mid-session is survived without an Underrun).

- **H11 — Single-window timeline UI shell + accessibility (capstone).** The first real application window:
  load a Project bundle, draw/scroll the timeline, transport controls, per-track metering, the mixer and
  piano-roll surfaces — all driving the H8 runtime and the H7–H10 feature set. Requires the still-pending
  **UI-stack ADR** (native JUCE Components + GPU timeline canvas vs WebView; README fork #2). *Exit:*
  mechanical — an agent-native-parity check (every UI action has an engine/command-layer equivalent),
  a headless smoke that the app loads a bundle and drives the transport, and the GPU timeline holding
  60fps while scrolling. **Visual feel is the single human spot-check, via a one-command launch.**

## Consequences

- **Positive:** the loop gets five crisp finish lines; H7–H10 are all autonomous/headless so the bulk of
  the work needs no human in the loop; the UI wires up a complete, stable feature set in one capstone pass
  (less rework, agent-native parity already proven); the H5/H6 "capability not wired" debt is discharged
  by H8; the open H0 hardware soak gets a home (H8); the audible milestone at H8 de-risks the UI before any
  pixels are drawn.
- **Negative / accepted costs:** no visible application until H11 (mitigated by the audible H8 milestone);
  H11 is the one horizon not fully autonomous-closeable — its visual feel needs Dan's eyes, though
  everything mechanical about it still gates in CI.
- **Follow-ups:** write the UI-stack ADR (native JUCE + GPU canvas vs WebView) before H11 code lands;
  write a focused per-horizon plan in `docs/plans/` at each kickoff, as H1–H6 did (H7's plan lands with
  this ADR). Any feature not reached by H11 (advanced accessibility certification, additional export
  codecs) is tracked as a post-H11 backlog item promoted with its own ADR.
