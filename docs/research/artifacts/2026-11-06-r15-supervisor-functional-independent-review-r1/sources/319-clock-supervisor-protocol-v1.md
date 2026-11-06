# Clock supervisor protocol v1

Status: Accepted architecture/protocol R2 under ADR-0090 after independent whole review; UNIMPLEMENTED and UNRUN. Only source authoring is authorized by the source plan. This supersedes only the unimplemented Proposed R1 protocol; preserved R1/R2 subjects, critics and R12/R13 sources remain frozen. No implementation, price, ABI, native association, resource fit or execution gate is accepted.

All binary integers are unsigned little-endian. A byte is eight bits; a UTF16 cell is exactly sixteen bits. Encode/decode individual cells; never cast a file buffer to a C++ struct or serialize padding. Unavailable fixed fields contain zero and have their observation flag clear; zero without its flag is not evidence. No raw native error text, environment enumeration or arbitrary extension field is permitted.

## 1. Owned names, current identity and admitted inputs

The existing child indices/names remain 0=`one-opaque-compile`, 1=`oracle-produce`, 2=`independent-full-verify`. Target argument counts are respectively 28/6/6. Target environment values remain exactly SystemRoot, windir, PATH, INCLUDE, LIB, TEMP, TMP, VSLANG, including the existing `<OWNED_OPAQUE_TEMP>` substitution before encoding. Arguments and these environment values retain the existing ASCII32..126, length<=512 guard; empty strings remain admitted where the predecessor admits them. Paths retain the corresponding existing valid-UTF16, length, canonical-root, filesystem, reparse and exact namespace guards. No blanket ASCII path restriction or new quiet-output condition is introduced.

The new owned directory is `<run-root>/transport`, a sibling of stage. For child i its only files are `child-i.request.bin` and `child-i.terminal.bin`, with i rendered as one ASCII digit0/1/2. All use no-overwrite creation. Partial files are preserved, not replaced or retried. Raw output stays in the existing stage/stdout.log and stage/stderr.log. Every new path, directory, file, partial tail and helper source/binary enters the successor manifest, namespace, complete fences, scratch and retained sums. None is opaque compiler output. A full prefix has six transport files and one directory; no extra progress or commit file is introduced.

The parent knows the exact current manifest digest, target identity and canonical run root before creating the request. A canonical run root here means the already admitted full path used as the current working directory; the protocol does not globally replace predecessor path normalization or its unknown/tilde fallback. The parent and helper use the same exact UTF16 cells for the identity, without Unicode normalization, case folding or slash substitution.

Run identity bytes are, in order: ASCII `YESDAW-CLOCK-SUPERVISOR-RUN-V1`, one zero byte, the32 manifest-SHA bytes, u32 code-unit length of that canonical run-root string, then each of its UTF16 cells as u16 LE. The run identity is SHA256 of exactly those bytes. The helper recomputes it from the validated working-directory field and manifest digest; the parent independently recomputes its expected value from its current admitted root. This is a domain-separated label, not proof of loaded-image identity. The parent additionally checks every path/value/argument against its current request recipe, and preserves original target console-PE and before/after binary/input fences. Helper identity is an additional pin, not a substitute.

Working directory equals that run-root string. Stage and transport are the admitted stage and transport children of the same root; no caller-selected other directory is accepted. All actual filesystem checks remain current, including parents/reparse state; this identity recipe caches no filesystem trust. Target identity is the current exact target SHA supplied by the existing compiler/native-target recipe. The parent validates current identities for both files and every terminal. All SHA fields are32 raw bytes, never64 textual cells in this format.

## 2. Request layout

Magic is the eight ASCII bytes `YCSREQ01`; version=1. The fixed header is140 bytes:

| Field | Offset | Bytes |
|---|---:|---:|
| magic | 0 | 8 |
| version | 8 | 4 |
| totalBytes | 12 | 4 |
| childIndex | 16 | 4 |
| argumentCount | 20 | 4 |
| environmentCount | 24 | 4 |
| helperReservation | 28 | 8 |
| mandatoryTailReservation | 36 | 8 |
| manifestSHA | 44 | 32 |
| runSHA | 76 | 32 |
| targetSHA | 108 | 32 |

Following the header are four strings: target path, working directory, stage root, transport root; then the eight environment values in the order in section1; then the exact target arguments. Each string is u32 code-unit length followed by that many u16 cells, with no terminating cell. Its code-unit length is<=512. The compiler has40 strings, giving140+40*(4+2*512)=41,260 bytes maximum. Each native target has18 strings, giving18,644 bytes maximum. The full three-request/three-terminal transport maximum is41,260+2*18,644+3*308=79,472 bytes, including partial files within those same per-file caps. These are format byte uppers, not managed/native allocation or fit claims.

