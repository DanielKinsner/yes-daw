"""Independent whole architecture/protocol source-and-data critic. No subject execution."""
import hashlib,html,json,pathlib,re
ROOT=pathlib.Path(__file__).resolve().parents[2]
HERE=pathlib.Path(__file__).parent
ADR=ROOT/'docs/adr/0090-clock-research-owned-process-supervisor.md'
PLAN=ROOT/'docs/plans/2026-11-06-clock-supervisor-source-plan.md'
HYP=ROOT/'build-ci/20261106-controller-owned-process-hypothesis-r1'
V=ROOT/'build-ci/20261106-controller-v-wrapper-source-closure-r1'
for name in ['review.json','review.md','checks.json','source-pins.json']:
 if (HERE/name).exists():raise RuntimeError('PRESERVE_FIRST_PASS_'+name)
checks=[];inputs={}
def sha(data):return hashlib.sha256(data).hexdigest().upper()
def rec(path):
 path=path.resolve();data=path.read_bytes()
 try:name=path.relative_to(ROOT).as_posix()
 except ValueError:name=path.as_posix()
 row={'path':name,'bytes':len(data),'sha256':sha(data)};inputs[name]=row
 return row
def read(path):rec(path);return path.read_text(encoding='utf-8')
def ck(name,condition):
 checks.append({'id':name,'pass':bool(condition)})
 if not condition:
  (HERE/'failed-checks.json').write_text(json.dumps(checks,indent=2)+'\n',encoding='utf-8',newline='\n')
  raise RuntimeError(name)
def enc(value):return (json.dumps(value,indent=2,ensure_ascii=True)+'\n').encode()
adr=read(ADR);plan=read(PLAN)
ck('ADR_PROPOSED_R1_EXACT_SEAL',rec(ADR)['sha256']=='697F696166AF1BB47C3CFC4844EF213A8B56D24A4B8371C2497D23C8A0B252EB')
ck('PLAN_PROPOSED_R1_EXACT_SEAL',rec(PLAN)['sha256']=='050294B6C025D2131BF222234BEC811FB59666BC3CFE414E286FE5E7B1DC6709')
ck('ADR_IS_PROPOSED','**Status:** Proposed' in adr)
ck('NO_CURRENT_IMPLEMENTATION_AUTHORITY','This ADR grants none of them yet.' in adr)
hyp_data=read(HYP/'hypothesis.json')
ck('FROZEN_HYPOTHESIS_SEAL',sha(hyp_data.encode())=='79F75D9781519941917CA43897512121649E3776406D724971033E6ACB578AF9')
ck('FROZEN_HYPOTHESIS_IS_SOURCE_ONLY',json.loads(hyp_data)['source_execution'] is False)
ck('FROZEN_V_BASELINE_SEAL',rec(V/'closure.json')['sha256']=='01B4289C2F8E521093987C259D15C5DD4BBF2388EFD9E594A90319C9B0BB5EE6')
read(HYP/'lineage-erratum.json')
runner_path=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1/run-clock.ps1'
runner=read(runner_path)
ck('FROZEN_R12_SOURCE_SEAL',rec(runner_path)['sha256']=='3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F')
for at,literal in enumerate([
 '$taskOut.Position;$taskBeforeB=$taskErr.Position',
 '$taskN=$taskRead.GetAwaiter().GetResult()',
 '$taskWritten+=$taskN;',
 "$taskEndA=$taskOut.Position",
 "raw_stdout_bytes=$(if($taskPositions){$taskEndA-$taskBeforeA}else{$null})",
 'timed_out=$taskTimedOut;poll_limited=$taskPollLimited;',
 'log_cap=$taskCap;',
 'if($Ascii -and ($taskC -lt 32 -or $taskC -gt 126))',
 "Assert-TaskExactKeys $taskManifest['environment'] @('SystemRoot','windir','PATH','INCLUDE','LIB','TEMP','TMP','VSLANG')",
 '$taskInfo.Environment.Clear();',
 '$taskInfo.ArgumentList.Add($taskArgument)']):ck('BASELINE_CORRESPONDENCE_ANCHOR_'+str(at),literal in runner)

