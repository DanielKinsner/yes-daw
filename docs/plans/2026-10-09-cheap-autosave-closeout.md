# Cheap autosave close-out checklist

Preparation only, 2026-10-09. [STATUS](../../STATUS.md) remains the live handoff; the
[active shell plan](2026-09-01-real-daw-ground-up-plan.md), especially sections 8.1-8.5 and the
Usable-song exit, remains authoritative. This checklist implements accepted
[ADR-0069](../adr/0069-cheap-autosave-hard-links.md) and
[ADR-0068](../adr/0068-autosave-and-recovery-write-stamp.md); it adds no scope, architecture decision,
exception or threshold. Unticked items are pending, not certified.

## Dependency and checkpoint rule

- [ ] Finish required proof for the current rendering repair and ADR-0069 cp1 before any dependent
  feature implementation, build or advancement. Inspect every required CI job on the exact code SHA;
  retain the current macOS frame failures and raw measurements. No automatic exception renewal:
  proceed only when the required repair proof passes or an applicable owner decision resolves the
  dependency. Independent preparation may continue meanwhile.
- [ ] For each small green unit below: applicable local checks and earlier/current Session drives,
  agent visual judgment and separate critic; update STATUS, commit, push, then inspect completed
  exact-code CI. Preserve rejected measurements and fixture/build identity. A docs-only green,
  cancelled run, diagnostic pass or alternate audio profile does not certify different code/hardware.

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

### 3. Expose real write cost (ADR-0069 cp2)

- [ ] Add only the accepted `autosave.writeMs`, `writeMsPeak`, `linked` and `copied` probe fields,
  measured by the control-thread writer. Preserve existing fields; no-op ticks must not replace
  the last write's measurements. Test actual carries, elapsed-time publication and peak retention.
  Keep the shipped trigger off until the following unit; no synthetic timing or carry credit.
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
