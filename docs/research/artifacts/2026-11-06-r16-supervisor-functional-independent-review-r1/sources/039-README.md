# Exact R16 review delivery in bounded JSON files

This NEW folder delivers all42 files of the frozen R16 review, including its three
raw input ZIPs, failed attempts, own checker and original first-green freeze. The
original folder is unchanged. Original total540637273 bytes; original whole-freeze
SHA256 E41AEAD77D8AA7D1BBED1AE6B1F0AD7FC97B7917051AE683B61A960517CAFDB8.

`delivery.json` selects the package and exact archive-index. The index lists each
original relative path, byte length and SHA256, plus gapless ordered raw extents.
Each extent records its offset/length/SHA, kind, optional ZIP member index and
ordered blob IDs. ZIP records are structurally split into local header and
already-compressed payload, followed by exact central-directory/trailer spans.
Large raw extents split into at-most8MiB blocks. Equal raw blocks share a SHA256
content address across all three ZIPs and the other files. Every stored blob is
zlib compressed then Base64 encoded; every JSON blob chunk is strictly<16MiB.
ZIP payloads are never inflated, extracted, interpreted as programs or executed.

To reconstruct a file, visit its extents in order, retrieve each listed blob from
the indexed JSON chunks, Base64-decode and zlib-decode that OWN envelope, then
concatenate the raw bytes. Validate blob, extent, local-record and whole-file
lengths/hashes. ZIP bytes remain raw throughout. Reconstruct directly into a
stream/hash/comparison for verification, since original ZIPs exceed100MB and
must not be introduced as Git files. No extracted native/program input is needed.

`metadata-input-identities.json` preserves all1381 original current input identity
rows, with their original source-pins hash. Their raw bytes are already inside
the preserved ZIP captures; this package does not archive those originals again.

`write_delivery.py` is a fresh source/file/JSON/byte writer. `check_delivery.py` is
a separate fresh verifier: it decodes only this package's OWN envelopes, compares
every reconstructed byte directly with each immutable original and validates all
whole/local/extent/blob hashes and metadata identities. Neither imports any
candidate/old checker or extracts/executes ZIP members. Full per-invocation input
indexes, source snapshots, partial output and result files are retained. A failed
attempt is not overwritten. The first complete green seals every file in this
new folder in `first-green-delivery-freeze.json`, which itself is separate.

`verification.json` reports the exact original/reconstructed/package sizes and
assertions. Root independently verifies staged reconstruction before any commit.
This is lossless delivery evidence only: no source price, native/ABI/resource fit,
binding, Data, limit, compiler/API-purpose/helper/fixture/Clock or execution credit.
