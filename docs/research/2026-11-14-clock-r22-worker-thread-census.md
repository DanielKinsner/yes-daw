# Clock R22 worker entry and publication scalar bounds

This proposal prices `start_workers`, `worker_entry`, `worker_error`,
`worker_exit_error`, `finish_worker` and `publish_ready` in the exact
R22 native source. It includes their reached meter boundaries and uses
the [scalar convention](artifacts/2026-11-05-clock-research-r9/cpp-census/census.md),
the [worker read schedule](2026-11-14-clock-r18-worker-read-census.md)
and the [capture occurrence ledger](2026-11-14-clock-r22-capture-meter-census.md).
The parent-owned setup outcomes and receive-side calls retain separate rows.
Native internals, visibility, scheduling and object lifetimes retain their
existing qualification.

## Source rows

| Region | Actions | Typed and control work |
| --- | ---: | --- |
| `start_workers` fixed boundary | 28 | State reference, call and return, converted U32 index zero, final test and loop exits. |
| One setup group | 400 | 40 for loop, ticket result and indexed worker binding. 92 for the State pointer write, created and four live tests, including each LONG_PTR/HANDLE sentinel. 138 for three event creations and their checks. 80 for CreateThread and result. 50 for started wait, result and immediate native error. |
| Worker thread entry and started signal | 64 | Native callback binding and entry, explicit void-pointer cast, references, typed SetEvent boundary and result, immediate GetLastError and error-route result control. |
| One worker generation outside inner read attempts | 148 | 24 for stop atomic, 40 for six record resets, 36 for EOF-short or open-operation setup and loop exits, 48 for copied kind, publication result, exit-kind selection, active-fill reset and generation control. |
| One reached read-loop body | 192 | 32 for active-fill test, stop atomic, read-ticket outcome and early control. 48 for typed ReadFile, its out count, BOOL/selected DWORD error and boundary. 64 for the broken-pipe and other native error routes, count extent, partial/error formation and EOF classification. 48 for zero/positive result, count update, fullness test and iteration control. |
| `worker_error` | 52 | Four caller operands, pointer/reference and scalar formals, error default and conditional DWORD conversions, five worker-field writes and return. |
| `worker_exit_error` | 84 | 27 for nested Failure formation and 16 for its assignment. Remaining 41 cover caller boundary and defaults, failure-count test, nested selector and postincrement, overflow flag and return. |
| `finish_worker` | 84 | Reference and enum arguments, nested WorkerExit writes, partial and kind selection, error-route check and result, Boolean and DWORD return operations. The nested error call is separate. |
| `publish_ready` local work | 176 | 28 for reference, charge result and generation check, 26 for generation and ready-consumed writes, 36 for SetEvent and one immediate native error, 36 for ack wait and typed error route, 50 for both LONG interlocked comparisons, selected enum, short circuit, tail outcome and returns. |
| Worker `charge` boundary and body | 96 | Uniform prefix and tail allowance from the 90-action prefix charge row, covering the local receiver and this direct call's default U64 unit conversion. No outer State spend wrapper is reached. |
| Worker `request_tail` | 72 | Typed meter reference, exclusive acquire/release with prototype arguments, tail test and frontier copy. |
| Worker `admit_read` | 172 | 12 for call boundary and the wrapper return around the Meter admission. The 160-action coordinator admission allowance covers the shared Meter overload and its typed boundaries. A worker read ticket does not call the State overload or BeginTail. |

CreateEventW has four evaluated and typed arguments. Boolean mode
literals bind BOOL formals. CreateThread has six formals including the
callback, worker context pointer, SIZE_T stack-size zero, DWORD flags
zero and null thread-ID output pointer. The setup group observes the
live handle after each creation. A non-live result and wait failure are
direct setup diagnostics. Each ends `start_workers` immediately.

