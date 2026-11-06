#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#include <windows.h>
#include <cstddef>
#include <cstdint>

// Source-only candidate. No definition of Data, accepted prices or true gate.
namespace yesdaw_clock_supervisor_r3 {
using U8 = std::uint8_t;
using U32 = std::uint32_t;
using U64 = std::uint64_t;
static_assert(sizeof(wchar_t)==2 && sizeof(U8)==1 && sizeof(U32)==4 && sizeof(U64)==8);
inline constexpr U32 request_header_bytes = 140;
inline constexpr U32 request_max_bytes = 41260;
inline constexpr U32 terminal_bytes = 308;
inline constexpr U32 path_units = 512;
inline constexpr U32 raw_command_units = 2576;
inline constexpr U32 target_command_units = 29271;
inline constexpr U32 environment_units = 4156;
inline constexpr U32 pipe_buffer_bytes = 4096;
inline constexpr U64 raw_log_limit = 1048576;
inline constexpr U64 stage_limit = 67108864;
inline constexpr U32 body_poll_limit = 7200;
inline constexpr DWORD poll_milliseconds = 250;
inline constexpr DWORD cleanup_milliseconds = 2000;
inline constexpr U32 allowed_flags = 0x0fffffff;
inline constexpr U32 allowed_cleanup = 0x00007fff;
inline constexpr bool source_review_accepted = false;
inline constexpr bool source_work_accepted = false;
inline constexpr bool native_profile_accepted = false;
inline constexpr bool controlled_resource_accepted = false;
inline constexpr bool execution_accepted = false;

enum class Family : U32 {
    Entry, Profile, RawCommand, CommandCell, Decode, DecodeCell,
    StringGuard, StringCell, Identity, HashFile, HashChunk, HashByte,
    HashBlock, HashRound,
    PathGuard, PathCell, StageScan, StageEntry, Encode, EncodeCell,
    TargetQuote, QuoteCell, Environment, EnvironmentCell, Owner, CaptureSetup, TargetResume,
    WorkerStart, WorkerRead, WorkerPublish,
    Coordinator, Poll, Consume, WriteChunk, WriteCall,
    TerminalValidation, Count
};
struct Digest { U8 bytes[32]; };
struct Text { const U8* bytes = nullptr; U32 length = 0; };
struct Wide { wchar_t cells[path_units + 1]; U32 length = 0; };
struct Failure {
    U32 site = 0;
    U32 reason = 0;
    U32 native_error = 0;
    bool native_known = false;
};
enum class Site : U32 {
    None, Request, Containment, Spawn, Read, StageAdmission, Write,
    Deadline, TargetExit, WorkerSettlement, JobSettlement,
    TerminalPublication, Meter, OwnedClose
};
enum class Reason : U32 {
    None, RequestMagic, RequestVersion, RequestLengthOrEof,
    RequestCountBinding, StringGuard, PathGrammar, RunIdentity,
    CurrentIdentity, ReservationBinding, RequestIo, UnsupportedJobProfile,
    JobSetup, CreateProcessFailure, ResumeFailure, ReadFailure, RawLogCap,
    StagePoolCap, StageUnknownName, StageReparse, StageDepth, StageExtent,
    StageIo, WriteFailure, ZeroWriteProgress, BodyTimeout, PollLimit,
    TargetExitUnobserved, TargetExitQueryFailure, WorkerJoinUnobserved,
    WorkerJoinFailure, JobNotEmpty, JobQueryFailure, FlushFailure,
    PositionFailure, CloseFailure, PublicationFailure, SourcePrefixLimit,
    MeterOverflow, CounterOverflow, CancelledReadDiagnostic,
    SourceInvariant, ThreadSetupFailure, StdinNulSetupFailure,
    NativeOwnerAllocationFailure, OwnedPathFailure, HandleInvariant,
    LateTailSourceInvariant, JobTerminationFailure
};
enum Flag : U32 {
    ChildCreated = 1u << 0, TargetExitKnown = 1u << 1,
    StdoutSettled = 1u << 2, StderrSettled = 1u << 3,
    StdoutEof = 1u << 4, StderrEof = 1u << 5,
    JobEmptyProved = 1u << 6, StdoutStartKnown = 1u << 7,
    StdoutEndKnown = 1u << 8, StderrStartKnown = 1u << 9,
    StderrEndKnown = 1u << 10, PrefixKnown = 1u << 11,
    PrimaryNativeKnown = 1u << 12, HasSecondary = 1u << 13,
    FirstSecondaryOwnNativeKnown = 1u << 14,
    FirstNativeSecondaryKnown = 1u << 15,
    StdoutErrorPartialKnown = 1u << 16,
    StderrErrorPartialKnown = 1u << 17, BodyTimedOut = 1u << 18,
    BodyPollLimited = 1u << 19, RawLogCap = 1u << 20,
    StagePoolCap = 1u << 21, StageGuardRejected = 1u << 22,
    SecondaryCountSaturated = 1u << 23,
    StdoutFlushed = 1u << 24, StderrFlushed = 1u << 25,
    TargetMayHaveRun = 1u << 26, CoordinatorCountersKnown = 1u << 27
};

// Future immutable binding contains real independently accepted source prices.
// It is intentionally only declared. No zero-price or permissive fallback exists.
struct Catalogue {
    bool source_bound;
    bool native_bound;
    bool payload_bound;
    bool controller_profile_bound;
    U64 prices[static_cast<U32>(Family::Count)];
    U64 bootstrap_prefix_limit;
    U64 helper_reservations[3];
    U64 tail_reservations[3];
    U64 runner_source_upper;
    U64 producer_source_upper;
    U64 verifier_source_upper;
    U64 whole_source_upper;
    SIZE_T attribute_bytes;
    const wchar_t* source_base;
    U32 source_base_length;
    const wchar_t* repo;
    U32 repo_length;
    const wchar_t* helper_path;
    U32 helper_path_length;
    const wchar_t* compiler_path;
    U32 compiler_path_length;
    Digest compiler_sha;
    U64 compiler_bytes;
    const wchar_t* compiler_arguments[28];
    U32 compiler_argument_lengths[28];
    const wchar_t* environment_values[8];
    U32 environment_value_lengths[8];
};
extern const Catalogue Data;

struct Meter {
    SRWLOCK lock = SRWLOCK_INIT;
    U64 used = 0;
    U64 limit = 0;
    U64 frozen_prefix = 0;
    bool tail = false;
    bool valid = true;
    bool refused = false;
    bool reported = false;
};
struct Sha256 {
    U32 h[8];
    U8 pending[64];
    U32 pending_count = 0;
    U64 bytes = 0;
};
struct Request {
    U8 capture[request_max_bytes];
    U32 captured = 0;
    U32 child_index = 0;
    U32 argument_count = 0;
    U64 reservation = 0;
    U64 tail_reservation = 0;
    Digest manifest_sha;
    Digest run_sha;
    Digest target_sha;
    Text strings[40];
    Wide target;
    Wide working;
    Wide stage;
    Wide transport;
    Wide request_path;
    Wide terminal_path;
    Wide helper_path;
    bool current = false;
};
struct PipeCount {
    U64 observed = 0;
    U64 written = 0;
    U64 omitted = 0;
    U64 unretained = 0;
    U64 start = 0;
    U64 end = 0;
    U64 error_partial = 0;
};
struct Terminal {
    U32 child_index = 0;
    U32 flags = CoordinatorCountersKnown;
    U32 target_pid = 0;
    U32 target_raw_exit = 0;
    U32 status = 4;
    Failure primary;
    Failure first_secondary;
    Failure first_native_secondary;
    U64 secondary_count = 0;
    U64 prefix_used = 0;
    U64 body_polls = 0;
    Digest manifest_sha;
    Digest run_sha;
    Digest target_sha;
    PipeCount pipes[2];
    U32 cleanup_mask = 0;
};
enum class ReadyKind : U32 { None, Full, EofShort, ZeroEof, Error, NoProgress };
enum class AckKind : LONG { Continue, Stop };
enum class PathResult : U32 { Ok, SourceRefused, ExtentRejected };
enum class WorkerExitKind : U32 { None, LogicalEof, Stopped, Fault };
struct WorkerExit {
    Failure failures[2];
    U32 failure_count = 0;
    U32 partial = 0;
    bool partial_known = false;
    bool logical_incomplete = false;
    bool overflow = false;
    WorkerExitKind kind = WorkerExitKind::None;
};
struct State;
struct Worker {
    State* state = nullptr;
    HANDLE pipe = nullptr;
    HANDLE thread = nullptr;
    HANDLE ready = nullptr;
    HANDLE ack = nullptr;
    HANDLE started = nullptr;
    U8 buffer[pipe_buffer_bytes];
    U32 count = 0;
    U32 partial = 0;
    DWORD error = 0;
    Reason error_reason = Reason::None;
    bool error_native_known = false;
    ReadyKind kind = ReadyKind::None;
    volatile LONG stop = 0;
    volatile LONG ack_kind = static_cast<LONG>(AckKind::Continue);
    U64 generation = 0;
    U64 consumed_generation = 0;
    bool generation_valid = true;
    bool created = false;
    bool ready_consumed = true;
    bool settled = false;
    bool eof_short_pending = false;
    U32 active_fill = 0;
    bool operation_open = false;
    WorkerExit exit;
};
struct State {
    Meter meter;
    Request request;
    Terminal terminal;
    Worker workers[2];
    HANDLE request_file = nullptr;
    HANDLE hash_file_owner = nullptr;
    HANDLE search_owner = INVALID_HANDLE_VALUE;
    HANDLE terminal_file = nullptr;
    HANDLE logs[2] = {nullptr, nullptr};
    HANDLE child_writes[2] = {nullptr, nullptr};
    HANDLE nul_input = nullptr;
    HANDLE job = nullptr;
    HANDLE process = nullptr;
    HANDLE primary_thread = nullptr;
    LPPROC_THREAD_ATTRIBUTE_LIST attributes = nullptr;
    void* attribute_allocation = nullptr;
    SIZE_T attribute_bytes = 0;
    HANDLE inherited[3] = {nullptr, nullptr, nullptr};
    HANDLE job_attribute[1] = {nullptr};
    wchar_t target_command[target_command_units];
    U32 target_command_length = 0;
    wchar_t environment[environment_units];
    U32 environment_length = 0;
    U8 hash_buffer[65536];
    U8 terminal_capture[terminal_bytes];
    U64 body_started = 0;
    bool body_clock_started = false;
    bool stop_body = false;
    bool record_frozen = false;
    bool tail_failed = false;
    bool target_exit_group_failed = false;
    bool job_ever_created = false;
    // Failed creator outputs have no established-handle provenance. These raw
    // representations are retained privately and NEVER passed to a native API.
    U8 failed_pipe_output[2][2 * sizeof(HANDLE)];
    bool failed_pipe_captured[2] = {false, false};
    U8 failed_process_output[sizeof(PROCESS_INFORMATION)];
    bool failed_process_captured = false;
    bool unknown_native_creation = false;
};

bool charge(Meter&, Family, U64 = 1) noexcept;
bool admit_body(Meter&, Family) noexcept;
bool admit_body(State&, Family) noexcept;
bool spend(State&, Family, U64 = 1) noexcept;
bool admit_read(Meter&) noexcept;
bool admit_write(Meter&) noexcept;
bool admit_write(State&) noexcept;
void request_tail(Meter&) noexcept;
bool tail_requested(Meter&) noexcept;
void begin_tail(State&) noexcept;
void fail(State&, Site, Reason, bool = false, DWORD = 0) noexcept;
Failure failure(Site, Reason, bool = false, DWORD = 0) noexcept;
bool checked_add(U64, U64, U64&) noexcept;
bool equal_digest(const Digest&, const Digest&) noexcept;
bool equal_wide(const wchar_t*, U32, const wchar_t*, U32, bool) noexcept;
bool validate_profile(State&) noexcept;
bool parse_command(State&) noexcept;
bool read_request(State&) noexcept;
bool decode_request(State&) noexcept;
bool validate_request_recipe(State&) noexcept;
bool encode_terminal(State&) noexcept;
bool validate_terminal(State&) noexcept;
bool build_target_command(State&) noexcept;
bool build_environment(State&) noexcept;
bool copy_text(State&, Text, Wide&, bool) noexcept;
PathResult join_path(State&, const Wide&, const wchar_t*, U32, Wide&) noexcept;
bool validate_path(State&, const Wide&, bool, Site = Site::Request) noexcept;
bool hash_file(State&, const Wide&, U64, Digest&, U64&) noexcept;
bool hash_run_identity(State&, Digest&) noexcept;
bool sha_init(State&, Sha256&) noexcept;
bool sha_update(State&, Sha256&, const U8*, U32) noexcept;
bool sha_finish(State&, Sha256&, Digest&) noexcept;
bool stage_scan(State&, U64, U64&) noexcept;
bool observe_position(State&, U32, bool) noexcept;
bool open_capture_owners(State&) noexcept;
bool create_contained_target(State&) noexcept;
bool start_workers(State&) noexcept;
DWORD WINAPI worker_entry(void*) noexcept;
bool consume_ready(State&, U32, bool) noexcept;
bool write_chunk(State&, U32, const U8*, U32) noexcept;
bool observe_target_exit(State&, bool) noexcept;
void coordinator(State&) noexcept;
void cleanup_prefix(State&) noexcept;
void finalize_record(State&) noexcept;
bool publish_terminal(State&) noexcept;
void close_safe_owners(State&, bool) noexcept;
[[noreturn]] void finish(State&, DWORD) noexcept;
int supervisor_main() noexcept;
}
