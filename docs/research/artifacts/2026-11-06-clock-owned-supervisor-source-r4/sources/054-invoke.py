"""Journal only own R17 source/file/JSON/integer author/check work."""
from pathlib import Path
import contextlib,hashlib,io,json,runpy,shutil,sys,traceback
HERE=Path(__file__).resolve().parent;ROOT=HERE.parent.parent
SIBLING=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r17-owned-supervisor-source-r1'
OLD=ROOT/'build-ci/20261106-clock-owned-supervisor-source-r3'
BASE=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r16-owned-supervisor-source-r1'
def pin(p):
    h=hashlib.sha256();size=0
    with p.open('rb') as f:
        while b:=f.read(1048576):h.update(b);size+=len(b)
    return dict(path=p.relative_to(ROOT).as_posix(),bytes=size,sha256=h.hexdigest().upper())
def put(p,v):p.write_text(json.dumps(v,indent=2,ensure_ascii=True)+'\n',encoding='utf-8',newline='\n')
assert not (HERE/'freeze.json').exists(),'FROZEN_NO_RERUN'
attempts=HERE/'attempts';attempts.mkdir(exist_ok=True);attempt=attempts/f'attempt-{len(list(attempts.iterdir()))+1:02d}';attempt.mkdir()
new_inputs=[p for p in HERE.iterdir() if p.is_file()]
for p in new_inputs:
    q=attempt/'inputs'/p.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
shared=[OLD/'source-pins.json',OLD/'freeze.json',*sorted((OLD/'native').glob('*')),*sorted(BASE.glob('*'))]
review=ROOT/'build-ci/20261106-r16-supervisor-functional-independent-review-r1'
shared += [review/n for n in ['review.md','review.json','source-pins.json','first-green-review-freeze.json','native-progress-holds.md']]
put(attempt/'shared-immutable-input-pins.json',[pin(p) for p in shared if p.is_file()])
put(attempt/'new-own-input-pins.json',[pin(p) for p in new_inputs])
out,err=io.StringIO(),io.StringIO();rc=0
try:
    with contextlib.redirect_stdout(out),contextlib.redirect_stderr(err):
        sys.dont_write_bytecode=True;runpy.run_path(str(HERE/'build.py'),run_name='__main__')
except BaseException:rc=1;traceback.print_exc(file=err)
(attempt/'stdout.txt').write_text(out.getvalue(),encoding='utf-8',newline='\n')
(attempt/'stderr.txt').write_text(err.getvalue(),encoding='utf-8',newline='\n')
put(attempt/'result.json',dict(exit_code=rc,kind='OWN_SOURCE_FILE_HASH_JSON_INTEGER_ONLY',target_execution=False))
# Exact new/changed subject bytes only; prior immutable archives are referenced,
# not recursively recopied or discarded. This also preserves partial failures.
for p in [*sorted(SIBLING.glob('*')),*sorted((HERE/'native').glob('*')),*[p for p in HERE.iterdir() if p.is_file() and p not in new_inputs]]:
    if p.is_file():
        q=attempt/'outputs'/p.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
if rc==0:
    checks=json.loads((HERE/'checks.json').read_text());assert checks['failed']==0
    files=[pin(p) for p in sorted(HERE.rglob('*')) if p.is_file() and p.name not in ['source-pins.json','freeze.json']]
    files += [pin(p) for p in sorted(SIBLING.glob('*')) if p.is_file()]
    put(HERE/'source-pins.json',dict(schema='R17_COMPLETE_SOURCE_CANDIDATE_PINS_V1',subjects=json.loads((HERE/'correspondence.json').read_text())['subjects'],
        inputs=json.loads((HERE/'input-pins.json').read_text()),files=files,prices_accepted=False,target_execution=False))
    put(HERE/'freeze.json',dict(status='FIRST_COMPLETE_OWN_SOURCE_PASS_FROZEN_UNRUN_FRESH_WHOLE_CRITIC_REQUIRED',checks=checks['count'],failed=0,
        source_pins=pin(HERE/'source-pins.json'),files=len(files),failed_invocations=len(list(attempts.iterdir()))-1,
        Data='UNDEFINED_BOTH',source_prices=False,source_native_resources_ABI_binding_fit_GO=False,postgreen_edits_authorized=False))
print(out.getvalue(),end='');print(err.getvalue(),end='',file=sys.stderr);raise SystemExit(rc)
