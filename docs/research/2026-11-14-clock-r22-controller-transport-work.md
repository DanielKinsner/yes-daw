# Clock R22 controller transport scalar bounds

This proposal covers the six local transport serialization helpers in
the exact R22 runner, SHA-256
`294C4D91D0FF0EB0E80048B71DFC1E24C45103E88AD97F2F7715F9BD252F4798`.
It counts the intended admitted controller path and its refusal prefixes.
The [R13 caller model](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/206-argument.md)
assigns evaluated arguments, dispatch and actual conversions to the caller,
and formal destinations, entry and return to the callee. Explicit casts
count their type reads and conversion operations. Conditional and
subexpression outputs retain their destinations. Each loop pays setup,
tests, branches, induction or enumerator state and retirement.

The controller caller owns its six helpers' external invocation sites.
Within this worksheet a nested script caller accounts for argument
evaluation and dispatch, with the nested row accounting for formals and
local work. Native and CLR primitive boundaries include their ports.
Bulk `Array.Copy`, array allocation and zeroing, hash implementations,
stream internals and the managed string constructor retain the existing
[primitive boundary](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/064-20261105-clock-oracle-research-bulk-leaf-clarification.md).
Their payloads remain in the [transport storage worksheet](2026-11-14-clock-r22-controller-transport-resources.md).

Each locally reached supervisor spend call owns its explicit Int32
to Int64 amount conversion. The nested spend body owns the formal
destination and meter logic.

## Little-endian scalar helpers

| Helper | Fixed actions | Per byte | Width four | Width eight |
| --- | ---: | ---: | ---: | ---: |
| `Set-TaskSupervisorInteger` | 80 | 32 | 208 | 336 |
| `Get-TaskSupervisorInteger` | 72 | 36 | 216 | 360 |

The Set fixed group allows 16 for entry/formals and return, 48 for
width, offset and capacity tests including `Length` and subtraction,
UInt32 static maximum, comparison, short circuit and throw, and 16 for
index setup and terminal condition. The Get fixed group allows 16
for entry/formals and singleton result, 36 for its range gate and
throw, and 20 for loop setup/exit and the typed U64 zero. A value
cast supplied by a caller remains in that caller's separate row.

Set's per-byte allowance splits into 10 for tested loop control,
13 for typed scalar shifts, index scaling, mask and explicit byte
cast, and nine for receiver, destination index and byte store.
Get allows the same ten for the loop, 12 for the computed byte index,
read and explicit U64 cast, and 14 for shift, OR, current-value read
and assignment. Subtraction-first guards limit both helper loops
to exactly their admitted width. Refusals skip a suffix of this work.

## Run hash and request argument

| Run hash region | Actions per entered call |
| --- | ---: |
| Local string guard invocation | 12 |
| Manifest digest extent test and refusal | 20 |
| Domain binding, typed array allocation, two property-bound loop setups/exits and formal entry | 64 |
| Thirty domain units at 29 actions | 870 |
| Exact 32-byte copy and cursor updates | 60 |
| Width-four Set caller and cursor advance | 24 |
| Up to 512 root units at 56 actions | 28,672 |
| Exact-end test, SHA-256 call and result | 44 |
| Local upper | 29,766 |

The domain has 30 UTF-16 units and its byte representation is confined
to those ASCII characters. The zero-filled allocated array supplies
the one following NUL. Its size is at most `30 + 1 + 32 + 4 +
2 × 512 = 1,091`. The domain loop describes its indexed String getter,
two written casts to int and byte, destination store and loop control.
The cursor adds the copied digest extent and the encoded U32 path
length. The root loop includes character read and int cast, two
postincrement index reads/updates, byte casts, mask/shift and stores.
The last group owns array length evaluation, EOF control, static
HashData's byte-array input and result, unary comma array setup, its
single digest-reference slot and return. The path validator and Set
body retain their own rows.

The request argument uses at most 144 fixed actions for its boundary,
string guard call, nonempty test, allocation and loop setup/ends, and
the string constructor and return. Twelve prefix units at 26 cover
the indexed source character, typed store and loop. Each of at most
512 path units uses 32 for the outer loop, indexed character and int
cast, and four 42-action inner bodies for the offset and nibble shift,
integer mask, hex getter, indexed char store and loop. This gives
`144 + 12 × 26 + 512 × 32 + 4 × 512 × 42 = 102,856`. Each continuing
unit adds four hex characters. The char array and result have up to
2,060 UTF-16 units including the 12-unit prefix. The string guard
body remains separate.

## Request write

Let `N` be the admitted number of strings, 40 for the compiler and
18 for each native slot. The index, argument and environment-count
checks precede allocation of the string view. Three 32-byte digest
checks follow. The preflight guard admits each string at up to 512
units and checks the derived byte length before capture allocation.
The maximum is `140 + 4N + 2(512N)`, giving 41,260 or 18,644 bytes.

| Writer region | Actions |
| --- | ---: |
| Boundary, admitted counts, digest array and guards, slot-specific cap and request meter outcome | 200 |
| Allocate string view, assign four paths and set up/end the environment and argument loops | 96 |
| Populate view of eight environment strings and up to 28 arguments | 28N |
| Preflight loop, string guard callers and accumulated extent | 44N |
| Magic, including loop, two casts and each byte store | 8 × 26 |
| Seven header Set call sites | 7 × 40 |
| Three digest copies and cursor initialization | 80 |
| One serialization group per string, including its Set call and UTF-16 loop setup/end | 48N |
| One source-written UTF-16 unit | Up to 56 × 512N |
| EOF assertion, path construction and I/O region | 272 |
| Local upper | `1,136 + 120N + 56 × 512N` |

