# Fixed managed type inventory — source only

**Uncompiled and unrun. Root must review this whole packet before compiling or
using it.** This is a separate metadata tool, not an R10 helper or allocation
probe. It grants no target execution, payload/32MiB fit, offset, reclaim or GO
credit. No prior source, evidence or code seal is changed.

`reader.cs` is an ordinary C# console/tool-host entry. It accepts zero arguments
and has four exact file paths and21 exact type names, grouped in request order.
It opens each DLL with read-only FileAccess and FileShare.Read, checks its exact
length/SHA256 on that same handle, rewinds, then uses PEReader/MetadataReader on
the file metadata. It never loads those files as assemblies, resolves types into
runtime Type objects, reads IL, executes methods/custom attributes/initializers,
constructs a target instance, reflects instance fields, calls GetValue, or reads
runtime fields/offsets. Its own ordinary host/provider/DTO/SDK constructors are
not constructors of the requested target types.

The fixed input files are the cached PowerShell runtime's Newtonsoft.Json,
System.Management.Automation,System.Collections.Specialized and CoreLib DLLs.
All four complete raw-byte lengths/hashes are in both source and manifest.
The manifest additionally pins the selected metadata/immutable/JSON/IO/console/
crypto/runtime reference dependencies used by the tool host. Those are file
identities, not a claim they have already been selected/loaded by a future host.
Root owns any future compiler, references, fresh host,timeout and output capture.
No Add-Type, compiler, target parser, helper, program or metadata reader was run
while authoring this packet. No package restore or network access is needed.

No runner is necessary in this source packet: Main is the entire narrow entry,
and root already owns compilation/launch. Root can compile to a fresh console
assembly with the recorded .NET10-compatible compiler/reference/host identity,
or call this own tool entry from a separately reviewed fresh tool host. It must
not load the target DLLs for inspection or substitute their runtime types. Do
not add R10 source, private-field reflection, GetUninitializedObject, unsafe
operations, target constructors, profile booleans or process/window controls.

The success output is one UTF8 JSON object plus LF. It is built in a private
buffer and published only after all21 exact types are found once and processed.
On failure stdout has no partial inventory; stderr has one bounded JSON object
containing a fixed exception category and fixed reason code only, never message,
stack,path,environment,field values or type contents. Exit0 means metadata
inventory completed; exit1 is a named failure, never a manual PASS row.

For every requested type it reports assembly/file identity,qualified name with
`+` for nesting,TypeDef token,TypeAttributes text/hex,layout flag,ClassLayout
metadata default/packing/declared-size,base handle/token/decoded type and raw
TypeSpec base signature blob if present,generic parameter names,and every
declared NONSTATIC field's name/token/attributes/decoded signature/raw blob hex.
Private field names/attributes are public file-metadata facts; their values are
never accessed. Static fields and inherited declarations are excluded.
ClassLayout values are metadata declarations; they are not actual runtime size,
layout,offset or allocation observations. Auto layout especially proves none.

TypeDef/TypeRef names preserve nesting and assembly/module scope. Signature
decoding handles class/value-type/unspecified modifier kinds,generic type/method
parameters,instantiations,SZ/general arrays with rank/sizes/lower bounds,byref,
pointer,pinned,required/optional custom modifiers,function pointers with their
raw header/generic/required-parameter counts,and all defined primitive types.
It does not guess an unsupported kind: decoding throws a bounded failure.
Generic references are metadata names/signatures, not instantiated runtime types.
Raw signature bytes remain alongside decoded notation for independent checking.
No external assembly is loaded to resolve a TypeRef or TypeSpec.

Only four pinned finite files,21 requested definitions and their declared fields
are processed. TypeDef discovery walks finite metadata rows and reads names only;
no method bodies/custom-attribute blobs are inspected. Declaring/reference chains
and TypeSpec decoding reject cycles. SDK signature validation/refusal remains
explicit; malformed/unpinned files do not produce invented signatures. The fixed
input lengths bound the metadata tables/blobs; no caller-supplied path/type list,
directory/module/environment enumeration,thread,process,network or native GUI
operation exists in this helper.

After a real reviewed run, output can settle which compiled instance fields and
base/value-type signature branches exist in these exact assemblies. Translating
that into complete controlled-payload lifetimes,CLR physical object/array sizes,
actual offsets,padding,simultaneous owners,GC reclamation or32MiB capacity remains
separate source/ABI/resource work. No such fit or runtime layout is certified here.
