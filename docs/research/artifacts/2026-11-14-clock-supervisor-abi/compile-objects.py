"""Compile the four exact R20 supervisor translation units without linking."""

import argparse
import ctypes
import json
from pathlib import Path
import runpy
import struct
import subprocess


HERE = Path(__file__).resolve().parent
PURPOSE = runpy.run_path(str(HERE / "run-purpose.py"))
ROOT = PURPOSE["ROOT"]
MANIFEST = ROOT / (
    "build-ci/musical-audio-persistent-planner-certification/source-revisions/"
    "clock-oracle-r20-path-copy-source-r1/source-manifest.json"
)
MANIFEST_HASH = "69A5E104B0FE69B898D14276A15CC1643AEE56658B779417D919B2769EDC4D56"
digest, load, require = (PURPOSE[name] for name in ("digest", "load", "require"))


def compile_objects(compiler, output):
    manifest_raw = MANIFEST.read_bytes()
    require(digest(manifest_raw) == MANIFEST_HASH, "unexpected R20 manifest")
    manifest = json.loads(manifest_raw)
    rows = manifest["supervisor"]["native_sources"]
    sources = {(ROOT / row["path"]).resolve(): row["sha256"] for row in rows}
    require({path.name for path in sources} == {"sup.h", "sup_core.cpp", "sup_codec.cpp",
                                               "sup_io.cpp", "sup_main.cpp"}, "wrong source set")
    header, = [path for path in sources if path.name == "sup.h"]

    def check_sources():
        for path, expected in sources.items():
            require(digest(path.read_bytes()) == expected, f"native source changed: {path}")

    check_sources()
    includes = PURPOSE["verify_inputs"](compiler)
    sdk_pins = PURPOSE["pin_sdk"]()
    allowed = {path: row["sha256"] for path, row in includes.items()} | sdk_pins | {header: sources[header]}
    output.mkdir()
    profile = load(PURPOSE["PROFILE"])
    env = profile["environment"] | {
        "PATH": f"{compiler};{profile['environment']['SystemRoot']}/System32",
        "TEMP": str(output), "TMP": str(output),
        "INCLUDE": ";".join([profile["environment"]["INCLUDE"], *map(str, PURPOSE["WIN32_ROOTS"])]),
    }
    flags = ["/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", "/MD", "/permissive-",
             "/fp:strict", "/O2", "/D_ITERATOR_DEBUG_LEVEL=0", f"/I{header.parent}", "/c"]
    kernel = ctypes.WinDLL("kernel32")
    old_mode = kernel.SetErrorMode(0x8003)
    objects = {}
    include_union = set()
    try:
        for source in sorted(path for path in sources if path.suffix == ".cpp"):
            obj, dep = output / f"{source.stem}.obj", output / f"{source.stem}.deps.json"
            stdout, stderr = output / f"{source.stem}.stdout", output / f"{source.stem}.stderr"
            args = [str(compiler / "cl.exe"), *flags, str(source), f"/Fo{obj}", "/sourceDependencies", str(dep)]
            with stdout.open("xb") as out, stderr.open("xb") as err:
                result = subprocess.run(args, cwd=output, env=env, stdin=subprocess.DEVNULL,
                                        stdout=out, stderr=err, timeout=60,
                                        creationflags=subprocess.CREATE_NO_WINDOW)
            require(result.returncode == 0, f"{source.name}: compiler exited {result.returncode}")
            require(20 <= obj.stat().st_size <= 16 * 1024 * 1024, f"object extent: {source.name}")
            for file in (dep, stdout, stderr):
                require(file.stat().st_size <= 1024 * 1024, f"output extent: {file.name}")
            data = load(dep)["Data"]
            observed = {Path(path).resolve() for path in data["Includes"]}
            require(Path(data["Source"]).resolve() == source and header in observed
                    and observed <= allowed.keys(), f"unbound include: {source.name}")
            include_union.update(observed)
            raw = obj.read_bytes()
            machine, sections, _, symbols, entries, optional, _ = struct.unpack_from("<HHIIIHH", raw)
            require(machine == 0x8664 and sections > 0 and optional == 0
                    and 20 + sections * 40 <= symbols <= len(raw)
                    and entries <= (len(raw) - symbols) // 18, f"unexpected object: {source.name}")
            objects[source.name] = {
                "source_sha256": sources[source],
                "object_sha256": digest(raw),
                "object_bytes": len(raw),
                "section_count": sections,
                "include_count": len(observed),
                "dependency_report_sha256": digest(dep.read_bytes()),
            }
    finally:
        kernel.SetErrorMode(old_mode)
    check_sources()
    require(PURPOSE["verify_inputs"](compiler) == includes and PURPOSE["pin_sdk"]() == sdk_pins,
            "tool or SDK changed")
    receipt = {"manifest_sha256": MANIFEST_HASH,
               "compiler_sha256": digest((compiler / "cl.exe").read_bytes()),
               "supervisor_header_sha256": sources[header], "flags": flags[:-2] + ["/I<sup.h-root>", "/c"],
               "objects": objects,
               "include_hashes": {path.as_posix(): allowed[path] for path in sorted(include_union)}}
    (output / "object-result.json").write_text(json.dumps(receipt, indent=2) + "\n",
                                               encoding="utf-8", newline="\n")
    print(f"PASS: {len(objects)} R20 supervisor COFF objects, no link")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    compile_objects(args.compiler.resolve(strict=True), args.output.resolve())
