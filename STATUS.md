# YES DAW — STATUS (live handoff)

**Read this first on any machine.** This is the single source of truth for *where we are right now*.
The [plan](docs/plans/2026-06-23-feat-yes-daw-architecture-roadmap-plan.md) and
[roadmap](docs/goals/roadmap.md) are the stable reference; **this** file is the live, constantly-updated
worklog.

> **Cross-machine rule:** `git pull` at the start of a session. At the end, update this file, commit in
> small chunks, and `git push`. Then the next machine — or the next session — is never lost.

**Last updated:** 2026-06-25
**Current horizon:** **H3 (mixer + plugin hosting)** — plugin-state storage/header review/fix green locally; mixer projection next

> **Verification = CI.** A change is done when CI is green, not when Dan listens or watches. The only
> human step is blessing a golden on an intended audio change (`cmake --build --preset ci --target bless-goldens`).

---

## Now — between chunks (every engine commit to date is CI-green)
- **Latest: REVIEW/FIX H3 plugin state chunk storage/header gate is green locally.**
  Reviewed the current `main` implementation (worker commit `85a29a7` plus hardening commit `9d26b7b`)
  against `STATUS.md`, ADR-0013, ADR-0012, ADR-0011, the H3 plan/roadmap/deepening notes, and current
  persistence contracts. Found no further defects: the surface stays storage/header-only, uses the
  persistent 16-byte node Entity ID as the key, stores opaque plugin bytes with host-owned metadata,
  computes CRC32 at the bundle boundary, validates SQLite storage classes plus `chunk_len` and `crc32`
  before restore handoff, preserves unreadable bytes in place, reports missing/corrupt chunks as
  default-state restore outcomes, and returns VST3 component state before controller state. The current
  hardening commit `9d26b7b` is green in remote CI run `28181189197` across Windows, Linux, macOS,
  RTSan, and TSan. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (145/145). No plugin-host code, scanner code,
  plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4 work, golden edits, broad graph
  rewiring, sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]` edits were made.
  Remote CI is pending until this status-only review/fix closeout commit is pushed.
  **Next:** WORKER H3 mixer graph projection foundation: add the smallest headless mixer projection over
  the frozen graph/Node contracts, using the existing Fader/Pan/Sum/Send/Return/Meter building blocks
  where they already exist and stopping if a new ADR-level mixer decision appears. Prove it with
  self-asserting tests only. Keep it headless and out of plugin-host code, scanner code, plugin UI,
  CLAP loading, out-of-process runtime IPC, export UX, H4 work, golden edits, broad graph rewiring,
  sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]` edits. Run the documented
  gate, update `STATUS.md`, commit/push, check CI, then create the follow-up REVIEW/FIX thread if green.
- **Latest: WORKER H3 plugin state chunk storage/header gate is green locally.**
  Added the smallest headless persistence surface for ADR-0013 plugin-state chunks on top of the
  existing `plugin_state_chunks` table reservation. `ProjectBundleDb` now writes opaque plugin bytes
  with host-owned metadata (`format`, `plugin_uid`, `plugin_version`, `chunk_kind`, `chunk_len`,
  `crc32`), computes and stores CRC32 at the bundle boundary, reads chunks only after validating
  `chunk_len` and `crc32`, preserves corrupt bytes in place, and reports missing/corrupt chunks as
  default-state restore outcomes instead of handing unreadable bytes to a plugin. The storage/API
  boundary uses the persistent 16-byte node Entity ID as the key and returns VST3 component state before
  VST3 controller state. New `YesDawPersistenceCheck` coverage proves opaque-byte/metadata storage,
  persistent Entity ID keying even when two nodes share the same low runtime-ID-shaped bits, header
  corruption fallback without byte mutation, missing-chunk fallback, and VST3 restore ordering. No
  plugin-host code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4
  work, golden edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth, or
  `[[clang::nonblocking]]` edits were made. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (145/145). Remote CI is
  pending until this worker commit is pushed.
  **Next:** REVIEW/FIX H3 plugin state chunk storage/header gate: verify the worker implementation
  against `STATUS.md`, ADR-0013, ADR-0012, ADR-0011, the H3 plan/roadmap/deepening notes, and current
  persistence contracts. Fix only proven defects; keep it storage/header-only and do not start
  plugin-host code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC, export UX, H4
  work, golden edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth, or
  `[[clang::nonblocking]]` edits. Run the documented gate, update `STATUS.md`, commit/push, check CI,
  then create the next WORKER thread from `STATUS.md` if green.
- **Latest: REVIEW/FIX H3 ADR-0013 plugin state + hosting isolation is green locally.**
  Reviewed ADR-0013 against `STATUS.md`, the H3 plan/roadmap/deepening notes, ADR index/template,
  ADR-0002/0006/0007/0008/0009/0012, `CONTEXT.md`, and the current Node / EventStream / Runtime /
  CompiledGraph / GraphBuilder / ProjectBundle / CMake/test contracts. Found one narrow documentation
  defect and fixed it: ADR-0013 now explicitly says `plugin_state_chunks.node_id` is the persistent
  16-byte node Entity ID stored in the bundle, not the runtime 32-bit `NodeId` used inside
  `CompiledGraph`; `CONTEXT.md` mirrors that glossary-level wording for Plugin state chunk. No
  plugin-host code, scanner code, plugin UI, CLAP loading, export UX, H4 work, golden edits, broad
  graph rewiring, schema implementation changes, sampled/pixel/snapped/derived Project truth, or
  `[[clang::nonblocking]]` edits were made. The ADR worker commit `3b00db8` is green in remote CI run
  `28151834609` across Windows, Linux, macOS, RTSan, and TSan. Local gate via documented Windows
  DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (142/142).
  Remote CI is pending until this review/fix commit is pushed.
  **Next:** WORKER H3 plugin state chunk storage/header gate: add the smallest headless persistence
  surface and self-asserting tests for ADR-0013 plugin-state chunks on top of the existing
  `plugin_state_chunks` reservation. Prove the bundle stores opaque bytes with host-owned metadata,
  uses the persistent 16-byte node Entity ID as the storage key, validates `chunk_len` + `crc32` before
  restore handoff, preserves original bytes, restores VST3 component before controller ordering at the
  storage/API boundary, and degrades corrupt/missing chunks to an unreadable/default-state result
  without crashing. Keep it storage/header-only: do not start plugin-host code, scanner code, plugin
  UI, CLAP loading, out-of-process runtime IPC, export UX, H4 work, golden edits, broad graph rewiring,
  sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]` edits. Run the documented
  gate, update `STATUS.md`, commit/push, check CI, then create the follow-up REVIEW/FIX thread if green.
- **Latest: WORKER H3 ADR-0013 plugin state + hosting isolation is green locally.**
  Added `docs/adr/0013-plugin-state-and-hosting-isolation.md` to lock plugin state as opaque
  host-wrapped chunks, VST3 + AU first then CLAP, out-of-process/sandboxed hosting from the start,
  `PluginNode` as the IPC proxy over serializable audio/Event buffers, one-Block nonblocking
  fail-open behavior, scanner watchdog/blacklist/cache behavior, and pluginval / `auval` /
  host-isolation gates. Updated `docs/adr/README.md` so engine decisions #11 and #12 are recorded by
  ADR-0013, and updated `CONTEXT.md` for the new shared plugin-hosting vocabulary. No plugin-host code,
  scanner code, plugin UI, CLAP loading, export UX, H4 work, golden edits, broad graph rewiring, schema
  implementation changes, sampled/pixel/snapped/derived Project truth, or `[[clang::nonblocking]]`
  edits were made. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (142/142). Remote CI is pending until this ADR
  worker commit is pushed.
  **Next:** REVIEW/FIX H3 ADR-0013 plugin state + hosting isolation: verify ADR-0013 against
  `STATUS.md`, the H3 plan/roadmap/deepening notes, ADR index, and current contracts. Fix only proven
  doc defects; do not start plugin-host code, scanner code, plugin UI, CLAP loading, export UX, H4
  work, golden edits, broad graph rewiring, schema implementation changes, sampled/pixel/snapped/derived
  Project truth, or `[[clang::nonblocking]]` edits. Run the documented gate, update `STATUS.md`,
  commit/push, check CI, then create the next WORKER thread from `STATUS.md` if green.
- **Latest: Dan approved the H2->H3 horizon boundary; H3 loop handoff is being opened.**
  H2's mechanical exit gates are green locally and in remote CI: command/diff edit-sequence undo/redo
  returns the live `Project` to bit-identical states, split-with-crossfade Project render matches
  Runtime/offline graph paths, and kill-mid-import bundle recovery is DB/filesystem consistent with
  committed Asset hash verification and no orphan audio files. Remote CI run `28146655906` for H2
  closeout commit `435d320` is green across Windows, Linux, macOS, RTSan, and TSan. Dan explicitly
  approved advancing to H3. H3 code must not start before its pending ADR is written: ADR index decision
  #11 plugin state as opaque chunks and #12 out-of-process/sandboxed hosting both point to ADR-0013.
  This status-only parent handoff passed the documented local Windows DevShell gate:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` (142/142). Remote CI is
  pending until this handoff commit is pushed.
  **Next:** WORKER H3 ADR-0013 plugin state + hosting isolation: write the narrow ADR only, covering
  opaque plugin-state chunks, VST3+AU first then CLAP, `PluginNode` as an out-of-process IPC proxy,
  one-block nonblocking fail-open audio behavior, scanner crash/hang blacklist behavior, pluginval /
  `auval` gates, and the host-isolation test implied by the H3 exit criterion. Update the ADR index and
  `CONTEXT.md` only if the ADR changes shared terms. Do not write plugin-host code, scanner code,
  plugin UI, CLAP loading, export UX, H4 work, golden edits, broad graph rewiring, schema semantics
  beyond ADR wording, or `[[clang::nonblocking]]` edits. After a green ADR worker commit, that worker
  must create the follow-up REVIEW/FIX H3 ADR-0013 thread. The review/fix thread must verify the ADR
  against the plan, deepening notes, ADR index, and existing code contracts, then create the next worker
  only if the review is green. Continue worker -> review/fix -> worker until H3 exit gates are green,
  then hard-stop for Dan's next horizon-boundary review.
- **Latest: WORKER H2 exit-gate closeout / CI-truth pass is green locally.**
  Verified from current repo truth that the H2 exit gates are represented by self-asserting tests:
  command/diff edit-sequence undo/redo returns the live `Project` to the bit-identical original value
  and redoes to the bit-identical edited value (`YesDawProjectCheck`); split-with-crossfade Project
  rendering is green through both Runtime and offline graph paths with exact adjacent Tick/source-frame
  windows, `evaluateClipGainEnvelope`-derived expected samples, and unchanged Asset/Project truth
  (`YesDawBundleRenderCheck`); and kill-mid-import bundle recovery is green via open-time
  DB/filesystem reconciliation, committed Asset hash verification, stale intent cleanup, and no orphan
  audio files (`YesDawPersistenceCheck`). Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (142/142). Latest pushed
  remote CI before this closeout, run `28146299670` for `9fe162f`, is green across Windows, Linux,
  macOS, RTSan, and TSan; this status-only closeout commit will be pushed and checked before handoff.
  No H3, UI shell, export UX, plugin hosting, ADR edits, roadmap edits, golden edits, broad render
  rewiring, schema semantics, sampled/pixel/snapped/derived values as Project truth, or
  `[[clang::nonblocking]]` edits were made.
  **Next:** Dan's H2 horizon-boundary review. Only Dan advances H2->H3; do not create an H3 worker
  unless `STATUS.md` is explicitly changed to say so.
