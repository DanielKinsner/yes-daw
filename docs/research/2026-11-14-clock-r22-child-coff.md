# Guarded Clock R22 child object

Data-only inspection used the [compile-only receipt](artifacts/2026-11-14-clock-r22-child-object/result.json)
and its 475,557-byte AMD64 COFF object, SHA-256
`D49BE069F183A5F0159854AA5306039C8076026D5014361C914F71B4696659B9`.
The existing [COFF decoder](artifacts/2026-11-14-clock-r22-coff-resource/read-object.py)
was applied in memory with the exact 1,706-section count and x64
handler flags 1 and 2 admitted in addition to its previous flag
profile. All records still passed its structure checks. A second
file read matched the named code and relocation targets. No child,
linker or compiler ran during this inspection.

The reader now accepts the child's exact receipt and verifies these
entry and call-site facts with its `--child <clock_stage.obj>` option
alongside the existing supervisor-object directory input.

The object carries 349 function-table ranges, including 133 chained
ranges with zero independent prologue growth. Its same-name section
totals include 4,413 bytes of `.rdata`, 24 of `.data`, 12 of `.bss`,
672 of `.rdata$r`, and 241 of `.data$r`. The `.data` symbol is the
24-byte `clock_stage::cleanupClose`. These are object contributions
before link-time section handling and the final mapped image layout.

| Named prologue | RSP decrement in bytes |
| --- | ---: |
| `main` | 232 |
| `run` | 341,864 |
| `readText` | 196,744 |
| `metadata` | 65,928 |
| Truncated and trailing action operators | 65,720 each |
| `writeText` | 65,640 |
| `number` | 65,624 |

`main`'s 94-byte code range reserves 232 bytes and constructs the
Rejection with the `SOURCE_HOLD_NO_CLOCK_EXECUTION_GO` literal.
Its direct relocations at offsets 34, 43, 50 and 63 resolve to that
literal, the Rejection constructor, the throw information and
`_CxxThrowException`. Separate 147-byte and 91-byte catch funclets
call `fprintf` and `reportCleanupClose`. The guarded entry has no
call to `run`.

The compiler emitted `run` separately. Its code offsets 7,184 and
10,037 have direct call relocations to `readText`. Their two named
prologues have a combined **538,608-byte** RSP decrement if one such
call is reached in this object. This excludes return addresses,
descendant calls, EH, loader and CRT paths. The combined number
does not describe a whole enabled child and cannot be used to
infer a future entry's frame.

There are 45 distinct unresolved externals including 21 `__imp_`
names and the undefined `source_costs::Catalogue::reviewed_`. No
external names a direct process-creation API. This observation is
limited to this object. It gives the final linked import and runtime
association no credit and does not certify physical stack, total
controlled scratch or 32 MiB fit.
