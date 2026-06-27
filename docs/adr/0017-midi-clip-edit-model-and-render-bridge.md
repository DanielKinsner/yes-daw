# 0017. MIDI clip edit model and render bridge

- **Status:** Accepted
- **Date:** 2026-06-27
- **Deciders:** Dan (owner), build agent (H4 kickoff)
- **Related:** ADR-0008 (Node contract), ADR-0009 (event stream), ADR-0010 (time model),
  ADR-0011 (Project identity), ADR-0015 (plugin-hosting runtime),
  [H4 plan](../plans/2026-06-27-h4-midi-editing-instruments-plan.md),
  [roadmap](../goals/roadmap.md), `CONTEXT.md`.

## Context

H4 makes the co-equal MIDI model user-facing: MIDI Clips, Notes, piano-roll edits, MIDI-effect Nodes,
instrument Nodes, and MPE voice allocation at the I/O boundary. The irreversible choice is where MIDI
lives. If raw MIDI bytes become the canonical Project state, piano-roll editing, MPE, undo/redo, and
tempo-map rendering all inherit a transport format's quirks. If Notes become a first-class edit model,
the existing ADR-0009 Event stream can stay the render boundary and MIDI timing can share the same PDC
path as automation and plugin events.

## Options considered

1. **Raw MIDI messages as canonical clip storage.**
   - Pros: smallest import path; close to SMF and JUCE `MidiMessageSequence`.
   - Cons: poor piano-roll edit semantics; MIDI-1 widths leak into the model; MPE and per-note identity
     become retrofits; tempo changes require ad hoc timestamp interpretation. Rejected.
2. **Fold MIDI Notes into the existing audio `Clip` type.**
   - Pros: one visible arrangement item type; reuses some edit plumbing.
   - Cons: audio Clips reference immutable Assets and source windows, while MIDI Clips own editable
     Notes. Mixing the two would make the invariants weaker and persistence harder to validate.
     Rejected.
3. **Separate `MidiClip` + `Note` edit model, flattened to ADR-0009 Events only at render.**
   - Pros: preserves clean Project invariants; exact tick-domain editing; one shared render stream for
     Notes, automation, hosted instruments, and MIDI effects; PDC can compensate MIDI by moving Events.
   - Cons: requires a bridge layer and persistence rows. Accepted.

## Decision

**MIDI uses a first-class edit model and a one-way render bridge.**

- `MidiClip` is a Project-owned arrangement item with an Entity ID, track ownership, timeline start,
  timeline length, `time_base`, and a collection of `Note` objects. It does not reference an Asset.
- `Note` is stored in clip-relative ticks with stable Entity ID, start tick, length ticks, key,
  normalized velocity, channel/port hints, and a stable note identity used to populate
  `VoiceAddress.noteId`. MIDI-1 import widens at the boundary; MIDI-2/MPE data maps into the same shape.
- Flattening happens only at the render boundary: clip tick + note tick -> Project tick -> sample frame
  through the tempo map -> half-open Block offset -> ADR-0009 `NoteOn` / `NoteOff` Events. Events exactly
  on a Block boundary belong to the next Block. A tempo change inside the Block is handled by the full
  tempo map, not by constant-tempo math.
- Event ordering is deterministic. Events sort by sample offset, then by stable clip/note identity and
  event role. A zero-length Note is represented by an On and Off at the same sample without creating a
  hung voice.
- MIDI-effect Nodes are Nodes that consume and produce Events. Instrument Nodes consume Note Events and
  produce audio; hosted instruments receive the same Event stream through the PluginNode boundary.
- MPE voice allocation is a boundary concern. The input/import layer assigns stable voice addresses;
  the graph preserves them. Full MPE expression editing can widen later without changing the Event or
  Note identity model.

## Consequences

- **Positive:** piano-roll edits operate on stable Note objects; MIDI timing uses the locked tempo map
  and Event stream; hosted and built-in instruments share the Node contract; the H4 exit gate can prove
  note timing through PDC mechanically.
- **Negative / accepted costs:** MIDI Clips need their own persistence rows and edit commands; flattening
  needs a tempo-map sample conversion helper; event ordering must be tested because same-sample Note
  edges are easy to get wrong.
- **Follow-ups:** add `MidiClip`, `Note`, `Instrument Node`, `MIDI-effect Node`, `Piano roll`, and
  `MPE voice allocation` to `CONTEXT.md`; add the H4 exit gate `YesDawMidiTimingCheck`; extend Project
  persistence once the value surface is green.
