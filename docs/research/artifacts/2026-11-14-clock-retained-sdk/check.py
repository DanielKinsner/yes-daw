"""Join exact R22 retained source inputs with the observed SDK include set."""

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
MANIFEST = ROOT / ("build-ci/musical-audio-persistent-planner-certification/source-revisions/"
                   "clock-oracle-r22-guard-read-source-r1/source-manifest.json")
RECEIPT = Path(__file__).resolve().parents[1] / "2026-11-14-clock-supervisor-abi/r22-object-result.json"
MANIFEST_PIN = "28619B3443768E03BA45FD698030BC5DD743C6B819125D475B4AA1A2687D0C06"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest().upper()


def identity(name):
    return str(ROOT.joinpath(name).resolve()).casefold()


def main():
    raw = MANIFEST.read_bytes()
    require(sha(raw) == MANIFEST_PIN, "unexpected manifest bytes")
    manifest = json.loads(raw)
    receipt = json.loads(RECEIPT.read_bytes())
    require(receipt["manifest_sha256"] == MANIFEST_PIN, "unexpected compile receipt subject")
    files = {identity(row["path"]): row for row in manifest["inputs"]}
    retained = manifest["retention"]
    require(len(files) == manifest["profile"]["input_count"], "duplicate or missing manifest input")
    require(sum(row["bytes"] for row in files.values()) == retained["input_bytes"], "input sum differs")
    upper = sum(retained[name] for name in ("input_bytes", "own5_reserved", "reference_pool",
                                            "compiler_admitted_final_pool", "stage_pool", "control_pool"))
    supervisor = manifest["supervisor"]
    upper += supervisor["transport_cap"] + supervisor["binary"]["maximum_bytes"]
    require(upper == retained["reservation_upper"], "manifest retained upper differs")
    includes = receipt["include_hashes"]
    require(len({identity(name) for name in includes}) == len(includes), "duplicate includes")
    extra = []
    known = 0
    for name, pin in includes.items():
        if identity(name) in files:
            require(files[identity(name)]["sha256"] == pin, f"shared path pin differs: {name}")
            known += 1
        else:
            require((ROOT / name).is_absolute(), f"untracked include location: {name}")
            data = (ROOT / name).read_bytes()
            require(sha(data) == pin, f"new include pin differs: {name}")
            extra.append(len(data))
    cap = retained["cap"]
    require(cap == 840 * 1024 * 1024 and upper + sum(extra) <= cap, "retained upper exceeds cap")
    require((known, len(extra), sum(extra)) == (23, 80, 4_414_985), "include count or sum differs")
    require((upper + sum(extra), cap - upper - sum(extra)) == (826_527_707, 54_276_133),
            "retained projection differs")
    print(f"PASS: {len(includes)} observed includes, {len(extra)} new, {sum(extra):,} bytes")
    print(f"Retained projection {upper + sum(extra):,} bytes, residual {cap - upper - sum(extra):,} bytes")


if __name__ == "__main__":
    main()
