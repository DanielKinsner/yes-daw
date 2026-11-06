"""Independent source/file/data review only; never load/parse/execute a subject."""
from pathlib import Path
import hashlib
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
PACKET = ROOT / 'build-ci/20261106-clock-owned-supervisor-source-r1'
REVS = ROOT / 'build-ci/musical-audio-persistent-planner-certification/source-revisions'
NEW = REVS / 'clock-oracle-r14-owned-supervisor-source-r1'
OLD = REVS / 'clock-oracle-r13-streamed-dependency-source-r1'

def sha(raw):
    return hashlib.sha256(raw).hexdigest().upper()

def file_sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        while True:
            block = f.read(1048576)
            if not block:
                return h.hexdigest().upper()
            h.update(block)

def dump(path, value):
    path.write_bytes((json.dumps(value, indent=2, ensure_ascii=True)+'\n').encode())

def rec(path):
    raw = path.read_bytes()
    return dict(path=path.relative_to(ROOT).as_posix(), bytes=len(raw), sha256=sha(raw))

def functions(text):
    starts = list(re.finditer(r'^function ([A-Za-z0-9-]+)', text, re.M))
    result = {}
    for i, match in enumerate(starts):
        end = starts[i+1].start() if i+1 < len(starts) else text.index('\ntry{', match.end())
        result[match.group(1)] = text[match.start():end]
    return result

if (OUT/'freeze.json').exists():
    raise RuntimeError('First green review already frozen; do not rerun')
attempts = OUT/'attempts'
attempts.mkdir(exist_ok=True)
attempt = attempts/f'attempt-{len(list(attempts.glob("attempt-*")))+1:02d}'
attempt.mkdir()
for name in ['check_review.py', 'review.md', 'findings.json', 'WORKING.md']:
    shutil.copyfile(OUT/name, attempt/name)

checks = []
def check(name, passed, evidence=None):
    checks.append(dict(id=name, passed=bool(passed), evidence=evidence))

pins = json.loads((PACKET/'source-pins.json').read_bytes())
check('candidate-pins', file_sha(PACKET/'source-pins.json') == '88BA2BDA5D1DEB4DC378D82431EAD036A51174D7C9AC11D2B4D03DC621E626A5')
check('candidate-first-green-freeze', file_sha(PACKET/'freeze.json') == 'E825D9CA77CB3A93284EFB99DD7D4D15918F713CA3432371A451664046E4258E')
for name, expected in pins['protected_r13'].items():
    check('protected-r13-'+name, file_sha(OLD/name) == expected)
for name, expected in pins['accepted_docs'].items():
    check('accepted-doc-'+name, file_sha(ROOT/name) == expected)
for row in pins['subjects']+[pins['input_manifest']]:
    path = ROOT/row['path']
    check('subject-'+row['path'], path.stat().st_size == row['bytes'] and file_sha(path) == row['sha256'])

manifest = json.loads((NEW/'source-manifest.json').read_bytes())
old_manifest = json.loads((OLD/'source-manifest.json').read_bytes())
runner = (NEW/'run-clock.ps1').read_text(encoding='utf-8')
old_runner = (OLD/'run-clock.ps1').read_text(encoding='utf-8')
native = {p.name:p.read_text(encoding='utf-8') for p in (PACKET/'native').glob('*') if p.suffix in ('.h','.cpp')}
core, codec, io, main, header = [native[n] for n in ['sup_core.cpp','sup_codec.cpp','sup_io.cpp','sup_main.cpp','sup.h']]
correspondence = json.loads((PACKET/'correspondence.json').read_bytes())
old_funcs, new_funcs = functions(old_runner), functions(runner)
changed = sorted(k for k in old_funcs if old_funcs[k] != new_funcs[k])
added = sorted(new_funcs.keys()-old_funcs.keys())
check('old74-new88', len(old_funcs)==74 and len(new_funcs)==88)
check('whole-changed-functions', changed==sorted(correspondence['changed_old_helpers']))
check('whole-new-functions', added==sorted(correspondence['new_helpers']))
check('whole-unchanged-functions', sorted(k for k in old_funcs if old_funcs[k]==new_funcs[k])==sorted(correspondence['unchanged_old_helpers']))
for name in ['clock_stage.cpp','source-costs.h']:
    check('target-unchanged-'+name, (NEW/name).read_bytes()==(OLD/name).read_bytes())
