# ClockOracle R4: independent whole-runner census — HOLD

This is a source-only census of the complete 299-line runner at `C:/Users/SM - Dan/Documents/GitHub/yes-daw/build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r4-source-prototype/run-clock.ps1`, SHA256 `B20DB319544BD879D2E132237A0518F4B2BEFAF510EF3142E2E8B57A0D33C8BF`. The exact five-file source manifest is `2D550D6DDDFF29DAC9BF589C3B23A52531A840E1BAFCC0B6AD5EA0591AF7E97B`. Nothing authored was parsed, imported, compiled, executed or bound. Only the three new census files are written.

**No numeric whole-runner upper or controlled-scratch certificate is supplied.** The complete source was inspected, including bootstrap, all 34 helpers, normal controls, child settlement, failure inventory and final serialization. Concrete unclosed terms below prevent converting the inventory into an admitted number. Old R2/R3 totals, the old 25/B+23 dependency price and the old 27,271,168-byte payload worksheet are not inherited as approved prices or bounds.

## Source-action convention

One action is charged for each reached explicit scalar variable/member/index read, literal value use, scalar initialization/write/assignment, arithmetic/comparison/boolean/type/cast operator, branch selection, call dispatch, return, throw, continue, and loop test/entry/increment. An increment has a read, increment operator and write. An index access pays its receiver/key expressions and the indexed read. Static type/function names are not additional literal operands; an actual type value on the right of `-is` is an operand. Callers pay argument evaluation and dispatch; callees pay their entry, parameter initialization and exclusive body. Nested helper bodies must be expanded once, not added twice.

For `foreach`, setup includes source evaluation, enumeration acquisition and state initialization. Each visited element pays MoveNext/test, branch selection, binding write and loop entry (four actions); the final unsuccessful test/branch costs two. For `while` and `for`, each explicit condition and increment is counted. A pipeline scriptblock invocation and its explicit body are counted for every input element, including elements later filtered away. `try`/`catch`/`finally` bodies are counted on their actual alternatives; a finite upper may take the larger alternative, with its complete argument/witness construction. Ordinary function-definition registration and all top-level literal setup remain obligations in the unfinished main-body row.

Explicit fixed-length array default initialization is a separate logical per-element obligation, consistent with the C++ source census; it is not silently priced as one scalar assignment. Hidden CLR capacity growth, method internals, SHA/UTF-8/JSON/OS primitive CPU, allocator bookkeeping and total heap/RSS remain opaque/unqualified. Method dispatch and visible receiver/argument expressions are still counted. This abstraction measures source actions, not cycles. It does not make hidden work free in a CPU/resource claim.

`Spend-Task` debits are protocol data, not prices. Charging 64 at a hash site does not prove that the site's actual source work costs 64. Its own condition, arithmetic, arguments and ledger update must be counted. Bootstrap executes before `taskRunnerLimit` is assigned and must be included in a final whole prepayment.

## Concrete closure gaps

1. **Run-root length is not admitted before its source loop/copy.** Main lines 18–20 form paths. Line 241 checks a direct-child `runs` prefix/shape, then line 242 calls `taskRoot.ToCharArray()` and visits every character. There is no preceding literal `taskRoot.Length <= 512` gate. A long single basename does not violate the shape predicate. The 512 check in `Get-TaskEntries` line 113 is later. No particular Windows/.NET maximum or future run-name operand is assumed. Thus the exact current equation contains `U_ascii(R) <= 16*R+6` and a `2*R` UTF-16 character-array payload, with `R` not bounded by the reviewed source/profile at this site. A separately pinned exact invocation profile, or an independently reviewed earlier length gate, is needed. Current all-false guards remain closed; this is a prospective whole-source closure issue, not a present execution bypass.

