"""Own independent R13 file/text/hash/JSON/integer check, not a target parser."""
from pathlib import Path
import hashlib
import json
import re
import difflib

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
BASE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions'
OLD=BASE/'clock-oracle-r12-hash-carrier-source-r1'
NEW=BASE/'clock-oracle-r13-streamed-dependency-source-r1'
AUTHOR=ROOT/'build-ci/20261106-clock-r13-source-authoring-r1'
checks=[]
inputs={}

def sha(b): return hashlib.sha256(b).hexdigest().upper()
def check(name,actual,expected=True):
    ok=actual==expected
    checks.append(dict(name=name,passed=ok))
    if not ok: raise ValueError(name)
def read(p):
    b=p.read_bytes()
    inputs[str(p.relative_to(ROOT)).replace('\\','/')]=dict(path=str(p.relative_to(ROOT)).replace('\\','/'),bytes=len(b),sha256=sha(b))
    return b
def txt(p):return read(p).decode('utf-8-sig')
def require(p,fragment):check(str(p.relative_to(ROOT))+':'+fragment[:70],fragment in txt(p))
def functions(s):
    # Line anchors identify written source partitions only, never PS AST.
    end=s.index('\ntry{\n # Hash literal manifest')
    found=list(re.finditer(r'^function ([A-Za-z0-9-]+)(?=\(|\{)',s[:end],re.M))
    return {m[1]:s[m.start():found[i+1].start() if i+1<len(found) else end] for i,m in enumerate(found)}

ap=read(AUTHOR/'source-pins.json')
check('exact author pins',sha(ap),'C0910CCA2D0B8AE0ABCEE842B18F7F1BD7E4B42A188309693F8A9F39BFB089BB')
for section in ['subject','author_evidence','inputs']:
    for row in json.loads(ap)[section]:
        b=read(ROOT/row['path'])
        check('author pin '+row['path'],(len(b),sha(b)),(row['bytes'],row['sha256']))
old=txt(OLD/'run-clock.ps1');new=txt(NEW/'run-clock.ps1')
check('exact runner',sha(new.encode()),'9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788')
om=json.loads(read(OLD/'source-manifest.json'));nm=json.loads(read(NEW/'source-manifest.json'))
check('exact manifest',sha(read(NEW/'source-manifest.json')),'10310CC3D798E7BFFFFA99A9438121FBAD56CDA3CFEB41353FF39AD333A9DED3')
of,nf=functions(old),functions(new)
check('predecessor 71',len(of),71);check('successor 74',len(nf),74)
changed={n for n in of if of[n]!=nf[n]}
added=set(nf)-set(of)
check('five changed bodies',changed,{'Add-TaskJsonBound','Visit-TaskJsonValue','Measure-TaskJson','Read-TaskJson','Assert-TaskDependencies'})
check('three new bodies',added,{'Close-TaskJsonReader','Read-TaskDependencyJson','Get-TaskDependencyString'})
check('all predecessor names retained',set(of)-set(nf),set())
for n in set(of)-changed:check('unchanged complete helper '+n,nf[n],of[n])
for n in ['Add-TaskJsonBound','Visit-TaskJsonValue','Measure-TaskJson']:
    recovered=nf[n]
    for label,index in [('cap',0),('nodes',1),('bytes',2)]:
        recovered=recovered.replace('$State['+str(index)+']',"$State['"+label+"']").replace('$taskState['+str(index)+']',"$taskState['"+label+"']")
    recovered=recovered.replace('([long[]]$State,','($State,').replace('[int]$Depth,[long[]]$State)','[int]$Depth,$State)')
    recovered=recovered.replace("# Private fixed state: cap,nodes,bytes are unboxed signed Int64 cells.\n $taskState=[long[]]::new(3);$taskState['cap']=$Maximum", '$taskState=@{cap=$Maximum;nodes=0L;bytes=0L}')
    check('whole state isomorphism '+n,recovered,of[n])
old_main=old[old.index('\ntry{\n # Hash literal manifest'):];new_main=new[new.index('\ntry{\n # Hash literal manifest'):]
normal=lambda t:re.sub(r'Assert-TaskRawJsonBytes \$taskBootstrap \d+ 512 4','BOOTSTRAP_LITERAL',t)
check('whole main only bootstrap literal',normal(new_main),normal(old_main))
check('whole globals exact',new[:new.index('function ')],old[:old.index('function ')])
for n in ['clock_stage.cpp','source-costs.h']:check('complete exact native source '+n,read(NEW/n),read(OLD/n))
for name in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json']:
    b=read(NEW/name);check('LF no BOM '+name,b'\r' not in b and not b.startswith(b'\xef\xbb\xbf'))
for row in nm['sources']:
    b=read(NEW/row['path']);check('actual own source descriptor '+row['path'],(len(b),len(b.splitlines()),sha(b)),(row['bytes'],row['lines'],row['sha256']))
