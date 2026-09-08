# YES DAW — the Real-DAW plan (G0–G8)

**Making what we have feel like Logic Pro / Pro Tools, editing and MIDI first.**
Written 2026-09-01 after Dan's first real session ("laggy, half-working, confusing — can you turn
this around?"). Locked by [ADR-0046](../adr/0046-feel-first-shell-arc.md), with execution and ordering
amended by [ADR-0049](../adr/0049-autonomous-delivery-and-usable-song-milestone.md) on 2026-09-08.
Vocabulary in
[`CONTEXT.md`](../../CONTEXT.md) (Arrange window, Editor dock, Focus context, Command router,
Object/Time selection, Edit mode, Smart tool, Nudge value, Snap mode, Session script, Session
drive, State probe, Feel budget, Reference-DAW rule).

> **Current execution contract — 2026-09-08.** Dan selected a targeted plan revision with minimal
> human involvement. Preserve the full DAW, engine and delivered work. Next: restore the earlier
> user journeys, then finish a **Usable-song milestone** before deepening plugin hosting. Agents
> review, test, repair and advance within the authorized milestone without checkpoint approvals.
> This document revision is not a request to launch implementation or a background loop.

### Execution order and finish lines

Historical G/SS identifiers are stable; numeric G order is no longer the execution order at G4.8.

| Order | Work | Exit / advancement |
|---|---|---|
| 1 | G4.0a recovery, G4.0b control navigation | Earlier SS-1–SS-3 pass again; keyboard contract proven before more FX faces |
| 2 | G4 built-in mixer: G4.2–G4.7 (G4.1 already delivered) | Logical SS-5 mixes with built-ins; current and earlier journeys pass |
| 3 | G5 project lifecycle, then G6 polish/accessibility | Logical SS-6, feel budgets and rubric pass; certify the Usable-song milestone below |
| 4 | Separate G4.8 / H18 plugin milestone | Recorded real-VST3 smoke PASS before kickoff; plugin lifecycle journey and kickoff-ADR gates pass |
| 5 | G7 recording | Logical SS-7 plus measured hardware recording proof |
| 6 | G8 alpha distribution | Packaged whole-arc checks and hardware evidence; publication remains a separate authorized action |

**Usable-song milestone:** on the packaged app, create/import a mixed audio/MIDI song, edit,
mix using built-ins, save, close/reopen, export, and recover interrupted work. Pass logical SS-1
through SS-6, all applicable feel budgets, the three-size/scaling rubric, packaged self-check and
the required packaged hardware-playback check. Preserve existing playback timing targets. No
manual listening or owner screenshot review is required. Retain versioned fixtures, exact code
SHA/build identity, script results, exported-audio assertions and raw measurements in the evidence.
This is an earlier product milestone, not an alpha declaration or proof of recording/plugins.

**Missing infrastructure is not success.** Separate-input app automation and hardware evidence
are capabilities to verify, not assumptions. A missing surface blocks the affected certification;
independent preparation may continue within scope, but no dependent phase or milestone closes.

> **Drift rule (read first).** If this plan contradicts code or docs reality, verify reality, log the
> deviation in `STATUS.md`, and follow the plan's *intent* (the laws in §2), never its letter.
> This plan **supersedes** the ordering of `docs/goals/2026-08-25-reality-run-backlog.md` (its open
> items are mapped into phases in §9) and the Phase-3 "dogfood prep" of the 2026-08-20 plan. The
> 2026-08-20 D-table decisions **D2** (the reference image is the visual truth), **D3** (no fake
> data), and **D6** (single-window topology) carry forward unchanged. Its **D9** out-of-scope list is
> retired: shuffle editing, time-stretch, MIDI CC, and comping are in scope here.

---

## 0. Who this is for and how to read it

- **Dan** reads §1 (the verdict), §2 (the laws), §3 (what it will look like), §4 (the keys), and
  the one-line exits in §6. Nothing here needs him to read code.
- **Agents** execute §6 top to bottom under §7 (verification) and §8 (process). §5 tells them how
  the code has to move. §9 tells them where every old item went. §10 lists the risks.
- Accepted decisions govern execution; explicitly named kickoff ADRs remain future work. The only
  reasons to contact Dan are concrete human dependencies in §8.4.

---

## 1. The verdict — what is there, and what a DAW is

### 1.1 What is there (verified in code on 2026-09-01, head `fabf3cc`)

**The engine and model are real and worth keeping.** Roughly 70 validated, undoable edit verbs with
row-diff undo (`src/engine/ProjectUndo.h`); a v21 SQLite bundle that round-trips everything the
model holds; compile-time PDC, a deterministic scheduler, equal-power pan and the stereo balance
law, mute/solo as a post-compile mask; tracks, buses, pre/post sends, submix outputs, master FX;
five built-in FX with full `ParamSpec` ranges; a live parameter lane for gain/pan/mute/FX param
that does not rebuild the engine; sample-accurate MIDI flattening at PPQ 15360; markers, locate
points, persisted loop and punch; a correct offline render; autosave with validated recovery; a
persisted multi-tier waveform peak cache; RTSan-enforced `[[clang::nonblocking]]` on every hot
path. 362 self-asserting gates are green on nine CI jobs.

**The shell fails the first minute of use.** Reproduced with injected input on the real exe:

| What Dan felt | What the code does |
|---|---|
| "Space worked half the time" | `Space` is bound to *Play only*; Stop is `K` (`UiActions.h:402,404`). No widget declines keyboard focus (`MainComponent.cpp:3036` is the only focus call), so a clicked button or combo eats the next key. In the reproduction, Space after clicking the timeline did nothing twice; after clicking a toolbar button it started the transport. |
| "Laggy" | Every action removes and re-adds the audio device callback (`handleAction` → `suspendDesktopAudioCallback`, `MainComponent.cpp:7165-7190`). The whole window repaints at 30 Hz through immediate-mode paint with no dirty regions (`timerCallback`, `:4138`; `kUiRefreshIntervalMs = 33`). Any edit that is not a strip scalar rebuilds the entire playback engine (`rebuildPlaybackForCurrentProject`, `UiAppModel.h:7655`). Export blocks the message thread. |
| "No clickable tools, no typical hotkeys" | Tools are 30 px unlabeled icons; menus paint labels without shortcuts (`getMenuForIndex`, `:7380-7396`); zero right-click menus in the shell (no `isPopupMenu`, no `showMenuAsync`). Default chords are invented: Split `B`, Loop `Ctrl+Alt+Shift+L`, Mixer `Ctrl+Alt+Shift+M`, Snap `Ctrl+1/2/3`, Import `Ctrl+I`, Add Track `Ctrl+T`, views on bare `1/2/3`. |
| "Obscure details instead of obvious things" | Five audit-carved backlogs shipped solo-safe rows, send caps, take provenance — and never a drag preview, a context menu, or a Space toggle. The grading signal (adversarial code audits) finds real defects but never asks "can a person cut a clip". |
| "UI just not there" | Fixed-pixel absolute layout (`UiTheme::Layout::*Bounds()` literals, `resized()` `:4733-4882`), no splitters, no resizable panels, modal view switching instead of a docked mixer, three tracks fill a 1440p screen, a dead island of toolbar in a wide window. |

Two shipped dishonesties found on the way, both of which the plan removes: the **Time Stretch**
action is a plain trim (`UiTimelineEdits.h:215-224`; `TimeStretchNode` exists but is unwired), and
the realtime clip fade is **linear** while the UI law is equal-power (`OfflineRenderer.h:384` vs
`ClipEnvelope.h:26-50`).

Structure: `MainComponent.cpp` is 11,727 lines with a ~1,100-line constructor, an 854-line
`configureMixerControls`, and ~200 member fields; `UiAppModel.h` is 8,709 lines. Six parallel
selection fields exist (clip vector + scalar, MIDI clip, note vector + scalar, mixer target) plus a
seventh (selected lane) living in the view. View state (zoom, scroll, dock, panel) lives on the
component and none of it persists.

### 1.2 What a DAW is (the model we build toward)

A DAW is one **loop** the user runs hundreds of times an hour:

> *hear it → stop where it's wrong → select the thing → do the edit → hear it again.*

Everything in Logic and Pro Tools exists to make one turn of that loop cost as close to zero as
possible. The surfaces are:

- **The Arrange window**: track headers, ruler, clip lanes, a playhead that is always one key
  away, markers and a cycle/loop range, an inspector for the selected thing, and an editor dock
  underneath for the mixer or the piano roll. One window; panels resize; nothing is modal.
- **Selection**: two kinds, always visible — the *objects* you clicked (clips, notes, tracks) and
  the *time range* you swept. Every verb acts on the current one. Undo takes it back, one step
  per gesture.
- **Editing gestures** with a **smart tool**: body moves, edges trim, corners fade, the lower band
  sweeps a time range; the cursor announces the zone before you press; the result paints while you
  drag; snap helps and a modifier defeats it; nudge keys move by a chosen value.
- **Edit modes**: whether neighbours stay put (overlap), get trimmed (no overlap), or close up
  (shuffle).
- **The piano roll**: notes on a grid with a keyboard, velocity and controller lanes, quantize
  with strength and swing, transpose by key, audition on click, step input, and an instrument on
  the track that actually has knobs.
- **The mixer**: one strip per track and bus with inserts, sends, pan, fader, meter, and routing,
  docked below the arrangement, and a master with a loudness readout.
- **Project lifecycle**: import anything, drop it where you point, save, autosave, export with a
  progress bar, stems.
- **Recording**: takes, comping, punch, monitoring — real, but *after* editing and MIDI in this
  plan.

Ten **feel laws** fall out of that, and they are the only taste this plan allows (they are also the
ADR-0046 laws, restated for daily use):

1. Transport is one key away and focus never matters, except in a text field. `Space` toggles.
2. Nothing is blind: every drag previews, every edit is visible within a frame, the playhead
   never stutters because of the UI, audio never hiccups because of the UI.
3. Everything is where you expect: right-click any object for its verbs; menus show keys;
   tooltips name the key; labels on tools.
4. The keyboard follows the industry (Logic first, Pro Tools second). No invented chords.
5. Density like Logic: eight to ten tracks visible at 1080p with the mixer docked.
6. Undo everything, one step per gesture, with a history you can read.
7. Selection is king: objects and time ranges, both visible, both first-class.
8. Snap is helpful, not tyrannical: grid, relative, events; a modifier inverts it; the grid adapts
   to zoom.
9. Editing depth over feature count: stretch, fades with shapes, slip, shuffle, clip colour and
   mute, tempo changes, before any new recording feature.
10. MIDI is co-equal: CC lanes, real quantize, a track instrument with parameters, step input,
    MIDI file in/out.

