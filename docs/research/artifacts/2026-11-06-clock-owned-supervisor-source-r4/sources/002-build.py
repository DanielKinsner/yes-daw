"""Complete R17 UNRUN source successor and own source/file/integer gate.

No old author/checker/model is imported or executed. No target parser/compiler.
"""
from pathlib import Path
from collections import deque
import copy,hashlib,json,re
HERE=Path(__file__).resolve().parent;ROOT=HERE.parent.parent
OLD=ROOT/'build-ci/20261106-clock-owned-supervisor-source-r3'
BASE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r16-owned-supervisor-source-r1'
NEW=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r17-owned-supervisor-source-r1'
REVIEW=ROOT/'build-ci/20261106-r16-supervisor-functional-independent-review-r1'
NAMES=['sup.h','sup_codec.cpp','sup_core.cpp','sup_io.cpp','sup_main.cpp']
OWN=['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md']
CHECKS=[]
def check(name,condition):
    CHECKS.append(dict(name=name,passed=bool(condition)))
    if not condition:raise AssertionError(name)
def load(p):return json.loads(p.read_text(encoding='utf-8'))
def write(p,text):p.write_bytes(text.encode('utf-8'))
def dump(p,value):write(p,json.dumps(value,indent=2,ensure_ascii=True)+'\n')
def rec(p,name=None):
    h=hashlib.sha256();size=0
    with p.open('rb') as f:
        while chunk:=f.read(1048576):h.update(chunk);size+=len(chunk)
    return dict(path=name or (p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix()),bytes=size,sha256=h.hexdigest().upper())
def source_rec(p,name=None):
    d=rec(p,name);d['lines']=p.read_bytes().count(b'\n');return d
def nodes(v):
    if isinstance(v,dict):return 1+sum(1+nodes(x) for x in v.values())
    if isinstance(v,list):return 1+sum(nodes(x) for x in v)
    return 1
