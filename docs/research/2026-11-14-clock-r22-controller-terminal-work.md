# Clock R22 controller terminal validation and cleanup helpers

This worksheet covers five local script bodies in the exact R22 runner,
SHA-256 `294C4D91D0FF0EB0E80048B71DFC1E24C45103E88AD97F2F7715F9BD252F4798`.
Line numbers refer to that runner. The caller owns evaluated arguments,
dispatch and actual argument coercions. The callee owns entry, formals
and return. The terminal reader already includes its validation caller.
The [R13 caller ledger](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/206-argument.md)
supplies the scope and loop model. The [transport worksheet](2026-11-14-clock-r22-controller-transport-work.md)
owns the file read, decode, captures and typed arrays.

## Scalar groups

All groups are source-action ceilings. An input consumed directly in a
destination can use the same write. CLR map search, comparer internals,
implicit bulk copying, and opaque command/runtime enumeration retain
their primitive boundary. Explicit members, callbacks and arrays are
accounted below.

| Group | Upper | Source ports |
| --- | ---: | --- |
| A, plain array access | 8 | Array reference, index value and typed index conversion when reached, selection, element value and result/argument destination |
| X, computed array index | 13 | A plus at most five for its addend, addition, actual conversion and intermediate index destination |
| M, one map selection | 8 | Receiver read and binding, key value and parameter, primitive dispatch/entry/return and value destination, or the equivalent source selection ports |
| F, bit test | 12 | Masked scalar and mask operands, band and comparison, comparison operand, type-sensitive conversions and the selected predicate destination |
| L, scalar comparison | 6 | Compared operand reads or result destinations, comparison, actual coercions and predicate destination |
| B, control | 3 | A written short-circuit, negation or branch decision and its reached scalar control/result |
| D, diagnostic | 4 | Diagnostic value, throw and reached error/result transfer |
| H, arithmetic or update | 8 | Scalar operands, arithmetic, actual conversion and typed result/update destination |

Each fixed `for` loop includes its setup, testing and exit at
`8 + 9N` for N bodies. The named foreach uses `4 + 5N`.
An explicit RHS cast costs its Type read and conversion. A typed
local's initialization includes the actual conversion and store.
`Exception.Data` reads and the List Count getter use six per
ordinary getter, including the actual receiver and result. Each
Data put uses the equivalent eight ports of M with a value input
instead of a value output. Runtime comparer and map bodies stay opaque.

## Reason compatibility

`Test-TaskSupervisorReason` reaches two small-reason tests and at
most one pass through the remaining switch. Each matched case returns
immediately. The switch has nine membership predicates with literal
array lengths 5, 3, 3, 4, 2, 2, 2, 2 and 3, one range predicate,
and five literal cases. The default returns false.

| Region | Upper |
| --- | ---: |
| Entry, two UInt32 formal destinations, return | 4 |
| Two small-reason checks at `L+B` | 18 |
| Switch control and scalar input, automatic conversion and exit | 12 |
| Nine membership predicates, `9 × 14 + 26 × 4` | 230 |
| Reached range predicate, `4 + 2L + 2B` | 22 |
| Five literal cases, each with match and control | 60 |
| Selected result, including either two-site alternative, `2L+B` | 15 |
| Local upper | **361** |

Each membership predicate pays its invocation, entry, return, automatic
UInt32 input destination and use, array setup, primitive match and
predicate control. Its four actions per written element cover the
literal, element-store, and mixed-type operand conversion and
destination where reached. Each literal case covers its match operand,
possible conversion, comparison/destination and selection branch.
The successful case, failed comparison, and default result each fit
the single result allowance.

## Terminal validation

`Assert-TaskSupervisorTerminal` receives one six-field map from the
reader, with UInt32 U, UInt64 V and P, UInt32 mask and the authenticated
child index. Counts cover invalid record contents reaching the assertion.
Each throw ends that assertion. Diagnostic terms in distinct sequential
groups can be summed without assuming multiple throws actually execute.

