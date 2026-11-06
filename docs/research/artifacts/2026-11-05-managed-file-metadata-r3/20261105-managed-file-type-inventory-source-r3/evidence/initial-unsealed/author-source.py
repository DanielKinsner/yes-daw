"""Source authoring only: one further fixed type, no compiler/reader invocation."""
from pathlib import Path
import difflib
import hashlib
import json

root=Path(__file__).resolve().parents[1]
old=root/'build-ci/20261105-managed-file-type-inventory-source-r2'
new=root/'build-ci/20261105-managed-file-type-inventory-source-r3'
assert not new.exists();new.mkdir();(new/'evidence').mkdir()
digest=lambda b:hashlib.sha256(b).hexdigest().upper()
original=(old/'reader.cs').read_bytes();assert digest(original)=='9215EED3EDAC196C983353ADC68D1BA95FCD0C4833ED1130FB7C8F39CCC8D348'
source=original.decode()
replacements={
    'new[] { "System.Tuple`5", "System.Collections.ArrayList", "System.Collections.DictionaryEntry" }':'new[] { "System.Tuple`5", "System.Collections.ArrayList", "System.Collections.DictionaryEntry", "System.Tuple`2" }',
    'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V2':'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V3',
    'json.WriteNumber("required_named_types", 21)':'json.WriteNumber("required_named_types", 22)',
    'requiredCount != 21':'requiredCount != 22',
    'count < 22 || count > 25':'count < 23 || count > 26',
}
for before,after in replacements.items():assert source.count(before)==1,before;source=source.replace(before,after)
(new/'reader.cs').write_text(source,encoding='utf-8',newline='\n')
(new/'evidence/reader.diff').write_text(''.join(difflib.unified_diff(original.decode().splitlines(True),source.splitlines(True),fromfile='frozen-r2/reader.cs',tofile='unrun-r3/reader.cs')),encoding='utf-8',newline='\n')
(new/'README.md').write_text('''# Public file metadata inventory R3: source only

UNCOMPILED / UNRUN. Independent whole source review is required before root prepares a separate bounded hidden launcher. This adds only System.Tuple`2 to the fixed CoreLib required type list; all original21 definitions, the discovered JObject iterator and three optional CoreLib enumerators remain unchanged. Success requires22 required types plus one generated type and0..3 present optional types:23..26 total. Schema is V3; count gates change consistently.

Main, failure categories, same-handle length/SHA/readonly PEReader gate, signature/base/field decoder, nested selection and optional presence behavior remain byte-exact to frozen R2. Only the one type literal and associated schema/count literals differ. Output remains complete buffered public-file metadata with ordinary exit/raw/complete-JSON checks required for transport. No target assembly load, target constructor/method/IL/custom-attribute execution, runtime Type, live field values, layout offsets, object size or allocation observation occurs. File metadata field declarations may inform a later source/ABI upper; they do not certify that upper or32MiB fit.

All four target DLL and12 dependency pins remain exact. R1/R2 sources, launchers and actual results stay frozen. No runner has been copied or repointed here. No compiler, metadata reader, clock-research parser/import/helper/program/stage, GUI, input or device was invoked. Root owns later compiler/host/reference/timeout association after independent review. No R12 execution authority, prices, Data, NativeFact, resource fit or GO is granted.
''',encoding='utf-8',newline='\n')
m=json.loads((old/'source-pins.json').read_text());m['schema']='FIXED_PUBLIC_FILE_METADATA_INVENTORY_SOURCE_PINS_R3';m['status']='SOURCE_ONLY_UNCOMPILED_UNRUN_REQUIRES_INDEPENDENT_WHOLE_REVIEW'
m['allowlist_required']=m.pop('allowlist_original_required')+['System.Tuple`2']
m['source_checks']={'all_original_target_dependency_pins_preserved':True,'only_one_type_and_schema_count_literals_changed':True,'all_authored_UTF8_LF':True,'reader_not_compiled_or_run':True}
m['predecessor_manifest']={'path':(old/'source-pins.json').relative_to(root).as_posix(),'bytes':(old/'source-pins.json').stat().st_size,'sha256':digest((old/'source-pins.json').read_bytes())}
m['source_files']=[{'path':p.relative_to(root).as_posix(),'bytes':p.stat().st_size,'sha256':digest(p.read_bytes())} for p in (new/'reader.cs',new/'README.md',new/'evidence/reader.diff',Path(__file__))]
for key in ('author_sources','author_evidence','authored_source_files'):
    m.pop(key,None)
(new/'source-pins.json').write_text(json.dumps(m,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'reader_bytes':len((new/'reader.cs').read_bytes()),'reader_sha256':digest((new/'reader.cs').read_bytes()),'manifest_sha256':digest((new/'source-pins.json').read_bytes()),'target_invoked':False}))
