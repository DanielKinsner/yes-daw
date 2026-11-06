# Clock-oracle resources and lifetime: explicit HOLD worksheet

## Source-operation convention

Use the accepted elementary source/API-operation research model, including library-visible loops, branch/call/setup/retirement and repeated visits. It is not instruction cycles, compiler CPU, wall time, internal primitive CPU, heap or RSS. Calling a hidden library loop one operation to make an aggregate fit is not allowed. Opaque libm/OS/allocator CPU remains separately unqualified even after source count closure.

## Finite counts supported by the current source

For rates44100/48000/96000, N is441000/480000/960000. gcd(N,153600) is600/19200/38400; residue counts are735/25/25, total785. The fraction-pair payload is12,560B if all three caches are retained, with only11,760B needed by the largest current cache. This counts pair payload, not vector capacity/allocator/ABI/RSS. A future fixed array735 with a bounded live count can avoid vector-growth ambiguity without changing fraction arithmetic, but must be reviewed as new source.

There are9*(441001+480001+960001)=16,929,027 dense entries. The old body adds2 endpoint queries and8 burst queries for each27 fixture:16,929,297 Fractions::cut calls and33,858,594 NumericAClock::frame calls. The inverse branch adds at most960000 inverse and960000 forward calls, so a conservative generation frame-call bound is34,818,594. Each port frame uses one segment upper_bound; inverse adds one prefix upper_bound, for at most35,778,594 searches/107,335,782 comparisons with at most4 map entries. Those are call/comparison counts, not a complete source-work charge.

Actual MSVC non-ranges upper_bound is in xutility7462-7486. Count halves on every iteration, so size4 requires at most3 body iterations. Get_unwrapped/iterator/distance/next/seek and predicate paths still require source accounting, not only the three comparisons. numeric661-711 uses binary GCD with shifts, countr-zero selection, swap and subtraction, not the generic Euclid loop. For positive inputs at most960000/153600, each nonterminal odd-pair step reduces their product by at least2; bit lengths20+18 give at most38 reductions plus a terminal step. Its selected countr-zero helper and dispatch remain additional pinned-helper obligations.

All nonzero cached rationals are at least1/735. First-one discovery therefore uses at most10 nextBit calls, followed by52 mantissa bits: at most62 nextBit calls per nonzero residue. Zero is exact. Mantissa conversion is exact below2^53; these fixture fractions are normal. This supports a finite scalar bound, but does not independently certify the full original2048/cache debit, std::gcd dispatch or a successor's replay/control code.

All maps enforce D<=20R. Window count is at most floor((20R-R/4)/(R/20))+1=396 per map, hence10,692 total. No eligible row is omitted. Native inverse is at most960000 records/15,360,000B. The original map-specific actual D remains to be computed and checked by the held oracle; no fabricated exact inverse row count appears here.

The old declared Budget has a maximum6,435,062,016:128,000,000 fixed +785*2048 +16,929,297*256 +10,692*512 +960000*2048. This is only its actual counter formula. It omits an implemented full independent replay and does not prove complete helper/stream/library cost. The older15,095,572,736 proposal likewise labels actual_callgraph_closed false and DENSE_CLOCK_COST_HOLD. Neither becomes a certificate here.

The old word routine performs8 out.put calls per Q word:135,432,216 byte insertions, plus up to15,360,000 for inverse, totaling150,792,216. Actual MSVC __msvc_ostream.hpp69-79 and525-540 wraps each put in stream sentry/lock/unlock/state/sputc paths. fstream forwards into file-buffer/CRT machinery. Their work and close/flush/error paths cannot be omitted inside a nominal256/cut debit. A bounded64KiB little-endian writer/reader with exact8-byte records would genuinely reduce stream-call repetition while preserving every output byte. Fixed metadata integer formatting, paths, hashes, EOF tests, control exceptions, stdlib helpers and disposal must then be counted against actual new source. No new coefficients are selected here.

## Complete work equation to close

One unreset stage reservation must include compiler-launch source helpers, full old input/toolchain before/after fences, cache/clock construction, every cut's integer product/division/cache access and both frames, every word write/read, all27 metadata/window/burst loops, every inverse/forward comparison, exact descriptor and file-set validation, all real controls, final bundle/hash/sums, error/footer/cancellation/reap/retirement and bounded quarantine. Independent production frameForMusicalTime does a second binary search on nonzero fractions, unlike the port's reused segment index; that additional actual path must be counted. No process/profile/map/window/channel reset or borrowed R4 reservation is permitted. Whole canonical source upper and remaining headroom are literal HOLD until the bodies and helper closure are available. No test is allowed to establish a missing static bound after it was exceeded.

