# 0087. Flat contiguous Track folders and one visible row projection

- **Status:** Accepted
- **Date:** 2026-11-05.
- **Deciders:** Dan's authorized Option A scope; root and an independent engine/saved-data critic (ADR-0049 / ADR-0078).
- **Reviewed source snapshot:** `ceac834dd41cc0ac5c7c9a18498fd80f1b562998`. Refresh implementation anchors and schema numbering before dependent code.
- **Related:** ADR-0011/0012, 0049, 0052, 0060/0061, 0064/0066, 0078, 0082 and 0083; [Edit-group contract](0086-linked-edit-groups.md) and the authorized editing plan.

## Context

Dan authorized collapsible Track organization independently of linked editing. Current Track is an
audible lane with strip, instrument, output and sends (`Project.h:491`). Current rail callbacks,
timeline lanes, stacked automation, keyboard traversal and accessibility interpret visible integers
as Project Track indices. Filtering paint alone would make input target the wrong hidden Track.

Project.tracks is also canonical engine/mixer/interchange order. It persists by snapshot insertion
order and reads ORDER BY rowid (`ProjectBundle.h:3289,4024`). A second folder order or an audible
folder Track would create conflicting authority. This is a stored-identity/order and view-projection
decision, not permission to alter routing, mixing or the Audio thread.

## Options considered

A. **Selected and recommended: separate flat Folder metadata, contiguous canonical members and one shared visible
row projection.** It gives familiar collapse/reorder behavior without another engine Track type.

B. Folder as a Bus/Track subtype. This could support summing controls but entangles organization
with routing, strip state, graph ownership and plugin/PDC behavior outside this scope.

C. Independent nested folder tree/order. More flexible organization, with multiple order lists,
cycle/reparent rules and substantially larger hit/drop/Undo semantics. Defer nesting.

## Decision

### Saved identity and canonical order

A **Track folder** has a globally unique nonzero EntityId, name and unique existing member Track
IDs. Names follow the current nonempty, at-most-127-byte Track command policy. Membership is separate
from Edit groups. A Track belongs to at most one folder. Folders are non-nested and non-empty; their
members occupy one contiguous block in Project.tracks. The header position is derived from the
first member in canonical order. Store no independent folder position, parent ID or member order.

Create from selected Track IDs by gathering them at the first selected position, preserving their
relative order and the relative order of all other Tracks. Validate the complete resulting folder
relations/order; a move that would split an existing folder refuses whole rather than silently
ungrouping other Tracks. Reparenting names destination membership explicitly and validates the
whole candidate; it is not inferred from whichever visible row happened to share an index.

| Command | Exact behavior |
|---|---|
| Create folder from B,D in A,B,C,D | Canonical A,B,D,C; members B/D; one Undo step |
| Rename | Saved metadata only |
| Remove folder | Ungroup its Tracks; retain their order, Clips, automation and routing |
| Remove Track | Existing media/Take/sidechain refusal policy remains; successful removal cleans membership and an empty folder in the same atomic step |
| Duplicate Track | Copy lands immediately below its source and joins the same folder; it does not silently join an Edit group |
| Move folder | Move its complete contiguous canonical Track block, retaining member order |
| Move/reparent Track | Explicit destination membership and insertion position; preserve remaining folder blocks or refuse whole |

The old index-only reorder API must reject an invalid contiguous-membership result. Projects with
no folders keep existing reorder behavior. No folder-level mute/solo/arm/gain/pan or Bus is added.
The folder header menu distinguishes Select member Tracks, Move folder and Remove folder; ordinary
Delete on a header must never delete hidden contents.

### One row projection

Build one pure immutable **visible arrangement row projection** from canonical Tracks, folder
relations and per-Project collapse state. A row is explicitly Track or Folder, with stable EntityId.
Only a Track row has a Track target/canonical index. The projection provides forward/reverse lookup,
visible positions, containing folder for hidden Tracks and canonical Track indices independently
from visible indices. Invalid metadata produces a named refusal, not a partial reordered view.

Every arrangement consumer uses that same projection:

- Track rail paint, heights, selection, mini-controls, resize/reorder, context menus and meters.
- Timeline Track presentation, Clip lane mapping, lane geometry, hit testing and audio/MIDI drops.
- Stacked automation canvases, lane choosers, keyboard traversal, selected-object reveal and scroll.
- Painted control records/accessibility, stable activation targets and debug/probe row observations.

Do not use project.tracks[visibleRow] after collapse. Capture stable Track/folder IDs for callbacks
and revalidate on activation. Semantic control identity uses the entity; an index in a probe is an
observed visible position, never authority to edit that index's current Track. A folder header has
no Clip/recording/automation destination. Audio/MIDI drop or cross-Track Clip move onto a header
refuses before any import/adoption; it never falls through to the next or first hidden member.

Expanded rows for A,B,D,C with F={B,D} are A,F,B,D,C. Collapsed rows are A,F,C. Clicking C resolves
Track ID C; collapsing never redirects it to hidden B or D. Row heights and scroll positions are
derived from these rows, including automation stacks only under visible Track rows.

