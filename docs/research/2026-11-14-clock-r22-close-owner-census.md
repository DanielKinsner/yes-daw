# Clock R22 handle close bounds

This worksheet covers `close_one` in
[the pinned I/O revision](artifacts/2026-11-14-clock-r20-path-copy/review.md).
It joins the helper's typed source actions with its invocation sites.
The existing [failure row](2026-11-14-clock-r20-scalar-review-delta.md)
supplies the diagnostic upper. Closing an unknown or retained native
owner has separate resource and semantic obligations.

| Region | Actions |
| --- | ---: |
| Evaluate caller state, handle reference and mask including shifted or default arguments, initialize parameters and enter | 24 |
| Call `live`, check null and converted invalid-handle sentinel, select close branch | 24 |
| Initialize held copy, evaluate and call `CloseHandle`, check Boolean result | 20 |
| On failure, capture `GetLastError` and update cleanup mask | 22 |
| On failure, `fail` including caller and diagnostic internals | 134 |
| On success or non-live handle, store converted null | 6 |
| Return and retire | 6 |
| Uniform source upper | 236 |

The upper reserves failure and success rows together. A failed native
close leaves the handle unchanged so that a later retirement attempt
still sees the original owner. The default mask is counted only when
omitted at the source site.

| Call region | Upper per helper |
| --- | ---: |
| Hash-file returns, one per identity file | 2 |
| Target setup closes two inherited writes and the NUL input | 3 |
| Terminal publication after successful flush | 1 |
| `close_safe_owners` before a record | 20 |
| `close_safe_owners` after a record | 21 |
| Total | 47 |

Each `close_safe_owners` call can reach 14 loop closes across both
directions, then the NUL, primary thread, process, job, request file and
hash file. Only the after-record call also checks the terminal file.
A setup refusal takes the early cleanup path in place of normal
prefix cleanup. At most 41 close calls occur in the two cleanup
invocations combined. Direct process, search, attribute and heap
retirement use separate sites. The table's uniform subtotal is
47 × 236 = 11,092 actions per helper, conditioned on the inherited
failure row. This source count is separate from native release evidence.

Independent review accepted the invocation and scalar upper under that
failure-row premise. It checked the pinned call sites, mutually exclusive
early and normal cleanup, and native argument and result boundaries.
