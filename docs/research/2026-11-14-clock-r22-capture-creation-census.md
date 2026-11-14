# Clock R22 native capture and process creation work

This worksheet covers `observe_position`, `open_capture_owners` and
`create_contained_target` in the [pinned I/O revision](artifacts/2026-11-14-clock-r20-path-copy/review.md).
It counts reached source, native formals/out-results and precise
structure initialization. The scan, join, close-one and admission
callees have separate rows. The local `fail` and `path_failure` costs
use the [shared diagnostic envelopes](2026-11-14-clock-r20-scalar-review-delta.md).

## Position check

| Region | Actions |
| --- | ---: |
| Argument evaluation, binding and other boundary work | 24 |
| Two `LARGE_INTEGER` zero aggregates and scope | 28 |
| Up to three complete indexed `live` checks, at 26 each | 78 |
| File-pointer outcome and possible native error retrieval | 40 |
| Position comparison, invalid mask and failure argument expression | 34 |
| Either counter position store and corresponding flag selection | 66 |
| When reached, direct diagnostic | 134 |
| Per call | 404 |

Each aggregate initializes its two DWORD/LONG leaves. The native
file-pointer row includes the by-value `LARGE_INTEGER` argument and
its two initialized fields, result and native destination. The three
liveness sites evaluate the file handle argument and typed sentinel
separately, including the third test when constructing the diagnostic.
Setup can observe both log beginnings, and cleanup can observe both
ends, so reserve up to 808 for each of these phases.

## Capture owners

| Region | Actions |
| --- | ---: |
| Fixed boundary, setup ticket outcome, unknown-creation test, extent and scan outcome | 72 |
| Existing-owner checks across both directions, up to 92 each | 184 |
| Two log-owner groups at 156 each | 312 |
| NUL owner liveness check and native open, output store | 68 |
| Initialize and retire `SECURITY_ATTRIBUTES` | 28 |
| Two pipe groups at 184 each | 368 |
| Optional failure representation copy, two eight-byte calls at 32 fixed plus 23 per byte | 432 |
| One possible local diagnostic route | 166 |
| Total | 1,630 |

Each owner precheck direction may call `live` three times at 26
actions. Remaining actions cover typed loop control, short circuits
and exit. The rest of the NUL owner check is in the fixed open row.
Each log group includes 34 for loop/ticket/local path and join
result; 42 for open, result and liveness; 44 for owner assignment,
the two-field converted zero and by-value file-pointer argument with
native result; and 36 for position outcome, error retrieval, plus any
additional indexed `live` boundary work. Each pipe group includes 48 for output-pointer setup, four
typed native arguments, result and conditional native error; 62 for
short-circuit results and both typed liveness tests; 44 for confirmed
owner assignment and inheritance-clear API with error result; and
30 for the private flags, refusal and scope events. There can be
one failed-pipe pair since both raw-copy alternatives return. The
copy's two eight-byte calls therefore total `2 × (32 + 8 × 23) = 432`.
Any local failing diagnostic leads directly to return. A separately
sponsored position failure can also end the phase.

## Contained target

| Region | Actions |
| --- | ---: |
| Boundary, ticket outcome, state/handle prechecks, job creation, native result and flag | 150 |
| Job limit structure and set-information call | 114 |
| Initial sizing query, allocation and initialized-list calls, including error and typed guards | 166 |
| Inheritance array formation and both attribute updates | 128 |
| Startup and process aggregate initialization | 114 |
| Six startup field updates and scope setup | 67 |
| Ten-argument process creation, output, result and conditional native error | 104 |
| Result owner validity, success assignments or failure flags and retirement | 150 |
| Three post-create close results and loop control | 60 |
| Optional 24-byte failed-process capture at 32 fixed plus 23 per byte and 16 scope actions | 600 |
| One possible local diagnostic route | 166 |
| Total | 1,819 |

The job limit structure includes 21 zero-initialized scalar leaves,
counted at three actions each. Two `LARGE_INTEGER` values contribute
two leaves each. Startup has 19 initialized scalar leaves including
the attribute pointer, and process information has four. Their
aggregate-scope and typed zero actions are in the initialization row.
The native process call receives evaluated mutable command and
environment pointers, two null security pointers, four creation
flags and both structure addresses. The failed return and the
invalid-owner success branch both retain a raw representation of
the same private process object. They are exclusive, so only one
24-byte copy is needed. The allocation term is checked against the
bound size in the source. The sizing and initialization calls retain
their own arguments, return, native error and failure branches.

These local schedules add **5,065 actions per helper** before
separate admissions, path and retirement callees. The 808 actions
for end positions belong to tail. The remaining upper is charged
before worker creation. This sheet gives no numeric Win32 backing or
timing. Independent review matched the exact R22 pins, SDK aggregate
definitions, raw-copy paths and arithmetic, and accepted this upper.
