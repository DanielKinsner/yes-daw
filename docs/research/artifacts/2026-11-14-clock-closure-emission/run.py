"""Check named local closure data emission in one isolated MSVC /c probe."""

import argparse
import ctypes
from pathlib import Path
import struct
import subprocess
import sys


HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "2026-11-14-clock-direct-layout"))
from measure import verify_inputs
from prepare import PROFILE, digest, load, require


def run(compiler, output):
    allowed = verify_inputs(compiler)
    source = HERE / "probe.cpp"
    source_digest = digest(source.read_bytes())
    output.mkdir()
    obj, deps = output / "probe.obj", output / "dependencies.json"
    profile = load(PROFILE)
    env = profile["environment"] | {
        "PATH": f"{compiler};{profile['environment']['SystemRoot']}/System32",
        "TEMP": str(output), "TMP": str(output),
    }
    args = [str(compiler / "cl.exe"), "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX",
            "/MD", "/permissive-", "/fp:strict", "/O2", "/D_ITERATOR_DEBUG_LEVEL=0",
            "/c", str(source), f"/Fo{obj}", "/sourceDependencies", str(deps)]
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
        require(path.stat().st_size < 1024 * 1024, f"probe output exceeded limit: {path.name}")
    require(digest(source.read_bytes()) == source_digest and verify_inputs(compiler) == allowed,
            "source or compiler input changed")
    dependency = load(deps)["Data"]
    includes = {Path(path).resolve() for path in dependency["Includes"]}
    require(Path(dependency["Source"]).resolve() == source and includes <= allowed.keys(),
            "unexpected compiler dependency")
    rows = decode(obj.read_bytes(), b"ClosureLayout")
    require(rows == {1: (8, 8), 2: (1, 1)}, "missing or unexpected closure records")
    print(f"PASS: probe closure records {rows}; COFF {digest(obj.read_bytes())}; {len(includes)} pinned includes")


def decode(raw, export_name):
    require(len(raw) >= 20, "truncated object")
    machine, sections, _, symbols, count, optional, _ = struct.unpack_from("<HHIIIHH", raw)
    require(machine == 0x8664 and optional == 0 and 1 <= sections
            and 20 + 40 * sections <= symbols <= len(raw)
            and count <= (len(raw) - symbols) // 18, "bad COFF header")
    rows = {}
    directives = b""
    for i in range(sections):
        name, _, _, size, start, _, _, relocations, _, flags = struct.unpack_from(
            "<8sIIIIIIHHI", raw, 20 + 40 * i)
        name = name.rstrip(b"\0")
        if name == b".drectve":
            require(start <= symbols and size <= symbols - start, "bad directive region")
            directives = raw[start:start + size]
        if name == b".yclos":
            require(size == 32 and 20 + sections * 40 <= start <= symbols - size
                    and relocations == 0 and flags & 0xE0000040 == 0x40000040,
                    "bad layout data region")
            magic, site, extent, alignment = struct.unpack_from("<4Q", raw, start)
            require(magic == 0x5944434C4F533031 and site not in rows,
                    "bad closure record tag or duplicate site")
            rows[site] = (extent, alignment)
    exports = [token for token in directives.split() if token.upper().startswith(b"/EXPORT:")]
    require(len(exports) == 2 and all(export_name in token for token in exports),
            "missing closure export directives")
    require(rows.keys() == {1, 2} and all(
        size > 0 and alignment > 0 and size % alignment == 0
        and alignment & (alignment - 1) == 0
        for size, alignment in rows.values()
    ), "missing or invalid closure records")
    return rows


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    run(args.compiler.resolve(strict=True), args.output.resolve())
