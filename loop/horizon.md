# Current horizon — H4 (MIDI editing & instruments) — CLOSED

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

## Status: **CLOSED (H4 exit gate green; H5 ready for Dan's boundary call)**

H4 opened on 2026-06-27 when Dan asked to complete H4. The review/close pass audited H4 against the
roadmap, H4 plan, ADR-0017, ADR-0009, ADR-0010, `STATUS.md`, and the blocking tests. `YesDawMidiTimingCheck`
is green locally together with the full `ci` preset, and the final implementation CI was green on Windows,
Linux, macOS, RTSan, and TSan. Project-owned MIDI Clips/Notes persist through schema v3. Piano-roll Note
edit commands have undo/redo bit-identity coverage. Deterministic MIDI-effect Nodes transform branch-local
writable Events before downstream Instruments consume them, hosted-instrument `PluginNode` receives
transformed Note Events through the RT lane, and MPE boundary allocation assigns stable concrete
`VoiceAddress` port/channel fields before flattening, including overlapping future explicit voice
reservations. H4 is closed; do not start H5 until Dan opens that boundary.

## The plan

Full build order, every subsystem, every finding/deferral dispositioned:
[`docs/plans/2026-06-27-h4-midi-editing-instruments-plan.md`](../docs/plans/2026-06-27-h4-midi-editing-instruments-plan.md).
