# 0081. Recover interrupted private import staging

- **Status:** Accepted (in-scope recovery decision; independent saved-data critic approved the lease binding and durability order before implementation)
- **Date:** 2026-11-03
- **Deciders:** build agent and independent saved-data critic, within Dan's editing mandate
- **Related:** [ADR-0012](0012-sqlite-bundle-schema-and-atomicity.md),
  [ADR-0080](0080-background-media-preparation.md)

## Context

The prepared-Asset seam copies source bytes into an exclusively created
`audio/.import-<32-lowercase-hex-id>/` directory before owner-thread adoption records
StageAsset in SQLite. Ordinary cancellation removes owned files, but process termination
does not run those destructors. The existing orphan sweep skips directories, so a crash
before adoption can retain source-sized private files indefinitely. This was reproduced
by static tracing during the saved-data critic pass; a child-process regression is required.

Recovery must distinguish an interrupted import from a live job, another job's files and
unrelated/replaced files. A directory name or item-name pattern alone is insufficient
deletion authority. Changing the general canonical-Asset orphan policy is outside this fix.

## Options

1. **An OS-held lease plus durable per-file cleanup receipts (recommended).** Hold the lease
   throughout all private snapshot owners, and reclaim only abandoned, identified files.
2. Flat temporary files reclaimed by the old sweep. This loses the unique-directory ownership
   boundary and cannot distinguish live jobs on another connection/process.
3. Leave interrupted directories for manual cleanup. This fails the mechanical recovery contract.

## Decision

Select option 1. This extends ADR-0080's private staging implementation without changing
Project entities, SQLite schema, canonical Asset names or undo semantics. The lease/receipts
are disposable filesystem cleanup coordination; they do not describe a resumable import,
save a pending edit, or authorize adopting a result after restart.

- A newly and exclusively created job directory contains one versioned lease/receipt file.
  Open it without following symlinks/reparse points, with a non-inheritable handle. Acquire
  an exclusive, nonblocking OS lock before publishing a valid header. Use `LockFileEx` with
  `LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY` on Windows and
  `flock(LOCK_EX | LOCK_NB)` on POSIX. Unsupported locking refuses preparation explicitly.
- The immutable header identifies this protocol/version, the job ID and captured filesystem
  identities of the bundle, audio and job directories and the lease file itself. The header's
  lease identity must match both the locked handle and the current no-follow marker path.
  Producer validation rechecks this binding before item mutation and adoption. Flush the
  header, its directory entry and the job's parent-directory entry before creating item bytes.
  Keep the lock through the shared directory owner until every staged/prepared snapshot owner
  is gone. Closing a wrapper must not release another snapshot's lease.
- Exclusively create each empty item file, obtain its filesystem identity, then append and
  flush a receipt containing its exact flat filename and volume/file identity, and flush the
  empty item's directory entry, **before copying bulk source bytes**. Retain the existing
  original-extension metadata. Do not put external
  paths or canonical Asset paths in receipts. Reject receipt names containing separators,
  parent references or the lease filename. A receipt never grants recursive deletion.
- Ordinary destruction keeps existing private/journal/canonical ownership boundaries. Remove
  the lease and directory only when no other files remain; otherwise retain recovery metadata
  and release the lock. Journal-owned files remain governed by StageAsset reconciliation.
- On bundle reconciliation, first apply the existing StageAsset recovery, then examine exact
  reserved job-directory names. Never follow a directory/lease symlink. Try the OS lock without
  waiting; a busy lease means a live owner and is skipped, including another owner in this process.
  Missing/invalid/unrecognized metadata is preserved rather than guessed at.
- After acquiring the lock, validate the marker and all captured directory identities again.
  Delete only exact receipt-named regular files whose current filesystem identity still matches.
  Preserve unreceipted files, replacements, subdirectories and symlinks. A malformed/truncated
  receipt grants no deletion authority; already complete valid receipts remain usable.
  Never remove canonical Assets through this mechanism. Flush directory changes as required
  by ADR-0012; report actual filesystem failures.
- A crash before a valid flushed header can leave an empty directory or tiny unrecognized
  marker. A crash between empty-file creation and its flushed receipt can leave an empty file.
  Preserve these unproven remnants; bulk audio is never copied in either interval. This small
  residual is preferable to deleting a live or foreign file based only on its name.

## Verification

Use an actual pure child process that stages bytes and exits without unwinding destructors.
Reopen must reclaim its identified private snapshot. Also prove that another live lease survives
reopen, snapshot ownership outlives its staging wrapper, unreceipted files and same-name replacements
survive, malformed metadata is preserved, and transferred canonical bytes remain protected.
Existing prepared-adoption, journal recovery and persistence suites stay green. No native app,
desktop input or audio/MIDI device is needed. Run Windows locally; POSIX proof comes from CI.

## References

- [Microsoft LockFileEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex)
- [Linux flock](https://man7.org/linux/man-pages/man2/flock.2.html)
- [Apple flock manual](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/flock.2.html)
