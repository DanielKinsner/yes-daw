from pathlib import Path
import hashlib
import json

here = Path(__file__).resolve().parent
root = here.parents[1]
assert not (here / 'source-pins.json').exists()
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
component = json.loads((here / 'small-owner-components.json').read_bytes())
for row in component['input_pins']:
    data = (root / row['path']).read_bytes()
    assert len(data) == row['bytes'] and digest(data) == row['sha256'], row['path']
files = sorted(p for p in here.iterdir() if p.is_file())
files += [root / 'build-ci' / name for name in ['20261106-fetch-small-runtime-owners.py', '20261106-continue-small-runtime-owner-source.py', '20261106-fetch-small-runtime-owners-supplement.py', '20261106-fetch-filetime-source.py']]
dest = here / 'source-pins.json'
dest.write_text(json.dumps(dict(schema='SELECTED_SMALL_RUNTIME_OWNER_COMPONENT_PINS_R1',
    status='SEALED_SOURCE_PROPOSAL_FOR_INDEPENDENT_REVIEW_NO_TARGET_OR_ALLOCATION_PROBE',
    files=[dict(path=p.relative_to(root).as_posix(), bytes=p.stat().st_size, sha256=digest(p.read_bytes())) for p in files]), indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps(dict(files=len(files), source_pins_sha256=digest(dest.read_bytes()))))
