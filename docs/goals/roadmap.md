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
> and the self-hosted real-device soak remain follow-up H6 product slices — **now carved into numbered
> horizons H7–H10 (ADR-0020).**

---

> **Horizons H7–H11 (ADR-0020).** H6 was the build plan's deliberately broad "ongoing, long-horizon"
> bucket; only autosave + the deadline soak are built. ADR-0020 carves the rest into numbered horizons
> with mechanical exit criteria, **feature-first with the UI as the capstone**: build the whole headless
> feature set autonomously (H7–H10), then wire it all into one UI shell (H11). An audible "it plays"
> milestone lands at H8. Each horizon gets a focused `docs/plans/` plan at kickoff.

## H7 — Offline render / export to file
A real offline-render module that bounces a Project to an audio file (canonical 32-bit-float WAV); the
highest-value DAW test (offline render vs an independent reference) becomes real. Fully headless.
**Exit:** the offline render of a Project to a file equals an **independent** reference render of the same
Project within tolerance (golden-file compare — not the engine compared to itself), and the exported file
re-imports to an Asset whose decoded samples round-trip — all green in CI.
**Status (2026-06-28):** closed. ADR-0021 accepted the canonical
float32-WAV format; `OfflineRenderer` renders the current sample-locked Project mixer surface through
`ProjectMixerProjection`; `WavFile` writes/reads pure float32 WAV; `YesDawOfflineRenderCheck` covers
render/reference, codec round-trip, export/import, and negative controls. Local focused gate 1/1 and full
`ci` preset 238/238 are green. Follow-on review/hardening completed; H8 has opened and closed.

## H8 — Playback runtime (device I/O + transport)
Wire the engine to a real audio device behind a transport (play/stop/locate/loop); give recording (H5)
and autosave (H6) their first production callers. **The audible milestone.**
**Exit:** a headless transport test proves play/stop/locate are sample-accurate against the offline
render of the same Project, **and** a one-command self-asserting hardware smoke plays a known Project out
the real device with zero Underruns at a 128-frame Block (ADR-0005 script pattern; absorbs the open H0
real-hardware soak).
**Status (2026-06-28):** closed and hardened. ADR-0022 accepted the absolute-frame transport model; `PlaybackEngine`
plays through `RuntimeAudioDriver`, supports play/stop/locate/loop, drives H5 recording capture, and exposes
the edit tick used by `persistence/PlaybackAutosave.h`. Follow-on hardening added bounded transport
validation, loop/locate parity coverage, and biting recording/autosave controls. `YesDawPlaybackCheck`
passes 9 cases / 271 assertions and the full local `ci` preset passes 239/239. The one-command hardware smoke is tracked as
`tools/playback-smoke.ps1` / `tools/playback-smoke.sh` and build-checked through `YesDawSoak --playback-project`;
it remains an owner-machine smoke, not a CI gate.

## H9 — Engine scaling & robustness
The deterministic scheduled worker executor + soak/fuzz hardening, and the cross-horizon debt (H3
blacklist-on-failure action; H4 CP2b MIDI auto-wire).
**Exit:** the CompiledGraph produces bit-identical output across 1..N worker threads (determinism gate)
under RTSan/TSan, the heavy-session soak holds the deadline with the parallel scheduler, and
structure-aware fuzzing of the bundle / plugin-state parsers runs clean.
**Status (2026-06-28):** closed and remote-green. ADR-0023 through ADR-0027 are accepted. `YesDawSchedulerCheck`
proves transport command queue concurrency, bit-identical scheduled render workers across 1/2/4/8 against
H7 serial offline render, scheduled Blocks through the H6 deadline oracle, seeded bundle/plugin-state fuzz
replay, plugin failure blacklist persistence, MIDI auto-wire with transport locate/loop parity, and the
ADR-0027 block-parallel safety guard that refuses unsafe graphs before scheduled rendering. Full local
`ctest --preset ci --output-on-failure` passes 240/240, and remote CI run `28339991428` is green on
`a5a1db4`. Honest scope: per-node DAG work-stealing inside one live `CompiledGraph` is still a scheduler
deepening, and the live plugin-host coordinator still needs stable plugin-identity plumbing before
automatic blacklist persistence from a child-process failure.

## H10 — Mixing/mastering features & interchange
Loudness metering (libebur128), DAWproject export (interchange insurance), a time-stretch Node
(Signalsmith), and device hot-swap survival — the feature set the UI will surface.
**Exit:** each lands as its own ADR-backed checkpoint with a mechanical gate — loudness matches the
libebur128 reference within tolerance, a DAWproject export round-trips through a reference reader, the
time-stretch Node is sample-accurate vs a golden, and a device change mid-session is survived without an
Underrun.
**Status (2026-06-29):** closed. ADR-0028 is accepted, and
`YesDawLoudnessCheck` is green in local `ctest` **241/241** plus remote CI run `28341446711` on
`1d29c02`. ADR-0029 is accepted; the DAWproject primitive preflight is locally green in
`YesDawDawprojectPrimitivesCheck` with full `ci` preset **242/242**; `YesDawDawprojectCheck` is green with
full local `ci` preset **243/243**, focused H10 lane **2/2**, and remote CI run `28348385319` on
`910ea1c`. ADR-0030 is accepted for an offline-prepared, Signalsmith-backed source-style
`TimeStretchNode`, green on remote CI run `28349381664`; `YesDawTimeStretchCheck` is locally green with
full local `ctest` **244/244** and focused H10 lane **3/3** for landed gates; remote CI run `28350136910`
is green on `ad50721`. ADR-0031 is accepted for a control-side stop/snapshot/rebuild/resume device
hot-swap coordinator around `PlaybackEngine`, with ADR docs green on remote CI run `28351125742`;
`YesDawDeviceHotSwapCheck` is locally green with full local `ctest` **245/245** and focused H10 lane
**4/4**; remote CI run `28351880753` is green on `f9d5a23`. H11 is the next horizon and is not opened by
this H10 closeout. See
[`docs/plans/2026-06-28-h10-mixing-mastering-interchange-plan.md`](../plans/2026-06-28-h10-mixing-mastering-interchange-plan.md).

## H11 — Single-window timeline UI shell + accessibility (capstone)
The first real application window, wiring up the complete H7–H10 feature set: load a Project bundle,
draw/scroll the timeline, transport, per-track metering, mixer and piano-roll surfaces — all driving the
H8 runtime. ADR-0032 chooses native JUCE Components plus a dedicated Timeline canvas, not a WebView main
shell.
**Exit:** mechanical — an agent-native-parity check (every UI action has an engine/command equivalent), a
headless smoke that the app loads a bundle and drives the transport, and the GPU timeline holding 60fps
while scrolling. **Visual feel is the single human spot-check, via a one-command launch.**
**Status (2026-06-29):** opened. ADR-0032 accepts native JUCE Components for the app shell, a dedicated
Timeline canvas for dense rendering, and a UI action registry as the command/keymap/accessibility seam.
The focused plan is
[`docs/plans/2026-06-29-h11-single-window-timeline-ui-plan.md`](../plans/2026-06-29-h11-single-window-timeline-ui-plan.md).
Kickoff docs are green on remote CI run `28382745216`.
