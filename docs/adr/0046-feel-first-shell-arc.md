# 0046. The feel-first shell arc: reference-DAW parity is the UI law

- **Status:** Accepted
- **Date:** 2026-09-01
- **Deciders:** Dan (owner — "turn this whole project around"), build agent
- **Related:** ADR-0005 (mechanical verification — extended, not weakened), ADR-0032 (UI stack and
  app shell — its action registry is kept; its keymap rule is tightened), ADR-0033 (operable
  session UX), ADR-0037 (alpha target — its "one batched human feel session" clause is amended),
  the plan [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md),
  the reference design `docs/design/arrangement-view-reference.png`.

## Context

On 2026-09-01 Dan used the shipped app for the first time in earnest and reported: laggy, things
not working or hard to understand, Space-to-play "worked half the time depending on focus", no
visible cut tools or familiar hotkeys, "focuses on obscure details instead of obvious things a
DAW would need", UI "just not there". The same evening the build agent reproduced the report on
the real executable with injected input and screenshots: Space after clicking in the timeline did
nothing (twice); Space after clicking a toolbar button started the transport.

The causes are not mysterious and none of them are in the engine:

- `Space` is bound to *Play only*; Stop is `K`. No widget excludes itself from keyboard focus, so
  any clicked button or combo box eats the next keypress (`MainComponent.cpp:3036` is the only
  focus declaration in the shell).
- Every UI action removes and re-adds the audio device callback
  (`handleAction` → `suspendDesktopAudioCallback`, `MainComponent.cpp:7165-7190`).
- The whole window repaints thirty times a second (`timerCallback` → `repaint()`,
  `kUiRefreshIntervalMs = 33`), and most edits rebuild the entire playback engine
  (`rebuildPlaybackForCurrentProject`).
- Default chords are invented: Split `B`, Loop `Ctrl+Alt+Shift+L`, Mixer dock
  `Ctrl+Alt+Shift+M`, Snap `Ctrl+1/2/3`, Import `Ctrl+I`, Add Track `Ctrl+T`. Menus paint labels
  without shortcuts (`getMenuForIndex`, `MainComponent.cpp:7380-7396`). Tools are unlabeled icons.
  There is no right-click menu anywhere in the shell.
- `MainComponent.cpp` is 11,727 lines; `UiAppModel.h` is 8,709.

Meanwhile 362 self-asserting gates are green and five audit-carved backlogs (B, E, M, N, R) were
completed on schedule. The process produced an honest *model* and an unusable *shell*. Adversarial
code audits find real defects, but they never ask "can a person press Space and cut a clip". ADR-0005
made verification mechanical so that Dan never has to judge by eye; in practice it drifted into
"only tests count", and nothing graded the experience. ADR-0037 sanctioned exactly one human feel
session at the end — far too late, and Dan cannot be the feedback loop anyway (he is not in the
software daily; agents wander when he is not).

What is hard to reverse: the *default keymap* (muscle memory of every future user), the *selection
and edit-mode model* (every editing verb depends on it), the *verification classes* the loop is
graded on (what gets measured gets built), and the *component topology* of the shell (a rebuild
that keeps the 11k-line god component would fail the same way).

## Options considered

1. **Option A — Continue the carved R18–R34 backlog, fix the Space bug as R-item zero.**
   - Pros: no process change; the loop protocol is proven to ship items.
   - Cons: ships more items nobody can reach; the grading signal (audit findings) is the thing
     that failed; eleven of the seventeen remaining items are recording and polish, not editing.
2. **Option B — Rewrite the shell from scratch in a new UI layer, keep engine and model.**
   - Pros: escapes the god component; a clean topology from day one.
   - Cons: one to two months of nothing visible; throws away a real action registry, real undo
     verbs, and 15k lines of shell gates; the same wandering would resume without new laws.
3. **Option C — Feel-first arc (chosen).** Freeze features. Re-found the shell on a small set of
   *laws* copied from Logic Pro and Pro Tools, executed as a phased plan (G0–G8) that carves the
   god component along the way, with a new **mechanical** gate class that drives the *real built
   executable* the way a user does, plus agent visual judgment at every UI checkpoint. Editing and
   MIDI first; recording last.
   - Pros: attacks the actual failure (feel, discoverability, grading signal) with the least
     rework; keeps every asset that is genuinely good; makes the loop self-correcting without Dan.
   - Cons: the first phase is plumbing (keys, rendering, engine boundary) with modest visible
     change; some existing gates are re-pinned to new semantics (never weakened); the keymap
     changes under anyone who learned the old one (nobody has).

## Decision

**Option C.** The following are law for the shell from this date. They bind every agent and every
backlog until a later ADR supersedes them.

1. **Reference-DAW rule.** Any UI question — a chord, a gesture, a layout, a default — is settled
   by "what does Logic Pro do; what does Pro Tools do". Logic first (it is the closest match to the
   reference design and to editing-plus-MIDI-first), Pro Tools second, then Cubase/Reaper for
   consensus. The precedent is written into the item. Nothing is invented where a precedent
   exists. Windows modifiers map `Cmd → Ctrl`, `Option → Alt`.
