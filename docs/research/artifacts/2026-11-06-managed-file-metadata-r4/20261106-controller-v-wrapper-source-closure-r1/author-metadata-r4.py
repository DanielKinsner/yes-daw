"""Produce a new, unrun R4 fixed-file metadata source packet. No target code executes."""
import copy,difflib,hashlib,json,pathlib,re

ROOT=pathlib.Path(__file__).resolve().parents[2]
HERE=pathlib.Path(__file__).parent
OLD=ROOT/'build-ci/20261105-managed-file-type-inventory-source-r3'
NEW=ROOT/'build-ci/20261106-managed-file-type-inventory-source-r4'
RUNTIME=pathlib.Path('C:/Users/SM - Dan/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell')
def sha(data):return hashlib.sha256(data).hexdigest().upper()
def rec(path):
 data=path.read_bytes()
 try: name=path.relative_to(ROOT).as_posix()
 except ValueError:name=path.as_posix()
 return {'path':name,'bytes':len(data),'sha256':sha(data)}
def encoded(value):return (json.dumps(value,indent=2,ensure_ascii=True)+'\n').encode()
checks=[]
def check(name,condition):
 checks.append({'id':name,'pass':bool(condition)})
 if not condition:raise RuntimeError(name)

check('NEW_OUTPUT_ABSENT',not NEW.exists())
old_source=(OLD/'reader.cs').read_bytes()
old_manifest=(OLD/'source-pins.json').read_bytes()
check('R3_READER_SEAL',sha(old_source)=='7A8CCCE39C75E2C0030D01A864E2BFD55FDB81E5EA2B6A9644A8EF8B3B5B7F51')
check('R3_MANIFEST_SEAL',sha(old_manifest)=='3A2060A1F51D275B2B6A160C1899DDDA55680CCC0081264484984E856F377CC2')
prior=json.loads(old_manifest)
plan=json.loads((HERE/'metadata-r4-fixed-type-plan.json').read_text())
extra=plan['extra_types']
old_text=old_source.decode()
start=old_text.index('    private static readonly Input[] Inputs =')
end=old_text.index('    private static readonly string[] OptionalCoreTypes =')
old_input_region=old_text[start:end]
old_groups=[]
for match in re.finditer(r'new\("([^"]+)", (\d+), "([A-F0-9]{64})",\s*new\[\]\s*\{([^}]+)\}\)',old_input_region):
 old_groups.append({'file':match[1],'bytes':int(match[2]),'sha256':match[3],'types':re.findall(r'"([^"]+)"',match[4])})
check('R3_FOUR_INPUT_GROUPS',len(old_groups)==4)
check('R3_REQUIRED_TYPES_22',sum(len(x['types']) for x in old_groups)==22)
groups=copy.deepcopy(old_groups)
for file,names in extra.items():
 existing=next((x for x in groups if x['file']==file),None)
 if existing is None:
  data=(RUNTIME/file).read_bytes()
  existing={'file':file,'bytes':len(data),'sha256':sha(data),'types':[]};groups.append(existing)
 existing['types'].extend(names)
required=sum(len(x['types']) for x in groups)
check('R4_REQUIRED_TYPES_84',required==84)
check('R4_INPUT_FILES_8',len(groups)==8)
for group in groups:
 data=(RUNTIME/group['file']).read_bytes()
 check('INPUT_PIN_'+group['file'],len(data)==group['bytes'] and sha(data)==group['sha256'])
 check('UNIQUE_TYPE_NAMES_'+group['file'],len(set(group['types']))==len(group['types']))
 for name in group['types']:
  terminal=name.rsplit('+',1)[-1] if '+' in name else name.rsplit('.',1)[-1]
  check('RAW_CANDIDATE_NAME_ONLY_'+group['file']+'_'+name,terminal.encode()+b'\0' in data)
for old in old_groups:
 group=next(x for x in groups if x['file']==old['file'])
 check('PRESERVED_R3_INPUT_'+old['file'],group['bytes']==old['bytes'] and group['sha256']==old['sha256'] and group['types'][:len(old['types'])]==old['types'])
for pin in prior['reader_dependency_file_pins']:
 data=pathlib.Path(pin['path']).read_bytes()
 check('DEPENDENCY_PIN_'+pathlib.Path(pin['path']).name,len(data)==pin['bytes'] and sha(data)==pin['sha256'])