Retain canonical Project.tracks for playback, mixer, recording identity, file export and SMF order.
Never feed a filtered list back to Project or engine compilation. A fold hides no audio/MIDI owner.

### Collapse, selection and view state

Collapse is per-Project view state in the existing view-state.txt sidecar, keyed by folder EntityId.
Use one `folder.<32 lowercase hex ID><TAB>0|1` record; missing means expanded. Ignore malformed,
unknown or deleted-folder keys. Preserve other known view keys when rewriting the complete record
through the existing temporary-sibling/rename mechanism (`UiAppModel.h:1785-1827`,
`MainComponentCommands.cpp:231-340`). No Project schema column, undo row or user preference stores
collapse. A failed sidecar write must not corrupt saved Project metadata or a previous valid file.

Collapse preserves hidden Clip/Track selections and any pre-existing explicit Time-selection Track
mask. New pointer/keyboard traversal uses visible Track rows; it does not implicitly add all folder
members to Time selection. A deliberate Reveal command may expand the containing folder and scroll
to the stable target. Collapsing a folder with a selected member must not immediately re-expand it.

Hidden Edit-group members still participate under the separate linking contract. Folder collapse
does not enable/disable a group, change its seeds or alter edit membership. Header activation is a
folder action, not an ambiguous Track selection. Accessibility announces folder name, collapsed
state and member count, exposes Expand/Collapse, and follows the same target IDs as pointer input.

### Persistence, lifecycle and engine behavior

The source schema is currently 35. Append the next available migration when the folder storage unit
is implemented; the Edit-group unit or other authorized work may advance it first. The future schema
number is chosen at implementation time. The normalized families are:

| Table | Stored contract |
|---|---|
| track_folders | id BLOB(16) primary key; name TEXT |
| track_folder_members | folder_id/track_id identity; unique track_id; RESTRICT foreign keys to track_folders/tracks; no independent ordering column |

Extend global EntityId validity/collision checks, session allocation, schema-fit/stored semantic
and storage-type validation, whole snapshot reads/writes, and exact folder/Track row diffs. Contiguity
is a cross-row Project semantic check, not something the two FKs establish. Explicit child-row
deletion must precede parent/Track deletion in the existing snapshot transaction. Load never silently
repairs unknown members, duplicate ownership or a noncontiguous folder. Old Projects migrate empty.

Folder CRUD, canonical gather/reorder and member cleanup are complete atomic Project/Undo changes;
rollback restores order and every metadata family together. Extend current whole-step history
navigation and stale-generation checks, including pending Musical-media history adoption. Selection
and view updates follow successful adoption, not a partially changed candidate.
When canonical Track order and membership change together, record one complete organization/order
diff over the affected Track/group/folder row families and apply those coupled values before one
semantic validation. Repeated index-only reorder commands must not expose or validate an invalid
intermediate folder block. Group-only or folder-only edits keep their smaller metadata row diff.

Metadata-only name/membership changes that leave Track order unchanged use the dedicated checked
metadata adoption required by [ADR-0086](0086-linked-edit-groups.md): persist first, then update model/history/context while
retaining the engine, decoded owners, clocks and transport. Collapse is view-only and does not call
Project adoption. An explicit canonical gather/reorder is structural and retains the existing Track
reorder publication law. Do not infer PCM bit identity for a reorder from a collapse/metadata check.

Save/open, autosave/recovery and whole Project copies preserve folder rows. Save As/Save a Copy keep
coherent identities under their existing bundle law. Extend layout templates additively to retain
folder names/membership, allocating fresh folder IDs and remapping every member to the new Track ID.
Continue excluding Clips, Assets, Takes and automation. Do not copy stale collapse-ID keys into a
new template Project; absent keys leave its folders expanded.

## Mechanical proof and consequences

Pure value/storage tests establish validation, global ID collisions, old-schema migration, stable
gather, whole-row Undo/Redo, failed persistence, template identity remapping and copy/autosave/recovery.
Pure projection tests establish A,F,B,D,C -> A,F,C, variable heights/automation rows, reverse lookup,
hidden targets and Folder-header non-target behavior. One real audited offscreen journey must
create/fold a folder, act on visible C, edit a hidden linked member, refuse a header drop, reveal by
ID, save/reopen view state and Undo metadata. It must assert actual persisted targets and no peer or
desktop audio request/open, rather than supply expected model results through a mock.

During deterministic headless playback, collapse/metadata edits cause zero playback replacements or
callback removal and retain PCM/contiguous timeline Frames. Hidden members still render/export in
canonical order. Structural reorder receives its separate existing-contract test. Keep existing
goldens, sanitizer/resource/timing gates and native evidence boundaries unchanged. Implementation tests and CI remain required; accepted architecture alone supplies no execution evidence.
ADR review can proceed while root manages pending CI.

The result is familiar organization with one order authority. Accepted costs are no nesting, no
folder mix strip and explicit membership on boundary moves. During implementation, root
adds Track folder and visible-row vocabulary to CONTEXT. Ordinary layout/control work needs no
additional ADR unless it changes the saved or architectural contracts above.
