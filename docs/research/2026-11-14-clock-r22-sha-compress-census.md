# Clock R22 SHA compression scalar bounds

The exact R22 `sup_core.cpp` matches the
[archived native source](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/057-sup_core.cpp).
This worksheet bounds local work in `compress` and the `rotate` helper.
The two `spend` bodies and their possible diagnostic calls have separate
rows. The full block count comes from the
[reviewed identity and hash bounds](2026-11-14-clock-r20-identity-read-census.md).

| Fixed work per invocation | Upper |
| --- | ---: |
| Caller arguments and bindings, dispatch, entry and return | 24 |
| Evaluate both meter results, including short circuit, early branch and return | 16 |
| Enter and retire schedule array, initialize three loop indices and test their final conditions | 32 |
| Read eight SHA accumulators into state words, including receiver, index and local writes | 56 |
| Add back eight accumulators, including receiver, index, prior reads and stores | 72 |
| Fixed upper | 200 |

The schedule array begins uninitialized. Every entry is written before
its use. A reached iteration's upper includes the continuing test,
body branch, induction and iteration event.

| Iteration | Loop work | Body actions included in the upper | Upper per iteration |
| --- | ---: | --- | ---: |
| Load 16 words | 10 | 6 for index scale and binding, 32 for four loads/casts with converted offsets, 10 for shifts and ORs, 6 for the indexed store | 64 |
| Expand 48 words | 10 | 16 for indexed reads and bindings of `x` and `y`, 72 for four rotations, 20 for their surrounding shifts/XORs and stores, 26 for the final indexed operands, adds and store | 144 |
| Run 64 rounds | 10 | 108 for six rotations, 13 for the rotation-result XORs and two bindings, 20 for choose and majority, 23 for `one` and `two`, 24 for eight state-word updates, and 14 for remaining literal conversions and expression or destination work | 212 |

The round residual includes one additional choose or majority action.
The separate spend boundary owns conversion of the default unit count
for the block spend and the explicit 64 count for the round spend.

One rotation can reach both argument evaluations and parameter writes,
conversion of its count argument, dispatch, entry, five operand reads
including the unsigned constant, subtraction, two shifts, OR and
return. Allow 18 per call, including retirement.

The loop uppers are 64, 144 and 212. They give
`200 + 16 × 64 + 48 × 144 + 64 × 212 = 21,704` per block excluding
the meter bodies. With 540,692 possible reached blocks, the local
subtotal is 11,735,179,168 per helper. A refusal can skip the schedule,
rounds and updates. The bound uses the full envelope for that block.

Independent review matched the exact R22 core pin, accepted the local
compression and rotation rows and reproduced both subtotals. The first
arithmetic check failed on a regex capture error. The corrected check
passed against the same source and worksheet. Update, finish, I/O,
meter and phase composition still need their own scalar bounds.
