from pathlib import Path
import hashlib
import json

here = Path(__file__).resolve().parent
root = here.parents[1]
assert not (here / 'source-pins.json').exists()
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
report = json.loads((here / 'writer-join.json').read_bytes())
for row in report['pinned_inputs']:
    data = (root / row['path']).read_bytes()
    assert len(data) == row['bytes'] and digest(data) == row['sha256'], row['path']
review = root / 'build-ci/20261106-clock-r12-writer-components-independent-review-r2/review.json'
assert digest(review.read_bytes()) == 'B2EA9D0FB7B0A17FC863F0C11C99E1922E9FE4D3BDEC5A58A520C6D6E7AA9F90'
files = sorted(p for p in here.iterdir() if p.is_file())
files += [root / 'build-ci/20261106-derive-writer-join-r3.py',
    root / 'build-ci/20261106-clock-r12-writer-join-r2/source-pins.json',
    root / 'build-ci/20261106-clock-r12-writer-join-r2/phase-graph-input-snapshot.json',
    review, root / 'build-ci/20261106-clock-r12-writer-components-independent-review-r2/source-pins.json']
dest = here / 'source-pins.json'
dest.write_text(json.dumps(dict(schema='R12_WRITER_JOIN_R3_PINS',
    status='ACCEPTED_CONDITIONAL_SOURCE_COMPONENT_ONLY_COLD_GLOBAL_FIT_BINDING_EXECUTION_HOLD',
    no_target_execution=True,
    files=[dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in files]), indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(files), source_pins_sha256=digest(dest.read_bytes()))))
