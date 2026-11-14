# Independent Clock R20 source review

Accept the functional source correspondence of manifest
`69A5E104B0FE69B898D14276A15CC1643AEE56658B779417D919B2769EDC4D56`.
The independent reviewer passed 7,730 source and data assertions. Its
checker is in the review transcript.

- All 512 allowed path lengths copy the initialized cells through the NUL.
  Ancestor traversal maintains that initialized-prefix invariant. The
  five-unit source audit found no other whole `Wide` or `Request` copies.
- The native diff replaces only the one implicit copy. Four older native
  identities and both target sources retain their original digests.
  If compiling the new I/O source separately, the unchanged native
  directory must be the explicit include location for its quoted `sup.h`.
- All 1,276 actual input files and 36 directory inventories passed.
  The 465,496,275 input bytes give 7,969 hash chunks. The only runner
  difference is the derived bootstrap bound of 17,069 nodes.

Run `python -E -B candidate.py` after materializing the exact local R19
source. The candidate is unrun. The scalar review corrections, whole
source census, remaining resource and native profile work, and executable
binding remain open.
