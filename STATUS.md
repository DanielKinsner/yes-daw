# YES DAW — STATUS (live handoff)

**Read this first on any machine.** This is the single source of truth for *where we are right now*.
The [active plan](docs/plans/2026-09-01-real-daw-ground-up-plan.md) and
[roadmap](docs/goals/roadmap.md) define the work; **this** file's newest entry is the live handoff.
Older entries below are dated history, not competing "Now" instructions.

> **Cross-machine rule:** `git pull` at the start of a session. At the end, update this file, commit in
> small chunks, and `git push`. Then the next machine — or the next session — is never lost.

## 2026-10-08 — Fix (found by SS-6): a double press on a browser row's play mark imported the file

**What a user saw:** pressing a file's ▶ mark in the browser twice quickly (start, then stop the audition) also
imported the file as a new clip - the second press's double-click ran the row's "import" gesture. ADR-0056 keeps them
apart: a double-click on a row imports, the play mark auditions. **Fix:** a double-click whose press landed on the mark
is the audition's stop and nothing else. **Found by:** SS-6 run 7 on the real app (two clips, one asset after the
audition step). **Gate:** `[audition][browser]` drives the real mouse sequence (down, up, down, up, double-click) on
the list: two audition toggles and no import on the mark; one import on the row's name. Red before the fix (an
import), green after. The older harness reached "double-click" by calling the import directly, which is why no gate
saw it. ctest 420/420, Clang clean. **Critic:** nothing wrong (JUCE delivers the second press before the double-click,
so the guard reads the right press).

## 2026-10-08 — Fix: a launch that cannot open the last project leaves the untitled session, not nothing

**What a user saw:** when the project YES DAW reopens at launch could not open (a missing or damaged bundle, one the
engine refuses, or Cancel on its missing-audio question), the app came up with no project at all - only File > New or
Open did anything. **Now (ADR-0062: "At launch, Cancel leaves the untitled session"):** the untitled session opens and
the reason stays on the status line ("Open failed: ... (<name>)" / "Open cancelled: ..."). The last-project record
still names the project that failed, so the next launch tries it again (a drive plugged back in, a file restored) -
the fallback session never takes its place until a Save, New or Open names a project. If the fallback itself cannot be
created, both reasons show. The deferred (native) question's fallback also sets the device to the session's rate.
**Gates:** `[empty-startup]` - a missing explicit bundle at launch opens an untitled session, says why by name, keeps the
earlier untitled bundle, and the next plain launch reopens the last real project (red with the record overwritten);
`[project-lifecycle]` an engine refusal at launch and `[relink]` a launch Cancel both leave the untitled session.
ctest 420/420, Clang clean. **Critic:** no blockers; its two should-fixes (the deferred path's device rate, a failed
fallback hiding its own failure) and test-bite gaps are fixed. Untested: the native deferred path (a modal question;
no headless seam). **Next:** SS-6 drive fixes (audition, export replacement) and a re-run; then ADR-0068 cp5.

## 2026-10-08 — Fix (found by SS-6): a project whose clips were all deleted reopened as nothing, silently

**What a user saw:** delete every clip of a project (its tracks and audio files stay), quit or crash, open it again —
nothing opened and nothing said why: no project, an empty status line. **Why:** the audio engine refuses to build a
graph for an empty timeline, and the open path had no transport-only fallback (the edit path already had one,
ADR-0041); both open paths (the launch reopen and File > Open) ignored the failed load. **Fix:** an empty timeline opens
with a transport, as after an edit; any load that fails says "Open failed: <reason> (<name>)" (R5). **See-it:** SS-6
run 5 — after the kill the project reopens with its 5 tracks and 6 audio files. **Gates:** `[project-lifecycle][reopen]`
— a bundle with audio and no clips reopens through the model and through the launch path; an engine refusal (a
tempo-locked clip) names its reason at launch and on File > Open and leaves the current project. Each red with its fix
reverted. ctest 420/420, Clang clean. **Critic:** no blockers. Next, its note: after a failed launch-time open the user
is left with no project at all (and the next launch tries the same bundle again) — the untitled session should open
with the reason shown.

## 2026-10-08 — SS-6 is built: the project-lifecycle drive runs on the real app (151 of 165 on its fourth run)

**Now:** `tools/session-scripts/ss8-project-lifecycle.ps1` — the plan's SS-6, G5's exit — exists and runs end to end on
the real exe. **Next:** a real bug it found (below) is being root-caused; then ADR-0068 (the autosave and recovery
contract) in a safe order — the stamp and the recovery rule first, the writes switched on last — so no commit ever
writes autosaves that the old "ask whenever one exists" rule could offer as a rollback; then ADR-0067 cp2.
**Decision still pending (Dan, plan §8.2):** the macOS frame exception (unchanged).

**What the drive does, on the real app:** creates a 44.1 kHz / 100 BPM project from the New Project dialog; drops a
file with a real Windows drag and auditions it from the browser's Recent list; drops AIFF, FLAC, Ogg, a 44.1 kHz WAV and
an MP3 on chosen lanes and times and checks every Asset's hash names its bundle file; refuses a malformed WAV with its
reason and declines an .m4a while it is dragged; reopens a copy with a missing asset — Cancel, a wrong file refused,
the original relinked; exports 24-bit PCM mix + stems (headers read back), cancels an export mid-job (no file, no
partial), refuses a second export while one runs, and replaces the project under a running export (the old job retired
silently); then the three autosave laps (ADR-0068); then Save As, Save a Copy and a relaunch keeping the preferences.

**Run 4: 151 passed, 14 failed.** Known-red until ADR-0068: the autosave laps (the shipped autosave never writes).
Script fixes since (run 5 pending): the browser dock's settle, the concurrent-export refusal (the chooser still asks
where; the refusal comes with the path), the replacement case (a real New, Don't Save, Create — it used to Cancel).
**A real bug found:** after Save As, deleting every clip and a kill, relaunching with that bundle opens nothing and
says nothing (no project, no status) — the launch and Open paths ignore a failed load. Being root-caused with a gate.

**Runner (`tools/session-drive.ps1`):** `FileDrop` (a real OLE file drag from a small topmost source form, checked to be
under the press; an aborted drag closes its form), `WaitAlert` / `AlertButton` / `AlertText` (JUCE alerts through UI
Automation), `KillApp` (not `Kill` — PowerShell resolves that to its Stop-Process alias first and waited on input),
`Launch -AutosaveIntervalMs`, and `Focus` accepts the app's own modal window holding the foreground (the missing-audio
alert at a relaunch). Fixtures: `tests/fixtures/import/drive_48k_stereo.{aiff,flac,ogg}`, `drive_44k_mono.wav` (ffmpeg
commands in its README). Lessons in the drives README: no Home key in JUCE popups (pick by name), no Alt mnemonics in
its menu bar, never name a helper after a PowerShell alias.

## 2026-10-08 — Fix: a drop that adds tracks left the master fader over the new strip

**Now:** found by the SS-6 smoke drive and fixed. **Next:** the SS-6 drive tooling and its live runs, then ADR-0068's
checkpoints, then ADR-0067 cp2. **Decision still pending (Dan, plan §8.2):** the macOS frame exception (unchanged).

**What a user saw:** dropping two files onto the first lane of a one-track project added a track, and the master pane's
fader stayed where the master pane used to be — drawn over the new track's strip (two fader knobs on that strip, none
on the master). The rail's + Track button had the same fault. **Why:** the mixer's live widgets (the master fader, the
strip scroll bar, the FX editor's rows) were laid out only on a resize; a drop and the + Track button refresh the
state without one. **Fix:** every action refresh lays them out (a no-op when nothing moved); the strip scroll bar shows
only when the dock shows the mixer, whoever lays it out.

**Gates:** `[file-drop][layout]` — a two-file drop and then the + Track button each move the master pane right and the
master fader stays inside it (red without the fix). The critic's other find — the inspector's widgets have the same
layout-only-on-resize shape — is the next fix after it is shown to bite. ctest 420/420, Clang clean.

## 2026-10-07 — G6.4 cp1: meters read every block since the last look (ADR-0067)

**Now:** ADR-0067 cp1 (meters from real state) is in; ADR-0068 (the recovery contract) is accepted (8336ef0). **Next:** a
stale-layout fix the SS-6 smoke drive found (a drop that adds a track leaves the master pane's fader drawn over the new
strip), then the SS-6 drive tooling and its first live run, then ADR-0068's checkpoints, then ADR-0067 cp2 (hover and
pressed). **Decision still pending (Dan, plan §8.2):** the macOS frame exception — 505ad13 failed only that check on macOS
(sustained 23.8 ms); RTSan and TSan green with the new meter primitive. Reported raw, not excused.

