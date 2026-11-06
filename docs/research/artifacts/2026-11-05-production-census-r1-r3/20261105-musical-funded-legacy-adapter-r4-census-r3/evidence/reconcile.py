"""Regenerate/check the complete production proposal; no subject execution."""
from collections import Counter
from pathlib import Path
from hashlib import sha256
import json

HERE=Path(__file__).resolve().parent
BASE=HERE.parent
ROOT=BASE.parent.parent
def load(p):return json.loads(p.read_bytes())
def write(p,x):p.write_text(json.dumps(x,indent=2)+'\n',encoding='utf-8',newline='\n')
def pin(p):return {'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':sha256(p.read_bytes()).hexdigest().upper()}
oldpath=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r2/census.json'
old=load(oldpath)
d=load(BASE/'census.json')
ledger=load(HERE/'ordered-operands.json')
L={x['site']:x for x in ledger['rows']}
reviewpath=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review-r2/review.json'
review=load(reviewpath)
assert pin(reviewpath)['sha256']=='AD543B6DBDF99D2C471B7FAD9405B04ED367FA7EB778955EB113152C420600BB'
assert len(d['events'])==420 and len(d['helpers'])==51 and len(d['schedules'])==38
assert len(d['query_quote_12_terms']['terms'])==12
checks=[]
def ck(name,ok,detail=None):
 checks.append({'name':name,'pass':bool(ok),'detail':detail})
 if not ok:raise AssertionError((name,detail))
def plus(*vs):return [sum(a) for a in zip(*vs)] if vs else [0]*10
def mul(a,n):return [x*n for x in a]
zero=[0]*10
for name,h in d['helpers'].items():
 ck('helper:'+name,len(h['tuple'])==10 and h['price']==sum(h['tuple']))
 ck('helper_unchanged_source_case:'+name,h==old['helpers'][name])
 ck('helper_nonnegative:'+name,all(isinstance(x,int) and x>=0 for x in h['tuple']+h.get('d_vector',zero)))

rows=[]
changed=[]
for index,(e,o) in enumerate(zip(d['events'],old['events'])):
 ck('enum_identity:'+e['site'],e['ordinal']==o['ordinal']==index and e['site']==o['site'])
 if e['site'] in L:
  body=plus(*(p['vector'] for p in L[e['site']]['ordered_body_operands']))
  ck('ordered_body:'+e['site'],body==e['body_vector']==L[e['site']]['body_vector'])
  e['ordered_body_operand_ledger']='evidence/ordered-operands.json#'+str(index)+'-'+e['site']
 else:
  body=e['body_vector']
  ck('unchanged_source_body_proposal:'+e['site'],body==o['body_vector'])
  e['ordered_body_operand_ledger']='Existing disjoint body proposal retained against exact unchanged source anchors; no positive certification from either prior REVISE. Recomputed below with every helper and ABI term for fresh whole review.'
 coeff=e.get('helper_coefficients',{})
 helper=plus(*(mul(d['helpers'][h]['tuple'],n) for h,n in coeff.items()))
 helper_d=plus(*(mul(d['helpers'][h].get('d_vector',zero),n) for h,n in coeff.items()))
 # ABI-only source actions remain explicit. They are not inferred from a layout equality.
 body_B=e['B_vector'][:]
 body_d=[a-b for a,b in zip(e['d_vector'],helper_d)]
 ck('separate_ABI_nonnegative:'+e['site'],all(x>=0 for x in body_B+body_d))
 fresh=plus(body,helper)
 fresh_d=plus(body_d,helper_d)
 ck('entire_factorization:'+e['site'],fresh==e['constant_vector'] and fresh_d==e['d_vector'])
 e['constant_vector']=fresh
 e['B_vector']=body_B
 e['d_vector']=fresh_d
 e['price_coefficients']=[sum(fresh),sum(body_B),sum(fresh_d)]
 e['status']='COMPLETE_R3_PROPOSAL_REQUIRES_FRESH_WHOLE_NUMERIC_REVIEW'
 e['body_B_vector']=body_B
 e['body_d_vector']=body_d
 ck('complete_vector_dimensions:'+e['site'],all(len(e[k])==10 for k in ['body_vector','constant_vector','B_vector','d_vector']))
 ck('all_vectors_nonnegative:'+e['site'],all(isinstance(x,int) and x>=0 for k in ['body_vector','constant_vector','B_vector','d_vector'] for x in e[k]))
 if e['price_coefficients']!=o['price_coefficients']:changed.append(index)
 rows.append({'ordinal':index,'site':e['site'],'source_anchors':e['source_anchors'],'body_vector':body,'body_B_vector':body_B,'body_d_vector':body_d,'helper_coefficients':coeff,'helper_vector':helper,'helper_d_vector':helper_d,'price':e['price_coefficients'],'body_ledger':e['ordered_body_operand_ledger'],'source_derivation':e.get('source_derivation',e.get('meaning','')),'caller_cases':e.get('caller_cases',[]),'scope':'Recomputed proposal. All explicit actual caller/parameter/copy/default/SDK/cleanup ownership remains subject to fresh whole numeric review.'})

