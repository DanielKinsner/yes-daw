# 0086. Explicit Edit groups with exact Clip correspondence

- **Status:** Accepted
- **Date:** 2026-11-05.
- **Deciders:** Dan's authorized Option A scope; root and an independent engine/saved-data critic (ADR-0049 / ADR-0078).
- **Reviewed source snapshot:** `ceac834dd41cc0ac5c7c9a18498fd80f1b562998`. Refresh implementation anchors and schema numbering before dependent code.
- **Related:** ADR-0002, 0010, 0011, 0012, 0017, 0036, 0049, 0052, 0060, 0078, 0080, 0082 and 0083; the authorized editing-improvements plan.

## Context

Dan authorized linked Track editing after Musical audio. An Edit group needs durable identity,
membership and exact operation boundaries before it can change a destructive command's scope.
Folders are a separate organizational feature; neither grouping nor collapse creates a Bus or mixes
parameters. This decides identity, matching, atomicity and storage. Implementation and verification remain separate delivery steps.

Current Project has no Edit-group family (`Project.h:1175`). Object selection and its active anchor
are separate model values (`UiAppModel.h:4310-4404`). Placement already builds a candidate Project
and undo stack, maps each native destination, and persists once, but its neighbour effects are still
applied one target at a time (`UiAppModel.h:7804-7909`). Split still accepts one untyped Tick and
audio Trim is not a generic mixed-Clip operation (`8530-8610`, `8884-9008`). Linking must not call
these public methods repeatedly or give one raw Tick to unlike time bases.

## Options considered

A. **Selected and recommended: explicit saved Edit groups and exact native corresponding Clips.** Scope is
predictable, independent of paint order, folders and mix controls. It deliberately does not infer
that differently timed or merely overlapping Clips are the same phrase.

B. Match all physically overlapping Clips on member Tracks. This accommodates loosely aligned
layers but makes edge-crossing/multiple overlaps ambiguous and expands destructive scope.

C. Use folder membership as edit membership. This reduces controls but makes organization and
hiding change edit scope. It is not the proposed design.

## Decision

### Identity and saved state

An **Edit group** is a separate Project value with a globally unique nonzero EntityId, a name, an
enabled flag, and unique existing Track IDs. A Track belongs to at most one Edit group, including
disabled groups. There are no transitive groups or Clip-content links. Creating a group needs at
least two Tracks; later Track removal may leave one member. Remove an empty group in that same
transaction. Names follow the existing Track naming command policy (currently nonempty, at most
127 bytes); duplicate names are allowed because IDs carry identity.

Creation, rename, enable/disable and membership are saved and undoable. **Suspend linked editing**
is a visible session toggle, initially false, reset on a fresh application session and not saved
in Project rows or user preferences. It changes resolution only. Do not borrow Alt, Ctrl or Shift
from existing copy, snap-inversion or selection gestures; no new default chord is required.

### Selection seeds, effective units and anchor

Keep actual explicit selection seeds, the active anchor and derived linked targets separate. With
linking active for an enabled group, a Clip's selection unit is the exact tuple
`(group EntityId, TimeBase, native start, checked native end)`. Otherwise its unit is its own Clip
EntityId. The effective Object selection/highlight is the closure of the seed units under current
group membership. No painted or implicit counterpart becomes a seed just because it is effective.

The existing Shift-click selection toggle remains Shift-click (`TimelineInputComponent.h:652,675`);
Ctrl remains snap inversion. Apply these rules to actual clicked/hit Clip IDs:

- Plain click on an already effective unit preserves every seed and changes the active anchor to
  the actual clicked Clip, even when that Clip was implicit. Plain click on an unselected unit
  replaces the seeds with only that clicked Clip ID, then derives its effective closure.
- Shift-click on an effective unit removes **all** seeds belonging to that unit, including when the
  clicked Clip was implicit. This clears that unit rather than toggling one painted counterpart and
  letting another seed add it back. Shift-click on an unselected unit adds only the clicked Clip ID
  and makes it the anchor; it does not add every counterpart as a seed.
