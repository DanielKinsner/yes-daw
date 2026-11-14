"""Compile the exact Clock target once to x64 COFF without linking or running it."""

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import re
import runpy
import struct
import subprocess


HERE = Path(__file__).resolve().parent
PROFILE = runpy.run_path(str(HERE.parent / "2026-11-14-clock-supervisor-abi/run-purpose.py"))
ROOT = PROFILE["ROOT"]
FOLDER = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                 "clock-oracle-r22-guard-read-source-r1")
PIN = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"
require = PROFILE["require"]


def hash_file(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest().upper()


def compile_only(output):
    manifest_file = FOLDER / "source-manifest.json"
    raw = manifest_file.read_bytes()
    require(PROFILE["digest"](raw) == PIN, "unexpected R22 manifest")
    manifest = json.loads(raw)
    require(all(value is False for value in manifest["guards"].values())
            and manifest["root_execution_go"] == "ABSENT", "R22 execution guard differs")
    compiler = Path(manifest["compiler"]["path"]).resolve(strict=True)
    require(hash_file(compiler) == manifest["compiler"]["sha256"], "wrong compiler identity")
    headers = PROFILE["verify_inputs"](compiler.parent)
    repo = (ROOT / "src").resolve(strict=True)
    inputs = {(ROOT / row["path"]).resolve(): row["sha256"] for row in manifest["inputs"]
              if (ROOT / row["path"]).resolve().is_relative_to(repo)}
    current = {(FOLDER / row["path"]).resolve(strict=True): row["sha256"]
               for row in manifest["sources"] if row["path"] in {"clock_stage.cpp", "source-costs.h"}}
    require({path.name for path in current} == {"clock_stage.cpp", "source-costs.h"}, "target inputs missing")
    inputs.update(current)

    def verify_target_inputs():
        for path, expected in inputs.items():
            require(hash_file(path) == expected, f"wrong target input: {path}")
    verify_target_inputs()
    source = (FOLDER / "clock_stage.cpp").resolve(strict=True)
    guard_values = re.findall(rb"\b(?:sourceReviewAccepted|sourceWorkAccepted|executionAccepted|"
                              rb"compilerAccepted|produceAccepted|verifyAccepted)=(false|true)",
                              source.read_bytes())
    require(guard_values == [b"false"] * 6, "target guards differ")
    recipe = manifest["compiler"]["arguments"]
    require(recipe[10:12] == ["/I<REPO>/src", "<BASE>/clock_stage.cpp"]
            and recipe[14:16] == ["/sourceDependencies", "<BUILD>/source-dependencies.json"]
            and recipe[16] == "/link", "compiler recipe differs")
    output.mkdir()
    obj = output / "clock_stage.obj"
    deps = output / "source-dependencies.json"
    env = manifest["environment"] | {"TEMP": str(output), "TMP": str(output)}
    flags = [*recipe[:10], f"/I{repo}", "/c"]
    args = [str(compiler), *flags, str(source), f"/Fo{obj}", "/sourceDependencies", str(deps)]
    kernel = ctypes.WinDLL("kernel32")
    old_mode = kernel.SetErrorMode(0x8003)
    try:
        with (output / "stdout").open("xb") as out, (output / "stderr").open("xb") as err:
            result = subprocess.run(args, cwd=output, env=env, stdin=subprocess.DEVNULL, stdout=out,
                                    stderr=err, timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
    finally:
        kernel.SetErrorMode(old_mode)
    require(result.returncode == 0, f"compiler exited {result.returncode}; see {output}")
    require(hash_file(manifest_file) == PIN and PROFILE["verify_inputs"](compiler.parent) == headers,
            "manifest or compiler files changed")
    verify_target_inputs()
    for path in (deps, output / "stdout", output / "stderr"):
        require(path.stat().st_size <= 1024 * 1024, f"output exceeds purpose extent: {path}")
    allowed = {path: row["sha256"] for path, row in headers.items()} | inputs
    dependency = PROFILE["load"](deps)["Data"]
    includes = {Path(name).resolve(strict=True) for name in dependency["Includes"]}
    require(Path(dependency["Source"]).resolve() == source and includes <= allowed.keys()
            and (FOLDER / "source-costs.h").resolve() in includes, "unbound target include")
    require(20 <= obj.stat().st_size <= 32 * 1024 * 1024, "object extent")
    data = obj.read_bytes()
    machine, sections, _, symbols, entries, optional, _ = struct.unpack_from("<HHIIIHH", data)
    require(machine == 0x8664 and sections > 0 and optional == 0
            and 20 + sections * 40 <= symbols <= len(data)
            and entries <= (len(data) - symbols) // 18, "invalid x64 object")
    require(not any(path.suffix.casefold() in {".exe", ".dll"} for path in output.iterdir()),
            "unexpected executable output")
    receipt = {"manifest_sha256": PIN, "compiler_sha256": hash_file(compiler),
               "source_sha256": inputs[source], "flags": [*recipe[:11], "/c"], "raw_exit": result.returncode,
               "object_bytes": len(data), "object_sha256": PROFILE["digest"](data),
               "section_count": sections, "include_count": len(includes),
               "dependency_report_sha256": hash_file(deps),
               "includes": {path.as_posix(): allowed[path] for path in sorted(includes)}}
    (output / "result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS: exact guarded target /c, {len(includes)} distinct pinned includes, {len(data):,} object bytes")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    compile_only(parser.parse_args().output.resolve())