---

## 2. Rules that keep agents on the rails (summary of ADR-0046)

These are restated here because agents read the plan, not the ADR, at 3 a.m.

- **Reference-DAW rule.** Any UI question: what does Logic do; what does Pro Tools do; write the
  precedent into the item. Windows modifiers `Cmd→Ctrl`, `Option→Alt`.
- **No invented chords.** A default chord needs two reference DAWs (or an obvious variant).
  Otherwise no default; reach it by mouse and the keymap editor. Never a three-modifier default.
- **Focus contexts.** Arrange / Piano roll / Mixer. Same chord may differ per context. Transport
  chords are global.
- **Keys go to the command router.** Widgets never own DAW shortcuts. Active text entry and the
  router-owned Control target have the ordered behavior in G4.0b; Focus context remains separate.
- **Everything reachable by mouse.** Menu with shortcut, context menu on the object, or labeled
  toolbar control with a tooltip. **No dead affordances**: a visible control either works and is
  explained, or it is removed.
- **Nothing is blind; nothing rebuilds needlessly.** Drag previews; no audio-callback teardown by
  UI; graph rebuild only on topology change.
- **Feel budgets are gates** (§7.3). They only tighten.
- **Selection model**: Object selection + Time selection; Edit modes Overlap / No overlap / Shuffle.
- **Density follows the reference** (`docs/design/arrangement-view-reference.png`) with the numbers
  in §3.4.
- **Session drive is a gate class** (§7.2). Runs at every checkpoint on the real exe.
- **Agent visual judgment at every UI checkpoint** (§7.4). Dan's sessions are optional and never
  gating.
- **Editing and MIDI before recording.** G7 opens only after G6 closes.
- **Anti-wander.** Follow the execution-order table within the authorized milestone. Repair
  earlier product regressions and broken required verification immediately; park unrelated
  features/audits. A phase closes on its session script, gates and rubric — never on a tick count.

---

## 3. What it will look like

### 3.1 The Arrange window (target, per the reference image)

```
┌──────────────────────────────────────────────────────────────────────────────────────────────┐
│ File Edit Track Clip MIDI View Transport Options Help                                       │  menu 28
├──────────────────────────────────────────────────────────────────────────────────────────────┤
│ [Pointer][Pencil][Scissors][Glue][Fade][Zoom] │ Snap: Beat ▾  Mode: Grid ▾ │ Edit: Overlap ▾   │
│ Nudge: 1/16 ▾ │ ⏮ ▶ ■ ● ⟲ │ 033|01|000  01:02:45.180 │ 120.00 4/4 │ [I][X][P][A] │ MASTER ▮▮ -7.2 LUFS │  toolbar 60
├───────────────┬──────────────────────────────────────────────────────────────┬───────────────┤
│ TRACKS   [+]  │ 1    5    9    13   17   21   25   29   33   37   41   45    │ CLIP │ TRACK  │
│               │ 0:00      0:10      0:20      0:30      0:40      0:50      │ ─────────────  │
│               │ ▸Intro        ▸Verse          ▸Chorus        ▸Bridge       │ Vocal Lead_03  │
├───────────────┼──────────────────────────────────────────────────────────────┤ Start 33.1.1   │
│ 1 ▮ Drums     │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓            │ End   41.1.1   │
│   M S R  ◐ ▮▮ │                                                              │ Length 8.0.0   │
│ 2 ▮ Bass DI   │    ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓    ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓    │ Gain  +2.4 dB  │
│   M S R  ◐ ▮▮ │                                                              │ Fade in  0.10s │
│ 3 ▮ Vocal     │           ╱▓▓▓▓▓▓▓▓▓▓▓▓▓▓╲  ╱▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓╲        │ Fade out 0.25s │
│   M S R  ◐ ▮▮ │                        ┃ playhead                            │ Shape  S-curve │
│ 4 ▮ Keys ♪    │      ▪▪ ▪▪▪ ▪▪  ▪▪ ▪▪▪ ▪▪  ▪▪ ▪▪▪ ▪▪  ▪▪ ▪▪▪ ▪▪               │ Stretch 100 %  │
│   M S R  ◐ ▮▮ │                                                              │ Colour ▮▮▮▮▮   │
│ 5 ▮ Ambience  │ ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  │ Automation ▾   │
│   M S R  ◐ ▮▮ │  ⋯ automation lane: Volume ──●────●──────●───                 │ Volume ~~~~    │
│ ═══════════════════════════════ splitter (drag) ══════════════════════════════════════════════ │
│ MIXER │ PIANO ROLL │ AUTOMATION                                                      [narrow ▾]│
│ Drums   Bass DI  Vocal    Keys    Ambience  │ Room Verb  Delay  │ Master                        │
│ [ins ]  [ins ]   [EQ   ]  [ins ]  [ins ]    │ [Reverb]  [Delay] │ [Limiter]                     │
│ [ins ]  [ins ]   [Comp ]  [ins ]  [ins ]    │                    │                               │
│ snd ▸   snd ▸    snd ▸ A  snd ▸   snd ▸     │                    │                               │
│  ◐ C     ◐ L12    ◐ C      ◐ R8    ◐ C      │  ◐ C      ◐ C      │  ◐ C                          │
│ M S R   M S R    M S R    M S R   M S R     │ M S       M S      │ M          -7.2 LUFS          │
│ ▮▮ ┃    ▮▮ ┃     ▮▮ ┃     ▮▮ ┃    ▮▮ ┃      │ ▮▮ ┃     ▮▮ ┃      │ ▮▮ ┃                          │
│ -6.2    -8.9     -5.1     -15.6   -18.2     │ -17.1    -14.3     │ -7.2                          │
└───────────────┴──────────────────────────────────────────────────────────────┴───────────────┘
   track headers 260 px  │  lanes fill  │ inspector 300 px (I toggles)  │ dock 300 px default (X / P toggle)
```

What changes versus today: one window with **draggable splitters** (header width, inspector, dock
height); the mixer and piano roll are **dock tabs**, not modal views; the ruler has **two time rows
plus a marker lane**; the toolbar carries **Snap mode, Edit mode, Nudge value** choosers and
**labeled** tools; the inspector has **numeric fields**; the header is a **flex row** (tools left,
transport centre, master meter right), so a 2560-wide window has no dead island.

### 3.2 The piano roll (dock tab)

```
│ PIANO ROLL  Vocal Lead_03 ▾ │ Snap 1/16 ▾ │ Quantize: 1/16 · 80% · swing 12% [Q] │ Scale: C maj ▾ │ [step ⏺] │
│ C4 ┃▬▬▬▬     ▬▬▬▬▬▬▬▬            ▬▬▬▬                                                            │
│ B3 ┃      ▬▬▬▬         ▬▬▬▬▬▬▬▬▬▬       ▬▬▬▬▬▬                                                   │
│ A3 ┃                                              ▬▬▬▬▬▬▬▬▬▬▬▬                                   │
│ G3 ┃  ▬▬▬▬▬▬            ▬▬▬                                                                      │
│    ┃ grid follows snap · black keys shaded · playhead shared with arrangement                    │
├────┼─────────────────────────────────────────────────────────────────────────────────────────────┤
│ Velocity ▾ │ ▌ ▌▌ ▌ ▌▌▌ ▌ ▌ ▌▌ ▌ ▌  (drag bars, Alt = ramp)                                    │
│ CC1 Mod  ▾ │ ╭──╮   ╭─────╮      ╭───╮   (pencil / line / points; CC64 sustain honoured)         │
```

### 3.3 Context menus (one builder, driven by the action registry + what was clicked)

| Right-click on | Items (in this order) |
|---|---|
| **Clip** | Cut · Copy · Paste · Duplicate · Delete ─ Split at Playhead · Split at Selection · Join · Crossfade ─ Mute Clip · Rename… · Colour ▸ · Gain… · Fades… · Stretch… · Reverse · Normalize · Strip Silence… ─ Select All on Track · Select All Following ─ Loop from Selection · Zoom to Selection |
| **Empty lane** | Paste at Pointer · Create MIDI Clip · Import Audio Here… ─ Select All on Track ─ Track ▸ (the header menu) |
| **Track header** | Rename · Colour ▸ · Duplicate Track · Delete Track ─ Add Audio Track · Add MIDI Track · Add Bus ─ Mute · Solo · Solo Safe · Arm ─ Output ▸ · Route to New Bus… ─ Show Automation · Track Height ▸ |
| **Ruler** | Add Marker · Add Tempo Change… · Add Meter Change… ─ Set Loop from Selection · Clear Loop ─ Time display ▸ (Bars, Min:Sec, SMPTE, Samples) |
| **Marker** | Rename… · Colour ▸ · Delete ─ Go to Marker |
| **Note** | Cut · Copy · Paste · Duplicate · Delete ─ Quantize · Transpose ▸ · Velocity… · Length ▸ ─ Select Same Pitch · Select All |
| **Mixer strip** | Rename · Colour ▸ ─ Add Insert ▸ · Add Send ▸ ─ Mute · Solo · Solo Safe · Arm ─ Output ▸ · Width: Narrow / Wide |
| **Insert slot** | Replace ▸ · Bypass · Remove ─ Move Up · Move Down ─ Open Editor |

### 3.4 Density and layout numbers (gated as tokens)

| Token | Value | Why |
|---|---|---|
| Menu bar height | 28 px | Windows convention |
| Toolbar (control bar) height | 60 px | Logic's control bar; today's 118 px header wastes a quarter of a 720p window |
| Ruler | 44 px (bars row 22, time row 22) + 20 px marker lane | Two time rows are standard |
| Track header width | 260 px default, drag 180–400 | Reference ≈ 20 % of 1536; Logic 200–300 |
| Default track height | 72 px; min 24; max 400; zoom-v adjusts | 9 tracks visible in a 1080p window with a 300 px dock |
| Inspector width | 300 px, `I` toggles | Reference; Logic 250–320 |
| Editor dock height | 300 px default, min 160, drag | Reference mixer band |
| Mixer strip width | 84 px narrow / 120 px wide | Logic narrow/wide |
| Base UI font | 12 px; labels 11 px; transport counter 20 px monospace | Reference |
| Minimum operable window | 1280×720 | Everything reachable; dock collapsible |
| Clip minimum grab body | 24 px (exists: `timelineClipEdgeMinGrabWidth`) | R1 |

---

## 4. Keymap v2 (decision table)

Rules: Logic first, Pro Tools second, consensus third. One default chord per action per Focus
context. Everything not in this table has **no default chord**. Old chords that change are listed
so gates can be re-pinned. Contexts: **G** global (works everywhere), **A** Arrange, **P** Piano
roll, **M** Mixer.

