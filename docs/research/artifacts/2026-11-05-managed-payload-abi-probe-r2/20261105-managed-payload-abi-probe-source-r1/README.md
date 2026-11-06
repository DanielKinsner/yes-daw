# Managed payload ABI probe, source revision 1

**UNRUN. Whole-source independent review is required before compilation or execution.**
This is a new, isolated allocation-measurement program. It does not parse or run R10,
invoke an R10 helper, qualify its path/error ownership census, change any cap, or grant
payload fit, NativeBinding, price, hardware, or execution-GO credit.

## Exact host and launch boundary

Use only the cached Windows x64 PowerShell at:

`C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe`

The future root-owned launcher supplies exactly these arguments, as separate argument
values (the last value is the absolute path to this runner):

`-NoLogo -NoProfile -NonInteractive -File <absolute run-probe.ps1 path>`

The launcher owns the fresh process, `CreateNoWindow`, hidden window style, stdout/stderr
redirection, finite timeout, no-error-box policy and process exit capture. This runner
does not launch a process or create a thread. Do not run it in Dan's interactive shell.
The launcher must use a clean, ordinary noninstrumented CLR startup: no startup hook,
profiler, additional dependency injection, stress/fault injection or altered GC/JIT
configuration. The probe does not read or print environment-variable values. A script
cannot retroactively disprove a hook that ran before it; this remains a launcher premise.

`run-probe.ps1` checks the exact seven R10 InitializeController file pins, plus pwsh.dll,
CoreCLR, CoreLib, CLR JIT, runtimeconfig and both Roslyn compiler DLLs. It also checks
the running executable, loaded CoreLib/PowerShell assembly paths and selected loaded
CoreCLR/JIT module paths using public APIs. Only the two selected module paths are
examined for association; no module inventory is emitted. File hashes associate current
file bytes; the clean fresh-host premise remains necessary for their loaded images.
The fixed metadata source is
`../20261105-managed-payload-abi-runtime-metadata-r1/runtime.json`, SHA-256
`45817E116A4E6F6F9C9AA5089D1773F3F8978442A369D1DAFFDCF6A03CFBB8D0`.

The required runtime is workstation CoreCLR `.NET 10.0.11`, pointer size 8, process
architecture X64, RID `win-x64`, CoreLib informational version
`10.0.11-servicing.26373.116+e2f47b0110ed922f21a1522da67279133ce28f32`.
These are observed setup requirements, not a claim about all CLR builds. The runner
compiles the exact pinned UTF-8 source bytes in memory with the pinned Add-Type cmdlet:
`/optimize+ /warnaserror+ /unsafe- /checked+`. There are no external packages, target
program references, private-field reflection, unsafe operations or uninitialized objects.

## Fixed measurements and assertions

Each pass has 52 fixed cases. Two full, uncredited warmup passes exercise every actual
measurement method, generic constructor, comparer and public capacity-query path before
three recorded repeats. Types and comparers are reused; fixtures are freshly constructed
on every pass. Warmup does not invent measurement rows. Tiered JIT remains the pinned
runtime's normal policy; three exact repeated results, rather than an assumption of no
later compilation, determine acceptance.

| Family | Positive requested sizes/capacities | Recorded cases |
| --- | --- | ---: |
| byte[], char[], int[], object[] | 1, 7, 17, 64, 435, 512 each | 72 |
| fresh string('x', n) | 1, 7, 17, 64, 435, 512 | 18 |
| independent int[] bucket calibration | 3, 7, 17, 37, 197, 1931 | 18 |
| Dictionary<string, object> and Dictionary<string, string> | 3, 7, 17, 37, 197, 1931 each | 36 |
| Hashtable(k, precreated OrdinalIgnoreCase) | 2, 3, 5, 10 | 12 |

The 156 rows contain raw before/after counters, their difference, requested size,
repeat, source-predicted bytes and capacity provenance. There are 21 further derived
coefficient rows and one summary: 178 bounded JSON lines on success, at most 179
with a final allocation-budget failure row.
No constructed fixture contents, keys, exception messages, stack traces, environment
values or private fields are read or printed. A Hashtable has no public capacity getter: its public-capacity
field is -1 and its bucket count is explicitly **source-predicted**, never called observed.

Each counter window contains the fixed integer switch plus the fresh allocation. For
dictionaries only, the same window also contains `EnsureCapacity(0)`, which returns the
actual public capacity without requesting a resize. No delegate, boxing, result-formatting,
result-buffer write or user callback occurs between the counters. After the closing
counter, the result escapes to a static reference and is passed to `GC.KeepAlive`.
This defeats allocation elimination through a nonescaping local; it gives no retained
heap, survival, reclaimability or simultaneous-liveness credit. Earlier fixtures may
become unreachable. The probe never calls GC.Collect.

Every recorded delta is checked against its formula; dictionary public capacities must
equal the requested primes. All three repeats independently derive and check array
strides and a 24-byte array header. Differences between lengths 64 and 512 identify
strides 1, 2, 4 and 8 without alignment-rounding ambiguity. All six lengths per family
then check the predicted 8-byte rounding. This is finite agreement with a source layout
prediction, not discovery of a universal alignment guarantee from six points.

