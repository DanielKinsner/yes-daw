#include "sup.h"

namespace yesdaw_clock_supervisor_r4 {
namespace {
bool live(HANDLE value) noexcept { return value!=nullptr && value!=INVALID_HANDLE_VALUE; }
bool counter(State& state,U64& value,U64 increment) noexcept {
    U64 next=0;
    if (!checked_add(value,increment,next)) {
        state.terminal.flags&=~CoordinatorCountersKnown;
        fail(state,Site::Read,Reason::CounterOverflow); return false;
    }
    value=next; return true;
}
void worker_error(Worker& worker,Reason reason,bool known=false,DWORD error=0) noexcept {
    worker.kind=ReadyKind::Error; worker.error_reason=reason;
    worker.error_native_known=known; worker.error=known?error:0;
    worker.count=0;
}
void worker_exit_error(Worker& worker,Site site,Reason reason,bool known=false,DWORD error=0) noexcept {
    // Written only by the worker, read only after an actual thread-exit acquire.
    // At most generation-refusal plus one fatal synchronization failure occur.
    if(worker.exit.failure_count<2) worker.exit.failures[worker.exit.failure_count++]=failure(site,reason,known,error);
    else worker.exit.overflow=true;
}
DWORD finish_worker(Worker& worker,WorkerExitKind kind) noexcept {
    worker.exit.logical_incomplete=worker.operation_open;
    worker.exit.partial=worker.operation_open?worker.active_fill:
        (worker.kind==ReadyKind::Error?worker.partial:0u);
    worker.exit.partial_known=true;
    if(worker.operation_open && worker.exit.failure_count==0)
        worker_exit_error(worker,Site::Read,Reason::CancelledReadDiagnostic);
    worker.exit.kind=worker.exit.failure_count==0?kind:WorkerExitKind::Fault;
    return worker.exit.kind==WorkerExitKind::Fault?1u:0u;
}
bool publish_ready(Worker& worker) noexcept {
    State& state=*worker.state;
    if (!charge(state.meter,Family::WorkerPublish)) {
        request_tail(state.meter);
        // Completion/publication/accounting is mandatory prepaid tail work.
        // Keep Full/EofShort/ZeroEof/Error/NoProgress exactly as completed.
    }
    if (worker.generation==UINT64_MAX) {
        request_tail(state.meter);worker.generation_valid=false;
        worker_exit_error(worker,Site::Read,Reason::CounterOverflow);
    } else ++worker.generation;
    worker.ready_consumed=false;
    // Event publication is the release; the coordinator waits before reading.
    if (!SetEvent(worker.ready)) {
        const DWORD error=GetLastError();
        worker_exit_error(worker,Site::WorkerSettlement,Reason::WorkerJoinFailure,true,error);
        return false;
    }
    const DWORD wait=WaitForSingleObject(worker.ack,INFINITE);
    if (wait!=WAIT_OBJECT_0) {
        const DWORD error=wait==WAIT_FAILED?GetLastError():0;
        worker_exit_error(worker,Site::WorkerSettlement,Reason::WorkerJoinFailure,wait==WAIT_FAILED,error);
        return false;
    }
    return InterlockedCompareExchange(&worker.ack_kind,0,0)==static_cast<LONG>(AckKind::Continue) &&
           InterlockedCompareExchange(&worker.stop,0,0)==0 && !tail_requested(state.meter);
}
void stop_workers(State& state) noexcept {
    for (U32 i=0;i<2;++i) InterlockedExchange(&state.workers[i].stop,1);
}
void acknowledge(State& state,Worker& worker,bool more) noexcept {
    InterlockedExchange(&worker.ack_kind,static_cast<LONG>(more?AckKind::Continue:AckKind::Stop));
    if (!SetEvent(worker.ack)) { const DWORD error=GetLastError(); state.terminal.cleanup_mask|=1u<<3; fail(state,Site::WorkerSettlement,Reason::WorkerJoinFailure,true,error); }
}
bool workers_settled(const State& state) noexcept {
    return state.workers[0].settled && state.workers[1].settled;
}
bool ready_now(State& state,Worker& worker) noexcept {
    const DWORD wait=WaitForSingleObject(worker.ready,0);
    if(wait==WAIT_OBJECT_0) return true;
    if(wait!=WAIT_TIMEOUT) {
        const DWORD error=wait==WAIT_FAILED?GetLastError():0;
        state.terminal.cleanup_mask|=1u<<3;
        fail(state,Site::WorkerSettlement,Reason::WorkerJoinFailure,wait==WAIT_FAILED,error);
    }
    return false;
}
void cancel_workers(State& state) noexcept {
    for (U32 i=0;i<2;++i) {
        Worker& worker=state.workers[i];
        if (worker.created && !worker.settled) {
            // Stop prevents further tickets; cancellation may race an already
            // admitted generation. ERROR_NOT_FOUND is not a completion claim.
            if (!CancelSynchronousIo(worker.thread)) {
                const DWORD error=GetLastError();
                if (error!=ERROR_NOT_FOUND) { state.terminal.cleanup_mask|=1u; fail(state,Site::Read,Reason::CancelledReadDiagnostic,true,error); }
            }
        }
    }
}
void settle_one(State& state,U32 i) noexcept {
    Worker& worker=state.workers[i];
    if (!worker.created) {
        worker.settled=true;
        state.terminal.flags|=i==0?StdoutSettled:StderrSettled;
        state.terminal.flags|=i==0?StdoutErrorPartialKnown:StderrErrorPartialKnown;
        return;
    }
    if (worker.settled) return;
    const DWORD wait=WaitForSingleObject(worker.thread,0);
    if (wait==WAIT_OBJECT_0) {
        // A terminated worker's final publication is stable even if signaling
        // failed. Consume any unconsumed generation once without another read.
        worker.settled=true;
        // The complete slot is acquired even when SetEvent never succeeded.
        // No reset/ack API is needed on this exited-worker acquisition path.
        if (!worker.ready_consumed) consume_ready(state,i,true);
        const WorkerExit& exit=worker.exit;
        const bool valid=exit.kind!=WorkerExitKind::None && exit.partial_known &&
            exit.partial<pipe_buffer_bytes && exit.failure_count<=2 && !exit.overflow;
        if(valid) {
            const U32 known=i==0?StdoutErrorPartialKnown:StderrErrorPartialKnown;
            if((state.terminal.flags&known)!=0 && state.terminal.pipes[i].error_partial!=exit.partial) {
                state.terminal.flags&=~CoordinatorCountersKnown;
                fail(state,Site::Read,Reason::SourceInvariant);
            } else {
                state.terminal.pipes[i].error_partial=exit.partial;
                state.terminal.flags|=known|(i==0?StdoutSettled:StderrSettled);
            }
            if(exit.logical_incomplete) state.terminal.cleanup_mask|=1u<<(4u+i);
            for(U32 j=0;j<exit.failure_count;++j) {
                const Failure& item=exit.failures[j];
                if(item.site==static_cast<U32>(Site::WorkerSettlement)) state.terminal.cleanup_mask|=1u<<3;
                else state.terminal.cleanup_mask|=1u<<(4u+i);
                fail(state,static_cast<Site>(item.site),static_cast<Reason>(item.reason),item.native_known,item.native_error);
            }
            // Missing EOF alone never manufactures a failure. Only an actual
            // missing exit cause, or impossible unrequested Stop, is invariant loss.
            if(exit.kind==WorkerExitKind::Stopped && exit.failure_count==0 &&
               InterlockedCompareExchange(&worker.stop,0,0)==0 && !tail_requested(state.meter) && !state.stop_body)
                fail(state,Site::Read,Reason::SourceInvariant);
        } else {
            state.terminal.flags&=~CoordinatorCountersKnown;
            fail(state,Site::Read,exit.overflow?Reason::CounterOverflow:Reason::SourceInvariant);
        }
    } else if (wait==WAIT_FAILED) {
        const DWORD error=GetLastError(); state.terminal.cleanup_mask|=1u<<3;
        fail(state,Site::WorkerSettlement,Reason::WorkerJoinFailure,true,error);
    } else if(wait!=WAIT_TIMEOUT) {
        state.terminal.cleanup_mask|=1u<<3;
        fail(state,Site::WorkerSettlement,Reason::WorkerJoinFailure);
    }
}
void zero_unavailable_counters(Terminal& terminal) noexcept {
    terminal.body_polls=0;
    for (U32 i=0;i<2;++i) {
        terminal.pipes[i].observed=0;terminal.pipes[i].written=0;
        terminal.pipes[i].omitted=0;terminal.pipes[i].unretained=0;
    }
}
}

DWORD WINAPI worker_entry(void* context) noexcept {
    Worker& worker=*static_cast<Worker*>(context);
    State& state=*worker.state;
    if (!SetEvent(worker.started)) { const DWORD error=GetLastError();worker_exit_error(worker,Site::WorkerSettlement,Reason::WorkerJoinFailure,true,error);return finish_worker(worker,WorkerExitKind::Fault); }
    for (;;) {
        if (InterlockedCompareExchange(&worker.stop,0,0)!=0) return finish_worker(worker,WorkerExitKind::Stopped);
        worker.count=0;worker.partial=0;worker.kind=ReadyKind::None;
        worker.error=0;worker.error_reason=Reason::None;worker.error_native_known=false;
        if (worker.eof_short_pending) {
            // ReadAtLeastAsync's next logical zero result after a short EOF;
            // no extra native ReadFile is invented for this synthetic result.
            worker.eof_short_pending=false;worker.kind=ReadyKind::ZeroEof;
        } else {
            worker.operation_open=true;
            while (worker.active_fill<pipe_buffer_bytes) {
                if (InterlockedCompareExchange(&worker.stop,0,0)!=0 || !admit_read(state.meter)) {
                    worker.partial=worker.active_fill;worker.operation_open=false; worker_error(worker,Reason::CancelledReadDiagnostic);
                    break;
                }
                DWORD got=0;
                const BOOL ok=ReadFile(worker.pipe,worker.buffer+worker.active_fill,pipe_buffer_bytes-worker.active_fill,&got,nullptr);
                const DWORD error=ok?0:GetLastError();
                if (!ok && error!=ERROR_BROKEN_PIPE) {
                    worker.partial=worker.active_fill;worker.operation_open=false;
                    worker_error(worker,error==ERROR_OPERATION_ABORTED?Reason::CancelledReadDiagnostic:Reason::ReadFailure,true,error);
                    break;
                }
                if (got>pipe_buffer_bytes-worker.active_fill) { worker.partial=worker.active_fill;worker.operation_open=false; worker_error(worker,Reason::SourceInvariant); break; }
                if (!ok) {
                    // Only documented broken-pipe closure is native EOF.
                    worker.count=worker.active_fill;worker.operation_open=false;
                    worker.kind=worker.active_fill==0?ReadyKind::ZeroEof:ReadyKind::EofShort;
                    worker.eof_short_pending=worker.active_fill!=0;
                    break;
                }
                if(got==0) {
                    // A peer zero-byte write is NOT EOF or logical completion.
                    // Yield once to the next normal coordinator poll, preserving
                    // active fill and the buffer across Continue.
                    worker.kind=ReadyKind::NoProgress;break;
                }
                worker.active_fill+=got;
                if (worker.active_fill==pipe_buffer_bytes) { worker.count=worker.active_fill;worker.kind=ReadyKind::Full;worker.operation_open=false; }
            }
        }
        const ReadyKind published=worker.kind;
        if (!publish_ready(worker)) return finish_worker(worker,published==ReadyKind::ZeroEof?WorkerExitKind::LogicalEof:(published==ReadyKind::Error?WorkerExitKind::Fault:WorkerExitKind::Stopped));
        if (published==ReadyKind::ZeroEof || published==ReadyKind::Error) return finish_worker(worker,published==ReadyKind::ZeroEof?WorkerExitKind::LogicalEof:WorkerExitKind::Fault);
        if(published!=ReadyKind::NoProgress) worker.active_fill=0;
    }
}

bool start_workers(State& state) noexcept {
    for (U32 i=0;i<2;++i) {
        // One admitted setup group covers these events, CreateThread and started
        // wait (including worker entry/started completion). A pre-cut ticket
        // may finish after cut from the static tail; a later group cannot start.
        if (!admit_body(state,Family::WorkerStart)) return false;
        Worker& worker=state.workers[i]; worker.state=&state;
        if(worker.created||live(worker.ready)||live(worker.ack)||live(worker.started)||live(worker.thread)) {
            fail(state,Site::Read,Reason::SourceInvariant);return false;
        }
        worker.ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if (!live(worker.ready)) { const DWORD error=GetLastError();fail(state,Site::Spawn,Reason::ThreadSetupFailure,true,error);return false; }
        worker.ack=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if (!live(worker.ack)) { const DWORD error=GetLastError();fail(state,Site::Spawn,Reason::ThreadSetupFailure,true,error);return false; }
        worker.started=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if (!live(worker.started)) { const DWORD error=GetLastError();fail(state,Site::Spawn,Reason::ThreadSetupFailure,true,error);return false; }
        // Zero requests the selected executable's normal stack profile. Its
        // actual controlled frames/stack profile remain a separate ABI gate.
        worker.thread=CreateThread(nullptr,0,worker_entry,&worker,0,nullptr);
        if (!live(worker.thread)) { const DWORD error=GetLastError();fail(state,Site::Spawn,Reason::ThreadSetupFailure,true,error);return false; }
        worker.created=true;
        const DWORD wait=WaitForSingleObject(worker.started,cleanup_milliseconds);
        if (wait!=WAIT_OBJECT_0) {
            const DWORD error=wait==WAIT_FAILED?GetLastError():0;
            fail(state,Site::Spawn,Reason::ThreadSetupFailure,wait==WAIT_FAILED,error);return false;
        }
    }
    return true;
}

bool consume_ready(State& state,U32 direction,bool cleanup) noexcept {
    Worker& worker=state.workers[direction];
    if (!worker.created || worker.ready_consumed) return true;
    // This call follows a ready-event acquire or an observed thread exit.
    // A refused body debit enters tail immediately. The already acquired slot
    // still MUST be accounted once from its prepaid completion/tail sponsor.
    if(cleanup) (void)spend(state,Family::Consume);
    else if(!admit_body(state,Family::Consume)) cleanup=true;
    if(!worker.generation_valid) state.terminal.flags&=~CoordinatorCountersKnown;
    worker.consumed_generation=worker.generation; worker.ready_consumed=true;
    const ReadyKind kind=worker.kind;
    bool ok=true;
    if (kind==ReadyKind::Full || kind==ReadyKind::EofShort || kind==ReadyKind::ZeroEof) {
        if ((kind==ReadyKind::Full && worker.count!=pipe_buffer_bytes) ||
            (kind==ReadyKind::EofShort && (worker.count==0 || worker.count>=pipe_buffer_bytes)) ||
            (kind==ReadyKind::ZeroEof && worker.count!=0)) {
            fail(state,Site::Read,Reason::SourceInvariant);ok=false;
        } else {
            PipeCount& count=state.terminal.pipes[direction];
            ok=counter(state,count.observed,worker.count) && counter(state,count.unretained,worker.count);
            if (kind==ReadyKind::ZeroEof) state.terminal.flags|=direction==0?StdoutEof:StderrEof;
            else if (ok && !cleanup && !state.stop_body && !tail_requested(state.meter)) ok=write_chunk(state,direction,worker.buffer,worker.count);
        }
    } else if(kind==ReadyKind::NoProgress) {
        // A private continuation control record adds no observed/omitted bytes,
        // exposes no mutable partial, and never sets logical EOF.
        if(worker.count!=0 || worker.partial!=0) {fail(state,Site::Read,Reason::SourceInvariant);ok=false;}
    } else if (kind==ReadyKind::Error) {
        if (worker.partial>=pipe_buffer_bytes) { fail(state,Site::Read,Reason::SourceInvariant);ok=false; }
        else {
            state.terminal.pipes[direction].error_partial=worker.partial;
            state.terminal.flags|=direction==0?StdoutErrorPartialKnown:StderrErrorPartialKnown;
            state.terminal.cleanup_mask|=1u<<(4u+direction);
            fail(state,Site::Read,worker.error_reason,worker.error_native_known,worker.error);
            ok=false;
        }
    } else { fail(state,Site::Read,Reason::SourceInvariant);ok=false; }
    const bool more=ok && !cleanup && !state.stop_body && !tail_requested(state.meter) && kind!=ReadyKind::ZeroEof;
    if(!worker.settled) {
        if (!ResetEvent(worker.ready)) { const DWORD error=GetLastError();state.terminal.cleanup_mask|=1u<<3; fail(state,Site::WorkerSettlement,Reason::WorkerJoinFailure,true,error); }
        acknowledge(state,worker,more && !state.stop_body);
    }
    return ok;
}

bool observe_target_exit(State& state,bool cleanup) noexcept {
    if ((state.terminal.flags&TargetExitKnown)!=0) return true;
    if ((state.terminal.flags&ChildCreated)==0) return true;
    const DWORD wait=WaitForSingleObject(state.process,0);
    if (wait==WAIT_TIMEOUT) return false;
    if (wait!=WAIT_OBJECT_0) {
        const DWORD error=wait==WAIT_FAILED?GetLastError():0;
        state.target_exit_group_failed=true;
        fail(state,Site::TargetExit,Reason::TargetExitQueryFailure,wait==WAIT_FAILED,error);
        if (cleanup) state.terminal.cleanup_mask|=1u<<1;
        return false;
    }
    DWORD exit=0;
    if (!GetExitCodeProcess(state.process,&exit)) {
        const DWORD error=GetLastError(); state.target_exit_group_failed=true;
        fail(state,Site::TargetExit,Reason::TargetExitQueryFailure,true,error);
        if (cleanup) state.terminal.cleanup_mask|=1u<<1;
        return false;
    }
    state.terminal.target_raw_exit=exit;state.terminal.flags|=TargetExitKnown;
    return true;
}

void coordinator(State& state) noexcept {
    for (;;) {
        if (!admit_body(state,Family::Coordinator)) break;
        const bool exited=observe_target_exit(state,false);
        if (exited && (state.terminal.flags&(StdoutEof|StderrEof))==(StdoutEof|StderrEof)) break;
        if (state.stop_body) break;
        if (state.terminal.body_polls>=body_poll_limit) {
            state.terminal.flags|=BodyPollLimited;fail(state,Site::Deadline,Reason::PollLimit);break;
        }
        if (!admit_body(state,Family::Poll) || !counter(state,state.terminal.body_polls,1)) break;
        for (U32 i=0;i<2;++i) {
            Worker& worker=state.workers[i];
            if(worker.settled) continue;
            if(ready_now(state,worker)) consume_ready(state,i,false);
            // This common acquire path preserves a completed un-signaled slot
            // AND its distinct real exit diagnostics; lack of EOF is not cause.
            settle_one(state,i);
            if (state.stop_body || tail_requested(state.meter)) break;
        }
        if (state.stop_body || tail_requested(state.meter)) break;
        const U64 now=GetTickCount64();
        if (!state.body_clock_started || now<state.body_started) { fail(state,Site::Read,Reason::SourceInvariant);break; }
        if (now-state.body_started>1800000u) { state.terminal.flags|=BodyTimedOut;fail(state,Site::Deadline,Reason::BodyTimeout);break; }
        Sleep(poll_milliseconds);
    }
}

void cleanup_prefix(State& state) noexcept {
    begin_tail(state);stop_workers(state);
    for (U32 i=0;i<2;++i) {
        Worker& worker=state.workers[i];
        if (worker.created && !worker.settled) {
            if(ready_now(state,worker)) consume_ready(state,i,true);
            acknowledge(state,worker,false);
        }
    }
    cancel_workers(state);
    // A normal target exit still requires the descendant/job-empty proof.
    if (live(state.job) && ((state.terminal.flags&ChildCreated)!=0 || state.unknown_native_creation)) {
        if (!TerminateJobObject(state.job,1)) {
            const DWORD error=GetLastError();state.target_exit_group_failed=true;
            state.terminal.cleanup_mask=(state.terminal.cleanup_mask|2u)&~4u;
            fail(state,Site::JobSettlement,Reason::JobTerminationFailure,true,error);
        }
    }
    if ((state.terminal.flags&ChildCreated)!=0 && (state.terminal.flags&TargetExitKnown)==0) {
        const DWORD waited=WaitForSingleObject(state.process,cleanup_milliseconds);
        if (waited==WAIT_FAILED) { const DWORD error=GetLastError();state.target_exit_group_failed=true;fail(state,Site::TargetExit,Reason::TargetExitQueryFailure,true,error); }
        if (waited==WAIT_OBJECT_0) observe_target_exit(state,true);
        if ((state.terminal.flags&TargetExitKnown)==0) {
            if (state.target_exit_group_failed) state.terminal.cleanup_mask|=1u<<1;
            else { state.terminal.cleanup_mask|=1u<<2;fail(state,Site::TargetExit,Reason::TargetExitUnobserved); }
        }
    }
    const U64 start=GetTickCount64();
    for (U32 retry=0;retry<=8;++retry) {
        for (U32 i=0;i<2;++i) {
            Worker& worker=state.workers[i];
            if (worker.created && !worker.settled) {
                if(ready_now(state,worker)) consume_ready(state,i,true);
                acknowledge(state,worker,false);
            }
            settle_one(state,i);
        }
        if (workers_settled(state)) break;
        const U64 now=GetTickCount64();
        if (retry==8 || now<start || now-start>=cleanup_milliseconds) break;
        cancel_workers(state);
        Sleep(poll_milliseconds);
    }
    if (!workers_settled(state)) {
        state.terminal.cleanup_mask|=(1u<<3)|(1u<<14);
        fail(state,Site::WorkerSettlement,Reason::WorkerJoinUnobserved);
    }
    // Release observed target process/thread references before asking for the
    // current job population. This selected native semantic is still gated.
    if ((state.terminal.flags&TargetExitKnown)!=0) {
        HANDLE* owners[2]={&state.primary_thread,&state.process};
        for (U32 i=0;i<2;++i) if (live(*owners[i])) {
            if (!CloseHandle(*owners[i])) { const DWORD error=GetLastError();state.terminal.cleanup_mask|=1u<<13;fail(state,Site::OwnedClose,Reason::CloseFailure,true,error); }
            else *owners[i]=nullptr;
        }
    }
    if (!state.job_ever_created && (state.terminal.flags&ChildCreated)==0) state.terminal.flags|=JobEmptyProved;
    else if (live(state.job)) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
        if (!QueryInformationJobObject(state.job,JobObjectBasicAccountingInformation,&accounting,sizeof(accounting),nullptr)) { const DWORD error=GetLastError();fail(state,Site::JobSettlement,Reason::JobQueryFailure,true,error); }
        else if (accounting.ActiveProcesses==0) state.terminal.flags|=JobEmptyProved;
        else fail(state,Site::JobSettlement,Reason::JobNotEmpty);
    }
    for (U32 i=0;i<2;++i) if (live(state.logs[i])) {
        if (!FlushFileBuffers(state.logs[i])) { const DWORD error=GetLastError();state.terminal.cleanup_mask|=1u<<(6u+i);fail(state,Site::Write,Reason::FlushFailure,true,error); }
        else state.terminal.flags|=i==0?StdoutFlushed:StderrFlushed;
        observe_position(state,i,true);
    }
    close_safe_owners(state,false);
}

