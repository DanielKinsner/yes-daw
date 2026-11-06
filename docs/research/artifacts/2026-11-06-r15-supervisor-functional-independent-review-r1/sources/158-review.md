# Whole R15 functional source review — REVISE

The complete frozen R15 candidate needs a successor. Its eight R14 corrections
are supported by the examined source, but a worker's pre-resume meter refusal can
still be followed by a new target-resume attempt. Current manifest failure-policy
descriptions also retain unqualified predecessor bounds. These are source
findings, not observed runtime failures. No candidate or predecessor was edited.

## Complete subject and method

The subject is `20261106-clock-owned-supervisor-source-r2`, pins
5806F35FE26CC1CB2A43B7E49AD6CEBEDE6C28EE7ABAC6BE52CDBE77292FDDB1,
freeze895F4F1BB04F955E9E40BFDC29AC29AE15AB844ECE21AF068D3DFAEF16914169,
and its five-file R15 sibling, runner
6B1016841798BA2B4D6F7CEAEFC8F663F3A5FE0D7CEC3CEC34355EC71B527A38,
manifestBDC2BE017BDF269691D727AE10A489FD6C1EFBB82F2B1C71DB392B1728D1B86F.
The author/regression/checker source, correspondence, counterexamples and
preserved failures were read as evidence, never imported or rerun.

This review read all325 header,479 codec,263 core,429 I/O and467 main lines;
the complete parent bootstrap, fourteen added helpers, changed predecessor
helpers, main/output/failure route, protected controls and before/after fences;
the remaining predecessor helper logic and its unchanged correspondence; and
Accepted ADR0090/0091, complete protocol v1/v2 and source plan. All88 current
parent bodies are compared byte-for-byte to R14. The only runner change is its
derived bootstrap node literal. Target CPP/header remain byte-exact. Whole
functional acceptance is not inferred from that correspondence or the author's
1764 assertions.

The independent `check.py` uses only ordinary file/JSON/hash/integer/text work.
It checks all1240 declared pins/461790563 bytes/7886 chunks, all33 closed
namespace file/name/entry records, five source/native companions and all
protected/accepted seals. It independently recomputes bootstrap values+keys16561,
depth4/max435, the request/terminal/transport/argv/environment extents,1321
catalogued identities and1246 complete current inputs. New source counterexample
and finite boundary arguments are independent of the candidate's handwritten
models. No target parser/AST/import, compiler, API purpose, helper, fixture,
program, Clock, GUI, desktop input, audio/MIDI device or Git operation ran.

## Findings requiring correction

**F9 — A worker's pre-resume meter cut does not stop the resume attempt.**

In `sup_core.cpp:43–60`, a refused first WorkerRead ticket sets `refused`, `tail`
and `frozen_prefix` under the meter lock. This can happen after worker0 signals
its started event while the established target is still suspended. In
`sup_core.cpp:9–34`, a later ordinary `spend` returns true once tail is set because
its `ok` starts true and its prefix branch is skipped. Therefore
`start_workers` (`sup_main.cpp:209–233`) can create/await worker1 and return true
after that cut. `supervisor_main:443–449` then sets TargetMayHaveRun and calls
ResumeThread without any source body-admission/cut decision. The coordinator
notices tail immediately afterward and proceeds to cleanup.

The exact source trace is preserved in `counterexample-pre-resume-cut.json`.
It needs no invented Data definition, actual price, native failure or runtime
observation. It selects a first-read debit refusal, a source failure path the
candidate expressly supports. The post-cut ResumeThread is not a native call
already admitted before the cut. Protocol v1 sections8/10 reserve the tail for
mandatory completion/cleanup/publication after refusal; it does not authorize
starting target execution after the cut. Honest TargetMayHaveRun reporting and
later termination do not repair the missing admission.

The successor should give worker startup/resume an explicit source-owned body
admission using the existing cut semantics, preserve an already admitted
operation's completion, and keep a target suspended when the cut precedes its
resume admission. Then settle that reached suspended owner through the existing
pre-execution failure route. Review analogous setup/body seams. Do not replace
this with a quiet-worker, sufficient-slack or input-domain premise, a new budget,
or lost completed-read evidence.

