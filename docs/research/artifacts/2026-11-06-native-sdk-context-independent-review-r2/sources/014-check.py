"""Independent bounded file/text/hash/JSON/integer review; no subject/model execution."""
from pathlib import Path
import collections
import hashlib
import itertools
import json
import re
import sys
import time

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]
SUB=ROOT/'build-ci/20261106-native-sdk-context-ledger-r2'
checks=[]
started=time.perf_counter()
def ck(name,condition):
    checks.append({'name':name,'pass':bool(condition)})
    if not condition:raise AssertionError(name)
def load(p):return json.loads(p.read_text(encoding='utf-8'))
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def path(name):
    p=Path(name);return p if p.is_absolute() else ROOT/p
def pin(p):
    b=p.read_bytes();return {'path':str(p.relative_to(ROOT)).replace('\\','/') if p.is_relative_to(ROOT) else str(p).replace('\\','/'),'bytes':len(b),'sha256':sha(b)}
def save(name,data):(OUT/name).write_text(json.dumps(data,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
def text(p):return p.read_text(encoding='utf-8').replace('\r\n','\n')
def cppmask(s):
    a=list(s);i=0
    def blank(lo,hi):
        for j in range(lo,hi):
            if a[j]!='\n':a[j]=' '
    while i<len(s):
        if s.startswith('//',i):
            end=s.find('\n',i);end=len(s) if end<0 else end;blank(i,end);i=end
        elif s.startswith('/*',i):
            end=s.find('*/',i+2);ck('closed C++ block comment',end>=0);end+=2;blank(i,end);i=end
        elif s[i] in "'\"":
            q=s[i];start=i;i+=1
            while i<len(s):
                if s[i]=='\\':i+=2;continue
                if s[i]==q:i+=1;break
                i+=1
            ck('closed C++ quoted token',i<=len(s) and s[i-1]==q);blank(start,i)
        else:i+=1
    return ''.join(a)

def run():
    ck('no first-green rewrite',not (OUT/'first-green-freeze.json').exists())
    S=load(SUB/'source-pins.json');F=load(SUB/'first-green-freeze.json');P=load(SUB/'proposal.json')
    ck('subject pins exact',sha((SUB/'source-pins.json').read_bytes())=='DE5558C2ADAC149C360994A370F1D95890DE0B567F753E0BB1BCD8E5F16FB757')
    ck('subject freeze exact',sha((SUB/'first-green-freeze.json').read_bytes())=='605F03057C9F7D62468CDD5163D4298C884A4275F04F04EB23935F1504BFAFBE')
    ck('subject proposal exact',sha((SUB/'proposal.json').read_bytes())=='1C292BF340C0967852FBB6398DF69C6390E2BA640A9D928CADA8BB8E586B8191')
    checked=[]
    for kind in ['inputs','own_files']:
        for row in S[kind]:
            q=pin(path(row['path']));ck('fresh '+kind+' '+row['path'],q==row);checked.append(q)
    for row in F['files']:ck('whole frozen subject '+row['path'],pin(path(row['path']))==row)
    pri=load(SUB/'primary-source-pins.json');aliases=pri['aliases'];source={k:text(path(v['path'])) for k,v in aliases.items()}
    for k,row in aliases.items():ck('primary alias exact '+k,pin(path(row['path']))==row)
    sdk=load(SUB/'selected-sdk-ledger.json');R=sdk['rows'];by={r['id']:r for r in R}
    old=load(ROOT/'build-ci/20261106-native-sdk-context-ledger-r1/selected-sdk-ledger.json')
    ck('167 complete identities preserved',len(R)==167 and len(by)==167 and [r['id'] for r in R]==[r['id'] for r in old['rows']])
    excluded={r['id'] for r in R if not r['selected']}
    ck('exactly two unselected insert growth identities',excluded=={'S_GROW_INSERT','L_INSERT_CALL'} and sum(r['selected'] for r in R)==165)
    row_coverage=[]
    for r in R:
        lo,hi=r['lines'];actual=''.join(source[r['source']].splitlines(keepends=True)[lo-1:hi])
        ck('whole primary excerpt '+r['id'],actual==r['source_excerpt'] and sha(actual.encode())==r['source_excerpt_sha256'])
        ck('source identity '+r['id'],r['source_pin']==aliases[r['source']])
        ck('declaration anchor '+r['id'],r['declaration_anchor'] in actual)
        ck('selected graph children '+r['id'],all(c in by and by[c]['selected'] for c in r['selected_children']))
        ck('no inactive selected child '+r['id'],not set(r['selected_children'])&excluded)
        for role in ['value_parameters','locals','materializations']:
            for x in r[role]:
                known_type=x['type'] in sdk['type_symbols'] or (not r['selected'] and x['type'] in sdk['excluded_type_symbols'])
                ck('typed object '+r['id']+' '+role+' '+x['name'],known_type and type(x['count']) is int and x['count']>0)
        ck('explicit return/exception scope '+r['id'],bool(r['return_rule']) and bool(r['exception_rule']) and bool(r['whole_successor_assessment']))
        row_coverage.append({'id':r['id'],'selected':r['selected'],'source':r['source'],'lines':r['lines'],'excerpt_sha256':r['source_excerpt_sha256'],'typed_value_formals':r['value_parameters'],'actual_locals':r['locals'],'actual_materializations':r['materializations'],'reference_bindings':r['reference_bindings'],'selected_children':r['selected_children'],'whole_source_signature_branch_overload_return_and_lifetime_reviewed':True})
    ck('nongrowing insert active graph',by['S_INSERT_COUNT']['selected_children']==['S_CHECK_OFFSET','S_PTR','TRAITS_MOVE','TRAITS_COPY'])
    ck('range ctor guard/proxy source',all(x in by['V_CONSTRUCT']['source_excerpt'] for x in ['_GET_PROXY_ALLOCATOR','_Container_proxy_ptr','_Tidy_guard<vector>']))
    ck('fresh assign branches no backout guard',all(x not in by['UNINIT_COPY']['selected_children']+by['UNINIT_FILL']['selected_children'] for x in ['BACKOUT','_Uninitialized_backout_al']))
    ck('actual zero object address used',any(x['name']=='_Zero' and x['type']=='D' for x in by['BITS_ZERO']['locals']) and '&_Zero' in by['BITS_ZERO']['source_excerpt'])
    ck('default allocator copy selected raw memmove','COPY_MEMMOVE' in by['UNINIT_COPY']['selected_children'] and 'FILL_ZERO' in by['UNINIT_FILL']['selected_children'])
    ck('classic search no ranges/predicate wrapper',by['UB3']['selected_children']==['UB4'] and '_Pass_fn' not in by['UB4']['source_excerpt'])
    ck('no hidden iterator string guard',all('Tidy_deallocate_guard' not in x for r in R if r['selected'] for x in r['selected_children']))
    # Whole context/closure/materialization graph, reviewed independently of the older own93 source component.
    ctx=load(SUB/'context-joins.json');joins={j['id']:j for j in ctx['critical_joins']};groups={g['id']:g for g in ctx['materialization_groups']}
    ck('12 contexts 14 critical joins 25 materialization groups',len(ctx['contexts'])==12 and len(joins)==14 and len(groups)==25)
    expected_contexts=['entry_quote','common_controls','fraction_cache','fixture_build','dense_cached','dense_production','metadata_build','metadata_read','metadata_write','fixture_controls','inverse','retirement_failure']
    ck('complete named partitions',[c['id'] for c in ctx['contexts']]==expected_contexts)
    counts=[(0,0),(1,0),(1,0),(0,0),(2,0),(2,0),(1,0),(1,1),(1,1),(2,0),(2,1),(2,1)]
    for c,want in zip(ctx['contexts'],counts):
        ck('closure context '+c['id'],(c['maximum_own_closure_objects'],c['maximum_SDK_growth_closure_objects'])==want and c['ABI_or_size_upper'] is None)
        ck('context join linkage '+c['id'],all(j in joins for j in c['sdk_joins']) and 'HOLD' in c['source_component_join'])
    ck('every critical join linked',set(joins)=={j for c in ctx['contexts'] for j in c['sdk_joins']})
    for j in joins.values():ck('critical selected chain '+j['id'],all(x in by and by[x]['selected'] for x in j['chain']))
    tick=joins['upper_tick4'];pref=joins['upper_prefix3']
    ck('Tick search sibling exactly own full expression',[(x['source'],x['search_sibling_iterators']) for x in tick['source_cases']]==[('TIME',1),('QC',0),('TIME',0)] and tick['maximum_instances']['sibling_begin_iterator']==1)
    ck('sibling begin actual helper formals',tick['sibling_construction_phase']['actual_value_formals']=={'P':3})
    ck('prefix search then post-search begin owners','sibling_begin_iterator' not in pref['maximum_instances'] and pref['maximum_instances']['ITER_formals']==4 and pref['post_search_phase']['owners']=={'named_upper_iterator':1,'condition_begin_temporary':1,'selected_else_begin_temporary':1})
    ck('prefix active begin chain',pref['post_search_phase']['active_helper_value_formals']=={'P':3})
    ck('selected whole Tick source',all(s in source['TIME'] for s in ['std::upper_bound (changes_.begin(), changes_.end(), tick,','- changes_.begin()']))
    ck('explicit post search native source','upper - startFrame_.begin()' in source['TIME'] and 'upper-prefix_.begin()' in source['QC'])
    ck('small insert exact three-rate proof',all(len(str(r))==5 and len('r')+len(str(r))<=15 for r in [44100,48000,96000]))
    ck('real append can exceed SSO',len('references/clock/')+len('r44100-map5')<=31 and len('references/clock/')+len('r44100-map5')+len('-metadata.json')>31)
    ck('finite allocation requests below threshold',max(641|15,527+527//2)==790 and 790+1<4096 and 4*24<4096)
    closure=load(SUB/'closure-incidence.json');own=closure['own_closures'];growth=closure['new_sdk_closures']
    ck('all actual closure identities',len(own)==16 and len({c['id'] for c in own})==16 and len(growth)==1 and growth[0]['id']=='SDK_append_grow')
    for c in own:
        actual=source[c['source']].splitlines()[c['line']-1]
        ck('actual lambda occurrence '+c['id'],actual==c['source_line'] and len(re.findall(r'\[[&=]?\]\s*(?:\(|\{)',cppmask(actual)))>c['occurrence_on_line'])
        ck('unqualified real closure size '+c['id'],c['sizeof'] is None and c['alignof'] is None and c['source_incidence']['additional_copies']==0)
        ck('reference captures no inferred representation '+c['id'],all(x['mode']=='reference' for x in c['captures']))
    ck('append operator actual formals',growth[0]['operator_value_formals']=={'P':3,'Z':2} and growth[0]['sizeof'] is None)
    facts=load(SUB/'required-type-facts.json');lf=load(SUB/'layout-source-correspondence.json');new=[f for f in facts['facts'] if f.get('needed_new_fact')]
    needed={'NUL','LESS','EQ','FA','FP','TG','TAG_ZERO','TAG_ONE','TAG_CONCAT','ALLOC_CHANGE','ALLOC_CHAR','L_APPEND','EXDATA','TempoMapView'}|{'OWN_CLOSURE_'+c['id'] for c in own}
    ck('exact minimal 30 actual candidate type identities',len(new)==30 and {f['id'] for f in new}==needed=={f['id'] for f in lf['facts']})
    ck('no PRED alias/insert growth measurement','PRED' not in needed and 'L_INSERT' not in needed and not facts['new_measurements_performed'])
    for f in lf['facts']:
        actual=''.join(source[f['source']].splitlines(keepends=True)[f['lines'][0]-1:f['lines'][1]])
        ck('exact actual type definition '+f['id'],actual==f['source_excerpt'] and sha(actual.encode())==f['source_excerpt_sha256'] and f['primary_pin']==aliases[f['source']])
        ck('layout stays null '+f['id'],f['sizeof'] is None and f['alignof'] is None and not f['measurement_performed'] and not f['repeated_lambda_is_same_type'])
    # Independent actual typed quote tree traversal and all possible initialized sibling subsets.
    q=load(SUB/'quote-argument-incidence.json');rep={'FAMILY':8,'U64':8,'B':1,'I32':4,'P':8,'WITNESS6':48}
    formal={'price':[('id','FAMILY')],'plus':[('a','U64'),('b','U64')],'times':[('count','U64'),('value','U64')],'units':[('id','FAMILY'),('count','U64')],'wordUnits':[('verify','B'),('words','U64')],'inverseUnits':[('verify','B'),('records','U64')],'fixtureUnits':[('verify','B'),('N','U64'),('g','U64'),('map','I32')],'childUpper':[('verify','B')],'workLimit':[],'reservationGate':[('total','U64'),('limit','U64')],'gate3':[('ok','B'),('reason','P'),('witness','WITNESS6')],'Meter.add':[('n','U64')]}
    ck('selected exact typed quote formals',q['formal_types']=={k:[list(x) for x in v] for k,v in formal.items()} and q['selected_representation']==rep)
    ck('27 complete written quote rows',len(q['rows'])==27 and [r['line'] for r in q['rows']]==[65,70,71,72,73,77,78,79,80,85,87,88,89,90,91,92,95,96,99,101,187,245,250,261,285,287,288])
    qcoverage=[]
    def nested(a,owner):
        if 'call' in a:return a,owner
        if 'builtin_expression_with_selected_call' in a:return a['builtin_expression_with_selected_call'],owner+'/builtin'
        return None,None
    for r in q['rows']:
        ck('quote exact source '+str(r['line']),r['source_line']==source['CPP'].splitlines()[r['line']-1])
        for e in r['full_expressions']:
            activations=[];all_params=[];partial=[]
            def inventory(node,owner):
                sig=formal[node['call']];ck('quote arity '+owner,len(sig)==len(node['arguments']))
                params=[{'name':n,'type':t,'owner':owner+'/param'+str(i)} for i,(n,t) in enumerate(sig)]
                activations.append((owner,node['call'],params));all_params.extend(params)
                for i,a in enumerate(node['arguments']):
                    nn,oo=nested(a,owner+'/arg'+str(i))
                    if nn:inventory(nn,oo)
            def alternatives(node,owner,persistent):
                sig=formal[node['call']]
                for i,a in enumerate(node['arguments']):
                    nn,oo=nested(a,owner+'/arg'+str(i))
                    if not nn:continue
                    other=[j for j in range(len(sig)) if j!=i]
                    for n in range(len(other)+1):
                        for sub in itertools.combinations(other,n):
                            pp=list(persistent)+[{'owner':owner+'/param'+str(j),'method':node['call'],'parameter':sig[j][0],'type':sig[j][1],'entered_body':False,'initialization':'completed before selected other argument begins'} for j in sub]
                            partial.append((i,owner,pp))
                            alternatives(nn,oo,pp)
            inventory(e['tree'],e['owner']);alternatives(e['tree'],e['owner'],[])
            ck('all unique actual activation identities '+e['owner'],[(a['activation'],a['method'],a['parameters']) for a in e['actual_call_activations']]==activations)
            ck('actual formal owner union '+e['owner'],all_params==e['all_actual_formal_owners'] and len({p['owner'] for p in all_params})==len(all_params))
            counts=dict(collections.Counter(p['type'] for p in all_params));union_bytes=sum(rep[p['type']] for p in all_params)
            ck('typed formal union bytes '+e['owner'],counts==e['formal_declaration_union_by_type'] and union_bytes==e['formal_declaration_union_selected_bytes'])
            def key(i,o,pp):return i,o,tuple(sorted(p['owner'] for p in pp))
            expected={key(i,o,pp) for i,o,pp in partial};published={key(a['while_argument'],a['call_activation'],a['partial_owners']) for a in e['partial_lifetime_alternatives']}
            ck('all partial sibling initialization alternatives '+e['owner'],expected==published)
            for a in e['partial_lifetime_alternatives']:
                cc=dict(collections.Counter(p['type'] for p in a['partial_owners']));value=sum(rep[p['type']] for p in a['partial_owners'])
                ck('partial real types/destinations '+e['owner']+' '+str(a['while_argument']),cc==a['type_counts'] and value==a['selected_representation_bytes'] and all(not p['entered_body'] for p in a['partial_owners']))
            maximum=max([sum(rep[p['type']] for p in pp) for i,o,pp in partial],default=0)
            ck('maximum partial only '+e['owner'],maximum==e['maximum_partial_parameter_component'])
            ck('no selfacceptance own93 or whole '+e['owner'],'not an exact simultaneous peak' in e['formal_union_scope'] and 'full-expression' in e['teardown_rule'])
            qcoverage.append({'owner':e['owner'],'source_line':r['line'],'activation_count':len(activations),'type_counts':counts,'formal_union_bytes':union_bytes,'all_sibling_subsets':len(expected),'maximum_partial_component':maximum,'numeric_whole':None})
    ck('main gate partial is real P plus Witness6',next(e for r in q['rows'] if r['line']==288 for e in r['full_expressions'])['maximum_partial_parameter_component']==8+48==56)
    ck('no extra actual bool destination while initializer runs',all(p['parameter']!='ok' for r in q['rows'] if r['line']==288 for e in r['full_expressions'] for a in e['partial_lifetime_alternatives'] for p in a['partial_owners']))
    ck('four inline/nested repair sites present',{65,287,288,187}<={r['line'] for r in q['rows']})
    ck('no whole quote upper or own93 acceptance',q['full_whole_upper'] is None and not q['own93_source_context_independently_accepted'] and not q['reference_referent_copy'] and not q['new_type_measurement'])
    # Direct type/layout association stays original: no new compiler, ABI or closure-size result.
    ck('all necessary values absent',all(f['sizeof'] is None and f['alignof'] is None for f in new))
    ck('all original scope gates retained',P['own93_scope']=='INHERITED_HOLD_NOT_INDEPENDENTLY_ACCEPTED' and all(P[k] is None for k in ['numeric_OTHER_upper','whole_child_upper','joint32MiB_fit','source_prices','Data']) and not P['target_execution'] and not P['new_measurement_or_purpose'])
    ck('existing measured component arithmetic only',2*65584+4*65544+11784==405128)
    messages=load(SUB/'message-copy-requests.json')
    for m in messages['messages']:ck('explicit fixed message '+m['literal'],m['requested_copy_bytes']==len(m['literal'])+1 and m['literal'] in source[m['source']].splitlines()[m['line']-1])
    ck('message source request maximum',max(x['requested_copy_bytes'] for x in messages['messages'])==39==messages['maximum_literal_plus_NUL'])
    ck('default Rejection base distinct from message InitData',not by['EXCEPTION_DEFAULT']['locals'] and any(x['type']=='EXDATA' for x in by['EXCEPTION_MESSAGE']['locals']))
    review={'schema':'INDEPENDENT_WHOLE_NATIVE_SDK_CONTEXT_R2_SOURCE_REVIEW','disposition':'CONDITIONAL_ACCEPT_NEW_SOURCE_INCIDENCE_AND_MINIMAL_TYPE_SET_ONLY','findings':[], 'scope':'Complete new167row inventory/165selected+2excluded,12contexts14joins25groups,16own+1SDK closures,27typedquote rows and30necessary null type facts. Not independent acceptance of reviewer-authored old93 component.', 'whole_row_coverage':row_coverage,'contexts_reviewed':ctx['contexts'],'critical_joins_reviewed':ctx['critical_joins'],'materialization_groups_reviewed':ctx['materialization_groups'],'typed_quote_recomputation':qcoverage,'necessary_null_types':sorted(needed),'minimum_remaining_HOLD':{'own93_source':'independent completeness review of surrounding named scopes and actual selected SDK/materialization/context owner unions; reviewer authored old93 and cannot supply its independent acceptance','types':'30 exact types require sizeof/alignof and actual source/ABI correspondence; no repeated lambda surrogate, field-count or capture-count representation','profile_native':'exact selected compiler/header/configuration and binary/native runtime/libm/fenv/dispatch associations retain separate gates; this source review performs no qualification','controlled_heap':'native vector/String backings, path/FILE/copied-message/static owners and source-requested bytes/lifetimes remain complete separate proof; no allocator/free/physical-frame credit','joint':'controller plus actual active/quarantined child joins under unchanged32MiB, actual process/read settlement, combined U/limit/binding remain unclosed'},'nonclaims':{'old93_independently_accepted':False,'new_numeric_OTHER':False,'whole_child_or_joint_fit':False,'new_layout_measurement':False,'prices_Data_binding_GO':False,'physical_stack_spills_shadow_RSS':False,'source_or_ADR_or_cap_changes':False},'execution':{'own_new_file_text_hash_JSON_integer_checker_only':True,'old_or_candidate_model_checker_executed':False,'target_parser_compiler_helper_native_API_or_Clock':False,'GUI_devices_Git':False},'checks':checks}
    save('review.json',review);(OUT/'review.md').write_text(REVIEW_TEXT,encoding='utf-8',newline='\n')
    save('own-check-receipt.json',{'raw_exit':0,'attempt':1+len(list(OUT.glob('failed-own-attempt-*'))),'check_count':len(checks),'wall_seconds_before_seal':time.perf_counter()-started,'only_own_source_data_checker':True,'preserved_failures':['Own checker wrongly required excluded insert-growth L_INSERT audit type in active symbols; corrected to consult excluded map only for inactive rows. No subject finding or admission of excluded type.','Own lambda scan wrongly required parentheses; actual valid `[&]{...}` also omits parameter-list. Corrected capture-following lexical alternatives only. Exact primary/recorded lines were already identical.']})
    save('source-pins.json',{'schema':'INDEPENDENT_NATIVE_SDK_CONTEXT_R2_SOURCE_REVIEW_PINS','subject_pins':pin(SUB/'source-pins.json'),'subject_freeze':pin(SUB/'first-green-freeze.json'),'checked_existing_inputs_and_subject_own_files':checked,'own_files':[pin(p) for p in sorted(OUT.rglob('*')) if p.is_file() and p.name not in ['source-pins.json','first-green-freeze.json']],'inherited_old93_not_independently_accepted':True,'new_measurement_numeric_OTHER_fit_Data_GO':False})
    save('first-green-freeze.json',{'schema':'INDEPENDENT_NATIVE_SDK_R2_FIRST_COMPLETE_OWN_GREEN','checks':len(checks),'postgreen_edit_authorized':False,'all_new_sources_helpers_oracles_outputs_frozen':True,'files':[pin(p) for p in sorted(OUT.rglob('*')) if p.is_file() and p.name!='first-green-freeze.json'],'disposition':review['disposition']})
    print(json.dumps({'raw_exit':0,'checks':len(checks),'disposition':review['disposition'],'review':pin(OUT/'review.json'),'pins':pin(OUT/'source-pins.json'),'freeze':pin(OUT/'first-green-freeze.json')}))

REVIEW_TEXT='''# Whole native SDK/context R2 independent review

**Conditional ACCEPT of the new source-incidence ledger and proposed minimal missing-type set only.** This is one fresh whole disposition on all167 inventory identities (165 selected runtime or explicit conservative alternatives; two excluded insert-growth identities), all12 contexts/14 critical joins/25 materialization groups, all16 own closure expressions and the selected append closure, all27 typed quote rows, and all30 necessary null type definitions/correspondences. It is not independent acceptance of the older93 named-scope component, which this reviewer authored. That component remains inherited HOLD; its4810-byte declaration sum is not a simultaneous bound. Actual194 layout cells/405128 large-component scope is unchanged.

The complete row assessment used the exact pinned MSVC14.44/x64/C++20/debug0/default-allocator source. Every selected signature, by-value parameter, reference alias, local/materialization, overload, runtime/compile-time branch, child edge, helper return and failure-lifetime rule was read against its primary excerpt. Raw hashes remain byte exact; only navigation excerpts normalize line endings. The own checker independently verifies all61 input rows/24 subject-owned files and the frozen inventory, exact primary excerpts/anchors, typed links and full graph structures. Candidate/old author, models/checkers and all target/parser/compiler/native/API/program execution are excluded. No huge ancestor archive copies are made; existing input paths and hashes remain references.

Vector list/copy construction retains backing/header/default or returned allocator objects, compressed tags, fake allocator/proxy and actual aggregate Tidy_guard. Fresh3/4-row range/fill assignments really start after Time's empty reset; selected raw POD memmove and positive-zero memset do not import fallback backout guards. Address-taken _Zero is a real double. Moves retain three nullptr_t source temporaries, three pointer-return materializations and Vector_val's three pointer formals, with one exchange local per sibling body. Member headers/base/union subobjects remain inside their containing layout. References do not copy referents, and neither debug0 nor an empty source body implies zero standalone size.

String C-string/pointer-count construction, all actual lvalue/rvalue concat overloads, append, move/take, capacity/growth, allocator/default-new/deallocation, traits, placement-new and iterator bodies remain distinct. There is no selected iterator-constructor Tidy_deallocate_guard. The sole const-pointer+rvalue insert is r+number(R), with actual five-digit rates44100/48000/96000 and SSO15: length6 selects fast insert. Its offset/count/pointer and fast locals remain; S_GROW_INSERT/L_INSERT_CALL, child edges, closure and L_INSERT type are excluded. Actual append growth is reached by a clock metadata suffix, so its one exact Fn and3pointer+2size operator formals remain. Allocation and callback are sequential phases. The641-character bound and old capacity527 give at most790 capacity/791 request; vector request is at most96, below4096. This excludes that manual-alignment source branch without guessing default alignment16, allocator overhead or free credit.

Classic Tick upper_bound has real iterator copies/predicate/raw pointer/difference and next/advance/unwrap/seek/constructor objects; no ranges/_Pass_fn is imported. Time650-653's subtraction sibling can exist before search and survives that full expression, with its distinct result and actual3pointer constructor/adopt chain. QC102-104 and Time754-756 store search results before later subtraction. Prefix UB3 has two outer and two inner iterator formals plus one less value object; its named result initializes one actual destination, because return-parameter is not NRVO. The later QC170-171/Time812 conditional has condition-begin and selected-else-begin temporaries with named upper, all post-search. Delayed parameter teardown is retained as an implementation-permitted alternative, rather than silently equating source lifetime with frame reuse.

Witness array equality retains equal3/equal4 pointer formals and one equal_to value object even when the actual uint64 memcmp branch bypasses its operator. Arrays referenced by equality are existing owners. Pointer unwrapping/distance/min/max/array/pair/initializer-list/move/forward/address/limits/bit_cast/math wrappers retain actual typed parameters, reference formation and selected locals. Pair forwarding can materialize real int/double sources; named lvalues do not create cloned referents. Word/XOR bit_cast sources add one8-byte reference-bound source object where actually prvalues; named operands do not. Compile-time selectors/traits/unevaluated operands supply no invented runtime objects, while an address-used constant object remains represented.

All27 quote source rows have actual typed expression trees. The independent traversal verifies unique activations and formal owner identities, typed declaration unions and every permitted subset of previously initialized sibling parameters at each nested initializer. Required signatures use Family/size_t, U64, bool, int, pointer and Witness6, with selected widths8/8/1/4/8/48 qualified only in their existing scope. The extra CPP65 times(count,price(id)), outer/nested CPP287 plus and reservation/workLimit partial activations are included. CPP288 can initialize reason-pointer8 and the direct Witness6 formal48 before its verify-only bool initializer's nested quote, so its partial component is56. The bool destination is not yet formed during its own initializer. This is no generic count-times8 estimate. CPP285 initializers and CPP287 statements are sequential; selected conditional arms are alternatives. Fully entered own93 body parameters and any allowed retained parameter lifetimes must still be united by distinct owner identity in the final independently reviewed surrounding context. The table expressly does not add all formal unions wholesale or claim a complete peak.

All12 contexts separate construction/search/naming/I/O/handler phases. Common controls end before FractionCache/get, and invalid-native frac1 refuses before Time search, so no fabricated action+comparator coexistence. FractionCache nextBit and independentFraction are sequential. Fixture construction precedes get; dense paths retain get plus the selected comparator, with whole/fractional searches sequential. Metadata retains get and real String/Text/Stream result/reference lifetimes; q remains an object after finish. Short/trailing actions use lvalue+C-string concat, not append growth. Prefix/ramp computations finish before their Expect actions. Truncated-record's handler can retain action while get lives, giving2 own closure objects; append-failure Fn is a separate alternative. The context maxima are independent alternatives, not one invented get+Expect+growth trace.

Exception default/message/copy/destructor/what and actual runtime_error/logic_error pointer overloads remain distinct. Rejection's default base creates no message InitData. Message constructors retain both pointer layers, one actual __std_exception_data, and fixed literal-plus-NUL controlled copy requests, separate from imported CRT/EH bookkeeping. Original and new wrong-control/cleanup Rejection payloads may overlap; reference catches do not clone them. No allocator/refcount/unwind/general runtime proof follows. The message inventory is request incidence, not a count of every message actually reached.

The proposed minimal new type set is30:16 exact own closure occurrences, one exact selected append closure, TempoMapView, nullptr_t, less<void>, equal_to<void>, allocator<qc::Change>, allocator<char>, Fake_allocator, Fake_proxy_ptr_impl, the actual vector<qc::Change> Tidy_guard, three reached tags and __std_exception_data. PRED is only an alias to an existing exact predicate identity; the unreachable insert type is not requested. Definitions/specializations/captures/occurrence IDs are source linked. All sizeof/alignof values remain null. A similar repeated lambda is a different type and cannot supply measurement. Fields, empty-looking structs and reference-capture counts supply no numeric layout claim.

The minimum remaining leaves are concrete: independently review own93 completeness and surrounding context/owner unions; qualify these30 exact sizeof/alignof facts and actual source/ABI identities; preserve exact compiler/header/profile and binary/native association gates; finish controlled native vector/String/path/FILE/message/static backing requests/lifetimes; then compose controller plus actual active/quarantined child phases under unchanged32MiB. Reference representation storage, if separately claimed, needs its own premise. General CRT/libm/allocator/EH/TLS bookkeeping retains the existing disclosed boundary, not zero or a new broad exemption. OTHER/whole-child/joint fit, source prices, combined U/limit, Data/binding and execution remain unbound. No physical stack/spill/shadow/RSS certificate,32768 padding, loop-count live-object multiplier, reclaim or process-settlement assumption is supplied.

First complete own green freezes every new review/helper/oracle/output. Any failed own source/data attempt is preserved with exact source and diagnostics; no frozen subject is repaired or executed by this review. Root retains acceptance and all later execution authority.
'''

if __name__=='__main__':
    OUT.mkdir(exist_ok=True)
    try:run()
    except Exception as e:
        i=1
        while (OUT/f'failed-own-attempt-{i:02d}').exists():i+=1
        d=OUT/f'failed-own-attempt-{i:02d}';d.mkdir();(d/'check.py').write_bytes(Path(__file__).read_bytes())
        (d/'failure.json').write_text(json.dumps({'exception':type(e).__name__,'message':str(e),'checks':checks,'wall_seconds':time.perf_counter()-started,'subject_execution':False},indent=2)+'\n',encoding='utf-8',newline='\n')
        print(json.dumps({'raw_exit':1,'checks':len(checks),'failure':str(e),'preserved':str(d)}));sys.exit(1)