# Wire arithmetic and offsets are independent integer calculations, not a compiler layout.
request_fields=[('magic',8),('version',4),('totalBytes',4),('childIndex',4),('argumentCount',4),('environmentCount',4),('reservation',8),('manifestSHA',32),('runSHA',32),('targetSHA',32)]
terminal_fields=[('magic',8),('version',4),('totalBytes',4),('childIndex',4),('flags',4),('targetPid',4),('targetRawExit',4),('status',4),('primarySite',4),('primaryError',4),('firstSecondaryError',4),('secondaryFailureCount',8),('sourceUsed',8),('manifestSHA',32),('runSHA',32),('targetSHA',32),('stdoutObserved',8),('stdoutWritten',8),('stdoutOmitted',8),('stdoutUnretained',8),('stderrObserved',8),('stderrWritten',8),('stderrOmitted',8),('stderrUnretained',8)]
def offsets(fields):
 out=[];position=0
 for name,length in fields:out.append({'field':name,'offset':position,'bytes':length});position+=length
 return out,position
request_offsets,header=offsets(request_fields);terminal_offsets,terminal=offsets(terminal_fields)
ck('REQUEST_HEADER132',header==132)
ck('REQUEST_COMPILER_STRINGS40',4+8+28==40)
ck('REQUEST_COMPILER_MAX41252',header+40*(4+2*512)==41252)
ck('REQUEST_NATIVE_STRINGS18',4+8+6==18)
ck('REQUEST_NATIVE_MAX18636',header+18*(4+2*512)==18636)
ck('TERMINAL224',terminal==224)
ck('NO_POSITION_VALUES_IN224',not any('position' in x['field'].lower() or 'start' in x['field'].lower() or 'end' in x['field'].lower() for x in terminal_offsets))
ck('FOUR_NEW_U64_POSITIONS_WOULD_BE256',terminal+4*8==256)
ck('EXISTING_REQUEST_RESERVATION_OFFSET_IS28',next(x for x in request_offsets if x['field']=='reservation')['offset']==28)
ck('EXPLICIT_LE_REQUIRED_FOR_UNALIGNED_FIELD','Encode fields\nexplicitly' in adr)

primary_docs={}
for row in json.loads(read(HYP/'document-origins.json')):
 path=HYP/'primary-documents'/row['file'];data=path.read_bytes();rec(path)
 ck('PINNED_PRIMARY_DOCUMENT_'+row['file'],sha(data)==row['sha256'])
 primary_docs[row['file']]={'url':row['url'],'text':' '.join(html.unescape(re.sub('<[^>]*>',' ',data.decode())).split())}
for file,words in [
 ('UpdateProcThreadAttribute.html',['PROC_THREAD_ATTRIBUTE_JOB_LIST','job handles to be assigned to the child process','Windows 10']),
 ('JobObjects.html',['JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE','last job object handle','not guaranteed']),
 ('JobBasicAccounting.html',['ActiveProcesses','currently associated with the job']),
 ('CancelPendingIO.html',['does not guarantee','will be canceled']),
 ('AnonymousPipes.html',['overlapped','not supported by anonymous pipes'])]:
 ck('PRIMARY_CONTRACT_'+file,all(x in primary_docs[file]['text'] for x in words))