lines=['    private static readonly Input[] Inputs =','    {']
for index,group in enumerate(groups):
 lines.extend([f'        new("{group["file"]}", {group["bytes"]}, "{group["sha256"]}", new[]','        {'])
 for at,name in enumerate(group['types']):lines.append('            "'+name+'"'+(',' if at+1<len(group['types']) else ''))
 lines.append('        })'+(',' if index+1<len(groups) else ''))
lines.extend(['    };','',''])
input_region='\n'.join(lines)
source=old_text[:start]+input_region+old_text[end:]
replacements={
'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V3':'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V4',
'json.WriteNumber("required_named_types", 22);':'json.WriteNumber("required_named_types", 84);',
'if (requiredCount != 22 ||':'if (requiredCount != 84 ||',
'|| count < 23 || count > 26)':'|| count < 85 || count > 88)'}
for before,after in replacements.items():
 check('EXACT_LITERAL_CHANGE_'+before,source.count(before)==1)
 source=source.replace(before,after)
normalized=source
for before,after in replacements.items():normalized=normalized.replace(after,before)
new_start=normalized.index('    private static readonly Input[] Inputs =')
new_end=normalized.index('    private static readonly string[] OptionalCoreTypes =')
normalized=normalized[:new_start]+old_input_region+normalized[new_end:]
check('WHOLE_DECODER_AND_CONTROL_LOGIC_BYTE_IDENTICAL_EXCEPT_INPUT_SCHEMA_COUNTS',normalized==old_text)
source_data=source.encode()
check('READER_UTF8_LF',b'\r' not in source_data and source_data.endswith(b'\n'))
unresolved=[
 'Fixed TypeDef names and declaring assembly association are not established by the raw-name authoring hits; the later same-handle metadata reader must independently satisfy every required name or fail.',
 'No live managed object size, generic instantiation layout, field offset, alignment, padding, native wrapper size, allocation observation or CLR/PS selected-source association is provided by this packet.',
 'ExecutionContext, continuation/delegate/fault and general exception graphs, task diagnostic/debug variants and thread-pool/native queue ownership remain separate qualification obligations; no recursive closure is claimed.',
 'Other CodePages provider encoding classes, fallback buffers and native code-page table ownership remain conditional; the selected code page is neither queried nor assumed.',
 'Pooled per-core cache arrays and per-thread cache ownership require their own bounds; metadata declaration output alone neither bounds processor/thread counts nor guarantees cache reuse or physical reclamation.',
 'The new three generated state-machine names are fixed candidate requests, not new discovery logic; the existing exactly-one JObject iterator discovery is unchanged.'
]
readme=('''# Public file metadata inventory R4: source only

UNCOMPILED / UNRUN. This is a bounded purpose source proposal for independent whole review before root prepares any separate hidden compiler/file-reader attempt. It retains all 22 required R3 types and adds 62 fixed required names for the selected controller stream, async task and pooled box, cancellation, process, encoding and CNG hash wrapper field owners. There are eight fixed input DLLs, 84 required named types, the unchanged one required JObject iterator selection and zero to three unchanged optional CoreLib enumerators: 85 to 88 complete output types.

The entire R3 decoder, same-handle length/SHA/read-only PEReader gates, base/field signature decoding, failure categories, buffered complete-output behavior, exactly-one nested JObject selection and optional presence behavior are byte-identical after replacing only the input data region and schema/count literals. The launcher changes only the manifest and reader length/hash pins. Every original target and dependency pin is preserved; four new target files are pinned. Public raw file string hits were used only to author candidate fixed names, without metadata/type/assembly association credit. Missing or incorrect names make the later reader fail before it publishes partial output.

Metadata reports declarations and signatures only. It does not load target assemblies, call target constructors/methods, inspect method bodies/IL/custom-attribute execution, instantiate target runtime types, read live field values, derive offsets, measure objects, or observe allocation. This source packet has not been compiled, imported or run. No clock target parser/AST/import/helper/program/stage, native probe, GUI, input, audio or Git was invoked. The source and its first passing own file-only check are frozen. Independent review and a separately authorized, pinned host/transport attempt are still required.

R3 and all earlier sources and execution records remain immutable. No R12/R13 source, prices, Data, NativeFact, payload fit, environment premise, resource conclusion or GO transfers. The manifest lists precise remaining qualifications. The purpose is finite field-shape evidence, not a recursive runtime closure or an execution gate for the clock runner.
''').encode()
diff=''.join(difflib.unified_diff(old_text.splitlines(True),source.splitlines(True),fromfile='R3/reader.cs',tofile='R4/reader.cs')).encode()
NEW.mkdir();(NEW/'evidence').mkdir()
(NEW/'reader.cs').write_bytes(source_data)
(NEW/'README.md').write_bytes(readme)
(NEW/'evidence/reader.diff').write_bytes(diff)
manifest={
 'schema':'FIXED_PUBLIC_FILE_METADATA_INVENTORY_SOURCE_PINS_R4',
 'status':'SOURCE_ONLY_UNCOMPILED_UNRUN_REQUIRES_INDEPENDENT_WHOLE_REVIEW',
 'predecessor_manifest':rec(OLD/'source-pins.json'),
 'target_inputs':[{'path':(RUNTIME/g['file']).as_posix(),'bytes':g['bytes'],'sha256':g['sha256']} for g in groups],
 'reader_dependency_file_pins':prior['reader_dependency_file_pins'],
 'authority_references':prior['authority_references']+[rec(OLD/'reader.cs'),rec(HERE/'metadata-r4-fixed-type-plan.json'),rec(HERE/'candidate-name-hits.json'),rec(HERE/'fixed-marker-hits.json')],
 'counts':{'target_files':8,'requested_types':84,'required_named_types':84,'added_fixed_required_types':62,'optional_named_types':3,'required_generated_selections':1,'reported_types_min':85,'reported_types_max':88},
 'allowlist_by_file':{g['file']:g['types'] for g in groups},
 'optional_named_types':prior['optional_named_types'],
 'generated_selection':prior['generated_selection'],
 'source_checks':{'all_original_target_dependency_pins_preserved':True,'whole_decoder_logic_identical_except_inputs_schema_counts':True,'all_authored_UTF8_LF':True,'reader_not_compiled_or_run':True,'raw_name_hits_are_not_metadata_or_source_association':True},
 'execution':{k:False for k in ['compile','run','AddType','target_parser_AST_import','target_assembly_load','target_constructors_fields_offsets','native_GUI_Git_CI']},
 'remaining_qualifications':unresolved,
 'source_files':[rec(NEW/'reader.cs'),rec(NEW/'README.md'),rec(NEW/'evidence/reader.diff'),rec(pathlib.Path(__file__))]
}
manifest_data=encoded(manifest);(NEW/'source-pins.json').write_bytes(manifest_data)
launcher=(OLD/'run-reader.ps1').read_text()
for before,after in [(sha(old_manifest),sha(manifest_data)),('22493',str(len(source_data))),('7A8CCCE39C75E2C0030D01A864E2BFD55FDB81E5EA2B6A9644A8EF8B3B5B7F51',sha(source_data))]:
 check('LAUNCHER_EXACT_PIN_SUBSTITUTION_'+before,launcher.count(before)==1)
 launcher=launcher.replace(before,after)
