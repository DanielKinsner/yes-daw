from pathlib import Path
import hashlib
import json

here = Path(__file__).resolve().parent
root = here.parents[1]
assert not (here / 'source-pins.json').exists()
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
component = json.loads((here / 'runtime-component.json').read_bytes())
for row in component['input_pins']:
    data = (root / row['path']).read_bytes()
    assert len(data) == row['bytes'] and digest(data) == row['sha256'], row['path']
paths = sorted(p for p in here.rglob('*') if p.is_file())
paths += [root / 'build-ci/20261106-prepare-json-serializer-runtime-source.py', root / 'build-ci/20261106-prepare-json-serializer-error-source.py']
record = dict(schema='R12_SERIALIZER_RUNTIME_AND_FAILURE_COMPONENT_PINS_R1',
    status='SEALED_PROPOSAL_FOR_INDEPENDENT_REVIEW_NOT_COMPLETE_COLD_CODEC_GLOBAL_FIT_OR_GO',
    no_target_execution=True,
    files=[dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in paths])
dest = here / 'source-pins.json'
dest.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(paths), source_pins_sha256=digest(dest.read_bytes()), component_sha256=digest((here / 'runtime-component.json').read_bytes()))))