The selected interlocked operations use volatile LONG pointer formals
and LONG value formals. The source rows count pointer formation,
integer or enum conversion, scalar arguments and returned value tests.
ReadFile binds five arguments, including buffer address, the computed
remaining U32 converted to DWORD, `&got` and null. The DWORD local
zero conversion and native result are distinct actions. Positive
progress returns through the typed DWORD-to-U32 fill update. A failure
or extent rejection terminates the generation, and zero progress
publishes a slot with its current fill preserved.

## Reached work and overlap

Two setup groups can reach two State admissions at 160, at most one
direct diagnostic at 134 and at most one refusing transition at 220.
The setup reservation is `28 + 2 × 400 + 2 × 160 + 134 + 220 = 1,502`.

| Worker-side item | Calls or bodies per helper |
| --- | ---: |
| Entry and final return | 2 each |
| Local outer generation work | 14,404 |
| Local read-loop bodies | 1,071,176 |
| Actual WorkerRead admissions | 1,071,174 |
| Publication, charge and publication tail check | 14,402 each |
| Worker error | 2 |
| Worker exit error | 4 |
| Worker-side explicit cut | 2 |

Every entering worker can attempt an initial publication. Later
generations need a Continue acknowledgement and the worker's own stop
and tail checks. Up to 7,200 normal polls supply at most one Continue
per direction each. This bounds publications by 14,402 and allows one
extra entered outer stop test per worker that can return without
publishing. The read bound adds up to two refusing tickets to the
1,071,172 native reads and another two entered bodies for an early
stop with no ticket. Each read termination substitutes for progress in
that direction. Starting and synchronous worker exit are alternatives
to these longer schedules when they fail earlier.

Each worker has at most 7,201 publications. The U64 generation
overflow branch has zero occurrences under this schedule. One
worker-error helper can be reached per direction before that worker
exits. Reserve up to two worker-exit errors per direction for a fatal
start or publication error and the optional unfinished-operation
record. The final helper only synthesizes that record when its exit
failure count is zero, so this reservation includes incompatible
branches. A refused publication charge can precede the explicit cut
lock in both workers. Each can enter the refusal route once, and
after either cut publishes the tail state all later charges use the
tail branch. The bound does not assume one worker's cut precedes the
other worker's first refusal.

The worker envelope is
`2 × 64 + 14,404 × 148 + 1,071,176 × 192 + 2 × 52 + 4 × 84 +
2 × 84 + 14,402 × (176 + 96 + 32) + 1,071,174 × 172 + 2 × 72
= 396,418,600`. The publication tail check uses the
[32-action tail query](2026-11-14-clock-r22-coordinator-census.md).

For an unknown cut placement, retain the full 1,502 setup envelope in
tail as well. This covers a reached setup group finishing and a later
ticket refusing. Per worker also reserve an entry and started signal,
two outer generation envelopes, two read-loop bodies and tickets,
two publication/charge/tail-query groups, terminal error and two
exit-error envelopes, one final exit and one worker cut. A pending
Continue can have passed its last tail test just before the cut. The
current publication suffix and one new generation can then be reached.
At most one already-admitted native read per direction can return after
cut, with a later attempted ticket refusing before native I/O. A Stop
or a refused read may still publish its Error slot. Further Continue
cannot renew admission after the worker observes the cut.

| Reservation | Actions |
| --- | ---: |
| Setup and complete worker envelope | 396,420,102 |
| Overlap for setup completion | 1,502 |
| Overlap for both workers | 4,144 |
| Per helper | 396,425,748 |
| Across three helpers | 1,189,277,244 |

The overlapping reserves use the same source uppers to cover work
already represented in the full worker schedule. Worker start and read
admissions, publication charge, worker tail query, worker error
construction and setup diagnostics are included once in each selected
full path. The receive-side reset, slot recording, acknowledgement,
position/log writes and worker cleanup retain separate source rows.
Independent review accepted the typed source and overlap rows. A
separate destination-store worksheet covers additional native reads
and atomic destinations. Native time and execution remain open.
