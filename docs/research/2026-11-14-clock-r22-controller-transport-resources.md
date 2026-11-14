# Clock R22 controller launch and transport storage

This covers the held R22 runner's request writer, helper launch, terminal
reader and quarantine source. The [source-only check](artifacts/2026-11-14-clock-r22-controller-transport/check.py)
pins the materialized manifest and runner and recomputes the selected amounts.
The arithmetic uses the previously measured managed allocation formulas
`A(n,w) = Align8(24 + n*w)` for a one-dimensional array and
`S(n) = Align8(22 + 2*n)` for a string. The measured runtime fingerprints
match the R22 input pins. Application of these formulas and of the archived
.NET source remains conditional on the eventual actual runtime association.
[Allocation ABI receipt](artifacts/2026-11-05-managed-payload-abi-probe-r2/20261105-managed-payload-abi-probe-source-r2/execution-r1/receipt.json)

## Selected source payloads

| Operation | Root or new backing | Bytes under the stated layout |
| --- | --- | ---: |
| Controller hash owner | One script-wide `byte[65536]`, reused by synchronous file hashes | 65,560 |
| Run identifier recipe | One `byte[1091]` at a 512-unit root during hash construction | 1,120 |
| Current identifier referents | Three distinct 32-byte digests, with the target digest also referenced by its hash tuple | 168 |
| Compiler request writer | One padded `byte[41260]` plus `string[40]` carrying references to the existing strings | 41,632 |
| Producer or verifier request writer | One padded `byte[18644]` plus `string[18]` | 18,840 |
| Environment preparation | One `string[8]` with references to expanded settings | 88 |
| Native request argument | One `char[2060]` and a distinct copied string of up to 2,060 units | 8,288 |
| Terminal decode | `byte[308]` plus `uint32[16]`, `uint64[3]` and `uint64[14]` | 608 |
| Retained terminal after successful read | Same four arrays referenced from the six-field map | 608 |
| Cleanup list and projection | 15 reference slots in the backing array and at most 15 in each projection | 144 each |

Request creation admits 4 paths, 8 settings and 28 or 6 arguments at 512
units each before serialization. The source writes one captured byte array
to an exclusive `CreateNew` file and returns the path only. The terminal
read allocates its byte buffer before I/O and keeps it alongside decoded
arrays when the terminal is accepted. Its path and mask map slot require
separate accounting. Hash and transport streams use buffer-size one.
Under the selected .NET source this bypasses `BufferedFileStreamStrategy`.
The terminal EOF probe uses one byte of synchronous span storage, with no
second terminal array. File, path and native I/O owners stay separate.
[File stream strategy](artifacts/2026-11-06-clock-r12-path-geometry/20261106-clock-oracle-r12-path-geometry-independent-review-r1/evidence/primary/Strategies_FileStreamHelpers.cs),
[synchronous EOF probe](artifacts/2026-11-06-clock-r12-path-geometry/20261106-clock-oracle-r12-path-geometry-independent-review-r1/evidence/primary/Strategies_OSFileStreamStrategy.cs)

The identifier construction's 1,091-byte array is distinct from its
32-byte result. The three-current-digest row covers the launch inputs after
forming those digests. A before/after identity check can add a fresh
digest and hash carrier. A nested PE fence can also add arrays of up to
64, 24 and 4,096 bytes, or 4,256 bytes under the array formula. The
controller's rooted hash buffer is charged once when joining these checks.
The two read and write payloads here are one allocation each per entered
invocation. Across the three run positions, the disk transport admits up
to 79,472 retained bytes. That cap does not cap the managed heaps or
prove reclamation of any allocation.

## Additional launch construction on the selected .NET path

