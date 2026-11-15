"""Join the reviewed R22 local scalar worksheets and check their source edges."""

from collections import Counter
import json
from pathlib import Path
import re
import runpy


HERE = Path(__file__).resolve().parent
DELTA = runpy.run_path(str(HERE / "check.py"))
require = DELTA["require"]


def check_edges(lines, bodies, local_names):
    # Exact pinned source spelling. No PowerShell parse, import or execution.
    watched = local_names | {"Invoke-TaskFencedChild"}
    regions = {name: body.split(b"{", 1)[1] for name, body in bodies.items()}
    regions["root"] = b"\n".join(DELTA["root_lines"](lines, bodies))
    edges = {}
    for name, region in regions.items():
        names = re.findall(rb"(?<![\w-])[A-Za-z]+-[A-Za-z][\w-]*", region)
        calls = Counter(value.decode("ascii") for value in names if value.decode("ascii") in watched)
        if calls:
            edges[name] = calls
    require(edges == {
        "root": {"Initialize-TaskController": 1, "Initialize-TaskPathCatalogue": 1,
                 "Assert-TaskPins": 2, "Invoke-TaskFencedChild": 3},
        "Invoke-TaskFencedChild": {"Assert-TaskPins": 2, "Invoke-TaskChild": 1},
        "Assert-TaskPins": {"Assert-TaskSupervisorBinding": 1},
        "Invoke-TaskChild": {
            "Assert-TaskSupervisorBinding": 2, "Spend-TaskSupervisor": 2, "Get-TaskSupervisorRunHash": 1,
            "Get-TaskSupervisorEnvironment": 1, "Write-TaskSupervisorRequest": 1,
            "Get-TaskSupervisorRequestArgument": 1, "Read-TaskSupervisorTerminal": 1,
            "Add-TaskSupervisorCleanup": 1, "New-TaskSupervisorFailure": 2, "New-TaskSupervisorParentFailure": 1,
        },
        "Get-TaskSupervisorRunHash": {"Set-TaskSupervisorInteger": 1},
        "Write-TaskSupervisorRequest": {"Spend-TaskSupervisor": 1, "Set-TaskSupervisorInteger": 8},
        "Read-TaskSupervisorTerminal": {"Spend-TaskSupervisor": 1, "Get-TaskSupervisorInteger": 4,
                                       "Assert-TaskSupervisorTerminal": 1},
        "Assert-TaskSupervisorTerminal": {"Test-TaskSupervisorReason": 1},
    }, "written caller graph changed")
    return edges


