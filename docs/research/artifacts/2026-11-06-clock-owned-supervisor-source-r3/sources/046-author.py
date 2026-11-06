"""Fresh complete R16 file/text/JSON authoring. Old checkers/models never run."""
from pathlib import Path
import copy,hashlib,json,re,shutil,contextlib,traceback,sys
OUT=Path(__file__).resolve().parent;ROOT=OUT.parents[1]
REVS=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions'
BASE=REVS/'clock-oracle-r15-owned-supervisor-source-r1'
NEW=REVS/'clock-oracle-r16-owned-supervisor-source-r1'
OLDPACKET=ROOT/'build-ci/20261106-clock-owned-supervisor-source-r2'
REVIEW=ROOT/'build-ci/20261106-r15-supervisor-functional-independent-review-r1'
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def rec(p,name=None):
 b=p.read_bytes();return {'path':name or p.relative_to(ROOT).as_posix(),'bytes':len(b),'lines':len(b.splitlines()),'sha256':sha(b)}
def write(p,s):p.write_text(s,encoding='utf-8',newline='\n')
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
 if (OUT/'freeze.json').exists():raise RuntimeError('FROZEN')
 assert sha((OLDPACKET/'source-pins.json').read_bytes())=='5806F35FE26CC1CB2A43B7E49AD6CEBEDE6C28EE7ABAC6BE52CDBE77292FDDB1'
 assert sha((OLDPACKET/'freeze.json').read_bytes())=='895F4F1BB04F955E9E40BFDC29AC29AE15AB844ECE21AF068D3DFAEF16914169'
 assert sha((REVIEW/'source-pins.json').read_bytes())=='2F455971EAE1F023BA5D49FF295C893736997C846A0D73E71DF29A57A5B3980A'
 assert sha((REVIEW/'first-green-review-freeze.json').read_bytes())=='8D68D9BA4F652BEDD83A64B89C5C459577789631D6F190441F589CED01B14788'
 NEW.mkdir(exist_ok=True)
 for n in ['clock_stage.cpp','source-costs.h']:(NEW/n).write_bytes((BASE/n).read_bytes())
 write(NEW/'README.md',(OUT/'candidate-readme.md').read_text(encoding='utf-8'))
 runner=(BASE/'run-clock.ps1').read_text(encoding='utf-8')
 d=copy.deepcopy(json.loads((BASE/'source-manifest.json').read_bytes()))
 d['schema']='YESDAW_CLOCK_ORACLE_R16_PREFIX_BODY_ADMISSION_CURRENT_METADATA_SOURCE_PROPOSAL_UNBOUND'
 d['stage']='AUTHORED_SOURCE_ONLY_ALL_GUARDS_FALSE_UNRUN'
 d['source_authoring_go']='ROOT_ONE_COMPLETE_R16_SUCCESSOR_WHOLE_R15_F9_F10_AND_ANALOGOUS_ADMISSION_PATHS'
 d['exact_source_critic']='PENDING_FRESH_WHOLE_R16_FUNCTIONAL_REVIEW'
 native=[rec(p) for p in sorted((OUT/'native').iterdir()) if p.suffix in ['.h','.cpp']]
 seen={x['path'] for x in d['inputs']}
 for p in [*(BASE/n for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','source-manifest.json','README.md']),*(ROOT/x['path'] for x in native)]:
  x=rec(p)
  if x['path'] in seen:raise ValueError('duplicate physical new input '+x['path'])
  seen.add(x['path']);d['inputs'].append({k:x[k] for k in ['path','bytes','sha256']})
 d['closed_directory_inputs'].append({'path':(OUT/'native').relative_to(ROOT).as_posix(),'recursive':False,'files':sorted(p.name for p in (OUT/'native').iterdir() if p.suffix in ['.h','.cpp']),'entry_count':5})
 d['supervisor']['native_sources']=native
 header=(OUT/'native/sup.h').read_text(encoding='utf-8')
 family_text=header.split('enum class Family : U32 {',1)[1].split('};',1)[0]
 families=[x.strip() for x in family_text.replace('\n','').split(',') if x.strip()]
 assert families[-1]=='Count'
 d['supervisor']['native_family_count']=len(families)-1
 d['supervisor']['body_admission']='PREFIX_ONLY_TICKET_SAME_METER_LOCK_BEFORE_NEW_SETUP_WORKER_RESUME_POLL_CONSUME_WRITE_GROUP'
 d['supervisor']['admitted_group_completion']='ALREADY_ADMITTED_CALL_OR_SETUP_GROUP_MAY_FINISH_AFTER_CUT_FROM_DERIVED_PREPAID_TAIL_NO_NEW_TICKET'
 d['supervisor']['ticket_prices']='ALL_FRESH_NATIVE_FAMILIES_AND_COMPLETE_GROUP_TAIL_CENSUS_UNBOUND_NO_R15_TRANSFER'
 # The current physical namespace has a prospective recipe, a success recipe
 # before the final bundle and a final success recipe. These are not synonyms.
 refs=['data/'+x['path'] for x in d['references']]
 builds=['build/'+x['name'] for x in d['compiler_final_outputs']]
 controls=[x['path'] for x in d['control_files']]
 stages=['stage/'+x for x in d['stage_files']]
 transport=['transport/'+x for x in d['supervisor']['transport_files']]
 prospective=refs+builds+controls+stages+transport
 success_stage=['stage/'+x for x in ['stdout.log','stderr.log','receipt.json','consumed.json']]
 prebundle=refs+builds+controls+success_stage+transport
 final=prebundle+['stage/bundle.json']
 assert len(set(prospective))==len(prospective)==75
 assert set(final)<set(prospective) and set(prospective)-set(final)=={'stage/failure-inventory.json'}
 p=d['profile']
 p['input_count']=len(d['inputs']);p['input_bytes']=sum(x['bytes'] for x in d['inputs'])
 p['input_hash_chunks']=sum((x['bytes']+65535)//65536 for x in d['inputs'])
 p['include_input_count']=p['input_count']+len(d['current_include_inputs'])
 p['complete_current_inputs']=p['input_count']+len(d['sources'])+1+1
 p['declared_success_run_files']=len(final);p['declared_failure_extra_files']=len(set(prospective)-set(final))
 p['maximum_complete_fence_identities']=p['complete_current_inputs']+len(prospective)
 p['directory_namespaces_count']=len(d['closed_directory_inputs'])
 p['directory_namespace_entries']=sum(x['entry_count'] for x in d['closed_directory_inputs'])
 p['directory_namespace_files']=sum(len(x['files']) for x in d['closed_directory_inputs'])
 p['profile_source']='PROVISIONAL_R16_SOURCE_UNION_ONLY_NATIVE_SDK_BINARY_AND_FINAL_CANONICAL_UNION_UNBOUND'
 # Existing runtime admission ceilings are preserved, rather than falsely
 # deriving them as exact directory/file counts from a successful output recipe.
 assert p['failure_inventory_max_files']==p['root_walk_entries_max']==85
 assert p['root_walk_depth_max']==64
 fp=d['failure_policy']
 old_descriptions={k:fp[k] for k in ['declared_final_artifact_recipe_files','maximum_raw_failed_prefix_file_rows','maximum_run_entries','bounded_child_cleanup_events']}
 fp['historical_R15_unqualified_descriptions']={'scope':'UNUSED_PREDECESSOR_DESCRIPTIONS_ONLY_NOT_CURRENT_LIMITS_OR_RECIPE','values':old_descriptions}
 fp['declared_final_artifact_recipe_files']=len(prospective)
 fp['recipe_meaning']='PROSPECTIVE_CATALOGUE_POSITIONS_INCLUDES_BUNDLE_AND_FAILURE_INVENTORY_NOT_NORMAL_SUCCESS'
 fp['normal_prebundle_file_recipe_files']=len(prebundle)
 fp['normal_success_file_recipe_files']=len(final)
 fp['maximum_raw_failed_prefix_file_rows']=p['failure_inventory_max_files']
 fp['maximum_run_entries']=p['root_walk_entries_max']
 fp['bounded_child_cleanup_events']=15
 fp['row_limit_meaning']='EXISTING_GETTASKENTRIES_ADMISSION85_FILTERS_TO_AT_MOST85_FILE_ROWS_NOT_A_75_FILE_FAILURE_CLAIM'
 fp['cleanup_meaning']='PROTOCOL_V2_14_PREFIX_CATEGORIES_MAX_PLUS_CONDITIONAL_LATE_TAIL_QUARANTINE_TOTAL15'
 fp['fixed_secondary_sites']=len(re.search(r'\$taskSecondarySites=@\(([^\n]+)\)',runner).group(1).split(','))
 fp['quarantine']='One rooted actual helper owner plus fixed native pending/unknown provenance; retain independent facts and files, prohibit another child; no silent owner-release or unbounded cleanup wait.'
 fp.pop('R14_cleanup_entries_max',None)
 d['closed_json_domain']['output_arrays_max_entries']=p['complete_current_inputs']
 d['closed_json_domain']['output_arrays_max_entries_recipe']='CURRENT_INPUTS_PLUS_FOUR_OWN_SOURCE_ROWS_PLUS_MANIFEST_AND_HELPER_DESCRIPTOR_INPUTS_PLUS6'
 d['retention']['input_bytes']=p['input_bytes']
 pools=[p['input_bytes'],d['retention']['own5_reserved'],d['retention']['reference_pool'],d['retention']['compiler_admitted_final_pool'],d['retention']['stage_pool'],d['retention']['control_pool'],d['supervisor']['transport_cap'],d['supervisor']['binary']['maximum_bytes']]
 d['retention']['reservation_upper']=sum(pools)
 d['retention']['headroom_under_840MiB']=d['retention']['cap']-sum(pools)
 life=d['controlled_lifetime']
 life['schedule'][3]='Child: fixed native two4096 reader buffers, immutable ready/exit states, prefix-only body tickets, explicit8-value target environment/NUL/private job;1800s/7200 normal polls/250ms and two2000ms cleanup groups. Pending/unknown owners quarantine without release credit.'
 life['schedule'][7]='C++: sequential rate caches735/25/25 pairs (785 total construction inventory,735 maximum live rows), streams/text/defaults/copies/metadata/arguments; Catalogue and native Data remain declaration-only. Actual source objects/ABI/backing count; dense vectors not introduced.'
 life['R16_status']='NEW_PREFIX_ONLY_ADMITTED_GROUP_AND_PREPAID_COMPLETION_OWNERS_ALL_JOINS_PENDING_NO_FREE_CREDIT'
 life['raw_scalar_counterexample_scope']='HISTORICAL_R7_R8_SOURCE_BEHAVIOR_ONLY_NOT_CURRENT_WHOLE_LOWER_OR_FIT'
 d['cache_pair_payload_meaning']='785_TOTAL_PAIR_CONSTRUCTION_INVENTORY_TIMES16_NOT_MAXIMUM_LIVE_CACHE_BYTES'
 d['cache_max_live_pair_rows']=max(d['fraction_residues'])
 d['source_work']['R16_status']='FRESH_WHOLE_R16_ALL_LANGUAGE_ENTRY_GROUP_COMPLETION_REFUSAL_AND_MANDATORY_TAIL_CENSUS_REQUIRED'
 d['source_work']['complete_census']='HOLD_PENDING_WHOLE_R16_ALL_CALLERS_BODIES_DEFAULTS_COPIES_ERRORS_TICKETS_AND_PREPAID_ADMITTED_GROUP_COMPLETION_TAIL'
 d['source_corrections_correspondence']['pending']='Fresh whole R16functional review, all-language census/resources/native/binding and separate one-attempt rootGO. No limit, Data or execution guard is bound.'
 d['R16_corrections']={'predecessor_manifest':rec(BASE/'source-manifest.json'),'whole_review_pins':rec(REVIEW/'source-pins.json'),'whole_review_freeze':rec(REVIEW/'first-green-review-freeze.json'),'findings':['F9 same-lock prefix-only resume and startup/body tickets','F10 current prospective/success/failure/JSON/namespace/profile descriptions derived separately'],'older_F1_F8':'PRESERVED_SOURCE_CORRECTIONS_STILL_SUBJECT_TO_WHOLE_FRESH_REVIEW','target_CPP_header_parent_helpers_controls_argv_environment':'UNCHANGED_EXCEPT_DERIVED_PARENT_BOOTSTRAP_NODE_LITERAL','all_previous_sources_reviews_attempts':'FROZEN_UNMODIFIED','old_prices_native_resources_binding_GO_transferred':False}
 d['author_verification']={'kind':'OWN_SOURCE_FILE_HASH_JSON_INTEGER_COUNTEREXAMPLES_ONLY','target_parser_AST_import':False,'compiler_native_API_helper_fixture_Clock':False,'first_complete_own_pass':'EXTERNAL_CHECKS_AND_FREEZE_RECORD_REQUIRED'}
 for _ in range(24):
  count=nodes(d)
  current,n=re.subn(r'Assert-TaskRawJsonBytes \$taskBootstrap \d+ 512 4',f'Assert-TaskRawJsonBytes $taskBootstrap {count} 512 4',runner)
  if n!=1:raise ValueError('one bootstrap seam')
  write(NEW/'run-clock.ps1',current)
  d['sources']=[rec(NEW/name,name) for name in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md']]
  if nodes(d)==count:break
 else:raise ValueError('bootstrap fixed point')
 assert geometry(d)[0]<=4 and geometry(d)[1]<=512
 dump(NEW/'source-manifest.json',d)
 counts={'prospective_recipe_files':len(prospective),'normal_prebundle_success_files':len(prebundle),'final_normal_success_files':len(final),'failure_extra_file_positions':p['declared_failure_extra_files'],'raw_failed_prefix_file_rows_max':p['failure_inventory_max_files'],'root_walk_entries_admission_max':p['root_walk_entries_max'],'root_walk_depth_admission_max':p['root_walk_depth_max'],'child_cleanup_entries_max':15,'fixed_secondary_slots':fp['fixed_secondary_sites'],'current_complete_inputs_and_json_array_max':p['complete_current_inputs'],'complete_path_catalogue_positions':p['maximum_complete_fence_identities'],'directory_namespaces':p['directory_namespaces_count'],'directory_namespace_entries':p['directory_namespace_entries'],'directory_namespace_files':p['directory_namespace_files'],'target_families':d['cost_catalogue']['family_count'],'native_families':len(families)-1,'rate_pair_counts':d['fraction_residues'],'pair_construction_inventory':sum(d['fraction_residues']),'maximum_live_cache_rows':max(d['fraction_residues'])}
 dump(OUT/'metadata-derivation.json',{'counts':counts,'prospective_run_names':prospective,'normal_prebundle_run_names':prebundle,'final_success_run_names':final,'interpretation':'Source/list/dynamic-caller correspondence only; failure85 and cleanup15 are unchanged source admission ceilings, not new limits or exact reached sizes.','target_execution':False})
 dump(OUT/'authoring.json',{'candidate':NEW.relative_to(ROOT).as_posix(),'sources':[rec(NEW/n) for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','source-manifest.json','README.md']],'native':native,'bootstrap_nodes':nodes(d),'geometry':geometry(d),'profile':p,'native_families':families[:-1],'source_only':True,'target_execution':False,'all_prices_native_ABI_resources_profile_final_union_binding_GO':'UNBOUND_HOLD'})
 print(json.dumps({'candidate':NEW.relative_to(ROOT).as_posix(),'inputs':p['input_count'],'bytes':p['input_bytes'],'bootstrap_nodes':nodes(d),'geometry':geometry(d),'counts':counts}))
if __name__=='__main__':
 ar=OUT/'author-attempts';ar.mkdir(exist_ok=True);att=ar/f'attempt-{len(list(ar.iterdir()))+1:02d}';att.mkdir()
 inputs=[Path(__file__),OUT/'candidate-readme.md',*sorted((OUT/'native').glob('*')),*sorted(BASE.glob('*')),OLDPACKET/'source-pins.json',OLDPACKET/'freeze.json',REVIEW/'review.md',REVIEW/'review.json',REVIEW/'source-pins.json',REVIEW/'first-green-review-freeze.json']
 for p in inputs:
  if p.is_file():
   q=att/'inputs'/p.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
 code=0
 with (att/'stdout.txt').open('w') as so,(att/'stderr.txt').open('w') as se,contextlib.redirect_stdout(so),contextlib.redirect_stderr(se):
  try:main()
  except BaseException:traceback.print_exc();code=1
 dump(att/'result.json',{'exit_code':code,'source_only':True,'target_execution':False})
 for p in [*sorted(NEW.glob('*')),OUT/'metadata-derivation.json',OUT/'authoring.json']:
  if p.is_file():
   q=att/'outputs'/p.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
 print((att/'stdout.txt').read_text(),end='')
 if code:print((att/'stderr.txt').read_text(),end='',file=sys.stderr)
 raise SystemExit(code)
