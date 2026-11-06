"""R13 UNRUN source authoring and file/JSON arithmetic checks only.

Does not import, parse or invoke PowerShell, any target helper or a compiler.
The first passing result is sealed; subsequent executions refuse to overwrite it.
"""
from pathlib import Path
import copy
import difflib
import hashlib
import json
import re

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
OLD = ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r12-hash-carrier-source-r1'
NEW = ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
SEAL = OUT/'source-pins.json'
if SEAL.exists() or NEW.exists():
    raise RuntimeError('Preserve first passing seal and every prior source attempt; do not overwrite.')
checks = []

def digest(b): return hashlib.sha256(b).hexdigest().upper()
def pin(p):
    b=p.read_bytes()
    return dict(path=p.relative_to(ROOT).as_posix(),bytes=len(b),sha256=digest(b))
def ck(name,a,b=True):
    def safe(x):
        if isinstance(x,bytes):return dict(bytes=len(x),sha256=digest(x))
        if isinstance(x,set):return sorted(x)
        return x
    checks.append(dict(name=name,actual=safe(a),expected=safe(b),passed=a==b))
    if a!=b: raise AssertionError(name)

old_manifest_raw=(OLD/'source-manifest.json').read_bytes()
old_runner_raw=(OLD/'run-clock.ps1').read_bytes()
ck('R12 manifest seal',digest(old_manifest_raw),'4C10468F6F521A754771F733E2E339B88FFCA28351D36C2C30C2F926F50CC557')
ck('R12 runner seal',digest(old_runner_raw),'3D9F23D675A9920969B6551A244BD6F07F144AE25440CA6325FAF0D4DB44319F')
old_m=json.loads(old_manifest_raw)
original=old_runner_raw.decode('utf-8')
runner=original
edits=[]

def replace_once(old,new):
    global runner
    ck('one exact source edit '+old[:65],runner.count(old),1)
    runner=runner.replace(old,new,1)
    edits.append(dict(old=old,new=new,occurrences=1))

def replace_helper(name, transform):
    global runner
    pattern=re.compile(r'^function '+re.escape(name)+r'(?=\(|\{).*?(?=^function |\Z)',re.M|re.S)
    match=pattern.search(runner)
    ck('helper anchor '+name,match is not None)
    old=match.group();new=transform(old)
    ck('changed helper '+name,old!=new)
    runner=runner[:match.start()]+new+runner[match.end():]
    edits.append(dict(helper=name,old=old,new=new,occurrences=1))

def index_state(s):
    for name,index in [('cap',0),('nodes',1),('bytes',2)]:
        s=s.replace("$State['"+name+"']",'$State['+str(index)+']')
    s=s.replace('($State,','([long[]]$State,')
    s=s.replace('[int]$Depth,$State)','[int]$Depth,[long[]]$State)')
    return s

replace_helper('Add-TaskJsonBound',index_state)
replace_helper('Visit-TaskJsonValue',index_state)

def measure(s):
    s=s.replace('$taskState=@{cap=$Maximum;nodes=0L;bytes=0L}',
        '# Private fixed state: cap,nodes,bytes are unboxed signed Int64 cells.\n $taskState=[long[]]::new(3);$taskState[0]=$Maximum')
    for name,index in [('cap',0),('nodes',1),('bytes',2)]:
        s=s.replace("$taskState['"+name+"']",'$taskState['+str(index)+']')
    return s
replace_helper('Measure-TaskJson',measure)

