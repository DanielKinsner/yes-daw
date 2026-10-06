# 0056. The media browser: a Browser dock tab with Files, Project and Recent, an audition voice on the monitor path, and import through the same verbs

- **Status:** Accepted (agent, 2026-10-06, under ADR-0049's implementation-ADR clause, after a separate agent
  critic pass whose findings are folded in: no new single-key chords beyond `Y` — the browser's keyboard law is
  ADR-0049's Control target (Space stays transport, Enter is Return to zero outside navigation); the audition's
  place in the device callback, its ownership, retirement, no-device refusal and rate are pinned; placing a project
  Asset has its own verb; Asset naming, the Recent record, header-read cost and the missing gates are decided; the
  work is split into two checkpoints. Committed alone, before any G5.2 code.)
- **Date:** 2026-10-06
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G5.2 ("Media browser (`Y`). File browser with audition, project assets and recent files.
  Reuse G5.1's format and rate policy. Gate: browser selection/audition/import reaches the same asset and drop
  location as the ordinary import path; unavailable files give a visible reason.") and §4.3's key (`Y`, Logic's
  Library) in [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md);
  **ADR-0049** (keyboard access through the Control target; Space is transport; Enter activates the Control target
  while navigating, else Return to zero); ADR-0054 (one decoder, the supported list, refusal reasons); ADR-0055
  (cross-rate views); ADR-0053 (the device callback's stages: the header peak scan, then the monitor Dim / Mute last;
  the loudness tap reads the mix inside the engine); ADR-0012 (orphaned asset files are trashed at reopen); ADR-0046
  (dock tabs, no widget shortcuts); ADR-0002 (the audio thread's rules); G3.1 (the Instrument dock tab — the pattern).

## Context

Importing today means a native file chooser or a drop from the OS. A person cannot browse their samples inside the
app, hear a file before importing it, see what the project already holds, or get back to a file they used an hour
ago. The bottom dock carries tabs (Mixer, Piano Roll, Instrument) shown by `X` / `P`; the plan gives the browser `Y`.
Keyboard access to panels goes through ADR-0049's Control target (Tab walks the controls; Enter activates; a chooser
steps its items with the arrows and keeps or restores), never through widget focus or new single-key chords —
`Space`, `Enter`, `Up`, `Down` and `A` are all global already. ADR-0054's decoder and reasons and ADR-0055's views
are the import law any new surface must reuse. The device callback already sums a non-mix signal into the outputs
(input monitoring) after the engine.

## Options considered

1. **A Browser dock tab (chosen).** Wide enough for a file list with its facts (name, format, rate, channels, length),
   below the lanes a file is dragged onto, and the tab mechanism exists (G3.1). `Y` shows it.
2. **A right-hand panel sharing the inspector's column (Logic's Browsers).** Narrow (300 px), it competes with the
   inspector the edit work needs, and a second right-side mechanism would be new. Rejected for now.
3. **A floating window.** Easy to lose, no drag alignment with the lanes, a new window-management surface. Rejected.

## Decision

**Placement and keys.** A **Browser** tab in the editor dock (beside Mixer, Piano Roll, Instrument), shown by
`View > Browser` and **`Y`** (a global toggle like `X` and `P`; one new action, `ViewBrowser`, at the end of the
table). No other new chord. The keyboard path is ADR-0049's **Control target**: Tab reaches the browser's source
chooser, its **list**, and its **Audition**, **Import** and **Up** buttons; the list is a chooser — Enter starts
browsing, Up / Down step the rows (the selection follows), Enter keeps: a file **imports**, a folder (or the `..`
row) **opens**; Esc restores the selection and ends. Space stays transport throughout. The mouse: a click selects, a
double-click imports a file or opens a folder, the row's play mark auditions, a drag onto the lanes imports.

