"""Rebuild R22 shared controller call envelopes from pinned source and manifest."""

from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import runpy


HERE = Path(__file__).resolve().parent
DELTA = runpy.run_path(str(HERE / "check.py"))
REQUIRE = DELTA["require"]


@dataclass(frozen=True)
class Walk:
    calls: int
    rows: int
    directories: int

    @property
    def candidates(self):
        return self.rows + self.calls


def bounded_walk(calls, maximum, recursive):
    return Walk(calls, calls * maximum, calls * (maximum + 1 if recursive else 1))


def source_sites(lines, name):
    pattern = rb"(?<![\w-])" + name.encode("ascii") + rb"(?![\w-])"
    return [
        i for i, line in enumerate(lines, 1)
        if not line.lstrip().startswith((b"function ", b"#"))
        for _ in re.finditer(pattern, line)
    ]


def pinned_inputs():
    DELTA["main"]()
    source = DELTA["CURRENT"]
    raw = source.with_name("source-manifest.json").read_bytes()
    REQUIRE(hashlib.sha256(raw).hexdigest().upper() == DELTA["MANIFEST_PIN"], "manifest changed")
    lines = DELTA["pinned"](source, DELTA["PINS"][1])
    return json.loads(raw), lines


def check_edges(lines):
    expected = {
        "Get-TaskEntries": [991, 1058, 1079, 1140, 1164, 1203, 1253],
        "Get-TaskStageBytes": [276, 280, 996],
        "Assert-TaskDocumentWrite": [1000, 1004, 1013, 1210],
        "Write-TaskJsonBytes": [1006, 1015],
        "Write-TaskJson": [1167, 1269],
        "Save-TaskReceipt": [1227, 1245, 1255, 1276],
        "Measure-TaskJson": [978, 1003, 1012, 1210, 1263],
        "Encode-TaskJson": [1005, 1014, 1210],
        "Assert-NoLink": [154, 246, 273, 294, 734, 743, 743, 1057, 1079, 1202, 1253],
        "Get-TaskCanonicalPath": [150, 733, 735, 1144],
        "Resolve-TaskInput": [1057, 1065, 1151, 1158, 1193, 1204, 1221, 1228, 1259],
        "Get-TaskCatalogueEntryPath": [142, 1047, 1051],
        "Add-TaskPathCatalogueEntry": [92, 93, 94, 95, 96, 97, 98, 99, 100],
        "Get-TaskCatalogueString": [118, 121, 126],
        "Assert-TaskFileSet": [1060, 1080, 1121, 1141, 1241, 1242, 1243, 1244, 1254],
        "Assert-TaskFreshHashIdentity": [248, 515, 1064, 1065, 1222, 1226, 1235],
        "Test-TaskHashValue": [184, 1062],
        "Get-TaskHashHex": [186, 1079, 1140, 1226, 1226, 1237, 1237, 1251, 1256, 1257, 1270],
        "Get-TaskPEHeader": [249, 1228, 1234, 1235, 1236],
        "Assert-TaskConsolePE": [249, 1228, 1234, 1235, 1237],
        "Read-TaskBoundedBytes": [967, 1173],
        "Get-TaskBootstrapHash": [571],
        "Read-TaskDependencyJson": [972],
        "Read-TaskJson": [1147, 1204],
        "Assert-TaskRawJsonBytes": [972, 972, 1175, 1214, 1214, 1215],
        "Assert-TaskRawJson": [1110, 1122],
        "Expect-TaskGate": [1215, 1217, 1218, 1219, 1220, 1226, 1237, 1242, 1243, 1244],
        "Close-TaskOwner": [160, 160, 282, 301, 554, 554, 737, 744, 744, 748,
                            933, 957, 957, 1086, 1136, 1212, 1224],
        "Close-TaskJsonReader": [957],
        "Complete-TaskOwnerClose": [160, 282, 301, 554, 737, 744, 748, 933, 957, 1086, 1136, 1212, 1224],
    }
    for name, sites in expected.items():
        actual = source_sites(lines, name)
        REQUIRE(actual == sites, f"source occurrences differ for {name}: {actual}")
    return expected