new_helpers='''function Close-TaskJsonReader($Owner){
 # The dependency reader alone owns the three borrowed-buffer parser wrappers.
 # Captured-byte SHA/raw grammar/compaction and visit admission precede its call.
 if($null -ne $Owner){try{$Owner.Close()}catch{if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_}else{Set-TaskSecondary 10 $_}}}
}
function Read-TaskDependencyJson([byte[]]$Bytes,[int]$Length){
 $taskMemory=$null;$taskTextReader=$null;$taskJsonReader=$null;$taskBefore=$script:taskPrimaryRaw;$taskBody=$null
 try{
  $taskMemory=[IO.MemoryStream]::new($Bytes,0,$Length,$false,$false)
  $taskTextReader=[IO.StreamReader]::new($taskMemory,[Text.UTF8Encoding]::new($false,$true),$false,1024,$true)
  $taskJsonReader=[Newtonsoft.Json.JsonTextReader]::new($taskTextReader);$taskJsonReader.CloseInput=$false
  $taskSettings=[Newtonsoft.Json.JsonSerializerSettings]::new()
  $taskSettings.DateParseHandling=[Newtonsoft.Json.DateParseHandling]::None
  $taskSettings.DateTimeZoneHandling=[Newtonsoft.Json.DateTimeZoneHandling]::RoundtripKind
  $taskSettings.TypeNameHandling=[Newtonsoft.Json.TypeNameHandling]::None
  $taskSettings.MetadataPropertyHandling=[Newtonsoft.Json.MetadataPropertyHandling]::Ignore
  $taskSettings.MaxDepth=24;$taskSettings.CheckAdditionalContent=$true
  $taskSerializer=[Newtonsoft.Json.JsonSerializer]::CreateDefault($taskSettings)
  # Unary comma preserves the single parser result across the function pipeline.
  # The consumer checks exact JObject root/Data types before selecting any field.
  return ,($taskSerializer.Deserialize($taskJsonReader))
 }catch{$taskBody=$_;if($null -eq $script:taskPrimaryRaw){$script:taskPrimaryRaw=$_};throw}
 finally{Close-TaskJsonReader $taskJsonReader;Close-TaskOwner $taskTextReader;Close-TaskOwner $taskMemory;Complete-TaskOwnerClose $taskBefore $taskBody}
}
function Get-TaskDependencyString($Token){
 # A JToken is never stringified or allowed to become a wire value.
 $taskValue=$null
 if((Get-TaskRuntimeType $Token) -eq [Newtonsoft.Json.Linq.JValue] -and $Token.get_Type() -eq [Newtonsoft.Json.Linq.JTokenType]::String){$taskValue=$Token.get_Value()}
 Assert-TaskString $taskValue 512
 return $taskValue
}
'''
replace_once('function Read-TaskJson(',new_helpers+'function Read-TaskJson(')
replace_once(" if($Maximum -eq 4194304){$taskCompactLength=Assert-TaskRawJsonBytes $taskBytes (2*[int]$taskManifest['profile']['include_input_count']+256) 512 24}else{$taskCompactLength=Assert-TaskRawJsonBytes $taskBytes 2420 270 3}",
             " if($Maximum -eq 4194304){$taskCompactLength=Assert-TaskRawJsonBytes $taskBytes (2*[int]$taskManifest['profile']['include_input_count']+256) 512 24;return ,(Read-TaskDependencyJson $taskBytes $taskCompactLength)}else{$taskCompactLength=Assert-TaskRawJsonBytes $taskBytes 2420 270 3}")

