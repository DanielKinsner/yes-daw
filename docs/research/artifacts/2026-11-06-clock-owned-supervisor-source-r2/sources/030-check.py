"""Bounded own source/file/hash/integer checks. No target language parser or run."""
from pathlib import Path
import hashlib,json,re,math,shutil,sys,traceback
from regressions import run as run_regressions

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
NEW=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r15-owned-supervisor-source-r1'
OLD=NEW.parent/'clock-oracle-r13-streamed-dependency-source-r1'
BASE=NEW.parent/'clock-oracle-r14-owned-supervisor-source-r1'
OLDPACKET=ROOT/'build-ci/20261106-clock-owned-supervisor-source-r1'
ATTEMPTS=OUT/'check-attempts'
ATTEMPTS.mkdir(exist_ok=True)
ATT=ATTEMPTS/f'attempt-{len(list(ATTEMPTS.glob("attempt-*")))+1:02d}'
ATT.mkdir()
if (OUT/'freeze.json').exists(): raise RuntimeError('Already frozen; do not rerun')
subject_paths=[*sorted((OUT/'native').glob('*.cpp')),*sorted((OUT/'native').glob('*.h')),
 *(NEW/n for n in ['run-clock.ps1','source-manifest.json','README.md','clock_stage.cpp','source-costs.h']),
 OUT/'author.py',OUT/'candidate-readme.md',OUT/'proposal.md',OUT/'prepare-native.py',OUT/'regressions.py',Path(__file__)]
for p in subject_paths:
    q=ATT/p.relative_to(ROOT)
    q.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(p,q)
checks=[]
def uncaught(kind,value,tb):
    text=''.join(traceback.format_exception(kind,value,tb))
    (ATT/'stderr.txt').write_text(text)
    sys.__excepthook__(kind,value,tb)
sys.excepthook=uncaught
def report(value):
    text=json.dumps(value)+'\n'
    (ATT/'stdout.txt').write_text(text)
    print(text,end='')
def check(name,value,evidence=None):
    checks.append({'id':name,'pass':bool(value),'evidence':evidence})
def sha(raw):return hashlib.sha256(raw).hexdigest().upper()
def file_sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        while True:
            b=f.read(1048576)
            if not b:break
            h.update(b)
    return h.hexdigest().upper()
def rec(p):
    raw=p.read_bytes()
    return {'path':p.relative_to(ROOT).as_posix(),'bytes':len(raw),'sha256':sha(raw)}
def dump(p,v):p.write_bytes((json.dumps(v,indent=2,ensure_ascii=True)+'\n').encode())
def functions(text):
    matches=list(re.finditer(r'^function ([A-Za-z0-9-]+)',text,re.M))
    return {m.group(1):text[m.start():matches[i+1].start() if i+1<len(matches) else text.index('\ntry{',m.end())] for i,m in enumerate(matches)}
def nodes(v):
    if isinstance(v,dict):return 1+sum(1+nodes(x) for x in v.values())
    if isinstance(v,list):return 1+sum(nodes(x) for x in v)
    return 1
