"""Read the current headless launcher's job membership and page geometry."""

import ctypes
import hashlib
import json
from pathlib import Path
import struct
import sys


SYSTEM32 = Path("C:/Windows/System32")
FILES = {
    "kernel32.dll": "E132EBA4166256E2F414B91FE7D5386590BC5EA8626AEE5AF9C9738383CF2AE1",
    "KernelBase.dll": "5C29B73622CE0976E1ADA566F31F2197BA0F8BEAEA89C3EC033366126172C3D0",
}


class SystemInfo(ctypes.Structure):
    _fields_ = [
        ("architecture", ctypes.c_ushort), ("reserved", ctypes.c_ushort), ("page_size", ctypes.c_uint),
        ("min_address", ctypes.c_void_p), ("max_address", ctypes.c_void_p), ("processor_mask", ctypes.c_size_t),
        ("processors", ctypes.c_uint), ("processor_type", ctypes.c_uint), ("granularity", ctypes.c_uint),
        ("processor_level", ctypes.c_ushort), ("processor_revision", ctypes.c_ushort),
    ]


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify():
    require(sys.platform == "win32" and sys.getwindowsversion()[:3] == (10, 0, 26200)
            and ctypes.sizeof(SystemInfo) == 48 and SystemInfo.granularity.offset == 40, "unexpected launch profile")
    for name, pin in FILES.items():
        require(hashlib.sha256((SYSTEM32 / name).read_bytes()).hexdigest().upper() == pin, f"unexpected {name} bytes")


def main():
    verify()
    api = ctypes.WinDLL(str(SYSTEM32 / "kernel32.dll"), use_last_error=True)
    handle = ctypes.c_void_p
    word = ctypes.c_uint
    flag = ctypes.c_int
    current = api.GetCurrentProcess
    current.argtypes = ()
    current.restype = handle
    in_job = api.IsProcessInJob
    in_job.argtypes = (handle, handle, ctypes.POINTER(flag))
    in_job.restype = flag
    get_system = api.GetSystemInfo
    get_system.argtypes = (ctypes.POINTER(SystemInfo),)
    get_system.restype = None
    query_job = api.QueryInformationJobObject
    query_job.argtypes = (handle, ctypes.c_int, ctypes.c_void_p, word, ctypes.POINTER(word))
    query_job.restype = flag

    info = SystemInfo()
    get_system(ctypes.byref(info))
    require((info.architecture, info.page_size, info.granularity) == (9, 4096, 65536),
            "unexpected memory geometry")
    member = flag()
    require(in_job(current(), None, ctypes.byref(member)) != 0,
            f"IsProcessInJob refused with error {ctypes.get_last_error()}")
    report = {"scope": "current headless Python launch context only", "page_bytes": info.page_size,
              "reservation_granularity_bytes": info.granularity, "in_job": bool(member.value)}
    if member.value:
        limits = (ctypes.c_uint64 * 18)()  # JOBOBJECT_EXTENDED_LIMIT_INFORMATION, 144 bytes and 8-byte aligned.
        ui = word()
        returned = word()
        require(query_job(None, 9, limits, ctypes.sizeof(limits), ctypes.byref(returned)) != 0
                and returned.value == ctypes.sizeof(limits), "immediate job extended limits unavailable")
        flags, = struct.unpack_from("<I", bytes(limits), 16)
        require(query_job(None, 4, ctypes.byref(ui), ctypes.sizeof(ui), ctypes.byref(returned)) != 0
                and returned.value == ctypes.sizeof(ui), "immediate job UI limits unavailable")
        report["immediate_job_limit_flags"] = flags
        report["immediate_job_ui_flags"] = ui.value
    verify()
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
