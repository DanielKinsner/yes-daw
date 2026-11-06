"""Independent artifact consistency and source pins; never target execution."""
from pathlib import Path
from hashlib import sha256
import json,re
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
def read(p):return json.loads(p.read_bytes())
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
checks=[]
def ck(name,ok,detail=None):checks.append(dict(check=name,passed=bool(ok),detail=detail))
B=read(HERE/'baseline.json');E=read(HERE/'evaluation.json');O=read(HERE/'originals.json');K=read(HERE/'components.json');C=read(HERE/'collections.json');N=read(HERE/'environment.json');H=read(HERE/'fixed-key-hash-chains.json')
base=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
pins_expected={'run-clock.ps1':'3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F','source-manifest.json':'4C10468F6F521A754771F733E2E339B88FFCA28351D36C2C30C2F926F50CC557','clock_stage.cpp':'F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206','source-costs.h':'0087C41C88C8647FCC9CAAD92DA55223164122A4E31FF2DFC27715FAAEB83A27','README.md':'DC39905A4833FB580FAA55BE562E15EAB923F3B9D1006371D88CBE928C386D90'}
for n,s in pins_expected.items():ck('R12:'+n,digest(base/n)==s)
source=(base/'run-clock.ps1').read_text()
ck('R12 helper count71',len(re.findall(r'^function ',source,re.M))==71)
ck('all source entry flags false',all('$'+s+'=$false' in source for s in ['taskSourceReviewAccepted','taskSourceWorkAccepted','taskExecutionAccepted','taskCompilerAccepted','taskProduceAccepted','taskVerifyAccepted']))
m=read(base/'source-manifest.json')
def visits(v):
 if isinstance(v,dict):return 1+len(v)+sum(visits(x) for x in v.values())
 if isinstance(v,list):return 1+sum(visits(x) for x in v)
 return 1
ck('bootstrap visit count16058',visits(m)==16058,visits(m))
ck('profile input count1220',len(m['inputs'])==1220)
ck('namespace31/1092/1045',[len(m['closed_directory_inputs']),sum(r['entry_count'] for r in m['closed_directory_inputs']),sum(len(r['files']) for r in m['closed_directory_inputs'])]==[31,1092,1045])
for p in B['phases']:
 ck('selected term join:'+p['id'],sum(p['selected_terms'].values())==p['selected_numeric_partial'])
 ck('positive nonfit/fit withholding:'+p['id'],p['selected_numeric_partial']>0 and 'V_native_child' in p['remaining_equation'] and 'A_association_delta' in p['remaining_equation'])
ck('six stage vector',[p['stage_calls'] for p in B['phases']]==[0,7,264,523,782,784])
ck('children vector',[p['children'] for p in B['phases']]==[0,0,1,2,3,3])
ck('FileInfo vector',[p['FileInfo_calls_upper'] for p in B['phases']]==[2359,2493,8746,15115,23921,23941])
ck('thirteen dispositions',[r['id'] for r in B['families']]==['F','R','J','H','W','O','K','B','P','A','N','V','E'])
ck('six explicit residual IDs',[r['id'] for r in B['remaining']]==['P_fresh','V_managed_native_stream_async','V_native_child','F_SDKcold','A_association_delta','PROFILE'])
ck('whole claim withheld',B['verdict']=='HOLD_NO_COMPLETE_NUMERIC_UPPER_NO_FIT_OR_NOFIT' and not B['target_execution'] and not B['source_or_cap_or_domain_changed'])
ck('joint cap unchanged',B['cap']==33554432)
for n,g in E['output_topologies'].items():
 ck('identifier keys:'+n,all(re.fullmatch('[A-Za-z_][A-Za-z_0-9]*',s) for s in g['keys']))
 ck('source depth/cardinality:'+n,g['maximum_container_chain']==dict(marker=1,receipt=5,failure=3,bundle=3)[n])
 ck('key width:'+n,g['maximum_key_units']==dict(marker=35,receipt=60,failure=38,bundle=41)[n])
