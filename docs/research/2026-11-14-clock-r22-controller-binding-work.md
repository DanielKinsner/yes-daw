# Clock R22 controller binding and meter work

These rows cover three added PowerShell bodies in the exact
[R22 controller](artifacts/2026-11-14-clock-r22-controller-delta/README.md).
They count their local actions under the
[reviewed source convention](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/182-argument.md).
Calls to the existing hash, path, string, type and retained limits
retain their own bodies. Native and managed primitive arguments,
actual conversions, formals and result stores are included.

## Supervisor spend

| Group | Actions |
| --- | ---: |
| Entry, formal destinations, scope and return | 16 |
| Negative count and nested catalogue lookup, Contains and failed guard | 46 |
| Price getter, diagnostic field formation and signed-integer guard caller | 38 |
| Reaching positive count, typed zero/remainder, signed DivRem call/out and product-cap test | 60 |
| Evaluate typed product, call Spend-Task and handle refusal | 27 |
| Remaining control and error selector | 13 |
| Per invocation | 200 |

The count is a typed Int64 formal. Its callers own omitted default
value formation and the explicit request/terminal integer conversions.
The lookup can fail before
selecting price. The cast to Int64 and product formation are counted
where written. The supplied remainder is a separate actual out
destination. One signed-integer body and one Spend-Task body can be
reached. A parent loop is bounded by the eventual selected positive
parent poll limit, not the native target's 7,200-poll policy.

## Supervisor environment

| Group | Actions |
| --- | ---: |
| Entry, exact-key guard caller and its fixed eight-key argument | 64 |
| Form value and key arrays and initialize/end the index loop | 50 |
| One of eight visits | 58 |
| Singleton array return and scope | 18 |
| Per invocation | 596 |

Each visit indexes the source key array and environment map, tests
the opaque-temp marker, may select the owned temp path, invokes two
string checks and stores one reference. The 58-action unit allows
ten for loop/control, 14 for typed source selection, 14 for the
two validation call sites, 12 for the marker and replacement branch,
and eight for storing the selected value. There can be one call per
child. Its exact-key body and up to sixteen string bodies are separate.

## Supervisor binding

| Group | Actions |
| --- | ---: |
| Initial map and four binding predicates | 52 |
| Fixed image name, protocol version and request/terminal/transport checks | 98 |
| Two integer call sites and file size relationships | 86 |
| Digest string/regex checks | 68 |
| Both reservation array extent guards | 30 |
| Three reservation pairs, 62 each | 186 |
| Two parent budget visits, 31 each | 62 |
| Signed Int32 representation check for poll count | 26 |
| Binary path and no-link, hash, verified identity and console PE call sites | 82 |
| Outer caller/result group and scope | 16 |
| Per invocation | 706 |

Each reservation visit owns typed index and loop events, the
two signed-integer guard argument expressions and dispatches,
and the tail-versus-total comparison and branch. The parent group
similarly covers path-dependent map indexing and exact types.
The PE-check statement contains two call sites. The outer argument
is the inner selected PE receipt. The binding row owns all five
binary operations' source call expressions, while their bodies
remain separate. One entered binding can reach two integral checks,
one digest string check, eight bound-integer checks, the no-link
check, the 32 MiB-capped hash, the fresh identity check, the PE reader
and the console check.

The parent checks binding before and after each reached helper,
and once in each input fence. Eight input fences cover the initial and
final checks and both fences for each of three children. The before
and after per-helper checks give six more, or at most 14 binding visits.
The extra validation on a failure prefix does not retry a launch.

The three slots share the same eventual parent-poll cap `P`. The
three helper paths can reach up to `9 + 3P` supervisor
spend calls, including the invoke, request and terminal debit in each
slot. The other two bodies contribute `14 × 706 + 3 × 596`.
The local subtotal is therefore **13,472 + 600P**. The shared cap and
inherited callee/caller components require the full parent schedule.
Independent review accepted the local typed work and call schedule.