Admission order is fixed: bounded raw command line -> admitted request path/current owned file -> file length between140 and the correct derived cap -> fixed-size bounded capture -> exact magic/version/header -> childIndex0..2 and exact argc/environmentCount8 -> positive same-origin reservations and identities -> bounded strings using subtraction-based remaining-byte checks -> exact EOF -> field-specific existing guards/current root recipe -> run identity -> current source/binary/fence/owner/profile gates. Length multiplication/addition is checked before access or allocation; count/length is not a request to allocate an arbitrary array. `helperReservation>=mandatoryTailReservation>0`; prefixLimit is their exact subtraction. Both values must equal the fresh reviewed same-origin helper slice in the current manifest/binding. No request-supplied allowance overrides that binding.

Before a valid request/current identity exists, no target is created and no purported valid terminal is fabricated with unknown identity bytes. Exit10 represents that unavailable transport outcome. The parent still retains the full helper reservation and records unknown/quarantine.

## 3. Request-path command line and process environments

The parent supplies the pinned absolute supervisor path as ProcessStartInfo.FileName and exactly one ArgumentList entry, `--request16=` followed by four uppercase hex digits per decoded UTF16 cell of the absolute request path. No source-produced literal quote is included in FileName. The selected SDK BuildCommandLine/PasteArguments correspondence must produce this exact raw GetCommandLineW grammar:

`"<absolute supervisor path>" --request16=<HEX>` followed by one NUL.

There is one U+0020 between the closing executable quote and prefix; no leading/trailing whitespace, other arguments or embedded raw NUL is admitted. The executable token contains1..512 valid UTF16 cells and no quote/NUL; it equals the admitted pinned helper path under the same expected-host comparison. The prefix is exactly12 ASCII cells. HEX contains4*n uppercase0..9/A..F cells for1<=n<=512. Decoded cells must form a valid UTF16 path with no NUL; no normalization is performed by the hex decoder. Current full-path/root/file guards then apply. The encoded argument is at most2,060 cells; the raw command line is at most2+512+1+12+2,048+1=2,576 cells including NUL. The bounded direct parser scans no farther than that extent and rejects missing NUL/quote, malformed groups, invalid decoded cells, extra argument or unexpected executable representation before opening the request. It uses neither CRT argv expansion nor ambient environment discovery as an unpriced parser. Entry/CRT effects still require source/import/resource accounting. The raw grammar's selected Windows/SDK association remains an execution premise until separately qualified; no universal OS rewriting guarantee is invented.

The parent explicitly clears the supervisor ProcessStartInfo.Environment and supplies exactly the same eight expanded values used for the target; it does not inherit the controller environment. Its Environment getter/capture/clear/dictionary/block costs remain counted, including the external controller-N premise; no current values are dumped. The supervisor has no redirected standard streams, never reads ambient stdin and never writes stdout/stderr. The trusted fresh launcher must separately establish the supervisor's noninteractive/null-stdin and error-dialog profile. No claim that CREATE_NO_WINDOW alone creates that premise is made.

The target receives its own explicitly opened inheritable `CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, inheritableSecurityAttributes, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)` read handle, not the supervisor's ambient stdin or an EOF pipe. That NUL handle, two child pipe writers, their closes/errors and all native owners are counted. The exact target inherited-handle list contains those three handles only; the private job, parent pipe readers, log files, request, terminal, thread and synchronization handles are noninheritable and excluded. Source uses no GUI/audio/MIDI/foreground/input API; the specified NUL/pipe/file I/O is the admitted Win32 I/O, not a blanket no-device-API assertion.

The native target environment block is built solely from the eight request values. Its fixed key order is INCLUDE, LIB, PATH, SystemRoot, TEMP, TMP, VSLANG, windir, the same OrdinalIgnoreCase ordering of those fixed keys. Each entry is the UTF16 cells of key, `=`, exact value and NUL; one additional NUL terminates the block. Empty values are retained as `key=` entries. `CREATE_UNICODE_ENVIRONMENT` is required and lpEnvironment is never null. No extra variables, inherited block, code-page conversion, expansion or normalization is performed. The keys have43 cells in total; with eight512-cell values, eight `=` cells, eight entry NULs and the final NUL, the maximum is4,156 cells/8,312 bytes. The parent helper block uses the same exact values and sorting, through its separately source-qualified SDK construction.

For target launch use the exact admitted target as lpApplicationName and a mutable command line consisting of that same path enclosed in quotes, then one U+0020 before each encoded argument. An empty argument is `""`. A nonempty argument with neither U+0020 nor a quote is copied as-is; the admitted ASCII domain contains no tab/newline. Otherwise enclose it in quotes: a run of n backslashes immediately before a literal quote becomes2*n+1 backslashes followed by that quote; a run at the end becomes2*n backslashes before the added closing quote; other backslashes/cells are copied. A quote with no preceding backslash run is the n=0 case. This is the normative predecessor ArgumentList/PasteArguments algorithm for the admitted domain, not shell escaping. Freeze its exact source correspondence before execution. The resulting target command line is bounded by512+2+argc*(2*512+3)+1 cells including NUL:29,271 compiler or6,677 native cells. No arbitrary native argv/environment parser substitutes for these rules.