def dependencies(s):
    prefix=s[:s.index(" if((Get-TaskRuntimeType $taskD)")]
    return prefix+'''  if((Get-TaskRuntimeType $taskD) -ne [Newtonsoft.Json.Linq.JObject]){throw 'DEPENDENCY_ROOT_MAP'}
  $taskData=$taskD.GetValue('Data',[StringComparison]::Ordinal);if((Get-TaskRuntimeType $taskData) -ne [Newtonsoft.Json.Linq.JObject]){throw 'DEPENDENCY_DATA_MAP'}
  $taskSource=Get-TaskDependencyString ($taskData.GetValue('Source',[StringComparison]::Ordinal))
  if((Resolve-TaskInput $taskSource) -ine (Join-Path $taskBase 'clock_stage.cpp')){throw 'Compiler source-dependency source binding'}
  foreach($taskKey in @('ImportedModules','ImportedHeaderUnits')){
   $taskOptional=$taskData.GetValue($taskKey,[StringComparison]::Ordinal)
   if($null -ne $taskOptional){if((Get-TaskRuntimeType $taskOptional) -ne [Newtonsoft.Json.Linq.JArray] -or $taskOptional.get_Count() -ne 0){throw 'Unqualified compiler module/header-unit dependency'}}
  }
  $taskIncludes=$taskData.GetValue('Includes',[StringComparison]::Ordinal)
  if((Get-TaskRuntimeType $taskIncludes) -ne [Newtonsoft.Json.Linq.JArray] -or $taskIncludes.get_Count() -lt 1 -or $taskIncludes.get_Count() -gt $taskManifest['profile']['include_input_count']){throw 'Compiler dependency Includes finite profile'}
  foreach($taskToken in $taskIncludes){$taskPath=Get-TaskDependencyString $taskToken;if(-not $taskIncludeIndex.ContainsKey((Resolve-TaskInput $taskPath))){throw 'Source-dependency include outside pre-fenced conservative inventory'}}
  # Only this source-built summary is retained; no arbitrary parsed field escapes.
  return @{includes=$taskIncludes.get_Count();report_is_additional_artifact_only=$true;complete_tool_inventory_replaced=$false}
 }
'''
replace_helper('Assert-TaskDependencies',dependencies)

# Authored metadata only. Existing inputs,SDK/domain/control/wire descriptions
# remain provisional; no physical resource or price credit is inferred.
m=copy.deepcopy(old_m)
m['schema']='YESDAW_CLOCK_ORACLE_R13_STREAMED_DEPENDENCY_SOURCE_PROTOTYPE_UNBOUND'
m['profile']['profile_source']='PROVISIONAL_EXISTING_ADMISSION_FOR_UNRUN_R13_FUNCTIONAL_SOURCE_PREPARATION; final functional union/root/env/census/resources require fresh recomposition before authority.'
m['closed_json_domain']['raw_preparse']='Captured identities precede raw UTF8 byte grammar and in-place compaction. Dependency reports then use borrowed compact-prefix MemoryStream/strict StreamReader/JsonTextReader to JObject; predecessor/bootstrap and child string routes remain. New complete census required.'
m['closed_json_domain']['projection']='Exact dependency JObject/Data/JArray shapes and JValue String payloads are selected ordinally; unused parsed fields stay private. Existing predecessor/child field sets/types/IDs/reasons and source-built wire maps remain unchanged. No JToken is a wire value.'
m['R13_streamed_dependency']={
 'state':'SOURCE_ONLY_UNRUN_ALL_GUARDS_FALSE_PROVISIONAL_ADMISSION_NO_PRICE_RESOURCE_OR_GO_TRANSFER',
 'predecessor_source_manifest_sha256':digest(old_manifest_raw),
 'measurement_state':'Private long[3]: cap,nodes,bytes. Only measurement helper/caller state accesses change; measurement DTO and counters retained.',
 'dependency_route':'Captured SHA,raw byte grammar,compaction,visit charge;borrowed byte[] MemoryStream(0,length,false,false),strict UTF8 StreamReader(false,1024,true),JsonTextReader CloseInput=false,JsonSerializer.CreateDefault(settings).',
 'settings':['DateParseHandling.None','DateTimeZoneHandling.RoundtripKind','TypeNameHandling.None','MetadataPropertyHandling.Ignore','MaxDepth24','CheckAdditionalContent=true'],
 'string_date_semantics':'Date parsing is None,so the explicit RoundtripKind setting does not convert date-shaped strings. Baseline DateKind.String likewise disables date parsing.',
 'consumer':'Exact JObject root/Data,JArray Includes and optional empty JArrays;GetValue Ordinal,get_Type/get_Value/get_Count;only actual builtin String JValue payloads enter original path checks.',
 'last_duplicate_semantics':'Same exact Ordinal Json.NET JObject replacement semantics;case-distinct names remain distinct. Missing/null/nonstring shapes retain the named existing refusals.',
 'owners':'Read helper closes JsonTextReader via.Close(),then StreamReader and MemoryStream via existing Dispose wrapper;CloseInput=false/leaveOpen=true;prior/body/secondary policy preserved.',
 'new_helpers':['Close-TaskJsonReader','Read-TaskDependencyJson','Get-TaskDependencyString'],
 'changed_helpers':['Add-TaskJsonBound','Visit-TaskJsonValue','Measure-TaskJson','Read-TaskJson','Assert-TaskDependencies'],
 'map_enumeration_and_key_boxing_optimized':False,
 'predecessor_bootstrap_Cpp_header_domain_controls_wire_preserved':True,
 'complete_scalar_capacity_phase_error_ABI_census':'PENDING;no claimed net reduction,32MiB fit,physical free,source price or target execution.',
 'canonical_union_root_environment_binding':'PENDING;R12 provisional input union is retained only for functional source preparation.'}