2. **Failure serialization retains a message without a source length bound.** In lines 214–219, `Save-TaskFailureInventory` stores the full caught `Exception.Message` in `taskReason` and then `receipt.failure_inventory_reason`. That path has no 512-character truncation; the truncation in lines 74 and 296 is a different path. `Encode-TaskJson` line 147 constructs serialized strings and a UTF-8 byte array before checking 1 MiB. No admitted primitive exception-message bound has been supplied. Therefore those pre-admission string/byte payloads and their source consumers stay HOLD. An after-allocation encoded-size gate does not by itself establish a before-allocation scratch bound. Other encode/probe sites also require a complete source-object-to-serialized-length derivation; their post-encode caps alone do not supply one.

3. **The former dependency-preparse price is incompatible with the stated source-action convention.** The complete derivation below gives a conservative proposal `49*B+44`, not `25*B+23`. A complete runner must incorporate the new row plus its caller, codecs, byte initialization, admission and retirement; subtracting/adding it to an unreviewed old total is not a whole proof.

4. **All remaining exclusive helper/main prices are intentionally unbound here.** The full invocation/loop/lifetime map below is supplied so an independent critic can assess coverage. It is not a numeric upper assembled from guessed helper operands. There is no complete `U_main` expansion and hence no legitimate numeric `runnerUpper` in this artifact. The smallest supported next step is to settle the explicit input/serialization profiles and then finish the exclusive rows against their exact successor source. No cap, threshold, budget reset or coverage change follows.

## Dependency preparse: actual literal proposal

Lines 128–140 have one text parameter. Let `B` be admitted input bytes; strict UTF-8 decoding gives at most B UTF-16 code units, so the following per-code-unit upper also bounds bytes.

| Exclusive fixed part | Actions |
|---|---:|
| Entry and parameter initialization | 2 |
| Seven literal scalar initializations (`nodes`, `depth`, `quoted`, `escape`, `unicode`, `stringUnits`, `scalar`) | 14 |
| `nodeMax = 2*[int]manifest['profile']['include_input_count']+256`: two numeric literal uses, manifest read, two key literals, two indexed reads, cast, multiply, add, write | 11 |
| Text read + ToCharArray dispatch + enumeration acquisition/state setup | 4 |
| Final unsuccessful foreach test/branch | 2 |
| Terminal four boolean reads, three `-or`, one branch | 8 |
| One possible rejecting literal/throw anywhere in the function | 2 |
| Implicit return | 1 |
| **Fixed upper** | **44** |

The worst unquoted scalar-entry visit is: quoted test 2; quote comparison/branch 4; open-brace/open-bracket chain 8; close-brace/close-bracket chain 8; comma/colon/IsWhiteSpace chain 11; `!scalar` test 3; scalar-entry writes 5; node-cap comparison/branch 4; foreach visit 4 = **49**. After the first character of an unquoted repeated `x` scalar, the five scalar-entry actions disappear: **44** actions per character. Such text reaches the complete shape loop before the downstream JSON parser rejects it. This is a reasoning witness; no parser or control was run.

Other branch classes are bounded by the same 49: opening structure <=34; closing structure <=39; separator/whitespace <=43; quote entry <=23; quoted ordinary/escaped character <=28; quoted Unicode path <=36. These character rows exclude the terminating literal/throw pair, which is paid once by the fixed row wherever rejection occurs. Short-circuit branches are respected. The proposed shape-only upper is therefore `49*B+44`, or **205,520,940** for B=4,194,304. Caller argument evaluation/dispatch and decoded/char-array/default-initialization obligations are separate rows. This proposal is UNRUN and not a whole-runner approval.

## Complete source/caller map

The rows are exclusive unless they explicitly name an inclusive equation. Their prices remain HOLD except the independently expanded shape row above. Do not sum every count in this table as though nested work were disjoint.

