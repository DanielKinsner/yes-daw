# YES DAW — STATUS (live handoff)

**Read this first on any machine.** This is the single source of truth for *where we are right now*.
The [active plan](docs/plans/2026-09-01-real-daw-ground-up-plan.md) and
[roadmap](docs/goals/roadmap.md) define the work; **this** file's newest entry is the live handoff.
Older entries below are dated history, not competing "Now" instructions.

> **Cross-machine rule:** `git pull` at the start of a session. At the end, update this file, commit in
> small chunks, and `git push`. Then the next machine — or the next session — is never lost.

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

**CI:** pending at writing; the code commits are pushed one at a time.

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
