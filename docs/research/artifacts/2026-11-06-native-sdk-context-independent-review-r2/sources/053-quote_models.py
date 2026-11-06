"""Handwritten typed full-expression ownership trees, not a C++ parser.

Every node is an actual written selected call, checked against frozen source.
Partial parameter destinations have unique activation identities. Actual entered
body parameters remain a separate own93 requirement, not a second copy here.
"""
from itertools import combinations
PARAMS={
 'price':[('id','FAMILY')], 'plus':[('a','U64'),('b','U64')],
 'times':[('count','U64'),('value','U64')], 'units':[('id','FAMILY'),('count','U64')],
 'wordUnits':[('verify','B'),('words','U64')],
 'inverseUnits':[('verify','B'),('records','U64')],
 'fixtureUnits':[('verify','B'),('N','U64'),('g','U64'),('map','I32')],
 'childUpper':[('verify','B')], 'workLimit':[],
 'reservationGate':[('total','U64'),('limit','U64')],
 'gate3':[('ok','B'),('reason','P'),('witness','WITNESS6')],
 'Meter.add':[('n','U64')]
}
SIZES={'FAMILY':8,'U64':8,'B':1,'I32':4,'P':8,'WITNESS6':48}
BASES={'FAMILY':'Family enum underlying std::size_t; selected size_t.sizeof8',
 'U64':'selected u64.sizeof8','B':'selected bool.sizeof1','I32':'selected int.sizeof4',
 'P':'selected pointer.sizeof8','WITNESS6':'selected Witness6.sizeof48'}
def V(label):return {'value':label}
def C(method,*args):
    assert method in PARAMS and len(args)==len(PARAMS[method]),(method,args)
    return {'call':method,'arguments':list(args)}
def B(call):return {'builtin_expression_with_selected_call':call,'result_type':'B'}

def trees():
    # Statements/declarators and selected ?: branches are distinct full expressions.
    return {
      65:[C('times',V('count'),C('price',V('id')))],
      70:[C('units',V('verify-selected dispatch Family'),V('words'))],
      71:[C('units',V('verify-selected fast Family'),V('words-fallback'))],
      72:[C('units',V('verify-selected fallback Family'),V('fallback'))],
      73:[C('plus',V('dispatch'),C('plus',V('fast'),V('slow')))],
      77:[C('plus',C('price',V('ProductionInverseNative')),C('price',V('ProductionForwardInverse')))],
      78:[C('plus',C('price',V('QcInverseNative')),C('price',V('QcForwardNative')))],
      79:[C('price',V('verify-selected inverse outer Family'))],
      80:[C('plus',C('times',V('records'),C('plus',V('api'),V('outer'))),C('wordUnits',V('verify'),V('2*records')))],
      85:[C('price',V('verify/ramp-selected fractional Family'))],
      87:[C('price',V('verify/ramp-selected zero Family'))],
      88:[C('price',V('RationalCut'))],
      89:[C('times',V('queries-zeros'),C('plus',V('cut'),C('times',V('literal2'),V('fractional'))))],
      90:[C('times',V('zeros'),C('plus',V('cut'),C('times',V('literal2'),V('zero'))))],
      91:[C('plus',C('units',V('DenseOuter'),V('N+1')),C('wordUnits',V('verify'),V('N+1')))],
      92:[C('plus',C('plus',V('fractionalQueries'),V('zeroQueries')),V('records'))],
      95:[C('price',V('verify-selected child-fixed Family'))],
      96:[C('plus',V('total'),C('units',V('PortCacheRefill'),V('27*153611')))],
      99:[C('plus',V('total'),C('fixtureUnits',V('verify'),V('10*R'),V('g'),V('map')))],
      101:[C('plus',V('total'),C('inverseUnits',V('verify'),V('inverseUpper')))],
      187:[C('Meter.add',C('price',V('PortCacheRefill')))],
      245:[C('Meter.add',C('price',V('verify-selected child-fixed Family')))],
      250:[C('Meter.add',C('fixtureUnits',V('verify'),V('N'),V('fractions.g'),V('map')))],
      261:[C('Meter.add',C('inverseUnits',V('verify'),V('std::uint64_t(b.D)')))],
      285:[C('childUpper',V('false')),C('childUpper',V('true'))],
      287:[C('plus',V('runner'),C('plus',V('producerUpper'),V('verifierUpper'))),
           C('reservationGate',V('composed'),C('workLimit'))],
      288:[C('gate3',B(C('plus',V('runner'),V('producerUpper'))),V('DISJOINT_CHILD_RESERVATION literal'),V('direct Witness6 aggregate formal'))]
    }

