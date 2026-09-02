# 0047. One persisted instrument per Track, shared by every MIDI Clip on it

- **Status:** Accepted (Dan, 2026-09-02: "Accept and go") <!-- Proposed | Accepted | Superseded by NNNN | Deprecated -->
- **Date:** 2026-09-02
- **Deciders:** Dan (the Real-DAW arc's G3.1 item), build agent (proposer)
- **Related:** ADR-0045 (MIDI plays through its owning Track's strip — **superseded in part** by
  this ADR: the `Instrument` node keyed by Clip id), ADR-0043 (SimpleSynth), ADR-0026 (the H9
  auto-wire, already superseded in part by ADR-0045), ADR-0039 (automation lanes), ADR-0037
  (FX before hosting), ADR-0046 (the feel-first shell arc), the plan's G3.1 in
  [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md)

## Context

ADR-0045 wires **each `MidiClip`** as `DecodedMidiClipNode -> SimpleSynthNode -> (the Track's
strip Sum) -> …`, with the two node identities (`MidiSource`, `Instrument`) **keyed by Clip id**.
That was the right stopgap for mix truth: the MIDI reaches its Track's strip. It leaves three
things a real DAW does not do:

1. **A synth per Clip.** Two MIDI Clips on one Track are two synths. A note held across a Clip
   boundary is cut and re-attacked; polyphony and voice stealing are per Clip, not per Track;
   CPU scales with Clip count, not Track count.
2. **No instrument choice, no patch.** ADR-0043's synth has no parameters a user can touch and
   nothing is persisted per Track ("per-Track instrument choice and patch editing remain
   deferred" — ADR-0045's own follow-up).
3. **Nothing to automate.** Automation lanes (ADR-0039) target strip and insert parameters; an
   instrument has none, so filter sweeps, the first thing anyone automates on a synth, cannot
   exist.

The plan's G3.1 ("Track instrument") asks for a persisted per-Track instrument slot that
**replaces** per-Clip instantiation, a chooser in the header / inspector, and `SimpleSynth`
parameters (`ParamSpec`) that are automatable and shown in an instrument panel; the Sampler
(G3.9) and hosted plugin instruments (G4.8) fill the same slot later.

What is hard to reverse: the **node identity law** (what `Instrument` is keyed by) is what every
projection gate, the graph-diff / no-rebuild adoption path, and the automation-lane targets key
on; and the **schema** gains a per-Track instrument row that every later instrument kind extends.
Changing an Accepted ADR's law is a plan §8.4 stop-and-ask trigger, hence this ADR is
**Proposed** and the build agent stopped at G3.1 until it was Accepted (2026-09-02).

## Options considered

1. **Option A — a per-Track instrument slot; one `Instrument` node per Track, fed by a per-Track
   MIDI merge of its Clips (this ADR).**
   - Pros: the reference-DAW model (Logic / Cubase / Pro Tools: the instrument is the track's);
     notes sustain across Clip edges; polyphony is per Track; automation lanes get a real target
     (`Instrument` params keyed by Track); the Sampler and plugins drop into the same slot.
   - Cons: ADR-0045's node identity law changes (Instrument keyed by **Track** id); the projection
     gains a merge stage (`MidiSource` per Clip stays, a per-Track `MidiMerge` node sums event
     streams in time order); renders of projects with overlapping MIDI Clips on one Track change
     (voices are shared — that is the fix). Golden renders with one Clip per Track stay
     bit-identical.
2. **Option B — keep per-Clip synths; add a per-Track "instrument settings" record that every
   Clip's synth copies its parameters from.**
   - Pros: no node-identity change; ADR-0045 stands untouched; small diff.
   - Cons: still a synth per Clip (the sustain-across-edges and polyphony defects stay);
     automation would have to fan out to N nodes per Track, with N changing on every Clip edit —
     the lane target would not be stable; the Sampler (per-pad state, voice pools) and hosted
     instruments cannot be duplicated per Clip at all. A dead end the plan explicitly rejects
     ("replacing per-clip instantiation").
3. **Option C — instrument as an FX-chain insert kind (`FxKind::Instrument`) at the head of the
   Track's chain.**
   - Pros: reuses the persisted FX chain, its undo verbs, its inspector slot UI and its automation
     targeting for free.
   - Cons: an instrument is not an insert (it has an event input and no audio input; it must sit
     before the chain, exactly once; a chain reorder verb could move or remove it); ADR-0037's
     chain law would need carve-outs everywhere; the header chooser the plan asks for would be a
     façade over slot 0. Rejected as a category error, though the *panel* UI may share the
     insert panel's parameter-row widget.

## Decision

**Option A.** A `Track` gains a persisted **instrument slot**:

- **Schema (additive migration):** `tracks.instrument_kind` (`none` | `simple_synth`; `sampler`
  and `plugin` later) and `tracks.instrument_state` (a versioned parameter blob; for
  `simple_synth` the `ParamSpec` values). A Track whose kind is `none` still auto-selects
  `simple_synth` when it holds a MIDI Clip, so every project saved by today's `main` opens and
  renders exactly as before (no §8.4 trigger 4).
- **Projection law (supersedes ADR-0045's identity law in part):** per Track holding MIDI Clips:
  `DecodedMidiClipNode (per Clip, keyed by Clip id) -> MidiMergeNode (keyed by Track id) ->
  Instrument (keyed by Track id) -> the Track's strip Sum -> FX chain -> Fader -> Pan -> Meter ->
  Master`. `MidiSource` keeps its Clip-id key; `Instrument` is keyed by **Track** id; `MidiMerge`
  is a new role. Everything after the Sum is unchanged.
- **`SimpleSynth` `ParamSpec`:** osc mix (sine ↔ the harmonic table), attack, decay, sustain,
  release, filter cutoff, filter resonance, glide, volume — each with range, default, unit and a
  stable id; the audio thread reads them through the same block-sliced parameter path inserts use
  (ADR-0039); no allocation, no trig in the read path (ADR-0008).
- **Undo:** one verb family (`SetTrackInstrument`, `SetTrackInstrumentParam`) with before-image
  diffs; the parameter verb coalesces like strip scalars (E21).
- **Automation:** lanes may target `Instrument` params by Track id + param id; the compiled lane
  path is ADR-0039's, unchanged.
- **Shell:** the rail's header and the inspector's TRACK tab get an instrument chooser; an
  instrument panel (a dock tab, per ADR-0046: never a modal) shows the `ParamSpec` rows.

## Consequences

- **Positive:** notes sustain across Clip edges; polyphony and CPU are per Track; the first
  automatable instrument parameter exists; the Sampler and hosted instruments have their slot;
  the header says which instrument a Track plays.
- **Negative / accepted costs:** ADR-0045's `Instrument`-by-Clip identity law is gone; the
  projection gains a merge node; renders of Tracks with **overlapping** MIDI Clips change (shared
  voices); every projection gate that keyed `Instrument` by Clip id is re-pinned by Track id with
  that rationale.
- **Follow-ups:** `CONTEXT.md` — "Instrument slot", "MIDI merge", "ParamSpec" (the arc's
  vocabulary section); the G3.1 gates: render golden (one Clip per Track bit-identical; two
  overlapping Clips on one Track share voices — a sustained note across the edge is one note),
  automation on an instrument param changes the render, a persisted instrument round-trips,
  `[track-instrument]` for the chooser and the panel.

## Verification (once Accepted)

- `[track-instrument]` (`tests/ui_input_tests.cpp`): the chooser persists, the panel's rows edit
  `ParamSpec` values through undoable verbs, the probe reports the Track's instrument.
- Render goldens (`tests/render_tests.cpp` or the existing MIDI render harness): (a) one Clip per
  Track — bit-identical to the pre-ADR render; (b) a note spanning two adjacent Clips renders as
  one held note (no re-attack); (c) an automation lane on filter cutoff changes the spectrum.
- Persistence: the migration adds the two columns; a bundle saved by the current `main` opens
  with `none` kind and renders bit-identically.
