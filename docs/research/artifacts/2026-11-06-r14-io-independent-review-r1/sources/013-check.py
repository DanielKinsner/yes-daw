"""Source/data-only review. Never parse/import/compile/execute a target."""
from pathlib import Path
import hashlib
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
CANDIDATE = ROOT / 'build-ci/20261106-clock-owned-supervisor-source-r1'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()

def write(name, obj):
    (OUT / name).write_text(json.dumps(obj, indent=2) + '\n', encoding='utf-8', newline='\n')

if (OUT / 'freeze.json').exists():
    raise SystemExit('FROZEN: no second invocation or meaning expansion')
rows = []
def check(label, condition):
    rows.append({'name': label, 'pass': bool(condition)})

pins_path = CANDIDATE / 'source-pins.json'
check('exact frozen candidate pin file', digest(pins_path) == '88BA2BDA5D1DEB4DC378D82431EAD036A51174D7C9AC11D2B4D03DC621E626A5')
candidate_pins = json.loads(pins_path.read_text(encoding='utf-8'))
inputs = []
for subject in candidate_pins['subjects']:
    path = ROOT / subject['path']
    check('candidate subject ' + subject['path'], path.stat().st_size == subject['bytes'] and digest(path) == subject['sha256'])
    inputs.append({'path': subject['path'], 'bytes': path.stat().st_size, 'sha256': digest(path)})
for name, sha in candidate_pins['accepted_docs'].items():
    path = ROOT / name
    check('accepted immutable ' + name, digest(path) == sha)
    inputs.append({'path': name, 'bytes': path.stat().st_size, 'sha256': sha})
for name in ('source-pins.json', 'freeze.json', 'correspondence.json'):
    path = CANDIDATE / name
    inputs.append({'path': path.relative_to(ROOT).as_posix(), 'bytes': path.stat().st_size, 'sha256': digest(path)})

io = (CANDIDATE / 'native/sup_io.cpp').read_text(encoding='utf-8')
main = (CANDIDATE / 'native/sup_main.cpp').read_text(encoding='utf-8')
codec = (CANDIDATE / 'native/sup_codec.cpp').read_text(encoding='utf-8')
header = (CANDIDATE / 'native/sup.h').read_text(encoding='utf-8')
proto = (ROOT / 'docs/research/clock-supervisor-protocol-v1.md').read_text(encoding='utf-8')
functions = re.findall(r'^(?:bool|void)\s+([a-z_]+)\(', io, re.M)
expected = ['live','close_one','path_failure','same_name','allowed_stage_name','native_count','validate_path','read_request','hash_file','stage_scan','observe_position','open_capture_owners','create_contained_target','write_chunk','close_safe_owners','publish_terminal']
check('all16 I/O functions inventoried', functions == expected)
apis = sorted(set(re.findall(r'\b([A-Z][A-Za-z0-9]+)\(', io)))
write('inventory.json', {'functions': functions, 'api_like_calls': apis, 'coverage': 'All385 lines read; all16 functions reviewed; relevant main/codec/core/State contracts read. No target parser was used.'})

