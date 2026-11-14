# Clock R19 stage-scan meter call counts

The [scan occurrence bound](2026-11-14-clock-r18-stage-scan-census.md)
and [path expansion](2026-11-14-clock-r18-scan-path-census.md) imply these
explicit meter occurrences for one scan. The source paths are
[`stage_scan` and `validate_path`](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/058-sup_io.cpp)
and [`join_path`](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/056-sup_codec.cpp).

| Debit family | Occurrences per scan | Derivation |
| --- | ---: | --- |
| StageScan | 1 | Entry before any scan work. |
| StageEntry | 10 | One per reached record, including the refusing record. |
| PathGuard | 528 | Eight joins each reach one. Eight validations each reach one outer guard and at most 64 ancestor guards. |
| PathCell | 16 | One per join and one per validation. The sum of reached count operands is at most 8,347. |
| Total | 555 | Each source call enters `spend(State&,Family,U64)` and then `charge` once. |

The 555 count is an upper for reached meter calls. A short circuit can
skip `PathCell` and an early refusal can skip most of the other sites.
The default count argument of one, the explicit variable operand,
each caller's binding/evaluation, and the work inside `charge` are separate
source actions.

A failed `charge` in the scan calls `begin_tail` and returns false.
After the lock makes a cut-visible charge tail-capable, the later charge
calls can return true while still consuming their source actions. Count
these calls in the prepaid tail, even though their ledger debit is zero.
At most the first refusing spend in a scan makes this extra `begin_tail`
call because every spend failure ends that scan. `begin_tail` can also
be reached elsewhere in the helper and those caller sites need their
own counts. Native lock internals retain their existing qualification.