| Runner lines and region | Local upper | Expression |
| --- | ---: | --- |
| 335–336, entry/map and flag loads | 52 | `4 + 4M + 2A` |
| 337–341, reason/native-pair loop | 620 | `8 + 3(9 + 195)` |
| 342, secondary presence | 74 | `2(F+A+2L) + 2B+D` |
| 343–345, primary, native-secondary and saturation conditions | 126 | `(F+A+L+2B+D) + (2F+2B+D) + (2F+A+L+5+3B+D)` |
| 346–347, native availability and first-native equality | 179 | `(F+3(A+L)+4B+D) + (2F+6A+3L+5B+D)` |
| 348–349, creation/PID and exit availability | 108 | `(2F+2A+2L+4B+2D) + (F+A+L+2B+D)` |
| 350–352, reservation, prefix and accumulator setup | 124 | `(6M+4+5+1) + (F+2(A+L)+3B+2D) + 9` |
| 353–367, two capture records | 1,404 | `8 + 2(9 + 689)` |
| 368, aggregate progress or unavailability | 83 | `F+2A+4L+H+5B+2D` |
| 369, three reason values | 29 | `3A+5` |
| 370, four flag/reason pairs | 249 | `4×5+5+4+4(5+F+2A+12+2B+D)` |
| 371, mutually exclusive refusal flags | 64 | `4F+4B+D` |
| 372, stage refusal filter | 123 | `F+20+3(4+2L+2B)+6+L+3B+D` |
| 373–375, omitted-byte gate and settled availability | 82 | `(2A+H+L+F+2B+D) + 2 + (L+F+2B+D)` |
| 376–389, status selection and selected body | 515 | `3(L+B) + max(447,36,488,53)` |
| Local upper, before reason bodies | **3,832** | |

Each reason record loop allows 12 for the two-operation offset and
24 for the bit selection and assigned result. It uses `A+X+3` for
the two indexed UInt32 arguments, dispatch and result use. The first
gate then uses `2F+X+A+2L+6B+D` for native availability and installed
site consistency. The other uses `X+L+2F+3B+D` for reason 48 and the
two relevant masks. Those five terms give 195 per body. The three
records at offsets 7, 10 and 13 can therefore call the reason helper
at most three times. A false result can cause an early throw.

On line 345, five covers the UInt64 maximum's static member and
typed value. The line 350 group uses three selectors on each side of
the subtraction, including the indexed reservation value. Four covers
the two written UInt64 casts, five the arithmetic and its typed input
or result ports, and one the final local. The initial accumulator
stores on line 352 each allow zero evaluation, conversion and store.

| One capture record, 353–367 | Upper | Expression |
| --- | ---: | --- |
| 354, index and five flag selectors | 63 | `H+5(L+B+2)` |
| 355, position availability and ordering | 143 | `4F+4X+3L+7B+D` |
| 356, partial availability and bound | 67 | `F+2(X+L)+3B+2D` |
| 357–358, settled and EOF partial obligations | 90 | `(2F+2B+D) + (2F+X+L+3B+D)` |
| 361, four pre-addition operand bounds | 148 | `8+4(9+X+L+B+D)` |
| 362–365, counter availability and equation or zero check | 142 | `F+B+max(4X+A+2L+H+2B+D+A+X+2H, A+3X+4L+4B+D)` |
| 366, selected UInt64 partial update | 36 | `F+X+H+B` |
| Per record | **689** | |

The line 370 outer array initializes four pair references. Each pair
initializes two integer slots. Each fixed pair retrieves both indexed
operands and evaluates one potentially mixed-type membership against
the three local UInt32 reasons. The 12 covers the operator, its input
and conversion/result ports in addition to the pair selectors. For
line 372 the 20 covers the `Where-Object` public caller, filter script
binding, pipeline setup and array capture. Its authored callback
requires dispatch/entry/return, the automatic input destination, two
comparisons and their short-circuit/result per reason. The capture's
Count getter and the outer negated availability test remain explicit.
The [R13 callback convention](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/293-20261106-research-callback-destination-clarifier-r1.md)
puts each automatic input and real result destination at its respective
callback boundary.

The status gate pays all three leading comparisons and one body.

| Status body | Upper | Expression and selected work |
| --- | ---: | --- |
| Complete 0 or 1 | 447 | `4+2F+2A+5L+5B+D + 8+2(9+2H+7X+A+5L+5B+D)`. Required/forbidden flags and status/exit consistency. Two byte equations with eight array reads each and position subtraction. |
| Pre-execution 2 | 36 | `F+A+L+2B+D`. Execution flag and reason. |
| Failed 3 | 488 | `F+2A+3L+4B+D + 8+2(9+3(L+B+2)+(2F+A+L+2H+3B+D)+2(2F+2H+2B+D))`. Main record and two sets of EOF, position and flush obligations. Each indexed diagnostic mask accounts for index addition and shift. |
| Quarantine 4 | 53 | `2A+2L+F+3B+D`. Primary/secondary absence and quarantine flag. |

## Failure objects and cleanup decode