## Identity census

R4 has96 artifact paths+147 transitive paths+its bundle=244 distinct identities. Add root audit1, these four prep docs4, six actually inspected MSVC cost headers6, four proposed authored successor files4, one compiler1, EXE/OBJ2, original56 clock/inverse files56 and five combined stage files5:323. The five stage files are consumed.json, combined stdout.log, combined stderr.log, receipt.json and bundle.json. All paths remain separate even where bytes match. This323 is the known projected inventory before additional actual linker, frontend, runtime, SDK, helper headers, compiler environment/response/dependency files or required byproducts. Compiler path/hash is not newly verified by reading an old receipt. No finite320/384 profile is selected.

The old256 guard is immutable and private to DefinitionR4. Its8MiB complete-input region also cannot absorb the new69,798,868B predecessor run tree. A new stage must explicitly classify/hash that exact immutable predecessor-data pool separately from source/tool inputs while counting both in whole retention and whole hash work. This does not omit its files or increase the original future840Mi envelope. A derived future cardinality and helper bound needs its own reviewed source profile. Container packing could reduce new physical paths but retains56 logical member identities and all244 old paths; it adds parser/offset/hash/replay costs and is not implemented.

## Retained bytes

The original56-file caps are27 Q tables135,432,216B +native inverse15,360,000B +28 metadata/descriptor caps*65536=1,835,008B:152,627,224B. All27 metadata cap entries and the missing native-inverse JSON descriptor are preserved. The parent R4 union is74,499,159B, including historical source/interpreter inputs; root audit adds1,719B. Do not add its70,024,440B defined source/run pool again: it is already a subset of this union.

A conservative conditional future worksheet is:

| Distinct pool | Bytes |
|---|---:|
| Complete old R4 physical union | 74,499,159 |
| Root review | 1,719 |
| Four prep sources, reviewed ceiling | 1,048,576 |
| New four-source unit, proposed ceiling | 1,048,576 |
| All56 new reference-file caps | 152,627,224 |
| All compiler objects/binaries, inherited envelope | 67,108,864 |
| All stage logs/receipts/transfer overlap | 67,108,864 |
| One unexpected-failure quarantine | 134,217,728 |
| Subtotal before all newly pinned tools/headers/runtime files | 497,660,710 |
| Remaining within existing840MiB/880,803,840B | 383,143,130 |

All new external tool/header/runtime bytes, including the six inspected cost headers, must be enumerated and fit that remaining383,143,130B. Current total compiler/tool closure is unknown. The64Mi compiler output envelope is an inherited reservation, not a source proof that a compiler cannot write past it; complete allowed byproducts, temporary-file lifetimes and pre-write enforcement are HOLD. An after-run directory size check cannot rescue an overshoot. These conditional envelopes do not admit the stage or change production resource limits. No PCM outputs or physical controls are present.

## Controlled lifetime and failure ownership

The oracle must stream Q and inverse records. It must never allocate a vector of16,929,027 entries or all inverse records. Proposed active payload: fixed fraction cache<=12,560B; two64KiB record buffers; one<=64KiB metadata buffer; bounded small maps/clocks; <=256KiB total control mutation/report storage; and <=2MiB launcher-owned raw-read/PE/receipt/fence state. Exact actual sizeof/capacity/allocator/stream/format/helper lifetimes must be reviewed; internal library/compiler heap and RSS remain unqualified. The existing32MiB controlled scratch ceiling is retained, with no unsupported all-process RSS claim.

Before one use, prepay the whole ledger/retention pools and create an exclusive consumed marker. Before and after compiler and both console modes, hash exact own source/manifest, all244 old paths, every new toolchain input and generated-output subset; final fence requires exactly56 references and complete EXE/OBJ/stage closure. Use fixed child/storage ownership from process construction onward inside try/finally. Preserve raw exit and launch/read failures; use CreateNoWindow/hidden console processes, actual bounded PE parsing before launch, bounded wall timeout and bounded kill/reap/settlement. Outstanding async reads keep their storage rooted and quarantine blocks the next child; no unbounded WaitForExit, abandoned read owner, swallowed raw error or retry is permitted. Logs have pre-write caps; manual checked dictionary-key sums count every retained byte. A failed or absent step cannot be relabeled PASS. All these remain future source obligations, not executed checks.
