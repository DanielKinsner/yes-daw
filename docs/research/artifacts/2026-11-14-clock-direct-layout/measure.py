"""Compile one data-only layout object with the recovered compiler and verify its cells."""

import argparse
import ctypes
import json
import os
from pathlib import Path
import struct
import subprocess
import time

from prepare import HERE, PINS, PROFILE, ROOT, digest, load, matches, require, toolchain_inputs


TYPES = (
    "std::nullptr_t", "std::less<void>", "std::equal_to<void>", "std::_Fake_allocator",
    "std::_Fake_proxy_ptr_impl", "std::_Tidy_guard<std::vector<qc::Change>>",
    "std::_Zero_then_variadic_args_t", "std::_One_then_variadic_args_t",
    "std::_String_constructor_concat_tag", "std::allocator<qc::Change>",
    "std::allocator<char>", "::__std_exception_data", "yesdaw::engine::TempoMapView",
)
MIB = 1024 * 1024
REVISIONS = ROOT / "build-ci/musical-audio-persistent-planner-certification/source-revisions"
CLOCK_INCLUDE = REVISIONS / "waveform-definition-r3/independent_clock.h"
COST_INCLUDE = REVISIONS / "clock-oracle-r13-streamed-dependency-source-r1/source-costs.h"
SOURCE = HERE / "layout.cpp"


def verify_inputs(compiler):
    tool_pins, header_pins = toolchain_inputs()
    found = {p.relative_to(compiler).as_posix().casefold(): p
             for p in compiler.rglob("*") if p.is_file()}
    require(found.keys() == tool_pins.keys(), "compiler directory membership changed")
    for name, path in found.items():
        require(matches(path.read_bytes(), tool_pins[name]), f"compiler file changed: {name}")

    archive_pins = {row["path"]: row for row in load(PINS)["inputs"]}
    include_files = [CLOCK_INCLUDE, COST_INCLUDE, ROOT / "src/engine/Time.h"]
    allowed = {Path(pin["path"]).resolve(): pin for pin in header_pins}
    for path in include_files:
        allowed[path.resolve()] = archive_pins[path.relative_to(ROOT).as_posix()]
    for path, pin in allowed.items():
        require(matches(path.read_bytes(), pin), f"header changed: {path}")
    return allowed


def read_cells(object_file, section_name, symbol, cell_count):
    raw = object_file.read_bytes()
    require(20 <= len(raw) <= 16 * MIB, "invalid COFF file size")
    machine, count, _, symbols, entries, optional, _ = struct.unpack_from("<HHIIIHH", raw)
    require(machine == 0x8664 and optional == 0 and 1 <= count <= 512, "expected x64 COFF")
    require(20 + count * 40 <= symbols <= len(raw) - 4, "invalid COFF table extent")
    require(1 <= entries <= 65536 and symbols + 18 * entries + 4 <= len(raw), "invalid symbol table")
    sections = []
    for index in range(count):
        label, _, _, size, start, _, _, relocations, _, flags = struct.unpack_from(
            "<8sIIIIIIHHI", raw, 20 + index * 40)
        require(not (size and flags & 0x20000000), "unexpected executable COFF data")
        if label.rstrip(b"\0") == section_name:
            require(relocations == 0 and flags & 0xE0000040 == 0x40000040,
                    "expected read-only layout data")
            require(size == cell_count * 8 and 20 + count * 40 <= start <= symbols - size,
                    "unexpected layout section extent")
            sections.append((index + 1, start))
    require(len(sections) == 1, "expected one layout section")
    section, start = sections[0]
    string_start = symbols + entries * 18
    (string_size,) = struct.unpack_from("<I", raw, string_start)
    require(4 <= string_size <= len(raw) - string_start, "invalid string table extent")
    names = raw[string_start:string_start + string_size]
    index = 0
    hits = []
    while index < entries:
        name, offset, owner, _, storage, auxiliaries = struct.unpack_from(
            "<8sIhHBB", raw, symbols + index * 18)
        if name.startswith(b"\0" * 4):
            pointer = int.from_bytes(name[4:], "little")
            require(4 <= pointer < len(names), "invalid symbol name offset")
            end = names.find(b"\0", pointer)
            require(end != -1, "unterminated symbol name")
            name = names[pointer:end]
        require(not name.startswith((b"??__E", b"??__F")), "unexpected dynamic initialization")
        if name == symbol:
            hits.append((offset, owner, storage))
        index += 1 + auxiliaries
    require(index == entries and hits == [(0, section, 2)], "unexpected layout data symbol")
    return struct.unpack_from(f"<{cell_count}Q", raw, start), digest(raw)


