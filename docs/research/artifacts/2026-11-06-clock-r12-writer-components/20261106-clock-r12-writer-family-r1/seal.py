from pathlib import Path
import hashlib
import json

here = Path(__file__).resolve().parent
root = here.parents[1]
dest = here / 'source-pins.json'
assert not dest.exists()
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
component = json.loads((here / 'writer-component.json').read_bytes())
supplement = json.loads((here / 'cold-supplement.json').read_bytes())
assert supplement['base_component_sha256'] == digest((here / 'writer-component.json').read_bytes())
for row in component['inputs'] + supplement['inputs']:
    data = (root / row['path']).read_bytes()
    assert len(data) == row['bytes'] and digest(data) == row['sha256'], row['path']
paths = sorted(p for p in here.rglob('*') if p.is_file())
paths += [root / 'build-ci' / name for name in [
    '20261106-prepare-json-writer-source.py',
    '20261106-prepare-json-writer-fallback-source.py',
    '20261106-prepare-json-writer-cold-source.py',
    '20261106-prepare-json-writer-cold-supplement-source.py']]
result = dict(schema='R12_SELECTED_WRITER_SOURCE_COMPONENT_PINS_R1',
    status='SEALED_COMPONENT_FOR_INDEPENDENT_REVIEW_COMPLETE_B_COLD_AND_GLOBAL_FIT_HOLD',
    runner_sha256=component['runner_sha256'],
    selected_B_prefixes=[p['selected_B_request_upper'] for p in component['phases']],
    selected_known_once_F_Bcold=supplement['known_once_F_Bcold_subtotal'],
    every_remaining_codec_boundary_explicit=True, no_target_execution=True,
    files=[dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in paths])
dest.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(paths), source_pins_sha256=digest(dest.read_bytes()), component_sha256=digest((here / 'writer-component.json').read_bytes()), supplement_sha256=digest((here / 'cold-supplement.json').read_bytes()))))