For each dictionary type, subtract independently measured int-array bucket bytes from
the total constructor-plus-capacity-query delta. The change between capacities 3 and 7
derives the entry stride; every remaining capacity checks the same slope and fixed
residual. Subtracting the independently measured common array-header prediction gives
the fixed dictionary-object coefficient. This decomposition assumes the pinned source's
two-array constructor and common SZ-array header; public counters do not expose private
entry-array bytes or field offsets. Hashtables similarly derive their bucket stride from
the 3-to-7 predicted bucket-count difference and check their fixed residual at all four
constructor sizes. Both generic reference-valued types are checked separately.

## Source predictions, not language guarantees

All source associations below use the exact CoreLib informational commit
`e2f47b0110ed922f21a1522da67279133ce28f32`:

- [CoreCLR object.h](https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/coreclr/vm/object.h):
  on this 64-bit layout, an object header plus method-table pointer uses 16 bytes;
  a vector's length and padding give a 24-byte SZ-array base. The predicted request
  is `Align8(24 + n * stride)`, with strides 1/2/4/8 for the four tested families.
- [object.inl](https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/coreclr/vm/object.inl) and
  [gchelpers.cpp](https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/coreclr/vm/gchelpers.cpp):
  string length, a terminator and two bytes per character give `Align8(22 + 2*n)`.
  No empty string or interned-string allocation credit is used.
- [Dictionary.cs](https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Generic/Dictionary.cs):
  Initialize allocates int buckets and Entry storage at a HashHelpers prime. Entry has
  two 32-bit fields and two references: predicted stride 24. The 64-bit dictionary's
  fixed object prediction is 80 bytes. Each measured total must equal
  `80 + Align8(24+4*p) + Align8(24+24*p)`. Both default string-comparer paths are warmed.
- [Hashtable.cs](https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/Hashtable.cs) and
  [HashHelpers.cs](https://raw.githubusercontent.com/dotnet/dotnet/e2f47b0110ed922f21a1522da67279133ce28f32/src/runtime/src/libraries/System.Private.CoreLib/src/System/Collections/HashHelpers.cs):
  the default load factor is float 0.72, and the positive constructor sizes here give
  `H(k)=GetPrime(floor(k/0.72f))` = 3, 7, 7, 17. Source code first performs float
  division and widens its result to double; the probe uses that order. The Bucket
  prediction is two references plus an int, rounded to stride 24; the fixed Hashtable
  object prediction is 72. Totals must equal `72 + Align8(24+24*H(k))`.

The byte-identical cached Dictionary/Hashtable/HashHelpers inputs used for authoring
are pinned in source-pins.json. Their source association is not a claim of complete
source-to-installed-binary equivalence. The finite runtime deltas must agree or the
probe fails. Public layout/storage APIs do not promise these implementation sizes.

The counter is documented to report managed allocations attributed to the current
thread, [not native allocations or bytes surviving collection](https://learn.microsoft.com/en-us/dotnet/api/system.gc.getallocatedbytesforcurrentthread?view=net-10.0).
The capacity getter reports the dictionary's [current capacity](https://learn.microsoft.com/en-us/dotnet/api/system.collections.generic.dictionary-2.ensurecapacity?view=net-10.0).
No peak, in-use, old-heap-free, native/runtime-overhead or hardware claim follows.

## Allocation and exit boundaries

The fixed fixtures are small: maximum array/entry count 1931, with no insertions,
resizes, file payload, test input, process creation or callback. The conservative
cumulative managed request budget for the C# probe is **8 MiB**, not a peak heap cap.
`predictedFixtures` sums the formulas for every constructor in all five passes;
the summary labels this as a prediction. Actual counters include warmup, fixture/result
buffers, checks, all 177 data/derived lines and their formatting on the calling thread.
After writing the summary, an actual final counter sample checks the complete Run
delta through summary formatting/writing against 8 MiB. Exceeding it emits one bounded
failure row and returns 1. The summary explicitly precedes this final budget gate;
its counters and tentative exit field do not replace the required raw process exit.
No source allocation occurs after a successful final gate. This is separate from PowerShell startup, script
hash checks and Add-Type compilation, which occur before Run and are not represented
as fixture-storage measurements. No failure path certifies a completed budget verdict.

Exit 0 requires all formula, capacity, repeated coefficient, count and budget assertions
to pass. Exit 1 reports any completed numerical/storage mismatch using actual failure
counts. Exit 2 means unsupported setup or an ordinary caught exception. No abort,
FailFast, deliberate OOM, trap or crash test exists. Root must additionally require
normal exit, complete bounded output and no stderr/timeout/crash before accepting this
separate probe receipt. It must not turn incomplete output into PASS.

Before first execution, review all source, runner, pins and this README as one unit.
At the first green execution, **freeze all four whole files**, including comments,
helpers and documentation. Any later source change needs a new revision and review.
No post-green helper or parser may change this probe's meaning or confer R10 fit.
