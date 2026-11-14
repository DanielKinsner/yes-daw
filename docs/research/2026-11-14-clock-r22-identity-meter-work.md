# Clock R22 request and identity meter work

This join applies the
[source meter boundary](2026-11-14-clock-r20-scalar-review-delta.md)
to the eight [reviewed debit families](2026-11-14-clock-r20-identity-read-census.md)
for request capture, decoding, identity and SHA. The price model
reserves the ordinary prefix `spend` path on every reached site plus
one refusal transition. It does not use the sum of debit arguments
as the number of `spend` calls.

| Scope | Scalar upper per helper |
| --- | ---: |
| 104,932,716 reached `spend` calls at 103 actions apiece | 10,808,069,748 |
| One refusing `spend` can call `begin_tail` and record its diagnostic | 220 |
| Identity and request meter subtotal | 10,808,069,968 |
| [Reviewed local request and hash work](2026-11-14-clock-r22-identity-local-work.md) | 19,215,448,436 |
| Combined subtotal | 30,023,518,404 |

All eight families call `spend` with the coordinator's `State`. They run
before capture workers start. A refused `spend` cuts prefix admission and the
caller propagates failure to the outer early-return path. The outer
cleanup's additional `begin_tail` and owner retirement remain outside
this join. A separate non-meter error may take that outer path as well.
The scale argument and surrounding caller branch belong to the local
caller's selected row. Typed count conversion belongs to `spend`.

The subtotal covers local calls and these eight meter boundaries
subject to the underlying scalar rows and occurrence premises.
Decode's local body, the recipe caller, path validation, closes and
the early-cleanup suffix have separate source accounting.

Independent review reproduced the source schedule and arithmetic. It
accepted this join conditional on the inherited 103-action spend and
220-action refusal rows. The additional early-cleanup cut call can
observe the already reported meter state without repeating its diagnostic.
