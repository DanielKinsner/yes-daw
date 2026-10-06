# 0058. Export v2: a worker job over an immutable snapshot, a temporary sibling committed only on success, integer formats with TPDF dither, ranges, export stems and normalize

- **Status:** Accepted (agent, 2026-10-06, under ADR-0049's implementation-ADR clause, after a separate agent
  critic pass whose findings are folded in: dither streams are seeded per file so stems never share noise and the
  stem-sum gate runs on float output; the whole-project range is today's render length, a reverb/delay tail is
  parked; stale `.partial` files are found by a folder glob; export stems are the top-level strips only (no double
  counting, Bus-to-Bus routing resolved) at the master's channel count; the responsiveness gate uses a
  deterministic worker latch, not timing; normalize, progress units, the Committing state, file-name rules and the
  memory ceiling are pinned. Committed alone, before any G5.3 code.)
- **Amended:** 2026-10-06, before publication: a successful export stays quiet on the status line (the existing
  law, `UiAppModel` and the `[status-line]` gate), rather than saying "Exported …" — found while implementing cp1.
- **Date:** 2026-10-06
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G5.3 ("Export v2, in three checkpoints") and SS-6 step 5 in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md); backlog R31
  (export off the message thread) in `docs/goals/2026-08-25-reality-run-backlog.md`; G5.4 (decoded-asset sharing,
  which must keep this ADR's ownership contract); CONTEXT **Export**, **Render**, **Canonical export WAV**, **Stem**;
  the H7 canonical float WAV and its bit-exact gate; ADR-0055 (the offline resampler tier an export uses); ADR-0053
  (the loudness tap reads the live mix only — an export passes none); ADR-0048 (Sampler pads reference Assets);
  ADR-0046 §6 (nothing a running thread may hold is freed under it); ADR-0002 (an export never runs on the audio
  thread).

## Context

Today `UiAppModel::exportAudioFile` renders the whole project and writes the file in one call on the message
thread. The shell already shows a progress readout and a Cancel button, but neither can work: the percent jumps
0 → 100 inside the call and nothing reads the cancel flag. A three-minute, sixteen-track song freezes the app for
seconds. The render borrows spans into the model's decoded audio (`makeDecodedViews`); an import, a recording, an
undo or opening another project reallocates or frees that storage, so the render cannot simply be moved to a thread.
The writers open the destination with `trunc`, so a failure mid-write leaves a half file where the user's previous
export was. Only 32-bit float output is bit-exact today; 16 / 24-bit output rounds with no dither; there are no
stems and no normalize. SS-6 step 5 asks for a range and stems with format options, a cancel mid-job that leaves no
partial output and no overwritten destination, and a project replaced during an export whose completion belongs to
the original job only.

What is hard to reverse: the ownership contract (what a job owns versus borrows) — G5.4 and every later background
job (freeze, bounce, analysis) inherit it — and the on-disk commit law.

## Options considered

1. **A worker thread per export over an immutable snapshot it owns (chosen).** The message thread builds the snapshot
   (a deep copy of the project and owning references to its audio) and hands it to the worker; nothing else is
   shared but atomics. Edits, imports and project replacement proceed freely.
   - Pros: no lifetime coupling to the model; cancel and progress are atomics; a stale job is just an object.
   - Cons: the snapshot copies the same-rate decoded audio until G5.4 shares it (memory and a short copy).
2. **Render in slices on the UI timer (message thread).** No threads.
   - Pros: no ownership questions.
   - Cons: the render still borrows the model's spans, so every edit during an export must be refused or must
     restart it; heavy slices still stall paint; the plan asks for a worker.
3. **Lock the model's audio while a worker renders.** The worker reads the live storage under a mutex.
   - Pros: no copy.
   - Cons: imports, recording commits and edits block behind a long render; a lock across the model is exactly the
     coupling ADR-0046 removed. Rejected.

## Decision

### Checkpoint 1 — the job

- **One job type.** An **export job** owns: an id; its **snapshot** — a deep copy of the project (in view frames,
  ADR-0055's `projectInViewFrames`, when it has cross-rate Assets), the options (destination, format, dither, range
  in frames, normalize, stems) and, for every Asset a clip or a Sampler pad uses, an owning
  `shared_ptr<const AssetSamples>`; the files it writes; and the worker `std::thread`. Same-rate Assets are copied
  once per Asset (not per clip) into those owners when the job starts (G5.4 later makes the model's own storage
  those shared buffers, removing the copy); cross-rate Assets keep their decoded source in the snapshot and the
  **worker** builds their offline-tier views. The worker reads only its snapshot and writes only its atomics, its
  result and its files: never the model, the shell, the project bundle database or the status line. The render
  passes no loudness tap (the live tap is the mix's, ADR-0053).
- **Shared state is atomics.** `state` (Preparing, Rendering, Writing, Committing, Succeeded, Failed, Cancelled),
  `workDone` / `workTotal` and `cancelRequested`. Work is counted in frames over two phases per output file —
  rendering it, then writing it — so `workTotal = files × rangeFrames × 2` and the bar moves evenly. The worker
  stores its result text before the terminal state with release order; the message thread reads it after an
  acquire load of a terminal state.
- **Progress and completion are polled on the UI tick.** The model's `serviceExport()` (called each UI tick, as
  `serviceAudition` is) turns `workDone / workTotal` into the existing percent, and when the active job is terminal
  it joins the thread (already finished, so the join is immediate), reports the outcome on the status line
  ("Export cancelled", "Export failed: song.wav: <reason>"; a success stays quiet — the status line's existing law
  — and the readout shows 100 %) and bumps the existing export count only on Succeeded — an aborted job never
  reports success. The model holds at most one **active** job; a second
  export while one is active is refused: "Export refused: an export is already running".
- **Cancel.** The existing Cancel button / `AudioExportCancel` sets `cancelRequested`. The renderer checks it once
  per render block (128 frames) and the writer once per write chunk; a cancelled job ends Cancelled. The readout
  shows "Cancelling…" until then. Once the job is **Committing**, cancel is ignored: the job ends Succeeded, or
  Failed with the destination's state named.
- **Stale jobs.** Opening, creating or restoring another project (every `attachProjectBundle` path) cancels the
  active job and moves it to a **retiring** list without joining; `serviceExport()` joins retiring jobs once
  terminal and reports nothing for them, so a job's completion can never touch a replacement project. App shutdown
  cancels every job and joins it in the model's destructor; the wait is one render block or one write chunk plus
  the job's temporary-file removal (on a stalled volume, that I/O — an accepted cost).
- **Rendering gains two optional hooks** in `OfflineRenderOptions`: a progress counter (atomic frames done) and a
  cancel flag, both null by default, so every existing caller and gate is unchanged; a cancelled render returns a
  Cancelled status and no audio. A third, test-only hook, a **latch** (hold the worker after block N until
  released), lets the gates prove concurrency without timing.

### Checkpoint 2 — committing to disk

- **A temporary sibling, committed on success only.** Every output file is first written as
  `<destination file name>.<job id>.partial` in the destination's folder (same volume, so the commit is a rename),
  streamed in chunks (the writer gains an open / append / finish form that patches the RIFF sizes at finish),
  flushed and closed, then renamed over the destination (`std::filesystem::rename`, which replaces an existing
  file).
- **Cancel or failure before the commit removes every temporary file and leaves the destination as it was**
  (absent, or the user's earlier file byte for byte). A failure names its cause. A multi-file export commits its
  files one by one only after all of them are written; a failure in the commit phase reports which files were
  committed.
- **Stale temporaries.** A crash can leave `.partial` files. Starting an export removes, in the destination's
  folder, every file matching `<destination file name>.*.partial` (for a stems export, the same for each output
  name) before writing.
- **Close and project replacement** follow checkpoint 1's law: they never block on the worker, and the worker's
  own cancel path removes its temporaries.

### Checkpoint 3 — formats and options

- **Format.** WAV 32-bit float (the canonical export WAV, unchanged and still bit-exact against its reference),
  24-bit and 16-bit integer.
- **Two passes per file.** The render phase writes the file's audio as 32-bit float to its `.partial` (one file's
  render buffer in memory at a time) and records its peak; the write phase streams it to the final format with the
  job's normalize gain and dither into a second temporary, which is what is committed. A float export with
  normalize off commits the render-phase file directly.
- **Dither.** Integer output gets **TPDF dither** of ±1 LSB (the sum of two independent uniform values in
  [−0.5, 0.5) LSB), then round to nearest and clamp; on by default for 16 and 24-bit, switchable off. The noise comes
  from a fixed-seed xorshift generator with **one stream per file and channel**, seeded from a hash of the output's
  index in the job (the mix 0, stems 1…n in strip order) and the channel index — so no two files carry the same
  noise, and the same project with the same options exports byte for byte the same. Float output is never dithered.
- **Range.** Whole project — today's render length (frame 0 to the end of the last clip or MIDI clip; a
  reverb or delay **tail** beyond it is parked for a later ADR) — the loop region, or the ruler range (today's
  rule: the ruler range wins when both exist). A range with nothing in it is refused with its reason.