for p in C['phases']:
 f=p['FileSet_vector'];s=p['FileSet_sums']
 ck('FileSet source vector sum:'+p['id'],s==dict(calls=sum(r['calls'] for r in f),N=sum(r['calls']*r['expected'] for r in f),M=sum(r['calls']*r['observed'] for r in f)))
ck('full FileSet259/8783/8784',C['phases'][-1]['FileSet_sums']==dict(calls=259,N=8783,M=8784))
ck('five measures one encode distinction',O['phases'][-1]['preflight_measure_schedule']==dict(marker=1,receipt=4,failure=1,bundle=5))
ck('bundle private counter boxes692112',O['preflight_per_document']['bundle']['stored_numeric_counter_boxes']==692112)
ck('whole preflight5136568',O['phases'][-1]['preflight_requests']==5136568)
ck('generic report graph3741640',K['report']['selected_graph_upper_excluding_BigInteger20']==3741640)
ck('child graph8008/12344',[K['child_graphs']['control_N13_depth2'],K['child_graphs']['summary_N25_depth1']]==[8008,12344])
ck('b20 dominated',64+160<=344<=1112)
ck('native cap-domain outer259 witness',255*4096+2<=1048576 and 2+255+2==259)
ck('inner candidate separate',1056772==1048576+2*4096+4)
ck('259 current not258','outerOps<=259c' in B['remaining'][1]['equation'] and 'Earlier258' in B['remaining'][1]['outer_correction'])
ck('conditional N gross143480 perchild',N['per_child_numeric_selected']==143480)
ck('N selected source wrapper24',B['N_wrapper_source_correction']['field_slot_upper']==24)
ck('N full3children430512',3*(N['per_child_numeric_selected']+24)==430512)
for t in H['tables']:
 ck('order independent collision bound:'+t['name'],all(r['source_chain_upper']<101 for r in t['rows']))
ck('precreated final no growth',[H['prefix'][n] for n in ['precreated_alias_capacity','final_alias_capacity','precreated_string_parent_capacity','final_string_parent_capacity']]==[4049,4049,1931,1931])
ck('unreachable fixed rehash384024',B['fixed_hash_theorem']['removed_optional_array_requests']==384024)