void finalize_record(State& state) noexcept {
    Terminal& t=state.terminal;
    if ((t.cleanup_mask&2u)!=0) t.cleanup_mask&=~4u;
    if ((t.flags&CoordinatorCountersKnown)==0) zero_unavailable_counters(t);
    const bool settled=(t.flags&(StdoutSettled|StderrSettled|JobEmptyProved))==(StdoutSettled|StderrSettled|JobEmptyProved);
    const bool ran=(t.flags&TargetMayHaveRun)!=0;
    const bool exited=(t.flags&TargetExitKnown)!=0;
    if (!settled || (ran&&!exited) || !state.meter.valid || (t.flags&CoordinatorCountersKnown)==0 || (t.cleanup_mask&0x3c00u)!=0 || state.unknown_native_creation) { t.status=4;t.cleanup_mask|=1u<<14; }
    else if (!ran) {
        if (t.primary.site==0) fail(state,Site::Read,Reason::SourceInvariant);
        t.status=2;
    } else if (t.primary.site!=0 || t.secondary_count!=0 || t.cleanup_mask!=0 || !state.meter.valid) t.status=3;
    else t.status=t.target_raw_exit==0?0u:1u;
    if (t.status<=1) {
        const U32 needed=ChildCreated|TargetMayHaveRun|TargetExitKnown|StdoutSettled|StderrSettled|StdoutEof|StderrEof|
            JobEmptyProved|StdoutStartKnown|StdoutEndKnown|StderrStartKnown|StderrEndKnown|PrefixKnown|
            StdoutErrorPartialKnown|StderrErrorPartialKnown|StdoutFlushed|StderrFlushed|CoordinatorCountersKnown;
        if ((t.flags&needed)!=needed) { fail(state,Site::Read,Reason::SourceInvariant);t.status=4;t.cleanup_mask|=1u<<14; }
    }
    if (!validate_terminal(state)) {
        // No malformed/noncurrent record is manufactured. Exit10 preserves
        // every owner/file and the parent retains the whole reservation.
        state.request.current=false;
    }
}