- Marquee replaces the seeds with the actual hit Clip IDs, deduplicated before deriving closure.
  Never pass the resulting resolved paint IDs back as marquee seeds. Preserve the current marquee
  anchor choice from the actual hit sequence; an empty hit set clears seeds and anchor.

After toggle removal, disable/enable, suspension/resumption or membership changes, prune nonexistent
seeds and rederive from the surviving actual seeds. Keep the anchor when it remains in that effective
closure. Otherwise choose the first surviving seed in canonical Project Track order, then full Clip
EntityId order. With no surviving seed there is no anchor or effective selection. An anchor may be
implicit while its unit has a surviving seed; an anchor alone never sustains a selection.

Successful edit receipts carry source-to-result **explicit-seed provenance** separately from all
resolved targets and generated IDs. Move preserves surviving seed IDs. Copy records provenance in
the captured clipboard without changing selection; copy-move/Duplicate map only the explicit source
seeds to their selected generated copies. Split preserves the existing original/left selection:
each surviving explicit seed retains its original ID, while new right halves and implicit members'
halves are not promoted to seeds. Delete/Cut prune removed seeds. Apply the verb's mapped anchor
when it remains effective, otherwise the deterministic surviving-seed rule above. Publish these
selection changes only after successful adoption. Rebuilding selection from every generated target
would lose intent and is forbidden.

### Exact target resolution

For every explicitly selected Clip on an enabled group's member Track, include all Clips on member
Tracks with the same TimeBase, identical native timelineStart, and identical checked native end.
Use full signed 64-bit native values, including values above 2^53; no floating cast, epsilon,
displayed/floored Frame, Asset, name or first-row heuristic establishes correspondence. Include
every exact coincident layer, across audio and MIDI when those native values actually match.

Deduplicate by full Clip EntityId. Return deterministic Project Track order, then full Clip ID,
with the active anchor separately identified. A Track with no corresponding Clip adds no target;
a missing group Track or stale captured target is an invalid operation, not an instruction to edit
the remaining subset. Zero-length native spans may resolve; each verb retains its own eligibility.

Explicitly selected unlike time bases remain legal under existing typed operation rules. They do
not automatically correspond. A move preserves each Clip's native length/content and uses the full
Tempo map at its destination. Equal requested Frame displacement is not a sample-exact mixed-group
end-alignment promise, and linking must not silently convert or stretch a member.

Freeze the explicit seeds/provenance, resolved IDs, source rows, Track destinations and relevant
attachment/map/group state for a command or gesture. No target expansion occurs against partially
moved, copied or split rows. Destination Paste/Repeat consumes that command's frozen receipt without
destination-group expansion; a later command resolves afresh from then-current seeds and groups.
The shipped placement gesture commits on mouse-up (`TimelineInputComponent.h:962-1260`); preserve
that simple one-commit path. Capture target identity at gesture start and validate it at commit;
foreign Project/target changes refuse the whole commit. If a later live-update gesture is needed,
its own successful updates must advance its expected state; a fixed editSerial check must not reject
the second update. This proposal does not add per-drag publication or a generic gesture platform.

Hidden or collapsed member Tracks participate identically. Show/count linked targets and name
hidden affected Tracks before destructive action. Grouping does not expand Time selection or its
explicit Track mask, Track-only clicks, piano-roll Note edits, solo, mute, arm, gain, pan or routing.

### Commands and typed boundaries

The delivered linked verbs are Move/Nudge, Duplicate/Repeat, Copy/Cut/Delete, Split, then audio Trim
left/right. Each command resolves once and validates the entire set before any durable change.

