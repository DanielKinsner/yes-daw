# 0015. Plugin hosting runtime: process model, IPC transport, and isolation gates

- **Status:** Accepted
- **Date:** 2026-06-25
- **Deciders:** Dan (owner), build agent (H3 plugin-hosting ADR worker)
- **Related:** ADR-0013 (plugin state chunks + out-of-process hosting decision — this ADR pins the
  implementation its Follow-ups deferred), ADR-0002 (#1 audio thread never blocks, #3 one Node contract),
  ADR-0004 (JUCE for framework surface; own engine), ADR-0006 (ordered publish/swap + off-thread
  reclamation), ADR-0007 (CompiledGraph + PDC validates plugin/IPC latency), ADR-0008 (`PluginNode` is the
  only place `juce::AudioProcessor` exists), ADR-0009 (serializable Event stream), ADR-0012 (`.yesdaw`
  bundle), [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) decisions #11/#12
  and H3, [deepening notes](../plans/2026-06-23-yes-daw-deepening-notes.md) -> *PluginNode*, *H3 - Hosting*,
  and the security review (#4 zero-trust hosting), [research](../research/Building-a-Modern-DAW-Research-2026.md)
  (REAPER process tiers; AUv3 out-of-process default; shared-memory ring IPC; pluginval/auval; crash-test
  plugin), `CONTEXT.md` (PluginNode, Plugin host child, Plugin scanner, Plugin blacklist).

## Context

ADR-0013 decided **out-of-process, sandboxed plugin hosting from the start**: `PluginNode` is a
graph-visible IPC proxy, the audio thread never waits on a plugin child, and plugin state is opaque
chunks. It deliberately left the implementation open ("must choose the shared-memory/ring-buffer details,
exact sandbox mechanism per OS, out-of-process plugin UI embedding, and exact CI plugin fixtures"). Those
choices are hard to reverse — they shape the process topology, the audio↔child seam, the build's layering,
and every host-isolation test — so they need an ADR before hosting code lands.

The forces: the audio thread must read a plugin's output without allocating, locking, or waiting on
another process (ADR-0002 #1); a crashing or hanging plugin must kill only a child, never the app or the
audio stream; the engine target must not link JUCE hosting headers (ADR-0008); and the isolation behaviour
must be provable by a deterministic, dependency-free CI gate.

## Options considered

1. **Hand-rolled child-process management + a single custom IPC channel for both control and audio.**
   - Pros: one transport to build.
   - Cons: a pipe/socket message channel is not RT-safe for per-Block audio (the audio thread would have
     to read a syscall-backed stream); re-implements process spawn/supervision/handshake that JUCE already
     ships as `ChildProcessCoordinator`/`ChildProcessWorker`. Rejected.
2. **Shared "separate-process" pool (several plugins per child) as the H3 default.**
   - Pros: fewer processes, lower per-plugin memory/startup cost.
   - Cons: one crash takes down every plugin in that child; crash/hang attribution is muddier; it is an
     optimisation over, not a prerequisite for, correct isolation. Deferred (kept as a later mode).
3. **Dedicated child process per plugin, managed by JUCE's child-process coordinator, with a separate
   shared-memory ring for RT audio/Events and the message channel for control/scan/handshake.**
   - Pros: maximal isolation and clean crash attribution (one plugin = one PID); reuses JUCE's coordinator/
     worker + watchdog plumbing; keeps the audio thread on lock-free shared memory it never blocks on;
     honours the ADR-0008 layering boundary. Accepted.

## Decision

**Process model: one dedicated `plugin host child` per hosted plugin, via JUCE
`ChildProcessCoordinator`/`ChildProcessWorker`.**

- A single new executable target (working name `YesDawPluginHost`) runs in *worker* mode (selected by
  `commandLineUniqueID`) and is the **only** target that links JUCE hosting
  (`juce_audio_processors` / `AudioPluginFormatManager`, VST3 + AU). The engine and app targets never link
  hosting headers; a layering check (grep/symbol gate in CI) enforces ADR-0008.
- The control-thread **plugin host coordinator** in the main process spawns, supervises, and tears down
  children, owns the message channel, and runs a **watchdog timer**: a plugin that *hangs* never fires
  `handleConnectionLost()`, so on watchdog timeout the coordinator kills the child PID, escalates the
  candidate into the persistent blacklist (ADR-0013), and enqueues a graph recompile that swaps in a
  "plugin crashed" bypass/placeholder node. A child crash (`handleConnectionLost`) takes the same path.
- The same coordinator/worker mechanism backs the **out-of-process scanner** (ADR-0013): scan in the
  worker, keep JUCE's `deadMansPedalFile` in-flight marker plus the persistent blacklist, and cache
  `KnownPluginList` keyed by `{format, canonical_path, file_mtime, file_size}` + discovered UID/version.
- Dedicated-per-plugin is the shipped default; a shared "separate-process" pool stays a later opt-in mode
  behind the same `PluginNode`/IPC seam.

**IPC transport: a two-lane seam — JUCE message channel for control, a shared-memory ring for RT audio.**

- **Control lane** (the coordinator/worker message channel): plugin load/unload, opaque state chunk
  push/pull (ADR-0013), parameter/automation that is not block-critical, latency-change notifications, and
  status/handshake. Never touched by the audio thread.
- **RT lane** (per-`PluginNode` shared-memory region): the only thing the audio thread reads/writes.
  Layout per node: input audio buffer(s), output audio buffer(s), a serializable **Event ring**
  (ADR-0009), and a small control word block (Block index, ready/sequence counters, validated
  plugin-reported latency, status). Bounded and allocated on the control thread at plugin
  prepare/load (`maxBlockSize` × channels × the double buffer); never resized on the audio thread.
- **One-Block pipeline (ADR-0013), made concrete:** the region is double-buffered. Inside
  `PluginNode::process()` the audio thread writes Block `N`'s input into the input ring and reads Block
  `N-1`'s already-published output from the output ring — both are lock-free stores/loads on shared memory,
  the audio thread's **only** IPC contact, with a release-store of the input sequence counter and an
  acquire-load of the output ready counter. It never signals, waits, allocates, logs, does I/O, or
  syscalls. **Waking the child** to process the freshly-written Block happens off the audio thread — either
  the child polls the input sequence counter, or a coordinator I/O thread posts an OS event/semaphore
  (an implementation choice within this invariant, never on the audio thread). The child runs
  `processBlock` under `ScopedNoDenormals` and publishes Block `N`'s output with a release-store of its
  ready counter, which the audio thread acquire-loads the following Block.
- **Fail-open (ADR-0002 / ADR-0013):** if the expected Block's output is not ready within the Block
  budget, the audio thread substitutes last-good output (if valid), then silence, then bypass/placeholder
  after repeated misses — all branch-only, no allocation/lock/log/I-O/wait.

**Latency and PDC reuse the existing compiler (ADR-0007).**

- `PluginNode` reports the one pipeline Block of IPC latency plus the plugin's validated, post-`prepareToPlay`
  latency (scan-time latency is unreliable). Plugin-reported `NodeProperties` are validated before they
  reach the compiler: `latencySamples` clamped to a sane maximum, negatives/impossible values rejected
  with quarantine, channel/Block claims checked — so a plugin reporting `INT_MAX` cannot overflow the PDC
  walk or blow up delay-line allocation.
- Latency-change is a control-thread event: update cached latency, enqueue a **coalesced** recompile
  (rate-limited so a latency-change storm cannot outrun the janitor), publish the next Snapshot. The audio
  thread only ever sees the new `CompiledGraph`.

**Sandboxing: the process boundary + watchdog is H3's isolation guarantee; OS-level hardening is an
additive follow-up.**

- For H3, crash/hang resilience is delivered by the dedicated child process, the coordinator watchdog, and
  the fail-open audio path. A malicious or buggy plugin (including a hostile state chunk processed by the
  plugin's own `setState`) is contained to its child.
- OS-level sandbox hardening (Windows restricted token / job object, macOS App Sandbox + AUv3's native
  out-of-process where available, Linux user namespaces / seccomp) and plugin provenance/signature checks
  tighten the **same** boundary without changing the `PluginNode` or Node contract, and are sequenced as
  follow-ups rather than gating the H3 hosting slice. On macOS, AU is hosted via AUv3's out-of-process
  default where the plugin offers it.

**Plugin editor UI embedding is deferred.**

- H3 hosts plugins **headlessly** (audio + Event processing + opaque state). Out-of-process editor
  embedding (reparenting native child windows / a shared render surface) is the fragile part per the
  research and is not required by H3's audio-correctness/isolation exit gates; it is a later chunk behind
  the same process model.

**CI gates: a deterministic in-repo crash-test plugin is the always-on gate; pluginval/auval gate real
plugins where available.**

- A small **in-repo synthetic test plugin** with selectable behaviours (passthrough, fixed reported
  latency, emit-NaN, hang, crash-on-cue) gives a deterministic, dependency-free host-isolation gate: the
  host must detect, isolate, and recover (last-good → silence → bypass + blacklist + recompile) **within
  the Block budget with zero xruns and never waiting on the child** — this is the H3 hosting exit gate.
- `pluginval` (Tracktion, GPL) is used as an **external binary** at strictness L8–10 (L10 for state
  fuzz / repeated save-restore) and `auval` on macOS, on runners that have plugins — never linked, so a
  CI **license gate** (no GPL/AGPL in the linked binary) stays satisfiable. The VST3 SDK / CLAP headers
  are pinned by commit-hash and the alpha CLAP-loading code lives only in the sandboxed host child.

## Consequences

- **Positive:** plugin hosting reuses JUCE's coordinator/worker + watchdog and the existing PDC/recompile
  machinery; the audio thread stays lock-free on shared memory and fails open; one plugin = one PID makes
  crash attribution and blacklisting clean; the engine target provably never links JUCE hosting; the
  isolation guarantee is provable by a deterministic CI gate independent of any installed plugin.
- **Negative / accepted costs:** a second executable and a shared-memory IPC layer to build and test; at
  least one Block of intentional latency for hosted plugins; per-plugin process memory/startup cost
  (mitigated later by the optional shared pool); OS-level sandbox hardening, plugin UI embedding, CLAP
  hosting, and provenance/signature checks are explicitly sequenced after the H3 hosting slice.
- **Follow-ups:** `CONTEXT.md` gains **Plugin host coordinator** (control-thread supervisor of host
  children) and **Host-isolation gate** (the deterministic crash-test recovery gate) if the terms recur in
  code. Backlog/tests: the `YesDawPluginHost` worker target + layering check; shared-memory RT-lane
  ring + one-Block double-buffer handshake; coordinator watchdog kill→bypass→recompile; latency validation
  + coalesced recompile rate-limit; the in-repo crash-test plugin + host-isolation no-dropout/nonblocking
  gate; pluginval L8–10 / `auval` external gates; the CI license gate (no GPL/AGPL linked) and pinned
  VST3/CLAP SDKs. Exact shared-memory byte layout, the shared-process-pool mode, OS sandbox mechanisms,
  editor embedding, and CLAP remain implementation/follow-up scope.
