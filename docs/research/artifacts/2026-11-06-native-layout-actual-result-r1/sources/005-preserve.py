"""Preserve already generated compile-only evidence; no compiler/decoder run."""
from pathlib import Path
import base64
import hashlib
import json

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(__file__).resolve().parent
PURPOSE = ROOT / 'build-ci/20261106-native-object-layout-purpose-source-r1'
RUN = PURPOSE / 'execution-r1'
sha = lambda raw: hashlib.sha256(raw).hexdigest().upper()
assert not (OUT / 'source-pins.json').exists()
receipt = json.loads((RUN / 'receipt.json').read_bytes())
values = json.loads((RUN / 'layout-values.json').read_bytes())
schema = json.loads((PURPOSE / 'schema.json').read_bytes())
assert receipt['first_green'] and receipt['compiler_raw_exit'] == 0
assert receipt['compiler_process_settlement_observed'] and receipt['failure'] is None
assert receipt['source_pins_sha256'] == sha((PURPOSE / 'source-pins.json').read_bytes())
assert receipt['object_sha256'] == sha((RUN / 'layout.obj').read_bytes()) == values['object_sha256']
assert receipt['layout_values_sha256'] == sha((RUN / 'layout-values.json').read_bytes())
assert values['cells'] == len(values['rows']) == len(schema['entries']) == 194
assert [(x['name'], x['expression'], x['kind']) for x in values['rows']] == [(x['name'], x['expression'], x['kind']) for x in schema['entries']]
assert len({x['name'] for x in values['rows']}) == 194
assert all(isinstance(x['value'], int) and 0 <= x['value'] < (1 << 64) for x in values['rows'])
assert receipt['include_count'] == 111 and values['machine'] == 0x8664
assert (RUN / 'compiler.stderr.log').stat().st_size == 0

inputs = []
for path in sorted(x for x in RUN.rglob('*') if x.is_file()):
    raw = path.read_bytes()
    inputs.append({'path': path.relative_to(ROOT).as_posix(), 'bytes': len(raw), 'sha256': sha(raw)})
    if path.suffix == '.obj':
        (OUT / 'raw-layout-coff.json').write_text(json.dumps({'encoding': 'base64', 'original_path': path.relative_to(ROOT).as_posix(), 'bytes': len(raw), 'sha256': sha(raw), 'data': base64.b64encode(raw).decode('ascii'), 'scope': 'Compile-only data object provenance, never linked or executed'}, indent=2) + '\n', encoding='utf-8', newline='\n')

for path in [PURPOSE / 'compile-authorization.json', PURPOSE / 'source-pins.json', ROOT / 'build-ci/20261106-native-layout-purpose-authority-context-r1.json', ROOT / 'build-ci/20261106-native-layout-root-launch-r1.py', ROOT / 'build-ci/20261106-native-layout-purpose-independent-review-r1/review.json', ROOT / 'build-ci/20261106-native-layout-caller-review-r1/review.json']:
    raw = path.read_bytes()
    inputs.append({'path': path.relative_to(ROOT).as_posix(), 'bytes': len(raw), 'sha256': sha(raw)})

rows = {x['name']: x['value'] for x in values['rows']}
summary = {'status': 'ACTUAL_COMPILE_ONLY_LAYOUT_QUALIFIED', 'compiler_raw_exit': receipt['compiler_raw_exit'], 'compiler_seconds': receipt['seconds'], 'include_count': receipt['include_count'], 'actual_cells': values['cells'], 'object_sha256': values['object_sha256'], 'actual_selected_sizes': {name: rows[name + '.sizeof'] for name in ['Text', 'Stream', 'FractionCache', 'Rejection', 'CachedPort', 'Production', 'Catalogue', 'DoublePair', 'String', 'ChangeVector']}, 'actual_MSC_FULL_VER': rows['compiler.MSC_FULL_VER'], 'no_link': True, 'Clock_or_supervisor_execution': False, 'not_qualified': ['whole_python_DLL_stdlib_closure', 'native_target_source_binary_association', 'automatic_lifetime_sum', 'physical_stack_or_RSS', 'generic_runtime_CRT_EH_FILE_heap', 'whole_resource_fit', 'prices', 'Data', 'Clock_or_supervisor_behavior']}
(OUT / 'actual-summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8', newline='\n')
(OUT / 'result.md').write_text('# Actual compile-only layout qualification\n\nThe separately reviewed and root-authorized single purpose compiled with raw exit0 in ' + str(receipt['seconds']) + ' seconds. All111 reported include paths were admitted; the reviewed file-only reader decoded194 actual compiler integer cells. Full source, raw stdout/stderr/dependencies/receipt, all values and lossless raw COFF provenance are retained. No native target was linked or executed.\n\nThe complete source and thin caller passed726 and30 independent file/source checks respectively before root authorization. These counts are not runtime test counts. The author\'s documentation-check failure and the exact first source seal remain preserved.\n\nPublic types and reviewed nominal replicas qualify only the selected type/layout facts under their pinned ABI correspondence. The result supplies no automatic-lifetime sum, physical stack/RSS, complete generic runtime/native storage, source/binary binding, work price, Data, whole32MiB fit or Clock/supervisor behavior. The Python executable hash is not a whole DLL/stdlib closure.\n', encoding='utf-8', newline='\n')
raw = Path(__file__).read_bytes()
inputs.append({'path': Path(__file__).relative_to(ROOT).as_posix(), 'bytes': len(raw), 'sha256': sha(raw)})
(OUT / 'source-pins.json').write_text(json.dumps({'schema': 'ACTUAL_NATIVE_LAYOUT_RESULT_DELIVERY_R1', 'inputs': inputs, 'no_new_execution': True}, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps({'actual_cells_preserved': 194, 'original_inputs': len(inputs), 'source_pins_sha256': sha((OUT / 'source-pins.json').read_bytes())}))
