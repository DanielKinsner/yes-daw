"""Independent JSON/source/integer checks; no author module or target execution."""
from pathlib import Path
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
SUB = 'build-ci/20261105-musical-funded-legacy-adapter-r4-census-r4'
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

pins = js(SUB+'/source-pins.json', '9A587302A9093829EE799D8B7C3E5F921B2C86D834999382909B7F229843E8B2')
for section in ('subject', 'reference_files', 'authored_sources', 'outputs'):
    for r in pins[section]:
        b = rd(r['path'], r['sha256'])
        ck('length:'+r['path'], len(b), r['bytes'])
d = js(SUB+'/census.json', 'BDBA8D8E2874BF7BD404108B575719EB0BF956C987E217F5A3DAD9E9BBE41E6E')
md = rd(SUB+'/census.md', 'C1AE759090DBC1002BC35F7E1A5B26ED4A6BA5A49FA712D43E4D6B645DD1611A').decode('utf-8-sig')
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

# Concrete independent source finding; retaining all other proposals is NOT acceptance.
e=E['ContextDecision']
line=source['formation-r8.h'][1575]
ck('finding ordinary Reader conditional',line.strip(),'Ledger::pay (s, s.roleLife_[Ledger::ix (Reader ? Role::Reader : Role::Worker)], Site::ContextDecision);')
ck('finding sealed C2',e['body_vector'][2],2)
ck('L no selector condition',H['L']['tuple'][2],H['P']['tuple'][2]+H['I']['tuple'][2])
necessary=e['body_vector'].copy();necessary[2]+=1
full=e['constant_vector'].copy();full[2]+=1
partial_root=f['rootEnvelope'].copy();partial_root[0]+=2
corrected_before=before+2
finding=dict(ordinal=104,site='ContextDecision',source='formation-r8.h:1576-1578',sealed_body=e['body_vector'],necessary_body_retaining_other_proposed_components=necessary,sealed_full=e['constant_vector'],necessary_full_retaining_other_proposed_components=full,sealed_price=e['price_coefficients'],necessary_price_retaining_other_proposed_components=[sum(full),0,0],schedule='WorkerOrReaderRole',sealed_schedule=S['WorkerOrReaderRole']['price_constant_B_d'],necessary_schedule_retaining_other_proposed_components=[S['WorkerOrReaderRole']['price_constant_B_d'][0]+1,0,0],root_partial_retaining_other_proposed_components=partial_root,example_root_partial=rr+2,example_before_N_partial=corrected_before,example_C1_N1_partial=corrected_before+24,example_N_ceilings_partial={str(C):(268435456-corrected_before)//(8+16*C) for C in (1,2)},unaffected_terms=['Q','LegacyFixed','variableCaptureFixed','perSearchRow','perMapRow','old190','old78','old13'],not_acceptance_of_other_prices=True)
changes={
    'ContextDecision':dict(component=2,constant=1,d=0,basis='Own ordinary Reader conditional at1576 omitted beside two dynamic predicates;L has no conditional caller.'),
    'LegacyClockEligibility':dict(component=3,constant=1,d=-1,basis='bit_width returns int;outer int-to-U64 conversion1,inner U64-to-size_t conversion d.'),
    'QueryQuote':dict(component=3,constant=1,d=-1,basis='Same int-to-U64 result conversion independent of input U64/size_t identity.'),
    'QueryPredictionEndpoints':dict(component=3,constant=1,d=-1,basis='bit_width of span size already uses size_t input;only result int-to-U64 conversion1.'),
    'ClipRow':dict(component=0,constant=1,d=0,basis='Logical vector stored-base R missing beside ci+two diagnostic oldvalues.'),
    'ClipFound':dict(component=0,constant=1,d=0,basis='Logical vector stored-base R missing beside ci+loopBreaks oldvalue.'),
    'AssetRow':dict(component=0,constant=1,d=0,basis='Logical vector stored-base R missing beside ai+clip pointer+two diagnostics.'),
    'AssetFound':dict(component=0,constant=1,d=0,basis='Logical vector stored-base R missing beside ai+loopBreaks oldvalue.'),
    'MapRow':dict(component=0,constant=1,d=0,basis='Logical vector stored-base R missing beside mi twice+diagnostic+bpm twice+curve twice.')}
overlay={}
for name,e in E.items():
    overlay[name]=e['price_coefficients'].copy()
    if name in changes:
        delta=changes[name]
        overlay[name][0]+=delta['constant'];overlay[name][2]+=delta['d']
        bv=e['body_vector'].copy();dv=e['body_d_vector'].copy()
        bv[delta['component']]+=delta['constant'];dv[delta['component']]+=delta['d']
        changes[name].update(ordinal=e['ordinal'],sealed_body=e['body_vector'],necessary_body_retaining_other_proposed_components=bv,sealed_body_d=e['body_d_vector'],necessary_body_d_retaining_other_proposed_components=dv,sealed_price=e['price_coefficients'],necessary_price_retaining_other_proposed_components=overlay[name])
OS={name:[sum(overlay[k][i]*n for k,n in s['site_counts'].items()) for i in range(3)] for name,s in S.items()}
affected={n:dict(sealed=S[n]['price_constant_B_d'],necessary_retaining_other_proposed_components=p) for n,p in OS.items() if p!=S[n]['price_constant_B_d']}
ck('affected schedules',sorted(affected),sorted(['WorkerOrReaderRole','LegacyFixed','PredictFixed','ResolveFixed','QueryNewFixed']))
root_overlay=ap(f['fixed_physical_prefix_constant_B_d'],*[OS[n] for n in root_roles])
ck('root necessary overlay',root_overlay,[51398,9,6])
search_overlay=f['perSearchRow_constant_B_d'].copy();search_overlay[0]+=2
map_overlay=f['perMapRow_constant_B_d'].copy();map_overlay[0]+=1
nn_overlay=nn+2*2+1
qq_overlay=qq+1
before_overlay=affine(root_overlay,4264,0)+nn_overlay+qq_overlay+7+4096+1024
ck('beforeN necessary overlay',before_overlay,11802973)
combined=dict(scope='Necessary conditional recomposition retaining every other source-price proposal,NOT numeric acceptance,whole fit or an actual trace.',affected_schedules=affected,root=root_overlay,perSearchRow=search_overlay,perMapRow=map_overlay,variableCaptureFixed=f['variableCaptureFixed_constant_B_d'],example=dict(root=affine(root_overlay,4264,0),needed=nn_overlay,Q=qq_overlay,before_N_term=before_overlay,C1_N1=before_overlay+24,N_ceilings={str(C):(268435456-before_overlay)//(8+16*C) for C in (1,2)}))
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
out=dict(scope='Whole sealed packet arithmetic, integrity and concrete independent source critique; no target/author code execution',verdict='REVISE',checks=len(checks),all_arithmetic_integrity_checks_passed=all(c['passed'] for c in checks),event_count=len(events),helper_count=len(H),schedule_count=len(S),source_anchors_checked=anchor_count,verified_files=sorted(records.values(),key=lambda r:r['path']),source_finding=finding,complete_necessary_source_findings=changes,combined_partial_recomposition=combined,independent_cell_formulas=cell_formulas,check_rows=checks)
(OUT/'packet-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({k:out[k] for k in ['verdict','checks','all_arithmetic_integrity_checks_passed','event_count','helper_count','schedule_count','source_anchors_checked','source_finding']}))