input_paths=[base/n for n in pins_expected]
rels=[
 'build-ci/20261105-clock-oracle-separate-research-contract/contract.md',
 'build-ci/20261105-clock-oracle-r12-hash-carrier-independent-review/review.json',
 'build-ci/20261105-clock-oracle-r12-hash-carrier-independent-review/source-pins.json',
 'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1/phases.json',
 'build-ci/20261105-clock-oracle-r11-payload-phase-closure-r1/fixed-inputs.json',
 'build-ci/20261105-clock-oracle-r11-payload-phase-independent-review-r1/review.json',
 'build-ci/20261105-clock-oracle-r11-payload-cap-domain-closure-r2/cap-domain.json',
 'build-ci/20261106-clock-r12-writer-join-r3/writer-join.json',
 'build-ci/20261106-clock-r12-writer-join-r3/source-pins.json',
 'build-ci/20261106-clock-r12-writer-components-independent-review-r2/review.json',
 'build-ci/20261106-clock-r12-writer-components-independent-review-r2/source-pins.json',
 'build-ci/20261106-clock-r12-writer-join-r2/writer-join.json',
 'build-ci/20261106-clock-r12-writer-family-r1/writer-component.json',
 'build-ci/20261106-clock-r12-writer-family-r1/cold-supplement.json',
 'build-ci/20261106-small-runtime-owner-source-r1/small-owner-components.json',
 'build-ci/20261106-small-runtime-owner-source-r1/source-pins.json',
 'build-ci/20261106-fixed-key-hash-independent-review-r1/freeze-manifest.json',
 'build-ci/20261106-fixed-key-hash-independent-review-r1/review.json',
 'build-ci/20261106-fixed-key-hash-independent-review-r1/source-pins.json',
 'build-ci/20261105-clock-oracle-r12-path-geometry-r1/geometry.json',
 'build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/review.json',
 'build-ci/20261106-clock-oracle-r12-path-geometry-independent-review-r1/source-pins.json',
 'build-ci/20261105-controller-environment-writer-closure-r2/writer-closure.json',
 'build-ci/20261105-controller-environment-writer-closure-r2/source-pins.json',
 'build-ci/20261106-controller-environment-writer-closure-independent-review-r2/review.json',
 'build-ci/20261105-managed-payload-abi-probe-source-r2/execution-r1/receipt.json',
 'build-ci/20261105-managed-payload-abi-probe-source-r2/execution-r1/stdout.jsonl',
 'build-ci/20261105-managed-payload-abi-probe-independent-review-r2/review.json',
 'build-ci/20261105-managed-file-type-inventory-source-r2/execution-r1/inventory.json',
 'build-ci/20261106-comparer-encoding-source-r1/RandomizedStringEqualityComparer.cs',
 'build-ci/20261106-comparer-encoding-source-r1/NonRandomizedStringEqualityComparer.cs',
 'build-ci/20261106-comparer-encoding-source-r1/String.Comparison.cs',
 'build-ci/20261106-controller-v-wrapper-source-closure-r1/primary/DictionaryWrapper.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/HashHelpers.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Hashtable.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Dictionary.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/List.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/StringReader.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Environment.Variables.Windows.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/ProcessStartInfo.cs',
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/Process.Windows.cs',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/OrderedDictionary.cs',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/ArrayList.cs',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JTokenWriter.cs',
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JsonSerializerInternalReader.cs',
 'build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/JsonTextReader.cs',
 'build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/Compiler.cs',
 'build-ci/20261105-clock-oracle-r11-tuple-metadata-independent-review/primary-sources/Binders.cs',
 'build-ci/20261105-controller-environment-writer-closure-r2/primary/src/Microsoft.PowerShell.Commands.Utility/commands/utility/WebCmdlet/JsonObject.cs',
 'build-ci/20261105-controller-environment-writer-closure-r2/primary/src/System.Management.Automation/engine/Pipe.cs',
 'build-ci/20261105-controller-environment-writer-closure-r2/primary/src/System.Management.Automation/engine/runtime/Operations/MiscOps.cs',
 'build-ci/20261106-clock-r12-writer-family-r1/primary/HashSet.cs',
 'build-ci/musical-audio-persistent-planner-certification/source-revisions/waveform-definition-r3/independent_clock.h',
 'src/engine/Time.h']
input_paths += [ROOT/p for p in rels]
input_paths += [ROOT/E['fixed_inputs'][1]['path']]
inputs=[]
for p in input_paths:
 ck('input exists:'+str(p.relative_to(ROOT)),p.is_file())
 if p.is_file():inputs.append(dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=digest(p)))
ck('frozen theorem helper pin',digest(HERE/'check-fixed-key-hashes.py')=='BACD6D7F66B76334429005BBF38A7C028733E66E3596C649CE11827DB5D5671C')
ck('frozen theorem result pin',digest(HERE/'fixed-key-hash-chains.json')=='587CB936BC03C25E3C6336AC1240C8B4EA763AEAE1EE327F4D43D1987610E1FD')
for p in HERE.iterdir():
 if p.is_file() and p.suffix in ['.py','.json','.md'] and p.name not in ['mechanical-evidence.json','source-pins.json']:
  ck('authored LF:'+p.name,b'\r' not in p.read_bytes())
failures=[r for r in checks if not r['passed']]
out=dict(schema='R12_RESOURCE_BASELINE_INDEPENDENT_CONSISTENCY_CHECK_R1',status='PASS_CONSISTENCY_ONLY' if not failures else 'REVISE',checks=checks,check_count=len(checks),failures=failures,input_pins=inputs,
 scope='Source/data/file/hash arithmetic only; consistency is not independent acceptance of all unfinished A/V/P/cold/profile obligations or complete32MiBfit.')
(HERE/'mechanical-evidence.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(checks=len(checks),failures=failures,input_files=len(inputs)),indent=2))
raise SystemExit(bool(failures))
