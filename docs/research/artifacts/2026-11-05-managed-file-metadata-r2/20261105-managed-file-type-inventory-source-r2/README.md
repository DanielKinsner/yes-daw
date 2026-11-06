# Fixed managed type inventory — revision2 source only

**UNCOMPILED / UNRUN. Independent whole source review is required before root
compiles or invokes this separate metadata tool.** It does not run the R11 runner,
an allocation probe, a target constructor, or target executable code. It grants
no object-size/offset/live/reclaim/32MiB fit, source price, binding or stage GO.
Frozen revision1 and its actual executions remain unchanged.

Revision2 retains all four exact input file lengths/hashes and all21 required
named types from revision1. It adds only this requested metadata closure:

- List the names of the types directly declared inside Newtonsoft.Json.Linq.JObject.
- Select exactly one direct child whose complete name begins with
  `Newtonsoft.Json.Linq.JObject+<GetEnumerator>d__` and has a nonempty ASCII decimal
  suffix. Missing or ambiguous selection fails; no generated numeric suffix is
  guessed. Report that selected type with the existing declared-field/base/signature
  inventory. The names of other children are listed without their fields or code.
- Look for CoreLib ``System.Collections.Generic.List`1+Enumerator``,
  `System.ArrayEnumerator`, and ``System.SZGenericArrayEnumerator`1``.
  Report each one's presence; inventory its fields/base/signatures only when
  present. Absence is data, never a fabricated field row or successful presence.

The JSON schema is V2. Success requires all21 original required definitions,
one selected generated definition, and three optional presence records. The
reported field-inventoried type count is22 through25. The nested-name list and
optional presence rows are measurement-generated file metadata; no manual PASS
rows are added. Output is buffered and published once, after the whole inventory
has passed its source count/selection checks. A fixed bounded stderr category and
reason code with ordinary exit1 represents failure; no exception message, path,
stack, environment, field value or partial inventory is published.

The unchanged reader pins each read-only DLL handle by length and SHA256 before
rewinding and parsing its public PE/CLR file metadata. FileShare.Read denies
writers/deletion while that handle is held. It never loads these files as
assemblies, gets a runtime Type, executes IL/methods/custom attributes/initializers,
constructs a requested target instance, reflects live private fields, reads field
values/offsets, scans the environment, or launches a process/thread/window/device.
Its own ordinary host/metadata/DTO constructors are distinct from target instances.
Only TypeDef names, declared field/base/signature/ClassLayout metadata and generic
parameter names are read. The existing bounded file extents bound the metadata
walk. Existing signature/nesting/TypeSpec cycle refusals remain unchanged.

AutoLayout/packing/declared size and generic signatures are metadata declarations,
not actual object size/offset/padding or allocation observations. The new fields
can select a compiled branch for a later separate source/ABI equation; they do
not certify that equation or GC reclamation. No R11 executable or protocol data
is inspected or modified. All original dependencies and exact file pins remain
in source-pins.json. Root owns any future compiler/reference/host/timeout/capture
runner after independent review. There is no new runner in this source packet.
The already frozen revision1 runner is neither edited nor blindly repointed.

Authoring used only Python source text/JSON/hash comparisons. No C# compiler,
Add-Type, target parser/AST/import/helper/program/protocol stage, metadata reader,
GUI/device, Git or CI was invoked. The entire new reader/README/pins must freeze
at its future first actual GREEN execution; no such execution has occurred here.
