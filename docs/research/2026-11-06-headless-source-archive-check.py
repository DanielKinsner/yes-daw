"""Verify staged source preservation; no archived source is run or parsed as code."""
from pathlib import Path, PurePosixPath
import base64, hashlib, json, subprocess, sys, zlib

ROOT=Path.cwd().resolve()
assert len(sys.argv)==3 and (ROOT/'.git').exists()
folder=(ROOT/sys.argv[1]).resolve()
assert folder.is_relative_to(ROOT/'docs/research/artifacts')
relative=folder.relative_to(ROOT).as_posix()
result_path=ROOT/'docs/research/verification-results'/(folder.name+'.json')
assert not result_path.exists(), 'Do not overwrite first passing result'
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
flags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0
def git(*args):
    return subprocess.run(['git',*args],cwd=ROOT,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=flags,check=True).stdout
def staged(path):
    return git('show',':'+path)
own=Path(__file__).resolve()
assert staged(own.relative_to(ROOT).as_posix())==own.read_bytes()
assert (ROOT/'AGENTS.md').read_bytes()==(ROOT/'CLAUDE.md').read_bytes()
assert sha((ROOT/'AGENTS.md').read_bytes())=='AFA45D3BAC0FDB892064C8928DDB21375FA0112D2B5840CF2877FAA472ECEB77'
assert sha((ROOT/'tests/musical_audio_call_recipe_tests.cpp').read_bytes())=='97B2A2ABE0BE0CDDFCE173A20E6A6264E837DB751FE204E93B4B094E8E22619C'
manifest_raw=staged(relative+'/archive-manifest.json')
assert sha(manifest_raw)==sys.argv[2]
manifest=json.loads(manifest_raw)
expected={relative+'/archive-manifest.json',relative+'/README.md'}
for row in manifest['files']:
    name=PurePosixPath(row['file'])
    assert not name.is_absolute() and '..' not in name.parts and '\\' not in row['file'] and ':' not in row['file']
    expected.add(relative+'/'+row['file'])
tracked={p.decode('utf-8') for p in git('ls-files','-z','--',relative).split(b'\0') if p}
assert tracked==expected, 'Every intended archive member, including ignored logs, must be staged'
raw_count=0
for row in manifest['files']:
    blob=staged(relative+'/'+row['file'])
    assert len(blob)==row['bytes'] and sha(blob)==row['sha256']
    original_path=(ROOT/row['original_path']).resolve()
    assert original_path.is_relative_to(ROOT)
    original=original_path.read_bytes()
    assert len(original)==row['original_bytes'] and sha(original)==row['original_sha256']
    if row['transformation'] in {'COMPLETE_RAW_DIFF_BASE64','COMPLETE_RAW_NON_UTF8_SOURCE_BASE64'}:
        restored=base64.b64decode(json.loads(blob)['original_raw_base64'],validate=True)
    elif 'original_raw_base64' in row:
        restored=base64.b64decode(row['original_raw_base64'],validate=True)
    else:
        restored=blob
    assert restored==original
    if original_path.name=='raw-payloads.json':
        payloads=json.loads(restored)
        assert payloads['schema']=='RAW_PRESERVATION_PAYLOADS_V1'
        for item in payloads['rows']:
            encoded=base64.b64decode(item['raw_base64'],validate=True)
            assert len(encoded)==item['encoded_bytes'] and sha(encoded)==item['encoded_sha256']
            decoder=zlib.decompressobj()
            raw=decoder.decompress(encoded,item['bytes']+1)
            assert decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail
            assert decoder.flush()==b''
            assert len(raw)==item['bytes'] and sha(raw)==item['sha256']
            raw_path=(ROOT/item['path']).resolve()
            assert raw_path.is_relative_to(ROOT) and raw==raw_path.read_bytes()
            raw_count+=1
git('diff','--cached','--check')
result=dict(scope='HEADLESS_STAGED_LOSSLESS_SOURCE_DELIVERY_ONLY',archive_manifest_sha256=sys.argv[2],archive_source_rows_verified=len(manifest['files']),additional_opaque_raw_payloads_roundtripped=raw_count,complete_staged_membership=True,git_whitespace_check='PASS',protected_test_and_instructions_unchanged=True,Git_CREATE_NO_WINDOW_and_null_stdin=(sys.platform=='win32'),checker_source_sha256=sha(own.read_bytes()),target_execution=False,numeric_resource_native_binding_acceptance=False,first_passing_result=True)
result_path.parent.mkdir(parents=True,exist_ok=True)
result_path.write_bytes((json.dumps(result,indent=2)+'\n').encode())
print(json.dumps(result))
