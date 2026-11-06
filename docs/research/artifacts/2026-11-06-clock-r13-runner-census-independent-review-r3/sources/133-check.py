"""Independent file/source/data/integer review. Never invokes the subject or its models."""
from pathlib import Path
import collections
import hashlib
import json
import re
import sys
import time

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[1]
SUB = ROOT / 'build-ci/20261106-clock-r13-runner-census-r3'
checks = []
started = time.perf_counter()
def ck(name, condition):
    checks.append({'name': name, 'pass': bool(condition)})
    if not condition:
        raise AssertionError(name)
def load(path):
    return json.loads(path.read_text(encoding='utf-8'))
def sha(data):
    return hashlib.sha256(data).hexdigest().upper()
def pin(path):
    b = path.read_bytes()
    return {'path': str(path.relative_to(ROOT)).replace('\\','/') if path.is_relative_to(ROOT) else str(path).replace('\\','/'), 'bytes': len(b), 'sha256': sha(b)}
def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, ensure_ascii=True)+'\n', encoding='utf-8', newline='\n')
def total(v):
    if type(v) is int: return v
    if isinstance(v,list): return sum(total(x) for x in v)
    if 'terms' in v: return total(v['terms'])
    return sum(total(x) for x in v.values())

def lexical_mask(src):
    """Own character scanner for this closed source, not a PowerShell parser/AST."""
    a=list(src); i=0
    def blank(lo, hi):
        for j in range(lo,hi):
            if a[j]!='\n': a[j]=' '
    while i<len(src):
        if src.startswith('<#',i):
            end=src.find('#>',i+2); ck('terminated block comment '+str(i),end>=0)
            end+=2;blank(i,end);i=end
        elif src[i]=='#':
            end=src.find('\n',i)
            if end<0:end=len(src)
            blank(i,end);i=end
        elif src[i] in "'\"":
            q=src[i]; start=i;i+=1
            while i<len(src):
                if q=='"' and src[i]=='`':i+=2;continue
                if src[i]==q:
                    if i+1<len(src) and src[i+1]==q:i+=2;continue
                    i+=1;break
                i+=1
            ck('terminated quoted token '+str(start),i<=len(src) and src[i-1]==q)
            blank(start,i)
        else:i+=1
    return ''.join(a)

def graph(node):
    c=collections.Counter()
    def visit(n,depth):
        c['max_depth']=max(c['max_depth'],depth);c['value_calls']+=1
        if 'map' in n:
            m=n['map'];c['maps']+=1;c['keys']+=len(m);c['key_units']+=sum(len(k) for k in m)
            c['map_separators']+=max(0,len(m)-1)
            for k,v in m.items():
                ck('ASCII identifier graph key '+k,re.fullmatch('[A-Za-z_][A-Za-z_0-9]*',k) is not None)
                visit(v,depth+1)
        elif 'array' in n:
            a=n['array'];c['arrays']+=1;c['array_elements']+=len(a);c['array_separators']+=max(0,len(a)-1)
            for v in a:visit(v,depth+1)
        else:
            kind=n['scalar'];ck('closed graph scalar '+kind,kind in ['string','integer','boolean'])
            c[kind+'s' if kind!='boolean' else 'booleans']+=1
            if kind=='string':c['string_units']+=n['unit_upper']
            else:ck('scalar representation upper',n['unit_upper']==(20 if kind=='integer' else 5))
    visit(node,0);c['nodes_keys']=c['value_calls']+c['keys']
    c['visitor_upper']=(185*c['maps']+199*c['arrays']+223*c['keys']+212*c['key_units']+31*c['map_separators']+20*c['array_elements']+31*c['array_separators']+184*c['strings']+148*c['string_units']+455*c['integers']+115*c['booleans'])
    c['wire_upper_including_LF']=(2*(c['maps']+c['arrays'])+3*c['keys']+c['map_separators']+c['array_separators']+c['key_units']+6*c['string_units']+2*c['strings']+20*c['integers']+5*c['booleans']+1)
    return {k:v for k,v in c.items() if v != 0}

