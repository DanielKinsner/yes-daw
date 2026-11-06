#include "sup.h"

namespace yesdaw_clock_supervisor_r4 {
namespace {
bool live(HANDLE value) noexcept { return value != nullptr && value != INVALID_HANDLE_VALUE; }
void capture_raw(U8* out,const void* object,SIZE_T bytes) noexcept {
    // Copy an output object's representation without interpreting any failed
    // creator's pointer/integer words as established native owners or identity.
    const U8* raw=static_cast<const U8*>(object);
    for(SIZE_T i=0;i<bytes;++i) out[i]=raw[i];
}
bool close_one(State& state, HANDLE& value, U32 mask = 0) noexcept {
    if (!live(value)) { value = nullptr; return true; }
    const HANDLE held = value;
    // A failed CloseHandle is not credited as release. Retain the owner value.
    if (!CloseHandle(held)) {
        const DWORD error = GetLastError();
        state.terminal.cleanup_mask |= mask;
        fail(state, Site::OwnedClose, Reason::CloseFailure, true, error);
        return false;
    }
    value = nullptr;
    return true;
}
bool path_failure(State& state, Site site, Reason reason, bool known = false, DWORD error = 0) noexcept {
    if (site == Site::StageAdmission) state.terminal.flags |= StageGuardRejected;
    fail(state, site, reason, known, error);
    return false;
}
bool same_name(const wchar_t* candidate, const wchar_t* name, U32 length) noexcept {
    for (U32 i = 0; i < length; ++i) if (candidate[i] != name[i]) return false;
    return candidate[length] == 0;
}
bool allowed_stage_name(const wchar_t* name) noexcept {
    return same_name(name,L"stdout.log",10) || same_name(name,L"stderr.log",10) ||
        same_name(name,L"receipt.json",12) || same_name(name,L"bundle.json",11) ||
        same_name(name,L"consumed.json",13) || same_name(name,L"failure-inventory.json",22);
}
bool native_count(State& state, U64& target, U64 amount) noexcept {
    U64 result = 0;
    if (!checked_add(target, amount, result)) {
        state.terminal.flags &= ~CoordinatorCountersKnown;
        fail(state, Site::Write, Reason::CounterOverflow);
        return false;
    }
    target = result;
    return true;
}
}

bool validate_path(State& state, const Wide& path, bool directory, Site site) noexcept {
    if (!spend(state, Family::PathGuard) ||
        !spend(state, Family::PathCell, path.length)) return false;
    const Reason io = site == Site::StageAdmission ? Reason::StageIo : Reason::OwnedPathFailure;
    if (path.length == 0 || path.length > path_units || path.cells[path.length] != 0)
        return path_failure(state, site, site == Site::StageAdmission ? Reason::StageExtent : Reason::OwnedPathFailure);
    // The bounded full-path result must be the currently admitted path. This
    // performs no cache/8.3/tilde equivalence substitution.
    wchar_t full[path_units + 1];
    const DWORD n = GetFullPathNameW(path.cells, path_units + 1, full, nullptr);
    if (n == 0) { const DWORD error=GetLastError(); return path_failure(state,site,io,true,error); }
    if (n > path_units || !equal_wide(full,n,path.cells,path.length,true))
        return path_failure(state,site,site==Site::StageAdmission?Reason::StageExtent:Reason::OwnedPathFailure);
    wchar_t volume[path_units + 1];
    if (!GetVolumePathNameW(path.cells, volume, path_units + 1)) {
        const DWORD error=GetLastError(); return path_failure(state,site,io,true,error);
    }
    U32 root = 0;
    while (root <= path_units && volume[root] != 0) ++root;
    if (root == 0 || root > path.length || root > path_units)
        return path_failure(state,site,io);
    Wide current = path;
    U32 depth = 0;
    bool first = true;
    for (;;) {
        if (++depth > 64) return path_failure(state,site,site==Site::StageAdmission?Reason::StageDepth:Reason::OwnedPathFailure);
        if (!spend(state,Family::PathGuard)) return false;
        const DWORD attributes=GetFileAttributesW(current.cells);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            const DWORD error=GetLastError(); return path_failure(state,site,io,true,error);
        }
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            return path_failure(state,site,site==Site::StageAdmission?Reason::StageReparse:Reason::OwnedPathFailure);
        if ((first && (((attributes & FILE_ATTRIBUTE_DIRECTORY)!=0) != directory)) ||
            (!first && (attributes & FILE_ATTRIBUTE_DIRECTORY)==0))
            return path_failure(state,site,io);
        first=false;
        while (current.length > root && (current.cells[current.length-1]==L'\\' || current.cells[current.length-1]==L'/'))
            current.cells[--current.length]=0;
        if (current.length <= root) break;
        U32 cut=current.length;
        while (cut > root && current.cells[cut-1]!=L'\\' && current.cells[cut-1]!=L'/') --cut;
        current.length = cut > root ? cut-1 : root;
        current.cells[current.length]=0;
    }
    return true;
}

