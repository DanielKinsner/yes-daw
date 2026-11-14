# Clock compile-only direct layouts

[Measured result](result.json) contains the 26 size/alignment cells and five
profile cells emitted by pinned MSVC 19.44.35228. The compiler exited zero
and reported 110 headers, all matched to the pinned inventory. The isolated
compiler directory contained all 91 original files. The original compiler
and Microsoft catalog provenance are in the [recovery note](../../2026-11-14-clock-r18-pinned-compiler.md).

The independent source review checked the 13 named types against the original
type correspondence and checked the 91 tool files, 427 SDK headers, three
project includes, and the compile-only scope. It verified corrections for
the repository root and expected 76 catalogue families. A preflight attempt
stopped before compiler startup because the profile basename also matched
the archived failed-attempt profile. Using the full original path resolved
that ambiguity before the successful compile.

Pass the 17.14.40 offline layout and a new compiler directory to `prepare.py`.
Pass that compiler directory and a new output directory to `measure.py`.
Run both with `python -E -B`. The measurement is a single `/c` invocation
and consumes the resulting COFF as read-only data. The COFF hash identifies
this run. Layout values and profile cells are the repeatable observations.

The 17 closure layout facts, loaded native module association, whole resource
fit and Clock execution remain pending.