| Operation | Contract |
|---|---|
| Move/Nudge | Use the existing checked Project-Frame displacement and each member's native destination. Preserve native lengths and full MIDI payload. Track changes use the current explicit-selection canonical lane-offset law. |
| Copy/Cut | Capture the resolved set and explicit-seed provenance once into the existing complete native clipboard. Cut adopts the clipboard and prunes removed seeds only after successful durable removal. |
| Paste/Repeat | Consume the frozen clipboard/copy receipt and map its explicit-seed provenance to selected outputs. Do not expand this command through destination Edit groups; future commands resolve fresh. Fresh Clip/Note/controller IDs and actual canonical repeat bounds remain required. |
| Duplicate | Resolve the source once and create all copies together. Map explicit source seeds to selected copy IDs; implicit copies are targets, not new seeds. Never recursively expand generated copies. |
| Delete | Delete the entire target set under existing Take/other eligibility rules and prune its removed seeds; no partial member deletion. |
| Split | Accept an absolute Project-frame boundary at the UI boundary, map independently into every member's native coordinates, and require a strict interior split in every member. Preserve existing MIDI crossing-Note/controller/loop behavior and audio Asset-window, reverse and cross-rate rules. Preserve original/left seed provenance independently of every generated half. |
| Audio Trim | Require that every resolved target is audio and supports the requested edge edit. A mixed audio/MIDI set refuses whole with a named reason; MIDI-Clip Trim is a separate feature. Map each absolute requested edge independently. |

Refresh the native audio adapters after the Musical-audio work. Current raw-Tick Split, scalar
source-length helpers and audio-only edit-mode routines are not proof of Musical-audio eligibility.
Existing linked-Take refusals are by Clip ID, not by shared Asset. No operation rewrites Asset bytes,
Take/Comp metadata or changes native meaning to make an unsupported target fit.

### Neighbours and automation

Plan neighbour effects from original rows as one batch. Exclude **all** operation target IDs,
including generated target fragments/copies where applicable, from neighbour processing. Deduplicate
coincident gap effects and process each neighbour/affected interval once, not once per selected Clip.

For No overlap, form the union of the actual placed spans per Track before trimming neighbours.
Unlike time bases require checked absolute Frame spans/comparisons and independent mapping back to
each neighbour. Ambiguous precision refuses whole; no epsilon or rounded-Frame equality. Preserve
the established reverse/cross-rate/Musical fragment rules, fresh tail IDs and Take-linked neighbour
refusals. MIDI-neighbour policy is unchanged by adding Track links.

Initial linked Shuffle admits one distinct native-exact target span per affected Track; coincident
duplicate spans count once. Multiple distinct spans, including unlike native spans on one Track,
refuse the complete command before editing. Apply the existing eligible audio start-based removal
then placement law once per logical span, excluding all targets. Use refreshed native-safe adapters;
linking does not invent a different gap law or a general interval relationship solver. Freeze the
supported Musical-audio gap/boundary oracle before activating that branch.

ADR-0052 stays exact. When enabled, same-Track time moves carry each owning lane's points under that
Clip's native time law, including the existing destination replacement and edge anchors. Supply all
resolved before/after moves, including permitted shuffled neighbours, to one automation-follow
pass. Overlapping targets never move a point twice. Copy, Split, Trim, Delete and cross-Track moves
leave automation alone. Group metadata and suspension never change automation.

### Atomicity, history and engine retention

Build the complete candidate Project and history from current copies, validate all target edits,
neighbours, automation and any required ready-media inputs, then persist/adopt once. Refusal or
failure preserves the **committed** Project, history/cursor, playback, clipboard and selection
(explicit seeds, anchor and effective closure), plus counters that count successful edits/dispatches.
Failure status and diagnostic attempt counts may change. Nonwrapping request IDs, intent and
cancellation generations, and allocation-attempt identities may advance on refusal or failure;
never roll back or reuse those identities to simulate atomicity. One successful command or placement
gesture is one Undo/Redo step, including every changed row family.

Add explicit group-row diffs and normal history recording; include them in whole-step navigation,
grouping and stale-history checks. Current HistoryToken/HistoryNavigation (`ProjectUndo.h:2938+`)
uses identity-bearing history, and a staging copy is a fresh generation. Do not mutate history rows
outside that protocol or transplant captured history during pending Musical-media adoption.
Track lifecycle cleanup must record its group changes in the same complete step; coupled organization
and Track-order changes use the complete organization/order diff defined by [ADR-0087](0087-track-folders.md), so apply and
Undo never validate a half-reparented or orphaned intermediate Project.