## 4. Terminal layout

Magic is `YCSRES01`; version=1; totalBytes=308. All fields are encoded individually. The fixed size is8+16*4+3*8+3*32+2*7*8+4=308, with no alignment or trailing padding.

After magic are sixteen u32 fields in this order: version, totalBytes, childIndex, flags, targetPid, targetRawExit, status, primarySite, primaryReason, primaryNativeError, firstSecondarySite, firstSecondaryReason, firstSecondaryOwnNativeError, firstNativeSecondarySite, firstNativeSecondaryReason, firstNativeSecondaryError. These occupy offsets8..71.

Then are u64 secondaryFailureCount at72, sourcePrefixUsed at80 and bodyPolls at88. The three32-byte manifest/run/target identities occupy96..191. Stdout then stderr each have seven u64 fields: observed, written, omitted, unretained, startPosition, endPosition, errorPartial. Stdout occupies192..247; stderr248..303. Finally cleanupMask is u32 at304. Exact EOF follows byte307. Reservation values are cross-checked against the retained current request, not silently inferred from a terminal label.

## 5. Flags and availability

| Bit | Name | Meaning when set |
|---:|---|---|
| 0 | ChildCreated | A valid returned target process identity/owner was observed, including a suspended target. |
| 1 | TargetExitKnown | Target process was actually signaled and its32-bit exit value was observed. |
| 2 | StdoutSettled | Its worker exit was observed, or it was conclusively never started. |
| 3 | StderrSettled | Same for stderr. |
| 4 | StdoutEof | Its zero-EOF logical result was consumed. |
| 5 | StderrEof | Same for stderr. |
| 6 | JobEmptyProved | A valid current empty-job proof was obtained, or no job/target ever existed by the reached owner state. |
| 7 | StdoutStartKnown | startPosition is an observed owned-handle position. |
| 8 | StdoutEndKnown | endPosition is an observed final owned-handle position. |
| 9 | StderrStartKnown | Same start observation for stderr. |
| 10 | StderrEndKnown | Same end observation for stderr. |
| 11 | PrefixKnown | sourcePrefixUsed is the valid frozen prefix-meter observation at section10's cut. |
| 12 | PrimaryNativeKnown | primaryNativeError was captured at its documented failing API. |
| 13 | HasSecondary | At least one secondary failure/diagnostic was recorded. |
| 14 | FirstSecondaryOwnNativeKnown | The first secondary itself has the native code in firstSecondaryOwnNativeError. |
| 15 | FirstNativeSecondaryKnown | The first native secondary has the dedicated site/reason/error triple. |
| 16 | StdoutErrorPartialKnown | Its final errorPartial count is frozen and known. |
| 17 | StderrErrorPartialKnown | Same for stderr. |
| 18 | BodyTimedOut | The elapsed target-body deadline was the refusing branch. |
| 19 | BodyPollLimited | The target-body poll-admission limit was the refusing branch. |
| 20 | RawLogCap | The shared1MiB raw-log cap refused a logical chunk. |
| 21 | StagePoolCap | The complete64MiB stage pool refused a logical chunk. |
| 22 | StageGuardRejected | A non-cap current stage name/reparse/depth/extent/I/O guard refused. |
| 23 | SecondaryCountSaturated | Another secondary occurred after UINT64_MAX was reached; count is then a lower bound. |
| 24 | StdoutFlushed | Its prefix flush completed successfully. |
| 25 | StderrFlushed | Same for stderr. |
| 26 | TargetMayHaveRun | ResumeThread was attempted; set before that call, not inferred from PID alone. |
| 27 | CoordinatorCountersKnown | All eight observed/written/omitted/unretained fields and bodyPolls are a frozen, valid coordinator prefix. |

Bits28..31 are invalid; the allowed mask is0x0FFFFFFF. All non-observed numeric positions/error codes/partial/source fields are canonical zero. If CoordinatorCountersKnown is clear, bodyPolls and all eight aggregate byte counters are zero/unavailable and their equalities are not used as evidence. TargetPid is zero if ChildCreated is clear; a set ChildCreated requires a nonzero observed PID and a reached valid owner state. TargetExitKnown implies ChildCreated. TargetMayHaveRun implies ChildCreated. No target exit is inferred from a numeric259/zero, helper exit, close, termination request or request label.

EOF and worker settlement are independent observations: an EOF result can be consumed before the worker's exit is observed. A never-started worker may be settled without EOF. Settled workers have known errorPartial; normal EOF has errorPartial0. A live worker's mutable partial count is not read: its partial-known bit is clear and field zero, while coordinator-owned observed/written/omitted/unretained remain stable known-prefix counters. TargetExitKnown may remain valid even when another owner is unresolved. JobEmptyProved is a proposition with actual proof, not a numeric active-count default.

## 6. Status, sites and reasons

Status codes are0 settled target success;1 settled target nonzero;2 pre-execution refusal;3 recorded body/cleanup failure with settled owners;4 unresolved/quarantine. Helper exit codes are separate (section11).