bool read_request(State& state) noexcept {
    if(live(state.request_file)) return false;
    if (!validate_path(state,state.request.request_path,false)) return false;
    HANDLE file=CreateFileW(state.request.request_path.cells,GENERIC_READ,FILE_SHARE_READ,
                           nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (!live(file)) { const DWORD error=GetLastError(); fail(state,Site::Request,Reason::RequestIo,true,error); return false; }
    state.request_file=file;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file,&size)) { const DWORD error=GetLastError(); fail(state,Site::Request,Reason::RequestIo,true,error); return false; }
    if (size.QuadPart < request_header_bytes || size.QuadPart > request_max_bytes) {
        fail(state,Site::Request,Reason::RequestLengthOrEof); return false;
    }
    const U32 length=static_cast<U32>(size.QuadPart);
    U32 at=0;
    while (at < length) {
        if (!spend(state,Family::Decode)) return false;
        DWORD got=0;
        if (!ReadFile(file,state.request.capture+at,length-at,&got,nullptr)) {
            const DWORD error=GetLastError(); fail(state,Site::Request,Reason::RequestIo,true,error); return false;
        }
        if (got==0 || got>length-at) { fail(state,Site::Request,Reason::RequestLengthOrEof); return false; }
        at+=got;
    }
    U8 extra=0; DWORD got=0;
    if (!ReadFile(file,&extra,1,&got,nullptr)) { const DWORD error=GetLastError(); fail(state,Site::Request,Reason::RequestIo,true,error); return false; }
    if (got!=0) { fail(state,Site::Request,Reason::RequestLengthOrEof); return false; }
    state.request.captured=length;
    return true;
}

bool hash_file(State& state, const Wide& path, U64 maximum, Digest& digest, U64& length) noexcept {
    if (!spend(state,Family::HashFile) || !validate_path(state,path,false)) return false;
    if(live(state.hash_file_owner)) return false;
    HANDLE file=CreateFileW(path.cells,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (!live(file)) { const DWORD error=GetLastError(); fail(state,Site::Request,Reason::CurrentIdentity,true,error); return false; }
    state.hash_file_owner=file;
    bool ok=true;
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file,&size)) { const DWORD error=GetLastError(); fail(state,Site::Request,Reason::CurrentIdentity,true,error); ok=false; }
    if (ok && (size.QuadPart<0 || static_cast<U64>(size.QuadPart)>maximum)) { fail(state,Site::Request,Reason::CurrentIdentity); ok=false; }
    Sha256 hash;
    U64 at=0;
    if (ok) ok=sha_init(state,hash);
    while (ok && at<static_cast<U64>(size.QuadPart)) {
        const U64 remaining=static_cast<U64>(size.QuadPart)-at;
        const DWORD want=remaining>65536?65536:static_cast<DWORD>(remaining);
        if (!spend(state,Family::HashChunk)) { ok=false; break; }
        DWORD got=0;
        if (!ReadFile(file,state.hash_buffer,want,&got,nullptr)) { const DWORD error=GetLastError(); fail(state,Site::Request,Reason::CurrentIdentity,true,error); ok=false; break; }
        if (got==0 || got>want) { fail(state,Site::Request,Reason::CurrentIdentity); ok=false; break; }
        ok=sha_update(state,hash,state.hash_buffer,got);
        at+=got;
    }
    if (ok) {
        U8 extra=0; DWORD got=0;
        if (!ReadFile(file,&extra,1,&got,nullptr)) { const DWORD error=GetLastError(); fail(state,Site::Request,Reason::CurrentIdentity,true,error); ok=false; }
        else if (got!=0) { fail(state,Site::Request,Reason::CurrentIdentity); ok=false; }
    }
    if (ok) ok=sha_finish(state,hash,digest);
    if (ok) length=at;
    if (!close_one(state,state.hash_file_owner,1u<<13)) ok=false;
    return ok;
}

