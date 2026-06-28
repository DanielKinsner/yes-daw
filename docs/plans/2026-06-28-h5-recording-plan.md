# H5 recording plan

**Why this exists.** H5 is complete when recording placement is mechanically proven. The first gate is
headless and deterministic: no device, no listening, no UI review.

## Exit gate

`YesDawRecordingCheck` is the H5 blocking gate:

- Audio callback input is copied into a bounded FIFO, then drained by a writer thread to a real temp file.
- A click reference recorded through non-zero input + output latency lands back within +/- 1 frame of the
  original Project frame after compensation.
- The same gate has a negative control showing zero compensation would land at the wrong frame.
- Punch/loop recording creates stable take ordinals, and comp selection chooses the requested take.
- MIDI recording uses the same device-frame-to-timeline-frame compensation model as audio.

## Build order

1. **H5 kickoff docs.** Accept ADR-0018, update `CONTEXT.md`, switch `loop/horizon.md` to H5, and update
   this handoff.
2. **Recording spine.** Add the pure engine recording surface: `RecordingLatencyModel`,
   `RecordingChunkFifo`, `RecordingTakeFileWriter`, punch/loop normalization, comp selection, and MIDI
   timestamp compensation.
3. **Gate.** Add `YesDawRecordingCheck` and wire it into the pure engine target list so normal CI,
   RTSan, and TSan build it.
4. **Close.** Run the focused gate and full `ci` preset. H5 closes only if both are green.

## Non-goals

- No device UI or arming UX.
- No final recorded-audio asset format decision; the gate uses an internal deterministic take file.
- No hardware latency calibration; the gate accepts explicit deterministic latency values.
- No Project bundle take-lane persistence yet; the recording alignment contract is the H5 finish line.
- No latency-compensated **monitoring** path (hearing live input through the app while recording).
- No production caller yet: the recording spine is exercised by the gate, not wired into the Runtime,
  audio driver, `Main.cpp`, or the Project surface. Wiring is H6+ work.

## Status

Implemented and closed. `YesDawRecordingCheck` covers the H5 exit gate directly and is part of the full
`ci` preset. A later adversarial review hardened the gate: the latency negative control became a real
broken-pipeline run, and biting cases were added for the stereo, backpressure (with exact
accepted/dropped accounting), `maxLoopTakes`, direct-input, file-format-error, multi-segment comp, and
MIDI-edge paths; the audio-path mapping helpers now carry `YESDAW_RT_HOT`. Full `ctest` 231/231 local.