Sites are0 None,1 Request,2 Containment,3 Spawn,4 Read,5 StageAdmission,6 Write,7 Deadline,8 TargetExit,9 WorkerSettlement,10 JobSettlement,11 TerminalPublication,12 Meter,13 OwnedClose. Reasons are the following fixed enumeration:

| Value | Reason | Allowed site(s) |
|---:|---|---|
| 0 | None | 0 |
| 1 | RequestMagic | 1 |
| 2 | RequestVersion | 1 |
| 3 | RequestLengthOrEof | 1 |
| 4 | RequestCountBinding | 1 |
| 5 | StringGuard | 1 |
| 6 | PathGrammar | 1 |
| 7 | RunIdentity | 1 |
| 8 | CurrentIdentity | 1 |
| 9 | ReservationBinding | 1 |
| 10 | RequestIo | 1 |
| 11 | UnsupportedJobProfile | 2 |
| 12 | JobSetup | 2 |
| 13 | CreateProcessFailure | 3 |
| 14 | ResumeFailure | 3 |
| 15 | ReadFailure | 4 |
| 16 | RawLogCap | 5 |
| 17 | StagePoolCap | 5 |
| 18 | StageUnknownName | 5 |
| 19 | StageReparse | 5 |
| 20 | StageDepth | 5 |
| 21 | StageExtent | 5 |
| 22 | StageIo | 5 |
| 23 | WriteFailure | 6 |
| 24 | ZeroWriteProgress | 6 |
| 25 | BodyTimeout | 7 |
| 26 | PollLimit | 7 |
| 27 | TargetExitUnobserved | 8 |
| 28 | TargetExitQueryFailure | 8 |
| 29 | WorkerJoinUnobserved | 9 |
| 30 | WorkerJoinFailure | 9 |
| 31 | JobNotEmpty | 10 |
| 32 | JobQueryFailure | 10 |
| 33 | FlushFailure | 6 |
| 34 | PositionFailure | 6 |
| 35 | CloseFailure | 13 |
| 36 | PublicationFailure | 11 |
| 37 | SourcePrefixLimit | 12 |
| 38 | MeterOverflow | 12 |
| 39 | CounterOverflow | 4,6 |
| 40 | CancelledReadDiagnostic | 4 |
| 41 | SourceInvariant | 4 |
| 42 | ThreadSetupFailure | 3 |
| 43 | StdinNulSetupFailure | 2 |
| 44 | NativeOwnerAllocationFailure | 2 |
| 45 | OwnedPathFailure | 1,5 |
| 46 | HandleInvariant | 2 |
| 47 | LateTailSourceInvariant | 12 |

No unlisted pair is admitted. Semantic refusals use their site/reason with native-known clear and native code0. Native-known is set only for an actually captured documented failing-API value, including a genuinely observed value0; it never causes status success. Site/reason0 occur together and have no native-known flag. A completed nonzero target with no other failure is status1, primary None; parent later raises the existing actual-child-nonzero failure after recording the raw value.

The first primary is immutable. Every later failure/diagnostic increments secondaryFailureCount with saturation, never wrap. The first secondary site/reason and its own optional native code are immutable. Separately retain the first native secondary triple, even if an earlier secondary was semantic. HasSecondary iff count>0; saturated implies count=UINT64_MAX and HasSecondary. FirstSecondaryOwnNativeKnown implies HasSecondary and FirstNativeSecondaryKnown; then the dedicated first-native triple equals the first-secondary triple. With no native secondary, its three fields are zero. Original controller primary/first-secondary ownership is not replaced by this helper record.

Status0/1 require ChildCreated, TargetMayHaveRun, TargetExitKnown, both settled/EOF/partial-known flags, JobEmptyProved, all four position flags, both flush flags, PrefixKnown and CoordinatorCountersKnown. Both errorPartial values are0; all written positions and counters are consistent; no primary, secondary, cleanup mask or refusal flag is present. Raw exit is0 for status0 and nonzero for status1. Status2 requires TargetMayHaveRun clear, a nonzero primary site/reason, settled workers, complete reached-owner cleanup/job proof, PrefixKnown and CoordinatorCountersKnown; a suspended created target may have been terminated before any resume attempt. Status3 requires a primary or nonempty secondary/cleanup diagnostic, TargetMayHaveRun set, known target exit, settled workers, job proof, PrefixKnown and CoordinatorCountersKnown; EOF/position/flush may be absent only with the corresponding recorded failure. Status4 applies whenever an owner/counter/proof needed to assert settlement remains unavailable; it requires a nonzero primary or secondary/cleanup quarantine diagnostic and never authorizes another child.

BodyTimedOut and BodyPollLimited are mutually exclusive and correspond to reason25 or26 in primary/secondary records. RawLogCap and StagePoolCap are mutually exclusive and correspond to reason16/17; either sets the old parent log_cap. StageGuardRejected corresponds to reasons18..22 and is not a cap or omission by itself. A later cleanup diagnostic cannot overwrite the original reason. No status0/1 can carry these refusal flags. All record arithmetic is checked before admission; unknown never passes an equality solely because two zero fields match.

