"""Compile an admission-enabled target source variant to COFF only."""

import ctypes
import json
from pathlib import Path
import re
import runpy
import struct
import subprocess
import sys


HERE = Path(__file__).resolve().parent
PURPOSE = runpy.run_path(str(HERE / "compile-only.py"))
FOLDER = PURPOSE["FOLDER"]
PROFILE = PURPOSE["PROFILE"]
sha = PROFILE["digest"]
hash_file = PURPOSE["hash_file"]
require = PURPOSE["require"]
ATTEMPT = Path("C:/Users/Daniel Kinsner/AppData/Local/Temp/YesDawClockR22EntryFrame-attempt1")
GUARDS = re.compile(rb"\b(sourceReviewAccepted|sourceWorkAccepted|executionAccepted|"
                    rb"compilerAccepted|produceAccepted|verifyAccepted)=(false|true)")


def preflight():
    manifest_raw = (FOLDER / "source-manifest.json").read_bytes()
    require(sha(manifest_raw) == PURPOSE["PIN"], "R22 manifest changed")
    manifest = json.loads(manifest_raw)
    require(not any(manifest["guards"].values()) and manifest["root_execution_go"] == "ABSENT",
            "unexpected staged authority")
    recipe = manifest["compiler"]["arguments"]
    require(recipe[:11] == ["/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", "/MD", "/permissive-",
                            "/fp:strict", "/O2", "/D_ITERATOR_DEBUG_LEVEL=0", "/I<REPO>/src"], "profile changed")
    receipt_raw = (HERE / "result.json").read_bytes()
    require(sha(receipt_raw) == "D9D2C7ED3C694D141D6807DCA23525DEFCEB2F3FE70A14A760E2EB99B9269A78",
            "guarded target receipt bytes changed")
    receipt = json.loads(receipt_raw)
    require(receipt["manifest_sha256"] == PURPOSE["PIN"] and receipt["include_count"] == 110,
            "guarded target receipt changed")
    pins = {Path(name).resolve(strict=True): digest for name, digest in receipt["includes"].items()}
    require(len(pins) == 110 and all(hash_file(path) == pin for path, pin in pins.items()), "target header changed")
    source = (FOLDER / "clock_stage.cpp").read_bytes()
    source_pin, = [row["sha256"] for row in manifest["sources"] if row["path"] == "clock_stage.cpp"]
    require(sha(source) == source_pin == receipt["source_sha256"], "original target source changed")
    matches = list(GUARDS.finditer(source))
    require(len(matches) == 6 and len({hit[1] for hit in matches}) == 6
            and all(hit[2] == b"false" for hit in matches), "original target guards changed")
    variant = GUARDS.sub(rb"\g<1>=true", source)
    require(len(variant) == len(source) - 6 and GUARDS.sub(rb"\g<1>=false", variant) == source,
            "variant changed outside the six guards")
    compiler = Path(manifest["compiler"]["path"]).resolve(strict=True)
    tools = PROFILE["verify_inputs"](compiler.parent)
    require(hash_file(compiler) == manifest["compiler"]["sha256"], "compiler changed")
    return manifest, compiler, tools, pins, variant


def retire(process):
    if process is None:
        return False
    try:
        if process.poll() is not None:
            return True
        process.kill()
    except (OSError, KeyboardInterrupt):
        pass
    try:
        return process.wait(timeout=5) is not None
    except (OSError, KeyboardInterrupt, subprocess.TimeoutExpired):
        return False