check('target-relative-clock-header', (NEW/'../waveform-definition-r3/independent_clock.h').is_file())
check('corrected-static-field', '[object].GetMethod(\'GetType\',[Type]::EmptyTypes)' in runner and '[Type[]]::EmptyTypes' not in runner)
check('no-target-execution-guard', not any(manifest['guards'].values()) and manifest['root_execution_go']=='ABSENT')
for name in ['SourceReview','SourceWork','Execution','Compiler','Produce','Verify','Supervisor','SupervisorProfile']:
    check('parent-false-'+name, '$task'+name+'Accepted=$false' in runner)
for name in ['source_review_accepted','source_work_accepted','native_profile_accepted','controlled_resource_accepted','execution_accepted']:
    check('native-false-'+name, 'bool '+name+' = false;' in header)
check('native-data-undefined', 'extern const Catalogue Data;' in header and re.search(r'\bCatalogue\s+Data\s*[={]', '\n'.join(native.values())) is None)
check('profile-provisional', 'PROVISIONAL' in manifest['profile']['profile_source'] and manifest['profile']['R14_pending_native_sdk_and_binary_inputs'])
check('profile-correspondence', manifest['profile']==correspondence['profile'])
rows = manifest['inputs']
check('input-count', len(rows)==1230==manifest['profile']['input_count'])
check('input-byte-sum', sum(r['bytes'] for r in rows)==manifest['profile']['input_bytes'])
check('input-paths-unique', len({r['path'].casefold() for r in rows})==len(rows))
for row in rows:
    path = ROOT/row['path']
    check('input-'+row['path'], path.stat().st_size==row['bytes'] and file_sha(path)==row['sha256'])
old_inputs = {r['path']:r for r in old_manifest['inputs']}
new_inputs = {r['path']:r for r in rows}
for name, row in old_inputs.items():
    check('predecessor-input-preserved-'+name, row==new_inputs[name])

flags = {name:1<<int(bit) for name,bit in re.findall(r'(\w+)\s*=\s*1u\s*<<\s*(\d+)', header)}
check('28-unique-flags', len(flags)==28 and sum(flags.values())==0x0fffffff)
required_names = 'ChildCreated TargetMayHaveRun TargetExitKnown StdoutSettled StderrSettled StdoutEof StderrEof JobEmptyProved StdoutStartKnown StdoutEndKnown StderrStartKnown StderrEndKnown PrefixKnown StdoutErrorPartialKnown StderrErrorPartialKnown StdoutFlushed StderrFlushed CoordinatorCountersKnown'.split()
forbidden_names = 'PrimaryNativeKnown HasSecondary FirstSecondaryOwnNativeKnown FirstNativeSecondaryKnown BodyTimedOut BodyPollLimited RawLogCap StagePoolCap StageGuardRejected SecondaryCountSaturated'.split()
settled_names = 'StdoutSettled StderrSettled JobEmptyProved PrefixKnown CoordinatorCountersKnown'.split()
for label,names,literal in [('required',required_names,251858943),('forbidden',forbidden_names,16576512),('settled',settled_names,134219852)]:
    check('parent-native-mask-'+label, sum(flags[n] for n in names)==literal and str(literal) in runner)
check('request-caps', 140+40*(4+2*512)==41260 and 140+18*(4+2*512)==18644)
check('transport-cap', 41260+2*18644+3*308==79472)
check('raw-cli-cap', 2+512+1+12+4*512+1==2576)
check('environment-cap', sum([7,3,4,10,4,3,6,6])+8+8*512+8+1==4156)
check('environment-final-cell-does-not-use-spare-append', 'state.environment[used++]=0;state.environment_length=used;' in codec)
check('terminal-offsets', correspondence['format']['offsets']['EOF']==308 and correspondence['format']['offsets']['cleanup']==304)
check('v2-request', "'Y','C','S','R','E','Q','0','2'" in codec and "$taskMagic='YCSREQ02'" in runner)
check('v2-terminal', "'Y','C','S','R','E','S','0','2'" in codec and "$taskMagic='YCSRES02'" in runner)
check('v2-domain', 'YESDAW-CLOCK-SUPERVISOR-RUN-V2' in core and 'YESDAW-CLOCK-SUPERVISOR-RUN-V2' in runner)
check('reason48-native', 'case 48:return site==10;' in codec and '48{return ($Site -eq 10)}' in runner)
check('job-query-and-termination-distinct', 'Reason::JobTerminationFailure' in main and 'Reason::JobQueryFailure' in main)
for label,token in [('query-current-empty','accounting.ActiveProcesses==0'),('exit259-not-liveness','GetExitCodeProcess(state.process,&exit)'),('prefix-no-refund','Spend-Task ([long]$taskManifest[\'supervisor\'][\'helper_reservations\'][$taskChildIndex])'),('commit-helper-exit','$taskHelperObserved -and $taskHelperExit -eq 0'),('signed32-bit-reinterpret','$taskSigned-=4294967296L'),('transport-EOF','if($taskFile.ReadByte() -ne -1)'),('unavailable-null','$taskStarted=$null;$taskExitObserved=$null'),('target-result-after-success',"$taskProduced=Read-TaskChildResult $taskProduce")]:
    check('source-contract-'+label, token in runner or token in main)
