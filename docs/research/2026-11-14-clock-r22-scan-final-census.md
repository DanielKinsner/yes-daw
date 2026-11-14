# Clock R22 stage scan scalar composition

This worksheet expands the inherited non-loop rows against the exact R22
native source. R22 uses R20's [I/O source](artifacts/2026-11-14-clock-r20-path-copy/review.md)
and the inherited [codec](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/056-sup_codec.cpp)
and [core](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/057-sup_core.cpp).
The [scalar convention](artifacts/2026-11-05-clock-research-r9/cpp-census/census.md)
counts typed caller evaluation, formal bindings, entries and returns, source
operations and loop control. The rows count entered code, including a
refusal. Native primitives retain their stated boundary.

## Scan-local work

| Region | Upper | Source coverage |
| --- | ---: | --- |
| Scan boundary | 16 | Evaluate and bind three arguments, allowing zero conversion or a remaining-length subtraction and conversion, then dispatch, enter and return. |
| Setup through the pattern result | 80 | Meter result, sentinel comparison and casts, `validate_path` outcome, `Wide` length default and lifetime, stage selector and wildcard argument, join-result initialization and both result tests. |
| Find record formation | 865 | The selected SDK record contains 274 WCHAR and 11 DWORD leaves. Each zero initializer takes three actions. The remaining ten cover aggregate scope events. |
| First search and normal initialization | 65 | First-search arguments, bindings, call and result, converted invalid sentinel, missing-file test and zero extent, reached handle store, two converted integer zeros and three Boolean initializations. Includes native error retrieval for empty-directory success. |
| Record dispatch | 360 | Ten occurrences at 36 for loop entry, meter result, both pseudo-name result tests, flag test and update, ordinary counter and slot selection, and refusal exits. |
| Ordinary record work | 4,606 | Seven records at 658. This includes the filename scan and the work below. The refusing eighth ordinary record uses the dispatch row. |
| Advance search | 225 | Nine occurrences at 25 for search handle and record address, native arguments and result, branch, error capture and EOF selection or loop continuation. |
| Close search and settle extent | 60 | FindClose call and result, owner sentinel store or converted cleanup-mask update and failed result, then the result test and extent copy. |
| Final pool test | 32 | Both extent comparisons, remaining-space subtraction, short-circuit selection and the pool flag update. |
| Two possible local diagnostics | 332 | Two 166-action route allowances. The direct `fail` sites fit the existing 134-action call and caller diagnostics within that allowance. |
| Total | 6,641 | Includes the scan-local filename loop and all selected SDK initialization actions. |

The 658 ordinary-record upper consists of 440 for up to 22 continuing
filename cells, 32 for its final condition, 28 for index and child
initialization and extent check, 28 for join and validation outcomes,
27 for nine converted SDK zeros, 32 for metadata call and result, 32 for
attribute masks and the combined U64 size, 29 for `checked_add` and its
outcome, and 10 for remaining scalar destinations and scope exits.
`checked_add` evaluates and binds three arguments, tests a two-operand
subtraction against the second input, and either returns false or adds and
writes the output before returning true. The 29 allowance includes the
enclosing negation, branch and refusal exit. Metadata and search call rows
cover arguments, native parameter bindings and object addresses. An
error-result read can also be covered by a diagnostic allowance. That
overlap is conservative. Search retirement in `close_safe_owners` belongs
to its own call site.

## Nested path work

| Join region | Upper | Source coverage |
| --- | ---: | --- |
| Caller, entry and return | 24 | Five evaluated and bound arguments, including array decay where needed, dispatch, entry and result. |
| Meter results and amount expression | 14 | Base length, suffix length and their sum, both result tests, short circuit and refusal. |
| Choose a separator | 45 | Nonempty-base test, two indexed comparisons with typed subtraction and character promotions, logical operators and Boolean destination. The narrow separator literals promote to int. |
| Extra and extent | 45 | Conditional U32 extra, three extent tests, subtraction-first operands, short-circuit selection and result. |
| Cell copying | 12,288 | At most 512 iterations across both loops at 24. Each covers test, source address and read, destination address and store with postincrement, and induction and loop events. |
| Setup and final stores | 55 | Converted zero indices and terminal conditions, conditional separator with converted wide store and index update, final converted NUL, length store and scope exits. |
| One join | 12,471 | |

