"""Exact source identities for the remaining conditional type/layout facts.

This authors no probe or repeated surrogate lambda and infers no sizeof/alignof.
"""
import hashlib
def derive(facts,closures,primary,aliases):
    loc={
      'NUL':('cstddef',27,31,'std::nullptr_t = decltype(nullptr); keep actual type identity, no pointer-size substitution'),
      'LESS':('type_traits',2384,2394,'std::less<void> actual value formal; operator operands references'),
      'EQ':('xutility',693,703,'std::equal_to<void> actual value formal even when memcmp bypasses operator'),
      'FA':('xmemory',1181,1181,'Standalone std::_Fake_allocator; separate from contained actual allocator'),
      'FP':('xmemory',1446,1454,'Standalone std::_Fake_proxy_ptr_impl; selected ctor takes existing container reference'),
      'TG':('xmemory',83,91,'Actual std::_Tidy_guard<std::vector<qc::Change>> aggregate with _Ty* _Target; aggregate field stays in whole guard'),
      'TAG_ZERO':('xmemory',1513,1515,'std::_Zero_then_variadic_args_t actual by-value compressed-pair parameter'),
      'TAG_ONE':('xmemory',1517,1519,'std::_One_then_variadic_args_t actual by-value compressed-pair parameter'),
      'TAG_CONCAT':('xstring',525,528,'std::_String_constructor_concat_tag actual by-value concat parameter'),
      'ALLOC_CHANGE':('xmemory',931,1016,'std::allocator<qc::Change> actual default/returned standalone temporary'),
      'ALLOC_CHAR':('xmemory',931,1016,'std::allocator<char> actual returned standalone temporary'),
      'L_APPEND':('xstring',1524,1529,'Exact closure expression inside selected basic_string<char> append specialization; Fn directly initialized'),
      'EXDATA':('vcruntime_exception.h',27,31,'Actual __std_exception_data _InitData; pointer/bool fields do not infer padding or aggregate size'),
      'TempoMapView':('TIME',350,356,'Actual by-value TempoMapView formal; changes pointer/count fields stay inside that object')}
    own={x['id']:x for x in closures['own_closures']}
    out=[]
    for fact in facts['facts']:
        if not fact.get('needed_new_fact'):continue
        ident=fact['id']
        if ident.startswith('OWN_CLOSURE_'):
            c=own[ident[len('OWN_CLOSURE_'):]];alias=c['source'];a=b=c['line']
            description='Exact actual lambda expression occurrence '+str(c['occurrence_on_line'])+'; captures '+str(c['captures'])
            fields=c['captures']
        else:
            alias,a,b,description=loc[ident];fields=None
        text=''.join(primary[alias].splitlines(keepends=True)[a-1:b])
        out.append(dict(id=ident,source=alias,path=aliases[alias]['path'],lines=[a,b],source_excerpt=text,
          source_excerpt_sha256=hashlib.sha256(text.encode()).hexdigest().upper(),primary_pin=aliases[alias],
          actual_type_identity=description,capture_source_fields=fields,sizeof=None,alignof=None,
          repeated_lambda_is_same_type=False,measurement_performed=False,
          source_layout_association='CONDITIONAL_MISSING_FACT_NOT_A_LAYOUT_PROBE_OR_CLASS_SIZE_PROOF',
          rule='Actual containing layout owns its members once. Standalone objects remain nonzero/unqualified, not empty-base zero. Reference capture representation/padding/ABI is not inferred from capture count.'))
    assert len(out)==30
    return dict(schema='R2_MINIMAL_CONDITIONAL_CLASS_LAYOUT_SOURCE_CORRESPONDENCE',facts=out,
      selected_required_count=30,excluded_insert_growth='L_INSERT never constructed by fixed admitted ingress; no necessary measurement request',
      new_purpose_or_execution_authorized=False,whole_upper=None)
