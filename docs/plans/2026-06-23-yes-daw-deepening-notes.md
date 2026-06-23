<!-- Companion to 2026-06-23-feat-yes-daw-architecture-roadmap-plan.md. Implementation depth from the 13-agent deepening pass, 2026-06-23. -->

# YES DAW — Deepening Package (Research + Reviews Consolidated)

This package adds depth to the existing plan. It does **not** restate or remove existing content. Every enhancement keys to a header already in `docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md`. Scope-reduction suggestions are quarantined in their own section and must not be applied. The full ambitious scope is preserved throughout.

---

## Per-section enhancements — keyed to the plan's existing headers

### → `### Threading & the real-time boundary`

**Research Insights to ADD:**

- **Make "blocking" a type-system property, not a discipline.** Annotate the device callback and every Node `process()` with `[[clang::nonblocking]]`; build a Clang leg with `-fsanitize=realtime` **and** `-Wfunction-effects -Werror`. Function Effect Analysis is the *static* gate (compile error on any transitively-blocking call); RTSan is the *dynamic* backstop. Run both — complementary, not redundant. Mark the few intentionally-blocking control-side helpers `[[clang::blocking]]` so the analyzer's call-graph stays honest. Wrap the attribute in a macro — MSVC ignores it, and shipping Windows is MSVC.

- **Name the lock-free primitives in the ADR, do not hand-roll three.** Control→audio = `choc::fifo::SingleReaderSingleWriterFIFO<Command>` (ISC, header-only). Audio→control single scalar (meter Level, playhead samples) = one `std::atomic<float>`/`std::atomic<int64_t>`, `store(release)`/`load(acquire)`. Audio→control **multi-word coherent snapshot** (playhead `{ppqPos, barBeat, isPlaying}`) = a **SeqLock** (writer bumps even→odd→even around the write; reader retries on odd/changed) or a triple-buffer. SeqLock is wait-free for the audio-thread writer; reader spin-retry is fine on the UI side.

- **One ordered seam carries both scalars and topology.** Keep `SwapGraph{const CompiledGraph* next}` as a *variant in the same SPSC queue* as `SetGain`/`SetPan`. Two queues (topology vs scalars) reorder: a `SetGain` targeting a node that only exists post-swap gets applied to the old graph and silently dropped. One ordered queue makes correct ordering free — this is exactly why the ADR says "one sanctioned mechanism."

- **The audio thread reads, never writes lifetime.** One `acquire`-load of `std::atomic<const CompiledGraph*>` per Block. Never `delete`, never drop the last reference, never allocate. A raw `atomic<const CompiledGraph*>` is lock-free on every platform; `std::atomic<std::shared_ptr<>>` is lock-free on almost none (libstdc++/MSVC fall back to a spinlock table → priority inversion) and a `load()` bumps a refcount whose decrement-to-zero runs a destructor on the audio thread. Use the raw pointer + janitor; this is the precise "passes review, glitches under load" bug.

- **Janitor = grace-period generation counter (start here, simplest correct).** Audio thread publishes a monotonic `processedGen` at end of Block (`store(release)`). Janitor (low-priority **own thread**, ~20 Hz — *not* a `juce::Timer` on a possibly-blocked message thread) frees a retired snapshot only once `processedGen > retiredAtGen`. That is RCU's quiescent state: the audio thread has completed ≥1 full Block on the new graph, so it provably can't touch the old one. Escalate to a `farbot::RealtimeObject`/`choc` deferred-release "zombie" queue only when you have *many* small retirements (per-clip/per-param), not just whole-graph swaps. Hazard pointers (C++26 `std::hazard_pointer`) are overkill for a single reader.

- **Publish ordering is release/acquire — not relaxed, not seq_cst.** `release` on the store / `acquire` on the load is the entire RCU contract: every write building `next` (buffer pool, delay lines, params) is visible the instant the audio thread sees the new pointer. `relaxed` lets the audio thread observe the pointer before the writes that built the graph land (half-constructed pool). `seq_cst` everywhere is correctness-safe but needlessly slows the per-Block load.

- **FTZ/DAZ is thread-local and, on ARM64, NOT inherited by child threads.** Set `juce::ScopedNoDenormals` at the top of *every* callback and on the first action of *every* audio/worker thread (the device thread and every H6 work-stealing worker). On Apple Silicon, missing flush-to-zero is **full-volume digital noise**, not a CPU stall. Reverb tails, IIR filters, and feedback-delay nodes generate denormals even when latency is reported zero. Smoothers decaying toward 0 should also snap to 0 below a threshold so downstream nodes see clean zero.

- **Bound everything explicitly.** Queue depth, recompile-swap rate, janitor backlog. Overflow policy is decided in advance: coalesce repeated `SetGain` to the same target into the latest value before push, or backpressure the *control* thread — never the audio thread. A growing retired-graph list is a **bug signal** (audio thread stalled, not advancing `processedGen`) — surface it, don't silently grow.

- **References:** RealtimeSanitizer / Function Effect Analysis (clang.llvm.org/docs/RealtimeSanitizer.html); `realtime-sanitizer/rtsan` (`ScopedDisabler`); `hogliux/farbot` (`fifo`, `RealtimeObject`); `Tracktion/choc` (`SingleReaderSingleWriterFIFO`); Micah Johnston — *Basedrop*; Timur Doumler — *Using locks in real-time audio processing, safely* / *Demystifying std::memory_order* (ADC 2025); Charles Frasch — *SPSC Lock-free FIFO from the Ground Up* (CppCon 2023); Mixxx #16126 (ARM64 FTZ full-volume noise); Intel FTZ/DAZ flags; Arm FPCR bit 24; Erik Rigtorp — *Latency implications of virtual memory*.

---

### → `### The graph (we build this — it is the engine)`

**Research Insights to ADD:**

- **The compiler is a 5-pass transform; specify complexity now.** (1) Flatten & assign stable IDs (a Send materializes as an *edge* to a Bus `SumNode`; pre/post-fader = tap index vs FaderNode). (2) **Iterative** DFS/topo order from Master backward (explicit work-stack — recursion blows the stack on deep bus trees); detect any non-`DelayNode` back-edge as a compile error. (3) PDC longest-path. (4) Buffer-pool liveness/slot assignment. (5) Mute mask + delay-state carry-over + publish. State each pass's bound: the whole pipeline is **O(V+E)**, never path-enumeration.

