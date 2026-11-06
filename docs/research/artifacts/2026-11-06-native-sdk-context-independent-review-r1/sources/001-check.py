"""Fresh independent file/text/JSON/integer critic. No subject import/execute.
All candidate excerpts are compared to actual pinned source. Older own93 data
is only an inherited HOLD input, never independently accepted by this script.
"""
from pathlib import Path
import hashlib,json,re,sys,traceback
P=Path(__file__).resolve().parent;R=P.parents[1]
S=R/'build-ci/20261106-native-sdk-context-ledger-r1'
checks=[];inputs=[]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def load(p):return json.loads(p.read_bytes())
def write(p,x):p.write_text(json.dumps(x,indent=2)+'\n',encoding='utf-8',newline='\n')
def record(p):
    raw=p.read_bytes();return dict(path=p.relative_to(R).as_posix() if p.is_relative_to(R) else p.as_posix(),bytes=len(raw),sha256=sha(raw))
def ck(n,v):
    checks.append(dict(name=n,pass_=bool(v)))
    if not v:raise ValueError(n)
def pin(p,h):
    z=record(p);ck('pin_'+z['path'],z['sha256']==h);inputs.append(z)
if (P/'first-green-freeze.json').exists():raise SystemExit('FROZEN_DO_NOT_RERUN')
try:
    pin(S/'source-pins.json','814915EDB72EFD1049EF15D543C6647F1AD648A54214B1B80D6803695B0DD6FF')
    pin(S/'first-green-freeze.json','74717481007876FB89E4206DB4D2829AD0CE3FC0115ACBAD1772F70D320BEB43')
    pins=load(S/'source-pins.json');freeze=load(S/'first-green-freeze.json')
    for kind,rows in [('inputs',pins['inputs']),('whole_subject',freeze['files'])]:
        for x in rows:
            path=Path(x['path']);path=path if path.is_absolute() else R/path
            z=record(path);ck(kind+'_'+x['path'],z['bytes']==x['bytes'] and z['sha256']==x['sha256'])
    sdk=load(S/'selected-sdk-ledger.json');context=load(S/'context-joins.json');closure=load(S/'closure-incidence.json');facts=load(S/'required-type-facts.json');quotes=load(S/'quote-argument-incidence.json');messages=load(S/'message-copy-requests.json');ingresses=load(S/'written-sdk-ingresses.json')
    rows={x['id']:x for x in sdk['rows']};types=sdk['type_symbols'];source_names={x['source'] for x in sdk['rows']}
    sources={}
    for x in pins['inputs']:
        pp=Path(x['path']);pp=pp if pp.is_absolute() else R/pp
        alias=x.get('alias',pp.name)
        if alias in source_names or alias in ['CPP','QC','TIME','BOOK']:sources[alias]=pp.read_text(encoding='utf-8-sig').splitlines(keepends=True)
    ck('all167_source_rows',len(rows)==len(sdk['rows'])==167)
    ck('all_row_sources_resolved',source_names<=set(sources))
    coverage=[]
    for row in sdk['rows']:
        key=row['id'];a,b=row['lines'];snippet=''.join(sources[row['source']][a-1:b])
        ck(key+'_range',1<=a<=b<=len(sources[row['source']]))
        ck(key+'_actual_excerpt',snippet==row['source_excerpt'])
        ck(key+'_normalized_excerpt_hash',sha(snippet.encode())==row['source_excerpt_sha256'])
        ck(key+'_declaration_anchor',row['declaration_anchor'] in snippet)
        ck(key+'_child_ids',all(z in rows for z in row['children']))
        for role in ['value_parameters','locals','materializations']:
            ck(key+'_'+role+'_unique',len({z['name'] for z in row[role]})==len(row[role]))
            for z in row[role]:
                ck(key+'_'+role+'_'+z['name'],z['type'] in types and isinstance(z['count'],int) and z['count']>0 and (types[z['type']]['size'] is None or types[z['type']]['size']>0))
        ck(key+'_references_are_alias_descriptions',isinstance(row['reference_bindings'],list))
        coverage.append(dict(id=key,source=row['source'],lines=row['lines'],source_excerpt_sha256=row['source_excerpt_sha256'],value_formals=len(row['value_parameters']),locals=len(row['locals']),materializations=len(row['materializations']),status='WHOLE_SOURCE_CONTEXT_REVIEW_NOT_INDIVIDUAL_SIZE_ACCEPTANCE'))
    depths_memo={}
    def depth(k,stack=()):
        if k in depths_memo:return depths_memo[k]
        ck('acyclic_'+','.join(stack+(k,)),k not in stack)
        value=1+max([depth(child,stack+(k,)) for child in rows[k]['children']] or [0])
        depths_memo[k]=value
        return value
    depths={k:depth(k) for k in rows};ck('bounded_source_chains',max(depths.values())<30)
    j={x['id']:x for x in context['critical_joins']}
    ck('12_contexts',len(context['contexts'])==len({x['id'] for x in context['contexts']})==12)
    ck('14_joins',len(j)==len(context['critical_joins'])==14 and sdk['joins']==context['critical_joins'])
    ck('25_materializations',len(context['materialization_groups'])==len({x['id'] for x in context['materialization_groups']})==25 and context['materialization_groups'][-1]['id']=='bit_cast_prvalue_sources')
    for x in context['contexts']:
        ck('context_links_'+x['id'],all(k in j for k in x['sdk_joins']) and x['ABI_or_size_upper'] is None)
    for x in j.values():ck('join_source_nodes_'+x['id'],all(k in rows for k in x['chain']))
    ck('16own_2candidateSDK_closures',closure['own_count']==len(closure['own_closures'])==16 and closure['SDK_count']==len(closure['new_sdk_closures'])==2)
    for x in closure['own_closures']:
        ck('own_closure_source_'+x['id'],sources[x['source']][x['line']-1].rstrip('\n')==x['source_line'])
        ck('own_closure_unknown_'+x['id'],x['sizeof'] is None and x['alignof'] is None and x['source_incidence']['additional_copies']==0)
    for x in closure['new_sdk_closures']:
        ck('sdk_closure_unknown_'+x['id'],x['sizeof'] is None and x['alignof'] is None and not x['captures'] and x['object_instances']==1 and x['additional_copy_instances']==0)
    needed=[x for x in facts['facts'] if x['needed_new_fact']]
    ck('31candidate_type_facts_null',len(needed)==31 and all(x['sizeof'] is None for x in needed))
    pred=next(x for x in facts['facts'] if x['id']=='PRED');ck('PRED_alias_no_extra_fact',pred['type_alias_only'] and not pred['needed_new_fact'])
    ck('eight_existing_symbol_widths',{k:types[k]['size'] for k in ['P','Z','DIFF','D','B','CH','IL','ITER']}==dict(P=8,Z=8,DIFF=8,D=8,B=1,CH=1,IL=16,ITER=8))
    for x in quotes['rows']:ck('quote_source_'+str(x['line']),sources['CPP'][x['line']-1].rstrip('\n')==x['source_line'])
    ck('19candidate_quote_rows',len(quotes['rows'])==19)
    for x in messages['messages']:
        ck('message_literal_'+str(x['source'])+'_'+str(x['line']),('"'+x['literal']+'"') in sources[x['source']][x['line']-1] and x['requested_copy_bytes']==len(x['literal'].encode())+1)
    ck('message_payload_bound_only',max(x['requested_copy_bytes'] for x in messages['messages'])==messages['maximum_literal_plus_NUL']==39 and messages['automatic_InitData_instances']==1)
    for n,x in enumerate(ingresses['rows']):ck('written_ingress_'+str(n),sources[x['source']][x['line']-1].rstrip('\n')==x['source_line'])
    ck('classic_upper_direct','_Pred(_Val, *_UMid)' in rows['UB4']['source_excerpt'] and '_Pass_fn' not in rows['UB4']['source_excerpt'])
    ck('vector_empty_assign_paths','_Newsize > _Oldcapacity' in rows['V_ASSIGN_FILL']['source_excerpt'] and '_Newsize > _Oldcapacity' in rows['V_ASSIGN_COUNTED']['source_excerpt'])
    ck('real_guard_and_zero',any(x['type']=='TG' for x in rows['V_CONSTRUCT']['locals']) and any(x['name']=='_Zero' and x['type']=='D' for x in rows['BITS_ZERO']['locals']))
    ck('move_nullable_and_forwarding_destinations',sum(x['type']=='NUL' for x in rows['V_MOVE']['materializations'])==3 and sum(x['type']=='P' for x in rows['V_MOVE']['materializations'])==3 and len(rows['V_VAL_CTOR']['value_parameters'])==3)
    ck('growth_append_value_formals',len(rows['L_APPEND_CALL']['value_parameters'])==5 and any(x['type']=='L_APPEND' for x in rows['S_GROW_APPEND']['value_parameters']))
    ck('equality_predicate_still_materializes',any(x['type']=='EQ' for x in rows['EQUAL4']['value_parameters']) and '_CSTD memcmp' in rows['EQUAL4']['source_excerpt'])
    ck('exception_base_distinction',not rows['EXCEPTION_DEFAULT']['locals'] and any(x['type']=='EXDATA' for x in rows['EXCEPTION_MESSAGE']['locals']))
    capacity=max(641|15,527+527//2);ck('bounded_small_allocations',capacity==790 and capacity+1==791 and 4*24==96 and 791<4096)
    # Concrete new-source findings. No target constructor/lambda is invoked.
    cpp=''.join(sources['CPP']);time=''.join(sources['TIME']);qc=''.join(sources['QC'])
    ck('N1_fixed_number_ingress','const auto stem="r"+number(R)+"-map"+number(std::uint64_t(map))' in cpp and 'R:{44100ull,48000ull,96000ull}' in cpp)
    rates=[44100,48000,96000];lengths=[dict(rate=r,number_units=len(str(r)),inserted_units=1,after_insert=len(str(r))+1,SSO_capacity=15) for r in rates]
    ck('N1_all_inserts_fit',all(x['number_units']==5 and x['after_insert']==6 and x['inserted_units']<=15-x['number_units'] for x in lengths))
    ck('N1_actual_STL_nongrowing_gate','_Count <= _Mypair._Myval2._Myres - _Old_size && !_STD is_constant_evaluated()' in rows['S_INSERT_COUNT']['source_excerpt'] and '_Right.insert(0, _Left)' in rows['S_PLUS_PR']['source_excerpt'])
    ck('N1_candidate_calls_it_needed',any(x['id']=='L_INSERT' for x in needed) and j['string_grow_insert']['maximum_instances']['L_INSERT']==1)
    ck('N2_Time_shared_expression',bool(re.search(r'std::upper_bound\s*\(\s*changes_\.begin\(\),\s*changes_\.end\(\),\s*tick,.*?\)\s*- changes_\.begin\(\)',time,re.S)))
    ck('N2_misplaced_sibling','sibling_begin_iterator' not in j['upper_tick4']['maximum_instances'] and j['upper_prefix3']['maximum_instances']['sibling_begin_iterator']==1)
    ck('N2_prefix_separate_statements','const auto upper=std::upper_bound(prefix_.begin(),prefix_.end(),frameIn);' in qc and 'const auto upper = std::upper_bound (startFrame_.begin(), startFrame_.end(), frame);' in time)
    quote_lines={x['line'] for x in quotes['rows']}
    ck('N3_inline_units_omitted','return times(count,price(id));' in sources['CPP'][64] and 65 not in quote_lines)
    ck('N3_main_nested_plus_omitted','Cost::plus(runner,Cost::plus(producerUpper,verifierUpper))' in sources['CPP'][286] and 287 not in quote_lines)
    ck('N3_old_QARG_names_main',287 in next(x for x in context['materialization_groups'] if x['id']=='quote_argument_partial_lifetimes')['lines'])
    ck('no_new_upper_or_authority',sdk['numeric_OTHER_upper'] is None and sdk['no_32768_credit'] and not sdk['execution'] and not sdk['independently_accepted'])
    ck('old_named_component_not_self_certified','inherited HOLD' in (P/'review.md').read_text(encoding='utf-8'))
    findings=[dict(id='N1',kind='REACHABILITY_AND_MINIMUM_TYPE_FACT',source=['CPP251','xstring1738-1775','xstring3232-3236'],problem='Insert-growth closure cannot be reached by fixed5-digit number(R)+one-character r prefix; it is falsely required as an actual/minimal fact.',new_type_measurement_needed_to_repair=False),dict(id='N2',kind='SIBLING_ITERATOR_CONTEXT_OWNER',source=['Time650-653','QC169-171','Time811-812'],problem='Tick4 join misses sibling begin temporary from one shared subtraction expression; prefix3 extra cannot sponsor that different context.',already_qualified_width=8,new_type_measurement_needed_to_repair=False),dict(id='N3',kind='INCOMPLETE_NESTED_QUOTE_INCIDENCE',source=['CPP65','CPP287'],problem='Nested times(count,price(id)) and main outer/inner plus omitted from quote incidence despite required partial argument lifetimes.',new_type_measurement_needed_to_repair=False)]
    for f in P.iterdir():
        if f.is_file():ck('LF_'+f.name,b'\r' not in f.read_bytes())
    for path in sorted(S.rglob('*')):
        if path.is_file() and path.name!='first-green-freeze.json':inputs.append(record(path))
    result=dict(verdict='REVISE',scope='INDEPENDENT_NEW_SDK_CONTEXT_SOURCE_ONLY',candidate_pins='814915EDB72EFD1049EF15D543C6647F1AD648A54214B1B80D6803695B0DD6FF',candidate_freeze='74717481007876FB89E4206DB4D2829AD0CE3FC0115ACBAD1772F70D320BEB43',checks=checks,check_count=len(checks),source_row_coverage=coverage,contexts=context['contexts'],critical_join_ids=list(j),materialization_group_ids=[x['id'] for x in context['materialization_groups']],candidate_closure_count=dict(own=16,SDK=2),candidate_needed_type_count=31,corrected_necessary_type_candidate_count=30,insert_capacity_evidence=lengths,source_graph_max_depth=max(depths.values()),findings=findings,
      remaining_HOLD=dict(old93_named_component='INHERITED_UNREVIEWED_NOT_SELF_CERTIFIED',new_concrete_layout_values='ALL_UNQUALIFIED',source_context_completion='N1_N2_N3_THEN_FRESH_WHOLE_REVIEW',native_compiler_header_binary_association='UNQUALIFIED',controlled_heap_path_FILE_message_static='SEPARATE_HOLD',joint_controller_child_quarantine='UNQUALIFIED',OTHER=None,whole_native_child=None,joint32MiB_fit=None,Data=False,prices=False,GO=False),
      limits=dict(no_target_or_candidate_helper_or_compiler_execution=True,no_new_layout_or_measurement_authority=True,no_cap_or_source_edit=True,no_physical_frame_RSS_theorem=True,general_runtime_boundary_unchanged=True,actual194_and405128_scope_preserved=True),argument='review.md')
    write(P/'review.json',result)
    authored=[record(f) for f in sorted(P.rglob('*')) if f.is_file() and f.name not in ['source-pins.json','first-green-freeze.json']]
    write(P/'source-pins.json',dict(verdict='REVISE',files=authored,inputs=inputs,old93_independent_acceptance=False,source_only=True,execution_authority=False))
    write(P/'first-green-freeze.json',dict(first_complete_own_pass=True,verdict='REVISE',checks=len(checks),files=authored+[record(P/'source-pins.json')],whole_review_helpers_oracles_frozen=True,no_subject_execution=True))
    print(json.dumps(dict(status='REVISE_SDK_CONTEXT_WHOLE_REVIEW_SEALED',checks=len(checks),source_rows=167,contexts=12,joins=14,findings=3,review=record(P/'review.json')['sha256'],pins=record(P/'source-pins.json')['sha256'])))
except Exception:
    n=1
    while (P/('failed-own-check-r'+str(n))).exists():n+=1
    d=P/('failed-own-check-r'+str(n));d.mkdir()
    for f in P.iterdir():
        if f.is_file():(d/f.name).write_bytes(f.read_bytes())
    write(d/'failure.json',dict(checks=checks,error=traceback.format_exc(),subject_execution=False))
    print('OWN_SOURCE_DATA_REVIEW_CHECK_FAILED',file=sys.stderr);raise
