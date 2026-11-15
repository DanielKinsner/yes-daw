"""Compile one admission-enabled supervisor translation unit to AMD64 COFF."""

import argparse
import ctypes
import json
from pathlib import Path
import re
import runpy
import struct
import subprocess


HERE = Path(__file__).resolve().parent
BASE = runpy.run_path(str(HERE / "compile-objects.py"))
TARGET = runpy.run_path(str(HERE.parent / "2026-11-14-clock-r22-child-object/entry-frame-purpose.py"))
PROFILE = BASE["PURPOSE"]
digest, load, require = (BASE[name] for name in ("digest", "load", "require"))
TEMP = Path("C:/Users/Daniel Kinsner/AppData/Local/Temp")
ATTEMPT = TEMP / "YesDawClockR22SupervisorEntryFrame-attempt1"
OLD_DEPS = TEMP / "YesDawClockSupervisorObjects-r22-attempt1/sup_main.deps.json"
OLD_RECEIPT_SHA = "D481911794898BCD644B0353A428671092C9CC4D708BCFBD540B96D6C8EF8032"
NAMES = (
    b"source_review_accepted", b"source_work_accepted", b"native_profile_accepted",
    b"controlled_resource_accepted", b"execution_accepted",
)
GUARDS = re.compile(rb"(?m)^inline constexpr bool (" + b"|".join(NAMES) + rb") = (false|true);$")
FLAGS = (
    "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", "/MD", "/permissive-",
    "/fp:strict", "/O2", "/D_ITERATOR_DEBUG_LEVEL=0",
)


def includes(record, source):
    data = record["Data"]
    require(Path(data["Source"]).resolve(strict=True) == source and not data["ProvidedModule"]
            and not data["ImportedModules"] and not data["ImportedHeaderUnits"], "dependency root or module changed")
    paths = tuple(Path(item).resolve(strict=True) for item in data["Includes"])
    require(len(paths) == 129 and len(set(paths)) == 103, "unexpected sup_main include extent")
    return paths


def preflight():
    folder, pin = BASE["MANIFESTS"]["r22"]
    manifest_raw = (BASE["REVISIONS"] / folder / "source-manifest.json").read_bytes()
    require(digest(manifest_raw) == pin, "R22 manifest changed")
    manifest = json.loads(manifest_raw)
    require(not any(manifest["guards"].values()) and manifest["root_execution_go"] == "ABSENT",
            "staged authority changed")
    sources = {(BASE["ROOT"] / row["path"]).resolve(strict=True): row["sha256"]
               for row in manifest["supervisor"]["native_sources"]}
    expected = {"sup.h", "sup_codec.cpp", "sup_core.cpp", "sup_io.cpp", "sup_main.cpp"}
    require({path.name for path in sources} == expected and len(sources) == len(expected),
            "unexpected R22 supervisor input set")
    source, = (path for path in sources if path.name == "sup_main.cpp")
    header, = (path for path in sources if path.name == "sup.h")
    source_bytes, original = source.read_bytes(), header.read_bytes()
    require(digest(source_bytes) == sources[source] and digest(original) == sources[header],
            "original supervisor input changed")
    matches = [(hit[1], hit[2]) for hit in GUARDS.finditer(original)]
    require(matches == [(name, b"false") for name in NAMES] and b"extern const Catalogue Data;" in original,
            "original supervisor guard or declaration changed")
    variant = GUARDS.sub(rb"inline constexpr bool \g<1> = true;", original)
    require(len(variant) == len(original) - len(NAMES)
            and GUARDS.sub(rb"inline constexpr bool \g<1> = false;", variant) == original,
            "header changed outside the five guard literals")

    raw_receipt = (HERE / "r22-object-result.json").read_bytes()
    require(digest(raw_receipt) == OLD_RECEIPT_SHA, "guarded receipt changed")
    receipt = json.loads(raw_receipt)
    entry = receipt["objects"]["sup_main.cpp"]
    require(receipt["manifest_sha256"] == pin and receipt["supervisor_header_sha256"] == sources[header]
            and receipt["flags"] == [*FLAGS, "/I<sup.h-root>", "/c"]
            and entry["source_sha256"] == sources[source] and entry["include_count"] == 103,
            "guarded entry receipt differs")
    deps = OLD_DEPS.read_bytes()
    require(digest(deps) == entry["dependency_report_sha256"], "guarded sup_main dependency report changed")
    paths = includes(json.loads(deps), source)
    listed = {Path(name).resolve(strict=True): sha for name, sha in receipt["include_hashes"].items()}
    require(paths[0] == header and paths.count(header) == 1 and set(paths) <= listed.keys()
            and all(digest(path.read_bytes()) == listed[path] for path in paths), "guarded sup_main include pin changed")
    compiler = Path(manifest["compiler"]["path"]).resolve(strict=True)
    compiler_sha = digest(compiler.read_bytes())
    require(compiler_sha == manifest["compiler"]["sha256"] == receipt["compiler_sha256"], "compiler changed")
    tools = PROFILE["verify_inputs"](compiler.parent)
    return pin, header, source_bytes, variant, paths, compiler, compiler_sha, tools


