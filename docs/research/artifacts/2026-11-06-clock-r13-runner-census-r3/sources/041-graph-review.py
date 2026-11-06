import json, pathlib, collections
root=pathlib.Path(__file__).resolve().parents[2]
x=json.loads((root/'build-ci/20261105-clock-oracle-r9-runner-census/census.json').read_text(encoding='utf-8'))
m=json.loads((root/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r9-source-prototype/source-manifest.json').read_text(encoding='utf-8'))
class S:
 def __init__(self,n):self.n=n
I=0;B=True
def lit(s):return S(len(s))
source=(root/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r9-source-prototype/run-clock.ps1').read_text(encoding='utf-8').splitlines()
child_keys='name started raw_exit_observed raw_exit timed_out poll_limited polls poll_limit log_cap stdout_start stderr_start raw_stdout_bytes raw_stderr_bytes position_record_complete raw_bytes_retained raw_bytes_observed observed_not_confirmed_retained omitted_at_cap unknown_undrained both_eof reads_settled owned_storage_quarantined cleanup failure'.split()
child_bools='started raw_exit_observed timed_out poll_limited log_cap position_record_complete unknown_undrained both_eof reads_settled owned_storage_quarantined'.split()
children=[]
for name in ['one-opaque-compile','oracle-produce','independent-full-verify']:
 c=dict.fromkeys(child_keys,I)
 for k in child_bools:c[k]=B
 c['name']=lit(name);c['cleanup']=[lit(z) for z in ['read_cancel','own_child_exit_not_observed_2000ms','read_settlement_diagnostic']+['settled_read_cancel_or_fault']*2+['retained_prefix_flush']*2+['retained_position_refusal']*2+['settled_owner_dispose']*4];c['failure']=lit('CHILD_BODY_FAILURE_RAW_RETAINED');children.append(c)
def summary(mode):
 s={'status':lit('CLOCK_PRODUCED_REQUIRES_INDEPENDENT_VERIFY' if mode=='produce' else 'CLOCK_VERIFIED_REQUIRES_ROOT_OUTPUT_REVIEW'),'source_manifest_sha256':S(64)}
 for k in ['entries','inverse_records','reserved','used','prior','composed_after']:s[k]=I
 for k in ['vendor','quality','Ready','compiler_peak_disk_qualified']:s[k]=B
 s['actual_control_witnesses']=[{'control':lit(k),'gate':lit(v),'operands':[I]*6} for k,v in m['module_controls'][mode].items()]
 return s
controls=[]
def control(id,gate,w):
 c={'id':lit(id),'gate':lit(gate),'witness':w};controls.append(c);return c
control('json-scalar-one-past-boundary','JSON_RAW_SCALAR_UNITS',{'observed':I,'maximum':I})
control('poll-one-past-boundary','CHILD_POLL_LIMIT',dict.fromkeys(['completed','limit','attempted'],I))
control('work-one-below','WHOLE_STAGE_WORK_RESERVATION',dict.fromkeys(['total','limit','global'],I))
control('bytes-one-below','WHOLE_STAGE_RETAINED_RESERVATION',dict.fromkeys(['total','limit','global'],I))
control('one-use-already-consumed','ONE_ATTEMPT_ALREADY_CONSUMED',{'path':S(512),'exists':B})
c=control('own-tool-or-predecessor-changed','BEFORE_AFTER_COMPLETE_INPUT_IDENTITY',{'path':S(512),'expected_bytes':I,'observed_bytes':I,'expected_hash':S(64),'observed_hash':S(64)})
c['mutation']={'path':S(512),'offset':I,'before':S(2),'after':S(2)};c['initial_copy_sha256']=S(64)
c=control('pe-gui-subsystem','ACTUAL_PE_CONSOLE_BEFORE_LAUNCH',dict.fromkeys(['signature','machine','magic','subsystem'],I))
c['mutation']={'path':S(512),'offset':I,'before':S(4),'after':S(4)};c['initial_copy_sha256']=S(64);c['actual_mutant_sha256']=S(64)
control('missing-inverse-descriptor','COMPLETE_EXACT_REFERENCE_FILE_SET',{'observed':I,'expected':I,'missing':lit('references/native-inverse/r48000-map5.json'),'unknown':lit(''),'duplicate':lit('')})
control('extra-file','COMPLETE_EXACT_REFERENCE_FILE_SET',{'observed':I,'expected':I,'missing':lit(''),'unknown':lit('unlisted.bin'),'duplicate':lit('')})
control('duplicate-file','COMPLETE_EXACT_REFERENCE_FILE_SET',{'observed':I,'expected':I,'missing':lit(''),'unknown':lit(''),'duplicate':S(max(len(r['path']) for r in m['references']))})
secondary_sites=['compile-after-fence','produce-after-fence','verify-after-fence','produce-after-binary','verify-after-binary','compile-child-finally','produce-child-finally','verify-child-finally','failure-inventory','failure-receipt','owned-dispose']
receipt={'schema':I,'stage':lit('ClockOracle'),'status':lit('CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED'),'source_manifest_sha256':S(64),'compiler_is_opaque':B,'compiler_internal_CPU_heap_RSS_temporary_peak_disk_qualified':B,'old_broad_compiler_output_bound_fulfilled':B,'children':children,'compiler_exit':I,'producer_exit':I,'verifier_exit':I,'vendor':B,'quality':B,'context':B,'Ready':B,'renderer':B,'reserved_source_work':I,'reserved_admitted_retained':I,'runner_controls':controls,'compiler_final_outputs':{'files':[{'path':S(512),'bytes':I,'sha256':S(64),'origin_bundle_sha256':S(64)} for i in range(3)],'bytes':I},'source_dependencies':{'includes':I,'report_is_additional_artifact_only':B,'complete_tool_inventory_replaced':B},'producer':summary('produce'),'verification':summary('verify'),'runner_used':I,'failure_code':lit('PRIMARY_FAILURE_RAW_RETAINED_NOT_SERIALIZED'),'primary_failure':B,'secondary_failures':[{'site':lit(s),'code':lit('FIRST_SECONDARY_RAW_RETAINED_NOT_SERIALIZED'),'count':I} for s in secondary_sites],'failure_inventory_complete':B,'failure_inventory_code':lit('BOUNDED_FAILURE_WALK_REFUSED'),'compiler_final_outputs_admitted':B}
def scan(v,c=None):
 if c is None:c=collections.Counter()
 if isinstance(v,dict):
  c['maps']+=1;c['keys']+=len(v);c['key_units']+=sum(map(len,v));c['map_separators']+=max(len(v)-1,0)
  for z in v.values():scan(z,c)
 elif isinstance(v,list):
  c['arrays']+=1;c['array_elements']+=len(v);c['array_separators']+=max(len(v)-1,0)
  for z in v:scan(z,c)
 elif isinstance(v,S):c['strings']+=1;c['string_units']+=v.n
 elif isinstance(v,bool):c['booleans']+=1
 elif isinstance(v,int):c['integers']+=1
 else:raise TypeError(v)
 return c
r=scan(receipt)
r['value_calls']=sum(r[k] for k in ['maps','arrays','strings','integers','booleans']);r['nodes_keys']=r['value_calls']+r['keys']
artifacts=[]
for q in m['references']:artifacts.append({'path':S(512),'relative_path':lit(q['path']),'bytes':I,'sha256':S(64),'origin_bundle_sha256':S(64)})
for i in range(11):artifacts.append({'path':S(512),'bytes':I,'sha256':S(64),'origin_bundle_sha256':S(64)})
bundle={'schema':I,'stage':lit('ClockOracle'),'status':lit('CONTROLLED_CLOCK_COMPLETE_ROOT_OUTPUT_REVIEW_REQUIRED'),'source_manifest_sha256':S(64),'predecessor_bundle':{'path':S(512),'sha256':S(64),'artifacts':I,'transitive':I,'complete_union_paths':I,'complete_union_bytes':I},'artifacts':artifacts,'transitive_inputs':[{'path':S(512),'bytes':I,'sha256':S(64),'origin_bundle_sha256':S(64)} for i in range(1191)],'admitted_retained_excluding_bundle':I,'bundle_bytes':I,'admitted_retained':I,'reserved_source_work':I,'runner_used':I,'compiler_operation':lit('OPAQUE'),'compiler_temporary_peak_disk_qualified':B,'compiler_CPU_internal_heap_RSS_qualified':B,'old_broad_compiler_output_bound_fulfilled':B,'vendor':B,'quality':B,'context':B,'Ready':B,'renderer':B}
failure={'files':[{'path':S(512),'bytes':I,'reparse':B,'admission':lit('NOT_AUTOMATICALLY_ADMITTED'),'opaque_temporary':B} for i in range(78)],'complete':B,'code':lit('BOUNDED_FAILURE_WALK_REFUSED'),'root_prefix':S(512),'compiler_temporary_peak_disk_qualified':B,'admitted_pool_success':B,'quarantine_retains_ownership':B}
marker={'stage':lit('ClockOracle'),'manifest':S(64),'work':I,'retained_reservation':I,'attempts':I,'opaque_compiler_peak_disk_qualified':B}
results={};checks=[]
for name,graph in [('receipt',receipt),('bundle',bundle),('failure',failure),('marker',marker)]:
 actual=scan(graph);actual['value_calls']=sum(actual[k] for k in ['maps','arrays','strings','integers','booleans']);actual['nodes_keys']=actual['value_calls']+actual['keys']
 want=x['graphs'][name];differences={k:[actual[k],v] for k,v in want.items() if type(v)==int and k in actual and actual[k]!=v}
 for k,v in want.items():
  if type(v)==int and k in actual:checks.append({'name':name+'.'+k,'actual':actual[k],'packet_upper':v,'pass':actual[k]<=v})
 hist=collections.Counter()
 def count_maps(v):
  if isinstance(v,dict):
   hist[len(v)]+=1
   for q in v.values():count_maps(q)
  elif isinstance(v,list):
   for q in v:count_maps(q)
 count_maps(graph)
 primes={2:3,3:3,4:7,5:7,6:7,7:7,13:17,21:23,24:29,30:37}
 cap=sum(primes[k]*v for k,v in hist.items());checks.append({'name':name+'.clone_primes','actual':cap,'packet_upper':want['clone_dictionary_prime_capacity_sum'],'pass':cap==want['clone_dictionary_prime_capacity_sum']})
 results[name]={'independent':{k:actual[k] for k in ['maps','arrays','keys','strings','integers','booleans','key_units','string_units','array_elements','map_separators','array_separators','value_calls','nodes_keys']},'differences':differences,'field_histogram':dict(hist),'clone_prime_capacity':cap}
report={'scope':'independent source-based symbolic graph model; no runner helper execution','graphs':results,'checks':checks,'failures':[q for q in checks if not q['pass']]}
(root/'build-ci/20261105-clock-oracle-r9-numeric-independent-review/graph-evidence.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'graph_counts':{k:{z:v['independent'][z] for z in ['maps','arrays','keys','string_units']} for k,v in results.items()},'failures':report['failures']},indent=2))
