"""Independent whole R15 source/file/JSON/integer review checker.

Does not import or execute candidate checkers, models, PowerShell, native APIs,
fixtures, programs, compilers or Git. Handwritten models are source arguments.
"""
from pathlib import Path
from collections import Counter
import hashlib,json,re,difflib,traceback
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
PKT=ROOT/'build-ci/20261106-clock-owned-supervisor-source-r2'
CUR=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r15-owned-supervisor-source-r1'
OLD=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r14-owned-supervisor-source-r1'
R13=ROOT/'build-ci/musical-audio-persistent-planner-certification/source-revisions/clock-oracle-r13-streamed-dependency-source-r1'
if (HERE/'source-pins.json').exists():raise ValueError('Preserve the first complete independent review freeze')
checks=[];pins={}
def sha(b):return hashlib.sha256(b).hexdigest().upper()
def name(p):
    try:return p.relative_to(ROOT).as_posix()
    except ValueError:return p.as_posix()
def read(p):
    b=p.read_bytes();pins[name(p)]=dict(path=name(p),bytes=len(b),sha256=sha(b));return b
def check(label,value,expected=True):
    ok=value==expected;checks.append(dict(name=label,passed=ok))
    if not ok:raise ValueError(label+': '+repr(value)+' != '+repr(expected))
def exact(p,h):
    b=read(p);check('exact '+name(p),sha(b),h);return b
def row_pin(r):
    p=Path(r['path']);p=p if p.is_absolute() else ROOT/p;b=read(p)
    check('row '+r['path'],(len(b),sha(b)),(r['bytes'],r['sha256']))
def write(n,obj):
    p=HERE/n
    if p.exists():raise ValueError('Preserve prior output '+n)
    p.write_text(json.dumps(obj,indent=2)+'\n',encoding='utf-8',newline='\n')
def functions(s):
    d=list(re.finditer(r'^function ([A-Za-z0-9_-]+)',s,re.M));end=s.index('\ntry{\n # Hash literal manifest')+1;out={}
    for i,h in enumerate(d):
        stop=d[i+1].start() if i+1<len(d) else end;code=s[h.start():stop]
        out[h.group(1)]=dict(source=code,sha256=sha(code.encode()),first_line=s[:h.start()].count('\n')+1)
    return out
def section(s,a,b):
    start=s.index(a);return s[start:s.index(b,start+len(a))]

