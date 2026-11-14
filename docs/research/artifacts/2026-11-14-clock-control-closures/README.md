# Measure the exact Clock control-action closures

The [compile-only result](result.json) covers the 11 original
`expect<F>` control-action closure types under pinned MSVC 19.44.35228.
Every tag refers to the original `F` deduced by the existing by-value
parameter. The independent review stripped the observer and tags and
recovered the original checker bytes exactly. Both headers remained exact.

The first wrapper invocation stopped before compiler startup when the
control script's `instrument` module shadowed the named-closure module.
The common launcher now loads its own instrumentation from its exact
path. The subsequent compile exited zero, checked 110 includes and
emitted all 11 read-only data records. No link or target run occurred.

Run `python -E -B instrument.py <new-source-directory>` followed by
`python -E -B run-purpose.py <source-directory> <compiler-directory> <new-output-directory>`.
