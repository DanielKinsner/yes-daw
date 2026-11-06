"""Seal exact source-only protocol delta; no helper/target/toolchain execution."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parents[2]
here = Path(__file__).resolve().parent
assert not (here / 'source-pins.json').exists()
specs = [
    (here / '0091-clock-supervisor-job-termination-diagnostic.md', '4FE0904A932E43F6CED553FFBD56925A13BE31A40CBDEE4D995B6FD84166CF7E'),
    (here / 'protocol-v2.md', '72A2D899C1AA618032EFC8B0937D33A30593BC70D8C333849BFF0FDFFE5D3F83'),
    (root / 'docs/research/clock-supervisor-protocol-v1.md', '7A933415DFAA6B00C76F317F723E68B8E953055B7D3F41AA65CBAAAA536367C2'),
    (root / 'docs/adr/0090-clock-research-owned-process-supervisor.md', '68DC0922BF56F0B9697CF178B69639D5346684EAC0829BEE9DB55F03CB4F6565'),
]
inputs = []
for path, expected in specs:
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest().upper()
    assert digest == expected
    inputs.append({'path': path.relative_to(root).as_posix(), 'bytes': len(raw), 'sha256': digest})
assert not list((root / 'docs/adr').glob('0091*'))
assert len(b'YCSREQ01') == len(b'YCSREQ02') == len(b'YCSRES01') == len(b'YCSRES02') == 8
assert len(b'YESDAW-CLOCK-SUPERVISOR-RUN-V1') == len(b'YESDAW-CLOCK-SUPERVISOR-RUN-V2')
assert 0 < 48 < (1 << 32)
assert 140 + 40 * (4 + 2 * 512) == 41260
assert 140 + 18 * (4 + 2 * 512) == 18644
assert 8 + 16 * 4 + 3 * 8 + 3 * 32 + 2 * 7 * 8 + 4 == 308
assert 41260 + 2 * 18644 + 3 * 308 == 79472
raw = Path(__file__).read_bytes()
inputs.append({'path': Path(__file__).relative_to(root).as_posix(), 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest().upper()})
result = {'schema': 'SOURCE_ONLY_SUPERVISOR_V2_DELTA_PINS_R1', 'status': 'PROPOSED_UNIMPLEMENTED_UNRUN_REQUIRES_FRESH_CRITIC', 'inputs': inputs, 'checks_scope': 'exact documents/unchanged accepted base/available ADR number/byte arithmetic only', 'execution_authority': False}
(here / 'source-pins.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
print(json.dumps({'pins_sha256': hashlib.sha256((here / 'source-pins.json').read_bytes()).hexdigest().upper(), 'source_only': True}))
