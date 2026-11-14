# Clock R19 path helper scalar census

The rows below bound the scalar source in
[`validate_path`](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/058-sup_io.cpp)
and [`join_path`](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/056-sup_codec.cpp).
They include entry/return and visible native call argument work. The
[spend census](2026-11-14-clock-r19-spend-scalar-census.md) supplies the
meter calls separately. Variable count expressions, branch selections on
their results, and native failure routing are included here.

## One join

| Region | Author upper |
| --- | ---: |
| Caller boundary with four arguments, callee entry and body return | 14 |
| Two spend-result guards and base-plus-suffix count | 11 |
| Test whether a separator is needed, compute extra and test extent | 50 |
| Copy the base and suffix, including index updates and loop entries, at most 512 × 24 | 12,288 |
| Initialize both loop indices and output index, terminal conditions, optional separator and final NUL/length | 39 |
| Total | 12,402 |

The early refusal paths skip copy work. Both copy loops share the accepted
512-unit extent. The 24-action loop row includes condition, two base and
index evaluations, two address computations, source read and destination
write, both index increments and the loop/control events.

## One validation

| Region | Author upper |
| --- | ---: |
| Caller boundary, parameters, entry and exit | 16 |
| Two initial meter outcomes, count expression, reason selection and path checks | 45 |
| Two uninitialized path arrays and their scope retirement | 4 |
| Full-path and volume API calls, result guards and folded comparison with its parameters and visible native call | 81 |
| Volume length loop, at most 513 × 16 plus setup and final checks | 8,227 |
| Initialize the copied `Wide` path, 513 array cells and length, plus the depth and first flags | 1,040 |
| Ancestor tests, meter-result branch, attributes call, type guards, next component update and loop events, at most 65 × 85 | 5,525 |
| Separator trimming and component discovery, at most 512 × 25 plus two terminating checks per reached ancestor | 14,750 |
| One diagnostic route including default or actual native error and `fail` | 136 |
| Total | 29,824 |

The clone row counts each array element read and initialization, including
cells beyond the NUL, plus the length read and initialization, two flag/depth
initializations and destination lifetime. The ancestor and component rows use
the [proved loop geometry](2026-11-14-clock-r18-scan-path-census.md).
Each spin pays visible condition and branch work. The ancestor row includes
one possible final depth failure before the attribute call. The trim and
discovery row includes up to 65 pairs of terminating checks at 15 actions
per check. The diagnostic row includes one possible `GetLastError`, the
guard's flag update, one `fail` and return. A refusal ends the validation.

Eight joins and eight validations contribute at most 337,808 scalar
actions per scan before the scan's own local work and meter. These
author rows still require independent scalar review before binding any price.
