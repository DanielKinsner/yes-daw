"""Complete file/hash/JSON/integer source candidate checks; no target execution."""
from pathlib import Path
import hashlib,json
R=Path(__file__).resolve().parents[2];O=Path(__file__).resolve().parent
assert not (O/'first-green-freeze.json').exists(),'FROZEN'
H=lambda p:hashlib.sha256(p.read_bytes()).hexdigest().upper()
rows=[]
def c(n,v):rows.append({'name':n,'pass':bool(v)})
def load(n):return json.loads((O/n).read_text(encoding='utf-8'))
pins=load('source-pins.json')
for x in pins['inputs']:
 p=Path(x['path']);p=p if p.is_absolute() else R/p
 c('pin '+x['path'],p.stat().st_size==x['bytes'] and H(p)==x['sha256'])
c('frozen envelope pins',H(R/'build-ci/20261106-native-controlled-object-envelope-r1/source-pins.json')=='F252B5B6EBEAB13FDBE6AE35FB5FA177CA0F01FBEE36433ED3C10AA71A20B4A3')
c('frozen envelope firstgreen',H(R/'build-ci/20261106-native-controlled-object-envelope-r1/first-green-freeze.json')=='98C23B436B9B256DC28407A64FF10C8453C5C14D8A063F9ABE17275C8216DA06')
sdk=load('selected-sdk-ledger.json');ids={x['id'] for x in sdk['rows']}
c('unique finite source nodes',len(ids)==len(sdk['rows']) and len(ids)>=150)
for x in sdk['rows']:
 c(x['id']+' declaration corresponds',x['declaration_anchor'] in x['source_excerpt'])
 c(x['id']+' excerpt hash',hashlib.sha256(x['source_excerpt'].encode()).hexdigest().upper()==x['source_excerpt_sha256'])
 c(x['id']+' child source closure',all(y in ids for y in x['children']))
 c(x['id']+' typed object list',all(v['type'] in sdk['type_symbols'] and v['count']==1 for k in ['value_parameters','locals','materializations'] for v in x[k]))
 c(x['id']+' no numerical nil type',all(sdk['type_symbols'][v['type']]['size'] is None or sdk['type_symbols'][v['type']]['size']>0 for k in ['value_parameters','locals','materializations'] for v in x[k]))
 c(x['id']+' source range extent',0<x['lines'][0]<=x['lines'][1])
graph={x['id']:x['children'] for x in sdk['rows']}
def depth(n,stack=()):
 if n in stack:raise ValueError(('recursive graph',stack,n))
 return 1+max([depth(x,stack+(n,)) for x in graph[n]] or [0])
depths={n:depth(n) for n in ids}
c('all selected call chains finite',max(depths.values())<30)
c('no numeric OTHER invented',sdk['numeric_OTHER_upper'] is None and sdk['no_32768_credit'])
actual={x['id']:x for x in sdk['rows']}
c('classic UB4 direct predicate', '_Pred(_Val, *_UMid)' in actual['UB4']['source_excerpt'] and '_Pass_fn' not in actual['UB4']['source_excerpt'])
c('prefix outer copies exist',len(actual['UB3']['value_parameters'])==2 and len(actual['UB4']['value_parameters'])==3)
c('append callback exact5 formals',len(actual['L_APPEND_CALL']['value_parameters'])==5)
c('insert callback exact6 formals',len(actual['L_INSERT_CALL']['value_parameters'])==6)
c('growth callback object append',any(x['type']=='L_APPEND' for x in actual['S_GROW_APPEND']['value_parameters']))
c('growth callback object insert',any(x['type']=='L_INSERT' for x in actual['S_GROW_INSERT']['value_parameters']))
c('actual vector guard',any(x['type']=='TG' for x in actual['V_CONSTRUCT']['locals']))
c('fresh assignment no invented guard/proxy',not any(x['type'] in ['TG','FP'] for x in actual['V_ASSIGN_COUNTED']['locals']+actual['V_ASSIGN_FILL']['locals']))
c('actual placement-new params',len(actual['PLACEMENT_NEW']['value_parameters'])==2)
c('actual zero-bit address object',any(x['name']=='_Zero' and x['type']=='D' for x in actual['BITS_ZERO']['locals']))
c('sizeof raw refs not assigned',all(isinstance(x['reference_bindings'],list) for x in sdk['rows']))
cl=load('closure-incidence.json')
c('exact16 own closures',cl['own_count']==16 and len(cl['own_closures'])==16)
c('two distinct SDK closures',cl['SDK_count']==2 and len({tuple(x['lines']) for x in cl['new_sdk_closures']})==2)
for x in cl['own_closures']+cl['new_sdk_closures']:
 c('closure no guessed layout '+x['id'],x.get('sizeof') is None and x.get('alignof') is None)
 if x in cl['own_closures']:
  c('closure no copied capture '+x['id'],x['source_incidence']['additional_copies']==0 and x['source_incidence']['maximum_direct_or_named_instances']==1)
 else:c('SDK closure direct Fn '+x['id'],x['captures']==[] and x['object_instances']==1 and x['additional_copy_instances']==0)
