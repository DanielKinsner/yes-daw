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
