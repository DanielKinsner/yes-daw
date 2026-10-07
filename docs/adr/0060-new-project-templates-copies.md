# 0060. New project, templates and copies: a New Project dialog, the device at the project's rate, layout templates with fresh identities, and Save As / Save a Copy through one atomic bundle copy

- **Status:** Accepted (agent, 2026-10-07, under ADR-0049's implementation-ADR clause, after a separate agent critic
  pass whose findings are folded in: Save As and Save a Copy write the current state into the copy, never the source;
  the source's database is copied consistently while open and the copy validated before its rename; the device-rate
  seam, the untitled session, Sampler tracks in templates, the remap's inclusions and exclusions, the test seams and
  the remembered-choices record are pinned; three checkpoints, three commits. Committed alone, before any G5.5 code.)
- **Date:** 2026-10-07
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G5.5 ("New project, templates and copies. Sample rate, tempo and template in the new-project
  dialog; Save As and Save a Copy. Gate: creation/copy/reopen retain assets and project state, a copy leaves the source
  usable, and failure preserves the last valid project. Respect G5.3's active-job rule.") and SS-6 steps 1 and 7 in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md); ADR-0011
  (entity IDs are unique across projects — what makes templates unambiguous); ADR-0012 (bundle atomicity: copy,
  validate, swap); ADR-0058 (export jobs; a replaced project cancels its job; the `.partial` commit law; the file-name
  rule); ADR-0059 (decoded buffers); ADR-0050 (per-user presets in the session-state folder — the model for templates);
  ADR-0031 (the engine's in-place hot-swap path does not change rates — untouched here); ADR-0055 (changing a
  project's rate stays unsupported); ADR-0049 (the Control target; an overlay keeps Tab inside it).

## Context

New today is only a native save chooser: every project starts at 48 kHz, 120 BPM, 4/4 with one audio track, and New
silently overwrites a bundle already at the chosen path (`openOrCreateBundle` opens it and the first snapshot write
replaces its rows), where Save As refuses one. Neither New nor Open asks about unsaved changes (only quitting does).
The audio device opens at whatever rate it picks and the project's rate is never asked of it; the engine reads frames
one for one, so a 44.1 kHz project on a 48 kHz device plays 9 % fast with only a status warning — offering a rate
choice without fixing this would make most choices wrong. Save As writes the current state into the source and then
copies the whole bundle straight into its final folder (a failure leaves a partial bundle; the autosave folder is
copied too, so the copy can open with a stale recovery prompt); Save a Copy does not exist. There are no templates.

What is hard to reverse: the template format and how a template's identities become a project's (ADR-0011), and the
bundle-copy commit law every copy path shares.

## Options considered

1. **One dialog, one copy routine, layout templates with fresh identities (chosen)** — below.
2. **Templates as whole bundles copied verbatim (clips and audio included, IDs kept).** Simple, but two projects made
   from one template would share every track, bus and clip ID — the cross-project ambiguity ADR-0011 rules out — and
   audio-carrying templates are large. Rejected.
3. **Leave the device rate alone; offer only the device's current rate in the dialog.** Avoids a device reopen, but a
   project opened on another device still plays at the wrong speed, and 44.1 kHz work on a 48 kHz default device would
   be impossible. Rejected.

## Decision

### Checkpoint 1 — the New Project dialog, the device rate, unsaved changes

- **The dialog.** `File > New` (Ctrl+N) opens a **New Project** overlay with **Sample rate** (44.1, 48, 88.2, 96 kHz),
  **Tempo** (20–300 BPM), **Meter** (the project's meter: numerator 1–32 over 2, 4, 8 or 16 — the model's
  `setProjectMeter` range), **Template** (Default — one audio track — first; the user's templates from checkpoint 3)
  and **Create** / **Cancel**. Create then asks for the bundle's location (the native chooser, as today). Every
  control is reachable through the Control target with Tab kept inside the overlay; Esc cancels; nothing is written
  until Create. The last choices are remembered per user in `new-project.txt` in the session-state folder (defaults
  48 kHz, 120 BPM, 4/4, Default).
- **New refuses an occupied target**, as Save As does ("a project already exists there"); the occupied bundle is not
  opened or written.
- **Unsaved changes are asked about before New and Open replace the project** — Save / Don't Save / Cancel, through the
  same hook quitting uses. Save on a named project saves it; Save on the untitled launch session goes through Save As
  (its chooser; cancelling that cancels the New / Open). A clean project, including a clean untitled session, is
  replaced without asking. The untitled session's folder is left as it is (cleaning abandoned untitled sessions is a
  later item).
- **The device runs at the project's rate.** When a project is created or opened and the open device's rate differs
  from the project's, the shell asks the device to run at the project's rate by reopening it with that rate (the
  native path sets `AudioDeviceSetup::sampleRate` and reopens, callback removed first; never the engine's in-place
  hot-swap of ADR-0031). The shell reaches the device through a new seam,
  `MainComponentFileChoices::requestAudioDeviceSampleRate (double hz) -> bool`, which tests inject; the probe reports
  `audio.deviceRateHz` and `audio.rateRequests`. A device that cannot run at the rate keeps its own and the existing
  mismatch warning names both rates. The project's rate never changes (ADR-0055).
- **A failure preserves the last valid project.** Creation writes the new bundle completely before anything is
  attached; any failure removes what it wrote and leaves the current project, its undo history and its view as they
  were. A running export is cancelled only when a new project is actually attached (ADR-0058).
- **Test seams** (headless gates): `newProjectDialogChoices` (rate, tempo, meter, template) answers the dialog when
  set; the existing `chooseNewProjectBundle` gives the location; `confirmCloseUnsavedChanges` answers the unsaved
  prompt for New / Open too.

### Checkpoint 2 — Save As and Save a Copy through one bundle copy

- **One copy routine, replacing today's Save As copy.** It writes a complete bundle into a temporary sibling folder
  `<target>.<n>.partial`: the database copied consistently from the open source (`VACUUM INTO`, which reads a
  committed state without closing the source), the Asset files, the peak caches and the per-project view state — not
  the autosave folder or the trash (listed out, never copied then deleted). It opens the copy and writes the
  **current in-memory project** into it (unsaved edits go to the copy, never to the source), validates it (the bundle
  validator, plus a new `validateBundleAssetFiles` that finds every Asset row's file under `audio/` with its size),
  closes it and renames the folder into place. An occupied target is refused before anything is written; any failure
  removes the temporary folder and leaves the source and the current project exactly as they were.
- **Save As** then opens the copy as the current project (undo history continues; the copy is clean). The source
  stays as it was last saved.
- **Save a Copy** (`File > Save a Copy…`, chooser seam `chooseSaveACopyProjectBundle`) leaves the source current and
  untouched — its database, its dirty state, its undo history and any running export carry on; the copy is written and
  closed.
- **An export keeps running** through either (its job owns its snapshot and buffers, ADR-0058/0059).

### Checkpoint 3 — templates

- **A template is a layout.** `File > Save as Template…` (a name through the seam `chooseSaveAsTemplateName`; the
  name made file-safe by ADR-0058's rule) writes a bundle `<session state>/templates/<name>.yesdaw` (an empty bundle's
  usual sub-folders included) holding:
  - **included:** the tracks (name, colour, height, instrument kind and parameters, strip — gain, pan, mute, solo-safe
    and inserts with parameters — sends, output), the buses (the same), the master strip and its inserts, the master
    gain, the sample rate, the tempo map, the meter and the markers. A Sampler track keeps its kind with **no pads**
    (pads reference Assets).
  - **excluded:** clips, MIDI clips, takes and comp segments, Assets, Sampler pads, automation lanes (and the strips'
    automation modes return to their defaults), locate points, loop and punch regions and the scale.
  A template of the same name is replaced only after asking.
- **Fresh identities.** Creating from a template gives the project a new project ID and a new ID to every entity it
  carries — each track, bus, FX insert (on tracks, buses and the master), send and marker — and rewrites every
  reference between them through one map: a track's or bus's output bus, a send's bus, a Compressor's sidechain
  source. No ID of the template appears in the project (ADR-0011).
- **The dialog's Template list** shows Default first, then the user's templates by name; a template the validator
  refuses is listed with its reason and cannot be chosen; choosing a template sets the dialog's rate, tempo and meter
  to the template's (the user may change them).

### Gates (`[project-lifecycle]`)

- **cp1:** the dialog creates a project with the chosen rate, tempo and meter (the reopened bundle holds them; the
  engine runs at that rate); the choices are remembered; Cancel and Esc change nothing; the keyboard alone creates a
  project; New over an existing bundle is refused and the existing bundle is byte-identical; New and Open with unsaved
  changes offer Save / Don't Save / Cancel and each does what it says (on a named project and on the untitled
  session); when the injected device accepts the rate it is asked once and the probe reads the rate, when it refuses
  the warning names both rates; a failed creation leaves the previous project current with its undo history.
- **cp2:** Save As and Save a Copy each produce a bundle that reopens with the same Assets (byte-identical files),
  clips and mixer, renders the same audio as the in-memory project and opens with no recovery prompt; Save As leaves
  the source as last saved and the copy current and clean; after Save a Copy the source is current, its database
  unchanged, still dirty, its undo works and a later Save writes the source; a failure mid-copy (an injected one)
  leaves no target and no `.partial` folder and the source untouched; an occupied target is refused; an export running
  through either still completes.
- **cp3:** a template holds exactly the included layout and none of the excluded content; a project created from it
  has the template's layout, a new project ID and no entity ID found in the template, and its routing intact (a test
  tone through its sends, buses and master renders as through a hand-built twin); a Sampler track arrives with no
  pads; a refused template is listed with its reason; the dialog's list reads the templates folder.

## Consequences

- **Positive:** projects start at the rate, tempo, meter and layout the user wants and play at the right speed; New
  and Open can no longer lose unsaved work or overwrite a project; Save As cannot leave a half bundle or touch the
  source; Save a Copy exists; templates never share identities between projects.
- **Negative / accepted costs:** creating or opening a project at a rate the device supports reopens the device (a
  short audio drop); templates carry no content (a song starter with audio is a later ADR); a copy copies every Asset
  file (no hard links); abandoned untitled sessions are not cleaned yet.
- **Follow-ups:** `CONTEXT.md` gains **Template** (a project's layout without content, instantiated with fresh
  identities) and **Save a Copy**. SS-6 steps 1 and 7 drive the dialog, Save As and Save a Copy on the real app.
