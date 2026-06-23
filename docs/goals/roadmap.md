# YES DAW roadmap (pre-brainstorm draft)

> **Status: DRAFT.** This merges the two staged roadmaps in `docs/research/` (Compass + Modern DAW
> Architecture) into a single arc. It is provisional until ADR fork #1 (the product wedge) is
> decided — the wedge reshuffles what belongs in which horizon. Treat exit criteria as the stable
> part and feature lists as negotiable.

Both research roadmaps agree on the *shape*: prove the risky core first (latency, lock-free
messaging, the node/event/thread model), then build outward, and aggressively defer plugin hosting,
MIDI, and time-stretch. The horizons below follow that shape.

---

## H0 — Spikes (validate the scary unknowns)

**Goal:** de-risk the four things that, if they don't work, change everything.
**Exit criterion:** four throwaway spikes run and are measured, with notes captured in `docs/solutions/`.

- Measure `cpal` round-trip latency on Windows (and macOS if targeted) at small buffer sizes.
- Prove a lock-free UI→audio command channel + audio→UI metering channel under load.
- Prove the chosen UI surface can render a **60 FPS scrolling waveform + live meters** from a
  lock-free queue without blocking. *(This is the single most important first unknown per research.)*
- Spike local stem separation (`demucs-rs` / ONNX HT-Demucs) for quality + runtime on real hardware.

**Avoid:** UI polish, any plugin hosting, session format work.

## H1 — Technical prototype (the engine breathes)

**Goal:** a playable multitrack engine built on the foundations both reports insist on.
**Exit criterion:** multitrack audio plays through the node-graph DAG with gain/pan/fades and
metering, the audio thread is **allocation/sanitizer-clean**, and a session round-trips through
SQLite (save → quit → reload → identical).

- Format-neutral **node contract**; DAG graph + layered scheduler; basic mixer/summing.
- Transport/timeline clock; clip↔asset indirection; audio file decode.
- Sample-accurate, block-sliced event model in place (even before MIDI exists).
- Per-node latency reporting + PDC scaffolding (built-ins report zero, but the machinery exists).
- SQLite session storage with autosave; round-trip + crash-recovery tests.

**Avoid:** locking the node/event/thread model in too late; third-party plugins; MIDI piano roll.

## H2 — Alpha (a real editing workflow)

**Goal:** non-destructive editing + offline render + the first AI workflow.
**Exit criterion:** import audio → edit non-destructively → run local stem separation → bounce an
offline render that matches a golden file within tolerance.

- Recording, monitoring, track arm/solo/mute; non-destructive split/trim/move; fades; snap/grid.
- Async waveform cache; undo/redo; crash recovery hardened.
- Offline bounce path with golden-file render tests.
- Local stem separation integrated into the timeline.

**Avoid:** MIDI, third-party plugin hosting, time-stretch/warp.

## H3 — Private beta (the wedge becomes the product)

**Goal:** the differentiated workflow (decided in ADR #1) feels like a product, not a tech demo.
**Exit criterion:** a user can take a real track through the full wedge workflow end to end
(e.g., import → stem split → rebalance/repair/arrange → AI-assisted master → export).

- AI mastering/finishing assistant (analyze → suggest → user overrides everything).
- Templates/presets; large-session performance; device hot-swap hardening.

**Avoid:** scope expansion beyond the wedge.

## H4 — Plugin hosting (decided by ADR #4)

**Goal:** host third-party plugins without the wedge ever crashing the whole app.
**Exit criterion:** CLAP plugins load, process sample-accurately, persist state, and survive a
plugin crash gracefully; `clap-validator` passes in CI.

- CLAP-first via `clack`; in-process+watchdog or out-of-process per ADR #4; then VST3.
- PDC proven with real high-latency plugins; sidechains.

**Avoid:** AU and full sandboxing unless the market demands it; MIDI instruments.

## H5 — v1

**Goal:** a polished, stable release of the wedge workflow with a migratable session format.
**Exit criterion:** stable session format with migrations; CLAP + VST3 hosting; the wedge workflow
is genuinely good, Windows (+ macOS if targeted).

**Avoid:** DAW-clone feature-count competition; video; surround; control surfaces.
