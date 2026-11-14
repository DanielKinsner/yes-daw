# Clock exact closure layout purpose

The [direct compile-only measurement](artifacts/2026-11-14-clock-direct-layout/result.json)
resolved 13 of the 30 missing type layouts. The other 17 must be associated
with the original closure expressions and the same template specializations.
[Reviewed closure correspondence](artifacts/2026-11-06-native-sdk-context-ledger-r2/sources/038-layout-source-correspondence.json)

Start with `get` in the checker TU and `nextBit` in the pinned independent
clock header. An instrumented copy can name each using `decltype` directly
after its original definition. Keep each expression, enclosing function,
capture list and variables intact. Associate two `sizeof` and `alignof`
pairs with a unique data record. Prototype a namespaced class-template
static data member with `__declspec(dllexport)` and a per-site integer tag.
Taking its address immediately after the named closure definition supplies
the exact `decltype`. The proposed export has to survive `/c` in the selected
compiler's object. Check the COFF directive, section, and exact tagged
record together before adopting this route.
[MSVC data export documentation](https://learn.microsoft.com/en-us/cpp/build/exporting-from-a-dll-using-declspec-dllexport?view=msvc-170)

The other closure groups need separate type association:

| Group | Count | Observation site |
| --- | ---: | --- |
| Checker control actions | 11 | Actual `F` template parameter of `expect<F>` for each exact call. |
| Clock comparators | 3 | Actual comparator type of the original four-argument `upper_bound` call, with the same forwarding and instantiation context. |
| Selected MSVC append closure | 1 | Actual `_Fn` used by the reached `basic_string<char>::_Reallocate_grow_by` specialization for the original append expression. |

An inline or anonymous closure may be absent from an optimized object.
Require a uniquely labeled and mechanically decoded compiler data record,
or a documented debug type record with a complete type and alignment
association. Prove emission in a small compile-only purpose before relying
on it for the whole type set. Preserve unknown values when an observation
cannot be associated. A source clone of a lambda expression creates a
distinct type and cannot establish any of these exact layout facts.

Use the recovered compiler, exact original include tree with only the
reviewed instrumentation substitutions, and the same ABI-affecting flags.
An eventual object can contain code from the actual-source instrumentation.
Decode it as data, with no link or execution. The source-plan review gate
still applies before the exact purpose is compiled.
