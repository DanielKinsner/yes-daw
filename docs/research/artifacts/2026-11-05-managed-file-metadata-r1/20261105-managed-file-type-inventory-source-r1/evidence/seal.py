import hashlib,json,pathlib,re
BASE=pathlib.Path(__file__).resolve().parent.parent
ROOT=BASE.parent.parent
RUNTIME=pathlib.Path('C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell')
def pin(p):return {'path':p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest().upper()}
source=(BASE/'reader.cs').read_text()
names=['Newtonsoft.Json.dll','System.Management.Automation.dll','System.Collections.Specialized.dll','System.Private.CoreLib.dll']
deps=['System.Reflection.Metadata.dll','System.Collections.Immutable.dll','System.Text.Json.dll','System.Text.Encodings.Web.dll','System.Runtime.dll','System.Console.dll','System.Collections.dll','System.Security.Cryptography.dll','System.Runtime.Extensions.dll','System.IO.dll','System.Memory.dll','System.Buffers.dll']
allow=re.findall(r'"((?:Newtonsoft\.Json\.|System\.Management\.Automation\.|System\.Collections\.|System\.Tuple)[^"]*)"',source)
allow=[n for n in allow if not n.endswith('.dll')]
assert len(allow)==len(set(allow))==21
records=[pin(RUNTIME/n) for n in names]
for r in records:assert str(r['bytes']) in source and r['sha256'] in source
for forbidden in [r'\bAssembly\.Load',r'\bGetValue\s*\(',r'\bMarshal\.',r'\bGetEnvironment',r'\bGetUninitializedObject',r'\bGetMethodBody']:
    assert re.search(forbidden,source) is None
refs=['build-ci/20261105-managed-payload-abi-runtime-metadata-r1/runtime.json','build-ci/20261105-managed-payload-abi-probe-source-r2/source-pins.json','build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r10-native-path-correction-r3/source-manifest.json']
d={'schema':'FIXED_PUBLIC_FILE_METADATA_INVENTORY_SOURCE_PINS_R1',
 'status':'SOURCE_ONLY_UNCOMPILED_UNRUN_REQUIRES_ROOT_WHOLE_REVIEW',
 'target_inputs':records,'reader_dependency_file_pins':[pin(RUNTIME/n) for n in deps],
 'authority_references':[pin(ROOT/n) for n in refs],'allowlist':allow,
 'counts':{'target_files':4,'requested_types':21,'newtonsoft':16,'sma':1,'specialized':1,'corelib':3},
 'authored_sources':[pin(BASE/n) for n in ['reader.cs','README.md','evidence/seal.py']],
 'source_checks':{'exact21_unique_types':True,'four_expected_length_hash_constants':True,'forbidden_runtime_inspection_apis_absent':True,'all_authored_UTF8_LF':all(b'\r' not in (BASE/n).read_bytes() for n in ['reader.cs','README.md','evidence/seal.py'])},
 'execution':{'compile':False,'run':False,'AddType':False,'target_parser_AST_import':False,'target_assembly_load':False,'target_constructors_fields_offsets':False,'native_GUI_Git_CI':False},
 'limits':'Public PE/CLR file metadata only. No actual runtime object layout/offset/size/reclaim or32MiB fit/GO credit. Host dependency pinning is file identity only; root owns actual compiler/host association.'}
(BASE/'source-pins.json').write_bytes((json.dumps(d,indent=2)+'\n').encode())
print('PASS source-only21-type allowlist,hash constants and API text checks; no compilation or metadata execution')
for n in ['reader.cs','README.md','source-pins.json']:print(n,pin(BASE/n)['sha256'])
