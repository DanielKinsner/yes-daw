# Clock R20 request and identity read bounds

These bounds cover the source-native reads from
[`read_request` and `hash_file`](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/058-sup_io.cpp)
and the nested SHA calls. The [R20 source change](artifacts/2026-11-14-clock-r20-path-copy/review.md)
is limited to validated ancestor path copies.

The request reader has one native loop read per byte in the admitted
file length and one additional one-byte EOF probe. A continuing
iteration advances by at least one byte. A failed, zero-progress or
oversize read ends the loop in place of a possible final progress step.
The early reader admits the same 41,260-byte capture for all three
helper types before child-index-specific recipe validation. Use
41,261 as the per-helper source upper for every slot. The later
18,644-byte recipe cap does not bound an early rejected request.

The identity recipe calls `hash_file` at most twice. It uses the
1,048,576-byte manifest admission and the 33,554,432-byte target
admission. The compiler byte identity is checked after its file hash,
so the compiler's actual linked executable size does not reduce the
worst-case read prefix. Each hash can make at most one loop read per
admitted byte and one EOF probe. Together with request capture, this
gives 34,644,271 native reads per helper and 103,932,813 across three.

For a file of admissible length `N`, a complete SHA path has at most
`N + 1` calls into `sha_update`, including one padding call.
The hash reader's 65,536-byte buffer bounds each request size.
The read loop permits fragmentation as small as one byte.

Padding adds between 9 and 72 bytes. One complete SHA input therefore
reaches exactly `floor((N + 72) / 64)` compression blocks. Uncompleted
prefixes never reach more than this. The combined file caps yield
540,674 blocks. The separate run-identity recipe's domain including
NUL, manifest digest, four length bytes and 512 two-byte path units
add at most 18 blocks. The per-helper SHA compression upper is 540,692
and its 64 rounds per block yield at most 34,604,288 rounds.

## Request meter calls

The native read loop can request at most 41,260 unit `Decode` debits.
The EOF probe has no debit. A completed capture may reach one more
`Decode` and one `DecodeCell` in `decode_request`. The decoder requests
the complete captured size before it checks the child-specific length
cap. The same bound covers successful, refused and malformed request
prefixes in each helper.

| Family | Calls | Sum of requested amounts |
| --- | ---: | ---: |
| `Decode` | 41,261 | 41,261 |
| `DecodeCell` | 1 | 41,260 |

## Identity and hash meter calls

The R22 recipe reaches `validate_request_recipe` once, `hash_run_identity`
once and each of the two `hash_file` sites once. The prior
[literal inventory](artifacts/2026-11-14-clock-meter-inventory/result.json)
identifies the corresponding meter calls. These per-helper uppers include
the call that refuses a debit.

| Family | Calls | Sum of requested amounts |
| --- | ---: | ---: |
| `Identity` | 2 | 2 |
| `HashFile` | 2 | 2 |
| `HashChunk` | 69,206,540 | 69,206,540 |
| `HashByte` | 34,603,526 | 34,604,288 |
| `HashBlock` | 540,692 | 540,692 |
| `HashRound` | 540,692 | 34,604,288 |

The file byte caps sum to 34,603,008. Each possible read-loop iteration
can reach the `HashChunk` debit in `hash_file` and another in `sha_update`.
Each file adds one debit in `sha_init`, one in `sha_finish` and one in
the padding update. Run identity adds 518 calls from those same three
locations, three initial input updates and up to 512 path-unit updates.
Each update reaches at most one `HashByte` call. Together the files add
at most 34,603,010 of those calls, and run identity adds 516.

The maximum sum of requested bytes includes a possible refusing debit.
File updates collectively request up to their admitted bytes, then
possibly padding. For any input length `n`, the complete total with
padding is `64 * floor((n + 72) / 64)`, which is monotone. The same
upper therefore covers the shorter refusal paths and the three full
hashes together. Each compression block can request one `HashBlock`
and one `HashRound` debit, with the latter requesting 64 units at once.

The [source and arithmetic check](artifacts/2026-11-14-clock-hash-meter/check.py)
asserts the exact R22 inputs, site coverage, the recipe caps and these totals.

The native SHA source and meters still need complete scalar and phase
accounting. Filesystem progress, native time and storage are separate
bounds.
