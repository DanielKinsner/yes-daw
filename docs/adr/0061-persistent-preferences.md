# 0061. Persistent preferences: one prefs.json owns the keymap, the view and dock defaults, editing and export defaults and the chosen audio devices; a project's own view state still wins

- **Status:** Accepted (agent, 2026-10-07, under ADR-0049's implementation-ADR clause, after a separate agent critic
  pass whose findings are folded in: the `[keymap-editor]` gate is re-pinned to `prefs.json`; one write site for a
  chosen device, never the launch restore; the launch order (defaults, then the remembered device, then the
  project-rate request); honest messages when no device opens; the places preferences are applied; every other
  context field named in or out; one read per folder. Committed alone, before any G5.6 code.)
- **Date:** 2026-10-07
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G5.6 ("Persistent preferences. `prefs.json` owns keymap, view defaults, device and dock
  defaults (R32); reuse existing project view state rather than overwriting it with defaults. Gate: relaunch
  preserves settings, malformed preferences fail safely, and missing devices receive an honest reason.") and SS-6
  step 7 ("relaunch and assert preferences and per-project view state retain their separate values") in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md), whose §5.1
  names `%APPDATA%\YES DAW\prefs.json`; R32 in
  [`docs/goals/2026-08-25-reality-run-backlog.md`](../goals/2026-08-25-reality-run-backlog.md) (dock collapse,
  inspector tab, snap unit, metronome, export bit depth and keymap reset on every launch or open); ADR-0046 (the
  keymap and its editor; G1.5 added the overrides file); ADR-0050 (FX presets, per-user files in the session-state
  folder; it names G5.6 as that folder's co-owner); G2.1's per-project view-state sidecar (shell code, D48; ADR-0052
  added automation-lane heights to it and its unknown-keys-ignored rule); ADR-0053 (monitor dim and mute are
  session state, never persisted); ADR-0056 and ADR-0060 (their per-user records, which stay as they are); ADR-0035
  (device UX).

## Context

The per-user session-state folder (`%APPDATA%\YES DAW`, or the harness's injected folder) holds line records
written by separate features: `keymap-overrides.txt`, `last-project.txt`, `recent-projects.txt`, `new-project.txt`,
`recent-audio.txt`, `browser-state.txt`, plus the presets and templates folders. Nothing else survives a launch:
every bundle attach resets the action context, so the dock's visibility and tab, the inspector's visibility and
tab, the snap mode and grid and the metronome return to factory values on every open; the export bit depth, dither
and normalize choices reset each launch; the audio device opens on the system default every time (nothing records
a chosen device, so a missing one cannot even be reported). The per-project `view-state.txt` (rail, inspector and
dock sizes, narrow strips, automation-lane heights) is read on attach and falls back to theme tokens when absent.
Four records (`keymap-overrides`, `last-project`, `recent-projects`, the project's `view-state`) are written by
truncating in place, so a crash mid-write leaves a torn file.

What is hard to reverse: the preferences file's shape (users keep it across versions), and which settings belong to
the user and which to the project.

## Options considered

1. **One `prefs.json` for what belongs to the user; the project keeps its own view state (chosen).** Below.
2. **More line records, one per feature.** Matches today's pattern, but every feature invents its own fallback and
   write rules, there is no single "malformed preferences" behaviour to gate, and the plan names `prefs.json`.
   Rejected.
3. **Everything in the project (snap, metronome, dock tab, devices).** Logic keeps some of these per project, but
   a device is a property of the machine, a keymap of the person, and R32's complaint is precisely that the user's
   choices vanish when a project opens. Rejected for the settings below; the project keeps what it already owns.

## Decision

### The file

- **`<session state>/prefs.json`**, one UTF-8 JSON object with `"version": 1`. The model owns one in-memory
  preferences value, read once when the session-state folder is set (launch), and written **whole** after each
  change through a temporary sibling and a rename (the same write as ADR-0056's records). Changes are rare
  (a drag ends, a toggle, a rebind, a device choice); no debounce.
- **One read per folder.** The model reads `prefs.json` when its session-state folder is set to a new folder; setting
  the same folder again (the native shell can set it twice at launch) reads nothing. With **no session-state folder**
  (the harness's default) preferences live in memory only: defaults, nothing read or written — so harness tests that
  inject no folder see factory values exactly as today.
- **Reading never fails the launch.** Text that is not a JSON object — empty, truncated, not JSON, a non-object —
  gives every default; the file is kept as `prefs.json.unreadable` (replacing an older one) and the status line
  says so once: "Preferences could not be read - defaults are in use (the old file is kept as
  prefs.json.unreadable)". A **known key** with a wrong type or an out-of-range value gives that key's default and
  leaves the others; the probe counts rejected keys (`prefs.rejectedKeys`). **Unknown keys** — a newer version's —
  are kept and written back untouched, and a file whose `version` is higher keeps its number.
- A missing file is the first launch: every default, nothing reported.

### What it holds (version 1)

- **`keymap`** — an object from an action's stable id to a chord (`""` = unbound), the non-default bindings only, as
  `keymap-overrides.txt` held them. On the first read with no `keymap` key, an existing `keymap-overrides.txt` is
  imported, written into `prefs.json` and renamed `keymap-overrides.txt.migrated`; that file is no longer read or
  written. The import is silent (the probe reports `prefs.migratedKeymap`). The `[keymap-editor]` gates that assert
  `keymap-overrides.txt` are re-pinned to the `keymap` object of `prefs.json` (a rebind adds its entry; Restore
  Defaults leaves the object empty).
- **`view`** — `railWidth`, `inspectorWidth`, `dockHeight` (pixels, the view-state record's clamps), `narrowStrips`,
  `inspectorVisible`, `inspectorTab` (`"track"` when the context's `inspectorTrackTabActive` is true, else
  `"clip"`), `dockVisible`, `dockTab` (`"mixer"`, `"pianoRoll"`, `"instrument"`, `"browser"`). They hold the **last**
  arrangement the user made: the sizes and narrow strips are written where the project's record is written (a
  splitter drag ends, the narrow toggle); visibility and tabs whenever the context changes them. A change made with
  no project open is remembered the same way.
- **`editing`** — `snapEnabled`, `snapMode`, `snapGridTicks` (one of the grid values the snap chooser offers; another
  value falls back), `metronome`.
- **`export`** — `bitDepth` (`"float32"`, `"int24"`, `"int16"`), `dither`, `normalize`. Range and stems stay per
  export.
- **`audio`** — `outputDevice`, `inputDevice` (device names; `""` = the system default). One write site: a
  successful `selectAudioOutputDeviceByName` / `selectAudioInputDeviceByName` — the paths both the chooser and the
  harness's device seams take. The launch restore and any fallback never write them.
- **Not preferences** (unchanged, a later item may add some): the nudge value, the quantize settings, count-in,
  playhead follow, return-to-start, window position and zoom; the monitor's dim and mute stay session state
  (ADR-0053).

### Precedence: the project's view state wins

- When a project attaches, its own `view-state.txt` decides what it records (rail, inspector and dock sizes,
  narrow strips, automation-lane heights); the `view` preferences supply only what the record lacks — a new
  project, or a project that never had a record. In code: the shell's view-state load starts from the preferred
  sizes (not the theme tokens) and then applies the record. The dock's and inspector's visibility and tab, the snap
  and the metronome are applied to the context **immediately after the attach resets it** (`context_ = {}` in the
  model's attach), so every attach starts from the preferences instead of factory values; the export choices are
  model state applied when preferences are read. Nothing writes preferences into a project's record or a
  project's record into another project.
- Hence SS-6 step 7: project A arranged at one size and project B at another keep their own sizes across a
  relaunch, while a new project starts from whichever arrangement was made last.

### Devices: the remembered one, or an honest reason

- **Launch order.** (1) The device opens exactly as today (`initialiseDesktopAudio`: the verified default pair, else
  the defaults). (2) If a remembered output names a device other than the one open and it is listed, the shell
  switches to it through the device-manager call the chooser's path makes, without writing preferences; the same
  for the input. A remembered name equal to the open device does nothing. (3) The project-rate request of ADR-0060
  runs last, unchanged.
- **Honest messages.** A remembered device that is not listed, or that refuses to open, leaves the open device
  as it is and the status line names both: "Audio output <remembered> is not available - using <open>" (and
  "Audio input <remembered> is not available - using <open input>", or "- no input" when none is open). Both
  missing is one line joining the two. When nothing could open at all, today's "No audio device could be opened:
  <reason>" gains "(<remembered> is not available)". The remembered names stay, so the device is used again once it
  is back.
- The harness drives this through the existing `listAudioOutputDevices` / `selectAudioOutputDevice` (and input)
  seams.

### Records written safely

- `last-project.txt`, `recent-projects.txt` and the project's `view-state.txt` move to the same temporary-and-rename
  write; their formats do not change. `new-project.txt`, `recent-audio.txt` and `browser-state.txt` already write
  this way and stay separate records (history and per-feature state, not preferences).

### Checkpoints and gates (`[prefs]`)

- **cp1 — the file and the keymap.** Gate: a rebind survives a relaunch through `prefs.json` (the re-pinned
  `[keymap-editor]` gates); `keymap-overrides.txt` is imported once and renamed, silently; empty, truncated, non-JSON and non-object files each give defaults, are kept as
  `.unreadable` and are reported; a wrong-typed or out-of-range key falls back alone; unknown keys and a higher
  version survive a rewrite; a write leaves no temporary file; the three records write atomically.
- **cp2 — view, dock, editing and export.** Gate: each setting survives a relaunch and a project open; a project with
  its own view-state record keeps its sizes while a new project starts from the last arrangement (A and B keep
  separate values across a relaunch); a project without a record gets the preferred sizes.
- **cp3 — devices.** Gate (through the harness's device seams and a launch-time device-open seam): a chosen output
  and input are reopened at the next launch, after the default and before the rate request; a missing remembered
  output, input, or both leaves the open device, names each in the status line, and stays remembered; with no
  device at all the message says so and names the remembered one; a fallback never overwrites the choice.

## Consequences

- **Positive:** the user's keymap, layout habits, editing and export choices and devices survive launches and
  opens; one gated behaviour for a broken preferences file; no torn per-user records.
- **Negative / accepted costs:** a preference changed in one running instance is overwritten by another instance's
  later write (last writer wins; one instance is the norm); the old keymap file is retired after its import;
  window position and zoom stay unremembered (a later item); tests that inject a session-state folder now see their
  own preference writes on a later New or Open (re-pinned where one assumed a factory reset).
- **Follow-ups:** `CONTEXT.md` gains **Preferences** (the user's settings in `prefs.json`, as distinct from a
  project's view state). SS-6 step 7 drives the relaunch on the real app.
