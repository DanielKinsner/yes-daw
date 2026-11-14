# Clock R22 request decoding scalar bounds

This worksheet covers `decode_request` and its reachable local conversion
and text helpers in the [exact R22 codec](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/056-sup_codec.cpp).
The native codec SHA-256 is
`AC5AD56D987626CF33AD6BC5FCDEA58EE9D3086F3F1B0119293C3CD6085BBEE3`.
The header fixes scalar widths, request capture caps and storage.
The [research scalar convention](artifacts/2026-11-06-native-object-layout-purpose-source-r1/sources/001-contract.md)
applies to all rows, including typed work, reached control, caller
evaluation, bindings and returns.

The decoder's direct lock acquire/release boundaries are included. Their
native internals remain primitive. Meter work belongs to the separate
`spend` worksheets. This includes the two request decoder sites and the
two sites in each reached `copy_text`. Local rows include the caller's
variable count evaluation and result branches, with the formal count
conversion in the meter boundary. Other callers of `cell` and
`valid_wide` use their own schedules.

## Wire and UTF-16 leaves

| Leaf | Actions per call | Included work |
| --- | ---: | --- |
| `get32` | 48 | Up to 19 for pointer argument and call boundary, including computed capture plus index. Four loads use up to five each for pointer, index, address, read and cast. Three shifts use up to two each, and three ORs use one each. |
| `get64` | 120 | Up to 24 for outer pointer argument and call boundary, two U64 casts, a typed shift and OR. Two complete `get32` calls at 48 each include their pointer expressions `p` and `p+4`. |
| `cell` | 56 | Up to 32 for arguments, binding and return, including implicit by-value Text formation with two scalar fields. The two byte expressions take up to 24 for nested pointer field reads, scaled offsets and indexed loads, casts, shift and OR. |
| `high` or `low` | 24 | Up to 12 for arguments and call boundary, including the longer adjacent-unit indexed load and explicit U32 cast. Two four-action typed range comparisons, short circuit and returned value fit within the other 12. |

## String copy and validity

Each reached `copy_text` with length `L` has a complete bound of
`148 + 216L`, with its own meter calls counted separately. A failed
meter or length test can skip both complete loops. A validity failure
can skip the suffix of the validity loop after the first rejected unit.

| Region | Fixed | Per unit | Included work |
| --- | ---: | ---: | --- |
| `copy_text` local | 112 | 24 | Fixed split is 40 for caller, by-value Text and formals, 24 for both meter outcomes and extent guard, 16 for loop entry and exit, 24 for output length and converted NUL stores, and 8 for return/retirement. Unit allowance includes loop control, destination address and cast/store of the returned cell. |
| Nested `cell` | 0 | 56 | One call with its argument evaluation per copy iteration. |
| `valid_wide` local | 36 | 64 | Fixed includes pointer decay, length and ASCII/path formals, entry, loop setup and final test, return. Unit split is 10 for loop control, 8 for read/cast/local write, 28 for ASCII/path guards including literal conversions, and 18 for surrogate-result branches, neighbor bounds and refusal/return. |
| Nested `high` or `low` | 0 | 72 | At most three calls per validity iteration at 24 each. The first test calls `high`. The high arm may try a following `low`. The other arm may test `low` then try a preceding `high`. The short circuit blocks the neighbor call at a boundary. |
| Total | 148 | 216 | Includes both loop envelopes and callee boundaries once. |

## Decoder control and copies

Let `N` be the accepted string count for the helper slot. A refusal on
child index or argument count stops before both later loops. The compiler
slot has `N=40`, and the producer and verifier slots each have `N=18`.
Before the copies are reached, each string length is capped at 512,
prechecked against the remaining bytes, and the final cursor comparison
requires exact exhaustion. With the 140-byte header, each slot's capture
cap is exactly `140 + 4N + 2(512N)`. The sum of all reached string
lengths is therefore at most `512N`, funding one copy pass and one
validity pass over those units. This also bounds an early copy
refusal. The larger pre-index capture admission does not increase
either later string-loop length.

| Local decoder region | Actions | Included work |
| --- | ---: | --- |
| Entry and first two spends' caller results | 36 | Invocation and entry, direct captured count read, logical/branch/early-return work and binding the request reference. |
| Minimum capture check and magic loop setup and exit | 24 | Typed capture check, index initialization and final condition. The constant magic table needs no per-invocation initialization. |
| Eight magic iterations | `8 × 32` | Loop control and byte comparison with both index/address/read paths, byte promotions and refusal branch. |
| Version, capture extent, header fields and slot checks | 120 | The direct wire leaves supply the reads. This row includes destination writes, returned values' typed comparisons, child-index and argument-count guards, selected cap, and false-prefix outcomes. |
| Reservation fields and budget guard | 88 | Two assignment destinations, two catalogue index/address/read paths, typed tail-zero test, total/tail relation, checked branch and the subtraction/limit binding reached on the successful path. The wire conversions remain separate. |
| Limit installation | 64 | Two direct Win32 primitive boundaries with state/meter/lock/address and argument binding. Includes validity and used-limit test, conditional limit store, refusal after release. |
| Digest-byte loop | `12 + 32 × 64` | Setup and final condition. Per iteration up to 45 for three source offset/index/address/byte reads and three nested destination member/index/address/stores, with up to 19 for loop work and retirement. |
| Form text views | `16 + 104N` | Setup and final condition. Per entry up to 12 for loop/count control, 17 for pre-read bounds/refusal, 6 for read-result binding and first cursor update, 17 for length/remaining guard, 28 for Text aggregate formation, implicit two-field assignment and retirement, 6 for second cursor update, and 18 for remaining typed receiver and branch work. `get32` owns the argument for its own read. |
| Start copy schedule and process outcomes | `72 + 24(N - 4)` | Final cursor check plus four short-circuit copy outcomes, the local Wide default length and lifetime, loop setup/exit, return of the decoder. Each additional copy visit admits its full loop control and outcome/refusal. |
| Publish decoded identity fields to terminal state | 1,432 | Up to 16 for child-index transfer and its nested receivers. Each of three implicit Digest assignments has up to 24 for reference formation, assignment dispatch/return and up to 14 per byte for both nested array accesses, element load/store and selection. The 32 bytes are counted once per assignment. |
| Local coefficient form | `4,168 + 104N + 24(N - 4)` | Contains the local groups above. |

Five direct `get32` calls read the fixed header, and each text view can
reach one additional call. The two `get64` calls include their four
inner `get32` calls. The decoder has at most `N` string copies
including the four explicit ones. At most one decoder attempt per
helper, the complete source upper is the local coefficient row, plus
`48(5 + N) + 2 × 120` for wire conversion, plus
`148N + 216(512N)` for string conversion and validity.

| Slot | Local decoder | Wire conversion | Copy and validity | Upper |
| --- | ---: | ---: | ---: | ---: |
| Compiler | 9,192 | 2,400 | 4,429,600 | 4,441,192 |
| Producer or verifier, each | 6,376 | 1,344 | 1,993,320 | 2,001,040 |
| All three slots | 21,944 | 5,088 | 8,416,240 | 8,443,272 |

This is a local source-action bound across normal and refusal paths.
Request file I/O, meter charge/refusal, recipe validation and later
formatters remain in their corresponding worksheets.
