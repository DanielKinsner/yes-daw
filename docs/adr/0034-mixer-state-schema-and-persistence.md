# 0034. Mixer-state schema and persistence (Track / Bus / strip state)

- **Status:** Proposed <!-- drafted by the 2026-06-29 adversarial review as a stub; grill before accepting -->
- **Date:** 2026-06-29
- **Deciders:** Dan (owner), build agent
- **Related:** ADR-0011 (asset/clip/project + IDs), ADR-0012 (SQLite bundle schema + migrations),
  ADR-0014 (mixer policy: mute/SIP-solo/solo-safe/sidechain), ADR-0033 (H12 operable session UX),
  H12 plan (`docs/plans/2026-06-29-h12-operable-session-ux-plan.md`), the 2026-06-29 adversarial review
  (`docs/reviews/2026-06-29-adversarial-review-h11-h12.md`), `CONTEXT.md`.

## Context

H12 (operable session UX) commits to a mixer the user can operate **and save**: its persistence-parity
gate requires "imported Assets, Clips, **mixer values**, MIDI Notes, loop/locate state ... survive
save/reopen." But mixer state has nowhere to live today:

- `src/engine/Project.h` has **no** `Track`, **no** `Bus`, and no pan/mute/solo fields. Audio `Clip`
  (Project.h:193) carries only `gain` + fades + timeline placement; it has no track grouping at all.
- `MidiClip` (Project.h:261) has a `trackId` (Project.h:264) but there is **no `Track` entity** it points
  to - it is a dangling grouping id, and the audio side has nothing equivalent.
- `src/persistence/ProjectBundle.h` (the SQLite schema) persists no mixer state.
- H11's `UiMixerSurfaceModel` therefore edits **transient UI control structs** only - fader/pan/mute/solo
  do not reach the Project, are not undoable, and are lost on reopen. H11 projects **one mixer strip per
  Clip**, which is wrong for real tracks (a track holds many clips).

ADR-0014 already decided the runtime **policy** (mute, SIP solo, solo-safe, sidechain) and ADR-0016 the
mute-mask scaling; this ADR is about **where strip state lives and how it persists** - a schema decision.
It is hard to reverse: ADR-0012 froze the bundle schema with a migration harness, and once a layout for
Track/Bus/strip rows ships in a saved `.yesdaw`, every later project must migrate from it. That is *why*
this is an ADR rather than an implementation detail, and why it must land **before** H12 step 6 (inspector
and mixer controls), not be discovered mid-checkpoint.

## Options considered

1. **Introduce first-class `Track` and `Bus` entities that own strip state (recommended).**
   A `Track` owns its Clips (audio Clips gain a `trackId`; `MidiClip.trackId` is unified to point at it)
   and carries strip state: `gain`, `pan`, `mute`, `solo`, `soloSafe`, and sidechain pins (the ADR-0014
   fields). `Bus` is a sibling entity for returns/sub-mixes. Persisted via a new bundle migration; the
   runtime mixer projection derives from Track/Bus instead of per-Clip.
   - Pros: matches how the mixer actually works and what every DAW models; gives strip state one
     unambiguous home; fixes the one-strip-per-Clip bug; `ProjectMixerProjection` becomes
     Track-driven; reuses ADR-0014 policy unchanged at runtime.
   - Cons: largest change - introduces a new entity and a clip->track ownership edge, touches ADR-0011's
     model and ADR-0012's schema; needs a real, round-trip-tested migration.
2. **Persist strip state per-Clip (no Track entity).**
   Extend `Clip`/`MidiClip` rows with `pan/mute/solo`; add a small `bus` table for returns.
   - Pros: smallest migration; reuses existing rows.
   - Cons: cements the wrong model (one Clip == one strip); breaks the moment a track holds two clips
     (which value wins?); creates debt H14 must unwind; the "track fader" is a per-clip fiction.
3. **Defer mixer persistence; keep H12 mixer edits session-only.**
   Narrow the H12 persistence gate to exclude mixer values; mixer is operable but not saved until H14.
   - Pros: no schema change in H12; keeps the H12 cadence small.
   - Cons: "an operable mixer you can't save" is barely operable; the gold-standard mockup implies saved
     mixes; pushes an irreversible decision later, when more code assumes the current shape.

## Decision

**Proposed: Option 1.** Introduce `Track` (owning audio + MIDI Clips) and `Bus` entities carrying the
ADR-0014 strip state, persisted via a new ADR-0012 migration, with `ProjectMixerProjection` deriving the
mixer graph from Tracks/Buses rather than per-Clip. A mixing-capable DAW needs a real strip identity;
doing it at H12 - before more UI assumes the per-Clip projection - avoids a worse migration later and is
what unblocks H12 step 6.

**Open questions to settle in the grill (do not treat as decided):**
- Exact strip-state field set and value ranges (reuse `mixerGainIsValid`/`mixerPanIsValid`?).
- Track vs Bus row layout and the clip->track foreign key; how `MidiClip.trackId` is reconciled.
- Migration shape and backward-compat (ADR-0012 round-trip + negative-control coverage).
- Default track for imported/placed Clips (ties into H12 step 4 import).
- Whether automation (ADR-0009) lanes for fader/pan ride on Track now or wait for H14.

## Consequences

- **Positive:** mixer fader/pan/mute/solo become real, undoable, persisted state; save/reopen parity for
  the mixer is achievable; the per-Clip strip bug is fixed; H12 step 6 is unblocked; `ProjectMixerProjection`
  gets a correct source of truth.
- **Negative / accepted costs:** a Project-model + bundle-schema migration is a meaty change touching frozen
  ADR-0011/0012 ground; it must be migration-tested with a real negative control (per ADR-0012) and keep
  existing bundles loadable; it enlarges H12 beyond pure UI wiring.
- **Follow-ups:** add **Track**, **Bus**, **strip state** to `CONTEXT.md`; write the bundle migration +
  round-trip test; repoint `ProjectMixerProjection` and `UiMixerSurfaceModel` at Track/Bus; make H12 plan
  step 6 depend on this ADR; confirm `MidiClip.trackId` unification doesn't regress H4 MIDI gates.
