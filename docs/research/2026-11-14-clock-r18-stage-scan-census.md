# Clock R18 stage-scan call bounds

This covers the `stage_scan` call sites in the
[R18 source receipt](artifacts/2026-11-14-clock-r18-provenance/receipt.json).
R18 uses the [five R17 native sources](artifacts/2026-11-06-clock-owned-supervisor-source-r4/archive-manifest.json).
The bounds are source occurrence counts for one authorized attempt's fixed
compiler, producer, and verifier call order.

## Per helper invocation

| Caller | Scan bound | Source basis |
| --- | ---: | --- |
| `open_capture_owners` | 1 | One setup call with a single scan before either log is opened. |
| `write_chunk`, initial scan | 14,400 | Only normal `consume_ready` can call `write_chunk`. The coordinator admits at most 7,200 polls and calls it at most once per direction per poll. All acquired-slot cleanup calls pass `cleanup=true`. |
| `write_chunk`, scan before a short write | 1,062,976 | At most 1,048,576 iterations reach a successful, accounted positive write. Every other reached scan ends that chunk's loop, at most once per chunk. This includes refusal of the scan or write ticket, failed or invalid native write progress, and accounting refusal. |
| `publish_terminal` | 309 | At most one initial scan and at most 308 scans inside the terminal-write loop. Each continuing write advances at least one byte, and a refusal ends the loop. |
| Total | 1,077,686 | Sum of the four source-site bounds, including the setup and publication paths reached on a failure prefix. |

The two log counts share one cap. Before each `write_chunk` accepts its request,
it compares the full requested length against the remaining cap. A continuing
write advances by a confirmed count no larger than the remaining chunk. Its
successful count update and decrement of unretained bytes precede the next
iteration. No other thread writes either log or resets these counters during the
helper's body. A failing iteration is covered by the per-chunk allowance.

## Expansion within each scan

Per call, `StageScan` spend and `FindFirstFileW` can each be reached once.
The two seen flags and the ordinary-record counter limit the loop to ten
`StageEntry` spends, nine `FindNextFileW` calls, and one in-scan `FindClose`.
Later retirement of a retained search owner belongs to `close_safe_owners`.
The one stage path check and up to seven ordinary-record checks yield at most
eight `validate_path` calls, eight `join_path` calls, and seven metadata calls
to `GetFileAttributesExW` within the scan. The eighth ordinary record refuses
before any of those per-record operations.

Across up to three helpers, these bounds yield 3,233,058 scan calls and
32,330,580 loop body entries. A normal write-path scan uses an admitted chunk
group and may finish after a concurrent cut. Terminal publication scans are
reached after `begin_tail`. Pricing still needs to
allocate the reached actions to the exact prefix, admitted completion, and
mandatory retirement schedules, including the meter and each refused scan.
Win32 time and storage, scalar prices, and the whole source work upper remain
pending.