expected={x['ordinal']:x for x in review['necessary_site_corrections_if_other_components_retained']}
ck('exactly_23_rebuilt_numeric_differences',changed==sorted(expected),changed)
for i,x in expected.items():
 e=d['events'][i]
 ck('independent_witness_result:'+e['site'],e['constant_vector']==x['necessary_vector_if_other_components_retained'] and e['price_coefficients']==x['necessary_price_if_other_components_retained'])
ck('old_190_whole_catalogue_exact',d['old_callee_exact_catalogue']==old['old_callee_exact_catalogue'])
ck('all_29_copy_cell_footprints_unchanged',d['copy_cells']==old['copy_cells'])
ck('aggregate_initialization_forms_unchanged',d['aggregate_initialization_cases']==old['aggregate_initialization_cases'])
ck('all_five_role_and_sequential_lifecycle_scope_unchanged',d['lifetime_schedule']==old['lifetime_schedule'])
E={e['site']:e for e in d['events']}
schedule_changes={}
for name,s in d['schedules'].items():
 ck('schedule_source_counts:'+name,s['site_counts']==old['schedules'][name]['site_counts'])
 fresh=[sum(E[k]['price_coefficients'][i]*n for k,n in s['site_counts'].items()) for i in range(3)]
 ck('schedule_recomposition:'+name,fresh==s['price_constant_B_d'])
 if fresh!=old['schedules'][name]['price_constant_B_d']:schedule_changes[name]={'old':old['schedules'][name]['price_constant_B_d'],'new':fresh}
ck('five_affected_complete_schedules',set(schedule_changes)=={'EndpointFixed','PredictFixed','ResolveFixed','QueryNewFixed','LegacyFixed'},schedule_changes)
ck('role_reserves_all_five',all(n in d['schedules'] for n in ['ControlRole','WorkerOrReaderRole','RetainedRole','ResultRole']))
ck('query_twelve_terms_later_11_exact',d['query_quote_12_terms']['terms'][1:]==old['query_quote_12_terms']['terms'][1:])
ck('query_first_term_recomputed',d['query_quote_12_terms']['terms'][0]['derived_price']==d['schedules']['QueryNewFixed']['price_constant_B_d'])
def q(R,M,L,D,A,conv):
 counts=[1,max(R-1,0),2*R.bit_length(),R+1 if R else 0,1,R.bit_length(),1,M,D,L,A,L]
 return sum(n*(a[0]+a[2]*conv) for n,t in zip(counts,d['query_quote_12_terms']['terms']) for a in [t['derived_price']])
for R in [0,1,2,64,1024]:
 ck('conditional_Q_replay:'+str(R),q(R,9,8284,1,4,0)==d['conditional_fit']['conditional_examples_only']['Q_for_R'][str(R)])
