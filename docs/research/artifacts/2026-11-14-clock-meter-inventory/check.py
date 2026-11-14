"""Inventory every native meter call site in the exact R22 source."""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import runpy


ROOT = Path(__file__).resolve().parents[4]
MANIFEST = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                   "clock-oracle-r22-guard-read-source-r1/source-manifest.json")
MANIFEST_HASH = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"
METER = re.compile(r"\b(spend|admit_body|charge)\([^,\n]+,\s*Family::(\w+)")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def inventory():
    raw = MANIFEST.read_bytes()
    require(digest(raw) == MANIFEST_HASH, "R22 manifest differs")
    native = {}
    for pin in json.loads(raw)["supervisor"]["native_sources"]:
        path = ROOT / pin["path"]
        data = path.read_bytes()
        require(len(data) == pin["bytes"] and digest(data) == pin["sha256"],
                f"native source differs: {path.name}")
        native[path.name] = data.decode()
    declaration = native["sup.h"].split("enum class Family : U32 {", 1)[1].split("};", 1)[0]
    families = [name.strip() for name in declaration.split(",")]
    require(len(families) == 37 and len(set(families)) == 37 and families.pop() == "Count",
            "unexpected family declaration")
    counts = Counter()
    sites = []
    for name, source in sorted(native.items()):
        if not name.endswith(".cpp"):
            continue
        for number, line in enumerate(source.splitlines(), start=1):
            for method, family in METER.findall(line):
                counts[family] += 1
                sites.append({"source": name, "line": number, "method": method,
                              "family": family, "text": line.strip()})
    require(counts.keys() == set(families) and len(sites) == 50, "incomplete literal call coverage")
    return {
        "manifest_sha256": MANIFEST_HASH,
        "family_count": len(families),
        "literal_site_count": len(sites),
        "family_sites": {name: counts[name] for name in families},
        "sites": sites,
        "scope": "Literal source calls only. Reachability, dynamic executions and work totals are separate.",
    }


def check_bounds(result):
    here = Path(__file__).resolve().parent
    families = set()
    for group in ("hash", "codec", "capture", "path"):
        script = here.parent / f"2026-11-14-clock-{group}-meter" / "check.py"
        checked = runpy.run_path(str(script))["check"]()
        require(families.isdisjoint(checked), f"duplicate scheduled meter family in {group}")
        families.update(checked)
    require(families == set(result["family_sites"]), "scheduled and declared meter families differ")
    print(f"PASS: all {len(families)} R22 meter families covered exactly once")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", type=Path)
    parser.add_argument("--check-bounds", action="store_true")
    args = parser.parse_args()
    result = inventory()
    if args.check is None and not args.check_bounds:
        print(json.dumps(result, indent=2))
    if args.check is not None:
        require(json.loads(args.check.read_bytes()) == result, "meter inventory differs")
        print(f"PASS: {result['family_count']} families, {result['literal_site_count']} literal meter calls")
    if args.check_bounds:
        check_bounds(result)