### 4.1 Transport (G)

| Action | New default | Precedent | Old |
|---|---|---|---|
| Play / Stop (toggle) | `Space` | Logic, Pro Tools, everyone | Play-only |
| Play from selection start | `Shift+Space` | Cubase/Reaper (play from cursor/selection) | play from last locate |
| Return to zero (go to beginning) | `Enter` outside active text/control navigation (G4.0b owns control activation) | Logic Return; Pro Tools Return | same outside control navigation |
| Go to project start / end | `Home` / `End` | Windows DAWs | Home only |
| Record | `R` | Logic R | same |
| Cycle (loop) on/off | `C` | Logic C | `Ctrl+Alt+Shift+L` |
| Metronome click | `K` | Logic K | `C` |
| Count-in on/off | `Shift+K` | Logic Shift+K | `Ctrl+Alt+Shift+R` |
| Rewind / forward one bar | `,` / `.` | Logic , . | nudge (moves, see below) |
| Previous / next marker | `Alt+,` / `Alt+.` | Logic Option+, Option+. | `Ctrl+Left/Right` |
| Move playhead by grid | `Left` / `Right` (A) | Cubase | same |
| Set loop from selection | `Ctrl+U` | Logic Cmd+U (set locators by selection) | `Shift+L` |
| Shuttle (JKL) | *no default* — View ▸ Transport menu | video convention only | `J/K/L` |
| Return-to-start-on-stop, tempo/meter set | *no default* — click the display | — | three-modifier chords |

### 4.2 Editing (A; also P where noted)

| Action | New default | Precedent | Old |
|---|---|---|---|
| Undo / Redo | `Ctrl+Z` / `Ctrl+Shift+Z` (`Ctrl+Y` alias) | universal | same |
| Cut / Copy / Paste (at playhead) | `Ctrl+X/C/V` | universal | same |
| Duplicate (copy after selection) | `Ctrl+D` | Pro Tools Ctrl+D | same |
| Repeat… (n copies) | `Ctrl+R` | Logic Cmd+R | same |
| Delete | `Delete` / `Backspace` | universal | Del |
| Select all (in focused editor) | `Ctrl+A` | universal | was "on track" |
| Select all on selected tracks | `Ctrl+Shift+A` | Logic-style variant | was "project" |
| Select all following | `Shift+F` | Logic Shift+F | — |
| Split at playhead | `Ctrl+T` | Logic Cmd+T | `B` |
| Split at time-selection edges (all tracks in selection) | `Ctrl+E` | Pro Tools Ctrl+E (Separate) | — (R23) |
| Join / heal | `Ctrl+J` | Logic Cmd+J | same |
| Fades… (default fade in/out; crossfade if two overlap) | `Ctrl+F` | Pro Tools Ctrl+F | `Ctrl+F` + `X` |
| Mute clip / note | `Ctrl+M` | Pro Tools Ctrl+M | — |
| Rename | `F2` | Windows | same |
| Colour… | `Alt+C` | Logic Option+C | — |
| Nudge left / right by Nudge value | `Alt+Left` / `Alt+Right` | Logic Option+arrows | `,` `.` |
| Fine nudge (Nudge value ÷ 10) | `Alt+Shift+Left/Right` | Logic ticks variant | `Shift+,` `.` |
| Clip gain ±1 dB | `Alt+Up` / `Alt+Down` (A) | Pro Tools clip-gain nudge variant | same |
| Transpose ±1 semitone / ±octave | `Alt+Up/Down` / `Alt+Shift+Up/Down` (P) | Logic | `Alt+Shift+Up` |
| Quantize selection (current setting) | `Q` (P; A on MIDI clips) | Logic Q | same |
| Zoom to fit selection (or all) | `Z` | Logic Z | zoom tool |
| Zoom horizontal in / out | `Ctrl+Right` / `Ctrl+Left` | Logic Cmd+arrows | `+` `-` |
| Zoom vertical in / out | `Ctrl+Down` / `Ctrl+Up` | Logic | — |
| Zoom to fit project | `Ctrl+0` | common | same |
| Snap on/off | `Alt+S` | Reaper | — |
| Add marker at playhead | `M` | Reaper M, Cubase Insert | same |
| Tool popup at mouse | `T` | Logic T | — |
| Pointer tool / cancel | `Esc` | Logic, Pro Tools | same |
| Tools 1–6 (Pointer, Pencil, Scissors, Glue, Fade, Zoom) | `1`–`6` | Cubase digits | `V P S H Z` |

### 4.3 Windows and panels (G)

| Action | New default | Precedent | Old |
|---|---|---|---|
| Inspector show/hide | `I` | Logic I | `Ctrl+Alt+I` tabs |
| Mixer dock show/hide | `X` | Logic X | `2`, `Ctrl+Alt+Shift+M` |
| Piano roll dock show/hide | `P` | Logic P | `3` |
| Automation show/hide | `A` | Logic A | same |
| Media browser | `Y` | Logic Y (library) | — |
| Undo history | `Alt+Z` | Logic Option+Z | — |
| Keymap editor | `Alt+K` | Logic Option+K | `Ctrl+/` |
| Playhead follow on/off | `Ctrl+Shift+F` | consensus-lite (menu-discoverable) | `Ctrl+Alt+Shift+F` |

### 4.4 Track and project (G)

| Action | New default | Precedent | Old |
|---|---|---|---|
| Mute / Solo selected tracks | `Shift+M` / `Shift+S` | Pro Tools | same |
| Arm selected track | `Shift+R` | — (kept; discoverable) | same |
| Select previous / next track | `Up` / `Down` | Logic | same |
| Add track… | `Ctrl+Shift+N` | Pro Tools | `Ctrl+T` |
| New / Open / Save / Save As | `Ctrl+N/O/S`, `Ctrl+Shift+S` | universal | same |
| Import audio… | `Ctrl+Shift+I` | Logic Shift+Cmd+I, Pro Tools Shift+Ctrl+I | `Ctrl+I` |
| Export (bounce)… | `Ctrl+B` | Logic Cmd+B | `Ctrl+Shift+E` |
| Duplicate / delete track, bus ops, sends, FX slots, locators, snap presets, take/comp, device ops | *no default* — menus, context menus, inspector | — | assorted Alt/Ctrl+Alt chords |

Removed as defaults (they stay as actions): every `Ctrl+Alt+*` and `Ctrl+Alt+Shift+*` chord in the
current table, the mixer "read" actions (`MixerRead*` are test/agent queries, not user verbs), and
the per-note `Alt+Shift+*` piano-roll chords (replaced by the P-context arrows and drag gestures).

---

## 5. How the code has to move

### 5.1 Shell topology (carve the god component along phase boundaries)

Today `MainComponent` is one 11.7k-line class with no header declaration, ~200 fields, five input
overlay children wired through ~90 `std::function` callbacks, and immediate-mode paint for
everything else. Target (each box is a `.h/.cpp` pair under `src/ui/`, extracted when the phase
that needs it starts — never as a big-bang refactor):

```
AppShell (window, menus, splitters, focus contexts)
├── CommandRouter          keys → action for the focus context; only text fields consume keys  (G0)
├── TransportBar           tools · snap/mode/nudge · transport · counter · panel toggles · master  (G0/G1)
├── ArrangeView                                                                        (G2)
│   ├── RulerBar           bars · time · markers · loop/punch · tempo lane
│   ├── TrackHeaderList    names · colour · M/S/R · I/O · automation · height drag · reorder
│   ├── LaneCanvas         clips · notes preview · automation lanes · ghosts · marquee · playhead layer
│   └── Overlays           drag previews, snap indicator, drop target, status hints
├── Inspector              Clip / Track / Note tabs with numeric fields                (G2)
├── EditorDock             tabbed: MixerView (G4) · PianoRollView (G3) · AutomationView (G4)
├── ContextMenus           one builder from the action registry + hit target            (G1)
├── StatusBar              the R4 status line + gesture hints                           (G1)
└── StateProbe             debug-only JSON writer                                       (G0)
```

Model side (`UiAppModel` stays the model; these are added, then the old fields deleted):

- `SelectionModel` — one object: `objects{clips|notes|tracks}`, `timeRange{start,end,tracks}`,
  `focusContext`. Replaces the six parallel selection fields and the view-owned lane selection.
- `ViewState` — zoom h/v, scroll, snap mode + grid, nudge value, edit mode, tool, dock/inspector
  visibility and sizes, time display mode. Persisted per project (schema bump, `view_state` table)
  and defaults in a prefs file (`%APPDATA%\YES DAW\prefs.json`).
- `EditMode` enum {Overlap, NoOverlap, Shuffle} consulted by every placement verb.
- `Keymap` gains contexts: `actionForChord(chord, context)`; uniqueness gate per context.
- `UiActionContext` sheds its ~30 test-only counters into a `TestCounters` struct the gates own.

### 5.2 Rendering (G0.4)

- Verify the active renderer via the State probe (`ComponentPeer::getCurrentRenderingEngine()`).
  JUCE 8.0.4 ships Direct2D on Windows; if the software renderer is active, select Direct2D.
- Replace whole-window `repaint()` per tick with **layered invalidation**: a static layer (ruler,
  headers, clip bodies, waveforms) cached as images invalidated on model/view change; a dynamic
  layer (playhead, meters, ghosts, marquee) repainted by `repaint(rect)` at the tick rate.
- `refreshActionState()` (393 lines) moves off the 30 Hz tick: event-driven on model change,
  throttled to 10 Hz for meter-dependent state.
- Waveform cache: keep the multi-tier peak cache; add a per-clip, per-zoom rendered tile cache
  keyed by (asset, zoom bucket, height) so zoomed-out dense views do not decimate per frame.

### 5.3 Engine edit lanes (G0.3, G0.5)

- Remove the per-action `suspendDesktopAudioCallback()`/`resume` bracket. The only legitimate
  suspends are device (re)open and sample-rate change. Everything else already rides the atomic
  engine pointer swap and the transport command queue; audit each `handleActionWhileAudioStopped`
  branch and route it through those.
- Three edit lanes, decided by the verb, not by luck:
  1. **Live scalar** (exists): strip gain/pan/mute/solo, FX param.
  2. **Live placement** (new): audio/MIDI clip add/move/trim/split/delete/gain/fade/mute/reverse/
     stretch-factor → publish an immutable `ClipSchedule` snapshot to the source nodes by atomic
     `shared_ptr` swap; MIDI clips re-flatten control-side and swap the event table the same way.
     No graph recompile.
  3. **Topology** (exists): add/remove track, bus, insert, send, route → full rebuild.