- **Latest: REVIEW/FIX H2 split-with-crossfade RT/offline render gate found no defects.**
  Reviewed worker commit `63c855a` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  The gate stays headless and narrow: it builds a Project through the current Clip edit helpers
  (`setClipGain`, `splitClip`, `setClipFades`), asserts exact adjacent Tick and source-frame windows
  before and after bundle reopen, uses `evaluateClipGainEnvelope` for expected decoded Clip samples and
  crossfade-compatible midpoint gains, and compares the same valid Project through Runtime and offline
  graph paths. Assets and Project truth remain metadata-only: unchanged Asset rows, unchanged Clip /
  Project values after write/render, and unchanged bundled Asset bytes. No SQLite undo journaling,
  autosave durability semantics, UI gesture timing, export UX, plugin hosting, H3 work, ADR edits,
  roadmap edits, golden edits, waveform cache changes, broad render rewiring, schema semantics,
  sampled/pixel/snapped/derived values as Project truth, or `[[clang::nonblocking]]` edits. Local gate
  via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (142/142). Remote CI run `28145624290` for worker commit `63c855a` and run
  `28145828642` for pre-review status tip `c194ff4` are green across Windows, Linux, macOS, RTSan, and
  TSan. Remote CI is pending until this status-only review/fix commit is pushed.
  **Next:** WORKER H2 exit-gate closeout / CI-truth pass: verify from repo truth that the H2 exit gates
  are represented by self-asserting tests and latest pushed CI: command/diff edit-sequence undo/redo
  returns the Project bit-identical, split-with-crossfade Project RT/offline render is green, and
  kill-mid-import bundle consistency is green with assets hash-verified/no orphans. Do not start H3, UI
  shell, export UX, plugin hosting, ADR edits, roadmap edits, golden edits, broad render rewiring,
  schema semantics, sampled/pixel/snapped/derived values as Project truth, or `[[clang::nonblocking]]`
  edits. If the H2 exit gates are green, update `STATUS.md` for Dan's horizon-boundary review and stop;
  only Dan advances H2->H3.
- **Latest: REVIEW/FIX H2 edit-sequence undo/redo property gate found no defects.**
  Reviewed worker commit `af31e8e` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  The deterministic headless sequence generator stays Project-local and command+diff only: it drives
  `moveClip`, `trimClip`, `splitClip`, `setClipGain`, and `setClipFades` through explicit
  `ProjectUndoStack` transaction-group boundaries, accepted/rejected group boundaries, grouped
  compatible coalescing, ungrouped same-verb separation, split-plus-right-Clip follow-up edits, and
  invalid gain/source-window commands. The gate proves apply-all / undo-all returns the live in-memory
  `Project` to the bit-identical original value and redo-all returns it to the bit-identical edited
  value. The slice remains command+diff and Project-local only: no SQLite undo journaling, autosave
  durability semantics, UI gesture timing, export, plugin hosting, H3 work, ADR edits, roadmap edits,
  golden edits, waveform cache changes, broad render rewiring, schema semantics,
  sampled/pixel/snapped/derived values as Project truth, or `[[clang::nonblocking]]` edits. Local gate
  via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (141/141). Remote CI is pending until this status-only review/fix commit is
  pushed.
  **Next:** WORKER H2 split-with-crossfade RT/offline render gate: add the smallest self-asserting
  headless Project render gate for a split Clip with crossfade-compatible existing gain/fade metadata,
  proving the same valid Project renders identically through RT playback and offline Render while
  Assets remain immutable and Project truth stays metadata-only. Use current Clip edit helpers and
  existing envelope evaluation where possible. Keep sampled/pixel/snapped/derived values out of Project
  truth. Do not expand into SQLite undo journaling, autosave durability semantics, UI gesture timing,
  export UX, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes,
  broad render rewiring, schema semantics, or `[[clang::nonblocking]]` edits. If crossfade
  curve/shared-ramp representation, timeline projection semantics, export scope, undo persistence, or
  any ADR-level decision rises, stop and report.