- **Normalize.** Off, or to a peak target (default −1.0 dBFS). One gain per job, from the loudest file the job
  writes (the mix, or the loudest stem when the mix is not written), applied to every file before dither, so stems
  keep their balance and still sum to the normalized mix. Silence is never boosted (gain 1 when the peak is zero).
- **Export stems.** One file per **top-level strip** — each Track and each Bus whose main output is the master. A
  Track or Bus routed into a Bus is inside that Bus's stem; a top-level Track's sends are inside the stems of the
  Buses they feed. So the stems never count a signal twice and they sum to the master's input. A stem is what that
  strip contributes at the master's input: the master sum fed by that strip's output alone, with the master's
  inserts and fader skipped; every stem file has the master's channel count (stereo). Everything upstream is built
  exactly as in the mix — clips, inserts, fader, pan, mute and solo state, sends, sidechain keys and delay
  compensation — so a stem sounds as that strip does in the mix (a muted strip's stem is silent). Stems render from
  the same snapshot, one after another, in the same job (one progress, one cancel, one commit phase). The mix file
  is optional in a stems export (default on). File names are `<base> - <strip name>.wav` through one named helper
  with one rule set on every platform: the characters `< > : " / \ | ? *` and control characters become `_`;
  trailing dots and spaces are trimmed; a Windows reserved name (CON, PRN, AUX, NUL, COM1–COM9, LPT1–LPT9, any
  case, with or without an extension) gains a leading `_`; the strip-name part is capped at 100 characters; an empty
  name becomes `Strip`; a duplicate gets ` (2)`, ` (3)`.

### Gates

- **cp1 (`[export-job]`):** a render on the worker equals the synchronous render sample for sample; with the job
  held at its latch mid-render, UI ticks run and an edit dispatches and completes (its own result ok) while the job
  stays Rendering, and after release the output still equals the synchronous render of the snapshot; an import, an
  undo and a recording commit during the held job leave its output unchanged; progress rises monotonically to 100;
  Cancel mid-render ends Cancelled with no file and no export-count bump; opening another project during the held
  job reports nothing for the stale job and leaves the new project's status line alone; a second export is refused
  with its reason; shutdown with a held job joins (the TSan leg covers the job's atomics and handoff).
- **cp2 (`[export-commit]`):** cancel during render and during write each leave no `.partial` file and a
  pre-existing destination byte-identical; an unwritable destination fails with its cause and leaves nothing;
  success replaces an existing destination; a stale `.partial` of the destination's name is removed; project
  replacement and app close during an export leave no temporary.
- **cp3 (`[export-options]`):** the float export stays bit-exact against its existing reference; 16 / 24-bit outputs
  equal a reference built by the same TPDF law in the test (dither off equals plain rounding); two stems never
  carry the same dither noise; each range exports exactly its frames; normalize brings the loudest file's peak to
  the target within 0.01 dB, applies the same gain to every file, and leaves silence silent; in float with no master
  inserts and a unity master fader, the stems sum to the mix within 1e−6 (Tracks into a Bus and Bus-to-Bus routing
  included in the fixture); a muted Track's stem is silent; the file-name helper's rules hold for reserved names,
  illegal characters, trailing dots, long and duplicate names.

## Consequences

- **Positive:** export never freezes the app and can be cancelled; a failed or cancelled export never destroys an
  earlier file; integer exports are properly dithered; stems and normalize land; the snapshot law is the template
  for later background jobs.
- **Negative / accepted costs:** until G5.4, a job copies each same-rate Asset's decoded audio once (memory equal to
  the decoded audio, for the job's life), plus one file's render buffer at a time; stems render serially; a Track
  inside a Bus has no stem of its own (a later ADR may add per-strip stems); a reverb tail past the last clip is
  still cut off (parked); app close waits for one render block or write chunk and the job's file cleanup.
- **Follow-ups:** `CONTEXT.md` gains **Export job** (the worker, its snapshot and its commit law) and **Export
  stem** (a top-level strip's contribution at the master's input), distinct from **Stem** in the separation
  sense. SS-6 step 5 in `ss8-project-lifecycle.ps1` drives range, stems, cancel and replacement on the real app.
  Parked: a render tail; per-strip stems below the top level; parallel stem rendering.
