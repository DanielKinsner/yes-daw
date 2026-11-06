"""Own ordinary file/text/JSON authoring only; never parses or runs a target."""
from pathlib import Path
import copy,hashlib,json,re,shutil,traceback,contextlib,sys

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]
REVS=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions'
BASE=REVS/'clock-oracle-r14-owned-supervisor-source-r1'
NEW=REVS/'clock-oracle-r15-owned-supervisor-source-r1'
def sha(raw):return hashlib.sha256(raw).hexdigest().upper()
def record(p,name=None):
    b=p.read_bytes();return {'path':name or p.relative_to(ROOT).as_posix(),'bytes':len(b),'lines':len(b.splitlines()),'sha256':sha(b)}
def write(p,s):p.write_bytes(s.encode())
def dump(p,v):write(p,json.dumps(v,indent=2,ensure_ascii=True)+'\n')
def nodes(v):
    if isinstance(v,dict):return 1+sum(1+nodes(x) for x in v.values())
    if isinstance(v,list):return 1+sum(nodes(x) for x in v)
    return 1
def geometry(v,level=0):
    if isinstance(v,dict):
        a=[geometry(x,level+1) for x in v.values()]
        return max([level+1]+[x[0] for x in a]),max([0]+[len(k.encode('utf-16-le'))//2 for k in v]+[x[1] for x in a])
    if isinstance(v,list):
        a=[geometry(x,level+1) for x in v]
        return max([level+1]+[x[0] for x in a]),max([0]+[x[1] for x in a])
    return level,len(v.encode('utf-16-le'))//2 if isinstance(v,str) else 0
def main():
    if (OUT/'freeze.json').exists():raise RuntimeError('Already frozen')
    NEW.mkdir(exist_ok=True)
    for n in ['clock_stage.cpp','source-costs.h']:(NEW/n).write_bytes((BASE/n).read_bytes())
    runner=(BASE/'run-clock.ps1').read_text()
    write(NEW/'README.md',(OUT/'candidate-readme.md').read_text())
    d=copy.deepcopy(json.loads((BASE/'source-manifest.json').read_bytes()))
    d['schema']='YESDAW_CLOCK_ORACLE_R15_OWNED_SUPERVISOR_COMPLETE_EIGHT_FIX_SOURCE_PROPOSAL_UNBOUND'
    d['stage']='AUTHORED_SOURCE_ONLY_ALL_GUARDS_FALSE_UNRUN'
    d['source_authoring_go']='ROOT_ONE_COMPLETE_R15_SUCCESSOR_ALL_EIGHT_R14_FINDINGS_AND_ANALOGOUS_PATHS'
    d['exact_source_critic']='PENDING_FRESH_WHOLE_R15_FUNCTIONAL_REVIEW'
    native=[record(p) for p in sorted((OUT/'native').glob('*')) if p.suffix in ['.h','.cpp']]
    paths=[*(BASE/n for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','source-manifest.json','README.md']),*(ROOT/x['path'] for x in native)]
    seen={x['path'] for x in d['inputs']}
    for p in paths:
        row=record(p)
        if row['path'] in seen:raise ValueError('duplicate physical new input '+row['path'])
        d['inputs'].append({k:row[k] for k in ['path','bytes','sha256']});seen.add(row['path'])
    d['closed_directory_inputs'].append({'path':(OUT/'native').relative_to(ROOT).as_posix(),'recursive':False,'files':sorted(p.name for p in (OUT/'native').glob('*') if p.suffix in ['.h','.cpp']),'entry_count':5})
    d['supervisor']['native_sources']=native
    d['supervisor']['private_worker_strategy']='IMMUTABLE_READY_SLOT_SEPARATE_EXIT_RECORD_TRUE_ZERO_NOPROGRESS_NORMAL_POLL_CONTINUATION'
    d['supervisor']['unknown_creation']='RAW_UNTRUSTED_BYTES_NOT_HANDLES_CANONICAL_UNAVAILABLE_PID_EXIT_STATUS4'
    d['supervisor']['normal_no_progress_consumptions_upper']=2*7200
    d['supervisor']['pending_no_progress_slots_upper']=2
    d['supervisor']['zero_native_read_results_upper']=2*7200+2
    d['supervisor']['zero_read_bound_status']='SOURCE_CONTROL_SCHEDULE_PROPOSAL_NOT_A_NEW_INPUT_GUARD_OR_ACCEPTED_PRICE'
    d['R15_corrections']={
      'predecessor_manifest':record(BASE/'source-manifest.json'),
      'whole_review_pins':record(ROOT/'build-ci/20261106-r14-supervisor-functional-review-r1/source-pins.json'),
      'whole_review_freeze':record(ROOT/'build-ci/20261106-r14-supervisor-functional-review-r1/freeze.json'),
      'io_review_pins':record(ROOT/'build-ci/20261106-r14-io-independent-review-r1/source-pins.json'),
      'findings':['F1 immutable published slot','F2 completed result retained through publication failure','F3 all wait failures captured','F4 TRUE0 private no-progress continuation','F5 retained owner slot cannot be overwritten','F6 remaining cap omission','F7 typed source refusal versus extent','F8 failed creator outputs are raw unknown provenance'],
      'target_CPP_header_control_argv_environment_unchanged':True,
      'all_old_sources_and_reviews_preserved':True,'old_prices_resources_GO_transferred':False,
      'own_verification':'SOURCE_FILE_HASH_INTEGER_REGRESSIONS_ONLY_RUNTIME_FIXTURES_FOR_LATER_SEPARATE_GO'}
    p=d['profile']
    p['input_count']=len(d['inputs']);p['input_bytes']=sum(x['bytes'] for x in d['inputs'])
    p['input_hash_chunks']=sum((x['bytes']+65535)//65536 for x in d['inputs'])
    p['include_input_count']=p['input_count']+1
    p['complete_current_inputs']=p['input_count']+6
    p['maximum_complete_fence_identities']=p['input_count']+6+p['declared_success_run_files']+p['declared_failure_extra_files']
    p['directory_namespaces_count']=len(d['closed_directory_inputs']);p['directory_namespace_entries']+=5;p['directory_namespace_files']+=5
    p['profile_source']='PROVISIONAL_R15_SOURCE_UNION_ONLY_NATIVE_SDK_BINARY_AND_FINAL_CANONICAL_UNION_UNBOUND'
    d['retention']['input_bytes']=p['input_bytes']
    d['retention']['reservation_upper']=p['input_bytes']+1048576+152627224+67108864+67108864+33652457+79472+33554432
    d['retention']['headroom_under_840MiB']=880803840-d['retention']['reservation_upper']
    d['controlled_lifetime']['R15_status']='NEW_SLOT_EXIT_NOPROGRESS_RAW_UNKNOWN_CREATION_OWNERS_ALL_JOIN_PENDING_NO_FREE_CREDIT'
    d['source_work']['R15_status']='FRESH_WHOLE_R15_CONTROLLER_HELPER_BOTH_CHILD_CENSUS_REQUIRED_NO_R14_ROW_PRICE_TRANSFER'
    d['source_work']['complete_census']='HOLD_PENDING_WHOLE_R15_ALL_TRANSITIVE_BODIES_CALLERS_DEFAULTS_COPIES_ERROR_PATHS_NOPROGRESS_SCHEDULE_AND_PREPAID_TAIL'
    d['author_verification']={'kind':'OWN_SOURCE_FILE_HASH_INTEGER_REGRESSIONS_ONLY','target_parser_AST_import':False,'compiler_helper_fixture_Clock':False,'first_complete_own_pass':'EXTERNAL_CHECKS_AND_FREEZE_RECORD_REQUIRED'}
    for _ in range(20):
        count=nodes(d)
        current,n=re.subn(r'Assert-TaskRawJsonBytes \$taskBootstrap \d+ 512 4',f'Assert-TaskRawJsonBytes $taskBootstrap {count} 512 4',runner)
        if n!=1:raise ValueError('bootstrap seam count')
        write(NEW/'run-clock.ps1',current)
        d['sources']=[record(NEW/n,n) for n in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md']]
        if nodes(d)==count:break
    else:raise ValueError('bootstrap fixed point')
    dump(NEW/'source-manifest.json',d)
    summary={'candidate':NEW.relative_to(ROOT).as_posix(),'sources':[record(NEW/n) for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','source-manifest.json','README.md']],
      'native':native,'bootstrap_nodes':nodes(d),'depth_max_string':geometry(d),'profile':p,
      'no_target_execution':True,'all_prices_native_ABI_profile_resources_binding_GO':'UNBOUND_HOLD','whole_own_check':'EXTERNAL_CHECKS_AND_FREEZE_REQUIRED'}
    dump(OUT/'authoring.json',summary)
    print(json.dumps({'written':summary['candidate'],'bootstrap_nodes':nodes(d),'geometry':geometry(d),'inputs':p['input_count'],'bytes':p['input_bytes']}))

if __name__=='__main__':
    a=OUT/'author-attempts';a.mkdir(exist_ok=True)
    attempt=a/f'attempt-{len(list(a.glob("attempt-*")))+1:02d}';attempt.mkdir()
    inputs=[Path(__file__),OUT/'candidate-readme.md',*sorted((OUT/'native').glob('*')),*sorted(BASE.glob('*'))]
    for source in inputs:
        if source.is_file():
            q=attempt/'inputs'/source.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,q)
    status=0
    with (attempt/'stdout.txt').open('w') as out,(attempt/'stderr.txt').open('w') as err,contextlib.redirect_stdout(out),contextlib.redirect_stderr(err):
        try:main()
        except BaseException:traceback.print_exc();status=1
    dump(attempt/'result.json',{'exit_code':status,'scope':'OWN_FILE_TEXT_JSON_INTEGER_ONLY','target_execution':False})
    print((attempt/'stdout.txt').read_text(),end='')
    if status:print((attempt/'stderr.txt').read_text(),file=sys.stderr,end='')
    raise SystemExit(status)