check('fresh own5 bytes',sum(len(read(NEW/n)) for n in ['clock_stage.cpp','source-costs.h','run-clock.ps1','README.md','source-manifest.json']),674220)
check('own five admitted',674220<=1048576)
diff=''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile='R12/run-clock.ps1',tofile='R13/run-clock.ps1'))
check('full exact diff',diff,txt(AUTHOR/'runner.diff'))
for key in om:
    if key not in ['schema','profile','closed_json_domain','sources','R10_payload_repairs']:
        check('complete unchanged manifest field '+key,nm[key],om[key])
check('only expected new manifest field',set(nm)-set(om),{'R13_streamed_dependency'})
for key in om['profile']:
    if key!='profile_source':check('preserved provisional profile '+key,nm['profile'][key],om['profile'][key])
for key in om['closed_json_domain']:
    if key not in ['projection','raw_preparse']:check('preserved domain '+key,nm['closed_json_domain'][key],om['closed_json_domain'][key])
check('no guard promotion',any(nm['guards'].values()),False)
check('Data undefined',nm['cost_catalogue']['definition_files'],[])

def shape(v,depth=0):
    if isinstance(v,dict):
        s=[shape(x,depth+1) for x in v.values()]
        return (1+sum(x[0] for x in s),len(v)+sum(x[1] for x in s),max([depth+1]+[x[2] for x in s]),max([len(k.encode('utf-16-le'))//2 for k in v]+[x[3] for x in s]),sum(x[4] for x in s))
    if isinstance(v,list):
        s=[shape(x,depth+1) for x in v]
        return (1+sum(x[0] for x in s),sum(x[1] for x in s),max([depth+1]+[x[2] for x in s]),max([0]+[x[3] for x in s]),sum(x[4] for x in s))
    return (1,0,0,len(v.encode('utf-16-le'))//2 if isinstance(v,str) else 0,int(isinstance(v,float)))
s=shape(nm)
check('complete bootstrap shape',s,(9997,6107,4,435,0))
check('bootstrap literal present','Assert-TaskRawJsonBytes $taskBootstrap 16104 512 4' in new_main)
check('bootstrap current metadata',nm['R10_payload_repairs']['bootstrap_observed'],dict(values=9997,keys=6107,max_string_units=435,container_depth=4,floating_values=0,nodes_and_keys=16104))
for row in nm['inputs']:
    p=Path(row['path'])
    if not p.is_absolute():p=ROOT/p
    b=p.read_bytes();check('fresh provisional input '+row['path'],(len(b),sha(b)),(row['bytes'],row['sha256']))
check('provisional input count',len(nm['inputs']),1220)
check('provisional input bytes',sum(r['bytes'] for r in nm['inputs']),460232026)
check('input chunks',sum((r['bytes']+65535)//65536 for r in nm['inputs']),7849)

r=nf['Read-TaskJson'];d=nf['Read-TaskDependencyJson'];c=nf['Assert-TaskDependencies']
check('admission order',r.index('Assert-TaskCodecBinding')<r.index('Read-TaskBoundedBytes')<r.index('DEPENDENCY_CAPTURED_BYTE_IDENTITY_BEFORE_PARSE')<r.index('Add-TaskJsonVisit')<r.index('Assert-TaskRawJsonBytes')<r.index('Read-TaskDependencyJson'))
check('predecessor codec tail exact',r[r.index(' $taskRaw='):],of['Read-TaskJson'][of['Read-TaskJson'].index(' $taskRaw='):])
for fragment in ['[IO.MemoryStream]::new($Bytes,0,$Length,$false,$false)','[IO.StreamReader]::new($taskMemory,[Text.UTF8Encoding]::new($false,$true),$false,1024,$true)','$taskJsonReader.CloseInput=$false','$taskSettings.DateParseHandling=[Newtonsoft.Json.DateParseHandling]::None','$taskSettings.DateTimeZoneHandling=[Newtonsoft.Json.DateTimeZoneHandling]::RoundtripKind','$taskSettings.TypeNameHandling=[Newtonsoft.Json.TypeNameHandling]::None','$taskSettings.MetadataPropertyHandling=[Newtonsoft.Json.MetadataPropertyHandling]::Ignore','$taskSettings.MaxDepth=24;$taskSettings.CheckAdditionalContent=$true','return ,($taskSerializer.Deserialize($taskJsonReader))','finally{Close-TaskJsonReader $taskJsonReader;Close-TaskOwner $taskTextReader;Close-TaskOwner $taskMemory;Complete-TaskOwnerClose $taskBefore $taskBody}']:
    check('new fixed parser source '+fragment,fragment in d)
check('outer comma boundary','return ,(Read-TaskDependencyJson $taskBytes $taskCompactLength)' in r)
check('only actual invocation new parser',new.count('(Read-TaskDependencyJson $taskBytes $taskCompactLength)'),1)
check('only dependency consumer',new.count('=Read-TaskJson $taskReportPath 4194304 $taskReportHash'),1)
check('closed new string consumers',new.count('Get-TaskDependencyString '),3)
check('no arbitrary token wire',c[c.index('return @{'):].strip(),"return @{includes=$taskIncludes.get_Count();report_is_additional_artifact_only=$true;complete_tool_inventory_replaced=$false}\n }")
check('original dependency descriptor prefix exact',c[:c.index('  if((Get-TaskRuntimeType $taskD)')],of['Assert-TaskDependencies'][:of['Assert-TaskDependencies'].index(' if((Get-TaskRuntimeType $taskD)')])
for reason in ['DEPENDENCY_ROOT_MAP','DEPENDENCY_DATA_MAP','Compiler source-dependency source binding','Unqualified compiler module/header-unit dependency','Compiler dependency Includes finite profile','Source-dependency include outside pre-fenced conservative inventory']:
    check('same dependency refusal '+reason,reason in c and reason in of['Assert-TaskDependencies'])

primary=ROOT/'build-ci/20261105-controller-environment-writer-closure-r2/primary/src/System.Management.Automation/engine'
require(primary/'LanguagePrimitives.cs','if (toType.IsAssignableFrom(fromType))')
require(primary/'LanguagePrimitives.cs','toType == fromType ? ConversionRank.Identity : ConversionRank.Assignable')
require(primary/'CoreAdapter.cs','return PSMethod.Create(methods[0].method.Name, this, obj, methods, isSpecial, methods.IsHidden) as T;')
require(primary/'OrderedHashtable.cs','_orderedDictionary = new OrderedDictionary(capacity);')
sources={
 'build-ci/20261105-controller-environment-writer-closure-r2/primary/src/Microsoft.PowerShell.Commands.Utility/commands/utility/WebCmdlet/JsonObject.cs':['case JsonDateKind.String:', 'dateParseHandling = DateParseHandling.None;', 'return returnHashtable', 'OrderedHashtable result = new(entries.Count);'],
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JsonSerializerInternalReader.cs':['return CreateJObject(reader);','using (JTokenWriter writer = new JTokenWriter())','writer.WriteToken(reader, true, true, false);','if (checkAdditionalContent)'],
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JTokenWriter.cs':['(_parent as JObject)?.Remove(name);','AddParent(new JProperty(name));'],
 'build-ci/20261105-clock-oracle-r10-controlled-payload-closure-r1/primary-sources/JObject.cs':['if (comparison != StringComparison.Ordinal)','var property = Property(propertyName, comparison);','return property?.Value;'],
 'build-ci/20261106-clock-r12-serializer-runtime-source-r1/primary/JsonSerializer.cs':['return Deserialize(reader, null);','JsonSerializerSettings? defaultSettings = JsonConvert.DefaultSettings?.Invoke();','ApplySerializerSettings(serializer, settings);'],
 'build-ci/20261105-clock-oracle-json-primary-sources/newtonsoft-JsonConvert.cs':['jsonSerializer.CheckAdditionalContent = true;','using (JsonTextReader reader = new JsonTextReader(new StringReader(value)))'],
 'build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/JsonTextReader.cs':['public override void Close()','if (CloseInput)','_stringBuffer.Clear(_arrayPool);'],
 'build-ci/20261105-clock-oracle-r9-numeric-independent-review/primary-sources/StreamReader.cs':['_detectEncoding = detectEncodingFromByteOrderMarks;','_closable = !leaveOpen;','if (_closable)']}
for path,fragments in sources.items():
    for fragment in fragments:require(ROOT/path,fragment)

cor=json.loads(read(AUTHOR/'source-correspondence.json'))
check('author reported correspondence independently recomposed',cor['bootstrap_shape'],nm['R10_payload_repairs']['bootstrap_observed'])
check('author whole shape not execution',cor['no_target_execution'] and cor['no_price_resource_fit_binding_GO'])
inventory=[dict(name=n,changed=n in changed,added=n in added,bytes=len(nf[n].encode()),sha256=sha(nf[n].encode())) for n in nf]
result=dict(status='PASS_INDEPENDENT_WHOLE_FILE_SOURCE_CORRESPONDENCE_ONLY',checks=checks,count=len(checks),inputs=list(inputs.values()),helpers=inventory,bootstrap=dict(values=s[0],keys=s[1],container_depth=s[2],max_string_units=s[3],floats=s[4]),no_target_execution=True)
dest=HERE/'checks.json'
if dest.exists():raise ValueError('Preserve first whole result')
dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status=result['status'],checks=result['count'],sha256=sha(dest.read_bytes()))))
