"""Recompose the existing hash body against the staged R22 caller domains."""

import hashlib
import json
from pathlib import Path
import re
import runpy


delta = runpy.run_path(str(Path(__file__).with_name("check.py")))
LEDGER = delta["BASELINE"].with_name("250-body-ledgers.json")
LEDGER_PIN = "7DD5C8A524BF76588B2EF359F43958DE92B1BA904B0ACC6E7FBF3F70FF261460"


def chunks(size):
    return (size + 65535) // 65536


def main():
    source = delta["CURRENT"]
    lines = delta["pinned"](source, delta["PINS"][1])
    old = delta["collect"](delta["pinned"](delta["BASELINE"], delta["PINS"][0]))
    new = delta["collect"](lines)
    require = delta["require"]
    require(old["Get-TaskHash"] == new["Get-TaskHash"], "reused hash source changed")

    sites = [index for index, line in enumerate(lines, 1) if not line.startswith(b"function ")
             and re.search(rb"\bGet-TaskHash(?![\w-])", line)]
    require(sites == [247, 420, 515, 1062, 1064, 1065, 1079, 1140, 1221, 1222, 1225,
                      1234, 1235, 1237, 1238, 1251, 1256, 1257, 1270], "hash callers changed")

    root = delta["main"]()["root"]
    manifest_bytes = source.with_name("source-manifest.json").read_bytes()
    require(hashlib.sha256(manifest_bytes).hexdigest().upper() == delta["MANIFEST_PIN"], "manifest changed")
    manifest = json.loads(manifest_bytes)
    profile = manifest["profile"]
    fence_calls = len(manifest["inputs"]) + len(manifest["sources"]) + 1
    fence_chunks = sum(chunks(row["bytes"]) for row in manifest["inputs"] + manifest["sources"])
    # This hash reads a current file. A changed manifest can reach the full
    # cap before the hash comparison, even if the reviewed manifest is shorter.
    fence_chunks += chunks(1048576)
    require((profile["normal_fence_calls"], fence_calls, fence_chunks) == (8, 1293, 8021), "fence domain changed")

    binary_chunks = chunks(manifest["supervisor"]["binary"]["maximum_bytes"])
    reference_cap = max(row["bytes_cap"] for row in manifest["references"])
    build_cap = max(row["bytes_cap"] for row in manifest["compiler_final_outputs"])
    retention = manifest["retention"]
    # Pool admission follows the hash. Include one candidate's full cap
    # after the accepted prefix, and at most N-1 per-file rounding extras.
    reference_chunks = chunks(retention["reference_pool"] + reference_cap) + 59
    build_chunks = chunks(retention["compiler_admitted_final_pool"] + build_cap) + 3
    rows = {
        "historical and own pin fences": (8 * fence_calls, 8 * fence_chunks),
        "all supervisor binding visits": (14, 14 * binary_chunks),
        "request target digest": (3, 3 * chunks(33554432)),
        "post-child expected target digest": (2, 2 * chunks(33554432)),
        "reference captures": (120, 2 * reference_chunks),
        "build captures": (8, 2 * build_chunks),
        "predecessor and mutation": (3, 3 * chunks(98009)),
        "binary and GUI control copies": (4, 4 * chunks(33554432)),
        "control artifacts": (len(manifest["control_files"]), sum(chunks(row["bytes_cap"]) for row in manifest["control_files"])),
        "transport artifacts": (root["transport_files"], root["transport_hash_chunks"]),
        "stage artifacts": (4, 4 * chunks(67108864)),
        "bundle": (1, chunks(1048576)),
    }
    calls = sum(row[0] for row in rows.values())
    chunk_visits = sum(row[1] for row in rows.values())
    ledger = LEDGER.read_bytes()
    require(hashlib.sha256(ledger).hexdigest().upper() == LEDGER_PIN, "hash ledger changed")
    body = next(row for row in json.loads(ledger)["helpers"] if row["name"] == "Get-TaskHash")
    ledger_source = body["whole_source"].encode("ascii")
    current_body = new["Get-TaskHash"]
    require(ledger_source.startswith(current_body)
            and all(line.startswith(b"# ") for line in ledger_source[len(current_body):].splitlines()),
            "ledger source differs")
    fixed = sum(group["units"] for group in body["fixed_groups"])
    per_chunk = sum(body["coefficients"]["read_chunk"].values())
    require((fixed, per_chunk) == (295, 65), "hash body arithmetic changed")
    upper = fixed * calls + per_chunk * chunk_visits
    require((calls, chunk_visits, upper) == (10513, 88908, 8880355), "hash schedule changed")
    for name, (row_calls, row_chunks) in rows.items():
        print(f"{name}: {row_calls:,} calls, {row_chunks:,} chunks")
    print(f"PASS: {calls:,} hash calls, {chunk_visits:,} chunks, {upper:,} conditional actions")


if __name__ == "__main__":
    main()