doc=(OLD/'README.md').read_text(encoding='utf-8')
doc=doc.replace('# ClockOracle R12 bounded byte admission and approved path strings','# ClockOracle R13 streamed dependency and private measurement state',1)
doc+='''
## R13 streamed dependency and private measurement state, source revision 1

UNRUN. All source/compilation/production/execution guards remain false;Data and
prices remain undefined. The unchanged R12 packet is the complete predecessor.

Measure-TaskJson now owns one long[3] for cap,nodes,bytes. Its helper/caller
accesses use indices0/1/2;the output measurement map,guards,order and global
counters remain. No GetEnumerator/key optimization is included: map/key boxing,
preprocess clone,writer capacities and error/phase lifetimes still need full proof.

Only the4MiB dependency-report branch replaces compacted full-string decoding and
ConvertFrom-Json's OrderedHashtable/object[] projection. The original captured
SHA,raw UTF8 grammar,exact visit charge and outside-string in-place compaction
remain before a borrowed compact-prefix MemoryStream. Strict UTF8 StreamReader
uses detect-BOM=false,buffer1024,leaveOpen=true;JsonTextReader has CloseInput=false.
JsonSerializer.CreateDefault receives DateParseNone,RoundtripKind,TypeNameNone,
MetadataIgnore,MaxDepth24 and CheckAdditionalContent=true. With date parsing off,
RoundtripKind does not convert date-shaped string values. Baseline JsonConvert
also forces additional-content checking. No new serializer callback is installed.

Dependency root/Data must be exact JObject. Includes must be exact JArray with
the original1..include_input_count extent;optional module/header arrays must be
absent or exact empty JArrays. GetValue(name,Ordinal) preserves last exact duplicate
replacement and case-distinct keys. Public CLR accessor methods avoid property
shadowing by parsed names. Only exact JValue String tokens with builtin string
Value pass the original bounded-string/path/source/include checks. Existing named
shape/domain/refusal reasons and the three-field summary remain. Unused parsed
fields are accepted privately as before;no JObject,JArray,JToken or raw arbitrary
field enters a wire graph. Unary comma preserves a single parser result across
both helper returns before the consumer's root check.

Read-TaskDependencyJson owns JsonTextReader,StreamReader and MemoryStream in that
order of cleanup. The new Close-TaskJsonReader uses public.Close() with the same
primary/secondary raw policy;the existing owner helper disposes text/memory,then
Complete-TaskOwnerClose runs after all three. CloseInput/leaveOpen prevent automatic
double close. Constructor,parse and cleanup failures retain every reached owner.
No null/scope/Dispose/ReturnBuffer operation is treated as physical-free evidence.

Predecessor/bootstrap routes,raw grammars and in-place byte compaction are retained;
the only main-body edit is the freshly derived bootstrap visit literal. All C++,
source-cost header,old provisional inputs/SDK/domain/control IDs/reasons/witnesses,
wire names/types and caps remain. This is source authoring,not a target parse or
runtime test. The new74-helper graph needs independent whole functional review,
fresh complete scalar/buffer/clone/container/error/lifetime census,canonical
functional/proof/Data union and exact root/environment/resource/native binding,
then separate root GO. No R12 numeric price,wire bound,root geometry or fit transfers.
'''

