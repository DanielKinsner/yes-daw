"""Complete fresh R3 file/JSON/source-integer reconstruction.

No PowerShell parser, AST, import, old Python helper or target program is used.
The R2 ordered role vectors are read only as unaccepted numerical/source data.
Every resulting model, caller, graph, schedule and family is rebuilt below.
"""
import copy
import hashlib
import json
from pathlib import Path
import re
import sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parent.parent
PREV=ROOT/'build-ci/20261106-clock-r13-runner-census-r2'
REVIEW=ROOT/'build-ci/20261106-clock-r13-runner-census-independent-review-r2'
MAIN=ROOT/'build-ci/20261106-r13-main-caller-ledger-r3'
SUBJECT=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
sys.path.insert(0,str(HERE))
# Only newly authored own source/data modules, never any old candidate helper.
from shapes import build_shapes, graph_counts
from schedules import reconstruct_schedule
from families import compose_all

CHECKS=[]
def check(name,condition,detail=None):
    row={'name':name,'pass':bool(condition)}
    if detail is not None: row['detail']=detail
    CHECKS.append(row)
    if not condition: raise AssertionError(row)
def load(path): return json.loads(path.read_text(encoding='utf-8'))
def put(name,obj): (HERE/name).write_text(json.dumps(obj,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
def pin(path):
    d=path.read_bytes()
    name=path.relative_to(ROOT).as_posix() if path.is_relative_to(ROOT) else path.as_posix()
    return {'path':name,'bytes':len(d),'sha256':hashlib.sha256(d).hexdigest().upper()}
def units(v):
    if isinstance(v,list): return sum(units(x) for x in v)
    if isinstance(v,dict) and 'terms' in v:
        check('role_group_'+v['label'],all(type(x) is int for x in v['terms'].values()))
        s=sum(v['terms'].values())
        check('role_sum_'+v['label'],s==v['units'])
        return s
    check('plain_integer_vector',isinstance(v,dict) and all(type(x) is int for x in v.values()),v)
    return sum(v.values())
def group(label,lines,**terms): return dict(label=label,source_lines=lines,terms=terms,units=sum(terms.values()))

# Source-only lexical masking: no target parser or AST. Preserve every offset,
# newline and repeated call; remove comments/quoted data and declaration names.
def mask_source(s):
    out=list(s); i=0; state='normal'
    while i<len(s):
        c=s[i]
        if state=='normal':
            if s.startswith('<#',i): out[i]=out[i+1]=' ';i+=2;state='block';continue
            if c=='#':state='comment';out[i]=' '
            elif c in "'\"":state=c;out[i]=' '
        elif state=='comment':
            if c=='\n':state='normal'
            else:out[i]=' '
        elif state=='block':
            if s.startswith('#>',i):out[i]=out[i+1]=' ';i+=2;state='normal';continue
            if c!='\n':out[i]=' '
        else:
            if c!='\n':out[i]=' '
            if state=="'" and c=="'" and i+1<len(s) and s[i+1]=="'":out[i+1]=' ';i+=2;continue
            if state=='"' and c=='`' and i+1<len(s):
                if s[i+1]!='\n':out[i+1]=' '
                i+=2;continue
            if c==state:state='normal'
        i+=1
    check('lexical_mask_terminated',state in ['normal','comment'])
    return ''.join(out)

source_bytes=(SUBJECT/'run-clock.ps1').read_bytes()
source=source_bytes.decode('utf-8');lines=source.splitlines(keepends=True)
manifest=load(SUBJECT/'source-manifest.json');main=load(MAIN/'caller-ledger.json')
old=load(PREV/'source-inventory.json');review=load(REVIEW/'review.json')
check('runner_pin',pin(SUBJECT/'run-clock.ps1')['sha256']=='9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788')
check('manifest_pin',pin(SUBJECT/'source-manifest.json')['sha256']=='10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3')
check('root_main_R3_pin',pin(MAIN/'caller-ledger.json')['sha256']=='6FB853C6244C0C5EEEC68F114471B75953C4A959944F3B44C3D5F61EB7A59BF3')
check('R2_pin_manifest',pin(PREV/'source-pins.json')['sha256']=='DFC0F598BF3CECC00484A3772B86819BA2EF71A11078B26B30ED73ECA8BB4B93')
check('critic_pin_manifest',pin(REVIEW/'source-pins.json')['sha256']=='60D7833F12C237370822E785F3058244A4C2CCB422DBE3064068FB498520B33F')
check('critic_freeze_pin',pin(REVIEW/'first-complete-own-pass-freeze.json')['sha256']=='60AFB6730C193E342C1962E61406F1E99FC3AB532F3B062E461D1AA06F6AB44F')
check('whole_review_REVISE',review['verdict']=='REVISE' and review['check_count']==3770 and len(review['checks'])==3770)
for r in review['checks']: check('critic_data_'+r['name'],r['pass_'] is True)
check('review_complete_shapes',len(review['body_coverage'])==74 and len(review['own_calls'])==347 and len(review['family_coverage'])==324)

# Rebuild all74 role models as current proposals. Numeric components, captions,
# former totals and former call sponsor lists are deliberately not transferred.
MODELS={}
for prev in old['helpers']:
    name=prev['name'];a,b=prev['lines']
    d={k:copy.deepcopy(prev[k]) for k in ['fixed_groups','coefficients','alternatives','separately_composed','note']}
    d.update(name=name,lines=[a,b],whole_source=''.join(lines[a-1:b]),source_sha256=hashlib.sha256(''.join(lines[a-1:b]).encode()).hexdigest().upper(),
             provenance='R2_ROLE_TERMS_AS_UNACCEPTED_DATA_REAUDITED_AGAINST_COMPLETE_FROZEN_SOURCE',
             status='NEW_COMPLETE_R3_PROPOSAL_NOT_ACCEPTED')
    check('whole_body_identity_'+name,d['source_sha256']==prev['sha256'] and d['whole_source']==prev['whole_source'])
    MODELS[name]=d

# F1: AtR1+Length6+ge1 + indexed4+indexed4+cne1+or1+B1=19.
MODELS['Assert-TaskRawJson']['coefficients']['literal_body']={
    'property_for_framework':14,'At_R':1,'Text_Length_getter':6,'ge_O':1,
    'Text_indexed_read':4,'Literal_indexed_read':4,'cne_O':1,'or_O':1,'selected_B':1,'At_increment':3}
# F2: selected B is distinct at each written tilde condition and on first build.
entry=MODELS['Get-TaskCatalogueEntryPath']
for g in entry['alternatives']['tilde']:
    if g['label'] in ['raw tilde guard','canonical tilde guard']:
        g['terms']['B']=1;g['units']=sum(g['terms'].values())
for g in entry['alternatives']['first']:
    if g['label']=='two tilde guards and normalization':
        g['terms']={'raw_tilde_predicate_and_B':13,'FullPath':7,'canonical_tilde_predicate_and_B':13}
        g['units']=sum(g['terms'].values())
# F3: complete alternative maximum, not the sum of an alias dispatch with an
# unreached strings lookup. Explicit upper76 is derived from the strings path.
canonical=MODELS['Get-TaskCanonicalPath']
canonical['fixed_groups']=[group('entry/parameter',[133,133],E=1,W=1),
    group('context own caller and complete callee',[134,134],D=1,result_W=1,Context=26)]
canonical['alternatives']={
 'context_false':[group('short-circuit and branch',[134,134],and_O=1,B=1),group('fallback return',[139,139],type_R=1,Path_R=1,param_W=1,D_E_T=3,T=1)],
 'tilde_fallback':[group('IndexOf tilde predicate',[134,134],receiver_R_W=2,char_type_R=1,literal126_R=1,cast_O=1,param_W=1,D_E_T=3,result_W=1,zero_R=1,lt_O=1,and_O=1,B=1),group('fallback return',[139,139],type_R=1,Path_R=1,param_W=1,D_E_T=3,T=1)],
 'alias_hit':[group('outer path predicate',[134,134],predicate_and_branch=14),group('aliases guard',[135,135],indexed=4,receiver_W=1,Path_R=1,param_W=1,D_E_T=3,result_W=1,B=1),group('alias own caller and return',[135,135],nested_indexed_R_O=7,own_D=1,T=1)],
 'strings_hit':[group('outer path predicate',[134,134],predicate_and_branch=14),group('aliases guard',[135,135],predicate_and_B=12),group('strings guard',[136,136],predicate_and_B=12),group('return strings indexed value',[136,136],nested_indexed_R_O=7,T=1)],
 'both_miss':[group('outer path predicate',[134,134],predicate_and_branch=14),group('aliases guard',[135,135],predicate_and_B=12),group('strings guard',[136,136],predicate_and_B=12),group('fallback return',[139,139],type_R=1,Path_R=1,param_W=1,D_E_T=3,T=1)]}
canonical['note']='Maximum over complete source alternatives. Alias callee EntryPath is separately composed; own D at135 is present. No unrelated slack or unreached lookup sponsors it.'
# F4: default expression R and semantic Int32->Int64 O belong to the caller;
# callee formal W stays in WriteJson's entry, exactly once.
for g in MODELS['Save-TaskFailureInventory']['fixed_groups']:
    if g['label']=='seven-field failure payload/caller':
        g['terms']['Maximum_default_Int32_to_Int64_O']=1;g['units']=sum(g['terms'].values())
# Correct source-coordinate correspondence throughout copied byte grammar and
# shifted ReadChildResult statements. This changes no byte-grammar action.
mapping={511:588,516:594,517:597,518:598,586:675}
for g in MODELS['Assert-TaskRawJsonBytes']['fixed_groups']:
    if g['source_lines'][0] in mapping:g['source_lines']=[mapping[x] for x in g['source_lines']]
MODELS['Assert-TaskRawJsonBytes']['alternatives']['first_failure_tail']['source_lines']=[662,662]
MODELS['Assert-TaskRawJsonBytes']['fixed_groups'][-2]['source_lines']=[595,595]
MODELS['Get-TaskIntegerDigits']['coefficients']['negative_threshold']={'foreach':5,'Value_R':1,'Threshold_R':1,'unary_minus_O':1,'le_O':1,'B':1,'Digits_increment':3}
MODELS['Get-TaskIntegerDigits']['coefficients']['unsigned_threshold']={'foreach':5,'Unsigned_R':1,'Threshold_R':1,'ge_O':1,'B':1,'Digits_increment':3}
MODELS['Get-TaskIntegerDigits']['coefficient_multiplicities']={'negative_threshold':18,'unsigned_threshold':19}
for g in MODELS['Read-TaskChildResult']['fixed_groups']:
    if 879<=g['source_lines'][0]<=892:g['source_lines']=[x+1 for x in g['source_lines']]
    elif g['label']=='catch raw primary':g['source_lines']=[894,894]
    elif g['label']=='owner finally':g['source_lines']=[895,895]

# All coefficients have concrete source incidence, including caller dispatches
# which sit in loops rather than a fixed row. Multiple disjoint spans are kept.
COEFFICIENT_LINES={
'Initialize-TaskPathCatalogue':{'input_row':[[87,87]],'own_or_stage_row':[[88,88],[93,93]],'descriptor_row':[[90,92]]},
'Get-TaskCatalogueEntryPath':{'parent_row':[[113,122]],'alias_row':[[124,129]]},
'Assert-NoLink':{'parent_body':[[144,144]]},'Get-TaskHash':{'read_chunk':[[150,150]]},
'Test-TaskHashValue':{'hex_body':[[163,166]],'binary_body':[[171,171]]},
'Invoke-TaskChild':{'argument_row':[[190,190],[203,203]],'string_UTF16':[[190,190],[202,202]],'normal_poll':[[207,225]],'completed_direction':[[213,214]],'positive_direction':[[215,219]],'environment_row':[[202,202]]},
'Add-TaskChildCleanup':{'Code_UTF16':[[266,266]]},'Replace-TaskBounded':{'all_argument_UTF16':[[285,285]],'IndexOf_body':[[288,288]]},
'Expect-TaskGate':{'witness_key':[[299,299]]},'Get-TaskBootstrapHash':{'parent':[[310,310]],'read_chunk':[[316,316]]},
'Initialize-TaskController':{'argument_UTF16':[[342,342]]},'Assert-TaskString':{'ASCII_body':[[359,364]],'general_surrogate_body':[[359,364]]},
'Get-TaskEscapedStringBytes':{'UTF16':[[431,435]]},
'Visit-TaskJsonValue':{'map_entry':[[456,462]],'map_key_UTF16':[[459,460]],'map_separator':[[461,461]],'array_entry':[[469,469]],'array_separator':[[469,469]]},
'Join-TaskBoundedNames':{'name_row':[[485,485]],'name_UTF16':[[485,485]]},
'Get-TaskEntries':{'directory':[[494,497]],'candidate':[[495,495]],'candidate_UTF16':[[495,495]],'overflow':[[495,495]]},
'Copy-TaskOwned':{'chunk':[[504,504]]},
'Assert-TaskRawJson':{'outer_integer_path':[[518,585]],'string_body':[[535,552]],'hex_body':[[543,546]],'digits_body':[[571,573]],'magnitude_body':[[580,582]],'literal_body':[[561,561]]},
'Assert-TaskRawJsonBytes':{'outer_integer_path':[[598,674]],'string_body':[[615,641]],'hex_body':[[632,635]],'digits_body':[[660,662]],'magnitude_body':[[669,671]],'literal_body':[[650,650]],'compaction':[[679,687]]},
'Get-TaskDependencyString':{'UTF16':[[723,723]]},'Encode-TaskJson':{'output_UTF16':[[744,744]]},
'Get-TaskStageBytes':{'row':[[751,751]]},'Assert-TaskDocumentWrite':{'Path_UTF16':[[754,754]]},'Resolve-TaskInput':{'Name_UTF16':[[804,804]]},
'Assert-TaskPins':{'namespace_row':[[815,819]],'Where_callback':[[817,817]],'projection_callback':[[818,818]],'own4_row':[[823,823]],'input_row':[[824,824]]},
'Assert-TaskFileSet':{'expected_declared_row':[[830,830]],'expected_missing_scan':[[833,833]],'observed_row':[[832,832]],'string_UTF16':[[830,830],[832,832]],'refusal_missing_extra':[[833,833]],'refusal_observed_extra':[[832,832]]},
'Get-TaskReferences':{'common_entry':[[838,838]],'file':[[838,838]]},'Assert-TaskSameReferences':{'before_row':[[841,841]],'after_row':[[841,841]]},
'Read-TaskChildResult':{'scan_UTF16':[[858,863]],'admitted_control':[[868,878]]},'Get-TaskBuildArtifacts':{'file_row':[[899,899]]},
'Assert-TaskDependencies':{'include_row':[[917,917]],'extracted_string_fixed':[[909,909],[917,917]],'string_UTF16':[[909,909],[917,917]]},
'Save-TaskFailureInventory':{'file_row':[[924,924]]},
'Assert-TaskExactKeys':{'declared_key':[[394,394]],'observed_key':[[395,395]],'key_UTF16':[[395,395]]},
'Get-TaskIntegerDigits':{'negative_threshold':[[421,421]],'unsigned_threshold':[[424,424]]}}
for n,m in MODELS.items():
    m['coefficient_source_lines']=COEFFICIENT_LINES.get(n,{})
    check('all_coefficient_locations_'+n,set(m['coefficients'])==set(m['coefficient_source_lines']))
    m['fixed']=units(m['fixed_groups'])
    for k,v in m['coefficients'].items():units(v)
    for k,v in m['alternatives'].items():units(v)

def price_components():
    p={};f=lambda n:MODELS[n]['fixed'];c=lambda n,k:units(MODELS[n]['coefficients'][k]);a=lambda n,k:units(MODELS[n]['alternatives'][k])
    scalar=['Spend-Task','Test-TaskPathCatalogueContext','Get-TaskHashHex','Set-TaskRawExit','Reject-TaskGate','Add-TaskJsonVisit','Get-TaskRuntimeType','Assert-TaskIntegral','Set-TaskSecondary','Get-TaskSecondaryCodes','Measure-TaskJson','Add-TaskJsonBound','Read-TaskBoundedBytes','Read-TaskDependencyJson','Write-TaskJsonBytes','Save-TaskReceipt','Get-TaskPredecessorDescriptor','Get-TaskPEHeader']
    for n in scalar:p[n]=f(n)
    for n,keys in [('Add-TaskPathCatalogueEntry',['repo_input','ordinary_alias']),('Get-TaskCatalogueString',['hit','miss']),('Get-TaskCatalogueEntryPath',['hit','tilde','first']),('Convert-TaskControlOperand',['builtin','biginteger']),('Read-TaskJson',['dependency','predecessor'])]:
        p[n]={k:f(n)+a(n,k) for k in keys}
    n='Get-TaskCanonicalPath';canonical['complete_alternative_units']={k:f(n)+a(n,k) for k in canonical['alternatives']};p[n]=max(canonical['complete_alternative_units'].values())
    n='Initialize-TaskPathCatalogue';p[n]=f(n)+c(n,'input_row')*len(manifest['inputs'])+c(n,'own_or_stage_row')*(4+6)+p['Add-TaskPathCatalogueEntry']['ordinary_alias']+c(n,'descriptor_row')*(56+3+4)
    n='Assert-NoLink';normal=f(n)+64*c(n,'parent_body');depth=normal+a(n,'depth65_prefix')-a(n,'replaced_normal_ending');p[n]={'normal':normal,'depth65':depth,'upper':max(normal,depth)}
    for n,co,key in [('Get-TaskHash','read_chunk','per_chunk'),('Get-TaskBootstrapHash','parent','per_parent')]:p[n]={'fixed':f(n),key:c(n,co)}
    p['Get-TaskBootstrapHash']['per_chunk']=c('Get-TaskBootstrapHash','read_chunk')
    n='Test-TaskHashValue';p[n]={k:f(n)+a(n,k)+32*c(n,k+'_body') for k in ['hex','binary']}
    n='Assert-TaskFreshHashIdentity';p[n]={'normal':f(n),'refusal_body':a(n,'refusal'),'refusal_surplus':a(n,'refusal')-1}
    n='Assert-TaskPollAdmission';p[n]={'normal':f(n),'refusal':a(n,'refusal')}
    maps={
     'Invoke-TaskChild':{'per_argument':'argument_row','per_string_unit':'string_UTF16','per_poll':'normal_poll','completion':'completed_direction','positive':'positive_direction'},
     'Add-TaskChildCleanup':{'per_Code_unit':'Code_UTF16'},'Replace-TaskBounded':{'per_aggregate_argument_unit':'all_argument_UTF16','per_occurrence':'IndexOf_body'},
     'Expect-TaskGate':{'per_expected_key':'witness_key'},'Initialize-TaskController':{'per_argument_unit':'argument_UTF16'},
     'Assert-TaskString':{'ASCII':'ASCII_body','general':'general_surrogate_body'},'Get-TaskEscapedStringBytes':{'per_unit':'UTF16'},
     'Join-TaskBoundedNames':{'per_name':'name_row','per_name_unit':'name_UTF16'},
     'Get-TaskEntries':{'per_directory':'directory','per_candidate':'candidate','per_path_unit':'candidate_UTF16','overflow':'overflow'},
     'Copy-TaskOwned':{'per_chunk':'chunk'},'Get-TaskDependencyString':{'per_unit':'UTF16'},'Encode-TaskJson':{'per_preLF_unit':'output_UTF16'},
     'Get-TaskStageBytes':{'per_row':'row'},'Assert-TaskDocumentWrite':{'per_path_unit':'Path_UTF16'},'Resolve-TaskInput':{'per_unit':'Name_UTF16'},
     'Assert-TaskPins':{'per_namespace':'namespace_row','per_own':'own4_row','per_input':'input_row'},
     'Get-TaskReferences':{'per_entry':'common_entry','per_file':'file'},
     'Read-TaskChildResult':{'per_text_unit':'scan_UTF16','per_control':'admitted_control'},
     'Get-TaskBuildArtifacts':{'per_entry':'file_row'},'Assert-TaskDependencies':{'per_include':'include_row','one_string_fixed':'extracted_string_fixed','per_string_unit':'string_UTF16'},
     'Save-TaskFailureInventory':{'per_file':'file_row'}}
    for n,fields in maps.items():p[n]={'fixed':f(n),**{k:c(n,v) for k,v in fields.items()}}
    p['Add-TaskChildCleanup']['max_Code43']=f('Add-TaskChildCleanup')+43*c('Add-TaskChildCleanup','Code_UTF16')
    n='Get-TaskEntries';p[n]['replaced_ending']=a(n,'replaced_normal_ending')
    n='Assert-TaskPins';p[n]['per_callback_pair']=c(n,'Where_callback')+c(n,'projection_callback')
    n='Read-TaskChildResult';p[n]['bad_ID_prefix']=a(n,'first_bad_ID_prefix')
    n='Invoke-TaskFencedChild';p[n]={'fixed':f(n),'binary':a(n,'binary_additional')}
    n='Get-TaskSafePrefix';p[n]={'ASCII':f(n),'general':f(n)+a(n,'general_extra_decrement')}
    for n in ['Close-TaskOwner','Close-TaskJsonReader']:
        p[n]={k:f(n)+a(n,k) for k in ['null','normal','firstFailure','secondaryFailure']};p[n]['upper']=max(p[n].values())
    n='Complete-TaskOwnerClose';p[n]={'normal':f(n),'propagate':a(n,'propagate')}
    n='Assert-TaskCodecBinding';p[n]={'normal':f(n),'refusal':a(n,'default_refusal')}
    n='Get-TaskIntegerDigits';p[n]={'negative':f(n)+18*c(n,'negative_threshold'),'unsigned':a(n,'unsigned_fixed')+19*c(n,'unsigned_threshold')}
    n='Assert-TaskExactKeys';p[n]={'fixed':f(n),'per_key':c(n,'declared_key')+c(n,'observed_key'),'per_actual_key_unit':c(n,'key_UTF16')}
    n='Visit-TaskJsonValue';p[n]={k:a(n,k) for k in ['string','boolean','integer','map_fixed','array_fixed']}
    p[n].update(null=f(n)+a(n,'null'),string_per_unit=148,map_per_entry=c(n,'map_entry'),map_per_key_unit=c(n,'map_key_UTF16'),map_separator=c(n,'map_separator'),array_per_entry=c(n,'array_entry'),array_separator=c(n,'array_separator'))
    p['Mutate-TaskOwnedBytes']=f('Mutate-TaskOwnedBytes')
    for n in ['Assert-TaskRawJson','Assert-TaskRawJsonBytes']:
        cl={k:c(n,k) for k in ['outer_integer_path','string_body','hex_body','digits_body','magnitude_body','literal_body']}
        if n.endswith('Bytes'):
            p[n]={'classes':cl,'grammar':sum(cl.values()),'common':f(n),'failure':a(n,'first_failure_tail'),'compaction':c(n,'compaction'),'compaction_fixed':a(n,'compaction_fixed'),'per_input_upper':sum(cl.values())+c(n,'compaction'),'fixed_upper':f(n)+max(a(n,'first_failure_tail'),a(n,'compaction_fixed'))}
        else:p[n]={'classes':cl,'per_input_upper':sum(cl.values()),'fixed_upper':f(n)+max(a(n,'first_failure_tail'),a(n,'normal_return'))}
    n='Write-TaskJson';p[n]={'measure_null':f(n),'measurement_supplied':f(n)-units(MODELS[n]['fixed_groups'][2])}
    for n in ['Assert-TaskIdentity','Assert-TaskReservation','Assert-TaskRetainedReservation','Assert-TaskAttempt','Assert-TaskConsolePE','Assert-TaskPredecessor']:
        p[n]={'normal':f(n),'refusal':f(n)+a(n,'refusal_surplus')}
    n='Assert-TaskBoundSourceInteger';p[n]={'normal':f(n),'kind':f(n)+a(n,'kind_surplus'),'nonpositive':f(n)+a(n,'nonpositive_surplus')}
    n='Assert-TaskFileSet';p[n]={'fixed':f(n),'expected':c(n,'expected_declared_row')+c(n,'expected_missing_scan'),'observed':c(n,'observed_row'),'per_name_unit':c(n,'string_UTF16'),'refusal_fixed':f(n)+a(n,'refusal_surplus'),'refusal_expected':c(n,'expected_declared_row')+c(n,'expected_missing_scan')+c(n,'refusal_missing_extra'),'refusal_observed':c(n,'observed_row')+c(n,'refusal_observed_extra')}
    n='Assert-TaskSameReferences';p[n]={'fixed':f(n),'per_file':c(n,'before_row')+c(n,'after_row')}
    check('all_74_complete_components',len(p)==74 and set(p)==set(MODELS))
    return p

PRICES=price_components()

# Every source-written own call, including punctuation-adjacent zero-arg calls.
masked=mask_source(source)
declarations=list(re.finditer(r'(?m)^function\s+([A-Za-z]+-[A-Za-z]+)',masked))
check('all_74_declarations',len(declarations)==74 and {x.group(1) for x in declarations}==set(MODELS))
chars=list(masked)
for m in declarations:chars[m.start(1):m.end(1)]=' '*(m.end(1)-m.start(1))
call_text=''.join(chars)
name_pattern='|'.join(re.escape(n) for n in sorted(MODELS,key=len,reverse=True))
calls=[]
for m in re.finditer(r'(?<![\w-])('+name_pattern+r')(?![\w-])',call_text):
    line=source.count('\n',0,m.start())+1;column=m.start()-source.rfind('\n',0,m.start())
    owner=next((n for n,x in MODELS.items() if x['lines'][0]<=line<=x['lines'][1]),'MAIN')
    refs=[]
    if owner=='MAIN':
        refs=[{'kind':'main_row','index':i,'label':r['label'],'multiplicity':r['multiplicity']} for i,r in enumerate(main['rows']) if r['lines'][0]<=line<=r['lines'][1]]
    else:
        model=MODELS[owner]
        refs=[{'kind':'fixed_group','label':g['label']} for g in model['fixed_groups'] if g['source_lines'][0]<=line<=g['source_lines'][1]]
        for label,spans in model['coefficient_source_lines'].items():
            if any(a<=line<=b for a,b in spans):refs.append({'kind':'coefficient','label':label})
        for key,items in model['alternatives'].items():
            gl=items if isinstance(items,list) else [items] if isinstance(items,dict) and 'terms' in items else []
            for g in gl:
                if g['source_lines'][0]<=line<=g['source_lines'][1]:refs.append({'kind':'alternative','alternative':key,'label':g['label']})
    check('call_has_source_sponsor_'+str(line)+'_'+str(column),bool(refs),{'caller':owner,'callee':m.group(1)})
    calls.append({'id':len(calls)+1,'caller':owner,'callee':m.group(1),'line':line,'column':column,'source':lines[line-1].rstrip('\n'),
                  'caller_evaluation_and_D_sponsors':refs,'callee_E_parameter_W_return':'OWNED_BY_CALLEE_COMPONENT_ONCE','referent_copy':False,'signature_metadata_type_reread':False})
check('all_347_calls',len(calls)==347 and sum(c['caller']=='MAIN' for c in calls)==114)
old_tuples={(c['line'],c['column'],c['callee']) for c in old['callers']}
check('all_347_current_tuples',old_tuples=={(c['line'],c['column'],c['callee']) for c in calls})
check('root_main_exact_114_calls',{(c['line'],c['column'],c['callee']) for c in calls if c['caller']=='MAIN'}=={(c['line'],c['column'],c['helper']) for c in main['own_written_main_calls']})
for r in main['rows']:
    for g in r['groups']:check('main_group_'+g['label'],g['units']==sum(g['roles'].values()))
    check('main_row_'+r['label'],r['units']==r['multiplicity']*sum(g['units'] for g in r['groups']))
check('root_main_sum',sum(r['units'] for r in main['rows'])==main['units']==180799)
check('root_main_partition',main['bootstrap_registration_units']==51125 and main['main_units']==129674 and main['own_helper_bodies_included'] is False)

# Own declarations with explicit source defaults. This is a fixed-source
# declaration inventory, not execution/parsing of the subject language.
DEFAULTS={
 'Invoke-TaskFencedChild':{'ExpectedBinary':{'literal':'$null','source_type':'null','formal':'object','implicit_O':0}},
 'Assert-TaskString':{'Maximum':{'literal':'512','source_type':'Int32','formal':'Int32','implicit_O':0},'Ascii':{'literal':'$false','source_type':'Boolean','formal':'Boolean','implicit_O':0}},
 'Get-TaskSafePrefix':{'Maximum':{'literal':'512','source_type':'Int32','formal':'Int32','implicit_O':0}},
 'Assert-TaskIntegral':{'Nonnegative':{'literal':'$false','source_type':'Boolean','formal':'Boolean','implicit_O':0}},
 'Get-TaskEntries':{'Destination':{'literal':'$null','source_type':'null','formal':'reference_List','implicit_O':0}},
 'Measure-TaskJson':{'Maximum':{'literal':'1048576','source_type':'Int32','formal':'Int64','implicit_O':1}},
 'Read-TaskJson':{'Maximum':{'literal':'1048576','source_type':'Int32','formal':'Int64','implicit_O':1},'ExpectedSha256':{'literal':'$null','source_type':'null','formal':'String','implicit_O':0}},
 'Encode-TaskJson':{'Maximum':{'literal':'1048576','source_type':'Int32','formal':'Int64','implicit_O':1},'Measurement':{'literal':'$null','source_type':'null','formal':'object','implicit_O':0}},
 'Write-TaskJson':{'Maximum':{'literal':'1048576','source_type':'Int32','formal':'Int64','implicit_O':1},'Measurement':{'literal':'$null','source_type':'null','formal':'object','implicit_O':0}}}
declaration_default_names={x.group(1) for x in declarations if '=' in lines[source.count('\n',0,x.start())].split('{',1)[0]}
check('complete_default_declarations',declaration_default_names==set(DEFAULTS))
for c in calls:
    if c['callee'] in DEFAULTS:
        c['default_signature_audit']=DEFAULTS[c['callee']]
        c['default_audit_scope']='All candidate calls inspected: actual supplied/default source values and semantic conversions stay at caller; formal W stays at callee. See default-callers.json.'

default_callers=[]
for call in calls:
    n,line=call['callee'],call['line']
    if n not in DEFAULTS:continue
    omitted=[]
    if n=='Invoke-TaskFencedChild' and line==991:omitted=['ExpectedBinary']
    if n=='Assert-TaskString':
        if line in [395,429,459,485,495,723,754,804,830,832]:omitted=['Ascii']
        if line==798 and call['column']==min(x['column'] for x in calls if x['line']==798 and x['callee']==n):omitted=['Ascii']
    if n=='Assert-TaskIntegral' and line==417:omitted=['Nonnegative']
    if n=='Get-TaskEntries' and line in [751,817,838,899,962,1012]:omitted=['Destination']
    if n=='Measure-TaskJson' and line in [969,1022]:omitted=['Maximum']
    if n=='Write-TaskJson' and line==926:omitted=['Maximum','Measurement']
    row={'call_id':call['id'],'caller':call['caller'],'callee':n,'line':line,'column':call['column'],'omitted':[],
         'supplied_optional_parameters':[k for k in DEFAULTS[n] if k not in omitted],
         'caller_sponsors':call['caller_evaluation_and_D_sponsors'],'formal_W_owner':'callee entry once; no added default formal W'}
    for parameter in omitted:
        facts=copy.deepcopy(DEFAULTS[n][parameter]);facts.update(parameter=parameter,literal_R=1)
        facts['sponsor']='root main R3 row' if call['caller']=='MAIN' else 'current helper fixed/loop/alternative role ledger'
        row['omitted'].append(facts)
    default_callers.append(row)
check('all_defaulted_call_sites',len(default_callers)==sum(c['callee'] in DEFAULTS for c in calls))
check('only_three_actual_omitted_long_default_sites',[(r['callee'],r['line']) for r in default_callers if any(x['implicit_O']==1 for x in r['omitted'])]==[('Write-TaskJson',926),('Measure-TaskJson',969),('Measure-TaskJson',1022)])
check('none_of_actual_Encode_calls_omits_Measurement',all(not r['omitted'] for r in default_callers if r['callee']=='Encode-TaskJson'))
check('all_GetEntries_defaults',len([r for r in default_callers if r['callee']=='Get-TaskEntries' and r['omitted']])==6)

# Every call's callee body has one declared composition home. This is distinct
# from its caller source evaluation/D reference, never an additional charge.
for call in calls:
    if call['caller']=='MAIN':call['callee_body_sponsor']='named global family schedule; root excludes all helper bodies'
    else:
        model=MODELS[call['caller']]
        separate=any(call['callee'] in x for x in model['separately_composed'])
        call['callee_body_sponsor']='separate global/helper composition, not included in caller numeric own body' if separate else 'embedded named callee component in caller fixed/loop/alternative source terms'

# Rebind representative repeated transitive atomic operands throughout ALL74
# models. The assertions below do not accept those prices; they ensure no old
# quoted body silently disagrees with the freshly evaluated named source model.
rebindings=[]
def require_binding(owner,container,key,expected,explanation):
    check('transitive_binding_'+owner+'_'+key,container[key]==expected)
    rebindings.append({'owner':owner,'term':key,'value':container[key],'recomputed':expected,'reason':explanation})
for name,m in MODELS.items():
    def visit_terms(value):
        if isinstance(value,list):
            for x in value:yield from visit_terms(x)
        elif isinstance(value,dict):
            if 'terms' in value:yield value['terms']
            elif all(type(x) is int for x in value.values()):yield value
            else:
                for x in value.values():yield from visit_terms(x)
    for terms in visit_terms([m['fixed_groups'],m['coefficients'],m['alternatives']]):
        for key in list(terms):
            if key in ['String_fixed','String_general','String_ASCII','RuntimeType','Context','Reject','SetSecondary','AddBound','AddVisit','ReadBounded_identity','GetDependencyString','WriteBytes_own','GetHashHex']:
                wanted={'String_fixed':PRICES['Assert-TaskString']['fixed'],'String_general':PRICES['Assert-TaskString']['general'],
                        'String_ASCII':PRICES['Assert-TaskString']['ASCII'],'RuntimeType':PRICES['Get-TaskRuntimeType'],
                        'Context':PRICES['Test-TaskPathCatalogueContext'],'Reject':PRICES['Reject-TaskGate'],
                        'SetSecondary':PRICES['Set-TaskSecondary'],'AddBound':PRICES['Add-TaskJsonBound'],
                        'AddVisit':PRICES['Add-TaskJsonVisit'],'ReadBounded_identity':PRICES['Read-TaskBoundedBytes'],
                        'GetDependencyString':PRICES['Get-TaskDependencyString']['fixed'],'WriteBytes_own':PRICES['Write-TaskJsonBytes'],
                        'GetHashHex':PRICES['Get-TaskHashHex']}[key]
                # Composite AddBound31 and String_ASCII64 entries include their
                # explicitly named caller/literal ports; verify those separately.
                if key=='AddBound' and terms[key]==31:wanted+=4
                if key=='String_ASCII' and terms[key]==3441:wanted=PRICES['Assert-TaskString']['fixed']+64*PRICES['Assert-TaskString']['ASCII']
                require_binding(name,terms,key,wanted,'freshly rebuilt source component plus only explicitly named caller ports')
            exact_bindings={
                'String_miss':PRICES['Get-TaskCatalogueString']['miss'],
                'String_miss_and_caller':PRICES['Get-TaskCatalogueString']['miss']+3,
                'parent_String_miss_and_caller':PRICES['Get-TaskCatalogueString']['miss']+3,
                'Add_true':PRICES['Add-TaskPathCatalogueEntry']['repo_input'],
                'Add_false':PRICES['Add-TaskPathCatalogueEntry']['ordinary_alias'],
                'Spend64_caller_and_body':PRICES['Spend-Task']+3,'Spend32_caller_and_body':PRICES['Spend-Task']+3,
                'Spend128_caller_and_body':PRICES['Spend-Task']+3,'Spend1':PRICES['Spend-Task']+3,
                'Spend2':PRICES['Spend-Task']+3,'Spend1_with_conversion':PRICES['Spend-Task']+3,
                'Spend_caller_and_body':PRICES['Spend-Task']+3,'Spend_literal_caller_conversion_body':PRICES['Spend-Task']+3,
                'two_Spend_literal_callers_and_bodies':2*(PRICES['Spend-Task']+3),
                'Codec':PRICES['Assert-TaskCodecBinding']['normal'],
                'Codec_caller_body':PRICES['Assert-TaskCodecBinding']['normal']+1,
                'Codec_D_body':PRICES['Assert-TaskCodecBinding']['normal']+1,
                'two_Close_max':2*PRICES['Close-TaskOwner']['upper'],
                'two_Close_callers_and_max':2*(PRICES['Close-TaskOwner']['upper']+2),
                'two_Close_caller_and_max':2*(PRICES['Close-TaskOwner']['upper']+2),
                'Close_caller_and_max':PRICES['Close-TaskOwner']['upper']+2,
                'CloseOwner_max':PRICES['Close-TaskOwner']['upper'],
                'CloseJson_caller_and_max':PRICES['Close-TaskJsonReader']['upper']+2,
                'CloseText_caller_and_max':PRICES['Close-TaskOwner']['upper']+2,
                'CloseMemory_caller_and_max':PRICES['Close-TaskOwner']['upper']+2,
                'Complete_max':PRICES['Complete-TaskOwnerClose']['propagate'],
                'Complete_caller_and_max':PRICES['Complete-TaskOwnerClose']['propagate']+3}
            if key in exact_bindings:require_binding(name,terms,key,exact_bindings[key],'complete rebuilt callee and explicitly named source caller ports only')

plain_alt_lines={
 'Close-TaskOwner':{'null':[[403,403]],'normal':[[403,403]],'firstFailure':[[403,403]],'secondaryFailure':[[403,403]]},
 'Close-TaskJsonReader':{'null':[[698,698]],'normal':[[698,698]],'firstFailure':[[698,698]],'secondaryFailure':[[698,698]]},
 'Invoke-TaskFencedChild':{'binary_additional':[[280,280]]},
 'Assert-TaskPollAdmission':{'refusal':[[184,184]]},
 'Visit-TaskJsonValue':{'null':[[446,446]],'string':[[448,448]],'boolean':[[449,449]],'integer':[[450,450]],'map_fixed':[[451,464]],'array_fixed':[[466,470]]},
 'Assert-TaskIdentity':{'refusal_surplus':[[777,777]]},'Assert-TaskBoundSourceInteger':{'kind_surplus':[[782,782]],'nonpositive_surplus':[[785,785]]},
 'Assert-TaskReservation':{'refusal_surplus':[[787,787]]},'Assert-TaskRetainedReservation':{'refusal_surplus':[[788,788]]},'Assert-TaskAttempt':{'refusal_surplus':[[789,789]]},
 'Assert-TaskPredecessor':{'refusal_surplus':[[794,794]]},'Assert-TaskConsolePE':{'refusal_surplus':[[847,847]]},
 'Assert-TaskFileSet':{'refusal_surplus':[[834,834]]}}
for call in calls:
    if call['caller']=='MAIN':continue
    for label,spans in plain_alt_lines.get(call['caller'],{}).items():
        if any(a<=call['line']<=b for a,b in spans):call['caller_evaluation_and_D_sponsors'].append({'kind':'scalar_alternative_vector','alternative':label,'source_lines':spans})
for n,m in MODELS.items():m['scalar_alternative_source_lines']=plain_alt_lines.get(n,{})

# All public-cmdlet declaration ports are freshly checked against pinned source,
# not presumed from command names. Their CPU body remains opaque by contract.
ports=load(PREV/'public-cmdlet-ports.json')
for r in ports['ports']:
    primary=ROOT/r['primary_path'];actual=primary.read_text(encoding='utf-8-sig').splitlines()
    check('public_port_'+r['command']+'_'+r['parameter'],r['declaration'].strip()==actual[r['line']-1].strip())
ports['schema']='R13_R3_FRESHLY_RECHECKED_SELECTED_PUBLIC_CMDLET_PORTS'
ports['whole_price_accepted']=False

schedule=reconstruct_schedule(manifest,len((SUBJECT/'source-manifest.json').read_bytes()))
shapes=build_shapes(len(manifest['inputs']))
graphs={k:graph_counts(v) for k,v in shapes.items()}
composition=compose_all(PRICES,schedule,graphs,main)

DOMINATION=[
 {'id':'canonical_alternatives','source_lines':[133,140],'actual_alternatives':canonical['complete_alternative_units'],'upper':max(canonical['complete_alternative_units'].values()),'scope':'same helper complete mutually exclusive source paths; own aliasD included'},
 {'id':'manifest_changed_overshoot','source_lines':[821,824],'current_manifest_chunks':(len((SUBJECT/'source-manifest.json').read_bytes())+65535)//65536,'changed_manifest_cap_chunks':16,'unreached_owned_input_normal_chunks':schedule['facts']['fence_chunks']-(len((SUBJECT/'source-manifest.json').read_bytes())+65535)//65536,'scope':'same failing fence before sources/inputs; no immutability or unrelated family'},
 {'id':'bad_key_validation','source_lines':[869,884],'key_unit_cap':128,'shortest_expected_key_units':4,'extra_source_upper':64*(128-4)+1,'same_record_unreached_ASCII':10288,'scope':'throw before the SAME control or summary mandatory String64/String128 validation'},
 {'id':'codec_refusal','source_lines':[351,351],'normal_body':PRICES['Assert-TaskCodecBinding']['normal'],'refusal_body':PRICES['Assert-TaskCodecBinding']['refusal'],'same_enclosing_helper_unreached_T':1,'scope':'Controller final T directly after Codec; Measure/ReadJson/Encode/ReadChild have larger later source prefix'},
 {'id':'quarantine_or_dispose','source_lines':[241,245],'disposal_alternative':9824,'quarantine_alternative':2456,'scope':'same child branch after shared settled/exit test; no disposal physical-free credit'},
 {'id':'walker_first_overflow','source_lines':[493,499],'per_walk_extra_candidates':1,'canonical_calls_per_walk':2,'normal_canonical_per_candidate':1,'scope':'initial Enqueue root and first-overflow Candidate; normal candidates covered by E'},
 {'id':'Entry_first_failure_retries','source_lines':[102,132],'known_identities':1294,'later_first_fail_sites':3,'upper':1297,'scope':'failure inventory root walk, failure-document stage walk, footer receipt stage walk after main abort'},
 {'id':'FileSet_refusal_replacements','source_lines':[827,835],'slots':2,'only_subtracted_sponsored_fixed':PRICES['Assert-TaskFileSet']['fixed'],'scope':'same actual failed slot; never subtract max-N normal work'},
 {'id':'raw_class_extent','source_lines':[511,689],'string_classes':PRICES['Assert-TaskRawJson']['classes'],'byte_classes':PRICES['Assert-TaskRawJsonBytes']['classes'],'scope':'each disjoint scan/replay class bounded by original input extent, not arbitrary retries'},
 {'id':'bad_ID_prefix','source_lines':[868,893],'extra_prefix':PRICES['Read-TaskChildResult']['bad_ID_prefix'],'raw_call_prefix_bounds':[4,16],'scope':'first bad control prefix replaces same invocation unreached summary Raw; complete reader common/finally still retained'},
 {'id':'posthash_pool_admission','source_lines':[838,899],'reference':{'pool':152627224,'largest':15360000,'attempts':60,'chunks':schedule['facts']['ref_chunk_upper']},'build':{'pool':67108864,'largest':33554432,'attempts':4,'chunks':schedule['facts']['build_chunk_upper']},'scope':'last hash precedes aggregate guard; ceil sum <=ceil(total/65536)+N-1'},
 {'id':'outer_requests','source_lines':[205,219],'cap':1048576,'minimum_nonterminal_positive':4096,'maximum_full_chunks_with_two_short_positive':255,'maximum_short_EOF_positives':2,'initial':2,'outer_requests':259,'scope':'outer source requests only; inner async/resource/fault ownership unqualified'}]
check('local_manifest_domination',DOMINATION[1]['unreached_owned_input_normal_chunks']>=16-DOMINATION[1]['current_manifest_chunks'])
check('local_bad_key_domination',DOMINATION[2]['extra_source_upper']<=DOMINATION[2]['same_record_unreached_ASCII'])
check('local_codec_domination',DOMINATION[3]['refusal_body']-DOMINATION[3]['normal_body']<=1)
check('local_quarantine_domination',2456<=9824)
check('source_raw_other_outer_classes',max(MODELS['Assert-TaskRawJson']['alternatives']['outer_other_paths'].values())<=PRICES['Assert-TaskRawJson']['classes']['outer_integer_path'])
check('read_regression_old_literal_omits_action',18<1+6+1+4+4+1+1+1)
check('read_regression_old_tilde_omits_branch',12<2+1+1+1+1+3+1+1+1+1)
check('read_regression_old_alias_omits_D',8<7+1+1)
check('read_regression_old_default_omits_O',1<1+1)

check('F1_guard19_and_class36',sum(v for k,v in MODELS['Assert-TaskRawJson']['coefficients']['literal_body'].items() if k not in ['property_for_framework','At_increment'])==19 and PRICES['Assert-TaskRawJson']['classes']['literal_body']==36)
check('F1_raw_string526',PRICES['Assert-TaskRawJson']['per_input_upper']==526)
check('F1_byte_literal_unchanged38',PRICES['Assert-TaskRawJsonBytes']['classes']['literal_body']==38)
check('F2_each_distinct_B',all(g['terms']['B']==1 and g['units']==13 for g in entry['alternatives']['tilde'] if g['label'] in ['raw tilde guard','canonical tilde guard']))
check('F3_exact_alternatives',canonical['complete_alternative_units']=={'context_false':39,'tilde_fallback':51,'alias_hit':65,'strings_hit':76,'both_miss':75})
check('F3_alias_dispatch_once',canonical['alternatives']['alias_hit'][-1]['terms']=={'nested_indexed_R_O':7,'own_D':1,'T':1})
check('F4_actual_coercion_once',next(g for g in MODELS['Save-TaskFailureInventory']['fixed_groups'] if g['label']=='seven-field failure payload/caller')['terms']['Maximum_default_Int32_to_Int64_O']==1)
check('graph_child_actual_classification',graphs['receipt']['integers']==177 and graphs['receipt']['booleans']==52)
check('graph_source_lengths',len('FIRST_SECONDARY_RAW_RETAINED_NOT_SERIALIZED')==43 and len('references/native-inverse/r48000-map5.json')==42)
check('graph_receipt_geometry',graphs['receipt']['string_units']==12245 and graphs['receipt']['keys']==300 and graphs['receipt']['maps']==58)
check('graph_receipt_recomposed',graphs['receipt']['visitor_upper']==2634579 and graphs['receipt']['wire_upper_including_LF']==81903)
check('phase_count_and_families',len(composition['phases'])==6 and all(len(p['composition'])==54 for p in composition['phases']))
for p in composition['phases']:
    check('phase_sum_'+p['id'],p['upper']==sum(p['composition'].values()))
    for n,v in p['composition'].items():check('family_'+p['id']+'_'+n,type(v) is int and v>=0)

# Entire admitted-source pin matrix, old packet and critic preserved. Read each
# physical identity separately; equal bytes at distinct paths do not coalesce.
input_rows={}
for container in [load(PREV/'source-pins.json')]:
    for r in container['inputs']+container['own_files']:input_rows[r['path']]=r
for r in load(REVIEW/'source-pins.json')['inputs']:input_rows[r['path']]=r
for folder,names in [(REVIEW,['review.md','review.json','source-pins.json','first-complete-own-pass-freeze.json']),
                     (MAIN,['caller-ledger.json','argument.md','source-pins.json'])]:
    for n in names:input_rows[pin(folder/n)['path']]=pin(folder/n)
for r in manifest['inputs']:
    p=Path(r['path']);p=p if p.is_absolute() else ROOT/p
    actual=pin(p)
    check('admitted_'+r['path'],actual['bytes']==r['bytes'] and actual['sha256']==r['sha256'].upper())
for name,r in input_rows.items():
    p=Path(name);p=p if p.is_absolute() else ROOT/p
    actual=pin(p)
    check('preserve_'+name,actual['bytes']==r['bytes'] and actual['sha256']==r['sha256'].upper())
put('input-pins.json',list(input_rows.values()))

profile={'nodes':0,'depth':0,'max_string_units':0}
def profile_walk(v,d=0):
    profile['nodes']+=1;profile['depth']=max(profile['depth'],d)
    if isinstance(v,dict):
        for k,x in v.items():profile['nodes']+=1;profile['max_string_units']=max(profile['max_string_units'],len(k));profile_walk(x,d+1)
    elif isinstance(v,list):
        for x in v:profile_walk(x,d+1)
    elif isinstance(v,str):profile['max_string_units']=max(profile['max_string_units'],len(v.encode('utf-16-le'))//2)
profile_walk(manifest)
check('bootstrap_original16104',profile=={'nodes':16104,'depth':4,'max_string_units':435})
check('source_false_guards',all(re.search(r'\$'+n+r'=\$false',source) for n in ['taskSourceReviewAccepted','taskSourceWorkAccepted','taskExecutionAccepted','taskCompilerAccepted','taskProduceAccepted','taskVerifyAccepted']))
check('R13_defect_preserved','[Type[]]::EmptyTypes' in source)
header=(SUBJECT/'source-costs.h').read_text(encoding='utf-8')
check('native_Catalogue_value_undefined','static const Catalogue reviewed_;' in header and not any('const Catalogue Catalogue::reviewed_' in x for x in header.splitlines() if not x.lstrip().startswith('//')))
check('no_prices_in_source',all(manifest['source_work'][k]=='HOLD_UNBOUND' for k in ['runner_upper','producer_upper','verifier_upper','whole_upper']))

put('body-ledgers.json',{'schema':'R13_R3_COMPLETE_74_SOURCE_ROLE_LEDGERS','status':'ALL_PROPOSED_NOT_ACCEPTED','helpers':list(MODELS.values())})
put('body-components.json',{'prices':PRICES,'all_74_rebuilt':True,'accepted_prices':False})
put('default-callers.json',{'declarations':DEFAULTS,'calls':default_callers,'complete_all_optional_call_sites':True,'source_types_not_formal_rewrites':True})
put('transitive-bindings.json',rebindings)
put('public-cmdlet-ports.json',ports)
put('source-inventory.json',{'helpers':[{'name':n,'lines':m['lines'],'sha256':m['source_sha256'],'bytes':len(m['whole_source'].encode())} for n,m in MODELS.items()],
     'callers':calls,'manifest_fields':old['manifest_fields'],'lexical_scanner_only':True})
put('main-caller-component.json',{'source':pin(MAIN/'caller-ledger.json'),'status':'CALLER_ONLY_UNACCEPTED_DATA','rows':main['rows'],'units':main['units'],'own_bodies_included':False})
put('source-graphs.json',graphs)
put('wire-shapes.json',shapes)
put('phase-schedules.json',schedule)
put('family-compositions.json',composition)
put('source-coupled-domination.json',{'entries':DOMINATION,'unrelated_slack_or_padding':False,'all_source_theorems_proposed_not_accepted':True})
put('census.json',{'schema':'R13_R3_COMPLETE_SOURCE_SCALAR_UPPER_PROPOSAL_V1','state':'WHOLE_NEW_UNACCEPTED_PROPOSAL_DIFFERENT_WHOLE_CRITIC_REQUIRED',
    'subject_runner':pin(SUBJECT/'run-clock.ps1'),'subject_manifest':pin(SUBJECT/'source-manifest.json'),
    'helper_count':74,'actual_call_occurrences':347,'helper_calls':233,'main_calls':114,'families':54,'phase_count':6,
    'scalar_upper_proposal':composition['upper'],'phases':composition['phases'],'graphs':graphs,
    'main_component':{'path':str((MAIN/'caller-ledger.json').relative_to(ROOT)),'sha256':pin(MAIN/'caller-ledger.json')['sha256'],'units':main['units'],'status':'CALLER_ONLY_UNACCEPTED_DATA'},
    'bootstrap_json_profile':profile,'source_functionality':'HOLD_CONFIRMED_Type_array_EmptyTypes_DEFECT',
    'source_prices_accepted':False,'resource_fit':False,'combined_U':False,'Data':False,'selected_limit':None,'execution_GO':False,
    'R14_R15_transfer':False,'no_target_execution':True,'no_old_helper_execution':True})
argument=(HERE/'argument.template.md').read_text(encoding='utf-8')
argument=argument.replace('{{PROPOSAL}}',format(composition['upper'],','))
argument=argument.replace('{{PHASES}}',', '.join(format(x['upper'],',') for x in composition['phases']))
argument=argument.replace('{{ENTRY}}',json.dumps(PRICES['Get-TaskCatalogueEntryPath'],sort_keys=True))
check('no_unfilled_argument_fields','{{' not in argument and '}}' not in argument)
(HERE/'argument.md').write_text(argument,encoding='utf-8',newline='\n')
md=['# Complete R13 scalar census R3 — proposal only','',f"Runner upper proposal: **{composition['upper']:,}**. All prices/fit/combined U/Data/GO remain unaccepted.",'',
    '| Phase | Complete source upper |','| --- | ---: |']
md += [f"| {x['id']} | {x['upper']:,} |" for x in composition['phases']]
md += ['','All74 bodies,347 calls (233 helper/114 main),54 families ×6 phases. Root main180799 is caller-only JSON data. Exact source remains R13; Type[].EmptyTypes remains HOLD.','',
       'See argument.md, body-ledgers.json, source-inventory.json, default-callers.json, source-coupled-domination.json and family-compositions.json. No target/old helper execution.','',
       '| Helper | Rebuilt proposal components |','| --- | --- |']
md += ['| '+name+' | '+json.dumps(value,sort_keys=True).replace('|','\\|')+' |' for name,value in PRICES.items()]
md += ['','| Family | '+' | '.join(x['id'] for x in composition['phases'])+' |','| --- | '+' | '.join(['---:']*6)+' |']
md += ['| '+n+' | '+' | '.join(str(x['composition'][n]) for x in composition['phases'])+' |' for n in composition['phases'][0]['composition']]
(HERE/'census.md').write_text('\n'.join(md)+'\n',encoding='utf-8',newline='\n')
check('MD_JSON_upper_consistency',format(composition['upper'],',') in (HERE/'census.md').read_text() and format(composition['upper'],',') in argument)
put('checks.json',{'count':len(CHECKS),'failed':sum(not r['pass'] for r in CHECKS),'checks':CHECKS,'scope':'OWN_FILE_SOURCE_JSON_INTEGER_ONLY'})
print(json.dumps({'complete_own_checks':len(CHECKS),'failed':0,'proposal':composition['upper'],'phases':[x['upper'] for x in composition['phases']],'helpers':74,'calls':347,'families':54,'semantic_acceptance':False},sort_keys=True))