- **Latest: WORKER H2 edit-sequence undo/redo property gate is green locally.**
  Added the smallest deterministic headless sequence generator over the current Project-local Clip edit
  command surface and explicit `ProjectUndoStack` transaction-group boundaries. The new
  `YesDawProjectCheck` gate drives `moveClip`, `trimClip`, `splitClip`, `setClipGain`, and
  `setClipFades` through accepted and rejected group boundaries, grouped compatible coalescing,
  ungrouped same-verb separation, split-plus-right-Clip follow-up edits, and invalid gain/source-window
  commands. It proves apply-all / undo-all returns the live in-memory `Project` to the bit-identical
  original value and redo-all returns it to the bit-identical edited value. The slice stays command+diff
  and Project-local only: no SQLite undo journaling, autosave durability semantics, UI gesture timing,
  export, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, broad
  render rewiring, schema semantics, sampled/pixel/snapped/derived values as Project truth, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (141/141). Remote CI run `28144622776` for
  worker commit `af31e8e` is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 edit-sequence undo/redo property gate: review the worker gate against
  `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2 plan/deepening notes, and the current Time /
  Project / ProjectBundle / render and persistence tests. Verify the sequence generator is only a
  Project-local command+diff proof over current helpers and explicit group boundaries, that invalid
  command handling and grouping semantics are explicit, and that apply/undo-all and redo-all prove
  bit-identical live `Project` values. Do not start SQLite undo journaling, autosave durability
  semantics, UI gesture timing, export, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits,
  waveform cache changes, broad render rewiring, schema semantics, sampled/pixel/snapped/derived values
  as Project truth, or `[[clang::nonblocking]]` edits.
- **Latest: REVIEW/FIX H2 undo transaction grouping/property gate foundation is green locally.**
  Reviewed worker commit `3670bd8` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  Found and fixed one narrow mechanical proof gap: `YesDawProjectCheck` now directly proves grouped
  same-verb/different-Clip edits stay separate, while compatible `trimClip` and `setClipFades`
  sequences coalesce inside an explicit transaction group and still undo/redo back to bit-identical
  live `Project` values. The implementation stays explicit and headless: only compatible consecutive
  same-verb/same-Clip one-row diffs coalesce inside an active group; `splitClip`, unrelated verbs,
  unrelated targets, and ungrouped edits stay separate. The slice remains command+diff and Project-local
  only: no SQLite undo journaling, autosave durability semantics, UI gesture timing, export, plugin
  hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, broad render
  rewiring, schema semantics, sampled/pixel/snapped/derived values as Project truth, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (140/140). Remote CI run `28143357400` for
  worker commit `3670bd8` is green. Remote CI run `28143828792` for review/fix commit `385bb36`
  is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** WORKER H2 edit-sequence undo/redo property gate: add the smallest self-asserting headless
  sequence generator over the current Clip edit helpers and explicit transaction groups, proving
  apply/undo-all returns the live in-memory `Project` to the bit-identical original and redo-all returns
  it to the edited value. Keep it Project-local command+diff only; no SQLite undo journaling, autosave
  durability semantics, UI gesture timing, export, plugin hosting, H3 work, ADR edits, roadmap edits,
  golden edits, waveform cache changes, broad render rewiring, schema semantics, sampled, pixel,
  snapped, or derived values as Project truth, or `[[clang::nonblocking]]` edits. If property-test framework choice,
  undo persistence/autosave semantics, coalescing semantics, crossfade curve/shared-ramp representation,
  or any ADR-level decision rises, stop and report.
- **Latest: WORKER H2 undo transaction grouping/property gate foundation is green locally.**
  Added the smallest headless transaction grouping layer on top of the live in-memory command/diff undo
  stack for the current H2 Clip edit helpers. `ProjectUndoStack` now has explicit
  `beginTransactionGroup` / `endTransactionGroup` boundaries; inside an active group, only consecutive
  one-row same-verb/same-Clip edits coalesce (`moveClip`, `trimClip`, `setClipGain`, `setClipFades`).
  `splitClip` and unrelated verbs or targets remain separate undo entries. Coalesced entries keep the
  original before row and latest after row, so undo/redo still applies exact Clip row diffs against the
  live in-memory `Project`. The slice stays command+diff and Project-local only: no SQLite undo
  journaling, autosave durability semantics, UI gesture timing, export, plugin hosting, H3 work, ADR
  edits, roadmap edits, golden edits, waveform cache changes, broad render rewiring, schema semantics,
  sampled/pixel/snapped/derived values as Project truth, or `[[clang::nonblocking]]` edits. The new
  `YesDawProjectCheck` coverage proves grouped compatible sequences coalesce to the expected undo
  depth, unrelated grouped edits stay separate, ungrouped compatible edits stay separate, and grouped
  plus ungrouped sequences undo/redo back to bit-identical `Project` values. Local gate via documented
  Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass
  (139/139). Remote CI run `28143357400` is green.
  **Next:** REVIEW/FIX H2 undo transaction grouping/property gate foundation.
- **Latest: REVIEW/FIX H2 command/diff undo/redo foundation found no defects.**
  Reviewed worker commit `8caf091` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  `ProjectEditCommand` stays a named edit intent, and `ProjectUndoStack` records exact Clip row
  before/after diffs for `moveClip`, `trimClip`, `splitClip`, `setClipGain`, and `setClipFades`.
  Undo applies the recorded before rows; redo applies the recorded after rows; invalid commands and
  mismatched live Clip rows reject without Project mutation. The slice stays live in-memory Project
  only: Assets remain immutable; SQLite undo journaling, autosave durability semantics, UI interaction,
  export, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes,
  broad render rewiring, schema semantics, sampled/pixel/snapped/derived values as Project truth, and
  `[[clang::nonblocking]]` edits are untouched. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (137/137). Remote CI run
  `28142543112` for worker commit `8caf091` is green across Windows, Linux, macOS, RTSan, and TSan.
  Remote CI is pending until this status-only review/fix commit is pushed.
  **Next:** WORKER H2 undo transaction grouping/property gate foundation.
- **Latest: WORKER H2 command/diff undo/redo foundation is green locally.**
  Added the smallest headless in-memory command/diff undo/redo surface for the current H2 Clip edit
  helpers: `moveClip`, `trimClip`, `splitClip`, `setClipGain`, and `setClipFades`. `ProjectEditCommand`
  records the named edit intent, and `ProjectUndoStack` records exact Clip row before/after diffs on
  successful commands so a live in-memory `Project` can undo back to the bit-identical original value
  and redo back to the edited value. Invalid commands and mismatched live Project state are rejected
  without mutation. The slice stays metadata-only: Assets remain immutable; SQLite undo journaling,
  autosave durability semantics, UI interaction, export, plugin hosting, H3 work, ADR edits, roadmap
  edits, golden edits, waveform cache changes, broad render rewiring, schema semantics,
  sampled/pixel/snapped/derived values as Project truth, and `[[clang::nonblocking]]` edits are
  untouched. `YesDawProjectCheck` now proves a mixed sequence of all five current Clip edit helpers can
  apply, undo to the exact original `Project`, and redo to the exact edited `Project`. Local gate via
  documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (137/137). Remote CI run `28142543112` is green across Windows, Linux,
  macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 command/diff undo/redo foundation.
- **Latest: REVIEW/FIX H2 Clip gain/fade/crossfade envelope render projection foundation found no defects.**
  Reviewed worker commit `232e384` against `STATUS.md`, ADR-0010, ADR-0011, ADR-0012, the H2
  plan/deepening notes, and the current Time / Project / ProjectBundle / render and persistence tests.
  The decoded Clip bundle projection applies the existing `evaluateClipGainEnvelope` result to decoded
  Clip source-window samples before RT/offline graph rendering, so existing Clip `gain`, `fadeIn`, and
  `fadeOut` metadata affects rendered samples deterministically. Project truth stays metadata-only:
  Assets and bundled bytes are unchanged; no sampled, pixel, snapped, or derived sample values are
  stored back into Project truth. Crossfade remains adjacent per-Clip envelopes over existing metadata
  only; no shared crossfade object, `curve_type`, schema semantics, undo/redo, UI interaction, export,
  plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, broad render
  rewiring, or `[[clang::nonblocking]]` edits slipped in. Local gate via documented Windows DevShell
  flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (135/135). Remote
  CI run `28141683206` for worker commit `232e384` is green across Windows, Linux, macOS, RTSan, and
  TSan. Remote CI is pending until this status-only review/fix commit is pushed.
  **Next:** WORKER H2 command/diff undo/redo foundation.
- **Latest: WORKER H2 Clip gain/fade/crossfade envelope render projection foundation is green locally.**
  Updated `YesDawBundleRenderCheck` so the decoded Clip projection uses the existing
  `evaluateClipGainEnvelope` result before RT/offline graph rendering: existing Clip `gain`, `fadeIn`,
  and `fadeOut` metadata now affects rendered decoded samples deterministically. The gate compares
  Runtime and offline Render output against evaluator-derived expected samples and proves the previous
  constant-gain-only projection differs, so the envelope path is mechanically covered. Project truth
  stays metadata-only: Assets and bundled bytes are unchanged; no sampled, pixel, snapped, or derived
  sample values are stored back into Project truth. Crossfade remains adjacent per-Clip envelopes over
  existing metadata only; no shared-ramp representation, `curve_type`, schema semantics, undo/redo, UI
  interaction, export, plugin hosting, H3 work, ADRs, roadmap, goldens, waveform cache, or
  `[[clang::nonblocking]]` annotations were touched. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (135/135). Remote CI run
  `28141683206` for worker commit `232e384` is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 Clip gain/fade/crossfade envelope render projection foundation.
- **Latest: REVIEW/FIX H2 Clip gain/fade/crossfade envelope evaluation foundation found no defects.**
  Reviewed worker commit `e4bb7ae` against H2 scope, ADR-0010, ADR-0011, ADR-0012, the H2 deepening
  notes, and the current Time / Project / ProjectBundle / render and persistence tests. The evaluator
  stays pure derived evaluation over one Clip's existing `gain`, `fadeIn`, and `fadeOut` metadata at a
  Clip-local Tick: it returns either a finite scalar or an invalid result, and stores nothing back into
  Project truth. Assets, source-frame windows, timeline Tick metadata, `timeBase`, schema, undo/redo,
  UI interaction, export, plugin hosting, H3 work, ADRs, roadmap, goldens, waveform cache, and
  `[[clang::nonblocking]]` annotations are untouched. Invalid storage-unsafe Clip metadata and
  out-of-Clip positions are rejected. Adjacent per-Clip midpoint compatibility is supported only by the
  current ADR/deepening-note envelope shape; no shared-ramp representation, `curve_type`, or schema
  semantics were invented. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (135/135). Remote CI run `28140746988` for
  worker commit `e4bb7ae` is green across Windows, Linux, macOS, RTSan, and TSan. Remote CI is pending
  until this status-only review/fix commit is pushed.
  **Next:** WORKER H2 Clip gain/fade/crossfade envelope render projection foundation: use the existing
  `evaluateClipGainEnvelope` result in the smallest headless RT/offline Project projection gate for
  decoded Clips, so existing Clip `gain`, `fadeIn`, and `fadeOut` affect rendered samples
  deterministically without becoming Project truth. Keep Project truth metadata-only, Assets immutable,
  and sampled/pixel/snapped/derived sample values derived rather than stored. Do not invent a shared
  crossfade object, `curve_type`, schema semantics, undo/redo, UI interaction, export, plugin hosting,
  H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, or `[[clang::nonblocking]]`
  edits; if curve/shared-ramp representation semantics rise to ADR level, stop and report.
- **Latest: WORKER H2 Clip gain/fade/crossfade envelope evaluation foundation is green.**
  Added the smallest headless derived evaluator for existing Clip envelope metadata:
  `evaluateClipGainEnvelope` derives a finite gain scalar from a Clip-local Tick using only existing
  `gain`, `fadeIn`, and `fadeOut` fields plus the current equal-power fade polynomial. Project truth
  stays metadata-only: no sampled, pixel, snapped, or derived sample values are stored, and Assets,
  source-frame windows, timeline Tick metadata, `timeBase`, schema, undo/redo, UI interaction, export,
  plugin hosting, H3 work, ADRs, roadmap, goldens, waveform cache, and `[[clang::nonblocking]]`
  annotations are untouched. Crossfade remains adjacent per-Clip envelopes over existing metadata only;
  no shared-ramp representation, `curve_type`, or schema semantics were invented. `YesDawProjectCheck`
  now proves equal-power fade-in/fade-out evaluation, adjacent per-Clip midpoint compatibility, invalid
  Clip envelope metadata rejection, out-of-Clip position rejection, and no Project mutation. Local gate
  via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (135/135). Remote CI run `28140746988` for worker commit `e4bb7ae` is green
  across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 Clip gain/fade/crossfade envelope evaluation foundation.
- **Latest: REVIEW/FIX H2 Clip gain/fade/crossfade metadata foundation found no defects.** Reviewed
  worker commit `c3819cc` against H2 scope, ADR-0010, ADR-0011, ADR-0012, the H2 deepening notes, and
  the current Time / Project / ProjectBundle / render and persistence tests. The helpers stay pure
  metadata over the existing Clip fields: `setClipGain` / `setClipFades` mutate only storage-safe
  `gain`, `fadeIn`, and `fadeOut`; Assets, timeline Tick placement, source-frame windows, `timeBase`,
  schema, sampled/pixel/snapped values, undo/redo, UI, export, plugin hosting, H3 work, ADRs, roadmap,
  goldens, waveform cache, and `[[clang::nonblocking]]` annotations are untouched. Invalid requested
  envelope values and invalid pre-existing storage-unsafe Clip metadata are rejected without Project
  mutation; the persistence proof covers exact schema v1 write/read of edited gain/fade metadata.
  Crossfade remains adjacent per-Clip envelope metadata only; no representation or curve semantics were
  invented. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (132/132). Remote CI is pending until this
  status-only review/fix commit is pushed.
  **Next:** WORKER H2 Clip gain/fade/crossfade envelope evaluation foundation: add the smallest
  headless derived evaluator/gate for existing Clip `gain`, `fadeIn`, and `fadeOut` metadata so
  RT/offline Project projection can later apply one per-Clip gain envelope. Keep Project truth
  metadata-only, Assets immutable, and sampled/pixel/snapped values derived rather than Project truth.
  Treat crossfade as adjacent Clip envelopes only if the current ADR and H2 deepening notes are
  sufficient; if curve/shared-ramp representation semantics rise to ADR level, stop and report. Do not
  start undo/redo, UI interaction, export, plugin hosting, H3 work, ADR edits, roadmap edits, golden
  edits, waveform cache changes, or `[[clang::nonblocking]]` edits.
- **Latest: WORKER H2 Clip gain/fade/crossfade metadata foundation is green.** Added the
  smallest headless Project-level edit helpers for the existing Clip envelope metadata:
  `setClipGain` and `setClipFades`. The slice stays pure metadata over the current
  Asset→Clip→Project value surface: only existing Clip `gain`, `fadeIn`, and `fadeOut` values change;
  Assets remain immutable; timeline Tick placement, source-frame windows, `timeBase`, snapped
  sample/pixel values, schema, undo/redo, UI, export, plugin hosting, H3 work, ADRs, roadmap, goldens,
  waveform cache, and `[[clang::nonblocking]]` annotations are untouched. Crossfade-specific
  representation/curve semantics were not invented; this worker only exposes the existing adjacent
  per-Clip envelope fields that current ADRs already store. `YesDawProjectCheck` proves gain/fade
  edits mutate only envelope metadata and reject invalid requested or pre-existing storage-unsafe
  Clip metadata without Project mutation. `YesDawPersistenceCheck` proves edited gain/fade metadata
  writes and reads back exactly through the current SQLite snapshot. Local gate via documented Windows
  DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (132/132).
  Remote CI run `28139588321` for worker commit `c3819cc` is green across Windows, Linux, macOS,
  RTSan, and TSan.
  **Next:** REVIEW/FIX H2 Clip gain/fade/crossfade metadata foundation.
- **Latest: REVIEW/FIX H2 Clip split/trim/move metadata foundation is green.** Reviewed worker
  commit `a081414` against H2 scope, ADR-0010, ADR-0011, ADR-0012, the H2 deepening notes, and the
  current Time / Project / ProjectBundle / render and persistence tests. Found and fixed one narrow
  storage-facing validity gap: the edit helpers now refuse to mutate a Project whose existing Clip
  metadata would be rejected by schema v1, including negative timeline lengths and invalid `timeBase`
  values. The slice stays pure metadata: only Tick timeline starts/lengths and source-frame windows are
  edited; Assets remain immutable; snapped sample/pixel values are not stored as Project truth; and
  there are no schema, undo/redo, UI, export, plugin hosting, H3, ADR, roadmap, golden,
  waveform-cache, or `[[clang::nonblocking]]` edits. `YesDawProjectCheck` now also proves these
  storage-invalid Clip metadata inputs are rejected without Project mutation. Local gate via documented
  Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass
  (131/131). Remote CI run `28138884108` for review/fix commit `189e2ac` is green across Windows,
  Linux, macOS, RTSan, and TSan.
  **Next:** WORKER H2 Clip gain/fade/crossfade metadata foundation: add the smallest headless
  Project-level edit helpers and self-asserting gates for existing Clip `gain`, `fadeIn`, and `fadeOut`
  metadata, keeping edits storage-safe, Assets immutable, and sampled/pixel/snapped values derived
  rather than Project truth. Treat crossfade as adjacent Clip envelope metadata only if the current ADR
  and H2 deepening notes are sufficient; if representation or curve semantics rise to ADR level, stop
  and report. Do not start undo/redo, UI interaction, export, plugin hosting, H3 work, ADR edits,
  roadmap edits, golden edits, waveform cache changes, or `[[clang::nonblocking]]` edits.
- **Latest: WORKER H2 Clip split/trim/move metadata foundation is green locally.** Added the smallest
  headless Project-level edit helpers over the existing Asset→Clip value surface: `splitClip`,
  `trimClip`, and `moveClip`. The slice stays pure metadata: only Tick timeline starts/lengths and
  existing source-frame windows change; Assets remain immutable; snapped sample/pixel values are not
  stored as Project truth; and there are no schema, undo/redo, UI, export, plugin hosting, H3, ADR,
  roadmap, golden, waveform-cache, or `[[clang::nonblocking]]` edits. `YesDawProjectCheck` proves exact
  split adjacency (`right.srcOffset == left.srcOffset + left.srcLen`), exact unsnapped Tick placement,
  trim/move metadata preservation, and invalid-input rejection without Project mutation.
  `YesDawPersistenceCheck` proves edited Clip metadata writes and reads back exactly through the current
  SQLite snapshot. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (131/131). Remote CI run `28136942439` for
  worker commit `a081414` is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 Clip split/trim/move metadata foundation.
- **Latest: REVIEW/FIX H2 snap/grid tick math foundation found no defects.** Reviewed worker commit
  `f7975bb` against H2 scope, ADR-0010, the H2 deepening notes, and the current Time / Project /
  timeline-layout tests. The slice stays headless and narrow: `SnapGrid`, `snapTick`,
  `gridIndexForTick`, and `tickForGridIndex` are pure integer Tick/grid math; invalid grids are
  rejected; overflow is refused; snapped Tick↔grid-index round trips are exact and stable; and Project
  schema/persistence/timeline layout remain untouched, so no snapped sample or pixel values are stored
  as canonical Project truth. No Clip editing operations, undo/redo, UI, export, plugin hosting, H3
  work, ADR edits, roadmap edits, golden edits, waveform cache changes, or `[[clang::nonblocking]]`
  edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (127/127). Remote CI run `28135729287` for
  worker commit `f7975bb` and run `28135936744` for pre-review status commit `bb49b73` are green across
  Windows, Linux, macOS, RTSan, and TSan.
  **Next:** WORKER H2 Clip split/trim/move metadata foundation: add the smallest headless Project-level
  edit operations over the existing Asset→Clip value surface, keeping edits as pure metadata with Tick
  timeline positions and existing source-frame windows. Do not start gain/fade/crossfade, undo/redo, UI,
  export, plugin hosting, H3 work, ADR edits, roadmap edits, golden edits, waveform cache changes, or
  `[[clang::nonblocking]]` edits; if operation semantics rise to ADR level, stop and report.
- **Latest: WORKER H2 snap/grid tick math foundation is green.** Added the smallest headless
  integer snap/grid surface to the ADR-0010 time layer: `SnapGrid`, `snapTick`, exact grid-index
  readback, and checked grid-index→Tick derivation. Snapped values remain derived from Tick/grid inputs;
  no Project schema, persistence, Clip editing operations, undo/redo, UI, export, plugin hosting, H3
  work, ADR edits, roadmap edits, golden edits, waveform cache changes, or `[[clang::nonblocking]]`
  edits. `YesDawTimeCheck` now proves deterministic nearest-grid integer behavior, stable/idempotent
  snapping, exact snapped Tick↔grid-index round trips, invalid-grid rejection, and overflow refusal.
  Local gate via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (127/127). Remote CI run `28135729287` for worker commit `f7975bb` is green
  across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 snap/grid tick math foundation.
- **Latest: REVIEW/FIX H2 waveform peak-cache foundation is green locally.** Reviewed worker commit
  `fa62e3b` against H2 scope, ADR-0011, ADR-0012, the H2 deepening notes, and the current
  `ProjectBundleDb` / `Asset` / `Project` / bundle decode tests. Found no implementation defect:
  `WaveformPeakCache` is derived Project-adjacent state under `peaks/<hash>.ypeaks`, built from decoded
  Asset samples off the audio hot path, and delete/regenerate leaves canonical Project truth unchanged.
  Fixed one narrow mechanical proof gap by extending `YesDawBundleRenderCheck` so the untrusted peak
  parser now rejects wrong stored content hashes, truncated payloads, and NaN payloads in addition to a
  corrupt header. No Clip editing operations, undo/redo, UI, export, plugin hosting, H3 work, ADR edits,
  roadmap edits, golden edits, or `[[clang::nonblocking]]` edits. Local gate via documented Windows
  DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (124/124).
  Remote CI run `28134965007` for review commit `9eb0c6f` is green across Windows, Linux, macOS, RTSan,
  and TSan.
  **Next:** WORKER H2 snap/grid tick math foundation: add the smallest headless integer `snapTick` /
  grid round-trip gate for H2, keeping snapped values derived rather than Project truth. Do not start
  Clip editing operations, undo/redo, UI, export, plugin hosting, H3 work, ADR edits, roadmap edits,
  golden edits, or `[[clang::nonblocking]]` edits.
- **Latest: WORKER H2 waveform peak-cache foundation is green locally.** Added the smallest headless
  derived peak-cache surface for bundled Assets: `WaveformPeakCache` builds deterministic min/max+RMS
  tiers from decoded Asset samples, folds higher tiers 16:1, stores/loads a content-hash-keyed
  `peaks/<hash>.ypeaks` file, and rejects invalid cache files by header/hash/tier-shape/length/finite
  value validation so they can be discarded and regenerated. `YesDawPersistenceCheck` proves exact
  tier math on deterministic samples; `YesDawBundleRenderCheck` imports the fixture WAV into a `.yesdaw`
  bundle, decodes the bundled Asset on the control/test side, writes the peak cache under `peaks/`,
  reloads it, deletes `peaks/`, reopens Project truth unchanged, regenerates identical cache data, and
  rejects/replaces a corrupt peak header. No Clip editing operations, undo/redo, UI, export, plugin
  hosting, H3 work, ADR edits, roadmap edits, golden edits, or `[[clang::nonblocking]]` edits. Local
  gate via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (124/124). Remote CI is pending until this worker commit is pushed.
  **Next:** REVIEW/FIX H2 waveform peak-cache foundation.
- **Latest: REVIEW/FIX H2 bundled Asset read/decode projection found no defects.** Reviewed worker
  commit `2aba17e` against H2 scope, ADR-0011, ADR-0012, the H2 deepening notes, and the current
  `ProjectBundleDb` / `Project` / render-test surfaces. The slice stays headless and narrow:
  `DecodedClipNode` is a pure source node that reads pre-decoded samples on the hot path, `GraphBuilder`
  classifies it as `Source`, and `YesDawBundleRenderCheck` reopens a `.yesdaw` bundle, decodes the
  bundled immutable Asset through the existing JUCE WAV reader path on the control/test side, projects
  two non-destructive Clip source windows through Runtime and offline graph paths, compares both against
  expected decoded Clip output, asserts non-silence, and proves bundled Asset bytes are unchanged. No
  code defect found and no waveform cache/peaks, Clip editing operations, undo/redo, UI, export, plugin
  hosting, ADR edits, roadmap edits, golden edits, or `[[clang::nonblocking]]` edits. Local gate via
  documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (122/122). Remote CI run `28132790457` for worker commit `2aba17e` and run
  `28133086695` for pre-review `main`/status commit `9a91ddb` are green across Windows, Linux, macOS,
  RTSan, and TSan.
  **Next:** WORKER H2 waveform peak-cache foundation: add the smallest headless content-hash-keyed
  peak/mipmap cache gate for bundled Assets, with deterministic min/max+RMS tiers and safe
  delete/regenerate behavior. Keep it off the audio hot path and do not start Clip editing operations,
  undo/redo, UI, export, plugin hosting, ADR edits, roadmap edits, golden edits, or
  `[[clang::nonblocking]]` edits; if a cache-format decision rises to ADR level, stop and report.
- **Latest: WORKER H2 bundled Asset read/decode projection is green locally.** Added the smallest
  headless projection from bundled `.yesdaw` Asset bytes into the graph/Render path: `DecodedClipNode`
  plays pre-decoded Clip source windows, `GraphBuilder` classifies it as a `Source`, and
  `YesDawBundleRenderCheck` imports the fixture WAV into the bundle with content-hash Asset storage,
  writes a Project with two Clips referencing that same immutable Asset, reopens the bundle, decodes the
  bundled `.asset` bytes through the existing JUCE WAV reader path, and renders through both Runtime and
  offline graph paths. The gate asserts RT/offline equality, decoded-Clip expected output equality,
  non-silence, and unchanged bundled Asset bytes after projection. No waveform cache/peaks, Clip
  editing operations, undo/redo, UI, export, plugin hosting, ADR edits, roadmap edits, golden edits, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (122/122). Remote CI run
  `28132790457` for `2aba17e` is green across Windows, Linux, macOS, RTSan, and TSan.
  **Next:** REVIEW/FIX H2 bundled Asset read/decode projection.
- **Latest: REVIEW/FIX H2 asset import + copy-to-bundle recovery gate found no defects.** Reviewed
  worker commit `31ab1c0` against H2 scope, ADR-0011, ADR-0012, the H2 deepening notes, and the current
  `ProjectBundleDb` / `YesDawPersistenceCheck` surface. The implementation stays headless and narrow:
  source bytes hash to SHA-256, copy to a same-directory temp file in `audio/`, re-hash after copy,
  atomically rename to the content-addressed `.asset` path, dedupe repeated imports to the existing
  Asset row, and reconcile stale uncommitted `pending_fs_ops` rows on open. Open verifies committed
  Asset rows against their content-hash bytes and sweeps orphan final files out of `audio/`; tests cover
  dedupe, interrupted-import reopen cleanup, and missing/corrupt committed asset bytes. No code defect
  found and no ADR, golden, roadmap, waveform cache, Clip editing, undo, UI, export, broad decoding,
  plugin hosting, H3 work, or `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell
  flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (121/121). Remote CI
  run `28131177994` for `31ab1c0` and run `28131500386` for latest pre-review `main` are green across
  Windows, Linux, macOS, RTSan, and TSan.
  **Next:** WORKER H2 bundled Asset read/decode projection feeding the graph/Render path without making
  Clips destructive; keep it headless and do not start waveform cache, Clip editing, undo, UI, export,
  plugin hosting, ADR edits, roadmap edits, golden edits, or `[[clang::nonblocking]]` edits.
