# H6 reliability plan

**Why this exists.** H6 is complete when the reliability exit criterion is mechanically proven: a heavy
session meets the block deadline over 60 minutes of audio frames, and a hard kill mid-edit recovers to the
last autosave with no corruption.

## Exit gate

`YesDawReliabilityCheck` is the H6 blocking gate:

- Builds a heavy synthetic engine session through `GraphBuilder`.
- Processes 60 minutes of audio frames at a 128-frame block.
- Records block processing times and asserts the 99.9th percentile is under the block period.
- Keeps the headless underrun counter at zero.
- Writes a bundle-shaped last-good autosave snapshot.
- Simulates a hard kill by abandoning a later edit transaction.
- Restores the last autosave and reopens the bundle through normal integrity, foreign-key, asset, and
  semantic validation.

## Build order

1. **H6 kickoff docs.** Accept ADR-0019, switch `loop/horizon.md` to H6, and update this handoff.
2. **Autosave recovery surface.** Add a narrow persistence helper for writing and restoring the latest
   bundle-shaped autosave snapshot.
3. **Deadline surface.** Add a small reliability stats helper and the focused H6 gate.
4. **Close.** Run the focused gate and full `ci` preset. H6 closes only if both are green.

## Non-goals

- No final multicore work-stealing scheduler yet.
- No DAWproject export.
- No loudness metering.
- No time-stretch Node.
- No accessibility surface.
- No device hot-swap UI or policy.
- No replacement for the real-device soak lane from ADR-0005.

## Status

Implemented and closed. `YesDawReliabilityCheck` covers the H6 exit gate directly and is part of the
full `ci` preset. Focused local gate: 2/2 green. Full local `ctest --preset ci`: 233/233 green.
