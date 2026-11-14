"""Materialize an unrun Clock R19 candidate using the verified local tool paths."""

import argparse
from pathlib import Path
import re
import runpy
import sys


ARTIFACTS = Path(__file__).resolve().parents[1]
RECOVERY = ARTIFACTS / "2026-11-14-clock-controller-recovery"
R18 = runpy.run_path(str(ARTIFACTS / "2026-11-14-clock-r18-provenance/candidate.py"))
R18_PATH = R18["R18"]
R19_PATH = R18_PATH.with_name("clock-oracle-r19-local-profile-source-r1")
ROOT = R18["REPO"]
describe, serialize, tokens = (R18[name] for name in ("describe", "json_bytes", "tokens"))
sys.path.insert(0, str(RECOVERY))
inventory = runpy.run_path(str(RECOVERY / "verify-inputs.py"))["inventory"]
require = inventory.__globals__["require"]


def path_text(path):
    value = path.as_posix()
    require(value.isascii() and "'" not in value and len(value) <= 512
            and all(32 <= ord(char) <= 126 for char in value), f"path outside Clock domain: {value}")
    return value


def prepare(controller, compiler):
    python = Path(sys.executable).resolve(strict=True)
    manifest, files, directories, moved = inventory(controller, compiler, python)
    require(moved == 415, "local candidate expects the three relocated tool paths")
    base = ROOT / R18_PATH
    (expected_files, _) = R18["derive_r18"]()
    previous = {name: (base / name).read_bytes() for name in expected_files}
    require(previous == expected_files, "R18 materialized source differs")

    old_controller = Path(manifest["controller"]["path"]).parent
    old_compiler = Path(manifest["compiler"]["path"]).parent
    for row in manifest["inputs"]:
        path = Path(row["path"])
        if path.is_absolute() and path != files[path]:
            row["path"] = path_text(files[path])
    for row in manifest["closed_directory_inputs"]:
        path = Path(row["path"])
        if path.is_absolute() and path != directories[path]:
            row["path"] = path_text(directories[path])
    manifest["inputs"].extend(describe((R18_PATH / name).as_posix(), data)
                              for name, data in previous.items())
    require(len({row["path"].casefold() for row in manifest["inputs"]}) == len(manifest["inputs"]),
            "duplicate input identity")
    manifest["controller"]["path"] = path_text(controller / "pwsh.exe")
    manifest["controller"]["modules"] = [
        path_text(controller / Path(path).relative_to(old_controller))
        for path in manifest["controller"]["modules"]
    ]
    manifest["compiler"]["path"] = path_text(compiler / "cl.exe")
    old_path = manifest["environment"]["PATH"]
    require(old_path.startswith(old_compiler.as_posix() + ";"), "unexpected compiler PATH")
    manifest["environment"]["PATH"] = path_text(compiler) + old_path[len(old_compiler.as_posix()):]
    manifest["schema"] = "YESDAW_CLOCK_ORACLE_R19_LOCAL_TOOL_PATH_SOURCE_PROPOSAL_UNBOUND"
    manifest["source_authoring_go"] = "ROOT_R19_SOURCE_ONLY_LOCAL_TOOL_RELOCATION"
    manifest["exact_source_critic"] = "PENDING_FRESH_WHOLE_R19_FUNCTIONAL_REVIEW"
    manifest["source_corrections_correspondence"]["pending"] = (
        "Fresh whole R19 source review, native profile, census, resources, binary binding and separate GO."
    )
    manifest["source_work"]["complete_census"] = "HOLD_PENDING_WHOLE_R19_SOURCE_CENSUS_AND_TAIL_PRICES"
    manifest["R19_corrections"] = {"R18_manifest_sha256": R18["sha256"](previous["source-manifest.json"]),
                                 "scope": "VERIFIED_TOOL_BYTES_RELOCATED_NATIVE_PROFILE_UNBOUND"}

    profile = manifest["profile"]
    inputs = manifest["inputs"]
    profile["input_count"] = len(inputs)
    profile["input_bytes"] = sum(row["bytes"] for row in inputs)
    profile["input_hash_chunks"] = sum((row["bytes"] + 65535) // 65536 for row in inputs)
    profile["include_input_count"] = len(inputs) + len(manifest["current_include_inputs"])
    complete = len(inputs) + len(manifest["sources"]) + 2
    profile["complete_current_inputs"] = complete
    profile["maximum_complete_fence_identities"] = (
        complete + profile["declared_success_run_files"] + profile["declared_failure_extra_files"]
    )
    profile["profile_source"] = "PROVISIONAL_R19_RELOCATED_SOURCE_UNION_NATIVE_BINDING_UNBOUND"
    manifest["closed_json_domain"]["output_arrays_max_entries"] = complete
    retained = manifest["retention"]
    retained["reservation_upper"] += profile["input_bytes"] - retained["input_bytes"]
    retained["input_bytes"] = profile["input_bytes"]
    retained["headroom_under_840MiB"] = retained["cap"] - retained["reservation_upper"]

    nodes = sum(1 for _ in tokens(manifest))
    runner = previous["run-clock.ps1"]
    old_line = f"$taskRuntime='{str(old_controller)}'".encode("ascii")
    new_line = f"$taskRuntime='{str(controller)}'".encode("ascii")
    require(runner.count(old_line) == 1, "unexpected runtime path literal")
    runner = runner.replace(old_line, new_line)
    old_limit = b"Assert-TaskRawJsonBytes $taskBootstrap 16948 512 4"
    require(runner.count(old_limit) == 1, "unexpected bootstrap node limit")
    runner = runner.replace(old_limit, f"Assert-TaskRawJsonBytes $taskBootstrap {nodes} 512 4".encode("ascii"))
    files = {**previous, "run-clock.ps1": runner, "README.md": (Path(__file__).parent / "candidate-readme.md").read_bytes()}
    manifest["sources"] = [describe(row["path"], files[row["path"]]) for row in manifest["sources"]]
    require(sum(1 for _ in tokens(manifest)) == nodes, "bootstrap node drift")
    require(R18["container_depth"](manifest) <= 4, "bootstrap depth overflow")
    require(max(len(s.encode("utf-16-le")) // 2 for s in tokens(manifest) if isinstance(s, str)) <= 512,
            "bootstrap string overflow")
    require(all(value is False for value in manifest["guards"].values())
            and manifest["root_execution_go"] == "ABSENT", "unexpected execution admission")
    files["source-manifest.json"] = serialize(manifest)
    require(sum(map(len, files.values())) <= R18["MIB"], "source file pack exceeds 1 MiB")
    return files, nodes


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("controller", type=Path)
    parser.add_argument("compiler", type=Path)
    args = parser.parse_args()
    files, nodes = prepare(args.controller.resolve(strict=True), args.compiler.resolve(strict=True))
    output = ROOT / R19_PATH
    output.mkdir(exist_ok=True)
    for name, data in files.items():
        path = output / name
        if path.exists():
            require(path.read_bytes() == data, f"existing candidate differs: {path}")
        else:
            path.write_bytes(data)
    print(serialize({"bootstrap_nodes": nodes, "sources": [describe(n, data) for n, data in files.items()]}).decode())
