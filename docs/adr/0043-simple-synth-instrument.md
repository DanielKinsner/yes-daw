# 0043. SimpleSynth — the built-in musical Instrument

- **Status:** Accepted
- **Date:** 2026-08-10
- **Deciders:** Dan (standing usable-DAW directive), build agent
- **Related:** ADR-0008 (Node contract), ADR-0017 (MIDI model), ADR-0027 (block-parallel guard),
  the usable-DAW P1 backlog in `docs/reviews/2026-08-09-shipped-parity-gap-audit.md`.

## Context

MIDI Clips were audible only as unit impulses through `ImpulseInstrumentNode` — a deliberate timing
instrument, not music. A usable DAW needs MIDI to make musical sound out of the box. Real instrument
plugin hosting is H18; the gap until then needs a built-in instrument that is honest, deterministic,
and RT-safe.

## Decision

`SimpleSynthNode` becomes the production instrument in the MIDI projection (playback and offline
render identically). The impulse node remains exclusively the H4 timing-gate instrument.

The law:
- **8 voices**, oldest-stolen when full. NoteOn allocates, NoteOff releases; every event applies
  sample-accurately at its `timeInBlock` (block-sliced rendering).
- **Wavetable oscillator**: one 2048-sample single cycle (sine + 0.35×2nd + 0.15×3rd harmonic) built
  in `prepare()`; the audio thread only accumulates phase and reads the table (ADR-0008 rule — no
  trig in the read path). A 128-entry note→phase-increment table maps keys (A4 = 440 Hz, equal
  temperament).
- **Envelope**: linear attack 5 ms to the note's normalized-velocity level, sustain, linear release
  120 ms. Velocity maps linearly to level.
- **Output**: mono at ×0.30 gain headroom; the strip's Pan widens it (ADR-0042 mono path).
- **Deterministic**: no randomness, no time queries; the same event stream renders bit-identically.
- **Not block-parallel-safe** (voice state spans Blocks) — the ADR-0027 guard already forces serial
  scheduling for such graphs.

## Consequences

- Renders containing MIDI clips change audibly (impulses → notes). Tests that asserted
  impulse-specific output through the projection are re-pinned to synth-aware assertions; the H4
  timing gates (which construct `ImpulseInstrumentNode` directly) are untouched.
- Patch design (waveform choice, ADSR editing, per-track instrument selection) is deliberately out
  of scope until instrument UI/hosting work; this node is the guaranteed-musical default.

## Verification

- A NoteOn renders a periodic waveform at the note's fundamental (zero-crossing period within one
  sample of 48000/f) with the attack reaching velocity level in 5 ms ±1 sample.
- NoteOff decays to exact zero within the release window and frees the voice.
- Two simultaneous notes sum; a 9th note steals the oldest voice.
- The same MIDI clip renders bit-identically across block sizes 1/128/512 (block-sliced events).
- RTSan/TSan stay green over process().
