"""Ordinary file/hash/text checks only; never runs or loads the metadata reader."""
from pathlib import Path
from hashlib import sha256
import difflib
import json
import re
import sys

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SUB=ROOT/'build-ci/20261105-managed-file-type-inventory-source-r2'
OLD=ROOT/'build-ci/20261105-managed-file-type-inventory-source-r1'
checks=[]
verified=[]
def pin(p):
    p=Path(p)
    if not p.is_absolute():p=ROOT/p
    b=p.read_bytes()
    return {'path':p.relative_to(ROOT).as_posix() if p.is_relative_to(ROOT) else p.as_posix(),'bytes':len(b),'sha256':sha256(b).hexdigest().upper()}
def check(name,result,detail=None):checks.append({'name':name,'pass':bool(result),'detail':detail})
mp=pin(SUB/'source-pins.json')
check('external_subject_manifest_pin',mp['sha256']=='72F84E45F43CB4ED86E91B6C1CCB464A904E043E5338FAF941C47284E69DC7DC')
verified.append(mp)
m=json.loads((SUB/'source-pins.json').read_bytes())
for group in ['target_inputs','reader_dependency_file_pins','authority_references','authored_sources']:
    for row in m[group]:
        actual=pin(row['path'])
        check('pin:'+row['path'],actual==row,actual)
        verified.append(actual)
r=(SUB/'reader.cs').read_bytes()
o=(OLD/'reader.cs').read_bytes()
rt=r.decode('utf-8');ot=o.decode('utf-8')
check('external_reader_pin',sha256(r).hexdigest().upper()=='9215EED3EDAC196C983353ADC68D1BA95FCD0C4833ED1130FB7C8F39CCC8D348')
check('reader_UTF8_LF',b'\r' not in r and rt.encode('utf-8')==r)
check('old_input_table_byte_exact',rt[rt.index('    private const string RuntimeRoot'):rt.index('    private static readonly string[] OptionalCoreTypes')]==ot[ot.index('    private const string RuntimeRoot'):ot.index('    private sealed class InventoryFailure')])
check('old_Main_Fail_byte_exact',rt[rt.index('    private sealed class InventoryFailure'):rt.index('    private static byte[] ReadAll()')]==ot[ot.index('    private sealed class InventoryFailure'):ot.index('    private static byte[] ReadAll()')])
check('old_WriteType_and_complete_signature_decoder_byte_exact',rt[rt.index('    private static void WriteType('):]==ot[ot.index('    private static void WriteType('):])
check('old_same_handle_hash_gate_byte_exact',rt[rt.index('                // Same read-only handle'):rt.index('                bool core =')]==ot[ot.index('                // Same read-only handle'):ot.index('                // This walks only TypeDef')])
check('no_runtime_type_assembly_execution',not re.search(r'Assembly\.Load|\bLoadFrom\s*\(|\.GetValue\s*\(|\.GetField\s*\(|\bFieldOffset\b|\.GetOffset\s*\(|Activator\.|Process\.Start|Environment\.|DllImport|\.GetMethodBody\s*\(|\.GetILBytes\s*\(|\.GetCustomAttributes\s*\(',rt))
check('args_override_refused','if (args.Length != 0) throw new InventoryFailure("ARGUMENTS_NOT_ALLOWED");' in rt)
check('original_21_type_allowlist_literals',all(('"'+n+'"') in rt for n in m['allowlist_original_required']) and len(m['allowlist_original_required'])==21)
check('exact_three_optional_type_literals',all(('"'+n+'"') in rt for n in m['optional_named_types']) and len(m['optional_named_types'])==3)
check('unique_generated_selection_required',all(x in rt for x in ['reader.GetTypeDefinition(wanted[parentName]).GetNestedTypes()','const string prefix = parentName + "+<GetEnumerator>d__";','if (!name.StartsWith(prefix, StringComparison.Ordinal)) continue;','bool numericSuffix = name.Length > prefix.Length;',"if (name[at] < '0' || name[at] > '9') numericSuffix = false;",'if (!selected.IsNil) throw new InventoryFailure("AMBIGUOUS_ENUMERATOR_NESTED_NAME");','if (selected.IsNil) throw new InventoryFailure("ENUMERATOR_NESTED_NAME_MISSING");']))
check('optional_fields_only_when_present','bool present = !handle.IsNil;' in rt and 'if (!present) continue;\n                        WriteType(json, input, reader, provider, handle, type);' in rt)
check('complete_count_gate','requiredCount != 21 || generatedCount != 1 || discoveries.Count != 1 || optionalPresence.Count != 3\n                || count < 22 || count > 25' in rt)
check('only_ordinal_matching',rt.count('StringComparison.Ordinal')==ot.count('StringComparison.Ordinal')+3)
check('declared_nonstatic_fields_only','if ((field.Attributes & FieldAttributes.Static) != 0) continue;' in rt)
check('only_one_published_stdout_buffer','byte[] output = ReadAll();' in rt and rt.count('stdout.Write(')==1 and rt.count('Console.OpenStandardOutput()')==1)
check('schema_v2_explicit','FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V2' in rt and 'FIXED_PUBLIC_FILE_METADATA_TYPE_INVENTORY_V1' not in rt)
check('three_presence_rows_even_when_absent','optionalPresence.Add(new OptionalPresence(input.File, type, present));\n                        if (!present) continue;' in rt)
check('new_generated_and_optional_use_same_WriteType',rt.count('WriteType(json, input, reader, provider,')==3)
check('ReadAll_no_new_file_network_native_APIs',not re.search(r'Http|WebRequest|Socket|Directory\.|EnumerateFiles|GetEnvironment|Assembly\.Load|Type\.GetType|Marshal\.',rt))
diff=''.join(difflib.unified_diff(ot.splitlines(True),rt.splitlines(True),fromfile='frozen_R1_reader',tofile='unrun_R2_reader'))
(HERE/'reader-independent.diff').write_text(diff,encoding='utf-8',newline='\n')
check('author_diff_matches_independent_diff_body', '\n'.join(diff.splitlines()[2:])=='\n'.join((SUB/'evidence/reader.diff').read_text().splitlines()[2:]))
out={'schema':'FIXED_METADATA_R2_INDEPENDENT_SOURCE_CHECKS','checks':checks,'count':len(checks),'failures':[c for c in checks if not c['pass']],'verified_files':verified,'reader_lines':len(rt.splitlines()),'reader_bytes':len(r),'scope':'Only ordinary file/hash/text. No reader parse/compile/load/run, target API, assembly execution, native, GUI, Git or CI.'}
(HERE/'mechanical-evidence.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps({'count':len(checks),'failures':out['failures'],'reader_lines':out['reader_lines'],'reader_bytes':len(r)},indent=2))
sys.exit(1 if out['failures'] else 0)
