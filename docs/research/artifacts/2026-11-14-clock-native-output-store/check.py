"""Check the pinned R22 native output-destination worksheet without running the helper."""

from collections import Counter
import hashlib
import json
from pathlib import Path
import re


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
RESEARCH = ROOT / "docs/research"
MANIFEST = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                   "clock-oracle-r22-guard-read-source-r1/source-manifest.json")
MANIFEST_PIN = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"
INCLUDED_CALLS = {
    "CreatePipe": 1, "CreateProcessW": 1, "CreateThread": 1,
    "DeleteProcThreadAttributeList": 1, "FindFirstFileW": 1, "FindNextFileW": 1,
    "GetCommandLineW": 1, "GetExitCodeProcess": 1, "GetFileAttributesExW": 1,
    "GetFileSizeEx": 2, "GetFullPathNameW": 1, "GetModuleFileNameW": 1,
    "GetVolumePathNameW": 1, "InitializeProcThreadAttributeList": 2,
    "InterlockedCompareExchange": 5, "InterlockedExchange": 2,
    "QueryInformationJobObject": 1, "ReadFile": 5, "SetFilePointerEx": 2,
    "UpdateProcThreadAttribute": 2, "WriteFile": 2,
    "AcquireSRWLockExclusive": 5, "AcquireSRWLockShared": 1,
    "ReleaseSRWLockExclusive": 5, "ReleaseSRWLockShared": 1,
}
RETURN_ONLY_OR_OPAQUE_CALLS = {
    "CancelSynchronousIo": 1, "CloseHandle": 2, "CompareStringOrdinal": 1,
    "CreateEventW": 3, "CreateFileW": 5, "CreateJobObjectW": 1, "ExitProcess": 1,
    "FindClose": 2, "FlushFileBuffers": 2, "GetFileAttributesW": 1, "GetLastError": 57,
    "GetProcessHeap": 2, "GetTickCount64": 4, "HeapAlloc": 1, "HeapFree": 1,
    "ResetEvent": 1, "ResumeThread": 1, "SetEvent": 3, "SetHandleInformation": 1,
    "SetInformationJobObject": 1, "Sleep": 2, "TerminateJobObject": 1,
    "WaitForSingleObject": 6,
}
SDK_HEADERS = {
    "fileapi.h", "jobapi2.h", "libloaderapi.h", "minwinbase.h", "namedpipeapi.h",
    "processenv.h", "processthreadsapi.h", "synchapi.h", "winnt.h",
}