def geometry(v,level=0):
    if isinstance(v,dict):
        rows=[geometry(x,level+1) for x in v.values()]
        return max([level+1]+[x[0] for x in rows]),max([0]+[len(k.encode('utf-16-le'))//2 for k in v]+[x[1] for x in rows])
    if isinstance(v,list):
        rows=[geometry(x,level+1) for x in v]
        return max([level+1]+[x[0] for x in rows]),max([0]+[x[1] for x in rows])
    return level,len(v.encode('utf-16-le'))//2 if isinstance(v,str) else 0

d=json.loads((NEW/'source-manifest.json').read_bytes())
prior=json.loads((OLD/'source-manifest.json').read_bytes())
runner=(NEW/'run-clock.ps1').read_text()
old=(OLD/'run-clock.ps1').read_text()
native={p.name:p.read_text() for p in (OUT/'native').glob('*') if p.suffix in ['.cpp','.h']}
all_native='\n'.join(native.values())
h=native['sup.h'];core=native['sup_core.cpp'];codec=native['sup_codec.cpp'];io=native['sup_io.cpp'];main=native['sup_main.cpp']
protected={
 'run-clock.ps1':'9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788',
 'source-manifest.json':'10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3',
 'README.md':'7243B4D955A821A59340928CE3D06D2810C85B9D58C5792AA97B8F34CC565512',
 'clock_stage.cpp':'F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206',
 'source-costs.h':'0087C41C88C8647FCC9CAAD92DA55223164122A4E31FF2DFC27715FAAEB83A27'}
for name,expected in protected.items():check('protected-r13-'+name,file_sha(OLD/name)==expected)
for name in ['clock_stage.cpp','source-costs.h']:check('target-byte-exact-'+name,(OLD/name).read_bytes()==(NEW/name).read_bytes())
include='../waveform-definition-r3/independent_clock.h'
check('target-relative-include-unchanged',include in (NEW/'clock_stage.cpp').read_text())
check('relative-include-resolves-existing-file',(NEW/'../waveform-definition-r3/independent_clock.h').is_file())

docs={
 'docs/adr/0090-clock-research-owned-process-supervisor.md':'68DC0922BF56F0B9697CF178B69639D5346684EAC0829BEE9DB55F03CB4F6565',
 'docs/plans/2026-11-06-clock-supervisor-source-plan.md':'6B572E5359764A41EBCF7E1AE5EF833A59C44B03E19CE4CD32278CCC059030CB',
 'docs/research/clock-supervisor-protocol-v1.md':'7A933415DFAA6B00C76F317F723E68B8E953055B7D3F41AA65CBAAAA536367C2',
 'docs/adr/0091-clock-supervisor-job-termination-diagnostic.md':'C46B61E7839BE765956D80E9B3AA24E86CC6B39DEDBF12C96A97449C1A2CE126',
 'docs/research/clock-supervisor-protocol-v2.md':'6F5FCA9F2A2831FB54B7E6CB2EB2F8A38060B6EDF2EC1CA4C590F8BB6ED9CDDC'}
for name,expected in docs.items():check('accepted-doc-'+name,file_sha(ROOT/name)==expected)
check('v2-independent-review-read',(ROOT/'build-ci/20261106-clock-supervisor-v2-independent-review-r1/review.md').is_file())
check('rejected-diagnostic-finding-preserved',(OLDPACKET/'diagnostic-gap-r1.json').is_file() and (OLDPACKET/'diagnostic-gap-snapshot-r1/sup_main.cpp').is_file())
finding=json.loads((OLDPACKET/'diagnostic-gap-r1.json').read_bytes())
for x in finding['files']:check('diagnostic-snapshot-'+Path(x['path']).name,file_sha(OLDPACKET/'diagnostic-gap-snapshot-r1'/Path(x['path']).name)==x['sha256'])
check('root-whole-review-pins',file_sha(ROOT/'build-ci/20261106-r14-supervisor-functional-review-r1/source-pins.json')=='B01F381BC5B28A56DBDFFEDC38BC4C1E47BEA8AA06F8515AB24BAB5EEF2D4389')
check('root-whole-review-freeze',file_sha(ROOT/'build-ci/20261106-r14-supervisor-functional-review-r1/freeze.json')=='5CAEEC7FA4EA5FF0B80F130DC1DE4DF952EB76578B857D8150D327FE7B955080')
check('io-review-pins',file_sha(ROOT/'build-ci/20261106-r14-io-independent-review-r1/source-pins.json')=='6EECF95CCC2026EB8FE19A6E1C8B8ADA423029CD6899F9EEAE9BE1F7F906ECF5')
r14_pins=json.loads((OLDPACKET/'source-pins.json').read_bytes())
for row in r14_pins['subjects']:
    p=ROOT/row['path'];check('frozen-r14-subject-'+row['path'],p.stat().st_size==row['bytes'] and file_sha(p)==row['sha256'])

check('native-exact-five-files',set(native)=={'sup.h','sup_core.cpp','sup_codec.cpp','sup_io.cpp','sup_main.cpp'})
check('no-Data-definition',all_native.count('extern const Catalogue Data;')==1 and re.search(r'\b(?:const|constinit)\s+Catalogue\s+Data\s*[={]',all_native) is None)
for flag in ['source_review_accepted','source_work_accepted','native_profile_accepted','controlled_resource_accepted','execution_accepted']:
    check('native-false-'+flag,f'bool {flag} = false;' in h)
for flag in ['SourceReview','SourceWork','Execution','Compiler','Produce','Verify','Supervisor','SupervisorProfile']:
    check('runner-false-'+flag,f'$task{flag}Accepted=$false' in runner)
check('manifest-all-guards-false',not any(d['guards'].values()))
check('all-whole-price-bounds-unbound',all(d['source_work'][x] is None for x in ['runner_upper','producer_upper','verifier_upper','whole_upper']))
check('helper-current-binary-unbound',d['supervisor']['binary']['bytes'] is None and d['supervisor']['binary']['sha256'] is None)
check('helper-price-slots-unbound',all(v is None for v in d['supervisor']['controller_prices'].values()))
check('helper-reservations-unbound',d['supervisor']['helper_reservations']==[None]*3 and d['supervisor']['tail_reservations']==[None]*3)
check('native-sdk-union-explicitly-unbound',d['supervisor']['native_sdk_include_inputs'].startswith('UNBOUND_'))
check('binding-before-launch',runner.index('Assert-TaskSupervisorBinding;Spend-TaskSupervisor')<runner.index('$taskProcess=[Diagnostics.Process]::Start($taskInfo)'))

# All original function bodies are compared, not default-accepted by omission.
old_functions=functions(old);new_functions=functions(runner)
changed={'Initialize-TaskPathCatalogue','Invoke-TaskChild','Add-TaskChildCleanup','Initialize-TaskController','Assert-TaskPins'}
check('old-function-coverage',len(old_functions)==74 and set(old_functions)<=set(new_functions))
for name,body in old_functions.items():
    if name in changed:check('changed-function-present-'+name,new_functions[name]!=body)
    else:check('unchanged-function-byte-text-'+name,new_functions[name]==body)
new_names=sorted(set(new_functions)-set(old_functions))
check('new-helpers-explicit',len(new_names)==14,new_names)
base_functions=functions((BASE/'run-clock.ps1').read_text())
check('all88-R14-helper-bodies-present',set(base_functions)==set(new_functions) and len(base_functions)==88)
for name,body in base_functions.items():check('R14-parent-body-unchanged-'+name,new_functions[name]==body)
check('TypeEmptyTypes-corrected-once',runner.count('[Type]::EmptyTypes')==1 and '[Type[]]::EmptyTypes' not in runner)
check('no-current-MainModule-query','MainModule.FileName' not in runner and '[Diagnostics.Process]::GetCurrentProcess()' not in runner)
check('ProcessPath-null-prefix-exact-host',all(x in new_functions['Initialize-TaskController'] for x in ['[Environment]::ProcessPath','if($null -eq $taskNativeHost)',".StartsWith('\\\\?\\',[StringComparison]::Ordinal)",'.Substring(4)','[StringComparison]::OrdinalIgnoreCase']))
check('startup-env-gates-byte-text',old_functions['Initialize-TaskController'].split(' $taskRuntime=')[0]==new_functions['Initialize-TaskController'].split(' $taskRuntime=')[0])
old_control_ids=re.findall(r"Expect-TaskGate '([^']+)'",old)
check('all-old-control-ids-order',old_control_ids==re.findall(r"Expect-TaskGate '([^']+)'",runner))
for field in ['compiler','environment','references','compiler_final_outputs','control_files','stage_files','normal_gate_controls','module_controls','predecessor_bundle','controller','current_include_inputs','closed_json_domain']:
    check('protected-manifest-field-'+field,d[field]==prior[field])

check('v2-manifest',d['supervisor']['protocol_version']==2)
for label,text in [('core',core),('runner',runner)]:check('v2-domain-'+label,'YESDAW-CLOCK-SUPERVISOR-RUN-V2' in text and 'YESDAW-CLOCK-SUPERVISOR-RUN-V1' not in text)
check('v2-request-magic-native',"'Y','C','S','R','E','Q','0','2'" in codec)
check('v2-terminal-magic-native',"'Y','C','S','R','E','S','0','2'" in codec)
check('v2-numeric-native','get32(r.capture+8)!=2' in codec and 'fields[16]={2,terminal_bytes' in codec)
check('v2-request-terminal-parent',"$taskMagic='YCSREQ02'" in runner and "$taskMagic='YCSRES02'" in runner and '$taskU[0] -ne 2' in runner and ' $taskBytes 8 4 2;' in runner)
check('no-current-v1-magic','YCSREQ01' not in runner and 'YCSRES01' not in runner and "'Q','0','1'" not in codec and "'S','0','1'" not in codec)
reason_names=[x.strip() for x in re.search(r'enum class Reason : U32 \{(.*?)\};',h,re.S).group(1).split(',')]
check('reason48-appended',len(reason_names)==49 and reason_names[32]=='JobQueryFailure' and reason_names[48]=='JobTerminationFailure')
check('reason48-pair-native','case 48:return site==10;' in codec)
check('reason48-pair-parent','48{return ($Site -eq 10)}' in runner)
term=main[main.index('if (!TerminateJobObject'):main.index('if ((state.terminal.flags&ChildCreated)',main.index('if (!TerminateJobObject'))]
check('termination-not-query', 'Reason::JobTerminationFailure,true,error' in term and 'Reason::JobQueryFailure' not in term)
check('termination-immediate-native-error',term.index('GetLastError()')<term.index('state.target_exit_group_failed'))
check('termination-reap-bit-and-precedence','cleanup_mask=(state.terminal.cleanup_mask|2u)&~4u' in term and 'if ((t.cleanup_mask&2u)!=0) t.cleanup_mask&=~4u;' in main)
query=main[main.index('if (!QueryInformationJobObject'):main.index('for (U32 i=0;i<2;++i) if (live(state.logs',main.index('if (!QueryInformationJobObject'))]
check('query-remains32','Reason::JobQueryFailure,true,error' in query and 'Reason::JobTerminationFailure' not in query)
check('termination-native-mask-reader',"SUPERVISOR_TERMINATION_FAILURE_NATIVE_REAP_GROUP" in runner and 'reason_present(t,48) && (t.cleanup_mask&2u)==0' in codec)

# Fixed format arithmetic and endian models are data checks, never target runs.
request_header=8+5*4+2*8+3*32
terminal_size=8+16*4+3*8+3*32+2*7*8+4
check('request-header140',request_header==140)
check('terminal308-no-padding',terminal_size==308)
check('compiler-request41260',140+40*(4+2*512)==41260)
check('native-request18644',140+18*(4+2*512)==18644)
check('transport79472',41260+2*18644+3*308==79472)
check('raw-command2576',2+512+1+12+4*512+1==2576)
check('target-command29271',512+2+28*(2*512+3)+1==29271)
check('environment4156',sum(map(len,['INCLUDE','LIB','PATH','SystemRoot','TEMP','TMP','VSLANG','windir']))+8*512+8+8+1==4156)
for width in [4,8]:
    for v in [0,1,259,(1<<(8*width-1))-1,1<<(8*width-1),(1<<(8*width))-1]:
        b=bytes((v>>(8*i))&255 for i in range(width))
        check(f'LE-data-roundtrip-{width}-{v}',sum(c<<(8*i) for i,c in enumerate(b))==v)
for raw,expected in [(0,0),(259,259),(0x7fffffff,2147483647),(0x80000000,-2147483648),(0xffffffff,-1)]:
    check('signed-exit-model-'+str(raw),(raw if raw<1<<31 else raw-(1<<32))==expected)
offsets={'version':8,'total':12,'index':16,'flags':20,'pid':24,'exit':28,'status':32,'primary_site':36,'primary_reason':40,'primary_error':44,'secondary_site':48,'secondary_reason':52,'secondary_error':56,'native_secondary_site':60,'native_secondary_reason':64,'native_secondary_error':68,'secondary_count':72,'prefix':80,'polls':88,'manifest':96,'run':128,'target':160,'out':192,'err':248,'cleanup':304,'EOF':308}
check('terminal-offset-chain',list(offsets.values())==list(range(8,72,4))+[72,80,88,96,128,160,192,248,304,308])
check('native-terminal-no-struct-serialize','put32(bytes+304,t.cleanup_mask)' in codec and 'put64(bytes+192u+56u*pipe+8u*i' in codec and 'reinterpret_cast' not in all_native)
check('parent-terminal-field-extents',all(x in runner for x in ['[uint32[]]::new(16)','[uint64[]]::new(3)','[uint64[]]::new(14)','[byte[]]::new(308)']))
check('parent-request-string-array','[string[]]::new(12+$Arguments.Length)' in runner)
check('borrowed-byte-UTF16-view','struct Text { const U8* bytes' in h and 'r.strings[i]=Text{r.capture+at,length}' in codec)

# Flags/status masks and full bounded cleanup-mask enumeration.
flags=re.findall(r'([A-Za-z][A-Za-z0-9]*)\s*=\s*1u\s*<<\s*(\d+)',re.search(r'enum Flag : U32 \{(.*?)\};',h,re.S).group(1))
check('flags28-exact-bits',len(flags)==28 and sorted(int(x[1]) for x in flags)==list(range(28)))
required=sum(1<<b for b in [0,1,2,3,4,5,6,7,8,9,10,11,16,17,24,25,26,27])
forbidden=sum(1<<b for b in [12,13,14,15,18,19,20,21,22,23])
settled=sum(1<<b for b in [2,3,6,11,27])
check('status-masks-independent',required==251858943 and forbidden==16576512 and settled==134219852 and required&forbidden==0)
check('status-masks-parent-source',f'$taskRequired={required};$taskForbidden={forbidden}' in runner and f'$taskSettledMask={settled}' in runner)
max_prefix=max_late=0
for mask in range(1<<15):
    if mask&2 and mask&4:continue
    n=mask.bit_count();max_prefix=max(max_prefix,n)
    max_late=max(max_late,n+1+(not bool(mask&(1<<14))))
check('all-cleanup-masks-upper14-15',(max_prefix,max_late)==(14,15))
check('all-unknown-child-values-null',all(f'${x}=$null' in new_functions['Invoke-TaskChild'] for x in ['taskStarted','taskExitObserved','taskExit','taskTimedOut','taskPollLimited','taskPolls','taskCap','taskBeforeA','taskBeforeB','taskRawA','taskRawB','taskPositions','taskWritten','taskObserved','taskUnretained','taskOmitted','taskBothEof','taskSettled']))
old_row=re.search(r'\$taskRow=\[ordered\]@\{(.*?)\}\n',old_functions['Invoke-TaskChild'],re.S).group(1)
new_row=re.search(r'\$taskRow=\[ordered\]@\{(.*?)\}\n',new_functions['Invoke-TaskChild'],re.S).group(1)
row_keys=lambda text:re.findall(r'(?:^|;)([a-z_]+)=',text)
check('all-old-child-keys-exact-order',row_keys(old_row)==row_keys(new_row),row_keys(new_row))
check('no-private-carrier-in-row',all(x not in new_row for x in ['taskPrefix','taskRequestPath','taskTargetDigest','taskU[','taskV[','taskP[']))
check('raw-terminal-not-json',"$Terminal['bytes']" not in new_row and 'terminal=$taskPrefix' not in new_row)
check('parent-raw259-not-active','if($taskSigned -ge 2147483648L)' in runner and '$taskSigned-=4294967296L' in runner and '259' not in new_functions['Invoke-TaskChild'])
check('parent-only-committed-success-return','if($null -ne $taskFailure -or -not $taskCommitted)' in runner and "if($taskExit -ne 0){throw 'ACTUAL_CHILD_RAW_EXIT_FAILURE'}" in runner)
check('late-failure-preserves-prefix','SUPERVISOR_TAIL_FAILED' in runner and "$taskU=$taskPrefix['u']" in runner and "$taskRow['owned_storage_quarantined']=$true" in runner)
check('no-parent-tree-kill-or-async-read','.Kill($true)' not in runner and 'ReadAtLeastAsync(' not in runner and '.Kill()' in new_functions['Invoke-TaskChild'])
check('helper-not-redirected',all(f'$taskInfo.RedirectStandard{x}=$false' in runner for x in ['Input','Output','Error']))
check('helper-explicit-env-cleared','$taskInfo.Environment.Clear();$taskKeys=' in runner and '$taskInfo.Environment[$taskKeys[$taskI]]=$taskEnvironment[$taskI]' in runner)
check('helper-single-hex-argument','$taskInfo.ArgumentList.Add($taskRequestArgument)' in runner and "$taskPrefix='--request16='" in runner)
check('parent-no-observed-source-total-fabrication','sourcePrefixUsed' not in new_row and "Spend-Task ([long]$taskManifest['supervisor']['helper_reservations'][$taskChildIndex])" in runner)

# Owner/control/source ordering. These are exact source facts, not native proofs.
for field in ['request_file','hash_file_owner','search_owner','terminal_file','logs[2]','child_writes[2]','nul_input','job','process','primary_thread','attributes','attribute_allocation','inherited[3]','job_attribute[1]','target_command[target_command_units]','environment[environment_units]','hash_buffer[65536]','terminal_capture[terminal_bytes]']:
    check('state-owner-'+field,field in h)
for field in ['HANDLE pipe','HANDLE thread','HANDLE ready','HANDLE ack','HANDLE started','buffer[pipe_buffer_bytes]','WorkerExit exit']:
    check('worker-owner-'+field,field in h)
check('no-unused-prefix-price-families',set(x.strip() for x in re.search(r'enum class Family : U32 \{(.*?)\};',h,re.S).group(1).split(','))<=set(re.findall(r'Family::(\w+)',all_native)))
check('one-metre-SRW-owner','SRWLOCK lock = SRWLOCK_INIT' in h and 'AcquireSRWLockExclusive(&meter.lock)' in core)
check('read-write-tickets-before-native-API','!admit_read(state.meter)' in main and io.index('if (!admit_write(state.meter))')<io.index('if (!WriteFile(state.logs'))
check('cut-freezes-prefix-before-tail',all(x in core for x in ['meter.tail = true','meter.frozen_prefix = meter.used','state.meter.reported = true']))
check('worker-no-terminal-writes','state.terminal' not in main[main.index('DWORD WINAPI worker_entry'):main.index('bool start_workers')])
check('ready-ack-stop-protocol',all(x in main for x in ['SetEvent(worker.ready)','WaitForSingleObject(worker.ack,INFINITE)','InterlockedExchange(&state.workers[i].stop,1)','worker.consumed_generation=worker.generation','worker.ready_consumed=true']))
check('partial-not-observed','worker.partial=worker.active_fill' in main and 'error_partial=exit.partial' in main and 'counter(state,count.observed,worker.count)' in main)
check('EOF-short-synthetic-zero','worker.eof_short_pending=false;worker.kind=ReadyKind::ZeroEof' in main)
check('unknown-live-partial-never-read','worker.partial' not in main[main.index('void coordinator'):main.index('void cleanup_prefix')])
check('worker-actual-exit-before-partial-copy',main.index('if (wait==WAIT_OBJECT_0) {')<main.index('error_partial=exit.partial'))
check('native-read-no-async-graph',all(x not in all_native for x in ['ReadFileEx(','OVERLAPPED ','std::future','std::thread','ReadAtLeastAsync(']))
check('default-pipe-size-and-NUL','CreatePipe(&read,&write,&security,0)' in io and 'CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE' in io)
check('exact-inherit-list-three','PROC_THREAD_ATTRIBUTE_HANDLE_LIST,state.inherited,sizeof(state.inherited)' in io and 'state.inherited[0]=state.nul_input' in io and 'state.inherited[2]=state.child_writes[1]' in io)
check('job-not-in-inherit-list','state.inherited[0]=state.job' not in io and 'state.inherited[1]=state.job' not in io and 'state.inherited[2]=state.job' not in io)
check('creation-time-job-list','PROC_THREAD_ATTRIBUTE_JOB_LIST,state.job_attribute' in io and 'AssignProcessToJobObject' not in all_native)
check('kill-on-close-no-breakaway','JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE' in io and 'BREAKAWAY_OK' not in io and 'CREATE_BREAKAWAY_FROM_JOB' not in io)
check('create-flags-exact','CREATE_SUSPENDED|CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT|EXTENDED_STARTUPINFO_PRESENT' in io)
check('failed-creator-raw-not-valid-owner','capture_raw(state.failed_process_output,&process' in io and 'state.unknown_native_creation=true' in io and '|| state.unknown_native_creation' in main)
check('attribute-exact-qualified-size','bytes!=Data.attribute_bytes' in io and 'HeapAlloc(GetProcessHeap(),0,bytes)' in io)
check('no-guessed-stack-reserve','CreateThread(nullptr,0,worker_entry' in main and 'STACK_SIZE_PARAM_IS_A_RESERVATION' not in all_native)
check('start-ready-before-resume',main.index('create_contained_target(state) && start_workers(state)')<main.index('state.terminal.flags|=TargetMayHaveRun')<main.index('ResumeThread(state.primary_thread)'))
check('clock-start-after-resume',main.index('ResumeThread(state.primary_thread)')<main.index('state.body_started=GetTickCount64()'))
check('target-exit-signaled-first','if (wait!=WAIT_OBJECT_0)' in main[main.index('bool observe_target_exit'):main.index('void coordinator')] and main.index('GetExitCodeProcess(state.process,&exit)')<main.index('target_raw_exit=exit'))
check('raw259-not-special-native','STILL_ACTIVE' not in all_native)
check('strict-target-deadline','now-state.body_started>1800000u' in main)
check('poll-bound-before-next-body',main.index('body_polls>=body_poll_limit')<main.index('counter(state,state.terminal.body_polls,1)'))
check('stdout-then-stderr-loop','for (U32 i=0;i<2;++i)' in main[main.index('void coordinator'):main.index('void cleanup_prefix')])
check('both-cleanup-groups-bounded','WaitForSingleObject(state.process,cleanup_milliseconds)' in main and 'retry<=8' in main and 'now-start>=cleanup_milliseconds' in main)
check('cancel-only-owned-thread','CancelSynchronousIo(worker.thread)' in main and 'error!=ERROR_NOT_FOUND' in main)
check('job-query-not-job-wait','QueryInformationJobObject(state.job,JobObjectBasicAccountingInformation' in main and 'WaitForSingleObject(state.job' not in all_native)
check('references-released-before-job-query',main.index('HANDLE* owners[2]')<main.index('QueryInformationJobObject(state.job'))
check('no-unsettled-worker-free','if (worker.settled || !worker.created)' in io)
check('nonreturning-boundary','[[noreturn]] void finish' in main and 'ExitProcess(code)' in main)
check('record-capture-freeze-after-encoding',io.index('if (!encode_terminal(state))')<io.index('state.record_frozen=true;'))
check('late-tail-exit11-precedence','if (state.record_frozen && state.tail_failed) code=11' in main)
check('helper-code-no-stdout-or-env-query',all(x not in all_native for x in ['printf(','puts(','std::cout','std::cerr','GetEnvironmentVariable','GetEnvironmentStrings','GetStdHandle','ReadConsole','WriteConsole']))
check('no-GUI-audio-input-call',all(x not in all_native for x in ['CreateWindow','MessageBox','SendInput','SetForegroundWindow','waveOut','midiOut','CoCreateInstance']))

check('raw-cap-before-write','length>raw_log_limit-retained' in io and io.index('length>raw_log_limit-retained')<io.index('if (!WriteFile(state.logs'))
check('complete-stage-before-each-log-write',io.index('if (!stage_scan(state,length-at,extent))')<io.index('if (!admit_write(state))')<io.index('if (!WriteFile(state.logs'))
check('complete-stage-before-each-terminal-write','if (!stage_scan(state,0,extent)) return false;\n        DWORD written=0;\n        if (!WriteFile(state.terminal_file' in io)
check('current-stage-metadata-not-find-cache','GetFileAttributesExW(child.cells' in io and 'validate_path(state,child,false,Site::StageAdmission)' in io)
check('stage-no-quiet-input-or-reserve1M','additional>stage_limit-extent' in io and 'FindNextFileW(search,&entry)' in io)
check('failed-write-output-not-used','if (!WriteFile(state.logs[direction],bytes+at,length-at,&written,nullptr)) { const DWORD error=GetLastError(); fail' in io)
check('zero-write-stops','written==0?Reason::ZeroWriteProgress' in io)
check('position-independent-confirmed','SetFilePointerEx(state.logs[direction],zero,&position,FILE_CURRENT)' in io and 'count.written,written' in io)
check('transport-create-new-only','GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW' in io and '[IO.FileMode]::CreateNew' in runner)
check('no-terminal-overwrite-or-retry','CREATE_ALWAYS' not in io and 'OPEN_ALWAYS' not in io[io.index('bool publish_terminal'):])
check('terminal-flush-close-separate','FlushFileBuffers(state.terminal_file)' in io and 'return close_one(state,state.terminal_file)' in io)
check('parent-stage-before-request-write','$null=Get-TaskStageBytes;$taskFile.Write($taskBytes,0,$taskBytes.Length)' in runner)
check('same-stage-names-native-parent',all(name in io for name in d['stage_files']))

# Independently derive SHA constants from FIPS180-4's first-prime root rule.
primes=[];n=2
while len(primes)<64:
    if all(n%p for p in primes if p*p<=n):primes.append(n)
    n+=1
def icbrt(n):
    lo,hi=0,1<<((n.bit_length()+2)//3)
    while lo+1<hi:
        mid=(lo+hi)//2
        if mid**3<=n:lo=mid
        else:hi=mid
    return hi if hi**3<=n else lo
k=[icbrt(p<<96)-(icbrt(p)<<32) for p in primes]
section=core.split('sha_constants[64] = {',1)[1].split('};',1)[0]
actual=[int(x,16) for x in re.findall(r'0x([0-9a-f]+)',section)]
check('SHA64-cube-root-constants',actual==k)
initial=[math.isqrt(p<<64)-(math.isqrt(p)<<32) for p in primes[:8]]
observed_initial=[int(x,16) for x in re.findall(r'sha\.h\[\d\]=0x([0-9a-f]+)',core)]
check('SHA8-square-root-initials',initial==observed_initial)
check('SHA-standard-big-endian-length','bits >> (56u-8u*i)' in core and 'sha.pending_count < 56' in core)
check('run-domain-one-NUL-native','static_cast<U32>(sizeof(domain))' in core)
check('run-domain-one-NUL-parent','$taskDomain.Length+1+32+4+2*$taskRoot.Length' in runner)

# Whole provisional input and source-pin correspondence; never parse input code.
check('old-input-rows-exact-prefix',d['inputs'][:len(prior['inputs'])]==prior['inputs'])
base_manifest=json.loads((BASE/'source-manifest.json').read_bytes())
check('all-R14-input-rows-exact-prefix',d['inputs'][:len(base_manifest['inputs'])]==base_manifest['inputs'])
check('input-unique-paths',len({x['path'] for x in d['inputs']})==len(d['inputs']))
for pin in d['inputs']:
    p=Path(pin['path'])
    if not p.is_absolute():p=ROOT/p
    check('input-pin-'+pin['path'],p.is_file() and p.stat().st_size==pin['bytes'] and file_sha(p)==pin['sha256'])
for pin in d['sources']:
    p=NEW/pin['path'];raw=p.read_bytes()
    check('source-pin-'+pin['path'],len(raw)==pin['bytes'] and sha(raw)==pin['sha256'] and len(raw.splitlines())==pin['lines'])
for pin in d['supervisor']['native_sources']:
    p=ROOT/pin['path'];raw=p.read_bytes()
    check('native-pin-'+p.name,len(raw)==pin['bytes'] and sha(raw)==pin['sha256'] and len(raw.splitlines())==pin['lines'])
check('input-count-profile',len(d['inputs'])==d['profile']['input_count'])
check('input-bytes-profile',sum(x['bytes'] for x in d['inputs'])==d['profile']['input_bytes'])
check('input-chunks-profile',sum((x['bytes']+65535)//65536 for x in d['inputs'])==d['profile']['input_hash_chunks'])
check('include-profile',d['profile']['include_input_count']==len(d['inputs'])+1)
current=len(d['inputs'])+4+1+1
catalogue=current+len(d['references'])+len(d['compiler_final_outputs'])+len(d['control_files'])+len(d['stage_files'])+6
check('current-inputs-with-binary',d['profile']['complete_current_inputs']==current)
check('whole-path-catalogue-recipe',d['profile']['maximum_complete_fence_identities']==catalogue)
check('catalogue-extra-transport-and-binary',"foreach($taskName in $taskManifest['supervisor']['transport_files'])" in runner and 'Add-TaskPathCatalogueEntry $taskSupervisorBinaryPath $false' in runner)
check('transport-files-exact',d['supervisor']['transport_files']==[f'child-{i}.{kind}.bin' for i in range(3) for kind in ['request','terminal']])
check('transport-directory-created','$taskOpaqueRoot,$taskTransportRoot,' in runner)
check('transport-final-run-file-set',"$taskExpectedRunFiles.Add('transport/'+$taskN)" in runner)
check('transport-hashed-artifact-rows',"foreach($taskN in $taskManifest['supervisor']['transport_files']){$taskPath=" in runner)
check('helper-binary-transitive-row',"$taskTransitive.Add(@{path=$taskSupervisorBinaryPath;bytes=$taskManifest['supervisor']['binary']['bytes']" in runner)
check('helper-retained-and-transport-reserved',"+$taskControlCaps+79472L+[long]$taskManifest['supervisor']['binary']['bytes']" in runner)
check('JSON-output-array-extra-helper','array_entries=($taskManifest[\'inputs\'].Count+$taskOwnNames.Count+2)' in runner)
check('own-five-text-cap',sum(x['bytes'] for x in d['sources'])+(NEW/'source-manifest.json').stat().st_size<=1048576)
check('bootstrap-exact-nodes',f'Assert-TaskRawJsonBytes $taskBootstrap {nodes(d)} 512 4' in runner)
check('bootstrap-depth4-string512',geometry(d)[0]<=4 and geometry(d)[1]<=512,geometry(d))
check('retained-arithmetic-not-fit',d['retention']['reservation_upper']==d['profile']['input_bytes']+1048576+152627224+67108864+67108864+33652457+79472+33554432)
check('retention-cap-unchanged',d['retention']['cap']==prior['retention']['cap']==880803840)
check('controlled-scratch-cap-unchanged',"$taskScratch=33554432L" in runner and d['supervisor']['scratch_scope'].endswith('32MiB_UNCHANGED'))
check('namespace-count-with-native',len(d['closed_directory_inputs'])==d['profile']['directory_namespaces_count']==len(base_manifest['closed_directory_inputs'])+1)
native_ns=d['closed_directory_inputs'][-1]
check('native-namespace-exact-five',native_ns['files']==sorted(native) and native_ns['entry_count']==5 and native_ns['recursive'] is False)
for p in subject_paths:check('LF-no-BOM-'+p.name,b'\r' not in p.read_bytes() and not p.read_bytes().startswith(b'\xef\xbb\xbf'))
regression_rows,counterexamples=run_regressions(native)
checks.extend(regression_rows)

inventory={'old_functions':len(old_functions),'all_functions':len(new_functions),'new_helpers':new_names,'changed_old_helpers':sorted(changed),
 'unchanged_old_helpers':sorted(set(old_functions)-changed),'bootstrap':{'nodes':nodes(d),'depth':geometry(d)[0],'maximum_string_units':geometry(d)[1]},
 'profile':d['profile'],'format':{'request_header':140,'request_caps':[41260,18644,18644],'terminal':308,'transport':79472,'offsets':offsets,'flags':flags,'cleanup_prefix_upper':max_prefix,'cleanup_late_upper':max_late},
 'native_prefix_families':[x.strip() for x in re.search(r'enum class Family : U32 \{(.*?)\};',h,re.S).group(1).split(',') if x.strip()!='Count'],
 'all88_parent_R14_bodies_byte_text_unchanged':True,'R15_findings_covered':[x['finding'] for x in counterexamples],
 'NoProgress':{'normal_continuations_upper':2*7200,'pending_slots_upper':2,'native_zero_results_upper':2*7200+2,'new_input_guard':False,'extra_normal_poll':False},
 'protocol':'IMMUTABLE_V1_PLUS_ACCEPTED_EXACT_V2_DELTA','scope':'OWN_SOURCE_CONSISTENCY_ONLY_INDEPENDENT_WHOLE_CRITIC_AND_ALL_FRESH_PROOF_GATES_PENDING',
 'unqualified':['Actual native SDK/header/import/binary association','Field/stack/owner ABI and all controlled joined phases','Same-origin positive controller/helper/target prices, full schedules and prepaid tail','Creation-time job/nested/creator-death/reference and current empty-query semantics','Synchronous I/O/cancellation/native call/ExitProcess settlement and time','Trusted fresh null/noninteractive/error-dialog controller/helper launch profile and external N','General CRT/CLR/native bookkeeping and exception/fault graphs under the accepted boundary','Final canonical input union, immutable actual Data storage and separate root execution GO']}
result={'scope':'SOURCE_FILE_HASH_INTEGER_ONLY_NO_TARGET_EXECUTION','checks':len(checks),'failed':sum(not x['pass'] for x in checks),'rows':checks}
dump(ATT/'checks.json',result)
dump(ATT/'counterexamples.json',counterexamples)
dump(ATT/'result.json',{'exit_code':1 if result['failed'] else 0,'scope':'OWN_SOURCE_FILE_HASH_INTEGER_ONLY','no_target_execution':True})
if result['failed']:
    report({'checks':len(checks),'failed':[x['id'] for x in checks if not x['pass']]})
    raise SystemExit(1)
pins={'protected_r13':protected,'protected_r14_subjects':r14_pins['subjects'],'accepted_docs':docs,'subjects':[rec(p) for p in subject_paths],
 'input_manifest':rec(NEW/'source-manifest.json'),'whole_input_rows_verified':len(d['inputs']),
 'target_parser_AST_import_compiler_helper_fixture_Clock_execution':False}
dump(OUT/'correspondence.json',inventory)
dump(OUT/'counterexamples.json',counterexamples)
dump(OUT/'checks.json',result)
dump(OUT/'source-pins.json',pins)
dump(OUT/'freeze.json',{'status':'FIRST_COMPLETE_OWN_SOURCE_CHECK_PASS_FROZEN_UNRUN_INDEPENDENT_REVIEW_PENDING','checks':len(checks),'failed':0,
 'outputs':[rec(OUT/x) for x in ['correspondence.json','checks.json','source-pins.json','authoring.json','counterexamples.json']],
 'subjects':pins['subjects'],'no_target_execution':True,'all_price_resource_ABI_native_profile_binding_execution_gates':'UNBOUND_HOLD'})
report({'checks':len(checks),'failed':0,'frozen':True,'helpers':len(new_functions),'bootstrap':inventory['bootstrap'],'profile':{x:d['profile'][x] for x in ['input_count','input_bytes','maximum_complete_fence_identities']}})
