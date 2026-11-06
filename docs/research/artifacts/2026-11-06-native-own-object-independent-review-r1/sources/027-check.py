"""Independent whole own93 source/file/hash/JSON/integer review checker.

No subject/old author/checker/model, parser, compiler, decoder or native program
is imported or run. Only literal source navigation and ordinary immutable data.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil
import sys
import traceback
import zipfile

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[1]
SUB=ROOT/'build-ci/20261106-native-controlled-object-envelope-r1'
LAYOUT=ROOT/'build-ci/20261106-native-object-layout-purpose-source-r1'
FIRST=OUT/'first-green-freeze.json'
if FIRST.exists():raise SystemExit('Whole first green frozen; no rerun or rewrite.')
ATT=OUT/'attempts'/('attempt-%02d'%(1+len(list((OUT/'attempts').glob('*')))))
ATT.mkdir(parents=True)
checks=[]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):return json.loads(p.read_bytes())
def dump(p,v):p.write_text(json.dumps(v,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
def absolute(n):
    p=Path(n);return p if p.is_absolute() else ROOT/p
def record(p):
    b=p.read_bytes();return {'path':p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(),'bytes':len(b),'sha256':sha(b)}
def ck(n,v,detail=None):
    checks.append({'name':n,'pass':bool(v),'detail':detail})

try:
    pins=read(SUB/'source-pins.json')
    env=read(SUB/'envelope.json')
    own=read(SUB/'own-named-objects.json')
    closures=read(SUB/'closure-contexts.json')
    frozen=read(SUB/'first-green-freeze.json')
    paths={p.resolve() for p in SUB.rglob('*') if p.is_file()}
    paths.update(absolute(x['path']).resolve() for x in pins['inputs'])
    paths.update(p.resolve() for p in OUT.glob('*') if p.is_file())
    paths.update((LAYOUT/n).resolve() for n in ['layout.cpp','layout-types.h','schema.json','execution-r1/layout.obj'])
    for n in ['AGENTS.md','CLAUDE.md','STATUS.md','build-ci/20261106-native-sdk-context-ledger-r2/argument.md']:
        paths.add((ROOT/n).resolve())
    records=[]
    with zipfile.ZipFile(ATT/'complete-inputs.zip','w',zipfile.ZIP_DEFLATED,compresslevel=1) as z:
        for i,p in enumerate(sorted(paths,key=lambda p:str(p).casefold())):
            b=p.read_bytes();member='%04d.raw'%i;z.writestr(member,b)
            records.append({'path':str(p),'member':member,'bytes':len(b),'sha256':sha(b)})
    dump(ATT/'complete-inputs.json',records)
    ck('exact old subject pins',sha((SUB/'source-pins.json').read_bytes())=='F252B5B6EBEAB13FDBE6AE35FB5FA177CA0F01FBEE36433ED3C10AA71A20B4A3')
    ck('exact old subject firstgreen',sha((SUB/'first-green-freeze.json').read_bytes())=='98C23B436B9B256DC28407A64FF10C8453C5C14D8A063F9ABE17275C8216DA06')
    for label,rows in [('packet',pins['files']),('inputs',pins['inputs']),('freeze',frozen['files'])]:
        for r in rows:
            b=absolute(r['path']).read_bytes()
            ck(label+' byte length '+r['path'],len(b)==r['bytes'])
            ck(label+' exact SHA '+r['path'],sha(b)==r['sha256'])
    ck('original777 is only authored consistency',frozen['checks']==777 and pins['helpers_are_file_data_only'])
    values=read(LAYOUT/'execution-r1/layout-values.json')
    receipt=read(LAYOUT/'execution-r1/receipt.json')
    v={x['name']:x['value'] for x in values['rows']}
    ck('all194 distinct actual cells',values['cells']==len(values['rows'])==len(v)==194)
    ck('actual preserved2845-byte object',len((LAYOUT/'execution-r1/layout.obj').read_bytes())==values['object_bytes']==2845 and sha((LAYOUT/'execution-r1/layout.obj').read_bytes())==values['object_sha256']==env['layout_values']['object_sha256']=='FE84226C8D35C527BF5EBE7386887BDD4259D6421E00722A7D05D96E62C34949')
    ck('actual values identity',sha((LAYOUT/'execution-r1/layout-values.json').read_bytes())==env['layout_values']['sha256']==receipt['layout_values_sha256']=='03A663FFA001DEAE6C284517D71B54607081F6017C1AF40A98B30B40945E750D')
    ck('recorded scoped compile evidence no rerun',receipt['compiler_raw_exit']==0 and receipt['no_link'] and receipt['no_Clock_execution'] and receipt['include_count']==111 and receipt['object_sha256']==values['object_sha256'])
    for row in values['rows']:
        ck('actual layout integer cell '+row['name'],isinstance(row['value'],int) and row['value']>=0)
    ck('selected compiler record',v['compiler.MSC_FULL_VER']==194435228 and v['compiler.CRT_PACKING']==8 and v['compiler.ITERATOR_DEBUG_LEVEL']==0 and v['source.familyCount']==76)
    ck('exact instruction pair',(ROOT/'AGENTS.md').read_bytes()==(ROOT/'CLAUDE.md').read_bytes())
    sources={r['alias']:absolute(r['path']).read_bytes().decode('utf-8').splitlines() for r in env['source_inputs']}
    texts={k:'\n'.join(lines) for k,lines in sources.items()}
    for r in env['source_inputs']:
        ck('exact full source '+r['alias'],len(absolute(r['path']).read_bytes())==r['bytes'] and sha(absolute(r['path']).read_bytes())==r['sha256'])
    correspondence=read(LAYOUT/'correspondence.json')
    for replica in correspondence['replicas']:
        definition=replica['original_definition']
        ck('exact original replica definition '+replica['name'],definition in absolute(env['source_inputs'][0]['path']).read_bytes().decode('utf-8') and sha(definition.encode('utf-8'))==replica['original_definition_sha256'])

    expected_names='''Rejection::Rejection|Rejection::what|rememberCloseFailure|cleanupFileClose|cleanupDescriptorClose|reportCleanupClose|gate_no_witness|gate_witness|bits|workLimit|reservationGate|bytesGate_unreached|Cost::catalogue|Cost::price|Cost::validate|Cost::plus|Cost::times|Cost::units|Cost::wordUnits|Cost::inverseUnits|Cost::fixtureUnits|Cost::childUpper|Meter::Meter|Meter::add|Text::add|Text::u|Text::i|Text::boolean|Text::quoted|number|path|Stream::Stream|Stream::~Stream|Stream::flush|Stream::put|Stream::word_write|Stream::exact|Stream::word_read|Stream::finish|writeText|textWitness|textGate|readText|multiply|compareRational|independentFraction|FractionCache::FractionCache|CachedPort::CachedPort|CachedPort::frame|productionRows|buildProduction|productionFrame|highEndpoint|endpointGate|admittedCut|cut_instantiations|wordGate|inverseGate|Picks::Picks|Picks::see|field|metadata|expect_instantiations|controls|run|decimal|boundedArgument|main|qc::require|qc::fixtureMap|qc::scaled|qc::checkFrame|qc::fractionBounds|qc::NumericAClock::append|qc::NumericAClock::segmentFor|qc::NumericAClock::NumericAClock|qc::NumericAClock::frame|qc::NumericAClock::inverse|engine::detail::isFinitePositive|engine::MusicalTime::hasValidFraction|engine::TempoChange::hasValidBpm|engine::SampleRate::isValid|engine::TempoMapView::empty|engine::detail::appendTempoSegmentFrames|engine::detail::scaledTimeRatio|engine::detail::checkedNativeFrame|engine::CompiledTempoMap::build|engine::CompiledTempoMap::frameForTick|engine::CompiledTempoMap::frameForMusicalTime|engine::CompiledTempoMap::musicalTimeForFrame|Catalogue::family|Catalogue::whole|immutableCatalogue'''.split('|')
    ck('whole93 exact expected body inventory',[r['name'] for r in own['frames']]==expected_names and len(set(expected_names))==93)
    # These source aliases identify distinct same-spelling declarations and
    # implicit language-required range objects; no target parser is invoked.
    aliases={'setvbuf_owner':'owner','flush_result':'result','flush_error':'error','close_result':'result','close_error':'error','a_loop_i':'i','b_loop_i':'i','prefix_mutant':'mutant','ramp_mutant':'mutant','prefix_changed':'changed','ramp_changed':'changed','prefix_count':'count','ramp_count':'count','prefix_rows':'rows','ramp_rows':'rows','prefix_changedA':'changedA','prefix_changedB':'changedB','inverse_actual':'actual','inverse_expected':'expected','validation_i':'i','prefix_i':'i'}
    synthetic={'range_backing','range_header','range_begin','range_end','manifest_begin','manifest_end'}
    coverage=[];unknown=[];total=0
    for frame in own['frames']:
        lo,hi=frame['source_lines'];lines=sources[frame['source']]
        ck('complete bounded source span '+frame['name'],1<=lo<=hi<=len(lines))
        span='\n'.join(lines[lo-1:hi])
        subtotal=0;ids=set()
        for group in frame['objects']:
            expected=v.get(group['type']+'.sizeof')
            ck('actual exact width '+frame['name']+'/'+group['type'],group['qualified_unit_size']==expected)
            ck('actual distinct group names '+frame['name']+'/'+group['type'],group['count']==len(group['names'])==len(set(group['names'])))
            for name in group['names']:
                ck('distinct declaration owner '+frame['name']+'/'+name,name not in ids);ids.add(name)
                if name not in synthetic:
                    token=aliases.get(name,name)
                    ck('literal declaration spelling '+frame['name']+'/'+name,re.search(r'\b'+re.escape(token)+r'\b',span) is not None)
            ck('group width multiplication '+frame['name']+'/'+group['type'],group['bytes']==(None if expected is None else expected*len(group['names'])))
            if expected is None:unknown.append((frame['name'],group['type']))
            else:subtotal+=expected*len(group['names'])
        ck('scope declaration union '+frame['name'],subtotal==frame['named_qualified_object_union'])
        ck('scope is not frame/whole bound '+frame['name'],frame['coverage']=='NAMED_OBJECT_COMPONENT_ONLY_NOT_COMPLETE_FRAME_OR_OTHER_ENVELOPE')
        total+=subtotal
        coverage.append({'scope':frame['name'],'source':frame['source'],'source_lines':[lo,hi],'source_excerpt':span,'source_excerpt_sha256':sha(span.encode('utf-8')),'declared_object_groups':frame['objects'],'reference_bindings':frame['reference_bindings'],'containing_or_large_sponsors':frame['large_or_containing_owner_sponsors'],'other_required_context':frame['other_required_context'],'qualified_declaration_width_union':subtotal,'not_coexistence_or_physical_frame':True})
    ck('4810 declaration widths no peak transfer',total==env['named_ledger']['qualified_declared_object_width_sum']==4810 and env['named_ledger']['sum_is_not_a_coexistence_upper'])
    ck('main186 run1419 exact components',next(x['named_qualified_object_union'] for x in own['frames'] if x['name']=='main')==env['named_ledger']['main_partial']==186 and next(x['named_qualified_object_union'] for x in own['frames'] if x['name']=='run')==env['named_ledger']['run_partial']==1419)
    ck('one unknown TempoMapView not guessed16',unknown==[('engine::CompiledTempoMap::build','TempoMapView_UNMEASURED')] and 'TempoMapView.sizeof' not in v)
    ck('view exact unqualified definition','const TempoChange* changes = nullptr;' in texts['TIME'] and 'std::size_t        count = 0;' in texts['TIME'])
    ck('two actual rate list source ingresses',texts['CPP'].count('for(const std::uint64_t R:{44100ull,48000ull,96000ull})')==2)
    for key in ['Cost::childUpper','run']:
        groups={x['type']:x for x in next(f for f in own['frames'] if f['name']==key)['objects']}
        ck('implicit rate backing/list represented '+key,groups['Rates3']['bytes']==24 and groups['RateInitializerList']['bytes']==16 and groups['pointer']['names']==['range_begin','range_end'] and groups['pointer']['bytes']==16)
    ck('lvalue String ranges exact','for(char c:s)' in texts['CPP'] and 'for(char c:manifest)' in texts['CPP'])
    ck('String range iterators match actual const type',v['StringConstIterator.sizeof']==8 and v['String.sizeof']==32)
    ck('large containing widths source-derived',2*v['Stream.sizeof']+4*v['Text.sizeof']+v['FractionCache.sizeof']==env['large_component']['bytes']==own['large_component']['bytes']==405128)
    ck('large component exact counts',env['large_component']['source_counts']==[2,4,1] and env['large_component']['Stream']==65584 and env['large_component']['Text']==65544 and env['large_component']['FractionCache']==11784)
    ck('large arrays contained not separate members',v['StreamBuffer.sizeof']==65536 and v['TextBuffer.sizeof']==65536 and v['FractionRows.sizeof']==11760 and v['Stream.offsetof.buffer']==8 and v['Text.offsetof.data']==0 and v['FractionCache.offsetof.rows']==0)
    ck('q survives finish before metadata/inverse',texts['CPP'].index('q.finish(8*(N+1))')<texts['CPP'].index('const auto text=metadata')<texts['CPP'].index('Stream inv(path(root,binary)'))
    ck('descriptor read helper precedes inv construction','if(verify)readText(path(root,json),descriptor);else writeText(path(root,json),descriptor);Stream inv' in texts['CPP'])
    ck('readText local and actual return destination retained','Text readText(' in texts['CPP'] and 'Text t;Stream f' in texts['CPP'] and 'return t;' in texts['CPP'])
    ck('cached source repair result present','const double result=wholeFrame+extra' in texts['CPP'])
    ck('run source repair map present','for(int map=0;map<9;++map)' in texts['CPP'])
    ck('unreached bytesGate is declaration component only','No current source caller; not counted as reached' in json.dumps(own))
    ck('legacy qc cut not current call','Cut cut(Tick whole' in texts['QC'] and '.cut(' not in texts['CPP'])
    ck('Catalogue has no current static value definition','static const Catalogue reviewed_;' in texts['BOOK'] and 'Catalogue::reviewed_{' not in re.sub(r'//[^\n]*','',texts['BOOK']))
    ck('Time legacy inverse is excluded','tickForFrame' not in texts['CPP'] and 'musicalTimeForFrame(frame,expected)' in texts['CPP'])
    ck('whole16 closure identities',len(closures['closures'])==len({x['id'] for x in closures['closures']})==16)
    for row in closures['closures']:
        ck('exact closure source occurrence '+row['id'],sources[row['source']][row['line']-1]==row['source_line'])
        ck('no invented closure layout '+row['id'],row['sizeof'] is None and row['alignof'] is None)
        ck('all recorded captures reference '+row['id'],all(x['mode']=='reference' for x in row['captures']))
    ck('closure counts12/2/2',{k:sum(x['source']==k for x in closures['closures']) for k in ['CPP','QC','TIME']}==closures['source_counts']=={'CPP':12,'QC':2,'TIME':2})
    for key,expected_count in [('source_materializations',24),('call_contexts',12),('sdk_objects',6),('remaining_obligations',6)]:
        ck('complete old scoped inventory '+key,len(env[key])==len({x['id'] for x in env[key]})==expected_count)
    forward='\n'.join(sources['TIME'][727:789])
    ck('old invalid-native comparator association explicitly excluded',forward.index('if (time.tick < 0 || ! time.hasValidFraction())')<forward.index('return TimeLookupStatus::InvalidInput;')<forward.index('frameForTick (time.tick, frame)') and '{S,independent.second}' in sources['CPP'][241])
    ck('fraction1 actually invalid by source','return frac >= 0.0 && frac < 1.0;' in texts['TIME'] and 'independent.second==1' in sources['CPP'][240])
    ck('current R2 context corrects old sentence','invalid-native action rejects' in (ROOT/'build-ci/20261106-native-sdk-context-ledger-r2/argument.md').read_text(encoding='utf-8'))
    ck('old candidates not whole current join authority',env['OTHER_source_upper'] is None and env['whole_child_upper'] is None and env['joint32MiB_fit'] is None and env['existing_32768_credit']==0)
    ck('static assertion only comparison not residual sponsor',env['static_assert_comparison']['value']==8*(65584+65544)+11784+32768==1093576)
    ck('no target authority changed',env['execution']['our_compile_link_decoder_launcher_target_runs']==0 and env['execution']['Data_definitions']==0 and env['execution']['execution_flags_changed']==0)
    ck('protected INCOMPLETE lineage remains','INCOMPLETE' in own['state'] and own['OTHER_upper'] is None and own['full_auto_upper'] is None)
    dump(OUT/'source-coverage.json',{'scope':'WHOLE_EXACT_93_NAMED_DECLARATION_WIDTH_COMPONENT','bodies':coverage,'unknown_named_types':unknown,'source_inputs':env['source_inputs'],'layout_values':env['layout_values'],'not_current_SDK_context_or_whole_resource_acceptance':True})
except BaseException:
    checks.append({'name':'checker completed without exception','pass':False,'detail':traceback.format_exc()})
    (ATT/'stderr.txt').write_text(traceback.format_exc(),encoding='utf-8',newline='\n')
result={'passed':sum(x['pass'] for x in checks),'failed':sum(not x['pass'] for x in checks),'checks':checks,'source_only':True,'target_execution':False,'no_other_or_frame_or_whole_resource_credit':True}
dump(OUT/'checks.json',result);dump(ATT/'result.json',result)
summary={'passed':result['passed'],'failed':result['failed'],'failed_checks':[x for x in checks if not x['pass']]}
(ATT/'stdout.txt').write_text(json.dumps(summary)+'\n',encoding='utf-8',newline='\n')
if not result['failed']:
    dump(OUT/'source-pins.json',{'subject_pins':'F252B5B6EBEAB13FDBE6AE35FB5FA177CA0F01FBEE36433ED3C10AA71A20B4A3','subject_firstgreen':'98C23B436B9B256DC28407A64FF10C8453C5C14D8A063F9ABE17275C8216DA06','complete_inputs':records,'input_capture':record(ATT/'complete-inputs.zip'),'no_ancestor_archive_copy':True,'subject_execution':False})
    dump(OUT/'review.json',{'status':'COMPLETE','verdict':'ACCEPT_EXACT_93_DECLARATION_WIDTH_AND_LARGE_COMPONENT_ONLY','findings_in_own93':[],'ancillary_not_adopted':[{'id':'H1','source':'closure-contexts.json candidate common-controls expression','claim':'invalid-native may enter Time comparator','reason':'frac1 rejects at Time731 before frameForTick/search; already corrected by current SDK/contextR2. No old-context acceptance follows.'}],'verification':{'checks':result['passed'],'failed':0,'scopes':93,'closure_identities':16,'old_materialization_groups':24,'old_contexts':12,'layout_cells':194,'main_declaration_width':186,'run_declaration_width':1419,'declaration_sum':4810,'large_component':405128},'TempoMapView_layout':None,'OTHER':None,'whole_child':None,'joint_fit':None,'current_SDK_context_acceptance':False,'physical_frame_acceptance':False,'native_binding_price_limit_Data_execution':False,'argument':'review.md'})
for p in OUT.glob('*'):
    if p.is_file():
        dest=ATT/'outputs'/p.name;dest.parent.mkdir(exist_ok=True);shutil.copyfile(p,dest)
if not result['failed']:
    rows=[record(p) for p in sorted(OUT.rglob('*')) if p.is_file()]
    dump(FIRST,{'status':'FIRST_COMPLETE_INDEPENDENT_WHOLE_OWN93_SOURCE_GREEN_FROZEN','checks':result['passed'],'failed':0,'files':rows,'scope':'EXACT_NAMED_DECLARATION_WIDTH_AND_LARGE_OWNER_COMPONENT_ONLY','subject_firstgreen':'98C23B436B9B256DC28407A64FF10C8453C5C14D8A063F9ABE17275C8216DA06','target_execution':False,'OTHER_whole_native_joint_fit_Data_or_GO':False})
print(json.dumps(summary))
raise SystemExit(int(result['failed']>0))