| Source lines | Function/body | Actual obligations and finite loop/caller information |
|---|---|---|
| 1–24 | Parameters, guards, roots, receipt | Parameter binding, validation attributes, all eight literal authorization/resource flag assignments, short-circuit guard/throw, path/string formation, every dictionary/list/default field. Root/profile term above remains open. |
| 25 | Spend-Task | Every caller's arguments/dispatch plus signed tests, remaining-budget subtraction, branch, possible throw and ledger read/add/write. No debit argument becomes an inherited price. |
| 26–29 | Assert-NoLink | GetFullPath, depth setup; <=65 reached iterations including the refusing 65th, <=64 parent transitions; all Exists/GetAttributes/bit tests and short-circuit branches. Caller path length remains separately admitted. |
| 30–37 | Get-TaskHash | Stat/length gate before fixed 65,536-byte array, owned open/length equality, each chunk's min/cast/ReadExactly/hash append/accounting, one exact EOF read, digest/hex result, both finally disposals. Caps also bound failure prefixes. |
| 38–99 | Invoke-TaskChild | Three exact names; all setup/default fields, two 4,096-byte arrays, environment clear and 8-key copy, 28/6/6 argument entries; three actual exit slots. <=7,202 polls/child, two direction visits/poll, bounded data completions, raw/stage refusal, catch prefix, cancel/reap/settlement, EOF/accounting and one rooted quarantine. |
| 101 | Reject-TaskGate | Exception construction, Data assignments, actual witness map and throw. No invented successful negative control. |
| 102–108 | Expect-TaskGate | Eight actual calls, action dispatch/catch, reason/schema checks, exactly 32 expected witness-key visits across the eight normal controls, returned actual witnesses. |
| 109 | Add-TaskJsonVisit | Every read, encode and probe visit plus signed cap gate and update. Finite visit cap is 14,388,953 bytes, not a price. |
| 110–117 | Get-TaskEntries | Queue/list/default setup, lazy iterator, every directory test, <=64 depth, stored row cap Maximum, first overflow at Maximum+1 before row append, every attributes/name/length/reparse branch, queue ownership and finally disposal. Input-path and overflow-witness payload are separate obligations. |
| 118–122 | Copy-TaskOwned | Two actual copies: 98,009-byte predecessor (2 chunks) and <=32 MiB PE (<=512 chunks); one 64 KiB array/call, source/destination ownership, initial length, before-write cap, EOF, Flush(true), both finally closes. |
| 123–127 | Mutate-TaskOwnedBytes | Two real owned mutations, offsets/ranges and original/replacement bytes (1 and 2), read/write/flush/dispose plus returned before/after/offset witnesses. |
| 128–140 | Assert-TaskDependencyJsonShape | Full body above: proposal 49*B+44; <=4 MiB, node cap 2*1119+256=2494, depth24, decoded string512. |
| 141–145 | Read-TaskBoundedBytes | Bootstrap/predecessor/dependency three calls; stat cap and opened equality precede variable byte-array allocation; one exact ReadExactly, EOF, returned ownership, finally close. Bootstrap has cap1 MiB before trusting actual441,380-byte manifest. |
| 146 | Read-TaskJson | Two calls (predecessor and dependency), Spend body, bounded owner, visit debit, strict UTF-8, byte-owner retirement before shape/parse, optional shape dispatch, JSON depth24 and returned object. |
| 147 | Encode-TaskJson | <=20 calls under 12 receipt prepayments + marker1 + failure inventory1 + fixed-point5 + final bundle write1. Every codec/Replace/newline/UTF8 call and argument; pre-size-admission payload remains open. |
| 148–150 | Get-TaskStageBytes | Before every retained nonzero child log chunk and every Write-TaskJson; top-level Maximum7 means <=7 stored rows, first overflow8; all six literal-name tests and byte-pool arithmetic. |
| 151 | Write-TaskJson | <=14 calls: receipts12, failure inventory1, bundle1. Encode, existing-length selection, GetStageBytes, complete pre-write pool test, actual WriteAllBytes. |
| 152 | Save-TaskReceipt | <=12 admitted writes; counter/refusal, complete probe serialization/UTF8 before64KiB test, visit debit and nested Write-TaskJson. Probe remains live during nested encoding. |
| 153 | Assert-TaskIdentity | Full length/hash reads, comparisons and actual expected/observed witness construction on failure, including real predecessor/PE mutations. |
| 154–163 | Assert-TaskBoundSourceInteger | Four actual normal calls before casts; untyped Value, four signed-kind alternatives, type-name/null branch, positivity and conditional Int64 upper check, both actual typed reason/witness constructions. No test execution invented. |
| 164 | Assert-TaskReservation | Normal prepayments, child-composition-related caller data and work-one-below action; complete signed/cap checks and witness. |
| 165 | Assert-TaskRetainedReservation | Initial reservation, byte-one-below action and final retained admission; complete signed/cap checks and witness. |
| 166 | Assert-TaskAttempt | Real root check plus consumed-marker normal action; boolean and actual path witness. |
| 167 | Assert-TaskPredecessor | Actual parsed predecessor96/147/source-manifest checks; hash/copy control is a different genuine gate. |
| 168 | Resolve-TaskInput | Every relative/rooted check and GetFullPath/Join-Path argument, including indexes, fences, dependency binding and final transitive rows. No equal-byte identity merging. |
| 169–181 | Assert-TaskPins | Eight actual normal fences at245,263 before/finally,270 before/finally,278 before/finally,286; nine prepaid. Twelve directory sets/fence, exact manifest plus four own sources,1118 history pins, source-name HashSet and total/count checks. |
| 182–189 | Assert-TaskFileSet | Expected ordinal HashSet once; observed set/duplicate/unknown list; second expected scan; comparisons and actual unknown/missing joins. Failure delimiters count. |
| 190–194 | Get-TaskReferences | Two calls. Traversal Maximum60; unknown cap-key refusal; hashes, aggregate152,627,224 before row admission, names/rows, exact set. Use literal traversal caps on failure, not only the final56 count. |
| 195 | Assert-TaskSameReferences | One call,56 normal old entries/index insertions and56 identity checks; failure prefix bounded by returned reference rows. |
| 196–199 | Get-TaskPEHeader | Four calls: compiler, actual oracle, copied console and GUI mutation. DOS64+header24+optional<=4096 arrays each; full size/offset/optional gates, actual reads/fields and finally close. |
| 200 | Assert-TaskConsolePE | Four actual calls, including the real GUI-owned-byte rejection. Complete signature/machine/magic/subsystem witness; mutant never launched. |
| 201 | Read-TaskChildResult | Two calls, each raw slice<=65,536 bytes BEFORE read allocation. Split+Where-Object can visit65,537 elements BEFORE33 nonempty-line gate. Then <=32 control parse/schema iterations and one summary parse/call; actual exact control IDs/reasons checked. |
| 202–206 | Get-TaskBuildArtifacts | Two traversals, Maximum4; known per-file caps32/32/4 MiB and aggregate64 MiB before row admission. Do not replace pre-admission failure prefixes with final3 count. |
| 207–213 | Assert-TaskDependencies | One call: bounded JSON plus shape; actual Source binding; two imported-module/unit checks; Includes1..1119; <=1119 normalized include dictionary lookups and exact own-header inclusion. |
| 214–220 | Save-TaskFailureInventory | At most one outer failure call. Lazy all-entry root Maximum78 stores prefix<=78 and sees first overflow79; files<=69; typed reparse fields, unknown tail explicitly incomplete. Full caught message term remains open. |
| 222–240 | Main bootstrap/admission | External manifest hash BEFORE config use; host check, two exact module imports (source only), unbound binding refusal; four strict-kind gates,1118 input-index entries,1118 include-index copies +1 actual header, source/filter/profile/retained arithmetic and own5 cap. |
| 241–250 | Main root/one-use startup | Root prefix/ASCII term, one-use check, sole-run traversal Maximum1/overflow2, first full fence, predecessor decode,56 cap entries,3 build entries, seven directory creations, marker create-new/write/finally close. |
| 251–268 | Main controls/compile | All eight controls, real copies/hash/mutation/PE witnesses; 28 argument-template iterations with three Replace calls each; before/after fence, opaque compiler child, outputs and source dependencies. |
| 269–286 | Main producer/verifier | Remaining two console children, actual source/binary before/after identities, two result readers, both56-reference inventories, normal file-set mutations, build recheck (3x3 filter callbacks), four control-file hashes,67 pre-bundle run paths, final lazy traversal and eighth fence/receipt. |
| 287–294 | Main final bundle | 63 reference/build/control artifact references +4 stage rows =67; all67 byte additions;1118+4+manifest transitive rows=1123; <=5 fixed-point encodes plus one independent final write/encode; retained pool and final bundle hash/output. |
| 295–299 | Main failure/footer | Truncated outer failure prefix, one inventory attempt, receipt attempt, explicit footer refusal and primary rethrow. No success if a footer/settlement is incomplete. |

