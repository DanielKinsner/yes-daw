# Clock R22 native launch formatting

The [pinned codec](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/056-sup_codec.cpp)
builds the mutable target command and sorted UTF-16 environment inside
`State`. These local bounds cover `build_target_command`,
`build_environment`, and their `append` and `cell` calls. The meter
wrappers and eventual native process creation have separate sponsors.

An `append` call can evaluate and bind four operands including the
state array and capacity, index reference, and converted character. It
tests the remaining extent, writes the character at the former index,
increments the index and writes another NUL. The upper is 24 actions
for the boundary, including the state-array decay and the longest
two-index environment name expression, eight for the guard and 18
for the two indexed stores, postincrement and return. A failed guard
exits before either store.

A `cell` call takes a by-value two-field `Text` and index. Allow 16
for argument formation, both field reads/destinations, typed index and
call events, up to 12 each for the two indexed byte loads with typed
offset and cast, and up to 16 for shift, OR, result and scope events.
Use 56 per call including its evaluated input. Reading a byte as U32
requires its explicit cast.

## Target command

| Work | Per unit | Instances |
| --- | ---: | ---: |
| State admission result, fixed literals/conditions, counters, caller and return | 128 | 1 |
| Scan and copy one target-path character outside the append call | 42 | 512 |
| One argument's declaration, empty, meter and quote choices, and loop endings | 124 | 28 |
| Quote probe local work per argument input cell | 38 | 14,336 |
| Quoted walk per input cell including slash-control tests and updates | 54 | 14,336 |
| Unquoted copy and remaining quote-body local work per input cell | 40 | 14,336 |
| Four possible `cell` calls per argument input cell | 224 | 14,336 |
| Append invocation and output loop actions | 67 | 29,271 |
| Per helper | 7,089,877 | |

The quote probe reaches up to two `cell` calls per input unit. The
quoted walk visits an input unit as either a slash or the next
ordinary character. It can inspect each once for the slash test and
once more when consuming the ordinary character. The unquoted loop
instead visits each unit once. The summed local groups reserve both
output alternatives. The slash-walk local group includes the post-test
selection of zero, doubled or single slash runs and quoted-copy loop
setup/exit. Only successful appends advance `n`, and no path changes
`n` elsewhere after initialization. At most 29,270 succeed, followed
by one possible refusal. The 17 output-loop actions above `append`'s
50 cover tests, induction, branch and event even when the append fails.

## Environment block

| Work | Per unit | Instances |
| --- | ---: | ---: |
| Caller, meter outcome, used index and final block NUL, return | 104 | 1 |
| One entry's setup, `Text` input selection and field debit, delimiter, checks and retire | 156 | 8 |
| Visit one name/value unit including loop and index control | 28 | 4,139 |
| Value unit `cell` invocation | 56 | 4,096 |
| Name/value and equals-sign appends | 50 | 4,147 |
| Per helper | 553,970 | |

The eight name lengths sum to 43 and the value cap is `8 × 512`.
Each entry's direct terminating NUL belongs to its fixed row. Its
equals-sign calls `append`, as do names and values. The final extra
NUL is a direct local store. The sum includes the failing prefix if
any append refuses.

The two stages contribute at most **7,643,847 source actions per helper**
with the [reviewed meter occurrence bounds](2026-11-14-clock-r22-codec-meter-census.md)
as schedule inputs. Independent review accepted the typed append
boundary, input and output traversal bounds, and arithmetic. Meter
costs remain separate.