bool stage_scan(State& state, U64 additional, U64& extent) noexcept {
    if (!spend(state,Family::StageScan)) return false;
    // A failed FindClose retains its sole owner slot. Never overwrite it with
    // another search, including the mandatory publication scans.
    if (state.search_owner!=INVALID_HANDLE_VALUE) return false;
    if (!validate_path(state,state.request.stage,true,Site::StageAdmission)) return false;
    Wide pattern;
    const PathResult pattern_result=join_path(state,state.request.stage,L"*",1,pattern);
    if (pattern_result!=PathResult::Ok) {
        if(pattern_result==PathResult::ExtentRejected) return path_failure(state,Site::StageAdmission,Reason::StageExtent);
        return false;
    }
    WIN32_FIND_DATAW entry{};
    HANDLE search=FindFirstFileW(pattern.cells,&entry);
    if (search==INVALID_HANDLE_VALUE) {
        const DWORD error=GetLastError();
        if (error==ERROR_FILE_NOT_FOUND) { extent=0; }
        else return path_failure(state,Site::StageAdmission,Reason::StageIo,true,error);
    } else {
        state.search_owner=search;
        U64 sum=0; U32 count=0; bool ok=true;
        bool seen_dot=false,seen_dotdot=false;
        for (;;) {
            if (!spend(state,Family::StageEntry)) { ok=false; break; }
            if (same_name(entry.cFileName,L".",1)) {
                if (seen_dot) { ok=path_failure(state,Site::StageAdmission,Reason::StageExtent); break; }
                seen_dot=true;
            } else if (same_name(entry.cFileName,L"..",2)) {
                if (seen_dotdot) { ok=path_failure(state,Site::StageAdmission,Reason::StageExtent); break; }
                seen_dotdot=true;
            } else {
                if (++count>7 || !allowed_stage_name(entry.cFileName)) { ok=path_failure(state,Site::StageAdmission,Reason::StageUnknownName); break; }
                U32 n=0; while (n<MAX_PATH && entry.cFileName[n]!=0) ++n;
                Wide child;
                if (n==MAX_PATH) { ok=path_failure(state,Site::StageAdmission,Reason::StageExtent); break; }
                const PathResult child_result=join_path(state,state.request.stage,entry.cFileName,n,child);
                if(child_result!=PathResult::Ok) {
                    if(child_result==PathResult::ExtentRejected) path_failure(state,Site::StageAdmission,Reason::StageExtent);
                    ok=false;break;
                }
                if (!validate_path(state,child,false,Site::StageAdmission)) { ok=false; break; }
                WIN32_FILE_ATTRIBUTE_DATA current{};
                if (!GetFileAttributesExW(child.cells,GetFileExInfoStandard,&current)) { const DWORD error=GetLastError(); ok=path_failure(state,Site::StageAdmission,Reason::StageIo,true,error); break; }
                if ((current.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))!=0) { ok=path_failure(state,Site::StageAdmission,Reason::StageReparse); break; }
                const U64 bytes=(static_cast<U64>(current.nFileSizeHigh)<<32)|current.nFileSizeLow;
                U64 next=0;
                if (!checked_add(sum,bytes,next)) { ok=path_failure(state,Site::StageAdmission,Reason::StageExtent); break; }
                sum=next;
            }
            if (!FindNextFileW(search,&entry)) {
                const DWORD error=GetLastError();
                if (error!=ERROR_NO_MORE_FILES) ok=path_failure(state,Site::StageAdmission,Reason::StageIo,true,error);
                break;
            }
        }
        if (!FindClose(search)) { const DWORD error=GetLastError();state.terminal.cleanup_mask|=1u<<13; fail(state,Site::OwnedClose,Reason::CloseFailure,true,error); ok=false; }
        else state.search_owner=INVALID_HANDLE_VALUE;
        if (!ok) return false;
        extent=sum;
    }
    if (extent>stage_limit || additional>stage_limit-extent) {
        state.terminal.flags|=StagePoolCap;
        fail(state,Site::StageAdmission,Reason::StagePoolCap);
        return false;
    }
    return true;
}

