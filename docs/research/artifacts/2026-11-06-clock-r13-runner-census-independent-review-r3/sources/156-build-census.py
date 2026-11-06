"""Complete own R2 source/data/integer checker and one-time proposal builder.

Only our newly authored Python integer ledgers execute. Subject PowerShell/C++,
prior author/review generators and external main helper are never executed.
"""
from pathlib import Path
from collections import Counter
import hashlib,json,re,runpy,traceback

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SUBJECT=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
MAIN=ROOT/'build-ci/20261106-r13-main-caller-ledger-r3'
if (HERE/'source-pins.json').exists():raise ValueError('Preserve first complete source seal; author a successor instead')
CHECKS=[]
INPUTS={}
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def relative(p):
    # The accepted input union explicitly includes fixed absolute SDK files.
    # Preserve that original identity; only workspace files get relative keys.
    try:return str(p.relative_to(ROOT)).replace('\\','/')
    except ValueError:return str(p).replace('\\','/')
def read(p):
    b=p.read_bytes();INPUTS[relative(p)]=dict(path=relative(p),bytes=len(b),sha256=sha(b));return b
def check(label,actual,expected=True):
    ok=actual==expected;CHECKS.append(dict(name=label,passed=ok))
    if not ok:raise ValueError(label+': '+repr(actual)+' != '+repr(expected))
def pinned(p,expected):
    b=read(p);check('pin '+relative(p),sha(b),expected);return b
