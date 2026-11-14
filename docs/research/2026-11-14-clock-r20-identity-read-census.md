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
The native scan's 65,536-byte buffer bounds each request size.
The read loop permits fragmentation as small as one byte.

Padding adds between 9 and 72 bytes. One complete SHA input therefore
reaches exactly `floor((N + 72) / 64)` compression blocks. Uncompleted
prefixes never reach more than this. The combined file caps yield
540,674 blocks. The separate run-identity recipe's domain including
NUL, manifest digest, four length bytes and 512 two-byte path units
add at most 18 blocks. The per-helper SHA compression upper is 540,692
and its 64 rounds per block yield at most 34,604,288 rounds.

The native SHA source and meters still need complete scalar and phase
accounting. These are source-call occurrence bounds. They do not
assume filesystem progress or provide native time and storage bounds.