coverage=[
 {'id':'architecture_scope','ADR_lines':[8,38],'plan_lines':[1,18],'verdict':'SUPPORTED_ARCHITECTURE_DIRECTION','finding':'Research-only S1/S2/S3 choice, frozen predecessors, no production/file-format/RT/golden rewrite, no present authoring/compile/run authority.'},
 {'id':'containment','ADR_lines':[40,54],'plan_lines':[63,68],'verdict':'SUPPORTED_AS_QUALIFICATION_OBLIGATION','finding':'Private noninheritable job, no breakaway, creation-time job list, inherited stdio exclusions and no invented atomic/creator-death proof. Actual profile/SDK semantics remain HOLD until qualified.'},
 {'id':'capture_stage','ADR_lines':[56,74],'plan_lines':[70,81],'verdict':'SUPPORTED_POLICY_NOT_YET_COMPLETE_STATE_MACHINE','finding':'Exactly two fixed4096 buffers, coordinator-only appends, current full stage scan/check before each write, no quiet-output premise, no anonymous-pipe overlapped shortcut and explicit headless profile. Ready/ack/error/settlement transitions and diagnostic projection still need closure.'},
 {'id':'request_wire','ADR_lines':[76,95],'plan_lines':[13,18,47,53],'verdict':'REVISE_EXACT_CONTRACT','finding':'Header132, compilermax41252 and nativemax18636 are correct. Run-identity encoding, raw request-path command grammar, field-specific original string guards and supervisor-versus-target environment recipe remain deferred.'},
 {'id':'terminal_wire','ADR_lines':[97,113],'plan_lines':[20,45,83,88],'verdict':'REVISE_EXACT_CONTRACT','finding':'224-byte LE arithmetic is correct. Values needed for physical-prefix/start diagnostics are not encoded; independent timeout/poll/log distinctions, precise validity/status/error/counter invariants and bit-exact signed parent exit projection are unspecified.'},
 {'id':'settlement','ADR_lines':[115,127],'plan_lines':[77,88],'verdict':'SUPPORTED_CONSERVATIVE_FAILURE_POLICY','finding':'Target exit distinct from helper exit; workers/output/job settlement all required; cancellation/close/notifications do not imply settlement; missing/partial/abnormal remains quarantine/no next child. Counter and owner cutoff ambiguity must still be resolved before code.'},
 {'id':'resource_and_metering','ADR_lines':[129,142],'plan_lines':[16,18,77,81,94,107],'verdict':'REVISE_SOURCE_METER_CUTOFF_AND_TAIL_SPONSORSHIP','finding':'Whole32MiB, source/resource unknowns, no opacity/free/GC/cap transfer and narrow ProcessPath correspondence are supported. SourceUsed snapshot versus publication/cleanup tail and same-origin reservation partitions are not defined.'},
 {'id':'gates_and_tests','ADR_lines':[144,155],'plan_lines':[7,11,55,68,94,128],'verdict':'REVISE_GATE_ORDER_WORDING','finding':'Independent whole reviews and separate compile-only/actual execution gates are correct. Complete native/profile/source pricing before implementation conflicts with later source-census and compile-only ABI steps; make the source-authoring gate distinct from complete executable acceptance.'}
]

position_counterexample={'stdout_start':100,'observed_completed_chunk':4096,'confirmed_successful_write':0,'write_failure_possible_physical_advance':3,'observed_final_position':103,'original_diagnostics':{'stdout_start':100,'raw_stdout_bytes':3,'raw_bytes_retained':0,'observed_not_confirmed_retained':4096},'current_terminal_pipe_fields':{'observed':4096,'written':0,'omitted':0,'unretained':4096,'final_position_observed_flag':True},'missing':'No224-byte field carries100 or103/3, and no parent before/after stat/immutability procedure is specified. Repurposing written would erase the confirmed-write distinction.'}
ck('PARTIAL_WRITE_COUNTEREXAMPLE_COHERENT',position_counterexample['observed_final_position']-position_counterexample['stdout_start']==3 and position_counterexample['current_terminal_pipe_fields']['written']+position_counterexample['current_terminal_pipe_fields']['unretained']==4096)
deadline_counterexample={'case_A':'Elapsed1800s body deadline reached before7200 polls: timed_out=true,poll_limited=false.','case_B':'After the7200th250ms sleep, next poll admission refuses before elapsed check: timed_out=false,poll_limited=true.','shared_terminal_class':{'status':3,'primarySite':7,'native_error_observed':False},'gap':'No independent timed_out/poll_limited flags or semantic reason code. A generic site7 cannot project both existing booleans.'}
for raw,signed in [(0,0),(2,2),(0x7fffffff,2147483647),(0x80000000,-2147483648),(0xffffffff,-1)]:
 ck('RAW_EXIT_SIGNED_PROJECTION_'+str(raw),(raw if raw<2**31 else raw-2**32)==signed)
ck('NO_TIMEOUT_POLL_LOG_FLAGS_DEFINED','Bits10/11 mean primary native error observed' in plan and 'All other bits are invalid' in plan)
ck('REQUEST_GRAMMAR_STILL_DEFERRED','Specify/review exact executable\nquoting, argument prefix, hex/NUL/EOF rules' in plan)
ck('CROSS_FIELD_RULES_NOT_EXPLICIT_MATRIX','all cross-field invariants must pass' in adr and 'status0 additionally requires\nevery success observation' in plan)

