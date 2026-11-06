"""New complete native SDK/context R2 data author. No target or old helper runs.

R1 JSON is an unaccepted source inventory operand; every excerpt is reread from
its pinned primary. Actual selections and whole context/lifetime assertions are
recomposed for this successor, then checked by a separate own full checker.
"""
from pathlib import Path
import json,hashlib,copy,re,runpy,traceback
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
OLD=ROOT/'build-ci/20261106-native-sdk-context-ledger-r1'
CRITIC=ROOT/'build-ci/20261106-native-sdk-context-independent-review-r1'
if (HERE/'first-green-freeze.json').exists():raise ValueError('Preserve first complete green')
pins={}
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def rel(p):
    try:return p.relative_to(ROOT).as_posix()
    except ValueError:return p.as_posix()
def read(p):
    b=p.read_bytes();pins[rel(p)]=dict(path=rel(p),bytes=len(b),sha256=sha(b));return b
def data(p):return json.loads(read(p))
def save(n,x):(HERE/n).write_text(json.dumps(x,indent=2)+'\n',encoding='utf-8',newline='\n')
def run():
    oldpins=data(OLD/'source-pins.json');data(OLD/'first-green-freeze.json')
    data(CRITIC/'source-pins.json');data(CRITIC/'first-green-freeze.json');read(CRITIC/'review.md');data(CRITIC/'review.json')
    aliases={};primaries={}
    for r in oldpins['inputs']:
        p=Path(r['path']);p=p if p.is_absolute() else ROOT/p;b=read(p)
        if (len(b),sha(b))!=(r['bytes'],r['sha256']):raise ValueError('changed primary '+r['path'])
        key=r.get('alias',p.name);aliases[key]=dict(path=rel(p),bytes=len(b),sha256=sha(b));primaries[key]=b.decode('utf-8').replace('\r\n','\n')
    oldledger=data(OLD/'selected-sdk-ledger.json');oldctx=data(OLD/'context-joins.json');oldcl=data(OLD/'closure-incidence.json');oldfacts=data(OLD/'required-type-facts.json')
    ingresses=data(OLD/'written-sdk-ingresses.json');messages=data(OLD/'message-copy-requests.json');data(OLD/'quote-argument-incidence.json');read(OLD/'argument.md')
    rows=[]
    for operand in oldledger['rows']:
        r=copy.deepcopy(operand);text=primaries[r['source']];a,b=r['lines'];excerpt=''.join(text.splitlines(keepends=True)[a-1:b])
        if sha(excerpt.encode())!=r['source_excerpt_sha256']:raise ValueError('source row changed '+r['id'])
        r['source_excerpt']=excerpt;r['source_excerpt_sha256']=sha(excerpt.encode());r['source_pin']=aliases[r['source']]
        r['excerpt_normalization']='UTF8_DECODE_CRLF_TO_LF_TEXT_NAVIGATION_ONLY_RAW_PRIMARY_PINS_STAY_BYTE_EXACT'
        r['selected']=r['id'] not in ('S_GROW_INSERT','L_INSERT_CALL')
        r['selection_status']='ACTUAL_SELECTED_RUNTIME_OR_EXPLICIT_CONSERVATIVE_SOURCE_ALTERNATIVE' if r['selected'] else 'EXCLUDED_FIXED_INGRESS_NO_INSERT_GROWTH'
        if r['id']=='I_CTOR':r['selection_status']='EXPLICIT_COMPILER_HEADER_FORMATION_ALTERNATIVE_NOT_CLAIMED_INVOCATION'
        if r['id']=='PRIMITIVE':r['selection_status']='EXISTING_PRIMITIVE_BOUNDARY_NOT_A_CLAIM_OF_ZERO_NATIVE_OWNERS'
        r['selected_children']=[n for n in r['children'] if n not in ('S_GROW_INSERT','L_INSERT_CALL')]
        r['inactive_child_alternatives']=[n for n in r['children'] if n not in r['selected_children']]
        if not r['selected']:r['selected_children']=[]
        r['source_header_alternative_children']=list(r['children'])
        r['children']=list(r['selected_children'])
        r['whole_successor_assessment']=dict(status='NEW_R2_UNACCEPTED_SOURCE_ASSESSMENT',
          owner_rule='Value formals/actual locals/materializations are distinct objects; reference bindings alias existing owners; embedded members remain within qualified containing layouts.',
          lifetime_rule='Join the entered selected chain and actual surrounding full-expression objects. Max over sequential/alternative phases; retain permitted delayed parameter teardown. No physical-frame or free/GC inference.',
          return_rule=r['return_rule'],profile='Pinned MSVC14.44/x64/C++20/default allocator/iterator-debug0 selected source; all native/compiler/ABI association scope retained.')
        if r['id']=='S_INSERT_COUNT':
            r['whole_successor_assessment']['selection_proof']='Only selected pointer+rvalue ingress is CPP251 r+number(R). R has5decimal characters, insertion count1/off0 and SSOcapacity15: _Check_overlap true, result6. All real fast-path formals/locals remain; growth callback is not entered.'
        if r['id'] in ('S_GROW_APPEND','L_APPEND_CALL'):
            r['whole_successor_assessment']['selection_proof']='Path(root,relative) rvalue append can exceed previous capacity; exact1524 callback initializes Fn directly. Allocation and callback phases sequential; Fn survives its own operator value parameters.'
        rows.append(r)
    if len(rows)!=167:raise ValueError('whole167 inventory')
    symbols=copy.deepcopy(oldledger['type_symbols']);excluded=symbols.pop('L_INSERT')
    symbols.update(U64=dict(size=8,basis='actual194 u64.sizeof'),I32=dict(size=4,basis='actual194 int.sizeof'),FAMILY=dict(size=8,basis='source enum Family underlying std::size_t and actual194 size_t.sizeof'),WITNESS6=dict(size=48,basis='actual194 Witness6.sizeof'))
    profile=copy.deepcopy(oldledger['profile'])
    profile['excluded'].append(dict(id='insert_growth_fixed_ingress',proof='CPP251 only r+number(R); R is44100/48000/96000, decimal5; add1 fits selected char SSOcapacity15. Nongrowing insert remains selected. No value/domain restriction is added.'))
    ledger=dict(schema='NATIVE_SELECTED_SDK_CONTEXT_SOURCE_PROPOSAL_R2',rows=rows,type_symbols=symbols,
      excluded_type_symbols=dict(L_INSERT=dict(**excluded,selected=False,necessary_new_fact=False)),
      profile=profile,selected_dispatch=copy.deepcopy(oldledger['selected_dispatch']),
      independently_accepted=False,numeric_OTHER_upper=None,whole_child_upper=None,joint32MiB_fit=None,
      source_scope='Every167 source row retained for audit; excluded alternatives do not supply selected owners or minimal layout requests. No old source price or whole acceptance transfers.',
      coexistence_algebra='Unique source owner identities + actual entered chain + retained full-expression destinations; take max over sequential phases and permitted evaluation orders. Complete own93 context remains inherited HOLD.',
      execution=False)
    joins=copy.deepcopy(oldctx['critical_joins'])
    for j in joins:
        if j['id']=='string_grow_insert':
            j.clear();j.update(id='string_insert_nongrowing',chain=['S_PLUS_PR','S_INSERT_CSTR','S_INSERT_COUNT','S_CHECK_OFFSET','S_PTR','TRAITS_MOVE','TRAITS_COPY'],
              persistent='number(R) String result5/capacity15, insert1/off0 and actual move-return destination. Old_size/shifted_count/check_overlap/Old_ptr/Insert_at are the real fast-path locals. Source and destination headers are actual distinct owners.',
              materializations='No insert-growth Fn/closure/operator is constructed. Pointer/count/off by-value formals remain actual source objects. This filename-preparation phase is separate from active metadata I/O.',
              maximum_instances=dict(SSO_source_String=1,actual_move_destination_String=1,insert_growth_closure=0),
              source_ingress=dict(source='CPP',line=251,R=[44100,48000,96000],digits=5,insert_count=1,selected_capacity=15,result_length=6))
        elif j['id']=='upper_tick4':
            j['maximum_instances']['sibling_begin_iterator']=1
            j['materializations']='Time650-653 alone subtracts a sibling changes.begin() in the SAME full expression. Either operand may evaluate first; retain its separate ITER result8 during search. Returned search iterator is a distinct actual destination because return First cannot use NRVO. QC102-104 and Time754-756 store upper in a previous statement, so have no search sibling.'
            j['source_cases']=[dict(source='TIME',lines=[650,653],search_sibling_iterators=1,closure='time_tick_comparator'),dict(source='QC',lines=[102,104],search_sibling_iterators=0,closure='qc_tick_comparator'),dict(source='TIME',lines=[754,756],search_sibling_iterators=0,closure='time_native_comparator')]
            j['sibling_construction_phase']=dict(chain=['V_BEGINEND','V_ITER_CTOR','FA_ADOPT'],iterator_result='ITER',actual_value_formals=dict(P=3),rule='Two iterator constructor pointer formals plus one Adopt pointer formal. Formation and active search are separate phases; allowed full-expression parameter teardown alternative remains explicit, never zeroed or treated as a referent copy.')
        elif j['id']=='upper_prefix3':
            j['maximum_instances'].pop('sibling_begin_iterator',None)
            j['materializations']='QC169 and Time811 store upper_bound result in a separate full expression; no later begin result exists during that search. OuterUB3 iterator formals and innerUB4 copies remain4, LESS one direct value formal, actual return destination one. The following conditional comparison/subtraction is a distinct phase.'
            j['post_search_phase']=dict(source_cases=[dict(source='QC',lines=[170,171]),dict(source='TIME',lines=[812,812])],
              owners=dict(named_upper_iterator=1,condition_begin_temporary=1,selected_else_begin_temporary=1),
              active_begin_helper_chain=['V_BEGINEND','V_ITER_CTOR','FA_ADOPT'],active_helper_value_formals=dict(P=3),
              rule='The condition begin temporary survives the full expression into the selected else subtraction. These are post-search objects, not an extra UB3/UB4 search iterator. Named upper is an existing own-body owner once.')
        j['context_authority']='NEW_R2_UNACCEPTED_FULL_SOURCE_JOIN; exact selected type/layout facts still required'
    if len(joins)!=14:raise ValueError('whole14 joins')
    mats=copy.deepcopy(oldctx['materialization_groups'])
    for g in mats:
        if g['id']=='quote_argument_partial_lifetimes':
            g['lines']=[65,73,77,78,80,89,90,91,92,96,99,101,187,245,250,261,285,287,288]
            g['objects']='Unique actual typed formal destinations in quote-argument-incidence.json, including inline times/price, nested main plus, reservation/workLimit and verify-only witness-gate partial activation.'
            g['rule']='Entered body formals remain normal own93 requirements. Already initialized outer parameters and permitted delayed-teardown parameters have distinct activation IDs; unite identities once, never a blanket count*8 or another full-body fee.'
        g['status']='NEW_R2_SOURCE_MATERIALIZATION_CONTEXT_PROPOSAL_NO_ADDITIVE_WHOLE_UPPER'
    contexts=copy.deepcopy(oldctx['contexts'])
    for c in contexts:
        c['sdk_joins']=[('string_insert_nongrowing' if n=='string_grow_insert' else n) for n in c['sdk_joins']]
        c['source_component_join']='Actual25 materialization groups, unique typed partial/full-expression owner incidences, selected SDK phase chain and exact own closure/body objects. Own93 normal-body/context completeness remains inherited HOLD; this is not a numeric whole upper.'
        c['phase_algebra']='Max of explicitly sequential phases; active enclosing named owners persist. No loop-count live-object multiplier, physical stack-slot reuse or heap-free credit.'
        c['ABI_or_size_upper']=None
        if c['id']=='entry_quote':
            c['maximum_own_closure_objects']=0;c['maximum_SDK_growth_closure_objects']=0
            c['source_phase_note']='Initial argv String construction precedes quote evaluation. Quote methods have no own lambda or String growth callback; exception message constructors are a separate selected chain.'
        elif c['id']=='fixture_build':
            c['maximum_own_closure_objects']=0
            c['source_phase_note']='CPP247 builds generator/Production before the get object at251. Rate-cache nextBit invocation ended before this fixture build.'
        elif c['id'] in ('metadata_read','metadata_write'):
            c['source_phases']=[dict(name='filename_preparation',source_lines=[251,257,260],selected_joins=['string_construct','string_concat_lvalue','string_insert_nongrowing','string_grow_append','string_take'],relation='stem insert only at251 before q; active helper streams are not invented during that preparation'),dict(name='active_IO',source_lines=[257,260],selected_joins=['exception_control_nested'] if c['id']=='metadata_read' else [],relation='q remains a named owner; one helper Stream active. String naming full expressions are distinct from earlier stem construction.')]
        elif c['id']=='retirement_failure':
            c['maximum_own_closure_objects']=2;c['maximum_SDK_growth_closure_objects']=1
            c['source_phase_note']='CPP255 truncated-record Expect keeps its action parameter alive in the handler while named get251 remains live:2 own closure objects. A separate path-append allocation-failure context can retain Fn; maxima are alternatives, not one fabricated joint trace.'
        elif c['id']=='common_controls':
            c['source_phase_note']='One action or one comparator/nextBit at a time. The invalid-native action has frac==1 and returns InvalidInput before Time search; do not invent action+comparator coexistence on that rejected branch.'
        elif c['id']=='fixture_controls':
            c['maximum_SDK_growth_closure_objects']=0
            c['source_phase_note']='get remains live alongside one Expect action. Short/trailing controls use direct lvalue+C-string concat; action bodies do not enter a path append-growth callback. Frame computations for prefix/ramp mutants precede their actions.'
    closures=copy.deepcopy(oldcl)
    excluded_sdk=[r for r in closures['new_sdk_closures'] if 'insert' in r['id'].lower()]
    closures['new_sdk_closures']=[r for r in closures['new_sdk_closures'] if 'insert' not in r['id'].lower()]
    closures['SDK_count']=len(closures['new_sdk_closures']);closures['excluded_unreached_SDK_closures']=excluded_sdk
    closures['layout_credit']='NONE_FOR_ALL16_OWN_PLUS_ONE_SELECTED_SDK_CLOSURE; ACTUAL_SOURCE_IDENTITIES_ONLY'
    closures['rule']='Actual direct/named/value-formal instances; no capture-count representation inference. The excluded insert lambda is neither reached nor a necessary type fact.'
    facts=copy.deepcopy(oldfacts)
    facts['facts']=[f for f in facts['facts'] if f['id']!='L_INSERT']
    for f in facts['facts']:
        if f.get('needed_new_fact'):
            f['sizeof']=None;f['alignof']=None;f['necessity_status']='CONDITIONAL_SOURCE_MINIMALITY_PROPOSAL_REQUIRES_FRESH_WHOLE_REVIEW'
    facts['minimality']='30 candidate necessary null facts:16 own closures,1 append closure,TempoMapView,nullptr_t,LESS,EQ,2allocator specializations,2fake types,actual vector tidy guard,3tags,EXDATA. L_INSERT excluded. PRED only selects an actual closure/LESS, never another type.'
    facts['new_measurements_performed']=False;facts['minimal_required_null_count']=sum(f.get('needed_new_fact',False) for f in facts['facts'])
    quotes=runpy.run_path(str(HERE/'quote_models.py'))['derive'](primaries['CPP'].splitlines())
    layouts=runpy.run_path(str(HERE/'layout_correspondence.py'))['derive'](facts,closures,primaries,aliases)
    save('selected-sdk-ledger.json',ledger);save('context-joins.json',dict(contexts=contexts,materialization_groups=mats,critical_joins=joins,
        scope='Complete new SDK/context incidence proposal, not independent own93 completeness or whole numeric OTHER/fit.'))
    save('closure-incidence.json',closures);save('required-type-facts.json',facts);save('quote-argument-incidence.json',quotes)
    save('layout-source-correspondence.json',layouts)
    save('written-sdk-ingresses.json',ingresses);save('message-copy-requests.json',messages)
    save('primary-source-pins.json',dict(inputs=sorted(pins.values(),key=lambda r:r['path']),aliases=aliases,
        old_data_status='UNACCEPTED_SOURCE_INVENTORY_OPERANDS_ONLY_NO_OLD_HELPER_EXECUTED',no_target_execution=True))
    save('authoring.json',dict(stage='PREPARED_COMPLETE_R2_DATA_NOT_YET_WHOLE_CHECKED',rows=len(rows),contexts=len(contexts),joins=len(joins),materialization_groups=len(mats),own_closures=len(closures['own_closures']),selected_SDK_closures=closures['SDK_count'],conditional_needed_type_facts=facts['minimal_required_null_count'],quote_source_rows=len(quotes['rows']),all_numeric_OTHER_child_jointfit=None,target_execution=False))
    print(json.dumps(dict(stage='PREPARED_NOT_YET_COMPLETE_OWN_GREEN',rows=len(rows),contexts=len(contexts),joins=len(joins),missing=facts['minimal_required_null_count'],quote_rows=len(quotes['rows']))))
if __name__=='__main__':
    try:run()
    except Exception:
        n=1
        while (HERE/('author-attempt-r'+str(n))).exists():n+=1
        dst=HERE/('author-attempt-r'+str(n));dst.mkdir()
        for p in HERE.iterdir():
            if p.is_file():(dst/p.name).write_bytes(p.read_bytes())
        (dst/'failure.json').write_text(json.dumps(dict(error=traceback.format_exc(),target_execution=False),indent=2)+'\n',encoding='utf-8',newline='\n')
        raise