f=d['formation_equations']
ck('same_origin_root',[51762,9,8]==f['same_root_physical_fixed_constant_B_d'])
ck('legacy_fixed',[24464,0,5]==d['schedules']['LegacyFixed']['price_constant_B_d'])
ck('getter_copy_six_calls',sum(e['helper_coefficients'].get('GetterTwo',0) for e in d['events'])==6)
ck('span_value_two_actual_sites',[e['ordinal'] for e in d['events'] if e['helper_coefficients'].get('ReturnSpanValue',0)]==[162,343])
root=f['same_root_physical_fixed_constant_B_d'][0]+9*4264
needed=f['variableCaptureFixed_constant_d'][0]+2*f['perSearchRow_constant_d'][0]+f['perMapRow_constant_d'][0]+d['schedules']['MapCleanup']['price_constant_B_d'][0]+d['schedules']['ParentCleanup']['price_constant_B_d'][0]
base=root+needed+q(1,9,8284,1,4,0)+(1+6)+4096+1024
ck('entire_conditional_base',base==11905976,{'root':root,'needed':needed,'Q':q(1,9,8284,1,4,0),'base':base})
ck('same_cap_and_conditional_ceiling',(268435456-base)//24==10688728 and (268435456-base)//40==6413237)

d['correction']={
 'kind':'COMPLETE_R3_SOURCE_ORDERED_BODY_AND_CALLER_SPONSOR_REBUILD_REQUIRES_FRESH_WHOLE_REVIEW_NOT_SCALAR_TOPUP',
 'prior_complete_R2_census':pin(oldpath),'complete_R2_REVISE':pin(reviewpath),
 'new_hypothesis':'Complete ordered source operations for23 full bodies (113 groups), explicit caller/parameter/SDK and earlier-sponsor ownership, followed by fresh all420 body/helper/ABI factorizations and all38 source schedules/12 quote terms/root/examples. No signed scalar cancellation or unused reserve grants price acceptance.',
 'changed_Site_ordinals':changed,
 'source_sized_effects':[{'ordinal':i,'site':E[old['events'][i]['site']]['site'],'old':old['events'][i]['price_coefficients'],'new':d['events'][i]['price_coefficients'],'delta_vector':[a-b for a,b in zip(d['events'][i]['constant_vector'],old['events'][i]['constant_vector'])]} for i in changed],
 'schedule_effects':schedule_changes,
 'prior_getter_and_span_fixes':'RETAINED: GetterTwo6actual copies,ReturnSpanValue2actual sites; no extra caller destination,prezero or reference referent copy.',
 'remaining_397_bodies':'UNCHANGED_SOURCE_PROPOSALS_RECOMPOSED_AND_REQUIRING_FRESH_WHOLE_NUMERIC_REVIEW; neither earlier REVISE certifies them.',
 'all_420_factorizations_51_helpers_38_schedules_12_terms':'REGENERATED_COMPLETE_NEW_PROPOSAL',
 'old190_vectors_78compositions_13templates':'PRESERVED_EXACT_NO_NEW_PRICE_OR_CEILING_CHANGE',
 'B_d_conditions':'B is sizeof current selected Raw for this source identity. R5 object-qualified4264 is a separate conditional illustration, never R4 layout/offset equality. d is exactly U64/size_t nonidentity conversion; it remains conditional, not destruction.',
 'old_seals':'R1,R2,both whole critics,exact R4/R5 source remain preserved.',
 'source_binding_or_fit_acceptance':False,
 'fresh_whole_numeric_review':'REQUIRED_BEFORE_ANY_SOURCE_PRICE_ACCEPTANCE_DATA_BINDING_OR_EXECUTION',
}
d['self_checks']['R3_ordered_body_caller_sponsorship_rebuild']='COMPLETE_23_FULL_VECTORS_113_ORDERED_GROUPS_ALL420_RECOMPOSED'
d['self_checks']['all_old_R1_R2_and_whole_critic_seals_preserved']=True
d['symbolic_ABI']['B']='sizeof Raw for exact selected source identity; R4 layout unqualified. Separately object-qualified R5 B4264 is a conditional illustration only, not R4 equality.'
d['symbolic_ABI']['S']='sizeof exact selected requestState; R4 resource/layout unqualified. R5 public layout is independently scoped, not inherited offsets or total allocation.'
d['symbolic_ABI']['d']='Exactly U64/size_t nonidentity conversion:0 if the selected fundamental types are identical,1 otherwise. Conditional; not destruction.'
write(BASE/'census.json',d)
write(HERE/'all-420-factorizations.json',{'schema':'PRODUCTION_R3_COMPLETE_EVENT_BODY_HELPER_ABI_FACTORIZATION','vector_order':d['vector_order'],'rows':rows,'scope':'Source proposal only; no reviewer acceptance, binding, measured trace, native/resource or execution credit.'})
write(HERE/'rebuild-checks.json',{'schema':'R3_COMPLETE_RECOMPUTATION_SELF_CHECK','checks':checks,'check_count':len(checks),'failures':[c for c in checks if not c['pass']],'changed_sites':d['correction']['source_sized_effects'],'changed_schedules':schedule_changes,'root':f['same_root_physical_fixed_constant_B_d'],'conditional_example':{'B':4264,'d':0,'root':root,'needed':needed,'Q':q(1,9,8284,1,4,0),'source_upper_excluding_N':base,'C1_N1':base+24,'C1_Nmax':(268435456-base)//24,'C2_Nmax':(268435456-base)//40},'target_execution':False})
print(json.dumps({'checks':len(checks),'failures':[],'changed_sites':changed,'changed_schedules':schedule_changes,'root':f['same_root_physical_fixed_constant_B_d'],'conditional_base':base},indent=2))