## 7. Read, write and partial-progress accounting

Each worker has one4096-byte buffer and one immutable ready record per generation. Its states are NotStarted -> Reading -> Ready -> AwaitAck -> Reading, or terminal ReadyEof/ReadyError -> AwaitAck -> Exited. The coordinator owns ready consumption and all published aggregate counters. It acknowledges only after all buffer uses end, with Continue or Stop. Native synchronization/visibility and actual field/ABI ownership require independent source/native review; no unsynchronized read of a live worker counter is permitted.

Positive native reads fill the current buffer. A full buffer publishes DataFull4096. Native EOF with a nonempty partial buffer publishes DataEofShortN; after Continue acknowledgement the worker publishes a logical zero-EOF result without a new native read. Native EOF with an empty buffer publishes zero-EOF directly. ReadFile broken-pipe EOF is distinguished from other documented errors. A short positive read or target exit alone is not EOF. On a non-EOF read error, the worker publishes ReadError and the known partial bytes accumulated since its last acknowledged chunk; those bytes are retained in the fixed buffer but never silently counted as a successful logical chunk or written. Their number is errorPartial0..4095, known only from a frozen published error/settled worker state. This matches the predecessor's distinction between successful ReadAtLeast results and an errored incomplete result while preserving the additional native-prefix evidence privately.

Consuming each successful logical DataFull/DataEofShort result adds its N exactly once to observed and unretained. Consuming zero-EOF adds zero and records logical EOF. During the body, a chunk that passes complete admission may be written. During cleanup, a successful but previously unconsumed ready result is accounted once as observed/unretained and is not written or marked omitted. Stop acknowledgement permits no next read. A settled read error is recorded as a diagnostic, including cancellation; its errorPartial is separate from observed. A body read error may be re-observed during cleanup as the predecessor's settled-read diagnostic, without adding its bytes twice or pretending a second native API failed.

Before writing a new logical chunk, refuse the whole chunk if N exceeds the remaining shared1MiB confirmed-write budget, setting RawLogCap and adding N to omitted. Otherwise perform the complete current stage scan. A stage aggregate-cap refusal sets StagePoolCap and adds N to omitted. A non-cap name/reparse/depth/extent/I/O guard sets StageGuardRejected, without increasing omitted. All remain unretained. No whole1MiB reservation is substituted for the per-write check.

Only the coordinator appends. For each actual WriteFile, including a short-write continuation, repeat the complete stage scan/check and check the remaining logical chunk before that write. On true return with positive confirmed count<=requested, increase written and decrease unretained by exactly that count. On true zero progress with bytes still required, fail with reason24; never spin. On false return do not use an undefined bytes-written output or infer physical progress; confirmed-written stays unchanged for that call. Stop further writes and retain known/unknown evidence. These more precise confirmed native increments are explicitly the successor's confirmed-write measure; they do not claim the predecessor could observe hidden internal partial writes.

When CoordinatorCountersKnown is set, for each pipe observed=written+unretained and omitted<=unretained; bodyPolls<=7200. Aggregate observed plus all known errorPartial values is<=1,048,576+2*4,096=1,056,768; an unobserved live partial is never inferred to be zero. Physical end-start is independently known only when both corresponding position flags are set and end>=start. A failed write may make physical bytes differ from confirmed written. Normal status0/1 requires physical delta=written=observed, omitted/unretained/errorPartial0. ErrorPartial is not smuggled into the old serialized successful-read observed counter. All counters are coordinator-stable prefix values at record freeze. Byte/poll/generation additions are checked and never wrap; a counter failure records a noncertifying reason and clears CoordinatorCountersKnown if exact aggregate availability cannot be retained. Canonical zero unavailable values then prove no progress. Secondary count alone has the explicit saturation rule.

## 8. Capture, deadlines and cleanup ownership

Before target creation, validate current request/binary/input/fences, reserve all owned storage, configure the private no-breakaway job and exact three-handle inheritance list, open bounded stage logs with exclusive-writer/share-read semantics, observe their starts, and prepare fixed worker/synchronization/attribute/NUL owners. Logs use OPEN_ALWAYS, GENERIC_WRITE, FILE_SHARE_READ, noninheritable handles and an observed seek to FILE_END before start-position capture; no other writer is permitted while owned. Pipe construction preserves the predecessor CreatePipe default size request, without claiming a fixed kernel-buffer allocation. Use creation-time job membership under a separately qualified Windows10+ interface/profile; do not substitute an unproved create-suspended/manual-assign atomicity claim. CreateProcessW uses CREATE_SUSPENDED|CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT|EXTENDED_STARTUPINFO_PRESENT, the exact three-handle list and no breakaway/new-console flag. Close supervisor copies of child pipe writers after successful inheritance. Create/ready both private workers, then set TargetMayHaveRun before attempting ResumeThread. A failure before resume may remain status2 only after its reached owners are settled. Partial returned process/handle states are conservatively owned and cannot be erased by an API false return.