**Three sources** (the chooser at the tab's top):
- **Files** — one folder at a time: a `..` row, its sub-folders, then the files of a supported format (ADR-0054's
  list); other files are not shown. It starts at the folder last browsed (per user, in session state; the user's
  Music folder at first) with the path shown. A row's facts (format, rate, channels, length) are read from its header
  when the row is first painted — never for rows not on screen — and cached by path, size and modification time; a
  file the decoder cannot open shows its **reason** in its row ("not a readable MP3 file").
- **Project** — the project's Assets, each named by the **first clip that uses it** (its clip name), or "Asset " and
  the first eight hex digits of its content hash when no clip uses it, with its rate, channels and length. An unused
  Asset exists only within a session (ADR-0012 trashes orphans at the next open), so a just-opened project shows none.
- **Recent** — the last 20 audio files imported by this user from any surface, newest first, in `recent-audio.txt`
  in the session-state folder (one UTF-8 path per line, written whole to a temporary file and renamed into place,
  lines longer than 4096 bytes ignored); a file that no longer exists shows "missing".

**Import is the existing verbs.** Keeping a file imports it onto the **selected track at the playhead** — the verb
behind `Ctrl+Shift+I` (ADR-0054's decoder, ADR-0055's rate law); dragging one or more rows onto the lanes lands them
through the **verb behind an OS drop** (consecutive tracks from the lane under the pointer at its snapped time, one
undo step). A **project Asset** placed either way goes through one new verb, `placeAssetsAt` — the same lane, time and
one-undo-step law, a new clip on the existing Asset, no copy and no decode. Every refusal is named on the status line
with its file and reason, exactly as on the other surfaces. A successful import from any surface adds its file to
Recent.

**Audition (the second checkpoint).** A file is heard through an **audition voice**: the control thread decodes it
(ADR-0054; through ADR-0055's live view when its rate differs from the project's, so it plays at the project rate like
everything the device callback plays) into a voice — its samples, channel count and length — and publishes it through
one atomic pointer. Inside `UiAppModel::processDeviceAudioBlock`, **after the engine's block and the input-monitoring
sum and before the method returns**, the device thread adds the voice's next frames to the outputs at unity (mono to
every output, stereo channel to channel) and advances its own index; at the end it marks the voice finished. So the
shell's header peak scan sees it and the monitor Dim / Mute (the last stage) applies to it, while **it is never in the
mix**: not in the engine's loudness tap, an export, a bounce or a recording. A new audition, a second press, transport
Play or Record, closing the browser, or the file's end stops it: the control thread swaps the pointer (to the new voice
or null) and moves the old voice to a retired list reclaimed by the existing `deviceBlocksStarted_` watermark (the
monitor chain's law), never freed while the device thread may hold it. An engine rebuild does not touch it. With no
audio device open, an audition is refused with "no audio device" on the status line and no voice is published. The
device thread only reads samples and advances an index — no allocation, lock, log or I/O.

**Not saved.** The browser's source and folder are per-user session state; the tab follows the dock's existing view
state; nothing of it is in the project.

**Two checkpoints.** **cp1:** the tab, `Y`, the three sources, the header-fact cache, Recent, the Control-target
keyboard path, import by keep / double-click / drag, `placeAssetsAt`, refusals. **cp2:** the audition voice.

## Consequences

- **Positive:** files are found, heard and placed without leaving the arrangement; the project's own media is one tab
  away and reusable without re-import; every import path keeps one law; no new single-key shortcut collides with the
  keymap.
- **Negative / accepted costs:** keyboard browsing is a Tab-then-Enter path (ADR-0049's law) rather than bare arrow
  keys; an audition of a long file decodes it whole first (as an import would); MP3 lengths shown are JUCE's
  (ADR-0054); searching, tags and a sample database stay out (a later ADR).
- **Gates (`[browser]`):** a file kept in the browser lands on the same track and tick as the same file through
  `Ctrl+Shift+I`, with an equal Asset row; a browser drag of two rows lands exactly as an OS drop of the same files at
  the same point (one undo step); a project Asset placed from the browser adds a clip on that Asset and no new Asset, in
  one undo step; an unreadable and a missing file each show their reason in the row and on the status line when kept,
  and change nothing; Files lists `..`, sub-folders, then supported files only, and a non-ASCII folder and file name
  work; Recent holds an imported file newest first and survives a new model reading the same session folder; the last
  folder and source survive likewise; keyboard only (Tab, Enter, arrows, Enter, Esc) browses, opens a folder and
  imports; the list's rows expose their names to accessibility; a 1000-file folder lists without reading a header for
  rows not shown (header reads counted). **cp2 (`[audition]`):** an audition puts the file's samples on the device
  outputs from frame 0 while the engine's mix, the loudness tap and an export of the same moment carry none of it;
  Play, a second press and another audition stop it; Mute silences it; with no device it is refused with its reason;
  the device thread's audition read is allocation-free (RTSan); SS-6 step 1's "open the browser and audition a
  supported file" is driven on the real app.
- **Follow-ups:** `CONTEXT.md` gains **Media browser** (the dock tab and its three sources) and **Audition** (a file
  heard on the monitor path, never in the mix).