def decode(object_file):
    cells, object_sha256 = read_cells(object_file, b".yexact", b"yesdaw_exact_context_cells", 34)
    require(cells[:3] == (0x5944455841435431, 1, 31), "unexpected cell header")
    require(cells[-5:] == (194435228, 1944, 8, 0, 76), "unexpected compiler or source profile")
    layouts = {}
    for index, name in enumerate(TYPES):
        size, alignment = cells[3 + index * 2:5 + index * 2]
        require(size > 0 and alignment > 0 and alignment & (alignment - 1) == 0
                and size % alignment == 0, f"invalid size/alignment for {name}")
        layouts[name] = {"sizeof": size, "alignof": alignment}
    return {"object_sha256": object_sha256, "layouts": layouts, "profile_cells": list(cells[-5:])}


def measure(compiler, output):
    allowed = verify_inputs(compiler)
    source_hash = digest(SOURCE.read_bytes())
    output.mkdir()
    obj = output / "layout.obj"
    dependencies = output / "dependencies.json"
    stdout = output / "compiler.stdout"
    stderr = output / "compiler.stderr"
    scratch = output / "compiler-temp"
    scratch.mkdir()
    profile = load(PROFILE)
    args = [
        str(compiler / "cl.exe"), "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX",
        "/MD", "/permissive-", "/fp:strict", "/O2", "/D_ITERATOR_DEBUG_LEVEL=0",
        f"/I{CLOCK_INCLUDE.parent}", f"/I{ROOT / 'src'}", f"/I{COST_INCLUDE.parent}",
        "/c", str(SOURCE), f"/Fo{obj}", "/sourceDependencies", str(dependencies),
    ]
    env = profile["environment"] | {
        "PATH": f"{compiler};{profile['environment']['SystemRoot']}/System32",
        "TEMP": str(scratch), "TMP": str(scratch),
    }
    paths = {stdout: MIB, stderr: MIB, obj: 16 * MIB, dependencies: MIB}
    start = time.monotonic()
    kernel = ctypes.WinDLL("kernel32")
    old_mode = kernel.SetErrorMode(0x8003)
    try:
        with stdout.open("xb") as out, stderr.open("xb") as err:
            child = subprocess.Popen(args, cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
                                     stdout=out, stderr=err, creationflags=subprocess.CREATE_NO_WINDOW)
            try:
                while child.poll() is None:
                    require(time.monotonic() - start < 120, "layout compile timeout")
                    require(all(not p.exists() or p.stat().st_size <= cap for p, cap in paths.items()),
                            "layout output extent exceeded")
                    time.sleep(0.05)
            finally:
                if child.poll() is None:
                    child.kill()
                    child.wait(timeout=5)
    finally:
        kernel.SetErrorMode(old_mode)
    require(child.returncode == 0, f"compiler exited {child.returncode}; see {stderr} and {stdout}")
    require(all(p.stat().st_size <= cap for p, cap in paths.items()), "layout output extent exceeded")
    require(stdout.stat().st_size + stderr.stat().st_size <= MIB, "diagnostic extent exceeded")
    require(source_hash == digest(SOURCE.read_bytes()), "layout source changed")
    require(verify_inputs(compiler) == allowed, "compiler or includes changed")
    data = load(dependencies)["Data"]
    includes = {Path(path).resolve() for path in data["Includes"]}
    require(Path(data["Source"]).resolve() == SOURCE and includes <= allowed.keys()
            and all(p.resolve() in includes for p in (CLOCK_INCLUDE, COST_INCLUDE, ROOT / "src/engine/Time.h")),
            "unexpected compiler dependencies")
    receipt = decode(obj) | {
        "source_sha256": source_hash,
        "profile_sha256": digest(PROFILE.read_bytes()),
        "compiler_sha256": digest((compiler / "cl.exe").read_bytes()),
        "include_count": len(includes),
        "include_hashes": {p.name: allowed[p]["sha256"] for p in sorted(includes)},
        "raw_exit": child.returncode,
    }
    require(len(receipt["include_hashes"]) == len(includes), "ambiguous include names")
    (output / "result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS: {len(TYPES)} direct types, {len(includes)} exact headers, MSVC 19.44.35228")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    measure(args.compiler.resolve(strict=True), args.output.resolve())
