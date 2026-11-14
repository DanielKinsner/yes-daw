# MSVC local closure emission check

The independent source reviewer accepted this two-closure compile-only probe.
Pinned MSVC 19.44.35228 emitted two read-only, relocation-free `.yclos`
records and two export directives under the same optimization and ABI flags
as the direct layout purpose. The eight reported headers matched their pins.

| Probe site | Size | Alignment |
| --- | ---: | ---: |
| Reference capture | 8 | 8 |
| Uncaptured | 1 | 1 |

The object SHA-256 was
`C1B9323F79E9BA4AA3F39E0A7DE9815D1CD7CA04DF9A3746221ECBDDE897CB30`.
Reproduce with `python -E -B run.py <compiler-directory> <new-output-directory>`
using [the recovered compiler](../2026-11-14-clock-direct-layout/README.md).
No link or execution occurred. The actual checker closure types still need
source-site instrumentation and measurement.
