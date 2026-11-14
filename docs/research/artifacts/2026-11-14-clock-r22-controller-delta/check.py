"""Compare pinned R13 and R22 controller function bodies as source text."""

import hashlib
import json
from pathlib import Path
import re


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
BASELINE = HERE.parent / "2026-11-06-clock-r13-runner-census-r3/sources/211-run-clock.ps1"
CURRENT = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                  "clock-oracle-r22-guard-read-source-r1/run-clock.ps1")
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


def main():
    before = collect(pinned(BASELINE, PINS[0]))
    after = collect(pinned(CURRENT, PINS[1]))
    shared = before.keys() & after.keys()
    changed = {name for name in shared if before[name] != after[name]}
    added = after.keys() - before.keys()
    require(before.keys() <= after.keys(), "unexpected removed function")
    require(changed == EXPECTED_CHANGED and added == EXPECTED_ADDED, "function delta differs")
    require((len(before), len(after), len(shared - changed)) == (74, 88, 69),
            "function counts differ")
    return {"old": len(before), "current": len(after), "same": len(shared - changed),
            "changed": sorted(changed), "added": sorted(added)}


if __name__ == "__main__":
    print(json.dumps(main(), indent=2))
