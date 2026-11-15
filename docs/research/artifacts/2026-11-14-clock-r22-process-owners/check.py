"""Check source-only bounds on R22 process references and supplied launch payloads."""

import hashlib
import json
from pathlib import Path
import re


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
REVISION = ROOT / (
    "build-ci/musical-audio-persistent-planner-certification/source-revisions/"
    "clock-oracle-r22-guard-read-source-r1"
)
UCRT = Path("C:/Program Files (x86)/Windows Kits/10/Source/10.0.26100.0/ucrt")
UCRT_PINS = {
    "env/get_environment_from_os.cpp": "C125DD169F72F0BB2E472F4BC707EAF014D38618CD057C9F952AA3908513F61B",
    "env/environment_initialization.cpp": "F457A8331C7981A8EC017A8DC364628A32F2FA2CE8AD82F901EF04F1DD44FDE5",
    "env/setenv.cpp": "E993CDFD73BA428744BA910DC1FD85168704362E8AD2392C20A7AC019BCDC7C4",
    "internal/initialization.cpp": "C86A17451507AB55F89D60FFF3D9713FCB6A0467E62B5A8D0982BF094DC81AB3",
    "startup/argv_data.cpp": "D6842504E923DBF581A6949E1E95D7932317907EA0D541FA5D2DC279178E852F",
    "startup/argv_parsing.cpp": "3D93772AE6B0730A36011F6F062387008509F965D4C9262337209971803B292F",
}
VC_STARTUP = Path(
    "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/"
    "14.44.35207/crt/src/vcruntime"
)
VC_STARTUP_PINS = {
    "argv_mode.cpp": "6B5E8F1F0FE7EB4C44351BE482847FC7C6393EEF3D91C89BD89A52D3510C50ED",
    "exe_common.inl": "06302037B260F066FED492D8BB29EF08D9906FD3AFB9DD34A71B9C4C537A9DBC",
    "vcstartup_internal.h": "A613679D05289005BCAE868AC4EA0A1123B4FA45AE7C3EDD9F4DCE2215010C22",
}


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def pinned(path, expected):
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == expected, f"pin differs: {path.name}")
    return raw


def crt_environment(block_units, value_count, width):
    return width * (2 * block_units - 1) + (value_count + 1) * 8