checks = {
 'IO1 owner overwritten on new enumeration': 'state.search_owner=search;' in io,
 'IO1 failed close retains old owner': 'else state.search_owner=INVALID_HANDLE_VALUE;' in io,
 'IO1 publication rescans after prefreeze cleanup': 'cleanup_prefix(state);finalize_record(state);' in main and 'const bool published=publish_terminal(state);' in main and 'stage_scan(state,0,extent)' in io,
 'IO1 one source-owned search slot': header.count('HANDLE search_owner = INVALID_HANDLE_VALUE;') == 1,
 'IO2 first cap adds omitted': 'if ((state.terminal.flags&StagePoolCap)!=0) native_count(state,count.omitted,length);' in io,
 'IO2 continuation cap returns without omitted': 'if (!stage_scan(state,length-at,extent)) return false;' in io,
 'IO2 cap contract': 'A stage aggregate-cap refusal sets StagePoolCap and adds N to omitted.' in proto,
 'IO3 join source refusal': 'if(!charge(state.meter,Family::PathGuard) || !charge(state.meter,Family::PathCell,base.length+length)) return false;' in codec,
 'IO3 pattern conversion': 'return path_failure(state,Site::StageAdmission,Reason::StageExtent);' in io,
 'IO3 child conversion': 'if (n==MAX_PATH || !join_path(state,state.request.stage,entry.cFileName,n,child)) { ok=path_failure(state,Site::StageAdmission,Reason::StageExtent); break; }' in io,
 'IO4 successful zero classified EOF': 'if (!ok || got==0) {' in main and 'worker.kind=filled==0?ReadyKind::ZeroEof:ReadyKind::EofShort;' in main,
 'IO4 native broken pipe separately identified': 'if (!ok && error!=ERROR_BROKEN_PIPE)' in main,
 'IO5 failed-pipe outputs copied before success check': io.index('state.workers[i].pipe=read;') < io.index('if (!ok) { const DWORD error=GetLastError(); fail(state,Site::Containment,Reason::NativeOwnerAllocationFailure,true,error);'),
 'IO5 never-started worker still closes pipe': 'if (worker.settled || !worker.created)' in io and 'close_one(state,worker.pipe,1u<<(10u+i));' in io,
 'IO5 process ChildCreated before success check': io.index('state.terminal.flags|=ChildCreated') < io.index('if (!created) {'),
 'no invalid HeapAlloc native-code claim': 'if (state.attribute_allocation==nullptr) { fail(state,Site::Containment,Reason::NativeOwnerAllocationFailure);' in io,
 'all native execution gates false': all('inline constexpr bool '+g+' = false;' in header for g in ('source_review_accepted','source_work_accepted','native_profile_accepted','controlled_resource_accepted','execution_accepted')),
 'immutable publication byte cut': io.index('state.record_frozen=true;') < io.index('state.terminal_file=CreateFileW'),
 'terminal no-overwrite': 'CREATE_NEW,FILE_ATTRIBUTE_NORMAL' in io,
 'exact8-key target environment still normative': '4,156 cells/8,312 bytes' in proto,
}
for key, value in checks.items():
    check(key, value)

models = []
for at in (0,1,4095):
    remaining = 4096-at
    models.append({'case': 'continuation-cap', 'written': at, 'unretained': remaining, 'expected_omitted': remaining, 'candidate_omitted': 0})
    check('IO2 bounded counterexample at'+str(at), remaining > 0 and 0 != remaining and 4096 == at+remaining)
models.append({'case':'search-close-failure','live_owners':['H1','H2'],'tracked_owner':'H2','unrepresented_owner':'H1'})
check('IO1 two live origins cannot fit overwritten one slot', len({'H1','H2'} - {'H2'}) == 1)
models.append({'case':'meter-refused-valid-stage-path','base_units':200,'suffix_units':1,'joined_units':202,'semantic_extent_refused':False,'meter_refused':True,'candidate_extent_flag':True})
check('IO3 valid extent not real refusal', 200+1+1 <= 512)
for filled in (0,17):
    models.append({'case':'true-zero-then-real-data','filled':filled,'native_result':[True,0],'next_peer_data_bytes':3,'candidate_logical_result':'ZeroEof' if filled==0 else 'EofShort then synthetic ZeroEof','lost_future_bytes':3})
    check('IO4 zero-read no closed-writer proof '+str(filled), 3>0)
models.append({'case':'failed-pipe-output','creator_success':False,'raw_word':'unknown nonnull word','candidate_promotes_owner':True,'valid_owner_proved':False})
check('IO5 numeric nonnull does not prove ownership', 'unknown nonnull word' not in {'success-returned pipe handle'})
write('models.json', {'models':models,'qualification':'Pure finite source counterexamples; no native API, helper or fixture was invoked. They are not observed runtime events.'})

