# 0074. ADR-0073 amended: a new project's Arrange (one empty track) offers Import audio

- **Status:** Accepted (agent, 2026-10-08, under ADR-0049's implementation-ADR clause - it keeps ADR-0073's laws and
  changes no owner-settled contract - after a separate agent critic; its two should-fixes are folded in)
- **Date:** 2026-10-08
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** amends [ADR-0073](0073-empty-states-and-first-run-tip.md) (empty-state rows, the first-run tip); the plan's
  G6.5 ("Explain the next available action using its actual current chord (for example, import audio)").

## Context

ADR-0073's research held that a new project and the Untitled launch session are "a loaded project with no tracks". They
are not: both are made by `UiAppModel::makeDefaultSessionProject` (`src/ui/UiAppModel.h:1630`), which adds one default
audio track. ADR-0073 cp2 found it when its gate read one track after File > New. So the empty Arrange a new user
actually sees - one empty track, no clips - has no row under ADR-0073, and its "no tracks" rows appear only after every
track is deleted (or from a template with none). The Default template is the canonical case; a user template (ADR-0060)
with several tracks and no clips is the same state. The first-run tip, keyed to "the lanes are empty" and "goes for good
the first time a track appears", would never show for that user either.

Lanes keep a fixed height and never stretch to fill the window (G0.7, `src/ui/TimelineCanvas.h:149-151`; the default
row is 72 px), so a new project's single lane leaves most of the clip area free below it.

## Decision

1. **The Arrange with tracks but no clips** (no audio clip and no MIDI clip in the project) offers one row,
   `empty.arrange.import_audio` - `Import audio  (Ctrl+Shift+I)` (`ProjectImportAudio`), by ADR-0073's §1-§2 laws
   (a painted control naming the chord that works there, else the menu path). It is centred in the clip area's free
   space **below the last lane** - never over a lane - where a dropped file lands as a new track; when empty lanes leave
   no room for it, it is absent (dropped whole, as ADR-0073 §5 drops any row that does not fit). ADR-0073's rows lay
   out the same way (with no tracks the free space is the whole clip area). The row takes left clicks only inside its
   own rect (checked before the lane's gestures); everything else on the canvas behaves as before. ADR-0073's no-tracks rows (`Add audio track`, `Import audio`) and
   no-project rows are unchanged; `empty.arrange.import_audio` is one id in both states (they never coexist).
2. **The first-run tip** (ADR-0073 §4) shows while the project has **no clips** (and no autosave recovery question is
   pending) and goes for good the first time **a clip** appears (imported, dropped, recorded, drawn, or a project with
   clips opened); undoing that clip does not bring it back (the dismissal is persisted). Its strip still sits just under
   the ruler; it shows only while every lane is empty, so it covers no clip; it never covers a record or a widget (gated).
3. Everything else in ADR-0073 stands.

## Gates

- **`[g65][empty-rows]`** gains: after File > New (one empty track) the rows are exactly `Import audio  (Ctrl+Shift+I)`;
  Enter on it imports the fixture and the row gives way; a MIDI clip alone (no audio) also removes it; a click just
  outside the row is the lane's (the row's action does not run) and a click inside runs it.
- **`[g65][no-obscured]`** runs in the one-empty-track state too: the row meets no record, no visible widget and no
  lane at every size, and is absent (not clipped) once added empty tracks leave no room below the last lane.
- **`[g65][tip]`** (cp4) keys to clips: the strip shows on a new project, stays after a track is added, goes for good at
  the first clip.

## Alternatives rejected

- **Make a new project start with no tracks.** It changes File > New and the launch session (G5, ADR-0060's templates)
  beyond G6.5, and every reference DAW but Logic's dialog opens with something to put audio on.
- **Rows centred over the lanes.** With many empty tracks a centred row would sit on a lane; below the last lane it
  never does.
- **"Add audio track" beside "Import audio" in this state.** A track is already there; the honest next step is to put
  sound on it.
