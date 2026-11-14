"""Derive the unrun R20 native path copy fix from the reviewed local R19."""

import json
from pathlib import Path
import runpy


HERE = Path(__file__).resolve().parent
R19 = runpy.run_path(str(HERE.parent / "2026-11-14-clock-r19-local-profile/candidate.py"))
R18 = R19["R18"]
ROOT = R19["ROOT"]
SOURCE = R19["R19_PATH"]
DEST = SOURCE.with_name("clock-oracle-r20-path-copy-source-r1")
NATIVE = Path("build-ci/20261114-clock-r20-native")
describe, require, serialize, tokens = (R19[n] for n in ("describe", "require", "serialize", "tokens"))
OLD = b"    Wide current = path;\n"
NEW = b"""    Wide current;
    current.length = path.length;
    for (U32 i = 0; i <= current.length; ++i)
        current.cells[i] = path.cells[i];
"""


def prepare():
    files = {name: (ROOT / SOURCE / name).read_bytes() for name in
             ("clock_stage.cpp", "run-clock.ps1", "source-costs.h", "README.md", "source-manifest.json")}
    manifest = json.loads(files["source-manifest.json"])
    prior_manifest = "2B82636B3F3A51B27FC8D9FE229EDB190A889778110338143679F7870190EEAE"
    require(R18["sha256"](files["source-manifest.json"]) == prior_manifest, "R19 manifest differs")
    for row in manifest["sources"]:
        require(describe(row["path"], files[row["path"]]) == row, "R19 source differs")
    native = manifest["supervisor"]["native_sources"]
    (path_pin,) = [row for row in native if row["path"].endswith("/sup_io.cpp")]
    old = (ROOT / path_pin["path"]).read_bytes()
    require(describe(path_pin["path"], old) == path_pin and old.count(OLD) == 1, "R19 native source differs")
    require(b"path.length > path_units || path.cells[path.length] != 0" in old[:old.index(OLD)],
            "path copy lacks its extent and terminator guard")
    patched = old.replace(OLD, NEW)
    new_pin = describe((NATIVE / "sup_io.cpp").as_posix(), patched)
    native[native.index(path_pin)] = new_pin
    manifest["inputs"].extend(describe((SOURCE / name).as_posix(), data) for name, data in files.items())
    manifest["inputs"].append(new_pin)
    manifest["closed_directory_inputs"].append(
        {"path": NATIVE.as_posix(), "recursive": False, "files": ["sup_io.cpp"], "entry_count": 1}
    )
    require(len({row["path"].casefold() for row in manifest["inputs"]}) == len(manifest["inputs"]),
            "duplicate input identity")
    manifest["schema"] = "YESDAW_CLOCK_ORACLE_R20_PATH_COPY_SOURCE_PROPOSAL_UNBOUND"
    manifest["source_authoring_go"] = "ROOT_R20_SOURCE_ONLY_BOUNDED_WIDE_PATH_COPY"
    manifest["exact_source_critic"] = "PENDING_FRESH_WHOLE_R20_FUNCTIONAL_REVIEW"
    manifest["source_corrections_correspondence"]["pending"] = (
        "Fresh whole R20 functional review, source census, native profile, resource and binding proof."
    )
    manifest["source_work"]["complete_census"] = "HOLD_PENDING_WHOLE_R20_SOURCE_CENSUS_AND_TAIL_PRICES"
    manifest["supervisor"]["ticket_prices"] = "HOLD_PENDING_R20_PATH_COPY_AND_FULL_NATIVE_CENSUS"
    manifest["R20_corrections"] = {"R19_manifest_sha256": prior_manifest,
                                     "scope": "COPY_ONLY_VALIDATED_WIDE_PATH_PREFIX_AND_NUL"}

    profile = manifest["profile"]
    inputs = manifest["inputs"]
    added = sum(row["bytes"] for row in inputs) - profile["input_bytes"]
    profile["input_count"] = len(inputs)
    profile["input_bytes"] += added
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
    profile["profile_source"] = "PROVISIONAL_R20_PATH_COPY_SOURCE_UNION_NATIVE_BINDING_UNBOUND"
    manifest["closed_json_domain"]["output_arrays_max_entries"] = complete
    retained = manifest["retention"]
    retained["reservation_upper"] += added
    retained["input_bytes"] += added
    retained["headroom_under_840MiB"] = retained["cap"] - retained["reservation_upper"]

    nodes = sum(1 for _ in tokens(manifest))
    old_limit = b"Assert-TaskRawJsonBytes $taskBootstrap 16999 512 4"
    require(files["run-clock.ps1"].count(old_limit) == 1, "unexpected R19 bootstrap count")
    files["run-clock.ps1"] = files["run-clock.ps1"].replace(
        old_limit, f"Assert-TaskRawJsonBytes $taskBootstrap {nodes} 512 4".encode()
    )
    files["README.md"] = (HERE / "candidate-readme.md").read_bytes()
    manifest["sources"] = [describe(row["path"], files[row["path"]]) for row in manifest["sources"]]
    require(sum(1 for _ in tokens(manifest)) == nodes and R18["container_depth"](manifest) <= 4,
            "unexpected manifest geometry")
    require(max(len(s.encode("utf-16-le")) // 2 for s in tokens(manifest) if isinstance(s, str)) <= 512,
            "manifest string exceeds limit")
    require(all(value is False for value in manifest["guards"].values())
            and manifest["root_execution_go"] == "ABSENT", "unexpected execution admission")
    files["source-manifest.json"] = serialize(manifest)
    require(sum(map(len, files.values())) <= R18["MIB"], "source file pack exceeds 1 MiB")
    return {(DEST / name): data for name, data in files.items()} | {NATIVE / "sup_io.cpp": patched}


if __name__ == "__main__":
    files = prepare()
    for relative, data in files.items():
        path = ROOT / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists():
            require(path.read_bytes() == data, f"existing candidate differs: {path}")
        else:
            path.write_bytes(data)
        print(R18["sha256"](data), relative)
