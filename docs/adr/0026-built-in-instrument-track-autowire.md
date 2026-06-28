# 0026. Built-in instrument-track auto-wire

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0009, ADR-0010, ADR-0017, ADR-0022, H9 engine scaling and robustness plan.

## Context

H4 proved MIDI timing through `DecodedMidiClipNode` and `ImpulseInstrumentNode`, but Project playback
still ignored `midiClips`. H8 also found that `DecodedMidiClipNode` advanced a local cursor and ignored
`Transport::timelineFrame`, so MIDI would desync on locate and loop once wired into runtime playback.

## Options considered

1. **Defer Project MIDI playback until hosted instruments exist.**
   - Pros: avoids a temporary instrument choice.
   - Cons: leaves the H4 Project debt open and keeps transport unproven for MIDI. Rejected.
2. **Persist a full instrument-track model now.**
   - Pros: closer to final product.
   - Cons: too much schema and UI design for H9. Rejected.
3. **Auto-wire each `MidiClip` to the built-in timing instrument.**
   - Pros: reuses existing H4 nodes, keeps the Project schema unchanged, and creates a mechanical runtime
     MIDI transport gate. Accepted.

## Decision

For H9, Project graph projection auto-wires each `MidiClip` as:

`DecodedMidiClipNode -> ImpulseInstrumentNode -> Fader -> Pan -> Meter -> Master`

- MIDI Clip Notes are flattened on the control side using the Project tempo map.
- `DecodedMidiClipNode` honors `Transport::timelineFrame` when present, and keeps the legacy monotonic
  cursor for direct graph callers without transport.
- The built-in `ImpulseInstrumentNode` is only a deterministic timing instrument. User-chosen instruments
  and persisted instrument assignment are later product work.

## Consequences

- **Positive:** MIDI Project playback, locate, and loop become mechanically testable through the same
  runtime path as audio.
- **Negative / accepted costs:** the sound is a timing impulse, not a musical instrument.
- **Follow-ups:** H10/H11 can add persisted instrument choice and UI once the headless transport contract
  is protected.