The 200 row covers seven script formals, the `if`-expression result
in the argument-length condition, explicit digest-array construction
and foreach ports, the admitted capture-length check and byte-array
allocation boundary. The population row covers two typed indices,
getter/read/store and loop events. It allows all N instead of only
N−4 entries. Preflight covers its string index, Boolean ASCII selector,
validator call boundary arguments, string-length getter, and the
two-unit extent arithmetic and compound update. The 40-action header
call ceiling includes the three nested map selectors and explicit U64
casts at the helper- and tail-reservation fields. Each string's fixed
48 includes the foreach input destination, width-four length-call
arguments and advancement. Its 56-action UTF-16 body mirrors the run
hash's typed stores and bounded loop.

The 272-action I/O group allows 56 for exact EOF/layout and path
construction, index-to-string conversion and no-link caller, 32 for
scope defaults and both stage-check call sites, 76 for the stream
constructor, enum/argument formals, Write and Flush, and 108 for
result, failure, catch and finally work. The catch includes selection
and automatic error input, actual `$_` reads and primary retention.
The finally includes both cleanup calls' evaluation and dispatch.
There is one CreateNew attempt, one possible Write and one possible
Flush. Partial transport output does not retry. The stage cap, cleanup
and meter bodies have separate rows. The resulting local upper is
1,152,816 for the compiler or 519,392 for a native slot.

## Terminal read

| Reader region | Actions per entered call |
| --- | ---: |
| Formal boundary, index-to-string path and no-link caller | 80 |
| Allocate one 308-byte array, scope defaults, initial control | 70 |
| Open, length guard, ReadExactly, EOF probe, catch and finally | 160 |
| Terminal meter call and magic-loop setup | 40 |
| Magic test, eight bodies at 26 | 208 |
| U/V/P decode, 33 bodies at 40 | 1,320 |
| Typed array creation, loop setup/end and mask-load caller | 140 |
| Version/length/index/status and typed mask guards | 176 |
| Three digest selections and loops' fixed work | 120 |
| 96 identity bytes at 38 for offset, indexed operands, branch and loop | 3,648 |
| Six-field return map and its destinations | 80 |
| Validation caller and return | 20 |
| Local upper | 6,062 |

The reader may reject during one open, one length query, one
`ReadExactly` or one further byte read. The EOF test includes the
signed Int32 negation in −1. The corresponding catch and cleanup
formation cover failure and preservation of the first raw error.
The typed arrays use UInt32 for U, UInt64 for V and P. A reached U
body includes the written UInt32 result cast. All bodies count the
offset multiplication/addition, Width literal, caller dispatch and
indexed store. The digest checks compare only the existing selected
32-byte arrays and the captured identity bytes. The map names and
initializes six distinct slots and retains references to the four
existing arrays. `Assert-TaskSupervisorTerminal` owns its separately
reviewed or pending body and formal boundary.

## Composition and separate callees

The run-hash has one width-four Set. Each writer has five width-four
header calls, two width-eight header calls and N width-four string
calls. Thus it and run-hash together reach 46 or 24 width-four calls
and two width-eight calls. The terminal reader reaches 16 width-four
U loads and the width-four mask load, and 3 + 14 width-eight loads.
Refusal can end any of these prefixes before the next call.

| Component | Compiler | Producer or verifier, each |
| --- | ---: | ---: |
| Run hash local | 29,766 | 29,766 |
| Request writer local | 1,152,816 | 519,392 |
| Request argument local | 102,856 | 102,856 |
| Terminal reader local | 6,062 | 6,062 |
| Set bodies, including the run-hash Set | 10,240 | 5,664 |
| Get bodies | 9,792 | 9,792 |
| Transport serialization subtotal | 1,311,532 | 673,532 |

The three fixed child positions give **2,658,596** source actions for
these scopes. One request and one terminal can be attempted per entered
child. A failed request, refused launch or unobserved exit can shorten
the schedule. Terminal read occurs in the child caller's finally only
after a helper start. It can be reached for a failed or unpublished
terminal when the helper did start. The parent caller's startup and
retirement proof must include both alternatives.

The separate callees below are reached at most these counts per child.
Their arguments and dispatch already occur in the local rows above.

| Callee | Compiler | Producer or verifier, each |
| --- | ---: | ---: |
| `Assert-TaskString` | 42 | 20 |
| `Assert-NoLink` | 2 | 2 |
| `Spend-TaskSupervisor` | 2 | 2 |
| `Get-TaskStageBytes` | 2 | 2 |
| `Close-TaskOwner` | 2 | 2 |
| `Complete-TaskOwnerClose` | 2 | 2 |
| `Assert-TaskSupervisorTerminal` | 1 | 1 |

The stage check precedes CreateNew and the actual Write. Each stream
region's finally can execute on a refusal. Closed-path expansion,
footer, terminal semantic validation, process and profile work and
general runtime failure state remain elsewhere. Independent review
checked the exact runner and corrected the two property-bound loop
setups in the run-identity recipe. The table includes that correction.
