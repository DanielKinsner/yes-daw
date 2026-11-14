# Clock R22 controller function correspondence

The [source check](check.py) compares the pinned R13 controller in
the reviewed census archive to the exact R22 controller. Of the
74 older function bodies, 69 are byte-identical and five changed.
R22 adds 14 functions for the supervisor handles, wire format,
budget and failure protocol. None of the older functions disappeared.

The five changed bodies are the path catalogue, child invocation,
child cleanup projection, controller initialization, and input pin
fence. Both scripts also have top-level statements with separate
source accounting. The scan reads exact source text with the known
column-one declarations. The first check found one shared function
whose closing brace is indented. The corrected check handles that
body as well. Neither PowerShell controller is loaded or parsed.

Run `python -E -B check.py` with the exact R22 candidate staged.
This starts the R22 controller ledger with a checked set of 19 new
or changed bodies and exposes the 69 reusable body identities for
review. Identical bodies still need current caller domains,
occurrence counts, different nested callees and fresh whole phase
composition before any bound transfers.

## Script root and success geometry

Excluding the function definitions leaves 177 baseline lines and
183 current lines, with 14 edited spans including comments and
spacing. The current manifest and source jointly supply these root
domains for the caller and lifetime worksheets:

| Root region | Current domain |
| --- | --- |
| Entry refusal and reflection | Eight false entry guards. The zero-parameter `GetType` lookup now takes the static `Type.EmptyTypes` field. |
| Bootstrap JSON | 1 MiB captured input, 17,209 token admission, 512 string units, depth four before the SDK parse. The previous token admission was 16,104. |
| Directory creation | One run root and seven listed child directory calls, including `transport`. |
| Expected file set before the final bundle | 56 reference, three build, four control, four stage and six transport names. The observed root walk is capped at 85 entries. |
| Transport artifact projection | Six name joins, at most six file hashes and six hex projections. The caps are 41,260, 18,644, 18,644 and three times 308 bytes. The 65,536-byte hash routine admits at most one explicit chunk per file. |
| Bundle projection | At most 73 artifact rows and 1,294 transitive rows, with at most five fixed-point measurements and one final encode. `sup.exe` adds one transitive descriptor. The final bundle is the 74th success file. |
| Retained sum | Add 79,472 reserved transport bytes and the actual helper pin. The six transport artifacts already enter the admitted sum through their individual rows. The helper is charged once as a separate input. |

The transport loops run at distinct sites. The first contributes six
expected-name additions and enlarges the file-set arguments. The
second contributes six artifact maps, digest carriers and hex calls
only after the final input fence. On failure, a cap or hash can stop
the second loop before all six visits. Use the current arguments for
the shared file-set, hash, JSON preflight and serialization bodies.
The listed cardinalities are conditional on the staged, authenticated
manifest. They give the new row and loop domains for fresh source and
resource accounting.

## Script root caller work

Run `python -E -B root-work.py` to recompose the pinned
[R13 root caller ledger](../2026-11-06-clock-r13-runner-census-r3/sources/260-main-caller-component.json)
under the current source and cardinalities. The function-free source
diff and geometry checker accounts for every changed span. Function
registration is one source event per declaration. The corrected
static field receiver, larger token cap and transitive ceiling constants
keep their previous direct port counts. New callee bodies are separate.

| Added caller work | Conditional upper |
| --- | ---: |
| Two guard stores and two additional guard terms | 10 |
| Static transport and helper joins | 18 |
| Additional function registrations | 14 |
| Retained-reservation operands | 16 |
| Added directory literal slot and creation site | 16 |
| 68 added iterations of both input and include loops | `68 × (40 + 22)` |
| Seven extra root walk candidates returned to the caller | `7 × 49` |
| Transport expected-name loop | `10 + 6 × 14` |
| Transport descriptor loop | `10 + 6 × (5 + 9 + 26 + 4 + 31 + 30)` |
| Added admitted binary amount and six added artifact rows | `14 + 6 × 14` |
| 68 additional input descriptors and the helper descriptor | `68 × 34 + 43` |
| Total delta | **7,820** |

The transport name loop pays the nested map selection and foreach
frame once, then foreach control, concatenation, and List Add with
argument slots. Its descriptor loop uses six separate terms per row
for foreach control, the static path join, maximum selection, hash
caller, terminal length guard, and map plus Add. The cap selection
allows the first `EndsWith` receiver/argument/enum and primitive result,
the second equality, both tests and the selected Int64 result. The
terminal length guard pays its own `EndsWith`, short-circuit, tuple
getter, comparison and reached throw. The descriptor pays for four
map field inputs, including the separate hash-hex caller and the List
Add boundary. Both reservation and admitted expressions account for
the nested binary map selection and direct Int64 cast. The helper's
four-field transitive descriptor accounts for the two separate nested
lookups plus List Add.

The current caller-only subtotal is **188,619 conditional source
actions**, including successful and refusal routes. Current hash and
other shared callee occurrences retain their own phase schedules.

## Shared file hash body

Run `python -E -B hash-schedule.py` to check all 19 written
`Get-TaskHash` call sites. The script matches the unchanged body to
its [R13 operand ledger](../2026-11-06-clock-r13-runner-census-r3/sources/250-body-ledgers.json)
and rebuilds the occurrence schedule from the R22 manifest and call
graph. There are at most 10,513 calls and 88,908 explicit read chunks
across the whole controller. The inherited 295 fixed and 65 per chunk
give **8,880,355 conditional actions**. The source uses `ReadExactly`,
then checks EOF, so a failed partial read cannot restart a chunk.

The schedule includes all 14 binding visits and the target digest for
each request. It reserves for the current manifest's full 1 MiB on an
identity mismatch, and for a final reference or build candidate that
can be hashed before refusing at the aggregate cap. Hash calls own
their path guard dispatch and scoped stream/hasher retirement. The
path guard bodies, caller casts, digest consumers, bootstrap hash
family and runtime buffer owners retain separate terms.

## Path catalogue payload

Run `python -E -B catalogue-geometry.py` to reconstruct the
fixed ordinary-drive profile with a direct `source-clock-r1` run leaf.
The tuple is 1,294 current inputs and 75 run-owned names. These yield
1,369 distinct lexical leaves, 1,549 distinct leaf and parent strings,
and at most 19 positions per path. The 1,549 figure assumes the eventual
Windows path profile has the same ordinary-drive normalization. It
bounds resident keys in both intern tables. The source-only cap without
that profile is 88,985 entries per table. Uncatalogued fallback paths
do not populate either table.

An independent caller audit bounded interrupted first builds at seven.
The main route can carry one initial failure, a child post-binding
failure, an outer finally fence failure and one captured-binary check
failure before it stops. The footer can continue through one inventory
walk failure, one inventory document failure and the final receipt
failure. Each active route can first fail during one unpublished build.
Every published entry can allocate one 65-reference array. Counting
all interrupted arrays gives at most 1,376 requests or 748,544 bytes
with the selected x64 array layout of 24 header bytes and eight bytes
per reference. No reclamation is credited. The same occurrence ceiling
applies to the two-reference alias pair after a reached parent walk.

These are resident-name and selected array bounds. Interned strings,
fresh normalization results, managed map implementations and resize
history, runtime layouts and all other owners still need the resource
join. An ordinary lexical path profile alone supplies no collision or
allocator proof.
