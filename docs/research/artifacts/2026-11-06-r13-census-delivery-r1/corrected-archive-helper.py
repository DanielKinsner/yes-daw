"""Lossless archive delivery only; preservation never supplies proof acceptance."""
from pathlib import Path
import base64
import hashlib
import json
import sys

assert len(sys.argv) == 4
root = Path(__file__).resolve().parents[1].resolve()
source = (root / 'build-ci' / sys.argv[1]).resolve()
dest = (root / 'docs/research/artifacts' / sys.argv[2]).resolve()
assert source.is_relative_to(root / 'build-ci')
assert dest.is_relative_to(root / 'docs/research/artifacts') and not dest.exists()
digest = lambda raw: hashlib.sha256(raw).hexdigest().upper()
pins_path = source / 'source-pins.json'
assert digest(pins_path.read_bytes()) == sys.argv[3]
pins = json.loads(pins_path.read_bytes())
paths = {p for p in source.rglob('*') if p.is_file()}
for values in pins.values():
    if not isinstance(values, list):
        continue
    for row in values:
        if not isinstance(row, dict) or not {'path', 'bytes', 'sha256'} <= row.keys():
            continue
        path = Path(row['path'])
        if not path.is_absolute():
            path = root / path
        data = path.read_bytes()
        assert len(data) == row['bytes'] and digest(data) == row['sha256'], row['path']
        if path.resolve().is_relative_to(root) and path.suffix.lower() not in {'.dll', '.exe', '.obj', '.lib', '.pdb', '.bin'}:
            paths.add(path)
assert all(p.suffix.lower() in {'.md', '.json', '.jsonl', '.py', '.cs', '.cpp', '.h', '.ps1', '.diff', '.log', '.txt', '.html', '.fragment', '.patch', '.tsv'} for p in paths)
dest.mkdir(parents=True)
records = []
for index, path in enumerate(sorted(paths), 1):
    raw = path.read_bytes()
    try:
        raw.decode('utf-8')
        non_utf8 = False
    except UnicodeDecodeError:
        non_utf8 = True
    relative = Path('sources') / (f'{index:03d}-' + path.name)
    if path.suffix == '.diff' or non_utf8:
        relative = relative.with_suffix(path.suffix + '.original.json')
        data = (json.dumps(dict(original_raw_base64=base64.b64encode(raw).decode('ascii'), original_bytes=len(raw), original_sha256=digest(raw)), indent=2) + '\n').encode()
        mode = 'COMPLETE_RAW_DIFF_BASE64' if path.suffix == '.diff' else 'COMPLETE_RAW_NON_UTF8_SOURCE_BASE64'
    else:
        data = raw.replace(b'\r\n', b'\n')
        data = b'\n'.join(line.rstrip(b' \t') for line in data.split(b'\n'))
        if data:
            data = data.rstrip(b'\n') + b'\n'
        mode = 'BYTE_EXACT' if raw == data else 'LF_DISPLAY_TRAILING_SPACE_SINGLE_EOF_LF_RAW_BASE64_PRESERVED'
    assert b'\r' not in data
    target = dest / relative
    assert len(str(target)) < 250
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
    row = dict(file=relative.as_posix(), original_path=path.relative_to(root).as_posix(), original_bytes=len(raw), original_sha256=digest(raw), bytes=len(data), sha256=digest(data), transformation=mode)
    if data != raw and mode not in {'COMPLETE_RAW_DIFF_BASE64', 'COMPLETE_RAW_NON_UTF8_SOURCE_BASE64'}:
        row['original_raw_base64'] = base64.b64encode(raw).decode('ascii')
    records.append(row)
(dest / 'archive-manifest.json').write_text(json.dumps(dict(schema='PINNED_SOURCE_PACKET_LOSSLESS_DELIVERY_V1', original_pins_sha256=sys.argv[3], original_verdict=pins.get('verdict', pins.get('status', 'SEE_ORIGINAL_SOURCE_PACKET')), scope='Preservation of the named packet only. No new independent source/numeric/resource acceptance, fit, NOFIT, native binding, target execution or feature completion follows.', files=records), indent=2) + '\n', encoding='utf-8', newline='\n')
(dest / 'README.md').write_text('''# Preserved source packet

The manifest maps short display names to every original path, byte count and SHA. All pinned inputs were rehashed before delivery; repository-local textual inputs and the complete named packet are included. Display whitespace changes retain complete raw originals, and diffs retain their complete raw bytes.

The original verdict and qualifications remain authoritative. Archive delivery is not independent proof acceptance, a whole numeric/resource upper, fit/NOFIT, native binding, target execution or editing completion. No target, GUI, input or device is invoked by this delivery helper.
''', encoding='utf-8', newline='\n')
for row in records:
    data = (dest / row['file']).read_bytes()
    assert len(data) == row['bytes'] and digest(data) == row['sha256']
print(json.dumps(dict(files=len(records), manifest_sha256=digest((dest / 'archive-manifest.json').read_bytes()))))