def reconstruct(manifest):
    profile = manifest["profile"]
    inputs = len(manifest["inputs"])
    namespaces = manifest["closed_directory_inputs"]
    fences = 2 + 2 * 3  # main initial/final, before and finally for each child
    binding = fences + 2 * 3  # before each helper and possible started-helper footer
    children = 3
    boot_hashes = 7
    _, hashes, hash_chunks, _ = runpy.run_path(str(HERE / "hash-schedule.py"))["work"]()

    references = len(manifest["references"])
    builds = len(manifest["compiler_final_outputs"])
    controls = len(manifest["control_files"])
    transport = len(manifest["supervisor"]["transport_files"])
    artifacts = references + builds + controls + 4 + transport
    root_cap = profile["root_walk_entries_max"]
    include_cap = profile["include_input_count"]
    module_ids = [len(manifest["module_controls"][mode]) for mode in ("produce", "verify")]
    namespace_rows = sum(row["entry_count"] + 1 for row in namespaces)
    namespace_dirs = sum(row["entry_count"] + 2 if row["recursive"] else 1 for row in namespaces)
    namespace_files = sum(len(row["files"]) for row in namespaces)
    REQUIRE((fences, inputs, len(namespaces), transport, artifacts, root_cap, include_cap, module_ids)
            == (8, 1288, 38, 6, 73, 85, 1289, [3, 11]), "finite controller shape changed")

    receipts = 3 + 1  # three normal sites and failure footer, which can coexist
    measures = 1 + receipts + 1 + 5  # marker, receipt, inventory, bundle fixed point
    encodes = 1 + receipts + 1 + 1
    document_checks = 1 + receipts * 2 + 2 + 2
    stage_calls = document_checks + children * 2
    walk = {
        "namespace_fences": Walk(fences * len(namespaces), fences * namespace_rows, fences * namespace_dirs),
        "stage_checks": bounded_walk(stage_calls, 7, False),
        "reference_captures": bounded_walk(2, 60, True),
        "build_captures": bounded_walk(2, 4, False),
        "current_root_and_failure": bounded_walk(2, root_cap, True),
        "sole_attempt_root": bounded_walk(1, 1, False),
    }
    walk_calls = sum(row.calls for row in walk.values())
    walk_dirs = sum(row.directories for row in walk.values())
    walk_rows = sum(row.rows for row in walk.values())
    candidates = walk_rows + walk_calls
    no_link = hashes + binding + transport + walk_dirs + 4 + fences * len(namespaces) + 120 + 1 + root_cap
    canonical = no_link + walk_calls + candidates + 1
    resolve = inputs * (fences + 2) + fences * len(namespaces) + include_cap + 1 + 3
    pe = binding + 4
    # Each tree-walk directory can own one iterator finally. All counted
    # file/hash/read scopes can enter finally with null as well as real owners.
    close = {
        "hashes": (2 * hashes, hashes),
        "bootstrap_hashes": (boot_hashes * 2, boot_hashes),
        "walker_iterators": (walk_dirs, walk_dirs),
        "transport_streams": (transport, transport),
        "control_copies_and_mutations": (2 * 2 + 2, 2 + 2),
        "bounded_json_reads": (3, 3),
        "streamed_dependency": (2, 1),
        "PE_headers": (pe, pe),
        "native_result_reads": (2, 2),
        "root_marker_and_mutant": (2, 2),
    }
    fresh = fences * (inputs + len(manifest["sources"])) + binding + 2 + 3
    binary_cmp = 3  # two captured target checks and the GUI mutant copy
    expected_sets = [
        ("namespace_fences", fences * len(namespaces), fences * namespace_files, fences * namespace_rows),
        ("reference_captures", 2, references * 2, 120),
        ("build_captures", 2, builds * 2, 8),
        ("root_preproducer_reference_set", 1, references, references),
        ("native_result_control_sets", 2, sum(module_ids), sum(module_ids)),
        ("final_root_set", 1, artifacts, root_cap),
        ("three_reference_controls", 3, 3 * references, 55 + 57 + 57),
    ]
    REQUIRE((walk_calls, stage_calls, document_checks, measures, encodes) == (330, 19, 13, 11, 7),
            "walker or document schedule changed")
    REQUIRE(fresh == 10355 and sum(module_ids) == 14, "identity or native control schedule changed")

    return {
        "schema": "R22_SHARED_CONTROLLER_OCCURRENCE_ENVELOPE",
        "input_identity_calls": {"fences": fences, "supervisor_binding": binding, "hashes": hashes,
                                 "hash_chunks": hash_chunks, "bootstrap_hashes": boot_hashes,
                                 "bootstrap_chunks": 379, "fresh_identity": fresh,
                                 "fresh_refusals_upper": 4, "text_digest_tests": fresh - binary_cmp + fences,
                                 "binary_digest_tests": binary_cmp, "standalone_wire_hex": 147,
                                 "refusal_wire_hex_in_fresh_body": 4},
        "walks": {name: dict(calls=row.calls, returned_rows=row.rows, candidate_visits=row.candidates,
                             directories=row.directories) for name, row in walk.items()},
        "paths": {"walk_calls": walk_calls, "walk_dirs": walk_dirs, "walk_rows": walk_rows,
                  "walk_candidates": candidates,
                  "recursive_walkers": fences * sum(bool(row["recursive"]) for row in namespaces) + 4,
                  "no_link": no_link, "canonical": canonical, "resolve": resolve,
                  "entry_path": canonical + resolve, "first_builds": 1376, "catalogue_repo_adds": inputs,
                  "catalogue_other_adds": len(manifest["sources"]) + 2 + artifacts + 2},
        "retirement": {"close_and_complete_by_scope": close, "close_json_reader": 1,
                       "close": sum(row[0] for row in close.values()),
                       "complete": sum(row[1] for row in close.values())},
        "file_sets": {"slots_expected_observed": expected_sets, "unplanned_diagnostic_refusals": 2,
                      "max_single_expected": max(len(row["files"]) for row in namespaces),
                      "max_single_observed": max(row["entry_count"] + 1 for row in namespaces)},
        "build_target_controls": {"build_captures": 2, "reference_captures": 2, "pe_reads": pe,
                                  "console_checks": pe, "dependency_calls": 1, "include_cap": include_cap,
                                  "copy_calls": 2, "copy_chunks": 514, "mutation_calls": 2,
                                  "compiler_replacements": 28 * 3, "expect_calls": 10,
                                  "expect_keys": 37, "native_result_reads": 2,
                                  "module_control_records": sum(module_ids), "bad_ID_prefixes": 1},
        "json_document": {"raw_byte_calls": 6, "raw_byte_units": 2 * 1048576 + 4194304 + 61,
                          "dependency_raw_nodes": 2 * include_cap + 256,
                          "raw_result_calls": sum(module_ids) + 2, "raw_result_units": 2 * 65536,
                          "bounded_reads": 3, "marker": 1, "receipts": receipts, "failure_inventory": 1,
                          "bundle": 1, "bundle_passes": 5, "measures": measures, "encodes": encodes,
                          "document_checks": document_checks, "stage_checks": stage_calls,
                          "stage_rows": walk["stage_checks"].rows},
        "trace_or_execution_claimed": False,
    }


def main():
    manifest, lines = pinned_inputs()
    edges = check_edges(lines)
    result = reconstruct(manifest)
    result["checked_source_occurrences"] = sum(len(sites) for sites in edges.values())
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
