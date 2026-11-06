from pathlib import Path
import base64
import hashlib
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
digest = lambda b: hashlib.sha256(b).hexdigest().upper()
assert digest((root / 'tests/musical_audio_call_recipe_tests.cpp').read_bytes()) == '97B2A2ABE0BE0CDDFCE173A20E6A6264E837DB751FE204E93B4B094E8E22619C'
assert (root / 'AGENTS.md').read_bytes() == (root / 'CLAUDE.md').read_bytes()
assert digest((root / 'AGENTS.md').read_bytes()) == 'AFA45D3BAC0FDB892064C8928DDB21375FA0112D2B5840CF2877FAA472ECEB77'
assert len(sys.argv) == 2
folder = root / sys.argv[1]
assert folder.resolve().is_relative_to((root / 'docs/research/artifacts').resolve())
manifest = json.loads((folder / 'archive-manifest.json').read_bytes())
for row in manifest['files']:
    relative = (folder / row['file']).relative_to(root).as_posix()
    blob = subprocess.run(['git', 'show', ':' + relative], cwd=root, check=True, stdout=subprocess.PIPE).stdout
    assert len(blob) == row['bytes'] and digest(blob) == row['sha256'], relative
    original = (root / row['original_path']).read_bytes()
    assert len(original) == row['original_bytes'] and digest(original) == row['original_sha256'], relative
    if row['transformation'] in {'COMPLETE_RAW_DIFF_BASE64', 'COMPLETE_RAW_NON_UTF8_SOURCE_BASE64'}:
        reconstructed = base64.b64decode(json.loads(blob)['original_raw_base64'], validate=True)
    elif 'original_raw_base64' in row:
        reconstructed = base64.b64decode(row['original_raw_base64'], validate=True)
    else:
        reconstructed = blob
    assert reconstructed == original, relative
subprocess.run(['git', 'diff', '--cached', '--check'], cwd=root, check=True)
print(json.dumps(dict(verified_staged_archive_files=len(manifest['files']), protected_test_and_instructions_unchanged=True)))
