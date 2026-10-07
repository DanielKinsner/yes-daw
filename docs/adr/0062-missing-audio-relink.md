# 0062. Missing audio relink: an open that finds missing or damaged Asset files asks for each one, adopts only its original bytes, and otherwise leaves everything as it was

- **Status:** Accepted (agent, 2026-10-07, under ADR-0049's implementation-ADR clause, after a separate agent critic
  pass whose findings are folded in: the inspection is its own read-only entry point that requires the current
  schema and checks every Asset file; a trashed file never replaces earlier evidence; a decode mismatch after a
  correct relink ends in today's refusal; unused Assets, the length format, the hashing cost and autosave recovery are
  pinned. Committed alone, before any G5.7 code.)
- **Date:** 2026-10-07
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G5.7 ("Missing-asset relink. Add a relink chooser to the existing missing-asset report.
  Validate replacement media against the project's asset/source-window requirements before adopting it;
  cancellation or invalid replacement preserves the project and reports what remains missing. Gate: relink,
  refusal, cancel and save/reopen through the real chooser and bundle validator.") and SS-6 step 4 ("Reopen a
  scratch copy with a deliberately missing asset; cancel relink, reject an incompatible file, then relink
  successfully and assert the validator and render recover the intended content.") in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md); ADR-0011
  (an Asset is immutable and identified by the SHA-256 of its source bytes; clips' source windows are in its frames);
  ADR-0012 (bundle atomicity: files before rows, temporary files, the trash); ADR-0054 (the Asset row is the authority
  on reopen; a mismatch is reported); ADR-0055 (a clip's length follows the Asset's rate); ADR-0056 (how an Asset is
  named when it has no file name: its first clip, else its hash prefix); ADR-0059 (decoded buffers are checked once,
  where they are made).

## Context

An Asset's bytes live in the bundle as `audio/<hash>.asset`, copied at import. Opening a bundle verifies every Asset
file against its content hash and stops at the **first** one that is missing or different: the project does not
open, and the status line says "Open failed: committed asset bytes are missing: <path>" for a few seconds — a hash
file name the user cannot act on. Nothing records the original file's name or place, and there is no way to put the
audio back except by hand. The plan's G5.7 adds the relink; SS-6 step 4 drives it.

What is hard to reverse: what counts as an acceptable replacement (it decides whether a reopened project can sound
different from the one that was saved) and that a relink writes into the bundle.

## Options considered

1. **Only the original bytes are accepted (chosen).** The chosen file must hash to the Asset's content hash. This is
   the reading of the plan's "the project's asset/source-window requirements" taken here: the Asset's requirement
   *is* its identity (ADR-0011), and an "incompatible file" (SS-6) is any other. It guarantees everything else at once: the frames, rate and channels the
   rows record, every clip's, take's and pad's source window, and a render identical to the one before the file went
   missing. The common case is a moved or deleted copy whose original still exists (the user's sample library, a
   backup, another project's bundle).
2. **Any file with the same shape (frames, rate, channels).** Pro Tools' and Logic's looser relinks (by name and
   length) recover a re-exported or transcoded file, but the project may then sound different with no sign of it,
   the Asset's identity would change under every clip that uses it (a new hash under an immutable Asset), and SS-6
   requires the render to recover the *intended* content. Rejected; a re-exported file can still be imported as new
   audio and the clips moved to it, which is visible and undoable.
3. **Open with missing Assets marked offline (silent clips) and relink later.** Lets the user work at once, but
   every editor, the engine, export and autosave would need an offline Asset state — a much larger change than G5.7.
   Parked as a later item; this ADR keeps the open all-or-nothing.

## Decision

### Finding what is missing

- When an open's validator refuses an Asset file (missing, or bytes that do not match the content hash), the shell
  **inspects** the bundle through a new persistence entry point (beside `openExistingBundle`): it opens `project.db`
  **read-only**, requires YES DAW's application id and the **current** schema version (an older schema is not
  inspected: today's refusal stands — the failed open has normally brought the schema current already, as it does
  today), runs the stored-project checks, reads the project and checks **every** Asset row's file for presence and
  content hash, collecting each failure instead of stopping at the first. It runs no migration, no reconcile, no
  sweep and no pending-operation repair, and writes nothing.
- Each failing Asset is described from the stored project: **"<name> - <m:ss.s> <rate> kHz <mono|stereo>, used by
  <n> clip(s)"** (e.g. "Kick - 0:03.2 48 kHz stereo, used by 2 clips") — the name by ADR-0056's rule (its first
  clip's name, else the first eight hex digits of its hash), takes and Sampler pads counted with the clips; an Asset
  nothing uses reads "not used by any clip" and is asked about like the others (the validator requires every row's
  file).

### Asking, one Asset at a time

- Each missing Asset is asked about in turn: **Locate…** (a file chooser with the import's supported files) or
  **Cancel** (stops the open). The native shell uses a modal box (Enter = Locate…, Esc = Cancel) then the native file
  chooser; the harness answers through a new seam, `MainComponentFileChoices::chooseMissingAudioReplacement
  (const UiMissingAsset&) -> path` (empty = Cancel).
- **A chosen file is accepted only when its SHA-256 equals the Asset's content hash.** Any other file — a different
  recording, a re-export, a transcoded copy, an unreadable file — is refused with a reason that names it ("<file>
  is not the missing audio: its content differs" or the read error), and the same Asset is asked again.

### Adopting a file

- An accepted file's bytes are copied into the bundle as the import copies them (ADR-0012): to a temporary
  `audio/.<hash>.tmp`, hashed again, renamed to `audio/<hash>.asset`. The Asset row does not change (it already
  names that hash). A damaged file the copy replaces is moved to the bundle's `.trash` first under a free name
  (`<hash>.asset.<n>`), never deleted and never replacing earlier evidence. Only the bundle being opened is written,
  and only inside `audio/` and `.trash/`. A crash between files leaves only whole `.asset` files (each its row's
  bytes) and at most a stray `.tmp` that the next open sweeps.
- When every missing Asset has been adopted, the ordinary open runs again from the start (validator, decode), so a
  relinked project opens exactly as an intact one does. If the bytes match but the decode still refuses (the reader
  no longer yields the row's frames, rate and channels — ADR-0054), the open fails with today's refusal naming the
  decode reason; it does not ask again.
- The chosen file is hashed on the message thread, one Asset at a time (about a second per gigabyte): an accepted
  cost here, parked for G6's responsiveness work.

### Cancel and refusal preserve the project

- **Cancel** stops the open: the current project stays current and untouched (its undo history, its view), and the
  status line names what remains missing: "Open cancelled: <n> audio file(s) still missing (<name>, <name>…)".
  Files already adopted stay in the bundle being opened — they are its own original bytes, so the next open asks only
  about the rest.
- A refused file changes nothing anywhere.

### Where it applies

- File > Open, Open Recent and the launch reopen of the last project use the same path. At launch, Cancel leaves the
  untitled session (as a failed launch open does today).

### Gates (`[relink]`)

- Two Assets' files removed from a scratch copy: the open asks about both (named and described), a different file is
  refused with its reason and the same Asset asked again, the original files are accepted, the project opens, and its
  render is byte-identical to the render before the files were removed; it saves and reopens through the validator
  with no question.
- Cancel at the first question: the current project is still current with its undo, the bundle unchanged, and the
  status line names both missing files; cancel after adopting one: the next open asks only about the other.
- An Asset file with damaged bytes is asked about like a missing one; after relink the damaged file is in `.trash`.
- An Asset used only by a Sampler pad, an Asset nothing uses, and the launch reopen take the same path; an old-schema
  bundle is not inspected (today's refusal).
- The existing gates that pin today's refusal (`[missing-asset-open]`, `[prepared-project-load]`) are re-pinned: with
  no answer from the seam (Cancel) the open still refuses and keeps the current project, and the status line now
  reads "Open cancelled: … still missing (…)" naming the audio instead of "Open failed: committed asset bytes are
  missing: <path>".

## Consequences

- **Positive:** a project with moved or deleted audio can be recovered from inside the app, and it then sounds
  exactly as it did; the user sees what is missing in words, all at once.
- **Negative / accepted costs:** a re-exported or transcoded replacement is refused (import it as new audio instead);
  the open stays all-or-nothing (no offline clips); hashing a large chosen file takes a moment on the message
  thread; the original file's name is still not recorded (a later schema item could store it to suggest a location).
- **Follow-ups:** `CONTEXT.md` gains **Relink** (putting a missing Asset's original bytes back into the bundle).
  SS-6 step 4 drives it on the real app. Parked: offline Assets; searching a folder for the other missing files;
  hashing off the message thread; autosave recovery of a snapshot whose Asset files are missing (recovery keeps
  skipping such a snapshot, as today).
