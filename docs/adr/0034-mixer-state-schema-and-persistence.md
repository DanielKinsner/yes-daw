# 0034. Mixer-state schema and persistence (Track / Bus / strip state)

- **Status:** Accepted
- **Date:** 2026-06-30
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

**Option 1 is accepted.** Introduce first-class `Track` and `Bus` entities carrying the saved mixer strip
state, persisted by the next ADR-0012 migration after the current schema. `ProjectMixerProjection` must
derive mixer strips from Tracks/Buses rather than from individual Clips.

The grill resolves the open questions this way:

- **Strip-state fields and ranges.** A saved Track/Bus strip carries `name`, `linearGain`, `pan`, `muted`,
  `soloed`, and `soloSafe`. `linearGain` uses the existing `mixerGainIsValid` range; `pan` uses the
  existing `mixerPanIsValid` range (`[-1, 1]`, finite). Meter and loudness readbacks are not persisted
  strip state; they are derived from the engine/metering surface. Sidechain routing remains ADR-0014
  policy and is owned by Track/Bus routing when that routing surface lands; H12 step 6 must not invent a
  loose UI-only sidechain flag as saved Project truth.
- **Track owns Clips.** Audio `Clip` gains a `trackId`. `MidiClip.trackId` is no longer a dangling
  grouping ID; it refers to the same Track table. A Track can own many audio Clips and many MIDI Clips.
  The Master bus remains the implicit final output, not a user-editable Bus row and not a solo target.
- **Bus rows are mixer targets for Returns/sub-mixes.** A Bus row carries its own strip state and can be
  solo-safe per ADR-0014. Bus/Return routing is still graph-projection work; this ADR only decides that
  the saved identity and strip state live in the Project instead of transient UI structs.
- **Migration and backward compatibility.** Do not renumber or rewrite existing migrations. The next
  migration from the current schema adds Track/Bus tables and the audio `clips.track_id` foreign key.
  Existing audio-only Projects receive one default Track (`Audio 1`) and all existing audio Clips are
  assigned to it. Existing distinct `midi_clips.track_id` values become Track rows so MIDI Clip identity is
  preserved. The migration must round-trip through `ProjectBundleDb`, keep old bundles openable, reject
  orphaned Clip/MIDI Clip track references, and include a negative control for invalid strip ranges.
- **Default Track for imports and placement.** A new or imported audio Clip uses the selected Track when
  one exists; otherwise the Project creates or reuses the default `Audio 1` Track. Import must not create
  a new mixer strip per Clip.
- **Automation waits.** H12 stores static fader/pan/mute/solo/solo-safe state. Automation lanes for Track
  fader/pan are deferred to the automation/mixer deepening horizon unless a later ADR brings them forward.
  This keeps ADR-0009's event model intact without adding a half-designed automation target surface here.

A mixing-capable DAW needs real strip identity; doing this at H12 - before more UI assumes the per-Clip
projection - avoids a worse migration later and unblocks H12 step 6.

## Consequences

- **Positive:** mixer fader/pan/mute/solo become real, undoable, persisted state; save/reopen parity for
  the mixer is achievable; the per-Clip strip bug is fixed; H12 step 6 is unblocked; `ProjectMixerProjection`
  gets a correct source of truth.
- **Negative / accepted costs:** a Project-model + bundle-schema migration is a meaty change touching frozen
  ADR-0011/0012 ground; it must be migration-tested with a real negative control (per ADR-0012) and keep
  existing bundles loadable; it enlarges H12 beyond pure UI wiring.
- **Follow-ups:** keep **Track**, **Bus**, and **strip state** aligned in `CONTEXT.md`; write the bundle
  migration + round-trip/negative-control tests; repoint `ProjectMixerProjection` and `UiMixerSurfaceModel`
  at Track/Bus; confirm `MidiClip.trackId` unification doesn't regress H4 MIDI gates.
