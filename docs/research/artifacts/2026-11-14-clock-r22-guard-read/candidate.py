"""Derive an unrun R22 with an explicit admission read."""

import json
from pathlib import Path
import runpy


HERE = Path(__file__).resolve().parent
R21 = runpy.run_path(str(HERE.parent / "2026-11-14-clock-r21-constant-guard/candidate.py"))
ROOT = R21["ROOT"]
SOURCE = R21["DEST"]
DEST = SOURCE.with_name("clock-oracle-r22-guard-read-source-r1")
NATIVE = Path("build-ci/20261114-clock-r22-native")
describe, require, serialize, tokens = (R21[name] for name in ("describe", "require", "serialize", "tokens"))
sha256 = R21["R20"]["R18"]["sha256"]


def prepare():
    files = {name: (ROOT / SOURCE / name).read_bytes() for name in
             ("clock_stage.cpp", "run-clock.ps1", "source-costs.h", "README.md", "source-manifest.json")}
    manifest = json.loads(files["source-manifest.json"])
    prior = "EF1E775617A04D73D97D0E950557D626846E9172677BC897BDAB225FEB71EBB8"
    require(sha256(files["source-manifest.json"]) == prior, "R21 manifest differs")
    for row in manifest["sources"]:
        require(describe(row["path"], files[row["path"]]) == row, "R21 source differs")
    native = manifest["supervisor"]["native_sources"]
    old_pin, = [row for row in native if row["path"].endswith("/sup_main.cpp")]
    original = (ROOT / old_pin["path"]).read_bytes()
    guard = (
        b"    if constexpr (!source_review_accepted || !source_work_accepted || !native_profile_accepted ||\n"
        b"        !controlled_resource_accepted || !execution_accepted) finish(state,10);"
    )
    replacement = (
        b"    const volatile bool admitted = source_review_accepted && source_work_accepted && native_profile_accepted &&\n"
        b"        controlled_resource_accepted && execution_accepted;\n"
        b"    if (!admitted) finish(state,10);"
    )
    require(describe(old_pin["path"], original) == old_pin and original.count(guard) == 1,
            "R21 guard source differs")
    revised = original.replace(guard, replacement)
    new_pin = describe((NATIVE / "sup_main.cpp").as_posix(), revised)
    native[native.index(old_pin)] = new_pin
    manifest["inputs"].extend(describe((SOURCE / name).as_posix(), data) for name, data in files.items())
    manifest["inputs"].append(new_pin)
    manifest["closed_directory_inputs"].append(
        {"path": NATIVE.as_posix(), "recursive": False, "files": ["sup_main.cpp"], "entry_count": 1}
    )
    require(len({row["path"].casefold() for row in manifest["inputs"]}) == len(manifest["inputs"]),
            "duplicate source path")

    manifest["schema"] = "YESDAW_CLOCK_ORACLE_R22_ADMISSION_READ_SOURCE_PROPOSAL_UNBOUND"
    manifest["source_authoring_go"] = "ROOT_R22_SOURCE_ONLY_EXPLICIT_ADMISSION_READ"
    manifest["exact_source_critic"] = "PENDING_FRESH_WHOLE_R22_FUNCTIONAL_REVIEW"
    manifest["source_corrections_correspondence"]["pending"] = (
        "Fresh whole R22 functional review, source census, native profile, resource and binding proof."
    )
    manifest["source_work"]["complete_census"] = "HOLD_PENDING_WHOLE_R22_SOURCE_CENSUS_AND_TAIL_PRICES"
    manifest["supervisor"]["ticket_prices"] = "HOLD_PENDING_R22_FULL_NATIVE_CENSUS"
    manifest["R22_corrections"] = {"R21_manifest_sha256": prior,
                                     "scope": "EXPLICIT_ADMISSION_READ_WITH_ALL_EXISTING_CONSTANTS_FALSE"}
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
    profile["profile_source"] = "PROVISIONAL_R22_ADMISSION_READ_SOURCE_UNION_NATIVE_BINDING_UNBOUND"
    manifest["closed_json_domain"]["output_arrays_max_entries"] = complete
    retained = manifest["retention"]
    retained["reservation_upper"] += additional
    retained["input_bytes"] += additional
    retained["headroom_under_840MiB"] = retained["cap"] - retained["reservation_upper"]

    nodes = sum(1 for _ in tokens(manifest))
    old_limit = b"Assert-TaskRawJsonBytes $taskBootstrap 17139 512 4"
    require(files["run-clock.ps1"].count(old_limit) == 1, "unexpected R21 bootstrap count")
    files["run-clock.ps1"] = files["run-clock.ps1"].replace(
        old_limit, f"Assert-TaskRawJsonBytes $taskBootstrap {nodes} 512 4".encode()
    )
    files["README.md"] = (HERE / "candidate-readme.md").read_bytes()
    manifest["sources"] = [describe(row["path"], files[row["path"]]) for row in manifest["sources"]]
    require(sum(1 for _ in tokens(manifest)) == nodes and R21["R20"]["R18"]["container_depth"](manifest) <= 4,
            "unexpected manifest geometry")
    require(max(len(s.encode("utf-16-le")) // 2 for s in tokens(manifest) if isinstance(s, str)) <= 512,
            "manifest string exceeds limit")
    require(all(value is False for value in manifest["guards"].values())
            and manifest["root_execution_go"] == "ABSENT", "unexpected execution admission")
    files["source-manifest.json"] = serialize(manifest)
    require(sum(map(len, files.values())) <= R21["R20"]["R18"]["MIB"], "source file pack exceeds 1 MiB")
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
