"""Source authoring only: writes a NEW unrun packet, never loads target code."""
from pathlib import Path
import copy,hashlib,json,ntpath,re
ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
OLD=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r10-native-path-correction-r3'
NEW=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r11-tuple-metadata-source-r1'
if NEW.exists():raise RuntimeError('new source packet must not already exist')
NEW.mkdir(parents=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
checks=[]
def ck(n,a,b):
 checks.append(dict(name=n,actual=a,expected=b,passed=a==b))
 if a!=b:raise ValueError(n)
old_manifest=json.loads((OLD/'source-manifest.json').read_text())
ck('exact predecessor manifest',sha(OLD/'source-manifest.json'),'824A2980CDACD71D084F5C9A62835299ACBB82E95FE693BF8FEF9E22F960D594')
for name in ['clock_stage.cpp','source-costs.h']:(NEW/name).write_bytes((OLD/name).read_bytes())
original=(OLD/'run-clock.ps1').read_text()
before='[pscustomobject]@{FullName=$taskPath;Name=[IO.Path]::GetFileName($taskPath);PSIsContainer=$taskDirectoryFlag;Length=$taskLength;Reparse=$taskLink}'
after='[System.Tuple[object,object,object,object,object]]::new($taskPath,[IO.Path]::GetFileName($taskPath),$taskDirectoryFlag,$taskLength,$taskLink)'
ck('one row producer',original.count(before),1)
runner=original.replace(before,after)
edits=[]
for receiver,fields in [('$taskF',{'FullName':'Item1','Name':'Item2','PSIsContainer':'Item3','Length':'Item4','Reparse':'Item5'}),('$_',{'FullName':'Item1','PSIsContainer':'Item3'})]:
 for field,item in fields.items():
  old=receiver+'.'+field;new=receiver+'.'+item;count=runner.count(old)
  if not count:raise ValueError('audited consumer missing:'+old)
  edits.append(dict(old=old,new=new,occurrences=count));runner=runner.replace(old,new)
ck('no old row member reads',len(re.findall(r'(?:\$taskF|\$_)\.(?:FullName|Name|PSIsContainer|Length|Reparse)\b',runner)),0)
ck('no remaining PSCustomObject producer',runner.lower().count('[pscustomobject]'),0)
comment='# Fresh internal metadata row: Item1 FullName, Item2 Name, Item3 directory,\n# Item4 Int64 length, Item5 reparse. Caller wire projections remain maps.\n'
runner=runner.replace('function Get-TaskEntries(',comment+'function Get-TaskEntries(',1)
doc=(OLD/'README.md').read_text()
doc=doc.replace('# ClockOracle R10', '# ClockOracle R11',1)
base=ntpath.normpath(str(NEW));repo=ntpath.normpath(str(ROOT))
own_paths=[ntpath.join(base,n) for n in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json']]
current_paths=[ntpath.normpath(r['path'] if ntpath.isabs(r['path']) else ntpath.join(repo,r['path'])) for r in old_manifest['inputs']]+own_paths
own_max=max(map(len,own_paths));path_max=max(map(len,current_paths))
doc=doc.replace('current own5 maximum is179 (ten more than R9)','current own5 maximum is'+str(own_max)+' (provisional R11 base)')
doc+='''
## R11 fresh tuple metadata rows, source revision 1

UNRUN, all guards false. This candidate changes the one fresh Get-TaskEntries
metadata row to System.Tuple<Object,Object,Object,Object,Object>. Its cells retain
FullName, Name, directory flag, Int64 length and reparse flag in that order. Only
closed metadata consumers use Item1..5; every serialized descriptor/failure/receipt
projection keeps the existing map fields, values, types and format. No metadata is
cached or recycled. Every enumeration/Current/name/attribute/length/reparse/depth/
Max/containment/hash/namespace check, case/context rule and unknown/tilde fallback
remains. Partial Destination rows and cleanup/primary outcomes remain owned.

The source minimum reduction is432 bytes per completed row under the qualified
x64 layout: old mandatory Hashtable264 + object[5]64 + five boxed DictionaryEntry
objects160, less a five-reference Tuple56. OrderedDictionary/ArrayList objects and
PSObject/property savings are omitted from that minimum. At least8360 required file
rows occur across eight successful namespace fences, giving3611520 source-request
bytes; the larger8736-row snapshot result is conditional on its directory entries.
This is not measured live/peak/reclaim evidence. Implicit PowerShell wrappers and
source literal/typed/binder/array copies are not asserted free or generally bounded.

The admitted inputs remain the predecessor R3 set PROVISIONALLY for functional
source preparation, with current own-source and bootstrap shape recomputed. This
is not the complete future canonical functional source/SDK/contract/census/review/
resource-proof/binding union. Root must settle that exact original-path union and
recompose its profile before any new complete numeric/binding authority; archive
copies/drafts are backups rather than automatically called inputs. Avoid a self-hash
cycle: census evidence identifies executable code/SDK/source graphs and parametric
profile correspondence; an external reviewed binding record closes the final
manifest/code/profile authority. No R10 price, old root geometry or GO transfers.

Next: independent whole functional review, complete new source census and all six
controlled payload/error/environment phase joins, checked whole upper, immutable
binding review and separate root GO. Source, parser/AST, compiler, SDK helper,
program/protocol stage, GUI and devices have not been executed for this packet.
The32MiB scratch/840MiB retained caps, all56files/matrices/thresholds/controls, source
serialization route and production budgets remain unchanged. No alternate SDK or
input rejection is introduced. Protected predecessors remain exact.
'''
manifest=copy.deepcopy(old_manifest)
manifest['schema']='YESDAW_CLOCK_ORACLE_R11_FRESH_TUPLE_METADATA_SOURCE_PROTOTYPE_UNBOUND'
manifest['source_authoring_go']='ROOT_APPROVED_ORDINARY_INTERNAL_FRESH_METADATA_TUPLE_REPRESENTATION'
manifest['exact_source_critic']='PENDING_FRESH_WHOLE_R11_FUNCTIONAL_REVIEW_THEN_NEW_COMPLETE_CENSUS_AND_SIX_PHASE_LIFETIME_CLOSURE'
manifest['R11_tuple_metadata']={
 'state':'SOURCE_ONLY_UNRUN_ALL_GUARDS_FALSE_PROVISIONAL_ADMISSION_NO_NEW_PRICE_OR_GO',
 'predecessor_source_manifest_sha256':sha(OLD/'source-manifest.json'),
 'row_type':'System.Tuple<System.Object,System.Object,System.Object,System.Object,System.Object>',
 'cells':['FullName','Name','PSIsContainer','Length','Reparse'],
 'semantics':'One fresh non-enumerable reference row per metadata append. Every filesystem/reparse/hash check and all wire projections remain.',
 'minimum_source_request_reduction_per_row':432,
 'unconditional_successful_namespace_file_rows':8360,
 'unconditional_source_request_reduction_lower':3611520,
 'snapshot_directory_rows_are_not_unconditional_lower':True,
 'implicit_PowerShell_wrapper_or_general_heap_zero_claim':False,
 'current_admission':'Provisional predecessor R3 set for functional source preparation only; recomputed own-source/bootstrap profile is not a completed canonical proof/data union.',
 'before_numeric_authority':'Root settles exact original-path functional source/SDK/contract/census/review/resource-proof/binding union; then recompute complete profile, all bodies, phase joins and bounds. Backup archive/draft copies are not automatic called inputs.',
 'self_hash_cycle_rule':'Census/reviews identify executable code/SDK/source graphs plus parametric profile correspondence; external reviewed binding record closes final manifest/code/profile identity.',
 'old_census_or_price_transferred':False,'global_payload_fit':False,'binding_or_execution_GO':False}
manifest['R10_payload_repairs']['current_own5_path_UTF16_max']=own_max
manifest['R10_payload_repairs']['known_current_path_UTF16_max']=path_max
manifest['R10_payload_repairs']['known_current_identity_paths_with_tilde']=sum('~' in p for p in current_paths)
manifest['controlled_lifetime']['metadata_rows']='Fresh five-object-reference Tuple per current entry. No metadata cache/recycle. Source clones/projections/binder/implicit wrapper terms and exact layout/source association remain separately accounted; no general heap or reclaim assertion.'
manifest['profile']['profile_source']='PROVISIONAL_PREDECESSOR_R3_ADMISSION_FOR_UNRUN_R11_FUNCTIONAL_SOURCE_PREPARATION; canonical functional proof/data union and all final bounds require root recomposition before authority.'
manifest['profile']['future_literal_binding_profile_state']='PENDING_CANONICAL_FUNCTIONAL_SOURCE_CONTRACT_CENSUS_RESOURCE_PROOF_DATA_UNION_AND_COMPLETE_RECOMPOSITION'

def funcs(text):
 starts=list(re.finditer(r'^function ([A-Za-z-]+)',text,re.M));main=text.index('\ntry{\n # Hash literal manifest')+1
 return {a[1]:text[a.start():starts[i+1].start() if i+1<len(starts) else main] for i,a in enumerate(starts)}
r9=(ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r9-source-prototype/run-clock.ps1').read_text()
old_functions=funcs(r9)

def shape(v,depth=0):
 if isinstance(v,dict):
  x=[shape(a,depth+1) for a in v.values()]
  return [1+sum(a[0] for a in x),len(v)+sum(a[1] for a in x),max([depth+1]+[a[2] for a in x]),max([len(k) for k in v]+[a[3] for a in x]+[0]),sum(a[4] for a in x)]
 if isinstance(v,list):
  x=[shape(a,depth+1) for a in v]
  return [1+sum(a[0] for a in x),sum(a[1] for a in x),max([depth+1]+[a[2] for a in x]),max([a[3] for a in x]+[0]),sum(a[4] for a in x)]
 return [1,0,depth,len(v) if isinstance(v,str) else 0,int(isinstance(v,float))]
last=None
for iteration in range(8):
 functions=funcs(runner)
 manifest['R10_payload_repairs']['helper_bodies_byte_exact_R9']=sorted(n for n in functions.keys()&old_functions.keys() if functions[n]==old_functions[n])
 manifest['R10_payload_repairs']['changed_helper_bodies']=sorted(n for n in functions.keys()&old_functions.keys() if functions[n]!=old_functions[n])
 (NEW/'run-clock.ps1').write_text(runner,encoding='utf-8',newline='\n')
 (NEW/'README.md').write_text(doc,encoding='utf-8',newline='\n')
 for record in manifest['sources']:
  p=NEW/record['path'];record.update(bytes=p.stat().st_size,lines=len(p.read_bytes().splitlines()),sha256=sha(p))
 s=shape(manifest);visits=s[0]+s[1]
 manifest['R10_payload_repairs']['raw_profiles']['bootstrap']['nodes_and_keys']=visits
 manifest['R10_payload_repairs']['raw_profiles']['bootstrap']['container_depth']=s[2]
 manifest['R10_payload_repairs']['bootstrap_observed']=dict(values=s[0],keys=s[1],max_string_units=s[3],container_depth=s[2],floating_values=s[4],nodes_and_keys=visits)
 runner=re.sub(r'Assert-TaskRawJsonBytes \$taskBootstrap [0-9]+ 512 [0-9]+','Assert-TaskRawJsonBytes $taskBootstrap '+str(visits)+' 512 '+str(s[2]),runner)
 doc=re.sub(r'Final source-profile arithmetic: bootstrap.*?caller uses [0-9]+/512/[0-9]+\.', 'Final source-profile arithmetic: bootstrap '+str(s[0])+' values + '+str(s[1])+' keys\n= '+str(visits)+' node/key visits; maximum string '+str(s[3])+' UTF16 units,\ncontainer depth '+str(s[2])+', no floating JSON values. The exact PS byte-helper\ncaller uses '+str(visits)+'/512/'+str(s[2])+'.',doc,flags=re.S)
 raw=(json.dumps(manifest,indent=2)+'\n').encode()
 state=(raw,runner,doc)
 if state==last:break
 last=state
else:raise RuntimeError('source-only metadata fixed point did not settle')
(NEW/'source-manifest.json').write_bytes(raw)
ck('current bootstrap caller matches exact data',re.search(r'Assert-TaskRawJsonBytes \$taskBootstrap ([0-9]+) 512 ([0-9]+)',runner).groups(),(str(visits),str(s[2])))
ck('README current bootstrap profile',re.search(r'bootstrap ([0-9]+) values \+ ([0-9]+) keys\n= ([0-9]+) node/key visits',doc).groups(),(str(s[0]),str(s[1]),str(visits)))
ck('new data source shape matches',shape(manifest),s)
ck('all guards false',list(manifest['guards'].values()),[False]*len(manifest['guards']))
ck('same provisional input identities',manifest['inputs']==old_manifest['inputs'],True)
ck('same numerical/output/reference/control domains',[manifest[k] for k in ['references','module_controls','control_files','compiler_final_outputs','dense_Q_entries','fraction_residues']],[old_manifest[k] for k in ['references','module_controls','control_files','compiler_final_outputs','dense_Q_entries','fraction_residues']])
ck('no source guard promotion',runner.count('$taskExecutionAccepted=$false'),1)
ck('own five admitted cap',sum((NEW/n).stat().st_size for n in ['clock_stage.cpp','run-clock.ps1','source-costs.h','README.md','source-manifest.json'])<=1048576,True)
for record in manifest['sources']:
 ck('final source pin:'+record['path'],sha(NEW/record['path']),record['sha256'])
for p in NEW.iterdir():
 b=p.read_bytes();ck('LF/noBOM:'+p.name,b'\r' not in b and not b.startswith(b'\xef\xbb\xbf'),True)
packet=dict(status='NEW_UNRUN_SOURCE_ONLY_REQUIRES_WHOLE_FUNCTIONAL_REVIEW_COMPLETE_NEW_CENSUS_AND_PAYLOAD_FIT',folder=NEW.relative_to(ROOT).as_posix(),predecessor_manifest=sha(OLD/'source-manifest.json'),manifest_sha256=sha(NEW/'source-manifest.json'),source_rows=manifest['sources'],consumer_edits=edits,bootstrap_shape=manifest['R10_payload_repairs']['bootstrap_observed'],own_path_max=own_max,current_identity_max=path_max,provisional_input_count=len(manifest['inputs']),provisional_canonical_union_pending=True,no_target_parser_AST_import_helper_compile_program_stage_run=True,checks=checks)
(OUT/'source-correspondence.json').write_text(json.dumps(packet,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({k:packet[k] for k in ['status','folder','manifest_sha256','source_rows','consumer_edits','bootstrap_shape','own_path_max','current_identity_max']},indent=2))
