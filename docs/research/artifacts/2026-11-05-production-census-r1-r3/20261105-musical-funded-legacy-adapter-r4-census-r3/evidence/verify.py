"""Final complete ordinary-file census check; no subject code/helper is imported."""
from pathlib import Path
from hashlib import sha256
import json
import re
import sys

HERE=Path(__file__).resolve().parent
BASE=HERE.parent
ROOT=BASE.parent.parent
def load(p):return json.loads(p.read_bytes())
def digest(p):return sha256(p.read_bytes()).hexdigest().upper()
d=load(BASE/'census.json')
pins=load(BASE/'source-pins.json')
checks=[]
def ck(name,test,detail=None):checks.append({'name':name,'pass':bool(test),'detail':detail})
for group in ['subject','reference_files','authored_evidence','outputs']:
 for r in pins[group]:
  if r['path'].endswith('/evidence/final-checks.json'):continue # own later output; external seal check pins it
  p=ROOT/r['path']
  ck('raw_pin:'+r['path'],p.stat().st_size==r['bytes'] and digest(p)==r['sha256'])
source=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-source-r4'
formation=(source/'formation-r8.h').read_text()
names=re.findall(r'^\s*([A-Za-z][A-Za-z0-9_]*),\s*// SITE\s*$',formation,re.M)
ck('actual_source_enum_420',len(names)==420 and names==[e['site'] for e in d['events']])
ck('complete_inventory',len(d['events'])==420 and len(d['helpers'])==51 and len(d['schedules'])==38 and len(d['query_quote_12_terms']['terms'])==12)
ck('declared_unentered_190',d['events'][190]['site']=='LegacyLocalOwnerReturn' and d['events'][190]['source_anchors']==[] and d['events'][190]['price_coefficients']==[0,0,0])
md=(BASE/'census.md').read_text()
def aff(a):return str(a[0])+''.join('+'+(str(v)+'*' if v!=1 else '')+n for v,n in zip(a[1:],['B','d']) if v)
for e in d['events']:
 n=e['ordinal'];name=e['site']
 ck('MD_event:'+name,'|'+str(n)+'|'+name+'|'+aff(e['price_coefficients'])+'|' in md)
 ck('positive_reached_price:'+name,n==190 or e['price_coefficients'][0]>0)
 for a in e['source_anchors']:
  line=(source/a['file']).read_text().splitlines()[a['line']-1]
  ck('actual_anchor:'+name+':'+str(a['line']),'Site::'+name in line)
 helper=[sum(d['helpers'][h]['tuple'][i]*k for h,k in e['helper_coefficients'].items()) for i in range(10)]
 hd=[sum(d['helpers'][h].get('d_vector',[0]*10)[i]*k for h,k in e['helper_coefficients'].items()) for i in range(10)]
 ck('fresh_vector:'+name,e['constant_vector']==[a+b for a,b in zip(e['body_vector'],helper)] and e['d_vector']==[a+b for a,b in zip(e['body_d_vector'],hd)])
 ck('scalar_sum:'+name,e['price_coefficients']==[sum(e['constant_vector']),sum(e['B_vector']),sum(e['d_vector'])])
for n,h in d['helpers'].items():
 ck('MD_helper:'+n,'|'+n+'|'+','.join(map(str,h['tuple']))+'|'+str(h['price'])+'|' in md)
for n,s in d['schedules'].items():
 ck('MD_schedule:'+n,'|'+n+'|'+aff(s['price_constant_B_d'])+'|' in md)
old=d['old_callee_exact_catalogue']
ck('exact_old_catalogue_raw_identity',digest(ROOT/old['path'])==old['sha256'])
ck('old_190_rows_13_templates',len(old['rows'])==190 and len(old['case_templates'])==13)
for n,r in old['rows'].items():
 ck('old_literal_sum:'+n,sum(r['vector'])==r['price'])
 if 'sum' in r:
  terms=[(k,1) if isinstance(k,str) else (k[0],k[1]) for k in r['sum']]
  ck('old_composition:'+n,r['vector']==[sum(old['rows'][k]['vector'][i]*multiplicity for k,multiplicity in terms) for i in range(10)])
ck('old_AS_jump_ramp',old['rows']['AS_JUMP']['price']==61 and old['rows']['AS_RAMP']['price']==94)
ck('max_refs3',d['lifetime_schedule']['max_simultaneous_origin_references']==3)
ck('no_binding_or_fit_acceptance',d['correction']['source_binding_or_fit_acceptance'] is False)
ck('generic_d_not_erased',d['formation_equations']['same_root_physical_fixed_constant_B_d']==[51762,9,8] and d['query_quote_12_terms']['terms'][0]['derived_price']==[161648,0,1])
ck('separate_selected_R5_d_fact',d['selected_R5_fundamental_type_identity_separate']['selected_R5_U64_size_t_nonidentity_d']==0 and d['selected_R5_fundamental_type_identity_separate']['no_target_execution'] is True)
ck('only_new_artifacts_LF',all(b'\r' not in p.read_bytes() for p in [BASE/'census.md',BASE/'census.json',BASE/'source-pins.json']))
out={'schema':'COMPLETE_R3_FINAL_ORDINARY_FILE_SELF_CHECK','checks':checks,'count':len(checks),'failures':[c for c in checks if not c['pass']],'scope':'Data/file/hash/text/integer arithmetic only. No target source parser/compiler/helper/assembly/program/stage/native/GUI/Git/CI.'}
(HERE/'final-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'checks':len(checks),'failures':out['failures']},indent=2))
sys.exit(1 if out['failures'] else 0)
