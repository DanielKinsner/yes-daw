# Clock R19 worker settlement call bounds

R19 reuses the exact native worker, coordinator, and cleanup functions.
These are per-helper invocation counts from
[sup_main.cpp](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/059-sup_main.cpp).
The normal and cleanup columns bound distinct call sites. An already
admitted normal call can have part of its work sponsored by the tail.

| Call | Normal coordinator | Explicit cleanup | Basis |
| --- | ---: | ---: | --- |
| `ready_now` | 14,400 | 20 | Two directions per 7,200 polls. Cleanup checks both before cancellation and in each of at most nine retry passes. |
| `settle_one` | 14,400 | 18 | One per reached direction in a poll or retry pass. The initial cleanup pass has no settlement call. |
| `consume_ready`, after ready event | 14,400 | 20 | At most one invocation per ready check. |
| `consume_ready`, after thread exit | 2 | 2 | `settle_one` marks that direction settled before acquiring an unconsumed generation. The two columns share at most two such calls. |
| `acknowledge` | 14,400 | 40 | Each unsettled ready-event consumption can acknowledge once. Cleanup also sends one explicit Stop per reached unsettled direction in the initial and retry passes. |
| `CancelSynchronousIo` | 0 | 18 | One initial cancellation pass and at most eight passes between retries, two directions per pass. |

An exit-acquire call skips the event reset and acknowledgement because its
worker is already marked settled. The ready-event invocation counts include
early returns for an already consumed slot. A successful native thread wait
can admit at most two observed worker exit failures from each stable exit
record. The accounting therefore preserves the difference between a helper
invocation, a native call and consumption of a new generation.

The worker settlement retry loop has at most nine bodies and eight interval
sleeps. No assumed early settlement or wait duration lowers those bounds.
The underlying worker visibility, reference and native timing premises
remain open.
