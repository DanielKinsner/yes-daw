# Clock R22 request and hash file I/O scalar bounds

These rows cover `read_request` and `hash_file` in the
[R20 I/O revision](artifacts/2026-11-14-clock-r20-path-copy/review.md)
inherited by R22. The [request and hash occurrence census](2026-11-14-clock-r20-identity-read-census.md)
supplies at most 41,260 request iterations and 34,603,008 hash
iterations across both file caps. Native primitive internals remain
outside this source action count. Their caller boundaries and out-results
are included. Meter bodies, path validation, all SHA calls and the
final `close_one` call are separate.

| Request region | Scalar upper |
| --- | ---: |
| Call boundary and return, both live-handle tests, and path-validation outcome | 46 |
| File open and owner store, size formation and native result, extent guard and bound length | 65 |
| Initialize progress, test final loop condition, probe EOF and store capture length | 35 |
| Additional fixed actions for typed boundaries and scope retirement | 14 |
| One reached loop body including its continuing test | 64 |
| Each diagnostic route, including the possible native error retrieval and argument conversion | 192 |
| Total per helper at up to three diagnostic routes | 2,641,376 |

The native request body includes context and current-length evaluation,
prefix address derivation, API call and out-result, result branches,
read extent guard and progress update. A failed read takes the
place of a continuing iteration. The 192-action route allowance
includes the 134-action `fail` upper and the actual error or default
leaf. Only one local request diagnostic can execute because every
route returns. The total reserves three, allowing independently for
the setup, read loop and EOF suffix.

The size formation includes both zero-initialized `DWORD LowPart` and
`LONG HighPart` in the pinned SDK's first `LARGE_INTEGER` variant.
Native size output is counted separately. The EOF probe includes its
one-byte scalar destination and the count result.
[C++20 aggregate initialization](https://timsong-cpp.github.io/cppwp/n4861/dcl.init.aggr#5)

| Hash file region | Scalar upper |
| --- | ---: |
| Call boundary and retire, path and owner checks, file open, size, local setup, SHA call outcomes, EOF probe, length store and close outcome | 256 per file |
| One reached read-loop body including its continuing test | 96 |
| Each diagnostic route, including the possible native error retrieval and argument conversion | 192 |
| Total across both files at up to three routes each | 3,321,890,432 |

The hash file loop tests `ok` and the converted size, selects the
bounded request, handles metering and native results, validates the
reported byte count and adds it to the file progress. The loop includes
the literal-to-U64 request comparison, the DWORD result selection, and
the reported count conversions to U32 for SHA and U64 for progress.
The fixed region includes the same two `LARGE_INTEGER` initialized
members and the `Sha256` default counter initialization and retirement.
Its two arrays have no source initialization. The host file buffer
already belongs to `State`. SHA charges its own byte loop.

Independent review accepted both corrected local bounds after
verifying the R22 source pins, typed Win32 boundaries, loop extents,
diagnostic routes and arithmetic. The separate callees and preparation,
recipe-validation and cleanup callers still need composition.
