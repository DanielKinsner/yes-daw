# Clock R18 scan path work bounds

These source occurrence bounds expand `stage_scan` into its two path helpers
and name checks. They apply per admitted scan call, including refusal.
[Stage census](2026-11-14-clock-r18-stage-scan-census.md)

| Operation | Upper per scan | Basis |
| --- | ---: | --- |
| `same_name` invocations | 62 | At most two pseudo-entry checks on each of ten records. At most seven ordinary records reach the six-name allowlist. |
| Character comparisons inside `same_name` | 638 | Pseudo checks take at most five comparisons per record, including both NUL checks. Allowlist lengths 10, 10, 12, 11, 13, and 22 give an upper of 84 comparisons per ordinary record, including one NUL check per name. |
| Loop iterations to find an allowed ordinary name's length | 154 | Seven names bounded by the longest allowed spelling, 22 units. Matching a candidate requires a NUL exactly at the allowed length. |
| `GetFileAttributesW` calls inside `validate_path` | 512 | At most eight path validations, each calling after the 64-ancestor guard. The 65th iteration, if reached, refuses before the native call. |
| `GetFullPathNameW`, `GetVolumePathNameW`, `CompareStringOrdinal` | 8 each | Each path validation reaches at most one of each. |
| Volume-root length loop iterations | 4,104 | At most 513 cells per path validation, including the oversize refusal prefix. |
| Separator-trim and component-discovery loop iterations | 4,096 combined | For each of the eight path validations, these loop bodies jointly remove at most the original 512 path units. Each discovery step measures the next discarded suffix. An unsuccessful comparison at either loop boundary is counted separately. |
| Code-unit copies inside `join_path` | 4,096 | Eight admitted output paths, each bounded to 512 units before either copying loop starts. |
| `PathCell` charge argument sum | 8,347 | Eight validation paths and eight join bases contribute at most 512 units each. The seven ordinary suffix lengths add at most 154 units and the initial star adds one. Each join charges the sum of base and suffix *before* the output extent check. |

The occurrence rows track inspected code units, scalar comparisons and native
API invocations. Aggregate initialization, loop-condition and refusal
bookkeeping, implicit copies, native leaf contracts, call and return costs,
and meter expansion remain part of the complete scalar census.