After successful launch/resume return, start the target-body clock with the same post-launch-return intent as the predecessor. The coordinator tests completion, then poll admission; at7200 completed body polls it records PollLimit before entering another iteration. Each admitted iteration increments bodyPolls, consumes stdout then stderr ready records, tests elapsed time strictly>1800 seconds and, if no refusal, performs the250ms rate-bound wait even after target exit while a pipe is pending. A poll-limit refusal is not relabeled as elapsed timeout. Known target exit requires the target handle to be signaled and a successful32-bit exit-code query;259 is a valid observed code, not proof of liveness. Source-price the complete worker/coordinator progress, synthetic zero-EOF, refusal and timing paths; transfer no old259/1,056,772 price without a fresh census.

The supervisor owns that target clock/poll policy. Parent helper polling is separately source-priced and must not infer target elapsed/poll flags from helper runtime. A separately admitted external host watchdog may stop an unfinished attempt; it is recorded as a supervisor host-budget abort/unknown quarantine, not a new target input-admission rule or a fabricated target timeout. Its finite profile and source/host-time bounds must be reviewed before execution; no longer target1800s allowance or invisible overall time proof is granted here.

On normal completion or first failure, stop admission of new body reads/writes and enter the mandatory tail. Cancellation is attempted only on owned worker handles with a source-owned operation-generation/Stop protocol. Native cancellation is not completion and ERROR_NOT_FOUND alone is not a failed-read witness. The bounded cleanup schedule retains the predecessor's target-exit attempt (2000ms) and worker-settlement attempt (2000ms), with finite source-priced retries/waits within those groups. Job termination/query and owner-release steps needed by the selected SDK must fit those groups or remain an explicit unqualified native premise; a timeout parameter is not a hard wall-clock proof.

Copy any actually observed target raw exit before releasing its process/thread handles. Account every required retained reference when obtaining the current empty-job proof. A job is not a generic all-exit event; close, termination request and absent notification are not evidence. A valid current zero population may be recorded only after the selected API/reference semantics have been qualified. Residual owned descendants may be terminated as cleanup without host enumeration; failures or unobserved settlement remain status4. A completion record never makes a child outside the admitted owned-job model disappear by assumption.

Consume final ready results once under section7, observe worker exit or conclusive never-started states, flush log prefixes and capture their final positions, then release only owners proved safe. Pending I/O buffers/worker state are never freed, recycled, unwound or cleared while a worker can use them. The status4 path preserves those owners through terminal publication and uses a reviewed nonreturning process-termination boundary rather than unwinding pending stack storage. Parent quarantine retains all unknown owner/native terms and files; OS process exit/handle close earns no scratch refund.

## 9. Cleanup-mask projection and first failures

cleanupMask has only bits0..14. They mean, in increasing order: cancellation group failed; target reap API group failed; target exit not observed in its cleanup group; worker-join diagnostic; stdout settled-read fault; stderr settled-read fault; stdout flush failed; stderr flush failed; stdout position unavailable; stderr position unavailable; stdout owner-release group failed; stderr owner-release group failed; worker/cancellation-owner release group failed; target/job-owner release group failed; unresolved-owner quarantine. Bits1/2 are mutually exclusive: an actual reap API failure takes precedence over the no-observed-exit category. Owner groups attempt their finite reached native releases without erasing later failure counts; one mask bit summarizes its group, while secondaryFailureCount and immutable first records retain first/multiplicity evidence. This explicitly replaces an accidental per-handle interpretation of the old opaque SDK cleanup groups.

The parent emits cleanup codes in this fixed logical order: bit0 `read_cancel`; bit1 `own_child_reap_failure` or bit2 `own_child_exit_not_observed_2000ms`; bit3 `read_settlement_diagnostic`; bits4/5 each `settled_read_cancel_or_fault`; bits6/7 each `retained_prefix_flush`; bits8/9 each `retained_position_refusal`; bits10..13 each `settled_owner_dispose`; bit14 `one_rooted_unsettled_owner_no_more_children`. The terminal's primary/secondary records preserve occurrence order independently of this canonical diagnostic-list order. A native failure is recorded immediately using its documented API error result, never a stale GetLastError; semantic diagnostics carry no invented native code.

The successor cleanup-list extent is derived anew: fifteen mask categories minus one mutually exclusive reap category give at most14 prefix entries. A valid terminal followed by a late helper failure adds `supervisor_tail_failed`, and adds the quarantine code only if absent; the maximum is15 entries, not an unreviewed transfer of the predecessor13-entry schedule. An invalid/missing terminal emits only `supervisor_terminal_unavailable` and quarantine; a host abort emits only `supervisor_host_budget_abort` and quarantine if there is no admitted prefix. This new finite extent, strings and all owners must be included in the fresh census/JSON/resource preflight; no global scratch/retained/log/work/time policy cap is raised.