check('after-fences-retained', new_funcs['Invoke-TaskFencedChild']==old_funcs['Invoke-TaskFencedChild'])
check('no-host-enumeration-killtree', 'Kill($true)' not in runner and 'MainModule' not in runner and 'GetProcesses' not in '\n'.join(native.values()))
check('worker-publication-error-seam', 'worker.exit_failure=failure(Site::Read,Reason::ReadFailure,wait==WAIT_FAILED,error);\n        worker_error(worker,Reason::ReadFailure,wait==WAIT_FAILED,error);' in main)
check('completed-chunk-loss-seam', 'request_tail(state.meter);\n        worker_error(worker,Reason::SourceInvariant);' in main and 'worker.count=0;' in main)
check('ignored-coordinator-wait-failure', 'else if (WaitForSingleObject(worker.thread,0)==WAIT_OBJECT_0' in main)
check('ignored-cleanup-ready-wait-failure', 'if (ready==WAIT_OBJECT_0) consume_ready(state,i,true);\n                acknowledge(state,worker,false);' in main)
check('zero-success-conflated-with-eof', 'if (!ok || got==0)' in main)
check('stage-search-owner-overwrite-seam', 'state.search_owner=search;' in io and 'else state.search_owner=INVALID_HANDLE_VALUE;' in io)
check('stage-retry-omission-seam', 'if (!stage_scan(state,length-at,extent)) return false;' in io)
check('failed-pipe-outputs-adopted-before-success-test', io.index('state.workers[i].pipe=read;') < io.index("if (!ok) { const DWORD error=GetLastError(); fail(state,Site::Containment"))

findings = json.loads((OUT/'findings.json').read_bytes())
check('complete-verdict-is-revise', findings['disposition']=='REVISE' and len(findings['findings'])>=3)
check('no-subject-runtime-credit', not any(findings['execution'].values()))
check('scoped-io-review-present', (ROOT/'build-ci/20261106-r14-io-independent-review-r1/freeze.json').is_file())
io_pins = ROOT/'build-ci/20261106-r14-io-independent-review-r1/source-pins.json'
check('io-critic-pins-match', file_sha(io_pins)==findings['io_review_pins_sha256'])

summary = dict(scope='SOURCE_FILE_DATA_INTEGER_ONLY_NOT_TARGET_EXECUTION', disposition='REVISE', passed=sum(c['passed'] for c in checks), failed=sum(not c['passed'] for c in checks), checks=checks)
dump(attempt/'checks.json', summary)
dump(OUT/'checks.json', summary)
if summary['failed']:
    print(json.dumps(dict(passed=summary['passed'], failed=summary['failed'], failures=[c['id'] for c in checks if not c['passed']])))
    raise SystemExit(1)
source_records = [rec(PACKET/'source-pins.json'),rec(PACKET/'freeze.json'),rec(PACKET/'correspondence.json'), *[rec(ROOT/r['path']) for r in pins['subjects']],rec(NEW/'source-manifest.json'),rec(io_pins),rec(io_pins.parent/'freeze.json')]
dump(OUT/'source-pins.json',dict(subjects=source_records, accepted_docs=pins['accepted_docs'], protected_r13=pins['protected_r13'], input_rows_verified=len(rows), target_execution=False))
own = [rec(OUT/n) for n in ['check_review.py','review.md','findings.json','WORKING.md','checks.json','source-pins.json']]
dump(OUT/'freeze.json',dict(first_passing_invocation=attempt.name, disposition='REVISE', files=own, target_execution=False))
print(json.dumps(dict(passed=summary['passed'],failed=0,disposition='REVISE',pins_sha256=file_sha(OUT/'source-pins.json'),freeze_sha256=file_sha(OUT/'freeze.json'))))
