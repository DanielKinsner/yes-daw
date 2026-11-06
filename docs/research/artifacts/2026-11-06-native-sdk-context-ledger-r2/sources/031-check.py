"""Complete bounded own SOURCE/DATA checker; no old helper or target runs."""
from pathlib import Path
from collections import Counter
import hashlib,json,re,traceback
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
OLD=ROOT/'build-ci/20261106-native-sdk-context-ledger-r1'
CRITIC=ROOT/'build-ci/20261106-native-sdk-context-independent-review-r1'
if (HERE/'first-green-freeze.json').exists():raise ValueError('Preserve first complete green')
checks=[];inputs={}
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def rel(p):
    try:return p.relative_to(ROOT).as_posix()
    except ValueError:return p.as_posix()
def read(p):
    b=p.read_bytes();inputs[rel(p)]=dict(path=rel(p),bytes=len(b),sha256=sha(b));return b
def data(p):return json.loads(read(p))
def ck(n,a,b=True):
    ok=a==b;checks.append(dict(name=n,passed=ok))
    if not ok:raise ValueError(n+': '+repr(a)+' != '+repr(b))
def exact(p,h):b=read(p);ck('exact '+rel(p),sha(b),h);return b
def dump(n,x):(HERE/n).write_text(json.dumps(x,indent=2)+'\n',encoding='utf-8',newline='\n')
def run():
    exact(OLD/'source-pins.json','814915EDB72EFD1049EF15D543C6647F1AD648A54214B1B80D6803695B0DD6FF')
    exact(OLD/'first-green-freeze.json','74717481007876FB89E4206DB4D2829AD0CE3FC0115ACBAD1772F70D320BEB43')
    exact(CRITIC/'source-pins.json','D66C6A8C97359967B11C8D4CBA591FE2862715929ED57C4BAE4951C8A3EAE7C8')
    exact(CRITIC/'first-green-freeze.json','F054826B2C1C16FE35DAAF9E5C3935241EB0E07540FCF9B82D0F919E0692BDC6')
    pri=data(HERE/'primary-source-pins.json');primary={}
    for r in pri['inputs']:
        p=Path(r['path']);p=p if p.is_absolute() else ROOT/p;b=read(p)
        ck('primary bytes '+r['path'],(len(b),sha(b)),(r['bytes'],r['sha256']))
    for alias,r in pri['aliases'].items():
        p=Path(r['path']);p=p if p.is_absolute() else ROOT/p;primary[alias]=read(p).decode('utf-8').replace('\r\n','\n')
    led=data(HERE/'selected-sdk-ledger.json');ctx=data(HERE/'context-joins.json');cl=data(HERE/'closure-incidence.json');facts=data(HERE/'required-type-facts.json');quotes=data(HERE/'quote-argument-incidence.json');layouts=data(HERE/'layout-source-correspondence.json');ing=data(HERE/'written-sdk-ingresses.json');msg=data(HERE/'message-copy-requests.json')
    old=data(OLD/'selected-sdk-ledger.json');ck('all167 row identities',len(led['rows']),167)
    rows={r['id']:r for r in led['rows']};ck('167 unique',len(rows),167);ck('whole predecessor inventory retained',set(rows),{r['id'] for r in old['rows']})
    alltypes=set(led['type_symbols'])|set(led['excluded_type_symbols'])
    for r in led['rows']:
        a,b=r['lines'];excerpt=''.join(primary[r['source']].splitlines(keepends=True)[a-1:b])
        ck('source text '+r['id'],excerpt,r['source_excerpt']);ck('source excerpt hash '+r['id'],sha(excerpt.encode()),r['source_excerpt_sha256'])
        ck('source anchor '+r['id'],r['declaration_anchor'] in excerpt)
        ck('new complete row assessment '+r['id'],r['whole_successor_assessment']['status'],'NEW_R2_UNACCEPTED_SOURCE_ASSESSMENT')
        for group in ['value_parameters','locals','materializations']:
            for i,o in enumerate(r[group]):
                ck('typed source object '+r['id']+'/'+str((group,i)),o['type'] in alltypes)
                ck('finite source multiplicity '+r['id']+'/'+str((group,i)),type(o['count']) is int and o['count']>0)
        for child in r['children']:ck('selected child exists '+r['id']+'/'+child,child in rows)
        ck('actual child authority '+r['id'],r['children'],r['selected_children'])
        if r['selected']:ck('no selected insert growth '+r['id'],not any(c in ['S_GROW_INSERT','L_INSERT_CALL'] for c in r['children']))
    ck('165 selected inventory rows',sum(r['selected'] for r in led['rows']),165)
    for n in ['S_GROW_INSERT','L_INSERT_CALL']:ck('excluded '+n,rows[n]['selected'],False)
    ck('real insert wrappers selected',rows['S_PLUS_PR']['selected'] and rows['S_INSERT_CSTR']['selected'] and rows['S_INSERT_COUNT']['selected'])
    ck('insert fast children',rows['S_INSERT_COUNT']['children'],['S_CHECK_OFFSET','S_PTR','TRAITS_MOVE','TRAITS_COPY'])
    for rate in [44100,48000,96000]:
        ck('rate five digits '+str(rate),len(str(rate)),5);ck('nongrowing insert '+str(rate),len(str(rate))+1<=15)
    ck('actual source first stem','"r"+number(R)+"-map"+number(std::uint64_t(map))' in primary['CPP'])
    ck('source SSO15','_Small_string_capacity' in primary['xstring'] and '_BUF_SIZE - 1' in primary['xstring'])
    ck('runtime insert overlap branch','_Count <= _Mypair._Myval2._Myres - _Old_size && !_STD is_constant_evaluated()' in primary['xstring'])
    ck('all12 contexts',len(ctx['contexts']),12);ck('all14 joins',len(ctx['critical_joins']),14);ck('all25 materializations',len(ctx['materialization_groups']),25)
    joins={j['id']:j for j in ctx['critical_joins']};contexts={c['id']:c for c in ctx['contexts']}
    ck('no phantom insert join','string_grow_insert' not in joins)
    ck('actual nongrowing join','string_insert_nongrowing' in joins)
    for c in ctx['contexts']:
        ck('no context numeric upper '+c['id'],c['ABI_or_size_upper'],None)
        for j in c['sdk_joins']:ck('context selected join '+c['id']+'/'+j,j in joins)
        ck('context25 not24 '+c['id'],'25' in c['source_component_join'])
    ck('retirement get plus action',contexts['retirement_failure']['maximum_own_closure_objects'],2)
    ck('entry quote own closures0',contexts['entry_quote']['maximum_own_closure_objects'],0)
    ck('fixture build get not yet constructed',contexts['fixture_build']['maximum_own_closure_objects'],0)
    ck('Tick sibling belongs correct join',joins['upper_tick4']['maximum_instances']['sibling_begin_iterator'],1)
    ck('prefix no sibling search','sibling_begin_iterator' not in joins['upper_prefix3']['maximum_instances'])
    ck('postsearch second begin retained',joins['upper_prefix3']['post_search_phase']['owners']['selected_else_begin_temporary'],1)
    ck('Time Tick same expression source','- changes_.begin());' in ''.join(primary['TIME'].splitlines(keepends=True)[649:653]))
    ck('Time prefix separate statements','const auto upper = std::upper_bound' in primary['TIME'].splitlines()[810] and 'i = upper ==' in primary['TIME'].splitlines()[811])
    ck('QC prefix separate statements','const auto upper=std::upper_bound' in primary['QC'].splitlines()[168] and 'const std::size_t i=upper==' in primary['QC'].splitlines()[169])
    for j in ctx['critical_joins']:
        for n in j['chain']:ck('critical chain source row '+j['id']+'/'+n,n in rows and rows[n]['selected'])
    ck('16 own closures',len(cl['own_closures']),16);ck('one selected SDK closure',len(cl['new_sdk_closures']),1)
    for c in cl['own_closures']:
        line=primary[c['source']].splitlines()[c['line']-1]
        ck('own closure exact line '+c['id'],line,c['source_line'])
        occurrences=list(re.finditer(r'\[(?:&)?\]\s*(?:\(|\{)',line))
        ck('own exact lambda occurrence '+c['id'],c['occurrence_on_line']<len(occurrences))
        ck('own closure size unqualified '+c['id'],c['sizeof'],None)
        for capture in c['captures']:ck('capture mode no repr guess '+c['id']+'/'+capture['name'],capture['mode'],'reference')
    needed=[f for f in facts['facts'] if f.get('needed_new_fact')]
    ck('minimal30 conditional',len(needed),30);ck('layout correspondence30',len(layouts['facts']),30)
    ck('layout identity set',set(f['id'] for f in layouts['facts']),set(f['id'] for f in needed))
    ck('L_INSERT unnecessary','L_INSERT' not in {f['id'] for f in facts['facts']})
    ck('PRED only alias',next(f for f in facts['facts'] if f['id']=='PRED')['type_alias_only'],True)
    for f in needed:ck('needed size/align null '+f['id'],(f['sizeof'],f['alignof']),(None,None))
    for f in layouts['facts']:
        a,b=f['lines'];s=''.join(primary[f['source']].splitlines(keepends=True)[a-1:b])
        ck('actual class layout source '+f['id'],s,f['source_excerpt']);ck('class source pin '+f['id'],sha(s.encode()),f['source_excerpt_sha256'])
        ck('no new measurement '+f['id'],f['measurement_performed'],False)
    # Whole written quote inventory, including definition-line bodies. This is
    # textual navigation plus manually authored typed trees, not target parsing.
    quote_names={'price','plus','times','units','wordUnits','inverseUnits','fixtureUnits','childUpper'}
    pattern=re.compile(r'(?<![A-Za-z0-9_])(?:(Cost)::)?('+'|'.join(sorted(quote_names,key=len,reverse=True))+r')\s*\(')
    actual={}
    for no,line in enumerate(primary['CPP'].splitlines(),1):
        visible=line.split('//',1)[0];found=[]
        for hit in pattern.finditer(visible):
            if not (54<=no<=103) and hit.group(1)!='Cost':continue
            if visible[:hit.start()].strip().startswith('static ') and '{' not in visible[:hit.start()]:continue
            found.append(hit.group(2))
        if found:actual[no]=Counter(found)
    ck('all27 quote source rows',len(quotes['rows']),27);ck('complete actual quote line inventory',{r['line'] for r in quotes['rows']},set(actual))
    sizes=quotes['selected_representation'];normal=set()
    layout_values=json.loads(primary['layout-values.json'])
    known={r['name']:r['value'] for r in layout_values['rows']}
    for t,n in {'FAMILY':'size_t.sizeof','U64':'u64.sizeof','B':'bool.sizeof','I32':'int.sizeof','P':'pointer.sizeof','WITNESS6':'Witness6.sizeof'}.items():
        ck('actual existing typed width '+t,sizes[t],known[n])
    signatures={
      'price':'price(Family id)', 'plus':'plus(std::uint64_t a,std::uint64_t b)',
      'times':'times(std::uint64_t count,std::uint64_t value)',
      'units':'units(Family id,std::uint64_t count)',
      'wordUnits':'wordUnits(bool verify,std::uint64_t words)',
      'inverseUnits':'inverseUnits(bool verify,std::uint64_t records)',
      'fixtureUnits':'fixtureUnits(bool verify,std::uint64_t N,std::uint64_t g,int map)',
      'childUpper':'childUpper(bool verify)', 'workLimit':'workLimit()',
      'reservationGate':'reservationGate(std::uint64_t total,std::uint64_t limit)',
      'gate3':'gate(bool ok,const char* r,std::array<std::uint64_t,6> v)',
      'Meter.add':'add(std::uint64_t n)'}
    actual_formal_types={
      'price':['FAMILY'],'plus':['U64','U64'],'times':['U64','U64'],
      'units':['FAMILY','U64'],'wordUnits':['B','U64'],'inverseUnits':['B','U64'],
      'fixtureUnits':['B','U64','U64','I32'],'childUpper':['B'],'workLimit':[],
      'reservationGate':['U64','U64'],'gate3':['B','P','WITNESS6'],'Meter.add':['U64']}
    for method,signature in signatures.items():
        ck('actual formal signature '+method,signature in primary['CPP'])
        ck('actual formal type tree '+method,[r[1] for r in quotes['formal_types'][method]],actual_formal_types[method])
    for r in quotes['rows']:
        ck('quote source exact '+str(r['line']),r['source_line'],primary['CPP'].splitlines()[r['line']-1])
        represented=Counter()
        for e in r['full_expressions']:
            params=e['all_actual_formal_owners'];ck('unique actual owners '+e['owner'],len({p['owner'] for p in params}),len(params))
            ck('typed union width '+e['owner'],e['formal_declaration_union_selected_bytes'],sum(sizes[p['type']] for p in params))
            for call in e['actual_call_activations']:
                normal.add(call['normal_body_owner'])
                if call['method'] in quote_names:represented[call['method']]+=1
                ck('normal body scope held '+call['activation'],call['normal_body_scope_status'],'INHERITED_HOLD_NOT_INDEPENDENTLY_ACCEPTED')
            for i,opt in enumerate(e['partial_lifetime_alternatives']):
                os=opt['partial_owners'];ck('partial actual typed widths '+e['owner']+'/'+str(i),opt['selected_representation_bytes'],sum(sizes[o['type']] for o in os))
                ck('partial object identity '+e['owner']+'/'+str(i),len(os),len({o['owner'] for o in os}))
                ck('partial not entered body '+e['owner']+'/'+str(i),all(not o['entered_body'] for o in os))
            ck('partial maximum '+e['owner'],e['maximum_partial_parameter_component'],max([0]+[o['selected_representation_bytes'] for o in e['partial_lifetime_alternatives']]))
        ck('written tree exact calls '+str(r['line']),represented,actual[r['line']])
    q={r['line']:r for r in quotes['rows']}
    ck('CPP65 actual typed times/price',Counter(c['method'] for c in q[65]['full_expressions'][0]['actual_call_activations']),Counter(['times','price']))
    ck('CPP65 partial outer count8',q[65]['full_expressions'][0]['maximum_partial_parameter_component'],8)
    ck('main287 partial outer plus8',q[287]['full_expressions'][0]['maximum_partial_parameter_component'],8)
    ck('main287 reservation before workLimit8',q[287]['full_expressions'][1]['maximum_partial_parameter_component'],8)
    ck('main288 pointer plus witness56',q[288]['full_expressions'][0]['maximum_partial_parameter_component'],56)
    ck('typed bool1/int4',sizes['B']==1 and sizes['I32']==4)
    ck('Family actual source underlying','enum class Family : std::size_t' in primary['SOURCE_COSTS'] if 'SOURCE_COSTS' in primary else any('enum class Family : std::size_t' in p for p in primary.values()))
    for r in ing['rows']:ck('written SDK ingress '+str((r['source'],r['line'])),r['source_line'],primary[r['source']].splitlines()[r['line']-1])
    for r in msg['messages']:
        ck('message literal source '+str((r['source'],r['line'])),r['literal'] in primary[r['source']].splitlines()[r['line']-1])
        ck('message copy literal plus NUL '+r['literal'],r['requested_copy_bytes'],len(r['literal'])+1)
    ck('path request upper<manual4096',max(641|15,527+527//2)+1,791)
    ck('vector96<manual4096',4*24<4096)
    ck('OTHER stays null',led['numeric_OTHER_upper'],None);ck('child stays null',led['whole_child_upper'],None);ck('fit stays null',led['joint32MiB_fit'],None)
    ck('no new measurements',facts['new_measurements_performed'],False)
    read(HERE/'argument.md')
    summary=dict(schema='R2_COMPLETE_NATIVE_SDK_CONTEXT_SOURCE_PROPOSAL',status='FIRST_COMPLETE_OWN_PASS_PENDING_FRESH_WHOLE_INDEPENDENT_REVIEW',
      rows=167,selected_inventory_rows=165,contexts=12,critical_joins=14,materialization_groups=25,own_closures=16,selected_SDK_closures=1,
      necessary_type_facts=30,all_new_type_values_null=True,typed_quote_source_rows=27,normal_body_requirements=sorted(normal),
      own93_scope='INHERITED_HOLD_NOT_INDEPENDENTLY_ACCEPTED',existing194_and405128_scope='ONLY_ORIGINAL_QUALIFIED_LAYOUT_AND_LARGE_COMPONENT_SCOPE_NO_NEW_JOIN_AUTHORITY',
      numeric_OTHER_upper=None,whole_child_upper=None,joint32MiB_fit=None,source_prices=None,Data=None,target_execution=False,new_measurement_or_purpose=False,independent_acceptance=False)
    dump('proposal.json',summary);dump('checks.json',dict(count=len(checks),passed=len(checks),checks=checks,source_only=True,target_execution=False))
    own=[]
    for p in sorted(HERE.rglob('*')):
        if p.is_file() and '__pycache__' not in p.parts:
            b=p.read_bytes();own.append(dict(path=rel(p),bytes=len(b),sha256=sha(b)))
    dump('source-pins.json',dict(schema='NATIVE_SDK_CONTEXT_R2_WHOLE_PINS',inputs=sorted(inputs.values(),key=lambda r:r['path']),own_files=own,
        no_old_helper_or_model_execution=True,no_target_execution=True))
    p=HERE/'source-pins.json';b=p.read_bytes();own.append(dict(path=rel(p),bytes=len(b),sha256=sha(b)))
    for r in own:
        b=(ROOT/r['path']).read_bytes()
        if (len(b),sha(b))!=(r['bytes'],r['sha256']):raise ValueError('whole firstpass freeze changed '+r['path'])
    dump('first-green-freeze.json',dict(schema='R2_FIRST_COMPLETE_OWN_WHOLE_SOURCE_FREEZE',files=own,checks=len(checks),independent_review_pending=True,target_execution=False))
    print(json.dumps(dict(status='COMPLETE_OWN_FIRST_GREEN_FROZEN_PROPOSAL',checks=len(checks),proposal_sha256=sha((HERE/'proposal.json').read_bytes()),pins_sha256=sha((HERE/'source-pins.json').read_bytes()),freeze_sha256=sha((HERE/'first-green-freeze.json').read_bytes()))))
if __name__=='__main__':
    try:run()
    except Exception:
        n=1
        while (HERE/('check-attempt-r'+str(n))).exists():n+=1
        dst=HERE/('check-attempt-r'+str(n));dst.mkdir()
        for p in HERE.iterdir():
            if p.is_file():(dst/p.name).write_bytes(p.read_bytes())
        (dst/'failure.json').write_text(json.dumps(dict(error=traceback.format_exc(),checks=len(checks),target_execution=False),indent=2)+'\n',encoding='utf-8',newline='\n')
        raise
