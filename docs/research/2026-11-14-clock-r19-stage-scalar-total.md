# Clock R19 stage-scan scalar composition

This is a source-only author proposal for the exact
[`stage_scan` body](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/058-sup_io.cpp).
Each entered scan contributes its source work once to the actual schedule.
The body envelope deliberately includes failure paths that may terminate
early. The rows distinguish local scalar work from nested calls.

| Local source region | Actions |
| --- | ---: |
| Entry arguments, formal initialization and final return | 10 |
| Meter result selection, occupied owner check, initial path and pattern call outcomes and pattern local initialization | 64 |
| `WIN32_FIND_DATAW` value initialization, 285 scalar zero values and writes, plus aggregate entry/retirement | 580 |
| FindFirst arguments and call, initial error and empty-directory arm or reached owner and loop-local initializations | 50 |
| Ten loop entries, meter outcomes, two pseudo-test result branches and dot update or ordinary dispatch | 300 |
| Seven ordinary record bodies, excluding the name and path callees | 4,032 |
| Nine FindNext steps, including one possible native error outcome | 144 |
| FindClose and owner/error handling, result selection and extent copy | 48 |
| Final extent checks, remaining-cap arithmetic and cap flag outcome | 24 |
| Up to two local diagnostic calls, at most 136 each | 272 |
| Local upper | 5,524 |

The ordinary row allows 22 successful length steps per name at 16 actions
each, plus 224 for terminal checks, index/child/attribute initialization,
call outcomes, bytes and metadata work. The metadata zeroing covers nine
scalar fields. The checked add includes its caller and condition, sum and
output write or early refusal. A loop failure and failed FindClose can both
report. The final cap diagnostic follows only a successful scan and close
so it cannot require a third local diagnostic. A validation's own failure
is already in the validation row.

The [name checks](2026-11-14-clock-r19-name-scalar-census.md) add 11,822
actions and the [path helpers](2026-11-14-clock-r19-path-scalar-census.md)
add 337,808. With the [spend rows](2026-11-14-clock-r19-spend-scalar-census.md),
the full prefix envelope is 408,627 actions and the full tail-start
envelope is 376,799.

For a conservative split, allow the full prefix envelope on all 1,048,837
setup/body scans, then reserve full prefix envelopes for up to two
already-admitted scans with work remaining after the cut. Allow 309
full tail-start envelopes for terminal publication. The overlapping
prefix/tail alternatives are reservations for unknown cut placement.
The resulting per-helper reservation component is 428,700,364,944
and the three-helper component is 1,286,101,094,832. These are source
author bounds for reached scan subtrees. A tighter ledger can transfer
exclusive rows to their exact sponsors.

The calculation leaves the surrounding write operations, other source
families, parent and target work and all native timing/storage premises
open. It supplies no executable price or whole fit.
