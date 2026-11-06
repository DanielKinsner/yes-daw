"""Own file/string/JSON authoring only. Never imports/parses/runs target sources."""
from pathlib import Path
import copy, hashlib, json, re, shutil

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
REVS=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions'
OLD=REVS/'clock-oracle-r13-streamed-dependency-source-r1'
NEW=REVS/'clock-oracle-r14-owned-supervisor-source-r1'
NEW.mkdir(exist_ok=True)
ATTEMPTS=OUT/'author-attempts'
ATTEMPTS.mkdir(exist_ok=True)
ATTEMPT=ATTEMPTS/f'attempt-{len(list(ATTEMPTS.glob("attempt-*")))+1:02d}'
ATTEMPT.mkdir()
for source in [Path(__file__),OUT/'runner-supervisor.ps1.fragment',*sorted((OUT/'native').glob('*'))]:
    if source.is_file(): shutil.copyfile(source,ATTEMPT/source.name)

def digest(raw): return hashlib.sha256(raw).hexdigest().upper()
def record(path, name=None):
    raw=path.read_bytes()
    return dict(path=name or path.relative_to(ROOT).as_posix(),bytes=len(raw),lines=len(raw.splitlines()),sha256=digest(raw))
def write(path,text): path.write_bytes(text.encode('utf-8'))
def dump(value): return json.dumps(value,indent=2,ensure_ascii=True)+'\n'
def replace_once(text,old,new):
    if text.count(old)!=1: raise ValueError(('author seam',old,text.count(old)))
    return text.replace(old,new,1)
def nodes(v):
    if isinstance(v,dict): return 1+sum(1+nodes(x) for x in v.values())
    if isinstance(v,list): return 1+sum(nodes(x) for x in v)
    return 1
