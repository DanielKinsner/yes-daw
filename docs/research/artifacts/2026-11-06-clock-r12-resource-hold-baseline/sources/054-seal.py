"""Record this already checked HOLD baseline; no target execution."""
from pathlib import Path
from hashlib import sha256
import json
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
def pin(p):return dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=sha256(p.read_bytes()).hexdigest().upper())
e=json.loads((HERE/'mechanical-evidence.json').read_bytes())
assert e['status']=='PASS_CONSISTENCY_ONLY' and not e['failures']
assert not (HERE/'source-pins.json').exists(), 'One first seal only; preserve it.'
for row in e['input_pins']:assert pin(ROOT/row['path'])==row,row['path']
authored=[pin(p) for p in sorted(HERE.iterdir()) if p.is_file() and p.name!='source-pins.json']
assert all(b'\r' not in (ROOT/r['path']).read_bytes() for r in authored)
out=dict(schema='R12_COMPLETE_HOLD_BASELINE_SOURCE_PINS_R1',verdict='HOLD_NO_COMPLETE_NUMERIC_UPPER_NO_FIT_OR_NOFIT',
 scope='Immutable resource baseline disposition only;13 families with selected source/data arithmetic and explicit remaining V/P/native/cold/A/profile obligations. Ordinary files/JSON/hash/ownPython arithmetic only; no target/GUI/environment/Git/CI execution.',
 input_admission='These are qualification/evidence/ordinary source inputs, not new authored Clock compiler/runtime inputs or archive-copy admissions. Final canonical union/profile remains pending.',
 authored_files=authored,input_pins=e['input_pins'],
 first_evaluation_preserved='evaluation.json872E24B3... remains byte-exact; successors explicitly compose corrections.',
 frozen_hash_theorem='HelperBACD6D7F... and result587CB936... preserved; independently accepted freeze1F7FC534... includes sixth HashHelpers input.',
 verification=dict(consistency_checks=e['check_count'],failures=0,meaning='Artifact/source consistency, not independent whole resource acceptance or runtime measurement.'))
(HERE/'source-pins.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8',newline='\n')
for n in ['baseline.md','baseline.json','source-pins.json']:print(json.dumps(pin(HERE/n)))
