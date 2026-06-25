# 0013. Plugin state chunks and out-of-process hosting isolation

- **Status:** Accepted
- **Date:** 2026-06-25
- **Deciders:** Dan (owner), build agent (H3 ADR worker)
- **Related:** ADR-0002 (#1 audio thread never blocks, #2 PDC, #3 one Node contract),
  ADR-0006 (ordered publish/swap and off-thread reclamation), ADR-0007 (CompiledGraph + PDC),
  ADR-0008 (`PluginNode` boundary), ADR-0009 (serializable Event stream), ADR-0012 (`.yesdaw`
  bundle reserves plugin-state chunk storage),
  [build plan](../plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) decisions #11/#12 and H3,
  [deepening notes](../plans/2026-06-23-yes-daw-deepening-notes.md) -> *Data model* and *H3 - Hosting*,
  `CONTEXT.md` (Plugin state chunk, PluginNode, Plugin host child, Plugin scanner, Plugin blacklist).

## Context

H3 adds third-party plugin hosting onto the contracts frozen in H1 and proven through H2. Two decisions
must be locked before code lands:

- **Plugin state is not parameter reconstruction.** Real plugins persist internal state that is not
  visible as automatable parameters: sample slots, oversampling buffers, modulation matrices, component
  vs controller state, and vendor-specific data. Rebuilding from parameter values would lose Projects.
- **Plugin binaries are untrusted native code.** A crash, hang, malformed state chunk, impossible latency
  report, or scan-time deadlock must not kill the Project or make the audio thread wait. ADR-0002 still
  applies at the plugin boundary: no allocation/locks/I/O/waits on the audio thread.

ADR-0012 reserved the `plugin_state_chunks` table header. ADR-0008 already says `PluginNode` is the
only place a hosted plugin exists behind the format-neutral Node contract. This ADR fills the missing
H3 contract without choosing every IPC implementation detail.

## Options considered

1. **Persist exposed parameters only; host plugins in-process.**
   - Pros: simplest code path; follows the traditional JUCE host shape.
   - Cons: loses non-parameter plugin state; every plugin runs with full process trust; a crash or hang
     can take down the app or stall audio. Rejected.
2. **Opaque state chunks, but in-process plugin hosting with a watchdog.**
   - Pros: preserves plugin state; less IPC work.
   - Cons: a watchdog cannot safely recover from arbitrary in-process memory corruption, blocking calls,
     or crashes inside the audio process. Rejected for the shipped default.
3. **Opaque state chunks plus out-of-process/sandboxed hosting from the start.**
   - Pros: preserves vendor state; `PluginNode` stays a normal Node to the graph; a plugin crash kills
     only a child process; scanner/runtime hangs are bounded; the audio thread can fail open without
     waiting.
   - Cons: more machinery: child processes, IPC buffers, watchdogs, cache/blacklist persistence, and
     platform-specific sandbox/UI details. Accepted.

## Decision

**Plugin state is persisted as opaque chunks wrapped by host-owned metadata.**

- The bundle stores plugin-provided bytes verbatim, never reconstructed from parameters:
  VST3 component state and controller state, AU class-info/state, and CLAP `clap.state`.
- YES DAW owns the wrapper/header shape reserved by ADR-0012:
  `{format, plugin_uid, plugin_version, chunk_kind, chunk_len, crc32, data}` keyed by
  `(node_id, chunk_kind)`.
- Restore validates `chunk_len` and `crc32` before handing bytes to the plugin child. A corrupt or
  missing chunk loads the plugin at defaults and marks the state unreadable; it must not crash open,
  block the audio thread, or mutate the original bytes in place.
- VST3 component state restores before VST3 controller state.

**Plugin format priority is VST3 + AU first, then CLAP.**

- VST3 and AU are H3's first shipped formats because they cover the commercial plugin baseline on
  Windows/macOS through JUCE's mature hosting surface.
- CLAP follows after that, behind the same `PluginNode` and state-chunk contract. The internal Node and
  Event contracts remain CLAP-shaped, so CLAP support is additive rather than a graph rewrite.
- LV2/AAX or other formats remain out of scope for this ADR.

**The shipped hosting default is out-of-process and sandboxed from the start.**

- Each hosted plugin runs in a plugin host child process. `PluginNode` is the graph-visible IPC proxy,
  not an in-process `juce::AudioProcessor` on the audio thread.
- `PluginNode` communicates through serializable audio and Event buffers. The exact shared-memory,
  ring-buffer, semaphore, and platform sandbox primitives are implementation choices, but the seam must
  remain serializable and bounded.
- The engine target still must not link JUCE hosting headers. Hosting code lives behind the `PluginNode`
  adapter boundary already required by ADR-0008.

**The audio thread never waits on a plugin child.**

- Runtime processing uses a one-Block pipeline: the host writes block `N` to the child side while the
  audio thread consumes the already-available result for block `N-1`.
- `PluginNode` reports that one pipeline Block as latency, plus validated plugin-reported latency, so
  ADR-0007 PDC compensates both. Plugin latency reports are clamped/rejected before reaching the graph
  compiler; negative or impossible values quarantine the plugin.
- If output for the expected block is late or missing by a bounded sub-deadline, audio fails open inside
  the current block budget: use last-good output if valid, then silence, then bypass/placeholder after
  repeated failures. None of those transitions may allocate, lock, log, perform I/O, or wait on the
  child from the audio thread.
- Latency-change notifications are control-thread events. They update cached latency and enqueue a
  coalesced graph recompile/swap; the audio thread only ever sees the next published Snapshot.

**Scanning and runtime isolation are persistent, bounded, and testable.**

- The plugin scanner is out-of-process. A scan crash, lost child connection, or watchdog timeout marks
  the candidate bad without killing the app.
- The scanner keeps JUCE's in-flight recovery marker where useful and also writes a persistent plugin
  blacklist. Runtime crash/hang attribution escalates into the same blacklist.
- The `KnownPluginList` cache key is `{format, canonical_path, file_mtime, file_size}` plus discovered
  plugin UID/version metadata. Path/mtime/size changes require re-scan; identical cache keys skip
  re-scan.
- A child that hangs is killed by a coordinator-side watchdog. A plugin crash or killed child leaves a
  "plugin crashed" placeholder/bypass node and triggers graph recompilation on the control side.

## Consequences

- **Positive:** saved Projects keep real plugin state; plugin binaries are treated as a zero-trust
  boundary; PDC handles plugin and IPC latency through the existing graph; plugin failures degrade to
  defaults/bypass/placeholders instead of app crashes or audio-thread waits.
- **Negative / accepted costs:** H3 must build and test host-child process management, bounded IPC,
  watchdogs, cache/blacklist persistence, and plugin validation gates before plugin hosting is shippable.
  There is at least one Block of intentional latency for hosted plugins.
- **Follow-ups:** implementation must choose the shared-memory/ring-buffer details, exact sandbox
  mechanism per OS, out-of-process plugin UI embedding, and exact CI plugin fixtures. Tests/gates:
  plugin-state header length/CRC/corruption fallback; pluginval levels 8-10 (level 10 preferred for
  state fuzz/repeated save-restore); `auval` on macOS; scanner crash/hang isolation; runtime
  crash/hang -> placeholder + blacklist; host-isolation no-dropout/nonblocking test proving the audio
  thread fails open within the Block budget and never waits on the child.