- Gate: rebuild counter in the probe is zero for a 100-edit script of lanes 1–2; RT-vs-offline
  render goldens unchanged; undo/redo exact.

### 5.4 Model gaps the phases fill (from the engine map)

| Gap | Phase | Note |
|---|---|---|
| Clip colour, clip mute, reverse flag, stretch factor, fade shape | G2 | schema bump (additive), undo verbs, projection support |
| `TimeStretchNode` unwired; "Time Stretch" is a trim | G2.9 | wire into projection + renderer; remove the action until then (G0.8) |
| RT fade is linear, UI law is equal-power | G2.10 | one evaluator for both |
| Time selection, edit modes, snap modes, nudge value | G2 | model + view state |
| Frame→tick inverse; piecewise bar\|beat; tempo/meter edit verbs | G2.15 | `Time.h` has the forward map; add the inverse and piecewise `computeBarBeat` |
| MIDI CC / pitch bend / aftertouch / program change storage + render | G3.3 | tables, flatten, `SimpleSynth` honours CC64/pitch bend |
| Track instrument (per track, persisted) + `SimpleSynth` `ParamSpec` | G3.1 | ADR for the instrument slot |
| Quantize strength/swing/length/humanize | G3.4 | verb widening |
| Step input; RT-safe MIDI input + thru | G3.6, G3.10 | SPSC MIDI input queue → engine |
| SMF import/export | G3.7 | new `src/interchange/Smf.h` |
| MIDI FX reachable (`FxKind` MIDI variants) + Arpeggiator, Chord | G3.8 | projection |
| Sampler instrument | G3.9 | ADR; one-shot + pitched |
| Sidechain creation verb | G4.4 | node exists |
| Automation Write mode, multi-lane view, region-follow | G4.6 | |
| `FxKind::Plugin` + scanner UI + insert chooser + editor hosting | G4.8 | opens H18; ADR first |
| Any-format import + sample-rate conversion | G5.1 | Preserve original bytes; resample at read per ADR-0010 |
| Threaded export with progress/cancel; stems | G5.3 | |
| Decoded-asset sharing instead of deep copy | G5.4 | |
| Prefs and view-state persistence | G5.6 (prefs), G2.1 (view state) | |

---

## 6. The phases (G0–G8)

Each item is written as: **story** · build · precedent · gate · see-it (the Session-script step
that proves it on the real exe). Follow the execution-order table, then item order within each
phase. Split large items into named committable steps in STATUS before implementation. An item is
done when exact-code-SHA CI is complete under §8.1, required current/earlier Session drives pass,
and (for UI items) the visual rubric is recorded. Named runner exceptions are reported explicitly.

### G0 — Stop the bleeding: the first minute

**Exit:** Session script **SS-1** passes; feel budgets **B1, B3, B4, B6** are gates; visual rubric
recorded at three sizes. *One-line exit for Dan: Space always works, nothing stutters, the app
looks like it has a reason for its layout.*

- **G0.1 — State probe + Session drive tool.** Story: an agent can drive the real app like a user
  and prove what happened. Build: `YESDAW_STATE_PROBE=<file>` makes the shell write a JSON snapshot
  each UI tick (schema in §7.2, includes hit-rects by element id so scripts click by *name*, not
  pixel); `tools/session-drive.ps1` with primitives Launch/Focus/Click/Drag/Key/Type/WaitProbe/
  Shot/Assert/Close; `tools/session-scripts/ss1-first-minute.ps1` authored *now* and red where the
  bugs are. Gate: `YesDawStateProbeCheck` (ctest: probe schema + element ids present);
  session-drive self-test (launches, reads probe, exits 0) runs locally at every checkpoint and as
  a **non-blocking** Windows CI job until it has been stable for one full phase, then blocking.
- **G0.2 — Command router.** Story: Space plays and stops no matter what I clicked last. Build:
  `Space` → `TransportTogglePlayStop`; every `TextButton`/`ComboBox`/`Slider`/`ToolbarActionButton`
  gets `setWantsKeyboardFocus(false)` and `setMouseClickGrabsKeyboardFocus(false)`; a
  `CommandRouter` `KeyListener` on the top-level window handles chords before children; rename
  editors are the only key consumers while visible; `Esc` cancels everything. Precedent: every DAW.
  Gate: `[command-router]` — click each widget class, press Space, assert `isPlaying` toggles;
  press Space while a rename editor is open, assert the editor received it. See-it: SS-1 steps 4–9.
- **G0.3 — Stop tearing down the audio callback.** Story: pressing a key never causes a dropout.
  Build: §5.3 first bullet. Gate: probe `audio.callbackRemovals == 0` after startup across SS-1;
  `[no-callback-teardown]` counts add/remove during 200 dispatched actions; TSan and RTSan legs
  green. See-it: SS-1 step 10 (fifty rapid keypresses while playing; zero underruns, zero
  removals).
- **G0.4 — Rendering budget.** Story: scrolling and playback look smooth at any window size. Build:
  §5.2. Gate: feel budget **B2** measured by the probe on the G0.6 fixture at 1920×1080 and
  2560×1440 (Windows job); `TimelineFrameCheck` unchanged. See-it: SS-1 step 12 (play for 5 s;
  probe `frame.paintMs` p95 ≤ 8 ms).
- **G0.5 — Placement edits don't rebuild the engine.** Story: moving a clip while the song plays
  does not hiccup. Build: §5.3 lane 2. Gate: **B4** (`audio.rebuilds == 0` across a 100-edit
  no-topology script); render goldens; undo exactness property test extended. See-it: SS-1 step
  11 (move, trim, split, undo during playback; `isPlaying` stays true, rebuilds 0, underruns 0).
- **G0.6 — The 16-track three-minute fixture.** Build: a deterministic generator
  (`tests/fixtures/make_song_fixture.cpp` → 16 stereo stems, 180 s, 48 kHz, plus 4 MIDI clips)
  producing a `.yesdaw` bundle used by B2/B4/B5 and every Session script. Never committed as WAV;
  committed as the generator + hash. Gate: hash-stable output.
- **G0.7 — First-minute density.** Story: the app uses my screen. Build: apply §3.4 tokens for
  header (28 + 60), ruler (44 + 20), default track height 72, header width 260; header becomes a
  flex row (tools · transport centred · master meter right); remove the fixed-pixel `Layout::*Bounds`
  literals for the header in favour of a row layout. Gate: token assertions; `[header-flex]` proves
  the master card is right-anchored at 1280 and 2560 and nothing overlaps. See-it: SS-1 step 13
  (screenshots at three sizes; rubric §7.4).
- **G0.8 — Remove the two shipped lies.** Build: hide the Time Stretch action from menus/keys until
  G2.9 wires the node (the verb stays in the registry, disabled with reason "coming in G2");
  remove the **Test Device** and **Refresh** buttons from the shell (developer tools; R24's fake
  provenance can no longer be stamped from the UI — device refresh moves to Options ▸ Audio
  Device…). Gate: re-pin the affected gates; `[no-dead-affordances]` lists every visible control and
  asserts each has a tooltip and an enabled-or-reasoned state.

**SS-1 "First minute"** (the Session drive script; each step asserts through the probe):
1. Launch with no project → an empty project, transport stopped, probe `focusContext == Arrange`.
2. `Ctrl+Shift+I` → import the fixture's first stem to track 1 (file chooser seam).
3. Click an empty lane. 4. `Space` → `isPlaying == true`, playhead advancing. 5. `Space` → stopped.
6. Click the **Play** button → playing. 7. `Space` → stopped. 8. Click the Snap combo, `Esc`,
`Space` → playing. 9. `K` → click toggles; `Space` → stopped; `Enter` → playhead 0.
10. `Space`; fifty `Alt+Right` nudges of a selected clip in 2 s → still playing, `underruns == 0`,
`callbackRemovals == 0`. 11. Drag the clip by name (`clip.<id>`), `Ctrl+T`, `Ctrl+Z` → `rebuilds`
unchanged, playing. 12. Play 5 s → `frame.paintMs` p95 ≤ 8 ms. 13. Screenshots at 1280×720,
1920×1080, 2560×1440. 14. `Ctrl+S` to a temp path; relaunch with that bundle → same clips,
transport stopped at 0, loop restored if set.

### G1 — The command surface: keys, menus, context menus, tools

**Exit:** **SS-2** passes; every daily verb reachable by mouse in ≤ 2 clicks and by its §4 chord;
keymap gate is per-context; no dead affordances remain. *For Dan: right-click works everywhere,
menus show keys, the toolbar has words.*

- **G1.1 — Keymap v2 + focus contexts.** Build: §4 table as the new descriptor defaults; `Keymap`
  contexts (G/A/P/M); per-context uniqueness gate; `chordForKeyPress` handles `Enter`/`Return`,
  numpad, `Ctrl+Y` alias; old→new re-pins for every affected `ui_input_tests` gate (never weakened).
  Gate: `[keymap-v2]` asserts the table verbatim from a generated markdown (the table in §4 is
  produced from the descriptors by a script, so docs and code cannot drift).
- **G1.2 — Menus with shortcuts.** Build: `File · Edit · Track · Clip · MIDI · View · Transport ·
  Options · Help`; `PopupMenu::Item::shortcutKeyDescription` from the keymap for the current
  context; tick states for toggles; disabled items carry the reason as tooltip. Precedent: Logic's
  menu order. Gate: `[menus-show-keys]` — every menu item whose action has a chord paints it.
- **G1.3 — Context menus.** Build: `ContextMenus` builder per §3.3; right-click on clip, empty lane,
  track header, ruler, marker, note, strip, insert slot; the clicked object becomes the selection
  first (Logic behaviour). Gate: `[context-menus]` — right-click each target type, assert item list
  and that the first item dispatches. See-it: SS-2 steps 1–8.
- **G1.4 — Toolbar v2.** Build: labeled tool group with tooltips naming the key; Snap grid + Snap
  mode chooser; Edit mode chooser; Nudge value chooser; panel toggles `I X P A`; transport counter
  shows bars|beats **and** min:sec, click cycles time display. Gate: tokens + `[toolbar-v2]`.
- **G1.5 — Keymap editor (`Alt+K`).** Build: searchable list per context, rebinding with
  conflict detection, persisted to prefs, "Restore defaults"; replaces the overlay that listed
  dead chords (R33). Gate: rebinding round-trips across relaunch.
