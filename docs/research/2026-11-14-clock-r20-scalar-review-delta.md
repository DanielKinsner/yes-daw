# Clock R20 scalar review corrections

The [R19 review](2026-11-14-clock-r19-stage-review.md) identified a path
source defect and several undercounted scalar actions. The R20 candidate
changes the copy source. The rows below are replacement inputs for
repricing and independent review.

| Region | Corrected accounting |
| --- | --- |
| `same_name` full loop | Add two promotions of the loaded `wchar_t` operands per unit. The terminal NUL comparison adds one promotion. The previous 21 + 18L becomes 22 + 20L. |
| Both pseudo checks | The two lengths sum to three. With the original two enclosing receiver actions per call, the envelope becomes 108 per reached body. |
| Six allowed-name checks | The lengths sum to 78. The previous short-circuit and enclosing caller work still applies, yielding 1,708 per allowlist call. |
| All directory name checks | 10 × 108 + 7 × 1,708 = 13,036, up by 1,214. |
| Charge lock boundaries | Include entry and the pointer formal at acquire and release. This adds four to each charge upper, giving 88 for prefix and 31 for tail. Reprice explicit spend and cut callers separately. |
| Validated prefix copy | The extent and terminator guard admits at most 513 cells. Allow 21 actions per copied cell and 25 for the destination declaration and length initialization, loop setup and final condition, and depth/first default values. The replacement row is 10,798, including the loop term of 10,773. |

Each copy iteration has up to five condition and branch actions, eleven
receiver/index/address/load/store actions, three induction-update actions
and two loop events. The [R20 source](artifacts/2026-11-14-clock-r20-path-copy/review.md)
has one reached copy per validation and all later reads stay within the
initialized prefix.

The SDK aggregate conversion correction is already reflected in the R19
draft. The remaining failure-construction, implicit assignment, and
diagnostic boundary expansions still need explicit new rows. No total or
catalogue values follow from this delta.
