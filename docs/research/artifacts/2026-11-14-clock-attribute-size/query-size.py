"""Observe the local two-entry process attribute list size without creating a list."""

import hashlib
import json
from pathlib import Path
import sys


SYSTEM32 = Path("C:/Windows/System32")
HOST_FILES = {
    "kernel32.dll": "E132EBA4166256E2F414B91FE7D5386590BC5EA8626AEE5AF9C9738383CF2AE1",
    "KernelBase.dll": "5C29B73622CE0976E1ADA566F31F2197BA0F8BEAEA89C3EC033366126172C3D0",
    "ntdll.dll": "651AC9041AF8E6B5EBD331D53163244C0A27D36D1433F0F15CAC1F28C5F2E82C",
}
PYTHON_FILES = {
    "python.exe": "5341746F92483A93E44C313DE830F2FBA2956F0759094404A16B2FED06C9A2ED",
    "python313.dll": "0565965617D94274D7F2C2958D0BEF33392CD9D2F346F99D8E1BEDBDF264EE85",
    "DLLs/_ctypes.pyd": "697D05CAC7C167C00CCF22EA4FDBC7A8DB93AB9C6421061191558E42478068C5",
    "DLLs/libffi-8.dll": "EFF52743773EB550FCC6CE3EFC37C85724502233B6B002A35496D828BD7B280A",
    "Lib/ctypes/__init__.py": "2E63F01AC6B8FFC1ACFA4EEC4C253EEF8FBD65E09506D690CB3BE71A2D78303A",
    "Lib/ctypes/_endian.py": "F2752AB2425DAC9BFAFF5AA1A3C6CD66AA342FEE0DB2F2A42DE568FC59D5CBEA",
    "vcruntime140.dll": "052AD6A20D375957E82AA6A3C441EA548D89BE0981516CA7EB306E063D5027F4",
    "vcruntime140_1.dll": "6A99BC0128E0C7D6CBBF615FCC26909565E17D4CA3451B97F8987F9C6ACBC6C8",
}


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def verify(base, files):
    for name, expected in files.items():
        with (base / name).open("rb") as data:
            observed = hashlib.file_digest(data, "sha256").hexdigest().upper()
        require(observed == expected, f"profile file changed: {name}")


def main():
    require(sys.platform == "win32" and sys.implementation.name == "cpython"
            and sys.version_info[:3] == (3, 13, 5) and sys.maxsize == (1 << 63) - 1,
            "expected pinned 64-bit CPython")
    require(sys.flags.isolated and sys.flags.no_site and sys.flags.ignore_environment
            and sys.dont_write_bytecode and not sys.flags.optimize and len(sys.argv) == 1,
            "run with python -I -S -B and no arguments")
    windows = sys.getwindowsversion()
    require(windows[:3] == (10, 0, 26200) and windows.platform_version == (10, 0, 26100),
            "unexpected Windows version profile")
    python_dir = Path(sys.executable).resolve().parent
    require(Path(sys.executable).name.casefold() == "python.exe"
            and Path(sys.base_prefix).resolve() == python_dir, "unexpected Python location")
    verify(python_dir, PYTHON_FILES)
    verify(SYSTEM32, HOST_FILES)

    import _ctypes
    import ctypes
    import ctypes._endian

    for module, path in (
        (ctypes, "Lib/ctypes/__init__.py"),
        (ctypes._endian, "Lib/ctypes/_endian.py"),
        (_ctypes, "DLLs/_ctypes.pyd"),
    ):
        require(Path(module.__file__).resolve() == (python_dir / path).resolve(),
                f"unexpected Python module origin: {module.__name__}")
    require(ctypes.sizeof(ctypes.c_void_p) == ctypes.sizeof(ctypes.c_size_t) == 8,
            "unexpected pointer or SIZE_T width")
    kernel = ctypes.WinDLL(str(SYSTEM32 / "kernel32.dll"), use_last_error=True)
    query = kernel.InitializeProcThreadAttributeList
    query.argtypes = (ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32,
                      ctypes.POINTER(ctypes.c_size_t))
    query.restype = ctypes.c_int
    required_bytes = ctypes.c_size_t(0)
    ctypes.set_last_error(0)
    returned = query(None, 2, 0, ctypes.byref(required_bytes))
    error = ctypes.get_last_error()

    verify(python_dir, PYTHON_FILES)
    verify(SYSTEM32, HOST_FILES)
    require(returned == 0 and error == 122 and required_bytes.value > 0,
            f"sizing query refused: return={returned}, last_error={error}, bytes={required_bytes.value}")
    print(json.dumps({
        "windows_reported_version": list(windows[:3]),
        "windows_platform_version": list(windows.platform_version),
        "python_files": PYTHON_FILES,
        "host_files": HOST_FILES,
        "attribute_count": 2,
        "flags": 0,
        "list_buffer": None,
        "returned": returned,
        "last_error": error,
        "required_bytes": required_bytes.value,
        "identity_scope": "Selected on-disk files and Python module origins; DLL loader association unverified",
    }, indent=2))


if __name__ == "__main__":
    main()
