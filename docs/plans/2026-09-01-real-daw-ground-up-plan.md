# YES DAW — the Real-DAW plan (G0–G8)

**Making what we have feel like Logic Pro / Pro Tools, editing and MIDI first.**
Written 2026-09-01 after Dan's first real session ("laggy, half-working, confusing — can you turn
this around?"). Locked by [ADR-0046](../adr/0046-feel-first-shell-arc.md). Vocabulary in
[`CONTEXT.md`](../../CONTEXT.md) (Arrange window, Editor dock, Focus context, Command router,
Object/Time selection, Edit mode, Smart tool, Nudge value, Snap mode, Session script, Session
drive, State probe, Feel budget, Reference-DAW rule).

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
- Everything in this plan is a decision already made. The only reasons to contact Dan are in §8.4.

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
- **Keys go to the command router.** Widgets never own keys. Only an active text field does.
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
- **Anti-wander.** Current phase only, top to bottom. No new audit carves. Findings go to
  `docs/goals/parking-lot.md`; promotion only at phase close. A phase closes on its session script,
  gates, and rubric — never on a tick count.

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
| Return to zero (go to beginning) | `Enter` | Logic Return; Pro Tools Return | same |
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
| Any-format import + sample-rate conversion | G5.1 | JUCE formats + resampler |
| Threaded export with progress/cancel; stems | G5.3 | |
| Decoded-asset sharing instead of deep copy | G5.4 | |
| Prefs and view-state persistence | G5.6 (prefs), G2.1 (view state) | |

---

## 6. The phases (G0–G8)

Each item is written as: **story** · build · precedent · gate · see-it (the Session-script step
that proves it on the real exe). Items are ordered; work them top to bottom. An item is done when
its gate is green in CI, its see-it step passes in the Session drive, and (for UI items) the visual
rubric is recorded.

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

**Exit:** **SS-5 "Mix the song"** passes. *For Dan: it mixes like Logic's mixer, and plugins load.*

- **G4.1 — Mixer dock v2** per §3.1: strips with name/colour, input, insert list, sends, pan,
  fader with dB scale, meter with peak-hold and clip, M/S/R, output chooser; bus and master
  strips; narrow/wide; strip context menu. Gate: `[mixer-v2]` geometry + tokens.
- **G4.2 — Insert slots and FX editors.** Build: click = add menu, double-click = editor window
  per built-in (EQ with curve display, compressor with GR meter, delay, reverb, limiter), bypass,
  drag reorder, remove, presets (save/load per FX). Gate: `[fx-editors]`.
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
- **G4.8 — Third-party plugins in strips (opens H18).** Build (ADR first): `FxKind::Plugin`,
  scanner UI (Options ▸ Plugins), insert chooser lists scanned VST3, editor window through the
  out-of-process host, blacklist UX. Gate: the synthetic host isolation gate stays; a real-plugin
  smoke stays owner-lane.

**SS-5**: route vocals to a new bus; EQ + compressor on it; send to a reverb bus; automate the
bus fader with Write while playing; solo-safe the reverb; export.

### G5 — Project lifecycle, import, export

**Exit:** **SS-6** passes. *For Dan: drop any file in, get a mix out, never lose work.*

- **G5.1 — Import anything, at the drop point.** Build: JUCE formats (WAV/AIFF/FLAC/OGG; MP3 where
  licensed), sample-rate conversion on import (windowed-sinc, replaces the 44.1 kHz refusal),
  drop onto a lane at the pointer, multi-file drop to consecutive tracks, undoable. Gate: import
  goldens per format.
- **G5.2 — Media browser (`Y`).** Build: file browser with audition, project assets list, recent.
- **G5.3 — Export v2.** Build: worker-thread render with real progress and cancel (R31), WAV
  16/24/32 with dither, range options, stems per track/bus, normalize option. Gate: bit-exact
  goldens; cancel mid-way leaves no partial file.
- **G5.4 — Decoded-asset sharing.** Build: one decoded buffer per asset shared by reference (R30).
  Gate: memory assertion on the fixture.
- **G5.5 — New-project dialog and templates.** Build: sample rate, tempo, template; Save As /
  Save a Copy.