sdk = Path('C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um')
for name, token in [('fileapi.h','ReadFile('),('namedpipeapi.h','CreatePipe('),('processthreadsapi.h','CreateProcessW(')]:
    path=sdk/name
    check('selected SDK declaration '+name, token in path.read_text(encoding='utf-8-sig'))
    inputs.append({'path':str(path),'bytes':path.stat().st_size,'sha256':digest(path),'scope':'Source declaration read only; no compiler/native association credit'})

references=[
 ('ReadFile','https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-readfile','The synchronous count is initialized to zero before work. Anonymous closed-writer reads return false with broken-pipe error. A true zero-count pipe read may reflect a peer zero-byte write.'),
 ('CreatePipe','https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-createpipe','Failed-call output parameters are indeterminate; callers may not assume their contents. Zero buffer size requests the system default, not a fixed kernel allocation.'),
 ('Anonymous Pipe Operations','https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations','Anonymous pipes use a uniquely named pipe implementation; synchronous reads can return after peer writes and after all writers close.'),
 ('WriteFile','https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-writefile','Zero requested bytes is a null write whose behavior depends on the underlying communications technology.'),
 ('CreateProcessW','https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw','Nonzero return signals success. PROCESS_INFORMATION receives new-process identification; this page does not promise valid ownership fields on a false return.'),
 ('PROCESS_INFORMATION','https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/ns-processthreadsapi-process_information','The structure identifies a newly created process/thread; the documented handle-close obligation is explicitly described for successful creation.'),
 ('CloseHandle','https://learn.microsoft.com/en-us/windows/win32/api/handleapi/nf-handleapi-closehandle','The input is a valid open-object handle. A numeric nonnull word is not such a proof.'),
 ('HeapFree','https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapfree','Current documentation permits GetLastError after zero failure; no stale-native-error finding applies merely because candidate queries it.'),
 ('HeapAlloc','https://learn.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc','Failure does not set the last error. Candidate correctly records allocation failure without a native code.')]
write('primary-references.json', {'retrieval_date':'2026-11-06','mechanism':'web primary documentation read; paraphrases/inferences only, not archived page bodies','references':[{'title':title,'url':url,'review_use':use} for title,url,use in references]})
check('review contains all5 source findings', all(('**IO'+str(i)+':') in (OUT/'review.md').read_text(encoding='utf-8') for i in range(1,6)))
check('byte-identical repository instructions', digest(ROOT/'AGENTS.md') == digest(ROOT/'CLAUDE.md') == 'AFA45D3BAC0FDB892064C8928DDB21375FA0112D2B5840CF2877FAA472ECEB77')
check('protected unrun recipe test unchanged', digest(ROOT/'tests/musical_audio_call_recipe_tests.cpp') == '97B2A2ABE0BE0CDDFCE173A20E6A6264E837DB751FE204E93B4B094E8E22619C')
write('source-pins.json', {'inputs':inputs,'disposition':'REVISE for complete native I/O review scope only','execution_performed':False})
result={'passed':sum(r['pass'] for r in rows),'failed':sum(not r['pass'] for r in rows),'checks':rows,'candidate_disposition':'REVISE','execution_performed':False}
attempts=OUT/'check-attempts'
attempts.mkdir(exist_ok=True)
attempt=attempts/('attempt-'+str(len(list(attempts.iterdir()))+1).zfill(2))
attempt.mkdir()
for path in OUT.iterdir():
    if path.is_file():
        (attempt/path.name).write_bytes(path.read_bytes())
(attempt/'checks.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
write('checks.json', result)
if result['failed']:
    print(json.dumps({'passed':result['passed'],'failed':result['failed'],'attempt':str(attempt)}))
    raise SystemExit(1)
freeze=[{'path':path.relative_to(ROOT).as_posix(),'bytes':path.stat().st_size,'sha256':digest(path)} for path in sorted(OUT.rglob('*')) if path.is_file()]
write('freeze.json', {'first_green':True,'files':freeze,'candidate_disposition':'REVISE','no_post_green_edits':True,'target_execution_performed':False})
print(json.dumps({'passed':result['passed'],'failed':0,'disposition':'REVISE','source_pins_sha256':digest(OUT/'source-pins.json'),'freeze_sha256':digest(OUT/'freeze.json')}))
