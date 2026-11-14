# Clock R22 log writer scalar work

The [pinned I/O revision](artifacts/2026-11-14-clock-r20-path-copy/review.md)
contains `write_chunk` and `native_count`. These rows price their local
work, count results and write ticket operations. Each directory scan
retains its own [scan subtree reservation](2026-11-14-clock-r22-scan-final-census.md).
One entry from `consume_ready` owns this full call boundary.

| Region | Actions |
| --- | ---: |
| Fixed writer call, chunk admission outcome and pipe counter reference | 44 |
| Aggregate confirmed-write total, including checked-add leaf and result | 64 |
| Cap/refusal, initial scan, omission outcome, and loop setup or return | 108 |
| Possible local fixed diagnostic including branch operands | 166 |
| One reached write-loop iteration before separate leaves and diagnostics | 160 |
| One possible local loop diagnostic including branch operands | 166 |
| One `native_count` call with potential diagnostic | 246 |

The local loop includes up to 20 for length/progress tests and loop
control, 32 for the complete scan outcome and conditional omitted
count path, 12 for the write ticket result, 46 for Win32 write and
result, 28 for short/zero/count/extent checks, and 22 for both progress
updates or source exit. Win32 binds the log handle, byte
address with offset, remaining DWORD, pointer to the converted DWORD
count, and null overlapped argument. The native buffer and scalar
outputs are distinct from local initialization. The loop can compute
remaining length several times within these components.

The `native_count` call includes 26 for caller index/count expression,
bindings and entry, 32 for `checked_add`, 54 for local zero, status
comparison, flag updates, result and return, and 134 for the possible
diagnostic. Actual positive confirmed work admits exactly one such
count update at a time. A cap path can use an omitted counter instead.
The bound reserves these leaves even on prefixes stopping after the
corresponding native failure.

There can be up to 259 chunk invocations and 1,048,577 write-loop
iterations. At most one `native_count` per invocation can occur in its
fixed cap/initial scan path. A loop iteration can reach one such call
for either a scan-pool refusal or a positive result. This gives
1,048,836 calls. Each fixed chunk can encounter only one direct
fixed diagnostic. The first loop failure ends body writes. Include
one direct loop diagnostic, separate from nested count and scan calls.

Use 160 per State admission, the same fixed bound as
[coordinator tickets](2026-11-14-clock-r22-coordinator-census.md).
The write-ticket wrapper adds 12 for its own boundary. The writer
uses only the State overload. There are at most 259 chunk and
1,048,577 write tickets. Reserve a 220-action cut transition for a
refusing admission. It can replace another refusing operation in
the whole body, or be kept as an overlap reserve.

| Component | Formula | Actions |
| --- | ---: | ---: |
| Local writer excluding scans and counts | 259 × 382 + 1,048,577 × 160 + 166 | 167,871,424 |
| Count calls | 1,048,836 × 246 | 258,013,656 |
| Tickets and refusal | 259 × 160 + 1,048,577 × 172 + 220 | 180,396,904 |
| Body subtotal | Sum | 606,281,984 |

An already-admitted writer can finish its current continuation after
the cut. Reserve one full fixed chunk, two loop bodies, two count
calls, a chunk and two write tickets, and one direct loop diagnostic
and refusal. This adds `382 + 2 × (160 + 246 + 172) + 160 + 166 + 220
= 2,084`. The two potentially overlapping directory scans are already
reserved in the separate scan worksheet. The fixed terminal writer
uses a separate cleanup schedule.

The proposed subtotal including this overlap is **606,284,068**.
Independent review verified the typed call and result paths and the
overlap through the next refused write ticket. Native write timing and
resource use remain separate.
