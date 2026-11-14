"""Reconstruct and verify the Clock R18 source-only candidate from frozen R17."""

import argparse
import base64
import hashlib
import json
from pathlib import Path
import re
import zlib


HERE = Path(__file__).resolve().parent
RESEARCH = HERE.parent.parent
REPO = RESEARCH.parent.parent
FROZEN = RESEARCH / "artifacts/2026-11-06-clock-owned-supervisor-source-r4/sources"
REVISIONS = Path("build-ci/musical-audio-persistent-planner-certification/source-revisions")
R17 = REVISIONS / "clock-oracle-r17-owned-supervisor-source-r1"
R18 = REVISIONS / "clock-oracle-r18-provenance-source-r1"
MIB = 1024 * 1024


def sha256(data):
    return hashlib.sha256(data).hexdigest().upper()


def describe(path, data):
    return {
        "path": path,
        "bytes": len(data),
        "sha256": sha256(data),
        "lines": data.count(b"\n"),
    }


def json_bytes(value):
    return (json.dumps(value, indent=2, ensure_ascii=True) + "\n").encode("ascii")


def tokens(value):
    if isinstance(value, dict):
        yield value
        for key, child in value.items():
            yield key
            yield from tokens(child)
    elif isinstance(value, list):
        yield value
        for child in value:
            yield from tokens(child)
    else:
        yield value


def container_depth(value):
    if isinstance(value, dict):
        return 1 + max(map(container_depth, value.values()), default=0)
    if isinstance(value, list):
        return 1 + max(map(container_depth, value), default=0)
    return 0


def load_r17():
    captured = {
        "clock_stage.cpp": "069-clock_stage.cpp",
        "run-clock.ps1": "071-run-clock.ps1",
        "source-costs.h": "072-source-costs.h",
        "README.md": "070-README.md",
        "source-manifest.json": "073-source-manifest.json",
    }
    files = {name: (FROZEN / path).read_bytes() for name, path in captured.items()}
    assert sha256(files["source-manifest.json"]) == (
        "3A7F1C7DEE2AAC06568F5AF6C238747094576471A94537B435CA83ADADF21697"
    )
    manifest = json.loads(files["source-manifest.json"])
    for pin in manifest["sources"]:
        assert describe(pin["path"], files[pin["path"]]) == pin

    native_pins = manifest["supervisor"]["native_sources"]
    assert len(native_pins) == 5
    for index, pin in enumerate(native_pins, start=55):
        name = Path(pin["path"]).name
        assert describe(pin["path"], (FROZEN / f"{index:03}-{name}").read_bytes()) == pin
        assert pin in manifest["inputs"]
    return manifest, files


def review_hash():
    handoff = RESEARCH / "checkpoints/2026-11-06-user-test-stop/paused-work.json"
    review_path = "build-ci/20261106-r17-supervisor-functional-independent-review-r1/review.md"
    (row,) = [row for row in json.loads(handoff.read_bytes())["rows"]
              if row["original_path"] == review_path]
    review = zlib.decompress(base64.b64decode(row["data"], validate=True))
    assert len(review) == row["bytes"]
    assert sha256(review) == row["sha256"]
    assert b"## M1: current manifest provenance still names the predecessor" in review
    return row["sha256"]