## Derived structural envelopes, not prices

- All1,118 historical identities were freshly hashed:453,368,054 bytes/7,671 chunks. The exact current own5 contributes539,530 bytes/11 chunks. One complete hash fence therefore covers1,123 identities/453,907,584 bytes/7,682 chunks. Nine prepayments cover10,107 hash calls/4,085,168,256 bytes/69,138 chunks. They are paths, not content-deduplicated blobs.
- The twelve current namespaces contain1,002 entries/956 files. Each runner call deliberately allows entry_count+1 rows and rejects on entry_count+2. Across a conservative all-namespace prefix this gives1,014 stored rows and1,026 reached MoveNext/overflow events; one traversal's directory visits are bounded by one root plus its stored rows. The expected-file/observed-name rejection envelope is956/1014 slots. Exact observed namespace sets were checked independently, with no reparse entries.
- Include index1119 = historical1118 + the literal current header. Maximum complete identity profile1192 = history1118 +own5+69 possible run files. Successful run files68 include the bundle; failure inventory may add one. No future binding file is included in those counts.
- Per child: <=7,202 timed polls; two direction checks/poll; at most256 full4KiB retained reads plus two terminal partial reads, conservatively <=258 nonzero writes; EOF/cap/final settlement completions also need their own rows. The source's unconditional250ms wait is essential to the poll bound. Per-child raw cap1MiB does not mean33 poll callbacks.
- With <=258 log writes/child and<=14 JSON writes, GetStageBytes has a conservative788-call envelope. Each top-level traversal stores<=7 rows and sees at most8 entry attempts. This is a caller-count proposal, not an exclusive helper price.
- Reader split arrays: <=65,537 input slots per64KiB result and <=32,768 nonempty slots before the33-line gate. At AMD64 eight-byte logical references that is <=786,440 bytes for the two arrays together, plus original text, substring contents and byte owner. This is a logical slot worksheet, not a CLR array/header/capacity or total-heap certificate.
- File-set failure joins require delimiters: `2*(513*(1014+956)-2) = 2,021,216` UTF-16 bytes when both lists are nonempty. The old2,017,280 figure omitted up to3,936 delimiter bytes. Their argument names/duplicate references and failure witness construction remain separate.
- Hashes outside the nine fences must cover real copy/PE mutations, both references/build scans, control files, four stage files and the final bundle. Normal56/3 counts alone are insufficient for arbitrary accepted prefix accounting; the complete upper must respect traversal Maximum60/4, cap-key checks and the aggregate-before-row gates. No unsupported final numeric hash count is used as a whole-source price.