def dump(p,obj):p.write_text(json.dumps(obj,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')

def build():
    runner=pinned(SUBJECT/'run-clock.ps1','9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788')
    manifest_bytes=pinned(SUBJECT/'source-manifest.json','10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3')
    manifest=json.loads(manifest_bytes);source=runner.decode('utf-8');lines=source.splitlines(keepends=True)
    for row in manifest['sources']:
        b=read(SUBJECT/row['path']);check('own source '+row['path'],(len(b),len(b.splitlines()),sha(b)),(row['bytes'],row['lines'],row['sha256']))
    check('own five bytes',len(manifest_bytes)+sum(r['bytes'] for r in manifest['sources']),674220)
    check('all false guards',any(manifest['guards'].values()),False)
    check('no definitions',manifest['cost_catalogue']['definition_files'],[])
    for row in manifest['inputs']:
        q=Path(row['path']);q=q if q.is_absolute() else ROOT/q
        b=read(q);check('admitted input '+row['path'],(len(b),sha(b)),(row['bytes'],row['sha256']))
    check('input count',len(manifest['inputs']),1220)
    check('input bytes',sum(r['bytes'] for r in manifest['inputs']),460232026)
    check('input chunks',sum((r['bytes']+65535)//65536 for r in manifest['inputs']),7849)

    notes={
     '20261105-clock-oracle-separate-research-contract/contract.md':'344408DAEC2BD355D44B0B6DA266BF00119B09DF5028B70524699E8E2CD36010',
     '20261105-clock-oracle-research-leaf-census-conventions.md':'DE33D91712372C2FE887A02FB137AA9A96C8C3BB93CB0D88BF57AA32DBEAE564',
     '20261105-clock-oracle-research-bulk-leaf-clarification.md':'268597D1F066779CE76DACFDBBC384FC452DC6B9AD319419F52DE597242BB3E8',
     '20261106-research-getter-destination-clarifier-r1.md':'1F47F55CFC659159E12DACB0C3A4612F081E507617EFE7318168D570B1E623B3',
     '20261106-research-nullable-argument-clarifier-r1.md':'F019F6C9481E6359667AE1252BC2639B03756D686E0007C89C5FA8596C9E5842',
     '20261106-research-static-property-clarifier-r1.md':'FA46DBDCD261641BD6830AF14D7814FC51CDE5167478C723701B92FA33D1C337',
     '20261106-research-typed-local-null-clarifier-r1.md':'A225EA324B9B6BCCC9C0821527DC52E2F03EEFBAF547E3BDC3951FDA4A3FE063',
     '20261106-research-powershell-signed-literal-clarifier-r1.md':'EBB5FF491FF3DC9A0B10046E31432B68186A40DAF545F7B28790D5A658E7B0BE',
     '20261106-research-array-cmdlet-boundary-clarifier-r1.md':'0A98D4889BE641CD427AA10DFFA648F0806DAAE7302083FBFA8347454E09E661',
     '20261106-research-callback-destination-clarifier-r1.md':'FD16BE18EA2D6D5EFE060150E0672E16E30FC637F540718B2D1E0C7DE649DAE1',
     '20261106-research-catch-input-clarifier-r1.md':'CA608A6E752DCA0582ED8F2B07FB519FD697EB40ECDA10ABA4C109C8A72DF71D',
     '20261106-research-array-boundary-independent-review-r1/review.json':'B34580517BAFAA2DD5DAE4ED03A28C725D093A1CC9ADB9A8C3A47DFBFE12A550',
     '20261106-research-array-boundary-independent-review-r1/source-pins.json':'F278A1BA3FD6DB0D3E8FB20190F65C2ED833A2604358F74239BA8F2F501C47D5',
     '20261106-clock-r13-runner-numeric-independent-review-r1/review.json':'57EFD3A0F94D4F0378168AB34D87BD69A86267DFA81A4797D722B35312F8E0DA',
     '20261106-clock-r13-runner-census-r1/census.json':'9DAEFFB03433D0A6D215054462A69D67A2DD79B9A1267F747B70C84DA9EC7024'}
    for name,h in notes.items():pinned(ROOT/'build-ci'/name,h)
    pinned(HERE/'public-cmdlet-ports.json','135B3092E3CD24219B8758BE44C2672CA6078F5775033EF5BDB4F3B70DC099FC')
    main_bytes=pinned(MAIN/'caller-ledger.json','6FB853C6244C0C5EEEC68F114471B75953C4A959944F3B44C3D5F61EB7A59BF3')
    main=json.loads(main_bytes)
    main_pins=json.loads(pinned(MAIN/'source-pins.json','B554C2BEAD02A1A3DC0816C1D896CF44448517838787ED88364EEE13798C32DB'))
    for row in main_pins['inputs']:
        b=read(ROOT/row['path']);check('external main input '+row['path'],(len(b),sha(b)),(row['bytes'],row['sha256']))
    check('main total',main['units'],180799)
    check('main partition',(main['bootstrap_registration_units'],main['main_units']),(51125,129674))
    check('all main rows',sum(r['units'] for r in main['rows']),main['units'])
    check('main rows127',len(main['rows']),127)
    check('main helpers excluded',main['own_helper_bodies_included'],False)
    for i,r in enumerate(main['rows']):
        check('main row '+str(i),r['units'],r['multiplicity']*sum(g['units'] for g in r['groups']))
        for j,g in enumerate(r['groups']):check('main group '+str((i,j)),g['units'],sum(g['roles'].values()))

    # Our helper modules only; hashes are frozen with this builder at firstpass.
    bodies=runpy.run_path(str(HERE/'body_ledgers.py'));models=bodies['MODELS']
    prices_module=runpy.run_path(str(HERE/'derive_prices.py'));total=prices_module['total']
    prices=prices_module['derive'](models)
    check('all models74',len(models),74);check('price set',set(prices),set(models))
    for name,m in models.items():
        check('fixed sum '+name,m['fixed'],sum(total(g) for g in m['fixed_groups']))
        for j,g in enumerate(m['fixed_groups']):
            check('fixed schema '+name+'/'+str(j),set(g),{'label','source_lines','terms','units'})
            check('fixed roles '+name+'/'+str(j),all(type(v) is int for v in g['terms'].values()))
            check('fixed arithmetic '+name+'/'+str(j),g['units'],sum(g['terms'].values()))
        for key,values in m['coefficients'].items():check('coefficient integers '+name+'/'+key,all(type(v) is int for v in values.values()))
        for key,v in m['alternatives'].items():
            units=total(v);check('alternative integer '+name+'/'+key,type(units) is int)
    declarations=list(re.finditer(r'^function ([A-Za-z0-9_-]+)',source,re.M))
    check('actual declarations74',len(declarations),74)
    check('actual model identity',{d.group(1) for d in declarations},set(models))
    main_offset=source.index('\ntry{\n # Hash literal manifest')+1
    main_line=source[:main_offset].count('\n')+1
    check('actual main boundary',main_line,929)
    helper_inventory=[]
    owners={}
    for i,d in enumerate(declarations):
        end=declarations[i+1].start() if i+1<len(declarations) else main_offset
        code=source[d.start():end];first=source[:d.start()].count('\n')+1;last=first+code.count('\n')-1
        name=d.group(1);m=models[name]
        helper_inventory.append(dict(name=name,lines=[first,last],bytes=len(code.encode()),sha256=sha(code.encode()),whole_source=code,
          fixed_groups=m['fixed_groups'],coefficients=m['coefficients'],alternatives=m['alternatives'],
          numeric_components=prices[name],separately_composed=m['separately_composed'],note=m['note'],
          authoring_caption_status='DRAFT_CAPTION_IS_NOT_A_NUMERIC_SPONSOR_NUMERIC_COMPONENTS_AND_ROLE_GROUPS_ARE_AUTHORITY',
          authoring_caption=m['formula']))
        for n in range(first,last+1):owners[n]=name
    calls=[]
    pattern=re.compile(r'(?<![A-Za-z0-9_-])('+'|'.join(sorted(map(re.escape,models),key=len,reverse=True))+r')(?![A-Za-z0-9_-])')
    quoted=re.compile(r"'(?:''|[^'])*'|\"(?:`.|[^`\"])*\"")
    for no,line in enumerate(lines,1):
        visible=line.split('#',1)[0].rstrip('\r\n')
        literal_spans=[(q.start(),q.end()) for q in quoted.finditer(visible)]
        for hit in pattern.finditer(visible):
            if visible[:hit.start()].rstrip()=='function':continue
            if any(lo<=hit.start()<hi for lo,hi in literal_spans):continue
            caller=owners.get(no,'MAIN_AND_BOOTSTRAP');callee=hit.group(1)
            if caller=='MAIN_AND_BOOTSTRAP':
                sponsor=[r['label'] for r in main['rows'] if r['lines'][0]<=no<=r['lines'][1]]
                check('main call covered '+str((no,hit.start())),bool(sponsor))
            else:
                model=models[caller]
                sponsor=[g['label'] for g in model['fixed_groups'] if g['source_lines'][0]<=no<=g['source_lines'][1]]
                if not sponsor:sponsor=['OWN_LOOP_OR_ALTERNATIVE:'+k for k in list(model['coefficients'])+list(model['alternatives'])]
                check('helper caller sponsor '+str((no,hit.start())),bool(sponsor))
            calls.append(dict(id='call-'+str(len(calls)+1),caller=caller,callee=callee,line=no,column=hit.start()+1,
              source=line.rstrip('\r\n'),caller_port_sponsors=sponsor,
              callee_entry_parameter_destinations='OWNED_ONCE_BY_CALLEE_SOURCE_MODEL',
              signature_metadata_reread=False,implicit_referent_copy_fee=False))
    check('all written calls347',len(calls),347)
    check('helper calls233',sum(c['caller']!='MAIN_AND_BOOTSTRAP' for c in calls),233)
    main_calls=[c for c in calls if c['caller']=='MAIN_AND_BOOTSTRAP']
    check('main calls114',len(main_calls),114)
    check('main exact occurrence multiset',Counter((c['line'],c['callee'],c['column']) for c in main_calls),Counter((c['line'],c['helper'],c['column']) for c in main['own_written_main_calls']))
    historical=json.loads(read(ROOT/'build-ci/20261106-clock-r13-runner-numeric-independent-review-r1/caller-inventory.json'))
    old_keys={(c['line'],c['callee'],c['column']) for c in historical['occurrences']}
    current_keys={(c['line'],c['callee'],c['column']) for c in calls}
    check('retain all historical330 occurrences',len(old_keys),330)
    check('historical inventory subset',old_keys<=current_keys)
    check('new punctuation-delimited occurrences17',len(current_keys-old_keys),17)
    check('end of line calls preserved',sum(bool(re.search(r'\b'+re.escape(c['callee'])+r'\s*$',c['source'])) for c in calls)>0)
    fields=[]
    for no,line in enumerate(lines,1):
        for h in re.finditer(r"\$taskManifest((?:\['[^']+'\])+)",line):fields.append(dict(line=no,written=h[0],keys=re.findall(r"\['([^']+)'\]",h[1])))
    # Actual bootstrap JSON recursion: values and keys, not a target parser.
    count=dict(values=0,keys=0,max_depth=0,max_string_units=0)
    def walk(v,d=0):
        count['values']+=1;count['max_depth']=max(count['max_depth'],d)
        if isinstance(v,dict):
            count['keys']+=len(v)
            for k,x in v.items():count['max_string_units']=max(count['max_string_units'],len(k.encode('utf-16-le'))//2);walk(x,d+1)
        elif isinstance(v,list):
            for x in v:walk(x,d+1)
        elif isinstance(v,str):count['max_string_units']=max(count['max_string_units'],len(v.encode('utf-16-le'))//2)
        else:check('bootstrap allowed scalar '+str(count['values']),v is None or type(v) in (int,bool))
    walk(manifest);check('bootstrap visits',count['values']+count['keys'],16104)
    check('bootstrap depth',count['max_depth']<=4);check('bootstrap string max',count['max_string_units']<=512)
    check('source known functionality defect','[Type[]]::EmptyTypes' in source)
    schedule=runpy.run_path(str(HERE/'derive_schedules.py'))['derive'](manifest,len(manifest_bytes))
    check('profile namespace',(schedule['facts']['namespace_count'],schedule['facts']['namespace_guard_rows'],schedule['facts']['directory_upper'],schedule['facts']['namespace_files']),(31,1123,720,1045))
    check('capture envelope',(schedule['facts']['ref_chunk_upper'],schedule['facts']['build_chunk_upper']),(2623,1539))
    graphs=runpy.run_path(str(HERE/'derive_graphs.py'))['derive'](len(manifest['inputs']))
    check('source marker graph',(graphs['marker']['keys'],graphs['marker']['key_units']),(6,80))
    check('source bundle graph',(graphs['bundle']['maps'],graphs['bundle']['arrays'],graphs['bundle']['keys'],graphs['bundle']['key_units'],graphs['bundle']['array_elements']),(1294,2,5251,46364,1292))
    check('source receipt graph',(graphs['receipt']['maps'],graphs['receipt']['arrays'],graphs['receipt']['keys'],graphs['receipt']['key_units'],graphs['receipt']['array_elements']),(58,23,300,2877,164))
    check('source failure graph',(graphs['failure']['maps'],graphs['failure']['keys'],graphs['failure']['string_units']),(79,397,42504))
    comp=runpy.run_path(str(HERE/'compose.py'))['derive'](prices,schedule,graphs,main)
    full=comp['phases'][-1]
    check('six phases',len(comp['phases']),6)
    check('same54 families',len({tuple(a['composition']) for a in comp['phases']}),1)
    for a in comp['phases']:
        check('family count '+a['id'],len(a['composition']),54)
        check('whole sum '+a['id'],a['upper'],sum(a['composition'].values()))
        for k,v in a['composition'].items():check('nonnegative integer '+a['id']+'/'+k,type(v) is int and v>=0)
    check('phase monotonic upper',all(a['upper']<=b['upper'] for a,b in zip(comp['phases'],comp['phases'][1:])))
    check('full actual hash bounds',(full['profile']['H'],full['profile']['hash_chunks']),(9946,78926))
    check('full wire hashes',full['profile']['wire_hex'],141)
    check('full Fresh refusals',full['profile']['Fresh_refusals'],4)
    check('full cleanup attempt/storage',(full['profile']['cleanup_helper_attempts'],full['profile']['cleanup_stored_rows']),(42,39))
    check('full measure/encode/doc',(full['profile']['Measure'],full['profile']['Encode'],full['profile']['DocumentWrite']),(11,7,13))
    check('actual ten expected helper bodies',full['expected_control_calls'],10)
    check('expected witness keys37',full['expected_witness_keys'],37)
    check('replacement match schedule',comp['replacement_helpers']['match_bodies'],28*(85+85+73))
    check('generic FileSet only guaranteed subtraction',comp['generic_FileSet_refusal']['replacement_subtraction_only_fixed'],102)
    check('Codec refusal same helper prefix domination',19<=18+1)
    check('bad key same control disjoint work',64*(128-4)+1<=2*56+53*(64+128))
    check('NoLink depth65 own prefix',prices['Assert-NoLink']['upper'],7091)
    check('rawString six source classes',prices['Assert-TaskRawJson']['per_input_upper'],525)
    check('rawBytes six source classes plus compaction',prices['Assert-TaskRawJsonBytes']['per_input_upper'],602)
    check('all checks passed before emission',all(c['passed'] for c in CHECKS))
    result=dict(schema='R13_COMPLETE_SCALAR_CENSUS_R2_SOURCE_PROPOSAL',
      state='UNACCEPTED_COMPLETE_OWN_CHECKED_PROPOSAL_REQUIRES_DIFFERENT_FRESH_WHOLE_CRITIC',
      subject_runner_sha256=sha(runner),subject_manifest_sha256=sha(manifest_bytes),
      scalar_upper_proposal=comp['upper'],phases=comp['phases'],graphs=comp['graphs'],
      helper_count=74,actual_call_occurrences=347,helper_call_occurrences=233,main_call_occurrences=114,
      families=54,main_component=dict(path=relative(MAIN/'caller-ledger.json'),sha256=sha(main_bytes),units=main['units'],status='EXTERNAL_UNACCEPTED_COMPONENT_DATA_ONLY'),
      generic_FileSet_refusal=comp['generic_FileSet_refusal'],reference_controls_upper=comp['reference_controls_upper'],
      replacement_helpers=comp['replacement_helpers'],bootstrap_json_profile=count,
      source_functionality='HOLD_CONFIRMED_TYPE_ARRAY_EMPTYTYPES_STATIC_FIELD_DEFECT_NO_RUNTIME_OBSERVATION',
      convention='344408/DE33/268597 research, literal reads, visible source operations and actual ports; primitive CLR/codec/binder CPU opaque',
      source_prices_accepted=False,combined_U_selected=False,limit_or_Data_binding=False,resource_fit=False,execution_GO=False,no_target_execution=True,
      conditional_premises=['Exact frozen R13 source/provisional input union; not R14 correspondence.',
        'Selected builtin source boundary and pinned caller clarifications; no hidden per-cell materialization CPU, all controlled storage still owed.',
        'Source-sized positive prefix envelopes and actual repeated-call multiplicity; no snapshot uniqueness, immutable-output, quiet-child, process-heap or GC credit.',
        'Different fresh whole numeric critic; final canonical union/profile/environment/native/resources/immutable binding and separate root GO remain pending.'])
    outputs={'census.json':result,'source-inventory.json':dict(helpers=helper_inventory,callers=calls,manifest_fields=fields),
      'body-components.json':dict(prices=prices,port_reconciliation=bodies['PORT_RECONCILIATION']),
      'phase-schedules.json':schedule,'source-graphs.json':comp['graphs'],
      'checks.json':dict(status='FIRST_COMPLETE_OWN_SOURCE_DATA_INTEGER_PASS_NOT_INDEPENDENT_ACCEPTANCE',count=len(CHECKS),passed=sum(c['passed'] for c in CHECKS),checks=CHECKS)}
    for name,value in outputs.items():
        check('output absent '+name,not (HERE/name).exists())
    # Output-absence checks are part of the saved final check record.
    outputs['checks.json']['count']=len(CHECKS);outputs['checks.json']['passed']=len(CHECKS)
    for name,value in outputs.items():dump(HERE/name,value)
    proof=(HERE/'argument.md').read_text(encoding='utf-8')
    md='# Complete R13 source scalar proposal R2\n\n'
    md+='The mechanically derived scalar upper proposal is **'+format(comp['upper'],',')+'**. It is unaccepted pending a different fresh whole critic. R13 functionality is HOLD because the unchanged source selects Type[].EmptyTypes; no runtime failure is claimed. No resource fit, Data, limit, combined U or execution GO follows.\n\n'
    md+='All74 source bodies,347 written own-call occurrences,54 families and six positive phase envelopes are included. The historical330 inventory missed17 punctuation-delimited calls and remains unchanged. Root main R3 is consumed as JSON data, not executed. All previous attempts remain unchanged.\n\n'
    md+='| Phase | Proposed scalar upper |\n|---|---:|\n'
    for a in comp['phases']:md+='|'+a['id']+'|'+format(a['upper'],',')+'|\n'
    md+='\n| Full-prefix family | Scalar upper |\n|---|---:|\n'
    for k,v in full['composition'].items():md+='|'+k+'|'+format(v,',')+'|\n'
    md+='\n'+proof
    (HERE/'census.md').write_text(md,encoding='utf-8',newline='\n')
    own=[]
    for q in sorted(HERE.rglob('*')):
        if q.is_file() and '__pycache__' not in q.parts:
            b=q.read_bytes();own.append(dict(path=relative(q),bytes=len(b),sha256=sha(b)))
    dump(HERE/'source-pins.json',dict(schema='R13_R2_WHOLE_SOURCE_DATA_PIN_MANIFEST',inputs=sorted(INPUTS.values(),key=lambda x:x['path']),own_files=own,
      first_complete_own_pass=True,source_numeric_acceptance=False,target_executed=False))
    freeze=own+[dict(path=relative(HERE/'source-pins.json'),bytes=(HERE/'source-pins.json').stat().st_size,sha256=sha((HERE/'source-pins.json').read_bytes()))]
    for row in freeze:
        b=(ROOT/row['path']).read_bytes()
        if (len(b),sha(b))!=(row['bytes'],row['sha256']):raise ValueError('post-write whole freeze mismatch '+row['path'])
    dump(HERE/'first-green-whole-source-freeze.json',dict(schema='R13_R2_FIRST_COMPLETE_OWN_PASS_WHOLE_FREEZE',files=freeze,
      checks=len(CHECKS),upper_proposal=comp['upper'],independent_numeric_review_pending=True,target_executed=False))
    print(json.dumps(dict(status='FIRST_COMPLETE_OWN_PASS_FROZEN_UNACCEPTED',checks=len(CHECKS),upper=comp['upper'],helpers=74,calls=347,families=54,
      census_sha256=sha((HERE/'census.json').read_bytes()),pins_sha256=sha((HERE/'source-pins.json').read_bytes()),whole_freeze_sha256=sha((HERE/'first-green-whole-source-freeze.json').read_bytes()))))

if __name__=='__main__':
    try:build()
    except Exception:
        # Preserve every unsuccessful own complete-check attempt. Never repair
        # or rerun a sealed first-green packet in place.
        n=1
        while (HERE/('complete-check-attempt-r'+str(n))).exists():n+=1
        dst=HERE/('complete-check-attempt-r'+str(n));dst.mkdir()
        for q in HERE.iterdir():
            if q.is_file() and q.suffix in ('.py','.json','.md'):(dst/q.name).write_bytes(q.read_bytes())
        dump(dst/'failure.json',dict(stage='complete own source/data/integer builder',error=traceback.format_exc(),checks_completed=len(CHECKS),target_execution=False))
        raise