**F10 — Current descriptive failure bounds are stale.**

Manifest11871–11882 still presents `maximum_raw_failed_prefix_file_rows=78`,
`maximum_run_entries=78` and `bounded_child_cleanup_events=13` without historical
labels. Current profile/main use85 root/failure rows and15 cleanup entries. The
added `R14_cleanup_entries_max=15` neither labels the other values historical nor
defines an override rule. The retained recipe69 is also the predecessor's
prospective set: this current reference/build/control/stage/transport recipe has
75 positions, whereas the separately defined normal success set has74. Those
meanings must remain distinct.

Recompute current descriptions or explicitly move/label predecessor values as
historical. Runtime admission already reads profile85 and the new15 guard; this
is a metadata/correspondence correction, not an observed runtime undercount,
resource violation or permission to set a new cap.

## Eight prior findings and analogous paths

F1 publication now writes all ready-slot fields before SetEvent, keeps those
fields unchanged through signal/ack failures, and stores separate exit failures.
The coordinator reads the exit record only after the actual thread-exit wait
acquires it. Its writes to consumed/ack state precede acknowledgement. A failed
reset records failure and acknowledges Stop, so a still-signaled event cannot
authorize a new filling generation while the coordinator reads it.

F2 completed Full/EOF-short/zero-EOF records survive publication-meter and
signal/ack refusals. An exited worker's unsignaled complete slot is consumed once,
without reset/ack APIs. A refused Consume debit enters tail but retains mandatory
accounting. Missing EOF alone does not create a false read failure. Exit-only
incomplete positive fill remains errorPartial, separate from observed/written;
neither mutable active_fill nor a live exit record is read by the coordinator.

F3 normal/initial-cleanup/retry ready waits use one immediately capturing helper;
body/cleanup exit acquisition uses the same explicit WAIT_FAILED branch. Started,
ack, process-body and process-cleanup waits retain their own immediate capture.
A later timeout cannot replace an earlier native observation. Current Microsoft
[wait documentation](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject)
supports the wait-result/error distinction; actual selected synchronization,
memory visibility, syscall/import and timing associations remain separate gates.

F4 TRUE/zero is a private NoProgress yield. It retains active fill and adds no
logical observation or EOF. Continue resumes the same fill; Stop/actual exit
acquires only frozen partial evidence. One visit per direction per normal poll
bounds14400 consumed controls plus two final pending slots. Pre-resume and
cleanup add no normal continuation. There is no zero-write-free assumption,
extra poll/deadline or drain-until-progress loop. Microsoft
[ReadFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile)
distinguishes pipe TRUE/zero from broken-pipe EOF. This finite zero-control bound
does not transfer an old outer259/inner1056772 scalar or resource price to native
workers; all new positive/zero/EOF/error/cleanup paths require fresh census.

F5 the sole occupied FindFirst/FindClose owner prevents a later search. Failed
close retains its exact owner and release diagnostic. Request/hash/log/pipe/job/
worker/attribute setup preconditions likewise prevent established-owner overwrite;
eligible later closes are finite retries of the same retained owner, not release
credit or new owner allocation.

F6 each actual short write still has a full fresh stage scan and admission. An
aggregate refusal adds exactly remaining `length-at` to omitted, once, retaining
previous confirmed writes. Non-cap refusals leave omitted unchanged. Failed
WriteFile never interprets an untrusted bytes-written output as confirmed
progress; separately observed file positions remain independently valid.

F7 PathResult preserves SourceRefused/ExtentRejected/Ok across all join callers.
Coordinator spend calls BeginTail before later path/I/O labels. Only actual
extent rejection becomes StageExtent; native error capture and immutable first
primary/secondary/native-secondary ordering remain separate. The new F9 finding
is the analogous distinction between tail sponsorship and fresh body admission,
which the existing wrapper does not establish for resume/startup.

