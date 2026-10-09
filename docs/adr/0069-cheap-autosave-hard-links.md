# 0069. A cheap autosave: carry asset bytes by hard link, verify by identity at write time

- **Status:** Accepted (Dan, 2026-10-09: "Approve hard links (Recommended)"). ADR-0019's copied audio bytes may
  instead be carried by a same-volume hard link, with a copy where that is impossible. Dan approved the shared-byte
  consequence: the snapshot no longer keeps an independent copy of the audio. Everything else remains within the
  implementation-ADR clause of ADR-0049.
- **Date:** 2026-10-08
- **Deciders:** Dan (owner) for the ADR-0019 point; build agent (proposer); a design workflow (three designs, three
  judges); a separate agent critic (its findings resolved below)
- **Related:** ADR-0012 (SQLite bundle, WAL + `synchronous=NORMAL`, file-before-row asset imports); ADR-0019 (the
  autosave recovery contract); ADR-0035 (Restore / Discard over a validated snapshot); ADR-0036; ADR-0049; ADR-0058
  (the model-owned worker pattern - the escape hatch below); ADR-0060 (always-current bundle; Save As excludes
  `autosave/`); ADR-0062 (missing-audio relink: `adoptAssetFile`); ADR-0068 (autosave on the edit serial, write stamp,
  unresolved marker, retirement - its cp1/cp2 "writes on" lands after this ADR); the dogfood verdict 2026-09-01 ("feel
  before features").

## Context

**ADR-0068 is ready to switch the shipped autosave on, and one autosave freezes the app for seconds.** The parked
ADR-0068 cp1/cp2 patch passes every gate and the real-app SS-6 drive (178 of 178), but each autosave runs on the UI
thread every 30 s while there is unsaved work, and `persistence::writeAutosaveSnapshot`
(`src/persistence/AutosaveRecovery.h`) per call:

1. writes the project rows into a fresh temporary bundle and flushes its `project.db` (small);
2. **copies every asset file** into the snapshot and fsyncs each (`autosave_detail::copyProjectAssets`);
3. **SHA-256-hashes every asset again** to validate the temporary snapshot (`openExistingBundle` →
   `reconcileBundleFilesystem` → `verifyStoredAssetFiles`);
4. publishes with the two-rename swap.

**Measured on Dan's Windows NVMe machine (2026-10-08):** 10 × 50 MB of assets → **2.8–4.9 s of frozen UI per
autosave**. A real song (~1 GB of audio) would freeze the app for several seconds every 30 s while editing, and write
~1 GB per autosave (~120 GB of SSD writes per hour of editing). Carrying the same assets by hard link, flushing each
link and checking identity instead of re-hashing measured **~11 ms** (85 ms on the first, cold run). (The shipped app
writes no autosave today: nothing calls `PlaybackEngine::markProjectEdited`, so `needsAutosave()` is never true -
ADR-0068's finding; the cost appears the moment cp1/cp2 lands.)

**Asset bytes are immutable and already durable.** Assets are content-addressed (`audio/<sha256>.asset`); clips
reference them and audio is never edited in place (CLAUDE.md). An import writes and fsyncs the file before the row
that names it commits (`ProjectBundleDb::importAssetBytes`). The one path that replaces a file at an asset's name is
ADR-0062's relink (`ProjectBundleDb::adoptAssetFile`): it writes the chosen bytes to a temporary, hashes them, and
renames over the name only when they match the hash - so whatever inode an asset's name points at holds bytes that
match its hash. A snapshot linked before a relink keeps the old inode (still matching); one linked after gets the new.

**`autosave/` lives inside the bundle folder**, on the same volume. NTFS, APFS, ext4, XFS, ReFS and Btrfs support
`std::filesystem::create_hard_link` there; exFAT, FAT32, SMB1 and some cloud-placeholder files do not.

**Which check matters when.** The recovery guarantee ADR-0019 measures is at *Restore*: every reference in the
recovered project resolves to bytes whose hash matches - `readAutosaveSnapshot` / `restoreAutosaveSnapshot` open the
snapshot through the full validator, and that is where bit rot or a damaged copy is caught. The write-time validation
(step 3) re-hashes bytes hashed at import, from files nothing rewrote; that is the check every autosave pays O(project
size) for.

## Compatibility with Accepted ADRs

- **ADR-0019 (the owner's point):** the snapshot stays bundle-shaped (its own `project.db`, its own `audio/` names),
  durable before acceptance, published by the two-rename swap, and validated in full (every hash) wherever it is read
  for recovery. **Changed:** "Asset bytes referenced by the autosaved Project are *copied* into the autosave snapshot
  before it is accepted" becomes "*carried into* the snapshot before it is accepted - by a hard link to the bundle's
  own immutable asset file on the same volume, or by a copy where a link is impossible". What a user observes is the
  same (an autosave recovers the project, with its audio, validated); what differs is that the snapshot shares the
  bundle's audio bytes instead of holding a second copy. Accepted only with the owner's yes (ADR-0049).
- **ADR-0012:** no PRAGMA, schema or migration change.
- **ADR-0035:** Restore and Discard stay the only choices over a validated snapshot. Refinement: Restore refuses before
  writing anything when a referenced asset is missing or damaged, and says which.
- **ADR-0036, ADR-0060:** unchanged (Save As still copies `audio/` into fresh files and excludes `autosave/`; a link
  never crosses bundles).
- **ADR-0062:** relink's hash-before-rename invariant is what makes identity a sound substitute for a re-hash (above).
- **ADR-0068:** every section stands; its cp1/cp2 lands after this ADR's cp1-cp2 (below), rebased onto them.

## Decision

### 1. Carry by hard link; copy where a link is impossible; flush every name

`copyProjectAssets` becomes `carryProjectAssets`, which records each asset's carry (`Linked` or `Copied`):
- `std::filesystem::create_hard_link (bundle/audio/<hash>.asset, snapshot/audio/<hash>.asset)`; on any error
  (cross-device, unsupported, a placeholder, anything) it falls back to today's copy.
- Every carried name is flushed with `detail::flushFileToDisk` - for a link this commits the new name's metadata (on
  Windows `flushDirectoryToDisk` is a no-op, so a per-file flush is what makes the name durable, as it does today for
  a copy). Measured: ~11 ms for ten links with their flushes and identity checks.

### 2. Write-time validation: identity for links, hash for copies

The temporary snapshot is still opened with the full connection bring-up and every database check
(`validateStoredProjectSemantics`, finite reals, ranges). Its asset files are checked per carry: a `Linked` file must
be `std::filesystem::equivalent` to the bundle's file (same volume and file index) - no hash; a `Copied` file is hashed
(`verifyAssetFile`) as today. The orphan sweep and pending-operation reconcile are not run on the fresh snapshot (it
was written by `writeProjectSnapshot` alone: no pending operations, and its `audio/` holds exactly its rows' files).
The stamp carry (ADR-0068 §5) is unchanged: `writeAutosaveSnapshot` still reads the source stamp itself (no signature
change; every caller keeps compiling).

### 3. Restore carries the same way and refuses cleanly

`restoreAutosaveSnapshot`, per asset of the recovered project: the bundle's name already points at the snapshot's
inode → nothing to do; the name is missing (a lost-writes bundle's open swept it to `.trash/`; the snapshot's link kept
the inode alive) → link it back; the name points at a different inode → today's copy and hash. Any missing or
mismatched asset → **Restore refuses before writing anything**: the bundle, the snapshot and ADR-0068's unresolved
marker are untouched, and the status line names the asset.

### 4. What a drive and a user can see

The probe's `autosave` block gains `writeMs` (the last write's wall time), `writeMsPeak` (the session's highest) and
`linked` / `copied` (the last write's carries). Nothing else in the probe changes.

## Consequences

- Each autosave on a linkable volume writes the project database (tens to hundreds of KB) and a few names - no audio
  bytes - and costs on the order of ten milliseconds on this machine instead of seconds. ADR-0068 cp1/cp2 can ship.
- On exFAT / FAT32 / network / placeholder volumes the cost is today's (copy + hash); correctness is today's.
- The snapshot shares the bundle's audio bytes. A lost-writes recovery still works when the bundle's open swept an
  un-rowed asset away (the snapshot's link keeps the bytes). What a shared inode does not give is a second, independent
  copy of the audio inside the bundle folder - neither the bundle nor the autosave protects against the audio file
  itself being damaged on disk; that is a backup's job, not the autosave's (the autosave exists for lost database
  writes, ADR-0012 / ADR-0019).
- An autosave increases each asset file's link count to 2 (3 briefly during the swap) while one exists.

## Gates (`[autosave][cheap]`; each red on today's code)

- **`[no-asset-bytes]`** (persistence): an autosave of a 10-asset fixture reads no source asset byte and hashes nothing
  on the link path (a read counter on the carry and a `hashFile` call counter; release builds pay nothing). Red today:
  every asset is read and hashed.
- **`[inode-identity]`**: after an autosave every snapshot asset is `equivalent` to the bundle's. Red today: copies.
- **`[copy-fallback]`**: with an injected link failure for one asset, the autosave still publishes, that asset is
  `Copied`, and it **was hashed** (the counter moved for exactly that asset); recovery reads it back. Red today: no
  carry record, no seam.
- **`[link-count]`** (a fresh bundle, no sweeps): across 200 autosaves of the same project every asset's link count
  stays within 2..3 and is 2 between autosaves; Save (retirement) brings it back to 1. Red today: always 1.
- **`[restore-after-sweep]`**: lost writes drop an asset's row; the bundle's open sweeps its file to `.trash/`; Restore
  re-links it, reads no asset byte, and the restored bundle passes the full validator. Red today: Restore copies.
- **`[restore-refuses-missing-asset]`**: with an asset gone from both the bundle and the snapshot, Restore returns a
  failure naming it; the bundle's rows, the snapshot and the unresolved marker are unchanged. Red today: the copy fails
  mid-way.
- **`[contract-preserved]`**: the existing reliability recovery test (`tests/reliability_tests.cpp`) and ADR-0068's
  stamp gates pass unmodified.
- **`[hardware-cost]`** (hidden; a one-command local check, PASS/FAIL): a 10 × 50 MB autosave on the local disk takes
  under 100 ms. Not a CI gate (CI disks vary); the counts above are CI's proof.
- **Real app:** SS-6 step 6 additionally asserts `autosave.writeMsPeak < 100` and `autosave.linked > 0`.

## Checkpoints

- **cp1 — Carry by link, identity validation, the seams.** `carryProjectAssets`, the per-carry validation, the read /
  hash counters and the link-failure seam. Gates `[no-asset-bytes]`, `[inode-identity]`, `[copy-fallback]`,
  `[link-count]`, `[contract-preserved]`, `[hardware-cost]`. The shipped app still writes no autosave.
- **cp2 — Restore by link, clean refusal, probe fields.** Gates `[restore-after-sweep]`,
  `[restore-refuses-missing-asset]`; the probe fields.
- **Then ADR-0068 cp1/cp2** (the parked patch, rebased): writes on, with the SS-6 assertions above.

## Alternatives rejected

- **Keep copying, on a model-owned background worker (ADR-0058's pattern).** No UI freeze, ADR-0019's wording kept -
  but still ~1 GB written and hashed every 30 s of editing (SSD wear, background disk and CPU load beside playback and
  recording), plus a worker's coordination with Save, Save As, Don't Save, open and Restore. Kept as the escape hatch
  if a slow volume ever breaks the cost bound (follow-up).
- **A database-only snapshot that references the bundle's assets.** Cheapest, but the open-time orphan sweep would
  have to spare files an autosave references (every bundle open made to read the snapshot), and after lost writes a
  swept asset would exist nowhere in the snapshot. The durability judge's fatal flaw.
- **Skip write-time validation entirely.** Loses the database checks that catch a corrupt row set.
- **Copy each asset once into a persistent autosave store.** Doubles the project's size on disk permanently, and the
  first autosave after each Save (retirement) or open still copies everything.

## Follow-ups (not decided here)

- The background-worker escape hatch above, triggered only if `[hardware-cost]` or SS-6's bound reds on real hardware.
- A plain project open also hashes every asset (measured 3.0 s for 500 MB): a "verified at stamp N" skip deserves its
  own ADR.
- A real Windows directory flush (`FlushFileBuffers` on a directory handle) would make the two-rename publish's
  directory flushes do something on Windows; today they are no-ops there (pre-existing, outside this ADR).
- Cloud-placeholder folders (OneDrive, Dropbox): confirm on a real placeholder that a link attempt fails into the copy
  rather than linking a placeholder; one drive on such a folder before ADR-0068 cp1/cp2 ships.
