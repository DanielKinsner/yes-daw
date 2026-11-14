# Clock R22 path meter work outside stage scans

The [path meter worksheet](2026-11-14-clock-r22-path-meter-census.md)
counts 2,060 joins and nine validations outside scans. Each join can
reach two `spend` calls. Each validation can reach 65 `PathGuard` calls
and one `PathCell` call. The total is 4,714 calls.

The terminal transport validation is the only one of those nine in
the publication suffix. It can reach 66 calls after `begin_tail`.
All other non-scan path calls occur before the workers start. Use the
prefix upper since a refusing call propagates out of its enclosing
preparation path.

| Region | Calculation | Actions per helper |
| --- | --- | ---: |
| Before terminal publication | 4,648 × 103 | 478,744 |
| Terminal transport validation | 66 × 44 | 2,904 |
| Possible refusing spend | One cut transition | 220 |
| Standalone upper | Sum | 481,868 |

The 103, 44 and 220 operands come from the
[shared meter worksheet](2026-11-14-clock-r20-scalar-review-delta.md).
The transport validation starts after the cut. Its `charge` takes the
tail branch and cannot refuse. A prefix `spend` refusal exits the
path helper and the caller cannot return to a new prefix path.
The standalone cut allowance overlaps the corresponding reserve in
the request and identity meter join. A phase composition can charge
that transition once across both, with the outer cleanup call counted
separately. Independent review verified the 4,714-call total and the
absence of a second refusal at the tail-start transport check. This
worksheet covers meter actions only.
