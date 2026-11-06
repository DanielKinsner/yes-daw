"""Pack the exact frozen display archive, without recoding any member."""
from pathlib import Path, PurePosixPath
import base64, hashlib, io, json, zipfile

ROOT=Path.cwd().resolve()
HERE=Path(__file__).resolve().parent
SOURCE=ROOT/'docs/research/artifacts/2026-11-06-clock-r13-runner-census-r2'
OUT=ROOT/'docs/research/artifacts/2026-11-06-clock-r13-runner-census-r2-packed-r1'
CRITIC=ROOT/'build-ci/20261106-r13-delivery-independent-critic-r1'
CENSUS=ROOT/'build-ci/20261106-clock-r13-runner-census-r2'
assert not OUT.exists()
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
def dump(path,value):
    path.write_bytes((json.dumps(value,indent=2,ensure_ascii=True)+'\n').encode())

assert sha((CRITIC/'review.json').read_bytes())=='E6EB1AC0841DD986AE8CAA2D20E9EE8898287377BD3A887AE1E55CDB678EF4B3'
assert sha((CRITIC/'freeze.json').read_bytes())=='9203B89D6236193F2C654CF588EEF35D62BA117F45EAB684212A647EBAFFD626'
manifest_raw=(SOURCE/'archive-manifest.json').read_bytes()
assert sha(manifest_raw)=='AC5AAE3957A58B56A7CB3128E95FFE35C4084DAA5A6024ABD9CC36D26AF17BE6'
manifest=json.loads(manifest_raw)
expected={'archive-manifest.json','README.md'}|{r['file'] for r in manifest['files']}
paths=sorted(p for p in SOURCE.rglob('*') if p.is_file())
assert len(paths)==412 and {p.relative_to(SOURCE).as_posix() for p in paths}==expected
assert all(not p.is_symlink() for p in paths)
buffer=io.BytesIO();members=[]
with zipfile.ZipFile(buffer,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for path in paths:
        name=path.relative_to(SOURCE).as_posix();pure=PurePosixPath(name)
        assert not pure.is_absolute() and '..' not in pure.parts and '\\' not in name
        raw=path.read_bytes()
        info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0))
        info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100600<<16
        z.writestr(info,raw)
        members.append(dict(path=name,bytes=len(raw),sha256=sha(raw)))
zip_bytes=buffer.getvalue()
with zipfile.ZipFile(io.BytesIO(zip_bytes),'r') as z:
    assert z.namelist()==[r['path'] for r in members]
    for row in members:
        raw=z.read(row['path'])
        assert len(raw)==row['bytes'] and sha(raw)==row['sha256']
        assert raw==(SOURCE/row['path']).read_bytes()
OUT.mkdir()
dump(OUT/'source-archive.json',dict(schema='LOSSLESS_FROZEN_SOURCE_TREE_ZIP_BASE64_V1',original_tree_path=SOURCE.relative_to(ROOT).as_posix(),preserved_tree_path='build-ci/20261106-r13-census-delivery-r1/attempt-02/complete-first-archive',original_manifest_sha256=sha(manifest_raw),members=members,zip_bytes=len(zip_bytes),zip_sha256=sha(zip_bytes),raw_base64=base64.b64encode(zip_bytes).decode('ascii'),scope='Raw preservation only; no numeric/resource/native/binding/target execution acceptance'))
copies=[(Path(__file__),'writer.py'),(HERE/'verify_staged.py','verify_staged.py'),(CENSUS/'census.md','census.md'),(CRITIC/'critic.py','strategy-critic.py'),(CRITIC/'review.json','strategy-review.json'),(CRITIC/'freeze.json','strategy-freeze.json')]
for source,name in copies:
    (OUT/name).write_bytes(source.read_bytes())
dump(OUT/'receipt.json',dict(scope='LOSSLESS_PRESENTATION_SUCCESSOR_ONLY',original_archive_members=410,complete_tree_members=412,original_manifest_sha256=sha(manifest_raw),candidate_pins_sha256='DFC0F598BF3CECC00484A3772B86819BA2EF71A11078B26B30ED73ECA8BB4B93',candidate_whole_freeze_sha256='F0C4439F557C5E59DBB1096AA0DACFE9DB62EDAD944E68AD52D14B20C4876BB6',strategy_critic_checks=422,all_original_members_unchanged=True,new_staged_verification='PENDING_SEPARATE_CHECK',target_execution=False))
records=[]
for path in sorted(OUT.iterdir()):
    raw=path.read_bytes();records.append(dict(file=path.name,bytes=len(raw),sha256=sha(raw)))
dump(OUT/'writer-first-green-freeze.json',dict(scope='FIRST_GREEN_WRITER_SOURCE_AND_RAW_CONTAINER_ONLY_STAGED_CHECK_PENDING',files=records,target_execution=False))
print(json.dumps(dict(tree_members=412,source_members=410,zip_bytes=len(zip_bytes),writer_freeze_sha256=sha((OUT/'writer-first-green-freeze.json').read_bytes()),container_sha256=sha((OUT/'source-archive.json').read_bytes()))))