launcher_data=launcher.encode();(NEW/'run-reader.ps1').write_bytes(launcher_data)
launcher_normalized=launcher.replace(sha(manifest_data),sha(old_manifest)).replace(str(len(source_data)),'22493').replace(sha(source_data),'7A8CCCE39C75E2C0030D01A864E2BFD55FDB81E5EA2B6A9644A8EF8B3B5B7F51')
check('LAUNCHER_LOGIC_IDENTICAL',launcher_normalized==(OLD/'run-reader.ps1').read_text())
for path in [NEW/'reader.cs',NEW/'run-reader.ps1',NEW/'README.md',NEW/'source-pins.json']:
 data=path.read_bytes();check('UTF8_LF_'+path.name,b'\r' not in data and data.decode('utf-8').endswith('\n'))
check('R3_READER_PRESERVED',sha((OLD/'reader.cs').read_bytes())==sha(old_source))
check('R3_MANIFEST_PRESERVED',sha((OLD/'source-pins.json').read_bytes())==sha(old_manifest))
report={'schema':'METADATA_R4_OWN_SOURCE_CHECKS_V1','status':'PASS_SOURCE_FILE_ONLY_UNCOMPILED_UNRUN','checks':checks,'check_count':len(checks),'required_type_count':required,'generated_jobject_count':1,'optional_count':3,'reported_min':85,'reported_max':88,'verified_files':[rec(NEW/x) for x in ['reader.cs','run-reader.ps1','README.md','source-pins.json','evidence/reader.diff']],'remaining_qualifications':unresolved,'subject_preservation':[rec(OLD/'reader.cs'),rec(OLD/'source-pins.json')]}
(NEW/'evidence/own-source-checks.json').write_bytes(encoded(report))
print(json.dumps({'status':report['status'],'checks':len(checks),'files':report['verified_files'],'evidence':rec(NEW/'evidence/own-source-checks.json')},indent=2))
