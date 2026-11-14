# Clock R19 directory name scalar census

This prices a small source-only portion of the scan, from the exact
[`same_name` and `allowed_stage_name` bodies](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/058-sup_io.cpp).
The upper includes one entry, parameter initialization and return,
using the existing [scalar convention](artifacts/2026-11-06-native-object-layout-purpose-source-r1/sources/001-contract.md).

## One name test

The caller pays the candidate address, literal name reference, length literal,
and dispatch, four actions. The callee has three parameter initializations,
one entry and two actions to initialize the index. Each full loop iteration
has these actions:

| Expression | Upper |
| --- | ---: |
| Index and length reads, comparison, loop selection | 4 |
| Two indexed loads, each base read, index read, address selection and element read | 8 |
| Comparison and branch selection | 2 |
| Index read, increment and write | 3 |
| Iteration entry | 1 |

Allow 18 actions per unit, four for the final condition and selection,
and seven for the last indexed load, zero literal, comparison and return.
The early false path skips the increment and final NUL work. An upper
including the call boundary is `21 + 18L` for a name of length `L`.
The caller evaluates the candidate array address before the first dot
test and again before the second when reached. Count two extra actions
for the enclosing entry receiver and array member per pseudo test.

## Use within the scan

The two pseudo tests contribute at most
`(21 + 18 + 2) + (21 + 36 + 2) = 100` per body.
The allowlist's six lengths total 78. All six calls plus the five
short-circuit selections cost at most
`6 × 21 + 18 × 78 + 5 × 2 = 1,540`.
Its own parameter initialization, entry and return, the enclosing
candidate array address evaluation and dispatch add at most six.
Ten pseudo-check bodies and seven allowlist calls contribute an upper of
`10 × 100 + 7 × 1,546 = 11,822` scalar actions per scan. This leaves
the scan's surrounding branches, loop bookkeeping, path helpers,
initialization, diagnostics and meter in their own rows.

This is an author upper for the named source slice. The full scan price,
cross-language composition and accepted work limit remain pending.
