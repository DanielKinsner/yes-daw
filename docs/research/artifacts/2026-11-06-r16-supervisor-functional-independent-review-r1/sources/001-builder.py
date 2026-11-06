"""Preserve the complete frozen chunked delivery as text; no subject execution."""
from pathlib import Path
import hashlib, json

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'build-ci/20261106-r16-review-chunked-delivery-r1'
OUT=ROOT/'build-ci/20261106-r16-review-archive-index-r1'
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
def record(p):
    raw=p.read_bytes()
    return dict(path=p.relative_to(ROOT).as_posix(),bytes=len(raw),sha256=sha(raw))
freeze=SOURCE/'first-green-delivery-freeze.json'
assert sha(freeze.read_bytes())=='F7F631B58E917E8CB177E53E4B54B0DBBB56AAE2C23BA5ADFA56BC7224E0508D'
rows=json.loads(freeze.read_bytes())['files']
for row in rows:
    assert record(ROOT/row['path'])==row
paths=sorted(p for p in SOURCE.rglob('*') if p.is_file())
assert len(paths)==37
assert {r['path'] for r in rows}|{freeze.relative_to(ROOT).as_posix()}=={p.relative_to(ROOT).as_posix() for p in paths}
assert not OUT.exists()
OUT.mkdir()
(OUT/'builder.py').write_bytes(Path(__file__).read_bytes())
(OUT/'README.md').write_text('Complete frozen chunked R16 review delivery, including every packaging source/attempt/output. Exact original ZIP bytes are reversibly encoded; no program, ZIP member or candidate checker is executed. This is preservation only.\n',encoding='utf-8',newline='\n')
pins=dict(schema='COMPLETE_FROZEN_CHUNKED_REVIEW_TEXT_ARCHIVE_INDEX',verdict='PRESERVATION_ONLY',inputs=[record(p) for p in paths],target_execution=False)
(OUT/'source-pins.json').write_text(json.dumps(pins,indent=2)+'\n',encoding='utf-8',newline='\n')
seal=dict(files=[record(p) for p in sorted(OUT.iterdir())],target_execution=False)
(OUT/'freeze.json').write_text(json.dumps(seal,indent=2)+'\n',encoding='utf-8',newline='\n')
print(json.dumps(dict(files=len(paths),pins_sha256=sha((OUT/'source-pins.json').read_bytes()),freeze_sha256=sha((OUT/'freeze.json').read_bytes()))))
