# Clock R22 admission read

The independent source review passed 7,324 checks, including the
1,288 actual input hashes, 38 closed directories, 32 guard combinations,
and full source correspondence. The first reviewer command failed on
a missing Python import. It was corrected and rerun against the same
candidate. The exact
`28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06`
manifest pins 1,288 files. The compile-only launcher uses that manifest
for the four exact native units.

The R21 guard correction exposed C4702 in the compiled body. R22 uses
the original five admission constants to initialize one local
`const volatile bool`, then evaluates the refusal from its read. The
32-combination truth table matches the original guard. The source
census must count both initialization and read, and the pre-body owner
inventory must include the local. The refusal remains exit 10.
The frozen candidate README overstates the language guarantee. C++
requires the volatile access. Whether the compiler emits a specific
conditional branch is a separate object-code observation.

Run `python -E -B candidate.py` with the exact local R21 candidate staged.
The materialized runner has a derived 17,209-node limit. All execution
admission constants remain false and the binary bindings remain open.

Pinned MSVC compiled all four exact
[R22 native units](../2026-11-14-clock-supervisor-abi/r22-object-result.json)
to x64 COFF using `/W4 /WX /c`. The isolated compile scope received
independent review. C4127 and C4702 cleared. Link, native profile,
source pricing and resource fit remain open.
