# Clock R22 worker acquisition and cleanup scalar bounds

This proposal covers `ready_now`, `settle_one`, `consume_ready`,
`acknowledge`, `stop_workers` and `cancel_workers` in the exact R22
native source. It complements the [worker-side worksheet](2026-11-14-clock-r22-worker-thread-census.md)
and uses the [settlement schedule](2026-11-14-clock-r19-worker-settlement-census.md)
and [coordinator and meter rows](2026-11-14-clock-r22-coordinator-census.md).
Each included helper owns its caller boundary. Outer result conditions
remain with their callers. Mutex, native visibility, output and owner
contracts retain their qualifications.

## Ready, acknowledge, stop and cancel

| Helper | Local upper | Direct `fail` upper | Source coverage |
| --- | ---: | ---: | --- |
| `ready_now` | 64 | 134 | State/worker references, handle and converted DWORD wait timeout, formal bindings, return and typed WAIT macro tests, conditional GetLastError, DWORD error and shifted mask. |
| `acknowledge` | 64 | 134 | Three formals and entry, enum choice and explicit LONG conversion, indexed volatile pointer and native exchange, event call and outcome. Immediate DWORD capture and shifted-mask update remain on the reached failure path. |
| `stop_workers` | 72 | 0 | One boundary, converted loop constants and induction, two indexed worker/stop addresses, two typed LONG exchange boundaries and returns. |
| `cancel_workers` | 160 | 268 | One boundary and two typed direction loops, indexed worker reference, short-circuit created/settled guards, CancelSynchronousIo and result, immediate DWORD error, ERROR_NOT_FOUND test and mask. At most one diagnostic per direction. |

Wait and event results use the actual DWORD or BOOL forms. A LONG
interlocked operation includes the volatile pointer argument and
existing primitive boundary. A failed event or cancellation call
can reach its own diagnostic even if a previous cleanup operation
already failed.

## Settlement and consumption

| One `settle_one` call | Local actions |
| --- | ---: |
| Boundary, indexed worker binding, uncreated or settled paths | 56 |
| Zero-time thread wait and selected outcome | 48 |
| Stable exit binding, ready-consumed test and nested consume outcome, validity tests | 80 |
| Partial/flag/mask updates, casts and observed errors | 88 |
| Two possible reached exit-failure loop bodies, including bindings and enum casts | 72 |
| Final unrequested-stop tests and alternate invalid/error handling | 40 |
| Local total | 384 |

The first group includes both zero-direction Boolean selections and
the corresponding status flag writes when the worker was never
created. The wait group includes the formal timeout conversion and
typed WAIT_OBJECT_0, WAIT_FAILED and WAIT_TIMEOUT conditions. The
stable record group checks kind, partial-known, partial length, the
two-failure bound and overflow. U32 partial widens to U64 in the
comparison and store. The loop reads each observed item, casts site
and reason, and supplies the actual error to `fail`. Its direct
diagnostics are separate. The success path sets `settled` before any
exit-slot consumption, so one exited-worker acquisition is possible
per direction. The optional tail query in the final stop test can
occur at most twice across all settlement calls.

One signaled valid record can project two exit failures and one
partial mismatch. The unrequested-stop test requires failure_count
zero, so it cannot add a fourth. An invalid record or failed wait
takes one diagnostic. Reserve three at 134 per settle invocation,
yielding `384 + 3 × 134 = 786` exclusive of optional exited-worker
consumption and tail queries.

| One `consume_ready` call | Local actions |
| --- | ---: |
| Reference boundary, indexed worker binding, created and consumed guards and meter result | 56 |
| Generation bookkeeping, record kind, outcome flags and initial Bool | 36 |
| Full, EOF-short and ZeroEof shape tests, indexed PipeCount and count/callee outcomes | 104 |
| NoProgress/Error and invalid kind, widened partial and direction-indexed masks | 84 |
| More/stop/tail tests, ResetEvent and native error, optional acknowledgement, return | 104 |
| Local total | 384 |

One slot's kind accounts through one shape route. At most one
direct record diagnostic and one ResetEvent diagnostic can execute
in an invocation. Acknowledgement has its own event and error row.
The counter routes use two 68-action bounds including the converted
U32 increments and nested checked additions. The slot cap is 4,096,
so even summing every admitted generation at the maximum gives
`14,402 × 4,096`, below U64. An unretained count decreases only after
a checked write. This bounds both counters and excludes overflow
of the two source counter additions. The invocation upper is
`384 + 2 × 134 + 2 × 68 = 788` before its meter, tail queries, optional
acknowledgement and writer. The two tail-query sites can each be
reached only on a normal-direction consumption route.

A normal receive uses the 160-action State admission and can
add the separate 220-action refusing transition. A cleanup receive
uses `spend` with the converted default U64 count. An exited-worker
acquisition invoked from a normal settlement may enter that cleanup
branch before the cut. Its pre-cut allowance is 103, with one
refusing transition possible. A consume refusal cuts admission
and completes the reached slot as cleanup. A cleanup invocation
continues its mandatory accounting if a reached `spend` refuses.

## Per-helper composition

The 7,200 polls permit 14,400 normal ready checks, ready-event
consumes and settlement checks each across two directions. Use one
normal acknowledgement per ready-event consume, two normal
tail queries and one State admission. Up to two further existing
exited-worker consumes receive the 103-prefix allowance. They
never acknowledge a worker that is already marked settled. Reserve
up to two settlement tail queries, and one refusing transition
across either normal consumption mode. The prefix envelope is
`14,400 × (198 + 786 + 788 + 198 + 2 × 32 + 160) +
2 × (788 + 103 + 32) + 220 = 31,595,666`.

The reached cleanup group begins after a cut. It permits twenty
ready checks and event consumes, eighteen settles, twenty explicit
Stop acknowledgements and up to twenty more within its event
consumes. Reserve both shared exited-worker consumes again for
unknown placement, their tail queries and the 44-action tail
charge. Include nine cancellation groups and one stop group.
This gives `20 × 198 + 18 × 786 + 22 × (788 + 44) +
40 × 198 + 2 × 32 + 9 × 428 + 72 = 48,320`.

A cut can leave both directions of one already-admitted poll within
the conservative local envelope. The coordinator checks tail state
before the next poll. Reserve two whole direction groups with their
admission result and possible BeginTail. Reserve both exited-worker
acquisitions and settlement tail queries once more. This covers a
prefix charge partly completed at the cut and a possible refusing
transition. These are overlapping source reservations, with upper
`2 × (198 + 786 + 788 + 198 + 2 × 32 + 160 + 220) +
2 × (788 + 103 + 32) + 220 = 6,894`.

| Reservation | Actions |
| --- | ---: |
| Normal receive and settlement envelope | 31,595,666 |
| Cleanup receive, stop, settlement and cancellation | 48,320 |
| Normal receive completion after cut | 6,894 |
| Per helper | 31,650,880 |
| Across three helpers | 94,952,640 |

The log writer and its callee boundary remain separate, including its
own short writes, diagnostics, meters and stage scans. Its local
result and the consume outcome belong to this worksheet. The
[capture occurrence ledger](2026-11-14-clock-r22-capture-meter-census.md)
counts the Consume family. This worksheet prices the reached
Consume operations and the direct settlement/consume meter and
diagnostic paths. The coordinator's own admission, worker-entry
and native retirement work retain their assigned rows. Source
pricing and phase admission still depend on the native profile and
integrated source review. Independent review accepted these typed and
overlap rows, with native destination writes accounted separately.