F8 failed pipe/process outputs remain raw private object representations. No
failed output word is evaluated as an owner/PID or sent to wait/close/resume/
identity APIs. Successful creator contracts establish ordinary owners. Even raw
zeros keep unknown_native_creation true, force status4/quarantine, and keep
normal PID/ChildCreated unavailable; independently observed job-empty and known
owner settlement do not erase that unknown. Current
[CreatePipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe)
documents indeterminate failed outputs. Actual raw-object representation/ABI,
creation/job membership and unknown native resource terms remain pending.

## Whole examined correspondence and limits

Raw command parsing is bounded before file access, requires quoted fixed helper,
one request16 uppercase argument and EOF, and compares current image/helper path.
LE request/string widths, subtraction-first bounds and exact EOF are explicit.
Run SHA uses the v2 domain including its zero byte, manifest32, u32 UTF16 length
and exact root cells. Request/manifest/target/helper SHA/current path checks do not
claim loaded-image association. The owned SHA rounds/padding and header cell
accesses were read in full; compiler/binary or cryptographic runtime test credit
is not fabricated.

Target environment has exactly eight explicit sorted entries and the4156-cell
double-terminated bound. Target argv quoting preserves empty/space/quote/backslash
cases in the admitted ASCII domain. The existing one-pass native template versus
sequential parent replacement correspondence still needs concrete immutable
binding/profile qualification; no new failing path or domain narrowing is
invented. NUL stdin, creation-time private job, explicit three-handle inheritance,
suspended creation, no-breakaway/no-window and no ambient helper stream use
remain explicit. Native creation/creator-death/nesting and last-reference/current
population semantics are not proved by source API names.

Target exit requires a real signal and successful query; observed259 remains259,
and uint32 values at/above2^31 are reinterpreted in a wide integer before int32.
The body tests completion, poll admission, stdout/stderr and strict >1800s time
in the accepted order, with7200 bodies and250ms waits. Native timeout parameters
are not whole wall-time guarantees. Parent host abort uses separate unavailable/
quarantine evidence and cannot manufacture target timeout/poll flags.

Cleanup retains both2000ms groups, finite retries, actual errorPartial/flush/
position evidence, all reached releases and unresolved owners. Current job-empty
proof follows the selected reference semantics, still unqualified. Pending workers
are not unwound, cleared or recycled; nonreturning ExitProcess remains a selected
native/time/resource obligation. A source close/action is not observed settlement
or physical-free credit.

Terminal validation checks every flag/site/reason/native-availability/secondary/
counter/position/partial/status relation. Reason48 remains an actual termination
failure with immediate native error and reap bit precedence, while query failure
stays32. Private fields never enter the wire. Record freeze precedes actual
publication and eligible late releases; late failure changes helper exit rather
than rewriting terminal bytes. Parent observed exit0 plus complete status0/1
matrix is the commit. Independently valid facts survive later helper/transport/
identity/parent-disposal refusal; malformed/missing evidence is null and globally
quarantined. Fifteen cleanup entries follow the mutually exclusive reap category
and conditional late/quarantine additions, with explicit guards.

The source input union retains all distinct old physical identities and adds
five R14 files plus five current native files, without content deduplication.
Transport six files/directory, helper companion, all before/after fences, ten
actual runner controls, fourteen module controls, all dense/fraction/inverse
payload recipes and success output projections remain examined. Source retention
arithmetic is816970452 using the future helper's maximum32MiB term, without
promoting an unknown actual binary to zero or certifying a resource peak.

This disposition grants no individual source price or unchanged-body acceptance,
native/ABI/stack/CRT/runtime/profile/canonical union/resource/binding proof, Data,
limit, combined U or execution GO. Both Data remain undefined and every execution
flag remains false. General runtime bookkeeping is unqualified, not zero or a
new whole-RSS proof requirement. No failed ReadFile broken-pipe count or HeapFree
stale-error finding is asserted. Preserve all current subjects/failed attempts;
one complete successor and another fresh whole functional review are required.
