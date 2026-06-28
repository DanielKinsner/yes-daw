# 0018. Recording latency compensation and take writer

- **Status:** Accepted
- **Date:** 2026-06-28
- **Deciders:** Dan (owner), build agent (H5)
- **Related:** ADR-0002 (real-time engine foundations), ADR-0006 (Runtime concurrency),
  ADR-0009 (Event stream), ADR-0010 (time model), ADR-0011 (Project identity),
  [H5 plan](../plans/2026-06-28-h5-recording-plan.md), [roadmap](../goals/roadmap.md),
  `CONTEXT.md`.

## Context

H5 introduces recording. The hard part is not a button or device UI; it is proving that captured audio
and MIDI are placed on the Project timeline at the frame they actually represent. The roadmap exit names
a recorded take aligned to a click reference with non-trivial input and output latency. That implies a
loopback-style deterministic gate: the click leaves the app after output latency, returns after input
latency, and the recorded take must subtract the full round-trip before writing timeline placement.

The audio thread still cannot allocate, lock, log, or do file I/O. Disk writing, take metadata, comping,
and project mutation belong off the audio thread.

## Options considered

1. **Write directly from the audio callback.**
   - Pros: smallest code.
   - Cons: violates ADR-0002 immediately by doing disk I/O on the audio thread. Rejected.
2. **Record into an unbounded queue or vector, then write later.**
   - Pros: easy tests.
   - Cons: hides backpressure and allocation risk from the exact boundary H5 needs to prove. Rejected.
3. **Audio callback copies fixed-size chunks into a bounded SPSC FIFO; a writer thread drains to a take
   file.**
   - Pros: matches the roadmap shape; backpressure is explicit; writer owns disk I/O; deterministic tests
     can measure alignment without hardware. Accepted.

## Decision

**Recording uses a bounded audio-thread FIFO and a writer-thread take file.**

- The audio callback copies planar input samples into fixed-capacity `RecordingChunk` records and pushes
  them to `RecordingChunkFifo`. The hot path never opens files or allocates.
- The writer thread drains chunks into a deterministic take file. The file records take ordinal, timeline
  start frame, channel count, and float samples for each chunk.
- For a loopback click-reference gate, recorded project frame = device input frame - input latency -
  output latency. For future direct-input recording, the same model can exclude output latency.
- Punch and loop recording are timeline filters over the compensated frame. Loop iterations become stable
  take ordinals. Comping selects timeline ranges from recorded take ordinals.
- MIDI recording uses the same latency compensation surface: device input frame timestamps become Project
  timeline frames before Note Events are persisted or flattened.

## Consequences

- **Positive:** H5 has a self-asserting, hardware-free gate for the irreversible recording alignment
  contract; the audio thread still obeys ADR-0002; audio and MIDI recording share one placement model.
- **Negative / accepted costs:** the first take file is a deterministic internal test format, not final
  user-facing WAV/BWF export; real device latency discovery and UI arming are later work.
- **Follow-ups:** connect the writer output to Project bundle assets/take lanes; add real device latency
  calibration; replace the internal test take format with the chosen user-facing recorded-audio asset
  format when export/import UX lands.