2. **No invented chords.** A default chord exists only when at least two reference DAWs use it or
   an obvious variant. Every other action has *no* default chord and is reached by mouse (menu,
   context menu, toolbar, inspector) and by the keymap editor. Three-modifier chords are never a
   default for a daily verb.
3. **Focus contexts.** The keymap is resolved per focused editor (Arrange, Piano roll, Mixer), so
   the same chord may mean different things in different editors, as in Logic. Chord uniqueness is
   enforced per context, not globally. Global transport chords work in every context.
4. **Keys go to the command router, not to widgets.** Only an active text field consumes keys.
   Buttons, combo boxes, sliders, and faders never take keyboard focus. `Space` toggles play/stop.
5. **Everything is reachable by mouse.** Every action a user performs daily is in a menu that
   paints its shortcut, in a right-click context menu on the object it acts on (clip, track
   header, ruler, empty lane, note, strip), or on a labeled toolbar control with a tooltip that
   names the key. A control that exists but is unreachable or unexplained is fixed or removed —
   no dead affordances.
6. **Nothing is blind.** Every drag paints its result while dragging; every edit is visible within
   one frame; the playhead never stutters because of the UI. The audio device callback is never
   removed by a UI action. Playback-graph rebuilds happen only on topology changes (add/remove
   track, bus, insert, send, route); placement and parameter edits are live deltas.
7. **Feel budgets are gates.** Action-to-paint ≤ 16 ms; paint per frame ≤ 8 ms at 2560×1440 with a
   16-track, three-minute stereo song; zero engine rebuilds in a 100-edit script that changes no
   topology; zero audio-callback removals after startup; zero Underruns while editing during
   playback. Numbers live in the plan and may only tighten.
8. **Selection model.** Two selection kinds exist and every verb acts on the current one: an
   *object selection* (clips, notes, tracks) and a *time selection* (a ruler range on one or more
   tracks). Edit modes (Overlap default, No-overlap, Shuffle) decide what happens to neighbours.
9. **Density follows the reference.** `docs/design/arrangement-view-reference.png` remains the
   visual truth (ADR-0037 plan D2), and the plan fixes numeric density targets (default track
   height, tracks visible at 1080p, header widths) that gates assert.
10. **Session drive is a gate class.** A one-command script (`tools/session-drive.ps1`) launches the
    real built executable, injects real Win32 input (proven 2026-09-01 with plain PowerShell — no
    accessibility permission dance), reads a debug-only state probe the app writes when
    `YESDAW_STATE_PROBE` is set, and asserts a numbered *Session script* of user steps. It is
    exit 0/1 and runs at every checkpoint of a phase and for every earlier phase's script. It is
    mechanical in the ADR-0005 sense; it complements ctest, never replaces it.
11. **Agent visual judgment at every UI checkpoint.** The agent screenshots the real shell at
    1280×720, 1920×1080, and 2560×1440, judges it against the reference and the density table,
    records the verdict in `STATUS.md`, and locks each fix with a token/layout gate. This amends
    ADR-0037's "one batched human feel session": Dan's session is optional and never gating.
12. **Editing and MIDI before recording.** Phases G0–G6 (feel, commands, arrange, MIDI, mixer,
    project, polish) close before any recording item (G7) opens. Recording items already carved
    (R24–R29) are parked into G7 unchanged.
13. **Anti-wander.** Agents work only the current phase's items, top to bottom. No new adversarial
    audit carves during the arc; any finding goes to `docs/goals/parking-lot.md` and is promoted
    only at a phase close. A phase closes on its session script, its gates, and its visual
    rubric — never on a count of ticked items.

## Consequences

- **Positive:** the loop is graded on what a user feels, mechanically; the keymap becomes learnable
  in an afternoon by anyone who has used Logic or Pro Tools; the god component is carved into
  parts along phase boundaries instead of rewritten; every existing engine, model, and gate asset
  is kept.
- **Negative / accepted costs:** roughly a dozen shipped chords change (documented in the plan's
  keymap table with old → new); several `ui_input_tests` gates are re-pinned to new semantics
  (e.g. `Ctrl+A` scope, `Space` toggle, tool letters); the session-drive gate is Windows-only at
  first (Linux/macOS keep ctest + screenshot gates); phase G0 is mostly plumbing.
- **Follow-ups:** `CONTEXT.md` gains *Focus context, Time selection, Object selection, Edit mode,
  Smart tool, Nudge value, Snap mode, Session script, Session drive, State probe, Feel budget,
  Reference-DAW rule, Editor dock, Arrange window*. `STATUS.md` opens the G-arc. The
  2026-08-25 reality-run backlog is closed as a list; its open items are mapped into phases by the
  plan. `docs/goals/roadmap.md` points at the plan for everything shell-side.