- **Latest: H1 exit-gate closeout / CI-truth pass is green.** Verified from repo truth that the four H1
  exit gates are represented by self-asserting tests and the latest pushed commit CI:
  Project bundle readback round-trips through `YesDawPersistenceCheck`; RT path vs offline Render
  equivalence is covered by `YesDawRenderCheck` with non-silence and `1e-6` max-abs diff; the audio hot
  path is covered by the Clang 20 RTSan CI leg over the pure engine tests; and interrupted save /
  interrupted migration reopen-clean recovery is covered by `YesDawPersistenceCheck` with
  `integrity_check == ok` and rollback/rerun assertions. Remote CI run `28125785485` for `ac4a576`
  is green across Windows, Linux, macOS, RTSan, and TSan. No ADR, golden, roadmap, code, or
  `[[clang::nonblocking]]` edits. **Next:** stop for Dan's H1/H2 horizon-boundary review; do not start
  H2 until Dan advances the horizon.
- **Latest: REVIEW/FIX H1 kill-during-save/migration reopen-clean gate is green locally.** Reviewed
  `bc5065b` against ADR-0012, ADR-0011, ADR-0010, `CONTEXT.md`, the H1 plan/roadmap, and the current
  SQLite bundle/migration/open-validation/readback code. Found and fixed one narrow test-proof gap:
  migration recovery now asserts the synthetic `schema_migrations.app_build = 'interrupted'` row did
  not survive, so reopen had to rerun and republish the v1 migration state. The save recovery gate
  already proves rollback to the last committed `Project` readback with `integrity_check == ok`. No ADR,
  golden, roadmap, UI, asset import/decoding, waveform cache, plugin hosting, broad automation lane, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --build --preset ci`; `ctest --preset ci` pass (118/118). **Next:** H1 exit-gate closeout /
  CI-truth pass; do not start H2 until Dan advances the horizon.
- **Latest: WORKER H1 kill-during-save/migration reopen-clean gate is green locally.** Added two narrow
  self-asserting recovery gates in `YesDawPersistenceCheck`: an interrupted save transaction closes
  without `COMMIT`, then the bundle reopens with `integrity_check == ok` and the last committed
  `Project` readback intact; an interrupted schema migration transaction writes v1 shape plus
  application/user identity without `COMMIT`, then reopen reruns migration cleanly and passes
  identity, `schema_migrations`, `integrity_check`, and semantic validation. No ADR, golden, roadmap,
  UI, asset import/decoding, waveform cache, plugin hosting, broad automation lane, or
  `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --build --preset ci`; `ctest --preset ci` pass (118/118). **Next:** REVIEW/FIX H1
  kill-during-save/migration reopen-clean gate.
- **Latest: REVIEW/FIX H1 RT-vs-offline Render equivalence gate is green locally.** Reviewed `968b16d`
  against ADR-0006, ADR-0007, ADR-0008, ADR-0009, ADR-0010, ADR-0011, `CONTEXT.md`, the H1 plan/roadmap,
  current Runtime/CompiledGraph/GraphBuilder/Node contracts, and the landed `YesDawRenderCheck` +
  CMake surface. Found no real defect: the gate stays inside the current `Project` value surface,
  builds two fresh `CompiledGraph`s from the same valid Project projection, exercises `Runtime`
  publish/process vs a direct offline graph with different Block schedules, and asserts non-silence,
  max-abs diff <= `1e-6`, plus graph-lifetime cleanup. No ADR, golden, roadmap, UI, asset
  import/decoding, waveform cache, plugin hosting, broad automation lane, kill-during-save/migration
  recovery, or `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (116/116). **Next:**
  WORKER H1 kill-during-save/migration reopen-clean gate for the current SQLite bundle/migration surface.
- **Latest: WORKER H1 RT-vs-offline Render equivalence gate is green locally.** Added
  `YesDawRenderCheck`, a narrow in-memory headless gate that builds a valid current `Project` value,
  compiles that same Project projection into two fresh `CompiledGraph`s, publishes one through
  `Runtime`, free-wheels the other as offline Render, slices the two paths with different Block
  schedules, and max-abs-diffs the audio within `1e-6` while asserting non-silence and graph-lifetime
  cleanup. No ADR, golden, roadmap, UI, asset import/decoding, waveform cache, plugin hosting, broad
  automation lane, kill-during-save/migration recovery, or `[[clang::nonblocking]]` edits. Local gate
  via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (116/116), with final build+ctest after the oscillator-backed refinement
  also green. **Next:** REVIEW/FIX H1 RT-vs-offline Render equivalence gate.
