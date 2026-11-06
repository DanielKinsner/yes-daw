"""Review-only bytes/text/JSON checks. Does not compile or execute the reader."""
from pathlib import Path
import difflib
import hashlib
import json
import re

ROOT=Path(__file__).resolve().parents[3]
BASE=ROOT/'build-ci/20261105-managed-file-type-inventory-source-r3'
PREV=ROOT/'build-ci/20261105-managed-file-type-inventory-source-r2'
OUT=Path(__file__).resolve().parent
checks=[]
files={}
def read(p):
    p=Path(p)
    if not p.is_absolute():p=ROOT/p
    b=p.read_bytes()
    try:name=p.relative_to(ROOT).as_posix()
    except ValueError:name=p.as_posix()
    files[name]={'path':name,'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest().upper()}
    return b
def ck(name,ok):
    assert ok,name
    checks.append({'check':name,'result':'PASS'})
def load(p):return json.loads(read(p))
m=load(BASE/'source-pins.json')
ck('exact supplied R3 manifest',hashlib.sha256(read(BASE/'source-pins.json')).hexdigest().upper()=='3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2')
for group in ['target_inputs','reader_dependency_file_pins','authority_references','source_files']:
    for r in m[group]:
        b=read(r['path'])
        ck(group+' '+r['path'],len(b)==r['bytes'] and hashlib.sha256(b).hexdigest().upper()==r['sha256'])
for key in ['predecessor_manifest','unsealed_lineage']:
    r=m[key];b=read(r['path'])
    ck(key,len(b)==r['bytes'] and hashlib.sha256(b).hexdigest().upper()==r['sha256'])
old=load(PREV/'source-pins.json')
cs=read(BASE/'reader.cs')
ck('exact supplied reader SHA',hashlib.sha256(cs).hexdigest().upper()=='7A8CCCE39C75E2C0030D01A864E2BFD55FDB81E5EA2B6A9644A8EF8B3B5B7F51')
ck('377 whole lines',len(cs.decode().splitlines())==377)
ck('four targets unchanged',m['target_inputs']==old['target_inputs'] and len(m['target_inputs'])==4)
ck('twelve dependencies unchanged',m['reader_dependency_file_pins']==old['reader_dependency_file_pins'] and len(m['reader_dependency_file_pins'])==12)
before=read(PREV/'reader.cs')
freeze=load(PREV/'execution-r1/first-green-whole-source-freeze.json')
oldrecord=next(r for r in freeze['files'] if r['path']=='reader.cs')
ck('actual frozen R2 reader',hashlib.sha256(before).hexdigest().upper()==oldrecord['sha256']=='9215EED3EDAC196C983353ADC68D1BA95FCD0C4833ED1130FB7C8F39CCC8D348')
oldreview=load('build-ci/20261105-managed-file-type-inventory-independent-review-r2/review.json')
ck('frozen R2 whole source review matches',oldreview['subject_manifest_sha256']==m['predecessor_manifest']['sha256'] and oldreview['reader_sha256']==oldrecord['sha256'])
changed=before.decode()
replacements=[
('new[] { "System.Tuple`5", "System.Collections.ArrayList", "System.Collections.DictionaryEntry" }','new[] { "System.Tuple`5", "System.Collections.ArrayList", "System.Collections.DictionaryEntry", "System.Tuple`2" }'),
('FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V2','FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V3'),
('json.WriteNumber("required_named_types", 21)','json.WriteNumber("required_named_types", 22)'),
('requiredCount != 21','requiredCount != 22'),
('count < 22 || count > 25','count < 23 || count > 26')]
for a,b in replacements:
    ck('single narrow replacement '+a,changed.count(a)==1)
    changed=changed.replace(a,b)
ck('entire R3 equals only five fixed replacements',changed.encode()==cs)
diff=''.join(difflib.unified_diff(before.decode().splitlines(True),cs.decode().splitlines(True),fromfile='frozen-r2/reader.cs',tofile='unrun-r3/reader.cs'))
ck('whole authored diff exact',read(BASE/'evidence/reader.diff').decode()==diff)
(OUT/'reader-independent.diff').write_text(diff,encoding='utf-8',newline='\n')
inputs=[]
top=cs.decode().split('private static readonly string[] OptionalCoreTypes')[0]
for match in re.finditer(r'new\("([^\"]+)",\s*(\d+),\s*"([0-9A-F]{64})",\s*new\[\]\s*\{(.*?)\}\)',top,re.S):
    inputs.append({'file':match[1],'bytes':int(match[2]),'sha256':match[3],'types':re.findall(r'"([^\"]+)"',match[4])})
ck('four source input definitions',len(inputs)==4)
for i,row in enumerate(inputs):
    expected=m['target_inputs'][i]
    ck('source target '+row['file'],expected['path'].endswith('/'+row['file']) and expected['bytes']==row['bytes'] and expected['sha256']==row['sha256'])
allow=[n for x in inputs for n in x['types']]
ck('22 unique exact required names',len(allow)==len(set(allow))==22 and allow==m['allowlist_required'])
ck('sole added name Tuple2',allow==old['allowlist_original_required']+['System.Tuple`2'])
ck('target type split16/1/1/4',[len(r['types']) for r in inputs]==[16,1,1,4])
ck('optional core types retained',m['optional_named_types']==old['optional_named_types'] and len(m['optional_named_types'])==3)
ck('no required/optional overlap',not set(allow)&set(m['optional_named_types']))
expectedcounts={'target_files':4,'requested_types':22,'newtonsoft':16,'sma':1,'specialized':1,'corelib':4,'required_named_types':22,'optional_named_types':3,'required_generated_selections':1,'reported_types_min':23,'reported_types_max':26}
ck('all count metadata exact',m['counts']==expectedcounts)
ck('exact count range arithmetic',22+1==23 and 22+1+3==26)
ck('same runtime fixed root source',b'private const string RuntimeRoot = "C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/";' in cs)
for needle in [
'if (args.Length != 0) throw new InventoryFailure("ARGUMENTS_NOT_ALLOWED")',
'FileMode.Open, FileAccess.Read, FileShare.Read',
'SHA256.HashData(file)',
'PEStreamOptions.PrefetchMetadata | PEStreamOptions.LeaveOpen',
'file.Position = 0',
'if (!pe.HasMetadata)',
'if (!reader.IsAssembly)',
'if (handle.IsNil) throw new InventoryFailure("REQUESTED_TYPE_MISSING")',
'if (!prior.IsNil) throw new InventoryFailure("DUPLICATE_REQUESTED_TYPE")',
'if (!selected.IsNil) throw new InventoryFailure("AMBIGUOUS_ENUMERATOR_NESTED_NAME")',
'if (selected.IsNil) throw new InventoryFailure("ENUMERATOR_NESTED_NAME_MISSING")',
'numericSuffix = name.Length > prefix.Length',
'if ((field.Attributes & FieldAttributes.Static) != 0) continue',
'field.DecodeSignature(provider, context)',
'GetGenericTypeParameter(GenericContext context, int index)',
'if (!activeSpecifications.Add(handle))',
'byte[] output = ReadAll()',
'stdout.Write(output, 0, output.Length)',
]:ck('preserved guard/behavior '+needle,needle.encode() in cs)
for p in [BASE/'reader.cs',BASE/'README.md',BASE/'source-pins.json',BASE/'evidence/reader.diff']:
    b=read(p);ck('UTF8 LF noBOM '+p.name,not b.startswith(b'\xef\xbb\xbf') and b'\r' not in b)
ck('unsealed counts expressly archival',m['unsealed_lineage']['reason'].startswith('Initial copied R2 counts') and 'authored_sources' not in m)
result={'schema':'FIXED_PUBLIC_METADATA_R3_INDEPENDENT_FILE_CHECKS','checks_passed':len(checks),'checks':checks,'verified_files':list(files.values()),'source_input_types':inputs,'allowed_delta':replacements,'no_reader_compile_or_run':True,'no_target_execution':True}
(OUT/'file-checks.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'checks':len(checks),'verified_files':len(files),'whole_lines':377,'required_types':len(allow),'output_range':[23,26]}))
