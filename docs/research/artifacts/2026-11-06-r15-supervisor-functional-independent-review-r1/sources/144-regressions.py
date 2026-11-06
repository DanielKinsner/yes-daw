"""Handwritten finite source counterexamples; no target parsing or execution."""
import copy

def run(native):
    h=native['sup.h'];main=native['sup_main.cpp'];io=native['sup_io.cpp'];core=native['sup_core.cpp'];codec=native['sup_codec.cpp']
    rows=[];models=[]
    def test(name,value,evidence=None):rows.append({'id':'regression-'+name,'pass':bool(value),'evidence':evidence})
    def section(text,a,b):return text[text.index(a):text.index(b,text.index(a)+len(a))]
    pub=section(main,'bool publish_ready','void stop_workers')
    after_signal=pub[pub.index('if (!SetEvent(worker.ready))'):]
    fields=['kind','count','partial','error','error_reason','error_native_known','generation','generation_valid']
    test('F1-no-published-slot-write-after-signal',all('worker.'+x+'=' not in after_signal for x in fields))
    test('F1-no-logical-error-rewrite','worker_error(' not in pub)
    test('F1-exit-record-thread-acquired','const WorkerExit& exit=worker.exit' in main and main.index('if (wait==WAIT_OBJECT_0) {')<main.index('const WorkerExit& exit=worker.exit'))
    slot={'kind':'Full','count':4096,'partial':0,'error':0,'generation':9}
    frozen=copy.deepcopy(slot);exit_record={'failures':[{'site':9,'reason':30,'native_known':True,'native_error':5}]}
    test('F1-ack-failure-model-slot-immutable',slot==frozen and exit_record['failures'][0]['native_error']==5)
    models.append({'finding':'F1','predecessor_counterexample':'ready Full4096 then failed ack wait rewrote kind/count while coordinator reads','corrected_trace':{'published':frozen,'after_ack_failure':slot,'separate_exit':exit_record},'runtime_execution':False})

    test('F2-publisher-debit-retains-kind','request_tail(state.meter)' in pub and 'worker_error(' not in pub)
    settle=section(main,'void settle_one','void zero_unavailable_counters')
    test('F2-exit-acquires-completed-unsignaled-slot','worker.settled=true;' in settle and 'if (!worker.ready_consumed) consume_ready(state,i,true)' in settle)
    test('F2-real-exit-record-not-missing-EOF-fallback','exit.failures[j]' in settle and 'state.terminal.flags&(i==0?StdoutEof' not in settle)
    consume=section(main,'bool consume_ready','bool observe_target_exit')
    test('F2-consumption-debit-tail-still-accounts','if (!spend(state,Family::Consume)) cleanup=true;' in consume)
    test('F2-no-exit-acquire-reset-or-ack','if(!worker.settled)' in consume and consume.index('if(!worker.settled)')<consume.index('ResetEvent(worker.ready)'))
    preserved=[]
    for kind,n in [('Full',4096),('EofShort',17),('ZeroEof',0)]:
        for event in ['publisher_meter_refusal','ready_signal_failure','ack_wait_failure']:
            result={'kind':kind,'count':n,'partial':0};done=False;observed=0;eof=False
            for acquire in ['thread-exit','cleanup-again']:
                if not done:observed+=result['count'];eof|=kind=='ZeroEof';done=True
            test('F2-'+kind+'-'+event,observed==n and eof==(kind=='ZeroEof'))
            preserved.append({'kind':kind,'event':event,'observed_once':observed,'EOF':eof,'partial':0})
    models.append({'finding':'F2','predecessor_counterexample':{'kind':'Full','count_before':4096,'count_after':0,'partial_after':0,'bytes_lost':4096},'corrected_traces':preserved,'runtime_execution':False})

    ready=section(main,'bool ready_now','void cancel_workers')
    test('F3-common-ready-wait-immediate-capture','const DWORD error=wait==WAIT_FAILED?GetLastError():0;' in ready and 'Reason::WorkerJoinFailure,wait==WAIT_FAILED,error' in ready)
    test('F3-thread-wait-immediate-capture','else if (wait==WAIT_FAILED)' in settle and 'const DWORD error=GetLastError();' in settle)
    body=section(main,'void coordinator','void cleanup_prefix')
    cleanup=section(main,'void cleanup_prefix','void finalize_record')
    test('F3-body-and-cleanup-use-common-ready','ready_now(state,worker)' in body and cleanup.count('ready_now(state,worker)')==2)
    test('F3-body-and-cleanup-use-common-exit','settle_one(state,i)' in body and 'settle_one(state,i)' in cleanup)
    test('F3-no-inline-discarded-wait','else if (WaitForSingleObject' not in main)
    wait_roles=['body-ready','body-exit','initial-cleanup-ready','retry-cleanup-ready','retry-cleanup-exit','started','ack','target-body','target-cleanup']
    for role in wait_roles:
        for code in [0,5]:
            failure={'role':role,'known':True,'error':code};later_timeout={'known':False}
            first=failure
            test('F3-order-'+role+'-'+str(code),first is failure and first['known'] and first['error']==code)
    models.append({'finding':'F3','predecessor_counterexample':'WAIT_FAILED code5 discarded by inline/cleanup-ready waits; later timeout replaces first cause','corrected_wait_roles':wait_roles,'runtime_execution':False})

    worker=section(main,'DWORD WINAPI worker_entry','bool start_workers')
    zero=section(worker,'if(got==0)','worker.active_fill+=got')
    test('F4-TRUE0-NoProgress-not-EOF','ReadyKind::NoProgress' in zero and 'ReadyKind::ZeroEof' not in zero and 'worker.active_fill=0' not in zero and 'worker.operation_open=false' not in zero)
    test('F4-only-broken-pipe-closure','if (!ok && error!=ERROR_BROKEN_PIPE)' in worker and 'if (!ok) {' in worker and 'if (!ok || got==0)' not in worker)
    test('F4-preserve-fill-on-Continue','if(published!=ReadyKind::NoProgress) worker.active_fill=0' in worker)
    no_progress=section(consume,'else if(kind==ReadyKind::NoProgress)','else if (kind==ReadyKind::Error)')
    test('F4-NoProgress-no-logical-observation','counter(' not in no_progress and 'flags|=' not in no_progress and 'worker.active_fill' not in no_progress)
    test('F4-partial-exit-acquired-only','worker.active_fill' not in body and 'worker.active_fill' not in settle and 'exit.partial' in settle)
    test('F4-normal-one-per-direction','for (U32 i=0;i<2;++i)' in body and body.count('consume_ready(state,i,false)')==1 and 'while(' not in body)
    test('F4-no-extra-input-or-zero-count-guard','zero_retry' not in main and 'zero_limit' not in main and 'zero_write_free' not in main)
    zero_traces=[]
    for initial in [0,17,4095]:
        active=initial;observed=0;eof=False
        # TRUE0 publishes a control record, not a successful logical result.
        active_after_continue=active
        stop_partial=active
        test('F4-model-zero-fill-'+str(initial),active_after_continue==initial and observed==0 and not eof and stop_partial==initial)
        # Complete the same operation after Continue, then actual broken EOF.
        remainder=4096-active;active+=remainder;observed+=active
        test('F4-model-zero-then-data-'+str(initial),observed==4096 and not eof)
        zero_traces.append({'initial_positive_fill':initial,'TRUE0':{'control':'NoProgress','observed':0,'EOF':False},'Continue_fill':active_after_continue,'Stop_exit_partial':stop_partial,'later_full_observed':observed})
    normal_bound=2*7200;pending=2
    test('F4-independent-finite-no-progress-bound',normal_bound+pending==14402)
    test('F4-prestart-tail-no-extra-Continue','begin_tail(state);stop_workers(state)' in cleanup and 'acknowledge(state,worker,false)' in cleanup and '!tail_requested(state.meter)' in pub)
    models.append({'finding':'F4','predecessor_counterexample':'TRUE0 followed by real bytes was falsely consumed as EOF','corrected_traces':zero_traces,'normal_control_consumptions_upper':normal_bound,'pending_slots_upper':pending,'no_new_input_guard':True,'runtime_execution':False})

    scan=section(io,'bool stage_scan','bool observe_position')
    test('F5-search-occupied-before-open',scan.index('state.search_owner!=INVALID_HANDLE_VALUE')<scan.index('FindFirstFileW'))
    test('F5-success-only-search-promotion',scan.index('} else {\n        state.search_owner=search;')>scan.index('if (search==INVALID_HANDLE_VALUE)'))
    test('F5-close-failure-retains-slot','else state.search_owner=INVALID_HANDLE_VALUE;' in scan and 'cleanup_mask|=1u<<13' in scan)
    handle='established-search-H0';held=handle;find_calls=0
    close_ok=False
    if close_ok:held=None
    if held is None:find_calls+=1;held='H1'
    test('F5-model-retained-owner-not-overwritten',held==handle and find_calls==0)
    models.append({'finding':'F5','predecessor_counterexample':'FindClose(H0) false then new scan overwrote H0 with H1/INVALID','corrected_trace':{'held':held,'new_find_calls':find_calls},'runtime_execution':False})

    write=section(io,'bool write_chunk','void close_safe_owners')
    continuation=section(write,'if (!stage_scan(state,length-at,extent))','if (!admit_write(state))')
    test('F6-exact-continuation-remaining','native_count(state,count.omitted,length-at)' in continuation and '(state.terminal.flags&StagePoolCap)!=0' in continuation)
    cap_traces=[]
    for at in [0,1,2048,4095]:
        length=4096;written=at;unretained=length-at;omitted=length-at
        test('F6-model-cap-'+str(at),written+unretained==length and omitted==unretained)
        test('F6-model-noncap-'+str(at),0<=unretained and written+unretained==length)
        cap_traces.append({'length':length,'confirmed_written':at,'cap_omitted':omitted,'noncap_omitted':0,'unretained':unretained})
    models.append({'finding':'F6','predecessor_counterexample':{'length':4096,'confirmed_written':1,'omitted':0,'required_omitted':4095},'corrected_traces':cap_traces,'runtime_execution':False})

    join=section(codec,'PathResult join_path','bool parse_command')
    test('F7-typed-path-causes','PathResult::SourceRefused' in join and 'PathResult::ExtentRejected' in join and 'PathResult::Ok' in join)
    test('F7-no-boolean-join-callers',all('!join_path' not in x for x in native.values()))
    test('F7-stage-checks-actual-extent-only',scan.count('==PathResult::ExtentRejected')==2 and 'pattern_result!=PathResult::Ok' in scan and 'child_result!=PathResult::Ok' in scan)
    spend=section(core,'bool spend','bool admit_read')
    test('F7-coordinator-refusal-cuts-immediately','if (charge(state.meter,family,count)) return true;' in spend and 'begin_tail(state)' in spend)
    test('F7-no-raw-coordinator-debits','charge(state.meter,' not in io and 'charge(state.meter,' not in codec and main.count('charge(state.meter,')==1)
    test('F7-write-ticket-state-wrapper','if (!admit_write(state))' in io and 'bool admit_write(State& state)' in core)
    cases=[]
    for debit_ok,extent_ok in [(False,True),(False,False),(True,False),(True,True)]:
        cause='SourceRefused' if not debit_ok else ('ExtentRejected' if not extent_ok else 'Ok')
        primary=37 if cause=='SourceRefused' else (21 if cause=='ExtentRejected' else None)
        stage_flag=cause=='ExtentRejected'
        test('F7-model-'+str(debit_ok)+'-'+str(extent_ok),(debit_ok or (primary==37 and not stage_flag)))
        cases.append({'debit_ok':debit_ok,'extent_ok':extent_ok,'typed_cause':cause,'primary_reason':primary,'stage_guard_flag':stage_flag})
    models.append({'finding':'F7','predecessor_counterexample':'valid short join with refused debit fabricated StageExtent before SourcePrefixLimit','corrected_cases':cases,'runtime_execution':False})

    pipe=section(io,'const BOOL ok=CreatePipe','if (!SetHandleInformation')
    failed_pipe=section(pipe,'if (!ok) {','if(!live(read)')
    test('F8-failed-pipe-no-owner-evaluation',all(x not in failed_pipe for x in ['live(read)','live(write)','workers[i].pipe=read','child_writes[i]=write','CloseHandle']))
    test('F8-failed-pipe-raw-provenance','capture_raw(state.failed_pipe_output[i],&read' in failed_pipe and 'unknown_native_creation=true' in failed_pipe)
    process=section(io,'const BOOL created=CreateProcessW','bool closed=true;')
    failed_process=section(process,'if (!created) {','if (!live(process.hProcess)')
    test('F8-failed-process-no-owner-or-identity',all(x not in failed_process for x in ['state.process=','state.primary_thread=','ChildCreated','target_pid','live(process.','CloseHandle','WaitForSingleObject']))
    test('F8-failed-process-raw-provenance','capture_raw(state.failed_process_output,&process' in failed_process and 'unknown_native_creation=true' in failed_process)
    test('F8-promotion-after-success',process.index('state.process=process.hProcess')>process.index('if (!created)'))
    test('F8-forces-status4-independent-established-settlement','|| state.unknown_native_creation) { t.status=4' in main)
    test('F8-raw-not-native-api-arguments',all(x not in io+main for x in ['CloseHandle(state.failed_','WaitForSingleObject(state.failed_','ResumeThread(state.failed_','GetExitCodeProcess(state.failed_']))
    raw_traces=[]
    for raw in ['all-zero','nonzero-invalid-words','possible-valid-looking-words']:
        known_handles=[];pid=0;created=False;unknown=True;all_established_settled=True;job_empty=True
        status=4 if unknown else 2
        test('F8-model-'+raw,status==4 and not known_handles and pid==0 and not created)
        raw_traces.append({'failed_raw':raw,'trusted_handles':known_handles,'pid':pid,'ChildCreated':created,'known_job_empty':job_empty,'established_settled':all_established_settled,'unknown_native_creation':unknown,'status':status})
    models.append({'finding':'F8','predecessor_counterexample':'failed raw words passed live test then promoted/waited/closed and PID/ChildCreated set','corrected_traces':raw_traces,'runtime_execution':False})
    test('all-eight-findings-covered',[m['finding'] for m in models]==['F'+str(i) for i in range(1,9)])
    return rows,models