- **Latest: REVIEW/FIX H1 Project round-trip bundle readback slice is green locally.** Reviewed
  `e84e612` against ADR-0012, ADR-0011, ADR-0010, `CONTEXT.md`, this handoff, and the H1 Project
  round-trip gate. Found and fixed one real SQLite dynamic-typing defect: existing bundles now reject
  non-canonical storage types on the current `Project`/`Asset`/`Clip` value rows before readback can
  coerce them (for example, a fractional `src_offset` truncating through `sqlite3_column_int64`). Added
  a reopen regression proving that bad row is refused during layered open validation. No ADR, golden,
  roadmap, UI, asset import/decoding, waveform cache, plugin hosting, broad automation lane, or
  audio-thread contract edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (115/115). **Next:** WORKER H1 RT-vs-offline
  Render equivalence gate, with no golden-file edits unless Dan explicitly blesses that boundary.
- **Latest: WORKER H1 Project round-trip bundle readback slice is green locally.** Added
  `ProjectBundleDb::readProjectSnapshot`, the smallest SQLite readback path for the current
  `Project`/`Asset`/`Clip` value surface, with layered validation before reconstructing values from a
  reopened `.yesdaw` bundle. Added a mechanical round-trip regression proving project id/sample rate,
  Asset ids/content hashes/frames/sample rates/channels, and Clip ids/Asset refs/ticks/source windows/
  gain/fades/time_base survive close + reopen. No ADR, golden, roadmap, UI, asset import/decoding,
  waveform cache, plugin hosting, broad automation lane, or audio-thread contract edits. Local gate via
  documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (111/111). **Next:** REVIEW/FIX H1 Project round-trip bundle readback slice
  for the existing Project/Asset/Clip value surface.
- **Latest: REVIEW/FIX ADR-0012 SQLite `.yesdaw` bundle schema slice is green locally.** Reviewed
  `d12c2a8` against ADR-0012 plus adjacent Project/Time/Event/Automation contracts. Found and fixed one
  real open-validation defect: existing bundles now run the layered quick/FK/semantic validator before a
  database handle is returned, and the row-exists helper no longer treats SQLite step errors as "no
  problem." Added a reopen regression proving a semantically corrupt Clip source window is refused on
  open. No ADR, golden, roadmap, UI, asset import/decoding, waveform cache, plugin hosting, broad
  automation lane, or audio-thread contract edits. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (110/110). **Next:**
  WORKER H1 Project round-trip bundle readback slice for the existing Project/Asset/Clip value surface.
- **Latest: WORKER ADR-0012 SQLite `.yesdaw` bundle schema slice is green locally.** Added the first
  narrow, headless persistence surface in `src/persistence/ProjectBundle.h`: official pinned SQLite
  amalgamation wiring, `.yesdaw` package layout creation, WAL/NORMAL/FK/busy-timeout/autocheckpoint/
  cache/temp-store bring-up, `application_id`/`user_version`, transactional v1 migration harness,
  normalized schema v1 with real Clip→Asset FKs, semantic validation hooks for the existing
  Project/Time/Automation value types, reserved plugin-state chunk header table, and `pending_fs_ops`
  intent-log rows for cross-file asset/blob operations. Added `YesDawPersistenceCheck` coverage for
  bring-up pragmas, forward-schema refusal, migration rollback/no version bump on failure, FK
  enforcement, Project semantic rejection, semantic checks beyond SQLite `quick_check`, and intent-log
  commit/rollback atomicity. No ADR, golden, roadmap, UI, asset import/decoding, waveform cache, plugin
  hosting, broad automation lane, or audio-thread contract edits. Local gate via documented Windows
  DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (109/109).
  **Next:** REVIEW/FIX ADR-0012 SQLite `.yesdaw` bundle schema v1 + FKs + migration harness +
  intent-log atomicity.
- **Latest: REVIEW/FIX ADR-0009 sample-accurate automation evaluator slice is green locally.** Reviewed
  `2855204` against ADR-0009, ADR-0010, `CONTEXT.md`, `AGENTS.md`, this handoff, and the H1 contracts.
  Found no real defect: the helper stays pure/headless, preserves the fixed-size `EventStream` surface,
  advances by cursor, honors half-open Block boundaries, handles output capacity without writing past
  caller storage, and generated parameter Events flow into `FaderNode` at exact in-Block offsets. No
  ADR, golden, SQLite persistence, broad lane/UI work, MIDI note handling, plugin hosting, audio-thread
  contract, or `[[clang::nonblocking]]` edits. Local gate via documented Windows DevShell flow:
  `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass (103/103). **Next:**
  WORKER ADR-0012 SQLite `.yesdaw` bundle schema v1 + FKs + migration harness + intent-log atomicity.
- **Previous: WORKER ADR-0009 sample-accurate automation evaluator slice is green locally.** Added the
  pure C++ automation value/evaluator surface in `src/engine/Automation.h`: storage-facing
  `AutomationPoint { tick, value, curveType }`, the locked ADR-0009 curve enum, parameter target/block
  value types, and a cursor-style `evaluateAutomationPointsForBlock` helper that writes preallocated
  parameter `Event`s through a caller-supplied tick→frame mapper. Added `YesDawEventCheck` coverage for
  enum storage, value validation, half-open Block boundaries, cursor advancement, output-capacity and
  invalid-input handling, and generated automation Events driving `FaderNode` at exact in-Block offsets.
  No ADR, golden, SQLite persistence, broad automation lane/UI work, MIDI note handling, plugin hosting,
  or audio-thread contract edits. Local gate via documented Windows DevShell flow:
  `cmake --build --preset ci`; `ctest --preset ci` pass (103/103). **Next:** REVIEW/FIX ADR-0009
  sample-accurate automation evaluator slice.
- **Latest: REVIEW/FIX ADR-0011 EntityId + Asset/Clip/Project value surface is green locally.** Reviewed
  `aa4f4dc` against ADR-0011, ADR-0012, ADR-0010, `CONTEXT.md`, `AGENTS.md`, this handoff, and the H1
  contracts. Found and fixed one real ULID allocator bug: entropy exhaustion no longer wraps the
  internal entropy state and later emits lower same-timestamp IDs. Added mechanical coverage for
  carry/reset behavior, repeated exhaustion failure, next-timestamp recovery, and Project ID collision
  cases. No ADR, golden, SQLite persistence, broad automation, MIDI note handling, plugin hosting, UI,
  or audio-thread edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (99/99). **Next:** WORKER ADR-0009
  sample-accurate automation evaluator slice.
- **Latest: WORKER ADR-0011 EntityId + Asset/Clip/Project value surface is green locally.** Added the
  pure C++/JUCE-free storage-facing value surface in `src/engine/Project.h`: fixed 16-byte
  `EntityId`, a monotonic 128-bit ULID allocator, 32-byte Asset content-hash shape, minimal
  `Asset`/`Clip`/`Project` value types, and Project/Clip invariants for valid unique IDs, Asset validity,
  Clip→Asset references, and `clip.src_offset + clip.src_len <= asset.frames` without overflow. Added
  `YesDawProjectCheck` coverage in `tests/project_tests.cpp`. No ADR, golden, SQLite persistence, broad
  automation, MIDI note handling, plugin hosting, UI, or audio-thread edits. Local gate via documented
  Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`; `ctest --preset ci` pass
  (99/99). **Next:** REVIEW/FIX ADR-0011 EntityId + Asset/Clip/Project value surface.
