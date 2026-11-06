from pathlib import Path
import hashlib
import json

here = Path(__file__).resolve().parent
root = here.parents[1]
assert not (here / 'source-pins.json').exists()
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
result = json.loads((here / 'writer-join.json').read_bytes())
for row in result['pinned_inputs']:
    data = (root / row['path']).read_bytes()
    assert len(data) == row['bytes'] and digest(data) == row['sha256'], row['path']
snapshot = (here / 'phase-graph-input-snapshot.json').read_bytes()
assert digest(snapshot) == result['graph_snapshot_original']['sha256']
files = sorted(p for p in here.iterdir() if p.is_file())
files.append(root / 'build-ci/20261106-derive-writer-join-r2.py')
dest = here / 'source-pins.json'
dest.write_text(json.dumps(dict(schema='R12_WRITER_JOIN_R2_PROPOSAL_PINS',
    status='PRESERVED_PENDING_CRITIC_ORPHAN_RETRY_ARRAY_OBLIGATION_NOT_COMPLETE_FIT_OR_GO',
    no_target_execution=True,
    files=[dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in files]), indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(files), source_pins_sha256=digest(dest.read_bytes()))))
