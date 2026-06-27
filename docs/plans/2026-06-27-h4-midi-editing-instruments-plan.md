# H4 MIDI editing and instruments plan

**Why this exists.** H4 is done only when MIDI timing is mechanically proven through the same Event,
Node, and PDC machinery as audio and automation. The finish line is the roadmap exit criterion, not a
collection of partial MIDI surfaces.

## Exit gate

`YesDawMidiTimingCheck` is the H4 blocking gate:

- MIDI Clips flatten Notes to `NoteOn` / `NoteOff` Events at known offsets across variable Block
  boundaries.
- A tempo change inside the tested window is converted through the full tempo map.
- The Events drive an Instrument Node with non-zero latency.
- The compiled graph's PDC compensates that latency so the rendered impulse lands at the predicted
  frame.
- The gate has negative controls for the three failure modes H4 is about: block-boundary off-by-one,
  constant-tempo flattening across a tempo change, and uncompensated instrument latency.

The gate must be green in the normal `ci` preset and must not depend on a human listening or looking.

## Build order

1. **H4 kickoff docs.** Accept ADR-0017, update `CONTEXT.md`, switch `loop/horizon.md` to H4, and update
   `STATUS.md`.
2. **Timing bridge.** Add the tempo-map tick-to-frame helper and MIDI Clip / Note flattening into
   ADR-0009 Events. Prove block-boundary and tempo-change offsets.
3. **Instrument timing gate.** Add the smallest built-in test Instrument Node with non-zero latency and
   wire `YesDawMidiTimingCheck` through `GraphBuilder` / `CompiledGraph` so PDC must compensate Events.
4. **Project surface.** Add Project-owned MIDI Clips and Note validation, then persist and round-trip
   them through the `.yesdaw` bundle.
5. **Piano-roll edit operations.** Add pure edit commands for move, length, split/cut, quantize, and
   transpose on Note objects, with undo/redo bit-identity coverage where the command layer exists.
6. **MIDI-effect Nodes.** Add narrow event-transform Nodes first: transpose and scale/chord are
   deterministic; arpeggiator can land only with a clocked, bounded-state contract.
7. **Hosted instrument/Event bridge.** Drive Note Events through the PluginNode RT lane so hosted
   instruments receive the same stream as built-ins. This closes the H3 deferral for full event
   tri-stream delivery through the worker boundary.
8. **MPE boundary allocation.** Assign stable concrete `VoiceAddress` port/channel values for wildcard
   Notes at the input/import/render boundary while preserving explicit hints and failing mechanically if
   overlapping Notes exhaust the configured member channels.
9. **Review and close.** Re-run the full gate, update `STATUS.md`, and leave H5 ready to start only if
   the evidence proves the horizon complete.

## Stop conditions

- Stop for a new ADR if any slice changes the accepted Note identity, Event ordering, tempo conversion,
  Node contract, plugin process model, or persistence invariants.
- Do not edit goldens unless the change intentionally alters rendered audio and the blessing command is
  run explicitly.
- The H0 real-hardware audio soak remains tracked as a human/hardware gate; it does not replace the H4
  CI gate.

## Green command

```
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
ctest --preset ci -R YesDawMidiTimingCheck
```

The first code checkpoints now exist: `YesDawMidiTimingCheck`, Project-owned MIDI Clips/Notes with
schema v3 persistence, piano-roll Note edit commands with undo/redo bit-identity coverage, and
deterministic MIDI-effect Nodes for transpose + scale-map event transforms. The hosted-instrument Event
bridge now drives transformed Note Events through `PluginNode`'s RT lane. MPE boundary allocation now
assigns stable concrete voice addresses before flattening while preserving explicit voice hints and
avoiding overlapping future explicit voice reservations. The H4 close-out pass audited the roadmap,
ADR-0017, ADR-0009, ADR-0010, `loop/horizon.md`, `STATUS.md`, and the blocking test evidence. H4 is
complete when the final close-out docs commit is green in CI; H5 remains unopened until Dan starts that
boundary.

## Review follow-ups (2026-06-27 adversarial review, full-close pass)

An adversarial review (build + mutation tests + 9 static dimensions, every finding re-verified) confirmed
the MIDI math and the gate are correct, but found the gate weaker than the prose claimed. Checkpoint 1
hardened it: the three named negative controls now exist as real tests and an integrated
boundary+tempo+PDC case was added (gate = 16 cases / 289 assertions). Two beyond-criterion items are
sequenced as ADR-gated checkpoints under the full-close pass:

1. **Runtime `MidiClip` -> engine source Node.** `flattenMidiClipNotesForBlock` has no production caller —
   it is invoked only from tests — so a loaded Project with MIDI Clips emits no events at playback. Needs a
   new ADR for the clip-event source contract and an RT-safe (non-allocating) flatten before code lands
   (this slice changes how events enter the graph, a Stop-for-ADR condition).
2. **ADR-0010 prefix-sum tempo lookup.** `tickToFrame` is an O(n) per-call scan with full per-call
   re-validation, diverging from ADR-0010's mandated cumulative prefix sum with O(log n) binary search.
   Lands as ADR-0010 conformance, guarded by a bit-identity test against the current closed-form result.

Minor follow-ups tracked (none reachable through the current H4 surface): `pdcShiftFrames` event-shift path
untested; LinearRamp + `floor()` 1-sample rounding untested; >1024 events/Block trips an audio-thread
RT_FATAL; `quantizeNote` snaps start only; persisted `MidiClip.timeBase` is ignored at flatten; MPE
zero-length / cross-port allocation edges; `ImpulseInstrumentNode` drops impulses past its 64-slot ring.