- **Latest: REVIEW/FIX ADR-0009 generic event stream flowing param-changes slice is green locally.**
  Reviewed `cce212a` against ADR-0009, ADR-0008, ADR-0010, `CONTEXT.md`, and the H1 contracts. Found
  and fixed one real command/event interaction bug: after a gain parameter Event moved `FaderNode` away
  from the old command target, a later `SetGain` command back to that same old value could be swallowed.
  `FaderNode` now tracks `SetGain` commands with a lock-free revision counter, so equal-valued commands
  still override prior event targets while event targets persist across blocks when no command arrives.
  New coverage proves the edge case. No ADR, golden, persistence, MIDI note handling, plugin hosting, or
  broad automation evaluator edits. Local gate via documented Windows DevShell flow: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci` pass (94/94). **Next:** WORKER ADR-0011
  EntityId + Asset/Clip/Project value surface.
- **Previous: WORKER ADR-0009 generic event stream flowing param-changes slice is green locally.** Replaced
  the `EventStream` placeholder with the first ADR-0009 fixed-size event surface: trivially-copyable
  `Event`, CLAP-style `VoiceAddress`, parameter/note/SysEx payload space, non-owning block-sliced
  `EventStream`, and a validator for sorted half-open `[0, numFrames)` offsets. `FaderNode` now consumes
  its gain parameter changes from the shared stream at exact in-Block offsets while preserving the frozen
  `Node::process` shape and the existing `SetGain` command seam. New `YesDawEventCheck` coverage proves
  fixed-size shape, sorted/boundary validation, wrong-node filtering, exact offset flow, and cross-block
  target persistence. No ADR, golden, persistence, MIDI note handling, or broad automation evaluator edits.
  Local gate via documented Windows DevShell flow: `cmake --preset ci`; `cmake --build --preset ci`;
  `ctest --preset ci` pass (93/93). **Next:** REVIEW/FIX ADR-0009 generic event stream flowing
  param-changes slice.
- **Previous: REVIEW/FIX ADR-0010 time-model types slice is green locally.** Reviewed `7412597` against
  ADR-0010, ADR-0008/0009/0011/0012, `CONTEXT.md`, and the H1 round-trip contracts. Found and fixed one
  real validation gap: `TempoChange::hasValidBpm()` and `SampleRate::isValid()` now reject non-finite
  values, matching the finite-tempo-map / sane-project-rate persistence contract before schema code
  starts depending on these helpers. No ADR, golden, event-stream, or `[[clang::nonblocking]]` edits.
  Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (89/89). **Next:** WORKER
  ADR-0009 generic event stream flowing param-changes slice.
- **Previous: WORKER ADR-0010 time-model types slice is green locally.** Added `src/engine/Time.h` with
  the storage-facing time value surface: canonical `Tick`, `PPQ = 15360`, render-only `MusicalTime`,
  `TimeBase`, tempo/meter change records, `SampleRate`, resample quality tiers, non-owning tempo/meter
  map views, and the ADR-0010 `Transport` body used by `Node::process`. New `YesDawTimeCheck` locks
  PPQ, enum storage values, fraction validity, map-view shape, and the default project sample rate.
  No ADR or golden edits. Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (88/88).
  **Next:** REVIEW/FIX ADR-0010 time-model types slice.
- **Previous: REVIEW/FIX compiler slice K is green locally.** Reviewed `e88a6b4` against ADR-0006/0007/0008
  and the locked compiler design. No code defect found: `Runtime` routes `SetGain`/`SetPan` through the
  one ordered command queue to the `CompiledGraph` current at each command point, `applySetGain`/
  `applySetPan` use the sorted `idIndex_` lookup and return false for degenerate/missing/wrong-kind
  targets, and matched commands only mutate `FaderNode`/`PanNode` target state. `Node.h` stayed frozen;
  slice I/J pool, mute, carry-over, deterministic input ordering, and bus bind invariants stayed intact.
  Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (84/84). **Next:** WORKER
  time-model types (ADR-0010).
- **Previous: WORKER compiler slice K is green locally.** Runtime now routes `SetGain`/`SetPan` from the
  one ordered command queue to the `CompiledGraph` that is current at that command point, using the
  sorted `idIndex_` lookup. `CompiledGraph::applySetGain/applySetPan` return false for degenerate,
  missing-id, and wrong-kind targets; matched commands only call `FaderNode::setTargetGain` or
  `PanNode::setPan`. New coverage proves a gain command before a `SwapGraph` does not mutate the new
  graph, a gain command after the swap does, PanNode routing is audible in rendered samples, invalid
  scalar commands do not corrupt output, and degenerate graphs stay no-op. `Node.h` stayed frozen and
  slice I/J invariants stayed untouched. Local gate: `cmake --build --preset ci` and `ctest --preset ci`
  pass (84/84).
- **Latest: REVIEW/FIX compiler slice J is green locally.** Reviewed `b649acc` against the locked
  compiler design plus ADR-0007/0008. Node.h stayed frozen and slice K SetGain/SetPan routing did not
  land. Slice I pool invariants still hold: greedy width-sized f32 slots, slot 0 permanent silence,
  locked Fader/Meter-only R3 aliasing, separate f64 bus scratch, and order-shuffle invariance. Fixed one
  real slice J carry-over bug: synthetic PDC LatencyNodes now carry a full 64-bit `DelayCacheKey`
  alongside their low 32-bit diagnostic `NodeId`, so distinct latency delay rings cannot collide in the
  DelayCache during carry-over/reclamation snapshots. New coverage proves colliding low NodeIds still
  snapshot as distinct full keys. Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass
  (78/78). **Next:** WORKER compiler slice K (SetGain/SetPan command routing).
- **Previous: WORKER compiler slice J is green locally.** Pass 5 now assigns mute bits, exposes an atomic
  mute mask on `CompiledGraph`, carries `DelayNode` ring state from `previousForCarryOver`, sorts
  multi-input metadata by producer `NodeId`, and asserts/debug-checks that bus-style multi-input nodes
  were bound on the control thread. Runtime janitor reclamation snapshots delay rings before delete.
  New coverage proves mute flip without rebuild, matching delay carry-over continuity, mismatched
  delay-ring zero-fill/no-NaN output, deterministic input order, and an assertable unbound-bus failure.
  Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (77/77).
  **Next:** REVIEW/FIX compiler slice J. Do not start slice K until that review/fix checkpoint is green.
- **Previous: REVIEW/FIX compiler slice I is green locally.** Reviewed `cdbefd3` against the locked
  compiler design plus ADR-0007/0008. The Pass 4 pool shape is correct: greedy last-reader allocation
  is sized to live width, slot 0 remains permanent silence, R3 aliasing is limited to the locked
  Fader/Meter predicate, and Sum/Master f64 scratch metadata stays separate from f32 audio slots.
  Fixed two review gaps: the locked debug NaN pool paint is now compiled into the builder gate, and
  bus input binding no longer wraps at the exact `uint16_t` maximum fan-in. Local gate:
  `cmake --build --preset ci` and `ctest --preset ci` pass (72/72).
  **Next:** WORKER compiler slice J (Pass 5 mute + carry-over + bind-check). Do not start slice K until
  slice J is reviewed/fixed green.
- **Previous: WORKER compiler slice I is green.** `GraphBuilder` now performs Pass 4 greedy
  buffer-pool allocation: slot 0 is permanent silence, output slots are sized to live width instead of
  one per node, last-reader analysis covers multi-input readers, R3 in-place reuse is limited to the
  locked Fader/Meter predicate, and Sum/Master bus scratch gets separate f64 slot metadata. `CompiledGraph`
  now respects aliased node slots on the hot path. New mechanical coverage proves width sizing, slot-0
  exclusion, R3 positive/negative cases, multi-input last-reader protection, Sum/Master f64 scratch, and
  order-shuffle invariance for equivalent diamond graphs. Local gate:
  `cmake --build --preset ci` and `ctest --preset ci` pass (71/71).
  **Next:** REVIEW/FIX compiler slice I. Do not start slice J until that review/fix checkpoint is green.
- **Previous: REVIEW/FIX compiler slice H is green.** Reviewed `b418fd9` against the locked compiler design
  plus ADR-0007/0008. No code defect found: Pass 3 PDC is a single longest-path walk over topo/input
  metadata, synthetic `LatencyNode` splices are owned by the payload and excluded from command routing,
  flat `uint16` compiled-node/slot metadata remains bounded, and the tests mechanically catch both the
  old two-peak no-splice behavior and spurious single-input splices. Verified no slice I buffer-pool,
  slice J carry-over, or slice K SetGain routing landed in slice H. Local gate:
  `cmake --build --preset ci` and `ctest --preset ci` pass (65/65).
  **Next:** WORKER compiler slice I (Pass 4 buffer pool + order-shuffle invariance).
- **Previous: worker compiler slice H landed.** `GraphBuilder` now performs Pass 3 PDC:
  longest-path latency metadata, synthetic `LatencyNode` splices at convergence points, `totalLatency()`
  publication, and no spurious splice on single-input chains. Added test-only `StubLatencyNode`/impulse
  coverage proving a 2.0 peak lands at exactly frame N, the old unspliced two-peak failure is guarded,
  `totalLatency()==N`, single-input chains stay unspliced, and INT64_MAX/negative latencies fail loudly.
  Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (65/65).
- **Previous: REVIEW/FIX compiler slice G landed.** The review found one real validation
  gap: an over-wide bus fan-in could overflow the flat `uint16` input metadata and compile to silence
  instead of failing loudly. `GraphBuilder` now rejects unrepresentable reachable-node/input counts with
  `GraphTooLarge`; coverage also asserts empty-project silence, missing-master rejection, and negative
  latency rejection. Local gate: `cmake --build --preset ci` and `ctest --preset ci` pass (61/61).
- **Previous: compiler slice G landed.** `GraphBuilder` now performs Pass 1+2 validation and iterative
  Master-backward topo, rejects duplicate/missing/over-latency/cyclic graphs, allows `DelayNode`
  feedback boundaries, and builds the first real payload graph with `MasterNode` + `IdentityDcNode`.
  `CompiledGraph` runs the minimal one-slot/node executor while preserving the legacy `(GraphId, dc)`
  degenerate fast path.
- **Previous: REVIEW/FIX of compiler slice F landed.** The review found one real lifecycle gap:
  `CompiledGraph` owns prepared Nodes but did not call `Node::release()` before destruction. That is fixed
  on the janitor/control-side destructor path and covered by a `YesDawGraphCheck` lifecycle test.
- **Previous: `CompiledGraph` compiler slice F landed, then the macOS warning was fixed.** The graph has
  the additive ADR-0007 state/layout surface (`Payload`, flat compiled-node metadata, input-slot table,
  buffer-pool layout, mute mask, master output bookkeeping, id index) behind the preserved legacy
  `(GraphId, dc)` degenerate fast path. `src/dsp/ScopedNoDenormals.h` landed with the written R1–R7
  buffer-pool contract; no builder/audio executor path is reachable yet. AppleClang's
  `-Wunused-lambda-capture` warning in `tests/pan_tests.cpp` is removed.
- **Previous: the five built-in Nodes are in & green.** `DelayNode` (the one PDC+feedback primitive;
  `LatencyNode` is an alias), `FaderNode` (ramped gain), `PanNode` (equal-power mono→stereo, LUT),
  `SumNode` (f64 Bus summing, canonical NodeId order), `MeterNode` (peak/RMS, lock-free publish) — each
  its own independently-green commit behind the frozen Node trait, each `YESDAW_RT_HOT` with a
  cross-block-size invariance gate. `src/dsp/LinearRamp.h` is the per-frame ramp helper. The locked
  compiler implementation design remains
  [docs/plans/2026-06-23-compiledgraph-compiler-design.md](docs/plans/2026-06-23-compiledgraph-compiler-design.md);
  build commits G–K from there.
- **The Node contract (ADR-0008) is frozen + green.** `src/engine/Node.h` is the CLAP-shaped
  trait (`NodeProperties`/`AudioBlock`/`ProcessArgs` + `prepare`/`process`/`reset`/`release`/`directInputs`);
  `process` is `noexcept` + `YESDAW_RT_HOT` (RTSan-clean). First built-in `OscillatorNode` (wraps
  `SineSource`); the H0 throwaway Node stub is retired and block-size independence is re-asserted through
  the real trait. `EventStream`/`Transport` are placeholders fleshed out by ADR-0009/0010. CI green on
  `787d854` (RTSan/TSan/3-OS).
- **Foundation: the RT-safe graph-swap core (ADR-0006) is in and green.** `src/engine/Runtime.h`
  is the seam between the control thread and the one audio thread: one ordered **choc SPSC** command
  queue carries `SwapGraph` (with a `SetGain`/`SetPan` seam reserved); the audio thread owns `current_`
  and reads an immutable `CompiledGraph`; retired graphs go to an audio→control queue and a
  **generation-counter janitor** frees them on the strict-greater `processedGen > retiredAtGen`
  fence-post. Design was chosen by a **4-design adversarial panel + 3 judges**; the must-fix grafts are
  in (retire-queue backpressure, trivially-copyable POD command, `static_assert` lock-free, a debug
  canary, INVARIANT comments). 25/25 Catch2 tests pass locally (MSVC); a 2-thread stress test is the
  **new TSan leg's** target. RTSan covers `processBlock`. *(A real bug surfaced + fixed: choc's
  `getFreeSlots()` over-reports by one, so the backpressure gate now uses `getUsedSlots()`.)*
- **Verification: GREEN.** CI on `747f46a` passed every leg — Windows/Linux/macOS build + ctest,
  **RTSan** (audio hot path never allocates/locks) and the **new TSan** leg (the release/acquire
  reclamation contract has no data race). The concurrency core is now mechanically proven, not argued.
  A 4-design panel + 3-judge design pass and a 3-reviewer adversarial code review (7 findings, all
  fixed) preceded green. *(One CI-only bug fixed post-push: a `Config cfg = {}` default arg MSVC
  accepts but Clang/GCC reject.)*
- **H0 carry-over decided:** the native GPU render shell + `max_frame_ms<16.6` soak gate is **folded
  into H2** (UI work). H1's exit is 100% headless CI, so it does not block. The audio soak still stands.

## Current-horizon checklist — H3 (mixer + plugin hosting; plain English, small steps)
> Exit gate: two parallel paths, one with a real high-latency plugin, stay sample-aligned
> (PDC impulse test passes against the live plugin); pluginval L8-10 + `auval` pass in CI; and a
> plugin that crashes or hangs mid-session is isolated so the session survives with a crashed-plugin
> placeholder and the offender is blacklisted, without an audio dropout or audio-thread wait.
- [x] **ADR-0013 plugin state + hosting isolation. First chunk.** Lock opaque plugin-state chunks and
  out-of-process/sandboxed hosting before any H3 plugin-host code lands.
- [ ] Mixer as graph projection: Fader/Pan/Sum/Send/Return/Meter, solo/mute mask, Sidechain input pins.
- [ ] Automation lanes honoring per-Block offsets through the graph projection.
- [ ] Out-of-process plugin scanner with persistent blacklist and hang watchdog.
- [ ] `PluginNode` IPC proxy: shared-memory audio/event buffers, one-block fail-open pipeline, no audio
  thread wait on child process.
- [ ] VST3 + AU hosting behind `PluginNode`; CLAP comes after VST3/AU.
- [x] Opaque plugin-state persistence and corrupt-chunk graceful fallback.
- [ ] H3 mechanical gates: live high-latency-plugin PDC impulse, pluginval L8-10, `auval`, and
  crash/hang isolation with blacklist and no-dropout/nonblocking proof.

## Previous-horizon checklist — H2 (closed by Dan boundary review; editing-first)
> Exit gate (all green in CI): any edit sequence + full undo returns the document bit-identical; a
> split-with-crossfade Project's RT playback matches offline Render; **and** a kill mid-import recovers
> with the bundle's DB↔filesystem consistent (assets hash-verified, no orphans).
- [x] Import + copy-to-bundle with content-hash dedupe, staged temp writes, re-hash-before-rename, and
  intent-log/reconcile-on-open recovery. **First chunk.**
- [x] Bundled Asset read/decode projection feeds the graph/Render path without making Clips destructive.
- [x] Clip editing as metadata: split, trim, move, gain, fade-in/out, and equal-power crossfade.
- [x] Snap/grid round-trips exactly through integer ticks↔samples.
- [x] Command/diff undo/redo with transaction grouping and a property-based bit-identical undo gate.
- [~] Offline Render/Export for edited Projects, including split-with-crossfade RT-vs-offline coverage
  — split-with-crossfade RT/offline coverage is green; export UX is not part of this exit gate.
- [ ] Single-window timeline-primary shell with remappable keymap; native GPU render shell / frame-time
  gate comes here as the folded H0 UI carry-over.
- [x] **Exit gates green:** property undo · split-crossfade RT-vs-offline · kill-mid-import bundle
  consistency. Local gate is green; remote CI run `28146655906` for closeout commit `435d320` is green
  across Windows, Linux, macOS, RTSan, and TSan. Dan approved H2->H3.

## Previous-horizon checklist — H1 (closed; spine)
> Exit gate (all green in CI): a Project round-trips (tempo/meter map, markers, clips intact); the RT
> path matches an offline Render within golden tolerance; the audio path is RTSan-clean; **and** a kill
> during save/migration reopens cleanly (WAL recovery + `integrity_check`).
- [x] **Freeze the irreversible contracts as ADRs 0006–0012** ✓ — graph+PDC, time model, event model,
  Node contract, concurrency, data-model indirection, persistence. (docs-only; CI green by construction.)
- [x] RT-safe audio callback skeleton (`YESDAW_RT_HOT` + RTSan coverage) — `Runtime::processBlock`
  outputs silence from a `nullptr` graph, renders the installed graph otherwise. ✓
- [x] SPSC command queue + queue-applied graph swap + generation-counter janitor (ADR-0006) ✓ — one
  ordered choc queue (`SwapGraph`), audio-thread-local `current_`, audio→control retire queue, strict
  `processedGen > retiredAtGen` fence-post; backpressure not leak. RTSan + TSan legs cover it in CI.
  *(`src/engine/{CompiledGraph,Command,Runtime}.h`, `tests/{compiledgraph,runtime}_tests.cpp`.)*
- [x] `CompiledGraph` 5-pass compiler with PDC wired in; all built-ins report 0 latency (ADR-0007);
  PDC impulse test + cross-buffer-size invariance + order-shuffle invariant as Catch2 gates. **Design
  locked** ([compiler-design note](docs/plans/2026-06-23-compiledgraph-compiler-design.md)); build
  commits F (CompiledGraph state), G (Pass 1+2 + Master/IdentityDc + first render), H (PDC), I
  (buffer pool), J (mute + carry-over + bind-check), and K (SetGain/SetPan seam) are done and
  reviewed/fixed.
- [x] Built-in Nodes behind the contract (ADR-0008) — **all five in & green**: `OscillatorNode`,
  `DelayNode`/`LatencyNode`, `FaderNode`, `PanNode`, `SumNode` (f64 Bus summing), `MeterNode`. Each a
  separate green commit. *(Master = a top-level SumNode + device-wiring land with the compiler / H2.)*
- [x] Generic event stream flowing param-changes (ADR-0009) ✓ — fixed-size `Event`/`EventStream`,
  half-open sorted offsets, exact-offset Fader gain events, and the `SetGain` command seam review/fix
  are green.
- [x] Project data-model value surface (ADR-0011) — 128-bit EntityId/ULID surface plus Asset/Clip/Project
  value types and invariants, before SQLite persistence wiring.
- [x] Automation evaluated sample-accurately — curve storage is locked by ADR-0009; broad evaluator/lane
  work stays deferred until the current H1 plan calls it forward.
- [x] SQLite `.yesdaw` bundle: schema v1 + FKs + migration harness + intent-log atomicity (ADR-0012).
- [x] **Exit gates green:** Project round-trip · RT-vs-offline golden diff · RTSan-clean ·
  kill-during-save/migration reopen-clean. H1 done when all four are green in CI.

## Previous-horizon checklist — H0 (closed; GPU render shell + 60fps gate folded into H2)
- [x] Install the C++ toolchain (CMake + MSVC via VS 2022 Build Tools). ✓
- [x] `cmake -B build` configures and fetches JUCE with no error. ✓
- [x] App builds and a window opens (`YesDaw.exe`). ✓ — *`Main.cpp` compiled clean first try.*
- [x] A 440 Hz tone plays out real hardware (spike #1: device round-trip core). ✓
- [x] **Stand up CI + a self-asserting check harness** ✓ — GitHub Actions (Win+Linux+mac) via the `ci`
  preset builds + runs Catch2 `YesDawCheck` (golden + Goertzel/zero-crossing 440 Hz + RMS/peak/symmetry/
  DC purity + fade + perf); RTSan leg (`-fsanitize=realtime`, Clang 20) enforces no-alloc on the hot
  path; warnings-as-errors; `bless-goldens`. Recorded in ADR-0005. *(green; see `docs/ci-mechanical-verification.md`)*
- [x] Tame the spike (fade-in / lower level) ✓ — 50 ms fade-in + `noteOn/noteOff` in `SineSource`,
  −20 dBFS default; asserted by the fade-in check. *(start-stop UI deferred — spike.)*
- [x] Real-machine soak harness built ✓ — `YesDawSoak` opens the real device, counts xruns/deadline-
  misses → PASS/FAIL; now enforces the **128-frame** target (`--block-size`, the roadmap stress case)
  and, with `--loopback`, that the captured tone is actually **440 Hz**. Run with `tools/soak.ps1`
  (native Windows, no Git Bash) or `tools/soak.sh`. Audio is clean (0 dropouts) on the owner's box, but
  the 128-frame target needs a **low-latency driver** (ASIO/WASAPI-exclusive — shared-mode Realtek
  forces 480). **Owner runs the 10-min gate; loopback needs an out→in jumper.**
- [x] Load + scrub one WAV ✓ — `YesDawAssetCheck` decodes a committed fixture WAV, golden-diffs the
  440 Hz sine (≤1e-4), recovers pitch (zero-crossings), and scrubs (sub-range read == slice, bit-
  identical). CI green on Win/Linux/mac. *(spike #1 complete)*
- [~] GPU timeline 100+ elements at 60fps (spike #2) — **CPU half done + green**: pure viewport
  virtualization (`src/ui/TimelineLayout.h`, `YesDawUiCheck`) lays out a 5000-clip viewport in
  **0.0069 ms/frame** (~2400× under the 16.6 ms budget), so the whole frame is the GPU's. *Remaining
  (real-hardware): a native GPU render shell + `max_frame_ms<16.6` in the soak (NOT yet implemented).*
  Native is the chosen direction (plan-recommended + this spike's cost validation); the formal UI-stack
  ADR (fork #2) is written at H1 — until then "native" is a strong lean, not a locked ADR.
- [x] One Node behind a stub of the format-neutral trait (spike #3) ✓ — `YesDawEngineCheck` drives a
  `ToneNode` via the trait at block sizes 1/31/128/512/4096/9000 → bit-identical output, finite, no
  denormals. *(throwaway stub; the real Node contract is frozen at H1.)*
- [ ] **Exit = two soak gates on a real machine** (no human judgment):
  - **(a) audio — IMPLEMENTED:** `soak.sh`/`soak.ps1` exits 0 with `xruns==0`, `deadline_misses==0`,
    `block_size<=128`, and (with `--loopback`) RMS>0.01 dominated by 440 Hz.
  - **(b) GPU 60 fps — NOT YET IMPLEMENTED:** `max_frame_ms<16.6` requires the native render shell that
    doesn't exist yet, so the soak does NOT check it — a soak PASS today is the AUDIO gate only.
  H0 is done when both are green on one machine at a 128-frame Block.

## Done recently
- 2026-06-23 — **Foundation** committed: research corpus, CONTEXT glossary, ADR-0001/0002, roadmap, CLAUDE.md.
- 2026-06-23 — **Brainstorm**: direction locked — full general-purpose DAW; C++/JUCE + our own engine;
  audio + MIDI co-equal; linear timeline; editing-first; long-horizon.
- 2026-06-23 — **Plan** written; ADR-0003 (product) + ADR-0004 (stack); roadmap rebuilt; docs reconciled.
- 2026-06-23 — **Deepen-plan** applied: deepening-notes companion; loops section; decision #14
  (sample-rate); 10 simplifications adopted (8 scope-cuts rejected — full scope kept); housekeeping.
- 2026-06-23 — **Loop workflow adopted in full**; **3 H1 conflicts resolved** (15360-tick grid /
  128-bit ULID / out-of-process hosting).
- 2026-06-23 — **Codex plan review applied** (all 7 findings, no scope cut): made the snapshot /
  state-ownership / graph-publication model exact; promoted bundle crash-recovery into H1's gate;
  fleshed the out-of-process host runtime + isolation gate; PDC test now covers automation + events;
  sample-rate → H1 + automation-curve added as decision #15; fixed stale docs (adr/README, CLAUDE.md).
- 2026-06-23 — **Codex review round 2 applied:** plugin-IPC nonblocking contract (audio thread never
  waits on a child — one-block pipeline + fail-open); per-run state arenas (RT vs offline never share
  state); fixed persistence contradiction; H1 recovery gate = save/migration (import-kill → H2);
  marked resolved conflicts historical.
- 2026-06-23 — **H0 kickoff:** committed CMake + JUCE scaffold + sine spike (`src/Main.cpp`), `AGENT.md`,
  `.gitignore`. Unverified until the toolchain is installed and it's built.
- 2026-06-23 — **H0 spike #1 core WORKING:** toolchain in (MSVC 19.44 / CMake), JUCE fetched + built,
  `Main.cpp` compiled clean **first try**, `YesDaw.exe` plays a 440 Hz sine out real hardware. Full
  stack proven end-to-end.
- 2026-06-23 — **Mechanical-first model + CI cheat-sheet** committed (`docs/ci-mechanical-verification.md`)
  + `bootstrap/windows.ps1` (idempotent one-command toolchain install; fixes the winget-quoting pain).
  Standing up CI is the agent's first H0 task. Commit rule: frequent, straight to main, no squash.
- 2026-06-23 — **CI + harness LIVE and GREEN** (the first H0 task, done in full): extracted a pure
  `SineSource` from the spike; Catch2 `YesDawCheck` (golden + pitch + level + purity + fade + perf);
  GitHub Actions 3-OS matrix via the `ci` preset; warnings-as-errors (SYSTEM-demoted deps); RTSan leg;
  `bless-goldens`; ADR-0005. An **adversarial multi-agent review** caught + closed two real gate holes
  (golden window inside the fade; asymmetric distortion passing) — both proven via injected-bug tests.
  Built the **real-machine soak** (`tools/soak.sh` + `YesDawSoak`); verified on this box.
- 2026-06-23 — **H1 contracts frozen as ADRs 0006–0012** (the precondition for engine code): time model
  + sample-rate (keep-original / resample-at-read), Node contract, event stream + automation (all four
  curves), CompiledGraph + PDC, immutable-snapshot concurrency, Asset→Clip→Project + 128-bit ULID,
  SQLite bundle + migrations. Two owner product calls made; the resolved forks recorded; CONTEXT.md +
  the ADR index synced. Docs-only checkpoint → CI green by construction. GPU render shell folded to H2.
- 2026-06-23 — **RT-safe graph-swap core landed (ADR-0006)** — `src/engine/{CompiledGraph,Command,Runtime}.h`:
  immutable graph + one ordered choc SPSC command queue (`SwapGraph` + scalar seam) + audio-thread-local
  `current_` + audio→control retire queue + generation-counter janitor (strict `processedGen>retiredAtGen`).
  Design from a 4-design/3-judge adversarial panel; grafts applied (backpressure, POD command, lock-free
  `static_assert`, canary, INVARIANT comments). New **TSan CI leg** added. 25/25 local; choc
  `getFreeSlots()` off-by-one found + fixed. choc pinned (`5685fb5`). Then a 3-reviewer adversarial code
  review (7 findings, all fixed: canary→always-on, dtor contract, null-publish guard, …) — CI green on
  `747f46a` (RTSan + TSan + 3-OS).
- 2026-06-23 — **Node contract landed (ADR-0008)** — `src/engine/Node.h` (CLAP-shaped trait) +
  `src/engine/nodes/OscillatorNode.h`; H0 stub retired; block-size independence re-asserted through the
  real trait; `process` RTSan-clean. CI green on `787d854`.
- 2026-06-23 — **CompiledGraph compiler design panel + all five built-in Nodes landed.** A 4-design
  adversarial panel + 3 judges chose the ADR-0007 compiler implementation (spine = incremental-landing;
  grafts from PDC-correctness / RT-safety / simplest-correct) → locked in
  `docs/plans/2026-06-23-compiledgraph-compiler-design.md`. Then five built-ins, each an
  independently-green commit behind the frozen Node trait: `DelayNode` (the one PDC+feedback primitive,
  write-then-read so delay 0 passes through), `FaderNode` + `LinearRamp`, `PanNode` (equal-power LUT),
  `SumNode` (f64 Bus summing, canonical NodeId order, f64-cancellation gate), `MeterNode` (lock-free
  peak/RMS). Each has a cross-block-size invariance gate; `ci` gate green at every commit (47/47 local).
  Fixed three real bugs in the panel's sketch (include convention, delay-0 read/write order, f64
  test using 1e30 instead of 1e8). The 5-pass compiler itself (commits F–K) is the next chunk.
- 2026-06-24 — **CompiledGraph compiler slice F landed.** `CompiledGraph` gained the additive ADR-0007
  state/layout surface and `Payload` constructor while preserving the legacy `(GraphId, dc)` degenerate
  fast path for existing Runtime/CompiledGraph tests. `ScopedNoDenormals` landed for the real node
  executor path. Local `ci` build + 47/47 tests green.
- 2026-06-24 — **macOS CI warning fix.** AppleClang rejected an unnecessary lambda capture in the
  PanNode block-size test under `-Werror`; removed the capture. Local `ci` build + 47/47 tests green.
- 2026-06-24 — **Slice F review/fix.** Reviewed `a642ce9` and `b8c8e7c` against the locked compiler
  design plus ADR-0007/0008. Fixed one lifecycle contract gap: `CompiledGraph` now calls
  `Node::release()` for owned Nodes on destruction, and a new graph lifecycle test asserts it. Local
  `ci` build + 48/48 tests green.
- 2026-06-24 — **CompiledGraph compiler slice G landed locally.** Added `GraphBuilder` Pass 1+2
  validation/topo, `MasterNode`, `IdentityDcNode`, and the first payload-graph executor path. New
  `YesDawBuilderCheck` coverage proves IdentityDc→Master DC, Osc→Master non-DC, 1000-node iterative
  topo, non-Delay cycle rejection, Delay feedback-boundary allowance, duplicate/missing/latency
  rejection, and channel clamp. Local `ci` build + 57/57 tests green.
- 2026-06-24 — **Slice G review/fix.** Reviewed `af7a0b0` against the locked compiler design plus
  ADR-0007/0008. Fixed one real validation bug: over-wide fan-in / reachable-node counts that cannot fit
  the flat `uint16` compiled metadata now fail as `GraphTooLarge` instead of silently compiling a bad
  graph. Added coverage for that bug plus empty-project silence, missing master, and negative latency.
  Local `ci` build + 61/61 tests green.
- 2026-06-24 — **CompiledGraph compiler slice H landed locally.** Added Pass 3 PDC in `GraphBuilder`:
  longest-path latency walk, synthetic `LatencyNode` splices at convergence points, and published
  `totalLatency()`. Added test-only `StubLatencyNode` + impulse coverage for aligned convergence, the
  old unspliced two-peak guard, no single-input splice, and INT64_MAX/negative latency rejection. Local
  `ci` build + 65/65 tests green.
- 2026-06-24 — **Slice H review/fix.** Reviewed `b418fd9` against the locked compiler design plus
  ADR-0007/0008. Found no code defect: PDC is O(V+E), convergence and `totalLatency()` are covered,
  synthetic latency nodes do not enter command routing, metadata bounds are preserved, and no slice
  I/J/K behavior leaked into H. Local `ci` build + 65/65 tests green.
- 2026-06-24 — **CompiledGraph compiler slice I landed locally.** Added Pass 4 greedy buffer-pool
  allocation: last-reader liveness, exact-channel free lists, slot-0 silence preservation, locked R3
  in-place reuse for Fader/Meter only, and separate Sum/Master f64 bus scratch metadata. `CompiledGraph`
  skips pre-clear/pre-copy for aliased nodes. New builder coverage proves width sizing, slot-0 exclusion,
  R3 positive/negative cases, multi-input last-reader protection, bus scratch slots, and diamond
  order-shuffle invariance. Local `ci` build + 71/71 tests green.
- 2026-06-24 — **Slice I review/fix.** Reviewed `cdbefd3` against the locked compiler design plus
  ADR-0007/0008. Fixed two review gaps: added the locked debug NaN pool paint to the builder gate, and
  made Sum/Master input binding safe at the exact `uint16_t` maximum fan-in. Local `ci` build + 72/72
  tests green.
- 2026-06-24 — **CompiledGraph compiler slice J landed locally.** Added Pass 5 mute metadata/state,
  delay-state carry-over from `previousForCarryOver`, deterministic producer-id input ordering, and
  assertable bus bind checks without changing the frozen Node trait or landing slice K scalar routing.
  Runtime reclamation snapshots delay rings before delete. Local `ci` build + 77/77 tests green.
- 2026-06-24 — **Slice J review/fix.** Reviewed `b649acc` against the locked compiler design plus
  ADR-0007/0008. Fixed one real carry-over key bug: synthetic PDC LatencyNodes now keep full 64-bit
  `DelayCacheKey` metadata instead of relying on the low 32-bit diagnostic NodeId, and a regression
  asserts low-ID collisions remain distinct in the DelayCache. Local `ci` build + 78/78 tests green.
- 2026-06-24 — **Slice K review/fix.** Reviewed `e88a6b4` against ADR-0006/0007/0008 and the locked
  compiler design. Found no code defect: scalar commands route through the one ordered queue to the
  graph current at each command point, `idIndex_` lookup returns false for degenerate/missing/wrong-kind
  targets, `Node.h` stayed frozen, and slice I/J invariants remain intact. Local `ci` build + 84/84
  tests green.
- 2026-06-24 — **ADR-0010 time-model types landed locally.** Added the pure C++ time value surface
  (`Tick`, `PPQ = 15360`, `MusicalTime`, `TimeBase`, tempo/meter change records, sample-rate/resample
  tier records, non-owning map views, and `Transport`) plus `YesDawTimeCheck`. Local `ci` build + 88/88
  tests green.
- 2026-06-24 — **ADR-0010 time-model types review/fix.** Reviewed `7412597` against ADR-0010 and the H1
  round-trip/persistence contracts. Fixed one real validity gap: non-finite tempo BPM and project sample
  rates are rejected mechanically. Local `ci` build + 89/89 tests green.
- 2026-06-24 — **ADR-0009 generic event stream param-change slice landed locally.** Added fixed-size
  `Event`/`EventStream` shape, parameter/note/SysEx payload space, sorted half-open block validation,
  and exact-offset Fader gain parameter consumption through the frozen `Node::process` event slot.
  Local `ci` build + 93/93 tests green.
- 2026-06-24 — **ADR-0009 event stream review/fix.** Reviewed `cce212a` and fixed one real SetGain/event
  interaction bug: command revisions now let an equal-valued `SetGain` command override a previous event
  target, while event targets still persist across blocks without a command. Local `ci` build + 94/94
  tests green.
- 2026-06-24 — **ADR-0011 EntityId + Asset/Clip/Project value-surface review/fix.** Reviewed `aa4f4dc`
  and fixed one real ULID allocator bug: an exhausted same-timestamp entropy range no longer wraps the
  allocator state and later emits lower IDs. Added regression coverage for carry/reset, repeated
  exhaustion failure, next-timestamp recovery, and Project ID collision checks. Local `ci` build + 99/99
  tests green.
- 2026-06-24 — **ADR-0009 sample-accurate automation evaluator slice landed locally.** Added the pure
  automation point/evaluator surface and `YesDawEventCheck` coverage for stored point shape,
  half-open Block event emission, cursor advancement, capacity/invalid-input handling, and generated
  Events feeding `FaderNode`. Local `ci` build + 103/103 tests green.
- 2026-06-24 — **ADR-0009 sample-accurate automation evaluator review/fix.** Reviewed `2855204` and
  found no code defect: stored point shape, locked curve enum, cursor semantics, half-open boundaries,
  output-capacity handling, EventStream compatibility, and FaderNode generated-event flow all match the
  current narrow contract. Local `ci` build + 103/103 tests green.
- 2026-06-24 — **ADR-0012 SQLite bundle schema slice landed locally.** Added the headless SQLite
  persistence surface and `YesDawPersistenceCheck`: pinned SQLite amalgamation, `.yesdaw` bundle
  layout, v1 schema/migration harness, FKs, Project semantic validation, reserved plugin chunk header,
  and `pending_fs_ops` intent-log atomicity. Local `ci` configure/build + 109/109 tests green.
- 2026-06-24 — **ADR-0012 review/fix landed locally.** Existing bundles now run layered semantic
  validation during open, so corrupt stored Clip source windows are refused before callers receive a DB
  handle. Local `ci` configure/build + 110/110 tests green.
- 2026-06-24 — **H1 Project round-trip readback slice landed locally.** Added
  `ProjectBundleDb::readProjectSnapshot` and a reopened-bundle round-trip test for the current
  `Project`/`Asset`/`Clip` value surface. Local `ci` configure/build + 111/111 tests green.
- 2026-06-24 — **H1 Project round-trip readback review/fix.** Existing bundles now reject
  non-canonical SQLite storage types for the current `Project`/`Asset`/`Clip` value rows before
  readback can coerce them. Local `ci` configure/build + 115/115 tests green.
- 2026-06-24 — **H1 RT-vs-offline Render equivalence gate landed locally.** Added
  `YesDawRenderCheck`: the same valid current Project projection is rendered through Runtime and a
  free-wheeling offline Render driver with different Block schedules, then compared within `1e-6`.
  Local `ci` configure/build + 116/116 tests green.
- 2026-06-24 — **H1 RT-vs-offline Render equivalence gate review/fix.** Reviewed `968b16d` against the
  locked H1 contracts and found no code defect: the gate proves the narrow current Project -> CompiledGraph
  projection through both Runtime and offline paths without drifting into deferred surfaces. Local `ci`
  configure/build + 116/116 tests green.
- 2026-06-24 — **H1 kill-during-save/migration reopen-clean gate landed locally.** Added persistence
  recovery tests for uncommitted save rollback to the last committed Project and uncommitted schema
  migration rollback/rerun on reopen. Both assert `integrity_check == ok`; the migration path also
  asserts identity/schema row publication and semantic validation. Local `ci` build + 118/118 tests green.
- 2026-06-24 — **H1 kill-during-save/migration reopen-clean gate review/fix.** Reviewed the recovery
  tests against the locked persistence and Project/time contracts. Fixed one narrow test-proof gap: the
  migration recovery gate now proves the synthetic interrupted migration row did not survive reopen.
  Local `ci` build + 118/118 tests green.
- 2026-06-24 — **H2 bundled Asset read/decode projection landed locally.** Added
  `DecodedClipNode` plus `YesDawBundleRenderCheck`: a headless `.yesdaw` bundle imports the fixture WAV,
  reopens Project/Asset/Clip rows, decodes the bundled Asset file, renders two non-destructive Clip
  source windows through Runtime/offline graph paths, and proves the bundled Asset bytes are unchanged.
  Local `ci` configure/build + 122/122 tests green.

## Next
- ✅ **H1 approved and closed.** H1 contracts, graph/runtime spine, built-in Nodes, persistence,
  RT-vs-offline Render, RTSan, and save/migration recovery gates are green.
- ✅ **H2 approved and closed.** H2's mechanical exit gates are green: bit-identical edit undo/redo,
  split-with-crossfade RT/offline render, and kill-mid-import bundle consistency.
- **Next chunk: WORKER H3 mixer graph projection foundation.** Pull, read `AGENTS.md` + this handoff
  first, then add the smallest headless mixer projection over the frozen graph/Node contracts, using
  the existing Fader/Pan/Sum/Send/Return/Meter building blocks where they already exist and stopping if
  a new ADR-level mixer decision appears. Prove it with self-asserting tests only. Keep it headless and
  do not start plugin-host code, scanner code, plugin UI, CLAP loading, out-of-process runtime IPC,
  export UX, H4 work, golden edits, broad graph rewiring, sampled/pixel/snapped/derived Project truth,
  or `[[clang::nonblocking]]` edits. Run the documented gate: `cmake --preset ci`;
  `cmake --build --preset ci`; `ctest --preset ci`. If green, update `STATUS.md`, commit/push, check CI,
  then create the follow-up REVIEW/FIX thread. The loop continues worker -> review/fix -> worker until
  H3 exit gates are green, then stops for Dan's horizon-boundary review.

## Blocked / open threads
- Engine concurrency model (plan's *Threading & the real-time boundary* + *The graph* sections) is out
  for a **Codex re-verify** pass. H0 does not depend on it, so H0 proceeds in parallel.
