---
title: "YES DAW — Architecture & Build Roadmap"
type: feat
date: 2026-06-23
status: draft
based_on:
  - docs/brainstorms/2026-06-23-direction-brainstorm.md
  - docs/adr/0002-realtime-engine-foundations.md
  - docs/research/ (3 independent reports + a 10-agent research synthesis)
supersedes: the stem-centric framing in docs/goals/roadmap.md and CONTEXT.md "wedge"
---

# ✨ YES DAW — Architecture & Build Roadmap

## Overview

YES DAW is a from-scratch, **full general-purpose multi-track DAW** (Logic / Pro Tools / Cubase /
Sonar class): audio and MIDI co-equal, linear-timeline core, built in **C++ on JUCE 8** as the
framework with **our own engine and data model** on top. It is a long-horizon ("pie in the sky")
project, planned thoroughly up front so day-to-day work is mostly steady progress and bug-fixing,
driven via `/goals` and `/loop`.

This plan grounds the build in three independent research reports, ADR-0002's five locked engine
invariants, and a 10-agent research synthesis. It defines: the architecture, the irreversible
decisions to lock as ADRs, a layered build order (H0–H6) with one testable exit criterion each, the
testing strategy, the top risks, and the open questions.

**One-sentence architecture:** *the audio thread reads an immutable compiled snapshot of the graph;
everything else is the machinery that produces and retires those snapshots without ever blocking the
audio thread.* Every other decision falls out of that.

## Enhancement summary (deepened 2026-06-23)

A 13-agent deepening pass added production-grade implementation detail, ran review agents, and
researched community agentic-loop workflows. The full depth lives in the companion
[**deepening notes**](2026-06-23-yes-daw-deepening-notes.md). Highlights:

- **10 simplifications adopted** (same scope, fewer mechanisms to build and prove RT-safe): one
  `DelayNode` for PDC + feedback; one per-clip gain-envelope for fades/crossfades/clip-gain; one
  ordered SPSC queue for scalars + topology; one driver + `Clock` for playback/record/monitor/offline;
  one `EntityId` allocator; named lock-free primitives (choc/farbot) instead of hand-rolled.
- **Sample-rate policy added as irreversible decision #14** (was a gap).
- **New cross-cutting risks surfaced** (see deepening notes): cross-file bundle atomicity, plugin
  hosting as a zero-trust boundary, supply-chain pinning, parallel-safety needs an H1 test gate.
- **A proposed agentic-loop workflow** (section below) — community-sourced, gated on our CI.
- **6 conflicts surfaced; the 3 substantive ones now RESOLVED** (2026-06-23): time stored on a large
  fixed grid (**15360 ticks/beat**); stable IDs = **128-bit ULID** (cross-project-unique); plugin
  hosting = **out-of-process / sandboxed from the start**. The 3 housekeeping conflicts were applied inline.

## Problem / Motivation