- **G5.6 — Prefs persist.** Build: `prefs.json` (keymap, view defaults, device, dock sizes) (R32).
- **G5.7 — Missing-asset relink.** Build: the R5 report gains a relink chooser.

### G6 — Visual identity and polish

**Exit:** the rubric passes at 100 %, 125 %, 150 %, 200 % Windows scaling and at three window
sizes; every control is keyboard-reachable (a11y tree). Build: token pass (colour, type scale,
spacing), one icon set, hover/pressed/focus states, playhead and meter ballistics, empty states
("Drop audio here or press Ctrl+Shift+I"), first-run tips, min window 1280×720. Gate: tokens,
a11y, screenshot montage committed at phase close.

### G7 — Recording (deferred, then done properly)

Opens only after G6 closes. Items are the parked R24–R29 verbatim plus: take lanes in the
Arrange window, swipe comping, punch-on-the-fly, input monitoring UI, MIDI CC capture, and the
owner hardware PASS. Exit: **SS-7 "Record a take"** passes and the loopback smoke has a PASS row.

### G8 — Alpha distribution

Fold the existing H17 plan (packaging, self-check, installer later, crash reporting). Exit: the
alpha gate in `docs/alpha-gate.md` with SS-1…SS-7 as its scripted session.

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

The drive runs **locally at every checkpoint** (all scripts of the current and earlier phases) and
in CI on the Windows runner as a **non-blocking** job until it has been stable for a whole phase,
then blocking. Linux/macOS keep ctest and the headless screenshot gate.

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

### 8.1 The loop per item (unchanged mechanics, new gates)

1. Audit the code path first; write the story, precedent, and gate in `STATUS.md`.
2. Author the gate (ctest) **and** the see-it step (Session script) red first.
3. Build. Local ctest green (owner-file isolation ritual per the 2026-08-11 brief). Session drive
   green for the current and earlier phases. Screenshots judged (§7.4) for UI items.
4. One feature commit (story · precedent · gate names · SS step · rubric verdict in the message),
   push, exact-head nine-job CI green (session-drive job non-blocking until promoted).
5. Docs-only evidence commit ticking the item here with SHA + run id.
6. Next item. **Never** skip ahead within a phase; never start the next phase before the exit.

### 8.2 Anti-wander rules

- Work only the current phase's items, top to bottom. If an item is blocked, log why in
  `STATUS.md`, leave it unticked, and continue with the next — do not invent a replacement item.
- **No new adversarial audit carves during the arc.** Any finding (yours, a reviewer's, a tool's)
  goes to `docs/goals/parking-lot.md` with file:line. Promotion into a phase happens only at that
  phase's close, and only if it serves the *next* phase's exit.
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

### 8.3 Do-not-touch (absolute)

ADRs, goldens, `[[clang::nonblocking]]` / `YESDAW_RT_HOT`, `.github/workflows/ci.yml` (except the
additive session-drive job, which is its own commit), the reference image, `docs/reality-lane.md`
result rows, engine RT rules. Never weaken or delete a gate (re-pinning with rationale is
expected). Never squash.

### 8.4 Stop-and-ask triggers (the only reasons to contact Dan)

1. An Accepted ADR must be superseded to proceed (write the superseding ADR as *Proposed*, stop).
2. Three consecutive red CI rounds on one item.
3. A phase exit cannot be met without changing the reference design or a §3.4 number by more
   than 20 %.
4. A schema change would break opening a bundle saved by the current `main` (additive migrations
   never trigger this).

Everything else: decide, log it in the deviation log in `STATUS.md`, continue. Dan's optional
lane: at each phase close, run that phase's Session script by hand for ten minutes and write
friction notes in `docs/dogfood/`. His notes go to the top of the *current* phase, ahead of every
other item. Never wait for them.

### 8.5 Phase close-out (one docs commit)

Session scripts of all phases so far green; feel budgets green; rubric recorded; montage in
`docs/evidence/<date>-gN.png`; `STATUS.md` "Now/Next" moved; parking-lot promotions decided (with
reasons); the roadmap's pointer updated.

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
