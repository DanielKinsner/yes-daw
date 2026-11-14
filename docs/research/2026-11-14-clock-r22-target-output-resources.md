# Clock R22 target file and diagnostic bounds

The R22 target source is byte-identical to the independently reviewed
289-line [Clock target](artifacts/2026-11-06-native-own-object-independent-review-r1/sources/048-clock_stage.cpp),
SHA-256 `F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206`.
The R22 `source-costs.h`, reached `independent_clock.h` and `Time.h`
also matched their archived source byte for byte. The admission guards
are false at lines 22–24 and 279–283. The write schedule below applies
to prospective admitted invocations. The checks read source and
measurement records and used integer arithmetic. No target, compiler
or linker ran.

## Source-controlled output

| Target mode and sink | Upper | Source |
| --- | ---: | --- |
| Producer Q files | 135,432,216 bytes across 27 files | Lines 247–252 and 257. For each of 44,100, 48,000 and 96,000 Hz, each of nine maps writes `8 * (10 * R + 1)` bytes. |
| Producer inverse binary | 15,360,000 bytes across one file | Lines 251 and 260–267. The only inverse is 48,000 Hz, map 5. The duration gate gives `0 < D <= 20 * 48000` and the inverse writes two 8-byte words per frame. |
| Producer metadata and inverse descriptor | 1,835,008 bytes across 28 files | Lines 109–115, 143, 257 and 260. One text per fixture and one inverse descriptor. Each Text and write is capped at 65,536 bytes. |
| Producer reference total | 152,627,224 bytes across 56 files | Sum of the three preceding rows. It equals the runner's reference-pool cap at line 1079. |
| Verifier control files | 16 bytes across two files | Lines 253–257. The first reached word control writes 7 and 9 bytes once. All 56 reference opens in verify are read-only. |

The writing constructor uses exclusive, binary creation at lines
122–126. Both byte-write paths admit the cumulative length before a
copy, including the direct 8-byte fast path. The only file payload
submission is `fwrite(..., 1, at, file)`, where `at <= 65536`.
Every intermediate flush sends a full buffer and the final flush sends
any remainder. The upper is 2,342 source `fwrite` calls in produce and
two in verify, by summing per-file ceiling divisions by 65,536. A short
`fwrite` immediately rejects. Failure can leave a partial output file.
The source figures bound binary payload submissions and the prospective
reference extent. They supply no filesystem allocation, cache or reported
physical position on an I/O error. [Microsoft fwrite contract](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fwrite?view=msvc-170)

The five formatted output sites are lines 43, 238, 269 and the two catch
arms at 289. Using 20 characters per unsigned 64-bit value and 11 per
signed 32-bit value gives these narrow-character maxima.

| Output | Source maximum including LF |
| --- | ---: |
| Producer stdout | 1,060 characters across at most four records |
| Verifier stdout | 2,650 characters across at most 12 records |
| Terminal stderr, including cleanup report | 780 characters across two records |

The summary is 427 characters in either mode using the fixed status
literal and exact 64-character manifest. The 11 control lines have
maxima of 206, 212, 215, 188, 207,
206, 203, 195, 184, 202 and 205 in source order. The first three are
common. Each name and matched rejection reason comes from a literal
at its call site. The verifier's five control flags limit the remaining
eight records to one each. Early failure can emit a prefix followed by
one catch report and one cleanup report. Each catch caps its interpolated
message at 512 characters. ANSI text-mode CRLF translation increases
the stdout maxima to 1,064 and 2,662 bytes under that profile. The same
profile gives a conservative stderr upper of 1,294 bytes allowing
512 extra CRs in the interpolated message and two in the fixed
terminators. [Microsoft CRT translation modes](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/setmode?view=msvc-170)

The reached project bodies contain no process-creation action. Their
only external I/O sites are the file opens, reads, writes, checks and
closes in Stream and the fixed standard-output and error reports above.
The compiler invocation is a separate child of the supervisor. The
target source proof does not establish a closed import set for a future
linked image or bound loader, CRT or crash output. The runner's 65,536
summary reader and the supervisor's shared 1 MiB log and stage
admission checks remain necessary. [Supervisor protocol](../adr/0090-clock-research-owned-process-supervisor.md)

## Target owner lifetimes

