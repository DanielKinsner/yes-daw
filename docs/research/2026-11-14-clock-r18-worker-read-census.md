# Clock R18 worker read call bounds

Scope is the two capture workers in the
[frozen native source](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/059-sup_main.cpp).
The bounds apply to one helper invocation and include all reached calls in
failure and cleanup schedules. They count source calls, with the positive
result bound conditioned on the reviewed ready/ack acquire and visibility
semantics.

| `ReadFile` return class | Calls across both workers |
| --- | ---: |
| Valid positive progress | 1,056,768 |
| Successful zero progress | 14,402 |
| Native failure or out-of-extent success | 2 |
| Total | 1,071,172 |

## Positive progress

Each call classified as positive advances `active_fill` by at least one byte
and at most the remaining capacity. The worker resets the fill only after a
completed result receives Continue. For a positive Full or EofShort result,
the coordinator offers Continue only after the whole result has been written
and accounted. The two logs share a 1,048,576-byte cap. NoProgress preserves the
fill. For each direction, at most one additional 4,096-byte fill can remain
uncommitted at the end, including an acquired slot, a partly written chunk, a
read error with a retained prefix, or a read admitted just before the cut.
This gives `1,048,576 + 2 × 4,096` positive calls.

## Zero progress and terminal results

A successful zero return publishes one NoProgress generation and leaves the
worker waiting for acknowledgement. Only the normal coordinator polls can
give Continue, at most 7,200 times per direction. One further zero result per
direction can occur before Stop, or remain pending when settlement is unknown.
Cleanup uses Stop and cannot renew a generation.

Each direction has at most one native failure or validated-out success that
ends reading. A non-broken-pipe failure or out-of-extent result publishes Error.
Broken pipe with a partial fill publishes EofShort, then the worker can publish
synthetic ZeroEof without making another native read. Broken pipe with an empty
fill publishes ZeroEof directly. A stop or read-ticket refusal before the
native call contributes no `ReadFile` occurrence.

Across three helper invocations the call bound is 3,213,516. Every reached read
has its own prefix ticket. At most one admitted native read per direction can
remain to complete after the cut. Worker publication, acknowledgement, final
slot accounting, and native settlement still require their own cost rows.
