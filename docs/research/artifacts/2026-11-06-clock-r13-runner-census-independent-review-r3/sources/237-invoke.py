"""Bounded own-source invocation journal. Never imports any subject/old helper."""
import contextlib
import hashlib
import io
import json
from pathlib import Path
import runpy
import shutil
import sys
import traceback

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent

def record(p):
    data = p.read_bytes()
    return {'path':p.relative_to(ROOT).as_posix(), 'bytes':len(data),
            'sha256':hashlib.sha256(data).hexdigest().upper()}

def dump(path, value):
    path.write_text(json.dumps(value, indent=2, ensure_ascii=True)+'\n', encoding='utf-8', newline='\n')

assert not (HERE/'whole-freeze.json').exists(), 'FROZEN_NO_RERUN'
attempts = HERE/'attempts'
attempts.mkdir(exist_ok=True)
run = attempts/f'attempt-{len(list(attempts.iterdir()))+1:02d}'
run.mkdir()
subject = ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
data_inputs = [ROOT/'build-ci/20261106-clock-r13-runner-census-r2'/n for n in
               ['source-inventory.json','source-pins.json','census.json','body-components.json','phase-schedules.json','source-graphs.json','first-green-whole-source-freeze.json','argument.md']]
data_inputs += [ROOT/'build-ci/20261106-clock-r13-runner-census-independent-review-r2'/n for n in
                ['review.md','review.json','source-pins.json','first-complete-own-pass-freeze.json']]
data_inputs += [ROOT/'build-ci/20261106-r13-main-caller-ledger-r3'/n for n in ['caller-ledger.json','argument.md','source-pins.json']]
data_inputs += [subject/n for n in ['run-clock.ps1','source-manifest.json','README.md','source-costs.h','clock_stage.cpp']]
own_before = [p for p in HERE.iterdir() if p.is_file()]
for p in own_before+data_inputs:
    dest = run/'inputs'/p.relative_to(ROOT)
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(p,dest)
dump(run/'input-pins.json',[record(p) for p in own_before+data_inputs])
out, err = io.StringIO(), io.StringIO()
rc=0
try:
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        sys.dont_write_bytecode=True
        runpy.run_path(str(HERE/'rebuild.py'),run_name='__main__')
except BaseException:
    rc=1
    traceback.print_exc(file=err)
(run/'stdout.txt').write_text(out.getvalue(),encoding='utf-8',newline='\n')
(run/'stderr.txt').write_text(err.getvalue(),encoding='utf-8',newline='\n')
dump(run/'result.json',{'exit_code':rc,'kind':'OWN_FILE_JSON_INTEGER_ONLY','subject_execution':False})
if rc==0:
    assert (HERE/'checks.json').exists()
    assert json.loads((HERE/'checks.json').read_text())['failed']==0
    rows=[record(p) for p in sorted(HERE.rglob('*')) if p.is_file() and p.name not in ['source-pins.json','whole-freeze.json']]
    # The complete input matrix is freshly checked by rebuild.py. These are
    # retained independently of own output pins to avoid any seal cycle.
    inputs=json.loads((HERE/'input-pins.json').read_text())
    dump(HERE/'source-pins.json',{'schema':'R13_R3_COMPLETE_SOURCE_PROPOSAL_PINS_V1','inputs':inputs,'own_files':rows,
                                'prices_accepted':False,'Data':False,'execution_GO':False})
    dump(HERE/'whole-freeze.json',{'schema':'R13_R3_FIRST_COMPLETE_OWN_GREEN_FREEZE_V1',
        'checks':json.loads((HERE/'checks.json').read_text())['count'],
        'failed_invocations':len(list(attempts.iterdir()))-1,
        'source_pins':record(HERE/'source-pins.json'),'own_file_count':len(rows),
        'all_helpers_and_oracles_frozen':True,'postgreen_edits_authorized':False,
        'source_price_acceptance':False,'source_functionality':'HOLD_Type_array_EmptyTypes',
        'resource_fit':False,'combined_U':False,'Data':False,'execution_GO':False})
print(out.getvalue(),end='')
print(err.getvalue(),end='',file=sys.stderr)
raise SystemExit(rc)