- The [accepted object review](artifacts/2026-11-06-native-own-object-independent-review-r1/sources/039-review.md#whole-declaration-coverage-and-object-interpretation)
  carries two 65,584-byte Streams, four 65,544-byte Texts and one
  11,784-byte FractionCache, totaling **405,128 bytes**. A second
  Stream object can coexist with `q` after `q.finish` at line 257,
  but the file reference is detached before metadata I/O or the inverse.
  The inverse descriptor helper ends before `inv` starts at line 260.
  The only two simultaneously open application stream owners are
  possible at the verifier's short/trailing controls before
  `q.finish`, with one helper at a time. With failed `_fdopen` or
  `setvbuf`, its cleanup chain runs before the exception stops the
  attempt. If an expected control's cleanup fails, line 238 rejects
  instead of opening the next helper. A failed close remains unresolved
  native ownership. The three standard streams are separate.
- The [30-layout receipt](artifacts/2026-11-14-clock-layout-joins/result.json)
  measures 32 bytes for the per-fixture `get` closure and 40 bytes for
  the largest by-value action, `q-off-by-one`. At line 253 both can
  be live, giving **72 bytes** for the selected own closures in that
  scope. The short/trailing operations occur in later expressions
  with 8-byte actions. The remaining action, comparator and SDK
  lifetimes need their own contexts. This term excludes the referenced
  objects, the operator formals and the 48-byte witness passed to `expect`.
- Under the [pinned xstring backing model](artifacts/2026-11-06-clock-native-resource-boundary-r1/sources/002-argument.md#strings-and-input-extent),
  the largest live path expression can retain the root and manifest
  allocations (528 + 80), both fixed inverse names (2 × 48), and
  both root-concat requests (528 + 791) during append growth. The
  **2,023-byte** string component counts both growth buffers. Regular
  fixture-relative concatenation needs at most 32 + 48 in place of
  the two 48-byte inverse names. Short and trailing paths occur in
  distinct full expressions and have one request each. This is a live
  payload component only. The older 78,888/81,064 cumulative producer
  and verifier requests remain available with no physical free credit.

## Conditional CRT payloads identified in local SDK source

The installed Windows SDK 10.0.26100.0 source gives source-level
formulas for additional payloads. Applying them to a future linked
target requires matching its actual imports and deployed UCRT behavior
to this source.

- `stdio/fopen.cpp` lines 22–60 and 89–95 call
  `stdio/openfile.cpp` lines 19–48, which selects `_sopen_s` for
  narrow paths. The writer calls `_sopen_s` directly.
  `lowio/open.cpp` lines 833–852 allocate one initially empty wide-path
  buffer before `_wsopen_nolock`. The buffer performs a sizing call,
  requests exactly `units * sizeof(wchar_t)`, converts and then retires
  when that call returns. There is no second allocation or conversion
  retry in this branch. `_fdopen` starts later and receives the
  descriptor and fixed mode, with no original path argument.
  Actual source paths have at most `512 + 1 + 42 = 555` characters.
  Under a one-unit-per-printable-ASCII conversion profile this gives
  one **1,112-byte** wide-path request live at a time, including NUL.
  The Q path needs at most 1,096 bytes. The verifier's short and trailing
  paths need at most 1,070 and 1,076 bytes per request, twice each.
  Summing requests without reuse gives **61,840 producer / 66,132 verifier
  bytes**. This leaves any OS-side normalization and selected code-page
  behavior to the exact runtime profile. [Win32 conversion sizing](https://learn.microsoft.com/en-us/windows/win32/api/stringapiset/nf-stringapiset-multibytetowidechar)
- `stdio/setvbuf.cpp` lines 70–79 use the FILE's two-byte embedded
  buffer for `_IONBF`. This branch makes no user-requested stream
  backing allocation. The target's captured stdout is separately
  eligible for ordinary buffering in `stdio/_flsbuf.cpp` lines
  169–181 and `stdio/_getbuf.cpp` lines 15–44. Stderr is eligible
  for a retained temporary output buffer in `stdio/_sftbuf.cpp` lines
  34–114. These supply two possible **4,096-byte** buffers. The selected
  ANSI pipe branch in `lowio/write.cpp` lines 404–459 can add one
  **5,120-byte** LF translation array. The narrow formatter uses an
  embedded **1,024-byte** conversion buffer. The selected formats are
  narrow strings and default precision integers, so their integer path
  does not request the dynamic precision buffer.

Key local UCRT source SHA-256s for that association work are
`lowio/open.cpp` = `D6AABCD41D36558903B6110C3C99BF2ECC8B05F5C5C515B174D6A1BD9CC77AB8`,
`inc/corecrt_internal_win32_buffer.h` = `52407763A7F753D61665203FF775CF199F2D50B1BA1E44F9CE4C3A376BDA7EB5`,
`lowio/write.cpp` = `993B67351B92E713B16B330B5F6606FC28BD19CBA751A779CA87C0C7A3CA903B`
and `inc/corecrt_internal_stdio_output.h` =
`36B1877834F52965F2F3793A8EB42E4FF00A83B9616913E8583183BB3E411222`.

Final target image and runtime association, remaining controlled
argument/environment and exception-message copies, the full selected
object and CRT call joins, and the controller/helper/target settlement
join are still open. The arithmetic above gives neither a physical
frame bound nor a joined 32 MiB certificate.
