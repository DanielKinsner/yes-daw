"""Compare pinned R13 and R22 controller function bodies as source text."""

from difflib import SequenceMatcher
import hashlib
import json
from pathlib import Path
import re


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
BASELINE = HERE.parent / "2026-11-06-clock-r13-runner-census-r3/sources/211-run-clock.ps1"
CURRENT = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                  "clock-oracle-r22-guard-read-source-r1/run-clock.ps1")
MANIFEST_PIN = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"
PINS = (
    "9E5DED855FF4A15FAE1D4B6D254AB4CF3FD96C5404BA34E1DC30F8C2BCB4E788",
    "294C4D91D0FF0EB0E80048B71DFC1E24C45103E88AD97F2F7715F9BD252F4798",
)
EXPECTED_CHANGED = {
    "Initialize-TaskPathCatalogue", "Invoke-TaskChild", "Add-TaskChildCleanup",
    "Initialize-TaskController", "Assert-TaskPins",
}
EXPECTED_ADDED = {
    "Spend-TaskSupervisor", "Set-TaskSupervisorInteger", "Get-TaskSupervisorInteger",
    "Get-TaskSupervisorRunHash", "Get-TaskSupervisorEnvironment", "Assert-TaskSupervisorBinding",
    "Write-TaskSupervisorRequest", "Get-TaskSupervisorRequestArgument",
    "Read-TaskSupervisorTerminal", "Test-TaskSupervisorReason",
    "Assert-TaskSupervisorTerminal", "New-TaskSupervisorFailure",
    "New-TaskSupervisorParentFailure", "Add-TaskSupervisorCleanup",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def pinned(path, expected):
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == expected, f"unexpected source: {path}")
    return raw.splitlines(keepends=True)


def collect(lines):
    # Only the formatting of these exact pinned sources is interpreted here.
    # Both use column-one declarations. One shared function closes at one indent.
    functions = {}
    for start, line in enumerate(lines):
        hit = re.match(rb"^function ([\w-]+)", line)
        if hit is None:
            continue
        name = hit[1].decode("ascii")
        end = start
        if not line.rstrip().endswith(b"}"):
            close = b" }" if name == "Assert-TaskDependencies" else b"}"
            end = next(i for i in range(start + 1, len(lines)) if lines[i].rstrip() == close)
        require(not any(child.startswith(b"function ") for child in lines[start + 1:end]),
                "function boundary overlaps another declaration")
        require(name not in functions, f"duplicate {name}")
        functions[name] = b"".join(lines[start:end + 1])
    return functions


def root_lines(lines, functions):
    root = b"".join(lines)
    for name, definition in functions.items():
        require(root.count(definition) == 1, f"ambiguous function boundary: {name}")
        root = root.replace(definition, b"", 1)
    return root.splitlines()


def root_geometry(before, after):
    require((len(before), len(after)) == (177, 183), "script root extent differs")
    matcher = SequenceMatcher(a=before, b=after, autojunk=False)
    edits = [(end_a - start_a, end_b - start_b)
             for operation, start_a, end_a, start_b, end_b in matcher.get_opcodes()
             if operation != "equal"]
    require(edits == [(0, 2), (1, 1), (1, 1), (0, 3), (1, 1), (0, 1), (1, 1),
                      (1, 1), (1, 1), (1, 1), (1, 1), (1, 1), (1, 1), (2, 2)],
            "unexpected script root edit spans")
    raw = CURRENT.with_name("source-manifest.json").read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == MANIFEST_PIN, "manifest source changed")
    manifest = json.loads(raw)
    root = b"\n".join(after)

    entry_guards = ("sourceReviewAccepted", "sourceWorkAccepted", "executionAccepted", "compilerAccepted",
                    "produceAccepted", "verifyAccepted", "supervisorAccepted", "supervisorProfileAccepted")
    for name in entry_guards:
        variable = ("$task" + name[0].upper() + name[1:]).encode("ascii")
        require(root.count(variable + b"=$false\n") == 1 and root.count(b"-not " + variable) == 1
                and manifest["guards"][name] is False, f"unexpected entry guard: {name}")
    require(root.count(b"$taskManifest['supervisor']['transport_files']") == 2,
            "expected one name loop and one artifact loop")
    require(b"$taskBootstrap 17209 512 4" in root and b"$taskOwnNames.Count+2" in root,
            "bootstrap or transitive-array limit changed")
    require(b"$taskOpaqueRoot,$taskTransportRoot," in root and root.count(b"$taskArtifacts.Add(") == 3,
            "directory or artifact schedule changed")
    require(b"$taskManifest['supervisor']['binary']['bytes'];" in root
            and b"$taskTransitive.Add(@{path=$taskSupervisorBinaryPath;" in root,
            "helper charges or transitive descriptor changed")
    require(b"{308L}elseif($taskN -ceq 'child-0.request.bin'){41260L}else{18644L}" in root,
            "transport cap branches changed")

    transport = {f"child-{slot}.{kind}.bin": cap for slot, request_cap in enumerate((41260, 18644, 18644))
                 for kind, cap in (("request", request_cap), ("terminal", 308))}
    require(list(transport) == manifest["supervisor"]["transport_files"], "transport names changed")
    transport_bytes = sum(transport.values())
    require(transport_bytes == manifest["retention"]["R14_transport_bytes_upper"] == 79472,
            "transport reservation differs")
    hash_chunks = sum((cap + 65535) // 65536 for cap in transport.values())
    require(hash_chunks == 6, "transport hash schedule differs")
    artifact_rows = sum(len(manifest[key]) for key in ("references", "compiler_final_outputs", "control_files"))
    artifact_rows += len(transport) + 4
    require(artifact_rows + 1 == manifest["profile"]["declared_success_run_files"] == 74,
            "successful file geometry changed")
    transitive_rows = len(manifest["inputs"]) + len(manifest["sources"]) + 2
    require(transitive_rows == manifest["closed_json_domain"]["output_arrays_max_entries"] == 1294,
            "transitive array ceiling changed")
    return {"root_line_counts": [len(before), len(after)], "edit_spans": len(edits),
            "transport_files": len(transport), "transport_bytes": transport_bytes,
            "transport_hash_chunks": hash_chunks, "artifact_rows": artifact_rows,
            "transitive_rows": transitive_rows,
            "successful_files_with_bundle": artifact_rows + 1,
            "root_walk_max": manifest["profile"]["root_walk_entries_max"]}


def main():
    baseline_lines = pinned(BASELINE, PINS[0])
    current_lines = pinned(CURRENT, PINS[1])
    before = collect(baseline_lines)
    after = collect(current_lines)
    shared = before.keys() & after.keys()
    changed = {name for name in shared if before[name] != after[name]}
    added = after.keys() - before.keys()
    require(before.keys() <= after.keys(), "unexpected removed function")
    require(changed == EXPECTED_CHANGED and added == EXPECTED_ADDED, "function delta differs")
    require((len(before), len(after), len(shared - changed)) == (74, 88, 69),
            "function counts differ")
    return {"old": len(before), "current": len(after), "same": len(shared - changed),
            "changed": sorted(changed), "added": sorted(added),
            "root": root_geometry(root_lines(baseline_lines, before), root_lines(current_lines, after))}


if __name__ == "__main__":
    print(json.dumps(main(), indent=2))