joins=load('context-joins.json')
c('all12 actual contexts',len(joins['contexts'])==12 and len({x['id'] for x in joins['contexts']})==12)
c('all24 preserved plus new bitcast group',len(joins['materialization_groups'])==25 and joins['materialization_groups'][-1]['id']=='bit_cast_prvalue_sources')
c('all14 critical lifetime joins',len(joins['critical_joins'])==14)
for x in joins['contexts']:
 c('context finite linkage '+x['id'],all(any(z['id']==k for z in joins['critical_joins']) for k in x['sdk_joins']))
 c('context no wholefit '+x['id'],x['ABI_or_size_upper'] is None)
ts=load('required-type-facts.json')
needed=[x for x in ts['facts'] if x['needed_new_fact']]
c('no generic PRED extra request',not any(x['id']=='PRED' for x in needed))
c('minimal exact31 missing type facts',len(needed)==31)
c('all missing explicit null',all(x['sizeof'] is None for x in needed))
c('no forbidden unused tags/guards',not any(x['id'] in ['_Leave_proxy_unbound','_Tidy_deallocate_guard'] for x in needed))
for root in range(513):
 old=max(root|15,15)
 first=max((root+1)|15,15)
 c('constructor capacity bounded '+str(root),old<=527 and first<=527)
 for rel in [0,1,42,128]:
  size=root+1+rel
  new=first if size<=first else max(size|15,first+first//2)
  c('source string capacity '+str(root)+':'+str(rel),size<=641 and new<=790 and new+1<4096)
c('vector small allocation',4*24==96 and 96<4096)
mess=load('message-copy-requests.json')
c('explicit message copies',mess['maximum_literal_plus_NUL']==max(x['requested_copy_bytes'] for x in mess['messages']))
c('quote own incidence explicit',bool(load('quote-argument-incidence.json')['rows']))
c('earlier14 range failures preserved',(O/'line-audit-attempts/attempt-01/result.json').exists() and json.loads((O/'line-audit-attempts/attempt-01/result.json').read_bytes())['failed']==14)
c('protected test unchanged',H(R/'tests/musical_audio_call_recipe_tests.cpp')=='97B2A2ABE0BE0CDDFCE173A20E6A6264E837DB751FE204E93B4B094E8E22619C')
c('instructions unchanged',(R/'AGENTS.md').read_bytes()==(R/'CLAUDE.md').read_bytes() and H(R/'AGENTS.md')=='AFA45D3BAC0FDB892064C8928DDB21375FA0112D2B5840CF2877FAA472ECEB77')
c('no author target authority',load('authoring.json')['all_target_execution'] is False and pins['execution'] is False)
arg=(O/'argument.md').read_text(encoding='utf-8')
c('argument no numeric wholefit','numeric OTHER' in arg and 'HOLD' in arg)
result={'passed':sum(x['pass'] for x in rows),'failed':sum(not x['pass'] for x in rows),'checks':rows,'graph_source_depths':depths,'scope':'File/hash/JSON/integer source consistency only; complete candidate remains unaccepted and all numeric/type/native/binding/wholefit/target execution gates HOLD.'}
attemptroot=O/'check-attempts';attemptroot.mkdir(exist_ok=True)
attempt=attemptroot/('attempt-'+str(len(list(attemptroot.iterdir()))+1).zfill(2));attempt.mkdir()
for p in O.iterdir():
 if p.is_file():(attempt/p.name).write_bytes(p.read_bytes())
(attempt/'result.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
(O/'checks.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
if result['failed']:
 print(json.dumps({'passed':result['passed'],'failed':result['failed'],'names':[x['name'] for x in rows if not x['pass']]}));raise SystemExit(1)
freeze=[{'path':p.relative_to(R).as_posix(),'bytes':p.stat().st_size,'sha256':H(p)} for p in sorted(O.rglob('*')) if p.is_file()]
(O/'first-green-freeze.json').write_text(json.dumps({'files':freeze,'first_complete_own_green':True,'target_execution':False,'independent_acceptance':False,'wholefit':None},indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'passed':result['passed'],'failed':0,'rows':len(sdk['rows']),'minimal_missing_type_facts':len(needed),'pins':H(O/'source-pins.json'),'freeze':H(O/'first-green-freeze.json')}))
