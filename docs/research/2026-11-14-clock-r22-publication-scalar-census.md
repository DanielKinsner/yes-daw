# Clock R22 terminal publication and owner retirement work

These rows cover the two exiting I/O helpers in the
[pinned R20 I/O revision](artifacts/2026-11-14-clock-r20-path-copy/review.md)
inherited by R22. The source transitive rows for `close_one`,
`encode_terminal`, `stage_scan` and `validate_path` own their respective
call boundaries. The primitive API entries, formals, results and
branches reached directly in these functions are included here.

## Terminal publication

| Region | Actions |
| --- | ---: |
| Caller, entry, encode outcome, refusal assignment, freeze and zero extent | 48 |
| Initial scan and transport-validation outcomes, tail-failure store and branch | 32 |
| Form terminal path pointer, call seven-argument `CreateFileW`, commit owner result, evaluate live-handle test | 60 |
| Progress initialization, final condition, flush success test, close result and scope exit | 60 |
| One possible local failure, including native-error retrieval and return | 192 |
| One write-loop body including the continuing test | 92 |
| Per helper at 308 iterations | 28,728 |

Each write-loop body allows up to 10 for loop control, six for the
scan outcome, six for the converted DWORD zero and lifetime,
38 for the five-argument `WriteFile` call and out-result, 22 for two
typed extent tests and the result conditions, and ten for progress or
exit. The output pointer evaluation includes the array decay and
addition. A continuing write advances by at least one byte within the
remaining 308-byte record. A refusal replaces a possible continuing
iteration. Native create, write, malformed count and flush failures
each return, so one direct diagnostic sponsor suffices. Failures from
the transitive calls keep their existing separate sponsors.

## Safe owner retirement

| Region | Actions per invocation |
| --- | ---: |
| Call and return including after-record input and invalid-freeze branch | 18 |
| Both worker iterations, loop and eligible-group tests | 88 |
| Monitor child/exit and job flags, including both conditional `live` calls | 76 |
| Attribute pointer test, `DeleteProcThreadAttributeList` and null store | 30 |
| Test heap owner and call `GetProcessHeap` and `HeapFree`, including typed zero and result | 38 |
| Capture the failed heap error and update the mask, or clear the pointer | 59 |
| Terminal-file after-record selection and loop-independent exits | 20 |
| Search sentinel/close/result, error capture and mask or sentinel storage | 70 |
| Up to three diagnostic calls at 134 each | 402 |
| Uniform upper | 801 |

The two worker iterations allow 44 actions each for conditions,
lookups and traversal events before the explicitly priced close
calls. The flag/membership row includes the integer-to-pointer
invalid-handle conversions in both possible `live` tests. The
direct search action may follow a failed heap release. The third
diagnostic allowance covers the exclusive invalid-freeze refusal.
The setup-failure branch and normal prefix cleanup are mutually
exclusive. A helper can also reach one after-record retirement.
Thus the local retirement subtotal is at most `2 × 801 = 1,602`.

Together these two local schedules reserve **30,330 actions per helper**.
The separate [close-one worksheet](2026-11-14-clock-r22-close-owner-census.md),
[terminal codec](2026-11-14-clock-r22-terminal-codec-scalar-census.md),
[scan worksheet](2026-11-14-clock-r22-scan-final-census.md) and
non-scan transport validation still contribute their own work.
This source count supplies no release or native-time evidence.
Independent review verified the exact I/O call sites, typed Win32
boundaries, failure overlap, short-write loop and arithmetic. It
accepted this local upper under the separate diagnostic rows.
