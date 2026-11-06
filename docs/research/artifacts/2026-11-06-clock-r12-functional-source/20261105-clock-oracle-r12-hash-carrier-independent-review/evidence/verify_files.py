"""Independent R12 review: ordinary files, JSON, text and integer arithmetic only."""
from pathlib import Path
import collections
import difflib
import hashlib
import json
import os
import re

ROOT=Path(__file__).resolve().parents[3]
BASE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
OLD=BASE.parent/'clock-oracle-r11-tuple-metadata-source-r1'
AUTHOR=ROOT/'build-ci/20261105-clock-oracle-r12-hash-carrier-source-authoring-r1'
OUT=Path(__file__).resolve().parent
checks=[]
files={}
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):
    p=Path(p)
    if not p.is_absolute():p=ROOT/p
    b=p.read_bytes()
    try:name=p.relative_to(ROOT).as_posix()
    except ValueError:name=p.as_posix()
    files[name]={'path':name,'bytes':len(b),'sha256':sha(b)}
    return b
def load(p):return json.loads(read(p))
def ck(name,ok):
    assert ok,name
    checks.append({'check':name,'result':'PASS'})
def pin(r,p=None):
    b=read(p or r['path']);ck('pin '+r['path'],len(b)==r['bytes'] and sha(b)==r['sha256'])

ap=load(AUTHOR/'source-pins.json')
ck('author manifest supplied seal',sha(read(AUTHOR/'source-pins.json'))=='B22F259F0BF75F786177C0CCBCDECCD893E8DC110993FAE4E57525A6C0DA3A13')
for group in ['subject','author_evidence','inputs']:
    for r in ap[group]:pin(r)
c=load(AUTHOR/'source-correspondence.json')
ck('author correspondence supplied seal',sha(read(AUTHOR/'source-correspondence.json'))=='89FEA73C3C03B29541AC2FEBF0704EA86D2C8E4418828C5E939C6CBC87DF3BCC')
m=load(BASE/'source-manifest.json');old=load(OLD/'source-manifest.json')
current=read(BASE/'run-clock.ps1').decode();previous=read(OLD/'run-clock.ps1').decode()
review=load('build-ci/20261105-clock-oracle-r11-tuple-metadata-independent-review/review.json')
rp=load('build-ci/20261105-clock-oracle-r11-tuple-metadata-independent-review/source-pins.json')
for r in rp['subject']:pin(r)
ck('whole accepted R11 subject matches',review['subject_manifest_sha256']==sha(read(OLD/'source-manifest.json'))==c['predecessor_manifest']
   and review['runner_sha256']==sha(previous.encode())=='0E41A01150AE5A588D7D42176F6A6C7A6D922F8CE3AE69AD474FDA26E05C7DA9'
   and review['verdict'].startswith('ACCEPT_WHOLE_FUNCTIONAL_SOURCE_ONLY'))
ck('current exact seals',sha(current.encode())=='3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F'
   and sha(read(BASE/'source-manifest.json'))=='4C10468F6F521A754771F733E2E339B88FFCA28351D36C2C30C2F926F50CC557')
for name in ['clock_stage.cpp','source-costs.h']:
    ck('byte exact predecessor '+name,read(BASE/name)==read(OLD/name))
for r in m['sources']:pin(r,BASE/r['path'])
ck('four own exact names',sorted(r['path'] for r in m['sources'])==['README.md','clock_stage.cpp','run-clock.ps1','source-costs.h'])
ck('all1220 provisional input descriptors unchanged',m['inputs']==old['inputs'] and len(m['inputs'])==1220)
paths=[]
for r in m['inputs']:
    pin(r);paths.append(r['path'])
