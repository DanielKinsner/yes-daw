"""Independent JSON/source/integer checks; no author module or target execution."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
SUB = 'build-ci/20261106-musical-funded-legacy-adapter-r4-census-r5'
checks = []
records = {}

def ck(label, actual, expected=True):
    ok = actual == expected
    checks.append(dict(check=label, passed=ok))
    if not ok:
        raise AssertionError((label, actual, expected))

def sha(b): return hashlib.sha256(b).hexdigest().upper()

def rd(name, expected=None):
    p = ROOT / name
    b = p.read_bytes()
    r = dict(path=name, bytes=len(b), sha256=sha(b))
    records[name] = r
    if expected:
        ck('SHA:' + name, r['sha256'], expected)
    return b

def js(name, expected=None): return json.loads(rd(name, expected))

def plus(*vv): return [sum(c) for c in zip(*vv)] if vv else [0] * 10
def mul(v, n): return [x*n for x in v]
def affine(a, B=0, d=0): return a[0]+a[1]*B+a[2]*d
def ap(*aa): return [sum(c) for c in zip(*aa)]

pins = js(SUB+'/source-pins.json', 'EB0EFB963D5B13587DBA9B0A3B1B7C390BB153781D9DFC3510209659690BEC5A')
for section in ('subject', 'reference_files', 'authored_sources', 'outputs'):
    for r in pins[section]:
        b = rd(r['path'], r['sha256'])
        ck('length:'+r['path'], len(b), r['bytes'])
d = js(SUB+'/census.json', 'E50F8ACCCBD222F78992FA0797C474B863B3122653D8C77C8C8765A5D8E7D33E')
md = rd(SUB+'/census.md', 'D17908488D90F537B9235F0D4C10924B66D2BD0FC4B647C91E44F8B78A9AD554').decode('utf-8-sig')
fact = js(SUB+'/all-420-factorizations.json')
inventory = js(SUB+'/source-inventory.json')
ck('ten-component order',d['vector_order'],['R','W','C','X','LE','H','Z','O','A','F'])
H = d['helpers']
for name, h in H.items():
    vv = [p['vector'] for p in h['ordered_parts']]
    for key, n in h['helper_coefficients'].items():
        ck('helper coefficient:'+name+':'+key, type(n) is int and n >= 0)
        vv.append(mul(H[key]['tuple'],n))
    ck('helper tuple:'+name,plus(*vv),h['tuple'])
    ck('helper scalar:'+name,sum(h['tuple']),h['price'])
    ck('helper d shape:'+name,len(h['d_vector']),10)
    ck('helper bare-name X0:'+name,h['bare_identifier_selector_X'],0)
    ck('helper vector domain:'+name,all(type(n) is int and n>=0 for n in h['tuple']+h['d_vector']))
events = d['events']
ck('complete factorization rows',fact['rows'],events)
ck('complete factorization helpers',fact['helpers'],H)
ck('all420',len(events),420)
ck('all51',len(H),51)
ck('ordinal order',[e['ordinal'] for e in events],list(range(420)))
E = {e['site']:e for e in events}
ck('unique sites',len(E),420)
source = {Path(r['path']).name:rd(r['path']).decode('utf-8-sig').splitlines() for r in pins['subject'] if r['path'].endswith(('.h','.cpp'))}
anchor_count = 0
for e in events:
    n = e['site']
    cv = plus(e['body_vector'],*[mul(H[k]['tuple'],v) for k,v in e['helper_coefficients'].items()])
    dv = plus(e['body_d_vector'],*[mul(H[k]['d_vector'],v) for k,v in e['helper_coefficients'].items()])
    ck('event expansion:'+n,cv,e['constant_vector'])
    ck('event d:'+n,dv,e['d_vector'])
    ck('event B:'+n,e['body_B_vector'],e['B_vector'])
    ck('event price:'+n,[sum(cv),sum(e['B_vector']),sum(dv)],e['price_coefficients'])
    ck('event operand factorization:'+n,plus(*[p['vector'] for p in e['ordered_body_operands']]),e['body_vector'])
    ck('event explicit source basis:'+n,bool(e['source_operand_derivation']))
    ck('event vector domain:'+n,all(type(v) is int and v>=0 for v in cv+dv+e['B_vector']))
    for a in e['source_anchors']:
        anchor_count += 1
        ck(f'anchor:{n}:{a["line"]}',source[a['file']][a['line']-1].strip(),a['text'].strip())
    found = re.findall(r'^\|'+str(e['ordinal'])+r'\|'+re.escape(n)+r'\|([^|]+)\|([^|]+)\|$',md,re.M)
    ck('MD row unique:'+n,len(found),1)
    ck('MD body:'+n,[int(x) for x in found[0][1].split(',')],e['body_vector'])
    price = e['price_coefficients']
    expected = str(price[0])+('+'+str(price[1])+'*B' if price[1] else '')+('+'+str(price[2])+'*d' if price[2] else '')
    ck('MD scalar:'+n,found[0][0],expected)

def sumsites(c):
    ck('site coefficient names',all(k in E and type(v) is int and v>=0 for k,v in c.items()))
    return [sum(E[k]['price_coefficients'][i]*n for k,n in c.items()) for i in range(3)]

S = d['schedules']
ck('all38',len(S),38)
for name,s in S.items():
    ck('schedule:'+name,sumsites(s['site_counts']),s['price_constant_B_d'])
    ck('reservation scope:'+name,s['reservation_only_not_actual_visits'],True)
    p=s['price_constant_B_d']
    expected=str(p[0])+('+'+str(p[1])+'*B' if p[1] else '')+('+'+str(p[2])+'*d' if p[2] else '')
    ck('MD schedule:'+name,'|'+name+'|'+expected+'|' in md)
for c in d['source_bound_comparisons']:
    for a in c['case_prices']:
        ck('branch B slope:'+c['label'],a[1],c['chosen'][1])
        for dd in (0,1):
            ck('branch domination:'+c['label']+':d'+str(dd),affine(c['chosen'],d=dd)>=affine(a,d=dd))
old=d['old_callee_exact_catalogue']
old_source=js(old['path'],old['sha256'])
old_pins=js('build-ci/20261105-musical-window-query-path-census-r1/source-pins.json')
vendor=[]
for value in old_pins.values():
    if isinstance(value,list):
        vendor.extend(x for x in value if isinstance(x,dict) and str(x.get('path','')).endswith(('/bit','/vector')))
ck('two actual SDK primary files',len(vendor),2)
sdk_text={}
for r in vendor:
    b=rd(r['path'],r['sha256'])
    ck('SDK primary length:'+r['path'],len(b),r['bytes'])
    sdk_text[Path(r['path']).name]=b.decode('utf-8-sig').splitlines()
ck('actual int bit_width result',sdk_text['bit'][129].strip(),'_NODISCARD constexpr int bit_width(const _Ty _Val) noexcept {')
ck('actual vector const[] base',sdk_text['vector'][1943].strip(),'return _My_data._Myfirst[_Pos];')
for key in ('rows','append_terminal_cases','case_templates'):
    ck('protected old:'+key,old[key],old_source[key])
ck('protected190',len(old['rows']),190)
ck('protected13',len(old['case_templates']),13)
ck('protected78',len(old['exact_compositions']),78)
old_rows=old['rows']
for c in old['exact_compositions']:
    vectors=[]
    for child in c['children']:
        name,n=(child,1) if isinstance(child,str) else child
        vectors.append(mul(old_rows[name]['vector'],n))
    ck('old composition:'+c['name'],plus(*vectors),c['vector'])
    ck('old composition price:'+c['name'],sum(c['vector']),c['price'])
    old_rows={**old_rows,c['name']:dict(vector=c['vector'])}

f=d['formation_equations']
root_roles=['StateCleanup','CaptureCheck','ControlRole','WorkerOrReaderRole','RetainedRole','WorkerOrReaderRole','ResultRole','LegacyFixed','LegacyCheck','LegacyCleanup']
ck('root role sum',ap(f['fixed_physical_prefix_constant_B_d'],*[S[n]['price_constant_B_d'] for n in root_roles]),f['same_root_physical_fixed_constant_B_d'])
ck('root chosen',f['rootEnvelope'],f['same_root_physical_fixed_constant_B_d'])
ck('legacy options actual payer count',S['LegacyFixed']['site_counts'].get('LegacyCaptureOptions'),1)
ck('chain actual payer count',S['LegacyFixed']['site_counts'].get('LegacyChainCaller'),1)
ck('ContextDecision role count',S['WorkerOrReaderRole']['site_counts'].get('ContextDecision'),1)
ck('all five reservations',d['lifetime_schedule']['all_five_role_reservations_coexist'])
ck('maximum refs',d['lifetime_schedule']['maximum_simultaneous_origin_references'],3)
ck('quote12',len(d['query_quote_12_terms']['terms']),12)
q=d['query_quote_12_terms']['terms']
q_expected=['QueryNewFixed','PrefixIteration','SearchIteration',None,None,None,'NativeMatchFixed','NativeComparedRegion','NativeComparedRegion','NativeComparedPage','NativeComparedPage','NativeComparedByte']
for i, name in enumerate(q_expected):
    if name:
        ck('quote schedule:'+str(i),q[i]['derived_price'],S[name]['price_constant_B_d'])
ck('old quote coefficients',[q[i]['derived_price'] for i in (3,4,5)],[[94,0,0],[970,0,0],[72,0,0]])

def quote(R,M,L,D,A,dd=0):
    counts=[1,max(R-1,0),2*R.bit_length(),R+1 if R else 0,1,R.bit_length(),1,M,D,L,A,L]
    return sum(affine(t['derived_price'],d=dd)*n for t,n in zip(q,counts))
ex=d['conditional_fit']['conditional_examples_only']
rr=affine(f['rootEnvelope'],4264,0)
nn=affine(f['variableCaptureFixed_constant_B_d'])+2*affine(f['perSearchRow_constant_B_d'])+affine(f['perMapRow_constant_B_d'])+affine(S['MapCleanup']['price_constant_B_d'])+affine(S['ParentCleanup']['price_constant_B_d'])
qq=quote(1,9,8284,1,4)
before=rr+nn+qq+7+4096+1024
ck('example root',rr,ex['root'])
ck('example needed',nn,ex['needed'])
ck('example Q',qq,ex['Q'])
ck('example beforeN',before,ex['before_N_term'])
for row,val in ex['Q_for_R'].items():ck('example Q R'+row,quote(int(row),9,8284,1,4),val)
ck('example C1N1',before+24,ex['C1_N1'])
for C in (1,2):ck('example N C'+str(C),(268435456-before)//(8+16*C),ex[f'C{C}_N_sufficient_ceiling'])

# Independent source-recipes for every root/capture affine, not a price transfer.
def ps(name): return E[name]['price_coefficients']
def ss(*names): return ap(*[ps(n) for n in names])
def mx(label,*values):
    candidates=[v for v in values if all(v[1]==w[1] and all(affine(v,d=dd)>=affine(w,d=dd) for dd in (0,1)) for w in values)]
    ck('source affine maximum:'+label,bool(candidates))
    return candidates[0]

snap=S['Snapshot']['price_constant_B_d']
post=mx('postcapture',snap,ap(ss('NewFailureTest','ReadoutCommit'),snap),
        ap(ss('NewFailureTest','SessionSelect','SessionInstall','ReadoutCommit'),mx('session branch',ps('SessionKeepRunning'),ps('SessionReusePending')),mul(snap,2)))
fi=ss('RootFailedAdopt','RootFailedSnapshot','RootFailedConstructed','RootFailedSlotErase','RootFailedDestroy','RootFailedSessionCheck','RootFailedSessionFinish')
fn=ss('RootFailedReadout','RootFailedDiscard','RootFailedSnapshot')
fp=ap(ss('RootBegin','RootSessionInit','RootCounterIssue','RootIdentity','RootAllocate','RootObject','RootInstall','LegacyRootInstall','Distribute','LegacyDistribute','CaptureDispatch','CaptureResultTest','CaptureRefused'),post)
ns=ap(ss('RootBegin','RootSessionInit','RootCounterIssue','RootIdentity','RootNoState'),mx('no-state failure',ps('RootIdentityRefused'),ss('RootAllocate','RootAllocateFailed')),fi)
physical=ap(fp,*[S[n]['price_constant_B_d'] for n in root_roles])
search=mx('clip/asset row',*[ap(ss(k+'LoopTest',k+'Row',k+'Decision'),mx(k+' found/continue',ps(k+'Found'),ps(k+'Continue'))) for k in ('Clip','Asset')])
maprow=ap(ss('MapLoopTest','MapRow','MapDecision'),mx('first/later tick',ps('MapFirst'),ps('MapLater')),mx('invalid/store continuation',ps('MapInvalid'),ss('MapStore','MapContinue')))
maploop=ss('MapLoopEntry','MapLoopTest','MapEnd','CaptureFinish')
allocation=ap(ps('MapAllocate'),mx('allocation result',ps('MapAllocateFailed'),ap(ps('MapInstall'),maploop)))
maptail=ap(ps('MapNeededTest'),mx('map absent/present',maploop,allocation))
product=ap(ps('SourceProduct'),mx('source product result',ps('SourceProductRefused'),maptail))
extent=ap(ps('SourceExtent'),mx('source extent result',ps('SourceExtentRefused'),product))
layout=ap(ps('SourceLayout'),mx('source layout result',ps('SourceLayoutRefused'),extent))
srctail=ap(ps('SourceCheck'),mx('owner present/missing',ps('SourceMissing'),layout))
metadata=ap(ss('CaptureMetadata','CaptureParents'),srctail)
afixed=ss('AssetLoopEntry','AssetLoopTest','AssetEnd','AssetMissingTest')
cfixed=ss('ClipLoopEntry','ClipLoopTest','ClipEnd','ClipMissingTest')
asset=ap(afixed,mx('asset found/missing',ps('AssetMissing'),metadata))
clip=ap(cfixed,mx('clip found/missing',ps('ClipMissing'),asset))
variable=ap(ps('CaptureWorkBegin'),clip)
formation_derived=dict(rootEnvelope=mx('root physical/no-state',physical,ns),same_root_physical_fixed_constant_B_d=physical,no_state_constant_B_d=ns,fixed_physical_prefix_constant_B_d=fp,postcapture_fixed=post,failed_initial_tail=fi,failed_next_tail=fn,perSearchRow_constant_B_d=search,perMapRow_constant_B_d=maprow,variableCaptureFixed_constant_B_d=variable)
for key,value in formation_derived.items():ck('independent formation:'+key,value,f[key])
branches=dict(snapshot=snap,map_loop=maploop,map_allocation=allocation,map_tail=maptail,product=product,extent=extent,layout=layout,source=srctail,metadata_parents=metadata,search_fixed_each=cfixed,asset_fixed=asset,clip_fixed=clip)
for key,value in branches.items():ck('independent capture branch:'+key,value,f['branch_expansions'][key])

cell_formulas={
    'LegacyRaw':12+4+3+3+3+3+3+20+4+2+3+4+5+5+5+1+3+2+3+3+3,
    'Raw':420+21+94,'Canonical':3*6,'Snapshot':4+2*6,
    'FormationReceipt':1+16+535+1,'RootCensus':535+3,'Formed':1+1+553+538,
    'Readout':553+1,'Bounds':420+5+5+3+3+3+4,
    'Clip':3*16+128+16,'Asset':16+32+3,'CapturedMetadata':16+192+51+1+3,
    'LegacyReadout':2+4+2+2*2+3*6+94+1,
    'NativeSnapshot':1+4+6+1+1,'Endpoint':3+10,'Prediction':3+10,
    'LegacyPreparationOptions':2+1+6+2+2,'EntityId':16,'TempoChange':3,
    'NativeBuildIdentity':6,'NativeControl':3,'NativeModule':3,'NativeImage':3,
    'NativePage':3+2+3,'MODULEINFO':3,'MEMORY_BASIC_INFORMATION_x64':8,
    'ActionVector':10,'AppendArguments':6,'Search':3}
ck('all29 cell formulas',len(cell_formulas),29)
for name,v in cell_formulas.items():ck('cell formula:'+name,v,d['copy_cells'][name])

# Full contemporary boundary inventory, including the formerly omitted member.
reader_expected={'ReceiveWorker':6,'ReceiveReader':6,'ContextReturn':4,'ContextCancel':2,'ContextInspect':6,'ContextDecision':3}
for name,C in reader_expected.items():ck('Reader body C:'+name,E[name]['body_vector'][2],C)
reader_lines=[(i+1,s.strip()) for i,s in enumerate(source['formation-r8.h']) if re.search(r'\bReader\s*\?',s)]
ck('all10 Reader selections',len(reader_lines),10)
vector_sites={'ClipRow':(1049,4,'in.project_.clips[static_cast<std::size_t> (ci)].id'),'ClipFound':(1054,3,'&in.project_.clips[static_cast<std::size_t> (ci)]'),'AssetRow':(1085,5,'in.project_.assets[static_cast<std::size_t> (ai)].id'),'AssetFound':(1090,3,'&in.project_.assets[static_cast<std::size_t> (ai)]'),'MapRow':(1175,8,'in.project_.tempoMap[static_cast<std::size_t> (mi)]')}
for name,(line,R,expression) in vector_sites.items():
    ck('vector base R:'+name,E[name]['body_vector'][0],R)
    ck('vector operator source:'+name,expression in source['formation-r8.h'][line-1])
for name,X,dd in [('LegacyClockEligibility',4,1),('QueryQuote',32,1),('QueryPredictionEndpoints',9,0)]:
    ck('bit result conversion constant:'+name,E[name]['body_vector'][3],X)
    ck('bit input-only d:'+name,E[name]['body_d_vector'][3],dd)
ck('one B owner',[(e['site'],e['B_vector']) for e in events if any(e['B_vector'])],[('RootBegin',[4,2,1,2,0,0,0,0,0,0])])
ck('native page three d results',H['NativePageLeaf']['d_vector'],[0,0,0,3,0,0,0,0,0,0])
for e in events:
    for case in e.get('concrete_caller_cases',[]):
        cv=plus(case['body'],*[mul(H[k]['tuple'],n) for k,n in e['helper_coefficients'].items()])
        ck('caller expansion:'+e['site']+':'+case['case'],cv,case['constant_vector'])
        ck('caller scalar:'+e['site']+':'+case['case'],sum(cv),case['price'])
        ck('caller scalar dominated:'+e['site']+':'+case['case'],case['price']<=e['price_coefficients'][0])
for name,component in [('ReceiveWorker',5),('ContextReturn',6)]:
    expected=E[name]['constant_vector'].copy();expected[component]-=1
    ck('adapter exact wrapper difference:'+name,E[name]['new_adapter_caller_vector'],expected)
ck('chain once fixed',S['LegacyFixed']['site_counts'].get('LegacyChainCaller'),1)
ck('options once fixed',S['LegacyFixed']['site_counts'].get('LegacyCaptureOptions'),1)
ck('two cell cleanup',S['LegacyCleanup']['site_counts'].get('LegacyCellDestroy'),2)
ck('four reserve unions',[(n,S['LegacyCheck']['site_counts'].get(n)) for n in ('ReserveScan','ReserveFail','ReserveInstall')],[('ReserveScan',4),('ReserveFail',4),('ReserveInstall',4)])
for C in (1,2):
    last=(268435456-before)//(8+16*C)
    ck('conditional ceiling passes C'+str(C),before+last*(8+16*C)<=268435456)
    ck('conditional next refuses C'+str(C),before+(last+1)*(8+16*C)>268435456)

report=dict(scope='Independent whole source/data numeric census under fixed production404E/709B, generic B/d; no ABI/native/resource/binding/GO certificate.',verdict='ACCEPT_WHOLE_SOURCE_OPERATION_CENSUS_CONDITIONAL_BINDING_AND_RESOURCE_HOLD',checks=len(checks),all_passed=all(c['passed'] for c in checks),event_count=420,helper_count=51,schedule_count=38,source_anchors_checked=anchor_count,independent_formation=formation_derived,independent_capture_branches=branches,independent_copy_cell_formulas=cell_formulas,Reader_source_occurrences=reader_lines,conditional_example=dict(root=rr,needed=nn,Q=qq,before_N_term=before,C1_N1=before+24,C1_N_ceiling=(268435456-before)//24,C2_N_ceiling=(268435456-before)//40),verified_files=sorted(records.values(),key=lambda r:r['path']),check_rows=checks)
(OUT/'packet-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({k:report[k] for k in ['verdict','checks','all_passed','event_count','helper_count','schedule_count','source_anchors_checked','independent_formation','conditional_example']}))
