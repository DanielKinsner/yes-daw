# Clock R22 coordinator and cleanup scalar bounds

This proposal covers the local supervisor entry, body poll, target exit,
cleanup and finalization, plus the named meter and counter helpers below.
It uses the exact R22 source and the [scalar convention](artifacts/2026-11-05-clock-research-r9/cpp-census/census.md).
An included helper owns its full call boundary. The caller owns result tests.
The envelopes condition on the existing 134-action `fail` and 220-action
`begin_tail` rows. The primitive boundary includes visible native call
arguments, bindings and results. Native internals retain their existing scope.
The exact R22 guard refuses after State initialization. The body and tail
counts bound the complete source schedule for later admitted entry.

## Entry and admission

| Part of `State state;` | Initialized scalar leaves | Named initialized scopes reserved |
| --- | ---: | ---: |
| Direct State members, including four handle arrays | 32 | 1 |
| Meter, including the SDK lock pointer | 8 | 2 |
| Request, 40 Text values and seven Wide lengths | 93 | 48 |
| Terminal, three Failures and two PipeCounts | 35 | 6 |
| Two Workers, two WorkerExits and four Failures | 74 | 8 |
| Total | 242 | 65 |

Reserve four actions per initialized scalar and eight actions per named
scope for default initialization and lifetime. Four additional actions
cover the signed negation, `LONG_PTR` and `HANDLE` casts in the invalid
handle sentinel. The selected SDK lock initializer forms one null pointer
from integer zero. This makes `242 × 4 + 4 + 65 × 8 = 1,492`. The capture,
digest, Wide cell, worker, launch and native-output byte arrays without
initializers have no source initialization. State lifetime includes no
claim about machine writes, stack probes or physical storage.

| Call or boundary | Upper | Source coverage |
| --- | ---: | --- |
| `admit_body(State&, Family)` and `Meter&` overload | 160 | 20 for outer arguments, formals, stop guard and return, 24 for exclusive lock boundaries, 34 for prefix checks and typed index/price, 64 for limits and outcome, 18 for refused-ticket tail test and frontier stores. A reached `begin_tail` has its own row. |
| `tail_requested` | 32 | 10 for reference and call boundaries, 16 for shared lock/unlock with pointer bindings and entry/return, six for local tail read and result. |
| `counter` on `body_polls` | 68 | 16 for nested arguments, U64 unit conversion, bindings and entry, 32 for `checked_add`, 20 for next value, test, store and return. |
| `workers_settled` | 24 | Reference boundary, two indexed Boolean loads, short circuit and return. |
| `finish` | 32 | Root call with DWORD conversion, late-tail flag tests, conditional code-11 conversion and write, and typed `ExitProcess` argument, binding and nonreturning entry. |

The counter overflow diagnostic has zero calls at this poll site.
The counter starts at zero and increments only below 7,200. Other
`counter` uses retain their own bounds and failure routes.

## Exit query and coordinator

| One `observe_target_exit` call | Actions |
| --- | ---: |
| Boundary, known/child flag tests and returns | 24 |
| Zero-time wait, handle and DWORD arguments, timeout and signal tests | 28 |
| Non-signal arm, error selection, repeated typed WAIT_FAILED test and optional cleanup mask | 45 |
| Exit query, zero DWORD out and address, BOOL result, native error and optional cleanup mask | 35 |
| Successful DWORD-to-U32 store and flag | 16 |
| At most one direct diagnostic | 134 |
| Total | 282 |

Both query alternatives are reserved, including selected SDK macro
operations/conversions and possible `GetLastError`. The non-signal arm
returns before the native exit query. The success store is reached
only after the native signal and successful exit query.

| Coordinator local region | Upper |
| --- | ---: |
| Invocation and exit | 16 |
| Outer iteration before worker visits | 112 |
| One worker direction | 68 |
| Outer iteration after worker visits | 96 |
| One direct loop diagnostic | 134 |

The 112 includes loop control, both ticket outcomes, exit result,
EOF flag mask, stop/cap tests, cap store and counter result. The 68
includes the typed direction loop, indexed worker reference, settled
test, ready result and stop/tail branch. The 96 includes the worker-loop
exit, outer stop/tail branch, clock boundary and U64 result, clock
guards, converted deadline, timeout flag and DWORD sleep boundary.
Any direct poll-limit, clock-invariant or timeout failure breaks the loop.

The reviewed schedule gives 7,201 outer iterations and Coordinator tickets,
7,201 exit observations, 7,200 Poll tickets and counter calls, and at most
21,600 tail checks. One refusal to admit Coordinator or Poll reaches
`begin_tail` and breaks. Reserve `16 + 7,201 × 344 + 134 + 7,201 × 282 +
14,401 × 160 + 7,200 × 68 + 21,600 × 32 + 220 = 7,993,156` before worker
callees.

After a concurrent cut, reserve a full reached iteration plus a possible
next outer admission refusal. This covers both admitted tickets, three
tail checks, the counter, the optional exit observation and diagnostic.
It costs `344 + 282 + 3 × 32 + 68 + 3 × 160 + 220 + 134 = 1,624`.
Worker completions use separate tail rows. This overlap is a conservative
reservation for unknown cut placement.