ck('physical identities unique with ordinal and case-fold',len(set(paths))==len(set(p.casefold() for p in paths))==1220)
ibytes=sum(r['bytes'] for r in m['inputs']);chunks=sum((r['bytes']+65535)//65536 for r in m['inputs'])
ck('whole input byte/chunk profile',ibytes==m['profile']['input_bytes']==460232026 and chunks==m['profile']['input_hash_chunks']==7849)
ck('closed namespaces unchanged',m['closed_directory_inputs']==old['closed_directory_inputs'])
entry_total=file_total=0
for spec in m['closed_directory_inputs']:
    directory=Path(spec['path']);directory=directory if directory.is_absolute() else ROOT/directory
    todo=[directory];observed=[];entries=0
    while todo:
        at=todo.pop()
        for e in sorted(os.scandir(at),key=lambda x:x.name):
            st=e.stat(follow_symlinks=False)
            ck('namespace no reparse '+e.path,not getattr(st,'st_file_attributes',0)&0x400)
            entries+=1
            if e.is_dir(follow_symlinks=False):
                if spec['recursive']:todo.append(Path(e.path))
            else:observed.append(Path(e.path).relative_to(directory).as_posix())
    ck('complete namespace '+spec['path'],entries==spec['entry_count'] and sorted(observed)==sorted(spec['files']))
    if 'file_count' in spec:ck('optional namespace file_count '+spec['path'],len(observed)==spec['file_count'])
    entry_total+=entries;file_total+=len(observed)
ck('31namespace/1092entry/1045file totals',len(m['closed_directory_inputs'])==31 and entry_total==1092 and file_total==1045)
ownbytes=sum(len(read(BASE/n)) for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json'])
ck('own five cap and actual bytes',ownbytes==666051 and ownbytes<1048576)
ck('inherited exact domain controls/environment/compiler/outputs',all(m[k]==old[k] for k in ['contract','controller','compiler','environment','references','reference_count','reference_caps_sum','dense_Q_entries','dense_Q_bytes','fraction_residues','cache_pair_payload','native_inverse_records_max','native_inverse_bytes_max','compiler_final_outputs','control_files','stage_files','module_controls','normal_gate_controls','failure_policy']))
ck('manifest all9guards false',len(m['guards'])==9 and not any(m['guards'].values()))
guardnames=['taskSourceReviewAccepted','taskSourceWorkAccepted','taskExecutionAccepted','taskCompilerAccepted','taskProduceAccepted','taskVerifyAccepted','taskCompilerPeakDiskQualified','taskProductionContextQualityRendererReady']
ck('eight source guard assignments false',all(re.search(r'\$'+n+r'=\$false\b',current) for n in guardnames))
ck('unbound data and execution retained',m['root_execution_go']=='ABSENT' and not m['pending_literal_binding']['guard_promotion']
   and m['pending_literal_binding']['physical_path'] is None and not m['cost_catalogue']['definition_files'])

replayed=previous
for i,e in enumerate(c['edits']):
    ck('exact literal edit multiplicity '+str(i),replayed.count(e['old'])==e['occurrences'])
    replayed=replayed.replace(e['old'],e['new'])
ck('old bootstrap literal occurs once',replayed.count('Assert-TaskRawJsonBytes $taskBootstrap 16019 512 4')==1)
replayed=replayed.replace('Assert-TaskRawJsonBytes $taskBootstrap 16019 512 4','Assert-TaskRawJsonBytes $taskBootstrap 16058 512 4')
ck('whole runner exactly18audited edits plus bootstrap literal',replayed==current)
diff=''.join(difflib.unified_diff(previous.splitlines(True),current.splitlines(True),fromfile='sealed-R11/run-clock.ps1',tofile='UNRUN-R12/run-clock.ps1'))
ck('author entire diff matches independent difference',read(AUTHOR/'runner.diff').decode()==diff)
(OUT/'runner-independent.diff').write_text(diff,encoding='utf-8',newline='\n')
def blocks(s):
    main=s.index('\ntry{\n # Hash literal manifest')
    matches=list(re.finditer(r'^function ([A-Za-z0-9-]+)',s[:main],re.M))
    return {r[1]:s[r.start():(matches[i+1].start() if i+1<len(matches) else main)] for i,r in enumerate(matches)}
newfunc=blocks(current);oldfunc=blocks(previous)
newnames=sorted(set(newfunc)-set(oldfunc));changed=sorted(n for n in oldfunc if newfunc[n]!=oldfunc[n]);same=sorted(n for n in oldfunc if newfunc[n]==oldfunc[n])
ck('all71helpers/new3/existing5changed/63exact',len(newfunc)==71 and len(oldfunc)==68 and len(same)==63
   and newnames==['Assert-TaskFreshHashIdentity','Get-TaskHashHex','Test-TaskHashValue']
   and changed==['Assert-TaskPins','Get-TaskBuildArtifacts','Get-TaskHash','Get-TaskReferences','Invoke-TaskFencedChild'])
pin(m['historical_R9_manifest'])
r9func=blocks(read(Path(m['historical_R9_manifest']['path']).parent/'run-clock.ps1').decode())
same9=sorted(n for n in r9func if n in newfunc and r9func[n]==newfunc[n])
changed9=sorted(n for n in r9func if n in newfunc and r9func[n]!=newfunc[n])
ck('whole inherited R9helper partition independently recomputed',same9==m['R10_payload_repairs']['helper_bodies_byte_exact_R9']
   and changed9==m['R10_payload_repairs']['changed_helper_bodies']
   and sorted(set(r9func)-set(newfunc))==[m['R10_payload_repairs']['removed_helper']]
   and sorted(set(newfunc)-set(r9func))==sorted(m['R10_payload_repairs']['added_helpers']+newnames))
ck('map-to-map identity helper byte exact',newfunc['Assert-TaskIdentity']==oldfunc['Assert-TaskIdentity'])
ck('whole hash producer identical except return and added comments',newfunc['Get-TaskHash'].split('# Internal hash carrier:')[0].rstrip()==oldfunc['Get-TaskHash'].replace('return @{bytes=$taskSeen;sha256=[Convert]::ToHexString($taskHash.GetHashAndReset())}','return [System.Tuple[object,object]]::new($taskSeen,$taskHash.GetHashAndReset())').rstrip())
hashcalls=[{'line':i,'source':line} for i,line in enumerate(current.splitlines(),1) if re.search(r'(?<!function )\bGet-TaskHash\s',line)]
freshcalls=[{'line':i,'source':line} for i,line in enumerate(current.splitlines(),1) if 'Assert-TaskFreshHashIdentity ' in line and not line.startswith('function ')]
mapcalls=[{'line':i,'source':line} for i,line in enumerate(current.splitlines(),1) if 'Assert-TaskIdentity ' in line and not line.startswith('function Assert-TaskIdentity')]
ck('16hash6fresh2descriptor map physical caller lines',len(hashcalls)==16 and len(freshcalls)==6 and len(mapcalls)==2)
ck('all old carrier field reads removed',not re.search(r'\$(?:taskH|taskMh|taskCorrupt|taskBundleHash|taskInitialBinary|taskBinaryHash)\[',current))
def controls(s):
    out=[]
    for line in s.splitlines():
        if "Expect-TaskGate '" in line:
            hit=re.search(r"Expect-TaskGate '([^']+)' .*?\}\s*'([^']+)'",line)
            ck('closed source control line decoded',hit is not None)
            out.append({'id':hit[1],'reason':hit[2]})
    return out
normal_controls=controls(current)
ck('all10normal control IDs/reasons unchanged',normal_controls==controls(previous)
   and [r['id'] for r in normal_controls]==m['normal_gate_controls']['runner'] and len(normal_controls)==10)
ck('14admitted CPPcontrol invocations unchanged',sum(len(v) for v in m['module_controls'].values())==14)
ck('five identity witness fields and exact gate',"Reject-TaskGate 'BEFORE_AFTER_COMPLETE_INPUT_IDENTITY' @{path=$Path;expected_bytes=$ExpectedBytes;observed_bytes=$Observed.Item1;expected_hash=$taskExpectedHex;observed_hash=$taskObservedHex}" in newfunc['Assert-TaskFreshHashIdentity'])
ck('untyped expected count and selected kind branch',newfunc['Assert-TaskFreshHashIdentity'].startswith('function Assert-TaskFreshHashIdentity([string]$Path,$ExpectedBytes,$ExpectedHash,$Observed)')
   and 'if($Expected -is [string])' in newfunc['Test-TaskHashValue'] and 'if($Expected -is [byte[]])' in newfunc['Test-TaskHashValue'])
ck('no caller digest field/index mutation',not re.search(r'\.Item2(?:\[[^\]]+\])?\s*(?:=|\+=|-=|\+\+|--)',current)
   and not re.search(r'\$(?:Digest|Expected)\[[^\]]+\]\s*(?:=|\+=|-=|\+\+|--)',newfunc['Test-TaskHashValue']))
for oldname in ['taskExpected','taskExpectedPath','taskExpectedBytes','taskExpectedHash']:
    if oldname=='taskExpected':
        ck('unrelated local expected controller/module data preserved','$taskExpected' in newfunc['Initialize-TaskController'] and newfunc['Initialize-TaskController']==oldfunc['Initialize-TaskController'])

# Independently recompute exact JSON original-byte admission shape.
shape={'values':0,'keys':0,'max_string_units':0,'container_depth':0,'floating_values':0}
def units(s):return len(s.encode('utf-16-le'))//2
def visit(v,depth=0):
    shape['values']+=1
    if isinstance(v,(dict,list)):
        depth+=1;shape['container_depth']=max(shape['container_depth'],depth)
    if isinstance(v,dict):
        shape['keys']+=len(v)
        for k,x in v.items():shape['max_string_units']=max(shape['max_string_units'],units(k));visit(x,depth)
    elif isinstance(v,list):
        for x in v:visit(x,depth)
    elif isinstance(v,str):shape['max_string_units']=max(shape['max_string_units'],units(v))
    elif isinstance(v,float):shape['floating_values']+=1
visit(m);shape['nodes_and_keys']=shape['values']+shape['keys']
ck('exact bootstrap9967values6091keys16058visits435unitsdepth4',shape==c['bootstrap_shape']==m['R10_payload_repairs']['bootstrap_observed']
   and shape=={'values':9967,'keys':6091,'max_string_units':435,'container_depth':4,'floating_values':0,'nodes_and_keys':16058})
ck('exact caller profile16058/512/4','Assert-TaskRawJsonBytes $taskBootstrap 16058 512 4' in current)
ck('provisional identities/capacities unchanged',m['profile']['complete_current_inputs']==1220+5==1225
   and m['profile']['maximum_complete_fence_identities']==1225+68+1==1294 and 65*1294==84110 and 2*1220+3*74==2662)
ownmax=max(units(str((BASE/n).resolve())) for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json'])
allmax=max([ownmax]+[units(str((Path(p) if Path(p).is_absolute() else ROOT/p).resolve())) for p in paths])
ck('current own/all physical path maxima',ownmax==c['own_path_max']==176 and allmax==c['current_identity_max']==270)
for row in c['source_rows']:
    b=read(BASE/row['path']);ck('source line/byte/hash '+row['path'],len(b)==row['bytes'] and sha(b)==row['sha256'] and len(b.decode().splitlines())==row['lines'])
for name in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json']:
    b=read(BASE/name);ck('UTF8 LF '+name,not b.startswith(b'\xef\xbb\xbf') and b'\r' not in b)

# Algebra of new per-byte hex comparison; never invoke the PowerShell helper.
alphabet='0123456789ABCDEF'
for byte in range(256):
    hx=alphabet[byte>>4]+alphabet[byte&15]
    ck('hex nibble bijection '+str(byte),hx==format(byte,'02X') and int(hx,16)==byte)
ck('uppercase alphabet has16unique code units',len(alphabet)==len(set(alphabet))==16 and all(ord(x)<128 for x in alphabet))
for n in ['build-ci/20261105-clock-oracle-r11-tuple-metadata-independent-review/primary-sources/Binders.cs',
          'build-ci/20261105-clock-oracle-r11-tuple-metadata-independent-review/primary-sources/LanguagePrimitives.cs',
          'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/Tuple.cs',
          'build-ci/20261105-clock-oracle-r11-hash-carrier-source-audit-r1/IncrementalHash.cs']:
    read(n)
tuple_source=read('build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/Tuple.cs').decode()
decl=re.search(r'public class Tuple<T1, T2>\s*:\s*([^\n]+)',tuple_source)[1]
ck('Tuple2 source no enumerable interfaces','IEnumerable' not in decl and 'IEnumerator' not in decl and 'ITuple' in decl)
ck('Tuple2 direct readonly cells and getters',all(x in tuple_source for x in ['private readonly T1 m_Item1;','private readonly T2 m_Item2;','public T1 Item1 => m_Item1;','public T2 Item2 => m_Item2;']))
crypto=read('build-ci/20261105-clock-oracle-r11-hash-carrier-source-audit-r1/IncrementalHash.cs').decode()
ck('fresh digest array primary source','public byte[] GetHashAndReset()' in crypto and 'byte[] ret = new byte[HashLengthInBytes];' in crypto)
inventory=load('build-ci/20261105-managed-file-type-inventory-source-r3/execution-r1/inventory.json')
ck('separate metadata result exact seal',sha(read('build-ci/20261105-managed-file-type-inventory-source-r3/execution-r1/inventory.json'))=='B015613B9A2EA1B844547C0135106C0977D8A99A4C4E4E52B22B0FBFE7EAEC9D')
receipt=load('build-ci/20261105-managed-file-type-inventory-source-r3/execution-r1/receipt.json')
ck('separate metadata receipt exact seal',sha(read('build-ci/20261105-managed-file-type-inventory-source-r3/execution-r1/receipt.json'))=='428181A357E23CB3FDEA710F6D7C4F9D0311CF932923928FA65E8FA0EF0B20AE')
ts=[t for t in inventory['types'] if t['qualified_name']=='System.Tuple`2']
ck('Tuple2 declared readonly generic pair file shape',len(ts)==1 and ts[0]['layout_kind']=='AutoLayout' and ts[0]['base_decoded']=='[System.Private.CoreLib]System.Object'
   and [(x['name'],x['decoded_signature'],x['attributes']) for x in ts[0]['declared_nonstatic_instance_fields']]==[('m_Item1','!0(T1)','Private, InitOnly'),('m_Item2','!1(T2)','Private, InitOnly')])

def difference(a,b,p=''):
    if type(a)!=type(b):return [p]
    if isinstance(a,dict):
        out=[]
        for k in sorted(set(a)|set(b)):
            out+=([p+'/'+k] if k not in a or k not in b else difference(a[k],b[k],p+'/'+k))
        return out
    if isinstance(a,list):
        if len(a)!=len(b):return [p]
        return [x for i,(aa,bb) in enumerate(zip(a,b)) for x in difference(aa,bb,p+'/'+str(i))]
    return [] if a==b else [p]
changes=difference(old,m)
result={'schema':'R12_WHOLE_FUNCTIONAL_FILE_DATA_CHECKS_R1','checks_passed':len(checks),'checks':checks,'verified_files':list(files.values()),
 'helpers':{'count':71,'predecessor':68,'new':newnames,'changed_existing':changed,'byte_exact_existing':same},
 'hash_caller_lines':hashcalls,'fresh_identity_caller_lines':freshcalls,'descriptor_identity_caller_lines':mapcalls,'normal_controls':normal_controls,
 'bootstrap':shape,'profile':{'inputs':1220,'bytes':ibytes,'chunks':chunks,'namespaces':31,'namespace_entries':entry_total,'namespace_files':file_total,'own_five_bytes':ownbytes},
 'manifest_changed_paths':changes,'Tuple2_metadata_shape':ts[0],'all_target_execution_prohibited_and_not_performed':True}
(OUT/'file-checks.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'checks':len(checks),'verified_files':len(files),'helper_count':len(newfunc),'exact_existing_helpers':len(same),'hash_calls':len(hashcalls),'profile':result['profile'],'bootstrap':shape,'manifest_changes':changes}))