def shape(v,depth=0):
    # values,keys,max container depth,max UTF16 units,floats
    if isinstance(v,dict):
        children=[shape(x,depth+1) for x in v.values()]
        return [1+sum(x[0] for x in children),len(v)+sum(x[1] for x in children),max([depth]+[x[2] for x in children]),max([0]+[len(k.encode('utf-16-le'))//2 for k in v]+[x[3] for x in children]),sum(x[4] for x in children)]
    if isinstance(v,list):
        children=[shape(x,depth+1) for x in v]
        return [1+sum(x[0] for x in children),sum(x[1] for x in children),max([depth]+[x[2] for x in children]),max([0]+[x[3] for x in children]),sum(x[4] for x in children)]
    return [1,0,0,len(v.encode('utf-16-le'))//2 if isinstance(v,str) else 0,int(isinstance(v,float))]

NEW.mkdir(parents=True)
for name in ['clock_stage.cpp','source-costs.h']:(NEW/name).write_bytes((OLD/name).read_bytes())
for attempt in range(10):
    s=shape(m);visits=s[0]+s[1]
    runner=re.sub(r'Assert-TaskRawJsonBytes \$taskBootstrap [0-9]+ 512 4',f'Assert-TaskRawJsonBytes $taskBootstrap {visits} 512 4',runner)
    m['R10_payload_repairs']['raw_profiles']['bootstrap']['nodes_and_keys']=visits
    m['R10_payload_repairs']['raw_profiles']['bootstrap']['container_depth']=s[2]
    m['R10_payload_repairs']['bootstrap_observed']=dict(values=s[0],keys=s[1],max_string_units=s[3],container_depth=s[2],floating_values=s[4],nodes_and_keys=visits)
    own_max=max(len(str(NEW/name).replace('/','\\')) for name in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json'])
    doc=re.sub(r'Final source-profile arithmetic: bootstrap.*?caller uses [0-9]+/512/[0-9]+\.',f'Final source-profile arithmetic: bootstrap {s[0]} values + {s[1]} keys\n= {visits} node/key visits; maximum string {s[3]} UTF16 units,\ncontainer depth {s[2]}, no floating JSON values. The exact PS byte-helper\ncaller uses {visits}/512/{s[2]}.',doc,flags=re.S)
    doc=re.sub(r'current own5 maximum is[0-9]+ \(provisional R12 base\)',f'current own5 maximum is{own_max} (provisional R13 base)',doc)
    (NEW/'run-clock.ps1').write_text(runner,encoding='utf-8',newline='\n')
    (NEW/'README.md').write_text(doc,encoding='utf-8',newline='\n')
    for row in m['sources']:
        p=NEW/row['path'];b=p.read_bytes();row.update(bytes=len(b),lines=len(b.splitlines()),sha256=digest(b))
    before=json.dumps(m,indent=2)+'\n'
    (NEW/'source-manifest.json').write_text(before,encoding='utf-8',newline='\n')
    if shape(m)==s and all(m['sources'][i]['sha256']==pin(NEW/m['sources'][i]['path'])['sha256'] for i in range(4)):
        break
else:raise AssertionError('metadata/bootstrap fixed point')

def functions(t):
    # Literal function-boundary text comparison only, not target parsing.
    matches=list(re.finditer(r'^function ([A-Za-z0-9-]+)(?=\(|\{)',t,re.M))
    main=t.index('\ntry{\n # Hash literal manifest')
    return {match.group(1):t[match.start():(matches[i+1].start() if i+1<len(matches) else main)] for i,match in enumerate(matches)}

of=functions(original);nf=functions(runner)
changed=[name for name in of if of[name]!=nf[name]]
added=[name for name in nf if name not in of]
ck('five changed helper set',set(changed),set(m['R13_streamed_dependency']['changed_helpers']))
ck('three added helper set',set(added),set(m['R13_streamed_dependency']['new_helpers']))
ck('74 helpers',len(nf),74)
ck('71 predecessor helpers',len(of),71)
ck('66 exact old helpers',sum(of[n]==nf[n] for n in of),66)
old_main=original[original.index('\ntry{\n # Hash literal manifest'):]
new_main=runner[runner.index('\ntry{\n # Hash literal manifest'):]
ck('main only bootstrap literal',re.sub(r'Assert-TaskRawJsonBytes \$taskBootstrap [0-9]+ 512 4','BOOTSTRAP',old_main),re.sub(r'Assert-TaskRawJsonBytes \$taskBootstrap [0-9]+ 512 4','BOOTSTRAP',new_main))
ck('unchanged pre-helper globals',original[:original.index('function ')],runner[:runner.index('function ')])
for name in ['clock_stage.cpp','source-costs.h']:ck('protected exact '+name,(NEW/name).read_bytes(),(OLD/name).read_bytes())
for key in ['inputs','closed_directory_inputs','current_include_inputs','references','compiler_final_outputs','control_files','stage_files','normal_gate_controls','module_controls','guards','cost_catalogue','controller','compiler','environment']:
    ck('preserved manifest '+key,m[key],old_m[key])
for key in m['profile']:
    if key!='profile_source':ck('preserved provisional profile '+key,m['profile'][key],old_m['profile'][key])
ck('all guards false',any(m['guards'].values()),False)
ck('no Data',m['cost_catalogue']['definition_files'],[])
ck('same raw grammar bytes',nf['Assert-TaskRawJsonBytes'],of['Assert-TaskRawJsonBytes'])
ck('predecessor decoder tail byte exact',nf['Read-TaskJson'][nf['Read-TaskJson'].index(' $taskRaw='):],of['Read-TaskJson'][of['Read-TaskJson'].index(' $taskRaw='):])
ck('preflight enumeration unchanged',nf['Visit-TaskJsonValue'].count('foreach($taskEntry in $Value.GetEnumerator())'),1)
ck('no old named state access',any(x in nf['Add-TaskJsonBound']+nf['Visit-TaskJsonValue']+nf['Measure-TaskJson'] for x in ["$State['cap']","$State['nodes']","$State['bytes']","$taskState['nodes']","$taskState['bytes']"]),False)
ck('one measurement long array',nf['Measure-TaskJson'].count('[long[]]::new(3)'),1)
ck('borrowed memory constructor',nf['Read-TaskDependencyJson'].count('[IO.MemoryStream]::new($Bytes,0,$Length,$false,$false)'),1)
ck('strict streaming constructor',nf['Read-TaskDependencyJson'].count('[IO.StreamReader]::new($taskMemory,[Text.UTF8Encoding]::new($false,$true),$false,1024,$true)'),1)
ck('single parser call',nf['Read-TaskDependencyJson'].count('return ,($taskSerializer.Deserialize($taskJsonReader))'),1)
ck('extra content explicit',nf['Read-TaskDependencyJson'].count('$taskSettings.CheckAdditionalContent=$true'),1)
ck('both parser-result boundaries unary comma','return ,(Read-TaskDependencyJson $taskBytes $taskCompactLength)' in nf['Read-TaskJson'])
ck('reader complete close order','finally{Close-TaskJsonReader $taskJsonReader;Close-TaskOwner $taskTextReader;Close-TaskOwner $taskMemory;Complete-TaskOwnerClose $taskBefore $taskBody}' in nf['Read-TaskDependencyJson'])
ck('bootstrap exact shape',shape(m),s)
ck('bootstrap no floats',s[4],0)
ck('bootstrap depth4',s[2],4)
ck('bootstrap string domain',s[3]<=512)
ck('source5 admission',sum(p.stat().st_size for p in NEW.iterdir())<=1048576)
for row in m['sources']:
    ck('source row '+row['path'],row['sha256'],pin(NEW/row['path'])['sha256'])
for r in m['inputs']:
    p=Path(r['path'])
    if not p.is_absolute():p=ROOT/p
    b=p.read_bytes();ck('preserved input '+r['path'],dict(bytes=len(b),sha256=digest(b)),dict(bytes=r['bytes'],sha256=r['sha256']))
for name in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json']:
    ck('LF/noBOM '+name,b'\r' not in (NEW/name).read_bytes() and not (NEW/name).read_bytes().startswith(b'\xef\xbb\xbf'))

(OUT/'runner.diff').write_text(''.join(difflib.unified_diff(original.splitlines(True),runner.splitlines(True),fromfile='R12/run-clock.ps1',tofile='R13/run-clock.ps1')),encoding='utf-8',newline='\n')
correspondence=dict(schema='R13_UNRUN_STREAMED_DEPENDENCY_COMPLETE_SOURCE_CORRESPONDENCE_V1',status='AUTHOR_CHECKS_ONLY_PENDING_INDEPENDENT_WHOLE_FUNCTIONAL_REVIEW',predecessor_manifest_sha256=digest(old_manifest_raw),manifest_sha256=pin(NEW/'source-manifest.json')['sha256'],source_rows=m['sources'],bootstrap_shape=m['R10_payload_repairs']['bootstrap_observed'],helper_count=74,unchanged_helpers=[n for n in of if of[n]==nf[n]],changed_helpers=changed,added_helpers=added,own_path_max=own_max,own5_bytes=sum(p.stat().st_size for p in NEW.iterdir()),provisional_input_count=len(m['inputs']),input_bytes=sum(x['bytes'] for x in m['inputs']),edits=edits,checks=checks,no_target_execution=True,no_price_resource_fit_binding_GO=True)
(OUT/'source-correspondence.json').write_text(json.dumps(correspondence,indent=2)+'\n',encoding='utf-8',newline='\n')
inputs=[pin(OLD/n) for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json']]
inputs += [pin(ROOT/p) for p in ['build-ci/20261105-clock-oracle-r12-hash-carrier-independent-review/review.json','build-ci/20261105-clock-oracle-r12-hash-carrier-source-authoring-r1/source-pins.json','build-ci/20261105-controller-environment-writer-closure-r2/primary/src/System.Management.Automation/engine/CoreAdapter.cs','build-ci/20261105-controller-environment-writer-closure-r2/primary/src/Microsoft.PowerShell.Commands.Utility/commands/utility/WebCmdlet/JsonObject.cs','build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JObject.cs','build-ci/20261106-clock-r12-serializer-runtime-source-r1/primary/JsonSerializer.cs','build-ci/20261105-clock-oracle-json-primary-sources/newtonsoft-JsonConvert.cs']]
seal=dict(schema='R13_UNRUN_STREAMED_DEPENDENCY_AUTHOR_SOURCE_PINS_V1',status='FIRST_PASSING_OWN_FILE_CHECKS_FROZEN_PENDING_INDEPENDENT_WHOLE_FUNCTIONAL_REVIEW',subject=[pin(NEW/n) for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json']],author_evidence=[pin(p) for p in sorted(OUT.iterdir()) if p.is_file() and p!=SEAL],inputs=inputs,execution='Only author-owned Python text/file/JSON/hash/integer checks. No PowerShell parser/AST/import/helper,compiler,program,stage,native probe,GUI,Git or CI.',authority='Root-authorized R13 UNRUN allocation-source repair only. All guards false,Data undefined,no old numeric/resource/root/GO transfer.',source_profile='Same provisional R12 input/SDK/domain/control/wire set;canonical functional/proof/resource/census/Data union remains pending.',checks=len(checks))
SEAL.write_text(json.dumps(seal,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(seals=dict(pins=pin(SEAL),correspondence=pin(OUT/'source-correspondence.json')),subject=seal['subject'],helper_count=74,bootstrap_shape=correspondence['bootstrap_shape'],own5_bytes=correspondence['own5_bytes'],checks=len(checks)),indent=2))
