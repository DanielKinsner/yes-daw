# Cheap autosave close-out checklist

Preparation only, 2026-10-09. [STATUS](../../STATUS.md) remains the live handoff; the
[active shell plan](2026-09-01-real-daw-ground-up-plan.md), especially sections 8.1-8.5 and the
Usable-song exit, remains authoritative. This checklist implements accepted
[ADR-0069](../adr/0069-cheap-autosave-hard-links.md) and
[ADR-0068](../adr/0068-autosave-and-recovery-write-stamp.md); it adds no scope, architecture decision,
exception or threshold. Unticked items are pending, not certified.

## Dependency and checkpoint rule

- [x] Finish required proof for the current rendering repair and ADR-0069 cp1 before any dependent
  feature implementation, build or advancement. Inspect every required CI job on the exact code SHA;
  retain the current macOS frame failures and raw measurements. No automatic exception renewal:
  proceed only when the required repair proof passes or an applicable owner decision resolves the
  dependency. Independent preparation may continue meanwhile. Closed by normal exact-code CI
  `37988272626` on `3106943` (all ten jobs pass); see the cp1 close-out evidence record and STATUS.
- [ ] For each small green unit below: applicable local checks and earlier/current Session drives,
  agent visual judgment and separate critic; update STATUS, commit, push, then inspect completed
  exact-code CI. Preserve rejected measurements and fixture/build identity. A docs-only green,
  cancelled run, diagnostic pass or alternate audio profile does not certify different code/hardware.

**Writes-on readiness:** unit 4's implementation and unit 5's pre-shipping app proof share one
readiness boundary. Prepare independently verifiable drive/helper corrections first. Before the
first enabled-writer push, prove the complete local packaged candidate through the three SS8 laps
and the real hydrated-placeholder app drive. Split only independently verified trees; do not credit
an intermediate commit with a later mixed tree's evidence. Then push and complete exact-code CI.
This orders the accepted gates; it changes no ADR, threshold or owner contract.

## Small implementation units

### 1. Read recovery sources without changing them (ADR-0069 cp2)

- [ ] Replace the mutable recovery selection/read path in `AutosaveRecovery.h` / `ProjectBundle.h`.
  Keep live-slot preference, valid-previous fallback and refusal of `last.tmp`; distinguish absent
  slots from invalid ones and retain the concrete validation error. Reuse the validated project,
  stamp and slot rather than opening the selected source again through normal reconciliation.
- [ ] Preserve all database integrity, foreign-key, semantic, storage-type and canonical asset-path
  checks, then hash every referenced source asset before any target mutation. Recovery never uses
  cp1's write-time identity shortcut. Do not run source sweep, pending-operation reconciliation,
  write PRAGMAs or in-place migration. Legacy rows must be read through an isolated metadata view
  migrated with the existing migrations; preserve committed WAL content and pre-v35 stamp-zero
  behavior. The original DB, sidecars, assets and extra files remain untouched.
- [ ] Gate successful and refused reads, current and legacy schemas, invalid-live/valid-previous
  fallback, corrupt rows and missing/damaged audio. Compare source file inventories and bytes,
  including sidecars and an orphan that ordinary open would sweep. Preserve existing reliability,
  stamp and `[migration-silent-pre-v35]` gates. `inspectAssetFiles` currently rejects older schemas;
  calling it unchanged does not satisfy this unit.

### 2. Restore links or refuse before writing (ADR-0069 cp2)

- [ ] After complete source validation, carry each distinct asset: equivalent target name means
  no change; missing target means link back and flush the name, with copy/hash fallback on link
  failure; a different inode uses existing `adoptAssetFile` behavior. Replace names, never overwrite
  shared audio bytes. Commit recovered rows and clear the unresolved marker together only after
  successful carries; publish the recovered output only on success.
