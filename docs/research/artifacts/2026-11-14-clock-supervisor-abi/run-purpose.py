"""Compile supervisor ABI cells using the exact header and a pinned SDK snapshot."""

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess
import sys


HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "2026-11-14-clock-direct-layout"))
from measure import read_cells, verify_inputs
from prepare import PROFILE, ROOT, digest, load, require


NATIVE = ROOT / "build-ci/20261106-clock-owned-supervisor-source-r4/native"
HEADER_HASH = "36AB958736136FB40A5B91C8383D8C08AC55EEDC8C180FC2A0DEA1B5CDDD32E7"
SDK = Path("C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0")
WIN32_ROOTS = (SDK / "shared", SDK / "um")
SOURCE = HERE / "layout.cpp"
TYPES = (
    "State", "Request", "Terminal", "Worker", "WorkerExit", "Meter", "Catalogue",
    "Wide", "Digest", "Text", "Sha256", "Failure", "PipeCount", "WIN32_FIND_DATAW",
    "WIN32_FILE_ATTRIBUTE_DATA", "STARTUPINFOEXW", "PROCESS_INFORMATION", "SECURITY_ATTRIBUTES",
    "JOBOBJECT_BASIC_ACCOUNTING_INFORMATION", "JOBOBJECT_EXTENDED_LIMIT_INFORMATION",
    "LARGE_INTEGER", "SRWLOCK", "HANDLE",
)
OFFSETS = (
    "State.request", "State.workers", "State.hash_buffer", "State.target_command",
    "State.environment", "State.terminal_capture", "Worker.buffer", "Request.capture",
)


def pin_sdk():
    files = {}
    for root in WIN32_ROOTS:
        for path in root.rglob("*"):
            if path.is_file():
                with path.open("rb") as stream:
                    files[path.resolve()] = hashlib.file_digest(stream, "sha256").hexdigest().upper()
    require(len(files) == 2492, "unexpected Windows SDK inventory")
    return files


def measure(compiler, output):
    allowed = verify_inputs(compiler)
    sdk_pins = pin_sdk()
    header = (NATIVE / "sup.h").resolve()
    require(digest(header.read_bytes()) == HEADER_HASH, "supervisor header changed")
    source_sha256 = digest(SOURCE.read_bytes())

    output.mkdir()
    obj, deps = output / "layout.obj", output / "dependencies.json"
    profile = load(PROFILE)
    env = profile["environment"] | {
        "PATH": f"{compiler};{profile['environment']['SystemRoot']}/System32",
        "TEMP": str(output), "TMP": str(output),
        "INCLUDE": ";".join([profile["environment"]["INCLUDE"], *map(str, WIN32_ROOTS)]),
    }
    args = [str(compiler / "cl.exe"), "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX",
            "/MD", "/permissive-", "/fp:strict", "/O2", "/D_ITERATOR_DEBUG_LEVEL=0",
            f"/I{NATIVE}", "/c", str(SOURCE), f"/Fo{obj}", "/sourceDependencies", str(deps)]
    kernel = ctypes.WinDLL("kernel32")
    old = kernel.SetErrorMode(0x8003)
    try:
        with (output / "stdout").open("xb") as out, (output / "stderr").open("xb") as err:
            result = subprocess.run(args, cwd=output, env=env, stdin=subprocess.DEVNULL,
                                    stdout=out, stderr=err, timeout=60,
                                    creationflags=subprocess.CREATE_NO_WINDOW)
    finally:
        kernel.SetErrorMode(old)
    require(result.returncode == 0, f"compiler failed with {result.returncode}")
    for path in (obj, deps, output / "stdout", output / "stderr"):
        require(path.stat().st_size <= 1024 * 1024, f"output exceeded limit: {path.name}")
    require(verify_inputs(compiler) == allowed and pin_sdk() == sdk_pins, "tool or SDK changed")
    require(digest(SOURCE.read_bytes()) == source_sha256 and digest(header.read_bytes()) == HEADER_HASH,
            "layout input changed")
    dependency = load(deps)["Data"]
    includes = {Path(path).resolve() for path in dependency["Includes"]}
    hashes = {p: row["sha256"] for p, row in allowed.items()} | sdk_pins | {header: HEADER_HASH}
    require(Path(dependency["Source"]).resolve() == SOURCE and header in includes
            and includes <= hashes.keys(), "incorrect or unpinned include")
    global_count = 2 + 2 * len(TYPES) + len(OFFSETS)
    cells, object_sha256 = read_cells(obj, b".ysupabi", b"yesdaw_supervisor_cells", global_count)
    require(cells[:2] == (0x5944535550414249, 1), "unexpected ABI record")
    layouts = {}
    for i, name in enumerate(TYPES):
        size, alignment = cells[2 + i * 2:4 + i * 2]
        require(size > 0 and alignment > 0 and alignment & (alignment - 1) == 0
                and size % alignment == 0, f"invalid layout: {name}")
        layouts[name] = {"sizeof": size, "alignof": alignment}
    receipt = {
        "source_sha256": source_sha256,
        "compiler_sha256": digest((compiler / "cl.exe").read_bytes()),
        "object_sha256": object_sha256,
        "sdk_root": SDK.as_posix(),
        "include_hashes": {path.as_posix(): hashes[path] for path in sorted(includes)},
        "layouts": layouts,
        "offsets": dict(zip(OFFSETS, cells[-len(OFFSETS):])),
        "raw_exit": result.returncode,
    }
    (output / "result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS: {len(layouts)} supervisor/Win32 types and {len(OFFSETS)} offsets; {len(includes)} pinned includes")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    measure(args.compiler.resolve(strict=True), args.output.resolve())
