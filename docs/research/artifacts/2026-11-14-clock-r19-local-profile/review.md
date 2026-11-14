# Independent Clock R19 source review

The independent critic accepted the whole functional source correspondence of
manifest SHA-256
`2B82636B3F3A51B27FC8D9FE229EDB190A889778110338143679F7870190EEAE`
after 8,463 source and data assertions. The checker is in the review
transcript. Its subject remained unrun.

- All 1,270 source input pins match their actual sizes and hashes. Of these,
  415 inherited rows use relocated file paths. Five new rows describe R18's
  materialized source files. All 35 closed directory inventories match.
- The runner's only differences are the runtime root and the derived 16,999
  bootstrap nodes. The controller's exact module paths and the seven early
  bootstrap pins follow the new root. The compiler's application path and
  the first compiler PATH segment point to the same recovered tool directory.
- R19 has 464,781,098 input bytes, 7,954 hash chunks, 1,276 current identities,
  and 1,351 prospective file positions. Bootstrap depth is four and the
  longest string remains 435 UTF-16 units.

The first generator attempt failed before writing any R19 files because a
local variable hid the verified path map. A distinct directory-membership
variable fixed the failure. The accepted subject has only the successful
generator's bytes.

Reproduce the result from the recovered directories with
`python -E -B candidate.py <controller-directory> <compiler-directory>`
after materializing R18. A different machine can produce different source
identities because the runtime path is part of the runner. The native
launcher profile, source work prices, resource fit, binary binding and
execution authority remain open.