def calls(node,path='root'):
    if 'builtin_expression_with_selected_call' in node:
        return calls(node['builtin_expression_with_selected_call'],path+'/builtin')
    if 'call' not in node:return []
    out=[dict(activation=path,method=node['call'],parameters=[dict(name=n,type=t,owner=path+'/param'+str(i)) for i,(n,t) in enumerate(PARAMS[node['call']])],
        normal_body_owner='own93::'+node['call'],normal_body_scope_status='INHERITED_HOLD_NOT_INDEPENDENTLY_ACCEPTED')]
    for i,a in enumerate(node['arguments']):out+=calls(a,path+'/arg'+str(i))
    return out

def has_call(node):return 'call' in node or 'builtin_expression_with_selected_call' in node
def partial_options(node,path='root',outer=()):
    # Only one argument initializer executes at a time; other already initialized
    # parameters persist while that initializer calls into its own selected body.
    if 'builtin_expression_with_selected_call' in node:
        return partial_options(node['builtin_expression_with_selected_call'],path+'/builtin',outer)
    if 'call' not in node:return []
    spec=PARAMS[node['call']];out=[]
    for i,arg in enumerate(node['arguments']):
        if not has_call(arg):continue
        siblings=[j for j in range(len(spec)) if j!=i]
        for k in range(len(siblings)+1):
            for subset in combinations(siblings,k):
                initialized=tuple(dict(owner=path+'/param'+str(j),method=node['call'],parameter=spec[j][0],type=spec[j][1],
                  entered_body=False,initialization='completed before selected other argument begins') for j in subset)
                live=outer+initialized
                out.append(dict(while_argument=i,call_activation=path,partial_owners=list(live),
                    selected_inner_calls=[r['method'] for r in calls(arg,path+'/arg'+str(i))],
                    type_counts={t:sum(o['type']==t for o in live) for t in SIZES if any(o['type']==t for o in live)},
                    selected_representation_bytes=sum(SIZES[o['type']] for o in live)))
                out+=partial_options(arg,path+'/arg'+str(i),live)
    return out

def derive(source_lines):
    out=[]
    for line,expressions in trees().items():
        exprs=[]
        for i,e in enumerate(expressions):
            owner='CPP'+str(line)+'/full-expression'+str(i)
            options=partial_options(e,owner)
            activations=calls(e,owner)
            actual_formals=[p for a in activations for p in a['parameters']]
            exprs.append(dict(owner=owner,tree=e,actual_call_activations=activations,partial_lifetime_alternatives=options,
              maximum_partial_parameter_component=max([0]+[o['selected_representation_bytes'] for o in options]),
              all_actual_formal_owners=actual_formals,
              formal_declaration_union_by_type={t:sum(p['type']==t for p in actual_formals) for t in SIZES if any(p['type']==t for p in actual_formals)},
              formal_declaration_union_selected_bytes=sum(SIZES[p['type']] for p in actual_formals),
              formal_union_scope='Finite source expression owner union, not an exact simultaneous peak and not added wholesale to own93. Use owner identities once when a full entered body, initialized partial activation or allowed delayed-teardown parameter is selected.',
              teardown_rule='Parameters may persist to the enclosing full-expression where permitted; returned/formal teardown alternatives must remain source/profile-qualified in the whole context. No physical frame/slot reuse or heap-free claim.',
              result_rule='Actual scalar/class result destination initializes the caller/formal once. Arithmetic prvalues do not gain an invented separately stored object.'))
        out.append(dict(line=line,source_line=source_lines[line-1],full_expressions=exprs,
          expression_alternatives='CPP77/78 are mutually exclusive selected ?: arms; CPP285 initializers and CPP287 statements are sequential; CPP288 quote is verify-only.',
          whole_context_QARG='Maximum over actual expression/entered-body/retained-parameter phases. This table supplies unique partial owners, not a second copy of normal entered own93 parameters.'))
    return dict(schema='R2_TYPED_QUOTE_AND_GUARD_FULL_EXPRESSION_INCIDENCE',rows=out,formal_types=PARAMS,
      selected_representation=SIZES,representation_basis=BASES,full_whole_upper=None,
      own93_source_context_independently_accepted=False,reference_referent_copy=False,new_type_measurement=False,
      inventory_scope='Every written Cost quote caller in CPP54-103 and all qualified Cost quote ingresses in the entire CPP; analogous witness/reservation argument parents explicitly included.')