## Payload lifetime and retained artifacts

Logical payload phases are separate from total CLR heap/RSS. Dropping a source reference establishes logical retirement, not physical GC reclamation. New dictionaries/HashSets, implicit arrays/copies, codec temporaries and every owner listed below still need an actual phase sum; the old25MiB parent number is not copied into a certified result.

| Phase/owner | Supported fact and remaining obligation |
|---|---|
| Bootstrap | Before trusting the manifest: byte allocation<=1MiB; exact stat/open/EOF; strict UTF8 text<=2MiB; actual accepted manifest441,380 bytes. Byte owner is nulled after parsing. Decoded manifest string payload is bounded by twice raw trusted bytes, but object/header/storage layout is not certified. |
| Persistent indexes |1118 normalized input keys; include index reuses those key/value references and adds one actual header identity. Final transitive construction calls Resolve again for1118 new path results: equal bytes do not justify merging those separate objects. Manifest paths, normalized index paths and newly resolved transitive paths are distinct lifetime obligations. |
| Dependency decode |<=4MiB byte owner +<=8MiB UTF16 text; bytes retired before shape. FileStream chosen buffer64KiB is recorded separately. |
| Dependency shape |<=8MiB raw text +<=8MiB ToCharArray. The array retires before JSON object materialization; 2494 nodes and512 code units/string imply<=2,553,856 bytes of decoded string payload, not general object layout. |
| Fence traversal/set | FullName, GetFileName and GetRelativePath can create separate strings. Queue/list/returned-array/reference slots, expected and observed sets, unknown/missing lists and joined strings overlap on a rejecting path. Proposed maximum joins are2,021,216 bytes, not the old value. Exact capacities/internal Hashtable storage remain unqualified. |
| Hash/copy/PE | One explicit64KiB hash/copy buffer per active helper; exact EOF/close paths preserved. Each PE parse owns64+24+optional<=4096 byte elements. No dense/reference payload copy or full hash-file read is introduced. |
| Child/output | Two explicit4KiB read arrays; at most one rooted unsettled owner; no next child when quarantined. Log FileStreams and pending tasks remain owned through cancel/reap/settlement. Quarantined buffers can coexist with failure/finally hashing. Internal pipe/process/CLR storage is not qualified. |
| Child result |<=64KiB byte owner,<=128KiB original text,<=128KiB substring content, Split/filter reference arrays above;<=32 record parses then summary. Both parsed result objects persist; byte/text/split locals retire on helper return. |
| Main marker/bundle | Main taskMarker byte array persists after writing. Main fixed-point taskRaw can remain live while Write-TaskJson creates a separate encoded taskRaw in its local scope. These byte owners cannot be merged by equal contents. |
| Receipt encode | Save-TaskReceipt probe remains live during nested Write-TaskJson. ConvertTo-Json, Replace, concatenation and UTF8 results exist before size rejection; successful post-size caps do not alone prove all failure-side pre-allocation maxima. |
| Failure inventory |<=78 stored prefix entries,<=69 files, unknown overflow tail not certified. Unbounded-in-source caught message and its serialization block a complete explicit-payload sum. |
| Future catalogue | No value object currently exists. A proposed AMD64 logical fields worksheet is4 uint64 plus76*(size_t,uint64)=1,248 bytes; mechanical sizeof/alignment and constant-initialization/storage/lifetime remain pending with the separate C++ author/review. A new literal data file/path/hash and updated source/include/profile/retention counts are required; it is not already an included identity. |

The existing **proposed admitted-retention** equation is independently consistent:

`453368054 + 1048576 + 152627224 + 67108864 + 67108864 + 33652457 = 774914039` bytes.

That leaves105,889,801 bytes under840MiB. It is a reservation for admitted artifacts, not an opaque compiler temporary-disk peak, before-write compiler-output guarantee, CPU/heap/RSS or scratch certificate. The actual current own5 is539,530 bytes; its reservation remains1MiB. This census does not admit itself or a future binding into the oracle's historical/current identity lists.

## Result and next supported step

`runnerUpper = HOLD_UNCLOSED`, `controlledScratchUpper = HOLD_UNCLOSED`, `whole = HOLD_UNBOUND`. No parser, codec, helper, control, compiler, child or test was run, and no synthetic receipt/PASS was created. All prototype flags remain false.

First close the run-argument and failure/serialization payload profiles by explicit reviewed source or a separately exact, mechanically enforced invocation/primitive contract. Then finish all exclusive helper/main rows under the stated convention, including the corrected shape row, default arrays, registration/setup, every callback and failure retirement. An independent root/critic review must precede any literal catalogue/data binding or execution. The unchanged single2^34 cap includes the complete runner and both C++ children; this artifact supplies no fitting whole claim.
