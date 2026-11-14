# Clock R22 capture and stage meter calls

These are per-helper source bounds under the same positive-read,
ready/ack and write-accounting premises as the
[read](2026-11-14-clock-r18-worker-read-census.md),
[settlement](2026-11-14-clock-r19-worker-settlement-census.md) and
[scan](2026-11-14-clock-r18-stage-scan-census.md) worksheets.
Every listed debit requests one unit. Calls include refusals and any
required continuation of an admitted group after the cut.

| Family | Calls |
| --- | ---: |
| `StageScan` | 1,049,146 |
| `StageEntry` | 10,491,460 |
| `Owner` | 3 |
| `CaptureSetup` | 1 |
| `TargetResume` | 1 |
| `WorkerStart` | 2 |
| `WorkerRead` | 1,071,174 |
| `WorkerPublish` | 14,402 |
| `Coordinator` | 7,201 |
| `Poll` | 7,200 |
| `Consume` | 14,422 |
| `WriteChunk` | 259 |
| `WriteCall` | 1,048,577 |

Capture setup can admit two log-owner groups. Containment can admit
one more owner group. The outer supervisor starts up to two workers
and gives the target at most one resume ticket.

A worker reaches the read-admission wrapper before each native read.
A refusal at that wrapper ends native reads for that worker and adds
one call beyond the native-read bound. A worker can publish its first
generation and releases each subsequent one only after a positive
continuation acknowledgement. There are at most two such
acknowledgements per normal poll across the two workers. Cleanup and
exit acquisition cannot send Continue. The bound therefore includes
two initial publications plus 14,400 more over the two workers.

The coordinator can re-enter its outer ticket check once after its
last poll. A refused poll ends that loop and can take the place of a
last successful attempt. Consumption can be attempted from 14,400
normal ready checks, 20 explicit cleanup ready checks and up to two
exited-worker acquisitions shared by both paths. Each attempt
evaluates at most one consume debit. A refused normal debit switches
to mandatory accounting in the same invocation.

Only normal consumption can call the chunk writer. The chunk and
pre-write scan schedule bounds the corresponding tickets. The sole
reachable write-admission wrapper is the `State&` overload. Tail
publication uses a separate statically reserved write loop.
Each scan may reach at most ten stage-entry debits.
