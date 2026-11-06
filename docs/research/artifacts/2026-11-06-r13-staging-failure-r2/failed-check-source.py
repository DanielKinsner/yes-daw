"""Read staged delivery bytes headlessly; never load a target source as code."""
from pathlib import Path, PurePosixPath
import base64
import hashlib
import io
import json
import subprocess
import sys
import zipfile

ROOT = Path.cwd().resolve()
assert len(sys.argv)==1 and (ROOT/'.git').exists()
RESULT = ROOT/'docs/research/2026-11-06-r13-staged-delivery-result.json'
assert not RESULT.exists(), 'First passing result must not be overwritten'
FLAGS = subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0

def sha(raw):
    return hashlib.sha256(raw).hexdigest().upper()

def git_bytes(*args):
    return subprocess.run(['git',*args], cwd=ROOT, stdin=subprocess.DEVNULL,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          creationflags=FLAGS, check=True).stdout

def staged(path):
    return git_bytes('show', ':'+path)

assert (ROOT/'AGENTS.md').read_bytes()==(ROOT/'CLAUDE.md').read_bytes()
assert sha((ROOT/'AGENTS.md').read_bytes())=='AFA45D3BAC0FDB892064C8928DDB21375FA0112D2B5840CF2877FAA472ECEB77'
assert sha((ROOT/'tests/musical_audio_call_recipe_tests.cpp').read_bytes())=='97B2A2ABE0BE0CDDFCE173A20E6A6264E837DB751FE204E93B4B094E8E22619C'
own_relative=Path(__file__).resolve().relative_to(ROOT).as_posix()
assert staged(own_relative)==Path(__file__).read_bytes()
archive='docs/research/artifacts/2026-11-06-clock-r13-runner-census-r2'
manifest_raw=staged(archive+'/archive-manifest.json')
assert sha(manifest_raw)=='AC5AAE3957A58B56A7CB3128E95FFE35C4084DAA5A6024ABD9CC36D26AF17BE6'
manifest=json.loads(manifest_raw)
assert len(manifest['files'])==410
raw_modes={'COMPLETE_RAW_DIFF_BASE64','COMPLETE_RAW_NON_UTF8_SOURCE_BASE64'}
for row in manifest['files']:
    blob=staged(archive+'/'+row['file'])
    assert len(blob)==row['bytes'] and sha(blob)==row['sha256']
    original=(ROOT/row['original_path']).read_bytes()
    assert len(original)==row['original_bytes'] and sha(original)==row['original_sha256']
    if row['transformation'] in raw_modes:
        reconstructed=base64.b64decode(json.loads(blob)['original_raw_base64'], validate=True)
    elif 'original_raw_base64' in row:
        reconstructed=base64.b64decode(row['original_raw_base64'], validate=True)
    else:
        reconstructed=blob
    assert reconstructed==original
delivery='docs/research/artifacts/2026-11-06-r13-census-delivery-r1'
freeze_raw=staged(delivery+'/first-green-freeze.json')
assert sha(freeze_raw)=='338035C42654E23D74AC76DC43D17B3BA3B0C99FF49BC99200828E52343B8E4E'
freeze=json.loads(freeze_raw)
assert len(freeze['files'])==7 and freeze['all266_partial_outputs_roundtripped']
for row in freeze['files']:
    blob=staged(delivery+'/'+row['file'])
    assert len(blob)==row['bytes'] and sha(blob)==row['sha256']
    assert blob==(ROOT/delivery/row['file']).read_bytes()
packed=json.loads(staged(delivery+'/failed-partial-output.json'))
zip_bytes=base64.b64decode(packed['raw_base64'], validate=True)
assert len(zip_bytes)==packed['zip_bytes'] and sha(zip_bytes)==packed['zip_sha256']
partial=ROOT/'build-ci/20261106-r13-census-delivery-r1/attempt-01/partial-archive'
assert len(packed['files'])==266
with zipfile.ZipFile(io.BytesIO(zip_bytes),'r') as z:
    assert z.namelist()==[r['path'] for r in packed['files']]
    for row in packed['files']:
        name=PurePosixPath(row['path'])
        assert not name.is_absolute() and '..' not in name.parts
        raw=z.read(row['path'])
        assert len(raw)==row['bytes'] and sha(raw)==row['sha256']
        assert raw==(partial/row['path']).read_bytes()
git_bytes('diff','--cached','--check')
result=dict(scope='STAGED_LOSSLESS_DELIVERY_ONLY_NOT_NUMERIC_RESOURCE_OR_TARGET_PROOF',
            archive_files_verified=410, frozen_delivery_files_verified=7,
            failed_partial_outputs_roundtripped=266, non_UTF8_originals_preserved=2,
            protected_test_and_instructions_unchanged=True,
            own_source_sha256=sha(Path(__file__).read_bytes()),
            all_git_children_CREATE_NO_WINDOW_and_null_stdin=(sys.platform=='win32'),
            target_execution=False, first_passing_result=True)
RESULT.write_bytes((json.dumps(result,indent=2)+'\n').encode())
print(json.dumps(result))