- [ ] Preserve the detailed asset reason through slot selection and
  `UiAppModel::restorePendingAutosaveSnapshot`; report it on the shared status line. Missing/damaged
  source refusal preserves target rows/files, source snapshots, output value, prompt and marker.
- [ ] Add `[restore-after-sweep]` and `[restore-refuses-missing-asset]`: actually lose a row and let
  normal open sweep its file; restore must relink it and pass full validation. Put a missing or
  damaged asset after one that would require a carry to prove preflight precedes all mutations.
  Cover forced link failure, an existing different inode and preservation of older shared names.
  Measure zero asset-byte work in the link carry separately from mandatory recovery hash reads;
  never remove full recovery validation to make the counter pass. Gate the shipped Restore action's
  named refusal and preserved recovery question as well.

Implementation checks from the separate preparation critic: handle `symlink_status` ENOENT as
absence, refuse other inspection errors, and keep dangling final symlinks on the existing-name
adoption path. Measure actual bytes transferred by the copy helper, separately from recovery hash
reads; equivalence alone cannot prove zero copy work. Flush newly carried final names, including
missing-name copy fallback, while preserving adoption's already-correct different-inode no-op.
Gate a real successful-link/failed-flush refusal without claiming rollback of names already carried.

### 3. Expose real write cost (ADR-0069 cp2)

- [ ] Add only the accepted `autosave.writeMs`, `writeMsPeak`, `linked` and `copied` probe fields,
  measured by the control-thread writer. Preserve existing fields; no-op ticks must not replace
  the last write's measurements. Test actual carries, elapsed-time publication and peak retention.
  Keep the shipped trigger off until the following unit; no synthetic timing or carry credit.
  Include a real nonzero copied count through model getters and shell JSON, then an all-link write
  returning it to zero. If test hooks span multiple translation units, enable them uniformly for
  the test target and keep the shipped app macro-free. Measure actual failed writer attempts as a
  coherent time/result sample; no-op ticks retain the previous sample and session peak.
- [ ] Close ADR-0069 cp2 only after its restore/refusal gates, existing cp1/reliability gates and
  checkpoint proof pass. Preserve the hidden local `[hardware-cost]` bound: 10 x 50 MB, under 100 ms.

### 4. Enable the model-owned writer safely (ADR-0068 cp1/cp2)

- [ ] Use the accepted model serials and due predicate: open bundle/engine, no pending question,
  unsaved work and an edit not already autosaved. Call `writeAutosaveSnapshot` directly; advance the
  autosaved serial/counts only on success. Reset/retire serials on the accepted lifecycle paths.
  Preserve the 30 s cadence and existing drive interval seam.
- [ ] Add pending skips, sticky `notDurable`, the accepted failure-injection seam and status report;
  failure leaves the serial due for retry, success clears the sticky flag. Expose ADR-0068 section 7
  fields and update its CONTEXT vocabulary. Gates: `[due]`, `[pending-gate]`,
  `[no-write-while-pending]`, `[status-sticky]`, including real filesystem failure.
- [ ] Re-ground the no-op/probe tests in actual due writes. Reuse, do not rebuild, the already-landed
  cp3/cp4 retirement and cp5 stamp/recovery behavior: exercise them with real scheduled snapshots,
  including Save, Save As, Save a Copy, Don't Save, unresolved restart, Restore and Discard. Retain
  engine helpers until the separate cp7 cleanup. Split commits only at independently safe gates.

Retain the accepted sticky flag until a real successful autosave, including across Save, retirement
and same-model attach. Canned test overrides retain the last real cost/carry sample. Gate pending
Save As after a nonzero edit: the new clean serials advance while the old unanswered snapshot and
marker survive. Discard must leave an unsaved edited tail due; it is not an autosaved clean point.

## Required frame-gate investigation

