# Clock R22 request and identity object storage

The [supervisor ABI receipt](artifacts/2026-11-14-clock-supervisor-abi/result.json)
measures the `State`, `Wide`, `Digest`, `Sha256` and Win32 object sizes
used below. This worksheet sums explicit local arrays and aggregates
in the exact R22 request reading and identity scope. The relevant
declarations are in [the recipe](../../build-ci/20261106-clock-owned-supervisor-source-r4/native/sup_codec.cpp),
[the file reader](../../build-ci/20261114-clock-r20-native/sup_io.cpp)
and [the SHA helpers](../../build-ci/20261106-clock-owned-supervisor-source-r4/native/sup_core.cpp).

| In-flight operation | Live local arrays and aggregates outside `State` | Bytes |
| --- | --- | ---: |
| Request path validation | Two 513-cell `wchar_t` arrays and one `Wide` | 3,084 |
| Request read | One `LARGE_INTEGER` | 8 |
| Recipe validates an owned path before opening or hashing a file | Recipe owners and 3,084 bytes from nested path validation | 5,262 |
| Run identity hashes a path unit | Recipe owners, `Sha256`, four-byte length, two-byte encoding and 64 `U32` compression words | 2,552 |
| Run identity finishes its hash | Recipe owners, `Sha256`, four-byte length, 128 padding bytes and 64 compression words | 2,678 |
| Identity file finishes its hash | Recipe owners, `LARGE_INTEGER`, `Sha256`, 128 padding bytes and 64 compression words | 2,682 |

The shared recipe owners at these sites are two `Wide` objects, one
`Digest` and the two local name arrays. The arrays have 20 and 21
`wchar_t` cells including NULs. Together the owners account for
2,178 bytes. The 65,536-byte file hash buffer and the request capture
already live inside `State`. The largest shown phase plus the
191,064-byte `State` gives 196,326 bytes.

`hash_file` validates the path before declaring its `LARGE_INTEGER`
and `Sha256`. `sha_finish` keeps padding live across the nested
`sha_update` and compression. The two-byte encoding's iteration
ends before the finish in run identity. The file loop's one-byte EOF
probe scope likewise ends before its call to `sha_finish`.

The sum covers named arrays and aggregates. Scalar formals and locals,
error-route objects, alignment gaps, underlying native objects, full
stack reservation and controller/target overlap remain open.