- **PDC is a convergence property — single-pass longest-path, never path enumeration.** In topo order: `pathLatency[v] = max over inputs u (pathLatency[u]) + ownLatency[v]`. At each convergence node `c` (≥2 inputs — Sum, sidechain consumer, Return): `target = max(pathLatency[u])`; for each shorter input `u`, splice a `LatencyNode(target − pathLatency[u])` onto edge `u→c`; re-stamp `pathLatency[c] = target + ownLatency[c]`. Single-input chains get **no** delay — latency just accumulates. This is exactly Tracktion's `SummingNode::createLatencyNodes` / `subtractNoWrap`. Enumerating source→sink paths is exponential in merge points and is the classic PDC performance trap.

- **PDC delay node and the explicit feedback delay node are ONE `DelayNode` primitive**, parameterized by sample count — PDC inserts it with a computed value, feedback uses a fixed one-Block value. One implementation, one test, one RT-critical component to harden.

- **Buffer pool: greedy liveness/interval allocation, sized to graph *width* not edge count.** Linear-scan the topo order tracking each buffer's live range (first write = producer; last read = last consumer); a slot returns to the free list when its last consumer runs (reuse JUCE's `isBufferNeededLater` idea against our pool). Pool size ≈ peak concurrent live slots (a few dozen for a 200-node session), not O(edges). **State the aliasing/lifetime contract explicitly**: when in-place processing is permitted, and that no pooled buffer is reused while a reader is still pending. The sidechain caveat (a long sidechain edge keeps its source buffer live across a long span) is handled automatically *iff* the sidechain is a real edge before Pass 3 — never special-case sidechains out of latency analysis.

- **Cache delay-line *state* across recompiles.** On a plugin latency change, carry the old `LatencyNode` ring-buffer contents into the new node keyed by stable node ID, or the path clicks/dropouts. The next `CompiledGraph` reuses already-prefaulted delay memory so a latency change neither clicks nor page-faults.

