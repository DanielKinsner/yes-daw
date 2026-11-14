# Clock R22 terminal codec scalar bounds

Scope is `validate_terminal` and `encode_terminal` in the
[exact R22 codec](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/056-sup_codec.cpp).
The upper counts caller argument formation, primitive calls and nested
helpers. The three literal meter sites retain separate meter rows.
Finalization and terminal publication retain their own local work.

## Transitive leaves

| Helper | Per-call upper | Basis |
| --- | ---: | --- |
| `pair` | 40 | 14 for nested field arguments, bindings and entry. 10 for two range tests and branches. 16 for selected switch label, at most two site comparisons, conversions and return. |
| `same_failure` | 52 | 16 for references, bindings, dispatch and return. 36 for four field pairs, comparisons and three short circuits. |
| `reason_present` | 40 | 12 for caller and call boundary, 18 for three nested-member loads and comparisons, and 10 for logical operators, short circuits and return. |
| `checked_add` | 32 | 14 for the larger indexed argument/binding case. 18 for subtraction, overflow test, selected branch, sum and output write or early return. |
| `equal_digest` | 790 | 22 for two nested digest reference arguments and bindings, call, loop setup/exit and return. Each of 32 iterations has up to 24 for loop, indexed operands and refusal branch. |
| `put32` | 132 | 36 for computed output pointer/value, call, loop setup/exit and return. Four loop bodies of up to 24 for typed limit, branch, increment, loop event, byte shift, cast and indexed store. |
| `put64` | 228 | The same fixed 36 plus eight bodies of up to 24. |

## Terminal validation

| Local region | Upper |
| --- | ---: |
| Outer call, meter outcome, terminal reference/flags | 24 |
| Initial mask, index and status tests | 96 |
| Pair outcomes, reason-48 native checks and specific cleanup bit | 72 |
| Three native-known flags and primary checks, including mask and zero conversions | 96 |
| Secondary presence, saturation, known/error comparisons and comparison result | 184 |
| Creation, PID, exit and prefix checks | 120 |
| Four typed accumulator zeros | 12 |
| Each two-direction validation body including its declarations, group guards and all callee outcomes | 272 |
| Pipe loop fixed setup/exit | 12 |
| Totals and body poll guard including add outcome | 54 |
| Timeout and poll flags and reason outcomes | 44 |
| Log and stage cap, omission and reason outcome checks | 68 |
| Stage guard's five reason visits, found flag and control | 80 |
| Settle mask and status-0/1 required/forbidden and scalar tests | 192 |
| Status-0/1 positions/confirmed output checks in both directions, including loop | 144 |
| Status-2 and status-3 entry tests and status-4 diagnostic test | 100 |
| Each status-3 pipe condition group including ternary flag selection and shifted cause masks | 144 |
| Final digest outcomes, return and local lifetime events | 100 |
| Local upper across all branches | 2,230 |

The primary pipe loop reaches at most four `checked_add` calls per
direction for known coordinator counters, plus one partial accumulator
addition. One further addition follows the loop, giving eleven in
total. There are three pair calls, one possible failure comparison,
ten reason calls, and three digest calls. Ten reason calls allow the
one reason-48 check, one each for timeout, poll, log and stage caps,
and all five stage-guard reason visits.

The validation upper is `2,230 + 3 × 40 + 52 + 10 × 40 + 11 × 32 +
3 × 790 = 5,524`. It intentionally includes the local checks in
mutually exclusive status branches.

## Terminal encoding

| Local region | Upper |
| --- | ---: |
| Outer call, results of the two spends and validation, pointers and exit | 36 |
| Copy eight magic bytes, including loop | 224 |
| Form the sixteen `U32` fields and their array lifetime | 144 |
| Loop overhead around sixteen `put32` calls and the final `put32` call | 476 |
| Copy all three digests, including loop | 1,728 |
| Both pipe groups and their local positions/array/loop work | 426 |
| Local upper | 3,034 |

The seventeen `put32` calls each use at most 132. The three header
64-bit writes and fourteen pipe-counter writes each use at most 228.
The encoding upper excluding validation and meters is
`3,034 + 17 × 132 + 17 × 228 = 9,154`.

Finalization and encoding can reach at most two validations and one
encoding. The local terminal-codec subtotal is therefore
`2 × 5,524 + 9,154 = 20,202` actions per helper. It is part of the
mandatory tail. Independent review checked the five native pins, call
and loop counts, typed operations and arithmetic, and accepted this
scoped upper.