def derive_r18():
    manifest, files = load_r17()
    predecessor = [
        describe((R17 / name).as_posix(), data) for name, data in files.items()
    ]

    # The manifest schema label is descriptive. The runner and native files
    # still use the inherited JSON structure and supervisor wire protocol v2.
    manifest["schema"] = "YESDAW_CLOCK_ORACLE_R18_PROVENANCE_SOURCE_PROPOSAL_UNBOUND"
    manifest["source_authoring_go"] = "ROOT_R18_SOURCE_ONLY_R17_M1_PROVENANCE_CORRECTION"
    manifest["exact_source_critic"] = "PENDING_FRESH_WHOLE_R18_FUNCTIONAL_REVIEW"
    manifest["source_corrections_correspondence"]["pending"] = (
        "Fresh whole R18 functional critic, source census/native/ABI/profile/resources/"
        "canonical union/binding and separate root one-attempt GO. No prices or limits bound."
    )
    manifest["source_work"]["complete_census"] = (
        "HOLD_PENDING_WHOLE_R18_SOURCE_BODY_CALLER_DEFAULT_COPY_ERROR_PREFIX_AND_MANDATORY_TAIL_PRICES"
    )
    manifest["R18_corrections"] = {
        "predecessor_manifest_sha256": predecessor[-1]["sha256"],
        "unsealed_R17_review_sha256": review_hash(),
        "scope": "M1_CURRENT_MANIFEST_PROVENANCE_AND_DERIVED_PROFILE",
    }
    assert manifest["root_execution_go"] == "ABSENT"
    assert all(value is False for value in manifest["guards"].values())
    assert manifest["cost_catalogue"]["binding_state"] == "HOLD_UNBOUND"
    assert manifest["cost_catalogue"]["definition_files"] == []
    assert manifest["supervisor"]["protocol_version"] == 2
    assert manifest["supervisor"]["bound"] is False
    assert manifest["supervisor"]["binary"]["sha256"] is None

    # Reuse the exact same native source paths. The previous five runner files
    # are distinct historical inputs even when some of their bytes are shared.
    inputs = manifest["inputs"]
    inputs.extend(predecessor)
    assert len({row["path"].casefold() for row in inputs}) == len(inputs)

    profile = manifest["profile"]
    profile["input_count"] = len(inputs)
    profile["input_bytes"] = sum(row["bytes"] for row in inputs)
    profile["input_hash_chunks"] = sum((row["bytes"] + 65535) // 65536 for row in inputs)
    profile["include_input_count"] = len(inputs) + len(manifest["current_include_inputs"])
    profile["complete_current_inputs"] = len(inputs) + len(manifest["sources"]) + 2
    profile["maximum_complete_fence_identities"] = (
        profile["complete_current_inputs"] + profile["declared_success_run_files"]
        + profile["declared_failure_extra_files"]
    )
    profile["profile_source"] = (
        "PROVISIONAL_R18_SOURCE_UNION_ONLY_NATIVE_SDK_BINARY_AND_FINAL_CANONICAL_UNION_UNBOUND"
    )
    manifest["closed_json_domain"]["output_arrays_max_entries"] = profile["complete_current_inputs"]

    retention = manifest["retention"]
    retention["input_bytes"] = profile["input_bytes"]
    retention["reservation_upper"] = (
        retention["input_bytes"] + retention["own5_reserved"] + retention["reference_pool"]
        + retention["compiler_admitted_final_pool"] + retention["stage_pool"]
        + retention["control_pool"] + manifest["supervisor"]["transport_cap"]
        + manifest["supervisor"]["binary"]["maximum_bytes"]
    )
    retention["headroom_under_840MiB"] = retention["cap"] - retention["reservation_upper"]

    files["README.md"] = (HERE / "candidate-readme.md").read_bytes()
    assert files["README.md"].endswith(b"\n") and b"\r" not in files["README.md"]
    nodes = sum(1 for _ in tokens(manifest))
    runner, replacements = re.subn(
        rb"(Assert-TaskRawJsonBytes \$taskBootstrap )16895( 512 4)",
        rb"\g<1>" + str(nodes).encode("ascii") + rb"\g<2>",
        files["run-clock.ps1"],
    )
    assert replacements == 1
    files["run-clock.ps1"] = runner
    manifest["sources"] = [describe(row["path"], files[row["path"]])
                           for row in manifest["sources"]]
    assert sum(1 for _ in tokens(manifest)) == nodes
    depth = container_depth(manifest)
    string_units = max(len(token.encode("utf-16-le")) // 2
                       for token in tokens(manifest) if isinstance(token, str))
    assert depth <= 4 and string_units <= 512
    files["source-manifest.json"] = json_bytes(manifest)
    own_bytes = sum(map(len, files.values()))
    assert own_bytes <= MIB

    receipt = {
        "revision": R18.as_posix(),
        "predecessor_manifest_sha256": predecessor[-1]["sha256"],
        "unsealed_R17_review_sha256": manifest["R18_corrections"]["unsealed_R17_review_sha256"],
        "sources": [describe(name, data) for name, data in files.items()],
        "bootstrap": {"nodes": nodes, "container_depth": depth, "string_units": string_units},
        "input_count": len(inputs),
        "input_bytes": profile["input_bytes"],
        "input_hash_chunks": profile["input_hash_chunks"],
        "own5_bytes": own_bytes,
        "complete_current_inputs": profile["complete_current_inputs"],
        "maximum_complete_fence_identities": profile["maximum_complete_fence_identities"],
        "reservation_upper": retention["reservation_upper"],
    }
    return files, receipt


def main():
    if not __debug__:
        raise SystemExit("Run without -O so the source checks execute")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--materialize", action="store_true", help="Write the verified source-only files")
    args = parser.parse_args()

    files, receipt = derive_r18()
    assert json.loads((HERE / "receipt.json").read_bytes()) == receipt, "R18 receipt differs"
    if args.materialize:
        output = REPO / R18
        output.mkdir(parents=True, exist_ok=True)
        for name, data in files.items():
            path = output / name
            if path.exists():
                assert path.read_bytes() == data, f"Existing revision differs: {path}"
            else:
                with path.open("xb") as destination:
                    destination.write(data)
    print(f"PASS: Clock R18 source-only receipt, {receipt['bootstrap']['nodes']} JSON nodes")


if __name__ == "__main__":
    main()