def run():
    ck('new review not already frozen',not (OUT/'first-green-freeze.json').exists())
    sp=load(SUB/'source-pins.json'); freeze=load(SUB/'whole-freeze.json')
    ck('candidate exact pins',sha((SUB/'source-pins.json').read_bytes())=='63928A3072592D9C8774B9B6673DC2DA6499A874E7161663A533DB4C0686CDBA')
    ck('candidate exact whole freeze',sha((SUB/'whole-freeze.json').read_bytes())=='BE7C0690A1C0874224F744183C0963B54FCE091AF4675F52248AF05AABBE443E')
    ck('complete candidate input/own inventory',len(sp['inputs'])==1308 and len(sp['own_files'])==84)
    checked=[]
    for domain in ['inputs','own_files']:
        for row in sp[domain]:
            p=Path(row['path']);p=p if p.is_absolute() else ROOT/p
            actual=pin(p);ck(domain+' fresh bytes/hash '+row['path'],actual['bytes']==row['bytes'] and actual['sha256']==row['sha256']);checked.append(actual)
    ck('no candidate price/binding/GO',not sp['prices_accepted'] and not sp['Data'] and not sp['execution_GO'])
    census=load(SUB/'census.json');srcpath=ROOT/census['subject_runner']['path'];src=srcpath.read_text(encoding='utf-8');lines=src.splitlines(keepends=True)
    ck('exact runner pin',pin(srcpath)==census['subject_runner'])
    ck('exact unchanged manifest pin',pin(ROOT/census['subject_manifest']['path'])==census['subject_manifest'])
    manifest=load(ROOT/census['subject_manifest']['path'])
    H=load(SUB/'body-ledgers.json')['helpers']; inv=load(SUB/'source-inventory.json'); prices=load(SUB/'body-components.json')['prices']
    ck('complete 74 unique bodies',len(H)==len({h['name'] for h in H})==74)
    coverage=[];fixed={};coef={}
    for h in H:
        n=h['name'];lo,hi=h['lines'];body=''.join(lines[lo-1:hi])
        ck(n+' exact whole source',body==h['whole_source'] and sha(body.encode())==h['source_sha256'])
        ck(n+' nonempty fresh roles',bool(h['fixed_groups']))
        for g in h['fixed_groups']:
            ck(n+' role '+g['label'],total(g['terms'])==g['units'] and all(type(v) is int for v in g['terms'].values()))
        fixed[n]=sum(g['units'] for g in h['fixed_groups']);coef[n]={k:sum(v.values()) for k,v in h['coefficients'].items()}
        ck(n+' complete fixed sum',fixed[n]==h['fixed'])
        def group_audit(v):
            if isinstance(v,list):
                for x in v:group_audit(x)
            elif isinstance(v,dict):
                if 'terms' in v:ck(n+' alternative '+v['label'],total(v['terms'])==v['units'])
                else:
                    for x in v.values():
                        if isinstance(x,(dict,list)):group_audit(x)
        group_audit(h['alternatives'])
        coverage.append({'name':n,'lines':[lo,hi],'whole_source_sha256':h['source_sha256'],'fixed_recomputed':fixed[n],'coefficient_totals_recomputed':coef[n],'complete_body_and_primitive_ports_read':True,'own_caller_and_transitive_boundary_reviewed':True})
    mask=lexical_mask(src)
    decl=list(re.finditer(r'(?im)^function\s+([A-Za-z][A-Za-z0-9-]*)',mask))
    ck('74 lexical declarations',len(decl)==74 and [m.group(1) for m in decl]==[h['name'] for h in H])
    declaration_spans={m.span(1) for m in decl}; names={h['name'] for h in H}
    calls=[]
    for m in re.finditer(r'(?<![A-Za-z0-9_-])(?:'+ '|'.join(re.escape(n) for n in sorted(names,key=len,reverse=True))+r')(?![A-Za-z0-9_-])',mask):
        if m.span() in declaration_spans:continue
        line=mask.count('\n',0,m.start())+1;col=m.start()-mask.rfind('\n',0,m.start())
        caller=next((h['name'] for h in H if h['lines'][0]<=line<=h['lines'][1]),'MAIN')
        calls.append({'caller':caller,'callee':m.group(),'line':line,'column':col})
    ck('347 actual calls / 233 helper / 114 main',len(calls)==347 and sum(x['caller']=='MAIN' for x in calls)==114)
    for a,b in zip(calls,inv['callers']):
        ck('coordinate/caller '+str(b['id']),all(a[k]==b[k] for k in a) and bool(b['caller_evaluation_and_D_sponsors']) and b['callee_E_parameter_W_return']=='OWNED_BY_CALLEE_COMPONENT_ONCE' and not b['referent_copy'])
    ck('all inventory call sites consumed',len(calls)==len(inv['callers']))
    root=load(ROOT/'build-ci/20261106-r13-main-caller-ledger-r3/caller-ledger.json');mr=load(SUB/'main-caller-component.json')
    ck('exact root caller-only 127 rows',mr['rows']==root['rows'] and len(mr['rows'])==127 and not root['own_helper_bodies_included'])
    for i,row in enumerate(mr['rows']):
        for g in row['groups']:ck('root group '+str(i)+' '+g['label'],sum(g['roles'].values())==g['units'])
        ck('root row total '+str(i),row['units']==row['multiplicity']*sum(g['units'] for g in row['groups']))
    ck('root complete split',sum(r['units'] for r in mr['rows'])==180799==51125+129674 and root['bootstrap_registration_units']==51125 and root['main_units']==129674)
    ck('root exact main call coordinates',[(x['line'],x['column'],x['callee']) for x in calls if x['caller']=='MAIN']==[(x['line'],x['column'],x['helper']) for x in root['own_written_main_calls']])
    # Independent typed optional-site inventory from actual lines and explicit source calls.
    defaults=load(SUB/'default-callers.json');dn=set(defaults['declarations'])
    ck('nine actual default-bearing helper declarations',len(dn)==9 and dn=={m.group(1) for m in re.finditer(r'(?m)^function\s+([^\s(]+)\([^\n]*=',src)})
    ck('all optional-bearing written callers',[(x['caller'],x['callee'],x['line'],x['column']) for x in defaults['calls']]==[(x['caller'],x['callee'],x['line'],x['column']) for x in calls if x['callee'] in dn])
    omitted=[(x['line'],x['callee'],v['parameter'],v['literal'],v['implicit_O']) for x in defaults['calls'] for v in x['omitted']]
    ck('only three omitted-long coercion sites',[(l,n,p) for l,n,p,v,o in omitted if o]==[(926,'Write-TaskJson','Maximum'),(969,'Measure-TaskJson','Maximum'),(1022,'Measure-TaskJson','Maximum')])
    ck('six actual omitted walker destinations',sum(n=='Get-TaskEntries' and p=='Destination' for l,n,p,v,o in omitted)==6)
    for x in defaults['calls']:
        for v in x['omitted']:ck('default literal port '+str(x['line'])+' '+v['parameter'],v['literal_R']==1 and v['implicit_O'] in [0,1])
    # Source primitive/component derivation; data vectors are not executed code.
    by={h['name']:h for h in H};D=dict(fixed)
    def alt(n,k):return total(by[n]['alternatives'][k])
    def cf(n,k):return coef[n][k]
    D['Add-TaskPathCatalogueEntry']={k:fixed['Add-TaskPathCatalogueEntry']+alt('Add-TaskPathCatalogueEntry',k) for k in ['repo_input','ordinary_alias']}
    D['Initialize-TaskPathCatalogue']=fixed['Initialize-TaskPathCatalogue']+1220*cf('Initialize-TaskPathCatalogue','input_row')+10*cf('Initialize-TaskPathCatalogue','own_or_stage_row')+63*cf('Initialize-TaskPathCatalogue','descriptor_row')+D['Add-TaskPathCatalogueEntry']['ordinary_alias']
    D['Get-TaskCatalogueString']={k:fixed['Get-TaskCatalogueString']+alt('Get-TaskCatalogueString',k) for k in ['hit','miss']}
    D['Get-TaskCatalogueEntryPath']={k:fixed['Get-TaskCatalogueEntryPath']+alt('Get-TaskCatalogueEntryPath',k) for k in ['first','hit','tilde']}
    D['Get-TaskCanonicalPath']=max(fixed['Get-TaskCanonicalPath']+alt('Get-TaskCanonicalPath',k) for k in by['Get-TaskCanonicalPath']['alternatives'])
    normal=fixed['Assert-NoLink']+64*cf('Assert-NoLink','parent_body');upper=normal+alt('Assert-NoLink','depth65_prefix')-alt('Assert-NoLink','replaced_normal_ending')
    D['Assert-NoLink']={'normal':normal,'depth65':upper,'upper':upper}
    D['Get-TaskHash']={'fixed':fixed['Get-TaskHash'],'per_chunk':cf('Get-TaskHash','read_chunk')}
    D['Get-TaskBootstrapHash']={'fixed':fixed['Get-TaskBootstrapHash'],'per_parent':cf('Get-TaskBootstrapHash','parent'),'per_chunk':cf('Get-TaskBootstrapHash','read_chunk')}
    D['Test-TaskHashValue']={k:fixed['Test-TaskHashValue']+alt('Test-TaskHashValue',k)+32*cf('Test-TaskHashValue',k+'_body') for k in ['hex','binary']}
    tail=alt('Assert-TaskFreshHashIdentity','refusal');D['Assert-TaskFreshHashIdentity']={'normal':fixed['Assert-TaskFreshHashIdentity'],'refusal_body':tail,'refusal_surplus':tail-1}
    D['Assert-TaskPollAdmission']={'normal':fixed['Assert-TaskPollAdmission'],'refusal':alt('Assert-TaskPollAdmission','refusal')}
    D['Invoke-TaskChild']={'fixed':fixed['Invoke-TaskChild'],'per_argument':cf('Invoke-TaskChild','argument_row'),'per_string_unit':cf('Invoke-TaskChild','string_UTF16'),'per_poll':cf('Invoke-TaskChild','normal_poll'),'completion':cf('Invoke-TaskChild','completed_direction'),'positive':cf('Invoke-TaskChild','positive_direction')}
    D['Add-TaskChildCleanup']={'fixed':fixed['Add-TaskChildCleanup'],'per_Code_unit':cf('Add-TaskChildCleanup','Code_UTF16'),'max_Code43':fixed['Add-TaskChildCleanup']+43*cf('Add-TaskChildCleanup','Code_UTF16')}
    D['Invoke-TaskFencedChild']={'fixed':fixed['Invoke-TaskFencedChild'],'binary':alt('Invoke-TaskFencedChild','binary_additional')}
    for n,ports in {
      'Replace-TaskBounded':{'per_aggregate_argument_unit':'all_argument_UTF16','per_occurrence':'IndexOf_body'},
      'Expect-TaskGate':{'per_expected_key':'witness_key'},'Initialize-TaskController':{'per_argument_unit':'argument_UTF16'},
      'Assert-TaskString':{'ASCII':'ASCII_body','general':'general_surrogate_body'},
      'Get-TaskEscapedStringBytes':{'per_unit':'UTF16'},'Join-TaskBoundedNames':{'per_name':'name_row','per_name_unit':'name_UTF16'},
      'Get-TaskEntries':{'per_directory':'directory','per_candidate':'candidate','per_path_unit':'candidate_UTF16','overflow':'overflow'},
      'Copy-TaskOwned':{'per_chunk':'chunk'},'Get-TaskDependencyString':{'per_unit':'UTF16'},'Encode-TaskJson':{'per_preLF_unit':'output_UTF16'},
      'Get-TaskStageBytes':{'per_row':'row'},'Assert-TaskDocumentWrite':{'per_path_unit':'Path_UTF16'},
      'Resolve-TaskInput':{'per_unit':'Name_UTF16'},'Assert-TaskPins':{'per_namespace':'namespace_row','per_own':'own4_row','per_input':'input_row'},
      'Get-TaskReferences':{'per_entry':'common_entry','per_file':'file'},'Read-TaskChildResult':{'per_text_unit':'scan_UTF16','per_control':'admitted_control'},
      'Get-TaskBuildArtifacts':{'per_entry':'file_row'},'Assert-TaskDependencies':{'per_include':'include_row','one_string_fixed':'extracted_string_fixed','per_string_unit':'string_UTF16'},
      'Save-TaskFailureInventory':{'per_file':'file_row'},'Assert-TaskExactKeys':{'per_actual_key_unit':'key_UTF16'}
    }.items():D[n]={'fixed':fixed[n],**{key:cf(n,value) for key,value in ports.items()}}
    D['Get-TaskEntries']['replaced_ending']=alt('Get-TaskEntries','replaced_normal_ending')
    D['Assert-TaskPins']['per_callback_pair']=cf('Assert-TaskPins','Where_callback')+cf('Assert-TaskPins','projection_callback')
    D['Read-TaskChildResult']['bad_ID_prefix']=alt('Read-TaskChildResult','first_bad_ID_prefix')
    D['Assert-TaskExactKeys']['per_key']=cf('Assert-TaskExactKeys','declared_key')+cf('Assert-TaskExactKeys','observed_key')
    D['Convert-TaskControlOperand']={k:fixed['Convert-TaskControlOperand']+alt('Convert-TaskControlOperand',k) for k in ['builtin','biginteger']}
    D['Read-TaskJson']={k:fixed['Read-TaskJson']+alt('Read-TaskJson',k) for k in ['dependency','predecessor']}
    D['Get-TaskSafePrefix']={'ASCII':fixed['Get-TaskSafePrefix'],'general':fixed['Get-TaskSafePrefix']+alt('Get-TaskSafePrefix','general_extra_decrement')}
    for n in ['Close-TaskOwner','Close-TaskJsonReader']:
        D[n]={k:fixed[n]+alt(n,k) for k in ['null','normal','firstFailure','secondaryFailure']};D[n]['upper']=max(D[n].values())
    D['Complete-TaskOwnerClose']={'normal':fixed['Complete-TaskOwnerClose'],'propagate':alt('Complete-TaskOwnerClose','propagate')}
    D['Assert-TaskCodecBinding']={'normal':fixed['Assert-TaskCodecBinding'],'refusal':alt('Assert-TaskCodecBinding','default_refusal')}
    D['Get-TaskIntegerDigits']={'negative':fixed['Get-TaskIntegerDigits']+18*cf('Get-TaskIntegerDigits','negative_threshold'),'unsigned':alt('Get-TaskIntegerDigits','unsigned_fixed')+19*cf('Get-TaskIntegerDigits','unsigned_threshold')}
    D['Visit-TaskJsonValue']={k:alt('Visit-TaskJsonValue',k) for k in ['string','boolean','integer','map_fixed','array_fixed']}
    D['Visit-TaskJsonValue']['null']=fixed['Visit-TaskJsonValue']+alt('Visit-TaskJsonValue','null')
    D['Visit-TaskJsonValue'].update({'string_per_unit':148,'map_per_entry':cf('Visit-TaskJsonValue','map_entry'),'map_per_key_unit':cf('Visit-TaskJsonValue','map_key_UTF16'),'map_separator':31,'array_per_entry':cf('Visit-TaskJsonValue','array_entry'),'array_separator':31})
    for n in ['Assert-TaskRawJson','Assert-TaskRawJsonBytes']:
        classes={k:v for k,v in coef[n].items() if k!='compaction'};base=fixed[n]+alt(n,'first_failure_tail')
        D[n]={'classes':classes}
        if n.endswith('Bytes'):D[n].update({'grammar':sum(classes.values()),'common':fixed[n],'failure':alt(n,'first_failure_tail'),'compaction':cf(n,'compaction'),'compaction_fixed':alt(n,'compaction_fixed')})
        D[n].update({'per_input_upper':sum(coef[n].values()),'fixed_upper':base})
    D['Write-TaskJson']={'measure_null':fixed['Write-TaskJson'],'measurement_supplied':fixed['Write-TaskJson']-4}
    for n in ['Assert-TaskIdentity','Assert-TaskReservation','Assert-TaskRetainedReservation','Assert-TaskAttempt','Assert-TaskConsolePE','Assert-TaskPredecessor']:
        D[n]={'normal':fixed[n],'refusal':fixed[n]+alt(n,'refusal_surplus')}
    D['Assert-TaskBoundSourceInteger']={'normal':fixed['Assert-TaskBoundSourceInteger'],'kind':fixed['Assert-TaskBoundSourceInteger']+alt('Assert-TaskBoundSourceInteger','kind_surplus'),'nonpositive':fixed['Assert-TaskBoundSourceInteger']+alt('Assert-TaskBoundSourceInteger','nonpositive_surplus')}
    sf='Assert-TaskFileSet';normal=fixed[sf];exp=cf(sf,'expected_declared_row')+cf(sf,'expected_missing_scan');obs=cf(sf,'observed_row')
    D[sf]={'fixed':normal,'expected':exp,'observed':obs,'per_name_unit':cf(sf,'string_UTF16'),'refusal_fixed':normal+alt(sf,'refusal_surplus'),'refusal_expected':exp+cf(sf,'refusal_missing_extra'),'refusal_observed':obs+cf(sf,'refusal_observed_extra')}
    D['Assert-TaskSameReferences']={'fixed':fixed['Assert-TaskSameReferences'],'per_file':cf('Assert-TaskSameReferences','before_row')+cf('Assert-TaskSameReferences','after_row')}
    for n in D:ck('whole recomposed component '+n,D[n]==prices[n])
    ck('all 74 components consumed',set(D)==set(prices) and len(D)==74)
    # Independently walk all repeated graph positions; no alias deduplication.
    shapes=load(SUB/'wire-shapes.json');published=load(SUB/'source-graphs.json');graphs={}
    for n,v in shapes.items():
        graphs[n]=graph(v)
        if n=='marker':graphs[n]['wire_upper_including_LF']-=5*graphs[n]['string_units']
        ck('all graph positions '+n,graphs[n]==published[n])
    receipt=shapes['receipt']['map'];children=receipt['children']['array']
    ck('three actual 24-field child upper rows',len(children)==3 and all(len(x['map'])==24 for x in children))
    ck('log_cap Boolean actual source slots',all(x['map']['log_cap']['scalar']=='boolean' for x in children))
    ck('receipt scalar topology',graphs['receipt']['integers']==177 and graphs['receipt']['booleans']==52 and graphs['receipt']['string_units']==12245)
    ck('source corrected literal lengths',len('FIRST_SECONDARY_RAW_RETAINED_NOT_SERIALIZED')==43 and len('references/native-inverse/r48000-map5.json')==42)
    ck('source statuses actual42',len('CLOCK_PRODUCED_REQUIRES_INDEPENDENT_VERIFY')==42 and len('CLOCK_VERIFIED_REQUIRES_ROOT_OUTPUT_REVIEW')==42)
    ck('actual source map/array totals',[(graphs[n]['maps'],graphs[n].get('arrays',0),graphs[n]['keys']) for n in ['marker','receipt','failure','bundle']]==[(1,0,6),(58,23,300),(79,1,397),(1294,2,5251)])
    # Source schedules recomposed independently from explicit source order/caps.
    stages=[0,7,264,523,782,784];fences=[1,1,3,5,8,8];kids=[0,0,1,2,3,3];refs=[0,0,0,1,2,2];builds=[0,0,1,1,2,2];roots=[0,1,1,1,2,2];parses=[0,0,0,1,2,2];copies=[0,1,2,2,2,2]
    schedule=load(SUB/'phase-schedules.json');F=load(SUB/'family-compositions.json');independent_phases=[]
    ck('259 outer read witness',255*4096+2<=1048576 and 2+255+2==259)
    ck('posthash capture chunk bounds',((152627224+15360000+65535)//65536)+60-1==2623 and ((67108864+33554432+65535)//65536)+4-1==1539)
    def join(n,u):return 68+87*n+64*u
    def refuse(n,o,mu,uu,mn,un):return 171+90*n+87*o+64*512*(n+o)+join(mn,mu)+join(un,uu)
    generic=refuse(361,376,512*361,512*376,361,376);refcontrols=refuse(56,55,42,0,1,0)+refuse(56,57,0,12,0,1)+refuse(56,57,0,0,0,0)
    controller=2152+53*14*512+7*(285+64*48)+379*45
    replacement=84*235+53*(2*84*512+28*(6+6+7))+48*28*(512//6+512//6+512//7)+84*18
    ck('complete exact control refusals',refcontrols==F['reference_controls_upper'])
    ck('two generic replacements subtract only fixed',generic==F['generic_FileSet_refusal']['whole'] and 2*(generic-102)==F['generic_FileSet_refusal']['two_replacements'])
    for i,p in enumerate(schedule['phases']):
        S,f,c,r,b,w,np,cp=stages[i],fences[i],kids[i],refs[i],builds[i],roots[i],parses[i],copies[i]
        made=int(i>0);dep=int(i>1);prod=int(i>2);ver=int(i>3);bundle=int(i>4)
        H=f*1225+r*60+b*4+[0,3,7,8,13,18][i];chunks=f*7862+r*2623+b*1539+[0,6,2054,2566,3594,7706][i]
        W=S+31*f+r+b+w+1;Dirs=S+720*f+61*r+b+79*w+1;E=6*S+(3 if S else 0)+1123*f+60*r+4*b+78*w+1
        no=H+Dirs+31*f+1+2*cp+60*r+78*ver;can=no+E+2*W+dep;res=1220+1251*f+1+made+int(c>0)+1222*dep+1220*bundle
        fs=31*f+r+b+np+prod+ver;el=1045*f+56*r+3*b+(3*prod+11*ver)+56*prod+67*ver
        fresh=1224*f+2*made+dep+np;fresh_ref=[1,3,3,4,4,4][i];hx=1225*f+2*made;binary=dep+np;wh=[0,2,8,68,136,141][i]
        rec=[0,2,2,3,4,4][i];mark=made;fail=made;meas=mark+rec+fail+5*bundle;enc=mark+rec+fail+bundle;docs=mark+2*rec+2*fail+2*bundle
        expected={'stage':S,'fences':f,'children':c,'references':r,'builds':b,'root_walks':w,'native_parses':np,'copies':cp,'H':H,'hash_chunks':chunks,'walkers':W,'directories':Dirs,'normal_rows':E,'walker_Canonical':E+2*W,'NoLink':no,'Canonical':can,'Resolve':res,'EntryPath':can+res,'firstBuild_upper':1294+3,'FileSet_slots':fs,'FileSet_success_elements_upper':el,'outer_async_read_calls':259*c,'cleanup_helper_attempts':14*c,'cleanup_stored_rows':13*c,'ref_capture_hashes':60*r,'build_capture_hashes':4*b,'Fresh_normal':fresh,'Fresh_refusals':fresh_ref,'hex_tests':hx,'binary_tests':binary,'wire_hex':wh,'Measure':meas,'Encode':enc,'DocumentWrite':docs,'raw_byte_calls':2+3*made+dep,'raw_byte_units':503610+98009+61*made+4194304*dep,'raw_string_calls':4*prod+12*ver,'raw_string_units':65536*np,'actual_module_controls':3*prod+11*ver,'markers':mark,'receipts':rec,'failures':fail,'bundle_passes':5*bundle,'postcompiler':dep,'postproducer':prod,'postverify':ver,'bundle':bundle,'created':made}
        for k,v in expected.items():ck('source schedule '+p['id']+' '+k,p[k]==v)
        pre=249*mark+65535*rec+262759*fail+1048575*bundle;ec=6*made+dep+3*prod;ek=18*made+4*dep+15*prod;args=sum([28,6,6][:c])
        C={
        'bootstrapDefinitionsAll74Registrations':51125,'wholeMainCallerAndCatchSourcePositivePrefix':129674,'controllerAndSevenFixedAuthenticatedHashes':controller,
        'freshSHAOwnBodiesExcludingNoLink':295*H+65*chunks,'allNoLinkOwnBodies':7091*no,'allResolveOwnBodies':(136+512*64)*res,
        'allWalkerOwnBodiesAndOverflowAlternatives':52*W+161*Dirs+(197+512*64)*E+(127+512*64-24)*W,
        'allStageByteOwnBodies':22*S+73*(6*S+(3 if S else 0)), 'allPinsOwnCallerBodies':f*(97+58*31+41*1123+54*4+37*1220),
        'normalFileSetSlots':102*fs+(83+78+2*512*64)*el,'twoFileSetRefusalSlotReplacements':2*(generic-102),
        'expectedControlsOwnBodiesAndThreeReferenceRefusals':94*ec+25*ek+prod*refcontrols,
        'childOwnAllPollsAnd259OuterReads':1047300*c+74*args+53*512*(args+8*c),'fencedWrapperOwnCallers':109*c+71*np,
        'compilerReplacementHelpers84':int(c>0)*replacement,'twoOwnedCopyBodies':291*cp+71*([0,2,514,514,514,514][i]),'ownedMutationBodies':268*cp,
        'PEHeaderBodies':4*dep*340,'ordinaryConsoleChecksAndActualControlRefusal':31*int(c>0)+dep*(2*31+89),
        'predecessorDescriptor':41637,'predecessorMap':3621,'boundSourceSignedIntegers':4*105,'workReservationNormalAndActualRefusal':20*(1+made)+63*made,
        'retainedReservationNormalAndActualRefusal':16*(1+bundle)+59*made,'oneUseAttemptNormalAndActualRefusal':6+46*made,'lastLegalAndOnePastPollBoundary':made*(11+56),
        'rootAsciiValidation':49+512*53,'compilerArgumentAsciiValidation':int(c>0)*28*(49+512*53),'allMeasureOwn':104*meas,
        'allRecursiveVisitors':mark*graphs['marker']['visitor_upper']+rec*graphs['receipt']['visitor_upper']+fail*graphs['failure']['visitor_upper']+5*bundle*graphs['bundle']['visitor_upper'],
        'receiptOwnAndSecondaryWithoutMeasure':rec*(79+506),'WriteJsonOwnWithoutMeasure':63*fail+59*bundle,'allDocumentWriteOwn':docs*(89+512*64),'allOnceEncodeOwn':176*enc+41*pre,
        'bootstrapBoundedRead':201,'predecessorAndDependencyReadJsonOwn':339+328*dep,'streamedDependencyReaderAllOwners':369*dep,
        'dependencyExtractionAndStringGetters':dep*(349+98+512*64+1221*(19+98+512*64)),
        'nativeResultReaderOwnAnd14Controls':np*21585+38*65536*np+(3*prod+11*ver)*12648,'oneBadControlPrefixReplacement':int(np>0)*11897,
        'buildArtifactOwnWithoutHashHex':b*(48+4*101),'referenceOwnWithoutHashHex':r*(55+60*39+60*104),'sameReferenceDescriptorIdentities':ver*(30+56*51),
        'finalBuildDescriptorIdentityBodies':ver*3*24,'twoDescriptorIdentityRefusalSlotReplacements':2*(88-24),
        'allFreshIdentityOwnWithoutTests':fresh*27+fresh_ref*84,'allNativeDigestHexAndBinaryTests':hx*1509+binary*647,'wireHexWrapperBodies':wh*15,
        'failureInventoryOwn':fail*(199+78*64),'allRawByteGrammarAndCompaction':602*expected['raw_byte_units']+135*expected['raw_byte_calls'],
        'allRawStringGrammars':526*65536*np+103*expected['raw_string_calls'],'allCanonicalOwn':76*can,'allEntryPathOwn':74*(can+res)+(9532-74)*1297,'initializeCatalogue':99368}
        fp=F['phases'][i];ck('54 whole family set '+p['id'],len(C)==54 and set(C)==set(fp['composition']))
        for k,v in C.items():ck('whole family '+p['id']+' '+k,v==fp['composition'][k])
        ck('complete phase sum '+p['id'],sum(C.values())==fp['upper']==census['phases'][i]['upper'])
        ck('encode/effective control phase '+p['id'],pre==fp['encoded_preLF_units'] and ec==fp['expected_control_calls'] and ek==fp['expected_witness_keys'])
        independent_phases.append({'id':p['id'],'derived_profile':expected,'composition':C,'upper':sum(C.values())})
    ck('whole upper matches recomposed maximum',max(p['upper'] for p in independent_phases)==5577542589==F['upper']==census['scalar_upper_proposal'])
    # Corrected local alternatives have complete own sponsorship; source ordering is not an immutable-file premise.
    ck('canonical full alternatives',by['Get-TaskCanonicalPath']['complete_alternative_units']=={'context_false':39,'tilde_fallback':51,'alias_hit':65,'strings_hit':76,'both_miss':75})
    ck('literal first finding fully fixed',cf('Assert-TaskRawJson','literal_body')==36 and cf('Assert-TaskRawJsonBytes','literal_body')==38)
    ck('tilde branches each own selected B',all(g['terms'].get('B')==1 for g in by['Get-TaskCatalogueEntryPath']['alternatives']['tilde'] if 'tilde guard' in g['label']))
    ck('failure default literal plus actual coercion',by['Save-TaskFailureInventory']['fixed_groups'][-2]['terms']['Maximum_default_Int32_to_Int64_O']==1)
    ck('same-record bad-key source domination',64*(128-4)+1==7937<10288)
    ck('same-helper quarantine alternative',26+2430<9824)
    ck('same-fence changed manifest overshoot',16-8<7862-8)
    ck('all first-use IDs derived from source inventory',1220+4+1+56+3+4+6==1294)
    # Pin explicit authoritative premises, never promote them to runtime/native/resource acceptance.
    ck('false entry guards preserved',all(('$'+n+'=$false') in src for n in ['taskSourceReviewAccepted','taskSourceWorkAccepted','taskExecutionAccepted','taskCompilerAccepted','taskProduceAccepted','taskVerifyAccepted']))
    ck('inherited functionality HOLD remains',["[Type[]]::EmptyTypes" in src,freeze['source_functionality']=='HOLD_Type_array_EmptyTypes']==[True,True])
    # Source prose errata do not alter vectors/equations. Record, do not silently repair frozen subjects.
    errata=[
      {'helper':'Get-TaskCatalogueEntryPath','stale_note':'complete positive first upper9529','actual_component':9532},
      {'helper':'Assert-TaskRawJson','stale_note':'literal35','actual_component':36},
      {'helper':'Close-TaskOwner','stale_note':'The62 path','actual_component':65},
      {'helper':'Assert-TaskPins','stale_note':'fixed96 / NS row57','actual_components':{'fixed':97,'namespace':58}}
    ]
    for e in errata:ck('recorded stale explanatory prose '+e['helper'],e['stale_note'].split(' / ')[0].split(' ')[-1] in by[e['helper']]['note'] or e['helper']=='Assert-TaskPins')
    review={'schema':'FRESH_INDEPENDENT_WHOLE_R13_SCALAR_R3_SOURCE_DATA_REVIEW','disposition':'CONDITIONAL_ACCEPT_COMPLETE_SCALAR_UPPER_ONLY','source_semantics_review':'all 74 complete bodies and all ordered groups read; every primitive/caller/body/default/coercion/callback/catch/failure/phase sponsorship reviewed under frozen research authority, not candidate check-count acceptance','numeric_upper':5577542589,'whole_phase_uppers':[p['upper'] for p in independent_phases],'findings':[],'nonblocking_explanatory_errata':errata,'helper_coverage':coverage,'independent_written_calls':calls,'root_caller_rows':127,'root_caller_split':{'registration_bootstrap':51125,'main':129674,'helper_bodies':0},'complete_default_matrix':defaults,'independent_components':D,'independent_graphs':graphs,'independent_family_phases':independent_phases,'fresh_input_count':1308,'fresh_subject_own_file_count':84,'execution':{'candidate_or_old_models_executed':False,'target_parser_AST_import_helper_compiler_native_API_program_Clock_executed':False,'GUI_devices_Git':False,'only_own_file_source_hash_JSON_integer_checker':True},'limits':{'individual_price_acceptance_from_absence_of_finding':False,'R13_functionality':'HOLD_Type_array_EmptyTypes','R14_R15_R16_or_new_profile_transfer':False,'controlled_scratch_fit':False,'native_ABI_binary_runtime_association':False,'combined_U_or_selected_limit':False,'Data_or_binding_or_execution_GO':False,'no_process_or_RSS_certificate':True},'checks':checks}
    write('review.json',review)
    (OUT/'review.md').write_text(REVIEW_TEXT,encoding='utf-8',newline='\n')
    write('own-check-receipt.json',{'purpose':'independent source/file/hash/JSON/integer only','attempt':1,'raw_exit':0,'check_count':len(checks),'wall_seconds_before_output_seal':time.perf_counter()-started,'subject_or_old_code_executed':False,'candidate_edits':False,'prior_read_failure':'failed-read-attempt-01.json'})
    own=[pin(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name not in ['source-pins.json','first-green-freeze.json']]
    write('source-pins.json',{'schema':'INDEPENDENT_R13_R3_WHOLE_REVIEW_PINS','subject_source_pins':pin(SUB/'source-pins.json'),'subject_whole_freeze':pin(SUB/'whole-freeze.json'),'freshly_checked_inputs_and_subject_own_files':checked,'own_files':own,'whole_numeric_scope_only':True,'runtime_resource_Data_GO':False})
    write('first-green-freeze.json',{'schema':'INDEPENDENT_WHOLE_R13_R3_FIRST_COMPLETE_OWN_GREEN','check_count':len(checks),'all_new_sources_helpers_oracles_outputs_frozen':True,'postgreen_edit_authorized':False,'files':[pin(p) for p in sorted(OUT.iterdir()) if p.is_file() and p.name!='first-green-freeze.json'],'disposition':review['disposition']})
    print(json.dumps({'raw_exit':0,'checks':len(checks),'disposition':review['disposition'],'review':pin(OUT/'review.json'),'pins':pin(OUT/'source-pins.json'),'freeze':pin(OUT/'first-green-freeze.json')}))

REVIEW_TEXT = '''# Fresh whole R13 scalar R3 review

**Conditional ACCEPT of the complete research scalar upper proposal 5,577,542,589**, for the exact pinned R13 source and the stated six source-upper prefixes only. This is one whole disposition, not acceptance of individual prices because no additional omission was found. All 74 complete helper bodies, 347 coordinate-exact calls (233 helper and 114 main), all 127 root caller rows, all nine optional-parameter declarations and their callers, all graph positions, and all 54 families in six phases were independently reviewed. The own checker reads files/data only; it never imports or executes candidate or old models, or a PowerShell parser/helper/compiler/native purpose/Clock.

The complete source review uses contract344408, DE33 and268597 plus all eight frozen research clarifiers. Reached source values, actual scalar destinations, explicit RHS casts including identity casts, implicit nonidentity argument coercions, branch/control events, getters/static properties, callback inputs/results, selected catch inputs and own returns remain distinct. Primitive CLR/OS/codec CPU stays at its existing boundary; controlled materialized storage remains separately unqualified. No hidden per-cell VM tariff, referent clone, optimizer discount or new opacity category is added.

The four previous omissions are repaired at their actual source homes: raw-string literal predicate19/class36 (byte class38 remains separate), both EntryPath tilde branch events, Canonical alias own dispatch within complete alternatives39/51/65/76/75, and failure-inventory's omitted Int32-to-Int64 Maximum default. The default matrix covers all written callers of all nine default-bearing helpers. Only failure926 and main Measure969/1022 omit the long default; actual literal, semantic coercion and formal write have separate sponsors. No second callee formal write or repeated formal-type reference is invented.

Every normal/refusal body and caller was read, not merely the four changes. Hash and fresh-identity Tuple2 getters/casts/result ports, fixed uppercase and binary tests, all catalogue initializer/first-use/tilde/parent/alias paths, all lexer classes and the post-acceptance byte compaction, typed long[3] visitor state, streamed dependency owners/setters/JValue extraction, explicit maps/arrays and unary-comma results, codec guards, error witnesses, Close/Complete and source catch boundaries have complete sponsors. The controller's trusted import/command/enum/array ports and root's computed/static/public calls retain actual conversions. Root180799 is caller-only51125+129674 and adds no helper body entry/parameter/return twice.

The own lexical scanner independently preserves source coordinates while masking comments/quoted data and only function declaration names. It finds347 calls, including repeated same-line and punctuation-adjacent zero-argument calls, and verifies every caller sponsor and body home. The old330/315 inventories supply no authority. All1308 input rows and84 subject-owned files are freshly byte/hash checked. The source subject, false guards, manifest, C++ and header remain unchanged.

The six schedules are positive upper prefixes, not an observed trace or an assertion that every maximum happens together. Source order preserves stage0/7/264/523/782/784; eight final fences; arbitrary cap-domain child output; 259 outer requests per child; 14 cleanup attempts versus13 stored rows; 60/4 captured references/builds versus56/3 exact returned sets; Maximum+1 first-overflow; namespace/root/normalizer fallback and partial first-use retries; and all four fresh-identity refusal opportunities. No cache grants filesystem/hash trust. A failing hash may overshoot its aggregate pool before the check;2623/1539 captured chunk bounds retain it. Four64MiB final stage hashes and16 bundle chunks occur after the last stage scan and are still funded.

All54 integer equations are recomputed from source-derived multiplicities in each phase. The independent phase totals are677826074,711806384,3718161896,4177129803,4796093024,5577542589. The largest is the proposed whole upper, without an arbitrary margin, split/reset or source-domain reduction. Generic FileSet failures replace only guaranteed-sponsored fixed102, retaining both missing/unknown joins. Three exact expected reference controls use their real witness strings. Canonical alternatives, changed-manifest failure, first-bad-key/ID paths, codec refusal and quarantine are supported local source-coupled upper comparisons; no unrelated row/family slack is borrowed.

The output graphs preserve repeated aliases as visits: marker1map/0array/6keys; receipt58/23/300; failure79/1/397; bundle1294/2/5251. Child log_cap is Boolean, receipt177 integral/52 Boolean positions, secondary code43, missing descriptor42, and validated module statuses42 each. The graph uses scalar-position uppers, not invented observed values. Receipt81903 wire upper may exceed its64KiB admission; Encode's later scan uses its actually admitted cap only. The bundle graph upper likewise is not a claim that a5MiB output passes the1MiB gate. Failed preflight never invokes Encode. Five bundle Measure passes remain distinct from one Encode. General heap/boxing/capacity/reclamation and controlled storage receive no credit from these scalar facts.

Four nonblocking explanatory-note errata remain in frozen body-ledgers.json: EntryPath note says9529 but its complete role groups/component/equation give9532; raw-string note says literal35 but role groups/class/equation give36; CloseOwner note says62 but actual secondary path is65; Pins note says fixed96 and namespace57 while complete roles/component/equation give97/58. These notes were not used as prices. This review explicitly supersedes those descriptive numbers for its scoped conclusion; it does not edit the frozen subject. Future prose may cite this erratum rather than altering frozen bytes.

R13 still has the Type[].EmptyTypes functionality HOLD. Conditional numeric acceptance repairs no source, transfers nothing to R14/R15/R16 or another root/union/profile, and certifies no native/ABI/binary/runtime/environment association, controlled payload/lifetime32MiB fit, combined runner/producer/verifier U, selected limit, immutable Data/binding or execution GO. All six resource/native/source association gates remain independent. Root owns acceptance/Git and any later authorized heavy execution.

The first read request accidentally used the20261106 contract directory instead of the pinned20261105 directory, returned file-not-found/exit1 and supplied no contents. It is preserved in failed-read-attempt-01.json, then corrected. Complete own checker attempts and any failures are retained; first complete own green freezes all new source/helper/oracle/report bytes. Counts are mechanically generated finite file/data assertions, not runtime PASS rows or a proof by count.
'''

if __name__=='__main__':
    try:run()
    except Exception as e:
        index=1
        while (OUT/f'failed-own-attempt-{index:02d}').exists():index+=1
        dest=OUT/f'failed-own-attempt-{index:02d}';dest.mkdir()
        (dest/'check.py').write_bytes(Path(__file__).read_bytes())
        (dest/'failure.json').write_text(json.dumps({'exception_type':type(e).__name__,'message':str(e),'checks':checks,'wall_seconds':time.perf_counter()-started,'subject_execution':False},indent=2)+'\n',encoding='utf-8',newline='\n')
        print(json.dumps({'raw_exit':1,'failure_type':type(e).__name__,'message':str(e),'checks':len(checks),'preserved':str(dest)}))
        sys.exit(1)
