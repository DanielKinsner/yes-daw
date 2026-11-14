"""Compile the reviewed named-closure instrumentation and decode its two data records."""

import argparse
import ctypes
import json
from pathlib import Path
import runpy
import subprocess
import sys


HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE / "instrument.py"))
ROOT, prepare = BASE["ROOT"], BASE["prepare"]
sys.path.insert(0, str(HERE.parent / "2026-11-14-clock-direct-layout"))
from measure import COST_INCLUDE, CLOCK_INCLUDE, verify_inputs
from prepare import PROFILE, digest, load, require

sys.path.insert(0, str(HERE.parent / "2026-11-14-clock-closure-emission"))
from run import decode


def measure(source_root, compiler, output, prepare_source=prepare, names=("get", "nextBit"),
            project_include=None, replaced_headers=(), first_include=None, required_headers=None):
    approved = prepare_source(source_root)
    allowed = verify_inputs(compiler)
    source = source_root / "checker/clock_stage.cpp"
    staged_headers = {path for path in approved if path != source}
    required_headers = staged_headers if required_headers is None else required_headers
    require(all(path.read_bytes() == expected for path, expected in approved.items()),
            "instrumented source differs")
    output.mkdir()
    obj, deps = output / "closures.obj", output / "dependencies.json"
    profile = load(PROFILE)
    env = profile["environment"] | {
        "PATH": f"{compiler};{profile['environment']['SystemRoot']}/System32",
        "TEMP": str(output), "TMP": str(output),
    }
    if first_include is not None:
        env["INCLUDE"] = f"{first_include};{env['INCLUDE']}"
    project_include = project_include or ROOT / "src"
    args = [str(compiler / "cl.exe"), "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX",
            "/MD", "/permissive-", "/fp:strict", "/O2", "/D_ITERATOR_DEBUG_LEVEL=0",
            f"/I{project_include}", "/c", str(source), f"/Fo{obj}", "/sourceDependencies", str(deps)]
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
    for path in (deps, output / "stdout", output / "stderr"):
        require(path.stat().st_size <= 1024 * 1024, f"output exceeded limit: {path.name}")
    require(obj.stat().st_size <= 16 * 1024 * 1024, "object exceeded limit")
    require(verify_inputs(compiler) == allowed, "compiler or headers changed")
    require(all(path.read_bytes() == expected for path, expected in approved.items()),
            "instrumented source changed")
    dependency = load(deps)["Data"]
    includes = {Path(path).resolve() for path in dependency["Includes"]}
    require(Path(dependency["Source"]).resolve() == source
            and required_headers <= includes <= allowed.keys() | staged_headers
            and not includes.intersection({CLOCK_INCLUDE.resolve(), COST_INCLUDE.resolve(),
                                           *map(Path.resolve, replaced_headers)})
            and (project_include / "engine/Time.h").resolve() in includes, "wrong source dependency set")
    raw = obj.read_bytes()
    sites = range(1, len(names) + 1)
    layouts = decode(raw, b"yesdaw_closure_layout", sites)
    receipt = {
        "source_sha256": {path.relative_to(source_root).as_posix(): digest(data)
                          for path, data in approved.items()},
        "object_sha256": digest(raw),
        "compiler_sha256": digest((compiler / "cl.exe").read_bytes()),
        "include_count": len(includes),
        "layouts": {name: {"sizeof": layouts[site][0], "alignof": layouts[site][1]}
                    for site, name in zip(sites, names)},
        "raw_exit": result.returncode,
    }
    result_json = json.dumps(receipt, indent=2) + "\n"
    (output / "result.json").write_text(result_json, encoding="utf-8", newline="\n")
    print(f"PASS: {len(layouts)} closure layouts, {len(includes)} pinned includes, COFF {receipt['object_sha256']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    measure(args.source.resolve(strict=True), args.compiler.resolve(strict=True), args.output.resolve())
