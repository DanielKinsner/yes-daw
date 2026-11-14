# Clock R22 native work by prefix and tail

The [coverage check](artifacts/2026-11-14-clock-r22-native-work-map/check.py)
maps all 87 native function definitions in the exact R22 source. The
table joins the local scalar worksheets by phase. It preserves the
conservative overlaps for incompletely observed cut placement and
counts direct scalar output stores once. Win32 bulk destination
extents have a [separate worksheet](2026-11-14-clock-r22-native-output-store-census.md).

| Work | Compiler prefix | Mandatory tail |
| --- | ---: | ---: |
| [Profile and command](2026-11-14-clock-r22-profile-command-census.md), one recipe bound-string copy, and [recipe](2026-11-14-clock-r22-request-recipe-census.md) | 13,279,834 | 0 |
| [Request decoder local](2026-11-14-clock-r22-decode-scalar-census.md) | 4,441,192 | 0 |
| [Request and identity with their meters](2026-11-14-clock-r22-identity-meter-work.md) | 30,023,518,404 | 0 |
| [Non-scan joins, validations](2026-11-14-clock-r22-scan-final-census.md#nested-path-work) and [meters](2026-11-14-clock-r22-nonscan-path-meter-work.md) | 26,643,356 | 62,418 |
| [Launch formatting](2026-11-14-clock-r22-launch-format-census.md) | 7,643,847 | 0 |
| [Capture and process creation with positions](2026-11-14-clock-r22-capture-creation-census.md) | 4,257 | 808 |
| [Log writer, counts and tickets](2026-11-14-clock-r22-log-writer-census.md) | 606,281,984 | 2,084 |
| [Workers, startup and publication](2026-11-14-clock-r22-worker-thread-census.md) | 396,420,102 | 5,646 |
| [Worker acquisitions and cleanup](2026-11-14-clock-r22-worker-coordinator-census.md) | 31,595,666 | 55,214 |
| [Coordinator, root and direct cleanup](2026-11-14-clock-r22-coordinator-census.md) | 7,995,138 | 7,270 |
| [Stage scan subset in both phases](2026-11-14-clock-r22-scan-final-census.md) | 683,128,514,840 | 192,374,335 |
| [Handle close](2026-11-14-clock-r22-close-owner-census.md) | 1,180 | 9,912 |
| [Terminal encoding](2026-11-14-clock-r22-terminal-codec-scalar-census.md) and [publication and retirement](2026-11-14-clock-r22-publication-scalar-census.md) with four tail spends | 0 | 50,708 |
| Remaining pre-worker meter families | 17,454 | 0 |
| [Interlocked scalar destinations](2026-11-14-clock-r22-native-output-store-census.md) | 1,128,786 | 64 |
| Total | 714,247,486,040 | 192,568,459 |

The profile/recipe line adds one `wide_from_binding` at
`140 + 160 × 512` outside the profile's own four calls. The
non-scan phase uses `2,060 × 12,471 + 8 × 59,294 + 478,744`
in prefix. Tail holds `59,294 + 2,904 + 220` for the terminal
transport path and a conservative refusing-transition reserve.
Handle close reserves five calls before capture workers start and
42 in the tail, including the publication close. Terminal validation reaches
at most two `spend` calls and the encoder can reach two more.

The remaining prefix meters are Entry, Profile, RawCommand and
CommandCell, 45 StringGuard and 73 StringCell calls, and up to 28
QuoteCell and eight EnvironmentCell calls. Their 158 `spend` calls
use 103 apiece. Six setup admissions cover TargetQuote, Environment,
CaptureSetup and three Owner tickets, at 160 each. Reserve one 220
refusal for the combined setup group, giving 17,454. These happen
before the workers start. The other families are priced in the named
component rows. A refusal that changes the phase still uses the
existing overlap or cleanup row.

| Helper slot | Prefix | Tail | Total |
| --- | ---: | ---: | ---: |
| Compiler | 714,247,486,040 | 192,568,459 | 714,440,054,499 |
| Producer or verifier, each | 714,245,045,888 | 192,568,459 | 714,437,614,347 |
| All supervisor invocations | 2,142,737,577,816 | 577,705,377 | 2,143,315,283,193 |

The two native target slots use the 2,001,040-action decoder instead
of the compiler's 4,441,192. Both slots retain the same filesystem
and capture envelopes. Independent review checked all component
operands, source families, phase placement and arithmetic.
It also confirmed that the accepted [bulk primitive convention](artifacts/2026-11-06-clock-r13-runner-census-r3/sources/064-20261105-clock-oracle-research-bulk-leaf-clarification.md)
charges the declared Win32 API boundaries and scalar outputs. The
updated table retains bulk capacities as resource obligations.

This is the conditional supervisor scalar envelope. The controller,
two target bodies, resource fit and final native association remain
separate. Per-family prices and catalogue binding are still open.

## Work before the helper reads its request reservation

The initial bound starts at entry and extends through the decoder's
attempt to install the request-specific meter limit. It includes the
profile and raw command parser, two non-scan path validations, request
I/O and the decoder up to its checked budget
transition. The full 4,441,192 compiler decoder envelope conservatively
sponsors its local prefix.

The reached meter sites in this interval are two profile calls,
eight bound-string calls, two raw-command calls, up to 41,262 read and
decoder calls, and up to 132 path validation calls. Thus 41,406 spend
calls at 103 apiece and one 220-action refusal can occur in the envelope.
Decoder string-copy calls come after the limit attempt. The earlier
refusal cleanup retains its separate tail worksheet.

The local source upper is
`(16 + 1,492 + 180) + 3,197,112 + 2 × 59,294 + 2,641,376 +
4,441,192 + 41,406 × 103 + 220 = 14,664,994`. This early work is already part of the overall
prefix. The numeric bootstrap debit limit still requires the actual
catalogue's debit-amount and price schedule.