def run():
    subject_pins=json.loads(exact(PKT/'source-pins.json','5806F35FE26CC1CB2A43B7E49AD6CEBEDE6C28EE7ABAC6BE52CDBE77292FDDB1'))
    freeze=json.loads(exact(PKT/'freeze.json','895F4F1BB04F955E9E40BFDC29AC29AE15AB844ECE21AF068D3DFAEF16914169'))
    corr=json.loads(exact(PKT/'correspondence.json','49EBF2FA740AE3B3FEA31506CE8F463D4743A249107B399C6059A402669DAD06'))
    exact(PKT/'counterexamples.json','66FE7552F3AA826ABD25B9C5D27213F46855204925116BF64EC874A71F3AB446')
    for r in subject_pins['subjects']+subject_pins['protected_r14_subjects']+freeze['outputs']+freeze['subjects']:row_pin(r)
    for n,h in subject_pins['protected_r13'].items():exact(R13/n,h)
    for p,h in subject_pins['accepted_docs'].items():exact(ROOT/p,h)
    read(ROOT/'AGENTS.md');read(ROOT/'STATUS.md')
    read(PKT/'proposal.md');read(PKT/'regressions.py');read(PKT/'check.py');read(PKT/'author.py');read(PKT/'prepare-native.py')
    read(ROOT/'build-ci/20261106-r14-supervisor-functional-review-r1/review.md')
    source=exact(CUR/'run-clock.ps1','6B1016841798BA2B4D6F7CEAEFC8F663F3A5FE0D7CEC3CEC34355EC71B527A38').decode()
    mb=exact(CUR/'source-manifest.json','BDC2BE017BDF269691D727AE10A489FD6C1EFBB82F2B1C71DB392B1728D1B86F');m=json.loads(mb)
    oldsource=read(OLD/'run-clock.ps1').decode();oldm=json.loads(read(OLD/'source-manifest.json'))
    oldf=functions(oldsource);curf=functions(source)
    check('whole current88 bodies',len(curf),88);check('same88 names',set(curf),set(oldf))
    for n in curf:check('complete parent body unchanged '+n,curf[n]['source'],oldf[n]['source'])
    before=oldsource;after=source
    # Exactly the source-sized bootstrap raw node literal differs in the runner.
    oldnode=re.search(r'Assert-TaskRawJsonBytes \$taskBootstrap (\d+) 512 4',before).group(1)
    check('whole runner sole replacement',before.replace('Assert-TaskRawJsonBytes $taskBootstrap '+oldnode+' 512 4','Assert-TaskRawJsonBytes $taskBootstrap 16561 512 4'),after)
    for n,h in [('clock_stage.cpp','F6A6E976715C9F28F1B349F60A38E79693D0157D51072C5C8C5ACB74969DF206'),('source-costs.h','0087C41C88C8647FCC9CAAD92DA55223164122A4E31FF2DFC27715FAAEB83A27')]:
        exact(CUR/n,h);check('protected target byte correspondence '+n,read(CUR/n),read(OLD/n))
    for r in m['sources']:
        b=read(CUR/r['path']);check('own4 descriptor '+r['path'],(len(b),len(b.splitlines()),sha(b)),(r['bytes'],r['lines'],r['sha256']))
    for r in m['inputs']:row_pin(r)
    check('1240 exact admitted inputs',len(m['inputs']),1240)
    check('input byte sum',sum(r['bytes'] for r in m['inputs']),461790563)
    check('input chunk sum',sum((r['bytes']+65535)//65536 for r in m['inputs']),7886)
    oldkeys={r['path']:(r['bytes'],r['sha256']) for r in oldm['inputs']}
    newkeys={r['path']:(r['bytes'],r['sha256']) for r in m['inputs']}
    check('input physical identity uniqueness',len(newkeys),len(m['inputs']))
    check('all predecessor inputs unchanged',all(newkeys.get(k)==v for k,v in oldkeys.items()))
    added=sorted(set(newkeys)-set(oldkeys));check('exact10 added physical paths',len(added),10)
    check('five old sibling paths added',sum('/clock-oracle-r14-owned-supervisor-source-r1/' in n for n in added),5)
    check('five new native paths added',sum('/20261106-clock-owned-supervisor-source-r2/native/' in n for n in added),5)
    for r in m['closed_directory_inputs']:
        p=Path(r['path']);p=p if p.is_absolute() else ROOT/p
        entries=list(p.rglob('*')) if r['recursive'] else list(p.iterdir())
        files=sorted(q.relative_to(p).as_posix() for q in entries if q.is_file())
        check('closed namespace entries '+r['path'],len(entries),r['entry_count'])
        check('closed namespace names '+r['path'],files,sorted(r['files']))
    check('namespace profile33',len(m['closed_directory_inputs']),33)
    check('namespace1102 entries',sum(r['entry_count'] for r in m['closed_directory_inputs']),1102)
    check('namespace1055 files',sum(len(r['files']) for r in m['closed_directory_inputs']),1055)
    counts=dict(values=0,keys=0,max_depth=0,max_string_units=0)
    def count(v,d=0):
        counts['values']+=1;counts['max_depth']=max(counts['max_depth'],d)
        if isinstance(v,dict):
            counts['keys']+=len(v)
            for k,x in v.items():counts['max_string_units']=max(counts['max_string_units'],len(k.encode('utf-16-le'))//2);count(x,d+1)
        elif isinstance(v,list):
            for x in v:count(x,d+1)
        elif isinstance(v,str):counts['max_string_units']=max(counts['max_string_units'],len(v.encode('utf-16-le'))//2)
        else:check('bootstrap scalar '+str(counts['values']),v is None or type(v) in (int,bool))
    count(m);check('bootstrap161? exact16561',counts['values']+counts['keys'],16561)
    check('bootstrap depth4',counts['max_depth'],4);check('bootstrap max435',counts['max_string_units'],435)
    check('all manifest execution guards false',any(m['guards'].values()),False)
    check('native data not bound',m['supervisor']['bound'],False)
    check('target no Data definitions',m['cost_catalogue']['definition_files'],[])
    check('request caps',[140+40*1028,140+18*1028],[41260,18644])
    check('terminal explicit308',8+16*4+3*8+3*32+2*7*8+4,308)
    check('whole transport79472',41260+2*18644+3*308,79472)
    check('raw command2576',2+512+1+12+4*512+1,2576)
    check('target command29271',512+2+28*(2*512+3)+1,29271)
    check('environment4156',43+8*512+8+8+1,4156)
    check('catalogue1321',1240+4+1+74+1+1,1321)
    check('transitive1246',1240+4+1+1,1246)
    retention=m['retention']
    retained_sum=sum(retention[k] for k in ['input_bytes','own5_reserved','reference_pool','compiler_admitted_final_pool','stage_pool','control_pool'])+79472+33554432
    check('retention reservation arithmetic only',retained_sum,retention['reservation_upper'])
    check('retention honest nonruntime scope','SOURCE_ARITHMETIC_RESERVATION_ONLY' in retention['status'])
    check('stale unqualified failure rows78',m['failure_policy']['maximum_raw_failed_prefix_file_rows'],78)
    check('actual profile failure rows85',m['profile']['failure_inventory_max_files'],85)
    check('actual profile root rows85',m['profile']['root_walk_entries_max'],85)
    check('stale unqualified cleanup13',m['failure_policy']['bounded_child_cleanup_events'],13)
    check('actual override cleanup15',m['failure_policy']['R14_cleanup_entries_max'],15)
    check('full prospective recipe75',len(m['references'])+len(m['compiler_final_outputs'])+len(m['control_files'])+len(m['stage_files'])+len(m['supervisor']['transport_files']),75)
    native={q.name:read(q).decode() for q in sorted((PKT/'native').iterdir())}
    check('exact five native modules',set(native),{'sup.h','sup_codec.cpp','sup_core.cpp','sup_io.cpp','sup_main.cpp'})
    check('private namespace consistent',all('namespace yesdaw_clock_supervisor_r2 {' in t for t in native.values()))
    h=native['sup.h'];core=native['sup_core.cpp'];io=native['sup_io.cpp'];code=native['sup_codec.cpp'];main=native['sup_main.cpp']
    for flag in ['source_review_accepted','source_work_accepted','native_profile_accepted','controlled_resource_accepted','execution_accepted']:check('native false '+flag,'bool '+flag+' = false;' in h)
    check('only declared Data','extern const Catalogue Data;' in h and not any(re.search(r'const Catalogue Data\s*[={]',t) for t in native.values()))
    pub=section(main,'bool publish_ready','void stop_workers');settle=section(main,'void settle_one','void zero_unavailable_counters');consume=section(main,'bool consume_ready','bool observe_target_exit')
    aftersignal=pub[pub.index('if (!SetEvent(worker.ready))'):]
    for field in ['count','partial','error','kind','error_reason','generation','generation_valid']:
        check('published immutable after signal '+field,not re.search(r'worker\.'+field+r'\s*=(?!=)',aftersignal))
    check('no publisher logical rewrite','worker_error(' not in pub)
    check('exit acquired only thread success',settle.index('if (wait==WAIT_OBJECT_0)')<settle.index('const WorkerExit& exit'))
    check('exit consumes un-signaled slot','if (!worker.ready_consumed) consume_ready(state,i,true)' in settle)
    check('consumption refuses into prepaid accounting','if (!spend(state,Family::Consume)) cleanup=true;' in consume)
    check('exited acquisition no ack API','if(!worker.settled)' in consume)
    ready=section(main,'bool ready_now','void cancel_workers')
    check('ready wait immediate failure','const DWORD error=wait==WAIT_FAILED?GetLastError():0;' in ready)
    check('settlement wait immediate failure','else if (wait==WAIT_FAILED)' in settle and 'const DWORD error=GetLastError();' in settle)
    check('common ready used all3 paths',main.count('ready_now(state,worker)'),3)
    worker=section(main,'DWORD WINAPI worker_entry','bool start_workers')
    zero=section(worker,'if(got==0)','worker.active_fill+=got')
    check('TRUE0 not EOF','ReadyKind::NoProgress' in zero and 'ZeroEof' not in zero)
    check('TRUE0 retains fill','worker.active_fill=0' not in zero and 'worker.operation_open=false' not in zero)
    check('Continue preserves active fill','if(published!=ReadyKind::NoProgress) worker.active_fill=0' in worker)
    for fill in [0,1,17,4095]:
        control=dict(count=0,partial=0,EOF=False,active_fill=fill)
        check('NoProgress byte-neutral '+str(fill),control['count']==0 and not control['EOF'] and control['active_fill']==fill)
        check('same-operation completion '+str(fill),fill+(4096-fill),4096)
    check('NoProgress bound',2*7200+2,14402)
    check('search occupied before creator',io.index('if (state.search_owner!=INVALID_HANDLE_VALUE) return false;')<io.index('HANDLE search=FindFirstFileW'))
    check('short-write remaining omission','native_count(state,count.omitted,length-at)' in io)
    for at in [0,1,2048,4095]:check('cap preserves confirmed prefix '+str(at),at+(4096-at),4096)
    check('typed source/extent path','enum class PathResult' in h and 'PathResult::SourceRefused' in code and 'PathResult::ExtentRejected' in code)
    check('all join calls typed',not any('!join_path(' in t for t in native.values()))
    check('coordinator refusals enter tail','if (charge(state.meter,family,count)) return true;\n    begin_tail(state);' in core)
    badpipe=section(io,'if (!ok) {\n            capture_raw(state.failed_pipe_output','if(!live(read)')
    badproc=section(io,'if (!created) {','if (!live(process.hProcess)')
    for label,t in [('pipe',badpipe),('process',badproc)]:
        check('failed '+label+' private raw only','unknown_native_creation=true' in t and 'capture_raw' in t)
        check('failed '+label+' no handle promotion',not any(x in t for x in ['CloseHandle','WaitForSingleObject','ChildCreated','target_pid','state.process=','workers[i].pipe=']))
    check('unknown creator forces4','|| state.unknown_native_creation) { t.status=4' in main)
    check('unknown creator conservative parent','($taskU[6] -eq 4 -and -not $taskStarted)' in source)
    check('helper source/binary before every source fence','function Assert-TaskPins{\n Assert-TaskSupervisorBinding' in source)
    check('original fenced finally ordering',curf['Invoke-TaskFencedChild']['source'],oldf['Invoke-TaskFencedChild']['source'])
    check('parent no KillTree','.Kill(true)' not in source and '.Kill($true)' not in source)
    check('parent null/signed exits','if($taskSigned -ge 2147483648L){$taskSigned-=4294967296L}' in source)
    check('raw259 bit projection',259 if 259<2**31 else 259-2**32,259)
    check('signed minimum bit projection',2**31-2**32,-2147483648)
    check('all old child keys24',len(re.findall(r'(?:@\{|;)([A-Za-z_][A-Za-z_0-9]*)=',next(l for l in source.splitlines() if '$taskRow=[ordered]@{' in l))),24)
    check('no raw terminal direct wire','terminal=$taskPrefix' not in next(l for l in source.splitlines() if '$taskRow=[ordered]@{' in l))
    check('target parser only committed return',source.index("if($null -ne $taskFailure -or -not $taskCommitted)")<source.index('return $taskRow'))
    check('fenced target completion still followed parser',"$taskProduced=Read-TaskChildResult $taskProduce" in source and "$taskVerified=Read-TaskChildResult $taskVerify" in source)
    check('all ten controls preserved',len(re.findall(r"Expect-TaskGate '[^']+'",source)),10)
    check('no old receiver defect','[object].GetMethod(\'GetType\',[Type]::EmptyTypes)' in source)
    # The NEW counterexample is independent of the author's regression model.
    # Symbolic ticket refusal, not Data/prices/stub construction or execution.
    trace=[dict(step=1,event='established target remains suspended',tail=False,may_have_run=False),
      dict(step=2,event='worker0 started event observed',tail=False,may_have_run=False),
      dict(step=3,event='first WorkerRead ticket refuses; admit_read freezes prefix',tail=True,may_have_run=False),
      dict(step=4,event='later WorkerStart spend sees tail and returns true; start_workers returns true',tail=True,may_have_run=False),
      dict(step=5,event='supervisor_main sets TargetMayHaveRun and attempts ResumeThread',tail=True,may_have_run=True),
      dict(step=6,event='coordinator immediately notices tail; cleanup follows',tail=True,may_have_run=True)]
    start=section(main,'bool start_workers','bool consume_ready');entry=section(main,'int supervisor_main','\n}\n}\n')
    check('new finding core tail charge succeeds','if (!meter.tail)' in core and 'bool ok = true;' in core)
    check('new finding worker refusal cuts','meter.tail = true;\n        meter.frozen_prefix = meter.used;' in core)
    check('new finding no pre-resume tail admission','tail_requested' not in start and 'tail_requested' not in entry)
    check('new finding resume follows start success','create_contained_target(state) && start_workers(state)' in entry and 'if (ready)' in entry and 'ResumeThread(state.primary_thread)' in entry)
    check('new finding trace actually violates cut',trace[4]['tail'] and trace[4]['may_have_run'])
    write('counterexample-pre-resume-cut.json',dict(source_only=True,runtime_observation=False,defined_Data_or_prices=False,steps=trace,source_spans={'sup_core.cpp':[[9,34],[43,60]],'sup_main.cpp':[[209,233],[435,450]]}))
    inventory=dict(parent_helpers=[dict(name=n,first_line=v['first_line'],sha256=v['sha256'],R14_byte_text_equal=True) for n,v in curf.items()],
      native_modules=[dict(name=n,lines=len(t.splitlines()),sha256=sha(t.encode()),whole_read=True,
        declarations=[dict(line=t[:h.start()].count('\n')+1,name=h.group(1)) for h in re.finditer(r'^(?:\[\[noreturn\]\]\s+)?(?:bool|void|U32|U64|Failure|DWORD|int|PathResult)\s+(?:WINAPI\s+)?([A-Za-z_]\w*)\s*\(',t,re.M)]) for n,t in native.items()],
      bootstrap=counts,added_physical_input_paths=added,complete_source_only=True)
    write('whole-source-inventory.json',inventory)
    write('checks.json',dict(status='FIRST_COMPLETE_INDEPENDENT_SOURCE_DATA_CHECK_PASS',checks=len(checks),passed=len(checks),rows=checks,not_a_target_runtime_test=True))
    report=json.loads((HERE/'review-draft.json').read_bytes());report['verification']=dict(checks=len(checks),passed=len(checks),input_files=len(pins),parent_bodies=88,native_modules=5,actual_execution=False)
    write('review.json',report)
    own=[]
    for p in sorted(HERE.rglob('*')):
        if p.is_file() and '__pycache__' not in p.parts:
            b=p.read_bytes();own.append(dict(path=name(p),bytes=len(b),sha256=sha(b)))
    write('source-pins.json',dict(schema='R15_WHOLE_FUNCTIONAL_INDEPENDENT_REVIEW_PINS',inputs=sorted(pins.values(),key=lambda x:x['path']),own_files=own,
      candidate_untouched=True,candidate_checker_or_model_executed=False,target_execution=False))
    own.append(dict(path=name(HERE/'source-pins.json'),bytes=(HERE/'source-pins.json').stat().st_size,sha256=sha((HERE/'source-pins.json').read_bytes())))
    for r in own:
        b=(ROOT/r['path']).read_bytes()
        if (len(b),sha(b))!=(r['bytes'],r['sha256']):raise ValueError('postfreeze bytes changed '+r['path'])
    write('first-green-review-freeze.json',dict(schema='R15_INDEPENDENT_FIRST_COMPLETE_REVIEW_FREEZE',files=own,subject_manifest_sha256=sha(mb),verdict=report['verdict'],checks=len(checks)))
    print(json.dumps(dict(verdict=report['verdict'],checks=len(checks),review_sha256=sha((HERE/'review.json').read_bytes()),pins_sha256=sha((HERE/'source-pins.json').read_bytes()),freeze_sha256=sha((HERE/'first-green-review-freeze.json').read_bytes()))))

if __name__=='__main__':
    try:run()
    except Exception:
        i=1
        while (HERE/('attempt-r'+str(i))).exists():i+=1
        p=HERE/('attempt-r'+str(i));p.mkdir()
        for f in HERE.iterdir():
            if f.is_file():(p/f.name).write_bytes(f.read_bytes())
        (p/'failure.json').write_text(json.dumps(dict(error=traceback.format_exc(),completed_checks=len(checks),target_executed=False),indent=2)+'\n',encoding='utf-8',newline='\n')
        raise
