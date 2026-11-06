"""Fresh staged raw-container verification; never execute an archived subject."""
from pathlib import Path, PurePosixPath
import base64, hashlib, io, json, stat, subprocess, sys, zipfile

ROOT=Path.cwd().resolve()
PACKET='docs/research/artifacts/2026-11-06-clock-r13-runner-census-r2-packed-r1'
RESULT=ROOT/'docs/research/2026-11-06-r13-packed-staged-delivery-result.json'
assert not RESULT.exists()
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
flags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0
def git(*args):
    return subprocess.run(['git',*args],cwd=ROOT,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=flags,check=True).stdout
def staged(path):
    return git('show',':'+path)

assert (ROOT/'AGENTS.md').read_bytes()==(ROOT/'CLAUDE.md').read_bytes()
assert sha((ROOT/'AGENTS.md').read_bytes())=='AFA45D3BAC0FDB892064C8928DDB21375FA0112D2B5840CF2877FAA472ECEB77'
assert sha((ROOT/'tests/musical_audio_call_recipe_tests.cpp').read_bytes())=='97B2A2ABE0BE0CDDFCE173A20E6A6264E837DB751FE204E93B4B094E8E22619C'
freeze=json.loads(staged(PACKET+'/writer-first-green-freeze.json'))
for row in freeze['files']:
    blob=staged(PACKET+'/'+row['file'])
    assert len(blob)==row['bytes'] and sha(blob)==row['sha256']
    assert blob==(ROOT/PACKET/row['file']).read_bytes()
assert staged(PACKET+'/verify_staged.py')==Path(__file__).read_bytes()
packed=json.loads(staged(PACKET+'/source-archive.json'))
raw=base64.b64decode(packed['raw_base64'],validate=True)
assert len(raw)==packed['zip_bytes'] and sha(raw)==packed['zip_sha256']
preserved=ROOT/'build-ci/20261106-r13-census-delivery-r1/attempt-02/complete-first-archive'
assert packed['preserved_tree_path']==preserved.relative_to(ROOT).as_posix()
assert not (ROOT/packed['original_tree_path']).exists()
names=[r['path'] for r in packed['members']]
assert len(names)==412 and len(set(names))==412 and names==sorted(names)
with zipfile.ZipFile(io.BytesIO(raw),'r') as z:
    assert z.namelist()==names
    actual={p.relative_to(preserved).as_posix() for p in preserved.rglob('*') if p.is_file()}
    assert actual==set(names)
    for row in packed['members']:
        name=PurePosixPath(row['path'])
        assert not name.is_absolute() and '..' not in name.parts and '.' not in name.parts
        assert '\\' not in row['path'] and ':' not in row['path']
        info=z.getinfo(row['path'])
        assert stat.S_ISREG(info.external_attr>>16) and not info.is_dir()
        blob=z.read(row['path'])
        assert len(blob)==row['bytes'] and sha(blob)==row['sha256']
        assert blob==(preserved/row['path']).read_bytes()
    manifest_raw=z.read('archive-manifest.json')
    assert sha(manifest_raw)=='AC5AAE3957A58B56A7CB3128E95FFE35C4084DAA5A6024ABD9CC36D26AF17BE6'
    manifest=json.loads(manifest_raw)
    assert len(manifest['files'])==410
    assert set(names)=={'archive-manifest.json','README.md'}|{r['file'] for r in manifest['files']}
    for row in manifest['files']:
        blob=z.read(row['file'])
        assert len(blob)==row['bytes'] and sha(blob)==row['sha256']
        if row['transformation'] in {'COMPLETE_RAW_DIFF_BASE64','COMPLETE_RAW_NON_UTF8_SOURCE_BASE64'}:
            original=base64.b64decode(json.loads(blob)['original_raw_base64'],validate=True)
        elif 'original_raw_base64' in row:
            original=base64.b64decode(row['original_raw_base64'],validate=True)
        else:
            original=blob
        assert len(original)==row['original_bytes'] and sha(original)==row['original_sha256']
        assert original==(ROOT/row['original_path']).read_bytes()
assert staged(PACKET+'/census.md')==(ROOT/'build-ci/20261106-clock-r13-runner-census-r2/census.md').read_bytes()
old='docs/research/artifacts/2026-11-06-r13-census-delivery-r1'
old_freeze_raw=staged(old+'/first-green-freeze.json')
assert sha(old_freeze_raw)=='338035C42654E23D74AC76DC43D17B3BA3B0C99FF49BC99200828E52343B8E4E'
old_freeze=json.loads(old_freeze_raw)
assert len(old_freeze['files'])==7
for row in old_freeze['files']:
    blob=staged(old+'/'+row['file'])
    assert len(blob)==row['bytes'] and sha(blob)==row['sha256']
    assert blob==(ROOT/old/row['file']).read_bytes()
partial=json.loads(staged(old+'/failed-partial-output.json'))
partial_bytes=base64.b64decode(partial['raw_base64'],validate=True)
assert len(partial_bytes)==partial['zip_bytes'] and sha(partial_bytes)==partial['zip_sha256']
with zipfile.ZipFile(io.BytesIO(partial_bytes),'r') as z:
    assert len(partial['files'])==266 and z.namelist()==[r['path'] for r in partial['files']]
    for row in partial['files']:
        name=PurePosixPath(row['path']);assert not name.is_absolute() and '..' not in name.parts
        blob=z.read(row['path'])
        assert len(blob)==row['bytes'] and sha(blob)==row['sha256']
        assert blob==(ROOT/'build-ci/20261106-r13-census-delivery-r1/attempt-01/partial-archive'/row['path']).read_bytes()
for number in [1,2]:
    folder=f'docs/research/artifacts/2026-11-06-r13-staging-failure-r{number}'
    failure=json.loads(staged(folder+'/failure-and-index.json'))
    prefix='original_' if number==1 else 'postfailure_precorrection_'
    index=base64.b64decode(failure[prefix+'git_index_raw_base64'],validate=True)
    assert len(index)==failure[prefix+'git_index_bytes'] and sha(index)==failure[prefix+'git_index_sha256']
    assert sha(staged(folder+'/failed-check-source.py'))=='D71CFF476DA4290FFED8221D2C1135A3E02693B1B3136DE79047F212FAD96BED'
    if number==2:
        output=base64.b64decode(failure['stdout_raw_base64'],validate=True)
        assert failure['current_read_only_reproduction_exit']==2 and len(output)==18659
assert not git('ls-files','--','docs/research/artifacts/2026-11-06-clock-r13-runner-census-r2')
git('diff','--cached','--check')
result=dict(scope='STAGED_LOSSLESS_CONTAINER_DELIVERY_ONLY_NOT_NUMERIC_RESOURCE_OR_TARGET_PROOF',source_rows_reconstructed=410,tree_members_verified=412,old_frozen_delivery_files_verified=7,failed_partial_outputs_roundtripped=266,failed_staging_index_snapshots_roundtripped=2,ignored_logs_preserved=True,unchanged_git_diff_cached_check='PASS',protected_test_and_instructions_unchanged=True,staged_checker_sha256=sha(Path(__file__).read_bytes()),target_execution=False,first_passing_result=True)
RESULT.write_bytes((json.dumps(result,indent=2)+'\n').encode())
print(json.dumps(result))