## 10. Same-origin reservation and observed prefix

Before launch the parent sponsors the entire helperReservation from the fresh reviewed runner-owned source slice, in addition to its own changed caller work and the unchanged separately owned target reservations. mandatoryTailReservation is a separately priced part of that same slice, not extra allowance. PrefixLimit=helperReservation-mandatoryTailReservation. The predecode/current-profile source and all first-failure paths are included; an unavailable/invalid request cannot turn their work into zero. No R13 price or aggregate slack establishes a new value.

Only the bootstrap/body prefix has a runtime observed prefix meter. A source-owned serialized debit/admission state (one bounded SRWLOCK/state owner, separately counted/qualified) makes worker/coordinator debits and BeginTail linearizable. Each prefix debit is checked against prefixLimit before the operation; no counter wraps. BeginTail closes body admission under that same state, freezes sourcePrefixUsed as the last admitted prefix debit total and switches all subsequent worker/coordinator work to the mandatory tail. Already admitted native calls may complete; their return/settlement/publish work is covered by the tail, not falsely added after the frozen prefix. No prefix debit may appear after the cut.

The entire bounded tail is reserved statically by the fresh whole census: cancellation, target/job/worker settlement attempts, final-ready accounting, flush/positions/safe releases, record construction, stage/transport checks, all short terminal writes, terminal flush/close, remaining eligible releases, process exit and late-failure branches. It is not reported as measured complete usage. Cleanup and record outcome collection run after the prefix cut; the immutable terminal-record freeze is later. Failures observed before record freeze appear in its primary/secondary/status/mask. Work and failures after record freeze cannot be hidden by an earlier status0.

PrefixKnown means only the frozen prefix observation is valid and<=prefixLimit. It does not mean tail usage was measured or total usage equals that value. Meter overflow/invariant loss clears PrefixKnown and stores0; the full reservation remains owned. Prefix-limit refusal records reason37, then uses the already prepaid tail. The tail never relies on unused prefix units or resets the bank. The parent retains/debits the full helper reservation for every launched attempt and never automatically refunds helperReservation-sourcePrefixUsed. It may expose the observation in private provenance, but must not redefine existing runner_used as measured total helper/controller usage or label prefix+reserved tail as observed actual usage. Source execution is prohibited until the real prices/tail schedule/bindings prove the reservation; a dynamic or native invariant break cannot be certified by a saturated counter.

## 11. Helper exit and terminal admission

The helper has five fixed exit codes:0 complete terminal publication and all required post-record actions completed without an unrecorded failure;10 no current usable terminal identity/record could be produced;11 publication or any post-record-freeze mandatory action failed;12 a complete status4 quarantine record was published;13 the prefix meter was invalid and a noncertifying record was published. Precedence is11 for late failure, else13 for invalid meter, else12 for status4, else0;10 applies when no usable record exists. These are ordinary controlled exits, not deliberate crashes/WER fixtures. No terminal can certify its own publication tail: only parent-observed helper exit0 plus the full terminal success matrix commits status0/1 evidence.

Terminal is created once, explicitly encoded, written with bounded positive-progress short-write handling and a fresh complete stage scan before each actual write (transport adds0 stage bytes), checked against its own308-byte transport cap/whole reservation, flushed and closed. No retry/overwrite file is created on failure. Any failed/no-progress write, flush, close or later mandatory action after record freeze requires exit11; the prewritten record is not rewritten into a fake complete observation. Missing/partial/malformed/current-identity mismatch is unavailable and quarantined even if the helper exits0. A valid failure terminal with helper exit0 can preserve its actual known outcomes; it does not authorize another target.

The parent first performs fixed-length/version/magic/current-identity/EOF and numeric range/pair/flag/counter validation. Only then may it use individual valid observations. If helper exit is nonzero or unobserved, it retains those independently valid prefix observations but forces global quarantine/no next child and never commits status0/1 as completed. It never reads unknown flags as numeric-zero success or uses target SHA as loaded-image proof. For transport failure without any admitted prefix, target start/exit/positions/counters/polls/EOF/settlement are unavailable, not fabricated false/zero; unknown_undrained and quarantine are conservatively true.

## 12. Parent child-row projection

Keep the existing serialized child-row keys. New unavailable values are explicit nulls, a documented strengthening of old zero sentinels; success types/values remain unchanged. No private terminal object/byte array enters JSON. Raw terminal/request files are the provenance for additional native fields.

