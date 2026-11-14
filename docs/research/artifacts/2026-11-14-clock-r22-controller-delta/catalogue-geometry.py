"""Check the lexical R22 path catalogue under one explicit ordinary-drive profile."""

import hashlib
import json
import ntpath
from pathlib import Path
import runpy


delta = runpy.run_path(str(Path(__file__).with_name("check.py")))
RUN_LEAF = "source-clock-r1"


def parents(path):
    while True:
        yield path
        parent = ntpath.dirname(path)
        if parent == path:
            break
        path = parent


def main():
    source = delta["CURRENT"]
    repo = str(delta["ROOT"])
    base = str(source.parent)
    run = ntpath.join(base, "runs", RUN_LEAF)
    require = delta["require"]
    raw = source.with_name("source-manifest.json").read_bytes()
    require(hashlib.sha256(raw).hexdigest().upper() == delta["MANIFEST_PIN"], "manifest pin changed")
    manifest = json.loads(raw)
    current = delta["pinned"](source, delta["PINS"][1])
    catalogue = delta["collect"](current)["Get-TaskCatalogueEntryPath"]
    require(b"[string[]]::new(65)" in catalogue and b"$Entry['parents']=$taskParents" in catalogue,
            "parent chain changed")

    current_inputs = [ntpath.join(repo, row["path"]) for row in manifest["inputs"]]
    current_inputs.extend(ntpath.join(base, row["path"]) for row in manifest["sources"])
    current_inputs.extend(ntpath.join(base, name) for name in ("source-manifest.json", "sup.exe"))
    run_owned = [ntpath.join(run, "data", row["path"]) for row in manifest["references"]]
    run_owned.extend(ntpath.join(run, "build", row["name"]) for row in manifest["compiler_final_outputs"])
    run_owned.extend(ntpath.join(run, row["path"]) for row in manifest["control_files"])
    run_owned.extend(ntpath.join(run, "stage", name) for name in manifest["stage_files"])
    run_owned.extend(ntpath.join(run, "transport", name) for name in manifest["supervisor"]["transport_files"])

    leaves = [ntpath.normpath(path) for path in current_inputs + run_owned]
    chains = [tuple(parents(leaf)) for leaf in leaves]
    for leaf in leaves:
        drive, tail = ntpath.splitdrive(leaf)
        require(len(drive) == 2 and drive[1] == ":" and tail.startswith("\\") and "~" not in leaf
                and len(leaf.encode("utf-16-le")) // 2 <= 512, "outside fixed ordinary-drive path domain")
    require(len(leaves) == len({leaf.casefold() for leaf in leaves})
            == manifest["profile"]["maximum_complete_fence_identities"] == 1369, "leaf geometry differs")
    current_names = {name for chain in chains[:len(current_inputs)] for name in chain}
    all_names = {name for chain in chains for name in chain}
    require((len(current_inputs), len(run_owned), len(current_names), len(all_names), max(map(len, chains)))
            == (1294, 75, 1464, 1549, 19), "ancestor geometry differs")
    # The failure worksheet bounds seven interrupted first builds. No arrays
    # from published or interrupted builds are reclaimed in this envelope.
    parent_arrays = len(leaves) + 7
    parent_payload = parent_arrays * (24 + 65 * 8)
    require((parent_arrays, parent_payload) == (1376, 748544), "parent array projection differs")
    print(f"PASS: {len(leaves):,} leaves, {len(all_names):,} leaf/ancestor names, at most 19 path positions")
    print(f"Conditional x64 array requests: {parent_arrays:,} parent arrays, {parent_payload:,} bytes")
    print("Fixed ordinary-drive lexical profile only")


if __name__ == "__main__":
    main()
