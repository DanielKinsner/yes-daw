"""Own file/hash/JSON/integer gate. Never imports/runs subject/peer helpers.
First complete passing gate freezes this scoped HOLD packet, not a resource proof.
"""
from pathlib import Path
import hashlib,json,sys,traceback
P=Path(__file__).resolve().parent;R=P.parents[1]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def j(p):return json.loads(p.read_text(encoding='utf-8'))
def record(p):
    b=p.read_bytes();return dict(path=p.relative_to(R).as_posix() if p.is_relative_to(R) else p.as_posix(),bytes=len(b),sha256=sha(b))
checks=[]
def ck(n,v):
    checks.append(dict(name=n,pass_=bool(v)))
    if not v:raise ValueError(n)
def write(p,o):p.write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8',newline='\n')
if (P/'first-green-freeze.json').exists():raise SystemExit('FROZEN_DO_NOT_RERUN')
try:
    e=j(P/'envelope.json');n=j(P/'own-named-objects.json');c=j(P/'closure-contexts.json')
    inputs=[]
    def pin(path,expected):
        p=R/path;row=record(p);ck('input_'+path,row['sha256']==expected);inputs.append(row);return p
    for r in e['source_inputs']:pin(r['path'],r['sha256'])
    pinned={
      'build-ci/20261105-clock-oracle-separate-research-contract/contract.md':'344408DAEC2BD355D44B0B6DA266BF00119B09DF5028B70524699E8E2CD36010',
      'build-ci/20261106-clock-native-child-source-owners-r1/owners.json':'73B9A2C4F0BACC420EA86E87F1BB7EC46098E9CB5806DD2D4B53EC97C514FDBE',
      'build-ci/20261106-clock-native-resource-boundary-audit-r1/audit.json':'C76A0FF1289E7B9EC094A7AF77D95372BA90736028640133FE6313F40F42DF1C',
      'build-ci/20261106-native-object-layout-purpose-source-r1/source-pins.json':'7B8EF452D04B97D38B92D1E790FA61B355660A141FA4CB1D21A1002C4B3EA165',
      'build-ci/20261106-native-layout-purpose-independent-review-r1/review.json':'EC89EF36D2C4085AC8EAEF5242428E2D2D6E8A20FA496EC23060961067E0D237',
      e['layout_values']['path']:e['layout_values']['sha256']}
    for p,s in pinned.items():pin(p,s)
    lp=R/'build-ci/20261106-native-object-layout-purpose-source-r1'
    for p in [lp/'execution-r1/receipt.json',lp/'profile.json',lp/'correspondence.json']:
        inputs.append(record(p))
    receipt=j(lp/'execution-r1/receipt.json');values=j(R/e['layout_values']['path']);v={x['name']:x['value'] for x in values['rows']}
    ck('actual_compile_only_result',receipt['compiler_raw_exit']==0 and receipt['no_link'] and receipt['no_Clock_execution'] and receipt['include_count']==111)
    ck('actual_result_correspondence',receipt['layout_values_sha256']==e['layout_values']['sha256'] and receipt['object_sha256']==e['layout_values']['object_sha256']==values['object_sha256'])
    ck('actual_cells',values['cells']==len(values['rows'])==194)
    def allpins(x):
        if isinstance(x,dict):
            if {'path','sha256'}<=x.keys():yield x
            for a in x.values():yield from allpins(a)
        elif isinstance(x,list):
            for a in x:yield from allpins(a)
    pp=list(allpins(j(lp/'source-pins.json')))
    sdk=Path('C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/include')
    for name in ['vector','xmemory','xutility','xstring','array','utility','exception','stdexcept','bit','limits','type_traits','initializer_list','algorithm']:
        path=sdk/name;rr=record(path);matches=[x for x in pp if x['path'].replace('\\','/').casefold()==path.as_posix().casefold()]
        ck('selected_header_'+name,bool(matches) and all(x['sha256']==rr['sha256'] for x in matches));inputs.append(rr)
    sources={r['alias']:(R/r['path']).read_text(encoding='utf-8').splitlines() for r in e['source_inputs']}
    ck('93_unique_named_scopes',len(n['frames'])==len({f['name'] for f in n['frames']})==e['named_ledger']['scopes']==93)
    total=0;unknown=[]
    for f in n['frames']:
        ck('span_'+f['name'],1<=f['source_lines'][0]<=f['source_lines'][1]<=len(sources[f['source']]))
        subtotal=0
        for r in f['objects']:
            ck('names_'+f['name']+'_'+r['type'],r['count']==len(r['names']) and len(r['names'])==len(set(r['names'])))
            expected=v.get(r['type']+'.sizeof')
            ck('size_'+f['name']+'_'+r['type'],r['qualified_unit_size']==expected and r['bytes']==(None if expected is None else expected*r['count']))
            if expected is None:unknown.append((f['name'],r['type']))
            else:subtotal+=r['bytes']
        ck('subtotal_'+f['name'],subtotal==f['named_qualified_object_union']);total+=subtotal
    ck('only_view_named_unqualified',unknown==[('engine::CompiledTempoMap::build','TempoMapView_UNMEASURED')])
    ck('named_sum_not_an_upper',total==e['named_ledger']['qualified_declared_object_width_sum']==4810 and e['named_ledger']['sum_is_not_a_coexistence_upper'])
    ck('main_run_components',e['named_ledger']['main_partial']==186 and e['named_ledger']['run_partial']==1419)
    ck('16_exact_closures',len(c['closures'])==len({r['id'] for r in c['closures']})==e['closures']['contexts']==16)
    for r in c['closures']:
        ck('closure_source_'+r['id'],sources[r['source']][r['line']-1]==r['source_line'])
        ck('closure_unqualified_'+r['id'],r['sizeof'] is None and r['alignof'] is None and all(x['mode']=='reference' for x in r['captures']))
    ck('closure_source_counts',{k:sum(r['source']==k for r in c['closures']) for k in ['CPP','QC','TIME']}==c['source_counts']==dict(CPP=12,QC=2,TIME=2))
    ck('large_measured_component',e['large_component']==n['large_component'] and e['large_component']['bytes']==2*v['Stream.sizeof']+4*v['Text.sizeof']+v['FractionCache.sizeof']==405128)
    for key,count in [('source_materializations',24),('call_contexts',12),('sdk_objects',6),('remaining_obligations',6)]:
        ck('finite_'+key,len(e[key])==len({r['id'] for r in e[key]})==count)
    xu=(sdk/'xutility').read_text(encoding='utf-8').splitlines()
    classic='\n'.join(xu[7461:7487])
    ck('classic_predicate_not_ranges','if (_Pred(_Val, *_UMid))' in classic and '_Pass_fn' not in classic and 'return _STD upper_bound(_First, _Last, _Val, less<>{});' in classic)
    ck('source_map_correction','for(int map=0;map<9;++map)' in sources['CPP'][246])
    ck('source_result_correction','const double result=wholeFrame+extra' in sources['CPP'][199])
    ck('view_exact_definition','const TempoChange* changes = nullptr;' in sources['TIME'][351] and 'std::size_t        count = 0;' in sources['TIME'][352])
    ck('no_false_total',all(e[k] is None for k in ['OTHER_source_upper','whole_child_upper','joint32MiB_fit']) and e['existing_32768_credit']==0)
    ck('static_assert_comparison_only',e['static_assert_comparison']['value']==8*(65584+65544)+11784+32768==1093576)
    ck('no_execution_or_binding',e['execution']==dict(our_compile_link_decoder_launcher_target_runs=0,source_or_cap_changes=0,Data_definitions=0,execution_flags_changed=0,own_data_authoring_only=True))
    ck('guard_still_false','sourceReviewAccepted=false, sourceWorkAccepted=false, executionAccepted=false' in sources['CPP'][21])
    argument=(P/'argument.md').read_text(encoding='utf-8')
    ck('argument_exact_partial_components','1,419 bytes' in argument and '186 bytes' in argument and '405,128 bytes' in argument and '**OTHER remains unqualified.**' in argument)
    for p in sorted(P.iterdir()):
        if p.is_file():ck('LF_'+p.name,b'\r' not in p.read_bytes())
    write(P/'own-checks.json',dict(verdict='PASS_SCOPED_PROPOSAL_CONSISTENCY_ONLY',checks=checks,count=len(checks),not_complete_resource_acceptance=True,no_subject_execution=True))
    files=[record(p) for p in sorted(P.iterdir()) if p.is_file() and p.name not in ['source-pins.json','first-green-freeze.json']]
    pins=dict(schema='NATIVE_CONTROLLED_OBJECT_ENVELOPE_SOURCE_PINS_R1',verdict=e['verdict'],files=files,inputs=inputs,OTHER_source_upper=None,whole_child_upper=None,joint32MiB_fit=None,helpers_are_file_data_only=True)
    write(P/'source-pins.json',pins)
    write(P/'first-green-freeze.json',dict(first_complete_own_pass=True,verdict=e['verdict'],checks=len(checks),all_packet_source_helper_oracle_bytes_frozen=True,files=files+[record(P/'source-pins.json')],no_target_or_compiler_authority=True))
    print(json.dumps(dict(status='PASS_SCOPED_HOLD_PACKET_FROZEN',checks=len(checks),large=405128,OTHER=None,pins=record(P/'source-pins.json')['sha256'],envelope=record(P/'envelope.json')['sha256'],argument=record(P/'argument.md')['sha256'])))
except Exception:
    failed=P/'failed-own-check-r1';failed.mkdir(exist_ok=False)
    for f in P.iterdir():
        if f.is_file():(failed/f.name).write_bytes(f.read_bytes())
    write(failed/'failure.json',dict(checks=checks,error=traceback.format_exc(),no_target_execution=True))
    print('OWN_FILE_DATA_CHECK_FAIL',file=sys.stderr);raise
