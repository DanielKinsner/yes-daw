# 0051. Compressor sidechain: one Track or Bus key, tapped pre-fader, stored per insert

- **Status:** Accepted (agent, 2026-10-06, under ADR-0049's implementation-ADR clause, after a separate
  agent critic; its three required changes — insert removal, bypass, Compressor-only field — are folded in
  below; committed alone, before any sidechain code)
- **Date:** 2026-10-06
- **Deciders:** build agent (proposer), separate agent critic
- **Related:** the plan's G4.4 ("Sidechain reachable: compressor sidechain source chooser; gate: render
  golden") in [`docs/plans/2026-09-01-real-daw-ground-up-plan.md`](../plans/2026-09-01-real-daw-ground-up-plan.md);
  **ADR-0014** (sidechain pins are real graph inputs; one pin, one stream; never audible; follows its
  source's mute and solo; PDC convergence) — this ADR keeps every one of those rules; ADR-0034 (sidechain
  routing belongs to Track/Bus routing, never a loose UI flag); ADR-0038 (the Compressor; "sidechain input
  deferred"); ADR-0042 (stereo strips).

## Context

ADR-0014 settled how a sidechain behaves in the graph and left four things open: where the choice is
stored, which strips may key what, where on the source strip the key is taken, and the UI. Today nothing
in the Project names a key: the Compressor has one input and detects on its own audio, the "SC" badge on a
strip is decorative, and the only keyed node (`SidechainGainNode`) is a test VCA owned by its own strip.
The plan's G4.4 asks for a source chooser on the Compressor that a user can hear, save and undo. Storage
and the source rules are hard to reverse once bundles carry them.

## Options considered

**Where the key is taken on the source strip**

1. **Pre-fader, after the source's inserts — the point a pre-fader send taps (chosen).**
   - Pros: riding or pulling down the source's fader never changes the ducking; the "ghost trigger"
     technique (a kick with its fader down still keys the bass) works; mute and solo still follow
     ADR-0014 because they act at the strip's source sum, upstream of this point.
   - Cons: a user who wants the key to follow the source's fader must use a send to a bus and key from
     that bus.
2. **Post-fader output.** Pros: "what you hear keys it". Cons: the fader changes the ducking; no ghost
   trigger.
3. **A per-insert pre/post choice.** Pros: both. Cons: one more stored field and UI for a first version;
   it can be added later without breaking the stored default.

**Where the choice is stored**

1. **A new table `fx_insert_sidechain (insert_id, source_entity)` in schema v32, a row only for a keyed
   insert (chosen).** Pros: additive, no change to existing rows; the key lives with the insert it
   belongs to, as ADR-0034 asks. Cons: one more table to migrate and validate.
2. **A nullable column on `fx_inserts`.** Pros: one row. Cons: an `ALTER TABLE` on every existing bundle
   for a field most inserts never use.

**When a key's source strip is removed**

1. **Refused while in use, with a reason (chosen)** — exactly as a Bus with routed sends or outputs is
   refused today. Pros: one rule; undo stays a single-domain edit. Cons: the user clears the key first.
2. **The removal also clears the key.** Pros: one click. Cons: one undo step must then span Track, Bus and
   FX rows together.

## Decision

- **Consumer:** the built-in **Compressor** insert on a Track, Bus or the master strip. No other kind takes
  a key in this ADR.
- **Source:** at most **one Track or Bus** per Compressor (ADR-0014: one pin, one stream). Never the
  master, never the Compressor's own strip, never a strip whose signal already depends on that
  Compressor's output — such a choice is refused as a **routing cycle** before any graph is built. The
  dependency walk follows every routing edge the Project has: `Track.outputBusId`, `Bus.outputBusId`, every
  send (pre- or post-fader, from a Track or a Bus) and every other Compressor key. The chooser disables a
  source that would make a cycle, and the verb refuses it independently (as the routing verbs do).
- **Tap:** the source strip's **pre-fader signal after its inserts** (a pre-fader send's tap point) —
  deliberately after the inserts, so the source's own EQ or dynamics shape the key (a de-esser keyed by an
  EQ'd copy). It follows the source's mute and solo (ADR-0014; both act at the strip's source sum,
  upstream of the tap); it is never audible and never metered as audio.
- **Key shape:** stereo; the detector reads `max(|L|, |R|)` of the key (stereo-linked, the same law the
  Compressor's own detector uses). A mono source feeds both key channels.
- **Latency:** ADR-0014 as written — the Compressor is a PDC convergence point; the main input and the key
  arrive at the same compensated sample.
- **Model and undo:** `FxInsert` gains a `sidechainSourceId` (invalid = no key: the Compressor detects on
  its own audio, as today). It is valid **only on a Compressor**: `FxInsert::isValid()` and the bundle
  loader reject any other kind carrying one. One verb, `SetFxInsertSidechain (owner, insert, source)`,
  sets or clears it as **one** undo step; it refuses a non-Compressor insert, an unknown source, the
  master, the insert's own strip and a cycle. The key is part of its insert: removing the keyed Compressor
  removes its key in the same step (undo restores both), and a reorder within its chain keeps it. Removing
  a Track or Bus that keys any Compressor is refused while it does, with the reason on the status line.
- **Bypass:** a bypassed Compressor keeps its key edge — the graph's PDC and cycle analysis see the same
  edges whether it is enabled or not — and passes its main input through unchanged; the key is simply
  not used while it is bypassed.
- **Persistence:** schema **v32** adds `fx_insert_sidechain (insert_id BLOB PRIMARY KEY REFERENCES
  fx_inserts(id) ON DELETE CASCADE, source_entity BLOB NOT NULL)`, a row only for a keyed insert, written
  and removed with its insert; a bundle whose key names a missing strip, or keys a non-Compressor, fails
  validation like any dangling routing reference. Older bundles migrate with no rows (nothing keyed).
- **UI:** the Compressor's editor gains a **Sidechain** chooser — None, then the Tracks, then the Buses,
  self and cycle-making strips disabled — and the strip's "SC" badge shows exactly when its Compressor is
  keyed.

## Consequences

- **Positive:** ducking and keyed compression are reachable, saved and undoable; every ADR-0014 rule
  (pins, PDC, mute/solo, never audible) holds; an unkeyed Compressor renders exactly as before.
- **Negative / accepted costs:** the Compressor becomes a two-input node, so the graph binder and its
  "all multi-input nodes bound" law learn a second kind; a keyed source strip cannot be removed until its
  key is cleared; no post-fader tap, key-listen or external-input key yet.
- **Follow-ups:** `CONTEXT.md` gains **Sidechain key**. Gates: a render check that a keyed Compressor's
  gain reduction follows the key's envelope while the main input is steady, and that the same render
  unkeyed is bit-identical to today's; PDC alignment of the key; mute and solo of the source silencing the
  key; the verb's refusals and one-step undo; v31→v32 migration and round-trip; the editor's chooser and
  the badge through the real app.
