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
| `write_chunk`, initial scan | 259 | At most 256 completed 4,096-byte Full chunks, two EOF-short chunks and one failed chunk. Only normal `consume_ready` can write. |
| `write_chunk`, scan before a short write | 1,048,577 | At most 1,048,576 iterations reach a successful, accounted positive write. At most one other iteration reaches a scan and ends the log-write schedule. |
| `publish_terminal` | 309 | At most one initial scan and at most 308 scans inside the terminal-write loop. Each continuing write advances at least one byte, and a refusal ends the loop. |
| Total | 1,049,146 | Sum of the four source-site bounds, including the setup and publication paths reached on a failure prefix. |

The two log counts share one cap. Before each `write_chunk` accepts its request,
it compares the full requested length against the remaining cap. A continuing
write advances by a confirmed count no larger than the remaining chunk. Its
successful count update and decrement of unretained bytes precede the next
iteration. No other thread writes either log or resets these counters during the
helper's body. The worker publishes an EOF-short slot once and then changes to
ZeroEof. The coordinator never writes a ZeroEof, Error or NoProgress slot.
Every failed chunk either raises `stop_body` or cuts prefix admission. The
occupied search owner denotes an earlier close failure, which already raised
`stop_body`. Setup and cleanup never call the chunk writer. Those paths leave at
most one failed chunk across both directions, including a final scan that
refuses before the write or a native result that fails accounting afterward.

## Expansion within each scan

Per call, `StageScan` spend and `FindFirstFileW` can each be reached once.
The two seen flags and the ordinary-record counter limit the loop to ten
`StageEntry` spends, nine `FindNextFileW` calls, and one in-scan `FindClose`.
Later retirement of a retained search owner belongs to `close_safe_owners`.
The one stage path check and up to seven ordinary-record checks yield at most
eight `validate_path` calls, eight `join_path` calls, and seven metadata calls
to `GetFileAttributesExW` within the scan. The eighth ordinary record refuses
before any of those per-record operations.

Across up to three helpers, these bounds yield 3,147,438 scan calls and
31,474,380 loop body entries.

## After the prefix cut

Setup can finish its one admitted scan. In the log writer, a cut at the
initial scan can still leave both that scan and the first pre-write scan
with post-cut work. A cut during a later scan or an admitted short write
can leave at most one pre-write scan with post-cut work. The next
`admit_write` refuses fresh work even if `stage_scan` finishes successfully.
The cut is permanent and one coordinator owns all these calls, so two
scans cover the post-cut remainder of the body. All 309 terminal
publication scans are reached after `begin_tail`. This gives an upper of
311 scans with post-cut work per helper, including partial scans.

The prefix and tail can both sponsor parts of the same scan. Pricing
still needs the exact branch, meter, and retirement actions.
Win32 time and storage, scalar prices, and the whole source work upper remain
pending.