- **G1.6 — Tooltips and status hints.** Build: every visible control has a tooltip with its chord;
  the status line shows the gesture hint for the hovered zone ("Drag to move · Alt-drag to copy ·
  Ctrl to defeat snap"). Gate: `[no-dead-affordances]` extended.
- **G1.7 — Dead-affordance sweep.** Build: walk every visible control; fix or remove: the `2x`
  combo (what is it?), `Comp` (moves to G7 take lane UI — hidden until then), `Arm`/`Monitor`
  (stay, with tooltips), FX-slot button arrays that paint when no FX exists, inspector controls
  with no selection. Gate: the sweep's list is the gate's list.

**SS-2 "By mouse, then by keys"**: from a fresh launch with the fixture: right-click clip → Split at
Playhead; right-click header → Duplicate Track; right-click ruler → Add Marker; right-click empty
lane → Create MIDI Clip; menu Edit → Undo ×4 (probe counts); then the same four by chord; open
`Alt+K`, rebind Split to `Ctrl+Shift+T`, relaunch, chord works; screenshots.

### G2 — The Arrange window: the editing core

**Exit:** **SS-3 "Edit a song"** (30 steps) passes; **B1–B6** all green; rubric at three sizes
matches the reference structurally. *For Dan: it edits like Logic.*

- **G2.1 — One window, docked panels, splitters.** Build: `AppShell` with `ArrangeView`,
  `Inspector` (I), `EditorDock` (X/P tabs), draggable splitters with min sizes; kill modal view
  switching (`1/2/3` become tools); `ViewState` persisted per project (schema bump) with prefs
  defaults. Precedent: reference image; Logic. Gate: `[dock-layout]` (drag splitter, relaunch,
  sizes restored; 1280×720 still operable).
- **G2.2 — Ruler v2.** Build: bars|beats row, time row (min:sec / SMPTE / samples via context
  menu), marker lane with names, loop brace on the upper row, punch on the lower; click = locate;
  lower-row drag = Time selection (Pro Tools), upper-row drag = loop (Logic cycle). Gate:
  `[ruler-v2]`.
- **G2.3 — Drag previews and auto-scroll.** Build: ghosts for move/trim/fade/gain/copy from
  `dragState` + pointer; snap landing line; edge-band auto-scroll by timer; `Esc` cancels.
  Precedent: every DAW. Gate: mid-drag paint differs in the ghost rect while the model is
  untouched (R18); edge drag scrolls a provable amount (R19).
- **G2.4 — Smart tool.** Build: pointer zones — body move, edges trim, top corners fade, lower
  third Time-select, `Alt` on the right edge = stretch (after G2.9); cursor changes per zone
  before the press; tools 1–6 and `T` popup remain. Precedent: Pro Tools smart tool, Logic
  pointer zones. Gate: `[smart-tool]` asserts zone → mode and cursor for a wide and a narrow clip.
- **G2.5 — Time selection is first-class.** Build: `SelectionModel.timeRange` across tracks;
  visible highlight; verbs: Cut/Copy/Paste-to/Delete/Silence within range, `Ctrl+E` split at
  edges on all selected tracks (R23), `Ctrl+U` loop from selection, `Z` zoom to selection (R22),
  `Shift+F` select all following. Gate: `[time-selection]`.
- **G2.6 — Edit modes.** Build: Overlap (default) / No overlap / Shuffle; toolbar chooser;
  placement verbs consult the mode. Precedent: Logic drag modes; Pro Tools Shuffle. Gate:
  `[edit-modes]` with neighbour assertions in each mode, one undo step each.
- **G2.7 — Snap modes.** Build: Grid (zoom-adaptive: the grid unit halves as you zoom in),
  Relative, Events (clip edges, markers, playhead), Off; `Ctrl` inverts during drag; indicator in
  the toolbar and a snap line during drag. Gate: `[snap-modes]`.
- **G2.8 — Nudge value.** Build: chooser (bar, beat, grid, 1 ms, 10 ms, 1 frame @ 30 fps, 1
  sample); `Alt+arrows` and fine variant; applies to clips, notes, and the time selection. Gate:
  `[nudge-value]`.
- **G2.9 — Time-stretch for real.** Build: clip `stretchFactor` (schema), projection and offline
  renderer host `TimeStretchNode` per stretched clip; `Alt`-drag right edge stretches (Logic);
  inspector numeric; "Stretch to loop length"; the disabled action from G0.8 returns. Gate:
  RT == offline render for a stretched clip; PDC alignment; undo exact.
- **G2.10 — Fades v2 and honest crossfades.** Build: fade shape {Linear, Equal-power, S-curve,
  Log} with a curve amount; **one** evaluator used by `DecodedClipNode` and the UI (fixes the
  linear/equal-power mismatch); draggable fade handles with shape drag; crossfade as a paired
  fade with its own shape; inspector fields. Gate: render golden per shape; `[fade-handles]`.
- **G2.11 — Slip.** Build: `Ctrl+Alt`-drag body slips `srcOffset` with the window fixed, clamped
  by `sourceWindowFits`, snap-aware, undoable (R20). Precedent: Logic slip. Gate: `[slip]`.
- **G2.12 — Clip properties.** Build: per-clip colour (schema; palette per Logic's 72 colours),
  clip mute (`Ctrl+M`, painted dim, silent in render), inline rename on double-click of the name,
  inspector numeric fields (start/end/length/offset/gain/fade in/out/shape/stretch/colour). Gate:
  `[clip-properties]`.
- **G2.13 — Clip processing (non-destructive).** Build: Reverse (flag read by the source node),
  Normalize (computes gain to a target peak/LUFS), Strip Silence… (threshold/min length → split +
  delete verbs in one undo step). Gate: render goldens; undo exact.
- **G2.14 — Markers v2.** Build: inline rename, colours, `Alt+,`/`Alt+.` navigation, marker list
  in the inspector, drag in the marker lane. Gate: `[markers-v2]`.
- **G2.15 — Tempo and meter map editing.** Build: frame→tick inverse; piecewise `computeBarBeat`
  and ruler; tempo lane in the ruler (right-click → Add Tempo Change…, drag to edit, ramp or
  jump); meter changes; MIDI clips follow, audio stays sample-locked (the model already refuses
  tempo-locked audio; that stays honest). Gate: bar|beat readout across a tempo change equals
  the closed form; render goldens with a ramp.
- **G2.16 — Zoom and navigation.** Build: `Ctrl+arrows` zoom h/v at the playhead or selection,
  zoom slider, real scroll bars, `Shift+wheel` horizontal, `Ctrl+wheel` zoom at the pointer,
  playhead follow modes (page / continuous), `Z` toggle, zoom history (`Alt+Z` is undo history,
  so zoom-back is menu-only). Gate: `[zoom-nav]`.
- **G2.17 — Track headers v2.** Build: name inline rename, colour strip, type icon, M/S/R, output
  chooser, automation button, height drag (exists), drag reorder, multi-select, `Up/Down` select,
  per-track colour applies to clips without their own. Gate: `[track-headers-v2]`.
- **G2.18 — Undo history window (`Alt+Z`).** Build: list of verbs with labels; click to jump.
  Precedent: Logic. Gate: `[undo-history]`.

**SS-3 "Edit a song"**: open the fixture; loop the chorus by ruler drag; play; while playing:
split at playhead, drag a clip to another track with snap, `Ctrl`-defeat snap, trim with preview,
fade in with a shape, crossfade two overlapping clips, slip a clip, stretch a clip to the loop,
colour and rename a clip, mute a clip, add markers and jump between them, Shuffle-delete a clip
and see neighbours close, Time-select two bars across three tracks and `Ctrl+E`, nudge by 10 ms,
`Z` to selection, `Ctrl+0`, add a tempo ramp and confirm the bar readout changes, undo twenty
steps, redo twenty steps, save, relaunch, byte-identical project, zoom/dock restored.

### G3 — MIDI and the piano roll to Logic class

**Exit:** **SS-4 "Write a beat and a chord progression"** passes; MIDI CC round-trips and renders;
a track instrument with parameters exists. *For Dan: you can write music in it.*

- **G3.1 — Track instrument.** Build (ADR): a persisted per-track instrument slot (`SimpleSynth`
  now, Sampler in G3.9, Plugin in G4.8) replacing per-clip instantiation; header/inspector
  chooser; `SimpleSynth` gains `ParamSpec` (osc mix, ADSR, filter cutoff/resonance, glide,
  volume), automatable and shown in an instrument panel. Gate: render golden; automation on an
  instrument param.
- **G3.2 — Piano roll dock v2.** Build: keyboard with note names, black-key shading, grid follows
  snap, velocity lane, tools (pointer/pencil/eraser/scissors/velocity), marquee, zoom, follow,
  audition on click through the track instrument, `Left/Right` select adjacent note (Logic),
  `Alt+arrows` transpose/octave, group nudge (R21), double-click empty = add note, drag length,
  `Ctrl+D` duplicate. Gate: `[piano-roll-v2]`.
- **G3.3 — MIDI CC, pitch bend, aftertouch, program change.** Build: storage (schema), persistence,
  flatten, render; `SimpleSynth` honours CC64 sustain, CC1 mod → filter, pitch bend ±2 st; CC lanes
  with pencil/line/point tools; recording of CC parked to G7. Gate: render golden with a sustain
  pedal and a pitch bend; round-trip.
- **G3.4 — Quantize v2.** Build: grid, strength %, swing %, quantize note ends, humanize;
  inspector quantize panel; `Q` applies the current setting; groove parked. Precedent: Logic
  region inspector. Gate: closed-form assertions per parameter.
- **G3.5 — MIDI clips at arrange level.** Build: mini-note preview in clips, `Ctrl+R` repeat,
  transpose in inspector, split/join, velocity offset, mute clip, loop-length aware. Gate:
  `[midi-clip-ops]`.
- **G3.6 — Step input and musical typing.** Build: step-input mode (note length from the toolbar,
  advance by grid), computer-keyboard MIDI (Logic `Cmd+K` musical typing → `Ctrl+K`). Gate:
  `[step-input]`.
- **G3.7 — MIDI file import/export.** Build: SMF 0/1 read/write in `src/interchange/Smf.h`;
  drag `.mid` onto a lane; export selected clips/tracks. Gate: round-trip goldens.
- **G3.8 — MIDI FX reachable + Arpeggiator + Chord.** Build: `FxKind` MIDI variants so the existing
  Transpose/ScaleMap nodes appear in the chain; Arpeggiator (rate, order, octaves) and Chord
  (intervals) nodes; project key/scale with scale assist in the piano roll. Gate: render goldens.
- **G3.9 — Sampler instrument.** Build (ADR): one-shot + pitched sample playback per pad/key,
  ADSR, per-pad file, drag audio in; drum-mode piano roll with pad names. Gate: render golden.
- **G3.10 — RT-safe MIDI input and thru.** Build: device → SPSC queue → engine (no message-thread
  hop); MIDI thru to the selected track's instrument so the synth is playable live; input
  indicator in the header. Gate: RTSan; latency assertion in the harness.

**SS-4**: new project; add a MIDI track (`Ctrl+Shift+N`); pencil an 8-bar drum pattern in drum
mode on the Sampler with the fixture's one-shots; add a Keys track with `SimpleSynth`; draw a
four-chord progression; quantize 80 % with swing; draw a filter sweep in a CC lane; arpeggiate
with the MIDI FX; loop and audition; export the MIDI file; reopen it; render equals golden.

### G4 — The mixer and routing

**Built-in mixer exit:** logical **SS-5 "Mix the song"** passes using built-ins, with all earlier
journeys restored. *For Dan: mix a complete song with EQ, compression, sends and automation.*
G4.8 is a separate later milestone; it does not block G5/G6 or imply that plugins are delivered.

- **G4.0a — Restore the basic journeys (next checkpoint).** Classify the current SS-1–SS-3
  New/Import, empty-launch and startup failures from reproducible evidence. Repair app or harness
  as indicated, preserving all assertions/thresholds. Run serially with no concurrent build/drive
  load when measuring startup. Pin each confirmed product defect with an appropriate negative
  control; repair synchronization/targeting if the harness is at fault. Unknown cause is not a
  tooling exemption. Gate: current SS-1–SS-3 fully pass on the same built app, plus applicable
  local/CI checks. No more editor features until this checkpoint restores the earlier proof.
- **G4.0b — Keyboard operation without losing transport.** Implement ADR-0049's Control target,
  separate from Focus context: Tab/Shift+Tab traverse visible enabled controls in a stable order;
  Tab or accessibility targeting starts control navigation; returning to an editor canvas ends it.
  Enter activates a button or enters/confirms a chooser/value interaction; arrows adjust only the active
  control interaction; Esc cancels it and restores the prior editor context. A visible ring and
  accessibility name/role/value/state identify the target. Priority: active text entry, Space
  transport, Control-target Enter/Esc/Tab navigation, other global transport, active control
  adjustment, then editor-context commands. Enter outside control navigation retains Return to zero.
  One key dispatches once: Enter must not both activate and locate; a slider arrow must not nudge a Clip.
  Hidden/removed controls leave no stale target. All changes use the existing command/undo path.
  Gate: `[control-navigation]` covers transport, EQ, a mixer fader, chooser, text field and panel
  closure, including keyboard-only journeys and cancellation. Extend ss7 with the same real
  keyboard gestures. This is shared interaction behavior; G6 later audits complete coverage.

- **G4.1 — Mixer dock v2** per §3.1: strips with name/colour, input, insert list, sends, pan,
  fader with dB scale, meter with peak-hold and clip, M/S/R, output chooser; bus and master
  strips; narrow/wide; strip context menu. Gate: `[mixer-v2]` geometry + tokens.
- **G4.2 — Insert slots and FX editors.** Build: click = add menu, double-click = editor window
  per built-in (EQ with curve display, compressor with GR meter, delay, reverb, limiter), bypass,
  drag reorder, remove, presets (save/load per FX). Gate: `[fx-editors]`.
  **2026-09-08 cp1 complete:** EQ response display and named band controls, `93eea07`,
  [run 34282374416](https://github.com/DanielKinsner/yes-daw/actions/runs/34282374416): nine jobs
  green; macOS red only on the parked `YesDawTimelineGpuCheck` timing exception. Local suite
  379/379; ss7 38/38. Earlier ss1–ss3 setup failures remain recorded in `STATUS.md`; this does
  not certify the whole session-drive arc. **G4.2 remains open:** compressor meter, remaining
  per-kind faces and presets still to come.
  Remaining committable steps: cp2 compressor meter; cp3 delay face; cp4 reverb face; cp5 limiter
  face; cp6 slot reorder/remove/bypass behavior; cp7 presets save/load including malformed preset
  refusal. Reuse already working behavior and tests; do not rebuild a completed step merely to
  match this list. Every face adopts G4.0b's keyboard contract and grows the same mix journey.
- **G4.3 — Sends and buses.** Build: `+` adds a send with a bus chooser or "New Bus…", pre/post,
  level, destination; "Route to New Bus" from the header. Gate: `[sends-v2]`.
- **G4.4 — Sidechain reachable.** Build: compressor sidechain source chooser (node exists).
  Gate: render golden.
- **G4.5 — Solo/mute UX.** Build: solo-safe in context menus, solo-clear button, `Ctrl`-click
  exclusive solo. Gate: `[solo-ux]`.
- **G4.6 — Automation v2.** Build: stacked per-track lanes, real Write mode, pencil/line tools,
  region-follow toggle (automation moves with clips), instrument parameters as targets. Gate:
  render goldens; `[automation-v2]`.
- **G4.7 — Master strip.** Build: dim/mute, loudness readout in the header (exists as a meter),
  limiter editor. Gate: tokens.
- **G4.8 — Separate plugin milestone (after Usable-song; opens H18).** Entry: a recorded PASS
  from one named/versioned real VST3 through the worker smoke, under ADR-0037. Synthetic PASS or
  setup exit 2 is not entry credit. An agent runs the available smoke with an approved fixture;
  only unavailable access/fixture approval requires Dan. Then write the kickoff ADR before code.
  It must define plugin identity/state persistence, missing/crashed plugin behavior, editor
  lifecycle, latency changes and trust posture. Existing process isolation protects session
  continuity, not user files; do not claim an OS sandbox that has not been built.
  Planned slices: scan/blacklist; insert/process; save/reopen opaque state; editor/parameters;
  crash/hang recovery and removal. The kickoff makes each slice independently testable. Keep
  synthetic isolation gates and add a real-plugin journey: scan, insert, change state, save,
  reopen, render, recover a failed instance, remove. Built-in and earlier journeys stay green.

**SS-5**: route vocals to a new bus; EQ + compressor on it; send to a reverb bus; automate the
bus fader with Write while playing; solo-safe the reverb; export.

### G5 — Project lifecycle, import, export

**Exit:** logical **SS-6 "Project lifecycle"** (`tools/session-scripts/ss8-project-lifecycle.ps1`)
passes, with all earlier logical sessions and the existing feel budgets. *For Dan: import supported
files, finish a mix, reopen it, and recover interrupted work without guessing what survived.*

- **G5.1 — Import formats and cross-rate audio.** JUCE formats: WAV/AIFF/FLAC/OGG, MP3 where
  licensed; drop at the pointer on a lane, multiple files on consecutive tracks, undoable.
  **ADR-0010 remains authoritative:** retain original, content-hashed asset bytes; resample at the
  read boundary, using the live and high-quality offline tiers. Decoded/resampled caches are derived
  data, never replacements for the original asset. Implement format decoding and then cross-rate
  playback/export as separate checkpoints. Gate: per-format decode/import goldens, unchanged source
  hashes, source-window and duration assertions, cross-rate RT/offline results against their declared
  references; invalid/unsupported input leaves the project unchanged and reports its reason.
- **G5.2 — Media browser (`Y`).** File browser with audition, project assets and recent files.
  Reuse G5.1's format and rate policy. Gate: browser selection/audition/import reaches the same asset
  and drop location as the ordinary import path; unavailable files give a visible reason.
- **G5.3 — Export v2, in three checkpoints.** **cp1:** move rendering to a worker with real progress,
  cancel and an immutable export snapshot: project, range/options, and owned decoded-audio lifetimes
  are captured together. No worker reads mutable shell state or borrowed buffers that a project edit
  can invalidate. Completion is posted to the originating job; a stale job cannot update a replacement
  project. A second export is refused with a reason while one is active.
  **cp2:** render/write to a temporary sibling of the destination and commit only on success.
  Cancel before commit removes the temporary output and preserves any existing destination; failure
  does the same and reports its cause. Close or project replacement requests cancellation, completes
  worker shutdown without blocking the message thread, then destroys/replaces project state; teardown
  never detaches a worker that retains shell pointers. Gate: cancel during render/write, destination
  preservation, close/replacement during export, stale completion, and responsive progress/control input.
  **cp3:** WAV 16/24/32, dither for integer output, range options, track/bus stems and normalize.
  Gate: existing bit-exact float reference plus deterministic format/dither, range and stem reference
  comparisons. Freeze each option into the same job snapshot; aborted jobs never report success.
- **G5.4 — Decoded-asset sharing.** One decoded buffer per asset shared by reference (R30), while
  preserving the immutable ownership/lifetime contract established for export. Gate: fixture memory
  assertion and edit/export/project-close lifetime tests; no extra decode per clip referencing an asset.
- **G5.5 — New project, templates and copies.** Sample rate, tempo and template in the new-project
  dialog; Save As and Save a Copy. Gate: creation/copy/reopen retain assets and project state, a copy
  leaves the source usable, and failure preserves the last valid project. Respect G5.3's active-job rule.
- **G5.6 — Persistent preferences.** `prefs.json` owns keymap, view defaults, device and dock defaults
  (R32); reuse existing project view state rather than overwriting it with defaults. Gate: relaunch
  preserves settings, malformed preferences fail safely, and missing devices receive an honest reason.
- **G5.7 — Missing-asset relink.** Add a relink chooser to the existing missing-asset report.
  Validate replacement media against the project's asset/source-window requirements before adopting it;
  cancellation or invalid replacement preserves the project and reports what remains missing.
  Gate: relink, refusal, cancel and save/reopen through the real chooser and bundle validator.

**SS-6 "Project lifecycle" — logical journey; all failure fixtures stay in a scratch project.**
1. Create from a template with an explicit rate/tempo; open the browser and audition a supported file.
2. Drop several formats, including an asset at a different rate, at a chosen lane/time; assert track
   placement, duration and original asset hashes; undo/redo and compare state.
3. Import malformed/unsupported media; assert a specific refusal and unchanged project/asset references.
4. Reopen a scratch copy with a deliberately missing asset; cancel relink, reject an incompatible file,
   then relink successfully and assert the validator and render recover the intended content.
5. Export a selected range and stems with the format options; verify outputs. Cancel a separate export
   mid-job and assert no partial output or overwritten destination; exercise project replacement during
   another active export and assert shutdown/completion belongs to the original job only.
6. Save, make an edit, wait for a confirmed autosave, interrupt the scratch session, recover through the
   shipped recovery path, and assert the recovered project equals the last confirmed autosave.
7. Save As and Save a Copy; close/reopen the copies, verify assets and render, then relaunch and assert
   preferences and per-project view state retain their separate values.

### G6 — Visual identity, keyboard access and polish

**Exit:** all checkpoints below pass; rubric at 100 %, 125 %, 150 %, 200 % Windows scaling and
1280×720, 1920×1080, 2560×1440; every operable control is keyboard-reachable through the command
router and represented accurately in the accessibility tree. Commit the phase montage; Dan does
not have to judge screenshots. Existing logical sessions SS-1–SS-6 and feel budgets remain gates.

- **G6.1 — Tokens and icons.** One colour/type/spacing system and icon set, including readable labels
  and tooltips. Gate: tokens, contrast/size assertions and agent rubric; no fake data or dead affordances.
- **G6.2 — Layout and scaling.** Preserve reference density and the 1280×720 operating floor across
  the stated window/scaling matrix. Gate: geometry, reachable controls and screenshot rubric at every
  combination; each discovered clipping/overlap defect gains a mechanical regression assertion.
- **G6.3 — Keyboard navigation and accessibility.** The command router owns a logical control target
  distinct from Arrange/Piano roll/Mixer focus context. It routes control traversal, activation and
  adjustment under ADR-0049; native buttons/choosers/sliders do not own
  global shortcuts. Only active text editing consumes text keys. Global transport retains its law.
  Expose the target, name, role, value, enabled state and supported actions to accessibility; clear or
  restore the target predictably when a control disappears or an editor closes. Gate: traverse and
  operate every visible control class, assert target/highlight/accessibility state, and verify transport
  before/after control use and text editing. No action becomes mouse-only through this change.
- **G6.4 — Interaction and motion states.** Hover, pressed and logical-target states; playhead and
  meter ballistics from real state. Gate: state transitions and existing frame/action/audio budgets;
  no animation or timer may make static controls lie or interrupt playback.
- **G6.5 — Empty states and first-run tips.** Explain the next available action using its actual
  current chord (for example, import audio); dismiss tips and preserve normal keyboard/mouse use.
  Gate: empty-project and missing-selection paths expose valid actions, with no obscured controls.

### G7 — Recording (after Usable-song and the separate G4.8 milestone)

**Exit:** logical **SS-7 "Record a take"** (`tools/session-scripts/ss9-record-a-take.ps1`) passes,
all earlier logical sessions remain green, and the shipped recording loopback checker records a
hardware PASS. Existing recording alignment, zero-drop, Underrun and timing gates are unchanged.
Recorded audio remains immutable WAV Assets plus Take/Comp metadata under ADR-0036.

- **G7.1 — Honest device provenance and refusal (R24/R34).** Refresh adopts the real device profile;
  deterministic profiles remain harness-only. Failed device switches retain the working device and
  paint the cause. Gate: profile/provenance assertions and no synthetic take from a real-shell path.
- **G7.2 — Continuous capture while operating the app (R25).** Keep harmless edits off callback
  teardown; refuse or queue incompatible actions with a visible reason. Gate: committed frame
  continuity during Save/zoom/edit operations and injected interruptions; never concatenate missing
  blocks into an apparently continuous take or relax zero-drop/timing limits.
- **G7.3 — Visible monitoring policy (R26).** Show the actual Off/DirectInput/LatencyCompensated
  state and input monitoring controls. Gate: label, model and rendered monitoring route agree.
- **G7.4 — Take lanes (R27).** Expand recorded take groups in Arrange; choosing an audible take is
  visible and undoable. Gate: real take geometry, selected-source render and save/reopen parity.
- **G7.5 — Swipe comping and live punch (R28).** Drag arbitrary comp regions across take lanes;
  persist the source windows, one undo step per gesture; use the existing fixed seam-fade default.
  Add punch-on-the-fly under the existing compensated timeline law. Gate: rendered regions come from
  the selected takes; punch boundaries, undo and reopen are exact. No new seam-tuning feature.
- **G7.6 — MIDI-only recording and CC capture (R29).** Record notes and supported G3 controller
  events without an armed audio input, retaining count-in/start/stop and compensation rules.
  Gate: MIDI-only placement, complete CC persistence/render and note/controller save/reopen parity.
- **G7.7 — Shipped-path hardware proof, including the missing evidence harness.** The current
  `tools/shipped-record-check.ps1` forwards its checker's correlation-based PASS/FAIL; it does not
  yet measure alignment or emit a result row. Extend the checker/evidence wrapper to retain the
  known output/capture reference, measure compensated placement under the existing calibrated
  timing contract, validate committed WAV content, and emit a structured artifact plus a result
  row only after all assertions pass. Preserve the existing deterministic alignment/zero-drop
  gates; they do not substitute for measured hardware alignment. Run the completed command through
  the actual shipped recording path with its required loopback route. Missing routing/reference/
  device access is a named setup dependency, not PASS. Ask only for physical setup automation
  cannot make; never ask Dan to judge sound, waveforms or timing. G7 stays open until proof exists.

**SS-7 "Record a take" — seven steps, each with objective assertions.**
1. Open the fixture, select the real input/device, refresh it and assert unchanged truthful provenance;
   exercise a refused switch through the harness and assert the working device and visible reason.
2. Select monitoring policy, arm an audio track and record the known loopback signal with count-in;
   assert transport/capture state and the chosen monitoring route.
3. Save, zoom and perform permitted edits during capture; stop and assert a contiguous committed take,
   unchanged zero-drop/Underrun counters and compensated placement against the known reference.
4. Record another pass with live punch; expand take lanes and choose the audible take; assert the
   recorded/punch windows and selected-source render, then undo/redo the choice.
5. Swipe a comp across the takes, undo/redo, and assert the rendered regions and fixed seam law.
6. Record a MIDI-only pass with notes and CC into the G3 instrument; save/reopen and assert audio Take,
   Comp, MIDI and controller state/render parity.
7. Run the G7.7-completed shipped-path hardware checker and require its generated PASS evidence;
   export the resulting song and
   validate it mechanically. No step substitutes a human ear/eye judgment for an assertion.

### G8 — Alpha distribution

Fold the existing H17 plan (packaging, self-check, installer later, crash reporting). Exit: the
alpha gate in `docs/alpha-gate.md` with logical SS-1…SS-7 plus the separate plugin journey, on the
packaged build. Agents run the scripts, review the rubric and record measured hardware evidence.
An alpha verification result does not itself authorize tags, publication, signing purchases or
announcements. Those actions require their own existing authorization.

---

## 7. Verification protocol v2

### 7.1 ctest gates (unchanged laws)

Build + Catch2 + goldens + RTSan + TSan + soak + the UI input harness stay as they are. New gates
follow the existing patterns (`[bracket]` names, negative controls in the same commit, never
weaken or delete, re-pin to new semantics with the rationale in-comment).

### 7.2 Session drive (new gate class; Windows; real exe)

`tools/session-drive.ps1 -Script tools/session-scripts/ssN-*.ps1 [-Exe <path>] [-Shots <dir>]`

Primitives: `Launch [bundle]`, `Focus`, `Click <elementId|x,y>`, `DoubleClick`, `RightClick`,
`Drag <from> <to> [modifiers]`, `Key "<chord>"`, `Type "<text>"`, `WaitProbe { predicate } [ms]`,
`Shot "<name>"`, `Assert <cond> "<message>"`, `Close`. Coordinates come from the probe's
`layout` map so scripts click by element id.

State probe JSON (written each UI tick when `YESDAW_STATE_PROBE` is set; never in a normal launch):

```json
{ "version": 1, "tick": 1234, "renderer": "Direct2D",
  "transport": { "isPlaying": true, "playheadFrame": 96000, "loop": { "enabled": true, "start": 0, "end": 384000 } },
  "selection": { "clips": ["01J…"], "notes": [], "tracks": [2], "timeRange": null },
  "focusContext": "Arrange", "lastAction": "TimelineClipSplit",
  "view": { "zoom": 0.5, "scrollSec": 12.0, "inspector": true, "dock": "Mixer", "dockHeight": 300 },
  "frame": { "paintMs": 3.1, "tickMs": 0.4, "actionToPaintMs": 6.0 },
  "audio": { "callbackAdds": 1, "callbackRemovals": 0, "rebuilds": 3, "underruns": 0 },
  "layout": { "toolbar.play": [412,40,48,40], "lane.0": [260,152,1700,72], "clip.01J…": [300,152,420,72] } }
```

The drive runs at every code checkpoint on a verified separate input session or an already-authorized
hands-off window (all scripts of the current and earlier phases). CI currently builds the app and
runs headless tests; **a real-app session-drive CI job is planned, not present**. Add it only after
proving the runner has an interactive input surface: start non-blocking, then make it blocking
after a full stable phase. Until then, recorded local/isolated real-app drives are mandatory.
Linux/macOS keep ctest and the headless screenshot gate. Missing input access is pending evidence,
not a silent skip or a substitute headless PASS.

Logical session names are distinct from filenames. The checked-in mapping is in
`tools/session-scripts/README.md`: SS-4 uses `ss6-write-a-beat.ps1`; SS-5 uses
`ss7-mix-the-song.ps1`. Planned SS-6 uses `ss8-project-lifecycle.ps1`, SS-7 uses
`ss9-record-a-take.ps1`, and the separate plugin journey uses `ss10-plugin-lifecycle.ps1`.
These three later scripts are not yet built. Author each before implementing its first feature.

### 7.3 Feel budgets (gates; only tighten)

| # | Budget | Measured by |
|---|---|---|
| B1 | Action → paint ≤ 16 ms | probe `frame.actionToPaintMs` over SS scripts |
| B2 | Paint per frame p95 ≤ 8 ms at 2560×1440, G0.6 fixture, 16 tracks | probe during 5 s playback |
| B3 | Audio-callback removals after startup == 0 | probe `audio.callbackRemovals` |
| B4 | Engine rebuilds in a 100-edit no-topology script == 0 | probe `audio.rebuilds` |
| B5 | Underruns during 60 s of editing while playing == 0 | probe `audio.underruns` (engine stats) |
| B6 | Launch → interactive with the fixture ≤ 3 s | probe first-tick timestamp |

### 7.4 Agent visual judgment (every UI checkpoint)

Screenshot the real shell at 1280×720, 1920×1080, 2560×1440 (and 150 % scaling from G6). Judge
against `docs/design/arrangement-view-reference.png` and §3.4 with this rubric, recording each
line in `STATUS.md` as PASS / FIX (with the item that fixes it):

1. Nothing overlaps, clips, or is cut off; no dead regions wider than 120 px.
2. Track count visible at 1080p ≥ 8 with the dock open.
3. Every control has a label or an unambiguous icon **and** a tooltip.
4. Text ≥ 11 px; contrast readable on the dark theme.
5. Selection, playhead, loop, and hover states are visually distinct.
6. The layout matches the reference's structure (header / headers / lanes / inspector / dock).
7. Nothing in the frame is fake data (D3).

Each FIX becomes a token/layout gate in the same item so it stays fixed. One montage PNG
(≤ 300 KB) per phase close is committed under `docs/evidence/`; per-checkpoint shots stay local
or in CI artifacts.

---

## 8. Process rules (how the loop runs without Dan)

### 8.1 The loop per checkpoint (agents own the routine work)

For a build/continue request, the current named milestone in STATUS is the default authorized
scope unless Dan names a smaller one. A plan-edit request alone starts no build loop. No recurring
automation, new goal, or background execution is inferred from this policy.

1. Read STATUS and current code/CI. Select one coherent step in the execution-order table;
   subdivide oversized items into plainly named steps before coding. Write story, precedent,
   affected contracts and gates. Do not reopen settled product/stack decisions.
2. Author the meaningful negative control and Session-script step before implementing behavior.
   Preserve existing assertions. Docs-only checkpoints use document consistency/link checks.
3. Build and run applicable local checks. For code checkpoints, current and earlier real-app
   journeys must pass; agent judges UI screenshots against §7.4. Do not measure performance while
   another build or drive competes for the machine. A separate agent critic reviews the scoped
   change; the writer verifies findings and repairs confirmed issues.
4. Update STATUS, commit small to main and push. CI requires the pushed commit: wait for every
   expected job on that exact code SHA to finish, inspect any failures, and fix red in small
   corrective commits. Never declare an in-progress run green or use a docs-only successor as
   code evidence. The sole standing runner exception is listed in §8.2; report actual conclusions.
5. Record code SHA, run id, local/drive results, rubric, fixture/build identity and any named
   exception. Keep evidence portable (checked-in generator/script plus hashed artifact/CI location);
   local-only screenshots/logs are explicitly named and are not assumed present on another machine.
6. Complete the checkpoint and continue automatically within scope. A phase boundary is an
   automated evidence check, not an owner-approval request. Report concise progress; stop only at
   the authorized finish line, a real external dependency, or host execution/budget limits.

A failed required gate means incomplete work. Capturing a failure or committing a repair is not
certification. Continue to repair within the mandate; if execution must stop, leave an honest
incomplete handoff. This does not require Dan to review code or judge the app.

### 8.2 Anti-wander rules

- Work within the authorized milestone and execution-order table. A blocked item stays unticked;
  only independent preparation may proceed, with the dependency stated. A missing phase exit
  never silently becomes optional. The explicit G4.8 reordering is not an ad-hoc skip.
- **Restore required behavior and proof immediately.** Earlier product regressions, data-loss
  risks and broken harnesses required by the active milestone take priority over new features.
  G4.0a therefore interrupts the next FX face. Unrelated audits and feature ideas still go to
  `docs/goals/parking-lot.md`; promotion is decided by an agent at phase close only when it serves
  the next approved exit. Broader scope/product changes are decisions, not automatic promotions.
- **Reference-DAW rule** for every UX question; write the precedent in the item. If Logic and
  Pro Tools disagree, prefer Logic for MIDI/arrangement, Pro Tools for audio editing gestures,
  and say which you chose.
- **No dead affordances.** If you ship a control, it works and it is explained. If you cannot
  make it work in this item, remove it from the shell (keep the action registered and disabled
  with a reason).
- **No taste.** Colours, sizes, and spacing come from §3.4 tokens and the reference image. If a
  value is missing, take Logic's, record it in §3.4 via the docs commit.
- **Delete before you add.** Extracting a component from `MainComponent.cpp` means the old code
  is gone in the same commit; no parallel implementations.

**Failure classification (evidence, not labels chosen for convenience):**

| Class | Required response | Can dependent work be certified? |
|---|---|---|
| Product regression / unmet behavior | Reproduce, add a biting check, repair the product and rerun the journey | Only after the required checks pass |
| Confirmed harness or environment failure | Show the cause, repair targeting/synchronization/setup, and rerun the real journey; keep assertions intact | No substitute headless result or unverified baseline earns credit |
| Unknown cause | Keep raw evidence; investigate app, harness and environment | No; failure before the changed feature is not proof of tooling fault |
| Accepted runner noise | Match the exact named exception and record raw outcome and evidence; do not rerun for luck | Only within the written exception's scope; never call the failing job green |
| Missing hardware/input access | Verify available alternatives, record the unavailable capability | No hardware/real-app claim until measured on the required surface |

**Standing exception:** macOS `YesDawTimelineGpuCheck` failing only its sustained-frame budget,
with all other jobs/tests passing, is the owner-accepted runner-floor issue. No reruns, no widened
threshold, no exception for crashes, wrong images or other failing tests. Preserve raw numbers,
CI run and local real-app/frame evidence. Re-evaluate at G6.2 on a measured runner/machine baseline
or earlier if the renderer/test/runner changes or the local check fails; no automatic renewal or
extension to a different failure. Missing SS-1–SS-3 proof is **not** covered by this exception.

**Bound retries, not investigation:** after three unsuccessful corrective attempts on the same
failure, stop repeating that approach and require a separate agent critic. Resume only with a
new evidence-backed hypothesis. If none exists, record a concrete blocker, finish independent
safe preparation and stop dependent work. Ask Dan only if his decision/access can resolve it;
do not hand him a red test as a debugging assignment.

### 8.3 Do-not-touch (absolute)

Existing Accepted ADR text, goldens, `[[clang::nonblocking]]` / `YESDAW_RT_HOT`, `.github/workflows/ci.yml` (except the
additive session-drive job, which is its own commit), the reference image, `docs/reality-lane.md`
existing result rows, engine RT rules. Never weaken or delete a gate (re-pinning with rationale is
expected). New in-scope implementation ADRs already called for by the plan may be authored and
accepted by the agent with critic review if they do not replace an owner-settled decision; an
incompatible choice follows §8.4. Append only genuine measurement-generated hardware results;
never edit a result into a PASS. Never squash.

### 8.4 Human dependencies (rare; prepare a concrete recommendation first)

1. A consequential product/architecture decision outside accepted scope: replacing an owner-settled
   ADR, breaking existing bundles, changing the reference structure or a density token by more than
   20 %, weakening a promised gate, or dropping a required capability. Prepare the proposed ADR or
   exact alternative and its consequences first. This revision is already authorized by ADR-0049.
2. Required access/equipment cannot be supplied by the agent: a physical loopback connection,
   missing approved plugin fixture, credentials/license acceptance, or no verified isolated input
   surface and no existing hands-off window. Check available resources and complete independent
   preparation first; ask once for the smallest missing action. Missing evidence remains pending.
3. An action needs authority not already granted: spending, public release/publishing/announcements,
   or destructive user-data changes. This plan does not grant that authority by implication.

Everything else stays with agents: reversible implementation choices, repairs, code review,
mechanical tests, visual-rubric judgment, evidence accounting and advancement within scope. Dan
may volunteer friction notes; record and address them within the current mandate. No manual
session, screenshot approval, listening test or recurring "continue?" is a gate.

**App/hardware execution:** use a verified separate machine/VM/logon-input session for unattended
input; prove it cannot steal Dan's focus/mouse. A Windows virtual desktop or hidden process alone
does not establish this. On the shared desktop honor the current hands-off authorization/window;
do not ask again per script within that window. Agents may run available one-command hardware
checks and commit their genuine outputs. The measurement script owns PASS/FAIL, not the operator.
Neither headless simulation nor a machine without the required real device earns hardware credit.

### 8.5 Phase close-out (one docs commit)

Agent verifies current/earlier logical journeys, exact-code-SHA CI, feel budgets and rubric;
records any strictly applicable §8.2 exception without changing its raw result; commits montage
in `docs/evidence/<date>-gN.png`; updates STATUS and the roadmap pointer; decides in-scope parking-lot
promotions with reasons. Advance automatically if the next phase is covered by the active mandate.
At the named milestone exit, collect the packaged end-to-end evidence and required hardware result,
report plainly and stop unless a broader existing mandate covers the next milestone. No owner
approval is needed to certify an objectively met exit.

---

## 9. Where every old item went

| Old | New home |
|---|---|
| R18 drag preview, R19 auto-scroll | G2.3 |
| R20 slip | G2.11 |
| R21 piano-roll group nudge | G3.2 |
| R22 zoom to selection | G2.5 / G2.16 |
| R23 select-to-end, razor all tracks | G2.5 (`Shift+F`, `Ctrl+E`) |
| R24 Test Device / Refresh fake provenance | G0.8 removes the buttons; the provenance fix itself → G7 |
| R25 sample drops during recording, R26 monitoring policy, R27 take lanes, R28 comping, R34 device switch reason | G7 |
| R29 MIDI-only recording + CC capture | storage in G3.3; capture in G7 |
| R30 import deep copies | G5.4 |
| R31 export off the message thread | G5.3 |
| R32 prefs persist | G5.6 (view state itself: G2.1) |
| R33 keymap overlay dead chords | G1.5 |
| Parked: third-party plugin insertion | G4.8 |
| Parked: FX presets | G4.2 |
| Parked: sample-rate conversion | G5.1 |
| Parked: relink UI | G5.7 |
| Parked: VCA groups, stereo width, polarity/input trim, pan-law choice, streaming from disk, loop-record cycles > 8 | `docs/goals/parking-lot.md` |
| 2026-08-20 plan Phase 3 (dogfood prep) | replaced by §8.4's optional lane |
| `docs/alpha-gate.md` scripted session | G8 adopts SS-1…SS-7 |

---

## 10. Risks and how the plan handles them

- **G0 is plumbing and Dan sees little.** G0.7 (density) is deliberately in G0 so the first phase
  changes the screenshot; the montage at G0 close is the first visible proof.
- **The session drive is flaky on CI.** It starts non-blocking; local runs are mandatory; it is
  promoted only after a phase of stability. Element-id clicks (not pixels) remove the main
  flake source.
- **Extracting components from an 11.7k-line class breaks hidden coupling.** Extraction happens
  per phase, only for the surface that phase touches, with the "delete before you add" rule; the
  15k-line UI harness is the safety net and its child-count change-detector is bumped
  deliberately, never disabled.
- **Live placement lane (G0.5) is the riskiest engine change.** It rides the existing snapshot
  swap pattern and keeps the goldens; if the schedule swap cannot be made RTSan-clean for MIDI
  in one item, audio ships first and MIDI follows as G0.5b — logged, not skipped.
- **Keymap changes break muscle memory.** Nobody has muscle memory for the old map; the table
  documents old → new and the keymap editor lets anyone restore a chord.
- **Scope pressure to "just add" a feature.** §8.2's parking-lot rule and phase exits are the
  answer; the only path into a phase is through its close-out.