def geometry(v,depth=0):
    high=depth;units=0
    if isinstance(v,dict):
        for k,x in v.items():
            d,u=geometry(x,depth+1);high=max(high,d);units=max(units,len(k.encode('utf-16-le'))//2,u)
    elif isinstance(v,list):
        for x in v:
            d,u=geometry(x,depth+1);high=max(high,d);units=max(units,u)
    elif isinstance(v,str):units=len(v.encode('utf-16-le'))//2
    return high,units

check('exact_R16_source_pins',rec(OLD/'source-pins.json')['sha256']=='4ED6C4F3EF0FCA1A7066C9FEA12CD9D2706AF9B956486EC6783D133C2345553C')
check('exact_R16_whole_freeze',rec(OLD/'freeze.json')['sha256']=='0922A22B37064DC71A0F760EE655FBA2FF9ABF5679E8D7628C89E9320EFE9CB0')
check('exact_R16_runner',rec(BASE/'run-clock.ps1')['sha256']=='86415D55AA1CF8ABED45A4E0879272C739BD5A16C0056534288B339C7DCF96BB')
check('exact_R16_manifest',rec(BASE/'source-manifest.json')['sha256']=='93220F660F085B85CDDBE4DD14AE7F61B0648DB6E791E0ED019BB7D4A30441A9')
old_manifest=load(BASE/'source-manifest.json')
protected=[BASE/n for n in [*OWN,'source-manifest.json']]+[OLD/'native'/n for n in NAMES]
protected += [OLD/'source-pins.json',OLD/'freeze.json']+[REVIEW/n for n in ['review.md','review.json','source-pins.json','first-green-review-freeze.json','native-progress-holds.md']]
protected_before=[rec(p) for p in protected]
accepted_doc_paths=['docs/adr/0090-clock-research-owned-process-supervisor.md','docs/adr/0091-clock-supervisor-job-termination-diagnostic.md','docs/research/clock-supervisor-protocol-v1.md','docs/research/clock-supervisor-protocol-v2.md','docs/plans/2026-11-06-clock-supervisor-source-plan.md']
docs_before=[rec(ROOT/p) for p in accepted_doc_paths]

NEW.mkdir(exist_ok=True);(HERE/'native').mkdir(exist_ok=True)
old_namespace='yesdaw_clock_supervisor_r3';new_namespace='yesdaw_clock_supervisor_r4'
old_guard='''            if (!same_name(entry.cFileName,L".",1) && !same_name(entry.cFileName,L"..",2)) {'''
new_guard='''            if (same_name(entry.cFileName,L".",1)) {
                if (seen_dot) { ok=path_failure(state,Site::StageAdmission,Reason::StageExtent); break; }
                seen_dot=true;
            } else if (same_name(entry.cFileName,L"..",2)) {
                if (seen_dotdot) { ok=path_failure(state,Site::StageAdmission,Reason::StageExtent); break; }
                seen_dotdot=true;
            } else {'''
old_locals='        U64 sum=0; U32 count=0; bool ok=true;'
new_locals=old_locals+'\n        bool seen_dot=false,seen_dotdot=false;'
for name in NAMES:
    original=(OLD/'native'/name).read_bytes().decode('utf-8')
    current=original.replace(old_namespace,new_namespace)
    check('native_namespace_present_'+name,old_namespace in original and old_namespace not in current)
    if name=='sup_io.cpp':
        check('one_old_dot_guard',current.count(old_guard)==1)
        check('one_old_scan_locals',current.count(old_locals)==1)
        current=current.replace(old_locals,new_locals,1).replace(old_guard,new_guard,1)
    write(HERE/'native'/name,current)
for name in ['clock_stage.cpp','source-costs.h']:(NEW/name).write_bytes((BASE/name).read_bytes())
write(NEW/'README.md',(HERE/'candidate-readme.md').read_text(encoding='utf-8'))
runner=(BASE/'run-clock.ps1').read_bytes().decode('utf-8')

d=copy.deepcopy(old_manifest)
# Whole previous input prefix is preserved by physical identity. Add previous
# own five and new current native five; no equal-content path deduplication.
existing={x['path'] for x in d['inputs']}
added=[source_rec(BASE/name) for name in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json']]
added += [source_rec(HERE/'native'/name) for name in NAMES]
for row in added:
    check('new_distinct_input_'+row['path'],row['path'] not in existing);existing.add(row['path']);d['inputs'].append(row)
d['supervisor']['native_sources']=[source_rec(HERE/'native'/name) for name in NAMES]
d['closed_directory_inputs'].append(dict(path=(HERE/'native').relative_to(ROOT).as_posix(),recursive=False,files=sorted(NAMES),entry_count=5))
sup=d['supervisor']
sup['stage_scan_record_bound']={'scope':'SOURCE_CONTROL_FLOW_ONLY_NO_NATIVE_ITERATOR_MULTIPLICITY_PREMISE',
    'ordinary_records_before_existing_refusal':7,'first_exact_dot_records':1,'first_exact_dotdot_records':1,
    'maximum_loop_bodies':10,'maximum_FindFirst_calls':1,'maximum_FindNext_calls':9,
    'repeat_diagnostic':'EXISTING_STAGEADMISSION_SITE5_STAGEEXTENT_REASON21_NONCAP',
    'prices_native_resources_ABI_binding_fit_GO':'UNBOUND_HOLD'}
sup['ticket_prices']='ALL_FRESH_R17_NATIVE_FAMILIES_AND_COMPLETE_ADMITTED_GROUP_TAIL_CENSUS_UNBOUND_NO_R16_PRICE_TRANSFER'
p=d['profile']
p['input_count']=len(d['inputs']);p['input_bytes']=sum(x['bytes'] for x in d['inputs'])
p['input_hash_chunks']=sum((x['bytes']+65535)//65536 for x in d['inputs'])
p['include_input_count']=p['input_count']+len(d['current_include_inputs'])
p['complete_current_inputs']=p['input_count']+len(d['sources'])+2
p['maximum_complete_fence_identities']=p['complete_current_inputs']+p['declared_success_run_files']+p['declared_failure_extra_files']
p['directory_namespaces_count']=len(d['closed_directory_inputs'])
p['directory_namespace_entries']=sum(x['entry_count'] for x in d['closed_directory_inputs'])
p['directory_namespace_files']=sum(len(x['files']) for x in d['closed_directory_inputs'])
p['profile_source']='PROVISIONAL_R17_SOURCE_UNION_ONLY_NATIVE_SDK_BINARY_AND_FINAL_CANONICAL_UNION_UNBOUND'
d['closed_json_domain']['output_arrays_max_entries']=p['complete_current_inputs']
d['retention']['input_bytes']=p['input_bytes']
d['retention']['reservation_upper']=sum([p['input_bytes'],d['retention']['own5_reserved'],d['retention']['reference_pool'],d['retention']['compiler_admitted_final_pool'],d['retention']['stage_pool'],d['retention']['control_pool'],sup['transport_cap'],sup['binary']['maximum_bytes']])
d['retention']['headroom_under_840MiB']=d['retention']['cap']-d['retention']['reservation_upper']
d['source_work']['R17_status']='FRESH_WHOLE_R17_ALL_LANGUAGE_STAGEENTRY_DOT_GUARD_AND_ADMITTED_COMPLETION_TAIL_CENSUS_REQUIRED'
d['source_work']['complete_census']='HOLD_PENDING_WHOLE_R17_SOURCE_BODY_CALLER_DEFAULT_COPY_ERROR_PREFIX_AND_MANDATORY_TAIL_PRICES'
d['controlled_lifetime']['R17_status']='TWO_ACTUAL_LOCAL_SEEN_FLAGS_PLUS_CHANGED_SOURCE_UNION_ALL_SOURCE_ABI_NATIVE_RESOURCE_JOINS_PENDING'
d['source_corrections_correspondence']['pending']='Fresh whole R17 functional critic, source census/native/ABI/profile/resources/canonical union/binding and separate root one-attempt GO. No prices or limits bound.'
d['R17_corrections']={'predecessor_manifest':rec(BASE/'source-manifest.json'),
    'whole_R16_review_pins':rec(REVIEW/'source-pins.json'),'whole_R16_review_freeze':rec(REVIEW/'first-green-review-freeze.json'),
    'scope':'TWO_LOCAL_SEEN_DOT_FLAGS_BOUND_CURRENT_STAGE_SCAN_WITH_EXISTING_NONCAP_STAGEEXTENT_DIAGNOSTIC',
    'no_observed_repeated_dot_runtime_failure':True,'all_previous_sources_reviews_attempts':'FROZEN_UNMODIFIED',
    'target_CPP_header_parent_bodies_protocol_caps_controls_arguments_environment':'UNCHANGED_EXCEPT_DERIVED_BOOTSTRAP_NODE_LITERAL',
    'old_prices_native_ABI_resources_fit_binding_GO_transferred':False}
d['author_verification']={'kind':'OWN_SOURCE_FILE_HASH_JSON_INTEGER_FINITE_ABSTRACT_TRANSITIONS_ONLY','target_parser_AST_import':False,
    'compiler_native_API_helper_fixture_Clock':False,'first_complete_own_pass':'EXTERNAL_CHECKS_AND_FREEZE_REQUIRED'}
for iteration in range(24):
    count=nodes(d)
    current,changes=re.subn(r'Assert-TaskRawJsonBytes \$taskBootstrap \d+ 512 4',f'Assert-TaskRawJsonBytes $taskBootstrap {count} 512 4',runner)
    check('one_bootstrap_seam_'+str(iteration),changes==1)
    write(NEW/'run-clock.ps1',current)
    d['sources']=[source_rec(NEW/name,name) for name in OWN]
    if nodes(d)==count:break
else:raise AssertionError('bootstrap fixed point not closed')
check('current_JSON_geometry',geometry(d)[0]<=4 and geometry(d)[1]<=512)
dump(NEW/'source-manifest.json',d)

# Independent finite category transition model. This executes integer/Boolean
# tuples only, never supervisor source or a Win32 fixture. All32 reachable states
# and all four possible record categories are enumerated, including duplicates.
def transition(state,category):
    ordinary,dot,dotdot=state
    if category=='dot':return (None,'StageExtent') if dot else ((ordinary,True,dotdot),None)
    if category=='dotdot':return (None,'StageExtent') if dotdot else ((ordinary,dot,True),None)
    if category=='legal':return (None,'StageUnknownName') if ordinary==7 else ((ordinary+1,dot,dotdot),None)
    return None,'StageUnknownName'
categories=['dot','dotdot','legal','illegal'];states={(0,False,False)};pending=deque(states);edges=[]
while pending:
    state=pending.popleft();rank=7-state[0]+int(not state[1])+int(not state[2])
    for category in categories:
        nxt,reason=transition(state,category)
        if nxt is not None:
            next_rank=7-nxt[0]+int(not nxt[1])+int(not nxt[2])
            check('rank_'+str(state)+'_'+category,next_rank==rank-1 and next_rank>=0)
            if nxt not in states:states.add(nxt);pending.append(nxt)
        else:check('existing_reason_'+str(state)+'_'+category,reason in ['StageExtent','StageUnknownName'])
        edges.append(dict(state=state,category=category,next=nxt,reason=reason,noncap=True,
                          terminal_local_ordinary_count=(state[0]+1 if category in ['legal','illegal'] else state[0]) if nxt is None else None))
check('complete_32_states',states=={(n,a,b) for n in range(8) for a in [False,True] for b in [False,True]})
check('all_128_edges',len(edges)==32*4)
check('all_accepted_transitions_decrease_rank',sum(x['next'] is not None for x in edges)==60)
check('all_repeated_dot_states_refuse',all(transition((n,True,b),'dot')==(None,'StageExtent') for n in range(8) for b in [False,True]))
check('all_repeated_dotdot_states_refuse',all(transition((n,a,True),'dotdot')==(None,'StageExtent') for n in range(8) for a in [False,True]))
check('all_eighth_ordinary_states_unchanged',all(transition((7,a,b),'legal')==(None,'StageUnknownName') for a in [False,True] for b in [False,True]))
# Every category sequence either terminates or has at most nine continuations.
# Dynamic longest-depth traversal is complete over all32 states, not a sample.
longest={(0,False,False):0}
for rank in range(9,-1,-1):
    for state in list(states):
        if state not in longest or 7-state[0]+int(not state[1])+int(not state[2])!=rank:continue
        for category in categories:
            nxt,_=transition(state,category)
            if nxt is not None:longest[nxt]=max(longest.get(nxt,0),longest[state]+1)
check('nine_maximum_continuations',max(longest.values())==9)
check('tenth_body_must_refuse',all(transition((7,True,True),c)[0] is None for c in categories))
regressions=dict(kind='FINITE_INTEGER_BOOLEAN_CATEGORY_MODEL_ONLY_NOT_RUNTIME_FIXTURE',states=sorted(states),edges=edges,
    ranking='(7-ordinary_count)+(!seen_dot)+(!seen_dotdot), initially9, decreases1 on every continuation',
    upper=dict(loop_bodies=10,FindFirst=1,FindNext=9),all_abstract_categories_complete=True,
    interruption_prefixes=['StageEntry spend refusal before category -> break','FindNext EOF/error -> existing break','FindClose true -> existing owner clear','FindClose false -> retained owner blocks later FindFirst'],
    no_native_sequence_observed=True,old_unbounded_abstract_stutter='R16 did not source-count dot records; no actual repeated-dot API failure is claimed')
dump(HERE/'regressions.json',regressions)

# Whole source correspondence, including the full predecessor paths and all
# 88 parent helper blocks. Current parent code differs only at bootstrap literal.
def helpers(text):
    starts=list(re.finditer(r'(?m)^function\s+([\w-]+)',text))
    main_start=text.index('\ntry{\n # Hash literal manifest')+1
    return {m.group(1):text[m.start():starts[i+1].start() if i+1<len(starts) else main_start] for i,m in enumerate(starts)}
old_runner=(BASE/'run-clock.ps1').read_bytes().decode('utf-8');new_runner=(NEW/'run-clock.ps1').read_bytes().decode('utf-8')
old_bodies,new_bodies=helpers(old_runner),helpers(new_runner)
check('88_helpers_both',len(old_bodies)==len(new_bodies)==88)
for name,body in old_bodies.items():check('helper_exact_'+name,new_bodies[name]==body)
check('only_runner_literal_changed',re.sub(r'(Assert-TaskRawJsonBytes \$taskBootstrap )\d+( 512 4)',r'\1NODE_COUNT\2',old_runner)==re.sub(r'(Assert-TaskRawJsonBytes \$taskBootstrap )\d+( 512 4)',r'\1NODE_COUNT\2',new_runner))
for name in ['clock_stage.cpp','source-costs.h']:check('target_exact_'+name,(BASE/name).read_bytes()==(NEW/name).read_bytes())
check('target_include_preserved',b'../waveform-definition-r3/independent_clock.h' in (NEW/'clock_stage.cpp').read_bytes())
for name in NAMES:
    expected=(OLD/'native'/name).read_bytes().decode('utf-8')
    actual=(HERE/'native'/name).read_bytes().decode('utf-8').replace(new_namespace,old_namespace)
    if name=='sup_io.cpp':expected=expected.replace(old_locals,new_locals,1).replace(old_guard,new_guard,1)
    check('whole_native_exact_delta_'+name,actual==expected)
io=(HERE/'native/sup_io.cpp').read_text();scan=io.split('bool stage_scan(',1)[1].split('bool observe_position(',1)[0]
check('spend_precedes_new_flags_use',scan.index('if (!spend(state,Family::StageEntry))')<scan.index('if (same_name(entry.cFileName,L".",1))'))
check('two_local_flags_once',scan.count('bool seen_dot=false,seen_dotdot=false;')==1)
check('both_repeat_refusals_exact',scan.count('ok=path_failure(state,Site::StageAdmission,Reason::StageExtent); break;')==2)
check('ordinary_eighth_rule_preserved','if (++count>7 || !allowed_stage_name(entry.cFileName))' in scan)
check('find_calls_source_unchanged',scan.count('FindFirstFileW(')==scan.count('FindNextFileW(')==scan.count('FindClose(')==1)
check('retained_search_owner_guard',scan.index('if (state.search_owner!=INVALID_HANDLE_VALUE) return false;')<scan.index('FindFirstFileW('))
check('failed_FindClose_retained','else state.search_owner=INVALID_HANDLE_VALUE;' in scan)
path_failure=io.split('bool path_failure(',1)[1].split('bool same_name(',1)[0]
check('existing_noncap_path_failure','state.terminal.flags |= StageGuardRejected;' in path_failure and 'fail(state, site, reason, known, error);' in path_failure and 'StagePoolCap' not in path_failure)
main=(HERE/'native/sup_main.cpp').read_text();core=(HERE/'native/sup_core.cpp').read_text();header=(HERE/'native/sup.h').read_text()
check('late_failure_preserved','if (state.record_frozen) state.tail_failed = true;' in core and 'if (!published || state.tail_failed) finish(state,11);' in main)
check('no_omission_added_in_repeat_guard','native_count' not in new_guard and 'omitted' not in new_guard and 'StagePoolCap' not in new_guard)
check('whole_prewrite_and_shortwrite_retained','if (!stage_scan(state,length-at,extent))' in io and 'if (!admit_write(state)) return false;' in io)
check('Data_declaration_only','extern const Catalogue Data;' in header and not re.search(r'const\s+Catalogue\s+Data\s*[={]',header))
check('all_native_gates_false',all('inline constexpr bool '+n+' = false;' in header for n in ['source_review_accepted','source_work_accepted','native_profile_accepted','controlled_resource_accepted','execution_accepted']))
check('all_parent_guards_false',all(re.search(r'\$'+n+r'=\$false',new_runner) for n in ['taskSourceReviewAccepted','taskSourceWorkAccepted','taskExecutionAccepted','taskCompilerAccepted','taskProduceAccepted','taskVerifyAccepted','taskSupervisorAccepted','taskSupervisorProfileAccepted']))
check('all_manifest_guards_false',all(x is False for x in d['guards'].values()))
check('unbound_all_prices',all(d['source_work'][n] is None for n in ['runner_upper','producer_upper','verifier_upper','whole_upper']) and sup['helper_reservations']==[None]*3 and sup['tail_reservations']==[None]*3)
check('target_Catalogue_still_undefined','static const Catalogue reviewed_;' in (NEW/'source-costs.h').read_text())
for key in ['compiler','environment','references','compiler_final_outputs','control_files','stage_files','normal_gate_controls','module_controls']:
    check('preserved_domain_'+key,d[key]==old_manifest[key])
for key in ['protocol_version','request_header_bytes','request_caps','terminal_bytes','transport_cap','transport_files','helper_exit_codes','binary','native_family_count','normal_no_progress_consumptions_upper','pending_no_progress_slots_upper','zero_native_read_results_upper']:
    check('supervisor_policy_'+key,sup[key]==old_manifest['supervisor'][key])
check('unaltered_failure_policies',d['failure_policy']==old_manifest['failure_policy'])
check('whole_R16_input_prefix',d['inputs'][:len(old_manifest['inputs'])]==old_manifest['inputs'])
check('exact1260_inputs',len(d['inputs'])==1260 and len({r['path'] for r in d['inputs']})==1260)
check('namespace35',p['directory_namespaces_count']==35 and p['directory_namespace_entries']==1112 and p['directory_namespace_files']==1065)
check('separate_current_and_prospective',p['complete_current_inputs']==1266 and p['maximum_complete_fence_identities']==1341)
check('recipe_limits_unchanged',p['declared_success_run_files']==74 and p['declared_failure_extra_files']==1 and p['root_walk_entries_max']==85 and p['failure_inventory_max_files']==85)
check('bootstrap_literal_exact',f'Assert-TaskRawJsonBytes $taskBootstrap {nodes(d)} 512 4' in new_runner)
check('manifest_roundtrip',load(NEW/'source-manifest.json')==d)
check('own5_byte_admission',sum((NEW/n).stat().st_size for n in [*OWN,'source-manifest.json'])<=1048576)
for name in OWN:
    current=source_rec(NEW/name,name);descriptor=next(r for r in d['sources'] if r['path']==name)
    check('own_source_descriptor_'+name,current==descriptor)
for row in d['inputs']:
    path=Path(row['path']);path=path if path.is_absolute() else ROOT/path
    actual=rec(path);check('input_pin_'+row['path'],actual['bytes']==row['bytes'] and actual['sha256']==row['sha256'].upper())
for ns in d['closed_directory_inputs']:
    path=Path(ns['path']);path=path if path.is_absolute() else ROOT/path
    candidates=list(path.rglob('*')) if ns['recursive'] else list(path.iterdir())
    check('namespace_entries_'+ns['path'],len(candidates)==ns['entry_count'])
    observed=sorted(x.relative_to(path).as_posix() for x in candidates if x.is_file())
    check('namespace_files_'+ns['path'],observed==sorted(ns['files']))
for before,path in zip(protected_before,protected):check('protected_'+before['path'],rec(path)==before)
for before,path in zip(docs_before,accepted_doc_paths):check('accepted_doc_'+path,rec(ROOT/path)==before)
for path in [*sorted((HERE/'native').glob('*')),*sorted(NEW.glob('*'))]:check('LF_'+path.name,b'\r' not in path.read_bytes())

subjects=[source_rec(NEW/name) for name in [*OWN,'source-manifest.json']]+[source_rec(HERE/'native'/name) for name in NAMES]
correspondence=dict(subjects=subjects,parent_helpers=88,all_parent_body_bytes_unchanged=True,runner_only_change='DERIVED_BOOTSTRAP_NODE_LITERAL',
    native_delta='PRIVATE_NAMESPACE_R4_IN_ALL5_PLUS_TWO_LOCAL_DOT_FLAGS_AND_EXISTING_NONCAP_REPEAT_REFUSALS_IN_SUP_IO_ONLY',
    stage_record_upper={'bodies':10,'FindFirst':1,'FindNext':9},bootstrap={'nodes':nodes(d),'depth':geometry(d)[0],'max_string_units':geometry(d)[1]},
    profile=p,retention_arithmetic_only=d['retention'],preserved_domains=['target_CPP_header_relative_include','all88parent_bodies','protocolv2_wire_fields','old_controls_arguments_environment','raw_cap_stage_cap_deadlines_cleanup_tickets_owners_and_quarantine'],
    source_prices_resources_native_ABI_profile_canonical_union_binding_fit_execution='UNBOUND_HOLD_NO_TRANSFER',
    predecessor_refs=protected_before,accepted_docs=docs_before,target_execution=False)
dump(HERE/'correspondence.json',correspondence)
dump(HERE/'authoring.json',dict(source_only=True,subject=NEW.relative_to(ROOT).as_posix(),subjects=subjects,added_inputs=added,all_previous_bytes_unchanged=True,
    whole_own_check_pending_external_freeze=True,source_price_claims=False,Data=False,execution_GO=False))
dump(HERE/'input-pins.json',dict(shared_immutable_predecessor_and_review=protected_before,accepted_docs=docs_before,
    whole_current_manifest_input_rows=d['inputs'],preservation='RAW_NEW_CHANGED_INPUTS_OUTPUTS_PER_ATTEMPT_PLUS_SHARED_IMMUTABLE_OLD_BYTE_REFERENCES_NO_ANCESTOR_ARCHIVE_COPY'))
proposal=f'''# Complete R17 source proposal — finite stage scan\n\nThis is a complete UNRUN successor to frozen R16. The private namespace is {new_namespace}. The only native algorithm change is two local seen-dot Boolean initializations and exact first/repeat handling inside stage_scan. The first exact dot and dotdot each continue; a repeat calls existing path_failure(StageAdmission,StageExtent), site5/reason21, without a native error. Ordinary eighth/name refusal remains StageUnknownName. No wire, cap, target input domain or accepted policy is changed.\n\nEvery body retains its first StageEntry spend. There are at most seven continued ordinary records plus two first pseudo records. Ranking (7-count)+(!seen_dot)+(!seen_dotdot) starts9 and decreases on every continuation; all32 reachable states and128 abstract category transitions are enumerated. Therefore at most10 loop bodies,1 FindFirst and9 FindNext calls occur, independently of native snapshot uniqueness or repeated-dot multiplicity. This is a source bound, not an observed Windows failure or runtime fixture. Early source refusal, EOF, enumeration error and failed FindClose can only stop the body earlier.\n\nThe old search owner slot and exactly one reached FindClose remain: a failed close retains the owner and blocks a later FindFirst. Complete stage admission before every actual short WriteFile and mandatory publication is unchanged. Repeated dots set the existing non-cap StageGuardRejected path; only the original StagePoolCap branches add omitted length-at. First primary/secondary/native-secondary and post-record late-tail exit11 remain exact. No new API, retry, drainage loop, terminal field, reason or whole-source allowance is introduced. The two real new local values must be freshly priced and included in selected source/stack ownership; no physical layout or free-storage claim is supplied.\n\nAll88 parent helper bodies, target clock_stage.cpp/source-costs.h and their relative independent-clock include are byte-exact R16. Only parent main's bootstrap literal is rederived from final JSON: {nodes(d)} nodes, depth{geometry(d)[0]}, maximum string{geometry(d)[1]} UTF16 units. Current provisional inputs are {p['input_count']}/{p['input_bytes']} bytes/{p['input_hash_chunks']} chunks; {p['directory_namespaces_count']} namespaces contain {p['directory_namespace_entries']} entries/{p['directory_namespace_files']} files. Current identities/array maximum {p['complete_current_inputs']} and prospective catalogue {p['maximum_complete_fence_identities']} follow actual lists; final native SDK/binary/canonical union remains unbound. The R16 input prefix remains exact, with its own five source files and five current native files added as distinct physical identities. Equal bytes never merge paths.\n\nProspective run positions75, prebundle73, final success74, failure extra1, root/failure admission85/depth64 and cleanup15 remain distinct unchanged source meanings. Compiler28/native6 arguments, eight environment values/NUL stdin, protocolv2, all runner/module controls, target1800s/7200 polls, two2000ms cleanup groups, prefix-only tickets and acquired-result completion are preserved. Parent/compiler/producer/verifier routing and before/after fences are unchanged. Retained arithmetic {d['retention']['reservation_upper']} is only the existing pool/union sum with future maximum helper bytes; it is neither actual binary size nor a retained/scratch fit certificate.\n\nThe complete candidate, JSON recomposition, all native normalizations,88 helper blocks, all1260 admitted pins, closed namespaces, guards/unbound fields, finite transition oracle and predecessor/doc preservation are checked by NEW own source/file/hash/JSON/integer code. No old author/checker/model is executed. Every failed invocation preserves its exact new scripts/data/changed subjects/output/exit and references shared immutable predecessor bytes, without recursively copying dependency binaries or ancestor archives. First complete own green freezes all helpers/oracles/source/evidence. A different fresh whole critic follows.\n\nBoth Data remain undefined and all execution guards false. The R16 whole functional acceptance and R13 scalar acceptance grant no R17 source price, limit, all-language prefix/tail reservation, native/ABI/SDK/source-binary/profile/cancellation/creation/time qualification, complete controlled resource/canonical union, binding, fit or GO. No target PowerShell parser/AST/import/compiler/native API-purpose/helper/fixture/Clock/GUI/device/Git has run. Root owns independent acceptance, the heavy lane and Git.\n'''
write(HERE/'proposal.md',proposal)
check('no_price_claims_in_authoring',load(HERE/'authoring.json')['source_price_claims'] is False)
dump(HERE/'checks.json',dict(count=len(CHECKS),failed=0,checks=CHECKS,scope='OWN_SOURCE_FILE_HASH_JSON_INTEGER_FINITE_ABSTRACT_CATEGORY_ONLY_NOT_RUNTIME'))
print(json.dumps(dict(status='COMPLETE_UNRUN_SOURCE_PROPOSAL',checks=len(CHECKS),failed=0,bootstrap_nodes=nodes(d),profile=p,source_prices=False,execution_GO=False)))