def main():
    manifest = json.loads(pinned(
        REVISION / "source-manifest.json", "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"))
    sources = {Path(row["path"]).name: pinned(ROOT / row["path"], row["sha256"]).decode()
               for row in manifest["supervisor"]["native_sources"]}
    require(set(sources) == {"sup.h", "sup_codec.cpp", "sup_core.cpp", "sup_io.cpp", "sup_main.cpp"},
            "supervisor source set differs")
    io, entry, codec = (sources[name] for name in ("sup_io.cpp", "sup_main.cpp", "sup_codec.cpp"))
    request = io.split("bool read_request(", 1)[1].split("bool hash_file(", 1)[0]
    require("state.request_file=file;" in request
            and "state.request.captured=length;" in request and "close_one(" not in request,
            "request lifetime differs")
    require("if (!close_one(state,state.hash_file_owner,1u<<13)) ok=false;" in io
            and io.count("state.search_owner=search;") == 1
            and io.count("state.terminal_file=CreateFileW(") == 1, "file and search owners differ")
    require("open_capture_owners(state) && create_contained_target(state) && start_workers(state)" in entry
            and entry.index("cleanup_prefix(state);finalize_record(state)") < entry.index("publish_terminal(state)")
            and entry.count("CreateThread(nullptr,0,worker_entry,&worker,0,nullptr)") == 1
            and entry.count("CreateEventW(nullptr,") == 3, "worker and terminal schedule differs")
    create = io.split("bool create_contained_target(", 1)[1].split("bool write_chunk(", 1)[0]
    require(create.count("CreateProcessW(") == 1
            and "CREATE_SUSPENDED|CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT|EXTENDED_STARTUPINFO_PRESENT" in create
            and "limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;" in create
            and "state.inherited[0]=state.nul_input; state.inherited[1]=state.child_writes[0]; state.inherited[2]=state.child_writes[1];" in create
            and "if (!close_one(state,state.nul_input,1u<<13)) closed=false;" in create
            and "if (!close_one(state,state.child_writes[i],1u<<13)) closed=false;" in create
            and "return closed;" in create, "target setup and inherited references differ")

    common = {"request", "out_log", "err_log", "out_reader", "err_reader", "job", "target_process", "primary_thread"}
    parent_stdio = {"nul", "out_writer", "err_writer"}
    worker = {f"{direction}_{role}" for direction in ("out", "err") for role in ("thread", "ready", "ack", "started")}
    phase_max = {
        "target_return_before_parent_stdio_closes": len(common | parent_stdio),
        "worker_setup_through_target_run": len(common | worker),
        "one_concurrent_stage_scan": len(common | worker | {"search"}),
        "terminal_publication_retaining_failed_closes": len(common | worker | {"search", "terminal"}),
    }
    require(list(phase_max.values()) == [11, 16, 17, 18], "unexpected phase widths")

    path_units = int(re.search(r"path_units = (\d+);", sources["sup.h"])[1])
    runner_pin, = (row["sha256"] for row in manifest["sources"] if row["path"] == "run-clock.ps1")
    runner = pinned(REVISION / "run-clock.ps1", runner_pin).decode().replace("\r\n", "\n")
    require("$taskTransportRoot=[IO.Path]::Join($taskRoot,'transport')" in runner
            and "$taskOpaqueRoot=[IO.Path]::Join($taskRoot,'opaque-temp')" in runner
            and "$taskPath=[IO.Path]::Join($taskTransportRoot,('child-'+$Index+'.request.bin'))" in runner
            and "function Get-TaskSupervisorRequestArgument([string]$Path){\n Assert-TaskString $Path 512" in runner
            and runner.index("$taskRequestArgument=Get-TaskSupervisorRequestArgument $taskRequestPath")
            < runner.index("$taskProcess=[Diagnostics.Process]::Start($taskInfo)"), "parent path admission changed")
    require(r'join_path(state,r.working,L"transport",9,expected)' in codec
            and 'join_path(state,r.transport,terminal_name,20,r.terminal_path)' in codec
            and r'join_path(state,r.working,L"build\\clock_stage.exe",21,expected)' in codec
            and 'join_path(state,r.working,L"data",4,expected)' in codec
            and 'join_path(state,r.working,L"opaque-temp",11,expected)' in codec
            and "r.argument_count!=(r.child_index==0?28u:6u)" in codec
            and "number.length>19" in codec, "native input paths or numeric widths differ")
    helper_prefix = path_units - len("transport") - 1 - len("child-0.request.bin")
    target_prefix = path_units - len("transport") - 1 - len("child-0.terminal.bin")
    executable = target_prefix + len(r"build\clock_stage.exe")
    data = target_prefix + len("data")
    native_arguments = 7 + 2 + data + 64 + 3 * 19
    command = 2 + executable + 6 + native_arguments + 1
    environment = manifest["environment"]
    require(set(environment) == {"SystemRoot", "windir", "PATH", "INCLUDE", "LIB", "TEMP", "TMP", "VSLANG"}
            and environment["TEMP"] == environment["TMP"] == "<OWNED_OPAQUE_TEMP>", "launch environment differs")
    fixed = sum(len(value) for key, value in environment.items() if key not in {"TEMP", "TMP"})
    count = len(environment)
    fixed_block = sum(map(len, environment)) + fixed + 2 * count + 1
    helper_block = fixed_block + 2 * (helper_prefix + len("opaque-temp"))
    target_block = fixed_block + 2 * (target_prefix + len("opaque-temp"))
    target_argv = executable + native_arguments - 2 + 7 + 8 * 8
    helper_arg_units = len("--request16=") + 4 * path_units
    helper_command = 2 + path_units + 1 + helper_arg_units + 1
    helper_argv = 2 * (path_units + helper_arg_units + 2) + 3 * 8
    helper_wide = crt_environment(helper_block, count, 2)
    helper_narrow = crt_environment(helper_block, count, 1)
    target_narrow = crt_environment(target_block, count, 1)
    # Clone growth needs at most one empty-table request plus the eight
    # successive sizes. Failed first empty-table requests reduce later visits.
    empty_then_grow = (8 * (first + sum(range(2, count - first + 3))) for first in range(1, count + 1))
    clone_arrays = max(count * 8, *empty_then_grow)
    helper_clone = 2 * (helper_block - 1) + clone_arrays
    helper_failed_wide = helper_narrow + 2 * helper_wide + helper_clone
    require((helper_prefix, target_prefix, helper_block, target_block, command, helper_argv, target_argv)
            == (483, 482, 1545, 1543, 1128, 5172, 1188), "launch arithmetic differs")
    require(helper_command == 2576
            and (helper_narrow, helper_wide, helper_narrow + helper_wide, target_narrow,
                 clone_arrays, helper_clone, helper_failed_wide) == (3161, 6250, 9411, 3157, 360, 3448, 19109),
            "CRT startup arithmetic differs")
    for name, expected in UCRT_PINS.items():
        pinned(UCRT / name, expected)
    for name, expected in VC_STARTUP_PINS.items():
        pinned(VC_STARTUP / name, expected)
    print(json.dumps({
        "known_helper_handle_references": phase_max,
        "target_inherited_handle_references": len(parent_stdio),
        "supplied_utf16_units": {"helper_command": helper_command, "helper_environment": helper_block,
                                 "native_target_command": command, "native_target_environment": target_block},
        "conditional_crt_payload_requests_bytes": {
            "target_narrow_argv": target_argv, "target_narrow_environment": target_narrow,
            "helper_wide_argv": helper_argv, "helper_narrow_environment": helper_narrow,
            "helper_wide_environment": helper_wide, "helper_direct_environment_sum": helper_narrow + helper_wide,
            "helper_wide_clone_after_two_direct_attempts": helper_clone,
            "helper_environment_total_with_failed_wide_attempts": helper_failed_wide,
        },
    }, indent=2))


if __name__ == "__main__":
    main()
