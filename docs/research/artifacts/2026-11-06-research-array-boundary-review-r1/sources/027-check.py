"""Independent file/text/hash/data checks only. Never invokes a target parser,
helper, compiler, runtime/metadata probe or an earlier checker.
"""
from pathlib import Path
import hashlib,json
P=Path(__file__).resolve().parent;R=P.parents[1]
if (P/'review.json').exists():raise ValueError('Preserve first complete own pass')
checks=[];pins=[]
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def read(p):
 b=p.read_bytes()
 try:n=p.relative_to(R).as_posix()
 except ValueError:n=p.as_posix()
 pins.append(dict(path=n,bytes=len(b),sha256=sha(b)));return b
def ck(n,c):
 checks.append(dict(name=n,passed=bool(c)))
 if not c:raise ValueError(n)
def source(p):return read(p).decode('utf-8-sig')
subject=R/'build-ci/20261106-research-array-cmdlet-boundary-clarifier-r1.md'
sb=read(subject);ck('exact root subject',sha(sb)=='0A98D4889BE641CD427AA10DFFA648F0806DAAE7302083FBFA8347454E09E661')
authority=[]
for n,h in [('20261105-clock-oracle-separate-research-contract/contract.md','344408DAEC2BD355D44B0B6DA266BF00119B09DF5028B70524699E8E2CD36010'),('20261105-clock-oracle-research-leaf-census-conventions.md','DE33D91712372C2FE887A02FB137AA9A96C8C3BB93CB0D88BF57AA32DBEAE564'),('20261105-clock-oracle-research-bulk-leaf-clarification.md','268597D1F066779CE76DACFDBBC384FC452DC6B9AD319419F52DE597242BB3E8')]:
 b=read(R/'build-ci'/n);ck('exact authority '+n,sha(b)==h);authority.append(dict(path=n,sha256=h,text=b.decode()))
base=R/'build-ci/20261105-controller-environment-writer-closure-r2'
origins=json.loads(read(base/'source-origins.json'))
ck('selected source commit',origins['commit']=='ea554a8f9c6085f54fb2828f7ea286c65c35599f')
names=['src/System.Management.Automation/engine/parser/Compiler.cs','src/System.Management.Automation/engine/runtime/Binding/Binders.cs','src/System.Management.Automation/engine/LanguagePrimitives.cs','src/System.Management.Automation/engine/ParameterBinderBase.cs','src/Microsoft.PowerShell.Commands.Utility/commands/utility/WebCmdlet/ConvertToJsonCommand.cs','src/Microsoft.PowerShell.Commands.Utility/commands/utility/WebCmdlet/ConvertFromJsonCommand.cs']
texts={}
for n in names:
 b=read(base/'primary'/n);match=[z for z in origins['files'] if z['repository_path']==n]
 ck('one origin '+n,len(match)==1);ck('selected source '+n,len(b)==match[0]['bytes'] and sha(b)==match[0]['sha256']);texts[n.rsplit('/',1)[-1]]=b.decode('utf-8-sig')
c=texts['Compiler.cs'];bd=texts['Binders.cs'];lp=texts['LanguagePrimitives.cs'];pb=texts['ParameterBinderBase.cs']
for path,n in [('build-ci/20261105-clock-oracle-r10-runner-census-independent-review-r1/primary-sources/Compiler.cs','Compiler.cs'),('build-ci/20261105-clock-oracle-r11-tuple-metadata-independent-review/primary-sources/Binders.cs','Binders.cs'),('build-ci/20261105-clock-oracle-r11-tuple-metadata-independent-review/primary-sources/LanguagePrimitives.cs','LanguagePrimitives.cs')]:
 ck('older primary copy exact '+n,source(R/path)==texts[n])
