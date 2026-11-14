# Clock R22 SHA update and padding scalar bounds

These rows cover `sha_init`, `sha_update` and `sha_finish` in the
[pinned core](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/057-sup_core.cpp).
Meter calls and diagnostics have separate rows. The
[compression worksheet](2026-11-14-clock-r22-sha-compress-census.md)
owns the complete `compress` call boundary and body.

| Region | Basis | Upper |
| --- | --- | ---: |
| Initialize SHA | 32 for caller, entry, first meter result and return, 64 for eight indexed initializers, 16 for pending count and byte count | 112 |
| Update call, fixed work | 32 for boundary and return, 12 for meter results and branches, 18 for overflow/end checks and diagnostics' outcome, 14 for updating the byte count and entering the loop | 76 |
| Update one byte | 10 for loop work, 20 for the two indexed operands, pending-count increment and copy, 10 for fullness selection, 4 for the compression result guard | 44 |
| Reset the pending count | Receiver, member, converted zero and store after a successful block, with the success event | 6 |
| Finish, fixed work | 32 for the boundary and return, 20 for meter/overflow checks, 36 for bit length, padding declaration, count and initial marker, 20 for the nested update result and pending guard, 20 for fixed parts of the three loops | 128 |
| Fill one padding byte with zero | 10 for the loop, 10 for the converted zero and indexed write | 20 |
| Fill one length byte | 10 for the loop, 22 for the bit offset, shift, cast and indexed store | 32 |
| Write four digest bytes | 10 for the loop, 94 for four loads, shifts, casts and indexed stores with typed offsets | 104 |

The padding count before its eight length bytes is at most 64.
It uses one initial marker, so the zero-fill loop reaches at most
63 iterations. Each other finish loop has eight iterations. The
finish upper is `128 + 63 × 20 + 8 × 32 + 8 × 104 = 2,476`.

Each helper can reach three initializations and three finishes.
The [identity and hash schedule](2026-11-14-clock-r20-identity-read-census.md)
allows 34,603,526 updates and 34,604,288 requested bytes. An update
whose byte debit is skipped because its chunk debit fails can still
be reached, but its call counts within the same one-update-per-file-
fragment or fixed run identity loop schedule. A byte-loop body needs
an accepted byte debit, and a pending-count reset needs a successful
compression. Thus at most 540,692 resets can contribute.

| Local work | Actions per helper |
| --- | ---: |
| Initialization | 336 |
| Updates and pending resets | 4,155,700,800 |
| Finish and padding | 7,428 |
| Total | 4,155,708,564 |

The table excludes callers above the listed boundaries, meter and
diagnostic callees, and the independently accounted compression.
Independent review accepted the local rows and reproduced the subtotal
against the exact core pin. It enumerated all 64 pending counts to
verify the zero-fill maximum. The nested `sha_update` and `compress`
argument evaluations and bindings belong to those callees' rows once.
