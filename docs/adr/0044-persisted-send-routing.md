# 0044. Persisted send routing — sends live on the Track

- **Status:** Accepted
- **Date:** 2026-08-10
- **Deciders:** Dan (standing usable-DAW directive), build agent
- **Related:** ADR-0038 (FX suite / mixer strips), ADR-0039 (automation lanes, SendLevel role),
  ADR-0042 (stereo law — bus width derives from its taps), the usable-DAW P1 backlog in
  `docs/reviews/2026-08-09-shipped-parity-gap-audit.md`.

## Context

Buses are fully persisted (`buses` table, strip state, FX chains, automation) and the mixer
projection can build track→bus send graphs — but the send ROUTES themselves exist only as
runtime config (`ProjectMixerProjectionConfig::sendRoutes`, populated per-render by options).
No route survives save/reopen, and no UI can create one. A usable DAW needs persisted,
undoable, user-editable sends.

## Decision

**Sends are rows on the owning Track.**

```
struct SendRow
{
    EntityId     id;                 // globally unique, joins the duplicate-id scan
    EntityId     busId;              // must reference an existing Bus
    MixerSendTap tap = PostFader;    // PreFader | PostFader
    float        linearGain = 1.0f;  // same validity law as strip gain
};
std::vector<SendRow> Track::sends;   // ordered; ordinal = index (ADR-0039 SendLevel paramId)
```

The law:
- **Single source of truth**: playback, offline render, and export derive
  `config.sendRoutes` from `project.tracks[].sends`. The low-level
  `OfflineRenderOptions::sendRoutes` seam remains for engine tests but is never populated by
  the app model once project-derived routes exist; project routes and option routes are
  concatenated by the caller that owns both (the app model passes only project routes).
- **Undoable commands** (trivially-copyable, same contract as every ProjectEditCommand):
  `AddBus`, `RemoveBus`, `AddSend`, `RemoveSend`, `SetSendLevel`. Bus rows get their own
  whole-vector diff family (like tracks); send edits ride the existing track-rows family
  (`Track::operator==` already covers `sends`).
- **Referential rules**: `AddSend` requires an existing target Bus and rejects self-referential
  or duplicate (track, bus) pairs. `RemoveBus` is refused while any Track sends to it and
  while any FX/automation targets it (mirror of empty-only track removal — no cascading
  deletes in the alpha).
- **Persistence**: new `sends` table (id, track_id, bus_id, position, tap, linear_gain),
  schema version 8 → 9. Old bundles open with empty send lists (additive migration); v9
  bundles refuse to open in v8 code by the existing version gate.
- **Automation**: SendLevel lanes keep ADR-0039 semantics — `paramId` is the send ordinal on
  the owning Track. Removing a send does NOT renumber ordinals of later sends in the alpha;
  a lane pointing at a removed ordinal is silently inert (documented, gate-covered).
- **Stereo**: unchanged ADR-0042 law — bus width derives from its taps.

## Consequences

- The mixer UI can offer "+ Bus", per-strip send rows (destination, level, remove), and the
  bus strips become reachable targets for real routing — the last P1 gap.
- Schema bump means new bundles are unreadable by older builds (accepted; alpha).
- The projection code is untouched: it already consumes routes; only the route SOURCE moves
  into the Project.

## Verification

- Randomized undo/redo property gate extends to the five new verbs and stays bit-identical.
- Bundle round-trip: a project with buses + sends reopens equal; a v8 bundle opens with empty
  sends.
- Audibility gate: a track with fader at zero and a post-fader send renders silence through
  the bus; a pre-fader send renders audio (tap law bites).
- Shell gates: + Bus creates a persisted Bus; the send row's level slider persists an
  undoable SetSendLevel; RemoveBus is refused while routed.
