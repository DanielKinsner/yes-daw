"""One-time ordinary-text native successor preparation; not a target parser."""
from pathlib import Path
import shutil,json,traceback
p=Path(__file__).resolve().parent
a=p/'preparation-attempt-01'
a.mkdir()
for f in [Path(__file__),*sorted((p/'native').glob('*'))]:shutil.copyfile(f,a/f.name)
try:
    for f in sorted((p/'native').glob('*')):
        text=f.read_text().replace('yesdaw_clock_supervisor_r1','yesdaw_clock_supervisor_r2')
        if f.name in ['sup_core.cpp','sup_codec.cpp','sup_io.cpp']:
            text=text.replace('charge(state.meter,','spend(state,')
        f.write_bytes(text.encode())
    (a/'result.json').write_text(json.dumps({'result':'SOURCE_TEXT_PREPARATION_ONLY','target_execution':False})+'\n')
except BaseException:
    (a/'error.txt').write_text(traceback.format_exc())
    raise
print('prepared new native namespace and coordinator-only spend call sites')