The original unit-1 CI run `38000812015` fails Windows dense-frame timing (17.2344 ms sustained,
16.6 ms limit). Its renderer, fixture and sampled checksum match the earlier certified code;
the cause is unknown. Before another renderer correction, preserve the existing diagnostic's
measurements even when its test passes: diagnostic-only Catch success output to an owned report,
cleared before CTest and printed/validated after it. Use documented CTest hooks, leave the workflow,
fixture, timer and assertions unchanged, and prove missing/stale capture cannot earn evidence.
One instrumented Windows observation may then separate paint stages from context teardown.
It is diagnostic only; normal-mode exact-code CI remains required. The compiler-only correction
`0b1c6b4` is neither a renderer repair nor a repeat requested to obtain a passing timing. Its completed
CI `38002137946` passed Linux and macOS but again failed Windows timing (18.4398 ms sustained,
19 slow frames against 8 allowed; other 438 checks pass). Collect one instrumented Windows observation
using the reviewed temporary GitHub Actions switch; remove that switch before clean certification.

## App and milestone evidence

### 5. Prove the writer through the app (ADR-0068 cp6)

- [ ] In `ss8-project-lifecycle.ps1` (logical SS-6), capture `autosave.writes` **before each edit** and
  pass that baseline into the wait. The current helper samples after the edit and can miss the only
  due write. Require a new confirmed write and `lastAutosavedEditSerial == editSerial`, then capture
  its counts; do not add sleeps or relax the wait to hide the race.
- [ ] Run the three accepted laps on the candidate packaged exe: kill/reopen preserves the current
  bundle without a question; restoring the saved scratch DB after a kill raises the lost-writes
  question and shipped Restore recovers the confirmed snapshot; Save/close/reopen retires it without
  a question. Keep the stamp and recovered-state assertions. On the linkable fixture add
  `autosave.writeMsPeak < 100` and `autosave.linked > 0`. The recorded 171/177 autosave-off result is
  a failure baseline, not cp6 completion.
- [ ] Before writes-on ships, run a self-asserting real-app drive on actual hydrated placeholder
  assets where the OS rejects hard links: verify copied carries, valid hashes and recovery, with
  isolated scratch setup and cleanup. Reuse the verified fixture approach in
  `tests/autosave_cloud_tests.cpp`; its persistence result alone is not app-drive evidence or proof
  of OneDrive/Dropbox synchronization. Keep this fallback case distinct from the linkable fixture.

### 6. Remove the obsolete engine trigger separately (ADR-0068 cp7)

- [ ] After the model writer is established, delete `needsAutosave`, `markProjectEdited`,
  `markAutosaved` and `writeAutosaveFromControlTick`, their obsolete state and test remnants in a
  separate green cleanup commit. Require `[autosave][one-writer]` in its final form: no old symbols
  under `src/` or `tests/`; the production snapshot caller is `UiAppModel::writeAutosaveTick`.
  This cleanup does not block cp6 and must not be folded into enabling writes or changing recovery.
  Complete it before the final milestone sign-off below.

### 7. Close the named milestone only on complete proof

- [ ] Re-run applicable earlier/current journeys and feel budgets; obtain the required agent rubric
  coverage. Bind results to the exact code SHA, executable/package hashes and versioned fixtures.
  Use verified isolated input or a current authorized hands-off window; missing access stays pending.
- [ ] Build the existing portable package, pass its packaged self-check and whole Usable-song arc,
  and obtain the required packaged hardware-playback result using the existing checker and targets.
  Keep raw results and genuine measurement-generated evidence; device-free fixtures or a build-tree
  checker are not packaged hardware credit. Do not expand this exit into recording/plugin proof,
  alpha publication or a new hardware threshold.
- [ ] Apply active-plan section 8.5: portable evidence and montage, STATUS and roadmap pointers,
  exact-code CI, all required journeys/budgets/rubric and packaged results. Certify only satisfied
  exits and stop at the authorized finish line; preserve any unresolved dependency explicitly.

No new feature decision is requested. The existing macOS sustained-frame exception renewal remains
unapproved; only an applicable owner decision can renew it if repair proof does not close that gate.