[[noreturn]] void finish(State& state,DWORD code) noexcept {
    // Pending worker buffers/stack owners do not unwind or recycle. Neither
    // ExitProcess nor the last job-handle close earns resource refund credit.
    if (state.record_frozen && state.tail_failed) code=11;
    ExitProcess(code);
}

int supervisor_main() noexcept {
    State state;
    if (!source_review_accepted || !source_work_accepted || !native_profile_accepted ||
        !controlled_resource_accepted || !execution_accepted) finish(state,10);
    if (!validate_profile(state) || !parse_command(state) || !read_request(state) ||
        !decode_request(state) || !validate_request_recipe(state)) {
        begin_tail(state);close_safe_owners(state,false);finish(state,10);
    }
    bool ready=build_target_command(state) && build_environment(state) && open_capture_owners(state) && create_contained_target(state) && start_workers(state);
    // The resume ticket is linearized BEFORE MayHaveRun and the native call.
    // Cut-before-ticket keeps the established target suspended. Cut-after-ticket
    // cannot revoke an already admitted call; its completion is prepaid tail.
    if (ready && admit_body(state,Family::TargetResume)) {
        state.terminal.flags|=TargetMayHaveRun;
        const DWORD previous=ResumeThread(state.primary_thread);
        if (previous==static_cast<DWORD>(-1)) { const DWORD error=GetLastError();fail(state,Site::Spawn,Reason::ResumeFailure,true,error); }
        else if (previous!=1) fail(state,Site::Spawn,Reason::ResumeFailure);
        else { state.body_started=GetTickCount64();state.body_clock_started=true;coordinator(state); }
    }
    cleanup_prefix(state);finalize_record(state);
    if (!state.request.current) finish(state,10);
    const bool published=publish_terminal(state);
    if (!state.record_frozen) finish(state,10);
    // No record rewrite after this point. Publication and every remaining
    // mandatory action are prepaid and an unrecorded failure changes exit.
    close_safe_owners(state,true);
    if (!published || state.tail_failed) finish(state,11);
    if (!state.meter.valid) finish(state,13);
    if (state.terminal.status==4) finish(state,12);
    finish(state,0);
}
}

int wmain() {
    return yesdaw_clock_supervisor_r4::supervisor_main();
}