evidence_specs=[('literal elements','Compiler.cs',6519,6528,['foreach (var element in arrayLiteralAst.Elements)','var elementValue = Compile(element);','Expression.NewArrayInit(typeof(object), elementValues)']),('literal capture fusion','Compiler.cs',6495,6517,['if (pureExprAst is ArrayLiteralAst)','return values;','PSToObjectArrayBinder.Get()']),('runtime capture branches','Binders.cs',805,837,['Array.Empty<object>()','if (value is List<object>)','CachedReflectionInfo.ObjectList_ToArray','CachedReflectionInfo.EnumerableOps_ToArray']),('typed array copies','LanguagePrimitives.cs',3459,3518,['originalAsArray.CopyTo(newValue, 0);','Array.CreateInstance(resultElementType, valueAsArray.Length)','resultArray.SetValue(resultElement, i);','return result.ToArray(resultElementType);']),('parameter shape','Compiler.cs',4278,4301,['if (arg != null)','CommandParameterInternal_CreateParameterWithArgument','CommandParameterInternal_CreateParameter,']),('presence semantics','ParameterBinderBase.cs',1347,1383,['if (toType == typeof(bool))','if (toType == typeof(SwitchParameter))','result = SwitchParameter.Present;']),('callable member name','Compiler.cs',6444,6465,['memberNameAst.Value','var memberNameExpr = Compile(invokeMemberExpressionAst.Member);']),('bareword value','Compiler.cs',6163,6171,['return Expression.Constant(stringConstantExpressionAst.Value);']),('enum conversion','LanguagePrimitives.cs',3581,3605,['string valueAsString = valueToConvert as string;','result = Enum.Parse(resultType, valueAsString);']),('json public switches and enum','ConvertToJsonCommand.cs',45,77,['public SwitchParameter Compress','public StringEscapeHandling EscapeHandling']),('fromjson public switches and enum','ConvertFromJsonCommand.cs',30,56,['public SwitchParameter AsHashtable','public JsonDateKind DateKind'])]
evidence=[]
for label,name,a,b,needles in evidence_specs:
 span='\n'.join(texts[name].splitlines()[a-1:b])
 for needle in needles:ck(label+':'+needle,needle in span)
 evidence.append(dict(label=label,file=name,lines=[a,b],text=span))
oldjson=source(R/'build-ci/20261105-clock-oracle-r10-runner-census/census.json')
ck('lineage constant ToArray group','"directToArrayCommaReturn": 7' in oldjson)
ck('lineage bulk note','no internal bulk byte CPU fees' in oldjson)
ck('lineage resource obligation','All source-literal,typedconversion,ToArray,pipeline,array-concatenation' in oldjson)
article=source(P/'review.md')
for text in ['ACCEPT as an application','Alternative B','not a new free category','No old helper/call/phase number','all prior owner','actual receiver/type/argument','failed candidate','one real nonidentity string-to-enum','second array','this accepts no row price']:
 ck('review disclosure '+text,text.lower() in article.lower())
prior=[('build-ci/20261106-clock-native-child-source-owners-r1/source-pins.json','D665AF77E103BDFE8F2D12D81D04F9E90C3C95A430343E673AC0D8F3597C8E4A'),('build-ci/20261106-clock-native-resource-boundary-audit-r1/source-pins.json','5F274105BB68DB2F0FFDCE4E7C13D5CA190C60032795C5ABBD750A971CAAE182')]
for n,h in prior:
 b=read(R/n);ck('prior packet pin untouched '+n,sha(b)==h)
 for row in json.loads(b)['files']:
  z=read(R/row['path']);ck('prior packet frozen file '+row['path'],len(z)==row['bytes'] and sha(z)==row['sha256'])
result=dict(schema='RESEARCH_ARRAY_CMDLET_EXISTING_BOUNDARY_INDEPENDENT_REVIEW_R1',verdict='ACCEPT_EXISTING_BOUNDARY_APPLICATION_SELECTED_BUILTIN_PATHS_ONLY',subject=dict(path=subject.relative_to(R).as_posix(),sha256=sha(sb)),authority=authority,selected_source_evidence=evidence,recommendation='Apply existing source/primitive boundary; no per-cell hidden runtime CPU tariff; retain all real source/callback/conversion/destination/storage work; whole R2 review still required.',unsupported=['Any new blanket PowerShell/VM opacity category','Hiding entered own callbacks/loops/helpers or selected C++ SDK bodies','Converting constant callable-designator rule into general constant folding','Per-cell hidden CLR/VM copy tariff not present in existing model','Free storage/GC/pool-hit/physical-reclaim assumption','Transfer of old ToArray/caller/whole prices'],ambiguity='The word VM is scoped to existing selected capture/binder/coercion machinery, not a new category. Compiler implementation facts do not qualify compiled runtime or assign a tariff. No unresolved contract change is needed for the selected cases; actual source/caller and resource/binary gates remain.',not_whole_census_or_numeric_resource_review=True,no_price_fit_Data_flags_or_GO=True,no_target_execution=True,checks=checks,input_pins=pins,discovery_failures_preserved=[dict(action='rg literal Windows glob path build-ci/20261105-clock-oracle*',raw_exit=1,diagnostic='os error123 invalid path syntax; corrected to rg over build-ci'),dict(action='read guessed primary-sources/origins.json under R10 review',raw_exit=1,diagnostic='file absent; selected source-origins ledger located under environment-writer-closure-r2; no subject changes')])
(P/'review.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(status='OWN_SOURCE_BOUNDARY_CHECK_EXIT0',checks=len(checks),verdict=result['verdict'],review_json_sha256=sha((P/'review.json').read_bytes()))))