def compile_variant(state):
    manifest, compiler, tools, pins, variant = state
    require(ATTEMPT.parent.resolve(strict=True) == ATTEMPT.parent, "scratch parent is redirected")
    ATTEMPT.mkdir()
    stage = ATTEMPT / "stage"
    stage.mkdir()
    source = stage / "clock_stage.cpp"
    source.write_bytes(variant)
    staged = {stage / "source-costs.h": FOLDER / "source-costs.h",
              ATTEMPT / "waveform-definition-r3/independent_clock.h": FOLDER.parent / "waveform-definition-r3/independent_clock.h"}
    for destination, original in staged.items():
        destination.parent.mkdir(exist_ok=True)
        data = original.read_bytes()
        require(sha(data) == pins[original.resolve()], "staged header changed")
        destination.write_bytes(data)
    obj, deps = ATTEMPT / "entry.obj", ATTEMPT / "dependencies.json"
    argv = [str(compiler), *manifest["compiler"]["arguments"][:10], f"/I{PURPOSE['ROOT'] / 'src'}",
            "/c", str(source), f"/Fo{obj}", "/sourceDependencies", str(deps)]
    env = manifest["environment"] | {"TEMP": str(ATTEMPT), "TMP": str(ATTEMPT)}
    kernel = ctypes.WinDLL("C:/Windows/System32/kernel32.dll")
    mode = kernel.SetErrorMode(0x8003)
    log = ATTEMPT / "compiler.log"
    process = None
    try:
        with log.open("xb") as output:
            process = subprocess.Popen(argv, executable=str(compiler), cwd=ATTEMPT, env=env,
                                       stdin=subprocess.DEVNULL, stdout=output, stderr=output,
                                       close_fds=True, shell=False, creationflags=subprocess.CREATE_NO_WINDOW)
            result = process.wait(timeout=60)
    except BaseException:
        if not retire(process):
            marker = {"status": "QUARANTINED_COMPILER_EXIT_UNOBSERVED", "pid": None if process is None else process.pid}
            with (ATTEMPT / "quarantine.json").open("x", encoding="ascii") as output:
                json.dump(marker, output)
        raise
    finally:
        kernel.SetErrorMode(mode)
    require(result == 0 and log.stat().st_size <= 1048576 and deps.stat().st_size <= 1048576,
            "object-only compile refused or wrote excess diagnostics")
    require(preflight() == state and source.read_bytes() == variant, "source or compiler inputs changed")
    dependency = json.loads(deps.read_bytes())["Data"]
    observed = {Path(name).resolve() for name in dependency["Includes"]}
    mapping = {staged_copy.resolve(): original.resolve() for staged_copy, original in staged.items()}
    require(Path(dependency["Source"]).resolve() == source.resolve() and len(observed) == 110 and mapping.keys() <= observed
            and {mapping.get(path, path) for path in observed} == pins.keys(), "variant include set changed")
    require(all(hash_file(copy) == pins[original] for copy, original in mapping.items()), "variant header changed")
    require(20 <= obj.stat().st_size <= 16 * 1024 * 1024
            and not any(path.suffix.casefold() in {".exe", ".dll"} for path in ATTEMPT.rglob("*")),
            "unexpected object or executable output")
    with obj.open("rb") as stream:
        machine, sections = struct.unpack("<HH", stream.read(4))
    require(machine == 0x8664 and sections > 0, "expected AMD64 object")
    info = {"object_bytes": obj.stat().st_size, "object_sha256": hash_file(obj), "section_count": sections}
    reader = runpy.run_path(str(HERE.parent / "2026-11-14-clock-r22-coff-resource/read-object.py"))
    decoded = reader["read_object"](obj, info)
    require(any("reviewed_@Catalogue@source_costs" in name for name in decoded["object_unresolved_symbols"]),
            "catalogue must be undefined")
    frames = [row for row in decoded["unwind_records"] if row["code"] == "main" or row["code"].startswith("main$")]
    require(any(row["code"] == "main" for row in frames), "missing entry unwind record")
    receipt = {"manifest_sha256": PURPOSE["PIN"], "variant_source_sha256": sha(variant),
               "compiler_sha256": hash_file(compiler), "guard_changes": 6, "include_count": len(pins),
               "raw_exit": result, **info, "unwind_count": len(decoded["unwind_records"]), "entry_frames": frames}
    (ATTEMPT / "entry-frame-result.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    require(sys.argv[1:] in ([], ["--check"]), "expected optional --check")
    state = preflight()
    if sys.argv[1:]:
        print(f"PASS: six-literal variant and {len(state[3])} pinned header identities, no compilation")
    else:
        compile_variant(state)
