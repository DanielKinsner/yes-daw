"""Seal a read-only source finding; no target source is parsed or executed."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[2]
here = Path(__file__).resolve().parent
assert not (here / 'source-pins.json').exists()
paths = [
    ('build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1/run-clock.ps1', '9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788'),
    ('build-ci/20261105-controller-environment-writer-closure-r2/primary/src/System.Management.Automation/engine/runtime/Binding/Binders.cs', 'CCA6BC97C2CDC16FDC1EB612FABCCCEA919629BC4C04F952C00A458FF7C9F6AC'),
    ('build-ci/20261105-controller-environment-writer-closure-r2/primary/src/System.Management.Automation/engine/MshObject.cs', 'E8553EE8EC58BD3E50DEAE8EACBC43E2E4C73B6A9E52EE01FC3DABDA7795E40B'),
    ('build-ci/20261105-controller-environment-writer-closure-r2/primary/src/System.Management.Automation/engine/CoreAdapter.cs', '59C929C3F7E8D7F053E46FBACB2FAD1105ABF62D1FB292414486D8EC1326023C'),
]
rows = []
for name, expected in paths:
    raw = (root / name).read_bytes()
    digest = hashlib.sha256(raw).hexdigest().upper()
    assert digest == expected, name
    rows.append({'path': name, 'bytes': len(raw), 'sha256': digest})
for path in [here / 'finding.md', Path(__file__)]:
    raw = path.read_bytes()
    rows.append({'path': path.relative_to(root).as_posix(), 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest().upper()})
result = {'schema': 'R13_BOOTSTRAP_STATIC_FIELD_SOURCE_FINDING_R1', 'scope': 'source-level correspondence defect; no runtime failure observed', 'inputs': rows, 'R14_correction_required': '[Type]::EmptyTypes', 'runtime_tested': False, 'target_execution': False}
(here / 'source-pins.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps({'preserved_inputs': len(rows), 'pins_sha256': hashlib.sha256((here / 'source-pins.json').read_bytes()).hexdigest().upper()}))