findings=[
 {'id':'F1','priority':'P1','title':'Terminal observation bits lack the physical position values required by the existing diagnostics','paths':[{'path':str(ADR),'lines':[97,113]},{'path':str(PLAN),'lines':[38,45]}],
 'evidence':position_counterexample,
 'required_fix':'Add four explicit u64 stdout/stderr start/end positions with separate observation flags, derive the new exact256-byte length, and define unavailable values plus ordered/end>=start checks. Keep confirmed written counters separate. Alternatively retain224 only with a complete reviewed parent before/after position/immutability protocol that really reconstructs all original start/physical-prefix values; the current docs do not specify one.',
 'scope':'This is an encoding/diagnostic correspondence gap, not a request to manufacture physical progress on write failure.'},
 {'id':'F2','priority':'P1','title':'Define exact terminal reason/validity/status/projection rules instead of deferring them','paths':[{'path':str(ADR),'lines':[103,113]},{'path':str(PLAN),'lines':[20,45,83,88]}],
 'evidence':{'deadline_counterexample':deadline_counterexample,'raw_exit_counterexample':'targetRawExit=0x80000000 is a valid ordinary nonzero32-bit process exit and corresponds to parent int32 -2147483648, not a checked positive int conversion.','other_unresolved_cases':['Shared raw-log cap versus complete-stage-pool cap versus non-cap stage guard; original log_cap/omitted booleans and witnesses differ.','A worker gets100 bytes in completed inner reads then errors before publishing a full/EOF-short chunk: define observed/omitted/unretained and parent projection; a generic settled-read phrase is insufficient.','First secondary failure may be semantic, followed by a native cleanup error: define count, first native error/site and existing primary/first-secondary ownership.','Position observations are not byte-counter validity. Define which prefix counters remain exact when a worker is still live, without unsynchronized reads or numeric-zero success.']},
 'required_fix':'Provide a normative flag/status/site/reason table and cross-field admission matrix, including independent timeout/poll-limit/log-cap distinctions, status0/1 versus refusal/quarantine prerequisites, signed raw-exit bit reinterpretation, counter snapshots/overflow, EOF/settlement relationships and all parent child-row/cleanup/primary-secondary projections. Add needed flags/reason fields explicitly and recompute length if fields change. Do not infer reasons from metered usage or generic site7.'},
 {'id':'F3','priority':'P1','title':'Complete the request launch/identity/environment contract before architecture acceptance','paths':[{'path':str(ADR),'lines':[83,95]},{'path':str(PLAN),'lines':[13,18,47,53,63,68]}],
 'evidence':{'path_grammar':'The plan expressly defers executable quoting, prefix and hex/NUL/EOF grammar; at most512 units is ambiguous between decoded path cells and the encoded token.','run_identity':'Canonical run-identity SHA has no defined domain tag, source fields, normalization/case/slash rules or byte serialization. Copying a label is not computing the expected current identity.','environment':'The eight target values/order are named, but the supervisor launch environment and target native environment-block construction/order/NUL rules are not specified. No environment inheritance must cover both processes.','string_profile':'Original target arguments and environment values use ASCII32..126 with length<=512; other paths use their corresponding valid-UTF16/length/filesystem guards. Applying one blanket stricter guard or treating embedded NUL/unknown roots as C strings is not correspondence.'},
 'required_fix':'Specify one exact bounded raw command-line grammar (including executable token, fixed argument prefix, decoded<=512 UTF16 cells/encoded extent, uppercase/lowercase hex decision, quoting, surrogate/NUL and no-extra-argument rules). Define canonical run-identity input bytes and exact expected childIndex/argc/envcount/identity binding. Define both helper and target explicit environment recipes, sorted/native block layout and full source-field string guards while preserving existing28/6 and eight-value target domain. Add a bounded request parser/check sequence before any length-based access/allocation or target creation.'},
 {'id':'F4','priority':'P1','title':'Define the source-counter observation cutoff and prepaid cleanup/publication tail','paths':[{'path':str(ADR),'lines':[84,85,99,106,146,150]},{'path':str(PLAN),'lines':[16,18,24,26,77,88,99,107]}],
 'evidence':'The terminal contains supervisorSourceUsed, yet encoding/writing/flushing/publishing the terminal and final cleanup themselves execute source operations after a snapshot. Source-counter observed alone does not say whether it is a prefix debit or complete usage. Budget exhaustion/failure must still pay for bounded cancellation, owner settlement attempts and an honest terminal/unknown record; no partition/cutoff/overflow rule is defined.',
 'required_fix':'Define the same-origin caller slice, predecode/unknown-usage ownership, body versus mandatory bounded cleanup/publication reservation, exact debit/observation cutoff and checked overflow behavior. Label/report a prefix observation honestly or establish the accepted precharge/tail accounting rule; do not silently count future work as already observed or reset the runner budget. Parent composition must retain the full reservation whenever complete usage is unavailable.'},
 {'id':'F5','priority':'P2','title':'Separate architecture/source-authoring review from complete source/native/resource execution proof','paths':[{'path':str(ADR),'lines':[146,150]},{'path':str(PLAN),'lines':[7,11,55,68,94,117]}],
 'evidence':'ADR146–148 requires complete source-work sponsorship and native/profile premises before implementation; the plan then authors the source, obtains a whole source census and later uses compile-only qualification for binary/ABI association. Read literally, final source/native facts are prerequisites for producing the source/object needed to establish them. The plan also requires every protocol ambiguity resolved before accepting ADR, so these missing exact contracts cannot simply be left to code.',
 'required_fix':'State the ordered gates explicitly: accepted exact architecture/protocol/owner/funding strategy -> authorized source-only candidate -> whole functional/census/resource proposal with all premises recorded -> separately reviewed bounded compile-only purpose qualification where needed -> complete actual source/native/resource/binding acceptance before any helper/fixture/Clock execution. Keep every execution gate closed until its own proof, without demanding nonexistent final-source/ABI facts merely to author an unrun candidate.'}
]
ck('FIVE_CONCRETE_FINDINGS',len(findings)==5 and all(x['required_fix'] for x in findings))
ck('WHOLE_EIGHT_COVERAGE_GROUPS',len(coverage)==8)
for path,expected in [(ADR,'697F696166AF1BB47C3CFC4844EF213A8B56D24A4B8371C2497D23C8A0B252EB'),(PLAN,'050294B6C025D2131BF222234BEC811FB59666BC3CFE414E286FE5E7B1DC6709')]:ck('SUBJECT_PRESERVED_'+path.name,rec(path)['sha256']==expected)
report={'schema':'CLOCK_SUPERVISOR_ARCHITECTURE_INDEPENDENT_REVIEW_R1','verdict':'REVISE_PROPOSED_ARCHITECTURE_EXACT_PROTOCOL_AND_GATE_ORDER','scope':'Whole ADR and source-plan critique only; S1 architecture direction supported. No executable source/ADR acceptance/compile/run/metadata/target/GUI/Git authority granted.','check_count':len(checks),'source_subjects_preserved':True,'supported':['132-byte request header and41,252 compiler/18,636 native request maxima;224-byte proposed terminal arithmetic.','Private job/no breakaway/exact inherited stdio/creation-time feature obligation without invented atomicity.','Two fixed4096 worker buffers with coordinator stage scan/check/write and no quiet-output/cap/free shortcut.','Actual raw target exit, worker/spool/job settlement, abnormal helper quarantine/no next child.','Whole controller+supervisor+actual target accounting under unchanged32MiB; transport outside stage remains counted.','Headless fixed-console source profile, no deliberately crashing/WER fixture, narrow ProcessPath premise and all current HOLD/GO boundaries.'],'whole_coverage':coverage,'wire_arithmetic':{'request_offsets':request_offsets,'request_header':header,'max_compiler_request':41252,'max_native_request':18636,'terminal_offsets':terminal_offsets,'proposed_terminal_bytes':224,'recommended_positions_only_successor_bytes':256},'findings':findings,'recommendation':'Revise the complete proposed docs with exact protocol/projections and noncircular gates, preserve R1, then obtain fresh whole critic review. Source implementation remains unauthorized.','verified_files':list(inputs.values()),'execution':{x:False for x in ['subject_code','PowerShell_parse_AST_import','metadata_reader','compiler','program','Clock_stage','GUI','Git']}}
md='''# Independent whole supervisor architecture review R1

REVISE. S1 is a supported architecture direction, and the containment, headless, whole-budget and conservative quarantine policies match the frozen hypothesis. The proposed exact protocol still leaves information and admission rules needed by the original diagnostics unresolved. No source implementation, ADR acceptance, compiler/program/fixture/Clock execution or GO is authorized.

Request arithmetic is correct:8+5*4+8+3*32=132 bytes;40 bounded strings yield41,252 compiler bytes and18 strings yield18,636 native bytes. Terminal arithmetic is also correct:8+10*4+2*8+3*32+8*8=224 bytes, with explicit LE encoding. This validates byte arithmetic only.

1. **Position values are missing.** ADR97–113 / plan38–45 describe observed positions but encode neither start nor end. A failed write can leave confirmed-written0 and an observed physical append3 from position100 to103. Both facts are needed by the original diagnostics. Add four u64 start/end values and explicit observation bits (256 bytes before any other added fields), or provide a complete alternate parent position protocol. Do not overload confirmed-written.
2. **Terminal meaning needs a normative matrix.** ADR103–113 / plan20–45,83–88 do not distinguish elapsed timeout from poll-limit refusal at site7, or state the original log-cap/omission/partial-read and primary-secondary projections. Define all flags/reasons/status/counter-validity relationships, overflow and parent rows. Preserve raw32-bit exit bits:0x80000000 projects to signed -2147483648, without a checked-conversion refusal.
3. **Request/launch details remain deferred.** ADR83–95 / plan13–18,47–53 need the exact bounded raw command-line and hex-path grammar, run-identity byte recipe, both helper and target explicit environments, native block layout and field-specific original string guards. Preserve28/6 arguments and eight target values. No unspecified stricter guard, inherited environment or arbitrary native argv discovery may fill the gap.
4. **Meter cutoff and tail ownership are undefined.** ADR84–85,99–106,146–150 / plan16–18,24–26,77–88 need an honest prefix/complete source-used definition, checked arithmetic and prepaid mandatory cleanup/publication tail. Terminal publication itself runs after a usage snapshot. Unknown usage retains the whole same-origin reservation; no reset or implied free work.
5. **Gate order is circular literally.** ADR146–150 asks for complete source/native premises before implementation, while plan55–117 creates source, then census and compile-only ABI facts. Separate architecture/source-only authoring from the later complete execution proof. Resolve exact protocol ambiguity before accepting the ADR, then allow only the explicitly reviewed unrun candidate/purpose qualification stages.

The whole review covers scope/options, job/handle containment, capture/coordinator ownership, request, terminal, failure/settlement, whole resources/metering, bootstrap, source order and headless verification. Every concrete finding, source line and supported correction is in review.json. The preserved Microsoft source/documents support job-list qualification, last-handle cleanup limits, actual job accounting, synchronous anonymous pipes and non-guaranteed cancellation; they do not supply missing protocol fields or certify creator-death/native semantics.

The two subjects, frozen hypothesis and V baseline were read only and rehashed. No target/source helper, parser/import, compiler, metadata reader, program, process enumeration, GUI, audio or Git operation was executed. Only this own source/data/integer checker ran.
'''
md+='\n'+str(len(checks))+' mechanical source/file/hash/integer checks passed at the first own checker run.\n'
(HERE/'review.json').write_bytes(enc(report));(HERE/'review.md').write_text(md,encoding='utf-8',newline='\n')
(HERE/'checks.json').write_bytes(enc({'schema':'OWN_ARCHITECTURE_SOURCE_DATA_CHECKS_V1','count':len(checks),'checks':checks,'position_counterexample':position_counterexample,'deadline_counterexample':deadline_counterexample}))
outputs=[]
for name in ['review.json','review.md','checks.json']:
 data=(HERE/name).read_bytes();outputs.append({'path':(HERE/name).relative_to(ROOT).as_posix(),'bytes':len(data),'sha256':sha(data)})
pins={'schema':'CLOCK_SUPERVISOR_ARCHITECTURE_REVIEW_PINS_R1','verdict':report['verdict'],'verified_inputs':list(inputs.values()),'review_outputs':outputs,'own_checker':rec(pathlib.Path(__file__)),'no_target_execution':True}
(HERE/'source-pins.json').write_bytes(enc(pins))
print(json.dumps({'verdict':report['verdict'],'checks':len(checks),'outputs':outputs,'pins_sha256':sha((HERE/'source-pins.json').read_bytes())},indent=2))
