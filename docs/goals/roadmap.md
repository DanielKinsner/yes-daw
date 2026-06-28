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

> **Status note (2026-06-27/28 adversarial review + render-path build).** SOLID and verified: the
> concurrency spine (lock-free graph swap + janitor reclamation + atomics, RTSan/TSan), the time/PDC/event
> contracts, and the SQLite round-trip (tempo/meter/markers/clips with real negative controls). FIXED this
> pass: the engine now renders audio Clips at their `timelineStart` positions and sums overlaps, gated by a
> reference-checked test (the old RT-vs-offline gate used OscillatorNode sines and compared
> `CompiledGraph::process` to itself — it could not catch a 2x output bug). STILL OPEN against this exit
> text: there is no separate offline-Render module and no project-render golden file (the "golden-file"
> qualifier is aspirational); the asset **decoder -> source-node projection** that would play real imported
> bytes end-to-end is not built; `DecodedClipNode` is not yet exercised under RTSan; open-time validation
> does not cover marker/tempo/meter storage types.

## H2 — Editing-first (the early priority)
Import + copy-to-bundle; async waveform cache; Clip split/trim/move/gain/fades/crossfade as pure
metadata; snap/grid; command/diff undo/redo; offline Render/Export; single-window timeline shell.
**Exit:** any edit sequence + full undo returns the document bit-identical (property test); a
split-with-crossfade Project's RT playback matches its offline Render.

> **Status note (2026-06-27/28 adversarial review + render-path build).** FIXED this pass and now genuine:
> the **property test** is real (seeded randomized edit sequences across all clip+note verbs, full undo ->
> bit-identical, full redo -> edited — it was a hand-coded 21-step array); and the engine now renders a real
> overlapping **crossfade** from clip fade metadata (was pre-baked into the test samples; clips weren't even
> overlapping). STILL OPEN / deferred (claimed-done but not built): the "async waveform cache" is fully
> synchronous; offline **Render/Export to a file** does not exist (rendering is an in-memory loop); the
> **single-window timeline shell** does not exist (`src/Main.cpp` is still the H0 sine spike); the undo
> surface cannot represent structural add/delete-clip edits; the crossfade is linear (equal-power is a later
> refinement). The split-with-crossfade-vs-offline clause is met by the engine render + reference compare,
> not by a separate offline-Render module.

## H3 — Mixer + plugin hosting
Mixer as a graph projection (Fader/Pan/Sum/Send/Return/Meter, solo/mute mask, Sidechain); automation
lanes; out-of-process scanner → VST3 + AU → CLAP hosting behind `PluginNode`; opaque-chunk state.
**Exit:** two parallel paths, one with a real high-latency plugin, stay sample-aligned (PDC impulse
test); pluginval L8–10 + `auval` pass in CI.

> **Status note (per ADR-0015 + the H3 close-out; updated by the 2026-06-27 H3 review).** The blocking CI
> gate is the in-repo **synthetic** host-isolation check (`YesDawHostIsolationCheck`), which spawns the
> real `YesDawPluginHost` worker process. `pluginval` L8–10 / `auval` against real third-party plugins are
> **non-blocking** (gated on runner plugin availability + GPL licensing) — they do not run in CI today.
> Known-open against this exit text, tracked as follow-ups (the adversarial review's findings): the
> "real high-latency plugin / two parallel paths" alignment is currently proven only via the synthetic
> processor (in-process stub, low latency); the worker only runs *passthrough*, so its emit-NaN / fixed-
> latency / hang modes are not yet driven across the real boundary; and **blacklist-on-failure is not
> wired** (a crash/hang recompiles to a placeholder but never persists a blacklist row). The mixer half is
> solid; the plugin-hosting half is real-but-shallow.

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

> **Status note (2026-06-28 H5 close; hardened by adversarial review).** The H5 alignment contract is
> implemented and mechanically gated by `YesDawRecordingCheck`: audio callback input enters a bounded
> FIFO, a writer thread drains to a real temp take file, input+output latency compensation places the
> recorded click back on its Project frame, and the missing-latency negative control is a real
> broken-pipeline run (lands at the wrong frame), not an arithmetic identity. The gate also bites
> punch/loop take ordinals + `maxLoopTakes`, multi-segment comp selection with zero-filled gaps, stereo
> per-channel round-trip, FIFO backpressure accounting, direct-input (input-latency-only) mode,
> take-file format errors, and MIDI timestamp compensation/edges; the audio-path mapping helpers carry
> `YESDAW_RT_HOT` so RTSan enforces nonblocking on them. **Deferred (the "Recording" capability is not
> wired up):** latency-compensated **monitoring** is NOT built; the recording spine has no production
> caller (nothing in the Runtime/audio driver/`Main.cpp`/Project calls it — the gate drives it directly);
> real device latency calibration, device UI/arming, Project bundle take-lane persistence, and the final
> user-facing recorded-audio asset format (the `.ysdtake` file is an internal test format with no
> playback decoder) are all H6+ work.

## H6 — Reliability & polish (exit gate closed; follow-up slices)
Autosave + crash recovery; device hot-swap; multicore work-stealing; DAWproject export; loudness
metering; time-stretch Node; full accessibility; soak/fuzz harness.
**Exit:** a heavy session runs 60 min at a 64–128-frame Block with zero Underruns (99.9th-pct Block
time under the Block period); a hard kill mid-edit recovers to the last autosave with no corruption.

> **Status note (2026-06-28 H6 close; hardened by adversarial review).** The H6 reliability exit contract
> is implemented and mechanically gated by `YesDawReliabilityCheck`: a 100-track synthetic **mixer**
> session built through `GraphBuilder` (each track is a real strip — DC source -> `FaderNode` ->
> `MeterNode`) processes 60 minutes of audio frames at a 128-frame Block (1,350,000 Blocks at 48 kHz),
> records every Block time, and fails unless p99.9 stays under the Block period with zero headless
> Underruns. The same gate writes a bundle-shaped last-good autosave under `autosave/last.yesdaw`,
> simulates a hard kill by abandoning a later edit transaction, restores the autosave, and reopens the
> Project bundle through the normal integrity, foreign-key, asset-file, and semantic validators.
> **Hardened after the review:** the deadline oracle now has a real **negative control** (over-budget,
> underrun, and empty soaks all fail `passesDeadline()` — previously nothing proved it could ever return
> false), and the autosave publish is **crash-safe and fsync'd** (keeps `last.previous` until the new
> snapshot is durable; recovery falls back to it, so the two-rename window never leaves zero valid
> snapshots). **Honest scope:** the "hard kill" is an in-process transaction rollback — OS-level crash /
> hot-WAL recovery is the ADR-0005 hardware soak lane; `underruns == 0` is a headless design choice, not a
> measured device result; and the autosave surface has **no production caller yet** (the gate drives it;
> nothing schedules an autosave or prompts recovery on launch). **Deferred:** final multicore
> work-stealing, DAWproject export, loudness metering, time-stretch, full accessibility, device hot-swap,
> and the self-hosted real-device soak remain follow-up H6 product slices.
