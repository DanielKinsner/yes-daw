# Clock R19 scan charge scalar census

This is an author upper for the `charge` call from `spend` in the
[native meter source](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/057-sup_core.cpp).
The [scalar convention](artifacts/2026-11-06-native-object-layout-purpose-source-r1/sources/001-contract.md)
counts caller evaluation, parameter initialization, branch selection and
source operations. Native lock internals retain their existing boundary.

| Source region | Upper actions | Included source |
| --- | ---: | --- |
| From `spend` to `charge` entry | 9 | State receiver, member selection and reference binding, two scalar reads, dispatch, three formal bindings, and entry. |
| Acquire and initialize | 7 | Lock receiver, member selection, address, dispatch, primitive return, initial `true` and local write. |
| Choose prefix or tail | 4 | Meter read and member access, negation and branch. |
| Family index and enum conversion | 3 | Scalar read, explicit conversion, destination. |
| Meter valid and index extent | 9 | Meter field evaluation and negation, comparison of index against enum count with explicit conversion, short circuit and branch. |
| Read the price | 7 | Catalogue address, array member, index, element selection/read, scalar conversion allowance and destination. |
| Price validity and product overflow | 13 | Both zero and nonzero checks, short-circuit operators, division operands and result comparison, branch. |
| Form the cost | 4 | Two reads, multiply, initialize. |
| Compare with remaining work | 15 | Meter receivers and field reads, cost read, remaining subtraction, both comparisons, short circuit and branch. |
| Update outcome | 6 | Covers updated `used` or two scalar assignments for refusal. Invalid metadata follows an earlier branch. |
| Release and return | 7 | Lock receiver, member selection, address, dispatch, primitive return, output read and return. |

The row sum is 84 actions for a prefix charge, including the refusing
prefix. A tail charge visits only the call boundary, lock acquire and
initialization, prefix/tail test, and release and return, at most 27 actions.

From the [scan meter occurrences](2026-11-14-clock-r19-stage-meter-census.md),
555 charges in one scan contribute at most 46,620 actions if all use the
prefix upper or 14,985 if all enter the tail. A scan crossing the cut
uses the relevant bound for each charge. The calling scan's invocation
of `spend`, the branch on the charge result, and `begin_tail` including
its diagnostic are outside this subtotal.
