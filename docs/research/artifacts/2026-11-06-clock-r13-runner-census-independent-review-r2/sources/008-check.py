"""Fresh critic file/text/JSON/integer checker. No subject code is executed.
The scanner masks comments/quoted text for fixed source coordinates; it is not
the PowerShell parser or an AST. Candidate Python is only hashed, never loaded.
"""
from pathlib import Path
import hashlib,json,re,sys,traceback
P=Path(__file__).resolve().parent;R=P.parents[1]
S=R/'build-ci/20261106-clock-r13-runner-census-r2'
T=R/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
checks=[];input_rows=[]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):return json.loads(p.read_bytes())
def write(p,o):p.write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8',newline='\n')
def rec(p):
    b=p.read_bytes();return dict(path=p.relative_to(R).as_posix() if p.is_relative_to(R) else p.as_posix(),bytes=len(b),sha256=sha(b))
def ck(n,v):
    checks.append(dict(name=n,pass_=bool(v)))
    if not v:raise ValueError(n)
def pinned(p,h):
    z=rec(p);ck('pin_'+z['path'],z['sha256']==h);input_rows.append(z);return p
if (P/'first-complete-own-pass-freeze.json').exists():raise SystemExit('FROZEN_DO_NOT_RERUN')
try:
    pinned(S/'census.json','90E6F31DBF49D8D4A5B5DAC4B08AA022BF47EA94112850AEFD228F0B14D069CC')
    pinned(S/'source-pins.json','DFC0F598BF3CECC00484A3772B86819BA2EF71A11078B26B30ED73ECA8BB4B93')
    pinned(S/'first-green-whole-source-freeze.json','F0C4439F557C5E59DBB1096AA0DACFE9DB62EDAD944E68AD52D14B20C4876BB6')
    pinned(T/'run-clock.ps1','9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788')
    pinned(T/'source-manifest.json','10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3')
    M=R/'build-ci/20261106-r13-main-caller-ledger-r3'
    pinned(M/'caller-ledger.json','6FB853C6244C0C5EEEC68F114471B75953C4A959944F3B44C3D5F61EB7A59BF3')
    pinned(M/'source-pins.json','B554C2BEAD02A1A3DC0816C1D896CF44448517838787ED88364EEE13798C32DB')
    candidate_pins=read(S/'source-pins.json')
    pin_counts={}
    for kind in ['inputs','own_files']:
        rows=candidate_pins[kind];pin_counts[kind]=len(rows)
        for row in rows:
            path=Path(row['path']);path=path if path.is_absolute() else R/path
            actual=rec(path);ck(kind+'_identity_'+row['path'],actual['bytes']==row['bytes'] and actual['sha256']==row['sha256'])
    C=read(S/'census.json');I=read(S/'source-inventory.json');B=read(S/'body-components.json');p=B['prices'];m={r['name']:r for r in I['helpers']}
    manifest=read(T/'source-manifest.json');main=read(M/'caller-ledger.json');raw=(T/'run-clock.ps1').read_bytes();text=raw.decode('utf-8');lines=text.splitlines(keepends=True)
    ck('LF_subject',b'\r' not in raw)
    # Fresh lexical masking with stable offsets; no syntax tree or evaluation.
    masked=list(text);state='code';i=0
    while i<len(text):
        ch=text[i];nxt=text[i:i+2]
        if state=='code':
            if nxt=='<#':masked[i]=masked[i+1]=' ';i+=2;state='block';continue
            if ch=='#':masked[i]=' ';state='line'
            elif ch in "'\"":masked[i]=' ';state='single' if ch=="'" else 'double'
            elif ch=='`' and i+1<len(text):masked[i]=' ';masked[i+1]=' ' if text[i+1]!='\n' else '\n';i+=2;continue
        elif state=='line':
            if ch=='\n':state='code'
            else:masked[i]=' '
        elif state=='block':
            if nxt=='#>':masked[i]=masked[i+1]=' ';i+=2;state='code';continue
            if ch!='\n':masked[i]=' '
        elif state=='single':
            if ch=="'":
                if text[i:i+2]=="''":masked[i]=masked[i+1]=' ';i+=2;continue
                state='code'
            if ch!='\n':masked[i]=' '
        elif state=='double':
            if ch=='`' and i+1<len(text):masked[i]=' ';masked[i+1]=' ' if text[i+1]!='\n' else '\n';i+=2;continue
            if ch=='"':state='code'
            if ch!='\n':masked[i]=' '
        i+=1
    ck('mask_ends_outside_literal',state in ('code','line'))
    clean=''.join(masked)
    declarations=list(re.finditer(r'(?m)^function\s+([A-Za-z][A-Za-z0-9-]*)',clean))
    names=[x.group(1) for x in declarations];definition_spans={x.span(1) for x in declarations}
    ck('74_declarations_and_models',len(names)==len(set(names))==len(m)==len(p)==74 and set(names)==set(m)==set(p))
    pattern=re.compile(r'(?<![A-Za-z0-9_$-])(?:'+ '|'.join(sorted(map(re.escape,names),key=len,reverse=True))+r')(?![A-Za-z0-9_-])')
    calls=[]
    for hit in pattern.finditer(clean):
        if hit.span() in definition_spans:continue
        line=clean.count('\n',0,hit.start())+1;column=hit.start()-clean.rfind('\n',0,hit.start())
        caller=next((n for n,r in m.items() if r['lines'][0]<=line<=r['lines'][1]),'MAIN')
        calls.append(dict(caller=caller,callee=hit.group(),line=line,column=column))
    ck('347_actual_calls',len(calls)==347 and sum(z['caller']=='MAIN' for z in calls)==114)
    actual={(z['line'],z['column'],z['callee']) for z in calls};declared={(z['line'],z['column'],z['callee']) for z in I['callers']}
    ck('exact_call_coordinates',actual==declared and len(declared)==347)
    body_coverage=[]
    def groups(x,prefix):
        if isinstance(x,dict):
            if {'terms','units'}<=x.keys():
                ck('integer_roles_'+prefix,all(isinstance(v,int) and not isinstance(v,bool) for v in x['terms'].values()))
                ck('role_sum_'+prefix,sum(x['terms'].values())==x['units'])
            for k,v in x.items():
                if k not in ['whole_source','note','authoring_caption']:groups(v,prefix+'/'+k)
        elif isinstance(x,list):
            for k,v in enumerate(x):groups(v,prefix+'/'+str(k))
    for name,model in m.items():
        whole=''.join(lines[model['lines'][0]-1:model['lines'][1]])
        ck('whole_body_'+name,whole==model['whole_source'] and len(whole.encode())==model['bytes'] and sha(whole.encode())==model['sha256'])
        ck('numeric_component_'+name,model['numeric_components']==p[name]);groups(model,name)
        body_coverage.append(dict(name=name,lines=model['lines'],sha256=model['sha256'],fixed_group_count=len(model['fixed_groups']),coefficient_groups=len(model['coefficients']),alternative_groups=len(model['alternatives']),status='WHOLE_REVIEWED_NOT_INDIVIDUAL_PRICE_ACCEPTANCE'))
    # Re-evaluate all74 final atomic component formulas from role terms. This
    # is separate from matching the two candidate output representations.
    def amount(z):
        if isinstance(z,list):return sum(amount(x) for x in z)
        if 'terms' in z:return sum(z['terms'].values())
        return sum(z.values())
    def fixed(name):return sum(amount(g) for g in m[name]['fixed_groups'])
    def alt(name,key):return amount(m[name]['alternatives'][key])
    def coef(name,key):return amount(m[name]['coefficients'][key])
    dp={name:fixed(name) for name in m if isinstance(p[name],int)}
    dp['Initialize-TaskPathCatalogue']=fixed('Initialize-TaskPathCatalogue')+1220*coef('Initialize-TaskPathCatalogue','input_row')+10*coef('Initialize-TaskPathCatalogue','own_or_stage_row')+119+63*coef('Initialize-TaskPathCatalogue','descriptor_row')
    simple_dicts={
      'Add-TaskPathCatalogueEntry':['repo_input','ordinary_alias'],
      'Get-TaskCatalogueString':['hit','miss'],
      'Get-TaskCatalogueEntryPath':['hit','tilde','first']}
    for name,keys0 in simple_dicts.items():dp[name]={k:fixed(name)+alt(name,k) for k in keys0}
    for name in ['Close-TaskOwner','Close-TaskJsonReader']:
        z={k:fixed(name)+alt(name,k) for k in ['null','normal','firstFailure','secondaryFailure']};z['upper']=max(z.values());dp[name]=z
    dp['Complete-TaskOwnerClose']=dict(normal=fixed('Complete-TaskOwnerClose'),propagate=alt('Complete-TaskOwnerClose','propagate'))
    dp['Assert-TaskCodecBinding']=dict(normal=fixed('Assert-TaskCodecBinding'),refusal=alt('Assert-TaskCodecBinding','default_refusal'))
    for name in ['Assert-TaskReservation','Assert-TaskRetainedReservation','Assert-TaskAttempt','Assert-TaskConsolePE','Assert-TaskPredecessor']:
        dp[name]=dict(normal=fixed(name),refusal=fixed(name)+alt(name,'refusal_surplus'))
    name='Assert-TaskIdentity';dp[name]=dict(normal=fixed(name),refusal=fixed(name)+alt(name,'refusal_surplus'))
    name='Assert-TaskBoundSourceInteger';dp[name]=dict(normal=fixed(name),kind=fixed(name)+alt(name,'kind_surplus'),nonpositive=fixed(name)+alt(name,'nonpositive_surplus'))
    name='Assert-NoLink';normal=fixed(name)+64*coef(name,'parent_body');depth=normal+alt(name,'depth65_prefix')-alt(name,'replaced_normal_ending');dp[name]=dict(normal=normal,depth65=depth,upper=max(normal,depth))
    for name in ['Get-TaskHash','Get-TaskBootstrapHash']:
        z=dict(fixed=fixed(name),per_chunk=coef(name,'read_chunk'))
        if name.endswith('BootstrapHash'):z['per_parent']=coef(name,'parent')
        dp[name]=z
    name='Test-TaskHashValue';dp[name]={k:fixed(name)+alt(name,k)+32*coef(name,k+'_body') for k in ['hex','binary']}
    name='Assert-TaskFreshHashIdentity';dp[name]=dict(normal=fixed(name),refusal_body=alt(name,'refusal'),refusal_surplus=alt(name,'refusal')-1)
    name='Assert-TaskPollAdmission';dp[name]=dict(normal=fixed(name),refusal=alt(name,'refusal'))
    name='Convert-TaskControlOperand';dp[name]={k:fixed(name)+alt(name,k) for k in ['builtin','biginteger']}
    name='Get-TaskIntegerDigits';dp[name]=dict(negative=fixed(name)+18*13,unsigned=alt(name,'unsigned_fixed')+19*12)
    name='Get-TaskSafePrefix';dp[name]=dict(ASCII=fixed(name),general=fixed(name)+alt(name,'general_extra_decrement'))
    name='Write-TaskJson';dp[name]=dict(measure_null=fixed(name),measurement_supplied=fixed(name)-4)
    dict_specs={
      'Invoke-TaskChild':dict(per_argument='argument_row',per_string_unit='string_UTF16',per_poll='normal_poll',completion='completed_direction',positive='positive_direction'),
      'Add-TaskChildCleanup':dict(per_Code_unit='Code_UTF16'),
      'Replace-TaskBounded':dict(per_aggregate_argument_unit='all_argument_UTF16',per_occurrence='IndexOf_body'),
      'Expect-TaskGate':dict(per_expected_key='witness_key'),
      'Initialize-TaskController':dict(per_argument_unit='argument_UTF16'),
      'Assert-TaskString':dict(ASCII='ASCII_body',general='general_surrogate_body'),
      'Get-TaskEscapedStringBytes':dict(per_unit='UTF16'),
      'Join-TaskBoundedNames':dict(per_name='name_row',per_name_unit='name_UTF16'),
      'Get-TaskDependencyString':dict(per_unit='UTF16'),
      'Encode-TaskJson':dict(per_preLF_unit='output_UTF16'),
      'Get-TaskStageBytes':dict(per_row='row'),
      'Assert-TaskDocumentWrite':dict(per_path_unit='Path_UTF16'),
      'Copy-TaskOwned':dict(per_chunk='chunk'),
      'Resolve-TaskInput':dict(per_unit='Name_UTF16'),
      'Assert-TaskPins':dict(per_namespace='namespace_row',per_callback_pair=None,per_own='own4_row',per_input='input_row'),
      'Get-TaskReferences':dict(per_entry='common_entry',per_file='file'),
      'Get-TaskBuildArtifacts':dict(per_entry='file_row'),
      'Get-TaskEntries':dict(per_directory='directory',per_candidate='candidate',per_path_unit='candidate_UTF16',overflow='overflow'),
      'Read-TaskChildResult':dict(per_text_unit='scan_UTF16',per_control='admitted_control'),
      'Assert-TaskDependencies':dict(per_include='include_row',one_string_fixed='extracted_string_fixed',per_string_unit='string_UTF16'),
      'Save-TaskFailureInventory':dict(per_file='file_row')}
    for name,fields in dict_specs.items():
        dp[name]=dict(fixed=fixed(name),**{k:coef(name,v) for k,v in fields.items() if v is not None})
    dp['Add-TaskChildCleanup']['max_Code43']=fixed('Add-TaskChildCleanup')+43*coef('Add-TaskChildCleanup','Code_UTF16')
    dp['Assert-TaskPins']['per_callback_pair']=coef('Assert-TaskPins','Where_callback')+coef('Assert-TaskPins','projection_callback')
    dp['Get-TaskEntries']['replaced_ending']=alt('Get-TaskEntries','replaced_normal_ending')
    dp['Read-TaskChildResult']['bad_ID_prefix']=alt('Read-TaskChildResult','first_bad_ID_prefix')
    name='Invoke-TaskFencedChild';dp[name]=dict(fixed=fixed(name),binary=alt(name,'binary_additional'))
    name='Assert-TaskExactKeys';dp[name]=dict(fixed=fixed(name),per_key=coef(name,'declared_key')+coef(name,'observed_key'),per_actual_key_unit=coef(name,'key_UTF16'))
    name='Assert-TaskSameReferences';dp[name]=dict(fixed=fixed(name),per_file=coef(name,'before_row')+coef(name,'after_row'))
    name='Read-TaskJson';dp[name]={k:fixed(name)+alt(name,k) for k in ['dependency','predecessor']}
    name='Assert-TaskFileSet';dp[name]=dict(fixed=fixed(name),expected=coef(name,'expected_declared_row')+coef(name,'expected_missing_scan'),observed=coef(name,'observed_row'),per_name_unit=coef(name,'string_UTF16'),refusal_fixed=fixed(name)+alt(name,'refusal_surplus'),refusal_expected=coef(name,'expected_declared_row')+coef(name,'expected_missing_scan')+coef(name,'refusal_missing_extra'),refusal_observed=coef(name,'observed_row')+coef(name,'refusal_observed_extra'))
    name='Visit-TaskJsonValue';dp[name]={k:alt(name,k) for k in ['string','boolean','integer','map_fixed','array_fixed']};dp[name].update(null=fixed(name)+alt(name,'null'),string_per_unit=148,map_per_entry=coef(name,'map_entry'),map_per_key_unit=coef(name,'map_key_UTF16'),map_separator=coef(name,'map_separator'),array_per_entry=coef(name,'array_entry'),array_separator=coef(name,'array_separator'))
    for name in ['Assert-TaskRawJson','Assert-TaskRawJsonBytes']:
        classes={k:coef(name,k) for k in ['outer_integer_path','string_body','hex_body','digits_body','magnitude_body','literal_body']};grammar=sum(classes.values());failure=alt(name,'first_failure_tail')
        if name.endswith('Bytes'):
            dp[name]=dict(classes=classes,grammar=grammar,common=fixed(name),failure=failure,compaction=coef(name,'compaction'),compaction_fixed=alt(name,'compaction_fixed'),per_input_upper=grammar+coef(name,'compaction'),fixed_upper=fixed(name)+max(failure,alt(name,'compaction_fixed')))
        else:dp[name]=dict(classes=classes,per_input_upper=grammar,fixed_upper=fixed(name)+max(failure,alt(name,'normal_return')))
    ck('all74_component_formulas',set(dp)==set(p))
    for name in p:ck('independent_component_formula_'+name,dp[name]==p[name])
    for n,row in enumerate(main['rows']):
        for g in row['groups']:ck('main_roles_'+str(n)+'_'+g['label'],sum(g['roles'].values())==g['units'])
        ck('main_row_'+str(n),row['units']==row['multiplicity']*sum(g['units'] for g in row['groups']))
    ck('main_127_rows',len(main['rows'])==127 and sum(r['units'] for r in main['rows'])==180799==main['units'])
    ck('main_split',main['bootstrap_registration_units']==51125 and main['main_units']==129674 and not main['own_helper_bodies_included'])
    # Independent field/size/prefix equations. These do not accept body prices.
    ceil=lambda x:(x+65535)//65536
    ni=len(manifest['inputs']);ns=len(manifest['closed_directory_inputs']);own=len(manifest['sources'])
    eg=sum(x['entry_count']+1 for x in manifest['closed_directory_inputs']);dg=sum(x['entry_count']+2 if x['recursive'] else 1 for x in manifest['closed_directory_inputs']);files=sum(len(x['files']) for x in manifest['closed_directory_inputs'])
    ids=ni+own+1;chunks=sum(ceil(x['bytes']) for x in manifest['inputs']+manifest['sources'])+ceil(len((T/'source-manifest.json').read_bytes()))
    ck('selected_input_geometry',(ni,ns,own,eg,dg,files,ids,chunks)==(1220,31,4,1123,720,1045,1225,7862))
    refs_last=max(x['bytes_cap'] for x in manifest['references']);build_last=max(x['bytes_cap'] for x in manifest['compiler_final_outputs']);cb=sum(ceil(x['bytes_cap']) for x in manifest['control_files'])
    rchunks=ceil(152627224+refs_last)+59;bchunks=ceil(67108864+build_last)+3
    ck('pre_return_hash_overshoot',(refs_last,build_last,rchunks,bchunks)==(15360000,33554432,2623,1539))
    vectors=dict(fences=[1,1,3,5,8,8],children=[0,0,1,2,3,3],stage=[0,7,264,523,782,784],references=[0,0,0,1,2,2],builds=[0,0,1,1,2,2],root_walks=[0,1,1,1,2,2],native_parses=[0,0,0,1,2,2],copies=[0,1,2,2,2,2],markers=[0,1,1,1,1,1],receipts=[0,2,2,3,4,4],failures=[0,1,1,1,1,1],bundle_passes=[0,0,0,0,0,5])
    derived_profiles=[]
    for ix,phase in enumerate(C['phases']):
        a=phase['profile'];q={k:v[ix] for k,v in vectors.items()};made=int(ix>0);dep=int(ix>=2);prod=int(ix>=3);done=int(ix>=4);bundle=int(ix==5)
        f=q['fences'];cc=q['children'];ss=q['stage'];rr=q['references'];bb=q['builds'];rw=q['root_walks'];nn=q['native_parses']
        h0=3*made+4*dep+nn+4*done+5*bundle;ch0=3*ceil(98009)*made+4*ceil(33554432)*dep+nn*ceil(33554432)+cb*done+bundle*(4096+16)
        W=ss+ns*f+rr+bb+rw+1;D=ss+dg*f+61*rr+bb+79*rw+1;E=6*ss+(3 if ss else 0)+eg*f+60*rr+4*bb+78*rw+1
        NL=ids*f+60*rr+4*bb+h0+f*ns+60*rr+78*done+1+D+2*q['copies']
        G=NL+2*W+E+dep;RS=ni*(f+1+bundle)+f*ns+dep*(manifest['profile']['include_input_count']+1)+1+made+dep
        expected=dict(id='P'+str(ix),**q,H=ids*f+60*rr+4*bb+h0,hash_chunks=chunks*f+rchunks*rr+bchunks*bb+ch0,noncapture_hashes=h0,noncapture_chunks=ch0,
          walkers=W,directories=D,normal_rows=E,walker_Canonical=2*W+E,NoLink=NL,Canonical=G,Resolve=RS,EntryPath=G+RS,firstBuild_upper=1297,
          FileSet_slots=f*ns+rr+bb+prod+nn+done,FileSet_success_elements_upper=f*files+56*rr+3*bb+56*prod+(0 if not nn else 3 if nn==1 else 14)+67*done,
          FileSet_generic_refusal_replacements=2,FileSet_generic_expected_max=361,FileSet_generic_observed_max=376,actual_module_controls=0 if not nn else 3 if nn==1 else 14,
          Measure=q['markers']+q['receipts']+q['failures']+q['bundle_passes'],Encode=q['markers']+q['receipts']+q['failures']+bundle,DocumentWrite=q['markers']+2*q['receipts']+2*q['failures']+2*bundle,
          raw_byte_calls=2+dep+3*made,raw_byte_units=len((T/'source-manifest.json').read_bytes())+98009+61*made+4194304*dep,raw_string_calls=0 if not nn else 4 if nn==1 else 16,raw_string_units=nn*65536,
          Fresh_normal=f*(ids-1)+(2+dep+nn if made else 0),Fresh_refusals=1 if not ix else 1+(2 if ix<3 else 3),hex_tests=f*ids+2*made,binary_tests=dep+nn,
          wire_hex=60*rr+4*bb+(2+2*dep+4*done+5*bundle if made else 0),outer_async_read_calls=259*cc,cleanup_helper_attempts=14*cc,cleanup_stored_rows=13*cc,
          ref_capture_hashes=60*rr,build_capture_hashes=4*bb,postcompiler=dep,postproducer=prod,postverify=done,bundle=bundle,created=made)
        for k,val in expected.items():ck('phase_'+str(ix)+'_'+k,a[k]==val)
        ck('no_unexplained_profile_fields_'+str(ix),set(a)==set(expected));derived_profiles.append(expected)
    ck('final_source_hash_schedule',(derived_profiles[-1]['H'],derived_profiles[-1]['hash_chunks'])==(9946,78926))
    ck('outer_read_counterexample',255*4096+2<=1048576 and 2+255+2==259)
    # All54 families: fresh arithmetic from their declared components, not an
    # import/call of compose.py. Atomic p remains the unaccepted candidate input.
    graph=read(S/'source-graphs.json')
    def visit(g):
        z=p['Visit-TaskJsonValue'];return sum([z['map_fixed']*g.get('maps',0),z['array_fixed']*g.get('arrays',0),z['map_per_entry']*g.get('keys',0),z['map_separator']*g.get('map_separators',0),z['map_per_key_unit']*g.get('key_units',0),z['array_per_entry']*g.get('array_elements',0),z['array_separator']*g.get('array_separators',0),z['string']*g.get('strings',0),z['string_per_unit']*g.get('string_units',0),z['integer']*g.get('integers',0),z['boolean']*g.get('booleans',0)])
    def wire(g):return 2*(g.get('maps',0)+g.get('arrays',0))+g.get('keys',0)+g.get('map_separators',0)+g.get('array_separators',0)+g.get('key_units',0)+2*g.get('keys',0)+6*g.get('string_units',0)+2*g.get('strings',0)+20*g.get('integers',0)+5*g.get('booleans',0)+1
    for name,g in graph.items():
        ck('visitor_algebra_'+name,visit(g)==g['visitor_upper']);ck('wire_algebra_'+name,wire(g)-(5*g['string_units'] if name=='marker' else 0)==g['wire_upper_including_LF'])
    jf=p['Join-TaskBoundedNames'];fs=p['Assert-TaskFileSet']
    def join(n,units):return jf['fixed']+jf['per_name']*n+jf['per_name_unit']*units
    def refuse(n,o,missing,unknown):return fs['refusal_fixed']+fs['refusal_expected']*n+fs['refusal_observed']*o+fs['per_name_unit']*512*(n+o)+join(len(missing),sum(map(len,missing)))+join(len(unknown),sum(map(len,unknown)))
    refneg=refuse(56,55,['references/native-inverse/r48000-map5.json'],[])+refuse(56,57,[],['unlisted.bin'])+refuse(56,57,[],[])
    generic=fs['refusal_fixed']+fs['refusal_expected']*361+fs['refusal_observed']*376+fs['per_name_unit']*512*(361+376)+join(361,512*361)+join(376,512*376)
    ck('generic_refusal_equation',generic==307+65713*361+65710*376==48429660)
    repl=p['Replace-TaskBounded'];replacement=84*repl['fixed']+(2*84*512+28*19)*repl['per_aggregate_argument_unit']+28*(512//6+512//6+512//7)*repl['per_occurrence']+84*18
    boot=p['Get-TaskBootstrapHash'];ctrl=p['Initialize-TaskController'];controller=ctrl['fixed']+ctrl['per_argument_unit']*14*512+7*(boot['fixed']+64*boot['per_parent'])+379*boot['per_chunk']
    family_rows=[];totals=[]
    for ix,a in enumerate(derived_profiles):
        f=a['fences'];c=a['children'];r=a['references'];b=a['builds'];made=a['created'];dep=a['postcompiler'];prod=a['postproducer'];done=a['postverify'];bundle=a['bundle'];native=a['native_parses']
        ex=6*made+dep+3*prod;keys=sum([2,3,3,3,2,5])*made+4*dep+15*prod
        cap=dict(marker=graph['marker']['wire_upper_including_LF'],receipt=65536,failure=min(1048576,graph['failure']['wire_upper_including_LF']),bundle=1048576)
        prelf=a['markers']*(cap['marker']-1)+a['receipts']*(cap['receipt']-1)+a['failures']*(cap['failure']-1)+bundle*(cap['bundle']-1)
        q={
        'bootstrapDefinitionsAll74Registrations':51125,'wholeMainCallerAndCatchSourcePositivePrefix':129674,'controllerAndSevenFixedAuthenticatedHashes':controller,
        'freshSHAOwnBodiesExcludingNoLink':p['Get-TaskHash']['fixed']*a['H']+p['Get-TaskHash']['per_chunk']*a['hash_chunks'],
        'allNoLinkOwnBodies':p['Assert-NoLink']['upper']*a['NoLink'],'allResolveOwnBodies':(p['Resolve-TaskInput']['fixed']+512*p['Resolve-TaskInput']['per_unit'])*a['Resolve'],
        'allWalkerOwnBodiesAndOverflowAlternatives':p['Get-TaskEntries']['fixed']*a['walkers']+p['Get-TaskEntries']['per_directory']*a['directories']+(p['Get-TaskEntries']['per_candidate']+512*p['Get-TaskEntries']['per_path_unit'])*a['normal_rows']+max(0,p['Get-TaskEntries']['overflow']+512*p['Get-TaskEntries']['per_path_unit']-p['Get-TaskEntries']['replaced_ending'])*a['walkers'],
        'allStageByteOwnBodies':p['Get-TaskStageBytes']['fixed']*a['stage']+p['Get-TaskStageBytes']['per_row']*(6*a['stage']+(3 if a['stage'] else 0)),
        'allPinsOwnCallerBodies':f*(p['Assert-TaskPins']['fixed']+31*p['Assert-TaskPins']['per_namespace']+1123*p['Assert-TaskPins']['per_callback_pair']+4*p['Assert-TaskPins']['per_own']+1220*p['Assert-TaskPins']['per_input']),
        'normalFileSetSlots':fs['fixed']*a['FileSet_slots']+(fs['expected']+fs['observed']+1024*fs['per_name_unit'])*a['FileSet_success_elements_upper'],
        'twoFileSetRefusalSlotReplacements':2*(generic-fs['fixed']),
        'expectedControlsOwnBodiesAndThreeReferenceRefusals':p['Expect-TaskGate']['fixed']*ex+p['Expect-TaskGate']['per_expected_key']*keys+prod*refneg,
        'childOwnAllPollsAnd259OuterReads':c*p['Invoke-TaskChild']['fixed']+sum([28,6,6][:c])*p['Invoke-TaskChild']['per_argument']+p['Invoke-TaskChild']['per_string_unit']*512*(sum([28,6,6][:c])+8*c),
        'fencedWrapperOwnCallers':c*p['Invoke-TaskFencedChild']['fixed']+native*p['Invoke-TaskFencedChild']['binary'],
        'compilerReplacementHelpers84':int(c>0)*replacement,'twoOwnedCopyBodies':a['copies']*p['Copy-TaskOwned']['fixed']+p['Copy-TaskOwned']['per_chunk']*(0 if not a['copies'] else 2 if a['copies']==1 else 514),
        'ownedMutationBodies':p['Mutate-TaskOwnedBytes']*a['copies'],'PEHeaderBodies':4*dep*p['Get-TaskPEHeader'],
        'ordinaryConsoleChecksAndActualControlRefusal':int(c>0)*p['Assert-TaskConsolePE']['normal']+dep*(2*p['Assert-TaskConsolePE']['normal']+p['Assert-TaskConsolePE']['refusal']),
        'predecessorDescriptor':p['Get-TaskPredecessorDescriptor'],'predecessorMap':p['Assert-TaskPredecessor']['refusal'],'boundSourceSignedIntegers':4*max(p['Assert-TaskBoundSourceInteger'].values()),
        'workReservationNormalAndActualRefusal':p['Assert-TaskReservation']['normal']*(1+made)+p['Assert-TaskReservation']['refusal']*made,
        'retainedReservationNormalAndActualRefusal':p['Assert-TaskRetainedReservation']['normal']*(1+bundle)+p['Assert-TaskRetainedReservation']['refusal']*made,
        'oneUseAttemptNormalAndActualRefusal':p['Assert-TaskAttempt']['normal']+p['Assert-TaskAttempt']['refusal']*made,
        'lastLegalAndOnePastPollBoundary':made*(p['Assert-TaskPollAdmission']['normal']+p['Assert-TaskPollAdmission']['refusal']),
        'rootAsciiValidation':p['Assert-TaskString']['fixed']+512*p['Assert-TaskString']['ASCII'],
        'compilerArgumentAsciiValidation':int(c>0)*28*(p['Assert-TaskString']['fixed']+512*p['Assert-TaskString']['ASCII']),
        'allMeasureOwn':p['Measure-TaskJson']*a['Measure'],
        'allRecursiveVisitors':a['markers']*visit(graph['marker'])+a['receipts']*visit(graph['receipt'])+a['failures']*visit(graph['failure'])+a['bundle_passes']*visit(graph['bundle']),
        'receiptOwnAndSecondaryWithoutMeasure':a['receipts']*(p['Save-TaskReceipt']+p['Get-TaskSecondaryCodes']),
        'WriteJsonOwnWithoutMeasure':p['Write-TaskJson']['measure_null']*a['failures']+p['Write-TaskJson']['measurement_supplied']*bundle,
        'allDocumentWriteOwn':a['DocumentWrite']*(p['Assert-TaskDocumentWrite']['fixed']+512*p['Assert-TaskDocumentWrite']['per_path_unit']),
        'allOnceEncodeOwn':p['Encode-TaskJson']['fixed']*a['Encode']+p['Encode-TaskJson']['per_preLF_unit']*prelf,
        'bootstrapBoundedRead':p['Read-TaskBoundedBytes'],'predecessorAndDependencyReadJsonOwn':p['Read-TaskJson']['predecessor']+dep*p['Read-TaskJson']['dependency'],
        'streamedDependencyReaderAllOwners':dep*p['Read-TaskDependencyJson'],
        'dependencyExtractionAndStringGetters':dep*(p['Assert-TaskDependencies']['fixed']+p['Get-TaskDependencyString']['fixed']+512*p['Get-TaskDependencyString']['per_unit']+1221*(p['Assert-TaskDependencies']['per_include']+p['Get-TaskDependencyString']['fixed']+512*p['Get-TaskDependencyString']['per_unit'])),
        'nativeResultReaderOwnAnd14Controls':native*p['Read-TaskChildResult']['fixed']+p['Read-TaskChildResult']['per_text_unit']*a['raw_string_units']+a['actual_module_controls']*p['Read-TaskChildResult']['per_control'],
        'oneBadControlPrefixReplacement':int(native>0)*p['Read-TaskChildResult']['bad_ID_prefix'],
        'buildArtifactOwnWithoutHashHex':b*(p['Get-TaskBuildArtifacts']['fixed']+4*p['Get-TaskBuildArtifacts']['per_entry']),
        'referenceOwnWithoutHashHex':r*(p['Get-TaskReferences']['fixed']+60*p['Get-TaskReferences']['per_entry']+60*p['Get-TaskReferences']['per_file']),
        'sameReferenceDescriptorIdentities':done*(p['Assert-TaskSameReferences']['fixed']+56*p['Assert-TaskSameReferences']['per_file']),
        'finalBuildDescriptorIdentityBodies':done*3*p['Assert-TaskIdentity']['normal'],
        'twoDescriptorIdentityRefusalSlotReplacements':2*(p['Assert-TaskIdentity']['refusal']-p['Assert-TaskIdentity']['normal']),
        'allFreshIdentityOwnWithoutTests':a['Fresh_normal']*p['Assert-TaskFreshHashIdentity']['normal']+a['Fresh_refusals']*p['Assert-TaskFreshHashIdentity']['refusal_surplus'],
        'allNativeDigestHexAndBinaryTests':a['hex_tests']*p['Test-TaskHashValue']['hex']+a['binary_tests']*p['Test-TaskHashValue']['binary'],'wireHexWrapperBodies':a['wire_hex']*p['Get-TaskHashHex'],
        'failureInventoryOwn':a['failures']*(p['Save-TaskFailureInventory']['fixed']+78*p['Save-TaskFailureInventory']['per_file']),
        'allRawByteGrammarAndCompaction':p['Assert-TaskRawJsonBytes']['per_input_upper']*a['raw_byte_units']+p['Assert-TaskRawJsonBytes']['fixed_upper']*a['raw_byte_calls'],
        'allRawStringGrammars':p['Assert-TaskRawJson']['per_input_upper']*a['raw_string_units']+p['Assert-TaskRawJson']['fixed_upper']*a['raw_string_calls'],
        'allCanonicalOwn':p['Get-TaskCanonicalPath']*a['Canonical'],'allEntryPathOwn':p['Get-TaskCatalogueEntryPath']['tilde']*a['EntryPath']+(p['Get-TaskCatalogueEntryPath']['first']-p['Get-TaskCatalogueEntryPath']['tilde'])*a['firstBuild_upper'],
        'initializeCatalogue':p['Initialize-TaskPathCatalogue']}
        ck('54_family_keys_'+str(ix),len(q)==54 and set(q)==set(C['phases'][ix]['composition']))
        for k,val in q.items():ck('family_'+str(ix)+'_'+k,val==C['phases'][ix]['composition'][k]);family_rows.append(dict(phase=a['id'],family=k,upper=val,status='RECOMPOSED_CANDIDATE_UNACCEPTED_ATOMIC_INPUTS'))
        totals.append(sum(q.values()));ck('phase_total_'+str(ix),sum(q.values())==C['phases'][ix]['upper'])
    ck('all_candidate_totals',totals==[677814798,711840789,3718171545,4177074190,4795962821,5577409900] and max(totals)==C['scalar_upper_proposal'])
    # Independently supported source-role findings; checks certify that the
    # finding matches frozen bytes, not a modified candidate price.
    ck('F1_source','if($taskAt -ge $Text.Length -or $Text[$taskAt] -cne $taskLiteral[$taskLiteralAt])' in lines[560])
    literal=m['Assert-TaskRawJson']['coefficients']['literal_body'];ck('F1_actual_candidate',literal==dict(for_framework=14,At_Length_or_char_compare=18,At_increment=3))
    ck('F1_required_atoms',1+6+1+4+4+1+1+1==19 and 14+19+3==36)
    ep=m['Get-TaskCatalogueEntryPath']['alternatives']
    for label in ['raw tilde guard','canonical tilde guard']:
        g=next(x for x in ep['tilde'] if x['label']==label);ck('F2_'+label,g['units']==12 and 'B' not in g['terms'])
    ck('F2_source',lines[107].lstrip().startswith('if($taskRaw.IndexOf') and lines[109].lstrip().startswith('if($taskCanonical.IndexOf'))
    canon=next(x for x in m['Get-TaskCanonicalPath']['fixed_groups'] if x['label']=='maximal selected return/caller');ck('F3_candidate',canon['terms']==dict(indexed=7,T=1) and canon['units']==8)
    ck('F3_source','return Get-TaskCatalogueEntryPath' in lines[134] and 7+1+1==9)
    failure=next(x for x in m['Save-TaskFailureInventory']['fixed_groups'] if x['label']=='seven-field failure payload/caller')
    ck('F4_candidate_default_ports',failure['terms']['Maximum_default_literal_R']==1 and failure['terms']['Measurement_default_null_R']==1 and not any('Long' in k or 'coerc' in k for k in failure['terms']))
    ck('F4_signature_and_call','[long]$Maximum=1048576' in lines[761] and 'Write-TaskJson (Join-Path' in lines[925])
    primary=R/'build-ci/20261105-controller-environment-writer-closure-r2/primary/src/System.Management.Automation/engine'
    for f in [primary/'ParameterBinderController.cs',primary/'ScriptParameterBinderController.cs',primary/'ScriptParameterBinder.cs',primary/'parser/Compiler.cs']:
        input_rows.append(rec(f))
    compiler=(primary/'parser/Compiler.cs').read_text(encoding='utf-8');binder=(primary/'ParameterBinderController.cs').read_text(encoding='utf-8')
    ck('F4_primary_default_constant_and_coercion','result.Value = constantValue;' in compiler and 'flags |= ParameterBindingFlags.ShouldCoerceType;' in binder and 'object result = spb.GetDefaultScriptParameterValue' in binder)
    corrected=dict(graph['receipt']);corrected['integers']-=3;corrected['booleans']+=3;corrected['string_units']+=11*(len('FIRST_SECONDARY_RAW_RETAINED_NOT_SERIALIZED')-41)+(len('references/native-inverse/r48000-map5.json')-41)-2*(128-42)
    graph_erratum=dict(source_secondary_code_units=43,recipe_secondary_code_units=41,source_missing_descriptor_units=42,recipe_missing_descriptor_units=41,source_child_integrals=11,source_child_booleans=10,recipe_child_integrals=12,recipe_child_booleans=9,corrected_same_domain_receipt=corrected,corrected_visitor=visit(corrected),corrected_wire=wire(corrected),not_a_replacement_price=True)
    ck('graph_literal_source_lengths',len('FIRST_SECONDARY_RAW_RETAINED_NOT_SERIALIZED')==43 and len('references/native-inverse/r48000-map5.json')==42)
    ck('graph_conservative_whole_receipt_preserved',corrected['string_units']==12245 and visit(corrected)<=visit(graph['receipt']) and wire(corrected)<=wire(graph['receipt']))
    findings=[dict(id='F1',helpers=['Assert-TaskRawJson'],runner_lines=[561],candidate='literal_body guard18, class35',required='guard19, class36 or explicit source-coupled whole proof',whole_price_replacement=None),dict(id='F2',helpers=['Get-TaskCatalogueEntryPath'],runner_lines=[108,110],candidate='Each predicate12 has no B; first branch embeds both12s',required='Sponsor each distinct B or provide correct alternative domination',whole_price_replacement=None),dict(id='F3',helpers=['Get-TaskCanonicalPath'],runner_lines=[135],candidate='selected caller/return7+T1=8 omits D',required='Alias return7+D1+T1=9 or correct explicit whole-helper alternative maximum',whole_price_replacement=None),dict(id='F4',helpers=['Save-TaskFailureInventory'],runner_lines=[762,926],candidate='Explicit Int32 default literalR but no long coercionO at helper caller',required='One actual default Int32->Int64 semantic O; preserve formalW once',whole_price_replacement=None)]
    for x in body_coverage:x['findings']=[z['id'] for z in findings if x['name'] in z['helpers']]
    for filename in ['body-components.json','source-inventory.json','phase-schedules.json','source-graphs.json','public-cmdlet-ports.json','argument.md','body_ledgers.py','compose.py','derive_schedules.py','derive_graphs.py','derive_prices.py','build-census.py']:
        input_rows.append(rec(S/filename))
    for f in [R/'build-ci/20261105-clock-oracle-separate-research-contract/contract.md',R/'build-ci/20261105-clock-oracle-research-leaf-census-conventions.md',R/'build-ci/20261105-clock-oracle-research-bulk-leaf-clarification.md']:
        input_rows.append(rec(f))
    for suffix in ['getter-destination','static-property','nullable-argument','typed-local-null','callback-destination','catch-input','powershell-signed-literal','array-cmdlet-boundary']:
        input_rows.append(rec(R/('build-ci/20261106-research-'+suffix+'-clarifier-r1.md')))
    for f in P.iterdir():
        if f.is_file():ck('LF_'+f.name,b'\r' not in f.read_bytes())
    evidence=dict(verdict='REVISE',scope='FRESH_INDEPENDENT_WHOLE_R13_R2_SCALAR_SOURCE_CRITIQUE',checks=checks,check_count=len(checks),body_coverage=body_coverage,own_calls=calls,main_rows=127,main_calls=114,helper_calls=233,phase_profiles=derived_profiles,family_coverage=family_rows,phase_candidate_totals=totals,candidate_proposal=5577409900,findings=findings,graph_correspondence_erratum=graph_erratum,source_pin_rows_checked=pin_counts,
      limits=dict(candidate_arithmetic_is_not_source_acceptance=True,not_proved_total_too_small=True,no_NOFIT_or_lower_claim=True,replacement_total=None,source_functionality='HOLD_Type_array_EmptyTypes',R14_R15_or_new_geometry_transfer=False,resource_fit=False,combined_U=False,Data=False,execution_GO=False,subject_edits=False,target_parser_AST_import_helper_compiler_program_execution=False,subject_or_old_generators_executed=False),failed_own_read_attempts=['failed-read-attempt-r1.json','failed-read-attempt-r2.json'],review_argument='review.md')
    write(P/'review.json',evidence)
    files=[rec(x) for x in sorted(P.iterdir()) if x.is_file() and x.name not in ['source-pins.json','first-complete-own-pass-freeze.json']]
    write(P/'source-pins.json',dict(verdict='REVISE',files=files,inputs=input_rows,candidate_input_pin_manifest_sha256='DFC0F598BF3CECC00484A3772B86819BA2EF71A11078B26B30ED73ECA8BB4B93',all_candidate_manifest_rows_freshly_checked=True,no_execution_authority=True))
    write(P/'first-complete-own-pass-freeze.json',dict(first_complete_own_pass=True,verdict='REVISE',checks=len(checks),files=files+[rec(P/'source-pins.json')],whole_critic_helpers_oracles_frozen=True,no_subject_execution=True))
    print(json.dumps(dict(status='REVISE_WHOLE_REVIEW_SEALED',checks=len(checks),helpers=74,calls=347,families=54,phases=6,findings=4,review=rec(P/'review.json')['sha256'],pins=rec(P/'source-pins.json')['sha256'])))
except Exception:
    n=1
    while (P/('failed-complete-check-r'+str(n))).exists():n+=1
    dest=P/('failed-complete-check-r'+str(n));dest.mkdir()
    for f in P.iterdir():
        if f.is_file():(dest/f.name).write_bytes(f.read_bytes())
    write(dest/'failure.json',dict(checks=checks,error=traceback.format_exc(),subject_executed=False))
    print('OWN_CRITIC_FILE_DATA_CHECK_FAILED',file=sys.stderr);raise