- **f64 summing at every Bus** (decision #13): accumulate into a `tempDoubleBuffer`, narrow to `float` on output — `SummingNode`'s pattern; the cost is one buffer, invisible against plugin cost.

- **`CompiledGraph` is flat, contiguous, immutable-after-publish.** `std::vector<CompiledNode>` in DFS order (just iterate — no scheduling logic on the audio thread), flat `edges[]`, `BufferPoolLayout`, `std::vector<DelayLine> pdcDelays`, `std::atomic<uint64_t> muteMask` (flipped without recompile), `int totalLatency` reported to Transport for record/monitor compensation.

- **Empty graph / first-callback safety:** `published` starts `nullptr`; the callback outputs silence and returns until the first install. The compiler must emit a valid silence-outputting graph for a project with no tracks.

- **Reference designs to STUDY, never link** (license + Risk #4): Dave Rowland *Introducing Tracktion Graph* (ADC20) — node contract + compile-to-flat-list + per-node latency; `tracktion_graph` source (`tracktion_Node.h`, `tracktion_LatencyNode.h`, `tracktion_SummingNode.h`, `tracktion_LockFreeMultiThreadedNodePlayer`). The actually load-bearing blueprint is narrow: **Rowland's graph + Doumler's two RT-comms patterns** — treat Tracktion-Engine-adoption and Rust designs as explicitly out-of-reference.

- **References:** JUCE `AudioProcessorGraph` (`isBufferNeededLater`, render-sequence reuse — for the *idea*, not the class); JUCE `AbstractFifo` (indices-only — you own storage and the two-region wrap copy); Ardour Manual — Latency & Latency-Compensation / Signal flow (processor box).

---

### → `### The Node contract (we build this — CLAP-shaped)`

**Research Insights to ADD:**

- **`process()` is `[[clang::nonblocking]]`; `prepare()` is the only place a Node allocates.** Reference `NodeProperties` shape: `{ producesAudio, producesMIDI(=produces events), channels, latencySamples (int64), id }`. `ProcessArgs { AudioBlock audio; EventStream& events; const Transport& transport; int32 numFrames(≤maxBlock); }`. Interface methods: `properties()`, `directInputs()`, `prepare(sr,maxBlock)`, `process(args)`, `reset()`, `release()`, plus a compiler-only `processedOutput()` (where this node's output landed in the pool this block).

- **`PluginNode` is the ONLY place `juce::AudioProcessor` exists — enforce with a layering test.** The engine target must not link the hosting headers. `prepare()` calls `setRateAndBufferSizeDetails` + `prepareToPlay`, sizes scratch buffers, and caches latency *after* `prepareToPlay` (plugins allocate look-ahead lazily — the scan-time latency is a lie). `process()` translates our event stream ↔ `MidiBuffer` and runs `processBlock` under `ScopedNoDenormals`. Latency-changed → `AudioProcessorListener::audioProcessorChanged` on the control thread → update cached latency → enqueue `RecompileGraph`. The audio thread only ever sees the new `CompiledGraph`.

- **Per-block evaluation is a Node-contract rule, not a suggestion.** Fades, clip-gain, pan laws, automation: evaluate **per-block with a ramp/smoother** or via lookup tables — never `std::cos`/`std::pow` per frame in any read path. Use `juce::SmoothedValue<float, Multiplicative>` for dB/Hz (perceptual ramp; clamp the floor — multiplicative can never reach exactly 0), `Linear` for pan and the fade `x` ramp. Make this a code-review/debug-check gate.

- **Validate plugin-reported `NodeProperties` before they reach PDC/pool** (security + robustness): clamp `latencySamples` to a sane max (a few seconds), reject negatives, use checked/saturating arithmetic in the `pathLatency` walk; validate channel-count and block-size claims against the `prepare()` contract; reject out-of-range with quarantine. A plugin reporting `INT_MAX` latency otherwise blows up delay-line allocation or overflows `pathLatency`.

- **One driver, one clock interface, two clock implementations.** Playback/record/monitor/offline-render are *not* four drivers: they are one `process`-pump driven by a `Clock` that is either device-callback-paced (RT) or free-wheeling (offline), plus a capture-FIFO flag (record) and live-input nodes (monitor). This is *why* the golden-file RT-vs-offline test stays honest — same driver, swapped clock — and the offline path **is** the shipping Export/Render path (nothing to drift to).

- **References:** CLAP overview & threading/event model (`free-audio/clap`); JUCE `AudioProcessor` (`get/setStateInformation`, `setLatencySamples`/`ChangeDetails::withLatencyChanged`); JUCE `SmoothedValue` Multiplicative.

---

### → `### Time & event model (we build this)`

**Research Insights to ADD:**

- **Canonical position is `int64` ticks — never float beats** (float accumulates rounding over long projects and breaks bit-exact round-trip). At 960 PPQ, `int64` overflows past ~9.6M bars (non-issue). Keep a render-time `MusicalTime { int64 tick; double frac; }` where `frac∈[0,1)` is populated only at flatten/render for groove/warp/MPE — sample-accuracy comes from `double` math at the sample boundary, **not** from a finer integer store grid. (See Conflicts: PPQ-freeze framing.)

- **Tempo map: exact closed-form ticks↔samples per segment — never iterate.** Constant (Jump) segment: `samples(tick) = startSample + (tick−startTick)·(60·sr)/(bpm·PPQ)`. Ramp segment (BPM linear in tick ⇒ time logarithmic): with `k = 60·sr/PPQ`, `m = (endBpm−startBpm)/D`, `Δsamples(x) = (k/m)·ln((startBpm + m·x)/startBpm)`; invert via `bx = startBpm·exp(dS·m/k)`. Degenerate `m≈0` → linear branch. **Precompute cumulative `startSample` per segment boundary (prefix sum); lookup is O(log n) binary search.** This is the single highest-leverage perf fix in the time layer — naive per-event linear scan makes note-flatten O(notes×segments). The meter map is parallel and independent: it changes bar/beat labels and snap grid only, never `tick↔samples`.

- **One generic event struct, MIDI is a variant** (modeled field-for-field on CLAP's family). `Event { uint32 timeInBlock (sample offset); EventType type; uint16 flags; VoiceAddress voice; union payload; }`, trivially-copyable, fixed-size, sorted ascending by `timeInBlock`. `VoiceAddress { int32 noteId; int16 portIndex, channel, key; }` (−1 = wildcard, PCKN superset). **Velocity and all controllers are normalized `double`, not `uint8`/`uint16`** — this *is* the MIDI-2 widening (16-bit velocity, 32-bit CC represent losslessly; MIDI-1 maps `vel7/127.0`, UMP maps `vel16/65535.0`). `pitchNote` is continuous semitones, separate from `key` (decouples played key from sounding pitch for MPE/microtuning). **SysEx never inline** — store `(offset,length)` into a per-block side buffer to keep `Event` fixed-size. Note-expression IDs mirror CLAP (Volume/Pan/Tuning/Vibrato/Expression/Brightness/Pressure).

- **Per-source event cursors, not per-block re-scan.** Each automation lane / MIDI sequence holds a **monotonic read cursor** advanced as the transport moves forward → per-block cost O(events-in-block), not O(all-events). On locate/seek/loop-wrap, re-seek all cursors by binary search (O(log n) per source, once per discontinuity). The compiler pre-resolves which sources target which node, so per-block merge is a k-way merge of small k, not all events.

- **Note edit-model ↔ render-stream bridge is one-way, at the render boundary, through the tempo map.** A note that starts before the current block must NOT emit a fresh `NoteOn` (track active flattened voices by `(clip, note.id)` across blocks, preserving `noteId` — otherwise instruments machine-gun-retrigger at every block boundary). On/off ticks convert to samples through the segment-aware map so a note crossing a tempo ramp gets monotonically-increasing offsets. **PDC shifts the event stream by the same per-path latency as audio.**

- **Edge cases to encode now:** event exactly on a block boundary belongs to the *next* block (half-open `[0,numFrames)`); tempo change mid-block — convert each event through the full map, don't assume constant samples/frame; zero-length note → on+off same sample (or 1-frame min); note crossing a loop boundary → emit a clean Off at the seam, never a hung voice; coalesce MIDI-1 14-bit CC MSB/LSB and RPN/NRPN at the input boundary into one normalized event.

- **JUCE classes are boundary adapters only:** `AudioPlayHead::PositionInfo` (all getters `Optional` — check; our Transport is authoritative) for interop; `MidiBuffer` (timestamp = samples-from-block-start, identical to our `timeInBlock`) only at the hosted-plugin boundary; `MidiMessageCollector` re-timestamps live input at the H5 recording boundary; `MidiMessageSequence` for SMF import/export only — never the edit model.

- **References:** CLAP `events.h`; MIDI 2.0/UMP spec (16-bit velocity, 32-bit controllers, per-note controllers); *The State of MIDI 2.0* (Feb 2026); PPQ background (960 / Reason 15360 / SMF division); Cubase Jump-vs-Ramp; Ardour tempo/time-signature linear ramp; KVR tempo-automation thread (piecewise integrate); JUCE `AudioPlayHead::PositionInfo` / `MidiMessageCollector`.

---

### → `### Data model & document (we build this; SQLite, not ValueTree, as canonical truth)`

**Research Insights to ADD:**

- **Bundle = package directory; only `project.db` is transactional.** Register as a macOS bundle via `LSItemContentTypes`/`CFBundleDocumentTypes`; plain folder elsewhere. Freeze `application_id` once (e.g. `0x59455331` "YES1") and use `user_version` as the monotonic schema version. Connection bring-up order matters: `journal_mode=WAL` first, then `synchronous=NORMAL` (live) / escalate to `FULL` + `wal_checkpoint(TRUNCATE)` at explicit Save, `foreign_keys=ON`, `busy_timeout=5000`, `wal_autocheckpoint=1000`, `cache_size=-16384`, `temp_store=MEMORY`.

- **Cross-file atomicity is NOT covered by SQLite ACID — add an intent log + reconcile-on-open.** Every asset/BLOB write is **file-before-row**: stage to a temp name in the *same directory* (atomic rename), `fsync` file, `fsync` dir, rename to final (`<hash>.<ext>`), then `COMMIT` the referencing row. Record intended FS ops in a `pending_fs_ops` table inside the same transaction; on open, replay/rollback `committed=0` ops and sweep orphans. GC never hard-deletes — move unreferenced assets to `.trash/`, swept only on explicit "Clean up project." This closes the import/delete/migration crash windows.

- **Durability contract, stated honestly.** `synchronous=NORMAL` + WAL survives a *clean process kill* but **not power loss** (rolls back to last checkpoint). Add a periodic durability barrier (every N s or N txns: `wal_checkpoint(PASSIVE)` + a `FULL` commit) so autosaved work becomes power-loss-durable on a bounded cadence, not only at explicit Save. On macOS, explicit Save must issue **`F_FULLFSYNC`** (plain `fsync` does not reach platter/flash on Apple hardware) — document it so it's never "optimized" away. Distinguish "autosaved (crash-recoverable)" from "saved (power-loss-durable)" in the UI.

- **Real referential integrity from schema v1.** Declare `FOREIGN KEY` constraints + `PRAGMA foreign_keys=ON` (the research schema sketch has none — retrofitting FKs onto populated shipped files is a painful migration). Layer integrity checks: fast `quick_check` on open + background full `integrity_check`; then **your own** referential checks (no orphan `media_id`/`lane_id`/`node_uuid`), semantic invariants (`clip.src_offset+len ≤ asset.frames`; monotonic finite tempo map — reject NaN/inf/non-monotonic; DAG acyclicity with depth/size cap *before* topo-sort; `time_base` enum range; positive PPQ/block/track counts). `integrity_check` validates B-tree structure only — a structurally-perfect DB can still be semantically corrupt.

- **Migrations: numbered, transactional, append-only — and copy-on-migrate.** One `Migration{toVersion, apply}` per change; `BEGIN IMMEDIATE` → apply → insert into `schema_migrations(version, applied_at_utc, app_build)` → bump `user_version` as the **last** statement → `COMMIT` (version flips only if everything succeeded). **Never edit/delete a shipped migration; never drop columns/tables in the v1 era (add-and-deprecate).** **Refuse-forward:** `user_version > CODE_SCHEMA_VERSION` → open read-only ("made by a newer version"). File-moving migrations route through the intent log; migrate a *copy* (Online Backup), validate it (`integrity_check` + referential), then atomically swap — the original is untouched until the new one is proven good.

- **Content-hash media with an explicit GC/verify contract.** SHA-256 over source bytes → `audio/<hash>.<ext>`, `INSERT OR IGNORE` dedupes. Copy to temp, **re-hash, then rename** (a truncated copy whose name implies a valid hash otherwise passes forever); verify-on-open lazily and re-import on mismatch. One asset backs many clips → deletes go through the undo/command layer (capture rows first), not raw FK cascades, during interactive editing.

- **Plugin state = opaque chunks, with a header you control.** Store VST3 component **and** controller chunks (restore component-first), AU class-info, CLAP `clap.state` verbatim — never reconstruct from parameters (loses oversampling buffers, sample slots, mod matrices). Wrap each chunk `{format, plugin_uid, plugin_version, chunk_kind, chunk_len, crc32}`; verify length+CRC before handing bytes to the plugin (PK `(node_uuid, chunk_kind)`). A corrupt chunk becomes "plugin state unreadable, loaded at defaults," not a crash.

- **`SmoothedValue` Multiplicative cannot reach 0** — a fader dragged to −∞ dB hangs just above silence; detect the floor and hard-switch to linear-to-zero at the bottom.

- **Autosave via Online Backup API**, not file copy (transactionally consistent with the DB open). Prune to N newest; on launch, if `project.db` fails `quick_check`, open the newest passing autosave. Recovery must tolerate a torn WAL tail (recover to last good checkpoint).

- **DAWproject is lossy interchange, never a save target** (ZIP of `project.xml`+`metadata.xml`+media via `juce::ZipFile::Builder`, audio entries stored not compressed). `.yesdaw → .dawproject → .yesdaw` will not preserve PDC cache, warp specifics, or the native node graph.

- **References:** SQLite *As An Application File Format* / WAL / *Atomic Commit* / *Online Backup API* / PRAGMA docs; avi.im (2025) & agwa — SQLite durability/fsync; user_version migration patterns; DAWproject (bitwig); VST3 Persistence; JUCE `SHA256`/`MemoryBlock`/`ZipFile::Builder`/`MemoryMappedFile`.

---

### → `## Build order / milestones` (cross-milestone insights)

**Research Insights to ADD (place under the relevant Hn):**

- **H2 — Editing/undo:** Undo is **command + diff** (named intent for menu/redo + exact row before/after for bit-identical reversal), not one or the other. Coalesce only same-verb/same-target/within-gesture (a 60-event fader drag → one transaction); a new verb or a gap closes the transaction; **never mix undoable and non-undoable mutations** (peak writes, autosave, meter updates bypass the recorded path). Apply/undo runs inside one SQLite transaction. **Split is the canonical edit** — `right.srcOffset == left.srcOffset + left.srcLength` exactly (assert in the unit test); every new cut edge gets a 2–5 ms equal-power declick fade. **Fades/crossfades/clip-gain are ONE per-clip gain envelope** evaluated at frame position, differing only by curve; a true crossfade drives both sides from one shared `x` ramp with fade-out `= 1 − fadeIn(x)` (independent ramps comb-filter). Equal-power default uses the Signalsmith cheap polynomial (`f(x)=[x(1−x)(1+1.4186·x(1−x))+x]²`, ~9 ops, no `sin/cos`).

- **H2 — Snap/peaks:** Snapping is integer tick math (`snapTick`), exactly reversible; never store a snapped sample/pixel as truth — recompute px↔tick each frame. Waveform draw is **O(visible pixels)** via a min/max+RMS mipmap pyramid (build tier 0 once streaming the asset, fold 16:1 in memory for higher tiers — never re-read the asset per tier), keyed by asset content-hash in `peaks/`, one shared background thread, LRU for on-screen tiers, mmap cold. Deleting `peaks/` is always safe. Zoomed past tier 0 → read raw samples for that small window.

- **H3 — Hosting:** Out-of-process **scanner** from day one via `ChildProcessCoordinator`/`ChildProcessWorker` (one executable, worker mode via `commandLineUniqueID`); a lost connection mid-scan = blacklist. Keep JUCE's `deadMansPedalFile` (in-flight recovery) **plus** a persistent blacklist table (known-bad). Add a coordinator-side **watchdog timer** — a plugin that *hangs* never fires `handleConnectionLost()`; on timeout, kill the child PID and blacklist. Persist `KnownPluginList` keyed by path+mtime+size to skip re-scans.

- **H3 — Timeline index:** Per-track **interval tree** (or augmented sorted-by-start array) answering "active clips at P" / "clips overlapping [A,B]" in O(log n + k), shared by the audio read path and the renderer; RCU it alongside the graph. Without it, both paths are O(clips) per block/frame — the single most common DAW scaling bug, currently absent from the plan.

- **H6 — Multicore is load-bearing for the H6 exit criterion, not optional.** A serial walk cannot meet "99.9th-pct Block time < Block period" on a genuinely heavy session (100 tracks × 3–5 plugins). Design the work-stealing scheduler now (cheap to write down): atomic per-node dependency counter; finished node decrements successors and pushes newly-ready nodes; fixed pool of workers pinned to the platform RT class (macOS Audio Workgroup `os_workgroup_join`; Windows MMCSS "Pro Audio" `AvSetMmThreadCharacteristics`; Linux `SCHED_FIFO`), spin-then-park with bounded backoff (never a mutex); pad dependency counters and meters to cache lines (false sharing); the compiler emits per-level node lists to expose parallelism width; the buffer-pool allocator must be parallel-aware (concurrent liveness, non-aliasing buffers per worker).

- **GPU renderer choice is an H0 decision (per-surface, architectural).** Windows JUCE 8 Direct2D is already GPU/VRAM-backed — you do **not** need OpenGL just to use the GPU. **Attaching an `OpenGLContext` makes that component render via GL instead of Direct2D — they cannot mix on one component** (the GL context wins). So either ride Direct2D (Win)/Metal-backed CoreGraphics (mac) as default, **or** commit the timeline canvas *fully* to one `OpenGLContext`. Configure the renderer in `parentHierarchyChanged()` (`getPeer()` is null until `addToDesktop()`); avoid `setBufferedToImage(true)` on the animated canvas (forces per-frame GPU→CPU readback). **Virtualize the viewport first, accelerate second** — an un-virtualized timeline dies on long projects no matter how fast the GPU is (the Meadowlark failure).

---

### → `## Testing & reliability strategy`

**Research Insights to ADD:**

- **Golden-file management is manual-bless-only.** One reviewed `.wav` golden per platform via Git LFS / hash-pinned; regenerated only behind an explicit `--bless` flag with human diff review — **never auto-updated by CI** (a regression that updates its own golden is invisible forever). Tolerance, not bit-exactness, cross-platform (peak-err ≤ −96 dB cross-platform / −120 same-platform, RMS-err ≤ −100 dB) — FMA/vector-width/transcendental differences are not bugs; a *new* same-platform divergence always is.

- **Order-shuffle test makes the parallel-safety invariant testable from H1** (the plan's biggest unverified bet). A debug mode randomizes the order of *independent* nodes within each topo-level of the serial walk, wired into the golden-file test. If output ever changes, "multicore is an addition, not a rewrite" is *already* broken — caught in CI years before the pool exists. Pair with the written buffer-pool aliasing contract.

- **Cross-buffer-size invariance test:** render the same project at 64/128/512 and a deliberately *prime* block size (113) and pairwise-diff — output must be block-size-independent (catches event-offset and smoother bugs).

- **Chaos-recovery suite (build in H1, not H6):** kill the process at randomized points during import / edit / autosave / **migration**; deterministically corrupt `project.db`, the WAL, an `audio/` file, and a plugin BLOB; assert detect-and-quarantine or graceful degradation, never a crash. Structure-aware fuzz (libFuzzer/AFL++ with ASan+UBSan) the bundle parser, the SQLite open path (hardened: `SQLITE_DBCONFIG_DEFENSIVE`, `trusted_schema=OFF`, `cell_size_check=ON`, authorizer forbidding `ATTACH`/extension-load), the audio decoders, the peak parser, and the plugin-state header. The plugin chunk's own parser is third-party and unreachable by your fuzzer — contain it (see new risks).

- **Define "no corruption" as a four-part assertion** for the H6 exit criterion: structural (`integrity_check` clean) AND referential (zero orphans/dangling refs) AND DB↔FS consistent (every referenced file present + hash-verified) AND semantic (clip/tempo invariants hold).

- **Large-session benchmark fixture wired into CI from H1** (200–500 tracks, 10k clips, dense MPE) asserting compile-cost bounds (< 5 ms @ 200 tracks, < 20 ms @ 500, curve-fit O(V+E)), query complexity (O(log n) active-clip/viewport lookups), and frame budget (60 fps scroll). **Every scaling hotspot is on the control thread or renderer, not the audio thread** — the RCU design moved the residual risk there, and the existing small-fixture gates pass while these hide.

- **Latency-change-storm soak case:** a stub Node spamming `latency-changed` during playback; assert recompile+swap keeps up without snapshot backpressure or Underruns, and cached delay-line state prevents clicks.

- **CI:** clone **pamplejuce** (CMake ≥3.25, CPM, Catch2 v3, macOS arm64+x64 → universal, Windows MSVC, Linux **Clang leg for RTSan**, ccache/sccache, notarization + Azure Trusted Signing). pluginval at **L10** (param fuzzing + repeated state save/restore), not the L5 smoke bar; `auval` mandatory on the macOS leg.

---

## Proposed new section: "Long-horizon execution via agentic loops"

> Insert after `## Build order / milestones`. Community-sourced (Ralph/RPI/Codex-milestone/verification-wrapped loops); no Anthropic official docs. This formalizes the plan's existing instinct ("`/loop` runs against the current horizon's exit criterion").

### Model

Day-to-day build proceeds as a **bounded agentic loop**, not bare `while-true` Ralph. YES DAW is brownfield-leaning and architecture-critical — the opposite of the greenfield throwaway-compiler case Ralph shines at — so we adopt the **RPI (Research → Plan → Implement) + milestone-checkpoint** register. The loop never decides architecture: the ADRs and this plan are **frozen spec**; the loop only implements toward the current horizon's exit criterion, with the CI gates as its green condition.

The fit is unusually good for one structural reason: **YES DAW already has the deterministic, hard-to-fake green gate the loop needs** (RTSan, golden-file render-diff, PDC impulse, property-based undo, save/load round-trip, migration fixtures). Most loops get metric-gamed because their gate is weak; these are behavioral oracles and property tests, not narrow metrics. And H0–H6, each with exactly one testable exit criterion, is already the milestone-checkpoint shape the long-horizon recipes converge on.

### Loop files (durable memory, re-read every tick, version-controlled, LF endings)

| File | Content |
|---|---|
| `loop/PROMPT.md` | Short (< 150 words): "Implement the highest-priority item in `loop/fix_plan.md` toward the H{N} exit criterion. One item only. Run the gate. Commit only if green. Update plan + progress. Never edit `docs/adr/**`, this plan, a golden file, or a `[[clang::nonblocking]]` annotation." |
| `loop/horizon.md` | The current horizon's exit criterion verbatim, its "done when," and the exact CI commands that constitute green. The frozen target. |
| `loop/fix_plan.md` | Prioritized, one-context-sized tasks decomposing the current horizon; each with an acceptance check, a validation command, and a `passes:` flag. |
| `AGENT.md` | Per-OS build/test/run commands (CMake Debug, ninja, ctest, the RTSan Clang leg, golden-render, PDC-impulse, RapidCheck undo, save/load, migration fixtures); accumulated "signs." |
| `loop/progress.txt` | Append-only learnings across context resets. |

### Per-tick sequence

Fresh context → read `horizon.md` + `fix_plan.md` → pick **one** item → research-then-implement (scoped diff, touch no ADRs/goldens) → run the item's gate subset locally → **on green** commit + tick the item; **on red** repair *before advancing* (stop-and-fix) → update `fix_plan.md` + `progress.txt` → session ends → repeat. One discrete unit per tick (fresh ~170k-token context; quality degrades approaching ~150k); short prompts beat long ones.

### Green condition by horizon (escalating exactly as the plan sequences gates)

- **H0:** spike exit (10-min zero-Underrun sine on real hardware both OSes; 60 fps timeline) — **human-confirmed** (hardware/perf can't be CI-faked).
- **H1+:** RTSan-clean (Clang leg) + golden-render-diff within tolerance + PDC-impulse + save/load round-trip + property-based undo + migration fixtures; ASan/UBSan clean on every change.
- **H3+:** add pluginval L10 + `auval` + crash-isolation/blacklist.
- **H4+:** add the MIDI-offset-across-blocks-and-tempo-change timing test.
- **H5+:** add the ±1-frame recorded-take alignment test.
- **H6:** add the 60-min heavy-session soak + kill-mid-edit recovery test.

### Writer/critic split (the anti-metric-gaming layer)

The implementing loop is the **writer**. A separate **critic pass** (different prompt — ideally the planned custom DAW review agents: real-time-safety reviewer, render-correctness reviewer) runs on the diff before merge to catch what gates can't: a `[[clang::nonblocking]]` annotation removed to "fix" RTSan, a golden regenerated to mask a regression, a property test's generators weakened. "A dumb model writes code, a smart model validates it."

### Stop conditions (all mandatory)

- Horizon exit criterion met + gates green → stop; hand to human for the horizon-boundary review (**only humans advance H{N}→H{N+1}** — never the loop's call).
- Max iterations per run (e.g. 20) reached → stop, report.
- Circuit-breaker: same gate fails 3× consecutively, or two consecutive empty diffs → open, file a GitHub issue + notify, halt.
- Token/dollar budget ceiling per overnight run.
- **Hard stop + escalate** on any attempted edit to `docs/adr/**`, this plan, a golden file, a `[[clang::nonblocking]]` annotation, or a `git reset --hard`.

### Operational guardrails

Commit-on-green only; tag on a clean gate run (start 0.0.0, bump patch — a crash never loses work); single-writer lock on shared headers / `CompiledGraph` core during a run; validation/build subagents capped at 1 concurrent (don't flood backpressure); loop runs on the Clang leg so RTSan is live; `progress.txt`/`AGENT.md` self-updated each tick; nightly run reports iteration count, tokens, exit reason, and gate deltas.

### Risks specific to the loop

- **Metric gaming of the gates** → goldens/ADRs/nonblocking-annotations are loop-immutable (hard-stop on edit) + critic pass.
- **Drift with no auto-detector** (the community's named blind spot) → `horizon.md` re-read every tick; human-gated horizon boundaries; RPI design-doc step before any non-trivial subsystem.
- **C++ is agent-hard-mode** (UB, lifetime, long header context, slow compiles) → ASan/UBSan baseline; supervised (critic + human) for engine-core/RCU/janitor/buffer-pool; never fire-and-forget.
- **The timeline GUI is the worst loop fit** (perceptual 60 fps isn't a deterministic oracle) → keep GUI work human-supervised; the loop is for the engine/data-model/test-gated layers where oracles are strong.
- **Cost runaway / brownfield mismatch / context rot** → iteration cap + circuit breaker + budget ceiling; the RPI/milestone register (not bare Ralph); one-thing-per-tick + fresh context + short prompt + externalized state.

### Sources (community)

Geoffrey Huntley — *Ralph Wiggum as a software engineer* / *everything is a ralph loop*; Dev Interrupted interview; ZeroSync — *Ralph Loop Technical Deep Dive*; `snarktank/ralph`; `frankbria/ralph-claude-code`; `vercel-labs/ralph-loop-agent`; `agairola/securing-ralph-loop`; LinearB — Dex Horthy/HumanLayer RPI; explainx.ai — *Loop Engineering* (2026); OpenAI Developers — *Run long-horizon tasks with Codex*; wrocpp — *AI coding agents for C++*; HN threads ("Ralph Wiggum Doesn't Work" / "What Ralph loops are missing"); AddyOsmani — *Self-Improving Coding Agents*.

---

## Simplicity — ADOPT (genuine simplifications, same scope — safe to apply)

These reduce *distinct mechanisms you must build and prove RT-safe*, not features.

1. **Name the lock-free primitives in ADR-0002 #5 instead of describing categories.** Control→audio = `choc::SingleReaderSingleWriterFIFO`; audio→control latest-value = triple-buffer/SeqLock. Borrow battle-tested wait-free correctness instead of re-deriving it under RTSan. The "sanctioned mechanism" becomes a named type.

2. **One ordered SPSC queue carries scalars *and* topology (`SwapGraph` as a variant), not two channels.** Makes ordering correct for free and eliminates the post-swap-target reordering bug class.

3. **Unify PDC delay node + feedback delay node into one `DelayNode`.** One RT-critical primitive, one test.

4. **Unify fades + crossfades + clip-gain into one per-clip gain-envelope** evaluated at frame position; crossfade = two adjacent envelopes from one shared ramp. Shrinks the H2 surface to one well-tested function.

5. **One driver + one `Clock` interface (two implementations) + capture-FIFO flag**, not four parallel driver paths. The offline path *is* both the Export feature and the golden-test path — nothing to keep in sync.

6. **One monotonic `EntityId` allocator (persisted `next_id`), type carried by the table** — not five per-entity ID conventions. Kills cross-table collision classes; trivial with SQLite `INTEGER PRIMARY KEY`. (Pin "never-reused monotonic" vs ULID before H1 if cross-project identity is in scope — see Conflicts.)

7. **One live in-memory document; SQLite is its serialization, not a second live model.** Commands mutate the document and emit diffs; Save/autosave writes to SQLite; load reads back. Removes "is the DB or the document authoritative right now" ambiguity. SQLite rows are the on-disk form, queried only at load/save and crash recovery — never on the edit hot path.

8. **Make the graph the single source of routing truth — Track/Bus/Master are arrangement *rows* that compile to nodes, with no fat channel-strip object.** (Requires the CONTEXT.md fix in Conflicts.) Removes the temptation to grow a parallel channel-strip model alongside the graph.

9. **Name the narrow blueprint in the H1 ADR: Rowland's ADC20 graph + Doumler's two RT-comms patterns.** Shrinks "research to internalize before H1" from three reports to two talks; everything else is explicitly out-of-reference.

10. **State the snapshot publication once as a single named primitive** (`farbot::RealtimeObject` mode / generation-counter janitor), not "RCU + janitor" in the graph section and "double/triple-buffer" in research — the same mechanism at two altitudes invites two implementations.

---

## Simplicity — SCOPE-CUTS (DO NOT APPLY — owner wants the full thing)

Listed for visibility only. **None of these are to be acted on.** The owner has explicitly rejected scope reduction; every item below stays in.

- **CLAP hosting (H3 tail).** VST3+AU cover the commercial market today; CLAP hosting is community-alpha. **Keep** (the CLAP-shaped internal contract is free and load-bearing).
- **MIDI-2.0/UMP-class wide fields + per-note IDs from H1.** A YAGNI reviewer would model MIDI-1 and widen later. The plan's counter (widening `uint8` is a rewrite) is exactly why it's kept now. **Keep.**
- **Multicore work-stealing scheduler (H6).** Already maximally deferred behind "once a real session stresses one core." **Keep** (and note: it's actually *required* for the H6 exit criterion — see enhancements).
- **DAWproject export (H6).** Pure interchange insurance, no user needs it to make music. **Keep** (longevity/insurance).
- **Time-stretch (Signalsmith) + loudness metering (libebur128) in H6.** Table-stakes but off the editing-first critical path. **Keep.**
- **Take-lanes/comping schema stubbed in H2 (data-only).** A minimalist wouldn't stub until H5. **Keep** (right ambitious call).
- **Linux/LV2 hosting (open question #3).** Adds a third plugin format + fourth OS leg. **Flagged for owner only — not cut.**
- **WebView UI escape hatch (open question #1).** Native recommended; carrying a WebView option is optional surface. **Owner's call — not forced to native-only here.**

---

## New considerations / risks discovered (cross-cutting)

1. **Sample-rate policy is nearly absent and is an irreversible-class decision.** No project-SR policy, asset-SR-mismatch behavior, resampler quality tiers, or mid-project SR-change handling. The research flags "internal SR policy + resampler tiers" as expensive-to-reverse. **Recommend a 14th irreversible decision**, resolved at H1/H2, with an exit criterion (a 44.1k asset in a 48k project renders bit-tolerant against an offline reference). Asset content-hashing and buffer-pool sizing both implicitly assume an SR answer.

2. **Automation curve representation is underspecified relative to its irreversibility.** Pin point storage (`{t_ticks, value, curve_type}`), the interpolation/segment enum (linear/bezier/hold/log), and sample-accurate-vs-block-offset evaluation in the **H1 freeze** — it rides the locked event stream, so its representation is as irreversible as the event model.

3. **Cross-file bundle atomicity (DB ↔ filesystem) is the biggest latent data-integrity gap.** SQLite ACID covers only `project.db`; import/delete/migration all cross the DB/FS seam. Mitigated by the intent-log + reconcile-on-open + `.trash/` soft-delete detailed in the data-model enhancement. **Write a bundle-consistency ADR alongside the schema-v1 ADR** — near-impossible to retrofit after users have bundles.

4. **Plugin hosting is a zero-trust boundary that ships at full trust through H3.** In-process hosting makes every VST3/AU/CLAP binary full-trust native code on your audio thread; a hostile `.yesdaw` plugin-state chunk is RCE via the plugin's `setState`. **Highest-leverage security move: commit to out-of-process + sandboxed plugin hosting as the shipped default, architected for in H1** (decision #12 already mandates serializable buffers+events — honor it by making `PluginNode` an IPC proxy over shared-memory ring buffers). Add a per-`process()` watchdog with a kill→bypass-node→recompile path, runtime crash attribution + escalating blacklist, plugin provenance/signature checks, and validate plugin-reported `NodeProperties` before they reach PDC.

5. **Untrusted-input parse surface is wide and shareable.** The bundle (SQLite, audio decoders, peak cache, plugin BLOBs, autosave/WAL) is attacker-shapeable once projects are shared. Path-traversal/symlink confinement, hardened SQLite open, resource ceilings (tracks/clips/segments/declared-frames/pool-size with checked arithmetic), and structure-aware fuzzing **pulled forward to H1** are required. Treat the peak cache as untrusted-and-regenerable (header-magic + length-validate, else discard+regenerate).

6. **Supply chain has no pinning/SBOM/CVE gate stated.** Pin every native dep (JUCE, SQLite, VST3 SDK, CLAP/`clap-wrapper`, choc, farbot, libebur128, Signalsmith, RapidCheck — and ONNX/FAISS/model-weights if AI verbs land) by commit-hash/checksum; generate an SBOM; run OSV-Scanner/Trivy in CI; isolate the alpha CLAP-loading code inside the sandboxed host child; load ML weights from a verified, pickle-free, sandboxed path; add a license gate failing the build on GPL/AGPL entering the linked binary (enforces "study `tracktion_graph`, never link" automatically).

7. **Recompile throughput / janitor backpressure is unbudgeted.** Plugins firing `latency-changed` storms, or drag-driven re-routes, can publish snapshots faster than the janitor retires them (memory spike) or lag the UI. Add: **coalesce** pending recompiles (only the latest desired topology), **debounce** drag gestures (recompile on gesture-end / ≤30 Hz), **bound** outstanding retired graphs (throttle the *control* thread, never the audio thread), and rate-limit latency-driven recompiles. A growing retired list is a bug signal to surface.

8. **Undo durability ↔ autosave coherence is an unresolved interaction, not just an open question.** In-memory undo + SQLite autosave at `NORMAL` can desync on crash recovery (recover a document state the undo stack doesn't describe; or a partial multi-row edit no single undo entry describes). **Record the invariant at H2** ("crash-recovery restore must produce a state the recovered undo stack can describe, or explicitly truncate the stack to the recovered point") even if journaling undo to SQLite is deferred — and prefer resolving open-question #4 toward **journal-from-H2** with autosave firing only on undo-group boundaries.

9. **Parallel-safety is the plan's biggest unverified bet and currently untested.** The order-shuffle debug mode (in the testing enhancement) converts it into an H1 CI gate. Pair it with a *written* buffer-pool aliasing/lifetime contract — the two together catch the races that "multicore is an addition" silently assumes away.

10. **`time_base` + tempo-map edits can silently desync any persisted derived sample positions.** Make it an explicit invariant: **samples are never persisted as authoritative.** If a derived-sample cache is stored for performance, store the tempo-map version/hash it was computed against and recompute on mismatch. Property test: edit tempo, assert all `SampleLocked` clips' sample windows are byte-identical.

11. **A logically-stale snapshot can be laundered into the persistent document via autosave.** RTSan catches alloc/lock on the audio thread but not a logically-stale snapshot being treated as current. Add a stress test hammering compile-and-publish against control-thread mutation, asserting the published snapshot is never logically stale and that autosave only ever serializes control-thread truth (never a value read back from a snapshot).

12. **`integrity_check` is O(db-size) on the open hot path** — run `quick_check` on open + background full check; otherwise a future dev skips it on large projects and loses the corruption gate.

---

## Conflicts flagged for human review

1. **PPQ freeze framing (irreversible decision #5 / open question #5) vs the rational/large-fixed-tick option.** A reviewer argues the "frozen PPQ, baked irreversibly into the format" is *self-imposed* — storing position as rational beats (`int64 num/den`) or a deliberately huge fixed tick grid would delete the irreversibility and open-question #5 at zero scope cost, with `frac`/`double` math at the render boundary recovering groove/MPE precision regardless. The enhancements above currently follow the plan (960 PPQ + render-time `frac`). **Decision needed:** keep the frozen-PPQ ADR as written, or adopt a representation that doesn't force the bet. This is the one place a reviewer pushed back on the plan's own framing.

2. **CONTEXT.md "Track … Carries its own mixer controls" vs "the mixer is the graph."** The glossary line pulls toward a fat Track object; the plan and ADOPT #8 say Track/Bus/Master are arrangement rows that *compile to* nodes with no parallel channel-strip model. **Resolve toward the graph** and update the CONTEXT.md Track entry (already adjacent to open question #8's "doc housekeeping").

3. **Plan header "supersedes … CONTEXT.md 'wedge'" vs ADRs-are-the-supersession-mechanism.** A *plan* claiming supersession over a *glossary* muddies the append-only-ADR rule (CLAUDE.md: "no code lands before the decisions it depends on are written as ADRs"). **Resolve:** have ADR-0003 (not the plan) be the thing that supersedes the wedge framing; update CONTEXT.md's stale "wedge" language (open question #8).

4. **13 (→14 with sample-rate) irreversible decisions → an unknown number of future ADRs, mapped informally.** Real risk a decision silently never gets its ADR and the plan becomes the de-facto ADR. **Resolve:** add a tracking table in `docs/adr/README.md` mapping each decision → target ADR number + milestone.

5. **Stable-ID mechanism (decision #8) is asserted but the mechanism isn't pinned.** `INTEGER PRIMARY KEY` reuses rowid space after deletes (alias/collision risk under undo/redo re-insert) and is unique only within one DB (no cross-project identity for templates/paste-between-projects/DAWproject). **Decision needed before H1:** monotonic-never-reused `INTEGER` vs 128-bit ULID — driven by whether cross-project identity is in long-horizon scope (the plan implies it is via DAWproject).

6. **In-process hosting (decision #12) vs the security recommendation to ship out-of-process by default.** Decision #12 says "in-process hosting, but Nodes communicate via serializable buffers + events … keeps out-of-process possible later." The security review argues "later" leaves a zero-trust boundary shipping at full trust through H3, and that decision #12's own serializable-seam mandate should be *honored from H1* by making `PluginNode` an IPC proxy. **Decision needed:** does out-of-process become the shipped default (and when), or stay a future option? This changes H3's shape and is the single highest-consequence open security question.