"""Prepare one new native candidate by exact source transforms; no compiler/API."""
from pathlib import Path
import hashlib,json,shutil,contextlib,traceback,sys
OUT=Path(__file__).resolve().parent;ROOT=OUT.parents[1]
BASE=ROOT/'build-ci/20261106-clock-owned-supervisor-source-r2'
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def dump(p,v):p.write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8',newline='\n')
def replace(s,old,new):
 if s.count(old)!=1:raise ValueError(('exact source seam',old,s.count(old)))
 return s.replace(old,new)
def main():
 if (OUT/'freeze.json').exists():raise RuntimeError('FROZEN')
 assert sha((BASE/'source-pins.json').read_bytes())=='5806F35FE26CC1CB2A43B7E49AD6CEBEDE6C28EE7ABAC6BE52CDBE77292FDDB1'
 assert sha((BASE/'freeze.json').read_bytes())=='895F4F1BB04F955E9E40BFDC29AC29AE15AB844ECE21AF068D3DFAEF16914169'
 native={p.name:p.read_text(encoding='utf-8').replace('yesdaw_clock_supervisor_r2','yesdaw_clock_supervisor_r3') for p in (BASE/'native').iterdir() if p.suffix in ['.cpp','.h']}
 h=native['sup.h']
 h=replace(h,'EnvironmentCell, Owner,\n    WorkerStart','EnvironmentCell, Owner, CaptureSetup, TargetResume,\n    WorkerStart')
 h=replace(h,'bool charge(Meter&, Family, U64 = 1) noexcept;','bool charge(Meter&, Family, U64 = 1) noexcept;\nbool admit_body(Meter&, Family) noexcept;\nbool admit_body(State&, Family) noexcept;')
 native['sup.h']=h
 core=native['sup_core.cpp']
 lo=core.index('bool admit_read(Meter& meter) noexcept {')
 hi=core.index('void request_tail(Meter& meter) noexcept {',lo)
 core=core[:lo]+'''bool admit_body(Meter& meter,Family family) noexcept {
    // Fresh body/setup admission is prefix-only. This ticket and BeginTail use
    // the SAME lock/cut state; a tail-capable spend is never a body ticket.
    AcquireSRWLockExclusive(&meter.lock);
    bool ok=!meter.tail && meter.valid && !meter.refused;
    if(ok) {
        const U32 index=static_cast<U32>(family);
        if(index>=static_cast<U32>(Family::Count)) { meter.valid=false;ok=false; }
        else {
            const U64 price=Data.prices[index];
            if(price==0) { meter.valid=false;ok=false; }
            else if(meter.used>meter.limit || price>meter.limit-meter.used) { meter.refused=true;ok=false; }
            else meter.used+=price;
        }
    }
    if(!ok && !meter.tail) { meter.tail=true;meter.frozen_prefix=meter.used; }
    ReleaseSRWLockExclusive(&meter.lock);
    return ok;
}
bool admit_body(State& state,Family family) noexcept {
    // Only the coordinator observes terminal causes. Already admitted groups
    // may finish after a concurrent cut from their mandatory tail sponsor.
    if(state.stop_body) { begin_tail(state);return false; }
    if(admit_body(state.meter,family)) return true;
    begin_tail(state);
    return false;
}
bool admit_read(Meter& meter) noexcept {
    // A native read ticket admitted before the cut may enter/return afterwards.
    return admit_body(meter,Family::WorkerRead);
}
bool admit_write(Meter& meter) noexcept {
    return admit_body(meter,Family::WriteCall);
}
bool admit_write(State& state) noexcept {
    return admit_body(state,Family::WriteCall);
}
''' + core[hi:]
 native['sup_core.cpp']=core
 main=native['sup_main.cpp']
 main=replace(main,'if (!spend(state,Family::WorkerStart)) return false;','''// One admitted setup group covers these events, CreateThread and started
        // wait (including worker entry/started completion). A pre-cut ticket
        // may finish after cut from the static tail; a later group cannot start.
        if (!admit_body(state,Family::WorkerStart)) return false;''')
 main=replace(main,'if (!spend(state,Family::Consume)) cleanup=true;','''if(cleanup) (void)spend(state,Family::Consume);
    else if(!admit_body(state,Family::Consume)) cleanup=true;''')
 main=replace(main,'if (tail_requested(state.meter)) break;\n        if (!spend(state,Family::Coordinator)) break;','if (!admit_body(state,Family::Coordinator)) break;')
 main=replace(main,'if (!spend(state,Family::Poll) || !counter(state,state.terminal.body_polls,1)) break;','if (!admit_body(state,Family::Poll) || !counter(state,state.terminal.body_polls,1)) break;')
 main=replace(main,'if (ready) {\n        state.terminal.flags|=TargetMayHaveRun;','''// The resume ticket is linearized BEFORE MayHaveRun and the native call.
    // Cut-before-ticket keeps the established target suspended. Cut-after-ticket
    // cannot revoke an already admitted call; its completion is prepaid tail.
    if (ready && admit_body(state,Family::TargetResume)) {
        state.terminal.flags|=TargetMayHaveRun;''')
 native['sup_main.cpp']=main
 io=native['sup_io.cpp']
 io=replace(io,'bool open_capture_owners(State& state) noexcept {\n    if(state.unknown_native_creation)', '''bool open_capture_owners(State& state) noexcept {
    if(!admit_body(state,Family::CaptureSetup)) return false;
    if(state.unknown_native_creation)''')
 io=replace(io,'if (!spend(state,Family::Owner)) return false;','if (!admit_body(state,Family::Owner)) return false;') if io.count('if (!spend(state,Family::Owner)) return false;')==1 else io.replace('if (!spend(state,Family::Owner)) return false;','if (!admit_body(state,Family::Owner)) return false;')
 # Exactly the two source-owned Owner admissions (per-log and target setup).
 assert io.count('admit_body(state,Family::Owner)')==2
 io=replace(io,'if (!spend(state,Family::WriteChunk)) return false;','if (!admit_body(state,Family::WriteChunk)) return false;')
 native['sup_io.cpp']=io
 codec=native['sup_codec.cpp']
 codec=replace(codec,'if(!spend(state,Family::TargetQuote)) return false;','if(!admit_body(state,Family::TargetQuote)) return false;')
 codec=replace(codec,'if(!spend(state,Family::Environment)) return false;','if(!admit_body(state,Family::Environment)) return false;')
 native['sup_codec.cpp']=codec
 dest=OUT/'native';dest.mkdir(exist_ok=True)
 records=[]
 for name,value in native.items():
  p=dest/name;p.write_text(value,encoding='utf-8',newline='\n')
  old=(BASE/'native'/name).read_bytes()
  records.append({'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes()),'predecessor_sha256':sha(old),'namespace_only_unchanged':old.decode().replace('yesdaw_clock_supervisor_r2','yesdaw_clock_supervisor_r3')==value})
 dump(OUT/'native-preparation.json',{'source_only':True,'all_target_execution':False,'files':records,'scope':'Complete new R16 native candidate. F1–F8 retained; F9 prefix-only tickets added. No prices/native/resources/ABI/binding acceptance.'})
 print(json.dumps({'native_files':len(records),'source_only':True}))
if __name__=='__main__':
 root=OUT/'preparation-attempts';root.mkdir(exist_ok=True)
 att=root/f'attempt-{len(list(root.iterdir()))+1:02d}';att.mkdir()
 inputs=[Path(__file__),BASE/'source-pins.json',BASE/'freeze.json',*sorted((BASE/'native').glob('*'))]
 for p in inputs:
  if p.is_file():
   q=att/'inputs'/p.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
 code=0
 with (att/'stdout.txt').open('w') as so,(att/'stderr.txt').open('w') as se,contextlib.redirect_stdout(so),contextlib.redirect_stderr(se):
  try:main()
  except BaseException:traceback.print_exc();code=1
 dump(att/'result.json',{'exit_code':code,'source_only':True,'target_execution':False})
 for p in [*sorted((OUT/'native').glob('*')),OUT/'native-preparation.json']:
  if p.is_file():
   q=att/'outputs'/p.relative_to(ROOT);q.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(p,q)
 print((att/'stdout.txt').read_text(),end='')
 if code:print((att/'stderr.txt').read_text(),end='',file=sys.stderr)
 raise SystemExit(code)
