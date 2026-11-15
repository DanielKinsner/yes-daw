# Clock supervisor compile-only ABI observations

Pinned MSVC 19.44.35228 [emitted 23 layouts and eight offsets](result.json)
using the exact supervisor header. Its reported 103 includes matched
the original compiler/header pins or the local Windows SDK
10.0.26100.0 snapshot. The source and SDK file hashes were checked
before and after compilation.

The resulting `State` is 191,064 bytes, aligned to eight. It includes
the `Request`, `Terminal`, both `Worker` instances and their
buffers. Count their storage inside `State` once. Separately formed
path, directory, job and startup objects still contribute to their
reached lifetime joins.

The independent review checked the compile-only scope and exact
decoder mapping. The object contains read-only layout data. No helper
code was linked or run. The 23 layouts and eight offsets qualify the
local SDK/profile only. Native backing objects, actual compiler
runtime module association, physical stack and whole resource fit remain
separate open terms.

Run `python -E -B run-purpose.py <compiler-directory> <new-output-directory>`.

## Exact R20 object compile attempt

A separate independently reviewed `/c` purpose attempted the four exact
R20 translation units. `sup_codec.cpp` and `sup_core.cpp` compiled. The
third unit, `sup_main.cpp`, exited 2 at the source-only guard with
MSVC C4127, followed by C2220 under `/WX`.
`sup_io.cpp` was after that failure and was not reached in this attempt.
The plan's existing warnings-as-errors setting stays in the purpose.
An unbound source successor can make this compile-time guard an
`if constexpr` and retain all five false admission constants.

Run `python -E -B compile-objects.py <compiler-directory> <new-output-directory>`
to reproduce the exact R20 compile-only attempt.

## Admission-enabled supervisor frame

The independently reviewed [entry purpose](entry-frame-purpose.py),
SHA-256 `EB699CE20949D83322A84905488187E0AFE3CA0BE2FDEC9A3DDD0E7DC3E7DB1B`,
compiled only exact R22 `sup_main.cpp` against a temporary copy of
`sup.h`. The copy changed exactly five admission literals. Pinned
MSVC exited 0 under `/WX /O2 /c`. The [receipt](entry-frame-result.json)
records 129 ordered include visits across 103 pinned paths. It
supplies no catalogue definition.

Run the purpose without flags for the read-only preflight. Use
`--read 330B778295798B763E7FE084BC84C1B8125D72233F8765B1BA6D09EE5A0BC3E6`
to decode the retained object. It shows the direct `wmain` call
into the 605-byte admitted `supervisor_main` and the latter's
profile-validation call. The two named frames decrement RSP
40 and 191,160 bytes respectively, totaling 191,200. The `State`
storage is included in that entry frame. Other live frames, return
addresses, the two worker stacks and runtime backing remain in
the resource proof. The measured code still needs a final PE join.
No link, supervisor run or Clock run occurred.