| Existing key(s) | Projection from an admitted terminal |
|---|---|
| name | Exact childIndex/name mapping. |
| started | ChildCreated; null if no admitted current prefix. |
| raw_exit_observed; raw_exit | TargetExitKnown; signed32 bit reinterpretation if set, else null. For u>=2^31 use u-2^32 in a wide integer before final int32. Code259 remains valid when observed. |
| timed_out; poll_limited | Bits18/19 only, never parent watchdog elapsed. Null without admitted prefix. |
| polls; poll_limit | bodyPolls0..7200 only with CoordinatorCountersKnown, otherwise null; fixed7200. |
| log_cap | RawLogCap or StagePoolCap; StageGuardRejected alone is false. |
| stdout_start; stderr_start | Corresponding start if observed, otherwise null. |
| position_record_complete | All four position flags. |
| raw_stdout_bytes; raw_stderr_bytes | end-start only when position_record_complete; otherwise both null, matching the predecessor's combined completeness rule. |
| raw_bytes_retained | Sum of confirmed written values only with CoordinatorCountersKnown, otherwise null; not a position delta. |
| raw_bytes_observed | Sum of successful logical-result observed values only with CoordinatorCountersKnown, otherwise null; errorPartial is separate private evidence. |
| observed_not_confirmed_retained | Sum of unretained only with CoordinatorCountersKnown, otherwise null. |
| omitted_at_cap | Sum of omitted only with CoordinatorCountersKnown, otherwise null; only declared raw/stage-cap refusals. |
| both_eof | Both logical zero-EOF flags. |
| unknown_undrained | ChildCreated and not both_eof; true conservatively if creation/prefix is unavailable. |
| reads_settled | Both settled flags; null without an admitted prefix. |
| owned_storage_quarantined | Status4, unavailable/invalid prefix, nonzero/unobserved helper exit, parent host abort or unresolved parent owner. Known settled facts are not erased by this global flag. |
| cleanup | Section9's bounded canonical list plus applicable parent tail/transport/quarantine code. |
| failure | `CHILD_POLL_LIMIT` for bit19; otherwise `CHILD_BODY_FAILURE_RAW_RETAINED` for a primary/body refusal; `Bounded cleanup refusal` for cleanup-only failure; null for fully committed status0/1 with no cleanup failure. A later parent failure preserves that first failure and adds its own raw primary/secondary record. |

With no admitted current prefix, raw byte counters, log_cap, position completeness, both_eof and reads_settled are null; no missing counter is treated as a retained0-byte success. The old current JSON codec permits nulls; every changed shape/extent and the new cleanup bound must nevertheless receive fresh whole correspondence/census/preflight review. Read-TaskChildResult is reached only after committed status0 and therefore has known start/raw-length fields.

The first existing controller raw primary remains immutable. A helper primary creates one bounded local carrier of its site/reason/native-known/code and current terminal provenance; it becomes the controller primary only if none exists, otherwise the existing child-finally first-secondary slot is used. The helper's first secondary is processed after that primary into the same existing first-secondary policy, without overwriting an occupied slot. A first native secondary is retained in private provenance even if an earlier semantic secondary occupied the slot. Further failures retain their count/mask/file evidence and cannot overwrite first records. General CLR/ErrorRecord/exception heaps remain explicitly unqualified under the existing model, not counted as zero.

A fully committed status1 first records the actual raw target exit and child row, then raises the existing actual-child-nonzero refusal; it is not translated to helper failure or a new native error. After/binary/input fences always execute using the existing primary/secondary ordering, even on helper/transport refusal. Parent late-tail/transport/host-abort failures create bounded local provenance with unavailable native code unless that parent itself observed a documented API error. They preserve any valid target/position/prefix observations, root quarantine, retain all files/owners/reservations and forbid another child.

## 13. Accounting, headless profile and gates

Count controller+supervisor+actual target together under unchanged32MiB controlled scratch, existing retained policy and original target/log/stage/body limits. Include140/41,260/18,644 request buffers,308 terminal buffer, hex/raw-command-line buffers, run-hash state, target quoting/environment block, two4096 workers, coordinator and main/worker stacks, synchronization/meter/attribute/job/handle/NUL/pipe/log/transport owners, CRT/TLS/cold/native terms, parent wrappers/polls, source reservations and all partial/failed/quarantined prefixes. No helper process exemption, compiler-opacity extension, GC/close/Dispose/free/pool-return credit or arbitrary safety multiplier is introduced. Actual source/binary/native ABI and SDK ownership/time premises remain separate proof gates.

The fixed console helper and gated console targets use no shell, redirected helper outputs, GUI/foreground/input/audio/MIDI APIs or deliberate crash/WER tests. CREATE_NO_WINDOW and the separately reviewed null-stdin/error-dialog fresh-launch profile are required; these are not guarantees for arbitrary executable types. Target NUL is explicit and counted. Future actual app drives still obey the repository's separate-session policy.

The order is: independent acceptance of this exact architecture/protocol/owner/funding strategy -> root authorization for a new unrun source-only successor -> freeze candidate plus fresh whole functional/census/resource proposals recording every unknown -> separately reviewed bounded compile-only ABI/binary purpose qualification as needed -> complete actual source/native/resource/binding acceptance -> separate root one-attempt helper/fixture/Clock execution gate. No final-source/ABI proof is demanded merely to author its unrun candidate; no incomplete source/census/ABI proposal authorizes execution. This architecture decision grants none of the later execution gates. Source-only authoring is separately authorized in the source plan.
