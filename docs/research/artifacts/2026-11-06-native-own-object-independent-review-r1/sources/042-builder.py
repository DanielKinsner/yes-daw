"""Source-preserving delivery index; no subject parser, import or execution."""
from pathlib import Path
import base64, hashlib, json, re, sys, zlib

ROOT=Path(__file__).resolve().parents[1]
assert len(sys.argv) in (4,5)
source=(ROOT/'build-ci'/sys.argv[1]).resolve()
out=(ROOT/'build-ci'/sys.argv[2]).resolve()
assert source.is_relative_to(ROOT/'build-ci') and out.is_relative_to(ROOT/'build-ci')
assert source!=out and not source.is_relative_to(out) and not out.is_relative_to(source)
assert source.is_dir() and not out.exists()
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
def digest_file(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        while True:
            block=f.read(1048576)
            if not block:return h.hexdigest().upper()
            h.update(block)
def dump(path,value):
    path.write_bytes((json.dumps(value,indent=2,ensure_ascii=True)+'\n').encode())
pin_path=source/'source-pins.json'
assert digest_file(pin_path)==sys.argv[3]
pins=json.loads(pin_path.read_bytes())
own_paths={p for p in source.rglob('*') if p.is_file()}
paths=set(own_paths)
extra_source=None
if len(sys.argv)==5:
    extra_source=(ROOT/'build-ci'/sys.argv[4]).resolve()
    assert extra_source.is_relative_to(ROOT/'build-ci') and extra_source.is_dir()
    assert extra_source!=out and not extra_source.is_relative_to(out)
    paths.update(p for p in extra_source.rglob('*') if p.is_file())
external=[]
for values in pins.values():
    if not isinstance(values,list):continue
    for row in values:
        if not isinstance(row,dict) or not {'path','bytes','sha256'}<=row.keys():continue
        p=Path(row['path'])
        if not p.is_absolute():p=ROOT/p
        assert p.stat().st_size==row['bytes'] and digest_file(p)==row['sha256']
        if p.resolve().is_relative_to(ROOT):paths.add(p)
        else:external.append(row)
allowed={'.md','.json','.jsonl','.py','.cs','.cpp','.h','.ps1','.diff','.log','.txt','.html','.fragment'}
text_rows=[];raw_rows=[]
for path in sorted(paths,key=lambda p:p.relative_to(ROOT).as_posix()):
    assert not path.is_symlink()
    raw=path.read_bytes();reason=None
    if path.suffix.lower() not in allowed:reason='NON_TEXT_EXTENSION_RAW_PRESERVATION'
    else:
        try:raw.decode('utf-8')
        except UnicodeDecodeError:reason='NON_UTF8_RAW_PRESERVATION'
    if reason is None and re.search(rb'^[ \t]* \t',raw,re.M):reason='LEADING_MIXED_INDENT_RAW_PRESERVATION'
    row=dict(path=path.relative_to(ROOT).as_posix(),bytes=len(raw),sha256=sha(raw))
    if reason is None:text_rows.append(row)
    else:
        compressed=zlib.compress(raw,6)
        row.update(reason=reason,encoding='ZLIB_BASE64_RAW_BYTES',encoded_bytes=len(compressed),encoded_sha256=sha(compressed),raw_base64=base64.b64encode(compressed).decode('ascii'))
        assert zlib.decompress(base64.b64decode(row['raw_base64'],validate=True))==raw
        raw_rows.append(row)
out.mkdir()
(out/'builder.py').write_bytes(Path(__file__).read_bytes())
dump(out/'raw-payloads.json',dict(schema='RAW_PRESERVATION_PAYLOADS_V1',rows=raw_rows,scope='Opaque raw preservation; cached bytecode is not an execution input or authority'))
dump(out/'source-pins.json',dict(schema='RAW_SOURCE_WRAPPER_R1',verdict='PRESERVATION_ONLY_ORIGINAL_CANDIDATE_UNACCEPTED',source_packet=source.relative_to(ROOT).as_posix(),source_packet_pins_sha256=sys.argv[3],source_packet_files=len(own_paths),associated_attempt_folder=extra_source.relative_to(ROOT).as_posix() if extra_source else None,textual_and_source_rows=len(text_rows),raw_preserved_rows=len(raw_rows),inputs=text_rows,external_identity_rows_verified_not_byte_archived=external,target_execution=False))
(out/'README.md').write_bytes(('''# Lossless source delivery index

The original packet, every helper/proof/attempt source and selected repository-local pinned inputs are preserved. Text rows enter the ordinary lossless source archive. Other byte sequences, including generated non-source caches, are retained in raw-payloads.json as compressed Base64 with exact length/hash; they are not executed. Original pins, freezes and meanings remain unchanged. This is no functional/numeric/resource/native/binding or execution acceptance. External inputs are identity-verified, not newly byte-archived.
''').encode())
for row in text_rows+raw_rows:
    p=ROOT/row['path']
    assert p.stat().st_size==row['bytes'] and digest_file(p)==row['sha256']
records=[]
for p in sorted(out.iterdir()):
    raw=p.read_bytes();records.append(dict(file=p.name,bytes=len(raw),sha256=sha(raw)))
dump(out/'freeze.json',dict(scope='FIRST_GREEN_LOSSLESS_SOURCE_INDEX_ONLY',files=records,target_execution=False))
print(json.dumps(dict(source_packet_files=len(own_paths),text_rows=len(text_rows),raw_rows=len(raw_rows),wrapper_pins_sha256=digest_file(out/'source-pins.json'),freeze_sha256=digest_file(out/'freeze.json'))))
