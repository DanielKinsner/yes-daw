"""Lossless preservation of one archive failure and its corrected delivery."""
from pathlib import Path
import base64
import hashlib
import io
import json
import zipfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT/'docs/research/artifacts/2026-11-06-r13-census-delivery-r1'
PARTIAL = HERE/'attempt-01/partial-archive'
CENSUS = ROOT/'build-ci/20261106-clock-r13-runner-census-r2'
ARCHIVE = ROOT/'docs/research/artifacts/2026-11-06-clock-r13-runner-census-r2'

def sha(data):
    return hashlib.sha256(data).hexdigest().upper()

def dump(path, value):
    path.write_bytes((json.dumps(value, indent=2, ensure_ascii=True)+'\n').encode())

assert not OUT.exists()
assert sha((ARCHIVE/'archive-manifest.json').read_bytes()) == 'AC5AAE3957A58B56A7CB3128E95FFE35C4084DAA5A6024ABD9CC36D26AF17BE6'
assert sha((CENSUS/'source-pins.json').read_bytes()) == 'DFC0F598BF3CECC00484A3772B86819BA2EF71A11078B26B30ED73ECA8BB4B93'
assert sha((CENSUS/'first-green-whole-source-freeze.json').read_bytes()) == 'F0C4439F557C5E59DBB1096AA0DACFE9DB62EDAD944E68AD52D14B20C4876BB6'
paths = sorted(p for p in PARTIAL.rglob('*') if p.is_file())
assert len(paths) == 266
members = []
buffer = io.BytesIO()
with zipfile.ZipFile(buffer, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for path in paths:
        raw = path.read_bytes()
        name = path.relative_to(PARTIAL).as_posix()
        info = zipfile.ZipInfo(name, date_time=(1980,1,1,0,0,0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = (0o100600 << 16)
        z.writestr(info, raw)
        members.append(dict(path=name, bytes=len(raw), sha256=sha(raw)))
zip_bytes = buffer.getvalue()
OUT.mkdir()
dump(OUT/'failed-partial-output.json', dict(format='COMPLETE_ZIP_BASE64_NO_EXTRACTION', original_root=PARTIAL.relative_to(ROOT).as_posix(), files=members, zip_bytes=len(zip_bytes), zip_sha256=sha(zip_bytes), raw_base64=base64.b64encode(zip_bytes).decode('ascii')))
sources = [
    (Path(__file__), 'preserve_delivery.py'),
    (HERE/'attempt-01/archive-research-source-packet-r1.py', 'failed-archive-helper.py'),
    (HERE/'attempt-01/failure.json', 'failed-attempt.json'),
    (ROOT/'build-ci/20261106-archive-research-source-packet-r1.py', 'corrected-archive-helper.py'),
    (ROOT/'build-ci/20261106-verify-staged-research-evidence-r1.py', 'staged-verifier.py'),
]
records = []
for source, name in sources:
    raw = source.read_bytes()
    (OUT/name).write_bytes(raw)
    records.append(dict(file=name, original_path=source.relative_to(ROOT).as_posix(), bytes=len(raw), sha256=sha(raw)))
assert records[1]['sha256']=='16387F692BF15FFA55F40A140B6701F51D32C4A0B776B894F9166112930F0480'
assert records[3]['sha256']=='822E0AF600EDFB2348B0F1046E1D5BC587C276D941838CD0D809C5AD702258F0'
assert records[4]['sha256']=='E511EF0C5D57D3C732B3CB71F418051E020FE15BB1F07738ECC75D0DD86F247A'
stored = json.loads((OUT/'failed-partial-output.json').read_bytes())
decoded = base64.b64decode(stored['raw_base64'], validate=True)
assert len(decoded)==stored['zip_bytes'] and sha(decoded)==stored['zip_sha256']
with zipfile.ZipFile(io.BytesIO(decoded), 'r') as z:
    assert z.namelist()==[r['path'] for r in members]
    for row in members:
        raw = z.read(row['path'])
        assert len(raw)==row['bytes'] and sha(raw)==row['sha256']
        assert raw==(PARTIAL/row['path']).read_bytes()
archive_manifest = json.loads((ARCHIVE/'archive-manifest.json').read_bytes())
bad_rows = [r for r in archive_manifest['files'] if r['transformation']=='COMPLETE_RAW_NON_UTF8_SOURCE_BASE64']
assert len(bad_rows)==2 and archive_manifest['files'][266] in bad_rows
for row in bad_rows:
    decoded = base64.b64decode(json.loads((ARCHIVE/row['file']).read_bytes())['original_raw_base64'], validate=True)
    assert decoded==(ROOT/row['original_path']).read_bytes()
dump(OUT/'receipt.json', dict(scope='DELIVERY_ONLY_NO_NUMERIC_RESOURCE_OR_RUNTIME_ACCEPTANCE', first_archive_exit=1, corrected_archive_exit=0, failed_partial_files_preserved_and_roundtripped=266, raw_non_UTF8_sources_preserved=bad_rows, census_archive_files=410, census_archive_manifest_sha256=sha((ARCHIVE/'archive-manifest.json').read_bytes()), candidate_pins_unchanged=True, target_execution=False, files=records))
output_records=[]
for p in sorted(OUT.iterdir()):
    raw=p.read_bytes()
    output_records.append(dict(file=p.name, bytes=len(raw), sha256=sha(raw)))
dump(OUT/'first-green-freeze.json', dict(scope='LOSSLESS_DELIVERY_ONLY', files=output_records, all266_partial_outputs_roundtripped=True, target_execution=False))
print(json.dumps(dict(partial_files_roundtripped=266, non_UTF8_sources_roundtripped=2, archive_files=410, output_files=len(output_records)+1, freeze_sha256=sha((OUT/'first-green-freeze.json').read_bytes()))))
