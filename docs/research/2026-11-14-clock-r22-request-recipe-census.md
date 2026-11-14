# Clock R22 admitted request recipe work

This is the local source bound for `validate_request_recipe` in
the [pinned codec](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/056-sup_codec.cpp).
It owns its branch and result handling and the string compares and
template expansion below. The separate run identity, file hashes,
path validation, join, bound-string copy and meter rows own their
call argument formation and bodies.

## Template and text helpers

| Template region | Actions per invocation |
| --- | ---: |
| Boundary, string meter outcome, initial output/state and outer test | 152 |
| Per input unit including loop, all token probes, token/length binding, compare and selection | 384 |
| Per output unit for copy and final direct-store branches | 28 |
| Return the result of one `valid_wide`, including its fixed work | 36 |
| Per output unit for UTF-16 and ASCII validation | 136 |

The token-probe upper allows 19 indexed narrow literal comparisons
across the 6-, 6- and 7-unit spellings at 14 actions each. Each
includes indexed wchar load, promotion, literal conversion, the
condition, and a reached short circuit. The three extent guards
reserve 24 and the other 94 cover input loop dispatch/control,
initializers, lengths, pointer and token changes, replacement cap,
join result and progress. Each continuing loop advances input by at
least one, including the `<BUILD>` branch. There can be one refusing
iteration in place of a continuing one. The replacement and direct
copy loops share at most 512 output cells. Their 28-action unit includes
indexed load and store, operand formation, postincrement, control and
loop events. The final converted NUL and per-copy-loop ending tests
use the fixed and per-input allowances.

The reviewed [decoder leaves](2026-11-14-clock-r22-decode-scalar-census.md)
supply the `valid_wide` fixed 36 and `64 + 3 × 24 = 136` per unit,
including the three possible surrogate helpers. This gives 280,764
per template, or 7,861,392 across 28 visits. The `join_path` calls
inside `<BUILD>` use the separately counted join schedule.

`text_matches` uses at most 32 fixed and 90 per unit, of which 56 is
the complete `cell` call, and 34 covers loop/branch, expected-path cell
selection and cast, and comparison. The recipe can reach two common
calls, up to 28 compiler templates or one native data path, and eight
environment substitutions. The common cap of 38 visits at 512 units
gives 1,752,256. Use the same fixed/unit form for the one `literal`
call with the seven-unit recipe argument.

## Root recipe work

| Local region | Upper |
| --- | ---: |
| Entry, identity result, references, local path defaults and output tests through prefix comparison | 184 |
| Path-component separator search | 18,944 |
| Stage/transport joins and comparison outcomes | 120 |
| Form the two named file-path suffix arrays and indices, join outcomes and identity receiver | 192 |
| Hash and path validation outcomes, child-arm control and compiler identity tests | 140 |
| Twenty-eight compiler loop visits with helper result checks | 952 |
| Native recipe digest text check, 32 visits at 64 local plus two complete cell calls at 56 each | 5,632 |
| Native recipe number check, three groups at up to 80 fixed, one 56-action leading-zero cell and 19 × (60 local + 56 cell-call) | 7,020 |
| Environment matching, eight groups at 128 fixed plus 512 × 84 | 345,088 |
| Final hash outcome, byte proof, current store, return and retirement | 64 |
| Local subtotal before the separate comparison leaves | 378,336 |

The separator-search row allows 512 × 37 including start and final
test, each of two reached separator comparisons with conversions and
indexed member work, and loop update. The suffix arrays have 20 and
21 initialized `wchar_t` cells. Their array initialization and two
digit-selection writes are included. The native-recipe number row
uses one `cell` per digit and can read the lead digit after the loop.
Each explicit UTF-16 numeric read is
budgeted there together with the subtraction, overflow test,
step, control and corresponding destination. The native digest loop
likewise includes up to two `cell` calls and the comparison work
per body. The environment non-template arm uses 56 per cell read
and 28 for its typed comparison, other operand and control. Its
8 × 128 fixed group includes selection, cap checks, loop limits and
the substitution arm's call outcomes.

Outside these groups three folded path comparisons have call bounds
of 72 each including Win32 `CompareStringOrdinal`. Eight nonfolded
sentinel comparisons use up to `48 + 19 × 28` apiece, including the
matched-length loop and Boolean guards. Up to four digest compares
use 790 each, following the reviewed
[terminal conversion leaves](2026-11-14-clock-r22-terminal-codec-scalar-census.md).
Together with one seven-unit `literal`, these cost 8,678.

| Component | Actions per helper |
| --- | ---: |
| Root recipe work | 378,336 |
| Template expansion and validity | 7,861,392 |
| Text matching | 1,752,256 |
| Other comparison leaves | 8,678 |
| Proposed local upper | 10,000,662 |

The bound sums work from mutually exclusive compiler and native
recipes for simplicity. Independent review matched the pinned source,
template advance and cap, call sites, native leading-zero check and
the resulting arithmetic.