- The helper command admits a filename up to 512 UTF-16 units. Its one
  `--request16=` argument contains only a literal prefix and uppercase
  hexadecimal, so the simple quoting path appends a space and the argument
  as a whole. Including up to two filename quotes and final NUL, the
  command has at most 2,576 units. The source check enumerates both quote
  cases and the bounded filename/request lengths including the final NUL
  growth step. The 256-cell stack buffer and every reached pooled rental
  in a single builder trace contribute at most 13,896 bytes under the
  selected array formula. The charge includes prior rentals in that trace
  without a pool-return credit. Existing pool caches, metadata and cold
  allocation still need the separate pool profile. [Archived CreateProcess wrapper](artifacts/2026-11-06-controller-v-source-hold/sources/001-Process.Windows.cs),
  [ValueStringBuilder growth](artifacts/2026-11-06-clock-r12-fresh-path-hold/sources/007-ValueStringBuilder.cs),
  [ArrayPool implementation](artifacts/2026-11-06-clock-r12-fresh-path-hold/sources/006-SharedArrayPool.cs),
  [Argument quoting source](https://github.com/dotnet/dotnet/blob/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Private.CoreLib/src/System/PasteArguments.cs)
- The eight outgoing keys total 43 units. Six fixed values total 497
  units. Both `TEMP` and `TMP` reference the same bounded 512-unit
  source path and independently appear in the serialized block. Thus the
  managed environment string has at most 1,580 units and its terminating
  NUL gives 1,581 units, or 3,162 bytes in the logical native block. The
  source check enumerates the same opaque path length for both keys and
  the ordinal-insensitive key order. The selected `StringBuilder` can
  request up to 5,568 bytes of chunk arrays, plus the 88-byte key array
  and up to 3,184 bytes for the resulting string. This 8,840-byte subtotal
  leaves builder object shells, sorting and P/Invoke argument/native
  ownership separate. [Archived environment formatter](artifacts/2026-11-06-controller-v-source-hold/sources/001-Process.Windows.cs),
  [Archived StringBuilder](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/055-StringBuilder.cs)
- The first property access in `ProcessStartInfo.Environment.Clear()`
  creates an ambient environment snapshot and populates a
  `DictionaryWrapper` before clearing the entries. The eight outgoing
  settings do not bound that initial native snapshot, strings, Hashtable
  or dictionary capacity. Clearing leaves the wrapper backed by its
  original dictionary. The older conditional startup equations require
  actual fresh-launch association and noninterference through all
  capture intervals. Keep the snapshot and retained dictionary capacity
  as distinct terms. [ProcessStartInfo source](https://github.com/dotnet/dotnet/blob/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Diagnostics.Process/src/System/Diagnostics/ProcessStartInfo.cs),
  [Environment writer review](artifacts/2026-11-06-controller-environment-source/20261106-controller-environment-writer-closure-independent-review-r2/review.md)

## Observed object declarations and failure retention

The exact .NET assembly inventory gives 51 fields for `Process`, two for
`Component` and zero for its marshal-by-reference base. It gives 24
fields for `ProcessStartInfo`, one for `DictionaryWrapper`, and three
for `Stopwatch`. Those declarations help join the object shells but still
need the inherited/closed-value-type layout and native ownership fit.
They do not measure a managed object graph. [File metadata receipt](artifacts/2026-11-06-managed-file-metadata-r4/20261106-managed-file-type-inventory-source-r4/execution-r1/receipt.json)

The source has one helper at a time. A failed exit observation, host
abort, invalid or late terminal, Dispose failure, or failed helper identity
check can retain a quarantine map and refuse a later child. When it
retains the process, the archived `Process.Start(ProcessStartInfo)`
path stores the same start-info object in that process. Therefore
quarantine retains the request argument string, filename, work dir,
argument-list backing and refilled environment dictionary, even when the
source selected no redirected streams. The initial launch uses
`bInheritHandles=true` in the archived branch, so the fresh profile must
also establish the admitted native inheritable-handle set. Before
`Process.Start` returns the process object, the controller has no distinct
helper handle or PID in its own variables. That prefix needs the actual
library/native failure association, including failed creation outputs and
any failure after native launch with an unreturned object. [Process.Start source](artifacts/2026-11-06-controller-v-source-hold/sources/042-Process.cs),
[CreateProcess branch](artifacts/2026-11-06-controller-v-source-hold/sources/001-Process.Windows.cs)

The quarantine's terminal and request path entries alias current objects.
It does not explicitly store the serialized request array, digest arrays,
or helper command builder. The terminal file remains a separate retained
artifact. The failure journal and the nested after-fence can run with
quarantine still rooted, including a fresh hash and PE header check. The
receipt can retain prior cleanup projections and, on an after-identity
refusal, a replacement projection can briefly coexist with the old one.
The terminal mask excludes simultaneous reap flags at bits 1 and 2.
Without a rooted marker the original mask yields at most 13 entries,
leaving two slots for the tail/disposal diagnostic and rooted marker.
With that marker present, at most 14 mask entries and one additional
diagnostic fit. Count the actual wrappers, child-row/map allocations,
path caches, native wrapper lifetimes and failure occupations in the
remaining join. Neither a scope exit, Dispose call, pointer clear nor a
pool return supplies physical-free credit.

## What a pipe query can establish

A distinct reviewed headless purpose could create one private anonymous
pipe with `CreatePipe(..., 0)`, call `GetNamedPipeInfo` on its read end
and close both references. This requires no target, worker or I/O
payload, and can observe the initial reported buffer sizes on that host.
It cannot establish a peak for both live pipes. Zero asks for the default;
the sizing interface describes buffers as advisory, and the underlying
named-pipe documentation permits growth to satisfy writes. One larger
pending target write, transient requests, native object backing and the
last-handle settlement are separate terms. The initial query would not
remove them. It was not run in this source worksheet. [Anonymous pipes](https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations),
[Pipe info](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-getnamedpipeinfo),
[Named-pipe buffering](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createnamedpipea)

This narrows the source payloads and failure overlap. The full controller
phase, general CLR/runtime components, native associated profile and
whole 32 MiB joined scratch result remain open.