def work():
    source = DELTA["CURRENT"]
    lines = DELTA["pinned"](source, DELTA["PINS"][1])
    bodies = DELTA["collect"](lines)
    local_names = DELTA["EXPECTED_ADDED"] | DELTA["EXPECTED_CHANGED"]
    edges = check_edges(lines, bodies, local_names)
    manifest = json.loads(b"".join(DELTA["pinned"](source.with_name("source-manifest.json"), DELTA["MANIFEST_PIN"])))
    profile, supervisor = manifest["profile"], manifest["supervisor"]

    children = edges["root"]["Invoke-TaskFencedChild"]
    fences = edges["root"]["Assert-TaskPins"] + children * edges["Invoke-TaskFencedChild"]["Assert-TaskPins"]
    bindings = fences + children * edges["Invoke-TaskChild"]["Assert-TaskSupervisorBinding"]
    args = (len(manifest["compiler"]["arguments"]), 6, 6)
    strings = tuple(count + 12 for count in args)
    require((children, fences, bindings, args, strings) == (3, profile["normal_fence_calls"], 14, (28, 6, 6), (40, 18, 18)),
            "reached child or fence geometry changed")

    fixed_spends = 3 * children
    set_four, set_eight = sum(count + 6 for count in strings), 2 * children
    get_four = get_eight = 17 * children
    require((set_four, set_eight, get_four, get_eight) == (94, 6, 51, 51), "integer transport schedule changed")
    fence = (24 + 17 + 27 + 30 + 4 + 58 * profile["directory_namespaces_count"]
             + 41 * (profile["directory_namespace_entries"] + profile["directory_namespaces_count"])
             + 54 * len(manifest["sources"]) + 37 * len(manifest["inputs"]))
    catalogue = 2 + 49 + 36 + 102 + 49 + 6 + 13 + 4 + 1288 * 11 + 16 * 15 + 63 * 18

    # Each pair gives the whole-run fixed actions and the coefficient of P.
    groups = {
        "transport": {
            "Set-TaskSupervisorInteger": (set_four * 208 + set_eight * 336, 0),
            "Get-TaskSupervisorInteger": (get_four * 216 + get_eight * 360, 0),
            "Get-TaskSupervisorRunHash": (children * 29766, 0),
            "Write-TaskSupervisorRequest": (sum(1136 + 120 * count + 56 * 512 * count for count in strings), 0),
            "Get-TaskSupervisorRequestArgument": (children * (144 + 12 * 26 + 512 * 32 + 4 * 512 * 42), 0),
            "Read-TaskSupervisorTerminal": (children * 6062, 0),
        },
        "binding and meter": {
            "Assert-TaskSupervisorBinding": (bindings * 706, 0),
            "Get-TaskSupervisorEnvironment": (children * 596, 0),
            "Spend-TaskSupervisor": (fixed_spends * 200, children * 200),
        },
        "child": {"Invoke-TaskChild": (children * (1396 + 3475) + 20 * sum(args), children * 84)},
        "terminal semantics": {
            "Assert-TaskSupervisorTerminal": (children * 3832, 0),
            "Test-TaskSupervisorReason": (children * 3 * 361, 0),
            "New-TaskSupervisorFailure": (children * 2 * 189, 0),
            "New-TaskSupervisorParentFailure": (children * 106, 0),
            "Add-TaskSupervisorCleanup": (children * 962, 0),
        },
        "fence and startup": {
            "Initialize-TaskPathCatalogue": (catalogue, 0),
            "Assert-TaskPins": (fences * fence, 0),
            "Initialize-TaskController": (1372, 0),
            "Add-TaskChildCleanup": (0, 0),
        },
    }
    names = [name for group in groups.values() for name in group]
    require(len(names) == len(set(names)) == 19 and set(names) == local_names, "local body partition changed")
    totals = {group: tuple(map(sum, zip(*rows.values()))) for group, rows in groups.items()}
    require(list(totals.values()) == [(2658596, 0), (13472, 600), (15413, 252), (19083, 0), (796783, 0)],
            "reviewed local subtotal changed")
    fixed, per_poll = map(sum, zip(*totals.values()))
    root, _ = runpy.run_path(str(HERE / "root-work.py"))["work"]()
    _, _, _, hashes = runpy.run_path(str(HERE / "hash-schedule.py"))["work"]()
    require((fixed, per_poll, fixed + root + hashes) == (3503347, 852, 12572321), "partial controller operand changed")

    sites = re.findall(rb"\bSpend-TaskSupervisor '([^']+)'(?: ([^;\r\n]+))?", b"".join(lines))
    require(sites == [(b"request", b"$taskLength"), (b"terminal", b"308"), (b"invoke", b""), (b"parent_poll", b"")],
            "controller catalogue spend sites changed")
    caps = tuple(140 + (4 + 2 * 512) * count for count in strings)
    require(list(caps) == supervisor["request_caps"] and supervisor["terminal_bytes"] == 308,
            "controller debit domains changed")
    require(supervisor["controller_prices"] == dict.fromkeys(("invoke", "request", "terminal", "parent_poll"))
            and supervisor["parent_poll_limit"] is None and supervisor["parent_watchdog_ms"] is None
            and supervisor["helper_reservations"] == [None] * children, "expected an unbound work profile")
    debits = {"invoke": (children, 0), "request": (sum(caps), 0),
              "terminal": (children * supervisor["terminal_bytes"], 0), "parent_poll": (0, children)}
    require(list(debits.values()) == [(3, 0), (78548, 0), (924, 0), (0, 3)], "debit arithmetic changed")
    require(b"Spend-Task ([long]$taskManifest['supervisor']['helper_reservations'][$taskChildIndex])"
            in bodies["Invoke-TaskChild"] and b"Spend-Task ([long]$taskPrice*$Count)" in bodies["Spend-TaskSupervisor"],
            "direct legacy meter edges changed")
    return {"local_groups": totals, "local_body_count": len(names), "local_body_actions": (fixed, per_poll),
            "root_caller_actions": root, "inclusive_hash_actions": hashes,
            "partial_controller_actions": (fixed + root + hashes, per_poll),
            "debit_amount_multipliers": debits, "direct_reservation_debits": children,
            "separate_legacy_spend_body_calls": (fixed_spends + children, children)}


if __name__ == "__main__":
    print(json.dumps(work(), indent=2))
