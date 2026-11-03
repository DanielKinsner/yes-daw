# 0079. Live MIDI note schedules and voice reconciliation

- **Status:** Accepted (in-scope implementation decision under Dan's 2026-11-03 editing mandate; engine/Audio-thread critic accepted the bounded design after review).
- **Date:** 2026-11-03
- **Deciders:** implementation agent; independent engine/Audio-thread critic. Acceptance is design approval; implementation and mechanical proof are still required.
- **Related:** ADR-0006, ADR-0009, ADR-0017, ADR-0046, ADR-0078.

## Context

Committed piano-roll note edits currently construct a replacement PlaybackEngine. Transport is restored, but the replacement cannot preserve the running instrument and FX state. The audio placement lane already publishes immutable schedules through the ordered command queue and retires them off the Audio thread. Dan authorized the editing improvements and headless-only verification; no desktop drive or audio device is authorized for this run.

## Decision

1. **Keep graph and stored Project contracts.** Add an immutable per-Clip MidiSchedule with the existing flattened absolute-frame event timeline and control-built note-span lookup metadata. Initial projection and updates use the same builder. Existing Clip-keyed sources, Track merge, instrument, routing, time bases and saved representations remain intact.
2. **Reuse ordered ownership.** A POD SetMidiSchedule command transfers one immutable schedule into the Runtime's existing command lane. The existing source exchanges its pointer on the Audio thread; the previous schedule is retired through the existing queue and exact generation fence (processedGen strictly greater than retiredAtGen). Rejection retires the new schedule. No allocation, lock, I/O, logging, deletion or shared ownership decrement is introduced on the Audio thread.
3. **First eligibility is narrow.** Live publication applies to note-content changes on one existing MIDI Clip whose identity, owning Track, placement, time base, settings and controller events remain unchanged, with no enabled MIDI FX on its Track and no other Project changes. The first slice refuses Clip-content loops, repeated/ambiguous full voice addresses and wire-ID collisions on the affected Track. Transport looping remains supported. Unsupported edits keep the current rebuild path.
4. **Honor existing VoiceAddress.** Native instrument voices retain the NoteOn address. NoteOff matches every specified noteId, portIndex, channel and key; -1 remains a wildcard. This implements ADR-0009/0017 without changing the Event layout, saved format or checked-in goldens, and prevents a release in one Clip from releasing an unrelated same-pitch voice.
5. **Reconcile at the actual audio boundary.** Track a fixed-capacity set of NoteOns actually emitted by each source. At schedule replacement, retain active notes whose address, pitch and velocity remain active in the new schedule; adopt changed note ends without retriggering unchanged voices. Release removed or changed active notes through addressed NoteOffs at offset zero. A changed or newly added onset already before the current boundary is not chased/retriggered in this first slice; it sounds on its next ordinary onset (including the next loop). Unchanged controller history and sustain behavior remain intact; never send a blanket reset.
6. **Admit only end-to-end bounded updates.** On the Control thread compute E, the total count of all ordinary flattened events from every Clip source on the affected Track (including controllers and note ends), as a conservative upper bound for any rendered window. Keep R as the maximum whole-Track NoteOn count across the initial installed schedules and every queued/admitted predecessor for this engine; never shrink it until a rebuild. Admit only E + R <= 1024 and only if the predecessor bound is also valid. This protects every per-source slot and the shared Track merge, rather than assuming a source's capacity protects its downstream merge. Dense or ambiguous Tracks take the rebuild path. Live audition events travel separately in ProcessArgs::liveEvents and are not merged into these slots. Retain sample-sorted event order; at offset zero, controllers precede reconciliation Offs, which precede ordinary NoteOns. Never put a later release before an earlier sample.
7. **Use bounded immutable lookups.** Each admitted schedule indexes spans by full Note EntityId plus occurrence (zero in the non-content-looping first slice), with an event-to-span index and a sorted span lookup. Active state owns copied identity/address/pitch/velocity data, never a borrowed pointer into a retired schedule. Reconcile at most 1024 tracked notes by binary lookup at the actual blockStart. Admission bounds ordinary processed events; unsupported ordinary overflow uses an end-index lookup rather than scanning the entire overflowing window.
8. **Stopped and discontinuous playback.** A stopped engine without audition/tails may defer Runtime command application until Play; this is pending application, not a Control-thread drain. At the first applicable graph segment after seek, Transport loop or stop/resume, issue addressed releases for tracked source notes before the new segment's ordinary onsets. A clipsSilenced segment must service pending releases before returning. Never reset controllers, live audition or unrelated source voices. Multiple schedule swaps before one render reconcile the actually emitted active set against only the final installed schedule.
9. **Preserve adoption/durability and converge failures.** Compile and certify on the Control thread, then write the durable snapshot, adopt the current Project/undo, and enqueue publication. Compile/save failure leaves model, undo and audio unchanged. Queue refusal triggers the existing rebuild convergence; failed convergence reports an explicit playback error rather than silently leaving stale sound. Runtime installation checks the exact existing graph/source identity. A successful enqueue's later install rejection is counted/read back so the owner can converge and report it. Keep Transport, instruments and FX untouched on successful live publication. Do not claim removal of persistence cost in this unit.

### Schedule ownership

| State | Sole owner / action |
|---|---|
| Initial schedule | Source Node; destroyed off-thread with graph |
| Built, not enqueued / enqueue refused | Control-side unique owner destroys it |
| Enqueued successfully | Command queue owns it |
| Installed | Source owns new schedule; old schedule transfers to retirement |
| Invalid target | New schedule transfers to retirement; installed source unchanged |
| Retired | Existing retirement queue / janitor; free only after strict generation fence |
| Queued during shutdown | Stopped Runtime destructor frees uninstalled schedule |
| Graph destroyed | Node frees only its currently installed schedule; retirement owns previous ones |

The existing retirement-capacity check applies before every new schedule command is drained. Node destruction and retirement never share ownership of one pointer.

## Alternatives

- **Recommended and selected:** publish note data into the existing source, with explicit voice reconciliation.
- Copy state into a replacement graph: larger cross-node serialization/lifetime contract and poorer isolation from unrelated graph changes.
- Rebuild on every note edit: retains the observed central editing cost and loses running DSP state.

## Verification

Pure tests prove addressed and wildcard release; source transition offsets and release pressure; queued, rejected and retired schedule ownership; and a model-level playing edit with no engine replacement and bit-identical held-note/FX output before the changed future onset. The old path must fail the continuity assertion. Run the existing headless suite and RTSan/TSan CI; build the app without launching it. Native input and hardware evidence remain pending under the explicit headless restriction.

## Consequences

This is an incremental MIDI live lane, not live controller editing or a graph-topology rewrite. Later work can widen eligibility after mechanical proof. Existing saved Projects retain their meaning. Golden outputs and existing RT annotations are not rewritten.