**What a user gets:** a peak or a clip in ANY audio block reaches the meters — before, a meter showed one block in three
to twelve (only the last before each 33 ms screen tick), so a short peak could be missed and the clip light could miss a
clip. Stopped with nothing playing, meters fall to silence because no block ran (not because a flag said so), while a
stopped track that IS sounding — a note's release, an audition — meters it. The master pane has the strips' peak hold and
clip latch, its clip indicator is a real control (clickable, keyboard-reachable, read by a screen reader as "Master clip
indicator"), and the header's master meter shows the louder channel (a clip on the right alone used to be missed).

**How:** every metered peak (each track and bus meter tap, the master's left and right, the armed inputs) publishes
through `PeakSinceRead`, read exactly once per UI tick into the hold states that every painter and the probe read; the
control thread no longer writes an input's peak.

**Gates:** `[g6-motion][meters]` — a track, a bus, the master and an armed input each latch a clip in a block that is
not the tick's last; a stopped strip reads silence after a tick though its last block was loud; a stopped instrument
track sounding an audition reads its signal; an un-armed-then-re-armed input reads silence; no device block reads
silence and keeps the latch; the master clip indicator (by key and by mouse) clears the latch; a source inventory finds
each meter source read only in its tick step. Each case was turned red by its own mutation and back. `meter_tests`
re-grounded (a reset leaves the reading intact). ctest 420/420, Clang clean. **Critic:** the first gate covered the master
and the input only; the strip, bus, stopped, audition and re-arm cases were added and mutation-checked.

## 2026-10-07 — SS-6 groundwork: the probe sees the lifecycle; finding: the shipped autosave never writes

**Now:** the state probe carries what the SS-6 drive (`ss8`, drafted) asserts the project lifecycle by. ADR-0067 (G6.4) is
accepted (149d0c1). **Next:** the recovery contract (ADR-0068, in a design workflow — see the finding below), then
ADR-0067 cp1 (meters) on the `PeakSinceRead` primitive, then the rest of `ss8`'s needs (an alert-window helper, a hard
kill, fixtures, a file drop). **Decision still pending (Dan, plan §8.2):** the macOS frame exception — b39cbc2 was green
on every job, macOS included; 523b320 failed only that check (sustained 20.0 ms). Reported raw, not excused.

**Finding (2026-10-07, by this checkpoint's gate):** the shipped app has **never written an autosave**. The 30 s cadence
asks, but the write runs only when `PlaybackEngine::needsAutosave()` is true, and nothing in the app calls
`markProjectEdited()`. Every edit is written straight through to the bundle (SQLite WAL: a killed process keeps every
edit; a power cut can lose the last ones), so a crash does not lose work — but the power-cut-safe snapshot ADR-0019
promises does not exist, and once it does, the recovery question as written ("an autosave exists") would offer a
snapshot older than the bundle. ADR-0068 (the recovery contract) is being designed and reviewed before any fix lands;
the first draft assumed a save-based model and was withdrawn.

**What the probe carries (for drives):** `project.{sampleRateHz, tempoBpm, trackCount, clipCount, midiClipCount,
assets[].{hash, sampleRateHz, channels, frames}}` (the hash names the bundle's `audio/<hash>.asset`);
`autosave.{enabled, intervalMs, writes, failures, lastWritten, recovery.{pending, prompts, restores, discards, tracks,
clips, midiClips, assets, takes, compSegments, bundlePath}}`; `relink.{asking, name, refusal, damaged, questions,
lastOutcome}` (the missing-audio question while it is up); `export.{lastResult, outcomes, destination, retiring}`. A
drive shortens the autosave cadence with `YESDAW_AUTOSAVE_INTERVAL_MS` (250 .. 600000; anything else ignored).

**Gates:** `[project-lifecycle][probe]` — the project's rate, tempo, counts and its Asset by the bundle file's hash; the
cadence seam; the export's `none` then `succeeded`, its outcome count and destination; `[relink][probe]` — the question
in the probe while it is up (its name, the refusal of the wrong file, the count) and `relinked` / `cancelled` after;
`[export-job]` — `cancelled` and the destination on the model. ctest 420/420 (with the next commit's primitive), Clang clean. **Critic:** no blockers;
the relink question now clears even if the chooser throws, the model floors the cadence at 250 ms, the destination is
the started job's, the recovery block names its bundle and the takes / comp segments.

## 2026-10-07 — G6.3 live: a screen reader's view checked on the real app, and its focus move now adopts the target

**Now:** G6.3's desktop proof is in: `ss7` Step 15 reads the app through UI Automation (the tree Narrator and NVDA read)
and passes. **Next:** ADR-0067 (G6.4) is through its design workflow and under a second critic round; then the SS-6
drive's missing pieces (`ss8`, drafted: probe fields, an alert-window helper, a hard kill, fixtures, a file drop).
**Decision still pending (Dan, plan §8.2):** the macOS frame exception — 523b320 fails only that check on macOS
(sustained 20.0 ms); every other job green; reported raw, not excused.

**What a user gets:** a screen reader that moves its focus onto a control — a strip's mute, a fader, a widget — now hands
the keyboard that control as its target (the ring goes there; Enter and the arrows act on it). It never worked on a real
window: JUCE parents an element on its nearest focus container, the window, not the shell, so the shell's own handler
never saw the focus move (headless gates could not see it: there is no handler without a native window).

**Found by:** the live step — "a screen reader's focus move adopts the target" was red on the real exe. **Fixed:** the
shell asks the window's handler for the focused element and keeps only what lies inside the shell.

**Gates:** `[native][accessibility]` (new, Windows) — the shell as the content of a hidden, off-screen window: a screen
reader's focus move onto a painted element and onto a widget each become the target; the router's own Tab moves the
screen reader's focus and the next ticks leave it; Esc does not restart navigation (red without the fix). **See-it:**
`ss7` Step 15 — Tab to track 1's rail mute: UI Automation's focus is that element, by name and type (check box), on the
painted cell, the keyboard still the shell's; its Toggle mutes the track and reads the new state; strip 2's fader is a
slider with a range whose set is one undo step; a SetFocus on strip 3's mute moves the router there; every one of the 38
painted zones laid out has exactly one element standing on it. `ss7` 161/161 (Step 11's playhead read now waits for the
probe: it raced the transport once). ctest 417/417, Clang clean. **Critic:** no blockers; two checks sharpened (the
element count matched by position, a tautological assert dropped).

## 2026-10-07 — The desktop is free: every earlier real-app drive passes again (SS-1 .. SS-5, seven scripts)

**Now:** Dan freed the desktop. The seven existing drives ran on the real exe: `ss1` passed as it was; `ss2` .. `ss7` went
red at their first step and are repaired; all seven pass. **Next:** G6.3's live screen-reader step (UI Automation, in
`ss7`), then G6.4 (its implementation ADR is being designed and critic-reviewed), and the SS-6 script (`ss8`, being
drafted). **Decision still pending (Dan, plan §8.2):** the macOS frame exception — e0f5d7f and 6c9a3ed fail only that
check on macOS (sustained 21.5 ms and 22.5 ms against 16.6 ms; every other job green); reported raw, not excused.

**What was wrong:** the drives had not run since G5 (the desktop was busy), and three G5 changes had moved under them:
File > New now shows the New Project dialog before the location chooser (G5.5), the keymap lives in `prefs.json` (G5.6),
and the File menu grew (G5.5: Save a Copy, Save as Template), so a "7th item" pick became Import Audio. A script that
waited for the chooser straight after New left the dialog open over the shell, and every later click and Tab landed in
it — `ss7`'s 25 failures were almost all that one dialog. **One product bug:** the state probe published the dialog's
controls only while the mixer dock showed (a drive with the dock hidden could not find Create); fixed, with a gate.

**Repairs:** the runner gains `NewProjectChooser` (New -> the dialog -> Create -> the native chooser, each hop asserted)
and `MenuPickByName` (a popup item by its text through UI Automation, the tree a screen reader reads; FAIL when missing
or disabled), plus `UiaFocused` / `UiaFind` for G6.3's live check. Every script's New goes through the helper; `ss5`'s
and `ss6`'s File-menu picks are by name; `ss2`'s keymap check reads `prefs.json`'s `keymap`. README: the new primitives
and three lessons.

**Gates:** `[project-lifecycle]` — the New Project dialog's controls are in the probe's layout with the dock hidden (red
without the fix). **See-it:** ss1 45, ss2 42, ss3 57, ss4 22, ss5 77, ss6 58, ss7 144 assertions — all PASS on this
machine (Windows 11, the shared desktop, Dan away); every launch 1.2 – 1.5 s to the first interactive tick (B6: 3 s).

## 2026-10-10 — G6.3 done headless: targets that go away, overlays, a full walk, the ring as drawn (ADR-0066 cp3)

**Now:** G6.3's three checkpoints are in, headless-certified locally. **Next: G6.4** (interaction and motion states —
hover, pressed and target states; playhead and meter ballistics from real state; its implementation ADR first), then
G6.5 (empty states and first-run tips). **Pending the desktop:** SS-6 and the earlier real-app drives, and a screen
reader's live focus on the painted elements. **Decision still pending (Dan, plan §8.2):** the macOS frame exception —
CI keeps failing only that check on macOS (3daa429: sustained 25.5 ms); reported raw, not claimed excused.

**What a user gets (cp3):** nothing new to see — the gates prove what the router already did: when the control under the
keyboard target goes away (its strip scrolled out of the mixer, its track deleted, its inspector section dropped by a
smaller window) the target moves to a live control on the next tick; closing the keymap editor or Cancelling the New
Project dialog puts the target back where it was; Tab walks a real session end to end; the target ring keeps 3:1
contrast as it is actually drawn (at its alpha), not only as an opaque token.

**Gates:** `[g6-keyboard]` (new case) — the three going-away cases each leave the target on a live control in the walk;
the keymap editor and the New Project dialog (a shell that shows it) each take the walk inside and give the target back
when their close / Cancel is pressed, and Cancel creates nothing; a Tab walk from the first control to the last lands
on the walk's order at every step and reaches buttons, toggles, choosers and values and every surface's zones (rail,
tool strip, header, mixer strips and master slots, transport, inspector); `[tokens]` — the ring blended at its alpha
over every one of the 20 surfaces it can land on is >= 3:1. 417/417 local, Clang clean.

**Critic:** one blocker, fixed — "moved to a live control" was weaker than the ADR's law; each going-away case now
asserts the exact retarget (the control at the vanished one's Tab index, clamped). In: the walk checks every surface's
zones, the drawn ring over every surface, the deleted-track case first requires its target in the walk. The FX editor's
restore stays G4.0b's EQ test (the same overlay law).

## 2026-10-09 (night) — G6.3 checkpoint 2: every painted control has its own accessible element (ADR-0066)

**Now:** G6.3 **cp2** is in, headless-certified locally. **Next: G6.3 cp3** — targets that go away (a deleted track, a
strip scrolled out, a dropped section), overlays restoring the target, a representative full-shell walk, the ring's
contrast. **Decision still pending (Dan, plan §8.2):** the macOS frame exception — CI keeps showing it alone: 813391b
(cp1a) sustained 22.1 ms, c3df53b (the undo fix) 18.0 ms, every other test green; reported raw, not claimed excused.

**What a user gets:** a screen reader now sees each painted control as its own element — "Stem 3 mute, toggle button,
checked", "Stem 3 pan, slider, R10", "Mixer strip 2 send 1 level, slider, -6.0 dB", "Pad 1: load a sample, button" —
instead of one element per surface; pressing or setting it does what Enter (and the mouse) does; the router's moves
speak through it while the keyboard stays with the shell. The master fader reads "0.0 dB" to a screen reader (it said
"1.00").

**Found and fixed:** a screen reader pressing a painted control could crash the app — the press refreshed the shell,
which handed that same element a new model while its function was still running; the element now runs a copy, and a
pool rebuild retires old elements to the next message-loop turn instead of destroying them under a running call.

**Gates:** `[accessibility]` (new) — for every control in the walk (70 painted, 39 widgets at 1920x1080 with a tall
dock): a painted control's element is click-through, id-less, never focusable, exactly on its rect, with its role,
title, value text, range (values) and checked state (toggles) matching the target and press / toggle where they
belong; a widget's own JUCE handler has the right role, a title, the target's value text (values) and checked state
(toggles); an element's press mutes a track; a pan set through the element is one undo step; an insert slot's element
offers its menu; the router's Tab announces through the element (a widget target clears it) and the keyboard stays with
the shell; 50 refreshes that change nothing create no element. Ten runs in a row clean. Pins moved, deliberately: the
shell's child count +1 (the header's element layer); `[header-flex]` skips a child that takes no clicks and has no id
(that layer). 417/417 local, Clang clean.

**Critic:** no blockers. In: the widget half checks JUCE's handlers (role, value, checked); the announced element clears
for a widget; duplicate ids asserted. Kept: the value interface is JUCE's base one (its ranged one forces a bare number,
not the readout "+3.0 dB"); the pool's shape key is rebuilt per refresh (refreshes are not per tick; the walk costs
0.52 ms). Pending the desktop: the live screen-reader focus grab (a native window's), checked by a drive.

## 2026-10-09 (late) — G6.3 checkpoint 1b: the mixer's zones, the pads and Shift+F10 (ADR-0066)

**Now:** G6.3 **cp1** (both halves) is in, headless-certified locally. **Next: G6.3 cp2** — an accessible element per
painted control (click-through, focus-less proxies with role, name, value, state and actions). **Decision still
pending (Dan, plan §8.2):** the macOS frame exception.

**What a user gets:** Tab reaches every painted control on each mixer strip shown — S / M / R, pan, fader, meter, the
I/O rows, each send row and insert slot — the master's insert slots and the Sampler's pads; Enter mutes, opens an
empty slot's or well's add menu, opens a filled slot's editor, opens an I/O row's choices, loads a pad; a routed send's
level and the pan adjust with the arrows (Esc restores). **Shift+F10** opens the target's right-click menu — an insert
slot's Bypass / Remove / Move had no other home. An adjustment Esc puts back now leaves nothing in the undo history,
and a drag released where it began no longer adds an empty step either (the redo history survives both).

**Gates:** `[g6-keyboard]` — every mixer zone the probe names on screen (cells, pan, fader, meter, I/O, sends, inserts;
the master's slots) is in the walk, each kind at least once; strip mute toggles; pan reads L10 and Esc restores it with
no undo step of its own (one undo then takes back the previous real edit); the meter clears its clip light; Enter on an
empty insert slot / the output row / an empty send well opens that zone's menu; the add menu's kind fills the slot;
Shift+F10 on the filled slot opens its menu with Bypass; Shift+F10 on a rail cell opens the row's menu; Enter on the
filled slot opens its editor; all 16 pads are in the walk and Enter loads the first. `[mix-scalars]` (engine): a
gesture that comes back leaves no step, one that changes leaves exactly one, an empty one changes nothing, and the redo
history survives the first and last but not the second. 417/417 local, Clang clean.

**Amendment (ADR-0066):** a pad's click loads a sample (not an audition); JUCE never delivers the Menu key, so Shift+F10
is the context-menu key. **Parked:** the pads' Shift / Ctrl verbs (mode, clear) still need a keyboard route (a pad menu);
the "edited" marker stays set after an adjustment that came back (pre-existing: the dirty flag counts edits, not net
change).

**Critics:** UI — no blockers (the master slots' surface is the strips' input overlay, which covers the master pane).
Engine — no blockers; in: the redo history set aside and returned.

## 2026-10-09 (later) — G6.3 checkpoint 1a: the rail, tool strip and header's painted controls are keyboard targets (ADR-0066)

**Now:** G6.3 **cp1a** is in, headless-certified locally; **G6.2 certified** (4dc95a2, the cp2 compile fix, green on
every job). **Next: G6.3 cp1b** — the mixer's painted zones (S / M / R, pan, meter, sends, insert slots, I/O rows, the
master's slots), the Sampler's pads and the context-menu key. **Decision still pending (Dan, plan §8.2):** the macOS
frame exception (see below).

**What a user gets:** Tab now reaches every painted control on the track rail (each row's M, S, O, pan, volume, colour
swatch and meter), the timeline's seven tool cells and the header's gear and time readout — Enter mutes / picks a tool /
cycles the time display as a click does; on pan and volume, Enter then the arrows adjust, Enter keeps it as one undo
step, Esc puts it back exactly. Space stays play / stop throughout.

**Gates:** `[g6-keyboard]` (new) — every one of the 23 painted zones the probe names on screen (two rows x 7, seven tool
cells, gear, time) is in the Tab walk, row 0's before row 1's; toggles flip the model and read on / off; the rail volume
steps in dB (capped at unity like its drag, never snapping a higher gain down), Enter keeps one undo step, Esc
restores exactly; pan reads R10 after two steps; colour advances (one undo step); the meter clears its clip light; a
tool cell picks the tool; the gear toggles the settings row; the time readout cycles; Space toggles play before, while
navigating, while adjusting, after Enter and after Esc; a track removed mid-adjustment closes the gesture with no
empty undo step. Walk cost: 154 controls at 24 tracks build in 0.52 ms (gate < 4 ms). Bite: with the painted
collection off, coverage fails. 416/416 local, Clang clean.

**Critic:** no blockers. In: a vanished row / strip closes its gesture without an edit; the rail volume reads the true
gain; the announcement falls back to the shell; undo through the action; row order checked; the walk's cost measured
instead of cached.

## 2026-10-09 — G6.2 checkpoint 3: the macOS frame exception re-measured (ADR-0064) — a decision for Dan

**Now:** G6.2's three checkpoints are in; **ADR-0066 accepted** (G6.3: every painted control a Control target with its
own accessible element). **Decision pending (Dan, plan §8.2):** renew the macOS frame exception or not. **Next:** G6.3
cp1 — painted controls as targets. SS-6 still waits for a free desktop.

**The baseline** ([evidence](docs/evidence/2026-10-g62-gpu-baseline.md)): on Dan's machine the timeline frame check
holds 4.1–4.5 ms sustained in ten runs (budget 16.6) — the renderer has about 4x headroom. On CI, macOS failed the check
in 41 of 56 runs since the exception was written (39 with nothing else failing), sustained 16.7–30.5 ms (median 22.4),
and it passes and fails on adjacent commits with no change to anything the check compiles (9ed2983 passed, 86a0b31
failed); Windows CI failed 2 of 62 (17.1, 17.9 ms). The check prints its numbers only when it fails, so passing runs
add counts, not numbers.

**Recommendation:** renew the exception with this baseline, in the same narrow scope (macOS, this check, the sustained
budget only, nothing else failing), re-evaluated when the renderer, the test or the runner image changes or Dan's
machine measures above 8 ms sustained. Alternatives: make the macOS frame check informational in CI (the owner-machine
`verify-hardware.ps1` frame stage stays binding), or renderer work for the macOS runner (not justified by the data).
Until Dan decides, macOS GPU-only reds are reported raw with their numbers at each checkpoint and not claimed as excused
by a renewal.

## 2026-10-09 — G6.2 checkpoint 2: the scaling matrix, the 1152x640 window, everything reachable (ADR-0064)

**Now:** G6.2 **cp2** is in, headless-certified locally. **Next: G6.2 cp3** — the macOS frame exception re-measured
(the baseline is gathered: `docs/evidence/2026-10-g62-gpu-baseline.md`; the decision is Dan's under plan §8.2). Then
G6.3 (keyboard navigation and accessibility; research done — the router already owns a control target, the gaps are
painted controls and what accessibility sees). SS-6 still waits for a free desktop.

**What a user gets:** the window can be made as small as 1152x640, so a maximized window fits a 1920x1080 laptop at
150 % or a 1440x900 one at 125 % (it overflowed under the taskbar before); the status line (messages, refusal reasons,
hover hints) stays on screen at every size — it used to vanish at 1280 wide, and the Snap-mode / Edit-mode / Nudge
choosers (all in the Edit and View menus) now drop before it does; the Snap chooser no longer disappears once the mixer
was used (it hid whenever the Mixer panel was active, a rule from the modal-mixer days, leaving its SNAP caption over
nothing); a bar number in the ruler sits right of its tick (the tick ran through the digit) and gives way to a tempo
or meter change's label at the same bar.

**Gates:** `[layout][scaling]` (new) — 20 cells: the plan's 3 window sizes x 4 scales, and the maximized client of each
of the plan's displays at Windows' scales (floor(W/s) x floor((H-48)/s - 32): 1280x640, 1920x1000, 1536x793, 1280x656,
2560x1360, 2048x1081, 1706x896, 1280x664). At each: the header's rects inside it and disjoint; the dock and the master
inside the window; the tool row disjoint; every shown identified control at any depth inside the window and hit-testing
to itself at its centre; every control shown at 2560x1440 (with a clip selected) but dropped here has its actions in a
menu (the four toolbar choosers, the zoom controls and the marker list mapped to theirs) or, for an inspector section,
comes back whole with the dock collapsed; the exact drop set per cell pinned; the render at the cell's scale is the
physical size; on Windows (JUCE's software rasteriser) the render box-filtered back matches the 100 % render per panel
(mean difference <= 6; measured max 3.13) and at 200 % carries real detail (difference from a bilinear upscale of the
1x render >= 0.8, measured 1.63-3.75; a 1x image drawn at 2x scores 0, asserted). `[shell-sizes]` floor at 1152x640:
the FADES section is back with the dock collapsed. The dock clamp at the window minimum. `[rubric-shots]`: the column
above each bar tick is ink-free. `[ruler]` (new): a tempo change at a labelled bar hides that bar's number and touches
no other. Bites: the status line dropping (before the fix), a 1x-rasterised canvas cache (0.32 < 0.8), the centred bar
label, an un-skipped bar number under a tempo label. 416/416 local, Clang clean.

**Visual judgment (agent):** all 20 renders written; 1280x656 @ 150 % (the common 1080p laptop) — crisp text and lines,
nothing cut, the pinned master's columns clean, two whole lanes with the dock open; 2560x1440 @ 200 % at native
resolution — text, icons and waveforms rasterised at full resolution (no 1x blur).

**Critic:** no blockers. In: the matrix selects a clip (the inspector's sections were never in the reference before),
walks descendants, renders in software for CI determinism, pins the drop sets; map labels win the bars row. Parked: a
scrollable inspector (Logic-style) would bring the marker card back at 720p; today the marker list's job is reachable
through Transport > Previous / Next Marker.

**CI recorded:** 08be04a (ADR-0065) green on every job, the macOS GPU check included — **ADR-0065 certified**.
1497d09 (this checkpoint) failed to COMPILE on Linux and macOS: `meanAbsDifference` is used only inside the
Windows-only raster gates, so `-Wunused-function` fired under `-Werror` there (the local Clang check runs with Windows
defines and could not see it); Windows passed. Corrective commit: the helper is `[[maybe_unused]]`.

## 2026-10-08 (night) — the mixer's strips scroll and the master stays reachable (ADR-0065, amended)

**Now:** ADR-0065 is in, headless-certified locally. **Next: G6.2 cp2** — the scaling matrix (plan cells at
100/125/150/200 % and the maximized-display cells), the window minimum 1152x640, reachability (every control
hit-tests to itself; every droppable control's action in a menu; dropped inspector sections back with the dock
collapsed), scaled renders; then cp3 (the macOS frame exception re-measured). SS-6 still waits for a free desktop.

**What a user gets:** with more strips than the dock holds (16 tracks at 1280x720, 24 at 1920x1080) the master no
longer falls off the right edge: it stays pinned there, whole, and the strips scroll beside it in whole strips — by a
scroll bar under them, by the wheel (a strip a notch; nothing over the master), and by the selection (a new track, a
rail click, Up / Down and a bus rename bring the strip into view; a manual scroll is never undone). With few strips
nothing changes. The master pane's meter row now reads scale | fader | L R meters side by side — the fader used to
sit over the dB numbers and the meters even at 1080p, and the narrow-strips master (64 px) could not hold them at all.

**Amendment (recorded in the ADR):** the master is never narrower than 122 px (its scale, a 24 px fader, its meters)
nor than the strips' share; the strips share what it leaves. Two pins moved with it, deliberately: the shell's child
count (+ the scroll bar) and the narrow-strips master width (64 -> 122).

**Gates:** `[mixer-scroll]` (new) — few strips: the N3 layout, no bar; 24 tracks + 2 buses at the window minimum,
1920x1080 and 1280x720: the master pane's right / bottom at the panel's, the master fader inside it, the bar shown
and left of the master, every shown strip whole at the minimum width, disjoint, left of the master, its bottom plus
the bar at the master's bottom, every hidden strip without parts; the wheel moves one strip, the master pane not at
all; at the end the last bus whole; a mute click on the first shown (deep) strip mutes that track; a clip latch
survives a scroll out and back; Up / Down x20 and a new track bring their strips into view; a manual scroll survives
an action refresh (the refresh counter proves it ran); a hidden bus's rename brings it into view with the editor on
its strip; the master's scale, fader (>= 24 px) and meters pairwise disjoint inside the pane, fit and overflow.
Bites: the never-overflow law fails the master-inside-the-panel check; a fader column over its neighbours fails the
disjoint check. 416/416 local, Clang clean.

**Visual judgment (agent):** 1280x720 / 16 tracks — 13 whole strips, the bar under them, the master whole at the
right with INTEGRATED / TRUE PEAK and scale | fader | meters clean; 1080p / 16 tracks — all strips fit, the master's
columns clean (the scale's numbers were under the fader before).

**Critic:** one blocker, fixed — the funnel followed the mixer target and the rail lane separately, so a refresh that
changed both to different strips scrolled the first back out; now the mixer target wins. In: the follow memory and
the offset reset when the project changes. Parked: trackpad momentum moves a strip per event in the mixer, the rail
and the timeline alike (a shell-wide wheel accumulation is its own item); a window shrink can leave the selected strip
hidden until the next selection (the ADR follows new selections only).

**CI recorded:** ea8c596 (G6.2 cp1) green on every job, the macOS GPU check included — **G6.2 cp1 certified**;
55b5013 (ADR-0065) green.

## 2026-10-08 (evening) — G6.2 checkpoint 1: eight lanes at 1080p, track headers level with their lanes (ADR-0064)

**Now:** G6.2 **cp1** is in, headless-certified locally; **ADR-0065 accepted** (mixer overflow). **Next: ADR-0065** (mixer strips scroll, the master pinned at
the right — found in this checkpoint's judgment, accepted after a critic pass), then **G6.2 cp2** (the scaling matrix,
the 1152x640 window minimum, reachability, scaled renders, the mixer scroll), then cp3 (the macOS frame exception
re-measured). SS-6 still waits for a free desktop.

**What a user gets:** at a 1920x1080 window with the dock open, eight whole tracks show (seven before); every track
header on the left sits level with its lane (they were 15 px high since the ruler grew) and stays level after
scrolling to the bottom at any height (the two panels clamped the scroll separately); the timeline's tool row is a
compact 28 px with its controls centred; the inspector's section captions (GAIN, FADES, FADE CURVE, MARKERS) sit
inside their cards instead of on the edge.

**Gates:** `[layout]` (new) — at the window minimum, 1280x720, 1920x1080, 2560x1440 and a token-derived height where
the two row areas would round apart: every control on the tool row lies inside it with equal gaps (the painted tool
cells too), every on-screen rail row's top equals its lane's (heights too for whole lanes), a rail row whose lane is
off the canvas lies outside the rail's row area, at the top and scrolled to the bottom (the last lane reached); density
`>= 8` at 1080p in both density gates (was `>= 7`); the GAIN caption leaves the card's first 8 px empty and paints just
after. Bites checked: the scroll clamp fails without the rail footer (at the straddle height, scrolled), the row
containment fails with the old 8/26 control inset, the caption gate fails with a 0 inset. 416/416 local, Clang clean.

**Visual judgment (agent):** 1080p — the rail rows and lanes share every separator, lane 8 ends 4 px above the canvas;
the tool row reads tools · SNAP Beat · zoom · slider · Snap / Edit / Nudge · I X P A, centred; 720p — three whole lanes
(two before); the floor shot keeps the tools and the view cluster (the rest drops whole). **Found:** with 16 tracks at
1280x720 the mixer's later strips and the whole master run off the dock's right edge with no way to reach them (24
tracks at 1920 the same) — ADR-0065.

**Critic:** no blockers. In: the dead automation-row tokens (`automationLaneRowTopInset = 92` and the helpers built on
it) removed; the rail-level gate also checks rows whose lanes are hidden and that the bottom reaches the last lane; the
straddle height asserts its default-dock premise.

**CI recorded:** 379b6ae and 1bc4a6f (G6.1 cp1, cp2) fail only the standing macOS GPU exception (sustained 22.37 /
22.64 ms; 413/414 green otherwise) — **G6.1 certified**. fd5eaed (ADR-0064) green.

**Baseline gathered for cp3** (CI history since 2026-09-08): macOS ran the GPU check 56 times and failed 41, all on
the sustained budget (16.7–30.5 ms, median 22.4 in the failures), passing and failing on adjacent commits with the same
renderer; Windows failed 2 of 62 (17.1, 17.9 ms). The check prints its numbers only when it fails.

## 2026-10-08 (afternoon) — G6.2 decided: ADR-0064 (layout density and scaling)

**Now:** ADR-0064 accepted (two critic passes). **Next: G6.2 cp1** — the 28 px tool row and 4 px panel inset (eight
whole 72 px lanes at 1920x1080), the rail's rows level with their lanes (header 93 = outer inset + tool row + ruler,
footer 11 = scroll bar + inset; one `rowArea()`), the density gates back to >= 8, row-containment and rail-level gates.
Then cp2 (the scaling matrix, the window minimum 1152x640 so a maximized window fits 125 % / 150 % laptop displays,
reachability, scaled renders) and cp3 (the macOS frame exception re-measured). SS-6 still waits for a free desktop.

**Found while measuring (defects, fixed in cp1):** the track headers sit 15 px above their lanes (the ruler grew to
64 px in G0.7 cp3; the rail's 86 px header was never moved); the two panels clamp the vertical scroll over row areas
11 px apart, so at some heights a scroll to the bottom leaves the headers a row off; a maximized window on a 1920x1080
display at 150 % (client ~1280x656) cannot fit the 720 px minimum height.

**CI recorded:** 379b6ae (G6.1 cp1) macOS fails only `YesDawTimelineGpuCheck` (sustained 22.37 ms, max 33.03 ms;
413/414 otherwise green) — the standing exception; Windows still running. 1bc4a6f (G6.1 cp2) queued.

## 2026-10-08 (midday) — G6.1 done: colours once, no fake data, labels and tooltips in words (ADR-0063 cp2)

**Now:** **G6.1** is in, headless-certified (both checkpoints). **Next: G6.2 — layout and scaling** (the window /
display scaling matrix, the 1280x720 floor vs the code's 1152 minimum, eight tracks at 1080p; its implementation ADR
first). SS-6 still waits for a free desktop.

**What a user gets:** every tooltip reads as words with its live chord ("Play  (Space)", "Clip start: drag to change")
— the toolbar's said `transport.play`, the autosave buttons theirs, the clip time and fade fields `clip.inspector.*`,
and the automation lane row its component id; the inspector's stretch slider now has a "Stretch" label and its value
("100%", muted while the slider is disabled); the window's own background is the theme's; each track colour is defined
once.

**Gates:** `[tokens]` / theme audit — no raw colour in `src/ui` or `src/Main.cpp` outside the token headers (the audit
now catches a `u` suffix; `UiColourValues.h` is the JUCE-free home of the shared values); every `Tone` array is read
somewhere (the unused fake `inspectorAutomationValues` is gone); `[tooltips]` — every toolbar button's tooltip is its
accessible name and live chord (shown or hidden) and no tooltip anywhere contains a dotted code word or a stable id;
screenshot — the stretch label and value are drawn legibly at 1280x720 and 1920x1080. 416/416 local, Clang clean.

**Deviation (recorded):** ADR-0063 names the raw-colour pattern `0x[0-9A-Fa-f]{8}[uU]?`; the audit uses
`0xff[0-9A-Fa-f]{6}[uU]?` (opaque ARGB, as before, plus the suffix) so 8-digit non-colour constants such as a
`0xFFFFFFFFu` sentinel are not flagged — it still bites the five accents and the window background the ADR targets.

**Visual judgment (agent):** the GAIN card reads "Stretch [slider] 100%", aligned with the gain row. Noted for G6.2:
the card's "GAIN" caption sits tight to the card's left edge.

**Critic:** no blockers. In: the tooltip gate rejects any dotted code word, not only registered action ids (it found
five inspector fields and the lane row); the autosave buttons' tooltips follow live chord rebinds; the stretch value is
muted while its slider is disabled.

**CI recorded:** 86a0b31 (G5.7 cp2) fails only the standing macOS GPU exception — **G5.7 certified**. 9ed2983 (G5.7
cp1): Windows `YesDawTimelineGpuCheck` sustained 17.44 ms vs 16.6 on the runner — the commit touches only
`ProjectBundle.h` and relink tests (the GPU test includes neither) and 86a0b31, which contains it, passed that test on
Windows; a re-run of the same SHA was requested for the record. 379b6ae (G6.1 cp1) re-queued after a re-run cancelled
its pending run.

## 2026-10-08 (morning) — G6.1 checkpoint 1: the type scale and WCAG contrast (ADR-0063)

**Now:** G6.1 **cp1** is in, headless-certified. **Next: G6.1 cp2** — every colour defined once (the track accents and
the window background into one JUCE-free header; the raw-colour audit catches `0x...u` and covers `src/Main.cpp`), the
unused fake `Tone` array gone, toolbar tooltips in words ("Play  (Space)", never `transport.play`), the inspector's
second gain slider labelled. Then G6.2 (layout and scaling). SS-6 still waits for a free desktop.

**What a user gets:** every label is at least 11 px and ordinary UI text 12 px (the plan's section 3.4), with WCAG AA
contrast where it is actually drawn: the faint third text level is gone; the rail's lit S / M / O cells now draw dark
ink on the track colour (they were light text at 2.3-3.4:1); clip names stay readable on every track colour (the clip's
bright top is a little calmer); the drum-mode key names, the delay taps' stems and the ruler ticks are solid colours.
On a narrow timeline the zoom trio, its slider and the Snap chooser (with its caption) drop whole instead of sliding
under the I / X / P / A buttons (Snap stays in Options, its modes in View); the piano roll's Key and Scale choosers show
"Scale: Off" whole.

**Gates:** `[tokens]` (4 cases) — the type scale; every one of the 64 colour tokens classified (text, surface, fill,
indicator, decoration — a new token fails until it is), parsed from `UiTheme.h`; ~180 WCAG pairs (text 4.5:1 on every
surface and fill it is drawn on, including a clip name on each track colour's body and top and the lit cells' ink;
indicators 3:1, the playhead line included); no `withAlpha` on a text token in `src/ui`. Screenshot `[tokens]`: the
timeline toolbar row never overlaps at 1152x720 / 1280x720 / 1366x768 / 1920x1080 / 2560x1440 with the inspector open
and closed, the four view toggles always there; every Key / Scale item fits its box with 2 px to spare. V1 measures
with the WCAG formula and also samples a clip name and a lit rail cell. 416/416 local, Clang clean.

**Visual judgment (agent):** the rubric shots at 1280x720 and 1920x1080 and the 1152x720 laptop shots read cleanly at
the new sizes (mixer rows, insert names, the master meter's scale, clip names, header cards). They found the two layout
faults above (both gated now) — the rubric's "truncated Scale / fade / master labels" row closes here.

**Critic:** no blockers. In: `soloActiveText` on the lit DIM and MUTE fills asserted; the playhead (`white`) is an
indicator, not decoration; the chooser-fit gate keeps 2 px to spare. Not taken: "Snap unreachable when its chooser
drops" (verified: Options holds Snap Off / Bar / Beat / Sixteenth and View the snap modes).

**CI recorded:** 0655b5e (G5.6 cp3) fails only the standing macOS GPU exception — G5.6 certified; 9ed2983, 86a0b31
(G5.7) and the ADR-0063 commit pushed or queued.

## 2026-10-08 (early) — G5.7 done: missing audio is asked for at open and put back from its original (ADR-0062)

**Now:** **G5.7** is in, headless-certified (both checkpoints) — **every G5 item (G5.1–G5.7) is headless-certified.**
G5's exit is the logical **SS-6** drive (`tools/session-scripts/ss8-project-lifecycle.ps1`, still to be written and
run on the real app): it **waits for a free desktop** (Dan's go-ahead covered one night only; drives stay paused
until he frees it). **Next:** safe headless work continues — G6's implementation ADRs (tokens and icons, layout and
scaling, keyboard/accessibility, interaction states, empty states) and their headless gates, with the SS-6 script
written alongside so it can run the moment the desktop is free.

**What a user gets:** opening a project whose audio files were moved or damaged no longer just fails with a hash file
name: each missing or damaged file is asked about in words — "Audio Clip - 0:01.0 48 kHz mono, used by 1 clip" —
with **Locate...** (the file chooser) or **Cancel**. Only the original file is taken (the exact bytes the project
was made with, so it sounds exactly as before); another file is refused with the reason and the same file asked
again; a file that is right but cannot be written says what failed instead. Cancel leaves the current project as it
was and the status line names what is still missing; files already put back stay, so the next open asks only about
the rest. File > Open, Open Recent and the launch reopen all ask (the native launch asks once the window exists).

**Gates:** `[relink]` +4 (7 cases) — the open asks about a missing and a damaged file in turn, refuses the wrong tone
with its reason and asks again, takes the originals, opens, and the reopened project renders byte-identical to before
the files were lost; it saves and reopens with no question; Cancel keeps the current project (title, undo) and the
bundle byte-identical, naming both; one put back then Cancel, then the next open asks only about the other; the
launch reopen asks the same way (Cancel: no project, as a failed launch open); a pad-only and an unused Asset are
described; a right file that cannot be written is not called the wrong audio. `[missing-asset-open]` and
`[prepared-project-load]` pass unchanged (with no answerer the open refuses as before — the re-pin the ADR expected
was not needed). 416/416 local, Clang clean. Visual: the questions are JUCE's modal box and file chooser (drive
pending).

**Critic:** no blockers. In: an I/O failure while putting a right file back now says what failed ("could not be put
back: ...") instead of calling it the wrong audio (gated); the native deferred launch open keeps an earlier launch
reason (a missing device) beside its Cancel line.

**Parked:** imported clips are all named "Audio Clip" (ADR-0056's name for an Asset is its first clip's), so the
names in these questions and the browser rarely tell files apart — the length, rate and channels do; naming an
imported clip after its source file (as Logic does) is a separate improvement. Offline Assets, searching a folder
for the others, hashing off the message thread (ADR-0062).

**CI recorded:** bb8dfd6 (G5.6 cp2) fails only the standing macOS GPU exception; 0655b5e and 9ed2983 pushed.

## 2026-10-08 (small hours) — G5.7 checkpoint 1: inspecting a bundle's missing audio and adopting original bytes (ADR-0062)

**Now:** G5.7 **cp1** is in, headless-certified. **Next: G5.7 cp2 — the open's questions**: an open refused over its
audio asks about each missing or damaged file (Locate... / Cancel), takes only the original, opens again; Cancel keeps
the current project and names what remains missing; the launch reopen too. Then SS-6 and G6.

**What it adds (persistence, no UI yet):** a read-only inspection of a bundle that refused to open over its audio — it
lists **every** missing or damaged Asset file (the open stops at the first) and the stored project, changes no byte
(no migration, reconcile or sweep; an older schema is not inspected), and works on read-only media; and the adoption of
an Asset's **original bytes only** — another file is refused ("its content differs") with nothing written, a file
already whole is left alone, a damaged one is copied to `.trash` under a free name (never over earlier evidence) and
then replaced in one rename, so a failure leaves it where it was.

**Gates:** `[relink]` (3 cases) — the inspection lists both a missing and a damaged file past the first and leaves
every bundle byte as it was; an old-schema bundle is not inspected and not migrated; adoption refuses another tone
writing nothing, takes the originals, keeps the first damaged copy when a second is trashed, leaves a whole file
alone, leaves no temporary, and the open validator then accepts the bundle. 416/416 local, Clang clean. (Earlier in
this checkpoint `YesDawTimelineGpuCheck` read 21-24 ms under the -j6 suite with other load on the machine; its binary
is unchanged since 19:18 and passes alone — load, not code.)

**Critic:** no blockers. In: the inspection opens the database truly read-only (no journal-mode write, works on
read-only media); a damaged file is copied aside and replaced in one rename (a failed rename used to leave it missing);
a file that already holds the bytes is left alone.

**CI recorded:** bb8dfd6 (G5.6 cp2) and 0655b5e (cp3) pushed; results pending.

## 2026-10-07 (late night) — G5.6 done: the chosen audio devices (ADR-0061 cp3); persistent preferences complete

**Now:** **G5.6** is in, headless-certified (all three checkpoints). **ADR-0062** (missing audio relink) is accepted.
**Next: G5.7 — relink**, in two checkpoints: cp1 the bundle inspection (every missing or damaged Asset file, nothing
written; an old schema not inspected) and the adoption of an Asset's original bytes only; cp2 the open's
Locate…/Cancel questions, the status line, the re-pinned refusal gates and the render-identity gate. Then SS-6
(`ss8-project-lifecycle`, needs a free desktop) and G6.

**What a user gets:** the output and input devices chosen in the device choosers are remembered and reopened at the
next launch (before the project asks for its sample rate). A remembered device that is not there leaves the open one
and the status line says so in words — "Audio output Interface is not available - using Speakers", "- no input", or
"No audio device could be opened (Interface is not available)" — and stays remembered, so it is used again once it
is back. A device of another type (ASIO next to Windows Audio) is switched to through its own type.

**Gates:** `[prefs]` +2 (11 cases, 12 with the keymap gate) — a chosen output and input (one with a non-ASCII name)
are reopened at the next launch in the order output, input, then the rate request; both missing, listed-but-refusing,
no input open, no device at all: each named exactly, the open device kept, the choice never overwritten; with the
rate request refused the device reason and the rate warning share one line; back again, the remembered pair is used
and nothing is reported. 416/416 local (the GPU timing test green again: the earlier reds were load), Clang clean.
Visual: no layout change.

**Critic:** its blocker is fixed — the rate request's warning overwrote the device reason (now one line holds both,
gated with a refusing device); also in: the native switch goes through the device type that lists the name (the
chooser path too); an implicitly-used default input is named so a remembered default is not switched to again; the
callback is suspended once around both switches; the chooser is refreshed and the block size re-read only after a
real switch. Not taken: a gate for the restore's do-not-remember flag (it can only re-write the same name, so no
behaviour can show it).

**CI recorded:** 67d61fc (G5.6 cp1) fails only the standing macOS GPU exception; bb8dfd6 (cp2) running.

## 2026-10-07 (night) — G5.6 checkpoint 2: view, dock, editing and export preferences (ADR-0061)

**Now:** G5.6 **cp2** is in, headless-certified. **Next: G5.6 cp3 — the chosen audio devices** (reopened at launch
before the project-rate request; a missing one leaves the open device and the status line names both; never
overwritten by a fallback), then G5.7 (missing-asset relink) and SS-6.

**What a user gets:** the dock's tab and visibility, the inspector's visibility and tab, the snap unit and mode, the
metronome and the export bit depth, dither and normalize now follow the user across opens and launches instead of
resetting on every open. Panel sizes: a project keeps its own (its view-state record wins); a new project, or one
without a record, starts from the last arrangement the user made. The export controls remember a change as it is
made and show the remembered choice after a relaunch.

**Gates:** `[prefs]` +4 (9 cases) — dock tab, inspector, snap (bar), snap mode and metronome survive a New, a
relaunch and an Open, while with no session folder every New is factory (the harness unchanged); a project keeps
its sizes, a new one starts from the last arrangement, and A and B keep separate values across a relaunch (SS-6 step
7's logic); the export controls write on the click itself and show the choice after a relaunch; export choices
round-trip; a malformed view, editing or export key falls back alone (counted). `[keymap-editor]` re-pinned: the
click is remembered too, so the rebound chord is judged by what it toggles. 415/416 local, Clang clean (22 files);
the one red is `YesDawTimelineGpuCheck` (sustained 21-24 ms vs 16.6) from an unchanged binary (built 19:18, before
cp1; passed in today's earlier full runs; it links neither changed file) while another session renders video with
headless Chrome on this machine — load, recorded; CI's Windows runner is the gate. Visual: no layout change.

**Critic:** no blockers; it verified the snap mapping, no write-every-tick oscillation, the no-folder path and the
other session-folder tests. In: the export controls change model state with no action, so a change was saved only
by a later unrelated action — they now note it at once (gated through the real controls); the bit-depth chooser was
fixed at 32-bit float at construction while preferences load later — the controls now show the model's choices on
every refresh; the harness sets the inspector width through its own setter.

**CI recorded:** 2bb136d (G5.5 cp3) fails only the standing macOS GPU exception — G5.5 fully certified; 50e1253
(docs) cancelled by the next push; 67d61fc (G5.6 cp1) running.

## 2026-10-07 (evening) — G5.6 checkpoint 1: prefs.json and the keymap (ADR-0061)

**Now:** G5.6 **cp1** is in, headless-certified. **Next: G5.6 cp2 — view, dock, editing and export preferences**
(the last arrangement as the default for projects without their own view state; dock/inspector visibility and
tab, snap and metronome applied on every open; export bit depth, dither and normalize), then cp3 (the chosen audio
devices, reopened at launch, an honest reason when one is missing), then G5.7 and SS-6.

**What a user gets:** the user's settings now live in one `prefs.json` in the per-user folder; this checkpoint moves
the keymap into it (an old `keymap-overrides.txt` is imported once, silently, and retired). A broken preferences
file never stops a launch: the defaults load, the file is kept as `prefs.json.unreadable` and the status line says
so; a single bad setting falls back alone; settings a newer version wrote survive. A file an editor saved with a
byte-order mark still reads. Two actions that traded chords both keep them. The last-project, recent-projects and
per-project view-state records are now written whole (a crash can no longer leave half a record).

**Gates:** `[prefs]` (5 cases) — a rebind lives in `prefs.json` across launches; the old record imported once,
renamed, silently, never again once `prefs.json` holds a keymap; the same folder set twice reads nothing; no folder
writes nothing; empty, truncated, non-JSON, array and string files each give the defaults, are kept aside and
reported, and the next change writes a fresh readable file; a wrong-typed binding and a duplicate chord are each
rejected alone (counted) while the rest loads; unknown keys, a newer action's binding and a higher version survive
a rewrite; a swapped pair binds in either the file or the old record; a BOM and `7.0` read; an unreadable file never
imports the old record behind "defaults are in use"; the three records written whole, no temporary left.
`[keymap-editor]` re-pinned from `keymap-overrides.txt` to `prefs.json`. 416/416 local, Clang clean; the theme
audit caught a high-byte escape in the first BOM check (now a byte comparison). Visual: no surface change.

**Critic:** no blockers. In: a BOM is skipped (Notepad); an unreadable file plus the old record no longer imports
behind a "defaults" message; bindings apply as one set (swaps were rejected in either order, also in the old
importer); a version written `7.0` reads; a failed save says so; the no-folder test proves nothing is written. Not
taken: the probe's "none" (it is the no-folder state, not dead code).

**ADR-0061** accepted after a critic pass (its blocker: the keymap-editor gate asserted the retired file).

**CI recorded:** c5704cb and b871300 (G5.5 cp1, cp2) fail only the standing macOS GPU exception; 2bb136d (cp3) and
50e1253 (ADR-0061) pushed, CI running.

## 2026-10-07 (afternoon) — G5.5 done: templates (ADR-0060 cp3); new project, templates and copies complete

**Now:** **G5.5** is in, headless-certified (all three checkpoints). **Next: G5.6 — preferences** (its
implementation ADR first: `prefs.json`, the settings that belong to the user rather than the project), then G5.7
(missing-asset relink) and SS-6 (`ss8-project-lifecycle`, the real-app drive of G5.1-G5.7 — waits for a free
desktop).

**What a user gets:** **File > Save as Template...** saves the project's layout — tracks (names, colours, heights,
instruments and their settings, strips with inserts, sends, outputs), buses, the master strip, rate, tempo, meter
and markers — as a template in the per-user templates folder, never its content (no clips, audio, MIDI, takes,
automation, Sampler pads, loop/punch, locate points or scale; nothing soloed). A template of the same name is
replaced only after asking (the question names the template on disk). The New Project dialog lists Default first,
then the templates by name; one that cannot be used (it does not open, or it holds content) is listed with its
reason and cannot be chosen; choosing a template fills in its rate, tempo and meter, which can still be changed. A
project made from a template gets a new identity for everything — the project, every track, bus, insert, send and
marker — with its routing (outputs, sends, sidechain keys) rewired, so two projects from one template never share
an ID (ADR-0011).

**Gates:** `[project-lifecycle]` +4 (18 cases) — a template holds exactly the layout and none of the content (and a
Sampler keeps its kind with no pads); a bundle dropped into the folder by hand brings only its layout; New from a
template: no ID of the template anywhere in the project, the routing rewired, and a tone through it renders within
1e-6 of a hand-built twin of the layout; the dialog lists the folder (Default first, a broken and a content-holding
bundle refused with reasons and unselectable, `.partial` folders never listed) and picking a template by keyboard
sets its rate, tempo and meter, then creates; Save as Template asks before replacing (No keeps, Yes replaces, no
temporary folders left), a cancelled name writes nothing, two names sharing a file name are named as the file; a
template that cannot be used refuses New with a reason. File menu re-pinned to 13; `docs/keymap-v2.md` regenerated.
416/416 local, Clang clean (22 files). Visual: the dialog's layout is unchanged; templates are text items in its existing chooser (refused ones greyed by JUCE).

**Critic:** no blockers; it confirmed the ID remap complete against `engine::Project`. In: instantiation always
passes through the layout filter (a hand-copied bundle cannot carry loop, punch, scale, locate points or an
automation mode into new projects); the replace question names the template file it would replace; a failed restore
says where the old template is; fresh IDs avoid every ID of the template file, not just its layout. Not taken:
forcing the first tempo point to a jump (it would flatten a template's tempo ramp; the dialog sets where it starts).
Parked: case-insensitive sorting of the list.

**CI recorded:** 8cd395d (G5.4) green on every job; 83a3649 and 91cef0f (docs) green; a618c5c fails only the
standing macOS GPU exception. c5704cb (cp1) and b871300 (cp2) pushed, CI running.

## 2026-10-07 (midday) — G5.5 checkpoint 2: Save As and Save a Copy through one atomic bundle copy (ADR-0060)

**Now:** G5.5 **cp2** is in, headless-certified. **Next: G5.5 cp3 — templates** (File > Save as Template..., a
layout-only bundle in the session-state folder; New from a template gives every entity a fresh ID through one
map), then G5.6 (preferences), G5.7 (relink) and SS-6.

**What a user gets:** **Save As** and the new **File > Save a Copy...** share one copy routine. It builds the whole
bundle in a temporary `<name>.yesdaw.<n>.partial` folder next to the target — the database copied consistently
from the open project (SQLite `VACUUM INTO`; the original is never written), the audio, peak caches and view state,
but never the autosave folder or the trash — writes the current project into it, checks it (opening it validates
every stored rule and every audio file against its content hash) and only then renames it into place. A failure
anywhere leaves no target and no temporary folder, and the original stays open and working. Save As then continues
in the copy (clean, undo intact); Save a Copy leaves you in the original, still unsaved if it was, its undo and any
running export carrying on. Before, Save As wrote the original first, copied straight into the final folder (a
failure left half a bundle), copied the autosave folder (the copy could open with a stale recovery prompt), and
there was no Save a Copy.

**Gates:** `[project-lifecycle]` +5 (14 cases) — Save As: the target appears only at the rename, no `.partial`
remains, the copy is clean and current, its Asset files byte-identical, no autosave carried, the copy reopened from
disk renders byte-identical audio to the in-memory project, undo continues into the copy and the original's stored
project reads back unchanged. Save a Copy: the original current and unsaved, the copy closed (its folder moves),
reopened it renders the same audio, undo and a later Save work on the original, a peak cache mid-write is not copied.
An injected corruption mid-copy fails both with "the copy did not validate", leaving no target, no `.partial`, the
original working; an occupied target is refused before anything is written. An export held mid-render completes
through both. File > Save a Copy through its chooser; a refusal says why. The old Save As safety gate rewritten to
this law (it pinned a half-copied target). File menu re-pinned to 12 items; `docs/keymap-v2.md` regenerated.
416/416 local, Clang clean (33 files). Visual: the only new surface is one File menu item (no layout change).

**Critic:** no blockers. In: the copy goes file by file — a peak cache the builder renames under the walk is
skipped, temporary files are never copied, links never followed (it flagged a Save-right-after-import race); a
failed target check says so instead of "already exists". Not taken: a success message (the status line's law is
that success stays quiet); POSIX `rename` replacing a directory created empty in the same instant (nothing to
lose; a non-empty one still refuses). Parked: sweeping stale `.partial` folders left by a crash; seeding the Save a
Copy chooser with the project's name. ADR-0060's "new validateBundleAssetFiles (presence and size)" is met by the
open path's existing check, which is stronger (content hash).

**CI recorded:** 69ababc fails only the standing macOS GPU exception; every other job green.

## 2026-10-07 (morning) — G5.5 checkpoint 1: the New Project dialog, the device at the project's rate (ADR-0060)

**Now:** G5.5 **cp1** is in, headless-certified. **Next: G5.5 cp2 — Save As and Save a Copy through one atomic
bundle copy** (`VACUUM INTO` into `<target>.<n>.partial`, the open path's content-hash check validates it, rename;
autosave and trash never copied), then cp3 (templates), G5.6 (preferences), G5.7 (relink) and SS-6.

**What a user gets:** `File > New` opens a **New Project** dialog — sample rate (44.1 / 48 / 88.2 / 96 kHz), tempo,
meter and template (Default for now; templates are cp3) — and remembers the last choices. New never opens or
overwrites a project already at the chosen place. New, Open and Open Recent ask **Save / Don't Save / Cancel** when
there are edits since the last Save (Save on the untitled session goes through Save As; cancelling it cancels).
When a project is created or opened — including the one opened at launch — the audio device is asked to run at the
project's rate (a reopen); a device that can't is put back as it was and the warning names both rates. Before, every
project was 48 kHz / 120 / 4/4, New silently overwrote an existing bundle, and a 44.1 kHz project on a 48 kHz device
played 9 % fast.

**Gates:** `[project-lifecycle]` (9 cases) — the chosen rate, tempo and meter reach the reopened bundle and are
remembered; the keyboard alone creates a project through the overlay, Tab stays inside it, Esc closes it (one press;
while navigating, ADR-0049's order: the first Esc ends navigation); an occupied target is refused and left
byte-identical; a failed creation keeps the current project and its undo; Save / Don't Save / Cancel on a named
project and on the untitled session (Save As cancelled cancels New); the device asked once, not again at the same
rate, the warning naming both rates when refused, and the launch project asked too; the dialog's choices apply to an
injected project; out-of-range choices refused; the remembered record is radix '.' both ways and a bad value falls
back to the defaults whole. Screenshot gate `[new-project]`: the dialog inside the window, every control visible and
inside it, nothing the layout raises above it (it bit: the dock splitter was drawn across the dialog). Agent visual
judgment of the 1280x720 shot: pass (cosmetic nit parked: the meter reads "4 4" with no "/" between the choosers).
416/416 local, Clang clean.

**Critic:** no blockers; its five should-fixes are in (the dialog's choices win over an injected project; a refused
native reopen restores the previous device setup so the callback is never lost; the launch project asks for its
rate; the untitled-session gate; a locale-proof, strict record) plus the nits worth taking (a 0.5 Hz rate tolerance,
invalid choices refused). **ADR-0060** amended before publication: every edit already persists, so the copy law is
"never writes the source", not "the source stays as last saved".

**CI recorded:** cb528a6 and 2b636f0 fail only the standing macOS GPU exception (`timeline_gpu_tests.cpp:84`);
every other job green.

## 2026-10-07 (early) — G5.4 done: each Asset's audio is one shared buffer (ADR-0059, closes R30)

**Now:** **G5.4** is in, headless-certified. **Next: G5.5 — new project, templates and copies** (sample rate, tempo
and template in the new-project dialog; Save As and Save a Copy; its implementation ADR first), then G5.6
(preferences), G5.7 (relink) and SS-6.

**What a user gets:** memory per imported or recorded file is its audio once (a cross-rate file adds its resampled
view). Before, an idle file sat in memory at least twice; every recording pass copied all the project's audio on the
message thread; a running export copied everything twice more; the peak builder copied every file on each request;
and a closed project's audio stayed held by a cache. Now the model, the engine's clips and Sampler pads, export
jobs and the peak builder all share one immutable buffer per Asset, freed when the last holder lets go (on the
control thread — the audio thread never holds one).

**Gates:** `[asset-sharing]` — an Asset's buffer is the same object across an import of another file, an undo, a
recording and a refused drop and a save; ten clips on one Asset leave exactly one buffer and the engine's build
copied nothing (it reports how many Assets it copied: 0); a cross-rate Asset has its buffer and one live view; an
export job holds the model's buffer while it runs, keeps a closed project's audio alive until it ends, then the
audio is freed. Every existing playback, render, export and reopen gate passes unchanged. Decodes are checked
finite once, where they are made (import, reopen, recording — a non-finite take is not kept, with a reason); the
open path also checks outside decodes and now hands the engine the opened project's buffers (it used to copy every
Asset at open). 416/416 local, Clang clean; the critic found no blockers (its should-fixes are in: a slot whose
takes were all refused records nothing; exact no-copy and lifetime gates).

**ADR-0059** accepted after a critic pass; amended before publication (the export-job gate follows ADR-0058's
cancel-on-replace law; the device hot-swap clause waits for a model call site).

## 2026-10-07 (small hours) — G5.3 done: Export v2 (ADR-0058)

**Now:** **G5.3** is in, headless-certified (all three checkpoints). **Next: G5.4 — decoded-asset sharing** (one
decoded buffer per Asset shared by reference, which also removes the export job's per-Asset copy), then G5.5–G5.7
and SS-6. The real-app drive of G5.1–G5.3 (SS-6 steps 1 and 5) waits for the desktop.

**Export stems (the last cp3 step):** the settings row's stems chooser offers **Mix Only / Mix + Stems / Stems
Only**. A stems export writes one file per **top-level** track and bus (those routed to the master) beside the mix,
named `<song> - <strip>.wav` (file-safe on every platform; duplicates get " (2)"); a track inside a bus is part of
that bus's stem, so the stems sum to the master's input. Each stem is what its strip contributes at the master's
input: the master sum with every other input at zero gain (so delay compensation is the mix's), the master's
inserts replaced by a pure delay of their latency (stems start exactly where the mix does), the master fader at
unity. One normalize gain spans every file; every file has its own dither noise.

**Gates:** `[export-options]` — float stems of a project with a track into a bus, a bus into a bus, a post-fader
send and a muted track sum to the mix within 1e-6, and the muted track's stem is silent; with a latent master
Limiter the stems start on the mix's frame; a master Compressor keyed from a track renders; one normalize gain
(the loudest file at -1 dBFS); identical stems in 16-bit carry different dither; cancel mid-stems leaves nothing;
the file-name rules (reserved names, illegal characters, trailing dots, the 100-character cap, accented case
variants kept apart); through the model, Mix + Stems and Stems Only write their files. The settings row at
1280x720 is gated and judged (the stems chooser's first item now reads "Mix Only"). 416/416 local, Clang clean;
the cp3 critic found no blockers and its should-fixes are in (a stem's helper-node ids retry with another salt on a
collision; the exception path removes every output's temporaries; the name cap and accented duplicates).

## 2026-10-06 (night) — G5.3 cp3, step 1: dithered 16 / 24-bit export (ADR-0058)

**Now:** cp3 lands in four small steps: **dither (in)**, **ranges (in)** — the render stops at the range's end, a
range past the end is refused before anything renders (the cp1 follow-up) — **normalize (in)**: a "Normalize"
toggle brings the export's peak to -1 dBFS with one gain (silence is never boosted) — then export stems.
Range gates: a range exports exactly its frames (equal to the full render's slice); a range running past the end
exports to the end; a past-the-end range fails without the render ever running; through the model the loop region
and the ruler range (which wins) export their lengths. **What a user gets:** a 16 or 24-bit export is TPDF
dithered (a "Dither" toggle in the settings row, on by default); float exports are never dithered and stay
bit-exact. **Gates:** `[export-options]` — 16 and 24-bit files equal a reference built in the test by an
independent implementation of the same TPDF law; dither off equals the one-shot writer's plain rounding; no two
files or channels share noise. 416/416 local, Clang clean. **CI:** 6ccd0b6 (audition) fails only the standing
macOS GPU exception (Windows green).

## 2026-10-06 (night) — G5.3 cp2: an export never destroys an earlier file (ADR-0058)

**Now:** **G5.3 checkpoint 2** is in, headless-certified. **Next:** G5.3 cp3 — WAV 16/24/32 with TPDF dither, the
range options (rendering only up to the range, which also refuses a range past the end before rendering), export
stems and normalize.

**What a user gets:** an export writes `<name>.<n>.partial` next to the destination and only renames it over the
destination when the whole file is written. Cancel at any moment — or a failure, a project switch or quitting the
app mid-export — leaves no partial file behind and an earlier export with the same name exactly as it was. A
`.partial` a crash left behind is cleared the next time you export to that name. A failure names its cause.

**Gates:** `[export-commit]` — the chunked writer's bytes equal the one-shot writers' (float, 24 and 16-bit, odd
chunking); cancel during the render and during the write (held one chunk into the temporary, which exists while the
destination is untouched) each leave no `.partial` and the earlier file byte for byte; success replaces the earlier
file with the reference bytes; an unwritable destination fails with its cause and leaves nothing; stale temporaries
of the destination go while another destination's stay; through the model, a replaced project's job and a job cut
off mid-write at exit leave no temporary and no file; a still-running replaced job's temporary survives a new
export's sweep. 416/416 local, Clang clean. The critic found no blockers; its hardening is in (the sweep skips live
jobs' temporaries; an unexpected exception fails the job instead of the app).

## 2026-10-06 (evening) — G5.3 cp1: export runs as a worker job (ADR-0058)

**Now:** **G5.3 checkpoint 1** is in, headless-certified. **Next:** G5.3 cp2 — the `.partial` sibling committed by
rename only on success (cancel or failure leaves an earlier file untouched), streamed writing with cancel per chunk.
Then cp3 (WAV 16/24/32 with TPDF dither, ranges, export stems, normalize).

**What a user gets:** Export no longer freezes the app. It renders and writes on a worker; the readout counts up
(render, then write), Cancel (or Esc) stops it — "Cancelling…", then "Export cancelled" with no file — and editing,
importing, undoing and even recording keep working while it runs; the file is the song as it was when you pressed
Export. A second Export while one runs is refused with its reason; opening another project quietly abandons the
running one; success stays quiet as before (the readout shows 100 %).

**Gates:** `[export-job]` — held at a test latch mid-render, the UI tick services the job (Rendering, progress
inside the render half) while a clip-gain edit, an import, an undo and a recording commit all complete, and the
released job's file equals the synchronous export made before them, byte for byte; Cancel ends it with no file and
no count; a second export is refused; opening another project retires it with nothing reported; destroying the
model with a held job returns. `YesDawExportJobCheck` (pure; RTSan and TSan legs) — the job's file equals the
synchronous render for a same-rate and a cross-rate Asset; held, cancelled, released and destroyed jobs.

**ADR-0058** (Export v2) is accepted after a critic pass; amended before publication so a successful export stays
quiet on the status line (the existing law). The cp1 critic's blocker (a test latch outlived by its job) and its
cancel-during-write finding are fixed. A ruler range wholly past the project's end was briefly refused only after the
render; cp3's range step refuses it before rendering again.

**CI:** d4ce08c's Windows re-run passed (the first attempt's 16.72 ms GPU frame was a runner sample; the renderer
was unchanged); d4ce08c and a77c6eb fail only the standing macOS GPU exception.

## 2026-10-06 (late afternoon) — G5.2 done: audition in the media browser (ADR-0056 cp2)

**Now:** **G5.2** is in, headless-certified (both checkpoints). **Next: G5.3 — Export v2** (its implementation ADR
first: a worker with progress and cancel, an immutable job snapshot, a temporary sibling committed only on success,
then WAV 16/24/32 with dither, range, stems and normalize). The real-app drive of G5.1 + G5.2 (SS-6 step 1: "open
the browser and audition a supported file") waits for the desktop — another session holds it for its drives now.

**What a user gets:** every file row in the browser has a play mark; click it (or select the file and press
Audition) to hear the file from its start on the monitor. It plays at the project's rate even when the file's rate
differs. Click again (the Audition button reads Stop), audition another file, press Play or Record, or close the
browser and it stops; it stops by itself at the file's end. Monitor Mute and Dim apply to it, the header meters show
it, but it is never in the mix: the loudness readout, an export, a bounce and a recording never contain it. With no
audio device it says "no audio device".

**Gates:** `[audition]` — two models in lockstep, one auditioning while the song plays: their device outputs differ
by exactly the file's samples from frame 0 (mono to both outputs), their live loudness readouts are identical, and
their exports are identical sample for sample; a new audition, Stop and Play end it; Mute silences it after its
5 ms ramp; no device refuses it with nothing published; a 44.1 kHz second plays 48 000 frames; the voice retires at
its end and is freed under the device-block watermark; the browser's button, play marks, second press, closing the
browser and Space all behave. `YesDawAuditionCheck` (pure, on the RTSan leg) proves the device-thread read never
allocates or locks. The browser shot gates the play mark (a file row paints one, a folder row none). 415/415 local,
Clang clean. The critic found no blockers; its should-fixes are in: Record's no-input path now stops an audition
too (gated with a real Record), and opening another project stops it.

**CI:** d4ce08c's Windows job failed the timeline GPU frame gate at 16.72 ms against 16.6 ms. Not the standing
macOS exception, so not excused: that commit changed only 12 lines of new browser layout constants in the
renderer's include set (7f2303a passed Windows with the same renderer). Re-run on the same SHA: Windows passed.

## 2026-10-06 (afternoon) — repair: a shortcut pressed while the app is busy means what you pressed (ADR-0057)

**Now:** done, outside the G5 line (Dan asked for the product fix of the drive finding below). **Next:** unchanged —
G5.2 cp2 per the entry below. [Evidence](docs/evidence/2026-10-06-key-time-modifiers.json).

**What a user gets:** a quick Ctrl+Z, Ctrl+S or Alt+arrow tapped while the app is busy (an edit that rebuilds the
engine, a project load) does what was pressed. Before, Windows' JUCE read Ctrl/Shift/Alt when the app got round to
the key, so the tap could arrive as a bare Z (zoom) — or a bare Z followed by Ctrl became an accidental Undo.

**How:** the shell's window publishes the modifiers Windows recorded with each key (its message-synchronised state)
while that key is handled, and the Command router builds the chord from them. No JUCE patch. JUCE text fields,
popup menus and Ctrl+click keep JUCE's own reading (out of scope, written in the ADR).

**Gates:** `[key-time-modifiers]` (the real shell on a hidden native window; the thread's key state says "Ctrl
held", the physical keyboard says "up"; a real key through JUCE's own message loop) — red with the router line
reverted (`"timeline.zoom.selection" == "edit.undo"`), green with it, 20/20 repeats. Real app: the drive's new
`KeyWhileBusy` holds the app's UI thread still while the chord goes in, so the whole chord waits in the queue every
time; SS-2 step 10 fails on the code without the fix (Ctrl+Z -> zoom, Ctrl+Shift+Z -> zoom, Z then Ctrl -> Undo)
and passes with it. Quiet batch on the fix: **ss1–ss7 PASS, 420 assertions**. ctest 414/414, Clang clean. The
drive's `Key` keeps its idle wait and modifier holds.

**Lessons:** a timing drive needs a quiet machine — another session's build pushed launches from 1.3 s to 6–19 s
(SS-3's B6 FAIL), and a foreground app refused the drive focus; both re-ran clean. Recreating a window resets the
thread's key state to the physical keyboard, so a test that sets that state must let the window settle first.

**CI:** the fix `f8174d7` (run 37534088888) and the `KeyWhileBusy` primitive `1dbd0d2` (37534125736) are green on
all 10 jobs, macOS included; the ADR `7f14c16` and this entry `365a55d` passed the docs path. `e3102ab` (the SS-2
script only) went red on Windows alone: `YesDawTimelineGpuCheck` sustained frame 17.91 ms vs 16.6 (max 20.04,
13 slow frames; run 37537021589). Every input to that check is byte-identical to the green `1dbd0d2` (it includes
nothing this repair changed), and the next two code runs on top, `6ccd0b6` and `0ab9c84`, are Windows-green:
runner timing variance near a thin margin, as with `b26c9e5`. Not rerun, not called green, threshold unchanged;
noted for the G6.2 runner baseline.

## 2026-10-06 (afternoon) — G5.2 cp1: the media browser (ADR-0056)

**Now:** **G5.2 checkpoint 1** is in, headless-certified. **Next:** G5.2 cp2 — the **audition voice** (hear a file
before importing it, on the monitor path only), then the real-app drive check of G5.1 + G5.2 when the desktop is
free (drives take the mouse and keyboard; the overnight hands-off grant has ended).

**What a user gets:** press **Y** (or View > Browser) and the editor dock shows a **Browser** tab:
- **Files** — one folder at a time: `..`, its folders, then only the audio files YES DAW can import, with each
  file's format, rate, channels and length (read from the header only when the row is on screen). A file that
  cannot be read says why, in red ("not a readable MP3 file"). It remembers the last folder.
- **Project** — this project's audio, named by the clip that uses it, ready to place again without re-importing.
- **Recent** — the last 20 audio files imported from anywhere, newest first; a file that is gone says "missing".
- Double-click, the Import button or Enter imports a file on the selected track at the playhead (exactly what
  Ctrl+Shift+I does); Ctrl / Shift+click several and drag them onto the lanes (exactly an OS drop: consecutive
  tracks, one undo step; below the last track makes new ones); a project Asset placed this way is a new clip on
  the same audio, no copy.
- Keyboard only: Tab to the list, Enter, arrows, Enter opens a folder or imports, Esc puts the selection back.

**Gates:** `[browser]` (8 cases) — keep equals Ctrl+Shift+I (same track, tick and Asset row); a two-row drag equals
an OS drop and undoes in one step; a project Asset adds a clip and no Asset; unreadable and missing files show
their reasons in the row and on the status line and change nothing; Files order with non-ASCII folder and file
names; Recent and the browser state survive a new shell; the keyboard path; a 1000-file folder reads only the
painted rows' headers (and a scroll only the new ones); a rebuild after another surface's import keeps the
selected rows. `[ui][screenshot][browser]` paints a folder row, a file's facts and a red reason in their
columns (agent visual judgment: rows read name -> facts; the first draft put facts 1,700 px from their names at
1920 wide — fixed). 414/414 local, Clang clean. The critic found no blockers; its should-fixes (over-eager
rebuild resetting the selection, ANSI file names on the status line, an unbounded header cache) are fixed.

**CI:** Linux and macOS failed to build `YesDawResampleCheck` from dd439e8 (a copying structured-binding loop
under -Werror); fixed in 7f2303a. 79c75c8 fixed a real macOS-only flake (two files of one drop sharing an id).

**Fixed: names outside the ANSI code page crashed the app on Windows.** `path.string()` converts through the ANSI
code page and throws on a character it cannot hold, so a project named in kanji (window title, Open Recent), a
drop of a kanji-named file (every drop named each file), a refused file, a Sampler pad or a MIDI export with such
a name threw. All 27 sites now go through `io::utf8Text`; the gate (a kanji project, a kanji WAV dropped, a kanji
junk file refused by name, Open Recent) threw "No mapping for the Unicode character" before the fix.

## 2026-10-06 (day) — G5.1 in: import formats (ADR-0054) and cross-rate audio (ADR-0055)

**Now:** **G5.1** (both checkpoints) is in, headless-certified. **Next:** the real-app drive check of the new
import surfaces (pending: the drives take the mouse and keyboard, and the overnight hands-off grant has ended —
they run when the desktop is free again), then **G5.2 — the media browser** (its ADR first).

**What a user gets:**
- WAV, AIFF, FLAC, Ogg Vorbis and MP3 import through Ctrl+Shift+I, a drop on the timeline and the Sampler pads,
  and reopen the same on every platform; the project keeps each file's own bytes. Every refusal says why
  ("junk.mp3: not a readable MP3 file", "6 channels (mono or stereo only)", "AIFF compression 'ulaw' is not
  supported", "unsupported sample rate (4000 Hz)"). A drop of several files lands them on consecutive tracks —
  making the tracks it needs — as one undo step.
- **Files at another sample rate import and play at their true speed and pitch** (8 kHz to 384 kHz): a 44.1 kHz
  file in a 48 kHz project plays through a resampled view built once on import (live tier) and exports through
  a long-kernel one (offline tier); the clip keeps its source window in the file's own frames; split, trim,
  slip and stretch work through the rate ratio; a 44.1 kHz Sampler pad sounds at its own pitch.
- Copy and paste keep every clip setting (stretch, reverse, fade shapes, colour, mute were dropped before).

**Gates:** `[import-formats]`, `[cross-rate]`, `[resample]`: lossless decodes equal the source PCM; MP3 lengths
pinned on every platform; a 44.1 kHz sine plays live at -95.5 dB and exports at -132.7 dB error against the
analytic 48 kHz sine (gates -60 / -80); a 96 kHz file's 30 kHz tone is kept from aliasing (-85 / -116 dB);
a click lands where the ratio puts it; a split renders bit-identically; slip moves 441 Asset frames for 480
ticks; reopen and the self-check play cross-rate bundles. A three-minute stereo 44.1 kHz file's live view
builds in 0.88 s (export view 3.2 s). 414/414 local, Clang clean.

**CI:** a CI-only race in the G4.7 master-inserts gate (the Limiter face filled on the next UI tick) is fixed
at the source in 130d184; macOS otherwise fails only on the standing GPU sustained-frame exception.

**Fixed after the G5.1 critic and CI (2026-10-06 afternoon):**
- **A multi-file drop could refuse a file** (macOS CI on 2665f20: 2 tracks, not 3). Every file in one drop took
  its Asset id from the same project state in the same millisecond, so two fast copies got one id and the second
  was refused. Session ids now carry a per-model serial; the gate makes 1000 ids back to back (all differ) and
  drops 24 tiny files at once (24 Assets).
- **Opening a cross-rate project while another rate's project was open** built its views at the old rate and
  refused the open; views are now built at the opened project's rate (gate: open a 48 kHz bundle with a
  44.1 kHz Asset over a 44.1 kHz project; it plays exactly as before). Views of Assets that left the project
  are freed.
- **A drop below the last track** landed on the last track; it now starts a new track there, and a MIDI file
  after the audio gets its own track instead of stacking.
- Trim and reverse on a 44.1 kHz clip keep the rate ratio (gated); the demo self-check renders through the
  view-frame project.

## 2026-10-06 (late night) — G4 done: SS-5 "Mix the song" passes with built-ins

**Now:** **G4 is certified** (plan §6 exit: logical SS-5 with built-ins, every earlier journey restored).
**Next: G5 — project lifecycle**, starting with **G5.1** (import formats and cross-rate audio; its
implementation ADR first, under ADR-0010). [Evidence](docs/evidence/2026-10-06-g4-exit.json) ·
[montage](docs/evidence/2026-10-06-g4.png).

**What a user can now do, end to end on the real app (ss7):** route a vocal to a bus with EQ and a
compressor, send it to a new reverb bus, ride the bus fader in Write while the song plays (a Bus Fader lane
appears; Write returns to Touch), keep the reverb solo-safe, put a Limiter on the master, read the mix's
loudness, dim or mute the monitor, and export the mix to a WAV.

**Gates:** quiet batch **413/413** (ss1–ss7) on 36e471b; ss7 now drives SS-5 as the plan writes it.
The probe adds each strip's soloSafe, the export count / progress and each fader's knob.

**Open observation:** one ss7 run lost the Compressor's editor between a Threshold drag and the Presets
click (the click fell through to the timeline); the next three runs passed it. Unknown cause — if it
recurs, capture fxEditor.visible per tick around the drag before acting.

**CI:** e1d0f43 green on every job but Windows (running at writing; macOS passed this time); the later heads
are pushed one at a time so each code commit gets its own run.

## 2026-10-06 (late night) — G4.7 done: the master strip (ADR-0053)

**Now:** **G4.7** is done. **Next: the G4 exit** — ss7 does SS-5 as the plan writes it (a reverb bus fed by
a send, Write automation of the bus fader while playing, the reverb bus solo-safe, export), then G5 → G6.
[Evidence](docs/evidence/2026-10-06-g47-master-strip.json).

**What a user gets:**
- **The LUFS readouts are real.** The header's LUFS button and the mixer's INTEGRATED / TRUE PEAK cards show
  the loudness of what you played — measured from Play or from a locate while playing, held when stopped,
  one listen across a loop wrap or an edit, "~" if the UI stalled long enough to drop audio.
- **Master Dim / Mute**: DIM (amber) and MUTE (red) on the header MASTER card, in the Transport menu and the
  master's menu. The speakers get 20 dB less or nothing after a 5 ms ramp; the header meter still shows the
  signal; exports and renders are never touched; the status line names them; nothing is saved or undoable.
- **The master's inserts are on its pane** (under TRUE PEAK, above the meters): an empty slot lists the
  kinds with the Limiter first, a double-click opens the editor with the Limiter's gain-reduction meter, the
  slot menu bypasses.

**Gates:** `[loudness-live]`, `[loudness-tap]`, `[g47]`, `[master-inserts]`. Quiet batch **396/396** on the
final build (489b653); ss7 Step 13 drives the master strip on the real app (the readout read -28.4 LUFS on
the song stem). One critic pass: both should-fixes taken (the compact pill face is opt-in; the ramp is timed
at the device rate); one claim rejected with a test (shuttle does not feed the meter).

**CI:** a8ce649 and b785626 green except macOS, red only on the standing GPU sustained-frame exception (plan
§8.2); e1d0f43 in progress and 489b653 pending at writing.

## 2026-10-06 (night) — G4.6 done: automation you can write, hear, draw and move with clips

**Now:** **G4.6** (ADR-0052) is done. **Next: G4.7 master strip** (dim / mute, a header loudness readout
that is actually fed, a limiter editor reachable from the master) → G5 → G6. [Evidence](docs/evidence/2026-10-06-g46-automation-v2.json).

**What a user gets:**
- **Modes Read, Touch, Latch, Write, Off.** Touch writes while you hold a control; **Latch** keeps writing
  your last value until stop; **Write** writes every lane the selected strip owns from play to stop (touch a
  control to ride it) and then returns to Touch — the pass and the mode change are one undo step.
- **You hear a ride while it lasts** — fader, pan, sends, insert parameters and instrument parameters.
- **A second pass rewrites the bars it covers** (it used to be refused); the automation outside a pass is
  untouched. A loop wrap or jump back commits the pass so far and carries on.
- **Lanes per track, stacked under the track** (the A key / button shows the selected track's lanes; saved
  with the project's view): each lane the track owns, named, one under another; the last lane's chooser
  starts a lane for another target; other shown tracks offer **+ Lane** there.
- **Pencil** draws (one point per snap step), **Shift+Pencil** draws a straight line, the **Eraser** sweeps
  points away — one undo step each.
- **Edit > Automation Follows Clips**: moving clips in time (including neighbours Shuffle moves) carries
  their automation, locked to the audio across tempo changes; one undo restores clips and automation.

**Gates:** `[automation-v2]`, `[automation-ride]`, `[automation-tools]`, `[follow-clips]`, `[budget]` and
render gates (the audio outside a replaced span is unchanged, a Write pass sounds its values, a clip moved
with its automation renders the same audio shifted). Quiet batch **385/385** on the final build; ss7 drives
the lanes (A, Shift+Pencil line, Eraser, Ctrl+Z) on the real app. Two critic passes; every should-fix taken.

**Repairs found on the way:** the engine's event budget undercounted dense lanes (a possible audio-thread
assert — now refused with a reason); a latched ride snapped back when another edit rebuilt the engine;
Shuffle neighbours left their automation behind; the drives sent key chords before the app was idle
("Ctrl+Z" read as "Z" — the drive now waits; the product side is its own task).

**CI:** heads through 677ab52 are green except macOS, red only on the standing GPU sustained-frame exception
(plan §8.2); e005b43 (the critic fixes) in progress at writing.

## 2026-10-06 (evening) — G4.5 done; four real bugs fixed; G4.6 automation v2 decided (ADR-0052)

**Now:** **G4.5** is done and **ADR-0052** (automation v2) is accepted. **Next: G4.6** — stacked lanes,
real Write and Latch, span-replacing rides you can hear, Pencil / Shift+Pencil line / Eraser, "automation
follows clips" — then G4.7 master strip → G5 → G6. [Evidence](docs/evidence/2026-10-06-g45-and-repairs.json).

**G4.5 — what a user gets:** Ctrl-click (Cmd on Mac) a Solo cell, in the mixer or the track list, solos only
that strip (one undo step; Ctrl-click the only soloed strip to clear it). The header's **SOLO** beside Loop
lights amber while anything is soloed; a click clears every solo. The strip and track-header menus carry
Solo Exclusively, Clear All Solos and Solo Safe. Quiet batch **377/377**; critic acceptable.

**Bugs found and fixed on the way (each proven by a test that failed first):**
- A **bus** strip's S / M cell did nothing unless some strip was already selected.
- **Instrument-parameter automation could not be saved**, and a project with **bus-send automation could not
  reopen** — schema **v33** rebuilds the automation tables (every existing lane and point kept).
- **Automation played 1.56x later than drawn** at 48 kHz / 120 BPM: the lane stored frames where the engine
  reads musical ticks. Existing projects keep their stored points — the sound is unchanged; the lane now
  shows each point where it really plays.
- **MIDI clips' notes were drawn ~1.56x late** in the arrange view (display only).

**Open observation:** one ss7 run inside a busy batch (two background agents working) missed six steps
(clicks/keys landing without effect); the same build passed ss7 alone twice and the whole batch when quiet.
Load is the supported suspect, not proven — if it recurs, the drive's input waits are next.

## 2026-10-06 (later) — G4.3 and G4.4 done: New Bus, Route to New Bus, compressor sidechain

**Now:** **G4.3** and **G4.4** are done. **Next: G4.5** solo/mute UX (solo-clear control, Ctrl-click
exclusive solo; solo-safe is already in the strip and header menus) → G4.6 automation v2 → G4.7 master
strip → G5 → G6. [Evidence](docs/evidence/2026-10-06-g43-g44.json).

**What a user gets:**
- **G4.3:** an empty send well's chooser (and the strip menu's Add Send) ends with **New Bus** — the bus and
  a unity post-fader send to it in one undo step; the OUTPUT chooser ends with New Bus too; the track
  header has **Route to New Bus**. New buses are named "Bus N" past any name already taken.
- **G4.4 (ADR-0051, accepted after a critic, committed before any code):** a Compressor's editor has a
  **Sidechain** chooser — None, the tracks, the buses; its own strip and anything that would loop the
  routing are disabled. The key is the source's pre-fader signal after its inserts, so a kick with its
  fader all the way down still ducks the bus (the "ghost trigger"); muting or solo-muting the source
  silences the key (ADR-0014). Saved in schema **v32**; one undo step; the strip's **SC** badge is now real;
  a keyed track or bus cannot be removed until its sidechain is cleared (the status line says so).

**Proof:** G4.3 batch 367/367 (worst launch 1,500 ms, paint p95 3.96 ms); G4.4 batch **372/372** (1,385 ms,
3.81 ms); local suite 384/384; Clang clean. Render gate: the ghost kick ducks the Pad's bus by 27 dB;
unkeyed, muted, solo-muted and bypassed renders stay steady; set-then-cleared renders bit-identically to
never keyed; a keyed Compressor is a latency-compensation convergence point. Both critics acceptable.
CI: `cb7b0ad` (G4.3) and `0ac78ad` (G4.4 engine) green except macOS on the standing GPU timing exception.

## 2026-10-06 — G4.2 complete (faces, drag reorder, FX presets); G4.3 sends and buses next

**Now:** **G4.2 is done.** One Release build (code `b26c9e5`, harness `a01a90e`) passed **all seven
journeys, 362/362** (SS7 91: drag-reorder + undo, and a Compressor preset saved through the name prompt,
loaded back after an edit, undone in one step). Worst launch 1,406 ms, paint p95 3.88 ms; local suite
380/380; Clang warnings clean. Exact-code CI (`a01a90e`, same app source): every job green except macOS
on the standing GPU sustained-frame exception (18.77 ms). **Next:** G4.3 — New Bus in the send and
output choosers, Route to New Bus on the track header (code and `[sends-v2]` gate written, being
verified) → G4.4 sidechain → G4.5–G4.7 → G5 → G6. [Evidence](docs/evidence/2026-10-06-g42-close.json).

**Shipped since the last entry (each its own commit):**
- **cp5** transfer curves for the compressor and limiter; faces drop whole when rows run short.
- **cp6** drag an insert slot to a new place in its chain — one undo step; a stale-carry bug that merged
  fader undo steps was found and fixed with a test that fails without the fix.
- **cp7 FX presets** under the new **ADR-0050** (accepted after two critic passes, committed before any
  code): per-user `.yesfx` files of real values by stable parameter name (the EQ's six bands carry an
  ordinal), all nine kinds, a Presets button and menu in the editor, Save Preset… asks for a name, a load
  is one undo step, and a bad file is refused whole with one reason (21 refusal cases gated). Critic on
  the code: acceptable; its NUL-byte suggestion was taken.
- **Product fix — garbled text.** The compressor face showed "Gain reduction (dB) â□□ not running" (seen
  in a drive screenshot). Root cause: JUCE reads a plain `const char*` as ASCII. Six more such strings
  (two tooltips, a piano-roll hint, the Edit menu's "Undo History…") were broken on every platform, and
  24 hover hints using `\u00b7` were right on an English Windows machine only by luck ("Â·" on Mac and
  Linux). All fixed; action labels are now ASCII ("Undo History..."); a new source audit with a negative
  control makes any such literal a red test.
- **Tools:** `clang-warnings.ps1` no longer dies before printing a finding; the native file chooser that
  drops typed characters (seen once in ss1: `sers\...` for `C:\Users\...`) is retyped, bounded, and
  Enter is pressed only on an exact readback.

**CI notes (raw numbers in the evidence file):** `b26c9e5` went red on **Windows and macOS**, each only
on the GPU sustained-frame budget (Windows 17.06 ms vs 16.6). Windows is not covered by the macOS-only
exception, so it was investigated: every input to that check is byte-identical to the green `394d752`,
and the next run on the same app source is Windows-green — runner timing variance near a thin margin.
Not rerun, not called green, threshold unchanged; flagged for the G6.2 runner-baseline review.

## 2026-10-05 (late) — G4.0b proven in the real app; G4.2 cp2–cp4 faces in

**Now:** one Release build of `765d2e6` passed **all seven journeys, 349/349** (SS7 78 incl. the
keyboard-only Step 11 and the compressor face), worst launch 1,379 ms, paint p95 4.00 ms, and the full
local suite 380/380. G4.0b's real-app proof is done; **exact-code CI on `765d2e6` is the open item**
(GitHub was still building it at this update). Dan granted the shared desktop for the whole night.
**Next:** G4.2 cp5 transfer curves (limiter + compressor) → cp6 drag-reorder of insert slots → cp7
presets → G4.3–G4.7 → G5 → G6. [Evidence](docs/evidence/2026-10-05-g40b-g42-drives.json).

**Shipped since cp1 (each its own commit, local suite green each time):**
- **G4.2 cp2** compressor/limiter face: a live gain-reduction meter fed by taps the engine harvests at
  create (one acquire-load; RT-safe per critic). Peak hold ~1.5 s; honest "not running" without a graph.
- **G4.2 cp3** delay face and **cp4** reverb face, each drawn from a private copy of the real node fed a
  click (EQ face's law). The reverb gate checks the DSP against its settings: 1 s decay falls 60 dB in
  960 ms, 0.5 s in 480 ms, 100 ms pre-delay starts at 120 ms. Window caps keep an edit's work ~3 ms (B1).
- Parameter rows read human names ("Threshold", "Time L", "Decay") instead of stable ids.
- **Product fix:** screen-reader adoption is edge-triggered (a level-triggered poll dragged Tab back).
- **Harness:** `WaitPopup` waits on the app's modal popup menus then 200 ms (JUCE's popup mouse tracker
  undoes a key sent before its first tick — proven from the source); native choosers re-establish
  verified focus while a dialog is still building; every FAIL prints the app's state (`[ctx]`).
- **CI:** the docs-only fast path now diffs the whole pushed range; it had reported three code commits
  green in 7 s under a docs head. Two Linux/macOS `-Wsign-compare` reds were fixed (`a608efc`).

**Raw failures on the way** are listed with causes and repairs in the evidence index; all have a
supported cause except one ss5 Scissors click (no `[ctx]` existed yet; not reproduced in two batches).

## 2026-10-05 — G4.0b cp1: the keyboard Control target (code + gates; drive pending)

**Now:** G4.0b's code and headless gate are in; the real-app proof (the new ss7 Step 11) and exact-code
CI are pending. **Next:** run ss7 on the built app in a hands-off desktop window, record it, then G4.2 cp2.

**What a user gets:** Tab / Shift+Tab walk every visible, enabled control in reading order (header, rail,
arrange, inspector, dock), with a purple ring on the target. Enter clicks a button, or starts a choice,
value or text interaction. Arrows adjust only that interaction: a chooser previews and Enter applies it;
a slider or the painted mixer fader moves live (fader by 1 dB, Shift 0.1 dB) as one undo step. Esc
restores the starting value; a second Esc ends navigation. Space is transport throughout. An open FX
editor, keymap editor or undo history keeps Tab inside it; closing it returns the earlier target. A
mouse press ends navigation. A screen reader moving onto a control makes it the target.

**How:** pure rules in `src/ui/ControlTarget.h` (priority table, reading order, traversal, stale-target
recovery, dB step); the shell half in `src/ui/MainComponentControls.cpp`, which performs every effect
through the control's own path (click, `onChange`, the slider's drag bracket, the painted fader's drag
verb). The router runs first in `keyPressed`; the probe gains `controlTarget` and each strip's
`linearGain`; ring tokens live in `UiTheme::Layout`.

**Gates:** six `[control-navigation]` cases (336 assertions): the priority table; a full Tab lap with
unique ids; Space while navigating; Enter on Play making exactly a mouse click's dispatches without
returning to zero; Esc dispatching nothing; chooser preview/apply/restore; a fader stepping +3.1 dB as
one undo step while Right/Left/Up do not leak (negative control: Right locates before Enter); the EQ
panel scope and Close restoring the target; text entry on Enter; a hidden control moving the target.
Full local suite 380/380 on the final code. ss7 Step 11 drives the same gestures with real keys.

**Critic:** no blocking finding. Taken: a cached target widget so the per-tick screen-reader poll does
not walk the tree, an O(n) reorder, and the focus-loop invariant documented. Rejected after checking the
code: an "unpaired" gesture end (the gesture always opens inside `onFaderDragged`; the flag is
idempotent) and ScrollBar/ListBox stops (Viewport scroll bars are `ScrollBar`s, already skipped).
A self-review negative control also caught a test that could not bite (Right is unbound in Mixer focus);
the test now holds Arrange focus and proves Right is live before the interaction.

**Deviation log:** choosers preview and apply on Enter, so stepping past an audio device never reopens it.
FX-parameter keys keep the mouse drag's undo granularity (that verb does not coalesce). The painted pan,
sends, M/S cells and insert slots are not targets yet; G6.3 audits whole-shell coverage. A text field is
announced on Tab, not given accessibility focus, because JUCE would also hand it the keyboard. Headless
gates reach screen-reader adoption through the harness, since no window means no native handlers.
The 2026-10-05 waveform observation is closed: the stem is a constant −19 dBFS signal, drawn correctly.

## 2026-10-05 — G4.0a certified: all seven journeys pass on one build

**Now:** G4.0a is **certified**. **Next: G4.0b** (keyboard Control target, ADR-0049), then G4.2–G4.7 →
G5 → G6 → Usable-song certification. Dan asked the agent to keep delivering autonomously and granted
one hands-off shared-desktop window for this batch; that window is now released.

**Result:** on DESKTOP-7LC339R, one Release build of `76ffbac` was driven serially, with no concurrent
build. Its code is byte-identical to `30bc84c`, whose
[CI 34315558059](https://github.com/DanielKinsner/yes-daw/actions/runs/34315558059) passed all ten jobs
with no timing exception. The unchanged driver passed **SS1 45, SS2 33, SS3 55, SS4 19, SS5 70, SS6 49,
SS7 40: 311/311 assertions**. All eleven launches met B6 (worst **1,404 ms** of 3,000). B2 paint p95 was
**4.05 ms** (limit 8), B3 had zero callback removals, B4 had no placement-edit rebuilds and B5 had zero
burst underruns. The same build passed the full local suite, **380/380** (107 s). App SHA-256
`88F896E6…62A6`; the [certification index](docs/evidence/2026-10-05-g40a-certification.json) lists
every log and number.

**Refuted on the way (kept as a raw result):** setting the chooser filename by `WM_SETTEXT`, so that
keyboard focus could not drop typed characters, does **not** reach Windows' own filename model
reliably. SS1 Save As read back the exact path but saved the dialog default `Documents\Untitled.yesdaw`
(44/45), while SS2's identical New/Import choosers accepted it (33/33). The experiment was reverted
before the certifying batch. Its stray bundle was moved out of Documents into the agent scratchpad.
The original 2026-09-08 SS7 blank readback stays unexplained and was not reproduced (SS7 40/40). The
structured readback added then remains in place to capture any recurrence.

**Critic:** a separate agent verified every count, budget, hash, diff and CI claim against the raw
logs and `gh`; it asked for the local-suite row (added). Its suggestion to credit `30bc84c` with fixing
the old readback was rejected: that failure occurred on that same candidate, so no cause is claimed.

**Rubric:** no new defect in the reviewed screenshots; rows 1–4 FIX (G6) stay open. Parked observation
to verify: song-fixture clips draw a near-flat waveform at default zoom (quiet passage vs paint bug).

## 2026-09-08 — G4.0a: remove duplicate saved-song loading, assert every startup

**Now:** G4.0a remains open. Reviewed repairs are pushed as
`30bc84c6f2a8add75da18f0fbd18276aa3237cdf`.
[CI 34315558059](https://github.com/DanielKinsner/yes-daw/actions/runs/34315558059) completed
**success, all ten expected jobs**, on that exact code SHA. macOS passed **378/378**, including
`YesDawTimelineGpuCheck` (2.80 s test duration, not a frame measurement). **No timing exception was
applied.** This certifies those CI results, not the incomplete G4.0a native journey. Dan authorized the shared
desktop for the night; input access is available, and no app drive is currently running. The
corrective `9060aeb` app passed all seven journeys' 300 assertions, but the
critic found an unasserted **3,002 ms** saved SS3 launch. Three diagnostic launches of that same
saved song then measured **3,010 / 3,079 / 2,944 ms**, with the new common B6 gate correctly failing
the first two. These are raw failures, not a tooling exception or certification reruns.

**Current repair:** the shell decoded a validated bundle, then the model reopened it, rehashed its
69 MB audio asset and copied the decoded audio. A move-only prepared bundle now carries the same
validated DB/snapshot and owned audio into the existing adoption path. Full integrity checks,
failed-load preservation, empty-project playback and recovery remain required. Every drive
`Launch` now asserts the unchanged **3,000 ms** B6 budget; existing SS1 assertions stay intact.
This implements the accepted G4.0a startup contract, with local tests and the same real-song
journeys as its gates. Release build and CTest passed **380/380** (198.39 s), with separate
correctness/adversarial/testing critics finding no actionable issue. Same-song diagnostic launches
improved to **2,837 / 2,717 / 2,700 ms**. The final repair's exact-code CI result is recorded above.

**Earlier verification interruption:** the same repaired app passed SS2–SS7, but SS1 reported one
ASUS input discontinuity during the edit burst (B5); startup and other budgets passed. Direct
WASAPI capture outside YES DAW/JUCE independently measured three post-start discontinuities from
the default ASUS virtual microphone in 30 seconds. That proves an environment contribution, not
the cause of every extra JUCE counter increment. Its raw FAIL remains recorded.

Realtek Stereo Mix was available but disabled. The agent enabled it through Windows Sound and
verified a 30-second direct capture with zero post-start discontinuities. A temporary Stereo Mix
default-input profile then passed SS1 B5 with **zero** underruns and B3 with **zero** callback
removals, but failed B2 at **11.45 ms p95** against the unchanged **8 ms** budget. The batch stopped.
An unchanged-script trace reproduced **11.75 ms**: full edit redraws were slow, and correctly
remained in the rolling 256-paint sample during subsequent 1–2 ms playback paints. Attribution
measured only ~1.7 ms inside canvas drawing; pinned JUCE's native offscreen context performs a
full GPU-to-CPU image readback after that drawing finishes.

**Paint repair:** Windows now keeps the same static canvas in an explicit software image cache,
preserving logical invalidation, transparency, physical scaling and the separate playhead layer.
Other platforms retain their existing cache. The unchanged traced SS1 passed **45/45**, with
**7.65 ms** B2 p95 (0.35 ms margin), zero B5 underruns and **2,606 / 2,808 ms** startup. Independent
cache critic and three-size visual comparison found no new defect. The final Release candidate
passed **380/380** local tests (192.05 s). Its untraced batch passed **SS1 45, SS2 33, SS3 55,
SS4 19, SS5 70 and SS6 49** assertions. **SS7 failed** at the native New filename readback,
before confirmation or project creation. The original log conflated unavailable readback with
empty text and sampled focus later; its cause remains **unknown**, so the batch is incomplete.
All eleven startup measurements were within 3,000 ms (worst **2,905 ms**); SS1 measured **7.39 ms**
paint p95 against 8 ms, with zero underruns and callback removals.

**Chooser investigation:** a traced SS7 completed all 40 assertions, and 20 same-process New/type/
cancel attempts delivered their paths correctly. These are diagnostic non-reproductions, not a
repair or certification. A fresh-process comparison delivered eight exact paths, then stopped
because its diagnostic Escape cleanup did not close the chooser; that separate failure is retained.
One further fresh-process diagnostic reproduced the Escape cleanup failure with an exact filename,
stable focus and all modifiers released; it stayed open through another 1,135 ms of read-only
observation. That is separate from the original filename refusal. Structured readback now captures
the resolved Edit HWND, success, character count, elapsed time,
last-error and text together. The exact gate-time focus/readback is reported, and tests distinguish
empty text, unavailable readback and focus loss while refusing confirmation for every failure.
The 500 ms readback timeout, all input timing, path equality and downstream assertions are unchanged.
Separate critic review covers these diagnostics. A final, single SS6 → SS7 predecessor diagnostic
used separate harness processes, unchanged tracked scripts and a bounded passive WinEvent observer.
SS6 passed **49** and SS7 **40** assertions; the observer recorded **4,494 / 787** events, zero drops,
and successful registration/unhooking of all 12 hooks. This is another non-reproduction, not an
explanation. The independent critic found **no evidence-supported corrective hypothesis** in the
trace. Per §8.2, stop repeating the same approach and keep dependent work blocked. The original
failure cannot be attributed retrospectively from its insufficient log. Its next useful evidence
is an actual refusal captured by the structured readback/observer, or a controlled environment
comparison that reproduces it; do not retype blindly, widen timeouts or rerun for a lucky PASS.
No G4.0a certification or later feature advancement is claimed.

**Host setup restored:** the agent restored ASUS AI Noise-Canceling Microphone in all three capture
roles and Stereo Mix's original disabled state through Windows Sound. Read-only endpoint queries
verified all six capture/render roles match their original IDs and Stereo Mix is state 2 (disabled).
Render remains Realtek Speakers. No app or observer remains running. Both profiles, raw failures,
and restoration measurements are indexed in `docs/evidence/2026-09-08-g40a-single-open.json` and
`docs/evidence/2026-09-08-g40a-predecessor-diagnostic.json`.

**Evidence:** `build-ci/g40a-corrective-*.log` and
`build-ci/session-shots/2026-09-08-g40a-corrective/` retain the 300-assertion batch, including all
**eleven** startup measurements. The earlier handoff incorrectly said twelve; its JSON contains
eleven. `build-ci/g40a-saved-baseline-timing.log` retains the three diagnostic results. These
artifacts are local only. No new visual regression was found; existing G6 rubric FIX rows remain.

**Hardware preflight (not final package proof):** Realtek at 48 kHz granted **480 frames** on all
three WASAPI modes, exceeding the locked **128-frame** playback limit. The measurement-generated
result is FAIL (`playback_block_exceeds_target`); `build-ci/g40a-hardware-preflight/20260908-205959/`
contains raw JSON/logs and checker identity. No hardware PASS is claimed. Software work can
continue; Usable-song certification still requires a passing device/machine and final package proof.

**Blocked handoff:** code, local tests, independent review and exact-code CI are complete for this
repair; the original native filename refusal remains unknown. Investigation exhausted the current
supported hypotheses, so dependent implementation stops under §8.2. Preserve the failing log and
capture any actual recurrence with the committed structured readback before choosing another
correction. A controlled environment comparison needs a stated hypothesis and verified setup.
Only after restored required journeys may work proceed to G4.0b, remaining G4.2–G4.7, G5, G6 and
Usable-song certification. Those later implementations remain unstarted. No owner code/visual
review is needed. Logs, screenshots, observer sources and local fixtures named in the evidence
indexes are local-only; git transfers the code, indexes and CI links, not those files.

## 2026-09-08 — G4.0a handoff: code CI green, corrective native rerun awaits access

**Now:** the authorized **Usable-song milestone is unfinished**. Current code is
`9060aeb0407e2cec0ddfe2a5de1cb0031d8758b7`. [CI 34297336345](https://github.com/DanielKinsner/yes-daw/actions/runs/34297336345)
completed **success, all ten jobs** on that exact SHA. Corrective Release build and local CTest
passed **380/380** (194.10 s). macOS passed all 378 tests, including the previously failing Save As
cases and `YesDawTimelineGpuCheck` (2.61 s test duration, not a frame measurement). **No macOS timing
exception was applied.** Separate corrective critic review completed with no blocking findings.

**Concrete blocker:** the previous shared-desktop hands-off window was released after its native
batch. A new window was requested for the macOS correction's required real-app rerun; no answer has
arrived. No further mouse/keyboard input was sent. **G4.0a is not yet certified:** the prior code's
seven scripts passed 300 assertions, but those results do not certify the changed corrective code.
Build/headless checks and exact-code CI were completed while native input access remained pending.

**Next when the window is available:** run `tools/session-drive.ps1` serially with each existing
`tools/session-scripts/ss1-*.ps1` through `ss7-*.ps1`, with distinct output folders and no concurrent
build. Preserve SS1's fresh and saved-fixture 3,000 ms assertions. Fix any failure from evidence;
then record the same-build results, update STATUS, commit/push and verify the applicable CI.
Only then proceed **G4.0b keyboard control navigation → remaining G4.2–G4.7 → G5 → G6 → Usable-song
certification**. None of those later implementations has started.

The current local app SHA-256 is `AA73848280971F15B187C3AD75C9F243862E8DC95636288ED06093550CA8C916`.
[Corrective evidence index](docs/evidence/2026-09-08-g40a-correction.json) binds CI and local logs to
this code; the [earlier drive index](docs/evidence/2026-09-08-g40a-recovery.json) retains all eleven
startup measurements (worst 2,994 ms, only 6 ms margin). Logs/screenshots under `build-ci/g40a-*`,
`build-ci/session-shots/2026-09-08-g40a-*` and critic receipts under `build-ci/ce-code-review/` are
local only. Existing G6 rubric FIX rows below remain open; no full visual/milestone proof is claimed.

## 2026-09-08 — G4.0a corrective checkpoint: macOS Save As portability

**Now:** G4.0a remains open; no G4.0b implementation. Code `3beeda21db172a9864913721d44eae4b9ea2f51e`
was pushed after the local evidence below. [CI 34295697790](https://github.com/DanielKinsner/yes-daw/actions/runs/34295697790)
found three macOS Save As failures in `YesDawUiInputCheck`. This is a product regression, not the
macOS timing exception: `YesDawTimelineGpuCheck` actually **passed** (2.76 s test duration; successful
CTest output does not publish its sustained-frame number). Other finished lanes passed; Windows
is still running at this update.

**Cause and correction:** libc++ `equivalent()` returns `not_supported` when either path is absent,
whereas the new containment guard expected missing-path errors. Every valid new Save As path was
therefore refused on macOS before destination creation. Identity comparison now runs only for two
existing paths; canonical lexical containment, occupied-target refusal and real error handling remain.
The separate critic verified the correction against [LLVM's implementation](https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-18.1.8/libcxx/src/filesystem/operations.cpp)
and the three failing tests. Their assertions remain intact, with refusal diagnostics added.
Corrective Release build and **380/380 local tests** passed (194.10 s); completed corrective CE
review found no blocking issues (`build-ci/ce-code-review/g40a-libcpp-review/review.json`).
Corrective exact-code CI and the new real-app batch are pending. No exception is applied.

**Next:** push the reviewed correction for platform verification, complete the required real-app
batch when input access is available, and wait for all jobs on the corrective SHA. This safe CI
verification proceeds while native evidence remains explicitly pending; it does not certify the
checkpoint. The previous shared-desktop batch was released; a new hands-off
window has been requested, with safe build/headless work continuing meanwhile. Keep G4.0b and later
features unstarted until G4.0a is verified. Raw macOS log: `build-ci/g40a-ci-macos-job.log`; corrective
local artifacts use `build-ci/g40a-libcpp-*`.

## 2026-09-08 — G4.0a recovery: local gates green, CI pending

**Now:** implementing the authorized Usable-song milestone, beginning with restored SS-1–SS-3.
G4.0a is not certified; G4.0b and later features have not started. Main was clean and safely
fast-forward checked at `2f48962`; no pre-existing work was changed.

**Confirmed causes and repairs under verification:**
- Empty launch was missing app behavior. Native fresh sessions now get a real backed Untitled
  project; first Save names it, canceled/failed naming preserves the working session, and reopen
  retains its content. Injected test shells keep their existing unloaded default.
- Missed early chords were a verified harness/environment focus failure: Explorer retained
  foreground while the drive assumed the app had it. Exact foreground verification plus a guarded
  app-caption click with temporary app-only elevation restored the normal New/Import path (6/6).
  The original topmost state is restored; no other window or global setting is changed.
- Chooser targeting assumed legacy control IDs and a fuzzy YES DAW title, which cannot identify
  Import WAV Audio. The harness now targets the observed filename-control ancestry, verifies
  native focus/path readback and exact chooser closure, and checks SendInput acceptance.
- Startup baseline SS-1 measured 5,137 ms against the unchanged
  3,000 ms budget. Instrumentation measured about 3,895 ms in audio initialization, versus about
  143 ms in enumeration. A guarded Windows shared-audio path avoids JUCE's duplicate endpoint
  capability construction only after proving its exact default pair shares a supported mix rate;
  missing proof or failed opening retains the existing full search/output-only fallback.
  The first integrated SS-1 reached 3,433 ms (still FAIL). Finer instrumentation then measured
  1,331 ms creating the native window: native-titlebar/resizability changes recreated its peer.
  Configuring styles before one peer creation reduced that stage to 490 ms and first probe to
  2,643 ms (bounded diagnostic PASS). The full SS-1 subsequently passed 42 assertions but its
  unasserted saved-fixture reopen measured 3,042 ms: still over B6. SS-1 now also asserts that
  reopen budget. Removing redundant native device scans reduced final fresh/reopen results to
  2,550 / 2,947 ms. SS-2 and SS-3 reopen measured 2,994 / 2,945 ms. All are below the unchanged
  3,000 ms limit, with a narrow worst-case 6 ms margin on this machine; no broad timing guarantee.
  Input/output chooser lists now share one backend scan and refresh after failed audio opening too.
- Critic review found a touched Save As data-loss risk: recursive destination deletion accepted
  occupied/related paths. The repair refuses them before mutation, preserves dirty state, and
  attempts to restore the original DB if the copied destination cannot reopen.

**Evidence so far:** original SS-1 30 passed/12 failed; SS-2 2/3; SS-3 1/2. Empty-startup red was
observed before implementation; first lifecycle-focused green was 160 assertions in 6 cases.
Both new Save As safety cases failed before their repair. Harness headless negative controls pass.
Integrated SS-1 passed 41/42 (only B6 failed); SS-2 passed 31/31 and SS-3 passed 53/53.
Full Release build passed. First full CTest passed 379/380; its sole failing native-startup test
expected no project, contrary to G4.0a. It now asserts the real single empty track; a separate
injected-shell test retains the old unloaded assertions. The corrected UI gate passed.
Critic-requested tests exercise optimized audio failure/default/output-only recovery and a real
SQLite reopen failure after Save As copy, proving the original still accepts edits and Save.
After the final enumeration fix, all seven real-app scripts
passed: SS1 **43**, SS2 **31**, SS3 **53**, instrument **18**, piano roll **69**, beat **47**, mixer
**39** (300 assertions total). These include real New/Import, naming, playback/editing, unchanged
database bytes after close/reopen and byte-identical MIDI-song render after reopen.
Final full CTest passed **380/380** in 199.88 seconds. Exact-code CI remains pending.
The first integrated audio-helper compile exposed a Windows `small` macro collision; native SDK
headers were moved into a separate implementation file. No failed result is covered by the macOS
exception, and no timing threshold or existing assertion has been relaxed.

**Next:** commit/push and wait for every expected exact-code CI job.
Only then advance to G4.0b. The user authorized one hands-off window for this test batch.
The pre-existing density test proves seven complete lanes at 1080p, below the plan's eight;
G6 layout certification must resolve this rather than claiming small-fixture screenshots prove it.
**Rubric:** rows 1–4 remain FIX for existing truncated Scale/fade/master labels, unlabeled gain
slider, font tokens below 11 px and missing eight-track proof (G6.1/G6.2/G6.3; master G4.7).
Rows 5–7 PASS within the observed fixture states: distinct selection/playhead/loop/hover, reference
shell structure, and actual project data. No full visual or Usable-song certification is claimed.
Separate critic reviews found no blocking source findings after fixes; requested recovery tests
passed in the UI gate. External Claude review was unavailable (missing jq), so only the completed
local adversarial review receives credit. Review receipts are local under
`build-ci/ce-code-review/g40a-20260908/`.
All current logs/screenshots are local in `build-ci/g40a-*` and
`build-ci/session-shots/2026-09-08-g40a-*`; generators/scripts are checked in, artifacts are not yet
portable certification evidence.
The checked-in [measurement index](docs/evidence/2026-09-08-g40a-recovery.json) records the final
app/fixture/log hashes and all eleven measured startup times. It does not imply those local logs
or screenshots are available on another machine.

## 2026-09-08 — Plan revised for autonomous delivery

**Now:** Dan accepted the targeted plan revision and asked to keep human involvement minimal.
[ADR-0049](docs/adr/0049-autonomous-delivery-and-usable-song-milestone.md) records that decision.
This checkpoint changes documentation only; no new app behavior, hardware proof or workflow PASS
is claimed. Document consistency/review and docs-only CI are the checks for this revision.

**Revision checks:** separate agent reviews completed. Fixed the Enter activation/Return-to-zero
priority conflict and labeled the recording alignment/result-emission harness as future work.
The local document check passes: 14 documentation files, 90 local links, LF/UTF-8, byte-identical
AGENTS/CLAUDE, preserved existing ADR text and unchanged hardware result history. The pushed
revision's CI uses the documentation classifier; it runs no new app or hardware tests.

**Current implementation milestone:** **Usable-song** — a packaged create/import → edit/MIDI →
built-in mix → save/reopen → export/recovery path, with automated journeys, feel/rubric evidence
and the required real hardware-playback result. It is not alpha. Execution order is G4.0a/G4.0b →
G4.2–G4.7 → G5 → G6 → Usable-song close; separate G4.8/H18 plugins then G7 recording and G8 alpha.
The full DAW scope and existing engine contracts are retained.

**Next, in small commits:**
1. **G4.0a — Restore SS-1–SS-3.** Diagnose New/Import, empty launch and startup failures from the
   actual app/harness; preserve assertions and budgets. Unknown cause stays blocking. Restore
   these earlier journeys before more FX faces; earlier "keep this parked" instructions are superseded.
2. **G4.0b — Prove keyboard control navigation.** The Command router owns a Control target separate
   from Focus context. Keyboard operation and global transport must coexist, with one dispatch per key.
3. Resume G4.2 cp2 compressor meter, remaining faces, slot behavior and presets, then G4.3–G4.7.

**Operating rule:** on a build/continue mandate, agents review, repair, test, commit/push and advance
within the current named milestone without routine checkpoint approval. Stop at its finish line
unless a broader mandate exists. Three failed corrective attempts trigger a separate agent critic,
not a request for Dan to debug. Product regressions and required harness repairs take priority;
unrelated scope remains parked. The exact macOS GPU-only timing exception remains narrowly recorded
in the active plan; it does not excuse the missing SS-1–SS-3 proof.

**Human dependencies only:** a consequential decision outside accepted contracts, inaccessible
equipment/credentials/input surface, or actions outside authorization such as spending/publication
or destructive user-data changes. Agents judge the visual rubric and run available self-asserting
hardware commands; only scripts generate result rows. First verify a separate input session for
unattended app drives; otherwise use an already-authorized hands-off window on the shared desktop.
Older per-launch permission and owner-only operator wording yields to ADR-0049; this is not a
blanket permission to take over Dan's mouse. No isolated-input setup is claimed present yet.

**Handoff boundary:** this request authorizes revising and syncing the plan, not starting feature
implementation, a scheduled automation or a new goal. The code remains `93eea07` (with its recorded
CI result below); later docs-only green does not re-certify that code. New future scripts are
explicitly marked planned in the session README; the old measurement rows remain unchanged.

## 2026-09-08 — G4.2 cp1: EQ response display

**Now:** G4.2 cp1 implemented, locally verified and pushed as `93eea07`. Exact-head CI is complete:
nine jobs green; macOS red only on the parked GPU timing failure. **Next:** G4.2 cp2 — compressor
gain-reduction meter, then the remaining per-kind faces and presets. Stop here; G4.2 and the G4
phase are not closed.

**Remote evidence.** Code SHA `93eea074666ce8740cf98efc64f326e4be9e6d3c`,
[CI run 34282374416](https://github.com/DanielKinsner/yes-daw/actions/runs/34282374416).
Windows, Linux, both sanitizer jobs, both packaging jobs, both verifier self-tests and classification passed.
macOS passed 377 / 378 tests; only `YesDawTimelineGpuCheck` failed at
`tests/timeline_gpu_tests.cpp:84` (19.4233 ms sustained vs 16.6 ms). This is the parked frame-budget
exception under Dan's 2026-09-04 instruction; no rerun or threshold change. Overall GitHub conclusion
is `failure`, not all-green. This docs-only evidence update does not replace that exact-code run.

**Story / precedent.** Opening an EQ shows its six bands' combined configured response above the
existing editable controls. Named Band 1–6 Type / Frequency / Gain / Q readouts and Bands 1–2 /
3–4 / 5–6 pages replace raw ParamSpec names. The precedent is Logic Pro's Channel EQ display
([Apple's guide](https://support.apple.com/guide/logicpro/channel-eq-overview-lgcef1edce5b/mac)).
This is a response display; the parameter controls remain the editing surface (no spectrum-analyzer claim).

**Implementation.** `EqNode::magnitudeAtFrequency()` evaluates the actual current TPT coefficients;
processing and RT annotations are untouched. `EqResponseComponent` owns a separate message-thread
EQ copy, prepared at the project rate and refreshed on settings changes. It caches a log-frequency
path bounded by visible columns, 20 Hz to min(20 kHz, 0.49 x rate), with a +/-24 dB view. Bypass
shows the flat effective response; unbypass restores the configured curve. No live audio-node state
is read. The EQ frame is 660 x 460 max and can float over the mixer dock to keep all eight rows
reachable at 720p. Splitters stay behind it without raising it over Keymap / Undo History. The probe
exports the parameter widgets and `fxEditor.eqResponseVisible` / `eqResponseDb1000` for real gestures.

**Mechanical evidence.** Full final Release build and `ctest --preset ci`: **379 / 379**, 169.21 s.
`[fx-editors]`: **435 assertions / 2 cases** (edits, undo, bypass, slot resets, non-EQ hiding,
all 24 labeled controls at 720p/1080p/1440p, probe targets, overlay order). `YesDawEqCheck`:
**17,600 assertions / 11 cases**; the three added display cases contribute 382 assertions comparing
all six shapes and a combined normalized chain to FFT/settled-sine renders at 44.1/48/96 kHz,
including DC/Nyquist and invalid-frequency bounds. Red first: missing response API; absent graph;
missing probe slider; clipped 720p rows; Keymap behind EQ. Every corresponding final gate is green.

**See-it.** Dan authorized the hands-off batch this session. Final `ss7-mix-the-song.ps1`:
**38 / 38** on the rebuilt real exe, including a gain drag, bypass/unbypass and the three window
sizes. `ss5-piano-roll`: **68 / 68**; `ss6-write-a-beat`: **46 / 46**; `ss4-track-instrument`:
**17 / 17** on its spaced retry (first attempt missed the lane-menu dispatch, 16 / 17).

**Earlier-drive limits — do not call the whole batch green.** Unchanged ss1/ss2/ss3 still failed
in native New/Import setup, before exercising this EQ. First pass: ss1 21 passed/17 failed,
ss2 0/5, ss3 1/2. One spaced retry: ss1 30/12, ss2 1/4, ss3 0/3. ss1 also retains D3 (no empty
project at launch) and exceeded its 3 s startup budget (4.831 s initially; 7.729 s on the retry
while rebuilding). No assertion or threshold was weakened. The New/Import behavior matches the
already parked native-chooser reliability family; these failures are recorded, not fixed or
claimed to be baseline-proven. The current checkpoint's local suite + ss7 are green; this is not
full-arc session-drive certification. Keep the drive tooling issue parked per the phase rules.

**Visual rubric.** Inspected the actual final EQ at logical 1280x720, 1920x1080 and 2560x1440
against the arrangement reference and the existing tokens. PASS for EQ containment, no clipped
rows/readouts, named controls/tooltips, readable 11.5 px graph/readout text and theme contrast,
visible selected strip / bypass state, the existing shell structure, and honest calculated data.
The three-track ss7 fixture with an intentionally enlarged mixer does not re-certify the global
eight-track density requirement. The only display fixes needed were the row fit, numbered labels
and axis type size; the layout/label regressions are pinned in `[fx-editors]`.

**Review / handoff.** Compound simplification and six-lens code review complete; the overlay-order
finding was fixed and tested. Receipt `20260908-141407-9409b0ef`: no remaining actionable findings.
The attempted Claude peer did not run (missing jq); the local adversarial pass completed. Nonblocking
coverage limit: no UI assertion yet varies project rate, although the engine response is measured
at three rates and the shell passes the project rate explicitly. Per-checkpoint screenshots are local
under `build-ci/session-shots/2026-09-08-g42-certified/ss7/`; earlier drive shots/logs and the review
receipt are local too, not synced by git. Re-run the checked-in script to recreate them. HEADLESS
remains the default for a new session; request a hands-off window before drives.

## 2026-09-01 the Real-DAW arc (G0–G8) — plan written, waiting for Dan's "go"

**What happened.** Dan used the app for real on 2026-09-01 and reported: laggy, things not working
or hard to understand, Space-to-play "worked half the time depending on focus", no visible cut
tools or familiar hotkeys, "obscure details instead of obvious things", UI "just not there". He
asked for a turnaround plan: read what's there, know what a DAW is, make them meet — editing and
MIDI first, recording later, rules that keep agents from wandering without him.

**Reproduced mechanically the same evening** (real exe, injected Win32 input, screenshots): Space
after clicking the timeline did nothing twice; Space after clicking a toolbar button started the
transport. Causes in code: Space is Play-only (Stop is `K`), no widget declines keyboard focus,
every action removes and re-adds the audio callback, whole-window 30 Hz repaint, every non-scalar
edit rebuilds the engine, invented chords, menus without shortcuts, zero context menus. Two shipped
lies found on the way: the Time Stretch action is a trim (node exists, unwired) and the RT clip fade
is linear while the UI law is equal-power.

**Decided and written (this session):**
- [ADR-0046 — the feel-first shell arc](docs/adr/0046-feel-first-shell-arc.md): reference-DAW
  rule, no invented chords, focus contexts, command router, everything reachable by mouse, nothing
  blind, feel budgets as gates, selection model, density from the reference, **Session drive** as a
  new mechanical gate class on the real exe, agent visual judgment at every UI checkpoint, editing
  and MIDI before recording, anti-wander (no audit carves; parking lot).
- [The Real-DAW plan (G0–G8)](docs/plans/2026-09-01-real-daw-ground-up-plan.md): verdict, laws,
  target wireframes, keymap v2 decision table (Logic first, Pro Tools second), the code moves
  (carve `MainComponent`, layered rendering, three engine edit lanes), phases with per-item
  story/precedent/gate/see-it, Session scripts SS-1…SS-7, feel budgets B1–B6, the visual rubric,
  process rules, where every R-item went, risks.
- `CONTEXT.md` gained the arc's vocabulary; `docs/goals/parking-lot.md` opened; the roadmap points
  at the plan for everything shell-side.

**The 2026-08-25 reality-run backlog is closed as a list.** R1–R17 are certified (below); R18–R34
are mapped into phases by the plan §9. Do not work R-items from that document any more.

**Now:** **G3.7 — MIDI file import / export** (plan §6 G3; this commit). `src/interchange/Smf.h` reads and
writes Standard MIDI Files (formats 0 / 1, running status, the tempo / meter / name metas) and bridges
them to the edit model in quarter notes; File > Import MIDI File and a `.mid` dropped on a lane land the
file's tracks as SampleLocked MIDI clips at the project's tempo (the first on the target lane, the rest on
new tracks named from the file with the synth on); File > Export MIDI File writes the selection (else every
MIDI clip) as a format-1 file at 960 PPQ. Gates `[smf]` (`YesDawSmfCheck`: byte golden, round-trip, the
tolerant reader, the bridge) and `[midi-file]` in `YesDawUiInputCheck`; ss5 Step 15 authored (the real
chooser by path). ss5 63 / 63 (twice), rubric PASS with one FIX applied; the G3.7 docs-evidence commit
waits for CI on `6c4473e`. **G3.8 — MIDI FX reachable + Arpeggiator + Chord: cp1 (this commit) — the engine
half**: `FxKind` gains `MidiTranspose` / `MidiScaleMap` / `MidiArpeggiator` / `MidiChord`; a Track's
MIDI-kind inserts sit on the MIDI path between its merged clips and its instrument in chain order; the
Arpeggiator (rate / order / octaves / gate) and Chord Trigger (three intervals / velocity) nodes; every
MIDI FX addressed by ParamSpec like the audio inserts; Track-only (`MidiFxNeedsTrack`); the bundle takes
kinds 5–8 in the same column. `YesDawMidiFxCheck` `[midi-fx]` (render goldens by equivalence); cp1
`317211b`. **cp2 (this commit) — the shell**: the strip's Add FX chooser and the slot menu list the four
MIDI FX (a Bus refuses by name on the status line); the slot names them; the param rows render their
choice specs as real choosers (E15's law, unchanged); the roll header's Key / Scale choosers set the
project's key / scale (schema v30, one row), the in-scale rows lift off the grid and the pencil lands on
the nearest in-scale key; the live lane posts a MIDI FX param to the running node through the new
`MidiEffectNode` contract. `[midi-fx-shell]`; ss5 68 / 68; cp2 `3e56c7d` (its push carried the G3.7
evidence commit on top, so CI classified the head docs-only and ran nothing on cp2's code — the G3.9
cp1 head below is the first full run over it; a finding for the parking lot). **G3.9 — Sampler
instrument: ADR-0048 (Proposed) + cp1 (this commit) — the engine half**: `TrackInstrumentKind::Sampler`;
pads as Track rows referencing Project Assets (`SamplerPad`, schema v31 `sampler_pads` with foreign
keys); `SamplerNode` (one-shot + pitched, sixteen voices, ADSR + gain as ParamSpecs); the projection's
`assetSamplesProvider` seam hands a pad its Asset's samples through the G0.5 ownership law; the pad
verbs (set / clear, undoable as Track row edits). `YesDawSamplerCheck` `[sampler]` (render goldens by
equivalence); cp1 `0fe4fad`. **cp2 (this commit) — the shell**: Sampler in both kind choosers; the
instrument panel's pad grid (two rows of eight, C2–D#3) — a click loads a WAV through the chooser seam,
a WAV dropped on a cell loads it, Shift+click toggles one-shot / pitched, Ctrl+click clears, every pad
edit one undo step; the drum-mode roll names a pad's key; `[sampler-shell]`; ss4 15 / 15; cp2 `2998374`.
**G3.10 — RT-safe MIDI input and thru (this commit; the last G3 item)**: `MidiInputQueue` — the lock-free
device→engine lane the model owns and every live engine drains at block top (no message-thread hop),
its thru target the selected Track's Instrument (an atomic the control thread keeps current); the
shell's MIDI callback posts straight into it; the header's MIDI lamp; `YesDawMidiInputCheck`
`[midi-input]` (the one-block latency law, the target law, the SPSC stress; on the RTSan leg) and
`[midi-input]` in the shell; ss4 17 / 17; G3.10 `25fc379`. **G3 close-out (this commit):** the exit
drive **SS-4 "Write a beat and a chord progression"** (`tools/session-scripts/ss6-write-a-beat.ps1`)
is **46 / 46** on the real exe; the montage is `docs/evidence/2026-09-05-g3.png`; the parking-lot
promotion is decided (one item into G4.1); the G3.7 export law is amended (found by SS-4: the roll's
clip no longer stands in for "everything" — selection-else-all, Logic's law). **Next:** the G3.10
evidence commit once CI is green on `25fc379`, then **G4 — The mixer and routing**, starting at
**G4.1 — Mixer dock v2** (plan §6; the exit is SS-5 "Mix the song"). Dan's word (2026-09-05): run G3
to its end and stop when confident — G3 is closed here (ADR-0048 Accepted `b28b2a9`). **G4 — The mixer
and routing (Dan's go 2026-09-05: "pickup at g4"). G4.1 — Mixer dock v2, cp1 (this commit): the strip's
anatomy** — an INPUT slot and an OUTPUT slot on the strip (Logic's I/O rows: input above the inserts, output
below the sends; a click opens the choice), the R cell beside S / M on Track strips (record arm, lit while
armed), narrow / wide strips (View > Narrow Strips; the strip menu; persisted with the view state), the
strip menu per strip kind (Track / Bus / master — three lists, the Add Insert / Add Send / Output / Input
submenus real), the promoted parking-lot item (a Bus or the master is offered the five audio FX only —
`fxKindsForStrip`), and the tools lane's dead readout rows deleted (plan §8.2 "delete before you add").
`[mixer-v2]` in `YesDawUiInputCheck`; ss7 21 / 21; cp1 `36bb3f5` + the Clang / GCC fix `7547e41` — run
`33984907368` green on nine jobs (macOS red = the parked GPU frame-budget flake; the `36bb3f5` run also
hit that flake on WINDOWS and an sccache outage on Alpha-verify Linux — both noise, both parking-lot).
G4.1 cp1 ✅, **cp2 ✅ (the tools lane folded into the strip: empty-slot / send-well clicks are the add
menus, a slot's double-click the FX editor, the send row's menu, Shift-fine + Touch rides on the painted
drags, the lane column and 79 widgets deleted; `[mixer-v2-fold]`; ss7 30 / 30, ss6 46 / 46; cp2 `51184b9` + the Clang / GCC fix `7b126af`, run
`34002327473` green on nine jobs; `tools/session-scripts/README.md` opened).** **Plan §5.1 carve cp1 ✅**
(the ten helper classes out of `MainComponent.cpp` into their own headers, 18 000 → 13 305 lines;
`1e80e52`, run `34004677311`) **and cp2 ✅** (the shell class declared in `MainComponentShell.h`, its
304 member bodies in seven `MainComponent*.cpp` by domain, none over 2 600 lines; `[shell-topology]`
re-pinned; behaviour unchanged; `53fe6d3`, run `34005526151`). The handover blocker is cleared: no shell file is over 2 600 lines
and the declaration is the map. **Next:** G4.2 (FX editors: per-kind faces in the cp2 frame, bypass /
reorder / remove exist, presets). Rules in force: HEADLESS by default (drives on Dan's go — given
2026-09-05 for this PC), the macOS GPU frame-budget red is noise, the local suite + drives are the working
gate; drive scripts launched back-to-back flake at Step 0 (pause between scripts).
G3.2 ✅ — piano roll dock v2: cp1 (`bdf0c36` + `e770d23`), cp2 (`c5be1f8`, audition via the live
note lane), the UI checkpoint (`71efc63`, `f20be6a`, `fabb781`); the head run `33672370057` is green on
all ten jobs (the intermediate run `33668863266` failed only on the macOS GPU frame-budget flake,
twice — parking lot; never weakened). The G3.2 drive is 31 / 31 on the real exe; the §7.4 rubric is
below.
**G3.2 follow-up (2026-09-04, found by Dan's hand-test on the office PC):** the timeline tool strip was
paint-only — it always lit the Pointer and no click reached it (the keys 1–6 worked; every drive picked
tools by key, so the buttons were never exercised). Fixed as one checkpoint: the strip is seven cells in
key order (1 Pointer · 2 Pencil · 3 Scissors · 4 Eraser · 5 Velocity · 6 Zoom) plus the mouse-only Hand;
a click dispatches the same select action the key does; the active tool's cell is lit; the hover hint
names the tool; the probe publishes `tool.<name>` cells. Pins: `[tool-strip]` in `YesDawUiInputCheck`
(a click on each cell selects that tool; the paint lights the active cell) and ss5 Step 10 (drive 38/38).
**The same-family sweep (2026-09-04, Dan's "a bunch of wins like that"):** an agent swept for painted-not-hit-tested
controls, key-only verbs and blind hover zones; the top claims were verified by reading the missing handlers.
Shipped, one commit each, every one with a `YesDawUiInputCheck` pin: the rail's **O record-arm badge** clicks
(`973a227`); the **header gear** toggles the settings row and lights (`bd234f5`); the painted **"SNAP: Bar"**
mockup field is gone (`4290d24`); **Transport ▸ Locate Points** submenu reaches the ten store / recall verbs
(`606b0cd`); a **mixer track strip double-click renames** inline (`c3ae41c`); the **piano roll keyboard column**
names itself on hover (`15acdfb`). Structural: the probe now names `rail.row.N.{mute,solo,arm}`,
`header.gear`, `mixer.strip.N.{solo,mute,fader,insert.K,send.K}`, and ss2 Step 8 clicks them on the real exe
(ss2 29/29). Also `7b25ebd`: the H12 shell test builds against a temp session dir (Dan's hand-made
Untitled.yesdaw had turned it red locally). **Sweep closed (2026-09-04, later):** the mixer's painted fader THUMB and pan knob on an unselected strip
drag that strip (`b55c656`, `[strip-fader]`); a tempo / meter change label click locates exactly on the
change so the ruler menu's verbs act on it (`[map-label]`); the ruler time-format switch was ALREADY in
the ruler menu ("Time Display" submenu, G2.2) — the sweep's claim was wrong, verified, no change. Still
decorative by design until G4.4: the mixer SC badge. **Follow-up to reproduce under a debugger:** an
earlier draft of the strip-fader fix hit-tested the whole rail; a plain strip click on a BUS strip then
segfaulted inside the shipped bus-strip test (`bus strips select and edit like real strips`). The thumb
target sidesteps that path and a bus thumb drag is pinned green, but the rail-click crash itself is
unexplained — parked, not fixed. Every macOS red in this batch was the GPU frame-budget flake (rerun). **Dan's call (2026-09-04): stop spending
time on that flake** — it is parked; treat a macOS red on `YesDawTimelineGpuCheck` alone as noise, no reruns,
the local suite + drives are the working gate until the runner floor is measured.
**Waveform lie fixed (2026-09-04, Dan's screenshot: identical waveforms after a split).** The canvas `Clip` had
no source window; `drawClipCachedWaveform` set its sample rate as `sourceFrames / clip.lengthSeconds` — the
WHOLE file squeezed into every clip, so every split, trim, slip and stretch painted the same waveform since
G2. Now `Clip` carries `sourceStartFrame` / `sourceFrameCount` (engine `srcOffset` / `srcLen`; 0 count =
whole source keeps every golden byte-identical), the painter maps clip time into that window (stretch and
reverse honoured inside it). Pins: `[source-window]` in `YesDawWaveformCacheCheck` (silent-half / loud-half
source) and `YesDawUiInputCheck` (a real split's halves carry adjacent windows that add up). Related, still
open: the 64× zoom cap (`timelineZoomMax`) is ~1–3 ms per pixel, no sample-level view — not in the plan;
and split / trim snap to the grid by default (Ctrl inverts, Snap Off exists) — by design, matches the
reference DAWs.
**Workflow rule (Dan, 2026-09-04): the agent works HEADLESS by default on Dan's PC.** The build, ctest and the
in-memory UI harness need no screen. The Session drives inject real input into the real window and fought
Dan's mouse and focus twice today — so **no drive, no app launch, no screenshot without asking Dan first**
and getting a yes ("run drives", "go ahead"). Batch the drives into one hands-off window per checkpoint.
Per-machine setup the drives need (not in git, not in the agent's memory on another PC): a `build-ci`
Release build via a vcvars64 wrapper; the song fixture once —
`build-ci\YesDawMakeSongFixture.exe --out "%LOCALAPPDATA%\YES DAW\fixtures"` (without it the 0.09 s sine
fixture makes ss3's split-while-playing fail every time); the app closed before a drive or a relink
(single-instance; LNK1104). The drive harness is Windows-only: the MacBook runs the headless suite only.
**G2.19 — zoom to sample level (Dan's call, 2026-09-04; "64× is hella restrictive"). Now.** The cap
`timelineZoomMax = 64` (× fit) is ~1–3 ms per pixel. Decision (recommended, Dan silent = accepted; say
"millisecond floor" to change): the ceiling is ONE SAMPLE PER PIXEL, with a floor on the visible window
(never less than the width in samples); past the peak cache's finest tier (256 frames / peak) the painter
reads decoded audio directly for the visible window only. Gates: the zoom slider / Ctrl+wheel reach the
ceiling and clamp there; a headless paint at the ceiling shows the actual wave shape (a synthetic ramp
paints monotonic columns); the frame-budget gate stays green (paint cost is bounded by the window width,
never the file); saved zooms above the old cap round-trip. **Done (2026-09-04):** `timelineZoomCeiling()` =
max(64, rate / fit px-per-second) replaces every `timelineZoomMax` clamp (wheel, tool, slider range, fit-loop,
zoom-to-selection, restore); the canvas `Clip` state carries a `waveformSampleLookup` (the decoded asset the
playback reads) and below tier 0 (256 frames / px) `drawClipCachedWaveform` projects columns from the samples
(`computeWaveformColumnsFromSamples`, one whole pixel per single-sample column); `TimelineCanvasPaintStats
.sampleWaveformClips` counts it. Pins: `[sample-zoom]` in `YesDawWaveformCacheCheck` (a ramp at one sample
per pixel: column n is sample n, the painted tops climb monotonically, the cache path differs) and in
`YesDawUiInputCheck` (Ctrl+wheel climbs past 64x and clamps at the ceiling; ceiling x fit px/s == the
project rate; Ctrl+0 returns to 1x; a 0.09 s project honestly stays at the 64 floor). Frame-budget gate
untouched (no sample lookup in the headless checks).
**Done:** G3.10 ✅ — RT-safe MIDI input and thru: `25fc379`; run `33952079484` green on nine jobs (the RTSan
leg included), macOS red only on the parked GPU frame-budget flake (`YesDawTimelineGpuCheck` alone). ss4
17 / 17; rubric PASS. **G3 closed** on SS-4 46 / 46 (`ss6-write-a-beat.ps1`), the montage
`docs/evidence/2026-09-05-g3.png`, the rubric and the promotion decision (the G3 close-out section).
G3.9 ✅ — Sampler instrument (ADR-0048 Accepted, Dan 2026-09-05): cp1 `0fe4fad` (run `33950273738` green on nine
jobs, macOS red only on the parked GPU frame-budget flake — `YesDawTimelineGpuCheck` alone), cp2 `2998374`;
certified by exact-head run `33951039513` (green on all ten jobs). ss4 15 / 15 (then 17 / 17 with G3.10's
step) on the real exe; rubric PASS.
G3.8 ✅ — MIDI FX reachable + Arpeggiator + Chord: cp1 `317211b` (run `33948586824` green on all ten jobs),
cp2 `3e56c7d` (its own push ran the docs-only fast path — the parking-lot CI finding — so cp2's code is
certified by the next code heads' full runs: `0fe4fad` on nine jobs and `2998374` on all ten). ss5 68 / 68;
rubric PASS.
G3.7 ✅ — MIDI file import / export: `6c4473e`; certified by exact-head run `33947932021` (green
on all ten jobs). ss5 63 / 63 on the real exe (twice) with the G3.7 Step 15; rubric PASS with one FIX
applied (the audio-only inspector controls under a MIDI clip's rows).
G3.6 ✅ — Step input and musical typing: `234bfcd`; certified by exact-head run `33946774937`
(green on all ten jobs). ss5 59 / 59 on the real exe with the G3.6 Step 14 (the whole roll script, Steps
1–14, in one launch); rubric PASS.
G3.5 ✅ — MIDI clips at arrange level: cp1 `a58391a` (the engine half; run `33944717371` green), cp2
`2f9571b` (the arrangement's split / heal / mute / copy / paste / repeat, the four inspector rows; its
run `33945749306` was red only on the Clang exhaustive-switch class, closed in `234bfcd`, whose run
`33946774937` is green on all ten jobs — the certifying run for cp2's content). ss5 51 / 51 with Step 13.
G3.4 ✅ — Quantize v2: cp1 `d139cb3` (the engine half), cp2 `603faa0` (the quantize panel);
head run `33944162506` green on nine jobs, macOS red only on the parked GPU frame-budget flake
(`YesDawTimelineGpuCheck` alone, 21.7 ms vs 16.6 — Dan's 2026-09-04 rule: noise, no rerun). ss5 48 / 48
on the real exe with the G3.4 Step 12; rubric PASS with the one FIX applied.
G3.3 ✅ — MIDI CC, pitch bend, aftertouch, program change: cp1 `17e3802` (the engine half), cp2
`b0a1c2f` (the control lane), the switch fix `63b06ab`, the drive-window fixes `e9d2c38`; certified by
exact-head run `33942955586` (green on all ten jobs) for `e9d2c38`. Earlier heads were red only on the
parked macOS GPU frame-budget flake (`33939222022`), the GCC / Clang switch warning (`33940220844`) and
a Clang dangling-reference warning in the gate (`33941926652`), each fixed in the next commit. Drives on
the real exe: ss5 43 / 43 (with the G3.3 Step 11), ss1 41 / 42 (D3), ss2 29 / 29, ss3 51 / 51, ss4 11 / 11.
Rubric PASS on every line (the long lane hint truncates in the status line — noted).
G0.1 ✅ — certified: exact-head GitHub Actions run `33587446396` green on all ten jobs for
full SHA `a6a5cf8807874347ada80b8919190cac37a3022c` (first try). Local suite 363/363.
G0.2 ✅ — certified by exact-head run `33589636898` (green on all ten jobs) for full SHA
`ea4dfea9351773eaca732cc79f3cb2996ef4f5a1`; its own commit's run `33588671718` was red on macOS
only (compile hide, D11), green everywhere else.
G0.3 ✅ — certified: exact-head run `33589636898` green on all ten jobs for full SHA
`ea4dfea9351773eaca732cc79f3cb2996ef4f5a1` (first try). Local suite 363/363.
G0.4 ✅ — certified: exact-head run `33592155901` green on all ten jobs for full SHA
`f608d3ddfc6e1b09a35bc9eb7eb19e3c52dafad8` (first try). Local suite 363/363.
G0.5 ✅ — certified by the fix commit's exact-head run `33596503075` (green on all ten jobs, full SHA `23d3e5382fc868c98abe092f59efffbe483f723b`): its own
run `33595208144` was red on Linux/macOS (D22); code `96e9829` + fix `23d3e53`.
G0.6 ✅ — certified by the same fix-commit run; its own run `33595972964` carried the D22 red;
code `24095d8` + fix `23d3e53`. Local suite 364/364 with the fix.
G0.7 ✅ — three checkpoints (`8795c42` header + row law, `8817813` panels + rail row, `4e9932a`
ruler rows); certified by exact-head run `33604946633` (green on all ten jobs) for full SHA
`171363ae4143c29bd6519069c0a4067a5e98976c` (the G1.1 cp1 commit, which carries every G0.7/G0.8 change). Its own
checkpoint runs were red on macOS only (D30 clang capture warning; D31 the inspector
exact-restore compare — did not recur; D32 the lane-budget gate's vacuous Windows pass).
SS-1 see-it steps stay pending the drive (D14).
G0.8 ✅ — `f117c42`; certified by the same run `33604946633` / `171363ae4143c29bd6519069c0a4067a5e98976c`. Its own run
`33604218038` was red on macOS only in the GPU frame-budget gate (18.1 ms sustained vs 16.7 —
runner noise: the same paint passed the run before and the run after; the failed job was rerun).
G1.1 ✅ — keymap v2 + focus contexts (`171363a` infrastructure, `708daed` the §4 remap); certified
by exact-head run `33608447371` (green on all ten jobs) for full SHA
`92a9f5464593995e6c21cf3ea0672a098f5f0c90` (the G1.2 commit, which carries the D41 fix). cp2's own
run was red on Linux/macOS at the compile step (D41).
G1.2 ✅ — `92a9f54`; certified by the same run `33608447371`.
G1.3 ✅ — context menus (`74ca3a2` seven targets, `4aa49b4` the insert slot, `998cd21` the
right-button fix for macOS Ctrl+click); certified by exact-head run `33611170410` (green on all ten
jobs) for full SHA `35814d5b51f1096a19c7f62c498cc0b62f53e555` (the G1.4 cp1 commit). Its own runs: cp1/cp2 red on
macOS only (D44), the fix's run cancelled by the next push (D47).
G1.4 ✅ — toolbar v2 (`35814d5`: nudge value chooser + verbs, the inspector toggle on I, the
two-readout counter); certified by the same run `33611170410`. Deferred inside the item, recorded:
Edit mode / Snap mode choosers (their model is G2), the X / P / A regrouping (G1.7 sweep), nudge
in ms / samples / frames (G2.15). The two defects its rubric shot exposed (D45 menu bar width,
D46 the caption clipped since G0.7) are fixed in `bd1117a`, whose run is watched.
G1.5 ✅ — the keymap editor (`1baa2ed`); certified by exact-head run `33612568972` (green on all
ten jobs, first try).
G1.6 ✅ — status hints + live tooltips (`15ce6e7`); its own run `33614073659` was red on macOS
only (D49); certified by G1.7's exact-head run `33615096877`.
G1.7 ✅ — the dead-affordance sweep (`96c93bd`); certified by exact-head run `33615096877`
(green on all ten jobs, first try). **G1 headless work complete**; SS-2 pending Dan's go (D14).
G2.1 cp1 ✅ — splitters + per-project view state (`98d1a89`); certified by exact-head run
`33616439236` (green on all ten jobs, first try).
G2.1 cp2 + cp3 ✅ — dock tabs (`669d155`), the letter cluster (`3a0ca74`), its paint fixes and
the Linux fix; certified by exact-head run `33625472284` (green on all ten jobs) — G2.1 complete.
G2.2 ✅ — ruler v2 (`9618882`); certified by the same run `33625472284`.
G2.3 ✅ — drag ghosts, landing line, Esc, auto-scroll (`83e8c5e`); G2.4 ✅ — the Smart tool
(`c56e0e9`); certified by run `33627456994` after a rerun of its Windows job (the GPU frame-budget
benchmark's hosted-runner noise, parking lot) — every job green on the same head.
G2.5 ✅ — Time selection first-class (`c6568e5`); G2.6 ✅ — Edit modes (`e25e008`); both certified
by run `33630615703` on `af3c034` (the one-label fix; D51 in G2.7's section) — green on all ten jobs.
G2.7 ✅ — Snap modes (`02ea877`); certified by exact-head run `33632069331` (green on all ten jobs).
G2.8 ✅ — Nudge value in clock units (`463646e`); certified by run `33633084596` after a rerun of its
macOS job (GPU frame-budget noise, parking lot) — every job green on the same head.
G2.9a ✅ — time-stretch engine half, schema v22 (`4e24714`); certified by exact-head run `33634707297`
(green on all ten jobs).
G2.9b ✅ — time-stretch gesture (`15e16e1`); G2.10 ✅ — fades v2, schema v23 (`d0800c6`); certified by
exact-head run `33637897699` on `d0800c6` (green on all ten jobs).
G2.11 ✅ — slip (`080c6b8`); G2.12 ✅ — clip properties, schema v24 (`0c39c05`); certified by run
`33639686386` on `0c39c05` after a rerun of its macOS job (GPU frame-budget noise, parking lot) — every
job green on the same head.
G2.13 ✅ — clip processing, schema v25 (`05472a5`); G2.14 ✅ — markers v2, schema v26 (`dba562f`);
certified by exact-head run `33641728768` on `dba562f` (green on all ten jobs).
G2.15 ✅ — tempo and meter map editing (`8beb692`); G2.16 ✅ — zoom and navigation (`a8f609c`); certified by run `33645105611` on `a8f609c` after a rerun of its macOS job (GPU frame-budget noise, parking lot) — every job green on the same head.
G2.17 ✅ — track headers v2 (`a5b2dbe`); G2.18 ✅ — the undo history window (`6f46a5e`); certified by exact-head run `33649032858` on `2eddd07` (the label fix for GCC/Clang -Wswitch, D51's lesson again — the engine label switch is now covered by the checker) — green on all ten jobs. **G2 headless work complete**; SS-3 pending Dan's go (D14).
G3.2 ✅ — piano roll dock v2: cp1 `bdf0c36`+`e770d23`, cp2 `c5be1f8`, the UI checkpoint `71efc63` /
`f20be6a` / `fabb781` (head run `33672370057` green on all ten jobs); drive ss5 31/31 — 38/38 with the
tool-strip follow-up of 2026-09-04 (see **Now**).
G3.1 ✅ — Track instrument (ADR-0047 Accepted): cp1 `381e8db` (run `33652552912`), cp2 `fa6c67e` (run
`33654084069`), cp3 `4c26a17` (run `33655909259`), the UI checkpoint `dc60b42` (run `33660679423`) —
every run green on all ten jobs; SS-1 41/42 (D3), SS-2 23/23, SS-3 51/51, the G3.1 see-it 11/11 on
the real exe.
**Next:** see **Now** above (the Done list is in order; the plan is the map).


## Older history

Per-checkpoint stories of the Real-DAW arc (G0–G4.1, plan §5.1) and every earlier handoff entry
(H0–H17, the August runs) moved verbatim to [`STATUS-HISTORY.md`](STATUS-HISTORY.md) on 2026-10-05.