def measure(v,depth=0):
    if isinstance(v,dict):
        rows=[measure(x,depth+1) for x in v.values()]
        return max([depth+1]+[x[0] for x in rows]),max([0]+[len(k.encode('utf-16-le'))//2 for k in v]+[x[1] for x in rows])
    if isinstance(v,list):
        rows=[measure(x,depth+1) for x in v]
        return max([depth+1]+[x[0] for x in rows]),max([0]+[x[1] for x in rows])
    return depth,len(v.encode('utf-16-le'))//2 if isinstance(v,str) else 0

original=(OLD/'run-clock.ps1').read_text(encoding='utf-8')
fragment=(OUT/'runner-supervisor.ps1.fragment').read_text(encoding='utf-8')
# Preserve all original functions outside the explicitly named child/helper
# changes. No PowerShell tokenization, AST, parser or target helper is invoked.
runner=replace_once(original,'[Type[]]::EmptyTypes','[Type]::EmptyTypes')
runner=replace_once(runner,'$taskVerifyAccepted=$false','$taskVerifyAccepted=$false\n$taskSupervisorAccepted=$false\n$taskSupervisorProfileAccepted=$false')
runner=replace_once(runner,'-or -not $taskVerifyAccepted){throw','-or -not $taskVerifyAccepted -or -not $taskSupervisorAccepted -or -not $taskSupervisorProfileAccepted){throw')
host_start=runner.index(' $taskSelf=[Diagnostics.Process]::GetCurrentProcess();')
host_end=runner.index(' $taskArgs=[Environment]::GetCommandLineArgs()',host_start)
host_successor=r''' # Selected fixed immutable fresh-host profile only; ProcessPath can grow/cache
 # outside that profile and supplies no unconditional allocation bound.
 $taskNativeHost=[Environment]::ProcessPath
 if($null -eq $taskNativeHost){throw 'EXACT_NATIVE_CONTROLLER_QUERY_UNAVAILABLE'}
 Assert-TaskString $taskNativeHost 512
 if($taskNativeHost.StartsWith('\\?\',[StringComparison]::Ordinal)){$taskNativeHost=$taskNativeHost.Substring(4)}
 if(-not $taskNativeHost.Equals([IO.Path]::Join($taskRuntime,'pwsh.exe'),[StringComparison]::OrdinalIgnoreCase)){throw 'EXACT_NATIVE_CONTROLLER'}
'''
runner=runner[:host_start]+host_successor+runner[host_end:]
start=runner.index('function Invoke-TaskChild(')
end=runner.index('function Add-TaskChildCleanup(',start)
runner=runner[:start]+fragment[fragment.index('function Spend-TaskSupervisor('):]+ '\n'+runner[end:]
runner=replace_once(runner,'$taskMaximum=1+1+1+2+2+2+4','$taskMaximum=15')
runner=replace_once(runner,'# cancel1 + reap1 + settlement1 + two reads + two flushes + two positions\n # + four settled owners. Quarantine replaces those four owner disposals.','# R14 fixed mask order has at most14 prefix categories (reap bits\n # mutually exclusive); late tail/quarantine extension has at most15 total.')
runner=replace_once(runner,"$taskOpaqueRoot=[IO.Path]::Join($taskRoot,'opaque-temp')","$taskOpaqueRoot=[IO.Path]::Join($taskRoot,'opaque-temp');$taskTransportRoot=[IO.Path]::Join($taskRoot,'transport')")
runner=replace_once(runner,"$taskOwnNames=@('clock_stage.cpp','run-clock.ps1','source-costs.h','README.md')","$taskOwnNames=@('clock_stage.cpp','run-clock.ps1','source-costs.h','README.md')\n# Native sources are explicit immutable repo inputs. sup.exe is a separate\n# same-origin binary companion whose exact future bytes/hash are unbound.\n$taskSupervisorBinaryPath=[IO.Path]::Join($taskBase,'sup.exe')")
runner=replace_once(runner,"foreach($taskName in $taskManifest['stage_files']){Add-TaskPathCatalogueEntry ([IO.Path]::Join($taskStageRoot,$taskName)) $false}","foreach($taskName in $taskManifest['stage_files']){Add-TaskPathCatalogueEntry ([IO.Path]::Join($taskStageRoot,$taskName)) $false}\n foreach($taskName in $taskManifest['supervisor']['transport_files']){Add-TaskPathCatalogueEntry ([IO.Path]::Join($taskTransportRoot,$taskName)) $false}\n Add-TaskPathCatalogueEntry $taskSupervisorBinaryPath $false")
runner=replace_once(runner,"+$taskManifest['profile']['declared_failure_extra_files']","+$taskManifest['profile']['declared_failure_extra_files']+1")
runner=replace_once(runner,"function Assert-TaskPins{","function Assert-TaskPins{\n Assert-TaskSupervisorBinding")
runner=replace_once(runner,"$taskControlCaps;Assert-TaskRetainedReservation","$taskControlCaps+79472L+[long]$taskManifest['supervisor']['binary']['bytes'];Assert-TaskRetainedReservation")
runner=replace_once(runner,"$taskStageRoot,$taskBuildRoot,$taskOpaqueRoot,","$taskStageRoot,$taskBuildRoot,$taskOpaqueRoot,$taskTransportRoot,")
runner=replace_once(runner,"foreach($taskN in @('stdout.log','stderr.log','receipt.json','consumed.json')){$taskExpectedRunFiles.Add('stage/'+$taskN)}","foreach($taskN in @('stdout.log','stderr.log','receipt.json','consumed.json')){$taskExpectedRunFiles.Add('stage/'+$taskN)};foreach($taskN in $taskManifest['supervisor']['transport_files']){$taskExpectedRunFiles.Add('transport/'+$taskN)}")
runner=replace_once(runner,"$taskArtifacts=[Collections.Generic.List[object]]::new();","$taskArtifacts=[Collections.Generic.List[object]]::new();foreach($taskN in $taskManifest['supervisor']['transport_files']){$taskPath=[IO.Path]::Join($taskTransportRoot,$taskN);$taskCap=if($taskN.EndsWith('.terminal.bin',[StringComparison]::Ordinal)){308L}elseif($taskN -ceq 'child-0.request.bin'){41260L}else{18644L};$taskH=Get-TaskHash $taskPath $taskCap;if($taskN.EndsWith('.terminal.bin',[StringComparison]::Ordinal) -and $taskH.Item1 -ne 308){throw 'SUCCESS_TRANSPORT_TERMINAL_LENGTH'};$taskArtifacts.Add(@{path=$taskPath;bytes=$taskH.Item1;sha256=(Get-TaskHashHex $taskH);origin_bundle_sha256=$taskExpectedManifest})};")
# The helper binary is counted once separately from authored text. Its hash is
# already fenced, and no opaque-compiler-output exception applies to it.
runner=replace_once(runner,"$taskAuthoredBytes;foreach($taskA in $taskArtifacts)","$taskAuthoredBytes+[long]$taskManifest['supervisor']['binary']['bytes'];foreach($taskA in $taskArtifacts)")
runner=replace_once(runner,"$taskTransitive.Add(@{path=$taskManifestPath;bytes=(Get-Item -LiteralPath $taskManifestPath).Length;sha256=$taskExpectedManifest;origin_bundle_sha256=$taskExpectedManifest})","$taskTransitive.Add(@{path=$taskManifestPath;bytes=(Get-Item -LiteralPath $taskManifestPath).Length;sha256=$taskExpectedManifest;origin_bundle_sha256=$taskExpectedManifest});$taskTransitive.Add(@{path=$taskSupervisorBinaryPath;bytes=$taskManifest['supervisor']['binary']['bytes'];sha256=$taskManifest['supervisor']['binary']['sha256'];origin_bundle_sha256=$taskExpectedManifest})")
runner=replace_once(runner,"$taskManifest['inputs'].Count -gt [int]::MaxValue-$taskOwnNames.Count-1","$taskManifest['inputs'].Count -gt [int]::MaxValue-$taskOwnNames.Count-2")
runner=replace_once(runner,"array_entries=($taskManifest['inputs'].Count+$taskOwnNames.Count+1)","array_entries=($taskManifest['inputs'].Count+$taskOwnNames.Count+2)")

for name in ['clock_stage.cpp','source-costs.h']:
    (NEW/name).write_bytes((OLD/name).read_bytes())

readme='''# Clock R14 owned supervisor source proposal

Complete UNRUN source candidate under Accepted ADR0090 and protocol v1. The
runner is in this sibling directory so unchanged clock_stage.cpp retains its
`../waveform-definition-r3/independent_clock.h` include. The five native source
modules live in build-ci/20261106-clock-owned-supervisor-source-r1/native.

All execution guards are false. Target and supervisor Data remain undefined.
No accepted price, source upper, payload fit, actual helper binary, SDK/native
association or execution GO is supplied by this packet. Existing target C++
and source-costs.h are byte-exact R13; every R13 source/catalogue stays frozen.

The runner writes exact binary requests and launches only the pinned console
sup.exe with one uppercase request-path hex argument, an explicit eight-key
environment and no redirected helper streams. A separately qualified fresh
launcher must establish helper null-stdin/noninteractive/error-dialog behavior.
The helper never reads ambient stdin, opens target NUL itself, creates the target
suspended in a creation-time no-breakaway job and admits only the NUL/two-pipe
handle list. Windows job-list/reference/settlement semantics are qualification
premises, not source-only observations. No child creation was attempted.

Two fixed 4096-byte synchronous readers publish one logical result at a time.
The coordinator preserves positive/short/zero-EOF/error-partial distinctions,
fresh complete stage scanning before EVERY actual write, per-child1MiB raw cap,
64MiB stage pool,7200 polls and strict >1800s target-body timeout. Cleanup owns
2000ms target and2000ms worker groups. Cancellation/termination/close does not
prove settlement. Unresolved owners remain rooted through terminal publication
and the nonreturning process-exit boundary; the parent quarantines the run.

Requests have140-byte headers and <=41260/18644-byte extents. Terminals are308
explicit LE bytes, with current manifest/run/target identities, four independently
observed positions, availability bits, ordered primary/secondary facts and exact
counter equations. Unknown parent child-row values are null. Valid observations
survive late helper failure, but only helper exit0 plus a complete status0/1
matrix commits completion. A committed target nonzero exit remains an actual
target refusal; raw259 is valid when actually observed. Terminal/request bytes,
typed arrays and native carriers never enter JSON; only bounded file descriptors
are added to artifacts. All six files and transport directory enter namespace,
failure inventory, complete fences, retained and controlled-scratch proposals.

Helper source is charged from a fresh same-origin runner reservation. Prefix
debits/BeginTail are serialized; sourcePrefixUsed is a PREFIX observation only.
The separately priced mandatory publication/cleanup tail is prepaid; no unused
prefix or process-exit/GC/free/close earns a refund. Existing runner_used remains
the controller ledger including full sponsored helper slices, not measured total
controller/helper execution. All new catalogue values and prices are null/unbound
and must receive a complete independent source census and whole resource proof.

The inherited bootstrap Type[]::EmptyTypes defect is corrected only here to
Type::EmptyTypes. The host check uses Environment.ProcessPath under the selected
fixed fresh-launch/immutable-host profile, rejects null, preserves the prior
leading extended-prefix treatment and exact OrdinalIgnoreCase host comparison.
Outside that profile its caching/growth/native behavior remains unqualified.
These are source changes, not observed target failures or resource measurements.
Other JSON/hash/dependency, negative-control, source/after-binary fence, target
wire and immutable predecessor behavior is retained, subject to fresh review.

Native implementation uses own integer SHA256, fixed typed buffers, SRW admission,
Win32 I/O and ordinary console CRT entry. The catalogue is only an extern
declaration. Explicit requested arrays/attribute buffer/frames/stacks/copies count;
general CRT/CLR/native bookkeeping remains unqualified, never zero or an exemption.
There is no arbitrary stack reserve, reserved padding, cap raise, output silence
law, host process enumeration, Kill(true), shell, foreground/input/audio call,
deliberate crash fixture or runtime measurement in author verification.

Next: independent WHOLE functional review, fresh controller+helper+target census
and six-phase controlled lifetime/native proof; separately reviewed compile-only
SDK/ABI/binary purpose qualification; exact immutable binding; separate root GO.
Own source/string/hash/integer checks establish correspondence only. They do not
parse, import, compile, execute or certify any proposed source.
'''
write(NEW/'README.md',readme)

d=copy.deepcopy(json.loads((OLD/'source-manifest.json').read_bytes()))
d['schema']='YESDAW_CLOCK_ORACLE_R14_OWNED_SUPERVISOR_SOURCE_PROPOSAL_UNBOUND'
d['source_authoring_go']='ADR0090_ACCEPTED_R2_ROOT_EXPLICIT_SOURCE_ONLY_AUTHORITY'
d['exact_source_critic']='PENDING_FRESH_WHOLE_R14_FUNCTIONAL_REVIEW'
d['stage']='AUTHORED_SOURCE_ONLY_ALL_GUARDS_FALSE_UNRUN'
d['root_execution_go']='ABSENT'
native=[record(p) for p in sorted((OUT/'native').glob('*')) if p.suffix in ['.cpp','.h']]
new_inputs=[]
seen={x['path'] for x in d['inputs']}
for p in [*(OLD/n for n in ['run-clock.ps1','source-manifest.json','README.md','clock_stage.cpp','source-costs.h']),*(OUT/x['path'].split(OUT.relative_to(ROOT).as_posix()+'/')[-1] for x in native)]:
    rec=record(p)
    if rec['path'] not in seen: new_inputs.append({k:rec[k] for k in ['path','bytes','sha256']});seen.add(rec['path'])
d['inputs'].extend(new_inputs)
d['closed_directory_inputs'].append({'path':(OUT/'native').relative_to(ROOT).as_posix(),'recursive':False,'files':[p.name for p in sorted((OUT/'native').glob('*')) if p.suffix in ['.cpp','.h']],'entry_count':len(native)})
transport=[f'child-{i}.{kind}.bin' for i in range(3) for kind in ['request','terminal']]
d['supervisor']={
 'bound':False,'native_profile_accepted':False,'controlled_resource_accepted':False,'fresh_launcher_profile_accepted':False,
 'protocol_version':1,'request_header_bytes':140,'request_caps':[41260,18644,18644],'terminal_bytes':308,'transport_cap':79472,
 'binary':{'name':'sup.exe','bytes':None,'sha256':None,'maximum_bytes':33554432,'source_binary_association':'UNBOUND'},
 'native_sources':native,'native_sdk_include_inputs':'UNBOUND_SEPARATE_PINNED_COMPILE_ONLY_INCLUDE_ASSOCIATION_REQUIRED',
 'helper_reservations':[None,None,None],'tail_reservations':[None,None,None],
 'controller_prices':{x:None for x in ['invoke','request','terminal','parent_poll']},
 'parent_poll_limit':None,'parent_watchdog_ms':None,'transport_files':transport,
 'helper_environment':'EXPLICIT_SAME_EIGHT_EXPANDED_VALUES_NO_INHERITED_BLOCK',
 'helper_stdin':'EXTERNAL_FRESH_LAUNCHER_NULL_NONINTERACTIVE_PROFILE_REQUIRED_NO_HELPER_READ',
 'target_stdin':'OWN_INHERITABLE_NUL_READ_HANDLE',
 'request_command':'QUOTED_PINNED_EXE_ONE_SPACE_REQUEST16_UPPER_HEX_EXACT_EOF',
 'Data':'UNDEFINED_EXTERN_DECLARATION_NO_PRICES_NO_ABI_NO_EXECUTION',
 'scratch_scope':'CONTROLLER_PLUS_HELPER_PLUS_TARGET_CONTROLLED_SOURCE_32MiB_UNCHANGED',
 'runtime_native_bookkeeping':'UNQUALIFIED_NOT_ZERO_OR_EXEMPT',
 'source_prefix':'LINEARIZED_OBSERVED_PREFIX_ONLY_ALL_PUBLICATION_CLEANUP_TAIL_PREPAID_NO_REFUND',
 'helper_exit_codes':[0,10,11,12,13],
 'all_current_resource_scalar_native_layout_profile_binding_gates':'HOLD'}
d['R14_owned_supervisor']={
 'predecessor':record(OLD/'source-manifest.json'),'bootstrap_reflection_fix':'[Type]::EmptyTypes',
 'architecture':record(ROOT/'docs/adr/0090-clock-research-owned-process-supervisor.md'),
 'plan':record(ROOT/'docs/plans/2026-11-06-clock-supervisor-source-plan.md'),
 'protocol':record(ROOT/'docs/research/clock-supervisor-protocol-v1.md'),
 'source_only':True,'target_execution':False,'compiler_or_helper_execution':False,
 'wire':'OLD_CHILD_KEYS_WITH_NULL_AVAILABILITY_AND_TRANSPORT_ARTIFACTS_ONLY',
 'census_transfer':False,'resource_fit':False,'source_binary_native_association':False}
d['guards']['supervisorAccepted']=False
d['guards']['supervisorProfileAccepted']=False
d['source_work'].update({k:None for k in ['runner_upper','producer_upper','verifier_upper','whole_upper']})
d['source_work']['R14_status']='FRESH_WHOLE_CONTROLLER_HELPER_BOTH_CHILD_CENSUS_REQUIRED_NO_R13_PRICE_TRANSFER'
d['profile']['input_count']=len(d['inputs'])
d['profile']['input_bytes']=sum(x['bytes'] for x in d['inputs'])
d['profile']['input_hash_chunks']=sum((x['bytes']+65535)//65536 for x in d['inputs'])
d['profile']['include_input_count']=len(d['inputs'])+1
d['profile']['complete_current_inputs']=len(d['inputs'])+4+1+1
d['profile']['directory_namespaces_count']=len(d['closed_directory_inputs'])
d['profile']['directory_namespace_entries']+=len(native)
d['profile']['directory_namespace_files']+=len(native)
d['profile']['declared_success_run_files']+=6
d['profile']['root_walk_entries_max']+=7
d['profile']['failure_inventory_max_files']+=7
d['profile']['maximum_complete_fence_identities']=len(d['inputs'])+4+1+d['profile']['declared_success_run_files']+d['profile']['declared_failure_extra_files']+1
d['profile']['R14_pending_native_sdk_and_binary_inputs']=True
d['profile']['profile_source']='PROVISIONAL_R14_SOURCE_UNION_ONLY_NATIVE_SDK_BINARY_AND_FINAL_CANONICAL_UNION_UNBOUND'
d['retention']['input_bytes']=d['profile']['input_bytes']
d['retention']['reservation_upper']=d['profile']['input_bytes']+1048576+152627224+67108864+67108864+33652457+79472+33554432
d['retention']['headroom_under_840MiB']=880803840-d['retention']['reservation_upper']
d['retention']['R14_transport_bytes_upper']=79472
d['retention']['R14_helper_binary_bytes']='ACTUAL_FUTURE_PIN_REQUIRED_NOT_ZERO'
d['controlled_lifetime']['R14_status']='ALL_CONTROLLER_SUPERVISOR_TARGET_OWNERS_JOIN_PENDING_NO_PHYSICAL_FREE_CREDIT'
d['failure_policy']['R14_cleanup_entries_max']=15
d['author_verification']={'kind':'OWN_SOURCE_FILE_HASH_INTEGER_ONLY','target_parser_AST_import':False,'compiler_program_helper_stage':False,'first_complete_own_pass':'RECORDED_EXTERNALLY_IN_CHECKS_AND_FREEZE_JSON_NO_EXECUTION_CREDIT'}

# Source/manifest/bootstrap-size dependencies are handled as an ordinary byte
# fixed point. Hashes are not guessed and the manifest never contains its hash.
for iteration in range(20):
    count=nodes(d)
    current=re.sub(r'Assert-TaskRawJsonBytes \$taskBootstrap \d+ 512 4',f'Assert-TaskRawJsonBytes $taskBootstrap {count} 512 4',runner)
    write(NEW/'run-clock.ps1',current)
    d['sources']=[record(NEW/name,name) for name in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md']]
    d['sources_excluding_manifest']=4
    if nodes(d)==count: break
else: raise ValueError('bootstrap fixed point did not settle')
write(NEW/'source-manifest.json',dump(d))
summary={'candidate':NEW.relative_to(ROOT).as_posix(),'sources':[record(NEW/n) for n in ['run-clock.ps1','source-manifest.json','README.md','clock_stage.cpp','source-costs.h']],
 'native':native,'bootstrap_nodes':nodes(d),'bootstrap_depth_string':measure(d),'input_count':len(d['inputs']),'input_bytes':sum(x['bytes'] for x in d['inputs']),
 'source_only':True,'no_parser_compiler_program_execution':True,'own_complete_checks':'EXTERNAL_CHECKS_AND_FREEZE_RECORD_REQUIRED','native_sdk_union':'EXPLICIT_UNBOUND_PENDING_PINNED_COMPILE_ONLY_INCLUDE_QUALIFICATION'}
write(OUT/'authoring.json',dump(summary))
print(json.dumps({'written':summary['candidate'],'bootstrap_nodes':summary['bootstrap_nodes'],'depth_string':summary['bootstrap_depth_string'],'own_complete_checks':'NOT_YET_RUN'}))
