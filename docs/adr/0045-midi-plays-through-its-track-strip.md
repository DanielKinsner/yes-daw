# 0045. MIDI plays through its owning Track's strip

- **Status:** Accepted — superseded in part by ADR-0047 (the `Instrument` node is keyed by Track, not Clip)
- **Date:** 2026-08-14
- **Deciders:** Dan (standing usable-DAW directive), build agent
- **Related:** ADR-0026 (built-in instrument-track auto-wire — superseded in part by this ADR),
  ADR-0043 (SimpleSynth), ADR-0042 (stereo sources and width), ADR-0014 (mute/solo policy),
  ADR-0039 (automation lanes), the M1 item in
  [`docs/goals/2026-08-14-mix-truth-and-strip-parity-backlog.md`](../goals/2026-08-14-mix-truth-and-strip-parity-backlog.md)

## Context

ADR-0026 decided the H9 stopgap: Project graph projection auto-wires **each `MidiClip`** as
`DecodedMidiClipNode -> Instrument -> Fader -> Pan -> Meter -> Master`. That ADR's own text calls
the arrangement temporary — "user-chosen instruments and persisted instrument assignment are later
product work" — and it was written before Tracks had strips worth speaking of.

Three horizons later the Track strip is a real product surface: fader, pan, mute/solo, an FX insert
chain, sends to buses, and automation lanes, all persisted and all editable from the shipped mixer.
But the per-Clip MIDI chain bypassed every bit of it. A MIDI Clip rendered at unity gain, centred,
dry, straight to master; only mute/solo were borrowed from the owning Track. Worse, the projection
skipped any Track that owned no **audio** Clip (`if (! ownsAudioClip) continue;`), so a MIDI-only
Track had no strip in the graph at all — while the UI painted it a full strip whose controls moved,
persisted, and changed nothing audible. Dead controls are the defect class this project exists to
kill (ADR-0005's mechanical-honesty rule); an adversarial re-audit on 2026-08-14 found this one
sitting under the whole "MIDI is a first-class citizen" line of work.

## Options considered

1. **Keep the per-Clip strip, and mirror the Track's strip state onto it.**
   - Pros: no projection surgery.
   - Cons: two strips per Track that must be kept in sync forever; sends, FX chains and automation
     lanes would each need a duplicate projection; the meter would still read the wrong thing.
     Rejected — it multiplies the lie instead of removing it.
2. **Persist a full instrument-track model first** (instrument per Track, patch state, the ADR-0026
   "later product work").
   - Pros: closest to the eventual product.
   - Cons: needs schema, UI and a patch model before a single dead control gets fixed; ADR-0043
     already defers patch design. Rejected as a prerequisite — it remains a later, separable step.
3. **Route each MIDI Clip's instrument into its owning Track's strip Sum.** *(chosen)*
   - Pros: one strip per Track, exactly as audio works; FX, fader, pan, meter, sends, automation and
     mute/solo apply to MIDI with no new mechanism; playback and offline share the same projection,
     so export == playback stays true by construction; no new persisted state, so no schema change.
   - Cons: MIDI and audio on one Track share one strip (correct for a DAW, but it means a Track
     cannot mix MIDI and audio at different levels without splitting Tracks) and the built-in
     instrument stays one-per-Clip until an instrument model lands.

## Decision

Project graph projection wires each `MidiClip` as:

`DecodedMidiClipNode -> SimpleSynthNode -> (the owning Track's strip Sum) -> FX chain -> Fader -> Pan -> Meter -> Master`

- The MIDI nodes are built by the **mixer projection** (`projectToMixerProjectionInputs`), alongside
  the Track's audio Clip sources, instead of by the offline renderer's per-Clip loop. Both playback
  and offline render reach it through the one shared `buildProjectGraph`.
- A Track projects when it owns an audio Clip **or** a MIDI Clip.
- Node identity is unchanged for the two surviving roles (`MidiSource`, `Instrument`, keyed by Clip
  id). The per-Clip `Fader`/`Pan`/`Meter` roles are no longer projected for MIDI.
- `SimpleSynthNode` emits the strip's width, carrying the ADR-0042 equal-power centre gain
  (cos(pi/4)) when it widens onto a stereo strip — the same law `DecodedClipNode` applies to a mono
  Asset, so a MIDI Clip's centred loudness does not depend on its Track's width.
- Mute/solo (ADR-0014) applies at the Track's Sum, which now carries MIDI, so the policy is
  evaluated once per strip rather than once per MIDI Clip.
- ADR-0026's per-Clip projection shape is **superseded**; its MIDI timing, tempo-map flattening and
  transport-cursor decisions stand unchanged.

## Consequences

- **Positive:** every control painted on a MIDI Track's strip does what it says; MIDI finally shows
  on the Track meter; automation lanes and sends work for MIDI with no MIDI-specific code; one
  projection path instead of two.
- **Negative / accepted costs:** a Track's MIDI and audio share one strip (by design); renders of
  projects whose MIDI Tracks carry non-default strip state change audibly — that is the fix, and
  such renders were previously wrong. At unity gain, centre pan and an empty FX chain the render is
  bit-identical, so existing gates and the golden files are untouched.
- **Follow-ups:** per-Track instrument choice and patch editing remain deferred (ADR-0043); M2
  extends projection to Tracks with no Clips at all so automation lanes can never orphan.

## Verification

- `[midi-strip]` (shipped boundary, `tests/ui_input_tests.cpp`): on a two-track project whose MIDI
  window and audio window never overlap in one render — the MIDI Track's fader at 0.5 halves the
  MIDI peak exactly and leaves the audio window bit-identical; hard-left pan silences the right
  channel of the MIDI window only; bypassing the Track's EQ insert changes the MIDI window and
  nothing else; removing the Track's send drops the MIDI window's level; each edit undoes to a
  bit-identical render. Red before this ADR at the first fader assertion.
- The full suite (350/350 locally) stays green with no re-pinned assertions, confirming the
  unity/centre/dry path is bit-identical.
