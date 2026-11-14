# Clock R22 path and string meter calls

These per-helper bounds use the exact R22 source. A call that
requests a debit counts even if the debit or a later guard refuses.

| Family | Calls | Sum of requested amounts |
| --- | ---: | ---: |
| `StringGuard` | 45 | 45 |
| `StringCell` | 73 | 4,295,004,159 |
| `PathGuard` | 553,951,733 | 553,951,733 |
| `PathCell` | 16,788,405 | 8,758,291,395 |

The profile can check four bound strings. The recipe can check the
source base again. Decoding can copy at most forty text entries and
the compiler recipe can expand up to twenty-eight templates. Each
template asks for one `StringCell` debit without a `StringGuard` call.
The bound for `StringCell` is 73 × 512 = 37,376 when Data binds all
profile path lengths within the admitted extent. Before that condition
is established, a `wide_from_binding` call can request any U32 length.
At most one oversize request is reached because its length rejection
ends the profile. The table conservatively uses UINT32_MAX + 72 × 512.

## Paths outside scans

Nine validations can be reached outside scans. The count includes
command parsing, request reading, both identity files, four direct
recipe paths and the terminal transport directory. Each validation can
debit `PathGuard` once on entry and at most 64 times for ancestors,
plus one `PathCell` debit for at most 512 units.

The recipe has six common joins totaling 77 suffix units. The compiler
arm can expand at most floor(512 / 7) build tokens per template across
28 templates. Each token joins a five-unit suffix. The other arm's two
joins fit within that mutually exclusive allowance. Up to eight
environment substitutions and two log setup joins add 88 and 20 suffix
units. Across the non-scan sites this gives at most 2,060 joins and
10,405 suffix units. Each join can debit `PathGuard` once and
`PathCell` once for its base and suffix, before the combined extent check.

## Scan paths

The [scan occurrence bound](2026-11-14-clock-r18-stage-scan-census.md)
allows up to 1,049,146 scans. The [path occurrence bounds](2026-11-14-clock-r18-scan-path-census.md)
give each scan up to eight validations, eight joins and 8,347
requested `PathCell` units. A scan therefore reaches up to 528
`PathGuard` and 16 `PathCell` calls.

The [headless check](artifacts/2026-11-14-clock-path-meter/check.py)
verifies the source pins, site counts and sums. The separate string and
path scalar schedules still need full accounting.

## Independent review of all 36 families

The critic reproduced all four tables from the five native pins,
50 literal meter sites and R22 constants. The review covered unequal
helper-slot caps, short reads and refusal paths, file and run-identity
padding, excess string lengths before rejection, mutually exclusive
recipe arms, and capture acknowledgements across the prefix cut. The
invocation and requested-amount bounds are conditionally accepted
under the exclusive ready/ack handoff, visibility and native output
extent premises. Successful debits by phase, scalar work prices and
native time remain open.

From the repository root, with the exact R22 candidate staged, run
`python -E -B docs/research/artifacts/2026-11-14-clock-meter-inventory/check.py --check-bounds`.
The command checks each schedule and requires every native family to
appear exactly once across the four tables.
