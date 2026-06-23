# YES DAW roadmap

> Long-horizon roadmap for a **full general-purpose multi-track DAW** (C++/JUCE + our own engine).
> Each horizon has one **testable exit criterion** — that's the finish line `/loop` runs against.
> Full detail, architecture, and rationale live in the build plan:
> [`docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md`](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md).

Build order: **playable spine first**, then widen. **Editing-first** (recording is later). MIDI is
co-equal in the *model* from H1, with its UI sequenced later. Hard rule (all three research reports):
no mixer-UI, MIDI-editor, or plugin-UI code until the project format and graph+PDC model are frozen
and round-trip-tested.

---

## H0 — Spikes (throwaway)
De-risk the three scariest unknowns before any "engine" exists: device round-trip, a 60fps GPU
timeline, one Node behind the format-neutral trait stub. Decide native UI (recommended) vs WebView.
**Exit:** zero Underruns over 10 min of sine at a 128-frame Block on Windows + macOS, GPU timeline
holding 60fps while scrolling.

## H1 — The spine (lock the irreversible contracts)
RT-safe callback; SPSC command queue + atomic graph-swap + janitor reclamation; `CompiledGraph` with
PDC wired in; multi-track audio playback to the Master bus with metering; SQLite bundle save/load
(schema v1 + migrations). Freeze: graph+PDC, time model, event model, Node contract.
**Exit:** a Project round-trips (tempo/meter map, markers, clips intact), RT path matches offline
Render within tolerance (golden-file), audio path RTSan-clean — all green in CI.

## H2 — Editing-first (the early priority)
Import + copy-to-bundle; async waveform cache; Clip split/trim/move/gain/fades/crossfade as pure
metadata; snap/grid; command/diff undo/redo; offline Render/Export; single-window timeline shell.
**Exit:** any edit sequence + full undo returns the document bit-identical (property test); a
split-with-crossfade Project's RT playback matches its offline Render.

## H3 — Mixer + plugin hosting
Mixer as a graph projection (Fader/Pan/Sum/Send/Return/Meter, solo/mute mask, Sidechain); automation
lanes; out-of-process scanner → VST3 + AU → CLAP hosting behind `PluginNode`; opaque-chunk state.
**Exit:** two parallel paths, one with a real high-latency plugin, stay sample-aligned (PDC impulse
test); pluginval L8–10 + `auval` pass in CI.

## H4 — MIDI editing & instruments
MIDI Clips; Notes flatten to render events; instrument Nodes; piano-roll editing; MIDI-effect Nodes;
MPE voice allocation at the I/O boundary.
**Exit:** note-ons at known offsets land sample-accurately across Block boundaries **and** a tempo
change, through an instrument Node with non-zero latency that PDC compensates.

## H5 — Recording (in scope, possibly user-test-gated)
Audio-thread→FIFO→writer-thread→disk; latency-compensated monitoring; take recording + comping;
punch/loop record; MIDI recording.
**Exit:** a recorded take aligns within ±1 frame of a click reference at non-trivial input+output
latency, against deterministic ground truth.

## H6 — Reliability & polish (ongoing)
Autosave + crash recovery; device hot-swap; multicore work-stealing; DAWproject export; loudness
metering; time-stretch Node; full accessibility; soak/fuzz harness.
**Exit:** a heavy session runs 60 min at a 64–128-frame Block with zero Underruns (99.9th-pct Block
time under the Block period); a hard kill mid-edit recovers to the last autosave with no corruption.
