"""Derive an unrun R21 with the supervisor's compile-time admission guard."""

import json
from pathlib import Path
import runpy


HERE = Path(__file__).resolve().parent
R20 = runpy.run_path(str(HERE.parent / "2026-11-14-clock-r20-path-copy/candidate.py"))
ROOT = R20["ROOT"]
SOURCE = R20["DEST"]
DEST = SOURCE.with_name("clock-oracle-r21-constant-guard-source-r1")
NATIVE = Path("build-ci/20261114-clock-r21-native")
describe, require, serialize, tokens = (R20[name] for name in ("describe", "require", "serialize", "tokens"))
sha256 = R20["R18"]["sha256"]


def prepare():
    files = {name: (ROOT / SOURCE / name).read_bytes() for name in
             ("clock_stage.cpp", "run-clock.ps1", "source-costs.h", "README.md", "source-manifest.json")}
    manifest = json.loads(files["source-manifest.json"])
    prior = "69A5E104B0FE69B898D14276A15CC1643AEE56658B779417D919B2769EDC4D56"
    require(sha256(files["source-manifest.json"]) == prior, "R20 manifest differs")
    for row in manifest["sources"]:
        require(describe(row["path"], files[row["path"]]) == row, "R20 source differs")
    native = manifest["supervisor"]["native_sources"]
    old_pin, = [row for row in native if row["path"].endswith("/sup_main.cpp")]
    original = (ROOT / old_pin["path"]).read_bytes()
    guard = b"    if (!source_review_accepted || !source_work_accepted || !native_profile_accepted ||"
    require(describe(old_pin["path"], original) == old_pin and original.count(guard) == 1,
            "R20 guard source differs")
    revised = original.replace(guard, guard.replace(b"if (", b"if constexpr ("))
    new_pin = describe((NATIVE / "sup_main.cpp").as_posix(), revised)
    native[native.index(old_pin)] = new_pin
    manifest["inputs"].extend(describe((SOURCE / name).as_posix(), data) for name, data in files.items())
    manifest["inputs"].append(new_pin)
    manifest["closed_directory_inputs"].append(
        {"path": NATIVE.as_posix(), "recursive": False, "files": ["sup_main.cpp"], "entry_count": 1}
    )
    require(len({row["path"].casefold() for row in manifest["inputs"]}) == len(manifest["inputs"]),
            "duplicate source path")

    manifest["schema"] = "YESDAW_CLOCK_ORACLE_R21_CONSTEXPR_GUARD_SOURCE_PROPOSAL_UNBOUND"
    manifest["source_authoring_go"] = "ROOT_R21_SOURCE_ONLY_CONSTEXPR_ADMISSION_GUARD"
    manifest["exact_source_critic"] = "PENDING_FRESH_WHOLE_R21_FUNCTIONAL_REVIEW"
    manifest["source_corrections_correspondence"]["pending"] = (
        "Fresh whole R21 functional review, source census, native profile, resource and binding proof."
    )
    manifest["source_work"]["complete_census"] = "HOLD_PENDING_WHOLE_R21_SOURCE_CENSUS_AND_TAIL_PRICES"
    manifest["supervisor"]["ticket_prices"] = "HOLD_PENDING_R21_FULL_NATIVE_CENSUS"
    manifest["R21_corrections"] = {"R20_manifest_sha256": prior,
                                     "scope": "EXPLICIT_CONSTEXPR_WITH_ALL_EXISTING_GUARD_CONSTANTS_FALSE"}
    inputs = manifest["inputs"]
    profile = manifest["profile"]
    additional = sum(row["bytes"] for row in inputs) - profile["input_bytes"]
    profile["input_count"] = len(inputs)
    profile["input_bytes"] += additional
    profile["input_hash_chunks"] = sum((row["bytes"] + 65535) // 65536 for row in inputs)
    profile["include_input_count"] = len(inputs) + len(manifest["current_include_inputs"])
    complete = len(inputs) + len(manifest["sources"]) + 2
    profile["complete_current_inputs"] = complete
    profile["maximum_complete_fence_identities"] = (
        complete + profile["declared_success_run_files"] + profile["declared_failure_extra_files"]
    )
    profile["directory_namespaces_count"] += 1
    profile["directory_namespace_entries"] += 1
    profile["directory_namespace_files"] += 1
    profile["profile_source"] = "PROVISIONAL_R21_CONSTEXPR_SOURCE_UNION_NATIVE_BINDING_UNBOUND"
    manifest["closed_json_domain"]["output_arrays_max_entries"] = complete
    retained = manifest["retention"]
    retained["reservation_upper"] += additional
    retained["input_bytes"] += additional
    retained["headroom_under_840MiB"] = retained["cap"] - retained["reservation_upper"]

    nodes = sum(1 for _ in tokens(manifest))
    old_limit = b"Assert-TaskRawJsonBytes $taskBootstrap 17069 512 4"
    require(files["run-clock.ps1"].count(old_limit) == 1, "unexpected R20 bootstrap count")
    files["run-clock.ps1"] = files["run-clock.ps1"].replace(
        old_limit, f"Assert-TaskRawJsonBytes $taskBootstrap {nodes} 512 4".encode()
    )
    files["README.md"] = (HERE / "candidate-readme.md").read_bytes()
    manifest["sources"] = [describe(row["path"], files[row["path"]]) for row in manifest["sources"]]
    require(sum(1 for _ in tokens(manifest)) == nodes and R20["R18"]["container_depth"](manifest) <= 4,
            "unexpected manifest geometry")
    require(max(len(s.encode("utf-16-le")) // 2 for s in tokens(manifest) if isinstance(s, str)) <= 512,
            "manifest string exceeds limit")
    require(all(value is False for value in manifest["guards"].values())
            and manifest["root_execution_go"] == "ABSENT", "unexpected execution admission")
    files["source-manifest.json"] = serialize(manifest)
    require(sum(map(len, files.values())) <= R20["R18"]["MIB"], "source file pack exceeds 1 MiB")
    return {(DEST / name): data for name, data in files.items()} | {NATIVE / "sup_main.cpp": revised}


if __name__ == "__main__":
    for relative, data in prepare().items():
        path = ROOT / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists():
            require(path.read_bytes() == data, f"existing candidate differs: {path}")
        else:
            path.write_bytes(data)
        print(sha256(data), relative)
