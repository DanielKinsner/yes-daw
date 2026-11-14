# Clock R19 spend and failure scalar census

This author upper composes with the [charge rows](2026-11-14-clock-r19-charge-scalar-census.md).
The exact source is [sup_core.cpp](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/057-sup_core.cpp)
with defaults and the four-field `Failure` declaration in
[sup.h](artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources/055-sup.h).
Native lock internals remain outside the scalar convention.

## Failure recording

| Region | Upper actions | Source basis |
| --- | ---: | --- |
| Call and enter `fail` | 11 | Receiver, site, reason, known and error evaluations, dispatch, five formal initializations and entry. The state argument is a reference. |
| Construct the failure item | 19 | Four argument evaluations and bindings, dispatch, entry, two casts with operands, known/error conditional with branch, four aggregate field writes and return. The aggregate is a directly constructed prvalue. |
| Terminal binding and primary test | 9 | Address, receiver and member selections, reference binding, site read, zero test and branch. |
| Record primary | 19 | Four field reads and writes plus receiver selection, then known guard and flag update. |
| Record secondary count, first secondary and first native secondary | 62 | Count saturation test/update up to 12, first-secondary condition up to 6 and assignment/flag/known work up to 22, first-native-secondary condition and assignment/flag up to 22. Three flags may update, and each candidate item copy accounts for its four fields. |
| Stop and retire | 10 | Set stop, read the record-freeze flag, branch, set late-tail failure when reached and return. |

The primary and secondary arms are alternatives. The larger arm gives
`11 + 19 + 9 + max(19, 62) + 10 = 111` including one call into `fail`.

## Cut and spend

The `begin_tail` call/entry needs four actions. Acquisition and release
each need at most six. Testing and storing the cut and prefix frontier
needs up to 15. Choosing the valid prefix case and updating the recorded
prefix and flags needs up to 24. Reading the report, valid and refused
bits and setting the one-time latch needs up to 17. The two diagnostic
conditions and the function return need up to ten. These rows total 82
before a diagnostic. Only one diagnostic can be called here, so the
upper with `fail` is 193.

For a scan caller, eight actions cover the `spend` dispatch, caller
state/family/count evaluation, the three formal initializations and entry.
Two further actions cover the branch on the return from `charge` and two
cover a literal Boolean return. Evaluation of a variable count expression
and enclosing caller branches go in their own rows.

Together with the 84-action prefix and 27-action tail charge upper, the
regular `spend` path is bounded by 96 prefix or 39 tail actions. The
refusing prefix can add the 193-action `begin_tail` and diagnostic.
The per-scan total for the [555 meter calls](2026-11-14-clock-r19-stage-meter-census.md)
is therefore at most 53,473 using the uniform prefix upper plus one cut.
For a scan begun in the tail it is at most 21,645. These bounds are
source rows for one component, with whole schedule and independent scalar
acceptance still open.