bool observe_position(State& state, U32 direction, bool end) noexcept {
    LARGE_INTEGER zero{},position{};
    const BOOL queried=live(state.logs[direction])?SetFilePointerEx(state.logs[direction],zero,&position,FILE_CURRENT):FALSE;
    const DWORD error=live(state.logs[direction]) && !queried?GetLastError():0;
    if (!queried || position.QuadPart<0) {
        state.terminal.cleanup_mask|=1u<<(8u+direction);
        fail(state,Site::Write,Reason::PositionFailure,live(state.logs[direction]) && !queried,error);
        return false;
    }
    PipeCount& count=state.terminal.pipes[direction];
    if (end) { count.end=static_cast<U64>(position.QuadPart); state.terminal.flags|=direction==0?StdoutEndKnown:StderrEndKnown; }
    else { count.start=static_cast<U64>(position.QuadPart); state.terminal.flags|=direction==0?StdoutStartKnown:StderrStartKnown; }
    return true;
}

bool open_capture_owners(State& state) noexcept {
    if(!admit_body(state,Family::CaptureSetup)) return false;
    if(state.unknown_native_creation) return false;
    for(U32 i=0;i<2;++i) if(live(state.logs[i])||live(state.workers[i].pipe)||live(state.child_writes[i])) return false;
    if(live(state.nul_input)) return false;
    U64 extent=0;
    if (!stage_scan(state,0,extent)) return false;
    for (U32 i=0;i<2;++i) {
        if (!admit_body(state,Family::Owner)) return false;
        Wide log;
        const PathResult log_result=join_path(state,state.request.stage,i==0?L"stdout.log":L"stderr.log",10,log);
        if(log_result!=PathResult::Ok) {
            if(log_result==PathResult::ExtentRejected) path_failure(state,Site::StageAdmission,Reason::StageExtent);
            return false;
        }
        HANDLE handle=CreateFileW(log.cells,GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (!live(handle)) { const DWORD error=GetLastError(); fail(state,Site::Write,Reason::WriteFailure,true,error); return false; }
        state.logs[i]=handle;
        LARGE_INTEGER zero{};
        if (!SetFilePointerEx(handle,zero,nullptr,FILE_END)) { const DWORD error=GetLastError(); fail(state,Site::Write,Reason::PositionFailure,true,error); return false; }
        if (!observe_position(state,i,false)) return false;
    }
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    for (U32 i=0;i<2;++i) {
        HANDLE read=nullptr,write=nullptr;
        const BOOL ok=CreatePipe(&read,&write,&security,0);
        const DWORD pipe_error=ok?0:GetLastError();
        if (!ok) {
            capture_raw(state.failed_pipe_output[i],&read,sizeof(read));
            capture_raw(state.failed_pipe_output[i]+sizeof(read),&write,sizeof(write));
            state.failed_pipe_captured[i]=true;state.unknown_native_creation=true;
            fail(state,Site::Containment,Reason::NativeOwnerAllocationFailure,true,pipe_error);return false;
        }
        if(!live(read)||!live(write)) {
            capture_raw(state.failed_pipe_output[i],&read,sizeof(read));
            capture_raw(state.failed_pipe_output[i]+sizeof(read),&write,sizeof(write));
            state.failed_pipe_captured[i]=true;state.unknown_native_creation=true;
            fail(state,Site::Containment,Reason::HandleInvariant);return false;
        }
        // Only a successful creator with its success contract establishes owners.
        state.workers[i].pipe=read;
        state.child_writes[i]=write;
        if (!SetHandleInformation(read,HANDLE_FLAG_INHERIT,0)) { const DWORD error=GetLastError(); fail(state,Site::Containment,Reason::HandleInvariant,true,error); return false; }
    }
    state.nul_input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (!live(state.nul_input)) { const DWORD error=GetLastError(); fail(state,Site::Containment,Reason::StdinNulSetupFailure,true,error); return false; }
    return true;
}

bool create_contained_target(State& state) noexcept {
    if (!admit_body(state,Family::Owner)) return false;
    if(state.unknown_native_creation||live(state.job)||live(state.process)||live(state.primary_thread)||state.attribute_allocation!=nullptr) return false;
    state.job=CreateJobObjectW(nullptr,nullptr);
    if (!live(state.job)) { const DWORD error=GetLastError(); fail(state,Site::Containment,Reason::JobSetup,true,error); return false; }
    state.job_ever_created=true;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(state.job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) { const DWORD error=GetLastError(); fail(state,Site::Containment,Reason::JobSetup,true,error); return false; }
    SIZE_T bytes=0;
    const BOOL queried=InitializeProcThreadAttributeList(nullptr,2,0,&bytes);
    const DWORD query_error=queried?0:GetLastError();
    if (queried || query_error!=ERROR_INSUFFICIENT_BUFFER || bytes==0 || bytes!=Data.attribute_bytes) { fail(state,Site::Containment,Reason::UnsupportedJobProfile, !queried,query_error); return false; }
    state.attribute_allocation=HeapAlloc(GetProcessHeap(),0,bytes);
    state.attribute_bytes=bytes;
    if (state.attribute_allocation==nullptr) { fail(state,Site::Containment,Reason::NativeOwnerAllocationFailure); return false; }
    state.attributes=static_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(state.attribute_allocation);
    if (!InitializeProcThreadAttributeList(state.attributes,2,0,&bytes)) { const DWORD error=GetLastError(); state.attributes=nullptr; fail(state,Site::Containment,Reason::JobSetup,true,error); return false; }
    state.inherited[0]=state.nul_input; state.inherited[1]=state.child_writes[0]; state.inherited[2]=state.child_writes[1];
    state.job_attribute[0]=state.job;
    if (!UpdateProcThreadAttribute(state.attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,state.inherited,sizeof(state.inherited),nullptr,nullptr)) { const DWORD error=GetLastError(); fail(state,Site::Containment,Reason::JobSetup,true,error); return false; }
    if (!UpdateProcThreadAttribute(state.attributes,0,PROC_THREAD_ATTRIBUTE_JOB_LIST,state.job_attribute,sizeof(state.job_attribute),nullptr,nullptr)) { const DWORD error=GetLastError(); fail(state,Site::Containment,Reason::UnsupportedJobProfile,true,error); return false; }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb=sizeof(startup);
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput=state.nul_input;
    startup.StartupInfo.hStdOutput=state.child_writes[0];
    startup.StartupInfo.hStdError=state.child_writes[1];
    startup.lpAttributeList=state.attributes;
    PROCESS_INFORMATION process{};
    const BOOL created=CreateProcessW(state.request.target.cells,state.target_command,nullptr,nullptr,TRUE,
        CREATE_SUSPENDED|CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT|EXTENDED_STARTUPINFO_PRESENT,
        state.environment,state.request.working.cells,&startup.StartupInfo,&process);
    const DWORD create_error=created?0:GetLastError();
    if (!created) {
        capture_raw(state.failed_process_output,&process,sizeof(process));
        state.failed_process_captured=true;state.unknown_native_creation=true;
        fail(state,Site::Spawn,Reason::CreateProcessFailure,true,create_error); return false;
    }
    if (!live(process.hProcess) || !live(process.hThread) || process.dwProcessId==0) {
        capture_raw(state.failed_process_output,&process,sizeof(process));
        state.failed_process_captured=true;state.unknown_native_creation=true;
        fail(state,Site::Containment,Reason::HandleInvariant); return false;
    }
    state.process=process.hProcess;state.primary_thread=process.hThread;
    state.terminal.flags|=ChildCreated;state.terminal.target_pid=process.dwProcessId;
    bool closed=true;
    for (U32 i=0;i<2;++i) if (!close_one(state,state.child_writes[i],1u<<13)) closed=false;
    if (!close_one(state,state.nul_input,1u<<13)) closed=false;
    return closed;
}

bool write_chunk(State& state, U32 direction, const U8* bytes, U32 length) noexcept {
    if (!admit_body(state,Family::WriteChunk)) return false;
    PipeCount& count=state.terminal.pipes[direction];
    U64 retained=0;
    if (!checked_add(state.terminal.pipes[0].written,state.terminal.pipes[1].written,retained)) {
        state.terminal.flags&=~CoordinatorCountersKnown; fail(state,Site::Write,Reason::CounterOverflow); return false;
    }
    if (retained>raw_log_limit || length>raw_log_limit-retained) {
        if (!native_count(state,count.omitted,length)) return false;
        state.terminal.flags|=RawLogCap;
        fail(state,Site::StageAdmission,Reason::RawLogCap); return false;
    }
    U64 extent=0;
    if (!stage_scan(state,length,extent)) {
        if ((state.terminal.flags&StagePoolCap)!=0) native_count(state,count.omitted,length);
        return false;
    }
    U32 at=0;
    while (at<length) {
        // Revalidate the COMPLETE pool before every actual short-write attempt.
        if (!stage_scan(state,length-at,extent)) {
            if((state.terminal.flags&StagePoolCap)!=0) native_count(state,count.omitted,length-at);
            return false;
        }
        if (!admit_write(state)) return false;
        DWORD written=0;
        if (!WriteFile(state.logs[direction],bytes+at,length-at,&written,nullptr)) { const DWORD error=GetLastError(); fail(state,Site::Write,Reason::WriteFailure,true,error); return false; }
        if (written==0 || written>length-at) { fail(state,written==0?Site::Write:Site::Read,written==0?Reason::ZeroWriteProgress:Reason::SourceInvariant); return false; }
        if (!native_count(state,count.written,written) || written>count.unretained) { state.terminal.flags&=~CoordinatorCountersKnown; fail(state,Site::Write,Reason::CounterOverflow); return false; }
        count.unretained-=written;
        at+=written;
    }
    return true;
}

void close_safe_owners(State& state, bool after_record) noexcept {
    if (after_record && !state.record_frozen) { fail(state,Site::Meter,Reason::LateTailSourceInvariant); return; }
    for (U32 i=0;i<2;++i) {
        Worker& worker=state.workers[i];
        if (worker.settled || !worker.created) {
            close_one(state,worker.pipe,1u<<(10u+i));
            close_one(state,worker.thread,1u<<12);
            close_one(state,worker.ready,1u<<12);
            close_one(state,worker.ack,1u<<12);
            close_one(state,worker.started,1u<<12);
        }
        close_one(state,state.logs[i],1u<<(10u+i));
        close_one(state,state.child_writes[i],1u<<13);
    }
    close_one(state,state.nul_input,1u<<13);
    // Closing process references cannot stand in for a job-empty observation.
    if ((state.terminal.flags&TargetExitKnown)!=0 || (!live(state.process)&&!live(state.primary_thread))) {
        close_one(state,state.primary_thread,1u<<13);
        close_one(state,state.process,1u<<13);
    }
    if ((state.terminal.flags&JobEmptyProved)!=0) close_one(state,state.job,1u<<13);
    if (state.attributes!=nullptr) { DeleteProcThreadAttributeList(state.attributes); state.attributes=nullptr; }
    if (state.attribute_allocation!=nullptr) {
        if (!HeapFree(GetProcessHeap(),0,state.attribute_allocation)) { const DWORD error=GetLastError(); state.terminal.cleanup_mask|=1u<<13; fail(state,Site::OwnedClose,Reason::CloseFailure,true,error); }
        else state.attribute_allocation=nullptr;
    }
    close_one(state,state.request_file,1u<<13);
    if (after_record) close_one(state,state.terminal_file);
    close_one(state,state.hash_file_owner,1u<<13);
    if (state.search_owner!=INVALID_HANDLE_VALUE) {
        if (!FindClose(state.search_owner)) { const DWORD error=GetLastError();state.terminal.cleanup_mask|=1u<<13;fail(state,Site::OwnedClose,Reason::CloseFailure,true,error); }
        else state.search_owner=INVALID_HANDLE_VALUE;
    }
}

bool publish_terminal(State& state) noexcept {
    if (!encode_terminal(state)) { state.request.current=false; return false; }
    // The immutable record is terminal_capture, not the later suffix's local
    // diagnostic accumulator. No API/error after this cut rewrites its bytes.
    state.record_frozen=true;
    U64 extent=0;
    if (!stage_scan(state,0,extent) || !validate_path(state,state.request.transport,true)) { state.tail_failed=true; return false; }
    state.terminal_file=CreateFileW(state.request.terminal_path.cells,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if (!live(state.terminal_file)) { const DWORD error=GetLastError(); fail(state,Site::TerminalPublication,Reason::PublicationFailure,true,error); return false; }
    U32 at=0;
    while (at<terminal_bytes) {
        if (!stage_scan(state,0,extent)) return false;
        DWORD written=0;
        if (!WriteFile(state.terminal_file,state.terminal_capture+at,terminal_bytes-at,&written,nullptr)) { const DWORD error=GetLastError(); fail(state,Site::TerminalPublication,Reason::PublicationFailure,true,error); return false; }
        if (written==0 || written>terminal_bytes-at) { fail(state,Site::TerminalPublication,Reason::PublicationFailure); return false; }
        at+=written;
    }
    if (!FlushFileBuffers(state.terminal_file)) { const DWORD error=GetLastError(); fail(state,Site::TerminalPublication,Reason::PublicationFailure,true,error); return false; }
    return close_one(state,state.terminal_file);
}
}