| Helper region | Upper |
| --- | ---: |
| Recorded failure boundary, UInt32 array receiver, record offset, native bit selection and exception construction | `4+M+12+24+8 = 56` |
| Five Data destinations, two plain and two computed UInt32 reads, bit test, terminal path and return value | `5(6+8)+2A+2X+F+M+1 = 133` |
| `New-TaskSupervisorFailure` local | **189** |
| Parent failure boundary, string validator caller and exception construction | `6+4+8 = 18` |
| Four Data destinations, two simple values and conditional exit/path values including subexpression outputs, then return value | `4(6+8)+2+8+L+B+M+4+1 = 88` |
| `New-TaskSupervisorParentFailure` local | **106** |
| Cleanup boundary, 15 literal string slots, array setup and loop setup/exit | `4+32+8 = 44` |
| Cleanup positions, shift/mask and selected flag test including loop control | `15(9+H+F+B) = 480` |
| Cleanup Count gate, typed string source and List Add ports for enabled bits | `14(6+L+B+A+8) = 434` |
| Cleanup refusal diagnostic allowance | 4 |
| `Add-TaskSupervisorCleanup` local | **962** |

The failure constructor uses runtime UInt32 record fields, a Boolean
native-known value, and the existing terminal path. The parent
constructor first calls `Assert-TaskString $Reason 64 $true` and
puts the actual nullable exit and path values in its Data map. That
validator's body stays separate and is reached only for the
22-unit `SUPERVISOR_TAIL_FAILED` literal at this caller. Neither constructor
expands the CLR exception allocation or its map implementation.

The mask is already checked at line 308. It has only bits 0 through
14 and bits 1 and 2 are exclusive. The 15 test positions thus have
at most 14 selected Count and Add sites. The supplied list is freshly
constructed and the decoder is its first use. Its internal cap
refusal cannot be triggered by an accepted mask at that call site.
The diagnostic allowance covers a last-prefix branch if the same
bounded decode were used with a prefilled list.

## Call composition and refusal paths

| Body | Reached per entered child | Reached across the three root slots | Local upper |
| --- | ---: | ---: | ---: |
| `Assert-TaskSupervisorTerminal` via reader | 1 | 3 | 3,832 |
| `Test-TaskSupervisorReason` inside the assertion | 3 | 9 | 361 |
| `New-TaskSupervisorFailure` | 2 | 6 | 189 |
| `New-TaskSupervisorParentFailure` | 1 | 3 | 106 |
| `Add-TaskSupervisorCleanup` | 1 | 3 | 962 |

The body sum is **6,361 per entered child** and **19,083 across the
three fixed child positions**. A started helper gets one terminal read
attempt in the child's finally even after a parent-side refusal. A
map rejection or absent record ends that route before the constructors
and cleanup decode. A returned prefix can carry both primary and
secondary records. The dedicated first native-secondary triple is
validated and retained by the terminal but cannot cause a third failure
constructor. A tail refusal can call the parent constructor even when
both record constructors were reached.

The direct `Invoke-TaskChild` work stays in its caller ledger. It
owns each lookup for the constructors and cleanup call, list comparers
at 458, 475 and 484, direct string additions, cleanup capacity tests
at 472, 476 and 485, root quarantine maps and failure status selection.
The case-sensitive cleanup comparison can be reached at most twice
per child. The prefix quarantine membership and the dispose-catch
membership exclude one another, and the post-binding catch is a
separate site. Each such comparison sees at most 15 strings. The
mask includes at most 14 mapped strings, and if it lacks the
quarantine bit it includes at most 13. Thus the direct additions
before a search fit, with the tail path adding one or the disposal
path adding one before its search. The no-prefix branch begins with
two direct strings. This argument includes the search performed
after a binding refusal. The line 467 failure-class check has one
literal 11-item membership expression.

The child caller has eight syntactic `Set-TaskSecondary` sites
at 440, 443, 448, 449, 455, 475, 482 and 488. The terminal-read
catch excludes the three decoded-record routes. The dispose catch
excludes a failed observation cleanup and an entered tail-refusal
branch. Counting both late binding and footer catches conservatively
gives at most six secondary calls per child. They address the same
child-specific slot `5 + $taskChildIndex`. Slot writes, refusal
state and their callee body remain in the existing secondary
worksheet and caller ledger. The parent failure string validator
can be added at most once per child. Independent source review checked the exact runner, typed selector and
callback destinations, record and cleanup trip counts, caller overlap,
and the corrected reason-record arithmetic.