def compile_variant(state):
    pin, header, source_bytes, variant, old_paths, compiler, compiler_sha, _ = state
    require(ATTEMPT.parent.resolve(strict=True) == ATTEMPT.parent, "scratch parent is redirected")
    ATTEMPT.mkdir()
    stage = ATTEMPT / "source"
    stage.mkdir()
    source, staged_header = stage / "sup_main.cpp", stage / "sup.h"
    source.write_bytes(source_bytes)
    staged_header.write_bytes(variant)
    obj, deps = ATTEMPT / "entry.obj", ATTEMPT / "entry.deps.json"
    args = [str(compiler), *FLAGS, f"/I{stage}", "/c", str(source), f"/Fo{obj}", "/sourceDependencies", str(deps)]
    profile = load(PROFILE["PROFILE"])
    environment = profile["environment"] | {
        "PATH": f"{compiler.parent};{profile['environment']['SystemRoot']}/System32",
        "TEMP": str(ATTEMPT), "TMP": str(ATTEMPT),
        "INCLUDE": ";".join([profile["environment"]["INCLUDE"], *map(str, PROFILE["WIN32_ROOTS"])]),
    }
    set_error_mode = ctypes.WinDLL("C:/Windows/System32/kernel32.dll").SetErrorMode
    set_error_mode.argtypes = (ctypes.c_uint,)
    set_error_mode.restype = ctypes.c_uint
    mode = set_error_mode(0x8003)
    process = None
    log = ATTEMPT / "compiler.log"
    try:
        with log.open("xb") as output:
            process = subprocess.Popen(args, executable=str(compiler), cwd=ATTEMPT, env=environment,
                                       stdin=subprocess.DEVNULL, stdout=output, stderr=output,
                                       close_fds=True, shell=False, creationflags=subprocess.CREATE_NO_WINDOW)
            result = process.wait(timeout=60)
    except BaseException:
        if not TARGET["retire"](process):
            marker = {"status": "QUARANTINED_COMPILER_EXIT_UNOBSERVED", "pid": None if process is None else process.pid}
            with (ATTEMPT / "quarantine.json").open("x", encoding="ascii") as output:
                json.dump(marker, output)
        raise
    finally:
        set_error_mode(mode)
    require(result == 0 and log.stat().st_size <= 1048576 and deps.stat().st_size <= 1048576,
            "compile refused or recorded excess diagnostics")
    require(preflight() == state and source.read_bytes() == source_bytes and staged_header.read_bytes() == variant,
            "compile inputs changed")
    actual = includes(load(deps), source.resolve(strict=True))
    require(actual[0] == staged_header.resolve(strict=True) and (header, *actual[1:]) == old_paths,
            "variant dependency order or identities changed")
    require(20 <= obj.stat().st_size <= 16 * 1024 * 1024
            and not any(path.suffix.casefold() in {".exe", ".dll"} for path in ATTEMPT.rglob("*")),
            "object extent or output kinds differ")
    raw = obj.read_bytes()
    machine, sections, _, symbols, entries, optional, _ = struct.unpack_from("<HHIIIHH", raw)
    require(machine == 0x8664 and sections > 0 and optional == 0 and 20 + sections * 40 <= symbols <= len(raw)
            and entries <= (len(raw) - symbols) // 18, "object is not normal AMD64 COFF")
    receipt = {"manifest_sha256": pin, "source_sha256": digest(source_bytes), "variant_header_sha256": digest(variant),
               "compiler_sha256": compiler_sha, "guard_changes": len(NAMES),
               "include_count": len(set(actual)), "include_visits": len(actual),
               "guarded_dependency_sha256": digest(OLD_DEPS.read_bytes()),
               "variant_dependency_sha256": digest(deps.read_bytes()), "raw_exit": result,
               "object_bytes": len(raw), "object_sha256": digest(raw), "section_count": sections}
    with (ATTEMPT / "entry-result.json").open("x", encoding="ascii", newline="\n") as output:
        json.dump(receipt, output, indent=2)
        output.write("\n")
    print(json.dumps(receipt, indent=2))


def read_frame(state, observed_sha):
    pin, _, source_bytes, variant, old_paths, _, compiler_sha, _ = state
    receipt = load(ATTEMPT / "entry-result.json")
    require(receipt["manifest_sha256"] == pin and receipt["source_sha256"] == digest(source_bytes)
            and receipt["variant_header_sha256"] == digest(variant) and receipt["compiler_sha256"] == compiler_sha
            and receipt["include_count"] == len(set(old_paths)) and receipt["include_visits"] == len(old_paths)
            and receipt["guard_changes"] == len(NAMES)
            and receipt["raw_exit"] == 0 and receipt["object_sha256"] == observed_sha.upper(),
            "unassociated entry receipt")
    reader = runpy.run_path(str(HERE.parent / "2026-11-14-clock-r22-coff-resource/read-object.py"))
    names = ("wmain", "?supervisor_main@yesdaw_clock_supervisor_r4")
    result = reader["read_object"](ATTEMPT / "entry.obj", receipt, code_names=names)
    code = result["selected_code"]
    entry = code[names[1]]
    require(any(label == entry["symbol"] for _, label in code[names[0]]["direct_relative_calls"])
            and any(label.startswith("?validate_profile@") for _, label in entry["direct_relative_calls"]),
            "expected wmain and admitted profile call edges")
    selected = {"$unwind$" + code[name]["symbol"] for name in names}
    frames = [row for row in result["unwind_records"] if row["unwind_symbol"] in selected and not row["chained"]]
    require(len(frames) == len(names), "missing unique entry unwind record")
    print(json.dumps({"object_sha256": receipt["object_sha256"], "entry_frames": frames, "entry_code": code}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--compile", action="store_true", help="one separately reviewed compile-only attempt")
    modes.add_argument("--read", metavar="SHA256", help="read an existing explicitly identified COFF artifact")
    options = parser.parse_args()
    verified = preflight()
    if options.compile:
        compile_variant(verified)
    elif options.read:
        read_frame(verified, options.read)
    else:
        print("PASS: reversible five-literal header, 129 original include visits and 103 distinct pins, no compile")
