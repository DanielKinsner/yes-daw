# Current horizon — H4 (MIDI editing & instruments)

> This file is the oracle for "is the horizon done?". H4 closes iff the exit gate below is green.

## Exit criterion (the finish line)

Note-ons at known offsets land sample-accurately across Block boundaries and a tempo change, through an
Instrument Node with non-zero latency that PDC compensates.

**`YesDawMidiTimingCheck`** proves this with in-repo deterministic MIDI Clips, a tempo map, a built-in
Instrument Node, and the compiled graph. It asserts:

- MIDI Clips flatten Notes to sorted ADR-0009 Events with half-open Block semantics.
- A tempo change inside the tested window is converted through the full tempo map.
- The Instrument Node has non-zero latency.
- PDC compensates that latency so the rendered impulse lands at the predicted frame.
- Negative controls catch block-boundary off-by-one errors, constant-tempo flattening across the tempo
  change, and missing event/audio PDC compensation.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --preset ci -R YesDawMidiTimingCheck
```

## Status: **PLANNED (gate not built yet)**

H4 opened on 2026-06-27 when Dan asked to complete H4. The first checkpoint records ADR-0017 and the
plan. The next code checkpoint builds the gate and the minimal timing bridge, then turns it green before
commit.

## The plan

Full build order, every subsystem, every finding/deferral dispositioned:
[`docs/plans/2026-06-27-h4-midi-editing-instruments-plan.md`](../docs/plans/2026-06-27-h4-midi-editing-instruments-plan.md).