Pure group metadata changes must retain the running engine, decoded owners, clocks and transport.
Add a narrowly checked metadata-adoption path with the existing persist-then-model/Undo/context law.
Do not call adoptEditedProjectWithoutPlaybackRebuild: it currently creates/replaces a transport-only
engine (`UiAppModel.h:12646-12675`). adoptScalarEditLive is a pattern, not a generic metadata API.
No Audio-thread allocation, work, locks, I/O or new graph node follows from grouping.

### Persistence, IDs and templates

The current schema is 35 (`ProjectBundle.h:46,1563-1613`). Append the next available migration at
implementation time; do not reserve a schema number here or edit prior migrations.

The normalized families are:

| Table | Stored contract |
|---|---|
| edit_groups | id BLOB(16) primary key; name TEXT; enabled INTEGER constrained to 0/1 |
| edit_group_tracks | group_id and track_id composite identity; unique track_id; foreign keys to edit_groups and tracks with RESTRICT behavior |

Membership is a relation, not another Track order. Resolution/display derives member order from
Project.tracks; deterministic persistence must not accidentally assign semantic meaning to SQL row
order. Parent group order may retain ordinary saved vector order. Extend Project value validation,
hasUniqueEntityIds, detail::projectContainsEntityId, session ID collision checks, schema-fit/stored
semantic/type checks, reads and whole-snapshot writes. Explicitly delete membership rows before
parents/Tracks in the snapshot transaction, then insert Tracks before memberships. Do not let an FK
cascade silently perform an undoable membership edit.

Old Projects migrate to no groups. Save/open, autosave/recovery, Save As and Save a Copy retain the
complete rows. Extend the layout-only template contract additively: preserve group names, enable
and Track relationships, allocate fresh group IDs, and remap member Track IDs through the same map.
Templates still contain no Clips/Assets/automation. Ordinary Project copies keep coherent IDs.

## Mechanical proof and consequences

Required proofs are headless and self-asserting: exact matching/dedup/disable/suspend/hidden members;
native values above 2^53 and unlike bases; group CRUD/migration/Undo/save/reopen/autosave/refusal;
one atomic multi-Track edit through the real shell; No-overlap target exclusion/union; duplicate-span
Shuffle once and distinct-span whole refusal; native Split and mixed Trim refusal; ADR-0052 lanes
once; clipboard destination non-expansion followed by fresh later-command resolution; fresh template
IDs; metadata engine retention/contiguous deterministic playback; stale target/history/persistence
failure preserving committed state and success counters while permitted attempt identities advance.

Selection proofs include seed A resolving implicit B, followed by Shift-toggle on B clearing the
whole unit; plain click on effective B changing only the anchor; disable/suspend retaining actual
seeds rather than B; marquee hits A/B in a three-Track group never promoting hidden C; and
Duplicate/Split receipts preserving explicit-source provenance when linking is subsequently disabled
or suspended. Also cover multiple seeds in one unit, deterministic anchor fallback and no anchor
after its last seed is removed. These are required mechanical cases; implementation results remain separate.

Use one feature smoke per logical change and existing owning suites; new tests do not replace old
goldens or raise resource/timing thresholds. Capture genuine public REDs for observed bugs, not a
missing new API compile error. XML-audit offscreen cases before execution; no native window/device,
desktop input or human testing gate. Root handles existing CI verdicts while authorized next pieces
continue under ADR-0078; this decision adds no global-CI or Dan approval prerequisite for ordinary authorized work.

This unlocks predictable layer editing at the cost of deliberately exact correspondence and a
restricted first Shuffle composition. Later content-instance links, loose-overlap matching, mix
links, named section moves and MIDI Trim remain separate scope. During implementation,
root adds Edit group and Suspend linked editing vocabulary to CONTEXT without rewriting old ADRs.