The join's length and pointer parameters enter once per call. The
scan-local setup and ordinary rows include the enclosing result
destinations and receiver expressions. The allowance can include an
extra decay already covered at a nested boundary.

| Validation region | Upper | Source coverage |
| --- | ---: | --- |
| Boundary and path arrays | 24 | Four argument evaluations and bindings, dispatch, entry and return, plus lifetimes of two arrays without source element initialization. |
| Meter results and initial guard | 60 | Length expression, both meter outcomes, reason selection, three path checks with typed zero comparisons, indexed NUL read, short circuit and failure selection. |
| Full- and volume-path calls and folded equality | 110 | Up to 26 for the full-path API and first result, 10 for the length cap and equality outcome, 54 for the folded helper and ordinal API, and 20 for the volume API and result. |
| Volume scan | 10,316 | 513 continuing cells at 20, final condition at 32, plus 24 for converted index zero, the three root checks and refusal selection. |
| Bounded local path copy | 10,798 | 513 cells at 21, plus setup and final condition at 25. This is the reviewed R20 copy row. |
| Ancestor work outside shrinking loops | 7,280 | At most 65 entries at 112, detailed below. |
| Separator trimming and component discovery | 30,540 | 512 combined continuing cells at 50 plus 130 terminating checks at 38. These are the reviewed R22 loop rows. |
| Validation diagnostic | 166 | At most one local path failure, including native error retrieval when reached. |
| One validation | 59,294 | |

The folded helper uses the constant true supplied at this call. Its
54 allowance includes pointer arguments and decays, the DWORD-to-U32
length conversion, argument bindings and entry, equal length, fold
and extent tests, both explicit length conversions from U32 to int,
the five native argument bindings, native call and result, the equality
test and return. The case-sensitive loop has zero occurrences here.

Each ancestor entry has allowances of 16 for depth, diagnostic reason
selection and meter result, 20 for the attributes API and invalid
result test, 19 for the reparse and error path, 28 for the first/ancestor
directory conditions and clearing the first flag, and 29 for the cut
declaration, post-trim test, selection of the next length, converted NUL
store and loop control. Terminal depth refusal reaches no attributes API.
The trim and discovery predicates and stores use the separate 30,540 row.

## Composition

The [loop occurrence proof](2026-11-14-clock-r18-scan-path-census.md)
supplies at most eight joins and eight validations per scan. The reviewed
name test gives `24 + 20L` per call. Two pseudo tests with their enclosing
receiver evaluations cost up to 112 per body. The allowlist's six tests
sum to 1,714 with their short-circuit work. Its wrapper has eight further
actions for caller receiver, member and decay, dispatch, pointer formal,
entry and return. The result is `10 × 112 + 7 × 1,722 = 13,174`.

| Component | Actions |
| --- | ---: |
| Scan-local work | 6,641 |
| Eight joins and eight validations | 574,120 |
| Filename comparisons and allowlist | 13,174 |
| Non-meter upper per scan | 593,935 |
| Prefix-start scan, including one possible cut | 651,320 |
| Tail-start scan | 618,355 |
| Per-helper scan subtree reservation | 683,320,889,175 |
| Three helpers | 2,049,962,667,525 |

These sums depend on the existing [meter and diagnostic envelopes](2026-11-14-clock-r20-scalar-review-delta.md).
Each scan can reach 555 meter calls. A scan starting in prefix uses
`555 × 103 + 220`, allowing one refusal which cuts admission. Subsequent
charges see the tail. A scan starting in tail uses `555 × 44`, with no
refusing charge. The `stage_scan` result's tests and the surrounding
read/write operations belong to the caller.

The [call schedule](2026-11-14-clock-r18-stage-scan-census.md) gives one
setup scan, 259 initial log scans and 1,048,577 scans before native log
writes. The reservation adds two full prefix envelopes for the possible
overlap after the cut, then 309 tail-start publication scans. The two
overlaps conservatively reserve continuations already represented in the
body schedule. Publication follows prefix cleanup. The subtotal is
`(1,048,837 + 2) × 651,320 + 309 × 618,355` per helper.

This is a proposed source bound for the reached scan subtree. Native
output and worker visibility premises, lock and filesystem internals,
other caller work, whole-helper pricing and execution remain open.
