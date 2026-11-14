# Clock R22 profile and raw command source work

These rows cover `validate_profile`, its calls to `wide_from_binding`,
and `parse_command` in the [pinned R22 codec](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/056-sup_codec.cpp).
The [core source](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/057-sup_core.cpp)
provides both folded comparisons. The
[decoder's UTF-16 leaf](2026-11-14-clock-r22-decode-scalar-census.md#string-copy-and-validity)
supplies `36 + 136L` actions per new invocation of `valid_wide` on
`L` units, including its boundary and surrogate helpers. Decoder and
recipe invocations have their own schedules.

## Bound string conversion

| Local region in `wide_from_binding` | Actions | Coverage |
| --- | ---: | --- |
| Function boundary | 28 | State/ref input, pointer and length fields, output reference, formals, dispatch, entry and return. |
| Meter outcomes and guard | 36 | Count input read, result negations, short circuits, pointer-null test, typed zero and extent tests, validity result and refusal. |
| Loop fixed operations | 16 | Index initialization with conversion, final comparison and scope events. |
| Copy loop | `24L` | Test, branch, two indexed addresses, source unit read and destination write, index update and iteration event. |
| Final output stores and exit control | 24 | Converted NUL and length read/address/store, return-value formation and scope exit. |
| Local upper | `104 + 24L` | Includes all reached local control on a refusing call. |

Including the new validity invocation, the bound is `140 + 160L`
for `0 <= L <= 512`. An oversize, zero-length or null input refuses
before visiting string storage. The length debit precedes
those guards. Its possible unadmitted U32 input remains accounted
for by the [meter occurrence worksheet](2026-11-14-clock-r22-path-meter-census.md).

## Profile admission

| Local group | Actions | Coverage |
| --- | ---: | --- |
| Invocation and initial catalogue tests | 64 | State reference, formal/dispatch and entry, four field Boolean tests, bootstrap and attribute zeros, conversions and refusal. |
| Family price loop | `16 + 36 × 32` | Index setup and final test. Each iteration counts typed enum bound and array price read/address/zero check, loop control and refusal. |
| Helper and tail reservation loop | `16 + 3 × 72` | Per slot: indexed catalogue reads, all three relationship checks, checked subtraction order, typed zero, conditional branches and loop control. |
| Runner, producer and verifier reservation constraints | 104 | All field and signed-limit reads, explicit U64 conversions of INT64_MAX, zero tests, subtraction/addition and subsequent conditional selection. |
| Bootstrap store and entry/profile debit outcomes | 64 | Nested meter destination and limit read, both debit outcomes, local Wide length initialization and lifetime, four bound-copy result tests and refusal. |
| Compiler argument and environment guards | `36 × 48` | Twenty-eight plus eight entries, each with pointer and length indexed reads, null/extent checks, validity outcome, typed loop bound, progress and branch. |
| Loop fixed operations and successful return | 32 | Both loop initial/final conditions, Boolean return and outer scope events. |
| Local upper | 3,392 | All entered local groups. |

At most four bound paths each take `140 + 160 × 512`. At most
36 further direct validity calls each take `36 + 136 × 512`.
They add 2,836,288 actions, yielding **2,839,680** per helper for
profile admission before meter costs. A refused early check can
only shorten the sequence.

## Raw command parsing

The pinned limit is 2,576 UTF-16 units, including the leading and
trailing helper-path quotes, one space, 12 prefix characters, up to
512 helper-path units, four hex digits for each of up to 512 request
units and the final NUL. The two destination lengths are guarded
independently. After 512 copies, a subsequent non-delimiter can
reach one extra outer condition and reject the count without a copy
or digit iteration. The fixed exit groups include those tests.

A shorter malformed command can reach part of a four-digit group.
Each read gets the next unit in order. A NUL inside the group fails
the digit classes immediately and leaves subsequent units unread.
The prefix loop also refuses at the first NUL it encounters. The
separator read follows an observed closing quote and an extent
test. If that quote ends the string, the next unit is the NUL.
These facts bound the reached prefixes under the documented
process-owned command string contract. [GetCommandLineW](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-getcommandlinew)

| Local group | Actions | Coverage |
| --- | ---: | --- |
| Invocation, debit results and raw pointer admission | 64 | State ref/call, both meter outcomes, parameter-free `GetCommandLineW` entry/return, LPWSTR-to-const pointer binding, null and promoted opening-quote tests, typed counter initializations. |
| Quoted path loop | `512 × 56` | Three while predicates and short circuits, indexed wide loads and promotions, extent guard, two postincrements with address formation, copied unit and iteration event. |
| Quoted path exit | 112 | Final while test including a possible refusing over-cap body, end-quote, nonempty, remaining-cap and separator tests, converted NUL and length stores and cursor advance. |
| Prefix loop | `16 + 12 × 40` | Setup and final test, zero request counter, typed per-step index bound, offset/postincrement, indexed unit reads and promotions, comparison, loop control and refusal. |
| Request hex loop outside the digit loop | `512 × 80` | Outer condition, count and remaining-space checks, converted accumulator zero, digit-loop fixed setup/exit, decoded zero rejection, wide cast/store and increment. |
| Four digit visits per request unit | `2,048 × 72` | Ten for loop control, ten for read/postincrement and digit initialization, 30 for both promoted class checks, 12 for chosen arithmetic/cast/result or refusal and ten for shifted accumulation and store. |
| Request hex loop exit and destination fields | 72 | Final test including a possible refusing over-cap body, checked end/cursor/nonempty guards, converted NUL and length stores. |
| Validity/match outcomes and module-path storage | 40 | Local Boolean selection for the two validity checks and first match, refusal and the uninitialized module-path array lifetime. |
| Self-module path query | 48 | `GetModuleFileNameW(HMODULE, LPWSTR, DWORD)` receives typed null, the decayed writable array, and the converted `path_units + 1` expression. Includes caller, formals, native entry/return and the DWORD destination. |
| Self-module result checks and final return | 32 | Typed zero and extent tests, short circuits, match result, forwarding the separately counted path validation result and scope retirement. |
| Local upper | 217,952 | Includes the Win32 boundary. The supplied array extent is resource evidence. |

The module query passes capacity 513. A result of 0 or a capacity
result of 513 refuses before comparison, leaving at most 512
observed units for its explicit-length compare. [GetModuleFileNameW](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew)

Each of the two folded `equal_wide` calls uses at most 72 source
actions. Allow 28 for source-side pointer/length evaluation, decays,
the DWORD-to-U32 input conversion on the module-path site, five
formals, dispatch and entry. Allow 18 for length equality, fold and
extent tests and conditional exits. The other 26 cover evaluated
native arguments, explicit U32-to-int length casts, five native
formals, native entry/return, the int result test against
`CSTR_EQUAL` and the wrapper return. Both callers pass true.
The case-sensitive unit loop therefore has no invocation here.
The folded call uses explicit lengths and `TRUE` for the native
BOOL. [CompareStringOrdinal](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-comparestringordinal)

Two new validity calls at up to 512 units add 139,336 and the two
folded calls add 144. This gives **357,432** for raw command parsing
and **3,197,112** for both preflight steps per helper invocation.
The final `validate_path` call retains its separate non-scan path
sponsor, including its evaluated arguments.

The debit sites remain in the [codec meter schedule](2026-11-14-clock-r22-codec-meter-census.md)
and bound-string meter schedule. These rows count outcome branches
and variable count work once, leaving `spend` its own complete
boundary and typed amount conversion. Refusal in either preflight
function propagates to the same early scope in `supervisor_main`.
That scope runs `begin_tail`, safe owner retirement and controlled
exit. Their bodies and the first cut transition in a refusing
meter belong to the separate cleanup and meter budgets.
