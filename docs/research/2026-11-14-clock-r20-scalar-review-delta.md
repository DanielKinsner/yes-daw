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

## Failure boundary replacements

| Region | Author upper | Included work |
| --- | ---: | --- |
| `failure` call and construction | 25 | Four evaluated arguments, four formal initializations, dispatch and entry, four value reads for the fields, two enum casts, the native-error condition and one selected operand, four field writes, and return. Includes the direct aggregate formation boundary. |
| One implicit `Failure` assignment | 16 | Evaluate and bind source and target and enter the member operation. Include four field reads, four writes and return by reference. |
| `fail` | 132 | Twelve for the outer boundary including default DWORD conversion, 25 for `failure`, nine for binding/testing the terminal, maximum of 25 primary or 76 secondary, and ten for stopping and exit. The secondary arm includes two possible assignments, count, first-error tests and flag updates. |
| `path_failure` | 164 | Twelve to evaluate/bind its arguments and enter, nine for the StageAdmission comparison, branch and flag update, 132 for `fail`, two to return false, and nine for a native error leaf and destination. |
| `begin_tail` | 218 | The existing 82 non-diagnostic actions, plus four for the two native lock boundaries and up to one 132-action diagnostic. |

These rows include default values when the corresponding source
argument is omitted. The two diagnostic conditions in `begin_tail` are
mutually exclusive. The revised regular spend upper is 100 in prefix
and 43 in tail, including the existing 12-action spend boundary.
One scan can enter `begin_tail` from a refused spend at most once.
The 555-spend component is 55,718 for a full prefix reservation and
23,865 for a tail-start reservation.

## Revised scan composition

The corrected R19 aggregate rows already include the SDK zero conversions.
Replace the two local diagnostic allowances in that draft to get 5,928
local actions. Eight joins and eight validations give 416,096 after
replacing the copy and each validation's diagnostic. Together with
13,036 name actions the non-meter scan upper is 435,060. Adding the
spend estimates gives 490,778 for the prefix envelope and 458,925 for a
tail-start scan.

Using the original 1,048,837 setup/body scan bounds, two possible
overlapping after-cut upper reservations and 309 terminal-publish scans,
the resulting reservation component is 514,888,914,567 per helper,
or 1,544,666,743,701 across three helpers. This is a scan-subtree
author proposal. Independent scalar review, all other work families
and final catalogue values remain pending.