def verify(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def pinned_source():
    raw = MANIFEST.read_bytes()
    verify(digest(raw) == MANIFEST_PIN, "manifest differs")
    sources = {}
    for row in json.loads(raw)["supervisor"]["native_sources"]:
        path = ROOT / row["path"]
        data = path.read_bytes()
        verify(len(data) == row["bytes"] and digest(data) == row["sha256"], f"pin differs: {path}")
        sources[path.name] = data.decode("utf-8")
    return sources


def pinned_sdk():
    receipt = HERE.with_name("2026-11-14-clock-supervisor-abi") / "r22-object-result.json"
    result = json.loads(receipt.read_bytes())
    verify(result["manifest_sha256"] == MANIFEST_PIN, "compile receipt subject differs")
    headers = {}
    for name, pin in result["include_hashes"].items():
        path = Path(name)
        basename = path.name.lower()
        if basename in SDK_HEADERS:
            raw = path.read_bytes()
            verify(digest(raw) == pin, f"SDK pin differs: {path}")
            headers[basename] = raw.decode("utf-8-sig")
    verify(set(headers) == SDK_HEADERS, "missing SDK declaration header")
    return headers


def check_inventory(sources, sdk):
    cpp = "\n".join(value for name, value in sources.items() if name.endswith(".cpp"))
    cpp = re.sub(r"//[^\r\n]*|/\*.*?\*/", "", cpp, flags=re.S)
    found = Counter(re.findall(r"\b([A-Z][a-z][A-Za-z0-9_]+)\s*\(", cpp))
    verify(INCLUDED_CALLS.keys().isdisjoint(RETURN_ONLY_OR_OPAQUE_CALLS), "overlapping groups")
    verify(found == INCLUDED_CALLS | RETURN_ONLY_OR_OPAQUE_CALLS, "unclassified or changed native call")
    verify("_Inout_opt_ LPWSTR lpCommandLine" in sdk["processthreadsapi.h"]
           and "_Out_ LPPROCESS_INFORMATION lpProcessInformation" in sdk["processthreadsapi.h"]
           and "_Out_ PHANDLE hReadPipe" in sdk["namedpipeapi.h"]
           and "_Out_ PHANDLE hWritePipe" in sdk["namedpipeapi.h"], "creation output ABI differs")
    verify("_Out_writes_bytes_to_opt_(nNumberOfBytesToRead, *lpNumberOfBytesRead)" in sdk["fileapi.h"]
           and "_In_reads_bytes_opt_(nNumberOfBytesToWrite)" in sdk["fileapi.h"]
           and "_Out_writes_(cchBufferLength) LPWSTR lpszVolumePathName" in sdk["fileapi.h"]
           and "_Outptr_opt_ LPWSTR* lpFilePart" in sdk["fileapi.h"], "file output ABI differs")
    verify("WCHAR  cFileName[ MAX_PATH ]" in sdk["minwinbase.h"]
           and "WCHAR  cAlternateFileName[ 14 ]" in sdk["minwinbase.h"]
           and "_Out_writes_bytes_to_(cbJobObjectInformationLength, *lpReturnLength)"
           in sdk["jobapi2.h"], "record or query ABI differs")
    return len(found), sum(found.values())


def integer_in_header(header, name):
    return int(re.search(rf"constexpr (?:U32|U64) {name} = (\d+);", header)[1])


def requested(maximum, fragment):
    remainder = min(maximum, fragment)
    return (maximum - remainder) * remainder + remainder * (remainder + 1) // 2


def arithmetic(sources):
    header = sources["sup.h"]
    codec = sources["sup_codec.cpp"]
    io = sources["sup_io.cpp"]
    main = sources["sup_main.cpp"]
    attribute = HERE.with_name("2026-11-14-clock-attribute-size") / "result.json"
    observed = json.loads(attribute.read_bytes())
    verify((observed["attribute_count"], observed["flags"], observed["list_buffer"],
            observed["returned"], observed["last_error"], observed["required_bytes"])
           == (2, 0, None, 0, 122, 72), "attribute observation differs")
    attribute_bytes = observed["required_bytes"]
    path = integer_in_header(header, "path_units") + 1
    worker = integer_in_header(header, "pipe_buffer_bytes")
    request = integer_in_header(header, "request_max_bytes")
    command = integer_in_header(header, "target_command_units")
    verify((path, worker, request, command) == (513, 4096, 41260, 29271), "output caps differ")
    fragment = int(re.search(r"want=remaining>(\d+)\?\1:", io)[1])
    caps = [int(n) for n in re.findall(r"hash_file\(state,(?:expected|r.target),(\d+),", codec)]
    verify(fragment == 65536 and caps == [1048576, 33554432], "file request caps differ")
    verify("ReadFile(file,state.request.capture+at,length-at,&got,nullptr)" in io
           and "ReadFile(file,state.hash_buffer,want,&got,nullptr)" in io
           and "pipe_buffer_bytes-worker.active_fill,&got,nullptr" in main,
           "read destination differs")
    verify("GetFullPathNameW(path.cells, path_units + 1, full, nullptr)" in io
           and "GetVolumePathNameW(path.cells, volume, path_units + 1)" in io
           and "GetModuleFileNameW(nullptr,actual,path_units+1)" in codec
           and "if (state.attributes!=nullptr) { DeleteProcThreadAttributeList(state.attributes);"
           " state.attributes=nullptr; }" in io, "path or retirement output differs")
    scan = 8 * 2 * path + 10 * (11 + 260 + 14) + 7 * 36
    return {
        "Request file body": (requested(request, request), 0),
        "Manifest hash body": (requested(caps[0], fragment), 0),
        "Target hash body": (requested(caps[1], fragment), 0),
        "Directory scans": ((1 + 259 + 1048577) * scan, (309 + 2) * scan),
        "Non-scan path validations": (8 * 2 * path, 2 * path),
        "Worker reads": (1071172 * worker, 2 * worker),
        "Interlocked destinations": (2 * 14402 + 14404 + 1071176 + 14400 + 2,
                                     2 * (2 * 2 + 2 + 2) + 40 + 2 + 2 + 4),
        "Process command": (command, 0),
        "Attribute list": (3 * attribute_bytes, attribute_bytes),
        "Job query": (0, 48),
    }


def main():
    sources = pinned_source()
    names, sites = check_inventory(sources, pinned_sdk())
    values = arithmetic(sources)
    prefix = sum(pair[0] for pair in values.values())
    tail = sum(pair[1] for pair in values.values())
    values["Projection with qualified 72-byte list"] = (prefix, tail)
    worksheet = RESEARCH / "2026-11-14-clock-r22-native-output-store-census.md"
    table = worksheet.read_text(encoding="utf-8").split("| Additional output |", 1)[1]
    row = r"^\| ([\w -]+) \| (?:\x603A = )?([\d,]+)\x60? \| (?:\x60A = )?([\d,]+)\x60? \|$"
    rows = re.findall(row, table, flags=re.M)
    found = {name: (int(body.replace(",", "")), int(suffix.replace(",", "")))
             for name, body, suffix in rows}
    verify(found == values and len(rows) == len(values), "worksheet output rows differ")
    verify((prefix, tail, 3 * (prefix + tail)) == (2280550078421, 3526812, 6841660815699),
           "output projection arithmetic differs")
    print(f"PASS: {names} Win32 names across {sites} pinned sites, "
          f"{prefix:,} prefix and {tail:,} tail mixed destination units per helper")


if __name__ == "__main__":
    main()