The owner currently has **no DAW** (can't afford one) and wants a legit one they own end-to-end and
are proud of — and can actually use. They already ship **standalone** Demucs-stemming and
mastering-DSP apps; those stay standalone and individually tuned. The DAW may integrate them **later,
only if clean**, via interfaces (host the mastering DSP as a plugin; call stemming as an external
tool/source node) — never by merging codebases. The DAW itself is **not** a stem/finishing tool.

## Recommended architecture

### Threading & the real-time boundary
- **Two worlds, one wait-free seam.** The **control thread** owns the document, UI, files, and graph
  compilation. The **audio thread** owns only `process()`. They communicate exactly two ways
  (ADR-0002 #5): a typed **SPSC command queue** control→audio (`SetGain`, `AddNode`, `Connect`,
  `LoadPlugin`, `SetClipGain`), and **atomic / triple-buffer latest-value** audio→control (meter
  Level, playhead). One sanctioned mechanism (its own ADR); nothing else crosses.
- **Graph publication is one ordered command, not a polled pointer.** The control thread compiles a
  new immutable `CompiledGraph` off-thread, then enqueues a `SwapGraph{next}` **command in the same
  SPSC queue as the scalar commands** (`SetGain`, `SetPan`, …). The audio thread drains the queue at
  the top of each Block; on dequeuing `SwapGraph` it switches its active graph and pushes the old one
  onto a **preallocated retirement record** for the janitor. Because the swap is *ordered with* the
  scalar commands, a `SetGain` enqueued before it hits the old graph and one after hits the new — no
  command is ever silently applied to the wrong topology. (This supersedes any "release-store a graph
  pointer the audio thread polls" framing — that variant reorders scalar commands against the swap.)
- **Immutable ≠ stateless — split immutable wiring from audio-owned mutable RT state.** The
  `CompiledGraph` is immutable *wiring*: topology, DFS order, connections, buffer-pool layout,
  compile-time params, inserted delay-line nodes. The **mutable RT state** a node owns (delay-line
  ring contents, source/event read cursors, smoother state, feedback buffers, hosted-plugin internal
  state) lives in a **per-run state arena (keyed by node ULID) owned by whichever driver is pumping**, and
  **persists across recompiles by identity**: a surviving node reuses its existing state object in
  place; only genuinely new nodes get fresh state (allocated control-side in `prepare()` *before* the
  swap), and removed-node state is retired through the janitor. The control-thread compiler **never
  reads live RT-mutated memory** — it wires references by ULID. This is how "cache delay-line state
  across a latency change" works without a data race: the node survives the recompile, so its ring is
  the same already-prefaulted object. **State arenas are per run, never global:** live playback and
  each offline Render get their own arena (`prepare()` resets it), so the golden RT-vs-offline test
  starts from clean, comparable state and the two never share history. Under H6 multicore the
  dependency-counter schedule guarantees no two workers touch the same node at once, so per-node state
  always has a single concurrent writer.
- **Janitor = grace-period generation counter.** The audio thread publishes a monotonic `processedGen`
  at end of Block; the janitor (its own low-priority thread) frees a retired graph or node-state only
  once `processedGen` has advanced past the swap — proof the audio thread can no longer touch it. The
  audio thread never `delete`s and never drops a `shared_ptr` to zero. C++ analog of Rust's `basedrop`.
- **One-shot param changes skip recompile.** Gain/pan/clip-gain ride the SPSC queue + per-node
  smoothers; only topology/latency changes trigger recompile-and-swap.
- **FTZ/DAZ per audio thread.** `juce::ScopedNoDenormals` at the top of every callback — non-optional
  from the first callback (on ARM64, denormals produce full-volume noise, not just CPU stalls).

### The graph (we build this — it is the engine)
- **Own `CompiledGraph`, not `juce::AudioProcessorGraph`.** JUCE's graph forces rebuilds onto the
  message thread, has no real lock-free topology swap, no first-class PDC, and couples every Node to
  `juce::AudioProcessor`. The community built `tracktion_graph` *because* `AudioProcessorGraph` was
  inadequate for a real DAW — we reimplement that capability (study it; never link it). Use
  `AudioProcessorGraph` only as a throwaway Stage-0 spike.
- **Two-layer graph.** Editable graph model (control thread) → a background **compiler** producing a
  flat, DFS-ordered Node list with resolved connections, a pre-sized **buffer pool** (sized once at
  compile time — never per-edge allocation), inserted **PDC delay-line nodes**, and a solo/mute mask.
- **PDC at compile time, from day one** (ADR-0002 #2): topo-sort; `pathLatency = max(input
  pathLatencies) + ownLatency`; at every convergence (Bus/`SumNode`, Sidechain input, Return) insert
  a delay line on each shorter path. **Compensate audio, MIDI, and automation** — or notes/params
  drift after a high-latency Node. Built-ins report `latencySamples = 0` initially, but the full
  machinery exists and is tested. Cache delay-line state across recompiles so latency changes don't click.
- **The mixer is the graph, not channel strips.** Track, Bus, Send, Master compile to Nodes; a
  **Send is just an edge** to a Bus's `SumNode`; pre/post-Fader = where the Send sits relative to the
  Fader (Ardour's processor-box model). Solo/mute is a post-compile atomic mute mask. Freeze / flatten
  / export are render *targets* of the same graph, not new subsystems.
- **Single-threaded topo-walk first; design seams for multicore now.** Ship a serial walk (correct,
  shippable); the Node contract already carries per-Node atomic dependency counters and no shared
  mutable state, so a pinned RT-priority work-stealing pool is an *addition* later, not a rewrite.
  Parallel workers must join the platform RT class (macOS Audio Workgroup; Windows MMCSS "Pro Audio";
  Linux `SCHED_FIFO`) or one descheduled worker stalls the graph.
- **Feedback is an explicit one-Block delay Node, never a graph edge** — the DAG forbids cycles.

### The Node contract (we build this — CLAP-shaped)
- **One format-neutral Node interface** (ADR-0002 #3), lifecycle modeled on CLAP:
  `prepare(sampleRate, maxBlockSize)` (alloc here, control thread) → `process(buffers, numFrames,
  events, transport)` (RT, never allocates) → `reset()` → `release()`. Each Node exposes
  `NodeProperties { latencySamples, channels, producesAudio, producesMIDI }`.
- **Built-ins and the future `PluginNode` implement the same interface** — one codebase. **Do not make
  built-ins `juce::AudioProcessor` subclasses** (that couples us to JUCE's param/bus model and
  forecloses the clean contract). `PluginNode` *wraps* a hosted `juce::AudioProcessor` behind our
  interface; it is not our interface.
- **One node codebase, distinct driver paths:** playback (RT, pull from disk-thread ring buffers),
  record (audio thread → lock-free FIFO → writer thread → disk), monitoring (live graph), offline
  Render/Freeze (same `CompiledGraph`, non-RT free-wheel driver). Only the driver/clock differs.

### Time & event model (we build this)
- **Store both domains, always.** Musical position as `int64` ticks/PPQ (canonical, survives tempo
  edits) + derived sample/frame position via the **tempo map**. Freeze a high PPQ now (≥960; 15360
  covers fine groove/MPE) — it's baked into the file format.
- **The tempo map is a first-class bidirectional `ticks↔samples` curve** (constant or ramped
  segments) with a parallel meter map. The **Transport** is the single source of musical time.
- **Per-clip `time_base` flag from clip #1:** tempo-locked vs sample-locked — cannot be reconstructed
  later. Early editing-first Clips are mostly sample-locked, but the flag exists.
- **Events are sample-accurate, block-sliced, event-type-generic** (ADR-0002 #4). One stream carries
  param-changes, notes, CC, per-note expression, each tagged with a sample offset. **MIDI is variants
  on this stream, never a second pipeline.** Fields are MIDI-2.0/UMP-class (float/wide-int, per-note
  IDs) from the start even though MIDI-1 features ship first — widening a `uint8` model later is a rewrite.
- **Two note representations** bridged by the tempo map: `Note` objects (edit model, in ticks) flatten
  to a sample-offset On/Off/CC stream (render model) only at the render boundary.

### Data model & document (we build this; SQLite, not ValueTree, as canonical truth)
- **Three immutable layers:** **Asset** (immutable imported audio, copied into the bundle,
  content-hashed) → **Clip** (`{asset_id, src_offset, length, timeline_pos, gain, fade_in, fade_out,
  curve_type, time_base, warp?}`) → **Project** (tracks/routing/automation recipe). Split/trim/move/
  fade/gain are **pure metadata**; the Asset is never edited in place (hard rule).
- **Canonical store = SQLite Project bundle (`.yesdaw`)**, WAL mode: `project.db` + `audio/` +
  `peaks/` + `plugins/` + `autosave/`. Normalized relational rows for structure (tracks, clips, notes,
  automation, tempo/meter map, markers); opaque BLOBs only for plugin state; media + peaks as files
  (SQLite loses to the filesystem above ~100 KB).
- **ValueTree decision:** do **not** make `juce::ValueTree` the canonical on-disk truth (pulls the
  data model into JUCE, conflicts with the SQLite-bundle decision). We build our own document +
  **command/diff undo**; we study `ValueTree`+`UndoManager` transaction semantics as the reference.
- **Normalized tables, not JSONB** (where the papers disagreed) — queryable, migratable; "JSONB"
  never surfaces to users.

### What we take from JUCE vs build ourselves

| Take from JUCE | Build ourselves |
|---|---|
| `AudioDeviceManager` / `AudioIODeviceCallback` (device I/O: ASIO/WASAPI/CoreAudio/ALSA) — own the callback | `CompiledGraph`, the compiler, atomic-swap + janitor reclamation, buffer pool, PDC, scheduler |
| `AudioPluginFormatManager` / `KnownPluginList` / `AudioPluginInstance` (VST3/AU scan + host) | Format-neutral Node contract; `PluginNode` adapter; out-of-process scanner; CLAP hosting |
| `juce::dsp`, `SmoothedValue` (multiplicative for dB/Hz), FFT, filters | Built-in Node DSP wrappers; fade/clip-gain evaluation in the read path |
| `MidiBuffer`, `MidiMessageCollector`, `MidiMessageSequence` (render/host helpers) | The `Note` edit model, tick timeline, generic event variant, UMP/MIDI-2 plumbing |
| `AudioFormatManager` / readers (Asset import) | Multi-tier persistent waveform peak cache |
| `ScopedNoDenormals`, `AccessibilityHandler` | SPSC command queue + atomic/triple-buffer seam (atop `choc`/`farbot`) |
| Native `Component` + Direct2D/Metal; `OpenGLContext` for the timeline canvas | Viewport-virtualized GPU timeline renderer; SQLite document + command/diff undo |

## Irreversible decisions to lock as ADRs

ADR-0002 locks the five engine invariants. These extend it; each is cheap now, a near-rewrite later.
They become individual ADRs as their milestone is planned (most at H1).

1. **Immutable compiled-snapshot concurrency model** — audio thread reads an immutable `CompiledGraph`
   via atomic swap; retired snapshots freed off-thread.
2. **Own `CompiledGraph` with compile-time PDC over audio + MIDI + automation** — not `AudioProcessorGraph`.
3. **CLAP-shaped format-neutral Node contract** (lifecycle, `process` signature, `NodeProperties`).
4. **Sample-accurate, block-sliced, event-type-generic stream** with per-note (UMP-superset) IDs and
   wide value fields.
5. **Dual time representation** (int64 ticks/PPQ + derived samples) + bidirectional tempo-map curve;
   **PPQ = 15360** (large fixed grid — chosen 2026-06-23, so resolution is never a real limit).
6. **Per-clip `time_base`** (tempo-locked vs sample-locked) in the schema from clip #1.
7. **Asset→Clip→Project non-destructive indirection;** Clips are references, Assets immutable + content-hashed.
8. **Stable persistent IDs** — **128-bit ULID** (never reused, unique across projects; enables
   templates + cross-project paste + clean interchange). Chosen 2026-06-23.
9. **Variable / renegotiable Block size** — `prepare(maxBlockSize)`, `process(numFrames)`; never hardcoded.
10. **SQLite Project bundle as canonical format;** normalized tables; `user_version`+`application_id`
    + migration harness; WAL `synchronous=NORMAL` for autosave, escalating to `FULL` at explicit Save.
11. **Plugin state persisted as opaque chunks** (VST3 component + controller; CLAP `clap.state`; AU
    class-info), never reconstructed from parameter values.
12. **Out-of-process / sandboxed plugin hosting as the shipped default** (from H3): each plugin runs
    in its own process; `PluginNode` is an IPC proxy over shared-memory ring buffers (honoring the
    serializable-seam mandate). **The audio thread never blocks on a plugin child** (ADR-0002 #1 at the
    IPC boundary): a **one-block pipeline** — the host writes block N and consumes the child's block
    N−1 output, the one block of latency reported and PDC-compensated — so the audio side only reads
    what is already in shared memory and never waits. A block not ready by a bounded sub-deadline
    **fails open** (last-good → silence → bypass) and flags the plugin late; the out-of-band watchdog
    kills + blacklists a child that stays late or hung. A plugin crash kills only its process, never
    the project. Chosen 2026-06-23.
13. **f64 summing on Bus mixdown;** internal sample type fixed now.
14. **Sample-rate policy** — project SR, asset-SR-mismatch resampling + quality tiers, mid-project SR
    change. **Lock at H1** (asset import + schema v1 depend on it).
15. **Automation curve representation** — point storage `{tick, value, curve_type}`, the interpolation
    enum (linear / bezier / hold / log), and sample-accurate-vs-block evaluation. **Lock at H1** — it
    rides the frozen event stream, so it is as irreversible as the event model.

## Build order / milestones

Editing-first; recording later; MIDI co-equal **in the model** from H1, its UI sequenced later.
**Sequencing rule (all three papers, adopted): write no mixer-UI, MIDI-editor, or plugin-UI code
until the session format and graph+PDC model are frozen and round-trip-tested.** Our one adaptation:
editing UI before mixer/MIDI/plugin UI; recording after editing.

### H0 — Spikes (throwaway, weeks)
- **Goal:** de-risk the three scariest unknowns before any "engine" exists.
- **Features:** device round-trip via `AudioDeviceManager` (sine → out; load + scrub one WAV) on
  Windows + macOS at a 128-frame Block; a GPU timeline canvas drawing 100+ elements at 60fps incl.
  fractional Windows scaling; wrap one Node behind a stub of the format-neutral trait. Decide
  native-vs-WebView (recommend **native**).
- **Exit criterion:** zero Underruns over 10 min of sine playback at a 128-frame Block on real
  hardware on both OSes, GPU timeline holding 60fps while scrolling.

### H1 — The spine (lock the irreversible contracts)
- **Goal:** thinnest end-to-end path that makes sound from a real Project, all irreversible contracts
  frozen and tested.
- **Features:** RT-safe callback; SPSC command queue + atomic graph-swap + janitor reclamation;
  `CompiledGraph` with **PDC wired in** (all built-ins report 0); buffer pool; FTZ/DAZ. Multi-track
  audio playback `SourceNode → FaderNode → PanNode → SumNode(Master) → device` with `MeterNode`.
  SQLite bundle save/load (schema v1 + migration harness). **Lock:** graph+PDC, time model, event
  model, Node contract. The event stream flows (carrying only param-changes so far) — MIDI dark but
  the pipe is MIDI-capable.
- **Exit criterion:** a saved Project round-trips (tempo map, time signatures, markers, clips intact);
  the RT path matches an offline Render within tolerance (golden-file); the audio path is RTSan-clean;
  **and a kill during save or migration reopens cleanly** (WAL recovery + `integrity_check`),
  structurally and referentially consistent — all green in CI. (Kill-during-import + asset
  DB↔filesystem consistency move to H2 where import exists; autosave recovery is H6.)

### H2 — Editing-first (the early priority)
- **Goal:** non-destructive multi-track editing on imported audio — the owner's primary need.
- **Features:** import + copy-to-bundle (content-hash dedupe); async multi-tier **waveform cache**;
  Clip **split / trim / move / gain / fade-in/out / crossfade** (equal-power default) as pure
  metadata; snap/grid round-tripping exactly through ticks↔samples; **command/diff undo/redo** with
  transaction grouping; offline Render/Export. Stub the take-lane/comping schema (data only).
  Single-window timeline-primary shell with remappable keymap.
- **Exit criterion:** any sequence of edits + full undo returns the document bit-identical
  (property-based test); a split-with-crossfade Project's RT playback matches its offline Render; **and
  a kill mid-import recovers with the bundle's DB↔filesystem consistent** (assets hash-verified, no orphans).

### H3 — Mixer + plugin hosting (onto frozen contracts)
- **Goal:** a real mixer and third-party plugins as projections/adapters over the existing graph.
- **Features:** mixer as graph projection (Fader/Pan/Sum/Send/Return/Meter, SIP solo + solo-safe via
  atomic mute mask, Sidechain as extra input pin); automation lanes honoring per-Block offsets.
  Out-of-process **plugin scanner** (blacklist-on-crash) → **VST3 + AU** hosted **out-of-process** (each in its own process; `PluginNode` = IPC proxy) via
  `AudioPluginFormatManager` behind `PluginNode`, PDC now exercised by real plugin latency → **CLAP**.
  Opaque-chunk state persistence. Begin the accessibility tree. The out-of-process **runtime**
  (detailed at H3-planning) covers: per-plugin shared-memory audio/event ring buffers with
  block-synchronization; out-of-process plugin-UI embedding; crash → bypass-node → recompile, and
  hang-watchdog → kill → blacklist; state save/restore across the IPC boundary; and coalesced
  latency-change handling over IPC.
- **Exit criterion:** two parallel paths, one with a real high-latency plugin, stay sample-aligned
  (PDC impulse test passes against the live plugin); pluginval L8–10 + `auval` pass; **and a plugin
  that crashes or hangs mid-session is isolated — the session survives with a "plugin crashed"
  placeholder and the offender is blacklisted**, all **without an audio dropout** — the audio thread
  fails open within the block budget and never waits on the child (a host-isolation test, not just
  pluginval) — all in CI.

### H4 — MIDI editing & instruments (co-equal surfaces)
- **Goal:** make the co-equal MIDI model user-facing.
- **Features:** MIDI Clips; flatten Notes → render events; route to a built-in or hosted instrument
  Node; **piano-roll editing** (cut/split/move/length/quantize/transpose on Note objects); MIDI
  effects (arp/chord/scale) as event-transform Nodes; MPE voice allocation at the I/O boundary.
- **Exit criterion:** note-ons at known offsets land sample-accurately across Block boundaries **and**
  across a tempo change, through an instrument Node with non-zero latency that PDC compensates.

### H5 — Recording (in scope, possibly user-test-gated)
- **Goal:** capture, as an additional path through the same graph.
- **Features:** audio-thread-writes-FIFO → writer-thread-drains-to-disk; latency-compensated
  monitoring using the round-trip latency the PDC pass already reports; take recording + comping onto
  the H2 schema; punch/loop record. MIDI recording via `MidiMessageCollector` re-timestamping.
- **Exit criterion:** a recorded take aligns within ±1 frame of a click reference at non-trivial
  input+output latency, verified against deterministic ground truth.

### H6 — Reliability & polish (ongoing, long-horizon)
- **Goal:** daily-driver robustness.
- **Features:** autosave + crash recovery (SQLite Online Backup / WAL checkpoint; `integrity_check` on
  open); device hot-swap survival; multicore work-stealing pool (only once a real session stresses one
  core); DAWproject export (interchange insurance, not canonical); loudness metering (libebur128);
  time-stretch via Signalsmith Stretch wrapped as a Node; full a11y pass; soak/fuzz harness.
- **Exit criterion:** a heavy session runs 60 min at a 64–128-frame Block with zero Underruns and a
  99.9th-percentile Block time under the Block period; a hard kill mid-edit recovers to the last
  autosave with no corruption.

## Testing & reliability strategy

Test what's deterministic on **every push**; reserve ears for spot-checks. CI matrix: macOS
(arm64+x64), Windows, Linux (RTSan needs Clang → the RT-safety gate runs on a Clang leg even though
shipping Windows uses MSVC). Clone the **pamplejuce** workflow for platform/signing/notarization.

**Gates that block every push (from H1):**
- **RTSan** (`-fsanitize=realtime`; audio callback + every Node `process()` annotated
  `[[clang::nonblocking]]`) — operationalizes ADR-0002 #1. Plus a debug `operator new` trap in the
  audio scope.
- **Golden-file render test diffing the RT path against the offline Render** of the same Project
  (tolerance-based; one reviewed golden per platform) — the single highest-value DAW test.
- **PDC impulse test (audio + automation + events), from H1** — a known-latency stub Node at a
  convergence point; assert an audio transient, an automation-ramp value, AND a synthetic event all
  land at the predicted compensated sample. The real-MIDI version is the H4 timing test.
- **Save/load round-trip** on the full document (tempo map, meter map, markers, clips, time_base).
- **Property-based undo/redo** (RapidCheck): do→undo == prior; undo-all→redo-all == original.
- **Schema-migration fixtures** — one Project per historical schema version, never deleted.

**Gates added with their subsystem:** pluginval L8–10 + `auval` (hosting); a crash-test Node proving
host isolation + blacklist-on-crash; headless soak harness (zero Underruns over 30–60 min); MIDI
timing test (offsets across Blocks + tempo changes); structure-aware fuzzing of bundle/plugin-state parsers.

## Long-horizon execution via agentic loops (adopted — full)

Community-sourced (Ralph / RPI / verification-wrapped loops; not Anthropic docs). Formalizes the
plan's instinct that `/loop` runs against the current horizon's exit criterion. **Adopted in full
(2026-06-23):** the loop drives every layer that has a CI gate, the engine core included. It activates
at **H1** (when the gates exist); H0 spikes are hands-on.

**Model.** A *bounded* loop (RPI: Research → Plan → Implement + milestone checkpoints), not bare
`while-true`. The loop never decides architecture — the ADRs and this plan are frozen spec; it only
implements toward the current horizon's exit criterion, with the **CI gates as its green condition**.
The fit is good because YES DAW already has hard-to-fake gates (RTSan, golden-file render-diff, PDC
impulse, property-based undo, save/load, migration fixtures) — behavioral oracles, not weak metrics.

**Loop files (version-controlled, re-read every tick):** `loop/PROMPT.md` (short marching orders),
`loop/horizon.md` (current exit criterion + exact green commands), `loop/fix_plan.md` (prioritized,
one-context tasks), `AGENT.md` (per-OS build/test commands), `loop/progress.txt` (append-only
learnings). Same `STATUS.md`-style ledger the cross-machine workflow already uses.

**Per tick:** fresh context → read horizon + fix_plan → pick ONE item → research-then-implement
(scoped diff; touch no ADRs/goldens) → run the gate → commit only if green, repair-before-advancing
if red → update fix_plan + progress → repeat.

**Writer/critic split:** the implementing loop is the writer; a separate critic pass (the planned
custom DAW review agents — real-time-safety, render-correctness) runs on the diff before merge to
catch what gates can't (a `nonblocking` annotation removed to "fix" RTSan, a golden regenerated to
mask a regression).

**Hard stops (all mandatory):** horizon met → stop for human boundary review (**only humans advance
H{N}→H{N+1}**); max iterations; circuit-breaker on 3× same-gate failure or 2 empty diffs; budget
ceiling; immediate hard-stop on any attempted edit to `docs/adr/**`, this plan, a golden file, a
`[[clang::nonblocking]]` annotation, or `git reset --hard`.

**Where it fits (full adoption):** the loop runs every layer that has an automated gate — engine core
included. Its safety net is the **automated critic pass + CI gates + commit-only-on-green +
hard-stops**, not human babysitting. Two honest limits remain: engine-core lock-free code (RCU /
janitor / buffer-pool) is the highest-risk C++, so the critic pass is mandatory there and ASan/UBSan
run on every change; and the **timeline GUI's visual feel has no automated oracle** — the loop builds
and tests its behaviour, but a human still eyeballs "does it look/feel right." Everything functional
still loops.

## Top risks + mitigations

1. **The "flat-chain trap"** (built-in-only chain + fixed Block size + no per-Node latency + no event
   offsets + UI/audio sharing state via mutexes) — individually tempting in an editing-first MVP,
   collectively fatal. **Mitigation:** all five locked as ADRs and CI-gated from H1.
2. **The timeline GUI is the historically fatal subsystem, not the engine** (Meadowlark died on the
   frontend). **Mitigation:** JUCE is the hedge; H0 spikes the GPU timeline first; native Components
   + `AccessibilityHandler`, not WebView, for data-dense surfaces.
3. **Scope overrun on a "pie in the sky" project.** **Mitigation:** playable-spine-first, editing-first
   horizon ordering; one testable exit criterion per horizon; `/loop` runs against the current criterion.
4. **The papers' headline advice contradicts the locked stack** (adopt Tracktion / custom Rust / ours
   is JUCE+own). **Mitigation:** treat all three as *reference designs for the graph we build
   ourselves* (`tracktion_graph` + Rowland's ADC20 talk = the blueprint to study, never link).
5. **Breaking the RT invariant in a way that passes review** (shared_ptr copy on audio thread, page
   fault, ValueTree read, worker at default priority). **Mitigation:** RTSan + `operator new` trap in
   CI; prefault/lock pages in `prepare`; engine reads only published snapshots; workers join the RT class.
6. **PDC silently forgetting MIDI/automation.** **Mitigation:** the latency walk covers all three
   streams from H1; the H4 exit criterion tests note alignment through a high-latency instrument.
7. **WAL durability misunderstanding** (`NORMAL` is not power-loss durable). **Mitigation:** `NORMAL`
   for autosave, escalate to `FULL`/`wal_checkpoint(TRUNCATE)` at explicit Save; `integrity_check` on open.
8. **CLAP hosting is not in JUCE mainline; community modules are alpha.** **Mitigation:** model the
   internal contract CLAP-first (free); ship VST3+AU first; `clap-wrapper` is a zero-code hedge.

## Open questions (resolve during milestone planning)

1. **UI stack ratification (fork #2):** native Components recommended; confirm whether any WebView
   panel is wanted later before the shell calcifies. Also resolves the user-facing chain word.
2. **Bundle/schema specifics (fork #5):** final `.yesdaw` extension + exact schema v1 — ADR before H1.
3. **Plugin format priority:** VST3 + AU (H3) then CLAP; confirm AU is macOS-launch-mandatory, and
   whether LV2/Linux hosting is in long-horizon scope.
4. **Undo persistence:** in-memory command/diff first (H2); journal to SQLite later? (doesn't block H2)
5. **PPQ value — RESOLVED:** 15360 (large fixed grid). See decision #5 / the ADR index.
6. **Negative/pre-roll positions:** is tick 0 = Project start, or are negative positions allowed?
7. **Recording user-test gate:** define the gate's pass condition during planning so H5 isn't blocked.
8. **Doc housekeeping:** revise CONTEXT.md "wedge", the old roadmap, and ADR fork #1 (scheduled — see
   ADR-0003/0004 and the rebuilt roadmap).

## References

**Internal:** [brainstorm](../brainstorms/2026-06-23-direction-brainstorm.md) ·
[ADR-0002](../adr/0002-realtime-engine-foundations.md) · [ADR-0003](../adr/0003-product-full-multitrack-daw.md) ·
[ADR-0004](../adr/0004-stack-juce-framework-own-engine.md) · [CONTEXT](../../CONTEXT.md) · `docs/research/` (3 reports)

**External (study, not link, for the engine):** Ross Bencina — *Real-time audio programming 101*;
Timur Doumler — *Using locks in real-time audio processing, safely* / *Demystifying std::memory_order*;
Dave Rowland — *Introducing Tracktion Graph* (ADC20); `tracktion_graph`; JUCE 8 docs; pamplejuce
(CI template); pluginval; libebur128 (loudness); Signalsmith Stretch (MIT time-stretch);
clap-juce-extensions / clap-wrapper; bitwig/dawproject.
