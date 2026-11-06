import hashlib,json,pathlib
BASE=pathlib.Path(__file__).resolve().parent.parent;ROOT=BASE.parent.parent
oldpath=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r1/census.json'
reviewpath=ROOT/'build-ci/20261105-musical-funded-legacy-adapter-r4-numeric-independent-review/review.json'
old=json.loads(oldpath.read_bytes());review=json.loads(reviewpath.read_bytes());d=json.loads((BASE/'census.json').read_bytes())
assert hashlib.sha256(reviewpath.read_bytes()).hexdigest().upper()=='449EE026E7A95A43B41DB678A3DDA2BC2D9723B00C73EC64C3ECA333D7B022F3'
def pin(p):return {'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest().upper()}
for x in review['subject_seals']:
 p=ROOT/x['path'];assert p.stat().st_size==x['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest().upper()==x['sha256']
expected={162:[185,0,0],168:[146,0,0],175:[156,0,0],176:[120,0,0],179:[172,0,0],343:[176,0,1]}
changed=[]
for o,n in zip(old['events'],d['events']):
 assert o['ordinal']==n['ordinal'] and o['site']==n['site']
 assert o['body_vector']==n['body_vector'],n['site']
 if any(o[k]!=n[k] for k in ['constant_vector','B_vector','d_vector','price_coefficients']):
  changed.append(n['ordinal']);assert n['price_coefficients']==expected[n['ordinal']]
 assert all(n[k]==o[k] for k in ['B_vector','d_vector'])
assert changed==list(expected)
assert d['helpers']['GetterTwo']['tuple']==[2,2,0,0,0,2,2,0,0,0]
assert d['helpers']['ReturnSpanValue']['tuple']==[2,2,0,0,0,2,2,0,0,0]
assert sum(e.get('helper_coefficients',{}).get('GetterTwo',0) for e in d['events'])==6
assert sum(e.get('helper_coefficients',{}).get('ReturnSpanValue',0) for e in d['events'])==2
assert [e['ordinal'] for e in d['events'] if e.get('helper_coefficients',{}).get('ReturnSpanValue')]==[162,343]
for k,v in old['helpers'].items():
 if k!='GetterTwo':assert v==d['helpers'][k],k
assert d['old_callee_exact_catalogue']==old['old_callee_exact_catalogue']
schedule_changes={}
for k,v in d['schedules'].items():
 assert v['site_counts']==old['schedules'][k]['site_counts'],k
 if v['price_constant_B_d']!=old['schedules'][k]['price_constant_B_d']:schedule_changes[k]={'old':old['schedules'][k]['price_constant_B_d'],'new':v['price_constant_B_d']}
assert schedule_changes=={
 'PredictFixed':{'old':[146065,0,1],'new':[146073,0,1]},
 'ResolveFixed':{'old':[154181,0,1],'new':[154189,0,1]},
 'QueryNewFixed':{'old':[161630,0,1],'new':[161638,0,1]},
 'LegacyFixed':{'old':[24433,0,5],'new':[24449,0,5]}}
assert d['formation_equations']['same_root_physical_fixed_constant_B_d']==[51747,9,8]
assert d['query_quote_12_terms']['terms'][0]['derived_price']==[161638,0,1]
assert d['query_quote_12_terms']['terms'][1:]==old['query_quote_12_terms']['terms'][1:]
d['correction']={
 'kind':'COMPLETE_R2_REPLACEMENT_PROPOSAL_REQUIRES_FRESH_WHOLE_NUMERIC_REVIEW_NOT_TOPUP',
 'prior_complete_census':pin(oldpath),'complete_REVISE_review':pin(reviewpath),
 'findings':['F1 six owned two-cell member-lvalue getter returns: existing2R/2W transfer once plus getter H/Z and selectedcopy H/Z','F2 rows() returns constructed span value at162/343:2R/2W logical base/extent plus getter/rangeconstructor H/Z,not reference access'],
 'changed_Site_ordinals':changed,'source_sized_effects':[{ 'ordinal':k,'old':old['events'][k]['price_coefficients'],'new':d['events'][k]['price_coefficients'],'delta_vector':[a-b for a,b in zip(d['events'][k]['constant_vector'],old['events'][k]['constant_vector'])]} for k in changed],
 'schedule_effects':schedule_changes,
 'helper_cases':'GetterTwo and ReturnSpanValue have equal numeric tuple but different actual source/constructor ownership; all actual references stay GetterReference. Existing size/bitwidth/getter caller selectors/parameters/destinations remain once.',
 'all_other_414_event_numeric_vectors':'RETAINED_PROPOSAL_ONLY_NOT_CERTIFIED_BY_PREVIOUS_REVISE',
 'all_420_factorizations_and_38_schedules':'REBUILT_AND_RECHECKED_AS_COMPLETE_NEW_PROPOSAL',
 'old190_vectors_and_78compositions':'PRESERVED_EXACT_NO_PRICE_CHANGE',
 'old_seals':'Prior R1 three files,review and all target R4/R5 source preserved raw-byte exact',
 'source_binding_or_fit_acceptance':False,
 'independent_fresh_whole_numeric_review':'REQUIRED_BEFORE_ANY_ACCEPTANCE_BINDING_OR_EXECUTION'}
d['self_checks']['R2_F1_F2_complete_source_case_propagation']='PASS_6SITES_4SCHEDULES_ROOT_QUERY_12TERMS_REBUILT'
d['self_checks']['all_old_R1_subject_seals_preserved']=True
(BASE/'census.json').write_bytes((json.dumps(d,indent=2)+'\n').encode())
report={'schema':'R2_COMPLETE_RECOMPUTATION_SELF_CHECK','changed_sites':d['correction']['source_sized_effects'],'changed_schedules':schedule_changes,'root':d['formation_equations']['same_root_physical_fixed_constant_B_d'],'old190_exact':True,'old3seals':True,'all420_rebuilt':True,'target_execution':False}
(BASE/'evidence/correction-checks.json').write_bytes((json.dumps(report,indent=2)+'\n').encode())
print(json.dumps(report,indent=2))