## Cleanup and finalization

| Cleanup local region | Upper | Source coverage |
| --- | ---: | --- |
| Boundary and initial call control | 24 | State reference, formals, entry, first group and return. |
| Initial two worker directions | 84 | 42 each for typed loop, worker indexing, created/settled checks and ready result. |
| Job termination | 104 | Live boundary, flag and raw-creation checks, typed status argument, BOOL and error, group-failed and mask update. |
| Target wait | 120 | Flag tests, DWORD wait/result conversions, failure retrieval, group flag, mask and observation result. |
| Settlement retry loop | 1,512 | Nine bodies with two 42-action worker directions and 84 for the retry index, settlement result, clock and elapsed comparison, break tests and sleep boundary. |
| Final unsettled check | 72 | Settled result, flags and combined shifted masks. |
| Direct process closes | 136 | Two groups at 68 for pointer-array initialization and loop, indexed dereferences, live/sentinel, native close/result, error and null or mask. |
| Job population | 124 | Job-known/live checks, 36 actions for twelve SDK zero leaves, five typed query arguments including structure address, size and null, and result/count branches. |
| Log flush and position | 124 | Two directions at 62 for handle selection/live, flush and error, shifted mask or selected flag, and observation outcome. |
| Remaining fixed state | 24 | Loop boundaries, local scope retirement and result boundary. |
| Local subtotal | 2,324 | Local control, native calls and scalar updates. |

The pinned SDK job accounting object contains four `LARGE_INTEGER` unions
and four DWORD members. Each union initializes its first two-field variant.
The twelve zero leaves are separate from the query's native output. At
most eight direct diagnostics can execute. The reach counts are one for
termination, one for the target wait or unobserved-exit result, one for
unsettled workers, two for direct closes, one for job query or nonempty
observation and two for flushes. A failed wait sets the group-failed flag
and rules out a second unobserved-exit diagnostic. The nested exit query
owns its diagnostic. Include one `begin_tail`, up to ten `workers_settled`
calls and one exit observation. This gives
`2,324 + 220 + 10 × 24 + 282 + 8 × 134 = 4,138`.

Finalization uses 360 local actions. Reserve 80 for reference and mask
setup and three Boolean bindings, 140 for complete status alternatives
and conversions, 100 for the required-mask expression and outcome, and
40 for validation result, request-current store and control. The
`zero_unavailable_counters` call uses up to 128 for nine converted U64
zero stores, eight indexed pipe selectors, loops and boundary. At most
one direct 134-action failure is reached. The status-2 missing-primary
and status-0/1 mask routes are disjoint. Local finalization totals 622.

## Composition

The preparatory root envelope is `16 + 1,492 + 180 + 160 + 134 = 1,982`.
It covers the `wmain` and supervisor boundary, State defaults, local work,
one TargetResume admission and one resume diagnostic. The 180 local
actions cover five admitted-profile Boolean reads and four possible
short circuits, volatile initialization/read and branch, profile and
setup outcomes, resume check, typed DWORD comparisons and native error,
and timestamp stores. The 412-action root suffix includes 160 for output
tests and outer control, 32 for one `finish` and 220 for the direct early
cut or a refused TargetResume ticket. These two root cut sites are exclusive.

| Reservation | Actions |
| --- | ---: |
| Root and coordinator prefix envelope | 7,995,138 |
| Root setup completion after cut | 474 |
| Already-admitted coordinator work after cut | 1,624 |
| Cleanup, finalization and root suffix | 5,172 |
| Per helper | 8,002,408 |
| Across three helpers | 24,007,224 |

A worker cut during setup can leave bounded root setup and launch
completion. Reserve `180 + 160 + 134` in the tail for the entire root
local setup envelope after State initialization. The launch callees
retain their own completion bounds. The extra root and coordinator rows
are overlapping reservations. Terminal work starts after prefix cleanup.

The [worker settlement schedule](2026-11-14-clock-r19-worker-settlement-census.md)
supplies up to 14,400 normal ready checks, ready-event consume calls and
settle calls each. Cleanup adds up to 20 ready checks, 20 ready-event
consume calls, 20 direct Stop acknowledgements, 18 settle calls, nine
cancellation groups and one `stop_workers`. At most two exited-worker
consume calls are shared across body and cleanup. These worker calls,
their direct meters and diagnostic and native boundaries remain in
separate worksheets. Cleanup reaches up to two `observe_position`
calls and one `close_safe_owners(false)`. An early root refusal can use
that pre-record cleanup in place of the normal one. At most one
`close_safe_owners(true)` follows record freeze.

Root profile, command parsing, request, recipe, launch and worker setup
callees retain their separate bounds. The reviewed
[terminal codec](2026-11-14-clock-r22-terminal-codec-scalar-census.md)
owns up to two validations and one encoder with subtotal 20,202 in the
tail. Publication, close-safe owners, codec meters and scan subtrees
are separate. The [capture meter sheet](2026-11-14-clock-r22-capture-meter-census.md)
gives occurrence bounds, so any composition must count the named
Coordinator, Poll and TargetResume admissions in this worksheet once.
The total remains a proposed source bound conditional on the existing
native visibility, output and lifetime premises.
