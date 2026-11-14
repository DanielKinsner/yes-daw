# Clock R22 scan loop scalar corrections

Independent review of the [revised scan subtotal](2026-11-14-clock-r20-scalar-review-delta.md)
reproduced the arithmetic and accepted the scan occurrence and phase
schedule for the subtree. It found undercounted typed comparisons and
control actions in the inherited loop rows. These replacement rows use
the same [loop occurrence bounds](2026-11-14-clock-r18-scan-path-census.md).
The R22 candidate inherits R20's path code.

## Path shrinking

Both predicates can evaluate two indexed separator comparisons.
Each includes evaluation of the base and length or cut, index
subtraction with conversion, subscript address and cell read, both
`wchar_t` promotions, literal evaluation and comparison. The
continuing upper covers both comparisons and the larger loop body.

| Work | Actions |
| --- | ---: |
| Both indexed comparisons, up to 14 each including decay and member access | 28 |
| Outer length comparison, up to four with member access | 4 |
| Two logical operators with their short-circuit decisions, and loop branch and event | 6 |
| Larger continuing body, trim predecrement plus indexed zero store | 12 |
| Continuing upper | 50 |
| Terminating upper, no decrement or store | 38 |

The trim body can evaluate the receiver for the indexed store and
again for the length decrement. Its twelve actions cover the base,
member and decay, the decrement's receiver/member/access, read,
arithmetic and write, plus the store address, converted zero
and destination. Discovery needs only the local cut read, decrement
and write. The original 14,750-action subtotal becomes
`512 × 50 + 130 × 38 = 30,540`.

## Name and volume scans

| Work | Volume-root loop | Stage filename loop |
| --- | ---: | ---: |
| Index and limit comparison, including conversion of `MAX_PATH` | 3 | 4 |
| Short-circuit test and branch | 2 | 2 |
| Indexed cell comparison including decay, promotion and zero | 8 | 9 |
| Induction read, increment and write | 3 | 3 |
| Loop entry and exit allowance | 2 | 2 |
| Continuing envelope | 18 | 20 |

Use 20 for both loops, and add up to 32 per loop for the final condition,
including its failed-comparison and exit branches. This full additional
allowance avoids drawing on any of the previous shared setup budgets.
The volume loop adds `513 × 4 + 32 = 2,084` actions per validation.
The seven stage filename loops add `154 × 4 + 7 × 32 = 840` actions
per scan. The full-path call group adds ten per validation to cover any
remaining count conversions at the API and comparison boundaries,
including DWORD to U32 into `equal_wide` and both U32 to int casts
into `CompareStringOrdinal`.

## Replacement subtotal

One validation adds 17,884 actions across the two corrected loop
groups and the call group. Eight validations and the filename loops
add 143,912 per scan. Name checks, charge, spend and scan-call
schedules keep their previously reviewed rows.

| Reservation | Actions |
| --- | ---: |
| One prefix scan | 636,519 |
| One tail-start scan | 603,554 |
| Subtree per helper | 667,792,449,627 |
| Subtree across three helpers | 2,003,377,348,881 |

The per-helper line reserves 1,048,837 setup/body scans, two overlaps
remaining after the cut and 309 terminal scans. The critic checked
the exact R22 I/O source pin and all four loop anchors, and accepted
the replacement rows and composition arithmetic. The critic did not
expand every inherited non-loop row in this pass. Full subtree
acceptance, other family work, timing and resource fit remain open.
