#include "sup.h"

namespace yesdaw_clock_supervisor_r1 {
namespace {
U32 get32(const U8* p) noexcept {
    return static_cast<U32>(p[0]) | (static_cast<U32>(p[1])<<8) |
           (static_cast<U32>(p[2])<<16) | (static_cast<U32>(p[3])<<24);
}
U64 get64(const U8* p) noexcept {
    return static_cast<U64>(get32(p)) | (static_cast<U64>(get32(p+4))<<32);
}
void put32(U8* p,U32 value) noexcept {
    for(U32 i=0;i<4;++i) p[i]=static_cast<U8>(value>>(8u*i));
}
void put64(U8* p,U64 value) noexcept {
    for(U32 i=0;i<8;++i) p[i]=static_cast<U8>(value>>(8u*i));
}
U32 cell(Text text,U32 at) noexcept {
    return static_cast<U32>(text.bytes[2u*at]) |
           (static_cast<U32>(text.bytes[2u*at+1u])<<8);
}
bool high(U32 ch) noexcept { return ch>=0xd800 && ch<=0xdbff; }
bool low(U32 ch) noexcept { return ch>=0xdc00 && ch<=0xdfff; }
bool valid_wide(const wchar_t* value,U32 length,bool ascii,bool path) noexcept {
    for(U32 i=0;i<length;++i) {
        const U32 ch=static_cast<U32>(value[i]);
        if((ascii && (ch<32 || ch>126)) || (path && ch==0)) return false;
        if(high(ch)) {
            if(i+1u>=length || !low(static_cast<U32>(value[i+1u]))) return false;
        } else if(low(ch) && (i==0 || !high(static_cast<U32>(value[i-1u])))) return false;
    }
    return true;
}
bool wide_from_binding(State& state,const wchar_t* value,U32 length,Wide& out) noexcept {
    if(!charge(state.meter,Family::StringGuard) ||
       !charge(state.meter,Family::StringCell,length)) return false;
    if(value==nullptr || length==0 || length>path_units || !valid_wide(value,length,false,true)) return false;
    for(U32 i=0;i<length;++i) out.cells[i]=value[i];
    out.cells[length]=0;out.length=length;
    return true;
}
bool literal(Text value,const wchar_t* text,U32 length) noexcept {
    if(value.length!=length) return false;
    for(U32 i=0;i<length;++i) if(cell(value,i)!=static_cast<U32>(text[i])) return false;
    return true;
}
bool append(wchar_t* out,U32 capacity,U32& used,wchar_t ch) noexcept {
    if(used>=capacity-1u) return false;
    out[used++]=ch;out[used]=0;
    return true;
}
bool text_matches(Text value,const Wide& expected) noexcept {
    if(value.length!=expected.length) return false;
    for(U32 i=0;i<value.length;++i) if(cell(value,i)!=static_cast<U32>(expected.cells[i])) return false;
    return true;
}
bool pair(U32 site,U32 reason) noexcept {
    if(reason==0) return site==0;
    if(reason<=10) return site==1;
    switch(reason) {
    case 11:case 12:case 43:case 44:case 46:return site==2;
    case 13:case 14:case 42:return site==3;
    case 15:case 40:case 41:return site==4;
    case 16:case 17:case 18:case 19:case 20:case 21:case 22:return site==5;
    case 23:case 24:case 33:case 34:return site==6;
    case 25:case 26:return site==7;
    case 27:case 28:return site==8;
    case 29:case 30:return site==9;
    case 31:case 32:return site==10;
    case 35:return site==13;
    case 36:return site==11;
    case 37:case 38:case 47:return site==12;
    case 39:return site==4 || site==6;
    case 45:return site==1 || site==5;
    default:return false;
    }
}
bool same_failure(const Failure& a,const Failure& b) noexcept {
    return a.site==b.site && a.reason==b.reason && a.native_error==b.native_error && a.native_known==b.native_known;
}
bool reason_present(const Terminal& t,U32 reason) noexcept {
    return t.primary.reason==reason || t.first_secondary.reason==reason ||
           t.first_native_secondary.reason==reason;
}
bool append_template(State& state,const wchar_t* input,U32 length,Wide& out) noexcept {
    if(input==nullptr || length>path_units || !charge(state.meter,Family::StringCell,length)) return false;
    out.length=0;out.cells[0]=0;
    for(U32 i=0;i<length;) {
        const wchar_t* replacement=nullptr;U32 replacement_length=0;U32 token=0;
        if(i+6u<=length && input[i]=='<' && input[i+1]=='B' && input[i+2]=='A' && input[i+3]=='S' && input[i+4]=='E' && input[i+5]=='>') {
            replacement=Data.source_base;replacement_length=Data.source_base_length;token=6;
        } else if(i+6u<=length && input[i]=='<' && input[i+1]=='R' && input[i+2]=='E' && input[i+3]=='P' && input[i+4]=='O' && input[i+5]=='>') {
            replacement=Data.repo;replacement_length=Data.repo_length;token=6;
        } else if(i+7u<=length && input[i]=='<' && input[i+1]=='B' && input[i+2]=='U' && input[i+3]=='I' && input[i+4]=='L' && input[i+5]=='D' && input[i+6]=='>') {
            Wide build;
            if(!join_path(state,state.request.working,L"build",5,build) ||
               build.length>path_units-out.length) return false;
            for(U32 j=0;j<build.length;++j) out.cells[out.length++]=build.cells[j];
            i+=7;continue;
        }
        if(replacement!=nullptr) {
            if(replacement_length>path_units-out.length) return false;
            for(U32 j=0;j<replacement_length;++j) out.cells[out.length++]=replacement[j];
            i+=token;
        } else {
            if(out.length==path_units) return false;
            out.cells[out.length++]=input[i++];
        }
    }
    out.cells[out.length]=0;
    return valid_wide(out.cells,out.length,true,false);
}
}

bool validate_profile(State& state) noexcept {
    if(!Data.source_bound || !Data.native_bound || !Data.payload_bound || !Data.controller_profile_bound) return false;
    if(Data.bootstrap_prefix_limit==0 || Data.attribute_bytes==0) return false;
    for(U32 i=0;i<static_cast<U32>(Family::Count);++i) if(Data.prices[i]==0) return false;
    for(U32 i=0;i<3;++i) {
        if(Data.helper_reservations[i]<Data.tail_reservations[i] || Data.tail_reservations[i]==0 ||
           Data.helper_reservations[i]-Data.tail_reservations[i]<Data.bootstrap_prefix_limit) return false;
    }
    if(Data.runner_source_upper==0 || Data.producer_source_upper==0 || Data.verifier_source_upper==0 ||
       Data.runner_source_upper>static_cast<U64>(INT64_MAX) ||
       Data.producer_source_upper>static_cast<U64>(INT64_MAX)-Data.runner_source_upper ||
       Data.verifier_source_upper>static_cast<U64>(INT64_MAX)-Data.runner_source_upper-Data.producer_source_upper ||
       Data.whole_source_upper!=Data.runner_source_upper+Data.producer_source_upper+Data.verifier_source_upper) return false;
    state.meter.limit=Data.bootstrap_prefix_limit;
    if(!charge(state.meter,Family::Entry) || !charge(state.meter,Family::Profile)) return false;
    Wide check;
    if(!wide_from_binding(state,Data.source_base,Data.source_base_length,check) ||
       !wide_from_binding(state,Data.repo,Data.repo_length,check) ||
       !wide_from_binding(state,Data.helper_path,Data.helper_path_length,check) ||
       !wide_from_binding(state,Data.compiler_path,Data.compiler_path_length,check)) return false;
    for(U32 i=0;i<28;++i) {
        if(Data.compiler_arguments[i]==nullptr || Data.compiler_argument_lengths[i]>path_units ||
           !valid_wide(Data.compiler_arguments[i],Data.compiler_argument_lengths[i],true,false)) return false;
    }
    for(U32 i=0;i<8;++i) {
        if(Data.environment_values[i]==nullptr || Data.environment_value_lengths[i]>path_units ||
           !valid_wide(Data.environment_values[i],Data.environment_value_lengths[i],true,false)) return false;
    }
    return true;
}
bool copy_text(State& state,Text value,Wide& out,bool ascii) noexcept {
    if(!charge(state.meter,Family::StringGuard) || !charge(state.meter,Family::StringCell,value.length)) return false;
    if(value.length>path_units) return false;
    for(U32 i=0;i<value.length;++i) out.cells[i]=static_cast<wchar_t>(cell(value,i));
    out.length=value.length;out.cells[out.length]=0;
    return valid_wide(out.cells,out.length,ascii,!ascii);
}
bool join_path(State& state,const Wide& base,const wchar_t* suffix,U32 length,Wide& out) noexcept {
    if(!charge(state.meter,Family::PathGuard) || !charge(state.meter,Family::PathCell,base.length+length)) return false;
    const bool separator=base.length!=0 && base.cells[base.length-1]!='\\' && base.cells[base.length-1]!='/';
    const U32 extra=separator?1u:0u;
    if(base.length>path_units || length>path_units-base.length || extra>path_units-base.length-length) return false;
    U32 n=0;
    for(U32 i=0;i<base.length;++i) out.cells[n++]=base.cells[i];
    if(separator) out.cells[n++]='\\';
    for(U32 i=0;i<length;++i) out.cells[n++]=suffix[i];
    out.cells[n]=0;out.length=n;
    return true;
}
bool parse_command(State& state) noexcept {
    if(!charge(state.meter,Family::RawCommand) || !charge(state.meter,Family::CommandCell,raw_command_units)) return false;
    const wchar_t* raw=GetCommandLineW();
    if(raw==nullptr || raw[0]!='"') return false;
    U32 at=1,n=0;
    while(at<raw_command_units && raw[at]!=0 && raw[at]!='"') {
        if(n==path_units) return false;
        state.request.helper_path.cells[n++]=raw[at++];
    }
    if(at>=raw_command_units || raw[at]!='"' || n==0 || at+2u>=raw_command_units || raw[at+1u]!=' ') return false;
    state.request.helper_path.cells[n]=0;state.request.helper_path.length=n;at+=2;
    static constexpr wchar_t prefix[]=L"--request16=";
    for(U32 i=0;i<12;++i) if(at>=raw_command_units || raw[at++]!=prefix[i]) return false;
    n=0;
    while(at<raw_command_units && raw[at]!=0) {
        if(n==path_units || at+4u>=raw_command_units) return false;
        U32 value=0;
        for(U32 i=0;i<4;++i) {
            const wchar_t ch=raw[at++];U32 digit=0;
            if(ch>='0' && ch<='9') digit=static_cast<U32>(ch-'0');
            else if(ch>='A' && ch<='F') digit=static_cast<U32>(ch-'A')+10u;
            else return false;
            value=(value<<4)|digit;
        }
        if(value==0) return false;
        state.request.request_path.cells[n++]=static_cast<wchar_t>(value);
    }
    if(at>=raw_command_units || raw[at]!=0 || n==0) return false;
    state.request.request_path.cells[n]=0;state.request.request_path.length=n;
    if(!valid_wide(state.request.helper_path.cells,state.request.helper_path.length,false,true) ||
       !valid_wide(state.request.request_path.cells,n,false,true) ||
       !equal_wide(state.request.helper_path.cells,state.request.helper_path.length,Data.helper_path,Data.helper_path_length,true)) return false;
    wchar_t actual[path_units+1];
    const DWORD used=GetModuleFileNameW(nullptr,actual,path_units+1);
    if(used==0 || used>path_units || !equal_wide(actual,used,Data.helper_path,Data.helper_path_length,true)) return false;
    return validate_path(state,state.request.request_path,false);
}
bool decode_request(State& state) noexcept {
    if(!charge(state.meter,Family::Decode) || !charge(state.meter,Family::DecodeCell,state.request.captured)) return false;
    Request& r=state.request;
    static constexpr U8 magic[8]={'Y','C','S','R','E','Q','0','1'};
    if(r.captured<request_header_bytes) return false;
    for(U32 i=0;i<8;++i) if(r.capture[i]!=magic[i]) return false;
    if(get32(r.capture+8)!=1 || get32(r.capture+12)!=r.captured) return false;
    r.child_index=get32(r.capture+16);r.argument_count=get32(r.capture+20);
    if(r.child_index>2 || get32(r.capture+24)!=8 ||
       r.argument_count!=(r.child_index==0?28u:6u)) return false;
    const U32 maximum=r.child_index==0?request_max_bytes:18644u;
    if(r.captured>maximum) return false;
    r.reservation=get64(r.capture+28);r.tail_reservation=get64(r.capture+36);
    if(r.reservation!=Data.helper_reservations[r.child_index] ||
       r.tail_reservation!=Data.tail_reservations[r.child_index] ||
       r.tail_reservation==0 || r.reservation<r.tail_reservation) return false;
    const U64 limit=r.reservation-r.tail_reservation;
    AcquireSRWLockExclusive(&state.meter.lock);
    const bool meter_ok=state.meter.valid && state.meter.used<=limit;
    if(meter_ok) state.meter.limit=limit;
    ReleaseSRWLockExclusive(&state.meter.lock);
    if(!meter_ok) return false;
    for(U32 i=0;i<32;++i) {
        r.manifest_sha.bytes[i]=r.capture[44u+i];
        r.run_sha.bytes[i]=r.capture[76u+i];
        r.target_sha.bytes[i]=r.capture[108u+i];
    }
    U32 at=request_header_bytes;
    for(U32 i=0;i<12u+r.argument_count;++i) {
        if(at>r.captured || r.captured-at<4u) return false;
        const U32 length=get32(r.capture+at);at+=4;
        if(length>path_units || length>(r.captured-at)/2u) return false;
        r.strings[i]=Text{r.capture+at,length};at+=2u*length;
    }
    if(at!=r.captured || !copy_text(state,r.strings[0],r.target,false) ||
       !copy_text(state,r.strings[1],r.working,false) ||
       !copy_text(state,r.strings[2],r.stage,false) ||
       !copy_text(state,r.strings[3],r.transport,false)) return false;
    Wide check;
    for(U32 i=4;i<12u+r.argument_count;++i) if(!copy_text(state,r.strings[i],check,true)) return false;
    state.terminal.child_index=r.child_index;
    state.terminal.manifest_sha=r.manifest_sha;
    state.terminal.run_sha=r.run_sha;
    state.terminal.target_sha=r.target_sha;
    return true;
}
bool validate_request_recipe(State& state) noexcept {
    if(!charge(state.meter,Family::Identity)) return false;
    Request& r=state.request;Wide expected;Wide base;
    if(!wide_from_binding(state,Data.source_base,Data.source_base_length,base) ||
       !join_path(state,base,L"runs",4,expected)) return false;
    const U32 prefix_length=expected.length;
    if(r.working.length<=prefix_length || r.working.cells[prefix_length]!='\\' ||
       !equal_wide(r.working.cells,prefix_length,expected.cells,prefix_length,true)) return false;
    for(U32 i=prefix_length+1u;i<r.working.length;++i) if(r.working.cells[i]=='\\' || r.working.cells[i]=='/') return false;
    if(!join_path(state,r.working,L"stage",5,expected) || !text_matches(r.strings[2],expected) ||
       !join_path(state,r.working,L"transport",9,expected) || !text_matches(r.strings[3],expected)) return false;
    wchar_t request_name[]=L"child-0.request.bin";
    wchar_t terminal_name[]=L"child-0.terminal.bin";
    request_name[6]=static_cast<wchar_t>('0'+r.child_index);
    terminal_name[6]=static_cast<wchar_t>('0'+r.child_index);
    if(!join_path(state,r.transport,request_name,19,expected) ||
       !equal_wide(expected.cells,expected.length,r.request_path.cells,r.request_path.length,true) ||
       !join_path(state,r.transport,terminal_name,20,r.terminal_path)) return false;
    Digest actual;U64 bytes=0;
    if(!hash_run_identity(state,actual) || !equal_digest(actual,r.run_sha)) return false;
    if(!join_path(state,base,L"source-manifest.json",20,expected) ||
       !hash_file(state,expected,1048576,actual,bytes) || !equal_digest(actual,r.manifest_sha)) return false;
    if(!validate_path(state,r.working,true) || !validate_path(state,r.stage,true) || !validate_path(state,r.transport,true) ||
       !validate_path(state,r.target,false)) return false;
    if(r.child_index==0) {
        if(!equal_wide(r.target.cells,r.target.length,Data.compiler_path,Data.compiler_path_length,true) ||
           !equal_digest(r.target_sha,Data.compiler_sha)) return false;
        for(U32 i=0;i<28;++i) {
            if(!append_template(state,Data.compiler_arguments[i],Data.compiler_argument_lengths[i],expected) ||
               !text_matches(r.strings[12u+i],expected)) return false;
        }
    } else {
        if(!join_path(state,r.working,L"build\\clock_stage.exe",21,expected) ||
           !equal_wide(expected.cells,expected.length,r.target.cells,r.target.length,true)) return false;
        if(!literal(r.strings[12],r.child_index==1?L"produce":L"verify",r.child_index==1?7u:6u) ||
           !join_path(state,r.working,L"data",4,expected) || !text_matches(r.strings[13],expected)) return false;
        static constexpr wchar_t alphabet[]=L"0123456789ABCDEF";
        if(r.strings[14].length!=64) return false;
        for(U32 i=0;i<32;++i) if(cell(r.strings[14],2*i)!=alphabet[r.manifest_sha.bytes[i]>>4] ||
            cell(r.strings[14],2*i+1)!=alphabet[r.manifest_sha.bytes[i]&15u]) return false;
        for(U32 i=15;i<18;++i) {
            const Text number=r.strings[i];
            if(number.length==0 || number.length>19) return false;
            U64 value=0;
            for(U32 j=0;j<number.length;++j) {
                const U32 ch=cell(number,j);
                if(ch<'0' || ch>'9' || value>(static_cast<U64>(INT64_MAX)-(ch-'0'))/10u) return false;
                value=value*10u+(ch-'0');
            }
            if(number.length>1 && cell(number,0)=='0') return false;
            const U64 expected_value=i==15u ? (r.child_index==1u?Data.runner_source_upper:Data.runner_source_upper+Data.producer_source_upper) :
                (i==16u?Data.runner_source_upper:Data.whole_source_upper);
            if(value!=expected_value) return false;
        }
    }
    if(!hash_file(state,r.target,33554432,actual,bytes) || !equal_digest(actual,r.target_sha) ||
       (r.child_index==0 && bytes!=Data.compiler_bytes)) return false;
    for(U32 i=0;i<8;++i) {
        if(Data.environment_value_lengths[i]==19 &&
           equal_wide(Data.environment_values[i],19,L"<OWNED_OPAQUE_TEMP>",19,false)) {
            if(!join_path(state,r.working,L"opaque-temp",11,expected) || !text_matches(r.strings[4u+i],expected)) return false;
        } else {
            if(r.strings[4u+i].length!=Data.environment_value_lengths[i]) return false;
            for(U32 j=0;j<r.strings[4u+i].length;++j) if(cell(r.strings[4u+i],j)!=Data.environment_values[i][j]) return false;
        }
    }
    r.current=true;
    return true;
}
bool build_environment(State& state) noexcept {
    if(!charge(state.meter,Family::Environment)) return false;
    static constexpr const wchar_t* names[8]={L"INCLUDE",L"LIB",L"PATH",L"SystemRoot",L"TEMP",L"TMP",L"VSLANG",L"windir"};
    static constexpr U32 lengths[8]={7,3,4,10,4,3,6,6};
    static constexpr U32 indices[8]={3,4,2,0,5,6,7,1};
    U32 used=0;
    for(U32 i=0;i<8;++i) {
        const Text value=state.request.strings[4u+indices[i]];
        if(!charge(state.meter,Family::EnvironmentCell,lengths[i]+value.length+2u)) return false;
        for(U32 j=0;j<lengths[i];++j) if(!append(state.environment,environment_units,used,names[i][j])) return false;
        if(!append(state.environment,environment_units,used,'=')) return false;
        for(U32 j=0;j<value.length;++j) if(!append(state.environment,environment_units,used,static_cast<wchar_t>(cell(value,j)))) return false;
        if(used>=environment_units-1u) return false;
        state.environment[used++]=0;
    }
    if(used>=environment_units) return false;
    state.environment[used++]=0;state.environment_length=used;
    return true;
}
bool build_target_command(State& state) noexcept {
    if(!charge(state.meter,Family::TargetQuote)) return false;
    U32 n=0;
    if(!append(state.target_command,target_command_units,n,'"')) return false;
    for(U32 i=0;i<state.request.target.length;++i) {
        const wchar_t ch=state.request.target.cells[i];if(ch=='"' || ch==0) return false;
        if(!append(state.target_command,target_command_units,n,ch)) return false;
    }
    if(!append(state.target_command,target_command_units,n,'"')) return false;
    for(U32 a=0;a<state.request.argument_count;++a) {
        const Text value=state.request.strings[12u+a];bool quotes=value.length==0;
        if(!charge(state.meter,Family::QuoteCell,value.length)) return false;
        for(U32 i=0;i<value.length;++i) if(cell(value,i)==' ' || cell(value,i)=='"') quotes=true;
        if(!append(state.target_command,target_command_units,n,' ')) return false;
        if(!quotes) {
            for(U32 i=0;i<value.length;++i) if(!append(state.target_command,target_command_units,n,static_cast<wchar_t>(cell(value,i)))) return false;
            continue;
        }
        if(!append(state.target_command,target_command_units,n,'"')) return false;
        for(U32 i=0;i<value.length;) {
            U32 slashes=0;while(i<value.length && cell(value,i)=='\\') {++slashes;++i;}
            if(i==value.length) {
                for(U32 j=0;j<2u*slashes;++j) if(!append(state.target_command,target_command_units,n,'\\')) return false;
                break;
            }
            const U32 ch=cell(value,i++);const U32 copies=ch=='"'?2u*slashes+1u:slashes;
            for(U32 j=0;j<copies;++j) if(!append(state.target_command,target_command_units,n,'\\')) return false;
            if(!append(state.target_command,target_command_units,n,static_cast<wchar_t>(ch))) return false;
        }
        if(!append(state.target_command,target_command_units,n,'"')) return false;
    }
    state.target_command_length=n;
    return true;
}
bool validate_terminal(State& state) noexcept {
    if(!charge(state.meter,Family::TerminalValidation)) return false;
    const Terminal& t=state.terminal;const U32 f=t.flags;
    if(t.child_index>2 || (f&~allowed_flags)!=0 || (t.cleanup_mask&~allowed_cleanup)!=0 ||
       ((t.cleanup_mask&2u)!=0 && (t.cleanup_mask&4u)!=0) || t.status>4) return false;
    if(!pair(t.primary.site,t.primary.reason) || !pair(t.first_secondary.site,t.first_secondary.reason) ||
       !pair(t.first_native_secondary.site,t.first_native_secondary.reason)) return false;
    if(((f&PrimaryNativeKnown)!=0)!=t.primary.native_known ||
       ((f&FirstSecondaryOwnNativeKnown)!=0)!=t.first_secondary.native_known ||
       ((f&FirstNativeSecondaryKnown)!=0)!=t.first_native_secondary.native_known) return false;
    if((t.primary.site==0 && (t.primary.reason!=0 || t.primary.native_known)) ||
       (!t.primary.native_known && t.primary.native_error!=0)) return false;
    const bool secondary=(f&HasSecondary)!=0;
    if(secondary!=(t.secondary_count!=0) || (secondary!=(t.first_secondary.site!=0))) return false;
    if((f&SecondaryCountSaturated)!=0 && (!secondary || t.secondary_count!=UINT64_MAX)) return false;
    if(!t.first_secondary.native_known && t.first_secondary.native_error!=0) return false;
    if(t.first_secondary.native_known && (!t.first_native_secondary.native_known || !same_failure(t.first_secondary,t.first_native_secondary))) return false;
    if(!t.first_native_secondary.native_known && (t.first_native_secondary.site!=0 || t.first_native_secondary.reason!=0 || t.first_native_secondary.native_error!=0)) return false;
    if((f&ChildCreated)==0 && (t.target_pid!=0 || (f&(TargetExitKnown|TargetMayHaveRun))!=0)) return false;
    if((f&ChildCreated)!=0 && t.target_pid==0) return false;
    if((f&TargetExitKnown)==0 && t.target_raw_exit!=0) return false;
    if((f&PrefixKnown)!=0) {
        if(t.prefix_used>state.request.reservation-state.request.tail_reservation) return false;
    } else if(t.prefix_used!=0) return false;
    U64 observed=0,partials=0,written=0,omitted=0;
    for(U32 i=0;i<2;++i) {
        const PipeCount& p=t.pipes[i];const U32 start=i==0?StdoutStartKnown:StderrStartKnown;
        const U32 end=i==0?StdoutEndKnown:StderrEndKnown;
        const U32 settled=i==0?StdoutSettled:StderrSettled;
        const U32 partial=i==0?StdoutErrorPartialKnown:StderrErrorPartialKnown;
        const U32 eof=i==0?StdoutEof:StderrEof;
        if(((f&start)==0 && p.start!=0) || ((f&end)==0 && p.end!=0) ||
           ((f&start)!=0 && (f&end)!=0 && p.end<p.start)) return false;
        if((f&partial)==0) {if(p.error_partial!=0) return false;}
        else if(p.error_partial>=pipe_buffer_bytes) return false;
        if((f&settled)!=0 && (f&partial)==0) return false;
        if((f&eof)!=0 && (f&partial)!=0 && p.error_partial!=0) return false;
        if((f&CoordinatorCountersKnown)!=0) {
            U64 sum=0;if(!checked_add(p.written,p.unretained,sum) || sum!=p.observed || p.omitted>p.unretained ||
                !checked_add(observed,p.observed,observed) || !checked_add(written,p.written,written) ||
                !checked_add(omitted,p.omitted,omitted)) return false;
        } else if(p.observed!=0 || p.written!=0 || p.omitted!=0 || p.unretained!=0) return false;
        if((f&partial)!=0 && !checked_add(partials,p.error_partial,partials)) return false;
    }
    if((f&CoordinatorCountersKnown)!=0) {
        U64 total=0;if(t.body_polls>body_poll_limit || !checked_add(observed,partials,total) || total>raw_log_limit+2u*pipe_buffer_bytes || written>raw_log_limit) return false;
    } else if(t.body_polls!=0) return false;
    if((f&BodyTimedOut)!=0 && ((f&BodyPollLimited)!=0 || !reason_present(t,25))) return false;
    if((f&BodyPollLimited)!=0 && !reason_present(t,26)) return false;
    if((f&RawLogCap)!=0 && ((f&StagePoolCap)!=0 || !reason_present(t,16))) return false;
    if((f&StagePoolCap)!=0 && !reason_present(t,17)) return false;
    if(omitted!=0 && (f&(RawLogCap|StagePoolCap))==0) return false;
    if((f&StageGuardRejected)!=0) {
        bool found=false;for(U32 r=18;r<=22;++r) if(reason_present(t,r)) found=true;
        if(!found) return false;
    }
    const U32 settled_mask=StdoutSettled|StderrSettled|JobEmptyProved|PrefixKnown|CoordinatorCountersKnown;
    if(t.status<4 && (f&settled_mask)!=settled_mask) return false;
    if(t.status<=1) {
        const U32 required=settled_mask|ChildCreated|TargetMayHaveRun|TargetExitKnown|StdoutEof|StderrEof|
            StdoutErrorPartialKnown|StderrErrorPartialKnown|StdoutStartKnown|StdoutEndKnown|StderrStartKnown|StderrEndKnown|StdoutFlushed|StderrFlushed;
        const U32 forbidden=PrimaryNativeKnown|HasSecondary|FirstSecondaryOwnNativeKnown|FirstNativeSecondaryKnown|
            BodyTimedOut|BodyPollLimited|RawLogCap|StagePoolCap|StageGuardRejected|SecondaryCountSaturated;
        if((f&required)!=required || (f&forbidden)!=0 || t.primary.site!=0 || t.cleanup_mask!=0 ||
           ((t.status==0)!=(t.target_raw_exit==0))) return false;
        for(U32 i=0;i<2;++i) {
            const PipeCount& p=t.pipes[i];
            if(p.end-p.start!=p.written || p.written!=p.observed || p.unretained!=0 || p.omitted!=0 || p.error_partial!=0) return false;
        }
    } else if(t.status==2) {
        if((f&TargetMayHaveRun)!=0 || t.primary.site==0) return false;
    } else if(t.status==3) {
        if((f&(TargetMayHaveRun|TargetExitKnown))!=(TargetMayHaveRun|TargetExitKnown) ||
           (t.primary.site==0 && !secondary && t.cleanup_mask==0)) return false;
        for(U32 i=0;i<2;++i) {
            const U32 eof=i==0?StdoutEof:StderrEof;
            const U32 positions=i==0?(StdoutStartKnown|StdoutEndKnown):(StderrStartKnown|StderrEndKnown);
            const U32 flushed=i==0?StdoutFlushed:StderrFlushed;
            if((f&eof)==0 && t.primary.site==0 && (t.cleanup_mask&(1u<<(4u+i)))==0) return false;
            if((f&positions)!=positions && (t.cleanup_mask&(1u<<(8u+i)))==0) return false;
            if((f&flushed)==0 && (t.cleanup_mask&(1u<<(6u+i)))==0) return false;
        }
    } else if(t.primary.site==0 && !secondary && (t.cleanup_mask&(1u<<14))==0) return false;
    return equal_digest(t.manifest_sha,state.request.manifest_sha) && equal_digest(t.run_sha,state.request.run_sha) && equal_digest(t.target_sha,state.request.target_sha);
}
bool encode_terminal(State& state) noexcept {
    if(!charge(state.meter,Family::Encode) || !charge(state.meter,Family::EncodeCell,terminal_bytes) || !validate_terminal(state)) return false;
    U8* bytes=state.terminal_capture;const Terminal& t=state.terminal;
    static constexpr U8 magic[8]={'Y','C','S','R','E','S','0','1'};
    for(U32 i=0;i<8;++i) bytes[i]=magic[i];
    const U32 fields[16]={1,terminal_bytes,t.child_index,t.flags,t.target_pid,t.target_raw_exit,t.status,
        t.primary.site,t.primary.reason,t.primary.native_error,t.first_secondary.site,t.first_secondary.reason,
        t.first_secondary.native_error,t.first_native_secondary.site,t.first_native_secondary.reason,t.first_native_secondary.native_error};
    for(U32 i=0;i<16;++i) put32(bytes+8u+4u*i,fields[i]);
    put64(bytes+72,t.secondary_count);put64(bytes+80,t.prefix_used);put64(bytes+88,t.body_polls);
    for(U32 i=0;i<32;++i) {bytes[96u+i]=t.manifest_sha.bytes[i];bytes[128u+i]=t.run_sha.bytes[i];bytes[160u+i]=t.target_sha.bytes[i];}
    for(U32 pipe=0;pipe<2;++pipe) {
        const PipeCount& p=t.pipes[pipe];const U64 values[7]={p.observed,p.written,p.omitted,p.unretained,p.start,p.end,p.error_partial};
        for(U32 i=0;i<7;++i) put64(bytes+192u+56u*pipe+8u*i,values[i]);
    }
    put32(bytes+304,t.cleanup_mask);
    return true;
}
}
